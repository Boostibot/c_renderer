#ifndef LIB_TODO
#define LIB_TODO

// This is a file that presents functions for curling @TODOs @NOTEs and other such tags from files.
// This is mainly for fun and to remind me of what needs to be done more than actual usefulness.
// 
// The general syntax is:
// 
// @marker(signature): Comment on the first line
//                    with a continuation on the second.
// 
// All this will get parsed into:
//   marker: "@marker" 
//   signature: "signature"
//   comment: "Comment on the first line with a continuation on the second."
// 
// Of course @marker is modifiable (does not have to have @ at the start) and signature can be anything. 
// The signature and colon is optional. This means both of the following are valid:
// 
// @marker(signature) todo missing colon
// 
// @SIGIL todo missing everything
//
// Alternatively markers can appear with the same syntax inside strings. Those are however only limited to single line.
// The string parsing is really really simplistic and doesnt take into account escaping.
//
// assert(1 > 0 && "this is an assert @TODO: make better!  ");
// will get parsed into: 
//   marker: "@TODO" 
//   comment: "make better!"
//

#include "lib/parse.h"
#include "lib/file.h"

typedef struct Todo
{
    isize line;
    String_Builder path;
    String_Builder comment; 
    String_Builder signature; 
    String_Builder marker; 
} Todo;

typedef Array(Todo) Todo_Array;

EXTERNAL void todo_init(Todo* todo, Allocator* alloc);
EXTERNAL void todo_deinit(Todo* todo);

EXTERNAL void todo_parse_source(Todo_Array* todos, String path, String todo_marker, String source);
EXTERNAL bool todo_parse_file(Todo_Array* todos, String path, String todo_marker);
EXTERNAL bool todo_parse_folder(Todo_Array* todos, String path, String todo_marker, isize depth);
EXTERNAL bool log_todos(const char* marker, isize depth);

#endif

#if (defined(LIB_ALL_IMPL) || defined(LIB_TODO_IMPL)) && !defined(LIB_TODO_HAS_IMPL)
#define LIB_TODO_HAS_IMPL

EXTERNAL void todo_init(Todo* todo, Allocator* alloc)
{
    todo_deinit(todo);
    
    builder_init(&todo->path, alloc);
    builder_init(&todo->comment, alloc); 
    builder_init(&todo->signature, alloc); 
    builder_init(&todo->marker, alloc); 
}

EXTERNAL void todo_deinit(Todo* todo)
{
    builder_deinit(&todo->path);
    builder_deinit(&todo->comment); 
    builder_deinit(&todo->signature); 
    builder_deinit(&todo->marker); 
    todo->line = 0;
}

EXTERNAL void todo_parse_source(Todo_Array* todos, String path, String todo_markers, String source)
{
    for(isize word_index = 0; word_index < todo_markers.len; )
    {
        match_whitespace(todo_markers, &word_index);
        isize from = word_index;
        match_whitespace_custom(todo_markers, &word_index, MATCH_INVERTED);
        isize to = word_index;

        String todo_marker = string_range(todo_markers, from, to);
        ASSERT(todo_marker.len > 0);

        for(Line_Iterator it = {0}; line_iterator_get_line(&it, source); )
        {
            String line = it.line;
            isize todo_i = string_find_first(line, todo_marker, 0);
            if(todo_i == -1)
                continue;

            isize past_todo_i = todo_i + todo_marker.len;
            isize comment_start = string_find_first(line, STRING("//"), 0);
            isize string_start = string_find_last_char_from(line, '"', todo_i);
        
            String signature = {0};
        
            //Try to match signature
            isize signature_from = past_todo_i;
            isize signature_to = past_todo_i;
            if(match_char(line, &signature_to, '(') 
                && match_any_of_custom(line, &signature_to, STRING(")"), MATCH_INVERTED) 
                && match_char(line, &signature_to, ')'))
            {
                //+1 -1 to remove '(' and ')'
                signature = string_range(line, signature_from + 1, signature_to - 1);
            }
            else
               signature_to = signature_from; 

            //Try to match the : after todo if it exists
            isize past_todo_colon_i = signature_to;
            match_char(line, &past_todo_colon_i, ':');
        
            Todo todo = {0};
            todo.line = it.line_number;
            todo.path = builder_from_string(NULL, path);
            todo.marker = builder_from_string(NULL, todo_marker);
            todo.signature = builder_from_string(NULL, signature);

            //is a comment todo
            if(comment_start != -1)
            {
                String first_content = string_trim_whitespace(string_tail(line, past_todo_colon_i));
                builder_append(&todo.comment, first_content);

                //find all neigbouring lines
                for(Line_Iterator subit = it; line_iterator_get_line(&subit, source); )
                {
                    isize index = 0;

                    //skip whitespace before comment
                    match_whitespace(subit.line, &index);
                    if(match_sequence(subit.line, &index, STRING("//")))
                    {
                        //if this line has another todo break
                        isize sub_todo_i = string_find_first(subit.line, todo_marker, 0);
                        if(sub_todo_i != -1)   
                            break;

                        String connecting_line = string_tail(subit.line, index);
                        String connecting_content = string_trim_whitespace(connecting_line);
                        if(connecting_content.len == 0)
                            break;

                        if(todo.comment.len > 0)
                            builder_push(&todo.comment, ' ');

                        builder_append(&todo.comment, connecting_content);

                        //skip this line
                        it = subit;
                    }
                    else
                        break;
                }

            }
            //is a string todo find end of string and end there
            else if(string_start != -1 && string_start < todo_i)
            {
                String first_content_line = string_safe_tail(line, past_todo_colon_i);
                isize string_end = string_find_first_char(first_content_line, '"', 0);
                if(string_end == -1)
                    string_end = line.len;

                String string = string_head(first_content_line, string_end);
                String content = string_trim_whitespace(string);
                builder_append(&todo.comment, content);
            }
            //is something else
            else 
            {
                //keep content empty
            }

            array_push(todos, todo);
        }
    }
}


EXTERNAL bool todo_parse_file(Todo_Array* todos, String path, String todo)
{
    Arena_Frame arena = scratch_arena_frame_acquire();
    String_Builder source = {arena.alloc};

    bool file_error = file_read_entire(path, &source, NULL);
    todo_parse_source(todos, path, source.string, todo);
    
    arena_frame_release(&arena);
    return file_error;
}

EXTERNAL bool todo_parse_folder(Todo_Array* todos, String path, String todo, isize depth)
{
    bool state = true;
    Arena_Frame arena = scratch_arena_frame_acquire();
    {
        Platform_Directory_Entry* entries = NULL;
        isize entries_count = 0;
        if(platform_directory_list_contents_alloc(path, &entries, &entries_count, depth) != 0)
        {
            LOG_ERROR("todo", "todo_parse_folder() couldnt open folder '%.*s' with todo '%.*s'", 
                STRING_PRINT(path), STRING_PRINT(todo));
        }

        if(state)
        {
            for(isize i = 0; i < entries_count; i++)
            {
                Platform_Directory_Entry entry = entries[i];
                if(entry.info.type == PLATFORM_FILE_TYPE_FILE)
                    state = state && todo_parse_file(todos, string_of(entry.path), todo);
            }
        }

        platform_directory_list_contents_free(entries);
    }
    arena_frame_release(&arena);
    return state;
}

EXTERNAL bool log_todos(const char* marker, isize depth)
{
    Todo_Array todos = {0};
    bool state = todo_parse_folder(&todos, STRING("./"), string_of(marker), depth);
    if(state)
    {
        String common_path_prefix = {0};
        for(isize i = 0; i < todos.len; i++)
        {
            String curent_path = todos.data[i].path.string;
            if(common_path_prefix.data == NULL)
                common_path_prefix = curent_path;
            else
            {
                isize matches_to = 0;
                isize min_size = MIN(common_path_prefix.len, curent_path.len);
                for(; matches_to < min_size; matches_to++)
                {
                    if(common_path_prefix.data[matches_to] != curent_path.data[matches_to])
                        break;
                }

                common_path_prefix = string_safe_head(common_path_prefix, matches_to);
            }
        }
    
        LOG_INFO("todos", "Logging TODOs (%lli):", (lli) todos.len);
        for(isize i = 0; i < todos.len; i++)
        {
            Todo todo = todos.data[i];
            String path = todo.path.string;
        
            if(path.len > common_path_prefix.len)
                path = string_safe_tail(path, common_path_prefix.len);

            if(todo.signature.len > 0)
                LOG_INFO(">todos", "%-20s %4lli %s(%s) %s\n", path.data, (lli) todo.line, todo.marker.data, todo.signature.data, todo.comment.data);
            else
                LOG_INFO(">todos", "%-20s %4lli %s %s\n", path.data, (lli) todo.line, todo.marker.data, todo.comment.data);
        }
    
        for(isize i = 0; i < todos.len; i++)
            todo_deinit(&todos.data[i]);

        array_deinit(&todos);
    }
    return state;
}

#endif