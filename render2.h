#pragma once

#include "lib/log.h"
#include "camera.h"
#include "gl_utils/gl.h"
#include "gl_utils/gl_shader_util.h"
#include "gl_utils/gl_pixel_format.h"
#include "gl_utils/gl_debug_output.h"
#include "resource_loading.h"
#include "resources.h"



typedef struct Render_Resource_Info {
    Id id;
    Resource_Ptr resource;
    Name name;
    i64 creation_etime;
    i64 last_used_etime;
    i64 resource_creation_etime;
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

typedef struct Render_Vertex_Data {
    GLuint vao;
    GLuint vbo;
    GLuint ebo;

    GLuint vertex_count;
    GLuint index_count;
    GLuint primitive_type;
} Render_Vertex_Data;

typedef struct Render_Map
{
    Render_Resource_Info info;
    
    Map_Info map_info;
    GLuint map;
    isize width;
    isize height;
    
    GL_Pixel_Format pixel_format;
} Render_Map;

typedef struct Render_Cubemap
{
    Render_Resource_Info info;
    
    GLuint map;

    Map_Info map_infos[6];
    GL_Pixel_Format pixel_formats[6];
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
    Material_Info material_info;
    Render_Ptr maps[MAP_TYPE_ENUM_COUNT];
    Render_Ptr cubemaps[CUBEMAP_TYPE_ENUM_COUNT];
} Render_Material;

DEFINE_ARRAY_TYPE(Render_Material, Render_Material_Array);



typedef struct Render_Internal_Command {
    GLuint indeces_count;
    GLuint instance_count;
    GLuint indeces_from;
    GLint  base_vertex;   
    GLuint base_instance;
} Render_Internal_Command;

enum {
    BLINN_PHONG_BATCH_SIZE = 8,
    BLINN_PHONG_NUM_BATCH_ENTRIES = 32,
    BLINN_PHONG_NUM_INSTANCES = 4096,
    BLINN_PHONG_NUM_COMMANDS = 128,
    BLINN_PHONG_MIN_TRIANGLES = 1024,
    BLINN_PHONG_MAX_TRIANGLES = 1024*1024,
};

typedef struct Render_Blinn_Phong_Vertex {
    Vec3 pos;
    Vec2 uv;
    Vec3 norm;
    Vec3 tan;
    Vec3 bitan;
} Render_Blinn_Phong_Vertex;


typedef struct Render_Blinn_Phong_Instance {
    Mat4 model;
} Render_Blinn_Phong_Instance;

typedef enum Render_Blinn_Phong_Uniforms {
    RENDER_BP_UNIFORM_COLOR_DIFFUSE,
    RENDER_BP_UNIFORM_COLOR_SPECULAR,
    RENDER_BP_UNIFORM_COLOR_AMBIENT,
    RENDER_BP_UNIFORM_SPECULAR_EXPONENT,

    RENDER_BP_UNIFORM_ENUM_COUNT
} Render_Blinn_Phong_Uniforms;

typedef struct Render_Blinn_Phong_Material_Uniforms {
    Vec3 diffuse_color;
    Vec3 ambient_color;
    Vec3 specular_color;
    f32 specular_exponent;
} Render_Blinn_Phong_Material_Uniforms;

typedef struct Render_Blinn_Phong_Buffers {
    bool is_init;
    GLuint command_buffer;
    GLuint instance_buffer;
    GLuint uniform_buffer;
    i32 instance_count;
    i32 uniform_buffer_size;
} Render_Blinn_Phong_Buffers;

void render_init_blinn_phong_buffers(Render_Blinn_Phong_Buffers& buffers, Render_Shader shader)
{
    
}

void render_blinn_phong_buffers_uniforms_fill(const Render_Material* materials, isize count)
{
    for(isize i = 0; i < count; i++)
    {
        
    }
}

typedef struct Render_Batch_Entry {
    i32 indeces_from;
    i32 indeces_to;
    i32 reference_count; //zero if slot empty
} Render_Batch_Entry;

DEFINE_ARRAY_TYPE(Render_Batch_Entry, Render_Batch_Entry_Array);

typedef struct Render_Batch {
    Id id;
    Render_Vertex_Data vertex_data;
    Render_Batch_Entry batch_entries[BLINN_PHONG_NUM_BATCH_ENTRIES];
    Name batch_names[BLINN_PHONG_NUM_BATCH_ENTRIES];
} Render_Batch;

typedef struct Render_Mesh {
    Render_Material material;
    Render_Ptr batch;
    i32 batch_entry;
    Render_Resource_Info info;
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

typedef struct Render_Scene_Layer {
    Name name;
} Render_Scene_Layer;

typedef struct Render_Command {
    Resource_Ptr environment;
    Resource_Ptr mesh;
    Shader_Type shader;
    Transform transform;
    f32 layer;
} Render_Command;

DEFINE_ARRAY_TYPE(Render_Command, Render_Command_Array);

//global render state
typedef struct Render
{
    Allocator* allocator;
    Allocator* scratch_allocator;

    Render_Shader shaders[SHADER_TYPE_ENUM_COUNT];

    Render_Blinn_Phong_Buffers blinn_phong_buffers;
    Render_Command_Array commands;
    Render_Command_Array last_frame_commands;

    Stable_Array cubemaps;
    Stable_Array maps;
    Stable_Array batches;
    Stable_Array geometries;
    
    Hash_Ptr hash_cubemaps;
    Hash_Ptr hash_maps;
    Hash_Ptr hash_batches;
    Hash_Ptr hash_geometries;
    Hash_Ptr model_to_geometries;

} Render;

void render_scene_begin();
void render_scene_end();


Render_Mesh* render_mesh_query(Render* render, Id id)
{
    return NULL;
}

Render_Material* render_material_query(Render* render, Id id)
{
    return NULL;
}

Render_Map* render_map_get(Render* render, Render_Ptr material)
{
    return NULL;
}

Mat4 mat4_from_transform(Transform transform)
{
    return mat4_scale_affine(mat4_translation(transform.translate), transform.translate);
}

int _compare_u64(u64 a, u64 b)
{
    if(a < b)
        return -1;
    else if(a > b)
        return 1;
    else
        return 0;
}

Render_Ptr get_geometry(Render* render, Resource_Ptr model, isize* index_or_null)
{
    Render_Ptr out = {0};

    //Render_Geometry* geometry = (Render_Geometry*) hash_ptr_get(render->model_to_geometries, (u64) model.id, 0);
    //if(geometry == NULL)
    //{
    //    
    //    Resource_Info* info = NULL;
    //    Trinagle* map = triangle_mesh_get_with_info(model.id, &info);
    //}

    //if(geometry != NULL)
    //    ASSERT(geometry->info.resource.id == mesh.id);

    if(index_or_null)
        *index_or_null = 0;

    return out;
}

typedef struct Validated_Render_Command {
    Resource_Ptr environment;
    Resource_Ptr mesh;
    Shader_Type shader;
    Transform transform;
    f32 layer;
    Render_Mesh* render_mesh;
} Validated_Render_Command;

DEFINE_ARRAY_TYPE(Validated_Render_Command, Validated_Render_Command_Array);

//@TODO: only sort indeces to commands and not the commands themselves
int _sort_validated_commands(const void* _a, const void* _b)
{
    Render* render = (Render*) NULL;
    const Validated_Render_Command* a = (Validated_Render_Command*) _a;
    const Validated_Render_Command* b = (Validated_Render_Command*) _b;

    if(a->layer < b->layer)
        return -1;
    else if(a->layer > b->layer)
        return 1;

    int diff_shader = _compare_u64((u64) a->shader, (u64) b->shader);
    if(diff_shader != 0)
        return diff_shader;
        
    int diff_env = _compare_u64((u64) a->environment.id, (u64) b->environment.id);
    if(diff_env != 0)
        return diff_env;

    if(a->render_mesh == b->render_mesh)
        return 0;

    int diff_mesh = _compare_u64((u64) a->render_mesh->batch.id, (u64) b->render_mesh->batch.id);
    if(diff_mesh != 0)
        return diff_mesh;
}

Validated_Render_Command_Array validate_render_commands(Render* render, Render_Command_Array commands)
{
    Validated_Render_Command_Array validated_commands = {render->scratch_allocator};
    array_reserve(&validated_commands, commands.size);

    for(isize i = 0; i < commands.size; i++)
    {
        Render_Command* command = &commands.data[i];
        Validated_Render_Command validated = {0};

        validated.layer = command->layer;
        validated.environment = command->environment;
        validated.shader = command->shader;
        validated.transform = command->transform;
        validated.mesh = command->mesh;
        validated.render_mesh = render_mesh_query(render, command->mesh.id);
        
        //No invlaid meshes! 
        //@TODO: draw some debug things
        ASSERT(validated.render_mesh);
        if(validated.render_mesh)
            array_push(&validated_commands, validated);
        else
            LOG_ERROR("render2", "invalid render command with mesh %llx", command->mesh.id);
    }

    return validated_commands;
}
Render_Batch* render_batch_get(Render* render, Render_Ptr)
{
    return NULL;
}

//@TODO: rename handles to handles (instead og shader.shader, map.map etc)

void render_batch(Render* render, 
        const Transform_Array transforms, 
        const i32 batch_slot_indeces[BLINN_PHONG_NUM_BATCH_ENTRIES], 
        const i32 batch_instance_indeces[BLINN_PHONG_NUM_BATCH_ENTRIES + 1],
        
        i32 batch_slots_used,
        Validated_Render_Command* validated, isize batch_from, isize batch_to)
{
    Validated_Render_Command* representant = &validated[batch_from];
    Render_Batch* batch = render_batch_get(render, representant->render_mesh->batch);
    if(representant->shader != SHADER_TYPE_BLINN_PHONG)
    {
        Render_Internal_Command commands[BLINN_PHONG_NUM_BATCH_ENTRIES] = {0};
        Render_Mesh* render_meshes[BLINN_PHONG_NUM_BATCH_ENTRIES] = {0};
        GLuint textures_difuse[BLINN_PHONG_NUM_BATCH_ENTRIES] = {0};
        GLuint textures_normal[BLINN_PHONG_NUM_BATCH_ENTRIES] = {0};

        //Prepare command buffer and texture data.
        for(i32 i = 0; i < batch_slots_used; i++)
        {
            i32 batch_slot = batch_slot_indeces[i];

            Render_Batch_Entry* batch_entry = &batch->batch_entries[batch_slot];
            commands[i].indeces_from = batch_entry->indeces_from;
            commands[i].indeces_count = batch_entry->indeces_to - batch_entry->indeces_from;
            commands[i].base_instance = batch_instance_indeces[i];
            commands[i].instance_count = batch_instance_indeces[i+1] - batch_instance_indeces[i];
            commands[i].base_vertex = 0;

            Validated_Render_Command* batched = &validated[batch_from + batch_slot];
            Render_Material* material = &batched->render_mesh->material;
            render_meshes[i] = (Render_Mesh*) &batched->render_mesh;

            //WAY TOO MANY GETS HERE!
            Render_Map* diffuse = render_map_get(render, material->maps[MAP_TYPE_DIFFUSE]);
            Render_Map* normal = render_map_get(render, material->maps[MAP_TYPE_NORMAL]);
            //...
            //@TODO: make into a forloop?
            textures_difuse[i] = diffuse->map;
            textures_normal[i] = normal->map;
        }

        Render_Blinn_Phong_Buffers* buffers = &render->blinn_phong_buffers;

        glBindBuffer(GL_UNIFORM_BUFFER, buffers->uniform_buffer);
        glBufferSubData(GL_UNIFORM_BUFFER, 0, buffers->uniform_buffer_size, buffers->uniform_buffer_block);

        //memcpy(buffers->uniform_buffer_offsets[RENDER_BP_UNIFORM_COLOR_AMBIENT]

        //@TODO: make into a forloop?
        //@TODO: layout binding!
        GLuint location_diffuse = render_shader_get_uniform_location(&render->shaders[SHADER_TYPE_BLINN_PHONG], "u_maps_diffuse");
        GLuint location_normal = render_shader_get_uniform_location(&render->shaders[SHADER_TYPE_BLINN_PHONG], "u_maps_normal");
        
        GLint map_slots_used = 0;
        GLint map_slots[BLINN_PHONG_NUM_BATCH_ENTRIES] = {0};

        for(isize i = 0; i < batch_slots_used; i++)
        {
            map_slots[i] = map_slots_used++;
            glActiveTexture(GL_TEXTURE0 + (GLenum) i);
            glBindTexture(GL_TEXTURE_2D, textures_difuse[i]);
        }
        glUniform1iv(location_diffuse, (GLsizei) batch_slots_used, map_slots);

        for(isize i = 0; i < batch_slots_used; i++)
        {
            map_slots[i] = map_slots_used++;
            glActiveTexture(GL_TEXTURE0 + (GLenum) i);
            glBindTexture(GL_TEXTURE_2D, textures_normal[i]);
        }
        glUniform1iv(location_diffuse, (GLsizei) batch_slots_used, map_slots);

        //...
    }
    else
        ASSERT(false);
}

void render_command_buffer(Render* render, Render_Command_Array commands)
{
    if(commands.size == 0)
        return;

    Render_Environment global_environment = {0};

    Validated_Render_Command_Array validated = validate_render_commands(render, commands);
    qsort(validated.data, sizeof *validated.data, validated.size, _sort_validated_commands);

    DEFINE_ARRAY_TYPE(Mat4, Mat4_Array);
    
    i32              batch_slot_indeces[BLINN_PHONG_NUM_BATCH_ENTRIES] = {0};
    i32              batch_instance_indeces[BLINN_PHONG_NUM_BATCH_ENTRIES] = {0};
    Transform_Array  batch_transforms[BLINN_PHONG_NUM_BATCH_ENTRIES] = {0};

    for(isize i = 0; i < STATIC_ARRAY_SIZE(batch_transforms); i++)
    {
        array_init(&batch_transforms[i], render->scratch_allocator);
        array_reserve(&batch_transforms[i], 32);
    }
    
    bool end_batch = false;
    isize last_command_i = -1;
    //Layer and shader
    for(isize command_i = 0; command_i < validated.size; last_command_i = command_i)
    {
        
        //Environment
        //for(; command_i < validated.size; )
        {
            Validated_Render_Command* representant = &validated.data[command_i];

            isize batch_from = 0;
            isize batch_to = 0;
            i32   batch_index = 0;
        
            for(isize i = 0; i < STATIC_ARRAY_SIZE(batch_transforms); i++)
                array_clear(&batch_transforms[i]);

            batch_from = command_i;

            //Element
            for(; command_i < validated.size; command_i++)
            {
                Validated_Render_Command* curr = &validated.data[command_i];
                ASSERT(curr->render_mesh != NULL);

                //If is different mesh test just how much different it is.
                //If the curr_render_mesh is in different batch storage break.
                if(representant->mesh.id != curr->mesh.id)
                {
                    if(curr->render_mesh->batch.id != representant->render_mesh->batch.id)
                        break;
                    else
                        batch_index += 1;
                }

                //If is category so different that it needs another batch stop.
                if(curr->shader != representant->shader 
                    || curr->layer != representant->layer)
                    break;


                //Append the transform
                array_push(&batch_transforms[batch_index], curr->transform);
                batch_slot_indeces[batch_index] = curr->render_mesh->batch_entry;
            }

            batch_to = command_i;
            render_batch(render, batch_transforms, batch_indeces, batch_index + 1, validated.data, batch_from, batch_to);
        }

        ASSERT_MSG(last_command_i != command_i, "stuck!");
    }

    for(isize i = 0; i < commands.size; i++)
    {
        Render_Command* command = &commands.data[i];

        Transform transform = command->transform;
        Render_Environment* environment = &global_environment;
        Render_Mesh* render_mesh = render_mesh_query(render, command->mesh.id);

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

Render_Resource_Info render_info_from_resource_info(const Resource_Info* info)
{
    Render_Resource_Info out = {0};

    name_from_builder(&out.name, info->path);
    out.creation_etime = platform_epoch_time();
    out.resource_creation_etime = info->creation_etime;
    out.id = id_generate();
    out.resource.id = info->id;
    out.resource.ptr = (Resource_Info*) info;

    return out;
}
bool render_map_from_map(Id map_id, Render_Ptr* render_map, Render* render)
{
    bool state = false;
    Resource_Info* info = NULL;
    Map* map = map_get_with_info(map_id, &info);
    if(map == NULL)
        LOG_ERROR("render", "invalid map_id submitted! %lli", (lli) map_id);
    else
    {
        Image_Builder* image = image_get(map->image);

        if(image == NULL)
            LOG_ERROR("render", "map with invalid image given to renderer. Map{name: %s, path: %s}", info->name.data, info->path.data);
        else
        {
            Render_Map out = {0};
            out.info = render_info_from_resource_info(info);
            
            out.map_info = map->info;
            out.height = image->height;
            out.width = image->width;
            out.pixel_format = gl_pixel_format_from_pixel_format((Pixel_Type) image->pixel_format, image_builder_channel_count(*image));

            if(out.pixel_format.unrepresentable)
                LOG_ERROR("render", "map with invalid pixel format %i given. Map{name: %s, path: %s}", image->pixel_format, info->name.data, info->path.data);
            else
            {
                glGenTextures(1, &out.map);
                glBindTexture(GL_TEXTURE_2D, out.map);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);	
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
                glTexImage2D(GL_TEXTURE_2D, 0, out.pixel_format.internal_format, (GLsizei) out.width, (GLsizei) out.height, 0, out.pixel_format.format, out.pixel_format.type, image->pixels);
                glGenerateMipmap(GL_TEXTURE_2D);
                glBindTexture(GL_TEXTURE_2D, 0);

                gl_check_error();
                
                Render_Map* addr = NULL;
                stable_array_insert(&render->cubemaps, (void**) &addr);
                *addr = out;
                hash_ptr_insert(&render->hash_cubemaps, (u64) map_id, (u64) addr);

                state = true;  
            }
        }
    }

    return state;
}

bool render_cubemap_from_cubemap(Id cubemap_id, Render_Ptr* render_cubemap, Render* render)
{
    bool state = false;
    Resource_Info* info = NULL;
    Cubemap* cubemap = cubemap_get_with_info(cubemap_id, &info);
    if(cubemap == NULL)
        LOG_ERROR("render", "invalid cubemap_id submitted! %lli", (lli) cubemap_id);
    else
    {
        Image_Builder* faces[6] = {NULL};
        
        bool had_null = false;
        for(isize i = 0; i < 6; i++)
        {
            faces[i] = image_get(cubemap->maps.faces[i].image);
            had_null = had_null || faces[i] == NULL;
        }

        if(had_null)
            LOG_ERROR("render", "Cubemap with invalid image/images given to renderer. Cubemap{name: %s, path: %s}", info->name.data, info->path.data);
        else
        {
            Render_Cubemap out = {0};
            out.info = render_info_from_resource_info(info);
            
            bool had_bad_format = false;
            for(isize i = 0; i < 6; i++)
            {
                Image_Builder* face = faces[i];
                out.map_infos[i] = cubemap->maps.faces[i].info;
                out.heights[i] = face->height;
                out.widths[i] = face->width;
                out.pixel_formats[i] = gl_pixel_format_from_pixel_format((Pixel_Type) face->pixel_format, image_builder_channel_count(*face));
            
                had_null = had_null || out.pixel_formats[i].unrepresentable;
            }

            if(had_bad_format)
                LOG_ERROR("render", "map with invalid pixel format given. Cubemap{name: %s, path: %s}", info->name.data, info->path.data);
            else
            {
                glGenTextures(1, &out.map);
                glBindTexture(GL_TEXTURE_CUBE_MAP, out.map);
                glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);	
                glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
                glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);

                for (isize i = 0; i < 6; i++)
                    glTexImage2D(GL_TEXTURE_2D, 0, out.pixel_formats[i].internal_format, (GLsizei) out.widths[i], (GLsizei) out.heights[i], 0, out.pixel_formats[i].format, out.pixel_formats[i].type, faces[i]->pixels);

                glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
                glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
                glGenerateMipmap(GL_TEXTURE_CUBE_MAP);
                glBindTexture(GL_TEXTURE_CUBE_MAP, 0);

                gl_check_error();

                ASSERT_MSG(false, "@TODO");
                render_cubemap->id = cubemap_id;
                stable_array_insert(&render->cubemaps, (void**) &render_cubemap->ptr);
                *(Render_Cubemap*) render_cubemap->ptr = out;
                hash_ptr_insert(&render->hash_cubemaps, (u64) cubemap_id, (u64) render_cubemap->ptr);


                state = true;  
            }
        }
    }

    return state;
}


Render_Shape* render_shape_from_shape(Id shape_id, Render* render)
{
    Render_Shape* out_ptr = NULL;
    Resource_Info* info = NULL;
    Shape_Assembly* shape = shape_get_with_info(shape_id, &info);
    if(shape == NULL)
        LOG_ERROR("render", "invalid shape_id submitted! %lli", (lli) shape_id);
    else
    {
        Render_Shape out = {0};
        out.info = render_info_from_resource_info(info);

        glGenVertexArrays(1, &out.vao);
        glGenBuffers(1, &out.vbo);
        glGenBuffers(1, &out.ebo);
  
        glBindVertexArray(out.vao);
        glBindBuffer(GL_ARRAY_BUFFER, out.vbo);
        glBufferData(GL_ARRAY_BUFFER, shape->vertices.size * sizeof(Vertex), shape->vertices.data, GL_STATIC_DRAW);

        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, out.ebo);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, shape->triangles.size * sizeof(Triangle_Index), shape->triangles.data, GL_STATIC_DRAW);
    
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, pos));
        glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, uv));
        glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, norm));
    
        glEnableVertexAttribArray(0);	
        glEnableVertexAttribArray(1);	
        glEnableVertexAttribArray(2);	
    
        stable_array_insert(&render->cubemaps, (void**) &out_ptr);
        hash_ptr_insert(&render->hash_cubemaps, (u64) shape_id, (u64) out_ptr);
        *out_ptr = out;

        glBindVertexArray(0);   
    }

    return out_ptr;
}


Render_Material* render_material_from_material(Id mat_id, Render* render);

bool render_mesh_from_mesh(Id mesh_id, Render_Ptr* render_mesh, Render* render)
{
    DEFINE_ARRAY_TYPE(Material*, Material_Ptr_Array);
    DEFINE_ARRAY_TYPE(Render_Material*, Render_Material_Ptr_Array);

    Render_Material_Ptr_Array render_materials = {0};
    array_init_backed(&render_materials, allocator_get_scratch(), 16);

    bool state = false;
    Resource_Info* info = NULL;
    Triangle_Mesh* mesh = triangle_mesh_get_with_info(mesh_id, &info);
    if(mesh == NULL)
        LOG_ERROR("render", "invalid mesh_id submitted! %lli", (lli) mesh_id);
    else
    {
        Shape_Assembly* shape = shape_get(mesh->shape);

        array_resize(&render_materials, mesh->materials.size);

        for(isize i = 0; i < mesh->materials.size; i++)
            render_materials.data[i] = render_material_from_material(mesh->materials.data[i], render);

        Render_Shape* render_shape = render_shape_from_shape(mesh->shape, render);

        if(shape == NULL)
            LOG_ERROR("render", "mesh with invalid shape given to renderer. Triangle_Mesh{name: %s, path: %s}", info->name.data, info->path.data);
        else
        {
            Render_Mesh out = {0};
            out.info = render_info_from_resource_info(info);
        }
    }

    array_deinit(&render_materials);
    return state;
}

void render_map_unuse(Render_Map map)
{
    (void) map;
    glBindTexture(GL_TEXTURE_2D, 0);
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
