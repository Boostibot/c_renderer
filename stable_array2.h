#pragma once

#include "block_array.h"

#define STABLE_ARRAY2_BLOCK_SIZE 64

typedef struct Stable_Array2_Block_Header {
    u32 magic;
    i32 next_not_filled_i1; 
    u64 mask;
} Stable_Array2_Block_Header;

typedef struct Stable_Array2 {
    Block_Array blocks;
    isize size;
    i32 first_not_filled_i1;
    i32 item_size;
    i32 growth_lin;
    f32 growth_mult;
} Stable_Array2;

EXPORT void  stable_array2_init_custom(Stable_Array2* stable, Allocator* alloc, isize item_size, isize item_align, isize growth_lin, f32 growth_mult);
EXPORT void  stable_array2_init(Stable_Array2* stable, Allocator* alloc, isize item_size);
EXPORT void  stable_array2_deinit(Stable_Array2* stable);

EXPORT isize stable_array2_capacity(const Stable_Array2* stable);
EXPORT void* stable_array2_get_block(const Stable_Array2* stable, isize index);
EXPORT void* stable_array2_get_block_safe(const Stable_Array2* stable, isize index);
EXPORT void* stable_array2_at(const Stable_Array2* stable, isize index);
EXPORT void* stable_array2_at_safe(const Stable_Array2* stable, isize index);
EXPORT void* stable_array2_at_noncrashing(const Stable_Array2* stable, isize index);
EXPORT isize stable_array2_insert(Stable_Array2* stable, void** out);
EXPORT bool  stable_array2_remove(Stable_Array2* stable, isize index);
EXPORT void  stable_array2_reserve(Stable_Array2* stable, isize to);

EXPORT Stable_Array2_Block_Header* stable_array2_get_block_header(const Stable_Array2* stable, void* block_addr);

#define ITERATE_STABLE_ARRAY2_BEGIN(stable, Ptr_Type, ptr, Index_Type, index)                    \
    for(isize _block_i = 0; _block_i < (stable).blocks.size; _block_i++)                        \
    {                                                                                           \
        void* _block = (stable).blocks.data[_block_i];                                          \
        Stable_Array2_Block_Header* _block_header = stable_array2_get_block_header(&(stable), _block); \
        for(isize _item_i = 0; _item_i < STABLE_ARRAY2_BLOCK_SIZE; _item_i++)                    \
        {                                                                                       \
            if(_block_header->mask & ((u64) 1 << _item_i))                                      \
            {                                                                                   \
                Ptr_Type ptr = (Ptr_Type) ((u8*) _block + _item_i*(stable).item_size); (void) ptr;   \
                Index_Type index = (Index_Type) (_item_i + _block_i * STABLE_ARRAY2_BLOCK_SIZE); (void) index; \
                
#define ITERATE_STABLE_ARRAY2_END }}}   

#define _STABLE_ARRAY2_DO_CHECKS
#define _STABLE_ARRAY2_DO_SLOW_CHECKS
#define _STABLE_ARRAY2_DO_FFS
#define _STABLE_ARRAY2_MAGIC                 (u16) 0x5354 /* 'ST' in hex */

typedef struct _Stable_Array2_Lookup {
    isize block_i;
    isize item_i;
    void* block;
    void* item;
} _Stable_Array2_Lookup;

EXPORT void stable_array2_init_custom(Stable_Array2* stable, Allocator* alloc, isize item_size, isize item_align, isize growth_lin, f32 growth_mult)
{
    ASSERT(item_size > 0 && is_power_of_two(item_align));

    stable_array2_deinit(stable);
    block_array_init(&stable->blocks, alloc, item_size*STABLE_ARRAY2_BLOCK_SIZE + sizeof(Stable_Array2_Block_Header), item_align);
    stable->size = 0;
    stable->item_size = (i32) item_size;
    stable->first_not_filled_i1 = 0;
    stable->growth_lin = (i32) growth_lin;
    stable->growth_mult = growth_mult;
}

EXPORT void stable_array2_init(Stable_Array2* stable, Allocator* alloc, isize item_size)
{
    stable_array2_init_custom(stable, alloc, item_size, DEF_ALIGN, STABLE_ARRAY2_BLOCK_SIZE, 1.5f);
}

INTERNAL _Stable_Array2_Lookup _stable_array2_lookup(const Stable_Array2* stable, isize index)
{
    _Stable_Array2_Lookup out = {0};
    out.block_i = index / STABLE_ARRAY2_BLOCK_SIZE;
    out.item_i = index %  STABLE_ARRAY2_BLOCK_SIZE;
    
    out.block = stable->blocks.data[out.block_i];
    out.item = (u8*) out.block + stable->item_size*out.item_i;

    return out;
}

EXPORT isize stable_array2_capacity(const Stable_Array2* stable)
{
    return stable->blocks.size * STABLE_ARRAY2_BLOCK_SIZE;
}

EXPORT Stable_Array2_Block_Header* stable_array2_get_block_header(const Stable_Array2* stable, void* block_addr)
{
    Stable_Array2_Block_Header* block_header = (Stable_Array2_Block_Header*) ((u8*) block_addr + stable->item_size*STABLE_ARRAY2_BLOCK_SIZE);
    return block_header;
}

INTERNAL void _stable_array2_check_invariants(const Stable_Array2* stable);

EXPORT void stable_array2_deinit(Stable_Array2* stable)
{
    block_array_deinit(&stable->blocks);
    stable->size = 0;
    stable->item_size = 0;
    stable->first_not_filled_i1 = 0;
}

EXPORT void* stable_array2_get_block(const Stable_Array2* stable, isize index)
{
    _stable_array2_check_invariants(stable);
    CHECK_BOUNDS(index, stable_array2_capacity(stable));

    isize block_i = index / STABLE_ARRAY2_BLOCK_SIZE;

    void* block = stable->blocks.data[block_i];
    return block;
}

EXPORT void* stable_array2_get_block_safe(const Stable_Array2* stable, isize index)
{
    _stable_array2_check_invariants(stable);
    if(0 <= index && index <= stable_array2_capacity(stable))
    {
        isize block_i = index / STABLE_ARRAY2_BLOCK_SIZE;

        void* block = stable->blocks.data[block_i];
        return block;
    }

    return NULL;
}

EXPORT void* stable_array2_at(const Stable_Array2* stable, isize index)
{
    _stable_array2_check_invariants(stable);
    CHECK_BOUNDS(index, stable_array2_capacity(stable));

    _Stable_Array2_Lookup lookup = _stable_array2_lookup(stable, index);

    #ifdef DO_BOUNDS_CHECKS
    Stable_Array2_Block_Header* block_header = stable_array2_get_block_header(stable, lookup.block);
    u64 bit = (u64) 1 << lookup.item_i;
    bool is_alive = !!(block_header->mask & bit);
    TEST_MSG(is_alive, "Needs to be alive! Use stable_array2_at_safe if unsure!");
    #endif // DO_BOUNDS_CHECKS

    return lookup.item;
}

EXPORT void* stable_array2_at_safe(const Stable_Array2* stable, isize index)
{
    _stable_array2_check_invariants(stable);
    if(0 <= index && index <= stable_array2_capacity(stable))
    {
        _Stable_Array2_Lookup lookup = _stable_array2_lookup(stable, index);
        Stable_Array2_Block_Header* block_header = stable_array2_get_block_header(stable, lookup.block);
        u64 bit = (u64) 1 << lookup.item_i;
        bool is_alive = !!(block_header->mask & bit);
        if(is_alive)
            return lookup.item;
    }

    return NULL;
}

//@TODO: remove
EXPORT void* stable_array2_at_noncrashing(const Stable_Array2* stable, isize index)
{
    _stable_array2_check_invariants(stable);
    if(0 <= index && index <= stable_array2_capacity(stable))
    {
        _Stable_Array2_Lookup lookup = _stable_array2_lookup(stable, index);
        return lookup.item;
    }

    return NULL;
}

EXPORT isize stable_array2_insert(Stable_Array2* stable, void** out)
{
    _stable_array2_check_invariants(stable);
    stable_array2_reserve(stable, stable->size + 1);

    ASSERT_MSG(out != NULL, "out must not be NULL!");
    ASSERT_MSG(stable->first_not_filled_i1 != 0, "needs to have a place thats not filled when we reserved one!");
    isize block_i = stable->first_not_filled_i1 - 1;
    CHECK_BOUNDS(block_i, stable->blocks.size);

    u8* block_ptr = (u8*) stable->blocks.data[block_i];
    Stable_Array2_Block_Header* block_header = stable_array2_get_block_header(stable, block_ptr);
    ASSERT_MSG(~block_header->mask != 0, "Needs to have a free slot");

    bool do_ffs = false;
    #ifdef _STABLE_ARRAY2_DO_FFS
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

    isize out_index = block_i * STABLE_ARRAY2_BLOCK_SIZE + first_empty_index;
    void* out_ptr = block_ptr + first_empty_index * stable->item_size;
    memset(out_ptr, 0, stable->item_size);

    stable->size += 1;
    _stable_array2_check_invariants(stable);

    *out = out_ptr;
    return out_index;
}

EXPORT bool stable_array2_remove(Stable_Array2* stable, isize index)
{
    _stable_array2_check_invariants(stable);
    if(0 <= index && index <= stable_array2_capacity(stable))
    {
        _Stable_Array2_Lookup lookup = _stable_array2_lookup(stable, index);
        Stable_Array2_Block_Header* block_header = stable_array2_get_block_header(stable, lookup.block);
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

        _stable_array2_check_invariants(stable);
        return is_alive;
    }

    return false;
}

EXPORT void stable_array2_reserve(Stable_Array2* stable, isize to_size)
{
    isize capacity = stable_array2_capacity(stable);
    if(to_size > capacity)
    {
        _stable_array2_check_invariants(stable);
        ASSERT_MSG(stable->first_not_filled_i1 == 0, "If there are not empty slots the stable array should really be full");

        i32 blocks_before = stable->blocks.size;
        block_array_grow(&stable->blocks, DIV_ROUND_UP(to_size, STABLE_ARRAY2_BLOCK_SIZE), DIV_ROUND_UP(stable->growth_lin, STABLE_ARRAY2_BLOCK_SIZE), stable->growth_mult);
        i32 blocks_after = stable->blocks.size;
        
        for(i32 block_i = blocks_after; block_i-- > blocks_before; )
        {
            u8* block = (u8*) stable->blocks.data[block_i];
            Stable_Array2_Block_Header* curr_block = stable_array2_get_block_header(stable, block);
            curr_block->mask = 0;
            curr_block->magic = _STABLE_ARRAY2_MAGIC;
            curr_block->next_not_filled_i1 = stable->first_not_filled_i1;
            stable->first_not_filled_i1 = block_i + 1;
        }
        
        _stable_array2_check_invariants(stable);
    }
}

INTERNAL void _stable_array2_check_invariants(const Stable_Array2* stable)
{
    //_block_array_check_invariants(&stable->blocks);

    bool do_checks = false;
    bool do_slow_check = false;

    #if defined(DO_ASSERTS)
    do_checks = true;
    #endif

    #if defined(_STABLE_ARRAY2_DO_SLOW_CHECKS) && defined(DO_ASSERTS_SLOW)
    do_slow_check = true;
    #endif

    #define IS_IN_RANGE(lo, a, hi) ((lo) <= (a) && (a) < (hi))

    if(do_checks)
    {
        TEST_MSG(stable->item_size > 0, 
            "The item size must be valid");

        TEST_MSG((stable->first_not_filled_i1 == 0) == (stable->size == stable_array2_capacity(stable)), 
            "The not filled list is empty exactly when the stable array is completely filled (size == capacity)");
        
        TEST_MSG(IS_IN_RANGE(0, stable->first_not_filled_i1, stable->blocks.size + 1), 
            "The not filled list needs to be in valid range");

        if(do_slow_check)
        {
            isize computed_size = 0;
            isize not_filled_blocks = 0;

            //Check all blocks for valdity
            for(isize i = 0; i < stable->blocks.size; i++)
            {
                u8* block_data = (u8*) stable->blocks.data[i];
                Stable_Array2_Block_Header* block = stable_array2_get_block_header(stable, block_data);

                TEST_MSG(block->magic == _STABLE_ARRAY2_MAGIC,                           
                    "block is not corrupted");
                TEST_MSG(IS_IN_RANGE(0, block->next_not_filled_i1, stable->blocks.size + 1),  
                    "its next not filled needs to be in range");

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
            
                TEST_MSG(IS_IN_RANGE(0, block_i1, stable->blocks.size + 1), "the block needs to be in range");
                void* block_ptr = stable->blocks.data[block_i1 - 1];
                Stable_Array2_Block_Header* block = stable_array2_get_block_header(stable, block_ptr);

                block_i1 = block->next_not_filled_i1;
                linke_list_size += 1;

                TEST_MSG(~block->mask > 0,               "needs to have an empty slot");
                TEST_MSG(iters++ <= stable->blocks.size, "needs to not get stuck in an infinite loop");
            }

            TEST_MSG(linke_list_size == not_filled_blocks, "the number of not_filled blocks needs to be the lenght of the list");
        }
    }

    #undef IS_IN_RANGE
}
