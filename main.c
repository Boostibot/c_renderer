//#define RUN_TESTS

#define _CRT_SECURE_NO_WARNINGS
#pragma warning(disable:4464) //Dissable "relative include path contains '..'"
#pragma warning(disable:4702) //Dissable "unrelachable code"
#pragma warning(disable:4820) //Dissable "Padding added to struct" 
#pragma warning(disable:4324) //Dissable "structure was padded due to alignment specifier" (when using explicti modifer to align struct)
#pragma warning(disable:4255) //Dissable "no function prototype given: converting '()' to '(void)"  
#pragma warning(disable:5045) //Dissable "Compiler will insert Spectre mitigation for memory load if /Qspectre switch specified"  
#pragma warning(disable:4201) //Dissable "nonstandard extension used: nameless struct/union" 
#pragma warning(disable:4189) //Dissable "local variable is initialized but not referenced" (for assert macro)
#pragma warning(disable:4296) //Dissable "expression is always true" (used for example in 0 <= val && val <= max where val is unsigned. This is used in generic CHECK_BOUNDS)

#define JOT_ALL_IMPL
#define LIB_MEM_DEBUG
#define DEBUG

#include "mdump.h"

#include "lib/profile_utils.h"
#include "lib/string.h"
#include "lib/platform.h"
#include "lib/allocator.h"
#include "lib/hash_index.h"
#include "lib/logger_file.h"
#include "lib/file.h"
#include "lib/allocator_debug.h"
#include "lib/allocator_malloc.h"
#include "lib/allocator_stack.h"
#include "lib/random.h"
#include "lib/math.h"
#include "lib/guid.h"
#include "lib/profile.h"
#include "lib/stable_array.h"

#include "gl_utils/gl.h"
#include "gl_utils/gl_shader_util.h"
#include "gl_utils/gl_debug_output.h"
#include "shapes.h"
#include "format_obj.h"
#include "image_loader.h"
#include "todo.h"
#include "resource_loading.h"
#include "render.h"
#include "camera.h"
#include "render_world.h"

#include "glfw/glfw3.h"
#include "control.h"

typedef struct App_Settings
{
    f32 fov;
    f32 movement_speed;
    f32 movement_sprint_mult;

    f32 screen_gamma;
    f32 screen_exposure;
    f32 paralax_heigh_scale;

    f32 mouse_sensitivity;
    f32 mouse_wheel_sensitivity;
    f32 mouse_sensitivity_scale_with_fov_ammount; //[0, 1]

    f32 zoom_adjust_time;
    i32 MSAA_samples;
} App_Settings;

typedef struct App_State
{
    App_Settings settings;
    Camera camera;
    
    f32 camera_yaw;
    f32 camera_pitch;

    Vec3 active_object_pos;
    Vec3 player_pos;

    f64 delta_time;
    f64 last_frame_timepoint;

    f32 zoom_target_fov;
    f64 zoom_target_time;
    f32 zoom_change_per_sec;

    i32 window_screen_width_prev;
    i32 window_screen_height_prev;
    i32 window_framebuffer_width_prev;
    i32 window_framebuffer_height_prev;
    
    i32 window_screen_width;
    i32 window_screen_height;
    i32 window_framebuffer_width;
    i32 window_framebuffer_height;

    bool is_in_mouse_mode;
    bool is_in_mouse_mode_prev;
    bool should_close;
    bool is_in_uv_debug_mode;

    Controls controls;
    Controls controls_prev;
    Control_Mapping control_mappings[CONTROL_MAPPING_SETS];
    bool key_states[GLFW_KEY_LAST + 1];
} App_State;


Vec3 gamma_correct(Vec3 color, f32 gamma)
{
    color.x = powf(color.x, 1.0f/gamma);
    color.y = powf(color.y, 1.0f/gamma);
    color.z = powf(color.z, 1.0f/gamma);

    return color;
}

Vec3 inv_gamma_correct(Vec3 color, f32 gamma)
{
    return gamma_correct(color, 1.0f/gamma);
}



typedef struct Fill_Ascii_Context {
    char* buffer;
    isize size;
} Fill_Ascii_Context;

void fill_random_ascii(char* buffer, isize size)
{
    const char* chars = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_-";
    isize i = 0;
    for(; i < size; i += 10)
    {
        //64 = 2^6 => x^64 can fit 10 random chars from a single
        //random number
        u64 random = random_u64();
        u8 rs[12] = {0};
        #define steps3(k) \
            rs[k + 0] = (u8) random % 64; random /= 64; \
            rs[k + 1] = (u8) random % 64; random /= 64; \

        steps3(0);
        steps3(2);
        steps3(4);
        steps3(6);
        steps3(8);
        #undef steps3

        isize remaining = MIN(size - i, 10);
        for(isize k = 0; k < remaining; k++)
        {
            char c = chars[rs[k]];
            buffer[k + i] = c;
        }
    }

    return;
}

char random_ascii()
{
    u64 random = random_u64();
    u64 random_capped = random % 64;
    const char* chars = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_-";
    return chars[random_capped];
}

char random_char()
{
    return (char) (random_u64());
}

bool _benchmark_fill_ascii(void* context)
{
    Fill_Ascii_Context* c = (Fill_Ascii_Context*) context; 
    fill_random_ascii(c->buffer, c->size);
    return true;
}

bool _benchmark_fill_ascii_by_one(void* context)
{
    Fill_Ascii_Context* c = (Fill_Ascii_Context*) context; 
    for(isize i = 0; i < c->size; i++)
        c->buffer[i] = random_ascii();

    return true;
}

void benchmark_random_cached(f64 discard, f64 time)
{
    f64 w = discard;
    f64 t = time;
    isize r = 16;
    
    char buffer[2048 + 1] = {0};
    Fill_Ascii_Context context = {buffer, 2048};

    log_perf_stats("TEST", LOG_INFO, "fill  ", perf_benchmark(w, t, r, _benchmark_fill_ascii, &context));
    log_perf_stats("TEST", LOG_INFO, "by_one", perf_benchmark(w, t, r, _benchmark_fill_ascii_by_one, &context));

    return;
}


void* glfw_malloc_func(size_t size, void* user);
void* glfw_realloc_func(void* block, size_t size, void* user);
void glfw_free_func(void* block, void* user);
void glfw_error_func(int code, const char* description);

void glfw_key_func(GLFWwindow* window, int key, int scancode, int action, int mods);
void glfw_mouse_button_func(GLFWwindow* window, int button, int action, int mods);
void glfw_scroll_func(GLFWwindow* window, f64 xoffset, f64 yoffset);

void controls_set_control_by_input(App_State* app, Input_Type input_type, isize index, f32 value, bool is_increment)
{
    for(isize i = 0; i < CONTROL_MAPPING_SETS; i++)
    {
        Control_Mapping* mapping = &app->control_mappings[i];
        u8* control_slot = control_mapping_get_entry(mapping, input_type, index);
        if(*control_slot != CONTROL_NONE)
        {
            u8* interactions = &app->controls.interactions[*control_slot];
            f32* stored_value = &app->controls.values[*control_slot];

            if(*interactions < UINT8_MAX)
                *interactions += 1;
    
            if(is_increment)
                *stored_value += value;
            else
                *stored_value = value;
        }
    }
}

void window_process_input(GLFWwindow* window, bool is_initial_call)
{
    App_State* app = (App_State*) glfwGetWindowUserPointer(window);
    if(is_initial_call)
    {
        //void glfw_framebuffer_resize_func(GLFWwindow* window, int width, int height);
        //void glfw_window_resize_func(GLFWwindow* window, float xscale, float yscale);
        //void glfw_cursor_position_callback(GLFWwindow* window, double xpos, double ypos);
        //glfwSetFramebufferSizeCallback(window, glfw_framebuffer_resize_func);
        //glfwSetWindowSizeCallback(window, glfw_window_resize_func);

        glfwSetKeyCallback(window, glfw_key_func);
        glfwSetMouseButtonCallback(window, glfw_mouse_button_func);
        glfwSetScrollCallback(window, glfw_scroll_func);
    }
    
    app->controls_prev = app->controls;
    memset(&app->controls.interactions, 0, sizeof app->controls.interactions);

    glfwPollEvents();
    app->should_close = glfwWindowShouldClose(window);
    
    STATIC_ASSERT(sizeof(int) == sizeof(i32));

    app->window_screen_width_prev = app->window_screen_width;
    app->window_screen_height_prev = app->window_screen_height;
    glfwGetWindowSize(window, (int*) &app->window_screen_width, (int*) &app->window_screen_height);

    app->window_framebuffer_width_prev = app->window_framebuffer_width;
    app->window_framebuffer_height_prev = app->window_framebuffer_height;
    glfwGetFramebufferSize(window, (int*) &app->window_framebuffer_width, (int*) &app->window_framebuffer_height);
    
    app->window_framebuffer_width = MAX(app->window_framebuffer_width, 1);
    app->window_framebuffer_height = MAX(app->window_framebuffer_height, 1);

    f64 new_mouse_x = 0;
    f64 new_mouse_y = 0;
    glfwGetCursorPos(window, &new_mouse_x, &new_mouse_y);
    controls_set_control_by_input(app, INPUT_TYPE_MOUSE, GLFW_MOUSE_X, (f32) new_mouse_x, false);
    controls_set_control_by_input(app, INPUT_TYPE_MOUSE, GLFW_MOUSE_Y, (f32) new_mouse_y, false);
    
    if(app->is_in_mouse_mode_prev != app->is_in_mouse_mode || is_initial_call)
    {
        if(app->is_in_mouse_mode == false)
        {
            if(glfwRawMouseMotionSupported())
                glfwSetInputMode(window, GLFW_RAW_MOUSE_MOTION, GLFW_TRUE);
            glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED); 
        }
        else
        {
            glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL); 
            glfwSetInputMode(window, GLFW_RAW_MOUSE_MOTION, GLFW_FALSE);
        }
    }

    //if(is_initial_call)
        //app->controls_prev = app->controls;

    app->is_in_mouse_mode_prev = app->is_in_mouse_mode;
}

void glfw_key_func(GLFWwindow* window, int key, int scancode, int action, int mods)
{
    (void) mods;
    (void) scancode;
    f32 value = 0.0f;
    if(action == GLFW_PRESS)
    {
        value = 1.0f;
    }
    else if(action == GLFW_RELEASE)
    {
        value = 0.0f;
    }
    else
        return;

    App_State* app = (App_State*) glfwGetWindowUserPointer(window);
    controls_set_control_by_input(app, INPUT_TYPE_KEY, key, value, false);
}

void glfw_mouse_button_func(GLFWwindow* window, int button, int action, int mods)
{
    (void) mods;
    f32 value = 0.0f;
    if(action == GLFW_PRESS)
        value = 1.0f;
    else if(action == GLFW_RELEASE)
        value = 0.0f;
    else
        return;

    App_State* app = (App_State*) glfwGetWindowUserPointer(window);
    controls_set_control_by_input(app, INPUT_TYPE_MOUSE_BUTTON, button, value, false);
}

void glfw_scroll_func(GLFWwindow* window, f64 xoffset, f64 yoffset)
{   
    (void) xoffset;
    f32 value = (f32) yoffset;
    App_State* app = (App_State*) glfwGetWindowUserPointer(window);
    controls_set_control_by_input(app, INPUT_TYPE_MOUSE, GLFW_MOUSE_SCROLL, value, true);
}

void* glfw_malloc_func(size_t size, void* user)
{
    return malloc_allocator_malloc((Malloc_Allocator*) user, size);
}

void* glfw_realloc_func(void* block, size_t size, void* user)
{
    return malloc_allocator_realloc((Malloc_Allocator*) user, block, size);
}

void glfw_free_func(void* block, void* user)
{
    malloc_allocator_free((Malloc_Allocator*) user, block);
}

void glfw_error_func(int code, const char* description)
{
    LOG_ERROR("APP", "GLWF error %d with message: %s", code, description);
}

void run_func(void* context);
void run_test_func(void* context);
void error_func(void* context, Platform_Sandbox_Error error);



int main()
{
    platform_init();

    Malloc_Allocator static_allocator = {0};
    malloc_allocator_init(&static_allocator);
    allocator_set_static(&static_allocator.allocator);
    
    Malloc_Allocator malloc_allocator = {0};
    malloc_allocator_init_use(&malloc_allocator, 0);
    
    error_system_init(&static_allocator.allocator);
    file_logger_init_use(&global_logger, &malloc_allocator.allocator, &malloc_allocator.allocator, "logs");

    Debug_Allocator debug_alloc = {0};
    debug_allocator_init_use(&debug_alloc, &malloc_allocator.allocator, DEBUG_ALLOCATOR_DEINIT_LEAK_CHECK | DEBUG_ALLOCATOR_CAPTURE_CALLSTACK);

    GLFWallocator allocator = {0};
    allocator.allocate = glfw_malloc_func;
    allocator.reallocate = glfw_realloc_func;
    allocator.deallocate = glfw_free_func;
    allocator.user = &malloc_allocator;
 
    glfwInitAllocator(&allocator);
    glfwSetErrorCallback(glfw_error_func);
    TEST_MSG(glfwInit(), "Failed to init glfw");

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_OPENGL_DEBUG_CONTEXT, true);  
 
    GLFWmonitor* monitor = glfwGetPrimaryMonitor();
    const GLFWvidmode* mode = glfwGetVideoMode(monitor);
    ASSERT(monitor && mode);
    if(monitor != NULL && mode != NULL)
    {
        glfwWindowHint(GLFW_RED_BITS, mode->redBits);
        glfwWindowHint(GLFW_GREEN_BITS, mode->greenBits);
        glfwWindowHint(GLFW_BLUE_BITS, mode->blueBits);
        glfwWindowHint(GLFW_REFRESH_RATE, mode->refreshRate);
    }
 
    GLFWwindow* window = glfwCreateWindow(1600, 900, "Render", NULL, NULL);
    TEST_MSG(window != NULL, "Failed to make glfw window");

    App_State app = {0};
    glfwSetWindowUserPointer(window, &app);
    glfwMakeContextCurrent(window);
    gladSetGLOnDemandLoader((GLADloadfunc) glfwGetProcAddress);

    gl_debug_output_enable();

    #ifndef RUN_TESTS
    platform_exception_sandbox(
        run_func, window, 
        error_func, window);
    #else
    platform_exception_sandbox(
        run_test_func, window, 
        error_func, window);
    #endif
    glfwDestroyWindow(window);
    glfwTerminate();

    debug_allocator_deinit(&debug_alloc);
    
    file_logger_deinit(&global_logger);
    error_system_deinit();

    //@TODO: fix
    ASSERT(malloc_allocator.bytes_allocated == 0);
    malloc_allocator_deinit(&malloc_allocator);
    platform_deinit();

    return 0;    
}


void process_first_person_input(App_State* app, f64 start_frame_time)
{
    App_Settings* settings = &app->settings;
    PERF_COUNTER_START(input_counter);

    //Movement
    {
        f32 move_speed = settings->movement_speed;
        if(control_is_down(&app->controls, CONTROL_SPRINT))
            move_speed *= settings->movement_sprint_mult;

        Vec3 direction_forward = vec3_norm(camera_get_look_dir(app->camera));
        Vec3 direction_up = vec3_norm(app->camera.up_dir);
        Vec3 direction_right = vec3_norm(vec3_cross(direction_forward, app->camera.up_dir));
            
        Vec3 move_dir = {0};
        move_dir = vec3_add(move_dir, vec3_scale(direction_forward, app->controls.values[CONTROL_MOVE_FORWARD]));
        move_dir = vec3_add(move_dir, vec3_scale(direction_up, app->controls.values[CONTROL_MOVE_UP]));
        move_dir = vec3_add(move_dir, vec3_scale(direction_right, app->controls.values[CONTROL_MOVE_RIGHT]));
            
        move_dir = vec3_add(move_dir, vec3_scale(direction_forward, -app->controls.values[CONTROL_MOVE_BACKWARD]));
        move_dir = vec3_add(move_dir, vec3_scale(direction_up, -app->controls.values[CONTROL_MOVE_DOWN]));
        move_dir = vec3_add(move_dir, vec3_scale(direction_right, -app->controls.values[CONTROL_MOVE_LEFT]));

        if(vec3_len(move_dir) != 0.0f)
            move_dir = vec3_norm(move_dir);

        Vec3 move_ammount = vec3_scale(move_dir, move_speed * (f32) app->delta_time);
        app->camera.pos = vec3_add(app->camera.pos, move_ammount);
    }

    //Camera rotation
    {
        f32 mousex_prev = app->controls_prev.values[CONTROL_LOOK_X];
        f32 mousey_prev = app->controls_prev.values[CONTROL_LOOK_Y];

        f32 mousex = app->controls.values[CONTROL_LOOK_X];
        f32 mousey = app->controls.values[CONTROL_LOOK_Y];
            
        if(mousex != mousex_prev || mousey != mousey_prev)
        {
            f64 xoffset = mousex - mousex_prev;
            f64 yoffset = mousey_prev - mousey; // reversed since y-coordinates range from bottom to top
            
            f32 fov_sens_modifier = sinf(app->zoom_target_fov);
            f32 total_sensitivity = settings->mouse_sensitivity * lerpf(1, fov_sens_modifier, settings->mouse_sensitivity_scale_with_fov_ammount);

            xoffset *= total_sensitivity;
            yoffset *= total_sensitivity;
            f32 epsilon = 1e-5f;

            app->camera_yaw   += (f32) xoffset;
            app->camera_pitch += (f32) yoffset; 
            app->camera_pitch = CLAMP(app->camera_pitch, -TAU/4.0f + epsilon, TAU/4.0f - epsilon);
        
            Vec3 direction = {0};
            direction.x = cosf(app->camera_yaw) * cosf(app->camera_pitch);
            direction.y = sinf(app->camera_pitch);
            direction.z = sinf(app->camera_yaw) * cosf(app->camera_pitch);

            app->camera.looking_at = vec3_norm(direction);
        }
    }
        
    //smooth zoom
    {
        //Works by setting a target fov, time to hit it and speed and then
        //interpolates smoothly until the value is hit
        if(app->zoom_target_fov == 0)
            app->zoom_target_fov = settings->fov;
                
        f32 zoom = app->controls.values[CONTROL_ZOOM];
        f32 zoom_prev = app->controls_prev.values[CONTROL_ZOOM];
        f32 zoom_delta = zoom_prev - zoom;
        if(zoom_delta != 0)
        {   
            f32 fov_sens_modifier = sinf(app->zoom_target_fov);
            f32 fov_delta = zoom_delta * settings->mouse_wheel_sensitivity * fov_sens_modifier;
                
            app->zoom_target_fov = CLAMP(app->zoom_target_fov + fov_delta, 0, TAU/2);
            app->zoom_target_time = start_frame_time + settings->zoom_adjust_time;
            app->zoom_change_per_sec = (app->zoom_target_fov - settings->fov) / settings->zoom_adjust_time;
        }
            
        if(start_frame_time < app->zoom_target_time)
        {
            f32 fov_before = settings->fov;
            settings->fov += app->zoom_change_per_sec * (f32) app->delta_time;

            //if is already past the target snap to target
            if(fov_before < app->zoom_target_fov && settings->fov > app->zoom_target_fov)
                settings->fov = app->zoom_target_fov;
            if(fov_before > app->zoom_target_fov && settings->fov < app->zoom_target_fov)
                settings->fov = app->zoom_target_fov;
        }
        else
            settings->fov = app->zoom_target_fov;
    }
            
    PERF_COUNTER_END(input_counter);
}

Mat4 mat4_from_transform(Transform trans)
{
    return mat4_translate(mat4_scaling(trans.scale), trans.translate);
}

GLuint get_uniform_block_info(GLuint program_handle, const char* block_name, const char* const* querried_names, GLint* offsets_or_null, GLint* sizes_or_null, GLint* strides_or_null, GLuint querried_count, GLint* block_size_or_null)
{
    GLuint blockIndex = glGetUniformBlockIndex(program_handle, block_name);
    if(block_size_or_null)
        glGetActiveUniformBlockiv(program_handle, blockIndex, GL_UNIFORM_BLOCK_DATA_SIZE, block_size_or_null);

    enum {max_indeces = 256};
    if(querried_count > max_indeces)
        querried_count = max_indeces;

    GLuint indices[max_indeces] = {0};
    glGetUniformIndices(program_handle, querried_count, querried_names, indices);

    if(offsets_or_null)
        glGetActiveUniformsiv(program_handle, querried_count, indices, GL_UNIFORM_OFFSET, offsets_or_null);

    if(sizes_or_null)
        glGetActiveUniformsiv(program_handle, querried_count, indices, GL_UNIFORM_SIZE, sizes_or_null);

    if(strides_or_null)
        glGetActiveUniformsiv(program_handle, querried_count, indices, GL_UNIFORM_ARRAY_STRIDE, strides_or_null);

    return blockIndex;
}

typedef struct Uniform_Block {
    GLuint index;
    GLuint handle;
    GLint  buffer_size;
    GLuint binding_point;
} Uniform_Block;

Uniform_Block uniform_block_make(GLuint program_handle, const char* name, GLuint binding_point)
{
    Uniform_Block out = {0};
    out.index = glGetUniformBlockIndex(program_handle, name);
                
    glGetActiveUniformBlockiv(program_handle, out.index , GL_UNIFORM_BLOCK_DATA_SIZE, &out.buffer_size);

    glUniformBlockBinding(program_handle, out.index, binding_point);
    glGenBuffers(1, &out.handle);

    glBindBuffer(GL_UNIFORM_BUFFER, out.handle);
    glBufferData(GL_UNIFORM_BUFFER, out.buffer_size, NULL, GL_DYNAMIC_DRAW);
    glBindBufferBase(GL_UNIFORM_BUFFER, binding_point, out.handle);
    
    return out;
}

#include "gl_utils/gl_pixel_format.h"

typedef struct Render_Texture_Array {
    GLuint handle;
    i32 width;
    i32 height;
    i32 layer_count; 

    i32 mip_level_count; //defaults to 1

    //either gl_forrmat or type and channel_count needs to be set correctly;
    GL_Pixel_Format gl_format; //needs to be specified or specified with
    i32 channel_count;
    Pixel_Type type; 

    GLuint mag_filter; //defaults to GL_LINEAR_MIPMAP_LINEAR
    GLuint min_filter; //defaults to GL_LINEAR
    GLuint wrap_s;     //defaults to GL_CLAMP_TO_EDGE
    GLuint wrap_t;     //defaults to GL_CLAMP_TO_EDGE
    
} Render_Texture_Array;

void render_texture_array_deinit(Render_Texture_Array* array)
{
    glDeleteTextures(1, &array->handle);
    array->handle = 0;
    //memset(array, 0, sizeof *array);
}

bool render_texture_array_init(Render_Texture_Array* partially_filled, void* data_or_null)
{
    render_texture_array_deinit(partially_filled);

    //source: https://www.khronos.org/opengl/wiki/Array_Texture#Creation_and_Management
    
    GL_Pixel_Format* p = &partially_filled->gl_format;
    if(partially_filled->type != PIXEL_TYPE_INVALID && p->internal_format == 0)
        *p = gl_pixel_format_from_pixel_type(partially_filled->type, partially_filled->type);
    else if(partially_filled->type == 0 && p->internal_format != 0)
        partially_filled->type = pixel_type_from_gl_internal_format(p->internal_format, &partially_filled->channel_count);

    if((partially_filled->type == 0 && p->internal_format == 0))
    {
        LOG_ERROR("render", "missing type in call to render_texture_array_init()");
        ASSERT(false);
        return false;
    }

    if(p->internal_format == 0)
    {
        LOG_ERROR("render", "submitted type is unrepresentable!");
        ASSERT(false);
        return false;
    }

    
    ASSERT_MSG(partially_filled->layer_count > 0 && partially_filled->width > 0 && partially_filled->height > 0, "No default!");

    if(partially_filled->mip_level_count == 0) partially_filled->mip_level_count = 1;
    if(partially_filled->min_filter == 0) partially_filled->min_filter = GL_LINEAR_MIPMAP_LINEAR;
    if(partially_filled->mag_filter == 0) partially_filled->mag_filter = GL_LINEAR;
    if(partially_filled->wrap_s == 0) partially_filled->wrap_s = GL_CLAMP_TO_EDGE;
    if(partially_filled->wrap_t == 0) partially_filled->wrap_t = GL_CLAMP_TO_EDGE;

    glGenTextures(1, &partially_filled->handle);

    glBindTexture(GL_TEXTURE_2D_ARRAY, partially_filled->handle);
    glTexStorage3D(GL_TEXTURE_2D_ARRAY, partially_filled->mip_level_count, p->internal_format, partially_filled->width, partially_filled->height, partially_filled->layer_count);

    if(data_or_null)
    {
        //                                   1. 2. 3. 4.
        glTexSubImage3D(GL_TEXTURE_2D_ARRAY, 0, 0, 0, 0, partially_filled->width, partially_filled->height, partially_filled->layer_count, p->access_format, p->channel_type, data_or_null);
        // 1. Mip level
        // 2. x of subrectangle
        // 3. y of subrectangle
        // 4. layer to which to write

        if(partially_filled->mip_level_count > 0)
            glGenerateMipmap(GL_TEXTURE_2D_ARRAY);
    }

    glTexParameteri(GL_TEXTURE_2D_ARRAY,GL_TEXTURE_MIN_FILTER, partially_filled->min_filter);
    glTexParameteri(GL_TEXTURE_2D_ARRAY,GL_TEXTURE_MAG_FILTER, partially_filled->mag_filter);
    glTexParameteri(GL_TEXTURE_2D_ARRAY,GL_TEXTURE_WRAP_S, partially_filled->wrap_s);
    glTexParameteri(GL_TEXTURE_2D_ARRAY,GL_TEXTURE_WRAP_T, partially_filled->wrap_t);

    gl_check_error();
    return true;
}

void render_texture_array_generate_mips(Render_Texture_Array* array)
{
    if(array->mip_level_count > 0)
    {
        glBindTexture(GL_TEXTURE_2D_ARRAY, array->handle);
        glGenerateMipmap(GL_TEXTURE_2D_ARRAY);
    }
}

void render_texture_array_set(Render_Texture_Array* array, i32 layer, i32 x, i32 y, Image image, bool regenerate_mips)
{
    glBindTexture(GL_TEXTURE_2D_ARRAY, array->handle);
    //@TODO: change from check bounds to error notify
    CHECK_BOUNDS(layer, array->layer_count);

    if(image.width > 0 && image.height > 0)
    {
        GL_Pixel_Format format = gl_pixel_format_from_pixel_type_size(image.type, image.pixel_size);
        glTexSubImage3D(GL_TEXTURE_2D_ARRAY, 0, x, y, layer, image.width, image.height, 1, format.access_format, format.channel_type, image.pixels);
    }

    if(regenerate_mips)
        glGenerateMipmap(GL_TEXTURE_2D_ARRAY);
}

//Fills txture array layer in such a way that mipmapping wont effect the edges of the image. 
//The image needs to be smaller than the texture array.
//That is the corner pixels will be expanded to fill the remianing size around the image.
//If the image is exactly the size of the array jsut copies it over.
bool render_texture_array_fill_layer(Render_Texture_Array* array, i32 layer, Image image, bool regenerate_mips, Image* temp_builder_or_null)
{
    bool state = true;
    CHECK_BOUNDS(layer, array->layer_count);
    
    if(image.width > array->width || image.height > array->height)
    {
        ASSERT(false);
        LOG_ERROR("render", "invalid texture size filled to array");
        state = false;
    }
    else if(image.width <= 0 || image.height <= 0)
    {
        if(regenerate_mips)
            render_texture_array_generate_mips(array);
    }
    else if(image.width == array->width && image.height == array->height)
    {
        render_texture_array_set(array, layer, 0, 0, image, regenerate_mips);
    }
    else
    {
        LOG_WARN("render", "image of size %d x %d doesnt fit exactly. Resorting back to extending it.", image.width, image.height);

        Image temp_storage = {0};
        Image* temp_builder = temp_builder_or_null;

        if(temp_builder_or_null == NULL)
        {
            image_init_unshaped(&temp_storage, allocator_get_scratch());
            temp_builder = &temp_storage;
        }

        ASSERT(image.height > 0 && image.width > 0);

        image_assign(temp_builder, image);
        
        isize stride = image_byte_stride(*temp_builder);
        i32 pixel_size = temp_builder->pixel_size;

        u8 color[3] = {0xff, 0, 0xff};
        //Extend sideways
        for(i32 y = 0; y < image.height; y++)
        {
            u8* curr_row = temp_builder->pixels + stride*y;
            u8* last_pixel = curr_row + pixel_size*(image.width - 1);

            //memset_pattern(curr_row + pixel_size*image.width, (array->width - image.width)*pixel_size, color, pixel_size);
            memset_pattern(curr_row + pixel_size*image.width, (array->width - image.width)*pixel_size, last_pixel, pixel_size);
        }

        //Extend downwards
        u8* last_row = temp_builder->pixels + stride*(image.height - 1);
        for(i32 y = image.height; y < temp_builder->height; y++)
        {
            u8* curr_row = temp_builder->pixels + stride*y;
            memmove(curr_row, last_row, stride);
        }
        
        //memset_pattern(temp_builder->pixels, image_all_pixels_size(*temp_builder), color, sizeof(color));

        render_texture_array_set(array, layer, 0, 0, *temp_builder, regenerate_mips);

        image_deinit(&temp_storage);
    }

    return state;
}

typedef struct Render_Texture_Layer {
    i32 width;
    i32 height;

    i32 layer;
    i32 resolution_index;
} Render_Texture_Layer;

typedef struct Render_Texture_Layer_Info {
    i32 used_width;
    i32 used_height;
    bool is_used;
} Render_Texture_Layer_Info;

DEFINE_ARRAY_TYPE(Render_Texture_Layer_Info, Render_Texture_Layer_Info_Array);

typedef struct Render_Texture_Resolution {
    Render_Texture_Array array;
    Render_Texture_Layer_Info_Array layers;
    i32 used_layers;
    i32 grow_by;
    
} Render_Texture_Resolution;

DEFINE_ARRAY_TYPE(Render_Texture_Resolution, Render_Texture_Resolution_Array);

//@TODO: what to do with outliers?
//@TODO: what to do with different formats?
//@TODO: rework images so that they have capacity! Add rehsape!

typedef struct Render_Texture_Manager {
    Render_Texture_Resolution_Array resolutions;
    Allocator* allocator;
} Render_Texture_Manager;

i32 int_log2_lower_bound(i32 val)
{
    if(val <= 0)
        return 0;
    return platform_find_last_set_bit32(val);
}

i32 int_log2_upper_bound(i32 val)
{
    ASSERT(val >= 0);
    i32 upper = int_log2_lower_bound(val);
    if(val > (1 << upper))
        upper += 1;

    return upper;
}

void render_texture_manager_generate_mips(Render_Texture_Manager* manager)
{
    for(isize i = 0; i < manager->resolutions.size; i++)
    {
        Render_Texture_Resolution* res = &manager->resolutions.data[i];
        render_texture_array_generate_mips(&res->array);
    }
}
 
isize render_texture_manager_add_resolution(Render_Texture_Manager* manager, i32 width, i32 height, i32 layers, i32 grow_by, Pixel_Type type, i32 channel_count)
{
    LOG_INFO("render", "adding texture resolution: " "%d x %d : %s x %d", width, height, pixel_type_name(type), channel_count);
    i32 max_dim = MAX(width, height);
    i32 log_2_resolution = int_log2_upper_bound(max_dim);

    Render_Texture_Resolution resolution = {0};
    resolution.array.mip_level_count = MAX(log_2_resolution - 1, 1);
    resolution.array.gl_format = gl_pixel_format_from_pixel_type(type, channel_count);
    resolution.array.type = type;
    resolution.array.channel_count = channel_count;
    resolution.array.layer_count = layers;
    resolution.array.width = width;
    resolution.array.height = height;
    resolution.grow_by = grow_by;
    resolution.used_layers = 0;

    isize out = -1;
    log_group_push();
    if(render_texture_array_init(&resolution.array, NULL))
    {
        array_init(&resolution.layers, manager->allocator);
        array_resize(&resolution.layers, layers);
        out = manager->resolutions.size;
        array_push(&manager->resolutions, resolution);
    }
    else
        LOG_ERROR("render", "Creation failed!");

    log_group_pop();

    return out;
}

void render_texture_manager_deinit(Render_Texture_Manager* manager)
{
    //@TODO
    ASSERT(false);
    //for(isize i = 0; i < manager->resolutions.size; i++)
    //{
    //    Render_Texture_Resolution* res = &manager->resolutions.data[i];
    //    render_texture_array_generate_mips(&res->array);
    //}

    memset(manager, 0, sizeof *manager);
}

void render_texture_manager_init(Render_Texture_Manager* manager, Allocator* allocator)
{
    //render_texture_manager_deinit(manager);
    if(allocator == NULL) 
        allocator = allocator_get_scratch();
        
    manager->allocator = allocator;
    array_init(&manager->resolutions, allocator);
}

void render_texture_manager_add_default_resolutions(Render_Texture_Manager* manager)
{
    i32 min_size = 32;
    i32 min_res_log2 = int_log2_lower_bound(min_size);

    i32 layer_counts[20] = {0};
    layer_counts[int_log2_lower_bound(32)] = 128;
    layer_counts[int_log2_lower_bound(64)] = 128;
    layer_counts[int_log2_lower_bound(128)] = 128;
    layer_counts[int_log2_lower_bound(256)] = 128;
    layer_counts[int_log2_lower_bound(512)] = 32;
    layer_counts[int_log2_lower_bound(1024)] = 32;
    layer_counts[int_log2_lower_bound(2048)] = 16;
    layer_counts[int_log2_lower_bound(4096)] = 8;

    isize combined_size = 0;
    for(i32 channels = 1; channels <= 3; channels ++)
    {
        for(i32 i = 0; i < 7; i++)
        {
            i32 size_log2 = (min_res_log2 + i);
            i32 size = 1 << size_log2;
            i32 layers = layer_counts[size_log2];
            i32 grow_by = layers / 8;
            
            i32 resolution_bytes_size = size*size*layers*channels;
            combined_size += resolution_bytes_size;
            
            render_texture_manager_add_resolution(manager, size, size, layers, grow_by, PIXEL_TYPE_U8, channels);
        }
    }

    LOG_WARN("render", "using " MEMORY_FMT " combined RAM on textures", MEMORY_PRINT(combined_size));
}

typedef enum Found_Type{
    NOT_FOUND = 0,
    FOUND_EXACT,
    FOUND_APPROXIMATE,
    FOUND_APPROXIMATE_BAD_FORMAT,
} Found_Type;

Render_Texture_Layer render_texture_manager_find(Render_Texture_Manager* manager, Found_Type* found_type_or_null, i32 width, i32 height, Pixel_Type type, i32 channel_count, bool used)
{
    PERF_COUNTER_START(render_texture_manager_find_counter);
    Render_Texture_Layer out = {0};

    bool state = false;
    i32 max_dim = MAX(width, height);
    i32 min_dim = MIN(width, height);
    isize needed_size = width * height;

    Found_Type found_type = NOT_FOUND;

    if(max_dim > 0)
    {
        //Simple for loop to find a best fit in terms of space wasted 
        // because this will not be called very often.
        isize min_diff = INT64_MAX;
        isize min_diff_index = -1;
        for(isize i = 0; i < manager->resolutions.size; i++)
        {
            Render_Texture_Resolution* resolution = &manager->resolutions.data[i];
            Render_Texture_Array* array = &resolution->array;
            isize max_layer_dim = MAX(array->width, array->height);
            isize min_layer_dim = MIN(array->width, array->height);

            //The layer must have:
            // 1) image_fits: big enough (can be rotated) @TODO: IMPLEMENT FURTHER DOWN!
            // 2) format_fits: the pixel format is the same and the number of channels is big enough
            // 3) has_space: needs to not be full or not be empty based on what we are looking for.
            bool image_fits = max_layer_dim >= max_dim && min_layer_dim >= min_dim;
            bool format_fits = array->type == type && array->channel_count >= channel_count;
            bool has_space = true;
         
            if(used == false)
                has_space = resolution->used_layers != resolution->layers.size;
            else
                has_space = resolution->used_layers != 0;

            if(image_fits && format_fits && has_space)
            {
                isize layer_size = array->width * array->height;
                isize size_diff = layer_size - needed_size;
                isize channel_diff = array->channel_count - channel_count;
                ASSERT(size_diff >= 0);

                //If number of channels dont match we effectively also waste all the other channels 
                // on that layer
                size_diff += channel_diff*layer_size;
                ASSERT(size_diff >= 0);

                if(min_diff > size_diff)
                {
                    min_diff = size_diff;
                    min_diff_index = i;
                }
            }
        }
        
        //If was found
        if(min_diff_index != -1)
        {
            Render_Texture_Resolution* resolution = &manager->resolutions.data[min_diff_index];
            if(min_diff == 0)
                found_type = FOUND_EXACT;
            else if(resolution->array.type == type)
                found_type = FOUND_APPROXIMATE;
            else
                found_type = FOUND_APPROXIMATE_BAD_FORMAT;

            out.resolution_index = (i32) min_diff_index + 1;
            out.layer = -1;
            for(isize i = 0; i < resolution->layers.size; i++)
            {
                if(resolution->layers.data[i].is_used == used)
                {
                    out.layer = (i32) i;
                    break;
                }
            }

            ASSERT_MSG(out.layer != -1, "The resolution->used_layers must be wrong!");
            state = true;
        }
    }

    if(found_type_or_null)
        *found_type_or_null = found_type;

    PERF_COUNTER_END(render_texture_manager_find_counter);
    return out;
}

//if((f64) manager->used_layers[resolution_index] >= layer_infos->size * 0.75)
//    LOG_WARN("render", "more than 75% texture slots used of size : width: %d height: %d", width, height);
//
//if(out.layer == -1)
//{
//    out.resolution = 0;
//    LOG_WARN("render", "out of render textures of size: width: %d height: %d", width, height);
//}

Render_Texture_Layer render_texture_manager_add(Render_Texture_Manager* manager, Image image)
{
    Found_Type found_type = NOT_FOUND;
    Render_Texture_Layer empty_slot = render_texture_manager_find(manager, &found_type, image.width, image.height, (Pixel_Type) image.type, image_channel_count(image), false);

    #define _ERROR_PARAMS "%d x %d : %s x %d", image.width, image.height, pixel_type_name(image.type), image_channel_count(image)

    if(empty_slot.resolution_index <= 0)
        LOG_ERROR("render", "render_texture_manager_add() Unable to find empty slot! " _ERROR_PARAMS);
    else
    {
        Render_Texture_Resolution* resolution = &manager->resolutions.data[empty_slot.resolution_index - 1];
        if(found_type == FOUND_APPROXIMATE)
        {
            LOG_WARN("render", "render_texture_manager_add() Found only approximate match!");
            LOG_WARN(">render", "image      " _ERROR_PARAMS);
            LOG_WARN(">render", "resolution %d x %d : %s x %d", resolution->array.width, resolution->array.height, pixel_type_name(resolution->array.type), resolution->array.channel_count);
        }
            
        if(found_type == FOUND_APPROXIMATE_BAD_FORMAT)
        {
            LOG_WARN("render", "render_texture_manager_add() Found only approximate match with wrong format!");
            LOG_WARN(">render", "image      " _ERROR_PARAMS);
            LOG_WARN(">render", "resolution %d x %d : %s x %d", resolution->array.width, resolution->array.height, pixel_type_name(resolution->array.type), resolution->array.channel_count);
        }

        resolution->used_layers += 1;

        Render_Texture_Layer_Info* layer_info = &resolution->layers.data[empty_slot.layer];
        layer_info->is_used = true;
        layer_info->used_width = image.width;
        layer_info->used_height = image.height;

        bool fill_state = render_texture_array_fill_layer(&resolution->array, empty_slot.layer, image, false, NULL);
        ASSERT(fill_state);
    }

    return empty_slot;
}

Error render_texture_manager_add_from_disk(Render_Texture_Manager* manager, String path, Render_Texture_Layer* image)
{
    LOG_INFO("render", "reading image at path '%s' current working dir '%s'", escape_string_ephemeral(path), platform_directory_get_current_working());
    log_group_push();
    PERF_COUNTER_START(image_read_counter);

    Image temp_storage = {allocator_get_scratch()};
    Error error = image_read_from_file(&temp_storage, path, 0, PIXEL_TYPE_U8, IMAGE_LOAD_FLAG_FLIP_Y);
    
    if(error_is_ok(error) == false)
    {
        LOG_ERROR("render", "error reading " ERROR_FMT " image!", ERROR_PRINT(error));
        ASSERT(false);
    }
    else
    {
        *image = render_texture_manager_add(manager, temp_storage);
    }
        
    image_deinit(&temp_storage);
    
    PERF_COUNTER_END(image_read_counter);
    log_group_pop();
    return error;
}

typedef struct Render_Batch_Group {
    i32 indeces_from;
    i32 indeces_to;

    Render_Texture_Layer diffuse;
    Render_Texture_Layer specular;
    
    Vec4 diffuse_color;
    Vec4 ambient_color;
    Vec4 specular_color;
    f32 specular_exponent;
    f32 metallic;
} Render_Batch_Group;

DEFINE_ARRAY_TYPE(Render_Batch_Group, Render_Batch_Group_Array);

typedef struct Render_Batch_Mesh {
    Render_Mesh mesh;
    Render_Batch_Group_Array groups;
} Render_Batch_Mesh;



void run_func(void* context)
{
    LOG_INFO("APP", "run_func enter");
    Allocator* upstream_alloc = allocator_get_default();

    GLFWwindow* window = (GLFWwindow*) context;
    App_State* app = (App_State*) glfwGetWindowUserPointer(window);
    App_Settings* settings = &app->settings;
    memset(app, 0, sizeof *app);

    app->camera.pos        = vec3(0.0f, 0.0f,  0.0f);
    app->camera.looking_at = vec3(1.0f, 0.0f,  0.0f);
    app->camera.up_dir     = vec3(0.0f, 1.0f,  0.0f);
    app->camera.is_position_relative = true;

    app->camera_yaw = -TAU/4;
    app->camera_pitch = 0.0;

    app->is_in_mouse_mode = false;
    settings->fov = TAU/4;
    settings->movement_speed = 2.5f;
    settings->movement_sprint_mult = 5;
    settings->screen_gamma = 2.2f;
    settings->screen_exposure = 1;
    settings->mouse_sensitivity = 0.002f;
    settings->mouse_wheel_sensitivity = 0.05f; // uwu
    settings->zoom_adjust_time = 0.1f;
    settings->mouse_sensitivity_scale_with_fov_ammount = 1.0f;
    settings->MSAA_samples = 4;

    mapping_make_default(app->control_mappings);
    
    Debug_Allocator resources_alloc = {0};
    Debug_Allocator renderer_alloc = {0};
    
    debug_allocator_init(&resources_alloc, upstream_alloc, DEBUG_ALLOCATOR_DEINIT_LEAK_CHECK | DEBUG_ALLOCATOR_CAPTURE_CALLSTACK);
    debug_allocator_init(&renderer_alloc, upstream_alloc, DEBUG_ALLOCATOR_DEINIT_LEAK_CHECK | DEBUG_ALLOCATOR_CAPTURE_CALLSTACK);

    Resources resources = {0};
    Render renderer = {0};

    render_world_init(&renderer_alloc.allocator);

    resources_init(&resources, &resources_alloc.allocator);
    resources_set(&resources);

    render_init(&renderer, &renderer_alloc.allocator);

    Render_Screen_Frame_Buffers_MSAA screen_buffers = {0};

    Shape uv_sphere = {0};
    Shape cube_sphere = {0};
    Shape screen_quad = {0};
    Shape unit_quad = {0};
    Shape unit_cube = {0};
    
    Shape combined_shape = {0};

    Render_Mesh render_uv_sphere = {0};
    Render_Mesh render_cube_sphere = {0};
    Render_Mesh render_screen_quad = {0};
    Render_Mesh render_cube = {0};
    Render_Mesh render_quad = {0};

    Render_Batch_Mesh render_batch = {0};

    Render_Image image_floor = {0};
    Render_Image image_debug = {0};
    Render_Image image_rusted_iron_metallic = {0};

    Render_Cubeimage cubemap_skybox = {0};

    Render_Shader shader_depth_color = {0};
    Render_Shader shader_solid_color = {0};
    Render_Shader shader_screen = {0};
    Render_Shader shader_debug = {0};
    Render_Shader shader_blinn_phong = {0};
    Render_Shader shader_skybox = {0};
    Render_Shader shader_instanced = {0};
    Render_Shader shader_instanced_batched = {0};

    Render_Mesh* render_indirect_meshes[] = {&render_cube, &render_uv_sphere};
    Render_Mesh render_indirect_mesh = {0};


    f64 fps_display_frequency = 4;
    f64 fps_display_last_update = 0;

    glEnable(GL_TEXTURE_CUBE_MAP_SEAMLESS);
    
    enum { 
        num_instances_x = 20,
        num_instances_y = 20,
        num_instances = num_instances_x * num_instances_y, 
        max_commands = 128,
        model_slot = 5 
    };

    GLuint instanceVBO = 0;
    {
        DEFINE_ARRAY_TYPE(Mat4, Mat4_Array);
        Mat4_Array models = {0};

        for(isize i = 0; i < num_instances; i++)
        {
            f32 x = (f32) (i / num_instances_x);
            f32 y = (f32) (i % num_instances_y);

            f32 wave_x = sinf(x/num_instances_x * 4 * TAU);
            f32 wave_y = sinf(y/num_instances_y * 4 * TAU);
            Transform transform = {0};
            transform.scale = vec3_of((wave_x*wave_x + wave_y*wave_y)/4 + 0.2f);
            transform.translate = vec3(x, y, 0);

            Mat4 mat = mat4_from_transform(transform);
            array_push(&models, mat);
        }

        glCreateBuffers(1, &instanceVBO);
        glNamedBufferData(instanceVBO, sizeof(Mat4) * models.size, models.data, GL_STATIC_DRAW);

        array_deinit(&models);
    }
    
    typedef  struct {
        GLuint count;
        GLuint instance_count;
        GLuint first_index;
        GLint  base_vertex;
        GLuint base_instance;
    } Draw_Elements_Indirect_Command;
    
    GLuint command_buffer = 0;
    Uniform_Block uniform_block_params = {0};
    Uniform_Block uniform_block_environment = {0};

    Render_Texture_Manager texture_manager = {0};
    render_texture_manager_init(&texture_manager, allocator_get_default());
    render_texture_manager_add_default_resolutions(&texture_manager);

    Render_Texture_Layer layer_floor = {0};
    Render_Texture_Layer layer_debug = {0};
    Render_Texture_Layer layer_tiles = {0};

    for(isize frame_num = 0; app->should_close == false; frame_num ++)
    {
        glfwSwapBuffers(window);
        f64 start_frame_time = clock_s();
        app->delta_time = start_frame_time - app->last_frame_timepoint; 
        app->last_frame_timepoint = start_frame_time; 

        window_process_input(window, frame_num == 0);
        if(app->window_framebuffer_width != app->window_framebuffer_width_prev 
            || app->window_framebuffer_height != app->window_framebuffer_height_prev
            || frame_num == 0)
        {
            LOG_INFO("APP", "Resizing");
            render_screen_frame_buffers_msaa_deinit(&screen_buffers);
            render_screen_frame_buffers_msaa_init(&screen_buffers, app->window_framebuffer_width, app->window_framebuffer_height, settings->MSAA_samples);

            //For some reason gets reset on change of frame buffers so we reset it as well
            glDeleteBuffers(1, &command_buffer);
            glCreateBuffers(1, &command_buffer);
            glNamedBufferData(command_buffer, sizeof(Draw_Elements_Indirect_Command) * max_commands, NULL, GL_DYNAMIC_DRAW);

            glViewport(0, 0, screen_buffers.width, screen_buffers.height);
        }
        
        if(control_was_pressed(&app->controls, CONTROL_REFRESH_ALL) 
            || control_was_pressed(&app->controls, CONTROL_REFRESH_SHADERS)
            || frame_num == 0)
        {
            LOG_INFO("APP", "Refreshing shaders");
            PERF_COUNTER_START(shader_load_counter);
            
            Error error = {0};
            error = ERROR_AND(error) render_shader_init_from_disk(&shader_solid_color,       STRING("shaders/solid_color.glsl"));
            error = ERROR_AND(error) render_shader_init_from_disk(&shader_depth_color,       STRING("shaders/depth_color.glsl"));
            error = ERROR_AND(error) render_shader_init_from_disk(&shader_screen,            STRING("shaders/screen.glsl"));
            error = ERROR_AND(error) render_shader_init_from_disk(&shader_blinn_phong,       STRING("shaders/blinn_phong.glsl"));
            error = ERROR_AND(error) render_shader_init_from_disk(&shader_skybox,            STRING("shaders/skybox.glsl"));
            error = ERROR_AND(error) render_shader_init_from_disk(&shader_debug,             STRING("shaders/uv_debug.glsl"));
            error = ERROR_AND(error) render_shader_init_from_disk(&shader_instanced,         STRING("shaders/instanced_texture.glsl"));
            error = ERROR_AND(error) render_shader_init_from_disk(&shader_instanced_batched, STRING("shaders/instanced_batched_texture.glsl"));

            ASSERT(error_is_ok(error));
            {
                uniform_block_params = uniform_block_make(shader_instanced_batched.shader, "Params", 0);
                uniform_block_environment = uniform_block_make(shader_instanced_batched.shader, "Environment", 1);

                GLint max_textures = 0;
                glGetIntegerv(GL_MAX_TEXTURE_IMAGE_UNITS, &max_textures);
                LOG_WARN("app", "max textures %i", max_textures);
            }

            PERF_COUNTER_END(shader_load_counter);
        }


        if(control_was_pressed(&app->controls, CONTROL_REFRESH_ALL) 
            || control_was_pressed(&app->controls, CONTROL_REFRESH_ART)
            || frame_num == 0)
        {
            LOG_INFO("APP", "Refreshing art");
            PERF_COUNTER_START(art_load_counter);

            PERF_COUNTER_START(art_counter_shapes);
            shape_deinit(&uv_sphere);
            shape_deinit(&cube_sphere);
            shape_deinit(&screen_quad);
            shape_deinit(&unit_cube);
            shape_deinit(&unit_quad);

            uv_sphere = shapes_make_uv_sphere(40, 1);
            cube_sphere = shapes_make_cube_sphere(40, 1);
            screen_quad = shapes_make_quad(2, vec3(0, 0, 1), vec3(0, 1, 0), vec3_of(0));
            unit_cube = shapes_make_unit_cube();
            unit_quad = shapes_make_unit_quad();
            PERF_COUNTER_END(art_counter_shapes);

            Error error = {0};

            render_mesh_init_from_shape(&render_uv_sphere, uv_sphere, STRING("uv_sphere"));
            render_mesh_init_from_shape(&render_cube_sphere, cube_sphere, STRING("cube_sphere"));
            render_mesh_init_from_shape(&render_screen_quad, screen_quad, STRING("screen_quad"));
            render_mesh_init_from_shape(&render_cube, unit_cube, STRING("unit_cube"));
            render_mesh_init_from_shape(&render_quad, unit_quad, STRING("unit_cube"));
            
            error = ERROR_AND(error) render_texture_manager_add_from_disk(&texture_manager, STRING("resources/floor.jpg"), &layer_floor);
            error = ERROR_AND(error) render_texture_manager_add_from_disk(&texture_manager, STRING("resources/debug.png"), &layer_debug);
            //error = ERROR_AND(error) render_texture_manager_add_from_disk(&texture_manager, STRING("resources/rustediron2/rustediron2_metallic.png"), &layer_tiles);

            render_texture_manager_generate_mips(&texture_manager);

            //error = ERROR_AND(error) render_image_init_from_disk(&image_floor, STRING("resources/floor.jpg"), STRING("floor"));
            //error = ERROR_AND(error) render_image_init_from_disk(&image_debug, STRING("resources/debug.png"), STRING("debug"));
            //error = ERROR_AND(error) render_image_init_from_disk(&image_rusted_iron_metallic, STRING("resources/rustediron2/rustediron2_metallic.png"), STRING("rustediron2"));
            error = ERROR_AND(error) render_cubeimage_init_from_disk(&cubemap_skybox, 
                STRING("resources/skybox_front.jpg"), 
                STRING("resources/skybox_back.jpg"), 
                STRING("resources/skybox_top.jpg"), 
                STRING("resources/skybox_bottom.jpg"), 
                STRING("resources/skybox_right.jpg"), 
                STRING("resources/skybox_left.jpg"), STRING("skybox"));

            PERF_COUNTER_END(art_load_counter);
            ASSERT(error_is_ok(error));

            {
                shape_deinit(&combined_shape);

                Render_Batch_Group group_cube_sphere = {0};
                group_cube_sphere.indeces_from = (i32) combined_shape.triangles.size*3;
                shape_append(&combined_shape, cube_sphere.vertices.data, cube_sphere.vertices.size, cube_sphere.triangles.data, cube_sphere.triangles.size);
                group_cube_sphere.indeces_to = (i32) combined_shape.triangles.size*3;
                
                Render_Batch_Group group_unit_cube = {0};
                group_unit_cube.indeces_from = (i32) combined_shape.triangles.size*3;
                shape_append(&combined_shape, unit_cube.vertices.data, unit_cube.vertices.size, unit_cube.triangles.data, unit_cube.triangles.size);
                group_unit_cube.indeces_to = (i32) combined_shape.triangles.size*3;

                render_mesh_init_from_shape(&render_batch.mesh, combined_shape, STRING("render_batch.mesh"));
                
                group_cube_sphere.diffuse = layer_debug;
                group_cube_sphere.diffuse_color = vec4(1, 1, 1, 1);
                group_cube_sphere.ambient_color = vec4(0, 0, 0, 1);
                group_cube_sphere.specular_color = vec4(0.9f, 0.9f, 0.9f, 0);
                group_cube_sphere.specular_exponent = 256;
                group_cube_sphere.metallic = 0;

                group_unit_cube.diffuse = layer_floor;
                group_unit_cube.diffuse_color = vec4(1, 1, 1, 1);
                group_unit_cube.ambient_color = vec4(0, 0, 0, 1);
                group_unit_cube.specular_color = vec4(0.5f, 0.5f, 0.5f, 0);
                group_unit_cube.specular_exponent = 3;

                array_push(&render_batch.groups, group_cube_sphere);
                array_push(&render_batch.groups, group_unit_cube);

            }

            {
                Render_Mesh* mesh = &render_batch.mesh;
                glBindBuffer(GL_ARRAY_BUFFER, instanceVBO);
                glBindVertexArray(mesh->vao);
                glVertexAttribPointer(model_slot + 0, 4, GL_FLOAT, GL_FALSE, sizeof(Mat4), (void*)offsetof(Mat4, col[0]));
                glVertexAttribPointer(model_slot + 1, 4, GL_FLOAT, GL_FALSE, sizeof(Mat4), (void*)offsetof(Mat4, col[1]));
                glVertexAttribPointer(model_slot + 2, 4, GL_FLOAT, GL_FALSE, sizeof(Mat4), (void*)offsetof(Mat4, col[2]));
                glVertexAttribPointer(model_slot + 3, 4, GL_FLOAT, GL_FALSE, sizeof(Mat4), (void*)offsetof(Mat4, col[3]));

                glEnableVertexAttribArray(model_slot + 0);	
                glEnableVertexAttribArray(model_slot + 1);	
                glEnableVertexAttribArray(model_slot + 2);	
                glEnableVertexAttribArray(model_slot + 3);	
    
                glVertexAttribDivisor(model_slot + 0, 1);  
                glVertexAttribDivisor(model_slot + 1, 1);  
                glVertexAttribDivisor(model_slot + 2, 1);  
                glVertexAttribDivisor(model_slot + 3, 1);  

                glBindVertexArray(0);
                glBindBuffer(GL_ARRAY_BUFFER, 0);
                gl_check_error();
            }
        }

        if(control_was_pressed(&app->controls, CONTROL_ESCAPE))
        {
            app->is_in_mouse_mode = !app->is_in_mouse_mode;
        }
        
        if(control_was_pressed(&app->controls, CONTROL_DEBUG_1))
        {
            log_perf_counters("app", LOG_INFO, true);
        }
        
        if(control_was_pressed(&app->controls, CONTROL_DEBUG_1))
        {
            app->is_in_uv_debug_mode = !app->is_in_uv_debug_mode;
        }

        if(app->is_in_mouse_mode == false)
            process_first_person_input(app, start_frame_time);

        
        app->camera.fov = settings->fov;
        app->camera.near = 0.01f;
        app->camera.far = 1000.0f;
        app->camera.is_ortographic = false;
        app->camera.is_position_relative = true;
        app->camera.aspect_ratio = (f32) app->window_framebuffer_width / (f32) app->window_framebuffer_height;

        Mat4 view = camera_make_view_matrix(app->camera);
        Mat4 projection = camera_make_projection_matrix(app->camera);

        
        //================ FIRST PASS ==================
        {
            PERF_COUNTER_START(first_pass_counter);
            render_screen_frame_buffers_msaa_render_begin(&screen_buffers);
            glClearColor(0.3f, 0.3f, 0.2f, 1.0f);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

            //render instanced sphere
            {
                #define MAX_LIGHTS 32
                #define MAX_BATCH 8
                #define MAX_RESOULTIONS 7
                
                typedef struct  {
                    MODIFIER_ALIGNED(16) Vec4 diffuse_color;
                    MODIFIER_ALIGNED(16) Vec4 ambient_color;
                    MODIFIER_ALIGNED(16) Vec4 specular_color;
                    MODIFIER_ALIGNED(4) float specular_exponent;
                    MODIFIER_ALIGNED(4) float metallic;
                    MODIFIER_ALIGNED(4) int map_diffuse; 
                    MODIFIER_ALIGNED(4) int map_specular; 
                    MODIFIER_ALIGNED(4) int map_normal; 
                    MODIFIER_ALIGNED(4) int map_ambient; 
                } Params;

                typedef struct {
                    MODIFIER_ALIGNED(16) Vec4 pos_and_range;
                    MODIFIER_ALIGNED(16) Vec4 color_and_radius;
                } Light;

                typedef struct {
                    MODIFIER_ALIGNED(16) Mat4 projection;
                    MODIFIER_ALIGNED(16) Mat4 view;
                    MODIFIER_ALIGNED(16) Vec4 view_pos;
                    MODIFIER_ALIGNED(16) Vec4 base_illumination;

                    MODIFIER_ALIGNED(4) float light_linear_attentuation;
                    MODIFIER_ALIGNED(4) float light_quadratic_attentuation;
                    MODIFIER_ALIGNED(4) float gamma;
                    MODIFIER_ALIGNED(4) int lights_count;

                    MODIFIER_ALIGNED(16) Light lights[MAX_LIGHTS];
                } Environment;

                Draw_Elements_Indirect_Command commands[2] = {0};

                u32 instance_step = num_instances / (u32) render_batch.groups.size;
                for(u32 i = 0; i < (u32) render_batch.groups.size; i++)
                {
                    Render_Batch_Group* group = &render_batch.groups.data[i];
                    commands[i].first_index = group->indeces_from;
                    commands[i].count = group->indeces_to - group->indeces_from;
                    commands[i].base_instance = instance_step * i;
                    commands[i].instance_count = instance_step;
                    commands[i].base_vertex = 0;
                }
                
                Params params[MAX_BATCH] = {0};
                Environment environment = {0};

                render_shader_use(&shader_instanced_batched);
                
                GLuint map_array_loc = render_shader_get_uniform_location(&shader_instanced_batched, "u_map_resolutions");
                GLint slot_index = 0;
                GLint map_slots[MAX_RESOULTIONS] = {0};
                for(GLint i = 0; i < MAX_RESOULTIONS && i < texture_manager.resolutions.size; i++)
                {
                    Render_Texture_Array* resolution_array = &texture_manager.resolutions.data[i].array;
                    glActiveTexture(GL_TEXTURE0 + slot_index);
                    glBindTexture(GL_TEXTURE_2D_ARRAY, resolution_array->handle);
                    map_slots[i] = slot_index;
                    slot_index += 1;
                }
                
                glUniform1iv(map_array_loc, (GLsizei) MAX_RESOULTIONS, map_slots);


                for(isize i = 0; i < render_batch.groups.size; i++)
                {
                    Render_Batch_Group* group = &render_batch.groups.data[i];
                    params[i].diffuse_color = group->diffuse_color;
                    params[i].specular_color = group->specular_color;
                    params[i].ambient_color = group->ambient_color;
                    params[i].specular_exponent = group->specular_exponent;
                    params[i].metallic = group->metallic;

                    #define COMPRESS_TEXTURE_LAYER(tex_layer) (tex_layer).layer | ((tex_layer).resolution_index << 16)

                    params[i].map_specular = COMPRESS_TEXTURE_LAYER(group->specular);
                    params[i].map_diffuse = COMPRESS_TEXTURE_LAYER(group->diffuse);
                }

                {
                    //This would also get abstarcted away to be asigned in the same place as render_batch.groups
                    environment.light_quadratic_attentuation = 0.05f;
                    environment.base_illumination = vec4_of(0.05f);
                    environment.projection = projection; 
                    environment.view = view; 
                    environment.view_pos = vec4_from_vec3(app->camera.pos);

                    environment.lights_count = 2;
                    environment.lights[0].color_and_radius = vec4(10, 8, 7, 0);
                    environment.lights[0].pos_and_range = vec4(40, 20, -10, 100);
                    
                    environment.lights[1].color_and_radius = vec4(8, 10, 8.5f, 0);
                    environment.lights[1].pos_and_range = vec4(0, 0, 10, 100);
                }

                ASSERT(sizeof(environment) == uniform_block_environment.buffer_size);
                ASSERT(sizeof(params) == uniform_block_params.buffer_size);
                
                glBindBuffer(GL_UNIFORM_BUFFER, uniform_block_environment.handle);
                glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(environment), &environment);
                
                glBindBuffer(GL_UNIFORM_BUFFER, uniform_block_params.handle);
                glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(params), &params);
                
                glBindBuffer(GL_DRAW_INDIRECT_BUFFER , command_buffer);
                glBufferSubData(GL_DRAW_INDIRECT_BUFFER, 0, sizeof(commands), commands);

                glBindVertexArray(render_batch.mesh.vao);
                glBindBuffer(GL_ARRAY_BUFFER, instanceVBO);

                glMultiDrawElementsIndirect(GL_TRIANGLES, GL_UNSIGNED_INT, NULL, (u32) render_batch.groups.size, 0);
                
            }

            //render skybox
            {
                Mat4 model = mat4_scaling(vec3_of(-1));
                Mat4 stationary_view = mat4_from_mat3(mat3_from_mat4(view));
                render_mesh_draw_using_skybox(render_cube, &shader_skybox, projection, stationary_view, model, 1, cubemap_skybox);
            }

            render_screen_frame_buffers_msaa_render_end(&screen_buffers);
            PERF_COUNTER_END(first_pass_counter);
        }

        // ============== POST PROCESSING PASS ==================
        {
            PERF_COUNTER_START(second_pass_counter);
            render_screen_frame_buffers_msaa_post_process_begin(&screen_buffers);
            render_mesh_draw_using_postprocess(render_screen_quad, &shader_screen, screen_buffers.screen_color_buff, settings->screen_gamma, settings->screen_exposure);
            render_screen_frame_buffers_msaa_post_process_end(&screen_buffers);
            PERF_COUNTER_END(second_pass_counter);
        }

        f64 end_frame_time = clock_s();
        f64 frame_time = end_frame_time - start_frame_time;
        if(end_frame_time - fps_display_last_update > 1.0/fps_display_frequency)
        {
            fps_display_last_update = end_frame_time;
            glfwSetWindowTitle(window, format_ephemeral("Render %5d fps", (int) (1.0f/frame_time)).data);
        }
    }
    
    shape_deinit(&uv_sphere);
    shape_deinit(&cube_sphere);
    shape_deinit(&screen_quad);
    shape_deinit(&unit_cube);
    shape_deinit(&unit_quad);
    
    render_mesh_deinit(&render_uv_sphere);
    render_mesh_deinit(&render_cube_sphere);
    render_mesh_deinit(&render_screen_quad);
    render_mesh_deinit(&render_cube);
    render_mesh_deinit(&render_quad);

    render_shader_deinit(&shader_solid_color);
    render_shader_deinit(&shader_depth_color);
    render_shader_deinit(&shader_screen);
    render_shader_deinit(&shader_blinn_phong);
    render_shader_deinit(&shader_skybox);
    render_shader_deinit(&shader_debug);

    resources_deinit(&resources);
    render_deinit(&renderer);

    render_image_deinit(&image_floor);
    render_image_deinit(&image_debug);
    render_cubeimage_deinit(&cubemap_skybox);

    render_screen_frame_buffers_msaa_deinit(&screen_buffers);
    
    log_perf_counters("APP", LOG_INFO, true);
    render_world_deinit();

    LOG_INFO("RESOURCES", "Resources allocation stats:");
    log_allocator_stats(">RESOURCES", LOG_INFO, &resources_alloc.allocator);

    debug_allocator_deinit(&resources_alloc);
    debug_allocator_deinit(&renderer_alloc);

    LOG_INFO("APP", "run_func exit");
}

void error_func(void* context, Platform_Sandbox_Error error)
{
    (void) context;
    const char* msg = platform_exception_to_string(error.exception);
    
    LOG_ERROR("APP", "%s exception occured", msg);
    LOG_TRACE("APP", "printing trace:");
    log_captured_callstack(">APP", LOG_ERROR, error.call_stack, error.call_stack_size);
}

#include "lib/_test_all.h"
void run_test_func(void* context)
{
    (void) context;
    test_all();
}
