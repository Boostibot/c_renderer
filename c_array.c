#include <string.h>
#include "c_array.h"

EXPORT bool _array_is_invariant(const void* array, isize item_size)
{
    u8_Array* base = (u8_Array*) array;
    bool is_capaicty_correct = 0 <= base->capacity;
    bool is_size_correct = (0 <= base->size && base->size < base->capacity);
    if(base->size == 0)
        is_size_correct = true;

    bool is_data_correct = (base->data == NULL) == (base->capacity == 0);
    bool result = is_capaicty_correct && is_size_correct && is_data_correct;
    return result;
}

EXPORT bool _array_is_backed(const void* array, isize item_size)
{
    u8_Array* base = (u8_Array*) array;
    bool is_backed = (usize) base->allocator & 1;
    return is_backed;
}

EXPORT void _array_set_backed(void* array, isize item_size, bool to)
{
    u8_Array* base = (u8_Array*) array;
    if(to)
        base->allocator = (Allocator*) ((usize) base->allocator | 1);
    else
        base->allocator = (Allocator*) ((usize) base->allocator & ~(usize)3);
}

EXPORT void _array_init(void* array, isize item_size, Allocator* allocator, void* backing, int64_t backing_size)
{
    u8_Array* base = (u8_Array*) array;
    _array_deinit(array, item_size, SOURCE_INFO());

    base->allocator = allocator; 
    isize backing_item_count = backing_size / item_size;
    if(backing != NULL && backing_size != 0)
    {
        base->data = backing;
        base->capacity = backing_item_count;
        memset(base->data, 0, backing_item_count * item_size);
        _array_set_backed(array, item_size, true);
    }
    else
    {
        _array_set_backed(array, item_size, false);
    }

    ASSERT(_array_is_invariant(array, item_size));
}

EXPORT void _array_deinit(void* array, isize item_size, Source_Info from)
{
    u8_Array* base = (u8_Array*) array;
    ASSERT(array != NULL);
    ASSERT(_array_is_invariant(array, item_size));
    if(_array_is_backed(array, item_size) == false && base->data != NULL)
    {
        allocator_deallocate(base->allocator, base->data, base->capacity * item_size, DEF_ALIGN, from);
    }
    
    *base = BRACE_INIT(u8_Array){NULL};
}

EXPORT void _array_set_capacity(void* array, isize item_size, isize capacity, Source_Info from)
{
    u8_Array* base = (u8_Array*) array;
    ASSERT(item_size > 0 && capacity >= 0 && item_size > 0);
    ASSERT(_array_is_invariant(array, item_size));

    if(capacity == 0)
    {
        if(_array_is_backed(array, item_size) == false && base->data != NULL)
            allocator_deallocate(base->allocator, base->data, base->capacity * item_size, DEF_ALIGN, from);
            
        _array_set_backed(array, item_size, false);
        base->capacity = 0;
        base->size = 0;
        base->data = NULL;
        return;
    }

    isize old_byte_size = item_size * base->capacity;
    isize new_byte_size = item_size * capacity;
    if(base->allocator == NULL)
        base->allocator = allocator_get_default();

    if(_array_is_backed(array, item_size))
    {
        void* new_data = allocator_allocate(base->allocator, new_byte_size, DEF_ALIGN, from);
        memmove(new_data, base->data, old_byte_size);
        base->data = (uint8_t*) new_data;
    }
    else
    {
        base->data = (uint8_t*) allocator_reallocate(base->allocator, new_byte_size, base->data, old_byte_size, DEF_ALIGN, from);
    }

    _array_set_backed(array, item_size, false);

    //Clear the allocated to 0
    if(new_byte_size > old_byte_size)
        memset(base->data + old_byte_size, 0, new_byte_size - old_byte_size);

    //trim the size if too big
    base->capacity = capacity;
    if(base->size >= base->capacity)
        base->size = base->capacity - 1;
        
    //escape the string/data
    memset(base->data + base->size*item_size, 0, item_size);
    ASSERT(_array_is_invariant(array, item_size));
}

EXPORT void _array_grow_capacity(void* array, isize item_size, isize capacity_at_least, Source_Info from)
{
    u8_Array* base = (u8_Array*) array;
    ASSERT(_array_is_invariant(array, item_size));
    
    isize new_capacity = base->capacity;
    while(new_capacity < capacity_at_least)
        new_capacity = new_capacity * 3/2 + 8;

    _array_set_capacity(array, item_size, new_capacity, from);
}

EXPORT void _array_resize(void* array, isize item_size, isize to_size, Source_Info from)
{
    u8_Array* base = (u8_Array*) array;
    _array_reserve(base, item_size, to_size, false, from);
    if(to_size > base->size)
        memset(base->data + base->size, 0, (to_size - base->size)*item_size);
        
    base->size = to_size;
    memset(base->data + base->size, 0, item_size);
    ASSERT(_array_is_invariant(array, item_size));
}


EXPORT void _array_reserve(void* array, isize item_size, isize to_fit, bool do_growth, Source_Info from)
{
    ASSERT(_array_is_invariant(array, item_size));
    u8_Array* base = (u8_Array*) array;
    if(base->capacity > to_fit)
        return;

    if(do_growth)
        _array_grow_capacity(array, item_size, to_fit + 1, from);
    else
        _array_set_capacity(array, item_size, to_fit + 1, from);
}

EXPORT void _array_prepare_push(void* array, isize item_size, Source_Info from)
{
    u8_Array* base = (u8_Array*) array;
    _array_reserve(array, item_size, base->size + 1, true, from);
    base->size += 1;
    memset(base->data + base->size*item_size, 0, item_size);
    ASSERT(_array_is_invariant(array, item_size));
}

EXPORT void _array_append(void* array, isize item_size, const void* data, isize data_count, Source_Info from)
{
    u8_Array* base = (u8_Array*) array;
    _array_reserve(base, item_size, base->size+data_count, true, from);
    memmove(base->data + item_size * base->size, data, item_size * data_count);
    base->size += data_count;
    memset(base->data + base->size*item_size, 0, item_size);
    ASSERT(_array_is_invariant(array, item_size));
}

EXPORT void _array_unappend(void* array, isize item_size, isize data_count)
{
    ASSERT(_array_is_invariant(array, item_size));
    u8_Array* base = (u8_Array*) array;
    ASSERT(base->size >= data_count && data_count >= 0);
    base->size -= data_count;

    if(data_count > 0)
        memset(base->data + base->size*item_size, 0, item_size);
}
