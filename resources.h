#ifndef LIB_RESOURCES
#define LIB_RESOURCES

#include "resource_descriptions.h"
#include "resource.h"
#include "name.h"
#include "lib/file.h"
#include "lib/serialize.h"


//A list of concatenated filenames. Each filename is followed by a NULL character.
//This makes it ideal for comunicating with io (NULL terminated strings) and reducing complexity
// (vs String_Builder_Array)
typedef struct Filename_List {
    String_Builder string;
} Filename_List;

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

DEFINE_ARRAY_TYPE(Id, Id_Array);

typedef struct Resources {
    Allocator* allocator;
    f64 check_time_every;
    i64 last_frame_etime;
    i64 last_check_etime;
    i64 frame_i;
    Clock clock;

    Resource_Manager resources[RESOURCE_TYPE_ENUM_COUNT];
} Resources;

typedef struct Shader {
    String_Builder vertex_shader_source;
    String_Builder fragment_shader_source;
    String_Builder geometry_shader_source;
} Shader;

typedef struct Map {
    Id image;
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
    //i32 next_i1;
    //i32 child_i1;
    //i32 depth;

    i32 triangles_from;
    i32 triangles_to;
} Triangle_Mesh_Group;

DEFINE_ARRAY_TYPE(Material, Material_Array);
DEFINE_ARRAY_TYPE(Triangle_Mesh_Group, Triangle_Mesh_Group_Array);
DEFINE_ARRAY_TYPE(Triangle_Mesh_Leaf_Group, Triangle_Mesh_Leaf_Group_Array);

typedef struct Triangle_Mesh
{
    Id material;
    Id shape;

    Triangle_Mesh_Group_Array groups;
    Id_Array materials;
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


#define RESOURCE_FUNCTION_DECL(Type_Name, TYPE_ENUM, name)       \
    EXPORT Type_Name*   name##_get(Id ptr); \
    EXPORT Type_Name*   name##_get_sure(Id id); \
    EXPORT Type_Name*   name##_get_with_info(Id ptr, Resource_Info** info); \
    EXPORT Id           name##_find_by_name(Hash_String name, isize* prev_found_and_finished_at); \
    EXPORT Id           name##_find_by_path(Hash_String path, isize* prev_found_and_finished_at); \
                                                                \
    EXPORT Id           name##_insert(Resource_Params params); \
    EXPORT bool         name##_remove(Id resource); \
    EXPORT bool         name##_force_remove(Id resource); \
                                                            \
    EXPORT Id           name##_make_shared(Id resource); \
    EXPORT Id           name##_make_unique(Id info, Resource_Params params); \
    EXPORT Id           name##_duplicate(Id info, Resource_Params params); \

RESOURCE_FUNCTION_DECL(Shape_Assembly,  RESOURCE_TYPE_SHAPE,            shape)
RESOURCE_FUNCTION_DECL(Image,   RESOURCE_TYPE_IMAGE,            image)
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

    #define RESOURCE_FUNCTION_DEF(Type_Name, TYPE_ENUM, type_name)       \
        EXPORT Type_Name* type_name##_get_with_info(Id id, Resource_Info** info) { \
            Resource_Ptr found = resource_get(resources_get_type(TYPE_ENUM), id); \
            if(info) *info = found.ptr; \
            ASSERT_MSG(resource_is_valid(found) == false || found.ptr->type_enum == TYPE_ENUM, "Inconsistent type!"); \
            return (Type_Name*) (found.ptr ? found.ptr->data : NULL); \
        } \
        EXPORT Type_Name*   type_name##_get(Id id) { \
            return type_name##_get_with_info(id, NULL); \
        } \
        EXPORT Type_Name* type_name##_get_sure(Id id) { \
            Type_Name* gotten = type_name##_get(id); \
            TEST_MSG(gotten != NULL, "Didnt find the resource with id %lli", (lli) id); \
            return gotten; \
        } \
        EXPORT Id type_name##_find_by_name(Hash_String name, isize* prev) { \
            return resource_get_by_name(resources_get_type(TYPE_ENUM), name, prev).id; \
        } \
        EXPORT Id type_name##_find_by_path(Hash_String path, isize* prev) { \
            return resource_get_by_path(resources_get_type(TYPE_ENUM), path, prev).id; \
        } \
        EXPORT Id type_name##_insert(Resource_Params params) { \
            return resource_insert(resources_get_type(TYPE_ENUM), params).id; \
        } \
        EXPORT bool type_name##_remove(Id resource) { \
            Resource_Ptr found = resource_get(resources_get_type(TYPE_ENUM), resource); \
            return resource_remove(resources_get_type(TYPE_ENUM), found);  \
        } \
        EXPORT bool type_name##_force_remove(Id resource) { \
            Resource_Ptr found = resource_get(resources_get_type(TYPE_ENUM), resource); \
            return resource_remove(resources_get_type(TYPE_ENUM), found); \
        } \
        EXPORT Id type_name##_make_shared(Id resource) { \
            Resource_Ptr found = resource_get(resources_get_type(TYPE_ENUM), resource); \
            return resource_share(resources_get_type(TYPE_ENUM), found).id; \
        } \
        EXPORT Id type_name##_make_unique(Id resource, Resource_Params params) { \
            Resource_Ptr found = resource_get(resources_get_type(TYPE_ENUM), resource); \
            return resource_make_unique(resources_get_type(TYPE_ENUM), found, params).id; \
        } \
        EXPORT Id type_name##_duplicate(Id resource, Resource_Params params) { \
            Resource_Ptr found = resource_get(resources_get_type(TYPE_ENUM), resource); \
            return resource_duplicate(resources_get_type(TYPE_ENUM), found, params).id; \
        } \
        
    RESOURCE_FUNCTION_DEF(Shape_Assembly,  RESOURCE_TYPE_SHAPE,            shape)
    RESOURCE_FUNCTION_DEF(Image,   RESOURCE_TYPE_IMAGE,            image)
    RESOURCE_FUNCTION_DEF(Map,             RESOURCE_TYPE_MAP,              map)
    RESOURCE_FUNCTION_DEF(Cubemap,         RESOURCE_TYPE_CUBEMAP,          cubemap)
    RESOURCE_FUNCTION_DEF(Material,        RESOURCE_TYPE_MATERIAL,         material)
    RESOURCE_FUNCTION_DEF(Triangle_Mesh,   RESOURCE_TYPE_TRIANGLE_MESH,    triangle_mesh)
    RESOURCE_FUNCTION_DEF(Shader,          RESOURCE_TYPE_SHADER,           shader)


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
    void _image_init(Image* out)
    {
        image_init(out, resources_allocator(), 1, PIXEL_TYPE_U8);
    }

    void _image_copy(Image* out, const Image* in)
    {
        image_assign(out, *in);
    }

    void _image_deinit(Image* out)
    {
        image_deinit(out);
    }
    
    //MAP
    void _map_init(Map* out)
    {
        map_info_init(&out->info, resources_allocator());
    }

    void _map_copy(Map* out, const Map* in)
    {
        out->info = in->info;
        out->image = image_make_shared(in->image);
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
        material_remove(item->material);

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
            out->materials.data[i] = material_make_shared(in->materials.data[i]);

        out->shape = shape_make_shared(in->shape);
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
    
    void resources_init(Resources* resources, Allocator* alloc)
    {
        resources_deinit(resources);
        resources->allocator = alloc;
    
        //#pragma warning(disable:4191)
        //@NOTE: safe function pointer cast here but visual studo complains
        #define RESOURCE_INIT(Type_Name, TYPE_ENUM, type_name)  \
            resource_manager_init(&resources->resources[TYPE_ENUM], alloc, sizeof(Type_Name), (Resource_Constructor) (void*) _##type_name##_init, (Resource_Destructor) (void*) _##type_name##_deinit, (Resource_Copy) (void*) _##type_name##_copy, #type_name, TYPE_ENUM);
        
        RESOURCE_INIT(Shape_Assembly,  RESOURCE_TYPE_SHAPE,            shape)
        RESOURCE_INIT(Image,   RESOURCE_TYPE_IMAGE,            image)
        RESOURCE_INIT(Map,             RESOURCE_TYPE_MAP,              map)
        RESOURCE_INIT(Cubemap,         RESOURCE_TYPE_CUBEMAP,          cubemap)
        RESOURCE_INIT(Material,        RESOURCE_TYPE_MATERIAL,         material)
        RESOURCE_INIT(Triangle_Mesh,   RESOURCE_TYPE_TRIANGLE_MESH,    triangle_mesh)
        RESOURCE_INIT(Shader,          RESOURCE_TYPE_SHADER,           shader)
        //#pragma warning(default:4191)
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
        i64 now = platform_epoch_time();
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
    
    EXPORT bool serialize_resource_info(Lpf_Dyn_Entry* entry, Resource_Info* info, Read_Or_Write action)
    {
        #if 0
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
        #endif

        const Serialize_Enum resource_type_enum[] = {
            SERIALIZE_ENUM_VALUE(RESOURCE_TYPE_SHAPE),
            SERIALIZE_ENUM_VALUE(RESOURCE_TYPE_IMAGE),
            SERIALIZE_ENUM_VALUE(RESOURCE_TYPE_MAP),
            SERIALIZE_ENUM_VALUE(RESOURCE_TYPE_CUBEMAP),
            SERIALIZE_ENUM_VALUE(RESOURCE_TYPE_MATERIAL),
            SERIALIZE_ENUM_VALUE(RESOURCE_TYPE_TRIANGLE_MESH),
            SERIALIZE_ENUM_VALUE(RESOURCE_TYPE_SHADER),
        };

        (void) resource_type_enum;

        const Serialize_Enum resource_lifetime_enum[] = {
            SERIALIZE_ENUM_VALUE(RESOURCE_LIFETIME_REFERENCED),
            SERIALIZE_ENUM_VALUE(RESOURCE_LIFETIME_PERSISTANT),
            SERIALIZE_ENUM_VALUE(RESOURCE_LIFETIME_TIMED),
            SERIALIZE_ENUM_VALUE(RESOURCE_LIFETIME_SINGLE_FRAME),
            SERIALIZE_ENUM_VALUE(RESOURCE_LIFETIME_EPHEMERAL),
        };
    
        const Serialize_Enum resource_reload_enum[] = {
            SERIALIZE_ENUM_VALUE(RESOURCE_RELOAD_ON_FILE_CHANGE),
            SERIALIZE_ENUM_VALUE(RESOURCE_RELOAD_ON_MEMORY_CHANGE),
            SERIALIZE_ENUM_VALUE(RESOURCE_RELOAD_NEVER),
        };

        bool state = true;
        state = state && serialize_id(serialize_locate(entry, "id", action),            &info->id, NULL, action);
        state = state && serialize_string(serialize_locate(entry, "name", action),      &info->name, STRING(""), action);
        serialize_string(serialize_locate(entry, "path", action),               &info->path, STRING(""), action);
        serialize_enum(serialize_locate(entry, "type_enum", action),      &info->type_enum, sizeof(info->type_enum), 0, "", resource_type_enum, STATIC_ARRAY_SIZE(resource_type_enum), action);

        state = state && serialize_enum(serialize_locate(entry, "lifetime", action),    &info->lifetime, sizeof(info->lifetime), 0, "", resource_lifetime_enum, STATIC_ARRAY_SIZE(resource_lifetime_enum), action);
        state = state && serialize_enum(serialize_locate(entry, "reload", action),      &info->reload, sizeof(info->reload), 0, "", resource_reload_enum, STATIC_ARRAY_SIZE(resource_reload_enum), action);
    
        if(state)
        {
            serialize_i64(serialize_locate(entry, "creation_etime", action),        &info->creation_etime, 0, action);
            serialize_i64(serialize_locate(entry, "modified_etime", action),        &info->modified_etime, 0, action);
            serialize_i64(serialize_locate(entry, "load_etime", action),            &info->load_etime, 0, action);
            serialize_i64(serialize_locate(entry, "file_modified_etime", action),   &info->file_modified_etime, 0, action);
        }
    
        if(state && action == SERIALIZE_WRITE)
            serialize_entry_set_identity(entry, STRING("Resource_Info"), STRING(""), LPF_KIND_SCOPE_START, LPF_FLAG_ALIGN_MEMBERS);

        return state;
    }

    EXPORT bool serialize_map_info(Lpf_Dyn_Entry* entry, Map_Info* info, Read_Or_Write action)
    {
        #if 0
        typedef enum Map_Scale_Filter {
            MAP_SCALE_FILTER_TRILINEAR = 0,
            MAP_SCALE_FILTER_BILINEAR,
            MAP_SCALE_FILTER_NEAREST,
        } Map_Scale_Filter;

        typedef enum Map_Repeat {
            MAP_REPEAT_REPEAT = 0,
            MAP_REPEAT_MIRRORED_REPEAT,
            MAP_REPEAT_CLAMP_TO_EDGE,
            MAP_REPEAT_CLAMP_TO_BORDER
        } Map_Repeat;

        #define MAX_CHANNELS 4
        typedef struct Map_Info {
            Vec3 offset;                
            Vec3 scale;
            Vec3 resolution;

            i32 channels_count; //the number of channels this texture should have. Is in range [0, MAX_CHANNELS] 
            i32 channels_idices1[MAX_CHANNELS]; //One based indeces into the image channels. 
            //If value is 0 then it is assumed to be equal to its index ie.:
            // channels_idices1 = {0, 0, 0, 0} ~~assumed~~> channels_idices1 = {1, 2, 3, 4}

            Map_Scale_Filter filter_minify;
            Map_Scale_Filter filter_magnify;
            Map_Repeat repeat_u;
            Map_Repeat repeat_v;
            Map_Repeat repeat_w;

            //linear_color = (contrast_minus_one + 1) * color^(1/gamma) + brigthness;
            //output_color = linear_color^screen_gamma
            f32 gamma;          //default 1
            f32 brigthness;     //default 0
            f32 contrast;       //default 0

            bool is_enabled; //whether or not to use this texture. Maybe [0, 1] float?
        } Map_Info;
        #endif

        const Serialize_Enum map_scale[] = {
            SERIALIZE_ENUM_VALUE(MAP_SCALE_FILTER_TRILINEAR),
            SERIALIZE_ENUM_VALUE(MAP_SCALE_FILTER_BILINEAR),
            SERIALIZE_ENUM_VALUE(MAP_SCALE_FILTER_NEAREST),
        };
    
        const Serialize_Enum map_repeat[] = {
            SERIALIZE_ENUM_VALUE(MAP_REPEAT_REPEAT),
            SERIALIZE_ENUM_VALUE(MAP_REPEAT_MIRRORED_REPEAT),
            SERIALIZE_ENUM_VALUE(MAP_REPEAT_CLAMP_TO_EDGE),
            SERIALIZE_ENUM_VALUE(MAP_REPEAT_CLAMP_TO_BORDER),
        };
        
        bool state = true;
        
        Map_Info default_values = {0};
        map_info_init(&default_values, allocator_get_default());

        serialize_vec3(serialize_locate(entry, "offset", action),       &info->offset, default_values.offset, action);
        serialize_vec3(serialize_locate(entry, "scale", action),        &info->scale, default_values.scale, action);
        serialize_vec3(serialize_locate(entry, "resolution", action),   &info->resolution, default_values.resolution, action);

        state = state && serialize_i32(serialize_locate(entry, "channels_count", action), &info->channels_count, default_values.channels_count, action);
        
        if(state)
        {
            STATIC_ASSERT(STATIC_ARRAY_SIZE(info->channels_idices1) == 4);
            serialize_int_count_typed(serialize_locate(entry, "channels_count", action), info->channels_idices1, sizeof(*info->channels_idices1), default_values.channels_idices1, 4, STRING("4i"), action);

            serialize_enum(serialize_locate(entry, "filter_minify", action), &info->filter_minify, sizeof(info->filter_minify), default_values.filter_minify, "Map_Scale_Filter", map_scale, STATIC_ARRAY_SIZE(map_scale), action);
            serialize_enum(serialize_locate(entry, "filter_magnify", action), &info->filter_magnify, sizeof(info->filter_magnify), default_values.filter_magnify, "Map_Scale_Filter", map_scale, STATIC_ARRAY_SIZE(map_scale), action);

            serialize_enum(serialize_locate(entry, "repeat_u", action),     &info->repeat_u, sizeof(info->repeat_u), default_values.repeat_u, "Map_Repeat", map_repeat, STATIC_ARRAY_SIZE(map_repeat), action);
            serialize_enum(serialize_locate(entry, "repeat_v", action),     &info->repeat_v, sizeof(info->repeat_v), default_values.repeat_v, "Map_Repeat", map_repeat, STATIC_ARRAY_SIZE(map_repeat), action);
            serialize_enum(serialize_locate(entry, "repeat_w", action),     &info->repeat_w, sizeof(info->repeat_w), default_values.repeat_w, "Map_Repeat", map_repeat, STATIC_ARRAY_SIZE(map_repeat), action);
        
            serialize_f32(serialize_locate(entry, "gamma", action),         &info->gamma, default_values.gamma, action);
            serialize_f32(serialize_locate(entry, "brigthness", action),    &info->brigthness, default_values.brigthness, action);
            serialize_f32(serialize_locate(entry, "contrast", action),      &info->contrast, default_values.contrast, action);
        }

        if(state && action == SERIALIZE_WRITE)
            serialize_entry_set_identity(entry, STRING("Map_Info"), STRING(""), LPF_KIND_SCOPE_START, LPF_FLAG_ALIGN_MEMBERS);

        return state;
    }
    
    EXPORT bool serialize_map_type(Lpf_Dyn_Entry* entry, Map_Type* type, Read_Or_Write action)
    {
        const Serialize_Enum map_type[] = {
            SERIALIZE_ENUM_VALUE(MAP_TYPE_ALBEDO),
            SERIALIZE_ENUM_VALUE(MAP_TYPE_ROUGNESS),
            SERIALIZE_ENUM_VALUE(MAP_TYPE_AMBIENT_OCCLUSION),
            SERIALIZE_ENUM_VALUE(MAP_TYPE_METALLIC),
    
            SERIALIZE_ENUM_VALUE(MAP_TYPE_AMBIENT),
            SERIALIZE_ENUM_VALUE(MAP_TYPE_DIFFUSE),
            SERIALIZE_ENUM_VALUE(MAP_TYPE_SPECULAR_COLOR),
            SERIALIZE_ENUM_VALUE(MAP_TYPE_SPECULAR_HIGHLIGHT),
    
            SERIALIZE_ENUM_VALUE(MAP_TYPE_ALPHA),
            SERIALIZE_ENUM_VALUE(MAP_TYPE_BUMP),
            SERIALIZE_ENUM_VALUE(MAP_TYPE_DISPLACEMENT),
            SERIALIZE_ENUM_VALUE(MAP_TYPE_STENCIL),
            SERIALIZE_ENUM_VALUE(MAP_TYPE_NORMAL),
            SERIALIZE_ENUM_VALUE(MAP_TYPE_REFLECTION),
            SERIALIZE_ENUM_VALUE(MAP_TYPE_EMMISIVE),
        };
    
        return serialize_enum(entry, type, sizeof(*type), 0, "Map_Type", map_type, STATIC_ARRAY_SIZE(map_type), action);
    }
    
    EXPORT bool serialize_cubemap_type(Lpf_Dyn_Entry* entry, Cubemap_Type* type, Read_Or_Write action)
    {
        const Serialize_Enum cubemap_type[] = {
            SERIALIZE_ENUM_VALUE(CUBEMAP_TYPE_SKYBOX),
            SERIALIZE_ENUM_VALUE(CUBEMAP_TYPE_ENVIRONMENT),
            SERIALIZE_ENUM_VALUE(CUBEMAP_TYPE_IRRADIANCE),
            SERIALIZE_ENUM_VALUE(CUBEMAP_TYPE_PREFILTER),
            SERIALIZE_ENUM_VALUE(CUBEMAP_TYPE_REFLECTION),

            SERIALIZE_ENUM_VALUE(CUBEMAP_TYPE_ENUM_COUNT),
        };
    
        return serialize_enum(entry, type, sizeof(*type), 0, "Cubemap_Type", cubemap_type, STATIC_ARRAY_SIZE(cubemap_type), action);
    }
    
    EXPORT bool serialize_material_info(Lpf_Dyn_Entry* entry, Material_Info* info, Read_Or_Write action)
    {
        #if 0
        typedef struct Material_Info {
            Vec3 ambient_color;                     
            Vec3 diffuse_color;                     
            Vec3 specular_color;                    
            f32 specular_exponent;    
    
            Vec3 albedo;
            Vec3 emissive;
            f32 roughness;
            f32 metallic;
            f32 ambient_occlusion;
            f32 opacity;

            Vec3 reflection_at_zero_incidence;

            f32 emissive_power;
            f32 emissive_power_map;
            f32 bump_multiplier;
        } Material_Info;
        #endif
        
        Material_Info default_values = {0};
        default_values.emissive_power = 1;
        default_values.emissive_power_map = 1;
        default_values.bump_multiplier = 1;
        default_values.reflection_at_zero_incidence = vec3_of(0.04f);

        bool phong_state = true;
        phong_state = phong_state && serialize_vec3(serialize_locate(entry, "ambient_color", action),       &info->ambient_color, default_values.ambient_color, action);
        phong_state = phong_state && serialize_vec3(serialize_locate(entry, "diffuse_color", action),       &info->diffuse_color, default_values.diffuse_color, action);
        phong_state = phong_state && serialize_vec3(serialize_locate(entry, "specular_color", action),      &info->specular_color, default_values.specular_color, action);
        phong_state = phong_state && serialize_vec3(serialize_locate(entry, "ambient_color", action),       &info->ambient_color, default_values.ambient_color, action);
        phong_state = phong_state && serialize_f32(serialize_locate(entry, "specular_exponent", action),    &info->specular_exponent, default_values.specular_exponent, action);
        
        bool pbr_state = true;
        pbr_state = pbr_state && serialize_vec3(serialize_locate(entry, "albedo", action),                  &info->albedo, default_values.albedo, action);
        pbr_state = pbr_state && serialize_vec3(serialize_locate(entry, "emissive", action),                &info->emissive, default_values.emissive, action);
        pbr_state = pbr_state && serialize_f32(serialize_locate(entry, "roughness", action),                &info->roughness, default_values.roughness, action);
        pbr_state = pbr_state && serialize_f32(serialize_locate(entry, "metallic", action),                 &info->metallic, default_values.metallic, action);
        pbr_state = pbr_state && serialize_f32(serialize_locate(entry, "ambient_occlusion", action),        &info->ambient_occlusion, default_values.ambient_occlusion, action);
        pbr_state = pbr_state && serialize_f32(serialize_locate(entry, "opacity", action),                  &info->opacity, default_values.opacity, action);

        serialize_vec3(serialize_locate(entry, "reflection_at_zero_incidence", action),                     &info->reflection_at_zero_incidence, default_values.reflection_at_zero_incidence, action);
        serialize_f32(serialize_locate(entry, "emissive_power", action),                                    &info->emissive_power, default_values.emissive_power, action);
        serialize_f32(serialize_locate(entry, "emissive_power_map", action),                                &info->emissive_power_map, default_values.emissive_power_map, action);
        serialize_f32(serialize_locate(entry, "bump_multiplier", action),                                   &info->bump_multiplier, default_values.bump_multiplier, action);
        
        if((phong_state || pbr_state) && action == SERIALIZE_WRITE)
            serialize_entry_set_identity(entry, STRING("Material_Info"), STRING(""), LPF_KIND_SCOPE_START, LPF_FLAG_ALIGN_MEMBERS);

        return phong_state || pbr_state;
    }

    
    EXPORT bool serialize_image(Lpf_Dyn_Entry* entry, Image* image, Read_Or_Write action)
    {
        const Serialize_Enum type[] = {
            SERIALIZE_ENUM_VALUE(PIXEL_TYPE_U8),
            SERIALIZE_ENUM_VALUE(PIXEL_TYPE_U16),
            SERIALIZE_ENUM_VALUE(PIXEL_TYPE_U24),
            SERIALIZE_ENUM_VALUE(PIXEL_TYPE_U32),
            SERIALIZE_ENUM_VALUE(PIXEL_TYPE_F32),
        };

        bool state = true;
        state = state && serialize_i32(serialize_locate(entry, "width", action),         &image->width, 0, action);
        state = state && serialize_i32(serialize_locate(entry, "height", action),        &image->height, 0, action);
        state = state && serialize_i32(serialize_locate(entry, "height", action),        &image->height, 0, action);

        bool pixel_format_state = serialize_enum(serialize_locate(entry, "type", action), &image->type, sizeof(image->type), PIXEL_TYPE_U8, "Pixel_Type", type, STATIC_ARRAY_SIZE(type), action);
        if(pixel_format_state == false)
            state = serialize_i32(serialize_locate(entry, "type", action), (i32*) &image->type, PIXEL_TYPE_U8, action);
        else
            state = pixel_format_state;

        return state;
    }

#endif