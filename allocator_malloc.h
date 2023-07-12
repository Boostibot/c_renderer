#pragma once
#include "allocator.h"
#include "platform.h"

typedef struct Malloc_Allocator
{
    Allocator allocator;
    const char* name;

    isize bytes_allocated;
    isize max_bytes_allocated;

    isize allocation_count;
    isize deallocation_count;
    isize reallocation_count;
} Malloc_Allocator;

EXPORT Malloc_Allocator malloc_allocator_make();

EXPORT void* _malloc_allocator_allocate(Allocator* self_, isize new_size, void* old_ptr, isize old_size, isize align, Source_Info called_from);
EXPORT Allocator_Stats _malloc_allocator_get_stats(Allocator* self_);

extern Malloc_Allocator global_malloc_allocator;

#if (defined(LIB_ALL_IMPL) || defined(LIB_MALLOC_ALLOCATOR_IMPL)) && !defined(LIB_MALLOC_ALLOCATOR_HAS_IMPL)
#define LIB_MALLOC_ALLOCATOR_HAS_IMPL
Malloc_Allocator global_malloc_allocator = {_malloc_allocator_allocate, _malloc_allocator_get_stats, "global_malloc_allocator"};

EXPORT Malloc_Allocator malloc_allocator_make()
{
    Malloc_Allocator malloc = {0};
    malloc.allocator.allocate = _malloc_allocator_allocate;
    malloc.allocator.get_stats = _malloc_allocator_get_stats;
    return malloc;
}

EXPORT void* _malloc_allocator_allocate(Allocator* self_, isize new_size, void* old_ptr, isize old_size, isize align, Source_Info called_from)
{
    Malloc_Allocator* self = (Malloc_Allocator*) (void*) self_;
    isize size_delta = new_size - old_size;
    if(new_size == 0)
    {
        self->deallocation_count += 1;
        platform_heap_reallocate(0, old_ptr, old_size, align);
        self->bytes_allocated += size_delta;
        return NULL;                
    }

    void* out_ptr = NULL;
    if(old_ptr == NULL)
    {
        self->allocation_count += 1;
        out_ptr = platform_heap_reallocate(new_size, NULL, 0, align);
    }
    else
    {
        self->reallocation_count += 1;
        out_ptr = platform_heap_reallocate(new_size, old_ptr, old_size, align);
    }

    if(out_ptr != NULL)
    {
        self->bytes_allocated += size_delta;
        if(self->max_bytes_allocated < self->bytes_allocated)
            self->max_bytes_allocated = self->bytes_allocated;
    }

    return out_ptr;
}

EXPORT Allocator_Stats _malloc_allocator_get_stats(Allocator* self_)
{
    Malloc_Allocator* self = (Malloc_Allocator*) (void*) self_;
    Allocator_Stats out = {0};
    out.type_name = "Malloc_Allocator";
    out.name = self->name;
    out.parent = NULL;
    out.is_top_level = true;
    out.max_bytes_allocated = self->max_bytes_allocated;
    out.bytes_allocated = self->bytes_allocated;
    out.allocation_count = self->allocation_count;
    out.deallocation_count = self->deallocation_count;
    out.reallocation_count = self->reallocation_count;

    return out;
}
#endif