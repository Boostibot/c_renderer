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


#if 0
/* Multi-producer/multi-consumer bounded queue
* 2010, Dmitry Vyukov
* Distributed under the terms of the GNU General Public License
* as published by the Free Software Foundation,
* either version 3 of the License,
* or (at your option) any later version.
* See: http://www.gnu.org/licenses
*/
template<typename T>
class mpmc_bounded_queue
{
    public:
    mpmc_bounded_queue(size_t buffer_size)
        : buffer_(new cell_t [buffer_size])
        , buffer_mask_(buffer_size - 1)
    {
        assert((buffer_size >= 2) && ((buffer_size & (buffer_size - 1)) == 0));
        for (size_t i = 0; i != buffer_size; i += 1)
            buffer_[i].sequence_.store(i, std::memory_order_relaxed);
        enqueue_pos_.store(0, std::memory_order_relaxed);
        dequeue_pos_.store(0, std::memory_order_relaxed);
    }

    ~mpmc_bounded_queue()
    {
        delete [] buffer_;
    }

    bool enqueue(T const& data)
    {
        cell_t* cell;
        size_t pos = enqueue_pos_.load(std::memory_order_relaxed);
        for (;;)
        {
            cell = &buffer_[pos & buffer_mask_];
            size_t seq = cell->sequence_.load(std::memory_order_acquire);
            intptr_t dif = (intptr_t)seq - (intptr_t)pos;
            if (dif == 0)
            {
                if (enqueue_pos_.compare_exchange_weak(pos, pos + 1,
                    std::memory_order_relaxed))
                    break;
            }
            else if (dif < 0)
                return false;
            else
                pos = enqueue_pos_.load(std::memory_order_relaxed);
        }

        cell->data_ = data;
        cell->sequence_.store(pos + 1, std::memory_order_release);

        return true;
    }

    bool dequeue(T& data)
    {
        cell_t* cell;
        size_t pos = dequeue_pos_.load(std::memory_order_relaxed);
        for (;;)
        {
            cell = &buffer_[pos & buffer_mask_];
            size_t seq = cell->sequence_.load(std::memory_order_acquire);
            intptr_t dif = (intptr_t)seq - (intptr_t)(pos + 1);
            if (dif == 0)
            {
                if (dequeue_pos_.compare_exchange_weak(pos, pos + 1,
                    std::memory_order_relaxed))
                break;
            }
            else if (dif < 0)
                return false;
            else
                pos = dequeue_pos_.load(std::memory_order_relaxed);
        }

        data = cell->data_;
        cell->sequence_.store(pos + buffer_mask_ + 1,
        std::memory_order_release);

        return true;
    }

    private:
    struct cell_t
    {
        std::atomic<size_t> sequence_;
        T data_;
    };

    static size_t const cacheline_size = 64;
    typedef char cacheline_pad_t [cacheline_size];

    cacheline_pad_t pad0_;
    cell_t* const buffer_;
    size_t const buffer_mask_;
    cacheline_pad_t pad1_;
    std::atomic<size_t> enqueue_pos_;
    cacheline_pad_t pad2_;
    std::atomic<size_t> dequeue_pos_;
    cacheline_pad_t pad3_;

    mpmc_bounded_queue(mpmc_bounded_queue const&);
    void operator = (mpmc_bounded_queue const&);
};
#endif