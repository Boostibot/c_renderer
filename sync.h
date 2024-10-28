#pragma once

#include "lib/platform.h"
#include "lib/defines.h"


typedef enum {
    ATOMIC_WAIT_PAUSE = 0,  //Busy waiting with _mm_pause() or similar in between.
    ATOMIC_WAIT_YIELD,      //Yields this threads time slice to another thread when colliding.
    ATOMIC_WAIT_BLOCK,      //blocks untill value changes. Good when expecting long waits. Must be provided for both blocking and blocked threads!
    ATOMIC_WAIT_BUSY,       //raw busy waiting - do not use!
} Atomic_Wait;

enum {
    ATOMIC_CELL_EMPTY,
    ATOMIC_CELL_STORING, 
    ATOMIC_CELL_LOADING, 
    ATOMIC_CELL_FULL,
};
typedef volatile uint32_t Atomic_Cell;

ATTRIBUTE_INLINE_ALWAYS 
static void atomic_wake(volatile uint32_t* state, Atomic_Wait wait)
{
    if(wait == ATOMIC_WAIT_BLOCK)
        platform_futex_wake(state);
}

ATTRIBUTE_INLINE_ALWAYS 
static void atomic_wait_for(volatile uint32_t* state, uint32_t desired, Atomic_Wait wait)
{
    for(;;) {
        uint32_t current = platform_atomic_load32(state);
        if(current == desired)
            break;

        if(wait == ATOMIC_WAIT_PAUSE)
            platform_processor_pause();
        else if(wait == ATOMIC_WAIT_YIELD)
            platform_thread_yield();
        else if(wait == ATOMIC_WAIT_BLOCK)
            platform_futex_wait(state, current, -1);
    }
}

ATTRIBUTE_INLINE_ALWAYS 
static void atomic_wait_for_from_current(volatile uint32_t* state, uint32_t desired, uint32_t current, Atomic_Wait wait)
{
    for(;;) {
        if(wait == ATOMIC_WAIT_PAUSE)
            platform_processor_pause();
        else if(wait == ATOMIC_WAIT_YIELD)
            platform_thread_yield();
        else if(wait == ATOMIC_WAIT_BLOCK)
            platform_futex_wait(state, current, -1);
        
        uint32_t current = platform_atomic_load32(state);
        if(current == desired)
            break;
    }
}


#if 0
//Implementation based on https://github.com/max0x7ba/atomic_queue/tree/master
typedef struct Atomic_Array_Queue {
    ATTRIBUTE_ALIGNED(64) 
    volatile uint32_t top;
    uint32_t capacity; //must be power of two!
    uint32_t _1[14];

    ATTRIBUTE_ALIGNED(64) 
    volatile uint32_t bot;
    uint32_t _2[15];
} Atomic_Array_Queue;

//Remaps index to minimize false sharing of neighboring cache lines. 
//We want to achieve that uint32_t* states indices i and i+1 will be 64B apart.
//Thus each we i and i+1 input indices should be 64/4=16 output indices apart.
//We achieve this by swapping the bottom 4 bits of the index. For queues smaller then that
// we attempt no swapping.
ATTRIBUTE_INLINE_ALWAYS 
static uint32_t atomic_array_queue_remap_index(uint32_t index, uint32_t capacity)
{
    uint32_t mod = index & (capacity - 1);
    if(capacity < 16)
        return mod;
    else
    {
        enum {BITS = 4};
        uint32_t mask = (1u << BITS) - 1;
        uint32_t mix = (mod ^ (mod >> BITS)) & mask;
        return mod ^ mix ^ (mix << BITS);
    }
}

ATTRIBUTE_INLINE_ALWAYS 
static uint32_t atomic_array_queue_push(Atomic_Array_Queue* queue, volatile uint32_t* states, Atomic_Wait wait, void (*store)(uint32_t index, void*), void* context)
{
    uint32_t top = platform_atomic_add32(&queue->top, 1);
    uint32_t top_i = atomic_array_queue_remap_index(top, queue->capacity);

    volatile uint32_t* state = states + top_i;
    for(;;)
    {
        if(platform_atomic_cas32(state, ATOMIC_CELL_EMPTY, ATOMIC_CELL_STORING)) 
        {
            store(top_i, context);
            platform_atomic_store32(state, ATOMIC_CELL_FULL);
            if(wait == ATOMIC_WAIT_BLOCK)
                platform_futex_wake_all(state);
            return top + 1;
        }

        atomic_wait_for(state, ATOMIC_CELL_EMPTY, wait);
    }
}

ATTRIBUTE_INLINE_ALWAYS 
static uint32_t atomic_array_queue_pop(Atomic_Array_Queue* queue, volatile uint32_t* states, Atomic_Wait wait, void (*load)(uint32_t index, void*), void* context)
{
    uint32_t bot = platform_atomic_add32(&queue->bot, 1);
    uint32_t bot_i = atomic_array_queue_remap_index(bot, queue->capacity);
    //if(wait == ATOMIC_WAIT_BLOCK)
        //platform_futex_wake(&queue->bot);

    volatile uint32_t* state = states + bot_i;
    for(;;)
    {
        if(platform_atomic_cas32(state, ATOMIC_CELL_FULL, ATOMIC_CELL_LOADING)) 
        {
            load(bot_i, context);
            platform_atomic_store32(state, ATOMIC_CELL_EMPTY);
            if(wait == ATOMIC_WAIT_BLOCK)
                platform_futex_wake_all(state);
            return bot + 1;
        }
        
        atomic_wait_for(state, ATOMIC_CELL_FULL, wait);
    }
}

//Can be used to wait for to do "on demand producing"
//  uint32_t pos = atomic_array_queue_push(..., ATOMIC_WAIT_BLOCK);
//  atomic_array_queue_wait_for_pop(queue, pos, ATOMIC_WAIT_BLOCK); //block untill pushed value is popped.
ATTRIBUTE_INLINE_ALWAYS 
static uint32_t atomic_array_queue_wait_for_pop(Atomic_Array_Queue* queue, uint32_t position, Atomic_Wait wait)
{
    for(;;) {
        uint32_t bot = platform_atomic_load32(&queue->bot);
        if(bot > position)
            return bot;

        if(wait == ATOMIC_WAIT_PAUSE)
            platform_processor_pause();
        else if(wait == ATOMIC_WAIT_YIELD)
            platform_thread_yield();
        else if(wait == ATOMIC_WAIT_BLOCK)
            platform_futex_wait(&queue->bot, bot, -1);
    }
}

ATTRIBUTE_INLINE_ALWAYS 
static uint32_t atomic_array_queue_len(const Atomic_Array_Queue* queue)
{
    uint32_t bot = platform_atomic_load32(&queue->bot);
    uint32_t top = platform_atomic_load32(&queue->top);
    return top > bot ? top - bot : 0;
}

static Atomic_Array_Queue atomic_array_queue_make(int64_t capacity)
{
    uint64_t n = (uint64_t) capacity;
    assert((n & (n-1)) == 0 && capacity > 0 && "Must be power of two!");

    Atomic_Array_Queue out = {0};
    out.capacity = (uint32_t) capacity;
}
#endif

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

#include <stdint.h>
typedef struct _Gen_Ptr_* Gen_Ptr; 

typedef struct Unpacked_Gen_Ptr {
    void* ptr;
    uint64_t gen;
} Unpacked_Gen_Ptr;

Unpacked_Gen_Ptr gen_ptr_unpacked(Gen_Ptr ptr, isize aligned)
{
    Unpacked_Gen_Ptr unpacked = {0};
    uint64_t mul = ((uint64_t) 1 << 48)/(uint64_t) aligned;
    uint64_t ptr_bits = (uint64_t) ptr % mul;
    uint64_t gen_bits = (uint64_t) ptr / mul;

    uint64_t low_ptr_bits = (uint64_t) ptr_bits * (uint64_t) aligned;
    uint64_t high_ptr_bits = ((uint64_t) &unpacked) & (((uint64_t) 1 << 48) - 1);
    
    unpacked.gen = gen_bits;
    unpacked.ptr = (void*) (high_ptr_bits | low_ptr_bits);
    return unpacked;
}

Gen_Ptr gen_ptr_pack(void* ptr, uint64_t gen, isize aligned)
{
    uint64_t mul = ((uint64_t) 1 << 48)/(uint64_t) aligned;

    uint64_t low_ptr_bits = ((uint64_t) ptr / (uint64_t) aligned) % mul;
    uint64_t high_gen_bits = ((uint64_t) gen * mul);
    uint64_t ptr = high_gen_bits | low_ptr_bits;
    return (Gen_Ptr) ptr;
}

Unpacked_Gen_Ptr gen_ptr_load(void* addr, isize aligned)
{
    uint64_t curr = platform_atomic_load64(addr);
    Unpacked_Gen_Ptr unpacked = gen_ptr_unpacked((Gen_Ptr) curr, aligned);
    return unpacked;
}

void gen_ptr_store(void* addr, void* ptr, uint64_t gen, isize aligned)
{
    Gen_Ptr packed = gen_ptr_pack(ptr, gen, aligned);
    platform_atomic_store64(addr, (uint64_t) packed);
}

ATTRIBUTE_INLINE_ALWAYS 
static void atomic_stack_push_chain(volatile Gen_Ptr* list, isize aligned, void* first, void* last)
{
    for(;;) {
        uint64_t curr = platform_atomic_load64(list);
        Unpacked_Gen_Ptr curr_unpacked = gen_ptr_unpacked((Gen_Ptr) curr, aligned);

        uint64_t new_curr = (uint64_t) gen_ptr_pack(first, curr_unpacked.gen, aligned);

        platform_atomic_store64(last, (uint64_t) curr_unpacked.ptr);
        if(platform_atomic_cas64(list, curr, new_curr))
            break;
    }
}

ATTRIBUTE_INLINE_ALWAYS 
static void atomic_stack_push(volatile Gen_Ptr* list, isize aligned, void* node)
{
    atomic_stack_push_chain(list, aligned, node, node);
}

ATTRIBUTE_INLINE_ALWAYS 
static void* atomic_stack_pop(volatile Gen_Ptr* list, isize aligned)
{
    for(;;) {
        uint64_t curr = platform_atomic_load64(list);
        Unpacked_Gen_Ptr curr_unpacked = gen_ptr_unpacked((Gen_Ptr) curr, aligned);
        if(curr_unpacked.ptr == NULL)
            return NULL;

        uint64_t next_ptr = platform_atomic_load64(curr_unpacked.ptr);
        uint64_t next = (uint64_t) gen_ptr_pack((void*) next_ptr, curr_unpacked.gen + 1, aligned);
        if(platform_atomic_cas64(list, curr, next))
            return curr_unpacked.ptr;
    }
}

ATTRIBUTE_INLINE_ALWAYS 
static void* atomic_stack_pop_all(volatile Gen_Ptr* list, isize aligned)
{
    for(;;) {
        uint64_t curr = platform_atomic_load64(list);
        Unpacked_Gen_Ptr curr_unpacked = gen_ptr_unpacked((Gen_Ptr) curr, aligned);
        if(curr_unpacked.ptr == NULL)
            return NULL;

        uint64_t next = (uint64_t) gen_ptr_pack(NULL, curr_unpacked.gen + 1, aligned);
        if(platform_atomic_cas64(list, curr, next))
            return curr_unpacked.ptr;
    }
}

//DCAS using atomic stack
typedef union Fat_Gen_Ptr {
    struct {
        void* ptr;
        uint64_t gen;
    };
    
    struct {
        Fat_Gen_Node* node;
        uint64_t _;
    };

    struct {
        uint64_t lo;
        uint64_t hi;
    };
} Fat_Gen_Ptr;

typedef struct Fat_Gen_Node {
    Fat_Gen_Ptr next;
} Fat_Gen_Node;

ATTRIBUTE_INLINE_ALWAYS
static Fat_Gen_Ptr fat_gen_ptr_load(void* addr)
{
    Fat_Gen_Ptr ptr = {0};
    ptr.lo = platform_atomic_load64((uint64_t*) addr);
    ptr.hi = platform_atomic_load64((uint64_t*) addr + 1);
    return ptr;
}

ATTRIBUTE_INLINE_ALWAYS
static void fat_gen_ptr_store(void* addr, Fat_Gen_Ptr ptr)
{
    platform_atomic_store64((uint64_t*) addr, ptr.lo);
    platform_atomic_store64((uint64_t*) addr + 1, ptr.hi);
}

ATTRIBUTE_INLINE_ALWAYS
static bool fat_gen_ptr_is_equal(Fat_Gen_Ptr a, Fat_Gen_Ptr b)
{
    return a.lo == b.lo && a.hi == b.hi;
}
ATTRIBUTE_INLINE_ALWAYS
static bool fat_gen_ptr_compare_and_swap(Fat_Gen_Ptr* target, Fat_Gen_Ptr old_value, Fat_Gen_Ptr new_value)
{
    return platform_atomic_cas128(target, old_value.lo, old_value.hi, new_value.lo, new_value.hi);
}
ATTRIBUTE_INLINE_ALWAYS
static bool fat_gen_ptr_compare_and_swap_weak(Fat_Gen_Ptr* target, Fat_Gen_Ptr old_value, Fat_Gen_Ptr new_value)
{
    return platform_atomic_cas_weak128(target, old_value.lo, old_value.hi, new_value.lo, new_value.hi);
}

ATTRIBUTE_INLINE_ALWAYS 
static void fat_atomic_stack_push_chain(Fat_Gen_Ptr* list, void* first, void* last)
{
    //DCAS is not necessary here since we require node to be exclusively owned
    atomic_list_push_chain((volatile void**) (void*) list, first, last);
}

ATTRIBUTE_INLINE_ALWAYS 
static void fat_atomic_stack_push(Fat_Gen_Ptr* list, void* node)
{
    fat_atomic_stack_push_chain(list, node, node);
}

ATTRIBUTE_INLINE_ALWAYS 
static void* fat_atomic_stack_pop(Fat_Gen_Ptr* list)
{
    for(;;) {
        Fat_Gen_Ptr curr = fat_gen_ptr_load(list);
        if(curr.ptr == NULL)
            return NULL;

        uint64_t next_ptr = platform_atomic_load64(&curr.node->next.node);
        Fat_Gen_Ptr next = {(Fat_Gen_Node*) next_ptr, curr.gen + 1};
        if(fat_gen_ptr_compare_and_swap(list, curr, next))
            return curr.ptr;
    }
}

ATTRIBUTE_INLINE_ALWAYS 
static void* fat_atomic_stack_pop_all(Fat_Gen_Ptr* list)
{
    for(;;) {
        Fat_Gen_Ptr curr = fat_gen_ptr_load(list);
        Fat_Gen_Ptr next = {0, curr.gen + 1}; 
        if(fat_gen_ptr_compare_and_swap_weak(list, curr, next))
            return curr.ptr;
    }
}

typedef struct Atomic_Link_Queue {
    ATTRIBUTE_ALIGNED(64) volatile Fat_Gen_Ptr head; //Owned by consumer threads
    uint8_t head_pad[6];

    ATTRIBUTE_ALIGNED(64) volatile Fat_Gen_Ptr tail; //Owned by producer threads
    uint8_t tail_pad[6];
} Atomic_Link_Queue;

typedef struct Atomic_Queue_Config{
    int32_t node_size;
    int32_t alloc_at_once;
    int32_t item_size;
} Atomic_Queue_Config;

ATTRIBUTE_INLINE_ALWAYS 
void atomic_linked_queue_init(Atomic_Link_Queue* queue, Atomic_Queue_Config config)
{
    Fat_Gen_Node* node = NULL; //todo
    Fat_Gen_Ptr sentinel = {node, 0};

    fat_gen_ptr_store(&queue->head, sentinel);
    fat_gen_ptr_store(&queue->tail, sentinel);

    platform_memory_barrier();
}

ATTRIBUTE_INLINE_ALWAYS 
void atomic_linked_queue_deinit(Atomic_Link_Queue* queue, Atomic_Queue_Config config)
{
    memset(queue, 0, sizeof *queue);

    platform_memory_barrier();
}

ATTRIBUTE_INLINE_ALWAYS 
void atomic_linked_queue_push(Atomic_Link_Queue* queue, const void* memory, Atomic_Queue_Config config)
{
    Fat_Gen_Node* node = NULL; //todo
    memcpy(node + 1, memory, config.item_size);

    Fat_Gen_Ptr tail = {0};
    Fat_Gen_Ptr next = {0};
    for(;;)
    {
        tail = fat_gen_ptr_load(&queue->tail);
        next = fat_gen_ptr_load(&tail.node->next);

        if(fat_gen_ptr_is_equal(tail, fat_gen_ptr_load(&queue->tail)))
        {
            if(next.ptr == NULL)
            {
                Fat_Gen_Ptr new_next = {node, next.gen + 1};
                if(fat_gen_ptr_compare_and_swap(&tail.node->next, next, new_next))
                    break;
            }
            else
            {
                Fat_Gen_Ptr new_tail = {next.ptr, tail.gen + 1};
                fat_gen_ptr_compare_and_swap(&queue->tail, tail, new_tail);
            }
        }
    }
    
    Fat_Gen_Ptr new_tail = {node, tail.gen + 1};
    fat_gen_ptr_compare_and_swap(&queue->tail, tail, new_tail);
}

ATTRIBUTE_INLINE_ALWAYS 
bool atomic_linked_queue_pop(Atomic_Link_Queue* queue, void* memory, Atomic_Queue_Config config)
{
    Fat_Gen_Ptr head = {0};
    Fat_Gen_Ptr tail = {0};
    Fat_Gen_Ptr next = {0};
    for(;;)
    {
        head = fat_gen_ptr_load(&queue->head);
        tail = fat_gen_ptr_load(&queue->tail);
        next = fat_gen_ptr_load(&head.node->next);

        if(fat_gen_ptr_is_equal(head, fat_gen_ptr_load(&queue->head)))
        {
            if(head.ptr == tail.ptr)
            {   
                if(next.ptr == NULL)
                    return false;

                Fat_Gen_Ptr new_tail = {next.ptr, tail.gen + 1};
                fat_gen_ptr_compare_and_swap(&queue->tail, tail, new_tail);
            }
            else
            {
                memcpy(memory, next.node + 1, config.item_size);
                Fat_Gen_Ptr new_head = {next.ptr, head.gen + 1};
                if(fat_gen_ptr_compare_and_swap(&queue->head, head, new_head))
                    break;
            }
        }
    }
    
    //todo free head.ptr
    return true;
}

#define EBR_LOCKED ((uint64_t) 1 << 63)

typedef struct EBR_Thread_State {
    struct EBR_Thread_State* next;
    volatile uint64_t time_and_is_locked;
} EBR_Thread_State;

void ebr_critical_section_enter(volatile EBR_Thread_State* this_thread)
{
    platform_atomic_add64(this_thread->time_and_is_locked, EBR_LOCKED);
}

void ebr_critical_section_leave(volatile EBR_Thread_State* this_thread)
{
    platform_atomic_add64(this_thread->time_and_is_locked, EBR_LOCKED + 1);
}

void ebr_critical_section_wait_for_exclusivity(volatile EBR_Thread_State* this_thread, volatile EBR_Thread_State* thread_states)
{
    assert((platform_atomic_load64(this_thread->time_and_is_locked) & EBR_LOCKED) == 0);

    enum {MAX_THREADS = 256};
    uint64_t  prev_times[MAX_THREADS]; (void) prev_times;
    uint64_t* live_times[MAX_THREADS]; (void) live_times;

    //Collect all threads that are currently in the critical section (and arent this thread)
    isize in_crit_section = 0; 
    for(EBR_Thread_State* curr = (EBR_Thread_State*) platform_atomic_load64(thread_states); curr; curr = curr->next)
    {
        if(curr != this_thread) //maybe remove? One load is very minor
        {
            uint64_t state = platform_atomic_load64(&curr->time_and_is_locked);
            if(state & EBR_LOCKED)
            {
                assert(in_crit_section <= MAX_THREADS);
                live_times[in_crit_section] = &curr->time_and_is_locked;
                prev_times[in_crit_section] = state;
                in_crit_section += 1;
            }
        }
    }
            
    //wait for all threads to leave
    for(isize i = 0; i < in_crit_section; i++)
    {
        for(;;) {
            uint64_t time = platform_atomic_load64(live_times[i]);
            if(time != prev_times[i])
                break;

            platform_processor_pause();
        }
    }
}

typedef struct EBR_Stack_Shared {
    ATTRIBUTE_ALIGNED(64) volatile void* head;

    volatile EBR_Thread_State* thread_state_list;
    volatile isize thread_state_count;
} EBR_Stack_Shared;

typedef struct EBR_Stack {
    EBR_Stack_Shared* shared;
    EBR_Thread_State* local;
} EBR_Stack;

void ebr_stack_push(EBR_Stack ebr, void* node)
{
    //node is exclusively owned so we dont need to go into critical section!
    atomic_list_push(&ebr.shared->head, node);
}

void* ebr_stack_pop(EBR_Stack ebr)
{
    EBR_Stack_Shared* shared = ebr.shared;
    EBR_Thread_State* local = ebr.local;

    ebr_critical_section_enter(local);

    for(;;) {
        uint64_t head = platform_atomic_load64(&shared->head);
        if(head == 0)
            break;

        uint64_t next = platform_atomic_load64((void*) head);
        if(platform_atomic_cas64(&shared->head, head, next))
        {
            ebr_critical_section_leave(local);
            ebr_critical_section_wait_for_exclusivity(local, shared->thread_state_list);
            return (void*) head;
        }
    }
    
    ebr_critical_section_leave(local);
    return NULL;
}

#ifndef ASSERT
    #include <assert.h>
    #define ASSERT(x, ...) assert(x)
#endif

typedef struct Squeue_Block {
    struct Squeue_Block* next;
    uint64_t mask; //capacity - 1
} Squeue_Block;

enum {
    SQUEUE_DISSALOW_GROW = 1,
};

typedef struct Squeue {
    volatile Squeue_Block* block;
    
    volatile uint64_t top; //owned by consumer
    volatile uint64_t bot; //owned by producer

    volatile uint32_t last_item_size;
    volatile uint32_t flags;
} Squeue;

ATTRIBUTE_INLINE_ALWAYS
void* _squeue_slot(Squeue_Block* block, uint64_t i, isize item_size)
{
    uint8_t* data = (int*) (void*) (block + 1);
    return data + (i & block->mask)*item_size;
}

ATTRIBUTE_INLINE_NEVER
Squeue_Block* squeue_grow_block(Squeue* queue, isize to_size, isize item_size)
{
    ASSERT(queue->last_item_size == (uint32_t) item_size || queue->last_item_size == 0);

    Squeue_Block* old_block = queue->block;
    Squeue_Block* out_block = old_block;
    uint64_t old_cap = old_block ? (old_block->mask + 1) : 0;

    if((queue->flags & SQUEUE_DISSALOW_GROW) == 0 && to_size > old_cap)
    {
        uint64_t new_cap = 16;
        while((isize) new_cap < to_size)
            new_cap *= 2;

        Squeue_Block* new_block = (Squeue_Block*) platform_heap_reallocate(sizeof(Squeue_Block) + new_cap*item_size, NULL, CACHE_LINE);
        new_block->next = old_block;
        new_block->mask = new_cap - 1;

        if(old_block)
        {
            uint64_t t = platform_atomic_load64(&queue->top);
            uint64_t b = platform_atomic_load64(&queue->bot);
            for(uint64_t i = t; i < b; i++)
                memcpy(_squeue_slot(new_block, i, item_size), _squeue_slot(old_block, i, item_size), item_size);
        }

        queue->last_item_size = (uint32_t) item_size;

        platform_atomic_store64(&queue->block, (uint64_t) new_block);
        out_block = new_block;
    }

    return out_block;
}

ATTRIBUTE_INLINE_ALWAYS
void squeue_reserve(Squeue* queue, isize to_size, isize item_size)
{
    Squeue_Block* block = queue->block;
    if(block == NULL || to_size + 1 > block->mask)
        return squeue_grow_block(queue, to_size, item_size);
}

ATTRIBUTE_INLINE_ALWAYS
bool squeue_push(Squeue* queue, const void* value, isize item_size)
{
    ASSERT(queue->last_item_size == (uint32_t) item_size || queue->last_item_size == 0);
    
    uint64_t b = platform_atomic_load64(&queue->bot);
    uint64_t t = platform_atomic_load64(&queue->top);
    isize size = (isize) b - (isize) t;
    
    Squeue_Block* block = queue->block;
    if(block == NULL || size + 1 > (isize) block->mask)
    {
        if(queue->flags & SQUEUE_DISSALOW_GROW)
            return false;
        block = squeue_grow_block(queue, size + 1, item_size);
    }

    memcpy(_squeue_slot(block, b, item_size), value, item_size);
    queue->bot = b + 1;
    return true;
}

ATTRIBUTE_INLINE_ALWAYS
isize squeue_steal_weak(Squeue* queue, void* value, isize item_size)
{
    Squeue_Block* block = queue->block;

    uint64_t t = platform_atomic_load64(&queue->top);
    uint64_t b = platform_atomic_load64(&queue->bot);
    isize size = (isize) b - (isize) t;

    if(size > 0)
    {
        memcpy(value, _squeue_slot(block, t, item_size), item_size);
        if(platform_atomic_cas_weak64(&queue->top, t, t + 1))
            return 1;
    }
    return 0;
}

ATTRIBUTE_INLINE_ALWAYS
isize squeue_pop_weak(Squeue* queue, void* value, isize item_size)
{
    ASSERT(queue->last_item_size == (uint32_t) item_size || queue->last_item_size == 0);
    Squeue_Block* block = queue->block;
    
    uint64_t b = platform_atomic_load64(&queue->bot) - 1;
    uint64_t t = platform_atomic_load64(&queue->top);
    
    //prevents calls to squeue_pop_front from succeeding
    platform_atomic_store64(&queue->bot, b); 

    //is there something in the queue?
    if(t <= b)
    {
        //Is there a possibility of a race against steal operation 
        // called between we loaded b and set queue->bot = b? 
        if(t == b)
        {
            //Try to move top. Either this thread or the stealing thread will succeed
            if(platform_atomic_cas64(&queue->top, t, t + 1) == false)
                goto fail;

            //restore back original pos
            platform_atomic_store64(&queue->bot, b + 1);
        }

        //We can delay load to only after the race because we are the only thread accessing b.
        memcpy(value, _squeue_slot(block, b, item_size), item_size);
        return 1;
    }   

    fail:
    {
        //restore back the original position of top
        platform_atomic_store64(&queue->bot, b + 1);
        return 0;
    }
}

ATTRIBUTE_INLINE_ALWAYS
isize squeue_steal(Squeue* queue, void* value, isize count, isize item_size)
{
    for(;;) {
        Squeue_Block* block = queue->block;

        uint64_t t = platform_atomic_load64(&queue->top);
        uint64_t b = platform_atomic_load64(&queue->bot);
        isize size = (isize) b - (isize) t;
        isize to_pop = 1;
        if(count > 1)
            to_pop = size < count ? size : count;

        if(size <= 0)
            return 0;
        else
        {
            for(uint64_t i = 0; i < to_pop; i++)
                memcpy(value, _squeue_slot(block, t + i, item_size), item_size);
            if(platform_atomic_cas_weak64(&queue->top, t, t + to_pop))
                return to_pop;
        }
    }
}

ATTRIBUTE_INLINE_ALWAYS
isize squeue_pop(Squeue* queue, void* value, isize count, isize item_size)
{
    //for comments see squeue_pop_weak.
    ASSERT(queue->last_item_size == (uint32_t) item_size || queue->last_item_size == 0);

    for(;;) {
        Squeue_Block* block = queue->block;
    
        uint64_t b = platform_atomic_load64(&queue->bot);
        uint64_t t = platform_atomic_load64(&queue->top);
        isize size = (isize) b - (isize) t;
    
        isize to_pop = 1;
        if(count > 1)
            to_pop = size < count ? size : count;

        platform_atomic_store64(&queue->bot, b - to_pop); 
        if(size > 0)
        {
            if(size == to_pop)
            {
                if(platform_atomic_cas64(&queue->top, t, t + to_pop) == false)
                {
                    platform_atomic_store64(&queue->bot, b);
                    continue;
                }
                platform_atomic_store64(&queue->bot, b);
            }
            
            for(uint64_t i = 0; i < to_pop; i++)
                memcpy(value, _squeue_slot(block, b + i, item_size), item_size);
        }   
        else
        {
            platform_atomic_store64(&queue->bot, b);
            return 0;
        }
    }
}

void squeue_deinit(Squeue* queue)
{
    for(Squeue_Block* curr = queue->block; curr; )
    {
        Squeue_Block* next = curr->next;
        platform_heap_reallocate(0, curr, 64);
        curr = next;
    }

    memset(queue, 0, sizeof *queue);
}

void squeue_init(Squeue* queue, isize to_size, isize item_size)
{
    squeue_deinit(queue);
    if(to_size > 0)
        squeue_grow_block(queue, to_size, item_size);
}



typedef struct Growing_Queue_Shared {
    volatile uint32_t refs;
    volatile uint32_t last_item_size;
    
} Growing_Queue_Shared;




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
