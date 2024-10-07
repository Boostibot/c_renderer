#pragma once
#include "asset.h"
#include "asset_types.h"
#include "asset_descriptions.h"
#include "format_obj.h"
#include "lib/file.h"
#include "lib/profile.h"
#include "image_loader.h"
#include "lib/allocator_malloc.h"

INTERNAL const char* _format_obj_mtl_translate_error(u32 code, void* context)
{
    (void) context;
    enum {LOCAL_BUFF_SIZE = 256, MAX_ERRORS = 4};
    static int error_index = 0;
    static char error_strings[MAX_ERRORS][LOCAL_BUFF_SIZE + 1] = {0};

    int line = code >> 8;
    int error_flag = code & 0xFF;

    const char* error_flag_str = format_obj_mtl_error_statement_to_string((Format_Obj_Mtl_Error_Statement) error_flag);

    char* out = error_strings[error_index];
    memset(out, 0, LOCAL_BUFF_SIZE);
    snprintf(out, LOCAL_BUFF_SIZE, "%s line: %d", error_flag_str, line);
    error_index += 1;
    return out;
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

INTERNAL const char* _resource_loading_translate_error(u32 code, void* context)
{
    (void) context;
    switch(code)
    {
        case RESOURCE_LOADING_ERROR_NONE: return "RESOURCE_LOADING_ERROR_NONE";
        case RESOURCE_LOADING_ERROR_UV_INDEX: return "RESOURCE_LOADING_ERROR_UV_INDEX";
        case RESOURCE_LOADING_ERROR_POS_INDEX: return "RESOURCE_LOADING_ERROR_POS_INDEX";
        case RESOURCE_LOADING_ERROR_NORM_INDEX: return "RESOURCE_LOADING_ERROR_NORM_INDEX";
        case RESOURCE_LOADING_ERROR_MATERIAL_NOT_FOUND: return "RESOURCE_LOADING_ERROR_MATERIAL_NOT_FOUND";
        
        case RESOURCE_LOADING_ERROR_OTHER: 
        default: return "RESOURCE_LOADING_ERROR_OTHER";
    }
}

EXTERNAL void process_obj_triangle_mesh(Shape_Assembly* shape_assembly, Triangle_Mesh_Group_Description_Array* descriptions, Format_Obj_Model model)
{
    hash_reserve(&shape_assembly->vertices_hash, model.indices.len);
    
    //Try to guess the final needed size
    array_reserve(&shape_assembly->triangles, shape_assembly->triangles.len + model.indices.len/3); //divided by three since are triangles
    array_reserve(&shape_assembly->vertices, shape_assembly->vertices.len + model.indices.len/2); //divide by two since some vertices are shared

    //Deduplicate groups
    for(isize parent_group_i = 0; parent_group_i < model.groups.len; parent_group_i++)
    {
        Format_Obj_Group* parent_group = &model.groups.data[parent_group_i];
        i32 triangles_before = (i32) shape_assembly->triangles.len;
        
        //Skip empty groups
        if(parent_group->trinagles_count <= 0)
            continue;

        //Skip groups that have no group
        if(parent_group->groups.len <= 0)
            continue;

        //Skip groups that we already merged through earlier groups
        bool already_merged = false;
        for(isize i = 0; i < descriptions->len; i++)
            if(string_is_equal(descriptions->data[i].name, parent_group->groups.data[0].string))
            {
                already_merged = true;
                break;
            }

        if(already_merged)
            continue;

        //Iterate over all not yet seen obj groups (start from parent_group_i) and add the vertices if it 
        //is the same group. This merges all groups with the same name and same object into one engine group
        for(isize child_group_i = parent_group_i; child_group_i < model.groups.len; child_group_i++)
        {
            Format_Obj_Group* child_group = &model.groups.data[child_group_i];

            //Skip groups that are empty 
            if(child_group->trinagles_count <= 0)
                continue;

            //... and those in different objects
            if(!builder_is_equal(child_group->object, parent_group->object))
                continue;

            //Shouldnt be possible with the current parser but just in case
            if(child_group->groups.len == 0 || parent_group->groups.len == 0)
                continue;
            
            //... and those that are not the same main group.
            //(We ignore all other groups than the first for now)
            if(!builder_is_equal(child_group->groups.data[0], parent_group->groups.data[0]))
                continue;

            isize i_min = (child_group->trinagles_from)*3;
            isize i_max = (child_group->trinagles_from + child_group->trinagles_count)*3;

            //Iterate all indices using a hash map to deduplicate vertex data
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
                Format_Obj_Vertex_Index index = model.indices.data[i];
                if(index.norm_i1 < 0 || index.norm_i1 > model.normals.len)
                {
                    error = INVALID_NORM_INDEX;
                    error_index = index.norm_i1;
                }
                else
                    composed_vertex.norm = model.normals.data[index.norm_i1 - 1];

                if(index.pos_i1 < 0 || index.pos_i1 > model.positions.len)
                {
                    error = INVALID_POS_INDEX;
                    error_index = index.pos_i1;
                }
                else
                    composed_vertex.pos = model.positions.data[index.pos_i1 - 1];

                if(index.uv_i1 < 0 || index.uv_i1 > model.uvs.len)
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

                    LOG_ERROR("ASSET", "bool processing obj file: %s with index %lli on index number %lli ", error_string, (lli) error_index, (lli) i);
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
        i32 triangles_after = (i32) shape_assembly->triangles.len;

        //No child or next
        group_desc.next_i1 = 0;
        group_desc.child_i1 = 0;

        //Flat for now
        group_desc.depth = 0;

        //assign its range
        group_desc.triangles_from = triangles_before;
        group_desc.triangles_to = triangles_after;

        group_desc.material_name = parent_group->material.string;
        if(parent_group->groups.len > 0)
            group_desc.name = parent_group->groups.data[0].string;

        //link the previous group to this one
        if(descriptions->len > 0)
            array_last(*descriptions)->next_i1 = (i32) descriptions->len + 1;

        array_push(descriptions, group_desc);
    }
}

INTERNAL void process_mtl_map(Map_Description* description, Format_Mtl_Map map, f32 expected_gamma, i8 channels)
{
    description->info.brigthness = map.modify_brigthness;
    description->info.contrast = map.modify_contrast;
    description->info.gamma = expected_gamma;
    description->info.channels_count = channels;
    description->info.is_enabled = map.path.len > 0;

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

    description->path = map.path.string;
}

#define GAMMA_SRGB 2.2f
#define GAMMA_LINEAR 1

EXTERNAL void process_mtl_material(Material_Description* description , Format_Mtl_Material material)
{
    description->name = material.name.string;
        
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

EXTERNAL bool material_link_images(Material_Asset* material, Asset_Handle_Array* images_to_load, Material_Description description)
{
    material->info = description.info;

    typedef struct Map_Or_Cubemap_Handles {
        Asset_Handle handle;
        Map_Description* description;
        u32 asset_type;
        i32 map_or_cubemap_i; 
        i32 face_i;
        i32 _;
    } Map_Or_Cubemap_Handles;

    Map_Or_Cubemap_Handles maps_or_cubemaps[MAP_TYPE_ENUM_COUNT + CUBEMAP_TYPE_ENUM_COUNT*6] = {0};

    //Add all images to be loaded
    for(i32 i = 0; i < MAP_TYPE_ENUM_COUNT; i++)
    {
        maps_or_cubemaps[i].description = &description.maps[i];
        maps_or_cubemaps[i].map_or_cubemap_i = i;
        maps_or_cubemaps[i].asset_type = ASSET_TYPE_IMAGE;
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
            maps_or_cubemaps[index].asset_type = ASSET_TYPE_CUBEMAP;
        }
    }

    Path dir_path = path_strip_to_containing_directory(path_parse(description.path));

    //Load everything
    for(isize i = 0; i < ARRAY_LEN(maps_or_cubemaps); i++)
    {
        SCRATCH_ARENA(arena)
        {
            Map_Or_Cubemap_Handles map_or_cubemap = maps_or_cubemaps[i];

            //if this image is not enabled or does not have a path dont even try
            if(map_or_cubemap.description->info.is_enabled == false || map_or_cubemap.description->path.len <= 0)
                continue;
        
            Path item_path = path_parse(map_or_cubemap.description->path);
            Path full_item_path = path_make_absolute(arena.alloc, item_path, dir_path).path;
        
            Hash_String path = hash_string_make(full_item_path.string);

            Image_Asset_Handle asset = image_asset_find(path, HSTRING());
            if(asset)
                LOG_INFO("ASSET", "load image found image \"%s\" in repository", full_item_path.data);
            else
            {   
                LOG_INFO("ASSET", "load image created \"%s\" in repository", full_item_path.data);
                Asset* created = asset_get(asset_create(map_or_cubemap.asset_type, path, HSTRING()));
                array_push(images_to_load, created->handle);
            }

            if(map_or_cubemap.asset_type == ASSET_TYPE_CUBEMAP)
                material->cubemaps[map_or_cubemap.map_or_cubemap_i][map_or_cubemap.face_i] = asset;
            else
                material->maps[map_or_cubemap.map_or_cubemap_i] = asset;
        }
    }

    (void) material;
    (void) description;

    return true;
}


#include "job.h"

typedef void (*Asset_Finish_Func)(void* context1, void* context2);

void asset_loading_finish_submit(Atomic_Transfer_Block** channel, void* func, void* context1, isize context_size1, void* context2, isize context_size2)
{
    if(platform_thread_is_main())
        ((Asset_Finish_Func) func)(context1, context2);
    else
    {
        void* ptrs[] = {func, context1, context2};
        isize sizes[] = {sizeof(func), context_size1, context_size2};
        atomic_transfer_write_parts(channel, ptrs, sizes, ARRAY_LEN(sizes));
    }
}

void asset_loading_finish_execute_all(Atomic_Transfer_Block** channel)
{
    Atomic_Transfer_Block* blocks = atomic_transfer_read(channel);
    for(Atomic_Transfer_Block* curr = blocks; curr != NULL; )
    {
        Asset_Finish_Func func = *(Asset_Finish_Func*) atomic_transfer_block_part(curr, 0);
        void* context1 = atomic_transfer_block_part(curr, 1);
        void* context2 = atomic_transfer_block_part(curr, 2);
        func(context1, context2);
        
        Atomic_Transfer_Block* prev = curr;
        curr = curr->next;
        atomic_transfer_block_free(prev);
    }
}

bool asset_job_launch(Platform_Thread_Func func, void* context, isize context_size)
{
    Platform_Thread thread = {0};
    Platform_Error error = platform_thread_launch(&thread, 0, func, context, context_size);
    if(error)
    {
        SCRATCH_ARENA(arena)
            LOG_ERROR("ASSET", "%s: Failed launching a thread with error '%s'", __func__, translate_error(arena.alloc, error).data);
    }
    else
    {
        platform_thread_detach(&thread);
    }

    return error == 0;
}

Asset_Handle_Array* global_material_load_queue()
{
    static Asset_Handle_Array array = {0};
    if(array.allocator == NULL)
        array.allocator = allocator_get_malloc();

    return &array;
}

void _material_read_entire_sub_finish(Material_Asset** parent_asset_ptr, Material_Description* description)
{
    Material_Asset* parent_asset = *parent_asset_ptr;
    Material_Asset* created = material_asset_create(parent_asset->asset.path, hash_string_make(description->name));
    asset_set_stage(created->uhandle, ASSET_STAGE_LOADING);

    array_push(&parent_asset->children, created->uhandle);
    bool state = material_link_images(created, global_material_load_queue(), *description);
    asset_set_stage(created->uhandle, state ? ASSET_STAGE_LOADED : ASSET_STAGE_FAILED);
}

void _material_read_entire_finish(Material_Asset** parent_asset_ptr, bool* state)
{
    Material_Asset* parent_asset = *parent_asset_ptr;
    asset_set_stage(parent_asset->uhandle, *state ? ASSET_STAGE_LOADED : ASSET_STAGE_FAILED);
}

typedef struct _Material_Read_Entire_Context {
    Material_Asset* out_material;
    Atomic_Transfer_Block** channel;
} _Material_Read_Entire_Context;

EXTERNAL int _material_read_entire_thread(void* context)
{
    Material_Asset* out_material = ((_Material_Read_Entire_Context*) context)->out_material;
    Atomic_Transfer_Block** channel = ((_Material_Read_Entire_Context*) context)->channel;

    bool state = true;
    SCRATCH_ARENA(arena)
    {
        LOG_INFO("ASSET", "Loading materials at '%.*s'", STRING_PRINT(out_material->asset.path));
        Hash_String path = out_material->asset.path;

        String_Builder file_content = {arena.alloc};
        state = file_read_entire(path.string, &file_content, log_error("ASSET"));
        if(state == false)
            LOG_ERROR("ASSET", "Error loading material file '%s'", path.data);
        else
        {   
            Array(Format_Obj_Mtl_Error) mtl_errors = {arena.alloc};
            array_resize(&mtl_errors, 100);

            isize had_mtl_errors = 0;
            Format_Mtl_Material_Array mtl_materials = {arena.alloc};
            format_mtl_read(&mtl_materials, file_content.string, mtl_errors.data, mtl_errors.len, &had_mtl_errors);

            for(isize i = 0; i < had_mtl_errors; i++)
                LOG_ERROR("ASSET", "bool parsing material file %s: " OBJ_MTL_ERROR_FMT, path.data, OBJ_MTL_ERROR_PRINT(mtl_errors.data[i]));
        
            for(isize i = 0; i < mtl_materials.len; i++)
            {
                //@TODO: danger!!! Material_Description has allocated strings which live inside file_contents! 
                // Thes will get freed on scope exit! We need to allocate those outside!
                Material_Description material_desription = {0};
                process_mtl_material(&material_desription, mtl_materials.data[i]);

                asset_loading_finish_submit(channel, (void*) _material_read_entire_sub_finish, &out_material, sizeof(out_material), &material_desription, sizeof(material_desription));
            }
        }
    }
    
    asset_loading_finish_submit(channel, (void*) _material_read_entire_finish, &out_material, sizeof(out_material), &state, sizeof(state));
    return !state;
}

EXTERNAL bool material_read_entire(Material_Asset_Handle* out_material, Path base_path, Path path, Atomic_Transfer_Block** channel)
{
    bool state = true;
    SCRATCH_ARENA(arena)
    {
        LOG_INFO("ASSET", "Loading materials at '%.*s'", STRING_PRINT(path));
    
        String_Builder full_path = path_make_absolute(arena.alloc, base_path, path).builder;
        Hash_String full_path_hashed = hash_string_make(full_path.string);

        asset_find_all_with_path(arena.alloc, ASSET_TYPE_MATERIAL, full_path_hashed);

        Material_Asset_Handle found_material = material_asset_find(full_path_hashed, HSTRING());
        //@TODO: should be multiple materials sharing the same path!!!

        if(found_material == NULL)
            *out_material = found_material;
        else
        {
            Material_Asset* parent = material_asset_create(full_path_hashed, HSTRING());
            asset_set_stage(parent->uhandle, ASSET_STAGE_LOADING);

            _Material_Read_Entire_Context context = {parent, channel};
            asset_job_launch(_material_read_entire_thread, &context, sizeof context);
        }
    }
    return state;
}

#if 0
EXTERNAL bool material_read_entire(Material_Asset_Handle* out_material, Asset_Handle_Array* materials_to_load, Path base_path, Path path)
{
    bool state = true;
    SCRATCH_ARENA(arena)
    {
        LOG_INFO("ASSET", "Loading materials at '%.*s'", STRING_PRINT(path));
    
        String_Builder full_path = path_make_absolute(arena.alloc, base_path, path).builder;
        Hash_String full_path_hashed = hash_string_make(full_path.string);

        asset_find_all_with_path(arena.alloc, ASSET_TYPE_MATERIAL, full_path_hashed);

        Material_Asset_Handle found_material = material_asset_find(full_path_hashed, HSTRING());
        //@TODO: should be multiple materials sharing the same path!!!

        if(found_material == NULL)
            *out_material = found_material;
        else
        {
            String_Builder file_content = {arena.alloc};
            state = file_read_entire(full_path.string, &file_content, log_error("ASSET"));
            if(state == false)
                LOG_ERROR("ASSET", "Error loading material file '%s'", full_path.data);
            else
            {   
                Array(Format_Obj_Mtl_Error) mtl_errors = {arena.alloc};
                array_resize(&mtl_errors, 100);

                isize had_mtl_errors = 0;
                Format_Mtl_Material_Array mtl_materials = {arena.alloc};
                format_mtl_read(&mtl_materials, file_content.string, mtl_errors.data, mtl_errors.len, &had_mtl_errors);

                for(isize i = 0; i < had_mtl_errors; i++)
                    LOG_ERROR("ASSET", "bool parsing material file %s: " OBJ_MTL_ERROR_FMT, full_path.data, OBJ_MTL_ERROR_PRINT(mtl_errors.data[i]));
        
                for(isize i = 0; i < mtl_materials.len; i++)
                {
                    Material_Description material_desription = {0};
                    process_mtl_material(&material_desription, mtl_materials.data[i]);

                    Asset* material = asset_create(ASSET_TYPE_MATERIAL, full_path_hashed, hash_string_make(material_desription.name));
                    material_link_images((Material_Asset*) (void*) material, material_desription);
                    *out_material = material->handle;
                }
            }
        }
    }
    return state;

    (void) out_material;
    (void) base_path;
    (void) path;
    return true;
}
#endif

EXTERNAL bool model_read_entire(Model_Asset_Handle* out_handle, Asset_Handle_Array* children_to_load, Path base_path, Path path)
{
    #if 0
    bool state = true;
    SCRATCH_ARENA(arena)
    {
        LOG_INFO("ASSET", "Loading mesh at '%.*s'", STRING_PRINT(path));
        Path full_path = path_make_absolute(arena.alloc, base_path, path).path;

        Model_Asset_Handle found_mesh = model_asset_find(hash_string_make(full_path.string), HSTRING());
        if(found_mesh != NULL)
            *out_handle = found_mesh;
        else
        {
            Geometry_Asset* geometry = geometry_asset_create(hash_string_make(full_path.string), HSTRING());
            Model_Asset* parent_model = model_asset_create(hash_string_make(full_path.string), HSTRING());


            *out_handle = parent_model;

            String_Builder file_content = {arena.alloc};
            state = file_read_entire(full_path.string, &file_content, log_error("ASSET"));
            if(state == false)
                LOG_ERROR("ASSET", "Failed to load triangle_mesh file '%s'", full_path.data);
            else
            {
                Format_Obj_Model obj_model = {0};
                format_obj_model_init(&obj_model, arena.alloc);

                Array(Format_Obj_Mtl_Error) obj_errors = {arena.alloc};
                array_resize(&obj_errors, 100);

                isize had_obj_errors = 0;
                format_obj_read(&obj_model, file_content.string, obj_errors.data, obj_errors.len, &had_obj_errors);
                for(isize i = 0; i < had_obj_errors; i++)
                    LOG_ERROR("ASSET", "bool parsing obj file %s: " OBJ_MTL_ERROR_FMT, full_path.data, OBJ_MTL_ERROR_PRINT(obj_errors.data[i]));

                Triangle_Mesh_Group_Description_Array groups = {arena.alloc};
                Asset_Handle_Array material_handles = {arena.alloc};
                process_obj_triangle_mesh(&geometry->shape, &groups, obj_model);
                
                Path parent_dir_path = path_strip_to_containing_directory(full_path);
                for(isize i = 0; i < groups.len; i++)
                {
                    SCRATCH_ARENA(small_arena)
                    {
                        Triangle_Mesh_Group_Description* group_desc = &groups.data[i];
                        Path material_full_path = path_make_absolute(small_arena.alloc, parent_dir_path, path_parse(group_desc->material_path)).path;

                        Hash_String mat_name = hash_string_make(group_desc->material_name);
                        Hash_String mat_path = hash_string_make(material_full_path.string);
                        Material_Asset_Handle material = material_asset_find(mat_name, mat_path);
                        if(material == NULL)
                        {
                            material = material_asset_create(mat_name, mat_path);
                            array_push(children_to_load, material);
                        }

                        Hash_String child_name = hash_string_make(group_desc->name);
                        Hash_String child_path = hash_string_make(full_path.string);
                        Model_Asset* child_model = model_asset_get(model_asset_create(child_path, child_name));
                        child_model->material = material;
                        child_model->geometry = geometry;
                        child_model->triangles_from = group_desc->triangles_from;
                        child_model->triangles_to = group_desc->triangles_to;

                        array_push(&parent_model->children, child_model->asset.handle);
                    }
                }

                parent_model->geometry = geometry->handle;
            }
        }
    
        if(state == false)
            LOG_ERROR("ASSET", "Error rading mesh file '%s'", full_path.data);
    
    }
    return state;
    #endif

    (void) out_handle, children_to_load, base_path, path;
    return true;
}
