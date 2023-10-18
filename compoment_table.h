#pragma once


// Compoment table is a base data structure for all compoment systems.
// Compoment can be any type.
// 
// It has the following main design considerations: 
//      1: Efficient id -> compoment ptr mapping 
//         We wish to use compoments and entitites liberaly throught the codebase
//         thus we will often get entity and wish to lookup some of its compoemnt. 
//         This needs to be very efficient. ID can and is shared across systems. Thus
//         we cannot exploit its tructure to provide a constant lookup time
//         (we explot this in handle_table.h with the Handle type). Thus the best 
//         performance we can achieve is a hash table find. We achieve precisely this 
//         level of performance with (*) no added overhead.
// 
//      2: Efficient compoment iteration time
//         Systems such as physics/collision/graphics (so not only one!) need to be updated every frame.
//         These updates include iterating over all compoments thus it is important to do this iteration 
//         fast. This implies the compoments should be ideally tightly packed in a contiguous piece of memory.
//         Further there should not be any unrelated data between the compoments if we ever want to use 
//         SIMD instructions to process them (**).
// 
//      3: Stable pointers
//         To allow to skip the id -> compoment ptr hash lookup we wish to at least locally (within a single frame)
//         reuse the previously obtained ptr. When any other addition/removal of this compoment type causes pointer
//         istability this is almost impossible to achieve. Note that having pointer stability prevents us from having
//         compoments ideally tightly packed. The holes are however filled very fast and dont cause too much trouble.
//         When processing with simd we can simply apply the transform on the holes as well but since they are unused 
//         this is okay.
//  
//  This results in the following structure:
//    
//   HASH:                                                              BLOCK
//  o---------------------------------------o             o----------------------------------------------------------------------o
//  | id hash 32 | offset 32 | block ptr 64 |      o----->| Next block ptr | Comp meta data[BLOCK_SIZE] | Compoments[BLOCK_SIZE] | -> NEXT BLOCK
//  |---------------------------------------|      |      o-----------------------------------^------------------------^---------o  
//  | ...        |    ...    |     ...      |      |                                          |                        |
//  | ...        |    ...    |     ...      |------o                                       offset                    offset
//  | ...        |    ...    |     ...      |
//  | ...        |    ...    |     ...      |
//  | ...        |    ...    |     ...      |
//  o---------------------------------------o
// 
//  On removal we indicate that the compoment slot is empty somehow (probably clearing the id to 0) and add it to
//  free slot collection. This can be either cyclic buffer of pointers + offsets or it can reuse some of the Comp meta data
//  to form a linked list of all empty slots.
// 
//  On addition we first use empty block and add it there. If there is no empty slot we allocate a new block, add all 
//  compoment slots to empty and try again.
// 
//  Comp meta data can include: 
//      1: The full ID (for hash collisions)
//      2: Reference count or generation
//      3: If is unsused pointer to the next unused slot. 
//         This can share the same space as full id and ref count. For example we can indicate empty by giving special meaning to 
//         the lowest bit of ref count and properly handling it. Then we can store in the full ID the pointer to next empty slot block
//         and in ref count store the offset. 
// 
//  (*) within reason of couse. We change some things so the performance is not absolutely equal.
//  (**) Currently we dont use SIMD for anything so we dont do this. Thus we can use simpler structure that stores everything inline. 
//       This means we can use the standard hash_index. See the following diagram
// 
//          HASH:                                                              BLOCK
//  o-------------------------------o             o--------------------------------------------------o
//  | id hash 64 | compoment ptr 64 |             | Next block ptr | Compoments_And_Meta[BLOCK_SIZE] | -> NEXT BLOCK
//  |-------------------------------|             o-----------------------------------^--------------o  
//  | ...        |       ...        |                                                 |                       
//  | ...        |       ...        |-------------------------------------------------o
//  | ...        |       ...        |
//  | ...        |       ...        |
//  | ...        |       ...        |
//  o-------------------------------o
// 
//  Where each Compoments_And_Meta is a struct:
//      o-----------------------o
//      | Comp_Meta | Comp_Data |
//      o-----------------------o
//
// Removal and addtion as well as Comp_Meta composition is similar as described above in the normal case.
// 
// Note that the fact that Comp_Meta and Comp_Data are next to one another is a tradeoff leading to faster 
// random lookup (we only acess one memory location instead of two) but slower iteration time (we cannot use 
// SIMD and must load Comp_Meta even if we dont need and can ignore holes).


#include "hash_index.h"
#include "array.h"
#include "entity.h"

//We can exploit granularity to allocate blocks of nice size

#define COMPOMENT_STORE_ALLOCATION_GRANULARITY 4*PAGE_BYTES

typedef struct Compoment_Store_Block {
    isize size;
    //** bit field of alive and dead slots (uses some number of u64) **
    //** align **
    //** Compoments **
} Compoment_Store_Block; 

typedef struct Compoment_Meta {
    union {
        Guid alive_id;
        Compoment_Meta* dead_next;
    } alive_or_dead;

    u32 ref_count; //if 0 then is dead
    u32 generation;
} Compoment_Meta;

DEFINE_ARRAY_TYPE(Compoment_Store_Block*, Compoment_Store_Block_Ptr_Array);

typedef struct Compoment_Store {
    Hash_Index64 entity_to_compoment_ptr;
    Compoment_Store_Block_Ptr_Array blocks;
    
    isize compoment_size;

} Compoment_Store;


typedef struct Hash_Ptr_Entry {
    u64 hash;
    void* value;
} Hash_Ptr_Entry;


#define _HASH_PTR_EMPTY      (u64) 0
#define _HASH_PTR_GRAVESTONE (u64) 1
#define _HASH_PTR_ALIVE      (u64) 2

void* ptr_high_bits_set(void* ptr, u8 num_bits, u64 bit_pattern)
{
    CHECK_BOUNDS(num_bits, 64 + 1);
    u64 val = (u64) ptr;
    u8 shift = 64 - num_bits;
    u64 mask = ((u64) 1 << shift) - 1;

    u64 out_val = (val & mask) | (bit_pattern << shift);
    return (void*) out_val;
}

u64 ptr_high_bits_get(void* ptr, u8 num_bits)
{
    CHECK_BOUNDS(num_bits, 64 + 1);
    u8 shift = 64 - num_bits;
    u64 val = (u64) ptr;
    u64 pattern = val >> shift;

    return pattern;
}
void* ptr_high_bits_restore(void* ptr, u8 num_bits)
{
    int _high_order_bit_tester = 0;
    u64 pattern = (u64) &_high_order_bit_tester;

    return ptr_high_bits_set(ptr, num_bits, pattern);
}

INTERNAL void _hash_ptr_set_empty(Hash_Ptr_Entry* entry)  
{ 
    entry->value = ptr_high_bits_set(entry->value, 2, _HASH_PTR_EMPTY);
}
INTERNAL void _hash_ptr_set_gravestone(Hash_Ptr_Entry* entry) 
{ 
    entry->value = ptr_high_bits_set(entry->value, 2, _HASH_PTR_GRAVESTONE);
}

//@TODO: Set alive on insert? 
//@DISCUSSION: 
// if we insted of the bottom bits use high bits for empty we can devise a bijective hash
// on the low 62 bits of the number. Such a hash can be used to map (almost) any ptr injectively
// without having to escape the value at all. This way we would not need any special handling of anything
// just use ptr_hash_index_hash64 and profit.
// 
//@DISCUSSION: 
// Still it would be useful to have a hash index where we use patterns inside value. For example when hashing 
// floats such a thing could be very useful since we would not need to validate the reuslt. 
// There is however a question how to do that generally without it being too annoying to use. The answer is
// probably to just use bit pattern of 00 for most things and ptr will have to be careful (separate interface?)
// 
    
INTERNAL bool _hash_ptr_is_empty(const Hash_Ptr_Entry* entry) 
{ 
    return ptr_high_bits_get(entry->value, 2) == _HASH_PTR_EMPTY;
}
INTERNAL bool _hash_ptr_is_gravestone(const Hash_Ptr_Entry* entry) 
{ 
    return ptr_high_bits_get(entry->value, 2) == _HASH_PTR_GRAVESTONE;
}

INTERNAL u64 _hash_ptr_hash_escape(u64 hash)
{
    return hash;
}

INTERNAL void* hash_ptr_get_ptr(void* store)
{
    return ptr_high_bits_restore(store, 2);
}

INTERNAL void* hash_ptr_set_ptr(void* store)
{
    return ptr_high_bits_set(store, 2, _HASH_PTR_ALIVE);
}

#define entry_set_empty        _hash_ptr_set_empty
#define entry_set_gravestone   _hash_ptr_set_gravestone
#define entry_is_empty         _hash_ptr_is_empty
#define entry_is_gravestone    _hash_ptr_is_gravestone
#define entry_hash_escape      _hash_ptr_hash_escape
    
#define Hash_Index  Hash_Index32
#define hash_index  hash_index32
#define Entry       Hash_Ptr_Entry
#define Hash        u32
#define Value       u32

#define HASH_INDEX_TEMPLATE_IMPL
#include "hash_index_template.h"

//
//