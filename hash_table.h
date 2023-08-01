#ifndef LIB_HASH_TABLE
#define LIB_HASH_TABLE

#include "allocator.h"

typedef struct Hash_Table64_Entry
{
    uint64_t hash;
    uint64_t value;
} Hash_Table64_Entry;

typedef struct Hash_Table32_Entry
{
    uint32_t hash;
    uint32_t value;
} Hash_Table32_Entry;

typedef struct Hash_Table64
{
    Allocator* allocator;                
    Hash_Table64_Entry* entries;                          
    int32_t size;                        
    int32_t entries_count;                   
} Hash_Table64;

typedef struct Hash_Table32
{
    Allocator* allocator;                
    Hash_Table32_Entry* entries;                          
    int32_t size;                        
    int32_t entries_count;                   
} Hash_Table32;

#define _HASH_GRAVESTONE 1
#define _HASH_EMPTY 0

void hash_table64_init(Hash_Table64* table, Allocator* allocator);
void hash_table64_deinit(Hash_Table64* table);
void hash_table64_copy(Hash_Table64* to_table, Hash_Table64 from_table);
void hash_table64_clear(Hash_Table64* to_table);

isize hash_table64_find(Hash_Table64 table, uint64_t hash);
isize hash_table64_find_next(Hash_Table64 table, uint64_t hash, isize prev_found);
void  hash_table64_rehash(Hash_Table64* table, isize to_size);
void  hash_table64_reserve(Hash_Table64* table, isize to_size); //reserves space such that inserting up to to_size elements will not trigger rehash
isize hash_table64_insert(Hash_Table64* table, uint64_t hash, uint64_t value);
Hash_Table64_Entry hash_table64_remove(Hash_Table64* table, isize found);
bool  hash_table64_is_invariant(Hash_Table64 table);
bool  hash_table64_is_entry_used(Hash_Table64_Entry entry);

//@TODO: If I dont end up using these just fuse them date: 7/4/2023 time to use one month => 8/4/2023 delete
bool     lin_probe_hash64_is_invariant(const Hash_Table64_Entry* entries, isize entries_size);
uint64_t lin_probe_hash64_escape(uint64_t hash);
isize    lin_probe_hash64_find(const Hash_Table64_Entry* entries, isize entries_size, uint64_t hash);
isize    lin_probe_hash64_find_next(const Hash_Table64_Entry* entries, isize entries_size, uint64_t hash, isize prev_found);
void     lin_probe_hash64_rehash(Hash_Table64_Entry* new_entries, isize new_entries_size, const Hash_Table64_Entry* entries, isize entries_size);
isize    lin_probe_hash64_insert(Hash_Table64_Entry* entries, isize entries_size, uint64_t hash, uint64_t value);
void     lin_probe_hash64_remove(Hash_Table64_Entry* entries, isize entries_size, isize found);

#endif

#if (defined(LIB_ALL_IMPL) || defined(LIB_HASH_TABLE_IMPL)) && !defined(LIB_HASH_TABLE_HAS_IMPL)
#define LIB_HASH_TABLE_HAS_IMPL

    #include <string.h>
    bool lin_probe_hash64_is_invariant(const Hash_Table64_Entry* entries, isize entries_size)
    {
        bool not_null = true;
        bool power = is_power_of_two_zero(entries_size);
        if(entries_size > 0)
            not_null = entries != NULL;

        bool invariant = not_null && power;
        ASSERT(invariant);
        return invariant;
    }

    uint64_t lin_probe_hash64_escape(uint64_t hash)
    {
        if(hash == _HASH_GRAVESTONE || hash == _HASH_EMPTY)
            hash += 2;

        return hash;
    }

    isize lin_probe_hash64_find_from(const Hash_Table64_Entry* entries, isize entries_size, uint64_t hash, isize prev_index)
    {
        uint64_t escaped = lin_probe_hash64_escape(hash);
        ASSERT(lin_probe_hash64_is_invariant(entries, entries_size));
        if(entries_size <= 0)
            return -1;
        
        uint64_t mask = (uint64_t) entries_size - 1;
        uint64_t i = prev_index & mask;
        isize counter = 0;
        for(; entries[i].hash != _HASH_EMPTY; i = (i + 1) & mask)
        {
            if(counter >= entries_size)
                break;
            if(entries[i].hash == escaped)
                return i;

            counter += 1;
        }

        return -1;
    }
    
    isize lin_probe_hash64_find(const Hash_Table64_Entry* entries, isize entries_size, uint64_t hash)
    {
        uint64_t escaped = lin_probe_hash64_escape(hash);
        uint64_t mask = (uint64_t) entries_size - 1;
        uint64_t start_at = escaped & mask;
        return lin_probe_hash64_find_from(entries, entries_size, escaped, start_at);
    }

    void lin_probe_hash64_rehash(Hash_Table64_Entry* new_entries, isize new_entries_size, const Hash_Table64_Entry* entries, isize entries_size)
    {
        ASSERT(lin_probe_hash64_is_invariant(entries, entries_size));
        ASSERT(lin_probe_hash64_is_invariant(new_entries, new_entries_size));
       
        if(entries_size > 0)
            ASSERT(new_entries_size != 0);
        
        memset(new_entries, 0, new_entries_size * sizeof *new_entries);

        uint64_t mask = (uint64_t) new_entries_size - 1;
        for(isize i = 0; i < entries_size; i++)
        {
            Hash_Table64_Entry curr = entries[i];
            if(curr.hash == _HASH_GRAVESTONE || 
                curr.hash == _HASH_EMPTY)
                continue;
            

            uint64_t k = curr.hash & mask;
            isize counter = 0;
            for(; new_entries[k].hash != _HASH_EMPTY; k = (k + 1) & mask)
            {
                ASSERT(counter < new_entries_size && "must not be completely full!");
                ASSERT(counter < entries_size && "its impossible to have more then what we started with");
                counter += 1;
            }

            new_entries[k] = curr;
        }
    }   

    isize lin_probe_hash64_insert(Hash_Table64_Entry* entries, isize entries_size, uint64_t hash, uint64_t value) 
    {
        ASSERT(lin_probe_hash64_is_invariant(entries, entries_size));
        ASSERT(entries_size > 0 && "there must be space for insertion");
        uint64_t mask = (uint64_t) entries_size - 1;
    
        uint64_t escaped = lin_probe_hash64_escape(hash);
        uint64_t i = escaped & mask;
        isize counter = 0;
        for(; entries[i].hash > _HASH_GRAVESTONE; i = (i + 1) & mask)
            ASSERT(counter ++ < entries_size && "must not be completely full!");

        entries[i].hash = escaped;
        entries[i].value = value;
        return i;
    }
    
    void lin_probe_hash64_remove(Hash_Table64_Entry* entries, isize entries_size, isize found) 
    {
        ASSERT(lin_probe_hash64_is_invariant(entries, entries_size));
        CHECK_BOUNDS(found, entries_size);

        Hash_Table64_Entry* found_entry = &entries[found];

        //ASSERT(found_entry->value == found.value);
        found_entry->hash = _HASH_GRAVESTONE;
        found_entry->value = 0;
    }

    void hash_table64_init(Hash_Table64* table, Allocator* allocator)
    {
        Hash_Table64 null = {0};
        *table = null;
        table->allocator = allocator;
    }   

    void hash_table64_deinit(Hash_Table64* table)
    {
        ASSERT(hash_table64_is_invariant(*table));
        if(table->entries != NULL)
            allocator_deallocate(table->allocator, table->entries, table->entries_count * sizeof *table->entries, DEF_ALIGN, SOURCE_INFO());

        Hash_Table64 null = {0};
        *table = null;
    }

    static bool _hash_table64_needs_rehash(Hash_Table64* table, isize to_size)
    {
        return to_size * 2 >= table->entries_count;
    }

    static void _hash_table64_init_allocator(Hash_Table64* table)
    {
        if(table->allocator == NULL)
           table->allocator = allocator_get_default();
    }

    void hash_table64_copy(Hash_Table64* to_table, Hash_Table64 from_table)
    {
        ASSERT(hash_table64_is_invariant(*to_table));
        ASSERT(hash_table64_is_invariant(from_table));

        if(_hash_table64_needs_rehash(to_table, from_table.size))
        {   
            int32_t rehash_to = 16;
            while(rehash_to < from_table.size)
                rehash_to *= 2;

            int32_t elem_size = sizeof to_table->entries[0];
            _hash_table64_init_allocator(to_table);
            to_table->entries = allocator_reallocate(to_table->allocator, rehash_to * elem_size, to_table->entries, to_table->entries_count * elem_size, DEF_ALIGN, SOURCE_INFO());
            to_table->entries_count = rehash_to;
        }
        
        to_table->size = from_table.size;
        lin_probe_hash64_rehash(to_table->entries, to_table->entries_count, from_table.entries, from_table.entries_count);
        
        ASSERT(hash_table64_is_invariant(*to_table));
        ASSERT(hash_table64_is_invariant(from_table));
    }

    void hash_table64_clear(Hash_Table64* to_table)
    {
        memset(to_table->entries, 0, to_table->entries_count*sizeof(*to_table->entries));
        to_table->size = 0;
    }

    bool hash_table64_is_invariant(Hash_Table64 table)
    {
        bool has_entries_inv = true;
        bool allocator_inv = true;

        //If one then both
        if(table.entries_count > 0 || table.entries != NULL)
            has_entries_inv = table.entries_count > 0 && table.entries != NULL && table.allocator != NULL;
        
        bool sizes_inv = table.size >= 0 && table.entries_count >= 0;
        bool cap_inv = is_power_of_two_zero(table.entries_count);
        bool final_inv = has_entries_inv && allocator_inv && sizes_inv && cap_inv;

        ASSERT(final_inv);
        return final_inv;
    }
    
    bool  hash_table64_is_entry_used(Hash_Table64_Entry entry)
    {
        return entry.hash > _HASH_GRAVESTONE;
    }

    isize hash_table64_find(Hash_Table64 table, uint64_t hash)
    {
        return lin_probe_hash64_find(table.entries, table.entries_count, hash);
    }
    isize hash_table64_find_next(Hash_Table64 table, uint64_t hash, isize prev_found)
    {
        return lin_probe_hash64_find_from(table.entries, table.entries_count, hash, prev_found + 1);
    }
    void hash_table64_rehash(Hash_Table64* table, isize to_size)
    {
        ASSERT(hash_table64_is_invariant(*table));
        Hash_Table64 rehashed = {0};
        hash_table64_init(&rehashed, table->allocator);
        
        isize rehash_to = 16;
        while(rehash_to < to_size)
            rehash_to *= 2;

        //Cannot shrink beyond needed size
        if(rehash_to <= table->size)
            return;

        isize elem_size = sizeof table->entries[0];
        _hash_table64_init_allocator(&rehashed);
        rehashed.entries = allocator_allocate(rehashed.allocator, rehash_to * elem_size, DEF_ALIGN, SOURCE_INFO());
        rehashed.size = table->size;
        rehashed.entries_count = (int32_t) rehash_to;


        lin_probe_hash64_rehash(rehashed.entries, rehashed.entries_count, table->entries, table->entries_count);
        hash_table64_deinit(table);
        *table = rehashed;
        
        ASSERT(hash_table64_is_invariant(*table));
    }

    void hash_table64_reserve(Hash_Table64* table, isize to_size)
    {
        if(_hash_table64_needs_rehash(table, to_size))
            hash_table64_rehash(table, to_size);
    }

    isize hash_table64_insert(Hash_Table64* table, uint64_t hash, uint64_t value)
    {
        hash_table64_reserve(table, table->size + 1);

        isize out = lin_probe_hash64_insert(table->entries, table->entries_count, hash, value);
        table->size += 1;
        ASSERT(hash_table64_is_invariant(*table));
        return out;
    }
    Hash_Table64_Entry hash_table64_remove(Hash_Table64* table, isize found)
    {
        ASSERT(table->size > 0);
        Hash_Table64_Entry removed = table->entries[found];
        lin_probe_hash64_remove(table->entries, table->entries_count, found);
        table->size -= 1;
        ASSERT(hash_table64_is_invariant(*table));
        return removed;
    }

#endif