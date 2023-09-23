#pragma once
#include "shapes.h"
#include "string.h"

typedef struct Texture_Info {
    String_Builder path;        
    Vec2 offset;                
    Vec2 scale;
    Vec2 resolution;
    i32 chanel_index;

    //linear_color = (contrast + 1) * color^(1/gamma) + brigthness;
    //output_color = linear_color
    f32 gamma;
    f32 brigthness; //default 0
    f32 contrast;   //default 0

    bool is_repeating;
    bool is_enabled;
} Texture_Info;

typedef struct Material_Info_Phong {
    String_Builder name;
    String_Builder path;
    
    Vec3 color_ambient_color;                     
    Vec3 color_diffuse_color;                     
    Vec3 color_specular_color;                    
    f32 specular_exponent;                  
    f32 opacity;      
    
    f32 bump_multiplier;                    

    Texture_Info map_ambient;           
    Texture_Info map_diffuse;   
    Texture_Info map_specular_color;    
    Texture_Info map_specular_highlight;
    Texture_Info map_alpha;             
    Texture_Info map_bump;              
    Texture_Info map_displacement;      
    Texture_Info map_stencil;           
    Texture_Info map_normal;           
    Texture_Info map_reflection;           
};

typedef struct Material_Info {
    String_Builder name;
    String_Builder path;

    Vec3 albedo;
    Vec3 emissive;
    f32 rougness;
    f32 metalic;
    f32 ambient_occlusion;
    f32 opacity;
    f32 emissive_power;

    f32 emissive_power_map;
    f32 bump_multiplier;

    Texture_Info map_alpha;  
    Texture_Info map_bump;              
    Texture_Info map_displacement;      
    Texture_Info map_stencil;  
    Texture_Info map_rougness;           
    Texture_Info map_metallic;           
    Texture_Info map_emmisive;           
    Texture_Info map_normal;             
    Texture_Info map_reflection;   
} Obj_Material_Info;

typedef struct Mesh_Group
{
    String_Builder name;
    i32 material_i;
    i32 material_phong_i;
    bool is_phong_material;

    i32 next_i;
    i32 child_i;
    i32 depth;

    i32 vertices_from;
    i32 vertices_to;
} Mesh_Group;

DEFINE_ARRAY_TYPE(Mesh_Group, Mesh_Group_Array);
DEFINE_ARRAY_TYPE(Material_Info, Material_Info_Array);
DEFINE_ARRAY_TYPE(Material_Info_Phong, Material_Info_Phong_Array);

typedef struct Mesh_Info
{
    String_Builder name;
    String_Builder path;
    Mesh_Group_Array groups;

    Material_Info_Array materials;
    Material_Info_Phong_Array materials_phong;
} Mesh_Info;

void gemoemtry_init();
void gemoemtry_deinit();
