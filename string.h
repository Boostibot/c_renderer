#ifndef LIB_STRING
#define LIB_STRING

#include "allocator.h"
#include "array.h"

typedef struct String
{
    const char* data;
    isize size;
} String;

DEFINE_ARRAY_TYPE(char, String_Builder);
DEFINE_ARRAY_TYPE(String, String_Array);
DEFINE_ARRAY_TYPE(String_Builder, String_Builder_Array);

//Constructs a String out of a string literal. Cannot be used with dynamic strings!
#define STRING(cstring) BRACE_INIT(String){cstring, sizeof(cstring) - 1}

//if the string is valid -> returns it
//if the string is NULL  -> returns ""
EXPORT const char*      cstring_escape(const char* string);
//Returns always null terminated string contained within a builder
EXPORT const char*      cstring_from_builder(String_Builder builder); 

//Returns a String contained within string builder. The data portion of the string MIGHT be null and in that case its size == 0
EXPORT String string_from_builder(String_Builder builder); 
EXPORT String string_make(const char* cstring); //converts a null terminated cstring into a String
EXPORT String string_head(String string, isize to); //keeps only charcters to to ( [0, to) interval )
EXPORT String string_tail(String string, isize from); //keeps only charcters from from ( [from, string.size) interval )
EXPORT String string_limit(String string, isize max_size); //if the string is longer than max_size trims it to max_size else returns it unchanged 
EXPORT String string_range(String string, isize from, isize to); //returns a string containing characters staring from from and ending in to ( [from, to) interval )
EXPORT String string_portion(String string, isize from, isize size); //returns a string containing size characters staring from ( [from, from + size) interval )
EXPORT bool   string_is_prefixed_with(String string, String prefix); 
EXPORT bool   string_is_postfixed_with(String string, String postfix);
EXPORT bool   string_is_equal(String a, String b); //Returns true if the contents and sizes of the strings match
EXPORT bool   string_is_equal_at(String larger_string, isize from_index, String smaller_string);
EXPORT int    string_compare(String a, String b); //Compares sizes and then lexographically the contents. Shorter strings are placed before longer ones.
EXPORT int    string_compare_at(String larger_string, isize from_index, String smaller_string);
EXPORT isize  string_find_first(String string, String search_for, isize from); 
EXPORT isize  string_find_last_from(String in_str, String search_for, isize from);
EXPORT isize  string_find_last(String string, String search_for); 

EXPORT isize  string_find_first_char(String string, char search_for, isize from); 
EXPORT isize  string_find_last_from_char(String in_str, char search_for, isize from);
EXPORT isize  string_find_last_char(String string, char search_for); 

EXPORT void             builder_append(String_Builder* builder, String string); //Appends a string
EXPORT String_Builder   builder_from_string(String string); //Allocates a String_Builder from String. The String_Builder needs to be deinit just line any other ???_Array type!
EXPORT String_Builder   builder_from_cstring(const char* cstring); //Allocates a String_Builder from cstring. The String_Builder needs to be deinit just line any other ???_Array type!
EXPORT String_Builder   builder_from_string_alloc(String string, Allocator* allocator);  //Allocates a String_Builder from String using an allocator. The String_Builder needs to be deinit just line any other ???_Array type!

EXPORT bool             builder_is_equal(String_Builder a, String_Builder b); //Returns true if the contents and sizes of the strings match
EXPORT int              builder_compare(String_Builder a, String_Builder b); //Compares sizes and then lexographically the contents. Shorter strings are placed before longer ones.

EXPORT void string_join_into(String_Builder* append_to, const String* strings, isize strings_count, String separator); //Appends all strings in the strings array to append_to
EXPORT void string_split_into(String_Array* append_to, String to_split, String split_by); //Splits the to_split string using split_by as a separator and appends the individual split parts into append_to

EXPORT String_Builder string_join(String a, String b); //Allocates a new String_Builder with the concatenated String's a and b
EXPORT String_Builder cstring_join(const char* a, const char* b); //Allocates a new String_Builder with the concatenated cstring's a and b
EXPORT String_Builder string_join_any(const String* strings, isize strings_count, String separator); 
EXPORT String_Array string_split(String to_split, String split_by);

#endif

#if (defined(LIB_ALL_IMPL) || defined(LIB_STRING_IMPL)) && !defined(LIB_STRING_HAS_IMPL)
#define LIB_STRING_HAS_IMPL

    #include <string.h>
    EXPORT String string_head(String string, isize to)
    {
        CHECK_BOUNDS(to, string.size + 1);
        String head = {string.data, to};
        return head;
    }

    EXPORT String string_tail(String string, isize from)
    {
        CHECK_BOUNDS(from, string.size + 1);
        String tail = {string.data + from, string.size - from};
        return tail;
    }

    EXPORT String string_limit(String string, isize max_size)
    {
        if(max_size < string.size)
            return string_head(string, max_size);
        else
            return string;
    }

    EXPORT String string_range(String string, isize from, isize to)
    {
        return string_tail(string_head(string, to), from);
    }

    EXPORT String string_portion(String string, isize from, isize size)
    {
        return string_head(string_tail(string, size), from);
    }

    EXPORT String string_make(const char* cstring)
    {
        String out = {cstring, 0};
        if(cstring != NULL)
            out.size = strlen(cstring);

        return out;
    }

    EXPORT isize string_find_first(String in_str, String search_for, isize from)
    {
        CHECK_BOUNDS(from, in_str.size);
        if(search_for.size == 0)
            return from;
        
        ASSERT(from >= 0);
        isize to = in_str.size - search_for.size + 1;
        for(isize i = from; i < to; i++)
        {
            bool found = true;
            for(isize j = 0; j < search_for.size; j++)
            {
                if(in_str.data[i + j] != search_for.data[j])
                {
                    found = false;
                    break;
                }
            }

            if(found)
                return i;
        };

        return -1;
    }
      
    EXPORT isize string_find_last_from(String in_str, String search_for, isize from)
    {
        CHECK_BOUNDS(from, in_str.size);
        if(search_for.size == 0)
            return from;

        ASSERT(false && "UNTESTED!");
        ASSERT(from >= 0);
        isize start = from;
        if(in_str.size - start < search_for.size)
            start = in_str.size - search_for.size;

        for(isize i = start; i-- > 0; )
        {
            bool found = true;
            for(isize j = 0; j < search_for.size; j++)
            {
                CHECK_BOUNDS(i + j, in_str.size);
                if(in_str.data[i + j] != search_for.data[j])
                {
                    found = false;
                    break;
                }
            }

            if(found)
                return i;
        };

        return -1;
    }

    EXPORT isize string_find_last(String in_str, String search_for)
    {
        isize from = MAX(in_str.size - 1, 0);
        return string_find_last_from(in_str, search_for, from);
    }
    
    EXPORT isize string_find_first_char(String string, char search_for, isize from)
    {
        ASSERT(from >= 0);
        for(isize i = from; i < string.size; i++)
            if(string.data[i] == search_for)
                return i;

        return -1;
    }

    EXPORT bool string_is_prefixed_with(String string, String prefix)
    {
        if(string.size < prefix.size)
            return false;

        String trimmed = string_head(string, prefix.size);
        return string_is_equal(trimmed, prefix);
    }

    EXPORT bool string_is_postfixed_with(String string, String postfix)
    {
        if(string.size < postfix.size)
            return false;

        String trimmed = string_tail(string, postfix.size);
        return string_is_equal(trimmed, postfix);
    }
    
    EXPORT int string_compare_at(String larger_string, isize from_index, String smaller_string)
    {
        ASSERT(from_index >= 0);
        isize rem_size = larger_string.size - from_index;
        isize diff = rem_size - smaller_string.size;
        if(diff != 0)
            return (int) diff;

        int res = memcmp(larger_string.data + from_index, smaller_string.data, rem_size);
        return res;
    }

    EXPORT int string_compare(String a, String b)
    {
        int res = string_compare_at(a, 0, b);
        return res;
    }
    
    EXPORT bool string_is_equal_at(String larger_string, isize from_index, String smaller_string)
    {
        bool eq = string_compare_at(larger_string, from_index, smaller_string) == 0;
        return eq;
    }

    EXPORT bool string_is_equal(String a, String b)
    {
        bool eq = string_compare(a, b) == 0;
        return eq;
    }

    EXPORT const char* cstring_escape(const char* string)
    {
        if(string == NULL)
            return "";
        else
            return string;
    }

    EXPORT const char* cstring_from_builder(String_Builder builder)
    {
        return cstring_escape(builder.data);
    }

    EXPORT String string_from_builder(String_Builder builder)
    {
        String out = {builder.data, builder.size};
        return out;
    }
    
    EXPORT void builder_append(String_Builder* builder, String string)
    {
        array_append(builder, string.data, string.size);
    }

    EXPORT String_Builder builder_from_string_alloc(String string, Allocator* allocator)
    {
        String_Builder builder = {allocator};
        array_append(&builder, string.data, string.size);
        return builder;
    }

    EXPORT String_Builder builder_from_string(String string)
    {
        return builder_from_string_alloc(string, allocator_get_default());
    }

    EXPORT String_Builder builder_from_cstring(const char* cstring)
    {
        return builder_from_string(string_make(cstring));
    }

    EXPORT bool builder_is_equal(String_Builder a, String_Builder b)
    {
        return string_is_equal(string_from_builder(a), string_from_builder(b));
    }
    
    EXPORT int builder_compare(String_Builder a, String_Builder b)
    {
        return string_compare(string_from_builder(a), string_from_builder(b));
    }

    EXPORT void string_join_into(String_Builder* append_to, const String* strings, isize strings_count, String separator)
    {
        if(strings_count == 0)
            return;

        isize size_sum = 0;
        for(isize i = 0; i < strings_count; i++)
            size_sum += strings[i].size;

        size_sum += separator.size * (strings_count - 1);

        array_reserve(append_to, append_to->size + size_sum);
        builder_append(append_to, strings[0]);

        for(isize i = 1; i < strings_count; i++)
        {
            builder_append(append_to, separator);
            builder_append(append_to, strings[i]);
        }
    }

    EXPORT void string_split_into(String_Array* parts, String to_split, String split_by)
    {
        isize from = 0;
        for(isize i = 0; i < to_split.size; i++)
        {
            isize to = string_find_first(to_split, split_by, from);
            if(to == -1)
            {
                String part = string_range(to_split, from, to_split.size);
                array_push(parts, part);
                break;
            }

            String part = string_range(to_split, from, to);
            array_push(parts, part);
            from = to + split_by.size;
        }
    }

    EXPORT String_Builder string_join(String a, String b)
    {
        String_Builder joined = {0};
        array_resize(&joined, a.size + b.size);
        ASSERT(joined.data != NULL);
        memcpy(joined.data, a.data, a.size);
        memcpy(joined.data + a.size, b.data, b.size);

        return joined;
    }

    EXPORT String_Builder cstring_join(const char* a, const char* b)
    {
        return string_join(string_make(a), string_make(b));
    }

    EXPORT String_Builder string_join_any(const String* strings, isize strings_count, String separator)
    {
        String_Builder builder = {0};
        string_join_into(&builder, strings, strings_count, separator);
        return builder;
    }
    EXPORT String_Array string_split(String to_split, String split_by)
    {
        String_Array parts = {0};
        string_split_into(&parts, to_split, split_by);
        return parts;
    }
#endif