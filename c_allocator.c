#include <string.h>
#include "c_array.h"
#include "c_allocator_malloc.h"

#include <stdlib.h>
#include <stdarg.h>
#include <stdio.h>

static Allocator* _mask_allocator_bits(Allocator* self)
{
    //mask off the lower 2 bits of allocator since allocators are required to be at least 4 aligned.
    //Those bits can be used internally to store some extra state (for example if deallocation is necessary)
    usize mask = ~(usize) 3;
    self = (Allocator*)((usize) self & mask);
    return self;
}

EXPORT void* allocator_try_reallocate(Allocator* from_allocator, isize new_size, void* old_ptr, isize old_size, isize align, Source_Info called_from)
{
    Allocator* masked = _mask_allocator_bits(from_allocator);
    ASSERT(masked != NULL && masked->allocate != NULL);
    return masked->allocate(masked, new_size, old_ptr, old_size, align, called_from);
}

EXPORT void* allocator_reallocate(Allocator* from_allocator, isize new_size, void* old_ptr, isize old_size, isize align, Source_Info called_from)
{
    void* obtained = allocator_try_reallocate(from_allocator, new_size, old_ptr, old_size, align, called_from);
    if(obtained == NULL && new_size != 0)
    {
        Allocator_Out_Of_Memory_Func out_of_memory = allocator_get_out_of_memory();
        out_of_memory(from_allocator, new_size, old_ptr, old_size, align, called_from, "");
    }

    return obtained;
}

EXPORT void* allocator_allocate(Allocator* from_allocator, isize new_size, isize align, Source_Info called_from)
{
    return allocator_reallocate(from_allocator, new_size, NULL, 0, align, called_from);
}

EXPORT void allocator_deallocate(Allocator* from_allocator, void* old_ptr,isize old_size, isize align, Source_Info called_from)
{
    allocator_reallocate(from_allocator, 0, old_ptr, old_size, align, called_from);
}

EXPORT Allocator_Stats allocator_get_stats(Allocator* self)
{
    Allocator* masked = _mask_allocator_bits(self);
    ASSERT(masked != NULL && masked->get_stats != NULL);
    return masked->get_stats(masked);
}

static void default_out_of_memory_handler(struct Allocator* allocator, isize new_size, void* old_ptr, isize old_size, isize align, struct Source_Info called_from, const char*format_string, ...);

//@TODO: make thread local
Allocator_Out_Of_Memory_Func out_of_memory_func = default_out_of_memory_handler;
Malloc_Allocator global_malloc_allocator = {_malloc_allocator_allocate, _malloc_allocator_get_stats};
Allocator* default_allocator = (Allocator*) (void*) &global_malloc_allocator;
Allocator* scratch_allocator = (Allocator*) (void*) &global_malloc_allocator;

EXPORT Allocator* allocator_get_default()
{
    return default_allocator;
}
EXPORT Allocator* allocator_get_scratch()
{
    return scratch_allocator;
}

EXPORT Allocator_Swap allocator_push_default(Allocator* new_default)
{
    Allocator_Swap out = {0};
    out.current = new_default;
    out.previous = default_allocator;
    out.is_default = true;

    default_allocator = new_default;

    return out;
}
EXPORT Allocator_Swap allocator_push_scratch(Allocator* new_default)
{
    Allocator_Swap out = {0};
    out.current = new_default;
    out.previous = default_allocator;
    out.is_default = false;

    scratch_allocator = new_default;

    return out;
}
EXPORT void allocator_pop(Allocator_Swap popped)
{
    if(popped.is_default)
    {
        assert(default_allocator == popped.current);
        allocator_push_default(popped.previous);
    }
    else
    {
        assert(scratch_allocator == popped.current);
        allocator_push_scratch(popped.previous);
    }
}

static void default_out_of_memory_handler(struct Allocator* allocator, isize new_size, void* old_ptr, isize old_size, isize align, struct Source_Info called_from, 
                const char* format_string, ...)
{
    typedef long long int lli;
    Allocator_Stats stats = {0};
    if(allocator != NULL && allocator->get_stats != NULL)
        stats = allocator_get_stats(allocator);
        
    if(stats.type_name == NULL)
        stats.type_name = "<no type name>";

    if(stats.name == NULL)
        stats.name = "<no name>";

    fprintf(stderr, 
        "Allocator %s %s ran out of memory\n"
        "new_size:    %lld B\n"
        "old_size:    %lld B\n"
        "called from: \"%s\" %s() (%lld)\n",
        stats.type_name, stats.name, 
        (lli) new_size, (lli) old_size, 
        called_from.file, called_from.function, (lli) called_from.line
    );

    fprintf(stderr, 
        "Allocator_Stats:{\n"
        "  bytes_allocated: %lld\n"
        "  max_bytes_allocated: %lld\n"
        "  allocation_count: %lld\n"
        "  deallocation_count: %lld\n"
        "  reallocation_count: %lld\n"
        "}", 
        (lli) stats.bytes_allocated, (lli) stats.max_bytes_allocated, 
        (lli) stats.allocation_count, (lli) stats.deallocation_count, (lli) stats.reallocation_count
    );
    
    va_list args;
    va_start(args, format_string);
    vfprintf(stderr, format_string, args);
    va_end(args);

    abort();
}

EXPORT Allocator_Out_Of_Memory_Func allocator_get_out_of_memory()
{
    return out_of_memory_func;
}

EXPORT void allocator_set_out_of_memory(Allocator_Out_Of_Memory_Func new_handler)
{
    out_of_memory_func = new_handler;
}