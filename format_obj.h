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

#define OBJ_LOADER_CHANNEL "ASSET"

#ifndef VEC_ARRAY_DEFINED

    #define VEC_ARRAY_DEFINED
    DEFINE_ARRAY_TYPE(Vec2, Vec2_Array);
    DEFINE_ARRAY_TYPE(Vec3, Vec3_Array);
    DEFINE_ARRAY_TYPE(Vec4, Vec4_Array);

#endif

typedef enum Obj_Parser_Error_On
{
    OBJ_PARSER_ERROR_ON_NONE,
    OBJ_PARSER_ERROR_ON_FACE,
    OBJ_PARSER_ERROR_ON_COMMENT,
    OBJ_PARSER_ERROR_ON_VERTEX_POS,
    OBJ_PARSER_ERROR_ON_VERTEX_NORM,
    OBJ_PARSER_ERROR_ON_VERTEX_UV,
    OBJ_PARSER_ERROR_ON_MATERIAL_LIBRARY,
    OBJ_PARSER_ERROR_ON_MATERIAL_USE,
    OBJ_PARSER_ERROR_ON_POINT,
    OBJ_PARSER_ERROR_ON_LINE,
    OBJ_PARSER_ERROR_ON_GROUP,
    OBJ_PARSER_ERROR_ON_SMOOTH_SHADING,
    OBJ_PARSER_ERROR_ON_OBJECT,
    OBJ_PARSER_ERROR_ON_INVALID_INDEX,
    OBJ_PARSER_ERROR_ON_OTHER,
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
    Vec3 offset;                //-o u (v (w))
    Vec3 scale;                 //-s u (v (w))
    Vec3 turbulance;            //-t u (v (w)) //is some kind of perturbance in the specified direction
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

    Obj_Texture_Info map_pbr_rougness;           //Pr/map_Pr [options] [name]
    Obj_Texture_Info map_pbr_metallic;           //Pm/map_Pm
    Obj_Texture_Info map_pbr_sheen;              //Ps/map_Ps
    Obj_Texture_Info map_pbr_clearcoat_thickness;//Pc
    Obj_Texture_Info map_pbr_clearcoat_rougness; //Pcr
    Obj_Texture_Info map_pbr_emmisive;           //Ke/map_Ke
    Obj_Texture_Info map_pbr_anisotropy;         //aniso
    Obj_Texture_Info map_pbr_anisotropy_rotation;//anisor
    Obj_Texture_Info map_pbr_normal;             //norm

    Obj_Texture_Info map_rma; //RMA material (roughness, metalness, ambient occlusion)
    Obj_Texture_Info map_orm; //alternate definition of map_RMA
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
EXPORT void obj_group_deinit(Obj_Group* info);
EXPORT void obj_model_deinit(Obj_Model* info);

EXPORT void obj_texture_info_init(Obj_Texture_Info* info, Allocator* alloc);
EXPORT void obj_group_init(Obj_Group* info, Allocator* alloc);
EXPORT void obj_model_init(Obj_Model* info, Allocator* alloc);

EXPORT const char* obj_parser_error_on_to_string(Obj_Parser_Error_On statement);
EXPORT Error obj_parser_error_on_to_error(Obj_Parser_Error_On statement);

EXPORT Error obj_load_obj_only(Obj_Model* out, String obj_path, Obj_Parser_Error_Array* output_errors_to_or_null, bool log_errors);
EXPORT Error obj_load_mtl_only(Obj_Material_Info_Array* out, String obj_path, Obj_Parser_Error_Array* output_errors_to_or_null, bool log_errors);

EXPORT Error obj_parse_obj(Obj_Model* out, String obj_source, Obj_Parser_Error_Array* output_errors_to_or_null, bool log_errors);
EXPORT Error obj_parse_mtl(Obj_Material_Info_Array* out, String obj_source, Obj_Parser_Error_Array* output_errors_to_or_null, bool log_errors);

//Loads obj model along with all of its required materials
EXPORT Error obj_load(Obj_Model* out, String obj_source, Obj_Parser_Error_Array* output_errors_to_or_null, bool log_errors);

#endif

#if (defined(LIB_ALL_IMPL) || defined(LIB_OBJ_PARSER_IMPL)) && !defined(LIB_OBJ_PARSER_HAS_IMPL)
#define LIB_OBJ_PARSER_HAS_IMPL

INTERNAL bool is_whitespace(char c)
{
    switch(c)
    {
        case ' ':
        case '\n':
        case '\t':
        case '\r':
        case '\v':
        case '\f':
            return true;
        default: 
            return false;
    }
}

INTERNAL bool is_digit(char c)
{
    return '0' <= c && c <= '9';
}

INTERNAL bool is_alpha(char c)
{
    //return ('a' <= c && c <= 'z') || ('A' <= c && c <= 'Z');

    //this is just a little flex of doing the two range checks in one
    //using the fact that all uppercase to lowercase leters are 32 appart.
    //That means we can just maks the fift bit and test once.
    //You can simply test this works by comparing the result of both approaches on all char values.
    char diff = c - 'A';
    char masked = diff & ~(1 << 5);
    bool is_letter = 0 <= masked && masked <= ('Z' - 'A');

    return is_letter;
}

//all characters permitted inside a common programming language id. [0-9], _, [a-z], [A-Z]
INTERNAL bool is_id_char(char c)
{
    return is_digit(c) || is_alpha(c) || c == '_';
}

typedef enum Match_Kind
{
    MATCH_NORMAL,
    MATCH_INVERTED,
} Match_Kind;

INTERNAL bool match_char_custom(String str, isize* index, char c, Match_Kind match)
{
    if(*index < str.size && (str.data[*index] == c) == (match == MATCH_NORMAL))
    {
        *index += 1;
        return true;
    }
    return false;
}

INTERNAL bool match_char(String str, isize* index, char c)
{
    return match_char_custom(str, index, c, MATCH_NORMAL);
}

INTERNAL bool match_sequence(String str, isize* index, String sequence)
{
    if(string_has_substring_at(str, *index, sequence))
    {
        *index += sequence.size;
        return true;
    }
    return false;   
}

INTERNAL bool match_whitespace_custom(String str, isize* index, Match_Kind match)
{
    isize i = *index;
    for(; i < str.size; i++)
    {
        if(is_whitespace(str.data[i]) != (match == MATCH_NORMAL))
            break;
    }

    bool matched = i != *index;
    *index = i;
    return matched;
}


INTERNAL bool match_whitespace(String str, isize* index)
{
    return match_whitespace_custom(str, index, MATCH_NORMAL);
}

INTERNAL bool match_id_chars(String str, isize* index)
{
    isize i = *index;
    for(; i < str.size; i++)
    {
        if(is_id_char(str.data[i]) == false)
            break;
    }

    bool matched = i != *index;
    *index = i;
    return matched;
}

//starts with _, [a-z], [A-Z] or _ then is followed by any number of [0-9], _, [a-z], [A-Z]
INTERNAL bool match_id(String str, isize* index)
{
    if(*index < str.size)
    {
        if(str.data[*index] == '_' || is_alpha(str.data[*index]))
        {
            *index += 1;
            for(; *index < str.size; *index += 1)
            {
                if(is_id_char(str.data[*index]) == false)
                    break;
            }

            return true;
        }
    }

    return false;
}

//matches a sequence of digits in decimal: "00113000" -> 113000
INTERNAL bool match_decimal_u64(String str, isize* index, i64* out)
{
    u64 parsed = 0;
    isize i = *index;
    for(; i < str.size; i++)
    {
        u64 digit_value = (u64) str.data[i] - (u64) '0';
        if(digit_value > 9)
            break;

        u64 new_parsed = parsed*10 + digit_value;

        //Correctly handle overflow. This is correct because unsigned numbers
        //have defined overflow. We still however limit the number on INT64_MAX
        //to make our life easier when we eventually use signed.
        if(new_parsed < parsed)
            parsed = INT64_MAX;
        else
            parsed = new_parsed;

    }

    bool matched = i != *index;
    *index = i;

    ASSERT(parsed <= INT64_MAX);
    *out = (i64) parsed;
    return matched;
}

//matches a sequence of signed digits in decimal: "-00113000" -> -113000
INTERNAL bool match_decimal_i64(String str, isize* index, i64* out)
{
    bool has_minus = match_char(str, index, '-');
    i64 uout = 0;
    bool matched_number = match_decimal_u64(str, index, &uout);
    if(has_minus)
        *out = -uout;
    else
        *out = uout;

    return matched_number;
}

INTERNAL bool match_decimal_i32(String str, isize* index, i32* out)
{
    bool has_minus = match_char(str, index, '-');
    i64 uout = 0;
    bool matched_number = match_decimal_u64(str, index, &uout);
    if(has_minus)
        *out = (i32) -uout;
    else
        *out = (i32) uout;

    return matched_number;
}

INTERNAL f32 _pow10f(isize pow)
{
    switch(pow)
    {
        case 0: return 1.0f;
        case 1: return 10.0f;
        case 2: return 100.0f;
        case 3: return 1000.0f;
        case 4: return 10000.0f;
        case 5: return 100000.0f;
        default: return powf(10.0f, (f32) pow);
    };
}

INTERNAL bool match_decimal_f32(String str, isize* index, f32* out)
{
    i64 before_dot = 0;
    i64 after_dot = 0;
    
    bool has_minus = match_char(str, index, '-');
    bool matched_before_dot = match_decimal_u64(str, index, &before_dot);
    if(!matched_before_dot)
        return false;

    match_char(str, index, '.');

    isize decimal_index = *index;
    match_decimal_u64(str, index, &after_dot);

    isize decimal_size = *index - decimal_index;
    f32 decimal_pow = _pow10f(decimal_size);
    f32 result = (f32) before_dot + (f32) after_dot / decimal_pow;
    if(has_minus)
        result = -result;

    *out = result;
    return true;
}

INTERNAL void test_match()
{
    {
        isize index = 0;
        TEST(match_whitespace(STRING("   "), &index));
        TEST(index == 3);
    }
    
    {
        isize index = 0;
        TEST(match_whitespace(STRING("   \n \r \t "), &index));
        TEST(index == 9);
    }
    
    {
        isize index = 0;
        TEST(match_whitespace(STRING("a "), &index) == false);
        TEST(index == 0);
    }
    
    {
        isize index = 0;
        TEST(match_whitespace_custom(STRING("a "), &index, MATCH_INVERTED));
        TEST(index == 1);
    }
    
    {
        isize index = 0;
        TEST(match_whitespace_custom(STRING("a"), &index, MATCH_INVERTED));
        TEST(index == 1);
    }

    {
        f32 test_f32 = 0;
        isize index = 0;
        TEST(match_decimal_f32(STRING("12"), &index, &test_f32));
        TEST(is_near_scaledf(12.0f, test_f32, EPSILON));
    }
    
    {
        f32 test_f32 = 0;
        isize index = 0;
        TEST(match_decimal_f32(STRING("-12"), &index, &test_f32));
        TEST(is_near_scaledf(-12.0f, test_f32, EPSILON));
    }
    
    {
        f32 test_f32 = 0;
        isize index = 0;
        TEST(match_decimal_f32(STRING("-12.05"), &index, &test_f32));
        TEST(is_near_scaledf(-12.05f, test_f32, EPSILON));
    }
}

EXPORT const char* obj_parser_error_on_to_string(Obj_Parser_Error_On statement)
{
    switch(statement)
    {
        case OBJ_PARSER_ERROR_ON_NONE: return "OBJ_PARSER_ERROR_ON_NONE";
        case OBJ_PARSER_ERROR_ON_FACE: return "OBJ_PARSER_ERROR_ON_FACE";
        case OBJ_PARSER_ERROR_ON_COMMENT: return "OBJ_PARSER_ERROR_ON_COMMENT";
        case OBJ_PARSER_ERROR_ON_VERTEX_POS: return "OBJ_PARSER_ERROR_ON_VERTEX_POS";
        case OBJ_PARSER_ERROR_ON_VERTEX_NORM: return "OBJ_PARSER_ERROR_ON_VERTEX_NORM";
        case OBJ_PARSER_ERROR_ON_VERTEX_UV: return "OBJ_PARSER_ERROR_ON_VERTEX_UV";
        case OBJ_PARSER_ERROR_ON_MATERIAL_LIBRARY: return "OBJ_PARSER_ERROR_ON_MATERIAL_LIBRARY";
        case OBJ_PARSER_ERROR_ON_MATERIAL_USE: return "OBJ_PARSER_ERROR_ON_MATERIAL_USE";
        case OBJ_PARSER_ERROR_ON_POINT: return "OBJ_PARSER_ERROR_ON_POINT";
        case OBJ_PARSER_ERROR_ON_LINE: return "OBJ_PARSER_ERROR_ON_LINE";
        case OBJ_PARSER_ERROR_ON_GROUP: return "OBJ_PARSER_ERROR_ON_GROUP";
        case OBJ_PARSER_ERROR_ON_SMOOTH_SHADING: return "OBJ_PARSER_ERROR_ON_SMOOTH_SHADING";
        case OBJ_PARSER_ERROR_ON_OBJECT: return "OBJ_PARSER_ERROR_ON_OBJECT";
        case OBJ_PARSER_ERROR_ON_INVALID_INDEX: return "OBJ_PARSER_ERROR_ON_INVALID_INDEX";
        
        default:
        case OBJ_PARSER_ERROR_ON_OTHER: return "OBJ_PARSER_ERROR_ON_OTHER";
    }
}

INTERNAL String obj_parser_translate_error(u32 code, void* context)
{
    (void) context;
    return string_make(obj_parser_error_on_to_string((Obj_Parser_Error_On) code));
}

EXPORT Error obj_parser_error_on_to_error(Obj_Parser_Error_On statement)
{
    static u32 shader_error_module = 0;
    if(shader_error_module == 0)
        shader_error_module = error_system_register_module(obj_parser_translate_error, STRING("obj_parser.h"), NULL);

    return error_make(shader_error_module, statement);
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

EXPORT Error obj_parser_parse(Shape* append_to, String obj_source, Obj_Parser_Error_Array* error_or_null, bool log_errors);
EXPORT Error obj_parse_obj(Obj_Model* out, String obj_source, Obj_Parser_Error_Array* output_errors_to_or_null, bool log_errors)
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
    isize max_error_print = 1000;
    
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

        bool error = false;
        Obj_Parser_Error_On statement = OBJ_PARSER_ERROR_ON_NONE;

        char first_char = line.data[0];
        switch (first_char) 
        {
            case '#': {
                statement = OBJ_PARSER_ERROR_ON_COMMENT;
                // Skip comments
                continue;
            } break;

            case 'v': {
                char second_char = line.data[1];
                switch (second_char)
                {
                    case ' ': {
                        statement = OBJ_PARSER_ERROR_ON_VERTEX_POS;

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
                            error = true;
                        else
                            array_push(&pos_array, pos);
                    } break;

                    case 'n': {
                        statement = OBJ_PARSER_ERROR_ON_VERTEX_NORM;

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
                            error = true;
                        else
                            array_push(&norm_array, norm);
                    } break;

                    case 't': {
                        statement = OBJ_PARSER_ERROR_ON_VERTEX_UV;

                        // @NOTE: Ignoring Z if present.
                        Vec2 tex_coord = {0};
                        isize line_index = 2;
                        bool matched = true
                            && match_whitespace(line, &line_index)
                            && match_decimal_f32(line, &line_index, &tex_coord.x)
                            && match_whitespace(line, &line_index)
                            && match_decimal_f32(line, &line_index, &tex_coord.y);
                        
                        if(!matched)
                            error = true;
                        else 
                            array_push(&uv_array, tex_coord);
                    } break;

                    default: {
                        error = true;
                    }
                }
            } break;

            case 'f': {
                statement = OBJ_PARSER_ERROR_ON_FACE;

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
                    error = true;

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
                statement = OBJ_PARSER_ERROR_ON_SMOOTH_SHADING;
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
                    error = true;
                }
            } break;
            
            //Group: g [group1] [group2] ...
            case 'g': {
                statement = OBJ_PARSER_ERROR_ON_GROUP;
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
                    error = true;
                }
            } break;

            //Object: g [object_name] ...
            case 'o': {
                statement = OBJ_PARSER_ERROR_ON_OBJECT;
            
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
                    error = true;
                }
            } break;

            case 'm': {
                statement = OBJ_PARSER_ERROR_ON_MATERIAL_LIBRARY;

                isize line_index = 0;
                bool matches = match_sequence(line, &line_index, STRING("mtllib"));
                matches = matches && match_whitespace(line, &line_index);
                    
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
                    error = true;
                }
            } break;
            
            case 'u': {
                statement = OBJ_PARSER_ERROR_ON_MATERIAL_USE;

                isize line_index = 0;
                bool matches = match_sequence(line, &line_index, STRING("usemtl"));
                matches = matches && match_whitespace(line, &line_index);
                    
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
                    error = true;
                }
            } break;

            default: {
                error = true;
            }
        }

        //Handle errors
        if(error)
        {
            bool unimplemented = false;
            had_error = obj_parser_error_on_to_error(statement);

            if(output_errors_to_or_null)
            {
                Obj_Parser_Error parser_error = {0};
                parser_error.index = line_start_i;
                parser_error.line = (i32) line_number;
                parser_error.statement = statement;
                parser_error.unimplemented = unimplemented;
                array_push(output_errors_to_or_null, parser_error);
            }

            if(log_errors && error_count <= max_error_print)
            {
                const char* error_text = obj_parser_error_on_to_string(statement);
                LOG_WARN(OBJ_LOADER_CHANNEL, "malformed statement %s on line %lld:\n" STRING_FMT, error_text, (lld) line_number, STRING_PRINT(line));
            }
            
            if(log_errors && error_count == max_error_print)
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
                    if(log_errors && error_count <= max_error_print)
                        LOG_ERROR(OBJ_LOADER_CHANNEL, "norm index number %lld was out of range with value: %lld", (lld) i / 3, (lld) index.norm_i1);
                    had_error = obj_parser_error_on_to_error(OBJ_PARSER_ERROR_ON_INVALID_INDEX);
                }
                else
                {
                    composed_vertex.norm = norm_array.data[index.norm_i1 - 1];
                }

                if(index.pos_i1 < 0 || index.pos_i1 > pos_array.size)
                {
                    if(log_errors && error_count <= max_error_print)
                        LOG_ERROR(OBJ_LOADER_CHANNEL, "pos index number %lld was out of range with value: %lld", (lld) i / 3, (lld) index.pos_i1);
                    had_error = obj_parser_error_on_to_error(OBJ_PARSER_ERROR_ON_INVALID_INDEX);
                    discard_triangle = true;
                }
                else
                {
                    composed_vertex.pos = pos_array.data[index.pos_i1 - 1];
                }

                if(index.uv_i1 < 0 || index.uv_i1 > uv_array.size)
                {
                    if(log_errors && error_count <= max_error_print)
                        LOG_ERROR(OBJ_LOADER_CHANNEL, "uv index number %lld was out of range with value: %lld", (lld) i / 3, (lld) index.uv_i1);
                    had_error = obj_parser_error_on_to_error(OBJ_PARSER_ERROR_ON_INVALID_INDEX);
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

    Error parse_state = ERROR_OR(read_state) obj_parse_obj(&model, string_from_builder(obj_data), error_or_null, log_errors);
    allocator_set(prev_allocs);

    array_append(&append_to->triangles, model.triangles.data, model.triangles.size);
    array_append(&append_to->vertices, model.vertices.data, model.vertices.size);

    PERF_COUNTER_END(c);
    LOG_INFO("PERF", "Loading of " STRING_FMT " took: %lf ms", STRING_PRINT(obj_path), perf_counter_get_ellapsed(c) * 1000);
    return parse_state;
}
#endif