#ifndef LIB_PLATFORM
#define LIB_PLATFORM

#include <stdint.h>
#include <time.h>
#include <stdbool.h>

//=========================================
// Virtual memory
//=========================================

typedef enum Platform_Virtual_Allocation
{
    PLATFORM_VIRTUAL_ALLOC_RESERVE, //Reserves adress space so that no other allocation can be made there
    PLATFORM_VIRTUAL_ALLOC_COMMIT,  //Commits adress space causing operating system to suply physical memory or swap file
    PLATFORM_VIRTUAL_ALLOC_DECOMMIT,//Removes adress space from commited freeing physical memory
    PLATFORM_VIRTUAL_ALLOC_RELEASE, //Free adress space
} Platform_Virtual_Allocation;

typedef enum Platform_Memory_Protection
{
    PLATFORM_MEMORY_PROT_NO_ACCESS,
    PLATFORM_MEMORY_PROT_READ,
    PLATFORM_MEMORY_PROT_WRITE,
    PLATFORM_MEMORY_PROT_READ_WRITE
} Platform_Memory_Protection;

void* platform_virtual_reallocate(void* allocate_at, int64_t bytes, Platform_Virtual_Allocation action, Platform_Memory_Protection protection);
void* platform_heap_reallocate(int64_t new_size, void* old_ptr, int64_t old_size, int64_t align);

//=========================================
// Threading
//=========================================

typedef struct Platform_Thread
{
    void* handle;
    int32_t id;
} Platform_Thread;

int64_t         platform_thread_get_proccessor_count();
Platform_Thread platform_thread_create(int (*func)(void*), void* context, int64_t stack_size); //CreateThread
void            platform_thread_destroy(Platform_Thread* thread); //CloseHandle 

int64_t         platform_thread_get_id();
void            platform_thread_yield();
void            platform_thread_sleep(int64_t ms);
void            platform_thread_exit(int code);
int             platform_thread_join(Platform_Thread thread);

//=========================================
// Atomics 
//=========================================
inline static void platform_compiler_memory_fence();
inline static void platform_memory_fence();
inline static void platform_processor_pause();

inline static int64_t platform_find_first_set_bit(int64_t num);
inline static int64_t platform_find_last_set_bit(int64_t num);
inline static int64_t platform_pop_count(int64_t num);

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
    PLATFORM_FILE_TYPE_NOT_FOUND = 0,
    PLATFORM_FILE_TYPE_FILE = 1,
    PLATFORM_FILE_TYPE_DIRECTORY = 4,
    PLATFORM_FILE_TYPE_CHARACTER_DEVICE = 2,
    PLATFORM_FILE_TYPE_PIPE = 3,
    PLATFORM_FILE_TYPE_OTHER = 5,
} File_Type;

typedef struct Platform_File_Info
{
    int64_t size;
    File_Type type;
    time_t created_time;
    time_t last_write_time;  
    time_t last_access_time; //The last time file was either read or written
    bool is_link; //if file/dictionary is actually just a link (hardlink or softlink or symlink)
} Platform_File_Info;
    
typedef struct Platform_Directory_Entry
{
    char* path;
    int64_t path_size;
    int64_t index_within_directory;
    int64_t directory_depth;
    Platform_File_Info info;
} Platform_Directory_Entry;

//retrieves info about the specified file or directory
bool platform_file_info(const char* file_path, Platform_File_Info* info);
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
bool platform_directory_list_contents_malloc(const char* directory_path, Platform_Directory_Entry** entries, int64_t* entries_count, int64_t max_depth);
//Frees previously allocated file list
void platform_directory_list_contents_free(Platform_Directory_Entry* entries);


//=========================================
// Window managmenet
//=========================================

typedef struct Window Window;
//typedef enum Platform_Mouse_Button Platform_Mouse_Button;
//typedef enum Platform_Keyboard_Key Platform_Keyboard_Key;
//Platform_
typedef enum Platform_Mouse_Button {
    PLATFORM_MOUSE_BUTTON_LEFT,
    PLATFORM_MOUSE_BUTTON_RIGHT,
    PLATFORM_MOUSE_BUTTON_MIDDLE,
    PLATFORM_MOUSE_BUTTON_4,
    PLATFORM_MOUSE_BUTTON_5,

    PLATFORM_MOUSE_BUTTON_ENUM_COUNT,
} Platform_Mouse_Button;

typedef enum Platform_Keyboard_Key {
    
    PLATFORM_KEY_BACKSPACE = 0x08,
    PLATFORM_KEY_ENTER = 0x0D,
    PLATFORM_KEY_TAB = 0x09,
    //PLATFORM_KEY_SHIFT = 0x10,
    //PLATFORM_KEY_CONTROL = 0x11,

    PLATFORM_KEY_PAUSE = 0x13,
    PLATFORM_KEY_CAPS_LOCK = 0x14,

    PLATFORM_KEY_ESCAPE = 0x1B,

    PLATFORM_KEY_SPACE = 0x20,
    PLATFORM_KEY_PAGEUP = 0x21,
    PLATFORM_KEY_PAGEDOWN = 0x22,
    PLATFORM_KEY_END = 0x23,
    PLATFORM_KEY_HOME = 0x24,
    PLATFORM_KEY_LEFT = 0x25,
    PLATFORM_KEY_UP = 0x26,
    PLATFORM_KEY_RIGHT = 0x27,
    PLATFORM_KEY_DOWN = 0x28,
    PLATFORM_KEY_SELECT = 0x29,
    PLATFORM_KEY_PRINT = 0x2A,
    PLATFORM_KEY_EXECUTE = 0x2B,
    PLATFORM_KEY_PRINTSCREEN = 0x2C,
    PLATFORM_KEY_INSERT = 0x2D,
    PLATFORM_KEY_DELETE = 0x2E,
    PLATFORM_KEY_HELP = 0x2F,

    PLATFORM_KEY_0 = 0x30,
    PLATFORM_KEY_1 = 0x31,
    PLATFORM_KEY_2 = 0x32,
    PLATFORM_KEY_3 = 0x33,
    PLATFORM_KEY_4 = 0x34,
    PLATFORM_KEY_5 = 0x35,
    PLATFORM_KEY_6 = 0x36,
    PLATFORM_KEY_7 = 0x37,
    PLATFORM_KEY_8 = 0x38,
    PLATFORM_KEY_9 = 0x39,

    PLATFORM_KEY_A = 0x41,
    PLATFORM_KEY_B = 0x42,
    PLATFORM_KEY_C = 0x43,
    PLATFORM_KEY_D = 0x44,
    PLATFORM_KEY_E = 0x45,
    PLATFORM_KEY_F = 0x46,
    PLATFORM_KEY_G = 0x47,
    PLATFORM_KEY_H = 0x48,
    PLATFORM_KEY_I = 0x49,
    PLATFORM_KEY_J = 0x4A,
    PLATFORM_KEY_K = 0x4B,
    PLATFORM_KEY_L = 0x4C,
    PLATFORM_KEY_M = 0x4D,
    PLATFORM_KEY_N = 0x4E,
    PLATFORM_KEY_O = 0x4F,
    PLATFORM_KEY_P = 0x50,
    PLATFORM_KEY_Q = 0x51,
    PLATFORM_KEY_R = 0x52,
    PLATFORM_KEY_S = 0x53,
    PLATFORM_KEY_T = 0x54,
    PLATFORM_KEY_U = 0x55,
    PLATFORM_KEY_V = 0x56,
    PLATFORM_KEY_W = 0x57,
    PLATFORM_KEY_X = 0x58,
    PLATFORM_KEY_Y = 0x59,
    PLATFORM_KEY_Z = 0x5A,

    PLATFORM_KEY_LSUPER = 0x5B, //Left windows/super key
    PLATFORM_KEY_RSUPER = 0x5C, //right windows/super key
    PLATFORM_KEY_APPS = 0x5D,

    PLATFORM_KEY_SLEEP = 0x5F,

    PLATFORM_KEY_NUMPAD0 = 0x60,
    PLATFORM_KEY_NUMPAD1 = 0x61,
    PLATFORM_KEY_NUMPAD2 = 0x62,
    PLATFORM_KEY_NUMPAD3 = 0x63,
    PLATFORM_KEY_NUMPAD4 = 0x64,
    PLATFORM_KEY_NUMPAD5 = 0x65,
    PLATFORM_KEY_NUMPAD6 = 0x66,
    PLATFORM_KEY_NUMPAD7 = 0x67,
    PLATFORM_KEY_NUMPAD8 = 0x68,
    PLATFORM_KEY_NUMPAD9 = 0x69,

    PLATFORM_KEY_MULTIPLY = 0x6A,
    PLATFORM_KEY_ADD = 0x6B,
    PLATFORM_KEY_SEPARATOR = 0x6C,
    PLATFORM_KEY_SUBTRACT = 0x6D,
    PLATFORM_KEY_DECIMAL = 0x6E,
    PLATFORM_KEY_DIVIDE = 0x6F,

    PLATFORM_KEY_F1 = 0x70,
    PLATFORM_KEY_F2 = 0x71,
    PLATFORM_KEY_F3 = 0x72,
    PLATFORM_KEY_F4 = 0x73,
    PLATFORM_KEY_F5 = 0x74,
    PLATFORM_KEY_F6 = 0x75,
    PLATFORM_KEY_F7 = 0x76,
    PLATFORM_KEY_F8 = 0x77,
    PLATFORM_KEY_F9 = 0x78,
    PLATFORM_KEY_F10 = 0x79,
    PLATFORM_KEY_F11 = 0x7A,
    PLATFORM_KEY_F12 = 0x7B,
    PLATFORM_KEY_F13 = 0x7C,
    PLATFORM_KEY_F14 = 0x7D,
    PLATFORM_KEY_F15 = 0x7E,
    PLATFORM_KEY_F16 = 0x7F,
    PLATFORM_KEY_F17 = 0x80,
    PLATFORM_KEY_F18 = 0x81,
    PLATFORM_KEY_F19 = 0x82,
    PLATFORM_KEY_F20 = 0x83,
    PLATFORM_KEY_F21 = 0x84,
    PLATFORM_KEY_F22 = 0x85,
    PLATFORM_KEY_F23 = 0x86,
    PLATFORM_KEY_F24 = 0x87,

    PLATFORM_KEY_NUMLOCK = 0x90,
    PLATFORM_KEY_SCROLL_LOCK = 0x91,
    PLATFORM_KEY_NUMPAD_EQUAL = 0x92,

    PLATFORM_KEY_LSHIFT = 0xA0,
    PLATFORM_KEY_RSHIFT = 0xA1,
    PLATFORM_KEY_LCONTROL = 0xA2,
    PLATFORM_KEY_RCONTROL = 0xA3,
    PLATFORM_KEY_LALT = 0xA4,
    PLATFORM_KEY_RALT = 0xA5,

    PLATFORM_KEY_SEMICOLON = 0x3B,

    PLATFORM_KEY_APOSTROPHE = 0xDE,
    PLATFORM_KEY_QUOTE = PLATFORM_KEY_APOSTROPHE,
    PLATFORM_KEY_EQUAL = 0xBB,
    PLATFORM_KEY_COMMA = 0xBC,
    PLATFORM_KEY_MINUS = 0xBD,
    PLATFORM_KEY_DOT   = 0xBE,
    PLATFORM_KEY_SLASH = 0xBF,
    PLATFORM_KEY_GRAVE = 0xC0,
    
    //EXTRA!
    PLATFORM_KEY_LPAREN = 0x0100,
    PLATFORM_KEY_RPAREN = 0x0101,

    PLATFORM_KEY_LBRACKET_SQUARE = 0x0102,
    PLATFORM_KEY_RBRACKET_SQUARE = 0x0103,
    
    PLATFORM_KEY_LBRACKET_CURLY = 0x0104,
    PLATFORM_KEY_RBRACKET_CURLY = 0x0105,
    PLATFORM_KEY_BACKSLASH = 0x0106, 

    PLATFORM_KEY_PIPE = 0xDC,
    PLATFORM_KEY_PLAY = 0xFA,
    PLATFORM_KEY_ZOOM = 0xFB,

    PLATFORM_KEY_ENUM_COUNT = 512,
    PLATFORM_KEY_PRESSED_BIT = 1 << 7,
} Platform_Keyboard_Key;

typedef enum Platform_Window_Visibility
{
    WINDOW_VISIBILITY_HIDDEN = 0,
    WINDOW_VISIBILITY_WINDOWED,
    WINDOW_VISIBILITY_MINIMIZED,
    WINDOW_VISIBILITY_MAXIMIZED,
    WINDOW_VISIBILITY_FULLSCREEN,
} Platform_Window_Visibility;

typedef enum Platform_Window_Popup_Style
{
    PLATFORM_POPUP_STYLE_OK = 0,
    PLATFORM_POPUP_STYLE_ERROR,
    PLATFORM_POPUP_STYLE_WARNING,
    PLATFORM_POPUP_STYLE_INFO,
    PLATFORM_POPUP_STYLE_RETRY_ABORT,
    PLATFORM_POPUP_STYLE_YES_NO,
    PLATFORM_POPUP_STYLE_YES_NO_CANCEL,
} Platform_Window_Popup_Style;

typedef enum Platform_Window_Popup_Controls
{
    PLATFORM_POPUP_CONTROL_OK,
    PLATFORM_POPUP_CONTROL_CANCEL,
    PLATFORM_POPUP_CONTROL_CONTINUE,
    PLATFORM_POPUP_CONTROL_ABORT,
    PLATFORM_POPUP_CONTROL_RETRY,
    PLATFORM_POPUP_CONTROL_YES,
    PLATFORM_POPUP_CONTROL_NO,
    PLATFORM_POPUP_CONTROL_IGNORE,
} Platform_Window_Popup_Controls;

typedef struct Platform_Input {
    int32_t mouse_native_delta_x;
    int32_t mouse_native_delta_y;

    int32_t mouse_screen_x;
    int32_t mouse_screen_y;

    //The number of presses/releases this frame. The highest bit indicates if it is currently down or up.
    // This means if key is pressed and released within single frame its counter will be 2 and highest bit 0.
    // If it started the frame pressed then was released and pressed this frame then counter will be 2 and and highest bit 1
    uint8_t key_half_transitions[PLATFORM_KEY_ENUM_COUNT];
    uint8_t mouse_button_half_transitions[PLATFORM_MOUSE_BUTTON_ENUM_COUNT];
    int32_t mouse_wheel_delta; 
    bool should_quit;
    
    int32_t window_position_x;
    int32_t window_position_y;
    int32_t window_size_x;
    int32_t window_size_y;
    
    int32_t client_position_x;
    int32_t client_position_y;
    int32_t client_size_x;
    int32_t client_size_y;
} Platform_Input;

typedef struct Platform_Window_Options
{
    Platform_Window_Visibility visibility;
    bool no_border;                         //If set will not have any window tray
    bool no_border_allow_resize;            //If set clicking and draging the window edge resizes the window
    bool no_border_allow_move;              //If set clicking and dragging anywhere within the window moves it
    bool no_focus;                          //If set the window will not recieve input untill clicked or tabbed to again
    const char* title;                      //a new title for the window. If null does nothing. When returned from platform_window_get_options() needs to be freed using platform_heap_reallocate()!
} Platform_Window_Options;

Window*         platform_window_init(const char* window_title);
void            platform_window_deinit(Window* window);
Platform_Input  platform_window_process_messages(Window* window);

bool                            platform_window_set_options(Window* window, Platform_Window_Options options);
Platform_Window_Options         platform_window_get_options(Window* window);
Platform_Window_Popup_Controls  platform_window_make_popup(Platform_Window_Popup_Style desired_style, const char* message, const char* title);

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

    //inline static int64_t platform_find_first_set_bit(int64_t num)
    //{
    //    
    //}
    //
    //inline static int64_t platform_find_last_set_bit(int64_t num)
    //{
    //    
    //}
    //
    //inline static int64_t platform_pop_count(int64_t num)
    //{
    //
    //}

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
        return 0;
        //return (int64_t) _InterlockedAddLargeStatistic((volatile long long*) target, (long long) value);
    }

    inline static int32_t platform_interlocked_add32(volatile int32_t* target, int32_t value)
    {
        return 0;
        //return (int32_t) _InterlockedAdd((volatile long*) target, (long) value);
    }
    
#endif

#endif