//#define RUN_TESTS

#define _CRT_SECURE_NO_WARNINGS
#pragma warning(disable:4464) //Dissable "relative include path contains '..'"
#pragma warning(disable:4702) //Dissable "unrelachable code"
#pragma warning(disable:4820) //Dissable "Padding added to struct" 
#pragma warning(disable:4255) //Dissable "no function prototype given: converting '()' to '(void)"  
#pragma warning(disable:5045) //Dissable "Compiler will insert Spectre mitigation for memory load if /Qspectre switch specified"  
#pragma warning(disable:4201) //Dissable "nonstandard extension used: nameless struct/union" 
#pragma warning(disable:4189) //Dissable "local variable is initialized but not referenced" (for assert macro)
#pragma warning(disable:4296) //Dissable "expression is always true" (used for example in 0 <= val && val <= max where val is unsigned. This is used in generic checking)

#define JOT_ALL_IMPL
#define LIB_MEM_DEBUG
#define DEBUG

#include "mdump.h"

#include "lib/platform.h"
#include "lib/allocator.h"
#include "lib/string.h"
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
    file_logger_init_use(&global_logger, &malloc_allocator.allocator, &malloc_allocator.allocator);

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

    int version = gladLoadGL((GLADloadfunc) glfwGetProcAddress);
    TEST_MSG(version != 0, "Failed to load opengl with glad");

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


int perf_counter_compare_total_time_func(const void* a_, const void* b_)
{
    Global_Perf_Counter* a = (Global_Perf_Counter*) a_;
    Global_Perf_Counter* b = (Global_Perf_Counter*) b_;
    
    if(profile_get_counter_total_running_time_s(*a) > profile_get_counter_total_running_time_s(*b))
        return -1;
    else 
        return 1;
}

int perf_counter_compare_file_func(const void* a_, const void* b_)
{
    Global_Perf_Counter* a = (Global_Perf_Counter*) a_;
    Global_Perf_Counter* b = (Global_Perf_Counter*) b_;
    
    int res = strcmp(a->file, b->file);
    if(res == 0)
        res = strcmp(a->function, b->function);
    if(res == 0)
        res = strcmp(a->name, b->name);

    return res;
}

//void log_perf_counter(const char* log_module, Log_Type log_type)
//{
//
//}

void log_perf_counters(const char* log_module, Log_Type log_type, bool sort_by_name)
{
    DEFINE_ARRAY_TYPE(Global_Perf_Counter, _Global_Perf_Counter_Array);

    String common_prefix = {0};

    _Global_Perf_Counter_Array counters = {0};
    for(Global_Perf_Counter* counter = profile_get_counters(); counter != NULL; counter = counter->next)
    {

        String curent_file = string_make(counter->file);
        if(common_prefix.data == NULL)
            common_prefix = curent_file;
        else
        {
            isize matches_to = 0;
            isize min_size = MIN(common_prefix.size, curent_file.size);
            for(; matches_to < min_size; matches_to++)
            {
                if(common_prefix.data[matches_to] != curent_file.data[matches_to])
                    break;
            }

            common_prefix = string_safe_head(common_prefix, matches_to);
        }

        array_push(&counters, *counter);
    }
    
    if(sort_by_name)
        qsort(counters.data, counters.size, sizeof(Global_Perf_Counter), perf_counter_compare_file_func);
    else
        qsort(counters.data, counters.size, sizeof(Global_Perf_Counter), perf_counter_compare_total_time_func);
    
    LOG(log_module, log_type, "Logging perf counters (still running %lli):", (lli) profile_get_total_running_counters_count());
    log_group_push();
    for(isize i = 0; i < counters.size; i++)
    {
        Global_Perf_Counter counter = counters.data[i];
        Perf_Counter_Stats stats = perf_counter_get_stats(counter.counter, 1);

        if(counter.is_detailed)
        {
		    LOG(log_module, log_type, "total: %15.7lf avg: %13.6lf runs: %-8lli σ/μ %13.6lf [%13.6lf %13.6lf] (ms) from %20s %-4lli %s \"%s\"", 
			    stats.total_s*1000,
			    stats.average_s*1000,
                (lli) stats.runs,
                stats.normalized_standard_deviation_s,
			    stats.min_s*1000,
			    stats.max_s*1000,
                counter.file + common_prefix.size,
			    (lli) counter.line,
			    counter.function,
			    counter.name
		    );
        }
        else
        {
		    LOG(log_module, log_type, "total: %15.8lf avg: %13.6lf runs: %-8lli (ms) from %20s %-4lli %s \"%s\"", 
			    stats.total_s*1000,
			    stats.average_s*1000,
                (lli) stats.runs,
                counter.file + common_prefix.size,
			    (lli) counter.line,
			    counter.function,
			    counter.name
		    );
        }
        if(counter.concurrent_running_counters > 0)
        {
            log_group_push();
            LOG(log_module, log_type, "COUNTER LEAKS! Still running %lli", (lli) counter.concurrent_running_counters);
            log_group_pop();
        }
    }
    log_group_pop();

    array_deinit(&counters);
}

void log_todos(const char* log_module, Log_Type log_type, const char* marker)
{
    Todo_Array todos = {0};
    todo_parse_folder(&todos, STRING("./"), string_make(marker), 1);

    String common_path_prefix = {0};
    for(isize i = 0; i < todos.size; i++)
    {
        String curent_path = string_from_builder(todos.data[i].path);
        if(common_path_prefix.data == NULL)
            common_path_prefix = curent_path;
        else
        {
            isize matches_to = 0;
            isize min_size = MIN(common_path_prefix.size, curent_path.size);
            for(; matches_to < min_size; matches_to++)
            {
                if(common_path_prefix.data[matches_to] != curent_path.data[matches_to])
                    break;
            }

            common_path_prefix = string_safe_head(common_path_prefix, matches_to);
        }
    }
    
    LOG(log_module, log_type, "Logging TODOs (%lli):", (lli) todos.size);
    log_group_push();
    for(isize i = 0; i < todos.size; i++)
    {
        Todo todo = todos.data[i];
        String path = string_from_builder(todo.path);
        
        if(path.size > common_path_prefix.size)
            path = string_safe_tail(path, common_path_prefix.size);

        if(todo.signature.size > 0)
            LOG(log_module, log_type, "%-20s %4lli %s(%s) %s\n", cstring_escape(path.data), (lli) todo.line, cstring_escape(todo.marker.data), cstring_escape(todo.signature.data), cstring_escape(todo.comment.data));
        else
            LOG(log_module, log_type, "%-20s %4lli %s %s\n", cstring_escape(path.data), (lli) todo.line, cstring_escape(todo.marker.data), cstring_escape(todo.comment.data));
    }
    log_group_pop();
    
    for(isize i = 0; i < todos.size; i++)
        todo_deinit(&todos.data[i]);

    array_deinit(&todos);
}


const char* format_memory_unit_ephemeral(isize bytes)
{
    isize unit = 1;
    static String_Builder formatted = {0};
    if(formatted.allocator == NULL)
        array_init(&formatted, allocator_get_static());

    const char* unit_text = get_memory_unit(bytes, &unit);
    f64 ratio = (f64) bytes / (f64) unit;
    format_into(&formatted, "%.1lf %s", ratio, unit_text);

    return formatted.data;
}

void log_allocator_stats(const char* log_module, Log_Type log_type, Allocator_Stats stats)
{
    String_Builder formatted = {0};
    array_init_backed(&formatted, NULL, 512);

    LOG(log_module, log_type, "bytes_allocated: %s", format_memory_unit_ephemeral(stats.bytes_allocated));
    LOG(log_module, log_type, "max_bytes_allocated: %s", format_memory_unit_ephemeral(stats.max_bytes_allocated));
    LOG(log_module, log_type, "allocation_count: %lli", (lli) stats.allocation_count);
    LOG(log_module, log_type, "deallocation_count: %lli", (lli) stats.deallocation_count);
    LOG(log_module, log_type, "reallocation_count: %lli", (lli) stats.reallocation_count);

    array_deinit(&formatted);
}


void break_debug_allocator()
{
    //My theory is that when reallocating from high aligned offset to low aligned offset we dont shift over the data
    //I belive this can be easily solved by alloc & dealloc pair instead of the singular realloc we do currenlty.

    Debug_Allocator debug_alloc = {0};
    debug_allocator_init(&debug_alloc, allocator_get_default(), DEBUG_ALLOCATOR_DEINIT_LEAK_CHECK | DEBUG_ALLOCATOR_CAPTURE_CALLSTACK);
    {
        Allocator* allocator = allocator_get_default();
        isize alloc_gran = 41;
        enum {ITERS = 100, TEST_VAL = 0x66};
        u8* allocs[ITERS] = {0};
        isize sizes[ITERS] = {0};
        isize aligns[ITERS] = {0};

        isize i = 6;
        //for(isize i = 0; i < ITERS; i++)
        {
            sizes[i] = alloc_gran*i;
            aligns[i] = (isize) 1 << (i % 13); 
            allocs[i] = (u8*) debug_allocator_allocate(allocator, sizes[i], NULL, 0, aligns[i], SOURCE_INFO());

            memset(allocs[i], TEST_VAL, sizes[i]);
            for(isize k = 0; k < sizes[i]; k++)
                TEST(allocs[i][k] == TEST_VAL);
        }
        
        //for(isize i = 0; i < ITERS; i++)
        {
            u8* alloc = allocs[i];
            isize size = sizes[i];
            isize align = aligns[i];

            allocs[i] = (u8*) debug_allocator_allocate(allocator, size + alloc_gran, alloc, size, align, SOURCE_INFO());
            sizes[i] = size + alloc_gran;
            for(isize k = 0; k < size - alloc_gran; k++)
                TEST(alloc[k] == TEST_VAL);
        }
        //for(isize i = 0; i < ITERS; i++)
        {
            u8* alloc = allocs[i];
            isize size = sizes[i];
            isize align = aligns[i]; 
            for(isize k = 0; k < size - alloc_gran; k++)
                TEST(alloc[k] == TEST_VAL);

            allocator_deallocate(allocator, alloc, size, align, SOURCE_INFO());
        }
    }
    debug_allocator_deinit(&debug_alloc);
}



Error render_shader_init_from_disk_compat(Render_Shader* shader, String vert, String frag, String name)
{
    (void) name;
    LOG_INFO("APP", "loading shader: " STRING_FMT, STRING_PRINT(vert));
    return render_shader_init_from_disk_split(shader, vert, frag, STRING(""));
}

void run_func(void* context)
{
    test_mdump();
    log_todos("APP", LOG_TYPE_DEBUG, "@TODO @TOOD @TEMP @SPEED @PERF");

    LOG_INFO("APP", "run_func enter");
    Allocator* upstream_alloc = allocator_get_default();
    
    //u8_Array stack_buffer = {0};
    //array_resize(&stack_buffer, MEBI_BYTE * 100);
    //Stack_Allocator stack = {0};
    //stack_allocator_init(&stack, stack_buffer.data, stack_buffer.size, allocator_get_default());
    //Allocator_Set scratch_set = allocator_set_both(&stack.allocator, &stack.allocator);

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
    
    Entity falcon = entity_generate(0);

    Id falcon_handle = {0};
    Render_Object render_falcon = {0};

    Render_Screen_Frame_Buffers_MSAA screen_buffers = {0};
    Render_Capture_Buffers capture_buffers = {0};

    i32 res_environment = 1024;
    i32 res_irradiance = 32;
    i32 res_prefilter = 128;
    i32 res_brdf_lut = 512;

    f32 irradicance_sample_delta = 0.025f;
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
    //Render_Mesh render_falcon = {0};

    Render_Image image_floor = {0};
    Render_Image image_debug = {0};
    Render_Image image_environment = {0};
    Render_Material material_metal = {0};

    Render_Cubeimage cubemap_skybox = {0};

    Render_Cubeimage cubemap_environment = {0};
    Render_Cubeimage cubemap_irradiance = {0};
    Render_Cubeimage cubemap_prefilter = {0};
    Render_Image image_brdf_lut = {0};

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
    bool use_mapping = false;

    glEnable(GL_TEXTURE_CUBE_MAP_SEAMLESS);

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
            glViewport(0, 0, screen_buffers.width, screen_buffers.height);
        }
        
        if(control_was_pressed(&app->controls, CONTROL_REFRESH_ALL) 
            || control_was_pressed(&app->controls, CONTROL_REFRESH_SHADERS)
            || frame_num == 0)
        {
            LOG_INFO("APP", "Refreshing shaders");
            PERF_COUNTER_START(shader_load_counter);
            
            //@TODO: refactor out Error and use bools.
            //@TODO: add include directive to shader files!
            Error error = {0};
            error = ERROR_AND(error) render_shader_init_from_disk(&shader_solid_color,       STRING("shaders/solid_color.frag_vert"));
            error = ERROR_AND(error) render_shader_init_from_disk(&shader_depth_color,       STRING("shaders/depth_color.frag_vert"));
            error = ERROR_AND(error) render_shader_init_from_disk(&shader_screen,            STRING("shaders/screen.frag_vert"));
            error = ERROR_AND(error) render_shader_init_from_disk(&shader_blinn_phong,       STRING("shaders/blinn_phong.frag_vert"));
            error = ERROR_AND(error) render_shader_init_from_disk(&shader_skybox,            STRING("shaders/skybox.frag_vert"));
            error = ERROR_AND(error) render_shader_init_from_disk(&shader_pbr,               STRING("shaders/pbr.frag_vert"));
            error = ERROR_AND(error) render_shader_init_from_disk(&shader_equi_to_cubemap,   STRING("shaders/equi_to_cubemap.frag_vert"));
            error = ERROR_AND(error) render_shader_init_from_disk(&shader_irradiance,        STRING("shaders/irradiance_convolution.frag_vert"));
            error = ERROR_AND(error) render_shader_init_from_disk(&shader_prefilter,         STRING("shaders/prefilter.frag_vert"));
            error = ERROR_AND(error) render_shader_init_from_disk(&shader_brdf_lut,          STRING("shaders/brdf_lut.frag_vert"));
            error = ERROR_AND(error) render_shader_init_from_disk(&shader_pbr_mapped,        STRING("shaders/pbr_mapped.frag_vert"));
            error = ERROR_AND(error) render_shader_init_from_disk(&shader_debug,             STRING("shaders/uv_debug.frag_vert"));
                
            PERF_COUNTER_END(shader_load_counter);
            ASSERT(error_is_ok(error));
        }

        if(control_was_pressed(&app->controls, CONTROL_REFRESH_ALL) 
            || control_was_pressed(&app->controls, CONTROL_REFRESH_ART)
            || frame_num == 0)
        {
            
            //Debug_Allocator art_reload_leak_check = {0};
            //debug_allocator_init_use(&art_reload_leak_check, DEBUG_ALLOCATOR_DEINIT_LEAK_CHECK | DEBUG_ALLOCATOR_CAPTURE_CALLSTACK);

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


            if(0)
            {
                triangle_mesh_read_entire(&falcon_handle, STRING("resources/sponza/sponza.obj"));
            
                
                Transform transf = {0};
                transf.translate = vec3(20, 0, 20);
                transf.scale = vec3_of(0.1f);
                render_compoment_add(&falcon, falcon_handle, transf, 0);

                //falcon_object = resources_object_get(&resources, falcon_handle);
                //@TODO: add error returns here!
                render_object_init_from_object(&render_falcon, &renderer, falcon_handle);
            }

            Error error = {0};

            render_mesh_init_from_shape(&render_uv_sphere, uv_sphere, STRING("uv_sphere"));
            render_mesh_init_from_shape(&render_cube_sphere, cube_sphere, STRING("cube_sphere"));
            render_mesh_init_from_shape(&render_screen_quad, screen_quad, STRING("screen_quad"));
            render_mesh_init_from_shape(&render_cube, unit_cube, STRING("unit_cube"));
            render_mesh_init_from_shape(&render_quad, unit_quad, STRING("unit_cube"));
            
            PERF_COUNTER_START(art_counter_single_tex);
            error = ERROR_AND(error) render_image_init_from_disk(&image_floor, STRING("resources/floor.jpg"), STRING("floor"));
            PERF_COUNTER_END(art_counter_single_tex);
            error = ERROR_AND(error) render_image_init_from_disk(&image_debug, STRING("resources/debug.png"), STRING("debug"));
            error = ERROR_AND(error) render_cubeimage_init_from_disk(&cubemap_skybox, 
                STRING("resources/skybox_front.jpg"), 
                STRING("resources/skybox_back.jpg"), 
                STRING("resources/skybox_top.jpg"), 
                STRING("resources/skybox_bottom.jpg"), 
                STRING("resources/skybox_right.jpg"), 
                STRING("resources/skybox_left.jpg"), STRING("skybox"));

            error = ERROR_AND(error) render_image_init_from_disk(&image_environment, STRING("resources/HDR_041_Path_Ref.hdr"), STRING("image_environment"));

            render_capture_buffers_init(&capture_buffers, res_environment, res_environment);
            
            f32 use_gamma = 2.0f;
            if(control_is_down(&app->controls, CONTROL_DEBUG_3))
                use_gamma = 1.0f;
                
            PERF_COUNTER_START(art_counter_environment);
            render_cubeimage_init_environment_from_environment_map(
                &cubemap_environment, res_environment, res_environment, &image_environment, use_gamma, 
                &capture_buffers, &shader_equi_to_cubemap, render_cube);
            PERF_COUNTER_END(art_counter_environment);
            
            PERF_COUNTER_START(art_counter_irafiance);
            render_cubeimage_init_irradiance_from_environment(
                &cubemap_irradiance, res_irradiance, res_irradiance, irradicance_sample_delta,
                &cubemap_environment, &capture_buffers, &shader_irradiance, render_cube);
            PERF_COUNTER_END(art_counter_irafiance);

            glViewport(0, 0, app->window_framebuffer_width, app->window_framebuffer_height); //@TODO stuff somehwere else!
            
            PERF_COUNTER_END(art_load_counter);
            ASSERT(error_is_ok(error));

            //debug_allocator_deinit(&art_reload_leak_check);
        }

        if(control_was_pressed(&app->controls, CONTROL_ESCAPE))
        {
            app->is_in_mouse_mode = !app->is_in_mouse_mode;
        }
        
        if(control_was_pressed(&app->controls, CONTROL_DEBUG_1))
        {
            log_perf_counters("app", LOG_TYPE_INFO, true);
        }
        
        if(control_was_pressed(&app->controls, CONTROL_DEBUG_1))
        {
            app->is_in_uv_debug_mode = !app->is_in_uv_debug_mode;
        }
        
        if(control_was_pressed(&app->controls, CONTROL_DEBUG_2))
        {
            use_mapping = !use_mapping;
        }

        if(app->is_in_mouse_mode == false)
        {
            PERF_COUNTER_START(input_counter);
            f32 fov_sens_modifier = sinf(app->zoom_target_fov);
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

        
        app->camera.fov = settings->fov;
        app->camera.near = 0.1f;
        app->camera.far = 100.0f;
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

            Sphere_Light lights[PBR_MAX_LIGHTS] = {0};

            f32 light_radius = 0.3f;
            if(control_is_down(&app->controls, CONTROL_DEBUG_4))
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

            lights[3].pos = vec3(20, 10, 20);
            lights[3].color = vec3(1, 1, 0.8f);
            lights[3].radius = light_radius;

            //render blinn phong sphere
            {
                Sphere_Light light = lights[0]; 
                Blinn_Phong_Params blinn_phong_params = {0};
                Mat4 model = mat4_translate(mat4_rotation(vec3(2, 1, 3), clock_s32() / 8), vec3(5, 0, -5));
                blinn_phong_params.projection = projection;
                blinn_phong_params.view = view;
                blinn_phong_params.model = model;

                blinn_phong_params.view_pos = app->camera.pos;
                blinn_phong_params.light_pos = light.pos;
                blinn_phong_params.light_color = light.color;

                blinn_phong_params.ambient_color = vec3_of(0.01f);
                blinn_phong_params.specular_color = vec3_of(0.4f);
                blinn_phong_params.light_linear_attentuation = 0;
                blinn_phong_params.light_quadratic_attentuation = 0.00f;
                blinn_phong_params.specular_exponent = 32;
                //blinn_phong_params.light_specular_effect = 0.4f; 
                //blinn_phong_params.gamma = settings->screen_gamma;
                blinn_phong_params.gamma = 1.3f; //looks better on the wood texture

                render_mesh_draw_using_blinn_phong(render_cube_sphere, &shader_blinn_phong, blinn_phong_params, image_floor);
            
                if(app->is_in_uv_debug_mode)
                    render_mesh_draw_using_uv_debug(render_cube_sphere, &shader_debug, projection, view, model);
            }
            
            //render pbr sphere
            {
                PBR_Params params = {0};
                for(isize i = 0; i < PBR_MAX_LIGHTS; i++)
                    params.lights[i] = lights[i];

                params.view_pos = app->camera.pos;
                params.gamma = settings->screen_gamma;
                params.attentuation_strength = 1.0f;
            
                f32 spacing = 3;
                f32 scale = 0.2f;
                isize iters = 7;
                Vec3 offset = {-2/scale, 0/scale, 2/scale};
                for(isize metallic_i = 0; metallic_i <= iters; metallic_i++)
                {
                    for(isize rough_i = 0; rough_i <= iters; rough_i++)
                    {
                        if(rough_i == iters)
                        {
                            isize x = 10;
                            (void) x;
                        }

                        f32 rough = (f32) rough_i / iters;
                        f32 metallic = (f32) metallic_i / iters;
                    
                        Render_Filled_Material material = {0};
                        material.info.metallic = metallic;
                        material.info.roughness = rough;
                        material.info.albedo = vec3(1, 1, 0.92f);
                        material.info.ambient_occlusion = default_ao;

                        Vec3 pos = vec3(spacing * rough_i, spacing*metallic_i, 0);
                        Mat4 model = mat4_scale_affine(mat4_translation(vec3_add(offset, pos)), vec3_of(scale));

                        //if(use_mapping)
                        //    render_mesh_draw_using_pbr_mapped(render_uv_sphere, &shader_pbr_mapped, projection, view, model, &params, &material,
                        //        &cubemap_environment, &cubemap_irradiance, &cubemap_prefilter, &image_brdf_lut);
                        //else
                            render_mesh_draw_using_pbr(render_uv_sphere, &shader_pbr, projection, view, model, &params, &material, NULL);
                    }
                }

                //Mat4 model = mat4_translate(mat4_scaling(vec3_of(0.5f)), vec3(0, 2, 5));
                //render_mesh_draw_using_pbr(render_uv_sphere, &shader_pbr, projection, view, model, &params, &material_metal);
                
                //if(use_mapping)
                //    render_mesh_draw_using_pbr_mapped(render_uv_sphere, &shader_pbr_mapped, projection, view, model, &params, &material_metal,
                //        &cubemap_environment, &cubemap_irradiance, &cubemap_prefilter, &image_brdf_lut);
                //else
                    //render_mesh_draw_using_pbr(render_uv_sphere, &renderer, &shader_pbr, projection, view, model, &params, &material_metal);
            }
            

            //render light
            for(isize i = 0; i < PBR_MAX_LIGHTS; i++)
            {
                Sphere_Light light = lights[i]; 
                Mat4 model = mat4_translate(mat4_scaling(vec3_of(light.radius)), light.pos);
                //render_mesh_draw_using_solid_color(render_uv_sphere, &shader_solid_color, projection, view, model, light.color, settings->screen_gamma);
                render_mesh_draw_using_solid_color(render_uv_sphere, &shader_solid_color, projection, view, model, light.color, 1);
            }

            //render falcon
            if(0)
            {
                Mat4 model = mat4_translate(mat4_scaling(vec3_of(0.01f)), vec3(20, 0, 20));
                //render_mesh_draw_using_depth_color(render_falcon, &shader_depth_color, projection, view, model, vec3(0.3f, 0.3f, 0.3f));

                
                PBR_Params params = {0};
                for(isize i = 0; i < PBR_MAX_LIGHTS; i++)
                    params.lights[i] = lights[i];

                params.view_pos = app->camera.pos;
                params.gamma = settings->screen_gamma;
                params.attentuation_strength = 0;
                params.ambient_color = vec3_scale(lights[3].color, 0.00f);

                render_object(&render_falcon, &renderer, &shader_pbr, projection, view, model, &params);
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

                render_mesh_draw_using_solid_color(render_cube, &shader_solid_color, projection, view, X, vec3(1, 0, 0), settings->screen_gamma);
                render_mesh_draw_using_solid_color(render_cube, &shader_solid_color, projection, view, Y, vec3(0, 1, 0), settings->screen_gamma);
                render_mesh_draw_using_solid_color(render_cube, &shader_solid_color, projection, view, Z, vec3(0, 0, 1), settings->screen_gamma);
                render_mesh_draw_using_solid_color(render_cube, &shader_solid_color, projection, view, MID, vec3(1, 1, 1), settings->screen_gamma);
            }

            //render skybox
            {
                Mat4 model = mat4_scaling(vec3_of(-1));
                Mat4 stationary_view = mat4_from_mat3(mat3_from_mat4(view));
                    //render_mesh_draw_using_skybox(render_cube, &shader_skybox, projection, stationary_view, model, settings->screen_gamma, cubemap_environment);
                    //render_mesh_draw_using_skybox(render_cube, &shader_skybox, projection, stationary_view, model, 1.0f/settings->screen_gamma, cubemap_skybox);
                //else
                if(control_is_down(&app->controls, CONTROL_DEBUG_3))
                    render_mesh_draw_using_skybox(render_cube, &shader_skybox, projection, stationary_view, model, 1, cubemap_prefilter);
                else
                    render_mesh_draw_using_skybox(render_cube, &shader_skybox, projection, stationary_view, model, 1, cubemap_environment);
                    //render_mesh_draw_using_skybox(render_cube, &shader_skybox, projection, stationary_view, model, 1, cubemap_skybox);
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

        //@HACK: this forsome reason depends on some calculation from the previous frame else it doesnt work dont know why
        if(control_was_pressed(&app->controls, CONTROL_REFRESH_ALL) 
            || control_was_pressed(&app->controls, CONTROL_REFRESH_ART)
            || frame_num == 0)
        {
            
            if(0)
            {
            PERF_COUNTER_START(art_counter_prefileter);
            render_cubeimage_init_prefilter_from_environment(
                &cubemap_prefilter, res_prefilter, res_prefilter,
                &cubemap_environment, &capture_buffers, &shader_prefilter, render_cube);
            PERF_COUNTER_END(art_counter_prefileter);
            
            PERF_COUNTER_START(art_counter_brdf_lut);
            render_image_init_BRDF_LUT(
                &image_brdf_lut, res_brdf_lut, res_brdf_lut,
                &capture_buffers, &shader_brdf_lut, render_quad);
            PERF_COUNTER_END(art_counter_brdf_lut);
            }

            log_perf_counters("APP", LOG_TYPE_DEBUG, true);

            LOG_INFO("RENDER", "Render allocation stats:");
            log_group_push();
                log_allocator_stats("RENDER", LOG_TYPE_INFO, allocator_get_stats(&renderer_alloc.allocator));
            log_group_pop();

            LOG_INFO("RESOURCES", "Resources allocation stats:");
            log_group_push();
                log_allocator_stats("RESOURCES", LOG_TYPE_INFO, allocator_get_stats(&resources_alloc.allocator));
            log_group_pop();
            
            LOG_INFO("RESOURCES", "Resources repository memory usage stats:");
            log_group_push();
            
                isize accumulated_total = 0;
                for(isize i = 0; i < RESOURCE_TYPE_ENUM_COUNT; i++)
                {
                    Resource_Manager* manager = resources_get_type(i);
                    isize type_alloced = stable_array_capacity(&manager->storage)*manager->type_size;
                    isize hash_alloced = manager->id_hash.entries_count * sizeof(Hash_Ptr_Entry);

                    LOG_INFO("RESOURCES", "%s", manager->type_name);
                    log_group_push();
                        LOG_INFO("RESOURCES", "type_alloced: %s", format_memory_unit_ephemeral(type_alloced));
                        LOG_INFO("RESOURCES", "hashes:       %s", format_memory_unit_ephemeral(hash_alloced));
                    log_group_pop();

                    accumulated_total += type_alloced + hash_alloced;
                }

                LOG_INFO("RESOURCES", "total overhead:       %s", format_memory_unit_ephemeral(accumulated_total));
            log_group_pop();

            glViewport(0, 0, app->window_framebuffer_width, app->window_framebuffer_height); //@TODO stuff somehwere else!
        }

        f64 end_frame_time = clock_s();
        f64 frame_time = end_frame_time - start_frame_time;
        if(end_frame_time - fps_display_last_update > 1.0/fps_display_frequency)
        {
            fps_display_last_update = end_frame_time;
            format_into(&fps_display, "Render %5d fps", (int) (1.0f/frame_time));
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
    
    render_object_deinit(&render_falcon, &renderer);
    resources_deinit(&resources);
    render_deinit(&renderer);

    render_pbr_material_deinit(&material_metal, &renderer);

    array_deinit(&fps_display);
    
    render_image_deinit(&image_floor);
    render_image_deinit(&image_debug);
    render_image_deinit(&image_environment);
    render_image_deinit(&image_brdf_lut);
    render_cubeimage_deinit(&cubemap_skybox);
    render_cubeimage_deinit(&cubemap_environment);
    render_cubeimage_deinit(&cubemap_irradiance);
    render_cubeimage_deinit(&cubemap_prefilter);

    render_screen_frame_buffers_msaa_deinit(&screen_buffers);
    
    log_perf_counters("APP", LOG_TYPE_INFO, true);
    render_world_deinit();

    LOG_INFO("RESOURCES", "Resources allocation stats:");
    log_group_push();
        log_allocator_stats("RESOURCES", LOG_TYPE_INFO, allocator_get_stats(&resources_alloc.allocator));
    log_group_pop();

    debug_allocator_deinit(&resources_alloc);
    debug_allocator_deinit(&renderer_alloc);
    
    //stack_allocator_deinit(&stack);
    //array_deinit(&stack_buffer);

    //allocator_set(scratch_set);

    LOG_INFO("APP", "run_func exit");
}

void error_func(void* context, Platform_Sandbox_Error error)
{
    (void) context;
    const char* msg = platform_exception_to_string(error.exception);
    
    LOG_ERROR("APP", "%s exception occured", msg);
    LOG_TRACE("APP", "printing trace:");
    log_group_push();
    log_translated_callstack("APP", LOG_TYPE_ERROR, error.call_stack, error.call_stack_size);
    log_group_pop();
}

#include "lib/_test_unicode.h"
#include "lib/_test_random.h"
#include "lib/_test_array.h"
#include "lib/_test_hash_index.h"
#include "lib/_test_log.h"
#include "lib/_test_hash_table.h"
#include "lib/_test_math.h"
#include "lib/_test_base64.h"
#include "lib/_test_hash_index.h"
#include "lib/_test_stable_array.h"
#include "lib/_test_lpf.h"

void run_test_func(void* context)
{

    (void) context;
    test_unicode(3.0);
    test_format_lpf();
    test_stable_array();
    test_hash_index(3.0);
    test_stable_array();
    test_base64(3.0);
    test_array(3.0);
    test_math(3.0);
    test_hash_table_stress(3.0);
    test_log();
    test_hash_index(3.0);
    test_random();
    LOG_INFO("TEST", "All tests passed! uwu");
    return;
}
