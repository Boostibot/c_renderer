#ifndef LIB_OBJ_PARSER
#define LIB_OBJ_PARSER

#include "math.h"
#include "array.h"
#include "string.h"
#include "format.h"
#include "parse.h"

#ifndef VEC_ARRAY_DEFINED
    #define VEC_ARRAY_DEFINED
    DEFINE_ARRAY_TYPE(Vec2, Vec2_Array);
    DEFINE_ARRAY_TYPE(Vec3, Vec3_Array);
    DEFINE_ARRAY_TYPE(Vec4, Vec4_Array);
#endif

#define OBJ_PARSER_MAX_ERRORS_TO_PRINT 1000
#define MTL_PARSER_MAX_ERRORS_TO_PRINT 1000

#define MTL_ILLUM_MIN 0
#define MTL_ILLUM_MAX 10

typedef enum Obj_Parser_Error_On
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
    FORMAT_OBJ_ERROR_INVALID_INDEX,
    FORMAT_OBJ_ERROR_OTHER,
    
    FORMAT_MTL_ERROR_NEWMTL,
    FORMAT_MTL_ERROR_MISSING_NEWMTL,
    FORMAT_MTL_ERROR_COLOR_AMBIENT,
    FORMAT_MTL_ERROR_COLOR_DIFFUSE,
    FORMAT_MTL_ERROR_COLOR_SPECULAR,
    FORMAT_MTL_ERROR_SPECULAR_EXPOMENT,
    FORMAT_MTL_ERROR_OPACITY,
    FORMAT_MTL_ERROR_TRANSMISSION_FILTER,
    FORMAT_MTL_ERROR_TRANSMISSION_OPTICAL_DENSITY,
    FORMAT_MTL_ERROR_ILLUMINATION_MODE,
    FORMAT_MTL_ERROR_MAP,
    FORMAT_MTL_ERROR_OTHER,
} Obj_Parser_Error_On;

typedef struct Obj_Parser_Error
{
    isize index;
    i32 line;
    bool unimplemented; //wheter the error is due to it being unimplemented
    Obj_Parser_Error_On statement;
} Obj_Parser_Error;

typedef struct Obj_Texture_Info {
    String_Builder path;        
    Vec3 offset;                //-o u (vec (w))
    Vec3 scale;                 //-s u (vec (w))
    Vec3 turbulance;            //-t u (vec (w)) //is some kind of perturbance in the specified direction
    i32 texture_resolution;     //-texres [i32 power of two]
    f32 mipmap_sharpness_boost; //-boost [f32]
    f32 bump_multiplier;        //-bm [f32]

    f32 modify_brigthness;      //-mm [f32 base/brightness] [f32 gain/contrast]
    f32 modify_contrast;        //-mm [f32 base/brightness] [f32 gain/contrast]

    bool is_repeating;          //-clamp  [on | off]
    bool blend_u;               //-blendu [on | off]
    bool blend_v;               //-blendv [on | off]
    char use_channel;           //-imfchan [r | g | b | m | l | z]  
} Obj_Texture_Info;

typedef struct Obj_Material_Info {
    String_Builder name;                    //newmtl [name]
    Vec3 ambient_color;                     //Ka [vec3]
    Vec3 diffuse_color;                     //Kd [vec3]
    Vec3 specular_color;                    //Ks [vec3]
    f32 specular_exponent;                  //Ns [f32]
    f32 opacity;                            //d/Tr [f32] //fully transparent is: d=0 and Tr=1 
    
    Vec3 only_for_transparent_transmission_filter_color;    //Tf [vec3] / Tf xyz [vec3]
    f32 only_for_transparent_optical_density;               //Ni [f32]
    i32 illumination_mode;                                  //illum [0, 10]

    Obj_Texture_Info map_ambient;                //map_Ka [options] [name]
    Obj_Texture_Info map_ambient_diffuse;        //map_Kd
    Obj_Texture_Info map_specular_color;         //map_Ks
    Obj_Texture_Info map_specular_highlight;     //map_Ns 
    Obj_Texture_Info map_alpha;                  //map_d
    Obj_Texture_Info map_bump;                   //map_bump/bump
    Obj_Texture_Info map_displacement;           //disp
    Obj_Texture_Info map_stencil;                //decal

    Obj_Texture_Info map_reflection_spehere;     //refl -type {sphere, cube_top, cube_bottom, cube_front, cube_back, cube_left, cube_right}
    Obj_Texture_Info map_reflection_cube_top;    //refl 
    Obj_Texture_Info map_reflection_cube_bottom; //refl
    Obj_Texture_Info map_reflection_cube_front;  //refl
    Obj_Texture_Info map_reflection_cube_back;   //refl
    Obj_Texture_Info map_reflection_cube_left;   //refl
    Obj_Texture_Info map_reflection_cube_right;  //refl

    Obj_Texture_Info map_pbr_rougness;           //Pr/map_Pr [options] [name]
    Obj_Texture_Info map_pbr_metallic;           //Pm/map_Pm
    Obj_Texture_Info map_pbr_sheen;              //Ps/map_Ps
    Obj_Texture_Info map_pbr_clearcoat_thickness;//Pc
    Obj_Texture_Info map_pbr_clearcoat_rougness; //Pcr
    Obj_Texture_Info map_pbr_emmisive;           //Ke/map_Ke
    Obj_Texture_Info map_pbr_anisotropy;         //aniso
    Obj_Texture_Info map_pbr_anisotropy_rotation;//anisor
    Obj_Texture_Info map_pbr_normal;             //norm

    Obj_Texture_Info map_pbr_rma; //RMA material (roughness, metalness, ambient occlusion) //map_RMA/map_ORM       
} Obj_Material_Info;

typedef struct Obj_Group {
    String_Builder object;          //o [String]
    String_Builder_Array groups;    //g [group1] [group2] ...
    String_Builder material;        //usemtl [String material_name]
    i32 smoothing_index;            //s [index > 0] //to set smoothing
                                    //s [0 | off]   //to set no smoothing
    f32 merge_group_resolution;     //mg [i32 smoothing_index] [f32 distance]

    //Each group has info and some number of trinagles.
    //Trinagles from this group are inside the shared trinagles array
    //with the indeces in range [trinagles_from, trinagles_from + trinagles_count)
    i32 trinagles_from;
    i32 trinagles_count;
} Obj_Group;

//one based indeces into the appropraite arrays.
//If value is 0 means not present.
typedef struct Obj_Vertex_Index
{
    i32 pos_i1;
    i32 uv_i1;
    i32 norm_i1;
} Obj_Vertex_Index;

DEFINE_ARRAY_TYPE(Triangle_Index_Array, Triangle_Index_Array_Array);
DEFINE_ARRAY_TYPE(Obj_Parser_Error, Obj_Parser_Error_Array);
DEFINE_ARRAY_TYPE(Obj_Group, Obj_Group_Array);
DEFINE_ARRAY_TYPE(Obj_Material_Info, Obj_Material_Info_Array);
DEFINE_ARRAY_TYPE(Obj_Vertex_Index, Obj_Vertex_Index_Array);

typedef struct Obj_Model {
    Allocator* alloc;
    Vec3_Array positions; 
    Vec2_Array uvs; 
    Vec3_Array normals;
    Obj_Vertex_Index_Array indeces;
    Obj_Group_Array groups;
    String_Builder_Array material_files;
} Obj_Model;

EXPORT void obj_texture_info_deinit(Obj_Texture_Info* info);
EXPORT void obj_material_info_deinit(Obj_Material_Info* info);
EXPORT void obj_group_deinit(Obj_Group* info);
EXPORT void obj_model_deinit(Obj_Model* info);

EXPORT void obj_texture_info_init(Obj_Texture_Info* info, Allocator* alloc);
EXPORT void obj_material_info_init(Obj_Material_Info* info, Allocator* alloc);
EXPORT void obj_group_init(Obj_Group* info, Allocator* alloc);
EXPORT void obj_model_init(Obj_Model* info, Allocator* alloc);


//Loads obj model along with all of its required materials
EXPORT Error obj_load(Obj_Model* out, String obj_source, Obj_Parser_Error_Array* output_errors_to_or_null, bool log_errors);

#endif

#if (defined(LIB_ALL_IMPL) || defined(LIB_OBJ_PARSER_IMPL)) && !defined(LIB_OBJ_PARSER_HAS_IMPL)
#define LIB_OBJ_PARSER_HAS_IMPL

EXPORT void obj_texture_info_deinit(Obj_Texture_Info* info)
{
    (void) info;
}
EXPORT void obj_material_info_deinit(Obj_Material_Info* info)
{
    (void) info;

}
EXPORT void obj_model_deinit(Obj_Model* info)
{
    (void) info;

}

EXPORT void obj_texture_info_init(Obj_Texture_Info* info, Allocator* alloc)
{
    (void) info, alloc;
}
EXPORT void obj_material_info_init(Obj_Material_Info* info, Allocator* alloc)
{
    (void) info, alloc;

}
EXPORT void obj_model_init(Obj_Model* info, Allocator* alloc)
{
    (void) info, alloc;

}

EXPORT const char* obj_parser_error_on_to_string(Obj_Parser_Error_On statement)
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
        case FORMAT_OBJ_ERROR_INVALID_INDEX: return "FORMAT_OBJ_ERROR_INVALID_INDEX";
        case FORMAT_MTL_ERROR_NONE: return "FORMAT_MTL_ERROR_NONE";
        case FORMAT_MTL_ERROR_NEWMTL: return "FORMAT_MTL_ERROR_NEWMTL";
        case FORMAT_MTL_ERROR_MISSING_NEWMTL: return "FORMAT_MTL_ERROR_MISSING_NEWMTL";
        case FORMAT_MTL_ERROR_COLOR_AMBIENT: return "FORMAT_MTL_ERROR_COLOR_AMBIENT";
        case FORMAT_MTL_ERROR_COLOR_DIFFUSE: return "FORMAT_MTL_ERROR_COLOR_DIFFUSE";
        case FORMAT_MTL_ERROR_COLOR_SPECULAR: return "FORMAT_MTL_ERROR_COLOR_SPECULAR";
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

EXPORT void obj_group_init(Obj_Group* info, Allocator* alloc)
{
    obj_group_deinit(info);
    array_init(&info->group, alloc);
    array_init(&info->object, alloc);
    array_init(&info->material, alloc);
}
EXPORT void obj_group_deinit(Obj_Group* info)
{
    array_deinit(&info->group);
    array_deinit(&info->object);
    array_deinit(&info->material);

    memset(info, 0, sizeof *info);
}

//EXPORT Error deduplicate_obj_data()
//{
//
//}

//@TODO: separate parsing and transforming to engine friendly format into two different things

INTERNAL Obj_Group* _obj_parser_add_group(Obj_Model* out, String active_object, String_Builder_Array* active_groups, isize vertex_index)
{
    if(out->groups.size != 0)
    {
        Obj_Group* last = array_last(out->groups);
        last->trinagles_count = vertex_index - last->trinagles_from;
    }

    Obj_Group new_group = {0};
    obj_group_init(&new_group, allocator_get_default());
    new_group.trinagles_from = vertex_index;
    new_group.groups = *active_groups;
    builder_assign(&new_group.object, active_object);

    String_Builder_Array null = {0};
    *active_groups = null;
    array_push(&out->groups, new_group);
    
    return array_last(out->groups);
}

INTERNAL Obj_Group* _obj_parser_get_active_group(Obj_Model* out, String active_object, isize vertex_index)
{
    //if the last group is still the active one
    if(out->groups.size != 0)
    {
        Obj_Group* last = array_last(out->groups);
        if(string_is_equal(string_from_builder(last->object), active_object))
            return last;   
    }
    
    //Add new default group
    //End the previous group

    String_Builder_Array def_groups = {0};
    array_push(&def_groups, builder_from_cstring("default"));

    return _obj_parser_add_group(out, active_object, &def_groups, vertex_index);
}

EXPORT bool obj_parser_parse(Obj_Model* out, String obj_source, Obj_Parser_Error* errors, isize errors_max_count, isize* had_errors)
{
    Allocator* alloc = out->alloc;
    if(alloc == NULL)
        alloc = allocator_get_default();

    obj_model_init(out, alloc);
    
    Allocator* scratch_alloc = allocator_get_scratch();
    Allocator* default_alloc = allocator_get_default();
    
    bool had_error = false;
    isize error_count = 0;
    
    //Try to guess the needed size based on a simple heurestic
    isize expected_line_count = obj_source.size / 32 + 128;
    array_reserve(&out->indeces, expected_line_count);
    array_reserve(&out->positions, expected_line_count);
    array_reserve(&out->uvs, expected_line_count);
    array_reserve(&out->normals, expected_line_count);
    array_init_backed(&out->groups, scratch_alloc, 64);

    String active_object = {0};
    Obj_Group* active_group = NULL;

    i32 trinagle_index = 0;

    //for loop to prevent infinite iteration bugs
    isize line_start_i = 0;
    isize line_end_i = -1;
    isize line_number = 0;
    for(isize ender_i = 0; ender_i < obj_source.size; ender_i++)
    {
        line_number += 1;
        line_start_i = line_end_i + 1;
        line_end_i = string_find_first_char(obj_source, '\n', line_start_i);

        //if is the last line stop at the next iteration
        if(line_end_i == -1)
        {
            ender_i = obj_source.size; 
            line_end_i = obj_source.size;
        }

        // Skip blank lines.
        if (line_end_i - line_start_i < 1) 
            continue;
        
        String line = string_range(obj_source, line_start_i, line_end_i);
        ASSERT(line.size > 0);

        Obj_Parser_Error_On error = FORMAT_OBJ_ERROR_NONE;

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

                Obj_Vertex_Index indeces[3] = {0};
                
                bool ok = false;
                isize line_index = 1;
                //tries first the ones based on our analysis and then the rest until valid is found
                if(has_double_slash)
                {
                    ok = true
                            && match_whitespace(line, &line_index)
                            && match_decimal_i32(line, &line_index, &indeces[0].pos_i1)
                            && match_sequence(line, &line_index, STRING("//"))
                            && match_decimal_i32(line, &line_index, &indeces[0].norm_i1)
                            && match_whitespace(line, &line_index)
                            && match_decimal_i32(line, &line_index, &indeces[1].pos_i1)
                            && match_sequence(line, &line_index, STRING("//"))
                            && match_decimal_i32(line, &line_index, &indeces[1].norm_i1)
                            && match_whitespace(line, &line_index)
                            && match_decimal_i32(line, &line_index, &indeces[2].pos_i1)
                            && match_sequence(line, &line_index, STRING("//"))
                            && match_decimal_i32(line, &line_index, &indeces[2].norm_i1);
                }
                else if(!has_slash)
                {
                    ok = true
                            && match_whitespace(line, &line_index)
                            && match_decimal_i32(line, &line_index, &indeces[0].pos_i1)
                            && match_whitespace(line, &line_index)
                            && match_decimal_i32(line, &line_index, &indeces[1].pos_i1)
                            && match_whitespace(line, &line_index)
                            && match_decimal_i32(line, &line_index, &indeces[2].pos_i1);
                }
                
                if(!ok)
                {
                    ok = true
                            && match_whitespace(line, &line_index)
                            && match_decimal_i32(line, &line_index, &indeces[0].pos_i1)
                            && match_char(line, &line_index, '/')
                            && match_decimal_i32(line, &line_index, &indeces[0].uv_i1)
                            && match_char(line, &line_index, '/')
                            && match_decimal_i32(line, &line_index, &indeces[0].norm_i1)
                            && match_whitespace(line, &line_index)
                            && match_decimal_i32(line, &line_index, &indeces[1].pos_i1)
                            && match_char(line, &line_index, '/')
                            && match_decimal_i32(line, &line_index, &indeces[1].uv_i1)
                            && match_char(line, &line_index, '/')
                            && match_decimal_i32(line, &line_index, &indeces[1].norm_i1)
                            && match_whitespace(line, &line_index)
                            && match_decimal_i32(line, &line_index, &indeces[2].pos_i1)
                            && match_char(line, &line_index, '/')
                            && match_decimal_i32(line, &line_index, &indeces[2].uv_i1)
                            && match_char(line, &line_index, '/')
                            && match_decimal_i32(line, &line_index, &indeces[2].norm_i1);
                }
                
                if(!ok)
                {
                    ok = true
                            && match_whitespace(line, &line_index)
                            && match_decimal_i32(line, &line_index, &indeces[0].pos_i1)
                            && match_char(line, &line_index, '/')
                            && match_decimal_i32(line, &line_index, &indeces[0].uv_i1)
                            && match_whitespace(line, &line_index)
                            && match_decimal_i32(line, &line_index, &indeces[1].pos_i1)
                            && match_char(line, &line_index, '/')
                            && match_decimal_i32(line, &line_index, &indeces[1].uv_i1)
                            && match_whitespace(line, &line_index)
                            && match_decimal_i32(line, &line_index, &indeces[2].pos_i1)
                            && match_char(line, &line_index, '/')
                            && match_decimal_i32(line, &line_index, &indeces[2].uv_i1);
                }

                if(!ok)
                    error = FORMAT_OBJ_ERROR_FACE;

                //correct negative values indeces. If index is negative it refers to the -i-nth last parsed
                // value in the given category. If the category does not recieve data (NULL) set the index to 0
                for(isize i = 0; i < 3; i++)
                {
                    Obj_Vertex_Index index = indeces[i];
                    if(index.pos_i1 < 0)
                        index.pos_i1 = (u32) out->positions.size + index.pos_i1 + 1;
                    
                    if(index.uv_i1 < 0)
                        index.uv_i1 = (u32) out->uvs.size + index.uv_i1 + 1;
                        
                    if(index.norm_i1 < 0)
                        index.norm_i1 = (u32) out->normals.size + index.norm_i1 + 1;
                }

                array_push(&out->indeces, indeces[0]); 
                array_push(&out->indeces, indeces[1]); 
                array_push(&out->indeces, indeces[2]);

                trinagle_index += 1;
            } break;
            
            //Smoothing
            case 's': {
                isize line_index1 = 1;
                i64 smoothing_index = 0;
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
                String_Builder_Array groups = {default_alloc};
                isize line_index = 1;
                while(true)
                {
                    bool matches = match_whitespace(line, &line_index);
                    isize group_from = line_index;
                    isize group_to = line_index;
                    matches = matches && match_whitespace_custom(line, &group_to, MATCH_INVERTED);

                    if(matches)
                    {
                        String group = string_range(line, group_from, group_to);
                        array_push(&groups, builder_from_string_alloc(group, default_alloc));
                    }
                    else
                    {
                        break;
                    }
                }

                if(groups.size > 0)
                    active_group = _obj_parser_add_group(out, active_object, &groups, trinagle_index);
                else
                    error = FORMAT_OBJ_ERROR_GROUP;

                builder_array_deinit(&groups);
            } break;

            //Object: g [object_name] ...
            case 'o': {
                isize line_index = 1;
                bool matches = match_whitespace(line, &line_index);

                isize object_from = line_index;
                isize object_to = line_index;
                matches = matches && match_whitespace_custom(line, &object_to, MATCH_INVERTED);

                if(matches)
                    active_object = string_range(line, object_from, object_to);
                else
                    error = FORMAT_OBJ_ERROR_OBJECT;
            } break;

            //Material library
            case 'm': {

                isize line_index = 0;
                bool matches = match_sequence(line, &line_index, STRING("mtllib"))
                    && match_whitespace(line, &line_index);
                    
                isize from_index = line_index;
                isize to_index = line_index;
                matches = matches && match_whitespace_custom(line, &to_index, MATCH_INVERTED);

                if(matches)
                {
                    String material_lib = string_range(line, from_index, to_index);
                    array_push(&out->material_files, builder_from_string(material_lib));
                }
                else
                {
                    error = FORMAT_OBJ_ERROR_MATERIAL_LIBRARY;
                }
            } break;
            
            //use material
            case 'u': {
                isize line_index = 0;
                bool matches = match_sequence(line, &line_index, STRING("usemtl"))
                    && match_whitespace(line, &line_index);
                    
                isize from_index = line_index;
                isize to_index = line_index;
                matches = matches && match_whitespace_custom(line, &to_index, MATCH_INVERTED);

                if(matches)
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
            Obj_Parser_Error parser_error = {0};
            parser_error.index = line_start_i;
            parser_error.line = (i32) line_number;
            parser_error.statement = error;
            parser_error.unimplemented = false;

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
    return had_error;
}

bool match_space_separated_vec3(String str, isize* index, Vec3* matched)
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

//matches the sequence:x (y (z)) where x, y, z are f32 and () means optional
bool match_space_separated_optional_vec3(String str, isize* index, Vec3* matched)
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

EXPORT bool obj_parser_parse_mtl(Obj_Material_Info_Array* out, String mtl_source, Obj_Parser_Error* errors, isize errors_max_count, isize* had_errors)
{
    Allocator* def_alloc = allocator_get_default();
    Obj_Material_Info* material = NULL;
    isize error_count = 0;
    bool had_error = false;

    isize line_start_i = 0;
    isize line_end_i = -1;
    isize line_number = 0;
    for(isize ender_i = 0; ender_i < mtl_source.size; ender_i++)
    {
        line_number += 1;
        line_start_i = line_end_i + 1;
        line_end_i = string_find_first_char(mtl_source, '\n', line_start_i);
        
        //if is the last line stop at the next iteration
        if(line_end_i == -1)
        {
            ender_i = mtl_source.size; 
            line_end_i = mtl_source.size;
        }

        String line = string_range(mtl_source, line_start_i, line_end_i);
        isize skipped_whitespace = 0;
        match_whitespace(line, &skipped_whitespace);

        line = string_tail(line, skipped_whitespace);

        //skip blank lines
        if(line.size == 0)
            continue;

        Obj_Parser_Error_On error = FORMAT_MTL_ERROR_NONE;
        isize i = 0;
        Vec3 vec = {0};

        if(i = 0, match_sequence(line, &i, STRING("newmtl")))
        {
            match_whitespace(line, &i);
            isize name_start = i;
            isize name_end = i;
            match_whitespace_custom(line, &name_end, MATCH_INVERTED);

            String name = string_range(line, name_start, name_end);
            if(name.size == 0)
                error = FORMAT_MTL_ERROR_NEWMTL;
            else
            {
                Obj_Material_Info new_material = {0};
                obj_material_info_init(&new_material, def_alloc);
                builder_assign(&new_material.name, name);

                array_push(out, new_material);
                material = array_last(*out);
            }
        }
        else if(i = 0, match_sequence(line, &i, STRING("#")))
        {
            //comment => do nothing
        }
        else if(material == NULL)
        {
            error = FORMAT_MTL_ERROR_MISSING_NEWMTL;
        }
        else if(i = 0, match_sequence(line, &i, STRING("Ka")))
        {
            if(match_whitespace(line, &i) && match_space_separated_vec3(line, &i, &vec))
                material->ambient_color = vec;
            else
                error = FORMAT_MTL_ERROR_COLOR_AMBIENT;
        }
        else if(i = 0, match_sequence(line, &i, STRING("Kd")))
        {
            if(match_whitespace(line, &i) && match_space_separated_vec3(line, &i, &vec))
                material->diffuse_color = vec;
            else
                error = FORMAT_MTL_ERROR_COLOR_DIFFUSE;
        }
        else if(i = 0, match_sequence(line, &i, STRING("Ks")))
        {
            if(match_whitespace(line, &i) && match_space_separated_vec3(line, &i, &vec))
                material->specular_color = vec;
            else
                error = FORMAT_MTL_ERROR_COLOR_SPECULAR;
        }
        else if(i = 0, match_sequence(line, &i, STRING("Ns"))) //d=0 => transparent
        {
            f32 val = 0;
            if(match_whitespace(line, &i) && match_decimal_f32(line, &i, &val))
                material->specular_exponent = val;
            else
                error = FORMAT_MTL_ERROR_SPECULAR_EXPOMENT;
        }
        else if(i = 0, match_sequence(line, &i, STRING("d"))) //d=0 => transparent
        {
            f32 val = 0;
            if(match_whitespace(line, &i) && match_decimal_f32(line, &i, &val))
                material->opacity = val;
            else
                error = FORMAT_MTL_ERROR_OPACITY;
        }
        else if(i = 0, match_sequence(line, &i, STRING("Tr"))) //Tr=1 => transparent
        {
            f32 val = 0;
            if(match_whitespace(line, &i) && match_decimal_f32(line, &i, &val))
                material->opacity = 1.0f - val;
            else
                error = FORMAT_MTL_ERROR_OPACITY;
        }
        else if(i = 0, match_sequence(line, &i, STRING("Tf")))
        {
            if(match_whitespace(line, &i) && match_space_separated_vec3(line, &i, &vec))
                material->only_for_transparent_transmission_filter_color = vec;
            else
                error = FORMAT_MTL_ERROR_TRANSMISSION_FILTER;
        }
        else if(i = 0, match_sequence(line, &i, STRING("Ni")))
        {
            f32 val = 0;
            if(match_whitespace(line, &i) && match_decimal_f32(line, &i, &val))
                material->only_for_transparent_optical_density = val;
            else
                error = FORMAT_MTL_ERROR_TRANSMISSION_OPTICAL_DENSITY;
        }
        else if(i = 0, match_sequence(line, &i, STRING("illum")))
        {
            i64 val = 0;
            if(match_whitespace(line, &i) && match_decimal_u64(line, &i, &val)
                && MTL_ILLUM_MIN <= val && val <= MTL_ILLUM_MAX)
                    material->illumination_mode = (i32) val;
            else
                error = FORMAT_MTL_ERROR_ILLUMINATION_MODE;
        }
        else
        {
            typedef struct {
                Obj_Texture_Info* tex_info;
                const char* sequences[2];
            } Mtl_Map_Table_Entry;

            //try to match any remaining statements as map kinds. 
            //We use a table to model this because all of the options have the same syntax
            Mtl_Map_Table_Entry map_table[] = {
                    {&material->map_ambient,                 "map_Ka"},
                    {&material->map_ambient_diffuse,         "map_Kd"},
                    {&material->map_specular_color,          "map_Ks"},
                    {&material->map_specular_highlight,      "map_Ns "},
                    {&material->map_alpha,                   "map_d"},
                    {&material->map_bump,                    "map_bump", "bump"},
                    {&material->map_displacement,            "disp"},
                    {&material->map_stencil,                 "decal"},
                    {&material->map_reflection_spehere,      "refl "},

                    //@TODO: Ke and map_Ke are different and so are probably the rest of these.
                    //       make the parsing proper!
                    {&material->map_pbr_rougness,            "Pr", "map_Pr"},
                    {&material->map_pbr_metallic,            "Pm", "map_Pm"},
                    {&material->map_pbr_sheen,               "Ps", "map_Ps"},
                    {&material->map_pbr_clearcoat_thickness, "Pc"},
                    {&material->map_pbr_clearcoat_rougness,  "Pcr"},
                    {&material->map_pbr_emmisive,            "Ke", "map_Ke"},
                    {&material->map_pbr_anisotropy,          "aniso"},
                    {&material->map_pbr_anisotropy_rotation, "anisor"},
                    {&material->map_pbr_normal,              "norm"},
                    {&material->map_pbr_rma,                 "map_RMA", "map_ORM"},
            };

            //try to match any of the sequences from the above table
            isize map_table_matched_i = -1;
            for(isize k = 0; k < STATIC_ARRAY_SIZE(map_table); k++)
            {
                Mtl_Map_Table_Entry map = map_table[k];
                for(isize j = 0; j < 2; j++)
                {
                    String str_sequence = string_make(map.sequences[j]);
                    if(str_sequence.size != 0 && match_sequence(line, &i, str_sequence))
                    {
                        map_table_matched_i = k;
                        break;
                    }
                }
            }

            //if found match try to parse arguments
            bool matched = map_table_matched_i != -1;
            if(!matched)
                error = FORMAT_MTL_ERROR_OTHER;
            else
            {
                Mtl_Map_Table_Entry map_entry = map_table[map_table_matched_i];
                Obj_Texture_Info* tex_info = map_entry.tex_info;
                Obj_Texture_Info temp_tex_info = {0};
                obj_texture_info_init(&temp_tex_info, def_alloc);

                while(matched && i < line.size)
                {
                    matched = match_whitespace(line, &i);
                    isize arg_from = i;
                    String arg = string_tail(line, arg_from);
                    isize arg_i = 0;
                        
                    Vec3 arg_vec = {0};
                    //offset
                    if(arg_i = 0, match_sequence(arg, &arg_i, STRING("-o")))
                    {
                        if(match_whitespace(arg, &arg_i) && match_space_separated_optional_vec3(arg, &arg_i, &arg_vec))
                            temp_tex_info.offset = arg_vec;
                        else
                            matched = false;
                    }
                    //scale
                    else if(arg_i = 0, match_sequence(arg, &arg_i, STRING("-s")))
                    {
                        
                        if(match_whitespace(arg, &arg_i) && match_space_separated_optional_vec3(arg, &arg_i, &arg_vec))
                            temp_tex_info.scale = arg_vec;
                        else
                            matched = false;
                    }
                    //turbulance
                    else if(arg_i = 0, match_sequence(arg, &arg_i, STRING("-t")))
                    {
                        
                        if(match_whitespace(arg, &arg_i) && match_space_separated_optional_vec3(arg, &arg_i, &arg_vec))
                            temp_tex_info.turbulance = arg_vec;
                        else
                            matched = false;
                    }
                    //texture_resolution
                    else if(arg_i = 0, match_sequence(arg, &arg_i, STRING("-texres")))
                    {
                        i64 res = 0;
                        if(match_whitespace(arg, &arg_i) && match_decimal_u64(arg, &arg_i, &res))
                            temp_tex_info.texture_resolution = (i32) res;
                        else
                            matched = false;
                    }
                    //mipmap_sharpness_boost
                    else if(arg_i = 0, match_sequence(arg, &arg_i, STRING("-boost")))
                    {
                        f32 val = 0;
                        if(match_whitespace(arg, &arg_i) && match_decimal_f32(arg, &arg_i, &val))
                            temp_tex_info.mipmap_sharpness_boost = val;
                        else
                            matched = false;
                    }
                    //bump_multiplier
                    else if(arg_i = 0, match_sequence(arg, &arg_i, STRING("-bm")))
                    {
                        f32 val = 0;
                        if(match_whitespace(arg, &arg_i) && match_decimal_f32(arg, &arg_i, &val))
                            temp_tex_info.bump_multiplier = val;
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
                            temp_tex_info.modify_brigthness = modify_brigthness;
                            temp_tex_info.modify_contrast = modify_contrast;
                        }
                        else
                            matched = false;
                    }
                    //is_repeating
                    else if(arg_i = 0, match_sequence(arg, &arg_i, STRING("-clamp")))
                    {
                        bool had_space = match_whitespace(arg, &arg_i);
                        if(had_space && match_sequence(arg, &arg_i, STRING("on")))
                            temp_tex_info.is_repeating = false;
                        else if(had_space && match_sequence(arg, &arg_i, STRING("off")))
                            temp_tex_info.is_repeating = true;
                        else
                            matched = false;
                    }
                    //blend_u
                    else if(arg_i = 0, match_sequence(arg, &arg_i, STRING("-blendu")))
                    {
                        bool had_space = match_whitespace(arg, &arg_i);
                        if(had_space && match_sequence(arg, &arg_i, STRING("on")))
                            temp_tex_info.blend_u = false;
                        else if(had_space && match_sequence(arg, &arg_i, STRING("off")))
                            temp_tex_info.blend_u = true;
                        else
                            matched = false;
                    }
                    //blend_v
                    else if(arg_i = 0, match_sequence(arg, &arg_i, STRING("-blendv")))
                    {
                        bool had_space = match_whitespace(arg, &arg_i);
                        if(had_space && match_sequence(arg, &arg_i, STRING("on")))
                            temp_tex_info.blend_v = false;
                        else if(had_space && match_sequence(arg, &arg_i, STRING("off")))
                            temp_tex_info.blend_v = true;
                        else
                            matched = false;
                    }
                    //-type sphere 
                    else if(arg_i = 0, match_sequence(arg, &arg_i, STRING("-type")))
                    {
                        ASSERT(map_entry.sequences[0] != NULL);
                        //if is not on refl map then error
                        if(strcmp(map_entry.sequences[0], "refl") != 0)
                            matched = false;
                        else
                        {
                            bool had_space = match_whitespace(arg, &arg_i);
                            if(had_space && match_sequence(arg, &arg_i, STRING("spehre")))
                                tex_info = &material->map_reflection_spehere;
                            else if(had_space && match_sequence(arg, &arg_i, STRING("cube_top")))
                                tex_info = &material->map_reflection_cube_top;
                            else if(had_space && match_sequence(arg, &arg_i, STRING("cube_bottom")))
                                tex_info = &material->map_reflection_cube_bottom;
                            else if(had_space && match_sequence(arg, &arg_i, STRING("cube_front")))
                                tex_info = &material->map_reflection_cube_front;
                            else if(had_space && match_sequence(arg, &arg_i, STRING("cube_back")))
                                tex_info = &material->map_reflection_cube_back;
                            else if(had_space && match_sequence(arg, &arg_i, STRING("cube_left")))
                                tex_info = &material->map_reflection_cube_left;
                            else if(had_space && match_sequence(arg, &arg_i, STRING("cube_right")))
                                tex_info = &material->map_reflection_cube_right;
                            else
                                matched = false;
                        }
                    }
                    //use_channel
                    //-imfchan [r | g | b | m | l | z]  
                    else if(arg_i = 0, match_sequence(arg, &arg_i, STRING("-imfchan")))
                    {
                        bool matched_arg = match_whitespace(arg, &arg_i);
                        char c = 0;
                        if(matched_arg && arg.size - arg_i > 1)
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
                                    temp_tex_info.use_channel = c;
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
                    

                    ASSERT_MSG(arg_i > 0, "will make progress");
                    i += arg_i;
                }

                obj_texture_info_deinit(tex_info);
                *tex_info = temp_tex_info;

                isize filename_from = i;
                isize filename_to = line.size;

                for(; filename_to-- > filename_from; )
                    if(char_is_space(line.data[filename_to]) == false)
                        break;

                if(filename_to < filename_from)
                    filename_to = filename_from;

                String filename = string_range(line, filename_from, filename_to);
                if(filename.size > 0)
                    builder_assign(&tex_info->path, filename);
                else
                    matched = FORMAT_MTL_ERROR_MAP;
            }

        }
    
        if(error != FORMAT_MTL_ERROR_NONE)
        {
            Obj_Parser_Error parser_error = {0};
            parser_error.index = line_start_i;
            parser_error.line = (i32) line_number;
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