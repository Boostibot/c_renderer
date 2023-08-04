#ifndef LIB_DEBUG_ALLOCATOR
#define LIB_DEBUG_ALLOCATOR

//It is extremely easy to mess up memory management in some way in C. Even when using hierarchical memory managemnt
// (local allocator tree) memory leeks are still localy possible which is often not idea. Thus we are need in
// of solid tooling to enable quick and reliable debugging of memory problems. 
// 
// This file attempts to create a simple
// allocator for just that using the Allocator interface. The advantage is that it can be swapped in even during runtime
// for maximum flexibility.
//
// From memory debugger we require the following functionality:
// 1) assert validity of all programmer given memory blocks without touching them
// 2) be able to list all currently active memory blocks along with some info to fascilitate debugging
// 3) assert that no overwrites (and ideally even overreads) happened 
//
// Additionally we would like the following:
// 4) to see some ammount of allocation history
// 5) runtime customize what will happen should a memory panic be raised
// 6) the allocator should be as fast as possible
//
// The approach this file takes is on the following schema:
//
//  Debug_Allocator
//  |-------------------------|
//  | Allocator* parent       |                        |----------------------------------------|
//  | ...                     |           0----------->| XXX | header | dead | USER| dead | XXX |
//  | alive_allocations_hash: |           |            |----------------------------------------|
//  | |-------------|         |           |
//  | | 0x8157190a0 | --------------------o
//  | | 0           |         |
//  | | 0           |         |                  *BLOCK*: allocated block from parent allocator 
//  | | ...         |         |       |-----------------------------------------------------------|
//  | | 0x140144100 | --------------->| XXXX | header | dead zone | USER DATA | dead zone | XXXXX |
//  | | 0           |         |       ^---------------------------^-------------------------------|
//  | |_____________|         |       ^                           ^ 
//  |_________________________|       L 8 aligned                 L aligned to user specified align
//
// 
// Within each *BLOCK* are contained the properly sized user data along with some header containing meta
// data about the alloction (is used to validate arguments and fasciliate debugging), dead zones which 
// are filled with 0x55 bytes (0 and 1 alteranting in binary), and some unspecified padding bytes which 
// may occur due to overaligned requirements for user data.
// 
// Prior to each access the block adress is looked up in the alive_allocations_hash. If it is found
// the dead zones and header is checked for validty (invalidty would indicate overwrites). Only then
// any allocation/deallocation takes place.


#include "allocator.h"
#include "array.h"
#include "hash_table.h"
#include "hash.h"
#include "time.h"
#include "log.h"

typedef struct Debug_Allocator          Debug_Allocator;
typedef struct Debug_Allocation         Debug_Allocation;
typedef struct Debug_Allocator_Options  Debug_Allocator_Options;
typedef enum Debug_Allocator_Panic_Reason Debug_Allocator_Panic_Reason;

DEFINE_ARRAY_TYPE(Debug_Allocation, Debug_Allocation_Array);
   
typedef void (*Debug_Allocator_Panic)(Debug_Allocator* allocator, Debug_Allocator_Panic_Reason reason, Debug_Allocation allocation, isize penetration, Source_Info called_from, void* context);

typedef struct Debug_Allocator
{
    Allocator allocator;
    Allocator* parent;
    const char* name;
    
    Debug_Allocation_Array dead_allocations;
    Hash_Table64 alive_allocations_hash;

    bool do_printing;            //wheter it should continually print new allocations/deallocations. can be togled
    bool do_contnual_checks;     //wheter it should checks all allocations for overwrites after each allocation.
                                 //icurs huge performance costs. can be toggled during runtime
    bool do_deinit_leak_check;   //If the memory use on initialization and deinitializtion does not match panics.

    isize dead_zone_size;        //size in bytes of the dead zone. CANNOT be changed after creation!
    isize dead_allocation_max;   //size of dead_allocations circular buffer
    isize dead_allocation_index; //the total ammount of dead allocations (does not wrap)
    
    //alive_allocations.data[dead_allocation_index % dead_allocation_max - 1] 
    //is the most recent allocation
    Debug_Allocator_Panic panic_handler;
    void* panic_context;

    isize bytes_allocated;
    isize max_bytes_allocated;

    isize allocation_count;
    isize deallocation_count;
    isize reallocation_count;

    Allocator_Swap allocator_swap;
    bool is_init; //prevents double init
} Debug_Allocator;



#define DEBUG_ALLOCATOR_CONTINUOUS          (u64) 1
#define DEBUG_ALLOCATOR_PRINT               (u64) 2  
#define DEBUG_ALLOCATOR_LARGE_DEAD_ZONE     (u64) 4
#define DEBUG_ALLOCATOR_DEINIT_LEAK_CHECK   (u64) 8

//Initalizes the debug allocator using a parent and options. 
//Many options cannot be changed during the life of the debug allocator.
EXPORT void debug_allocator_init(Debug_Allocator* allocator, Allocator* parent, Debug_Allocator_Options options);
//Initalizes the debug allocator and makes it the dafault and scratch global allocator.
//Additional flags defined above can be passed to quickly tweak the allocator.
//Restores the old allocators on deinit. 
EXPORT void debug_allocator_init_use(Debug_Allocator* allocator, u64 flags);
//Deinits the debug allocator
EXPORT void debug_allocator_deinit(Debug_Allocator* allocator);

EXPORT void* debug_allocator_allocate(Allocator* self_, isize new_size, void* old_ptr, isize old_size, isize align, Source_Info called_from);
EXPORT Allocator_Stats debug_allocator_get_stats(Allocator* self_);

//Returns info about the specific alive debug allocation @TODO
EXPORT Debug_Allocation       debug_allocator_get_allocation(Debug_Allocator* allocator, void* ptr); 
//Returns up to get_max currectly alive allocations sorted by their time of allocation. If get_max <= 0 returns all
EXPORT Debug_Allocation_Array debug_allocator_get_alive_allocations(const Debug_Allocator allocator, isize print_max);
//Returns up to get_max last dead allocations sorted by their time of allocation. If get_max <= 0 returns up to dead_allocation_max (specified during construction)
EXPORT Debug_Allocation_Array debug_allocator_get_dead_allocations(const Debug_Allocator allocator, isize get_max);

//Prints up to get_max currectly alive allocations sorted by their time of allocation. If get_max <= 0 returns all
EXPORT void debug_allocator_print_alive_allocations(const Debug_Allocator allocator, isize print_max); 
//Returns up to get_max last dead allocations sorted by their time of allocation. If get_max <= 0 returns up to dead_allocation_max (specified during construction)
EXPORT void debug_allocator_print_dead_allocations(const Debug_Allocator allocator, isize print_max);

//Default panic handler for debug allocators. Prints if printing is enbaled and then aborts the program 
EXPORT void debug_allocator_panic_func(Debug_Allocator* allocator, Debug_Allocator_Panic_Reason reason, Debug_Allocation allocation, isize penetration, Source_Info called_from, void* context);
//Converts a panic reason to string
EXPORT const char* debug_allocator_panic_reason_to_string(Debug_Allocator_Panic_Reason reason);

typedef struct Debug_Allocation
{
    void* ptr;                     
    isize size;
    isize align;
    Source_Info allocation_source;
    Source_Info deallocation_source;
    double allocation_time_s;
    double deallocation_time_s;
} Debug_Allocation;

typedef struct Debug_Allocator_Options
{
    //size in bytes of overwite prevention dead zone. If 0 then default 16 is used. If less then zero no dead zone is used.
    isize dead_zone_size; 
    //size of history to keep. If 0 then default 1000 is used. If less then zero no history is kept.
    isize max_dead_allocations;

    //Pointer to Debug_Allocator_Panic. If none is set uses debug_allocator_panic_func
    Debug_Allocator_Panic panic_handler; 
    //Context for panic_handler. No default is set.
    void* panic_context;

    bool do_printing;        //prints all allocations/deallocation
    bool do_contnual_checks; //continually checks all allocations
    bool do_deinit_leak_check;   //If the memory use on initialization and deinitializtion does not match panics.

    //Optional name of this allocator for printing and debugging. No defualt is set
    const char* name;
} Debug_Allocator_Options;

typedef enum Debug_Allocator_Panic_Reason
{
    //no error
    DEBUG_ALLOC_PANIC_NONE = 0,

    //the provided pointer does not point to previously allocated block
    DEBUG_ALLOC_PANIC_INVALID_PTR,

    //size and/or alignment for the given allocation ptr do not macth or are invalid (less then zero, not power of two)
    DEBUG_ALLOC_PANIC_INVALID_PARAMS, 

    //memory was written before valid user allocation segemnt
    DEBUG_ALLOC_PANIC_OVERWRITE_BEFORE_BLOCK,
    //memory was written after valid user allocation segemnt
    DEBUG_ALLOC_PANIC_OVERWRITE_AFTER_BLOCK,

    //memory usage on startup doesnt match memory usage on deinit. Only used when initialized with do_deinit_leak_check = true
    DEBUG_ALLOC_PANIC_DEINIT_MEMORY_LEAKED,
} Debug_Allocator_Panic_Reason;
#endif

#if (defined(LIB_ALL_IMPL) || defined(LIB_DEBUG_ALLOCATOR_IMPL)) && !defined(LIB_DEBUG_ALLOCATOR_HAS_IMPL)
#define LIB_DEBUG_ALLOCATOR_HAS_IMPL

#define DEBUG_ALLOCATOR_MAGIC_NUM8  (u8)  0x55

#define _CRT_SECURE_NO_WARNINGS
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

typedef struct Debug_Allocation_Header
{
    isize size;
    i32 align;
    i32 block_start_offset;
    Source_Info allocation_source;
    double allocation_time_s;
} Debug_Allocation_Header;

EXPORT void debug_allocator_init(Debug_Allocator* debug, Allocator* parent, Debug_Allocator_Options options)
{
    ASSERT(debug->is_init == false && "must not be init!");

    array_init(&debug->dead_allocations, &global_malloc_allocator.allocator);
    hash_table64_init(&debug->alive_allocations_hash, &global_malloc_allocator.allocator);

    if(options.dead_zone_size == 0)
        options.dead_zone_size = 16;
    if(options.dead_zone_size < 0)
        options.dead_zone_size = 0;
    options.dead_zone_size = DIV_ROUND_UP(options.dead_zone_size, DEF_ALIGN)*DEF_ALIGN;

    if(options.max_dead_allocations == 0)
        options.max_dead_allocations = 1000;
    if(options.max_dead_allocations < 0)
        options.max_dead_allocations = 0;

    if(options.panic_handler == NULL)
        options.panic_handler = debug_allocator_panic_func;

    debug->do_deinit_leak_check = options.do_deinit_leak_check;
    debug->name = options.name;
    debug->do_contnual_checks = options.do_contnual_checks;
    debug->dead_zone_size = options.dead_zone_size;
    debug->do_printing = options.do_printing;
    debug->parent = parent;
    debug->allocator.allocate = debug_allocator_allocate;
    debug->allocator.get_stats = debug_allocator_get_stats;
    debug->panic_handler = options.panic_handler;
    debug->panic_context = options.panic_context;
    debug->dead_allocation_max = options.max_dead_allocations;
    debug->dead_allocation_index = 0;
    debug->is_init = true;

    array_resize(&debug->dead_allocations, options.max_dead_allocations);
}

EXPORT void debug_allocator_init_use(Debug_Allocator* debug, u64 flags)
{
    Debug_Allocator_Options options = {0};
    if(flags & DEBUG_ALLOCATOR_CONTINUOUS)
        options.do_contnual_checks = true;
    if(flags & DEBUG_ALLOCATOR_PRINT)
        options.do_printing = true;
    if(flags & DEBUG_ALLOCATOR_DEINIT_LEAK_CHECK)
        options.do_deinit_leak_check = true;
    if(flags & DEBUG_ALLOCATOR_LARGE_DEAD_ZONE)
        options.dead_zone_size = 64;

    debug_allocator_init(debug, &global_malloc_allocator.allocator, options);

    debug->allocator_swap = allocator_push_both(&debug->allocator);
}


INTERNAL void* _debug_allocator_panic(Debug_Allocator* self, Debug_Allocator_Panic_Reason reason, void* ptr, Source_Info called_from);

EXPORT void debug_allocator_deinit(Debug_Allocator* allocator)
{
    if(allocator->bytes_allocated != 0)
        _debug_allocator_panic(allocator, DEBUG_ALLOC_PANIC_DEINIT_MEMORY_LEAKED, NULL, SOURCE_INFO());

    allocator_pop(allocator->allocator_swap);
    array_deinit(&allocator->dead_allocations);
    hash_table64_deinit(&allocator->alive_allocations_hash);
    
    Debug_Allocator null = {0};
    *allocator = null;
}

EXPORT const char* debug_allocator_panic_reason_to_string(Debug_Allocator_Panic_Reason reason)
{
    switch(reason)
    {
        case DEBUG_ALLOC_PANIC_NONE: return "DEBUG_ALLOC_PANIC_NONE";

        case DEBUG_ALLOC_PANIC_INVALID_PTR: return "DEBUG_ALLOC_PANIC_INVALID_PTR";

        case DEBUG_ALLOC_PANIC_INVALID_PARAMS: return "DEBUG_ALLOC_PANIC_INVALID_PARAMS"; 

        case DEBUG_ALLOC_PANIC_OVERWRITE_BEFORE_BLOCK: return "DEBUG_ALLOC_PANIC_OVERWRITE_BEFORE_BLOCK";
        case DEBUG_ALLOC_PANIC_OVERWRITE_AFTER_BLOCK: return "DEBUG_ALLOC_PANIC_OVERWRITE_AFTER_BLOCK";

        default: return "DEBUG_ALLOC_PANIC_NONE";
    }
}

EXPORT void debug_allocator_panic_func(Debug_Allocator* allocator, Debug_Allocator_Panic_Reason reason, Debug_Allocation allocation, isize penetration, Source_Info called_from, void* context)
{   
    const char* reason_str = debug_allocator_panic_reason_to_string(reason);

    if(allocator->do_printing)
    {
        LOG_FATAL("MEMORY", "PANIC because of %s at pointer 0x%p " SOURCE_INFO_FMT "\n", reason_str, allocation.ptr, SOURCE_INFO_PRINT(called_from));
        debug_allocator_print_alive_allocations(*allocator, 0);
    }
    abort();
}


INTERNAL Debug_Allocation_Header* _debug_allocator_get_header(const Debug_Allocator self, void* user_ptr)
{
    u8* dead_zone = (u8*) user_ptr - self.dead_zone_size;
    Debug_Allocation_Header* header = (Debug_Allocation_Header*) dead_zone - 1;
    return header;
}

INTERNAL int _debug_allocation_alloc_time_compare(const void* a, const void* b)
{
    Debug_Allocation* a_ = (Debug_Allocation*) a;
    Debug_Allocation* b_ = (Debug_Allocation*) b;

    if(a_->allocation_time_s < b_->allocation_time_s)
        return -1;
    else
        return 1;
}

INTERNAL Debug_Allocation _debug_allocator_header_to_allocation(const Debug_Allocation_Header* header)
{
    Debug_Allocation allocation = {0};
    allocation.align = header->align;
    allocation.size = header->size;
    allocation.allocation_source = header->allocation_source;
    allocation.allocation_time_s = header->allocation_time_s;
    allocation.ptr = (void*) (header + 1);

    return allocation;
}

INTERNAL isize _debug_allocator_find_allocation(const Debug_Allocator self, void* ptr)
{
    u64 hashed = hash64((u64) ptr);
    isize hash_found = hash_table64_find(self.alive_allocations_hash, hashed);
    
    for(isize counter = 0; counter < self.alive_allocations_hash.entries_count; counter ++)
    {
        if(hash_found == -1)
            break;
        void* found_ptr = (void*) self.alive_allocations_hash.entries[hash_found].value;
        if(found_ptr == ptr)
            break;

        hash_found = hash_table64_find_next(self.alive_allocations_hash, hashed, hash_found);
    }

    return hash_found;
}

INTERNAL Debug_Allocator_Panic_Reason _debug_allocator_check_header(const Debug_Allocator self, Debug_Allocation_Header* header, isize* interpenetration)
{
    interpenetration = 0; //@TEMP
    u8* pre_dead_zone = (u8*) (header + 1);;
    u8* ptr = pre_dead_zone + self.dead_zone_size;;

    isize hash_found = _debug_allocator_find_allocation(self, ptr);
    if(hash_found == -1)
    {
        return DEBUG_ALLOC_PANIC_INVALID_PTR;
    }
        
    for(isize i = 0; i < self.dead_zone_size; i++)
    {
        if(pre_dead_zone[i] != DEBUG_ALLOCATOR_MAGIC_NUM8)
        {
            return DEBUG_ALLOC_PANIC_OVERWRITE_BEFORE_BLOCK;
        }
    }
        
    if(header->align <= 0 
        || header->size <= 0)
        return DEBUG_ALLOC_PANIC_OVERWRITE_BEFORE_BLOCK;

    u8* post_dead_zone = ptr + header->size;
    for(isize i = 0; i < self.dead_zone_size; i++)
    {
        if(post_dead_zone[i] != DEBUG_ALLOCATOR_MAGIC_NUM8)
        {
            return DEBUG_ALLOC_PANIC_OVERWRITE_AFTER_BLOCK;
        }
    }

    return DEBUG_ALLOC_PANIC_NONE;
}

INTERNAL void _debug_allocator_assert_header(const Debug_Allocator allocator, Debug_Allocation_Header* header)
{
    #ifndef NDEBUG
        isize interpenetration = 0;
        Debug_Allocator_Panic_Reason reason = _debug_allocator_check_header(allocator, header, &interpenetration);
        ASSERT(reason == DEBUG_ALLOC_PANIC_NONE);
    #endif // !NDEBUG
}

INTERNAL bool _debug_allocator_is_invariant(const Debug_Allocator allocator)
{
    ASSERT(allocator.dead_zone_size/DEF_ALIGN*DEF_ALIGN == allocator.dead_zone_size 
        && "dead zone size must be a valid multiple of alignment"
        && "this is so that the pointers within the header will be properly aligned!");

    //All alive allocations must be in hash

    if(allocator.do_contnual_checks)
    {
        isize size_sum = 0;
        for(isize i = 0; i < allocator.alive_allocations_hash.entries_count; i ++)
        {
            Hash_Table64_Entry curr = allocator.alive_allocations_hash.entries[i];
            if(hash_table64_is_entry_used(curr) == false)
                continue;
        
            isize hash_found = _debug_allocator_find_allocation(allocator, (void*) curr.value);
            ASSERT(hash_found != -1 && "All alive allocations must be in hash");

            Debug_Allocation_Header* header = _debug_allocator_get_header(allocator, (void*) curr.value);
        
            isize interpenetration = 0;
            Debug_Allocator_Panic_Reason reason = _debug_allocator_check_header(allocator, header, &interpenetration);
            ASSERT(reason == DEBUG_ALLOC_PANIC_NONE);

            size_sum += header->size;
        }

        ASSERT(size_sum == allocator.bytes_allocated);
        ASSERT(size_sum <= allocator.max_bytes_allocated);
    }

    return true;
}

EXPORT Debug_Allocation_Array debug_allocator_get_alive_allocations(const Debug_Allocator allocator, isize print_max)
{
    isize count = print_max;
    const Hash_Table64* hash = &allocator.alive_allocations_hash;
    if(count <= 0)
        count = hash->size;
        
    if(count >= hash->size);
        count = hash->size;
        
    Debug_Allocation_Array out = {&global_malloc_allocator.allocator};
    for(isize i = 0; i < hash->entries_count; i++)
    {
        if(hash_table64_is_entry_used(hash->entries[i]))
        {

            Debug_Allocation_Header* header = _debug_allocator_get_header(allocator, (void*) hash->entries[i].value);
            _debug_allocator_assert_header(allocator, header);

            Debug_Allocation allocation = _debug_allocator_header_to_allocation(header);
            array_push(&out, allocation);
        }
    }
    
    qsort(out.data, out.size, sizeof *out.data, _debug_allocation_alloc_time_compare);

    ASSERT(out.size <= count);
    array_resize(&out, count);

    return out;
}

EXPORT Debug_Allocation_Array debug_allocator_get_dead_allocations(const Debug_Allocator allocator, isize get_max)
{   
    if(get_max <= 0)
        get_max = allocator.dead_allocation_index;

    isize count = MIN(get_max, allocator.dead_allocation_max);
    count = MIN(count, allocator.dead_allocation_index);

    Debug_Allocation_Array out = {&global_malloc_allocator.allocator};
    for(isize i = 0; i < count; i++)
    {
        isize index = (allocator.dead_allocation_index - i - 1) % allocator.dead_allocation_max;
        CHECK_BOUNDS(index, allocator.dead_allocations.size);
        Debug_Allocation curr = allocator.dead_allocations.data[index];

        array_push(&out, curr);
    }

    for(isize i = 0; i < out.size - 1; i++)
    {
        CHECK_BOUNDS(i,     out.size);
        CHECK_BOUNDS(i + 1, out.size);
        ASSERT(out.data[i].deallocation_time_s > out.data[i + 1].deallocation_time_s && "deallocations should be already sorted so that the most recent one is first!");
    }
    
    return out;
}

EXPORT void debug_allocator_print_alive_allocations(const Debug_Allocator allocator, isize print_max)
{
    _debug_allocator_is_invariant(allocator);
    Debug_Allocation_Array alive = debug_allocator_get_alive_allocations(allocator, print_max);
    if(print_max > 0)
        ASSERT(alive.size <= print_max);

    typedef long long int lld;
    LOG_INFO("MEMORY", "Printing ALIVE allocations (%lld) below:\n", (lld)alive.size);
    log_group_push();

    for(isize i = 0; i < alive.size; i++)
    {
        Debug_Allocation curr = alive.data[i];
        LOG_INFO("MEMORY", "%-3lld - size %-8lld ptr: 0x%p align: %-2lld" SOURCE_INFO_FMT "\n",
            (lld) i, (lld) curr.size, curr.ptr, (lld) curr.align, SOURCE_INFO_PRINT(curr.allocation_source));
    }

    log_group_pop();
    array_deinit(&alive);
}

EXPORT void debug_allocator_print_dead_allocations(const Debug_Allocator allocator, isize print_max)
{
    Debug_Allocation_Array dead = debug_allocator_get_dead_allocations(allocator, print_max);
    if(print_max > 0)
        ASSERT(dead.size <= print_max);

    typedef long long int lld;
    LOG_INFO("MEMORY", "Printing DEAD allocations (%lld) below:\n", (lld)dead.size);
    for(isize i = 0; i < dead.size; i++)
    {
        Debug_Allocation curr = dead.data[i];

        LOG_INFO("MEMORY", "%-3lld - size %-8lld ptr: 0x%p align: %-2lld",
            (lld) i, (lld) curr.size, curr.ptr, curr.align);

        Source_Info from_source = curr.allocation_source;
        Source_Info to_source = curr.deallocation_source;
        bool files_match = strcmp(from_source.file, to_source.file) == 0;

        if(files_match)
        {
            LOG_INFO("MEMORY", "%-3lld - size %-8lld ptr: 0x%p align: %-2lld (%s : %3lld -> %3lld)\n",
                (lld) i, (lld) curr.size, curr.ptr, curr.align,
                to_source.file, (lld) from_source.line, (lld) to_source.line);
        }
        else
        {
            LOG_INFO("MEMORY", "%-3lld - size %-8lld ptr: 0x%p align: %-2lld\n",
                "[%-3lld] " SOURCE_INFO_FMT " -> " SOURCE_INFO_FMT"\n",
                (lld) i, (lld) curr.size, curr.ptr, curr.align,
                i, SOURCE_INFO_PRINT(from_source), SOURCE_INFO_PRINT(to_source));
        }
    }

    array_deinit(&dead);
}

INTERNAL void* _debug_allocator_panic(Debug_Allocator* self, Debug_Allocator_Panic_Reason reason, void* ptr, Source_Info called_from)
{
    //#error @TODO: add reason and interpenetration
    Debug_Allocation allocation = {0};
    allocation.ptr = ptr;

    if(self->panic_handler != NULL)
        self->panic_handler(self, reason, allocation, 0, called_from, self->panic_context);
    else
        debug_allocator_panic_func(self, reason, allocation, 0, called_from, self->panic_context);

    return NULL;
}

INTERNAL bool _debug_allocator_is_aligned(void* ptr, isize alignment)
{
    return ptr == align_backward(ptr, alignment);
}

EXPORT void* debug_allocator_allocate(Allocator* self_, isize new_size, void* old_ptr_, isize old_size, isize align, Source_Info called_from)
{
    //This function executes the following steps. 
    // The order is crucial because we need to ensure that: 
    //  1: all inputs are checked
    //  2: no changes are commit before the allocation because it might fail
    //  3: there is ever only one block with the given adress => block needs to be removed then added back 
    // 
    // STEPS:
    // if old_ptr is given: 
    //    check its validty 
    //    check the block it points to for overwrites etc.
    // 
    // reallocate using parent allocator
    // 
    // if failed: 
    //    return NULL (error!)
    // 
    // if deallocating: 
    //    remove from control structure
    // 
    // if allocating: 
    //    init new memory block (if is reallocating simply overrides)
    //    add to control structures
    //
    // prints the allocation (optional)
    // updates statistics

    double curr_time = clock_s();
    Debug_Allocator* self = (Debug_Allocator*) (void*) self_;
    _debug_allocator_is_invariant(*self);

    isize preamble_size = sizeof(Debug_Allocation_Header) + self->dead_zone_size;
    isize postamble_size = self->dead_zone_size;
    isize extra_size = preamble_size + postamble_size + align;

    isize total_new_size = new_size + extra_size;
    isize total_old_size = old_size + extra_size;

    if(new_size == 0)
        total_new_size = 0;
    
    if(old_size == 0)
        total_old_size = 0;

    u8* old_block_ptr = NULL;
    u8* new_block_ptr = NULL;
    u8* old_ptr = (u8*) old_ptr_;
    u8* new_ptr = NULL;
    Debug_Allocation_Header* old_header = NULL;
    Debug_Allocation_Header* new_header = NULL;
    Debug_Allocation old_allocation = {0};

    //Check old_ptr if any for correctness
    if(old_ptr != NULL)
    {
        u8* old_pre_dead_zone = old_ptr - self->dead_zone_size;
        u8* old_post_dead_zone = old_ptr + old_size;
        old_header = _debug_allocator_get_header(*self, old_ptr);
        ASSERT(_debug_allocator_is_aligned(old_header, DEF_ALIGN));

        old_allocation = _debug_allocator_header_to_allocation(old_header);
        old_allocation.deallocation_time_s = curr_time;
        old_allocation.deallocation_source = called_from;

        isize interpenetration = 0;

        //@TODO: fuse all this into one call
        Debug_Allocator_Panic_Reason reason = _debug_allocator_check_header(*self, old_header, &interpenetration);
        if(reason != DEBUG_ALLOC_PANIC_NONE)
            return _debug_allocator_panic(self, reason, old_ptr, called_from);

        if(old_header->size != old_size 
            || old_header->align != align)
            return _debug_allocator_panic(self, DEBUG_ALLOC_PANIC_INVALID_PARAMS, old_ptr, called_from);
        
        old_block_ptr = old_ptr - preamble_size - old_header->block_start_offset;
        ASSERT(old_block_ptr + total_old_size >= old_post_dead_zone + self->dead_zone_size && "total_old_size must be correct");
    }

    new_block_ptr = (u8*) self->parent->allocate(self->parent, total_new_size, old_block_ptr, total_old_size, DEF_ALIGN, called_from);
    
    //if failed return failiure and do nothing
    if(new_block_ptr == NULL && new_size != 0)
        return NULL;
    
    //if previous block existed remove it from controll structures
    if(old_ptr != NULL)
    {
        isize hash_found = _debug_allocator_find_allocation(*self, old_ptr);
        ASSERT(hash_found != -1 && "must be found!");

        hash_table64_remove(&self->alive_allocations_hash, hash_found);

        if(self->dead_allocation_max > 0)
        {
            isize index = self->dead_allocation_index % self->dead_allocation_max;
            CHECK_BOUNDS(index, self->dead_allocations.size);

            self->dead_allocations.data[index] = old_allocation;
            self->dead_allocation_index += 1;
        }
        self->bytes_allocated -= old_allocation.size;
    }

    //if allocated/reallocated (new block exists)
    if(new_size != 0)
    {
        isize fixed_align = MAX(align, DEF_ALIGN);
        new_ptr = (u8*) align_forward(new_block_ptr + preamble_size, fixed_align);

        u8* new_pre_dead_zone = new_ptr - self->dead_zone_size;
        u8* new_post_dead_zone = new_ptr + new_size;
        
        new_header = _debug_allocator_get_header(*self, new_ptr);
        new_header->align = (i32) align;
        new_header->size = new_size;
        new_header->block_start_offset = (i32) ((u8*) new_header - new_block_ptr);
        new_header->allocation_source = called_from;
        new_header->allocation_time_s = curr_time;

        memset(new_pre_dead_zone, DEBUG_ALLOCATOR_MAGIC_NUM8, self->dead_zone_size);
        memset(new_post_dead_zone, DEBUG_ALLOCATOR_MAGIC_NUM8, self->dead_zone_size);
        
        ASSERT(_debug_allocator_find_allocation(*self, new_ptr) == -1 && "Must not be added already!");

        u64 hashed = hash64((u64) new_ptr);
        hash_table64_insert(&self->alive_allocations_hash, hashed, (u64) new_ptr);
    
        self->bytes_allocated += new_header->size;
        self->max_bytes_allocated = MAX(self->max_bytes_allocated, self->bytes_allocated);

        _debug_allocator_assert_header(*self, new_header);
    }
    
    if(self->do_printing)
    {
        typedef long long int lld;
        LOG_INFO("MEMORY", "size %6lld -> %-6lld ptr: 0x%p -> 0x%p align: %lld " SOURCE_INFO_FMT "\n",
            (lld) old_size, (lld) new_size, old_ptr, new_ptr, (lld) align, SOURCE_INFO_PRINT(called_from));
    }

    if(old_ptr == NULL)
        self->allocation_count += 1;
    else if(new_size == 0)
        self->deallocation_count += 1;
    else
        self->reallocation_count += 1;
    
    _debug_allocator_is_invariant(*self);
    return new_ptr;
}

EXPORT Allocator_Stats debug_allocator_get_stats(Allocator* self_)
{
    Debug_Allocator* self = (Debug_Allocator*) (void*) self_;
    Allocator_Stats out = {0};
    out.type_name = "Debug_Allocator";
    out.name = self->name;
    out.parent = self->parent;
    out.max_bytes_allocated = self->max_bytes_allocated;
    out.bytes_allocated = self->bytes_allocated;
    out.allocation_count = self->allocation_count;
    out.deallocation_count = self->deallocation_count;
    out.reallocation_count = self->reallocation_count;

    return out;
}
#endif