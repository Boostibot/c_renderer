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
#include "assert.h"

typedef enum Lpf_Kind {
    LPF_KIND_BLANK = 0,
    LPF_KIND_ENTRY,
    LPF_KIND_CONTINUATION,
    LPF_KIND_ESCAPED_CONTINUATION,
    LPF_KIND_COMMENT,
    LPF_KIND_SCOPE_START,
    LPF_KIND_SCOPE_END,
} Lpf_Kind;

typedef enum Lpf_Error {
    LPF_ERROR_NONE = 0,
    LPF_ERROR_ENTRY_INVALID_CHAR_BEFORE_START,

    LPF_ERROR_ENTRY_MISSING_START,
    LPF_ERROR_ENTRY_MULTIPLE_TYPES,
    LPF_ERROR_ENTRY_CONTINUNATION_WITHOUT_START,
    LPF_ERROR_ENTRY_CONTINUNATION_HAS_LABEL,
    
    LPF_ERROR_SCOPE_END_HAS_LABEL,
    LPF_ERROR_SCOPE_MULTIPLE_TYPES,
    LPF_ERROR_SCOPE_CONTENT_AFTER_START,
    LPF_ERROR_SCOPE_CONTENT_AFTER_END,
    LPF_ERROR_SCOPE_TOO_MANY_ENDS,
} Lpf_Error;

enum {
    LPF_FLAG_WHITESPACE_SENSITIVE = 1,  //All whitespace matters
    LPF_FLAG_WHITESPACE_PREFIX_AGNOSTIC = 2, //prefix whitespace including newlines does not matter
    LPF_FLAG_WHITESPACE_POSTFIX_AGNOSTIC = 4, //postfix whitespace including newlines does not matter. Allows space betweem value and comment
    LPF_FLAG_NEWLINE_AGNOSTIC = 8, //newlines are treated as whitespace (dont need escaping)
    LPF_FLAG_WHITESPACE_AGNOSTIC = LPF_FLAG_NEWLINE_AGNOSTIC | LPF_FLAG_WHITESPACE_PREFIX_AGNOSTIC | LPF_FLAG_WHITESPACE_POSTFIX_AGNOSTIC, //whitespace and newlines dont matter (as long as there is at least one)
    LPF_FLAG_DONT_WRITE = 16,
};

typedef struct Lpf_Dyn_Entry Lpf_Dyn_Entry; 

typedef struct Lpf_Entry {
    Lpf_Kind kind;
    Lpf_Error error;
    u16 format_flags;
    isize line_number;
    isize depth;

    String label;
    String type;
    String value;
    String comment;

    Lpf_Dyn_Entry* children;
    isize children_size;
    isize children_capacity;
} Lpf_Entry;

typedef struct Lpf_Dyn_Entry {
    i8  kind;
    i8  error;
    u16 format_flags;
    u32 depth;
    i64 line_number;

    Allocator* allocator;
    char* text_parts;

    isize comment_size;
    isize label_size;
    isize type_size;
    isize value_size;

    Lpf_Dyn_Entry* children;
    u32 children_size;
    u32 children_capacity;
} Lpf_Dyn_Entry;

typedef struct Lpf_Write_Options {
    isize line_indentation;
    isize comment_indentation;
    
    isize pad_prefix_to;

    bool put_space_before_marker;
    bool put_space_before_value;
    bool put_space_before_comment;
    bool comment_terminate_value;
} Lpf_Write_Options;

typedef struct Lpf_Format_Options {
    isize max_value_size;
    isize max_comment_size;
    i32 pad_prefix_to;

    i32 line_identation_per_level;
    i32 comment_identation_per_level;

    i32 line_indentation_offset;
    i32 comment_indentation_offset;

    String hash_escape;
    bool pad_continuations;
    bool put_space_before_marker;

    bool skip_comments;
    bool skip_inline_comments;
    bool skip_blanks;
    bool skip_scopes;
    bool skip_scope_ends;
    bool skip_types;
    bool skip_errors;
    bool log_errors;

    bool correct_errors;
    bool stop_on_error;
} Lpf_Format_Options;

typedef struct Lpf_Writer {
    isize depth;
    isize line_number;
} Lpf_Writer;

typedef struct Lpf_Reader {
    bool            has_last_entry;
    Lpf_Entry       last_entry;
    String_Builder  last_value;
    String_Builder  last_comment;

    ptr_Array scopes;
    isize depth;
    isize line_number;
} Lpf_Reader;


const char*         lpf_error_to_string(Lpf_Error error);
Lpf_Format_Options  lpf_make_default_format_options();

Lpf_Dyn_Entry       lpf_dyn_entry_from_entry(Lpf_Entry entry, Allocator* alloc);
Lpf_Entry           lpf_entry_from_dyn_entry(Lpf_Dyn_Entry dyn);

isize               lpf_lowlevel_read_entry(String source, isize from, Lpf_Entry* parsed);
isize               lpf_lowlevel_write_entry_unescaped(String_Builder* source, Lpf_Entry entry, Lpf_Write_Options options);
void                lpf_lowlevel_write_entry(String_Builder* source, Lpf_Entry entry, const Lpf_Write_Options* options_or_null);

void                lpf_write_entry(Lpf_Writer* writer, String_Builder* into, Lpf_Entry entry, const Lpf_Format_Options* format);
Lpf_Error           lpf_read_entry(Lpf_Reader* reader, Lpf_Dyn_Entry* into, Lpf_Entry entry, const Lpf_Format_Options* options);

void                lpf_reader_deinit(Lpf_Reader* reader);
void                lpf_reader_reset(Lpf_Reader* reader);
void                lpf_reader_commit_entries(Lpf_Reader* reader);

void                lpf_dyn_entry_deinit(Lpf_Dyn_Entry* dyn);
void                lpf_dyn_entry_push_dyn(Lpf_Dyn_Entry* dyn, Lpf_Dyn_Entry pushed);
void                lpf_dyn_entry_push(Lpf_Dyn_Entry* dyn, Lpf_Entry pushed);
void                lpf_dyn_entry_map(Lpf_Dyn_Entry* dyn, void(*preorder_func)(Lpf_Dyn_Entry* dyn, void* context), void(*postorder_func)(Lpf_Dyn_Entry* dyn, void* context), void* context);

Lpf_Error           lpf_read_custom(String source, Lpf_Dyn_Entry* root, const Lpf_Format_Options* options);
void                lpf_write_custom(String_Builder* source, Lpf_Dyn_Entry root, const Lpf_Format_Options* options);

Lpf_Dyn_Entry*      lpf_find(Lpf_Dyn_Entry in_children_of, Lpf_Kind kind, const char* label, const char* type);
isize               lpf_find_index(Lpf_Dyn_Entry in_children_of, Lpf_Kind kind, String label, String type, isize from);


Lpf_Format_Options lpf_make_default_format_options()
{
    Lpf_Format_Options options = {0};
    options.max_value_size = 200;
    options.max_comment_size = 200;
    options.line_identation_per_level = 4;
    options.comment_identation_per_level = 2;
    options.pad_continuations = true;
    options.put_space_before_marker = true;
    options.hash_escape = STRING(":hashtag:");

    return options;
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
    
        case LPF_ERROR_SCOPE_END_HAS_LABEL: return "LPF_ERROR_SCOPE_END_HAS_LABEL";
        case LPF_ERROR_SCOPE_CONTENT_AFTER_START: return "LPF_ERROR_SCOPE_CONTENT_AFTER_START";
        case LPF_ERROR_SCOPE_CONTENT_AFTER_END: return "LPF_ERROR_SCOPE_CONTENT_AFTER_END";
        case LPF_ERROR_SCOPE_TOO_MANY_ENDS: return "LPF_ERROR_SCOPE_TOO_MANY_ENDS";
        case LPF_ERROR_SCOPE_MULTIPLE_TYPES: return "LPF_ERROR_SCOPE_MULTIPLE_TYPES";
    }
}

isize lpf_parse_inline_comment(String source, isize line_size, String* comment)
{
    isize value_to = line_size;
    isize tag_pos = string_find_last_char_from(source, '#', line_size - 1);
    if(tag_pos != -1)
    {
        value_to = tag_pos;
        *comment = string_range(source, tag_pos + 1, line_size);
    }
    else
    {
        *comment = STRING("");
    }

    return value_to;
}

isize lpf_lowlevel_read_entry(String source_, isize from, Lpf_Entry* parsed)
{
    String source = string_tail(source_, from);

    isize source_i = 0;
    Lpf_Entry entry = {LPF_KIND_BLANK};
    struct {
        isize from; 
        isize to;
    } word_range = {0};

    //parse line start
    bool had_non_space = false;
    bool is_within_type = false;

    //@TODO: do the comment parsing marker parsing and comment parsing all here within a single loop
    // then only do the prefix words if it is desired and it exists
    isize line_size = string_find_first_char(source, '\n', 0);
    if(line_size == -1)
        line_size = source.size;
    
    isize line_end = MIN(line_size + 1, source.size);

    enum {MAX_TYPES = 2};

    isize had_types = 0;
    String types[MAX_TYPES] = {0};

    for(; source_i < line_size; source_i++)
    {
        char c = source.data[source_i];

        switch(c)
        {
            case ':': entry.kind = LPF_KIND_ENTRY; break;
            case ';': entry.kind = LPF_KIND_ESCAPED_CONTINUATION;  break;
            case ',': entry.kind = LPF_KIND_CONTINUATION;  break;
            case '#': entry.kind = LPF_KIND_COMMENT;  break;
            case '{': entry.kind = LPF_KIND_SCOPE_START; break;
            case '}': entry.kind = LPF_KIND_SCOPE_END; break;
            default: {
                if(char_is_space(c) == false)
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
            } break;
        }

        if((entry.kind != LPF_KIND_BLANK && is_within_type) || word_range.to != 0)
        {
            word_range.to = source_i;

            if(had_types < 2)
                types[had_types] = string_range(source, word_range.from, word_range.to);
            
            had_types += 1;
            word_range.to = 0;
        }

        if(entry.kind != LPF_KIND_BLANK)
            break;
    }
        
    //if was a blank line skip
    if(entry.kind == LPF_KIND_BLANK && had_non_space == false)
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
            if(entry.kind == LPF_KIND_CONTINUATION || entry.kind == LPF_KIND_ESCAPED_CONTINUATION)
            {
                if(had_types > 0)
                {
                    entry.error = LPF_ERROR_ENTRY_CONTINUNATION_HAS_LABEL;
                    goto line_end;
                }
            }
            else
            {
                if(had_types > 2)
                {
                    entry.error = LPF_ERROR_ENTRY_MULTIPLE_TYPES;
                    goto line_end;
                }

                entry.label = types[0];
                entry.type = types[1];
            }

            isize value_from = source_i;
            isize value_to = lpf_parse_inline_comment(source, line_size, &entry.comment);
            entry.value = string_range(source, value_from, value_to);
        }
        break;

        case LPF_KIND_COMMENT: {
            if(had_types > 0)
            {
                entry.error = LPF_ERROR_SCOPE_END_HAS_LABEL;
                goto line_end;
            }
                
            entry.comment = string_range(source, source_i, line_size);
        }
        break;

        case LPF_KIND_SCOPE_START: 
        case LPF_KIND_SCOPE_END: {
            if(entry.kind == LPF_KIND_SCOPE_END)
            {
                if(had_types > 0)
                {
                    entry.error = LPF_ERROR_SCOPE_END_HAS_LABEL;
                    goto line_end;
                }
            }
            else
            {
                if(had_types > 2)
                {
                    entry.error = LPF_ERROR_SCOPE_MULTIPLE_TYPES;
                    goto line_end;
                }

                entry.label = types[0];
                entry.type = types[1];
            }
            
            isize value_from = source_i;
            isize value_to = lpf_parse_inline_comment(source, line_size, &entry.comment);
            String value = string_range(source, value_from, value_to);

            //Value needs to be whitespace only.
            String non_white = string_trim_prefix_whitespace(value);
            if(non_white.size > 0)
            {
                if(entry.kind == LPF_KIND_SCOPE_START)
                    entry.error = LPF_ERROR_SCOPE_CONTENT_AFTER_START;
                else
                    entry.error = LPF_ERROR_SCOPE_CONTENT_AFTER_END;
                goto line_end;
            }
        }
        break;
            
        default: assert(false);
    }

    
    line_end:

    *parsed = entry;
    return from + line_end;
}

void builder_pad_to(String_Builder* builder, isize to_size, char with)
{
    if(builder->size >= to_size)
        return;

    isize size_before = array_resize(builder, to_size);
    memset(builder->data + size_before, with, builder->size - size_before);
}

bool lpf_is_prefix_allowed_char(char c)
{
    if(char_is_space(c))
        return false;

    if(c == ':' || c == ',' || c == '#' ||c == ';' || c == '{' || c == '}')
        return false;

    return true;
}

isize lpf_lowlevel_write_entry_unescaped(String_Builder* builder, Lpf_Entry entry, Lpf_Write_Options options)
{
    array_grow(builder, builder->size + 30 + entry.value.size + entry.comment.size + entry.type.size + entry.label.size);

    #ifdef DO_ASSERTS_SLOW
        for(isize i = 0; i < entry.label.size; i++)
            ASSERT_SLOW_MSG(lpf_is_prefix_allowed_char(entry.label.data[i]), 
                "label must contain only valid data! Label: " STRING_FMT, STRING_PRINT(entry.label));

        for(isize i = 0; i < entry.type.size; i++)
            ASSERT_SLOW_MSG(lpf_is_prefix_allowed_char(entry.type.data[i]),
                "type must contain only valid data! Label: " STRING_FMT, STRING_PRINT(entry.type));

        isize newline_pos = string_find_first_char(entry.value, '\n', 0);
        isize tag_pos = string_find_first_char(entry.value, '#', 0);
        isize comment_newline_pos = string_find_first_char(entry.comment, '\n', 0);
        isize comment_tag_pos = string_find_first_char(entry.comment, '#', 0);

        ASSERT_SLOW_MSG(newline_pos == -1, "value must not contain newlines. Value: \""STRING_FMT"\"", STRING_PRINT(entry.value));
        ASSERT_SLOW_MSG(comment_newline_pos == -1, "comment must not contain newlines. Value: \""STRING_FMT"\"", STRING_PRINT(entry.comment));
        if(tag_pos != -1)
            ASSERT_SLOW_MSG(options.comment_terminate_value, "If the value contains # it must be comment terminated!");

        if(entry.kind != LPF_KIND_COMMENT)
            ASSERT_SLOW_MSG(comment_tag_pos == -1, "comment must not contain #. Value: \""STRING_FMT"\"", STRING_PRINT(entry.comment));
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
            builder_pad_to(builder, builder->size + options.comment_indentation, ' ');
            builder_append(builder, entry.comment);
            array_push(builder, '\n');

            return 0;
        } break;

        case LPF_KIND_ENTRY:                marker_char = ':'; break;
        case LPF_KIND_CONTINUATION:         marker_char = ','; break;
        case LPF_KIND_ESCAPED_CONTINUATION: marker_char = ';'; break;
        case LPF_KIND_SCOPE_START:     marker_char = '{'; break;
        case LPF_KIND_SCOPE_END:       marker_char = '}'; break;
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
    

    builder_append(builder, entry.value);
    if(entry.comment.size > 0)
    {
        if(options.put_space_before_comment && options.comment_terminate_value == false)
            array_push(builder, ' ');

        array_push(builder, '#');
        builder_pad_to(builder, builder->size + options.comment_indentation, ' ');
        builder_append(builder, entry.comment);
    }
    else if(options.comment_terminate_value)
        array_push(builder, '#');
    
    array_push(builder, '\n');

    return prefix_size;
}

String lpf_escape_label_or_type(String_Builder* into, String label_or_line)
{
    for(isize i = 0; i < label_or_line.size; i++)
    {
        char c = label_or_line.data[i];
        if(lpf_is_prefix_allowed_char(c))
            array_push(into, c);
    }

    return string_from_builder(*into);
}

String lpf_trim_escape(String value, char c1, char c2)
{
    isize i = 0;
    for(; i < value.size; i++)
    {
        char c = value.data[i];
        if(c == c1 || c == c2)
            break;
    }

    String escaped = string_head(value, i);
    return escaped;
}

isize _lpf_lowlevel_write_entry(String_Builder* builder, Lpf_Entry entry, const Lpf_Write_Options* options_or_null)
{
    Lpf_Write_Options options = {0};
    Lpf_Entry escaped = {entry.kind};
    if(options_or_null)
        options = *options_or_null;

    switch(entry.kind)
    {
        default:
        case LPF_KIND_BLANK: {
            builder_pad_to(builder, builder->size + options.line_indentation, ' ');
            array_push(builder, '\n');

            return 0;
        } break;

        case LPF_KIND_COMMENT: {
            escaped.comment = entry.comment;
        } break;

        case LPF_KIND_ENTRY: {
            escaped = entry;
        }break;
        case LPF_KIND_CONTINUATION:        
        case LPF_KIND_ESCAPED_CONTINUATION: {
            escaped.value = entry.value;
            escaped.comment = entry.comment;
        } break;

        case LPF_KIND_SCOPE_START: {
            escaped.label = entry.label;
            escaped.type = entry.type;
            escaped.comment = entry.comment;
        } break;

        case LPF_KIND_SCOPE_END: {
            escaped.comment = entry.comment;
        } break;
    }
    
    enum {LOCAL_BUFF = 256, SEGMENTS = 8};
    String_Builder escaped_label_builder = {0};
    String_Builder escaped_type_builder = {0};

    Allocator* scratch = allocator_get_scratch();

    array_init_backed(&escaped_label_builder, scratch, LOCAL_BUFF);
    array_init_backed(&escaped_type_builder, scratch, LOCAL_BUFF);

    //Escape label
    if(escaped.label.size > 0)
        escaped.label = lpf_escape_label_or_type(&escaped_label_builder, escaped.label);
    
    //Escape type
    if(escaped.type.size > 0)
        escaped.type = lpf_escape_label_or_type(&escaped_type_builder, escaped.type);

    //Escape value
    if(escaped.value.size > 0)
    {
        bool had_hash = false;
        isize i = 0;
        for(; i < escaped.value.size; i++)
        {
            char c = escaped.value.data[i];
            if(c == '\n')
                break;
            if(c == '#')
                had_hash = true;
        }

        if(had_hash)
            options.comment_terminate_value = true;

        escaped.value = string_head(escaped.value, i);
    }

    //Escape comments
    if(escaped.comment.size > 0)
    {
        if(entry.kind == LPF_KIND_COMMENT)
            escaped.comment = lpf_trim_escape(escaped.comment, '\n', '\n');
        else
            escaped.comment = lpf_trim_escape(escaped.comment, '\n', '#');
    }
    
    //@NOTE: this cretes new information instead of destroying it like the rest of the functions here.
    //       Should this be allowed?
    if(escaped.type.size > 0 && escaped.label.size == 0)
        escaped.label = STRING("_");
    
    isize prefix_size = lpf_lowlevel_write_entry_unescaped(builder, escaped, options);
    
    array_deinit(&escaped_label_builder);
    array_deinit(&escaped_type_builder);

    return prefix_size;
}

void lpf_lowlevel_write_entry(String_Builder* builder, Lpf_Entry entry, const Lpf_Write_Options* options_or_null)
{
    _lpf_lowlevel_write_entry(builder, entry, options_or_null);
}

typedef struct Lpf_Segment {
    Lpf_Kind kind;
    String string;
} Lpf_Segment;

DEFINE_ARRAY_TYPE(Lpf_Segment, Lpf_Segment_Array);

bool lpf_split_into_segments(Lpf_Segment_Array* segemnts, String value, isize max_size)
{
    ASSERT(max_size > 0);
    if(max_size <= 0)
        max_size = INT64_MAX;

    bool had_too_log = false;
    for(Line_Iterator it = {0}; line_iterator_get_line(&it, value); )
    {
        String line = it.line;
        Lpf_Kind kind = LPF_KIND_CONTINUATION;
        while(line.size > max_size)
        {
            had_too_log = true;
            Lpf_Segment segment = {0};
            segment.kind = kind;
            segment.string = string_head(line, max_size);
            
            array_push(segemnts, segment);
            line = string_tail(line, max_size);
            kind = LPF_KIND_ESCAPED_CONTINUATION;
        }
        
        Lpf_Segment last_segemnt = {0};
        last_segemnt.kind = kind;
        last_segemnt.string = line;
        array_push(segemnts, last_segemnt);
    }

    return had_too_log;
}

void lpf_write_entry(Lpf_Writer* writer, String_Builder* builder, Lpf_Entry entry, const Lpf_Format_Options* format)
{
    Lpf_Kind kind = entry.kind;
    String label = entry.label;
    String type = entry.type;
    String value = entry.value;
    String comment = entry.comment;
    
    if(format->skip_errors && entry.error != LPF_ERROR_NONE)
        return;
    if(format->skip_blanks && kind == LPF_KIND_BLANK)
        return;
    if(format->skip_scopes && (kind == LPF_KIND_SCOPE_START || kind == LPF_KIND_SCOPE_END))
        return;
    if(format->skip_comments && kind == LPF_KIND_COMMENT)
        return;
    if(entry.format_flags & LPF_FLAG_DONT_WRITE)
        return;

    if(kind == LPF_KIND_BLANK)
    {
        if(format->skip_blanks == false)
        {
            Lpf_Write_Options options = {0};
            lpf_lowlevel_write_entry_unescaped(builder, entry, options);
        }

        return;
    }

    if(format->skip_types)
        type = STRING("");

    if(format->skip_inline_comments && kind != LPF_KIND_COMMENT)
        comment = STRING("");

    enum {LOCAL_BUFF = 256, SEGMENTS = 8};
    Lpf_Segment_Array value_segments = {0};
    Lpf_Segment_Array comment_segments = {0};
    String_Builder escaped_inline_comment = {0};

    Allocator* scratch = allocator_get_scratch();

    array_init_backed(&escaped_inline_comment, scratch, LOCAL_BUFF);
    array_init_backed(&value_segments, scratch, SEGMENTS);
    array_init_backed(&comment_segments, scratch, SEGMENTS);
    
    Lpf_Write_Options options = {0};
    options.line_indentation = format->line_identation_per_level*writer->depth + format->line_indentation_offset;
    options.pad_prefix_to = format->pad_prefix_to;
    options.put_space_before_marker = format->put_space_before_marker;
    options.put_space_before_comment = !!(entry.format_flags & LPF_FLAG_WHITESPACE_POSTFIX_AGNOSTIC);
    options.comment_terminate_value = !!(entry.format_flags & LPF_FLAG_WHITESPACE_SENSITIVE);
    
    if(entry.format_flags & LPF_FLAG_WHITESPACE_PREFIX_AGNOSTIC)
        value = string_trim_prefix_whitespace(value); 
                    
    if(entry.format_flags & LPF_FLAG_WHITESPACE_POSTFIX_AGNOSTIC)
        value = string_trim_postfix_whitespace(value);

    //Escape the inline comment:
    // "  an inline comment \n"
    // "  with # and lots of space  "
    // ->
    // "  an inline comment with :hashtag: and lots of space"
    if(comment.size > 0 && entry.kind != LPF_KIND_COMMENT)
    {
        isize last_size = escaped_inline_comment.size;
        for(Line_Iterator line_it = {0}; line_iterator_get_line(&line_it, comment); )
        {
            String line = string_trim_postfix_whitespace(line_it.line);
            if(line_it.line_number != 1)
                line = string_trim_prefix_whitespace(line);

            if(last_size != escaped_inline_comment.size)
                array_push(&escaped_inline_comment, ' ');
            
            last_size = escaped_inline_comment.size;
            for(isize i = 0; i <= line.size; i++)
            {
                isize next_i = string_find_first_char(line, '#', i);
                if(next_i == -1)
                    next_i = line.size;
                
                if(i != 0)
                    builder_append(&escaped_inline_comment, format->hash_escape);

                String current_range = string_range(line, i, next_i);
                builder_append(&escaped_inline_comment, current_range);
                i = next_i;
            }
        }

        comment = string_from_builder(escaped_inline_comment);
    }

    //Writers scopes normally:
    // label type { #comment
    //      #increased indentation!   
    // } #comment
    if(entry.kind == LPF_KIND_SCOPE_END || entry.kind == LPF_KIND_SCOPE_START)
    {
        if(entry.kind == LPF_KIND_SCOPE_END)
        {
            options.line_indentation -= format->line_identation_per_level;
            writer->depth = MAX(writer->depth - 1, 0);
        }
        
        if(entry.kind == LPF_KIND_SCOPE_START)
            writer->depth += 1;

        Lpf_Entry continuation = {kind};
        continuation.type = type;
        continuation.label = label;
        continuation.comment = comment;

        options.put_space_before_comment = true;
        lpf_lowlevel_write_entry(builder, continuation, &options);
    }

    //Writes comment:
    // "this is a comment thats too long \n"
    // "with newlines \n"
    // ->
    // # this is comment
    // #  thats too long
    // # with newlines
    //  <-------------->
    // writer->max_comment_size
    else if(entry.kind == LPF_KIND_COMMENT)
    {
        //Split comment into segments (by lines and so that its shorter then max_value_size)
        lpf_split_into_segments(&comment_segments, comment, format->max_comment_size);
        for(isize i = 0; i < comment_segments.size; i++)
        {
            Lpf_Segment seg = comment_segments.data[i];
            Lpf_Entry continuation = {LPF_KIND_COMMENT};
            continuation.comment = seg.string;
            lpf_lowlevel_write_entry(builder, continuation, &options);
        }
    }
    
    //Writes entry:
    // "this is a value thats too long \n"
    // "with newlines \n"
    // ->
    // :this is a value thats #
    // ;too long #
    // ,with newlines #
    // ,#
    //  <-------------------->
    //  writer->max_value_size
    //  (with no flag)
    //
    // " 123\n123", "comment"
    // ->
    // : 123 
    // ,123 #comment
    // (with LPF_FLAG_COMMENT_TERMINATE_NEVER)
    else
    {
        //Split value into segments (by lines and so that its shorter then max_value_size)
        bool had_too_long = false;
        if(value.size > 0)
            had_too_long = lpf_split_into_segments(&value_segments, value, format->max_value_size);
            
        if(value_segments.size > 1)
            if((entry.format_flags & LPF_FLAG_WHITESPACE_AGNOSTIC) == 0)
                options.comment_terminate_value = true;

        if(had_too_long)
            options.comment_terminate_value = true;
            
        for(isize i = 0; i < value_segments.size; i++)
        {
            Lpf_Segment segment = value_segments.data[i];
            Lpf_Entry continuation = {0};
            continuation.kind = segment.kind;
            continuation.value = segment.string;

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
                continuation.comment = comment;
            }
        
            isize padded_prefix = _lpf_lowlevel_write_entry(builder, continuation, &options);
            if(format->pad_continuations)
                options.pad_prefix_to = padded_prefix;
        }
    }
    
    array_deinit(&escaped_inline_comment);
    array_deinit(&value_segments);
    array_deinit(&comment_segments);
}

void lpf_reader_deinit(Lpf_Reader* reader)
{
    lpf_reader_commit_entries(reader);
    array_deinit(&reader->scopes);
    array_deinit(&reader->last_value);
    array_deinit(&reader->last_comment);
    memset(reader, 0, sizeof *reader);
}

void lpf_reader_reset(Lpf_Reader* reader)
{
    lpf_reader_commit_entries(reader);
    array_clear(&reader->scopes);
    array_clear(&reader->last_value);
    array_clear(&reader->last_comment);
}

void _lpf_dyn_entry_deinit_self_only(Lpf_Dyn_Entry* dyn, void* context)
{
    (void) context;
    if(dyn->text_parts != NULL)
    {
        isize combined_size = dyn->label_size + dyn->type_size + dyn->comment_size + dyn->value_size;
        isize allocated_size = combined_size + 4;

        allocator_deallocate(dyn->allocator, dyn->text_parts, allocated_size, DEF_ALIGN, SOURCE_INFO());
        dyn->text_parts = NULL;
    }
    
    if(dyn->children != NULL)
    {
        allocator_deallocate(dyn->allocator, dyn->children, dyn->children_capacity*sizeof(Lpf_Dyn_Entry), DEF_ALIGN, SOURCE_INFO());
    }
}

void lpf_dyn_entry_map(Lpf_Dyn_Entry* dyn, void(*preorder_func)(Lpf_Dyn_Entry* dyn, void* context), void(*postorder_func)(Lpf_Dyn_Entry* dyn, void* context), void* context)
{
    if(dyn->children_size > 0)
    {
        typedef struct Iterator {
            Lpf_Dyn_Entry* scope;
            isize i;
        } Iterator;

        DEFINE_ARRAY_TYPE(Iterator, Iterator_Array);
        
        Iterator_Array iterators = {0};
        array_init_backed(&iterators, allocator_get_scratch(), 32);
        
        Iterator top_level_it = {dyn, 0};
        array_push(&iterators, top_level_it);

        while(iterators.size > 0)
        {
            Iterator* it = array_last(iterators);
            bool going_deeper = false;

            for(; it->i < it->scope->children_size; it->i++)
            {
                Lpf_Dyn_Entry* child = &it->scope->children[it->i];
                if(preorder_func)
                    preorder_func(child, context);

                if(child->children != NULL)
                {
                    Iterator child_level_it = {child, 0};
                    array_push(&iterators, child_level_it);
                    going_deeper = true;
                }
                else
                {
                    if(postorder_func)
                        postorder_func(child, context);
                }
            }

            if(going_deeper == false)
            {
                if(postorder_func)
                    postorder_func(it->scope, context);

                array_pop(&iterators);
            }
        }
    }
    else
    {
        if(preorder_func)
            preorder_func(dyn, context);
            
        if(postorder_func)
            postorder_func(dyn, context);
    }
}

void lpf_dyn_entry_deinit(Lpf_Dyn_Entry* dyn)
{
    if(dyn->children != NULL)
        lpf_dyn_entry_map(dyn, NULL, _lpf_dyn_entry_deinit_self_only, NULL);
    else
        _lpf_dyn_entry_deinit_self_only(dyn, NULL);
    memset(dyn, 0, sizeof *dyn);
}


isize lpf_find_index(Lpf_Dyn_Entry in_children_of, Lpf_Kind kind, String label, String type, isize from)
{
    for(isize i = from; i < in_children_of.children_size; i++)
    {
        Lpf_Dyn_Entry* child = &in_children_of.children[i];
        if(child->kind != kind)
            continue;

        Lpf_Entry child_e = lpf_entry_from_dyn_entry(*child);
        if(label.size > 0 && string_is_equal(child_e.label, label) == false)
            continue;
            
        if(type.size > 0 && string_is_equal(child_e.type, type) == false)
            continue;

        return i;
    }

    return -1;
}


Lpf_Dyn_Entry* lpf_find(Lpf_Dyn_Entry in_children_of, Lpf_Kind kind, const char* label, const char* type)
{
    String label_str = string_make(label);
    String type_str = string_make(type);
    isize found = lpf_find_index(in_children_of, kind, label_str, type_str, 0);
    if(found == -1)
        return NULL;
    else
        return &in_children_of.children[found];
}

void lpf_dyn_entry_push_dyn(Lpf_Dyn_Entry* dyn, Lpf_Dyn_Entry push)
{
    if(dyn->children_size + 1 > dyn->children_capacity)
    {
        isize new_capacity = 2;
        while(new_capacity <= dyn->children_capacity)
            new_capacity *= 2;

        if(dyn->allocator == NULL)
            dyn->allocator = allocator_get_default();
        dyn->children = (Lpf_Dyn_Entry*) allocator_reallocate(dyn->allocator, new_capacity*sizeof(Lpf_Dyn_Entry), dyn->children, dyn->children_capacity*sizeof(Lpf_Dyn_Entry), DEF_ALIGN, SOURCE_INFO());
        dyn->children_capacity = (u32) new_capacity;
    }
        
    dyn->children[dyn->children_size++] = push;
}

void lpf_dyn_entry_push(Lpf_Dyn_Entry* dyn, Lpf_Entry push)
{
    Lpf_Dyn_Entry pushed = lpf_dyn_entry_from_entry(push, dyn->allocator);
    lpf_dyn_entry_push_dyn(dyn, pushed);
}

Lpf_Dyn_Entry lpf_dyn_entry_from_entry(Lpf_Entry entry, Allocator* alloc)
{
    Lpf_Dyn_Entry dyn = {0};
    dyn.allocator = alloc;
    if(dyn.allocator == NULL)
        dyn.allocator = allocator_get_default();

    dyn.kind = entry.kind;
    dyn.error = entry.error;
    dyn.line_number = entry.line_number;
    dyn.depth = (u32) entry.depth;
    dyn.format_flags = entry.format_flags;

    isize combined_size = entry.label.size + entry.type.size + entry.comment.size + entry.value.size;
    if(combined_size > 0)
    {
        isize allocated_size = combined_size + 4;
        u8* data = allocator_allocate(dyn.allocator, allocated_size, DEF_ALIGN, SOURCE_INFO());
        dyn.text_parts = (char*) data;

        isize curr_pos = 0;

        dyn.label_size = entry.label.size;
        memcpy(data + curr_pos, entry.label.data, entry.label.size);
        curr_pos += dyn.label_size;
        data[curr_pos ++] = 0;
        
        dyn.type_size = entry.type.size;
        memcpy(data + curr_pos, entry.type.data, entry.type.size);
        curr_pos += dyn.type_size;
        data[curr_pos ++] = 0;
        
        dyn.comment_size = entry.comment.size;
        memcpy(data + curr_pos, entry.comment.data, entry.comment.size);
        curr_pos += dyn.comment_size;
        data[curr_pos ++] = 0;
        
        dyn.value_size = entry.value.size;
        memcpy(data + curr_pos, entry.value.data, entry.value.size);
        curr_pos += dyn.value_size;
        data[curr_pos ++] = 0;

        ASSERT(curr_pos == allocated_size);
    }

    //We dont copy fake children.
    //dyn.children = entry->children;
    //dyn.children_size = entry->children_size;
    //dyn.children_capacity = entry->children_capacity;

    return dyn;
}

Lpf_Entry lpf_entry_from_dyn_entry(Lpf_Dyn_Entry dyn)
{
    Lpf_Entry entry = {0};

    entry.kind = dyn.kind;
    entry.error = dyn.error;
    entry.line_number = dyn.line_number;
    entry.depth = dyn.depth;
    entry.format_flags = dyn.format_flags;

    entry.children = dyn.children;
    entry.children_size = dyn.children_size;
    entry.children_capacity = dyn.children_capacity;

    if(dyn.text_parts != NULL)
    {
        isize label_from = 0;
        isize label_to = dyn.label_size;

        isize type_from = label_to + 1;
        isize type_to = type_from + dyn.type_size;

        isize comment_from = type_to + 1;
        isize comment_to = comment_from + dyn.comment_size;
                
        isize value_from = comment_to + 1;
        isize value_to = value_from + dyn.value_size;

        isize comnined_size = dyn.label_size + dyn.type_size + dyn.comment_size + dyn.value_size;
        String whole_text = {dyn.text_parts, comnined_size + 4};

        entry.label = string_range(whole_text, label_from, label_to);
        entry.type = string_range(whole_text, type_from, type_to);
        entry.value = string_range(whole_text, value_from, value_to);
        entry.comment = string_range(whole_text, comment_from, comment_to);
    }

    return entry;
}

void lpf_reader_commit_entries(Lpf_Reader* reader)
{
    if(reader->has_last_entry)
    {
        Lpf_Entry last = reader->last_entry;
        last.value = string_from_builder(reader->last_value);
        last.comment = string_from_builder(reader->last_comment);

        Lpf_Dyn_Entry* parent = (Lpf_Dyn_Entry*) *array_last(reader->scopes);
        lpf_dyn_entry_push(parent, last);
        
        Lpf_Entry null_entry = {0};
        reader->last_entry = null_entry;
        array_clear(&reader->last_comment);
        array_clear(&reader->last_value);
    }
    reader->has_last_entry = false;
}

void lpf_reader_queue_entry(Lpf_Reader* reader, Lpf_Entry entry, const Lpf_Format_Options* options)
{
    lpf_reader_commit_entries(reader);

    if(entry.kind != LPF_KIND_COMMENT && options->skip_inline_comments)
        entry.comment = STRING("");

    if(options->skip_types)
        entry.type = STRING("");

    reader->has_last_entry = true;
    reader->last_entry = entry;
    builder_append(&reader->last_comment, entry.comment);
    builder_append(&reader->last_value, entry.value);
}
    
Lpf_Error lpf_read_entry(Lpf_Reader* reader, Lpf_Dyn_Entry* into, Lpf_Entry entry, const Lpf_Format_Options* options)
{
    if(reader->scopes.size == 0)
        array_push(&reader->scopes, into);

    Lpf_Kind last_kind = reader->last_entry.kind;

    reader->line_number += 1;
    entry.line_number = reader->line_number;
    entry.depth = reader->depth;

    if(entry.error != LPF_ERROR_NONE)
    {
        if(options->skip_errors)
            goto function_end;
    }
    switch(entry.kind)
    {
        case LPF_KIND_BLANK: {
            if(options->skip_blanks)
                lpf_reader_commit_entries(reader);
            else
            {
                if(reader->has_last_entry && last_kind == LPF_KIND_BLANK)
                {
                    //** nothing **
                }
                else
                {
                    lpf_reader_queue_entry(reader, entry, options);
                }
            }
        } break;

        case LPF_KIND_COMMENT: {
            if(options->skip_comments)
                lpf_reader_commit_entries(reader);
            else
            {
                if(reader->has_last_entry && last_kind == LPF_KIND_COMMENT)
                {
                    builder_append(&reader->last_comment, entry.value);
                    builder_append(&reader->last_comment, STRING("\n"));
                }
                else
                {
                    lpf_reader_queue_entry(reader, entry, options);
                }
            }
        } break;
            
        case LPF_KIND_ENTRY: {
            lpf_reader_queue_entry(reader, entry, options);
        } break;

        case LPF_KIND_CONTINUATION:
        case LPF_KIND_ESCAPED_CONTINUATION: {
            bool was_last_proper_continaution = reader->has_last_entry 
                &&  (last_kind == LPF_KIND_ENTRY 
                    || last_kind == LPF_KIND_CONTINUATION 
                    || last_kind == LPF_KIND_ESCAPED_CONTINUATION);

            if(was_last_proper_continaution)
            {
                if(entry.kind == LPF_KIND_CONTINUATION)
                    builder_append(&reader->last_value, STRING("\n"));
                builder_append(&reader->last_value, entry.value);

                if(entry.comment.size > 0)
                {
                    builder_append(&reader->last_comment, entry.value);
                    builder_append(&reader->last_comment, STRING("\n"));
                }
            }
            else
            {
                if(options->correct_errors)
                    entry.kind = LPF_KIND_ENTRY;
                else if(options->skip_errors)
                    goto function_end;
                else
                    entry.error = LPF_ERROR_ENTRY_CONTINUNATION_WITHOUT_START;
                    
                lpf_reader_queue_entry(reader, entry, options);
            }
        } break;

        case LPF_KIND_SCOPE_START: {
            reader->depth += 1;
            
            lpf_reader_commit_entries(reader);
            if(options->skip_scopes == false)
            {
                lpf_reader_queue_entry(reader, entry, options);
                lpf_reader_commit_entries(reader);
            
                Lpf_Dyn_Entry* parent = (Lpf_Dyn_Entry*) *array_last(reader->scopes); 
                Lpf_Dyn_Entry* pushed_scope = &parent->children[parent->children_size - 1];
                array_push(&reader->scopes, pushed_scope);
                ASSERT(pushed_scope->depth == entry.depth);
                ASSERT(pushed_scope->line_number == entry.line_number);
            }
        } break;

        case LPF_KIND_SCOPE_END: {
            lpf_reader_commit_entries(reader);
            if(options->skip_scopes == false)
            {
                if(options->skip_scope_ends == false)
                {
                    lpf_reader_queue_entry(reader, entry, options);
                    lpf_reader_commit_entries(reader);
                }

                if(reader->scopes.size >= 1)
                    array_pop(&reader->scopes);
            }
            

            if(reader->depth <= 0)
                entry.error = LPF_ERROR_SCOPE_TOO_MANY_ENDS;
            reader->depth = MAX(reader->depth - 1, 0);
        } break;
    }

    function_end:
    return entry.error;
}

Lpf_Error lpf_read_custom(String source, Lpf_Dyn_Entry* into, const Lpf_Format_Options* options)
{
    Lpf_Reader reader = {0};
    isize last_source_i = 0;
    Lpf_Error last_error = LPF_ERROR_NONE;
    while(true)
    {
        Lpf_Entry entry = {0};
        isize next_source_i = lpf_lowlevel_read_entry(source, last_source_i, &entry);
        if(last_source_i == next_source_i)
            break;

        Lpf_Error error = lpf_read_entry(&reader, into, entry, options);
        if(error != LPF_ERROR_NONE)
        {
            last_error = error;
            if(options->log_errors)
            {
                String line = string_range(source, last_source_i, next_source_i);
                LOG_ERROR("LPF", "Error %s reading lpf file on line %lli depth %lli", lpf_error_to_string(entry.error), (lli) entry.line_number, (lli) entry.depth);
                log_group_push();
                    LOG_ERROR("LPF", STRING_FMT, STRING_PRINT(line));
                log_group_pop();
            }

            if(options->stop_on_error)
                break;
        }

        last_source_i = next_source_i;
    }

    lpf_reader_commit_entries(&reader);
    lpf_reader_deinit(&reader);

    return last_error;
}

void lpf_write_custom(String_Builder* source, Lpf_Dyn_Entry root, const Lpf_Format_Options* options)
{
    typedef struct Iterator {
        const Lpf_Dyn_Entry* scope;
        isize i;
    } Iterator;

    DEFINE_ARRAY_TYPE(Iterator, Iterator_Array);

    Lpf_Writer writer = {0};
    Allocator_Set prev_allocs = allocator_set_default(allocator_get_scratch());
    
    Iterator_Array iterators = {0};
    array_init_backed(&iterators, NULL, 32);

    Iterator top_level_it = {&root, 0};
    array_push(&iterators, top_level_it);

    while(iterators.size > 0)
    {
        bool going_deeper = false;
        bool had_explicit_ending = false;

        Iterator* it = array_last(iterators);
        for(; it->i < it->scope->children_size; )
        {
            const Lpf_Dyn_Entry* dyn = &it->scope->children[it->i];
            Lpf_Entry entry = lpf_entry_from_dyn_entry(*dyn);
            lpf_write_entry(&writer, source, entry, options);

            it->i += 1;
            if(entry.kind == LPF_KIND_SCOPE_START)
            {
                Iterator child_level_it = {dyn, 0};
                array_push(&iterators, child_level_it);
                going_deeper = true;
                break;
            }
            if(entry.kind == LPF_KIND_SCOPE_END)
            {
                had_explicit_ending = true;
                break;
            }
        }

        if(going_deeper == false)
        {
            array_pop(&iterators);
            if(had_explicit_ending == false && iterators.size > 0)
            {
                Lpf_Entry entry = {LPF_KIND_SCOPE_END};
                lpf_write_entry(&writer, source, entry, options);
            }
        }
    }

    array_deinit(&iterators);
    allocator_set(prev_allocs);
}

Lpf_Error lpf_read_all(String source, Lpf_Dyn_Entry* root)
{
    Lpf_Format_Options options = lpf_make_default_format_options();
    return lpf_read_custom(source, root, &options);
}

Lpf_Error lpf_read_meaningful(String source, Lpf_Dyn_Entry* root)
{
    Lpf_Format_Options options = lpf_make_default_format_options();
    options.skip_blanks = true;
    options.skip_comments = true;
    options.skip_inline_comments = true;
    options.skip_errors = true;
    options.skip_scope_ends = true;
    options.correct_errors = true;
    return lpf_read_custom(source, root, &options);
}

Lpf_Error lpf_read(String source, Lpf_Dyn_Entry* root)
{
    Lpf_Format_Options options = lpf_make_default_format_options();
    options.correct_errors = true;
    options.skip_blanks = true;
    options.skip_scope_ends = true;
    return lpf_read_custom(source, root, &options);
}

void lpf_write(String_Builder* builder, Lpf_Dyn_Entry root)
{
    Lpf_Format_Options options = lpf_make_default_format_options();
    lpf_write_custom(builder, root, &options);
}

void lpf_write_meaningful(String_Builder* builder, Lpf_Dyn_Entry root)
{
    Lpf_Format_Options options = lpf_make_default_format_options();
    options.skip_blanks = true;
    options.skip_comments = true;
    options.skip_inline_comments = true;
    options.skip_errors = true;
    lpf_write_custom(builder, root, &options);
}