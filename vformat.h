#ifndef LIB_VFORMAT
#define LIB_VFORMAT

#include "string.h"
#include "profile.h"
#include <stdarg.h>

EXPORT void vformat_append_into(String_Builder* append_to, const char* format, va_list args);
EXPORT void vformat_append_into_sized(String_Builder* append_to, String format, va_list args);

EXPORT void format_append_into(String_Builder* append_to, const char* format, ...);
EXPORT void format_append_into_sized(String_Builder* append_to, String format, ...);

EXPORT void vformat_into(String_Builder* into, const char* format, va_list args);
EXPORT void vformat_into_sized(String_Builder* into, String format, va_list args);

EXPORT void format_into(String_Builder* into, const char* format, ...);
EXPORT void format_into_sized(String_Builder* into, String format, ...);

#define CSTRING_ESCAPE(s) (s) == NULL ? "" : (s)

#define STRING_FMT "%.*s"
#define STRING_PRINT(string) (string).size, (string).data

#define SOURCE_INFO_FMT "( %s : %lld )"
#define SOURCE_INFO_PRINT(source_info) cstring_escape((source_info).file), (source_info).line

#define STACK_TRACE_FMT "%-30s " SOURCE_INFO_FMT
#define STACK_TRACE_PRINT(trace) cstring_escape((trace).function), SOURCE_INFO_PRINT(trace)

typedef long long int lld;
#endif // !LIB_VFORMAT

#if (defined(LIB_ALL_IMPL) || defined(LIB_VFORMAT_IMPL)) && !defined(LIB_VFORMAT_HAS_IMPL)
#define LIB_VFORMAT_HAS_IMPL
    #include <stdio.h>

    EXPORT void vformat_append_into(String_Builder* append_to, const char* format, va_list args)
    {
        PERF_COUNTER_START(c);
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
            count = vsnprintf(append_to->data + base_size, (size_t) (append_to->size - base_size), format, args);
        }
    
        //if(count != 0)
            //ASSERT(append_to->data[base_size + count - 1] != '\0');

        array_resize(append_to, base_size + count);
        
        PERF_COUNTER_END(c);
        return;
    }

    EXPORT void vformat_append_into_sized(String_Builder* append_to, String format, va_list args)
    {
        String_Builder escaped = {0};
        array_init_backed(&escaped, allocator_get_scratch(), 1024);
        builder_append(&escaped, format);
    
        vformat_append_into(append_to, cstring_from_builder(escaped), args);

        array_deinit(&escaped);
    }

    EXPORT void format_append_into(String_Builder* append_to, const char* format, ...)
    {
        va_list args;
        va_start(args, format);
        vformat_append_into(append_to, format, args);
        va_end(args);
    }

    EXPORT void format_append_into_sized(String_Builder* append_to, String format, ...)
    {
        va_list args;
        va_start(args, format);
        vformat_append_into_sized(append_to, format, args);
        va_end(args);
    }
    
    EXPORT void vformat_into(String_Builder* into, const char* format, va_list args)
    {
        array_clear(into);
        vformat_append_into(into, format, args);
    }

    EXPORT void vformat_into_sized(String_Builder* into, String format, va_list args)
    {
        array_clear(into);
        vformat_append_into_sized(into, format, args);
    }

    EXPORT void format_into(String_Builder* into, const char* format, ...)
    {
        va_list args;
        va_start(args, format);
        vformat_into(into, format, args);
        va_end(args);
    }

    EXPORT void format_into_sized(String_Builder* into, String format, ...)
    {
        va_list args;
        va_start(args, format);
        vformat_into_sized(into, format, args);
        va_end(args);
    }

#endif