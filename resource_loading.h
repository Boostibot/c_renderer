#pragma once
#include "resources.h"
#include "format_obj.h"
#include "lib/error.h"
#include "lib/file.h"
#include "lib/profile.h"
#include "image_loader.h"
#include "lib/allocator_malloc.h"

INTERNAL String _format_obj_mtl_translate_error(u32 code, void* context)
{
    (void) context;
    enum {LOCAL_BUFF_SIZE = 256, MAX_ERRORS = 4};
    static int error_index = 0;
    static char error_strings[MAX_ERRORS][LOCAL_BUFF_SIZE + 1] = {0};

    int line = code >> 8;
    int error_flag = code & 0xFF;

    const char* error_flag_str = format_obj_mtl_error_statement_to_string((Format_Obj_Mtl_Error_Statement) error_flag);

    memset(error_strings[error_index], 0, LOCAL_BUFF_SIZE);
    snprintf(error_strings[error_index], LOCAL_BUFF_SIZE, "%s line: %d", error_flag_str, line);
    String out = string_make(error_strings[error_index]);
    error_index += 1;
    return out;
}

INTERNAL Error _format_obj_mtl_error_on_to_error(Format_Obj_Mtl_Error_Statement statement, isize line)
{
    static u32 error_module = 0;
    if(error_module == 0)
        error_module = error_system_register_module(_format_obj_mtl_translate_error, STRING("format_obj_mtl.h"), NULL);

    u32 splatted = (u32) line << 8 | (u32) statement;
    return error_make(error_module, splatted);
}

#define OBJ_MTL_ERROR_FMT           "%s at line %d" 
#define OBJ_MTL_ERROR_PRINT(error)  format_obj_mtl_error_statement_to_string((error).statement), (error).line

typedef enum Resource_Loading_Error {
    RESOURCE_LOADING_ERROR_NONE,
    RESOURCE_LOADING_ERROR_UV_INDEX,
    RESOURCE_LOADING_ERROR_POS_INDEX,
    RESOURCE_LOADING_ERROR_NORM_INDEX,
    RESOURCE_LOADING_ERROR_MATERIAL_NOT_FOUND,
    RESOURCE_LOADING_ERROR_OTHER,

} Resource_Loading_Error;

INTERNAL String _resource_loading_translate_error(u32 code, void* context)
{
    (void) context;
    switch(code)
    {
        case RESOURCE_LOADING_ERROR_NONE: return STRING("RESOURCE_LOADING_ERROR_NONE");
        case RESOURCE_LOADING_ERROR_UV_INDEX: return STRING("RESOURCE_LOADING_ERROR_UV_INDEX");
        case RESOURCE_LOADING_ERROR_POS_INDEX: return STRING("RESOURCE_LOADING_ERROR_POS_INDEX");
        case RESOURCE_LOADING_ERROR_NORM_INDEX: return STRING("RESOURCE_LOADING_ERROR_NORM_INDEX");
        case RESOURCE_LOADING_ERROR_MATERIAL_NOT_FOUND: return STRING("RESOURCE_LOADING_ERROR_MATERIAL_NOT_FOUND");
        
        case RESOURCE_LOADING_ERROR_OTHER: 
        default: return STRING("RESOURCE_LOADING_ERROR_OTHER");
    }
}

INTERNAL Error _resource_loading_error_to_error(Resource_Loading_Error error)
{
    static u32 error_module = 0;
    if(error_module == 0)
        error_module = error_system_register_module(_resource_loading_translate_error, STRING("resource_loading.h"), NULL);

    return error_make(error_module, (u32) error);
}

typedef struct Load_Error 
{
    String_Builder path;
    Error error;
} Load_Error;

DEFINE_ARRAY_TYPE(Error, Error_Array);

EXPORT void process_obj_triangle_mesh(Shape_Assembly* shape_assembly, Triangle_Mesh_Description* description, Format_Obj_Model model)
{
    hash_index64_reserve(&shape_assembly->vertices_hash, model.indeces.size);
    
    //Copy over materials
    for(isize i = 0; i < model.material_files.size; i++)
    {
        String material_file = string_from_builder(model.material_files.data[i]);
        array_push(&description->material_files, builder_from_string(material_file, NULL));
    }

    //Try to guess the final needed size
    array_reserve(&shape_assembly->triangles, shape_assembly->triangles.size + model.indeces.size/3); //divided by three since are triangles
    array_reserve(&shape_assembly->vertices, shape_assembly->vertices.size + model.indeces.size/2); //divide by two since some vertices are shared

    Allocator* def_alloc = allocator_get_default();

    //Deduplicate groups
    for(isize parent_group_i = 0; parent_group_i < model.groups.size; parent_group_i++)
    {
        Format_Obj_Group* parent_group = &model.groups.data[parent_group_i];
        i32 triangles_before = (i32) shape_assembly->triangles.size;
        
        //Skip empty groups
        if(parent_group->trinagles_count <= 0)
            continue;

        //Skip groups that have no group
        if(parent_group->groups.size <= 0)
            continue;

        //Skip groups that we already merged through earlier groups
        bool already_merged = false;
        for(isize i = 0; i < description->groups.size; i++)
            if(builder_is_equal(description->groups.data[i].name, parent_group->groups.data[0]))
            {
                already_merged = true;
                break;
            }

        if(already_merged)
            continue;

        //Iterate over all not yet seen obj groups (start from parent_group_i) and add the vertices if it 
        //is the same group. This merges all groups with the same name and same object into one engine group
        for(isize child_group_i = parent_group_i; child_group_i < model.groups.size; child_group_i++)
        {
            Format_Obj_Group* child_group = &model.groups.data[child_group_i];

            //Skip groups that are empty 
            if(child_group->trinagles_count <= 0)
                continue;

            //... and those in different objects
            if(!builder_is_equal(child_group->object, parent_group->object))
                continue;

            //Shouldnt be possible with the current parser but just in case
            if(child_group->groups.size == 0 || parent_group->groups.size == 0)
            
            //... and those that are not the same main group.
            //(We ignore all other groups than the first for now)
            if(!builder_is_equal(child_group->groups.data[0], parent_group->groups.data[0]))
                continue;

            isize i_min = (child_group->trinagles_from)*3;
            isize i_max = (child_group->trinagles_from + child_group->trinagles_count)*3;

            //Iterate all indeces using a hash map to deduplicate vertex data
            //and shape it into the vertex structure. 
            //Discards all triangles that dont have a valid position
            bool discard_triangle = false;
            Triangle_Index triangle = {0};
            for(isize i = i_min; i < i_max; i++)
            {
                enum {
                    NONE = 0,
                    INVALID_NORM_INDEX,
                    INVALID_POS_INDEX,
                    INVALID_UV_INDEX,
                } error = NONE;
                isize error_index = 0;

                Vertex composed_vertex = {0};
                Format_Obj_Vertex_Index index = model.indeces.data[i];
                if(index.norm_i1 < 0 || index.norm_i1 > model.normals.size)
                {
                    error = INVALID_NORM_INDEX;
                    error_index = index.norm_i1;
                }
                else
                    composed_vertex.norm = model.normals.data[index.norm_i1 - 1];

                if(index.pos_i1 < 0 || index.pos_i1 > model.positions.size)
                {
                    error = INVALID_POS_INDEX;
                    error_index = index.pos_i1;
                }
                else
                    composed_vertex.pos = model.positions.data[index.pos_i1 - 1];

                if(index.uv_i1 < 0 || index.uv_i1 > model.uvs.size)
                {
                    error = INVALID_UV_INDEX;
                    error_index = index.uv_i1;
                }
                else
                    composed_vertex.uv = model.uvs.data[index.uv_i1 - 1];
        
                
                if(error != NONE)
                {
                    const char* error_string = "";
                    switch(error)
                    {
                        default:
                        case NONE: error_string = "none"; break;

                        case INVALID_NORM_INDEX: error_string = "invalid normal index"; break;
                        case INVALID_POS_INDEX: error_string = "invalid position index"; break;
                        case INVALID_UV_INDEX: error_string = "invalid uv-coordinate index"; break;
                    }

                    LOG_ERROR("ASSET", "Error processing obj file: %s with index %lli on index number %lli ", error_string, (lli) error_index, (lli) i);
                }

                u32 final_index = shape_assembly_add_vertex_custom(&shape_assembly->vertices_hash, &shape_assembly->vertices, composed_vertex);
                u32 mod = i%3;
                triangle.vertex_i[mod] = final_index;
                if(mod == 2)
                {
                    if(discard_triangle == false)
                        array_push(&shape_assembly->triangles, triangle);
                    discard_triangle = false;
                }
            }
        }
    
        Triangle_Mesh_Group_Description group_desc = {0};
        triangle_mesh_group_description_init(&group_desc, def_alloc);
        
        i32 triangles_after = (i32) shape_assembly->triangles.size;

        //No child or next
        group_desc.next_i1 = 0;
        group_desc.child_i1 = 0;

        //Flat for now
        group_desc.depth = 0;

        //assign its range
        group_desc.triangles_from = triangles_before;
        group_desc.triangles_to = triangles_after;

        builder_assign(&group_desc.material_name, string_from_builder(parent_group->material));
        if(parent_group->groups.size > 0)
            builder_assign(&group_desc.name, string_from_builder(parent_group->groups.data[0]));


        //link the previous group to this one
        if(description->groups.size > 0)
            array_last(description->groups)->next_i1 = (i32) description->groups.size + 1;

        array_push(&description->groups, group_desc);
    }
}

INTERNAL void process_mtl_map(Map_Description* description, Format_Mtl_Map map, f32 expected_gamma, i8 channels)
{
    description->info.brigthness = map.modify_brigthness;
    description->info.contrast = map.modify_contrast;
    description->info.gamma = expected_gamma;
    description->info.channels_count = channels;
    description->info.is_enabled = map.path.size > 0;

    Map_Repeat repeat = map.is_clamped ? MAP_REPEAT_CLAMP_TO_EDGE : MAP_REPEAT_REPEAT;
    description->info.repeat_u = repeat;
    description->info.repeat_v = repeat;
    description->info.repeat_w = repeat;

    description->info.filter_magnify = MAP_SCALE_FILTER_BILINEAR;
    description->info.filter_minify = MAP_SCALE_FILTER_BILINEAR;

    description->info.offset = map.offset;
    description->info.scale = map.scale;
    description->info.resolution = vec3_of((f32) map.texture_resolution);
    if(is_nearf(vec3_len(description->info.scale), 0, EPSILON))
        description->info.scale = vec3_of(1);

    builder_assign(&description->path, string_from_builder(map.path));
}

#define GAMMA_SRGB 2.2f
#define GAMMA_LINEAR 1

EXPORT void process_mtl_material(Material_Description* description , Format_Mtl_Material material)
{
    
    builder_assign(&description->name, string_from_builder(material.name));
        
    f32 guessed_ao = 1.0f - CLAMP(vec3_len(material.ambient_color), 0.0f, 1.0f);
    f32 guessed_metallic = (f32)(material.specular_exponent >= 32);
    f32 guessed_rougness = 1.0f - CLAMP(material.specular_exponent / 64.0f, 0.0f, 1.0f);

    description->info.ambient_occlusion = guessed_ao;
    description->info.albedo = material.diffuse_color;
        
    if(material.was_set_pbr_metallic)
        description->info.metallic = material.pbr_metallic;
    else
        description->info.metallic = guessed_metallic;
        
    if(material.was_set_pbr_roughness)
        description->info.roughness = material.pbr_roughness;
    else
        description->info.roughness = guessed_rougness;
        
    description->info.opacity = material.opacity;
    description->info.bump_multiplier = material.map_bump.bump_multiplier;
        
    description->info.emissive_power = vec3_max_len(material.emissive_color);
    description->info.emissive = description->info.emissive_power > 0 ? vec3_max_norm(material.emissive_color) : vec3_of(0);
    description->info.emissive_power_map = 0;
        
    description->info.ambient_color = material.ambient_color;                     
    description->info.diffuse_color = material.diffuse_color;                     
    description->info.specular_color = material.specular_color;                    
    description->info.specular_exponent = material.specular_exponent;                  
    description->info.opacity = material.opacity;      

    //maps
    Map_Description* maps = description->maps;
    Cubemap_Description* cubemaps = description->cubemaps;

    //if has RMA - rougness, metallic, ambient map use it
    if(material.was_set_map_pbr_rma > 0)
    {
        process_mtl_map(&maps[MAP_TYPE_ROUGNESS], material.map_pbr_rma, GAMMA_LINEAR, 1);
        process_mtl_map(&maps[MAP_TYPE_METALLIC], material.map_pbr_rma, GAMMA_LINEAR, 1);
        process_mtl_map(&maps[MAP_TYPE_AMBIENT_OCCLUSION], material.map_pbr_rma, GAMMA_LINEAR, 1);

        maps[MAP_TYPE_ROUGNESS].info.channels_idices1[0] = 1;
        maps[MAP_TYPE_METALLIC].info.channels_idices1[0] = 2;
        maps[MAP_TYPE_AMBIENT_OCCLUSION].info.channels_idices1[0] = 3;
    }

    //... but if has a specific texture it takes priority
    if(material.was_set_map_pbr_roughness > 0)
        process_mtl_map(&maps[MAP_TYPE_ROUGNESS], material.map_pbr_roughness, GAMMA_LINEAR, 1);

    if(material.was_set_map_pbr_metallic)
        process_mtl_map(&maps[MAP_TYPE_METALLIC], material.map_pbr_metallic, GAMMA_LINEAR, 1);

    if(material.was_set_map_pbr_non_standard_ambient_occlusion)
        process_mtl_map(&maps[MAP_TYPE_AMBIENT_OCCLUSION], material.map_pbr_non_standard_ambient_occlusion, GAMMA_LINEAR, 1);

    process_mtl_map(&maps[MAP_TYPE_ALBEDO], material.map_diffuse, GAMMA_SRGB, 3);
    process_mtl_map(&maps[MAP_TYPE_ALPHA], material.map_alpha, GAMMA_LINEAR, 1);
    process_mtl_map(&maps[MAP_TYPE_EMMISIVE], material.map_emmisive, GAMMA_LINEAR, 1);
        
    process_mtl_map(&maps[MAP_TYPE_AMBIENT], material.map_ambient, GAMMA_SRGB, 3);           
    process_mtl_map(&maps[MAP_TYPE_DIFFUSE], material.map_diffuse, GAMMA_SRGB, 3);   
    process_mtl_map(&maps[MAP_TYPE_SPECULAR_COLOR], material.map_specular_color, GAMMA_SRGB, 3);    
    process_mtl_map(&maps[MAP_TYPE_SPECULAR_HIGHLIGHT], material.map_specular_highlight, GAMMA_LINEAR, 1);

    process_mtl_map(&maps[MAP_TYPE_BUMP], material.map_bump, GAMMA_LINEAR, 1);
    process_mtl_map(&maps[MAP_TYPE_DISPLACEMENT], material.map_displacement, GAMMA_LINEAR, 1);
    process_mtl_map(&maps[MAP_TYPE_STENCIL], material.map_stencil, GAMMA_LINEAR, 1);
    process_mtl_map(&maps[MAP_TYPE_NORMAL], material.map_normal, GAMMA_LINEAR, 3);

    process_mtl_map(&maps[MAP_TYPE_REFLECTION], material.map_reflection_sphere, GAMMA_SRGB, 3);

    process_mtl_map(&cubemaps[CUBEMAP_TYPE_REFLECTION].top, material.map_reflection_cube_top, GAMMA_SRGB, 3);
    process_mtl_map(&cubemaps[CUBEMAP_TYPE_REFLECTION].bottom, material.map_reflection_cube_bottom, GAMMA_SRGB, 3);
    process_mtl_map(&cubemaps[CUBEMAP_TYPE_REFLECTION].front, material.map_reflection_cube_front, GAMMA_SRGB, 3);
    process_mtl_map(&cubemaps[CUBEMAP_TYPE_REFLECTION].back, material.map_reflection_cube_back, GAMMA_SRGB, 3);
    process_mtl_map(&cubemaps[CUBEMAP_TYPE_REFLECTION].left, material.map_reflection_cube_left, GAMMA_SRGB, 3);
    process_mtl_map(&cubemaps[CUBEMAP_TYPE_REFLECTION].right, material.map_reflection_cube_right, GAMMA_SRGB, 3);
}

EXPORT Error material_load_images(Material* material, Material_Description description)
{
    Error out_error = {0};
    material->info = description.info;

    typedef struct Map_Or_Cubemap_Handles {
        Id handle;
        Map_Description* description;
        bool is_cubemap;
        i32 map_or_cubemap_i; 
        i32 face_i;
    } Map_Or_Cubemap_Handles;

    Map_Or_Cubemap_Handles maps_or_cubemaps[MAP_TYPE_ENUM_COUNT + CUBEMAP_TYPE_ENUM_COUNT*6] = {0};

    //Add all images to be loaded
    for(i32 i = 0; i < MAP_TYPE_ENUM_COUNT; i++)
    {
        maps_or_cubemaps[i].description = &description.maps[i];
        maps_or_cubemaps[i].map_or_cubemap_i = i;
    }
    
    //Add all cubemaps to be loaded
    for(i32 i = 0; i < CUBEMAP_TYPE_ENUM_COUNT; i++)
    {
        for(i32 j = 0; j < 6; j++)
        {
            i32 index = MAP_TYPE_ENUM_COUNT + i*6 + j;
            maps_or_cubemaps[index].description = &description.cubemaps[i].faces[j];
            maps_or_cubemaps[index].map_or_cubemap_i = i;
            maps_or_cubemaps[index].face_i = j;
            maps_or_cubemaps[index].is_cubemap = true;
        }
    }

    //Load everything
    for(isize i = 0; i < STATIC_ARRAY_SIZE(maps_or_cubemaps); i++)
    {
        Map_Or_Cubemap_Handles map_or_cubemap = maps_or_cubemaps[i];

        //if this image is not enabled or does not have a path dont even try
        if(map_or_cubemap.description->info.is_enabled == false || map_or_cubemap.description->path.size <= 0)
            continue;
        
        String item_path = string_from_builder(map_or_cubemap.description->path);
        String dir_path = path_get_file_directory(string_from_builder(description.path));

        //@TODO: make into string builder and dont relly on ephemerals since we are fairly high up!
        String full_item_path = path_get_full_ephemeral_from(item_path, dir_path);
        Error load_error = {0};
        
        Id found_image = image_find_by_path(hash_string_from_string(full_item_path), NULL);
        Id map_image = NULL;

        Resource_Params params = {0};
        params.path = full_item_path;
        params.name = path_get_name_from_path(params.path);
        params.was_loaded = true;

        if(found_image != NULL)
        {
            LOG_INFO("ASSET", "load image found image \"%s\" in repository", full_item_path.data);
            map_image = image_make_shared(found_image);
        }
        else
        {   
            map_image = image_insert(params);
            Image_Builder* loaded_image = image_get(map_image); 

            load_error = image_read_from_file(loaded_image, full_item_path, 0, PIXEL_FORMAT_U8, 0);
            if(error_is_ok(load_error) == false)
                image_remove(map_image);
        }

        if(map_image == NULL)
            LOG_ERROR("ASSET", "Error rading image " STRING_FMT ": " ERROR_FMT, STRING_PRINT(full_item_path), ERROR_PRINT(load_error));
        else
        {
            Map* map = NULL;
            if(map_or_cubemap.is_cubemap)
                map = &material->cubemaps[map_or_cubemap.map_or_cubemap_i].maps.faces[map_or_cubemap.face_i];
            else
                map = &material->maps[map_or_cubemap.map_or_cubemap_i];

            map->image = map_image;
            map->info = map_or_cubemap.description->info;
        }
    }

    return out_error;
}

#define FLAT_ERRORS_COUNT 100

EXPORT Error material_read_entire(Id_Array* materials, String path)
{
    Malloc_Allocator arena = {0};
    malloc_allocator_init_use(&arena, 0);

    String_Builder full_path =  {0};
    String_Builder file_content = {0};
    Format_Mtl_Material_Array mtl_materials = {0};
    Material_Description material_desription = {0};

    array_init_backed(&full_path, NULL, 512);
    Error error = path_get_full(&full_path, path);
    if(error_is_ok(error))
    {
        //@TODO: In the future we will want to separate the concept of resource from
        //       path and name. Resource is simply the data and name and path are a means
        //       of acessing that data. There sould be a bidirectional hash amp linakge between
        //       name/path and handle. This repository could also contain when the file was loaded and other things.
        //       In addition to being nicer to work with it will also be faster since we will not need to 
        //       iterate all resources when looking for path.

        Platform_File_Info file_info = {0};
        Error file_info_error = error_from_platform(platform_file_info(full_path.data, &file_info));
        if(error_is_ok(file_info_error) == false)
            LOG_ERROR("ASSET", "Error getting info of material file %s: " ERROR_FMT, full_path.data, ERROR_PRINT(file_info_error));

        bool was_found = false;
        bool is_outdated = false;
    
        Id_Array found_materials = {0};
        array_init_backed(&found_materials, NULL, 16);

        isize last = -1;
        Id found_material = {0};
        while(true)
        {
            found_material = material_find_by_path(hash_string_from_builder(full_path), &last);
            if(found_material == NULL)
                break;

            if(error_is_ok(file_info_error))
            {
                Resource_Info* material_info = NULL;
                material_get_with_info(found_material, &material_info);
                if(material_info->load_etime < file_info.last_write_epoch_time)
                {
                    is_outdated = true;
                    break;
                }
            }

            array_push(&found_materials, found_material);
        }

        if(is_outdated == false)
        {
            for(isize i = 0; i < found_materials.size; i++)
                array_push(materials, material_make_shared(found_material););
            
            was_found = found_materials.size > 0;
        }

        if((was_found == false || is_outdated))
        {
            error = file_read_entire(string_from_builder(full_path), &file_content);
            if(error_is_ok(error))
            {
                Format_Obj_Mtl_Error mtl_errors[FLAT_ERRORS_COUNT] = {0};
                isize had_mtl_errors = 0;
                format_mtl_read(&mtl_materials, string_from_builder(file_content), mtl_errors, FLAT_ERRORS_COUNT, &had_mtl_errors);

                for(isize i = 0; i < had_mtl_errors; i++)
                    LOG_ERROR("ASSET", "Error parsing material file %s: " OBJ_MTL_ERROR_FMT, full_path.data, OBJ_MTL_ERROR_PRINT(mtl_errors[i]));
            
                for(isize i = 0; i < mtl_materials.size; i++)
                {
                    process_mtl_material(&material_desription, mtl_materials.data[i]);
                    array_copy(&material_desription.path, full_path);
                
                    Resource_Params params = {0};
                    params.name = string_from_builder(material_desription.name);
                    params.path = string_from_builder(material_desription.path);
                    params.was_loaded = true;

                    Id local_handle = material_insert(params);
                    Material* material = material_get(local_handle);
                    material_load_images(material, material_desription);
                    
                    array_push(materials, local_handle);
                }
            }
        }
    }
    
    if(error_is_ok(error) == false)
        LOG_ERROR("ASSET", "Error rading material file %s: " ERROR_FMT, full_path.data, ERROR_PRINT(error));

    malloc_allocator_deinit(&arena);

    return error;
}

EXPORT Error triangle_mesh_read_entire(Id* triangle_mesh_handle, String path)
{
    Malloc_Allocator arena = {0};
    malloc_allocator_init_use(&arena, 0);

    Id out_handle = {0};
    String_Builder full_path = {0};
    array_init_backed(&full_path, 0, 512);
    Error error = path_get_full(&full_path, path);
    
    if(error_is_ok(error))
    {
        Platform_File_Info file_info = {0};
        Error file_info_error = error_from_platform(platform_file_info(full_path.data, &file_info));
        if(error_is_ok(file_info_error) == false)
            LOG_ERROR("ASSET", "Error getting info of object file %s: " ERROR_FMT, full_path.data, ERROR_PRINT(file_info_error));

        Id found_mesh = triangle_mesh_find_by_path(hash_string_from_builder(full_path), NULL);
        if(found_mesh != NULL)
        {
            Resource_Info* resource_info = NULL;
            triangle_mesh_get_with_info(found_mesh, &resource_info);
            if(resource_info->load_etime >= file_info.last_write_epoch_time)
                out_handle = triangle_mesh_make_shared(found_mesh);
        }

        if(out_handle == NULL)
        {
            String_Builder file_content = {0};
            error = file_read_entire(string_from_builder(full_path), &file_content);

            if(error_is_ok(error))
            {
                Format_Obj_Model model = {0};
                format_obj_model_init(&model, 0);

                Format_Obj_Mtl_Error obj_errors[FLAT_ERRORS_COUNT] = {0};
                isize had_obj_errors = 0;
                format_obj_read(&model, string_from_builder(file_content), obj_errors, FLAT_ERRORS_COUNT, &had_obj_errors);

                //@NOTE: So that we dont waste too much memory while loading
                array_deinit(&file_content); 

                for(isize i = 0; i < had_obj_errors; i++)
                    LOG_ERROR("ASSET", "Error parsing obj file %s: " OBJ_MTL_ERROR_FMT, full_path.data, OBJ_MTL_ERROR_PRINT(obj_errors[i]));

                Resource_Params params = {0};
                params.path = string_from_builder(full_path);
                params.name = path_get_name_from_path(params.path);
                params.was_loaded = true;

                out_handle = triangle_mesh_insert(params);
                Triangle_Mesh* triangle_mesh = triangle_mesh_get(out_handle);

                triangle_mesh->shape = shape_insert(params);
                Shape_Assembly* out_assembly = shape_get(triangle_mesh->shape);

                Triangle_Mesh_Description description = {0};
                process_obj_triangle_mesh(out_assembly, &description, model);

                String dir_path = path_get_file_directory(string_from_builder(full_path));
                for(isize i = 0; i < description.material_files.size; i++)
                {
                    String file = string_from_builder(description.material_files.data[i]);
                    String mtl_path = path_get_full_ephemeral_from(file, dir_path);

                    Error major_material_error = material_read_entire(&triangle_mesh->materials, mtl_path);

                    if(error_is_ok(major_material_error) == false)
                        LOG_ERROR("ASSET", "Failed to load material file %s while loading %s", mtl_path.data, full_path.data);
                }

                for(isize i = 0; i < description.groups.size; i++)
                {
                    Triangle_Mesh_Group_Description* group_desc = &description.groups.data[i];
        
                    bool unable_to_find_group = true;
                    for(isize j = 0; j < triangle_mesh->materials.size; j++)
                    {
                        Id material_handle = triangle_mesh->materials.data[j];

                        
                        Resource_Info* material_info = NULL;
                        if(material_get_with_info(material_handle, &material_info) == NULL)
                            continue;

                        String material_name = string_from_builder(material_info->name);
                        if(string_is_equal(material_name, string_from_builder(group_desc->material_name)))
                        {
                            unable_to_find_group = false;

                            Triangle_Mesh_Group group = {0};

                            name_from_string(&group.name, material_name);
                            group.material_i1 = (i32) j + 1;

                            group.next_i1 = group_desc->next_i1;
                            group.child_i1 = group_desc->child_i1;
                            group.depth = group_desc->depth;

                            group.triangles_from = group_desc->triangles_from;
                            group.triangles_to = group_desc->triangles_to;

                            array_push(&triangle_mesh->groups, group);
                        }

                        //find the default material
                        if(triangle_mesh->material == NULL && string_is_equal(material_name, STRING("default")))
                            triangle_mesh->material = material_make_shared(material_handle);
                    }
                    
                    if(unable_to_find_group)
                        LOG_ERROR("ASSET", "Failed to find a material called %s while loadeing %s", group_desc->material_name.data, description.path.data);
                }
            }
            else
            {
                LOG_ERROR("ASSET", "Failed to load triangle_mesh file %s", full_path.data);
            }
        }
    }
    
    if(error_is_ok(error) == false)
        LOG_ERROR("ASSET", "Error rading material file %s: " ERROR_FMT, full_path.data, ERROR_PRINT(error));
    
    malloc_allocator_deinit(&arena);
    *triangle_mesh_handle = out_handle;
    return error;
}
