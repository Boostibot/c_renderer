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
void shape_append(Shape* shape, const Vertex* vertices, isize vertices_count, const Triangle_Index* indeces, isize trinagles_count)
{
    isize vertex_count_before = shape->vertices.size;
    isize triangle_count_before = shape->triangles.size;

    array_append(&shape->vertices, vertices, vertices_count);
    array_resize(&shape->triangles, triangle_count_before + trinagles_count);
    for(isize i = 0; i < trinagles_count; i++)
    {
        Triangle_Index index = indeces[i];
        CHECK_BOUNDS(index.vertex_i[0], vertices_count);
        CHECK_BOUNDS(index.vertex_i[1], vertices_count);
        CHECK_BOUNDS(index.vertex_i[2], vertices_count);
        
        Triangle_Index offset_index = index;
        offset_index.vertex_i[0] += (u32) vertex_count_before;
        offset_index.vertex_i[1] += (u32) vertex_count_before;
        offset_index.vertex_i[2] += (u32) vertex_count_before;

        *array_get(shape->triangles, triangle_count_before + i) = offset_index;
        //shape->triangles.data[vertex_count_before + i] = offset_index;
    }
}
typedef struct Render_Batch_Group {
    i32 indeces_from;
    i32 indeces_to;

    Render_Image diffuse;
    Render_Image specular;
    
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

typedef struct Render_Texture_Array_Info {
    i32 width;
    i32 height;
    i32 layer_count;

    i32 mip_level_count;
    GL_Pixel_Format pixel_format;

    GLuint mag_filter;
    GLuint min_filter;
    GLuint wrap_s;
    GLuint wrap_t;
} Render_Texture_Array_Info;

typedef struct Render_Texture_Array {
    GLuint handle;
    Render_Texture_Array_Info info;
} Render_Texture_Array;

void render_texture_array_deinit(Render_Texture_Array* array)
{
    glDeleteTextures(1, &array->handle);
    memset(array, 0, sizeof *array);
}

bool render_texture_array_init(Render_Texture_Array* array, Render_Texture_Array_Info info, void* data_or_null)
{
    //source: https://www.khronos.org/opengl/wiki/Array_Texture#Creation_and_Management
    render_texture_array_deinit(array);
    
    Render_Texture_Array_Info copied = info;
    GL_Pixel_Format* p = &copied.pixel_format;
    if(p->equivalent != 0 && p->access_format == 0)
        *p = gl_pixel_format_from_pixel_format(p->equivalent, p->channels);
    else if(p->equivalent == 0 && p->access_format != 0)
    {
        isize channels = 0;
        p->equivalent = pixel_format_from_gl_pixel_format(*p, &channels);
        p->channels = (i32) channels;
    }
    else if((p->equivalent == 0 && p->access_format == 0) || p->unrepresentable)
    {
        LOG_ERROR("render", "missing pixel_format in call to render_texture_array_init()");
        ASSERT(false);
        return false;
    }

    if(copied.layer_count == 0) copied.layer_count = 0;
    if(copied.mip_level_count == 0) copied.mip_level_count = 1;
    if(copied.min_filter == 0) copied.min_filter = GL_LINEAR_MIPMAP_LINEAR;
    if(copied.mag_filter == 0) copied.mag_filter = GL_LINEAR;
    if(copied.wrap_s == 0) copied.wrap_s = GL_CLAMP_TO_EDGE;
    if(copied.wrap_t == 0) copied.wrap_t = GL_CLAMP_TO_EDGE;
    
    array->info = copied;

    glGenTextures(1, &array->handle);

    glBindTexture(GL_TEXTURE_2D_ARRAY, array->handle);
    glTexStorage3D(GL_TEXTURE_2D_ARRAY, copied.mip_level_count, p->storage_format, copied.width, copied.height, copied.layer_count);

    if(data_or_null)
    {
        //                                   1. 2. 3. 4.
        glTexSubImage3D(GL_TEXTURE_2D_ARRAY, 0, 0, 0, 0, copied.width, copied.height, copied.layer_count, p->access_format, p->channel_type, data_or_null);
        // 1. Mip level
        // 2. x of subrectangle
        // 3. y of subrectangle
        // 4. layer to which to write

        if(copied.mip_level_count > 0)
            glGenerateMipmap(GL_TEXTURE_2D_ARRAY);
    }

    glTexParameteri(GL_TEXTURE_2D_ARRAY,GL_TEXTURE_MIN_FILTER, copied.min_filter);
    glTexParameteri(GL_TEXTURE_2D_ARRAY,GL_TEXTURE_MAG_FILTER, copied.mag_filter);
    glTexParameteri(GL_TEXTURE_2D_ARRAY,GL_TEXTURE_WRAP_S, copied.wrap_s);
    glTexParameteri(GL_TEXTURE_2D_ARRAY,GL_TEXTURE_WRAP_T, copied.wrap_t);

    gl_check_error();
    return true;
}

void render_texture_array_generate_mips(Render_Texture_Array* array)
{
    glBindTexture(GL_TEXTURE_2D_ARRAY, array->handle);
    if(array->info.mip_level_count > 0)
        glGenerateMipmap(GL_TEXTURE_2D_ARRAY);
}

void render_texture_array_set(Render_Texture_Array* array, i32 layer, i32 x, i32 y, Image_Builder image, bool regenerate_mips)
{
    glBindTexture(GL_TEXTURE_2D_ARRAY, array->handle);
    if(image.width > 0 && image.height > 0)
    {
        GL_Pixel_Format format = gl_pixel_format_from_pixel_format(image.pixel_format, image_builder_channel_count(image));
        glTexSubImage3D(GL_TEXTURE_2D_ARRAY, 0, x, y, layer, image.width, image.height, 1, format.access_format, format.channel_type, image.pixels);
    }

    if(regenerate_mips)
        glGenerateMipmap(GL_TEXTURE_2D_ARRAY);
}


isize _gcd(isize a, isize b)
{
    while(b != 0) 
    {
        isize rem = a % b;
        a = b;
        b = rem;
    }

    return a;
}

void test_gcd()
{
    TEST(_gcd(0, 0) == 0);
    TEST(_gcd(0, 12) == 12);
    TEST(_gcd(12, 0) == 12);
    TEST(_gcd(12, 1) == 1);
    TEST(_gcd(1, 12) == 1);
    TEST(_gcd(6, 12) == 6);
    TEST(_gcd(6, 12) == 6);
    TEST(_gcd(12, 12) == 12);
    TEST(_gcd(10, 12) == 2);
}

void test_memset_pattern()
{
    typedef struct  {
        const char* pattern;
        int field_size;
        const char* expected;
    } Test_Case;
    
    Test_Case test_cases[] = {
        {"",            0,  ""},
        {"a",           0,  ""},
        {"ba",          1,  "b"},
        {"hahe",        7,  "hahehah"},
        {"xxxxyyyy",    7,  "xxxxyyy"},
        {"hahe",        9,  "hahehaheh"},
        {"hahe",        24, "hahehahehahehahehahehahe"},
        {"hahe",        25, "hahehahehahehahehahehaheh"},
        {"hahe",        26, "hahehahehahehahehahehaheha"},
        {"hahe",        27, "hahehahehahehahehahehahehah"},
    };
    
    char field[128] = {0};
    char expected[128] = {0};
    for(isize i = 0; i < STATIC_ARRAY_SIZE(test_cases); i++)
    {
        Test_Case test_case = test_cases[i];
        isize pattern_len = strlen(test_case.pattern);

        memset(field, 0, sizeof field);
        memset(expected, 0, sizeof expected);

        memset_pattern(field, test_case.field_size, test_case.pattern, pattern_len);

        memcpy(expected, test_case.expected, strlen(test_case.expected));
        TEST(memcmp(field, expected, sizeof field) == 0);
    }
}

void log_perf_counter_stats(const char* log_module, Log_Type log_type, const char* name, Perf_Counter_Stats stats)
{
    LOG(log_module, log_type, "%20s total: %15.8lf avg: %12.8lf runs: %-8lli σ/μ %13.6lf [%13.6lf %13.6lf] (ms)", 
        name,
		stats.total_s*1000,
		stats.average_s*1000,
        (lli) stats.runs,
        stats.normalized_standard_deviation_s,
		stats.min_s*1000,
		stats.max_s*1000
	);
}

typedef bool (*Benchamrk_Func)(void* context);

Perf_Counter_Stats benchmark(f64 warmup, f64 time, isize repeats, Benchamrk_Func func, void* context)
{
    Perf_Counter counter = {0};
    for(f64 start = clock_s();; )
    {
        f64 now = clock_s();
        if(now > start + time)
            break;

        Perf_Counter_Running running = perf_counter_start();
        u8 keep = true;
        for(isize i = 0; i < repeats; i++)
            keep &= (u8) func(context);

        if(keep && now >= warmup + start)
            perf_counter_end(&counter, running);
    }

    return perf_counter_get_stats(counter, repeats);
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

    log_perf_counter_stats("TEST", LOG_TYPE_INFO, "fill", benchmark(w, t, r, _benchmark_fill_ascii, &context));
    log_perf_counter_stats("TEST", LOG_TYPE_INFO, "by_one", benchmark(w, t, r, _benchmark_fill_ascii_by_one, &context));

    return;
}

//Fills txture array layer in such a way that mipmapping wont effect the edges of the image. 
//The image needs to be smaller than the texture array.
//That is the corner pixels will be expanded to fill the remianing size around the image.
//If the image is exactly the size of the array jsut copies it over.
bool render_texture_array_fill_layer(Render_Texture_Array* array, i32 layer, Image_Builder image, bool regenerate_mips, Image_Builder* temp_builder_or_null)
{
    bool state = true;
    if(image.width > array->info.width || image.height > array->info.height)
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
    else if(image.width == array->info.width && image.height == array->info.height)
    {
        render_texture_array_set(array, layer, 0, 0, image, regenerate_mips);
    }
    else
    {
        Image_Builder temp_storage = {0};
        Image_Builder* temp_builder = temp_builder_or_null;

        if(temp_builder_or_null == NULL)
        {
            image_builder_init_from_pixel_size(&temp_storage, allocator_get_scratch(), image.pixel_size, image.pixel_format);
            temp_builder = &temp_storage;
        }

        ASSERT(image.height > 0 && image.width > 0);

        image_builder_resize(temp_builder, array->info.width, array->info.height);
        image_builder_copy(temp_builder, image_from_builder(image), 0, 0);
        
        isize stride = image_builder_byte_stride(*temp_builder);
        i32 pixel_size = temp_builder->pixel_size;

        u8 color[3] = {0xff, 0, 0xff};
        //Extend sideways
        for(i32 y = 0; y < image.height; y++)
        {
            u8* curr_row = temp_builder->pixels + stride*y;
            u8* last_pixel = curr_row + pixel_size*(image.width - 1);

            //memset_pattern(curr_row + pixel_size*image.width, (array->info.width - image.width)*pixel_size, color, pixel_size);
            memset_pattern(curr_row + pixel_size*image.width, (array->info.width - image.width)*pixel_size, last_pixel, pixel_size);
        }

        //Extend downwards
        u8* last_row = temp_builder->pixels + stride*(image.height - 1);
        for(i32 y = image.height; y < temp_builder->height; y++)
        {
            u8* curr_row = temp_builder->pixels + stride*y;
            memmove(curr_row, last_row, stride);
        }
        
        //memset_pattern(temp_builder->pixels, image_builder_all_pixels_size(*temp_builder), color, sizeof(color));

        render_texture_array_set(array, layer, 0, 0, *temp_builder, regenerate_mips);

        image_builder_deinit(&temp_storage);
    }

    return state;
}

Error render_texture_array_fill_layer_from_disk(Render_Texture_Array* array, i32 layer, String path, bool regenerate_mips, Image_Builder* temp_builder_or_null)
{
    Image_Builder temp_storage = {allocator_get_scratch()};
    Error error = image_read_from_file(&temp_storage, path, 0, PIXEL_FORMAT_U8, IMAGE_LOAD_FLAG_FLIP_Y);
    ASSERT_MSG(error_is_ok(error), ERROR_FMT, ERROR_PRINT(error));
    
    if(error_is_ok(error))
    {
        if(render_texture_array_fill_layer(array, layer, temp_storage, regenerate_mips, temp_builder_or_null) == false)
            error.code = 1;
    }
        
    image_builder_deinit(&temp_storage);
    return error;
}

void run_func(void* context)
{
    test_memset_pattern();

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
        num_instances_x = 40,
        num_instances_y = 40,
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

        //@TODO: dont do named since that requires opengl 4.5
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

    Render_Texture_Array texture_array = {0};
    Render_Texture_Array_Info tex_arr_info = {0};
    tex_arr_info.width = 1024;
    tex_arr_info.height = 1024;
    tex_arr_info.layer_count = 3;
    tex_arr_info.pixel_format.channels = 3;
    tex_arr_info.mip_level_count = 2;
    tex_arr_info.pixel_format.equivalent = PIXEL_FORMAT_U8;

    TEST(render_texture_array_init(&texture_array, tex_arr_info, NULL));

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
            error = ERROR_AND(error) render_shader_init_from_disk(&shader_solid_color,       STRING("shaders/solid_color.frag_vert"));
            error = ERROR_AND(error) render_shader_init_from_disk(&shader_depth_color,       STRING("shaders/depth_color.frag_vert"));
            error = ERROR_AND(error) render_shader_init_from_disk(&shader_screen,            STRING("shaders/screen.frag_vert"));
            error = ERROR_AND(error) render_shader_init_from_disk(&shader_blinn_phong,       STRING("shaders/blinn_phong.frag_vert"));
            error = ERROR_AND(error) render_shader_init_from_disk(&shader_skybox,            STRING("shaders/skybox.frag_vert"));
            error = ERROR_AND(error) render_shader_init_from_disk(&shader_debug,             STRING("shaders/uv_debug.frag_vert"));
            error = ERROR_AND(error) render_shader_init_from_disk(&shader_instanced,         STRING("shaders/instanced_texture.frag_vert"));
            error = ERROR_AND(error) render_shader_init_from_disk(&shader_instanced_batched, STRING("shaders/instanced_batched_texture.frag_vert"));

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
            
            error = ERROR_AND(error) render_texture_array_fill_layer_from_disk(&texture_array, 0, STRING("resources/floor.jpg"), true, NULL);

            error = ERROR_AND(error) render_image_init_from_disk(&image_floor, STRING("resources/floor.jpg"), STRING("floor"));
            error = ERROR_AND(error) render_image_init_from_disk(&image_debug, STRING("resources/debug.png"), STRING("debug"));
            error = ERROR_AND(error) render_image_init_from_disk(&image_rusted_iron_metallic, STRING("resources/rustediron2/rustediron2_metallic.png"), STRING("rustediron2"));
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
                
                group_cube_sphere.diffuse = image_debug;
                //group_cube_sphere.specular = image_rusted_iron_metallic;
                group_cube_sphere.diffuse_color = vec4(1, 1, 1, 1);
                group_cube_sphere.ambient_color = vec4(0, 0, 0, 1);
                group_cube_sphere.specular_color = vec4(0.9f, 0.9f, 0.9f, 0);
                group_cube_sphere.specular_exponent = 256;
                group_cube_sphere.metallic = 0.95f;

                group_unit_cube.diffuse = image_floor;
                //group_unit_cube.specular = image_rusted_iron_metallic;
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
            log_perf_counters("app", LOG_TYPE_INFO, true);
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
                #define MAP_BIT_DIFFUSE 1
                #define MAP_BIT_AMBIENT 2
                #define MAP_BIT_SPCULAR 4
                #define MAP_BIT_NORMAL 8
                
                typedef struct  {
                    MODIFIER_ALIGNED(16) Vec4 diffuse_color;
                    MODIFIER_ALIGNED(16) Vec4 ambient_color;
                    MODIFIER_ALIGNED(16) Vec4 specular_color;
                    MODIFIER_ALIGNED(4) float specular_exponent;
                    MODIFIER_ALIGNED(4) float metallic;
                    MODIFIER_ALIGNED(4) int map_flags;
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
                
                GLuint map_diffuse_loc = render_shader_get_uniform_location(&shader_instanced_batched, "u_maps_diffuse");
                GLuint map_specular_loc = render_shader_get_uniform_location(&shader_instanced_batched, "u_maps_specular");
                GLuint map_array_loc = render_shader_get_uniform_location(&shader_instanced_batched, "u_map_array_test");

                GLint map_slots[MAX_BATCH] = {0};
                GLint slot_i = 0;

                for(isize i = 0; i < render_batch.groups.size; i++)
                {
                    Render_Batch_Group* group = &render_batch.groups.data[i];
                    if(group->diffuse.map != 0)
                        render_image_use(&group->diffuse, slot_i);
                    map_slots[i] = slot_i++;
                }
                glUniform1iv(map_diffuse_loc, (GLsizei) render_batch.groups.size, map_slots);
                
                for(isize i = 0; i < render_batch.groups.size; i++)
                {
                    Render_Batch_Group* group = &render_batch.groups.data[i];
                    if(group->specular.map != 0)
                        render_image_use(&group->specular, slot_i);

                    map_slots[i] = slot_i++;
                }
                glUniform1iv(map_specular_loc, (GLsizei) render_batch.groups.size, map_slots);

                glActiveTexture(GL_TEXTURE0 + slot_i);
                glBindTexture(GL_TEXTURE_2D_ARRAY, texture_array.handle);
                glUniform1i(map_array_loc, slot_i++);

                for(isize i = 0; i < render_batch.groups.size; i++)
                {
                    Render_Batch_Group* group = &render_batch.groups.data[i];
                    params[i].diffuse_color = group->diffuse_color;
                    params[i].specular_color = group->specular_color;
                    params[i].ambient_color = group->ambient_color;
                    params[i].specular_exponent = group->specular_exponent;
                    params[i].metallic = group->metallic;

                    if(group->specular.map)
                        params[i].map_flags |= MAP_BIT_SPCULAR;
                            
                    if(group->diffuse.map)
                        params[i].map_flags |= MAP_BIT_DIFFUSE;
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
    
    log_perf_counters("APP", LOG_TYPE_INFO, true);
    render_world_deinit();

    LOG_INFO("RESOURCES", "Resources allocation stats:");
    log_group_push();
        allocator_log_stats_get(&resources_alloc.allocator, "RESOURCES", LOG_TYPE_INFO);
    log_group_pop();

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
    test_gcd();
    test_memset_pattern();

    benchmark_random_cached(0.3, 1);
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
