#include "platform.h"


#include <assert.h>
#include <windows.h>
#include <memoryapi.h>
#include <processthreadsapi.h>
#include <winnt.h>
#include <profileapi.h>
#include <hidusage.h>
#include <windowsx.h>
#include <intrin.h>
#include <direct.h>

enum { WINDOWS_FILE_TYPE_PIPE = FILE_TYPE_PIPE };
#undef FILE_TYPE_PIPE

//=========================================
// Virtual memory
//=========================================
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

//=========================================
// Threading
//=========================================
isize platform_thread_get_proccessor_count()
{
    return GetCurrentProcessorNumber();
}

//typedef DWORD (WINAPI *PTHREAD_START_ROUTINE)(
//    LPVOID lpThreadParameter
//    );
Thread platform_thread_create(int (*func)(void*), void* context, isize stack_size)
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
    Sleep((DWORD) ms);
}

void platform_thread_exit(int code)
{
    ExitThread(code);
}

int platform_thread_join(Thread thread)
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

static time_t _filetime_to_time_t(FILETIME ft)  
{    
    ULARGE_INTEGER ull;    
    ull.LowPart = ft.dwLowDateTime;    
    ull.HighPart = ft.dwHighDateTime;    
    return (time_t) (ull.QuadPart / 10000000ULL - 11644473600ULL);  
}

static bool _is_file_link(const wchar_t* directory_path)
{
    HANDLE file = CreateFileW(directory_path, 0, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                              NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    size_t requiredSize = GetFinalPathNameByHandleW(file, NULL, 0, FILE_NAME_NORMALIZED);
    CloseHandle(file);

    return requiredSize == 0;
}

bool platform_file_info(const char* file_path, File_Info* info)
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
        info->type = FILE_TYPE_DIRECTORY;
    else
        info->type = FILE_TYPE_FILE;

    return state;
}
    
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

static bool _directory_list_contents_malloc(const wchar_t* directory_path, _Buffer* entries, isize max_depth)
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
        
            File_Info info = {0};
            info.created_time = _filetime_to_time_t(dir_context->visitor.current_entry.ftCreationTime);
            info.last_access_time = _filetime_to_time_t(dir_context->visitor.current_entry.ftLastAccessTime);
            info.last_write_time = _filetime_to_time_t(dir_context->visitor.current_entry.ftLastWriteTime);
            info.size = ((int64_t) dir_context->visitor.current_entry.nFileSizeHigh << 32) | ((int64_t) dir_context->visitor.current_entry.nFileSizeLow);
        
            info.type = FILE_TYPE_FILE;
            if(dir_context->visitor.current_entry.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
                info.type = FILE_TYPE_DIRECTORY;
            else
                info.type = FILE_TYPE_FILE;

            if(info.is_link)
                info.is_link = _is_file_link(_wstring(built_path));  


            int flag = IO_NORMALIZE_LINUX;
            if(info.type == FILE_TYPE_DIRECTORY)
                flag |= IO_NORMALIZE_DIRECTORY;
            else
                flag |= IO_NORMALIZE_FILE;
            _Buffer out_string = _malloc_full_path(_wstring(built_path), flag);

            Directory_Entry entry = {0};
            entry.info = info;
            entry.path = _string(out_string);
            entry.path_size = out_string.size;
    
            _buffer_push(entries, &entry, sizeof(entry));

            bool recursion = dir_context->depth + 1 < max_depth || max_depth <= 0;
            if(info.type == FILE_TYPE_DIRECTORY && info.is_link == false && recursion)
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
    Directory_Entry terminator = {0};
    _buffer_push(entries, &terminator, sizeof(terminator));

    _buffer_deinit(&stack);
    _buffer_deinit(&built_path);
    return true;
}

bool platform_directory_list_contents_malloc(const char* directory_path, Directory_Entry** entries, int64_t* entries_count, isize max_depth)
{
    int64_t path_size = _strlen(directory_path);
    assert(entries != NULL && entries_count != NULL);
    _Buffer entries_stack = _buffer_init(sizeof(Directory_Entry));
    IO_NORMALIZE_PATH_OP(directory_path, path_size,
        bool ok = _directory_list_contents_malloc(_wstring(normalized_path), &entries_stack, max_depth);
    )

    *entries = (Directory_Entry*) entries_stack.data;
    *entries_count = entries_stack.size;
    return ok;
}

//Frees previously allocated file list
void platform_directory_list_contents_free(Directory_Entry* entries)
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
    HWND handle;
    WNDCLASSEX class_;
    RECT rect;  
    Window_Visibility visibility;
    bool was_shown;
} Window;

typedef int64_t isize;
Window*     global_windows = NULL;
isize       global_window_slots = 0;
bool        global_window_class_registered = false;
WNDCLASSEX  global_window_class = {0};

static isize _find_window(HWND handle)
{
    for(isize i = 0; i < global_window_slots; i++)
    {
        assert(global_windows != NULL);
        if(global_windows[i].handle == handle)
            return i;
    }
    return -1;
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

Window* platform_window_init(const char* window_title)
{
    HINSTANCE hInstance = 0;
    const wchar_t WINDOW_CLASS_NAME[] = L"jot_window_class";
    if(!global_window_class_registered)
    {
        global_window_class.cbSize        = sizeof(WNDCLASSEX);
        global_window_class.style         = 0;
        global_window_class.lpfnWndProc   = WndProc;
        global_window_class.cbClsExtra    = 0;
        global_window_class.cbWndExtra    = 0;
        global_window_class.hInstance     = hInstance;
        global_window_class.hIcon         = LoadIconW(NULL, IDI_APPLICATION);
        global_window_class.hCursor       = LoadCursorW(NULL, IDC_ARROW);
        global_window_class.hbrBackground = (HBRUSH)(COLOR_WINDOW+1);
        global_window_class.lpszMenuName  = NULL;
        global_window_class.lpszClassName = WINDOW_CLASS_NAME;
        global_window_class.hIconSm       = LoadIconW(NULL, IDI_APPLICATION);
        if(!RegisterClassExW(&global_window_class))
            return NULL;

        global_window_class_registered = true;
    }

    isize empty_index = _find_window(NULL);
    if(empty_index == -1)
    {
        isize new_count = global_window_slots * 2;
        if(new_count < 8)
            new_count = 8;

        Window* new_data = (Window*) realloc(global_windows, sizeof(Window)*new_count);
        if(new_data == NULL)
            return NULL;

        memset(new_data + global_window_slots, 0, (new_count - global_window_slots)*sizeof(Window));
        empty_index = global_window_slots;
        global_windows = new_data;
        global_window_slots = new_count;
    }

    Window* created = &global_windows[empty_index];
    assert(created->handle == NULL);

    char window_title_buffer[IO_LOCAL_BUFFER_SIZE] = {0}; 
    _Buffer wide_title = _buffer_init_backed(window_title_buffer, IO_LOCAL_BUFFER_SIZE, sizeof(wchar_t));
    _utf8_to_utf16(window_title, _strlen(window_title), &wide_title);

    created->handle = CreateWindowExW(
        WS_EX_CLIENTEDGE,
        WINDOW_CLASS_NAME,
        _wstring(wide_title),
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
        NULL, NULL, hInstance, NULL);
        
    _buffer_deinit(&wide_title);
    if(created->handle == NULL)
        return NULL;
    
    RAWINPUTDEVICE raw_input_device;
    raw_input_device.usUsagePage = HID_USAGE_PAGE_GENERIC; 
    raw_input_device.usUsage = HID_USAGE_GENERIC_MOUSE; 
    raw_input_device.dwFlags = RIDEV_INPUTSINK;   
    raw_input_device.hwndTarget = created->handle;
    RegisterRawInputDevices(&raw_input_device, 1, sizeof(raw_input_device));

    created->class_ = global_window_class;
    return created;
}

void platform_window_deinit(Window* window)
{
    assert(window->handle != NULL);
    DestroyWindow(window->handle);
    Window null = {0};
    *window = null;
}

//@TODO: make better
static void _window_show_buttons(Window* window, bool close, bool minimize, bool maximize)
{
    UINT close_flags = 0;
    LONG other_flags = GetWindowLongW(window->handle, GWL_STYLE);
    if(close)
        close_flags = MF_BYCOMMAND | MF_ENABLED;
    else
        close_flags = MF_BYCOMMAND | MF_DISABLED | MF_GRAYED;

    if(minimize)
        other_flags |= WS_MINIMIZE;
    else
        other_flags &= ~WS_MINIMIZE;
        
    if(maximize)
        other_flags |= WS_MAXIMIZE;
    else
        other_flags &= ~WS_MAXIMIZE;

    SetWindowLongW(window->handle, GWL_STYLE, other_flags);
    EnableMenuItem(GetSystemMenu(window->handle, FALSE), SC_CLOSE, close_flags);
}

static bool _window_is_active(Window* window)
{
    HWND temp = GetActiveWindow();
    return temp == window->handle;
}

void platform_window_set_visibility(Window* window, Window_Visibility visibility, bool focused)
{
    //first showing should have SW_SHOWNORMAL
    if(window->was_shown == false)
    {
        ShowWindow(window->handle, SW_SHOWNORMAL);
        window->was_shown = true;
    }

    int show = SW_SHOWNORMAL;
    switch(visibility)
    {
        case WINDOW_VISIBILITY_FULLSCREEN: show = SW_SHOWMAXIMIZED; break;
        case WINDOW_VISIBILITY_BORDERLESS_FULLSCREEN: show = SW_SHOWMAXIMIZED; break;
        case WINDOW_VISIBILITY_WINDOWED: show = SW_SHOWNORMAL; break;
        case WINDOW_VISIBILITY_MINIMIZED: show = SW_SHOWMINIMIZED; break;
        default: show = SW_SHOWNORMAL; break;
    }

    ShowWindow(window->handle, show);
    if(_window_is_active(window) != focused)
    {
        if(focused) 
            ShowWindow(window->handle, SW_SHOW);
        else
            ShowWindow(window->handle, SW_SHOWNA);
    }

    UpdateWindow(window->handle);
}

Input platform_window_process_messages(Window* window)
{
    MSG Msg = {0};
    Input input = {0};
    while(PeekMessageW(&Msg, window->handle, 0, 0, PM_REMOVE) > 0)
    {
        TranslateMessage(&Msg);
        //DispatchMessageW(&Msg);

        switch(Msg.message)
        {
            case WM_MOUSEMOVE:
            {
                input.mouse_app_resolution_x = GET_X_LPARAM(Msg.lParam); 
                input.mouse_app_resolution_y = GET_Y_LPARAM(Msg.lParam); 
                //printf("ABS mouse pos %d %d\n", xPosAbsolute, yPosAbsolute);
                break;
            }
            case WM_INPUT: 
            {
                //break;
                UINT dwSize = sizeof(RAWINPUT);
                uint8_t lpb[sizeof(RAWINPUT)] = {0};

                GetRawInputData((HRAWINPUT)Msg.lParam, RID_INPUT, lpb, &dwSize, sizeof(RAWINPUTHEADER));

                RAWINPUT* raw = (RAWINPUT*)lpb;

                if (raw->header.dwType == RIM_TYPEMOUSE) 
                {
                    input.mouse_relative_x += raw->data.mouse.lLastX;
                    input.mouse_relative_y += raw->data.mouse.lLastY;
                } 
                break;
            }

            case WM_CLOSE:
            case WM_DESTROY:
                input.should_quit = true;
            break;

            default:
                DefWindowProcW(window->handle, Msg.message, Msg.wParam, Msg.lParam);
            break;
        }
    }

    (void) GetKeyboardState(input.keys);

    input.window_position_x = window->rect.left;
    input.window_position_y = window->rect.top;
    input.window_size_x = (int64_t) window->rect.right - window->rect.left;
    input.window_size_y = (int64_t) window->rect.bottom - window->rect.top;

    return input;
}

static Window* _get_window(HWND hwnd)
{
    isize window_index = _find_window(hwnd);
    assert(window_index != -1 && global_windows != NULL);
    return &global_windows[window_index];
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch(msg)
    {
        case WM_MOVING:
        case WM_SIZING: {
            RECT* rect = (RECT*) lParam; 
            Window* window = _get_window(hwnd);
            window->rect = *rect;
            return 0;
        }
        
        //case WM_INPUT: 
        //{
        //    UINT dwSize = sizeof(RAWINPUT);
        //    uint8_t lpb[sizeof(RAWINPUT)] = {0};
        //
        //    GetRawInputData((HRAWINPUT)msg, RID_INPUT, lpb, &dwSize, sizeof(RAWINPUTHEADER));
        //
        //    RAWINPUT* raw = (RAWINPUT*)lpb;
        //
        //    if (raw->header.dwType == RIM_TYPEMOUSE) 
        //    {
        //        if(!(raw->data.mouse.usFlags & MOUSE_MOVE_ABSOLUTE))
        //        {
        //            Window* window = get_window(hwnd);
        //            window->mouse_relative_x += raw->data.mouse.lLastX;
        //            window->mouse_relative_y += raw->data.mouse.lLastY;
        //        }
        //    } 
        //    break;
        //}
        //case WM_CLOSE:
        //    DestroyWindow(hwnd);
        //break;
        //case WM_DESTROY:
        //    PostQuitMessage(0);
        //break;

        default:
            return DefWindowProcW(hwnd, msg, wParam, lParam);
    }
}

Window_Popup_Controls platform_window_make_popup(Window_Popup_Style desired_style, const char* message, const char* title)
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
