#ifndef LIB_FILE
#define LIB_FILE

#include "platform.h"
#include "allocator.h"
#include "string.h"

Platform_Error file_read_entire(String file_path, String_Builder* data);
Platform_Error file_append_entire(String file_path, String contents);
Platform_Error file_write_entire(String file_path, String data);

Platform_Error file_create(String file_path, bool* was_just_created);
Platform_Error file_remove(String file_path, bool* was_just_removed);
void file_translate_error(String_Builder* into, u64 error);

#endif


#define LIB_ALL_IMPL

#if (defined(LIB_ALL_IMPL) || defined(LIB_FILE_IMPL)) && !defined(LIB_FILE_HAS_IMPL)
#define LIB_FILE_HAS_IMPL

#include "hash_index.h"
#include "hash_table.h"
#include "time.h"
#include "log.h"

Platform_Error file_read_entire(String file_path, String_Builder* contents)
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
        array_append(contents, (char*) mapping.address, mapping.size);
        platform_file_memory_unmap(&mapping);
    }

    array_deinit(&escpaed_file_path);
    return error;
}

Platform_Error file_append_entire(String file_path, String contents)
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
    return error;
}

Platform_Error file_write_entire(String file_path, String contents)
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
    return error;
}


Platform_Error file_create(String file_path, bool* was_just_created)
{
    String_Builder escpaed_file_path = {0};
    array_init_backed(&escpaed_file_path, allocator_get_scratch(), 512);
    builder_append(&escpaed_file_path, file_path);

    Platform_Error error = platform_file_create(cstring_from_builder(escpaed_file_path), was_just_created);
    
    array_deinit(&escpaed_file_path);
}
Platform_Error file_remove(String file_path, bool* was_just_removed)
{
    String_Builder escpaed_file_path = {0};
    array_init_backed(&escpaed_file_path, allocator_get_scratch(), 512);
    builder_append(&escpaed_file_path, file_path);

    Platform_Error error = platform_file_remove(cstring_from_builder(escpaed_file_path), was_just_removed);
    
    array_deinit(&escpaed_file_path);
}
void file_translate_error(String_Builder* into, u64 error)
{
    char* msg = platform_translate_error_alloc(error);
    String msg_string = string_make(msg);

    builder_append(into, msg_string);

    platform_heap_reallocate(0, msg_string.data, msg_string.size, 8);
}

#endif
