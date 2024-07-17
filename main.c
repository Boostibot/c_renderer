#define _CRT_SECURE_NO_WARNINGS
#pragma warning(error:4820)   //error on "Padding added to struct" 
#pragma warning(error:4324)   //error "structure was padded due to alignment specifier" (when using explicti modifer to align struct)
#pragma warning(disable:4464) //Dissable "relative include path contains '..'"
#pragma warning(disable:4702) //Dissable "unrelachable code"
#pragma warning(disable:4255) //Dissable "no function prototype given: converting '()' to '(void)"  
#pragma warning(disable:5045) //Dissable "Compiler will insert Spectre mitigation for memory load if /Qspectre switch specified"  
#pragma warning(disable:4201) //Dissable "nonstandard extension used: nameless struct/union" 
#pragma warning(disable:4296) //Dissable "expression is always true" (used for example in 0 <= val && val <= max where val is unsigned. This is used in generic CHECK_BOUNDS)
#pragma warning(disable:4996) //Dissable "This function or variable may be unsafe. Consider using localtime_s instead. To disable deprecation, use _CRT_SECURE_NO_WARNINGS. See online help for details."

#define JOT_ALL_IMPL
#define JOT_ALL_TEST
#define JOT_COUPLED
//#define RUN_TESTS
//#define RUN_JUST_TESTS

//#include "mdump.h"

#include "lib/string.h"
#include "lib/platform.h"
#include "lib/allocator.h"
#include "lib/hash_index.h"
#include "lib/log_file.h"
#include "lib/file.h"
#include "lib/allocator_debug.h"
#include "lib/allocator_malloc.h"
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
#include "camera.h"

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
    f32 _padding1;

    f64 delta_time;
    f64 last_frame_timepoint;

    f32 zoom_target_fov;
    f32 _padding2;
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
    bool _padding3[3];
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
    malloc_allocator_init(&static_allocator, "static allocator");
    allocator_set_static(&static_allocator.allocator);
    
    Malloc_Allocator malloc_allocator = {0};
    malloc_allocator_init(&malloc_allocator, "fallback allocator");

    profile_init(&malloc_allocator.allocator);

    File_Logger file_logger = {0};
    //error_system_init(&static_allocator.allocator);
    file_logger_init_use(&file_logger, &malloc_allocator.allocator, "logs");

    Debug_Allocator debug_alloc = {0};
    debug_allocator_init_use(&debug_alloc, &malloc_allocator.allocator, DEBUG_ALLOCATOR_DEINIT_LEAK_CHECK | DEBUG_ALLOCATOR_CAPTURE_CALLSTACK);

    #ifdef RUN_TESTS
    platform_exception_sandbox(
        run_test_func, NULL, 
        error_func, NULL);
    #endif

    bool quit = false;
    #ifdef RUN_JUST_TESTS
        quit = true;
    #endif
    if(quit)
        return 0;

    GLFWallocator allocator = {0};
    allocator.allocate = glfw_malloc_func;
    allocator.reallocate = glfw_realloc_func;
    allocator.deallocate = glfw_free_func;
    allocator.user = &malloc_allocator;
 
    glfwInitAllocator(&allocator);
    glfwSetErrorCallback(glfw_error_func);
    TEST(glfwInit(), "Failed to init glfw");

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
    TEST(window != NULL, "Failed to make glfw window");

    App_State app = {0};
    glfwSetWindowUserPointer(window, &app);
    glfwMakeContextCurrent(window);
    gladSetGLOnDemandLoader((GLADloadfunc) glfwGetProcAddress);

    gl_debug_output_enable();

    platform_exception_sandbox(
        run_func, window, 
        error_func, window);

    glfwDestroyWindow(window);
    glfwTerminate();

    debug_allocator_deinit(&debug_alloc);
    
    file_logger_deinit(&file_logger);
    //error_system_deinit();

    //@TODO: fix
    ASSERT(malloc_allocator.bytes_allocated == 0);
    malloc_allocator_deinit(&malloc_allocator);
    platform_deinit();

    return 0;    
}


void process_first_person_input(App_State* app, f64 start_frame_time)
{
    App_Settings* settings = &app->settings;
    PROFILE_START(input_counter);

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
            
    PROFILE_END(input_counter);
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

    enum {max_indices = 256};
    if(querried_count > max_indices)
        querried_count = max_indices;

    GLuint indices[max_indices] = {0};
    glGetUniformIndices(program_handle, querried_count, querried_names, indices);

    if(offsets_or_null)
        glGetActiveUniformsiv(program_handle, querried_count, indices, GL_UNIFORM_OFFSET, offsets_or_null);

    if(sizes_or_null)
        glGetActiveUniformsiv(program_handle, querried_count, indices, GL_UNIFORM_SIZE, sizes_or_null);

    if(strides_or_null)
        glGetActiveUniformsiv(program_handle, querried_count, indices, GL_UNIFORM_ARRAY_STRIDE, strides_or_null);

    return blockIndex;
}

typedef struct GL_Shader_Block_Info {
    GLuint index;
    GLint  buffer_size;
    GLuint binding_point;
} GL_Shader_Block_Info;

GL_Shader_Block_Info uniform_block_make(GLuint program_handle, const char* name,  GLuint binding_point, GLuint buffer)
{
    GLuint index = glGetUniformBlockIndex(program_handle, name);
         
    GLint buffer_size = 0;
    //glGetProgramResourceIndex for more general.
    glGetActiveUniformBlockiv(program_handle, index , GL_UNIFORM_BLOCK_DATA_SIZE, &buffer_size);

    glUniformBlockBinding(program_handle, index, binding_point);

    glBindBuffer(GL_UNIFORM_BUFFER, buffer);
    glBindBufferBase(GL_UNIFORM_BUFFER, binding_point, buffer);
    
    GL_Shader_Block_Info out = {0};
    out.index = index;
    out.binding_point = binding_point;
    out.buffer_size = buffer_size;
    
    return out;
}

GL_Shader_Block_Info shader_storage_block_make(GLuint program_handle, const char* name, GLuint binding_point, GLuint buffer)
{
    //Out2 because visual studio is a buggy mess and fails to display "out" ??!?!?
    GLuint index = glGetProgramResourceIndex(program_handle, GL_SHADER_STORAGE_BLOCK, name);

    //GLenum queried_properties[] = {GL_BUFFER_DATA_SIZE};
    //glGetProgramResourceiv(program_handle, GL_SHADER_STORAGE_BUFFER, out.index, ARRAY_SIZE(queried_properties), queried_properties, 1, NULL, &out.buffer_size);

    glShaderStorageBlockBinding(program_handle, index, binding_point);

    glBindBuffer(GL_SHADER_STORAGE_BUFFER, buffer);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, binding_point, buffer);
    
    GL_Shader_Block_Info out = {0};;
    out.index = index;
    out.binding_point = binding_point;
    return out;
}

#include "gl_utils/gl_pixel_format.h"

typedef struct GL_Texture_Array {
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
    
} GL_Texture_Array;

void gl_texture_array_deinit(GL_Texture_Array* array)
{
    glDeleteTextures(1, &array->handle);
    array->handle = 0;
    //memset(array, 0, sizeof *array);
}

bool gl_texture_array_init(GL_Texture_Array* partially_filled, void* data_or_null)
{
    gl_texture_array_deinit(partially_filled);

    //source: https://www.khronos.org/opengl/wiki/Array_Texture#Creation_and_Management
    
    GL_Pixel_Format* p = &partially_filled->gl_format;
    if(partially_filled->type != PIXEL_TYPE_INVALID && p->internal_format == 0)
        *p = gl_pixel_format_from_pixel_type(partially_filled->type, partially_filled->type);
    else if(partially_filled->type == 0 && p->internal_format != 0)
        partially_filled->type = pixel_type_from_gl_internal_format(p->internal_format, &partially_filled->channel_count);

    if((partially_filled->type == 0 && p->internal_format == 0))
    {
        LOG_ERROR("render", "missing type in call to gl_texture_array_init()");
        ASSERT(false);
        return false;
    }

    if(p->internal_format == 0)
    {
        LOG_ERROR("render", "submitted type is unrepresentable!");
        ASSERT(false);
        return false;
    }

    
    ASSERT(partially_filled->layer_count > 0 && partially_filled->width > 0 && partially_filled->height > 0, "No default!");

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

void gl_texture_array_generate_mips(GL_Texture_Array* array)
{
    if(array->mip_level_count > 0)
    {
        glBindTexture(GL_TEXTURE_2D_ARRAY, array->handle);
        glGenerateMipmap(GL_TEXTURE_2D_ARRAY);
    }
}

void gl_texture_array_set(GL_Texture_Array* array, i32 layer, i32 x, i32 y, Image image, bool regenerate_mips)
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
bool gl_texture_array_fill_layer(GL_Texture_Array* array, i32 layer, Image image, bool regenerate_mips)
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
            gl_texture_array_generate_mips(array);
    }
    else if(image.width == array->width && image.height == array->height)
    {
        gl_texture_array_set(array, layer, 0, 0, image, regenerate_mips);
    }
    else
    {
        LOG_WARN("render", "image of size %d x %d doesnt fit exactly. Resorting back to extending it.", image.width, image.height);

        Arena_Frame arena = scratch_arena_acquire();
        Image temp_storage = {0};
        image_init_sized(&temp_storage, &arena.allocator, array->width, array->height, image.pixel_size, image.type, NULL);

        ASSERT(image.height > 0 && image.width > 0);
        image_copy(&temp_storage, subimage_of(image), 0, 0);
        
        isize stride = image_byte_stride(temp_storage);
        i32 pixel_size = temp_storage.pixel_size;

        //Extend sideways
        for(i32 y = 0; y < image.height; y++)
        {
            u8* curr_row = temp_storage.pixels + stride*y;
            u8* last_pixel = curr_row + pixel_size*(image.width - 1);

            //memset_pattern(curr_row + pixel_size*image.width, (array->width - image.width)*pixel_size, color, pixel_size);
            memset_pattern(curr_row + pixel_size*image.width, (array->width - image.width)*pixel_size, last_pixel, pixel_size);
        }

        //Extend downwards
        u8* last_row = temp_storage.pixels + stride*(image.height - 1);
        for(i32 y = image.height; y < temp_storage.height; y++)
        {
            u8* curr_row = temp_storage.pixels + stride*y;
            memmove(curr_row, last_row, stride);
        }
        
        //memset_pattern(temp_storage->pixels, image_all_pixels_size(*temp_storage), color, sizeof(color));
        gl_texture_array_set(array, layer, 0, 0, temp_storage, regenerate_mips);

        arena_frame_release(&arena);
    }

    return state;
}

#include "name.h"

typedef struct Render_Texture_Layer {
    i32 layer;
    i32 resolution_index;
} Render_Texture_Layer;

typedef struct Render_Texture_Layer_Info {
    Name name;
    i32 used_width;
    i32 used_height;
    bool is_used;
    bool _padding[7];
} Render_Texture_Layer_Info;

typedef Array(Render_Texture_Layer_Info) Render_Texture_Layer_Info_Array;

typedef struct Render_Texture_Resolution {
    GL_Texture_Array array;
    Render_Texture_Layer_Info_Array layers;
    i32 used_layers;
    i32 grow_by;
    
} Render_Texture_Resolution;

typedef Array(Render_Texture_Resolution) Render_Texture_Resolution_Array;

//@TODO: what to do with outliers?
//@TODO: what to do with different formats?
//@TODO: rework images so that they have capacity! Add rehsape!

typedef struct Render_Texture_Manager {
    Render_Texture_Resolution_Array resolutions;
    Allocator* allocator;

    isize memory_used;
    isize memory_budget;
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
        gl_texture_array_generate_mips(&res->array);
    }
}

isize render_texture_manager_add_resolution(Render_Texture_Manager* manager, i32 width, i32 height, i32 layers, i32 grow_by, Pixel_Type type, i32 channel_count)
{
    LOG_INFO("render", "adding texture resolution: " "%d x %d x %d : %s x %d", width, height, layers, pixel_type_name(type), channel_count);
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
    isize needed_size = (isize) width * height * layers * channel_count * pixel_type_size(type);
    if(manager->memory_used + needed_size > manager->memory_budget)
        LOG_ERROR(">render", "Out of texture memory! Using %s / %s (%lli%%)", format_bytes(manager->memory_used, 0).data, format_bytes(manager->memory_budget, 0).data, (lli) manager->memory_used * 100 / manager->memory_budget);
    else if(layers == 0 || width == 0 || height == 0)
        LOG_ERROR(">render", "Zero sized!");
    else
    {
        log_indent();
        if(gl_texture_array_init(&resolution.array, NULL) == false)
            LOG_ERROR("render", "Creation failed!");
        else
        {
            array_init(&resolution.layers, manager->allocator);
            array_resize(&resolution.layers, layers);
            out = manager->resolutions.size;
            array_push(&manager->resolutions, resolution);
            manager->memory_used  += needed_size;
        }
        log_outdent();
    }

    return out;
}

void render_texture_manager_deinit(Render_Texture_Manager* manager)
{
    //@TODO
    ASSERT(false);
    //for(isize i = 0; i < manager->resolutions.size; i++)
    //{
    //    Render_Texture_Resolution* res = &manager->resolutions.data[i];
    //    gl_texture_array_generate_mips(&res->array);
    //}

    memset(manager, 0, sizeof *manager);
}

void render_texture_manager_init(Render_Texture_Manager* manager, Allocator* allocator, isize memory_budget)
{
    //render_texture_manager_deinit(manager);
    if(allocator == NULL) 
        allocator = allocator_get_default();
        
    manager->allocator = allocator;
    manager->memory_budget = memory_budget;
    array_init(&manager->resolutions, allocator);
}

void render_texture_manager_add_default_resolutions(Render_Texture_Manager* manager, f64 fraction_of_remaining_memory_budget)
{
    isize remaining_budget = manager->memory_budget - manager->memory_used;
    LOG_INFO("render", "Adding default resolutions for %lf of remainign memory budget (%s) Using %s / %s", 
        fraction_of_remaining_memory_budget, format_bytes(remaining_budget, 0).data, format_bytes(manager->memory_used, 0).data, format_bytes(manager->memory_budget, 0).data);

    Pixel_Type pixel_type = PIXEL_TYPE_U8;
    i32 channel_counts[4] = {1, 2, 3, 0};

    //These act like proportions rather than final counts because we still have the memory budget to worry about!
    i32 layer_counts[20] = {0};
    layer_counts[int_log2_lower_bound(32)] = 128;
    layer_counts[int_log2_lower_bound(64)] = 128;
    layer_counts[int_log2_lower_bound(128)] = 128;
    layer_counts[int_log2_lower_bound(256)] = 128;
    layer_counts[int_log2_lower_bound(512)] = 32;
    layer_counts[int_log2_lower_bound(1024)] = 32;
    layer_counts[int_log2_lower_bound(2048)] = 16;
    layer_counts[int_log2_lower_bound(4096)] = 8;

    isize combined_layer_size = 0;
    for(isize i = 0; i < ARRAY_SIZE(layer_counts); i++)
    {
        i32 size = 1 << i;
        i32 layers = layer_counts[i];
        combined_layer_size += size*size*layers;
    }
    
    isize combined_channel_count = 0;
    for(isize i = 0; i < ARRAY_SIZE(channel_counts); i++)
        combined_channel_count += channel_counts[i];

    isize ideal_total_size = combined_channel_count * combined_layer_size * pixel_type_size(pixel_type);
    isize desired_total_size = (isize) (remaining_budget * fraction_of_remaining_memory_budget);
    
    f64 scaling_factor = (f64) desired_total_size / (f64) ideal_total_size;
    
    log_indent();
    isize combined_size = 0;
    for(i32 j = 0; j < ARRAY_SIZE(channel_counts); j++)
    {
        for(i32 i = 0; i < ARRAY_SIZE(layer_counts); i++)
        {
            i32 size = 1 << i;
            i32 channels = channel_counts[j];
            i32 layers = layer_counts[i];
            i32 scaled_layers = (i32) ((f64) layers * scaling_factor);
            if(scaled_layers == 0 && layers != 0)
            {
                LOG_WARN("render", "skipped resolution %d x %d because number of layers got rounded to zero!", size, size);
            }

            if(layers && channels)
            {
                i32 grow_by = layers / 8;
            
                i32 resolution_bytes_size = size*size*scaled_layers*channels;
                combined_size += resolution_bytes_size;
            

                render_texture_manager_add_resolution(manager, size, size, scaled_layers, grow_by, PIXEL_TYPE_U8, channels);
            }
        }
    }
    log_outdent();

    LOG_WARN("render", "using %s combined RAM on textures", format_bytes(combined_size, 0).data);
}

typedef enum Found_Type{
    NOT_FOUND = 0,
    FOUND_EXACT,
    FOUND_APPROXIMATE,
    FOUND_APPROXIMATE_BAD_FORMAT,
} Found_Type;

Render_Texture_Layer render_texture_manager_find(Render_Texture_Manager* manager, Found_Type* found_type_or_null, i32 width, i32 height, Pixel_Type type, i32 channel_count, bool used)
{
    PROFILE_START(render_texture_manager_find_counter);
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
            GL_Texture_Array* array = &resolution->array;
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

            ASSERT(out.layer != -1, "The resolution->used_layers must be wrong!");
            state = true;
        }
    }

    if(found_type_or_null)
        *found_type_or_null = found_type;

    PROFILE_END(render_texture_manager_find_counter);
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

Render_Texture_Layer render_texture_manager_add(Render_Texture_Manager* manager, Image image, String name)
{
    Found_Type found_type = NOT_FOUND;
    Render_Texture_Layer empty_slot = render_texture_manager_find(manager, &found_type, image.width, image.height, (Pixel_Type) image.type, image_channel_count(image), false);

    LOG_INFO("render", "adding texture %d x %d : %s x %d", image.width, image.height, pixel_type_name(image.type), image_channel_count(image));

    if(empty_slot.resolution_index <= 0)
        LOG_ERROR(">render", "render_texture_manager_add() Unable to find empty slot! ");
    else
    {
        Render_Texture_Resolution* resolution = &manager->resolutions.data[empty_slot.resolution_index - 1];
        LOG_DEBUG(">render", "added to resolution #%d layer #%d", empty_slot.resolution_index, empty_slot.layer + 1);

        if(found_type == FOUND_APPROXIMATE)
        {
            LOG_WARN(">render", "Found only approximate match!");
            LOG_WARN(">render", "resolution %d x %d : %s x %d", resolution->array.width, resolution->array.height, pixel_type_name(resolution->array.type), resolution->array.channel_count);
        }
            
        if(found_type == FOUND_APPROXIMATE_BAD_FORMAT)
        {
            LOG_WARN(">render", "Found only approximate match with wrong format!");
            LOG_WARN(">render", "resolution %d x %d : %s x %d", resolution->array.width, resolution->array.height, pixel_type_name(resolution->array.type), resolution->array.channel_count);
        }

        resolution->used_layers += 1;

        Render_Texture_Layer_Info* layer_info = &resolution->layers.data[empty_slot.layer];
        layer_info->is_used = true;
        layer_info->used_width = image.width;
        layer_info->used_height = image.height;
        layer_info->name = name_make(name);

        bool fill_state = gl_texture_array_fill_layer(&resolution->array, empty_slot.layer, image, false);
        ASSERT(fill_state);
    }

    return empty_slot;
}

//@TODO: once its needed use this to abstract away.
typedef struct Vertex_Attribute {
    Name name;
    i32 offset;
    i32 size;
    i32 index;
    i32 instance_divisor; //if 0 assume not instanced
    Pixel_Type type;
    u32 _padding;
} Vertex_Attribute;

//i32 vertex_size;
//i32 index_size;
//Vertex_Index_Type index_type;

typedef enum Vertex_Index_Type {
    VERTEX_INDEX_TYPE_I32 = 0,
    VERTEX_INDEX_TYPE_I16,

    VERTEX_INDEX_TYPE_U32,
    VERTEX_INDEX_TYPE_U16,
} Vertex_Index_Type;




typedef struct Render_Info {
    Name name;
    //Name path;
    Id id;
    i64 created_etime;
    i64 last_modified_etime;
    i64 generation;
} Render_Info;

Render_Info render_info_make(String name)
{
    ASSERT(name.size > 0);
    Render_Info out = {0};
    out.name = name_make(name);
    out.id = id_generate();
    out.created_etime = platform_epoch_time();
    return out;
}

void render_info_modify(Render_Info* info)
{
    info->last_modified_etime = platform_epoch_time();
    info->generation += 1;
}

typedef struct Render_Geometry_Batch_Index {
    i32 batch_index;
    i32 index_from;
    i32 index_count;
    
    i32 vertex_from;
    i32 vertex_count;
} Render_Geometry_Batch_Index;

typedef struct Render_Geometry_Batch_Group {
    Name name;

    i32 index_from;
    i32 index_count;
    
    i32 vertex_from;
    i32 vertex_count;
} Render_Geometry_Batch_Group;

typedef Array(Render_Geometry_Batch_Group) Render_Batch_Group_Info_Array;


typedef struct Render_Geometry_Batch{
    GLuint vertex_array_handle;
    GLuint vertex_buffer_handle;
    GLuint index_buffer_handle;

    i32 vertex_count;
    i32 index_count;

    i32 used_vertex_count;
    i32 used_index_count;

    i32 num_vertex_attributes;

    #if 0
    #define MAX_VERTEX_ATTRIBUTES 128
    Vertex_Attribute attributes[MAX_VERTEX_ATTRIBUTES];
    Vertex_Index_Type vertex_type;

    i32 vertex_size;
    i32 index_size;

    i32 vertex_count;
    i32 index_count;
    #endif
    Render_Batch_Group_Info_Array groups;
} Render_Geometry_Batch;

typedef Array(Render_Geometry_Batch) Render_Geometry_Batch_Array;

void render_geometry_batch_init(Render_Geometry_Batch* mesh, Allocator* alloc, isize vertex_count, isize index_count, GLuint instance_buffer)
{
    memset(mesh, 0, sizeof* mesh);

    array_init(&mesh->groups, alloc);

    glGenVertexArrays(1, &mesh->vertex_array_handle);
    glGenBuffers(1, &mesh->vertex_buffer_handle);
    glGenBuffers(1, &mesh->index_buffer_handle);
  
    glBindVertexArray(mesh->vertex_array_handle);
    glBindBuffer(GL_ARRAY_BUFFER, mesh->vertex_buffer_handle);
    glBufferData(GL_ARRAY_BUFFER, vertex_count * sizeof(Vertex), NULL, GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mesh->index_buffer_handle);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, index_count * sizeof(i32), NULL, GL_STATIC_DRAW);
    
    i32 i = 0;
    i++; glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, pos));
    i++; glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, uv));
    i++; glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, norm));
    i++; glVertexAttribPointer(3, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, tan));
    
    glEnableVertexAttribArray(0);	
    glEnableVertexAttribArray(1);	
    glEnableVertexAttribArray(2);	
    glEnableVertexAttribArray(4);	

    if(instance_buffer)
    {
        enum {model_slot = 4};

        glBindVertexArray(mesh->vertex_array_handle);
        glBindBuffer(GL_ARRAY_BUFFER, instance_buffer);
        i++; glVertexAttribPointer(model_slot + 0, 4, GL_FLOAT, GL_FALSE, sizeof(Mat4), (void*)offsetof(Mat4, col[0]));
        i++; glVertexAttribPointer(model_slot + 1, 4, GL_FLOAT, GL_FALSE, sizeof(Mat4), (void*)offsetof(Mat4, col[1]));
        i++; glVertexAttribPointer(model_slot + 2, 4, GL_FLOAT, GL_FALSE, sizeof(Mat4), (void*)offsetof(Mat4, col[2]));
        i++; glVertexAttribPointer(model_slot + 3, 4, GL_FLOAT, GL_FALSE, sizeof(Mat4), (void*)offsetof(Mat4, col[3]));

        glEnableVertexAttribArray(model_slot + 0);	
        glEnableVertexAttribArray(model_slot + 1);	
        glEnableVertexAttribArray(model_slot + 2);	
        glEnableVertexAttribArray(model_slot + 3);	
    
        glVertexAttribDivisor(model_slot + 0, 1);  
        glVertexAttribDivisor(model_slot + 1, 1);  
        glVertexAttribDivisor(model_slot + 2, 1);  
        glVertexAttribDivisor(model_slot + 3, 1);  
    }

    mesh->num_vertex_attributes = i;
    mesh->vertex_count = (i32) vertex_count;
    mesh->index_count = (i32) index_count;

    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    gl_check_error();
}

void render_geometry_batch_set_data(Render_Geometry_Batch* mesh, const Vertex vertices[], i32 vertex_count, i32 vertex_offset, const i32 indices[], i32 index_count, i32 index_offset)
{
    glNamedBufferSubData(mesh->vertex_buffer_handle, sizeof(Vertex)*vertex_offset, sizeof(Vertex)*vertex_count, vertices);
    glNamedBufferSubData(mesh->index_buffer_handle, sizeof(i32)*index_offset, sizeof(i32)*index_count, indices);
}

typedef struct GL_Buffer {
    GLuint handle;
    i32 item_size;
    i32 item_count;
    bool is_static_draw;
    bool _padding[3];
    isize byte_size;
} GL_Buffer;

GL_Buffer gl_buffer_make(isize item_size, isize item_count, const void* data_or_null, bool is_static_draw)
{
    GL_Buffer out = {0};
    out.item_size = (i32) item_size;
    out.item_count = (i32) item_count;
    out.is_static_draw = is_static_draw;
    out.byte_size = item_size*item_count;

    glCreateBuffers(1, &out.handle);
    glNamedBufferData(out.handle, out.byte_size, data_or_null, is_static_draw ? GL_STATIC_DRAW : GL_DYNAMIC_DRAW);

    return out;
}

typedef struct Render_Geometry_Manager {
    Render_Geometry_Batch_Array batches;

    GL_Buffer* instance_buffer;

    i32 def_batch_vertex_size;
    i32 def_batch_index_size;
    
    i32 outlier_vertex_size;
    i32 outlier_index_size;

    isize used_memory;
    isize memory_limit;

    Allocator* allocator;
} Render_Geometry_Manager;

void render_geometry_manager_init(Render_Geometry_Manager* manager, Allocator* alloc, GL_Buffer* instance_buffer, isize memory_limit)
{
    manager->allocator = alloc ? alloc : allocator_get_default();
    array_init(&manager->batches, manager->allocator);

    manager->def_batch_index_size = 1000*1000;
    manager->def_batch_vertex_size = 100*1000;

    manager->outlier_index_size = manager->def_batch_index_size * 4/3;
    manager->outlier_vertex_size = manager->def_batch_vertex_size * 4/3;

    manager->instance_buffer = instance_buffer;

    manager->memory_limit = memory_limit;
}

Render_Geometry_Batch_Index render_geometry_manager_find(Render_Geometry_Manager* manager, String name)
{
    Render_Geometry_Batch_Index out = {0};
    Name n = name_make(name);
    for(isize k = 0; k < manager->batches.size; k++)
    {
        Render_Geometry_Batch* batch = &manager->batches.data[k];
        for(isize i = 0; i < batch->groups.size; i++)
        {
            Render_Geometry_Batch_Group* group = &batch->groups.data[i];
            if(name_is_equal(&group->name, &n))
            {
                out.index_from = group->index_from;
                out.index_count = group->index_count;
                out.vertex_from = group->vertex_from;
                out.vertex_count = group->vertex_count;
                out.batch_index = (i32) k + 1;
                goto function_end;
            }
        }
    }

    function_end:
    return out;
}

i32 render_geometry_manager_add_batch(Render_Geometry_Manager* manager, isize min_vertex_count, isize min_index_count)
{
    isize vertex_count = MAX(manager->def_batch_vertex_size, min_vertex_count);
    isize index_count = MAX(manager->def_batch_index_size, min_index_count);

    isize memory_requirement = min_vertex_count * sizeof(Vertex) + min_index_count * sizeof(i32);

    if(manager->used_memory >= manager->memory_limit * 3/4)
        LOG_WARN("render", "Geometry manager nearly out of memory. Using %s out of %s", format_bytes(manager->used_memory, 0).data, format_bytes(manager->memory_limit, 0).data);
    
    i32 out = 0;
    if(memory_requirement + manager->used_memory <= manager->memory_limit)
    {
        LOG_INFO("render", "creating new geometry batch %i:%i", (int) vertex_count, (int) index_count);
        Render_Geometry_Batch batch = {0};
        render_geometry_batch_init(&batch, manager->allocator, vertex_count, index_count, manager->instance_buffer->handle);
        manager->used_memory += memory_requirement;

        array_push(&manager->batches, batch);
        out = (i32) manager->batches.size;
    }
    else
    {
        LOG_ERROR("render", "Not enough memory");
    }
    return out;
}

Render_Geometry_Batch_Index render_geometry_manager_add(Render_Geometry_Manager* manager, const Vertex vertices[], isize vertex_count, const i32 indices[], isize index_count, String name)
{
    

    i32 batch_index = 0;
    for(isize i = 0; i < manager->batches.size; i++)
    {
        Render_Geometry_Batch* batch = &manager->batches.data[i];
        if(batch->used_index_count + index_count <= batch->index_count && batch->used_vertex_count + vertex_count <= batch->vertex_count)
        {
            batch_index = (i32) i + 1;
            break;
        }
    }
    
    if(batch_index == 0)
    {
        log_indent();
        batch_index = render_geometry_manager_add_batch(manager, vertex_count, index_count);
        log_outdent();
    }

    Render_Geometry_Batch_Index out = {0};
    if(batch_index)
    {
        Render_Geometry_Batch* batch = &manager->batches.data[batch_index - 1];
        Render_Geometry_Batch_Group group = {0};
        group.index_from = batch->used_index_count;
        group.index_count = (i32) index_count;
        
        group.vertex_from = batch->used_vertex_count;
        group.vertex_count = (i32) vertex_count;

        group.name = name_make(name);

        batch->used_index_count += (i32) index_count;
        batch->used_vertex_count += (i32) vertex_count;

        render_geometry_batch_set_data(batch, vertices, group.vertex_count, group.vertex_from, indices, group.index_count, group.index_from);

        out.index_from = group.index_from;
        out.index_count = group.index_count;
        out.vertex_from = group.vertex_from;
        out.vertex_count = group.vertex_count;
        out.batch_index = batch_index;
        LOG_INFO("render", "render_geometry_manager_add() '%s' %i:%i (vertex:index) added to batch #%i", cstring_ephemeral(name), (int) vertex_count, (int) index_count, (int) out.batch_index);

        array_push(&batch->groups, group);
    }
    else
    {
        LOG_ERROR("render", "render_geometry_manager_add() '%s' %i:%i (vertex:index) failed", cstring_ephemeral(name), (int) vertex_count, (int) index_count);
    }

    return out;
}




#define DEFINE_RENDER_PTR_TYPE(Type) \
    typedef struct Type ## _Ptr { \
        Id id; \
        Type* ptr; \
    } Type ## _Ptr \

    
#define MAX_TEXTURES_PER_MATERIAL 32



#define MAX_LIGHTS 32
#define MAX_RESOULTIONS 7

typedef struct Render_Geometry {
    Render_Info info;
    Render_Geometry_Batch_Index group;
    u32 _padding;
} Render_Geometry;

typedef struct Render_Texture {
    Render_Info info;
    Render_Texture_Layer layer;
} Render_Texture;
DEFINE_RENDER_PTR_TYPE(Render_Texture);

typedef struct Render_Material {
    Render_Info info;
    Render_Texture_Ptr textures[MAX_TEXTURES_PER_MATERIAL];
    u8 used_textures;
    u8 _padding[3];

    Vec3 diffuse_color;
    Vec3 specular_color;
    Vec3 ambient_color;
    f32 specular_exponent;
    f32 metallic;
} Render_Material;

typedef struct {
    Vec3 pos;
    Vec3 color;
    float range;
    float radius;
} Render_Light;

typedef struct Render_Environment {
    Render_Info info;
} Render_Environment;

DEFINE_RENDER_PTR_TYPE(Render_Info);
DEFINE_RENDER_PTR_TYPE(Render_Material);
DEFINE_RENDER_PTR_TYPE(Render_Geometry);
DEFINE_RENDER_PTR_TYPE(Render_Environment);

typedef struct Render_Phong_Command {
    Mat4 transform;
    Render_Material_Ptr material;
    Render_Geometry_Ptr geometry;
    Render_Environment_Ptr environment;
} Render_Phong_Command;

typedef Array(Render_Phong_Command) Render_Phong_Command_Array;

typedef struct Render_Command_Expanded {
    Render_Material* material;
    Render_Geometry* geometry;
    Render_Environment* environment;
    i32 transform_index;
    //Is used in the primary phase of sorting so
    // this saves us one ptr deref per iteration
    i32 geometry_batch_index;
} Render_Command_Expanded;

typedef Array(Render_Geometry_Ptr) Render_Geometry_Ptr_Array;
typedef Array(Render_Material_Ptr) Render_Material_Ptr_Array;
typedef Array(Render_Environment_Ptr) Render_Environment_Ptr_Array;
typedef Array(Render_Command_Expanded) Render_Command_Expanded_Array;
typedef Array(Mat4) Mat4_Array;

#define COMMAND_BUFFER_TRANSFORM_BIT 1
#define COMMAND_BUFFER_GEOMETRY_BIT 2
#define COMMAND_BUFFER_MATERIAL_BIT 4
#define COMMAND_BUFFER_ENVIRONMENT_BIT 8

#define DO_MONO_RENDER_QUEUE
//#define DO_MONO_EXPANDED_QUEUE

typedef struct Render_Queue {
    u8_Array masks;
    Mat4_Array transforms;
    Render_Geometry_Ptr_Array geometries;
    Render_Material_Ptr_Array materials;
    Render_Environment_Ptr_Array environments;
    Render_Command_Expanded_Array expanded;

    Render_Phong_Command_Array mono_queue;
} Render_Queue;

typedef Render_Shader GL_Shader;


enum {MAX_TEXTURE_SLOTS = 32};




// We render a single BATCH at a time.
// A batch has multiple DRAWS (different geometries and/or materials).
// A draw has multiple INSTANCES (different positions/transforms of a single model).
// 
// These are the rules for this style of opengl renderer:
// 
// BATCH
// 1) Each shader and its own batch.
// 2) Each environemnt requires its own batch 
//    (environment is defined as data shared across batched entries)
// 3) Each vertex buffer (VAO) requires itw own bacth
// 3) There can be at most MAX_TEXTURE_SLOTS different texture arrays bound within single batch
// 
// DRAW
// 1) Each range of vertices/indices within the vertex buffer requires its own draw
// 2) Each different binding set of individual textures (ie material) requires its own draw
//    (we simply change draw on every material change)
// 
// INSTANCE
// 1) There may be unlimited number of instances within a single draw.
    
//Each batch can only have MAX_TEXTURE_SLOTS texture array slots.


//Each draw must track where it bound its textures to
typedef struct Render_Per_Draw {
    i32 instance_from;
    u32 _padding;
    Render_Material* material;
    Render_Geometry* geometry;

    Render_Texture_Layer bound_textures[MAX_TEXTURE_SLOTS];
} Render_Per_Draw;

typedef struct Render_Per_Instance {
    Mat4 model;
} Render_Per_Instance;

typedef Array(Render_Per_Draw) Render_Per_Draw_Array;
typedef Array(Render_Per_Instance) Render_Per_Instance_Array;

typedef  struct {
    GLuint count;
    GLuint instance_count;
    GLuint first_index;
    GLint  base_vertex;
    GLuint base_instance;
} Gl_Draw_Elements_Indirect_Command;

typedef Array(Gl_Draw_Elements_Indirect_Command) Gl_Draw_Elements_Indirect_Command_Array;

typedef struct  {
    ATTRIBUTE_ALIGNED(16) Mat4 model;
} Blinn_Phong_Per_Instance;

typedef struct Blinn_Phong_Per_Draw {
    ATTRIBUTE_ALIGNED(16) Vec4 diffuse_color;
    ATTRIBUTE_ALIGNED(16) Vec4 ambient_color;
    ATTRIBUTE_ALIGNED(16) Vec4 specular_color;
    ATTRIBUTE_ALIGNED(4) float specular_exponent;
    ATTRIBUTE_ALIGNED(4) float metallic;
    ATTRIBUTE_ALIGNED(4) int map_diffuse; 
    ATTRIBUTE_ALIGNED(4) int map_specular; 
    ATTRIBUTE_ALIGNED(4) int map_normal; 
    ATTRIBUTE_ALIGNED(4) int map_ambient; 
    ATTRIBUTE_ALIGNED(4) int _padding[2]; 
} Blinn_Phong_Per_Draw;

typedef struct Blinn_Phong_Light {
    ATTRIBUTE_ALIGNED(16) Vec4 pos_and_range;
    ATTRIBUTE_ALIGNED(16) Vec4 color_and_radius;
} Blinn_Phong_Light;

typedef struct Blinn_Phong_Per_Batch {
    ATTRIBUTE_ALIGNED(16) Mat4 projection;
    ATTRIBUTE_ALIGNED(16) Mat4 view;
    ATTRIBUTE_ALIGNED(16) Vec4 view_pos;
    ATTRIBUTE_ALIGNED(16) Vec4 base_illumination;

    ATTRIBUTE_ALIGNED(4) float light_linear_attentuation;
    ATTRIBUTE_ALIGNED(4) float light_quadratic_attentuation;
    ATTRIBUTE_ALIGNED(4) float gamma;
    ATTRIBUTE_ALIGNED(4) int lights_count;

    ATTRIBUTE_ALIGNED(16) Blinn_Phong_Light lights[MAX_LIGHTS];
} Blinn_Phong_Per_Batch;

typedef Array(Blinn_Phong_Per_Instance) Blinn_Phong_Per_Instance_Array;
typedef Array(Blinn_Phong_Per_Draw) Blinn_Phong_Per_Draw_Array;
typedef Array(Blinn_Phong_Per_Batch) Blinn_Phong_Per_Batch_Array;

//The main idea of this new rendere is to use the incredibly efficient
// command buffer instanced calls (see below) to drastically reduce the number of draw calls,
// while keeping as much flexibility as possible (ie not generting texture atlasses etc). 
// 
// We do this by keeping geometry in 'batches'. A batch is a single GL buffer 
// containing aggragate of multiple different meshes or submeshes (groups in OBJ format - sponza has about a hundred). 
// However a single model can be at most in one batch (ie we never split because too big).
// The command buffer commands can dispatch any number of sub ranges of indices (so meshes/groups in out batch) across any 
// number of instances within single draw call. (So we can draw just the pot and plant from sponza.obj 100x at once!)
// 
// To use this we need:
// 1) To group geometry into batches. 
//      This is done by putting each object into its separate batch if big enough or appending to previously allocated batch
//      untill big enough.
// 
// 2) To sort and group draw requests by shader/batch/obejct and then transform into the Gl_Draw_Elements_Indirect_Command structure.
//      We simply sort.
// 
// 3) To set material 'uniforms' per model 
//      We make a uniform buffer where instead of single field for each uniform we have an array of BLINN_PHONG_NUM_BATCH_ENTRIES.
//      We then read at gl_DrawID to obtain our 'uniforms'
// 
// 4) To set samplers per model 
//      Here are two solutions: 
//          1 - use "bindless textures" and store them just like above - this is probably the more optimal solution
//              and its call overhead is esentially zero
//          2 - Have an array of sampler uniforms but since we are limited by the number of samplers we 'allocate' these slots
//              so that we place indices to the appropriate maps into the uniform buffer. This requires fancy code and might not be
//              worth doing.
//
//
//Gl_Draw_Elements_Indirect_Command can be understood as doing the following 
// in a hypothetical opengl implementation:
#if 0
for(int draw_i = 0; draw_i < command_count; draw_i++)
{
    Gl_Draw_Elements_Indirect_Command c = commands[draw_i];
    for(int instance_i = 0; instance_i < c.instance_count; instance_i++)
    {
        for(int i = c.indices_from; i < c.indices_from + c.indices_count; i++)
        {
            GLuint gl_VertexID = indices[i] + c.base_vertex;
            GLuint gl_InstanceID = instance_i + c.base_instance;
            GLuint gl_DrawID = draw_i;
 
            GLuint gl_BaseVertex = c.base_vertex;
            GLuint gl_BaseInstance = c.base_instance;
 
            //... fire vertext shader and render ...
        }
    }
}
#endif

typedef struct Render {
    GL_Shader shader_blinn_phong;
    
    GL_Buffer buffer_command;
    GL_Buffer buffer_environment_uniform;
    GL_Buffer buffer_draw_uniform;
    GL_Buffer buffer_instance;

    //For debug mostly
    GL_Shader_Block_Info uniform_block_environment;
    GL_Shader_Block_Info uniform_block_draw;

    Blinn_Phong_Per_Instance_Array blinn_phong_per_instance;
    Blinn_Phong_Per_Draw_Array blinn_phong_per_draw;

    Render_Per_Instance_Array render_per_instance;
    Render_Per_Draw_Array render_per_draw;

    Gl_Draw_Elements_Indirect_Command_Array indirect_draws;

    Render_Texture_Manager texture_manager;
    Render_Geometry_Manager geometry_manager;
    Render_Queue render_queue;

    Stable_Array textures;
    Stable_Array geometries;
    Stable_Array materials;

    Allocator* allocator;
} Render;

Render_Texture* render_texture_get(Render* render, Render_Texture_Ptr ptr);
Render_Geometry* render_geometry_get(Render* render, Render_Geometry_Ptr ptr);
Render_Material* render_material_get(Render* render, Render_Material_Ptr ptr);
Render_Environment* render_environment_get(Render* render, Render_Environment_Ptr ptr);


Render_Environment* render_environment_get(Render* render, Render_Environment_Ptr ptr)
{
    (void) render, ptr;
    return (Render_Environment*) 1;
}

Render_Texture_Ptr render_texture_find(Render* render, String name, Id id);
Render_Geometry_Ptr render_geometry_find(Render* render, String name, Id id);
Render_Material_Ptr render_material_find(Render* render, String name, Id id);
Render_Environment render_environment_find(Render* render, String name, Id id);

void render_queue_init(Render_Queue* buffers, Allocator* allocator, isize total_byte_size)
{
    allocator = allocator ? allocator : allocator_get_default();

    isize command_bytes = (isize) (total_byte_size * 0.75);
    isize material_bytes = (isize) (total_byte_size * 0.10);
    isize geometry_bytes = (isize) (total_byte_size * 0.10);
    isize environemnt_bytes = total_byte_size - command_bytes - material_bytes - geometry_bytes; // * 0.05

    isize command_cound = command_bytes / (sizeof(Mat4) + sizeof(u8) + sizeof(Render_Command_Expanded));
    isize material_count = material_bytes / sizeof(Render_Material);
    isize geometry_count = geometry_bytes / sizeof(Render_Geometry);
    isize environemnt_count = environemnt_bytes / sizeof(Render_Environment);

    //If you dont want these arrays to grow you can give failing allocator.
    array_init_with_capacity(&buffers->masks, allocator, command_cound);
    array_init_with_capacity(&buffers->transforms, allocator, command_cound);
    array_init_with_capacity(&buffers->expanded, allocator, command_cound);
    array_init_with_capacity(&buffers->materials, allocator, material_count);
    array_init_with_capacity(&buffers->geometries, allocator, geometry_count);
    array_init_with_capacity(&buffers->environments, allocator, environemnt_count);
    array_init_with_capacity(&buffers->mono_queue, allocator, command_cound);
}

void render_queue_submit_phong(Render* render, const Render_Phong_Command* command)
{
    Render_Queue* buffers = &render->render_queue;
    PROFILE_START(render_queue_submit);
    
    //#if defined(DO_MONO_RENDER_QUEUE)

    //array_push(&buffers->mono_queue, *command);

    #if defined(DO_MONO_EXPANDED_QUEUE)

    Render_Geometry* geometry = render_geometry_get(render, command->geometry);
    Render_Material* material = render_material_get(render, command->material);
    Render_Environment* environment = render_environment_get(render, command->environment);
    
    if(geometry && material && environment)
    {
        i32 geometry_batch_index = geometry->group.batch_index;
        
        Render_Command_Expanded expanded = {0};
        expanded.geometry = geometry;
        expanded.material = material;
        expanded.environment = environment;
        expanded.transform_index = (i32) buffers->transforms.size;
        expanded.geometry_batch_index = geometry_batch_index;

        array_push(&buffers->transforms, command->transform);
        array_push(&buffers->expanded, expanded);
    }

    #else
    if(buffers->transforms.size == 0)
    //if(1)
    {
        //If empty just add all
        array_push(&buffers->geometries, command->geometry);
        array_push(&buffers->materials, command->material);
        array_push(&buffers->transforms, command->transform);
        array_push(&buffers->environments, command->environment);
        array_push(&buffers->masks, (u8) -1);
    }
    else
    {
        //If not only ad if chnaged. This greatly reduces the size of our buffers thus speeds
        // the supbsequent expanding and sorting.
        u8 mask = COMMAND_BUFFER_TRANSFORM_BIT;
        Render_Geometry_Ptr* last_geometry = array_last(buffers->geometries);
        Render_Material_Ptr* last_material = array_last(buffers->materials);
        Render_Environment_Ptr* last_environment = array_last(buffers->environments);
        if(last_geometry->id != command->geometry.id)
        {
            array_push(&buffers->geometries, command->geometry);
            mask |= COMMAND_BUFFER_GEOMETRY_BIT;
        }

        if(last_material->id != command->material.id)
        {
            array_push(&buffers->materials, command->material);
            mask |= COMMAND_BUFFER_MATERIAL_BIT;
        }
        
        if(last_environment->id != command->environment.id)
        {
            array_push(&buffers->environments, command->environment);
            mask |= COMMAND_BUFFER_ENVIRONMENT_BIT;
        }

        array_push(&buffers->transforms, command->transform);
        array_push(&buffers->masks, mask);
    }

    #endif
    PROFILE_END(render_queue_submit);
}

void render_queue_expand(Render* render)
{
    PROFILE_START(render_queue_expand);
    Render_Queue* buffers = &render->render_queue;
    array_clear(&buffers->expanded);
    
    i32 skipped_meshes_count = 0;
    
    Render_Geometry* geometry = NULL;
    Render_Material* material = NULL;
    Render_Environment* environment = NULL;

    i32 geometry_cursor = 0;
    i32 material_cursor = 0;
    i32 environment_cursor = 0;
    i32 geometry_batch_index = 0;


    for(isize i = 0; i < buffers->masks.size; i++)
    {
        u8 mask = buffers->masks.data[i];

        if(mask & COMMAND_BUFFER_GEOMETRY_BIT)
        {
            ASSERT(geometry_cursor < buffers->geometries.size);
            Render_Geometry_Ptr geometry_ptr = buffers->geometries.data[geometry_cursor++];
            geometry = render_geometry_get(render, geometry_ptr);
            if(geometry)
            {
                geometry_batch_index = geometry->group.batch_index;
            }
        }
        
        if(mask & COMMAND_BUFFER_MATERIAL_BIT)
        {
            ASSERT(material_cursor < buffers->materials.size);
            Render_Material_Ptr material_ptr = buffers->materials.data[material_cursor++];
            material = render_material_get(render, material_ptr);
        }

        if(mask & COMMAND_BUFFER_ENVIRONMENT_BIT)
        {
            ASSERT(environment_cursor < buffers->environments.size);
            Render_Environment_Ptr environment_ptr = buffers->environments.data[environment_cursor++];
            environment = render_environment_get(render, environment_ptr);
        }

        if(geometry && material && environment)
        {
            Render_Command_Expanded expanded = {0};
            expanded.geometry = geometry;
            expanded.material = material;
            expanded.environment = environment;
            expanded.transform_index = (i32) i;
            expanded.geometry_batch_index = geometry_batch_index;

            array_push(&buffers->expanded, expanded);
        }
        else
        {
            skipped_meshes_count += 1;
        }
    }

    if(skipped_meshes_count > 0)
        LOG_WARN("render", "render_queue_expand detected %i / %i are invalid meshes!", (int) skipped_meshes_count, (int) buffers->masks.size);
    PROFILE_END(render_queue_expand);
}

void render_queue_clear(Render_Queue* buffers)
{
    array_clear(&buffers->masks);
    array_clear(&buffers->transforms);
    array_clear(&buffers->expanded);
    array_clear(&buffers->materials);
    array_clear(&buffers->geometries);
    array_clear(&buffers->environments);
}

typedef struct Render_Memory_Budget {
    isize geometry; 
    isize texture;
    isize instance_buffer;
    isize draw_buffer; 
    isize command_buffer;
} Render_Memory_Budget;

void render_init(Render* render, Allocator* alloc, GL_Shader* blinn_phong, Render_Memory_Budget mem_budget)
{
    render->allocator = alloc ? alloc : allocator_get_default();

    isize max_num_instances = mem_budget.instance_buffer / (sizeof(Blinn_Phong_Per_Instance) + sizeof(Render_Per_Instance));
    isize max_num_draws = mem_budget.draw_buffer / (sizeof(Blinn_Phong_Per_Draw) + sizeof(Render_Per_Draw) + sizeof(Gl_Draw_Elements_Indirect_Command));
    
    array_init_with_capacity(&render->blinn_phong_per_instance, render->allocator, max_num_instances);
    array_init_with_capacity(&render->blinn_phong_per_draw, render->allocator, max_num_draws);
    array_init_with_capacity(&render->render_per_instance, render->allocator, max_num_instances);
    array_init_with_capacity(&render->render_per_draw, render->allocator, max_num_draws);
    array_init_with_capacity(&render->indirect_draws, render->allocator, max_num_draws);
    //Allocator* failing = allocator_get_failing();

    //@TODO: budget?
    stable_array_init(&render->textures, render->allocator, sizeof(Render_Texture));
    stable_array_init(&render->geometries, render->allocator, sizeof(Render_Material));
    stable_array_init(&render->materials, render->allocator, sizeof(Render_Geometry));

    render->buffer_command = gl_buffer_make(sizeof(Gl_Draw_Elements_Indirect_Command), max_num_draws, NULL, false);
    render->buffer_instance = gl_buffer_make(sizeof(Blinn_Phong_Per_Instance), max_num_instances, NULL, false);
    render->buffer_environment_uniform = gl_buffer_make(sizeof(Blinn_Phong_Per_Batch), 1, NULL, false);
    render->buffer_draw_uniform = gl_buffer_make(sizeof(Blinn_Phong_Per_Draw), max_num_draws, NULL, false);

    render_texture_manager_init(&render->texture_manager, render->allocator, mem_budget.texture);
    render_geometry_manager_init(&render->geometry_manager, render->allocator, &render->buffer_instance, mem_budget.geometry);
    render_texture_manager_add_default_resolutions(&render->texture_manager, 1.0f);
    render_queue_init(&render->render_queue, render->allocator, mem_budget.command_buffer);

    render->shader_blinn_phong = *blinn_phong;

    
    render->uniform_block_draw = shader_storage_block_make(render->shader_blinn_phong.shader, "Params", 0, render->buffer_draw_uniform.handle);
    render->uniform_block_environment = uniform_block_make(render->shader_blinn_phong.shader, "Environment", 1, render->buffer_environment_uniform.handle);
}

Render_Info_Ptr _render_resource_find(Stable_Array stable, String name, Id id)
{
    Render_Info_Ptr out = {0};
    Name n = name_make(name);
    //@TODO?
    STABLE_ARRAY_FOR_EACH_BEGIN(stable, Render_Info*, info, isize, index)
        if(info->id == id || name_is_equal(&info->name, &n))
        {
            out.ptr = info;
            out.id = info->id;
            return out;
        }
    STABLE_ARRAY_FOR_EACH_END
    return out;
}

Render_Info* _render_resource_get(Stable_Array stable, Render_Info_Ptr ptr)
{
    if(ptr.ptr != NULL && ptr.ptr->id == ptr.id)
        return ptr.ptr;
    
    //Will happen very very infrequently!
    Render_Info_Ptr found = _render_resource_find(stable, STRING(""), ptr.id);
    if(found.ptr != NULL && found.ptr->id == found.id)
        return found.ptr;

    return NULL;
}



//TEXTURE
Render_Texture_Ptr render_texture_find(Render* render, String name, Id id)
{
    Render_Info_Ptr _out = _render_resource_find(render->textures, name, id);
    Render_Texture_Ptr out = {_out.id, (Render_Texture*) _out.ptr};
    return out;
}

Render_Texture* render_texture_get(Render* render, Render_Texture_Ptr ptr)
{
    Render_Info_Ptr _ptr = {ptr.id, (Render_Info*) ptr.ptr};
    return (Render_Texture*) _render_resource_get(render->textures, _ptr);
}

//GEOMETRY
Render_Geometry_Ptr render_geometry_find(Render* render, String name, Id id)
{
    Render_Info_Ptr _out = _render_resource_find(render->geometries, name, id);
    Render_Geometry_Ptr out = {_out.id, (Render_Geometry*) _out.ptr};
    return out;
}

Render_Geometry* render_geometry_get(Render* render, Render_Geometry_Ptr ptr)
{
    Render_Info_Ptr _ptr = {ptr.id, (Render_Info*) ptr.ptr};
    return (Render_Geometry*) _render_resource_get(render->geometries, _ptr);
}

//MATERIAL
Render_Material_Ptr render_material_find(Render* render, String name, Id id)
{
    Render_Info_Ptr _out = _render_resource_find(render->materials, name, id);
    Render_Material_Ptr out = {_out.id, (Render_Material*) _out.ptr};
    return out;
}

Render_Material* render_material_get(Render* render, Render_Material_Ptr ptr)
{
    Render_Info_Ptr _ptr = {ptr.id, (Render_Info*) ptr.ptr};
    return (Render_Material*) _render_resource_get(render->materials, _ptr);
}


int sign_32(i32 x) 
{
    return (x > 0) - (x < 0);
}

int sign_64(i64 x) 
{
    return (x > 0) - (x < 0);
}

int command_buffer_compare_func(const void* a_, const void* b_)
{
    const Render_Command_Expanded* a = (const Render_Command_Expanded*) a_; 
    const Render_Command_Expanded* b = (const Render_Command_Expanded*) b_; 

    int env_diff = sign_64(a->environment - b->environment);
    int geometry_batch_diff = sign_32(a->geometry_batch_index - b->geometry_batch_index);
    int geometry_diff = sign_64(a->geometry - b->geometry);

    if(env_diff)
        return env_diff;
    if(geometry_batch_diff)
        return geometry_batch_diff;
    if(geometry_diff)
        return geometry_diff;

    return sign_64(a->material - b->material);
}

void render_render(Render* render, Camera camera)
{
    //@TODO get from environment
    Mat4 view = camera_make_view_matrix(camera);
    Mat4 projection = camera_make_projection_matrix(camera);

    Render_Queue* buffers = &render->render_queue;
    
    #if !defined(DO_MONO_EXPANDED_QUEUE)
    render_queue_expand(render);
    #endif

    PROFILE_START(render_queue_sort);
    qsort(buffers->expanded.data, buffers->expanded.size, sizeof *buffers->expanded.data, command_buffer_compare_func);
    PROFILE_END(render_queue_sort);

    glEnable(GL_DEPTH_TEST); 
    glEnable(GL_CULL_FACE);  
    glCullFace(GL_BACK);
    glFrontFace(GL_CCW);
    
    typedef struct Render_Per_Batch {
        i32 texture_slots[MAX_TEXTURE_SLOTS];
        i32 used_texture_slots;

        i32 geometry_batch_index;
        //@TODO
        Render_Environment* environment;
        Render_Shader* shader;
    } Render_Per_Batch;

    Render_Per_Draw_Array* batch_draws = &render->render_per_draw;
    Render_Per_Instance_Array* batch_instances = &render->render_per_instance;

    i32 not_found_textures = 0;
    for(isize j = 0; j < buffers->expanded.size; )
    {
        PROFILE_START(batch_prepare);

        Render_Command_Expanded first = buffers->expanded.data[j];

        Render_Per_Batch batch = {0};
        array_clear(batch_draws);
        array_clear(batch_instances);

        batch.geometry_batch_index = first.geometry->group.batch_index;
        batch.environment = first.environment;
        batch.shader = NULL; //@TODO

        Render_Per_Draw prev_draw = {0};
        isize k = j;
        for(; k < buffers->expanded.size; k++)
        {
            Render_Command_Expanded curr = buffers->expanded.data[k];   
            Mat4 curr_transform = buffers->transforms.data[curr.transform_index];

            //If differs too much end the batch
            if(curr.geometry_batch_index != first.geometry_batch_index || 
                curr.environment != first.environment)
                goto end_batch;
                
            //If draws or isntances would no longer fit into the gpu buffers
            // end the batch.
            if(batch_draws->size >= render->buffer_draw_uniform.item_count
                || batch_instances->size >= render->buffer_instance.item_count)
            {
                ASSERT(batch_draws->size == render->buffer_draw_uniform.item_count || batch_instances->size == render->buffer_instance.item_count);
                goto end_batch;
            }

            bool push_new_draw = false;
            Render_Per_Draw new_draw = {0}; //@TODO: modify directly draw

            //if geoemtries or matterials differ add new draw
            if(curr.material != prev_draw.material)
            {
                push_new_draw = true;

                //We prohibit a single material with too many textures to be rendered!
                isize used_textures = curr.material->used_textures;
                (void) used_textures; //@TODO
                    
                //material changed so we analyze anew its textures
                //Go through all of the materials textures and check if it was added
                //If not add it. If too full end the batch early.
                for(isize tex_i = 0; tex_i < curr.material->used_textures; tex_i ++)
                {
                    Render_Texture_Ptr texture_ptr = curr.material->textures[tex_i];
                    Render_Texture* texture = render_texture_get(render, texture_ptr);
                    if(!texture)
                        not_found_textures += 1;
                    else
                    {
                        i32 resolution_index = texture->layer.resolution_index;
                        ASSERT(resolution_index > 0, "valid textures must have valid res index. index %d", resolution_index);

                        //We use dense hashtable to acelaret the search
                        // Start at the has for the texture (its id can be used as hash)
                        // and iterate unill we have either 
                        // 1) iterated the whole array
                        // 2) found it
                        // 3) found empty index (thus this hash couldnt have been added before)
                        //bool was_texture_found = false;
                        //u64 slot_at = 0;
                        //for(u64 iter = 0; iter < MAX_TEXTURE_SLOTS; iter++)
                        //{
                        //    slot_at = ((u64) texture_ptr.id + iter) % MAX_TEXTURE_SLOTS;
                        //    if(batch.texture_slots[slot_at] == 0)
                        //        break;
                        //    if(batch.texture_slots[slot_at] == resolution_index)
                        //    {
                        //        was_texture_found = true;
                        //        break;
                        //    }
                        //}

                        i32 found = 0;
                        for(i32 iter = 0; iter < batch.used_texture_slots; iter++)
                        {
                            if(batch.texture_slots[iter] == resolution_index)
                            {
                                found = iter + 1;
                                break;
                            }
                        }

                        //If not found add it
                        if(!found)
                        {
                            //If we have too many textures end the batch
                            if(batch.used_texture_slots >= MAX_TEXTURE_SLOTS)
                                goto end_batch;

                            //ASSERT(slot_at < MAX_TEXTURE_SLOTS);
                            //batch.texture_slots[slot_at] = resolution_index;
                            //batch.used_texture_slots += 1;

                            //This is slightly confusing configuration with all the indexes flying about but...
                            // the draw should have its textures in the exact same order as within the material.
                            //Therefor we assign to index tex_i (the index of the current texture)
                            // and link it to the slot_at and appropriate texture layer.
                            //new_draw.compressed_texture_layers[tex_i] = COMPRESS_TEXTURE_LAYER(texture->layer.layer, slot_at);

                            batch.texture_slots[batch.used_texture_slots] = resolution_index;
                            batch.used_texture_slots += 1;
                            found = batch.used_texture_slots;
                        }
                        
                        new_draw.bound_textures[tex_i].layer = texture->layer.layer;
                        new_draw.bound_textures[tex_i].resolution_index = found;
                    }
                }
            }
            else if(curr.geometry != prev_draw.geometry)
            {
                if(curr.material == prev_draw.material)
                    memcpy(new_draw.bound_textures, prev_draw.bound_textures, sizeof prev_draw.bound_textures);
                push_new_draw = true;
            }

            if(batch_draws->size == 0)
                ASSERT(push_new_draw);

            if(push_new_draw)
            {
                new_draw.material = curr.material;
                new_draw.geometry = curr.geometry;
                new_draw.instance_from = (i32) batch_instances->size;
                prev_draw = new_draw;
                array_push(batch_draws, new_draw);
            }

            ASSERT(batch_draws->size > 0);

            Render_Per_Instance instance = {0};
            instance.model = curr_transform;
            array_push(batch_instances, instance);
        }

        end_batch:
        

        //Only now we prepare the OPENGL specific buffers
        //@NOTE: 
        //THE FOLLOWING IS VERY SPECIFIC TO OUR CURRENT SHADERS!
        //Also at this point we could push all finalized batches into a buffer 
        // and then have a separet loop execute all the draw commands. Howver we can use 
        // the fact that gl functions are async to overlap the rendering time with 
        // prepering the next batch draw.

        //@NOTE: we dont currently use blinn_phong_per_instance at all since the ata required is the same as batch_instances.
        //array_resize_for_overwrite(&render->blinn_phong_per_instance, batch_instances);

        array_resize_for_overwrite(&render->blinn_phong_per_draw, batch_draws->size);
        array_resize_for_overwrite(&render->indirect_draws, batch_draws->size);

        ASSERT(render->indirect_draws.size >= batch_draws->size);
        Gl_Draw_Elements_Indirect_Command* indirect_commands = render->indirect_draws.data;
        for(u32 i = 0; i < (u32) batch_draws->size; i++)
        {
            Render_Per_Draw* draw = &batch_draws->data[i];
            Render_Geometry_Batch_Index* geometry = &draw->geometry->group;

            indirect_commands[i].first_index = geometry->index_from;
            indirect_commands[i].count = geometry->index_count;
            indirect_commands[i].base_instance = draw->instance_from;
            indirect_commands[i].base_vertex = geometry->vertex_from;

            //@TODO: this can be solved in some better way.
            if(i != batch_draws->size - 1)
            {
                Render_Per_Draw* next_draw = &batch_draws->data[i + 1];
                indirect_commands[i].instance_count = next_draw->instance_from - draw->instance_from;
            }
            else
            {
                indirect_commands[i].instance_count = (GLuint) (batch_instances->size - draw->instance_from);
            }
        }
                
        
        ASSERT(render->blinn_phong_per_draw.size >= batch_draws->size);
        //Blinn_Phong_Per_Draw* blinn_per_draw = render->blinn_phong_per_draw.data;
        for(isize i = 0; i < batch_draws->size; i++)
        {
            Render_Per_Draw* draw = &batch_draws->data[i];
            Render_Material* material = draw->material;
            Blinn_Phong_Per_Draw* blinn = &render->blinn_phong_per_draw.data[i];
            blinn->diffuse_color = vec4_from_vec3(material->diffuse_color);
            blinn->specular_color = vec4_from_vec3(material->specular_color);
            blinn->ambient_color = vec4_from_vec3(material->ambient_color);
            blinn->specular_exponent = material->specular_exponent;
            blinn->metallic = material->metallic;
            //@TODO: strcuture
            #define COMPRESS_TEXTURE_LAYER(layer, slot_at) (i32) ((u32) (layer) | ((u32) (slot_at) << 16))
            #define COMPRESS_TEXTURE_LAYER2(layer_struct) COMPRESS_TEXTURE_LAYER(layer_struct.layer, layer_struct.resolution_index)

            blinn->map_diffuse =  COMPRESS_TEXTURE_LAYER2(draw->bound_textures[0]);
            blinn->map_specular = COMPRESS_TEXTURE_LAYER2(draw->bound_textures[1]);
        }

        Blinn_Phong_Per_Batch blinn_environment = {0};
        {
            blinn_environment.light_quadratic_attentuation = 0.05f;
            blinn_environment.base_illumination = vec4_of(0.5f);
            blinn_environment.projection = projection; 
            blinn_environment.view = view; 
            blinn_environment.view_pos = vec4_from_vec3(camera.pos);

            blinn_environment.lights_count = 2;
            blinn_environment.lights[0].color_and_radius = vec4(10, 8, 7, 0);
            blinn_environment.lights[0].pos_and_range = vec4(40, 20, -10, 100);
                    
            blinn_environment.lights[1].color_and_radius = vec4(8, 10, 8.5f, 0);
            blinn_environment.lights[1].pos_and_range = vec4(0, 0, 10, 100);
        }
        
        PROFILE_END(batch_prepare);
        
        PROFILE_START(batch_flush);

        
        PROFILE_START(texture_set);
        render_shader_use(&render->shader_blinn_phong);
        GLuint map_array_loc = render_shader_get_uniform_location(&render->shader_blinn_phong, "u_map_resolutions");

        i32 tex_slots[MAX_TEXTURE_SLOTS] = {0};
        for(GLint i = 0; i < MAX_TEXTURE_SLOTS; i++)
        {
            tex_slots[i] = i;
            i32 resolution_index = batch.texture_slots[i];

            if(resolution_index)
            {
                CHECK_BOUNDS(resolution_index - 1, render->texture_manager.resolutions.size);
                GL_Texture_Array* resolution_array = &render->texture_manager.resolutions.data[resolution_index - 1].array;

                glActiveTexture(GL_TEXTURE0 + i);
                glBindTexture(GL_TEXTURE_2D_ARRAY, resolution_array->handle);
            }
        }
        glUniform1iv(map_array_loc, (GLsizei) MAX_TEXTURE_SLOTS, tex_slots);
        PROFILE_END(texture_set);

        ASSERT(sizeof(blinn_environment) <= render->uniform_block_environment.buffer_size);
        //ASSERT(array_byte_size(render->blinn_phong_per_draw) <= render->uniform_block_draw.buffer_size);
        ASSERT(array_byte_size(render->render_per_instance) <= render->buffer_instance.byte_size);
        ASSERT(array_byte_size(render->blinn_phong_per_draw) <= render->buffer_draw_uniform.byte_size);

        ASSERT(sizeof *batch_instances->data == render->buffer_instance.item_size);
        ASSERT(batch_instances->size <= render->buffer_instance.item_count);

        
        PROFILE_START(batch_upload);
        glNamedBufferSubData(render->buffer_environment_uniform.handle, 0, sizeof(blinn_environment), &blinn_environment);
        glNamedBufferSubData(render->buffer_draw_uniform.handle, 0, array_byte_size(render->blinn_phong_per_draw), render->blinn_phong_per_draw.data);
        glNamedBufferSubData(render->buffer_instance.handle, 0, array_byte_size(render->render_per_instance), render->render_per_instance.data);
        //glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, render->buffer_draw_uniform.handle); //@TEMP
        PROFILE_END(batch_upload);

        
        PROFILE_START(draw_call);
        glBindBuffer(GL_DRAW_INDIRECT_BUFFER , render->buffer_command.handle);
        glBufferSubData(GL_DRAW_INDIRECT_BUFFER, 0, batch_draws->size * sizeof *indirect_commands, indirect_commands);


        Render_Geometry_Batch* geometry_batch = &render->geometry_manager.batches.data[batch.geometry_batch_index - 1];
        glBindVertexArray(geometry_batch->vertex_array_handle);
        //glBindBuffer(GL_ARRAY_BUFFER, geometry_batch->vertex_buffer_handle);
        glBindBuffer(GL_ARRAY_BUFFER, render->buffer_instance.handle);
        PROFILE_END(draw_call);

        PROFILE_END(batch_flush);

        glMultiDrawElementsIndirect(GL_TRIANGLES, GL_UNSIGNED_INT, NULL, (u32) batch_draws->size, 0);

        ASSERT(k > j, "Must make progress k:%i > i:%i", (int) k, (int) j);
        j = k;
    }


    render_queue_clear(&render->render_queue);
}

Render_Texture_Ptr render_texture_add(Render* render, Image image, String name)
{
    Render_Texture texture = {0};
    texture.info = render_info_make(name);
    texture.layer = render_texture_manager_add(&render->texture_manager, image, name);

    Render_Texture_Ptr out = {0};
    out.id = texture.info.id;
    stable_array_insert(&render->textures, (void**) &out.ptr);
    *out.ptr = texture;
    return out;
}

Render_Geometry_Ptr render_geometry_add(Render* render, const Vertex vertices[], isize vertex_count, const i32 indices[], isize index_count, String name)
{
    Render_Geometry geometry = {0};
    geometry.info = render_info_make(name);
    geometry.group = render_geometry_manager_add(&render->geometry_manager, vertices, vertex_count, indices, index_count, name);

    Render_Geometry_Ptr out = {0};
    out.id = geometry.info.id;
    stable_array_insert(&render->geometries, (void**) &out.ptr);
    *out.ptr = geometry;
    return out;
}

Render_Material_Ptr render_material_add(Render* render, String name)
{
    Render_Material material = {0};
    material.info = render_info_make(name);

    Render_Material_Ptr out = {0};
    out.id = material.info.id;
    stable_array_insert(&render->geometries, (void**) &out.ptr);
    *out.ptr = material;
    return out;
}

Render_Geometry_Ptr render_geometry_add_shape(Render* render, Shape shape, String name)
{
    return render_geometry_add(render, shape.vertices.data, shape.vertices.size, (i32*) (void*) shape.triangles.data, shape.triangles.size * 3, name);
}

bool render_texture_add_from_disk_named(Render* render, Render_Texture_Ptr* out, String path, String name)
{
    LOG_INFO("render", "Adding texture at path '%s' current working dir '%s'", cstring_ephemeral(path), platform_directory_get_current_working());
    log_indent();
    PROFILE_START(image_read_counter);

    bool state = true;
    Arena_Frame arena = scratch_arena_acquire();
    {
        Image temp_storage = {&arena.allocator};
        state = image_read_from_file(&temp_storage, path, 0, PIXEL_TYPE_U8, IMAGE_LOAD_FLAG_FLIP_Y);
        if(state)
            *out = render_texture_add(render, temp_storage, name);
    }
    arena_frame_release(&arena);
    
    PROFILE_END(image_read_counter);
    log_outdent();
    return state;
}

bool render_texture_add_from_disk(Render* render, Render_Texture_Ptr* out, String path)
{
    return render_texture_add_from_disk_named(render, out, path, path_get_filename_without_extension(path_parse(path)));
}
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
    Render render = {0};

    resources_init(&resources, &resources_alloc.allocator);
    resources_set(&resources);

    Shape uv_sphere = {0};
    Shape cube_sphere = {0};
    Shape screen_quad = {0};
    Shape unit_quad = {0};
    Shape unit_cube = {0};
    
    Render_Geometry_Ptr render_uv_sphere = {0};
    Render_Geometry_Ptr render_cube_sphere = {0};
    Render_Geometry_Ptr render_screen_quad = {0};
    Render_Geometry_Ptr render_cube = {0};
    Render_Geometry_Ptr render_quad = {0};

    Render_Texture_Ptr image_floor = {0};
    Render_Texture_Ptr image_debug = {0};
    Render_Texture_Ptr image_rusted_iron_metallic = {0};

    Render_Shader shader_depth_color = {0};
    Render_Shader shader_solid_color = {0};
    Render_Shader shader_screen = {0};
    Render_Shader shader_debug = {0};
    Render_Shader shader_blinn_phong = {0};
    Render_Shader shader_skybox = {0};
    Render_Shader shader_instanced = {0};
    Render_Shader shader_instanced_batched = {0};

    Render_Material_Ptr material_shiny_debug = {0};
    Render_Material_Ptr material_mat_floor = {0};

    TEST(render_shader_init_from_disk(&shader_instanced_batched, STRING("shaders/instanced_batched_texture.glsl")));
    
    Render_Memory_Budget render_mem_budget = {0};
    render_mem_budget.geometry = GIBI_BYTE / 2;
    render_mem_budget.texture = GIBI_BYTE / 2;
    render_mem_budget.command_buffer = MEBI_BYTE * 256;
    render_mem_budget.instance_buffer = MEBI_BYTE * 100;
    render_mem_budget.draw_buffer = MEBI_BYTE * 100;
    render_init(&render, &renderer_alloc.allocator, &shader_instanced_batched, render_mem_budget);

    f64 fps_display_frequency = 4;
    f64 fps_display_last_update = 0;

    glEnable(GL_TEXTURE_CUBE_MAP_SEAMLESS);
    
    //Platform_File_Watch watch = {0};
    //platform_file_watch(&watch, STRING("./"), PLATFORM_FILE_WATCH_ALL | PLATFORM_FILE_WATCH_SUBDIRECTORIES, NULL, NULL);

    for(isize frame_num = 0; app->should_close == false; frame_num ++)
    {
        //Platform_File_Watch_Event file_event = {0};
        //while(platform_file_watch_poll(watch, &file_event))
        //{
        //    const char* event = "";
        //    switch(file_event.action) {
        //        case PLATFORM_FILE_WATCH_CREATED : event = "CREATED"; break;
        //        case PLATFORM_FILE_WATCH_DELETED : event = "DELETED"; break;
        //        case PLATFORM_FILE_WATCH_MODIFIED: event = "MODIFIED"; break;
        //        case PLATFORM_FILE_WATCH_RENAMED : event = "RENAMED"; break;
        //    }

        //    LOG_OKAY("FILE WATCH", "%s: %s '%s' -> '%s'", event, file_event.watched_path, file_event.old_path, file_event.path);
        //}

        glfwSwapBuffers(window);
        f64 start_frame_time = clock_s();
        app->delta_time = start_frame_time - app->last_frame_timepoint; 
        app->last_frame_timepoint = start_frame_time; 

        window_process_input(window, frame_num == 0);
        //if(app->window_framebuffer_width != app->window_framebuffer_width_prev 
        //    || app->window_framebuffer_height != app->window_framebuffer_height_prev
        //    || frame_num == 0)
        //{
        //    ASSERT(false);
        //}
        
        if(control_was_pressed(&app->controls, CONTROL_REFRESH_ALL) 
            || control_was_pressed(&app->controls, CONTROL_REFRESH_SHADERS)
            || frame_num == 0)
        {
            LOG_INFO("APP", "Refreshing shaders");
            PROFILE_START(shader_load_counter);
            
            bool shader_state = true;
            shader_state = shader_state && render_shader_init_from_disk(&shader_solid_color,       STRING("shaders/solid_color.glsl"));
            shader_state = shader_state && render_shader_init_from_disk(&shader_depth_color,       STRING("shaders/depth_color.glsl"));
            shader_state = shader_state && render_shader_init_from_disk(&shader_screen,            STRING("shaders/screen.glsl"));
            shader_state = shader_state && render_shader_init_from_disk(&shader_blinn_phong,       STRING("shaders/blinn_phong.glsl"));
            shader_state = shader_state && render_shader_init_from_disk(&shader_skybox,            STRING("shaders/skybox.glsl"));
            shader_state = shader_state && render_shader_init_from_disk(&shader_debug,             STRING("shaders/uv_debug.glsl"));
            shader_state = shader_state && render_shader_init_from_disk(&shader_instanced,         STRING("shaders/instanced_texture.glsl"));

            ASSERT(shader_state);

            {
                GLint max_textures = 0;
                glGetIntegerv(GL_MAX_TEXTURE_IMAGE_UNITS, &max_textures);
                LOG_WARN("app", "max textures %i", max_textures);
            }

            PROFILE_END(shader_load_counter);
        }


        if(control_was_pressed(&app->controls, CONTROL_REFRESH_ALL) 
            || control_was_pressed(&app->controls, CONTROL_REFRESH_ART)
            || frame_num == 0)
        {
            LOG_INFO("APP", "Refreshing art");
            PROFILE_START(art_load_counter);

            PROFILE_START(art_counter_shapes);
            shape_deinit(&uv_sphere);
            shape_deinit(&cube_sphere);
            shape_deinit(&screen_quad);
            shape_deinit(&unit_cube);
            shape_deinit(&unit_quad);

            uv_sphere = shapes_make_uv_sphere(40, 1);
            cube_sphere = shapes_make_cube_sphere(20, 1);
            screen_quad = shapes_make_quad(2, vec3(0, 0, 1), vec3(0, 1, 0), vec3_of(0));
            unit_cube = shapes_make_unit_cube();
            unit_quad = shapes_make_unit_quad();
            PROFILE_END(art_counter_shapes);

            bool texture_state = true;
            render_uv_sphere = render_geometry_add_shape(&render, uv_sphere, STRING("uv_sphere"));
            render_cube_sphere = render_geometry_add_shape(&render, cube_sphere, STRING("cube_sphere"));
            render_screen_quad = render_geometry_add_shape(&render, screen_quad, STRING("screen_quad"));
            render_cube = render_geometry_add_shape(&render, unit_cube, STRING("unit_cube"));
            render_quad = render_geometry_add_shape(&render, unit_quad, STRING("unit_cube"));
            
            texture_state = texture_state && render_texture_add_from_disk(&render, &image_rusted_iron_metallic, STRING("resources/rustediron2/rustediron2_metallic.png"));
            texture_state = texture_state && render_texture_add_from_disk(&render, &image_floor, STRING("resources/floor.jpg"));
            texture_state = texture_state && render_texture_add_from_disk(&render, &image_debug, STRING("resources/debug.png"));

            material_shiny_debug = render_material_add(&render, STRING("material_shiny_debug"));
            material_mat_floor = render_material_add(&render, STRING("material_mat_floor"));

            //@TODO
            material_shiny_debug.ptr->diffuse_color = vec3(1, 0, 0);
            material_shiny_debug.ptr->specular_color = vec3(1, 1, 1);
            material_shiny_debug.ptr->specular_exponent = 256;
            material_shiny_debug.ptr->textures[0] = image_debug;
            material_shiny_debug.ptr->textures[1] = image_rusted_iron_metallic;
            material_shiny_debug.ptr->used_textures = 2;

            material_mat_floor.ptr->diffuse_color = vec3(0, 1, 0);
            material_mat_floor.ptr->specular_color = vec3(0, 1, 1);
            material_mat_floor.ptr->specular_exponent = 10;
            material_mat_floor.ptr->textures[0] = image_floor;
            material_mat_floor.ptr->used_textures = 1;

            render_texture_manager_generate_mips(&render.texture_manager);

            PROFILE_END(art_load_counter);
            ASSERT(texture_state);
        }

        if(control_was_pressed(&app->controls, CONTROL_ESCAPE))
        {
            app->is_in_mouse_mode = !app->is_in_mouse_mode;
        }
        
        if(control_was_pressed(&app->controls, CONTROL_DEBUG_1))
        {
            profile_log_all(log_info("app"), true);
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

        //Mat4 view = camera_make_view_matrix(app->camera);
        //Mat4 projection = camera_make_projection_matrix(app->camera);

        
        //================ FIRST PASS ==================
        {
            PROFILE_START(first_pass_counter);
            //render_screen_frame_buffers_msaa_render_begin(&screen_buffers);
            glClearColor(0.3f, 0.3f, 0.2f, 1.0f);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
            
            
            enum {
                GRID_Y = 400,
                GRID_X = 400,
            };


            PROFILE_START(submit_counter);
            for(isize y = 0; y < GRID_Y; y++)
                for(isize x = 0; x < GRID_X; x++)
                {
                    Render_Phong_Command phong_command = {0};

                    phong_command.transform = mat4_translation(vec3((f32) 2*x, (f32) 2*y, 0));

                    i32 do_x = 0;
                    if(1)
                    {
                        isize v = x + y;
                        do_x = v % 3 == 0;
                    }
                    else
                    {
                        isize v = x + y*GRID_Y;
                        if(v < GRID_X*GRID_Y * 2/3)
                            do_x = 1;
                        if(v < GRID_X*GRID_Y * 1/3)
                            do_x = 2;
                    }

                    if(do_x == 0)
                    {
                        phong_command.material = material_shiny_debug;    
                        phong_command.geometry = render_cube;
                    }
                    else if(do_x == 1)
                    {
                        phong_command.material = material_shiny_debug;    
                        phong_command.geometry = render_cube_sphere;
                    }
                    else
                    {
                        phong_command.geometry = render_cube;
                        phong_command.material = material_mat_floor;
                    }

                    render_queue_submit_phong(&render, &phong_command);
                }
            PROFILE_END(submit_counter);
            render_render(&render, app->camera);
  

            //render skybox
            //{
            //    Mat4 model = mat4_scaling(vec3_of(-1));
            //    Mat4 stationary_view = mat4_from_mat3(mat3_from_mat4(view));
            //    render_mesh_draw_using_skybox(render_cube, &shader_skybox, projection, stationary_view, model, 1, cubemap_skybox);
            //}

            //render_screen_frame_buffers_msaa_render_end(&screen_buffers);
            PROFILE_END(first_pass_counter);
        }

        //// ============== POST PROCESSING PASS ==================
        //{
        //    PROFILE_START(second_pass_counter);
        //    render_screen_frame_buffers_msaa_post_process_begin(&screen_buffers);
        //    render_mesh_draw_using_postprocess(render_screen_quad, &shader_screen, screen_buffers.screen_color_buff, settings->screen_gamma, settings->screen_exposure);
        //    render_screen_frame_buffers_msaa_post_process_end(&screen_buffers);
        //    PROFILE_END(second_pass_counter);
        //}

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

    render_shader_deinit(&shader_solid_color);
    render_shader_deinit(&shader_depth_color);
    render_shader_deinit(&shader_screen);
    render_shader_deinit(&shader_blinn_phong);
    render_shader_deinit(&shader_skybox);
    render_shader_deinit(&shader_debug);

    resources_deinit(&resources);
    
    profile_log_all(log_info("APP"), true);

    LOG_INFO("RESOURCES", "Resources allocation stats:");
    log_allocator_stats(log_info(">RESOURCES"), &resources_alloc.allocator);

    debug_allocator_deinit(&resources_alloc);
    debug_allocator_deinit(&renderer_alloc);

    LOG_INFO("APP", "run_func exit");
}

void error_func(void* context, Platform_Sandbox_Error error)
{
    (void) context;
    const char* msg = platform_exception_to_string(error.exception);
    
    LOG_ERROR("APP", "%s exception occurred", msg);
    LOG_TRACE("APP", "printing trace:");
    log_captured_callstack(log_error(">APP"), error.call_stack, error.call_stack_size);
}

//#include "mdump2.h"
#include "lib/_test_all.h"


ATTRIBUTE_INLINE_NEVER 
void profile_smartness_tester()
{
    for(int i = 0; i < 1000; i++)
    {
        PROFILE_COUNTER();
    }
}

void run_test_func(void* context)
{
    profile_smartness_tester();
    //test_profile();
    (void) context;
    test_all(3.0);
}
