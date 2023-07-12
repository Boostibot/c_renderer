#pragma once

#include "allocator.h"
#include "allocator_malloc.h"
#include "array.h"
#include "hash_table.h"
#include "hash.h"
#include "time.h"

#include <stdio.h>

#define DEBUG_ALLOCATOR_MAGIC_NUM64 (u64) 0xA1B2C3D4E5F60708
#define DEBUG_ALLOCATOR_MAGIC_NUM32 (u32) 0xA1B2C3D4
#define DEBUG_ALLOCATOR_MAGIC_NUM8  (u8)  0x55

enum Debug_Allocator_Panic
{
    DEBUG_ALLOC_PANIC_INVALID_FREE,
    DEBUG_ALLOC_PANIC_INVALID_REALLOC,
    DEBUG_ALLOC_PANIC_OVERWRITE_BEFORE_BLOCK,
    DEBUG_ALLOC_PANIC_OVERWRITE_AFTER_BLOCK,
};

typedef struct Debug_Allocation_Preamble
{
    void* ptr;
    isize size;
    isize align;
    Source_Info allocation_source;
    double allocation_time_s;
    u64 magic_number;
} Debug_Allocation_Preamble;

typedef struct Debug_Allocation_Postamble
{
    u64 magic_number;
} Debug_Allocation_Postamble;

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

#define DEBUG_ALLOCATOR_FAST

DEFINE_ARRAY_TYPE(Debug_Allocation, Debug_Allocation_Array);

typedef struct Debug_Allocator Debug_Allocator;
typedef void (*Debug_Allocator_Panic)(Debug_Allocator* allocator, void* invalid, void* context, Source_Info called_from);
  
//typedef struct Debug_Allocator_Params
//{
//
//} Debug_Allocator_Params;

typedef struct Debug_Allocator
{
    Allocator allocator;
    Allocator* parent;
    const char* name;

    Debug_Allocation_Array alive_allocations;
    Debug_Allocation_Array dead_allocations;
    isize first_free;               //index of first free slot in alive_allocations 
    Hash_Table64 allocation_hash;

    isize dead_zone_size;
    bool do_pointer_checks;
    bool do_printing;            //wheter it should continually print new allocations/deallocations 
    isize dead_allocation_max;   //size of dead_allocations circular buffer
    isize dead_allocation_count; //the total ammount of dead allocations (does not wrap)
    
    //alive_allocations.data[dead_allocation_count % dead_allocation_max - 1] 
    //is the most recent allocation
    Debug_Allocator_Panic panic_handler;
    void* panic_context;

    isize bytes_allocated;
    isize max_bytes_allocated;

    isize allocation_count;
    isize deallocation_count;
    isize reallocation_count;
} Debug_Allocator;

#define SOURCE_INFO_FORMAT "( %s : %lld )"
#define SOURCE_INFO_PRINT(source_info) (source_info).file, (source_info).line


EXPORT Debug_Allocator debug_allocator_make(Allocator* parent, Debug_Allocator_Panic panic_handler, void* panic_context, isize max_dead_allocations);
EXPORT void debug_allocator_deinit(Debug_Allocator* allocator);


EXPORT void* debug_allocator_allocate(Allocator* self_, isize new_size, void* old_ptr, isize old_size, isize align, Source_Info called_from);
EXPORT Allocator_Stats debug_allocator_get_stats(Allocator* self_);

EXPORT void debug_allocator_panic_func(Debug_Allocator* allocator, void* invalid, void* context, Source_Info called_from);
EXPORT void debug_allocator_print_active_allocations(const Debug_Allocator allocator, isize print_max); //if print_max <= 0 then prints all
EXPORT void debug_allocator_print_dead_allocations(const Debug_Allocator allocator, isize print_max);   //if print_max <= 0 then prints all upto dead_allocation_max

#if (defined(LIB_ALL_IMPL) || defined(LIB_DEBUG_ALLOCATOR_IMPL)) && !defined(LIB_DEBUG_ALLOCATOR_HAS_IMPL)
#define LIB_DEBUG_ALLOCATOR_HAS_IMPL
EXPORT Debug_Allocator debug_allocator_make(Allocator* parent, Debug_Allocator_Panic panic_handler, void* panic_context, isize max_dead_allocations)
{
    Debug_Allocator debug = {0};
    array_init(&debug.alive_allocations, &global_malloc_allocator.allocator);
    array_init(&debug.dead_allocations, &global_malloc_allocator.allocator);

    hash_table64_init(&debug.allocation_hash, &global_malloc_allocator.allocator);

    debug.parent = parent;
    debug.allocator.allocate = debug_allocator_allocate;
    debug.allocator.get_stats = debug_allocator_get_stats;
    debug.panic_handler = panic_handler;
    debug.panic_context = panic_context;
    debug.do_printing = true;
    debug.dead_allocation_max = max_dead_allocations;
    debug.dead_allocation_count = 0;
    array_resize(&debug.dead_allocations, max_dead_allocations);
    return debug;
}

EXPORT void debug_allocator_panic_func(Debug_Allocator* allocator, void* invalid, void* context, Source_Info called_from)
{   
    if(allocator->do_printing)
        printf("[MEM_DEBUG]: invalid realloc/dealloc of pointer 0x%p " SOURCE_INFO_FORMAT "\n", invalid, SOURCE_INFO_PRINT(called_from));
    abort();
}

EXPORT void debug_allocator_deinit(Debug_Allocator* allocator)
{
    array_deinit(&allocator->alive_allocations);
    array_deinit(&allocator->dead_allocations);
    hash_table64_deinit(&allocator->allocation_hash);
}

EXPORT void debug_allocator_print_active_allocations(const Debug_Allocator allocator, isize print_max)
{
    isize to = print_max;
    if(to <= 0)
        to = allocator.alive_allocations.size;
        
    if(to >= allocator.alive_allocations.size);
        to = allocator.alive_allocations.size;

    typedef long long int lld;
    printf("[MEM_DEBUG]: Printing ALIVE allocations (%lld) below:\n", (lld)to);
    for(isize i = 0; i < to; i++)
    {
        CHECK_BOUNDS(i, allocator.alive_allocations.size);
        Debug_Allocation curr = allocator.alive_allocations.data[i];
        Source_Info from_source = curr.allocation_source;

        printf("[MEM_DEBUG]: [%-3lld] size %-8lld ptr: 0x%p align: %-2lld" SOURCE_INFO_FORMAT "\n",
            (lld) i, (lld) curr.size, curr.ptr, curr.align, SOURCE_INFO_PRINT(from_source));
    }
}

static isize _debug_allocator_get_dead_allocations_range(const Debug_Allocator allocator, isize max, isize* from, isize* to)
{
    if(max <= 0)
        max = allocator.dead_allocation_count;

    isize count = MIN(max, allocator.dead_allocation_max);
    count = MIN(count, allocator.dead_allocation_count);

    if(allocator.dead_allocation_max <= 0)
    {
        *from = 0;
        *to = 0;
    }
    else
    {
        *from = (allocator.dead_allocation_count - count) % allocator.dead_allocation_max;
        *to = (*from + count) % allocator.dead_allocation_max;
    }

    return count;
}

EXPORT void debug_allocator_print_dead_allocations(const Debug_Allocator allocator, isize print_max)
{
    isize from = 0;
    isize to = 0;
    isize count = _debug_allocator_get_dead_allocations_range(allocator, print_max, &from, &to);
    
    typedef long long int lld;
    printf("[MEM_DEBUG]: Printing DEAD allocations (%lld) below:\n", (lld)count);

    if(count == 0)
        return;

    CHECK_BOUNDS(from, allocator.dead_allocation_max);
    CHECK_BOUNDS(to, allocator.dead_allocation_max);
    isize counter = 0;
    for(isize i = from; i != to; i = (i + 1) % allocator.dead_allocation_max, counter++)
    {
        CHECK_BOUNDS(i, allocator.dead_allocations.size);
        Debug_Allocation curr = allocator.dead_allocations.data[i];

        printf("[MEM_DEBUG]: [%-3lld] size %-8lld ptr: 0x%p align: %-2lld",
            (lld) counter, (lld) curr.size, curr.ptr, curr.align);

        Source_Info from_source = curr.allocation_source;
        Source_Info to_source = curr.deallocation_source;
        bool files_match = strcmp(from_source.file, to_source.file) == 0;

        if(files_match)
        {
            printf("(%s : %3lld -> %3lld)\n",
                to_source.file, (lld) from_source.line, (lld) to_source.line);
        }
        else
        {
            printf("\n         [%-3lld] " SOURCE_INFO_FORMAT " -> " SOURCE_INFO_FORMAT"\n",
                counter, SOURCE_INFO_PRINT(from_source), SOURCE_INFO_PRINT(to_source));
        }
    }
}
static isize _debug_allocator_find_allocation(const Debug_Allocator self, void* ptr, isize* value)
{
    u64 hashed = hash64((u64) ptr);
    isize hash_found = hash_table64_find(self.allocation_hash, hashed);
    
    
    for(isize counter = 0; counter < self.allocation_hash.slot_count; counter ++)
    {
        if(hash_found == -1)
            break;

        isize index = self.allocation_hash.entries[hash_found].value;
        CHECK_BOUNDS(index, self.alive_allocations.size);
        if(self.alive_allocations.data[index].ptr == ptr)
        {
            *value = index;
            break;
        }

        hash_found = hash_table64_find_next(self.allocation_hash, hashed, hash_found);
    }
    return hash_found;
}

static bool _debug_allocator_is_invariant(const Debug_Allocator allocator)
{

    //All alive allocations must be in hash
    for(isize i = 0; i < allocator.alive_allocations.size; i ++)
    {
        Debug_Allocation curr = allocator.alive_allocations.data[i];
        
        isize found = 0;
        isize hash_found = _debug_allocator_find_allocation(allocator, curr.ptr, &found);

        ASSERT(found != -1 && hash_found != -1 && "All alive allocations must be in hash");
        CHECK_BOUNDS(found, allocator.alive_allocations.size);
        CHECK_BOUNDS(hash_found, allocator.allocation_hash.slot_count);
        ASSERT(found == i && "the roundtrip must be correct");
    }

    //All dead allocations must NOT be in hash
    isize dead_from = 0;
    isize dead_to = 0;
    isize dead_count = _debug_allocator_get_dead_allocations_range(allocator, 0, &dead_from, &dead_to);
    
    for(isize i = dead_from; i != dead_to; i = (i + 1) % allocator.dead_allocation_max)
    {
        isize dummy_found = 0;
        CHECK_BOUNDS(i, allocator.dead_allocations.size);
        Debug_Allocation curr = allocator.dead_allocations.data[i];
        ASSERT(_debug_allocator_find_allocation(allocator, curr.ptr, &dummy_found) == -1 && "All dead allocations must NOT be in hash");
    }

    return true;
}

static void _debug_allocator_remove_allocation(Debug_Allocator* self, void* ptr, Source_Info called_from)
{
    isize found = 0;
    isize hash_found = _debug_allocator_find_allocation(*self, ptr, &found);
    Debug_Allocation_Array* allocs = &self->alive_allocations;
    Debug_Allocation removed_val = {0};
    if(hash_found != -1)
    {
        CHECK_BOUNDS(found, self->alive_allocations.size);
        CHECK_BOUNDS(hash_found, self->allocation_hash.slot_count);
        Debug_Allocation* removed = &self->alive_allocations.data[found];
        Debug_Allocation* last = &array_last(&self->alive_allocations);
        
        removed_val = *removed;

        //Properly remove the allocation
        isize last_found = found;
        isize last_hash_found = hash_found;
        if(last != removed)
        {
            last_found = 0;
            last_hash_found = _debug_allocator_find_allocation(*self, last->ptr, &last_found);
            ASSERT(last_hash_found != -1 && "last must not be null - all alive entreies must be properly hashed!");
            SWAP(removed, last, Debug_Allocation);
        }

        array_pop(&self->alive_allocations);
        CHECK_BOUNDS(last_hash_found, self->allocation_hash.slot_count);
        self->allocation_hash.entries[last_hash_found].value = found;

        hash_table64_remove(&self->allocation_hash, last_found);
        
        //Add a new dead allocation
        removed_val.deallocation_source = called_from;
        if(self->dead_allocation_max > 0)
        {
            isize index = self->dead_allocation_count % self->dead_allocation_max;
            CHECK_BOUNDS(index, self->dead_allocations.size);

            self->dead_allocations.data[index] = removed_val;
            self->dead_allocation_count += 1;
        }
        self->bytes_allocated -= removed_val.size;
    }
    else
    {
        if(self->panic_handler != NULL)
            self->panic_handler(self, ptr, self->panic_context, called_from); 
    }
}

static void _debug_allocator_add_allocation(Debug_Allocator* self, Debug_Allocation allocation, Source_Info called_from)
{
    allocation.allocation_source = called_from;
    array_push(&self->alive_allocations, allocation);
    
    isize found = 0;
    ASSERT(_debug_allocator_find_allocation(*self, allocation.ptr, &found) == -1 && "Must not be added already!");

    u64 hashed = hash64((u64) allocation.ptr);
    hash_table64_insert(&self->allocation_hash, hashed, self->alive_allocations.size - 1);
    
    self->bytes_allocated += allocation.size;
    self->max_bytes_allocated = MAX(self->max_bytes_allocated, self->bytes_allocated);
}

//typedef struct _Debug_Allocation_Parts
//{
//    Debug_Allocation_Preamble* preamble;
//    u8* pre_dead_zone;
//    void* user_ptr;
//    u8* post_dead_zone;
//    Debug_Allocation_Preamble* postamble;
//} _Debug_Allocation_Parts;
//
//static _Debug_Allocation_Parts _debug_allocator_get_parts(Debug_Allocator* allocator, void* whole_ptr)
//{
//
//}

EXPORT void* debug_allocator_allocate(Allocator* self_, isize new_size, void* old_ptr, isize old_size, isize align, Source_Info called_from)
{
    Debug_Allocator* self = (Debug_Allocator*) (void*) self_;
    _debug_allocator_is_invariant(*self);

    isize preamble_size = sizeof(Debug_Allocation_Preamble);
    isize postamble_size = sizeof(Debug_Allocation_Postamble);
    isize dead_zone_size = self->dead_zone_size;
    isize extra_size = preamble_size + postamble_size + align + 2*dead_zone_size;

    isize total_new_size = new_size + extra_size;
    isize total_old_size = old_size + extra_size;

    //If dealloc only dealloc
    if(old_size == 0)
        total_old_size = 0;

    void* out = self->parent->allocate(self->parent, total_new_size, total_old_size, old_size, align, called_from);

    char* user_ptr = (char*) out + preamble_size + dead_zone_size;
    char* aligned_ptr = (char*) align_forward(user_ptr, align);

    Debug_Allocation_Preamble*  preamble  = ((Debug_Allocation_Preamble*) aligned_ptr) - 1;
    Debug_Allocation_Postamble* postamble = (Debug_Allocation_Postamble*) (aligned_ptr + new_size);

    Debug_Allocation new_allocation = {0};
    new_allocation.align = align;
    new_allocation.ptr = out;
    new_allocation.size = new_size;
    new_allocation.time_s = clock_s();
    
    if(self->do_printing)
    {
        typedef long long int lld;
        printf("[MEM_DEBUG]: size %6lld -> %-6lld ptr: 0x%p -> 0x%p align: %lld " SOURCE_INFO_FORMAT "\n",
            (lld) old_size, (lld) new_size, old_ptr, out, (lld) align, SOURCE_INFO_PRINT(called_from));
    }

    if(new_size == 0)
    {
        _debug_allocator_remove_allocation(self, old_ptr, called_from);
        self->deallocation_count += 1;
    }
    else if(old_ptr == NULL)
    {
        _debug_allocator_add_allocation(self, new_allocation, called_from);
        self->allocation_count += 1;
    }
    else
    {
        _debug_allocator_remove_allocation(self, old_ptr, called_from);
        _debug_allocator_add_allocation(self, new_allocation, called_from);
        self->reallocation_count += 1;
    }
    
    _debug_allocator_is_invariant(*self);
    return out;
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