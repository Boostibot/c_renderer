#include "c_string.h"

String string_head(String string, isize to)
{
    CHECK_BOUNDS(to, string.size + 1);
    String head = {string.data, to};
    return head;
}

String string_tail(String string, isize from)
{
    CHECK_BOUNDS(from, string.size + 1);
    String tail = {string.data + from, string.size - from};
    return tail;
}

String string_limit(String string, isize max_size)
{
    if(max_size < string.size)
        return string_head(string, max_size);
    else
        return string;
}

String string_range(String string, isize from, isize to)
{
    return string_tail(string_head(string, to), from);
}

String string_portion(String string, isize from, isize size)
{
    return string_head(string_tail(string, size), from);
}

String string_make(const char* cstring)
{
    String out = {cstring, 0};
    if(cstring != NULL)
        out.size = strlen(cstring);

    return out;
}

isize string_find_first(String in_str, String search_for, isize from)
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
      
isize string_find_last_from(String in_str, String search_for, isize from)
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

isize string_find_last(String in_str, String search_for)
{
    isize from = MAX(in_str.size - 1, 0);
    return string_find_last_from(in_str, search_for, from);
}
    
bool string_is_prefixed_with(String string, String prefix)
{
    if(string.size < prefix.size)
        return false;

    String trimmed = string_head(string, prefix.size);
    return string_is_equal(trimmed, prefix);
}

bool string_is_postfixed_with(String string, String postfix)
{
    if(string.size < postfix.size)
        return false;

    String trimmed = string_tail(string, postfix.size);
    return string_is_equal(trimmed, postfix);
}

int string_compare(String a, String b)
{
    isize diff = a.size - b.size;
    if(diff != 0)
        return (int) diff;

    int res = memcmp(a.data, b.data, a.size);
    return res;
}

bool string_is_equal(String a, String b)
{
    bool eq = string_compare(a, b) == 0;
    return eq;
}

const char* builder_cstring(String_Builder builder)
{
    if(builder.data == NULL)
        return "";
    else
        return builder.data;
}

String builder_string(String_Builder builder)
{
    String out = {builder.data, builder.size};
    return out;
}
    
void builder_append(String_Builder* builder, String string)
{
    array_append(builder, string.data, string.size);
}

String_Builder builder_from_string_alloc(String string, Allocator* allocator)
{
    String_Builder builder = {allocator};
    array_assign(&builder, string.data, string.size);
    return builder;
}

String_Builder builder_from_string(String string)
{
    return builder_from_string_alloc(string, allocator_get_default());
}

String_Builder builder_from_cstring(const char* cstring)
{
    return builder_from_string(string_make(cstring));
}

void string_join_into(String_Builder* append_to, const String* strings, isize strings_count, String separator)
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

void string_split_into(String_Array* parts, String to_split, String split_by)
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

String_Builder string_join(String a, String b)
{
    String_Builder joined = {0};
    array_resize(&joined, a.size + b.size);
    ASSERT(joined.data != NULL);
    memcpy(joined.data, a.data, a.size);
    memcpy(joined.data + a.size, b.data, b.size);

    return joined;
}

String_Builder cstring_join(const char* a, const char* b)
{
    return string_join(string_make(a), string_make(b));
}

String_Builder string_join_any(const String* strings, isize strings_count, String separator)
{
    String_Builder builder = {0};
    string_join_into(&builder, strings, strings_count, separator);
    return builder;
}
String_Array string_split(String to_split, String split_by)
{
    String_Array parts = {0};
    string_split_into(&parts, to_split, split_by);
    return parts;
}
