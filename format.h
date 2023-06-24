#pragma once

#include "string.h"
#include <stdio.h>
#include <stdarg.h>

static void vformat_into(String_Builder* append_to, const char* format, va_list args);
static void vformat_into_sized(String_Builder* append_to, String format, va_list args);

static void format_into(String_Builder* append_to, const char* format, ...);
static void format_into_sized(String_Builder* append_to, String format, ...);
static void formatln_into(String_Builder* append_to, const char* format, ...);
static void formatln_into_sized(String_Builder* append_to, String format, ...);

static void vformat_into(String_Builder* append_to, const char* format, va_list args)
{
    //an attempt to estimate the needed size so we dont need to call vsnprintf twice
    isize format_size = strlen(format);
    isize estimated_size = format_size + 10 + format_size/4;
    isize base_size = append_to->size; 
    array_resize(append_to, base_size + estimated_size);

    va_list args_copy;
    va_copy(args_copy, args);
    isize count = vsnprintf(append_to->data + base_size, (size_t) (append_to->size - base_size), format, args);
        
    if(count > estimated_size)
    {
        array_resize(append_to, base_size + count + 3);
        isize count = vsnprintf(append_to->data + base_size, (size_t) (append_to->size - base_size), format, args);
    }
    
    if(count != 0)
        ASSERT(append_to->data[base_size + count - 1] != '\0');

    array_resize(append_to, base_size + count);
    return;
}
static void vformat_into_sized(String_Builder* append_to, String format, va_list args)
{
    char backing[1024] = {0};

    String_Builder escaped = {0};
    array_init_backed(&escaped, allocator_get_scratch(), backing, sizeof(backing));
    builder_append(&escaped, format);
    
    vformat_into(append_to, builder_cstring(escaped), args);

    array_deinit(&escaped);
}

static void format_into(String_Builder* append_to, const char* format, ...)
{
    va_list args;
    va_start(args, format);
    vformat_into(append_to, format, args);
    va_end(args);
}

static void format_into_sized(String_Builder* append_to, String format, ...)
{
    va_list args;
    va_start(args, format);
    vformat_into_sized(append_to, format, args);
    va_end(args);
}

//@TODO remove
static void formatln_into(String_Builder* append_to, const char* format, ...)
{
    va_list args;
    va_start(args, format);
    vformat_into(append_to, format, args);
    va_end(args);
    array_push(append_to, '\n');
}
static void formatln_into_sized(String_Builder* append_to, String format, ...)
{
    va_list args;
    va_start(args, format);
    vformat_into_sized(append_to, format, args);
    va_end(args);
    array_push(append_to, '\n');
}

//@TEMP: This will eventually be replaced by proper logging
static void print(const char* format, ...)
{
    va_list args;
    va_start(args, format);
    vprintf(format, args);
    va_end(args);
}
static void println(const char* format, ...)
{
    va_list args;
    va_start(args, format);
    vprintf(format, args);
    va_end(args);
    putchar('\n');
}
static void print_string(String string)
{
    fwrite(string.data, 1, string.size, stdout);
}
static void print_builder(String_Builder string)
{
    print_string(builder_string(string));
}
static void print_cstring(const char* cstring)
{
    fputs(cstring, stdout);
}
