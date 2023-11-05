#pragma once

#include "stable_array.h"
#include "hash_index.h"
#include "guid.h"
#include "string.h"
#include "hash_string.h"
#include "log.h"
#include "vformat.h"

#define RESOURCE_CALLSTACK_SIZE 8
#define RESOURCE_EPHEMERAL_SIZE 4
#define RESOURCE_CHECK_LIFE_EVERY_MS 100 

typedef struct Resource_Callstack {
    void* stack_frames[RESOURCE_CALLSTACK_SIZE];
} Resource_Callstack;

typedef enum Resource_Lifetime {
    RESOURCE_LIFETIME_REFERENCED = 0,   //cleaned up when reference count reaches 0
    RESOURCE_LIFETIME_PERSISTANT = 2,   //cleaned up when explicitly demanded
    RESOURCE_LIFETIME_TIMED = 4,         //cleaned up when time is up
    RESOURCE_LIFETIME_SINGLE_FRAME = 8, //cleaned up automatically when this frame ends
    RESOURCE_LIFETIME_EPHEMERAL = 16,   //cleaned up on some subsequent call to this function. Should be immediately used or coppied
} Resource_Lifetime;

typedef enum Resource_Reload {
    RESOURCE_RELOAD_ON_FILE_CHANGE = 0,
    RESOURCE_RELOAD_ON_MEMORY_CHANGE = 1,
    RESOURCE_RELOAD_NEVER = 2,
} Resource_Reload;

typedef struct Resource_Info {
    Id id;

    String_Builder name;
    String_Builder path;
    Resource_Callstack callstack;
    i32 reference_count;
    u32 storage_index;
    u32 type_enum; //some enum value used for debugging

    i64 creation_etime;
    i64 death_etime;
    i64 modified_etime;
    i64 load_etime;
    i64 file_modified_etime; 

    Resource_Lifetime lifetime;
    Resource_Reload reload;

    void* data;
} Resource_Info;

typedef struct Resource_Ptr {
    Id id;
    Resource_Info* ptr;
} Resource_Ptr;

DEFINE_ARRAY_TYPE(Resource_Ptr, Resource_Ptr_Array);

typedef struct Resource_Params {
    Id id;  //if 0 generates a new one
    String path;
    String name;
    Resource_Lifetime lifetime;
    Resource_Reload reload;
    i64 death_etime;
    i64 file_modified_etime;

    bool was_loaded;
} Resource_Params;

EXPORT Resource_Params resource_params_make_simple(String name, String path);


typedef void(*Resource_Destructor)(void* item);
typedef void(*Resource_Constructor)(void* item);
typedef void(*Resource_Copy)(void* to, void* from);

typedef struct Resource_Manager {
    Stable_Array storage;

    Hash_Ptr id_hash;
    Hash_Ptr name_hash;
    Hash_Ptr path_hash;

    Resource_Ptr_Array timed;
    Resource_Ptr_Array single_frame;

    Resource_Constructor constructor;
    Resource_Destructor destructor;
    Resource_Copy copy;

    const char* type_name;
    i32 type_size;
    u32 type_enum;
} Resource_Manager;

EXPORT void resource_manager_init(Resource_Manager* manager, Allocator* alloc, isize item_size, Resource_Constructor constructor, Resource_Destructor destructor, Resource_Copy copy, const char* type_name, u32 type_enum);

EXPORT void resource_manager_deinit(Resource_Manager* manager);

EXPORT bool         resource_is_valid(Resource_Ptr resource);

EXPORT Resource_Ptr resource_get(Resource_Manager* manager, Id id);
EXPORT Resource_Ptr resource_get_by_name(Resource_Manager* manager, Hash_String name, isize* prev_found_and_finished_at);
EXPORT Resource_Ptr resource_get_by_path(Resource_Manager* manager, Hash_String path, isize* prev_found_and_finished_at);

EXPORT Resource_Ptr resource_insert(Resource_Manager* manager, Resource_Params params);

EXPORT bool         resource_remove_custom(Resource_Manager* manager, Resource_Ptr resource, void* removed_data, isize removed_data_size, bool* was_copied);
EXPORT bool         resource_remove(Resource_Manager* manager, Resource_Ptr resource);

EXPORT bool         resource_force_remove_custom(Resource_Manager* manager, Resource_Ptr resource, void* removed_data, isize removed_data_size, bool* was_copied);
EXPORT bool         resource_force_remove(Resource_Manager* manager, Resource_Ptr resource);

EXPORT Resource_Ptr resource_share(Resource_Manager* manager, Resource_Ptr resource);
EXPORT Resource_Ptr resource_make_unique(Resource_Manager* manager, Resource_Ptr resource, Resource_Params params);
EXPORT Resource_Ptr resource_duplicate(Resource_Manager* manager, Resource_Ptr resource, Resource_Params params);

EXPORT void resource_manager_frame_cleanup(Resource_Manager* manager);
EXPORT void resource_manager_time_cleanup(Resource_Manager* manager);

// ========================================= IMPL =================================================

EXPORT void resource_manager_init(Resource_Manager* manager, Allocator* alloc, isize item_size, Resource_Constructor constructor, Resource_Destructor destructor, Resource_Copy copy, const char* type_name, u32 type_enum)
{
    resource_manager_deinit(manager);

    stable_array_init(&manager->storage, alloc, item_size + sizeof(Resource_Info));
    hash_ptr_init(&manager->id_hash, alloc);
    hash_ptr_init(&manager->name_hash, alloc);
    hash_ptr_init(&manager->path_hash, alloc);

    array_init(&manager->timed, alloc);
    array_init(&manager->single_frame, alloc);
    
    manager->type_name = type_name;
    manager->type_size = (i32) item_size;
    manager->type_enum = type_enum;

    manager->constructor = constructor;
    manager->destructor = destructor;
    manager->copy = copy;
}

EXPORT void resource_manager_deinit(Resource_Manager* manager)
{
    STABLE_ARRAY_FOR_EACH_BEGIN(manager->storage, Resource_Info*, info, isize, index)
        if(manager->destructor)
            manager->destructor(info->data);
            
        array_deinit(&info->path);
        array_deinit(&info->name);
    STABLE_ARRAY_FOR_EACH_END

    stable_array_deinit(&manager->storage);
    hash_ptr_deinit(&manager->id_hash);
    hash_ptr_deinit(&manager->name_hash);
    hash_ptr_deinit(&manager->path_hash);

    array_deinit(&manager->timed);
    array_deinit(&manager->single_frame);

    memset(manager, 0, sizeof *manager);
}

EXPORT bool         resource_is_valid(Resource_Ptr resource)
{
    return resource.ptr && resource.ptr->id == resource.id;
}

EXPORT Resource_Ptr _resource_get(Resource_Manager* manager, Id id, isize* prev_found_and_finished_at)
{
    Resource_Ptr out = {0};
    isize found = hash_ptr_find(manager->id_hash, (u64) id);
    if(found == -1)
        return out;

    Hash_Ptr_Entry entry = manager->id_hash.entries[found];
    ASSERT_MSG(entry.hash == (u64) id, "hash ptr has no hash collisions on 64 bit number!");

    Resource_Info* ptr = (Resource_Info*) hash_ptr_ptr_restore((void*) entry.value);
    out.id = id;
    out.ptr = ptr;

    ASSERT_MSG(ptr->id == id, "the hash needs to be kept up to date");
    if(prev_found_and_finished_at)
        *prev_found_and_finished_at = found;
    return out;
}

EXPORT Resource_Ptr resource_get_by_name(Resource_Manager* manager, Hash_String name, isize* prev_found_and_finished_at)
{
    Resource_Ptr out = {0};
    isize finished_at = 0;
    isize found = 0;
    if(prev_found_and_finished_at && *prev_found_and_finished_at != -1)
        found = hash_ptr_find_next(manager->name_hash, name.hash, *prev_found_and_finished_at, &finished_at);
    else
        found = hash_ptr_find(manager->name_hash, name.hash);

    while(found != -1)
    {
        Hash_Ptr_Entry entry = manager->name_hash.entries[found];
        Resource_Info* ptr = (Resource_Info*) hash_ptr_ptr_restore((void*) entry.value);

        if(string_is_equal(string_from_builder(ptr->name), string_from_hashed(name)))
        {
            out.id = ptr->id;
            out.ptr = ptr;
            break;
        }

        found = hash_ptr_find_next(manager->name_hash, name.hash, found, &finished_at);
    }

    if(prev_found_and_finished_at)
        *prev_found_and_finished_at = found;

    return out;
}

EXPORT Resource_Ptr resource_get_by_path(Resource_Manager* manager, Hash_String path, isize* prev_found_and_finished_at)
{
    Resource_Ptr out = {0};
    
    isize finished_at = 0;
    isize found = 0;
    if(prev_found_and_finished_at && *prev_found_and_finished_at != -1)
        found = hash_ptr_find_next(manager->path_hash, path.hash, *prev_found_and_finished_at, &finished_at);
    else
        found = hash_ptr_find(manager->path_hash, path.hash);

    while(found != -1)
    {
        Hash_Ptr_Entry entry = manager->path_hash.entries[found];
        Resource_Info* ptr = (Resource_Info*) hash_ptr_ptr_restore((void*) entry.value);

        if(string_is_equal(string_from_builder(ptr->path), string_from_hashed(path)))
        {
            out.id = ptr->id;
            out.ptr = ptr;
            break;
        }

        found = hash_ptr_find_next(manager->path_hash, path.hash, found, &finished_at);
    }

    if(prev_found_and_finished_at)
        *prev_found_and_finished_at = found;

    return out;
}

EXPORT Resource_Ptr resource_get(Resource_Manager* manager, Id id)
{
    return _resource_get(manager, id, NULL);
}

void _resource_log_wrong_id(Resource_Ptr out)
{
    if(out.ptr != NULL)
    {
        if(out.ptr)
            LOG_ERROR("RESOURCE", "Wrong id %lli used. \n"
                    "curr name: " STRING_FMT " path:" STRING_FMT "\n",
                    (lli) out.id, STRING_PRINT(out.ptr->name), STRING_PRINT(out.ptr->path));
        else
            LOG_ERROR("RESOURCE", "Wrong id %lli used", (lli) out.id);

        log_group_push();
            log_callstack("RESOURCE", LOG_TYPE_ERROR, -1, 1);
        log_group_pop();

        ASSERT(false);
    }
}

EXPORT Resource_Ptr resource_insert(Resource_Manager* manager, Resource_Params params)
{
    Resource_Info* info = NULL;
    if(params.id == 0)
        params.id = id_generate();

    Resource_Ptr out = resource_get(manager, params.id);
    if(out.id != NULL)
    {
        LOG_ERROR("RESOURCE", "Duplicate id %lli added. \n"
            "Old name: " STRING_FMT " path:" STRING_FMT "\n"
            "New name: " STRING_FMT " path:" STRING_FMT "\n", 
            (lli) params.id, 
            STRING_PRINT(out.ptr->name), STRING_PRINT(out.ptr->path), 
            STRING_PRINT(params.name), STRING_PRINT(params.path));
        
        return out;
    }

    isize index = stable_array_insert(&manager->storage, (void**) &info);
    i64 now = platform_universal_epoch_time();

    ASSERT(params.name.size != 0);

    info->data = info + 1;
    info->id = params.id;
    info->reference_count = 1;
    info->storage_index = (u32) index;
    Allocator* alloc = manager->storage.allocator;
    info->path = builder_from_string(params.path, alloc);
    info->name = builder_from_string(params.name, alloc);
    platform_capture_call_stack(info->callstack.stack_frames, STATIC_ARRAY_SIZE(info->callstack.stack_frames), 1);
    info->creation_etime = now;
    info->modified_etime = now;
    info->death_etime = params.death_etime;
    info->type_enum = manager->type_enum;
    info->reload = params.reload;
    info->lifetime = params.lifetime;

    if(params.was_loaded)
        info->load_etime = now;

    Hash_String name_hashed = hash_string_from_string(params.name);
    Hash_String path_hashed = hash_string_from_string(params.path);

    hash_ptr_insert(&manager->id_hash, (u64) params.id, (u64) info);
    hash_ptr_insert(&manager->name_hash, name_hashed.hash, (u64) info);
    hash_ptr_insert(&manager->path_hash, path_hashed.hash, (u64) info);
    
    out.id = params.id;
    out.ptr = info;

    if((params.lifetime & RESOURCE_LIFETIME_SINGLE_FRAME) || (params.lifetime & RESOURCE_LIFETIME_EPHEMERAL))
        array_push(&manager->single_frame, out);
        
    if(params.lifetime & RESOURCE_LIFETIME_TIMED)
        array_push(&manager->timed, out);

    if(manager->constructor)
        manager->constructor(info->data);

    return out;
}

EXPORT bool resource_force_remove_custom(Resource_Manager* manager, Resource_Ptr resource, void* removed_data, isize removed_data_size, bool* was_copied)
{
    if(removed_data)
        ASSERT_MSG(removed_data_size == manager->type_size, "Incorrect size %lli submitted. Expected %lli", removed_data_size, manager->type_size);

    //@TODO: This adding and especially removing from hashes is kinda a big deal. Can we make it more compact? 
    //       Can we have like a bloom filter or something? -> NO THAT ONLY MAKES THINGS WORSE!

    if(resource_is_valid(resource))
    {
        Id id = resource.id;
        Resource_Info* info = resource.ptr;
        if(removed_data)
        {
            memmove(removed_data, info->data, removed_data_size);
            if(was_copied)
                *was_copied = false;
        }
        else if(manager->destructor)
            manager->destructor(info->data);
            
        Hash_String name = hash_string_from_builder(info->name);
        Hash_String path = hash_string_from_builder(info->path);

        isize id_found = -1;
        isize name_found = -1;
        isize path_found = -1;

        Resource_Ptr by_id = _resource_get(manager, id, &id_found);
        Resource_Ptr by_name = {0};
        Resource_Ptr by_path = {0};

        while(by_name.id != id)
        {
            by_name = resource_get_by_name(manager, name, &name_found);
            if(resource_is_valid(by_name) == false)
            {
                ASSERT_MSG(false, "Must never happen");
                break;
            }
        }
        
        while(by_path.id != id)
        {
            by_path = resource_get_by_path(manager, path, &path_found);
            if(resource_is_valid(by_path) == false)
            {
                ASSERT_MSG(false, "Must never happen");
                break;
            }
        }

        ASSERT(by_id.ptr == info);
        ASSERT(by_name.ptr == info);
        ASSERT(by_path.ptr == info);

        if(id_found != -1)
            hash_ptr_remove(&manager->id_hash, id_found);
            
        if(name_found != -1)
            hash_ptr_remove(&manager->name_hash, name_found);
            
        if(path_found != -1)
            hash_ptr_remove(&manager->path_hash, path_found);

        array_deinit(&info->path);
        array_deinit(&info->name);
        stable_array_remove(&manager->storage, info->storage_index);
    }
    else
        _resource_log_wrong_id(resource);

    if(was_copied)
        *was_copied = false;
    return false;
}

EXPORT bool resource_force_remove(Resource_Manager* manager, Resource_Ptr resource)
{
    return resource_force_remove_custom(manager, resource, NULL, 0, NULL);
}

EXPORT bool resource_remove_custom(Resource_Manager* manager, Resource_Ptr resource, void* removed_data, isize removed_data_size, bool* was_copied)
{
    if(resource_is_valid(resource))
    {
        Resource_Info* info = resource.ptr;
        if(info->reference_count <= 1 && (info->lifetime & RESOURCE_LIFETIME_PERSISTANT) == 0)
            resource_force_remove_custom(manager, resource, removed_data, removed_data_size, was_copied);
        else
            info->reference_count -= 1;

        return true;
    }
    else
        _resource_log_wrong_id(resource);

    return false;
}

EXPORT bool resource_remove(Resource_Manager* manager, Resource_Ptr resource)
{
    return resource_remove_custom(manager, resource, NULL, 0, NULL);
}

EXPORT Resource_Ptr resource_share(Resource_Manager* manager, Resource_Ptr resource)
{
    (void) manager;
    Resource_Ptr out = {0};
    if(resource_is_valid(resource))
    {
        resource.ptr->reference_count += 1;
        out = resource;
    }
    else
        _resource_log_wrong_id(resource);

    return out;
}
EXPORT Resource_Ptr resource_make_unique(Resource_Manager* manager, Resource_Ptr resource, Resource_Params params)
{
    Resource_Ptr out = {0};
    if(resource_is_valid(resource))
    {
        if(resource.ptr->reference_count <= 1)
            out = resource;
        else
            out = resource_duplicate(manager, resource, params);
    }
    else
        _resource_log_wrong_id(resource);

    return out;
}
EXPORT Resource_Ptr resource_duplicate(Resource_Manager* manager, Resource_Ptr resource, Resource_Params params)
{
    Resource_Ptr out = {0};
    if(resource_is_valid(resource))
    {
        Resource_Info* info = resource.ptr;
        out = resource_insert(manager, params);
        
        if(manager->copy)
            manager->copy(out.ptr->data, info->data);
    }
    else
        _resource_log_wrong_id(resource);

    return out;
}


EXPORT void resource_manager_frame_cleanup(Resource_Manager* manager)
{
    for(isize i = 0; i < manager->single_frame.size; i++)
    {
        Resource_Ptr resource = manager->single_frame.data[i];
        resource_remove(manager, resource);
    }

    array_clear(&manager->single_frame);
}

EXPORT void resource_manager_time_cleanup(Resource_Manager* manager)
{
    i64 now = platform_universal_epoch_time();
    for(isize i = 0; i < manager->timed.size; i++)
    {
        Resource_Ptr* curr = &manager->timed.data[i];
        bool remove = true;
        if(resource_is_valid(*curr))
        {
            if(curr->ptr->death_etime >= now)
                resource_remove(manager, *curr);
            else
                remove = false;
        }

        if(remove)
        {
            Resource_Ptr* last = array_last(manager->timed);
            SWAP(curr, last, Resource_Ptr);
            array_pop(&manager->timed);
            i--; //repeat this index because the item here is the last item
        }
    }

    array_clear(&manager->single_frame);
}
