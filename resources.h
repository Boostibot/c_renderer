#ifndef LIB_RESOURCES
#define LIB_RESOURCES

#include "shapes.h"
#include "string.h"
#include "image.h"
#include "handle_table.h"

//@TODO: cubemap info!

typedef enum Map_Scale_Filter {
    MAP_SCALE_FILTER_NEAREST,
    MAP_SCALE_FILTER_BILINEAR,
    MAP_SCALE_FILTER_TRILINEAR,
} Map_Scale_Filter;

typedef enum Map_Repeat {
    MAP_REPEAT_REPEAT,
    MAP_REPEAT_MIRRORED_REPEAT,
    MAP_REPEAT_CLAMP_TO_EDGE,
    MAP_REPEAT_CLAMP_TO_BORDER
} Map_Repeat;

#define MAX_CHANNELS 4
typedef struct Map_Info {
    Vec2 offset;                
    Vec2 scale;
    Vec2 resolution;

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

// ========================= Descriptions =========================
// These should perfectly describe the thing they are substituting but have
// a path to the data instead of the data itself.
//
// At the moment descriptions are not part of Resources. This will likely change in the future
// once we have a proper concept of asset.

typedef struct Map_Description {
    String_Builder path;
    Map_Info info;
} Map_Description;

typedef union Cubemap_Description {
    struct {
        Map_Description top;   
        Map_Description bottom;
        Map_Description front; 
        Map_Description back;  
        Map_Description left;  
        Map_Description right; 
    };
    Map_Description faces[6];
} Cubemap_Description;


typedef enum Map_Type{
    MAP_TYPE_ALBEDO,
    MAP_TYPE_ROUGNESS,
    MAP_TYPE_AMBIENT_OCCLUSION,
    MAP_TYPE_METALLIC,
    
    MAP_TYPE_AMBIENT,
    MAP_TYPE_DIFFUSE,
    MAP_TYPE_SPECULAR_COLOR,
    MAP_TYPE_SPECULAR_HIGHLIGHT,
    
    MAP_TYPE_ALPHA,
    MAP_TYPE_BUMP,
    MAP_TYPE_DISPLACEMENT,
    MAP_TYPE_STENCIL,
    MAP_TYPE_NORMAL,
    MAP_TYPE_REFLECTION,
    MAP_TYPE_EMMISIVE,

    MAP_TYPE_ENUM_COUNT,
} Map_Type;

typedef enum Cubemap_Type{
    CUBEMAP_TYPE_SKYBOX,
    CUBEMAP_TYPE_ENVIRONMENT,
    CUBEMAP_TYPE_IRRADIANCE,
    CUBEMAP_TYPE_PREFILTER,
    CUBEMAP_TYPE_REFLECTION,

    CUBEMAP_TYPE_ENUM_COUNT,
} Cubemap_Type;

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

typedef struct Material_Description {
    String_Builder name;
    String_Builder path;
    
    Material_Info info;

    Map_Description maps[MAP_TYPE_ENUM_COUNT];
    Cubemap_Description cubemaps[CUBEMAP_TYPE_ENUM_COUNT];
} Material_Description;

typedef struct Object_Group_Description {
    String_Builder name;
    String_Builder material_name;
    String_Builder material_path;

    i32 next_i1;
    i32 child_i1;
    i32 depth;

    i32 triangles_from;
    i32 triangles_to;
} Object_Group_Description;

DEFINE_ARRAY_TYPE(Material_Description, Material_Description_Array);
DEFINE_ARRAY_TYPE(Object_Group_Description, Object_Group_Description_Array);

typedef struct Object_Description {
    String_Builder name;
    String_Builder path;
    Object_Group_Description_Array groups;
    String_Builder_Array material_files;
} Object_Description;

void map_info_init(Map_Info* description, Allocator* alloc);
void map_info_deinit(Map_Info* description);

void map_description_init(Map_Description* description, Allocator* alloc);
void map_description_deinit(Map_Description* description);

void cubemap_description_init(Cubemap_Description* description, Allocator* alloc);
void cubemap_description_deinit(Cubemap_Description* description);

void material_description_init(Material_Description* description, Allocator* alloc);
void material_description_deinit(Material_Description* description);

void object_group_description_init(Object_Group_Description* description, Allocator* alloc);
void object_group_description_deinit(Object_Group_Description* description);

void object_description_init(Object_Description* description, Allocator* alloc);
void object_description_deinit(Object_Description* description);

//========================= Engine types =========================
// These represent things the engine recognizes. 
// It uses handles to more complex things instead of the things iteself to enable sharing.

#define NAME_SIZE 256
#define RESOURCE_CALLSTACK_SIZE 8

typedef struct Resource_Callstack {
    void* stack_frames[RESOURCE_CALLSTACK_SIZE];
} Resource_Callstack;

typedef struct Uid {
    i64 epoch_time;
    i64 microsecond_index;
} Uid;

typedef struct Name {
    char data[NAME_SIZE];
    isize size;
} Name;

typedef enum Resource_Duration {
    RESOURCE_DURATION_REFERENCED = 0,   //use and clean when reference count reaches 0
    RESOURCE_DURATION_TIME,             //use and clean when time is up
    RESOURCE_DURATION_SINGLE_FRAME,     //use and clean automatically when this frame ends
    RESOURCE_DURATION_PERSISTANT,       //use and clean when explicitly demanded
} Resource_Duration;

typedef enum Resource_Reload {
    RESOURCE_RELOAD_ON_FILE_CHANGE = 0,
    RESOURCE_RELOAD_ON_MEMORY_CHANGE = 1,
    RESOURCE_RELOAD_NEVER = 2,
} Resource_Reload;

typedef struct Resource_Params {
    String path;
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

DEFINE_HANDLE_TYPES(Loaded_Image);
DEFINE_HANDLE_TYPES(Map);
DEFINE_HANDLE_TYPES(Cubemap);
DEFINE_HANDLE_TYPES(Shape_Assembly);
DEFINE_HANDLE_TYPES(Material);
DEFINE_HANDLE_TYPES(Object);

//@TODO: Add perf counters everywhere

//@TODO make proper clocks!


typedef struct Resources
{
    Allocator* alloc;

    Handle_Table images;
    Handle_Table shapes;
    Handle_Table materials;
    Handle_Table objects;
} Resources;

typedef struct Map {
    Name name;
    Loaded_Image_Handle image;
    Map_Info info;
} Map;

typedef struct Cubemap 
{    
    Name name;
    union {
        struct {
            Loaded_Image_Handle top;   
            Loaded_Image_Handle bottom;
            Loaded_Image_Handle front; 
            Loaded_Image_Handle back;  
            Loaded_Image_Handle left;  
            Loaded_Image_Handle right;
        };

        Loaded_Image_Handle faces[6];
    } images;
} Cubemap;

typedef struct Material {
    Name name;
    Material_Info info;

    Map_Handle maps[MAP_TYPE_ENUM_COUNT];
    Cubemap_Handle cubemaps[CUBEMAP_TYPE_ENUM_COUNT];
} Material;

DEFINE_ARRAY_TYPE(Material, Material_Array);

typedef struct Object_Leaf_Group
{
    Name name;
    Material_Handle material;

    i32 triangles_from;
    i32 triangles_to;
} Object_Leaf_Group;

typedef struct Object_Group
{
    Name name;
    Material_Handle material;

    i32 next_i1;
    i32 child_i1;
    i32 depth;

    i32 triangles_from;
    i32 triangles_to;
} Object_Group;

DEFINE_ARRAY_TYPE(Object_Group, Object_Group_Array);

typedef struct Object
{
    Material_Handle fallback_material;
    Shape_Assembly_Handle shape;

    Object_Group_Array groups;
} Object;
    
typedef struct Loaded_Image {
    Name name;
    Image_Builder image;
} Loaded_Image;

#define DEFINE_RESOURCES_RESOURCE_DECL(Type, name, member_name)                         \
    Type##_Handle resources_##name##_add(Resources* resources, Type* item, Resource_Params info);\
    void resources_##name##_remove(Resources* resources, Type##_Handle handle);         \
    void resources_##name##_force_remove(Resources* resources, Type##_Handle handle);   \
    Type##_Handle resources_##name##_share(Resources* resources, Type##_Handle handle); \
    Type##_Handle resources_##name##_make_unique(Resources* resources, Type##_Ref handle); \
    Type* resources_##name##_get(Resources* resources, Type##_Handle handle);           \
    Resource_Info resources_##name##_get_info(Type##_Ref handle);                       \
    String resources_##name##_get_path(Type##_Ref handle);                              \
    Type##_Ref resources_##name##_find_by_path(String path);                            \
    Type##_Ref resources_##name##_find_by_name(String name);                            \
    
DEFINE_RESOURCES_RESOURCE_DECL(Loaded_Image, loaded_image, images)
DEFINE_RESOURCES_RESOURCE_DECL(Shape_Assembly, shape, shapes)
DEFINE_RESOURCES_RESOURCE_DECL(Material, material, materials)
DEFINE_RESOURCES_RESOURCE_DECL(Material, material_phong, materials_phong)
DEFINE_RESOURCES_RESOURCE_DECL(Object, object, objects)

void resources_init(Resources* resources, Allocator* alloc);
void resources_deinit(Resources* resources);
void resources_end_frame(Resources* resources);
void resources_check_reloads(Resources* resources);
void resources_update(Resources* resources);

void object_init(Object* item, Resources* resources);
void object_deinit(Object* item, Resources* resources);

#endif


#if (defined(LIB_ALL_IMPL) || defined(LIB_RESOURCES_IMPL)) && !defined(LIB_RESOURCES_HAS_IMPL)
#define LIB_RESOURCES_HAS_IMPL

void map_info_init(Map_Info* description, Allocator* alloc)
{
    //Nothing so far but we might add dynamic behaviour later
    (void) description;
    (void) alloc;
}
void map_info_deinit(Map_Info* description)
{
    //Nothing so far but we might add dynamic behaviour later
    (void) description;
}

void map_description_init(Map_Description* description, Allocator* alloc)
{
    map_description_deinit(description);
    map_info_init(&description->info, alloc);
    array_init(&description->path, alloc);
}
void map_description_deinit(Map_Description* description)
{
    array_deinit(&description->path);
    map_info_deinit(&description->info);
    memset(description, 0, sizeof *description);
}

void cubemap_description_init(Cubemap_Description* description, Allocator* alloc)
{
    for(isize i = 0; i < 6; i++)
        map_description_init(&description->faces[i], alloc);
}
void cubemap_description_deinit(Cubemap_Description* description)
{
    for(isize i = 0; i < 6; i++)
        map_description_deinit(&description->faces[i]);
}

void material_description_init(Material_Description* description, Allocator* alloc)
{
    material_description_deinit(description);

    array_init(&description->path, alloc);
    array_init(&description->name, alloc);

    for(isize i = 0; i < MAP_TYPE_ENUM_COUNT; i++)
        map_description_init(&description->maps[i], alloc);  
        
    for(isize i = 0; i < CUBEMAP_TYPE_ENUM_COUNT; i++)
        cubemap_description_init(&description->cubemaps[i], alloc);  

    memset(description, 0, sizeof *description);
}

void material_description_deinit(Material_Description* description)
{
    array_deinit(&description->path);
    array_deinit(&description->name);
    
    for(isize i = 0; i < MAP_TYPE_ENUM_COUNT; i++)
        map_description_deinit(&description->maps[i]);  
        
    for(isize i = 0; i < CUBEMAP_TYPE_ENUM_COUNT; i++)
        cubemap_description_deinit(&description->cubemaps[i]);  


    memset(description, 0, sizeof *description);
}

void object_group_description_init(Object_Group_Description* description, Allocator* alloc)
{
    object_group_description_deinit(description);

    array_init(&description->name, alloc);
    array_init(&description->material_name, alloc);
    array_init(&description->material_path, alloc);
}
void object_group_description_deinit(Object_Group_Description* description)
{
    array_deinit(&description->name);
    array_deinit(&description->material_name);
    array_deinit(&description->material_path);

    memset(description, 0, sizeof *description);
}

void object_description_init(Object_Description* description, Allocator* alloc)
{
    object_description_deinit(description);

    array_init(&description->name, alloc);
    array_init(&description->path, alloc);
    array_init(&description->groups, alloc);
    array_init(&description->material_files, alloc);
}
void object_description_deinit(Object_Description* description)
{
    array_deinit(&description->name);
    array_deinit(&description->path);

    for(isize i = 0; i < description->groups.size; i++)
        object_group_description_deinit(&description->groups.data[i]);

    array_deinit(&description->groups);
    builder_array_deinit(&description->material_files);

    memset(description, 0, sizeof *description);
}


#if 0
    Type##_Handle resources_##name##_add(Resources* resources, Type* item, Resource_Params info);\
    void resources_##name##_remove(Resources* resources, Type##_Handle handle);         \
    void resources_##name##_force_remove(Resources* resources, Type##_Handle handle);   \
    Type##_Handle resources_##name##_share(Resources* resources, Type##_Handle handle); \
    Type##_Handle resources_##name##_make_unique(Resources* resources, Type##_Ref handle); \
    Type* resources_##name##_get(Resources* resources, Type##_Handle handle);           \
    Resource_Info resources_##name##_get_info(Type##_Ref handle);                       \
    String resources_##name##_get_path(Type##_Ref handle);                              \
    Type##_Ref resources_##name##_find_by_path(String path);                            
#endif
    typedef struct {                                                                    \
        Loaded_Image data;                                                                      \
        Resource_Info info;                                                             \
        String_Builder full_path;                                                       \
    } _Resources_Internal_Loaded_Image;                                                 \

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

    Resource_Info resource_info_make(Resource_Params params)
    {   
        Resource_Info info = {0};
        info.creation_epoch_time = platform_universal_epoch_time();
        info.duration_time_s = params.duration_time_s;
        info.duration_type = params.duration_type;
        info.reload_policy = params.reload_policy;
        info.reference_count = 1;

        return info;
    }                          
    
    Loaded_Image_Ref resources_image_find_by_path(Resources* resources, String full_path)
    {   
        Loaded_Image_Ref ref = {0};

        HANDLE_TABLE_FOR_EACH_BEGIN(resources->images, Loaded_Image_Ref, found_ref, _Resources_Internal_Loaded_Image*, _loaded_image)
            if(string_is_equal(string_from_builder(_loaded_image->full_path), full_path))
            {
                ref = found_ref;
                break;
            }
        HANDLE_TABLE_FOR_EACH_END

        return ref;
    }
    
    Loaded_Image* resources_image_get(Resources* resources, Loaded_Image_Handle handle)
    {   
        _Resources_Internal_Loaded_Image* item = (_Resources_Internal_Loaded_Image*) handle_table_get(resources->images, (Handle*) &handle);
        if(item)
            return &item->data;
        else
            return NULL;
    }
    
    Resource_Info resources_image_get_info(Resources* resources, Loaded_Image_Ref handle)
    {
        Resource_Info info = {0};
        _Resources_Internal_Loaded_Image* item = (_Resources_Internal_Loaded_Image*) handle_table_get(resources->images, (Handle*) &handle);
        if(item)
            info = item->info;

        return info;
    }
    
    String resources_image_get_path(Resources* resources, Loaded_Image_Ref handle)
    {
        String path = {0};
        _Resources_Internal_Loaded_Image* item = (_Resources_Internal_Loaded_Image*) handle_table_get(resources->images, (Handle*) &handle);
        if(item)
            path = string_from_builder(item->full_path);

        return path;
    }

    Loaded_Image_Handle _resources_image_add(Resources* resources, Loaded_Image* item, String path, Resource_Info info)
    {
        String emphemeral_full_path = file_get_ephemeral_full_path(path);
        Loaded_Image_Ref ref = resources_image_find_by_path(resources, emphemeral_full_path);
        
        if(HANDLE_IS_NULL(ref) == false)
        {
            _Resources_Internal_Loaded_Image* prev = (_Resources_Internal_Loaded_Image*) handle_table_get(resources->images, (Handle*) &ref);
            LOG_ERROR("RESOURCES", "added duplicit resource named %s on path " STRING_FMT " previous named %s", item->name, STRING_PRINT(emphemeral_full_path), prev->data.name);
        }

        Loaded_Image_Handle obtained = {0};
        _Resources_Internal_Loaded_Image* created = (_Resources_Internal_Loaded_Image*) handle_table_add(&resources->images, (Handle*) &obtained);

        created->info = info;
        created->full_path = builder_from_string(emphemeral_full_path, resources->alloc);

        //if uses the internal allocator -> steal
        //else -> copy
        if(item->image.allocator == resources->alloc)
            SWAP(&created->data, item, Loaded_Image);
        else
        {
            image_builder_init_from_image(&created->data.image, resources->alloc, image_from_builder(item->image));
            memcpy(&created->data.name, &item->name, sizeof item->name);
        }

        return obtained;
    }

    bool resources_image_remove(Resources* resources, Loaded_Image_Handle handle)
    {
        _Resources_Internal_Loaded_Image* prev = (_Resources_Internal_Loaded_Image*) handle_table_get(resources->images, (Handle*) &handle);
        if(prev && prev->info.reference_count-- <= 0)
        {
            prev->info.reference_count = 0;
            if(prev->info.duration_type != RESOURCE_DURATION_PERSISTANT)
                resources_image_force_remove(resources, handle);
        }

        return prev != NULL;
    }

    bool resources_image_force_remove(Resources* resources, Loaded_Image_Handle handle)
    {
        _Resources_Internal_Loaded_Image* prev = (_Resources_Internal_Loaded_Image*) handle_table_get(resources->images, (Handle*) &handle);
        if(prev)
        {
            loaded_image_deinit(&prev->data);
            handle_table_remove(&resources->images, (Handle*) &handle);
        }

        return prev != NULL;
    }

    Loaded_Image_Handle resources_image_make_shared(Resources* resources, Loaded_Image_Handle handle)
    {
        Loaded_Image_Handle out = {0};
        _Resources_Internal_Loaded_Image* item = (_Resources_Internal_Loaded_Image*) handle_table_get(resources->images, (Handle*) &handle);
        if(item)
        {
            item->info.reference_count += 1;
            out = handle;
        }

        return out;
    }

    Loaded_Image_Handle resources_image_make_unique(Resources* resources, Loaded_Image_Ref handle)
    {
        Loaded_Image_Handle out = {0};
        _Resources_Internal_Loaded_Image* item = (_Resources_Internal_Loaded_Image*) handle_table_get(resources->images, (Handle*) &handle);
        if(item)
        {
            //if its the only one it is already unique.
            if(item->info.reference_count == 1)
                out = *(Loaded_Image_Handle*) &handle;
            //Else duplicate it
            else
            {   
                Loaded_Image copy = {0};
                image_builder_init_from_image(&copy.image, resources->alloc, image_from_builder(item->data.image));
                memcpy(&copy.name, &item->data.name, sizeof item->data.name);

                Resource_Info new_info = item->info;
                new_info.reference_count = 1;
                new_info.creation_epoch_time = platform_universal_epoch_time();
                platform_capture_call_stack(new_info.callstack.stack_frames, RESOURCE_CALLSTACK_SIZE, 1);

                out = _resources_image_add(resources, &copy, string_from_builder(item->full_path), new_info);

                loaded_image_deinit(&copy);
            }
        }

        return out;
    }


void resources_init(Resources* resources, Allocator* alloc)
{
    resources_deinit(resources);

    resources->alloc = alloc;

    handle_table_init(&resources->images, alloc,            sizeof(Loaded_Image), DEF_ALIGN);
    handle_table_init(&resources->shapes, alloc,            sizeof(Shape_Assembly), DEF_ALIGN);
    handle_table_init(&resources->materials, alloc,         sizeof(Material), DEF_ALIGN);
    handle_table_init(&resources->objects, alloc,           sizeof(Object), DEF_ALIGN);
}

//@TODO
void _resources_cleanup(Resources* resources, bool is_complete_deinit)
{
    
    HANDLE_TABLE_FOR_EACH_BEGIN(resources->images, h, Loaded_Image*, loaded_image)
        loaded_image_deinit(loaded_image);
    HANDLE_TABLE_FOR_EACH_END

    HANDLE_TABLE_FOR_EACH_BEGIN(resources->objects, h, Object*, object)
        object_deinit(object, resources);
    HANDLE_TABLE_FOR_EACH_END
    
    HANDLE_TABLE_FOR_EACH_BEGIN(resources->materials, h, Material*, material)
        material_deinit(material, resources);
    HANDLE_TABLE_FOR_EACH_END
    
    HANDLE_TABLE_FOR_EACH_BEGIN(resources->shapes, h, Shape_Assembly*, shape_assembly)
        shape_assembly_deinit(shape_assembly);
    HANDLE_TABLE_FOR_EACH_END

    handle_table_deinit(&resources->images);
    handle_table_deinit(&resources->shapes);
    handle_table_deinit(&resources->materials);
    handle_table_deinit(&resources->objects);

    memset(resources, 0, sizeof *resources);
}

void map_init(Map* item, Resources* resources)
{
    map_deinit(item, resources);
    array_init(&item->path, resources->alloc);
    array_init(&item->name, resources->alloc);
    map_info_init(&item->info, resources->alloc);
}

void map_deinit(Map* item, Resources* resources)
{
    array_deinit(&item->path);
    array_deinit(&item->name);
    map_info_deinit(&item->info);
    resources_loaded_image_remove(resources, item->image);
    memset(item, 0, sizeof *item);
}

void cubemap_init(Cubemap* item, Resources* resources)
{
    for(isize i = 0; i < 6; i++)
        map_init(&item->faces[i], resources);
}

void cubemap_deinit(Cubemap* item, Resources* resources)
{
    for(isize i = 0; i < 6; i++)
        map_deinit(&item->faces[i], resources);
}

void material_init(Material* item, Resources* resources)
{
    material_deinit(item, resources);
    array_init(&item->path, resources->alloc);
    array_init(&item->name, resources->alloc);
    
    for(isize i = 0; i < MAP_TYPE_ENUM_COUNT; i++)
        map_init(&item->maps[i], resources);  
        
    for(isize i = 0; i < CUBEMAP_TYPE_ENUM_COUNT; i++)
        cubemap_init(&item->cubemaps[i], resources);  
}

void material_deinit(Material* item, Resources* resources)
{
    array_deinit(&item->path);
    array_deinit(&item->name);
    
    for(isize i = 0; i < MAP_TYPE_ENUM_COUNT; i++)
        map_deinit(&item->maps[i], resources);  
        
    for(isize i = 0; i < CUBEMAP_TYPE_ENUM_COUNT; i++)
        cubemap_deinit(&item->cubemaps[i], resources);  

    memset(item, 0, sizeof *item);
}

void object_group_init(Object_Group* item, Resources* resources)
{
    object_group_deinit(item, resources);
    array_init(&item->name, resources->alloc);
}

void object_group_deinit(Object_Group* item, Resources* resources)
{
    array_deinit(&item->name);
    resources_material_remove(resources, item->material);
    memset(item, 0, sizeof *item);
}

void object_init(Object* item, Resources* resources)
{
    object_deinit(item, resources);

    array_init(&item->path, resources->alloc);
    array_init(&item->name, resources->alloc);
    array_init(&item->groups, resources->alloc);
}

void object_deinit(Object* item, Resources* resources)
{
    array_deinit(&item->path);
    array_deinit(&item->name);
    resources_material_remove(resources, item->fallback_material);
    resources_shape_remove(resources, item->shape);

    for(isize i = 0; i < item->groups.size; i++)
        object_group_deinit(&item->groups.data[i], resources);
        
    array_init(&item->groups, resources->alloc);
    memset(item, 0, sizeof *item);
}

#endif