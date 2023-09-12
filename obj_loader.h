#ifndef LIB_OBJ_PARSER
#define LIB_OBJ_PARSER

#include "math.h"
#include "array.h"
#include "string.h"
#include "format.h"
#include "log.h"
#include "error.h"
#include "shapes.h"

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

DEFINE_ARRAY_TYPE(Obj_Parser_Error, Obj_Parser_Error_Array);

EXPORT const char* obj_parser_error_on_to_string(Obj_Parser_Error_On statement);
EXPORT Error obj_parser_error_on_to_error(Obj_Parser_Error_On statement);

EXPORT Error obj_parser_load(Shape* append_to, String obj_path, Obj_Parser_Error_Array* output_errors_to_or_null, bool log_errors);
EXPORT Error obj_parser_parse(Shape* append_to, String obj_source, Obj_Parser_Error_Array* output_errors_to_or_null, bool log_errors);

#endif

#if (defined(LIB_ALL_IMPL) || defined(LIB_OBJ_PARSER_IMPL)) && !defined(LIB_OBJ_PARSER_HAS_IMPL)
#define LIB_OBJ_PARSER_HAS_IMPL

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

INTERNAL String translate_obj_parser_error(u32 code, void* context)
{
    (void) context;
    return string_make(obj_parser_error_on_to_string((Obj_Parser_Error_On) code));
}

EXPORT Error obj_parser_error_on_to_error(Obj_Parser_Error_On statement)
{
    static u32 shader_error_module = 0;
    if(shader_error_module == 0)
        shader_error_module = error_system_register_module(translate_obj_parser_error, STRING("obj_parser.h"), NULL);

    return error_make(shader_error_module, statement);
}

EXPORT Error obj_parser_parse(Shape* append_to, String obj_source, Obj_Parser_Error_Array* error_or_null, bool log_errors)
{
    //one based indeces into the appropraite arrays.
    //If value is 0 means not present.
    typedef struct Obj_Vertex_Index
    {
        i32 pos_i1;
        i32 uv_i1;
        i32 norm_i1;
    } Obj_Vertex_Index;

    DEFINE_ARRAY_TYPE(Obj_Vertex_Index, Obj_Vertex_Index_Array);

    enum {FILE_NAME_RESERVE = 512};

    //Init temporary buffers
    #define FORMAT_TEMP "%32s"
    char temp[32] = {0};
    Error had_error = {0};

    Allocator* scratch = allocator_get_scratch();
    Obj_Vertex_Index_Array obj_indeces_array = {scratch};
    Vec3_Array pos_or_null = {scratch}; 
    Vec2_Array uv_or_null = {scratch}; 
    Vec3_Array norm_or_null = {scratch};
    String_Builder line_builder = {scratch};

    array_init_backed(&line_builder, scratch, FILE_NAME_RESERVE);

    //Try to guess the needed size based on a simple heurestic
    isize expected_line_count = obj_source.size / 32 + 128;
    array_reserve(&obj_indeces_array, expected_line_count);
    array_reserve(&pos_or_null, expected_line_count);
    array_reserve(&uv_or_null, expected_line_count);
    array_reserve(&norm_or_null, expected_line_count);

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
        array_assign(&line_builder, line.data, line.size);
        ASSERT(line.size > 0);

        bool error = false;
        Obj_Parser_Error_On statement = OBJ_PARSER_ERROR_ON_NONE;

        ASSERT(line_builder.data != NULL);
        char first_char = line_builder.data[0];
        switch (first_char) 
        {
            case '#': {
                statement = OBJ_PARSER_ERROR_ON_COMMENT;
                // Skip comments
                continue;
            } break;

            case 'v': {
                char second_char = line_builder.data[1];
                switch (second_char)
                {
                    case ' ': {
                        statement = OBJ_PARSER_ERROR_ON_VERTEX_POS;

                        Vec3 pos = {0};
                        memset(temp, 0, sizeof temp);
                        int read = sscanf(line_builder.data, FORMAT_TEMP" %f %f %f", temp, &pos.x, &pos.y, &pos.z);
                        
                        if(read < 3 || strcmp(temp, "v") != 0)
                            error = true;
                        else
                            array_push(&pos_or_null, pos);
                    } break;

                    case 'n': {
                        statement = OBJ_PARSER_ERROR_ON_VERTEX_NORM;

                        Vec3 norm = {0};
                        memset(temp, 0, sizeof temp);
                        int read = sscanf(line_builder.data, FORMAT_TEMP" %f %f %f", temp, &norm.x, &norm.y, &norm.z);
                        
                        if(read < 3 || strcmp(temp, "vn") != 0)
                            error = true;
                        else
                            array_push(&norm_or_null, norm);
                    } break;

                    case 't': {
                        statement = OBJ_PARSER_ERROR_ON_VERTEX_UV;

                        Vec2 tex_coord = {0};
                        memset(temp, 0, sizeof temp);

                        // NOTE: Ignoring Z if present.
                        int read = sscanf(line_builder.data, FORMAT_TEMP" %f %f", temp, &tex_coord.x, &tex_coord.y);
                        
                        if(read < 2 || strcmp(temp, "vt") != 0)
                            error = true;
                        else 
                            array_push(&uv_or_null, tex_coord);
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
                //tries first the ones based on our analysis and then the rest until valid is found
                if(has_double_slash)
                {
                    memset(temp, 0, sizeof temp);
                    int read = sscanf(line_builder.data, FORMAT_TEMP" %d//%d %d//%d %d//%d", temp,
                               &indeces[0].pos_i1,
                               &indeces[0].norm_i1,

                               &indeces[1].pos_i1,
                               &indeces[1].norm_i1,

                               &indeces[2].pos_i1,
                               &indeces[2].norm_i1);
                    
                    if(read == 6 + 1 || strcmp(temp, "f") != 0)
                        ok = true;
                }
                else if(!has_slash)
                {
                    memset(temp, 0, sizeof temp);
                    int read = sscanf(line_builder.data, FORMAT_TEMP" %d %d %d", temp,
                               &indeces[0].pos_i1,
                               &indeces[1].pos_i1,
                               &indeces[2].pos_i1);
                    
                    if(read == 3 + 1 || strcmp(temp, "f") != 0)
                        ok = true;
                }
                
                if(!ok)
                {
                    memset(temp, 0, sizeof temp);
                    int read = sscanf(line_builder.data, FORMAT_TEMP" %d/%d/%d %d/%d/%d %d/%d/%d", temp,
                               &indeces[0].pos_i1,
                               &indeces[0].uv_i1, 
                               &indeces[0].norm_i1,

                               &indeces[1].pos_i1,
                               &indeces[1].uv_i1, 
                               &indeces[1].norm_i1,

                               &indeces[2].pos_i1,
                               &indeces[2].uv_i1, 
                               &indeces[2].norm_i1);
                    
                    if(read == 9 + 1 || strcmp(temp, "f") != 0)
                        ok = true;
                }
                
                if(!ok)
                {
                    memset(temp, 0, sizeof temp);
                    int read = sscanf(line_builder.data, FORMAT_TEMP" %d/%d %d/%d %d/%d", temp,
                               &indeces[0].pos_i1,
                               &indeces[0].uv_i1, 

                               &indeces[1].pos_i1,
                               &indeces[1].uv_i1, 

                               &indeces[2].pos_i1,
                               &indeces[2].uv_i1);
                    
                    if(read == 6 + 1 || strcmp(temp, "f") != 0)
                        ok = true;
                }

                if(!ok)
                    error = true;

                //correct negative values indeces. If index is negative it refers to the -i-nth last parsed
                // value in the given category. If the category does not recieve data (NULL) set the index to 0
                for(isize i = 0; i < 3; i++)
                {
                    Obj_Vertex_Index index = indeces[i];
                    if(index.pos_i1 < 0)
                        index.pos_i1 = (u32) pos_or_null.size + index.pos_i1 + 1;
                    
                    if(index.uv_i1 < 0)
                        index.uv_i1 = (u32) uv_or_null.size + index.uv_i1 + 1;
                        
                    if(index.norm_i1 < 0)
                        index.norm_i1 = (u32) norm_or_null.size + index.norm_i1 + 1;
                }

                array_push(&obj_indeces_array, indeces[0]); 
                array_push(&obj_indeces_array, indeces[1]); 
                array_push(&obj_indeces_array, indeces[2]);
            } break;
            
            default: {
                error = true;
            }
        }

        //Handle errors
        if(error)
        {
            bool unimplemented = false;
            if(statement == OBJ_PARSER_ERROR_ON_NONE)
            {
                unimplemented = true;
                if(string_is_prefixed_with(line, STRING("g ")))
                    statement = OBJ_PARSER_ERROR_ON_GROUP;
                else if(string_is_prefixed_with(line, STRING("usemtl ")))
                    statement = OBJ_PARSER_ERROR_ON_MATERIAL_USE;
                else if(string_is_prefixed_with(line, STRING("mtllib ")))
                    statement = OBJ_PARSER_ERROR_ON_MATERIAL_LIBRARY;
                else if(string_is_prefixed_with(line, STRING("s ")))
                    statement = OBJ_PARSER_ERROR_ON_SMOOTH_SHADING;
                else if(string_is_prefixed_with(line, STRING("o ")))
                    statement = OBJ_PARSER_ERROR_ON_OBJECT;
                else
                {
                    unimplemented = true;
                    statement = OBJ_PARSER_ERROR_ON_OTHER;
                }
            }

            if(!unimplemented)
                had_error = obj_parser_error_on_to_error(statement);

            if(error_or_null)
            {
                Obj_Parser_Error parser_error = {0};
                parser_error.index = line_start_i;
                parser_error.line = (i32) line_number;
                parser_error.statement = statement;
                parser_error.unimplemented = unimplemented;
                array_push(error_or_null, parser_error);
            }

            if(log_errors)
            {
                const char* error_text = obj_parser_error_on_to_string(statement);
                LOG_WARN(OBJ_LOADER_CHANNEL, "obj_parser_parse() encountered malformed statement %s while parsing line %lld:\n%s", error_text, (lld) line_number, line_builder.data);
            }
        }
    } 
    
    //Iterate all indeces using a hash map to deduplicate vertex data
    //and shape it into the vertex structure. 
    //Discards all triangles that dont have a valid position
    Hash_Index shape_assembly = {allocator_get_scratch()};
    hash_index_reserve(&shape_assembly, obj_indeces_array.size);

    //Try to guess the final needed size
    array_reserve(&append_to->indeces, append_to->indeces.size + obj_indeces_array.size/3); //divided by three since are triangles
    array_reserve(&append_to->vertices, append_to->indeces.size + obj_indeces_array.size/2); //divide by two since some vertices are shared
    
    bool discard_triangle = false;
    Triangle_Index triangle = {0};
    for(isize i = 0; i < obj_indeces_array.size; i++)
    {
        Vertex composed_vertex = {0};
        Obj_Vertex_Index index = obj_indeces_array.data[i];
        if(index.norm_i1 < 0 || index.norm_i1 > norm_or_null.size)
        {
            if(log_errors)
                LOG_ERROR(OBJ_LOADER_CHANNEL, "obj_parser_parse() norm index number %lld was out of range with value: %lld", (lld) i / 3, (lld) index.norm_i1);
            had_error = obj_parser_error_on_to_error(OBJ_PARSER_ERROR_ON_INVALID_INDEX);
        }
        else
        {
            composed_vertex.norm = norm_or_null.data[index.norm_i1 - 1];
        }

        if(index.pos_i1 < 0 || index.pos_i1 > pos_or_null.size)
        {
            if(log_errors)
                LOG_ERROR(OBJ_LOADER_CHANNEL, "obj_parser_parse() pos index number %lld was out of range with value: %lld", (lld) i / 3, (lld) index.pos_i1);
            had_error = obj_parser_error_on_to_error(OBJ_PARSER_ERROR_ON_INVALID_INDEX);
            discard_triangle = true;
        }
        else
        {
            composed_vertex.pos = pos_or_null.data[index.pos_i1 - 1];
        }

        if(index.uv_i1 < 0 || index.uv_i1 > uv_or_null.size)
        {
            if(log_errors)
                LOG_ERROR(OBJ_LOADER_CHANNEL, "obj_parser_parse() uv index number %lld was out of range with value: %lld", (lld) i / 3, (lld) index.uv_i1);
            had_error = obj_parser_error_on_to_error(OBJ_PARSER_ERROR_ON_INVALID_INDEX);
        }
        else
        {
            composed_vertex.uv = uv_or_null.data[index.uv_i1 - 1];
        }

        obj_indeces_array.data[i] = index;
        
        u32 final_index = shape_assembly_add_vertex(&shape_assembly, &append_to->vertices, composed_vertex);
        u32 mod = i%3;
        triangle.vertex_i[mod] = final_index;
        if(mod == 2)
        {
            if(discard_triangle == false)
                array_push(&append_to->indeces, triangle);
            discard_triangle = false;
        }
    }
    
    hash_index_deinit(&shape_assembly);
    array_deinit(&line_builder);
    return had_error;
}

EXPORT Error obj_parser_load(Shape* append_to, String obj_path, Obj_Parser_Error_Array* error_or_null, bool log_errors)
{
    Allocator* scratch = allocator_get_scratch();
    String_Builder obj_data = {scratch};
    Error read_state = file_read_entire(obj_path, &obj_data);
    if(!error_ok(read_state) && log_errors)
    {
        String error_string = error_code(read_state);
        LOG_ERROR(OBJ_LOADER_CHANNEL, "obj_parser_load() failed to read file " STRING_FMT " with error " STRING_FMT, 
            STRING_PRINT(obj_path), STRING_PRINT(error_string));
    }

    Error parse_state = ERROR_OR(read_state) obj_parser_parse(append_to, string_from_builder(obj_data), error_or_null, log_errors);
    return parse_state;
}
#endif