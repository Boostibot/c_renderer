#pragma once

#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <assert.h>
#include <string.h>
#include <stdarg.h>

typedef int32_t lpf_size;

typedef enum Lpf_Kind {
    LPF_KIND_BLANK = 0,
    LPF_KIND_ENTRY,
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
    lpf_size first_child_i1;
    lpf_size last_child_i1;
    lpf_size next_i1;
    lpf_size prev_i1;
    
    lpf_size children_count;
    lpf_size line_number;
    lpf_size depth;
} Lpf_Collection;


typedef void*(*Lpf_Reallocate)(lpf_size new_size, void* old_ptr, lpf_size old_size, void* context);
typedef bool(*Lpf_Log)(Lpf_Error error, lpf_size line, lpf_size line_index, lpf_size data_offset, void* context, const char* format, va_list args);

typedef struct Lpf_File {
    Lpf_Entry* entries;
    lpf_size entries_size;
    lpf_size entries_capacity;

    Lpf_Collection* collections;
    lpf_size collections_size;
    lpf_size collections_capacity;

    void* realloc_context;
    Lpf_Reallocate realloc_func; 

    Lpf_Log log_func;
    void* log_context;
} Lpf_File;

#define LPF_FLAG_KEEP_COMMENTS 1
#define LPF_FLAG_KEEP_ERRORS 2
#define LPF_FLAG_BREAK_ON_ERROR 4

#define LPF_VERSION_DEFAULT 0
#define LPF_VERSION_MIN 0
#define LPF_VERSION_MAX 0

void lpf_file_deinit(Lpf_File* file)
{
    file->realloc_func(0, file->entries, file->entries_capacity*sizeof(Lpf_Entry), file->realloc_context);
    file->realloc_func(0, file->collections, file->collections_capacity*sizeof(Lpf_Collection), file->realloc_context);
}

const char* lpf_error_to_string(Lpf_Error error)
{
    switch(error)
    {
        default:
        case LPF_ERROR_NONE: return "LPF_ERROR_NONE";
        case LPF_ERROR_ENTRY_INVALID_CHAR_BEFORE_START: return "LPF_ERROR_ENTRY_INVALID_CHAR_BEFORE_START";

        case LPF_ERROR_ENTRY_MISSING_START: return "LPF_ERROR_ENTRY_MISSING_START";
        case LPF_ERROR_ENTRY_MULTIPLE_TYPES: return "LPF_ERROR_ENTRY_MULTIPLE_TYPES";
        case LPF_ERROR_ENTRY_CONTINUNATION_WITHOUT_START: return "LPF_ERROR_ENTRY_CONTINUNATION_WITHOUT_START";
        case LPF_ERROR_ENTRY_CONTINUNATION_HAS_LABEL: return "LPF_ERROR_ENTRY_CONTINUNATION_HAS_LABEL";
    
        case LPF_ERROR_COLLECTION_END_HAS_LABEL: return "LPF_ERROR_COLLECTION_END_HAS_LABEL";
        case LPF_ERROR_COLLECTION_CONTENT_AFTER_START: return "LPF_ERROR_COLLECTION_CONTENT_AFTER_START";
        case LPF_ERROR_COLLECTION_CONTENT_AFTER_END: return "LPF_ERROR_COLLECTION_CONTENT_AFTER_END";
        case LPF_ERROR_COLLECTION_TOO_MANY_ENDS: return "LPF_ERROR_COLLECTION_TOO_MANY_ENDS";
    }
}

lpf_size lpf_parse(Lpf_File* into, const char* lpf_source, lpf_size source_size, lpf_size flags);

bool lpf_log(Lpf_File* file, Lpf_Error error, lpf_size line, lpf_size line_index, lpf_size data_offset, const char* format, ...)
{
    bool run = false;
    if(file->log_func)
    {
        va_list args;
        va_start(args, format);
        run = file->log_func(error, line, line_index, data_offset, file->log_context, format, args);
        va_end(args);
    }

    return run;
}

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

lpf_size lpf_parse(Lpf_File* into, const char* lpf_source, lpf_size source_size, lpf_size flags)
{
    lpf_size collection_i1 = 0;
    lpf_size depth = 0;
    lpf_size line_number = 0;
    lpf_size source_i = 0;
    for(; source_i < source_size; source_i++)
    {
        line_number += 1;
        lpf_size line_source_pos = source_i;
        Lpf_Entry entry = {LPF_KIND_BLANK};
        Lpf_Range word_range = {0};

        //parse line start
        bool had_non_space = false;
        bool is_within_type = false;
        for(; source_i < source_size && lpf_source[source_i] != '\n'; source_i++)
        {
            char c = lpf_source[source_i];
            if(lpf_is_space(c) == false)
            {
                had_non_space = true;
                if(is_within_type == false)
                {
                    is_within_type = true;
                    word_range.from = source_i;
                }
            }
            else
            {
                if(is_within_type)
                {
                    is_within_type = false;
                    word_range.to = source_i;
                }
            }

            switch(c)
            {
                case ':': entry.kind = LPF_KIND_ENTRY; break;
                case ';': entry.kind = LPF_KIND_ESCAPED_CONTINUATION;  break;
                case ',': entry.kind = LPF_KIND_CONTINUATION;  break;
                case '#': entry.kind = LPF_KIND_COMMENT;  break;
                case '{': entry.kind = LPF_KIND_COLLECTION_START; break;
                case '}': entry.kind = LPF_KIND_COLLECTION_END; break;
            }
            
            if((entry.kind != LPF_KIND_BLANK && is_within_type) || word_range.to != 0)
            {
                word_range.to = source_i;
                if(entry.label_text.to == 0)
                    entry.label_text = word_range;
                else if (entry.type_text.to == 0)
                    entry.type_text = word_range;
                else
                {
                    entry.error = LPF_ERROR_ENTRY_MULTIPLE_TYPES;
                    goto loop_end;
                }

                word_range.to = 0;
            }

            if(entry.kind != LPF_KIND_BLANK)
                break;
        }
        
        //if was a blank line skip
        if(had_non_space == false)
        {
            entry.kind = LPF_KIND_BLANK;
            goto loop_end;
        }

        source_i += 1;
        switch(entry.kind)
        {

            case LPF_KIND_BLANK: {
                //Missing start
                entry.error = LPF_ERROR_ENTRY_MISSING_START;
                goto loop_end;
            }
            case LPF_KIND_ENTRY:
            case LPF_KIND_CONTINUATION:
            case LPF_KIND_ESCAPED_CONTINUATION: {
                
                //Else find the end of the line parsing it
                entry.value_text.from = source_i;
                lpf_size furthest_comment_i = -1;
                for(; source_i < source_size && lpf_source[source_i] != '\n'; source_i++)
                {
                    char c = lpf_source[source_i];
                    if(c == '#')
                        furthest_comment_i = source_i;
                }

                if(furthest_comment_i == -1)
                   entry.value_text.to = source_i;
                else
                {
                    entry.value_text.to = furthest_comment_i;
                    entry.comment_text.from = furthest_comment_i + 1;
                    entry.comment_text.to = source_i;
                }
            }
            break;

            case LPF_KIND_COMMENT: {
                if(entry.label_text.to != 0 || entry.type_text.to != 0)
                {
                    entry.error = LPF_ERROR_COLLECTION_END_HAS_LABEL;
                    goto loop_end;
                }
                
                entry.comment_text.from = source_i;
                for(; source_i < source_size && lpf_source[source_i] != '\n'; source_i++);
                entry.comment_text.to = source_i; 
            }
            break;

            case LPF_KIND_COLLECTION_START: 
            case LPF_KIND_COLLECTION_END: {
                if(entry.kind == LPF_KIND_COLLECTION_END)
                {
                    if(entry.label_text.to != 0 || entry.type_text.to != 0)
                    {
                        entry.error = LPF_ERROR_COLLECTION_END_HAS_LABEL;
                        goto loop_end;
                    }
                }

                lpf_size furthest_comment_i = -1;
                for(; source_i < source_size && lpf_source[source_i] != '\n'; source_i++)
                {
                    char c = lpf_source[source_i];
                    if(c == '#')
                        furthest_comment_i = source_i;
                    else if(lpf_is_space(c) == false && furthest_comment_i == -1)
                    {
                        if(entry.kind == LPF_KIND_COLLECTION_START)
                            entry.error = LPF_ERROR_COLLECTION_CONTENT_AFTER_START;
                        else
                            entry.error = LPF_ERROR_COLLECTION_CONTENT_AFTER_END;
                        goto loop_end;
                    }
                }

                if(furthest_comment_i != -1)
                {
                    entry.comment_text.from = furthest_comment_i;
                    entry.comment_text.to = source_i;
                }

                if(entry.kind == LPF_KIND_COLLECTION_START)
                {
                    assert(into->collections_size > 0);
                    assert(0 < collection_i1 && collection_i1 <= into->collections_size);
                    
                    lpf_push((void**) &into->collections, &into->collections_size, &into->collections_capacity, 
                        sizeof(Lpf_Collection), into->realloc_func, into->realloc_context);

                    lpf_size parent_i1 = collection_i1;
                    Lpf_Collection* parent = &into->collections[parent_i1 - 1];

                    lpf_size curr_i1 = into->collections_size;
                    Lpf_Collection* curr = &into->collections[curr_i1 - 1];

                    curr->parent_i1 = parent_i1;
                    curr->depth = parent->depth + 1;
                    curr->line_number = line_number;
                    curr->type_text = entry.type_text;
                    curr->label_text = entry.label_text;
                    curr->entries.from = into->entries_size;
                    curr->entries.to = into->entries_size;

                    if(parent->last_child_i1 == 0)
                        parent->first_child_i1 = curr_i1;
                    else
                    {
                        lpf_size prev_i1 = parent->last_child_i1;
                        Lpf_Collection* prev = &into->collections[prev_i1 - 1];
                        prev->next_i1 = curr_i1;
                        curr->prev_i1 = prev_i1;
                    }

                    parent->last_child_i1 = curr_i1;
                    parent->children_count += 1;
                    
                    depth += 1;
                    collection_i1 = curr_i1;
                }
                else
                {
                    assert(0 < collection_i1 && collection_i1 <= into->collections_size);
                    lpf_size curr_i1 = collection_i1;
                    Lpf_Collection* curr = &into->collections[curr_i1 - 1];

                    if(curr->parent_i1 <= 0)
                    {
                        entry.error = LPF_ERROR_COLLECTION_TOO_MANY_ENDS;
                        goto loop_end;
                    }

                    curr->entries.to = into->entries_size;
                    collection_i1 = curr->parent_i1;
                    depth -= 1;
                }
            }
            break;
            
            default: assert(false);
        }


        loop_end:
        if(entry.error != LPF_ERROR_NONE)
        {
            bool continune_running = lpf_log(into, LPF_ERROR_ENTRY_MULTIPLE_TYPES, line_number, line_source_pos, source_i, "@TODO");
            if(continune_running == false)
                break;
        
            for(; source_i < source_size && lpf_source[source_i] != '\n'; source_i++);
            
            if((flags & LPF_FLAG_KEEP_ERRORS) == 0)
                continue;
        }

        if(entry.kind != LPF_KIND_COMMENT || (flags & LPF_KIND_COMMENT))
        {
            lpf_push((void**) &into->entries, &into->entries_size, &into->entries_capacity, 
                sizeof entry, into->realloc_func, into->realloc_context);
            into->entries[into->entries_size - 1] = entry;
        }
    }

    if(source_i > source_size)
        source_i = source_size;
    return source_i;
}


void* lpf_default_realloc(lpf_size new_size, void* old_ptr, lpf_size old_size, void* context)
{
    (void) old_size;
    (void) context;

    if(new_size == 0)
    {
        free(old_ptr);
        return NULL;
    }
    else
    {
        return realloc(old_ptr, new_size);
    }
}

#include <stdio.h>
bool lpf_default_log(Lpf_Error error, lpf_size line, lpf_size line_index, lpf_size data_offset, void* context, const char* format, va_list args)
{
    (void) context;
    printf("Had error %s at line: %d char: %d (%d) with message: ", lpf_error_to_string(error), (int) line, (int) line_index, (int) data_offset);
    vprintf(format, args);
    return false;
}
