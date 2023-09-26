#ifndef LIB_OBJ_PARSER
#define LIB_OBJ_PARSER

#include "math.h"
#include "array.h"
#include "string.h"
#include "format.h"
#include "log.h"
#include "error.h"
#include "shapes.h"
#include "profile.h"
#include "parse.h"

#define OBJ_LOADER_CHANNEL "ASSET"

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
    OBJ_PARSER_ERROR_NONE = 0,
    FORMAT_MTL_ERROR_NONE = 1,

    OBJ_PARSER_ERROR_FACE,
    OBJ_PARSER_ERROR_COMMENT,
    OBJ_PARSER_ERROR_VERTEX_POS,
    OBJ_PARSER_ERROR_VERTEX_NORM,
    OBJ_PARSER_ERROR_VERTEX_UV,
    OBJ_PARSER_ERROR_MATERIAL_LIBRARY,
    OBJ_PARSER_ERROR_MATERIAL_USE,
    OBJ_PARSER_ERROR_POINT,
    OBJ_PARSER_ERROR_LINE,
    OBJ_PARSER_ERROR_GROUP,
    OBJ_PARSER_ERROR_SMOOTH_SHADING,
    OBJ_PARSER_ERROR_OBJECT,
    OBJ_PARSER_ERROR_INVALID_INDEX,
    OBJ_PARSER_ERROR_OTHER,
    
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

typedef enum Obj_Illumination_Mode
{
    OBJ_ILLUMINATION_COLOR_AND_AMBIENT_OFF = 0,
    OBJ_ILLUMINATION_COLOR_AND_AMBIENT_ON = 1,
    OBJ_ILLUMINATION_HIGHLIGH_ON = 2,
} Obj_Illumination_Mode;

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
    String_Builder object;  //o [String]
    String_Builder group;  //g [space separeted groups names]
    String_Builder material; //usemtl [String material_name]
    i32 smoothing_index;    //s [index > 0] //to set smoothing
                            //s [0 | off]   //to set no smoothing
    f32 merge_group_resolution; //mg [i32 smoothing_index] [f32 distance]

    //Each group has info and some number of trinagles.
    //Trinagles from this group are inside the shared trinagles array
    //with the indeces in range [trinagles_from, trinagles_from + trinagles_count)
    i32 trinagles_from;
    i32 trinagles_count;
} Obj_Group;

DEFINE_ARRAY_TYPE(Triangle_Index_Array, Triangle_Index_Array_Array);
DEFINE_ARRAY_TYPE(Obj_Parser_Error, Obj_Parser_Error_Array);
DEFINE_ARRAY_TYPE(Obj_Group, Obj_Group_Array);
DEFINE_ARRAY_TYPE(Obj_Material_Info, Obj_Material_Info_Array);

typedef struct Obj_Model {
    Vertex_Array vertices;
    Hash_Index vertices_hash;
    Triangle_Index_Array triangles;
    Obj_Group_Array groups;
    Obj_Material_Info_Array materials;
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
        case OBJ_PARSER_ERROR_NONE: return "OBJ_PARSER_ERROR_NONE";
        case OBJ_PARSER_ERROR_FACE: return "OBJ_PARSER_ERROR_FACE";
        case OBJ_PARSER_ERROR_COMMENT: return "OBJ_PARSER_ERROR_COMMENT";
        case OBJ_PARSER_ERROR_VERTEX_POS: return "OBJ_PARSER_ERROR_VERTEX_POS";
        case OBJ_PARSER_ERROR_VERTEX_NORM: return "OBJ_PARSER_ERROR_VERTEX_NORM";
        case OBJ_PARSER_ERROR_VERTEX_UV: return "OBJ_PARSER_ERROR_VERTEX_UV";
        case OBJ_PARSER_ERROR_MATERIAL_LIBRARY: return "OBJ_PARSER_ERROR_MATERIAL_LIBRARY";
        case OBJ_PARSER_ERROR_MATERIAL_USE: return "OBJ_PARSER_ERROR_MATERIAL_USE";
        case OBJ_PARSER_ERROR_POINT: return "OBJ_PARSER_ERROR_POINT";
        case OBJ_PARSER_ERROR_LINE: return "OBJ_PARSER_ERROR_LINE";
        case OBJ_PARSER_ERROR_GROUP: return "OBJ_PARSER_ERROR_GROUP";
        case OBJ_PARSER_ERROR_SMOOTH_SHADING: return "OBJ_PARSER_ERROR_SMOOTH_SHADING";
        case OBJ_PARSER_ERROR_OBJECT: return "OBJ_PARSER_ERROR_OBJECT";
        case OBJ_PARSER_ERROR_INVALID_INDEX: return "OBJ_PARSER_ERROR_INVALID_INDEX";
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
        case OBJ_PARSER_ERROR_OTHER: return "OBJ_PARSER_ERROR_OTHER";
    }
}

INTERNAL String obj_parser_translate_error(u32 code, void* context)
{
    (void) context;
    enum {LOCAL_BUFF_SIZE = 256, MAX_ERRORS = 4};
    static int error_index = 0;
    static char error_strings[MAX_ERRORS][LOCAL_BUFF_SIZE + 1] = {0};

    int line = code >> 8;
    int error_flag = code & 0xFF;

    const char* error_flag_str = obj_parser_error_on_to_string((Obj_Parser_Error_On) error_flag);

    memset(error_strings[error_index], 0, LOCAL_BUFF_SIZE);
    snprintf(error_strings[error_index], LOCAL_BUFF_SIZE, "%s line: %d", error_flag_str, line);
    String out = string_make(error_strings[error_index]);
    error_index += 1;
    return out;
}

EXPORT u32 obj_parser_error_module()
{
    static u32 error_module = 0;
    if(error_module == 0)
        error_module = error_system_register_module(obj_parser_translate_error, STRING("obj_parser.h"), NULL);

    return error_module;
}


EXPORT Error obj_parser_error_on_to_error(Obj_Parser_Error_On statement, isize line)
{
    u32 splatted = (u32) line << 8 | (u32) statement;
    return error_make(obj_parser_error_module(), splatted);
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

EXPORT Error obj_parser_parse(Obj_Model* out, String obj_source, Obj_Parser_Error_Array* output_errors_to_or_null, bool log_errors)
{
    test_match();

    log_group_push();
    PERF_COUNTER_START(c);
    
    PERF_COUNTER_START(parsing_counter);

    //one based indeces into the appropraite arrays.
    //If value is 0 means not present.
    typedef struct Obj_Vertex_Index
    {
        i32 pos_i1;
        i32 uv_i1;
        i32 norm_i1;
    } Obj_Vertex_Index;

    DEFINE_ARRAY_TYPE(Obj_Vertex_Index, Obj_Vertex_Index_Array);
    
    Allocator* scratch_alloc = allocator_get_scratch();
    Allocator* default_alloc = allocator_get_default();
    
    isize num_allocations_before = allocator_get_stats(allocator_get_scratch()).allocation_count;

    //Init temporary buffers
    Error had_error = {0};
    isize error_count = 0;
    
    Obj_Group_Array temp_groups = {scratch_alloc};
    Obj_Vertex_Index_Array obj_indeces_array = {scratch_alloc};
    Vec3_Array pos_array = {scratch_alloc}; 
    Vec2_Array uv_array = {scratch_alloc}; 
    Vec3_Array norm_array = {scratch_alloc};

    array_init_backed(&temp_groups, scratch_alloc, 64);
    
    //Try to guess the needed size based on a simple heurestic
    isize expected_line_count = obj_source.size / 32 + 128;
    array_reserve(&obj_indeces_array, expected_line_count);
    array_reserve(&pos_array, expected_line_count);
    array_reserve(&uv_array, expected_line_count);
    array_reserve(&norm_array, expected_line_count);

    String active_object = {0};
    Obj_Group active_group = {0};
    obj_group_init(&active_group, scratch_alloc);
    builder_assign(&active_group.group, STRING("default"));
    builder_assign(&active_group.object, STRING("default"));

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

        Obj_Parser_Error_On error = OBJ_PARSER_ERROR_NONE;

        char first_char = line.data[0];
        switch (first_char) 
        {
            case '#': {
                // Skip comments
                continue;
            } break;

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
                            error = OBJ_PARSER_ERROR_VERTEX_POS;
                        else
                            array_push(&pos_array, pos);
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
                            error = OBJ_PARSER_ERROR_VERTEX_NORM;
                        else
                            array_push(&norm_array, norm);
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
                            error = OBJ_PARSER_ERROR_VERTEX_UV;
                        else 
                            array_push(&uv_array, tex_coord);
                    } break;

                    default: {
                        error = OBJ_PARSER_ERROR_OTHER;
                    }
                }
            } break;

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
                    error = OBJ_PARSER_ERROR_FACE;

                //correct negative values indeces. If index is negative it refers to the -i-nth last parsed
                // value in the given category. If the category does not recieve data (NULL) set the index to 0
                for(isize i = 0; i < 3; i++)
                {
                    Obj_Vertex_Index index = indeces[i];
                    if(index.pos_i1 < 0)
                        index.pos_i1 = (u32) pos_array.size + index.pos_i1 + 1;
                    
                    if(index.uv_i1 < 0)
                        index.uv_i1 = (u32) uv_array.size + index.uv_i1 + 1;
                        
                    if(index.norm_i1 < 0)
                        index.norm_i1 = (u32) norm_array.size + index.norm_i1 + 1;
                }

                array_push(&obj_indeces_array, indeces[0]); 
                array_push(&obj_indeces_array, indeces[1]); 
                array_push(&obj_indeces_array, indeces[2]);

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
                    active_group.smoothing_index = (i32) smoothing_index;
                }
                else
                {
                    error = OBJ_PARSER_ERROR_SMOOTH_SHADING;
                }
            } break;
            
            //Group: g [group1] [group2] ...
            case 'g': {
                //We permit only a single group owner.
                //We simply ignore further group names.
                //All trinagles are owned by a single object.

                isize line_index = 1;
                bool matches = match_whitespace(line, &line_index);

                isize group_from = line_index;
                isize group_to = line_index;
                matches = matches && match_whitespace_custom(line, &group_to, MATCH_INVERTED);

                if(matches)
                {
                    active_group.trinagles_count = trinagle_index - active_group.trinagles_from;
                    array_push(&temp_groups, active_group);

                    Obj_Group new_group = {0};
                    obj_group_init(&new_group, scratch_alloc);
                    new_group.trinagles_from = trinagle_index;
                        
                    String group = string_range(line, group_from, group_to);
                    builder_assign(&new_group.group, group);
                    builder_assign(&new_group.object, active_object);

                    active_group = new_group;
                }
                else
                {
                    error = OBJ_PARSER_ERROR_GROUP;
                }
            } break;

            //Object: g [object_name] ...
            case 'o': {
            
                isize line_index = 1;
                bool matches = match_whitespace(line, &line_index);

                isize object_from = line_index;
                isize object_to = line_index;
                matches = matches && match_whitespace_custom(line, &object_to, MATCH_INVERTED);

                if(matches)
                {
                    active_group.trinagles_count = trinagle_index - active_group.trinagles_from;
                    array_push(&temp_groups, active_group);

                    Obj_Group new_group = {0};
                    obj_group_init(&new_group, scratch_alloc);
                    new_group.trinagles_from = trinagle_index;
                    
                    String object = string_range(line, object_from, object_to);
                    builder_assign(&new_group.group, STRING("default"));
                    builder_assign(&new_group.object, object);
                    active_group = new_group;
                    active_object = object;
                }
                else
                {
                    error = OBJ_PARSER_ERROR_OBJECT;
                }
            } break;

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
                    error = OBJ_PARSER_ERROR_MATERIAL_LIBRARY;
                }
            } break;
            
            case 'u': {
                isize line_index = 0;
                bool matches = match_sequence(line, &line_index, STRING("usemtl"))
                    && match_whitespace(line, &line_index);
                    
                isize from_index = line_index;
                isize to_index = line_index;
                matches = matches && match_whitespace_custom(line, &to_index, MATCH_INVERTED);

                if(matches)
                {
                    String material_use = string_range(line, from_index, to_index);
                    builder_assign(&active_group.material, material_use);
                }
                else
                {
                    error = OBJ_PARSER_ERROR_MATERIAL_USE;
                }
            } break;

            default: {
                error = OBJ_PARSER_ERROR_OTHER;
            }
        }

        //Handle errors
        if(error)
        {
            bool unimplemented = false;
            if(error_is_ok(had_error))
                had_error = obj_parser_error_on_to_error(error, line_number);

            if(output_errors_to_or_null)
            {
                Obj_Parser_Error parser_error = {0};
                parser_error.index = line_start_i;
                parser_error.line = (i32) line_number;
                parser_error.statement = error;
                parser_error.unimplemented = unimplemented;
                array_push(output_errors_to_or_null, parser_error);
            }

            if(log_errors && error_count <= OBJ_PARSER_MAX_ERRORS_TO_PRINT)
            {
                const char* error_text = obj_parser_error_on_to_string(error);
                LOG_WARN(OBJ_LOADER_CHANNEL, "malformed statement %s on line %lld:\n" STRING_FMT, error_text, (lld) line_number, STRING_PRINT(line));
            }
            
            if(log_errors && error_count == OBJ_PARSER_MAX_ERRORS_TO_PRINT)
            {
                LOG_WARN(OBJ_LOADER_CHANNEL, "file contains more than %lld errors. Stopping to print.", (lld) error_count);
            }

            error_count++;
        }
    } 
    
    active_group.trinagles_count = trinagle_index - active_group.trinagles_from;
    array_push(&temp_groups, active_group);

    PERF_COUNTER_END(parsing_counter);
    PERF_COUNTER_START(processing_counter);
    
    //Iterate all indeces using a hash map to deduplicate vertex data
    //and shape it into the vertex structure. 
    //Discards all triangles that dont have a valid position
    Hash_Index shape_assembly = {allocator_get_scratch()};
    hash_index_reserve(&shape_assembly, obj_indeces_array.size);

    //Try to guess the final needed size
    array_reserve(&out->triangles, out->triangles.size + obj_indeces_array.size/3); //divided by three since are triangles
    array_reserve(&out->vertices, out->vertices.size + obj_indeces_array.size/2); //divide by two since some vertices are shared
    
    bool discard_triangle = false;
    Triangle_Index triangle = {0};

    
    ASSERT_MSG(active_group.trinagles_from + active_group.trinagles_count == obj_indeces_array.size / 3, "the groups must contain all indeces!");

    //Deduplicate groups
    for(isize parent_group_i = 0; parent_group_i < temp_groups.size; parent_group_i++)
    {
        Obj_Group* parent_group = &temp_groups.data[parent_group_i];
        i32 triangles_before = (i32) out->triangles.size;
        
        //Skip empty groups
        if(parent_group->trinagles_count <= 0)
            continue;

        for(isize child_group_i = parent_group_i; child_group_i < temp_groups.size; child_group_i++)
        {
            Obj_Group* child_group = &temp_groups.data[child_group_i];

            //Skip groups that are empty 
            if(child_group->trinagles_count <= 0)
                continue;

            //... and those that are not the same group
            if(!builder_is_equal(child_group->group, parent_group->group)
               || !builder_is_equal(child_group->object, parent_group->object))
                continue;

            isize i_min = (child_group->trinagles_from)*3;
            isize i_max = (child_group->trinagles_from + child_group->trinagles_count)*3;
        
            //Deduplicate and parse vertices
            for(isize i = i_min; i < i_max; i++)
            {
                Vertex composed_vertex = {0};
                Obj_Vertex_Index index = obj_indeces_array.data[i];
                if(index.norm_i1 < 0 || index.norm_i1 > norm_array.size)
                {
                    if(log_errors && error_count <= OBJ_PARSER_MAX_ERRORS_TO_PRINT)
                        LOG_ERROR(OBJ_LOADER_CHANNEL, "norm index number %lld was out of range with value: %lld", (lld) i / 3, (lld) index.norm_i1);
                    had_error = obj_parser_error_on_to_error(OBJ_PARSER_ERROR_INVALID_INDEX, 0);
                }
                else
                {
                    composed_vertex.norm = norm_array.data[index.norm_i1 - 1];
                }

                if(index.pos_i1 < 0 || index.pos_i1 > pos_array.size)
                {
                    if(log_errors && error_count <= OBJ_PARSER_MAX_ERRORS_TO_PRINT)
                        LOG_ERROR(OBJ_LOADER_CHANNEL, "pos index number %lld was out of range with value: %lld", (lld) i / 3, (lld) index.pos_i1);
                    had_error = obj_parser_error_on_to_error(OBJ_PARSER_ERROR_INVALID_INDEX, 0);
                    discard_triangle = true;
                }
                else
                {
                    composed_vertex.pos = pos_array.data[index.pos_i1 - 1];
                }

                if(index.uv_i1 < 0 || index.uv_i1 > uv_array.size)
                {
                    if(log_errors && error_count <= OBJ_PARSER_MAX_ERRORS_TO_PRINT)
                        LOG_ERROR(OBJ_LOADER_CHANNEL, "uv index number %lld was out of range with value: %lld", (lld) i / 3, (lld) index.uv_i1);
                    had_error = obj_parser_error_on_to_error(OBJ_PARSER_ERROR_INVALID_INDEX, 0);
                }
                else
                {
                    composed_vertex.uv = uv_array.data[index.uv_i1 - 1];
                }

                obj_indeces_array.data[i] = index;
        
                u32 final_index = shape_assembly_add_vertex_custom(&shape_assembly, &out->vertices, composed_vertex);
                u32 mod = i%3;
                triangle.vertex_i[mod] = final_index;
                if(mod == 2)
                {
                    if(discard_triangle == false)
                        array_push(&out->triangles, triangle);
                    discard_triangle = false;
                }
            }
        }
    
        Obj_Group export_group = {0};
        obj_group_init(&export_group, default_alloc);
        builder_assign(&export_group.group, string_from_builder(parent_group->group));
        builder_assign(&export_group.object, string_from_builder(parent_group->object));
        builder_assign(&export_group.material, string_from_builder(parent_group->material));
        export_group.smoothing_index = parent_group->smoothing_index;
        export_group.merge_group_resolution = parent_group->merge_group_resolution;
        export_group.trinagles_from = triangles_before;
        export_group.trinagles_count = (i32) out->triangles.size - triangles_before;

        array_push(&out->groups, export_group);
    }

    //deinit temp_groups
    for(isize parent_group_i = 0; parent_group_i < temp_groups.size; parent_group_i++)
    {
        obj_group_deinit(&temp_groups.data[parent_group_i]);
    }

    isize num_allocations_after = allocator_get_stats(allocator_get_scratch()).allocation_count;
    isize allocation_delta = num_allocations_after - num_allocations_before;
    LOG_INFO(OBJ_LOADER_CHANNEL, "Obj loader caused %lld allocations", (lld) allocation_delta);
    
    hash_index_deinit(&shape_assembly);
    PERF_COUNTER_END(processing_counter);
    
    array_deinit(&temp_groups);
    array_deinit(&obj_indeces_array);
    array_deinit(&pos_array);
    array_deinit(&uv_array);
    array_deinit(&norm_array);
    log_group_pop();

    PERF_COUNTER_END(c);
    return had_error;
}

EXPORT Error obj_parser_load(Shape* append_to, String obj_path, Obj_Parser_Error_Array* error_or_null, bool log_errors)
{
    //log_errors = false;
    PERF_COUNTER_START(c);
    Allocator_Set prev_allocs = allocator_set_both(allocator_get_scratch(), allocator_get_scratch());

    Obj_Model model = {0};
    String_Builder obj_data = {0};
    Error read_state = file_read_entire(obj_path, &obj_data);
    if(!error_is_ok(read_state) && log_errors)
    {
        String error_string = error_code(read_state);
        LOG_ERROR(OBJ_LOADER_CHANNEL, "obj_parser_load() failed to read file " STRING_FMT " with error " STRING_FMT, 
            STRING_PRINT(obj_path), STRING_PRINT(error_string));
    }

    Error parse_state = ERROR_OR(read_state) obj_parser_parse(&model, string_from_builder(obj_data), error_or_null, log_errors);
    allocator_set(prev_allocs);

    array_append(&append_to->triangles, model.triangles.data, model.triangles.size);
    array_append(&append_to->vertices, model.vertices.data, model.vertices.size);

    PERF_COUNTER_END(c);
    LOG_INFO("PERF", "Loading of " STRING_FMT " took: %lf ms", STRING_PRINT(obj_path), perf_counter_get_ellapsed(c) * 1000);
    return parse_state;
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

EXPORT Error obj_parser_parse_mtl(Obj_Material_Info_Array* out, String mtl_source, Obj_Parser_Error_Array* output_errors_to_or_null, bool log_errors)
{
    Allocator* def_alloc = allocator_get_default();
    Obj_Material_Info* material = NULL;
    Error out_error = {0};
    isize error_count = 0;
    
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
            if(error_is_ok(out_error))
                out_error = obj_parser_error_on_to_error(error, line_number);

            if(output_errors_to_or_null)
            {
                Obj_Parser_Error parser_error = {0};
                parser_error.index = i;
                parser_error.line = (i32) line_number;
                parser_error.unimplemented = false;
                parser_error.statement = error;

                array_push(output_errors_to_or_null, parser_error);
            }
            
            if(log_errors && error_count <= MTL_PARSER_MAX_ERRORS_TO_PRINT)
            {
                const char* error_text = obj_parser_error_on_to_string(error);
                LOG_WARN(OBJ_LOADER_CHANNEL, "malformed statement %s on line %lld:\n" STRING_FMT, error_text, (lld) line_number, STRING_PRINT(line));
            }
            
            if(log_errors && error_count == MTL_PARSER_MAX_ERRORS_TO_PRINT)
            {
                LOG_WARN(OBJ_LOADER_CHANNEL, "file contains more than %lld errors. Stopping to print.", (lld) error_count);
            }

            error_count += 1;
        }
    }

    return out_error;
}

EXPORT Error obj_parser_load_mtl(Obj_Material_Info_Array* out, String mtl_path, Obj_Parser_Error_Array* output_errors_to_or_null, bool log_errors)
{
    //log_errors = false;
    PERF_COUNTER_START(c);
    Allocator_Set prev_allocs = allocator_set_both(allocator_get_scratch(), allocator_get_scratch());

    String_Builder obj_data = {0};
    Error read_state = file_read_entire(mtl_path, &obj_data);
    if(!error_is_ok(read_state) && log_errors)
    {
        String error_string = error_code(read_state);
        LOG_ERROR(OBJ_LOADER_CHANNEL, "obj_parser_load() failed to read file " STRING_FMT " with error " STRING_FMT, 
            STRING_PRINT(mtl_path), STRING_PRINT(error_string));
    }

    Error parse_state = ERROR_OR(read_state) obj_parser_parse_mtl(out, string_from_builder(obj_data), output_errors_to_or_null, log_errors);
    allocator_set(prev_allocs);

    PERF_COUNTER_END(c);
    LOG_INFO("PERF", "Loading of " STRING_FMT " took: %lf ms", STRING_PRINT(mtl_path), perf_counter_get_ellapsed(c) * 1000);
    return parse_state;
}

#endif