#ifndef LIB_HANDLE_TABLE
#define LIB_HANDLE_TABLE

// Handle_Table is generation counted, stable adressed, safe acess container. 
// 
// Its main purpose is to act as the main storage for all engine systems. 
// Because of this we need:
//   1: Performance*
//      All operations should be in O(1) time. Further this structure should allocate
//      optimally and cause no external fragmentation.
// 
//   2: Lifetime safe access 
//      We need to know if the thing we are acessing is still alive because it might be managed 
//      in some other far away system. We also need to know if the item we are referencing is the 
//      same one we initially referenced. We solve this by generation counters. 
//      This means this container supports weak handles.
// 
//   3: Shared lifetime
//      One item might be comprised of multiple other child items (and those items are integral part of it). 
//      As such when we deinit all members of the item we need to deinit these child items as well.
//      Because other items might be comprised (in an integral part) of the same child items we cannot simply 
//      delete them. We solve this by introducing reference counter.
//      This means this container support shared strong handles.
// 
//      Note that having these shared child items doesnt necessarily mean the usual accidental 
//      interconectedness of unrelated parent items. We can keep the value like appearance of the parents
//      by simply duplicating the child item when it should be modified.
// 
//   4: Stable pointers 
//      We often get a pointer to something through handle and then proceed to add another item. 
//      This addition can be very indirect through some engine system and thus go unnoticed. 
//      We will then use the first pointer causing memroy corruption from the caller. 
//      This bug is annoying and fairly hard to get rid of. 
//  
// The result* is a series of large page size multiple sized stable blocks with densely packed data.  
// In addition we store the generation counters and pointers to the blocks inside lookup accelerating array.
// We use space of the currently unsued item slots to store a linked list of indeces conecting all unsued items together.
// 
// This ensures strong cache coherency on the contained data, near optimal iteration speed when ietarting
// over all items. We achieve O(1) operatiosn: 
//   - lookup by doing double lookup first into the accelerating array then into the appropriate block.
//   - deletion by lookup and then adding the found item into the unused items list.
//   - addition by inserting into the first unsued item slot. if there is no such slot we allocate a new block. 
//     Since all blocks have the same size this is O(1)
// 
// * Not yet. For now we use the least efficient implementation using an evergrowing 
// array of pointers to separately allocated structures. This matches requirements 2 and 3 
// but not 1. However we can change this behind the scenes later.
// 

#include "array.h"

typedef struct Handle {
    i32 index;
    u32 generation;
} Handle;

typedef struct Handle_Table_Slot {
    void* ptr;
    u32 generation;
    i32 references;
} Handle_Table_Slot;

DEFINE_ARRAY_TYPE(Handle_Table_Slot, Handle_Table_Slot_Array);

typedef struct Handle_Table {
    Handle_Table_Slot_Array slots;
    i32 type_size;
    i32 type_align;
} Handle_Table;

#define NULL_HANDLE(T) BRACE_INIT(T){0}
#define HANDLE_IS_NULL(handle) ((handle).h.index == 0)

EXPORT void handle_table_init(Handle_Table* table, Allocator* alloc, i32 type_size, i32 type_align);
EXPORT void handle_table_deinit(Handle_Table* table);

//Adds an item to the table an returns a handle to it.
EXPORT Handle handle_table_add(Handle_Table* table);

//Shares the item referenced by handle increasing its reference count. If handle is invalid returns NULL_HANDLE.
EXPORT Handle handle_table_share(Handle_Table* table, Handle handle); 

//@TODO: figure out a simple interface like handle_table_remove to enable this. Duplicating the data itself might not be a good idea.
//If the item referenced by handle is unique returns back adress of the item and saves to out_handle the original handle. 
//Otherwise adds a new copy of the item and saves a handle to it.
EXPORT void* handle_table_get_unique(Handle_Table* table, Handle handle, Handle* out_handle); 

//Removes the item referenced by handle decreasing its reference count. 
//If the items reference count reaches zero saves it to the removed pointer,
//deallocate its data and returns true. Else returns false.
//If handle is invalid does nothing and returns false.
EXPORT bool handle_table_remove(Handle_Table* table, Handle handle, void* removed);

//Retrives an adress of the item referenced by handle. If the handle is invalid returns NULL.
EXPORT void* handle_table_get(Handle_Table table, Handle handle);

//A macro used to iterate all entries of a handle table.
//This is better than iterating using handles because we can skip all the checks.
#define HANDLE_TABLE_FOR_EACH_BEGIN(handle_table, handle_name, Ptr_Type, ptr_name)      \
    for(isize _i = 0; _i < (handle_table).slots.size; _i++)                             \
    {                                                                                   \
        Handle_Table_Slot* _slot = &(handle_table).slots.data[_i];                      \
        Handle handle_name = {(i32) _i + 1, _slot->generation};                         \
        Ptr_Type ptr_name = (Ptr_Type) _slot->ptr;                                      \
        if(ptr_name)                                                                    \
        {                                                                               \

#define HANDLE_TABLE_FOR_EACH_END }}

#endif
#define LIB_ALL_IMPL

#if (defined(LIB_ALL_IMPL) || defined(LIB_HANDLE_TABLE_IMPL)) && !defined(LIB_HANDLE_TABLE_HAS_IMPL)
#define LIB_HANDLE_TABLE_HAS_IMPL

EXPORT void handle_table_init(Handle_Table* table, Allocator* alloc, i32 type_size, i32 type_align)
{
    ASSERT(type_size > 0 && is_power_of_two(type_align));

    handle_table_deinit(table);
    array_init(&table->slots, alloc);
    table->type_align = type_align;
    table->type_size = type_size;
}
EXPORT void handle_table_deinit(Handle_Table* table)
{
    array_deinit(&table->slots);
    table->type_align = 0;
    table->type_size = 0;
}

INTERNAL Allocator* _handle_table_get_allocator(const Handle_Table* table)
{
    Allocator* alloc = array_get_allocator(table->slots);
    return alloc;
}

INTERNAL Handle_Table_Slot* _handle_table_slot_by_handle(const Handle_Table* table, Handle handle)
{
    if(handle.index < 1 || handle.index > table->slots.size)
        return NULL;

    Handle_Table_Slot* found_slot = &table->slots.data[handle.index - 1];
    return found_slot;
}

EXPORT Handle handle_table_add(Handle_Table* table)
{
    Handle out_handle = {0};
    Allocator* alloc = _handle_table_get_allocator(table);
    for(isize i = 0; i < table->slots.size; i++)
    {
        Handle_Table_Slot* slot = &table->slots.data[i];
        if(slot->ptr == NULL)
            out_handle.index = (i32) i + 1;
    }

    if(out_handle.index == 0)
    {
        Handle_Table_Slot new_slot = {0};
        array_push(&table->slots, new_slot);
        out_handle.index = (i32) table->slots.size;
    }
    
    CHECK_BOUNDS(out_handle.index - 1, table->slots.size);
    Handle_Table_Slot* empty_slot = &table->slots.data[out_handle.index - 1];
    empty_slot->generation += 1;
    empty_slot->references += 1;
    empty_slot->ptr = allocator_allocate(alloc, table->type_size, table->type_align, SOURCE_INFO());
    memset(empty_slot->ptr, 0, table->type_size);

    out_handle.generation = empty_slot->generation;

    return out_handle;
}

EXPORT Handle handle_table_share(Handle_Table* table, Handle handle)
{
    Handle_Table_Slot* found_slot = _handle_table_slot_by_handle(table, handle);
    if(found_slot != NULL && found_slot->generation == handle.generation)
    {
        ASSERT(found_slot->references > 0);
        found_slot->references += 1;
        return handle;
    }
    
    Handle null = {0};
    return null;
}

EXPORT bool handle_table_remove(Handle_Table* table, Handle handle, void* removed)
{
    Handle_Table_Slot* found_slot = _handle_table_slot_by_handle(table, handle);

    bool state = found_slot != NULL && found_slot->generation == handle.generation;
    if(state)
    {
        ASSERT(found_slot->references > 0);
        found_slot->references -= 1;
        if(found_slot->references == 0)
        {
            memmove(removed, found_slot->ptr, table->type_size);

            Allocator* alloc = _handle_table_get_allocator(table);
            allocator_deallocate(alloc, found_slot->ptr, table->type_size, table->type_align, SOURCE_INFO());
            found_slot->ptr = NULL;
            found_slot->generation += 1;
        }
    }

    return state;
}

EXPORT void* handle_table_get(Handle_Table table, Handle handle)
{
    Handle_Table_Slot* found_slot = _handle_table_slot_by_handle(&table, handle);
    if(found_slot != NULL && found_slot->generation == handle.generation)
        return found_slot->ptr;
    else
        return NULL;
}


#endif