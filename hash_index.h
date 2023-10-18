#ifndef LIB_HASH_INDEX
#define LIB_HASH_INDEX

// A simple and flexible linear-probing-hash style hash index.
// 
// We use the term hash index as opposed to hash table because this structure
// doesnt provide the usual hash table ineterface, it marely stores indeces or pointers
// to the key-value data elsewhere.
//
// =================== REASONING ====================
// 
// This approach has certain benefits most importantly enabling to simply
// write SQL style tables where every single row value can have 
// its own accelarating Hash_Index64 (thats where the name comes from). 
// Consider the following table:
//
// OWNER     AGE   NAME         ANIMAL   BIG_CHUNK_OF_DATA
// "Alice"   7     "Timotheo"   cat      /* blob */
// "Bob"     3     "Neo"        dog      /* blob */
// ....
// 
// We wish to query rows in O(1) time by owner, name and age. 
// How do we achieve this with traditional key value hash table? 
// If the key is OWNER then the owner field will be missing from the row 
// (which is bad when we query by NAME). If the key is NAME the name 
// field will be missing. The only solution is to have 3 hash tables:
// 
//   owner_table: OWNER -> index
//   name_table:  NAME -> index
//   age_table:   AGE -> index
//
// and one array:
// 
//   rows_array: Array<(OWNER, AGE, NAME, ANIMAL, BIG_CHUNK_OF_DATA)>
// 
// Now the problem is we are effectively duplicating most of the data in the table!
// This is clearly pointless and impractical especially when its fields need to be updated! 
// 
// Our approach solves this problem by only considering the hashes of the fields 
// and not the fields themselves. So we have:
// 
//   owner_index: hash -> index
//   name_index:  hash -> index
//   age_index:   hash -> index
//   rows_array:  Array<(OWNER, AGE, NAME, ANIMAL, BIG_CHUNK_OF_DATA)>
// 
// Now whenever we lookup row by owner we first hash the owner field
// (by whtever but always the same hash algorhitm) then use the owner_index 
// index to find the row. 
// 
// Only caveat being a hash colision might occur so we need to remmeber to always check if the 
// row we were looking for is the one we actually got! Because of this each table will most often implement its
// own functions (find_by_owner(...), find_by_name(...), find_by_age(...)...).
// 
// See hash_table.h for a simple String -> Any hash table implementation
// 
// =================== IMPLEMENTATION ====================
// 
// We have a simple dynamic array of tuples containing the hashed key and value.
// Hashed key is 64 bit number (can be 32 bit for better perf). Value is usually 
// an index but often we want to store pointers. Thus we again use a 64 bit field.
// 
// Again if we wanted the best performance (as opposed to usability in C) we would
// do 32 bit hash and 32 bit value which is an index. This would gratley improve the 
// cache utilization and thus the lookup speed on larger indeces.
// 
// We do linear probing to find our entries. We use two special values of hashes
// namely 0 (EMPTY) and 1 (GRAVESTONE) to mark empty spots. We rehash really soon
// (on 50% utilization) to prevent pitfalls of linear probing. 
// See implementation for details.
//
// @TODO: Implement robin-hood hashing for better percantage utilization 
// @TODO: Implement a 32 bit variant for faster lookup (was not needed yet)

#include "allocator.h"

typedef struct Hash_Index64_Entry {
    u64 hash;
    u64 value;
} Hash_Index64_Entry;

typedef struct Hash_Index64
{
    Allocator* allocator;                
    Hash_Index64_Entry* entries;                          
    int32_t size;                        
    int32_t entries_count;                   
} Hash_Index64;

typedef struct Hash_Index32_Entry {
    u32 hash;
    u32 value;
} Hash_Index32_Entry;

typedef struct Hash_Index32
{
    Allocator* allocator;                
    Hash_Index32_Entry* entries;                          
    int32_t size;                        
    int32_t entries_count;                   
} Hash_Index32;

EXPORT void  hash_index64_init(Hash_Index64* table, Allocator* allocator);
EXPORT void  hash_index64_deinit(Hash_Index64* table);
EXPORT void  hash_index64_copy(Hash_Index64* to_table, Hash_Index64 from_table);
EXPORT void  hash_index64_clear(Hash_Index64* to_table);
EXPORT isize hash_index64_find(Hash_Index64 table, uint64_t hash);
EXPORT isize hash_index64_find_first(Hash_Index64 table, uint64_t hash, isize* finished_at);
EXPORT isize hash_index64_find_next(Hash_Index64 table, uint64_t hash, isize prev_found, isize* finished_at);
EXPORT isize hash_index64_find_or_insert(Hash_Index64* table, uint64_t hash, uint64_t value_if_inserted);
EXPORT isize hash_index64_rehash(Hash_Index64* table, isize to_size); //rehashes 
EXPORT void  hash_index64_reserve(Hash_Index64* table, isize to_size); //reserves space such that inserting up to to_size elements will not trigger rehash
EXPORT isize hash_index64_insert(Hash_Index64* table, uint64_t hash, uint64_t value);
EXPORT bool  hash_index64_needs_rehash(Hash_Index64 table, isize to_size);
EXPORT Hash_Index64_Entry hash_index64_remove(Hash_Index64* table, isize found);
EXPORT bool  hash_index64_is_invariant(Hash_Index64 table);
EXPORT bool  hash_index64_is_entry_used(Hash_Index64_Entry entry);

EXPORT void  hash_index32_init(Hash_Index32* table, Allocator* allocator);
EXPORT void  hash_index32_deinit(Hash_Index32* table);
EXPORT void  hash_index32_copy(Hash_Index32* to_table, Hash_Index32 from_table);
EXPORT void  hash_index32_clear(Hash_Index32* to_table);
EXPORT isize hash_index32_find(Hash_Index32 table, uint32_t hash);
EXPORT isize hash_index32_find_first(Hash_Index32 table, uint32_t hash, isize* finished_at);
EXPORT isize hash_index32_find_next(Hash_Index32 table, uint32_t hash, isize prev_found, isize* finished_at);
EXPORT isize hash_index32_find_or_insert(Hash_Index32* table, uint32_t hash, uint32_t value_if_inserted);
EXPORT isize hash_index32_rehash(Hash_Index32* table, isize to_size); //rehashes 
EXPORT void  hash_index32_reserve(Hash_Index32* table, isize to_size); //reserves space such that inserting up to to_size elements will not trigger rehash
EXPORT isize hash_index32_insert(Hash_Index32* table, uint32_t hash, uint32_t value);
EXPORT bool  hash_index32_needs_rehash(Hash_Index32 table, isize to_size);
EXPORT Hash_Index32_Entry hash_index32_remove(Hash_Index32* table, isize found);
EXPORT bool  hash_index32_is_invariant(Hash_Index32 table);
EXPORT bool  hash_index32_is_entry_used(Hash_Index32_Entry entry);

#endif

#if (defined(LIB_ALL_IMPL) || defined(LIB_HASH_INDEX_IMPL)) && !defined(LIB_HASH_INDEX_HAS_IMPL)
#define LIB_HASH_INDEX_HAS_IMPL
    #include <string.h>
    
    typedef enum {
        _HASH_EMPTY = 0,
        _HASH_GRAVESTONE = 1,
        _HASH_ALIVE = 2,
    } Hash_Liveliness;

    INTERNAL void _hash_index64_set_empty(Hash_Index64_Entry* entry)  { entry->hash = _HASH_EMPTY; }
    INTERNAL void _hash_index64_set_gravestone(Hash_Index64_Entry* entry) { entry->hash = _HASH_GRAVESTONE; }
    
    INTERNAL bool _hash_index64_is_empty(const Hash_Index64_Entry* entry) { return entry->hash == _HASH_EMPTY; }
    INTERNAL bool _hash_index64_is_gravestone(const Hash_Index64_Entry* entry) { return entry->hash == _HASH_GRAVESTONE; }

    INTERNAL uint64_t _hash_index64_hash_escape(uint64_t hash)
    {
        if(hash == _HASH_GRAVESTONE || hash == _HASH_EMPTY)
            hash += 2;

        return hash;
    }
    
    #define entry_set_empty        _hash_index64_set_empty
    #define entry_set_gravestone   _hash_index64_set_gravestone
    #define entry_is_empty         _hash_index64_is_empty
    #define entry_is_gravestone    _hash_index64_is_gravestone
    #define entry_hash_escape      _hash_index64_hash_escape
    
    #define Hash_Index  Hash_Index64
    #define hash_index  hash_index64
    #define Entry       Hash_Index64_Entry
    #define Hash        u64
    #define Value       u64

    #define HASH_INDEX_TEMPLATE_IMPL
    #define HASH_INDEX_IS_EMPTY_ZERO
    #include "hash_index_template.h"

    
    INTERNAL void _hash_index32_set_empty(Hash_Index32_Entry* entry)  { entry->hash = _HASH_EMPTY; }
    INTERNAL void _hash_index32_set_gravestone(Hash_Index32_Entry* entry) { entry->hash = _HASH_GRAVESTONE; }
    
    INTERNAL bool _hash_index32_is_empty(const Hash_Index32_Entry* entry) { return entry->hash == _HASH_EMPTY; }
    INTERNAL bool _hash_index32_is_gravestone(const Hash_Index32_Entry* entry) { return entry->hash == _HASH_GRAVESTONE; }

    INTERNAL uint64_t _hash_index32_hash_escape(uint32_t hash)
    {
        if(hash == _HASH_GRAVESTONE || hash == _HASH_EMPTY)
            hash += 2;

        return hash;
    }

    #define entry_set_empty        _hash_index32_set_empty
    #define entry_set_gravestone   _hash_index32_set_gravestone
    #define entry_is_empty         _hash_index32_is_empty
    #define entry_is_gravestone    _hash_index32_is_gravestone
    #define entry_hash_escape      _hash_index32_hash_escape
    
    #define Hash_Index  Hash_Index32
    #define hash_index  hash_index32
    #define Entry       Hash_Index32_Entry
    #define Hash        u32
    #define Value       u32

    #define HASH_INDEX_TEMPLATE_IMPL
    #define HASH_INDEX_IS_EMPTY_ZERO
    #include "hash_index_template.h"

#endif