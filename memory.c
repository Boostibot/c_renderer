#include <windows.h>
#include <memoryapi.h>
#include <processthreadsapi.h>
#include <stdint.h>
#include <stdbool.h>
#include <winnt.h>
#include <profileapi.h>
#include <intrin.h>

enum { WINDOWS_FILE_TYPE_PIPE = FILE_TYPE_PIPE };
#undef FILE_TYPE_PIPE

typedef int64_t isize;

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

typedef struct Thread
{
    void* handle;
    int32_t id;
} Thread;


void* platform_virtual_reallocate(void* allocate_at, isize bytes, Virtual_Allocation_Action action, Memory_Protection protection);
void* platform_heap_reallocate(isize new_size, void* old_ptr, isize old_size, isize align);

isize  platform_thread_get_proccessor_count();
Thread platform_thread_create(void (*func)(void*), void* context, isize stack_size); //CreateThread
void   platform_thread_destroy(Thread* thread); //CloseHandle 

isize  platform_thread_get_id();
void   platform_thread_yield();
void   platform_thread_sleep(isize ms);
void   platform_thread_exit(int code);
int    platform_thread_join(Thread thread);

#if 0
typedef struct Thread_Local_Key Thread_Local_Key;

bool  platform_thread_local_alloc(Thread_Local_Key* key);
bool  platform_thread_local_free(Thread_Local_Key* key);
void* platform_thread_local_get(isize key);
void  platform_thread_local_set(isize key, void* value);
#endif 

 //use inline statics
inline static void platform_compiler_memory_fence();
inline static void platform_memory_fence();
inline static void platform_processor_pause();
inline static int64_t platform_interlocked_excahnge64(volatile int64_t* target, int64_t value);
inline static int32_t platform_interlocked_excahnge32(volatile int32_t* target, int32_t value);

int64_t platform_perf_counter();            //returns the current value of performance counter
int64_t platform_perf_counter_base();       //returns the value of performence conuter at the first time this function was called which is taken as the startup time
int64_t platform_perf_counter_frequency();  //returns the frequency of the performance counter
double  platform_perf_counter_frequency_d(); //returns the frequency of the performance counter as double (saves expensive integer to float cast)

isize platform_utf32_to_utf8( const uint32_t* from, isize from_size,  char* to, isize to_size);
isize platform_utf8_to_utf32( const char* from, isize from_size,      uint32_t* to, isize to_size);

isize platform_utf32_to_utf16(const uint32_t* from, isize from_size,  uint16_t* to, isize to_size);
isize platform_utf16_to_utf32(const uint16_t* from, isize from_size,  uint32_t* to, isize to_size);

isize platform_utf8_to_utf16(const uint16_t* from, isize from_size,  uint32_t* to, isize to_size);
isize platform_utf16_to_utf8( const char* from, isize from_size,      uint32_t* to, isize to_size);

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
bool platform_file_info(const char* file_path, int64_t path_size, File_Info* info);
//Creates an empty file at the specified path
bool platform_file_create(const char* file_path, int64_t path_size);
//Removes a file at the specified path
bool platform_file_remove(const char* file_path, int64_t path_size);
//Moves or renames a file. If the file cannot be found or renamed to file already exists fails
bool platform_file_move(const char* new_path, int64_t new_path_size, const char* old_path, int64_t old_path_size);
//Copies a file. If the file cannot be found or copy_to_path file already exists fails
bool platform_file_copy(const char* copy_to_path, int64_t to_path_size, const char* copy_from_path, int64_t from_path_size);

//Makes an empty directory
bool platform_directory_create(const char* dir_path, int64_t path_size);
//Removes an empty directory
bool platform_directory_remove(const char* dir_path, int64_t path_size);
//changes the current working directory to the new_working_dir.  
bool platform_directory_set_current_working(const char* new_working_dir, int64_t path_size);    
//Retrieves the current working directory as allocated string. Needs to be freed using io_free()
char* platform_directory_get_current_working_malloc();    

//Gathers and allocates list of files in the specified directory. Saves a pointer to array of entries to entries and its size to entries_count. 
//Needs to be freed using directory_list_contents_free()
bool platform_directory_list_contents_malloc(const char* directory_path, int64_t path_size, Directory_Entry** entries, int64_t* entries_count, isize max_depth);
//Frees previously allocated file list
void platform_directory_list_contents_free(Directory_Entry* entries);

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
    POPUP_CONTROL_OK = 1 << 0,
    POPUP_CONTROL_CANCEL = 1 << 1,
    POPUP_CONTROL_CONTINUE = 1 << 2,
    POPUP_CONTROL_ABORT = 1 << 3,
    POPUP_CONTROL_RETRY = 1 << 4,
    POPUP_CONTROL_YES = 1 << 5,
    POPUP_CONTROL_NO = 1 << 6,
    POPUP_CONTROL_IGNORE = 1 << 7,
} Window_Popup_Controls;

Window_Popup_Controls platform_make_popup(Window_Popup_Style desired_style, char* message, char* title);


//Impl
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
    YieldProcessor();
}

inline static int64_t platform_interlocked_excahnge64(volatile int64_t* target, int64_t value)
{
    return (int64_t) _InterlockedExchange64((volatile LONG64*) target, (LONG64) value);
}

inline static int32_t platform_interlocked_excahnge32(volatile int32_t* target, int32_t value)
{
    return (int32_t) _InterlockedExchange((volatile LONG*) target, (LONG) value);
}

typedef struct Platform_Keyboard
{
    uint8_t keys[256];

} Platform_Keyboard;

#if 0
typedef enum Platform_Keys
{
    PLT_KEY_A = 'a',
    PLT_KEY_A = 'a',
    PLT_KEY_A = 'a',
    PLT_KEY_A = 'a',
    PLT_KEY_A = 'a',
    PLT_KEY_A = 'a',
    PLT_KEY_A = 'a',
    PLT_KEY_A = 'a',
    PLT_KEY_A = 'a',
    PLT_KEY_A = 'a',
    PLT_KEY_A = 'a',
    PLT_KEY_A = 'a',
    PLT_KEY_A = 'a',
    PLT_KEY_A = 'a',
    PLT_KEY_A = 'a',
    PLT_KEY_A = 'a',
    PLT_KEY_A = 'a',
    PLT_KEY_A = 'a',
    PLT_KEY_A = 'a',
    PLT_KEY_A = 'a',
    PLT_KEY_A = 'a',
    PLT_KEY_A = 'a',
} Platform_Keys;

void* platform_virtual_reallocate(void* adress, isize bytes, Virtual_Allocation_Action action, Memory_Protection protection)
{
    //CreateThread();
    if(action == VIRTUAL_ALLOC_RELEASE)
    {
        (void) VirtualFree(adress, 0, MEM_RELEASE);  
        return NULL;
    }

    if(action == VIRTUAL_ALLOC_DECOMMIT)
    {
        (void) VirtualFree(adress, bytes, MEM_DECOMMIT);  
        return NULL;
    }
 
    int prot = 0;
    if(protection == MEMORY_PROT_NO_ACCESS)
        prot = PAGE_NOACCESS;
    if(protection == MEMORY_PROT_READ)
        prot = PAGE_READONLY;
    else
        prot = PAGE_READWRITE;

    if(action == VIRTUAL_ALLOC_RESERVE)
        return VirtualAlloc(adress, bytes, MEM_RESERVE, prot);
    else
        return VirtualAlloc(adress, bytes, MEM_COMMIT, prot);
}

void* platform_heap_reallocate(isize new_size, void* old_ptr, isize old_size, isize align)
{
    if(new_size == 0)
    {
        _aligned_free(old_ptr);
        return NULL;
    }

    return _aligned_realloc(old_ptr, new_size, align);
}


isize platform_thread_get_proccessor_count()
{
    return GetCurrentProcessorNumber();
}

Thread platform_thread_create(void (*func)(void*), void* context, isize stack_size)
{
    Thread thread = {0};
    thread.handle = CreateThread(0, stack_size, func, context, 0, &thread.id);
    return thread;
}
void platform_thread_destroy(Thread* thread)
{
    Thread null = {0};
    CloseHandle(thread->handle);
    *thread = null;
}

isize platform_thread_get_id()
{
    return GetCurrentThreadId();
}
void platform_thread_yield()
{
    SwitchToThread();
}
void platform_thread_sleep(isize ms)
{
    Sleep(ms);
}

void platform_thread_exit(int code)
{
    ExitThread(code);
}

int platform_thread_join(Thread thread)
{
    WaitForSingleObject(thread.handle, INFINITE);
    WORD code = 0;
    GetExitCodeThread(thread.handle, &code);
    return code;
}


int64_t platform_perf_counter()
{
    LARGE_INTEGER ticks;
    ticks.QuadPart = 0;
    (void) QueryPerformanceCounter(&ticks);
    return ticks.QuadPart;
}

int64_t platform_perf_counter_base()
{
    static int64_t base = 0;
    if(base == 0)
        base = platform_perf_counter();
    return base;
}

int64_t platform_perf_counter_frequency()
{
    static int64_t freq = 0;
    if(freq == 0)
    {
        LARGE_INTEGER ticks;
        ticks.QuadPart = 0;
        (void) QueryPerformanceFrequency(&ticks);
        freq = ticks.QuadPart;
    }
    return freq;
}
    
double platform_perf_counter_frequency_d()
{
    static double freq = 0;
    if(freq == 0)
        freq = (double) platform_perf_counter_frequency(); 
    return freq;
}

Window_Popup_Controls platform_make_popup(Window_Popup_Style desired_style, char* message, char* title)
{
    int style = 0;
    int icon = 0;
    switch(desired_style)
    {
        case POPUP_STYLE_OK:            style = MB_OK; break;
        case POPUP_STYLE_ERROR:         style = MB_OK; icon = MB_ICONERROR; break;
        case POPUP_STYLE_WARNING:       style = MB_OK; icon = MB_ICONWARNING; break;
        case POPUP_STYLE_INFO:          style = MB_OK; icon = MB_ICONINFORMATION; break;
        case POPUP_STYLE_RETRY_ABORT:   style = MB_ABORTRETRYIGNORE; icon = MB_ICONWARNING; break;
        case POPUP_STYLE_YES_NO:        style = MB_YESNO; break;
        case POPUP_STYLE_YES_NO_CANCEL: style = MB_YESNOCANCEL; break;
        default: style = MB_OK; break;
    }

    int value = MessageBoxA(0, message, title, style | icon);

    switch(value)
    {
        case IDABORT: return POPUP_CONTROL_ABORT;
        case IDCANCEL: return POPUP_CONTROL_CANCEL;
        case IDCONTINUE: return POPUP_CONTROL_CONTINUE;
        case IDIGNORE: return POPUP_CONTROL_IGNORE;
        case IDYES: return POPUP_CONTROL_YES;
        case IDNO: return POPUP_CONTROL_NO;
        case IDOK: return POPUP_CONTROL_OK;
        case IDRETRY: return POPUP_CONTROL_RETRY;
        case IDTRYAGAIN: return POPUP_CONTROL_RETRY;
        default: return POPUP_CONTROL_OK;
    }
}
#endif