#ifndef LIB_HANDLE_TABLE
#define LIB_HANDLE_TABLE

// Handle_Table is generation counted, stable adressed, safe acess container. 
// 
// Its main purpose is to act as the main storage for all engine systems. 
// Because of this we need:
//   1: Performance
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

#include "array.h"
#include "block_array.h"

typedef struct Handle {
    i32 index;
    u32 generation;
} Handle;

typedef struct Handle_Table_Slot {
    u32 generation;
    u32 references;
} Handle_Table_Slot;

DEFINE_ARRAY_TYPE(Handle_Table_Slot, Handle_Table_Slot_Array);

typedef struct Handle_Table {
    Block_Array blocks;
    isize size;
    i32 type_size;
    i32 type_align;

    u32 free_slot_i1;
} Handle_Table;

#define NULL_HANDLE_T(T) BRACE_INIT(T){0}
#define HANDLE_SPREAD(handle) (handle).index, (handle).generation
#define HANDLE_T_FROM_HANDLE(T, handle) BRACE_INIT(T){(handle).index, (handle).generation}
#define HANDLE_CAST(T, handle) *(T*) &handle

#define HANDLE_IS_NULL(handle) ((handle).index == 0)
#define HANDLE_IS_VALID(handle) ((handle).index != 0)
#define HANDLE_FROM_HANDLE(handle) BRACE_INIT(Handle){(handle).index, (handle).generation}

EXPORT void* handle_table_insert(Handle_Table* table, Handle* _out_handle);
EXPORT isize handle_table_remove(Handle_Table* table, const Handle* handle, void* save_to_if_removed, isize save_to_byte_size);
EXPORT void  handle_table_reserve(Handle_Table* table, isize to_size);
EXPORT void* handle_table_get(Handle_Table table, const Handle* handle);
EXPORT void* handle_table_make_shared(Handle_Table* table, const Handle* handle, Handle* out_handle);
EXPORT void* handle_table_make_unique(Handle_Table* table, const Handle* handle, Handle* out_handle);

EXPORT void handle_table_init(Handle_Table* table, Allocator* alloc, i32 type_size, i32 type_align);
EXPORT void handle_table_deinit(Handle_Table* table);

//A macro used to iterate all entries of a handle table.
//This is better than iterating using handles because we can skip all the checks.
#define HANDLE_TABLE_FOR_EACH_BEGIN(handle_table, Handle_Type, handle_name, Ptr_Type, ptr_name)             \
    for(isize _block_i = 0; _block_i < (table).blocks.size; _block_i++)                                     \
    {                                                                                                       \
        u8* _block = (u8*) (table).blocks.data[_block_i];                                                   \
        u32 _slot_size = (table).blocks.block_size / HANDLE_TABLE_BLOCK_SIZE;                               \
                                                                                                            \
        for(isize _item_i = 0; _item_i < HANDLE_TABLE_BLOCK_SIZE; _item_i++)                                \
        {                                                                                                   \
            Handle_Table_Slot* slot = (Handle_Table_Slot*) (_block + _slot_size * _item_i);                 \
            if((slot->references & HANDLE_TABLE_LINK_BIT) == 0)                                             \
            {                                                                                               \
                Ptr_Type ptr_name = (Ptr_Type) slot + 1; ;                                             \
                Handle_Type handle_name = {(i32) (_item_i + _block_i * HANDLE_TABLE_BLOCK_SIZE), slot->references}; \
                (void) ptr_name; (void) handle_name;

    
#define HANDLE_TABLE_FOR_EACH_END }}}

#endif
#define LIB_ALL_IMPL

#if (defined(LIB_ALL_IMPL) || defined(LIB_HANDLE_TABLE_IMPL)) && !defined(LIB_HANDLE_TABLE_HAS_IMPL)
#define LIB_HANDLE_TABLE_HAS_IMPL

#define HANDLE_TABLE_BLOCK_SIZE 32
#define HANDLE_TABLE_LINK_BIT ((u32) 1 << 31) 

EXPORT void handle_table_init(Handle_Table* table, Allocator* alloc, i32 type_size, i32 type_align)
{
    ASSERT(type_size > 0 && is_power_of_two(type_align));

    handle_table_deinit(table);
    i32 u32s = sizeof(u32);
    i32 actual_align = MAX(type_align, u32s);
    i32 actual_size = sizeof(Handle_Table_Slot) + DIV_ROUND_UP(type_size, u32s)*u32s;
    i32 block_size = HANDLE_TABLE_BLOCK_SIZE * actual_size;

    block_array_init(&table->blocks, alloc, block_size, actual_align);
    table->type_align = type_align;
    table->type_size = type_size;
}

EXPORT void handle_table_deinit(Handle_Table* table)
{
    block_array_deinit(&table->blocks);
    table->type_align = 0;
    table->type_size = 0;
}

INTERNAL Handle_Table_Slot* _handle_table_slot_by_index(const Handle_Table* table, u32 index)
{
    u32 block_i = (index - 1) / HANDLE_TABLE_BLOCK_SIZE;
    u32 item_i = (index - 1) % HANDLE_TABLE_BLOCK_SIZE;

    ASSERT(index < table->blocks.size);
    u8* block = (u8*) table->blocks.data[block_i];

    u32 slot_size = table->blocks.block_size / HANDLE_TABLE_BLOCK_SIZE;
    Handle_Table_Slot* slot = (Handle_Table_Slot*) (block + slot_size * item_i);
    return slot;
}

INTERNAL Handle_Table_Slot* _handle_table_slot_by_handle(const Handle_Table* table, Handle handle)
{
    if(handle.index < 1 || handle.index > table->size)
        return NULL;

    Handle_Table_Slot* slot = _handle_table_slot_by_index(table, (u32) handle.index);
    if(slot->generation != handle.generation)
        return NULL;

    return slot;
}

EXPORT void handle_table_reserve(Handle_Table* table, isize to_size)
{
    if(to_size > table->blocks.size * HANDLE_TABLE_BLOCK_SIZE)
    {
        ASSERT_MSG(table->free_slot_i1 == 0, "If there are not empty slots the table should really be full");

        i32 blocks_before = table->blocks.size;
        block_array_grow(&table->blocks, DIV_ROUND_UP(to_size, HANDLE_TABLE_BLOCK_SIZE), 1, 1.5);
        i32 blocks_after = table->blocks.size;
        
        // iterate backwards through the new blocks and add them to the empty list 
        // this is so the free_slot_i1 points to the first and thus we end up 
        // pushing to the "first" index first 
        // (it mostly doesnt matter but why not)
        
        u32 slot_size = table->blocks.block_size / HANDLE_TABLE_BLOCK_SIZE;
        u32 prev_free_slot = table->free_slot_i1 - 1;
        for(i32 block_i = blocks_after; block_i-- > blocks_before; )
        {
            u8* block = (u8*) table->blocks.data[block_i];
            for(i32 item_i = HANDLE_TABLE_BLOCK_SIZE; item_i-- > 0; )
            {
                Handle_Table_Slot* slot = (Handle_Table_Slot*) (block + slot_size * item_i);
                slot->generation = 1;
                slot->references = prev_free_slot | HANDLE_TABLE_LINK_BIT;
                prev_free_slot = block_i * HANDLE_TABLE_BLOCK_SIZE + item_i;
            }
        }

        table->free_slot_i1 = prev_free_slot + 1;
    }
}

EXPORT void* handle_table_insert(Handle_Table* table, Handle* _out_handle)
{
    handle_table_reserve(table, table->size + 1);

    ASSERT(table->free_slot_i1 > 0);

    u32 free_slot_i = table->free_slot_i1 - 1;
    Handle_Table_Slot* slot = _handle_table_slot_by_index(table, free_slot_i);
    ASSERT_MSG(slot->references & HANDLE_TABLE_LINK_BIT, "the empty slot needs to be really empty!");
    
    _out_handle->index = free_slot_i;
    _out_handle->generation = ++slot->generation;

    table->free_slot_i1 = (slot->references & ~HANDLE_TABLE_LINK_BIT) + 1;
    table->size += 1;
    slot->references = 1;

    void* out_ptr = slot + 1;

    return out_ptr;
}

EXPORT isize handle_table_remove(Handle_Table* table, const Handle* handle, void* save_to_if_removed, isize save_to_byte_size)
{
    Handle_Table_Slot* found_slot = _handle_table_slot_by_handle(table, *handle);
    isize new_refs = -1;
    if(found_slot)
    {
        new_refs = --found_slot->references;
        if(new_refs <= 0)
        {
            new_refs = 0;
            void* item = found_slot + 1;
            ASSERT_MSG(save_to_byte_size >= table->type_size, "not enough space given!");
            memmove(save_to_if_removed, item, MIN(save_to_byte_size, table->type_size));
            memset(item, 0, table->type_size);

            found_slot->generation += 1;
            found_slot->references = (table->free_slot_i1 - 1) | HANDLE_TABLE_LINK_BIT;
            table->free_slot_i1 == handle->index + 1;
        }
    }

    return new_refs;
}

EXPORT void* handle_table_get(Handle_Table table, const Handle* handle)
{
    Handle_Table_Slot* found_slot = _handle_table_slot_by_handle(&table, *handle);
    if(found_slot)
        return found_slot + 1;
    else
        return NULL;
}

EXPORT void* handle_table_make_shared(Handle_Table* table, const Handle* handle, Handle* out_handle)
{
    Handle_Table_Slot* found_slot = _handle_table_slot_by_handle(table, *handle);
    if(found_slot)
    {
        found_slot->references += 1;
        *out_handle = *handle;
        return found_slot + 1;
    }

    Handle null = {0};
    *out_handle = null;
    return NULL;
}

EXPORT void* handle_table_make_unique(Handle_Table* table, const Handle* handle, Handle* out_handle)
{
    Handle_Table_Slot* found_slot = _handle_table_slot_by_handle(table, *handle);
    if(found_slot && found_slot->references == 1)
    {
        *out_handle = *handle;
        return found_slot + 1;
    }
    else
        return handle_table_insert(table, out_handle);
}


#endif