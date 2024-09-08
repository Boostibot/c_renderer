#pragma once

#include "lib/arena.h"

typedef struct _Handle_Base* Handle;

typedef union Handle_Struct{
    Handle handle;
    struct {
        u16 type;
        u16 generation;
        u32 index;
    };
} Handle_Struct;

Handle_Struct handle_unpack(Handle handle)
{
    Handle_Struct s = {handle};
    return s;
}

Handle_Struct* handle_modify(Handle* handle_ptr)
{
    return (Handle_Struct*) (void*) handle_ptr;
}


typedef struct Virtual_Freelist{
    union {
        u8* data;
        Arena arena;
    };
    u32 size;
    u32 capacity;
    u32 item_size;
    u32 first_free_i1;
} Virtual_Freelist;

void virtual_freelist_deinit(Virtual_Freelist* freelist)
{
    arena_deinit(&freelist->arena);
    memset(freelist, 0, sizeof *freelist);
}

bool virtual_freelist_init_custom(Virtual_Freelist* freelist, isize item_size, isize reserve_bytes_or_zero, isize commit_at_once_or_zero)
{
    virtual_freelist_deinit(freelist);
    arena_init(&freelist->arena, reserve_bytes_or_zero, commit_at_once_or_zero);
    freelist->item_size = item_size;
}

void* virtual_freelist_at(Virtual_Freelist* freelist, u32 at)
{
    CHECK_BOUNDS(at, freelist->capacity);
    return (freelist->data + at*freelist->item_size);
}

u32 virtual_freelist_push(Virtual_Freelist* freelist)
{
    if(freelist->first_free_i1 == 0)
    {
        arena_push_nonzero_unaligned(&freelist->arena, freelist->arena.commit_granularity);
        isize new_capacity = freelist->arena.commit / freelist->item_size;
        isize old_capacity = freelist->capacity;

        freelist->capacity = new_capacity;
        //add all items to free list in reverse
        for(isize i = new_capacity; i-- > old_capacity;)
        {
            u32* item = (u32*) (u32*) virtual_freelist_at(freelist, i);
            *item = freelist->first_free_i1;
            freelist->first_free_i1 = i + 1;
        }
    }
    
    u32 free = freelist->first_free_i1 - 1;
    u32* free_item = (u32*) virtual_freelist_at(freelist, free);
    freelist->first_free_i1 = *free_item;
    freelist->len += 1;
    return free;
}
    
u32 virtual_freelist_pop(Virtual_Freelist* freelist, u32 index)
{
    u32* freed = (u32*) virtual_freelist_at(freelist, index);
    *freed = freelist->first_free_i1;
    freelist->first_free_i1 = index + 1;
    freelist->len -= 1;
}

#include "lib/hash.h"
#include "lib/hash.h"
#include "lib/log.h"

typedef struct Link {
    Handle from;
    Handle to;
    u32 next_from;
    u32 prev_from;
    u32 next_to;
    u32 prev_to;
} Link;

typedef struct Link_Table{
    u32 link_count;
    u32 link_capacity;
    u32 item_count;

    Hash pair_hash_to_link;
    Link* links;
    u32 first_free_i1;
    Arena arena;
} Link_Table_Bucket; 

u64 hash_link(Handle from, Handle to)
{
    return hash64_mix(hash64_bijective((u64) from), hash64_bijective((u64) to));
}

u64 hash_from_link(Handle from)
{
    return hash64_mix(hash64_bijective((u64) from), 0);
}

u64 hash_to_link(Handle to)
{
    return hash64_mix(0, hash64_bijective((u64) to));
}


String_Buffer_64 format_handle(Handle handle)
{
    const char* types[] = {
        "tex",
        "geo",
        "mesh",
        "sky",
        "cnfg",
        "model",
        "matrl"
    };

    Handle_Struct unpacked = handle_unpack(handle);
    const char* type = unpacked.type < ARRAY_SIZE(type) ? types[unpacked.type] : "err";
    String_Buffer_64 out = {0};
    snprintf(out.data, sizeof(out.data), "H[%s %i/%i]", type, unpacked.index, unpacked.generation);
    return out;
}

u32 _link_table_add_freelist_link(Link_Table* table)
{
    return 0;
}

u32 _link_table_add_link(Link_Table* table, Handle from, Handle to)
{
    u64 from_h = hash64_bijective((u64) from);
    u64 to_h = hash64_bijective((u64) from);

    u64 from_hash = hash64_mix(from_h, 0);
    u64 to_hash = hash64_mix(0, to_h);
    u64 conect_hash = hash64_mix(from_h, to_h);

    hash_reserve(&table->pair_hash_to_link, table->pair_hash_to_link.len + 3);

    isize conect_found = hash_find_or_insert(&table->pair_hash_to_link, conect_hash, 0);
    Hash_Entry* connect_entry = &table->pair_hash_to_link.entries[conect_found];
    if(connect_entry->value != 0)
    {
        Link* connect_link = &table->links[connect_entry->value];
        if(connect_link->from == from && connect_link->to == to)
        {
            LOG_WARN("LINK", "Link %s -> %s already present!", format_handle(from).data, format_handle(to).data);
            return connect_entry->value;
        }
    }

    isize from_found = hash_find_or_insert(&table->pair_hash_to_link, from_hash, 0);
    isize to_found = hash_find_or_insert(&table->pair_hash_to_link, to_hash, 0);

    Hash_Entry* from_entry = &table->pair_hash_to_link.entries[from_found];
    Hash_Entry* to_entry = &table->pair_hash_to_link.entries[to_found];
    
    //If dont exist will point to nill node so its fine
    Link* connect_link = &table->links[connect_entry->value];
    Link* from_link = &table->links[from_entry->value];
    Link* to_link = &table->links[to_entry->value];

    if(connect_entry->value == 0) 
    {
        connect_entry->value = _link_table_add_freelist_link(table);
        connect_link = &table->links[connect_entry->value];

    }
    if(from_entry->value == 0) 
    {
        from_entry->value = _link_table_add_freelist_link(table);
        from_link = &table->links[from_entry->value];
        from_link->from = from;
        from_link->from = from;
        from_link->to = NULL;
        from_link->next_from = connect_entry->value;
        from_link->prev_from = connect_entry->value;
        from_link->next_to = 0;
        from_link->prev_to = 0;
    }
    else
    {
    
    }
    if(to_entry->value == 0) 
    {
        to_entry->value = _link_table_add_freelist_link(table);
        to_link = &table->links[to_entry->value];
        to_link->from = NULL;
        to_link->to = to;
        to_link->next_to = connect_entry->value;
        to_link->prev_to = connect_entry->value;
        to_link->next_from = 0;
        to_link->prev_from = 0;
    }
    

}