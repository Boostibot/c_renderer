#ifndef LIB_RESOURCE_DESCRIPTIONS
#define LIB_RESOURCE_DESCRIPTIONS

#include "shapes.h"
#include "string.h"
#include "lib/image.h"
#include "clock.h"

typedef enum Map_Scale_Filter {
    MAP_SCALE_FILTER_BILINEAR = 0,
    MAP_SCALE_FILTER_TRILINEAR,
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
    Vec3 scale; //default to 1 1 1
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
    String path;
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
    String name;
    String path;
    
    Material_Info info;

    Map_Description maps[MAP_TYPE_ENUM_COUNT];
    Cubemap_Description cubemaps[CUBEMAP_TYPE_ENUM_COUNT];
} Material_Description;

typedef struct Triangle_Mesh_Group_Description {
    String name;
    String material_name;
    String material_path;

    i32 next_i1;
    i32 child_i1;
    i32 depth;

    i32 triangles_from;
    i32 triangles_to;
    u32 _;
} Triangle_Mesh_Group_Description;

typedef Array(Material_Description) Material_Description_Array;
typedef Array(Triangle_Mesh_Group_Description) Triangle_Mesh_Group_Description_Array;


#endif