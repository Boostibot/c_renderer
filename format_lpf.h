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
    LPF_KIND_COLLECTION_START,
    LPF_KIND_COLLECTION_END,
} Lpf_Kind;

typedef enum Lpf_Error {
    LPF_ERROR_NONE = 0,
    LPF_ERROR_ENTRY_INVALID_CHAR_BEFORE_START,

    LPF_ERROR_ENTRY_MISSING_START,
    LPF_ERROR_ENTRY_MULTIPLE_TYPES,
    LPF_ERROR_ENTRY_CONTINUNATION_WITHOUT_START,
    LPF_ERROR_ENTRY_CONTINUNATION_HAS_LABEL,
    
    LPF_ERROR_COLLECTION_END_HAS_LABEL,
    LPF_ERROR_COLLECTION_CONTENT_AFTER_START,
    LPF_ERROR_COLLECTION_CONTENT_AFTER_END,
    LPF_ERROR_COLLECTION_TOO_MANY_ENDS,
} Lpf_Error;

typedef struct Lpf_Range {
    lpf_size from;
    lpf_size to;
} Lpf_Range;

typedef struct Lpf_Entry {
    Lpf_Range label_text;
    Lpf_Range type_text;
    Lpf_Range value_text;
    Lpf_Range comment_text;

    lpf_size line_number;
    lpf_size collection_i1;
    Lpf_Kind kind;
    Lpf_Error error;
} Lpf_Entry;

typedef struct Lpf_Collection {
    Lpf_Range entries;
    Lpf_Range label_text;
    Lpf_Range type_text;

    lpf_size parent_i1;
    lpf_size child_i1;
    lpf_size next_i1;
    
    lpf_size line_number;
    lpf_size depth;
} Lpf_Collection;


typedef void*(*Lpf_Reallocate)(lpf_size new_size, void* old_ptr, lpf_size old_size, void* context);
typedef bool(*Lpf_Log)(Lpf_Error error, lpf_size line, lpf_size line_index, lpf_size data_offset, const char* format, ...);

typedef struct Lpf_File {
    Lpf_Entry* entries;
    lpf_size entries_size;
    lpf_size entries_capacity;

    Lpf_Collection* collections;
    lpf_size collections_size;
    lpf_size collections_capacity;

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

lpf_size lpf_parse(Lpf_File* into, const char* lpf_source, lpf_size source_size, lpf_size flags, Lpf_Log log);

bool lpf_is_space(char c)
{
    switch(c)
    {
        case ' ':
        case '\n':
        case '\t':
        case '\r':
        case '\v':
        case '\f':
            return true;
        default: 
            return false;
    }
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


lpf_size lpf_parse(Lpf_File* into, const char* lpf_source, lpf_size source_size, lpf_size flags, Lpf_Log log)
{
    typedef lpf_size isize;

    const Lpf_Range null_range = {0, 0};
    isize entries_count = 0;
    
    isize collection_parrent_i1 = 0;
    isize collection_i1 = 0;
    isize depth = 0;
    isize line_number = 0;
    isize line_i = 0;
    bool run = true;
    for(isize source_i = 0; source_i < source_size; source_i += line_i + 1)
    {
        line_number += 1;
        Lpf_Kind kind = (Lpf_Kind) -1;
        Lpf_Entry entry = {0};
        Lpf_Range word_range = {0};


        //parse line start
        bool is_within_type = false;
        for(line_i = 0; line_i < source_size && lpf_source[line_i] != '\n'; line_i++)
        {
            char c = lpf_source[line_i];
            if(lpf_is_space(c) == false)
            {
                if(is_within_type == false)
                {
                    is_within_type = true;
                    word_range.from = line_i;
                }
            }
            else
            {
                if(is_within_type)
                {
                    is_within_type = false;
                    word_range.to = line_i;
                }
            }

            switch(c)
            {
                case ':': kind = LPF_KIND_ENTRY; break;
                case ';': kind = LPF_KIND_ESCAPED_CONTINUATION;  break;
                case ',': kind = LPF_KIND_CONTINUATION;  break;
                case '#': kind = LPF_KIND_COMMENT;  break;
                case '{': kind = LPF_KIND_COLLECTION_START; break;
                case '}': kind = LPF_KIND_COLLECTION_END; break;
            }
            
            if((kind != -1 && is_within_type) || word_range.to != 0)
            {
                word_range.to = line_i;
                if(entry.label_text.to == 0)
                    entry.label_text = word_range;
                else if (entry.type_text.to == 0)
                    entry.type_text = word_range;
                else
                {
                    if(log) run = log(LPF_ERROR_ENTRY_MULTIPLE_TYPES, line_number, line_i, source_i, "@TODO");
                    goto had_error;
                }

                word_range.to = 0;
            }

            if(kind != -1)
                break;
        }
        
        //Missing start
        if(kind == (Lpf_Kind) -1)
        {
            if(log) run = log(LPF_ERROR_ENTRY_MISSING_START, line_number, line_i, source_i, "@TODO");
            goto had_error;
        }
        
        entry.kind = kind;
        switch(kind)
        {
            case LPF_KIND_ENTRY:
            case LPF_KIND_CONTINUATION:
            case LPF_KIND_ESCAPED_CONTINUATION: {
                
                //Else find the end of the line parsing it
                entry.value_text.from = line_i;
                isize furthest_comment_i = -1;
                for(; line_i < source_size && lpf_source[line_i] != '\n'; line_i++)
                {
                    char c = lpf_source[line_i];
                    if(c == '#')
                        furthest_comment_i = line_i;
                }

                if(furthest_comment_i == -1)
                   entry.value_text.to = line_i;
                else
                {
                    entry.value_text.to = furthest_comment_i;
                    entry.comment_text.from = furthest_comment_i + 1;
                    entry.comment_text.to = line_i + 1;
                }
            }
            break;

            case LPF_KIND_COMMENT: {
                if(entry.label_text.to != 0 || entry.type_text.to != 0)
                {
                    if(log) run = log(LPF_ERROR_COLLECTION_END_HAS_LABEL, line_number, line_i, source_i, "@TODO");
                    goto had_error;
                }
                
                entry.comment_text.from = line_i;
                for(; line_i < source_size && lpf_source[line_i] != '\n'; line_i++);
                entry.comment_text.to = line_i; 
            }
            break;

            case LPF_KIND_COLLECTION_START: 
            case LPF_KIND_COLLECTION_END: {
                if(kind == LPF_ERROR_COLLECTION_END_HAS_LABEL)
                {
                    if(entry.label_text.to != 0 || entry.type_text.to != 0)
                    {
                        if(log) run = log(LPF_ERROR_COLLECTION_END_HAS_LABEL, line_number, line_i, source_i, "@TODO");
                        goto had_error;
                    }
                }

                line_i += 1;
                for(; line_i < source_size && lpf_source[line_i] != '\n'; line_i++)
                {
                    char c = lpf_source[line_i];
                    if(lpf_is_space(c) == false)
                    {
                        Lpf_Error error = kind == LPF_KIND_COLLECTION_END ? LPF_ERROR_COLLECTION_CONTENT_AFTER_END : LPF_ERROR_COLLECTION_CONTENT_AFTER_START;
                        if(log) run = log(error, line_number, line_i, source_i, "@TODO");
                        goto had_error;
                    }
                }

                if(kind == LPF_KIND_COLLECTION_START)
                {
                    assert(into->collections_size > 0);
                    Lpf_Collection* prev_collection = &into->collections[collection_i1 - 1];

                    Lpf_Collection collection = {0};
                    collection.parent_i1 = collection_parrent_i1;
                    collection.depth = depth;
                    collection.line_number = line_number;
                    collection.type_text = entry.type_text;
                    collection.label_text = entry.label_text;
                    collection.entries.from = into->entries_size;
                    collection.entries.to = -1;

                    lpf_push((void**) &into->collections, &into->collections_size, &into->collections_capacity, 
                        sizeof collection, into->realloc_func, into->realloc_context);
                    into->collections[into->collections_size - 1] = collection;

                    depth += 1;
                    prev_collection->next_i1 = into->collections_size;
                    collection_parrent_i1 = into->collections_size;
                }
                else
                {
                    if(into->collections_size <= 0)
                    {
                        if(log) run = log(LPF_ERROR_COLLECTION_END_HAS_LABEL, line_number, line_i, source_i, "@TODO");
                        goto had_error;
                    }
                    Lpf_Collection* prev_collection = &into->collections[into->collections_size - 1];
                    prev_collection->entries.to = into->entries_size;
                    collection_parrent_i1 = prev_collection->parent_i1;
                    depth -= 1;
                }
            }
            break;
            
            default: assert(false);
        }


        lpf_push((void**) &into->entries, &into->entries_size, &into->entries_capacity, 
            sizeof entry, into->realloc_func, into->realloc_context);
        into->entries[into->entries_size - 1] = entry;
        continue;

        had_error:
        if(run == false)
            break;
    }
}