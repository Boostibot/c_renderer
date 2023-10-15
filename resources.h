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
    RESOURCE_TYPE_IMAGE,
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
DEFINE_HANDLE_TYPES(Image);
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
    Image_Handle image;
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

Resource_Params      make_resource_params(String name, String path);
String               get_ephemeral_full_path(String path);
String               name_to_string(const Name* name);
void                 string_to_name(Name* name, String string);


#define DEFINE_RESOURCES_RESOURCE_DECL(Type, name)                         \
    Type##_Handle        name##_add(Resource_Params info, Map** out_ptr_or_null); \
    Type##_Handle        name##_copy(const Type##_Ref* old_ref, Map** out_ptr_or_null, Resource_Params params); \
    Type##_Handle        name##_make_shared(const Type##_Ref* old_ref, Map** out_ptr_or_null); \
    Type##_Handle        name##_make_unique(const Type##_Handle* old_ref, Map** out_ptr_or_null, Resource_Params params); \
    Type##_Ref           name##_find_by_path(String path, const Type##_Ref* prev_found_or_null); \
    Type##_Ref           name##_find_by_name(String name, const Type##_Ref* prev_found_or_null); \
    Type##_Ref           name##_find(String name, String path, const Type##_Ref* prev_found_or_null); \
    Map*                 name##_get(const Type##_Ref* old_ref); \
    Resource_Info*       name##_get_info(const Type##_Ref* old_ref); \
    bool                 name##_get_path(String* name, const Type##_Ref* old_ref); \
    bool                 name##_get_name(String* name, const Type##_Ref* old_ref); \
    bool                 name##_set_path(String name, const Type##_Ref* old_ref); \
    bool                 name##_set_name(String name, const Type##_Ref* old_ref); \
    bool                 name##_remove(Type##_Handle* handle); \
    bool                 name##_force_remove(Type##_Handle* handle); \
    
//#define TESTING
    
#define Storage_Type    Image_Builder
#define Type            Image
#define type_name       image
#include "resources_template_file.h"
    
#ifndef TESTING

#define Storage_Type    Shape_Assembly
#define Type            Shpae
#define type_name       shape
#include "resources_template_file.h"
 
#define Type            Map
#define type_name       map
#include "resources_template_file.h"
 
#define Storage_Type    Cubemap
#define Type            Cubemap
#define type_name       cubemap
#include "resources_template_file.h"
     
#define Storage_Type    Material
#define Type            Material
#define type_name       material
#include "resources_template_file.h"
     
#define Storage_Type    Triangle_Mesh
#define Type            Triangle_Mesh
#define type_name       trinagle_mesh
#include "resources_template_file.h"
     
#define Storage_Type    Shader
#define Type            Shader
#define type_name       shader
#include "resources_template_file.h"       
    
#endif

#endif


#if (defined(LIB_ALL_IMPL) || defined(LIB_RESOURCES_IMPL)) && !defined(LIB_RESOURCES_HAS_IMPL)
#define LIB_RESOURCES_HAS_IMPL

    #define RESOURCE_TEMPLATE_IMPL   
    
    #define Storage_Type    Image_Builder
    #define Type            Image
    #define type_name       image
    #include "resources_template_file.h"
    
    #ifndef TESTING

    #define Storage_Type    Shape_Assembly
    #define Type            Shape
    #define type_name       shape
    #include "resources_template_file.h"
 
    #define Type            Map
    #define type_name       map
    #include "resources_template_file.h"
 
    #define Storage_Type    Cubemap
    #define Type            Cubemap
    #define type_name       cubemap
    #include "resources_template_file.h"
     
    #define Storage_Type    Material
    #define Type            Material
    #define type_name       material
    #include "resources_template_file.h"
     
    #define Storage_Type    Triangle_Mesh
    #define Type            Triangle_Mesh
    #define type_name       trinagle_mesh
    #include "resources_template_file.h"
     
    #define Storage_Type    Shader
    #define Type            Shader
    #define type_name       shader
    #include "resources_template_file.h"       
    
    #endif

    #endif

    void _image_init(Image_Builder* out)
    {
        image_builder_init(out, resources_get()->alloc, 1, PIXEL_FORMAT_U8);
    }

    void _image_copy(Image_Builder* out, const Image_Builder* in)
    {
        image_builder_init_from_image(out, resources_get()->alloc, image_from_builder(*in));
    }

    void _image_deinit(Image_Builder* out)
    {
        image_builder_deinit(out);
    }
    
    void _material_init(Material* item)
    {
        _material_deinit(item); 
    }
    
    void _material_copy(Material* out, const Material* in)
    {
        for(isize i = 0; i < MAP_TYPE_ENUM_COUNT; i++)
            if(HANDLE_IS_VALID(out->maps[i]))
            {
                *out = map_make_shared()            
            }
                map_share(&out->maps[i]);  
        
        for(isize i = 0; i < CUBEMAP_TYPE_ENUM_COUNT; i++)
            if(HANDLE_IS_VALID(out->cubemaps[i]))
                cubemap_remove(&out->cubemaps[i]);  

        memset(out, 0, sizeof *out);
    }
    
    void _material_deinit(Material* out)
    {
        for(isize i = 0; i < MAP_TYPE_ENUM_COUNT; i++)
            if(HANDLE_IS_VALID(out->maps[i]))
                map_remove(&out->maps[i]);  
        
        for(isize i = 0; i < CUBEMAP_TYPE_ENUM_COUNT; i++)
            if(HANDLE_IS_VALID(out->cubemaps[i]))
                cubemap_remove(&out->cubemaps[i]);  

        memset(out, 0, sizeof *out);
    }

    void triangle_mesh_group_init(Triangle_Mesh_Group* item)
    {
        triangle_mesh_group_deinit(item);
        array_init(&item->name, resources_get()->alloc);
    }

    void triangle_mesh_group_deinit(Triangle_Mesh_Group* item)
    {
        array_deinit(&item->name);
        resources_material_remove(item->material);
        memset(item, 0, sizeof *item);
    }

    void triangle_mesh_init(Triangle_Mesh* item)
    {
        triangle_mesh_deinit(item);

        array_init(&item->path, resources_get()->alloc);
        array_init(&item->name, resources_get()->alloc);
        array_init(&item->groups, resources_get()->alloc);
    }

    void triangle_mesh_deinit(Triangle_Mesh* item)
    {
        array_deinit(&item->path);
        array_deinit(&item->name);
        resources_material_remove(item->fallback_material);
        resources_shape_remove(item->shape);

        for(isize i = 0; i < item->groups.size; i++)
            triangle_mesh_group_deinit(&item->groups.data[i], resources);
        
        array_init(&item->groups, resources_get()->alloc);
        memset(item, 0, sizeof *item);
    }

    //@TODO: make similar thing for escaping stuff
    String get_ephemeral_full_path(String path)
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
    
void resources_deinit()
{
    _resources_cleanup(resources_get(), _RESOURCE_CLEANUP_TOTAL);

    memset(resources_get(), 0, sizeof *resources_get());
}

void resources_init(Allocator* alloc)
{
    resources_deinit();
    resources_get()->alloc = alloc;

    handle_table_init(&resources_get()->resources[RESOURCE_TYPE_MAP], alloc, sizeof(_Image_Resource), DEF_ALIGN);
    handle_table_init(&resources_get()->resources[RESOURCE_TYPE_SHAPE], alloc, sizeof(_Shape_Resource), DEF_ALIGN);
    handle_table_init(&resources_get()->resources[RESOURCE_TYPE_TRIANGLE_MESH], alloc, sizeof(_Triangle_Mesh_Resource), DEF_ALIGN);
    handle_table_init(&resources_get()->resources[RESOURCE_TYPE_MAP], alloc, sizeof(_Map_Resource), DEF_ALIGN);
    handle_table_init(&resources_get()->resources[RESOURCE_TYPE_CUBEMAP], alloc, sizeof(_Cubemap_Resource), DEF_ALIGN);
    handle_table_init(&resources_get()->resources[RESOURCE_TYPE_SHADER], alloc, sizeof(_Shader_Resource), DEF_ALIGN);
}

enum {
    _RESOURCE_CLEANUP_TIME = 1,
    _RESOURCE_CLEANUP_FRAME = 2,
    _RESOURCE_CLEANUP_PERSISTANT = 4,
    _RESOURCE_CLEANUP_TOTAL = 32,
};

void _resources_cleanup(Resources* resources, i64 cleanup_flags)
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
    typedef struct Resource_Functions {
        Resource_Info*  (*get_info)(Handle);
        bool            (*remove)(Handle);
        bool            (*force_remove)(Handle);
    } Resource_Functions;

    Resource_Functions resource_functions[RESOURCE_TYPE_ENUM_COUNT] = {0};

    //@TODO rest
    resource_functions[RESOURCE_TYPE_IMAGE] = BRACE_INIT(Resource_Functions){image_get_info, image_remove, image_force_remove};

    for(isize j = 0; j < RESOURCE_TYPE_ENUM_COUNT; j++)
    {
        Handle_Table* handle_table = &resources->resources[j];
        Handle_Array* frame_limited = &resources_get()->frame_limited_resources[j];
        Handle_Array* time_limited = &resources_get()->timed_resources[j];
        Resource_Functions funcs = resource_functions[j];

        if(cleanup_flags & _RESOURCE_CLEANUP_TIME)
        {
            for(isize i = 0; i < time_limited->size; i++)
            {
                Handle h = frame_limited->data[i];
                Resource_Info* info = funcs.get_info(h);
                ASSERT(info->duration_type == RESOURCE_DURATION_TIME);
                f64 resource_creation_time = epoch_time_to_clock_time(info->creation_epoch_time);
         
                if(resource_creation_time + info->duration_time_s > local_time)
                    funcs.remove(h);
            }
        }
        
        if(cleanup_flags & _RESOURCE_CLEANUP_FRAME)
        {
            for(isize i = 0; i < frame_limited->size; i++)
            {
                Handle h = frame_limited->data[i];
                funcs.remove(h);
            }
        }
        
        if(cleanup_flags & _RESOURCE_CLEANUP_TOTAL)
        {
            for(isize i = 0; i < handle_table->slots.size; i++)
            {
                Handle_Table_Slot slot = handle_table->slots.data[i];
                void* ptr = slot.ptr;
                if(ptr == NULL)
                    continue;

                Handle h = {i, slot.generation};
                funcs.force_remove(h);
            }
        }
    }

    //@TODO the rest
}



#endif