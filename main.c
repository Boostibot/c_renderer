#define LIB_ALL_IMPL
#define LIB_MEM_DEBUG
#define GLAD_GL_IMPLEMENTATION

#include "platform.h"
#include "string.h"
#include "hash_index.h"
#include "log.h"
#include "file.h"
#include "allocator_debug.h"
#include "random.h"
#include "math.h"

#include "glad2/gl.h"
#include "glfw/glfw3.h"



typedef struct Inputs
{
    uint8_t keys[256];  
} Inputs;

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
    CONTROL_MOVE_LEFT,
    CONTROL_MOVE_RIGHT,
    CONTROL_MOVE_UP,
    CONTROL_MOVE_DOWN,

    CONTROL_JUMP,
    CONTROL_INTERACT,
    CONTROL_ESCAPE,
    CONTROL_PAUSE,
    CONTROL_SPRINT,
    
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

#define CONTROL_MAPPING_SETS 3

//Maps physical inputs to abstract game actions
//each entry can be assigned to one action.
//values are interpreted as Control_Name
//A key can be assigned to multiple actions simultaneously
// by duplicating this structure
typedef struct Control_Mapping
{
    uint8_t mouse_x;
    uint8_t mouse_y;
    uint8_t mouse_scroll;

    //Joystick and gamepads later....
    uint8_t mouse_buttons[GLFW_MOUSE_BUTTON_LAST];
    uint8_t keys[GLFW_KEY_LAST];
} Control_Mapping;


//An abstarct set of game actions to be used by the game code
//we represent all values as ammount and the number of interactions
//this allows us to rebind pretty much everything to everything including 
//keys to control camera and mouse to control movement.
//This is useful for example because controllers use smooth controls for movement while keyboard does not
//The values range from -1 to 1 where 0 indicates off and nonzero indicates on. 
typedef struct Controls
{
    f32 values[CONTROL_COUNT];
    uint8_t interactions[CONTROL_COUNT];
} Controls;

typedef struct Camera
{
    int x;
} Camera;

typedef struct Game_State
{
    Vec3 camera_pos;
    Vec3 camera_front_dir;
    Vec3 camera_up_dir;
    Vec3 active_object_pos;

    f32 movement_speed;
    f32 movement_sprint_mult;

    f64 delta_time;
    f64 last_frame_timepoint;
    f32 screen_gamma;
    f32 screen_exposure;

    f32 mouse_sensitivity;
    f32 mouse_wheel_sensitivity;

    f32 camera_yaw;
    f32 camera_pitch;
    f32 camera_fov;
    f32 heigh_scale;

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

    Controls controls;
    Controls controls_prev;
    Control_Mapping control_mappings[CONTROL_MAPPING_SETS];
} Game_State;

void error_func(void* context, Platform_Sandox_Error error_code);
void run_test_func(void* context);

void mapping_make_default(Control_Mapping mappings[CONTROL_MAPPING_SETS]);
void game_state_init(Game_State* state);

void window_process_input(GLFWwindow* window, bool is_initial_call);

//there must not be 2 or more of the same control on the same physical input device
bool mapping_check(Control_Mapping mappings[CONTROL_MAPPING_SETS])
{
    (void) mappings;
    return true;
}

void run_func(void* context)
{
    GLFWwindow* window = (GLFWwindow*) context;
    Game_State* game_state = (Game_State*) glfwGetWindowUserPointer(window);
    
    game_state_init(game_state);

    mapping_make_default(game_state->control_mappings);
    window_process_input(window, true);

    while(game_state->should_close == false)
    {
        if(game_state->controls.interactions[CONTROL_ESCAPE] && game_state->controls.values[CONTROL_ESCAPE] > 0)
            LOG_INFO("APP", "escaped interacted with %d and value %f", (int) game_state->controls.interactions[CONTROL_ESCAPE], game_state->controls.values[CONTROL_ESCAPE]);
            
        //if(game_state->controls.values[CONTROL_ESCAPE] > 0)

        //platform_thread_sleep(500);
        window_process_input(window, false);
    }
}

void mapping_make_default(Control_Mapping mappings[CONTROL_MAPPING_SETS])
{
    memset(mappings, 0, sizeof(Control_Mapping) * CONTROL_MAPPING_SETS);

    mappings[1].mouse_x = CONTROL_MOUSE_X;
    mappings[1].mouse_y = CONTROL_MOUSE_Y;

    mappings[1].mouse_scroll = CONTROL_SCROLL;
    
    mappings[0].mouse_x = CONTROL_LOOK_X;
    mappings[0].mouse_y = CONTROL_LOOK_Y;
    
    mappings[0].mouse_scroll = CONTROL_ZOOM;

    mappings[0].keys[GLFW_KEY_W] = CONTROL_MOVE_FORWARD;
    mappings[0].keys[GLFW_KEY_S] = CONTROL_MOVE_BACKWARD;
    mappings[0].keys[GLFW_KEY_A] = CONTROL_MOVE_LEFT;
    mappings[0].keys[GLFW_KEY_D] = CONTROL_MOVE_RIGHT;
    mappings[0].keys[GLFW_KEY_SPACE] = CONTROL_MOVE_UP;
    mappings[0].keys[GLFW_KEY_LEFT_SHIFT] = CONTROL_MOVE_DOWN;
    
    mappings[1].keys[GLFW_KEY_UP] = CONTROL_MOVE_FORWARD;
    mappings[1].keys[GLFW_KEY_DOWN] = CONTROL_MOVE_BACKWARD;
    mappings[1].keys[GLFW_KEY_LEFT] = CONTROL_MOVE_LEFT;
    mappings[1].keys[GLFW_KEY_RIGHT] = CONTROL_MOVE_RIGHT;

    
    mappings[1].keys[GLFW_KEY_SPACE] = CONTROL_JUMP;
    mappings[0].keys[GLFW_KEY_E] = CONTROL_INTERACT;
    mappings[0].keys[GLFW_KEY_ESCAPE] = CONTROL_ESCAPE;
    mappings[1].keys[GLFW_KEY_ESCAPE] = CONTROL_PAUSE;
    mappings[0].keys[GLFW_KEY_LEFT_ALT] = CONTROL_SPRINT;

    
    mappings[0].keys[GLFW_KEY_F1] = CONTROL_DEBUG_1;
    mappings[0].keys[GLFW_KEY_F2] = CONTROL_DEBUG_2;
    mappings[0].keys[GLFW_KEY_F3] = CONTROL_DEBUG_3;
    mappings[0].keys[GLFW_KEY_F4] = CONTROL_DEBUG_4;
    mappings[0].keys[GLFW_KEY_F5] = CONTROL_DEBUG_5;
    mappings[0].keys[GLFW_KEY_F6] = CONTROL_DEBUG_6;
    mappings[0].keys[GLFW_KEY_F7] = CONTROL_DEBUG_7;
    mappings[0].keys[GLFW_KEY_F8] = CONTROL_DEBUG_8;
    mappings[0].keys[GLFW_KEY_F9] = CONTROL_DEBUG_9;
    mappings[0].keys[GLFW_KEY_F10] = CONTROL_DEBUG_10;
}

void game_state_init(Game_State* state)
{
    memset(state, 0, sizeof *state);

    state->camera_yaw = -PI/2.0f;
    state->camera_pitch = 0.0f;
    state->camera_fov = PI/4.0f;
    state->camera_pos       = VEC3(0.0f, 0.0f,  3.0f);
    state->camera_front_dir = VEC3(1.0f, 0.0f,  0.0f);
    state->camera_up_dir    = VEC3(0.0f, 1.0f,  0.0f);
    state->active_object_pos = VEC3(0, 0, 0);

    state->movement_speed = 2.5f;
    state->movement_sprint_mult = state->movement_speed * 5;

    state->delta_time = 0.0f;
    state->last_frame_timepoint = 0.0f;
    state->screen_gamma = 2.2f;
    state->screen_exposure = 1.0;

    state->mouse_sensitivity = 0.002f;
    state->mouse_wheel_sensitivity = 0.01f; // uwu
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
        game_state->is_in_mouse_mode = false;
    }
    
    memset(&game_state->controls.interactions, 0, sizeof game_state->controls.interactions);
    glfwPollEvents();
    f64 time = clock_s();
    game_state->should_close = glfwWindowShouldClose(window);
    game_state->delta_time = time - game_state->last_frame_timepoint; 
    game_state->last_frame_timepoint = time; 
    
    STATIC_ASSERT(sizeof(int) == sizeof(i32));

    game_state->window_screen_width_prev = game_state->window_screen_width;
    game_state->window_screen_height_prev = game_state->window_screen_height;
    glfwGetWindowSize(window, (int*) &game_state->window_screen_width, (int*) &game_state->window_screen_height);

    game_state->window_framebuffer_width_prev = game_state->window_framebuffer_width;
    game_state->window_framebuffer_height_prev = game_state->window_framebuffer_height;
    glfwGetFramebufferSize(window, (int*) &game_state->window_framebuffer_width, (int*) &game_state->window_framebuffer_height);
    
    //game_state->window_screen_width = MAX(game_state->window_screen_width, 1);
    //game_state->window_screen_height = MAX(game_state->window_screen_height, 1);
    game_state->window_framebuffer_width = MAX(game_state->window_framebuffer_width, 1);
    game_state->window_framebuffer_height = MAX(game_state->window_framebuffer_height, 1);

    f64 new_mouse[2] = {0};
    glfwGetCursorPos(window, &new_mouse[0], &new_mouse[1]);

    for(isize i = 0; i < CONTROL_MAPPING_SETS; i++)
    {
        Control_Mapping* mapping = &game_state->control_mappings[i];
        u8 controls[2] = {mapping->mouse_x, mapping->mouse_y};
        for(isize k = 0; k < 2; k++)
        {
            u8 control = controls[k];
            f32 mouse = (f32) new_mouse[k];

            if(control != CONTROL_NONE)
            {
                if(game_state->controls.interactions[control] < UINT8_MAX)
                    game_state->controls.interactions[control] ++;
                game_state->controls.values[control] = mouse;
            }
        }
    }
    
    if(game_state->is_in_mouse_mode_prev != game_state->is_in_mouse_mode)
    {
        if(game_state->is_in_mouse_mode == false)
        {
            glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL); 
        }
        else
        {
            if(glfwRawMouseMotionSupported())
                glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED); 
        }
    }

    game_state->controls_prev = game_state->controls;
    game_state->is_in_mouse_mode_prev = game_state->is_in_mouse_mode;
    
}

void glfw_key_func(GLFWwindow* window, int key, int scancode, int action, int mods)
{
    (void) mods;
    (void) scancode;
    f32 to_value = 0.0f;
    if(action == GLFW_PRESS)
    {
        to_value = 1.0f;
    }
    else if(action == GLFW_RELEASE)
    {
        to_value = 0.0f;
    }
    else
    {
        return;
    }


    Game_State* game_state = (Game_State*) glfwGetWindowUserPointer(window);
    for(isize i = 0; i < CONTROL_MAPPING_SETS; i++)
    {
        Control_Mapping* mapping = &game_state->control_mappings[i];
        u8 control = mapping->keys[key];
        if(control != CONTROL_NONE)
        {
            if(game_state->controls.interactions[control] < UINT8_MAX)
                game_state->controls.interactions[control] ++;
            game_state->controls.values[control] = to_value;
        }
    }
}

void glfw_mouse_button_func(GLFWwindow* window, int button, int action, int mods)
{
    (void) mods;
    f32 to_value = 0.0f;
    if(action == GLFW_PRESS)
        to_value = 1.0f;
    else if(action == GLFW_RELEASE)
        to_value = 0.0f;
    else
        return;

    Game_State* game_state = (Game_State*) glfwGetWindowUserPointer(window);
    for(isize i = 0; i < CONTROL_MAPPING_SETS; i++)
    {
        Control_Mapping* mapping = &game_state->control_mappings[i];
        u8 control = mapping->mouse_buttons[button];
        if(control != CONTROL_NONE)
        {
            if(game_state->controls.interactions[control] < UINT8_MAX)
                game_state->controls.interactions[control] ++;
            game_state->controls.values[control] = to_value;
        }
    }
}

void glfw_scroll_func(GLFWwindow* window, f64 xoffset, f64 yoffset)
{   
    (void) xoffset;
    Game_State* game_state = (Game_State*) glfwGetWindowUserPointer(window);
    for(isize i = 0; i < CONTROL_MAPPING_SETS; i++)
    {
        Control_Mapping* mapping = &game_state->control_mappings[i];
        u8 control = mapping->mouse_scroll;
        if(control != CONTROL_NONE)
        {
            if(game_state->controls.interactions[control] < UINT8_MAX)
                game_state->controls.interactions[control] ++;
            game_state->controls.values[control] += (f32) yoffset;
        }
    }
}

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
void run_test_func(void* context)
{
    (void) context;
    test_hash_table_stress(3.0);
    test_log();
    test_array(1.0);
    test_hash_index(1.0);
    test_random();
    LOG_INFO("TEST", "All tests passed! uwu");
    return;
}