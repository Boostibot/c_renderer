#pragma once

#include "log.h"
#include "camera.h"
#include "gl.h"
#include "gl_shader_util.h"
#include "resource_loading.h"

typedef struct Render_Screen_Frame_Buffers
{
    GLuint frame_buff;
    GLuint screen_color_buff;
    GLuint render_buff;

    i32 width;
    i32 height;

    String_Builder name;
} Render_Screen_Frame_Buffers;

typedef struct Render_Screen_Frame_Buffers_MSAA
{
    GLuint frame_buff;
    GLuint map_color_multisampled_buff;
    GLuint render_buff;
    GLuint intermediate_frame_buff;
    GLuint screen_color_buff;

    i32 width;
    i32 height;
    
    //used so that this becomes visible to debug_allocator
    // and thus we prevent leaking
    String_Builder name;
} Render_Screen_Frame_Buffers_MSAA;

typedef struct Render_Mesh
{
    String_Builder path;
    String_Builder name;

    GLuint vao;
    GLuint vbo;
    GLuint ebo;

    GLuint vertex_count;
    GLuint triangle_count;
} Render_Mesh;

typedef struct Render_Image
{
    String_Builder path;
    String_Builder name;

    GLuint map;
    isize width;
    isize height;
    
    GLuint pixel_format;
    GLuint channel_count;

    bool is_clamped;
} Render_Image;

typedef struct Render_Cubeimage
{
    String_Builder path;
    String_Builder name;

    GLuint map;

    isize widths[6];
    isize heights[6];
} Render_Cubeimage;

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


#define DEFINE_HANDLE_TYPES(Type)                           \
    typedef struct { i32 index; u32 generation; } Type##_Handle;             \
    typedef struct { i32 index; u32 generation; } Type##_Ref;                \
    DEFINE_ARRAY_TYPE(Type##_Handle, Type##_Handle_Array);  \
    DEFINE_ARRAY_TYPE(Type##_Ref, Type##_Ref_Array)         \

DEFINE_HANDLE_TYPES(Render_Mesh);
DEFINE_HANDLE_TYPES(Render_Image);
DEFINE_HANDLE_TYPES(Render_Cubeimage);
DEFINE_HANDLE_TYPES(Render_Shader);
DEFINE_HANDLE_TYPES(Render_Material);

typedef struct Render_Map
{
    Render_Image_Handle image;
    Map_Info info;
} Render_Map;

typedef struct Render_Cubemap
{
    Render_Cubeimage_Handle image;
    Map_Info info;
} Render_Cubemap;

typedef struct Render_Material
{
    String_Builder name;
    String_Builder path;

    Material_Info info;

    Render_Map maps[MAP_TYPE_ENUM_COUNT];
    Render_Cubemap cubemaps[CUBEMAP_TYPE_ENUM_COUNT];
} Render_Material;

#define MAX_SINGLE_COLOR_MAPS 20
#define MAX_PBR_UNIFORM_BUFFERS 10
#define MAX_PHONG_UNIFORM_BUFFERS 10

//global render state
typedef struct Render
{
    Allocator* alloc;
    Render_Image single_color_images[MAX_SINGLE_COLOR_MAPS];

    Handle_Table shaders;
    Handle_Table images;
    Handle_Table cubeimages;
    Handle_Table meshes;
    Handle_Table materials;
} Render;


#define PBR_MAX_LIGHTS 4
#define PBR_MAX_SPHERE_LIGHTS 128
#define PBR_MAX_SHADOW_CASTERS 16
#define PBR_MAX_QUAD_LIGHTS 4
#define PBR_MAX_LINE_LIGHTS 4

typedef struct Sphere_Light
{
    Vec3 pos;
    Vec3 color;
    f32 radius;
} Sphere_Light;

typedef struct Line_Light
{
    Vec3 pos1;
    Vec3 pos2;
    Vec3 color1;
    Vec3 color2;
    f32 radius;
} Line_Light;

typedef struct Quad_Light
{
    Vec3 top_left;
    Vec3 bottom_right;
    Vec3 color;
} Quad_Light;

typedef struct PBR_Params
{
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

typedef struct Render_Mesh_Environment
{
    int a;
} Render_Mesh_Environment;

typedef struct Render_Scene_Layer
{
    Name name;
} Render_Scene_Layer;

typedef struct Render_Command {
    Render_Environment* environment;
    //Triangle_Mesh_Ref mesh;
    //Shader_Ref shader;
    Transform transform;
    Render_Mesh_Environment mesh_environment;
} Render_Command;


DEFINE_ARRAY_TYPE(Render_Command, Render_Command_Array);

typedef struct Render_Command_Buffer
{
    Render_Command_Array commands; 
} Render_Command_Buffer;



//
// GameWorld world_world = {};
// Entity 


// render_layer_begin("Objects", NULL);
//    render_add_spherical_lights(world.lights);
//    render_add_spherical_lights(world.lights);
// 
//    render_add_mesh(mesh, environment, 
// render_layer_end();
//
// render_add_post_process(shader_postprocess1);
// render_add_post_process(shader_postprocess2);
//
// render_draw_frame();
// render_next_frame();

void render_scene_begin();
void render_scene_end();


#define VEC2_FMT "{%f, %f}"
#define VEC2_PRINT(vec) (vec).x, (vec).y

#define VEC3_FMT "{%f, %f, %f}"
#define VEC3_PRINT(vec) (vec).x, (vec).y, (vec).z

#define VEC4_FMT "{%f, %f, %f, %f}"
#define VEC4_PRINT(vec) (vec).x, (vec).y, (vec).z, (vec).w

void render_screen_frame_buffers_deinit(Render_Screen_Frame_Buffers* buffer)
{
    glBindVertexArray(0);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    glDeleteFramebuffers(1, &buffer->frame_buff);
    glDeleteBuffers(1, &buffer->screen_color_buff);
    glDeleteRenderbuffers(1, &buffer->render_buff);

    array_deinit(&buffer->name); 

    memset(buffer, 0, sizeof *buffer);
}

void render_screen_frame_buffers_init(Render_Screen_Frame_Buffers* buffer, i32 width, i32 height)
{
    render_screen_frame_buffers_deinit(buffer);

    LOG_INFO("RENDER", "render_screen_frame_buffers_init %-4d x %-4d", width, height);
    
    buffer->width = width;
    buffer->height = height;
    buffer->name = builder_from_cstring("Render_Screen_Frame_Buffers", NULL);

    //@NOTE: 
    //The lack of the following line caused me 2 hours of debugging why my application crashed due to NULL ptr 
    //deref in glDrawArrays. I still dont know why this occurs but just for safety its better to leave this here.
    glBindVertexArray(0);

    glGenFramebuffers(1, &buffer->frame_buff);
    glBindFramebuffer(GL_FRAMEBUFFER, buffer->frame_buff);    

    // generate map
    glGenTextures(1, &buffer->screen_color_buff);
    glBindTexture(GL_TEXTURE_2D, buffer->screen_color_buff);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB32F, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR );
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glBindTexture(GL_TEXTURE_2D, 0);

    // attach it to currently bound render_screen_frame_buffers object
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, buffer->screen_color_buff, 0);

    glGenRenderbuffers(1, &buffer->render_buff);
    glBindRenderbuffer(GL_RENDERBUFFER, buffer->render_buff); 
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, width, height);  
    glBindRenderbuffer(GL_RENDERBUFFER, 0);

    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, buffer->render_buff);

    TEST_MSG(glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE, "frame buffer creation failed!");

    glDisable(GL_FRAMEBUFFER_SRGB);
    glBindFramebuffer(GL_FRAMEBUFFER, 0); 
}

void render_screen_frame_buffers_render_begin(Render_Screen_Frame_Buffers* buffer)
{
    glBindFramebuffer(GL_FRAMEBUFFER, buffer->frame_buff); 
        
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_FRONT); 
    glFrontFace(GL_CW); 
}

void render_screen_frame_buffers_render_end(Render_Screen_Frame_Buffers* buffer)
{
    (void) buffer;
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void render_screen_frame_buffers_post_process_begin(Render_Screen_Frame_Buffers* buffer)
{
    (void) buffer;
    glBindFramebuffer(GL_FRAMEBUFFER, 0); // back to default
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
}

void render_screen_frame_buffers_post_process_end(Render_Screen_Frame_Buffers* buffer)
{
    (void) buffer;
    //nothing
}


void render_screen_frame_buffers_msaa_deinit(Render_Screen_Frame_Buffers_MSAA* buffer)
{
    glBindVertexArray(0);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    glDeleteFramebuffers(1, &buffer->frame_buff);
    glDeleteBuffers(1, &buffer->map_color_multisampled_buff);
    glDeleteRenderbuffers(1, &buffer->render_buff);
    glDeleteFramebuffers(1, &buffer->intermediate_frame_buff);
    glDeleteBuffers(1, &buffer->screen_color_buff);

    array_deinit(&buffer->name);

    memset(buffer, 0, sizeof *buffer);
}

bool render_screen_frame_buffers_msaa_init(Render_Screen_Frame_Buffers_MSAA* buffer, i32 width, i32 height, i32 sample_count)
{
    render_screen_frame_buffers_msaa_deinit(buffer);
    LOG_INFO("RENDER", "render_screen_frame_buffers_msaa_init %-4d x %-4d samples: %d", width, height, sample_count);

    glBindVertexArray(0);

    buffer->width = width;
    buffer->height = height;
    buffer->name = builder_from_cstring("Render_Screen_Frame_Buffers_MSAA", NULL);

    bool state = true;
    glGenFramebuffers(1, &buffer->frame_buff);
    glBindFramebuffer(GL_FRAMEBUFFER, buffer->frame_buff);

    // create a multisampled color attachment map
    glGenTextures(1, &buffer->map_color_multisampled_buff);
    glBindTexture(GL_TEXTURE_2D_MULTISAMPLE, buffer->map_color_multisampled_buff);
    glTexImage2DMultisample(GL_TEXTURE_2D_MULTISAMPLE, sample_count, GL_RGB32F, width, height, GL_TRUE);
    glBindTexture(GL_TEXTURE_2D_MULTISAMPLE, 0);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D_MULTISAMPLE, buffer->map_color_multisampled_buff, 0);

    // create a (also multisampled) renderbuffer object for depth and stencil attachments
    glGenRenderbuffers(1, &buffer->render_buff);
    glBindRenderbuffer(GL_RENDERBUFFER, buffer->render_buff);
    glRenderbufferStorageMultisample(GL_RENDERBUFFER, sample_count, GL_DEPTH24_STENCIL8, width, height);
    glBindRenderbuffer(GL_RENDERBUFFER, 0);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, buffer->render_buff);

    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
    {
        LOG_ERROR("RENDER", "frame buffer creation failed!");
        ASSERT(false);
        state = false;
    }
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    // configure second post-processing framebuffer
    glGenFramebuffers(1, &buffer->intermediate_frame_buff);
    glBindFramebuffer(GL_FRAMEBUFFER, buffer->intermediate_frame_buff);

    // create a color attachment map
    glGenTextures(1, &buffer->screen_color_buff);
    glBindTexture(GL_TEXTURE_2D, buffer->screen_color_buff);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB32F, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, buffer->screen_color_buff, 0);	// we only need a color buffer

    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
    {
        LOG_ERROR("RENDER", "frame buffer creation failed!");
        ASSERT(false);
        state = false;
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    return false;
}

void render_screen_frame_buffers_msaa_render_begin(Render_Screen_Frame_Buffers_MSAA* buffer)
{
    glBindFramebuffer(GL_FRAMEBUFFER, buffer->frame_buff); 
        
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_FRONT); 
    glFrontFace(GL_CW); 
}

void render_screen_frame_buffers_msaa_render_end(Render_Screen_Frame_Buffers_MSAA* buffer)
{
    (void) buffer;
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void render_screen_frame_buffers_msaa_post_process_begin(Render_Screen_Frame_Buffers_MSAA* buffer)
{
    // 2. now blit multisampled buffer(s) to normal colorbuffer of intermediate FBO. Image_Builder is stored in screenTexture
    glBindFramebuffer(GL_READ_FRAMEBUFFER, buffer->frame_buff);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, buffer->intermediate_frame_buff);
    glBlitFramebuffer(0, 0, buffer->width, buffer->height, 0, 0, buffer->width, buffer->height, GL_COLOR_BUFFER_BIT, GL_NEAREST);
    
    glBindFramebuffer(GL_FRAMEBUFFER, 0); // back to default
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
}

void render_screen_frame_buffers_msaa_post_process_end(Render_Screen_Frame_Buffers_MSAA* buffer)
{
    (void) buffer;
}


GLenum render_pixel_format_from_image_pixel_format(isize pixel_format)
{
    switch(pixel_format)
    {
        case PIXEL_FORMAT_U8:  return GL_UNSIGNED_BYTE;
        case PIXEL_FORMAT_U16: return GL_UNSIGNED_SHORT;
        case PIXEL_FORMAT_F32: return GL_FLOAT;
        default: ASSERT(false); return 0;
    }
}

GLenum render_channel_format_from_image_builder_channel_count(isize channel_count)
{
    switch(channel_count)
    {
        case 1: return GL_RED;
        case 2: return GL_RG;
        case 3: return GL_RGB;
        case 4: return GL_RGBA;
        default: ASSERT(false); return 0;
    }
}

void render_image_deinit(Render_Image* render)
{
    if(render->map != 0)
        glDeleteTextures(1, &render->map);
    array_deinit(&render->name);
    array_deinit(&render->path);
    memset(&render, 0, sizeof(render));
}

void render_image_init(Render_Image* render, Image image, String name, GLenum internal_format_or_zero)
{
    GLenum pixel_format = render_pixel_format_from_image_pixel_format(image.pixel_format);
    GLenum channel_format = render_channel_format_from_image_builder_channel_count(image_channel_count(image));

    Image_Builder contiguous = {0};

    //not contigous in memory => make contiguous copy
    if(image_is_contiguous(image) == false)
    {
        image_builder_init_from_image(&contiguous, allocator_get_scratch(), image);
        image = image_from_builder(contiguous);
    }

    if(internal_format_or_zero == 0)
        internal_format_or_zero = channel_format;

    render_image_deinit(render);
    render->height = image.height;
    render->width = image.width;
    render->pixel_format = pixel_format;
    render->channel_count = (u32) image_channel_count(image);
    render->name = builder_from_string(name, NULL);
    //array_init(render->name, )

    glGenTextures(1, &render->map);
    glBindTexture(GL_TEXTURE_2D, render->map);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);	
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexImage2D(GL_TEXTURE_2D, 0, internal_format_or_zero, (GLsizei) image.width, (GLsizei) image.height, 0, channel_format, pixel_format, image.pixels);
    glGenerateMipmap(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, 0);

    image_builder_deinit(&contiguous);
} 

void render_image_init_from_single_pixel(Render_Image* render, Vec4 color, u8 channels, String name)
{
    Image image = {0};
    u8 channel_values[4] = {
        (u8) (255*CLAMP(color.x, 0, 1)), 
        (u8) (255*CLAMP(color.y, 0, 1)), 
        (u8) (255*CLAMP(color.z, 0, 1)), 
        (u8) (255*CLAMP(color.w, 0, 1)), 
    };

    image.pixels = channel_values;
    image.pixel_size = channels;
    image.pixel_format = PIXEL_FORMAT_U8;
    image.width = 1;
    image.height = 1;
    image.containing_width = 1;
    image.containing_height = 1;

    render_image_init(render, image, name, 0);
}

void render_image_use(const Render_Image* render, isize slot)
{
    glActiveTexture(GL_TEXTURE0 + (GLenum) slot);
    glBindTexture(GL_TEXTURE_2D, render->map);
}

void render_image_unuse(const Render_Image* render)
{
    (void) render;
    glBindTexture(GL_TEXTURE_2D, 0);
}

void render_cubeimage_deinit(Render_Cubeimage* render)
{
    if(render->map != 0)
        glDeleteTextures(1, &render->map);
    array_deinit(&render->name);
    array_deinit(&render->path);
    memset(&render, 0, sizeof(render));
}

void render_cubeimage_init(Render_Cubeimage* render, const Image images[6], String name)
{
    for (isize i = 0; i < 6; i++)
    {
        ASSERT(image_pixel_count(images[i]) != 0);
        ASSERT(images[i].pixels != NULL && "cannot miss data");
    }

    render_cubeimage_deinit(render);
    render->name = builder_from_string(name, NULL);

    glGenTextures(1, &render->map);
    glBindTexture(GL_TEXTURE_CUBE_MAP, render->map);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);	
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);

    for (isize i = 0; i < 6; i++)
    {
        Image image = images[i];
        render->heights[i] = image.height;
        render->widths[i] = image.width;

        Image_Builder contiguous = {0};
        if(image_is_contiguous(image) == false)
        {
            image_builder_init_from_image(&contiguous, allocator_get_scratch(), image);
            image = image_from_builder(contiguous);
        }
        
        GLenum channel_format = render_channel_format_from_image_builder_channel_count(image_channel_count(image));
        GLenum pixel_format = render_pixel_format_from_image_pixel_format(image.pixel_format);
        //glTexImage2D(GL_TEXTURE_2D, 0, internal_format_or_zero, (GLsizei) image.width, (GLsizei) image.height, 0, channel_format, pixel_format, image.pixels);
        glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + (GLenum) i, 0, channel_format, (GLsizei) image.width, (GLsizei) image.height, 0, channel_format, pixel_format, image.pixels);

        image_builder_deinit(&contiguous);
    }

    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glGenerateMipmap(GL_TEXTURE_CUBE_MAP);
    glBindTexture(GL_TEXTURE_CUBE_MAP, 0);
} 

void render_cubeimage_use(const Render_Cubeimage* render, isize slot)
{
    glActiveTexture(GL_TEXTURE0 + (GLenum) slot);
    glBindTexture(GL_TEXTURE_CUBE_MAP, render->map);
}

void render_cubeimage_unuse(const Render_Cubeimage* render)
{
    (void) render;
    glBindTexture(GL_TEXTURE_CUBE_MAP, 0);
}

Error render_image_init_from_disk(Render_Image* tetxure, String path, String name)
{
    Image_Builder image_builder = {0};
    
    Allocator_Set prev_allocs = allocator_set_default(allocator_get_scratch());
    Error error = image_read_from_file(&image_builder, path, 0, PIXEL_FORMAT_U8, IMAGE_LOAD_FLAG_FLIP_Y);
    allocator_set(prev_allocs);
    
    ASSERT_MSG(error_is_ok(error), ERROR_FMT, ERROR_PRINT(error));
    if(error_is_ok(error))
        render_image_init(tetxure, image_from_builder(image_builder), name, 0);

    image_builder_deinit(&image_builder);

    return error;
}

Error render_image_init_hdr_from_disk(Render_Image* tetxure, String path, String name)
{
    Image_Builder image_builder = {0};
    
    Allocator_Set prev_allocs = allocator_set_default(allocator_get_scratch());
    Error error = image_read_from_file(&image_builder, path, 0, PIXEL_FORMAT_F32, IMAGE_LOAD_FLAG_FLIP_Y);
    allocator_set(prev_allocs);
    
    ASSERT_MSG(error_is_ok(error), ERROR_FMT, ERROR_PRINT(error));
    if(error_is_ok(error))
        render_image_init(tetxure, image_from_builder(image_builder), name, GL_RGB16F);

    image_builder_deinit(&image_builder);
    
    return error;
}

Error render_cubeimage_init_from_disk(Render_Cubeimage* render, String front, String back, String top, String bot, String right, String left, String name)
{
    Image_Builder face_image_builders[6] = {0};
    Image face_images[6] = {0};
    String face_paths[6] = {right, left, top, bot, front, back};

    Error error = {0};
    for (isize i = 0; i < 6; i++)
    {
        error = ERROR_OR(error) image_read_from_file(&face_image_builders[i], face_paths[i], 0, PIXEL_FORMAT_U8, IMAGE_LOAD_FLAG_FLIP_Y);
        face_images[i] = image_from_builder(face_image_builders[i]);
    }
    
    render_cubeimage_init(render, face_images, name);
    
    for (isize i = 0; i < 6; i++)
        image_builder_deinit(&face_image_builders[i]);

    return error;
}

void render_mesh_init(Render_Mesh* mesh, const Vertex* vertices, isize vertices_count, const Triangle_Index* triangles, isize triangles_count, String name)
{
    memset(mesh, 0, sizeof *mesh);

    glGenVertexArrays(1, &mesh->vao);
    glGenBuffers(1, &mesh->vbo);
    glGenBuffers(1, &mesh->ebo);
  
    glBindVertexArray(mesh->vao);
    glBindBuffer(GL_ARRAY_BUFFER, mesh->vbo);
    glBufferData(GL_ARRAY_BUFFER, vertices_count * sizeof(Vertex), vertices, GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mesh->ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, triangles_count * sizeof(Triangle_Index), triangles, GL_STATIC_DRAW);
    
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, pos));
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, uv));
    glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, norm));
    
    glEnableVertexAttribArray(0);	
    glEnableVertexAttribArray(1);	
    glEnableVertexAttribArray(2);	
    
    //@NOTE: this causes crashes??
    //@TODO: investigate
    
    //glBindBuffer(GL_ARRAY_BUFFER, 0);
    //glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

    glBindVertexArray(0);   
    
    builder_append(&mesh->name, name);
    mesh->triangle_count = (GLuint) triangles_count;
    mesh->vertex_count = (GLuint) vertices_count;
}

void render_mesh_init_from_shape(Render_Mesh* mesh, Shape shape, String name)
{
    render_mesh_init(mesh, shape.vertices.data, shape.vertices.size, shape.triangles.data, shape.triangles.size, name);
}

void render_mesh_deinit(Render_Mesh* mesh)
{
    array_deinit(&mesh->name);
    array_deinit(&mesh->path);
    glDeleteVertexArrays(1, &mesh->vao);
    glDeleteBuffers(1, &mesh->ebo);
    glDeleteBuffers(1, &mesh->vbo);
    memset(mesh, 0, sizeof *mesh);
}

void render_mesh_draw(Render_Mesh mesh)
{
    glBindVertexArray(mesh.vao);
    glDrawElements(GL_TRIANGLES, mesh.triangle_count * 3, GL_UNSIGNED_INT, 0);
    glBindVertexArray(0);
}

void render_mesh_draw_range(Render_Mesh mesh, isize from, isize to)
{
    CHECK_BOUNDS(from, to + 1);
    isize range = to - from;
    glBindVertexArray(mesh.vao);
    glDrawElements(GL_TRIANGLES, (GLsizei) range * 3, GL_UNSIGNED_INT, (void*)(from * sizeof(Triangle_Index)));
    glBindVertexArray(0);
}

typedef struct Render_Capture_Buffers
{
    GLuint frame_buff;
    GLuint render_buff;

    String_Builder name;
} Render_Capture_Buffers;

void render_capture_buffers_deinit(Render_Capture_Buffers* capture)
{
    glDeleteFramebuffers(1, &capture->frame_buff);
    glDeleteRenderbuffers(1, &capture->render_buff);
    array_deinit(&capture->name);
    memset(capture, 0, sizeof(*capture));
}

void render_capture_buffers_init(Render_Capture_Buffers* capture, i32 prealloc_width, i32 prealloc_height)
{
    render_capture_buffers_deinit(capture);
    capture->name = builder_from_cstring("Render_Capture_Buffers", NULL);

    glGenFramebuffers(1, &capture->frame_buff);
    glGenRenderbuffers(1, &capture->render_buff);

    glBindFramebuffer(GL_FRAMEBUFFER, capture->frame_buff);
    glBindRenderbuffer(GL_RENDERBUFFER, capture->render_buff);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, prealloc_width, prealloc_height);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, capture->render_buff);
    
    TEST_MSG(glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE, "frame buffer creation failed!");
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

INTERNAL void _make_capture_projections(Mat4* capture_projection, Mat4 capture_images[6])
{
    *capture_projection = mat4_perspective_projection(TAU/4, 1.0f, 0.1f, 10.0f);
    capture_images[0] = mat4_look_at(vec3(0.0f, 0.0f, 0.0f), vec3( 1.0f,  0.0f,  0.0f), vec3(0.0f, -1.0f,  0.0f));
    capture_images[1] = mat4_look_at(vec3(0.0f, 0.0f, 0.0f), vec3(-1.0f,  0.0f,  0.0f), vec3(0.0f, -1.0f,  0.0f));
    capture_images[2] = mat4_look_at(vec3(0.0f, 0.0f, 0.0f), vec3( 0.0f,  1.0f,  0.0f), vec3(0.0f,  0.0f,  1.0f));
    capture_images[3] = mat4_look_at(vec3(0.0f, 0.0f, 0.0f), vec3( 0.0f, -1.0f,  0.0f), vec3(0.0f,  0.0f, -1.0f));
    capture_images[4] = mat4_look_at(vec3(0.0f, 0.0f, 0.0f), vec3( 0.0f,  0.0f,  1.0f), vec3(0.0f, -1.0f,  0.0f));
    capture_images[5] = mat4_look_at(vec3(0.0f, 0.0f, 0.0f), vec3( 0.0f,  0.0f, -1.0f), vec3(0.0f, -1.0f,  0.0f));
}


void render_cubeimage_init_environment_from_environment_map(Render_Cubeimage* cubemap_environment, i32 width, i32 height, const Render_Image* environment_tetxure, f32 map_gamma,
    const Render_Capture_Buffers* capture, const Render_Shader* shader_equi_to_cubemap, Render_Mesh cube_mesh)
{
    render_cubeimage_deinit(cubemap_environment);
    cubemap_environment->name = builder_from_cstring("environment", NULL);

    glGenTextures(1, &cubemap_environment->map);
    glBindTexture(GL_TEXTURE_CUBE_MAP, cubemap_environment->map);
    for (u32 i = 0; i < 6; ++i)
    {
        cubemap_environment->widths[i] = width;
        cubemap_environment->heights[i] = height;
        glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, GL_RGB16F, width, height, 0, GL_RGB, GL_FLOAT, NULL);
    }
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR); 
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    // pbr: convert HDR equirectangular environment map to cubemap equivalent
    // ----------------------------------------------------------------------
    glViewport(0, 0, width, height); // don't forget to configure the viewport to the capture dimensions.
    glBindFramebuffer(GL_FRAMEBUFFER, capture->frame_buff);
    glBindRenderbuffer(GL_RENDERBUFFER, capture->render_buff);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, width, height);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    
    Mat4 capture_projection = {0};
    Mat4 capture_images[6] = {0};
    _make_capture_projections(&capture_projection, capture_images);

    render_shader_use(shader_equi_to_cubemap);
    render_shader_set_mat4(shader_equi_to_cubemap, "projection", capture_projection);
    render_shader_set_f32(shader_equi_to_cubemap, "gamma", map_gamma);
    render_image_use(environment_tetxure, 0);
    render_shader_set_i32(shader_equi_to_cubemap, "equirectangularMap", 0);

    for (u32 i = 0; i < 6; ++i)
    {
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, cubemap_environment->map, 0);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        render_shader_set_mat4(shader_equi_to_cubemap, "view", capture_images[i]);
        render_mesh_draw(cube_mesh);
    }
    render_shader_unuse(shader_equi_to_cubemap);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    
    // then let OpenGL generate mipmaps from first mip face (combatting visible dots artifact)
    glBindTexture(GL_TEXTURE_CUBE_MAP, cubemap_environment->map);
    glGenerateMipmap(GL_TEXTURE_CUBE_MAP);
}

void render_cubeimage_init_irradiance_from_environment(
    Render_Cubeimage* cubemap_irradiance, i32 width, i32 height, f32 sample_delta, const Render_Cubeimage* cubemap_environment,
    const Render_Capture_Buffers* capture, const Render_Shader* shader_irradiance_gen, Render_Mesh cube_mesh)
{
    // pbr: create an irradiance cubemap, and re-scale capture FBO to irradiance scale.
    // --------------------------------------------------------------------------------
    render_cubeimage_deinit(cubemap_irradiance);
    cubemap_irradiance->name = builder_from_cstring("irradiance", NULL);

    glGenTextures(1, &cubemap_irradiance->map);
    glBindTexture(GL_TEXTURE_CUBE_MAP, cubemap_irradiance->map);
    for (u32 i = 0; i < 6; ++i)
    {
        cubemap_irradiance->widths[i] = width;
        cubemap_irradiance->heights[i] = height;
        glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, GL_RGB16F, width, height, 0, GL_RGB, GL_FLOAT, NULL);
    }
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    glBindFramebuffer(GL_FRAMEBUFFER, capture->frame_buff);
    glBindRenderbuffer(GL_RENDERBUFFER, capture->render_buff);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, width, height);

    // pbr: solve diffuse integral by convolution to create an irradiance (cube)map.
    // -----------------------------------------------------------------------------
    
    Mat4 capture_projection = {0};
    Mat4 capture_images[6] = {0};
    _make_capture_projections(&capture_projection, capture_images);

    render_shader_use(shader_irradiance_gen);
    render_shader_set_mat4(shader_irradiance_gen, "projection", capture_projection);
    render_shader_set_f32(shader_irradiance_gen, "sample_delta", sample_delta);
    render_cubeimage_use(cubemap_environment, 0);
    render_shader_set_i32(shader_irradiance_gen, "environmentMap", 0);

    glViewport(0, 0, width, height); // don't forget to configure the viewport to the capture dimensions.
    glBindFramebuffer(GL_FRAMEBUFFER, capture->frame_buff);
    for (u32 i = 0; i < 6; ++i)
    {
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, cubemap_irradiance->map, 0);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        
        render_shader_set_mat4(shader_irradiance_gen, "view", capture_images[i]);
        render_mesh_draw(cube_mesh);
    }
    render_cubeimage_unuse(cubemap_environment);
    render_shader_unuse(shader_irradiance_gen);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void render_cubeimage_init_prefilter_from_environment(
    Render_Cubeimage* cubemap_irradiance, i32 width, i32 height, const Render_Cubeimage* cubemap_environment,
    const Render_Capture_Buffers* capture, const Render_Shader* shader_prefilter, Render_Mesh cube_mesh)
{
    // pbr: create a pre-filter cubemap, and re-scale capture FBO to pre-filter scale.
    // --------------------------------------------------------------------------------
    render_cubeimage_deinit(cubemap_irradiance);
    cubemap_irradiance->name = builder_from_cstring("pbr_prefilter", NULL);

    glGenTextures(1, &cubemap_irradiance->map);
    glBindTexture(GL_TEXTURE_CUBE_MAP, cubemap_irradiance->map);
    for (u32 i = 0; i < 6; ++i)
    {
        cubemap_irradiance->widths[i] = width;
        cubemap_irradiance->heights[i] = height;
        glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, GL_RGB16F, width, height, 0, GL_RGB, GL_FLOAT, NULL);
    }
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR); // be sure to set minification filter to mip_linear 
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    // generate mipmaps for the cubemap so OpenGL automatically allocates the required memory.
    glGenerateMipmap(GL_TEXTURE_CUBE_MAP);

    // pbr: run a quasi monte-carlo simulation on the environment lighting to create a prefilter (cube)map.
    // ----------------------------------------------------------------------------------------------------
    Mat4 capture_projection = {0};
    Mat4 capture_images[6] = {0};
    _make_capture_projections(&capture_projection, capture_images);

    render_shader_use(shader_prefilter);
    render_shader_set_mat4(shader_prefilter, "projection", capture_projection);
    render_cubeimage_use(cubemap_environment, 0);
    render_shader_set_i32(shader_prefilter, "environmentMap", 0);

    glBindFramebuffer(GL_FRAMEBUFFER, capture->frame_buff);
    u32 maxMipLevels = 5; //@TODO: hoist out
    for (u32 mip = 0; mip < maxMipLevels; ++mip)
    {
        // reisze framebuffer according to mip-level size.
        i32 mipWidth  = (i32)(width * pow(0.5, mip));
        i32 mipHeight = (i32)(height * pow(0.5, mip));
        glBindRenderbuffer(GL_RENDERBUFFER, capture->render_buff);
        glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, mipWidth, mipHeight);
        glViewport(0, 0, mipWidth, mipHeight);

        f32 roughness = (f32)mip / (f32)(maxMipLevels - 1);
        render_shader_set_f32(shader_prefilter, "roughness", roughness);
        for (u32 i = 0; i < 6; ++i)
        {
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, cubemap_irradiance->map, mip);

            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
            
            render_shader_set_mat4(shader_prefilter, "view", capture_images[i]);
            render_mesh_draw(cube_mesh);
        }
    }
    render_cubeimage_unuse(cubemap_environment);
    render_shader_unuse(shader_prefilter);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void render_image_init_BRDF_LUT(
    Render_Image* cubemap_irradiance, i32 width, i32 height,
    const Render_Capture_Buffers* capture, const Render_Shader* shader_brdf_lut, Render_Mesh screen_quad_mesh)
{
    // pbr: create a pre-filter cubemap, and re-scale capture FBO to pre-filter scale.
    // --------------------------------------------------------------------------------
    render_image_deinit(cubemap_irradiance);
    cubemap_irradiance->height = height;
    cubemap_irradiance->width = width;
    cubemap_irradiance->name = builder_from_cstring("BRDF_LUT", NULL);

    glGenTextures(1, &cubemap_irradiance->map);
    glBindTexture(GL_TEXTURE_2D, cubemap_irradiance->map);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RG16F, width, height, 0, GL_RG, GL_FLOAT, 0);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    // then re-configure capture framebuffer object and render screen-space quad with BRDF shader.
    glBindFramebuffer(GL_FRAMEBUFFER, capture->frame_buff);
    glBindRenderbuffer(GL_RENDERBUFFER, capture->render_buff);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, width, height);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, cubemap_irradiance->map, 0);

    glViewport(0, 0, width, height);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    render_shader_use(shader_brdf_lut);
    render_mesh_draw(screen_quad_mesh);
    render_shader_unuse(shader_brdf_lut);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glBindTexture(GL_TEXTURE_2D, 0);
}

void render_mesh_draw_using_solid_color(Render_Mesh mesh, const Render_Shader* shader_depth_color, Mat4 projection, Mat4 view, Mat4 model, Vec3 color, f32 gamma)
{
    render_shader_use(shader_depth_color);
    render_shader_set_vec3(shader_depth_color, "color", color);
    render_shader_set_mat4(shader_depth_color, "projection", projection);
    render_shader_set_mat4(shader_depth_color, "view", view);
    render_shader_set_mat4(shader_depth_color, "model", model);
    render_shader_set_f32(shader_depth_color, "gamma", gamma);
    render_mesh_draw(mesh);
    render_shader_unuse(shader_depth_color);
}

void render_mesh_draw_using_depth_color(Render_Mesh mesh, const Render_Shader* shader_depth_color, Mat4 projection, Mat4 view, Mat4 model, Vec3 color)
{
    render_shader_use(shader_depth_color);
    render_shader_set_vec3(shader_depth_color, "color", color);
    render_shader_set_mat4(shader_depth_color, "projection", projection);
    render_shader_set_mat4(shader_depth_color, "view", view);
    render_shader_set_mat4(shader_depth_color, "model", model);
    render_mesh_draw(mesh);
    render_shader_unuse(shader_depth_color);
}

void render_mesh_draw_using_uv_debug(Render_Mesh mesh, const Render_Shader* uv_shader_debug, Mat4 projection, Mat4 view, Mat4 model)
{
    render_shader_use(uv_shader_debug);
    Mat4 inv = mat4_inverse_nonuniform_scale(model);
    Mat3 normal_matrix = mat3_from_mat4(inv);
    normal_matrix = mat3_from_mat4(model);

    render_shader_set_mat4(uv_shader_debug, "projection", projection);
    render_shader_set_mat4(uv_shader_debug, "view", view);
    render_shader_set_mat4(uv_shader_debug, "model", model);
    render_shader_set_mat3(uv_shader_debug, "normal_matrix", normal_matrix);
    render_mesh_draw(mesh);
    render_shader_unuse(uv_shader_debug);
}

void render_mesh_draw_using_blinn_phong(Render_Mesh mesh, const Render_Shader* blin_phong_shader, Mat4 projection, Mat4 view, Mat4 model, Vec3 view_pos, Vec3 light_pos, Vec3 light_color, Blinn_Phong_Params params, Render_Image diffuse)
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
    
    render_mesh_draw(mesh);
    render_image_unuse(&diffuse);
    render_shader_unuse(blin_phong_shader);
}

void render_mesh_draw_using_skybox(Render_Mesh mesh, const Render_Shader* skybox_shader, Mat4 projection, Mat4 view, Mat4 model, f32 gamma, Render_Cubeimage skybox)
{
    glDepthFunc(GL_LEQUAL);
    render_shader_use(skybox_shader);
    render_shader_set_mat4(skybox_shader, "projection", projection);
    render_shader_set_mat4(skybox_shader, "view", view);
    render_shader_set_mat4(skybox_shader, "model", model);
    render_shader_set_f32(skybox_shader, "gamma", gamma);
    render_mesh_draw(mesh);
    
    render_cubeimage_use(&skybox, 0);
    render_shader_set_i32(skybox_shader, "cubemap_diffuse", 0);
    
    render_mesh_draw(mesh);
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

bool render_filled_map_use(Render_Filled_Map map, const Render_Shader* shader, const char* name, isize* slot)
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
bool render_map_use(Render_Map map, Render* render, const Render_Shader* shader, const char* name, isize* slot)
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
void render_mesh_draw_using_pbr(Render_Mesh mesh, const Render_Shader* pbr_shader, Mat4 projection, Mat4 view, Mat4 model, const PBR_Params* params, const Render_Filled_Material* material, 
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

void render_object(const Render_Object* render_object, Render* render, const Render_Shader* pbr_shader, Mat4 projection, Mat4 view, Mat4 model, const PBR_Params* params)
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
void render_mesh_draw_using_pbr_mapped(Render_Mesh mesh, const Render_Shader* pbr_shader, Mat4 projection, Mat4 view, Mat4 model, const PBR_Params* params, const Render_Material* material, 
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

void render_mesh_draw_using_postprocess(Render_Mesh screen_quad, const Render_Shader* shader_screen, GLuint screen_map, f32 gamma, f32 exposure)
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
