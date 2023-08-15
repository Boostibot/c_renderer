#ifndef LIB_HASH_INDEX
#define LIB_HASH_INDEX

#include "allocator.h"

typedef struct Hash_Index_Entry
{
    uint64_t hash;
    uint64_t value;
} Hash_Index_Entry;

typedef struct Hash_Index
{
    Allocator* allocator;                
    Hash_Index_Entry* entries;                          
    int32_t size;                        
    int32_t entries_count;                   
} Hash_Index;

EXPORT void  hash_index_init(Hash_Index* table, Allocator* allocator);
EXPORT void  hash_index_deinit(Hash_Index* table);
EXPORT void  hash_index_copy(Hash_Index* to_table, Hash_Index from_table);
EXPORT void  hash_index_clear(Hash_Index* to_table);
EXPORT isize hash_index_find(Hash_Index table, uint64_t hash);
EXPORT isize hash_index_find_first(Hash_Index table, uint64_t hash, isize* finished_at);
EXPORT isize hash_index_find_next(Hash_Index table, uint64_t hash, isize prev_found, isize* finished_at);
EXPORT isize hash_index_rehash(Hash_Index* table, isize to_size); //rehashes 
EXPORT void  hash_index_reserve(Hash_Index* table, isize to_size); //reserves space such that inserting up to to_size elements will not trigger rehash
EXPORT isize hash_index_insert(Hash_Index* table, uint64_t hash, uint64_t value);
EXPORT bool  hash_index_needs_rehash(Hash_Index table, isize to_size);

EXPORT Hash_Index_Entry hash_index_remove(Hash_Index* table, isize found);
EXPORT bool  hash_index_is_invariant(Hash_Index table);
EXPORT bool  hash_index_is_entry_used(Hash_Index_Entry entry);

#endif

#if (defined(LIB_ALL_IMPL) || defined(LIB_HASH_INDEX_IMPL)) && !defined(LIB_HASH_INDEX_HAS_IMPL)
#define LIB_HASH_INDEX_HAS_IMPL

    #include <string.h>
    
    #define _HASH_GRAVESTONE 1
    #define _HASH_EMPTY 0

    INTERNAL uint64_t _lin_probe_hash_escape(uint64_t hash)
    {
        if(hash == _HASH_GRAVESTONE || hash == _HASH_EMPTY)
            hash += 2;

        return hash;
    }
    
    INTERNAL isize _lin_probe_hash_find_from(const Hash_Index_Entry* entries, isize entries_size, uint64_t hash, isize prev_index, isize* finished_at)
    {
        uint64_t escaped = _lin_probe_hash_escape(hash);
        if(entries_size <= 0)
        {
            *finished_at = 0;
            return -1;
        }

        CHECK_BOUNDS(prev_index, entries_size);
        uint64_t mask = (uint64_t) entries_size - 1;
        uint64_t i = prev_index & mask;
        isize counter = 0;
        for(; entries[i].hash != _HASH_EMPTY; i = (i + 1) & mask)
        {
            if(counter >= entries_size)
                break;
            if(entries[i].hash == escaped)
            {
                *finished_at = i;
                return i;
            }

            counter += 1;
        }

        *finished_at = i;
        return -1;
    }

    INTERNAL isize _lin_probe_hash_rehash(Hash_Index_Entry* new_entries, isize new_entries_size, const Hash_Index_Entry* entries, isize entries_size)
    {  
        if(entries_size > 0)
            ASSERT(new_entries_size != 0);
        
        memset(new_entries, 0, new_entries_size * sizeof *new_entries);
        
        isize hash_colisions = 0;
        uint64_t mask = (uint64_t) new_entries_size - 1;
        for(isize i = 0; i < entries_size; i++)
        {
            Hash_Index_Entry curr = entries[i];
            if(curr.hash == _HASH_GRAVESTONE || 
                curr.hash == _HASH_EMPTY)
                continue;
            

            uint64_t k = curr.hash & mask;
            isize counter = 0;
            for(; new_entries[k].hash != _HASH_EMPTY; k = (k + 1) & mask)
            {
                hash_colisions += 1;
                ASSERT(counter < new_entries_size && "must not be completely full!");
                ASSERT(counter < entries_size && "its impossible to have more then what we started with");
                counter += 1;
            }

            new_entries[k] = curr;
        }

        return hash_colisions;
    }   

    INTERNAL isize _lin_probe_hash_insert(Hash_Index_Entry* entries, isize entries_size, uint64_t hash, uint64_t value) 
    {
        ASSERT(entries_size > 0 && "there must be space for insertion");
        uint64_t mask = (uint64_t) entries_size - 1;
    
        uint64_t escaped = _lin_probe_hash_escape(hash);
        uint64_t i = escaped & mask;
        isize counter = 0;
        for(; entries[i].hash > _HASH_GRAVESTONE; i = (i + 1) & mask)
            ASSERT(counter ++ < entries_size && "must not be completely full!");

        entries[i].hash = escaped;
        entries[i].value = value;
        return i;
    }
    
    INTERNAL void _lin_probe_hash_remove(Hash_Index_Entry* entries, isize entries_size, isize found) 
    {
        CHECK_BOUNDS(found, entries_size);

        Hash_Index_Entry* found_entry = &entries[found];

        //ASSERT(found_entry->value == found.value);
        found_entry->hash = _HASH_GRAVESTONE;
        found_entry->value = 0;
    }

    EXPORT void hash_index_init(Hash_Index* table, Allocator* allocator)
    {
        Hash_Index null = {0};
        *table = null;
        table->allocator = allocator;
    }   

    EXPORT void hash_index_deinit(Hash_Index* table)
    {
        ASSERT(hash_index_is_invariant(*table));
        if(table->entries != NULL)
            allocator_deallocate(table->allocator, table->entries, table->entries_count * sizeof *table->entries, DEF_ALIGN, SOURCE_INFO());

        Hash_Index null = {0};
        *table = null;
    }

    EXPORT bool hash_index_needs_rehash(Hash_Index table, isize to_size)
    {
        return to_size * 2 >= table.entries_count;
    }

    EXPORT void hash_index_copy(Hash_Index* to_table, Hash_Index from_table)
    {
        ASSERT(hash_index_is_invariant(*to_table));
        ASSERT(hash_index_is_invariant(from_table));

        if(hash_index_needs_rehash(*to_table, from_table.size))
        {   
            int32_t rehash_to = 16;
            while(rehash_to < from_table.size)
                rehash_to *= 2;

            int32_t elem_size = sizeof to_table->entries[0];
            if(to_table->allocator == NULL)
               to_table->allocator = allocator_get_default();
            to_table->entries = allocator_reallocate(to_table->allocator, rehash_to * elem_size, to_table->entries, to_table->entries_count * elem_size, DEF_ALIGN, SOURCE_INFO());
            to_table->entries_count = rehash_to;
        }
        
        to_table->size = from_table.size;
        _lin_probe_hash_rehash(to_table->entries, to_table->entries_count, from_table.entries, from_table.entries_count);
        
        ASSERT(hash_index_is_invariant(*to_table));
        ASSERT(hash_index_is_invariant(from_table));
    }

    EXPORT void hash_index_clear(Hash_Index* to_table)
    {
        memset(to_table->entries, 0, to_table->entries_count*sizeof(*to_table->entries));
        to_table->size = 0;
    }
    
    EXPORT bool hash_index_is_invariant(Hash_Index table)
    {
        bool ptr_size_inv = (table.entries == NULL) == (table.entries_count == 0);
        bool allocator_inv = true;
        if(table.entries != NULL)
            allocator_inv = table.allocator != NULL;

        bool sizes_inv = table.size >= 0 && table.entries_count >= 0 && table.entries_count >= table.size;
        bool cap_inv = is_power_of_two_zero(table.entries_count);
        bool final_inv = ptr_size_inv && allocator_inv && cap_inv && sizes_inv && cap_inv;

        ASSERT(final_inv);
        return final_inv;
    }
    
    EXPORT bool  hash_index_is_entry_used(Hash_Index_Entry entry)
    {
        return entry.hash > _HASH_GRAVESTONE;
    }
    
    EXPORT isize hash_index_find_first(Hash_Index table, uint64_t hash, isize* finished_at)
    {
        uint64_t escaped = _lin_probe_hash_escape(hash);
        uint64_t mask = (uint64_t) table.entries_count - 1;
        uint64_t start_at = escaped & mask;
        return _lin_probe_hash_find_from(table.entries, table.entries_count, escaped, start_at, finished_at);
    }
    EXPORT isize hash_index_find(Hash_Index table, uint64_t hash)
    {
        isize finished_at = 0;
        return hash_index_find_first(table, hash, &finished_at);
    }
    EXPORT isize hash_index_find_next(Hash_Index table, uint64_t hash, isize prev_found, isize* finished_at)
    {
        return _lin_probe_hash_find_from(table.entries, table.entries_count, hash, prev_found + 1, finished_at);
    }
    EXPORT isize hash_index_rehash(Hash_Index* table, isize to_size)
    {
        ASSERT(hash_index_is_invariant(*table));
        Hash_Index rehashed = {0};
        hash_index_init(&rehashed, table->allocator);
        
        isize rehash_to = 16;
        while(rehash_to < to_size)
            rehash_to *= 2;

        //Cannot shrink beyond needed size
        if(rehash_to <= table->size)
            return 0;

        isize elem_size = sizeof table->entries[0];
        if(rehashed.allocator == NULL)
           rehashed.allocator = allocator_get_default();

        rehashed.entries = allocator_allocate(rehashed.allocator, rehash_to * elem_size, DEF_ALIGN, SOURCE_INFO());
        rehashed.size = table->size;
        rehashed.entries_count = (int32_t) rehash_to;


        isize hash_colisions = _lin_probe_hash_rehash(rehashed.entries, rehashed.entries_count, table->entries, table->entries_count);
        hash_index_deinit(table);
        *table = rehashed;
        
        ASSERT(hash_index_is_invariant(*table));
        return hash_colisions;
    }

    EXPORT void hash_index_reserve(Hash_Index* table, isize to_size)
    {
        if(hash_index_needs_rehash(*table, to_size))
            hash_index_rehash(table, to_size);
    }

    EXPORT isize hash_index_insert(Hash_Index* table, uint64_t hash, uint64_t value)
    {
        hash_index_reserve(table, table->size + 1);

        isize out = _lin_probe_hash_insert(table->entries, table->entries_count, hash, value);
        table->size += 1;
        ASSERT(hash_index_is_invariant(*table));
        return out;
    }
    EXPORT Hash_Index_Entry hash_index_remove(Hash_Index* table, isize found)
    {
        ASSERT(table->size > 0);
        Hash_Index_Entry removed = table->entries[found];
        _lin_probe_hash_remove(table->entries, table->entries_count, found);
        table->size -= 1;
        ASSERT(hash_index_is_invariant(*table));
        return removed;
    }

#endif