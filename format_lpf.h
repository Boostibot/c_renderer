#pragma once

#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <assert.h>
#include <string.h>

typedef int32_t lpf_size;

typedef enum Lpf_Kind {
    LPF_KIND_ENTRY = 0,
    LPF_KIND_CONTINUATION,
    LPF_KIND_ESCAPED_CONTINUATION,
    LPF_KIND_COMMENT,
    LPF_KIND_ARRAY,
    LPF_KIND_MAP,
} Lpf_Kind;

typedef struct Lpf_Range {
    lpf_size from;
    lpf_size to;
} Lpf_Range;

typedef struct Lpf_Entry {
    Lpf_Range value_text;
    Lpf_Range type_text;

    lpf_size line_number;

    Lpf_Kind kind;
} Lpf_Entry;

typedef struct Lpf_Collection {
    Lpf_Range entries;
    Lpf_Range type_text;

    lpf_size parent_i1;
    lpf_size child_i1;
    lpf_size next_i1;

    lpf_size depth;
    lpf_size line_number;
    
    Lpf_Kind kind;
} Lpf_Collection;

typedef enum Lpf_Error_Type {
    LPF_ERROR_NONE = 0,
    LPF_ERROR_TOO_MANY_ARRAYS,
    LPF_ERROR_TOO_MANY_MAPS,
} Lpf_Error_Type;

typedef struct Lpf_Error {
    lpf_size at;
    Lpf_Error_Type type;
} Lpf_Error;

typedef void*(*Lpf_Reallocate)(lpf_size new_size, void* old_ptr, lpf_size old_size, void* context);

typedef struct Lpf_File {
    Lpf_Entry* entries;
    lpf_size entries_size;
    lpf_size entries_capacity;

    Lpf_Collection* collections;
    lpf_size collections_size;
    lpf_size collections_capacity;

    Lpf_Error* errors;
    lpf_size errors_size;
    lpf_size errors_capacity;

    void* realloc_context;
    Lpf_Reallocate realloc_func; 
} Lpf_File;

#define LPF_FLAG_IGNORE_TYPES 8
#define LPF_FLAG_SKIP_TYPE_CHECK 16
#define LPF_FLAG_KEEP_COMMENTS 1
#define LPF_FLAG_KEEP_ERRORS 2
#define LPF_FLAG_BREAK_ON_ERROR 4

#define LPF_VERSION_DEFAULT 0
#define LPF_VERSION_MIN 0
#define LPF_VERSION_MAX 0

lpf_size lpf_parse(Lpf_File* into_or_null, const char* lpf_source, lpf_size source_size, lpf_size flags);


lpf_size lpf_parse_line(Lpf_Collection* into_or_null, const char* lpf_source, lpf_size source_size, lpf_size flags)
{

}

bool lpf_is_alpha(char c)
{
    return ('a' <= c && c <= 'z') || ('A' <= c && c <= 'Z');
}

bool lpf_is_digit(char c)
{
    return ('0' <= c && c <= '9');
}

void lpf_push(void** ptr, lpf_size* size, lpf_size* capacity, lpf_size item_size, Lpf_Reallocate realloc_func, void* realloc_context)
{
    if(*size >= *capacity)
    {
        lpf_size new_cap = *size * 3/2;
        if(new_cap < 8)
            new_cap = 8;
    
        lpf_size new_byte_cap = new_cap * item_size;
        lpf_size old_byte_cap = *capacity * item_size;

        void* old_ptr = *ptr;
        void* new_ptr = realloc_func(new_byte_cap, old_ptr, old_byte_cap, realloc_context);
        assert(new_ptr != NULL);

        *ptr = new_ptr;
        *capacity = new_cap;
    }
    
    *size += 1;
}

lpf_size lpf_parse(Lpf_File* into, const char* lpf_source, lpf_size source_size, lpf_size flags)
{
    typedef lpf_size isize;

    const Lpf_Range null_range = {0, 0};
    isize entries_count = 0;
    
    isize collection_parrent_i1 = 0;
    isize collection_i1 = 0;
    isize depth = 0;
    isize line_number = 0;
    isize line_i = 0;
    bool next_is_escaped = false;
    for(isize source_i = 0; source_i < source_size; source_i += line_i + 1)
    {
        line_number += 1;
        Lpf_Range val_text = {-1, -1};
        Lpf_Range type_text = {0, 0};
        Lpf_Range type_text_collection = {0, 0};
        Lpf_Error error = {0};
        Lpf_Kind kind = LPF_KIND_ENTRY;

        bool is_continuation = false;
        bool is_comment = false;

        bool found_any_start_marker = false;

        bool has_array_start = false;
        bool has_array_end = false;
        
        bool has_map_start = false;
        bool has_map_end = false;

        bool set_next_escaped = false;

        bool is_within_type = false;

        //parse line start
        for(line_i = 0; line_i < source_size && lpf_source[line_i] != '\n'; line_i++)
        {
            char c = lpf_source[line_i];

            //parse type. Only keeps the very last type
            if(flags & LPF_FLAG_IGNORE_TYPES == 0)
            {
                if(lpf_is_alpha(c) || lpf_is_digit(c) || (c == '_'))
                {
                    if(is_within_type == false)
                    {
                        is_within_type = true;
                        type_text.from = line_i;
                    }
                }
                else
                {
                    if(is_within_type)
                    {
                        is_within_type = false;
                        type_text.to = line_i;
                    }
                }
            }

            switch(c)
            {
                //starter kinds
                case ':': val_text.from = line_i; break;
                case ',': val_text.from = line_i; is_continuation = true; break;
                case '#': val_text.from = line_i; is_comment = true; break;
                    
                //collection markers
                    
                case '{': 
                    type_text_collection = type_text; 
                    type_text = null_range;
                    has_map_start = true; break;
                case '}': 
                    has_map_end = true; break;
            }
            
            if(val_text.from > -1)
                break;

            if(c == '[' || c == '{')
            {
                Lpf_Collection collection = {0};
                collection.parent_i1 = collection_i1;
                collection.depth = depth;
                collection.line_number = line_number;
                collection.type_text = type_text;
                collection.kind = c == '[' ? LPF_KIND_ARRAY : LPF_KIND_MAP;
                collection.entries.from = into->entries_size;
                collection.entries.to = into->entries_size;

                lpf_push(
                    (void**) &into->collections, &into->collections_size, &into->collections_capacity, 
                    sizeof collection, into->realloc_func, into->realloc_context);

                into->collections[into->collections_size - 1] = collection;

                depth += 1;
                type_text = null_range;
                collection_i1 = into->collections_size;
            }

            if()
        }

        //If does not have any starter assume the whole line is a comment.
        if(val_text.from == -1)
        {
            val_text.from = 0;
            val_text.to = 0;
            is_comment = true;
        }
        //Else find the end of the line parsing it
        else
        {
            //https://graphics.stanford.edu/~seander/bithacks.html#ValueInWord
            #define haszero(v)          (((int64_t)(v) - 0x0101010101010101ULL) & ~(int64_t)(v) & 0x8080808080808080ULL)
            #define broadcast_byte(c)   0x0101010101010101ULL * (int64_t)(c)
            const int64_t new_line_mask = broadcast_byte('\n');

            isize furthest_comment_i = -1;
            isize furthest_escape_i = -1;
            for(; line_i < source_size && lpf_source[line_i] != '\n'; line_i++)
            {
                char c = lpf_source[line_i];
                if(c == '\\')
                {
                    set_next_escaped = true;
                    furthest_escape_i = line_i;
                }
                if(c == '#')
                    furthest_comment_i = line_i;
            }

            val_text.to = furthest_comment_i > furthest_escape_i ? furthest_comment_i : furthest_escape_i;
            if(val_text.to == -1)
               val_text.to = line_i; 
        }
        
        if(is_comment)
            kind = LPF_KIND_COMMENT;   
        else if(is_continuation)
        {
            if(next_is_escaped)
                kind = LPF_KIND_ESCAPED_CONTINUATION;
            else
                kind = LPF_KIND_CONTINUATION;
        }
        else
        {
            kind = LPF_KIND_ENTRY;
        }

        Lpf_Entry entry = {0};
        entry.value_text = val_text;
        entry.type_text = type_text;
        entry.kind = kind;
        entry.line_number = line_number;



        next_is_escaped = set_next_escaped;
    }
}