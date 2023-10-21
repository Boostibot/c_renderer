#ifndef LIB_BLOCK_ARRAY
#define LIB_BLOCK_ARRAY

// This is an array of seperately allocated blocks. The interface is kept as simple as possible.
// 
// Its primary use case is for implementing data structures whose item addresses dont change
// (so called "stable" data structures).
// This is achieved by two layers of indirection: first we have a simple array fo pointers 
// that gets reallocated (is not stable) but within that are pointers to allocated blocks.
// Those are in turn stable. 
//
// Note that the blocks dont have to be allocated each individually. We can allocate multiple
// blocks in row and then just distribute the pointers to the appropriate offsets. This saves
// us a lot of performance. We just need to remmeber to deallocate only the first block in the
// block chunk. For this we store extra bit field of which blocks were allocated at the end of 
// the block ptr unstable array. This is esentially free.
//
// Also note the term BLOCK and not ITEM. This hints at the fact that this really should be used 
// as block = collection of items and not block = item. The reasoning for this is twofold:
//  1: Cahce coherency withing block. 
//     This cache coherency is important if we want to match the iteration speed of regular arrays.
//
//  2: Memory saving. 
//     Simply put if we store every 32nd pointer instead of every single one we save 31*4 = 124 B per
//     block. This is again important for cache use this time for random acess. When we acess a block
//     through index we have to do two pointer fetches. One to obtain its pointer and the other to get to
//     the block. We always need to obtain the adress of the block and thus if the unstable ptr array
//     is all within cahce (<= the smaller the better) we can skip one memery fetch per access.

#include "allocator.h"

typedef struct Block_Array {
    Allocator* allocator;
    void** data;
    i32 size;
    i32 capacity;

    i32 block_size;
    i32 block_align;
} Block_Array;

EXPORT void block_array_init(Block_Array* stable, Allocator* alloc, isize item_size, isize item_align);
EXPORT void block_array_deinit(Block_Array* stable);
EXPORT void block_array_grow(Block_Array* stable, isize to, isize growth_base, f32 growth_mult);
                
#endif

#if (defined(LIB_ALL_IMPL) || defined(LIB_LOG_IMPL)) && !defined(LIB_LOG_HAS_IMPL)
#define LIB_LOG_HAS_IMPL

#define _BLOCK_ARRAY_DO_CHECKS
#define _BLOCK_ARRAY_DO_SLOW_CHECKS

INTERNAL void _block_array_check_invariants(const Block_Array* stable);

INTERNAL u64* _block_array_alloced_mask(const Block_Array* blocks, u32 block_index, u64* bit)
{
    u32 mask_i = block_index / 64;
    u32 bit_i = block_index % 64;

    *bit = (u64) 1 << bit_i;
    return (u64*) &blocks->data[blocks->capacity + mask_i];
}

INTERNAL isize _block_array_alloced_mask_size(isize size)
{
    return DIV_ROUND_UP(size, 64);
}

EXPORT void block_array_grow(Block_Array* blocks, isize to_size, isize growth_lin, f32 growth_mult)
{
    if(blocks->size < to_size)
    {
        _block_array_check_invariants(blocks);
        isize old_size = blocks->size;
        isize new_size = (isize) (blocks->size * growth_mult) + growth_lin;
        new_size = MAX(new_size, to_size);

        isize blocks_added = new_size - old_size;
        ASSERT(blocks_added > 0);

        //If the ptr array needs reallocating
        if(new_size > blocks->capacity)
        {
            isize new_capacity = 16;
            while(new_capacity < new_size)
                new_capacity *= 2;

            isize old_extra = _block_array_alloced_mask_size(blocks->capacity) * sizeof(u64);
            isize old_alloced = blocks->capacity * sizeof(void*);

            isize new_extra = _block_array_alloced_mask_size(new_capacity) * sizeof(u64);
            isize new_alloced = new_capacity * sizeof(void*);
            
            u8* alloced = (u8*) allocator_reallocate(blocks->allocator, new_alloced + new_extra, blocks->data, old_alloced + old_extra, 64, SOURCE_INFO());
            
            u64 alloced_bit1 = 0;
            u64* alloced_mask1 = _block_array_alloced_mask(blocks, (u32) old_size, &alloced_bit1);
            (void) alloced_mask1;

            ASSERT(old_extra <= new_extra);

            //move over the extra info
            memmove(alloced + new_alloced, alloced + old_alloced, old_extra);
            //clear the newly added info
            memset(alloced + new_alloced + old_extra, 0, new_extra - old_extra);
            //zero the added pointers - optional
            memset(alloced + old_alloced, 0, new_alloced - old_alloced);

            blocks->data = (void**) alloced;
            blocks->capacity = (i32) new_capacity;
        }

        //Calculate the needed size for blocks
        isize alloced_blocks_bytes = 0;
        for(isize i = 0; i < blocks_added; i++)
        {
            alloced_blocks_bytes = (isize) align_forward((void*) alloced_blocks_bytes, blocks->block_align);
            alloced_blocks_bytes += blocks->block_size;
        }

        u8* alloced_blocks = (u8*) allocator_allocate(blocks->allocator, alloced_blocks_bytes, blocks->block_align, SOURCE_INFO());
        
        //Optional!
        memset(alloced_blocks, 0, alloced_blocks_bytes);

        //Add the blocks into our array
        u8* curr_block_addr = (u8*) alloced_blocks;
        for(isize i = old_size; i < new_size; i++)
        {
            curr_block_addr = (u8*) align_forward(curr_block_addr, blocks->block_align);
            blocks->data[i] = curr_block_addr;
            curr_block_addr += blocks->block_size;
        }
        
        blocks->size = (i32) new_size;
        u64 alloced_bit = 0;
        u64* alloced_mask = _block_array_alloced_mask(blocks, (u32) old_size, &alloced_bit);
        *alloced_mask |= alloced_bit;

        _block_array_check_invariants(blocks);
    }
}

EXPORT void block_array_deinit(Block_Array* blocks)
{
    if(blocks->capacity > 0)
        _block_array_check_invariants(blocks);

    for(i32 i = 0; i < blocks->size;)
    {
        i32 k = i + 1;
        for(; k < blocks->size; k ++)
        {   
            u64 alloced_bit = 0;
            u64* alloced_mask = _block_array_alloced_mask(blocks, (u32) k, &alloced_bit);
            if(*alloced_mask & alloced_bit)
                break;
        }

        u8* start_block_ptr = (u8*) blocks->data[i];
        u8* end_block_ptr = (u8*) blocks->data[k - 1];

        isize alloced_bytes = (end_block_ptr - start_block_ptr) + blocks->block_size;
        allocator_deallocate(blocks->allocator, start_block_ptr, alloced_bytes, blocks->block_align, SOURCE_INFO());
        i = k;
    }

    isize prev_extra = _block_array_alloced_mask_size(blocks->capacity) * sizeof(u64);
    isize prev_size = blocks->capacity * sizeof(void*);
    allocator_deallocate(blocks->allocator, blocks->data, prev_size + prev_extra, 64, SOURCE_INFO());

    memset(blocks, 0, sizeof *blocks);
}

EXPORT void  block_array_init(Block_Array* stable, Allocator* alloc, isize block_size, isize block_align)
{
    ASSERT(block_size > 0 && is_power_of_two(block_align));

    block_array_deinit(stable);
    stable->allocator = alloc;
    stable->data = NULL;
    stable->capacity = 0;
    stable->block_size = (i32) block_size;
    stable->block_align = (i32)block_align;
    stable->size = 0;
}

INTERNAL void _block_array_check_invariants(const Block_Array* stable)
{
    bool do_checks = false;
    bool do_slow_check = false;

    #if defined(DO_ASSERTS)
    do_checks = true;
    #endif

    #if defined(_BLOCK_ARRAY_DO_SLOW_CHECKS) && defined(DO_ASSERTS_SLOW)
    do_slow_check = true;
    #endif

    if(do_checks)
    {
        if(stable->capacity > 0)
            TEST_MSG((stable->allocator != NULL), 
                "If there is something alloced allocator must not be null!");

        TEST_MSG((stable->data != NULL) == (stable->capacity > 0), 
            "When blocks are alloced capacity is non zero");

        TEST_MSG(stable->size <= stable->capacity, 
            "Size must be smaller than capacity!");

        TEST_MSG(stable->block_size > 0 && is_power_of_two(stable->block_align), 
            "The item size and item align are those of a valid C type");

        //Count allocated blocks
        if(do_slow_check)
        {
            isize alloacted_count = 0;
            for(isize i = 0; i < stable->size; i++)
            {
                void* block = stable->data[i];
                TEST_MSG(block != NULL,                                                     
                    "block is never empty");

                TEST_MSG(block == align_forward(block, stable->block_align), 
                    "the block must be properly aligned");

                u64 alloced_bit = 0;
                u64* alloced_mask = _block_array_alloced_mask(stable, (u32) i, &alloced_bit);
                bool was_alloced = !!(*alloced_mask & alloced_bit);
                if(i == 0)
                    TEST_MSG(was_alloced, 
                        "The first block must be always alloced!");

                if(was_alloced)
                    alloacted_count += 1;
            }

            TEST_MSG(alloacted_count <= stable->size, 
                "The allocated size must be smaller than the total size. ");
        }
    }

    #undef IS_IN_RANGE
}

#endif