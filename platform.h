#pragma once

#include <stdint.h>
#include <time.h>
#include <stdbool.h>

typedef int64_t isize;

//=========================================
// Virtual memory
//=========================================

typedef enum Virtual_Allocation_Action
{
    VIRTUAL_ALLOC_RESERVE, //Reserves adress space so that no other allocation can be made there
    VIRTUAL_ALLOC_COMMIT,  //Commits adress space causing operating system to suply physical memory or swap file
    VIRTUAL_ALLOC_DECOMMIT,//Removes adress space from commited freeing physical memory
    VIRTUAL_ALLOC_RELEASE, //Free adress space
} Virtual_Allocation_Action;

typedef enum Memory_Protection
{
    MEMORY_PROT_NO_ACCESS,
    MEMORY_PROT_READ,
    MEMORY_PROT_WRITE,
    MEMORY_PROT_READ_WRITE
} Memory_Protection;

void* platform_virtual_reallocate(void* allocate_at, isize bytes, Virtual_Allocation_Action action, Memory_Protection protection);
void* platform_heap_reallocate(isize new_size, void* old_ptr, isize old_size, isize align);


//=========================================
// Threading
//=========================================

typedef struct Thread
{
    void* handle;
    int32_t id;
} Thread;

isize  platform_thread_get_proccessor_count();
Thread platform_thread_create(int (*func)(void*), void* context, isize stack_size); //CreateThread
void   platform_thread_destroy(Thread* thread); //CloseHandle 

isize  platform_thread_get_id();
void   platform_thread_yield();
void   platform_thread_sleep(isize ms);
void   platform_thread_exit(int code);
int    platform_thread_join(Thread thread);


//=========================================
// Atomics 
//=========================================
inline static void platform_compiler_memory_fence();
inline static void platform_memory_fence();
inline static void platform_processor_pause();
inline static int64_t platform_interlocked_excahnge64(volatile int64_t* target, int64_t value);
inline static int32_t platform_interlocked_excahnge32(volatile int32_t* target, int32_t value);
inline static int32_t platform_interlocked_add32(volatile int32_t* target, int32_t value);
inline static int64_t platform_interlocked_add64(volatile int64_t* target, int64_t value);


//=========================================
// Timings
//=========================================

int64_t platform_perf_counter();            //returns the current value of performance counter
int64_t platform_perf_counter_base();       //returns the value of performence conuter at the first time this function was called which is taken as the startup time
int64_t platform_perf_counter_frequency();  //returns the frequency of the performance counter
double  platform_perf_counter_frequency_d(); //returns the frequency of the performance counter as double (saves expensive integer to float cast)


//=========================================
// Filesystem
//=========================================

typedef enum File_Type
{
    FILE_TYPE_NOT_FOUND = 0,
    FILE_TYPE_FILE = 1,
    FILE_TYPE_DIRECTORY = 4,
    FILE_TYPE_CHARACTER_DEVICE = 2,
    FILE_TYPE_PIPE = 3,
    FILE_TYPE_OTHER = 5,
} File_Type;

typedef struct File_Info
{
    int64_t size;
    File_Type type;
    time_t created_time;
    time_t last_write_time;  
    time_t last_access_time; //The last time file was either read or written
    bool is_link; //if file/dictionary is actually just a link (hardlink or softlink or symlink)
} File_Info;
    
typedef struct Directory_Entry
{
    char* path;
    int64_t path_size;
    int64_t index_within_directory;
    int64_t directory_depth;
    File_Info info;
} Directory_Entry;

//retrieves info about the specified file or directory
bool platform_file_info(const char* file_path, File_Info* info);
//Moves or renames a file. If the file cannot be found or renamed to file already exists fails
bool platform_file_move(const char* new_path, const char* old_path);
//Copies a file. If the file cannot be found or copy_to_path file already exists fails
bool platform_file_copy(const char* copy_to_path, const char* copy_from_path);

//Makes an empty directory
bool platform_directory_create(const char* dir_path);
//Removes an empty directory
bool platform_directory_remove(const char* dir_path);
//changes the current working directory to the new_working_dir.  
bool platform_directory_set_current_working(const char* new_working_dir);    
//Retrieves the current working directory as allocated string. Needs to be freed using io_free()
char* platform_directory_get_current_working_malloc();    

//Gathers and allocates list of files in the specified directory. Saves a pointer to array of entries to entries and its size to entries_count. 
//Needs to be freed using directory_list_contents_free()
bool platform_directory_list_contents_malloc(const char* directory_path, Directory_Entry** entries, int64_t* entries_count, isize max_depth);
//Frees previously allocated file list
void platform_directory_list_contents_free(Directory_Entry* entries);


//=========================================
// Window managmenet
//=========================================

typedef struct Window Window;

typedef struct Input {
    int64_t mouse_relative_x;
    int64_t mouse_relative_y;

    int64_t mouse_app_resolution_x;
    int64_t mouse_app_resolution_y;

    uint8_t keys[256];
    bool should_quit;
    
    int64_t window_position_x;
    int64_t window_position_y;
    int64_t window_size_x;
    int64_t window_size_y;
} Input;

typedef enum Window_Visibility
{
    WINDOW_VISIBILITY_FULLSCREEN,
    WINDOW_VISIBILITY_BORDERLESS_FULLSCREEN,
    WINDOW_VISIBILITY_WINDOWED,
    WINDOW_VISIBILITY_MINIMIZED,
} Window_Visibility;

typedef enum Window_Popup_Style
{
    POPUP_STYLE_OK,
    POPUP_STYLE_ERROR,
    POPUP_STYLE_WARNING,
    POPUP_STYLE_INFO,
    POPUP_STYLE_RETRY_ABORT,
    POPUP_STYLE_YES_NO,
    POPUP_STYLE_YES_NO_CANCEL,
} Window_Popup_Style;

typedef enum Window_Popup_Controls
{
    POPUP_CONTROL_OK,
    POPUP_CONTROL_CANCEL,
    POPUP_CONTROL_CONTINUE,
    POPUP_CONTROL_ABORT,
    POPUP_CONTROL_RETRY,
    POPUP_CONTROL_YES,
    POPUP_CONTROL_NO,
    POPUP_CONTROL_IGNORE,
} Window_Popup_Controls;

Window* platform_window_init(const char* window_title);
void    platform_window_deinit(Window* window);
Input   platform_window_process_messages(Window* window);

void platform_window_set_visibility(Window* window, Window_Visibility visibility, bool focused);
Window_Popup_Controls platform_window_make_popup(Window_Popup_Style desired_style, const char* message, const char* title);

// =================== INLINE IMPLEMENTATION ============================
#if defined(_MSC_VER)
    #include <intrin.h>
    inline static void platform_compiler_memory_fence() 
    {
        _ReadWriteBarrier();
    }

    inline static void platform_memory_fence()
    {
        _ReadWriteBarrier(); 
        __faststorefence();
    }

    inline static void platform_processor_pause()
    {
        _mm_pause();
    }

    inline static int64_t platform_interlocked_excahnge64(volatile int64_t* target, int64_t value)
    {
        return (int64_t) _InterlockedExchange64((volatile long long*) target, (long long) value);
    }

    inline static int32_t platform_interlocked_excahnge32(volatile int32_t* target, int32_t value)
    {
        return (int32_t) _InterlockedExchange((volatile long*) target, (long) value);
    }
    
    inline static int64_t platform_interlocked_add64(volatile int64_t* target, int64_t value)
    {
        return (int64_t) _InterlockedAddLargeStatistic((volatile long long*) target, (long long) value);
    }

    inline static int32_t platform_interlocked_add32(volatile int32_t* target, int32_t value)
    {
        return (int32_t) _InterlockedAdd((volatile long*) target, (long) value);
    }
    
#endif