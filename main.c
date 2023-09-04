#define LIB_ALL_IMPL
#define LIB_MEM_DEBUG
#define GLAD_GL_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION

#include "platform.h"
#include "string.h"
#include "hash_index.h"
#include "log.h"
#include "file.h"
#include "allocator_debug.h"
#include "allocator_malloc.h"
#include "random.h"
#include "math.h"

#include "gl.h"
#include "gl_shader_util.h"
#include "gl_debug_output.h"
#include "shapes.h"

#include "stb/stb_image.h"
#include "glfw/glfw3.h"

typedef enum Control_Name
{
    CONTROL_NONE = 0,

    /* mouse on screen when mouse is visible */
    CONTROL_MOUSE_X, 
    CONTROL_MOUSE_Y,

    /* scrolling while mouse is visible*/
    CONTROL_SCROLL, 

    /* looking around in a 3D world*/
    CONTROL_LOOK_X, 
    CONTROL_LOOK_Y,
    CONTROL_ZOOM,

    CONTROL_MOVE_FORWARD,
    CONTROL_MOVE_BACKWARD,
    CONTROL_MOVE_RIGHT,
    CONTROL_MOVE_LEFT,
    CONTROL_MOVE_UP,
    CONTROL_MOVE_DOWN,

    CONTROL_JUMP,
    CONTROL_INTERACT,
    CONTROL_ESCAPE,
    CONTROL_PAUSE,
    CONTROL_SPRINT,
    
    CONTROL_REFRESH_SHADERS,
    CONTROL_REFRESH_ART,
    CONTROL_REFRESH_CODE,
    CONTROL_REFRESH_ALL,

    CONTROL_DEBUG_1,
    CONTROL_DEBUG_2,
    CONTROL_DEBUG_3,
    CONTROL_DEBUG_4,
    CONTROL_DEBUG_5,
    CONTROL_DEBUG_6,
    CONTROL_DEBUG_7,
    CONTROL_DEBUG_8,
    CONTROL_DEBUG_9,
    CONTROL_DEBUG_10,

    CONTROL_COUNT,
} Control_Name;

//just for consistency here
#define GLFW_MOUSE_X 0
#define GLFW_MOUSE_Y 1
#define GLFW_MOUSE_SCROLL 2
#define GLFW_MOUSE_LAST GLFW_MOUSE_SCROLL

//Maps physical inputs to abstract game actions
//each entry can be assigned to one action.
//values are interpreted as Control_Name
//A key can be assigned to multiple actions simultaneously
// by duplicating this structure
typedef struct Control_Mapping
{
    u8 mouse[GLFW_MOUSE_LAST + 1];
    u8 mouse_buttons[GLFW_MOUSE_BUTTON_LAST + 1];
    u8 keys[GLFW_KEY_LAST + 1];
    //@TODO: Joystick and gamepads later....
} Control_Mapping;

typedef enum Input_Type
{
    INPUT_TYPE_MOUSE,
    INPUT_TYPE_MOUSE_BUTTON,
    INPUT_TYPE_KEY
} Input_Type;

//An abstarct set of game actions to be used by the game code
//we represent all values as ammount and the number of interactions
//this allows us to rebind pretty much everything to everything including 
//keys to control camera and mouse to control movement.
//This is useful for example because controllers use smooth controls for movement while keyboard does not
//The values range from -1 to 1 where 0 indicates off and nonzero indicates on. 
typedef struct Controls
{
    f32 values[CONTROL_COUNT];
    u8 interactions[CONTROL_COUNT];
} Controls;

//windows defines...
#undef near
#undef far

typedef struct Camera
{
    f32 fov;
    f32 aspect_ratio;
    Vec3 pos;
    Vec3 looking_at;
    Vec3 up_dir;

    f32 near;
    f32 far;
    
    //if is set to true looking_at is relative to pos 
    //effectively making the looking_at become looking_at + pos
    bool is_position_relative;  

    //specifies if should use ortographic projection.
    //If is true uses top/bot/left/right. The area of the rectangle must not be 0!
    bool is_ortographic;        
    f32 top;
    f32 bot;
    f32 left;
    f32 right;
} Camera;

typedef struct Game_Settings
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
} Game_Settings;

#define CONTROL_MAPPING_SETS 3
typedef struct Game_State
{
    Game_Settings settings;
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
} Game_State;


bool control_was_pressed(Controls* controls, Control_Name control)
{
    u8 interactions = controls->interactions[control];
    f32 value = controls->values[control];

    //was pressed and released at least once
    if(interactions > 2)
        return true;

    //was interacted with once to pressed state
    if(interactions == 1 && value != 0.0f)
        return true;

    return false;
}

bool control_was_released(Controls* controls, Control_Name control)
{
    u8 interactions = controls->interactions[control];
    f32 value = controls->values[control];

    //was pressed and released at least once
    if(interactions > 2)
        return true;

    //was interacted with once to released state
    if(interactions == 1 && value == 0.0f)
        return true;

    return false;
}

bool control_is_down(Controls* controls, Control_Name control)
{
    f32 value = controls->values[control];
    return value != 0;
}

u8* control_mapping_get_entry(Control_Mapping* mapping, Input_Type type, isize index)
{
    switch(type)
    {
        case INPUT_TYPE_MOUSE:        
            CHECK_BOUNDS(index, GLFW_MOUSE_LAST + 1); 
            return &mapping->mouse[index];
        case INPUT_TYPE_MOUSE_BUTTON: 
            CHECK_BOUNDS(index, GLFW_MOUSE_BUTTON_LAST + 1); 
            return &mapping->mouse_buttons[index];
        case INPUT_TYPE_KEY:          
            CHECK_BOUNDS(index, GLFW_KEY_LAST + 1); 
            return &mapping->keys[index];
        default: ASSERT(false); return NULL;
    }
}

void control_mapping_add(Control_Mapping mappings[CONTROL_MAPPING_SETS], Control_Name control, Input_Type type, isize index)
{
    for(isize i = 0; i < CONTROL_MAPPING_SETS; i++)
    {
        Control_Mapping* mapping = &mappings[i];
        u8* control_slot = control_mapping_get_entry(mapping, type, index);
        if(*control_slot == CONTROL_NONE)
        {
            *control_slot = control;
            break;
        }
    }
}

void mapping_make_default(Control_Mapping mappings[CONTROL_MAPPING_SETS])
{
    memset(mappings, 0, sizeof(Control_Mapping) * CONTROL_MAPPING_SETS);

    const Input_Type MOUSE = INPUT_TYPE_MOUSE;
    const Input_Type KEY = INPUT_TYPE_KEY;
    const Input_Type MOUSE_BUTTON = INPUT_TYPE_MOUSE_BUTTON;

    (void) MOUSE_BUTTON;

    control_mapping_add(mappings, CONTROL_MOUSE_X,      MOUSE, GLFW_MOUSE_X);
    control_mapping_add(mappings, CONTROL_MOUSE_Y,      MOUSE, GLFW_MOUSE_Y);
    control_mapping_add(mappings, CONTROL_LOOK_X,       MOUSE, GLFW_MOUSE_X);
    control_mapping_add(mappings, CONTROL_LOOK_Y,       MOUSE, GLFW_MOUSE_Y);
    control_mapping_add(mappings, CONTROL_SCROLL,       MOUSE, GLFW_MOUSE_SCROLL);
    control_mapping_add(mappings, CONTROL_ZOOM,         MOUSE, GLFW_MOUSE_SCROLL);
    
    control_mapping_add(mappings, CONTROL_MOVE_FORWARD, KEY, GLFW_KEY_W);
    control_mapping_add(mappings, CONTROL_MOVE_BACKWARD,KEY, GLFW_KEY_S);
    control_mapping_add(mappings, CONTROL_MOVE_RIGHT,   KEY, GLFW_KEY_D);
    control_mapping_add(mappings, CONTROL_MOVE_LEFT,    KEY, GLFW_KEY_A);
    control_mapping_add(mappings, CONTROL_MOVE_UP,      KEY, GLFW_KEY_SPACE);
    control_mapping_add(mappings, CONTROL_MOVE_DOWN,    KEY, GLFW_KEY_LEFT_SHIFT);
    
    control_mapping_add(mappings, CONTROL_MOVE_FORWARD, KEY, GLFW_KEY_UP);
    control_mapping_add(mappings, CONTROL_MOVE_FORWARD, KEY, GLFW_KEY_DOWN);
    control_mapping_add(mappings, CONTROL_MOVE_RIGHT,   KEY, GLFW_KEY_RIGHT);
    control_mapping_add(mappings, CONTROL_MOVE_RIGHT,   KEY, GLFW_KEY_LEFT);
    
    control_mapping_add(mappings, CONTROL_JUMP,         KEY, GLFW_KEY_SPACE);
    control_mapping_add(mappings, CONTROL_INTERACT,     KEY, GLFW_KEY_E);
    control_mapping_add(mappings, CONTROL_ESCAPE,       KEY, GLFW_KEY_ESCAPE);
    control_mapping_add(mappings, CONTROL_PAUSE,        KEY, GLFW_KEY_ESCAPE);
    control_mapping_add(mappings, CONTROL_SPRINT,       KEY, GLFW_KEY_LEFT_ALT);
    
    control_mapping_add(mappings, CONTROL_REFRESH_ALL,  KEY, GLFW_KEY_R);
    control_mapping_add(mappings, CONTROL_DEBUG_1,      KEY, GLFW_KEY_F1);
    control_mapping_add(mappings, CONTROL_DEBUG_2,      KEY, GLFW_KEY_F2);
    control_mapping_add(mappings, CONTROL_DEBUG_3,      KEY, GLFW_KEY_F3);
    control_mapping_add(mappings, CONTROL_DEBUG_4,      KEY, GLFW_KEY_F4);
    control_mapping_add(mappings, CONTROL_DEBUG_5,      KEY, GLFW_KEY_F5);
    control_mapping_add(mappings, CONTROL_DEBUG_6,      KEY, GLFW_KEY_F6);
    control_mapping_add(mappings, CONTROL_DEBUG_7,      KEY, GLFW_KEY_F7);
    control_mapping_add(mappings, CONTROL_DEBUG_8,      KEY, GLFW_KEY_F8);
    control_mapping_add(mappings, CONTROL_DEBUG_9,      KEY, GLFW_KEY_F9);
    control_mapping_add(mappings, CONTROL_DEBUG_10,     KEY, GLFW_KEY_F10);
}

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

void controls_set_control_by_input(Game_State* game_state, Input_Type input_type, isize index, f32 value, bool is_increment)
{
    for(isize i = 0; i < CONTROL_MAPPING_SETS; i++)
    {
        Control_Mapping* mapping = &game_state->control_mappings[i];
        u8* control_slot = control_mapping_get_entry(mapping, input_type, index);
        if(*control_slot != CONTROL_NONE)
        {
            u8* interactions = &game_state->controls.interactions[*control_slot];
            f32* stored_value = &game_state->controls.values[*control_slot];

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
    Game_State* game_state = (Game_State*) glfwGetWindowUserPointer(window);
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
    
    game_state->controls_prev = game_state->controls;
    memset(&game_state->controls.interactions, 0, sizeof game_state->controls.interactions);

    glfwPollEvents();
    game_state->should_close = glfwWindowShouldClose(window);
    
    STATIC_ASSERT(sizeof(int) == sizeof(i32));

    game_state->window_screen_width_prev = game_state->window_screen_width;
    game_state->window_screen_height_prev = game_state->window_screen_height;
    glfwGetWindowSize(window, (int*) &game_state->window_screen_width, (int*) &game_state->window_screen_height);

    game_state->window_framebuffer_width_prev = game_state->window_framebuffer_width;
    game_state->window_framebuffer_height_prev = game_state->window_framebuffer_height;
    glfwGetFramebufferSize(window, (int*) &game_state->window_framebuffer_width, (int*) &game_state->window_framebuffer_height);
    
    game_state->window_framebuffer_width = MAX(game_state->window_framebuffer_width, 1);
    game_state->window_framebuffer_height = MAX(game_state->window_framebuffer_height, 1);

    f64 new_mouse_x = 0;
    f64 new_mouse_y = 0;
    glfwGetCursorPos(window, &new_mouse_x, &new_mouse_y);
    controls_set_control_by_input(game_state, INPUT_TYPE_MOUSE, GLFW_MOUSE_X, (f32) new_mouse_x, false);
    controls_set_control_by_input(game_state, INPUT_TYPE_MOUSE, GLFW_MOUSE_Y, (f32) new_mouse_y, false);
    
    if(game_state->is_in_mouse_mode_prev != game_state->is_in_mouse_mode || is_initial_call)
    {
        if(game_state->is_in_mouse_mode == false)
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
        //game_state->controls_prev = game_state->controls;

    game_state->is_in_mouse_mode_prev = game_state->is_in_mouse_mode;
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

    Game_State* game_state = (Game_State*) glfwGetWindowUserPointer(window);
    controls_set_control_by_input(game_state, INPUT_TYPE_KEY, key, value, false);
    
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

    Game_State* game_state = (Game_State*) glfwGetWindowUserPointer(window);
    controls_set_control_by_input(game_state, INPUT_TYPE_MOUSE_BUTTON, button, value, false);
}

void glfw_scroll_func(GLFWwindow* window, f64 xoffset, f64 yoffset)
{   
    (void) xoffset;
    f32 value = (f32) yoffset;
    Game_State* game_state = (Game_State*) glfwGetWindowUserPointer(window);
    controls_set_control_by_input(game_state, INPUT_TYPE_MOUSE, GLFW_MOUSE_SCROLL, value, true);
}

const char* platform_sandbox_error_to_cstring(Platform_Sandox_Error error)
{
    switch(error)
    {
        case PLATFORM_EXCEPTION_NONE: return "PLATFORM_EXCEPTION_NONE";
        case PLATFORM_EXCEPTION_ACCESS_VIOLATION: return "PLATFORM_EXCEPTION_ACCESS_VIOLATION";
        case PLATFORM_EXCEPTION_DATATYPE_MISALIGNMENT: return "PLATFORM_EXCEPTION_DATATYPE_MISALIGNMENT";
        case PLATFORM_EXCEPTION_FLOAT_DENORMAL_OPERAND: return "PLATFORM_EXCEPTION_FLOAT_DENORMAL_OPERAND";
        case PLATFORM_EXCEPTION_FLOAT_DIVIDE_BY_ZERO: return "PLATFORM_EXCEPTION_FLOAT_DIVIDE_BY_ZERO";
        case PLATFORM_EXCEPTION_FLOAT_INEXACT_RESULT: return "PLATFORM_EXCEPTION_FLOAT_INEXACT_RESULT";
        case PLATFORM_EXCEPTION_FLOAT_INVALID_OPERATION: return "PLATFORM_EXCEPTION_FLOAT_INVALID_OPERATION";
        case PLATFORM_EXCEPTION_FLOAT_OVERFLOW: return "PLATFORM_EXCEPTION_FLOAT_OVERFLOW";
        case PLATFORM_EXCEPTION_FLOAT_UNDERFLOW: return "PLATFORM_EXCEPTION_FLOAT_UNDERFLOW";
        case PLATFORM_EXCEPTION_FLOAT_OTHER: return "PLATFORM_EXCEPTION_FLOAT_OTHER";
        case PLATFORM_EXCEPTION_PAGE_ERROR: return "PLATFORM_EXCEPTION_PAGE_ERROR";
        case PLATFORM_EXCEPTION_INT_DIVIDE_BY_ZERO: return "PLATFORM_EXCEPTION_INT_DIVIDE_BY_ZERO";
        case PLATFORM_EXCEPTION_INT_OVERFLOW: return "PLATFORM_EXCEPTION_INT_OVERFLOW";
        case PLATFORM_EXCEPTION_ILLEGAL_INSTRUCTION: return "PLATFORM_EXCEPTION_ILLEGAL_INSTRUCTION";
        case PLATFORM_EXCEPTION_PRIVILAGED_INSTRUCTION: return "PLATFORM_EXCEPTION_PRIVILAGED_INSTRUCTION";
        case PLATFORM_EXCEPTION_BREAKPOINT: return "PLATFORM_EXCEPTION_BREAKPOINT";
        case PLATFORM_EXCEPTION_BREAKPOINT_SINGLE_STEP: return "PLATFORM_EXCEPTION_BREAKPOINT_SINGLE_STEP";
        case PLATFORM_EXCEPTION_STACK_OVERFLOW: return "PLATFORM_EXCEPTION_STACK_OVERFLOW";
        case PLATFORM_EXCEPTION_ABORT: return "PLATFORM_EXCEPTION_ABORT";
        case PLATFORM_EXCEPTION_TERMINATE: return "PLATFORM_EXCEPTION_TERMINATE";
        case PLATFORM_EXCEPTION_OTHER: return "PLATFORM_EXCEPTION_OTHER";
        default:
            return "PLATFORM_EXCEPTION_OTHER";
    }
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

Mat4 camera_make_projection_matrix(Camera camera)
{
    Mat4 projection = {0};
    if(camera.is_ortographic)
        projection = mat4_ortographic_projection(camera.bot, camera.top, camera.left, camera.right, camera.near, camera.far);
    else
        projection = mat4_perspective_projection(camera.fov, camera.aspect_ratio, camera.near, camera.far);
    return projection;
}

Vec3 camera_get_look_dir(Camera camera)
{
    Vec3 look_dir = camera.looking_at;
    if(camera.is_position_relative == false)
        look_dir = vec3_sub(look_dir, camera.pos);
    return look_dir;
}

Vec3 camera_get_looking_at(Camera camera)
{
    Vec3 looking_at = camera.looking_at;
    if(camera.is_position_relative)
        looking_at = vec3_add(looking_at, camera.pos); 
    return looking_at;
}

Mat4 camera_make_view_matrix(Camera camera)
{
    Vec3 looking_at = camera_get_looking_at(camera);
    Mat4 view = mat4_look_at(camera.pos, looking_at, camera.up_dir);
    return view;
}

void run_func(void* context);
void error_func(void* context, Platform_Sandox_Error error_code);

int main()
{
    platform_init();
    Malloc_Allocator malloc_allocator = {0};
    malloc_allocator_init_use(&malloc_allocator, 0);
    log_system_init(&malloc_allocator.allocator, &malloc_allocator.allocator);

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
 
    GLFWwindow* window = glfwCreateWindow(1600, 900, "Game", NULL, NULL);
    TEST_MSG(window != NULL, "Failed to make glfw window");

    Game_State game_state = {0};
    glfwSetWindowUserPointer(window, &game_state);
    glfwMakeContextCurrent(window);

    int version = gladLoadGL((GLADloadfunc) glfwGetProcAddress);
    TEST_MSG(version != 0, "Failed to load opengl with glad");

    gl_debug_output_enable();

    platform_exception_sandbox(
        run_func, window, 
        error_func, window);

    glfwDestroyWindow(window);
    glfwTerminate();

    log_system_deinit();
    ASSERT(malloc_allocator.bytes_allocated == 0);
    malloc_allocator_deinit(&malloc_allocator);
    platform_deinit();

    return 0;    
}

typedef struct Render_Screen_Frame_Buffers
{
    GLuint frame_buff;
    GLuint screen_color_buff;
    GLuint render_buff;

    i32 width;
    i32 height;

    String_Builder name;
} Render_Screen_Frame_Buffers;

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
    buffer->name = builder_from_cstring("Render_Screen_Frame_Buffers");

    //@NOTE: 
    //The lack of the following line caused me 2 hours of debugging why my application crashed due to NULL ptr 
    //deref in glDrawArrays. I still dont know why this occurs but just for safety its better to leave this here.
    glBindVertexArray(0);

    glGenFramebuffers(1, &buffer->frame_buff);
    glBindFramebuffer(GL_FRAMEBUFFER, buffer->frame_buff);    

    // generate texture
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

typedef struct Render_Screen_Frame_Buffers_MSAA
{
    GLuint frame_buff;
    GLuint texture_color_multisampled_buff;
    GLuint render_buff;
    GLuint intermediate_frame_buff;
    GLuint screen_color_buff;

    i32 width;
    i32 height;
    
    //used so that this becomes visible to debug_allocator
    // and thus we prevent leaking
    String_Builder name;
} Render_Screen_Frame_Buffers_MSAA;

void render_screen_frame_buffers_msaa_deinit(Render_Screen_Frame_Buffers_MSAA* buffer)
{
    glBindVertexArray(0);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    glDeleteFramebuffers(1, &buffer->frame_buff);
    glDeleteBuffers(1, &buffer->texture_color_multisampled_buff);
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
    buffer->name = builder_from_cstring("Render_Screen_Frame_Buffers_MSAA");

    bool state = true;
    glGenFramebuffers(1, &buffer->frame_buff);
    glBindFramebuffer(GL_FRAMEBUFFER, buffer->frame_buff);

    // create a multisampled color attachment texture
    glGenTextures(1, &buffer->texture_color_multisampled_buff);
    glBindTexture(GL_TEXTURE_2D_MULTISAMPLE, buffer->texture_color_multisampled_buff);
    glTexImage2DMultisample(GL_TEXTURE_2D_MULTISAMPLE, sample_count, GL_RGB32F, width, height, GL_TRUE);
    glBindTexture(GL_TEXTURE_2D_MULTISAMPLE, 0);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D_MULTISAMPLE, buffer->texture_color_multisampled_buff, 0);

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

    // create a color attachment texture
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
    // 2. now blit multisampled buffer(s) to normal colorbuffer of intermediate FBO. Image is stored in screenTexture
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


#define VEC2_FMT "{%f, %f}"
#define VEC2_PRINT(vec) (vec).x, (vec).y

#define VEC3_FMT "{%f, %f, %f}"
#define VEC3_PRINT(vec) (vec).x, (vec).y, (vec).z

#define VEC4_FMT "{%f, %f, %f, %f}"
#define VEC4_PRINT(vec) (vec).x, (vec).y, (vec).z, (vec).w

typedef struct Render_Mesh
{
    String_Builder name;

    GLuint vao;
    GLuint vbo;
    GLuint ebo;

    GLuint vertex_count;
    GLuint index_count;
} Render_Mesh;

typedef struct Bitmap
{
    u8* data;
    i32 width;
    i32 height;
    u8 channels;
    bool is_floats;

    i32 from_x;
    i32 to_x;
    
    i32 from_y;
    i32 to_y;

    String_Builder name;
} Bitmap;

typedef struct Render_Texture
{
    GLuint texture;
    isize width;
    isize height;
    
    String_Builder name;
} Render_Texture;

typedef struct Render_Cubemap
{
    GLuint texture;

    isize widths[6];
    isize heights[6];
    
    String_Builder name;
} Render_Cubemap;

void render_texture_deinit(Render_Texture* render)
{
    if(render->texture != 0)
        glDeleteTextures(1, &render->texture);
    array_deinit(&render->name);
    memset(&render, 0, sizeof(render));
}

void render_texture_init(Render_Texture* render, Bitmap bitmap, String name)
{
    GLenum bitmap_format = bitmap.is_floats ? GL_FLOAT : GL_UNSIGNED_BYTE;
    GLenum format = GL_RGB;
    switch(bitmap.channels)
    {
        case 1: format = GL_RED; break;
        case 2: format = GL_RG; break;
        case 3: format = GL_RGB; break;
        case 4: format = GL_RGBA; break;
        default: format = GL_RGB; break;
    }

    render_texture_deinit(render);
    render->height = bitmap.height;
    render->width = bitmap.width;
    render->name = builder_from_string(name);
    glGenTextures(1, &render->texture);
    glBindTexture(GL_TEXTURE_2D, render->texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);	
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexImage2D(GL_TEXTURE_2D, 0, format, (GLsizei) bitmap.width, (GLsizei) bitmap.height, 0, format, bitmap_format, bitmap.data);
    glGenerateMipmap(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, 0);
} 

void render_texture_init_hdr(Render_Texture* render, Bitmap bitmap, String name)
{
    GLenum bitmap_format = bitmap.is_floats ? GL_FLOAT : GL_UNSIGNED_BYTE;
    GLenum format = GL_RGB;
    switch(bitmap.channels)
    {
        case 1: format = GL_RED; break;
        case 2: format = GL_RG; break;
        case 3: format = GL_RGB; break;
        case 4: format = GL_RGBA; break;
        default: format = GL_RGB; break;
    }

    render_texture_deinit(render);
    render->height = bitmap.height;
    render->width = bitmap.width;
    render->name = builder_from_string(name);
    glGenTextures(1, &render->texture);
    glBindTexture(GL_TEXTURE_2D, render->texture);
    
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB16F, (GLsizei) bitmap.width, (GLsizei) bitmap.height, 0, format, bitmap_format, bitmap.data);
    glBindTexture(GL_TEXTURE_2D, 0);
} 

void render_texture_init_from_single_pixel(Render_Texture* render, Vec4 color, u8 channels, String name)
{
    Bitmap bitmap = {0};
    u8 channel_values[4] = {
        (u8) (255*CLAMP(color.x, 0, 1)), 
        (u8) (255*CLAMP(color.y, 0, 1)), 
        (u8) (255*CLAMP(color.z, 0, 1)), 
        (u8) (255*CLAMP(color.w, 0, 1)), 
    };

    bitmap.data = channel_values;
    bitmap.channels = channels;
    bitmap.width = 1;
    bitmap.height = 1;
    bitmap.to_x = 1;
    bitmap.to_y = 1;

    render_texture_init(render, bitmap, name);
}

void render_texture_use(const Render_Texture* render, isize slot)
{
    glActiveTexture(GL_TEXTURE0 + (GLenum) slot);
    glBindTexture(GL_TEXTURE_2D, render->texture);
}

void render_texture_unuse(const Render_Texture* render)
{
    (void) render;
    glBindTexture(GL_TEXTURE_2D, 0);
}

void render_cubemap_deinit(Render_Cubemap* render)
{
    if(render->texture != 0)
        glDeleteTextures(1, &render->texture);
    array_deinit(&render->name);
    memset(&render, 0, sizeof(render));
}

void render_cubemap_init(Render_Cubemap* render, const Bitmap bitmaps[6], String name)
{
    for (isize i = 0; i < 6; i++)
        ASSERT(bitmaps[i].data != NULL && "cannot miss data");

    render_cubemap_deinit(render);
    render->name = builder_from_string(name);

    glGenTextures(1, &render->texture);
    glBindTexture(GL_TEXTURE_CUBE_MAP, render->texture);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);	
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);

    for (isize i = 0; i < 6; i++)
    {
        Bitmap bitmap = bitmaps[i];
        render->heights[i] = bitmap.height;
        render->widths[i] = bitmap.width;

        GLenum format = GL_RGB;
        switch(bitmap.channels)
        {
            case 1: format = GL_R; break;
            case 2: format = GL_RG; break;
            case 3: format = GL_RGB; break;
            case 4: format = GL_RGBA; break;
            default: format = GL_RGB; break;
        }
        
        glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + (GLenum) i, 0, format, (GLsizei) bitmap.width, (GLsizei) bitmap.height, 0, format, GL_UNSIGNED_BYTE, bitmap.data);
    }

    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glGenerateMipmap(GL_TEXTURE_CUBE_MAP);
    glBindTexture(GL_TEXTURE_CUBE_MAP, 0);
} 

void render_cubemap_use(const Render_Cubemap* render, isize slot)
{
    glActiveTexture(GL_TEXTURE0 + (GLenum) slot);
    glBindTexture(GL_TEXTURE_CUBE_MAP, render->texture);
}

void render_cubemap_unuse(const Render_Cubemap* render)
{
    (void) render;
    glBindTexture(GL_TEXTURE_CUBE_MAP, 0);
}

void bitmap_deinit(Bitmap* bitmap)
{
    stbi_image_free(bitmap->data);
    array_deinit(&bitmap->name);
    memset(bitmap, 0, sizeof *bitmap);
}

#define BITMAP_FROM_DISK_FLIP 1
#define BITMAP_FROM_DISK_FLOATS 2

bool bitmap_init_from_disk(Bitmap* bitmap, String path, int flags)
{
    bitmap_deinit(bitmap);
    bitmap->name = builder_from_string(path);

    String_Builder escaped = {0};
    array_init_backed(&escaped, allocator_get_scratch(), 256);
    builder_append(&escaped, path);

    bool flip = !!(flags & BITMAP_FROM_DISK_FLIP);
    bool is_floats = !!(flags & BITMAP_FROM_DISK_FLOATS);

    // load and generate the texture
    int width = 0;
    int height = 0;
    int channels = 0;
    stbi_set_flip_vertically_on_load(flip);
    unsigned char *data = stbi_load(cstring_from_builder(escaped), &width, &height, &channels, 0);

    if(data)
    {
        bitmap->data = data;
        bitmap->width = width;
        bitmap->height = height;
        bitmap->channels = (u8) channels;
        bitmap->is_floats = is_floats;

        bitmap->to_x = width;
        bitmap->to_y = height;
    }
    else
    {
        LOG_ERROR("ASSET", "Failed to open a file: \"" STRING_FMT "\"", path);
        ASSERT(false);
    }

    array_deinit(&escaped);
    return data != NULL;
}

bool render_texture_init_from_disk(Render_Texture* tetxure, String path, String name)
{
    Bitmap bitmap = {0};
    bool state = bitmap_init_from_disk(&bitmap, path, BITMAP_FROM_DISK_FLIP);
    ASSERT(state);
    render_texture_init(tetxure, bitmap, name);
    bitmap_deinit(&bitmap);

    return state;
}

bool render_texture_init_hdr_from_disk(Render_Texture* tetxure, String path, String name)
{
    Bitmap bitmap = {0};
    bool state = bitmap_init_from_disk(&bitmap, path, BITMAP_FROM_DISK_FLIP | BITMAP_FROM_DISK_FLOATS);
    ASSERT(state);
    render_texture_init_hdr(tetxure, bitmap, name);
    bitmap_deinit(&bitmap);

    return state;
}


bool render_cubemap_init_from_disk(Render_Cubemap* render, String front, String back, String top, String bot, String right, String left, String name)
{
    Bitmap face_bitmaps[6] = {0};
    String face_paths[6] = {right, left, top, bot, front, back};

    bool state = true;
    for (isize i = 0; i < 6; i++)
        state = state && bitmap_init_from_disk(&face_bitmaps[i], face_paths[i], 0);

    render_cubemap_init(render, face_bitmaps, name);
    
    for (isize i = 0; i < 6; i++)
        bitmap_deinit(&face_bitmaps[i]);

    return state;
}

void render_mesh_init(Render_Mesh* mesh, const Vertex* vertices, isize vertices_count, const Triangle_Indeces* indeces, isize indeces_count, String name)
{
    memset(mesh, 0, sizeof *mesh);

    glGenVertexArrays(1, &mesh->vao);
    glGenBuffers(1, &mesh->vbo);
    glGenBuffers(1, &mesh->ebo);
  
    glBindVertexArray(mesh->vao);
    glBindBuffer(GL_ARRAY_BUFFER, mesh->vbo);
    glBufferData(GL_ARRAY_BUFFER, vertices_count * sizeof(Vertex), vertices, GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mesh->ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, indeces_count * sizeof(Triangle_Indeces), indeces, GL_STATIC_DRAW);
    
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
    mesh->index_count = (GLuint) indeces_count;
    mesh->vertex_count = (GLuint) vertices_count;
}

void render_mesh_deinit(Render_Mesh* mesh)
{
    array_deinit(&mesh->name);
    glDeleteVertexArrays(1, &mesh->vao);
    glDeleteBuffers(1, &mesh->ebo);
    glDeleteBuffers(1, &mesh->vbo);
    memset(mesh, 0, sizeof *mesh);
}

void render_mesh_draw(Render_Mesh mesh)
{
    glBindVertexArray(mesh.vao);
    glDrawElements(GL_TRIANGLES, mesh.index_count * 3, GL_UNSIGNED_INT, 0);
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
    capture->name = builder_from_cstring("Render_Capture_Buffers");

    glGenFramebuffers(1, &capture->frame_buff);
    glGenRenderbuffers(1, &capture->render_buff);

    glBindFramebuffer(GL_FRAMEBUFFER, capture->frame_buff);
    glBindRenderbuffer(GL_RENDERBUFFER, capture->render_buff);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, prealloc_width, prealloc_height);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, capture->render_buff);
    
    TEST_MSG(glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE, "frame buffer creation failed!");
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

INTERNAL void _make_capture_projections(Mat4* capture_projection, Mat4 capture_views[6])
{
    *capture_projection = mat4_perspective_projection(TAU/4, 1.0f, 0.1f, 10.0f);
    capture_views[0] = mat4_look_at(vec3(0.0f, 0.0f, 0.0f), vec3( 1.0f,  0.0f,  0.0f), vec3(0.0f, -1.0f,  0.0f));
    capture_views[1] = mat4_look_at(vec3(0.0f, 0.0f, 0.0f), vec3(-1.0f,  0.0f,  0.0f), vec3(0.0f, -1.0f,  0.0f));
    capture_views[2] = mat4_look_at(vec3(0.0f, 0.0f, 0.0f), vec3( 0.0f,  1.0f,  0.0f), vec3(0.0f,  0.0f,  1.0f));
    capture_views[3] = mat4_look_at(vec3(0.0f, 0.0f, 0.0f), vec3( 0.0f, -1.0f,  0.0f), vec3(0.0f,  0.0f, -1.0f));
    capture_views[4] = mat4_look_at(vec3(0.0f, 0.0f, 0.0f), vec3( 0.0f,  0.0f,  1.0f), vec3(0.0f, -1.0f,  0.0f));
    capture_views[5] = mat4_look_at(vec3(0.0f, 0.0f, 0.0f), vec3( 0.0f,  0.0f, -1.0f), vec3(0.0f, -1.0f,  0.0f));
}


void render_cubemap_init_environment_from_environment_texture(Render_Cubemap* cubemap_environment, i32 width, i32 height, const Render_Texture* environment_tetxure, f32 texture_gamma,
    const Render_Capture_Buffers* capture, const Render_Shader* shader_equi_to_cubemap, Render_Mesh cube_mesh)
{
    render_cubemap_deinit(cubemap_environment);
    cubemap_environment->name = builder_from_cstring("environment");

    glGenTextures(1, &cubemap_environment->texture);
    glBindTexture(GL_TEXTURE_CUBE_MAP, cubemap_environment->texture);
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
    Mat4 capture_views[6] = {0};
    _make_capture_projections(&capture_projection, capture_views);

    render_shader_use(shader_equi_to_cubemap);
    render_shader_set_mat4(shader_equi_to_cubemap, "projection", capture_projection);
    render_shader_set_f32(shader_equi_to_cubemap, "gamma", texture_gamma);
    render_texture_use(environment_tetxure, 0);
    render_shader_set_i32(shader_equi_to_cubemap, "equirectangularMap", 0);

    for (u32 i = 0; i < 6; ++i)
    {
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, cubemap_environment->texture, 0);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        render_shader_set_mat4(shader_equi_to_cubemap, "view", capture_views[i]);
        render_mesh_draw(cube_mesh);
    }
    render_shader_unuse(shader_equi_to_cubemap);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    
    // then let OpenGL generate mipmaps from first mip face (combatting visible dots artifact)
    glBindTexture(GL_TEXTURE_CUBE_MAP, cubemap_environment->texture);
    glGenerateMipmap(GL_TEXTURE_CUBE_MAP);
}

void render_cubemap_init_irradiance_from_environment(
    Render_Cubemap* cubemap_irradiance, i32 width, i32 height, f32 sample_delta, const Render_Cubemap* cubemap_environment,
    const Render_Capture_Buffers* capture, const Render_Shader* shader_irradiance_gen, Render_Mesh cube_mesh)
{
    // pbr: create an irradiance cubemap, and re-scale capture FBO to irradiance scale.
    // --------------------------------------------------------------------------------
    render_cubemap_deinit(cubemap_irradiance);
    cubemap_irradiance->name = builder_from_cstring("irradiance");

    glGenTextures(1, &cubemap_irradiance->texture);
    glBindTexture(GL_TEXTURE_CUBE_MAP, cubemap_irradiance->texture);
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
    Mat4 capture_views[6] = {0};
    _make_capture_projections(&capture_projection, capture_views);

    render_shader_use(shader_irradiance_gen);
    render_shader_set_mat4(shader_irradiance_gen, "projection", capture_projection);
    render_shader_set_f32(shader_irradiance_gen, "sample_delta", sample_delta);
    render_cubemap_use(cubemap_environment, 0);
    render_shader_set_i32(shader_irradiance_gen, "environmentMap", 0);

    glViewport(0, 0, width, height); // don't forget to configure the viewport to the capture dimensions.
    glBindFramebuffer(GL_FRAMEBUFFER, capture->frame_buff);
    for (u32 i = 0; i < 6; ++i)
    {
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, cubemap_irradiance->texture, 0);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        
        render_shader_set_mat4(shader_irradiance_gen, "view", capture_views[i]);
        render_mesh_draw(cube_mesh);
    }
    render_cubemap_unuse(cubemap_environment);
    render_shader_unuse(shader_irradiance_gen);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void render_cubemap_init_prefilter_from_environment(
    Render_Cubemap* cubemap_irradiance, i32 width, i32 height, const Render_Cubemap* cubemap_environment,
    const Render_Capture_Buffers* capture, const Render_Shader* shader_prefilter, Render_Mesh cube_mesh)
{
    // pbr: create a pre-filter cubemap, and re-scale capture FBO to pre-filter scale.
    // --------------------------------------------------------------------------------
    render_cubemap_deinit(cubemap_irradiance);
    cubemap_irradiance->name = builder_from_cstring("pbr_prefilter");

    glGenTextures(1, &cubemap_irradiance->texture);
    glBindTexture(GL_TEXTURE_CUBE_MAP, cubemap_irradiance->texture);
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
    Mat4 capture_views[6] = {0};
    _make_capture_projections(&capture_projection, capture_views);

    render_shader_use(shader_prefilter);
    render_shader_set_mat4(shader_prefilter, "projection", capture_projection);
    render_cubemap_use(cubemap_environment, 0);
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
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, cubemap_irradiance->texture, mip);

            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
            
            render_shader_set_mat4(shader_prefilter, "view", capture_views[i]);
            render_mesh_draw(cube_mesh);
        }
    }
    render_cubemap_unuse(cubemap_environment);
    render_shader_unuse(shader_prefilter);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void render_texture_init_BRDF_LUT(
    Render_Texture* cubemap_irradiance, i32 width, i32 height,
    const Render_Capture_Buffers* capture, const Render_Shader* shader_brdf_lut, Render_Mesh screen_quad_mesh)
{
    // pbr: create a pre-filter cubemap, and re-scale capture FBO to pre-filter scale.
    // --------------------------------------------------------------------------------
    render_texture_deinit(cubemap_irradiance);
    cubemap_irradiance->height = height;
    cubemap_irradiance->width = width;
    cubemap_irradiance->name = builder_from_cstring("BRDF_LUT");

    glGenTextures(1, &cubemap_irradiance->texture);
    glBindTexture(GL_TEXTURE_2D, cubemap_irradiance->texture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RG16F, width, height, 0, GL_RG, GL_FLOAT, 0);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    // then re-configure capture framebuffer object and render screen-space quad with BRDF shader.
    glBindFramebuffer(GL_FRAMEBUFFER, capture->frame_buff);
    glBindRenderbuffer(GL_RENDERBUFFER, capture->render_buff);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, width, height);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, cubemap_irradiance->texture, 0);

    glViewport(0, 0, width, height);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    render_shader_use(shader_brdf_lut);
    render_mesh_draw(screen_quad_mesh);
    render_shader_unuse(shader_brdf_lut);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
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

void render_mesh_draw_using_blinn_phong(Render_Mesh mesh, const Render_Shader* blin_phong_shader, Mat4 projection, Mat4 view, Mat4 model, Vec3 view_pos, Vec3 light_pos, Vec3 light_color, Blinn_Phong_Params params, Render_Texture diffuse)
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

    render_texture_use(&diffuse, 0);
    render_shader_set_i32(blin_phong_shader, "texture_diffuse", 0);
    
    render_mesh_draw(mesh);
    render_texture_unuse(&diffuse);
    render_shader_unuse(blin_phong_shader);
}

void render_mesh_draw_using_skybox(Render_Mesh mesh, const Render_Shader* skybox_shader, Mat4 projection, Mat4 view, Mat4 model, f32 gamma, Render_Cubemap skybox)
{
    glDepthFunc(GL_LEQUAL);
    render_shader_use(skybox_shader);
    render_shader_set_mat4(skybox_shader, "projection", projection);
    render_shader_set_mat4(skybox_shader, "view", view);
    render_shader_set_mat4(skybox_shader, "model", model);
    render_shader_set_f32(skybox_shader, "gamma", gamma);
    render_mesh_draw(mesh);
    
    render_cubemap_use(&skybox, 0);
    render_shader_set_i32(skybox_shader, "cubemap_diffuse", 0);
    
    render_mesh_draw(mesh);
    render_cubemap_unuse(&skybox);
    render_shader_unuse(skybox_shader);
    glDepthFunc(GL_LESS);
}

#define PBR_MAX_LIGHTS 4
typedef struct PBR_Light
{
    Vec3 pos;
    Vec3 color;
    f32 radius;
} PBR_Light;

typedef struct PBR_Params
{
    PBR_Light lights[PBR_MAX_LIGHTS];
    Vec3 view_pos;
    f32 gamma;
    f32 attentuation_strength;
} PBR_Params;

typedef struct Render_PBR_Material
{
    Render_Texture texture_albedo;
    Render_Texture texture_normal;
    Render_Texture texture_metallic;
    Render_Texture texture_roughness;
    Render_Texture texture_ao;

    Vec3 solid_albedo;
    f32 solid_metallic;
    f32 solid_roughness;
    f32 solid_ao;
    Vec3 reflection_at_zero_incidence;
} Render_PBR_Material;

void render_pbr_material_deinit(Render_PBR_Material* material)
{
    render_texture_deinit(&material->texture_albedo);
    render_texture_deinit(&material->texture_normal);
    render_texture_deinit(&material->texture_metallic);
    render_texture_deinit(&material->texture_roughness);
    render_texture_deinit(&material->texture_ao);
    memset(material, 0, sizeof(*material));
}

void render_mesh_draw_using_pbr(Render_Mesh mesh, const Render_Shader* pbr_shader, Mat4 projection, Mat4 view, Mat4 model, const PBR_Params* params, const Render_PBR_Material* material)
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

    //material
    Vec3 reflection_at_zero_incidence = material->reflection_at_zero_incidence;
    if(vec3_is_equal(reflection_at_zero_incidence, vec3_of(0)))
        reflection_at_zero_incidence = vec3_of(0.04f);
    render_shader_set_vec3(pbr_shader, "reflection_at_zero_incidence", reflection_at_zero_incidence);

    //Temp
    bool use_textures = material->texture_albedo.texture 
        || material->texture_normal.texture
        || material->texture_metallic.texture
        || material->texture_roughness.texture
        || material->texture_ao.texture;
    render_shader_set_i32(pbr_shader, "use_textures", use_textures);

    if(use_textures)
    {
        render_texture_use(&material->texture_albedo, 0);
        render_shader_set_i32(pbr_shader, "texture_albedo", 0);    
        
        render_texture_use(&material->texture_normal, 1);
        render_shader_set_i32(pbr_shader, "texture_normal", 1);   
        
        render_texture_use(&material->texture_metallic, 2);
        render_shader_set_i32(pbr_shader, "texture_metallic", 2);   
        
        render_texture_use(&material->texture_roughness, 3);
        render_shader_set_i32(pbr_shader, "texture_roughness", 3);   
        
        render_texture_use(&material->texture_ao, 4);
        render_shader_set_i32(pbr_shader, "texture_ao", 4);   
    }
    else
    {
        render_shader_set_vec3(pbr_shader, "solid_albedo", material->solid_albedo);
        render_shader_set_f32(pbr_shader, "solid_metallic", material->solid_metallic);
        render_shader_set_f32(pbr_shader, "solid_roughness", material->solid_roughness);
        render_shader_set_f32(pbr_shader, "solid_ao", material->solid_ao);
    }
    
    render_mesh_draw(mesh);

    if(use_textures)
    {
        render_texture_unuse(&material->texture_albedo);
        render_texture_unuse(&material->texture_normal);
        render_texture_unuse(&material->texture_metallic);
        render_texture_unuse(&material->texture_roughness);
        render_texture_unuse(&material->texture_ao);
    }
    render_shader_unuse(pbr_shader);
}


void render_mesh_draw_using_pbr_mapped(Render_Mesh mesh, const Render_Shader* pbr_shader, Mat4 projection, Mat4 view, Mat4 model, const PBR_Params* params, const Render_PBR_Material* material, 
    const Render_Cubemap* environment, const Render_Cubemap* irradience, const Render_Cubemap* prefilter, const Render_Texture* brdf_lut)
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
    render_cubemap_use(irradience, slot);
    render_shader_set_i32(pbr_shader, "cubemap_irradiance", slot++);  
    
    render_cubemap_use(prefilter, slot);
    render_shader_set_i32(pbr_shader, "cubemap_prefilter", slot++);  
    
    render_texture_use(brdf_lut, slot);
    render_shader_set_i32(pbr_shader, "texture_brdf_lut", slot++);  

    //Temp
    bool use_textures = material->texture_albedo.texture 
        || material->texture_normal.texture
        || material->texture_metallic.texture
        || material->texture_roughness.texture
        || material->texture_ao.texture;
    render_shader_set_i32(pbr_shader, "use_textures", use_textures);

    if(use_textures)
    {
        render_texture_use(&material->texture_albedo, slot);
        render_shader_set_i32(pbr_shader, "texture_albedo", slot++);    
        
        render_texture_use(&material->texture_normal, slot);
        render_shader_set_i32(pbr_shader, "texture_normal", slot++);   
        
        render_texture_use(&material->texture_metallic, slot);
        render_shader_set_i32(pbr_shader, "texture_metallic", slot++);   
        
        render_texture_use(&material->texture_roughness, slot);
        render_shader_set_i32(pbr_shader, "texture_roughness", slot++);   
        
        render_texture_use(&material->texture_ao, slot);
        render_shader_set_i32(pbr_shader, "texture_ao", slot++);   
    }
    else
    {
        render_shader_set_vec3(pbr_shader, "solid_albedo", material->solid_albedo);
        render_shader_set_f32(pbr_shader, "solid_metallic", material->solid_metallic);
        render_shader_set_f32(pbr_shader, "solid_roughness", material->solid_roughness);
        render_shader_set_f32(pbr_shader, "solid_ao", material->solid_ao);
    }

    render_mesh_draw(mesh);

    if(use_textures)
    {
        render_texture_unuse(&material->texture_albedo);
        render_texture_unuse(&material->texture_normal);
        render_texture_unuse(&material->texture_metallic);
        render_texture_unuse(&material->texture_roughness);
        render_texture_unuse(&material->texture_ao);
    }

    render_cubemap_unuse(irradience);
    render_cubemap_unuse(prefilter);
    render_texture_unuse(brdf_lut);
    render_shader_unuse(pbr_shader);
}


void render_mesh_draw_using_postprocess(Render_Mesh screen_quad, const Render_Shader* shader_screen, GLuint screen_texture, f32 gamma, f32 exposure)
{
    render_shader_use(shader_screen);
    render_shader_set_f32(shader_screen, "gamma", gamma);
    render_shader_set_f32(shader_screen, "exposure", exposure);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, screen_texture);
    render_shader_set_i32(shader_screen, "screen", 0);
            
    render_mesh_draw(screen_quad);
    glBindTexture(GL_TEXTURE_2D, 0);
    render_shader_unuse(shader_screen);
}

void run_func(void* context)
{
    LOG_INFO("APP", "run_func enter");
    
    Debug_Allocator debug_alloc = {0};
    debug_allocator_init_use(&debug_alloc, DEBUG_ALLOCATOR_DEINIT_LEAK_CHECK | DEBUG_ALLOCATOR_CAPTURE_CALLSTACK);

    GLFWwindow* window = (GLFWwindow*) context;
    Game_State* game_state = (Game_State*) glfwGetWindowUserPointer(window);
    memset(game_state, 0, sizeof *game_state);

    game_state->camera.pos        = vec3(0.0f, 0.0f,  0.0f);
    game_state->camera.looking_at = vec3(1.0f, 0.0f,  0.0f);
    game_state->camera.up_dir     = vec3(0.0f, 1.0f,  0.0f);
    game_state->camera.is_position_relative = true;

    game_state->camera_yaw = -TAU/4;
    game_state->camera_pitch = 0.0;

    game_state->is_in_mouse_mode = false;
    game_state->settings.fov = TAU/4;
    game_state->settings.movement_speed = 2.5f;
    game_state->settings.movement_sprint_mult = 5;
    game_state->settings.screen_gamma = 2.2f;
    game_state->settings.screen_exposure = 1;
    game_state->settings.mouse_sensitivity = 0.002f;
    game_state->settings.mouse_wheel_sensitivity = 0.05f; // uwu
    game_state->settings.zoom_adjust_time = 0.2f;
    game_state->settings.mouse_sensitivity_scale_with_fov_ammount = 1.0f;
    game_state->settings.MSAA_samples = 4;

    mapping_make_default(game_state->control_mappings);

    Render_Screen_Frame_Buffers_MSAA screen_buffers = {0};
    Render_Capture_Buffers capture_buffers = {0};
    
    i32 res_environment = 1024;
    i32 res_irradiance = 32;
    i32 res_prefilter = 128;
    i32 res_brdf_lut = 512;

    f32 irradicance_sample_delta = 0.025f; //big danger! If set too low pc will lag completely!
    f32 default_ao = 0.2f;

    Shape uv_sphere = {0};
    Shape cube_sphere = {0};
    Shape screen_quad = {0};
    Shape unit_quad = {0};
    Shape unit_cube = {0};

    Render_Mesh render_uv_sphere = {0};
    Render_Mesh render_cube_sphere = {0};
    Render_Mesh render_screen_quad = {0};
    Render_Mesh render_cube = {0};
    Render_Mesh render_quad = {0};
    

    Render_Texture texture_floor = {0};
    Render_Texture texture_debug = {0};
    Render_Texture texture_environment = {0};
    Render_PBR_Material material_metal = {0};

    Render_Cubemap cubemap_skybox = {0};

    Render_Cubemap cubemap_environment = {0};
    Render_Cubemap cubemap_irradiance = {0};
    Render_Cubemap cubemap_prefilter = {0};
    Render_Texture texture_brdf_lut = {0};

    Render_Shader shader_depth_color = {0};
    Render_Shader shader_solid_color = {0};
    Render_Shader shader_screen = {0};
    Render_Shader shader_debug = {0};
    Render_Shader shader_blinn_phong = {0};
    Render_Shader shader_skybox = {0};
    Render_Shader shader_pbr = {0};
    Render_Shader shader_pbr_mapped = {0};

    Render_Shader shader_equi_to_cubemap = {0};
    Render_Shader shader_irradiance = {0};
    Render_Shader shader_prefilter = {0};
    Render_Shader shader_brdf_lut = {0};

    f64 fps_display_frequency = 4;
    f64 fps_display_last_update = 0;
    String_Builder fps_display = {0};
    bool use_mapping = true;

    glEnable(GL_TEXTURE_CUBE_MAP_SEAMLESS);
    for(isize frame_num = 0; game_state->should_close == false; frame_num ++)
    {
        glfwSwapBuffers(window);
        f64 start_frame_time = clock_s();
        game_state->delta_time = start_frame_time - game_state->last_frame_timepoint; 
        game_state->last_frame_timepoint = start_frame_time; 

        window_process_input(window, frame_num == 0);
        if(game_state->window_framebuffer_width != game_state->window_framebuffer_width_prev 
            || game_state->window_framebuffer_height != game_state->window_framebuffer_height_prev
            || frame_num == 0)
        {
            LOG_INFO("APP", "Resizing");
            render_screen_frame_buffers_msaa_deinit(&screen_buffers);
            render_screen_frame_buffers_msaa_init(&screen_buffers, game_state->window_framebuffer_width, game_state->window_framebuffer_height, game_state->settings.MSAA_samples);
            glViewport(0, 0, game_state->window_framebuffer_width, game_state->window_framebuffer_height); //@TODO stuff somehwere else!
        }
        
        if(control_was_pressed(&game_state->controls, CONTROL_REFRESH_ALL) 
            || control_was_pressed(&game_state->controls, CONTROL_REFRESH_SHADERS)
            || frame_num == 0)
        {
            LOG_INFO("APP", "Refreshing shaders");

            bool ok = true;
            ok = ok && render_shader_init_from_disk(&shader_solid_color, 
                STRING("shaders/solid_color.vert"), 
                STRING("shaders/solid_color.frag"), 
                STRING("shader_solid_color"));
            ok = ok && render_shader_init_from_disk(&shader_depth_color, 
                STRING("shaders/depth_color.vert"), 
                STRING("shaders/depth_color.frag"), 
                STRING("shader_depth_color"));
            ok = ok && render_shader_init_from_disk(&shader_screen, 
                STRING("shaders/screen.vert"), 
                STRING("shaders/screen.frag"), 
                STRING("shader_screen"));
            ok = ok && render_shader_init_from_disk(&shader_blinn_phong, 
                STRING("shaders/blinn_phong.vert"), 
                STRING("shaders/blinn_phong.frag"), 
                STRING("shader_blinn_phong"));
            ok = ok && render_shader_init_from_disk(&shader_skybox, 
                STRING("shaders/skybox.vert"), 
                STRING("shaders/skybox.frag"), 
                STRING("shader_skybox"));
            ok = ok && render_shader_init_from_disk(&shader_pbr, 
                STRING("shaders/pbr.vert"), 
                STRING("shaders/pbr.frag"), 
                STRING("shader_pbr"));
            ok = ok && render_shader_init_from_disk(&shader_equi_to_cubemap, 
                STRING("shaders/equi_to_cubemap.vert"), 
                STRING("shaders/equi_to_cubemap.frag"), 
                STRING("shader_equi_to_cubemap"));
            ok = ok && render_shader_init_from_disk(&shader_irradiance, 
                STRING("shaders/irradiance_convolution.vert"), 
                STRING("shaders/irradiance_convolution.frag"), 
                STRING("shader_irradiance"));
            ok = ok && render_shader_init_from_disk(&shader_prefilter, 
                STRING("shaders/prefilter.vert"), 
                STRING("shaders/prefilter.frag"), 
                STRING("shader_prefilter"));
            ok = ok && render_shader_init_from_disk(&shader_brdf_lut, 
                STRING("shaders/brdf_lut.vert"), 
                STRING("shaders/brdf_lut.frag"), 
                STRING("shader_brdf_lut"));
            ok = ok && render_shader_init_from_disk(&shader_pbr_mapped, 
                STRING("shaders/pbr_mapped.vert"), 
                STRING("shaders/pbr_mapped.frag"), 
                STRING("shader_pbr_mapped"));
            ok = ok && render_shader_init_from_disk_custom(&shader_debug, 
                STRING("shaders/uv_debug.vert"), 
                STRING("shaders/uv_debug.frag"), 
                STRING("shaders/uv_debug.geom"), 
                STRING(""), STRING("shader_debug"), NULL);

            ASSERT(ok);
        }

        if(control_was_pressed(&game_state->controls, CONTROL_REFRESH_ALL) 
            || control_was_pressed(&game_state->controls, CONTROL_REFRESH_ART)
            || frame_num == 0)
        {
            
            LOG_INFO("APP", "Refreshing art");

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

            render_mesh_init(&render_uv_sphere, uv_sphere.vertices.data, uv_sphere.vertices.size, uv_sphere.indeces.data, uv_sphere.indeces.size, STRING("uv_sphere"));
            render_mesh_init(&render_cube_sphere, cube_sphere.vertices.data, cube_sphere.vertices.size, cube_sphere.indeces.data, cube_sphere.indeces.size, STRING("cube_sphere"));
            render_mesh_init(&render_screen_quad, screen_quad.vertices.data, screen_quad.vertices.size, screen_quad.indeces.data, screen_quad.indeces.size, STRING("screen_quad"));
            render_mesh_init(&render_cube, unit_cube.vertices.data, unit_cube.vertices.size, unit_cube.indeces.data, unit_cube.indeces.size, STRING("unit_cube"));
            render_mesh_init(&render_quad, unit_quad.vertices.data, unit_quad.vertices.size, unit_quad.indeces.data, unit_quad.indeces.size, STRING("unit_cube"));

            bool ok = true;
            ok = ok && render_texture_init_from_disk(&texture_floor, STRING("resources/floor.jpg"), STRING("floor"));
            ok = ok && render_texture_init_from_disk(&texture_debug, STRING("resources/debug.png"), STRING("debug"));
            ok = ok && render_cubemap_init_from_disk(&cubemap_skybox, 
                STRING("resources/skybox_front.jpg"), 
                STRING("resources/skybox_back.jpg"), 
                STRING("resources/skybox_top.jpg"), 
                STRING("resources/skybox_bottom.jpg"), 
                STRING("resources/skybox_right.jpg"), 
                STRING("resources/skybox_left.jpg"), STRING("skybox"));
                
            ok = ok && render_texture_init_from_disk(&material_metal.texture_albedo, STRING("resources/rustediron2/rustediron2_basecolor.png"), STRING("rustediron2_basecolor"));
            ok = ok && render_texture_init_from_disk(&material_metal.texture_metallic, STRING("resources/rustediron2/rustediron2_metallic.png"), STRING("rustediron2_metallic"));
            ok = ok && render_texture_init_from_disk(&material_metal.texture_normal, STRING("resources/rustediron2/rustediron2_normal.png"), STRING("rustediron2_normal"));
            ok = ok && render_texture_init_from_disk(&material_metal.texture_roughness, STRING("resources/rustediron2/rustediron2_roughness.png"), STRING("rustediron2_roughness"));
            render_texture_init_from_single_pixel(&material_metal.texture_ao, vec4_of(default_ao), 1, STRING("rustediron2_ao"));

            ok = ok && render_texture_init_from_disk(&texture_environment, STRING("resources/HDR_041_Path_Ref.hdr"), STRING("texture_environment"));
            
            render_capture_buffers_init(&capture_buffers, res_environment, res_environment);
            
            f32 use_gamma = 2.0f;
            if(control_is_down(&game_state->controls, CONTROL_DEBUG_3))
                use_gamma = 1.0f;

            render_cubemap_init_environment_from_environment_texture(
                &cubemap_environment, res_environment, res_environment, &texture_environment, use_gamma, 
                &capture_buffers, &shader_equi_to_cubemap, render_cube);
            render_cubemap_init_irradiance_from_environment(
                &cubemap_irradiance, res_irradiance, res_irradiance, irradicance_sample_delta,
                &cubemap_environment, &capture_buffers, &shader_irradiance, render_cube);

            glViewport(0, 0, game_state->window_framebuffer_width, game_state->window_framebuffer_height); //@TODO stuff somehwere else!

            ASSERT(ok);
        }
        
        


        if(control_was_pressed(&game_state->controls, CONTROL_ESCAPE))
        {
            game_state->is_in_mouse_mode = !game_state->is_in_mouse_mode;
        }
        
        if(control_was_pressed(&game_state->controls, CONTROL_DEBUG_1))
        {
            game_state->is_in_uv_debug_mode = !game_state->is_in_uv_debug_mode;
        }
        
        if(control_was_pressed(&game_state->controls, CONTROL_DEBUG_2))
        {
            use_mapping = !use_mapping;
        }

        if(game_state->is_in_mouse_mode == false)
        {
            f32 fov_sens_modifier = sinf(game_state->zoom_target_fov);
            //Movement
            {
                f32 move_speed = game_state->settings.movement_speed;
                if(control_is_down(&game_state->controls, CONTROL_SPRINT))
                    move_speed *= game_state->settings.movement_sprint_mult;

                Vec3 direction_forward = vec3_norm(camera_get_look_dir(game_state->camera));
                Vec3 direction_up = vec3_norm(game_state->camera.up_dir);
                Vec3 direction_right = vec3_norm(vec3_cross(direction_forward, game_state->camera.up_dir));
            
                Vec3 move_dir = {0};
                move_dir = vec3_add(move_dir, vec3_scale(direction_forward, game_state->controls.values[CONTROL_MOVE_FORWARD]));
                move_dir = vec3_add(move_dir, vec3_scale(direction_up, game_state->controls.values[CONTROL_MOVE_UP]));
                move_dir = vec3_add(move_dir, vec3_scale(direction_right, game_state->controls.values[CONTROL_MOVE_RIGHT]));
            
                move_dir = vec3_add(move_dir, vec3_scale(direction_forward, -game_state->controls.values[CONTROL_MOVE_BACKWARD]));
                move_dir = vec3_add(move_dir, vec3_scale(direction_up, -game_state->controls.values[CONTROL_MOVE_DOWN]));
                move_dir = vec3_add(move_dir, vec3_scale(direction_right, -game_state->controls.values[CONTROL_MOVE_LEFT]));

                if(vec3_len(move_dir) != 0.0f)
                    move_dir = vec3_norm(move_dir);

                Vec3 move_ammount = vec3_scale(move_dir, move_speed * (f32) game_state->delta_time);
                game_state->camera.pos = vec3_add(game_state->camera.pos, move_ammount);
            }

            //Camera rotation
            {
                f32 mousex_prev = game_state->controls_prev.values[CONTROL_LOOK_X];
                f32 mousey_prev = game_state->controls_prev.values[CONTROL_LOOK_Y];

                f32 mousex = game_state->controls.values[CONTROL_LOOK_X];
                f32 mousey = game_state->controls.values[CONTROL_LOOK_Y];
            
                if(mousex != mousex_prev || mousey != mousey_prev)
                {
                    f64 xoffset = mousex - mousex_prev;
                    f64 yoffset = mousey_prev - mousey; // reversed since y-coordinates range from bottom to top

                    f32 total_sensitivity = game_state->settings.mouse_sensitivity * lerpf(1, fov_sens_modifier, game_state->settings.mouse_sensitivity_scale_with_fov_ammount);

                    xoffset *= total_sensitivity;
                    yoffset *= total_sensitivity;
                    f32 epsilon = 1e-5f;

                    game_state->camera_yaw   += (f32) xoffset;
                    game_state->camera_pitch += (f32) yoffset; 
                    game_state->camera_pitch = CLAMP(game_state->camera_pitch, -TAU/4.0f + epsilon, TAU/4.0f - epsilon);
        
                    Vec3 direction = {0};
                    direction.x = cosf(game_state->camera_yaw) * cosf(game_state->camera_pitch);
                    direction.y = sinf(game_state->camera_pitch);
                    direction.z = sinf(game_state->camera_yaw) * cosf(game_state->camera_pitch);

                    game_state->camera.looking_at = vec3_norm(direction);
                }
            }
        
            //smooth zoom
            {
                //Works by setting a target fov, time to hit it and speed and then
                //interpolates smoothly until the value is hit
                if(game_state->zoom_target_fov == 0)
                    game_state->zoom_target_fov = game_state->settings.fov;
                
                f32 zoom = game_state->controls.values[CONTROL_ZOOM];
                f32 zoom_prev = game_state->controls_prev.values[CONTROL_ZOOM];
                f32 zoom_delta = zoom_prev - zoom;
                if(zoom_delta != 0)
                {   
                    f32 fov_delta = zoom_delta * game_state->settings.mouse_wheel_sensitivity * fov_sens_modifier;
                
                    game_state->zoom_target_fov = CLAMP(game_state->zoom_target_fov + fov_delta, 0, TAU/2);
                    game_state->zoom_target_time = start_frame_time + game_state->settings.zoom_adjust_time;
                    game_state->zoom_change_per_sec = (game_state->zoom_target_fov - game_state->settings.fov) / game_state->settings.zoom_adjust_time;
                }
            
                if(start_frame_time < game_state->zoom_target_time)
                {
                    f32 fov_before = game_state->settings.fov;
                    game_state->settings.fov += game_state->zoom_change_per_sec * (f32) game_state->delta_time;

                    //if is already past the target snap to target
                    if(fov_before < game_state->zoom_target_fov && game_state->settings.fov > game_state->zoom_target_fov)
                        game_state->settings.fov = game_state->zoom_target_fov;
                    if(fov_before > game_state->zoom_target_fov && game_state->settings.fov < game_state->zoom_target_fov)
                        game_state->settings.fov = game_state->zoom_target_fov;
                }
                else
                    game_state->settings.fov = game_state->zoom_target_fov;
            }
            
        }

        
        game_state->camera.fov = game_state->settings.fov;
        game_state->camera.near = 0.1f;
        game_state->camera.far = 100.0f;
        game_state->camera.is_ortographic = false;
        game_state->camera.is_position_relative = true;
        game_state->camera.aspect_ratio = (f32) game_state->window_framebuffer_width / (f32) game_state->window_framebuffer_height;

        Mat4 view = camera_make_view_matrix(game_state->camera);
        Mat4 projection = camera_make_projection_matrix(game_state->camera);

        
        //================ FIRST PASS ==================
        {
            render_screen_frame_buffers_msaa_render_begin(&screen_buffers);
            glClearColor(0.3f, 0.3f, 0.2f, 1.0f);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

            PBR_Light lights[PBR_MAX_LIGHTS] = {0};

            f32 light_radius = 0.3f;
            if(control_is_down(&game_state->controls, CONTROL_DEBUG_4))
                light_radius = 1;

            lights[0].pos = vec3(0, 4, 0);
            lights[0].color = vec3(1, 1, 0.5f);
            lights[0].radius = light_radius;
            
            lights[1].pos = vec3(0, 4, 4);
            lights[1].color = vec3(1, 0.5f, 1);
            lights[1].radius = light_radius;

            lights[2].pos = vec3(0, 0, 4);
            lights[2].color = vec3(1, 1, 0.5f);
            lights[2].radius = light_radius;

            lights[3].pos = vec3(2, 2, 4);
            lights[3].color = vec3(0.5f, 1, 1);
            lights[3].radius = light_radius;

            //render blinn phong sphere
            {
                PBR_Light light = lights[0]; 
                Blinn_Phong_Params blinn_phong_params = {0};
                blinn_phong_params.light_ambient_strength = 0.01f;
                blinn_phong_params.light_specular_strength = 0.4f;
                blinn_phong_params.light_linear_attentuation = 0;
                blinn_phong_params.light_quadratic_attentuation = 0.00f;
                blinn_phong_params.light_specular_sharpness = 32;
                blinn_phong_params.light_specular_effect = 0.4f; 
                //blinn_phong_params.gamma = game_state->settings.screen_gamma;
                blinn_phong_params.gamma = 1.3f; //looks better on the wood texture

                Mat4 model = mat4_translate(mat4_rotation(vec3(2, 1, 3), clock_sf() / 8), vec3(5, 0, -5));
                render_mesh_draw_using_blinn_phong(render_cube_sphere, &shader_blinn_phong, projection, view, model, game_state->camera.pos, light.pos, light.color, blinn_phong_params, texture_floor);
            
                if(game_state->is_in_uv_debug_mode)
                    render_mesh_draw_using_uv_debug(render_cube_sphere, &shader_debug, projection, view, model);
            }

            
            //render pbr sphere
            {
                PBR_Params params = {0};
                for(isize i = 0; i < PBR_MAX_LIGHTS; i++)
                    params.lights[i] = lights[i];

                params.view_pos = game_state->camera.pos;
                params.gamma = game_state->settings.screen_gamma;
                params.attentuation_strength = 1.0f;
            
                f32 spacing = 3;
                f32 scale = 0.2f;
                isize iters = 7;
                Vec3 offset = {-2/scale, 0/scale, 2/scale};
                for(isize metalic_i = 0; metalic_i <= iters; metalic_i++)
                {
                    for(isize rough_i = 0; rough_i <= iters; rough_i++)
                    {
                        if(rough_i == iters)
                        {
                            isize x = 10;
                            (void) x;
                        }

                        f32 rough = (f32) rough_i / iters;
                        f32 metallic = (f32) metalic_i / iters;
                    
                        Render_PBR_Material material = {0};
                        material.solid_metallic = metallic;
                        material.solid_roughness = rough;
                        material.solid_albedo = vec3(1, 1, 0.92f);
                        material.solid_ao = default_ao;

                        Vec3 pos = vec3(spacing * rough_i, spacing*metalic_i, 0);
                        Mat4 model = mat4_scale_affine(mat4_translation(vec3_add(offset, pos)), vec3_of(scale));

                        if(use_mapping)
                            render_mesh_draw_using_pbr_mapped(render_uv_sphere, &shader_pbr_mapped, projection, view, model, &params, &material,
                                &cubemap_environment, &cubemap_irradiance, &cubemap_prefilter, &texture_brdf_lut);
                        else
                            render_mesh_draw_using_pbr(render_uv_sphere, &shader_pbr, projection, view, model, &params, &material);
                    }
                }

                Mat4 model = mat4_translate(mat4_scaling(vec3_of(0.5f)), vec3(0, 2, 5));
                //render_mesh_draw_using_pbr(render_uv_sphere, &shader_pbr, projection, view, model, &params, &material_metal);
                
                if(use_mapping)
                    render_mesh_draw_using_pbr_mapped(render_uv_sphere, &shader_pbr_mapped, projection, view, model, &params, &material_metal,
                        &cubemap_environment, &cubemap_irradiance, &cubemap_prefilter, &texture_brdf_lut);
                else
                    render_mesh_draw_using_pbr(render_uv_sphere, &shader_pbr, projection, view, model, &params, &material_metal);
            }
            

            //render light
            for(isize i = 0; i < PBR_MAX_LIGHTS; i++)
            {
                PBR_Light light = lights[i]; 
                Mat4 model = mat4_translate(mat4_scaling(vec3_of(light.radius)), light.pos);
                //render_mesh_draw_using_solid_color(render_uv_sphere, &shader_solid_color, projection, view, model, light.color, game_state->settings.screen_gamma);
                render_mesh_draw_using_solid_color(render_uv_sphere, &shader_solid_color, projection, view, model, light.color, 1);
            }

            //render gismos
            if(0)
            {
                f32 size = 0.05f;
                f32 length = 0.8f;
                f32 gap = 0.2f;
                f32 offset = gap + length/2;

                Mat4 X = mat4_translate(mat4_scaling(vec3(length, size, size)), vec3(offset, 0, 0));
                Mat4 Y = mat4_translate(mat4_scaling(vec3(size, length, size)), vec3(0, offset, 0));
                Mat4 Z = mat4_translate(mat4_scaling(vec3(size, size, length)), vec3(0, 0, offset));
                Mat4 MID = mat4_scaling(vec3(size, size, size));

                render_mesh_draw_using_solid_color(render_cube, &shader_solid_color, projection, view, X, vec3(1, 0, 0), game_state->settings.screen_gamma);
                render_mesh_draw_using_solid_color(render_cube, &shader_solid_color, projection, view, Y, vec3(0, 1, 0), game_state->settings.screen_gamma);
                render_mesh_draw_using_solid_color(render_cube, &shader_solid_color, projection, view, Z, vec3(0, 0, 1), game_state->settings.screen_gamma);
                render_mesh_draw_using_solid_color(render_cube, &shader_solid_color, projection, view, MID, vec3(1, 1, 1), game_state->settings.screen_gamma);
            }

            //render skybox
            {
                Mat4 model = mat4_scaling(vec3_of(-1));
                Mat4 stationary_view = mat4_from_mat3(mat3_from_mat4(view));
                    //render_mesh_draw_using_skybox(render_cube, &shader_skybox, projection, stationary_view, model, game_state->settings.screen_gamma, cubemap_environment);
                    //render_mesh_draw_using_skybox(render_cube, &shader_skybox, projection, stationary_view, model, 1.0f/game_state->settings.screen_gamma, cubemap_skybox);
                //else
                if(control_is_down(&game_state->controls, CONTROL_DEBUG_3))
                    render_mesh_draw_using_skybox(render_cube, &shader_skybox, projection, stationary_view, model, 1, cubemap_prefilter);
                else
                    render_mesh_draw_using_skybox(render_cube, &shader_skybox, projection, stationary_view, model, 1, cubemap_environment);
                    //render_mesh_draw_using_skybox(render_cube, &shader_skybox, projection, stationary_view, model, 1, cubemap_skybox);
            }

            render_screen_frame_buffers_msaa_render_end(&screen_buffers);
        }

        // ============== POST PROCESSING PASS ==================
        {
            render_screen_frame_buffers_msaa_post_process_begin(&screen_buffers);
            render_mesh_draw_using_postprocess(render_screen_quad, &shader_screen, screen_buffers.screen_color_buff, game_state->settings.screen_gamma, game_state->settings.screen_exposure);
            render_screen_frame_buffers_msaa_post_process_end(&screen_buffers);
        }

        //@HACK: this forsome reason depends on some calculation from the previous frame else it doesnt work dont know why
        if(control_was_pressed(&game_state->controls, CONTROL_REFRESH_ALL) 
            || control_was_pressed(&game_state->controls, CONTROL_REFRESH_ART)
            || frame_num == 0)
        {
            render_cubemap_init_prefilter_from_environment(
                &cubemap_prefilter, res_prefilter, res_prefilter,
                &cubemap_environment, &capture_buffers, &shader_prefilter, render_cube);
            render_texture_init_BRDF_LUT(
                &texture_brdf_lut, res_brdf_lut, res_brdf_lut,
                &capture_buffers, &shader_brdf_lut, render_quad);
            glViewport(0, 0, game_state->window_framebuffer_width, game_state->window_framebuffer_height); //@TODO stuff somehwere else!
        }

        f64 end_frame_time = clock_s();
        f64 frame_time = end_frame_time - start_frame_time;
        if(end_frame_time - fps_display_last_update > 1.0/fps_display_frequency)
        {
            fps_display_last_update = end_frame_time;
            format_into(&fps_display, "Game %5d fps", (int) (1.0f/frame_time));
            glfwSetWindowTitle(window, cstring_from_builder(fps_display));
        }
    }
    
    shape_deinit(&uv_sphere);
    shape_deinit(&cube_sphere);
    shape_deinit(&screen_quad);
    shape_deinit(&unit_cube);
    shape_deinit(&unit_quad);
    
    render_capture_buffers_deinit(&capture_buffers);

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
    render_shader_deinit(&shader_pbr);
    render_shader_deinit(&shader_debug);

    render_shader_deinit(&shader_equi_to_cubemap);
    render_shader_deinit(&shader_irradiance);
    render_shader_deinit(&shader_prefilter);
    render_shader_deinit(&shader_brdf_lut);
    render_shader_deinit(&shader_pbr_mapped);


    render_pbr_material_deinit(&material_metal);

    array_deinit(&fps_display);
    
    render_texture_deinit(&texture_floor);
    render_texture_deinit(&texture_debug);
    render_texture_deinit(&texture_environment);
    render_texture_deinit(&texture_brdf_lut);
    render_cubemap_deinit(&cubemap_skybox);
    render_cubemap_deinit(&cubemap_environment);
    render_cubemap_deinit(&cubemap_irradiance);
    render_cubemap_deinit(&cubemap_prefilter);

    render_screen_frame_buffers_msaa_deinit(&screen_buffers);

    debug_allocator_deinit(&debug_alloc);
    

    LOG_INFO("APP", "run_func exit");
}

void error_func(void* context, Platform_Sandox_Error error_code)
{
    (void) context;
    const char* msg = platform_sandbox_error_to_cstring(error_code);
    
    LOG_ERROR("APP", "%s exception occured", msg);
    LOG_TRACE("APP", "printing trace:");
    log_group_push();
    log_callstack(STRING("APP"), -1, 1);
    log_group_pop();
}

//@TODO: environment mapping for pbr
//       shadow mapping
//       resource management using debug_allocator

#include "_test_random.h"
#include "_test_array.h"
#include "_test_hash_index.h"
#include "_test_log.h"
#include "_test_hash_table.h"
#include "_test_math.h"
void run_test_func(void* context)
{
    (void) context;
    test_math(10.0);
    test_hash_table_stress(3.0);
    test_log();
    test_array(1.0);
    test_hash_index(1.0);
    test_random();
    LOG_INFO("TEST", "All tests passed! uwu");
    return;
}