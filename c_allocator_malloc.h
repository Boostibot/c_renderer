#pragma once
#include "c_allocator.h"

typedef struct Malloc_Allocator
{
    Allocator allocator;

    isize bytes_allocated;
    isize max_bytes_allocated;

    isize allocation_count;
    isize deallocation_count;
    isize reallocation_count;
} Malloc_Allocator;

///c malloc except allocates aligned
EXPORT void* jot_aligned_malloc(isize byte_size, isize align);
EXPORT void  jot_aligned_free(void* aligned_ptr, isize align);
EXPORT void* jot_aligned_realloc(void* aligned_ptr, isize new_size, isize align);

EXPORT Malloc_Allocator malloc_allocator_make();

EXPORT void* _malloc_allocator_allocate(Allocator* self_, isize new_size, void* old_ptr, isize old_size, isize align, Source_Info called_from);
EXPORT Allocator_Stats _malloc_allocator_get_stats(Allocator* self_);