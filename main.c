#define LIB_ALL_IMPL
#include "platform.h"
#include "string.h"
#include "hash_table.h"

int main()
{
    Window* window = platform_window_init("My window");
    assert(window != NULL);
    platform_window_set_visibility(window, WINDOW_VISIBILITY_WINDOWED, true);

    Hash_Table64 hash_table = {0};

    Hash_Found64 found = hash_table64_insert(&hash_table, 164, 10);
    ASSERT(found.hash_index != -1);
    ASSERT(found.value == 10);
    ASSERT(hash_table.data[found.hash_index].value == 10);

    Hash_Found64 found2 = hash_table64_find(hash_table, 164);
    ASSERT(memcmp(&found, &found2, sizeof(found)) == 0);

    Input input = {0};
    while(input.should_quit == false)
    {
        Input new_input = platform_window_process_messages(window);
        
        if(new_input.mouse_relative_x != 0 || new_input.mouse_relative_y != 0)
            printf("REL mouse pos %d %d\n", (int) new_input.mouse_relative_x, (int) new_input.mouse_relative_y);
            
        //if(new_input.mouse_app_resolution_x != input.mouse_app_resolution_x 
            //|| new_input.mouse_app_resolution_y != input.mouse_app_resolution_y )
            //printf("ABS mouse pos %d %d\n", (int) new_input.mouse_app_resolution_x, (int) new_input.mouse_app_resolution_y);

        if(new_input.keys[0x41] & (1 << 7))
        {
            platform_window_make_popup(POPUP_STYLE_ERROR, "A", "A");
        }
        if(new_input.keys[0x42] & (1 << 7))
        {
            platform_window_make_popup(POPUP_STYLE_ERROR, "B", "B");
        }

        input = new_input;
    }

    platform_window_deinit(window);
    return 0;    
}

#include "array.h"
#include "time.h"
#include "string.h"
#include "format.h"

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
