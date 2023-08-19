#pragma once

#include "file.h"
#include "math.h"
#include "gl.h"

#define SHADER_UTIL_CHANEL "ASSET"

String_Builder shader_source_prepend(String data, String prepend, Allocator* allocator)
{
    String_Builder composed = {allocator};
    if(prepend.size == 0)
    {
        builder_append(&composed, data);
    }
    else
    {
        isize version_i = string_find_first(data, STRING("#version"), 0);
        isize after_version = 0;
        //if it contains no version directive start at start
        if(version_i == -1)
            after_version = 0;
        //if it does start just after the next line break
        else
        {
            ASSERT(version_i <= data.size);
            after_version = string_find_first_char(data, '\n', version_i);
            if(after_version == -1)
                after_version = data.size - 1;

            after_version += 1;
        }
            
        ASSERT(after_version <= data.size);

        String before_insertion = string_head(data, after_version);
        String after_insertion = string_tail(data, after_version);
        builder_append(&composed, before_insertion);
        builder_append(&composed, prepend);
        builder_append(&composed, STRING("\n"));
        builder_append(&composed, after_insertion);
    }

    return composed;
}

GLuint shader_load_from_source(const char* vertex, const char* fragment, const char* geometry, String_Builder* error, isize* error_at_stage)
{
    ASSERT(vertex != NULL && fragment != NULL);

    GLuint shader_program = 0;
    GLuint shader_types[3] = {GL_VERTEX_SHADER, GL_FRAGMENT_SHADER, GL_GEOMETRY_SHADER};
    GLuint shaders[3] = {0};
    const char* shader_sources[3] = {vertex, fragment, geometry};
    int  success = true;
    bool has_geometry = geometry != NULL && strlen(geometry) != 0;

    for(isize i = 0; i < 3 && success; i++)
    {
        //geometry shader doesnt have to exist
        if(shader_types[i] == GL_GEOMETRY_SHADER && has_geometry == false)
            break;

        shaders[i] = glCreateShader(shader_types[i]);
        glShaderSource(shaders[i], 1, &shader_sources[i], NULL);
        glCompileShader(shaders[i]);

        //Check for errors
        glGetShaderiv(shaders[i], GL_COMPILE_STATUS, &success);
        if(!success)
        {
            if(error != NULL)
            {
                array_resize(error, 1024);
                glGetShaderInfoLog(shaders[i], (GLsizei) error->size, NULL, error->data);
                array_resize(error, strlen(error->data));
            }
            if(error_at_stage)
                *error_at_stage = i + 1;
            break;
        }
    }
        
    if(success)
    {
        shader_program = glCreateProgram();
        glAttachShader(shader_program, shaders[0]);
        glAttachShader(shader_program, shaders[1]);
        if(has_geometry)
            glAttachShader(shader_program, shaders[2]);
            
        glLinkProgram(shader_program);
        glGetProgramiv(shader_program, GL_LINK_STATUS, &success);
        if(!success) 
        {
            if(error != NULL)
            {
                array_resize(error, 1024);
                glGetProgramInfoLog(shader_program, (GLsizei) error->size, NULL, error->data);
                array_resize(error, strlen(error->data));
            }
            if(error_at_stage)
                *error_at_stage = 4;
        }
    }

    glDeleteShader(shaders[0]);
    glDeleteShader(shaders[1]);
    glDeleteShader(shaders[2]);

    if(success == false)
    {
        glDeleteProgram(shader_program);
        shader_program = 0;
    }

    return shader_program;
}

GLuint shader_load(String vertex_path, String fragment_path, String geometry_path, String prepend, String_Builder* error)
{
    Allocator* scratch = allocator_get_scratch();
    String_Builder temp = {scratch};

    Platform_Error vertex_error = file_read_entire(vertex_path, &temp);
    String_Builder vertex_source = shader_source_prepend(string_from_builder(temp), prepend, scratch);
    array_clear(&temp);

    Platform_Error fragment_error = file_read_entire(fragment_path, &temp);
    String_Builder fragment_source = shader_source_prepend(string_from_builder(temp), prepend, scratch);
    array_clear(&temp);

    String_Builder geometry_source = {0};
    Platform_Error geomtery_error = PLATFORM_ERROR_OK;
    if(geometry_path.size > 0)
    {
        geomtery_error = file_read_entire(geometry_path, &temp);
        geometry_source = shader_source_prepend(string_from_builder(temp), prepend, scratch);
        array_clear(&temp);
    }

    isize error_at_compilation_stage = 0; 
    GLuint shader_program = 0;
    if(!vertex_error && !fragment_error && !geomtery_error)
    {
        //Else tries to build the program
        // if an error ocurs here it gets written to error
        // and shader_program = 0. This means we dont have to do anything
        shader_program = shader_load_from_source(
            cstring_from_builder(vertex_source),
            cstring_from_builder(fragment_source),
            cstring_from_builder(geometry_source),
            &temp, &error_at_compilation_stage);
    }
        
    //Error reporting (that is longer than the function itself :( )
    if(shader_program == 0)
    {
        #if defined(DO_LOG) && defined(DO_LOG_ERROR)
        //LOG_DEBUG(SHADER_UTIL_CHANEL, "vertex:\n" STRING_FMT, STRING_PRINT(vertex_source));
        //LOG_DEBUG(SHADER_UTIL_CHANEL, "fragment:\n" STRING_FMT, STRING_PRINT(fragment_source));
        //LOG_DEBUG(SHADER_UTIL_CHANEL, "geometry:\n" STRING_FMT, STRING_PRINT(geometry_source));

        String_Builder vertex_error_string = {scratch};
        String_Builder fragment_error_string = {scratch};
        String_Builder geometry_error_string = {scratch};

        if(vertex_error)
            file_translate_error(&vertex_error_string, vertex_error);
        if(fragment_error)
            file_translate_error(&fragment_error_string, fragment_error);
        if(geomtery_error)
            file_translate_error(&geometry_error_string, geomtery_error);

        //if has error print the translated version
        LOG_ERROR(SHADER_UTIL_CHANEL, "shader_load() failed! files:\n");
        log_group_push();
            LOG_ERROR(SHADER_UTIL_CHANEL, "vertex:   \"" STRING_FMT "\"", STRING_PRINT(vertex_path));
                log_group_push();
                if(vertex_error_string.size > 0)
                    LOG_ERROR(SHADER_UTIL_CHANEL, STRING_FMT, STRING_PRINT(vertex_error_string));
                if(error_at_compilation_stage == 1)
                    LOG_ERROR(SHADER_UTIL_CHANEL, STRING_FMT, STRING_PRINT(temp));
            log_group_pop();

            LOG_ERROR(SHADER_UTIL_CHANEL, "fragment: \"" STRING_FMT "\"", STRING_PRINT(fragment_path));
            log_group_push();
                if(fragment_error_string.size > 0)
                    LOG_ERROR(SHADER_UTIL_CHANEL, STRING_FMT, STRING_PRINT(fragment_error_string));
                if(error_at_compilation_stage == 2)
                    LOG_ERROR(SHADER_UTIL_CHANEL, STRING_FMT, STRING_PRINT(temp));
            log_group_pop();

            if(geometry_path.size > 0)
            {
                LOG_ERROR(SHADER_UTIL_CHANEL, "geometry: \"" STRING_FMT "\"", STRING_PRINT(geometry_path));
                log_group_push();
                    if(geometry_error_string.size > 0)
                        LOG_ERROR(SHADER_UTIL_CHANEL, STRING_FMT, STRING_PRINT(geometry_error_string));
                    if(error_at_compilation_stage == 3)
                        LOG_ERROR(SHADER_UTIL_CHANEL, STRING_FMT, STRING_PRINT(temp));
                log_group_pop();
            }
            
            if(prepend.size > 0)
            {
                LOG_ERROR(SHADER_UTIL_CHANEL, "prepend:");
                log_group_push();
                    LOG_ERROR(SHADER_UTIL_CHANEL, STRING_FMT, STRING_PRINT(prepend));
                log_group_pop();
            }

            if(error_at_compilation_stage == 4)
            {
                LOG_ERROR(SHADER_UTIL_CHANEL, "link:");
                log_group_push();
                    LOG_ERROR(SHADER_UTIL_CHANEL, STRING_FMT, STRING_PRINT(temp));
                log_group_pop();
            }
        log_group_pop();

        if(error)
        {
            if(vertex_error)
                array_copy(error, vertex_error_string);
            else if(fragment_error)
                array_copy(error, fragment_error_string);
            else if(geomtery_error)
                array_copy(error, geometry_error_string);
            else if(shader_program == 0)
                array_copy(error, temp);
        }

        array_deinit(&vertex_error_string);
        array_deinit(&fragment_error_string);
        array_deinit(&geometry_error_string);
        #endif
    }

    array_deinit(&vertex_source);
    array_deinit(&fragment_source);
    array_deinit(&geometry_source);
    array_deinit(&geometry_source);
    array_deinit(&temp);
    return shader_program;
}
    
void shader_unload(GLuint* shader)
{
    glUseProgram(0);
    glDeleteProgram(*shader);
    *shader = 0;
}

void shader_use(GLuint program)
{
    ASSERT(program);
    glUseProgram(program);
}

void shader_unuse()
{
    glUseProgram(0);
}

INTERNAL int _get_shader_uniform_location(GLuint program, const char* name)
{
    int location = glGetUniformLocation(program, name);
    if(location == -1)
    {
        //@NOTE: Exponential logging
        static i32 log_index = 0;
        if(is_power_of_two_zero(log_index++))
            LOG_ERROR("RENDER", "failed to find uniform location %s", name);
        //ASSERT(false);
    }

    return location;
}

bool shader_set_i32(GLuint program, const char* name, GLint val)
{
    int location = _get_shader_uniform_location(program, name);
    if(location == -1) 
        return false;

    glUniform1i(location, val);
    return true;
}
    
bool shader_set_f32(GLuint program, const char* name, GLfloat val)
{
    int location = _get_shader_uniform_location(program, name);
    if(location == -1)
        return false;

    glUniform1f(location, val);
    return true;
}

bool shader_set_vec3(GLuint program, const char* name, Vec3 val)
{
    int location = _get_shader_uniform_location(program, name);
    if(location == -1)
        return false;

    glUniform3fv(location, 1, AS_FLOATS(val));
    return true;
}

bool shader_set_mat4(GLuint program, const char* name, Mat4 val)
{
    int location = _get_shader_uniform_location(program, name);
    if(location == -1)
        return false;

    glUniformMatrix4fv(location, 1, GL_FALSE, AS_FLOATS(val));
    return true;
}
    
bool shader_set_mat3(GLuint program, const char* name, Mat3 val)
{
    int location = _get_shader_uniform_location(program, name);
    if(location == -1)
        return false;

    glUniformMatrix3fv(location, 1, GL_FALSE, AS_FLOATS(val));
    return true;
}
