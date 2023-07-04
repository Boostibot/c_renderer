#pragma once

#include "array.h"

typedef struct Handle
{
    uint32_t handle;
} Handle;

typedef struct Weak_Handle
{
    uint32_t handle;
    uint32_t generation;
} Weak_Handle;


typedef struct Handle_Table_Slot_Header
{
    uint32_t ref_count;
    uint32_t generation;

} Handle_Table_Slot_Header;

typedef struct Handle_Table_Slot_Empty
{
    uint32_t ref_count;
    uint32_t generation;
    Handle_Table_Slot_Empty* next_empty;
} Handle_Table_Slot_Empty;

typedef struct Handle_Table_Block
{
    u8* data;
    u32 size;
    bool was_allocated;
} Handle_Table_Block;

#define BLOCK_SIZE (u32)64

typedef struct Handle_Table 
{
    Allocator* allocator;
    Handle_Table_Block* blocks;
    u32 blocks_size;
    u32 blocks_capacity;
    u32 granularity;
    u32 item_size;
    u32 size;
    u32 capacity;
    void* first_empty_slot;
} Handle_Table;

void handle_table_init(Handle_Table* table, Allocator* allocator, i32 item_size, i32 granularity)
{
    if(item_size < sizeof(Handle_Table_Slot_Empty*))
        item_size = sizeof(Handle_Table_Slot_Empty*);
}

bool handle_table_is_invariant(Handle_Table table)
{
    return true;
}

void* handle_table_get(Handle_Table* table, Weak_Handle handle)
{
    ASSERT(handle_table_is_invariant(*table));
    u32 index = handle.handle;
    u32 block_index = index / BLOCK_SIZE;
    u32 slot_index = index % BLOCK_SIZE;

    u32 slot_size = table->item_size + sizeof(Handle_Table_Slot_Header);
    Handle_Table_Block* block = &table->blocks[block_index]; 
    Handle_Table_Slot_Header* slot = (Handle_Table_Slot_Header*) (block->data + (u64) slot_size * slot_index);

    if(block_index >= table->blocks_size || slot_index >= block->size)
        return NULL;

    ASSERT(block->data != NULL);
    if(slot->generation != handle.generation)
        return NULL;

    ASSERT(slot->ref_count > 0);
    void* output = slot + 1;
    return output;
}