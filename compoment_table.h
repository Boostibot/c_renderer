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
    Hash_Index entity_to_compoment_ptr;
    Compoment_Store_Block_Ptr_Array blocks;
    
    isize compoment_size;

} Compoment_Store;

//
//