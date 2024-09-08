#pragma once

#include "lib/array.h"
#include "lib/string.h"
#include "lib/stable_array.h"
#include "lib/arena.h"
#include "lib/string_map.h"
#include "lib/log.h"
#include "lib/allocator_tlsf.h"

typedef struct _NAsset_Handle* NAsset_Handle;

typedef union NAsset_Handle_Val {
    NAsset_Handle handle;
    struct {
        u32 index : 32;
        u32 generation : 24;
        u32 type : 8;
    };
} NAsset_Handle_Val;

typedef struct NAsset {
    union {
        NAsset_Handle handle;
        struct {
            u32 index : 32;
            u32 generation : 24;
            u32 type : 8;
        };
    };

    i32 references;
    bool no_unload;
    bool is_loaded;
    bool _[2];

    struct NAsset* next_free;

    Hash_String path; //has to be unique or empty
    Hash_String name; //has to be unique or empty
    
    Platform_File_Info file_info;

    i64 create_time;
    i64 load_time;
} NAsset;

typedef Array(NAsset_Handle) NAsset_Handle_Array;
typedef Array(NAsset)        NAsset_Array;

typedef void (*NAsset_Init)(void* asset);
typedef void (*NAsset_Deinit)(void* asset);
typedef void (*NAsset_Copy)(void* to, const void* from);
typedef void (*NAsset_Get_Refs)(void* asset, NAsset_Handle_Array* handles);

typedef struct NAsset_Type_Description {
    isize size;
    isize reserve_bytes_or_zero;

    String          name;
    String          abbreviation;
    NAsset_Init     init;
    NAsset_Deinit   deinit;
    NAsset_Copy     copy;
    NAsset_Get_Refs get_refs;
} NAsset_Type_Description;

typedef struct NAsset_Type {
    //sizeof(NAsset_Type) + sizeof(T) 
    u32 combined_size; 
    u32 type_index; //If this asset type slot is unused is zero

    String         name;
    String         abbreviation;
    NAsset_Init     init;
    NAsset_Deinit   deinit;
    NAsset_Copy     copy;
    NAsset_Get_Refs get_refs;
    
    NAsset* first_free;
    NAsset* assets;
    u32 asset_count;
    u32 asset_capacity;

    Arena arena;
} NAsset_Type;

enum {
    ASSET_FLAG_LOADED = 1,
    ASSET_FLAG_NO_UNLOAD = 2,
};

enum {ASSET_MAX_TYPES = 256};
typedef struct NAsset_System {
    Allocator* strings_allocator;
    Allocator* default_allocator;

    Hash path_hash;
    Hash name_hash;
    bool has_name_hash_collision;
    bool has_path_hash_collision;
    bool is_init;
    bool _[5];

    NAsset_Type types[ASSET_MAX_TYPES];
} NAsset_System;

typedef struct NAsset_Creation NAsset_Creation;

NAsset_System* asset_system_get()
{
    static NAsset_System asset_system = {0};
    return &asset_system;
}

NAsset_System* asset_system_init(Allocator* allocator, Allocator* strings_allocator)
{
    NAsset_System* sys = asset_system_get();
    if(sys->is_init == false)
    {
        memset(sys, 0, sizeof *sys);
        hash_init(&sys->name_hash, allocator);
        hash_init(&sys->path_hash, allocator);

        sys->default_allocator = allocator;
        sys->strings_allocator = strings_allocator;
        sys->is_init = true;
    }
    return sys;
}

Allocator* asset_string_allocator()
{
    return asset_system_get()->strings_allocator;
}

String asset_string_allocate(String str)
{
    return string_allocate(asset_string_allocator(), str);
}

void asset_string_deallocate(String* str)
{
    string_deallocate(asset_string_allocator(), str);
}

EXTERNAL NAsset_Type* asset_type_add(u32 index, NAsset_Type_Description desc)
{
    NAsset_System* sys = asset_system_get();
    if(0 < index && index < ARRAY_SIZE(sys->types))
    {
        NAsset_Type* out = &sys->types[index];
        if(out->type_index == 0)
        {
            ASSERT(out->asset_capacity == 0);
            memset(out, 0, sizeof *out);

            out->abbreviation = asset_string_allocate(desc.abbreviation);
            out->name = asset_string_allocate(desc.name);

            ASSERT(desc.size >= sizeof(NAsset_Type));
            out->combined_size = (u32) desc.size;
            out->init = desc.init;
            out->deinit = desc.deinit;
            out->copy = desc.copy;
            out->get_refs = desc.get_refs;
            out->type_index = index;
        
            isize reserve = desc.reserve_bytes_or_zero ? desc.reserve_bytes_or_zero : 256*MB;
            Platform_Error error = arena_init(&out->arena, "type arena", reserve, 256*KB);
            TEST(error == 0, "reserve of %s failed with:%s", format_bytes(reserve).data, platform_translate_error(error));

            return out;
        }
    }
    return NULL;
}

EXTERNAL NAsset_Type* asset_type_get(u32 index)
{
    NAsset_System* sys = asset_system_get();
    if(index < ARRAY_SIZE(sys->types))
    {
        NAsset_Type* type = &sys->types[index];
        if(type->type_index)
            return type;
    }

    return NULL;
}

NAsset* asset_get(NAsset_Handle handle)
{
    NAsset_System* sys = asset_system_get();
    NAsset_Handle_Val handle_val = {handle};
    if(handle_val.type < ARRAY_SIZE(sys->types))
    {
        NAsset_Type* type = &sys->types[handle_val.type];
        if(handle_val.index < type->asset_capacity)
        {
            ASSERT(type->type_index);
            NAsset* asset = (NAsset*) ((u8*) type->assets + handle_val.index*type->combined_size);
            if(asset->handle == handle)
                return asset;
        }
    }
    return NULL;
}

NAsset* asset_get_assert(NAsset_Handle handle)
{
    NAsset_System* sys = asset_system_get();
    NAsset_Handle_Val handle_val = {handle};
    ASSERT(handle_val.type < ARRAY_SIZE(sys->types));

    NAsset_Type* type = &sys->types[handle_val.type];
    ASSERT(handle_val.index < type->asset_capacity);
    ASSERT(type->type_index);

    NAsset* asset = (NAsset*) ((u8*) type->assets + handle_val.index*type->combined_size);
    ASSERT(asset->handle == handle);
    return asset;
}

EXTERNAL String_Buffer_64 format_asset_handle(NAsset_Handle handle)
{
    NAsset_Handle_Val handle_val = {handle};
    NAsset* asset = asset_get(handle);
    NAsset_Type* type = asset_type_get(handle_val.type);

    const char* found_str = asset ? "" : "!!";  
    String_Buffer_64 out = {0};
    if(type)
        snprintf(out.data, sizeof out.data, "[%s-%u-%u%s]", type->abbreviation.data, handle_val.index, handle_val.generation, found_str);
    else
        snprintf(out.data, sizeof out.data, "[0%u-%u-%u%s]", handle_val.type, handle_val.index, handle_val.generation, found_str);

    return out;
}

#include "lib/vformat.h"
EXTERNAL String_Builder format_asset_handle_and_ids(Allocator* alloc, NAsset_Handle handle)
{
    NAsset* asset = asset_get(handle);
    String_Buffer_64 handle_fmt = format_asset_handle(handle);
    String_Builder builder = builder_make(alloc, 64);
    builder_append(&builder, string_of(handle_fmt.data));
    
    if(asset != NULL && (asset->path.len != 0 || asset->name.len != 0))
    {
        if(asset->path.len != 0 && asset->name.len != 0)
            format_append_into(&builder, "{%.*s|%.*s}", STRING_PRINT(asset->name), STRING_PRINT(asset->path));
        else if(asset->path.len != 0)
            format_append_into(&builder, "{|%.*s}", STRING_PRINT(asset->path));
        else if(asset->name.len != 0)
            format_append_into(&builder, "{%.*s|}", STRING_PRINT(asset->name));
    }

    return builder;
}

INTERNAL NAsset* _asset_get_by_name_or_path(Hash_String id, bool is_path)
{
    NAsset_System* sys = asset_system_get();

    Hash* hash = is_path ? &sys->path_hash : &sys->name_hash;
    bool* has_collision = is_path ? &sys->has_path_hash_collision : &sys->has_name_hash_collision;

    for(Hash_Found found = hash_find(*hash, id.hash); found.index != -1; found = hash_find_next(*hash, found))
    {
        NAsset* asset = asset_get_assert((NAsset_Handle) found.value);
        if(*has_collision == false)
            return asset;

        Hash_String asset_id = is_path ? asset->path : asset->name;
        ASSERT(asset_id.hash == id.hash);
        if(string_is_equal(asset_id.string, id.string))
            return asset;
    }

    return NULL;
}

INTERNAL bool _asset_set_name_or_path(NAsset_Handle handle, Hash_String id, bool is_path)
{
    NAsset_System* sys = asset_system_get();
    NAsset* to_asset = asset_get(handle);
    
    bool state = true;
    SCRATCH_ARENA(arena)
    {
        LOG_DEBUG("ASSET", "Changing %s of an asset %s to '%.*s'", 
            is_path ? "path" : "name", format_asset_handle_and_ids(arena.alloc, handle).data, STRING_PRINT(id));

        if(to_asset == NULL)
        {
            state = false;
            LOG_ERROR("ASSET", "%s of invalid handle %s", 
                is_path ? "asset_set_path" : "asset_set_name", format_asset_handle(handle).data);
        }
        else
        {
            Hash_String* to_asset_id = is_path ? &to_asset->path : &to_asset->name;
            Hash* hash = is_path ? &sys->path_hash : &sys->name_hash;
            bool* has_collision = is_path ? &sys->has_path_hash_collision : &sys->has_name_hash_collision;

            //Add new
            if(id.len != 0)
            {
                if(is_path && to_asset->path.len != 0)
                    LOG_WARN("ASSET", "Changing path of an asset %s to '%.*s'", format_asset_handle_and_ids(arena.alloc, handle).data, STRING_PRINT(id));

                Hash_Found found = {0};
                for(found = hash_find_or_insert(hash, id.hash, (u64) handle); 
                    found.index != -1; 
                    found = hash_find_or_insert_next(hash, found, (u64) handle))
                {
                    if(found.index == -1)
                    {
                        *has_collision = true;
                        hash_insert_next(&sys->name_hash, found, (u64) handle);
                        break;
                    }

                    if(found.inserted)
                        break;

                    NAsset* asset = asset_get_assert((NAsset_Handle) found.value);
                    Hash_String asset_id = is_path ? asset->path : asset->name;

                    ASSERT(asset_id.hash == id.hash);
                    if(string_is_equal(asset_id.string, id.string))
                    {
                        state = false;
                        break;
                    }
                    else
                    {
                        LOG_WARN("ASSET", "Hash collision detected while setting %s of %s to '%.*s' with %s",
                            is_path ? "path" : "name", 
                            format_asset_handle_and_ids(arena.alloc, handle).data, 
                            STRING_PRINT(asset_id), 
                            format_asset_handle_and_ids(arena.alloc, (NAsset_Handle) found.value).data);
                    }
                }
            }
            
            //Remove old
            if(to_asset_id->len != 0 && state)
            {
                for(Hash_Found found = hash_find(*hash, to_asset_id->hash); ; found = hash_find_next(*hash, found))
                {
                    if(found.index == -1)
                    {
                        LOG_FATAL("ASSET", "%s of an asset %s was not found in hash! This should never happen", 
                            is_path ? "path" : "name", format_asset_handle_and_ids(arena.alloc, handle).data);
                    
                        state = false;
                        ASSERT(false);
                        break;
                    }

                    if((NAsset_Handle) found.value == handle)
                    {
                        hash_remove_found(hash, found.index);
                        break;
                    }
                }
                
                *to_asset_id = BINIT(Hash_String){0};
                NAsset* found_back = _asset_get_by_name_or_path(id, is_path);
                ASSERT_SLOW(found_back == NULL);
            }

            if(state)
            {
                if(id.len > 0)
                {
                    *to_asset_id = hash_string_make(builder_from_string(sys->strings_allocator, id.string).string);
                    NAsset* found_back = _asset_get_by_name_or_path(id, is_path);
                    ASSERT_SLOW(found_back == to_asset);
                }
            }
        }
    }
    return state;
}

NAsset* asset_create(u32 type_i)
{
    NAsset_System* sys = asset_system_get();
    ASSERT(sys->is_init);
    
    NAsset_Type* type = asset_type_get(type_i);
    if(type == NULL)
    {   
        LOG_ERROR("ASSET", "Attempted to create an asset of unregistered type %u", type_i);
        ASSERT(false);
        return NULL;
    }
    
    ASSERT(type->asset_count <= type->asset_capacity);
    ASSERT((type->assets == NULL) == (type->asset_capacity == 0));
    if(type->first_free == NULL)
    {
        enum {AT_ONCE = 16};
        NAsset* added = ARENA_PUSH(&type->arena, AT_ONCE, NAsset);
        type->asset_capacity += AT_ONCE;
        type->assets = (NAsset*) (void*) type->arena.data;
        for(isize i = AT_ONCE; i-- > 0;)
        {
            added[i].next_free = type->first_free;
            type->first_free = &added[i];
        }
    }

    NAsset* out = type->first_free;
    memset(out + 1, 0, type->combined_size - sizeof(NAsset));

    type->first_free = type->first_free->next_free;
    type->asset_count += 1;
    
    out->next_free = NULL;
    out->is_loaded = false;
    out->create_time = platform_epoch_time();
    out->generation += 1;
    out->type = type_i;
    out->index = (u32) (out - type->assets);

    if(type->init) type->init(out);

    ASSERT(type->asset_count >= 0);
    ASSERT(type->asset_count <= type->asset_capacity);
    return out;
}

ATTRIBUTE_INLINE_NEVER
EXTERNAL NAsset* asset_get_by_name(Hash_String name)
{
    return _asset_get_by_name_or_path(name, false);
}

ATTRIBUTE_INLINE_NEVER
EXTERNAL NAsset* asset_get_by_path(Hash_String path)
{
    return _asset_get_by_name_or_path(path, true);
}

ATTRIBUTE_INLINE_NEVER
EXTERNAL bool asset_set_name(NAsset_Handle handle, Hash_String name)
{
    return _asset_set_name_or_path(handle, name, false);
}

ATTRIBUTE_INLINE_NEVER
EXTERNAL bool asset_set_path(NAsset_Handle handle, Hash_String path)
{
    return _asset_set_name_or_path(handle, path, true);
}

void asset_delete(NAsset_Handle handle, bool force)
{
    Hash_String nil_string = {0};
    NAsset* asset = asset_get(handle);
    
    SCRATCH_ARENA(arena)
    {
        if(asset == NULL)
            LOG_ERROR("ASSET", "Deleted asset %s not found", format_asset_handle_and_ids(arena.alloc, handle).data);
        else if(asset->references != 0 && force == false)
        {
            LOG_DEBUG("ASSET", "Attempting to deleting an asset %s protected by %i references. Ignoring.", 
                format_asset_handle_and_ids(arena.alloc, handle).data, asset->references);
        }
        else
        {
            LOG_DEBUG("ASSET", "Deleting an asset %s", format_asset_handle_and_ids(arena.alloc, handle).data);
            if(asset->path.len > 0)
                asset_set_path(handle, nil_string);
            
            if(asset->name.len > 0)
                asset_set_name(handle, nil_string);

            
            NAsset_Type* type = asset_type_get(asset->type);
            if(type->get_refs)
            {
                NAsset_Handle_Array refs = {arena.alloc};
                type->get_refs(asset, &refs);
                for(isize i = 0; i < refs.len; i++)
                {
                    NAsset_Handle refed_handle = refs.data[i];
                    NAsset* refed = asset_get(refed_handle);
                    if(refed == NULL)
                    {
                        LOG_WARN("ASSET", "Referenced asset %s not found while deleting asset %s", 
                            format_asset_handle(refed_handle).data, format_asset_handle_and_ids(arena.alloc, handle).data);
                    }
                    else
                    {
                        refed->references -= 1;
                        if(refed->references < 0)
                        {  
                            LOG_WARN("ASSET", "Referenced asset %s improperly tracking self references %i found while deleting asset %s", 
                                format_asset_handle(refed_handle).data, refed->references, format_asset_handle_and_ids(arena.alloc, handle).data);
                            refed->references = 0;
                        }
                        if(refed->references == 0 && refed->no_unload == false)
                        {
                            log_indent();
                            asset_delete(refed_handle, false);
                            log_outdent();
                        }
                    }
                }
            }

            asset_string_deallocate(&asset->path.string);
            asset_string_deallocate(&asset->name.string);
            if(type->deinit)
                type->deinit(asset);

            NAsset_Handle old_handle = asset->handle;
            memset(asset, 0, sizeof asset);

            asset->handle = old_handle;
            asset->generation += 1;
            asset->next_free = type->first_free;
            type->first_free = asset;
            type->asset_count -= 1;
            ASSERT(type->asset_count >= 0);
            ASSERT(type->asset_count <= type->asset_capacity);
        }
    }
}

NAsset_Handle_Array asset_get_refs(Allocator* alloc, NAsset_Handle handle)
{
    NAsset_Handle_Array out = {alloc};
    NAsset* asset = asset_get(handle);
    if(asset == NULL)
        LOG_ERROR("ASSET", "NAsset handle %s passed into asset_get_refs not found", format_asset_handle(handle).data);
    else
    {
        NAsset_Type* type = asset_type_get(asset->type);
        if(type->get_refs)
            type->get_refs(asset, &out);
    }

    return out;
}


