#pragma once

#include "lib/hash_index.h"
#include "lib/allocator.h"

typedef uint64_t ID;

typedef union Link_Table_Hash_Val {
    uint64_t packed;
    struct {
        uint32_t index;
        uint32_t size;
    };
} Link_Table_Hash_Val;

typedef union Link {
    struct {
        uint32_t from_next;
        uint32_t from_prev;
        uint32_t to_next;
        uint32_t to_prev;
        ID from;
        ID to;
    };
    uint32_t next_free;
} Link;

typedef struct Link_Table {
    Allocator* allocator;
    Link* links;

    Hash_Index from_hash;
    Hash_Index to_hash;

    uint32_t first_free_link;
    uint32_t links_size;
    uint32_t links_capacity;

    bool resize_disallowed;
} Link_Table;

void link_table_reserve(Link_Table* table, Allocator* allocator, isize to_size)
{
    if(table->allocator == NULL)
    {
        table->allocator = allocator_or_default(allocator);
        hash_index_init(&table->from_hash, table->allocator);
        hash_index_init(&table->to_hash, table->allocator);

        //Prevent jumping all over memory
        table->from_hash.do_in_place_rehash = true;
        table->to_hash.do_in_place_rehash = true;
    }

    if(to_size >= table->links_capacity)
    {
        TEST(table->resize_disallowed == false, 
            "Link table needed to be resized with:"
            "\n size %i"
            "\n capacity %i" 
            "\n from_hash: {size: %i entries_count: %i}"
            "\n to_hash: {size: %i entries_count: %i}", 
            table->links_size, table->links_capacity,
            table->from_hash.size, table->from_hash.entries_count,
            table->to_hash.size, table->to_hash.entries_count,
        );

        isize new_capacity = to_size;
        isize growth_step = table->links_capacity * 3/2 + 8;
        if(new_capacity < growth_step)
            new_capacity = growth_step;

        allocator_reallocate(table->allocator, new_capacity*sizeof(Link), &table->links, table->links_capacity*sizeof(Link), DEF_ALIGN);
        memset(&table->links[table->links_capacity], 0, (new_capacity - table->links_capacity)*sizeof(Link));

        //Add all to the free list
        for(isize i = new_capacity; i-- > table->links_capacity; )
        {
            if(table->first_free_link != 0)
                table->links[table->first_free_link].next_free = i;

            table->first_free_link = i;
        }
    }
}

isize _find_from_link_forward(Link_Table* table, u32 start_index, u32 max_size, ID from)
{
    u32 iter = 0;
    for(u32 link_i = start_index; link_i != -1; link_i = table->links[link_i].to_next, iter ++)
    {
        Link* link = &table->links[link_i];
        ASSERT(iter < max_size);
        if(link->from == from)
            return link_i;
    }

    return -1;
}

isize _find_to_link_forward(Link_Table* table, u32 start_index, u32 max_size, ID to)
{
    u32 iter = 0;
    for(u32 link_i = start_index; link_i != -1; link_i = table->links[link_i].from_next, iter ++)
    {
        Link* link = &table->links[link_i];
        ASSERT(iter < max_size);
        if(link->to == to)
            return link_i;
    }

    return -1;
}

isize link_table_find(Link_Table* table, ID from, ID to)
{
    isize found = -1;
    isize from_hash_i = hash_index_find(table->from_hash, from);
    if(from_hash_i != -1)
    {
        Link_Table_Hash_Val from_val = {table->from_hash.entries[from_hash_i].value};
        found = _find_to_link_forward(table, from_val.index, from_val.size, to);
    }

    return found;
}

ID* link_table_find_all_from(Link_Table* table, ID from, Allocator* alloc)
{
    ID* out = NULL;
    ASSERT(alloc != NULL);
    isize from_hash_i = hash_index_find(table->from_hash, from);
    if(from_hash_i != -1)
    {
        Link_Table_Hash_Val from_val = {table->from_hash.entries[from_hash_i].value};
        out = (ID*) allocator_allocate(alloc, from_val.size*sizeof(ID), DEF_ALIGN);

        u32 iter = 0;
        for(u32 link_i = from_val.index; link_i != -1; link_i = table->links[link_i].to_next, iter ++)
        {
            ASSERT(iter < from_val.size);
            out[iter] = table->links[link_i].to;
        }
    }

    return out;
}

ID* link_table_find_all_to(Link_Table* table, ID to, Allocator* alloc)
{
    ID* out = NULL;
    ASSERT(alloc != NULL);
    isize to_hash_i = hash_index_find(table->to_hash, to);
    if(to_hash_i != -1)
    {
        Link_Table_Hash_Val to_val = {table->to_hash.entries[to_hash_i].value};
        out = (ID*) allocator_allocate(alloc, to_val.size*sizeof(ID), DEF_ALIGN);

        u32 iter = 0;
        for(u32 link_i = to_val.index; link_i != -1; link_i = table->links[link_i].to_next, iter ++)
        {
            ASSERT(iter < to_val.size);
            out[iter] = table->links[link_i].to;
        }
    }

    return out;
}

isize link_table_insert(Link_Table* table, ID from, ID to)
{
    isize found = -1;
    isize link_i = table->first_free_link;
    //Link_Table_Hash_Val if_inserted = {0};
    //if_inserted.index = link_i;
    //if_inserted.size = 0;s
    
    ASSERT(link_i != 0);
    isize from_hash_i = hash_index_find_or_insert(&table->from_hash, from, 0);
    isize to_hash_i = hash_index_find_or_insert(&table->to_hash, to, 0);
    ASSERT(from_hash_i != -1 && to_hash_i != -1);
    
    Link_Table_Hash_Val* from_val = (Link_Table_Hash_Val*) (void*) &table->from_hash.entries[from_hash_i].value;
    Link_Table_Hash_Val* to_val = (Link_Table_Hash_Val*) (void*) &table->to_hash.entries[to_hash_i].value;

    //If at least one was just added then the link cant
    // possibly be inside.
    if(from_val->size != 0 && to_val->size != 0)
    {
        isize found_i = _find_to_link_forward(table, from_val->index, from_val->size, to);
        if(found_i != -1)
            return found_i;
    }

    Link* link = &table->links[link_i];
    table->first_free_link = link->next_free;

    link->from = from;
    link->from_prev = 0;
    link->from_next = from_val->index;
    link->to = to;
    link->to_prev = 0;
    link->to_next = to_val->index;

    //Link to front
    table->links[from_val->index].from_prev = link_i;
    table->links[to_val->index].to_prev = link_i;

    //Update hashes
    from_val->size += 1;
    to_val->size += 1;
    from_val->index = link_i;
    to_val->index = link_i;

    return link_i;
}

bool link_table_remove(Link_Table* table, ID from, ID to)
{
    isize from_hash_i = hash_index_find(table->from_hash, from);
    isize to_hash_i = hash_index_find(table->to_hash, to);
    if(from_hash_i < 0 || to_hash_i < 0)
        return false;

    Link_Table_Hash_Val* from_val = (Link_Table_Hash_Val*) (void*) &table->from_hash.entries[from_hash_i].value;
    Link_Table_Hash_Val* to_val = (Link_Table_Hash_Val*) (void*) &table->to_hash.entries[to_hash_i].value;
    isize link_i = _find_to_link_forward(table, from_val->index, from_val->size, to);
    if(link_i == -1)
        return false;

    ASSERT(link_i != 0);
    Link* link = &table->links[link_i];
    
    //unlik worry free because of sentinel value
    table->links[link->from_prev].from_next = link->from_next;
    table->links[link->from_next].from_prev = link->from_prev;
    
    table->links[link->to_prev].to_next = link->to_next;
    table->links[link->to_next].to_prev = link->to_prev;

    //reset sentinel. This is just one instruction.
    table->links[0].from_next = 0;
    table->links[0].from_prev = 0;
    table->links[0].to_next = 0;
    table->links[0].to_prev = 0;

    //Fill with garbage 
    #define DEBUG
    #ifdef DEBUG
        memset(link, -1, sizeof *link);
    #endif

    //Add to free list
    link->next_free = table->first_free_link;
    table->first_free_link = link_i;

    //Update hashes
    from_val->size -= 1;
    to_val->size -= 1;

    if(from_val->size <= 0)
        hash_index_remove(&table->from_hash, from_hash_i);
        
    if(to_val->size <= 0)
        hash_index_remove(&table->to_hash, to_hash_i);

    return true;
}
