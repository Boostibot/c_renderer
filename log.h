#ifndef LIB_LOG
#define LIB_LOG

//This is a simple immediate mode logging system.

//The main focus is to be able to split logs into chanels and types and then use these chanels
// to filter the output. For example we know that one system lets say animation - ANIM - system 
// is behaving oddly so we simply set the console to only display ANIM logs and only 
// ones that are WARNING, ERROR, or FATAL.

//There is also no initialization required for each chanel or type. Everything gets init on
// first use. Chanel settings and global setings can still be however set and are valid until
// that particular chanel is deinit via call to explicit deinit call.

//The syntax is: 
// LOG_INFO("ANIM", "iterating all entitites");
// log_group_push();
// for(int i = 0; i < 10; i++)
//     LOG_INFO("ANIM", "entity id:%d found", i);
// 
// log_group_pop();
// LOG_FATAL("ANIM", 
//    "Fatal error encountered!\n"
//    "Some more info\n" 
//    "%d-%d", 10, 20);
//

//Which results in the following files:
// log__<data>_<time>_<id>.txt
// log_ANIM_<data>_<time>_<id>.txt
// 
//Where log_..._.txt contains logs from all chanels and
// log_ANIM_..._.txt comtains just {ANIM} logs 
//
//Since we only logged {ANIM} both contain the same content:
// 
//[00:00:00 000  INFO] { ANIM} :iterating all entitites
//[00:00:00 000  INFO] { ANIM} .  :entity id:0 found
//                             .  :Hello from entity
//[00:00:00 000  INFO] { ANIM} .  :entity id:1 found
//                             .  :Hello from entity
//[00:00:00 000  INFO] { ANIM} .  :entity id:2 found
//                             .  :Hello from entity
//[00:00:00 000  INFO] { ANIM} .  :entity id:3 found
//                             .  :Hello from entity
//[00:00:00 000  INFO] { ANIM} .  :entity id:4 found
//                             .  :Hello from entity
//[00:00:00 000 FATAL] { ANIM} :Fatal error encountered!
//                             :Some more info
//                             :10-20

#include "string.h"
#include <stdarg.h>

#define LOG_TYPE_INFO    (u8) 0
#define LOG_TYPE_WARN    (u8) 1
#define LOG_TYPE_ERROR   (u8) 2
#define LOG_TYPE_FATAL   (u8) 3
#define LOG_TYPE_DEBUG   (u8) 4
#define LOG_TYPE_TRACE   (u8) 5
//... others can be defined ...

#define LOG(chanel, log_type, format, ...) log_message(string_make(chanel), log_type, format, __VA_ARGS__)

#define LOG_INFO(chanel, format, ...) LOG(chanel, LOG_TYPE_INFO, format, __VA_ARGS__)
#define LOG_WARN(chanel, format, ...) LOG(chanel, LOG_TYPE_WARN, format, __VA_ARGS__)
#define LOG_ERROR(chanel, format, ...) LOG(chanel, LOG_TYPE_ERROR, format, __VA_ARGS__)
#define LOG_FATAL(chanel, format, ...) LOG(chanel, LOG_TYPE_FATAL, format, __VA_ARGS__)
#define LOG_DEBUG(chanel, format, ...) LOG(chanel, LOG_TYPE_DEBUG, format, __VA_ARGS__)
#define LOG_TRACE(chanel, format, ...) LOG(chanel, LOG_TYPE_TRACE, format, __VA_ARGS__)

typedef struct Log_Chanel_Entry Log_Chanel_Entry;
typedef struct Log_System_Options Log_System_Options;
typedef struct Log_Chanel_Options Log_Chanel_Options;

EXPORT void log_message(String chanel, u8 log_type, const char* format, ...);
EXPORT void log_group_push();  
EXPORT void log_group_pop();
EXPORT i32  log_group_depth();
EXPORT void log_flush_all();
EXPORT void log_flush(String chanel);

EXPORT Log_Chanel_Options* log_chanel_options(String chanel); //@TODO
EXPORT Log_System_Options* log_system_options();
EXPORT void log_system_get_chanels(String_Array* chanels);
EXPORT void log_system_deinit();

typedef struct Log_System_Options {
    String_Builder file_output_directory_path;      //defaults to "logs/"
    String_Builder file_output_filename;            //defaults to "log_2023-07-23__23-03-01_0"
    isize buffer_size;                              //defaults to 4K
    double flush_every_s;                           //defaults to 2ms

    u64 console_takes_log_types;                    //defaults to all 1s in binary ie 0xFFFFFFFF    
    bool console_disabled;                          //defaults to false
    bool console_use_chanel_filter;                 //defaults to false
    String_Builder_Array console_tchannel_filter;   //defaults to {0} //@TODO

    u64 file_output_takes_log_types;                //defaults to all 1s in binary ie 0xFFFFFFFF    
    bool file_output_disabled;                      //defaults to false
    bool file_output_use_chanel_filter;             //defaults to true
    String_Builder_Array file_output_chanel_filter; //defaults to {0} //@TODO
} Log_System_Options;

//void log_add_file
typedef struct Log_Chanel_Entry {
    i64 epoch_time; 
    u8 log_type;
    i32 group_depth;
    String_Builder chanel;
    String_Builder message;
    isize chanel_index;
} Log_Chanel_Entry;

DEFINE_ARRAY_TYPE(Log_Chanel_Entry, Log_Chanel_Entry_Array);

EXPORT void log_process_message(Log_Chanel_Entry* save_into, String chanel, u8 log_type, const char* format, ...);
EXPORT void vlog_process_message(Log_Chanel_Entry* save_into, String chanel, u8 log_type, const char* format, va_list args);
EXPORT void log_format_entry(String_Builder* append_to, Log_Chanel_Entry entry);

EXPORT Log_Chanel_Entry_Array log_decode(String log);
EXPORT String_Builder         log_encode(Log_Chanel_Entry_Array array);

#endif

#if (defined(LIB_ALL_IMPL) || defined(LIB_LOG_IMPL)) && !defined(LIB_LOG_HAS_IMPL)
#define LIB_LOG_HAS_IMPL

#include <math.h>
#include <string.h> //should move to exposed "string.h"!

#include "format.h"
#include "platform.h"
#include "hash_table.h"
#include "hash.h"

#define _LOG_SYSTEM_EXTENSION STRING_LIT("txt")
#define _LOG_SYSTEM_CONSOLE_PRINT(cstring) printf(cstring)
#define _LOG_FILE_SLOTS 2
#define _LOG_DEF_BUFFER_FLUSH_SIZE 0
#define _LOG_DEF_FUSH_EVERY_S 0.02

typedef struct Log_Chanel_State {
    String_Builder buffer;
    String_Builder name;
    String_Builder file_name;
    String_Builder description;
    
    double last_flush_time;
    double flush_every_s;
    isize buffer_flush_size;

    u64 creation_epoch_time;
    bool first_record_written;
    bool is_init;
} Log_Chanel_State;

DEFINE_ARRAY_TYPE(Log_Chanel_State, Log_Chanel_State_Array);

//typedef struct Log_Chanel_Link {
//    u32 chanel_state_index;
//    u32 chanel_config_bitmask;
//} Log_Chanel_Link;

typedef struct 
{
    Log_System_Options system_options;
    u64 creation_epoch_time;
    i32 group_depth;
    bool is_init;

    ptr_Array open_files;
    String_Builder_Array open_file_names;
    isize open_files_max;
    isize central_chanel_index;

    Hash_Table64 chanel_hash;
    Log_Chanel_State_Array chanels;

    u64 last_hash;
    isize last_index;
} Log_System_Module_State;

static Log_System_Module_State global_log_state = {0};

EXPORT void log_process_message(Log_Chanel_Entry* save_into, String chanel, u8 log_type, const char* format, ...)
{
    va_list args;
    va_start(args, format);
    vlog_process_message(save_into, chanel, log_type, format, args);
    va_end(args);
}

EXPORT void vlog_process_message(Log_Chanel_Entry* save_into, String chanel, u8 log_type, const char* format, va_list args)
{
    save_into->epoch_time = platform_local_epoch_time();

    save_into->log_type = log_type;
    save_into->group_depth = log_group_depth();
    
    vformat_into(&save_into->message, format, args);
    save_into->chanel = builder_from_string(chanel);

    //assert(false && "add chanel handling here!");
}

INTERNAL void _log_system_create_log_name(String_Builder* into, u64 epoch_time, String chanel, String extension)
{
    Platform_Calendar_Time calendar = platform_epoch_time_to_calendar_time(epoch_time);
    format_into(into, "log_"STR_FMT"_%04d-%02d-%02d_%02d-%02d-%02d_%03d." STR_FMT, 
        STR_PRINT(chanel),
        (int) calendar.year, (int) calendar.month, (int) calendar.day, 
        (int) calendar.hour, (int) calendar.minute, (int) calendar.second,
        (int) calendar.millisecond, 
        STR_PRINT(extension));
}

INTERNAL void _log_system_init();

//typedef struct Log_Chanel_Link {
//    u32 chanel_state_index;
//    u32 chanel_config_bitmask;
//} Log_Chanel_Link;

EXPORT void string_builder_array_deinit(String_Builder_Array* array)
{
    for(isize i = 0; i < array->size; i++)
        array_deinit(&array->data[i]);

    array_deinit(array);
}

EXPORT void log_system_deinit()
{
    for(isize i = 0; i < global_log_state.chanels.size; i++)
    {
        Log_Chanel_State* state = &global_log_state.chanels.data[i];
        array_deinit(&state->buffer);
        array_deinit(&state->description);
        array_deinit(&state->file_name);
        array_deinit(&state->name);
    }

    Log_System_Options* options = &global_log_state.system_options;
    

    //file_output_directory_path
    array_deinit(&global_log_state.chanels);
    array_deinit(&options->file_output_directory_path);
    array_deinit(&options->file_output_filename);
    
    string_builder_array_deinit(&options->console_tchannel_filter);
    string_builder_array_deinit(&options->file_output_chanel_filter);
    string_builder_array_deinit(&global_log_state.open_file_names);

    for(isize i = 0; i < global_log_state.open_files.size; i++)
    {
        FILE* file = (FILE*) global_log_state.open_files.data[i];
        if(file)
            fclose(file);
    }

    array_deinit(&global_log_state.open_files);
    hash_table64_deinit(&global_log_state.chanel_hash);

    Log_System_Module_State null = {0};
    global_log_state = null;
    //ASSERT(false && "@TODO properly");
}

EXPORT void log_group_push()
{
    _log_system_init();
    global_log_state.group_depth += 1;
}
EXPORT void log_group_pop()
{
    _log_system_init();
    global_log_state.group_depth -= 1;
}
EXPORT i32  log_group_depth()
{
    _log_system_init();
    return global_log_state.group_depth;
}

EXPORT Log_System_Options* log_system_options()
{
    _log_system_init();
    return &global_log_state.system_options;
}

EXPORT void log_system_get_chanels(String_Array* chanels)
{
    for(isize i = 0; i < global_log_state.chanels.size; i++)
    {
        Log_Chanel_State* state = &global_log_state.chanels.data[i];
        String chanel = builder_string(state->name);
        array_push(chanels, chanel); 
    }
}

INTERNAL void _log_chanel_state_init(Log_Chanel_State* chanel_state, String chanel_name)
{
    if(chanel_state->is_init == false)
    {
        chanel_state->is_init = true;
        chanel_state->buffer_flush_size = global_log_state.system_options.buffer_size;
        chanel_state->flush_every_s = global_log_state.system_options.flush_every_s;
        chanel_state->creation_epoch_time = platform_local_epoch_time();
        array_clear(&chanel_state->name);
        array_clear(&chanel_state->file_name);

        builder_append(&chanel_state->name, chanel_name);
        _log_system_create_log_name(&chanel_state->file_name, chanel_state->creation_epoch_time, chanel_name, _LOG_SYSTEM_EXTENSION);

        array_grow(&chanel_state->buffer, chanel_state->buffer_flush_size);
    }
}


INTERNAL isize _log_find_init_chanel(String chanel)
{
    u64 chanel_hash = hash64_murmur(chanel.data, chanel.size, 0);

    isize chanel_index = -1;
    Log_System_Module_State* state = &global_log_state; 
    if(chanel_hash == state->last_hash)
        chanel_index = state->last_index;

    if(chanel_index == -1)
    {
        //@TODO: check hash conflicts
        isize found = hash_table64_find(state->chanel_hash, chanel_hash);
        if(found == -1)
        {
            Log_Chanel_State chanel_state = {0};
            found = hash_table64_insert(&state->chanel_hash, chanel_hash, state->chanels.size);
            array_push(&state->chanels, chanel_state);
        }

        chanel_index = state->chanel_hash.entries[found].value;
    }

    CHECK_BOUNDS(chanel_index, state->chanels.size);
    _log_chanel_state_init(&state->chanels.data[chanel_index], chanel);
    return chanel_index;
}

INTERNAL void _log_system_init()
{
    if(global_log_state.is_init == false)
    {
        u64 now = platform_local_epoch_time();
        global_log_state.is_init = true;
        global_log_state.last_index = -1;
        global_log_state.creation_epoch_time = now;
        global_log_state.central_chanel_index = global_log_state.chanels.size;
        global_log_state.open_files_max = _LOG_FILE_SLOTS;
        array_resize(&global_log_state.open_files, global_log_state.open_files_max);
        array_resize(&global_log_state.open_file_names, global_log_state.open_files_max);

        Log_Chanel_State state = {0};
        _log_chanel_state_init(&state, STRING_LIT(""));
        array_push(&global_log_state.chanels, state);
        
        Log_System_Options options = {0};
        options.buffer_size = _LOG_DEF_BUFFER_FLUSH_SIZE;
        options.flush_every_s = _LOG_DEF_FUSH_EVERY_S;
        options.console_takes_log_types = 0xFFFFFFFF;
        options.file_output_takes_log_types = 0xFFFFFFFF;
        
        options.file_output_directory_path = builder_from_cstring("logs/");
        array_copy(&options.file_output_filename, state.file_name);
        global_log_state.system_options = options;
    }
}

INTERNAL void _log_flush_chanel(Log_Chanel_State* chanel_state)
{
    if(chanel_state->buffer.size == 0)
        return;

    String chanel_file = builder_string(chanel_state->file_name);
    isize file_index = -1;
    
    isize furthest_flush_time_index = 0;
    double furthest_flush_time = INFINITY;

    for(isize i = 0; i < global_log_state.open_files_max; i++)
    {
        CHECK_BOUNDS(i, global_log_state.open_file_names.size);
        String open_file = builder_string(global_log_state.open_file_names.data[i]);
        if(string_is_equal(open_file, chanel_file))
            file_index = i;

        double flush_time = global_log_state.chanels.data[i].last_flush_time;
        if(furthest_flush_time > flush_time)
        {
            furthest_flush_time = flush_time;
            furthest_flush_time_index = i;
        }
    }

    if(file_index == -1)
    {
        file_index = furthest_flush_time_index;
        CHECK_BOUNDS(file_index, global_log_state.open_files_max);

        FILE* file = (FILE*) global_log_state.open_files.data[file_index];
        if(file != NULL)
            fclose(file);

        String_Builder local_path = {0};
        String dir_path = builder_string(global_log_state.system_options.file_output_directory_path);
        array_init_backed(&local_path, allocator_get_scratch(), 128);
        builder_append(&local_path, dir_path);
        builder_append(&local_path, STRING_LIT("/"));
        builder_append(&local_path, chanel_file);

        platform_directory_create(dir_path.data);
        file = fopen(local_path.data, "ab");

        ASSERT(file != NULL);
        bool ok = setvbuf(file, NULL, _IONBF, 0) == 0;
        ASSERT(ok && "disabeling of buffering must work!");

        global_log_state.open_files.data[file_index] = file;
        array_assign(&global_log_state.open_file_names.data[file_index], chanel_file.data, chanel_file.size);
        array_deinit(&local_path);
    }

    isize written = fwrite(chanel_state->buffer.data, 1, chanel_state->buffer.size, global_log_state.open_files.data[file_index]);
    ASSERT(written == chanel_state->buffer.size && "no posix partial write BS");
    array_clear(&chanel_state->buffer);
}

INTERNAL void _log_print_to_chanel(Log_Chanel_State* chanel_state, String message)
{
    builder_append(&chanel_state->buffer, message);
    
    double now = clock_s();
    double elapsed = now - chanel_state->last_flush_time;
    if(chanel_state->buffer.size >= chanel_state->buffer_flush_size 
        || elapsed > chanel_state->flush_every_s)
        _log_flush_chanel(chanel_state);
}

INTERNAL void log_flush_all()
{
    _log_system_init();
    for(isize i = 0; i < global_log_state.chanels.size; i++)
        _log_flush_chanel(&global_log_state.chanels.data[i]);
}

INTERNAL void log_flush(String chanel)
{
    _log_system_init();
    isize chanel_index = _log_find_init_chanel(chanel);
    _log_flush_chanel(&global_log_state.chanels.data[chanel_index]);
}

EXPORT void log_format_entry(String_Builder* append_to, Log_Chanel_Entry entry)
{
    String_Builder header_string = {0};
    array_init_backed(&header_string, allocator_get_scratch(), 128);
    
    const char* log_type_str = NULL;
    switch(entry.log_type)
    {
        case LOG_TYPE_INFO: log_type_str = "INFO"; break;
        case LOG_TYPE_WARN: log_type_str = "WARN"; break;
        case LOG_TYPE_ERROR: log_type_str = "ERROR"; break;
        case LOG_TYPE_FATAL: log_type_str = "FATAL"; break;
        case LOG_TYPE_DEBUG: log_type_str = "DEBUG"; break;
        case LOG_TYPE_TRACE: log_type_str = "TRACE"; break;
    }
    
    Platform_Calendar_Time c = platform_epoch_time_to_calendar_time(entry.epoch_time);
    if(log_type_str != NULL)
    {
        format_into(&header_string, "[%02d:%02d:%02d %03d %-5s] {%-5s} ", 
            (int) c.hour, (int) c.minute, (int) c.second, (int) c.millisecond, log_type_str, builder_cstring(entry.chanel));
    }
    else
    {
        format_into(&header_string, "[%02d:%02d:%02d %03d %-5d] {%-5s} ", 
            (int) c.hour, (int) c.minute, (int) c.second, (int) c.millisecond, (int) entry.log_type, builder_cstring(entry.chanel));
    }
    
    ASSERT(header_string.size != 0);

    String group_separator = string_make(".  ");
    String message_string = builder_string(entry.message);
    
    //Skip all trailing newlines
    isize message_size = message_string.size;
    for(; message_size > 0; message_size --)
    {
        if(message_string.data[message_size - 1] != '\n')
            break;
    }

    ASSERT(message_size <= message_string.size);

    isize size_before = append_to->size;
    array_grow(append_to, size_before + message_string.size + 10);

    isize curr_line_pos = 0;
    bool run = true;
    while(run)
    {
        if(curr_line_pos >= message_size)
            break;

        isize next_line_pos = string_find_first_char(message_string, '\n', curr_line_pos);
        if(next_line_pos == -1)
        {
            next_line_pos = message_size;
            run = false;
        }
        
        ASSERT(curr_line_pos < message_size);
        ASSERT(next_line_pos <= message_size);

        String curr_line = string_range(message_string, curr_line_pos, next_line_pos);

        //if is first line insert header
        if(curr_line_pos == 0)
        {
            builder_append(append_to, builder_string(header_string));
        }
        //else insert header-sized ammount of spaces
        else
        {
            isize before_padding = append_to->size;
            array_resize(append_to, before_padding + header_string.size);
            memset(append_to->data + before_padding, ' ', header_string.size);
        }

        //insert n times group separator
        for(isize i = 0; i < entry.group_depth; i++)
            builder_append(append_to, group_separator);
        
        array_push(append_to, ':');
        builder_append(append_to, curr_line);
        array_push(append_to, '\n');

        curr_line_pos = next_line_pos + 1;
    }

    array_deinit(&header_string);
}

EXPORT void log_message(String chanel, u8 log_type, const char* format, ...)
{
    ASSERT(log_type < 64);
    _log_system_init();
    
    String_Builder message = {0};
    Log_Chanel_Entry chanel_entry = {0};
    array_init_backed(&message, allocator_get_scratch(), 1024);
    array_init_backed(&chanel_entry.message, allocator_get_scratch(), 128);
    array_init_backed(&chanel_entry.chanel, allocator_get_scratch(), 32);

    va_list args;
    va_start(args, format);
    vlog_process_message(&chanel_entry, chanel, log_type, format, args);
    va_end(args);

    log_format_entry(&message, chanel_entry);

    Log_System_Module_State* state = &global_log_state; 
    Log_System_Options* opts = &state->system_options;
    u64 log_type_bit = (u64) 1 << log_type;
    
    if(opts->console_takes_log_types & log_type_bit)
    {
        _LOG_SYSTEM_CONSOLE_PRINT(message.data);
    }
    
    if(opts->file_output_takes_log_types & log_type_bit)
    {
        Log_Chanel_State* central_state = &state->chanels.data[state->central_chanel_index];
        _log_print_to_chanel(central_state, builder_string(message));
    }
    
    isize chanel_index = _log_find_init_chanel(chanel);
    Log_Chanel_State* chanel_state = &state->chanels.data[chanel_index];
    _log_print_to_chanel(chanel_state, builder_string(message));

    array_deinit(&message);
    array_deinit(&chanel_entry.message);
    array_deinit(&chanel_entry.chanel);
}

#endif
