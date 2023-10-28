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
    LPF_FLAG_WHITESPACE_AGNOSTIC = 8 | LPF_FLAG_WHITESPACE_PREFIX_AGNOSTIC | LPF_FLAG_WHITESPACE_POSTFIX_AGNOSTIC, //whitespace and newlines dont matter (as long as there is at least one)
};

typedef struct Lpf_Entry {
    Lpf_Kind kind;
    Lpf_Error error;

    String label;
    String type;
    String value;
    String comment;

    isize line_number;
    isize depth;
} Lpf_Entry;

typedef struct Lpf_Dyn_Entry Lpf_Dyn_Entry; 

DEFINE_ARRAY_TYPE(Lpf_Dyn_Entry, Lpf_Dyn_Entry_Array);
DEFINE_ARRAY_TYPE(Lpf_Entry, Lpf_Entry_Array);

typedef struct Lpf_Dyn_Entry {
    Lpf_Kind kind;
    Lpf_Error error;

    String_Builder label;
    String_Builder type;
    String_Builder value;
    String_Builder comment;

    Lpf_Dyn_Entry_Array children;
    Lpf_Dyn_Entry* parent;
    
    isize depth;
    isize line_number;
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

typedef struct Lpf_Writer {
    String_Builder* builder;
    
    isize max_value_size;
    isize max_comment_size;
    i32 pad_prefix_to;

    i32 line_identation_per_level;
    i32 comment_identation_per_level;

    i32 line_indentation_offset;
    i32 comment_indentation_offset;

    i32 line_indentation;
    i32 comment_indentation;
    
    String hash_escape;
    bool pad_continuations;

    bool skip_comments;
    bool skip_inline_comments;
    bool skip_blanks;
    bool skip_scopes;
    bool skip_types;
    
    //@TODO: SKIP ERRORS!
    //@TODO: log errors
    
    i32 written_entries;
} Lpf_Writer;

typedef struct Lpf_Reader {
    bool skip_comments;
    bool skip_inline_comments;
    bool skip_blanks;
    bool skip_scopes;
    bool skip_types;
    bool skip_errors;

    bool log_errors;
    bool correct_errors;
    bool stop_on_error;

    String source;
    isize source_i;

    Lpf_Error last_error;
    Lpf_Dyn_Entry* parent;
    isize depth;
    isize line_number;
} Lpf_Reader;

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

isize lpf_read_lowlevel_entry(String source, Lpf_Entry* parsed)
{
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
    return line_end;
}

void builder_pad_to(String_Builder* builder, isize to_size, char with)
{
    if(builder->size >= to_size)
        return;

    isize size_before = array_resize(builder, to_size);
    memset(builder->data + size_before, with, builder->size - size_before);

    //for(isize i = size_before; i < builder->size; i++)
        //builder->data[i] = with;
}

bool lpf_is_prefix_allowed_char(char c)
{
    if(char_is_space(c))
        return false;

    if(c == ':' || c == ',' || c == '#' ||c == ';' || c == '{' || c == '}')
        return false;

    return true;
}

isize lpf_write_entry_unescaped(String_Builder* builder, Lpf_Entry entry, Lpf_Write_Options options)
{
    array_grow(builder, builder->size + 30 + entry.value.size + entry.comment.size + entry.type.size + entry.label.size);

    #ifdef DO_ASSERTS
        for(isize i = 0; i < entry.label.size; i++)
            ASSERT_MSG(lpf_is_prefix_allowed_char(entry.label.data[i]), 
                "label must contain only valid data! Label: " STRING_FMT, STRING_PRINT(entry.label));

        for(isize i = 0; i < entry.type.size; i++)
            ASSERT_MSG(lpf_is_prefix_allowed_char(entry.type.data[i]),
                "type must contain only valid data! Label: " STRING_FMT, STRING_PRINT(entry.type));

        ASSERT_MSG(string_find_first_char(entry.value, '\n', 0) == -1, "value must not contain newlines. Value: \""STRING_FMT"\"", STRING_PRINT(entry.value));
        ASSERT_MSG(string_find_first_char(entry.comment, '\n', 0) == -1, "comment must not contain newlines. Value: \""STRING_FMT"\"", STRING_PRINT(entry.comment));
        if(entry.kind != LPF_KIND_COMMENT)
            ASSERT_MSG(string_find_first_char(entry.comment, '\n', 0) == -1, "comment must not contain #. Value: \""STRING_FMT"\"", STRING_PRINT(entry.comment));
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

isize _lpf_write_lowlevel_entry(String_Builder* builder, Lpf_Entry entry, const Lpf_Write_Options* options_or_null)
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
    
    isize prefix_size = lpf_write_entry_unescaped(builder, escaped, options);
    
    array_deinit(&escaped_label_builder);
    array_deinit(&escaped_type_builder);

    return prefix_size;
}

void lpf_write_lowlevel_entry(String_Builder* builder, Lpf_Entry entry, const Lpf_Write_Options* options_or_null)
{
    _lpf_write_lowlevel_entry(builder, entry, options_or_null);
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

Lpf_Writer lpf_writer_make(String_Builder* builder)
{
    Lpf_Writer writer = {0};
    writer.builder = builder;
    writer.line_identation_per_level = 4;
    writer.comment_identation_per_level = 2;
    writer.comment_indentation_offset = 0;
    writer.pad_continuations = true;
    writer.max_value_size = INT64_MAX;
    writer.max_comment_size = INT64_MAX;
    writer.hash_escape = STRING(":hashtag:");

    return writer;
}

void lpf_write_entry_custom(Lpf_Writer* writer, Lpf_Entry entry, i64 flags)
{
    Lpf_Kind kind = entry.kind;
    String label = entry.label;
    String type = entry.type;
    String value = entry.value;
    String comment = entry.comment;
    
    if(writer->skip_blanks && kind == LPF_KIND_BLANK)
        return;
    if(writer->skip_scopes && (kind == LPF_KIND_SCOPE_START || kind == LPF_KIND_SCOPE_END))
        return;
    if(writer->skip_comments && kind == LPF_KIND_COMMENT)
        return;

    if(kind == LPF_KIND_BLANK)
    {
        if(writer->skip_blanks == false)
        {
            Lpf_Write_Options options = {0};
            lpf_write_entry_unescaped(writer->builder, entry, options);
        }

        return;
    }

    if(writer->skip_types)
        type = STRING("");

    if(writer->skip_inline_comments && kind != LPF_KIND_COMMENT)
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
    options.comment_indentation = writer->comment_identation_per_level*writer->comment_indentation + writer->comment_indentation_offset;
    options.line_indentation = writer->line_identation_per_level*writer->line_indentation + writer->line_indentation_offset;
    options.pad_prefix_to = writer->pad_prefix_to;
    options.put_space_before_marker = true;
    options.put_space_before_comment = !!(flags & LPF_FLAG_WHITESPACE_POSTFIX_AGNOSTIC);
    options.comment_terminate_value = !!(flags & LPF_FLAG_WHITESPACE_SENSITIVE);
    
    if(flags & LPF_FLAG_WHITESPACE_PREFIX_AGNOSTIC)
        value = string_trim_prefix_whitespace(value); 
                    
    if(flags & LPF_FLAG_WHITESPACE_POSTFIX_AGNOSTIC)
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
                    builder_append(&escaped_inline_comment, writer->hash_escape);

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
            options.line_indentation -= writer->line_identation_per_level;
            writer->line_indentation -= 1;
        }

        Lpf_Entry continuation = {kind};
        continuation.type = type;
        continuation.label = label;
        continuation.comment = comment;

        options.put_space_before_comment = true;
        lpf_write_lowlevel_entry(writer->builder, continuation, &options);
        
        if(entry.kind == LPF_KIND_SCOPE_START)
            writer->line_indentation += 1;
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
        lpf_split_into_segments(&comment_segments, comment, writer->max_comment_size);
        for(isize i = 0; i < comment_segments.size; i++)
        {
            Lpf_Segment seg = comment_segments.data[i];
            Lpf_Entry continuation = {LPF_KIND_COMMENT};
            continuation.comment = seg.string;
            lpf_write_lowlevel_entry(writer->builder, continuation, &options);
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
            had_too_long = lpf_split_into_segments(&value_segments, value, writer->max_value_size);
            
        if(value_segments.size > 1)
            if((flags & LPF_FLAG_WHITESPACE_AGNOSTIC) == 0)
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
        
            isize padded_prefix = _lpf_write_lowlevel_entry(writer->builder, continuation, &options);
            if(writer->pad_continuations)
                options.pad_prefix_to = padded_prefix;
        }
    }
    
    array_deinit(&escaped_inline_comment);
    array_deinit(&value_segments);
    array_deinit(&comment_segments);
}




Lpf_Reader lpf_reader_make(String source)
{
    Lpf_Reader reader = {0};
    reader.source = source;
    reader.skip_errors = true;
    reader.skip_blanks = true;
    reader.log_errors = true;
    reader.correct_errors = true;

    return reader;
}

//void scope_read_write_texture(Scope* scope, const char* label ,Texture* tex, Serialize_Read_Write rw)
//{
//  Scope* sub_scope = NULL
//  scope_read_write_scope(scope, label, "TEX", &sub_scope, rw);
//  {
//      scope_read_write_vec4(sub_scope,    "offset",   &tex->offset, vec4(0,0,0,0), rw);
//      scope_read_write_string(sub_scope,  "path",     &tex->path, STRING(""), rw);
//      scope_read_write_name(sub_scope,    "name",     &tex->path, STRING(""), rw);
//      bool has_id = scope_read_write_id(sub_scope,      "id",       &tex->offset, id_generate(), rw);
// 
//      if(has_id == false)
//      {
//          LOG_ERROR("", "texture does not have ID!");
//      }
//  }
//}

void lpf_dyn_entry_from_entry(Lpf_Dyn_Entry* dyn, const Lpf_Entry* entry);

void lpf_entry_from_dyn_entry(Lpf_Entry* entry, const Lpf_Dyn_Entry* dyn)
{
    entry->label = string_from_builder(dyn->label);
    entry->type = string_from_builder(dyn->type);
    entry->value = string_from_builder(dyn->value);
    entry->comment = string_from_builder(dyn->comment);
    
    entry->kind = dyn->kind;
    entry->error = dyn->error;
    entry->line_number = dyn->line_number;
    entry->depth = dyn->depth;
}

void lpf_reader_push_child(Lpf_Reader* reader, Lpf_Dyn_Entry_Array* into, Lpf_Entry entry)
{
    if(entry.kind != LPF_KIND_COMMENT && reader->skip_inline_comments)
        entry.comment = STRING("");

    if(reader->skip_types)
        entry.type = STRING("");
        
    Allocator* alloc = allocator_get_default();
    String_Builder null_builder = {alloc};

    String label = entry.label;
    String type = entry.type;
    String value = entry.value;
    String comment = entry.comment;

    Lpf_Dyn_Entry dyn = {0};
    dyn.label      = label.size == 0   ? null_builder : builder_from_string(label, alloc);
    dyn.type       = type.size == 0    ? null_builder : builder_from_string(type, alloc);
    dyn.value      = value.size == 0   ? null_builder : builder_from_string(value, alloc);
    dyn.comment    = comment.size == 0 ? null_builder : builder_from_string(comment, alloc);
    array_init(&dyn.children, alloc);

    dyn.kind = entry.kind;
    dyn.error = entry.error;
    dyn.line_number = entry.line_number;
    dyn.depth = entry.depth;
           
    dyn.parent = reader->parent;
    array_push(into, dyn);
}
    
Lpf_Error lpf_read_entry(Lpf_Reader* reader, Lpf_Dyn_Entry_Array* into, Lpf_Dyn_Entry* first_level_parent)
{
    if(reader->last_error != LPF_ERROR_NONE && reader->stop_on_error)
        return reader->last_error;

    if(reader->source_i >= reader->source.size)
        return LPF_ERROR_NONE;

    Lpf_Dyn_Entry_Array* parent_children = into;
    if(reader->parent != NULL)
        parent_children = &reader->parent->children;
    else
        reader->parent = first_level_parent;

    Lpf_Dyn_Entry* last_entry = NULL;
    if(parent_children->size > 0)
    {
        last_entry = array_last(*parent_children);

        //If had something in between its not a proper last entry
        if(last_entry->line_number != reader->line_number)
            last_entry = NULL;
    }

    Lpf_Entry entry = {0}; 
    isize line_i = reader->source_i;
    String source_rest = string_tail(reader->source, line_i);
    isize next_line_i = lpf_read_lowlevel_entry(source_rest, &entry);
    reader->line_number += 1;
    reader->source_i += next_line_i;

    entry.line_number = reader->line_number;
    entry.depth = reader->depth;

    if(entry.error != LPF_ERROR_NONE)
    {
        if(reader->skip_errors)
            goto function_end;
    }

    switch(entry.kind)
    {
        case LPF_KIND_BLANK: {
            if(reader->skip_blanks)
                goto function_end;

            if(last_entry && last_entry->kind == LPF_KIND_BLANK)
            {
                //builder_append(&reader->last_entry_data, entry.value);
            }
            else
            {
                lpf_reader_push_child(reader, parent_children, entry);
            }
        } break;

        case LPF_KIND_COMMENT: {
            if(reader->skip_comments)
                goto function_end;

            if(last_entry && last_entry->kind == LPF_KIND_COMMENT)
            {
                builder_append(&last_entry->comment, entry.value);
            }
            else
            {
                lpf_reader_push_child(reader, parent_children, entry);
            }
        } break;
            
        case LPF_KIND_ENTRY: {
            lpf_reader_push_child(reader, parent_children, entry);
        } break;

        case LPF_KIND_CONTINUATION:
        case LPF_KIND_ESCAPED_CONTINUATION: {
            bool was_last_proper_continaution = last_entry 
                &&  (last_entry->kind == LPF_KIND_ENTRY 
                    || last_entry->kind == LPF_KIND_CONTINUATION 
                    || last_entry->kind == LPF_KIND_ESCAPED_CONTINUATION);

            if(was_last_proper_continaution)
            {
                if(entry.kind == LPF_KIND_CONTINUATION)
                    builder_append(&last_entry->value, STRING("\n"));
                builder_append(&last_entry->value, entry.value);
            }
            else
            {
                entry.error = LPF_ERROR_ENTRY_CONTINUNATION_WITHOUT_START;
                if(reader->correct_errors)
                    entry.kind = LPF_KIND_ENTRY;
                else if(reader->skip_errors)
                    goto function_end;

                lpf_reader_push_child(reader, parent_children, entry);
            }
        } break;

        case LPF_KIND_SCOPE_START: {
            if(reader->skip_scopes)
                goto function_end;

            lpf_reader_push_child(reader, parent_children, entry);

            reader->parent = array_last(*parent_children);
            reader->depth += 1;

            ASSERT(reader->parent->depth == entry.depth);
            ASSERT(reader->parent->line_number == entry.line_number);
        } break;

        case LPF_KIND_SCOPE_END: {
            if(reader->skip_scopes)
                goto function_end;

            if(reader->parent == NULL || reader->depth < 0)
            {
                entry.error = LPF_ERROR_SCOPE_TOO_MANY_ENDS;

                reader->parent = first_level_parent;
                reader->depth = 0;
            }
            else
            {
                Lpf_Dyn_Entry* grandparent = reader->parent->parent;
                reader->parent = grandparent;
                reader->depth -= 1;
            }
        } break;
    }

    function_end:

    if(entry.error != LPF_ERROR_NONE)
        reader->last_error = entry.error;

    if(entry.error != LPF_ERROR_NONE && reader->log_errors)
    {
        String line = string_range(reader->source, line_i, line_i + next_line_i);
        LOG_ERROR("LPF", "Error %s reading lpf file on line %lli depth %lli", lpf_error_to_string(entry.error), (lli) entry.line_number, (lli) entry.depth);
        log_group_push();
            LOG_ERROR("LPF", STRING_FMT, STRING_PRINT(line));
        log_group_pop();
    }

    return entry.error;
}

Lpf_Error lpf_read_custom(Lpf_Reader* reader, Lpf_Dyn_Entry_Array* into, Lpf_Dyn_Entry* first_level_parent)
{
    isize last_source_i = -1;
    while(reader->source_i < reader->source.size && reader->source_i != last_source_i)
    {
        lpf_read_entry(reader, into, first_level_parent);
    }

    return reader->last_error;
}

Lpf_Error lpf_read_all(Lpf_Dyn_Entry_Array* into, String source)
{
    Lpf_Reader reader = {0};
    reader.log_errors = true;
    reader.source = source;
    return lpf_read_custom(&reader, into, NULL);
}


Lpf_Error lpf_read_meaningful(Lpf_Dyn_Entry_Array* into, String source)
{
    Lpf_Reader reader = lpf_reader_make(source);
    reader.skip_comments = true;
    reader.skip_inline_comments = true;
    return lpf_read_custom(&reader, into, NULL);
}

Lpf_Error lpf_read(Lpf_Dyn_Entry_Array* into, String source)
{
    Lpf_Reader reader = lpf_reader_make(source);
    return lpf_read_custom(&reader, into, NULL);
}

void lpf_write_custom(Lpf_Writer* writer, const Lpf_Dyn_Entry_Array* entries)
{
    typedef struct Iterator {
        const Lpf_Dyn_Entry_Array* arr;
        isize i;
    } Iterator;

    DEFINE_ARRAY_TYPE(Iterator, Iterator_Array);

    Allocator_Set prev_allocs = allocator_set_default(allocator_get_scratch());
    
    Iterator_Array iterators = {0};
    array_init_backed(&iterators, NULL, 32);

    Iterator top_level_it = {entries, 0};
    array_push(&iterators, top_level_it);

    while(iterators.size > 0)
    {
        bool going_deeper = false;
        Iterator* it = array_last(iterators);
        for(; it->i < it->arr->size; )
        {
            const Lpf_Dyn_Entry* dyn = &it->arr->data[it->i];
            Lpf_Entry entry = {0};
            lpf_entry_from_dyn_entry(&entry, dyn);
            lpf_write_entry_custom(writer, entry, 0);

            it->i += 1;
            if(entry.kind == LPF_KIND_SCOPE_START)
            {
                Iterator child_level_it = {&dyn->children, 0};
                array_push(&iterators, child_level_it);
                going_deeper = true;
                break;
            }
        }

        if(going_deeper == false)
        {
            array_pop(&iterators);
            if(iterators.size > 0)
            {
                Lpf_Entry entry = {LPF_KIND_SCOPE_END};
                lpf_write_entry_custom(writer, entry, 0);
            }
        }
    }

    array_deinit(&iterators);
    allocator_set(prev_allocs);
}

void lpf_write(String_Builder* builder, const Lpf_Dyn_Entry_Array* entries)
{
    Lpf_Writer writer = lpf_writer_make(builder);
    lpf_write_custom(&writer, entries);
}

void lpf_write_meaningful(String_Builder* builder, const Lpf_Dyn_Entry_Array* entries)
{
    Lpf_Writer writer = lpf_writer_make(builder);
    writer.skip_blanks = true;
    writer.skip_comments = true;
    writer.skip_inline_comments = true;
    lpf_write_custom(&writer, entries);
}