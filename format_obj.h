#ifndef LIB_FORMAT_OBJ_MTL
#define LIB_FORMAT_OBJ_MTL

// A file that only parses .obj and .mtl files without any postprocessing.
// It also tries to be as independent upon the rest of the engine as possible.
// 
// This is of course not total independence but hopefully enought to reuse this 
// file with greater ease.
// 
// Probably the bigest dependency is on the array.h file but doing the parsing without
// it would be needlessly annoying.
// 
// The parsing itself is really fast. About 8x faster than using scanf however the float parsing is not
// very accurate at the moment.

#include "lib/math.h"
#include "lib/array.h"
#include "lib/string.h"
#include "lib/parse.h"
#include "lib/arena.h"

#ifndef VEC_ARRAY_DEFINED
    #define VEC_ARRAY_DEFINED
    typedef Array(Vec2) Vec2_Array;
    typedef Array(Vec3) Vec3_Array;
    typedef Array(Vec4) Vec4_Array;
#endif
typedef struct Format_Mtl_Map {
    String_Builder path;        
    Vec3 offset;                //-o u (v (w)) (default 0 0 0)
    Vec3 scale;                 //-s u (v (w)) (default 1 1 1)
    Vec3 turbulance;            //-t u (v (w)) (default 0 0 0) //is some kind of perturbance in the specified direction
    i32 texture_resolution;     //-texres [i32 power of two]
    f32 mipmap_sharpness_boost; //-boost [f32]
    f32 bump_multiplier;        //-bm [f32] (default 1)

    f32 modify_brigthness;      //-mm [f32 base/brightness] [f32 gain/contrast]
    f32 modify_contrast;        //-mm [f32 base/brightness] [f32 gain/contrast]

    bool is_clamped;            //-clamp  [on | off]
    bool blend_u;               //-blendu [on | off]
    bool blend_v;               //-blendv [on | off]
    char use_channel;           //-imfchan [r | g | b | m | l | z]  

    bool was_set_offset;                
    bool was_set_scale;                 
    bool was_set_turbulance;            
    bool was_set_texture_resolution;     
    bool was_set_mipmap_sharpness_boost; 
    bool was_set_bump_multiplier;        

    bool was_set_modify_brigthness;      
    bool was_set_modify_contrast;        

    bool was_set_is_clamped;            
    bool was_set_blend_u;               
    bool was_set_blend_v;               
    bool was_set_use_channel;           
} Format_Mtl_Map;

typedef struct Format_Mtl_Material {
    String_Builder name;                    //newmtl [name]
    Vec3 ambient_color;                     //Ka [Vec3]
    Vec3 diffuse_color;                     //Kd [Vec3]
    Vec3 specular_color;                    //Ks [Vec3]
    Vec3 emissive_color;                    //Ke [Vec3]
    f32 specular_exponent;                  //Ns [f32]
    f32 opacity;                            //d/Tr [f32] //fully transparent is: d=0 and Tr=1 
    i32 illumination_mode;                  //illum [0, 10]

    f32 pbr_roughness;                           //Pr [f32]
    f32 pbr_metallic;                            //Pm
    f32 pbr_sheen;                               //Ps
    f32 pbr_anisotropy;                          //aniso
    f32 pbr_anisotropy_rotation;                 //anisor
    f32 pbr_clearcoat_thickness;                 //Pc
    f32 pbr_clearcoat_roughness;                 //Pcr

    Vec3 only_for_transparent_transmission_filter_color;    //Tf [vec3] / Tf xyz [vec3]
    f32 only_for_transparent_optical_density;               //Ni [f32]

    Format_Mtl_Map map_ambient;                //map_Ka [options] [name]
    Format_Mtl_Map map_diffuse;                //map_Kd
    Format_Mtl_Map map_specular_color;         //map_Ks
    Format_Mtl_Map map_specular_highlight;     //map_Ns 
    Format_Mtl_Map map_alpha;                  //map_d
    Format_Mtl_Map map_bump;                   //map_bump/bump
    Format_Mtl_Map map_emmisive;               //map_Ke
    Format_Mtl_Map map_displacement;           //disp
    Format_Mtl_Map map_stencil;                //decal
    Format_Mtl_Map map_normal;                 //norm

    Format_Mtl_Map map_reflection_sphere;     //refl -type {sphere, cube_top, cube_bottom, cube_front, cube_back, cube_left, cube_right}
    Format_Mtl_Map map_reflection_cube_top;    //refl ...
    Format_Mtl_Map map_reflection_cube_bottom; //refl ...
    Format_Mtl_Map map_reflection_cube_front;  //refl ...
    Format_Mtl_Map map_reflection_cube_back;   //refl ...
    Format_Mtl_Map map_reflection_cube_left;   //refl ...
    Format_Mtl_Map map_reflection_cube_right;  //refl ...

    Format_Mtl_Map map_pbr_roughness;          //map_Pr [options] [name]
    Format_Mtl_Map map_pbr_metallic;           //map_Pm
    Format_Mtl_Map map_pbr_sheen;              //map_Ps
    Format_Mtl_Map map_pbr_non_standard_ambient_occlusion;  //map_Pa //!DISCLAIMER! !THIS IS NOT STANDARD!
    Format_Mtl_Map map_pbr_rma;                //map_RMA/map_ORM  //RMA material (roughness, metalness, ambient occlusion)      

    bool was_set_ambient_color;                     //Ka [bool was_set_
    bool was_set_diffuse_color;                     //Kd [bool was_set_
    bool was_set_specular_color;                    //Ks [bool was_set_
    bool was_set_emissive_color;                    //Ke [bool was_set_
    bool was_set_specular_exponent;                  //Ns [bool was_set_
    bool was_set_opacity;                            //d/Tr [bool was_set_ //fully transparent is: d=0 and Tr=1 
    bool was_set_illumination_mode;                  //illum [0, 10]

    bool was_set_pbr_roughness;                           //Pr [bool was_set_
    bool was_set_pbr_metallic;                            //Pm
    bool was_set_pbr_sheen;                               //Ps
    bool was_set_pbr_anisotropy;                          //aniso
    bool was_set_pbr_anisotropy_rotation;                 //anisor
    bool was_set_pbr_clearcoat_thickness;                 //Pc
    bool was_set_pbr_clearcoat_roughness;                 //Pcr

    bool was_set_only_for_transparent_transmission_filter_color;    //Tf [vec3] / Tf xyz [vec3]
    bool was_set_only_for_transparent_optical_density;               //Ni [bool was_set_

    bool was_set_map_ambient;                
    bool was_set_map_diffuse;                
    bool was_set_map_specular_color;         
    bool was_set_map_specular_highlight;      
    bool was_set_map_alpha;                  
    bool was_set_map_bump;                   
    bool was_set_map_emmisive;               
    bool was_set_map_displacement;           
    bool was_set_map_stencil;                
    bool was_set_map_normal;                 

    bool was_set_map_reflection_sphere;     
    bool was_set_map_reflection_cube_top;    
    bool was_set_map_reflection_cube_bottom; 
    bool was_set_map_reflection_cube_front;  
    bool was_set_map_reflection_cube_back;   
    bool was_set_map_reflection_cube_left;   
    bool was_set_map_reflection_cube_right;  

    bool was_set_map_pbr_roughness;          
    bool was_set_map_pbr_metallic;           
    bool was_set_map_pbr_sheen;              
    bool was_set_map_pbr_non_standard_ambient_occlusion;  
    bool was_set_map_pbr_rma;    
    
    bool _[2];
} Format_Mtl_Material;

typedef struct Format_Obj_Group {
    String_Builder object;          //o [String]
    String_Builder_Array groups;    //g [group1] [group2] ...
    String_Builder material;        //usemtl [String material_name]
    i32 smoothing_index;            //s [index > 0] //to set smoothing
                                    //s [0 | off]   //to set no smoothing
    f32 merge_group_resolution;     //mg [i32 smoothing_index] [f32 distance]

    //Each group has info and some number of trinagles.
    //Trinagles from this group are inside the shared trinagles array
    //with the indices in range [trinagles_from, trinagles_from + trinagles_count)
    i32 trinagles_from;
    i32 trinagles_count;
} Format_Obj_Group;

typedef struct Format_Obj_Vertex_Index
{
    //one based indices into the appropraite arrays.
    //If value is 0 means not present.

    //THESE INDECES ARE NOT VALIDATED!
    //ENSURE THEIR VALUES ARE VALID BEFORE USE!
    i32 pos_i1;
    i32 uv_i1;
    i32 norm_i1;
} Format_Obj_Vertex_Index;

typedef Array(Format_Obj_Group) Format_Obj_Group_Array;
typedef Array(Format_Mtl_Material) Format_Mtl_Material_Array;
typedef Array(Format_Obj_Vertex_Index) Format_Obj_Vertex_Index_Array;

typedef struct Format_Obj_Model {
    Vec3_Array positions; 
    Vec2_Array uvs; 
    Vec3_Array normals;
    Format_Obj_Vertex_Index_Array indices;
    Format_Obj_Group_Array groups;
    String_Builder_Array material_files;
} Format_Obj_Model;

typedef enum Format_Obj_Mtl_Error_Statement Format_Obj_Mtl_Error_Statement;
typedef struct Format_Obj_Mtl_Error Format_Obj_Mtl_Error;

EXTERNAL void format_obj_texture_info_init(Format_Mtl_Map* info, Allocator* alloc);
EXTERNAL void format_obj_material_info_init(Format_Mtl_Material* info, Allocator* alloc);
EXTERNAL void format_obj_group_init(Format_Obj_Group* info, Allocator* alloc);
EXTERNAL void format_obj_model_init(Format_Obj_Model* info, Allocator* alloc);

EXTERNAL void format_obj_texture_info_deinit(Format_Mtl_Map* info);
EXTERNAL void format_obj_material_info_deinit(Format_Mtl_Material* info);
EXTERNAL void format_obj_group_deinit(Format_Obj_Group* info);
EXTERNAL void format_obj_model_deinit(Format_Obj_Model* info);

EXTERNAL bool format_obj_read(Format_Obj_Model* out, String obj_source, Format_Obj_Mtl_Error* errors, isize errors_max_count, isize* had_errors);
EXTERNAL bool format_mtl_read(Format_Mtl_Material_Array* out, String mtl_source, Format_Obj_Mtl_Error* errors, isize errors_max_count, isize* had_errors);

EXTERNAL const char* format_obj_mtl_error_statement_to_string(Format_Obj_Mtl_Error_Statement statement);

enum {
    FORMAT_MTL_ILLUM_MIN = 0,
    FORMAT_MTL_ILLUM_MAX = 10,
};

typedef enum Format_Obj_Mtl_Error_Statement
{
    FORMAT_OBJ_ERROR_NONE = 0,
    FORMAT_MTL_ERROR_NONE = 1,

    FORMAT_OBJ_ERROR_FACE,
    FORMAT_OBJ_ERROR_COMMENT,
    FORMAT_OBJ_ERROR_VERTEX_POS,
    FORMAT_OBJ_ERROR_VERTEX_NORM,
    FORMAT_OBJ_ERROR_VERTEX_UV,
    FORMAT_OBJ_ERROR_MATERIAL_LIBRARY,
    FORMAT_OBJ_ERROR_MATERIAL_USE,
    FORMAT_OBJ_ERROR_POINT,
    FORMAT_OBJ_ERROR_LINE,
    FORMAT_OBJ_ERROR_GROUP,
    FORMAT_OBJ_ERROR_SMOOTH_SHADING,
    FORMAT_OBJ_ERROR_OBJECT,
    FORMAT_OBJ_ERROR_INVALID_POS_INDEX,
    FORMAT_OBJ_ERROR_INVALID_UV_INDEX,
    FORMAT_OBJ_ERROR_INVALID_NORM_INDEX,
    FORMAT_OBJ_ERROR_OTHER,
    
    FORMAT_MTL_ERROR_NEWMTL,
    FORMAT_MTL_ERROR_MISSING_NEWMTL,
    FORMAT_MTL_ERROR_COLOR_AMBIENT,
    FORMAT_MTL_ERROR_COLOR_DIFFUSE,
    FORMAT_MTL_ERROR_COLOR_SPECULAR,
    FORMAT_MTL_ERROR_COLOR_EMISSIVE,

    FORMAT_MTL_ERROR_ROUGHNESS,
    FORMAT_MTL_ERROR_METALIC,
    FORMAT_MTL_ERROR_SHEEN,
    FORMAT_MTL_ERROR_CLEARCOAT_THICKNESS,
    FORMAT_MTL_ERROR_CLEARCOAT_ROUGHNESS,

    FORMAT_MTL_ERROR_SPECULAR_EXPOMENT,
    FORMAT_MTL_ERROR_OPACITY,
    FORMAT_MTL_ERROR_TRANSMISSION_FILTER,
    FORMAT_MTL_ERROR_TRANSMISSION_OPTICAL_DENSITY,
    FORMAT_MTL_ERROR_ILLUMINATION_MODE,
    FORMAT_MTL_ERROR_MAP,
    FORMAT_MTL_ERROR_OTHER,
} Format_Obj_Mtl_Error_Statement;

typedef struct Format_Obj_Mtl_Error
{
    isize index;
    i32 line;
    Format_Obj_Mtl_Error_Statement statement;
    bool unimplemented; //whether the error is due to it being unimplemented
    bool _[7];
} Format_Obj_Mtl_Error;

#endif

#define LIB_ALL_IMPL

#if (defined(LIB_ALL_IMPL) || defined(LIB_FORMAT_OBJ_MTL_IMPL)) && !defined(LIB_FORMAT_OBJ_MTL_HAS_IMPL)
#define LIB_FORMAT_OBJ_MTL_HAS_IMPL

EXTERNAL void _format_obj_texture_info_init_or_deinit(Format_Mtl_Map* info, Allocator* alloc, bool is_init)
{
    if(is_init)
    {
        info->scale = vec3_of(1);
        info->bump_multiplier = 1;
        info->modify_contrast = 1;
        builder_init(&info->path, alloc);
    }
    else
    {
        builder_deinit(&info->path);
        memset(info, 0, sizeof *info);
    }
}
EXTERNAL void _format_obj_material_info_init_or_deinit(Format_Mtl_Material* info, Allocator* alloc, bool is_init)
{       
    if(is_init)
        builder_init(&info->name, alloc);
    else
        builder_deinit(&info->name);

    info->opacity = 1;
    info->illumination_mode = 1;
    _format_obj_texture_info_init_or_deinit(&info->map_ambient, alloc, is_init);                //map_Ka [options] [name]
    _format_obj_texture_info_init_or_deinit(&info->map_diffuse, alloc, is_init);                //map_Kd
    _format_obj_texture_info_init_or_deinit(&info->map_specular_color, alloc, is_init);         //map_Ks
    _format_obj_texture_info_init_or_deinit(&info->map_specular_highlight, alloc, is_init);     //map_Ns 
    _format_obj_texture_info_init_or_deinit(&info->map_alpha, alloc, is_init);                  //map_d
    _format_obj_texture_info_init_or_deinit(&info->map_bump, alloc, is_init);                   //map_bump/bump
    _format_obj_texture_info_init_or_deinit(&info->map_displacement, alloc, is_init);           //disp
    _format_obj_texture_info_init_or_deinit(&info->map_stencil, alloc, is_init);                //decal
    _format_obj_texture_info_init_or_deinit(&info->map_normal, alloc, is_init);                 //norm
    _format_obj_texture_info_init_or_deinit(&info->map_reflection_sphere, alloc, is_init);      //refl -type {sphere, cube_top, cube_bottom, cube_front, cube_back, cube_left, cube_right}
    _format_obj_texture_info_init_or_deinit(&info->map_reflection_cube_top, alloc, is_init);    //refl ...
    _format_obj_texture_info_init_or_deinit(&info->map_reflection_cube_bottom, alloc, is_init); //refl ...
    _format_obj_texture_info_init_or_deinit(&info->map_reflection_cube_front, alloc, is_init);  //refl ...
    _format_obj_texture_info_init_or_deinit(&info->map_reflection_cube_back, alloc, is_init);   //refl ...
    _format_obj_texture_info_init_or_deinit(&info->map_reflection_cube_left, alloc, is_init);   //refl ...
    _format_obj_texture_info_init_or_deinit(&info->map_reflection_cube_right, alloc, is_init);  //refl ...

    _format_obj_texture_info_init_or_deinit(&info->map_pbr_roughness, alloc, is_init);          //map_Pr [options] [name]
    _format_obj_texture_info_init_or_deinit(&info->map_pbr_metallic, alloc, is_init);           //map_Pm
    _format_obj_texture_info_init_or_deinit(&info->map_pbr_sheen, alloc, is_init);              //map_Ps
    _format_obj_texture_info_init_or_deinit(&info->map_pbr_non_standard_ambient_occlusion, alloc, is_init);              //map_Pa
    _format_obj_texture_info_init_or_deinit(&info->map_emmisive, alloc, is_init);           //map_Ke
    _format_obj_texture_info_init_or_deinit(&info->map_pbr_rma, alloc, is_init);                //map_RMA/map_ORM  //RMA material (roughness, metalness, ambient occlusion)      
    
    if(!is_init)
        memset(info, 0, sizeof *info);
} 

EXTERNAL void format_obj_texture_info_deinit(Format_Mtl_Map* info)
{
    _format_obj_texture_info_init_or_deinit(info, NULL, false);
}

EXTERNAL void format_obj_material_info_deinit(Format_Mtl_Material* info)
{
    _format_obj_material_info_init_or_deinit(info, NULL, false);
}

EXTERNAL void format_obj_group_deinit(Format_Obj_Group* info)
{
    builder_array_deinit(&info->groups);
    builder_deinit(&info->object);
    builder_deinit(&info->material);

    memset(info, 0, sizeof *info);
}

EXTERNAL void format_obj_model_deinit(Format_Obj_Model* info)
{
    for(isize i = 0; i < info->groups.len; i++)
        format_obj_group_deinit(&info->groups.data[i]);
        
    array_deinit(&info->positions);
    array_deinit(&info->uvs);
    array_deinit(&info->normals);
    array_deinit(&info->indices);
    array_deinit(&info->groups);
    builder_array_deinit(&info->material_files);
}

EXTERNAL void format_obj_texture_info_init(Format_Mtl_Map* info, Allocator* alloc)
{
    _format_obj_texture_info_init_or_deinit(info, NULL, false); //first deinit
    _format_obj_texture_info_init_or_deinit(info, alloc, true); //then init again
}
EXTERNAL void format_obj_material_info_init(Format_Mtl_Material* info, Allocator* alloc)
{
    _format_obj_material_info_init_or_deinit(info, NULL, false); //first deinit
    _format_obj_material_info_init_or_deinit(info, alloc, true); //then init again

}
EXTERNAL void format_obj_model_init(Format_Obj_Model* info, Allocator* alloc)
{
    array_init(&info->positions, alloc);
    array_init(&info->uvs, alloc);
    array_init(&info->normals, alloc);
    array_init(&info->indices, alloc);
    array_init(&info->groups, alloc);
    array_init(&info->material_files, alloc);
}

EXTERNAL void format_obj_group_init(Format_Obj_Group* info, Allocator* alloc)
{
    format_obj_group_deinit(info);
    array_init(&info->groups, alloc);
    builder_init(&info->object, alloc);
    builder_init(&info->material, alloc);
}

EXTERNAL const char* format_obj_mtl_error_statement_to_string(Format_Obj_Mtl_Error_Statement statement)
{
    switch(statement)
    {
        case FORMAT_OBJ_ERROR_NONE: return "FORMAT_OBJ_ERROR_NONE";
        case FORMAT_OBJ_ERROR_FACE: return "FORMAT_OBJ_ERROR_FACE";
        case FORMAT_OBJ_ERROR_COMMENT: return "FORMAT_OBJ_ERROR_COMMENT";
        case FORMAT_OBJ_ERROR_VERTEX_POS: return "FORMAT_OBJ_ERROR_VERTEX_POS";
        case FORMAT_OBJ_ERROR_VERTEX_NORM: return "FORMAT_OBJ_ERROR_VERTEX_NORM";
        case FORMAT_OBJ_ERROR_VERTEX_UV: return "FORMAT_OBJ_ERROR_VERTEX_UV";
        case FORMAT_OBJ_ERROR_MATERIAL_LIBRARY: return "FORMAT_OBJ_ERROR_MATERIAL_LIBRARY";
        case FORMAT_OBJ_ERROR_MATERIAL_USE: return "FORMAT_OBJ_ERROR_MATERIAL_USE";
        case FORMAT_OBJ_ERROR_POINT: return "FORMAT_OBJ_ERROR_POINT";
        case FORMAT_OBJ_ERROR_LINE: return "FORMAT_OBJ_ERROR_LINE";
        case FORMAT_OBJ_ERROR_GROUP: return "FORMAT_OBJ_ERROR_GROUP";
        case FORMAT_OBJ_ERROR_SMOOTH_SHADING: return "FORMAT_OBJ_ERROR_SMOOTH_SHADING";
        case FORMAT_OBJ_ERROR_OBJECT: return "FORMAT_OBJ_ERROR_OBJECT";
        case FORMAT_OBJ_ERROR_INVALID_POS_INDEX: return "FORMAT_OBJ_ERROR_INVALID_POS_INDEX";
        case FORMAT_OBJ_ERROR_INVALID_UV_INDEX: return "FORMAT_OBJ_ERROR_INVALID_UV_INDEX";
        case FORMAT_OBJ_ERROR_INVALID_NORM_INDEX: return "FORMAT_OBJ_ERROR_INVALID_NORM_INDEX";
        
        case FORMAT_MTL_ERROR_NONE: return "FORMAT_MTL_ERROR_NONE";
        case FORMAT_MTL_ERROR_NEWMTL: return "FORMAT_MTL_ERROR_NEWMTL";
        case FORMAT_MTL_ERROR_MISSING_NEWMTL: return "FORMAT_MTL_ERROR_MISSING_NEWMTL";
        case FORMAT_MTL_ERROR_COLOR_AMBIENT: return "FORMAT_MTL_ERROR_COLOR_AMBIENT";
        case FORMAT_MTL_ERROR_COLOR_DIFFUSE: return "FORMAT_MTL_ERROR_COLOR_DIFFUSE";
        case FORMAT_MTL_ERROR_COLOR_SPECULAR: return "FORMAT_MTL_ERROR_COLOR_SPECULAR";

        case FORMAT_MTL_ERROR_COLOR_EMISSIVE: return "FORMAT_MTL_ERROR_COLOR_EMISSIVE";
        case FORMAT_MTL_ERROR_ROUGHNESS: return "FORMAT_MTL_ERROR_ROUGHNESS";
        case FORMAT_MTL_ERROR_METALIC: return "FORMAT_MTL_ERROR_METALIC";
        case FORMAT_MTL_ERROR_SHEEN: return "FORMAT_MTL_ERROR_SHEEN";
        case FORMAT_MTL_ERROR_CLEARCOAT_THICKNESS: return "FORMAT_MTL_ERROR_CLEARCOAT_THICKNESS";
        case FORMAT_MTL_ERROR_CLEARCOAT_ROUGHNESS: return "FORMAT_MTL_ERROR_CLEARCOAT_ROUGHNESS";

        case FORMAT_MTL_ERROR_SPECULAR_EXPOMENT: return "FORMAT_MTL_ERROR_SPECULAR_EXPOMENT";
        case FORMAT_MTL_ERROR_OPACITY: return "FORMAT_MTL_ERROR_OPACITY";
        case FORMAT_MTL_ERROR_TRANSMISSION_FILTER: return "FORMAT_MTL_ERROR_TRANSMISSION_FILTER";
        case FORMAT_MTL_ERROR_TRANSMISSION_OPTICAL_DENSITY: return "FORMAT_MTL_ERROR_TRANSMISSION_OPTICAL_DENSITY";
        case FORMAT_MTL_ERROR_ILLUMINATION_MODE: return "FORMAT_MTL_ERROR_ILLUMINATION_MODE";
        case FORMAT_MTL_ERROR_MAP: return "FORMAT_MTL_ERROR_MAP";
        case FORMAT_MTL_ERROR_OTHER: return "FORMAT_MTL_ERROR_OTHER";
        
        default:
        case FORMAT_OBJ_ERROR_OTHER: return "FORMAT_OBJ_ERROR_OTHER";
    }
}

INTERNAL Format_Obj_Group* _obj_parser_add_group(Format_Obj_Model* out, String active_object, String_Builder_Array* active_groups, isize vertex_index)
{
    //End the previous group
    if(out->groups.len != 0)
    {
        Format_Obj_Group* last = array_last(out->groups);
        last->trinagles_count = (i32) vertex_index - last->trinagles_from;
    }

    Format_Obj_Group new_group = {0};
    format_obj_group_init(&new_group, out->groups.allocator);
    new_group.trinagles_from = (i32) vertex_index;
    new_group.groups = *active_groups;
    builder_assign(&new_group.object, active_object);

    String_Builder_Array null = {0};
    *active_groups = null;
    array_push(&out->groups, new_group);
    
    return array_last(out->groups);
}

INTERNAL Format_Obj_Group* _obj_parser_get_active_group(Format_Obj_Model* out, String active_object, isize vertex_index)
{
    //if the last group is still the active one
    if(out->groups.len != 0)
    {
        Format_Obj_Group* last = array_last(out->groups);
        if(string_is_equal(last->object.string, active_object))
            return last;   
    }
    
    //Add new default group
    String_Builder_Array def_groups = {0};
    array_push(&def_groups, builder_from_cstring(NULL, "default"));

    return _obj_parser_add_group(out, active_object, &def_groups, vertex_index);
}

EXTERNAL bool format_obj_read(Format_Obj_Model* out, String obj_source, Format_Obj_Mtl_Error* errors, isize errors_max_count, isize* had_errors)
{
    Allocator* alloc = out->indices.allocator;
    ASSERT(alloc);

    format_obj_model_init(out, alloc);
    
    bool had_error = false;
    isize error_count = 0;
    
    //Try to guess the needed size based on a simple heurestic
    isize expected_line_count = obj_source.len / 32 + 128;
    array_reserve(&out->indices, expected_line_count);
    array_reserve(&out->positions, expected_line_count);
    array_reserve(&out->uvs, expected_line_count);
    array_reserve(&out->normals, expected_line_count);

    String active_object = {0};
    Format_Obj_Group* active_group = NULL;
    SCRATCH_ARENA(arena)
    {
        array_init_with_capacity(&out->groups, arena.alloc, 64);
        i32 trinagle_index = 0;
        for(Line_Iterator it = {0}; line_iterator_get_line(&it, obj_source); )
        {
            String line = string_trim_whitespace(it.line);

            // Skip blank lines.
            if(line.len == 0)
                continue;

            Format_Obj_Mtl_Error_Statement error = FORMAT_OBJ_ERROR_NONE;

            char first_char = line.data[0];
            switch (first_char) 
            {
                case '#': {
                    // Skip comments
                    continue;
                } break;

                //vertex variants
                case 'v': {
                    char second_char = line.data[1];
                    switch (second_char)
                    {
                        case ' ': {
                            Vec3 pos = {0};
                            isize line_index = 1;
                            bool matched = true
                                && match_whitespace(line, &line_index)
                                && match_decimal_f32(line, &line_index, &pos.x)
                                && match_whitespace(line, &line_index)
                                && match_decimal_f32(line, &line_index, &pos.y)
                                && match_whitespace(line, &line_index)
                                && match_decimal_f32(line, &line_index, &pos.z);

                            if(!matched)
                                error = FORMAT_OBJ_ERROR_VERTEX_POS;
                            else
                                array_push(&out->positions, pos);
                        } break;

                        case 'n': {

                            Vec3 norm = {0};
                            isize line_index = 2;
                            bool matched = true
                                && match_whitespace(line, &line_index)
                                && match_decimal_f32(line, &line_index, &norm.x)
                                && match_whitespace(line, &line_index)
                                && match_decimal_f32(line, &line_index, &norm.y)
                                && match_whitespace(line, &line_index)
                                && match_decimal_f32(line, &line_index, &norm.z);

                            if(!matched)
                                error = FORMAT_OBJ_ERROR_VERTEX_NORM;
                            else
                                array_push(&out->normals, norm);
                        } break;

                        case 't': {

                            // @NOTE: Ignoring Z if present.
                            Vec2 tex_coord = {0};
                            isize line_index = 2;
                            bool matched = true
                                && match_whitespace(line, &line_index)
                                && match_decimal_f32(line, &line_index, &tex_coord.x)
                                && match_whitespace(line, &line_index)
                                && match_decimal_f32(line, &line_index, &tex_coord.y);
                            
                            if(!matched)
                                error = FORMAT_OBJ_ERROR_VERTEX_UV;
                            else 
                                array_push(&out->uvs, tex_coord);
                        } break;

                        default: {
                            error = FORMAT_OBJ_ERROR_OTHER;
                        }
                    }
                } break;

                //faces
                case 'f': {

                    // can be one of the following:
                    // 1: f 1/1/1 2/2/2 3/3/3   ~~ pos/tex/norm pos/tex/norm pos/tex/norm
                    // 2: f 1/1 2/2 3/3         ~~ pos/tex pos/tex pos/tex
                    // 3: f 1//1 2//2 3//3      ~~ pos//norm pos//norm pos//norm
                    // 3: f 1 2 3               ~~ pos pos pos

                    //looks for "//" or the lack of "/" inside the string to narrow our options
                    bool has_double_slash = string_find_first(line, STRING("//") , 0) != -1;
                    bool has_slash = string_find_first_char(line, '/' , 0) != -1;

                    Format_Obj_Vertex_Index indices[3] = {0};
                    
                    bool ok = false;
                    isize line_index = 1;
                    //tries first the ones based on our analysis and then the rest until valid is found
                    
                    //f 1//1 2//2 3//3 
                    if(has_double_slash)
                    {
                        line_index = 1;
                        ok = true
                                && match_whitespace(line, &line_index)
                                && match_decimal_i32(line, &line_index, &indices[0].pos_i1)
                                && match_sequence(line, &line_index, STRING("//"))
                                && match_decimal_i32(line, &line_index, &indices[0].norm_i1)
                                && match_whitespace(line, &line_index)
                                && match_decimal_i32(line, &line_index, &indices[1].pos_i1)
                                && match_sequence(line, &line_index, STRING("//"))
                                && match_decimal_i32(line, &line_index, &indices[1].norm_i1)
                                && match_whitespace(line, &line_index)
                                && match_decimal_i32(line, &line_index, &indices[2].pos_i1)
                                && match_sequence(line, &line_index, STRING("//"))
                                && match_decimal_i32(line, &line_index, &indices[2].norm_i1);
                    }
                    //f 1 2 3
                    else if(!has_slash)
                    {
                        line_index = 1;
                        ok = true
                                && match_whitespace(line, &line_index)
                                && match_decimal_i32(line, &line_index, &indices[0].pos_i1)
                                && match_whitespace(line, &line_index)
                                && match_decimal_i32(line, &line_index, &indices[1].pos_i1)
                                && match_whitespace(line, &line_index)
                                && match_decimal_i32(line, &line_index, &indices[2].pos_i1);
                    }
                    
                    //f 1/1/1 2/2/2 3/3/3
                    if(!ok)
                    {
                        line_index = 1;
                        ok = true
                                && match_whitespace(line, &line_index)
                                && match_decimal_i32(line, &line_index, &indices[0].pos_i1)
                                && match_char(line, &line_index, '/')
                                && match_decimal_i32(line, &line_index, &indices[0].uv_i1)
                                && match_char(line, &line_index, '/')
                                && match_decimal_i32(line, &line_index, &indices[0].norm_i1)
                                && match_whitespace(line, &line_index)
                                && match_decimal_i32(line, &line_index, &indices[1].pos_i1)
                                && match_char(line, &line_index, '/')
                                && match_decimal_i32(line, &line_index, &indices[1].uv_i1)
                                && match_char(line, &line_index, '/')
                                && match_decimal_i32(line, &line_index, &indices[1].norm_i1)
                                && match_whitespace(line, &line_index)
                                && match_decimal_i32(line, &line_index, &indices[2].pos_i1)
                                && match_char(line, &line_index, '/')
                                && match_decimal_i32(line, &line_index, &indices[2].uv_i1)
                                && match_char(line, &line_index, '/')
                                && match_decimal_i32(line, &line_index, &indices[2].norm_i1);
                    }

                    //f 1/1 2/2 3/3
                    if(!ok)
                    {
                        line_index = 1;
                        ok = true
                                && match_whitespace(line, &line_index)
                                && match_decimal_i32(line, &line_index, &indices[0].pos_i1)
                                && match_char(line, &line_index, '/')
                                && match_decimal_i32(line, &line_index, &indices[0].uv_i1)
                                && match_whitespace(line, &line_index)
                                && match_decimal_i32(line, &line_index, &indices[1].pos_i1)
                                && match_char(line, &line_index, '/')
                                && match_decimal_i32(line, &line_index, &indices[1].uv_i1)
                                && match_whitespace(line, &line_index)
                                && match_decimal_i32(line, &line_index, &indices[2].pos_i1)
                                && match_char(line, &line_index, '/')
                                && match_decimal_i32(line, &line_index, &indices[2].uv_i1);
                    }

                    if(!ok)
                        error = FORMAT_OBJ_ERROR_FACE;

                    //correct negative values indices. If index is negative it refers to the -i-nth last parsed
                    // value in the given category. If the category does not recieve data (NULL) set the index to 0
                    for(isize i = 0; i < 3; i++)
                    {
                        Format_Obj_Vertex_Index index = indices[i];
                        if(index.pos_i1 < 0)
                            index.pos_i1 = (u32) out->positions.len + index.pos_i1 + 1;
                        
                        if(index.uv_i1 < 0)
                            index.uv_i1 = (u32) out->uvs.len + index.uv_i1 + 1;
                            
                        if(index.norm_i1 < 0)
                            index.norm_i1 = (u32) out->normals.len + index.norm_i1 + 1;
                    }

                    array_push(&out->indices, indices[0]); 
                    array_push(&out->indices, indices[1]); 
                    array_push(&out->indices, indices[2]);

                    trinagle_index += 1;
                } break;
                
                //Smoothing
                case 's': {
                    isize line_index1 = 1;
                    u64 smoothing_index = 0;
                    bool matched_smoothing_index = true
                        && match_whitespace(line, &line_index1)
                        && match_decimal_u64(line, &line_index1, &smoothing_index);
                        
                    isize line_index2 = 1;
                    bool matched_smoothing_off = !matched_smoothing_index
                        && match_whitespace(line, &line_index2)
                        && match_sequence(line, &line_index2, STRING("off"));
                    
                    if(matched_smoothing_off)
                        smoothing_index = 0;

                    if(matched_smoothing_index || matched_smoothing_off)
                    {
                        if(active_group == NULL)
                            active_group = _obj_parser_get_active_group(out, active_object, trinagle_index);

                        active_group->smoothing_index = (i32) smoothing_index;
                    }
                    else
                    {
                        error = FORMAT_OBJ_ERROR_SMOOTH_SHADING;
                    }
                } break;
                
                //Group: g [group1] [group2] ...
                case 'g': {
                    String_Builder_Array groups = {alloc};
                    isize line_index = 1;
                    while(true)
                    {
                        isize group_from = 0;
                        isize group_to = 0;
                        if(match_whitespace_separated(line, &line_index, &group_from, &group_to))
                        {
                            String group = string_range(line, group_from, group_to);
                            array_push(&groups, builder_from_string(alloc, group));
                        }
                        else
                        {
                            break;
                        }
                    }

                    if(groups.len > 0)
                        active_group = _obj_parser_add_group(out, active_object, &groups, trinagle_index);
                    else
                        error = FORMAT_OBJ_ERROR_GROUP;

                    builder_array_deinit(&groups);
                } break;

                //Object: g [object_name] ...
                case 'o': {
                    isize line_index = 1;
                    isize object_from = 0;
                    isize object_to = 0;

                    if(match_whitespace_separated(line, &line_index, &object_from, &object_to))
                        active_object = string_range(line, object_from, object_to);
                    else
                        error = FORMAT_OBJ_ERROR_OBJECT;
                } break;

                //Material library
                case 'm': {
                    isize line_index = 0;
                    isize from_index = 0;
                    isize to_index = 0;

                    if(match_sequence(line, &line_index, STRING("mtllib")) 
                        && match_whitespace_separated(line, &line_index, &from_index, &to_index))
                    {
                        String material_lib = string_range(line, from_index, to_index);
                        array_push(&out->material_files, builder_from_string(NULL, material_lib));
                    }
                    else
                    {
                        error = FORMAT_OBJ_ERROR_MATERIAL_LIBRARY;
                    }
                } break;
                
                //use material
                case 'u': {
                    isize line_index = 0;
                    isize from_index = 0;
                    isize to_index = 0;

                    if(match_sequence(line, &line_index, STRING("usemtl")) 
                        && match_whitespace_separated(line, &line_index, &from_index, &to_index))
                    {
                        if(active_group == NULL)
                            active_group = _obj_parser_get_active_group(out, active_object, trinagle_index);

                        String material_use = string_range(line, from_index, to_index);
                        builder_assign(&active_group->material, material_use);
                    }
                    else
                    {
                        error = FORMAT_OBJ_ERROR_MATERIAL_USE;
                    }
                } break;

                default: {
                    error = FORMAT_OBJ_ERROR_OTHER;
                }
            }

            //Handle errors
            if(error)
            {
                Format_Obj_Mtl_Error parser_error = {0};
                parser_error.index = it.line_from;
                parser_error.line = (i32) it.line_number;
                parser_error.statement = error;
                parser_error.unimplemented = false; //@TODO

                had_error = true;
                if(errors && error_count < errors_max_count)
                    errors[error_count] = parser_error;

                error_count++;
            }
        } 
        
        //end the last group
        if(active_group == NULL)
            active_group = _obj_parser_get_active_group(out, active_object, trinagle_index);
        active_group->trinagles_count = trinagle_index - active_group->trinagles_from;

        *had_errors = error_count;
    }

    return had_error;
}

//matches: x y z where x, y, z are f32
INTERNAL bool _match_space_separated_vec3(String str, isize* index, Vec3* matched)
{
    isize i = *index;
    bool state = match_decimal_f32(str, &i, &matched->x)
        && match_whitespace(str, &i)
        && match_decimal_f32(str, &i, &matched->y)
        && match_whitespace(str, &i)
        && match_decimal_f32(str, &i, &matched->z);

    if(state)
        *index = i;
    return state;
}

//matches the sequence:x (y (z)) where  () means optional
INTERNAL bool _match_space_separated_optional_vec3(String str, isize* index, Vec3* matched)
{
    isize i = *index;
    bool matched_x = match_decimal_f32(str, &i, &matched->x);

    bool matched_y = matched_x 
        && match_whitespace(str, &i)
        && match_decimal_f32(str, &i, &matched->y);
    
    bool matched_z = matched_y 
        && match_whitespace(str, &i)
        && match_decimal_f32(str, &i, &matched->z);

    bool state = matched_x || matched_y || matched_z;
    if(state)
        *index = i;
    return state;
}


EXTERNAL bool format_mtl_read(Format_Mtl_Material_Array* out, String mtl_source, Format_Obj_Mtl_Error* errors, isize errors_max_count, isize* had_errors)
{
    Allocator* def_alloc = out->allocator;
    ASSERT(def_alloc);
    Format_Mtl_Material* material = NULL;
    isize error_count = 0;
    bool had_error = false;

    for(Line_Iterator it = {0}; line_iterator_get_line(&it, mtl_source); )
    {
        String line = string_trim_whitespace(it.line);

        //skip blank lines
        if(line.len == 0)
            continue;

        Format_Obj_Mtl_Error_Statement error = FORMAT_MTL_ERROR_NONE;
        isize i = 0;
        Vec3 vec = {0};

        //material name
        if(i = 0, match_sequence(line, &i, STRING("newmtl")))
        {
            isize name_start = 0;
            isize name_end = 0;
            if(match_whitespace_separated(line, &i, &name_start, &name_end))
            {
                String name = string_range(line, name_start, name_end);
                Format_Mtl_Material new_material = {0};
                format_obj_material_info_init(&new_material, def_alloc);
                builder_assign(&new_material.name, name);

                array_push(out, new_material);
                material = array_last(*out);
            }
            else
            {
                error = FORMAT_MTL_ERROR_NEWMTL;
            }
        }

        //comment
        else if(i = 0, match_sequence(line, &i, STRING("#")))
        {
            //comment => do nothing
        }

        //.. the rest require material name and thus material to be already present
        //so we check and error here
        else if(material == NULL)
        {
            error = FORMAT_MTL_ERROR_MISSING_NEWMTL;
        }
        
        #define SET_PROP(on, prop) (on)->was_set_##prop = true, (on)->prop

        //ambient_color
        else if(i = 0, match_sequence(line, &i, STRING("Ka")))
        {
            if(match_whitespace(line, &i) && _match_space_separated_vec3(line, &i, &vec))
                SET_PROP(material,ambient_color) = vec;
            else
                error = FORMAT_MTL_ERROR_COLOR_AMBIENT;
        }

        //diffuse_color
        else if(i = 0, match_sequence(line, &i, STRING("Kd")))
        {
            if(match_whitespace(line, &i) && _match_space_separated_vec3(line, &i, &vec))
                SET_PROP(material, diffuse_color) = vec;
            else
                error = FORMAT_MTL_ERROR_COLOR_DIFFUSE;
        }

        //specular_color
        else if(i = 0, match_sequence(line, &i, STRING("Ks")))
        {
            if(match_whitespace(line, &i) && _match_space_separated_vec3(line, &i, &vec))
                SET_PROP(material, specular_color) = vec;
            else
                error = FORMAT_MTL_ERROR_COLOR_SPECULAR;
        }

        //emissive_color
        else if(i = 0, match_sequence(line, &i, STRING("Ke")))
        {
            if(match_whitespace(line, &i) && _match_space_separated_vec3(line, &i, &vec))
                SET_PROP(material, emissive_color) = vec;
            else
                error = FORMAT_MTL_ERROR_COLOR_EMISSIVE;
        }
        
        //specular_exponent
        else if(i = 0, match_sequence(line, &i, STRING("Ns")))
        {
            f32 val = 0;
            if(match_whitespace(line, &i) && match_decimal_f32(line, &i, &val))
                SET_PROP(material, specular_exponent) = val;
            else
                error = FORMAT_MTL_ERROR_SHEEN;
        }

        //pbr_roughness
        else if(i = 0, match_sequence(line, &i, STRING("Pr")))
        {
            f32 val = 0;
            if(match_whitespace(line, &i) && match_decimal_f32(line, &i, &val))
                SET_PROP(material, pbr_roughness) = val;
            else
                error = FORMAT_MTL_ERROR_ROUGHNESS;
        }

        //pbr_metallic
        else if(i = 0, match_sequence(line, &i, STRING("Pm")))
        {
            f32 val = 0;
            if(match_whitespace(line, &i) && match_decimal_f32(line, &i, &val))
                SET_PROP(material, pbr_metallic) = val;
            else
                error = FORMAT_MTL_ERROR_METALIC;
        }

        //pbr_sheen
        else if(i = 0, match_sequence(line, &i, STRING("Ps")))
        {
            f32 val = 0;
            if(match_whitespace(line, &i) && match_decimal_f32(line, &i, &val))
                SET_PROP(material, pbr_sheen) = val;
            else
                error = FORMAT_MTL_ERROR_SHEEN;
        }
        
        //pbr_anisotropy
        else if(i = 0, match_sequence(line, &i, STRING("aniso")))
        {
            f32 val = 0;
            if(match_whitespace(line, &i) && match_decimal_f32(line, &i, &val))
                SET_PROP(material, pbr_anisotropy) = val;
            else
                error = FORMAT_MTL_ERROR_SHEEN;
        }
        
        //pbr_anisotropy_rotation
        else if(i = 0, match_sequence(line, &i, STRING("anisor")))
        {
            f32 val = 0;
            if(match_whitespace(line, &i) && match_decimal_f32(line, &i, &val))
                SET_PROP(material, pbr_anisotropy_rotation) = val;
            else
                error = FORMAT_MTL_ERROR_SHEEN;
        }

        //pbr_clearcoat_thickness
        else if(i = 0, match_sequence(line, &i, STRING("Pc")))
        {
            f32 val = 0;
            if(match_whitespace(line, &i) && match_decimal_f32(line, &i, &val))
                SET_PROP(material, pbr_clearcoat_thickness) = val;
            else
                error = FORMAT_MTL_ERROR_CLEARCOAT_THICKNESS;
        }

        //pbr_clearcoat_roughness
        else if(i = 0, match_sequence(line, &i, STRING("Pcr")))
        {
            f32 val = 0;
            if(match_whitespace(line, &i) && match_decimal_f32(line, &i, &val))
                SET_PROP(material, pbr_clearcoat_roughness) = val;
            else
                error = FORMAT_MTL_ERROR_CLEARCOAT_ROUGHNESS;
        }

        //opacity (d syntax)
        else if(i = 0, match_sequence(line, &i, STRING("d")))
        {
            f32 val = 0;
            if(match_whitespace(line, &i) && match_decimal_f32(line, &i, &val))
                SET_PROP(material, opacity) = val; //d=0 => transparent
            else
                error = FORMAT_MTL_ERROR_OPACITY;
        }

        //opacity (Tr syntax)
        else if(i = 0, match_sequence(line, &i, STRING("Tr"))) 
        {
            f32 val = 0;
            if(match_whitespace(line, &i) && match_decimal_f32(line, &i, &val))
                SET_PROP(material, opacity) = 1.0f - val; //Tr=1 => transparent
            else
                error = FORMAT_MTL_ERROR_OPACITY;
        }

        //transmission_filter_color
        else if(i = 0, match_sequence(line, &i, STRING("Tf")))
        {
            if(match_whitespace(line, &i) && _match_space_separated_vec3(line, &i, &vec))
                SET_PROP(material, only_for_transparent_transmission_filter_color) = vec;
            else
                error = FORMAT_MTL_ERROR_TRANSMISSION_FILTER;
        }

        //optical_density
        else if(i = 0, match_sequence(line, &i, STRING("Ni")))
        {
            f32 val = 0;
            if(match_whitespace(line, &i) && match_decimal_f32(line, &i, &val))
                SET_PROP(material, only_for_transparent_optical_density) = val;
            else
                error = FORMAT_MTL_ERROR_TRANSMISSION_OPTICAL_DENSITY;
        }

        //illumination_mode
        else if(i = 0, match_sequence(line, &i, STRING("illum")))
        {
            u64 val = 0;
            if(match_whitespace(line, &i) && match_decimal_u64(line, &i, &val)
                && FORMAT_MTL_ILLUM_MIN <= val && val <= FORMAT_MTL_ILLUM_MAX)
                    SET_PROP(material, illumination_mode) = (i32) val;
            else
                error = FORMAT_MTL_ERROR_ILLUMINATION_MODE;
        }

        //Else - error or some kind of map
        else
        {
            typedef struct {
                Format_Mtl_Map* tex_info;
                const char* sequences[2];
            } Mtl_Map_Table_Entry;

            //try to match any remaining statements as map kinds. 
            //We use a table to model this because all of the options have the same syntax
            Mtl_Map_Table_Entry map_table[] = {
                    {&material->map_ambient,                 "map_Ka"},
                    {&material->map_diffuse,                 "map_Kd"},
                    {&material->map_specular_color,          "map_Ks"},
                    {&material->map_specular_highlight,      "map_Ns "},
                    {&material->map_alpha,                   "map_d"},
                    {&material->map_bump,                    "map_bump", "bump"},
                    {&material->map_displacement,            "disp"},
                    {&material->map_stencil,                  "decal"},
                    {&material->map_reflection_sphere,        "refl "},
                    {&material->map_normal,                   "norm"},

                    {&material->map_pbr_roughness,           "map_Pr"},
                    {&material->map_pbr_metallic,            "map_Pm"},
                    {&material->map_pbr_sheen,               "map_Ps"},
                    {&material->map_emmisive,                "map_Ke"},
                    {&material->map_pbr_rma,                 "map_RMA", "map_ORM"},
                    {&material->map_pbr_non_standard_ambient_occlusion,               "map_Pa"},
            };

            //try to match any of the sequences from the above table
            isize map_table_matched_i = -1;
            for(isize k = 0; k < ARRAY_SIZE(map_table); k++)
            {
                Mtl_Map_Table_Entry map = map_table[k];
                for(isize j = 0; j < 2; j++)
                {
                    String str_sequence = string_of(map.sequences[j]);
                    if(str_sequence.len != 0 && match_sequence(line, &i, str_sequence))
                    {
                        map_table_matched_i = k;
                        break;
                    }
                }
            }
            
            #define SET_PROP_(on, prop) (on).was_set_##prop = true, (on).prop
            //if found match try to parse arguments
            bool matched = map_table_matched_i != -1;
            if(!matched)
                error = FORMAT_MTL_ERROR_OTHER;
            else
            {
                Mtl_Map_Table_Entry map_entry = map_table[map_table_matched_i];
                Format_Mtl_Map* tex_info = map_entry.tex_info;
                Format_Mtl_Map temp_tex_info = {0};
                format_obj_texture_info_init(&temp_tex_info, def_alloc);

                while(matched && i < line.len)
                {
                    matched = match_whitespace(line, &i);
                    isize arg_from = i;
                    String arg = string_tail(line, arg_from);
                    isize arg_i = 0;
                        
                    Vec3 arg_vec = {0};
                    //offset
                    if(arg_i = 0, match_sequence(arg, &arg_i, STRING("-o")))
                    {
                        if(match_whitespace(arg, &arg_i) && _match_space_separated_optional_vec3(arg, &arg_i, &arg_vec))
                            SET_PROP_(temp_tex_info,offset) = arg_vec;
                        else
                            matched = false;
                    }
                    //scale
                    else if(arg_i = 0, match_sequence(arg, &arg_i, STRING("-s")))
                    {
                        
                        if(match_whitespace(arg, &arg_i) && _match_space_separated_optional_vec3(arg, &arg_i, &arg_vec))
                            SET_PROP_(temp_tex_info,scale) = arg_vec;
                        else
                            matched = false;
                    }
                    //turbulance
                    else if(arg_i = 0, match_sequence(arg, &arg_i, STRING("-t")))
                    {
                        
                        if(match_whitespace(arg, &arg_i) && _match_space_separated_optional_vec3(arg, &arg_i, &arg_vec))
                            SET_PROP_(temp_tex_info,turbulance) = arg_vec;
                        else
                            matched = false;
                    }
                    //texture_resolution
                    else if(arg_i = 0, match_sequence(arg, &arg_i, STRING("-texres")))
                    {
                        u64 res = 0;
                        if(match_whitespace(arg, &arg_i) && match_decimal_u64(arg, &arg_i, &res))
                            SET_PROP_(temp_tex_info,texture_resolution) = (i32) res;
                        else
                            matched = false;
                    }
                    //mipmap_sharpness_boost
                    else if(arg_i = 0, match_sequence(arg, &arg_i, STRING("-boost")))
                    {
                        f32 val = 0;
                        if(match_whitespace(arg, &arg_i) && match_decimal_f32(arg, &arg_i, &val))
                            SET_PROP_(temp_tex_info,mipmap_sharpness_boost) = val;
                        else
                            matched = false;
                    }
                    //bump_multiplier
                    else if(arg_i = 0, match_sequence(arg, &arg_i, STRING("-bm")))
                    {
                        f32 val = 0;
                        if(match_whitespace(arg, &arg_i) && match_decimal_f32(arg, &arg_i, &val))
                            material->map_bump.bump_multiplier = val - 1.0f;
                        else
                            matched = false;
                    }
                    //modify_brigthness modify_contrast
                    else if(arg_i = 0, match_sequence(arg, &arg_i, STRING("-mm")))
                    {
                        f32 modify_brigthness = 0;
                        f32 modify_contrast = 0;
                        if(match_whitespace(arg, &arg_i) && match_decimal_f32(arg, &arg_i, &modify_brigthness)
                            && match_whitespace(arg, &arg_i) && match_decimal_f32(arg, &arg_i, &modify_contrast))
                        {
                            SET_PROP_(temp_tex_info,modify_brigthness) = modify_brigthness;
                            SET_PROP_(temp_tex_info,modify_contrast) = modify_contrast - 1.0f;
                        }
                        else
                            matched = false;
                    }
                    //is_clamped
                    else if(arg_i = 0, match_sequence(arg, &arg_i, STRING("-clamp")))
                    {
                        bool had_space = match_whitespace(arg, &arg_i);
                        if(had_space && match_sequence(arg, &arg_i, STRING("on")))
                            SET_PROP_(temp_tex_info,is_clamped) = true;
                        else if(had_space && match_sequence(arg, &arg_i, STRING("off")))
                            SET_PROP_(temp_tex_info,is_clamped) = false;
                        else
                            matched = false;
                    }
                    //blend_u
                    else if(arg_i = 0, match_sequence(arg, &arg_i, STRING("-blendu")))
                    {
                        bool had_space = match_whitespace(arg, &arg_i);
                        if(had_space && match_sequence(arg, &arg_i, STRING("on")))
                            SET_PROP_(temp_tex_info,blend_u) = false;
                        else if(had_space && match_sequence(arg, &arg_i, STRING("off")))
                            SET_PROP_(temp_tex_info,blend_u) = true;
                        else
                            matched = false;
                    }
                    //blend_v
                    else if(arg_i = 0, match_sequence(arg, &arg_i, STRING("-blendv")))
                    {
                        bool had_space = match_whitespace(arg, &arg_i);
                        if(had_space && match_sequence(arg, &arg_i, STRING("on")))
                            SET_PROP_(temp_tex_info,blend_v) = false;
                        else if(had_space && match_sequence(arg, &arg_i, STRING("off")))
                            SET_PROP_(temp_tex_info,blend_v) = true;
                        else
                            matched = false;
                    }
                    //-type sphere 
                    else if(arg_i = 0, match_sequence(arg, &arg_i, STRING("-type")))
                    {
                        Format_Mtl_Map* found_tex_info = NULL; 
                        if(match_whitespace(arg, &arg_i))
                        {
                            if(match_sequence(arg, &arg_i, STRING("sphere")))
                                found_tex_info = &material->map_reflection_sphere;
                            else if(match_sequence(arg, &arg_i, STRING("cube_top")))
                                found_tex_info = &material->map_reflection_cube_top;
                            else if(match_sequence(arg, &arg_i, STRING("cube_bottom")))
                                found_tex_info = &material->map_reflection_cube_bottom;
                            else if(match_sequence(arg, &arg_i, STRING("cube_front")))
                                found_tex_info = &material->map_reflection_cube_front;
                            else if(match_sequence(arg, &arg_i, STRING("cube_back")))
                                found_tex_info = &material->map_reflection_cube_back;
                            else if(match_sequence(arg, &arg_i, STRING("cube_left")))
                                found_tex_info = &material->map_reflection_cube_left;
                            else if(match_sequence(arg, &arg_i, STRING("cube_right")))
                                found_tex_info = &material->map_reflection_cube_right;
                        }

                        if(found_tex_info == NULL)
                            matched = false;
                        else    
                        {
                            //if is not on refl map then do nothing. 
                            //This could be a hard error but its mostly harmless
                            ASSERT(map_entry.sequences[0] != NULL);
                            if(strcmp(map_entry.sequences[0], "refl") == 0)
                                tex_info = found_tex_info;
                        }
                    }
                    //use_channel
                    //-imfchan [r | g | b | m | l | z]  
                    else if(arg_i = 0, match_sequence(arg, &arg_i, STRING("-imfchan")))
                    {
                        bool matched_arg = match_whitespace(arg, &arg_i);
                        char c = 0;
                        if(matched_arg && arg.len - arg_i > 1)
                            c = arg.data[arg_i];

                        if(matched_arg)
                            switch(c)
                            {
                                case 'r': 
                                case 'g': 
                                case 'b': 
                                case 'm': 
                                case 'l': 
                                case 'z':
                                    SET_PROP_(temp_tex_info,use_channel) = c;
                                    break;
                                default: 
                                    matched_arg = false;
                                    break;
                            }

                        if(!matched_arg)
                            matched = false;
                    }
                    else
                    {
                        break;         
                    }
                    

                    ASSERT(arg_i > 0, "will make progress");
                    i += arg_i;
                }


                isize filename_from = i;
                isize filename_to = line.len;

                String filename = string_range(line, filename_from, filename_to);
                if(filename.len > 0)
                    builder_assign(&temp_tex_info.path, filename);
                else
                    matched = FORMAT_MTL_ERROR_MAP;
                    
                format_obj_texture_info_deinit(tex_info);
                *tex_info = temp_tex_info;
            }

        }
    
        if(error != FORMAT_MTL_ERROR_NONE)
        {
            Format_Obj_Mtl_Error parser_error = {0};
            parser_error.index = it.line_from;
            parser_error.line = (i32) it.line_number;
            parser_error.statement = error;
            parser_error.unimplemented = false;

            had_error = true;
            if(errors && error_count < errors_max_count)
                errors[error_count] = parser_error;

            error_count++;
        }
    }

    *had_errors = error_count;
    return had_error;
}

#endif