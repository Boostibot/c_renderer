#ifndef LIB_FILE
#define LIB_FILE

#include "platform.h"
#include "allocator.h"
#include "string.h"

#define FILE_FLAG_BUFFERED      (isize) 1
#define FILE_FLAG_UNBUFFERED    (isize) 2
#define FILE_FLAG_CREATE        (isize) 4
#define FILE_FLAG_CREATE_NEW    (isize) 8
#define FILE_FLAG_APPEND        (isize) 16

#define FILE_DEF_BUFFER_SIZE    (isize) 4096
#define FILE_DEF_STREAMS        (isize) 4
#define FILE_DEF_FLAGS          FILE_FLAG_BUFFERED
#define FILE_DEF_KEEP_FILE_STREAMS (isize) 4

typedef enum File_IO_State
{
    FILE_STATE_OK,
    FILE_STATE_ERROR,
    FILE_STATE_EOF,
} File_IO_State;

typedef struct File_IO_Result
{
    File_IO_State state;
    isize processed;
    u64 error; //if 0 then no error
} File_IO_Result;

void file_system_init(Allocator* default_allocator, Allocator* scratch_allocator);
void file_system_deinit();

File_IO_Result file_read(String file_path, isize position, isize size, void* data);
File_IO_Result file_write(String file_path, isize position, isize size, const void* data);

Platform_Error file_read_entire(String file_path, String_Builder* data);
Platform_Error file_append_entire(String file_path, String contents);
Platform_Error file_write_entire(String file_path, String data);

bool file_create(String file_path);
bool file_remove(String file_path);

bool file_close(String file_path); //flushes and removes file from active. If file is not active does nothing
bool file_flush(String file_path); 

void file_set_options(String file_path, isize buffer_size, isize flags);

void file_system_set_options(isize concurrent_streams);

String_Builder_Array file_system_get_active_files();
void file_system_translate_error(String_Builder* into, u64 error);

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

typedef struct File_State
{
    String_Builder file_path;
    isize file_stream_index;
    isize buffer_size;
    Platform_Calendar_Time init_time;
    bool is_init;
} File_State;

typedef struct File_Stream
{
    FILE* handle;
    String_Builder file_path;
    String_Builder buffer;
    f64   last_use_time;
    isize position;
    isize file_size;
    isize written;
} File_Stream;

DEFINE_ARRAY_TYPE(File_Stream, File_Stream_Array);
DEFINE_ARRAY_TYPE(File_State, File_State_Array);
DEFINE_HASH_TABLE_TYPE(File_State, File_State_Hash_Table)

typedef struct File_System_Module_State
{
    Allocator* default_allocator;
    Allocator* scratch_allocator;

    File_State_Hash_Table file_state_table;
    File_Stream_Array file_streams;
    
    //array of unused file buffers
    String_Builder_Array unused_buffers;
    isize default_buffer_size;
    
    f64  init_time;
    bool is_init;
} File_System_Module_State;

File_System_Module_State global_file_module_state = {0};

String_Builder _file_get_optimal_buffer(isize needed_buffer_size)
{
    //try to find best sized buffer that is the smallest buffer that is just bigger than 
    //the needed size
    isize best_index = -1;
    isize best_delta = INT64_MAX;
    for(isize i = 0; i < global_file_module_state.unused_buffers.size; i++)
    {
        isize curr_buffer_size = global_file_module_state.unused_buffers.data[i].size;
        isize delta = curr_buffer_size - needed_buffer_size;
        if(delta > 0 && delta < best_delta)
        {
            best_delta = delta;
            best_index = i;
        }
    }

    String_Builder out = {0};
    //If nothing was found make one
    if(best_index == -1)
    {
        array_init(&out, global_file_module_state.default_allocator);
        array_resize(&out, needed_buffer_size);
    }
    //else steal it from the array
    else
    {
        //@TODO: make a function for this
        String_Builder* best_buffer = array_get(global_file_module_state.unused_buffers, best_index);
        String_Builder* last_buffer = array_last(global_file_module_state.unused_buffers);

        out = *best_buffer;
        *best_buffer = *last_buffer;
        array_pop(&global_file_module_state.unused_buffers);
    }

    return out;
}

void _file_recycle_buffer(String_Builder* buffer)
{
    array_push(&global_file_module_state.unused_buffers, *buffer);
    *buffer = BRACE_INIT(String_Builder){0};
}

void _file_state_init(File_State* state, String path)
{
    if(state->is_init != false)
        return;
        
    state->is_init = true;
    state->buffer_size = global_file_module_state.default_buffer_size;
    state->init_time = platform_epoch_time_to_calendar_time(platform_local_epoch_time());
    state->file_path = builder_from_string_alloc(path, global_file_module_state.default_allocator);
}

void _file_state_deinit(File_State* state)
{
    if(state->is_init == false)
        return;
    
    array_deinit(&state->file_path);
    memset(state, 0, sizeof *state);
}

void _file_stream_deinit(File_Stream* state)
{
    if(state->handle != NULL)
        fclose(state->handle);

    state->handle = NULL;
    array_deinit(&state->file_path);
}

void file_system_init(Allocator* default_allocator, Allocator* scratch_allocator)
{
    if(global_file_module_state.is_init == false)
    {
        LOG_INFO("FILE", "File system init");
        global_file_module_state.init_time = clock_s();
        global_file_module_state.default_allocator = default_allocator;
        global_file_module_state.scratch_allocator = scratch_allocator;
        global_file_module_state.default_buffer_size = FILE_DEF_BUFFER_SIZE;

        hash_table_init(&global_file_module_state.file_state_table, default_allocator, sizeof(File_State), 0);
        array_init(&global_file_module_state.file_streams, default_allocator);
        array_init(&global_file_module_state.unused_buffers, default_allocator);

        isize file_streams = FOPEN_MAX - FILE_DEF_KEEP_FILE_STREAMS;
        if(file_streams <= 0)
            file_streams = 2;

        file_streams = 1; 
        array_resize(&global_file_module_state.file_streams, file_streams);
        
        for(isize i = 0; i < global_file_module_state.file_streams.size; i++)
        {
            File_Stream* stream = &global_file_module_state.file_streams.data[i];
            array_init(&stream->file_path, default_allocator);
        }

        global_file_module_state.is_init = true;
    }
    else
    {
        LOG_ERROR("FILE", "Attempted to init FILE system again!");
    }
}

void file_system_deinit()
{
    File_System_Module_State* module_state = &global_file_module_state;
    if(module_state->is_init == true)
    {
        LOG_INFO("FILE", "File system deinit");
        for(isize i = 0; i < module_state->file_streams.size; i++)
            _file_stream_deinit(&module_state->file_streams.data[i]);
            
        for(isize i = 0; i < module_state->file_state_table.size; i++)
            _file_state_deinit(&module_state->file_state_table.values[i]);

        for(isize i = 0; i < module_state->unused_buffers.size; i++)
            array_deinit(&module_state->unused_buffers.data[i]);

        hash_table_deinit(&module_state->file_state_table);
        array_deinit(&module_state->file_streams);
        array_deinit(&module_state->unused_buffers);
        memset(module_state, 0, sizeof *module_state);
    }
    else
    {
        LOG_ERROR("FILE", "Attempted to deinit FILE system again!");
    }
}

File_IO_Result _file_read_or_write(String file_path, isize position, isize size, void* data, bool is_read)
{
    File_IO_Result result = {FILE_STATE_ERROR};
    File_System_Module_State* module_state = &global_file_module_state;
    const char* function_name = is_read ? "file_read" : "file_write";

    if(module_state->is_init == false)
    {
        LOG_ERROR("FILE", "%s called without file_system_init while reading from: " STRING_FMT, function_name, STRING_PRINT(file_path));
        return result;
    }

    File_State* file_state = (File_State*) hash_table_get_or_make(&module_state->file_state_table, file_path, NULL);
    _file_state_init(file_state, file_path);
    
    File_Stream* file_stream = array_get(module_state->file_streams, file_state->file_stream_index);
    
    //Checks if the stream is still owned by this state. 
    //If not tries to 'steal' the least recently used stream
    if(builder_is_equal(file_stream->file_path, file_state->file_path) == false)
    {
        //Finds the stream that wasnt used for the longest time and use that one
        isize oldest_stream_index = 0;
        f64 oldest_stream_time = INFINITY;
        for(isize i = 0; i < module_state->file_streams.size; i++)
        {
            File_Stream* stream = &module_state->file_streams.data[i];
            if(oldest_stream_time > stream->last_use_time)
            {
                oldest_stream_time = stream->last_use_time;
                oldest_stream_index = i;
            }
        }
        
        file_state->file_stream_index = oldest_stream_index;
        file_stream = array_get(module_state->file_streams, file_state->file_stream_index);
        file_stream->position = 0;
        file_stream->file_size = 0;


        //reopen it for the desired filename
        const char* c_file_path = cstring_from_builder(file_state->file_path);
        const char* open_mode = "r+b";
        if(file_stream->handle == NULL)
            file_stream->handle = fopen(c_file_path, open_mode);
        else
            file_stream->handle = freopen(c_file_path, open_mode, file_stream->handle);
        
        if(file_stream->handle == NULL)
        {
            result.error = (u64) errno;
            LOG_ERROR("FILE", "%s couldnt open file: " STRING_FMT, function_name, STRING_PRINT(file_path));
            goto error;
        }
        
        if(file_stream->buffer.size > file_state->buffer_size)
        {
            
        }

        if(setvbuf(file_stream->handle, (char*) file_stream->buffer.data, _IOFBF, file_stream->buffer.size) != 0)
        {
            result.error = (u64) errno;
            LOG_ERROR("FILE", "%s couldnt set buffering: " STRING_FMT, function_name, STRING_PRINT(file_path));
            goto error;
        }

        fseek(file_stream->handle, 0, SEEK_END);
        isize file_size = ftell(file_stream->handle);
        fseek(file_stream->handle, 0, SEEK_SET);
        
        file_stream->file_size = file_size;
        array_assign(&file_stream->file_path, file_path.data, file_path.size);
    }

    {
        ASSERT(builder_is_equal(file_state->file_path, file_stream->file_path));
        ASSERT(string_is_equal(string_from_builder(file_state->file_path), file_path));
        ASSERT(file_stream->handle != NULL);

        isize true_position = position;
        if(true_position < 0)
            true_position = file_stream->file_size;

        //seek to appropriate position if it differs from the one within the file_stream
        if(file_stream->position != true_position)
        {
            bool okay = fseek(file_stream->handle, (long) true_position, SEEK_SET) == 0;
            ASSERT(okay);
            file_stream->position = true_position;
        }

        //Read/write
        if(is_read)
            result.processed = fread(data, 1, size, file_stream->handle);
        else
            result.processed = fwrite(data, 1, size, file_stream->handle);
        
        if(result.processed == size)
            result.state = FILE_STATE_OK;
        else
        {
            bool had_eof = !!feof(file_stream->handle);
            result.error = (u64) errno;
            result.state = had_eof ? FILE_STATE_EOF : FILE_STATE_ERROR;
        }
            
        file_stream->last_use_time = clock_s();
        file_stream->position += result.processed;
        if(file_stream->file_size < file_stream->position)
            file_stream->file_size = file_stream->position;


    }

    error:
    if(result.error != 0)
    {
        #if defined(DO_LOG) && defined(DO_LOG_ERROR)
            String_Builder error_msg = {module_state->scratch_allocator};
            file_system_translate_error(&error_msg, result.error);
            LOG_ERROR("FILE", "%s error from file: " STRING_FMT, function_name, STRING_PRINT(error_msg), STRING_PRINT(file_path));
            log_group_push();
                LOG_ERROR("FILE", STRING_FMT, STRING_PRINT(error_msg));
            log_group_pop();
            array_deinit(&error_msg);
        #endif
    }

    return result;
}

File_IO_Result file_read(String file_path, isize position, isize size, const void* data)
{
    return _file_read_or_write(file_path, position, size, (void*) data, true);
}

File_IO_Result file_write(String file_path, isize position, isize size, void* data)
{
    return _file_read_or_write(file_path, position, size, (void*) data, false);
}

bool file_close(String file_path)
{
    Hash_Found found = hash_table_find(&global_file_module_state.file_state_table, file_path);;
    if(found.entry == -1)
        return false;
    File_State* file_state = &global_file_module_state.file_state_table.values[found.entry];
        
    _file_state_deinit(file_state);
    hash_table_remove_found(&global_file_module_state.file_state_table, found, NULL);
    return true;
}

bool file_flush(String file_path)
{
    File_State* file_state = (File_State*) hash_table_get(&global_file_module_state.file_state_table, file_path);
    if(file_state == NULL)
        return false;

    if(file_state->file_stream_index != -1)
    {
        File_Stream* stream = array_get(global_file_module_state.file_streams, file_state->file_stream_index);
        if(stream->handle != NULL)
            fflush(stream->handle);
    }
    return true;
}

void file_system_translate_error(String_Builder* into, u64 error)
{
    builder_append(into, STRING("<unimplemented>"));
}

bool file_create(String file_path)
{
    String_Builder escaped_path = {0};
    array_init_backed(&escaped_path, global_file_module_state.scratch_allocator, 512);
    builder_append(&escaped_path, file_path);

    FILE* opened = fopen(cstring_from_builder(escaped_path), "a");
    fclose(opened);

    array_deinit(&escaped_path);

    return opened != NULL;
}

bool file_remove(String file_path)
{
    String_Builder escaped_path = {0};
    array_init_backed(&escaped_path, global_file_module_state.scratch_allocator, 512);
    builder_append(&escaped_path, file_path);

    bool state = remove(cstring_from_builder(escaped_path));

    array_deinit(&escaped_path);

    return state;
}

#endif
