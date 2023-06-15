#include "c_array.h"
#include "c_time.h"
#include "c_string.h"
#include "c_format.h"

#include <stdio.h>

int main()
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

    allocator_reallocate(allocator_get_default(), NULL, 1000000000000, 0, 8, SOURCE_INFO());
}
