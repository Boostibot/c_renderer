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

typedef struct Lpf_Scope Lpf_Scope;

DEFINE_ARRAY_TYPE(Lpf_Entry, Lpf_Entry_Array);
DEFINE_ARRAY_TYPE(Lpf_Scope, Lpf_Scope_Array);

typedef struct Lpf_Scope {
    String label;
    String type;

    isize first_child_i1;
    isize last_child_i1;
    isize next_i1;
    isize prev_i1;
    isize parent_i1;
    
    isize children_count;

    isize line_number;
    isize depth;
} Lpf_Scope;


typedef struct Lpf_Write_Options {
    isize line_indentation;
    isize comment_indentation;
    
    isize pad_prefix_to;

    bool put_space_before_marker;
    bool comment_terminate_value;
} Lpf_Write_Options;

typedef struct Lpf_Writer {
    String_Builder* builder;
    
    isize max_value_size;
    isize max_comment_size;

    i32 line_identation_per_level;
    i32 comment_identation_per_level;
    i32 comment_indentation_offset;

    i32 line_indentation;
    i32 comment_indentation;
    
    i32 pad_prefix_to;
    i32 max_value_size;
    i32 max_comment_size;
    
    bool set_escapes;
    String hash_escape;
    char label_escape;
    char type_escape;
    bool pad_continuations;

    bool skip_comments;
    bool skip_inline_comments;
    bool skip_blanks;
    bool skip_scopes;
    bool skip_types;
    
    i32 written_entries;
} Lpf_Writer;

typedef struct Lpf_Read_Options {
    bool skip_comments;
    bool skip_inline_comments;
    bool skip_blanks;
    bool skip_scopes;
    bool skip_types;
} Lpf_Read_Options;

isize lpf_read_lowlevel_entry(String source, Lpf_Entry* parsed, const Lpf_Read_Options* options_or_null);
void lpf_write_lowlevel_entry(String_Builder* builder, Lpf_Entry entry);


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

    isize line_size = string_find_first_char(source, '\n', 0);
    if(line_size == -1)
        line_size = source.size;

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
    return line_size;
}

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

isize lpf_write_entry_unescaped(String_Builder* builder, Lpf_Entry entry, Lpf_Write_Options options)
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
        array_push(builder, '#');
        builder_pad_to(builder, builder->size + options.comment_indentation, ' ');
        builder_append(builder, entry.comment);
    }
    else if(options.comment_terminate_value)
        array_push(builder, '#');
    
    array_push(builder, '\n');

    return prefix_size;
}

String lpf_escape_label_or_type(String_Builder* into, String label_or_line, char escape)
{
    builder_assign(into, string_trim_whitespace(label_or_line));
    for(isize i = 0; i < into->size; i++)
    {
        char* c = &into->data[i];
        if(lpf_is_prefix_allowed_char(*c) == false)
            *c = escape;
    }

    return string_from_builder(*into);
}

typedef enum {
    LPF_SPLIT_NEWLINE, LPF_SPLIT_TOO_LONG 
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
        Lpf_Split_Reason reason = LPF_SPLIT_NEWLINE;
        while(line.size > max_size)
        {
            Lpf_Segment segment = {0};
            segment.split_reason = reason;
            segment.string = string_head(line, max_size);
            
            array_push(segemnts, segment);
            line = string_tail(line, max_size);
            reason = LPF_SPLIT_TOO_LONG;
        }
        
        Lpf_Segment last_segemnt = {0};
        last_segemnt.split_reason = reason;
        last_segemnt.string = line;
        array_push(segemnts, last_segemnt);
    }
}

void lpf_write_lowlevel_entry(String_Builder* builder, Lpf_Entry entry, const Lpf_Write_Options* options_or_null)
{
    Lpf_Write_Options options = {0};

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

        case LPF_KIND_SCOPE_START: {
            value = STRING("");
        } break;

        case LPF_KIND_SCOPE_END: {
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
    String_Builder escaped_inline_comment = {0};

    Allocator* scratch = allocator_get_scratch();

    array_init_backed(&escaped_label_builder, scratch, LOCAL_BUFF);
    array_init_backed(&escaped_type_builder, scratch, LOCAL_BUFF);
    array_init_backed(&escaped_inline_comment, scratch, LOCAL_BUFF);
    array_init_backed(&value_segments, scratch, SEGMENTS);
    array_init_backed(&comment_segments, scratch, SEGMENTS);

    array_grow(builder, builder->size + 30 + value.size + comment.size + type.size + label.size);
    
    //Escape label
    if(label.size > 0)
        label = lpf_escape_label_or_type(&escaped_label_builder, label, options.label_escape);
    
    //Escape type
    if(type.size > 0)
    {
        type = lpf_escape_label_or_type(&escaped_type_builder, type, options.type_escape);

        if(label.size == 0)
            label = STRING("_");
    }
    
    //Escape the inline comment
    if(comment.size > 0 && entry.kind != LPF_KIND_COMMENT)
    {
        comment = string_trim_postfix_whitespace(comment);
        for(Line_Iterator line_it = {0}; line_iterator_get_line(&line_it, comment); )
        {
            String line = line_it.line;
            if(line_it.line_number != 1)
                array_push(&escaped_inline_comment, ' ');
            
            for(isize i = 0; i <= line.size; i++)
            {
                isize next_i = string_find_first_char(line, '#', i);
                if(next_i == -1)
                    next_i = line.size;
                
                if(i != 0)
                    builder_append(&escaped_inline_comment, options.hash_escape);

                String current_range = string_range(line, i, next_i);
                builder_append(&escaped_inline_comment, current_range);
                i = next_i;
            }
        }

        comment = string_from_builder(escaped_inline_comment);
    }

    Lpf_Write_Options downstream_options = options;
    if(entry.kind == LPF_KIND_COMMENT)
    {
        //Split comment into segments (by lines and so that its shorter then max_value_size)
        lpf_split_into_segments(&comment_segments, comment, options.max_comment_size);
        for(isize i = 0; i < comment_segments.size; i++)
        {
            Lpf_Segment seg = comment_segments.data[i];
            Lpf_Entry continuation = {LPF_KIND_COMMENT};
            continuation.comment = seg.string;
            if(seg.split_reason == LPF_SPLIT_TOO_LONG)
                continuation.comment = string_trim_prefix_whitespace(seg.string);

            lpf_write_entry_unescaped(builder, continuation, downstream_options);
        }

    }
    else if(entry.kind == LPF_KIND_SCOPE_END || entry.kind == LPF_KIND_SCOPE_START)
    {
        Lpf_Entry continuation = {entry.kind};
        continuation.type = type;
        continuation.label = label;
        continuation.comment = comment;

        lpf_write_entry_unescaped(builder, continuation, downstream_options);
    }
    else
    {
        //Split value into segments (by lines and so that its shorter then max_value_size)
        if(value.size > 0)
            lpf_split_into_segments(&value_segments, value, options.max_value_size);

        for(isize i = 0; i < value_segments.size; i++)
        {
            Lpf_Entry continuation = {LPF_KIND_CONTINUATION};
            Lpf_Segment segment = value_segments.data[i];
            continuation.value = segment.string;
            if(segment.split_reason == LPF_SPLIT_TOO_LONG)
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
                continuation.comment = comment;
        
            isize padded_prefix = lpf_write_entry_unescaped(builder, continuation, downstream_options);
            if(options.pad_continuations)
                downstream_options.pad_prefix_to = padded_prefix;
        }
    }
    
    array_deinit(&escaped_label_builder);
    array_deinit(&escaped_type_builder);
    array_deinit(&value_segments);
    array_deinit(&comment_segments);
}



enum {
    LPF_FLAG_NO_SPACE_BEFORE_VALUE = 1,
    LPF_FLAG_NO_SPACE_BEFORE_COMMENT = 2,
    LPF_FLAG_SPACE_BEFORE_MARKER = 4,
    LPF_FLAG_COMMENT_TERMINATE = 8,
};

Lpf_Writer lpf_writer_make(String_Builder* builder)
{
    Lpf_Writer writer = {0};
    writer.builder = builder;
    writer.line_identation_per_level = 4;
    writer.comment_identation_per_level = 2;
    writer.comment_indentation_offset = 1;

    return writer;
}

void lpf_write_entry_custom(Lpf_Writer* writer, String label, String type, String value, String inline_comment, i64 flags)
{
    Lpf_Entry entry = {LPF_KIND_ENTRY};
    entry.label = label;
    entry.value = value;
    if(writer->skip_inline_comments == false)
        entry.comment = inline_comment;
    if(writer->skip_types == false)
        entry.type = type;

    Lpf_Write_Options options = {0};
    options.line_indentation = writer->line_identation_per_level * writer->line_indentation;
    options.comment_indentation = writer->comment_identation_per_level * writer->comment_indentation + writer->comment_indentation_offset;
    
    options.pad_prefix_to = writer->pad_prefix_to;
    options.max_value_size = writer->max_value_size;
    options.max_comment_size = writer->max_comment_size;

    options.put_space_before_value = !(flags & LPF_FLAG_NO_SPACE_BEFORE_VALUE);
    options.put_space_before_comment = !(flags & LPF_FLAG_NO_SPACE_BEFORE_COMMENT);
    options.put_space_before_marker = !!(flags & LPF_FLAG_SPACE_BEFORE_MARKER);
    options.comment_terminate_value = !!(flags & LPF_FLAG_COMMENT_TERMINATE);
    options.pad_continuations = true;

    lpf_write_lowlevel_entry(writer->builder, entry, &options);
}

void lpf_write_blank(Lpf_Writer* writer)
{
    if(writer->skip_blanks)
        return;
        
    Lpf_Entry entry = {LPF_KIND_BLANK};
    Lpf_Write_Options options = {0};
    options.line_indentation = writer->line_identation_per_level * writer->line_indentation;
    
    lpf_write_lowlevel_entry(writer->builder, entry, NULL);
}

void lpf_write_comment(Lpf_Writer* writer, String comment)
{
    if(writer->skip_comments)
        return;
        
    Lpf_Entry entry = {LPF_KIND_COMMENT};
    entry.comment = comment;
    
    Lpf_Write_Options options = {0};
    options.line_indentation = writer->line_identation_per_level * writer->line_indentation;
    options.comment_indentation = writer->comment_identation_per_level * writer->comment_indentation + writer->comment_indentation_offset;
    
    lpf_write_lowlevel_entry(writer->builder, entry, &options);
}

void lpf_write_scope_custom(Lpf_Writer* writer, String label, String type, String inline_comment, bool is_start)
{
    if(writer->skip_scopes)
        return;

    Lpf_Entry entry = {0};
    if(is_start)
        entry.kind = LPF_KIND_SCOPE_START;
    else
        entry.kind = LPF_KIND_SCOPE_END;
        
    entry.label = label;
    if(writer->skip_inline_comments == false)
        entry.comment = inline_comment;
    if(writer->skip_types == false)
        entry.type = type;
        
    if(is_start == false)
        writer->line_indentation -= 1;

    Lpf_Write_Options options = {0};
    options.line_indentation = writer->line_identation_per_level * writer->line_indentation;
    
    options.pad_prefix_to = writer->pad_prefix_to;
    options.max_value_size = writer->max_value_size;
    options.max_comment_size = writer->max_comment_size;

    options.put_space_before_comment = true;
    options.put_space_before_marker = true;
    options.pad_continuations = true;
    

    lpf_write_lowlevel_entry(writer->builder, entry, &options);

    if(is_start)
        writer->line_indentation += 1;
}

//================== CUSTOM =================

void lpf_write_raw_custom(Lpf_Writer* writer, String label, String raw, String inline_comment)
{
    lpf_write_entry_custom(writer, label, STRING("raw"), raw, inline_comment, LPF_FLAG_NO_SPACE_BEFORE_VALUE | LPF_FLAG_NO_SPACE_BEFORE_COMMENT | LPF_FLAG_COMMENT_TERMINATE);
}

void lpf_write_binary_custom(Lpf_Writer* writer, String label, String bin, String inline_comment)
{
    lpf_write_entry_custom(writer, label, STRING("bin"), bin, inline_comment, LPF_FLAG_NO_SPACE_BEFORE_VALUE | LPF_FLAG_NO_SPACE_BEFORE_COMMENT | LPF_FLAG_COMMENT_TERMINATE);
}

void lpf_write_i64_custom(Lpf_Writer* writer, String label, i64 value, String inline_comment)
{
    String formatted = format_ephemeral("%lli", (lli) value);
    lpf_write_entry_custom(writer, label, STRING("i"), formatted, inline_comment, 0);
}

void lpf_write_u64_custom(Lpf_Writer* writer, String label, u64 value, String inline_comment)
{
    String formatted = format_ephemeral("%llu", (llu) value);
    lpf_write_entry_custom(writer, label, STRING("u"), formatted, inline_comment, 0);
}

void lpf_write_f64_custom(Lpf_Writer* writer, String label, f64 value, String inline_comment)
{
    String formatted = format_ephemeral("%lf", value);
    lpf_write_entry_custom(writer, label, STRING("f"), formatted, inline_comment, 0);
}

void lpf_write_f32_custom(Lpf_Writer* writer, String label, f32 value, String inline_comment)
{
    String formatted = format_ephemeral("%f", value);
    lpf_write_entry_custom(writer, label, STRING("f"), formatted, inline_comment, 0);
}

void lpf_write_string_custom(Lpf_Writer* writer, String label, String name, String inline_comment)
{
    lpf_write_entry_custom(writer, label, STRING("s"), name, inline_comment, 0);
}

void lpf_write_name_custom(Lpf_Writer* writer, String label, String name, String inline_comment)
{
    lpf_write_entry_custom(writer, label, STRING("n"), name, inline_comment, 0);
}

void lpf_write_char_custom(Lpf_Writer* writer, String label, char c, String inline_comment)
{
    String str = {&c, 1};
    lpf_write_entry_custom(writer, label, STRING("n"), str, inline_comment, 0);
}

void lpf_write_bool_custom(Lpf_Writer* writer, String label, bool b, String inline_comment)
{
    lpf_write_entry_custom(writer, label, STRING("b"), b ? STRING("true") : STRING("false"), inline_comment, 0);
}

void lpf_write_null_custom(Lpf_Writer* writer, String label, String inline_comment)
{
    lpf_write_entry_custom(writer, label, STRING("null"), STRING(""), inline_comment, 0);
}

//===================== FINAL ==========================
void lpf_write_entry(Lpf_Writer* writer, const char* label, const char* type, String value)
{
    lpf_write_entry_custom(writer, string_make(label), string_make(type), value, STRING(""), 0);
}

void lpf_write_raw(Lpf_Writer* writer, const char* label, String raw)
{
    lpf_write_raw_custom(writer, string_make(label), raw, STRING(""));
}

void lpf_write_binary(Lpf_Writer* writer, const char* label, String bin)
{
    lpf_write_binary_custom(writer, string_make(label), bin, STRING(""));
}

void lpf_write_i64(Lpf_Writer* writer, const char* label, i64 value)
{
    lpf_write_i64_custom(writer, string_make(label), value, STRING(""));
}

void lpf_write_u64(Lpf_Writer* writer, const char* label, u64 value)
{
    lpf_write_u64_custom(writer, string_make(label), value, STRING(""));
}

void lpf_write_f64(Lpf_Writer* writer, const char* label, f64 value)
{
    lpf_write_f64_custom(writer, string_make(label), value, STRING(""));
}

void lpf_write_f32(Lpf_Writer* writer, const char* label, f32 value)
{
    lpf_write_f32_custom(writer, string_make(label), value, STRING(""));
}

void lpf_write_string(Lpf_Writer* writer, const char* label, String name)
{
    lpf_write_string_custom(writer, string_make(label), name, STRING(""));
}

void lpf_write_name(Lpf_Writer* writer, const char* label, String name)
{
    lpf_write_name_custom(writer, string_make(label), name, STRING(""));
}

void lpf_write_char(Lpf_Writer* writer, const char* label, char c)
{
    lpf_write_char_custom(writer, string_make(label), c, STRING(""));
}

void lpf_write_bool(Lpf_Writer* writer, const char* label, bool b)
{
    lpf_write_bool_custom(writer, string_make(label), b, STRING(""));
}

void lpf_write_null(Lpf_Writer* writer, const char* label)
{
    lpf_write_null_custom(writer, string_make(label), STRING(""));
}

void lpf_write_scope_start(Lpf_Writer* writer, const char* label, const char* type)
{
    lpf_write_scope_custom(writer, string_make(label), string_make(type), STRING(""), true);
}

void lpf_write_scope_end(Lpf_Writer* writer)
{
    lpf_write_scope_custom(writer, STRING(""), STRING(""), STRING(""), false);
}

void lpf_comment_group_push(Lpf_Writer* writer)
{
    writer->comment_indentation += 1;
}
void lpf_comment_group_pop(Lpf_Writer* writer)
{
    writer->comment_indentation -= 1;
}



//void lpf_file_deinit(Lpf_Reader* file)
//{
//    array_deinit(&file->entries);
//    array_deinit(&file->scopes);
//}

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

typedef struct Lpf_Reader {
    bool skip_comments;
    bool skip_inline_comments;
    bool skip_blanks;
    bool skip_scopes;
    bool skip_types;

    Lpf_Scope* scope;

    isize depth;
    isize line_number;
    isize source_i;

    bool has_queued;
    Lpf_Entry current_entry;
    String_Builder current_entry_data;
} Lpf_Reader;

String lpf_string_own_by(String_Builder* builder, String str)
{
    if(str.size <= 0)
        return STRING("");

    isize size_before = builder->size;
    builder_append(builder, str);
    isize size_after = builder->size;
    String owned = {builder->data + size_before, size_after - size_before};
    return owned;
}

void lpf_reader_commit_queued(Lpf_Reader* reader)
{
    if(reader->has_queued)
    {
        Lpf_Entry entry = reader->current_entry;
        String value = string_from_builder(reader->current_entry_data);
        entry.label = lpf_string_own_by(&reader->scope->entries_data, entry.label);
        entry.value = lpf_string_own_by(&reader->scope->entries_data, value);

        if(reader->skip_types)
            entry.comment = STRING("");
        else
            entry.type = lpf_string_own_by(&reader->scope->entries_data, entry.type);
    
        if(reader->skip_inline_comments)
            entry.comment = STRING("");
        else
            entry.comment = lpf_string_own_by(&reader->scope->entries_data, entry.comment);

        array_push(&reader->scope->entries, entry);
        array_clear(&reader->current_entry_data);
        reader->has_queued = false;
    }
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

isize lpf_read_(Lpf_Reader* reader, String source, Lpf_Scope_Array* scopes, bool has_queued)
{

    Lpf_Entry entry = {0}; 
        
    isize line_start_i = reader->source_i;
    String line = string_tail(source, line_start_i);
    isize next_line_i = lpf_read_lowlevel_entry(line, &entry);
    entry.line_number = reader->line_number;
    entry.depth = reader->depth;

    if(entry.error != LPF_ERROR_NONE)
    {
        LOG_ERROR("LPF", "Error %s reading lpf file on line %lli", lpf_error_to_string(entry.error), (lli) reader->line_number);
    }

    if(reader->has_queued == false && (entry.kind == LPF_KIND_ESCAPED_CONTINUATION || entry.kind == LPF_KIND_CONTINUATION))
    {
        LOG_ERROR("LPF", "Error %s reading lpf file on line %lli", lpf_error_to_string(LPF_ERROR_ENTRY_CONTINUNATION_WITHOUT_START), (lli) reader->line_number);
        entry.kind = LPF_KIND_ENTRY;
    }

    


    switch(entry.kind)
    {
        case LPF_KIND_BLANK: 
        case LPF_KIND_COMMENT: 
        case LPF_KIND_ENTRY: {
            lpf_reader_commit_queued(reader);
            if(entry.kind == LPF_KIND_BLANK && reader->skip_blanks)
                return;
            
            if(entry.kind == LPF_KIND_COMMENT && reader->skip_comments)
                return;

            array_clear(&reader->current_entry_data);
            reader->current_entry = entry;
            builder_append(&reader->current_entry_data, entry.value);
            reader->has_queued = true;
        } break;
                
        case LPF_KIND_CONTINUATION: {
            ASSERT(reader->has_queued);
            builder_append(&reader->current_entry_data, entry.value);
            array_push(&reader->current_entry_data, '\n');
        } break;

        case LPF_KIND_ESCAPED_CONTINUATION: {
            ASSERT(reader->has_queued);
            builder_append(&reader->current_entry_data, entry.value);
        } break;

        case LPF_KIND_SCOPE_START: {
            Lpf_Scope new_scope = {0};
            new_scope.parent = reader->scope;
            new_scope.label = entry.label;
            new_scope.type = entry.type;
            new_scope.line_number = entry.line_number;
            new_scope.depth = entry.depth;


            array_push(scopes, new_scope);
            reader->scope = array_last(*scopes);
            reader->depth += 1;
        } break;

        case LPF_KIND_SCOPE_END: {

        } break;
            
    }
        
    return next_line_i;
}
