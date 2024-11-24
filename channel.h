#pragma once

#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>

#ifndef ASSERT
    #include <assert.h>
    #define ASSERT(x, ...) assert(x)
#endif

#if defined(_MSC_VER)
    #define _CHAN_INLINE_ALWAYS   __forceinline
    #define _CHAN_INLINE_NEVER    __declspec(noinline)
    #define _CHAN_ALIGNED(bytes)  __declspec(align(bytes))
#elif defined(__GNUC__) || defined(__clang__)
    #define _CHAN_INLINE_ALWAYS   __attribute__((always_inline)) inline
    #define _CHAN_INLINE_NEVER    __attribute__((noinline))
    #define _CHAN_ALIGNED(bytes)  __attribute__((aligned(bytes)))
#else
    #define _CHAN_INLINE_ALWAYS   inline
    #define _CHAN_INLINE_NEVER
    #define _CHAN_ALIGNED(bytes)  _Align(bytes)
#endif

#ifndef CHAN_CUSTOM
    #define CHANAPI        _CHAN_INLINE_ALWAYS static
    #define CHAN_INTRINSIC _CHAN_INLINE_ALWAYS static
    #define CHAN_OS_API
    #define CHAN_CACHE_LINE 64
#endif

//==========================================================================
// Channel (high throuput concurrent queue)
//==========================================================================
// An linearizable blocking concurrent queue based on the design described in 
// "T. R. W. Scogland - Design and Evaluation of Scalable Concurrent Queues for Many-Core Architectures, 2015" 
// which can be found at https://synergy.cs.vt.edu/pubs/papers/scogland-queues-icpe15.pdf.
// 
// We differ from the implementation in the paper in that we support proper thread blocking via futexes and
// employ more useful semantics around closing.
// 
// The channel acts pretty much as a Go buffered channel augmented with additional
// non-blocking and ticket interfaces. These allow us to for example only push if the
// channel is not full or wait for item to be processed.
//
// The basic idea is to do a very fine grained locking: each item in the channel has a dedicated
// ticket lock. On push/pop we perform atomic fetch and add (FAA) one to the tail/head indices, which yields
// a number used to calculate our slot and operation id. This slot is potentially shared with other 
// pushes or pops because the queue has finite capacity. We go to that slot and wait on its ticket lock 
// to signal id corresponding to this push/pop operation. Only then we push/pop the item then advance 
// the ticket lock, allowing a next operation on that slot to proceed.
// 
// This procedure means that unless the queue is full/empty a single push/pop contains only 
// one atomic FAA on the critical path (ticket locks are uncontested), resulting in extremely
// high throughput pretty much only limited by the FAA contention.

typedef int64_t isize;

typedef bool (*Sync_Wait_Func)(volatile void* state, uint32_t undesired, isize timeout_or_minus_one_if_infinite);
typedef void (*Sync_Wake_Func)(volatile void* state);

typedef struct Channel_Info {
    isize item_size;
    Sync_Wait_Func wait;
    Sync_Wake_Func wake;
} Channel_Info;

typedef struct Channel {
    _CHAN_ALIGNED(CHAN_CACHE_LINE) 
    volatile uint64_t head;
    volatile uint64_t head_barrier;
    volatile uint64_t head_cancel_count;
    uint64_t _head_pad[5];

    _CHAN_ALIGNED(CHAN_CACHE_LINE) 
    volatile uint64_t tail;
    volatile uint64_t tail_barrier;
    volatile uint64_t tail_cancel_count;
    uint64_t _tail_pad[5];

    _CHAN_ALIGNED(CHAN_CACHE_LINE) 
    Channel_Info info;
    isize capacity; 
    uint8_t* items; 
    uint32_t* ids; 
    volatile uint32_t ref_count; 
    volatile uint32_t allocated;
    volatile uint32_t closing_state;
    volatile uint32_t closing_lock_requested;
    volatile uint32_t closing_lock_completed;
    uint8_t _[60];
} Channel;

typedef enum Channel_Res {
    CHANNEL_OK = 0,
    CHANNEL_CLOSED = 1,
    CHANNEL_LOST_RACE = 2,
    CHANNEL_FULL = 3,
    CHANNEL_EMPTY = 4,
} Channel_Res;


enum {
    CHANNEL_CLOSING_PUSH = 1, 
    CHANNEL_CLOSING_POP = 2, 
    CHANNEL_CLOSING_CLOSED = 4, 
    CHANNEL_CLOSING_HARD = 8,
};


//Allocates a new channel on the heap and returns pointer to it. If the allocation fails returns 0.
//capacity >= 0. If capacity == 0 then creates an "unbuffered" channel which acts as unbuffered channel in Go.
// That is there is just one slot in the channel and after a call to channel_push the calling thread is
// waiting until channel_pop is called (or until the channel is closed).
CHANAPI Channel* channel_malloc(isize capacity, Channel_Info info);

//Increments the ref count of the channel. Returns the passed in channel.
CHANAPI Channel* channel_share(Channel* chan);

//Decrements the ref count and if it reaches zero deinitializes the channel. 
//If the channel was allocated through channel_malloc frees it.
//If it was created through channel_init only memsets the channel to zero.
CHANAPI int32_t channel_deinit(Channel* chan);

CHANAPI void     channel_init(Channel* chan, void* items, uint32_t* ids, isize capacity, Channel_Info info);
CHANAPI isize    channel_memory_size(isize capacity, Channel_Info info); //Obtains the combined needed size for the Channel struct and capacity items
CHANAPI Channel* channel_init_into_memory(void* aligned_memory, isize capacity, Channel_Info info); //Places and initializes the Channel struct into the given memory

//Pushes an item, waiting if channel is full. If the channel (side) is closed returns false instead of waiting else returns true.
CHANAPI bool channel_push(Channel* chan, const void* item, Channel_Info info);
//Pops an item, waiting if channel is empty. If the channel (side) is closed returns false instead of waiting else returns true.
CHANAPI bool channel_pop(Channel* chan, void* item, Channel_Info info);

//Attempts to push an item stored in item without blocking returning CHANNEL_OK on success.
// If the channel (side) is closed returns CHANNEL_CLOSED
// If the channel is full returns CHANNEL_FULL
// If lost a race to concurrent call to this function returns CHANNEL_LOST_RACE.
CHANAPI Channel_Res channel_try_push_weak(Channel* chan, const void* item, Channel_Info info);
//Attempts to pop an item storing it in item without blocking returning CHANNEL_OK on success.
// If the channel (side) is closed returns CHANNEL_CLOSED
// If the channel is empty returns CHANNEL_EMPTY
// If lost a race to concurrent call to this function returns CHANNEL_LOST_RACE.
CHANAPI Channel_Res channel_try_pop_weak(Channel* chan, void* item, Channel_Info info); 

//Same as channel_try_push/pop_weak but never returns CHANNEL_LOST_RACE.
//Instead retries until the operation completes successfully or some other error appears.
CHANAPI Channel_Res channel_try_push(Channel* chan, const void* item, Channel_Info info);
CHANAPI Channel_Res channel_try_pop(Channel* chan, void* item, Channel_Info info);

CHANAPI bool channel_close_push(Channel* chan, Channel_Info info);
CHANAPI bool channel_close_soft(Channel* chan, Channel_Info info);
CHANAPI bool channel_close_hard(Channel* chan, Channel_Info info);

CHANAPI bool channel_reopen(Channel* chan, Channel_Info info);
CHANAPI bool channel_is_closed(const Channel* chan); 

CHANAPI bool channel_hard_reopen(Channel* chan, Channel_Info info);
CHANAPI bool channel_is_hard_closed(const Channel* chan); 

//Returns upper bound to the distance between head and tail indices. 
//This can be used to approximately check the number of blocked threads or get the number of items in the channel.
CHANAPI isize channel_signed_distance(const Channel* chan);

//Returns upper bound to the number of items in the channel. Returned value is in range [0, chan->capacity]
CHANAPI isize channel_count(const Channel* chan);
CHANAPI bool channel_is_empty(const Channel* chan);

//==========================================================================
// Channel ticket interface 
//==========================================================================
// These functions work just like their regular counterparts but can also return the ticket of the completed operation.
// The ticket can be used to signal completion using the channel_ticket_is_less function. 
//
// For example when producer pushes into a channel and wants to wait for the consumer to process the pushed item, 
// it uses these functions to also obtain a ticket. The consumer also pops and takes and receives a ticket. 
// After each processed item it sets its ticket to global variable. The producer thus simply waits for the 
// the global variable to becomes not less then the received ticket using channel_ticket_is_less.


#define CHANNEL_MAX_TICKET (UINT64_MAX/4)

//Returns whether ticket_a came before ticket_b. 
//Unless unsigned number overflow happens this is just `ticket_a < ticket_b`.
CHANAPI bool channel_ticket_is_less(uint64_t ticket_a, uint64_t ticket_b);

//Returns whether ticket_a came before or is equal to ticket_b. 
//Unless unsigned number overflow happens this is just `ticket_a <= ticket_b`.
CHANAPI bool channel_ticket_is_less_or_eq(uint64_t ticket_a, uint64_t ticket_b);

CHANAPI bool channel_ticket_push(Channel* chan, const void* item, uint64_t* ticket_or_null, Channel_Info info);
CHANAPI bool channel_ticket_pop(Channel* chan, void* item, uint64_t* ticket_or_null, Channel_Info info);
CHANAPI Channel_Res channel_ticket_try_push(Channel* chan, const void* item, uint64_t* ticket_or_null, Channel_Info info);
CHANAPI Channel_Res channel_ticket_try_pop(Channel* chan, void* item, uint64_t* ticket_or_null, Channel_Info info);
CHANAPI Channel_Res channel_ticket_try_push_weak(Channel* chan, const void* item, uint64_t* ticket_or_null, Channel_Info info);
CHANAPI Channel_Res channel_ticket_try_pop_weak(Channel* chan, void* item, uint64_t* ticket_or_null, Channel_Info info);

//==========================================================================
// Wait free list 
//==========================================================================
// A simple growing stack where no ABA problem can occur. 
// Before call to sync_list_push() the "pusher" thread has exclusive ownership over the node. 
// After push it must no longer touch it.
// Once a node is popped using sync_list_pop_all() the "popper" thread has exclusive ownership of that node
//  and can even dealloc the node without any risk of use after free.
#define sync_list_push(head_ptr_ptr, node_ptr) sync_list_push_chain(head_ptr_ptr, node_ptr, node_ptr)
#define sync_list_pop_all(head_ptr_ptr) (void*) chan_atomic_exchange64((head_ptr_ptr), 0)
#define sync_list_push_chain(head_ptr_ptr, first_node_ptr, last_node_ptr) \
    for(;;) { \
        uint64_t curr = chan_atomic_load64((head_ptr_ptr)); \
        chan_atomic_store64(&(last_node_ptr)->next, curr); \
        if(chan_atomic_cas64((head_ptr_ptr), curr, (uint64_t) (first_node_ptr))) \
            break; \
    } \


//==========================================================================
// Wait/Wake helpers
//==========================================================================
// Provide helpers for waiting for certain condition. 
// This can be used to implement wait groups, semaphores and much more.
// Alternatively there is also timed version which gives up after certain 
// amount of time and returns failure (false).
typedef struct Sync_Wait {
    Sync_Wait_Func wait;
    Sync_Wake_Func wake;
    uint32_t notify_bit;
    uint32_t _;
} Sync_Wait;

CHANAPI bool sync_wait(volatile void* state, uint32_t current, isize timeout, Sync_Wait wait);
CHANAPI void sync_wake(volatile void* state, uint32_t prev, Sync_Wait wait);
CHANAPI void sync_set_and_wake(volatile void* state, uint32_t to, Sync_Wait wait);
CHANAPI uint32_t sync_wait_for_equal(volatile void* state, uint32_t desired, Sync_Wait wait);
CHANAPI uint32_t sync_wait_for_not_equal(volatile void* state, uint32_t desired, Sync_Wait wait);
CHANAPI uint32_t sync_wait_for_smaller(volatile void* state, uint32_t desired, Sync_Wait wait);
CHANAPI uint32_t sync_wait_for_greater(volatile void* state, uint32_t desired, Sync_Wait wait);

typedef struct Sync_Timed_Wait {
    double freq_s;
    isize freq_ms;
    isize wait_ticks;
    isize start_ticks;
} Sync_Timed_Wait;

CHANAPI Sync_Timed_Wait sync_timed_wait_start(double wait);
CHANAPI bool sync_timed_wait(volatile void* state, uint32_t current, Sync_Timed_Wait timeout, Sync_Wait wait);
CHANAPI bool sync_timed_wait_for_equal(volatile void* state, uint32_t desired, double timeout, Sync_Wait wait);
CHANAPI bool sync_timed_wait_for_not_equal(volatile void* state, uint32_t desired, double timeout, Sync_Wait wait);
CHANAPI bool sync_timed_wait_for_smaller(volatile void* state, uint32_t desired, double timeout, Sync_Wait wait);
CHANAPI bool sync_timed_wait_for_greater(volatile void* state, uint32_t desired, double timeout, Sync_Wait wait);

//==========================================================================
// Wait Group
//==========================================================================
// A simple counter that allows incrementing and decrementing (push/pop) and waiting
// for it to hit zero. Is based on Go's wait groups and can be used in exactly the same way.
// This behaviour could be also achieved by using sync_wait_for_equal() and similar at a cost 
// of potentially more wakeups. This implementation guarantees that the waiting thread will 
// get woken up exactly once. 
// The wake field is incremented every time the count crosses to or below zero, preventing
// the ABA problem.
typedef union Wait_Group {
    uint64_t combined;
    struct {
        volatile int32_t count;
        volatile uint32_t wakes;
    };
} Wait_Group; 

CHANAPI int32_t wait_group_count(volatile Wait_Group* wg);
CHANAPI Wait_Group* wait_group_push(volatile Wait_Group* wg, isize count);
CHANAPI bool wait_group_pop(volatile Wait_Group* wg, isize count, Sync_Wait wait);
CHANAPI void wait_group_wait(volatile Wait_Group* wg, Sync_Wait wait);
CHANAPI bool wait_group_wait_timed(volatile Wait_Group* wg, double timeout, Sync_Wait wait);

//==========================================================================
// Once
//==========================================================================
//Calls the provided function exactly once. Can also be used like
// static Sync_Once once = 0;
// if(sync_once_begin(&once)) {
//   //init code here...
//   sync_once_end(&once);
// }
typedef uint32_t Sync_Once;
enum {
    SYNC_ONCE_UNINIT = 0,
    SYNC_ONCE_INIT = 1,
    SYNC_ONCE_INITIALIZING = 2,
};

CHANAPI bool sync_once(volatile Sync_Once* once, void(*func)(void* context), void* context, Sync_Wait wait);
CHANAPI bool sync_once_begin(volatile Sync_Once* once, Sync_Wait wait);
CHANAPI void sync_once_end(volatile Sync_Once* once, Sync_Wait wait);

//==========================================================================
// Wait/Wake functions
//==========================================================================
//These functions can be used for Sync_Wait_Func/Sync_Wake_Func interafces in the channel
// and sync_wait interfaces.

CHAN_INTRINSIC void chan_pause();
CHAN_INTRINSIC void chan_wake_block(volatile void* state);
CHAN_OS_API bool     chan_wait_block(volatile void* state, uint32_t undesired, isize timeout_or_minus_one_if_infinite);
CHAN_OS_API bool     chan_wait_yield(volatile void* state, uint32_t undesired, isize timeout_or_minus_one_if_infinite);

#ifdef __cplusplus
    #define _CHAN_SINIT(T) T
#else
    #define _CHAN_SINIT(T) (T)
#endif
#define SYNC_WAIT_BLOCK _CHAN_SINIT(Sync_Wait){chan_wait_block, chan_wake_block}
#define SYNC_WAIT_YIELD _CHAN_SINIT(Sync_Wait){chan_wait_yield}
#define SYNC_WAIT_SPIN  _CHAN_SINIT(Sync_Wait){}
#define SYNC_WAIT_BLOCK_BIT(bit) _CHAN_SINIT(Sync_Wait){chan_wait_block, chan_wake_block, 1u << bit}

//Atomics
CHAN_INTRINSIC uint64_t chan_atomic_load64(const volatile void* target);
CHAN_INTRINSIC uint32_t chan_atomic_load32(const volatile void* target);
CHAN_INTRINSIC void     chan_atomic_store64(volatile void* target, uint64_t value);
CHAN_INTRINSIC void     chan_atomic_store32(volatile void* target, uint32_t value);
CHAN_INTRINSIC bool     chan_atomic_cas64(volatile void* target, uint64_t old_value, uint64_t new_value);
CHAN_INTRINSIC bool     chan_atomic_cas32(volatile void* target, uint32_t old_value, uint32_t new_value);
CHAN_INTRINSIC uint64_t chan_atomic_exchange64(volatile void* target, uint64_t value);
CHAN_INTRINSIC uint32_t chan_atomic_exchange32(volatile void* target, uint32_t value);
CHAN_INTRINSIC uint64_t chan_atomic_add64(volatile void* target, uint64_t value);
CHAN_INTRINSIC uint32_t chan_atomic_add32(volatile void* target, uint32_t value);
CHAN_INTRINSIC uint64_t chan_atomic_sub64(volatile void* target, uint64_t value);
CHAN_INTRINSIC uint32_t chan_atomic_sub32(volatile void* target, uint32_t value);
CHAN_INTRINSIC uint64_t chan_atomic_and64(volatile void* target, uint64_t value);
CHAN_INTRINSIC uint32_t chan_atomic_and32(volatile void* target, uint32_t value);
CHAN_INTRINSIC uint64_t chan_atomic_or64(volatile void* target, uint64_t value);
CHAN_INTRINSIC uint32_t chan_atomic_or32(volatile void* target, uint32_t value);
CHAN_INTRINSIC uint32_t chan_atomic_xor32(volatile void* target, uint32_t value);
CHAN_INTRINSIC bool     chan_atomic_cas128(volatile void* target, uint64_t old_value_lo, uint64_t old_value_hi, uint64_t new_value_lo, uint64_t new_value_hi);

//Os specific
CHAN_OS_API int64_t chan_perf_counter();
CHAN_OS_API int64_t chan_perf_frequency();
CHAN_OS_API void* chan_aligned_alloc(isize bytes, isize aligned);
CHAN_OS_API void chan_aligned_free(void* block, isize aligned);
CHAN_OS_API void chan_start_thread(void(*start_address)(void *), void* args);
CHAN_OS_API void chan_sleep(double seconds);
CHAN_OS_API const char* chan_thread_name();
CHAN_OS_API uint32_t chan_thread_id();

//Debug 
typedef struct Sync_Mem_Log {
    const char* n;
    const char* m;
    uint64_t c1;
    uint64_t c2;
} Sync_Mem_Log;

typedef struct Sync_Mem_Logger {
    uint64_t capacity_pow2;
    Sync_Mem_Log* logs;
    volatile uint64_t pos;
} Sync_Mem_Logger;

CHANAPI uint64_t sync_mem_log_func(Sync_Mem_Logger* logger, const char* msg, uint64_t custom1, uint64_t custom2);


//IMPL FROM HERE

CHANAPI uint64_t sync_mem_log_func(Sync_Mem_Logger* logger, const char* msg, uint64_t custom1, uint64_t custom2)
{
    uint64_t curr = chan_atomic_add64(&logger->pos, 1);
    uint64_t index = curr & (logger->capacity_pow2 - 1);

    Sync_Mem_Log log = {chan_thread_name(), msg, custom1, custom2};
    logger->logs[index] = log;

    return curr;
}


#define CHANNEL_DEBUG
#ifdef CHANNEL_DEBUG
    static Sync_Mem_Log _sync_mem_logs[1 << 20] = {0};
    static Sync_Mem_Logger _sync_mem_logger = {1 << 20, _sync_mem_logs};

    //_sync_mem_logs + (_sync_mem_logger.pos - 128) % _sync_mem_logger.capacity_pow2, [128]

    #define sync_mem_log(msg)                  sync_mem_log_func(&_sync_mem_logger, msg, 0, 0)
    #define sync_mem_log_val(msg, val)         sync_mem_log_func(&_sync_mem_logger, msg, val, 0)
    #define sync_mem_log_val2(msg, val1, val2) sync_mem_log_func(&_sync_mem_logger, msg, val1, val2)
    #define chan_wait_n(n)                     _chan_wait_n(n)
    
    CHANAPI void _chan_wait_n(int n)
    {
        static uint32_t dummy = 0;
        for(int i = 0; i < n; i++)
            chan_atomic_add32(&dummy, 1);
    }
#else
    #define sync_mem_log(msg)                  
    #define sync_mem_log_val(msg, val)         
    #define sync_mem_log_val2(msg, val1, val2) 
    #define chan_wait_n(n)
#endif

#define _CHAN_ID_WAITING_BIT        ((uint32_t) 1)
#define _CHAN_ID_CLOSE_NOTIFY_BIT   ((uint32_t) 2)
#define _CHAN_ID_FILLED_BIT         ((uint32_t) 4)

#define _CHAN_TICKET_PUSH_CLOSED_BIT    ((uint64_t) 1)
#define _CHAN_TICKET_POP_CLOSED_BIT     ((uint64_t) 2)
#define _CHAN_TICKET_INCREMENT          ((uint64_t) 4)

CHANAPI uint64_t _channel_get_target(const Channel* chan, uint64_t ticket)
{
    return ticket % (uint64_t) chan->capacity;
}

CHANAPI uint32_t _channel_get_id(const Channel* chan, uint64_t ticket)
{
    return ((uint32_t) (ticket / (uint64_t) chan->capacity)*_CHAN_ID_FILLED_BIT*2);
}

CHANAPI bool _channel_id_equals(uint32_t id1, uint32_t id2)
{
    return ((id1 ^ id2) / _CHAN_ID_FILLED_BIT) == 0;
}

CHANAPI void _channel_advance_id(Channel* chan, uint64_t target, uint32_t id, Channel_Info info)
{
    uint32_t* id_ptr = &chan->ids[target];
    
    uint32_t new_id = (uint32_t) (id + _CHAN_ID_FILLED_BIT);
    if(info.wake)
    {
        uint32_t prev_id = chan_atomic_exchange32(id_ptr, new_id);
        ASSERT(_channel_id_equals(prev_id + _CHAN_ID_FILLED_BIT, new_id));
        if(prev_id & _CHAN_ID_WAITING_BIT)
            info.wake(id_ptr);
    }
    else
        chan_atomic_store32(id_ptr, new_id);
}

_CHAN_INLINE_NEVER
static bool _channel_ticket_push_potentially_cancel(Channel* chan, uint64_t ticket, uint32_t closing)
{
    bool canceled = false;
    if(closing & CHANNEL_CLOSING_HARD)
        canceled = true;
    else
    {
        uint64_t new_tail = chan_atomic_load64(&chan->tail);
        uint64_t new_head = chan_atomic_load64(&chan->head);
        uint64_t barrier = chan_atomic_load64(&chan->tail_barrier);

        if((new_head & _CHAN_TICKET_PUSH_CLOSED_BIT) || (new_tail & _CHAN_TICKET_PUSH_CLOSED_BIT))
            if(channel_ticket_is_less_or_eq(barrier, ticket))
                canceled = true; 
    }

    if(canceled)
    {
        chan_atomic_add64(&chan->tail_cancel_count, _CHAN_TICKET_INCREMENT);
        chan_atomic_sub64(&chan->tail, _CHAN_TICKET_INCREMENT);
        return false;
    }
    else
        return true;
}

//Must load tail aftter loading curr ticket because:
//  we cannot do "load curr, check if matching, else check if past barrier" because that does not respect barriers 
//  we cannot do "load tail and check if past else check mathcing" because the following could happen:
//   t1: push to a full queue
//   t1: push checks for past barrier but there is no barrier
//   t1: push is just before the check for curr 
//   t3: close tail placing barrier to head + capacity (so just before the ticket of the push above)
//   t2: pop first 
//   t1: push succeeds but by now we should have detected closed!
//Thus the only option is to load, load check check in this order
CHANAPI bool channel_ticket_push(Channel* chan, const void* item, uint64_t* out_ticket_or_null, Channel_Info info) 
{
    ASSERT(memcmp(&chan->info, &info, sizeof info) == 0, "info must be matching");
    ASSERT(item || (item == NULL && info.item_size == 0), "item must be provided");
    
    uint64_t tail = chan_atomic_add64(&chan->tail, _CHAN_TICKET_INCREMENT);
    uint64_t ticket = tail / _CHAN_TICKET_INCREMENT;
    uint64_t target = _channel_get_target(chan, ticket);
    uint32_t id = _channel_get_id(chan, ticket);
    sync_mem_log_val("push called", ticket);
    
    for(;;) {
        uint32_t curr = chan_atomic_load32(&chan->ids[target]);
        chan_wait_n(3);
        uint32_t closing = chan_atomic_load32(&chan->closing_state);
        if(closing) {
            if(_channel_ticket_push_potentially_cancel(chan, ticket, closing) == false) {
                sync_mem_log_val("push canceled", ticket);
                return false;
            }
        }

        chan_wait_n(3);
        if(_channel_id_equals(curr, id))
            break;
            
        if(info.wake) {
            chan_atomic_or32(&chan->ids[target], _CHAN_ID_WAITING_BIT);
            curr |= _CHAN_ID_WAITING_BIT;
        }

        sync_mem_log_val("push waiting", ticket);
        if(info.wait)
            info.wait(&chan->ids[target], curr, -1);
        else
            chan_pause();
        sync_mem_log_val("push woken", ticket);
    }
    
    memcpy(chan->items + target*info.item_size, item, info.item_size);
    
    #ifdef CHANNEL_DEBUG
        uint32_t closing = chan_atomic_load32(&chan->closing_state);
        if((closing & ~CHANNEL_CLOSING_HARD))
        {
            uint64_t new_tail = chan_atomic_load64(&chan->tail);
            uint64_t new_head = chan_atomic_load64(&chan->head);
            uint64_t barrier = chan_atomic_load64(&chan->tail_barrier);
            chan_wait_n(1);
            
            const char* name = chan_thread_name(); (void) name;
            if((new_head & _CHAN_TICKET_PUSH_CLOSED_BIT) || (new_tail & _CHAN_TICKET_PUSH_CLOSED_BIT))
                ASSERT(channel_ticket_is_less(ticket, barrier));
        }
    #endif
    _channel_advance_id(chan, target, id, info);

    if(out_ticket_or_null)
        *out_ticket_or_null = ticket;

    sync_mem_log_val("push done", ticket);
    return true;
}

_CHAN_INLINE_NEVER
bool channel_push_int(Channel* chan, const int* item) 
{
    Channel_Info info = {sizeof(int)};
    return channel_ticket_push(chan, item, NULL, info);
}

_CHAN_INLINE_NEVER 
static bool _channel_ticket_pop_potentially_cancel(Channel* chan, uint64_t ticket, uint32_t closing)
{
    bool canceled = false;
    if(closing & CHANNEL_CLOSING_HARD)
        canceled = true;
    else
    {
        uint64_t new_head = chan_atomic_load64(&chan->head);
        uint64_t barrier = chan_atomic_load64(&chan->head_barrier);

        canceled = (new_head & _CHAN_TICKET_POP_CLOSED_BIT) && channel_ticket_is_less_or_eq(barrier, ticket); 
    }

    if(canceled)
    {
        sync_mem_log_val("push canceled", ticket);
        chan_atomic_add64(&chan->head_cancel_count, _CHAN_TICKET_INCREMENT);
        chan_atomic_sub64(&chan->head, _CHAN_TICKET_INCREMENT);
        return false;
    }
    return true;
}

CHANAPI bool channel_ticket_pop(Channel* chan, void* item, uint64_t* out_ticket_or_null, Channel_Info info) 
{
    ASSERT(memcmp(&chan->info, &info, sizeof info) == 0, "info must be matching");
    ASSERT(item || (item == NULL && info.item_size == 0), "item must be provided");

    uint64_t head = chan_atomic_add64(&chan->head, _CHAN_TICKET_INCREMENT);
    uint64_t ticket = head / _CHAN_TICKET_INCREMENT;
    uint64_t target = _channel_get_target(chan, ticket);
    uint32_t id = _channel_get_id(chan, ticket) + _CHAN_ID_FILLED_BIT;
    sync_mem_log_val("pop called", ticket);

    for(;;) {
        uint32_t curr = chan_atomic_load32(&chan->ids[target]);
        sync_mem_log_val("pop loaded curr", curr);
        uint32_t closing = chan_atomic_load32(&chan->closing_state);
        if(closing) {
            if(_channel_ticket_pop_potentially_cancel(chan, ticket, closing) == false) {
                sync_mem_log_val("pop canceled", ticket);
                return false;
            }
        }
        
        sync_mem_log_val("pop loaded closing", closing);
        chan_wait_n(10);
        if(_channel_id_equals(curr, id))
            break;
        
        if(info.wake) {
            chan_atomic_or32(&chan->ids[target], _CHAN_ID_WAITING_BIT);
            curr |= _CHAN_ID_WAITING_BIT;
        }
        
        sync_mem_log_val("pop waiting", ticket);
        if(info.wait)
            info.wait(&chan->ids[target], curr, -1);
        else
            chan_pause();
        sync_mem_log_val("pop woken", ticket);
    }
    
    memcpy(item, chan->items + target*info.item_size, info.item_size);
    
    #ifdef CHANNEL_DEBUG
        uint32_t closing = chan_atomic_load32(&chan->closing_state);
        if((closing & ~CHANNEL_CLOSING_HARD) != 0)
        {
            uint64_t new_head = chan_atomic_load64(&chan->head);
            uint64_t barrier = chan_atomic_load64(&chan->head_barrier);

            const char* name = chan_thread_name(); (void) name;
            if(new_head & _CHAN_TICKET_POP_CLOSED_BIT)
                ASSERT(channel_ticket_is_less(ticket, barrier));
        }
        memset(chan->items + target*info.item_size, -1, info.item_size);
    #endif
    _channel_advance_id(chan, target, id, info);
    if(out_ticket_or_null)
        *out_ticket_or_null = ticket;
    
    sync_mem_log_val("pop done", ticket);
    return true;
}

CHANAPI Channel_Res channel_ticket_try_push_weak(Channel* chan, const void* item, uint64_t* out_ticket_or_null, Channel_Info info) 
{
    ASSERT(memcmp(&chan->info, &info, sizeof info) == 0, "info must be matching");
    ASSERT(item || (item == NULL && info.item_size == 0), "item must be provided");

    uint64_t tail = chan_atomic_load64(&chan->tail);
    uint64_t ticket = tail / _CHAN_TICKET_INCREMENT;
    uint64_t target = _channel_get_target(chan, ticket);
    uint32_t id = _channel_get_id(chan, ticket);
    
    chan_wait_n(3);
    uint32_t curr_id = chan_atomic_load32(&chan->ids[target]);
    chan_wait_n(3);
    uint32_t closing = chan_atomic_load32(&chan->closing_state);
    if(closing)
    {
        if(closing & CHANNEL_CLOSING_HARD)
            return CHANNEL_CLOSED;
        else
        {
            uint64_t new_tail = chan_atomic_load64(&chan->tail);
            chan_wait_n(10);
            uint64_t new_head = chan_atomic_load64(&chan->head);
            chan_wait_n(10);
            uint64_t barrier = chan_atomic_load64(&chan->tail_barrier);

            if((new_head & _CHAN_TICKET_PUSH_CLOSED_BIT) || (new_tail & _CHAN_TICKET_PUSH_CLOSED_BIT))
                if(channel_ticket_is_less_or_eq(barrier, ticket))
                    return CHANNEL_CLOSED;
        }
    }

    if(_channel_id_equals(curr_id, id) == false)
        return CHANNEL_FULL;
        
    chan_wait_n(3);
    if(chan_atomic_cas64(&chan->tail, tail, tail+_CHAN_TICKET_INCREMENT) == false)
        return CHANNEL_LOST_RACE;

    memcpy(chan->items + target*info.item_size, item, info.item_size);
    _channel_advance_id(chan, target, id, info);
    if(out_ticket_or_null)
        *out_ticket_or_null = ticket;

    return CHANNEL_OK;
}

CHANAPI Channel_Res channel_ticket_try_pop_weak(Channel* chan, void* item, uint64_t* out_ticket_or_null, Channel_Info info) 
{
    ASSERT(memcmp(&chan->info, &info, sizeof info) == 0, "info must be matching");
    ASSERT(item || (item == NULL && info.item_size == 0), "item must be provided");

    uint64_t head = chan_atomic_load64(&chan->head);
    uint64_t ticket = head / _CHAN_TICKET_INCREMENT;
    uint64_t target = _channel_get_target(chan, ticket);
    uint32_t id = _channel_get_id(chan, ticket) + _CHAN_ID_FILLED_BIT;
    
    chan_wait_n(3);
    uint32_t curr_id = chan_atomic_load32(&chan->ids[target]);
    chan_wait_n(3);
    uint32_t closing = chan_atomic_load32(&chan->closing_state);
    if(closing)
    {
        if(closing & CHANNEL_CLOSING_HARD)
            return CHANNEL_CLOSED;
        else
        {
            chan_wait_n(10);
            uint64_t new_head = chan_atomic_load64(&chan->head);
            chan_wait_n(10);
            uint64_t barrier = chan_atomic_load64(&chan->head_barrier);

            if((new_head & _CHAN_TICKET_POP_CLOSED_BIT))
                if(channel_ticket_is_less_or_eq(barrier, ticket))
                    return CHANNEL_CLOSED;
        }
    }

    if(_channel_id_equals(curr_id, id) == false)
        return CHANNEL_EMPTY;
        
    chan_wait_n(3);
    if(chan_atomic_cas64(&chan->head, head, head+_CHAN_TICKET_INCREMENT) == false)
        return CHANNEL_LOST_RACE;
        
    memcpy(item, chan->items + target*info.item_size, info.item_size);
    #ifdef CHANNEL_DEBUG
        memset(chan->items + target*info.item_size, -1, info.item_size);
    #endif
    _channel_advance_id(chan, target, id, info);
    if(out_ticket_or_null)
        *out_ticket_or_null = ticket;

    return CHANNEL_OK;
}

CHANAPI void _channel_close_lock(Channel* chan, Channel_Info info)
{
    uint32_t ticket = chan_atomic_add32(&chan->closing_lock_requested, 1);
    for(;;) {
        uint32_t curr_completed = chan_atomic_load32(&chan->closing_lock_completed);
        if(curr_completed == ticket)
            break;

        if(info.wait)
            info.wait(&chan->closing_lock_completed, curr_completed, -1);
        else
            chan_pause();
    }
}

CHANAPI void _channel_close_unlock(Channel* chan, Channel_Info info)
{
    chan_atomic_add32(&chan->closing_lock_completed, 1);
    if(info.wake)
        info.wake(&chan->closing_lock_completed);
}

CHANAPI void _channel_close_wakeup_ticket_range(Channel* chan, uint64_t from, uint64_t to, Channel_Info info)
{
    sync_mem_log_val2("close waking up range", from, to);
    if(info.wake)
    {
        for(uint64_t ticket = from; channel_ticket_is_less(ticket, to); ticket++)
        {
            uint64_t target = _channel_get_target(chan, ticket);
            chan_atomic_or32(&chan->ids[target], _CHAN_ID_CLOSE_NOTIFY_BIT);
            uint32_t id = chan_atomic_load32(&chan->ids[target]);
            if(id & _CHAN_ID_WAITING_BIT)
            {
                chan_atomic_and32(&chan->ids[target], ~_CHAN_ID_WAITING_BIT);
                sync_mem_log_val2("close waken up", ticket, id);
                info.wake(&chan->ids[target]);
            }
            else
            {
                sync_mem_log_val2("close ored", id, id & ~_CHAN_ID_CLOSE_NOTIFY_BIT);
            }
        }
    }
    sync_mem_log_val2("close waking up range done", from, to);
}

CHANAPI bool _channel_close_soft_custom(Channel* chan, Channel_Info info, bool push_close) 
{
    bool out = false;
    if(channel_is_closed(chan) == false)
    {
        _channel_close_lock(chan, info);
        if(channel_is_closed(chan) == false)
        {
            out = true;
            
            uint64_t tail = 0;
            uint64_t head = 0;
            uint64_t tail_barrier = 0;
            uint64_t head_barrier = 0;

            chan_atomic_or32(&chan->closing_state, CHANNEL_CLOSING_PUSH);
            for(;;) {
                tail = chan_atomic_load64(&chan->tail);
                head = chan_atomic_load64(&chan->head);

                uint64_t barrier_from_head = (head + chan->capacity*_CHAN_TICKET_INCREMENT)/_CHAN_TICKET_INCREMENT;
                uint64_t barrier_from_tail = tail/_CHAN_TICKET_INCREMENT;

                if(channel_ticket_is_less(barrier_from_head, barrier_from_tail))
                {
                    tail_barrier = barrier_from_head;
                    chan_atomic_store64(&chan->tail_barrier, tail_barrier);
                    if(chan_atomic_cas64(&chan->head, head, head | _CHAN_TICKET_PUSH_CLOSED_BIT))
                    {
                        //since we didnt CAS tail we dont know if it hasnt changed
                        // if it has changed. Thus we need to load it.
                        // - it has changed between the load at the start of the loop and the CAS 
                        //   then: the new load is accurate
                        // - it has changed between the CAS and this load 
                        //   then: the change must have been a result of backing off (thus is LESS)
                        //         because of this we also keep count of the number of backoffs
                        //         and add it to get upper estimate on the tail at the time of CAS.
                        chan_wait_n(20); 
                        uint64_t tail_after_backoff = chan_atomic_load64(&chan->tail);
                        chan_wait_n(10);
                        uint64_t tail_backed_off_count = chan_atomic_load64(&chan->tail_cancel_count);
                        tail = (tail_after_backoff + tail_backed_off_count) % CHANNEL_MAX_TICKET;
                        break;
                    }
                }
                else
                {
                    tail_barrier = barrier_from_tail;
                    chan_atomic_store64(&chan->tail_barrier, tail_barrier);
                    if(chan_atomic_cas64(&chan->tail, tail, tail | _CHAN_TICKET_PUSH_CLOSED_BIT))
                        break;
                }
            }
            
            sync_mem_log_val2("_channel_close_soft tail_barrier", tail_barrier, tail/_CHAN_TICKET_INCREMENT);

            chan_wait_n(10);
            chan_atomic_or32(&chan->closing_state, CHANNEL_CLOSING_POP);
            if(push_close)
            {
                head_barrier = tail_barrier;
                chan_atomic_store64(&chan->head_barrier, head_barrier);
                head = chan_atomic_or64(&chan->head, _CHAN_TICKET_POP_CLOSED_BIT);
            }
            else
            {
                for(;;) {
                    head = chan_atomic_load64(&chan->head);
                
                    uint64_t barrier_from_head = head/_CHAN_TICKET_INCREMENT;  // owo
                    uint64_t barrier_from_tail = tail_barrier;
                
                    head_barrier = channel_ticket_is_less(barrier_from_head, barrier_from_tail) ? barrier_from_head : barrier_from_tail;
                    chan_atomic_store64(&chan->head_barrier, head_barrier);
                    if(chan_atomic_cas64(&chan->head, head, head | _CHAN_TICKET_POP_CLOSED_BIT))
                        break;
                }
            }

            uint64_t head_ticket = head/_CHAN_TICKET_INCREMENT;
            uint64_t tail_ticket = tail/_CHAN_TICKET_INCREMENT;

            bool limited = channel_ticket_is_less(tail_barrier, tail_ticket);
            ASSERT(channel_ticket_is_less_or_eq(head_barrier, tail_barrier));
            ASSERT(channel_ticket_is_less_or_eq(tail_barrier, tail_ticket));
            if(push_close == false)
                ASSERT(channel_ticket_is_less_or_eq(head_barrier, head_ticket));

            sync_mem_log_val2("_channel_close_soft head_barrier", head_barrier, head_ticket);
            sync_mem_log_val2("_channel_close_soft limiting", (uint64_t) limited, (uint64_t) chan->capacity);

            _channel_close_wakeup_ticket_range(chan, head_barrier, head_ticket, info);
            _channel_close_wakeup_ticket_range(chan, tail_barrier, tail_ticket, info);
            
            chan_atomic_or32(&chan->closing_state, CHANNEL_CLOSING_CLOSED);
        }
        _channel_close_unlock(chan, info);
    }

    return out;
}

CHANAPI bool channel_close_soft(Channel* chan, Channel_Info info) 
{
    sync_mem_log("channel_close_soft called");
    bool out = _channel_close_soft_custom(chan, info, false);
    sync_mem_log("channel_close_soft done");
    return out;
}

CHANAPI bool channel_close_push(Channel* chan, Channel_Info info) 
{
    sync_mem_log("channel_close_push called");
    bool out = _channel_close_soft_custom(chan, info, true);
    sync_mem_log("channel_close_push done");
    return out;
}

CHANAPI bool channel_is_invariant_converged_state(Channel* chan, Channel_Info info) 
{
    bool out = true;
    if(channel_is_hard_closed(chan) == false)
    {
        uint64_t tail_and_closed = chan_atomic_load64(&chan->tail);
        uint64_t head_and_closed = chan_atomic_load64(&chan->head);

        uint64_t tail = tail_and_closed/_CHAN_TICKET_INCREMENT;
        uint64_t head = head_and_closed/_CHAN_TICKET_INCREMENT;
        
        uint64_t tail_barrier = chan_atomic_load64(&chan->tail_barrier);
        uint64_t head_barrier = chan_atomic_load64(&chan->head_barrier);

        uint32_t closing = chan_atomic_load32(&chan->closing_state);
        if(closing & CHANNEL_CLOSING_CLOSED)
        {
            int64_t dist_barrier = (int64_t)(tail_barrier - head_barrier);
            out = out && 0 <= dist_barrier && dist_barrier <= chan->capacity;
            out = out && ((tail_and_closed & _CHAN_TICKET_PUSH_CLOSED_BIT) || (head_and_closed & _CHAN_TICKET_PUSH_CLOSED_BIT));
            out = out && (head_and_closed & _CHAN_TICKET_POP_CLOSED_BIT);
        }
        else
        {
            out = out && tail*_CHAN_TICKET_INCREMENT == tail_and_closed;
            out = out && head*_CHAN_TICKET_INCREMENT == head_and_closed;
            out = out && tail_barrier == 0;
            out = out && head_barrier == 0;
        }
        //ASSERT(out);

        uint64_t head_p_cap = (head + chan->capacity) % CHANNEL_MAX_TICKET;
        uint64_t max_filled = channel_ticket_is_less(tail, head_p_cap) ? tail : head_p_cap;

        for(uint64_t ticket = head; channel_ticket_is_less(ticket, max_filled); ticket ++)
        {
            uint64_t target = _channel_get_target(chan, ticket);
            uint32_t id = _channel_get_id(chan, ticket) + _CHAN_ID_FILLED_BIT;
            uint32_t curr_id = chan->ids[target];
            out = out && _channel_id_equals(curr_id, id);
        }

        for(uint64_t ticket = max_filled; channel_ticket_is_less(ticket, head_p_cap); ticket ++)
        {
            uint64_t target = _channel_get_target(chan, ticket);
            uint32_t id = _channel_get_id(chan, ticket);
            uint32_t curr_id = chan->ids[target];
            out = out && _channel_id_equals(curr_id, id);
                
            #ifdef CHANNEL_DEBUG
            uint8_t* item = chan->items + target*info.item_size;
            bool is_empty_invariant = true;
            for(isize i = 0; i < info.item_size; i++)
                is_empty_invariant = is_empty_invariant && item[i] == (uint8_t) -1;

            out = out && is_empty_invariant;
            //ASSERT(is_empty_invariant);
            #endif
        }
    }

    return out;
}
CHANAPI bool channel_reopen(Channel* chan, Channel_Info info) 
{
    ASSERT(memcmp(&chan->info, &info, sizeof info) == 0, "info must be matching");

    sync_mem_log("channel_reopen called");
    bool out = false;
    if(channel_is_closed(chan))
    {
        _channel_close_lock(chan, info);
        if(channel_is_closed(chan) && channel_is_hard_closed(chan) == false)
        {
            sync_mem_log("channel_reopen lock start");
            chan_atomic_store32(&chan->closing_state, 0);
            for(isize i = 0; i < chan->capacity; i++)
                chan_atomic_and32(&chan->ids[i], ~_CHAN_ID_CLOSE_NOTIFY_BIT);

            chan_atomic_and64(&chan->head, ~(_CHAN_TICKET_PUSH_CLOSED_BIT | _CHAN_TICKET_POP_CLOSED_BIT));
            chan_atomic_and64(&chan->tail, ~(_CHAN_TICKET_PUSH_CLOSED_BIT | _CHAN_TICKET_POP_CLOSED_BIT));
            chan_atomic_store64(&chan->head_barrier, 0);
            chan_atomic_store64(&chan->head_cancel_count, 0);
            chan_atomic_store64(&chan->tail_barrier, 0);
            chan_atomic_store64(&chan->tail_cancel_count, 0);

            out = true;
            sync_mem_log("channel_reopen lock end");
        }
        _channel_close_unlock(chan, info);
    }
    sync_mem_log("channel_reopen done");
    return out;
}

CHANAPI bool channel_close_hard(Channel* chan, Channel_Info info)
{
    ASSERT(memcmp(&chan->info, &info, sizeof info) == 0, "info must be matching");
    
    sync_mem_log("channel_close_hard called");
    bool out = (chan_atomic_or32(&chan->closing_state, CHANNEL_CLOSING_HARD) & CHANNEL_CLOSING_HARD) == 0;
    sync_mem_log("channel_close_hard done");
    return out;
}

CHANAPI Channel_Res channel_ticket_try_push(Channel* chan, const void* item, uint64_t* out_ticket_or_null, Channel_Info info)
{
    for(;;) {
        Channel_Res res = channel_ticket_try_push_weak(chan, item, out_ticket_or_null, info);
        if(res != CHANNEL_LOST_RACE)
            return res;
    }
}

CHANAPI Channel_Res channel_ticket_try_pop(Channel* chan, void* item, uint64_t* out_ticket_or_null, Channel_Info info)
{
    for(;;) {
        Channel_Res res = channel_ticket_try_pop_weak(chan, item, out_ticket_or_null, info);
        if(res != CHANNEL_LOST_RACE)
            return res;
    }
}

CHANAPI bool channel_push(Channel* chan, const void* item, Channel_Info info)
{
    return channel_ticket_push(chan, item, NULL, info);
}
CHANAPI bool channel_pop(Channel* chan, void* item, Channel_Info info)
{
    return channel_ticket_pop(chan, item, NULL, info);
}
CHANAPI Channel_Res channel_try_push_weak(Channel* chan, const void* item, Channel_Info info)
{
    return channel_ticket_try_push_weak(chan, item, NULL, info);
}
CHANAPI Channel_Res channel_try_pop_weak(Channel* chan, void* item, Channel_Info info)
{
    return channel_ticket_try_pop_weak(chan, item, NULL, info);
}
CHANAPI Channel_Res channel_try_push(Channel* chan, const void* item, Channel_Info info)
{
    return channel_ticket_try_push(chan, item, NULL, info);
}
CHANAPI Channel_Res channel_try_pop(Channel* chan, void* item, Channel_Info info)
{
    return channel_ticket_try_pop(chan, item, NULL, info);
}

CHANAPI isize channel_signed_distance(const Channel* chan)
{
    uint64_t head = chan_atomic_load64(&chan->head);
    uint64_t tail = chan_atomic_load64(&chan->tail);

    uint64_t diff = tail/_CHAN_TICKET_INCREMENT - head/_CHAN_TICKET_INCREMENT;
    return (isize) diff;
}

CHANAPI isize channel_count(const Channel* chan) 
{
    isize dist = channel_signed_distance(chan);
    if(dist <= 0)
        return 0;
    if(dist >= chan->capacity)
        return chan->capacity;
    else
        return dist;
}

CHANAPI bool channel_is_empty(const Channel* chan) 
{
    return channel_signed_distance(chan) <= 0;
}

CHANAPI bool channel_is_closed(const Channel* chan) 
{
    uint32_t closing = chan_atomic_load32(&chan->closing_state);
    return closing != 0;
}

CHANAPI bool channel_is_hard_closed(const Channel* chan) 
{
    uint32_t closing = chan_atomic_load32(&chan->closing_state);
    return (closing & CHANNEL_CLOSING_HARD) != 0;
}

CHANAPI bool channel_ticket_is_less(uint64_t ticket_a, uint64_t ticket_b)
{
    uint64_t diff = ticket_a - ticket_b;
    int64_t signed_diff = (int64_t) diff; 
    return signed_diff < 0;
}

CHANAPI bool channel_ticket_is_less_or_eq(uint64_t ticket_a, uint64_t ticket_b)
{
    uint64_t diff = ticket_a - ticket_b;
    int64_t signed_diff = (int64_t) diff; 
    return signed_diff <= 0;
}

CHANAPI void channel_init(Channel* chan, void* items, uint32_t* ids, isize capacity, Channel_Info info)
{
    ASSERT(ids);
    ASSERT(capacity > 0 && "must be nonzero");
    ASSERT(items != NULL || (items == NULL && info.item_size == 0));
;
    memset(chan, 0, sizeof* chan);
    chan->items = (uint8_t*) items;
    chan->ids = ids;
    chan->capacity = capacity; 
    chan->info = info;
    chan->ref_count = 1;

    memset(ids, 0, (size_t) capacity*sizeof *ids);
    #ifdef CHANNEL_DEBUG
        memset(items, -1, (size_t) capacity*info.item_size);
    #endif

    //essentially a memory fence with respect to any other function
    chan_atomic_store64(&chan->head, 0);
    chan_atomic_store64(&chan->tail, 0);
    chan_atomic_store32(&chan->closing_state, 0);
}

CHANAPI isize channel_memory_size(isize capacity, Channel_Info info)
{
    return sizeof(Channel) + capacity*sizeof(uint32_t) + capacity*info.item_size;
}

CHANAPI Channel* channel_init_into_memory(void* aligned_memory, isize capacity, Channel_Info info)
{
    Channel* chan = (Channel*) aligned_memory;
    if(chan)
    {
        uint32_t* ids = (uint32_t*) (void*) (chan + 1);
        void* items = ids + capacity;

        channel_init(chan, items, ids, capacity, info);
        chan_atomic_store32(&chan->allocated, true);
    }
    return chan;
}

CHANAPI Channel* channel_malloc(isize capacity, Channel_Info info)
{
    isize total_size = channel_memory_size(capacity, info);
    void* mem = chan_aligned_alloc(total_size, CHAN_CACHE_LINE);
    return channel_init_into_memory(mem, capacity, info);
}

CHANAPI Channel* channel_share(Channel* chan)
{
    if(chan != NULL)
        chan_atomic_add32(&chan->ref_count, 1);
    return chan;
}

CHANAPI int32_t channel_deinit(Channel* chan)
{
    if(chan == NULL)
        return 0;

    int32_t refs = (int32_t) chan_atomic_sub32(&chan->ref_count, 1) - 1;
    if(refs == 0)
    {
        if(chan_atomic_load32(&chan->allocated))
            chan_aligned_free(chan, CHAN_CACHE_LINE);
        else
            memset(chan, 0, sizeof *chan);
    }

    return refs;
}

//SYNC

CHANAPI bool sync_wait(volatile void* state, uint32_t current, isize timeout, Sync_Wait wait)
{
    if(wait.notify_bit) 
    {
        chan_atomic_or32(state, wait.notify_bit);
        current |= wait.notify_bit;
    }

    if(wait.wait)
        return wait.wait((void*) state, current, timeout);
    else
        return false;
}

CHANAPI void sync_wake(volatile void* state, uint32_t prev, Sync_Wait wait)
{
    if(wait.wake)
    {
        if(wait.notify_bit)
        {
            if(prev & wait.notify_bit)
                wait.wake((void*) state);
        }
        else
            wait.wake((void*) state);
    }
}

CHANAPI void sync_set_and_wake(volatile void* state, uint32_t to, Sync_Wait wait)
{
    if(wait.wake == NULL)
        chan_atomic_store32(state, to);
    else
    {
        uint32_t prev = chan_atomic_exchange32(state, to);
        sync_wake(state, prev, wait);
    }
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

CHANAPI uint32_t sync_wait_for_not_equal(volatile void* state, uint32_t desired, Sync_Wait wait)
{
    for(;;) {
        uint32_t current = chan_atomic_load32(state);
        sync_mem_log_val("wait neq", current);
        if((current & ~wait.notify_bit) != (desired & ~wait.notify_bit))
        {
            sync_mem_log_val("wait neq done", desired);
            return current & ~wait.notify_bit;
        }
        
        sync_mem_log_val("wait neq still waiting", current);
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

#define SYNC_WAIT_FOR(state, cond_, wait)
    

CHANAPI uint32_t sync_wait_for_greater(volatile void* state, uint32_t desired, Sync_Wait wait)
{
    for(;;) {
        uint32_t current = chan_atomic_load32(state);
        if((current & ~wait.notify_bit) > (desired & ~wait.notify_bit))
            return current & ~wait.notify_bit;
            
        sync_wait(state, current, -1, wait);
    }
}

CHANAPI Sync_Timed_Wait sync_timed_wait_start(double wait)
{
    static double freq_s = 0;
    static isize freq_ms = 0;
    if(freq_ms == 0)
    {
        freq_s = (double) chan_perf_frequency();
        freq_ms = (chan_perf_frequency() + 999)/1000;
    }

    Sync_Timed_Wait out = {0};
    out.freq_s = freq_s;
    out.freq_ms = freq_ms;
    out.wait_ticks = (isize) (wait*freq_s);
    out.start_ticks = chan_perf_counter();
    return out;
}

CHANAPI bool sync_timed_wait(volatile void* state, uint32_t current, Sync_Timed_Wait timeout, Sync_Wait wait)
{
    isize curr_ticks = chan_perf_counter();
    isize ellapsed_ticks = curr_ticks - timeout.start_ticks;
    if(ellapsed_ticks >= timeout.wait_ticks)
        return false;
        
    isize wait_ms = (timeout.wait_ticks - ellapsed_ticks)/timeout.freq_ms;
    return sync_wait(state, current, wait_ms, wait);
}

CHANAPI bool sync_timed_wait_for_equal(volatile void* state, uint32_t desired, double timeout, Sync_Wait wait)
{
    for(Sync_Timed_Wait timed_wait = sync_timed_wait_start(timeout);;) {
        uint32_t current = chan_atomic_load32(state);
        if((current & ~wait.notify_bit) == (desired & ~wait.notify_bit))
            return true;

        if(sync_timed_wait(state, current, timed_wait, wait) == false)
            return false;
    }
}

CHANAPI bool sync_timed_wait_for_not_equal(volatile void* state, uint32_t desired, double timeout, Sync_Wait wait)
{
    for(Sync_Timed_Wait timed_wait = sync_timed_wait_start(timeout);;) {
        uint32_t current = chan_atomic_load32(state);
        if((current & ~wait.notify_bit) != (desired & ~wait.notify_bit))
            return true;

        if(sync_timed_wait(state, current, timed_wait, wait) == false)
            return false;
    }
}

CHANAPI bool sync_timed_wait_for_smaller(volatile void* state, uint32_t desired, double timeout, Sync_Wait wait)
{
    for(Sync_Timed_Wait timed_wait = sync_timed_wait_start(timeout);;) {
        uint32_t current = chan_atomic_load32(state);
        if((current & ~wait.notify_bit) < (desired & ~wait.notify_bit))
            return true;

        if(sync_timed_wait(state, current, timed_wait, wait) == false)
            return false;
    }
}

CHANAPI bool sync_timed_wait_for_greater(volatile void* state, uint32_t desired, double timeout, Sync_Wait wait)
{
    for(Sync_Timed_Wait timed_wait = sync_timed_wait_start(timeout);;) {
        uint32_t current = chan_atomic_load32(state);
        if((current & ~wait.notify_bit) > (desired & ~wait.notify_bit))
            return true;

        if(sync_timed_wait(state, current, timed_wait, wait) == false)
            return false;
    }
}

CHANAPI bool sync_once_begin(volatile Sync_Once* once, Sync_Wait wait)
{
    uint32_t before_value = chan_atomic_load32(once);
    if(before_value != SYNC_ONCE_INIT)
        return false;

    if(chan_atomic_cas32(once, SYNC_ONCE_UNINIT, SYNC_ONCE_INITIALIZING))
        return true;
    else
        sync_wait_for_equal(once, SYNC_ONCE_INIT, wait);
    return false;
}
CHANAPI void sync_once_end(volatile Sync_Once* once, Sync_Wait wait)
{
    sync_set_and_wake(once, SYNC_ONCE_INIT, wait);
}
CHANAPI bool sync_once(volatile Sync_Once* once, void(*func)(void* context), void* context, Sync_Wait wait)
{
    if(sync_once_begin(once, wait))
    {
        func(context);
        sync_once_end(once, wait);
        return true;
    }
    return false;
}

CHANAPI int32_t wait_group_count(volatile Wait_Group* wg)
{
    return (int32_t) chan_atomic_load32(&wg->count);
}
CHANAPI Wait_Group* wait_group_push(volatile Wait_Group* wg, isize count)
{
    if(count > 0)
        chan_atomic_add32(&wg->count, (uint32_t) count);
    return (Wait_Group*) wg;
}
CHANAPI bool wait_group_pop(volatile Wait_Group* wg, isize count, Sync_Wait wait)
{
    bool out = false;
    if(count <= 0)
        sync_mem_log_val("wait_group_pop negative", (uint64_t) -count);
    else
    {
        int32_t old_val = (int32_t) chan_atomic_sub32(&wg->count, (uint32_t) count);
        sync_mem_log_val2("wait_group_pop", (uint32_t) old_val, (uint32_t) old_val - (uint32_t) count);

        //if this was the pop that got it over the line wake 
        if(old_val - count <= 0 && old_val > 0)
        {
            chan_atomic_add32(&wg->wakes, 1);
            sync_mem_log("wait_group_pop WAKE");
            wait.wake(wg);
            out = true;
        }
    }

    return out;
}
CHANAPI void wait_group_wait(volatile Wait_Group* wg, Sync_Wait wait)
{
    Wait_Group before = {chan_atomic_load64(wg)};
    Wait_Group curr = before;
    for(;; curr.combined = chan_atomic_load64(wg)) {
        sync_mem_log_val2("wait_group_wait", (uint32_t) curr.count, curr.wakes);
        if(curr.count <= 0 || before.wakes != curr.wakes)
        {
            sync_mem_log_val2("wait_group_wait done", (uint32_t) curr.count, curr.wakes);
            return;
        }

        sync_mem_log_val("wait_group_wait WAIT", curr.count);
        if(wait.wait)
            wait.wait(wg, (uint32_t) curr.count, -1);
        else
            chan_pause();
    }
}

CHANAPI bool wait_group_wait_timed(volatile Wait_Group* wg, double timeout, Sync_Wait wait)
{
    static double freq_s = 0;
    static isize freq_ms = 0;
    if(freq_ms == 0)
    {
        freq_s = (double) chan_perf_frequency();
        freq_ms = (chan_perf_frequency() + 999)/1000;
    }

    isize wait_ticks = (isize) (timeout*freq_s);
    isize start_ticks = chan_perf_counter();

    Wait_Group before = {chan_atomic_load64(wg)};
    Wait_Group curr = before;
    for(;; curr.combined = chan_atomic_load64(wg)) {
        sync_mem_log_val2("wait_group_wait_timed", (uint32_t) curr.count, curr.wakes);
        if(curr.count <= 0 || before.wakes != curr.wakes)
        {
            sync_mem_log_val2("wait_group_wait_timed done", (uint32_t) curr.count, curr.wakes);
            return true;
        }

        isize curr_ticks = chan_perf_counter();
        isize ellapsed_ticks = curr_ticks - start_ticks;
        if(ellapsed_ticks > wait_ticks)
        {
            sync_mem_log_val("wait_group_wait_timed given after ticks", (uint64_t) wait_ticks);
            return false;
        }
        
        isize wait_ms = (wait_ticks - ellapsed_ticks)/freq_ms;
        if(wait.wait)
            wait.wait(wg, (uint32_t) curr.count, wait_ms);
        else
            chan_pause();
    }
}

//#define CHAN_TESTING

#ifdef _MSC_VER
    #ifdef CHAN_TESTING
        CHAN_OS_API void chan_random_sleep();
        #define CHAN_RAND_SLEEP() chan_random_sleep()
    #else
        #define CHAN_RAND_SLEEP()
    #endif

    //#include <winnt.h>
    //#include <stdatomic.h>
    CHAN_INTRINSIC uint64_t chan_atomic_load64(const volatile void* target)
    {
        CHAN_RAND_SLEEP();
        uint64_t out = (uint64_t) __iso_volatile_load64((const long long*) target);
        //MemoryBarrier(); //???
        return out;
    }
    CHAN_INTRINSIC uint32_t chan_atomic_load32(const volatile void* target)
    {
        CHAN_RAND_SLEEP();
        return (uint32_t) __iso_volatile_load32((const int*) target);
    }
    CHAN_INTRINSIC void chan_atomic_store64(volatile void* target, uint64_t value)
    {
        CHAN_RAND_SLEEP();
        __iso_volatile_store64((long long*) target, (long long) value);
    }
    CHAN_INTRINSIC void chan_atomic_store32(volatile void* target, uint32_t value)
    {
        CHAN_RAND_SLEEP();
        __iso_volatile_store32((int*) target, (int) value);
    }
    CHAN_INTRINSIC bool chan_atomic_cas64(volatile void* target, uint64_t old_value, uint64_t new_value)
    {
        CHAN_RAND_SLEEP();
        return _InterlockedCompareExchange64((long long*) target, (long long) new_value, (long long) old_value) == (long long) old_value;
    }
    CHAN_INTRINSIC bool chan_atomic_cas32(volatile void* target, uint32_t old_value, uint32_t new_value)
    {
        CHAN_RAND_SLEEP();
        return _InterlockedCompareExchange((long*) target, (long) new_value, (long) old_value) == (long) old_value;
    }
    CHAN_INTRINSIC uint64_t chan_atomic_exchange64(volatile void* target, uint64_t value)
    {
        CHAN_RAND_SLEEP();
        return (uint64_t) _InterlockedExchange64((long long*) target, (long long) value);
    }
    CHAN_INTRINSIC uint32_t chan_atomic_exchange32(volatile void* target, uint32_t value)
    {
        CHAN_RAND_SLEEP();
        return (uint32_t) _InterlockedExchange((long*) target, (long) value);
    }
    CHAN_INTRINSIC uint64_t chan_atomic_add64(volatile void* target, uint64_t value)
    {
        CHAN_RAND_SLEEP();
        return (uint64_t) _InterlockedExchangeAdd64((long long*) (void*) target, (long long) value);
    }
    CHAN_INTRINSIC uint32_t chan_atomic_add32(volatile void* target, uint32_t value)
    {
        CHAN_RAND_SLEEP();
        return (uint32_t) _InterlockedExchangeAdd((long*) (void*) target, (long long) value);
    }
    CHAN_INTRINSIC uint64_t chan_atomic_sub64(volatile void* target, uint64_t value)
    {
        CHAN_RAND_SLEEP();
        return chan_atomic_add64(target, (uint64_t) -(int64_t) value);
    }
    CHAN_INTRINSIC uint32_t chan_atomic_sub32(volatile void* target, uint32_t value)
    {
        CHAN_RAND_SLEEP();
        return chan_atomic_add32(target, (uint64_t) -(int64_t) value);
    }
    CHAN_INTRINSIC uint64_t chan_atomic_or64(volatile void* target, uint64_t value)
    {
        CHAN_RAND_SLEEP();
        return (uint64_t) _InterlockedOr64((long long*) (void*) target, (long long) value);
    }
    CHAN_INTRINSIC uint32_t chan_atomic_or32(volatile void* target, uint32_t value)
    {
        CHAN_RAND_SLEEP();
        return (uint32_t) _InterlockedOr((long*) (void*) target, (long) value);
    }
    CHAN_INTRINSIC uint64_t chan_atomic_and64(volatile void* target, uint64_t value)
    {
        CHAN_RAND_SLEEP();
        return (uint64_t) _InterlockedAnd64((long long*) (void*) target, (long long) value);
    }
    CHAN_INTRINSIC uint32_t chan_atomic_and32(volatile void* target, uint32_t value)
    {
        CHAN_RAND_SLEEP();
        return (uint32_t) _InterlockedAnd((long*) (void*) target, (long) value);
    }
    CHAN_INTRINSIC uint32_t chan_atomic_xor32(volatile void* target, uint32_t value)
    {
        CHAN_RAND_SLEEP();
        return (uint32_t) _InterlockedXor((long*) (void*) target, (long) value);
    }
    CHAN_INTRINSIC bool chan_atomic_cas128(volatile void* target, uint64_t old_value_lo, uint64_t old_value_hi, uint64_t new_value_lo, uint64_t new_value_hi)
    {
        long long new_val[] = {(long long) old_value_lo, (long long) old_value_hi};
        return (bool) _InterlockedCompareExchange128((volatile long long*) target, (long long) new_value_hi, (long long) new_value_lo, new_val);
    }
#elif defined(__GNUC__) || defined(__clang__)
    //for reference see: https://gcc.gnu.org/onlinedocs/gcc/_005f_005fatomic-Builtins.html
    CHAN_INTRINSIC bool chan_atomic_cas64(volatile void* target, uint64_t old_value, uint64_t new_value)
    {
        return __atomic_compare_exchange_n(target, &old_value, new_value, false, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST);
    }
    CHAN_INTRINSIC bool chan_atomic_cas32(volatile void* target, uint32_t old_value, uint32_t new_value)
    {
        return __atomic_compare_exchange_n(target, &old_value, new_value, false, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST);
    }
    CHAN_INTRINSIC uint64_t chan_atomic_load64(volatile const uint64_t* target)
    {
        return (uint64_t) __atomic_load_n(target, __ATOMIC_SEQ_CST);
    }
    CHAN_INTRINSIC uint32_t chan_atomic_load32(volatile const uint32_t* target)
    {
        return (uint32_t) __atomic_load_n(target, __ATOMIC_SEQ_CST);
    }
    CHAN_INTRINSIC void chan_atomic_store64(volatile void* target, uint64_t value)
    {
        __atomic_store_n(target, value, __ATOMIC_SEQ_CST);
    }
    CHAN_INTRINSIC void chan_atomic_store32(volatile void* target, uint32_t value)
    {
        __atomic_store_n(target, value, __ATOMIC_SEQ_CST);
    }
    CHAN_INTRINSIC uint64_t chan_atomic_exchange64(volatile void* target, uint64_t value)
    {
        return (uint64_t) __atomic_exchange_n(target, value, __ATOMIC_SEQ_CST);
    }
    CHAN_INTRINSIC uint32_t chan_atomic_exchange32(volatile void* target, uint32_t value)
    {
        return (uint32_t) __atomic_exchange_n(target, value, __ATOMIC_SEQ_CST);
    }
    CHAN_INTRINSIC uint32_t chan_atomic_add32(volatile void* target, uint32_t value)
    {
        return (uint32_t) __atomic_add_fetch(target, value, __ATOMIC_SEQ_CST);
    }
    CHAN_INTRINSIC uint64_t chan_atomic_add64(volatile void* target, uint64_t value)
    {
        return (uint64_t) __atomic_add_fetch(target, value, __ATOMIC_SEQ_CST);
    }
    CHAN_INTRINSIC uint32_t chan_atomic_sub32(volatile void* target, uint32_t value)
    {
        return (uint32_t) __atomic_sub_fetch(target, value, __ATOMIC_SEQ_CST);
    }
    CHAN_INTRINSIC uint64_t chan_atomic_sub64(volatile void* target, uint64_t value)
    {
        return (uint64_t) __atomic_sub_fetch(target, value, __ATOMIC_SEQ_CST);
    }
    CHAN_INTRINSIC uint32_t chan_atomic_and32(volatile void* target, uint32_t value)
    {
        return (uint32_t) __atomic_and_fetch(target, value, __ATOMIC_SEQ_CST);
    }
    CHAN_INTRINSIC uint64_t chan_atomic_and64(volatile void* target, uint64_t value)
    {
        return (uint64_t) __atomic_and_fetch(target, value, __ATOMIC_SEQ_CST);
    }
    CHAN_INTRINSIC uint32_t chan_atomic_or32(volatile void* target, uint32_t value)
    {
        return (uint32_t) __atomic_or_fetch(target, value, __ATOMIC_SEQ_CST);
    }
    CHAN_INTRINSIC uint64_t chan_atomic_or64(volatile void* target, uint64_t value)
    {
        return (uint64_t) __atomic_or_fetch(target, value, __ATOMIC_SEQ_CST);
    }
#else
    #error UNSUPPORTED COMPILER
#endif

//ARCH DETECTION
#define CHAN_ARCH_UNKNOWN   0
#define CHAN_ARCH_X86       1
#define CHAN_ARCH_X64       2
#define CHAN_ARCH_ARM       3

#ifndef CHAN_ARCH
    #if defined(_M_CEE_PURE) || defined(_M_IX86)
        #define CHAN_ARCH CHAN_ARCH_X86
    #elif defined(_M_X64) && !defined(_M_ARM64EC)
        #define CHAN_ARCH CHAN_ARCH_X64
    #elif defined(_M_ARM) || defined(_M_ARM64) || defined(_M_ARM64EC)
        #define CHAN_ARCH CHAN_ARCH_ARM
    #else
        #define CHAN_ARCH CHAN_ARCH_UNKNOWN
    #endif
#endif

//OS DETECTION
#define CHAN_OS_UNKNOWN     0 
#define CHAN_OS_WINDOWS     1
#define CHAN_OS_ANDROID     2
#define CHAN_OS_UNIX        3 // Debian, Ubuntu, Gentoo, Fedora, openSUSE, RedHat, Centos and other
#define CHAN_OS_BSD         4 // FreeBSD, NetBSD, OpenBSD, DragonFly BSD
#define CHAN_OS_APPLE_IOS   5
#define CHAN_OS_APPLE_OSX   6
#define CHAN_OS_SOLARIS     7 // Oracle Solaris, Open Indiana
#define CHAN_OS_HP_UX       8
#define CHAN_OS_IBM_AIX     9

#if !defined(CHAN_OS) || CHAN_OS == CHAN_OS_UNKNOWN
    #undef CHAN_OS
    #if defined(_WIN32)
        #define CHAN_OS CHAN_OS_WINDOWS // Windows
    #elif defined(_WIN64)
        #define CHAN_OS CHAN_OS_WINDOWS // Windows
    #elif defined(__CYGWIN__) && !defined(_WIN32)
        #define CHAN_OS CHAN_OS_WINDOWS // Windows (Cygwin POSIX under Microsoft Window)
    #elif defined(__ANDROID__)
        #define CHAN_OS CHAN_OS_ANDROID // Android (implies Linux, so it must come first)
    #elif defined(__linux__)
        #define CHAN_OS CHAN_OS_UNIX // Debian, Ubuntu, Gentoo, Fedora, openSUSE, RedHat, Centos and other
    #elif defined(__unix__) || !defined(__APPLE__) && defined(__MACH__)
        #include <sys/param.h>
        #if defined(BSD)
            #define CHAN_OS CHAN_OS_BSD // FreeBSD, NetBSD, OpenBSD, DragonFly BSD
        #endif
    #elif defined(__hpux)
        #define CHAN_OS CHAN_OS_HP_UX // HP-UX
    #elif defined(_AIX)
        #define CHAN_OS CHAN_OS_IBM_AIX // IBM AIX
    #elif defined(__APPLE__) && defined(__MACH__) // Apple OSX and iOS (Darwin)
        #include <TargetConditionals.h>
        #if TARGET_IPHONE_SIMULATOR == 1
            #define CHAN_OS CHAN_OS_APPLE_IOS // Apple iOS
        #elif TARGET_OS_IPHONE == 1
            #define CHAN_OS CHAN_OS_APPLE_IOS // Apple iOS
        #elif TARGET_OS_MAC == 1
            #define CHAN_OS CHAN_OS_APPLE_OSX // Apple OSX
        #endif
    #elif defined(__sun) && defined(__SVR4)
        #define CHAN_OS CHAN_OS_SOLARIS // Oracle Solaris, Open Indiana
    #else
        #define CHAN_OS CHAN_OS_UNKNOWN
    #endif
#endif 

#include <intrin.h>
CHAN_INTRINSIC void chan_pause()
{
    #if CHAN_ARCH == CHAN_ARCH_X86 || CHAN_ARCH == CHAN_ARCH_X64
        _mm_pause();
    #elif CHAN_ARCH == CHAN_ARCH_ARM
        __yield();
    #endif
}

#if CHAN_OS == CHAN_OS_WINDOWS
    #pragma comment(lib, "synchronization.lib")

    typedef int BOOL;
    typedef unsigned long DWORD;

    void __stdcall WakeByAddressSingle(void*);
    void __stdcall WakeByAddressAll(void*);
    BOOL __stdcall WaitOnAddress(volatile void* Address, 
        void* CompareAddress, size_t AddressSize, DWORD dwMilliseconds);

    BOOL __stdcall SwitchToThread(void);
    void __stdcall Sleep(DWORD);

    typedef union _LARGE_INTEGER LARGE_INTEGER;
    BOOL __stdcall QueryPerformanceCounter(LARGE_INTEGER* ticks);
    BOOL __stdcall QueryPerformanceFrequency(LARGE_INTEGER* ticks);

    __declspec(dllimport)
    DWORD GetCurrentThreadId(void);

    uintptr_t _beginthread( // NATIVE CODE
       void( __cdecl *start_address )( void * ),
       unsigned stack_size,
       void *arglist
    );
    
    _ACRTIMP void __cdecl _aligned_free(void* _Block);
    _ACRTIMP void* __cdecl _aligned_malloc(size_t _Size, size_t _Alignment);

    CHAN_OS_API int64_t chan_perf_counter()
    {
        int64_t ticks = 0;
        (void) QueryPerformanceCounter((LARGE_INTEGER*) (void*)  &ticks);
        return ticks;
    }

    CHAN_OS_API int64_t chan_perf_frequency()
    {
        int64_t ticks = 0;
        (void) QueryPerformanceFrequency((LARGE_INTEGER*) (void*) &ticks);
        return ticks;
    }
    
    CHAN_OS_API void* chan_aligned_alloc(isize bytes, isize aligned)
    {
        return _aligned_malloc((size_t) bytes, (size_t) aligned);
    }
    CHAN_OS_API void chan_aligned_free(void* block, isize aligned)
    {
        (void) aligned;
        _aligned_free(block);
    }

    CHAN_OS_API bool chan_wait_block(volatile void* state, uint32_t undesired, isize timeout_or_minus_one_if_infinite)
    {
        DWORD wait = (DWORD) timeout_or_minus_one_if_infinite;
        if(timeout_or_minus_one_if_infinite < 0)
            wait = (DWORD) -1; //INFINITE

        CHAN_RAND_SLEEP();
        bool value_changed = (bool) WaitOnAddress(state, &undesired, sizeof undesired, wait);
        if(!value_changed)
            sync_mem_log_val("futex timed out", value_changed);
        CHAN_RAND_SLEEP();
        return value_changed;
    }
    CHAN_OS_API bool chan_wait_yield(volatile void* state, uint32_t undesired, isize timeout_or_minus_one_if_infinite)
    {
        (void) state; (void) undesired; (void) timeout_or_minus_one_if_infinite;
        SwitchToThread();
        return true;
    }
    CHAN_OS_API void chan_wake_block(volatile void* state)
    {
        WakeByAddressAll((void*) state);
    }
    CHAN_OS_API void chan_start_thread(void(*start_address)(void *), void* args)
    {
        _beginthread(start_address, 0, args);
    }
    CHAN_OS_API void chan_sleep(double seconds)
    {
        if(seconds >= 0)
            Sleep((DWORD)(seconds * 1000));
    }
    
    static double _g_rand_sleep_chance = 0;
    static double _g_rand_sleep_max = 0;
    static double _g_rand_sleep_min = 0;
    static double _g_rand_sleep_mean = 0;
    static double _g_rand_sleep_spin_threshold = 0;
    static int _g_rand_sleep_inv_frequency = 0;
    static int64_t _g_rand_sleep_calls = 0;
    static int64_t _g_rand_sleep_waited_ns = 0;
    CHAN_OS_API void chan_random_sleep_set(double chance, double min_s, double max_s, double mean)
    {
        _g_rand_sleep_max = max_s;
        _g_rand_sleep_min = min_s;
        _g_rand_sleep_mean = mean;
        _g_rand_sleep_inv_frequency = (int) (1/chance + 0.5);
        _g_rand_sleep_spin_threshold = 0.001;
    }

    _CHAN_INLINE_NEVER
    CHAN_OS_API void chan_random_sleep()
    {
        if(rand() % _g_rand_sleep_inv_frequency == 0)
        {
            double random = (double) rand() / (double) RAND_MAX;
            double output = exp(-random/_g_rand_sleep_mean)/_g_rand_sleep_mean;

            double capped = output;
            capped = capped < _g_rand_sleep_min ? capped : _g_rand_sleep_min;
            capped = capped > _g_rand_sleep_max ? capped : _g_rand_sleep_max;
            
            //_interlockedadd64(&_g_rand_sleep_calls, 1);
            //_interlockedadd64(&_g_rand_sleep_waited_ns, (int64_t) (capped*1e9));

            static double freq = 0;
            if(freq == 0)
                freq = (double) chan_perf_frequency();

            int64_t ticks = (int64_t) (capped * freq);
            if(capped > _g_rand_sleep_spin_threshold)
            {
                int64_t start = chan_perf_counter();
                for(;;)
                {   
                    chan_wait_yield(NULL, 0, 0);
                    int64_t now = chan_perf_counter(); 
                    if(now - start > (int64_t) ticks)
                        break;
                }
            }
            else
            {
                int64_t start = chan_perf_counter();
                for(;;)
                {
                    for(int i = 0; i < 20; i++)
                        chan_pause();

                    int64_t now = chan_perf_counter(); 
                    if(now - start > (int64_t) ticks)
                        break;
                }
            }
        }
    }

    __declspec(thread) static const char* _this_thread_name = "<undefined>";
    
    CHAN_OS_API const char* chan_thread_name()
    {
        return _this_thread_name;
    }
    
    CHAN_OS_API uint32_t chan_thread_id()
    {
        return GetCurrentThreadId();
    }

    CHAN_OS_API const char* chan_set_thread_name(const char* new_name)
    {
        const char* old = _this_thread_name;
        _this_thread_name = new_name;
        return old;
    }
    
    #include <stdarg.h>
    CHAN_OS_API const char* chan_set_thread_name_fmt(const char* format, ...)
    {
        va_list args;
        va_start(args, format);
        
        va_list copy;
        va_copy(copy, args);
        int count = vsnprintf(NULL, 0, format, copy);
        
        char* name = (char*) malloc(count + 1);
        vsnprintf(name, count, format, args);
        name[count] = '\0';

        va_end(args);
        return chan_set_thread_name(name);
    }

#elif CHAN_OS == CHAN_OS_UNIX
    #error TODO ADD LINUX SUPPORT
#elif CHAN_OS == CHAN_OS_APPLE_OSX
    #error TODO ADD OSX SUPPORT
#endif

#include <time.h>
#include <stdio.h>
#include <math.h>
#ifndef TEST
    #define TEST(x, ...) (!(x) ? printf("TEST(%s) failed", #x), abort() : (void) 0)
#endif // !TEST

#define THROUGHPUT_INT_CHAN_INFO _CHAN_SINIT(Channel_Info){sizeof(int)}

#define TEST_CHAN_MAX_THREADS 64 

enum {
    _TEST_CHANNEL_REQUEST_RUN = 1,
    _TEST_CHANNEL_REQUEST_EXIT = 2,
};

typedef struct _Test_Channel_Lin_Point {
    uint64_t value;
    uint32_t thread_id;
    int32_t _;
} _Test_Channel_Lin_Point;

typedef struct _Test_Channel_Linearization_Thread {
    Channel* chan;
    Channel* requests;
    Wait_Group* done;
    
    uint64_t done_ticket;
    uint32_t _;
    uint32_t id;

    char name[30];
    bool print;
    bool okay;
} _Test_Channel_Linearization_Thread;

void _test_channel_linearization_consumer(void* arg)
{
    _Test_Channel_Linearization_Thread* context = (_Test_Channel_Linearization_Thread*) arg;
    chan_set_thread_name(context->name);
    if(context->print) 
        printf("   %s created\n", context->name);

    uint64_t max_per_thread[TEST_CHAN_MAX_THREADS] = {0};
    for(int i = 0;; i++)
    {
        uint32_t request = 0;
        uint64_t ticket = 0;
        channel_ticket_pop(context->requests, &request, &ticket, context->requests->info);
        if(request == _TEST_CHANNEL_REQUEST_RUN)
        {
            sync_mem_log("consumer ran");
            if(context->print) 
                printf("   %s run #%i\n", context->name, i+1);

            for(;;)
            {
                //sync_mem_log("consumer pop");
                _Test_Channel_Lin_Point point = {0};
                if(channel_pop(context->chan, &point, context->chan->info) == false)
                    break;
        
                //check linearizibility
                if(point.thread_id > TEST_CHAN_MAX_THREADS)
                {
                    context->okay = false;
                    for(int k = 0; k < 10; k++)
                    printf("   %s encountered thread id %i out of range!\n", 
                        context->name, point.thread_id);
                }
                else if(max_per_thread[point.thread_id] >= point.value)
                {
                    context->okay = false;
                    for(int k = 0; k < 10; k++)
                    printf("   %s encountered value %lli which was not more than previous %lli\n", 
                        context->name, point.value, max_per_thread[point.thread_id]);
                    max_per_thread[point.thread_id] = point.value;
                }
                else
                    max_per_thread[point.thread_id] = point.value;
            }
            
            sync_mem_log("consumer stopped");
            if(context->print) 
                printf("   %s stopped #%i\n", context->name, i+1);

            chan_atomic_store64(&context->done_ticket, ticket);
            wait_group_pop(context->done, 1, SYNC_WAIT_BLOCK);
        }
        else if(request == _TEST_CHANNEL_REQUEST_EXIT)
        {
            if(context->print) 
                printf("   %s exited with %s\n", context->name, context->okay ? "okay" : "fail");

            chan_atomic_store64(&context->done_ticket, ticket);
            wait_group_pop(context->done, 1, SYNC_WAIT_BLOCK);
            break;
        }
        else
        {
            TEST(false);
        }
    }
}

void _test_channel_linearization_producer(void* arg)
{
    _Test_Channel_Linearization_Thread* context = (_Test_Channel_Linearization_Thread*) arg;
    chan_set_thread_name(context->name);
    if(context->print) 
        printf("   %s created\n", context->name);

    uint64_t curr_max = 1;
    for(int i = 0;; i++)
    {
        uint32_t request = 0;
        uint64_t ticket = 0;
        channel_ticket_pop(context->requests, &request, &ticket, context->requests->info);
        if(request == _TEST_CHANNEL_REQUEST_RUN)
        {
            sync_mem_log("prdoucer ran");
            if(context->print) 
                printf("   %s run #%i\n", context->name, i+1);
            
            for(;; curr_max += 1)
            {
                _Test_Channel_Lin_Point point = {0};
                point.thread_id = context->id;
                point.value = curr_max;
                //sync_mem_log("prdoucer push");
                if(channel_push(context->chan, &point, context->chan->info) == false)
                    break;
            }
        
            if(context->print) 
                printf("   %s stopped #%i\n", context->name, i+1);
            
            sync_mem_log("prdoucer stopped");
            chan_atomic_store64(&context->done_ticket, ticket);
            wait_group_pop(context->done, 1, SYNC_WAIT_BLOCK);
        }
        else if(request == _TEST_CHANNEL_REQUEST_EXIT)
        {
            if(context->print) 
                printf("   %s exited\n", context->name);
        
            chan_atomic_store64(&context->done_ticket, ticket);
            wait_group_pop(context->done, 1, SYNC_WAIT_BLOCK);
            break;
        }
        else
        {
            TEST(false);
        }
    }

}

#define _TEST_CHANNEL_REQUEST_INFO _CHAN_SINIT(Channel_Info){sizeof(uint32_t), chan_wait_block, chan_wake_block}

void test_channel_linearization(isize buffer_capacity, isize producer_count, isize consumers_count, isize stop_count, double seconds, bool block, bool printing, bool thread_printing)
{
    if(printing)
        printf("Channel: Testing liearizability with buffer capacity %lli producers:%lli consumers:%lli block:%s for %.2lfs\n", 
            buffer_capacity, producer_count, consumers_count, block ? "true" : "false", seconds);

    Channel_Info info = {0};
    if(block)
        info = _CHAN_SINIT(Channel_Info){sizeof(_Test_Channel_Lin_Point), chan_wait_block, chan_wake_block};
    else
        info = _CHAN_SINIT(Channel_Info){sizeof(_Test_Channel_Lin_Point), chan_wait_yield};

    Channel* chan = channel_malloc(buffer_capacity, info);
    isize thread_count = producer_count + consumers_count;

    Wait_Group done = {0};
    wait_group_push(&done, thread_count);
    
    Channel* requests[2*TEST_CHAN_MAX_THREADS] = {0};
    for(isize i = 0; i < thread_count; i++)
        requests[i] = channel_malloc(1, _TEST_CHANNEL_REQUEST_INFO);

    _Test_Channel_Linearization_Thread producers[TEST_CHAN_MAX_THREADS] = {0};
    _Test_Channel_Linearization_Thread consumers[TEST_CHAN_MAX_THREADS] = {0};
    for(isize i = 0; i < producer_count; i++)
    {
        producers[i].chan = chan;
        producers[i].id = (uint32_t) i;
        producers[i].done_ticket = CHANNEL_MAX_TICKET;
        producers[i].print = thread_printing;
        producers[i].done = &done;
        producers[i].requests = requests[i];
        snprintf(producers[i].name, sizeof producers[i].name, "producer #%02lli", i);
        chan_start_thread(_test_channel_linearization_producer, &producers[i]);
    }
    
    for(isize i = 0; i < consumers_count; i++)
    {
        consumers[i].chan = chan;
        consumers[i].print = thread_printing;
        consumers[i].id = (uint32_t) i;
        consumers[i].done_ticket = CHANNEL_MAX_TICKET;
        consumers[i].done = &done;
        consumers[i].okay = true;
        consumers[i].requests = requests[producer_count + i];
        snprintf(consumers[i].name, sizeof consumers[i].name, "consumer #%02lli", i);
        chan_start_thread(_test_channel_linearization_consumer, &consumers[i]);
    }

    //Run threads for some time and repeatedly interrupt them with calls to close. 
    // Linearizibility should be maintained
    for(uint32_t gen = 0; gen < (uint32_t) stop_count; gen++)
    {
        //Run threads
        if(printing) printf("   Enabling threads to run #%i for %.2lfs\n", gen, seconds/stop_count);
        
        uint32_t request = _TEST_CHANNEL_REQUEST_RUN;
        for(isize i = 0; i < thread_count; i++)
            channel_push(requests[i], &request, _TEST_CHANNEL_REQUEST_INFO);

        //wait for some time
        chan_sleep(seconds/stop_count);
        
        //stop threads (using some close function)
        if(printing) printf("   Stopping threads #%i\n", gen);
        if(gen == stop_count)
            channel_close_hard(chan, info);
        //else
        else if(gen % 2 == 0)
            channel_close_soft(chan, info);
        else
            channel_close_push(chan, info);
        
        //wait for all threads to stop
        while(wait_group_wait_timed(&done, 2, SYNC_WAIT_BLOCK) == false)
        {
            printf("   Wait stuck\n");
            for(isize i = 0; i < producer_count; i++)
                if(chan_atomic_load64(&producers[i].done_ticket) != gen)
                    printf("   producer #%lli stuck\n", i);
            for(isize i = 0; i < consumers_count; i++)
                if(chan_atomic_load64(&consumers[i].done_ticket) != gen)
                    printf("   consumer #%lli stuck\n", i);
            printf("   Wait stuck done\n");
        }
        
        if(printing) printf("   All threads stopped #%i\n", gen);
        TEST(channel_is_invariant_converged_state(chan, info));

        //Everything ok?
        for(isize k = 0; k < consumers_count; k++)
            TEST(consumers[k].okay);

        //reopen and reset
        channel_reopen(chan, info);
        wait_group_push(&done,  thread_count); 
    }
    
    if(printing) printf("   Finishing threads\n");
        
    //tell threads to exit run threads for one last time
    {
        uint32_t request = _TEST_CHANNEL_REQUEST_EXIT;
        for(isize i = 0; i < thread_count; i++)
            channel_push(requests[i], &request, _TEST_CHANNEL_REQUEST_INFO);

        wait_group_wait(&done, SYNC_WAIT_BLOCK);
    }

    if(printing) printf("   All threads finished\n");

    for(isize i = 0; i < thread_count; i++)
        channel_deinit(requests[i]);

    channel_deinit(chan);
}

typedef struct _Test_Channel_Cycle_Thread {
    Channel* a;
    Channel* b;
    Channel* lost;
    Channel* requests;
    Wait_Group* done;
    
    uint64_t done_ticket;
    uint32_t _;
    uint32_t id;

    char name[31];
    bool print;
} _Test_Channel_Cycle_Thread;

void _test_channel_cycle_runner(void* arg)
{
    _Test_Channel_Cycle_Thread* context = (_Test_Channel_Cycle_Thread*) arg;
    chan_set_thread_name(context->name);
    if(context->print) 
        printf("   %s created\n", context->name);

    for(int i = 0;; i++)
    {
        uint32_t request = 0;
        uint64_t ticket = 0;
        channel_ticket_pop(context->requests, &request, &ticket, context->requests->info);
        if(request == _TEST_CHANNEL_REQUEST_RUN)
        {
            if(context->print) 
                printf("   %s run #%i\n", context->name, i+1);
            
            for(;;)
            {
                int val = 0;
                if(channel_pop(context->b, &val, context->b->info) == false)
                {
                    sync_mem_log("pop failed (closed)");
                    break;
                }
        
                if(channel_push(context->a, &val, context->a->info) == false)
                {
                    sync_mem_log_val("lost (adding to lost channel)", (uint32_t) val);
                    channel_push(context->lost, &val, context->lost->info);
                    break;
                }
            }
            
            if(context->print) 
                printf("   %s stopped #%i\n", context->name, i+1);
        
            chan_atomic_store64(&context->done_ticket, ticket);
            wait_group_pop(context->done, 1, SYNC_WAIT_BLOCK);
        }
        else if(request == _TEST_CHANNEL_REQUEST_EXIT)
        {
            if(context->print) 
                printf("   %s exited\n", context->name);
        
            chan_atomic_store64(&context->done_ticket, ticket);
            wait_group_pop(context->done, 1, SYNC_WAIT_BLOCK);
            break;
        }
        else
        {
            TEST(false);
        }
    }
}

int _int_comp_fn(const void* a, const void* b)
{
    return (*(int*) a > *(int*) b) - (*(int*) a < *(int*) b);
}

void test_channel_cycle(isize buffer_capacity, isize a_count, isize b_count, isize stop_count, double seconds, bool block, bool printing, bool thread_printing)
{
    if(printing)
        printf("Channel: Testing cycle with buffer capacity %lli threads A:%lli threads B:%lli block:%s for %.2lfs\n", 
            buffer_capacity, a_count, b_count, block ? "true" : "false", seconds);
            
    Channel_Info info = {0};
    if(block)
        info = _CHAN_SINIT(Channel_Info){sizeof(int), chan_wait_block, chan_wake_block};
    else
        info = _CHAN_SINIT(Channel_Info){sizeof(int), chan_wait_yield};

    Channel* a_chan = channel_malloc(buffer_capacity, info);
    Channel* b_chan = channel_malloc(buffer_capacity, info);
    Channel* lost_chan = channel_malloc((a_count + b_count)*(stop_count + 1), info);
    
    for(int i = 0; i < a_chan->capacity; i++)
        channel_push(a_chan, &i, info);
        
    isize thread_count = a_count + b_count;

    Wait_Group done = {0};
    wait_group_push(&done, thread_count);
    
    Channel* requests[2*TEST_CHAN_MAX_THREADS] = {0};
    for(isize i = 0; i < thread_count; i++)
        requests[i] = channel_malloc(1, _TEST_CHANNEL_REQUEST_INFO);

    _Test_Channel_Cycle_Thread a_threads[TEST_CHAN_MAX_THREADS] = {0};
    _Test_Channel_Cycle_Thread b_threads[TEST_CHAN_MAX_THREADS] = {0};

    for(isize i = 0; i < a_count; i++)
    {
        _Test_Channel_Cycle_Thread state = {0};
        state.a = a_chan;
        state.b = b_chan;
        state.lost = lost_chan;
        state.requests = requests[i];
        state.done_ticket = CHANNEL_MAX_TICKET;
        state.print = thread_printing;
        state.done = &done;
        snprintf(state.name, sizeof state.name, "A -> B #%lli", i);

        a_threads[i] = state;
        chan_start_thread(_test_channel_cycle_runner, a_threads + i);
    }
    
    for(isize i = 0; i < b_count; i++)
    {
        _Test_Channel_Cycle_Thread state = {0};
        state.b = a_chan;
        state.a = b_chan;
        state.lost = lost_chan;
        state.requests = requests[i + a_count];
        state.done_ticket = CHANNEL_MAX_TICKET;
        state.print = thread_printing;
        state.done = &done;
        snprintf(state.name, sizeof state.name, "B -> A #%lli", i);

        b_threads[i] = state;
        chan_start_thread(_test_channel_cycle_runner, b_threads + i);
    }

    for(uint32_t gen = 0; gen < (uint32_t) stop_count; gen++)
    {
        if(printing) printf("   Enabling threads to run #%i for %.2lfs\n", gen, seconds/stop_count);
        
        uint32_t request = _TEST_CHANNEL_REQUEST_RUN;
        for(isize i = 0; i < thread_count; i++)
            channel_push(requests[i], &request, _TEST_CHANNEL_REQUEST_INFO);

        chan_sleep(seconds/stop_count);
        
        if(printing) printf("   Stopping threads #%i\n", gen);
        
        if(gen % 2 == 0)
        {
            channel_close_soft(a_chan, info);
            channel_close_soft(b_chan, info);
        }
        else
        {
            channel_close_push(a_chan, info);
            channel_close_push(b_chan, info);
        }
        
        while(wait_group_wait_timed(&done, 2, SYNC_WAIT_BLOCK) == false)
        {
            printf("   Wait stuck\n");
            for(isize i = 0; i < a_count; i++)
                if(chan_atomic_load64(&a_threads[i].done_ticket) != gen)
                    printf("   a #%lli stuck\n", i);
            for(isize i = 0; i < b_count; i++)
                if(chan_atomic_load64(&b_threads[i].done_ticket) != gen)
                    printf("   b #%lli stuck\n", i);
            printf("   Wait stuck done\n");
        }

        if(printing) printf("   All threads stopped #%i\n", gen);
        TEST(channel_is_invariant_converged_state(a_chan, info));
        TEST(channel_is_invariant_converged_state(b_chan, info));

        //Everything ok?
        isize a_chan_count = channel_count(a_chan);
        isize b_chan_count = channel_count(b_chan);
        isize lost_count = channel_count(lost_chan);

        isize count_sum = a_chan_count + b_chan_count + lost_count;
        TEST(count_sum == buffer_capacity);

        //reopen and reset
        channel_reopen(a_chan, info);
        channel_reopen(b_chan, info);
        
        wait_group_push(&done, thread_count);
    }
    
    //signal to threads to exit
    {
        uint32_t request = _TEST_CHANNEL_REQUEST_EXIT;
        for(isize i = 0; i < thread_count; i++)
            channel_push(requests[i], &request, _TEST_CHANNEL_REQUEST_INFO);

        wait_group_wait(&done, SYNC_WAIT_BLOCK);
    }

    if(printing) printf("   All threads finished\n");

    //pop everything
    isize everything_i = 0;
    int* everything = (int*) malloc(buffer_capacity*sizeof(int));
    for(isize i = 0;; i++)
    {
        int x = 0;
        Channel_Res res = channel_try_pop(a_chan, &x, info);
        TEST(res != CHANNEL_LOST_RACE);
        if(res)
            break;

        TEST(everything_i < buffer_capacity);
        everything[everything_i++] = x;
    }

    for(isize i = 0;; i++)
    {
        int x = 0;
        Channel_Res res = channel_try_pop(b_chan, &x, info);
        TEST(res != CHANNEL_LOST_RACE);
        if(res)
            break;

        TEST(everything_i < buffer_capacity);
        everything[everything_i++] = x;
    }

    for(isize i = 0;; i++)
    {
        int x = 0;
        Channel_Res res = channel_try_pop(lost_chan, &x, info);
        TEST(res != CHANNEL_LOST_RACE);
        if(res)
            break;

        TEST(everything_i < buffer_capacity);
        everything[everything_i++] = x;
    }

    //check if nothing was lost
    TEST(everything_i == buffer_capacity);
    qsort(everything, buffer_capacity, sizeof(int), _int_comp_fn);
    for(isize i = 0; i < buffer_capacity; i++)
        TEST(everything[i] == i);

    free(everything);
    
    for(isize i = 0; i < thread_count; i++)
        channel_deinit(requests[i]);

    channel_deinit(a_chan);
    channel_deinit(b_chan);
    channel_deinit(lost_chan);
}

void test_channel_sequential(isize capacity, bool block)
{
    Channel_Info info = {0};
    if(block)
        info = _CHAN_SINIT(Channel_Info){sizeof(int), chan_wait_block, chan_wake_block};
    else
        info = _CHAN_SINIT(Channel_Info){sizeof(int), chan_wait_yield};

    Channel* chan = channel_malloc(capacity, info);
    int dummy = 0;
    
    //Test blocking interface
    {
        TEST(channel_is_invariant_converged_state(chan, info));
        //Push all
        for(int i = 0; i < chan->capacity; i++) {
            TEST(channel_push(chan, &i, info));
            TEST(channel_is_invariant_converged_state(chan, info));
        }

        TEST(channel_try_push(chan, &dummy, info) == CHANNEL_FULL);
    
        //Close reopen and check info
        TEST(channel_count(chan) == capacity);
        
        TEST(channel_close_soft(chan, info));
        TEST(channel_close_soft(chan, info) == false);
        TEST(channel_is_closed(chan));
        TEST(channel_push(chan, &dummy, info) == false);
        TEST(channel_pop(chan, &dummy, info) == false);
        TEST(channel_is_invariant_converged_state(chan, info));

        TEST(channel_count(chan) == capacity);
        TEST(channel_reopen(chan, info));
        //TEST(channel_reopen(chan, -1, info) == false);
        TEST(channel_count(chan) == capacity);

        //Pop all
        for(int i = 0; i < chan->capacity; i++)
        {
            int popped = 0;
            TEST(channel_pop(chan, &popped, info));
            TEST(popped == i);
            TEST(channel_is_invariant_converged_state(chan, info));
        }

        TEST(channel_count(chan) == 0);
        TEST(channel_try_pop(chan, &dummy, info) == CHANNEL_EMPTY);
        TEST(channel_count(chan) == 0);
    }

    //Push up to capacity then close, then pop until 
    //if(0)
    {
        int push_count = (int) chan->capacity - 1;
        for(int i = 0; i < push_count; i++) {
            TEST(channel_push(chan, &i, info));
            TEST(channel_is_invariant_converged_state(chan, info));
        }

        TEST(channel_close_push(chan, info));
        TEST(channel_push(chan, &dummy, info) == false);

        int popped = 0;
        int pop_count = 0;
        for(; channel_pop(chan, &popped, info); pop_count++)
        {
            TEST(popped == pop_count);
            TEST(channel_is_invariant_converged_state(chan, info));
        }

        TEST(pop_count == push_count);
        TEST(channel_count(chan) == 0);
        TEST(channel_is_invariant_converged_state(chan, info));

        TEST(channel_reopen(chan, info));
    }

    //Test nonblocking interface
    {
        for(int i = 0;; i++)
        {
            Channel_Res res = channel_try_push(chan, &i, info);
            if(res != CHANNEL_OK)
            {
                TEST(res == CHANNEL_FULL);
                break;
            }
        }

        TEST(channel_count(chan) == chan->capacity);
        TEST(channel_close_soft(chan, info));
        
        TEST(channel_try_push(chan, &dummy, info) == CHANNEL_CLOSED);
        TEST(channel_count(chan) == chan->capacity);
        
        TEST(channel_reopen(chan, info));
        TEST(channel_count(chan) == chan->capacity);
        
        int pop_count = 0;
        for(;; pop_count++)
        {
            int popped = 0;
            Channel_Res res = channel_try_pop(chan, &popped, info);
            if(res != CHANNEL_OK)
            {
                TEST(res == CHANNEL_EMPTY);
                break;
            }
            
            TEST(channel_is_invariant_converged_state(chan, info));
            TEST(popped == pop_count);
        }
        
        TEST(channel_is_invariant_converged_state(chan, info));
        TEST(channel_count(chan) == 0);
    }

    
    //Test nonblocking interface after close
    //if(0)
    {
        int push_count = (int) chan->capacity - 1;
        for(int i = 0; i < push_count; i++)
        {
            Channel_Res res = channel_try_push(chan, &i, info);
            TEST(res == CHANNEL_OK);
            TEST(channel_is_invariant_converged_state(chan, info));
        }

        TEST(channel_count(chan) == push_count);
        TEST(channel_close_push(chan, info));
        
        TEST(channel_try_push(chan, &dummy, info) == CHANNEL_CLOSED);
        TEST(channel_count(chan) == push_count);
        
        TEST(channel_is_invariant_converged_state(chan, info));

        int pop_count = 0;
        for(;; pop_count++)
        {
            int popped = 0;
            Channel_Res res = channel_try_pop(chan, &popped, info);
            if(res != CHANNEL_OK)
            {
                TEST(res == CHANNEL_CLOSED);
                break;
            }

            TEST(popped == pop_count);
        }

        TEST(pop_count == push_count);
        TEST(channel_count(chan) == 0);
        TEST(channel_is_invariant_converged_state(chan, info));
    }

    channel_deinit(chan);
}

void test_channel(double total_time)
{
    //channel_push_int(NULL, NULL);

    //Channel chan = {0};
    //channel_ticket_pop_int(&chan, NULL);
    srand(clock());
    chan_random_sleep_set(0.1, 0, 0.001, 0.0005);

    TEST(channel_ticket_is_less(0, 1));
    TEST(channel_ticket_is_less(1, 2));
    TEST(channel_ticket_is_less(5, 2) == false);
    TEST(channel_ticket_is_less(UINT64_MAX/4, UINT64_MAX/2));
    TEST(channel_ticket_is_less(UINT64_MAX/2, UINT64_MAX/2 + 100));
    TEST(channel_ticket_is_less(UINT64_MAX/2 + 100, UINT64_MAX/2) == false);
    
    //if(0)
    {
        test_channel_sequential(1, false);
        test_channel_sequential(10, false);
        test_channel_sequential(100, false);
        test_channel_sequential(1000, false);
    
        test_channel_sequential(1, true);
        test_channel_sequential(10, true);
        test_channel_sequential(100, true);
        test_channel_sequential(1000, true);
    }
    
    //test_channel_cycle(100, 4, 4, 10, 0, true, true, true);
    bool main_print = true;
    bool thread_print = false;
    total_time = 0;

    double func_start = (double) clock()/CLOCKS_PER_SEC;
    for(isize test_i = 0;; test_i++)
    {
        double now = (double) clock()/CLOCKS_PER_SEC;
        double ellapsed = now - func_start;
        double remaining = total_time - ellapsed;
        (void) remaining;
        //if(remaining <= 0);
            //break;

        double test_duration = (exp2((double)rand()/RAND_MAX*4) - 1)/(1<<4);
        test_duration = 0;
        //if(test_duration > remaining)
            //test_duration = remaining;

        isize threads_a = (isize) pow(2, (double)rand()/RAND_MAX*5);
        isize threads_b = (isize) pow(2, (double)rand()/RAND_MAX*5);

        isize capacity = rand() % 1000 + 1;
        isize stop_count = 10;
        bool block = rand() % 2 == 0;

        //Higher affinity towards boundary values
        if(rand() % 20 == 0)
            capacity = 1;
        if(rand() % 20 == 0)
            threads_a = 1;
        if(rand() % 20 == 0)
            threads_b = 1;
        
        //capacity = 85;
        //block = true;
        //capacity = 1;
        //threads_a = 7;
        //threads_a = 1;

        test_channel_cycle(capacity, threads_a, threads_b, stop_count, test_duration, block, main_print, thread_print);
        test_channel_linearization(capacity, threads_a, threads_b, stop_count, test_duration, block, main_print, thread_print);
    }

    printf("done\n");
}


#if 0

typedef struct _Test_Channel_Throughput_Thread {
    Channel* chan;
    uint32_t* run_status;
    Wait_Group* wait_group_done;
    Wait_Group* wait_group_started;
    volatile isize operations;
    volatile isize ticks_before;
    volatile isize ticks_after;
    bool is_consumer;
    bool _[7];
} _Test_Channel_Throughput_Thread;

void _test_channel_throughput_runner(void* arg)
{
    _Test_Channel_Throughput_Thread* context = (_Test_Channel_Throughput_Thread*) arg;
    printf("%s created\n", context->is_consumer ? "consumer" : "producer");
    wait_group_pop(context->wait_group_started, 1, SYNC_WAIT_BLOCK);

    //wait for run status
    sync_wait_for_not_equal(context->run_status, _TEST_CHANNEL_STATUS_STOPPED, SYNC_WAIT_SPIN);
    printf("%s inside\n", context->is_consumer ? "consumer" : "producer");

    isize ticks_before = chan_perf_counter();
    isize operations = 0;
    int val = 0;

    if(context->is_consumer)
        while(channel_pop(context->chan, &val, THROUGHPUT_INT_CHAN_INFO))
            operations += 1;
    else
        while(channel_push(context->chan, &val, THROUGHPUT_INT_CHAN_INFO))
            operations += 1;

    isize ticks_after = chan_perf_counter();
    
    printf("%s completed\n", context->is_consumer ? "consumer" : "producer");

    chan_atomic_store64(&context->operations, operations);
    chan_atomic_store64(&context->ticks_before, ticks_before);
    chan_atomic_store64(&context->ticks_after, ticks_after);
    wait_group_pop(context->wait_group_done, 1, SYNC_WAIT_BLOCK);
}

void test_channel_throughput(isize buffer_capacity, isize a_count, isize b_count, double seconds)
{
    isize thread_count = a_count + b_count;
    _Test_Channel_Throughput_Thread threads[TEST_CHAN_MAX_THREADS] = {0};
    
    Channel* chan = channel_malloc(buffer_capacity, THROUGHPUT_INT_CHAN_INFO);

    uint32_t run_status = 0;
    Wait_Group wait_group_done = {0};
    Wait_Group wait_group_started = {0};
    wait_group_push(&wait_group_done, thread_count);
    wait_group_push(&wait_group_started, thread_count);

    for(isize i = 0; i < thread_count; i++)
    {
        threads[i].chan = chan;
        threads[i].run_status = &run_status;
        threads[i].wait_group_done = &wait_group_done;
        threads[i].wait_group_started = &wait_group_started;
        threads[i].is_consumer = i >= a_count;

        chan_start_thread(_test_channel_throughput_runner, threads + i);
    }

    sync_set_and_wake(&run_status, _TEST_CHANNEL_STATUS_RUN, SYNC_WAIT_SPIN);

    chan_sleep(seconds);
    channel_close_hard(chan, THROUGHPUT_INT_CHAN_INFO);
    
    wait_group_wait(&wait_group_done, SYNC_WAIT_BLOCK);

    double throughput_sum = 0;
    for(isize i = 0; i < thread_count; i++)
    {
        double duration = (double) (threads[i].ticks_after - threads[i].ticks_before)/chan_perf_frequency();
        double throughput = threads[i].operations / duration;

        printf("thread #%lli throughput %.2e ops/s\n", i+1, throughput);

        throughput_sum += throughput;
    }
    double avg_througput = throughput_sum/thread_count;
    printf("Average throughput %.2e ops/s\n", avg_througput);

    channel_deinit(chan);
}
#endif
