#pragma once

#include "lib/platform.h"
#include "lib/defines.h"

#include "channel.h"

ATTRIBUTE_INLINE_ALWAYS 
static void atomic_list_push_chain(volatile void** list, void* first, void* last)
{
    for(;;) {
        uint64_t curr = platform_atomic_load64(list);
        platform_atomic_store64(last, curr);
        if(platform_atomic_cas_weak64(list, curr, (uint64_t) first))
            break;
    }
}

ATTRIBUTE_INLINE_ALWAYS 
static void atomic_list_push(volatile void** list, void* node)
{
    atomic_list_push_chain(list, node, node);
}

ATTRIBUTE_INLINE_ALWAYS 
static void* atomic_list_pop_all(volatile void** list)
{
    return (void*) platform_atomic_exchange64(list, 0);
}

typedef struct Sync_Wait {
    Sync_Wait_Func wait;
    Sync_Wake_Func wake;
    uint32_t notify_bit;
    uint32_t _;
} Sync_Wait;

CHANAPI bool sync_wait(volatile void* state, uint32_t current, isize timeout, Sync_Wait wait)
{
    if(wait.notify_bit) 
    {
        chan_atomic_or32(state, wait.notify_bit);
        current |= wait.notify_bit;
    }
    return wait.wait((void*) state, current, timeout);
}

CHANAPI void sync_wake(volatile void* state, uint32_t prev, Sync_Wait wait)
{
    if(wait.notify_bit)
    {
        if(prev & wait.notify_bit)
            wait.wake((void*) state);
    }
    else
        wait.wake((void*) state);
}

CHANAPI uint32_t sync_wait_for_equal(volatile void* state, uint32_t desired, Sync_Wait wait)
{
    for(;;) {
        uint32_t current = chan_atomic_load32(state);
        if((current & ~wait.notify_bit) == (desired & ~wait.notify_bit))
            return current & ~wait.notify_bit;

        sync_wait(state, current, -1, wait);
    }
}

CHANAPI uint32_t sync_wait_for_greater(volatile void* state, uint32_t desired, Sync_Wait wait)
{
    for(;;) {
        uint32_t current = chan_atomic_load32(state);
        if((current & ~wait.notify_bit) > (desired & ~wait.notify_bit))
            return current & ~wait.notify_bit;
            
        sync_wait(state, current, -1, wait);
    }
}

CHANAPI uint32_t sync_wait_for_smaller(volatile void* state, uint32_t desired, Sync_Wait wait)
{
    for(;;) {
        uint32_t current = chan_atomic_load32(state);
        if((current & ~wait.notify_bit) < (desired & ~wait.notify_bit))
            return current & ~wait.notify_bit;
            
        sync_wait(state, current, -1, wait);
    }
}

typedef struct Sync_Timed_Wait {
    double rdtsc_freq_s;
    isize rdtsc_freq_ms;
    isize wait_ticks;
    isize start_ticks;
} Sync_Timed_Wait;

CHANAPI Sync_Timed_Wait sync_timed_wait_start(double wait)
{
    static double rdtsc_freq_s = 0;
    static isize rdtsc_freq_ms = 0;
    if(rdtsc_freq_s == 0)
    {
        rdtsc_freq_s = (double) platform_rdtsc_frequency();
        rdtsc_freq_ms = platform_rdtsc_frequency()/1000;
    }

    Sync_Timed_Wait out = {0};
    out.rdtsc_freq_s = rdtsc_freq_s;
    out.rdtsc_freq_ms = rdtsc_freq_ms;
    out.wait_ticks = (isize) (wait*rdtsc_freq_s);
    out.start_ticks = platform_rdtsc();
    return out;
}

CHANAPI bool sync_timed_wait(volatile void* state, uint32_t current, Sync_Timed_Wait timeout, Sync_Wait wait)
{
    isize curr_ticks = platform_rdtsc();
    isize ellapsed_ticks = curr_ticks - timeout.start_ticks;
    if(ellapsed_ticks >= timeout.wait_ticks)
        return false;
        
    isize wait_ms = (timeout.wait_ticks - ellapsed_ticks)/timeout.rdtsc_freq_ms;
    return sync_wait(state, current, wait_ms, wait);
}


//Retrieves a block of at least size from the alloc cache. 
//This operation happens most of the time without using any atomics nor calls to malloc.
void* atomic_block_alloc(isize size);
//Returns a block to allocation cache. Costs juts one CAS instruction to probably uncosted adress.
void atomic_block_free(void* addr);
//Frees all blocks in the cache
void atomic_block_cleanup();
//Frees all blocks in the cache. All blocks that are in use remain valid but get dealloced on subsequent call to
// atomic_block_free. 
void atomic_block_thread_deinit();

struct Atomic_Block_Cache;

typedef struct Atomic_Block {
    union {
        struct Atomic_Block* next;
        struct Atomic_Block_Cache* cache;
    };
    //... data ...
} Atomic_Block;

typedef struct Atomic_Block_Cache {
    ATTRIBUTE_ALIGNED(64)
    volatile Atomic_Block* external_list;
    volatile isize abandoned;

    volatile isize alloced_blocks;
    volatile isize freed_blocks;

    ATTRIBUTE_ALIGNED(64)
    volatile Atomic_Block* internal_list;

} Atomic_Block_Cache;

#define ATOMIC_BLOCK_CACHE_SIZES_COUNT 4 //64 128 256 512

ATTRIBUTE_THREAD_LOCAL static Atomic_Block_Cache* caches[ATOMIC_BLOCK_CACHE_SIZES_COUNT] = {0};

void* atomic_block_alloc(isize size)
{
    enum {MAX_SIZE = (1 << ATOMIC_BLOCK_CACHE_SIZES_COUNT)/2*64 - sizeof(Atomic_Block)};
    if(size > MAX_SIZE)
    {
        Atomic_Block* block = (Atomic_Block*) platform_heap_reallocate(sizeof(Atomic_Block) + size, NULL, 64);
        block->cache = NULL;
        return block + 1;
    }
    else
    {
        isize cache_line_count = (size + sizeof(void*) + 63)/64;
        isize cache_index = 0;
        cache_index += cache_line_count > 1;
        cache_index += cache_line_count > 2;
        cache_index += cache_line_count > 4;

        assert(cache_index < ATOMIC_BLOCK_CACHE_SIZES_COUNT);
        if(caches[cache_index] == NULL)
        {
            caches[cache_index] = (Atomic_Block_Cache*) platform_heap_reallocate(sizeof(Atomic_Block_Cache), NULL, 64);
            memset(caches[cache_index], 0, sizeof(Atomic_Block_Cache));
        }
        
        Atomic_Block_Cache* cache = caches[cache_index];
        assert(platform_atomic_load64(cache->abandoned) == 0);
        if(cache->internal_list == NULL)
        {
            Atomic_Block* popped = (Atomic_Block*) atomic_list_pop_all((void**) &cache->external_list);
            for(Atomic_Block* curr = popped; curr;)
            {
                Atomic_Block* next = curr->next;
                curr->next = cache->internal_list; 
                cache->internal_list = curr;
                curr = next;
            }
        }

        Atomic_Block* block = NULL;
        if(cache->internal_list)
        {
            block = cache->internal_list;
            cache->internal_list = cache->internal_list->next;
        }
        else
        {
            block = (Atomic_Block*) platform_heap_reallocate(cache_line_count*64, NULL, 64);
            platform_atomic_add64(&cache->alloced_blocks, 1);
        }

        block->cache = cache;
        return block + 1;
    }
}

ATTRIBUTE_INLINE_NEVER
static void _atomic_block_cleanup(Atomic_Block_Cache* cache, bool also_internal)
{
    uint64_t freed_block = 0;
    Atomic_Block* external = (Atomic_Block*) atomic_list_pop_all(&cache->external_list);
    for(Atomic_Block* curr = external; curr; )
    {
        Atomic_Block* next = curr->next;
        platform_heap_reallocate(curr, NULL, 64);
        freed_block = platform_atomic_add64(&cache->freed_blocks, 1) + 1;
        curr = next;
    }

    if(also_internal)
    {
        Atomic_Block* internal = cache->internal_list;
        cache->internal_list = NULL;
        for(Atomic_Block* curr = internal; curr; )
        {
            Atomic_Block* next = curr->next;
            platform_heap_reallocate(curr, NULL, 64);
            freed_block = platform_atomic_add64(&cache->freed_blocks, 1) + 1;
            curr = next;
        }
    }

    uint64_t alloced_blocks = platform_atomic_load64(&cache->alloced_blocks);
    if(freed_block == alloced_blocks)
        platform_heap_reallocate(0, cache, 64);
}

void atomic_block_free(void* addr)
{
    Atomic_Block* block = (Atomic_Block*) addr - 1;
    Atomic_Block_Cache* cache = block->cache;
    if(cache)
    {
        block->cache = NULL;
        atomic_list_push((void**) &cache->external_list, block);
        
        if(platform_atomic_load64(cache->abandoned))
            _atomic_block_cleanup(cache, false);
    }
    else
    {
        platform_heap_reallocate(block, NULL, 64);
    }
}

void atomic_block_cleanup()
{
    for(isize i = 0; i < ATOMIC_BLOCK_CACHE_SIZES_COUNT; i++)
    {
        Atomic_Block_Cache* cache = caches[i];
        if(cache)
            _atomic_block_cleanup(cache, true);
    }
}

void atomic_block_thread_deinit()
{
    for(isize i = 0; i < ATOMIC_BLOCK_CACHE_SIZES_COUNT; i++)
    {
        Atomic_Block_Cache* cache = caches[i];
        if(cache)
        {
            platform_atomic_store64(&cache->abandoned, 1);
            _atomic_block_cleanup(cache, true);
        }
        
        caches[i] = NULL;
    }
}


/*
    //Notice     - get/set, cas, wait for 
    //Wait_Group - add/done and wait on those (probably just an atomic counter)
    
    void thread_2(Q list, Notice* canceled, Wait_Group* wait)
    {
        for(int i = 0;; i++)
        {
            if(notice_get(canceled))
                break;

            transfer_queue_push(&list, &i);
        }

        wait_group_done(wait);
        transfer_queue_deinit(&list);
    }
    void thread_1()
    {
        Notice canceled = {0};
        Wait_Group wait = {0};

        Q queue = queue_make(-1);

        for(int i = 0; i < 10; i++)
            go thread_2(queue_share(queue), &canceled, wait_group_add(&wait));

        sleep(2);

        notice_set(&canceled, true);
        wait_group_wait(wait); //or...
        queue_wait_for_exclusivity(queue);
        
        while(true)
        {
            int items[16];
            isize popped = transfer_queue_pop(list, items, 16);
            if(popped == 0)
                break;
            for()
                printf(items[i]);
        }
        transfer_queue_deinit_and_recycle_all(&list);
    }
    
    void thread_2(Q list, Notice* canceled)
    {
        for(int i = 0;; i++)
        {
            if(notice_get(canceled))
                break;

            transfer_queue_push(&list, &i);
        }

        transfer_queue_deinit(&list);
    }
    void thread_1()
    {
        Notice canceled = {0};
        Wait_Group wait = {0};

        Q queue = queue_make(-1, WAIT_JOB);

        for(int i = 0; i < 10; i++)
            go thread_2(queue_share(queue), &canceled);

        sleep_job(2);

        int items[16];
        for(Queue_Pop_All pop_all = {0}; queue_pop_all(&queue, &pop_all, items, 16); )
        {
            for(pop_all.count)
                item[16]
        }
        
        // turns into:

        for(Queue_Thread* thread = queue_threads(queue); thread; thread = thread->next) 
        {
            isize count = queue_thread_get_count(thread);
            int items[16];
            for(isize i = 0; i < count; i += 16)
            {
                isize popped = transfer_queue_pop(list, items, 16);

            }
        }

        while(true)
        {
            int items[16];
            isize popped = transfer_queue_pop(list, items, 16);
            if(popped == 0)
                break;
            for()
                printf(items[i]);
        }

        notice_set(&canceled, true);
        transfer_queue_deinit_and_recycle_all(&list);
    }
*/
