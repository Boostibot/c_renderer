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

    SHADER_TYPE_ENUM_COUNT,
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

typedef struct Blinn_Phong_Params {
    Mat4 projection; 
    Mat4 view; 
    Mat4 model; 
    Mat3 normal_matrix;
    Vec3 view_pos; 
    Vec3 light_pos; 
    Vec3 light_color; 
    Vec3 ambient_color; 
    Vec3 specular_color;

    f32 specular_exponent;
    f32 light_linear_attentuation;
    f32 light_quadratic_attentuation;
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
    Resource_Ptr material;
    i32 trinagles_from;
    i32 triangles_to;
    Shader_Type shader;
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

    Render_Ptr selected_shaders[SHADER_TYPE_ENUM_COUNT];

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


Render_Mesh* render_mesh_query(Render* render, Id id)
{
    return NULL;
}

Render_Shader2* render_shader_query(Render* render, Id id)
{
    return NULL;
}

Render_Material* render_material_query(Render* render, Id id)
{
    return NULL;
}

Render_Material* render_material_get(Render* render, Render_Ptr material)
{
    return NULL;
}

Render_Shape* render_shape_get(Render* render, Render_Ptr material)
{
    return NULL;
}

Render_Shader2* render_shaper_get(Render* render, Render_Ptr material)
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
    Render_Shader2* selected_shaders[SHADER_TYPE_ENUM_COUNT] = {0};

    for(isize i = 0; i < SHADER_TYPE_ENUM_COUNT; i++)
        selected_shaders[i] = render_shaper_get(render, render->selected_shaders[i]);

    for(isize i = 0; i < commands.commands.size; i++)
    {
        Render_Command* command = &commands.commands.data[i];

        Transform transform = command->transform;
        Render_Environment* environment = command->environment_or_null;
        Render_Mesh* render_mesh = render_mesh_query(render, command->mesh.id);

        if(environment == NULL)
            environment = &global_environment;

        if(render_mesh == NULL)
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
        Mat3 normal_matrix = mat3_from_mat4(mat4_identity());

        if(transform.scale.x != transform.scale.y || transform.scale.x != transform.scale.z)
        {
            Mat4 inv = mat4_inverse_nonuniform_scale(model);
            normal_matrix = mat3_from_mat4(inv);
        }

        Render_Shape* shape = render_shape_get(render, render_mesh->shape);

        for(isize i = 0; i < render_mesh->submeshes.size; i++)
        {
            Render_Sub_Mesh* submesh = &render_mesh->submeshes.data[i];
            Render_Material* material = render_material_get(render, submesh->material);
            Material_Info* mat_info = &material->material_info;

            if(command->shader == SHADER_TYPE_BLINN_PHONG)
            {
                Blinn_Phong_Params blinn_phong_params = {0};
                blinn_phong_params.ambient_color = mat_info->ambient_color;
                blinn_phong_params.specular_color = mat_info->specular_color;
                blinn_phong_params.specular_exponent = mat_info->specular_exponent;

                blinn_phong_params.projection = projection;
                blinn_phong_params.view = view;
                blinn_phong_params.model = model;
                blinn_phong_params.normal_matrix = normal_matrix;

                blinn_phong_params.view_pos = view_pos;
                blinn_phong_params.light_pos = light_pos;

                blinn_phong_params.gamma = 1;
                blinn_phong_params.light_linear_attentuation = 0;
                blinn_phong_params.light_quadratic_attentuation = lerpf(0, 1, environment->attentuation_strength);

                Render_Map diffuse = {0};
                render_shape_draw_using_blinn_phong(*shape, submesh->triangles_from, submesh->triangles_to, &selected_shaders[SHADER_TYPE_BLINN_PHONG]->shader, blinn_phong_params, diffuse);
            }
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

bool render_map_use(Render_Shader* shader, const char* name, Render_Map map, isize slot)
{
    glActiveTexture(GL_TEXTURE0 + (GLenum) slot);
    glBindTexture(GL_TEXTURE_2D, map.map);
    render_shader_set_i32(shader, name, (i32) slot);
}


void render_cubeimage_use(Render_Shader* shader, const char* name, const Render_Cubemap* map, isize slot)
{
    glActiveTexture(GL_TEXTURE0 + (GLenum) slot);
    glBindTexture(GL_TEXTURE_CUBE_MAP, map->map);
    render_shader_set_i32(shader, name, (i32) slot);
}

void render_shape_draw_using_solid_color(Render_Shape mesh, Render_Shader* shader_depth_color, Mat4 projection, Mat4 view, Mat4 model, Vec3 color, f32 gamma)
{
    render_shader_set_vec3(shader_depth_color, "color", color);
    render_shader_set_mat4(shader_depth_color, "projection", projection);
    render_shader_set_mat4(shader_depth_color, "view", view);
    render_shader_set_mat4(shader_depth_color, "model", model);
    render_shader_set_f32(shader_depth_color, "gamma", gamma);
    render_shape_draw(mesh);
}

void render_shape_draw_using_depth_color(Render_Shape mesh, Render_Shader* shader_depth_color, Mat4 projection, Mat4 view, Mat4 model, Vec3 color)
{
    render_shader_set_vec3(shader_depth_color, "color", color);
    render_shader_set_mat4(shader_depth_color, "projection", projection);
    render_shader_set_mat4(shader_depth_color, "view", view);
    render_shader_set_mat4(shader_depth_color, "model", model);
    render_shape_draw(mesh);
}

void render_shape_draw_using_uv_debug(Render_Shape mesh, Render_Shader* uv_shader_debug, Mat4 projection, Mat4 view, Mat4 model)
{
    Mat4 inv = mat4_inverse_nonuniform_scale(model);
    Mat3 normal_matrix = mat3_from_mat4(inv);
    normal_matrix = mat3_from_mat4(model);

    render_shader_set_mat4(uv_shader_debug, "projection", projection);
    render_shader_set_mat4(uv_shader_debug, "view", view);
    render_shader_set_mat4(uv_shader_debug, "model", model);
    render_shader_set_mat3(uv_shader_debug, "normal_matrix", normal_matrix);
    render_shape_draw(mesh);
}

void render_shape_draw_using_blinn_phong(Render_Shape mesh, isize triangles_from, isize triangles_to, Render_Shader* blin_phong_shader, Blinn_Phong_Params params, Render_Map diffuse)
{

    render_shader_set_mat4(blin_phong_shader, "projection", params.projection);
    render_shader_set_mat4(blin_phong_shader, "view", params.view);
    render_shader_set_mat4(blin_phong_shader, "model", params.model);
    render_shader_set_mat3(blin_phong_shader, "normal_matrix", params.normal_matrix);

    render_shader_set_vec3(blin_phong_shader, "view_pos", params.view_pos);
    render_shader_set_vec3(blin_phong_shader, "light_pos", params.light_pos);
    render_shader_set_vec3(blin_phong_shader, "light_color", params.light_color);
    render_shader_set_vec3(blin_phong_shader, "ambient_color", params.ambient_color);
    render_shader_set_vec3(blin_phong_shader, "specular_color", params.specular_color);

    render_shader_set_f32(blin_phong_shader, "light_linear_attentuation", params.light_linear_attentuation);
    render_shader_set_f32(blin_phong_shader, "light_quadratic_attentuation", params.light_quadratic_attentuation);
    render_shader_set_f32(blin_phong_shader, "specular_exponent", params.specular_exponent);
    render_shader_set_f32(blin_phong_shader, "gamma", params.gamma);

    render_map_use(blin_phong_shader, "map_diffuse", diffuse, 0);
    
    render_shape_draw_range(mesh, triangles_from, triangles_to);
}

void render_shape_draw_using_skybox(Render_Shape mesh, Render_Shader* skybox_shader, Mat4 projection, Mat4 view, Mat4 model, f32 gamma, Render_Cubemap skybox)
{
    glDepthFunc(GL_LEQUAL);
    render_shader_set_mat4(skybox_shader, "projection", projection);
    render_shader_set_mat4(skybox_shader, "view", view);
    render_shader_set_mat4(skybox_shader, "model", model);
    render_shader_set_f32(skybox_shader, "gamma", gamma);
    
    render_cubeimage_use(skybox_shader, "cubemap_diffuse", &skybox, 0);
    
    render_shape_draw(mesh);
    glDepthFunc(GL_LESS);
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
