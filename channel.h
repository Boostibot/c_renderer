#pragma once

#include "lib/platform.h"
#include <string.h>

#ifdef __cplusplus
    #define _CHAN_ALIGNED(align)  alignas(align)    
#else
#endif

#ifndef _CHAN_CUSTOM
    #define _CHAN_CACHE_LINE 64
#endif

#if defined(_MSC_VER)
    #define _CHAN_INLINE_ALWAYS   __forceinline
    #define _CHAN_INLINE_NEVER    __declspec(noinline)
    #define _CHAN_ALIGNED(bytes)  __declspec(align(bytes))
#elif defined(__GNUC__) || defined(__clang__)
    #define _CHAN_INLINE_ALWAYS   __attribute__((always_inline)) inline
    #define _CHAN_INLINE_NEVER    __attribute__((noinline))
    #define _CHAN
#else
    #define _CHAN_INLINE_ALWAYS   inline
    #define _CHAN_INLINE_NEVER
    #define _CHAN_ALIGNED(bytes)  _Align(bytes)
#endif

typedef int64_t isize;

typedef bool (*Sync_Wait_Func)(volatile void* state, uint32_t undesired, isize timeout_or_minus_one_if_infinite);
typedef void (*Sync_Wake_Func)(volatile void* state);

typedef struct Channel_Info {
    isize item_size;
    Sync_Wait_Func wait;
    Sync_Wake_Func wake;
} Channel_Info;

typedef struct Channel {
    _CHAN_ALIGNED(_CHAN_CACHE_LINE) 
    volatile uint64_t head;
    uint64_t _head_pad[7];

    _CHAN_ALIGNED(_CHAN_CACHE_LINE) 
    volatile uint64_t tail;
    uint64_t _tail_pad[7];

    _CHAN_ALIGNED(_CHAN_CACHE_LINE) 
    Channel_Info info;
    uint64_t capacity; 
    uint8_t* items; 
    uint32_t* ids; 
    volatile uint32_t ref_count; 
    volatile uint32_t closed;
    volatile uint32_t allocated;
    uint8_t _shared_pad[4];
} Channel;

typedef enum Channel_Res {
    CHANNEL_OK = 0,
    CHANNEL_CLOSED = 1,
    CHANNEL_FULL = 2,
    CHANNEL_EMPTY = 3,
    CHANNEL_LOST_RACE = 4,
} Channel_Res;

//Allocates a new channel on the heap and returns pointer to it. If the allocation fails returns 0.
//capacity >= 0. If capacity == 0 then creates an "unbuffered" channel which acts as unbuffered channel in Go.
// That is there is just one slot in the channel and after a call to channel_push the calling thread is
// waiting until channel_pop is called (or until the channel is closed).
Channel* channel_init(isize capacity, Channel_Info info);

//Increments the ref count of the channel. Returns the passed in channel.
Channel* channel_share(Channel* chan);

//Decrements the ref count and if it reaches zero deinitializes the channel. 
//If the channel was allocated through channel_init frees it.
//If it was created through channel_init_custom only memsets the channel to zero.
int32_t channel_deinit(Channel* chan);
//Calls channel_deinit then channel_close
int32_t channel_deinit_close(Channel* chan);
//Waits for the channels ref count to hit 1.
void channel_wait_for_exclusivity(const Channel* chan, Channel_Info info);

//Initializes the channel using the passed in arguments. 
// This can be used to allocate a channel on the stack or use a different allocator such as an arena.
// The items need to point to at least capacity*info.item_size bytes (info.item_size can be zero in which case items can be NULL). 
// The ids need to point to an array of at least capacity uint64_t's.
void channel_init_custom(Channel* chan, void* items, uint64_t* ids, isize capacity, Channel_Info info);

//Pushes an item. item needs to point to an array of at least info.item_size bytes.
//If the operation succeeds returns true.
//If the channel is full, waits until its not. 
//If the channel is closed or becomes closed while waiting returns false.
bool channel_push(Channel* chan, const void* item, Channel_Info info);
//Pushes an item. item needs to point to an array of at least info.item_size bytes.
//If the operation succeeds returns true.
//If the channel is empty, waits until its not. 
//If the channel is closed or becomes closed while waiting returns false.
bool channel_pop(Channel* chan, void* item, Channel_Info info);

//Attempts to push an item stored in item without blocking. 
// item needs to point to an array of at least info.item_size bytes.
// If succeeds returns CHANNEL_OK (0). 
// If the channel is closed returns CHANNEL_CLOSED (1).
// If the channel is full returns CHANNEL_FULL(2)
// If lost a race to concurrent call to this function returns CHANNEL_LOST_RACE (4).
Channel_Res channel_try_push_weak(Channel* chan, const void* item, Channel_Info info);
//Attempts to pop an item storing it in item without blocking. 
// item needs to point to an array of at least info.item_size bytes.
// If succeeds returns CHANNEL_OK (0). 
// If the channel is closed returns CHANNEL_CLOSED (1).
// If the channel is empty returns CHANNEL_FULL(3)
// If lost a race to concurrent call to this function returns CHANNEL_LOST_RACE (4).
Channel_Res channel_try_pop_weak(Channel* chan, void* item, Channel_Info info); 

//Same as channel_try_push_weak but never returns CHANNEL_LOST_RACE (1).
//Instead retries until the operation completes successfully or some other error appears.
Channel_Res channel_try_push(Channel* chan, const void* item, Channel_Info info);
//Same as channel_try_pop_weak but never returns CHANNEL_LOST_RACE (1).
//Instead retries until the operation completes successfully or some other error appears.
Channel_Res channel_try_pop(Channel* chan, void* item, Channel_Info info);

//Closes the channel causing all threads waiting in channel_push and channel_pop to return with failure (false). 
//Subsequent operations on the channel will fail immediately.
//Returns whether the channel was already closed.
bool channel_close(Channel* chan);
//Reopens an already closed channel, making it possible to use it again.
// This function is unsafe and must only be called when we are sure we are the only thread
// still referencing the channel. Failure to do so can result in items in the channel being
// skipped and other items being processed twice.
//Returns whether the channel was still open.
bool channel_reopen(Channel* chan);
bool channel_is_closed(const Channel* chan); 

//Returns upper bound to the distance between head and tail indices. 
//This can be used to approximately check the number of blocked threads or get the number of items in the channel.
isize channel_signed_distance(const Channel* chan);

//Returns upper bound to the number of items in the channel. Returned value is in range [0, chan->capacity]
isize channel_count(const Channel* chan);
bool channel_is_empty(const Channel* chan);

//=========================================
// Ticket interface 
//========================================

// These functions work just like their regular counterparts but can also return the ticket of the completed operation.
// The ticket can be used to signal completion using the channel_ticket_is_after function. 
//
// For example when producer pushes into a channel and wants to wait for the consumer to process the pushed item, 
// it uses these functions to also obtain a ticket. The consumer also pops and takes and recieves a ticket. 
// After each processed item it sets its ticket to global variable. The producer thus simply waits for the 
// the global variable to becomes at least its ticket using channel_ticket_wait_for().

//Returns whether ticket_a came after ticket_b. 
//Unless unsigned number overflow happens this is just `ticket_a > ticket_b`.
bool channel_ticket_is_after(uint64_t ticket_a, uint64_t ticket_b);

bool channel_ticket_push(Channel* chan, const void* item, uint64_t* ticket_or_null, Channel_Info info);
bool channel_ticket_pop(Channel* chan, void* item, uint64_t* ticket_or_null, Channel_Info info);
Channel_Res channel_ticket_try_push(Channel* chan, const void* item, uint64_t* ticket_or_null, Channel_Info info);
Channel_Res channel_ticket_try_pop(Channel* chan, void* item, uint64_t* ticket_or_null, Channel_Info info);
Channel_Res channel_ticket_try_push_weak(Channel* chan, const void* item, uint64_t* ticket_or_null, Channel_Info info);
Channel_Res channel_ticket_try_pop_weak(Channel* chan, void* item, uint64_t* ticket_or_null, Channel_Info info);


_CHAN_INLINE_ALWAYS static bool     chan_wait_block(volatile void* state, uint32_t undesired, isize timeout_or_minus_one_if_infinite);
_CHAN_INLINE_ALWAYS static bool     chan_wait_yield(volatile void* state, uint32_t undesired, isize timeout_or_minus_one_if_infinite);
_CHAN_INLINE_ALWAYS static bool     chan_wait_pause(volatile void* state, uint32_t undesired, isize timeout_or_minus_one_if_infinite);
_CHAN_INLINE_ALWAYS static bool     chan_wait_noop(volatile void* state, uint32_t undesired, isize timeout_or_minus_one_if_infinite);

_CHAN_INLINE_ALWAYS static void     chan_wake_block(volatile void* state);
_CHAN_INLINE_ALWAYS static void     chan_wake_noop(volatile void* state);

#ifdef __cplusplus
    #define _CHAN_SINIT(T) T
#else
    #define _CHAN_SINIT(T) (T)
#endif

#define _CHAN_WAITING_BIT           ((uint32_t) 1)
#define _CHAN_PAIRITY_BIT           ((uint32_t) 2)
#define _CHAN_MAX_DISTANCE          (UINT64_MAX/2)

uint64_t _channel_get_target(uint64_t ticket, uint64_t capacity)
{
    return ticket % capacity;
}

uint32_t _channel_get_id(const Channel* chan, uint64_t ticket)
{
    return ((uint32_t) (ticket / chan->capacity)*4);
}

bool _channel_id_equals(uint32_t id1, uint32_t id2)
{
    return ((id1 ^ id2) & ~_CHAN_WAITING_BIT) == 0;
}

void _channel_advance_id(Channel* chan, uint64_t target, uint32_t id, Channel_Info info)
{
    uint32_t* id_ptr = &chan->ids[target];
    
    uint32_t new_id = (uint32_t) (id + _CHAN_PAIRITY_BIT);
    if(info.wake == chan_wake_noop)
        platform_atomic_store32(id_ptr, new_id);
    else
    {
        uint32_t prev_id = platform_atomic_exchange32(id_ptr, new_id);
        ASSERT(_channel_id_equals(prev_id + _CHAN_PAIRITY_BIT, new_id));
        if(prev_id & _CHAN_WAITING_BIT)
            info.wake(id_ptr);
    }
}

bool channel_close(Channel* chan) 
{
    bool out = false;
    if(chan)
    {
        out = platform_atomic_cas32(&chan->closed, false, true);
        
        //If we closed and have wake up mechanisms
        Sync_Wake_Func wake = chan->info.wake;
        if(out && wake != chan_wake_noop)
        {
            //iterate all slots and wake up blocked threads
            for(uint64_t i = 0; i < chan->capacity; i++)
            {
                uint64_t prev_id = platform_atomic_and32(&chan->ids[i], ~_CHAN_WAITING_BIT);
                if(prev_id & _CHAN_WAITING_BIT)
                    wake(&chan->ids[i]);
            }
        }
    }

    return out;
}

bool channel_reopen(Channel* chan) 
{
    if(chan)
    {
        enum {OPENING = 2};
        if(platform_atomic_cas32(&chan->closed, true, OPENING))
        {
            

            platform_atomic_store32(&chan->closed, false);
        }
    }
}

bool channel_ticket_is_after(uint64_t ticket_a, uint64_t ticket_b)
{
    return (ticket_a > ticket_b) || (ticket_b - ticket_a > _CHAN_MAX_DISTANCE);
}

bool channel_ticket_push(Channel* chan, const void* item, uint64_t* out_ticket_or_null, Channel_Info info) 
{
    if(platform_atomic_load32(&chan->closed))
        return false;

    uint64_t ticket = platform_atomic_add64(&chan->tail, 1);
    uint64_t target = _channel_get_target(ticket, chan->capacity);
    uint32_t id = _channel_get_id(chan, ticket);
    
    for(;;) {
        uint32_t curr = chan->ids[target];
        if(_channel_id_equals(curr, id))
            break;

        if(platform_atomic_load32(&chan->closed))
        {
            //platform_atomic_sub64(&chan->tail, 1);
            return false;
        }
        
        if(info.wake != chan_wake_noop) {
            platform_atomic_or32(&chan->ids[target], _CHAN_WAITING_BIT);
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

bool channel_ticket_pop(Channel* chan, void* item, uint64_t* out_ticket_or_null, Channel_Info info) 
{
    
    if(platform_atomic_load32(&chan->closed))
        return false;

    uint64_t ticket = platform_atomic_add64(&chan->head,1);
    uint64_t target = _channel_get_target(ticket, chan->capacity);
    uint32_t id = _channel_get_id(chan, ticket) + _CHAN_PAIRITY_BIT;
    
    for(;;) {
        uint32_t curr = chan->ids[target];
        if(_channel_id_equals(curr, id))
            break;
            
        if(platform_atomic_load32(&chan->closed))
        {
            //platform_atomic_sub64(&chan->head, 1);
            return false;
        }
        
        if(info.wake != chan_wake_noop) {
            platform_atomic_or32(&chan->ids[target], _CHAN_WAITING_BIT);
            curr |= _CHAN_WAITING_BIT;
        }

        info.wait(&chan->ids[target], curr, -1);
    }
    
    memcpy(item, chan->items + target*info.item_size, info.item_size);
    _channel_advance_id(chan, target, id, info);
    if(out_ticket_or_null)
        *out_ticket_or_null = ticket;

    return true;
}

Channel_Res channel_ticket_try_push_weak(Channel* chan, const void* item, uint64_t* out_ticket_or_null, Channel_Info info) 
{
    if(platform_atomic_load32(&chan->closed))
        return CHANNEL_CLOSED;

    uint64_t ticket = platform_atomic_load64(&chan->tail);
    uint64_t target = _channel_get_target(ticket, chan->capacity);
    uint32_t id = _channel_get_id(chan, ticket);

    uint32_t curr_id = platform_atomic_load32(&chan->ids[target]);
    if(_channel_id_equals(curr_id, id) == false)
        return CHANNEL_FULL;
    if(platform_atomic_cas64(&chan->tail, ticket, ticket+1) == false)
        return CHANNEL_LOST_RACE;

    memcpy(chan->items + target*info.item_size, item, info.item_size);
    _channel_advance_id(chan, target, id, info);
    if(out_ticket_or_null)
        *out_ticket_or_null = ticket;

    return CHANNEL_OK;
}

Channel_Res channel_ticket_try_pop_weak(Channel* chan, void* item, uint64_t* out_ticket_or_null, Channel_Info info) 
{
    if(platform_atomic_load32(&chan->closed))
        return CHANNEL_CLOSED;

    uint64_t ticket = platform_atomic_load64(&chan->head);
    uint64_t target = _channel_get_target(ticket, chan->capacity);
    uint32_t id = _channel_get_id(chan, ticket) + _CHAN_PAIRITY_BIT;
    
    uint32_t curr_id = platform_atomic_load32(&chan->ids[target]);
    if(_channel_id_equals(curr_id, id) == false)
        return CHANNEL_EMPTY;
    if(platform_atomic_cas64(&chan->head, ticket, ticket+1) == false)
        return CHANNEL_LOST_RACE;
        
    memcpy(item, chan->items + target*info.item_size, info.item_size);
    _channel_advance_id(chan, target, id, info);
    if(out_ticket_or_null)
        *out_ticket_or_null = ticket;

    return CHANNEL_OK;
}

Channel_Res channel_ticket_try_push(Channel* chan, const void* item, uint64_t* out_ticket_or_null, Channel_Info info)
{
    for(;;) {
        Channel_Res res = channel_ticket_try_push_weak(chan, item, out_ticket_or_null, info);
        if(res != CHANNEL_LOST_RACE)
            return res;
    }
}

Channel_Res channel_ticket_try_pop(Channel* chan, void* item, uint64_t* out_ticket_or_null, Channel_Info info)
{
    for(;;) {
        Channel_Res res = channel_ticket_try_pop_weak(chan, item, out_ticket_or_null, info);
        if(res != CHANNEL_LOST_RACE)
            return res;
    }
}

bool channel_push(Channel* chan, const void* item, Channel_Info info)
{
    return channel_ticket_push(chan, item, NULL, info);
}
bool channel_pop(Channel* chan, void* item, Channel_Info info)
{
    return channel_ticket_pop(chan, item, NULL, info);
}
Channel_Res channel_try_push_weak(Channel* chan, const void* item, Channel_Info info)
{
    return channel_ticket_try_push_weak(chan, item, NULL, info);
}
Channel_Res channel_try_pop_weak(Channel* chan, void* item, Channel_Info info)
{
    return channel_ticket_try_pop_weak(chan, item, NULL, info);
}
Channel_Res channel_try_push(Channel* chan, const void* item, Channel_Info info)
{
    return channel_ticket_try_push(chan, item, NULL, info);
}
Channel_Res channel_try_pop(Channel* chan, void* item, Channel_Info info)
{
    return channel_ticket_try_pop(chan, item, NULL, info);
}

bool channel_is_closed(const Channel* chan) 
{
    return platform_atomic_load32(&chan->closed) > 0;
}

isize channel_signed_distance(const Channel* chan)
{
    if(platform_atomic_load32(&chan->closed))
        return 0;

    uint64_t head = chan->head;
    uint64_t tail = chan->tail;

    uint64_t diff = tail - head;
    return (isize) diff;
}

isize channel_count(const Channel* chan) 
{
    isize dist = channel_signed_distance(chan);
    if(dist <= 0)
        return 0;
    if(dist >= (isize) chan->capacity)
        return (isize) chan->capacity;
    else
        return dist;
}

bool channel_is_empty(const Channel* chan) 
{
    return channel_signed_distance(chan) <= 0;
}

void channel_init_custom(Channel* chan, void* items, uint32_t* ids, isize capacity, Channel_Info info)
{
    assert(ids);
    assert(capacity > 0 && "must be power of two");

    memset(chan, 0, sizeof* chan);
    chan->items = (uint8_t*) items;
    chan->ids = ids;
    chan->capacity = (uint64_t) capacity; 
    chan->info = info;
    chan->ref_count = 2;

    //essentially a memory fence with respect to any other function
    platform_atomic_store64(&chan->head, 0);
    platform_atomic_store64(&chan->tail, 0);
    platform_atomic_store32(&chan->closed, false);
}

Channel* channel_init(isize capacity, Channel_Info info)
{
    isize total_size = sizeof(Channel) + capacity*sizeof(uint32_t) + capacity*info.item_size;

    Channel* chan = (Channel*) platform_heap_reallocate(total_size, NULL, _CHAN_CACHE_LINE);
    uint32_t* ids = (uint32_t*) (void*) (chan + 1);
    void* items = ids + capacity;
    memset(ids, 0, capacity*sizeof *ids);

    if(chan)
    {
        channel_init_custom(chan, items, ids, capacity, info);
        platform_atomic_store32(&chan->allocated, true);
    }

    return chan;
}

Channel* channel_share(Channel* chan)
{
    uint32_t prev = platform_atomic_add32(&chan->ref_count, 2);
    if(chan->info.wake != chan_wake_noop && (prev & _CHAN_WAITING_BIT))
    {
        platform_atomic_and32(&chan->ref_count, ~_CHAN_WAITING_BIT);
        chan->info.wake(&chan->ref_count);
    }

    return chan;
}

int32_t channel_deinit(Channel* chan)
{
    if(chan == NULL)
        return 0;

    int32_t refs = (int32_t) platform_atomic_sub32(&chan->ref_count, 2) - 2;
    if(chan->info.wake != chan_wake_noop && (refs & _CHAN_WAITING_BIT))
    {
        platform_atomic_and32(&chan->ref_count, ~_CHAN_WAITING_BIT);
        chan->info.wake(&chan->ref_count);
    }

    if(refs == 0)
    {
        if(platform_atomic_load32(&chan->allocated))
            platform_heap_reallocate(0, chan, _CHAN_CACHE_LINE);
        else
            memset(chan, 0, sizeof *chan);
    }

    return refs;
}

int32_t channel_deinit_close(Channel* chan)
{
    int32_t refs = channel_deinit(chan);
    if(refs > 0)
        channel_close(chan);
    return refs;
}

void channel_wait_for_exclusivity(const Channel* chan, Channel_Info info)
{
    for(;;) {
        uint32_t curr = platform_atomic_load32(&chan->ref_count);

        ASSERT(curr < UINT32_MAX / 2);
        if(curr <= 2 || curr >= UINT32_MAX / 2) //we exit on undeflows as well but assert.
            break;

        if(chan->info.wake != chan_wake_noop) {
            platform_atomic_or32((void*) &chan->ref_count, _CHAN_WAITING_BIT);
            curr |= _CHAN_WAITING_BIT;
        }

        info.wait((void*) &chan->ref_count, curr, -1);
    }
}

typedef struct Sync_Wait {
    Sync_Wait_Func wait;
    Sync_Wake_Func wake;
    uint32_t notify_bit;
    uint32_t _;
} Sync_Wait;

static bool sync_wait(volatile void* state, uint32_t current, isize timeout, Sync_Wait wait)
{
    if(wait.notify_bit) 
    {
        platform_atomic_or32(state, wait.notify_bit);
        current |= wait.notify_bit;
    }
    return wait.wait((void*) state, current, timeout);
}

static void sync_wake(volatile void* state, uint32_t prev, Sync_Wait wait)
{
    if(wait.notify_bit)
    {
        if(prev & wait.notify_bit)
            wait.wake((void*) state);
    }
    else
        wait.wake((void*) state);
}

static uint32_t sync_wait_for_equal(volatile void* state, uint32_t desired, Sync_Wait wait)
{
    for(;;) {
        uint32_t current = platform_atomic_load32(state);
        if((current & ~wait.notify_bit) == (desired & ~wait.notify_bit))
            return current & ~wait.notify_bit;

        sync_wait(state, current, -1, wait);
    }
}

static uint32_t sync_wait_for_greater(volatile void* state, uint32_t desired, Sync_Wait wait)
{
    for(;;) {
        uint32_t current = platform_atomic_load32(state);
        if((current & ~wait.notify_bit) > (desired & ~wait.notify_bit))
            return current & ~wait.notify_bit;
            
        sync_wait(state, current, -1, wait);
    }
}

static uint32_t sync_wait_for_smaller(volatile void* state, uint32_t desired, Sync_Wait wait)
{
    for(;;) {
        uint32_t current = platform_atomic_load32(state);
        if((current & ~wait.notify_bit) < (desired & ~wait.notify_bit))
            return current & ~wait.notify_bit;
            
        sync_wait(state, current, -1, wait);
    }
}

typedef struct Timeout_Wait {
    double rdtsc_freq_s;
    isize rdtsc_freq_ms;
    isize wait_ticks;
    isize start_ticks;
} Timeout_Wait;

Timeout_Wait timeout_wait_start(double wait)
{
    static double rdtsc_freq_s = 0;
    static isize rdtsc_freq_ms = 0;
    if(rdtsc_freq_s == 0)
    {
        rdtsc_freq_s = (double) platform_rdtsc_frequency();
        rdtsc_freq_ms = platform_rdtsc_frequency()/1000;
    }

    Timeout_Wait out = {0};
    out.rdtsc_freq_s = rdtsc_freq_s;
    out.rdtsc_freq_ms = rdtsc_freq_ms;
    out.wait_ticks = (isize) (wait*rdtsc_freq_s);
    out.start_ticks = platform_rdtsc();
    return out;
}

bool timeout_wait(volatile void* state, uint32_t current, Timeout_Wait timeout, Sync_Wait wait)
{
    isize curr_ticks = platform_rdtsc();
    isize ellapsed_ticks = curr_ticks - timeout.start_ticks;
    if(ellapsed_ticks >= timeout.wait_ticks)
        return false;
        
    isize wait_ms = (timeout.wait_ticks - ellapsed_ticks)/timeout.rdtsc_freq_ms;
    return sync_wait(state, current, wait_ms, wait);
}

#define TEST_INT_CHAN_INFO _CHAN_SINIT(Channel_Info){sizeof(int), chan_wait_block, chan_wake_block}
#define TEST_CYCLE_RUN_INFO _CHAN_SINIT(Sync_Wait){chan_wait_block, chan_wake_block, 1u << 31}


#if 0
PLATFORM_INTRINSIC void platform_processor_pause()
{
    #ifdef _PLATFORM_MSVC_X86
        _mm_pause();
    #else
        __yield();
    #endif
}
    
PLATFORM_INTRINSIC uint64_t platform_atomic_load64(const volatile void* target)
{
    return (uint64_t) __iso_volatile_load64((const long long*) target);
}
PLATFORM_INTRINSIC uint32_t platform_atomic_load32(const volatile void* target)
{
    return (uint32_t) __iso_volatile_load32((const volatile int*) target);
}

PLATFORM_INTRINSIC void platform_atomic_store64(volatile void* target, uint64_t value)
{
    __iso_volatile_store64((long long*) target, (long long) value);
}
PLATFORM_INTRINSIC void platform_atomic_store32(volatile void* target, uint32_t value)
{
    __iso_volatile_store32((volatile int*) target, (int) value);
}
    
PLATFORM_INTRINSIC bool platform_atomic_cas64(volatile void* target, uint64_t old_value, uint64_t new_value)
{
    return _InterlockedCompareExchange64((long long*) target, (long long) new_value, (long long) old_value) == (long long) old_value;
}
PLATFORM_INTRINSIC bool platform_atomic_cas32(volatile void* target, uint32_t old_value, uint32_t new_value)
{
    return _InterlockedCompareExchange((volatile long*) target, (long) new_value, (long) old_value) == (long) old_value;
}

PLATFORM_INTRINSIC uint64_t platform_atomic_exchange64(volatile void* target, uint64_t value)
{
    return (uint64_t) _InterlockedExchange64((long long*) target, (long long) value);
}
PLATFORM_INTRINSIC uint32_t platform_atomic_exchange32(volatile void* target, uint32_t value)
{
    return (uint32_t) _InterlockedExchange((long*) target, (long) value);
}
    
PLATFORM_INTRINSIC uint64_t platform_atomic_add64(volatile void* target, uint64_t value)
{
    return (uint64_t) _InterlockedExchangeAdd64((long long*) (void*) target, (long long) value);
}
PLATFORM_INTRINSIC uint64_t platform_atomic_sub64(volatile void* target, uint64_t value)
{
    return platform_atomic_add64(target, (uint64_t) -(int64_t) value);
}
   
PLATFORM_INTRINSIC uint64_t platform_atomic_or64(volatile void* target, uint64_t value)
{
    return (uint64_t) _InterlockedOr64((long long*) (void*) target, (long long) value);
}
PLATFORM_INTRINSIC uint32_t platform_atomic_or32(volatile void* target, uint32_t value)
{
    return (uint32_t) _InterlockedOr((long*) (void*) target, (long) value);
}
PLATFORM_INTRINSIC uint64_t platform_atomic_and64(volatile void* target, uint64_t value)
{
    return (uint64_t) _InterlockedAnd64((long long*) (void*) target, (long long) value);
}
PLATFORM_INTRINSIC uint32_t platform_atomic_and32(volatile void* target, uint32_t value)
{
    return (uint32_t) _InterlockedAnd((long*) (void*) target, (long) value);
}
#endif


_CHAN_INLINE_ALWAYS static bool     chan_wait_block(volatile void* state, uint32_t undesired, isize timeout_or_minus_one_if_infinite)
{
    return platform_futex_wait(state, undesired, timeout_or_minus_one_if_infinite);
}
_CHAN_INLINE_ALWAYS static bool     chan_wait_yield(volatile void* state, uint32_t undesired, isize timeout_or_minus_one_if_infinite)
{
    (void) state; (void) undesired; (void) timeout_or_minus_one_if_infinite;
    platform_thread_yield();
    return true;
}
_CHAN_INLINE_ALWAYS static bool     chan_wait_pause(volatile void* state, uint32_t undesired, isize timeout_or_minus_one_if_infinite)
{
    (void) state; (void) undesired; (void) timeout_or_minus_one_if_infinite;
    platform_processor_pause();
    return true;
}
_CHAN_INLINE_ALWAYS static bool     chan_wait_noop(volatile void* state, uint32_t undesired, isize timeout_or_minus_one_if_infinite)
{
    (void) state; (void) undesired; (void) timeout_or_minus_one_if_infinite;
    return true;
}

_CHAN_INLINE_ALWAYS static void     chan_wake_block(volatile void* state)
{
    platform_futex_wake_all(state);
}
_CHAN_INLINE_ALWAYS static void     chan_wake_noop(volatile void* state)
{
    (void) state;
}

#include "lib/assert.h"
typedef struct _Test_Channel_Cycle_Thread {
    Channel* a;
    Channel* b;
    Channel* lost;
    char name[32];
    uint32_t* run;
} _Test_Channel_Cycle_Thread;

int _test_channel_cycle_runner(void* arg)
{
    _Test_Channel_Cycle_Thread* context = (_Test_Channel_Cycle_Thread*) arg;
    printf("%s created!\n", context->name);
    sync_wait_for_equal(context->run, true, TEST_CYCLE_RUN_INFO);
    printf("%s run!\n", context->name);

    char buffer[1024] = {0};
    for(;;)
    {
        if(channel_pop(context->b, buffer, context->b->info) == false)
            break;
        
        if(channel_push(context->a, buffer, context->a->info) == false)
        {
            printf("%s lost value!\n", context->name);
            channel_push(context->lost, buffer, context->lost->info);
            break;
        }
    }

    channel_deinit(context->a);
    channel_deinit(context->b);
    printf("%s exited!\n", context->name);
    return 0;
}

int _int_comp_fn(const void* a, const void* b)
{
    return (*(int*) a > *(int*) b) - (*(int*) a < *(int*) b);
}

//WAIT_BLOCK_BIT(1)

void test_channel_cycle(isize buffer_capacity, isize a_count, isize b_count, double seconds)
{
    enum { MAX_THREADS = 256 };

    Channel* a_chan = channel_init(buffer_capacity, TEST_INT_CHAN_INFO);
    Channel* b_chan = channel_init(buffer_capacity, TEST_INT_CHAN_INFO);
    Channel* lost_chan = channel_init(a_count + b_count, TEST_INT_CHAN_INFO);

    Platform_Thread a_threads[MAX_THREADS] = {0};
    Platform_Thread b_threads[MAX_THREADS] = {0};
    
    for(int i = 0; i < a_chan->capacity; i++)
        channel_push(a_chan, &i, TEST_INT_CHAN_INFO);

    uint32_t run = false;
    for(isize i = 0; i < a_count; i++)
    {
        _Test_Channel_Cycle_Thread state = {0};
        state.a = channel_share(a_chan);
        state.b = channel_share(b_chan);
        state.lost = channel_share(lost_chan);
        state.run = &run;
        snprintf(state.name, sizeof state.name, "A -> B #%lli", i);
        Platform_Error err = platform_thread_launch(a_threads + i, 0, _test_channel_cycle_runner, &state, sizeof state);
        TEST(err == 0);
    }
    
    for(isize i = 0; i < b_count; i++)
    {
        _Test_Channel_Cycle_Thread state = {0};
        state.b = channel_share(a_chan);
        state.a = channel_share(b_chan);
        state.lost = channel_share(lost_chan);
        state.run = &run;
        snprintf(state.name, sizeof state.name, "B -> A #%lli", i);
        Platform_Error err = platform_thread_launch(b_threads + i, 0, _test_channel_cycle_runner, &state, sizeof state);
        TEST(err == 0);
    }
    
    platform_thread_sleep(1);

    uint32_t prev = platform_atomic_exchange32(&run, true);
    sync_wake(&run, prev, TEST_CYCLE_RUN_INFO);
    
    platform_thread_sleep((isize) (seconds*1000));
    
    channel_close(a_chan);
    channel_close(b_chan);

    channel_wait_for_exclusivity(a_chan, TEST_INT_CHAN_INFO);
    channel_wait_for_exclusivity(b_chan, TEST_INT_CHAN_INFO);

    isize a_chan_count = channel_count(a_chan);
    isize b_chan_count = channel_count(b_chan);
    isize lost_count = channel_count(lost_chan);

    channel_reopen(a_chan);
    channel_reopen(b_chan);

    isize count_sum = a_chan_count + b_chan_count + lost_count;
    TEST(count_sum == buffer_capacity);

    isize everything_i = 0;
    int* everything = (int*) malloc(buffer_capacity*sizeof(int));
    for(isize i = 0; i < a_chan_count; i++)
    {
        int x = 0;
        Channel_Res res = channel_try_pop(a_chan, &x, TEST_INT_CHAN_INFO);
        if(res != 0)
            res = channel_try_pop(a_chan, &x, TEST_INT_CHAN_INFO);
        TEST(res == 0);
        everything[everything_i++] = x;
    }

    for(isize i = 0; i < b_chan_count; i++)
    {
        int x = 0;
        Channel_Res res = channel_try_pop(b_chan, &x, TEST_INT_CHAN_INFO);
        if(res != 0)
            res = channel_try_pop(b_chan, &x, TEST_INT_CHAN_INFO);
        TEST(res == 0);
        everything[everything_i++] = x;
    }

    for(isize i = 0; i < lost_count; i++)
    {
        int x = 0;
        Channel_Res res = channel_try_pop(lost_chan, &x, TEST_INT_CHAN_INFO);
        if(res != 0)
            res = channel_try_pop(lost_chan, &x, TEST_INT_CHAN_INFO);
        TEST(res == 0);
        everything[everything_i++] = x;
    }

    TEST(everything_i == buffer_capacity);
    qsort(everything, buffer_capacity, sizeof(int), _int_comp_fn);
    for(isize i = 0; i < buffer_capacity; i++)
        TEST(everything[i] == i);

    free(everything);

    channel_deinit(a_chan);
    channel_deinit(b_chan);
}

void test_channel_sequential(isize capacity)
{
    Channel* chan = channel_init(capacity, TEST_INT_CHAN_INFO);
    
    for(int i = 0; i < chan->capacity; i++)
    {
        bool res = channel_push(chan, &i, TEST_INT_CHAN_INFO);
        TEST(res);
    }
    
    for(int i = 0; i < chan->capacity; i++)
    {
        int popped = 0;
        bool res = channel_pop(chan, &popped, TEST_INT_CHAN_INFO);
        TEST(res);
        TEST(popped == i);
    }

    channel_deinit_close(chan);
}

void test_channel()
{
    test_channel_sequential(1);
    test_channel_sequential(10);
    test_channel_sequential(100);
    test_channel_sequential(1000);
    
    //test_channel_cycle(1, 1, 1, 2);
    //test_channel_cycle(10, 1, 1, 2);
    //test_channel_cycle(100, 1, 1, 2);
    
    for(;;) {
        test_channel_cycle(1, 4, 4, 2);
        test_channel_cycle(10, 4, 4, 2);
        test_channel_cycle(100, 4, 4, 2);
    }
}


#if defined(_MSC_VER)
    #include <stdio.h>
    #include <intrin.h>
    _CHAN_INLINE_ALWAYS static void chan_pause()
    {
        #ifdef _PLATFORM_MSVC_X86
            _mm_pause();
        #else
            __yield();
        #endif
    }
#endif