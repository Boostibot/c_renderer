#pragma once
#include "shapes.h"
#include "string.h"
#include "format_obj.h"

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


#if 0
EXPORT Error obj_parser_translated_obj_data_to_engine_data(Shape_Assembly* shape_assembly, Obj_Model model)
{
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
            if(!builder_is_equal(child_group->groups, parent_group->groups)
               || !builder_is_equal(child_group->object, parent_group->object))
                continue;

            isize i_min = (child_group->trinagles_from)*3;
            isize i_max = (child_group->trinagles_from + child_group->trinagles_count)*3;
        
            //Deduplicate and parse vertices
            for(isize i = i_min; i < i_max; i++)
            {
                Vertex composed_vertex = {0};
                Obj_Vertex_Index index = obj_indeces_array.data[i];
                if(index.norm_i1 < 0 || index.norm_i1 > model.normals.size)
                {
                    //if(log_errors && error_count <= OBJ_PARSER_MAX_ERRORS_TO_PRINT)
                        //LOG_ERROR(OBJ_LOADER_CHANNEL, "norm index number %lld was out of range with value: %lld", (lld) i / 3, (lld) index.norm_i1);
                    //had_error = obj_parser_error_on_to_error(OBJ_PARSER_ERROR_INVALID_INDEX, 0);
                }
                else
                {
                    composed_vertex.norm = model.normals.data[index.norm_i1 - 1];
                }

                if(index.pos_i1 < 0 || index.pos_i1 > model.positions.size)
                {
                    //if(log_errors && error_count <= OBJ_PARSER_MAX_ERRORS_TO_PRINT)
                        //LOG_ERROR(OBJ_LOADER_CHANNEL, "pos index number %lld was out of range with value: %lld", (lld) i / 3, (lld) index.pos_i1);
                    //had_error = obj_parser_error_on_to_error(OBJ_PARSER_ERROR_INVALID_INDEX, 0);
                    discard_triangle = true;
                }
                else
                {
                    composed_vertex.pos = model.positions.data[index.pos_i1 - 1];
                }

                if(index.uv_i1 < 0 || index.uv_i1 > model.uvs.size)
                {
                    //if(log_errors && error_count <= OBJ_PARSER_MAX_ERRORS_TO_PRINT)
                        //LOG_ERROR(OBJ_LOADER_CHANNEL, "uv index number %lld was out of range with value: %lld", (lld) i / 3, (lld) index.uv_i1);
                    //had_error = obj_parser_error_on_to_error(OBJ_PARSER_ERROR_INVALID_INDEX, 0);
                }
                else
                {
                    composed_vertex.uv = model.uvs.data[index.uv_i1 - 1];
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
    
        //Obj_Group export_group = {0};
        //obj_group_init(&export_group, default_alloc);
        //builder_assign(&export_group.group, string_from_builder(parent_group->group));
        //builder_assign(&export_group.object, string_from_builder(parent_group->object));
        //builder_assign(&export_group.material, string_from_builder(parent_group->material));
        //export_group.smoothing_index = parent_group->smoothing_index;
        //export_group.merge_group_resolution = parent_group->merge_group_resolution;
        //export_group.trinagles_from = triangles_before;
        //export_group.trinagles_count = (i32) out->triangles.size - triangles_before;

        //array_push(&out->groups, export_group);
    }

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

