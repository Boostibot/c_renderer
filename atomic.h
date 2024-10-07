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
static void atomic_wait_for(volatile uint32_t* state, uint32_t desired, uint32_t undesired, Atomic_Wait wait)
{
    do {
        if(wait == ATOMIC_WAIT_PAUSE)
            platform_processor_pause();
        else if(wait == ATOMIC_WAIT_YIELD)
            platform_thread_yield();
        else if(wait == ATOMIC_WAIT_BLOCK)
        {
            platform_processor_pause();
            platform_futex_wait(state, undesired, -1);
        }
    } while(platform_atomic_load32(state) != desired);
}

ATTRIBUTE_INLINE_ALWAYS 
static void atomic_cell_store(Atomic_Cell* cell, Atomic_Wait wait, void (*store)(void*), void* context)
{
    for(;;)
    {
        if(platform_atomic_compare_and_swap32(cell, ATOMIC_CELL_EMPTY, ATOMIC_CELL_STORING)) 
        {
            store(context);
            platform_atomic_store32(cell, ATOMIC_CELL_FULL);
            if(wait == ATOMIC_WAIT_BLOCK)
                platform_futex_wake(cell);
            return;
        }

        atomic_wait_for(cell, ATOMIC_CELL_EMPTY, ATOMIC_CELL_FULL, wait);
    }
}

//Call from the storing side to wait for the load to happen. Can be used to implemt go style channels.
ATTRIBUTE_INLINE_ALWAYS 
static void atomic_cell_wait_after_store(Atomic_Cell* cell, Atomic_Wait wait)
{
    atomic_wait_for(cell, ATOMIC_CELL_EMPTY, ATOMIC_CELL_FULL, wait);
}

ATTRIBUTE_INLINE_ALWAYS 
static void atomic_cell_load(Atomic_Cell* cell, Atomic_Wait wait, void (*load)(void*), void* context)
{
    for(;;)
    {
        if(platform_atomic_compare_and_swap32(cell, ATOMIC_CELL_FULL, ATOMIC_CELL_LOADING)) 
        {
            load(context);
            platform_atomic_store32(cell, ATOMIC_CELL_EMPTY);
            if(wait == ATOMIC_WAIT_BLOCK)
                platform_futex_wake(cell);
            return;
        }
        
        atomic_wait_for(cell, ATOMIC_CELL_FULL, ATOMIC_CELL_EMPTY, wait);
    }
}

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
        return index ^ mix ^ (mix << BITS);
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
        if(platform_atomic_compare_and_swap32(state, ATOMIC_CELL_EMPTY, ATOMIC_CELL_STORING)) 
        {
            store(top_i, context);
            platform_atomic_store32(state, ATOMIC_CELL_FULL);
            if(wait == ATOMIC_WAIT_BLOCK)
                platform_futex_wake(state);
            return top + 1;
        }

        atomic_wait_for(state, ATOMIC_CELL_EMPTY, ATOMIC_CELL_FULL, wait);
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
        if(platform_atomic_compare_and_swap32(state, ATOMIC_CELL_FULL, ATOMIC_CELL_LOADING)) 
        {
            load(bot_i, context);
            platform_atomic_store32(state, ATOMIC_CELL_EMPTY);
            if(wait == ATOMIC_WAIT_BLOCK)
                platform_futex_wake(state);
            return bot + 1;
        }
        
        atomic_wait_for(state, ATOMIC_CELL_FULL, ATOMIC_CELL_EMPTY, wait);
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

//Growing structures below. Because of the ABA problem caused by memory reclamation they all require DCAS or a some virtual memory tricks
// (reserve huge block, replace ptrs with 32 bit indices, pack a 32 bit generation field alongside the index and use CAS). 
// The main problem is as follows: we allocate a node (using malloc or a freelist, etc.), insert into datastructure, remove from datastructure, 
// allocate a new node at the position of the old node (same address or pointer, key, etc.). A racing thread which was put to sleep mid way through the removal 
// step gets woken up and does CAS on some shared pointer. It thinks its comparing an old node which should not succeed in the CAS 
// but because we inserted a new one at the same ptr it succeeds. Datastructure broken.
// 
// Alternatively we could use a more clever technique like Epoch-Based Reclamation (EBR), 
// Quiescent-State-Based Reclamation (QSBR) or Hazard-Pointer-Based Reclamation (HPBR).
// Read: T. E. Hart, P. E. McKenney and A. D. Brown, "Making lockless synchronization fast: performance implications of memory reclamation" 
// url: https://sysweb.cs.toronto.edu/publication_files/0000/0165/ipdps06.pdf


ATTRIBUTE_INLINE_ALWAYS 
static void atomic_list_push_chain(volatile void** list, void* first, void* last)
{
    for(;;) {
        uint64_t curr = platform_atomic_load64(list);
        platform_atomic_store64(last, curr);
        if(platform_atomic_compare_and_swap_weak64(list, curr, (uint64_t) first))
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

typedef struct Gen_Node Gen_Node;

typedef union Gen_Ptr {
    struct {
        void* ptr;
        uint64_t gen;
    };
    
    struct {
        Gen_Node* node;
        uint64_t _;
    };

    struct {
        uint64_t lo;
        uint64_t hi;
    };
} Gen_Ptr;

typedef struct Gen_Node {
    Gen_Ptr next;
} Gen_Node;

ATTRIBUTE_INLINE_ALWAYS
static Gen_Ptr gen_ptr_load(void* addr)
{
    Gen_Ptr ptr = {0};
    ptr.lo = platform_atomic_load64((uint64_t*) addr);
    ptr.hi = platform_atomic_load64((uint64_t*) addr + 1);
    return ptr;
}

ATTRIBUTE_INLINE_ALWAYS
static void gen_ptr_store(void* addr, Gen_Ptr ptr)
{
    platform_atomic_store64((uint64_t*) addr, ptr.lo);
    platform_atomic_store64((uint64_t*) addr + 1, ptr.hi);
}

ATTRIBUTE_INLINE_ALWAYS
static bool gen_ptr_is_equal(Gen_Ptr a, Gen_Ptr b)
{
    return a.lo == b.lo && a.hi == b.hi;
}
ATTRIBUTE_INLINE_ALWAYS
static bool gen_ptr_compare_and_swap(Gen_Ptr* target, Gen_Ptr old_value, Gen_Ptr new_value)
{
    return platform_atomic_compare_and_swap128(target, old_value.lo, old_value.hi, new_value.lo, new_value.hi);
}
ATTRIBUTE_INLINE_ALWAYS
static bool gen_ptr_compare_and_swap_weak(Gen_Ptr* target, Gen_Ptr old_value, Gen_Ptr new_value)
{
    return platform_atomic_compare_and_swap_weak128(target, old_value.lo, old_value.hi, new_value.lo, new_value.hi);
}

ATTRIBUTE_INLINE_ALWAYS 
static void atomic_stack_push_chain(Gen_Ptr* list, void* first, void* last)
{
    //DCAS is not necessary here since we require node to be exclusively owned
    atomic_list_push_chain((volatile void**) (void*) list, first, last);
}

ATTRIBUTE_INLINE_ALWAYS 
static void atomic_stack_push(Gen_Ptr* list, void* node)
{
    atomic_stack_push_chain(list, node, node);
}

ATTRIBUTE_INLINE_ALWAYS 
static void* atomic_stack_pop(Gen_Ptr* list)
{
    for(;;) {
        Gen_Ptr curr = gen_ptr_load(list);
        if(curr.ptr == NULL)
            return NULL;

        uint64_t next_ptr = platform_atomic_load64(&curr.node->next.node);
        Gen_Ptr next = {(Gen_Node*) next_ptr, curr.gen + 1};
        if(gen_ptr_compare_and_swap(list, curr, next))
            return curr.ptr;
    }
}

ATTRIBUTE_INLINE_ALWAYS 
static void* atomic_stack_pop_all(Gen_Ptr* list)
{
    for(;;) {
        Gen_Ptr curr = gen_ptr_load(list);
        Gen_Ptr next = {0, curr.gen + 1}; 
        if(gen_ptr_compare_and_swap_weak(list, curr, next))
            return curr.ptr;
    }
}

#if 0
typedef struct Lock_Queue_Node {
    volatile struct Lock_Queue* next;
    bool was_alloced;
    uint64_t padding[7];
    // data
} Lock_Queue_Node;

typedef struct Lock_Queue_Chain {
    Lock_Queue_Node* first;
    Lock_Queue_Node* last;
    isize count;
} Lock_Queue_Chain;

enum {
    LOCK_QUEUE_SINGLE_PRODUCER = 1, //do not take lock during push operations because there is just oen thread pushing.
    LOCK_QUEUE_SINGLE_CONSUMER = 2, //do not take lock during pop operations because there is just oen thread popping.
    LOCK_QUEUE_WAIT_PAUSE = 4,       //do not call futex wake on push. threads calling pop_wait will spin calling _mm_pause or similar
    LOCK_QUEUE_WAIT_YIELD = 8,       //do not call futex wake on push. threads calling pop_wait will spin yielding the threads time slice to the os
    LOCK_QUEUE_DONT_PAD_TO_CACHE_LINE = 16,
};

typedef struct Lock_Queue {
    //Owned by consumer threads
    ATTRIBUTE_ALIGNED(64) volatile Lock_Queue_Node* head;
    Platform_Mutex head_lock;
    uint64_t _head_pad[6];

    //Owned by producer threads
    ATTRIBUTE_ALIGNED(64) volatile Lock_Queue_Node* tail;
    Platform_Mutex tail_lock;
    uint64_t _tail_pad[6];

    //shared between threads 
    ATTRIBUTE_ALIGNED(64) 
    volatile Lock_Queue_Node* free_list;
    volatile isize len;
    
    isize item_size;
    isize node_size;
    isize alloc_at_once;
    isize references;
    isize flags;
    uint64_t _list_pad[5];
} Lock_Queue;

void lock_queue_init(Lock_Queue* queue, isize item_size, int flags)
{
    memset(queue, 0, sizeof *queue);
    platform_mutex_init(&queue->head_lock);
    platform_mutex_init(&queue->tail_lock);

    if(flags & LOCK_QUEUE_DONT_PAD_TO_CACHE_LINE)
        queue->node_size = item_size + sizeof(Lock_Queue_Node);
    else
        queue->node_size = (item_size + sizeof(Lock_Queue_Node) + 63)/64*64;
    queue->flags = flags;
    queue->references = 1;
    queue->alloc_at_once = 8;

    Lock_Queue_Node* node = lock_queue_alloc_node(queue);
    queue->head = node;
    queue->tail = node;
}

bool lock_queue_deinit(Lock_Queue* queue)
{
    isize before_refs = platform_atomic_sub64(&queue->references, 1);
    if(before_refs == 1)
    {
        //just in case...
        if((queue->flags & LOCK_QUEUE_SINGLE_PRODUCER) == 0) platform_mutex_lock(&queue->tail_lock);
        if((queue->flags & LOCK_QUEUE_SINGLE_CONSUMER) == 0) platform_mutex_lock(&queue->head_lock);

        //We dont know the order of the nodes and the nodes are allocated by blocks.
        //This means we cannot simply free all nodes marked with was_alloced.
        //Instead we first collect all of them into a list and then free them all.
        Lock_Queue_Node* list_to_free = NULL;

        //collect all used allocated nodes
        for(Lock_Queue_Node* curr = queue->head; curr != NULL;)
        {
            Lock_Queue_Node* next = curr->next;
            if(curr->was_alloced)
            {
                curr->next = list_to_free;
                list_to_free = curr;
            }

            curr = next;
        }

        //collect all unsued allocated nodes
        for(Lock_Queue_Node* curr = queue->free_list; curr != NULL;)
        {
            Lock_Queue_Node* next = curr->next;
            if(curr->was_alloced)
            {
                curr->next = list_to_free;
                list_to_free = curr;
            }

            curr = next;
        }
        
        //free all collected
        for(Lock_Queue_Node* curr = list_to_free; curr != NULL;)
        {
            assert(curr->was_alloced);
            Lock_Queue_Node* next = curr->next;
            free(curr);
            curr = next;
        }

        if((queue->flags & LOCK_QUEUE_SINGLE_PRODUCER) == 0) platform_mutex_unlock(&queue->tail_lock);
        if((queue->flags & LOCK_QUEUE_SINGLE_CONSUMER) == 0) platform_mutex_unlock(&queue->head_lock);

        platform_mutex_deinit(&queue->head_lock);
        platform_mutex_deinit(&queue->tail_lock);

        memset(queue, 0, sizeof *queue);
    }

    return before_refs == 1;
}

Lock_Queue* lock_queue_share(Lock_Queue* queue)
{
    if(platform_atomic_load64(&queue->references) > 0)
        platform_atomic_add64(&queue->references, 1);
    return queue;
}

Lock_Queue_Node* lock_queue_alloc_node(Lock_Queue* queue)
{
    assert(queue->references > 0);
    assert(queue->node_size >= sizeof(Lock_Queue_Node));
    Lock_Queue_Node* node = atomic_stack_pop(&queue->free_list);
    if(node == NULL)
    {
        enum {AT_ONCE = 8};
        isize size = queue->node_size;
        uint8_t* alloced = (uint8_t*) calloc(1, AT_ONCE*size);

        for(int i = 1; i < AT_ONCE; i++)
        {
            Lock_Queue_Node* prev = (Lock_Queue_Node*) (alloced + (i-1)*size);
            Lock_Queue_Node* curr = (Lock_Queue_Node*) (alloced + i*size);
            prev[i].next = curr;
        }

        Lock_Queue_Node* first = (Lock_Queue_Node*) (alloced + 0*size);
        Lock_Queue_Node* last = (Lock_Queue_Node*) (alloced + (AT_ONCE-1)*size);
        first->was_alloced = true;

        node = first;
        atomic_stack_push_chain(&queue->free_list, first->next, last);
        platform_memory_barrier();
    }

    node->next = NULL;
    return node;
}

void lock_queue_recycle(Lock_Queue* queue, Lock_Queue_Node* node)
{
    atomic_stack_push_chain(&queue->free_list, node, node);
}

void lock_queue_recycle_many(Lock_Queue* queue, Lock_Queue_Chain chain)
{
    atomic_stack_push_chain(&queue->free_list, chain.first, chain.last);
}

void lock_queue_push_many(Lock_Queue* queue, Lock_Queue_Chain chain)
{
    assert(queue->references > 0);
    assert(chain.last->next == NULL);
    if((queue->flags & LOCK_QUEUE_SINGLE_PRODUCER) == 0)
        platform_mutex_lock(&queue->tail_lock);
    
    platform_compiler_barrier();
    queue->tail->next = chain.first;
    queue->tail = chain.last;
    platform_compiler_barrier();
    
    if((queue->flags & LOCK_QUEUE_SINGLE_PRODUCER) == 0)
        platform_mutex_unlock(&queue->tail_lock);

    platform_atomic_add64(&queue->len, chain.count);
    if((queue->flags & (LOCK_QUEUE_WAIT_PAUSE | LOCK_QUEUE_WAIT_YIELD)))
        platform_futex_wake(&queue->len);
}

void lock_queue_push(Lock_Queue* queue, const void* data)
{
    Lock_Queue_Node* node = lock_queue_alloc_node(queue);
    memcpy(node + 1, data, queue->item_size);

    Lock_Queue_Chain chain = {node, node, 1};
    lock_queue_push_many(queue, chain);
}

Lock_Queue_Chain lock_queue_pop_many(Lock_Queue* queue, isize count)
{
    Lock_Queue_Chain out = {0};
    assert(queue->references > 0);
    if((queue->flags & LOCK_QUEUE_SINGLE_CONSUMER) == 0)
        platform_mutex_lock(&queue->head_lock);
    platform_compiler_barrier();

    for(isize i = 0; i < count; i++)
    {
        Lock_Queue_Node* node = queue->head;
        Lock_Queue_Node* new_head = queue->head->next;
        if(new_head == NULL)
            break;
        else
        {
            memcpy(node + 1, new_head + 1, queue->item_size);
            node->next = NULL;
            if(i == 0)
                out.first = node;
            else
                out.last->next = node;
                
            out.last = node;
            out.count += 1;
            queue->head = new_head;
        }
    }
        
    platform_atomic_sub64(&queue->len, out.count);
    platform_compiler_barrier();
    if((queue->flags & LOCK_QUEUE_SINGLE_CONSUMER) == 0)
        platform_mutex_unlock(&queue->head_lock);

    return out;
}

Lock_Queue_Chain lock_queue_pop_all(Lock_Queue* queue)
{
    return lock_queue_pop_many(queue, INT64_MAX);
}

bool lock_queue_pop(Lock_Queue* queue, void* out_block)
{
    assert(queue->references > 0);
    if((queue->flags & LOCK_QUEUE_SINGLE_CONSUMER) == 0)
        platform_mutex_lock(&queue->head_lock);
    platform_compiler_barrier();

    Lock_Queue_Node* node = queue->head;
    Lock_Queue_Node* new_head = queue->head->next;
    assert(node != NULL);
    if(new_head != NULL)
    {
        memcpy(out_block, new_head + 1, queue->item_size);
        queue->head = new_head;
    }
        
    platform_compiler_barrier();
    if((queue->flags & LOCK_QUEUE_SINGLE_CONSUMER) == 0)
        platform_mutex_unlock(&queue->head_lock);

    return new_head != NULL;
}

bool lock_queue_pop_wait(Lock_Queue* queue, void* out_block)
{
    for(;;)
    { 
        //wait for len != 0
        while(platform_atomic_load64(queue->len) == 0)
        {
            if(queue->flags & LOCK_QUEUE_WAIT_PAUSE)
                platform_processor_pause();
            else if(queue->flags & LOCK_QUEUE_WAIT_YIELD)
                platform_thread_yield();
            else 
                platform_futex_wait(&queue->len, 0, -1);
        }
        
        //take mutex and try to pop. 
        //If in the meantime of us waiting for the mutex someone else 
        // already popped either go back to waiting
        assert(queue->references > 0);
        if((queue->flags & LOCK_QUEUE_SINGLE_CONSUMER) == 0)
            platform_mutex_lock(&queue->head_lock);
        if(platform_atomic_load64(queue->len) > 0)
        {
            platform_compiler_barrier();

            Lock_Queue_Node* node = queue->head;
            Lock_Queue_Node* new_head = queue->head->next;
            assert(new_head != NULL);
            assert(node != NULL);

            memcpy(out_block, new_head + 1, queue->item_size);
            queue->head = new_head;
        
            platform_compiler_barrier();
            platform_atomic_sub64(&queue->len, 1);
        }
        if((queue->flags & LOCK_QUEUE_SINGLE_CONSUMER) == 0)
            platform_mutex_unlock(&queue->head_lock);
    }


    return true;
}

void lock_queue_pop_atomic_wait(Lock_Queue* queue, void* out_block)
{
    for(;;)
    { 
        //wait for len != 0
        while(platform_atomic_load64(queue->len) == 0)
        {
            if(queue->flags & LOCK_QUEUE_WAIT_PAUSE)
                platform_processor_pause();
            else if(queue->flags & LOCK_QUEUE_WAIT_YIELD)
                platform_thread_yield();
            else 
                platform_futex_wait(&queue->len, 0, -1);
        }

        //attempt to pop
        Lock_Queue_Node* node = queue->head;
        Lock_Queue_Node* new_head = queue->head->next;

        Lock_Queue_Node* node = (Lock_Queue_Node*) platform_atomic_load64(&queue->head); assert(node);
        Lock_Queue_Node* next = (Lock_Queue_Node*) platform_atomic_load64(&node->next);
        if(next)
        {
            if(platform_atomic_compare_and_swap64(&queue->head, (uint64_t) node, (uint64_t) next))
            {
                memcpy(out_block, new_head + 1, queue->item_size);
                platform_atomic_sub64(&queue->len, 1);
                node->next = NULL;
                return;
            }
        }
    }
}

bool lock_queue_pop_atomic(Lock_Queue* queue, void* out_block)
{
    for(;platform_atomic_load64(queue->len) > 0;)
    { 
        //attempt to pop
        Lock_Queue_Node* node = queue->head;
        Lock_Queue_Node* new_head = queue->head->next;

        Lock_Queue_Node* node = (Lock_Queue_Node*) platform_atomic_load64(&queue->head); assert(node);
        Lock_Queue_Node* next = (Lock_Queue_Node*) platform_atomic_load64(&node->next);
        if(next)
        {
            if(platform_atomic_compare_and_swap64(&queue->head, (uint64_t) node, (uint64_t) next))
            {
                memcpy(out_block, new_head + 1, queue->item_size);
                platform_atomic_sub64(&queue->len, 1);
                node->next = NULL;
                return true;
            }
        }
    }
    return false;
}

void lock_queue_pop_wait(Lock_Queue* queue, void* out_block)
{


}
#endif


#if 1

ATTRIBUTE_INLINE_NEVER 
Gen_Node* atomic_alloc_list_force_allocate(volatile Gen_Ptr* free_list, volatile Gen_Ptr* block_list, volatile isize* capacity_or_null, isize at_once, isize block_size)
{
    assert(at_once > 0);
    assert(block_size >= sizeof(Gen_Node));
    if(at_once <= 0)
        at_once = 1;

    Gen_Ptr list = gen_ptr_load(free_list);

    isize total_size = at_once*block_size + sizeof(Gen_Node);
    uint8_t* alloced = (uint8_t*) platform_heap_reallocate(total_size, 0, CACHE_LINE);
    memset(alloced, -1, sizeof alloced);

    #define NODE_AT(i) ((Gen_Node*) (alloced + (i)*block_size))

    Gen_Node* first = NODE_AT(0);
    Gen_Node* second = NODE_AT(1);
    Gen_Node* last = NODE_AT(at_once - 1);
    Gen_Node* block = NODE_AT(at_once);

    //Link all nodes
    for(int i = 0; i < at_once; i++)
    {
        NODE_AT(i)->next.ptr = NODE_AT(i+1);
        NODE_AT(i)->next.gen = list.gen;
    }

    #undef NODE_AT
    first->next.ptr = NULL;
    last->next.ptr = NULL;

    //Add this allocated block to alloc block list
    atomic_stack_push(free_list, second, last);

    //return the first allocated node and push the rest into the free list
    if(first != last)
        atomic_stack_push_chain(free_list, second, last);
    if(capacity_or_null)
        platform_atomic_add64(capacity_or_null, at_once);

    platform_memory_barrier();

    return first;
}

ATTRIBUTE_INLINE_ALWAYS 
Gen_Node* atomic_alloc_list_get(volatile Gen_Ptr* free_list, volatile Gen_Ptr* block_list, volatile isize* capacity_or_null, isize at_once, isize block_size)
{
    Gen_Node* node = atomic_stack_pop(free_list);
    if(node == NULL) //unlikely branch code hidden under function call
        node = atomic_alloc_list_force_allocate(free_list, block_list, capacity_or_null, at_once, block_size);

    return node;
}

ATTRIBUTE_INLINE_ALWAYS 
Gen_Node* atomic_alloc_list_free_all(volatile Gen_Ptr* free_list, volatile Gen_Ptr* block_list, isize at_once, isize block_size)
{
    //this function is not atomic! Only call this at cleanup!
    Gen_Node* nodes = (Gen_Node*) atomic_stack_pop_all(free_list); (void) nodes;
    Gen_Node* blocks = (Gen_Node*) atomic_stack_pop_all(block_list);

    for(Gen_Node* curr_block = blocks; curr_block;)
    {
        Gen_Node* next_block = curr_block->next.ptr;

        void* last_node_in_block_end = curr_block - 1;
        Gen_Node* first_node_in_block = (uint8_t*) last_node_in_block_end - at_once*block_size;
        free(first_node_in_block);

        curr_block = next_block;
    }
}

typedef struct Atomic_Link_Queue {
    ATTRIBUTE_ALIGNED(64) volatile Gen_Ptr head; //Owned by consumer threads
    uint8_t head_pad[6];

    ATTRIBUTE_ALIGNED(64) volatile Gen_Ptr tail; //Owned by producer threads
    uint8_t tail_pad[6];

    //shared by both consumers and producer threads 
    ATTRIBUTE_ALIGNED(64) 
    volatile Gen_Ptr free_list;
    volatile Gen_Ptr block_list;
    volatile isize capacity;
    uint8_t shared_pad[3];
} Atomic_Link_Queue;

typedef struct Atomic_Queue_Config{
    int32_t node_size;
    int32_t alloc_at_once;
    int32_t item_size;
} Atomic_Queue_Config;

ATTRIBUTE_INLINE_ALWAYS 
void atomic_linked_queue_init(Atomic_Link_Queue* queue, Atomic_Queue_Config config)
{
    Gen_Node* node = atomic_alloc_list_get(&queue->free_list, &queue->block_list, &queue->capacity, config.alloc_at_once, config.node_size);
    Gen_Ptr sentinel = {node, 0};

    gen_ptr_store(&queue->head, sentinel);
    gen_ptr_store(&queue->tail, sentinel);

    platform_memory_barrier();
}

ATTRIBUTE_INLINE_ALWAYS 
void atomic_linked_queue_deinit(Atomic_Link_Queue* queue, Atomic_Queue_Config config)
{
    atomic_alloc_list_free_all(&queue->free_list, &queue->block_list, config.alloc_at_once, config.node_size);
    memset(queue, 0, sizeof *queue);

    platform_memory_barrier();
}

ATTRIBUTE_INLINE_ALWAYS 
void atomic_linked_queue_push(Atomic_Link_Queue* queue, const void* memory, Atomic_Queue_Config config)
{
    Gen_Node* node = atomic_alloc_list_get(&queue->free_list, &queue->block_list, &queue->capacity, config.alloc_at_once, config.item_size);
    memcpy(node + 1, memory, config.item_size);

    Gen_Ptr tail = {0};
    Gen_Ptr next = {0};
    for(;;)
    {
        tail = gen_ptr_load(&queue->tail);
        next = gen_ptr_load(&tail.node->next);

        if(gen_ptr_is_equal(tail, gen_ptr_load(&queue->tail)))
        {
            if(next.ptr == NULL)
            {
                Gen_Ptr new_next = {node, next.gen + 1};
                if(gen_ptr_compare_and_swap(&tail.node->next, next, new_next))
                    break;
            }
            else
            {
                Gen_Ptr new_tail = {next.ptr, tail.gen + 1};
                gen_ptr_compare_and_swap(&queue->tail, tail, new_tail);
            }
        }
    }
    
    Gen_Ptr new_tail = {node, tail.gen + 1};
    gen_ptr_compare_and_swap(&queue->tail, tail, new_tail);
}

ATTRIBUTE_INLINE_ALWAYS 
bool atomic_linked_queue_pop(Atomic_Link_Queue* queue, void* memory, Atomic_Queue_Config config)
{
    Gen_Ptr head = {0};
    Gen_Ptr tail = {0};
    Gen_Ptr next = {0};
    for(;;)
    {
        head = gen_ptr_load(&queue->head);
        tail = gen_ptr_load(&queue->tail);
        next = gen_ptr_load(&head.node->next);

        if(gen_ptr_is_equal(head, gen_ptr_load(&queue->head)))
        {
            if(head.ptr == tail.ptr)
            {   
                if(next.ptr == NULL)
                    return false;

                Gen_Ptr new_tail = {next.ptr, tail.gen + 1};
                gen_ptr_compare_and_swap(&queue->tail, tail, new_tail);
            }
            else
            {
                memcpy(memory, next.node + 1, config.item_size);
                Gen_Ptr new_head = {next.ptr, head.gen + 1};
                if(gen_ptr_compare_and_swap(&queue->head, head, new_head))
                    break;
            }
        }
    }
    
    atomic_stack_push(&queue->free_list, head.ptr);
    return true;
}

#define EBR_LOCK ((uint64_t) 1 << 63)

typedef struct EBR_Stack_Local {
    struct EBR_Stack_Local* next;
    volatile uint64_t time_and_is_locked;
} EBR_Stack_Local;

typedef struct EBR_Stack_Shared {
    ATTRIBUTE_ALIGNED(64) volatile void* head;

    volatile EBR_Stack_Local* thread_state_list;
    volatile isize thread_state_count;
} EBR_Stack_Shared;

typedef struct EBR_Stack {
    EBR_Stack_Shared* shared;
    EBR_Stack_Local* local;
} EBR_Stack;

void ebr_critical_section_enter(EBR_Stack ebr)
{
    platform_atomic_add64(&ebr.local->time_and_is_locked, EBR_LOCK);
}

void ebr_critical_section_leave(EBR_Stack ebr)
{
    platform_atomic_add64(&ebr.local->time_and_is_locked, EBR_LOCK + 1);
}

void ebr_critical_section_wait_for_exclusivity(EBR_Stack ebr)
{
    enum {MAX_THREADS = 256};
    assert(ebr.shared->thread_state_count <= MAX_THREADS);

    uint64_t  prev_times[MAX_THREADS]; (void) prev_times;
    uint64_t* live_times[MAX_THREADS]; (void) live_times;

    isize in_crit_section = 0; 
    for(EBR_Stack_Local* curr = (EBR_Stack_Local*) platform_atomic_load64(ebr.shared->thread_state_list); curr; curr = curr->next)
    {
        if(curr != ebr.local)
        {
            uint64_t state = platform_atomic_load64(&curr->time_and_is_locked);
            if(state & EBR_LOCK)
            {
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

void ebr_stack_push(EBR_Stack ebr, void* node)
{
    //node is exclusively owned so we dont need to go into critical section!
    atomic_list_push(&ebr.shared->head, node);
}

void* ebr_stack_pop(EBR_Stack ebr)
{
    EBR_Stack_Shared* shared = ebr.shared;
    EBR_Stack_Local* local = ebr.local;

    ebr_critical_section_enter(ebr);

    for(;;) {
        uint64_t head = platform_atomic_load64(&shared->head);
        if(head == 0)
            break;

        uint64_t next = platform_atomic_load64((void*) head);
        if(platform_atomic_compare_and_swap64(&shared->head, head, next))
        {
            ebr_critical_section_leave(ebr);
            ebr_critical_section_wait_for_exclusivity(ebr);
            return (void*) head;
        }
    }
    
    ebr_critical_section_leave(ebr);
    return NULL;
}

typedef struct JSQ_Node {
    struct JSQ_Node* next;
    uint8_t data[56];
} JSQ_Node;

typedef struct Stealing_Job_Queue_Local {
    ATTRIBUTE_ALIGNED(64) //shared with other threads
    volatile void* returned_list;
    volatile JSQ_Node* head;

    ATTRIBUTE_ALIGNED(64) //exclusive to this thread
    void* private_list;
    JSQ_Node* tail;
} Stealing_Job_Queue_Local;

void* jsq_local_alloc(Stealing_Job_Queue_Local* local)
{
    if(!local->private_list)
        local->private_list = atomic_list_pop_all(&local->returned_list);

    if(local->private_list)
    {
        void* out = local->private_list;
        local->private_list = *(void**) local->private_list;
        return out;
    }
    else
    {
        //malloc!
        return NULL;
    }
}

void jsq_local_push(Stealing_Job_Queue_Local* local, void* data)
{
    JSQ_Node* node = (JSQ_Node*) jsq_local_alloc(local);
    JSQ_Node* prev_tail = local->tail;
    memcpy(prev_tail->data, data, sizeof prev_tail->data);

    local->tail = node;
    platform_atomic_store64(&prev_tail->next, (uint64_t) node);
}

void* jsq_local_free(Stealing_Job_Queue_Local* local, JSQ_Node* node)
{
    node->next = (JSQ_Node*) local->private_list;
    local->private_list = node;
}

void* jsq_extern_free(Stealing_Job_Queue_Local* local, JSQ_Node* node)
{
    atomic_list_push(&local->returned_list, node);
}

JSQ_Node* jsq_pop(Stealing_Job_Queue_Local* local, void* data)
{
    for(;;)
    {
        JSQ_Node* head = (JSQ_Node*) platform_atomic_load64(&local->head);
        JSQ_Node* next = (JSQ_Node*) platform_atomic_load64(&head->next);
        if(next == NULL)
            return NULL;

        if(platform_atomic_compare_and_swap64(&local->head, head, next))
        {
            
        }
    }
}

typedef struct Atomic_Steal_Queue {
    ATTRIBUTE_ALIGNED(64) volatile Gen_Ptr producer_list; //Owned by consumer threads
    uint8_t head_pad[6];

    ATTRIBUTE_ALIGNED(64) volatile Gen_Ptr consumer_list; //Owned by producer threads
    Atomic_Cell transfering_cell;
    uint8_t tail_pad[5];

    //shared by both consumers and producer threads 
    ATTRIBUTE_ALIGNED(64) 
    volatile Gen_Ptr free_list;
    volatile Gen_Ptr block_list;
    volatile isize capacity;
    uint8_t shared_pad[3];
} Atomic_Steal_Queue;

ATTRIBUTE_INLINE_ALWAYS 
void atomic_linked_queue_push(Atomic_Steal_Queue* queue, void* node)
{
    atomic_stack_push(&queue->producer_list, node);
}

ATTRIBUTE_INLINE_ALWAYS 
void* atomic_linked_queue_pop(Atomic_Steal_Queue* queue, void* node)
{
    enum {IDLE = 0, TRANSFERING = 1};

    void* node = atomic_stack_pop(&queue->consumer_list, node);
    if(node == NULL)
    {
        for(;;)
        {
            uint32_t cell = platform_atomic_load32(&queue->transfering_cell);
            if(cell == IDLE)
                if(platform_atomic_compare_and_swap32(&queue->transfering_cell, IDLE, TRANSFERING))
                {
                    Gen_Node* all_nodes = (Gen_Node*) atomic_stack_pop_all(&queue->producer_list);
                    node = all_nodes;
                    if(all_nodes)
                    {
                        Gen_Node* 
                    }

                    platform_atomic_store32(&queue->transfering_cell, IDLE);
                }

            platform_processor_pause();
        }

        if(node == NULL)
    }

    return node;
}


typedef struct Atomic_Job {
    void (*func)(struct Atomic_Job*);
    
    union {
        uint8_t data[40];
        void* ptr;
    };
} Atomic_Job;

#define ATOMIC_WORK_QUEUE_CONFIG BINIT(Atomic_Queue_Config){64, 8, sizeof(Atomic_Job)}

typedef struct Work_Queue {
    Atomic_Link_Queue queue;
    volatile isize len;
} Work_Queue;

void work_queue_push(Work_Queue* queue, Atomic_Job job)
{
    atomic_linked_queue_push(&queue->queue, &job, ATOMIC_WORK_QUEUE_CONFIG);
    platform_atomic_add64(&queue->len, 1);
    platform_futex_wake(&queue->len);
}   

bool work_queue_pop(Work_Queue* queue, Atomic_Job* job)
{
    bool out = atomic_linked_queue_pop(&queue->queue, job, ATOMIC_WORK_QUEUE_CONFIG);
    if(out)
        platform_atomic_sub64(&queue->len, 1);
    return out;
}   

void work_queue_pop_wait(Work_Queue* queue, Atomic_Job* job)
{
    for(;;)
    {
        isize len = platform_atomic_load64(&queue->len);
        if(len == 0)
            platform_futex_wait(&queue->len, 0, -1);
        else if(atomic_linked_queue_pop(&queue->queue, job, ATOMIC_WORK_QUEUE_CONFIG))
        {
            platform_atomic_sub64(&queue->len, 1);
            break;
        }
    }
}   

enum {
    ATOMIC_RUNNING = 0,
    ATOMIC_CANCELED = 1,
};

ATTRIBUTE_INLINE_ALWAYS 
static uint32_t atomic_wait_for_cancel(volatile uint32_t* cancel)
{
    for(;;) {
        uint32_t state = platform_atomic_load32(cancel);
        if(state != ATOMIC_RUNNING)
            return state;

        platform_futex_wait(state, ATOMIC_RUNNING, -1);
    }
}

ATTRIBUTE_INLINE_ALWAYS 
static bool atomic_is_canceled(volatile uint32_t* cancel)
{
    return platform_atomic_load32(cancel) != ATOMIC_RUNNING;
}

ATTRIBUTE_INLINE_ALWAYS 
static void atomic_cancel(volatile uint32_t* cancel)
{
    platform_atomic_store32(cancel, ATOMIC_CANCELED);
    platform_futex_wake_all(cancel);
}

typedef struct Transfer_Block {
    union {
        struct Transfer_Block* next;
        Gen_Ptr _;
    };
    isize size : 63;
    bool malloced : 1;

    union {
        uint8_t* malloced_data;
        uint8_t  inline_data[8];
    };
} Transfer_Block;

enum {
    TRANSFER_LIST_NO_MALLOC_BLOCKS = 1,
    TRANSFER_LIST_NO_PAD_TO_CACHE_LINE = 2,
};

typedef struct Transfer_List {
    volatile Gen_Ptr head;
    volatile Gen_Ptr free_list;
    volatile Gen_Ptr block_list;
    volatile isize capacity;
    
    volatile int32_t references;
    int32_t flags;
    isize item_size;
    isize block_size;

} Transfer_List;

void trasnfer_list_deinit(Transfer_List* list)
{
    int32_t refs = (int32_t) platform_atomic_sub32(&list->references, 1);
    assert(refs >= 1);
    if(refs == 1)
    {
        atomic_alloc_list_free_all(&list->free_list, &list->block_list, 8, list->block_size);
        memset(list, 0, sizeof *list);
    }
}

Transfer_List* trasnfer_list_share(Transfer_List* list)
{
    assert(list->references >= 1);
    platform_atomic_add32(&list->references, 1);
    return list;
}

void trasnfer_list_init(Transfer_List* list, isize item_size, int32_t flags)
{
    memset(list, 0, sizeof *list);
    isize item_size_capped = item_size > 8 ? item_size : 8;
    isize block_size = sizeof(Transfer_Block) - 8 + item_size_capped;
    isize block_size_rounded = (block_size + CACHE_LINE - 1)/CACHE_LINE*CACHE_LINE;

    list->block_size = flags & TRANSFER_LIST_NO_PAD_TO_CACHE_LINE ? block_size : block_size_rounded;
    list->item_size = item_size;
    list->flags = flags;
    list->references = 1;

    platform_memory_barrier();
}

void* transfer_stack_data(Transfer_Block* block)
{
    if(block->malloced)
        return block->malloced_data;
    else
        return block->inline_data;
}

void transfer_stack_push_parts(Transfer_List* list, const void** ptrs, const int64_t* sizes, int64_t count)
{
    assert(list->references >= 1);

    int64_t total_size = 0;
    for(int64_t i = 0; i < count; i++)
        total_size += sizes[i];

    Transfer_Block* block = (Transfer_Block*) atomic_alloc_list_get(&list->free_list, &list->block_list, &list->capacity, 8, list->block_size);
    block->size = total_size;
    block->malloced = total_size > list->item_size;

    uint8_t* data = block->inline_data;
    if(block->malloced)
    {
        assert((list->flags & TRANSFER_LIST_NO_MALLOC_BLOCKS));  
        block->malloced_data = (uint8_t*) malloc(total_size);
        data = block->malloced_data;
    }

    assert(data);
    int64_t curr_pos = 0;
    for(int64_t i = 0; i < count; i++)
    {
        if(ptrs[i])
            memcpy(data + curr_pos, ptrs[i], sizes[i]);
        curr_pos += sizes[i];
    }

    atomic_stack_push(&list->head, block);
}

void transfer_stack_push(Transfer_List* list, const void* ptr, int64_t size)
{
    transfer_stack_push_parts(list, &ptr, &size, 1);
}

Transfer_Block* transfer_stack_pop(Transfer_List* list)
{
    Transfer_Block* out = (Transfer_Block*) atomic_stack_pop(&list->head);
    out->next = NULL;
    return out;
}

Transfer_Block* transfer_stack_pop_all(Transfer_List* list)
{
    return (Transfer_Block*) atomic_stack_pop_all(&list->head);
}

void transfer_stack_recycle(Transfer_List* list, Transfer_Block* first, Transfer_Block* last_or_null)
{
    Transfer_Block* last = last_or_null;
    for(Transfer_Block* curr = first; curr; curr = curr->next)
    {
        if(curr->malloced)
            free(transfer_stack_data(curr));

        last = curr;
        if(curr == last_or_null)
            break;
    }

    atomic_stack_push_chain(&list->free_list, first, last);
}


#endif

// TICKET LOCK

ATTRIBUTE_INLINE_ALWAYS 
static uint32_t ticket_lock_acquire(volatile uint32_t* next_ticket, volatile uint32_t* now_serving, Atomic_Wait wait)
{
    uint32_t my_ticket = platform_atomic_add32(next_ticket, 1);
    uint32_t curr_value = platform_atomic_load32(now_serving);
    if(my_ticket != curr_value)
        atomic_wait_for(now_serving, my_ticket, curr_value, wait);

    return my_ticket;
}

ATTRIBUTE_INLINE_ALWAYS 
static uint32_t ticket_lock_release(volatile uint32_t* now_serving, Atomic_Wait wait)
{
    uint32_t serving = platform_atomic_add32(now_serving, 1);
    if(wait == ATOMIC_WAIT_BLOCK)
        platform_futex_wake(now_serving);

    return serving;
}
