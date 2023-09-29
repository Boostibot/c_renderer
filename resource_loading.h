#pragma once
#include "resources.h"
#include "format_obj.h"
#include "error.h"
#include "file.h"
#include "profile.h"
#include "image_loader.h"

//void obj_load_description(String path, Shape* shape, String_Builder_Array* material_names);
//void mtl_load_description(String path, Material_Description_Array material_descriptions);
//
//void mtl_finilize(Resources* resources, Material_Description description, Material* material);
//void obj_finalize(Shape_Handle shape_assembly, Object_Description description, Material_Ref_Array materials, Object* object);
//
//Error obj_load(Resources* resources, String path, Object* object);

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

//@TODO: Move into math.h
//Returns the maximum compoment of a vector.
//This is also the maximum norm
INTERNAL f32 vec3_max_len(Vec3 vec)
{
    f32 max1 = MAX(vec.x, vec.y);
    f32 max = MAX(max1, vec.z);

    return max;
}

//Normalize vector using the maximum norm.
INTERNAL Vec3 vec3_max_norm(Vec3 vec)
{
    return vec3_scale(vec, 1.0f / vec3_max_len(vec));
}

EXPORT void process_obj_object(Shape_Assembly* shape_assembly, Object_Description* description, Format_Obj_Model model, Error_Array* errors_or_null, isize max_errors)
{
    hash_index_reserve(&shape_assembly->vertices_hash, model.indeces.size);
    
    //Copy over materials
    for(isize i = 0; i < model.material_files.size; i++)
    {
        String material_file = string_from_builder(model.material_files.data[i]);
        array_push(&description->material_files, builder_from_string(material_file));
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
                Format_Obj_Mtl_Error error = {0};
                Vertex composed_vertex = {0};
                Format_Obj_Vertex_Index index = model.indeces.data[i];
                if(index.norm_i1 < 0 || index.norm_i1 > model.normals.size)
                {
                    error.statement = FORMAT_OBJ_ERROR_INVALID_NORM_INDEX;
                    error.line = index.norm_i1;
                }
                else
                    composed_vertex.norm = model.normals.data[index.norm_i1 - 1];

                if(index.pos_i1 < 0 || index.pos_i1 > model.positions.size)
                {
                    error.statement = FORMAT_OBJ_ERROR_INVALID_POS_INDEX;
                    error.line = index.pos_i1;
                }
                else
                    composed_vertex.pos = model.positions.data[index.pos_i1 - 1];

                if(index.uv_i1 < 0 || index.uv_i1 > model.uvs.size)
                {
                    error.statement = FORMAT_OBJ_ERROR_INVALID_UV_INDEX;
                    error.line = index.uv_i1;
                }
                else
                    composed_vertex.uv = model.uvs.data[index.uv_i1 - 1];
        
                if(error.statement != FORMAT_OBJ_ERROR_NONE && errors_or_null && errors_or_null->size < max_errors)
                    array_push(errors_or_null, _format_obj_mtl_error_on_to_error(error.statement, error.index));

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
        group_desc.vertices_from = triangles_before;
        group_desc.vertices_to = triangles_after;

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
    description->info.contrast_minus_one = map.modify_contrast;
    description->info.gamma = expected_gamma;
    description->info.channels_count = channels;
    description->info.is_enabled = map.path.size > 0;
    description->info.is_clamped = map.is_clamped;
    description->info.offset = vec2_from_vec3(map.offset);
    description->info.scale = vec2_from_vec3(map.scale);
    description->info.resolution = vec2_of((f32) map.texture_resolution);
    if(is_nearf(vec2_len(description->info.scale), 0, EPSILON))
        description->info.scale = vec2_of(1);

    builder_assign(&description->path, string_from_builder(map.path));
}

#define GAMMA_SRGB 2.2f
#define GAMMA_LINEAR 1

EXPORT void process_mtl_material(Material_Description* description_or_null, Material_Phong_Description* description_phong_or_null, Format_Mtl_Material material)
{
    Material_Description* pbr = description_or_null;
    Material_Phong_Description* phong = description_phong_or_null;

    if(pbr)
    {
        builder_assign(&pbr->name, string_from_builder(material.name));

        //values
        pbr->albedo = material.diffuse_color;
        pbr->metalic = material.pbr_metallic;
        pbr->roughness = material.pbr_roughness;
        pbr->opacity = material.opacity;

        f32 ao_guess = 1.0f - CLAMP(vec3_len(material.ambient_color), 0.0f, 1.0f);
        pbr->ambient_occlusion = ao_guess;

        pbr->bump_multiplier_minus_one = material.map_bump.bump_multiplier - 1;
        
        pbr->emissive_power = vec3_max_len(material.emissive_color);
        pbr->emissive = pbr->emissive_power > 0 ? vec3_max_norm(material.emissive_color) : vec3_of(0);
        pbr->emissive_power_map = 0;
        
        //maps

        //if has RMA - rougness, metalic, ambient map use it
        if(material.map_pbr_rma.path.size > 0)
        {
            process_mtl_map(&pbr->map_roughness, material.map_pbr_rma, GAMMA_LINEAR, 1);
            process_mtl_map(&pbr->map_metallic, material.map_pbr_rma, GAMMA_LINEAR, 1);
            process_mtl_map(&pbr->map_ambient_occlusion, material.map_pbr_rma, GAMMA_LINEAR, 1);

            pbr->map_roughness.info.channels_idices1[0] = 1;
            pbr->map_metallic.info.channels_idices1[0] = 2;
            pbr->map_ambient_occlusion.info.channels_idices1[0] = 3;
        }

        //... but if has a specific texture it takes priority
        if(material.map_pbr_roughness.path.size > 0)
            process_mtl_map(&pbr->map_roughness, material.map_pbr_roughness, GAMMA_LINEAR, 1);

        if(material.map_pbr_metallic.path.size > 0)
            process_mtl_map(&pbr->map_metallic, material.map_pbr_metallic, GAMMA_LINEAR, 1);

        if(material.map_pbr_non_standard_ambient_occlusion.path.size > 0)
            process_mtl_map(&pbr->map_ambient_occlusion, material.map_pbr_non_standard_ambient_occlusion, GAMMA_LINEAR, 1);
            
        process_mtl_map(&pbr->map_albedo, material.map_diffuse, GAMMA_SRGB, 3);

        process_mtl_map(&pbr->map_alpha, material.map_alpha, GAMMA_LINEAR, 1);
        process_mtl_map(&pbr->map_emmisive, material.map_alpha, GAMMA_LINEAR, 1);
        
        process_mtl_map(&pbr->map_bump, material.map_alpha, GAMMA_LINEAR, 1);
        process_mtl_map(&pbr->map_displacement, material.map_alpha, GAMMA_LINEAR, 1);
        process_mtl_map(&pbr->map_stencil, material.map_alpha, GAMMA_LINEAR, 1);
        process_mtl_map(&pbr->map_normal, material.map_alpha, GAMMA_LINEAR, 3);

        process_mtl_map(&pbr->map_reflection_sphere, material.map_alpha, GAMMA_SRGB, 3);

        process_mtl_map(&pbr->map_reflection_cube.top, material.map_reflection_cube_top, GAMMA_SRGB, 3);
        process_mtl_map(&pbr->map_reflection_cube.bottom, material.map_reflection_cube_bottom, GAMMA_SRGB, 3);
        process_mtl_map(&pbr->map_reflection_cube.front, material.map_reflection_cube_front, GAMMA_SRGB, 3);
        process_mtl_map(&pbr->map_reflection_cube.back, material.map_reflection_cube_back, GAMMA_SRGB, 3);
        process_mtl_map(&pbr->map_reflection_cube.left, material.map_reflection_cube_left, GAMMA_SRGB, 3);
        process_mtl_map(&pbr->map_reflection_cube.right, material.map_reflection_cube_right, GAMMA_SRGB, 3);
    }

    if(phong)
    {
        builder_assign(&phong->name, string_from_builder(material.name));
    
        phong->ambient_color = material.ambient_color;                     
        phong->diffuse_color = material.diffuse_color;                     
        phong->specular_color = material.specular_color;                    
        phong->specular_exponent = material.specular_exponent;                  
        phong->opacity = material.opacity;      
    
        phong->bump_multiplier_minus_one = material.map_bump.bump_multiplier - 1;                    

        process_mtl_map(&phong->map_ambient, material.map_ambient, GAMMA_SRGB, 3);           
        process_mtl_map(&phong->map_diffuse, material.map_diffuse, GAMMA_SRGB, 3);   
        process_mtl_map(&phong->map_specular_color, material.map_specular_color, GAMMA_SRGB, 3);    
        process_mtl_map(&phong->map_specular_highlight, material.map_specular_highlight, GAMMA_LINEAR, 1);

        process_mtl_map(&phong->map_alpha, material.map_alpha, GAMMA_LINEAR, 1);             
        process_mtl_map(&phong->map_bump, material.map_bump, GAMMA_LINEAR, 1);              
        process_mtl_map(&phong->map_displacement, material.map_displacement, GAMMA_LINEAR, 1);      
        process_mtl_map(&phong->map_stencil, material.map_stencil, GAMMA_LINEAR, 1);           
        process_mtl_map(&phong->map_normal, material.map_normal, GAMMA_LINEAR, 3);          
        
        process_mtl_map(&phong->map_reflection_sphere, material.map_reflection_sphere, GAMMA_SRGB, 3);

        process_mtl_map(&phong->map_reflection_cube.top, material.map_reflection_cube_top, GAMMA_SRGB, 3);
        process_mtl_map(&phong->map_reflection_cube.bottom, material.map_reflection_cube_bottom, GAMMA_SRGB, 3);
        process_mtl_map(&phong->map_reflection_cube.front, material.map_reflection_cube_front, GAMMA_SRGB, 3);
        process_mtl_map(&phong->map_reflection_cube.back, material.map_reflection_cube_back, GAMMA_SRGB, 3);
        process_mtl_map(&phong->map_reflection_cube.left, material.map_reflection_cube_left, GAMMA_SRGB, 3);
        process_mtl_map(&phong->map_reflection_cube.right, material.map_reflection_cube_right, GAMMA_SRGB, 3);
    }
}

EXPORT Error material_load_images(Resources* resources, Material* material, Material_Description description, Error_Array* errors_or_null, isize max_errors)
{
    Error out_error = {0};
    
    typedef struct {
        Map* to;
        Map_Description* from;
    } Map_Pairing;

    Map_Pairing pairings[] = {
        #define MAP_PAIRING(name) {&material->name, &description.name}

        MAP_PAIRING(map_albedo),  
        MAP_PAIRING(map_roughness),           
        MAP_PAIRING(map_ambient_occlusion),           
        MAP_PAIRING(map_metallic),           
        MAP_PAIRING(map_emmisive),
    
        MAP_PAIRING(map_alpha),  
        MAP_PAIRING(map_bump),              
        MAP_PAIRING(map_displacement),      
        MAP_PAIRING(map_stencil),  
        MAP_PAIRING(map_normal),
    
        MAP_PAIRING(map_reflection_sphere), 
        MAP_PAIRING(map_reflection_cube.top), 
        MAP_PAIRING(map_reflection_cube.bottom), 
        MAP_PAIRING(map_reflection_cube.front), 
        MAP_PAIRING(map_reflection_cube.back), 
        MAP_PAIRING(map_reflection_cube.left), 
        MAP_PAIRING(map_reflection_cube.right), 

        #undef MAP_PAIRING
    };

    array_copy(&material->name, description.name);
    array_copy(&material->path, description.path);
    
    //@TODO: maka this into an info object or completely merge descriptions with representations and add bool is_loaded to Maps
    material->albedo = description.albedo;
    material->emissive = description.emissive;
    material->roughness = description.roughness;
    material->metalic = description.metalic;
    material->ambient_occlusion = description.ambient_occlusion;
    material->opacity = description.opacity;
    material->emissive_power = description.emissive_power;

    material->emissive_power_map = description.emissive_power_map;
    material->bump_multiplier_minus_one = description.bump_multiplier_minus_one;

    for(isize i = 0; i < STATIC_ARRAY_SIZE(pairings); i++)
    {
        Map_Pairing pairing = pairings[i];
        Error load_error = {0};
        //look for loaded map inside resources.
        //@SPEED: linear search here. This will only become problematic once we start loading A LOT of images.
        //        I am guessing up to 1000 images this doenst make sense to optimize in any way.
        Loaded_Image_Handle found_handle = {0};
        HANDLE_TABLE_FOR_EACH_BEGIN(resources->images, h, Loaded_Image*, image_ptr)
            if(builder_is_equal(image_ptr->path, pairing.from->path))
            {
                found_handle.h = h;
                break;
            }
        HANDLE_TABLE_FOR_EACH_END

        //If not found load it from disk
        //@TODO: make macro for checking null handles
        if(found_handle.h.index == 0)
        {
            String path = string_from_builder(pairing.from->path);
            Loaded_Image loaded = {0};
            loaded_image_init(&loaded, resources->alloc);

            load_error = image_read_from_file(&loaded.image, path, 0, PIXEL_FORMAT_U8, 0);
            if(error_is_ok(load_error))
            {
                builder_assign(&loaded.path, path);
                found_handle = resources_loaded_image_add(resources, &loaded);
            }
            else
            {
                if(errors_or_null && errors_or_null->size < max_errors)
                    array_push(errors_or_null, load_error);
            }
            
            loaded_image_deinit(&loaded);
        }

         pairing.to->image = found_handle;
         pairing.to->info = pairing.from->info;
         array_copy(&pairing.to->path, pairing.from->path);
         out_error = ERROR_OR(out_error) load_error;
    }

    return out_error;
}

EXPORT void object_finalize(Resources* resources, Object* object, Shape_Assembly* assembly, Object_Description description, Material_Handle_Array materials)
{
    Shape_Assembly_Handle assembly_handle = resources_shape_assembly_add(resources, assembly);
    object_init(object, resources);

    object->shape_assembly = assembly_handle;
    object->shape = NULL_HANDLE(Shape_Handle);
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

        LOG_INFO("ASSET", "%s", material->name.data);
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

                group.vertices_from = group_desc->vertices_from;
                group.vertices_to = group_desc->vertices_to;

                array_push(&object->groups, group);
            }
        }

        if(unable_to_find_group)
            LOG_ERROR("ASSET", "Failed to find a material called %s while loadeing %s", group_desc->material_name.data, description.path.data);
    }
}

#define FLAT_ERRORS_COUNT 100

EXPORT Error material_read_entire(Resources* resources, Material_Handle_Array* materials, String path, Error_Array* errors_or_null, isize max_errors)
{
    Error error = {0};

    String_Builder full_path = {0};
    Allocator* scratch = allocator_get_scratch();
    array_init_backed(&full_path, scratch, 512);
    file_get_full_path(&full_path, path);
    
    //@TODO: add dirty flag or something similar so this handles reloading.
    //       For now we can only reload if we eject all resources
    bool was_found = false;

    //@TODO: handle phong materials
    //search for an object. If found return its duplicated handle.
    HANDLE_TABLE_FOR_EACH_BEGIN(resources->materials, h, Object*, found_object)
        if(builder_is_equal(full_path, found_object->path))
        {
            was_found = true;
            Material_Handle local_handle = {h};
            Material_Handle duplicate_handle = resources_material_share(resources, local_handle);

            array_push(materials, duplicate_handle);
        }
    HANDLE_TABLE_FOR_EACH_END
    
    if(was_found == false)
    {
        String_Builder file_content = {scratch};
        Allocator_Set allocs_before = allocator_set_default(scratch);
        error = file_read_entire(string_from_builder(full_path), &file_content);
        
        if(error_is_ok(error))
        {
            //@TODO: this a good place for local arena. Managing all this state is a headache.
            Format_Mtl_Material_Array mtl_materials = {0};
            Format_Obj_Mtl_Error mtl_errors[FLAT_ERRORS_COUNT] = {0};
            isize had_mtl_errors = 0;
            format_mtl_read(&mtl_materials, string_from_builder(file_content), mtl_errors, FLAT_ERRORS_COUNT, &had_mtl_errors);

            if(had_mtl_errors > 0)
                error = _format_obj_mtl_error_on_to_error(mtl_errors[0].statement, mtl_errors[0].line);

            if(errors_or_null)
            {
                for(isize i = 0; i < MIN(FLAT_ERRORS_COUNT, had_mtl_errors); i++)
                    array_push(errors_or_null, _format_obj_mtl_error_on_to_error(mtl_errors[i].statement, mtl_errors[i].line));
            }
            
            if(error_is_ok(error))
            {
                for(isize i = 0; i < mtl_materials.size; i++)
                {
                    Material material = {0};

                    material_init(&material, resources);
            
                    //@TODO: handle phong materials
                    Material_Description material_desription = {0};
                    process_mtl_material(&material_desription, NULL, mtl_materials.data[i]);
                    array_copy(&material_desription.path, full_path);

                    material_load_images(resources, &material, material_desription, errors_or_null, max_errors);
                    Material_Handle local_handle = resources_material_add(resources, &material);
                    
                    array_push(materials, local_handle);
                    material_description_deinit(&material_desription);
                }
            }

            for(isize i = 0; i < mtl_materials.size; i++)
                format_obj_material_info_deinit(&mtl_materials.data[i]);

            array_deinit(&mtl_materials);
        }

        
        if(error_is_ok(error) == false)
        {
            if(errors_or_null && errors_or_null->size < max_errors)
                array_push(errors_or_null, error);
        }

        array_deinit(&file_content);
        allocator_set(allocs_before);
    }
    
    array_deinit(&full_path);

    return error;
}

EXPORT Error object_read_entire(Resources* resources, Object_Handle* object, String path, Error_Array* errors_or_null, isize max_errors)
{
    Object_Handle out_handle = {0};
    Error error = {0};
    String_Builder full_path = {0};
    Allocator* scratch = allocator_get_scratch();
    array_init_backed(&full_path, scratch, 512);
    file_get_full_path(&full_path, path);

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
        Allocator_Set allocs_before = allocator_set_default(resources->alloc);
        String_Builder file_content = {scratch};

        error = file_read_entire(string_from_builder(full_path), &file_content);

        if(error_is_ok(error))
        {
            Format_Obj_Model model = {0};
            format_obj_model_init(&model, scratch);

            Format_Obj_Mtl_Error obj_errors[FLAT_ERRORS_COUNT] = {0};
            isize had_obj_errors = 0;
            format_obj_read(&model, string_from_builder(file_content), obj_errors, FLAT_ERRORS_COUNT, &had_obj_errors);

            if(had_obj_errors > 0)
                error = _format_obj_mtl_error_on_to_error(obj_errors[0].statement, obj_errors[0].line);

            if(errors_or_null)
            {
                for(isize i = 0; i < MIN(FLAT_ERRORS_COUNT, had_obj_errors); i++)
                    array_push(errors_or_null, _format_obj_mtl_error_on_to_error(obj_errors[i].statement, obj_errors[i].line));
            }

            if(error_is_ok(error))
            {
                Shape_Assembly out_assembly = {0};
                shape_assembly_init(&out_assembly, resources->alloc);

                String full_path_s = string_from_builder(full_path);

                //@TODO: refactor out. Make handling of paths consistent!
                isize last_dir_index = string_find_last_char(full_path_s, '/') + 1;

                //@TODO: remove builder_from_string and make builder_from_string_alloc the default
                String dir_path = string_head(full_path_s, last_dir_index);

                Object_Description description = {0};
                process_obj_object(&out_assembly, &description, model, errors_or_null, max_errors);
                array_copy(&description.path, full_path);

                Object out_object = {0};
                object_init(&out_object, resources);

                Material_Handle_Array materials = {0};
                for(isize i = 0; i < description.material_files.size; i++)
                {
                    String_Builder mtl_path = builder_from_string_alloc(dir_path, scratch);
                    String file = string_from_builder(description.material_files.data[i]);
                    builder_append(&mtl_path, file);
                    material_read_entire(resources, &materials, string_from_builder(mtl_path), errors_or_null, max_errors);

                    array_deinit(&mtl_path);
                }
            
                object_finalize(resources, &out_object, &out_assembly, description, materials);

                out_handle = resources_object_add(resources, &out_object);
                array_deinit(&materials);
            }

            format_obj_model_deinit(&model);
        }

        array_deinit(&file_content);
        allocator_set(allocs_before);
    }
        
    if(error_is_ok(error) == false)
    {
        if(errors_or_null && errors_or_null->size < max_errors)
            array_push(errors_or_null, error);
    }

    array_deinit(&full_path);

    *object = out_handle;
    return error;
}
