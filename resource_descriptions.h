#ifndef LIB_RESOURCE_DESCRIPTIONS
#define LIB_RESOURCE_DESCRIPTIONS

#include "shapes.h"
#include "string.h"
#include "lib/image.h"
#include "clock.h"

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
    i32 channels_idices1[MAX_CHANNELS]; //One based indices into the image channels. 
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

    b32 is_enabled; //whether or not to use this texture. Maybe [0, 1] float?
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
    u32 _;
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

typedef struct Triangle_Mesh_Group_Description {
    String_Builder name;
    String_Builder material_name;
    String_Builder material_path;

    i32 next_i1;
    i32 child_i1;
    i32 depth;

    i32 triangles_from;
    i32 triangles_to;
    u32 _;
} Triangle_Mesh_Group_Description;

typedef Array(Material_Description) Material_Description_Array;
typedef Array(Triangle_Mesh_Group_Description) Triangle_Mesh_Group_Description_Array;

typedef struct Triangle_Mesh_Description {
    String_Builder name;
    String_Builder path;
    Triangle_Mesh_Group_Description_Array groups;
    String_Builder_Array material_files;
} Triangle_Mesh_Description;

EXTERNAL void map_info_init(Map_Info* description, Allocator* alloc);
EXTERNAL void map_info_deinit(Map_Info* description);

EXTERNAL void map_description_init(Map_Description* description, Allocator* alloc);
EXTERNAL void map_description_deinit(Map_Description* description);

EXTERNAL void cubemap_description_init(Cubemap_Description* description, Allocator* alloc);
EXTERNAL void cubemap_description_deinit(Cubemap_Description* description);

EXTERNAL void material_description_init(Material_Description* description, Allocator* alloc);
EXTERNAL void material_description_deinit(Material_Description* description);

EXTERNAL void triangle_mesh_group_description_init(Triangle_Mesh_Group_Description* description, Allocator* alloc);
EXTERNAL void triangle_mesh_group_description_deinit(Triangle_Mesh_Group_Description* description);

EXTERNAL void triangle_mesh_description_init(Triangle_Mesh_Description* description, Allocator* alloc);
EXTERNAL void triangle_mesh_description_deinit(Triangle_Mesh_Description* description);

#endif

#if (defined(LIB_ALL_IMPL) || defined(LIB_RESOURCE_DESCRIPTIONS_IMPL)) && !defined(LIB_RESOURCE_DESCRIPTIONS_HAS_IMPL)
#define LIB_RESOURCE_DESCRIPTIONS_HAS_IMPL

EXTERNAL void map_info_init(Map_Info* description, Allocator* alloc)
{
    //Nothing so far but we might add dynamic behaviour later
    (void) alloc;
    memset(description, 0, sizeof *description);

    description->offset = vec3_of(0);                
    description->scale = vec3_of(1);
    description->resolution = vec3_of(0);

    description->channels_count = 0;

    description->filter_minify = MAP_SCALE_FILTER_BILINEAR;
    description->filter_magnify = MAP_SCALE_FILTER_BILINEAR;
    description->repeat_u = MAP_REPEAT_REPEAT;
    description->repeat_v = MAP_REPEAT_REPEAT;
    description->repeat_w = MAP_REPEAT_REPEAT;

    description->gamma = 1;          
    description->brigthness = 0;     
    description->contrast = 1;       
}
EXTERNAL void map_info_deinit(Map_Info* description)
{
    //Nothing so far but we might add dynamic behaviour later
    memset(description, 0, sizeof *description);
}

EXTERNAL void map_description_init(Map_Description* description, Allocator* alloc)
{
    map_description_deinit(description);
    map_info_init(&description->info, alloc);
    builder_init(&description->path, alloc);
}
EXTERNAL void map_description_deinit(Map_Description* description)
{
    builder_deinit(&description->path);
    map_info_deinit(&description->info);
    memset(description, 0, sizeof *description);
}

EXTERNAL void cubemap_description_init(Cubemap_Description* description, Allocator* alloc)
{
    for(isize i = 0; i < 6; i++)
        map_description_init(&description->faces[i], alloc);
}
EXTERNAL void cubemap_description_deinit(Cubemap_Description* description)
{
    for(isize i = 0; i < 6; i++)
        map_description_deinit(&description->faces[i]);
}

EXTERNAL void material_description_init(Material_Description* description, Allocator* alloc)
{
    material_description_deinit(description);

    builder_init(&description->path, alloc);
    builder_init(&description->name, alloc);

    for(isize i = 0; i < MAP_TYPE_ENUM_COUNT; i++)
        map_description_init(&description->maps[i], alloc);  
        
    for(isize i = 0; i < CUBEMAP_TYPE_ENUM_COUNT; i++)
        cubemap_description_init(&description->cubemaps[i], alloc);  

    memset(description, 0, sizeof *description);
}
EXTERNAL void material_description_deinit(Material_Description* description)
{
    builder_deinit(&description->path);
    builder_deinit(&description->name);
    
    for(isize i = 0; i < MAP_TYPE_ENUM_COUNT; i++)
        map_description_deinit(&description->maps[i]);  
        
    for(isize i = 0; i < CUBEMAP_TYPE_ENUM_COUNT; i++)
        cubemap_description_deinit(&description->cubemaps[i]);  


    memset(description, 0, sizeof *description);
}

EXTERNAL void triangle_mesh_group_description_init(Triangle_Mesh_Group_Description* description, Allocator* alloc)
{
    triangle_mesh_group_description_deinit(description);

    builder_init(&description->name, alloc);
    builder_init(&description->material_name, alloc);
    builder_init(&description->material_path, alloc);
}
EXTERNAL void triangle_mesh_group_description_deinit(Triangle_Mesh_Group_Description* description)
{
    builder_deinit(&description->name);
    builder_deinit(&description->material_name);
    builder_deinit(&description->material_path);

    memset(description, 0, sizeof *description);
}

EXTERNAL void triangle_mesh_description_init(Triangle_Mesh_Description* description, Allocator* alloc)
{
    triangle_mesh_description_deinit(description);

    builder_init(&description->name, alloc);
    builder_init(&description->path, alloc);
    array_init(&description->groups, alloc);
    array_init(&description->material_files, alloc);
}
EXTERNAL void triangle_mesh_description_deinit(Triangle_Mesh_Description* description)
{
    builder_deinit(&description->name);
    builder_deinit(&description->path);

    for(isize i = 0; i < description->groups.len; i++)
        triangle_mesh_group_description_deinit(&description->groups.data[i]);

    array_deinit(&description->groups);
    builder_array_deinit(&description->material_files);

    memset(description, 0, sizeof *description);
}

#endif