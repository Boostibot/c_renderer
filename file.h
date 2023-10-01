#ifndef LIB_FILE
#define LIB_FILE

#include "platform.h"
#include "allocator.h"
#include "string.h"
#include "error.h"
#include "profile.h"

EXPORT Error file_map(String file_path, isize desired_size, Error (func)(void* mapped, isize mapped_size, void* context), void* context);
EXPORT Error file_read_entire_append_into(String file_path, String_Builder* append_into);
EXPORT Error file_read_entire(String file_path, String_Builder* data);
EXPORT Error file_append_entire(String file_path, String data);
EXPORT Error file_write_entire(String file_path, String data);
EXPORT Error file_create(String file_path, bool* was_just_created);
EXPORT Error file_remove(String file_path, bool* was_just_removed);
EXPORT Error file_get_full_path(String_Builder* into, String path);


//Represents the following:
//C:/Users/Program_Files/./../Dir/file.txt
//<-><-------------------------->|<------>
// R         D                   |  F  <->
//                               M*      E
//                               
//Where:
//  R - root_size
//  D - directory_size
//  F - file_size
//  E - extension_size
//  M* - This / is explicitly not including in directory_size.
//       This is because non normalized directory paths can bu dont have to end
//       on /. This makes sure that both cases have the same size.
// 
typedef struct Path_Info {
    i32 root_size;
    i32 directories_size;
    i32 file_size;
    i32 extension_size;
} Path_Info;


//path_parse: String -> Path_Info fast entirely on my side. Should be okay since we have the parse.h. 
//path_validate: String -> File_Info
//path_get_full: String -> Path

//path_get_fr

//Path is fully safe path. It however does not have to point to a valid file.

String path_get_file_directory(String file_path)
{
    //@TODO: refactor out. Make handling of paths consistent!
    isize last_dir_index = string_find_last_char(file_path, '/') + 1;

    //@TODO: remove builder_from_string and make builder_from_string_alloc the default
    String dir_path = string_head(file_path, last_dir_index);
    return dir_path;
}
#endif

#if (defined(LIB_ALL_IMPL) || defined(LIB_FILE_IMPL)) && !defined(LIB_FILE_HAS_IMPL)
#define LIB_FILE_HAS_IMPL


EXPORT Error file_map(String file_path, isize desired_size, Error (func)(void* mapped, isize mapped_size, void* context), void* context)
{
    String_Builder escpaed_file_path = {0};
    array_init_backed(&escpaed_file_path, allocator_get_scratch(), 512);
    builder_append(&escpaed_file_path, file_path);
    Platform_Memory_Mapping mapping = {0};
    Platform_Error platform_error = platform_file_memory_map(cstring_from_builder(escpaed_file_path), desired_size, &mapping);
    Error output_error = {0};

    if(platform_error == 0)
    {
        //@NOTE: if this fails because we dont have enough memory then the file remains mapped!
        //@TODO: make this proper!
        output_error = func(mapping.address, mapping.size, context);
        platform_file_memory_unmap(&mapping);
    }
    else
    {
        output_error = error_from_platform(platform_error);
    }
    
    array_deinit(&escpaed_file_path);
    return output_error;
}

EXPORT Error file_read_entire_append_into(String file_path, String_Builder* append_into)
{
    PERF_COUNTER_START(c);
    String_Builder escpaed_file_path = {0};
    array_init_backed(&escpaed_file_path, allocator_get_scratch(), 512);
    builder_append(&escpaed_file_path, file_path);

    Platform_Memory_Mapping mapping = {0};
    Platform_Error error = platform_file_memory_map(cstring_from_builder(escpaed_file_path), 0, &mapping);
    if(error == 0)
    {
        //@NOTE: if this fails because we dont have enough memory then the file remains mapped!
        //@TODO: make this proper!
        array_append(append_into, (char*) mapping.address, mapping.size);
        platform_file_memory_unmap(&mapping);
    }

    array_deinit(&escpaed_file_path);
    PERF_COUNTER_END(c);
    return error_from_platform(error);
}

EXPORT Error file_read_entire(String file_path, String_Builder* data)
{
    array_clear(data);
    return file_read_entire_append_into(file_path, data);
}

EXPORT Error file_append_entire(String file_path, String contents)
{
    PERF_COUNTER_START(c);
    String_Builder escpaed_file_path = {0};
    array_init_backed(&escpaed_file_path, allocator_get_scratch(), 512);
    builder_append(&escpaed_file_path, file_path);

    Platform_Memory_Mapping mapping = {0};
    Platform_Error error = platform_file_memory_map(cstring_from_builder(escpaed_file_path), -contents.size, &mapping);
    if(error == 0)
    {
        char* bytes = mapping.address;
        char* last_bytes = bytes + mapping.size - contents.size;
        memcpy(last_bytes, contents.data, contents.size);
        platform_file_memory_unmap(&mapping);
    }

    array_deinit(&escpaed_file_path);
    PERF_COUNTER_END(c);
    return error_from_platform(error);
}

EXPORT Error file_write_entire(String file_path, String contents)
{
    PERF_COUNTER_START(c);
    String_Builder escpaed_file_path = {0};
    array_init_backed(&escpaed_file_path, allocator_get_scratch(), 512);
    builder_append(&escpaed_file_path, file_path);

    Platform_Memory_Mapping mapping = {0};
    Platform_Error error = platform_file_memory_map(cstring_from_builder(escpaed_file_path), contents.size, &mapping);
    if(error == 0)
    {
        memcpy(mapping.address, contents.data, contents.size);
        platform_file_memory_unmap(&mapping);
    }

    array_deinit(&escpaed_file_path);
    PERF_COUNTER_END(c);
    return error_from_platform(error);
}

EXPORT Error file_create(String file_path, bool* was_just_created)
{
    PERF_COUNTER_START(c);
    String_Builder escpaed_file_path = {0};
    array_init_backed(&escpaed_file_path, allocator_get_scratch(), 512);
    builder_append(&escpaed_file_path, file_path);

    Platform_Error error = platform_file_create(cstring_from_builder(escpaed_file_path), was_just_created);
    
    array_deinit(&escpaed_file_path);
    PERF_COUNTER_END(c);
    return error_from_platform(error);
}

EXPORT Error file_remove(String file_path, bool* was_just_removed)
{
    PERF_COUNTER_START(c);
    String_Builder escpaed_file_path = {0};
    array_init_backed(&escpaed_file_path, allocator_get_scratch(), 512);
    builder_append(&escpaed_file_path, file_path);

    Platform_Error error = platform_file_remove(cstring_from_builder(escpaed_file_path), was_just_removed);
    
    array_deinit(&escpaed_file_path);
    PERF_COUNTER_END(c);
    return error_from_platform(error);
}

EXPORT Error file_get_full_path(String_Builder* into, String path)
{
    //@TEMP: implement this in platform. Implement platform allocator support
    builder_assign(into, path);
    return ERROR_OK;
}

#endif
