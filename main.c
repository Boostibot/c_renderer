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
    u8 mouse[GLFW_MOUSE_LAST];
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

    f32 zoom_adjust_time;
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
    Debug_Allocator* allocator = (Debug_Allocator*) user;
    return allocator_try_reallocate(&allocator->allocator, size, NULL, 0, 8, SOURCE_INFO());
}

void* glfw_realloc_func(void* block, size_t size, void* user)
{
    Debug_Allocator* allocator = (Debug_Allocator*) user;
    Debug_Allocation allocation = debug_allocator_get_allocation(allocator, block); 
    void* out = allocator_try_reallocate(&allocator->allocator, size, allocation.ptr, allocation.size, 8, SOURCE_INFO());
    debug_allocator_deinit_allocation(&allocation);
    return out;
}

void glfw_free_func(void* block, void* user)
{
    Debug_Allocator* allocator = (Debug_Allocator*) user;
    Debug_Allocation allocation = debug_allocator_get_allocation(allocator, block); 
    allocator_try_reallocate(&allocator->allocator, 0, allocation.ptr, allocation.size, 8, SOURCE_INFO());
    debug_allocator_deinit_allocation(&allocation);
}

void glfw_error_func(int code, const char* description)
{
    LOG_ERROR("APP", "GLWF error %d with message: %s", code, description);
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
        look_dir = vec3_add(look_dir, camera.pos);
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

int main()
{
    platform_init();
    log_system_init(&global_malloc_allocator.allocator, &global_malloc_allocator.allocator);

    Debug_Allocator debug_allocator = {0};
    debug_allocator_init_use(&debug_allocator, DEBUG_ALLOCATOR_DEINIT_LEAK_CHECK);

    GLFWallocator allocator = {0};
    allocator.allocate = glfw_malloc_func;
    allocator.reallocate = glfw_realloc_func;
    allocator.deallocate = glfw_free_func;
    allocator.user = &debug_allocator;
 
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
 
    //Create window set it as context and set its resize event
    GLFWwindow* window = glfwCreateWindow(1600, 900, "title", NULL, NULL);
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

    debug_allocator_deinit(&debug_allocator);

    log_system_deinit();
    platform_deinit();

    return 0;    
}

typedef struct Screen_Frame_Buffers
{
    GLuint frame_buff;
    GLuint color_buff;
    GLuint render_buff;
} Screen_Frame_Buffers;

void screen_frame_buffers_init(Screen_Frame_Buffers* buffer, i32 width, i32 height)
{
    memset(buffer, 0, sizeof *buffer);

    LOG_INFO("RENDR", "screen_frame_buffers_init %-4d x %-4d", width, height);

    //@NOTE: 
    //The lack of the following line caused me 2 hours of debugging why my application crashed due to NULL ptr 
    //deref in glDrawArrays. I still dont know why this occurs but just for safety its better to leave this here.
    glBindVertexArray(0);

    glGenFramebuffers(1, &buffer->frame_buff);
    glBindFramebuffer(GL_FRAMEBUFFER, buffer->frame_buff);    

    // generate texture
    glGenTextures(1, &buffer->color_buff);
    glBindTexture(GL_TEXTURE_2D, buffer->color_buff);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB32F, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR );
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glBindTexture(GL_TEXTURE_2D, 0);

    // attach it to currently bound screen_frame_buffers object
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, buffer->color_buff, 0);

    glGenRenderbuffers(1, &buffer->render_buff);
    glBindRenderbuffer(GL_RENDERBUFFER, buffer->render_buff); 
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, width, height);  
    glBindRenderbuffer(GL_RENDERBUFFER, 0);

    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, buffer->render_buff);

    if(glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
       TEST_MSG(false, "frame buffer creation failed!"); 

    glDisable(GL_FRAMEBUFFER_SRGB);
    glBindFramebuffer(GL_FRAMEBUFFER, 0); 
}

void screen_frame_buffers_deinit(Screen_Frame_Buffers* buffer)
{
    glBindVertexArray(0);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    glDeleteFramebuffers(1, &buffer->frame_buff);
    glDeleteBuffers(1, &buffer->color_buff);
    glDeleteRenderbuffers(1, &buffer->render_buff);

    memset(buffer, 0, sizeof *buffer);
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

typedef struct Bitmap_View
{
    u8* data;
    i32 width;
    i32 height;
    u8 channels;

    i32 from_x;
    i32 to_x;
    
    i32 from_y;
    i32 to_y;
} Bitmap_View;

typedef struct Render_Texture
{
    GLuint texture;
    isize width;
    isize height;
} Render_Texture;

typedef struct Render_Cubemap
{
    GLuint texture;

    isize widths[6];
    isize heights[6];
} Render_Cubemap;

void render_texture_deinit(Render_Texture* render);

void render_texture_init(Render_Texture* render, Bitmap_View bitmap)
{
    GLenum format = GL_RGB;
    switch(bitmap.channels)
    {
        case 1: format = GL_R; break;
        case 2: format = GL_RG; break;
        case 3: format = GL_RGB; break;
        case 4: format = GL_RGBA; break;
        default: format = GL_RGB; break;
    }

    render_texture_deinit(render);
    render->height = bitmap.height;
    render->width = bitmap.width;
    glGenTextures(1, &render->texture);
    glBindTexture(GL_TEXTURE_2D, render->texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);	
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexImage2D(GL_TEXTURE_2D, 0, format, (GLsizei) bitmap.width, (GLsizei) bitmap.height, 0, format, GL_UNSIGNED_BYTE, bitmap.data);
    glGenerateMipmap(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, 0);
} 


void render_texture_use(Render_Texture* render, isize slot)
{
    glActiveTexture(GL_TEXTURE0 + (GLenum) slot);
    glBindTexture(GL_TEXTURE_2D, render->texture);
}

void render_texture_unuse(Render_Texture* render)
{
    (void) render;
    glBindTexture(GL_TEXTURE_2D, 0);
}

void render_texture_deinit(Render_Texture* render)
{
    glDeleteTextures(1, &render->texture);
    memset(&render, 0, sizeof(render));
}

void render_cubemap_deinit(Render_Cubemap* render)
{
    glDeleteTextures(1, &render->texture);
    memset(&render, 0, sizeof(render));
}

void render_cubemap_init(Render_Cubemap* render, const Bitmap_View bitmaps[6])
{
    for (isize i = 0; i < 6; i++)
        ASSERT(bitmaps[i].data != NULL && "cannot miss data");

    render_cubemap_deinit(render);
    glGenTextures(1, &render->texture);
    glBindTexture(GL_TEXTURE_CUBE_MAP, render->texture);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);	
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);

    for (isize i = 0; i < 6; i++)
    {
        Bitmap_View bitmap = bitmaps[i];
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

void render_cubemap_use(Render_Cubemap* render, isize slot)
{
    glActiveTexture(GL_TEXTURE0 + (GLenum) slot);
    glBindTexture(GL_TEXTURE_CUBE_MAP, render->texture);
}

void render_cubemap_unuse(Render_Cubemap* render)
{
    (void) render;
    glBindTexture(GL_TEXTURE_CUBE_MAP, 0);
}

void bitmap_deinit(Bitmap_View* bitmap)
{
    stbi_image_free(bitmap->data);
    memset(bitmap, 0, sizeof *bitmap);
}

bool bitmap_init_from_disk(Bitmap_View* bitmap, String path, bool flip)
{
    bitmap_deinit(bitmap);

    String_Builder escaped = {0};
    array_init_backed(&escaped, allocator_get_scratch(), 256);
    builder_append(&escaped, path);

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

bool render_texture_init_from_disk(Render_Texture* tetxure, String path)
{
    Bitmap_View bitmap = {0};
    bool state = bitmap_init_from_disk(&bitmap, path, true);
    render_texture_init(tetxure, bitmap);
    bitmap_deinit(&bitmap);

    return state;
}

bool render_cubemap_init_from_disk(Render_Cubemap* render, String front, String back, String top, String bot, String right, String left)
{
    Bitmap_View face_bitmaps[6] = {0};
    String face_paths[6] = {right, left, top, bot, front, back};

    bool state = true;
    for (isize i = 0; i < 6; i++)
        state = state && bitmap_init_from_disk(&face_bitmaps[i], face_paths[i], false);

    render_cubemap_init(render, face_bitmaps);
    
    for (isize i = 0; i < 6; i++)
        bitmap_deinit(&face_bitmaps[i]);

    return state;
}

bool shader_set_texture(GLuint program, const char* name, Render_Texture texture)
{
    int location = _get_shader_uniform_location(program, name);
    if(location == -1)
        return false;

    glUniform1i(location, texture.texture);
    return true;
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

    //glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, pos));
    //glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, norm));
    //glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, tan));
    //glVertexAttribPointer(3, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, bitan));
    //glVertexAttribPointer(4, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, uv));

    //glEnableVertexAttribArray(0);	
    //glEnableVertexAttribArray(1);	
    //glEnableVertexAttribArray(2);	
    //glEnableVertexAttribArray(3);	
    //glEnableVertexAttribArray(4);
    
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

void render_mesh_draw_using_solid_color(Render_Mesh mesh, GLuint shader_depth_color, Mat4 projection, Mat4 view, Mat4 model, Vec3 color)
{
    shader_use(shader_depth_color);
    shader_set_vec3(shader_depth_color, "color", color);
    shader_set_mat4(shader_depth_color, "projection", projection);
    shader_set_mat4(shader_depth_color, "view", view);
    shader_set_mat4(shader_depth_color, "model", model);
    render_mesh_draw(mesh);
    shader_unuse();
}

void render_mesh_draw_using_depth_color(Render_Mesh mesh, GLuint shader_depth_color, Mat4 projection, Mat4 view, Mat4 model, Vec3 color)
{
    shader_use(shader_depth_color);
    shader_set_vec3(shader_depth_color, "color", color);
    shader_set_mat4(shader_depth_color, "projection", projection);
    shader_set_mat4(shader_depth_color, "view", view);
    shader_set_mat4(shader_depth_color, "model", model);
    render_mesh_draw(mesh);
    shader_unuse();
}

void render_mesh_draw_using_uv_debug(Render_Mesh mesh, GLuint uv_shader_debug, Mat4 projection, Mat4 view, Mat4 model)
{
    shader_use(uv_shader_debug);
    Mat4 inv = mat4_inverse_nonuniform_scale(model);
    Mat3 normal_matrix = mat3_from_mat4(inv);
    normal_matrix = mat3_from_mat4(model);

    shader_set_mat4(uv_shader_debug, "projection", projection);
    shader_set_mat4(uv_shader_debug, "view", view);
    shader_set_mat4(uv_shader_debug, "model", model);
    shader_set_mat3(uv_shader_debug, "normal_matrix", normal_matrix);
    render_mesh_draw(mesh);
    shader_unuse();
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

void render_mesh_draw_using_blinn_phong(Render_Mesh mesh, GLuint blin_phong_shader, Mat4 projection, Mat4 view, Mat4 model, Vec3 view_pos, Vec3 light_pos, Vec3 light_color, Blinn_Phong_Params params, Render_Texture diffuse)
{
    shader_use(blin_phong_shader);
    Mat4 inv = mat4_inverse_nonuniform_scale(model);
    Mat3 normal_matrix = mat3_from_mat4(inv);
    normal_matrix = mat3_from_mat4(model);

    shader_set_mat4(blin_phong_shader, "projection", projection);
    shader_set_mat4(blin_phong_shader, "view", view);
    shader_set_mat4(blin_phong_shader, "model", model);
    shader_set_mat3(blin_phong_shader, "normal_matrix", normal_matrix);

    shader_set_vec3(blin_phong_shader, "view_pos", view_pos);
    shader_set_vec3(blin_phong_shader, "light_pos", light_pos);
    shader_set_vec3(blin_phong_shader, "light_color", light_color);
    
    shader_set_f32(blin_phong_shader, "light_ambient_strength", params.light_ambient_strength);
    shader_set_f32(blin_phong_shader, "light_specular_strength", params.light_specular_strength);
    shader_set_f32(blin_phong_shader, "light_specular_sharpness", params.light_specular_sharpness);
    shader_set_f32(blin_phong_shader, "light_ambient_strength", params.light_ambient_strength);
    shader_set_f32(blin_phong_shader, "light_ambient_strength", params.light_ambient_strength);
    shader_set_f32(blin_phong_shader, "light_specular_effect", params.light_specular_effect);
    shader_set_f32(blin_phong_shader, "gamma", params.gamma);

    render_texture_use(&diffuse, 0);
    shader_set_i32(blin_phong_shader, "texture_diffuse", 0);
    
    render_mesh_draw(mesh);
    render_texture_unuse(&diffuse);
    shader_unuse();
}

void render_mesh_draw_using_skybox(Render_Mesh mesh, GLuint skybox_shader, Mat4 projection, Mat4 view, Mat4 model, f32 gamma, Render_Cubemap skybox)
{
    glDepthFunc(GL_LEQUAL);
    shader_use(skybox_shader);
    shader_set_mat4(skybox_shader, "projection", projection);
    shader_set_mat4(skybox_shader, "view", view);
    shader_set_mat4(skybox_shader, "model", model);
    shader_set_f32(skybox_shader, "gamma", gamma);
    render_mesh_draw(mesh);
    
    render_cubemap_use(&skybox, 0);
    shader_set_i32(skybox_shader, "cubemap_diffuse", 0);
    
    render_mesh_draw(mesh);
    render_cubemap_unuse(&skybox);
    shader_unuse();
    glDepthFunc(GL_LESS);
}

void run_func(void* context)
{
    LOG_INFO("APP", "run_func enter");
    
    Debug_Allocator debug_alloc = {0};
    debug_allocator_init_use(&debug_alloc, DEBUG_ALLOCATOR_DEINIT_LEAK_CHECK);

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
    game_state->settings.screen_exposure = 1.0;
    game_state->settings.mouse_sensitivity = 0.002f;
    game_state->settings.mouse_wheel_sensitivity = 0.05f; // uwu
    game_state->settings.zoom_adjust_time = 0.2f;

    mapping_make_default(game_state->control_mappings);

    Screen_Frame_Buffers screen_buffers = {0};

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

    Render_Cubemap cubemap_skybox = {0};

    GLuint shader_depth_color = 0;
    GLuint shader_solid_color = 0;
    GLuint shader_screen = 0;
    GLuint shader_debug = 0;
    GLuint shader_blinn_phong = 0;
    GLuint shader_skybox = 0;

    f64 fps_display_frequency = 4;
    f64 fps_display_last_update = 0;
    String_Builder fps_display = {0};
    for(bool first_run = true; game_state->should_close == false; first_run = false)
    {
        f64 start_frame_time = clock_s();
        game_state->delta_time = start_frame_time - game_state->last_frame_timepoint; 
        game_state->last_frame_timepoint = start_frame_time; 

        window_process_input(window, first_run);
        if(game_state->window_framebuffer_width != game_state->window_framebuffer_width_prev 
            || game_state->window_framebuffer_height != game_state->window_framebuffer_height_prev
            || first_run)
        {
            LOG_INFO("APP", "Resizing");
            screen_frame_buffers_deinit(&screen_buffers);
            screen_frame_buffers_init(&screen_buffers, game_state->window_framebuffer_width, game_state->window_framebuffer_height);
            glViewport(0, 0, game_state->window_framebuffer_width, game_state->window_framebuffer_height);
        }
        
        if(control_was_pressed(&game_state->controls, CONTROL_REFRESH_ALL) 
            || control_was_pressed(&game_state->controls, CONTROL_REFRESH_SHADERS)
            || first_run)
        {
            LOG_INFO("APP", "Refreshing shaders");
            shader_unload(&shader_solid_color);
            shader_unload(&shader_depth_color);
            shader_unload(&shader_screen);
            shader_unload(&shader_debug);
            shader_unload(&shader_blinn_phong);
            shader_unload(&shader_skybox);

            shader_solid_color = shader_load(STRING("shaders/solid_color.vert"), STRING("shaders/solid_color.frag"), STRING(""), STRING(""), NULL);
            shader_depth_color = shader_load(STRING("shaders/depth_color.vert"), STRING("shaders/depth_color.frag"), STRING(""), STRING(""), NULL);
            shader_screen = shader_load(STRING("shaders/screen.vert"), STRING("shaders/screen.frag"), STRING(""), STRING(""), NULL);
            shader_debug = shader_load(STRING("shaders/uv_debug.vert"), STRING("shaders/uv_debug.frag"), STRING("shaders/uv_debug.geom"), STRING(""), NULL);
            shader_blinn_phong = shader_load(STRING("shaders/blinn_phong.vert"), STRING("shaders/blinn_phong.frag"), STRING(""), STRING(""), NULL);
            shader_skybox = shader_load(STRING("shaders/skybox.vert"), STRING("shaders/skybox.frag"), STRING(""), STRING(""), NULL);
        }
        
        if(control_was_pressed(&game_state->controls, CONTROL_REFRESH_ALL) 
            || control_was_pressed(&game_state->controls, CONTROL_REFRESH_ART)
            || first_run)
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
    
            render_mesh_deinit(&render_uv_sphere);
            render_mesh_deinit(&render_cube_sphere);
            render_mesh_deinit(&render_screen_quad);
            render_mesh_deinit(&render_cube);
            render_mesh_deinit(&render_quad);

            render_mesh_init(&render_uv_sphere, uv_sphere.vertices.data, uv_sphere.vertices.size, uv_sphere.indeces.data, uv_sphere.indeces.size, STRING("uv_sphere"));
            render_mesh_init(&render_cube_sphere, cube_sphere.vertices.data, cube_sphere.vertices.size, cube_sphere.indeces.data, cube_sphere.indeces.size, STRING("cube_sphere"));
            render_mesh_init(&render_screen_quad, screen_quad.vertices.data, screen_quad.vertices.size, screen_quad.indeces.data, screen_quad.indeces.size, STRING("screen_quad"));
            render_mesh_init(&render_cube, unit_cube.vertices.data, unit_cube.vertices.size, unit_cube.indeces.data, unit_cube.indeces.size, STRING("unit_cube"));
            render_mesh_init(&render_quad, unit_quad.vertices.data, unit_quad.vertices.size, unit_quad.indeces.data, unit_quad.indeces.size, STRING("unit_cube"));

            render_texture_deinit(&texture_floor);
            render_texture_deinit(&texture_debug);

            bool load_okay = true;
            load_okay = load_okay && render_texture_init_from_disk(&texture_floor, STRING("resources/floor.jpg"));
            load_okay = load_okay && render_texture_init_from_disk(&texture_debug, STRING("resources/debug.png"));
            load_okay = load_okay && render_cubemap_init_from_disk(&cubemap_skybox, 
                STRING("resources/skybox_front.jpg"), 
                STRING("resources/skybox_back.jpg"), 
                STRING("resources/skybox_top.jpg"), 
                STRING("resources/skybox_bottom.jpg"), 
                STRING("resources/skybox_right.jpg"), 
                STRING("resources/skybox_left.jpg"));

            ASSERT(load_okay);
        }
        
        if(control_was_pressed(&game_state->controls, CONTROL_ESCAPE))
        {
            game_state->is_in_mouse_mode = !game_state->is_in_mouse_mode;
        }
        
        if(control_was_pressed(&game_state->controls, CONTROL_DEBUG_1))
        {
            game_state->is_in_uv_debug_mode = !game_state->is_in_uv_debug_mode;
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

                    xoffset *= game_state->settings.mouse_sensitivity * fov_sens_modifier;
                    yoffset *= game_state->settings.mouse_sensitivity * fov_sens_modifier;
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

        //@TODO: PBR rendering

        game_state->camera.fov = game_state->settings.fov;
        game_state->camera.near = 0.1f;
        game_state->camera.far = 100.0f;
        game_state->camera.is_ortographic = false;
        game_state->camera.is_position_relative = true;
        game_state->camera.aspect_ratio = (f32) game_state->window_framebuffer_width / (f32) game_state->window_framebuffer_height;

        Mat4 view = camera_make_view_matrix(game_state->camera);
        Mat4 projection = camera_make_projection_matrix(game_state->camera);
        
        //================ FIRST PASS ==================
        glBindFramebuffer(GL_FRAMEBUFFER, screen_buffers.frame_buff); 
        glClearColor(0.3f, 0.3f, 0.2f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        
        glEnable(GL_DEPTH_TEST);
        glEnable(GL_CULL_FACE);
        glCullFace(GL_FRONT); 
        glFrontFace(GL_CW); 
        
        Blinn_Phong_Params blinn_phong_params = {0};
        blinn_phong_params.light_ambient_strength = 0.01f;
        blinn_phong_params.light_specular_strength = 0.4f;
        blinn_phong_params.light_linear_attentuation = 0;
        blinn_phong_params.light_quadratic_attentuation = 0.00f;
        blinn_phong_params.light_specular_sharpness = 32;
        blinn_phong_params.light_specular_effect = 0.4f; 
        //blinn_phong_params.gamma = game_state->settings.screen_gamma;
        blinn_phong_params.gamma = 1.3f; //looks better on the wood texture

        Vec3 light_pos = vec3(10*sinf(clock_sf()), 10*cosf(clock_sf()), 0);
        light_pos = vec3(4, 4, 0);
        Vec3 light_color = vec3(1, 1, 0.9f);

        //render sphere
        {
            Mat4 model = mat4_translate(mat4_rotation(vec3(2, 1, 3), clock_sf() / 8), vec3(5, 0, 5));
            render_mesh_draw_using_blinn_phong(render_cube_sphere, shader_blinn_phong, projection, view, model, game_state->camera.pos, light_pos, light_color, blinn_phong_params, texture_floor);
            
            if(game_state->is_in_uv_debug_mode)
                render_mesh_draw_using_uv_debug(render_cube_sphere, shader_debug, projection, view, model);
        }

        //render light
        {
            Mat4 model = mat4_scale_affine(mat4_translation(light_pos), vec3_of(0.3f));
            render_mesh_draw_using_solid_color(render_uv_sphere, shader_solid_color, projection, view, model, light_color);
        }

        //render gismos
        {
            f32 size = 0.05f;
            f32 length = 1;
            f32 offset = size + 0.3f + length/2;

            Mat4 X = mat4_scale_affine(mat4_translation(vec3(offset, 0, 0)), vec3(length, size, size));
            Mat4 Y = mat4_scale_affine(mat4_translation(vec3(0, offset, 0)), vec3(size, length, size));
            Mat4 Z = mat4_scale_affine(mat4_translation(vec3(0, 0, offset)), vec3(size, size, length));
            Mat4 MID = mat4_scaling(vec3(size, size, size));

            render_mesh_draw_using_solid_color(render_cube, shader_solid_color, projection, view, X, vec3(1, 0, 0));
            render_mesh_draw_using_solid_color(render_cube, shader_solid_color, projection, view, Y, vec3(0, 1, 0));
            render_mesh_draw_using_solid_color(render_cube, shader_solid_color, projection, view, Z, vec3(0, 0, 1));
            render_mesh_draw_using_solid_color(render_cube, shader_solid_color, projection, view, MID, vec3(1, 1, 1));
        }

        //render skybox
        {
            Mat4 model = mat4_scaling(vec3_of(-1));
            Mat4 stationary_view = mat4_from_mat3(mat3_from_mat4(view));
            render_mesh_draw_using_skybox(render_cube, shader_skybox, projection, stationary_view, model, game_state->settings.screen_gamma, cubemap_skybox);
        }

        {
            // ============== POST PROCESSING PASS ==================
            glBindFramebuffer(GL_FRAMEBUFFER, 0); // back to default
            glDisable(GL_DEPTH_TEST);
            glDisable(GL_CULL_FACE);

            {
                shader_use(shader_screen);
                shader_set_i32(shader_screen, "screen", 0);
                shader_set_f32(shader_screen, "gamma", game_state->settings.screen_gamma);
                shader_set_f32(shader_screen, "exposure", game_state->settings.screen_exposure);

                glActiveTexture(GL_TEXTURE0);
                glBindTexture(GL_TEXTURE_2D, screen_buffers.color_buff);
            
                render_mesh_draw(render_screen_quad);
            }
        }

        f64 end_frame_time = clock_s();
        f64 frame_time = end_frame_time - start_frame_time;
        if(end_frame_time - fps_display_last_update > 1.0/fps_display_frequency)
        {
            fps_display_last_update = end_frame_time;
            array_clear(&fps_display);
            format_into(&fps_display, "Game %5d fps", (int) (1.0f/frame_time));
            glfwSetWindowTitle(window, cstring_from_builder(fps_display));
        }

        glfwSwapBuffers(window);
        
    }

    //@TODO: deinit!

    LOG_INFO("APP", "run_func exit");
}
