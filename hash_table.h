#pragma once
#include "allocator.h"
#include "array.h"

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

typedef struct Hash_Found32
{
    isize hash_index;
    uint32_t value;
} Hash_Found32;

typedef struct Hash_Found64
{
    isize hash_index;
    uint64_t value;
} Hash_Found64;

DEFINE_ARRAY_TYPE(Hash_Table64_Entry, Hash_Table64);
DEFINE_ARRAY_TYPE(Hash_Table32_Entry, Hash_Table32);

#define _HASH_GRAVESTONE 1
#define _HASH_EMPTY 0

void hash_table64_init(Hash_Table64* table, Allocator* allocator);
void hash_table64_init_backed(Hash_Table64* table, Allocator* allocator, void* backing_array, isize backing_array_size);
void hash_table64_deinit(Hash_Table64* table);
void hash_table64_copy(Hash_Table64* to_table, Hash_Table64 from_table);
void hash_table64_clear(Hash_Table64* to_table);

Hash_Found64 hash_table64_find(Hash_Table64 table, uint64_t hash);
Hash_Found64 hash_table64_find_next(Hash_Table64 table, Hash_Found64 prev);
void         hash_table64_rehash(Hash_Table64* table, isize to_size);
Hash_Found64 hash_table64_insert(Hash_Table64* table, uint64_t hash, uint64_t value);
void         hash_table64_remove(Hash_Table64* table, Hash_Found64 found);
bool         hash_table64_is_overful(Hash_Table64 table);

//@TODO: If I dont end up using these just fuse them
bool            lin_probe_hash64_is_invariant(const Hash_Table64_Entry* entries, isize entries_size);
uint64_t        lin_probe_hash64_escape(uint64_t hash);
Hash_Found64    lin_probe_hash64_find(const Hash_Table64_Entry* entries, isize entries_size, uint64_t hash);
Hash_Found64    lin_probe_hash64_find_next(const Hash_Table64_Entry* entries, isize entries_size, Hash_Found64 prev);
void            lin_probe_hash64_rehash(const Hash_Table64_Entry* entries, isize entries_size, Hash_Table64_Entry* new_entries, isize new_entries_size);
Hash_Found64    lin_probe_hash64_insert(Hash_Table64_Entry* entries, isize entries_size, uint64_t hash, uint64_t value);
void            lin_probe_hash64_remove(Hash_Table64_Entry* entries, isize entries_size, Hash_Found64 found);

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

    Hash_Found64 lin_probe_hash64_find(const Hash_Table64_Entry* entries, isize entries_size, uint64_t hash)
    {
        ASSERT(lin_probe_hash64_is_invariant(entries, entries_size));
        Hash_Found64 found = {0};
        found.hash_index = -1;
        if(entries_size <= 0)
            return found;

        uint64_t escaped = lin_probe_hash64_escape(hash);
        uint64_t mask = (uint64_t) entries_size - 1;
        uint64_t i = escaped & mask;
        isize counter = 0;
        for(; entries[i].hash != _HASH_EMPTY; i = (i + 1) & mask)
        {
            ASSERT(counter ++ < entries_size);
            if(entries[i].hash == escaped)
            {
                found.hash_index = i;
                found.value = entries[i].value;
                return found;
            }
        }

        return found;
    }

    Hash_Found64 lin_probe_hash64_find_next(const Hash_Table64_Entry* entries, isize entries_size, Hash_Found64 prev)
    {
        return lin_probe_hash64_find(entries, entries_size, prev.hash_index + 1);
    }

    void lin_probe_hash64_rehash(const Hash_Table64_Entry* entries, isize entries_size, Hash_Table64_Entry* new_entries, isize new_entries_size)
    {
        const isize ENTRY_SIZE = (isize) sizeof(uint64_t);

        ASSERT(lin_probe_hash64_is_invariant(entries, entries_size));
        ASSERT(lin_probe_hash64_is_invariant(new_entries, new_entries_size));

        uint64_t mask = (uint64_t) new_entries_size - 1;
        for(isize i = 0; i < entries_size; i++)
        {
            if(entries[i].hash == _HASH_GRAVESTONE || 
                entries[i].hash == _HASH_EMPTY)
                continue;
            
            uint64_t k = entries->hash & mask;
            isize counter = 0;
            for(; entries[k].hash != _HASH_EMPTY; k = (k + 1) & mask)
                ASSERT(counter ++ < entries_size && "must not be completely full!");

            new_entries[k] = entries[i];
        }
    }   

    Hash_Found64 lin_probe_hash64_insert(Hash_Table64_Entry* entries, isize entries_size, uint64_t hash, uint64_t value) 
    {
        ASSERT(lin_probe_hash64_is_invariant(entries, entries_size));
        uint64_t mask = (uint64_t) entries_size - 1;
    
        uint64_t escaped = lin_probe_hash64_escape(hash);
        uint64_t i = escaped & mask;
        isize counter = 0;
        for(; entries[i].hash > _HASH_GRAVESTONE; i = (i + 1) & mask)
            ASSERT(counter ++ < entries_size && "must not be completely full!");

        entries[i].hash = escaped;
        entries[i].value = value;
        Hash_Found64 found = {0};
        found.hash_index = i;
        found.value = value;
        return found;
    }
    
    void lin_probe_hash64_remove(Hash_Table64_Entry* entries, isize entries_size, Hash_Found64 found) 
    {
        ASSERT(lin_probe_hash64_is_invariant(entries, entries_size));
        CHECK_BOUNDS(found.hash_index, entries_size);

        Hash_Table64_Entry* found_entry = &entries[found.hash_index];

        ASSERT(found_entry->value == found.value);
        found_entry->hash = _HASH_GRAVESTONE;
        found_entry->value = 0;
    }

    void hash_table64_init(Hash_Table64* table, Allocator* allocator)
    {
        array_init(table, allocator);
    }   

    void hash_table64_init_backed(Hash_Table64* table, Allocator* allocator, void* backing_array, isize backing_array_size)
    {
        array_init_backed(table, allocator, backing_array, backing_array_size);
    }

    void hash_table64_deinit(Hash_Table64* table)
    {
        array_deinit(table);
    }

    void hash_table64_copy(Hash_Table64* to_table, Hash_Table64 from_table)
    {
        array_copy(to_table, from_table);
    }

    void hash_table64_clear(Hash_Table64* to_table)
    {
        array_clear(to_table);
        memset(to_table->data, 0, to_table->capacity*sizeof(*to_table->data));
    }

    Hash_Found64 hash_table64_find(Hash_Table64 table, uint64_t hash)
    {
        return lin_probe_hash64_find(table.data, table.capacity, hash);
    }
    Hash_Found64 hash_table64_find_next(Hash_Table64 table, Hash_Found64 prev)
    {
        return lin_probe_hash64_find_next(table.data, table.capacity, prev);
    }
    void hash_table64_rehash(Hash_Table64* table, isize to_size)
    {
        Hash_Table64 fresh = {0};
        Hash_Table64 temp = *table;

        fresh.allocator = table->allocator;
        if(to_size < table->size)
            to_size = table->size;

        isize rehash_to = 16;
        while(rehash_to < to_size)
            rehash_to *= 2;

        array_set_capacity(&fresh, rehash_to);
        fresh.size = table->size;
        ASSERT(fresh.capacity == rehash_to);
        ASSERT(fresh.capacity >= fresh.size);

        lin_probe_hash64_rehash(table->data, table->capacity, fresh.data, fresh.capacity);
        array_deinit(&temp);
        *table = fresh;
    }
    bool hash_table64_is_overful(Hash_Table64 table)
    {
        if(table.size * 2 >= table.capacity)
            return true;
        else
            return false;
    }

    Hash_Found64 hash_table64_insert(Hash_Table64* table, uint64_t hash, uint64_t value)
    {
        if(hash_table64_is_overful(*table))
            hash_table64_rehash(table, table->capacity*2);

        Hash_Found64 out = lin_probe_hash64_insert(table->data, table->capacity, hash, value);
        table->size += 1;
        return out;
    }
    void hash_table64_remove(Hash_Table64* table, Hash_Found64 found)
    {
        ASSERT(table->size > 0);
        lin_probe_hash64_remove(table->data, table->capacity, found);
        table->size -= 1;
    }

#endif