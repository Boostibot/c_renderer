#ifndef LIB_RESOURCES
#define LIB_RESOURCES

#include "resource_descriptions.h"

#define RESOURCE_NAME_SIZE 64
#define RESOURCE_CALLSTACK_SIZE 8
#define RESOURCE_EPHEMERAL_SIZE 4

typedef struct Resource_Callstack {
    void* stack_frames[RESOURCE_CALLSTACK_SIZE];
} Resource_Callstack;

typedef struct Uid {
    i64 epoch_time;
    i64 microsecond_index;
} Uid;

typedef struct Name {
    char data[RESOURCE_NAME_SIZE];
    isize size;
} Name;

//A list of concatenated filenames. Each filename is followed by a NULL character.
//This makes it ideal for comunicating with io (NULL terminated strings) and reducing complexity
// (vs String_Builder_Array)
typedef struct Filename_List {
    String_Builder string;
} Filename_List;

typedef enum Resource_Duration {
    RESOURCE_DURATION_REFERENCED = 0,   //cleaned up when reference count reaches 0
    RESOURCE_DURATION_TIME,             //cleaned up when time is up
    RESOURCE_DURATION_SINGLE_FRAME,     //cleaned up automatically when this frame ends
    RESOURCE_DURATION_PERSISTANT,       //cleaned up when explicitly demanded
    RESOURCE_DURATION_EPHEMERAL,        //cleaned up on some subsequent call to this function. Should be immediately used or coppied
} Resource_Duration;

typedef enum Resource_Reload {
    RESOURCE_RELOAD_ON_FILE_CHANGE = 0,
    RESOURCE_RELOAD_ON_MEMORY_CHANGE = 1,
    RESOURCE_RELOAD_NEVER = 2,
} Resource_Reload;

typedef enum Resource_Type {
    RESOURCE_TYPE_SHAPE,
    RESOURCE_TYPE_MAP,
    RESOURCE_TYPE_CUBEMAP,
    RESOURCE_TYPE_MATERIAL,
    RESOURCE_TYPE_TRIANGLE_MESH,
    RESOURCE_TYPE_SHADER,

    RESOURCE_TYPE_ENUM_COUNT
};

typedef struct Resource_Params {
    String path;
    String name;
    f64 duration_time_s;
    Resource_Duration duration_type;
    Resource_Reload reload_policy;
} Resource_Params;

typedef struct Resource_Info {
    Resource_Callstack callstack;

    i64 reference_count;

    i64 creation_epoch_time;
    i64 memory_change_epoch_time;
    i64 file_change_epoch_time;
    i64 last_load_epoch_time;

    f64 duration_time_s;
    Resource_Duration duration_type;
    Resource_Reload reload_policy;

    bool is_directory;
    bool is_file_present;
} Resource_Info;

// Handle means strong owning reference
// Ref    means weak non-owning reference

#define DEFINE_HANDLE_TYPES(Type)                           \
    typedef struct { i32 index; u32 generation; } Type##_Handle;             \
    typedef struct { i32 index; u32 generation; } Type##_Ref;                \
    DEFINE_ARRAY_TYPE(Type##_Handle, Type##_Handle_Array);  \
    DEFINE_ARRAY_TYPE(Type##_Ref, Type##_Ref_Array)         \

DEFINE_HANDLE_TYPES(Shape_Assembly);
DEFINE_HANDLE_TYPES(Map);
DEFINE_HANDLE_TYPES(Cubemap);
DEFINE_HANDLE_TYPES(Material);
DEFINE_HANDLE_TYPES(Triangle_Mesh);
DEFINE_HANDLE_TYPES(Shader);

DEFINE_ARRAY_TYPE(Handle, Handle_Array);

typedef struct Resources {
    Allocator* alloc;
    Clock clock;
    f64 check_time_every;
    isize frame_i;

    Handle_Table resources[RESOURCE_TYPE_ENUM_COUNT];
    Handle_Array timed_resources[RESOURCE_TYPE_ENUM_COUNT];
    Handle_Array frame_limited_resources[RESOURCE_TYPE_ENUM_COUNT];
} Resources;

typedef struct Shader {
    String_Builder vertex_shader_source;
    String_Builder fragment_shader_source;
    String_Builder geometry_shader_source;
} Shader;

typedef struct Map {
    Image_Builder image;
    Map_Info info;
} Map;

typedef struct Cubemap  {    
    union {
        struct {
            Map_Handle top;   
            Map_Handle bottom;
            Map_Handle front; 
            Map_Handle back;  
            Map_Handle left;  
            Map_Handle right;
        };

        Map_Handle faces[6];
    } maps;

} Cubemap;

typedef struct Material {
    Material_Info info;

    Map_Handle maps[MAP_TYPE_ENUM_COUNT];
    Cubemap_Handle cubemaps[CUBEMAP_TYPE_ENUM_COUNT];
} Material;

typedef struct Triangle_Mesh_Leaf_Group {
    Material_Handle material;

    i32 triangles_from;
    i32 triangles_to;
} Triangle_Mesh_Leaf_Group;

typedef struct Triangle_Mesh_Group {
    Material_Handle material;

    i32 next_i1;
    i32 child_i1;
    i32 depth;

    i32 triangles_from;
    i32 triangles_to;
} Triangle_Mesh_Group;

DEFINE_ARRAY_TYPE(Material, Material_Array);
DEFINE_ARRAY_TYPE(Triangle_Mesh_Group, Triangle_Mesh_Group_Array);
DEFINE_ARRAY_TYPE(Triangle_Mesh_Leaf_Group, Triangle_Mesh_Leaf_Group_Array);

typedef struct Triangle_Mesh
{
    Material_Handle fallback_material;
    Shape_Assembly_Handle shape;

    Triangle_Mesh_Group_Array groups;
} Triangle_Mesh;
    

Resources* resources_get();
Resources* resources_set(Resources* new_resources_ptr);

void resources_init(Resources* resources, Allocator* alloc);
void resources_deinit(Resources* resources);
void resources_cleanup_framed();
void resources_cleanup_timed();
void resources_end_frame();
void resources_check_reloads();

// 
// Triangle_Mesh_Handle h = {0};
// Resource_Params params = {STRING("resources/img.jpg")};
// Triangle_Mesh* t = triangle_mesh_add(&h, params);
//
// t->shape = shape_share(my_shape);
// t->fallback_material = material_share(metal);
// 
// Material_Handle blinking_material = material_load_from_disk(STRING("blinking.mat"), resource_params_timed(5.0));
// 
//


Map_Handle           map_add(Resource_Params info, Map** out_ptr_or_null);
Map_Handle           map_copy(const Map_Ref* old_ref, Map** out_ptr_or_null);
Map_Handle           map_make_shared(const Map_Ref* old_ref, Map** out_ptr_or_null);
Map_Handle           map_make_unique(const Map_Ref* old_ref, Map** out_ptr_or_null);
Map_Ref              map_find_by_path(String path, const Map_Ref* prev_found_or_null);
Map_Ref              map_find_by_name(String name, const Map_Ref* prev_found_or_null);
Map*                 map_get(const Map_Ref* old_ref);
Resource_Info*       map_get_info(const Map_Ref* old_ref);
bool                 map_get_path(String* name, const Map_Ref* old_ref);
bool                 map_get_name(String* name, const Map_Ref* old_ref);
bool                 map_set_path(String name, const Map_Ref* old_ref);
bool                 map_set_name(String name, const Map_Ref* old_ref);
bool                 map_remove(Map_Handle* handle);
bool                 map_force_remove(Map_Handle* handle);

Triangle_Mesh_Handle triangle_mesh_submit_all(Triangle_Mesh* mesh, Resource_Params info);
Triangle_Mesh_Handle triangle_mesh_add(Resource_Params info, Triangle_Mesh** out_ptr_or_null);
Triangle_Mesh_Handle triangle_mesh_copy(const Triangle_Mesh_Ref* old_ref, Triangle_Mesh** out_ptr_or_null);
Triangle_Mesh_Handle triangle_mesh_make_shared(const Triangle_Mesh_Ref* old_ref, Triangle_Mesh** out_ptr_or_null);
Triangle_Mesh_Handle triangle_mesh_make_unique(const Triangle_Mesh_Ref* old_ref, Triangle_Mesh** out_ptr_or_null);
Triangle_Mesh_Ref    triangle_mesh_find_by_path(String path, const Triangle_Mesh_Ref* prev_found_or_null);
Triangle_Mesh_Ref    triangle_mesh_find_by_name(String name, const Triangle_Mesh_Ref* prev_found_or_null);
Triangle_Mesh*       triangle_mesh_get(const Triangle_Mesh_Ref* old_ref);
Resource_Info*       triangle_mesh_get_info(const Triangle_Mesh_Ref* old_ref);
String               triangle_mesh_get_path(const Triangle_Mesh_Ref* old_ref);
bool                 triangle_mesh_remove(Triangle_Mesh_Handle* handle);
bool                 triangle_mesh_force_remove(Triangle_Mesh_Handle* handle);

#define DEFINE_RESOURCES_RESOURCE_DECL(Type, name, member_name)                         \
    Type##_Handle ##name##_add(Type* item, Resource_Params info);\
    void ##name##_remove(Type##_Handle handle);         \
    void ##name##_force_remove(Type##_Handle handle);   \
    Type##_Handle ##name##_share(Type##_Handle handle); \
    Type##_Handle ##name##_make_unique(Type##_Ref handle); \
    Type* ##name##_get(Type##_Handle handle);           \
    Resource_Info ##name##_get_info(Type##_Ref handle);                       \
    String ##name##_get_path(Type##_Ref handle);                              \
    Type##_Ref ##name##_find_by_path(String path);                            \
    Type##_Ref ##name##_find_by_name(String name);                            \

DEFINE_RESOURCES_RESOURCE_DECL(Shape_Assembly, shape, shapes)
DEFINE_RESOURCES_RESOURCE_DECL(Material, material, materials)
DEFINE_RESOURCES_RESOURCE_DECL(Material, material_phong, materials_phong)
DEFINE_RESOURCES_RESOURCE_DECL(Triangle_Mesh, triangle_mesh, triangle_meshs)
#endif


#if (defined(LIB_ALL_IMPL) || defined(LIB_RESOURCES_IMPL)) && !defined(LIB_RESOURCES_HAS_IMPL)
#define LIB_RESOURCES_HAS_IMPL

#if 0
    Type##_Handle resources_##name##_add(Type* item, Resource_Params info);\
    void resources_##name##_remove(Type##_Handle handle);         \
    void resources_##name##_force_remove(Type##_Handle handle);   \
    Type##_Handle resources_##name##_share(Type##_Handle handle); \
    Type##_Handle resources_##name##_make_unique(Type##_Ref handle); \
    Type* resources_##name##_get(Type##_Handle handle);           \
    Resource_Info resources_##name##_get_info(Type##_Ref handle);                       \
    String resources_##name##_get_path(Type##_Ref handle);                              \
    Type##_Ref resources_##name##_find_by_path(String path);                            
#endif
    typedef struct {                                                                    \
        Map data;                                                                      \
        Resource_Info info;                                                             \
        String_Builder full_path;                                                       \
    } _Resources_Map;                                                 \

    //@TODO: make similar thing for escaping stuff
    String file_get_ephemeral_full_path(String path)
    {
        enum {SLOT_COUNT = 4};
        static isize used_count = 0;
        static String_Builder full_paths[SLOT_COUNT] = {0};

        if(used_count == 0)
        {
            for(isize i = 0; i < SLOT_COUNT; i++)
                array_init(&full_paths[i], allocator_get_static());
        }

        isize curr_index = used_count % SLOT_COUNT;
        Error error = file_get_full_path(&full_paths[curr_index], path);
        String out_string = string_from_builder(full_paths[curr_index]);
        used_count += 1;

        if(error_is_ok(error) == false)
            LOG_ERROR("RESOURCES", "Failed to get full path of file " STRING_FMT " with error: " ERROR_FMT, STRING_PRINT(path), ERROR_PRINT(error));

        return out_string;
    }

    String name_to_string(const Name* name)
    {
        String out = {name->data, name->size};
    }

    void string_to_name(Name* name, String string)
    {
        String trimmed = string_safe_head(string, RESOURCE_NAME_SIZE);
        memcpy(name->data, trimmed.data, trimmed.size);
        name->size = trimmed.size;
    }

    Resource_Info resource_info_make(Resource_Params params)
    {   
        Resource_Info info = {0};
        info.creation_epoch_time = platform_universal_epoch_time();
        info.duration_time_s = params.duration_time_s;
        info.duration_type = params.duration_type;
        info.reload_policy = params.reload_policy;
        info.reference_count = 1;
        platform_capture_call_stack(info.callstack.stack_frames, RESOURCE_CALLSTACK_SIZE, 1);

        return info;
    }                          
    
    
    Map_Handle _map_add(String name, String path, Resource_Info info)
    {
        String emphemeral_full_path = file_get_ephemeral_full_path(path);
        Map_Ref ref = map_find_by_path(emphemeral_full_path, NULL);
        
        if(HANDLE_IS_NULL(ref) == false)
        {
            _Resources_Map* prev = (_Resources_Map*) handle_table_get(resources_get()->resources[RESOURCE_TYPE_MAP], (Handle*) &ref);
            LOG_WARN("RESOURCES", "added duplicit resource on path " STRING_FMT " previous named %s", STRING_PRINT(emphemeral_full_path), prev->data.name);
        }

        Map_Handle obtained = {0};
        _Resources_Map* created = (_Resources_Map*) handle_table_add(&resources_get()->resources[RESOURCE_TYPE_MAP], (Handle*) &obtained);

        created->info = info;
        created->full_path = builder_from_string(emphemeral_full_path, resources_get()->alloc);

        //Init here
        image_builder_init(&created->data.image, resources_get()->alloc, 1, PIXEL_FORMAT_U8);
        string_to_name(&created->data.image, name);

        return obtained;
    }

    Map_Handle map_add(Resource_Params params, Map** out_ptr_or_null)
    {
        Resource_Info info = resource_info_make(params);
        Map_Handle out = _map_add(params.path, info);
        return out;
    }
    
    Map_Handle map_copy(const Map_Ref* old_ref, Map** out_ptr_or_null)
    {
        
    }

    Map_Handle map_make_shared(const Map_Ref* old_ref, Map** out_ptr_or_null);
    Map_Handle map_make_unique(const Map_Ref* old_ref, Map** out_ptr_or_null);

    Map_Ref map_find_by_path(String full_path, const Map_Ref* prev_found_or_null)
    {   
        Map_Ref ref = {0};
        (void) prev_found_or_null; //@TODO
        HANDLE_TABLE_FOR_EACH_BEGIN(resources_get()->resources[RESOURCE_TYPE_MAP], 
            Map_Ref, found_ref, _Resources_Map*, map)
            if(string_is_equal(string_from_builder(map->full_path), full_path))
            {
                ref = found_ref;
                break;
            }
        HANDLE_TABLE_FOR_EACH_END

        return ref;
    }
    
    Map_Ref map_find_by_name(String name, const Map_Ref* prev_found_or_null)
    {   
        Map_Ref ref = {0};
        (void) prev_found_or_null;
        HANDLE_TABLE_FOR_EACH_BEGIN(resources_get()->resources[RESOURCE_TYPE_MAP], 
            Map_Ref, found_ref, _Resources_Map*, map)

            String found_name = name_to_string(&map->data.name);
            if(string_is_equal(found_name, name))
            {
                ref = found_ref;
                break;
            }
        HANDLE_TABLE_FOR_EACH_END

        return ref;
    }

    Map* map_get(const Map_Ref* old_ref)
    {   
        _Resources_Map* item = (_Resources_Map*) handle_table_get(resources_get()->resources[RESOURCE_TYPE_MAP], (Handle*) old_ref);
        if(item)
            return &item->data;
        else
            return NULL;
    }
    
    Resource_Info* map_get_info(const Map_Ref* old_ref)
    {
        _Resources_Map* item = (_Resources_Map*) handle_table_get(resources_get()->resources[RESOURCE_TYPE_MAP], (Handle*) old_ref);
        if(item)
            return &item->info;
        else
            return NULL;
    }
    
    String map_get_path(const Map_Ref* old_ref)
    {
        String path = {0};
        _Resources_Map* item = (_Resources_Map*) handle_table_get(resources_get()->resources[RESOURCE_TYPE_MAP], (Handle*) old_ref);
        if(item)
            path = string_from_builder(item->full_path);

        return path;
    }


    Map_Handle map_add(Map* item, String path, Resource_Params params)
    {
    }

    bool map_remove(Map_Handle handle)
    {
        _Resources_Map* prev = (_Resources_Map*) handle_table_get(resources_get()->resources[RESOURCE_TYPE_MAP], (Handle*) &handle);
        if(prev && prev->info.reference_count-- <= 0)
        {
            prev->info.reference_count = 0;
            if(prev->info.duration_type != RESOURCE_DURATION_PERSISTANT)
                map_force_remove(resources, handle);
        }

        return prev != NULL;
    }

    bool map_force_remove(Map_Handle handle)
    {
        _Resources_Map* prev = (_Resources_Map*) handle_table_get(resources_get()->resources[RESOURCE_TYPE_MAP], (Handle*) &handle);
        if(prev)
        {
            loaded_image_deinit(&prev->data);
            handle_table_remove(&resources_get()->resources[RESOURCE_TYPE_MAP], (Handle*) &handle);
        }

        return prev != NULL;
    }

    Map_Handle map_make_shared(Map_Handle handle)
    {
        Map_Handle out = {0};
        _Resources_Map* item = (_Resources_Map*) handle_table_get(resources_get()->resources[RESOURCE_TYPE_MAP], (Handle*) &handle);
        if(item)
        {
            item->info.reference_count += 1;
            out = handle;
        }

        return out;
    }

    void loaded_image_copy(Map* out, const Map* in)
    {
        image_builder_init_from_image(&out->image, resources_get()->alloc, image_from_builder(in->image));
        memcpy(&out->name, &in->name, sizeof in->name);
    }

    Map_Handle map_make_unique(Map_Ref handle)
    {
        Map_Handle out = {0};
        _Resources_Map* item = (_Resources_Map*) handle_table_get(resources_get()->resources[RESOURCE_TYPE_MAP], (Handle*) &handle);
        if(item)
        {
            //if its the only one it is already unique.
            if(item->info.reference_count == 1)
                out = *(Map_Handle*) &handle;
            //Else duplicate it
            else
            {   
                Map copy = {0};
                loaded_image_copy(&copy, &item->data, &resources);

                Resource_Info new_info = item->info;
                new_info.reference_count = 1;
                new_info.creation_epoch_time = platform_universal_epoch_time();
                platform_capture_call_stack(new_info.callstack.stack_frames, RESOURCE_CALLSTACK_SIZE, 1);

                out = _map_add(resources, &copy, string_from_builder(item->full_path), new_info);

                loaded_image_deinit(&copy);
            }
        }

        return out;
    }


void resources_init(Allocator* alloc)
{
    resources_deinit(resources);

    resources_get()->alloc = alloc;

    handle_table_init(&resources_get()->resources[RESOURCE_TYPE_MAP], alloc, sizeof(_Resources_Map), DEF_ALIGN);
    handle_table_init(&resources_get()->resources[RESOURCE_TYPE_SHAPE], alloc, sizeof(_Resources_Map), DEF_ALIGN);
    handle_table_init(&resources_get()->resources[RESOURCE_TYPE_OBJECT], alloc, sizeof(_Resources_Map), DEF_ALIGN);
    handle_table_init(&resources_get()->resources[RESOURCE_TYPE_MAP], alloc, sizeof(_Resources_Map), DEF_ALIGN);
    handle_table_init(&resources_get()->resources[RESOURCE_TYPE_CUBEMAP], alloc, sizeof(_Resources_Map), DEF_ALIGN);
    handle_table_init(&resources_get()->resources[RESOURCE_TYPE_SHADER], alloc, sizeof(_Resources_Map), DEF_ALIGN);
}


void resources_end_frame(Resources* resources);
void resources_check_reloads(Resources* resources);
void resources_update(Resources* resources);


void _resources_cleanup(Resources* resources)
{
    i64 global_epoch_time = platform_universal_epoch_time();
    f64 global_time = epoch_time_to_clock_time(global_epoch_time);
    f64 local_time = clock_get_local_time(resources_get()->clock, global_time);

    //@TODO: make proper
    static f64 last_update_time = 0;
    if(local_time - last_update_time < resources_get()->check_time_every)
        return;

    last_update_time = local_time;

    //@HACK: function type casting but it should be okay.
    typedef Resource_Info(*Get_Info_Func)(Resources*, Handle);
    typedef bool(*Remove_Func)(Resources*, Handle);

    Get_Info_Func get_infos[RESOURCE_TYPE_ENUM_COUNT] = {0};
    Remove_Func removes[RESOURCE_TYPE_ENUM_COUNT] = {0};

    get_infos[RESOURCE_TYPE_MAP] = map_get_info;
    removes[RESOURCE_TYPE_MAP] = map_remove;

    #error @TODO add the other types

    for(isize j = 0; j < RESOURCE_TYPE_ENUM_COUNT; j++)
    {
        Handle_Array* frame_limited = &resources_get()->frame_limited_resources[j];
        Handle_Array* time_limited = &resources_get()->timed_resources[j];
        Get_Info_Func get_info = get_infos[j];
        Remove_Func remove = removes[j];

        for(isize i = 0; i < time_limited->size; i++)
        {
            Handle h = frame_limited->data[i];
            Resource_Info info = get_info(resources, h);
            ASSERT(info.duration_type == RESOURCE_DURATION_TIME);
            f64 resource_creation_time = epoch_time_to_clock_time(info.creation_epoch_time);
         
            if(resource_creation_time + info.duration_time_s > local_time)
                remove(resources, h);
        }

        for(isize i = 0; i < frame_limited->size; i++)
        {
            Handle h = frame_limited->data[i];
            remove(resources, h);
        }
    }

    //@TODO the rest
}

//@TODO
void resources_deinit(bool is_complete_deinit)
{


    HANDLE_TABLE_FOR_EACH_BEGIN(resources_get()->resources[RESOURCE_TYPE_MAP], Map_Handle, h, _Resources_Map*, internal_image)
        map_force_remove(resources, h);
    HANDLE_TABLE_FOR_EACH_END

    #error do the rest
        
    for(isize j = 0; j < RESOURCE_TYPE_ENUM_COUNT; j++)
        handle_table_deinit(&resources_get()->resources[j]);

    memset(resources, 0, sizeof *resources);
}

void material_init(Material* item)
{
    material_deinit(item, resources); 
}

void material_deinit(Material* item)
{
    for(isize i = 0; i < MAP_TYPE_ENUM_COUNT; i++)
        resources_map_remove(&item->maps[i], resources);  
        
    for(isize i = 0; i < CUBEMAP_TYPE_ENUM_COUNT; i++)
        resources_cubemap_remove(&item->cubemaps[i], resources);  

    memset(item, 0, sizeof *item);
}

void triangle_mesh_group_init(Triangle_Mesh_Group* item)
{
    triangle_mesh_group_deinit(item, resources);
    array_init(&item->name, resources_get()->alloc);
}

void triangle_mesh_group_deinit(Triangle_Mesh_Group* item)
{
    array_deinit(&item->name);
    resources_material_remove(resources, item->material);
    memset(item, 0, sizeof *item);
}

void triangle_mesh_init(Triangle_Mesh* item)
{
    triangle_mesh_deinit(item, resources);

    array_init(&item->path, resources_get()->alloc);
    array_init(&item->name, resources_get()->alloc);
    array_init(&item->groups, resources_get()->alloc);
}

void triangle_mesh_deinit(Triangle_Mesh* item)
{
    array_deinit(&item->path);
    array_deinit(&item->name);
    resources_material_remove(resources, item->fallback_material);
    resources_shape_remove(resources, item->shape);

    for(isize i = 0; i < item->groups.size; i++)
        triangle_mesh_group_deinit(&item->groups.data[i], resources);
        
    array_init(&item->groups, resources_get()->alloc);
    memset(item, 0, sizeof *item);
}

#endif