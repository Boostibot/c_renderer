#ifndef LIB_FILE
#define LIB_FILE

#include "platform.h"
#include "allocator.h"
#include "string.h"
#include "error.h"

EXPORT Error file_read_entire_append_into(String file_path, String_Builder* append_into);
EXPORT Error file_read_entire(String file_path, String_Builder* data);
EXPORT Error file_append_entire(String file_path, String data);
EXPORT Error file_write_entire(String file_path, String data);
EXPORT Error file_create(String file_path, bool* was_just_created);
EXPORT Error file_remove(String file_path, bool* was_just_removed);

#endif

#if (defined(LIB_ALL_IMPL) || defined(LIB_FILE_IMPL)) && !defined(LIB_FILE_HAS_IMPL)
#define LIB_FILE_HAS_IMPL

EXPORT Error file_read_entire_append_into(String file_path, String_Builder* append_into)
{
    String_Builder escpaed_file_path = {0};
    array_init_backed(&escpaed_file_path, allocator_get_scratch(), 512);
    builder_append(&escpaed_file_path, file_path);

    Platform_Memory_Mapping mapping = {0};
    Platform_Error error = platform_file_memory_map(cstring_from_builder(escpaed_file_path), 0, &mapping);
    if(error == 0)
    {
        //@NOTE: if this fails because we dont have enough memory then the file remains mapped!
        //@TOOD: make this proper!
        array_append(append_into, (char*) mapping.address, mapping.size);
        platform_file_memory_unmap(&mapping);
    }

    array_deinit(&escpaed_file_path);
    return error_from_platform(error);
}

EXPORT Error file_read_entire(String file_path, String_Builder* data)
{
    array_clear(data);
    return file_read_entire_append_into(file_path, data);
}

EXPORT Error file_append_entire(String file_path, String contents)
{
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
    return error_from_platform(error);
}

EXPORT Error file_write_entire(String file_path, String contents)
{
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
    return error_from_platform(error);
}

EXPORT Error file_create(String file_path, bool* was_just_created)
{
    String_Builder escpaed_file_path = {0};
    array_init_backed(&escpaed_file_path, allocator_get_scratch(), 512);
    builder_append(&escpaed_file_path, file_path);

    Platform_Error error = platform_file_create(cstring_from_builder(escpaed_file_path), was_just_created);
    
    array_deinit(&escpaed_file_path);
    return error_from_platform(error);
}

EXPORT Error file_remove(String file_path, bool* was_just_removed)
{
    String_Builder escpaed_file_path = {0};
    array_init_backed(&escpaed_file_path, allocator_get_scratch(), 512);
    builder_append(&escpaed_file_path, file_path);

    Platform_Error error = platform_file_remove(cstring_from_builder(escpaed_file_path), was_just_removed);
    
    array_deinit(&escpaed_file_path);
    return error_from_platform(error);
}

#endif
