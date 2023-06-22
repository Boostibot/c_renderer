
#include <stdio.h>
#include <Windows.h>
#include <WinUser.h>
#include <stdbool.h>
#include <stdint.h>
#include <hidusage.h>
#include <windowsx.h>

#include <windows.h>

const wchar_t g_szClassName[] = L"myWindowClass";

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

typedef struct Input {
    int64_t mouse_relative_x;
    int64_t mouse_relative_y;

    uint8_t keys[256];
    bool should_quit;
    bool window_resized;
    
    int64_t window_size_x;
    int64_t window_size_y;
} Input;


// Step 4: the Window Procedure
LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch(msg)
    {
        case WM_ENTERSIZEMOVE:
        {
            break;
        }
        case WM_EXITSIZEMOVE:
        {
            break;
        }
        case WM_CLOSE:
            DestroyWindow(hwnd);
        break;
        case WM_DESTROY:
            PostQuitMessage(0);
        break;
        default:
            return DefWindowProcW(hwnd, msg, wParam, lParam);
    }
    return 0;
}

#if 0
int APIENTRY WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance,
    LPSTR lpCmdLine, int nCmdShow)
{
    WNDCLASSEX wc;
    HWND hwnd;
    MSG Msg;

#else


typedef struct Window Window;

typedef enum Window_Visibility
{
    WINDOW_VISIBILITY_FULLSCREEN,
    WINDOW_VISIBILITY_BORDERLESS_FULLSCREEN,
    WINDOW_VISIBILITY_WINDOWED,
    WINDOW_VISIBILITY_MINIMIZED,
} Window_Visibility;

Window* window_init(const char* window_title);
void    window_deinit(Window* window);
Input   window_process_messages(Window* window);

void window_set_visibility(Window* window, Window_Visibility visibility, bool focused);


int main()
{
    int nCmdShow = SW_SHOWNORMAL;
    LPSTR lpCmdLine = NULL;
    HINSTANCE hInstance = 0;
    HINSTANCE hPrevInstance = 0;

    WNDCLASSEX wc;
    HWND hwnd;
    MSG Msg;

    //Step 1: Registering the Window Class
    wc.cbSize        = sizeof(WNDCLASSEX);
    wc.style         = 0;
    wc.lpfnWndProc   = WndProc;
    wc.cbClsExtra    = 0;
    wc.cbWndExtra    = 0;
    wc.hInstance     = hInstance;
    wc.hIcon         = LoadIconW(NULL, IDI_APPLICATION);
    wc.hCursor       = LoadCursorW(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW+1);
    wc.lpszMenuName  = NULL;
    wc.lpszClassName = g_szClassName;
    wc.hIconSm       = LoadIconW(NULL, IDI_APPLICATION);

#endif
    if(!RegisterClassExW(&wc))
    {
        MessageBoxW(NULL, L"Window Registration Failed!", L"Error!",
            MB_ICONEXCLAMATION | MB_OK);
        return 0;
    }

    // Step 2: Creating the Window
    hwnd = CreateWindowExW(
        WS_EX_CLIENTEDGE,
        g_szClassName,
        L"The title of my window",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, 240, 120,
        NULL, NULL, hInstance, NULL);

    if(hwnd == NULL)
    {
        MessageBoxW(NULL, L"Window Creation Failed!", L"Error!",
            MB_ICONEXCLAMATION | MB_OK);
        return 0;
    }
    
    RAWINPUTDEVICE Rid[1];
    Rid[0].usUsagePage = HID_USAGE_PAGE_GENERIC; 
    Rid[0].usUsage = HID_USAGE_GENERIC_MOUSE; 
    Rid[0].dwFlags = RIDEV_INPUTSINK;   
    Rid[0].hwndTarget = hwnd;
    RegisterRawInputDevices(Rid, 1, sizeof(Rid[0]));

    ShowWindow(hwnd, nCmdShow);
    UpdateWindow(hwnd);

    platform_make_popup(POPUP_STYLE_ERROR, "Hello :D", "Title");


    uint8_t keyboard[256] = {0};
    int64_t i = 0;
    // Step 3: The Message Loop
    
    bool is_being_resized = false;
    Input global_input = {0};
    while(true)
    {
        bool had_msg = false;
        while(PeekMessageA(&Msg, hwnd, 0, 0, PM_REMOVE) > 0)
        {
            had_msg = true;
            TranslateMessage(&Msg);
            //DispatchMessageW(&Msg);

            global_input.window_resized = is_being_resized;
            if(is_being_resized)
                printf("resizing\n");
            switch(Msg.message)
            {
                case WM_ENTERSIZEMOVE:
                {
                    is_being_resized = true;
                    break;
                }
                case WM_EXITSIZEMOVE:
                {
                    is_being_resized = false;
                    break;
                }
                case WM_MOUSEMOVE:
                {
                    int xPosAbsolute = GET_X_LPARAM(Msg.lParam); 
                    int yPosAbsolute = GET_Y_LPARAM(Msg.lParam);
                    printf("ABS mouse pos %d %d\n", xPosAbsolute, yPosAbsolute);
                    break;
                }
                case WM_INPUT: 
                {
                    UINT dwSize = sizeof(RAWINPUT);
                    uint8_t lpb[sizeof(RAWINPUT)] = {0};

                    GetRawInputData((HRAWINPUT)Msg.lParam, RID_INPUT, lpb, &dwSize, sizeof(RAWINPUTHEADER));

                    RAWINPUT* raw = (RAWINPUT*)lpb;

                    if (raw->header.dwType == RIM_TYPEMOUSE) 
                    {
                        global_input.mouse_relative_x = raw->data.mouse.lLastX;
                        global_input.mouse_relative_x = raw->data.mouse.lLastY;
                        printf("REL mouse pos %d %d\n", (int) global_input.mouse_relative_x, (int) global_input.mouse_relative_y);
                    } 
                    break;
                }
                
                default:
                    DefWindowProcW(hwnd, Msg.message, Msg.wParam, Msg.lParam);
            }

            i++;
            bool ok = GetKeyboardState(keyboard);
            if(!ok)
                platform_make_popup(POPUP_STYLE_ERROR, "Bad", "Bad input");
        
            //A
            if(keyboard[0x41] & (1 << 7))
            {
                platform_make_popup(POPUP_STYLE_ERROR, "A", "A");
            }
            if(keyboard[0x42] & (1 << 7))
            {
                platform_make_popup(POPUP_STYLE_ERROR, "B", "B");
            }
        }
        
        if(had_msg == false)
            printf("No msgs\n");
    }

    return Msg.wParam;
}

#include "c_array.h"
#include "c_time.h"
#include "c_string.h"
#include "c_format.h"

int _main()
{


    double start = clock_s();

    String_Builder builder = {0};
    String str = string_make("Hello.world.here.and.there.and.back");

    builder_append(&builder, str);

    println("string: \"%s\"", builder_cstring(builder));

    String_Array parts = string_split(str, string_make("."));
    String_Builder formatted = {0};
    for(isize i = 0; i < parts.size; i++)
    {
        String part = parts.data[i];
        formatln_into(&formatted, "%.*s", part.size, part.data);
    }

    String_Builder composite = string_join_any(parts.data, parts.size, string_make("//"));
    print_builder(formatted);
    println("formatted: \"%s\"", builder_cstring(formatted));
    println("composite: \"%s\"", builder_cstring(composite));
    //_array_deinit(&builder, sizeof(char), SOURCE_INFO());
    array_deinit(&builder);
    array_deinit(&parts);
    array_deinit(&composite);
    array_deinit(&formatted);

    Allocator* alloc = allocator_get_default();
    Allocator_Stats stats = allocator_get_stats(allocator_get_default());

    double end = clock_s();

    println("diff: %lf", end - start);

    allocator_reallocate(allocator_get_default(), 1000000000000, NULL, 0, 8, SOURCE_INFO());
    return 0;
}
