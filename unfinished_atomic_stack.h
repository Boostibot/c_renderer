#pragma once

#include "lib/channel.h"
#include "lib/sync.h"

#define CHAN_ATOMIC(t) t

typedef union Gen_Index {
    struct {
        uint32_t index;
        uint32_t generation;
    };
    uint64_t combined;
} Gen_Index;

typedef struct Atomic_Stack_Slot {
    uint32_t next;
    uint8_t data[];
} Atomic_Stack_Slot;

Atomic_Stack_Slot* atomic_stack_slot(void* data, uint32_t index, uint32_t slot_size)
{
    uint8_t* slot = (uint8_t*) data + (uint64_t) index*slot_size;
    return (Atomic_Stack_Slot*) (void*) slot;
}

void atomic_stack_push(CHAN_ATOMIC(void*)* data_ptr, CHAN_ATOMIC(Gen_Index)* last_ptr, uint32_t index, uint32_t slot_size)
{
    Atomic_Stack_Slot* slot = atomic_stack_slot(data, index, slot_size);
    for(;;) {
        //Note: we must read data after last because of the following case:
        // - the stack data was just reallocted to bigger array
        // - load last which alredy refers to an index in the larger array
        // - load data which refers to the old smaller array
        // - we access the last and get out of bounds read/write
        Gen_Index last = atomic_load(last_ptr, memory_order_acquire);
        atomic_thread_fence(memory_order_seq_cst);
        void* data = atomic_load_explicit(data_ptr, memory_order_acquire);

        Gen_Index new_last = {index, last.generation};
        atomic_store(&slot->next, last);
        if(atomic_compare_exchange_weak(last_ptr, &last, new_last))
            break;
    }
}

//push - starts reallocating
//pop
//push to old array
//push - stops reallocating
// -> the second push is not visible!

uint32_t atomic_stack_pop(CHAN_ATOMIC(void*)* data_ptr, CHAN_ATOMIC(Gen_Index)* last_ptr, uint32_t slot_size)
{
    for(;;) {
        //*same note as with push*
        Gen_Index last = atomic_load(last_ptr, memory_order_acquire);
        atomic_thread_fence(memory_order_seq_cst);
        void* data = atomic_load_explicit(data_ptr, memory_order_acquire);

        if(last.index == (uint32_t) -1)
            return (uint32_t) -1;
            
        Atomic_Stack_Slot* slot = atomic_stack_slot(data, last.index, slot_size);
        Gen_Index new_last = {slot->next, last.generation + 1};
        if(atomic_compare_exchange_weak(last_ptr, &last, new_last))
        {
            slot->next = (uint32_t) -1;
            return last.index;
        }
    }
}

uint32_t atomic_stack_pop_all(CHAN_ATOMIC(Gen_Index)* last_ptr, uint32_t slot_size)
{
    for(;;) {
        Gen_Index last = atomic_load(last_ptr);
        if(last.index == (uint32_t) -1)
            return (uint32_t) -1;
        
        Gen_Index new_last = {last.index, last.generation + 1};
        if(atomic_compare_exchange_weak(last_ptr, &last, new_last))
            return last.index;
    }
}

typedef struct _Gen_Ptr_* Gen_Ptr; 

typedef struct Unpacked_Gen_Ptr {
    void* ptr;
    uint64_t gen;
} Unpacked_Gen_Ptr;

Gen_Ptr gen_ptr_pack(void* ptr, uint64_t gen, isize aligned)
{
    const uint64_t mul = ((uint64_t) 1 << 48)/(uint64_t) aligned;

    uint64_t ptr_part = ((uint64_t) ptr / aligned) % mul;
    uint64_t gen_part = (uint64_t) gen * mul;

    uint64_t gen_ptr_val = ptr_part | gen_part;
    return (Gen_Ptr) gen_ptr_val;
}

Unpacked_Gen_Ptr gen_ptr_unpack(Gen_Ptr ptr, const isize aligned)
{
    //Everything marked const will get compiled at compile time 
    // (given that aligned is known at compile time)
    Unpacked_Gen_Ptr unpacked = {0};
    const uint64_t mul = ((uint64_t) 1 << 48)/(uint64_t) aligned;
    
    uint64_t ptr_bits = (uint64_t) ptr % mul;
    uint64_t gen_bits = (uint64_t) ptr / mul;
    
    //prepare ptr_mask = 0x0000FFFFFFFFFFF0 (at compile time)
    const uint64_t ptr_low_bits_mask = (uint64_t) -1 * (uint64_t) aligned;
    const uint64_t ptr_high_bits_mask = (uint64_t) -1 >> 16;
    const uint64_t ptr_mask = ptr_low_bits_mask & ptr_high_bits_mask;

    //Stuff dummy_ptr_bits with the bits of some address. 
    // I am hoping this will compile to simply using the stack pointer register
    uint64_t dummy_ptr_bits = (uint64_t) &unpacked;

    //"blend" between ptr_bits and dummy_ptr_bits based on ptr_mask: 
    // where there are 1s in ptr_mask the ptr_bits should be written else dummy_ptr_bits should be written.
    //The following two lines do the same thing except the uncommented is one instruction faster (and less readable)
    uint64_t ptr_val = ptr_bits ^ ((ptr_bits ^ dummy_ptr_bits) & ptr_mask); 
    //uint64_t ptr_val = (dummy_ptr_bits & ~ptr_mask) | (ptr_bits & ptr_mask);
    
    unpacked.gen = gen_bits;
    unpacked.ptr = ptr_val;
    return unpacked;
}



#if defined(_MSC_VER)
_CHAN_INLINE_ALWAYS  
static bool atomic_cas128_weak(volatile void* destination, 
    uint64_t old_val_lo, uint64_t old_val_hi,
    uint64_t new_val_lo, uint64_t new_val_hi,
    memory_order succ_mem_order,
    memory_order fail_mem_ordder)
{
    (void) succ_mem_order;
    (void) fail_mem_ordder;

    __int64 compare_and_out[] = {(__int64) old_val_lo, (__int64) old_val_hi};
    return _InterlockedCompareExchange128(destination, (__int64) new_val_hi, (__int64) new_val_lo, compare_and_out) != 0;
}
#elif defined(__GNUC__) || defined(__clang__)
_CHAN_INLINE_ALWAYS 
static bool atomic_cas128_weak(volatile void* destination, 
    uint64_t old_val_lo, uint64_t old_val_hi,
    uint64_t new_val_lo, uint64_t new_val_hi,
    memory_order succ_mem_order,
    memory_order fail_mem_ordder)
{
    const bool weak = true;
    __uint128_t old_val = ((__uint128_t) old_val_hi << 64) | (__uint128_t) old_val_lo;
    __uint128_t new_val = ((__uint128_t) new_val_hi << 64) | (__uint128_t) new_val_lo;
    return __atomic_compare_exchange_n(
        destination, old_val, new_val, weak, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST);
}
#else
    #error Unsupported compiler. I will write a version that doesnt depend on these eventually...
#endif

#define FREE_LIST_BLOCK_SIZE 64

typedef struct Sync_Free_List {
    CHAN_ATOMIC(Gen_Index) last_used;
    CHAN_ATOMIC(Gen_Index) first_free;

    CHAN_ATOMIC(isize) capacity;
    CHAN_ATOMIC(void**) blocks;

    uint32_t item_size;
    uint32_t slot_size;
    Ticket_Lock growing_lock;
} Sync_Free_List;

_CHAN_INLINE_NEVER
void _sync_free_list_unsafe_grow(Sync_Free_List* list, isize min_size)
{
    void** last_block = atomic_load_explicit(&list->blocks);
    isize capacity = atomic_load_explicit(&list->capacity);
    isize new_capacity = capacity*3/2 + 8;
    if(new_capacity < min_size)
        new_capacity = min_size;

    
    void* allocated = chan_aligned_alloc(FREE_LIST_BLOCK_SIZE*list->slot_size, 64);
    void* new_data = (void**) allocated + 1;

    uint32_t prev_slot = (uint32_t) -1;
    for(isize i = new_capacity; i-- > capacity; )
    {
        uint8_t* slot = (uint8_t*) new_data + i*list->slot_size;
        ((Sync_Pool_Slot*) slot)->next = prev_slot;
        prev_slot = i;
    }

    atomic_fetch_add(&list->capacity, FREE_LIST_BLOCK_SIZE);
    atomic_store_explicit(&list->data, new_data, memory_order_release);
    atomic_store_explicit(&list->first_free, prev_slot, memory_order_release);
    atomic_store_explicit(&list->capacity, new_capacity, memory_order_release);
}

typedef struct Sync_Free_List_Allocation {
    uint32_t index;
    void* ptr;
} Sync_Free_List_Allocation;

Atomic_Stack_Slot* sync_free_list_slot(Sync_Free_List* list, isize index)
{
    ASSERT(0 <= index && index <= list->capacity);
    uint64_t block_i = (uint64_t) index / FREE_LIST_BLOCK_SIZE;
    uint64_t item_i = (uint64_t) index % FREE_LIST_BLOCK_SIZE;
    
    void** blocks = atomic_load(&list->blocks);
    void* block = blocks[block_i];

    return (Atomic_Stack_Slot*) (void*) ((uint8_t*) block + (uint64_t) index*list->slot_size);
}

void _sync_free_list_push(Sync_Free_List* list, CHAN_ATOMIC(Gen_Index)* last_ptr, Sync_Free_List_Allocation allocation)
{
    //maybe we can get rid of this one op as well...
    Atomic_Stack_Slot* slot = ((Atomic_Stack_Slot*) (void*) allocation.ptr) - 1;
    for(;;) {
        Gen_Index last = atomic_load(last_ptr);
        Gen_Index new_last = {allocation.index, last.generation};
        atomic_store(&slot->next, last);
        if(atomic_compare_exchange_weak(last_ptr, &last, new_last))
            break;
    }
}

Sync_Free_List_Allocation _sync_free_list_pop(Sync_Free_List* list, CHAN_ATOMIC(Gen_Index)* last_ptr)
{
    for(;;) {
        Gen_Index last = atomic_load(last_ptr);
        if(last.index == (uint32_t) -1) {
            Sync_Free_List_Allocation out = {-1, NULL};
            return out;
        }
            
        Atomic_Stack_Slot* slot = sync_free_list_slot(list, last.index);
        Gen_Index new_last = {slot->next, last.generation + 1};
        if(atomic_compare_exchange_weak(last_ptr, &last, new_last))
        {
            slot->next = (uint32_t) -1;
            Sync_Free_List_Allocation out = {last.index, slot->data};
            return out;
        }
    }
}

Sync_Free_List_Allocation sync_free_list_alloc(Sync_Free_List* list)
{
    Sync_Free_List_Allocation first_free = {0};
    for(;;) {
        //I could check the gen counter???
        isize capacity = atomic_load_explicit(&list->capacity);
        first_free = _sync_free_list_pop(list, &list->first_free);
        if(first_free.ptr == NULL)
            break;
        else
        {
            ticket_lock(&list->growing_lock, SYNC_WAIT_BLOCK);
            isize reload_capacity = atomic_load_explicit(&list->capacity);
            if(reload_capacity == capacity)
                _sync_free_list_unsafe_grow(list, 0);
            ticket_unlock(&list->growing_lock, SYNC_WAIT_BLOCK);
        }
    }

    return first_free;
}

void sync_free_list_free(Sync_Free_List* list, Sync_Free_List_Allocation allocation)
{
    _sync_free_list_push(list, &list->first_free, allocation);
}

void sync_stack_push(Sync_Free_List* list, const void* data)
{
    Sync_Free_List_Allocation alloc = sync_free_list_alloc(list);
    memcpy(alloc.ptr, data, list->item_size);
    _sync_free_list_push(list, &list->last_used, alloc);
}

bool sync_stack_pop(Sync_Free_List* list, void* data)
{
    Sync_Free_List_Allocation alloc = _sync_free_list_pop(list, &list->last_used);
    if(alloc.ptr == NULL)
        return false;

    memcpy(data, alloc.ptr, list->item_size);
    _sync_free_list_push(list, &list->first_free, alloc);
    return true;
}


void sync_free_list_reserve(Sync_Free_List* list, isize to_size)
{
    isize capacity = atomic_load_explicit(&list->capacity, memory_order_acquire);
    if(capacity < to_size)
    {
        void* data = atomic_load_explicit(&list->data, memory_order_acquire);
    }
}