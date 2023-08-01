#include "platform.h"

#include <assert.h>
#include <windows.h>

#include <gl\gl.h>          // Header File For The OpenGL32 Library
#include <gl\glu.h>         // Header File For The GLu32 Library

#include <memoryapi.h>
#include <processthreadsapi.h>
#include <winnt.h>
#include <profileapi.h>
#include <hidusage.h>
#include <windowsx.h>
#include <intrin.h>
#include <direct.h>
#include <dwmapi.h>
#pragma comment(lib, "dwmapi.lib")


enum { WINDOWS_PLATFORM_FILE_TYPE_PIPE = PLATFORM_FILE_TYPE_PIPE };
#undef PLATFORM_FILE_TYPE_PIPE

//=========================================
// Virtual memory
//=========================================
void* platform_virtual_reallocate(void* adress, int64_t bytes, Platform_Virtual_Allocation action, Platform_Memory_Protection protection)
{
    //CreateThread();
    if(action == PLATFORM_VIRTUAL_ALLOC_RELEASE)
    {
        (void) VirtualFree(adress, 0, MEM_RELEASE);  
        return NULL;
    }

    if(action == PLATFORM_VIRTUAL_ALLOC_DECOMMIT)
    {
        (void) VirtualFree(adress, bytes, MEM_DECOMMIT);  
        return NULL;
    }
 
    int prot = 0;
    if(protection == PLATFORM_MEMORY_PROT_NO_ACCESS)
        prot = PAGE_NOACCESS;
    if(protection == PLATFORM_MEMORY_PROT_READ)
        prot = PAGE_READONLY;
    else
        prot = PAGE_READWRITE;

    if(action == PLATFORM_VIRTUAL_ALLOC_RESERVE)
        return VirtualAlloc(adress, bytes, MEM_RESERVE, prot);
    else
        return VirtualAlloc(adress, bytes, MEM_COMMIT, prot);
}

void* platform_heap_reallocate(int64_t new_size, void* old_ptr, int64_t old_size, int64_t align)
{
    if(new_size == 0)
    {
        _aligned_free(old_ptr);
        return NULL;
    }

    return _aligned_realloc(old_ptr, new_size, align);
}

//=========================================
// Threading
//=========================================
int64_t platform_thread_get_proccessor_count()
{
    return GetCurrentProcessorNumber();
}

//typedef DWORD (WINAPI *PTHREAD_START_ROUTINE)(
//    LPVOID lpThreadParameter
//    );
Platform_Thread platform_thread_create(int (*func)(void*), void* context, int64_t stack_size)
{
    Platform_Thread thread = {0};
    thread.handle = CreateThread(0, stack_size, func, context, 0, &thread.id);
    return thread;
}
void platform_thread_destroy(Platform_Thread* thread)
{
    Platform_Thread null = {0};
    CloseHandle(thread->handle);
    *thread = null;
}

int64_t platform_thread_get_id()
{
    return GetCurrentThreadId();
}
void platform_thread_yield()
{
    SwitchToThread();
}
void platform_thread_sleep(int64_t ms)
{
    Sleep((DWORD) ms);
}

void platform_thread_exit(int code)
{
    ExitThread(code);
}

int platform_thread_join(Platform_Thread thread)
{
    WaitForSingleObject(thread.handle, INFINITE);
    DWORD code = 0;
    GetExitCodeThread(thread.handle, &code);
    return code;
}


//=========================================
// Timings
//=========================================
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


//typedef struct Platform_Calendar_Time
//{
//    int8_t year;        // any
//    int8_t month;       // [0, 12)
//    int8_t day_of_week; // [0, 7)
//    int8_t day;// [0, 31] !note the end bracket!
//    
//    int8_t hour;        // [0, 24)
//    int8_t minute;      // [0, 60)
//    int8_t second;      // [0, 60)
//    
//    int16_t millisecond; // [0, 1000)
//    int16_t day_of_year; // [0, 356]
//} Platform_Calendar_Time;

//time -> filetime
//(ts * 10000000LL) + 116444736000000000LL = t
//(ts * 10LL) + 116444736000LL = tu = t/1 0000 000
    
//filetime -> time
//ts = (t - 116444736000000000LL) / 10000000;
//t / 10000000 - 11644473600LL = ts;
//t / 10 - 11644473600 000 000LL = tu = ts*1 0000 000;
static int64_t _filetime_to_epoch_time(FILETIME t)  
{    
    ULARGE_INTEGER ull;    
    ull.LowPart = t.dwLowDateTime;    
    ull.HighPart = t.dwHighDateTime;
    int64_t tu = ull.QuadPart / 10 - 11644473600000000LL;
    return tu;
}

static FILETIME _epoch_time_to_filetime(int64_t tu)  
{    
    ULARGE_INTEGER time_value;
    FILETIME time;
    time_value.QuadPart = (tu + 11644473600000000LL)*10;

    time.dwLowDateTime = time_value.LowPart;
    time.dwHighDateTime = time_value.HighPart;
    return time;
}
static time_t _filetime_to_time_t(FILETIME ft)  
{    
    ULARGE_INTEGER ull;    
    ull.LowPart = ft.dwLowDateTime;    
    ull.HighPart = ft.dwHighDateTime;    
    return (time_t) (ull.QuadPart / 10000000ULL - 11644473600ULL);  
}

int64_t platform_universal_epoch_time()
{
    FILETIME filetime;
    GetSystemTimeAsFileTime(&filetime);
    int64_t epoch_time = _filetime_to_epoch_time(filetime);
    return epoch_time;
}

int64_t platform_local_epoch_time()
{
    FILETIME filetime;
    GetSystemTimeAsFileTime(&filetime);
    FILETIME local_filetime = {0};
    bool okay = FileTimeToLocalFileTime(&filetime, &local_filetime);
    assert(okay);
    int64_t epoch_time = _filetime_to_epoch_time(local_filetime);
    return epoch_time;
}

Platform_Calendar_Time platform_epoch_time_to_calendar_time(int64_t epoch_time_usec)
{
    #define EPOCH_YEAR              (int64_t) 1970
    #define MILLISECOND_MICROSECOND (int64_t) 1000
    #define SECOND_MICROSECONDS     (int64_t) 1000000
    #define DAY_SECONDS             (int64_t) 86400
    #define YEAR_SECONDS            (int64_t) 31556952
    #define DAY_MICROSECONDS        (DAY_SECONDS * SECOND_MICROSECONDS)
    #define YEAR_MICROSECONDS       (YEAR_SECONDS * SECOND_MICROSECONDS)

    SYSTEMTIME systime = {0};
    FILETIME filetime = _epoch_time_to_filetime(epoch_time_usec);
    bool okay = FileTimeToSystemTime(&filetime, &systime);
    assert(okay);

    Platform_Calendar_Time time = {0};
    time.day = systime.wDay;
    time.day_of_week = systime.wDayOfWeek - 1;
    time.hour = systime.wHour;
    time.millisecond = systime.wMilliseconds;
    time.minute = systime.wMinute;
    time.month = systime.wMonth;
    time.second = systime.wSecond;
    time.year = systime.wYear;

    int64_t years_since_epoch = (int64_t) time.year - EPOCH_YEAR;
    int64_t microsec_diff = epoch_time_usec - years_since_epoch*YEAR_MICROSECONDS;
    int64_t microsec_remainder = microsec_diff % YEAR_MICROSECONDS;

    int64_t day_of_year = (microsec_remainder / DAY_MICROSECONDS);
    //int64_t microsecond = (microsec_remainder % MILLISECOND_MICROSECOND);
    int64_t microsecond = (microsec_diff % MILLISECOND_MICROSECOND);

    //time.day_of_year = (int16_t) day_of_year;
    time.microsecond = (int16_t) microsecond;

    //int64_t times = time.day_of_year;
    assert(0 <= time.month && time.month < 12);
    assert(0 <= time.day && time.day < 31);
    assert(0 <= time.hour && time.hour < 24);
    assert(0 <= time.minute && time.minute < 60);
    assert(0 <= time.second && time.second < 60);
    assert(0 <= time.millisecond && time.millisecond < 999);
    assert(0 <= time.day_of_week && time.day_of_week < 7);
    //assert(0 <= time.day_of_year && time.day_of_year <= 365);
    return time;
}

int64_t platform_calendar_time_to_epoch_time(Platform_Calendar_Time calendar_time)
{
    SYSTEMTIME systime = {0};
    systime.wDay = calendar_time.day;
    systime.wDayOfWeek = calendar_time.day_of_week + 1;
    systime.wHour = calendar_time.hour;
    systime.wMilliseconds = calendar_time.millisecond;
    systime.wMinute = calendar_time.minute;
    systime.wMonth = calendar_time.month;
    systime.wSecond = calendar_time.second;
    systime.wYear = calendar_time.year;

    FILETIME filetime;
    bool okay = SystemTimeToFileTime(&systime, &filetime);
    assert(okay);

    int64_t epoch_time = _filetime_to_epoch_time(filetime);
    epoch_time += calendar_time.microsecond;

    #ifndef NDEBUG
    Platform_Calendar_Time roundtrip = platform_epoch_time_to_calendar_time(epoch_time);
    assert(memcmp(&roundtrip, &calendar_time, sizeof calendar_time) == 0 && "roundtrip must be correct");
    #endif // !NDEBUG

    return epoch_time;
}

//=========================================
// Filesystem
//=========================================

#define IO_ALIGN 8
#define IO_LOCAL_BUFFER_SIZE 1024
#define IO_NORMALIZE_LINUX 0
#define IO_NORMALIZE_DIRECTORY 0
#define IO_NORMALIZE_FILE 0

typedef struct _Buffer
{
    void* data;
    int32_t item_size;
    int32_t is_alloced;
    int64_t size;
    int64_t capacity;
} _Buffer;

static wchar_t* _wstring(_Buffer stack);
static char*    _string(_Buffer stack);
static _Buffer  _buffer_init_backed(void* backing, int64_t backing_size, int32_t item_size);
static _Buffer  _buffer_init(int64_t item_size);
static void*    _buffer_resize(_Buffer* stack, int64_t new_size);
static int64_t  _buffer_push(_Buffer* stack, const void* item, int64_t item_size);
static void*    _buffer_at(_Buffer* stack, int64_t index);
static void     _buffer_deinit(_Buffer* stack);

static int64_t _strlen(const char* str)
{
    if(str == NULL)
        return 0;
    else
        return (int64_t) strlen(str);
}

static void _utf16_to_utf8(const wchar_t* utf16, int64_t utf16len, _Buffer* output) 
{
    if (utf16 == NULL || utf16len == 0) 
    {
        _buffer_resize(output, 0);
        return;
    }

    int utf8len = WideCharToMultiByte(CP_UTF8, 0, utf16, (int) utf16len, NULL, 0, NULL, NULL);
    _buffer_resize(output, utf8len);
    WideCharToMultiByte(CP_UTF8, 0, utf16, (int) utf16len, _string(*output), (int) utf8len, 0, 0);
}
    
static void _utf8_to_utf16(const char* utf8, int64_t utf8len, _Buffer* output) 
{
    if (utf8 == NULL || utf8len == 0) 
    {
        _buffer_resize(output, 0);
        return;
    }

    int utf16len = MultiByteToWideChar(CP_UTF8, 0, utf8, (int) utf8len, NULL, 0);
    _buffer_resize(output, utf16len);
    MultiByteToWideChar(CP_UTF8, 0, utf8, (int) utf8len, _wstring(*output), (int) utf16len);
}

static wchar_t* _wstring(_Buffer stack)
{
    assert(stack.item_size == sizeof(wchar_t));
    return (wchar_t*) stack.data;
}

static char* _string(_Buffer stack)
{
    assert(stack.item_size == sizeof(char));
    return (char*) stack.data;
}

static _Buffer _buffer_init_backed(void* backing, int64_t backing_size, int32_t item_size)
{
    _Buffer out = {0};
    out.data = backing;
    out.item_size = item_size;
    out.is_alloced = false;
    out.capacity = backing_size/item_size;
    memset(backing, 0, backing_size);

    return out;
}

static _Buffer _buffer_init(int64_t item_size)
{
    _Buffer out = {0};
    out.item_size = (int32_t) item_size;
    return out;
}

static void* _buffer_resize(_Buffer* stack, int64_t new_size)
{
    int64_t i_size = stack->item_size;
    assert(i_size != 0);
    assert(stack->size <= stack->capacity);
    assert(stack->item_size != 0);

    if(new_size >= stack->capacity)
    {
        int64_t new_capaity = 8;
        while(new_capaity < new_size + 1)
            new_capaity *= 2;

        void* prev_ptr = NULL;
        if(stack->is_alloced)
            prev_ptr = stack->data;
        stack->data = platform_heap_reallocate((size_t) (new_capaity * i_size), prev_ptr, stack->capacity, IO_ALIGN);
        stack->is_alloced = true;

        //null newly added portion
        memset((char*) stack->data + stack->capacity*i_size, 0, (new_capaity - stack->capacity)*i_size);
        stack->capacity = new_capaity;
    }

    //Null terminates the buffer
    stack->size = new_size;
    memset((char*) stack->data + new_size*i_size, 0, i_size);
    return stack->data;
}

static int64_t _buffer_push(_Buffer* stack, const void* item, int64_t item_size)
{
    if(stack->item_size == 0)
        stack->item_size = (int32_t) item_size;

    assert(stack->item_size == item_size);
    _buffer_resize(stack, stack->size + 1);

    memmove((char*) stack->data + (stack->size - 1)*item_size, item, item_size);
    return stack->size;
}

static void* _buffer_at(_Buffer* stack, int64_t index)
{
    assert(0 <= index && index < stack->size);
    return (char*) stack->data + index*stack->item_size;
}

static void _buffer_deinit(_Buffer* stack)
{
    if(stack->is_alloced)
        (void) platform_heap_reallocate(0, stack->data, stack->capacity, IO_ALIGN);
    
    _Buffer empty = {0};
    *stack = empty;
}

static void _w_concat(const wchar_t* a, const wchar_t* b, const wchar_t* c, _Buffer* output)
{
    int64_t a_size = a != NULL ? (int64_t) wcslen(a) : 0;
    int64_t b_size = b != NULL ? (int64_t) wcslen(b) : 0;
    int64_t c_size = c != NULL ? (int64_t) wcslen(c) : 0;
    int64_t composite_size = a_size + b_size + c_size;
        
    _buffer_resize(output, composite_size);
    memmove(_wstring(*output),                   a, sizeof(wchar_t) * a_size);
    memmove(_wstring(*output) + a_size,          b, sizeof(wchar_t) * b_size);
    memmove(_wstring(*output) + a_size + b_size, c, sizeof(wchar_t) * c_size);
}

_Buffer _normalize_convert_to_utf16_path(const char* path, int64_t path_size, int flags, char* buffer, int64_t buffer_size)
{
    _Buffer output = _buffer_init_backed(buffer, buffer_size, sizeof(wchar_t));
    _utf8_to_utf16(path, path_size, &output);
    return output;
}


_Buffer _convert_to_utf8_normalize_path(const wchar_t* path, int64_t path_size, int flags, char* buffer, int64_t buffer_size)
{
    _Buffer output = _buffer_init_backed(buffer, buffer_size, sizeof(char));
    _utf16_to_utf8(path, path_size, &output);
    return output;
}

bool platform_file_move(const char* new_path, const char* old_path)
{       
    int64_t new_path_size = _strlen(new_path);
    int64_t old_path_size = _strlen(old_path);
    char new_path_buffer[IO_LOCAL_BUFFER_SIZE] = {0}; 
    char old_path_buffer[IO_LOCAL_BUFFER_SIZE] = {0}; 
    _Buffer new_path_norm = _normalize_convert_to_utf16_path(new_path, new_path_size, 0, new_path_buffer, IO_LOCAL_BUFFER_SIZE);
    _Buffer old_path_norm = _normalize_convert_to_utf16_path(old_path, old_path_size, 0, old_path_buffer, IO_LOCAL_BUFFER_SIZE);
        
    bool state = !!MoveFileExW(_wstring(old_path_norm), _wstring(new_path_norm), MOVEFILE_COPY_ALLOWED);

    _buffer_deinit(&new_path_norm);
    _buffer_deinit(&old_path_norm);
    return state;
}

bool platform_file_copy(const char* copy_to_path, const char* copy_from_path)
{
    int64_t to_path_size = _strlen(copy_to_path);
    int64_t from_path_size = _strlen(copy_from_path);
    char to_path_buffer[IO_LOCAL_BUFFER_SIZE] = {0}; 
    char from_path_buffer[IO_LOCAL_BUFFER_SIZE] = {0}; 
    _Buffer to_path_norm = _normalize_convert_to_utf16_path(copy_to_path, to_path_size, 0, to_path_buffer, IO_LOCAL_BUFFER_SIZE);
    _Buffer from_path_norm = _normalize_convert_to_utf16_path(copy_from_path, from_path_size, 0, from_path_buffer, IO_LOCAL_BUFFER_SIZE);
        
    bool state = !!CopyFileW(_wstring(from_path_norm), _wstring(to_path_norm), true);

    _buffer_deinit(&to_path_norm);
    _buffer_deinit(&from_path_norm);
    return state;
}


static bool _is_file_link(const wchar_t* directory_path)
{
    HANDLE file = CreateFileW(directory_path, 0, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                              NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    size_t requiredSize = GetFinalPathNameByHandleW(file, NULL, 0, FILE_NAME_NORMALIZED);
    CloseHandle(file);

    return requiredSize == 0;
}

bool platform_file_info(const char* file_path, Platform_File_Info* info)
{    
    int64_t path_size = _strlen(file_path);
    #define IO_NORMALIZE_PATH_OP(path, path_size, execute)  \
        char normalized_path_buffer[IO_LOCAL_BUFFER_SIZE] = {0}; \
        _Buffer normalized_path = _normalize_convert_to_utf16_path(path, path_size, 0, normalized_path_buffer, IO_LOCAL_BUFFER_SIZE); \
        \
        execute\
        \
        _buffer_deinit(&normalized_path); \

    WIN32_FILE_ATTRIBUTE_DATA native_info;
    memset(&native_info, 0, sizeof(native_info)); 
    memset(info, 0, sizeof(*info)); 
    IO_NORMALIZE_PATH_OP(file_path, path_size, 
        bool state = !!GetFileAttributesExW(_wstring(normalized_path), GetFileExInfoStandard, &native_info);
        
        if(native_info.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT)
            info->is_link = _is_file_link(_wstring(normalized_path));
    )   
    if(!state)
        return false;
            
    info->created_time = _filetime_to_time_t(native_info.ftCreationTime);
    info->last_access_time = _filetime_to_time_t(native_info.ftLastAccessTime);
    info->last_write_time = _filetime_to_time_t(native_info.ftLastWriteTime);
    info->size = ((int64_t) native_info.nFileSizeHigh << 32) | ((int64_t) native_info.nFileSizeLow);
        
    if(native_info.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
        info->type = PLATFORM_FILE_TYPE_DIRECTORY;
    else
        info->type = PLATFORM_FILE_TYPE_FILE;

    return state;
}
    
//@TODO: create a cyclically overriden buffers that will get used for utf8 conversions
// this ensures that they will not ever have to get freed while periodally freeing them
//We have lets say 4 allocations slots. We allocate into the first then the second then the third and so on.
// once we run out of slots we allocate only if we need to grow. If we use a slot some number of times (2x lets say)
// we reallocate it to ensure that the space shrinks once not needed.

bool platform_directory_create(const char* dir_path)
{
    int64_t path_size = _strlen(dir_path);
    IO_NORMALIZE_PATH_OP(dir_path, path_size, 
        bool state = !!CreateDirectoryW(_wstring(normalized_path), NULL);
    )
    return state;
}
    
bool platform_directory_remove(const char* dir_path)
{
    int64_t path_size = _strlen(dir_path);
    IO_NORMALIZE_PATH_OP(dir_path, path_size, 
        bool state = !!RemoveDirectoryW(_wstring(normalized_path));
    )
    return state;
}

static _Buffer _malloc_full_path(const wchar_t* local_path, int flags)
{
    char full_path_buffer[IO_LOCAL_BUFFER_SIZE] = {0}; 
    _Buffer full_path = _buffer_init_backed(full_path_buffer, IO_LOCAL_BUFFER_SIZE, sizeof(wchar_t));

    int64_t needed_size = GetFullPathNameW(local_path, 0, NULL, NULL);
    if(needed_size > full_path.size)
    {
        _buffer_resize(&full_path, needed_size);
        needed_size = GetFullPathNameW(local_path, (DWORD) full_path.size, _wstring(full_path), NULL);
    }
    _Buffer out_string = _convert_to_utf8_normalize_path(_wstring(full_path), full_path.size, flags, NULL, 0);
        
    _buffer_deinit(&full_path);
    return out_string;
}

typedef struct Directory_Visitor
{
    WIN32_FIND_DATAW current_entry;
    HANDLE first_found;
    bool failed;
} Directory_Visitor;

#define WIO_FILE_MASK_ALL L"\\*.*"

static Directory_Visitor _directory_iterate_init(const wchar_t* dir_path, const wchar_t* file_mask)
{
    char built_path_buffer[IO_LOCAL_BUFFER_SIZE] = {0}; 
    _Buffer built_path = _buffer_init_backed(built_path_buffer, IO_LOCAL_BUFFER_SIZE, sizeof(wchar_t));
    
    _w_concat(dir_path, file_mask, NULL, &built_path);

    Directory_Visitor visitor = {0};
    assert(built_path.data != NULL);
    visitor.first_found = FindFirstFileW(_wstring(built_path), &visitor.current_entry);
    while(visitor.failed == false && visitor.first_found != INVALID_HANDLE_VALUE)
    {
        if(wcscmp(visitor.current_entry.cFileName, L".") == 0
            || wcscmp(visitor.current_entry.cFileName, L"..") == 0)
            visitor.failed = !FindNextFileW(visitor.first_found, &visitor.current_entry);
        else
            break;
    }
    return visitor;
}

static bool _directory_iterate_has(const Directory_Visitor* visitor)
{
    return visitor->first_found != INVALID_HANDLE_VALUE && visitor->failed == false;
}

static void _directory_iterate_next(Directory_Visitor* visitor)
{
    visitor->failed = visitor->failed || !FindNextFileW(visitor->first_found, &visitor->current_entry);
}

static void _directory_iterate_deinit(Directory_Visitor* visitor)
{
    FindClose(visitor->first_found);
}

static bool _directory_list_contents_malloc(const wchar_t* directory_path, _Buffer* entries, int64_t max_depth)
{
    typedef struct Dir_Context
    {
        Directory_Visitor visitor;
        const wchar_t* path;    
        int64_t depth;          
        int64_t index;          
    } Dir_Context;

    Dir_Context first = {0};

    
    first.visitor = _directory_iterate_init(directory_path, WIO_FILE_MASK_ALL);
    first.path = directory_path;
    if(_directory_iterate_has(&first.visitor) == false)
        return false;
        
    char stack_buffer[IO_LOCAL_BUFFER_SIZE] = {0}; 
    char built_path_buffer[IO_LOCAL_BUFFER_SIZE] = {0}; 
    
    _Buffer stack = _buffer_init_backed(stack_buffer, IO_LOCAL_BUFFER_SIZE, sizeof(Dir_Context));
    _Buffer built_path = _buffer_init_backed(built_path_buffer, IO_LOCAL_BUFFER_SIZE, sizeof(wchar_t));

    _buffer_push(&stack, &first, sizeof(first));

    const int64_t MAX_RECURSION = 10000;
        
    //While the queue is not empty iterate
    for(int64_t reading_from = 0; reading_from < stack.size; reading_from++)
    {
        Dir_Context* dir_context = (Dir_Context*) _buffer_at(&stack, reading_from);
        for(; _directory_iterate_has(&dir_context->visitor); _directory_iterate_next(&dir_context->visitor), dir_context->index++)
        {
            //Build up our file path using the passed in 
            //  [path] and the file/foldername we just found: 
            _w_concat(dir_context->path, L"\\", dir_context->visitor.current_entry.cFileName, &built_path);
            assert(built_path.data != 0);
        
            Platform_File_Info info = {0};
            info.created_time = _filetime_to_time_t(dir_context->visitor.current_entry.ftCreationTime);
            info.last_access_time = _filetime_to_time_t(dir_context->visitor.current_entry.ftLastAccessTime);
            info.last_write_time = _filetime_to_time_t(dir_context->visitor.current_entry.ftLastWriteTime);
            info.size = ((int64_t) dir_context->visitor.current_entry.nFileSizeHigh << 32) | ((int64_t) dir_context->visitor.current_entry.nFileSizeLow);
        
            info.type = PLATFORM_FILE_TYPE_FILE;
            if(dir_context->visitor.current_entry.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
                info.type = PLATFORM_FILE_TYPE_DIRECTORY;
            else
                info.type = PLATFORM_FILE_TYPE_FILE;

            if(info.is_link)
                info.is_link = _is_file_link(_wstring(built_path));  


            int flag = IO_NORMALIZE_LINUX;
            if(info.type == PLATFORM_FILE_TYPE_DIRECTORY)
                flag |= IO_NORMALIZE_DIRECTORY;
            else
                flag |= IO_NORMALIZE_FILE;
            _Buffer out_string = _malloc_full_path(_wstring(built_path), flag);

            Platform_Directory_Entry entry = {0};
            entry.info = info;
            entry.path = _string(out_string);
            entry.path_size = out_string.size;
    
            _buffer_push(entries, &entry, sizeof(entry));

            bool recursion = dir_context->depth + 1 < max_depth || max_depth <= 0;
            if(info.type == PLATFORM_FILE_TYPE_DIRECTORY && info.is_link == false && recursion)
            {
                _Buffer next_path = _buffer_init(sizeof(wchar_t));
                _w_concat(_wstring(built_path), NULL, NULL, &next_path);

                Dir_Context next = {0};
                next.visitor = _directory_iterate_init(_wstring(next_path), WIO_FILE_MASK_ALL);
                next.depth = dir_context->depth + 1;
                next.path = _wstring(next_path);
                assert(next.depth < MAX_RECURSION && "must not get stuck in an infinite loop");
                _buffer_push(&stack, &next, sizeof(next));
            }
        }
    }

    for(int64_t i = 0; i < stack.size; i++)
    {
        Dir_Context* dir_context = (Dir_Context*) _buffer_at(&stack, i);
        if(dir_context->path != directory_path)
            platform_heap_reallocate(0, (void*) dir_context->path, 0, IO_ALIGN);
        _directory_iterate_deinit(&dir_context->visitor);
    }
    
    //Null terminate the entries
    Platform_Directory_Entry terminator = {0};
    _buffer_push(entries, &terminator, sizeof(terminator));

    _buffer_deinit(&stack);
    _buffer_deinit(&built_path);
    return true;
}

bool platform_directory_list_contents_malloc(const char* directory_path, Platform_Directory_Entry** entries, int64_t* entries_count, int64_t max_depth)
{
    int64_t path_size = _strlen(directory_path);
    assert(entries != NULL && entries_count != NULL);
    _Buffer entries_stack = _buffer_init(sizeof(Platform_Directory_Entry));
    IO_NORMALIZE_PATH_OP(directory_path, path_size,
        bool ok = _directory_list_contents_malloc(_wstring(normalized_path), &entries_stack, max_depth);
    )

    *entries = (Platform_Directory_Entry*) entries_stack.data;
    *entries_count = entries_stack.size;
    return ok;
}

//Frees previously allocated file list
void platform_directory_list_contents_free(Platform_Directory_Entry* entries)
{
    if(entries == NULL)
        return;

    for(int64_t i = 0; entries[i].path != NULL; i++)
        platform_heap_reallocate(0, entries[i].path, 0, IO_ALIGN);
            
    platform_heap_reallocate(0, entries, 0, IO_ALIGN);
}

bool platform_directory_set_current_working(const char* new_working_dir)
{
    int64_t path_size = _strlen(new_working_dir);
    IO_NORMALIZE_PATH_OP(new_working_dir, path_size,
        bool state = _wchdir(_wstring(normalized_path)) == 0;
    )
    return state;
}
    
char* platform_directory_get_current_working_malloc()
{
    //@TODO: use the local malloc
    wchar_t* current_working = _wgetcwd(NULL, 0);
    if(current_working == NULL)
        abort();

    _Buffer output = _convert_to_utf8_normalize_path(current_working, (int64_t) wcslen(current_working), IO_NORMALIZE_LINUX | IO_NORMALIZE_DIRECTORY, NULL, 0);
    free(current_working);
    return _string(output);
}

//=========================================
// Window managmenet
//=========================================
typedef struct Window {
    
    //Window stuff
    HWND handle;
    WNDCLASSEX class_;
    RECT window_rect;  
    RECT client_rect;  
    Platform_Window_Options options;
    Platform_Window_Visibility visibility;
    RECT prev_window_rect;
    bool was_shown;
    const char* last_errors;

    //Opengl stuff
    HDC device_context;
    int pixel_format;
    HGLRC rendering_context;

    //input
    Platform_Input input;
} Window;

LRESULT CALLBACK window_proc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

typedef struct Platform_Error_Msg
{
    bool ok;
    const char* msg;
} Platform_Error_Msg;

Platform_Error_Msg _platform_window_opengl_init(Window* window);
void _platform_window_opengl_deinit(Window* window);


Window* platform_window_init(const char* window_title)
{
    HINSTANCE hInstance = GetModuleHandleW(NULL);
    const wchar_t WINDOW_CLASS_NAME[] = L"jot_window_class";
    static bool        global_window_class_registered = false;
    static WNDCLASSEX  global_window_class = {0};

    if(!global_window_class_registered)
    {
        global_window_class.cbSize        = sizeof(WNDCLASSEX);
        global_window_class.style         = CS_HREDRAW | CS_VREDRAW | CS_OWNDC; // Redraw On Size, And Own DC For Window.
        global_window_class.lpfnWndProc   = window_proc;
        global_window_class.cbClsExtra    = 0;
        global_window_class.cbWndExtra    = 0;
        global_window_class.hInstance     = hInstance;
        global_window_class.hIcon         = LoadIconW(NULL, IDI_APPLICATION);
        global_window_class.hCursor       = LoadCursorW(NULL, IDC_ARROW);
        global_window_class.hbrBackground = (HBRUSH)(COLOR_WINDOW+1); //???
        global_window_class.lpszMenuName  = NULL;
        global_window_class.lpszClassName = WINDOW_CLASS_NAME;
        global_window_class.hIconSm       = LoadIconW(NULL, IDI_APPLICATION);
        if(!RegisterClassExW(&global_window_class))
            return NULL;

        global_window_class_registered = true;
    }

    Window* window = calloc(2, sizeof(Window));
    if(window == NULL)
        return NULL;

    char window_title_buffer[IO_LOCAL_BUFFER_SIZE] = {0}; 
    _Buffer wide_title = _buffer_init_backed(window_title_buffer, IO_LOCAL_BUFFER_SIZE, sizeof(wchar_t));
    _utf8_to_utf16(window_title, _strlen(window_title), &wide_title);
        
    //wprintf(_wstring(wide_title));
    window->handle = CreateWindowExW(
        WS_EX_CLIENTEDGE,                                           // style. Should we use: WS_EX_APPWINDOW | WS_EX_WINDOWEDGE???
        WINDOW_CLASS_NAME,                                          // class
        _wstring(wide_title),                                       // title
        WS_OVERLAPPEDWINDOW,                                        // style. Should we use: WS_POPUP???
        CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, // default rect
        NULL,                                                       // no parent window
        NULL,                                                       // no menu
        hInstance,                                                  // instance
        window);                                                    // save poinnter to our window structure so we can query it from within the winproc

    _buffer_deinit(&wide_title);

    RAWINPUTDEVICE raw_input_device = {0};
    raw_input_device.usUsagePage = HID_USAGE_PAGE_GENERIC; 
    raw_input_device.usUsage = HID_USAGE_GENERIC_MOUSE; 
    raw_input_device.dwFlags = RIDEV_INPUTSINK;   
    raw_input_device.hwndTarget = window->handle;
    RegisterRawInputDevices(&raw_input_device, 1, sizeof(raw_input_device));

    window->class_ = global_window_class;

    //====================== Opengl stuff ========================
    Platform_Error_Msg msg = _platform_window_opengl_init(window);
    if(msg.ok == false)
    {
        platform_window_deinit(window);
        return NULL;
    }

    return window;
}

void platform_window_deinit(Window* window)
{
    if(window->handle != NULL);
        DestroyWindow(window->handle);

    _platform_window_opengl_deinit(window);
    free(window);
}

Platform_Input platform_window_process_messages(Window* window) {
    assert(window != NULL); 

    MSG message = {0};
    while (PeekMessageW(&message, window->handle, 0, 0, PM_REMOVE)) 
    {
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }

    Platform_Input out = window->input;
    POINT cursor = {0};
    (void) GetCursorPos(&cursor);

    out.client_position_x = window->client_rect.left;
    out.client_position_y = window->client_rect.top;
    
    out.client_size_x = window->client_rect.right - window->client_rect.left;
    out.client_size_y = window->client_rect.bottom - window->client_rect.top;

    assert(out.client_size_x >= 0);
    assert(out.client_size_y >= 0);
    
    out.window_position_x = window->window_rect.left;
    out.window_position_y = window->window_rect.top;
    
    out.window_size_x = window->window_rect.right - window->window_rect.left;
    out.window_size_y = window->window_rect.bottom - window->window_rect.top;

    assert(out.window_size_x >= 0);
    assert(out.window_size_y >= 0);

    out.mouse_screen_x = cursor.x;
    out.mouse_screen_y = cursor.y;
    
    Platform_Input null = {0};
    window->input = null;
    return out;
}

static uint8_t _offset_half_trans_count(uint8_t current, int8_t offset, bool is_down)
{
    uint8_t down_bit = 1 << 7;
    uint8_t masked = current & ~down_bit;
                
    //Increment the half transition counter but cap it so it doesnt wrap
    int16_t upcast = masked + offset;
    if(upcast < 0)
        upcast = 0;

    if(upcast > down_bit - 1)
        upcast = down_bit - 1;

    masked = (uint8_t) upcast;
    //set the down_bit 
    if(is_down)
        masked = masked | down_bit;
    else
        masked = masked & ~down_bit;

    return masked;
}

LRESULT CALLBACK window_proc(HWND hwnd, UINT msg, WPARAM w_param, LPARAM l_param) 
{
    if (msg == WM_NCCREATE) 
    {
        CREATESTRUCTW* create_struct = (CREATESTRUCTW*) l_param;
        void* user_data = create_struct->lpCreateParams;
        // store window instance pointer in window user data
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, (LONG_PTR) user_data);
    }
    Window* window = (Window*) GetWindowLongPtrW(hwnd, GWLP_USERDATA);
    if (!window) 
        return DefWindowProcW(hwnd, msg, w_param, l_param);

    switch (msg) 
    {
        case WM_NCCALCSIZE: 
        {
            if (w_param == TRUE && window->options.no_border) {
                NCCALCSIZE_PARAMS* params = (NCCALCSIZE_PARAMS*) l_param;
                RECT* rect = &params->rgrc[0];
                WINDOWPLACEMENT placement = {0};
                if (!GetWindowPlacement(hwnd, &placement))
                    break;

                bool is_maximized = placement.showCmd == SW_MAXIMIZE;
                HMONITOR monitor = MonitorFromWindow(hwnd, MONITOR_DEFAULTTONULL);
                if (!monitor) 
                    break;

                MONITORINFO monitor_info = {0};
                monitor_info.cbSize = sizeof(monitor_info);
                if (!GetMonitorInfoW(monitor, &monitor_info)) 
                    break;

                *rect = monitor_info.rcWork;
                return 0;
            }
            break;
        }

        case WM_NCHITTEST: {
            // When we have no border or title bar, we need to perform our
            // own hit testing to allow resizing and moving.
            if (!window->options.no_border) 
                break;

            POINT mouse = {0};
            mouse.x = GET_X_LPARAM(l_param);
            mouse.y = GET_Y_LPARAM(l_param);

            // identify borders and corners to allow resizing the window.
            // Note: On Windows 10, windows behave differently and
            // allow resizing outside the visible window frame.
            // This implementation does not replicate that behavior.
            POINT border = {0};
            border.x = GetSystemMetrics(SM_CXFRAME) + GetSystemMetrics(SM_CXPADDEDBORDER);
            border.y = GetSystemMetrics(SM_CYFRAME) + GetSystemMetrics(SM_CXPADDEDBORDER);

            LRESULT drag = window->options.no_border_allow_move ? HTCAPTION : HTCLIENT;
            RECT window_rect = {0};
            if(!GetWindowRect(window->handle, &window_rect))
                return HTNOWHERE;

            enum region_mask {
                client = 0,
                left   = 1,
                right  = 2,
                top    = 4,
                bottom = 8,
            };

            int result =
                left    * (mouse.x <  (window_rect.left   + border.x)) |
                right   * (mouse.x >= (window_rect.right  - border.x)) |
                top     * (mouse.y <  (window_rect.top    + border.y)) |
                bottom  * (mouse.y >= (window_rect.bottom - border.y));

            if(result != 0)
            {
                int i = 4;
            }
            bool allow_resize = window->options.no_border_allow_resize;
            switch (result) {
                case left          : return allow_resize ? HTLEFT        : drag;
                case right         : return allow_resize ? HTRIGHT       : drag;
                case top           : return allow_resize ? HTTOP         : drag;
                case bottom        : return allow_resize ? HTBOTTOM      : drag;
                case top | left    : return allow_resize ? HTTOPLEFT     : drag;
                case top | right   : return allow_resize ? HTTOPRIGHT    : drag;
                case bottom | left : return allow_resize ? HTBOTTOMLEFT  : drag;
                case bottom | right: return allow_resize ? HTBOTTOMRIGHT : drag;
                case client        : return drag;
                default            : return HTNOWHERE;
            }
        }

        case WM_ERASEBKGND:
            // Notify the OS that erasing will be handled by the application to prevent flicker.
            return 1;
        case WM_CLOSE: {
            window->input.should_quit = true;
            return 0;
        }
        case WM_DESTROY: {
            PostQuitMessage(0);
            return 0;
        }
            
        case WM_SIZE: {
            RECT window_rect = {0};
            RECT client_rect = {0};

            //bool ok1 = GetWindowRect(hwnd, &window_rect);
            bool ok1 = DwmGetWindowAttribute(hwnd, DWMWA_EXTENDED_FRAME_BOUNDS, &window_rect, sizeof window_rect) == S_OK;
            bool ok2 = GetClientRect(hwnd, &client_rect);

            POINT top_left = {0, 0};
            bool ok3 = ClientToScreen(hwnd, &top_left);
            assert(ok1 && ok2 && ok3);

            client_rect.left += top_left.x;
            client_rect.right += top_left.x;
            client_rect.top += top_left.y;
            client_rect.bottom += top_left.y;

            window->window_rect = window_rect;
            window->client_rect = client_rect;
        } break;

        case WM_KEYDOWN:
        case WM_SYSKEYDOWN:
        case WM_KEYUP:
        case WM_SYSKEYUP: 
        {
            // Key pressed/released
            bool is_down = (msg == WM_KEYDOWN || msg == WM_SYSKEYDOWN);
            uint32_t key = (uint32_t)w_param;

            // Check for extended scan code.
            bool is_extended = (HIWORD(l_param) & KF_EXTENDED) == KF_EXTENDED;

            // Keypress only determines if _any_ alt/ctrl/shift key is pressed. Determine which one if so.
            if (w_param == VK_MENU) 
                key = is_extended ? PLATFORM_KEY_RALT : PLATFORM_KEY_LALT;
            else if (w_param == VK_CONTROL) 
                key = is_extended ? PLATFORM_KEY_RCONTROL : PLATFORM_KEY_LCONTROL;
            else if (w_param == VK_SHIFT) 
            {
                // Annoyingly, KF_EXTENDED is not set for shift keys.
                uint32_t left_shift = MapVirtualKeyW(VK_LSHIFT, MAPVK_VK_TO_VSC);
                uint32_t scancode = ((l_param & (0xFF << 16)) >> 16);

                key = scancode == left_shift ? PLATFORM_KEY_LSHIFT : PLATFORM_KEY_RSHIFT;
            }
            
            assert(key < PLATFORM_KEY_ENUM_COUNT);
            
            uint8_t* key_ptr = &window->input.key_half_transitions[key];
            *key_ptr = _offset_half_trans_count(*key_ptr, 1, is_down);

            // Return 0 to prevent default window behaviour for some keypresses, such as alt.
            return 0;
        }
        
        case WM_INPUT: 
        {
            UINT dwSize = sizeof(RAWINPUT);
            uint8_t lpb[sizeof(RAWINPUT)] = {0};

            GetRawInputData((HRAWINPUT) l_param, RID_INPUT, lpb, &dwSize, sizeof(RAWINPUTHEADER));

            RAWINPUT* raw = (RAWINPUT*)lpb;

            if (raw->header.dwType == RIM_TYPEMOUSE) 
            {
                window->input.mouse_native_delta_x += raw->data.mouse.lLastX;
                window->input.mouse_native_delta_y += raw->data.mouse.lLastY;
            } 
            break;
        }

        case WM_MOUSEWHEEL: {
            int32_t z_delta = GET_WHEEL_DELTA_WPARAM(w_param);
            window->input.mouse_wheel_delta += z_delta;
            break;
        } 

        case WM_LBUTTONDOWN:
        case WM_MBUTTONDOWN:
        case WM_RBUTTONDOWN:
        case WM_XBUTTONDOWN:
        case WM_LBUTTONUP:
        case WM_MBUTTONUP:
        case WM_RBUTTONUP: 
        case WM_XBUTTONUP: {
            Platform_Mouse_Button mouse_button = PLATFORM_MOUSE_BUTTON_ENUM_COUNT;
            bool is_down = false;
            int return_ = 0;

            switch (msg) 
            {
                case WM_LBUTTONDOWN: 
                    is_down = true;
                case WM_LBUTTONUP:
                    mouse_button = PLATFORM_MOUSE_BUTTON_LEFT;
                    break;
                
                case WM_MBUTTONDOWN:
                    is_down = true;
                case WM_MBUTTONUP:
                    mouse_button = PLATFORM_MOUSE_BUTTON_MIDDLE;
                    break;
                
                case WM_RBUTTONDOWN:
                    is_down = true;
                case WM_RBUTTONUP:
                    mouse_button = PLATFORM_MOUSE_BUTTON_RIGHT;
                    break;
                    
                case WM_XBUTTONDOWN:
                    is_down = true;
                case WM_XBUTTONUP:
                    if (GET_XBUTTON_WPARAM(w_param) == XBUTTON1)
                        mouse_button = PLATFORM_MOUSE_BUTTON_4;
                    else
                        mouse_button = PLATFORM_MOUSE_BUTTON_5;
                    return_ = TRUE;
                    break;
            }

            if(mouse_button != PLATFORM_MOUSE_BUTTON_ENUM_COUNT)
            {
                uint8_t* button = &window->input.mouse_button_half_transitions[mouse_button];
                *button = _offset_half_trans_count(*button, 1, is_down);
            }

            return return_;
        }
    }

    return DefWindowProcW(hwnd, msg, w_param, l_param);
}

//Source: https://stackoverflow.com/a/2416613
static bool _enter_fullscreen(HWND hwnd, int fullscreen_width, int fullscreen_height, int colour_bits, int refresh_rate) 
{
    DEVMODE fullscreenSettings = {0};
    bool isChangeSuccessful = true;
    RECT windowBoundary = {0};

    EnumDisplaySettingsW(NULL, 0, &fullscreenSettings);
    fullscreenSettings.dmPelsWidth        = fullscreen_width;
    fullscreenSettings.dmPelsHeight       = fullscreen_height;
    fullscreenSettings.dmBitsPerPel       = colour_bits;
    fullscreenSettings.dmDisplayFrequency = refresh_rate;
    fullscreenSettings.dmFields           = DM_PELSWIDTH |
                                            DM_PELSHEIGHT |
                                            DM_BITSPERPEL |
                                            DM_DISPLAYFREQUENCY;

    SetWindowLongPtrW(hwnd, GWL_EXSTYLE, WS_EX_APPWINDOW | WS_EX_TOPMOST);
    SetWindowLongPtrW(hwnd, GWL_STYLE, WS_POPUP | WS_VISIBLE);
    SetWindowPos(hwnd, HWND_TOPMOST, 0, 0, fullscreen_width, fullscreen_height, SWP_SHOWWINDOW);
    isChangeSuccessful = ChangeDisplaySettings(&fullscreenSettings, CDS_FULLSCREEN) == DISP_CHANGE_SUCCESSFUL;
    ShowWindow(hwnd, SW_MAXIMIZE);

    return isChangeSuccessful;
}

//Source: https://stackoverflow.com/a/2416613
static bool _exit_fullscreen(HWND hwnd, int window_x, int window_y, int windowed_width, int windowed_height, int windowed_padding_x, int windowed_padding_y) 
{
    bool isChangeSuccessful;

    SetWindowLongPtrW(hwnd, GWL_EXSTYLE, WS_EX_LEFT);
    SetWindowLongPtrW(hwnd, GWL_STYLE, WS_OVERLAPPEDWINDOW | WS_VISIBLE);
    isChangeSuccessful = ChangeDisplaySettings(NULL, CDS_RESET) == DISP_CHANGE_SUCCESSFUL;
    SetWindowPos(hwnd, HWND_NOTOPMOST, window_x, window_y, windowed_width + windowed_padding_x, windowed_height + windowed_padding_y, SWP_SHOWWINDOW);
    ShowWindow(hwnd, SW_RESTORE);

    return isChangeSuccessful;
}

static bool _is_fullscreen(Window* window)
{   
    if(window == NULL)
        return false;
    DWORD style = GetWindowLongW(window->handle, GWL_EXSTYLE);
    if (style & WS_EX_TOPMOST)
        return true;
    else
        return false;
}

static bool _set_fullscreen(Window* window, bool fullscreen)
{
    bool current = _is_fullscreen(window);
    if(current == fullscreen)
        return true;

    if (fullscreen) 
    {
        HDC window_HDC = GetDC(window->handle);
        int fullscreen_width  = GetDeviceCaps(window_HDC, DESKTOPHORZRES);
        int fullscreen_height = GetDeviceCaps(window_HDC, DESKTOPVERTRES);
        int colour_bits       = GetDeviceCaps(window_HDC, BITSPIXEL);
        int refresh_rate      = GetDeviceCaps(window_HDC, VREFRESH);

        if(!GetWindowRect(window->handle, &window->prev_window_rect))
            return false;
            
        window->options.visibility = WINDOW_VISIBILITY_FULLSCREEN;
        return _enter_fullscreen(window->handle, fullscreen_width, fullscreen_height, colour_bits, refresh_rate);
    }
    else
    {
        RECT prev = window->prev_window_rect;
        return _exit_fullscreen(window->handle, prev.left, prev.top, prev.right - prev.left, prev.bottom - prev.top, 0, 0);
    }

}

enum {
    WINDOWED   = WS_OVERLAPPEDWINDOW | WS_THICKFRAME | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX | WS_MAXIMIZEBOX,
    NO_BORDER  = WS_POPUP            | WS_THICKFRAME | WS_CAPTION | WS_SYSMENU | WS_MAXIMIZEBOX | WS_MINIMIZEBOX,
};

static bool _set_no_border(Window* window, bool enabled) 
{
    LONG combined = WINDOWED | NO_BORDER;
    LONG old_style = (LONG) GetWindowLongPtrW(window->handle, GWL_STYLE);
    LONG new_style = old_style & ~combined;
    if(enabled)
    {
        new_style |= NO_BORDER;
        window->options.no_border = true;
    }
    else
    {
        new_style |= WINDOWED;
        window->options.no_border = false;
    }

    bool state = SetWindowLongPtrW(window->handle, GWL_STYLE, new_style);
    state = state && SetWindowPos(window->handle, NULL, 0, 0, 0, 0, SWP_FRAMECHANGED | SWP_NOMOVE | SWP_NOSIZE);
    return state;
}

bool platform_window_set_options(Window* window, Platform_Window_Options options)
{
    if(window == NULL || window->handle == NULL)
        return false;

    //first showing should have SW_SHOWNORMAL
    if(window->was_shown == false)
    {
        ShowWindow(window->handle, SW_SHOWNORMAL);
        window->was_shown = true;
    }

    int show = SW_SHOWNORMAL;
    switch(options.visibility)
    {
        case WINDOW_VISIBILITY_MAXIMIZED: show = SW_SHOWMAXIMIZED; break;
        case WINDOW_VISIBILITY_WINDOWED: show = SW_SHOWNORMAL; break;
        case WINDOW_VISIBILITY_MINIMIZED: show = SW_SHOWMINIMIZED; break;
        case WINDOW_VISIBILITY_HIDDEN: show = SW_HIDE; break;
    }
    
    bool state = true;
    if(options.title)
    {
        char window_title_buffer[IO_LOCAL_BUFFER_SIZE] = {0}; 
        _Buffer wide_title = _buffer_init_backed(window_title_buffer, sizeof(window_title_buffer), sizeof(wchar_t));
        _utf8_to_utf16(options.title, _strlen(options.title), &wide_title);
        
        state = SetWindowTextW(window->handle, _wstring(wide_title));
            
        _buffer_deinit(&wide_title);
    }

    window->options.no_border = options.no_border;
    window->options.no_border_allow_resize = options.no_border_allow_resize;
    window->options.no_border_allow_move = options.no_border_allow_move;
    window->options.visibility = options.visibility;

    state = _set_no_border(window, options.no_border) && state;
    state = _set_fullscreen(window, options.visibility == WINDOW_VISIBILITY_FULLSCREEN) && state;

    state = ShowWindow(window->handle, show) && state;
    HWND active_window = GetActiveWindow();
    bool is_active = active_window == window->handle;
    if(!is_active != options.no_focus && options.visibility != WINDOW_VISIBILITY_HIDDEN)
    {
        if(options.no_focus) 
            state = ShowWindow(window->handle, SW_SHOWNA) && state;
        else
            state = ShowWindow(window->handle, SW_SHOW)  && state;
    }

    return state;
}

Platform_Window_Options platform_window_get_options(Window* window)
{
    Platform_Window_Options out = {0};
    if(window == NULL || window->handle == NULL)
        return out;

    out = window->options;
    
    char window_title_buffer[IO_LOCAL_BUFFER_SIZE] = {0}; 
    _Buffer wide_title = _buffer_init_backed(window_title_buffer, sizeof(window_title_buffer), sizeof(wchar_t));
    _buffer_resize(&wide_title, wide_title.capacity-1);
    int needed = GetWindowTextW(window->handle, _wstring(wide_title), (int) wide_title.size);

    _Buffer title = _buffer_init(sizeof(char));
    _utf16_to_utf8(_wstring(wide_title), wide_title.size, &title);
    out.title = title.data;

    _buffer_deinit(&wide_title);

    return out;
}

Platform_Window_Popup_Controls platform_window_make_popup(Platform_Window_Popup_Style desired_style, const char* message, const char* title)
{
    int style = 0;
    int icon = 0;
    switch(desired_style)
    {
        case PLATFORM_POPUP_STYLE_OK:            style = MB_OK; break;
        case PLATFORM_POPUP_STYLE_ERROR:         style = MB_OK; icon = MB_ICONERROR; break;
        case PLATFORM_POPUP_STYLE_WARNING:       style = MB_OK; icon = MB_ICONWARNING; break;
        case PLATFORM_POPUP_STYLE_INFO:          style = MB_OK; icon = MB_ICONINFORMATION; break;
        case PLATFORM_POPUP_STYLE_RETRY_ABORT:   style = MB_ABORTRETRYIGNORE; icon = MB_ICONWARNING; break;
        case PLATFORM_POPUP_STYLE_YES_NO:        style = MB_YESNO; break;
        case PLATFORM_POPUP_STYLE_YES_NO_CANCEL: style = MB_YESNOCANCEL; break;
        default: style = MB_OK; break;
    }
    
    char message_buffer[IO_LOCAL_BUFFER_SIZE] = {0}; 
    char title_buffer[IO_LOCAL_BUFFER_SIZE] = {0}; 
    _Buffer message_wide = _buffer_init_backed(message_buffer, IO_LOCAL_BUFFER_SIZE, sizeof(wchar_t));
    _Buffer title_wide = _buffer_init_backed(title_buffer, IO_LOCAL_BUFFER_SIZE, sizeof(wchar_t));

    _utf8_to_utf16(message, _strlen(message), &message_wide);
    _utf8_to_utf16(title, _strlen(title), &title_wide);

    wchar_t* message_ptr = _wstring(message_wide);
    wchar_t* title_ptr = _wstring(title_wide);

    int value = MessageBoxW(0, message_ptr, title_ptr, style | icon);

    _buffer_deinit(&message_wide);
    _buffer_deinit(&title_wide);

    switch(value)
    {
        case IDABORT: return PLATFORM_POPUP_CONTROL_ABORT;
        case IDCANCEL: return PLATFORM_POPUP_CONTROL_CANCEL;
        case IDCONTINUE: return PLATFORM_POPUP_CONTROL_CONTINUE;
        case IDIGNORE: return PLATFORM_POPUP_CONTROL_IGNORE;
        case IDYES: return PLATFORM_POPUP_CONTROL_YES;
        case IDNO: return PLATFORM_POPUP_CONTROL_NO;
        case IDOK: return PLATFORM_POPUP_CONTROL_OK;
        case IDRETRY: return PLATFORM_POPUP_CONTROL_RETRY;
        case IDTRYAGAIN: return PLATFORM_POPUP_CONTROL_RETRY;
        default: return PLATFORM_POPUP_CONTROL_OK;
    }
}

bool _platform_window_error(Window* window, const char* msg)
{
    window->last_errors = msg;
    return false;
}


void            platform_window_activate_opengl(Window* window)
{
    bool okay = wglMakeCurrent(window->device_context, window->rendering_context);
    //assert(okay);
    //_platform_window_opengl_init(window);
}
void            platform_window_swap_buffers(Window* window)
{
    SwapBuffers(window->device_context);
}

//================================ OPENGL ===========================================================
typedef HGLRC WINAPI wglCreateContextAttribsARB_type(HDC hdc, HGLRC hShareContext, const int *attribList);
wglCreateContextAttribsARB_type *wglCreateContextAttribsARB = NULL;

typedef BOOL WINAPI wglChoosePixelFormatARB_type(HDC hdc, const int *piAttribIList, const FLOAT *pfAttribFList, UINT nMaxFormats, int *piFormats, UINT *nNumFormats);
wglChoosePixelFormatARB_type *wglChoosePixelFormatARB = NULL;

bool wglExtensionsInit = false;

// See https://www.khronos.org/registry/OpenGL/extensions/ARB/WGL_ARB_create_context.txt for all values
#define WGL_CONTEXT_MAJOR_VERSION_ARB             0x2091
#define WGL_CONTEXT_MINOR_VERSION_ARB             0x2092
#define WGL_CONTEXT_PROFILE_MASK_ARB              0x9126

#define WGL_CONTEXT_CORE_PROFILE_BIT_ARB          0x00000001


// See https://www.khronos.org/registry/OpenGL/extensions/ARB/WGL_ARB_pixel_format.txt for all values
#define WGL_DRAW_TO_WINDOW_ARB                    0x2001
#define WGL_ACCELERATION_ARB                      0x2003
#define WGL_SUPPORT_OPENGL_ARB                    0x2010
#define WGL_DOUBLE_BUFFER_ARB                     0x2011
#define WGL_PIXEL_TYPE_ARB                        0x2013
#define WGL_COLOR_BITS_ARB                        0x2014
#define WGL_DEPTH_BITS_ARB                        0x2022
#define WGL_STENCIL_BITS_ARB                      0x2023

#define WGL_FULL_ACCELERATION_ARB                 0x2027
#define WGL_TYPE_RGBA_ARB                         0x202B

static Platform_Error_Msg _error_msg(char *msg)
{
    Platform_Error_Msg error_msg = {false, msg};
    return error_msg;
}

static Platform_Error_Msg _error_msg_ok(char *msg)
{
    Platform_Error_Msg error_msg = {true, msg};
    return error_msg;
}

static Platform_Error_Msg _platform_window_init_opengl_extensions(void)
{
    // Before we can load extensions, we need a dummy OpenGL context, created using a dummy window.
    // We use a dummy window because you can only set the pixel format for a window once. For the
    // real window, we want to use wglChoosePixelFormatARB (so we can potentially specify options
    // that aren't available in PIXELFORMATDESCRIPTOR), but we can't load and use that before we
    // have a context.
    WNDCLASSA window_class = {0};
    window_class.style = CS_HREDRAW | CS_VREDRAW | CS_OWNDC;
    window_class.lpfnWndProc = DefWindowProcA;
    window_class.hInstance = GetModuleHandle(0);
    window_class.lpszClassName = "Dummy_WGL_class";

    if (!RegisterClassA(&window_class)) 
        return _error_msg("Failed to register dummy OpenGL window.");

    HWND dummy_window = CreateWindowExA(
        0,
        window_class.lpszClassName,
        "Dummy OpenGL Window",
        0,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        0,
        0,
        window_class.hInstance,
        0);

    if (!dummy_window) {
        return _error_msg("Failed to create dummy OpenGL window.");
    }

    HDC dummy_dc = GetDC(dummy_window);

    PIXELFORMATDESCRIPTOR pfd = {0};
    pfd.nSize = sizeof(pfd);
    pfd.nVersion = 1;
    pfd.iPixelType = PFD_TYPE_RGBA;
    pfd.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
    pfd.cColorBits = 32;
    pfd.cAlphaBits = 8;
    pfd.iLayerType = PFD_MAIN_PLANE;
    pfd.cDepthBits = 24;
    pfd.cStencilBits = 8;

    int pixel_format = ChoosePixelFormat(dummy_dc, &pfd);
    if (!pixel_format) {
        return _error_msg("Failed to find a suitable pixel format.");
    }
    if (!SetPixelFormat(dummy_dc, pixel_format, &pfd)) {
        return _error_msg("Failed to set the pixel format.");
    }

    HGLRC dummy_context = wglCreateContext(dummy_dc);
    if (!dummy_context) {
        return _error_msg("Failed to create a dummy OpenGL rendering context.");
    }

    if (!wglMakeCurrent(dummy_dc, dummy_context)) {
        return _error_msg("Failed to activate dummy OpenGL rendering context.");
    }

    //assert(!wglMakeCurrent(dummy_dc, dummy_context) && "should fail the other time!");

    wglCreateContextAttribsARB = (wglCreateContextAttribsARB_type*)wglGetProcAddress(
        "wglCreateContextAttribsARB");
    wglChoosePixelFormatARB = (wglChoosePixelFormatARB_type*)wglGetProcAddress(
        "wglChoosePixelFormatARB");

    wglExtensionsInit = true;
    wglMakeCurrent(dummy_dc, 0);
    wglDeleteContext(dummy_context);
    ReleaseDC(dummy_window, dummy_dc);
    if(dummy_window != 0);
        DestroyWindow(dummy_window);

    return _error_msg_ok("OK");
}

#include <strsafe.h>
void ErrorExit(LPTSTR lpszFunction) 
{ 
    // Retrieve the system error message for the last-error code

    LPVOID lpMsgBuf;
    LPVOID lpDisplayBuf;
    DWORD dw = GetLastError(); 

    FormatMessage(
        FORMAT_MESSAGE_ALLOCATE_BUFFER | 
        FORMAT_MESSAGE_FROM_SYSTEM |
        FORMAT_MESSAGE_IGNORE_INSERTS,
        NULL,
        dw,
        MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
        (LPTSTR) &lpMsgBuf,
        0, NULL );

    // Display the error message and exit the process

    lpDisplayBuf = (LPVOID)LocalAlloc(LMEM_ZEROINIT, 
        (lstrlen((LPCTSTR)lpMsgBuf) + lstrlen((LPCTSTR)lpszFunction) + 40) * sizeof(TCHAR)); 
    StringCchPrintf((LPTSTR)lpDisplayBuf, 
        LocalSize(lpDisplayBuf) / sizeof(TCHAR),
        TEXT("%s failed with error %d: %s"), 
        lpszFunction, dw, lpMsgBuf); 
    MessageBox(NULL, (LPCTSTR)lpDisplayBuf, TEXT("Error"), MB_OK); 

    LocalFree(lpMsgBuf);
    LocalFree(lpDisplayBuf);
    ExitProcess(dw); 
}


static Platform_Error_Msg _platform_window_opengl_init(Window* window)
{
    return _error_msg_ok("OK");

    window->device_context = GetDC(window->handle);
    if(!window->device_context)
        return _error_msg("Failed GetDC");

    if(wglExtensionsInit == false)
    {
        Platform_Error_Msg msg = _platform_window_init_opengl_extensions();
        if(msg.ok == false)
            return msg;
    }

    // Now we can choose a pixel format the modern way, using wglChoosePixelFormatARB.
    int pixel_format_attribs[] = {
        WGL_DRAW_TO_WINDOW_ARB,     GL_TRUE,
        WGL_SUPPORT_OPENGL_ARB,     GL_TRUE,
        WGL_DOUBLE_BUFFER_ARB,      GL_TRUE,
        WGL_ACCELERATION_ARB,       WGL_FULL_ACCELERATION_ARB,
        WGL_PIXEL_TYPE_ARB,         WGL_TYPE_RGBA_ARB,
        WGL_COLOR_BITS_ARB,         32,
        WGL_DEPTH_BITS_ARB,         24,
        WGL_STENCIL_BITS_ARB,       8,
        0
    };

    int pixel_format = 0;
    UINT num_formats = 0;
    wglChoosePixelFormatARB(window->device_context, pixel_format_attribs, 0, 1, &pixel_format, &num_formats);
    if (!num_formats) {
        return _error_msg("Failed to set the OpenGL 3.3 pixel format.");
    }

    PIXELFORMATDESCRIPTOR pfd = {0};
    DescribePixelFormat(window->device_context, pixel_format, sizeof(pfd), &pfd);
    if (!SetPixelFormat(window->device_context, pixel_format, &pfd)) {
        return _error_msg("Failed to set the OpenGL 3.3 pixel format.");
    }

    // Specify that we want to create an OpenGL 3.3 core profile context
    int gl33_attribs[] = {
        WGL_CONTEXT_MAJOR_VERSION_ARB, 3,
        WGL_CONTEXT_MINOR_VERSION_ARB, 3,
        WGL_CONTEXT_PROFILE_MASK_ARB,  WGL_CONTEXT_CORE_PROFILE_BIT_ARB,
        0,
    };

    HGLRC gl33_context = wglCreateContextAttribsARB(window->device_context, 0, gl33_attribs);
    if (!gl33_context) {
        return _error_msg("Failed to create OpenGL 3.3 context.");
    }
    
    //Platform_Window_Options show = {0};
    //show.visibility = WINDOW_VISIBILITY_WINDOWED;
    //platform_window_set_options(window, show);
    

    window->rendering_context = gl33_context;
    if (!wglMakeCurrent(window->device_context, gl33_context)) {
        //ErrorExit(L"wglMakeCurrent");
        //return _error_msg("Failed to activate OpenGL 3.3 rendering context.");
    }
    
    //defaults
    glShadeModel(GL_SMOOTH);                            // Enable Smooth Shading
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);               // Black Background
    glClearDepth(1.0f);                                 // Depth Buffer Setup
    glEnable(GL_DEPTH_TEST);                            // Enables Depth Testing
    glDepthFunc(GL_LEQUAL);                             // The Type Of Depth Testing To Do
    glHint(GL_PERSPECTIVE_CORRECTION_HINT, GL_NICEST);  // Really Nice Perspective Calculations

    return _error_msg_ok("OK");
}

void _platform_window_opengl_deinit(Window* window)
{


}