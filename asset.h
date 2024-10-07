#pragma once

#include "lib/array.h"
#include "lib/string.h"
#include "lib/stable_array.h"
#include "lib/arena.h"
//#include "lib/string_map.h"
#include "lib/hash.h"
#include "lib/hash_string.h"
#include "lib/log.h"
#include "lib/allocator_tlsf.h"
#include "lib/vformat.h"
#include "lib/hash_func.h"

typedef struct _Asset_Handle* Asset_Handle;
typedef Array(Asset_Handle) Asset_Handle_Array;

typedef union Asset_Handle_Val {
    Asset_Handle handle;
    struct {
        u32 index : 32;
        u32 generation : 24;
        u32 type : 8;
    };
} Asset_Handle_Val;

typedef enum {
    ASSET_STAGE_UNLOADED = 0,
    ASSET_STAGE_LOADING = 1,
    ASSET_STAGE_LOADED = 2,
    ASSET_STAGE_FAILED = 3,
} Asset_Loading_Stage;

const char* asset_load_stage_to_cstring(Asset_Loading_Stage load_stage)
{
    switch(load_stage)
    {
        case ASSET_STAGE_UNLOADED: return "unloaded";
        case ASSET_STAGE_LOADING: return "loading";
        case ASSET_STAGE_LOADED: return "loaded";
        case ASSET_STAGE_FAILED: return "failed";
        default: return "<error>";
    }
}

enum {
    ASSET_FLAG_NO_UNLOAD = 1,
    ASSET_FLAG_NO_AUTO_UNLOAD = 2,
    ASSET_FLAG_NO_RELOAD = 3,
};

typedef struct Asset {
    union {
        Asset_Handle handle;
        struct {
            u32 index : 32;
            u32 generation : 24;
            u32 type : 8;
        };
    };

    //i32 asset_referenced_count;
    i32 user_referenced_count;
    u32 flags;
    Asset_Loading_Stage stage;
    u32 _;

    struct Asset* next_free;

    //id given by user for convenience. 
    //{type, id} is unique or empty
    Hash_String id;

    //path of the pack within which this asset resides. 
    //name of the specific asset within the pack.
    //{type, path, name} has to be unique tuple or path == name == ""
    Hash_String path; 
    Hash_String name; 

    Platform_File_Info file_info;

    i64 create_time;
    i64 load_start_time;
    i64 load_end_time;
    i64 access_time;
} Asset;

typedef Array(Asset*) Asset_Ptr_Array;

typedef void (*Asset_Init)(void* asset);
typedef void (*Asset_Deinit)(void* asset);
typedef void (*Asset_Copy)(void* to, const void* from);
typedef void (*Asset_Get_Refs)(void* asset, Asset_Handle_Array* handles);

typedef struct Asset_Type_Description {
    isize size;
    isize reserve_bytes_or_zero;

    String         name;
    String         abbreviation;
    Asset_Init     init;
    Asset_Deinit   deinit;
    Asset_Copy     copy;
    Asset_Get_Refs get_refs;
} Asset_Type_Description;

typedef struct Asset_Type {
    u32 combined_size; //sizeof(Asset_Type) + sizeof(T) 
    u32 type_index;    //If this asset type slot is unused is zero

    String         name;
    String         abbreviation;
    Asset_Init     init;
    Asset_Deinit   deinit;
    Asset_Copy     copy;
    Asset_Get_Refs get_refs;
    
    Asset* first_free;
    Asset* assets;
    u32 asset_count;
    u32 asset_capacity;
    
    Hash path_name_hash; 
    Hash path_hash; //multihash
    Hash id_hash;

    //@TODO
    //bool has_path_name_hash_collision;
    //bool has_path_hash_collision;
    //bool has_id_hash_collision;

    Arena arena;
} Asset_Type;

enum {ASSET_MAX_TYPES = 255};
typedef struct Asset_System {
    Allocator* strings_allocator;
    Allocator* default_allocator;
    
    Asset_Type types[ASSET_MAX_TYPES];

    bool is_init;
    bool _[7];
} Asset_System;

Asset_System* asset_system_get()
{
    static Asset_System asset_system = {0};
    return &asset_system;
}

Asset_System* asset_system_init(Allocator* allocator, Allocator* strings_allocator)
{
    Asset_System* sys = asset_system_get();
    if(sys->is_init == false)
    {   
        LOG_DEBUG("ASSET", "Initialize asset system");
        memset(sys, 0, sizeof *sys);

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

Allocator* asset_allocator()
{
    return asset_system_get()->default_allocator;
}

String asset_string_allocate(String str)
{
    return string_allocate(asset_string_allocator(), str);
}

void asset_string_deallocate(String* str)
{
    string_deallocate(asset_string_allocator(), str);
}

Asset_Handle asset_downcast(u32 type, void* handle)
{
    Asset_Handle_Val val = {(Asset_Handle) handle};
    if(handle)
        ASSERT(val.type == type);
    return handle;
}


EXTERNAL Asset_Type* asset_type_add(u32 index, Asset_Type_Description desc)
{
    Asset_System* sys = asset_system_get();
    if(0 < index && index < ARRAY_LEN(sys->types))
    {
        Asset_Type* out = &sys->types[index];
        if(out->type_index == 0)
        {
            ASSERT(out->asset_capacity == 0);
            memset(out, 0, sizeof *out);
            hash_init(&out->path_name_hash, asset_allocator());
            hash_init(&out->path_hash, asset_allocator());
            hash_init(&out->id_hash, asset_allocator());

            out->abbreviation = asset_string_allocate(desc.abbreviation);
            out->name = asset_string_allocate(desc.name);

            ASSERT(desc.size >= sizeof(Asset_Type));
            out->combined_size = (u32) desc.size;
            out->init = desc.init;
            out->deinit = desc.deinit;
            out->copy = desc.copy;
            out->get_refs = desc.get_refs;
            out->type_index = index;
        
            isize reserve = desc.reserve_bytes_or_zero ? desc.reserve_bytes_or_zero : 256*MB;
            Platform_Error error = arena_init(&out->arena, "type arena", reserve, 256*KB);
            
            SCRATCH_ARENA(arena)
                TEST(error == 0, "reserve of %s failed with:%s", format_bytes(reserve).data, translate_error(arena.alloc, error).data);
            
            LOG_INFO("ASSET", "Added asset type %.*s [%.*s] to index %u", 
                STRING_PRINT(desc.name), STRING_PRINT(desc.abbreviation), index);

            return out;
        }
        else
        {
            LOG_ERROR("ASSET", "Could not add asset type %.*s [%.*s] to index %u. Index used by type %.*s [%.*s]", 
                STRING_PRINT(desc.name), STRING_PRINT(desc.abbreviation), index, STRING_PRINT(out->name), STRING_PRINT(out->abbreviation));
        }
    }
    
    LOG_ERROR("ASSET", "Could not add asset type %.*s [%.*s]. Index %u out of range (0, %u]", 
        STRING_PRINT(desc.name), STRING_PRINT(desc.abbreviation), index, (u32) ARRAY_LEN(sys->types));
    return NULL;
}

EXTERNAL Asset_Type* asset_type_get(u32 index)
{
    Asset_System* sys = asset_system_get();
    if(index < ARRAY_LEN(sys->types))
    {
        Asset_Type* type = &sys->types[index];
        if(type->type_index)
            return type;
    }

    return NULL;
}

EXTERNAL Asset* asset_get(Asset_Handle handle)
{
    Asset_System* sys = asset_system_get();
    Asset_Handle_Val handle_val = {handle};
    if(handle_val.type < ARRAY_LEN(sys->types))
    {
        Asset_Type* type = &sys->types[handle_val.type];
        if(handle_val.index < type->asset_capacity)
        {
            ASSERT(type->type_index);
            Asset* asset = (Asset*) ((u8*) type->assets + handle_val.index*type->combined_size);
            if(asset->handle == handle)
                return asset;
        }
    }
    return NULL;
}

EXTERNAL Asset* asset_get_assert(Asset_Handle handle)
{
    Asset_System* sys = asset_system_get();
    Asset_Handle_Val handle_val = {handle};
    ASSERT(handle_val.type < ARRAY_LEN(sys->types));

    Asset_Type* type = &sys->types[handle_val.type];
    ASSERT(handle_val.index < type->asset_capacity);
    ASSERT(type->type_index);

    Asset* asset = (Asset*) ((u8*) type->assets + handle_val.index*type->combined_size);
    ASSERT(asset->handle == handle);
    return asset;
}

ATTRIBUTE_INLINE_NEVER
EXTERNAL String_Buffer_64 format_asset_handle(Asset_Handle handle)
{
    String_Buffer_64 out = {0};
    if(handle == NULL)
        snprintf(out.data, sizeof out.data, "[NULL]");
    else
    {
        Asset_Handle_Val handle_val = {handle};
        Asset* asset = asset_get(handle);
        Asset_Type* type = asset_type_get(handle_val.type);

        const char* found_str = asset ? "" : "!!";  
        if(type)
            snprintf(out.data, sizeof out.data, "[%s-%u-%u%s]", type->abbreviation.data, handle_val.index, handle_val.generation, found_str);
        else
            snprintf(out.data, sizeof out.data, "[0%u-%u-%u%s]", handle_val.type, handle_val.index, handle_val.generation, found_str);
    }
    return out;
}

ATTRIBUTE_INLINE_NEVER
EXTERNAL String_Builder format_asset_handle_and_ids(Allocator* alloc, Asset_Handle handle)
{
    Asset* asset = asset_get(handle);
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

EXTERNAL Asset_Handle asset_find_id(u32 type_i, Hash_String id)
{
    Asset_Type* type = asset_type_get(type_i);
    if(type == NULL)
        LOG_ERROR("ASSET", "%s: Cannot find id '%.*s'. Invalid type %u given.", __func__, STRING_PRINT(id), type_i);
    else
    {
        for(Hash_Found found = hash_find(type->id_hash, id.hash); found.index != -1; found = hash_find_next(type->id_hash, found))
        {
            Asset* asset = asset_get_assert((Asset_Handle) found.value);
            ASSERT(asset->id.hash == id.hash);
            if(string_is_equal(asset->id.string, id.string))
                return asset->handle;
        }
    }

    return NULL;
}

EXTERNAL Asset_Handle asset_find(u32 type_i, Hash_String path, Hash_String name)
{
    Asset_Type* type = asset_type_get(type_i);
    if(type == NULL)
        LOG_ERROR("ASSET", "%s: Cannot find path name {%.*s|%.*s}. Invalid type %u given.", __func__, STRING_PRINT(path), STRING_PRINT(name), type_i);
    else
    {
        u64 hash = hash64_mix(path.hash, name.hash);
        for(Hash_Found found = hash_find(type->path_name_hash, hash); found.index != -1; found = hash_find_next(type->path_name_hash, found))
        {
            Asset* asset = asset_get_assert((Asset_Handle) found.value);
            ASSERT(hash64_mix(asset->path.hash, asset->name.hash) == hash);
            if(string_is_equal(asset->path.string, path.string) && string_is_equal(asset->name.string, name.string))
                return asset->handle;
        }
    }

    return NULL;
}

EXTERNAL Asset_Handle_Array asset_find_all_with_path(Allocator* alloc, u32 type_i, Hash_String path)
{
    Asset_Handle_Array out = {alloc};
    Asset_Type* type = asset_type_get(type_i);
    if(type == NULL)
        LOG_ERROR("ASSET", "%s: Cannot find path '%.*s'. Invalid type %u given.", __func__, STRING_PRINT(path), type_i);
    else
    {
        for(Hash_Found found = hash_find(type->path_hash, path.hash); found.index != -1; found = hash_find_next(type->path_hash, found))
        {
            Asset* asset = asset_get_assert((Asset_Handle) found.value);
            ASSERT(asset->path.hash == path.hash);
            if(string_is_equal(asset->path.string, path.string))
                array_push(&out, asset->handle);
        }
    }

    return out;
}

enum {
    ASSET_GET_ALL = 0,
    ASSET_GET_ALL_UNREFERENCED = 1,
    ASSET_GET_ALL_REFERENCED = 2,
};

EXTERNAL Asset_Handle_Array asset_type_get_all(Allocator* alloc, u32 type_i, u32 get_all_flags)
{
    Asset_Handle_Array out = {alloc};
    Asset_Type* type = asset_type_get(type_i);
    if(type == NULL)
        LOG_ERROR("ASSET", "%s: Invalid type %u given.", __func__, type_i);
    else
    {
        for(isize i = 0; i < type->asset_capacity; i++)
        {
            Asset* asset = (Asset*) ((u8*) type->assets + i*type->combined_size);
            if(asset->next_free == NULL)
            {
                bool push = true;
                if(get_all_flags != ASSET_GET_ALL)
                {
                    bool referenced = asset->user_referenced_count != 0;
                    push = false;
                    if(get_all_flags == ASSET_GET_ALL_UNREFERENCED && referenced == false)
                        push = true;
                    if(get_all_flags == ASSET_GET_ALL_REFERENCED && referenced == true)
                        push = true;
                }

                if(push)
                    array_push(&out, asset->handle);
            }
        }
    }
    return out;
}

enum {
    ASSET_INVARIANT_BASE = 1,
    ASSET_INVARIANT_CHECK_FREE = 2,
    ASSET_INVARIANT_CHECK_USED = 4,
    ASSET_INVARIANT_FIND_FREE = 8,
    ASSET_INVARIANT_FIND_USED = 16,
    ASSET_INVARIANT_ALL = 31
};

EXTERNAL void asset_type_test_invariant(u32 type_i, u32 invariants)
{
    Asset_Type* type = asset_type_get(type_i);
    if(type != NULL)
    {
        SCRATCH_ARENA(arena)
        {
            Asset_Handle_Array used = asset_type_get_all(arena.alloc, type_i, 0);
            TEST(used.len == type->asset_count);
            TEST(type->asset_count >= type->asset_capacity);
            TEST(type->combined_size >= sizeof(Asset));
            TEST((u8*) type->assets == type->arena.data);

            //free list
            isize free_count = 0;
            for(Asset* asset = type->first_free; asset != NULL; asset = asset->next_free)
            {
                if(invariants & ASSET_INVARIANT_CHECK_FREE)
                {
                    //@TODO: rename memcheck to memnfind/memfind
                    TEST(memcheck(&asset->id, 0, sizeof asset->id) == NULL);
                    TEST(memcheck(&asset->name, 0, sizeof asset->name) == NULL);
                    TEST(memcheck(&asset->path, 0, sizeof asset->path) == NULL);
                    TEST(memcheck(&asset->file_info, 0, sizeof asset->file_info) == NULL);
                    TEST(asset->create_time == 0);
                    TEST(asset->load_start_time == 0);
                    TEST(asset->load_end_time == 0);
                    TEST(asset->user_referenced_count == 0);
                    TEST(asset->flags == 0);
                    TEST(asset->type == type_i);
                }

                free_count += 1;
                TEST(free_count < type->asset_capacity);
            }
            
            TEST(used.len + free_count == type->asset_capacity);
            
            //used
            for(isize i = 0; i < used.len; i++)
            {
                SCRATCH_ARENA(iter_arena)
                {
                    Asset_Handle handle = used.data[i];
                    Asset* asset = asset_get(handle);
                    TEST(asset->type == type_i);
                    TEST(asset->next_free == NULL);
                    TEST(asset->user_referenced_count >= 0);

                    const char* handle_formatted = format_asset_handle_and_ids(iter_arena.alloc, handle).data; 
                    (void) handle_formatted;

                    if(invariants & ASSET_INVARIANT_FIND_USED)
                    {
                        if(asset->name.len || asset->path.len)
                        {
                            Asset_Handle found = asset_find(asset->type, asset->path, asset->name);
                            TEST(found == handle);

                            Asset_Handle_Array all_with_path = asset_find_all_with_path(iter_arena.alloc, asset->type, asset->path);
                            Asset_Handle found_by_path = NULL;
                            for(isize j = 0; j < all_with_path.len; j++)
                                if(all_with_path.data[j] == handle)
                                    found_by_path = handle;
                            TEST(found_by_path == handle);
                        }
                    
                        if(asset->id.len)
                        {
                            Asset_Handle found = asset_find_id(asset->type, asset->id);
                            TEST(found == handle);
                        }
                    }
                }
            }
        }
    }
}

INTERNAL void asset_type_check_inavriants(u32 type_i)
{
    u32 invariants = ASSET_INVARIANT_BASE;
    #ifdef DO_ASSERTS_SLOW
    invariants |= ASSET_INVARIANT_ALL;
    #endif

    #ifdef DO_ASSERTS
        asset_type_test_invariant(type_i, invariants);
    #endif
}

EXTERNAL bool asset_set_path_name(Asset_Handle handle, Hash_String path, Hash_String name)
{
    Asset* to_asset = asset_get(handle);
    Asset_Handle_Val handle_val = {handle};

    bool state = true;
    SCRATCH_ARENA(arena)
    {
        const char* handle_formatted = format_asset_handle_and_ids(arena.alloc, handle).data;
        LOG_DEBUG("ASSET", "%s: Changing path name of asset %s to {%.*s|%.*s}", 
            __func__, handle_formatted, STRING_PRINT(path), STRING_PRINT(name));

        if(to_asset == NULL)
        {
            state = false;
            LOG_ERROR("ASSET", "%s: Cannot set name path of asset %s to {%.*s|%.*s}. Handle is invalid.", 
                __func__, handle_formatted, STRING_PRINT(path), STRING_PRINT(name));
        }
        else
        {
            Asset_Type* type = asset_type_get(handle_val.type);
            Asset_Handle found_handle = asset_find(handle_val.type, path, name);
            if(found_handle == handle)
            {
                LOG_WARN("ASSET", "%s: Setting name path of asset %s to {%.*s|%.*s}. Path name given is the same as already set.", 
                    __func__, handle_formatted, STRING_PRINT(path), STRING_PRINT(name));
            }
            else if(found_handle != handle && found_handle != NULL)
            {
                state = false;
                LOG_ERROR("ASSET", "%s: Cannot set name path of asset %s to {%.*s|%.*s}. Path name already used by asset %s", 
                    __func__, handle_formatted, STRING_PRINT(path), STRING_PRINT(name), format_asset_handle_and_ids(arena.alloc, found_handle).data);
            }
            else if(found_handle == NULL)
            {
                u64 path_name_hashed = hash64_mix(path.hash, name.hash);
                
                //Remove old
                if(to_asset->path.len != 0 || to_asset->name.len != 0)
                {
                    for(Hash_Found found = hash_find(type->path_name_hash, path_name_hashed); ; found = hash_find_next(type->path_name_hash, found))
                    {
                        ASSERT(found.index != -1);
                        if(found.value == (u64) handle)
                        {
                            hash_remove_found(&type->path_name_hash, found.index);
                            break;
                        }
                    }
                
                    for(Hash_Found found = hash_find(type->path_hash, path.hash); ; found = hash_find_next(type->path_hash, found))
                    {
                        ASSERT(found.index != -1);
                        if(found.value == (u64) handle)
                        {
                            hash_remove_found(&type->path_hash, found.index);
                            break;
                        }
                    }

                    hash_string_deallocate(asset_string_allocator(), &to_asset->path);
                    hash_string_deallocate(asset_string_allocator(), &to_asset->name);
                    
                    ASSERT_SLOW(asset_find(handle_val.type, path, name) == NULL);
                    #ifdef ASSERT_SLOW
                        Asset_Handle_Array all_with_path = asset_find_all_with_path(arena.alloc, handle_val.type, path);
                        for(isize i = 0; i < all_with_path.len; i++)
                            ASSERT_SLOW(all_with_path.data[i] != handle);
                    #endif
                }

                //Add new
                if(path.len != 0 || name.len != 0)
                {
                    if(to_asset->path.len != 0)
                        LOG_WARN("ASSET", "%s: Setting name path of asset %s to {%.*s|%.*s}. Changing previously set path name!", 
                            __func__, handle_formatted, STRING_PRINT(path), STRING_PRINT(name));

                    hash_insert(&type->path_name_hash, path_name_hashed, (u64) handle);
                    hash_insert(&type->path_hash, path.hash, (u64) handle);
                    
                    to_asset->path = hash_string_allocate(asset_string_allocator(), path);
                    to_asset->name = hash_string_allocate(asset_string_allocator(), name);
                    
                    ASSERT_SLOW(asset_find(handle_val.type, path, name) == handle);
                }
            }
        }
    }

    asset_type_check_inavriants(handle_val.type);
    return state;
}

EXTERNAL bool asset_set_id(Asset_Handle handle, Hash_String id)
{
    Asset* to_asset = asset_get(handle);
    Asset_Handle_Val handle_val = {handle};

    bool state = true;
    SCRATCH_ARENA(arena)
    {
        const char* handle_formatted = format_asset_handle_and_ids(arena.alloc, handle).data;
        LOG_DEBUG("ASSET", "%s: Changing id of asset %s to [%.*s]", 
            __func__, handle_formatted, STRING_PRINT(id));

        if(to_asset == NULL)
        {
            state = false;
            LOG_ERROR("ASSET", "%s: Cannot set name path of asset %s to [%.*s]. Handle is invalid.", 
                __func__, handle_formatted, STRING_PRINT(id));
        }
        else
        {
            Asset_Type* type = asset_type_get(handle_val.type);
            Asset_Handle found_handle = asset_find_id(handle_val.type, id);
            if(found_handle == handle)
            {
                LOG_WARN("ASSET", "%s: Setting name path of asset %s to [%.*s]. id given is the same as already set.", 
                    __func__, handle_formatted, STRING_PRINT(id));
            }
            else if(found_handle != handle && found_handle != NULL)
            {
                state = false;
                LOG_ERROR("ASSET", "%s: Cannot set name path of asset %s to [%.*s]. id already used by asset %s", 
                    __func__, handle_formatted, STRING_PRINT(id), format_asset_handle_and_ids(arena.alloc, found_handle).data);
            }
            else if(found_handle == NULL)
            {
                //Remove old
                if(to_asset->id.len != 0)
                {
                    for(Hash_Found found = hash_find(type->id_hash, id.hash); ; found = hash_find_next(type->id_hash, found))
                    {
                        ASSERT(found.index != -1);
                        if(found.value == (u64) handle)
                        {
                            hash_remove_found(&type->id_hash, found.index);
                            break;
                        }
                    }
                
                    hash_string_deallocate(asset_string_allocator(), &to_asset->id);
                    ASSERT_SLOW(asset_find_id(handle_val.type, id) == NULL);
                }

                //Add new
                if(id.len != 0)
                {
                    if(to_asset->path.len != 0)
                        LOG_WARN("ASSET", "%s: Setting name path of asset %s to [%.*s]. Changing previously set id!", 
                            __func__, handle_formatted, STRING_PRINT(id));

                    hash_insert(&type->id_hash, id.hash, (u64) handle);
                    
                    to_asset->path = hash_string_allocate(asset_string_allocator(), id);
                    ASSERT_SLOW(asset_find_id(handle_val.type, id) == handle);
                }
            }
        }
    }

    asset_type_check_inavriants(handle_val.type);
    return state;
}

EXTERNAL Asset* asset_create_bare(u32 type_i)
{
    Asset_System* sys = asset_system_get();
    ASSERT(sys->is_init);
    
    Asset* out = NULL;
    Asset_Type* type = asset_type_get(type_i);
    if(type == NULL)
    {   
        LOG_ERROR("ASSET", "Attempted to create an asset of unregistered type %u", type_i);
        ASSERT(false);
    }
    else
    {
        asset_type_check_inavriants(type_i);

        ASSERT(type->asset_count <= type->asset_capacity);
        ASSERT((type->assets == NULL) == (type->asset_capacity == 0));
        if(type->first_free == NULL)
        {
            enum {AT_ONCE = 16};
            Asset* added = ARENA_PUSH(&type->arena, AT_ONCE, Asset);
            type->asset_capacity += AT_ONCE;
            type->assets = (Asset*) (void*) type->arena.data;
            for(isize i = AT_ONCE; i-- > 0;)
            {
                added[i].next_free = type->first_free;
                type->first_free = &added[i];
            }
        }

        memset(out + 1, 0, type->combined_size - sizeof(Asset));

        type->first_free = type->first_free->next_free;
        type->asset_count += 1;
    
        out->next_free = NULL;
        out->stage = ASSET_STAGE_UNLOADED;
        out->create_time = platform_epoch_time();
        out->generation += 1;
        out->type = type_i;
        out->index = (u32) (out - type->assets);
        out->user_referenced_count = 1;

        if(type->init) type->init(out);

        ASSERT(type->asset_count >= 0);
        ASSERT(type->asset_count <= type->asset_capacity);
    
        LOG_DEBUG("ASSET", "Created asset %s", format_asset_handle(out->handle).data);
        asset_type_check_inavriants(type_i);
    }
    return out;
}

EXTERNAL Asset_Handle asset_create_or_get(u32 type_i, Hash_String path, Hash_String name)
{
    Asset_Handle handle = asset_find(type_i, path, name);
    if(handle == NULL)
    {
        Asset* asset = asset_create_bare(type_i);
        bool assigned = asset_set_path_name(asset->handle, path, name);
        ASSERT(assigned);
        handle = asset->handle;
    }
    
    return handle;
}

EXTERNAL Asset_Handle asset_create(u32 type_i, Hash_String path, Hash_String name)
{
    Asset_Handle handle = asset_find(type_i, path, name);
    if(handle)
        handle = NULL;
    else
    {
        Asset* asset = asset_create_bare(type_i);
        bool assigned = asset_set_path_name(asset->handle, path, name);
        ASSERT(assigned);
        handle = asset->handle;
    }
    
    return handle;
}

EXTERNAL void asset_unload(Asset_Handle handle)
{
    SCRATCH_ARENA(arena)
    {
        const char* handle_str = format_asset_handle_and_ids(arena.alloc, handle).data;
        Asset* asset = asset_get(handle);
        if(asset == NULL)
            LOG_ERROR("ASSET", "%s: Unloaded asset %s not found", 
                __func__, handle_str);
        else
        {
            
            if(asset->stage == ASSET_STAGE_LOADED)
            {
                LOG_INFO("ASSET", "%s: Unloading an asset %s", 
                    __func__, handle_str);

                Asset_Type* type = asset_type_get(asset->type);
                if(type->deinit)
                    type->deinit(asset);
                memset(asset + 1, 0, type->combined_size - sizeof(Asset));
                asset_type_check_inavriants(asset->type);
            }
            else
            {
                LOG_INFO("ASSET", "%s: Unloading an asset %s which is %s. Ignoring", 
                    __func__, handle_str, asset_load_stage_to_cstring(asset->stage));
            }
        }
    }
}

EXTERNAL void asset_set_stage(Asset_Handle handle, Asset_Loading_Stage stage)
{   
    SCRATCH_ARENA(arena)
    {
        const char* handle_str = format_asset_handle_and_ids(arena.alloc, handle).data;
        Asset* asset = asset_get(handle);
        if(asset == NULL)
            LOG_ERROR("ASSET", "%s: Asset %s not found", 
                __func__, handle_str);
        else
        {
            asset->stage = stage;
            if(stage == ASSET_STAGE_LOADING)
                asset->load_start_time = platform_epoch_time();
                
            if(stage == ASSET_STAGE_LOADED || stage == ASSET_STAGE_FAILED)
                asset->load_end_time = platform_epoch_time();
        }
    }
}

EXTERNAL void asset_load(Asset_Handle handle, bool retry_failed)
{
    SCRATCH_ARENA(arena)
    {
        (void) retry_failed;
        const char* handle_str = format_asset_handle_and_ids(arena.alloc, handle).data;
        TODO("Make this after it becomes more apparent what we want here. %s ", handle_str);
    }
}

EXTERNAL void asset_delete(Asset_Handle handle, bool force)
{
    Hash_String nil_string = {0};
    Asset* asset = asset_get(handle);
    
    SCRATCH_ARENA(arena)
    {
        const char* handle_str = format_asset_handle_and_ids(arena.alloc, handle).data;
        if(asset == NULL)
            LOG_ERROR("ASSET", "%s: Deleted asset %s not found", 
                __func__, handle_str);
        else
        {
            if(asset->user_referenced_count > 1 && force == false)
            {
                LOG_DEBUG("ASSET", "%s: Decrementing an asset %s user reference counter %u -> %u. ", 
                    __func__, handle_str, asset->user_referenced_count, asset->user_referenced_count - 1);
                
                asset->user_referenced_count -= 1;
            }
            else
            {
                LOG_DEBUG("ASSET", "%s: Deleting an asset %s", 
                    __func__, handle_str);
                if(asset->path.len || asset->name.len)
                    asset_set_path_name(handle, nil_string, nil_string);
                if(asset->id.len)
                    asset_set_id(handle, nil_string);
            
                Asset_Type* type = asset_type_get(asset->type);
                if(type->deinit)
                    type->deinit(asset);

                Asset_Handle old_handle = asset->handle;
                memset(asset, 0, type->combined_size);

                asset->handle = old_handle;
                asset->generation += 1;
                asset->next_free = type->first_free;
                type->first_free = asset;
                type->asset_count -= 1;
                ASSERT(type->asset_count >= 0);
                ASSERT(type->asset_count <= type->asset_capacity);
            
                asset_type_check_inavriants(asset->type);
            }
        }
    }
}

EXTERNAL Asset_Handle_Array asset_get_refs(Allocator* alloc, Asset_Handle handle)
{
    Asset_Handle_Array out = {alloc};
    Asset* asset = asset_get(handle);
    if(asset == NULL)
        LOG_ERROR("ASSET", "%s: Asset handle %s not found", __func__, format_asset_handle(handle).data);
    else
    {
        Asset_Type* type = asset_type_get(asset->type);
        if(type->get_refs)
            type->get_refs(asset, &out);
    }

    return out;
}

void char_set_difference(const char* a, isize a_len, const char* b, isize b_len, String_Builder* a_extra, String_Builder* b_extra)
{
    isize ai = 0;
    isize bi = 0;
    while(ai < a_len && bi < b_len)
    {
        if(a[ai] < b[bi])
        {
            builder_push(a_extra, a[ai]);
            ai += 1;
        }
        else if(a[ai] > b[bi])
        {
            builder_push(b_extra, b[bi]);
            bi += 1;
        }
        else
        {
            bi += 1;
            ai += 1;
        }
    }

    if(ai < a_len)
        builder_append(a_extra, string_make(a + ai, a_len - ai));
    if(bi < b_len)
        builder_append(b_extra, string_make(b + bi, b_len - bi));
}

void test_set_difference_single(const char* a, const char* b, const char* a_extra_exp, const char* b_extra_exp)
{
    SCRATCH_ARENA(arena)
    {
        String_Builder a_extra = builder_make(arena.alloc, 0);
        String_Builder b_extra = builder_make(arena.alloc, 0);

        char_set_difference(a, strlen(a), b, strlen(b), &a_extra, &b_extra);
        TEST(strcmp(a_extra_exp, a_extra.data) == 0);
        TEST(strcmp(b_extra_exp, b_extra.data) == 0);
    }
}

void test_set_difference()
{
    test_set_difference_single("", "", "", "");
    test_set_difference_single("1", "2", "1", "2");
    test_set_difference_single("", "123", "", "123");
    test_set_difference_single("123", "", "123", "");
    test_set_difference_single("123", "123", "", "");
    test_set_difference_single("123", "1234", "", "4");
    test_set_difference_single("1234", "123", "4", "");
    test_set_difference_single("0123456", "0111222666", "345", "112266");
    test_set_difference_single("01234456", "0111222666", "3445", "112266");
}
