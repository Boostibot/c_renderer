#pragma once

#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <assert.h>
#include <string.h>
#include <stdarg.h>

#include "string.h"
#include "parse.h"

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
    isize from;
    isize to;
} Lpf_Range;

typedef struct Lpf_Entry {
    String label;
    String type;
    String value;
    String comment;

    isize line_number;
    isize collection_i1;
    Lpf_Kind kind;
    Lpf_Error error;
} Lpf_Entry;

typedef struct Lpf_Collection {
    Lpf_Range entries;
    Lpf_Range label;
    Lpf_Range type;

    isize parent_i1;
    isize first_child_i1;
    isize last_child_i1;
    isize next_i1;
    isize prev_i1;
    
    isize children_count;
    isize line_number;
    isize depth;
} Lpf_Collection;

isize lpf_parse_line(const char* lpf_source, isize from, isize to, Lpf_Entry* entry);
isize lpf_write_line(char* write_to, isize from, isize to, Lpf_Entry entry);

typedef struct Lpf_Write_Options {
    isize line_indentation;
    isize pad_prefix_to;
    isize max_line_size;

    //bool separate_type_and_marker;
    //bool separate_marker_and_value;

    //bool separate_type_and_collection;
    //bool separate_collection_and_value;
    //bool separate_comment;

    bool put_space_before_marker;
    bool put_space_before_value;
    bool put_space_before_comment;
    
    bool comment_terminate_line;
    bool pad_continuations;
} Lpf_Write_Options;

/*
isize lpf_parse_line(const char* lpf_source, isize from, isize to, Lpf_Entry* parsed)
{
    isize source_i = from;
    Lpf_Entry entry = {LPF_KIND_BLANK};
    Lpf_Range word_range = {0};

    //parse line start
    bool had_non_space = false;
    bool is_within_type = false;
    for(; source_i < to && lpf_source[source_i] != '\n'; source_i++)
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
            if(entry.label.to == 0)
                entry.label = word_range;
            else if (entry.type.to == 0)
                entry.type = word_range;
            else
            {
                entry.error = LPF_ERROR_ENTRY_MULTIPLE_TYPES;
                goto line_end;
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
        goto line_end;
    }

    source_i += 1;
    switch(entry.kind)
    {
        case LPF_KIND_BLANK: {
            //Missing start
            entry.error = LPF_ERROR_ENTRY_MISSING_START;
            goto line_end;
        }
        case LPF_KIND_ENTRY:
        case LPF_KIND_CONTINUATION:
        case LPF_KIND_ESCAPED_CONTINUATION: {
                
            //Else find the end of the line parsing it
            entry.value.from = source_i;
            isize furthest_comment_i = -1;
            for(; source_i < to && lpf_source[source_i] != '\n'; source_i++)
            {
                char c = lpf_source[source_i];
                if(c == '#')
                    furthest_comment_i = source_i;
            }

            if(furthest_comment_i == -1)
                entry.value.to = source_i;
            else
            {
                entry.value.to = furthest_comment_i;
                entry.comment.from = furthest_comment_i + 1;
                entry.comment.to = source_i;
            }
        }
        break;

        case LPF_KIND_COMMENT: {
            if(entry.label.to != 0 || entry.type.to != 0)
            {
                entry.error = LPF_ERROR_COLLECTION_END_HAS_LABEL;
                goto line_end;
            }
                
            entry.comment.from = source_i;
            for(; source_i < to && lpf_source[source_i] != '\n'; source_i++);
            entry.comment.to = source_i; 
        }
        break;

        case LPF_KIND_COLLECTION_START: 
        case LPF_KIND_COLLECTION_END: {
            if(entry.kind == LPF_KIND_COLLECTION_END)
            {
                if(entry.label.to != 0 || entry.type.to != 0)
                {
                    entry.error = LPF_ERROR_COLLECTION_END_HAS_LABEL;
                    goto line_end;
                }
            }

            isize furthest_comment_i = -1;
            for(; source_i < to && lpf_source[source_i] != '\n'; source_i++)
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
                    goto line_end;
                }
            }

            if(furthest_comment_i != -1)
            {
                entry.comment.from = furthest_comment_i + 1;
                entry.comment.to = source_i;
            }

        }
        break;
            
        default: assert(false);
    }

    
    line_end:
    for(; source_i < to && lpf_source[source_i] != '\n'; source_i++);

    *parsed = entry;
    if(source_i > to)
        source_i = to;
    return source_i;
}
*/
void lpf_write_line_custom(String_Builder* builder, Lpf_Entry entry, const Lpf_Write_Options* options_or_null);
void builder_pad_to(String_Builder* builder, isize to_size, char with)
{
    if(builder->size >= to_size)
        return;

    isize size_before = array_resize(builder, to_size);
    for(isize i = size_before; i < builder->size; i++)
        builder->data[i] = with;
}

bool lpf_is_prefix_allowed_char(char c)
{
    if(char_is_space(c))
        return false;

    if(c == ':' || c == ',' || c == '#' ||c == ';' || c == '{' || c == '}')
        return false;

    return true;
}

isize lpf_write_line_unescaped(String_Builder* builder, Lpf_Entry entry, Lpf_Write_Options options)
{
    #ifdef DO_ASSERTS
        for(isize i = 0; i < entry.label.size; i++)
            ASSERT_MSG(lpf_is_prefix_allowed_char(entry.label.data[i]), 
                "label must contain only valid data! Label: " STRING_FMT, STRING_PRINT(entry.label));

        for(isize i = 0; i < entry.type.size; i++)
            ASSERT_MSG(lpf_is_prefix_allowed_char(entry.type.data[i]),
                "type must contain only valid data! Label: " STRING_FMT, STRING_PRINT(entry.type));

        ASSERT_MSG(string_find_first_char(entry.value, '\n', 0) == -1, "value must not contain newlines. Value: \""STRING_FMT"\"", STRING_PRINT(entry.value));
        ASSERT_MSG(string_find_first_char(entry.comment, '\n', 0) == -1, "comment must not contain newlines. Value: \""STRING_FMT"\"", STRING_PRINT(entry.comment));
    #endif

    char marker_char = '?';

    switch(entry.kind)
    {
        default:
        case LPF_KIND_BLANK: {
            builder_pad_to(builder, builder->size + options.line_indentation, ' ');
            array_push(builder, '\n');

            return 0;
        } break;
        case LPF_KIND_COMMENT: {
            builder_pad_to(builder, builder->size + options.line_indentation, ' ');
            array_push(builder, '#');
            builder_append(builder, entry.comment);
            array_push(builder, '\n');

            return 0;
        } break;

        case LPF_KIND_ENTRY:                marker_char = ':'; break;
        case LPF_KIND_CONTINUATION:         marker_char = ','; break;
        case LPF_KIND_ESCAPED_CONTINUATION: marker_char = ';'; break;
        case LPF_KIND_COLLECTION_START:     marker_char = '{'; break;
        case LPF_KIND_COLLECTION_END:       marker_char = '}'; break;
    }

    builder_pad_to(builder, builder->size + options.line_indentation, ' ');

    isize size_before = builder->size;
    builder_append(builder, entry.label);
    if(entry.type.size > 0)
    {
        array_push(builder, ' ');
        builder_append(builder, entry.type);
    }

    builder_pad_to(builder, size_before + options.pad_prefix_to, ' ');
    isize prefix_size = builder->size - size_before;

    if(prefix_size != 0 && options.put_space_before_marker)
        array_push(builder, ' ');

    array_push(builder, marker_char);

    if(entry.value.size > 0 && options.put_space_before_value)
        array_push(builder, ' ');

    builder_append(builder, entry.value);
    if(entry.comment.size > 0)
    {
        if(options.put_space_before_comment)
            array_push(builder, ' ');
                    
        array_push(builder, '#');
        builder_append(builder, entry.comment);
    }
    else if(options.comment_terminate_line)
        array_push(builder, '#');
    
    array_push(builder, '\n');

    return prefix_size;
}

String lpf_escape_label_or_type(String_Builder* into, String label_or_line)
{
    builder_assign(into, string_trim_whitespace(label_or_line));
    for(isize i = 0; i < into->size; i++)
    {
        char* c = &into->data[i];
        if(lpf_is_prefix_allowed_char(*c) == false)
            *c = '_';
    }

    return string_from_builder(*into);
}

typedef enum {
    SPLIT_NEWLINE, SPLIT_TOO_LONG 
} Lpf_Split_Reason;

typedef struct Lpf_Segment {
    Lpf_Split_Reason split_reason;
    String string;
} Lpf_Segment;


DEFINE_ARRAY_TYPE(Lpf_Segment, Lpf_Segment_Array);

void lpf_split_into_segments(Lpf_Segment_Array* segemnts, String value, isize max_size)
{
    for(Line_Iterator it = {0}; line_iterator_get_line(&it, value); )
    {
        String line = it.line;
        Lpf_Split_Reason reason = SPLIT_NEWLINE;
        while(line.size > max_size)
        {
            Lpf_Segment segment = {0};
            segment.split_reason = reason;
            segment.string = string_head(line, max_size);
            
            array_push(segemnts, segment);
            line = string_tail(line, max_size);
            reason = SPLIT_TOO_LONG;
        }
        
        Lpf_Segment last_segemnt = {0};
        last_segemnt.split_reason = reason;
        last_segemnt.string = line;
        array_push(segemnts, last_segemnt);
    }
}

void lpf_write_line_custom(String_Builder* builder, Lpf_Entry entry, const Lpf_Write_Options* options_or_null)
{
    Lpf_Write_Options options = {0};
    options.line_indentation = 0;
    options.pad_prefix_to = 0;
    options.put_space_before_marker = true;
    options.put_space_before_value = false;
    options.put_space_before_comment = false;
    options.pad_continuations = true;
    options.max_line_size = INT64_MAX;
    
    if(options_or_null)
        options = *options_or_null;

    if(options.max_line_size == 0)
        options.max_line_size = INT64_MAX;
    
    String label = entry.label;
    String type = entry.type;
    String value = entry.value;
    String comment = entry.comment;

    switch(entry.kind)
    {
        default:
        case LPF_KIND_BLANK: {
            builder_pad_to(builder, builder->size + options.line_indentation, ' ');
            array_push(builder, '\n');

            return;
        } break;

        case LPF_KIND_COMMENT: {
            label = STRING("");
            type = STRING("");
            value = STRING("");
        } break;

        case LPF_KIND_ENTRY: break;
        case LPF_KIND_CONTINUATION:        
        case LPF_KIND_ESCAPED_CONTINUATION: {
            label = STRING("");
            type = STRING("");
        } break;

        case LPF_KIND_COLLECTION_START: {
            value = STRING("");
        } break;

        case LPF_KIND_COLLECTION_END: {
            label = STRING("");
            value = STRING("");
            type = STRING("");
        } break;
    }

    enum {LOCAL_BUFF = 256, SEGMENTS = 8};
    Lpf_Segment_Array value_segments = {0};
    Lpf_Segment_Array comment_segments = {0};
    String_Builder escaped_label_builder = {0};
    String_Builder escaped_type_builder = {0};

    Allocator* scratch = allocator_get_scratch();
    array_grow(builder, builder->size + 30 + value.size + comment.size + type.size + label.size);

    array_init_backed(&escaped_label_builder, scratch, LOCAL_BUFF);
    array_init_backed(&escaped_type_builder, scratch, LOCAL_BUFF);
    array_init_backed(&value_segments, scratch, SEGMENTS);
    array_init_backed(&comment_segments, scratch, SEGMENTS);
    
    //Escape label
    if(label.size > 0)
        label = lpf_escape_label_or_type(&escaped_label_builder, label);
    
    //Escape type
    if(type.size > 0)
        type = lpf_escape_label_or_type(&escaped_type_builder, type);
    
    //Split value into segments (by lines and so that its shorter then max_line_size)
    if(value.size > 0)
        lpf_split_into_segments(&value_segments, value, options.max_line_size);
    
    bool comment_needs_escaping = false;

    //Split comment into segments (by lines and so that its shorter then max_line_size)
    if(comment.size > 0)
    {
        lpf_split_into_segments(&comment_segments, comment, options.max_line_size);
    
        if(value_segments.size > 0)
        {
            String last_string = array_last(value_segments)->string;
            if(last_string.size + comment.size > options.max_line_size)
                comment_needs_escaping = true;
        }

        if(comment_needs_escaping == false && string_find_first_char(comment, '#', 0) != -1)
            comment_needs_escaping = true;
    }

    Lpf_Write_Options downstream_options = options;
    
    if(entry.kind == LPF_KIND_COLLECTION_END || entry.kind == LPF_KIND_COLLECTION_START)
    {
        Lpf_Entry continuation = {0};
        continuation.kind = entry.kind;
        continuation.type = type;
        continuation.label = label;
        
        if(comment_segments.size == 1 && comment_needs_escaping == false)
        {
            continuation.comment = comment_segments.data[0].string;
            array_pop(&comment_segments);
        }

        isize padded_prefix = lpf_write_line_unescaped(builder, continuation, downstream_options);
        if(options.pad_continuations)
            downstream_options.pad_prefix_to = padded_prefix;
    }
    else
    {
        for(isize i = 0; i < value_segments.size; i++)
        {
            Lpf_Entry continuation = {0};
            Lpf_Segment segment = value_segments.data[i];
            continuation.value = segment.string;
            continuation.kind = LPF_KIND_CONTINUATION;
            if(segment.split_reason == SPLIT_TOO_LONG)
                continuation.kind = LPF_KIND_ESCAPED_CONTINUATION;
        
            //if is first segment add the prefix as well
            if(i == 0)
            {
                continuation.kind = entry.kind;
                continuation.type = type;
                continuation.label = label;
            }

            //Only add the comments to the last entry
            if(i == value_segments.size - 1)
            {
                //the comment must not need escaping though
                if(comment_segments.size == 1 && comment_needs_escaping == false)
                {
                    continuation.comment = comment_segments.data[0].string;
                    array_pop(&comment_segments);
                }
            }
        
            isize padded_prefix = lpf_write_line_unescaped(builder, continuation, downstream_options);
            if(options.pad_continuations)
                downstream_options.pad_prefix_to = padded_prefix;
        }
    }

    
    //Add the remaining comments that need escaping
    if(options.pad_continuations)
        downstream_options.line_indentation += downstream_options.pad_prefix_to;

    for(isize i = 0; i < comment_segments.size; i++)
    {
        Lpf_Entry comment_entry = {0};
        comment_entry.kind = LPF_KIND_COMMENT;
        comment_entry.comment = comment_segments.data[i].string;

        lpf_write_line_unescaped(builder, comment_entry, downstream_options);
    }
    
    array_deinit(&escaped_label_builder);
    array_deinit(&escaped_type_builder);
    array_deinit(&value_segments);
    array_deinit(&comment_segments);
}

typedef void*(*Lpf_Reallocate)(isize new_size, void* old_ptr, isize old_size, void* context);
typedef bool(*Lpf_Log)(Lpf_Error error, isize line, isize line_index, isize data_offset, void* context, const char* format, va_list args);

typedef struct Lpf_File {
    Lpf_Entry* entries;
    isize entries_size;
    isize entries_capacity;

    Lpf_Collection* collections;
    isize collections_size;
    isize collections_capacity;

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

isize lpf_parse(Lpf_File* into, const char* lpf_source, isize source_size, isize flags);

bool lpf_log(Lpf_File* file, Lpf_Error error, isize line, isize line_index, isize data_offset, const char* format, ...)
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

void lpf_push(void** ptr, isize* size, isize* capacity, isize item_size, Lpf_Reallocate realloc_func, void* realloc_context)
{
    if(*size >= *capacity)
    {
        isize new_cap = *size * 3/2;
        if(new_cap < 8)
            new_cap = 8;
    
        isize new_byte_cap = new_cap * item_size;
        isize old_byte_cap = *capacity * item_size;

        void* old_ptr = *ptr;
        void* new_ptr = realloc_func(new_byte_cap, old_ptr, old_byte_cap, realloc_context);
        assert(new_ptr != NULL);

        *ptr = new_ptr;
        *capacity = new_cap;
    }
    
    *size += 1;
}

isize lpf_parse(Lpf_File* into, const char* lpf_source, isize source_size, isize flags)
{
    (void) into, lpf_source, source_size, flags;
    return 0;
    //if(entry.kind == LPF_KIND_COLLECTION_START)
    //{
    //    assert(into->collections_size > 0);
    //    assert(0 < collection_i1 && collection_i1 <= into->collections_size);
    //                   
    //    lpf_push((void**) &into->collections, &into->collections_size, &into->collections_capacity, 
    //        sizeof(Lpf_Collection), into->realloc_func, into->realloc_context);
    //
    //    isize parent_i1 = collection_i1;
    //    Lpf_Collection* parent = &into->collections[parent_i1 - 1];
    //
    //    isize curr_i1 = into->collections_size;
    //    Lpf_Collection* curr = &into->collections[curr_i1 - 1];
    //
    //    curr->parent_i1 = parent_i1;
    //    curr->depth = parent->depth + 1;
    //    curr->line_number = line_number;
    //    curr->type = entry.type;
    //    curr->label = entry.label;
    //    curr->entries.from = into->entries_size;
    //    curr->entries.to = into->entries_size;
    //        
    //    if(parent->last_child_i1 == 0)
    //        parent->first_child_i1 = curr_i1;
    //    else
    //    {
    //        isize prev_i1 = parent->last_child_i1;
    //        Lpf_Collection* prev = &into->collections[prev_i1 - 1];
    //        prev->next_i1 = curr_i1;
    //        curr->prev_i1 = prev_i1;
    //    }
    //
    //    parent->last_child_i1 = curr_i1;
    //    parent->children_count += 1;
    //                
    //    depth += 1;
    //    collection_i1 = curr_i1;
    //}
    //else
    //{
    //    assert(0 < collection_i1 && collection_i1 <= into->collections_size);
    //    isize curr_i1 = collection_i1;
    //    Lpf_Collection* curr = &into->collections[curr_i1 - 1];
    //
    //    if(curr->parent_i1 <= 0)
    //    {
    //        entry.error = LPF_ERROR_COLLECTION_TOO_MANY_ENDS;
    //        goto line_end;
    //    }
    //
    //    curr->entries.to = into->entries_size;
    //    collection_i1 = curr->parent_i1;
    //    depth -= 1;
    //}
}


void* lpf_default_realloc(isize new_size, void* old_ptr, isize old_size, void* context)
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
bool lpf_default_log(Lpf_Error error, isize line, isize line_index, isize data_offset, void* context, const char* format, va_list args)
{
    (void) context;
    printf("Had error %s at line: %d char: %d (%d) with message: ", lpf_error_to_string(error), (int) line, (int) line_index, (int) data_offset);
    vprintf(format, args);
    return false;
}
