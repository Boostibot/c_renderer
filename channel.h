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
// A fairly faithful implementation of the linearizable blocking concurent queue 
// described in "T. R. W. Scogland - Design and Evaluation of Scalable Concurrent Queues for Many-Core Architectures, 2015" 
// which can be found at https://synergy.cs.vt.edu/pubs/papers/scogland-queues-icpe15.pdf
// One major addition is that we extend this to allow proper futex based blocking. I highly recommend reading the article.
// 
// The channel acts pretty much as a Go buffered channel augmented with additional
// non-blocking and ticket interfaces. These allow us to for example only push if the
// channel is not full or wait for item to be processed.
//
// The basic idea is to do a very fine grained locking: each item in the channel has a dedicated
// ticket lock. On push/pop we perform atomic fetch and add (FAA) one to the tail/head indices, which yields
// a number used to calculate our slot and operation id. This slot is potentially shared with other 
// pushes or pops because the queue has finite capcity. We go to that slot and wait on its ticket lock 
// to signal id corresponding to this push/pop opperation. Only then we push/pop the item then advance 
// the ticket lock, allowing a next operation on that slot to proceed.
// 
// This procedure means that unless the queue is full/empty a single push/pop contains only 
// one atomic FAA on the critical path (ticket locks are uncontested), resulting in extremely
// high throughput pretty much only limited by the FAA performance. Modern CPUs can perform around
// 700 million atomic FAA per second so we expect the throughput of the queue to be similar.
// 

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
    volatile uint64_t ticket_head;
    uint64_t _head_pad[6];

    _CHAN_ALIGNED(CHAN_CACHE_LINE) 
    volatile uint64_t tail;
    volatile uint64_t ticket_tail;
    uint64_t _tail_pad[6];

    _CHAN_ALIGNED(CHAN_CACHE_LINE) 
    Channel_Info info;
    isize capacity; 
    uint8_t* items; 
    uint32_t* ids; 
    volatile uint32_t ref_count; 
    volatile uint32_t closed;
    volatile uint32_t allocated;
    volatile uint32_t closing_lock;
    volatile uint64_t barrier;
    uint8_t _shared_pad[56];
    //uint8_t _shared_pad[4];
} Channel;

typedef enum Channel_Res {
    CHANNEL_OK = 0,
    CHANNEL_CLOSED = 1,
    CHANNEL_LOST_RACE = 2,
    CHANNEL_FULL = 3,
    CHANNEL_EMPTY = 4,
} Channel_Res;

//Allocates a new channel on the heap and returns pointer to it. If the allocation fails returns 0.
//capacity >= 0. If capacity == 0 then creates an "unbuffered" channel which acts as unbuffered channel in Go.
// That is there is just one slot in the channel and after a call to channel_push the calling thread is
// waiting until channel_pop is called (or until the channel is closed).
CHANAPI Channel* channel_init(isize capacity, Channel_Info info);

//Increments the ref count of the channel. Returns the passed in channel.
CHANAPI Channel* channel_share(Channel* chan);

//Decrements the ref count and if it reaches zero deinitializes the channel. 
//If the channel was allocated through channel_init frees it.
//If it was created through channel_init_custom only memsets the channel to zero.
CHANAPI int32_t channel_deinit(Channel* chan);
//Calls channel_deinit then channel_close
CHANAPI int32_t channel_deinit_close(Channel* chan);
//Waits for the channels ref count to hit 1.
CHANAPI void channel_wait_for_exclusivity(const Channel* chan, Channel_Info info);

//Initializes the channel using the passed in arguments. 
// This can be used to allocate a channel on the stack or use a different allocator such as an arena.
// The items need to point to at least capacity*info.item_size bytes (info.item_size can be zero in which case items can be NULL). 
// The ids need to point to an array of at least capacity uint64_t's.
CHANAPI void channel_init_custom(Channel* chan, void* items, uint32_t* ids, isize capacity, Channel_Info info);

//Pushes an item. item needs to point to an array of at least info.item_size bytes.
//If the operation succeeds returns true.
//If the channel is full, waits until its not. 
//If the channel is closed or becomes closed while waiting returns false.
CHANAPI bool channel_push(Channel* chan, const void* item, Channel_Info info);
//Pushes an item. item needs to point to an array of at least info.item_size bytes.
//If the operation succeeds returns true.
//If the channel is empty, waits until its not. 
//If the channel is closed or becomes closed while waiting returns false.
CHANAPI bool channel_pop(Channel* chan, void* item, Channel_Info info);

//Attempts to push an item stored in item without blocking. 
// item needs to point to an array of at least info.item_size bytes.
// If succeeds returns CHANNEL_OK (0). 
// If the channel is closed returns CHANNEL_CLOSED
// If the channel is full returns CHANNEL_FULL
// If lost a race to concurrent call to this function returns CHANNEL_LOST_RACE.
CHANAPI Channel_Res channel_try_push_weak(Channel* chan, const void* item, Channel_Info info);
//Attempts to pop an item storing it in item without blocking. 
// item needs to point to an array of at least info.item_size bytes.
// If succeeds returns CHANNEL_OK (0). 
// If the channel is closed returns CHANNEL_CLOSED
// If the channel is empty returns CHANNEL_FULL
// If lost a race to concurrent call to this function returns CHANNEL_LOST_RACE.
CHANAPI Channel_Res channel_try_pop_weak(Channel* chan, void* item, Channel_Info info); 

//Same as channel_try_push_weak but never returns CHANNEL_LOST_RACE (2).
//Instead retries until the operation completes successfully or some other error appears.
CHANAPI Channel_Res channel_try_push(Channel* chan, const void* item, Channel_Info info);
//Same as channel_try_pop_weak but never returns CHANNEL_LOST_RACE (2).
//Instead retries until the operation completes successfully or some other error appears.
CHANAPI Channel_Res channel_try_pop(Channel* chan, void* item, Channel_Info info);

//Closes the channel causing all threads waiting in channel_push and channel_pop to return with failure (false). 
//Subsequent operations on the channel will fail immediately.
//Returns whether the channel was already closed.
CHANAPI bool channel_close(Channel* chan, Channel_Info info);
//Reopens an already closed channel, making it possible to use it again.
// If sort_items is true sorts the items according to their tickets, thus perserving linearizability.
// If you just care about the items and order is not of too high importance you are advised to leave it false.
// Tickets from before before the call to channel_close() stop being valid after call to channel_reopen()!
// Note that this function is unsafe and must only be called when we are sure we are the only thread
// still referencing the channel. Failure to do so can result in items in the channel being
// skipped and other items being processed twice.
//Returns whether the channel was still open.
CHANAPI bool channel_reopen(Channel* chan, Channel_Info info);
CHANAPI bool channel_reopen_unordered(Channel* chan, Channel_Info info);
CHANAPI bool channel_is_closed(const Channel* chan); 

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
// it uses these functions to also obtain a ticket. The consumer also pops and takes and recieves a ticket. 
// After each processed item it sets its ticket to global variable. The producer thus simply waits for the 
// the global variable to becomes at least its ticket using channel_ticket_wait_for().

//Returns whether ticket_a came before ticket_b. 
//Unless unsigned number overflow happens this is just `ticket_a < ticket_b`.
CHANAPI bool channel_ticket_is_less(uint64_t ticket_a, uint64_t ticket_b);

CHANAPI bool channel_ticket_push(Channel* chan, const void* item, uint64_t* ticket_or_null, Channel_Info info);
CHANAPI bool channel_ticket_pop(Channel* chan, void* item, uint64_t* ticket_or_null, Channel_Info info);
CHANAPI Channel_Res channel_ticket_try_push(Channel* chan, const void* item, uint64_t* ticket_or_null, Channel_Info info);
CHANAPI Channel_Res channel_ticket_try_pop(Channel* chan, void* item, uint64_t* ticket_or_null, Channel_Info info);
CHANAPI Channel_Res channel_ticket_try_push_weak(Channel* chan, const void* item, uint64_t* ticket_or_null, Channel_Info info);
CHANAPI Channel_Res channel_ticket_try_pop_weak(Channel* chan, void* item, uint64_t* ticket_or_null, Channel_Info info);

//==========================================================================
// Wait free list 
//==========================================================================
// A simple growing stack. This is a lot of the time all that is required for thread comunication. 
// No ABA problem can occur. 
// Before call to sync_list_push() the "pusher" thread has exclusive ownership over the node. 
// After push it must no longer touch it.
// Once a node is popped using sync_list_pop_all() the "popper" thread has exlcusive ownership of that node
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
// Provide helpers for waiting for certain condtion. 
// This can be used to implement wait groups, semaphores and much more.
// Alternatively there is also timed version which gives up after certain 
// ammount of time and returns failure.
typedef struct Sync_Wait {
    Sync_Wait_Func wait;
    Sync_Wake_Func wake;
    uint32_t notify_bit;
    uint32_t _;
} Sync_Wait;

CHANAPI bool sync_wait(volatile void* state, uint32_t current, isize timeout, Sync_Wait wait);
CHANAPI void sync_wake(volatile void* state, uint32_t prev, Sync_Wait wait);
CHANAPI uint32_t sync_wait_for_equal(volatile void* state, uint32_t desired, Sync_Wait wait);
CHANAPI uint32_t sync_wait_for_smaller(volatile void* state, uint32_t desired, Sync_Wait wait);

typedef struct Sync_Timed_Wait {
    double freq_s;
    isize freq_ms;
    isize wait_ticks;
    isize start_ticks;
} Sync_Timed_Wait;

CHANAPI Sync_Timed_Wait sync_timed_wait_start(double wait);
CHANAPI bool sync_timed_wait(volatile void* state, uint32_t current, Sync_Timed_Wait timeout, Sync_Wait wait);
CHANAPI bool sync_timed_wait_for_equal(volatile void* state, uint32_t desired, double timeout, Sync_Wait wait);
CHANAPI bool sync_timed_wait_for_smaller(volatile void* state, uint32_t desired, double timeout, Sync_Wait wait);

typedef uint32_t Sync_Once; 


//==========================================================================
// Wait Group
//==========================================================================
// Acts precisely as Go
typedef int32_t Wait_Group; 

CHANAPI int32_t wait_group_count(volatile Wait_Group* wg);
CHANAPI Wait_Group* wait_group_push(volatile Wait_Group* wg, isize count);
CHANAPI bool wait_group_pop(volatile Wait_Group* wg, isize count, Sync_Wait wait);
CHANAPI void wait_group_wait(volatile Wait_Group* wg, Sync_Wait wait);
CHANAPI bool wait_group_wait_timed(volatile Wait_Group* wg, double timeout, Sync_Wait wait);

//==========================================================================
// Wait/Wake functions
//==========================================================================
//These functions can be used for Sync_Wait_Func/Sync_Wake_Func interafces in the channel
// and sync_wait interfaces.
CHAN_INTRINSIC bool     chan_wait_pause(volatile void* state, uint32_t undesired, isize timeout_or_minus_one_if_infinite);
CHAN_INTRINSIC void     chan_wake_block(volatile void* state);
CHAN_INTRINSIC void     chan_wake_noop(volatile void* state);
CHAN_INTRINSIC bool     chan_wait_noop(volatile void* state, uint32_t undesired, isize timeout_or_minus_one_if_infinite);
CHAN_OS_API bool     chan_wait_block(volatile void* state, uint32_t undesired, isize timeout_or_minus_one_if_infinite);
CHAN_OS_API bool     chan_wait_yield(volatile void* state, uint32_t undesired, isize timeout_or_minus_one_if_infinite);

#ifdef __cplusplus
    #define _CHAN_SINIT(T) T
#else
    #define _CHAN_SINIT(T) (T)
#endif
#define SYNC_WAIT_BLOCK _CHAN_SINIT(Sync_Wait){chan_wait_block, chan_wake_block}
#define SYNC_WAIT_YIELD _CHAN_SINIT(Sync_Wait){chan_wait_yield, chan_wake_noop}
#define SYNC_WAIT_SPIN  _CHAN_SINIT(Sync_Wait){chan_wait_pause, chan_wake_noop}
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

//Os specific
CHAN_OS_API int64_t chan_perf_counter();
CHAN_OS_API int64_t chan_perf_frequency();
CHAN_OS_API void* chan_aligned_alloc(isize bytes, isize aligned);
CHAN_OS_API void chan_aligned_free(void* block, isize aligned);
CHAN_OS_API void chan_start_thread(void(*start_address)(void *), void* args);
CHAN_OS_API void chan_sleep(double seconds);
CHAN_OS_API const char* chan_thread_name();
CHAN_OS_API uint32_t chan_thread_id();


typedef struct In_Mem_Log {
    const char* n;
    const char* m;
    uint64_t c1;
    uint64_t c2;
} In_Mem_Log;

typedef struct In_Mem_Logger {
    uint64_t capacity_pow2;
    In_Mem_Log* logs;
    volatile uint64_t pos;
} In_Mem_Logger;

static In_Mem_Log _in_mem_logs[1 << 20] = {0};
static In_Mem_Logger _in_mem_logger = {1 << 20, _in_mem_logs};

CHANAPI isize in_mem_log_curr_pos()
{
    return chan_atomic_load64(&_in_mem_logger.pos);
}

CHANAPI uint64_t in_mem_log_val2(const char* msg, uint64_t custom1, uint64_t custom2)
{
    if(0)
    {
        uint64_t curr = chan_atomic_add64(&_in_mem_logger.pos, 1);
        uint64_t index = curr & (_in_mem_logger.capacity_pow2 - 1);

        In_Mem_Log log = {chan_thread_name(), msg, custom1, custom2};
        _in_mem_logger.logs[index] = log;

        return curr;
    }
    return 0;
}

CHANAPI uint64_t in_mem_log_val(const char* msg, uint64_t custom1)
{
    return in_mem_log_val2(msg, custom1, 0);
}

CHANAPI uint64_t in_mem_log(const char* msg)
{
    return in_mem_log_val2(msg, 0, 0);
}

//==========================================================================
// Notes
//==========================================================================
//
//  TICKET LOCKS
// 
// I will try to outline a bit more about how the ticket locks work. The queue is using 
// 'virtualized' indices for head and tail. I say virtualized because they are only increasing 
// (and wrapping aorund on unsigned overflow). To get the actual index to which we push we do `tail % capacity`.
// Because of this the ticket locks at each slot need only to distinguish between two pushes more than capacity
// from each other (ie. head while doing the second -  head while doing the first push >= capacity), else
// each would map to different indices. Thus it is sufficient to calculate the number for wich we wait
// at the ticket lock, which we call id, as `tail / capacity`.
// 
// I will now give a not quite right example for 2 concurent pushes to the same index and fix it afterwards.
// I will also denote by C the capcity of the channel.
// Thread1 starts pushing an item, FAAs tail and gets back 0. C-1 other pushes happen concurently 
// and finally thread2 starts pushing another item, FAAs tail and gets back C. Both of these map
// to index 0%C = C%C = 0. So both thread1 and thread2 go to index 0 and wait on the ticket lock
// to become their id which is 0/C=0 for thread1 and C/C=1 for thread2. The ticket lock is currently 0, 
// so thread1 goes forward and stores its item. When its done it sets the ticket lock to its id + 1 
// (here 0+1). This signals to thread2 (and exclusiveluy thread2!) that its free to proceed and once again
// stores the item and sets the ticket lock to 1+1.
// Exactly the same procedure can be done for the pop operation except incrementing head not tail.
// 
// The scheduling works but we dont want to store to a slot that already has an item! Similarly we dont 
// want to pop from a slot that does not have an item. So between each two pushes at the same index there 
// should be a pop at the same index. Similarly between two pops should be push to the same index. 
// There is a simple way to enforce this: All ids for pushes are even and all ids for pops are odd. 
// Thus if the lock is even only a push will be able to proceed (pops will wait). We store the item and 
// incrment the ticket lock to id+1 thus swicthcing the 'parity' from odd to even and allowing only pops.
// Similarly for starting with odd ticket lock. Now we just need to tweak the id assignment a bit to
// tail/2C for pushes and 1 + head/2C for pops. This gives the complete model for locking.
//
// 
//  BUBBLES
//
// The above might give a wrong impression that the channel acts very predictibly while pushing/popping, 
// yet the opposite is true. The queue spends a lot of its time in state which to outside
// observer looks utterly messy. Consider the simple following sequence of operations:
// (t1,t2,t3 denote independent threads, capacity=3)
// 0) The channel is empty
// 1) t1: push item1
// 2) t2: push item2 - thread gets pause midway through the its execution by the sheduler
// 3) t3: push item3
// 4) t4: pop (item1) - thread gets pause midway through the its execution by the sheduler 
// 
// If we looked at the channel at this instant we would find that it looks like
// 
// +-------+-------+-------+
// | item1 |       | item3 |
// +-------+-------+-------+
//         ^               ^
//         Head            Tail
// 
// It contains a "bubble" - a place that is not yet filled yet treated as if it was 
// (or still filled and already treated as if it wasnt). However to each thread the queue
// behaves as expected. They treat the queue as if it already looked like the following 
// 
// +-------+-------+-------+
// |       | item2 | item3 |
// +-------+-------+-------+
//         ^               ^
//         Head            Tail
// 
// because they only concern themselves with the head/tail indices and then wait their turn.
// This might seem completely obvious it its important to remeber this because it means that 
// for example the following are NOT true (should be visible from the example above):
// 1) When index is outside the range [head, tail) it is not touched any thread
// 2) If all threads were to be stopped at a specific time the queue would look as expected
//    and continue working properly
// 3) If an operation is not completed (ie the ticket lock is not incremented) but the head/tail
//    pointer is decremented (thus reverting it back to the state before the operation) 
//    the channel returns back to its original state.
// 
// 
// CLOSING and LINEARIZABLITY
// 
// When the channel is full, subsequent pushes will make the calling thread wait/block. 
// Because of this its essential to have some mechanism that will wake/unblock the waiting threads. 
// In the paper authors propose a state of closed for this purpose: a channel can be closed once
// unblocking all waiting threads causing them to fail and back off - restore head/tail indices 
// to where they were before the operation - and causing all subsequent operations to fail. At first
// glance this seems perhaps too fatalistic: once close is called no other operation can ever be 
// be peformed. Wouldnt some operation like cancel(), which behaves like close() but does not prevent
// subsequent operations, perhaps be more appropriate? From the discussion under section Bubbles 
// it should be clear that without special treatment the queue after a call to close() is likely
// in an invalid state. We will formalize the intuition using a concept of linearizability.
// 
// Data structure is said to be linearizable if its history of operation requests and completions
// satisfies the following:
// 1) its invocations and responses can be reordered to yield a sequential history
// 2) that sequential history is correct according to the sequential definition of the object
// 3) if a response preceded an invocation in the original history, it must still precede 
//    it in the sequential reordering.
// 
// The channel is linearizable which according to the paper this specifically means that 
// the following needs to hold: when a thread1 calls push(x), push(y) then if any 
// other thread2 pops both of these values it must recieve them exactly in the 
// order x=pop(), y=pop(). We test precisely this in test_channel_linearization.
// 
// Now lets consider a problematic sequence of events: 
//  0) The channel is empty.
//  1) t1: push item1 (to slot #1)
//  2) t1: push item2 (to slot #2)
//  3) t1: push item3 (to slot #3)
//  4) t2: pop  item1 (to slot #1) - does not complete
//  5) t3: push item4 (to slot #1) - is blocked by pending push
//  6) t1: pop  item2 (to slot #2)
//  7) t1: push item5 (to slot #2)
//  8) t1: close() - cancels steps 4) 5)
// 
// The channel looks as follows:
// 
// +-------+-------+-------+
// | item1 | item5 | item3 |
// +-------+-------+-------+
//         ^       ^
//         Head    Tail
//  
// If we ware to continue using the queue we would find this state is contradicting the 
// linearizability chracterization: thread t1 has performed both push(item3) and push(item5), 
// if some other thread would call pop twice they would get item5=pop(), item3=pop(). 
// 
// So why does the paper propose fatalistic close() function? Its a way to prevent 
// the nonlinearizibility: prior to close the channel is linearizable and past close 
// nothing can be observed anymore. Problem solver right?
// 
// Though there are cases when we NEED to use the channel after its closed. If we have a 
// producer thread for example parsing a file and putting the results onto a queue it eventually
// needs to signal that the entire file has been parsed and all consuming thread should stop 
// waiting on pop operation. However if we close, all not yet processed items in the queue are lost! 
// There might be cases where we cant afford that. We might be tempted to circumvent this by sending 
// a special END_OF_INPUT message into the channel (as a regular item) and when recieved by a consumer,
// cause it to exit. The problem though is that we need to send one END_OF_INPUT for each consumer thread
// and the channel can already be full, thus our producer ends up being blocked (still sometimes
// this is the optimal solution).
// 
// We solve this by introducing reopen function, which iterates all items left in the channel, 
// reorders them so that there are no bubble and linearizibility is perserved. 
// This function must be called when we are sure all previously started operations 
// completed (ie no thread still touches the queue). Because the function is quite expensive we also provide
// channel_reopen_unordered which just removes bubbles but does not sort the items. This can change the order
// of elements in the queue but sometimes this is fine. 
// 
// I spend a long time thinking about alternative approaches
// but it seams they all require to use compare and set (CAS) for a regular push/pop which drastically limits the
// throughput of the queue.
//
// 
//  BLOCKING
// 
// Lastly I want to discuss my approach to blocking using futexes since its not discussed in the paper. The idea
// is very straightforward: place a futex on each ticket lock, wait on it when we wait our turn, wake it whenever 
// we increment the ticket lock. However, since we expect some channels to have fairly big capacity, this approach is
// likely to cause contention in the per process futex hash table. This can result in unexpected delays and lower 
// throughput (not just while using the queue but any construct employing futexes). Ideally we would not need to call
// futex wake on ticket lock increase if no thread is blocked on this address. 
// 
// We can achieve this by dedicating the lowest bit of the ticket lock to signal whether 
// someone is waiting on it. Instead of store to increase the ticket lock we perform an atomic exchange 
// and only call futex wake when the waiting bit was set. On the other hand before we call futex wait we 
// alway atomic or the waiting bit. You can verify that this always properly wakes up the waiting thread, by 
// considering all possible ordering of these operations with respect to each other. 
// (Having the lowest bit be the waiting bit means that we have to move the parity bit to be the second lowest
//  as well as tweak slighlty the way we assign ids).
// 
// This approach also enables the use of much hevier multithreaded/multiproces comunication primitives since 
// unless any block occurs (which is the normal case) no call to the potentially expensive wait/wake functions 
// are performed. We enable this and integration with thread pool task systems by making the user supply the 
// wait/wake function themselves. One can supply noops if they wish for the waiting to be a spinlock.

#define _CHAN_WAITING_BIT           ((uint32_t) 1)
#define _CHAN_CANCEL_NOTIFY_BIT     ((uint32_t) 2)
#define _CHAN_FILLED_BIT            ((uint32_t) 4)
#define _CHAN_INCR_TICKET           ((uint64_t) 2)


CHANAPI uint64_t _channel_get_target(const Channel* chan, uint64_t ticket)
{
    return ticket % (uint64_t) chan->capacity;
}

CHANAPI uint32_t _channel_get_id(const Channel* chan, uint64_t ticket)
{
    return ((uint32_t) (ticket / (uint64_t) chan->capacity)*_CHAN_FILLED_BIT*2);
}

CHANAPI bool _channel_id_equals(uint32_t id1, uint32_t id2)
{
    return ((id1 ^ id2) / _CHAN_FILLED_BIT) == 0;
}

CHANAPI bool channel_ticket_is_less(uint64_t ticket_a, uint64_t ticket_b)
{
    uint64_t diff = ticket_a - ticket_b;
    int64_t signed_diff = (int64_t) diff; 
    return signed_diff < 0;
}

CHANAPI void _channel_advance_id(Channel* chan, uint64_t target, uint32_t id, Channel_Info info)
{
    uint32_t* id_ptr = &chan->ids[target];
    
    uint32_t new_id = (uint32_t) (id + _CHAN_FILLED_BIT);
    if(info.wake == chan_wake_noop)
        chan_atomic_store32(id_ptr, new_id);
    else
    {
        uint32_t prev_id = chan_atomic_exchange32(id_ptr, new_id);
        ASSERT(_channel_id_equals(prev_id + _CHAN_FILLED_BIT, new_id));
        if(prev_id & _CHAN_WAITING_BIT)
            info.wake(id_ptr);
    }
}

CHANAPI bool channel_ticket_push_old(Channel* chan, const void* item, uint64_t* out_ticket_or_null, Channel_Info info) 
{
    if(chan_atomic_load32(&chan->closed))
        return false;

    uint64_t ticket = chan_atomic_add64(&chan->tail, 1);
    uint64_t target = _channel_get_target(chan, ticket);
    uint32_t id = _channel_get_id(chan, ticket);
    
    for(;;) {
        uint32_t curr = chan->ids[target];
        if(_channel_id_equals(curr, id))
            break;

        if(chan_atomic_load32(&chan->closed))
        {
            chan_atomic_sub64(&chan->tail, 1);
            return false;
        }
        
        if(info.wake != chan_wake_noop) {
            chan_atomic_or32(&chan->ids[target], _CHAN_WAITING_BIT);
            curr |= _CHAN_WAITING_BIT;
        }

        info.wait(&chan->ids[target], curr, -1);
    }
    
    memcpy(chan->items + target*info.item_size, item, info.item_size);
    _channel_advance_id(chan, target, id, info);
    if(out_ticket_or_null)
        *out_ticket_or_null = ticket;

    return true;
}

CHANAPI bool channel_ticket_pop_old(Channel* chan, void* item, uint64_t* out_ticket_or_null, Channel_Info info) 
{
    if(chan_atomic_load32(&chan->closed))
        return false;

    uint64_t ticket = chan_atomic_add64(&chan->head,1);
    uint64_t target = _channel_get_target(chan, ticket);
    uint32_t id = _channel_get_id(chan, ticket) + _CHAN_FILLED_BIT;
    
    for(;;) {
        uint32_t curr = chan->ids[target];
        if(_channel_id_equals(curr, id))
            break;
            
        if(chan_atomic_load32(&chan->closed))
        {
            chan_atomic_sub64(&chan->head, 1);
            return false;
        }
        
        if(info.wake != chan_wake_noop) {
            chan_atomic_or32(&chan->ids[target], _CHAN_WAITING_BIT);
            curr |= _CHAN_WAITING_BIT;
        }

        info.wait(&chan->ids[target], curr, -1);
    }
    
    memcpy(item, chan->items + target*info.item_size, info.item_size);
    #ifndef NDEBUG
        memset(chan->items + target*info.item_size, -1, info.item_size);
    #endif
    _channel_advance_id(chan, target, id, info);
    if(out_ticket_or_null)
        *out_ticket_or_null = ticket;

    return true;
}


#define _CHANNEL_CLOSED_BIT ((uint64_t) 1)
CHANAPI bool channel_ticket_push(Channel* chan, const void* item, uint64_t* out_ticket_or_null, Channel_Info info) 
{
    if(chan_atomic_load64(&chan->tail) & _CHANNEL_CLOSED_BIT)
        return false;

    uint64_t ticket_and_closed = chan_atomic_add64(&chan->tail, _CHAN_INCR_TICKET);
    uint64_t ticket = ticket_and_closed / _CHAN_INCR_TICKET;
    uint64_t target = _channel_get_target(chan, ticket);
    uint32_t id = _channel_get_id(chan, ticket);

    for(;;) {
        if(ticket_and_closed & _CHANNEL_CLOSED_BIT)
        {
            uint64_t barrier = chan_atomic_load64(&chan->barrier);
            if(ticket >= barrier) 
            {
                in_mem_log_val("push canceled", ticket);
                chan_atomic_sub64(&chan->tail, _CHAN_INCR_TICKET);
                return false;
            }
        }
        
        uint32_t curr = chan->ids[target];
        if(_channel_id_equals(curr, id))
            break;

        if(info.wake != chan_wake_noop) {
            chan_atomic_or32(&chan->ids[target], _CHAN_WAITING_BIT);
            curr |= _CHAN_WAITING_BIT;
        }

        in_mem_log_val("push waiting", ticket);
        info.wait(&chan->ids[target], curr, -1);
        //in_mem_log("push still waiting");
    }
    
    memcpy(chan->items + target*info.item_size, item, info.item_size);
    _channel_advance_id(chan, target, id, info);
    if(out_ticket_or_null)
        *out_ticket_or_null = ticket;

    return true;
}

CHANAPI bool channel_ticket_pop(Channel* chan, void* item, uint64_t* out_ticket_or_null, Channel_Info info) 
{
    uint64_t ticket_and_closed = chan_atomic_add64(&chan->head, _CHAN_INCR_TICKET);
    uint64_t ticket = ticket_and_closed / _CHAN_INCR_TICKET;
    uint64_t target = _channel_get_target(chan, ticket);
    uint32_t id = _channel_get_id(chan, ticket) + _CHAN_FILLED_BIT;
    
    for(;;) {
        uint32_t curr = chan->ids[target];
        if(_channel_id_equals(curr, id))
            break;
            
        int64_t head = chan_atomic_load64(&chan->head);
        if(head & _CHANNEL_CLOSED_BIT)
        {
            uint64_t barrier = chan_atomic_load64(&chan->barrier);
            if(ticket >= barrier) 
            {
                in_mem_log_val("pop canceled", ticket);
                chan_atomic_sub64(&chan->head, _CHAN_INCR_TICKET);
                return false;
            }
        }

        if(info.wake != chan_wake_noop) {
            chan_atomic_or32(&chan->ids[target], _CHAN_WAITING_BIT);
            curr |= _CHAN_WAITING_BIT;
        }
        
        in_mem_log_val("pop waiting", ticket);
        info.wait(&chan->ids[target], curr, -1);
    }
    
    memcpy(item, chan->items + target*info.item_size, info.item_size);
    #ifndef NDEBUG
        memset(chan->items + target*info.item_size, -1, info.item_size);
    #endif
    _channel_advance_id(chan, target, id, info);
    if(out_ticket_or_null)
        *out_ticket_or_null = ticket;

    return true;
}

//INTERLEAVING A before B
//Q empty
// C: chan->tail |= 1
// A: ticket = FAA(tail, 1) //fail
// B: ticket = FAA(head, 1) //success - waiting
// C: chan->head |= 1
// ~~~~~
// C: wake up all
// B: wake up retry and find out its after barrier => stop
// 
//Q empty
// A: ticket = FAA(tail, 1) //success
// C: chan->tail |= 1
// B: ticket = FAA(head, 1) //success
// C: chan->head |= 1
// ~~~~~
// C: wake up all
// 
//Q empty
// A: ticket = FAA(tail, 1) //success
// C: chan->tail |= 1
// C: chan->head |= 1
// B: ticket = FAA(head, 1) //success - popped just pushed
// ~~~~~
// C: wake up all
// 

//INTERLEAVING B before A
//Q empty
// C: chan->tail |= 1
// B: ticket = FAA(head, 1) //success - waiting
// A: ticket = FAA(tail, 1) //fail
// C: chan->head |= 1
// ~~~~~
// C: wake up all
// B: wake up retry and find out its after barrier => stop
// 
//Q empty
// B: ticket = FAA(head, 1) //success - waiting
// C: chan->tail |= 1
// A: ticket = FAA(tail, 1) //success
// C: chan->head |= 1
// ~~~~~
// B: wake up retry and find out its BEFORE barrier => success
// C: wake up all
// 
//Q empty
// B: ticket = FAA(head, 1) //success - waiting
// C: chan->tail |= 1
// C: chan->head |= 1
// A: ticket = FAA(tail, 1) //fail
// ~~~~~
// C: wake up all
// B: wake up retry and find out its after barrier => stop
// 

// W: old = FETCH_AND(var, ~1)
// W: old = EXCH(var, unique2)
// S: curr = unique1;
// S: var |= 1
// S: wait(var, curr | 1)
// W: if(old & 1) wake

CHANAPI bool channel_close(Channel* chan, Channel_Info info) 
{
    if(!chan)
        return false;
        
    in_mem_log("cancel called");
    enum {FREE, TAKEN};
    //Take lock with respect to other calls to close and reopen
    //(the lock can be unfair because it does not matter which thread 
    // succeeds in closing)
    for(;;) {
        uint64_t tail_now = chan_atomic_load64(&chan->tail);
        if(tail_now & _CHANNEL_CLOSED_BIT)
            return false;

        if(chan_atomic_cas32(&chan->closing_lock, FREE, TAKEN))
            break;

        info.wait(&chan->closing_lock, TAKEN, -1);
    }
        
    //atomically set chan->barrier and closed bit with respect to push/pop
    // (thus in conjuction with lock above with respect to everything)
    uint64_t tail = 0;
    uint64_t head = 0;
    for(;;) {
        tail = chan_atomic_load64(&chan->tail);
        ASSERT((tail & _CHANNEL_CLOSED_BIT) == 0);

        chan_atomic_store64(&chan->barrier, tail/_CHAN_INCR_TICKET);
        if(chan_atomic_cas64(&chan->tail, tail, tail | _CHANNEL_CLOSED_BIT))
            break;
    }
    
    head = chan_atomic_or64(&chan->head, _CHANNEL_CLOSED_BIT);
    in_mem_log_val("cancel barrier", tail/_CHAN_INCR_TICKET);

    //If we are using wake up mechanism wakeup all pop tickets in the 
    // range [tail, head) (or more accurately [tail/_CHAN_INCR_TICKET, head/_CHAN_INCR_TICKET) 
    // since they contain the closed bit).
    //We can be sure we dont need to wake up anything else since 
    // after we have set the closed bit to both
    // - tail cant change at all
    // - head can advance but only temporarily. The threads will then
    //   check the closed flag revert themselves.
    // Thus the [tail, head) is superset of all waiting pop tickets!
    if(info.wake != chan_wake_noop)
    {
        for(uint64_t i = tail/_CHAN_INCR_TICKET; i < head/_CHAN_INCR_TICKET; i++)
        {
            uint64_t target = _channel_get_target(chan, i);
                
            //flip cancel notify flag causing pops which are right now
            // between the closed bit check and waiting
            chan_atomic_xor32(&chan->ids[target], _CHAN_CANCEL_NOTIFY_BIT);
            //... and clear the waiting bit for good measure
            // this is optional but will prevent one needless wake up should the 
            // queue be reopened
            chan_atomic_and32(&chan->ids[target], ~_CHAN_WAITING_BIT);
            in_mem_log_val("cancel waken up", i);
            info.wake(&chan->ids[target]);
        }
    }

    //release lock
    chan_atomic_store32(&chan->closing_lock, FREE);
    info.wake(&chan->closing_lock);
    return true;
}

CHANAPI bool channel_reopen(Channel* chan, Channel_Info info) 
{
    if(!chan)
        return false;

    enum {FREE, TAKEN};
    //Take lock with respect to other calls to close and reopen
    //(the lock can be unfair because it does not matter which thread 
    // succeeds in opening)
    for(;;) {
        uint64_t tail_now = chan_atomic_load64(&chan->tail);
        if((tail_now & _CHANNEL_CLOSED_BIT) == 0)
            return false;

        if(chan_atomic_cas32(&chan->closing_lock, FREE, TAKEN))
            break;

        info.wait(&chan->closing_lock, TAKEN, -1);
    }
        
    //clear the flags
    chan_atomic_and64(&chan->head, ~_CHANNEL_CLOSED_BIT);
    chan_atomic_and64(&chan->tail, ~_CHANNEL_CLOSED_BIT);

    //release lock
    chan_atomic_store32(&chan->closing_lock, FREE);
    info.wake(&chan->closing_lock);
    return true;
}


CHANAPI Channel_Res channel_ticket_try_push_weak_old(Channel* chan, const void* item, uint64_t* out_ticket_or_null, Channel_Info info) 
{
    if(chan_atomic_load32(&chan->closed))
        return CHANNEL_CLOSED;

    uint64_t ticket = chan_atomic_load64(&chan->tail);
    uint64_t target = _channel_get_target(chan, ticket);
    uint32_t id = _channel_get_id(chan, ticket);

    uint32_t curr_id = chan_atomic_load32(&chan->ids[target]);
    if(_channel_id_equals(curr_id, id) == false)
        return CHANNEL_FULL;
    if(chan_atomic_cas64(&chan->tail, ticket, ticket+1) == false)
        return CHANNEL_LOST_RACE;

    memcpy(chan->items + target*info.item_size, item, info.item_size);
    _channel_advance_id(chan, target, id, info);
    if(out_ticket_or_null)
        *out_ticket_or_null = ticket;

    return CHANNEL_OK;
}

CHANAPI Channel_Res channel_ticket_try_pop_weak_old(Channel* chan, void* item, uint64_t* out_ticket_or_null, Channel_Info info) 
{
    if(chan_atomic_load32(&chan->closed))
        return CHANNEL_CLOSED;

    uint64_t ticket = chan_atomic_load64(&chan->head);
    uint64_t target = _channel_get_target(chan, ticket);
    uint32_t id = _channel_get_id(chan, ticket) + _CHAN_FILLED_BIT;
    
    uint32_t curr_id = chan_atomic_load32(&chan->ids[target]);
    if(_channel_id_equals(curr_id, id) == false)
        return CHANNEL_EMPTY;
    if(chan_atomic_cas64(&chan->head, ticket, ticket+1) == false)
        return CHANNEL_LOST_RACE;
        
    memcpy(item, chan->items + target*info.item_size, info.item_size);
    _channel_advance_id(chan, target, id, info);
    if(out_ticket_or_null)
        *out_ticket_or_null = ticket;

    return CHANNEL_OK;
}

CHANAPI Channel_Res channel_ticket_try_push_weak(Channel* chan, const void* item, uint64_t* out_ticket_or_null, Channel_Info info) 
{
    uint64_t ticket_and_closed = chan_atomic_load64(&chan->tail);
    if(ticket_and_closed & _CHANNEL_CLOSED_BIT)
        return CHANNEL_CLOSED;

    uint64_t ticket = ticket_and_closed / _CHAN_INCR_TICKET;
    uint64_t target = _channel_get_target(chan, ticket);
    uint32_t id = _channel_get_id(chan, ticket);

    uint32_t curr_id = chan_atomic_load32(&chan->ids[target]);
    if(_channel_id_equals(curr_id, id) == false)
        return CHANNEL_FULL;
    if(chan_atomic_cas64(&chan->tail, ticket_and_closed, ticket_and_closed+_CHAN_INCR_TICKET) == false)
        return CHANNEL_LOST_RACE;

    memcpy(chan->items + target*info.item_size, item, info.item_size);
    _channel_advance_id(chan, target, id, info);
    if(out_ticket_or_null)
        *out_ticket_or_null = ticket;

    return CHANNEL_OK;
}

CHANAPI Channel_Res channel_ticket_try_pop_weak(Channel* chan, void* item, uint64_t* out_ticket_or_null, Channel_Info info) 
{
    uint64_t ticket_and_closed = chan_atomic_load64(&chan->head);
    uint64_t ticket = ticket_and_closed / _CHAN_INCR_TICKET;
    uint64_t target = _channel_get_target(chan, ticket);
    uint32_t id = _channel_get_id(chan, ticket) + _CHAN_FILLED_BIT;
    
    uint32_t curr_id = chan_atomic_load32(&chan->ids[target]);
    if(_channel_id_equals(curr_id, id) == false)
        return CHANNEL_EMPTY; 
    if(chan_atomic_cas64(&chan->head, ticket_and_closed, ticket_and_closed+_CHAN_INCR_TICKET) == false)
        return CHANNEL_LOST_RACE;
        
    memcpy(item, chan->items + target*info.item_size, info.item_size);
    _channel_advance_id(chan, target, id, info);
    if(out_ticket_or_null)
        *out_ticket_or_null = ticket;

    return CHANNEL_OK;
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

    uint64_t diff = tail/_CHAN_INCR_TICKET - head/_CHAN_INCR_TICKET;
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
    return (chan_atomic_load64(&chan->tail) & _CHANNEL_CLOSED_BIT) != 0;
}

CHANAPI bool channel_close_old(Channel* chan, Channel_Info info) 
{
    bool out = false;
    if(chan)
    {
        out = chan_atomic_cas32(&chan->closed, false, true);
        
        //If we closed and have wake up mechanisms
        Sync_Wake_Func wake = info.wake;
        if(out && wake != chan_wake_noop)
        {
            //iterate all slots and wake up blocked threads
            for(uint64_t i = 0; i < (uint64_t) chan->capacity; i++)
            {
                uint64_t prev_id = chan_atomic_and32(&chan->ids[i], ~_CHAN_WAITING_BIT);
                if(prev_id & _CHAN_WAITING_BIT)
                    wake(&chan->ids[i]);
            }
        }
    }

    return out;
}

CHANAPI bool channel_reopen_old(Channel* chan, Channel_Info info) 
{
    enum {OPENING = 2};
    bool out = chan && chan_atomic_cas32(&chan->closed, true, OPENING);
    if(out)
    {
        ASSERT(channel_signed_distance(chan) >= 0);
        isize S = info.item_size;
        uint64_t cap = chan->capacity;
        //remove all holes
        uint64_t filled_to = chan->head;
        for(uint64_t i = chan->head; i != chan->head + cap; i++)
        {
            uint64_t target = _channel_get_target(chan, i);
            uint32_t id = chan->ids[target];
            if(id & _CHAN_FILLED_BIT)
            {
                if(filled_to != i)
                {
                    uint64_t filled_to_target = _channel_get_target(chan, filled_to);
                    chan->ids[filled_to_target] = id;
                    memcpy(chan->items + filled_to_target*S, chan->items + target*S, S);
                }
                filled_to += 1;
            }
        }
        
        uint64_t item_count = filled_to - chan->head;
        ASSERT(item_count <= cap);

        //insertion sort items by their tickets where:
        // ticket = id*capacity + pos
        // 
        //for i = 1; i < length(A); i += 1
        //    j = i
        //    while j > 0 and A[j-1] > A[j]
        //        swap A[j] and A[j-1]
        //        j = j - 1
        //    end while
        //end for

        uint8_t temp_item[1024] = {0};
        for (uint64_t i = 1; i < item_count; i++) 
        {
            for(uint64_t j = i; j > 0; j--)
            {
                isize k = j - 1;
                uint64_t j_target = _channel_get_target(chan, chan->head + j);
                uint64_t k_target = _channel_get_target(chan, chan->head + k);

                uint64_t j_ticket = (chan->ids[j_target] & ~_CHAN_WAITING_BIT)*cap + j_target;
                uint64_t k_ticket = (chan->ids[k_target] & ~_CHAN_WAITING_BIT)*cap + k_target;
                if(k_ticket <= j_ticket)
                    break;

                memcpy(temp_item,                 chan->items + j_target*S, S);
                memcpy(chan->items + j_target*S,  chan->items + k_target*S, S);
                memcpy(chan->items + k_target*S,  temp_item, S);

                uint32_t temp_id = chan->ids[j_target];
                chan->ids[j_target] = chan->ids[k_target];
                chan->ids[k_target] = temp_id;
            }
        }
        
        //Reset ids
        for(uint64_t i = 0; i < cap; i++)
        {
            uint64_t ticket = i + chan->head;
            uint64_t target = _channel_get_target(chan, ticket);
            uint32_t id = _channel_get_id(chan, ticket);

            if(i < item_count)
                chan->ids[target] = id | _CHAN_FILLED_BIT;
            else
                chan->ids[target] = id & ~_CHAN_FILLED_BIT;
        }

        chan->tail = filled_to;
        chan_atomic_store32(&chan->closed, false);
    }

    return out;
}

CHANAPI bool channel_reopen_old_unordered(Channel* chan, Channel_Info info) 
{
    enum {OPENING = 2};
    bool out = chan && chan_atomic_cas32(&chan->closed, true, OPENING);
    if(out)
    {
        ASSERT(channel_signed_distance(chan) >= 0);
        isize S = info.item_size;

        //remove all holes
        uint64_t filled_to = 0;
        for(uint64_t i = 0; i < (uint64_t) chan->capacity; i++)
        {
            uint32_t id = chan->ids[i];
            if(id & _CHAN_FILLED_BIT)
            {
                if(filled_to != i)
                    memcpy(chan->items + filled_to*S, chan->items + i*S, S);

                filled_to += 1;
            }
        }
        
        //reset ids
        for(uint64_t i = 0; i < (uint64_t) chan->capacity; i++)
        {
            chan->ids[i] = i < filled_to ? _CHAN_FILLED_BIT : 0;
            if(i >= filled_to)
                memset(chan->items + i*S, -1, S);
        }

        chan->head = 0;
        chan->tail = filled_to;
        chan_atomic_store32(&chan->closed, false);
    }

    return out;
}

CHANAPI void channel_init_custom(Channel* chan, void* items, uint32_t* ids, isize capacity, Channel_Info info)
{
    assert(ids);
    assert(capacity > 0 && "must be power of two");

    memset(chan, 0, sizeof* chan);
    chan->items = (uint8_t*) items;
    chan->ids = ids;
    chan->capacity = capacity; 
    chan->info = info;
    chan->ref_count = 2;

    //essentially a memory fence with respect to any other function
    chan_atomic_store64(&chan->head, 0);
    chan_atomic_store64(&chan->tail, 0);
    chan_atomic_store32(&chan->closed, false);
}

CHANAPI Channel* channel_init(isize capacity, Channel_Info info)
{
    isize total_size = sizeof(Channel) + capacity*sizeof(uint32_t) + capacity*info.item_size;

    Channel* chan = (Channel*) chan_aligned_alloc(total_size, CHAN_CACHE_LINE);
    uint32_t* ids = (uint32_t*) (void*) (chan + 1);
    void* items = ids + capacity;
    memset(ids, 0, (size_t) capacity*sizeof *ids);

    if(chan)
    {
        channel_init_custom(chan, items, ids, capacity, info);
        chan_atomic_store32(&chan->allocated, true);
    }

    return chan;
}

CHANAPI Channel* channel_share(Channel* chan)
{
    uint32_t prev = chan_atomic_add32(&chan->ref_count, 2);
    if(chan->info.wake != chan_wake_noop && (prev & _CHAN_WAITING_BIT))
    {
        chan_atomic_and32(&chan->ref_count, ~_CHAN_WAITING_BIT);
        chan->info.wake(&chan->ref_count);
    }

    return chan;
}

CHANAPI int32_t channel_deinit(Channel* chan)
{
    if(chan == NULL)
        return 0;

    int32_t refs = (int32_t) chan_atomic_sub32(&chan->ref_count, 2) - 2;
    if(chan->info.wake != chan_wake_noop && (refs & _CHAN_WAITING_BIT))
    {
        chan_atomic_and32(&chan->ref_count, ~_CHAN_WAITING_BIT);
        chan->info.wake(&chan->ref_count);
    }

    if(refs == 0)
    {
        if(chan_atomic_load32(&chan->allocated))
            chan_aligned_free(chan, CHAN_CACHE_LINE);
        else
            memset(chan, 0, sizeof *chan);
    }

    return refs;
}

CHANAPI int32_t channel_deinit_close(Channel* chan)
{
    int32_t refs = channel_deinit(chan);
    if(refs > 0)
        channel_close(chan, chan->info);
    return refs;
}

CHANAPI void channel_wait_for_exclusivity(const Channel* chan, Channel_Info info)
{
    for(;;) {
        uint32_t curr = chan_atomic_load32(&chan->ref_count);

        ASSERT(curr < UINT32_MAX / 2);
        if(curr <= 2 || curr >= UINT32_MAX / 2) //we exit on undeflows as well but assert.
            break;

        if(chan->info.wake != chan_wake_noop) {
            chan_atomic_or32((void*) &chan->ref_count, _CHAN_WAITING_BIT);
            curr |= _CHAN_WAITING_BIT;
        }

        info.wait((void*) &chan->ref_count, curr, -1);
    }
}

//A: futex |= 1;
//A: wait(futex)
//B: old = EXCH(futex, 0)
//B: if(old & 1) wake()
// wake

//B would need to happen as an atomic lockstep!
//B: curr = EXCH(futex, 0)
//B: loop
//B:    if(curr & 1)
//B:      wake;
//B:    if(curr == futex)
//B:       break
//B:    curr = futex


//A: curr = futex
//A: if curr != 0
//B: old = EXCH(futex, 0)
//A:    futex |= 1;
//A:    wait(futex, curr | 1)
//B: if(old & 1) wake()
// 

//B: old = EXCH(futex, 0)
//A: futex |= 1;
//A: wait(futex)
//B: if(old & 1) wake()
// 
//B: if(futex & 1) wake()

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

CHANAPI void sync_set_and_wake(volatile void* state, uint32_t to, Sync_Wait wait)
{
    if(wait.wake == chan_wake_noop)
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
        in_mem_log_val("wait neq", current);
        if((current & ~wait.notify_bit) != (desired & ~wait.notify_bit))
        {
            in_mem_log_val("wait neq done", desired);
            return current & ~wait.notify_bit;
        }
        
        in_mem_log_val("wait neq still waiting", current);
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

CHANAPI int32_t wait_group_count(volatile Wait_Group* wg)
{
    return (int32_t) chan_atomic_load32(wg);
}
CHANAPI Wait_Group* wait_group_push(volatile Wait_Group* wg, isize count)
{
    if(count > 0)
        chan_atomic_add32(wg, (uint32_t) count);
    return (Wait_Group*) wg;
}
CHANAPI bool wait_group_pop(volatile Wait_Group* wg, isize count, Sync_Wait wait)
{
    bool out = false;
    if(count <= 0)
        in_mem_log_val("wait_group_pop negative", (uint64_t) -count);
    else
    {
        uint32_t old_val = chan_atomic_sub32(wg, (uint32_t) count);
        in_mem_log_val2("wait_group_pop", old_val, old_val - (uint32_t) count);

        //if this was the pop that got it over the line wake 
        int32_t old_val_signed = (int32_t) old_val;
        if(wait.wake != chan_wake_noop && old_val_signed - count <= 0 && old_val_signed > 0)
        {
            in_mem_log_val2("wait_group_pop WAKE", old_val, old_val - (uint32_t) count);
            wait.wake(wg);
            out = true;
        }
    }

    return out;
}
CHANAPI void wait_group_wait(volatile Wait_Group* wg, Sync_Wait wait)
{
    for(;;) {
        uint32_t current = chan_atomic_load32(wg);
        in_mem_log_val("wait_group_wait", current);
        if((int32_t) current <= 0)
        {
            in_mem_log("wait_group_wait done");
            return;
        }

        in_mem_log_val("wait_group_wait WAIT", current);
        wait.wait(wg, current, -1);
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
    for(;;) {
        uint32_t current = chan_atomic_load32(wg);
        in_mem_log_val("wait_group_wait_timed", current);
        if((int32_t) current <= 0)
        {
            in_mem_log("wait_group_wait_timed done");
            return true;
        }

        isize curr_ticks = chan_perf_counter();
        isize ellapsed_ticks = curr_ticks - start_ticks;
        if(ellapsed_ticks > wait_ticks)
        {
            in_mem_log_val("wait_group_wait_timed given after ticks", (uint64_t) wait_ticks);
            return false;
        }
        
        isize wait_ms = (wait_ticks - ellapsed_ticks)/freq_ms;
        wait.wait(wg, current, wait_ms);
    }
}

#ifdef _MSC_VER
    CHAN_INTRINSIC uint64_t chan_atomic_load64(const volatile void* target)
    {
        return (uint64_t) __iso_volatile_load64((const long long*) target);
    }
    CHAN_INTRINSIC uint32_t chan_atomic_load32(const volatile void* target)
    {
        return (uint32_t) __iso_volatile_load32((const int*) target);
    }
    CHAN_INTRINSIC void chan_atomic_store64(volatile void* target, uint64_t value)
    {
        __iso_volatile_store64((long long*) target, (long long) value);
    }
    CHAN_INTRINSIC void chan_atomic_store32(volatile void* target, uint32_t value)
    {
        __iso_volatile_store32((int*) target, (int) value);
    }
    CHAN_INTRINSIC bool chan_atomic_cas64(volatile void* target, uint64_t old_value, uint64_t new_value)
    {
        return _InterlockedCompareExchange64((long long*) target, (long long) new_value, (long long) old_value) == (long long) old_value;
    }
    CHAN_INTRINSIC bool chan_atomic_cas32(volatile void* target, uint32_t old_value, uint32_t new_value)
    {
        return _InterlockedCompareExchange((long*) target, (long) new_value, (long) old_value) == (long) old_value;
    }
    CHAN_INTRINSIC uint64_t chan_atomic_exchange64(volatile void* target, uint64_t value)
    {
        return (uint64_t) _InterlockedExchange64((long long*) target, (long long) value);
    }
    CHAN_INTRINSIC uint32_t chan_atomic_exchange32(volatile void* target, uint32_t value)
    {
        return (uint32_t) _InterlockedExchange((long*) target, (long) value);
    }
    CHAN_INTRINSIC uint64_t chan_atomic_add64(volatile void* target, uint64_t value)
    {
        return (uint64_t) _InterlockedExchangeAdd64((long long*) (void*) target, (long long) value);
    }
    CHAN_INTRINSIC uint32_t chan_atomic_add32(volatile void* target, uint32_t value)
    {
        return (uint32_t) _InterlockedExchangeAdd((long*) (void*) target, (long long) value);
    }
    CHAN_INTRINSIC uint64_t chan_atomic_sub64(volatile void* target, uint64_t value)
    {
        return chan_atomic_add64(target, (uint64_t) -(int64_t) value);
    }
    CHAN_INTRINSIC uint32_t chan_atomic_sub32(volatile void* target, uint32_t value)
    {
        return chan_atomic_add32(target, (uint64_t) -(int64_t) value);
    }
    CHAN_INTRINSIC uint64_t chan_atomic_or64(volatile void* target, uint64_t value)
    {
        return (uint64_t) _InterlockedOr64((long long*) (void*) target, (long long) value);
    }
    CHAN_INTRINSIC uint32_t chan_atomic_or32(volatile void* target, uint32_t value)
    {
        return (uint32_t) _InterlockedOr((long*) (void*) target, (long) value);
    }
    CHAN_INTRINSIC uint64_t chan_atomic_and64(volatile void* target, uint64_t value)
    {
        return (uint64_t) _InterlockedAnd64((long long*) (void*) target, (long long) value);
    }
    CHAN_INTRINSIC uint32_t chan_atomic_and32(volatile void* target, uint32_t value)
    {
        return (uint32_t) _InterlockedAnd((long*) (void*) target, (long) value);
    }
    CHAN_INTRINSIC uint32_t chan_atomic_xor32(volatile void* target, uint32_t value)
    {
        return (uint32_t) _InterlockedXor((long*) (void*) target, (long) value);
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
CHAN_INTRINSIC bool chan_wait_pause(volatile void* state, uint32_t undesired, isize timeout_or_minus_one_if_infinite)
{
    (void) state; (void) undesired; (void) timeout_or_minus_one_if_infinite;
    #if CHAN_ARCH == CHAN_ARCH_X86 || CHAN_ARCH == CHAN_ARCH_X64
        _mm_pause();
    #elif CHAN_ARCH == CHAN_ARCH_ARM
        __yield();
    #endif
    return true;
}
CHAN_INTRINSIC bool chan_wait_noop(volatile void* state, uint32_t undesired, isize timeout_or_minus_one_if_infinite)
{
    (void) state; (void) undesired; (void) timeout_or_minus_one_if_infinite;
    return true;
}
CHAN_INTRINSIC void chan_wake_noop(volatile void* state)
{
    (void) state;
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

        in_mem_log_val("futex wait", undesired);
        bool value_changed = (bool) WaitOnAddress(state, &undesired, sizeof undesired, wait);

        #if 0
        bool value_changed = false;
        for(isize i = 0; i < 8; i++)
        {
            value_changed = !!WaitOnAddress(state, &undesired, sizeof undesired, 1000);
            if(value_changed)
                break;
                
            in_mem_log_val("futex still waiting", undesired);
        }
        #endif
        
        if(value_changed)
            in_mem_log_val("futex done", value_changed);
        else
            in_mem_log_val("futex failed", value_changed);
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
#ifndef TEST
    #define TEST(x, ...) (!(x) ? printf("TEST(%s) failed", #x), abort() : (void) 0)
#endif // !TEST

#define TEST_INT_CHAN_INFO _CHAN_SINIT(Channel_Info){sizeof(int), chan_wait_block, chan_wake_block}
#define THROUGHPUT_INT_CHAN_INFO _CHAN_SINIT(Channel_Info){sizeof(int), chan_wait_pause, chan_wake_noop}

#define TEST_CHAN_MAX_THREADS 128 

typedef struct _Test_Channel_Lin_Point {
    uint64_t value;
    uint32_t thread_id;
    int32_t _;
} _Test_Channel_Lin_Point;

typedef struct _Test_Channel_Linearization_Thread {
    Channel* chan;
    Wait_Group* reached_notification;
    Wait_Group* done;
    uint32_t* run_status;
    uint32_t my_id;
    uint32_t _;
    char name[30];
    bool print;
    bool okay;
} _Test_Channel_Linearization_Thread;

#define TEST_CHAN_LIN_POINT_INFO _CHAN_SINIT(Channel_Info){sizeof(_Test_Channel_Lin_Point), chan_wait_yield, chan_wake_noop}

enum {
    _TEST_CHANNEL_STATUS_STOPPED = 0,
    _TEST_CHANNEL_STATUS_RUN = 1,
    _TEST_CHANNEL_STATUS_EXIT = 2,
};

void _test_channel_linearization_consumer(void* arg)
{
    _Test_Channel_Linearization_Thread* context = (_Test_Channel_Linearization_Thread*) arg;
    chan_set_thread_name(context->name);
    context->okay = true;
    if(context->print) 
        printf("   %s created\n", context->name);

    uint64_t max_per_thread[TEST_CHAN_MAX_THREADS] = {0};
    for(int i = 0;; i++)
    {
        //Notify controlling thread we have reached the notificication
        wait_group_pop(context->reached_notification, 1, SYNC_WAIT_BLOCK);

        //wait for run status
        uint32_t curr = sync_wait_for_not_equal(context->run_status, _TEST_CHANNEL_STATUS_STOPPED, SYNC_WAIT_BLOCK_BIT(31));
        if(curr == _TEST_CHANNEL_STATUS_EXIT)
            break;

        if(context->print) 
            printf("   %s run #%i\n", context->name, i+1);
            
        for(;;)
        {
            _Test_Channel_Lin_Point point = {0};
            if(channel_pop(context->chan, &point, TEST_CHAN_LIN_POINT_INFO) == false)
                break;
        
            if(point.thread_id > TEST_CHAN_MAX_THREADS)
            {
                context->okay = false;
                printf("   %s encountered thread id %i out of range!\n", 
                    context->name, point.thread_id);
            }
            else if(max_per_thread[point.thread_id] >= point.value)
            {
                context->okay = false;
                printf("   %s encountered value %lli which was nor more than previous %lli\n", 
                    context->name, point.value, max_per_thread[point.thread_id]);
                max_per_thread[point.thread_id] = point.value;
            }
            else
                max_per_thread[point.thread_id] = point.value;
        }
    }

    if(context->print) 
        printf("   %s exited with %s\n", context->name, context->okay ? "okay" : "fail");

    wait_group_pop(context->done, 1, SYNC_WAIT_BLOCK);
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
        //Notify controlling thread we have reached the notificication
        wait_group_pop(context->reached_notification, 1, SYNC_WAIT_BLOCK);

        //wait for run status
        uint32_t curr = sync_wait_for_not_equal(context->run_status, _TEST_CHANNEL_STATUS_STOPPED, SYNC_WAIT_BLOCK_BIT(31));
        if(curr == _TEST_CHANNEL_STATUS_EXIT)
            break;
            
        if(context->print) 
            printf("   %s run #%i\n", context->name, i+1);
            
        for(;; curr_max += 1)
        {
            _Test_Channel_Lin_Point point = {0};
            point.thread_id = context->my_id;
            point.value = curr_max;
            if(channel_push(context->chan, &point, TEST_CHAN_LIN_POINT_INFO) == false)
                break;
        }
    }

    if(context->print) 
        printf("   %s exited\n", context->name);

    wait_group_pop(context->done, 1, SYNC_WAIT_BLOCK);
}

void test_channel_linearization(isize buffer_capacity, isize producer_count, isize consumers_count, isize stop_count, double seconds, bool printing, bool thread_printing)
{
    if(printing)
        printf("Channel: Testing liearizability with buffer capacity %lli producers:%lli consumers:%lli for %.2lfs\n", 
            buffer_capacity, producer_count, consumers_count, seconds);

    Channel* chan = channel_init(buffer_capacity, TEST_CHAN_LIN_POINT_INFO);
    
    uint32_t run_status = _TEST_CHANNEL_STATUS_STOPPED;
    Wait_Group done = 0;
    Wait_Group reached_notification = 0;
    wait_group_push(&done, producer_count + consumers_count);
    wait_group_push(&reached_notification, producer_count + consumers_count);
    
    _Test_Channel_Linearization_Thread producers[TEST_CHAN_MAX_THREADS] = {0};
    _Test_Channel_Linearization_Thread consumers[TEST_CHAN_MAX_THREADS] = {0};

    for(isize i = 0; i < producer_count; i++)
    {
        producers[i].chan = chan;
        producers[i].my_id = (uint32_t) i;
        producers[i].print = thread_printing;
        producers[i].reached_notification = &reached_notification;
        producers[i].run_status = &run_status;
        producers[i].done = &done;
        snprintf(producers[i].name, sizeof producers[i].name, "producer #%02lli", i);
        chan_start_thread(_test_channel_linearization_producer, &producers[i]);
    }
    
    for(isize i = 0; i < consumers_count; i++)
    {
        consumers[i].chan = chan;
        consumers[i].print = thread_printing;
        consumers[i].reached_notification = &reached_notification;
        consumers[i].run_status = &run_status;
        consumers[i].done = &done;
        snprintf(consumers[i].name, sizeof consumers[i].name, "consumer #%02lli", i);
        chan_start_thread(_test_channel_linearization_consumer, &consumers[i]);
    }
    
    //wait for all threads to wait on notification 
    //(no threads are currently using the channel)
    wait_group_wait(&reached_notification, SYNC_WAIT_BLOCK);
    wait_group_push(&reached_notification,  producer_count + consumers_count); //reset the notification

    //Run threads for seconds and repeatedly interrupt them with calls to close. 
    // No items needs to be permamently lost
    for(int i = 0; i < stop_count; i++)
    {
        if(printing)
            printf("   Enabling threads to run #%i for %.2lfs\n", i+1, seconds/stop_count);
        //Run the threads for seconds/stop_count
        sync_set_and_wake(&run_status, _TEST_CHANNEL_STATUS_RUN, SYNC_WAIT_BLOCK_BIT(31));

        chan_sleep(seconds/stop_count);
        
        if(printing)
            printf("   Stopping threads #%i\n", i+1);

        //Stop/close channels
        sync_set_and_wake(&run_status, _TEST_CHANNEL_STATUS_STOPPED, SYNC_WAIT_BLOCK_BIT(31));
        channel_close(chan, TEST_INT_CHAN_INFO);
        
        //wait for all threads to wait on notification 
        //(no threads are currently using the channel)
        wait_group_wait(&reached_notification, SYNC_WAIT_BLOCK);
        wait_group_push(&reached_notification,  producer_count + consumers_count); //reset the notification
        
        if(printing)
            printf("   All threads stopped #%i\n", i+1);

        //Everything ok?
        for(isize k = 0; k < consumers_count; k++)
            TEST(consumers[k].okay);

        channel_reopen(chan, TEST_INT_CHAN_INFO);
    }
    
    sync_set_and_wake(&run_status, _TEST_CHANNEL_STATUS_EXIT, SYNC_WAIT_BLOCK_BIT(31));
    wait_group_wait(&done, SYNC_WAIT_BLOCK);
    
    if(printing)
        printf("   All threads finished\n");

    channel_deinit(chan);
}

typedef struct _Test_Channel_Cycle_Thread {
    Channel* a;
    Channel* b;
    Channel* lost;
    uint32_t* run_status;
    Wait_Group* reached_notification;
    Wait_Group* done;
    char name[31];
    bool print;
} _Test_Channel_Cycle_Thread;

void _test_channel_cycle_runner(void* arg)
{
    _Test_Channel_Cycle_Thread* context = (_Test_Channel_Cycle_Thread*) arg;
    chan_set_thread_name(context->name);
    in_mem_log("created");

    for(int i = 0;; i++)
    {
        //Notify controlling thread we have reached the notificication
        wait_group_pop(context->reached_notification, 1, SYNC_WAIT_BLOCK);
        

        //wait for run status
        in_mem_log("waiting for not equal");
        uint32_t curr = sync_wait_for_not_equal(context->run_status, _TEST_CHANNEL_STATUS_STOPPED, SYNC_WAIT_BLOCK_BIT(31));
        if(curr == _TEST_CHANNEL_STATUS_EXIT)
            break;

        for(;;)
        {
            int val = 0;
            if(channel_pop(context->b, &val, TEST_INT_CHAN_INFO) == false)
            {
                in_mem_log("pop failed (closed)");
                break;
            }
        
            in_mem_log_val("popped", (uint32_t) val);
            if(channel_push(context->a, &val, TEST_INT_CHAN_INFO) == false)
            {
                in_mem_log_val("lost (adding to lost channel)", (uint32_t) val);
                channel_push(context->lost, &val, TEST_INT_CHAN_INFO);
                break;
            }
            in_mem_log_val("pushed", (uint32_t) val);
        }
    }
    
    in_mem_log("exited");
    wait_group_pop(context->done, 1, SYNC_WAIT_BLOCK);
}

int _int_comp_fn(const void* a, const void* b)
{
    return (*(int*) a > *(int*) b) - (*(int*) a < *(int*) b);
}

void test_channel_cycle(isize buffer_capacity, isize a_count, isize b_count, isize stop_count, double seconds, bool printing, bool thread_printing)
{
    if(printing)
        printf("Channel: Testing cycle with buffer capacity %lli threads A:%lli threads B:%lli for %.2lfs\n", 
            buffer_capacity, a_count, b_count, seconds);

    Channel* a_chan = channel_init(buffer_capacity, TEST_INT_CHAN_INFO);
    Channel* b_chan = channel_init(buffer_capacity, TEST_INT_CHAN_INFO);
    Channel* lost_chan = channel_init((a_count + b_count)*(stop_count + 1), TEST_INT_CHAN_INFO);
    
    for(int i = 0; i < a_chan->capacity; i++)
        channel_push(a_chan, &i, TEST_INT_CHAN_INFO);
        
    uint32_t run_status = _TEST_CHANNEL_STATUS_STOPPED;
    Wait_Group reached_notification = 0;
    Wait_Group done = 0;
    wait_group_push(&reached_notification, a_count + b_count);
    wait_group_push(&done, a_count + b_count);

    _Test_Channel_Cycle_Thread a_threads[TEST_CHAN_MAX_THREADS] = {0};
    _Test_Channel_Cycle_Thread b_threads[TEST_CHAN_MAX_THREADS] = {0};

    for(isize i = 0; i < a_count; i++)
    {
        _Test_Channel_Cycle_Thread state = {0};
        state.a = a_chan;
        state.b = b_chan;
        state.lost = lost_chan;
        state.run_status = &run_status;
        state.print = thread_printing;
        state.reached_notification = &reached_notification;
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
        state.run_status = &run_status;
        state.print = thread_printing;
        state.reached_notification = &reached_notification;
        state.done = &done;
        snprintf(state.name, sizeof state.name, "B -> A #%lli", i);

        b_threads[i] = state;
        chan_start_thread(_test_channel_cycle_runner, b_threads + i);
    }
    
    //wait for all threads to wait on notification 
    //(no threads are currently using the channel)
    wait_group_wait_timed(&reached_notification, 5, SYNC_WAIT_BLOCK);
    wait_group_push(&reached_notification, a_count + b_count); //reset the notification

    //Run threads for seconds and repeatedly interrupt them with calls to close. 
    // No items needs to be permamently lost
    for(int i = 0; i < stop_count; i++)
    {
        
        in_mem_log_val("Enabling threads to run", i+1);
        if(printing) printf("   Enabling threads to run #%i for %.2lfs\n", i+1, seconds/stop_count);
        //Run the threads for seconds/stop_count
        sync_set_and_wake(&run_status, _TEST_CHANNEL_STATUS_RUN, SYNC_WAIT_BLOCK_BIT(31));

        chan_sleep(seconds/stop_count);
        
        in_mem_log_val("Stopping threads ", i+1);
        //if(printing) printf("   Stopping threads #%i\n", i+1);

        //Stop/close channels
        sync_set_and_wake(&run_status, _TEST_CHANNEL_STATUS_STOPPED, SYNC_WAIT_BLOCK_BIT(31));
        channel_close(a_chan, TEST_INT_CHAN_INFO);
        channel_close(b_chan, TEST_INT_CHAN_INFO);
        
        //wait for all threads to wait on notification 
        //(no threads are currently using the channel)
        TEST(wait_group_wait_timed(&reached_notification, 5, SYNC_WAIT_BLOCK));
        wait_group_push(&reached_notification, a_count + b_count); //reset the notification
        
        in_mem_log_val("All threads stopped ", i+1);
        //if(printing) printf("   All threads stopped #%i\n", i+1);

        //reopen channels and check we havent lost anything
        channel_reopen(a_chan, TEST_INT_CHAN_INFO);
        channel_reopen(b_chan, TEST_INT_CHAN_INFO);
        isize a_chan_count = channel_count(a_chan);
        isize b_chan_count = channel_count(b_chan);
        isize lost_count = channel_count(lost_chan);

        isize count_sum = a_chan_count + b_chan_count + lost_count;
        TEST(count_sum == buffer_capacity);
    }
    
    //signal to threads to exit
    sync_set_and_wake(&run_status, _TEST_CHANNEL_STATUS_EXIT, SYNC_WAIT_BLOCK_BIT(31));
    wait_group_wait(&done, SYNC_WAIT_BLOCK);

    //if(printing) printf("   All threads finished\n");
    in_mem_log("All threads finished ");

    isize everything_i = 0;
    int* everything = (int*) malloc(buffer_capacity*sizeof(int));
    for(isize i = 0;; i++)
    {
        int x = 0;
        Channel_Res res = channel_try_pop(a_chan, &x, TEST_INT_CHAN_INFO);
        TEST(res != CHANNEL_LOST_RACE);
        if(res)
            break;
        everything[everything_i++] = x;
        TEST(everything_i <= buffer_capacity);
    }

    for(isize i = 0;; i++)
    {
        int x = 0;
        Channel_Res res = channel_try_pop(b_chan, &x, TEST_INT_CHAN_INFO);
        TEST(res != CHANNEL_LOST_RACE);
        if(res)
            break;
        everything[everything_i++] = x;
        TEST(everything_i <= buffer_capacity);
    }

    for(isize i = 0;; i++)
    {
        int x = 0;
        Channel_Res res = channel_try_pop(lost_chan, &x, TEST_INT_CHAN_INFO);
        TEST(res != CHANNEL_LOST_RACE);
        if(res)
            break;
        everything[everything_i++] = x;
        TEST(everything_i <= buffer_capacity);
    }

    TEST(everything_i == buffer_capacity);
    qsort(everything, buffer_capacity, sizeof(int), _int_comp_fn);
    for(isize i = 0; i < buffer_capacity; i++)
        TEST(everything[i] == i);

    free(everything);

    channel_deinit(a_chan);
    channel_deinit(b_chan);
    channel_deinit(lost_chan);
}

void test_channel_sequential(isize capacity)
{
    Channel* chan = channel_init(capacity, TEST_INT_CHAN_INFO);
    int dummy = 0;
    
    //Test blocking interface
    {
        //Push all
        for(int i = 0; i < chan->capacity; i++)
            TEST(channel_push(chan, &i, TEST_INT_CHAN_INFO));

        TEST(channel_try_push(chan, &dummy, TEST_INT_CHAN_INFO) == CHANNEL_FULL);
    
        //Close reopen and check info
        TEST(channel_count(chan) == capacity);
        TEST(channel_close(chan, TEST_INT_CHAN_INFO));
        TEST(channel_is_closed(chan));
        TEST(channel_count(chan) == capacity);
        TEST(channel_reopen(chan, TEST_INT_CHAN_INFO));
        TEST(channel_count(chan) == capacity);

        //Pop all
        for(int i = 0; i < chan->capacity; i++)
        {
            int popped = 0;
            TEST(channel_pop(chan, &popped, TEST_INT_CHAN_INFO));
            TEST(popped == i);
        }

        TEST(channel_count(chan) == 0);
        TEST(channel_try_pop(chan, &dummy, TEST_INT_CHAN_INFO) == CHANNEL_EMPTY);
        TEST(channel_count(chan) == 0);
    }

    //Push up to capacity then close, then pop until 
    {
        int push_count = (int) chan->capacity - 1;
        for(int i = 0; i < push_count; i++)
            TEST(channel_push(chan, &i, TEST_INT_CHAN_INFO));
    
        TEST(channel_close(chan, TEST_INT_CHAN_INFO));
        TEST(channel_try_push(chan, &dummy, TEST_INT_CHAN_INFO) == CHANNEL_CLOSED);

        int popped = 0;
        int pop_count = 0;
        for(; channel_pop(chan, &popped, TEST_INT_CHAN_INFO); pop_count++)
        {
            TEST(popped == pop_count);
        }

        TEST(pop_count == push_count);
        TEST(channel_count(chan) == 0);

        TEST(channel_reopen(chan, TEST_INT_CHAN_INFO));
    }

    //Test nonblocking interface
    {
        for(int i = 0;; i++)
        {
            Channel_Res res = channel_try_push(chan, &i, TEST_INT_CHAN_INFO);
            if(res != CHANNEL_OK)
            {
                TEST(res == CHANNEL_FULL);
                break;
            }
        }

        TEST(channel_count(chan) == chan->capacity);
        channel_close(chan, TEST_INT_CHAN_INFO);
        
        TEST(channel_try_push(chan, &dummy, TEST_INT_CHAN_INFO) == CHANNEL_CLOSED);
        TEST(channel_count(chan) == chan->capacity);
        
        channel_reopen(chan, TEST_INT_CHAN_INFO);
        TEST(channel_count(chan) == chan->capacity);
        
        int pop_count = 0;
        for(;; pop_count++)
        {
            int popped = 0;
            Channel_Res res = channel_try_pop(chan, &popped, TEST_INT_CHAN_INFO);
            if(res != CHANNEL_OK)
            {
                TEST(res == CHANNEL_EMPTY);
                break;
            }

            TEST(popped == pop_count);
        }

        TEST(channel_count(chan) == 0);
    }

    
    //Test nonblocking interface after close
    {
        int push_count = (int) chan->capacity - 1;
        for(int i = 0; i < push_count; i++)
        {
            Channel_Res res = channel_try_push(chan, &i, TEST_INT_CHAN_INFO);
            TEST(res == CHANNEL_OK);
        }

        TEST(channel_count(chan) == push_count);
        channel_close(chan, TEST_INT_CHAN_INFO);
        
        TEST(channel_try_push(chan, &dummy, TEST_INT_CHAN_INFO) == CHANNEL_CLOSED);
        TEST(channel_count(chan) == push_count);
        
        int pop_count = 0;
        for(;; pop_count++)
        {
            int popped = 0;
            Channel_Res res = channel_try_pop(chan, &popped, TEST_INT_CHAN_INFO);
            if(res != CHANNEL_OK)
            {
                TEST(res == CHANNEL_EMPTY);
                break;
            }

            TEST(popped == pop_count);
        }

        TEST(pop_count == push_count);
        TEST(channel_count(chan) == 0);
        channel_reopen(chan, TEST_INT_CHAN_INFO);
    }

    channel_deinit_close(chan);
}

typedef struct _Test_Channel_Throughput_Thread {
    Channel* chan;
    uint32_t* run_status;
    Wait_Group* wait_group_done;
    Wait_Group* wait_group_started;
    volatile isize operations;
    volatile isize ticks_before;
    volatile isize ticks_after;
    bool is_consumer;
    bool is_no_bubble;
    bool _[6];
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

    if(context->is_no_bubble)
    {
        /*
        if(context->is_consumer)
            while(channel_ticket_pop_no_bubble(context->chan, &val, NULL, THROUGHPUT_INT_CHAN_INFO))
                operations += 1;
        else
            while(channel_ticket_push_no_bubble(context->chan, &val, NULL, THROUGHPUT_INT_CHAN_INFO))
                operations += 1;
        */
    }
    else
    {
        if(context->is_consumer)
            while(channel_pop(context->chan, &val, THROUGHPUT_INT_CHAN_INFO))
                operations += 1;
        else
            while(channel_push(context->chan, &val, THROUGHPUT_INT_CHAN_INFO))
                operations += 1;
    }   

    isize ticks_after = chan_perf_counter();
    
    printf("%s completed\n", context->is_consumer ? "consumer" : "producer");

    chan_atomic_store64(&context->operations, operations);
    chan_atomic_store64(&context->ticks_before, ticks_before);
    chan_atomic_store64(&context->ticks_after, ticks_after);
    wait_group_pop(context->wait_group_done, 1, SYNC_WAIT_BLOCK);
}

void test_channel_throughput(isize buffer_capacity, isize a_count, isize b_count, double seconds, bool no_bubble)
{
    isize thread_count = a_count + b_count;
    _Test_Channel_Throughput_Thread threads[TEST_CHAN_MAX_THREADS] = {0};
    
    Channel* chan = channel_init(buffer_capacity, THROUGHPUT_INT_CHAN_INFO);

    uint32_t run_status = 0;
    Wait_Group wait_group_done = 0;
    Wait_Group wait_group_started = 0;
    wait_group_push(&wait_group_done, thread_count);
    wait_group_push(&wait_group_started, thread_count);

    for(isize i = 0; i < thread_count; i++)
    {
        threads[i].chan = chan;
        threads[i].run_status = &run_status;
        threads[i].wait_group_done = &wait_group_done;
        threads[i].wait_group_started = &wait_group_started;
        threads[i].is_consumer = i >= a_count;
        threads[i].is_no_bubble = no_bubble;

        chan_start_thread(_test_channel_throughput_runner, threads + i);
    }

    sync_set_and_wake(&run_status, _TEST_CHANNEL_STATUS_RUN, SYNC_WAIT_SPIN);

    chan_sleep(seconds);
    channel_close(chan, THROUGHPUT_INT_CHAN_INFO);
    
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

#include <math.h>
void test_channel(double total_time)
{
    test_channel_throughput(256, 6, 6, 10, false);
    //test_channel_throughput(256, 6, 6, 10, true);

    //exit(0);
    //TEST(channel_ticket_is_less(0, 1));
    //TEST(channel_ticket_is_less(1, 2));
    //TEST(channel_ticket_is_less(5, 2) == false);
    //TEST(channel_ticket_is_less(UINT64_MAX/4, UINT64_MAX/2));
    //TEST(channel_ticket_is_less(UINT64_MAX/2, UINT64_MAX/2 + 100));
    //TEST(channel_ticket_is_less(UINT64_MAX/2 + 100, UINT64_MAX/2) == false);

    //test_channel_sequential(1);
    //test_channel_sequential(10);
    //test_channel_sequential(100);
    //test_channel_sequential(1000);
    
    //for(int i = 0; i < 10; i++)
    //    test_channel_cycle(100, 4, 1, 5, 1, true, true);

    exit(0);
    test_channel_cycle(100, 4, 4, 10, 1, true, true);
    test_channel_linearization(100, 4, 4, 10, 3, true, true);

    bool main_print = true;
    bool thread_print = false;
    
    double func_start = (double) clock()/CLOCKS_PER_SEC;
    for(isize test_i = 0;; test_i++)
    {
        double now = (double) clock()/CLOCKS_PER_SEC;
        double ellapsed = now - func_start;
        double remaining = total_time - ellapsed;
        if(remaining <= 0)
            break;

        double test_duration = (double)rand()/RAND_MAX*2 + 0.1;
        if(test_duration > remaining)
            test_duration = remaining;

        isize threads_a = (isize) pow(2, (double)rand()/RAND_MAX*5);
        isize threads_b = (isize) pow(2, (double)rand()/RAND_MAX*5);
        isize capacity = rand() % 1000 + 1;
        isize stop_count = 3;

        //Higher affinity towards boundary values
        if(rand() % 20 == 0)
            capacity = 1;
        if(rand() % 20 == 0)
            threads_a = 1;
        if(rand() % 20 == 0)
            threads_b = 1;
        
        test_channel_cycle(capacity, threads_a, threads_b, stop_count, test_duration, main_print, thread_print);
        test_channel_linearization(capacity, threads_a, threads_b, stop_count, test_duration, main_print, thread_print);
    }
}

