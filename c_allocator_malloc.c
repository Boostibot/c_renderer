#include <stdlib.h>
#include "c_allocator_malloc.h"

EXPORT void* _malloc_allocator_allocate(Allocator* self_, isize new_size, void* old_ptr, isize old_size, isize align, Source_Info called_from)
{
    Malloc_Allocator* self = (Malloc_Allocator*) (void*) self_;
    if(new_size == 0)
    {
        self->deallocation_count += 1;
        jot_aligned_free(old_ptr, align);
        self->bytes_allocated -= old_size;
        self->bytes_allocated += new_size;
        return NULL;                
    }

    if(self->max_bytes_allocated < self->bytes_allocated)
        self->max_bytes_allocated = self->bytes_allocated;

    void* out_ptr = NULL;
    if(old_ptr == NULL)
    {
        self->allocation_count += 1;
        out_ptr = jot_aligned_malloc(new_size, align);
    }
    else
    {
        self->reallocation_count += 1;
        out_ptr = jot_aligned_realloc(old_ptr, new_size, align);
    }

    if(out_ptr != NULL)
    {
        self->bytes_allocated -= old_size;
        self->bytes_allocated += new_size;

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
    out.parent = NULL;
    out.is_top_level = true;
    out.max_bytes_allocated = self->max_bytes_allocated;
    out.bytes_allocated = self->bytes_allocated;
    out.allocation_count = self->allocation_count;
    out.deallocation_count = self->deallocation_count;
    out.reallocation_count = self->reallocation_count;

    return out;
}

EXPORT Malloc_Allocator malloc_allocator_make()
{
    Malloc_Allocator malloc = {0};
    malloc.allocator.allocate = _malloc_allocator_allocate;
    malloc.allocator.get_stats = _malloc_allocator_get_stats;
    return malloc;
}

EXPORT void* jot_aligned_realloc(void* old_ptr, isize new_size, isize align)
{
    typedef struct Aligned_Malloc_Header
    {
        u32 align_padding;
        
        #ifndef NDEBUG
        u32 magic_number;
        #endif
    } Aligned_Malloc_Header;

    assert(new_size >= 0 && is_power_of_two(align));
    //For the vast majority of cases the default align will suffice
    if(align <= sizeof(size_t))
    {
        if(new_size == 0)
        {
            free(old_ptr);
            return NULL;
        }

        return realloc(old_ptr, (size_t) new_size);
    }
        
    void* prev_original_ptr = NULL;
    if(old_ptr != NULL)
    {
        Aligned_Malloc_Header* prev_header = (Aligned_Malloc_Header*) old_ptr - 1;
        assert(prev_header->magic_number == 0xABCDABCD && "Heap coruption detetected");
        prev_original_ptr = (uint8_t*) old_ptr - prev_header->align_padding;
    }

    if(new_size == 0)
    {
        free(prev_original_ptr);
        return NULL;
    }

    //Else we allocate extra size for alignemnt and u32 marker
    size_t byte_size = (size_t) (new_size + align) + sizeof(Aligned_Malloc_Header);
    uint8_t* original_ptr = (uint8_t*) realloc(prev_original_ptr, byte_size);
    uint8_t* original_end = original_ptr + byte_size;
    if(original_ptr == NULL)
        return NULL;

    Aligned_Malloc_Header* aligned_ptr = (Aligned_Malloc_Header*) align_forward(original_ptr + sizeof(Aligned_Malloc_Header), align);
    Aligned_Malloc_Header* header = aligned_ptr - 1;

    (void) original_end;
    ASSERT(original_end - (uint8_t*) aligned_ptr >= (isize) byte_size);

    //set the marker thats just before the return adress 
    #ifndef NDEBUG
    header->magic_number = 0xABCDABCD;
    #endif
    header->align_padding = (u32) ((uint8_t*) aligned_ptr - original_ptr);
    return aligned_ptr;
}


EXPORT void* jot_aligned_malloc(isize new_size, isize align)
{
    return jot_aligned_realloc(NULL, new_size, align);
}
    
EXPORT void jot_aligned_free(void* aligned_ptr, isize align)
{
    jot_aligned_realloc(aligned_ptr, 0, align);
}
