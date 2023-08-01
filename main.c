#define PROGRAM 2
#define _CRT_SECURE_NO_WARNINGS
#define LIB_ALL_IMPL
#define LIB_MEM_DEBUG

#include "platform.h"
#include "string.h"
#include "hash_table.h"
#include "_test_random.h"
#include "_test_array.h"
#include "_test_hash_table.h"

#include "_test_log.h"
int main()
{
    test_log();

    Window* window = platform_window_init("My window");
    assert(window != NULL);
    Platform_Window_Options show = {0};
    show.visibility = WINDOW_VISIBILITY_WINDOWED;
    //show.no_border = true;
    //show.no_border_allow_move = true;
    platform_window_set_options(window, show);
    platform_window_activate_opengl(window);

    int i = 0;
    Platform_Input input = {0};
    while(input.should_quit == false)
    {
        Platform_Input new_input = platform_window_process_messages(window);
        
        //if(new_input.mouse_native_delta_x != 0 || new_input.mouse_`native_delta_y != 0)
            //printf("REL mouse pos %d %d\n", (int) new_input.mouse_native_delta_x, (int) new_input.mouse_native_delta_y);
            
        //if(new_input.mouse_screen_x != input.mouse_screen_x 
            //|| new_input.mouse_screen_y != input.mouse_screen_y )
            //printf("ABS mouse pos %d %d\n", (int) new_input.mouse_screen_x, (int) new_input.mouse_screen_y);
        
        if(new_input.window_size_x != input.window_size_x)
        {
            printf("\n");
            printf("client pos:  [%d, %d]\n", (int) new_input.client_position_x, (int) new_input.client_position_y);
            printf("window pos:  [%d, %d]\n", (int) new_input.window_position_x, (int) new_input.window_position_y);

            printf("client size: [%d, %d]\n", (int) new_input.client_size_x, (int) new_input.client_size_y);
            printf("window size: [%d, %d]\n", (int) new_input.window_size_x, (int) new_input.window_size_y);
        }

        uint8_t key = new_input.key_half_transitions[PLATFORM_KEY_EXECUTE];
        if((key & ~PLATFORM_KEY_PRESSED_BIT) > 0)
        {
            const char* down = key & PLATFORM_KEY_PRESSED_BIT ? "down" : "up";
            printf("EXE %s %d\n", down, key & ~PLATFORM_KEY_PRESSED_BIT);
        }
        
        key = new_input.key_half_transitions[PLATFORM_KEY_ENTER];
        if((key & ~PLATFORM_KEY_PRESSED_BIT) > 0)
        {
            if(key & PLATFORM_KEY_PRESSED_BIT)
            {
                i += 1;
                if(i % 2 == 0)
                {
                    show.title = "small áěářěéř";
                    show.visibility = WINDOW_VISIBILITY_HIDDEN;

                    Platform_Window_Options options = platform_window_get_options(window);
                    printf("%s\n", options.title);
                }
                else
                {
                    show.title = "BIG ÝŘĚÁŘÉ";
                    show.visibility = WINDOW_VISIBILITY_WINDOWED;
                }

                platform_window_set_options(window, show);
            }
            const char* down = key & PLATFORM_KEY_PRESSED_BIT ? "down" : "up";


            printf("ENT %s %d\n", down, key & ~PLATFORM_KEY_PRESSED_BIT);
        }
        
        key = new_input.mouse_button_half_transitions[PLATFORM_MOUSE_BUTTON_LEFT];
        if((key & ~PLATFORM_KEY_PRESSED_BIT) > 0)
        {
            const char* down = key & PLATFORM_KEY_PRESSED_BIT ? "down" : "up";
            printf("BT4 %s %d\n", down, key & ~PLATFORM_KEY_PRESSED_BIT);
        }

        platform_window_swap_buffers(window);
        //SwapBuffers(hDC);               // Swap Buffers (Double Buffering)
        input = new_input;
    }

    platform_window_deinit(window);
    return 0;    
}