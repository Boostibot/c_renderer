#ifndef LIB_FORMAT
#define LIB_FORMAT

#include "string.h"
#include <stdarg.h>

EXPORT void vformat_into(String_Builder* append_to, const char* format, va_list args);
EXPORT void vformat_into_sized(String_Builder* append_to, String format, va_list args);

EXPORT void format_into(String_Builder* append_to, const char* format, ...);
EXPORT void format_into_sized(String_Builder* append_to, String format, ...);

#define STR_FMT "%.*s"
#define STR_PRINT(string) (string).size, (string).data

#define SOURCE_INFO_FMT "( %s : %lld )"
#define SOURCE_INFO_PRINT(source_info) (source_info).file != NULL ? (source_info).file : "", (source_info).line

#endif // !LIB_FORMAT

#if (defined(LIB_ALL_IMPL) || defined(LIB_FORMAT_IMPL)) && !defined(LIB_FORMAT_HAS_IMPL)
#define LIB_FORMAT_HAS_IMPL

    #define _CRT_SECURE_NO_WARNINGS
    #include <stdio.h>

    EXPORT void vformat_into(String_Builder* append_to, const char* format, va_list args)
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
    EXPORT void vformat_into_sized(String_Builder* append_to, String format, va_list args)
    {
        String_Builder escaped = {0};
        array_init_backed(&escaped, allocator_get_scratch(), 1024);
        builder_append(&escaped, format);
    
        vformat_into(append_to, builder_cstring(escaped), args);

        array_deinit(&escaped);
    }

    EXPORT void format_into(String_Builder* append_to, const char* format, ...)
    {
        va_list args;
        va_start(args, format);
        vformat_into(append_to, format, args);
        va_end(args);
    }

    EXPORT void format_into_sized(String_Builder* append_to, String format, ...)
    {
        va_list args;
        va_start(args, format);
        vformat_into_sized(append_to, format, args);
        va_end(args);
    }

#endif