#pragma once
#include "c_defines.h"
#include "c_allocator.h"
#include <string.h>

#define DEFINE_ARRAY_TYPE(Type, Struct_Name) \
    typedef struct Struct_Name {             \
        Allocator* allocator;                \
        Type* data;                          \
        int64_t size;                        \
        int64_t capacity;                    \
    } Struct_Name                            \

DEFINE_ARRAY_TYPE(uint8_t,  u8_Array);
DEFINE_ARRAY_TYPE(uint16_t, u16_Array);
DEFINE_ARRAY_TYPE(uint32_t, u32_Array);
DEFINE_ARRAY_TYPE(uint64_t, u64_Array);

DEFINE_ARRAY_TYPE(int8_t,   i8_Array);
DEFINE_ARRAY_TYPE(int16_t,  i16_Array);
DEFINE_ARRAY_TYPE(int32_t,  i32_Array);
DEFINE_ARRAY_TYPE(int64_t,  i64_Array);

DEFINE_ARRAY_TYPE(float,    f32_Array);
DEFINE_ARRAY_TYPE(double,   f64_Array);
DEFINE_ARRAY_TYPE(void*,    ptr_Array);

EXPORT void _array_init(void* array, isize item_size, Allocator* allocator, void* backing, int64_t backing_size); 
EXPORT void _array_deinit(void* array, isize item_size, Source_Info from);
EXPORT void _array_set_capacity(void* array, isize item_size, isize capacity, Source_Info from); 
EXPORT void _array_grow_capacity(void* array, isize item_size, isize capacity_at_least, Source_Info from); 
EXPORT bool _array_is_invariant(const void* array, isize item_size);
EXPORT bool _array_is_backed(const void* array, isize item_size);
EXPORT void _array_set_backed(void* array, isize item_size, bool to);
EXPORT void _array_resize(void* array, isize item_size, isize to_size, Source_Info from);
EXPORT void _array_reserve(void* array, isize item_size, isize to_capacity, bool do_growth, Source_Info from);
EXPORT void _array_prepare_push(void* array, isize item_size, Source_Info from);
EXPORT void _array_append(void* array, isize item_size, const void* data, isize data_count, Source_Info from);
EXPORT void _array_unappend(void* array, isize item_size, isize data_count);

#define array_set_capacity(darray_ptr, capacity) \
    _array_set_capacity(darray_ptr, sizeof *(darray_ptr)->data, capacity, SOURCE_INFO())
    
#define array_init(darray_ptr, allocator) \
    /* or you can simply do My_Type_Array array = {allocator}; */ \
    _array_init(darray_ptr, sizeof *(darray_ptr)->data, allocator, NULL, 0)

#define array_init_backed(darray_ptr, allocator, backing_array, backing_array_size) \
    _array_init(darray_ptr, sizeof *(darray_ptr)->data, allocator, backing_array, backing_array_size)

#define array_deinit(darray_ptr) \
    _array_deinit(darray_ptr, sizeof (darray_ptr)->data[0], SOURCE_INFO())

#define array_reserve(darray_ptr, to_capacity) \
    _array_reserve(darray_ptr, sizeof *(darray_ptr)->data, to_capacity, false, SOURCE_INFO()) 

#define array_grow(darray_ptr, to_capacity) \
    _array_reserve(darray_ptr, sizeof *(darray_ptr)->data, to_capacity, true, SOURCE_INFO()) 

#define array_resize(darray_ptr, to_size)              \
    array_reserve(darray_ptr, to_size),                \
    (darray_ptr)->size = to_size                       \
    
#define array_clear(darray_ptr) \
    (darray_ptr)->size = 0

#define array_append(darray_ptr, items, item_count) \
    /* Here is a little hack to typecheck the items array.*/ \
    /* We try to assign the first item to the first data but never actaully run it */ \
    0 ? *(darray_ptr)->data = *(items) : 0, \
    _array_append(darray_ptr, sizeof *(darray_ptr)->data, items, item_count, SOURCE_INFO())
    
#define array_unappend(darray_ptr, item_count) \
    _array_unappend(darray_ptr, sizeof *(darray_ptr)->data, item_count)
        
#define array_assign(darray_ptr, items, item_count) \
    array_clear(darray_ptr), \
    array_append(darray_ptr, items, item_count)
    
#define array_copy(copy_into_darray_ptr, copy_from_darray) \
    array_assign(copy_into_darray_ptr, (copy_from_darray).data, (copy_from_darray).size)

#define array_push(darray_ptr, item_value)            \
    _array_prepare_push(darray_ptr, sizeof *(darray_ptr)->data, SOURCE_INFO()), \
    (darray_ptr)->data[(darray_ptr)->size - 1] = item_value \

#define array_pop(darray_ptr) \
    _array_unappend(darray_ptr, sizeof *(darray_ptr)->data, 1)
    