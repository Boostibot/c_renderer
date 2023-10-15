//THIS IS A TEMPLATE FILE DONT INCLUDE IT ON ITS OWN!

//To include this file the following need to be defined:
#ifndef Storage_Type
    #error Storage_Type must be defined to one of the resource types!
#endif

#ifndef Type
    #error Type must be defined to one of the resource types!
#endif

#ifndef type_name
    #error type_name must be defined
#endif


#include "defines.h"
#include "string.h"
#include "resources.h"

#define CC_(a, b, c) a##b##c
#define CC2(a, b) CC_(a, b,)
#define CC3(a, b, c) CC_(a, b, c)

#if 0
typedef int Storage_Type;
typedef struct Type_Ref {int a, b;} Type_Ref;
typedef struct Type_Handle {int a, b;} Type_Handle;

#else

#define Type_Ref    CC2(Type,_Ref)
#define Type_Handle CC2(Type,_Handle)
#define _Resource   CC3(_,Type,_Resource)

#endif


#define resource_add            CC2(type_name,_add)
#define resource_copy           CC2(type_name,_copy)
#define resource_make_shared    CC2(type_name,_make_shared)
#define resource_make_unique    CC2(type_name,_make_unique)
#define resource_find_by_path   CC2(type_name,_find_by_path)
#define resource_find_by_name   CC2(type_name,_find_by_name)
#define resource_find           CC2(type_name,_find)
#define resource_get            CC2(type_name,_get)
#define resource_get_info       CC2(type_name,_get_info)
#define resource_get_path       CC2(type_name,_get_path)
#define resource_get_name       CC2(type_name,_get_name)
#define resource_set_path       CC2(type_name,_set_path)
#define resource_set_name       CC2(type_name,_set_name)
#define resource_remove         CC2(type_name,_remove)
#define resource_force_remove   CC2(type_name,_force_remove)

//
#define _resource_find          CC3(_,type_name,_find)
#define _resource_add           CC3(_,type_name,_add)

#define _resource_deinit        CC3(_,type_name,_deinit)
#define _resource_copy          CC3(_,type_name,_copy)
#define _resource_init          CC3(_,type_name,_init)

Type_Handle     resource_add(Resource_Params info, Storage_Type** out_ptr_or_null);
Type_Handle     resource_copy(const Type_Ref* old_ref, Storage_Type** out_ptr_or_null, Resource_Params params);
Type_Handle     resource_make_shared(const Type_Ref* old_ref, Storage_Type** out_ptr_or_null);
Type_Handle     resource_make_unique(const Type_Handle* old_ref, Storage_Type** out_ptr_or_null, Resource_Params params);
Type_Ref        resource_find_by_path(String path, const Type_Ref* prev_found_or_null);
Type_Ref        resource_find_by_name(String name, const Type_Ref* prev_found_or_null);
Type_Ref        resource_find(String name, String path, const Type_Ref* prev_found_or_null);
Storage_Type*           resource_get(const Type_Ref* old_ref);
Resource_Info*  resource_get_info(const Type_Ref* old_ref);
bool            resource_get_path(String* name, const Type_Ref* old_ref);
bool            resource_get_name(String* name, const Type_Ref* old_ref);
bool            resource_set_path(String name, const Type_Ref* old_ref);
bool            resource_set_name(String name, const Type_Ref* old_ref);
bool            resource_remove(Type_Handle* handle);
bool            resource_force_remove(Type_Handle* handle);

#ifdef RESOURCE_TEMPLATE_IMPL

//These need to be defined if impl is desired!
void _resource_init(Storage_Type* out);
void _resource_copy(Storage_Type* out, const Storage_Type* in);
void _resource_deinit(Storage_Type* out);

typedef struct {                                                                    
    Storage_Type data;                                                                       
    Resource_Info info;                                                             
    String_Builder full_path;                                                       
    String_Builder name;
} _Resource;  

INTERNAL Type_Ref _resource_find(String full_path, bool by_path, String name, bool by_name, const Type_Ref* prev_found_or_null)
{
    Type_Ref ref = {0};
    (void) prev_found_or_null;
    HANDLE_TABLE_FOR_EACH_BEGIN(resources_get()->resources[RESOURCE_TYPE_MAP], 
        Type_Ref, found_ref, _Resource*, map)
        if(by_name && string_is_equal(string_from_builder(map->name), name) == false)
            continue;

        if(by_path && string_is_equal(string_from_builder(map->full_path), full_path) == false)
            continue;

        ref = found_ref;
        break;
    HANDLE_TABLE_FOR_EACH_END

    return ref;
}

INTERNAL Type_Handle _resource_add(String name, String path, Resource_Info info)
{
    String emphemeral_full_path = get_ephemeral_full_path(path);

    #ifdef DEBUG
    Type_Ref ref = resource_find(name, emphemeral_full_path, NULL);
    if(HANDLE_IS_NULL(ref) == false)
    {
        _Resource* prev = (_Resource*) handle_table_get(resources_get()->resources[RESOURCE_TYPE_MAP], (Handle*) &ref);
        LOG_WARN("RESOURCES", "added duplicit resource on path " STRING_FMT " named " " previous named ", STRING_PRINT(emphemeral_full_path), STRING_PRINT(name), STRING_PRINT(prev->name));
    }
    #endif // DEBUG

    Type_Handle obtained = {0};
    _Resource* created = (_Resource*) handle_table_add(&resources_get()->resources[RESOURCE_TYPE_MAP], (Handle*) &obtained);

    created->info = info;
    created->full_path = builder_from_string(emphemeral_full_path, resources_get()->alloc);
    created->name = builder_from_string(name, resources_get()->alloc);

    _resource_init(&created->data);

    return obtained;
}

Type_Ref resource_find_by_path(String full_path, const Type_Ref* prev_found_or_null)
{   
    return _resource_find(full_path, true, STRING(""), false, prev_found_or_null);
}
    
Type_Ref resource_find_by_name(String name, const Type_Ref* prev_found_or_null)
{   
    return _resource_find(STRING(""), false, name, true, prev_found_or_null);
}
    
Type_Ref resource_find(String full_path, String name, const Type_Ref* prev_found_or_null)
{
    return _resource_find(full_path, true, name, true, prev_found_or_null);
}

Storage_Type* resource_get(const Type_Ref* old_ref)
{   
    _Resource* item = (_Resource*) handle_table_get(resources_get()->resources[RESOURCE_TYPE_MAP], (Handle*) old_ref);
    if(item)
        return &item->data;
    else
        return NULL;
}
    
Resource_Info* resource_get_info(const Type_Ref* old_ref)
{
    _Resource* item = (_Resource*) handle_table_get(resources_get()->resources[RESOURCE_TYPE_MAP], (Handle*) old_ref);
    if(item)
        return &item->info;
    else
        return NULL;
}
    
bool resource_get_path(String* path, const Type_Ref* old_ref)
{
    _Resource* item = (_Resource*) handle_table_get(resources_get()->resources[RESOURCE_TYPE_MAP], (Handle*) old_ref);
    if(item)
        *path = string_from_builder(item->full_path);

    return item != NULL;
}

bool resource_get_name(String* name, const Type_Ref* old_ref)
{
    _Resource* item = (_Resource*) handle_table_get(resources_get()->resources[RESOURCE_TYPE_MAP], (Handle*) old_ref);
    if(item)
        *name = string_from_builder(item->name);

    return item != NULL;
}

bool resource_set_path(String path, const Type_Ref* old_ref)
{
    _Resource* item = (_Resource*) handle_table_get(resources_get()->resources[RESOURCE_TYPE_MAP], (Handle*) old_ref);
    if(item)
    {
        String full_path = get_ephemeral_full_path(path);
        builder_assign(&item->name, full_path);
    }

    return item != NULL;
}

bool resource_set_name(String name, const Type_Ref* old_ref)
{
    _Resource* item = (_Resource*) handle_table_get(resources_get()->resources[RESOURCE_TYPE_MAP], (Handle*) old_ref);
    if(item)
        builder_assign(&item->name, name);

    return item != NULL;
}

String resource_get_path(const Type_Ref* old_ref)
{
    String path = {0};
    _Resource* item = (_Resource*) handle_table_get(resources_get()->resources[RESOURCE_TYPE_MAP], (Handle*) old_ref);
    if(item)
        path = string_from_builder(item->full_path);

    return path;
}
    
Type_Handle resource_add(Resource_Params params, Storage_Type** out_ptr_or_null)
{
    Resource_Info info = resource_info_make(params);
    Type_Handle out = _resource_add(params.name, params.path, info);
    return out;
}
    

bool resource_remove(Type_Handle* handle)
{
    _Resource* prev = (_Resource*) handle_table_get(resources_get()->resources[RESOURCE_TYPE_MAP], (Handle*) &handle);
    if(prev && prev->info.reference_count-- <= 0)
    {
        prev->info.reference_count = 0;
        if(prev->info.duration_type != RESOURCE_DURATION_PERSISTANT)
            resource_force_remove(handle);
    }

    Type_Handle null = {0};
    *handle = null;
    return prev != NULL;
}
    

bool resource_force_remove(Type_Handle* handle)
{
    _Resource* prev = (_Resource*) handle_table_get(resources_get()->resources[RESOURCE_TYPE_MAP], (Handle*) &handle);
    if(prev)
    {
        _resource_deinit(&prev->data);

        array_deinit(&prev->full_path);
        array_deinit(&prev->name);
        handle_table_remove(&resources_get()->resources[RESOURCE_TYPE_MAP], (Handle*) &handle);
    }
        
    Type_Handle null = {0};
    *handle = null;
    return prev != NULL;
}
    
Type_Handle resource_make_shared(const Type_Ref* old_ref, Storage_Type** out_ptr_or_null)
{
    Type_Handle out = {0};
    _Resource* item = (_Resource*) handle_table_get(resources_get()->resources[RESOURCE_TYPE_MAP], (Handle*) old_ref);
    if(item)
    {
        if(out_ptr_or_null)
            *out_ptr_or_null = &item->data;

        item->info.reference_count += 1;
        out = *(Type_Handle*) old_ref;
    }

    return out;
}
    

Type_Handle resource_copy(const Type_Ref* old_ref, Storage_Type** out_ptr_or_null, Resource_Params params)
{
    Type_Handle out = {0};
    _Resource* item = (_Resource*) handle_table_get(resources_get()->resources[RESOURCE_TYPE_MAP], (Handle*) old_ref);
    if(item)
    {
        Storage_Type* made = NULL;
        Resource_Info new_info = item->info;
        new_info.reference_count = 1;
        new_info.duration_type = params.duration_type;
        new_info.duration_time_s = params.duration_time_s;
        new_info.reload_policy = params.reload_policy;

        out = _resource_add(params.name, params.path, new_info);
        _resource_copy(made, &item->data);

        if(out_ptr_or_null)
            *out_ptr_or_null = made;
    }
}

Type_Handle resource_make_unique(const Type_Handle* old_handle, Storage_Type** out_ptr_or_null, Resource_Params params)
{
    Type_Handle out = {0};
    _Resource* item = (_Resource*) handle_table_get(resources_get()->resources[RESOURCE_TYPE_MAP], (Handle*) old_handle);
    if(item)
    {
        //if its the only one it is already unique.
        if(item->info.reference_count <= 1)
            out = *old_handle;
        //Else duplicate it 
        else
        {
            item->info.reference_count -= 1;
            out = resource_copy((Type_Ref*) old_handle, out_ptr_or_null, params);
        }
    }

    return out;
}
#endif

//undef all the things!
#undef Storage_Type
#undef type_name


#undef Type_Ref 
#undef Handle 
#undef _Resource 

#undef resource_add            
#undef resource_copy           
#undef resource_make_shared    
#undef resource_make_unique    
#undef resource_find_by_path   
#undef resource_find_by_name   
#undef resource_find           
#undef resource_get            
#undef resource_get_info       
#undef resource_get_path       
#undef resource_get_name       
#undef resource_set_path       
#undef resource_set_name       
#undef resource_remove         
#undef resource_force_remove   

#undef _resource_find          
#undef _resource_add           

#undef _resource_deinit        
#undef _resource_copy          
#undef _resource_init          