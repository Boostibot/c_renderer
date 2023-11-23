#pragma once

#include "lib/log.h"
#include "camera.h"
#include "gl_utils/gl.h"
#include "gl_utils/gl_shader_util.h"
#include "gl_utils/gl_pixel_format.h"
#include "resource_loading.h"
#include "resources.h"



typedef struct Render_Resource_Info {
    Resource_Ptr resource;
    bool is_loaded;
    String_Builder name;
} Render_Resource_Info;

typedef struct Render_Ptr {
    Id id;
    Render_Resource_Info* ptr;
} Render_Ptr;


DEFINE_ARRAY_TYPE(Render_Ptr, Render_Ptr_Array);

bool render_ref_is_valid(Render_Ptr ref)
{
    return ref.ptr->resource.id == ref.id;
}

typedef enum Shader_Type{
    SHADER_TYPE_SOLID_COLOR,
    SHADER_TYPE_DEPTH_COLOR,
    SHADER_TYPE_BLINN_PHONG,
    SHADER_TYPE_PBR,
    SHADER_TYPE_PBR_MAPPED,
    SHADER_TYPE_SKYBOX,
} Shader_Type;

typedef struct Render_Shader2 {
    Render_Resource_Info info;
    Shader_Type shader_type;
    Render_Shader shader;
} Render_Shader2;

typedef struct Render_Shape
{
    Render_Resource_Info info;

    GLuint vao;
    GLuint vbo;
    GLuint ebo;

    GLuint vertex_count;
    GLuint triangle_count;
} Render_Shape;

typedef struct Render_Map
{
    Render_Resource_Info info;
    
    Map_Info map_info;
    GLuint map;
    isize width;
    isize height;
    
    GL_Pixel_Format pixel_format;

    bool is_clamped;
} Render_Map;

typedef struct Render_Cubemap
{
    Render_Resource_Info info;
    
    Map_Info map_info;
    GLuint map;

    GL_Pixel_Format pixel_format;
    isize widths[6];
    isize heights[6];
} Render_Cubemap;

typedef struct Blinn_Phong_Params
{
    f32 light_ambient_strength;
    f32 light_specular_strength;
    f32 light_specular_sharpness;
    f32 light_linear_attentuation;
    f32 light_quadratic_attentuation;
    f32 light_specular_effect;
    f32 gamma;
} Blinn_Phong_Params;

typedef struct Render_Material
{
    Render_Resource_Info info;

    Material_Info material_info;
    Render_Ptr maps[MAP_TYPE_ENUM_COUNT];
    Render_Ptr cubemaps[CUBEMAP_TYPE_ENUM_COUNT];
} Render_Material;

DEFINE_ARRAY_TYPE(Render_Material, Render_Material_Array);

typedef struct Render_Sub_Mesh {
    Render_Ptr material;
    isize triangles_from;
    isize triangles_to;
} Render_Sub_Mesh;

DEFINE_ARRAY_TYPE(Render_Sub_Mesh, Render_Sub_Mesh_Array);

typedef struct Render_Mesh {
    Render_Sub_Mesh_Array submeshes;
    Render_Ptr shape;
} Render_Mesh;

#define MAX_SINGLE_COLOR_MAPS 20
#define MAX_PBR_UNIFORM_BUFFERS 10
#define MAX_PHONG_UNIFORM_BUFFERS 10


#define PBR_MAX_LIGHTS 4
#define PBR_MAX_SPHERE_LIGHTS 128
#define PBR_MAX_SHADOW_CASTERS 16
#define PBR_MAX_QUAD_LIGHTS 4
#define PBR_MAX_LINE_LIGHTS 4

typedef struct Sphere_Light {
    Vec3 pos;
    Vec3 color;
    f32 radius;
} Sphere_Light;

typedef struct Line_Light {
    Vec3 pos1;
    Vec3 pos2;
    Vec3 color1;
    Vec3 color2;
    f32 radius;
} Line_Light;

typedef struct Quad_Light {
    Vec3 top_left;
    Vec3 bottom_right;
    Vec3 color;
} Quad_Light;

typedef struct PBR_Params {
    Sphere_Light lights[PBR_MAX_LIGHTS];
    Vec3 view_pos;
    f32 gamma;
    f32 attentuation_strength;
    Vec3 ambient_color;
} PBR_Params;

typedef struct Render_Environment {
    Sphere_Light sphere_lights[PBR_MAX_SPHERE_LIGHTS];
    Line_Light   line_lights[PBR_MAX_SPHERE_LIGHTS];
    Quad_Light   quad_lights[PBR_MAX_QUAD_LIGHTS];

    Camera camera;
    f32 attentuation_strength;
    Vec3 ambient_color;
} Render_Environment;

typedef struct Render_Mesh_Environment {
    int xxx;
} Render_Mesh_Environment;

typedef struct Render_Scene_Layer {
    Name name;
} Render_Scene_Layer;

typedef struct Render_Command {
    Render_Environment* environment_or_null;
    Resource_Ptr mesh;
    Resource_Ptr shader;
    Transform transform;
} Render_Command;


DEFINE_ARRAY_TYPE(Render_Command, Render_Command_Array);

typedef struct Render_Command_Buffer {
    Render_Command_Array commands; 
} Render_Command_Buffer;


//global render state
typedef struct Render
{
    Allocator* allocator;
    Allocator* scratch_allocator;

    Render_Command_Buffer commands;
    Render_Command_Buffer last_frame_commands;

    Stable_Array shaders;
    Stable_Array cubemaps;
    Stable_Array maps;
    Stable_Array meshes;
    Stable_Array materials;
    
    Hash_Ptr hash_shaders;
    Hash_Ptr hash_cubemaps;
    Hash_Ptr hash_maps;
    Hash_Ptr hash_meshes;
    Hash_Ptr hash_materials;
} Render;

void render_scene_begin();
void render_scene_end();


#define VEC2_FMT "{%f, %f}"
#define VEC2_PRINT(vec) (vec).x, (vec).y

#define VEC3_FMT "{%f, %f, %f}"
#define VEC3_PRINT(vec) (vec).x, (vec).y, (vec).z

#define VEC4_FMT "{%f, %f, %f, %f}"
#define VEC4_PRINT(vec) (vec).x, (vec).y, (vec).z, (vec).w

Render_Mesh* render_mesh_get(Render* render, Id id)
{
    return NULL;
}

Render_Shader2* render_shader_get(Render* render, Id id)
{
    return NULL;
}

Render_Material* render_material_get(Render* render, Id id)
{
    return NULL;
}

Render_Material* render_material_retrieve(Render* render, Render_Ptr material)
{
    return NULL;
}

Render_Shape* render_shape_retrieve(Render* render, Render_Ptr material)
{
    return NULL;
}


Mat4 mat4_from_transform(Transform transform)
{
    return mat4_scale_affine(mat4_translation(transform.translate), transform.translate);
}

void render_command_buffer(Render* render, Render_Command_Buffer commands)
{
    Render_Environment global_environment = {0};
    for(isize i = 0; i < commands.commands.size; i++)
    {
        Render_Command* command = &commands.commands.data[i];

        Transform transform = command->transform;
        Render_Environment* environment = command->environment_or_null;
        Render_Mesh* render_mesh = render_mesh_get(render, command->mesh.id);
        Render_Shader2* shader = render_shader_get(render, command->shader.id);

        if(environment == NULL)
            environment = &global_environment;

        if(render_mesh == NULL || shader == NULL)
        {
            LOG_ERROR("render", "invalid render command submitted: @TODO");
            continue;
        }

        Mat4 projection = camera_make_projection_matrix(environment->camera);
        Mat4 view = camera_make_view_matrix(environment->camera);
        Mat4 model = mat4_from_transform(transform);
        Vec3 view_pos = environment->camera.pos;
        Vec3 light_pos = environment->sphere_lights->pos;
        Vec3 light_color = environment->sphere_lights->color;

        Render_Shape* shape = render_shape_retrieve(render, render_mesh->shape);
        

        for(isize i = 0; i < render_mesh->submeshes.size; i++)
        {
            Render_Sub_Mesh* submesh = &render_mesh->submeshes.data[i];
            Render_Material* material = render_material_retrieve(render, submesh->material);
            Material_Info* mat_info = &material->material_info;

            Blinn_Phong_Params blinn_phong_params = {0};
            blinn_phong_params.light_ambient_strength = vec3_max_len(mat_info->ambient_color);
            blinn_phong_params.light_linear_attentuation = 0;
            blinn_phong_params.light_quadratic_attentuation = lerpf(0, 1, environment->attentuation_strength);
            blinn_phong_params.light_specular_effect = vec3_max_len(mat_info->specular_color);
            blinn_phong_params.light_specular_strength = mat_info->

            //Render_Map diffuse = {0};

            ASSERT(shader->shader_type == SHADER_TYPE_BLINN_PHONG);
            render_shape_draw_using_blinn_phong(*shape, submesh->triangles_from, submesh->triangles_to, &shader->shader, projection, view, model, view_pos, light_pos, light_color, params, diffuse);
        }
    }
}

void render_shape_draw(Render_Shape mesh)
{
    glBindVertexArray(mesh.vao);
    glDrawElements(GL_TRIANGLES, mesh.triangle_count * 3, GL_UNSIGNED_INT, 0);
    glBindVertexArray(0);
}

void render_shape_draw_range(Render_Shape mesh, isize from, isize to)
{
    CHECK_BOUNDS(from, to + 1);
    isize range = to - from;
    glBindVertexArray(mesh.vao);
    glDrawElements(GL_TRIANGLES, (GLsizei) range * 3, GL_UNSIGNED_INT, (void*)(from * sizeof(Triangle_Index)));
    glBindVertexArray(0);
}

void render_shape_draw_using_solid_color(Render_Shape mesh, Render_Shader* shader_depth_color, Mat4 projection, Mat4 view, Mat4 model, Vec3 color, f32 gamma)
{
    render_shader_use(shader_depth_color);
    render_shader_set_vec3(shader_depth_color, "color", color);
    render_shader_set_mat4(shader_depth_color, "projection", projection);
    render_shader_set_mat4(shader_depth_color, "view", view);
    render_shader_set_mat4(shader_depth_color, "model", model);
    render_shader_set_f32(shader_depth_color, "gamma", gamma);
    render_shape_draw(mesh);
    render_shader_unuse(shader_depth_color);
}

void render_shape_draw_using_depth_color(Render_Shape mesh, Render_Shader* shader_depth_color, Mat4 projection, Mat4 view, Mat4 model, Vec3 color)
{
    render_shader_use(shader_depth_color);
    render_shader_set_vec3(shader_depth_color, "color", color);
    render_shader_set_mat4(shader_depth_color, "projection", projection);
    render_shader_set_mat4(shader_depth_color, "view", view);
    render_shader_set_mat4(shader_depth_color, "model", model);
    render_shape_draw(mesh);
    render_shader_unuse(shader_depth_color);
}

void render_shape_draw_using_uv_debug(Render_Shape mesh, Render_Shader* uv_shader_debug, Mat4 projection, Mat4 view, Mat4 model)
{
    render_shader_use(uv_shader_debug);
    Mat4 inv = mat4_inverse_nonuniform_scale(model);
    Mat3 normal_matrix = mat3_from_mat4(inv);
    normal_matrix = mat3_from_mat4(model);

    render_shader_set_mat4(uv_shader_debug, "projection", projection);
    render_shader_set_mat4(uv_shader_debug, "view", view);
    render_shader_set_mat4(uv_shader_debug, "model", model);
    render_shader_set_mat3(uv_shader_debug, "normal_matrix", normal_matrix);
    render_shape_draw(mesh);
    render_shader_unuse(uv_shader_debug);
}

void render_shape_draw_using_blinn_phong(Render_Shape shape, isize triangles_from, isize triangles_to, Render_Shader* blin_phong_shader, Mat4 projection, Mat4 view, Mat4 model, Vec3 view_pos, Vec3 light_pos, Vec3 light_color, Blinn_Phong_Params params, Render_Map diffuse)
{
    render_shader_use(blin_phong_shader);
    Mat4 inv = mat4_inverse_nonuniform_scale(model);
    Mat3 normal_matrix = mat3_from_mat4(inv);
    normal_matrix = mat3_from_mat4(model);

    render_shader_set_mat4(blin_phong_shader, "projection", projection);
    render_shader_set_mat4(blin_phong_shader, "view", view);
    render_shader_set_mat4(blin_phong_shader, "model", model);
    render_shader_set_mat3(blin_phong_shader, "normal_matrix", normal_matrix);

    render_shader_set_vec3(blin_phong_shader, "view_pos", view_pos);
    render_shader_set_vec3(blin_phong_shader, "light_pos", light_pos);
    render_shader_set_vec3(blin_phong_shader, "light_color", light_color);
    
    render_shader_set_f32(blin_phong_shader, "light_ambient_strength", params.light_ambient_strength);
    render_shader_set_f32(blin_phong_shader, "light_specular_strength", params.light_specular_strength);
    render_shader_set_f32(blin_phong_shader, "light_specular_sharpness", params.light_specular_sharpness);
    render_shader_set_f32(blin_phong_shader, "light_ambient_strength", params.light_ambient_strength);
    render_shader_set_f32(blin_phong_shader, "light_ambient_strength", params.light_ambient_strength);
    render_shader_set_f32(blin_phong_shader, "light_specular_effect", params.light_specular_effect);
    render_shader_set_f32(blin_phong_shader, "gamma", params.gamma);

    render_image_use(&diffuse, 0);
    render_shader_set_i32(blin_phong_shader, "map_diffuse", 0);
    
    render_shape_draw(mesh);
    render_image_unuse(&diffuse);
    render_shader_unuse(blin_phong_shader);
}

void render_shape_draw_using_skybox(Render_Shape mesh, Render_Shader* skybox_shader, Mat4 projection, Mat4 view, Mat4 model, f32 gamma, Render_Cubeimage skybox)
{
    glDepthFunc(GL_LEQUAL);
    render_shader_use(skybox_shader);
    render_shader_set_mat4(skybox_shader, "projection", projection);
    render_shader_set_mat4(skybox_shader, "view", view);
    render_shader_set_mat4(skybox_shader, "model", model);
    render_shader_set_f32(skybox_shader, "gamma", gamma);
    render_shape_draw(mesh);
    
    render_cubeimage_use(&skybox, 0);
    render_shader_set_i32(skybox_shader, "cubemap_diffuse", 0);
    
    render_shape_draw(mesh);
    render_cubeimage_unuse(&skybox);
    render_shader_unuse(skybox_shader);
    glDepthFunc(GL_LESS);
}

void render_pbr_material_deinit(Render_Material* material, Render* render);
void render_pbr_material_init(Render_Material* material, Render* render);


void render_deinit(Render* render)
{
    HANDLE_TABLE_FOR_EACH_BEGIN(render->materials, Handle, h, Render_Material*, render_pbr_material)
        render_pbr_material_deinit(render_pbr_material, render);
    HANDLE_TABLE_FOR_EACH_END

    HANDLE_TABLE_FOR_EACH_BEGIN(render->images, Handle, h, Render_Image*, render_image)
        render_image_deinit(render_image);
    HANDLE_TABLE_FOR_EACH_END
    
    HANDLE_TABLE_FOR_EACH_BEGIN(render->shaders, Handle, h, Render_Shader*, render_shader)
        render_shader_deinit(render_shader);
    HANDLE_TABLE_FOR_EACH_END
    
    HANDLE_TABLE_FOR_EACH_BEGIN(render->meshes, Handle, h, Render_Mesh*, render_mesh)
        render_mesh_deinit(render_mesh);
    HANDLE_TABLE_FOR_EACH_END
    
    handle_table_deinit(&render->materials);
    handle_table_deinit(&render->images);
    handle_table_deinit(&render->cubeimages);
    handle_table_deinit(&render->shaders);
    handle_table_deinit(&render->meshes);

    memset(render, 0, sizeof *render);
}

void render_init(Render* render, Allocator* alloc)
{
    render_deinit(render);

    render->alloc = alloc;
    handle_table_init(&render->images, alloc, sizeof(Render_Image), DEF_ALIGN);
    handle_table_init(&render->cubeimages, alloc, sizeof(Render_Cubeimage), DEF_ALIGN);
    handle_table_init(&render->shaders, alloc, sizeof(Render_Shader), DEF_ALIGN);
    handle_table_init(&render->meshes, alloc, sizeof(Render_Mesh), DEF_ALIGN);
    handle_table_init(&render->materials, alloc, sizeof(Render_Material), DEF_ALIGN);
}

#define DEFINE_RENDERER_RESOURCE(Type, name, member_name)                              \
    Type##_Handle render_##name##_add(Render* render, Type* item)                \
    {                                                                                   \
        Type##_Handle out = {0};                                                        \
        Type* added_ptr = (Type*) handle_table_add(&render->member_name, (Handle*) &out);     \
        SWAP(added_ptr, item, Type);                                                    \
                                                                                        \
        return out;                                                                     \
    }                                                                                   \
                                                                                        \
    Type##_Handle render_##name##_share(Render* render, Type##_Handle handle)     \
    {                                                                                   \
        Type##_Handle out = {0};                                                        \
        handle_table_share(&render->member_name, (Handle*) &handle, (Handle*) &out); \
        return out;                                                                     \
    }                                                                                   \
                                                                                        \
    Type* render_##name##_get(Render* render, Type##_Handle handle)               \
    {                                                                                   \
        Type* out = (Type*) handle_table_get(render->member_name, (Handle*) &handle); \
        return out;                                                                     \
    }                                                                                   \
                                                                                        \
    Type* render_##name##_get_ref(Render* render, Type##_Ref handle)              \
    {                                                                                   \
        return (Type*) handle_table_get(render->member_name, (Handle*) &handle);      \
    }                                                                                   \

DEFINE_RENDERER_RESOURCE(Render_Image, image, images)
DEFINE_RENDERER_RESOURCE(Render_Cubeimage, cubeimage, cubeimages)
DEFINE_RENDERER_RESOURCE(Render_Shader, shader, shaders)
DEFINE_RENDERER_RESOURCE(Render_Mesh, mesh, meshes)
DEFINE_RENDERER_RESOURCE(Render_Material, material, materials)

void render_image_remove(Render* render, Render_Image_Handle handle)      
{                                                                           
    Render_Image* removed = (Render_Image*) handle_table_get_unique(render->images, (Handle*) &handle);
    if(removed)
        render_image_deinit(removed);

    handle_table_remove(&render->images, (Handle*) &handle);
}    

void render_cubeimage_remove(Render* render, Render_Cubeimage_Handle handle)      
{                                                                               
    Render_Cubeimage* removed = (Render_Cubeimage*) handle_table_get_unique(render->cubeimages, (Handle*) &handle);
    if(removed)
        render_cubeimage_deinit(removed);

    handle_table_remove(&render->cubeimages, (Handle*) &handle);
}    

void render_shader_remove(Render* render, Render_Shader_Handle handle)      
{                                                                               
    Render_Shader* removed = (Render_Shader*) handle_table_get_unique(render->shaders, (Handle*) &handle);
    if(removed)
        render_shader_deinit(removed);
    handle_table_remove(&render->shaders, (Handle*) &handle);
}  
void render_mesh_remove(Render* render, Render_Mesh_Handle handle)      
{                                                                               
    Render_Mesh* removed = (Render_Mesh*) handle_table_get_unique(render->meshes, (Handle*) &handle);
    if(removed)
        render_mesh_deinit(removed);
    handle_table_remove(&render->meshes, (Handle*) &handle);
}  

void render_material_remove(Render* render, Render_Material_Handle handle)      
{                                                                               
    Render_Material* removed = (Render_Material*) handle_table_get_unique(render->materials, (Handle*) &handle);
    if(removed)
        render_pbr_material_deinit(removed, render);
    handle_table_remove(&render->materials, (Handle*) &handle);
}  

void render_pbr_material_deinit(Render_Material* material, Render* render)
{
    array_deinit(&material->name);
    array_deinit(&material->path);
    
    for(isize i = 0; i < MAP_TYPE_ENUM_COUNT; i++)
        render_image_remove(render, material->maps[i].image);  

    for(isize i = 0; i < CUBEMAP_TYPE_ENUM_COUNT; i++)
        render_cubeimage_remove(render, material->cubemaps[i].image);  
    
    memset(material, 0, sizeof *material);
}

void render_pbr_material_init(Render_Material* material, Render* render)
{
    render_pbr_material_deinit(material, render);
    array_init(&material->name, render->alloc);
    array_init(&material->path, render->alloc);
}

Render_Map render_map_from_map(Map map, Render* render)
{
    Render_Map out = {0};

    out.info = map.info;
    
    Resource_Info* info = NULL;
    Image_Builder* image = image_get_with_info(map.image, &info);

    if(image)
    {
        String path = string_from_builder(info->path);
        Render_Image_Handle handle = {0};

        //Scan if we have this image already
        HANDLE_TABLE_FOR_EACH_BEGIN(render->images, Render_Image_Handle, h, Render_Image*, found_image)
            if(string_is_equal(string_from_builder(found_image->path), path))
            {
                handle = h;
                break;
            }
        HANDLE_TABLE_FOR_EACH_END

        if(HANDLE_IS_NULL(handle))
        {
            LOG_INFO("RENDER", "Created map "STRING_FMT" (%lli channels)", STRING_PRINT(path), image_builder_channel_count(*image));
            Render_Image render_image = {0};
            render_image_init(&render_image, image_from_builder(*image), STRING("@TEMP"), 0);
            builder_assign(&render_image.path, path);
            handle = render_image_add(render, &render_image);
        }

        out.image = handle;
    }

    return out;
}

Render_Cubemap render_cubemap_from_cubemap(Cubemap cubemap, Render* render)
{
    String_Builder concatenated_paths = {render->alloc};

    bool had_at_least_one_side = false;
    Image images[6] = {0};
    for(isize i = 0; i < 6; i++)
    {
        Map* map = &cubemap.maps.faces[i];
        Resource_Info* info = NULL;
        Image_Builder* image = image_get_with_info(map->image, &info);

        if(image)
        {
            images[i] = image_from_builder(*image);
            had_at_least_one_side = true;
            String path = string_from_builder(info->path);;
            builder_append(&concatenated_paths, path);
        }

        array_push(&concatenated_paths, '\0');
    }
    
    Render_Cubeimage_Handle handle = {0};

    //Scan if we have this cubemap already
    HANDLE_TABLE_FOR_EACH_BEGIN(render->images, Render_Cubeimage_Handle, h, Render_Image*, image_ptr)
        if(builder_is_equal(image_ptr->path, concatenated_paths))
        {
            handle = h;
            break;
        }
    HANDLE_TABLE_FOR_EACH_END

    if(HANDLE_IS_NULL(handle))
    {
        if(had_at_least_one_side)
        {
            Render_Cubeimage render_image = {0};
            //String name = {0};
            //map_get_name(&name, (Map_Ref*) &cubemap.maps.faces[0]);
            //render_cubeimage_init(&render_image, images, name);
            //SWAP(&render_image.path, &concatenated_paths, String_Builder);

            ASSERT_MSG(false, "@NOTE: something seems to be missing here!");

            handle = render_cubeimage_add(render, &render_image);
        }
    }
    
    array_deinit(&concatenated_paths);

    Render_Cubemap out = {0};
    //@NOTE: not setting info!
    out.image = handle;
    return out;
}

void render_map_unuse(Render_Map map)
{
    (void) map;
    glBindTexture(GL_TEXTURE_2D, 0);
}


void render_pbr_material_init_from_material(Render_Material* render_material, Render* render, Id material_ref)
{
    render_pbr_material_init(render_material, render);
    Resource_Info* info = NULL;
    Material* material = material_get_with_info(material_ref, &info);
    if(material)
    {
        String path = string_from_builder(info->path);
        String name = string_from_builder(info->name);

        builder_assign(&render_material->name, name);
        builder_assign(&render_material->path, path);

        render_material->info = material->info;

        for(isize i = 0; i < MAP_TYPE_ENUM_COUNT; i++)
            render_material->maps[i] = render_map_from_map(material->maps[i], render);  

        for(isize i = 0; i < CUBEMAP_TYPE_ENUM_COUNT; i++)
            render_material->cubemaps[i] = render_cubemap_from_cubemap(material->cubemaps[i], render);  
    }
}

//@TODO: carry the concept of leaf groups also to Object!
typedef struct Render_Object_Leaf_Group {
    i32 triangles_from;
    i32 triangles_to;

    Render_Material material;
} Render_Object_Leaf_Group;

DEFINE_ARRAY_TYPE(Render_Object_Leaf_Group, Render_Object_Leaf_Group_Array);

typedef struct Render_Object {
    Render_Mesh_Handle mesh;
    Render_Object_Leaf_Group_Array leaf_groups; 
} Render_Object;

void render_object_deinit(Render_Object* render_object, Render* render)
{
    render_mesh_remove(render, render_object->mesh);

    for(isize i = 0; i < render_object->leaf_groups.size; i++)
        render_pbr_material_deinit(&render_object->leaf_groups.data[i].material, render);

    array_deinit(&render_object->leaf_groups);
}

void render_object_init(Render_Object* render_object, Render* render)
{
    render_object_deinit(render_object, render);
    array_init(&render_object->leaf_groups, render->alloc);
}

void render_object_init_from_object(Render_Object* render_object, Render* render, Id object_ref)
{
    Resource_Info* info = NULL;
    Triangle_Mesh* object = triangle_mesh_get_with_info(object_ref, &info);

    render_object_init(render_object, render);

    if(object)
    {
        String path = string_from_builder(info->path);
        String name = string_from_builder(info->name);

        Shape_Assembly* shape_assembly = shape_get(object->shape);
        ASSERT(shape_assembly);
        if(name.size == 0)
            name = STRING("default");

        if(shape_assembly)
        {
            Shape shape = {0};
            shape.triangles = shape_assembly->triangles;
            shape.vertices = shape_assembly->vertices;

            Render_Mesh out_mesh = {0};
            render_mesh_init_from_shape(&out_mesh, shape, name);
    
            render_object->mesh = render_mesh_add(render, &out_mesh);
        }

        LOG_INFO("RENDER", "Creating render object from obect at "STRING_FMT". (%lli groups)", STRING_PRINT(path), (lli) object->groups.size);
        log_group_push();
        for(isize i = 0; i < object->groups.size; i++)
        {
            Triangle_Mesh_Group* group = &object->groups.data[i];

            ASSERT_MSG(group->child_i1 == 0, "@TEMP: assuming only leaf groups");
            if(group->material_i1)
            {
                Id material_ref = object->materials.data[group->material_i1 - 1];
                Material* material = material_get(material_ref);
                if(material)
                {
                    Render_Object_Leaf_Group out_group = {0};
                    render_pbr_material_init_from_material(&out_group.material, render, material_ref);
                    out_group.triangles_from = group->triangles_from;
                    out_group.triangles_to = group->triangles_to;

                    array_push(&render_object->leaf_groups, out_group);
                }
            }
            
        }
        log_group_pop();
    }
}

typedef struct Triangle_Sub_Range
{
    isize triangles_from;
    isize triangles_to;
} Triangle_Sub_Range;

typedef struct Render_Filled_Map
{
    Map_Info info;
    const Render_Image* image;
} Render_Filled_Map;

typedef struct Render_Filled_Material
{
    Material_Info info;

    Render_Filled_Map albedo;
    Render_Filled_Map metallic;
    Render_Filled_Map roughness;
    Render_Filled_Map ambient_occlusion;
    Render_Filled_Map normal;
} Render_Filled_Material; 

bool render_filled_map_use(Render_Filled_Map map, Render_Shader* shader, const char* name, isize* slot)
{
    if(map.image)
    {
        render_image_use(map.image, *slot);
        render_shader_set_i32(shader, name, (i32) *slot);
    }
    
    *slot += 1;
    return map.image != NULL;
}

//@TODO: remove till 20/10/2023 
bool render_map_use(Render_Map map, Render* render, Render_Shader* shader, const char* name, isize* slot)
{
    Render_Image* image = render_image_get(render, map.image);
    if(image)
    {
        render_image_use(image, *slot);
        render_shader_set_i32(shader, name, (i32) *slot);
    }
    
    *slot += 1;
    return image != NULL;
}

//@TODO: restore esentially to the previous version. Ie do the querying from Render to outside of this func and supply something like Render_Material_Pointers.
// This is necessary so that this whole thing becomes at least a bit disentagled from the handle mess. I want concrete things in my life again :( 
void render_mesh_draw_using_pbr(Render_Mesh mesh, Render_Shader* pbr_shader, Mat4 projection, Mat4 view, Mat4 model, const PBR_Params* params, const Render_Filled_Material* material, 
    const Triangle_Sub_Range* sub_range_or_null)
{
    render_shader_use(pbr_shader);
    Mat4 inv = mat4_inverse_nonuniform_scale(model);
    Mat3 normal_matrix = mat3_from_mat4(inv);
    normal_matrix = mat3_from_mat4(model);

    //shader
    render_shader_set_mat4(pbr_shader, "projection", projection);
    render_shader_set_mat4(pbr_shader, "view", view);
    render_shader_set_mat4(pbr_shader, "model", model);
    render_shader_set_mat3(pbr_shader, "normal_matrix", normal_matrix);

    String_Builder name_str = {0};
    array_init_backed(&name_str, allocator_get_scratch(), 256);
    for(isize i = 0; i < PBR_MAX_LIGHTS; i++)
    {
        format_into(&name_str, "lights_pos[%d]", (i32)i);
        render_shader_set_vec3(pbr_shader, cstring_from_builder(name_str), params->lights[i].pos);

        format_into(&name_str, "lights_color[%d]", (i32)i);
        render_shader_set_vec3(pbr_shader, cstring_from_builder(name_str), params->lights[i].color);
        
        format_into(&name_str, "lights_radius[%d]", (i32)i);
        render_shader_set_f32(pbr_shader, cstring_from_builder(name_str), params->lights[i].radius);
    }
    array_deinit(&name_str);
    
    render_shader_set_vec3(pbr_shader, "view_pos", params->view_pos);
    render_shader_set_f32(pbr_shader, "gamma", params->gamma);
    render_shader_set_f32(pbr_shader, "attentuation_strength", params->attentuation_strength);
    render_shader_set_vec3(pbr_shader, "ambient_color", params->ambient_color);

    //material
    Vec3 reflection_at_zero_incidence = material->info.reflection_at_zero_incidence;
    if(vec3_is_equal(reflection_at_zero_incidence, vec3_of(0)))
        reflection_at_zero_incidence = vec3_of(0.04f);
    render_shader_set_vec3(pbr_shader, "reflection_at_zero_incidence", reflection_at_zero_incidence);

    isize slot = 0;

    bool use_albedo_map = render_filled_map_use(material->albedo, pbr_shader, "map_albedo", &slot);
    bool use_normal_map = render_filled_map_use(material->normal, pbr_shader, "map_normal", &slot);
    bool use_metallic_map = render_filled_map_use(material->metallic, pbr_shader, "map_metallic", &slot);
    bool use_roughness_map = render_filled_map_use(material->roughness, pbr_shader, "map_roughness", &slot);
    bool use_ao_map = render_filled_map_use(material->ambient_occlusion, pbr_shader, "map_ao", &slot);

    render_shader_set_i32(pbr_shader, "use_albedo_map", use_albedo_map);
    render_shader_set_i32(pbr_shader, "use_normal_map", use_normal_map);
    render_shader_set_i32(pbr_shader, "use_metallic_map", use_metallic_map);
    render_shader_set_i32(pbr_shader, "use_roughness_map", use_roughness_map);
    render_shader_set_i32(pbr_shader, "use_ao_map", use_ao_map);
    
    //@TODO: add the solid color images
    render_shader_set_vec3(pbr_shader, "solid_albedo", material->info.albedo);
    render_shader_set_f32(pbr_shader, "solid_metallic", material->info.metallic);
    render_shader_set_f32(pbr_shader, "solid_roughness", material->info.roughness);
    render_shader_set_f32(pbr_shader, "solid_ao", material->info.ambient_occlusion);
    
    if(sub_range_or_null)
        render_mesh_draw_range(mesh, sub_range_or_null->triangles_from, sub_range_or_null->triangles_to);
    else
        render_mesh_draw(mesh);
    //if(use_maps)
    //{
    //    render_image_unuse(&material->map_albedo);
    //    render_image_unuse(&material->map_normal);
    //    render_image_unuse(&material->map_metallic);
    //    render_image_unuse(&material->map_roughness);
    //    render_image_unuse(&material->map_ao);
    //}


    render_shader_unuse(pbr_shader);
}


Render_Filled_Map render_filled_map_from_map(Render* render, Render_Map map)
{
    Render_Filled_Map filled_map = {0};
    filled_map.image = render_image_get(render, map.image);
    filled_map.info = map.info;

    return filled_map;
}

void render_object(const Render_Object* render_object, Render* render, Render_Shader* pbr_shader, Mat4 projection, Mat4 view, Mat4 model, const PBR_Params* params)
{
    Render_Mesh* mesh = render_mesh_get(render, render_object->mesh);
    if(!mesh)
    {
        LOG_ERROR("RENDER", "render object does not have mesh! " SOURCE_INFO_FMT, SOURCE_INFO_PRINT(SOURCE_INFO()));
        return;
    }
    Render_Filled_Material filled_material = {0};
    Triangle_Sub_Range sub_range = {0};
    for(isize i = 0; i < render_object->leaf_groups.size; i++)
    {
        Render_Object_Leaf_Group* group = &render_object->leaf_groups.data[i];
        sub_range.triangles_from = group->triangles_from;
        sub_range.triangles_to = group->triangles_to;
        filled_material.info = group->material.info;
        
        filled_material.albedo = render_filled_map_from_map(render, group->material.maps[MAP_TYPE_ALBEDO]);
        filled_material.normal = render_filled_map_from_map(render, group->material.maps[MAP_TYPE_NORMAL]);
        filled_material.metallic = render_filled_map_from_map(render, group->material.maps[MAP_TYPE_METALLIC]);
        filled_material.roughness = render_filled_map_from_map(render, group->material.maps[MAP_TYPE_ROUGNESS]);
        filled_material.ambient_occlusion = render_filled_map_from_map(render, group->material.maps[MAP_TYPE_AMBIENT_OCCLUSION]);

        render_mesh_draw_using_pbr(*mesh, pbr_shader, projection, view, model, params, &filled_material, &sub_range);
    }
}

#if 0
void render_mesh_draw_using_pbr_mapped(Render_Mesh mesh, Render_Shader* pbr_shader, Mat4 projection, Mat4 view, Mat4 model, const PBR_Params* params, const Render_Material* material, 
    const Render_Cubeimage* environment, const Render_Cubeimage* irradience, const Render_Cubeimage* prefilter, const Render_Image* brdf_lut)
{
    render_shader_use(pbr_shader);
    Mat4 inv = mat4_inverse_nonuniform_scale(model);
    Mat3 normal_matrix = mat3_from_mat4(inv);
    normal_matrix = mat3_from_mat4(model);

    (void) environment;
    (void) prefilter;
    (void) brdf_lut;

    //shader
    render_shader_set_mat4(pbr_shader, "projection", projection);
    render_shader_set_mat4(pbr_shader, "view", view);
    render_shader_set_mat4(pbr_shader, "model", model);
    render_shader_set_mat3(pbr_shader, "normal_matrix", normal_matrix);

    String_Builder name_str = {0};
    array_init_backed(&name_str, allocator_get_scratch(), 256);
    for(isize i = 0; i < PBR_MAX_LIGHTS; i++)
    {
        format_into(&name_str, "lights_pos[%d]", (i32)i);
        render_shader_set_vec3(pbr_shader, cstring_from_builder(name_str), params->lights[i].pos);

        format_into(&name_str, "lights_color[%d]", (i32)i);
        render_shader_set_vec3(pbr_shader, cstring_from_builder(name_str), params->lights[i].color);
        
        format_into(&name_str, "lights_radius[%d]", (i32)i);
        render_shader_set_f32(pbr_shader, cstring_from_builder(name_str), params->lights[i].radius);
    }
    array_deinit(&name_str);
    
    render_shader_set_vec3(pbr_shader, "view_pos", params->view_pos);
    render_shader_set_f32(pbr_shader, "gamma", params->gamma);
    render_shader_set_f32(pbr_shader, "attentuation_strength", params->attentuation_strength);

    //material
    Vec3 reflection_at_zero_incidence = material->reflection_at_zero_incidence;
    if(vec3_is_equal(reflection_at_zero_incidence, vec3_of(0)))
        reflection_at_zero_incidence = vec3_of(0.04f);
    render_shader_set_vec3(pbr_shader, "reflection_at_zero_incidence", reflection_at_zero_incidence);
     
    i32 slot = 0;
    render_cubeimage_use(irradience, slot);
    render_shader_set_i32(pbr_shader, "cubemap_irradiance", slot++);  
    
    render_cubeimage_use(prefilter, slot);
    render_shader_set_i32(pbr_shader, "cubemap_prefilter", slot++);  
    
    render_image_use(brdf_lut, slot);
    render_shader_set_i32(pbr_shader, "map_brdf_lut", slot++);  

    //Temp
    bool use_maps = material->map_albedo.map 
        || material->map_normal.map
        || material->map_metallic.map
        || material->map_roughness.map
        || material->map_ao.map;
    render_shader_set_i32(pbr_shader, "use_maps", use_maps);

    if(use_maps)
    {
        render_image_use(&material->map_albedo, slot);
        render_shader_set_i32(pbr_shader, "map_albedo", slot++);    
        
        render_image_use(&material->map_normal, slot);
        render_shader_set_i32(pbr_shader, "map_normal", slot++);   
        
        render_image_use(&material->map_metallic, slot);
        render_shader_set_i32(pbr_shader, "map_metallic", slot++);   
        
        render_image_use(&material->map_roughness, slot);
        render_shader_set_i32(pbr_shader, "map_roughness", slot++);   
        
        render_image_use(&material->map_ao, slot);
        render_shader_set_i32(pbr_shader, "map_ao", slot++);   
    }
    else
    {
        render_shader_set_vec3(pbr_shader, "solid_albedo", material->solid_albedo);
        render_shader_set_f32(pbr_shader, "solid_metallic", material->solid_metallic);
        render_shader_set_f32(pbr_shader, "solid_roughness", material->solid_roughness);
        render_shader_set_f32(pbr_shader, "solid_ao", material->solid_ao);
    }

    render_mesh_draw(mesh);

    if(use_maps)
    {
        render_image_unuse(&material->map_albedo);
        render_image_unuse(&material->map_normal);
        render_image_unuse(&material->map_metallic);
        render_image_unuse(&material->map_roughness);
        render_image_unuse(&material->map_ao);
    }

    render_cubeimage_unuse(irradience);
    render_cubeimage_unuse(prefilter);
    render_image_unuse(brdf_lut);
    render_shader_unuse(pbr_shader);
}
#endif 

void render_mesh_draw_using_postprocess(Render_Mesh screen_quad, Render_Shader* shader_screen, GLuint screen_map, f32 gamma, f32 exposure)
{
    render_shader_use(shader_screen);
    render_shader_set_f32(shader_screen, "gamma", gamma);
    render_shader_set_f32(shader_screen, "exposure", exposure);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, screen_map);
    render_shader_set_i32(shader_screen, "screen", 0);
            
    render_mesh_draw(screen_quad);
    glBindTexture(GL_TEXTURE_2D, 0);
    render_shader_unuse(shader_screen);
}
