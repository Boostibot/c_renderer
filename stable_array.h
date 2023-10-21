#pragma once

#include "allocator.h"

#define STABLE_ARRAY_BLOCK_SIZE 64

typedef struct Stable_Array {
    Allocator* allocator;
    void** blocks;
    isize size;
    i32 blocks_size;
    i32 blocks_capacity;
    i32 first_not_filled_i1;
    i32 item_size;
    i32 item_align;
    i32 growth_lin;
    f32 growth_mult;
} Stable_Array;

EXPORT void  stable_array_init_custom(Stable_Array* stable, Allocator* alloc, isize item_size, isize item_align, isize growth_base, f32 growth_mult);
EXPORT void  stable_array_init(Stable_Array* stable, Allocator* alloc, isize item_size, isize item_align);
EXPORT void  stable_array_deinit(Stable_Array* stable);

EXPORT isize stable_array_capacity(const Stable_Array* stable);
EXPORT void* stable_array_get_block(const Stable_Array* stable, isize index);
EXPORT void* stable_array_get_block_safe(const Stable_Array* stable, isize index);
EXPORT void* stable_array_at(const Stable_Array* stable, isize index);
EXPORT void* stable_array_at_safe(const Stable_Array* stable, isize index);
EXPORT isize stable_array_insert(Stable_Array* stable, void** out);
EXPORT bool  stable_array_remove(Stable_Array* stable, isize index);
EXPORT void  stable_array_reserve(Stable_Array* stable, isize to);

#define _STABLE_ARRAY_DO_CHECKS
#define _STABLE_ARRAY_DO_SLOW_CHECKS
#define _STABLE_ARRAY_DO_FFS
#define _STABLE_ARRAY_MAGIC                 (u16) 0x5354 /* 'ST' in hex */
#define _STABLE_ARRAY_BLOCKS_PTR_ALIGN      64 
#define _STABLE_ARRAY_BLOCKS_PTR_DEF_SIZE   16 
#define _STABLE_ARRAY_BLOCKS_PTR_DEF_GROW   2 

typedef struct _Stable_Array_Block {
    u16 magic;
    u16 was_alloced;
    i32 next_not_filled_i1; 
    u64 mask;
} _Stable_Array_Block;

typedef struct _Stable_Array_Lookup {
    isize block_i;
    isize item_i;
    void* block;
    void* item;
} _Stable_Array_Lookup;

EXPORT void  stable_array_init_custom(Stable_Array* stable, Allocator* alloc, isize item_size, isize item_align, isize growth_base, f32 growth_mult)
{
    ASSERT(item_size > 0 && is_power_of_two(item_align));

    stable_array_deinit(stable);
    stable->allocator = alloc;
    stable->blocks = NULL;
    stable->blocks_capacity = 0;
    stable->blocks_size = 0;
    stable->item_size = (i32) item_size;
    stable->item_align = (i32)item_align;
    stable->growth_lin = (i32) growth_base;
    stable->growth_mult = growth_mult;
    stable->size = 0;
    stable->first_not_filled_i1 = 0;
}

EXPORT void  stable_array_init(Stable_Array* stable, Allocator* alloc, isize item_size, isize item_align)
{
    stable_array_init_custom(stable, alloc, item_size, item_align, 64, 1.5f);
}

INTERNAL _Stable_Array_Lookup _stable_array_lookup(const Stable_Array* stable, isize index)
{
    _Stable_Array_Lookup out = {0};
    out.block_i = index / STABLE_ARRAY_BLOCK_SIZE;
    out.item_i = index %  STABLE_ARRAY_BLOCK_SIZE;
    
    out.block = stable->blocks[out.block_i];
    out.item = (u8*) out.block + stable->item_size*out.item_i;

    return out;
}

EXPORT isize stable_array_capacity(const Stable_Array* stable)
{
    return stable->blocks_size * STABLE_ARRAY_BLOCK_SIZE;
}

INTERNAL _Stable_Array_Block* _stable_array_get_block_header(const Stable_Array* stable, void* block_addr)
{
    _Stable_Array_Block* block_header = (_Stable_Array_Block*) ((u8*) block_addr + stable->item_size*STABLE_ARRAY_BLOCK_SIZE);
    return block_header;
}

INTERNAL void _stable_array_check_invariants(const Stable_Array* stable);

EXPORT void  stable_array_deinit(Stable_Array* stable)
{
    if(stable->blocks_size > 0)
        _stable_array_check_invariants(stable);

    for(isize i = 0; i < stable->blocks_size;)
    {
        void* block = stable->blocks[i];
        _Stable_Array_Block* block_header = _stable_array_get_block_header(stable, block);

        ASSERT(block_header->magic == _STABLE_ARRAY_MAGIC);
        ASSERT(block_header->was_alloced == true);

        isize k = i + 1;
        void* end_block_ptr = block;
        for(; k < stable->blocks_size; k ++)
        {
            void* sub_block = stable->blocks[k];
            _Stable_Array_Block* sub_block_header = _stable_array_get_block_header(stable, sub_block);
            ASSERT(sub_block_header->magic == _STABLE_ARRAY_MAGIC);
            if(sub_block_header->was_alloced)
                break;

            end_block_ptr = sub_block;
        }
        
        _Stable_Array_Block* end_block_header = _stable_array_get_block_header(stable, end_block_ptr);
        ASSERT(end_block_header->magic == _STABLE_ARRAY_MAGIC);
        void* past_end = end_block_header + 1;
            
        isize alloced_bytes = (isize) ((u8*) past_end - (u8*) block);
        isize alloced_align = MAX(DEF_ALIGN, stable->item_align);

        allocator_deallocate(stable->allocator, block, alloced_bytes, alloced_align, SOURCE_INFO());
        i = k;
    }

    allocator_deallocate(stable->allocator, stable->blocks, stable->blocks_capacity * sizeof(void*), _STABLE_ARRAY_BLOCKS_PTR_ALIGN, SOURCE_INFO());
    memset(stable, 0, sizeof *stable);
}

EXPORT void* stable_array_get_block(const Stable_Array* stable, isize index)
{
    _stable_array_check_invariants(stable);
    CHECK_BOUNDS(index, stable_array_capacity(stable));

    isize block_i = index / STABLE_ARRAY_BLOCK_SIZE;

    void* block = stable->blocks[block_i];
    return block;
}

EXPORT void* stable_array_get_block_safe(const Stable_Array* stable, isize index)
{
    _stable_array_check_invariants(stable);
    if(0 <= index && index <= stable_array_capacity(stable))
    {
        isize block_i = index / STABLE_ARRAY_BLOCK_SIZE;

        void* block = stable->blocks[block_i];
        return block;
    }

    return NULL;
}

EXPORT void* stable_array_at(const Stable_Array* stable, isize index)
{
    _stable_array_check_invariants(stable);
    CHECK_BOUNDS(index, stable_array_capacity(stable));

    _Stable_Array_Lookup lookup = _stable_array_lookup(stable, index);

    #ifdef DO_BOUNDS_CHECKS
    _Stable_Array_Block* block_header = _stable_array_get_block_header(stable, lookup.block);
    u64 bit = (u64) 1 << lookup.item_i;
    bool is_alive = !!(block_header->mask & bit);
    TEST_MSG(is_alive, "Needs to be alive! Use stable_array_at_safe if unsure!");
    #endif // DO_BOUNDS_CHECKS

    return lookup.item;
}

EXPORT void* stable_array_at_safe(const Stable_Array* stable, isize index)
{
    _stable_array_check_invariants(stable);
    if(0 <= index && index <= stable_array_capacity(stable))
    {
        _Stable_Array_Lookup lookup = _stable_array_lookup(stable, index);
        _Stable_Array_Block* block_header = _stable_array_get_block_header(stable, lookup.block);
        u64 bit = (u64) 1 << lookup.item_i;
        bool is_alive = !!(block_header->mask & bit);
        if(is_alive)
            return lookup.item;
    }

    return NULL;
}

EXPORT isize stable_array_insert(Stable_Array* stable, void** out)
{
    _stable_array_check_invariants(stable);
    stable_array_reserve(stable, stable->size + 1);

    ASSERT_MSG(out != NULL, "out must not be NULL!");
    ASSERT_MSG(stable->first_not_filled_i1 != 0, "needs to have a place thats not filled when we reserved one!");
    isize block_i = stable->first_not_filled_i1 - 1;
    CHECK_BOUNDS(block_i, stable->blocks_size);

    u8* block_ptr = (u8*) stable->blocks[block_i];
    _Stable_Array_Block* block_header = _stable_array_get_block_header(stable, block_ptr);
    ASSERT_MSG(~block_header->mask != 0, "Needs to have a free slot");

    bool do_ffs = false;
    #ifdef _STABLE_ARRAY_DO_FFS
    do_ffs = true;
    #endif

    isize first_empty_index = -1;
    if(do_ffs)
    {
        first_empty_index = platform_find_first_set_bit64(~block_header->mask);
    }
    else
    {
        for(isize k = 0; k < 64; k++)
        {
            u64 bit = (u64) 1 << k;
            if(~block_header->mask & bit)
            {
                first_empty_index = k;
                break;
            }
        }
    }
    
    ASSERT_MSG(first_empty_index != -1, "Needs to be able to find the free slot");
    
    block_header->mask |= (u64) 1 << first_empty_index;

    //If is full remove from the linked list
    if(~block_header->mask == 0)
    {
        stable->first_not_filled_i1 = block_header->next_not_filled_i1;
        block_header->next_not_filled_i1 = 0;
    }

    isize out_index = block_i * STABLE_ARRAY_BLOCK_SIZE + first_empty_index;
    void* out_ptr = block_ptr + first_empty_index * stable->item_size;
    memset(out_ptr, 0, stable->item_size);

    stable->size += 1;
    _stable_array_check_invariants(stable);

    *out = out_ptr;
    return out_index;
}

EXPORT bool stable_array_remove(Stable_Array* stable, isize index)
{
    _stable_array_check_invariants(stable);
    if(0 <= index && index <= stable_array_capacity(stable))
    {
        _Stable_Array_Lookup lookup = _stable_array_lookup(stable, index);
        _Stable_Array_Block* block_header = _stable_array_get_block_header(stable, lookup.block);
        u64 bit = (u64) 1 << lookup.item_i;

        #ifdef DO_ASSERTS
        memset(lookup.item, 0x55, stable->item_size);
        #endif

        bool is_alive = !!(block_header->mask & bit);

        //If is full
        if(~block_header->mask == 0)
        {
            block_header->next_not_filled_i1 = stable->first_not_filled_i1;
            stable->first_not_filled_i1 = (i32) lookup.block_i + 1;
        }
        
        if(is_alive)
        {
            stable->size -= 1;
            block_header->mask = block_header->mask & ~bit;
        }

        _stable_array_check_invariants(stable);
        return is_alive;
    }

    return false;
}

EXPORT void stable_array_reserve(Stable_Array* stable, isize to)
{
    isize capacity = stable_array_capacity(stable);
    if(to > capacity)
    {
        _stable_array_check_invariants(stable);

        //Grow once and if the growth is not enough use `to` for the next_size.
        //This is so that we can use stable_array_reserve for both growing and
        //exact resizing.
        isize next_size = (isize) ((stable->growth_mult - 1) * capacity) + stable->growth_lin;
        if(next_size < to)
            next_size = to;
        
        isize blocks_after = DIV_ROUND_UP(next_size, STABLE_ARRAY_BLOCK_SIZE);
        isize blocks_before = stable->blocks_size;
        isize blocks_added = blocks_after - blocks_before;
        ASSERT(blocks_added > 0);
        
        //Caluclate the needed size and lign for the combined blocks
        isize alloced_align = MAX(DEF_ALIGN, stable->item_align);
        isize alloced_bytes = 0;
        //If is not over aligned we can just caluclate it without any fancy loops
        if(alloced_align <= 16)
            alloced_bytes = (STABLE_ARRAY_BLOCK_SIZE * stable->item_size + sizeof(_Stable_Array_Block))*blocks_added;
        //else loop and align
        else
        {
            for(isize i = 0; i < blocks_added; i++)
            {
                alloced_bytes = (isize) align_forward((void*) alloced_bytes, alloced_align);
                alloced_bytes += STABLE_ARRAY_BLOCK_SIZE * stable->item_size;
                alloced_bytes += sizeof(_Stable_Array_Block);
            }
        }

        void* allocated_block = allocator_allocate(stable->allocator, alloced_bytes, alloced_align, SOURCE_INFO());
        
        //Optional!
        memset(allocated_block, 0, alloced_bytes);

        //Reize blocks array if needed
        u8* curr_block_addr = (u8*) allocated_block;
        if(stable->blocks_size + blocks_after > stable->blocks_capacity)
        {
            i32 new_capacity = _STABLE_ARRAY_BLOCKS_PTR_DEF_SIZE;
            while(new_capacity < stable->blocks_size + blocks_after)
                new_capacity *= _STABLE_ARRAY_BLOCKS_PTR_DEF_GROW;

            stable->blocks = allocator_reallocate(stable->allocator, new_capacity * sizeof(void*), stable->blocks, stable->blocks_capacity * sizeof(void*), _STABLE_ARRAY_BLOCKS_PTR_ALIGN, SOURCE_INFO());
            stable->blocks_capacity = new_capacity;
        }

        stable->blocks_size = (i32) blocks_after;
        _Stable_Array_Block* last_block = NULL;
        
        //Iterate and add all blocks to the blocks array. 
        //Fill in all the _Stable_Array_Block fields 
        for(isize i = 0; i < blocks_added; i++)
        {
            curr_block_addr = (u8*) align_forward(curr_block_addr, alloced_align);
            stable->blocks[blocks_before + i] = curr_block_addr;

            _Stable_Array_Block* curr_block = _stable_array_get_block_header(stable, curr_block_addr);
            curr_block->mask = 0;
            curr_block->magic = (u16) _STABLE_ARRAY_MAGIC;
            curr_block->next_not_filled_i1 = 0;
            curr_block->was_alloced = false;

            //If is first add it the allocation
            if(i == 0)
                curr_block->was_alloced = true;
            //Else link it
            else
                last_block->next_not_filled_i1 = (i32) (blocks_before + i + 1);

            last_block = curr_block;
            curr_block_addr = (u8*) (curr_block + 1);
        }
        
        last_block->next_not_filled_i1 = stable->first_not_filled_i1;
        stable->first_not_filled_i1 = (i32) (blocks_before + 1);
        _stable_array_check_invariants(stable);
    }
}

void _stable_array_check_invariants(const Stable_Array* stable)
{
    bool do_checks = false;
    bool do_slow_check = false;

    #if defined(DO_ASSERTS)
    do_checks = true;
    #endif

    #if defined(_STABLE_ARRAY_DO_SLOW_CHECKS) && defined(DO_ASSERTS_SLOW)
    do_slow_check = true;
    #endif

    #define IS_IN_RANGE(lo, a, hi) ((lo) <= (a) && (a) < (hi))

    if(do_checks)
    {
        TEST_MSG(stable->item_size > 0 && is_power_of_two(stable->item_align), 
            "The item size and item align are those of a valid C type");

        TEST_MSG((stable->first_not_filled_i1 == 0) == (stable->size == stable_array_capacity(stable)), 
            "The not filled list is empty exactly when the stable array is completely filled (size == capacity)");
        
        TEST_MSG(IS_IN_RANGE(0, stable->first_not_filled_i1, stable->blocks_size + 1), 
            "The not filled list needs to be in valid range");

        if(do_slow_check)
        {
            isize computed_size = 0;
            isize not_filled_blocks = 0;

            //Check all blocks for valdity
            for(isize i = 0; i < stable->blocks_size; i++)
            {
                u8* block_data = (u8*) stable->blocks[i];
                _Stable_Array_Block* block = _stable_array_get_block_header(stable, block_data);

                TEST_MSG(block != NULL,                                                 "block is never empty");
                TEST_MSG(block_data == align_forward(block_data, stable->item_align),   "block is aligned to item_align");
                TEST_MSG(block->magic == _STABLE_ARRAY_MAGIC,                           "block is not corrupted");
                TEST_MSG(IS_IN_RANGE(0, block->next_not_filled_i1, stable->blocks_size + 1),  "its next not filled needs to be in range");

                isize item_count_in_block = 0;
                for(isize k = 0; k < 64; k++)
                {
                    u64 bit = (u64) 1 << k;
                    if(block->mask & bit)
                        item_count_in_block += 1;
                }

                if(item_count_in_block < 64)
                    not_filled_blocks += 1;

                computed_size += item_count_in_block;
            }
            TEST_MSG(computed_size == stable->size, "The size retrieved from the used masks form all blocks needs to be exactly the tracked size");

            //Check not filled linked list for validity
            isize linke_list_size = 0;
            isize iters = 0;
            for(i32 block_i1 = stable->first_not_filled_i1;;)
            {
                if(block_i1 == 0)
                    break;
            
                TEST_MSG(IS_IN_RANGE(0, block_i1, stable->blocks_size + 1), "the block needs to be in range");
                void* block_ptr = stable->blocks[block_i1 - 1];
                _Stable_Array_Block* block = _stable_array_get_block_header(stable, block_ptr);

                block_i1 = block->next_not_filled_i1;
                linke_list_size += 1;

                TEST_MSG(~block->mask > 0,               "needs to have an empty slot");
                TEST_MSG(iters++ <= stable->blocks_size, "needs to not get stuck in an infinite loop");
            }

            TEST_MSG(linke_list_size == not_filled_blocks, "the number of not_filled blocks needs to be the lenght of the list");
        }
    }

    #undef IS_IN_RANGE
}
