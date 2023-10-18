#ifndef LIB_RESOURCES
#define LIB_RESOURCES

#include "resource_descriptions.h"
#include "file.h"

#define RESOURCE_NAME_SIZE 64
#define RESOURCE_CALLSTACK_SIZE 8
#define RESOURCE_EPHEMERAL_SIZE 4

typedef struct Resource_Callstack {
    void* stack_frames[RESOURCE_CALLSTACK_SIZE];
} Resource_Callstack;


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
} Resource_Type;

typedef struct Resource_Params {
    String path;
    String name;
    f64 duration_time_s;
    Resource_Duration duration_type;
    Resource_Reload reload_policy;
    bool was_loaded;
} Resource_Params;

typedef struct Resource_Info {
    Resource_Callstack callstack;
    i64 reference_count;

    i64 creation_epoch_time;
    i64 last_load_epoch_time;

    f64 duration_time_s;
    Resource_Duration duration_type;
    Resource_Reload reload_policy;
} Resource_Info;

// Handle means strong owning reference
// Ref    means weak non-owning reference

#define DEFINE_HANDLE_TYPES(Type)                           \
    typedef struct { i32 index; u32 generation; } Type##_Handle;             \
    typedef struct { i32 index; u32 generation; } Type##_Ref;                \
    DEFINE_ARRAY_TYPE(Type##_Handle, Type##_Handle_Array);  \
    DEFINE_ARRAY_TYPE(Type##_Ref, Type##_Ref_Array)         \

DEFINE_HANDLE_TYPES(Shape);
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
            Map top;   
            Map bottom;
            Map front; 
            Map back;  
            Map left;  
            Map right;
        };

        Map faces[6];
    } maps;
} Cubemap;

typedef struct Material {
    Material_Info info;

    Map maps[MAP_TYPE_ENUM_COUNT];
    Cubemap cubemaps[CUBEMAP_TYPE_ENUM_COUNT];
} Material;

typedef struct Triangle_Mesh_Leaf_Group {
    i32 material_i1;

    i32 triangles_from;
    i32 triangles_to;
} Triangle_Mesh_Leaf_Group;

typedef struct Triangle_Mesh_Group {
    Name name;

    i32 material_i1;
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
    i32 fallback_material_i1;
    Shape_Handle shape;

    Triangle_Mesh_Group_Array groups;
    Material_Handle_Array materials;
} Triangle_Mesh;

Resources* resources_get();
Resources* resources_set(Resources* new_resources_ptr);
Allocator* resources_allocator();

void resources_init(Resources* resources, Allocator* alloc);
void resources_deinit(Resources* resources);
void resources_cleanup_framed();
void resources_cleanup_timed();
void resources_end_frame();
void resources_check_reloads();

Resource_Params      make_resource_params(String name, String path);
String               string_from_name(const Name* name);
void                 name_from_string(Name* name, String string);
    
#define Storage_Type    Image_Builder
#define Type            Image
#define RESOURCE_TYPE   RESOURCE_TYPE_IMAGE
#define type_name       image
#include "resources_template.h"

#define Storage_Type    Shape_Assembly
#define Type            Shape
#define RESOURCE_TYPE   RESOURCE_TYPE_SHAPE
#define type_name       shape
#include "resources_template.h"
 
#define Storage_Type    Map
#define Type            Map
#define RESOURCE_TYPE   RESOURCE_TYPE_MAP
#define type_name       map
#include "resources_template.h"
 
#define Storage_Type    Cubemap
#define Type            Cubemap
#define RESOURCE_TYPE   RESOURCE_TYPE_CUBEMAP
#define type_name       cubemap
#include "resources_template.h"
     
#define Storage_Type    Material
#define Type            Material
#define RESOURCE_TYPE   RESOURCE_TYPE_MATERIAL
#define type_name       material
#include "resources_template.h"
     
#define Storage_Type    Triangle_Mesh
#define Type            Triangle_Mesh
#define RESOURCE_TYPE   RESOURCE_TYPE_TRIANGLE_MESH
#define type_name       triangle_mesh
#include "resources_template.h"
     
#define Storage_Type    Shader
#define Type            Shader
#define RESOURCE_TYPE   RESOURCE_TYPE_SHADER
#define type_name       shader
#include "resources_template.h"       

#endif


#if (defined(LIB_ALL_IMPL) || defined(LIB_RESOURCES_IMPL)) && !defined(LIB_RESOURCES_HAS_IMPL)
#define LIB_RESOURCES_HAS_IMPL

    
    Resource_Info resource_info_make(Resource_Params params)
    {   
        Resource_Info info = {0};
        info.creation_epoch_time = platform_universal_epoch_time();
        info.duration_time_s = params.duration_time_s;
        info.duration_type = params.duration_type;
        info.reload_policy = params.reload_policy;
        info.reference_count = 1;
        if(params.was_loaded)
            info.last_load_epoch_time = info.creation_epoch_time;
        platform_capture_call_stack(info.callstack.stack_frames, RESOURCE_CALLSTACK_SIZE, 1);

        return info;
    }   
    
    //#define TESTING
    #ifndef TESTING
    #define RESOURCE_TEMPLATE_IMPL   
    
    #define Storage_Type    Image_Builder
    #define Type            Image
    #define RESOURCE_TYPE   RESOURCE_TYPE_IMAGE
    #define type_name       image
    #include "resources_template.h"

    #define Storage_Type    Shape_Assembly
    #define Type            Shape
    #define RESOURCE_TYPE   RESOURCE_TYPE_SHAPE
    #define type_name       shape
    #include "resources_template.h"
 
    #define Storage_Type    Map
    #define Type            Map
    #define RESOURCE_TYPE   RESOURCE_TYPE_MAP
    #define type_name       map
    #include "resources_template.h"
 
    #define Storage_Type    Cubemap
    #define Type            Cubemap
    #define RESOURCE_TYPE   RESOURCE_TYPE_CUBEMAP
    #define type_name       cubemap
    #include "resources_template.h"
     
    #define Storage_Type    Material
    #define Type            Material
    #define RESOURCE_TYPE   RESOURCE_TYPE_MATERIAL
    #define type_name       material
    #include "resources_template.h"
     
    #define Storage_Type    Triangle_Mesh
    #define Type            Triangle_Mesh
    #define RESOURCE_TYPE   RESOURCE_TYPE_TRIANGLE_MESH
    #define type_name       triangle_mesh
    #include "resources_template.h"
     
    #define Storage_Type    Shader
    #define Type            Shader
    #define RESOURCE_TYPE   RESOURCE_TYPE_SHADER
    #define type_name       shader
    #include "resources_template.h"      
    
    #endif

    #undef RESOURCE_TEMPLATE_IMPL

    Resources* _global_resources = NULL;
    Resources* resources_get()
    {
        return _global_resources;
    }

    Resources* resources_set(Resources* new_resources_ptr)
    {
        Resources* prev = _global_resources;
        _global_resources = new_resources_ptr;
        return prev;
    }
    
    Allocator* resources_allocator()
    {
        ASSERT(_global_resources);
        if(_global_resources)
            return _global_resources->alloc;
        else
            return NULL;
    }

    //SHAPE
    void _shape_init(Shape_Assembly* out)
    {
        shape_assembly_init(out, resources_allocator());
    }

    void _shape_copy(Shape_Assembly* out, const Shape_Assembly* in)
    {
        shape_assembly_copy(out, in);
    }

    void _shape_deinit(Shape_Assembly* out)
    {
        shape_assembly_deinit(out);
    }

    //IMAGE
    void _image_init(Image_Builder* out)
    {
        image_builder_init(out, resources_allocator(), 1, PIXEL_FORMAT_U8);
    }

    void _image_copy(Image_Builder* out, const Image_Builder* in)
    {
        image_builder_init_from_image(out, resources_allocator(), image_from_builder(*in));
    }

    void _image_deinit(Image_Builder* out)
    {
        image_builder_deinit(out);
    }
    
    //MAP
    void _map_init(Map* out)
    {
        map_info_init(&out->info, resources_allocator());
        Image_Handle null = {0};
        out->image = null;
    }

    void _map_copy(Map* out, const Map* in)
    {
        out->info = in->info;
        if(HANDLE_IS_VALID(in->image))
            out->image = image_make_shared((Image_Ref*) &in->image, NULL);
    }

    void _map_deinit(Map* out)
    {
        image_remove(&out->image);
    }

    //CUBEMAP
    void _cubemap_init(Cubemap* out)
    {
        memset(out, 0, sizeof *out);
    }

    void _cubemap_copy(Cubemap* out, const Cubemap* in)
    {
        for(isize i = 0; i < 6; i++)
            _map_copy(&out->maps.faces[i], &in->maps.faces[i]);
    }

    void _cubemap_deinit(Cubemap* out)
    {
        for(isize i = 0; i < 6; i++)
            _map_deinit(&out->maps.faces[i]);
    }
    
    //MATERIAL
    void _material_init(Material* out)
    {
        memset(out, 0, sizeof *out);
    }

    void _material_copy(Material* out, const Material* in)
    {
        for(isize i = 0; i < MAP_TYPE_ENUM_COUNT; i++)
            _map_copy(&out->maps[i], &in->maps[i]);
        
        for(isize i = 0; i < CUBEMAP_TYPE_ENUM_COUNT; i++)
            _cubemap_copy(&out->cubemaps[i], &in->cubemaps[i]);

        memset(out, 0, sizeof *out);
    }
    
    void _material_deinit(Material* out)
    {
        for(isize i = 0; i < MAP_TYPE_ENUM_COUNT; i++)
            _map_deinit(&out->maps[i]);
        
        for(isize i = 0; i < CUBEMAP_TYPE_ENUM_COUNT; i++)
            _cubemap_deinit(&out->cubemaps[i]);

        memset(out, 0, sizeof *out);
    }

    //TRIANGLE MESH
   
    void _triangle_mesh_deinit(Triangle_Mesh* item)
    {
        shape_remove(&item->shape);

        for(isize i = 0; i < item->materials.size; i++)
            material_remove(&item->materials.data[i]);
        
        array_deinit(&item->groups);
        array_deinit(&item->materials);
        memset(item, 0, sizeof *item);
    }

    void _triangle_mesh_init(Triangle_Mesh* item)
    {
        _triangle_mesh_deinit(item);
        array_init(&item->groups, resources_allocator());
        array_init(&item->materials, resources_allocator());
    }
    
    void _triangle_mesh_copy(Triangle_Mesh* out, const Triangle_Mesh* in)
    {
        array_resize(&out->groups, in->groups.size);
        array_resize(&out->materials, in->materials.size);
        
        for(isize i = 0; i < in->materials.size; i++)
            out->materials.data[i] = material_make_shared((Material_Ref*) &in->materials.data[i], NULL);

        out->shape = shape_make_shared((Shape_Ref*) &in->shape, NULL);
    }

    //SHADER
    void _shader_init(Shader* out)
    {
        array_init(&out->fragment_shader_source, resources_allocator());
        array_init(&out->geometry_shader_source, resources_allocator());
        array_init(&out->vertex_shader_source, resources_allocator());
    }

    void _shader_copy(Shader* out, const Shader* in)
    {
        array_copy(&out->fragment_shader_source, in->fragment_shader_source);
        array_copy(&out->geometry_shader_source, in->geometry_shader_source);
        array_copy(&out->vertex_shader_source, in->vertex_shader_source);
    }

    void _shader_deinit(Shader* out)
    {
        array_deinit(&out->fragment_shader_source);
        array_deinit(&out->geometry_shader_source);
        array_deinit(&out->vertex_shader_source);
    }
    
    String string_from_name(const Name* name)
    {
        String out = {name->data, name->size};
        return out;
    }

    void name_from_string(Name* name, String string)
    {
        String trimmed = string_safe_head(string, RESOURCE_NAME_SIZE);
        memcpy(name->data, trimmed.data, trimmed.size);
        name->size = trimmed.size;
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
    f64 local_time = clock_get_local_time(resources->clock, global_time);

    //@TODO: make proper
    static f64 last_update_time = 0;
    if(local_time - last_update_time < resources->check_time_every)
        return;

    last_update_time = local_time;

    //@HACK: function type casting but it should be okay.
     
    typedef Resource_Info*  (*Get_info)(Handle*);
    typedef bool            (*Remove)(Handle*);
    typedef bool            (*Force_remove)(Handle*);

    typedef struct Resource_Functions {
        void* get_info;
        void* remove;
        void* force_remove;
    } Resource_Functions;

    Resource_Functions resource_functions[RESOURCE_TYPE_ENUM_COUNT] = {0};

    //@TODO rest
    resource_functions[RESOURCE_TYPE_IMAGE] = BRACE_INIT(Resource_Functions){(void*) image_get_info, (void*) image_remove, (void*) image_force_remove};

    for(isize j = 0; j < RESOURCE_TYPE_ENUM_COUNT; j++)
    {
        Handle_Table* handle_table = &resources->resources[j];
        Handle_Array* frame_limited = &resources->frame_limited_resources[j];
        Handle_Array* time_limited = &resources->timed_resources[j];
        Resource_Functions funcs = resource_functions[j];

        Get_info get_info = (Get_info) funcs.get_info;
        Remove remove = (Remove) funcs.remove;
        Force_remove force_remove = (Force_remove) funcs.force_remove;

        if(cleanup_flags & _RESOURCE_CLEANUP_TIME)
        {
            for(isize i = 0; i < time_limited->size; i++)
            {
                Handle h = frame_limited->data[i];
                Resource_Info* info = get_info(&h);
                ASSERT(info->duration_type == RESOURCE_DURATION_TIME);
                f64 resource_creation_time = epoch_time_to_clock_time(info->creation_epoch_time);
         
                if(resource_creation_time + info->duration_time_s > local_time)
                    remove(&h);
            }
        }
        
        if(cleanup_flags & _RESOURCE_CLEANUP_FRAME)
        {
            for(isize i = 0; i < frame_limited->size; i++)
            {
                Handle h = frame_limited->data[i];
                remove(&h);
            }
        }
        
        if(cleanup_flags & _RESOURCE_CLEANUP_TOTAL)
        {
            for(i32 i = 0; i < handle_table->slots.size; i++)
            {
                Handle_Table_Slot slot = handle_table->slots.data[i];
                void* ptr = slot.ptr;
                if(ptr == NULL)
                    continue;

                Handle h = {i, slot.generation};
                force_remove(&h);
            }
        }
    }

    //@TODO the rest
}

void resources_init(Resources* resources, Allocator* alloc)
{
    resources_deinit(resources);
    resources->alloc = alloc;

    handle_table_init(&resources->resources[RESOURCE_TYPE_SHAPE], alloc, sizeof(_Shape_Resource), DEF_ALIGN);
    handle_table_init(&resources->resources[RESOURCE_TYPE_IMAGE], alloc, sizeof(_Image_Resource), DEF_ALIGN);
    handle_table_init(&resources->resources[RESOURCE_TYPE_MAP], alloc, sizeof(_Map_Resource), DEF_ALIGN);
    handle_table_init(&resources->resources[RESOURCE_TYPE_CUBEMAP], alloc, sizeof(_Cubemap_Resource), DEF_ALIGN);
    handle_table_init(&resources->resources[RESOURCE_TYPE_MATERIAL], alloc, sizeof(_Material_Resource), DEF_ALIGN);
    handle_table_init(&resources->resources[RESOURCE_TYPE_TRIANGLE_MESH], alloc, sizeof(_Triangle_Mesh_Resource), DEF_ALIGN);
    handle_table_init(&resources->resources[RESOURCE_TYPE_SHADER], alloc, sizeof(_Shader_Resource), DEF_ALIGN);
}

void resources_cleanup_framed()
{
    _resources_cleanup(resources_get(), _RESOURCE_CLEANUP_FRAME);
}

void resources_cleanup_timed()
{
    _resources_cleanup(resources_get(), _RESOURCE_CLEANUP_TIME);
}

void resources_end_frame()
{
    _resources_cleanup(resources_get(), _RESOURCE_CLEANUP_TIME | _RESOURCE_CLEANUP_FRAME);
    resources_get()->frame_i += 1;
}

void resources_check_reloads()
{
    ASSERT_MSG(false, "@TODO");
}

void resources_deinit(Resources* resources)
{
    _resources_cleanup(resources, _RESOURCE_CLEANUP_TOTAL);
    
    handle_table_deinit(&resources->resources[RESOURCE_TYPE_SHAPE]);
    handle_table_deinit(&resources->resources[RESOURCE_TYPE_IMAGE]);
    handle_table_deinit(&resources->resources[RESOURCE_TYPE_MAP]);
    handle_table_deinit(&resources->resources[RESOURCE_TYPE_CUBEMAP]);
    handle_table_deinit(&resources->resources[RESOURCE_TYPE_MATERIAL]);
    handle_table_deinit(&resources->resources[RESOURCE_TYPE_TRIANGLE_MESH]);
    handle_table_deinit(&resources->resources[RESOURCE_TYPE_SHADER]);

    memset(resources, 0, sizeof *resources);
}

#endif