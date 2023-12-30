void run_func(void* context)
{
    test_mdump();
    log_todos("APP", LOG_DEBUG, "@TODO @TOOD @TEMP @SPEED @PERF");

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
            log_perf_counters("app", LOG_INFO, true);
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

            log_perf_counters("APP", LOG_DEBUG, true);

            LOG_INFO("RENDER", "Render allocation stats:");
                log_allocator_stats(">RENDER", LOG_INFO, &renderer_alloc.allocator);

            LOG_INFO("RESOURCES", "Resources allocation stats:");
                log_allocator_stats(">RESOURCES", LOG_INFO, &resources_alloc.allocator);
            
            LOG_INFO("RESOURCES", "Resources repository memory usage stats:");
            
                isize accumulated_total = 0;
                for(isize i = 0; i < RESOURCE_TYPE_ENUM_COUNT; i++)
                {
                    Resource_Manager* manager = resources_get_type(i);
                    isize type_alloced = stable_array_capacity(&manager->storage)*manager->type_size;
                    isize hash_alloced = manager->id_hash.entries_count * sizeof(Hash_Ptr_Entry);

                    LOG_INFO(">RESOURCES", "%s", manager->type_name);
                        LOG_INFO(">>RESOURCES", "type_alloced: %s", format_memory_unit_ephemeral(type_alloced));
                        LOG_INFO(">>RESOURCES", "hashes:       %s", format_memory_unit_ephemeral(hash_alloced));

                    accumulated_total += type_alloced + hash_alloced;
                }

                LOG_INFO(">RESOURCES", "total overhead:       %s", format_memory_unit_ephemeral(accumulated_total));

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
    
    log_perf_counters("APP", LOG_INFO, true);
    render_world_deinit();

    LOG_INFO("RESOURCES", "Resources allocation stats:");
        log_allocator_stats(">RESOURCES", LOG_INFO, allocator_get_stats(&resources_alloc.allocator));

    debug_allocator_deinit(&resources_alloc);
    debug_allocator_deinit(&renderer_alloc);
    
    //stack_allocator_deinit(&stack);
    //array_deinit(&stack_buffer);

    //allocator_set(scratch_set);

    LOG_INFO("APP", "run_func exit");
}
