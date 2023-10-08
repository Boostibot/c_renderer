#pragma once
#include "resources.h"
#include "format_obj.h"
#include "error.h"
#include "file.h"
#include "profile.h"
#include "image_loader.h"

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

EXPORT void process_obj_object(Shape_Assembly* shape_assembly, Object_Description* description, Format_Obj_Model model)
{
    hash_index_reserve(&shape_assembly->vertices_hash, model.indeces.size);
    
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

                    LOG_ERROR("ASSET", "Error processing obj file: %s with index %lld on index number %lld ", error_string, (lld) error_index, (lld) i);
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
    
        Object_Group_Description group_desc = {0};
        object_group_description_init(&group_desc, def_alloc);
        
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

    description->info.offset = vec2_from_vec3(map.offset);
    description->info.scale = vec2_from_vec3(map.scale);
    description->info.resolution = vec2_of((f32) map.texture_resolution);
    if(is_nearf(vec2_len(description->info.scale), 0, EPSILON))
        description->info.scale = vec2_of(1);

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

EXPORT Error material_load_images(Resources* resources, Material* material, Material_Description description)
{
    Error out_error = {0};

    array_copy(&material->name, description.name);
    array_copy(&material->path, description.path);
    
    material->info = description.info;

    Map* to_all[MAP_TYPE_ENUM_COUNT + CUBEMAP_TYPE_ENUM_COUNT*6] = {NULL};
    Map_Description* desc_all[MAP_TYPE_ENUM_COUNT + CUBEMAP_TYPE_ENUM_COUNT*6] = {NULL};

    //Add all images to be loaded
    for(isize i = 0; i < MAP_TYPE_ENUM_COUNT; i++)
    {
        to_all[i] = &material->maps[i];
        desc_all[i] = &description.maps[i];
    }
    
    //Add all cubemaps to be loaded
    for(isize i = 0; i < CUBEMAP_TYPE_ENUM_COUNT; i++)
    {
        for(isize j = 0; j < 6; j++)
        {
            isize index = MAP_TYPE_ENUM_COUNT + i*6 + j;
            to_all[index] = &material->cubemaps[i].faces[j];;
            desc_all[index] = &description.cubemaps[i].faces[j];
        }
    }

    Allocator_Set prev = allocator_set_default(resources->alloc);

    for(isize i = 0; i < STATIC_ARRAY_SIZE(to_all); i++)
    {
        Map* to = to_all[i];
        Map_Description* desc = desc_all[i];

        //if this image is not enabled or does not have a path dont even try
        if(desc->info.is_enabled == false || desc->path.size <= 0)
            continue;
        
        //@TODO: full path here
        String item_path = string_from_builder(desc->path);
        String dir_path = path_get_file_directory(string_from_builder(description.path));
            
        String_Builder composed_path = builder_from_string(dir_path, NULL);
        builder_append(&composed_path, item_path);

        Error load_error = {0};
        //look for loaded map inside resources.
        //@SPEED: linear search here. This will only become problematic once we start loading A LOT of images.
        //        I am guessing up to 1000 images this doenst make sense to optimize in any way.
        Loaded_Image_Handle found_handle = {0};
        HANDLE_TABLE_FOR_EACH_BEGIN(resources->images, h, Loaded_Image*, image_ptr)
            if(builder_is_equal(image_ptr->path, composed_path))
            {
                found_handle.h = h;
                break;
            }
        HANDLE_TABLE_FOR_EACH_END

        //If not found load it from disk
        //@TODO: make macro for checking null handles
        if(found_handle.h.index == 0)
        {
            Loaded_Image loaded = {0};
            loaded_image_init(&loaded, resources->alloc);

            load_error = image_read_from_file(&loaded.image, string_from_builder(composed_path), 0, PIXEL_FORMAT_U8, 0);
            if(error_is_ok(load_error))
            {
                array_copy(&loaded.path, composed_path);
                found_handle = resources_loaded_image_add(resources, &loaded);
            }
            else
                LOG_ERROR("ASSET", "Error rading image %s: " ERROR_FMT, composed_path.data, ERROR_PRINT(load_error));
            
            loaded_image_deinit(&loaded);
        }

         to->image = found_handle;
         to->info = desc->info;
         array_copy(&to->path, desc->path);
         out_error = ERROR_OR(out_error) load_error;

         array_deinit(&composed_path);
    }

    allocator_set(prev);

    return out_error;
}

EXPORT void object_finalize(Resources* resources, Object* object, Shape_Assembly* assembly, Object_Description description, Material_Handle_Array materials)
{
    Shape_Assembly_Handle assembly_handle = resources_shape_add(resources, assembly);
    object_init(object, resources);

    object->shape = assembly_handle;
    array_copy(&object->name, description.name);
    array_copy(&object->path, description.path);

    if(materials.size > 0)
        object->fallback_material = materials.data[0];

    //@TEMP
    for(isize j = 0; j < materials.size; j++)
    {
        Material_Handle material_handle = materials.data[j];
        Material* material = resources_material_get(resources, material_handle);
        if(material == NULL)
            continue;
    }

    for(isize i = 0; i < description.groups.size; i++)
    {
        Object_Group_Description* group_desc = &description.groups.data[i];
        
        bool unable_to_find_group = true;
        for(isize j = 0; j < materials.size; j++)
        {
            Material_Handle material_handle = materials.data[j];
            Material* material = resources_material_get(resources, material_handle);
            if(material == NULL)
                continue;

            if(builder_is_equal(group_desc->material_name, material->name))
            {
                unable_to_find_group = false;

                Object_Group group = {0};
                object_group_init(&group, resources);

                array_copy(&group.name, group_desc->name);
                group.material = material_handle;

                group.next_i1 = group_desc->next_i1;
                group.child_i1 = group_desc->child_i1;
                group.depth = group_desc->depth;

                group.triangles_from = group_desc->triangles_from;
                group.triangles_to = group_desc->triangles_to;

                array_push(&object->groups, group);
            }
        }

        if(unable_to_find_group)
            LOG_ERROR("ASSET", "Failed to find a material called %s while loadeing %s", group_desc->material_name.data, description.path.data);
    }
}

#define FLAT_ERRORS_COUNT 100

EXPORT Error material_read_entire(Resources* resources, Material_Handle_Array* materials, String path)
{
    Malloc_Allocator arena = {0};
    malloc_allocator_init_use(&arena, 0);

    String_Builder full_path = {0};
    String_Builder file_content = {0};
    Format_Mtl_Material_Array mtl_materials = {0};
    Material_Description material_desription = {0};

    array_init_backed(&full_path, NULL, 512);
    Error error = file_get_full_path(&full_path, path);
    
    //@TODO: add dirty flag or something similar so this handles reloading.
    //       For now we can only reload if we eject all resources
    bool was_found = false;

    if(error_is_ok(error))
    {
        //search for the material. If found return its duplicated handle.
        HANDLE_TABLE_FOR_EACH_BEGIN(resources->materials, h, Object*, found_object)
            if(builder_is_equal(full_path, found_object->path))
            {
                was_found = true;
                Material_Handle local_handle = {h};
                Material_Handle duplicate_handle = resources_material_share(resources, local_handle);

                array_push(materials, duplicate_handle);
            }
        HANDLE_TABLE_FOR_EACH_END
    }

    if(error_is_ok(error) && was_found == false)
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
                Material material = {0};
                material_init(&material, resources);

                process_mtl_material(&material_desription, mtl_materials.data[i]);
                array_copy(&material_desription.path, full_path);

                material_load_images(resources, &material, material_desription);
                Material_Handle local_handle = resources_material_add(resources, &material);
                    
                array_push(materials, local_handle);

            }
        }
    }
    
    if(error_is_ok(error) == false)
        LOG_ERROR("ASSET", "Error rading material file %s: " ERROR_FMT, full_path.data, ERROR_PRINT(error));

    #ifdef DO_WORTHLESS_DEINITS
    material_description_deinit(&material_desription);
    for(isize i = 0; i < mtl_materials.size; i++)
        format_obj_material_info_deinit(&mtl_materials.data[i]);

    array_deinit(&mtl_materials);
    array_deinit(&file_content);
    array_deinit(&full_path);
    allocator_set(allocs_before);
    #endif

    malloc_allocator_deinit(&arena);

    return error;
}

EXPORT Error object_read_entire(Resources* resources, Object_Handle* object, String path)
{
    Debug_Allocator debug_allocator = {0};
    debug_allocator_init_use(&debug_allocator, DEBUG_ALLOCATOR_CAPTURE_CALLSTACK | DEBUG_ALLOCATOR_DEINIT_LEAK_CHECK);

    Object_Handle out_handle = {0};
    String_Builder full_path = {0};
    Allocator* scratch = allocator_get_scratch();
    array_init_backed(&full_path, scratch, 512);
    Error error = file_get_full_path(&full_path, path);
    
    if(error_is_ok(error))
    {
        //search for an object. If found return its duplicated handle.
        HANDLE_TABLE_FOR_EACH_BEGIN(resources->objects, h, Object*, found_object)
            if(builder_is_equal(full_path, found_object->path))
            {
                Object_Handle local_handle = {h};
                out_handle = resources_object_share(resources, local_handle);
                break;
            }
        HANDLE_TABLE_FOR_EACH_END

        //@TODO factor
        if(out_handle.h.index == 0)
        {
            Malloc_Allocator arena = {0};
            malloc_allocator_init_use(&arena, 0);

            Allocator_Set allocs_before = allocator_set_default(resources->alloc);

            String_Builder file_content = {scratch};
            String_Builder mtl_path = {scratch};
            error = file_read_entire(string_from_builder(full_path), &file_content);

            if(error_is_ok(error))
            {
                Format_Obj_Model model = {0};
                format_obj_model_init(&model, scratch);

                Format_Obj_Mtl_Error obj_errors[FLAT_ERRORS_COUNT] = {0};
                isize had_obj_errors = 0;
                format_obj_read(&model, string_from_builder(file_content), obj_errors, FLAT_ERRORS_COUNT, &had_obj_errors);

                for(isize i = 0; i < had_obj_errors; i++)
                    LOG_ERROR("ASSET", "Error parsing obj file %s: " OBJ_MTL_ERROR_FMT, full_path.data, OBJ_MTL_ERROR_PRINT(obj_errors[i]));

                Shape_Assembly out_assembly = {0};
                shape_assembly_init(&out_assembly, resources->alloc);

                String dir_path = path_get_file_directory(string_from_builder(full_path));

                Object_Description description = {0};
                process_obj_object(&out_assembly, &description, model);
                array_copy(&description.path, full_path);

                Object out_object = {0};
                object_init(&out_object, resources);

                Material_Handle_Array materials = {resources->alloc};
                for(isize i = 0; i < description.material_files.size; i++)
                {
                    String file = string_from_builder(description.material_files.data[i]);

                    array_clear(&mtl_path);
                    builder_append(&mtl_path, dir_path);
                    builder_append(&mtl_path, file);

                    Error major_material_error = material_read_entire(resources, &materials, string_from_builder(mtl_path));

                    if(error_is_ok(major_material_error) == false)
                        LOG_ERROR("ASSET", "Failed to load material file %s while loading %s", mtl_path.data, full_path.data);

                }
            
                object_finalize(resources, &out_object, &out_assembly, description, materials);
                object_description_deinit(&description);

                out_handle = resources_object_add(resources, &out_object);
                array_deinit(&materials);

                format_obj_model_deinit(&model);
            }
            else
            {
                LOG_ERROR("ASSET", "Failed to load object file %s", full_path.data);
            }
            
            array_deinit(&mtl_path);
            array_deinit(&file_content);
            
            allocator_set(allocs_before);
            malloc_allocator_deinit(&arena);
        }
    }
    
    if(error_is_ok(error) == false)
        LOG_ERROR("ASSET", "Error rading material file %s: " ERROR_FMT, full_path.data, ERROR_PRINT(error));

    array_deinit(&full_path);
    
    debug_allocator_deinit(&debug_allocator);
    *object = out_handle;
    return error;
}
