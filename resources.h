#ifndef LIB_RESOURCES
#define LIB_RESOURCES

#include "resource_descriptions.h"
#include "resource.h"
#include "file.h"

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

typedef struct Resources {
    Allocator* allocator;
    f64 check_time_every;
    i64 last_frame_etime;
    i64 last_check_etime;
    i64 frame_i;
    Clock clock;
    
    Resource_Manager resources[RESOURCE_TYPE_ENUM_COUNT];
} Resources;

#define RESOURCE_NAME_SIZE 64

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


typedef enum Shader_Pass {
    SHADER_PASS_VERTEX,
    SHADER_PASS_GEOMETRY,
    SHADER_PASS_FRAGMENT,
    
    SHADER_PASS_ENUM_COUNT,
} Shader_Pass;

typedef struct Shader {
    String_Builder sources[SHADER_PASS_ENUM_COUNT];
} Shader;

typedef struct Map {
    Resource_Ptr image;
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
    Resource_Ptr shape;

    Triangle_Mesh_Group_Array groups;
    Resource_Ptr_Array materials;
} Triangle_Mesh;

Resources* resources_get();
Resources* resources_set(Resources* new_resources_ptr);
Allocator* resources_allocator();

void resources_init(Resources* resources, Allocator* alloc);
void resources_deinit(Resources* resources);
void resources_frame_cleanup();
void resources_time_cleanup();
void resources_end_frame();
void resources_check_reloads();

String               string_from_name(const Name* name);
void                 name_from_string(Name* name, String string);

#define RESOURCE_FUNCTION_DECL(Type_Name, TYPE_ENUM, name)       \
    EXPORT Type_Name*   name##_get(Resource_Ptr ptr); \
    EXPORT Resource_Ptr name##_find(Id id); \
    EXPORT Resource_Ptr name##_find_by_name(Hash_String name, isize* prev_found_and_finished_at); \
    EXPORT Resource_Ptr name##_find_by_path(Hash_String path, isize* prev_found_and_finished_at); \
                                                                \
    EXPORT Resource_Ptr name##_insert(String name, String path); \
    EXPORT Resource_Ptr name##_insert_custom(Id id, String name, String path, Resource_Lifetime lifetime, i64 death_time); \
    EXPORT bool         name##_remove(Resource_Ptr resource); \
    EXPORT bool         name##_force_remove(Resource_Ptr resource); \
                                                            \
    EXPORT Resource_Ptr name##_share(Resource_Ptr resource); \
    EXPORT Resource_Ptr name##_make_unique(Resource_Ptr info); \
    EXPORT Resource_Ptr name##_duplicate(Resource_Ptr info); \

RESOURCE_FUNCTION_DECL(Shape_Assembly,  RESOURCE_TYPE_SHAPE,            shape)
RESOURCE_FUNCTION_DECL(Image_Builder,   RESOURCE_TYPE_IMAGE,            image)
RESOURCE_FUNCTION_DECL(Map,             RESOURCE_TYPE_MAP,              map)
RESOURCE_FUNCTION_DECL(Cubemap,         RESOURCE_TYPE_CUBEMAP,          cubemap)
RESOURCE_FUNCTION_DECL(Material,        RESOURCE_TYPE_MATERIAL,         material)
RESOURCE_FUNCTION_DECL(Triangle_Mesh,   RESOURCE_TYPE_TRIANGLE_MESH,    triangle_mesh)
RESOURCE_FUNCTION_DECL(Shader,          RESOURCE_TYPE_SHADER,           shader)
#endif


#if (defined(LIB_ALL_IMPL) || defined(LIB_RESOURCES_IMPL)) && !defined(LIB_RESOURCES_HAS_IMPL)
#define LIB_RESOURCES_HAS_IMPL

    Resources* _global_resources = NULL;
    Resources* resources_get()
    {
        return _global_resources;
    }
    Resource_Manager* resources_get_type(Resource_Type type)
    {
        return &_global_resources->resources[type];
    }
    
    #define RESOURCE_FUNCTION_DEF(Type_Name, TYPE_ENUM, type_name)       \
        EXPORT Type_Name*   name##_get(Resource_Ptr ptr) { \
            ASSERT(resource_is_valid(ptr) && ptr.ptr->type_enum == TYPE_ENUM); \
            return (Type_Name*) ptr.ptr->data; \
        } \
        EXPORT Resource_Ptr type_name##_find(Id id) { \
            return resource_find(resources_get_type(TYPE_ENUM), id); \
        } \
        EXPORT Resource_Ptr type_name##_find_by_name(Hash_String name, isize* prev) { \
            return resource_find_by_name(resources_get_type(TYPE_ENUM), name, prev); \
        } \
        EXPORT Resource_Ptr type_name##_find_by_path(Hash_String path, isize* prev) { \
            return resource_find_by_path(resources_get_type(TYPE_ENUM), path, prev); \
        } \
        EXPORT Resource_Ptr type_name##_insert(String name, String path) { \
            return resource_insert(resources_get_type(TYPE_ENUM), id_generate(), name, path); \
        } \
        EXPORT Resource_Ptr type_name##_insert_custom(Id id, String name, String path, Resource_Lifetime lifetime, i64 death_time) { \
            return resource_insert_custom(resources_get_type(TYPE_ENUM), id, name, path, lifetime, death_time); \
        } \
        EXPORT bool         type_name##_remove(Resource_Ptr resource) { \
            return resource_remove(resources_get_type(TYPE_ENUM), resource); \
        } \
        EXPORT bool         type_name##_force_remove(Resource_Ptr resource) { \
            return resource_force_remove(resources_get_type(TYPE_ENUM), resource); \
        } \
        EXPORT Resource_Ptr type_name##_share(Resource_Ptr resource) { \
            return resource_share(resources_get_type(TYPE_ENUM), resource); \
        } \
        EXPORT Resource_Ptr type_name##_make_unique(Resource_Ptr resource) { \
            return resource_make_unique(resources_get_type(TYPE_ENUM), resource); \
        } \
        EXPORT Resource_Ptr type_name##_duplicate(Resource_Ptr resource) { \
            return resource_duplicate(resources_get_type(TYPE_ENUM), resource); \
        } \
        
    RESOURCE_FUNCTION_DEF(Shape_Assembly,  RESOURCE_TYPE_SHAPE,            shape)
    RESOURCE_FUNCTION_DEF(Image_Builder,   RESOURCE_TYPE_IMAGE,            image)
    RESOURCE_FUNCTION_DEF(Map,             RESOURCE_TYPE_MAP,              map)
    RESOURCE_FUNCTION_DEF(Cubemap,         RESOURCE_TYPE_CUBEMAP,          cubemap)
    RESOURCE_FUNCTION_DEF(Material,        RESOURCE_TYPE_MATERIAL,         material)
    RESOURCE_FUNCTION_DEF(Triangle_Mesh,   RESOURCE_TYPE_TRIANGLE_MESH,    triangle_mesh)
    RESOURCE_FUNCTION_DEF(Shader,          RESOURCE_TYPE_SHADER,           shader)

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
            return _global_resources->allocator;
        else
            return NULL;
    }
    
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
    }

    void _map_copy(Map* out, const Map* in)
    {
        out->info = in->info;
        if(resource_is_valid(in->image))
            out->image = image_share(in->image);
    }

    void _map_deinit(Map* out)
    {
        image_remove(out->image);
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
        shape_remove(item->shape);

        for(isize i = 0; i < item->materials.size; i++)
            material_remove(item->materials.data[i]);
        
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
            out->materials.data[i] = material_share(in->materials.data[i]);

        out->shape = shape_share(in->shape);
    }

    //SHADER
    void _shader_init(Shader* out)
    {
        for(isize i = 0; i < SHADER_PASS_ENUM_COUNT; i++)
            array_init(&out->sources[i], resources_allocator());
    }

    void _shader_copy(Shader* out, const Shader* in)
    {
        for(isize i = 0; i < SHADER_PASS_ENUM_COUNT; i++)
            array_copy(&out->sources[i], in->sources[i]);
    }

    void _shader_deinit(Shader* out)
    {
        for(isize i = 0; i < SHADER_PASS_ENUM_COUNT; i++)
            array_deinit(&out->sources[i]);
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

void resources_init(Resources* resources, Allocator* alloc)
{
    resources_deinit(resources);
    resources->allocator = alloc;

    #define RESOURCE_INIT(Type_Name, TYPE_ENUM, type_name)  \
        resource_manager_init(&resources->resources[TYPE_ENUM], alloc, sizeof(Type_Name), (Resource_Constructor) _##type_name##_init, (Resource_Destructor) _##type_name##_deinit, (Resource_Copy) _##type_name##_copy, resources, #type_name, TYPE_ENUM);
        
    RESOURCE_INIT(Shape_Assembly,  RESOURCE_TYPE_SHAPE,            shape)
    RESOURCE_INIT(Image_Builder,   RESOURCE_TYPE_IMAGE,            image)
    RESOURCE_INIT(Map,             RESOURCE_TYPE_MAP,              map)
    RESOURCE_INIT(Cubemap,         RESOURCE_TYPE_CUBEMAP,          cubemap)
    RESOURCE_INIT(Material,        RESOURCE_TYPE_MATERIAL,         material)
    RESOURCE_INIT(Triangle_Mesh,   RESOURCE_TYPE_TRIANGLE_MESH,    triangle_mesh)
    RESOURCE_INIT(Shader,          RESOURCE_TYPE_SHADER,           shader)
}

void resources_frame_cleanup()
{
    for(isize i = 0; i < RESOURCE_TYPE_ENUM_COUNT; i++)
        resource_manager_frame_cleanup(&resources_get()->resources[i]);
}

void resources_time_cleanup()
{
    for(isize i = 0; i < RESOURCE_TYPE_ENUM_COUNT; i++)
        resource_manager_time_cleanup(&resources_get()->resources[i]);
}

void resources_end_frame()
{
    Resources* resources = resources_get();
    i64 now = platform_universal_epoch_time();
    i64 period = (i64) (resources->check_time_every * 10000);
    if(resources->last_check_etime + period < now)
    {
        resources->last_check_etime = now;
        resources_time_cleanup();
    }

    resources_frame_cleanup();
    resources->last_frame_etime = now;
    resources->frame_i += 1;
}

void resources_check_reloads()
{
    ASSERT_MSG(false, "@TODO");
}

void resources_deinit(Resources* resources)
{
    for(isize i = 0; i < RESOURCE_TYPE_ENUM_COUNT; i++)
        resource_manager_deinit(&resources->resources[i]);

    memset(resources, 0, sizeof *resources);
}

#endif