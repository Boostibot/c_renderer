#pragma once
#include "lib/string.h"
#include "lib/platform.h"
#include "lib/sync.h"

typedef struct Atomic_Transfer_Block Atomic_Transfer_Block;
typedef struct Atomic_Transfer_Block {
  struct Atomic_Transfer_Block* _Atomic next;
  int64_t total_size;
  int64_t parts_count;
  //offsets
  //data
} Atomic_Transfer_Block;
  
void atomic_transfer_write_parts(Atomic_Transfer_Block** channel, const void** ptrs, const int64_t* sizes, int64_t count)
{
    int64_t total_size = 0;
    for(int64_t i = 0; i < count; i++)
        total_size += sizes[i];

    Atomic_Transfer_Block* block = (Atomic_Transfer_Block*) malloc(sizeof(Atomic_Transfer_Block) + total_size);
    if(block)
    {
        block->next = NULL;
        block->total_size = total_size;
        block->parts_count = count;

        void* after = block + 1;
        
        int64_t curr_offset = 0;
        int64_t* offsets = (int64_t*) after;
        for(int64_t i = 0; i < count - 1; i++)
        {
            curr_offset += sizes[i];
            offsets[i] = curr_offset; 
        }
        
        int64_t curr_pos = 0;
        uint8_t* parts = (uint8_t*) (void*) (offsets + count);
        for(int64_t i = 0; i < count; i++)
        {
            if(ptrs[i])
                memcpy(parts + curr_pos, ptrs[i], sizes[i]);
            curr_pos += sizes[i];
        }
  
        TODO();
        int64_t cur_transfer = 0;
        (void) cur_transfer;
        
        sync_list_push(channel, block);
    }
}

void atomic_transfer_write(Atomic_Transfer_Block** channel, const void* data, int64_t size)
{
    atomic_transfer_write_parts(channel, &data, &size, 1);
}

Atomic_Transfer_Block* atomic_transfer_read_last(Atomic_Transfer_Block** channel)
{
    return sync_list_pop_all((Atomic_Transfer_Block* _Atomic*) (void*) channel);
}

Atomic_Transfer_Block* atomic_transfer_read(Atomic_Transfer_Block** channel)
{
    return sync_list_pop_all((Atomic_Transfer_Block* _Atomic*) (void*) channel);
}

void* atomic_transfer_block_part(Atomic_Transfer_Block* block, int64_t index)
{
    ASSERT(index < block->parts_count);
    void* after = block + 1;
    if(index == 0)
        return after;

    int64_t* offsets = (int64_t*) after;
    uint8_t* parts = (uint8_t*) (void*) (offsets + block->parts_count);

    return parts + offsets[index - 1];
}

void atomic_transfer_block_free(Atomic_Transfer_Block* block)
{
    free(block);
}