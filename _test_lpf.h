#pragma once
#include "_test.h"
#include "lpf_parser.h"
#include "string.h"

void* lpf_allocator_realloc(isize new_size, void* old_ptr, isize old_size, void* context)
{
    return allocator_reallocate((Allocator*) context, new_size, old_ptr, old_size, DEF_ALIGN, SOURCE_INFO());
}

typedef struct Lpf_Test_Entry {
    Lpf_Kind kind;

    const char* label;
    const char* type;
    const char* value;
    const char* comment;

    Lpf_Error error;
} Lpf_Test_Entry;

void lpf_test_single_parse(const char* ctext, const Lpf_Test_Entry* entries, isize entries_size)
{
    Lpf_File file = {0};
    file.realloc_func = lpf_allocator_realloc;
    file.realloc_context = allocator_get_default();
    file.log_func = lpf_default_log;

    String text = string_make(ctext);
    isize finished_at = lpf_parse(&file, text.data, (isize) text.size, LPF_FLAG_KEEP_COMMENTS | LPF_FLAG_KEEP_ERRORS);

    TEST(finished_at == text.size);

    isize min_size = MIN(file.entries_size, entries_size);
    for(isize i = 0; i < min_size; i++)
    {
        Lpf_Entry entry = file.entries[i];
        Lpf_Test_Entry test_entry = entries[i];

        TEST(entry.error == test_entry.error);
        if(entry.error == LPF_ERROR_NONE)
        {
            TEST(string_is_equal(entry.label,   string_make(test_entry.label)));
            TEST(string_is_equal(entry.type,    string_make(test_entry.type)));
            TEST(string_is_equal(entry.value,   string_make(test_entry.value)));
            TEST(string_is_equal(entry.comment, string_make(test_entry.comment)));
        }
    }

    TEST_MSG(file.entries_size == entries_size, "The numebr of entries must match!");
    lpf_file_deinit(&file);
}

void lpf_test_parse()
{
    {
        const char* text = ":hello world!";
        Lpf_Test_Entry entries[] = {{LPF_KIND_ENTRY, "", "", "hello world!", ""}};
        lpf_test_single_parse(text, entries, STATIC_ARRAY_SIZE(entries));
    }
    
    {
        const char* text = "label:...value...\n";
        Lpf_Test_Entry entries[] = {{LPF_KIND_ENTRY, "label", "", "...value...", ""}};
        lpf_test_single_parse(text, entries, STATIC_ARRAY_SIZE(entries));
    }

    {
        const char* text = "label type:...value...#comment";
        Lpf_Test_Entry entries[] = {{LPF_KIND_ENTRY, "label", "type", "...value...", "comment"}};
        lpf_test_single_parse(text, entries, STATIC_ARRAY_SIZE(entries));
    }
    
    {
        const char* text = "label t";
        Lpf_Test_Entry entries[] = {{LPF_KIND_BLANK, "", "", "", "", LPF_ERROR_ENTRY_MISSING_START}};
        lpf_test_single_parse(text, entries, STATIC_ARRAY_SIZE(entries));
    }
    
    {
        const char* text = ",hello world!";
        Lpf_Test_Entry entries[] = {{LPF_KIND_CONTINUATION, "", "", "", "", LPF_ERROR_ENTRY_CONTINUNATION_WITHOUT_START}};
        lpf_test_single_parse(text, entries, STATIC_ARRAY_SIZE(entries));
    }

    {
        const char* text = 
            "texture s:first line\n"
            "          ,continuation\n"
            "          ;continuation without line break\n"         
            ;
        Lpf_Test_Entry entries[] = {
            {LPF_KIND_ENTRY,                "texture", "s", "first line", ""},
            {LPF_KIND_CONTINUATION,         "", "", "continuation", ""},
            {LPF_KIND_ESCAPED_CONTINUATION, "", "", "continuation without line break", ""}
        };
        lpf_test_single_parse(text, entries, STATIC_ARRAY_SIZE(entries));
    }

    
    {
        const char* text = 
            "#this is a texture declaration\n"
            "texture TEX { #some comment\n"
            "   val: 9814814\n"
            "   offset 3f: 0 0 0\n"
            "   \n"
            "} # end \n"
            ;
        Lpf_Test_Entry entries[] = {
            {LPF_KIND_COMMENT,              "", "", "", "this is a texture declaration"},
            {LPF_KIND_COLLECTION_START,     "texture", "TEX", "", "some comment"},
            {LPF_KIND_ENTRY,                "val", "", " 9814814", ""},
            {LPF_KIND_ENTRY,                "offset", "3f", " 0 0 0", ""},
            {LPF_KIND_BLANK,                "", "", "", ""},
            {LPF_KIND_COLLECTION_END,       "", "", "", " end "}
        };
        lpf_test_single_parse(text, entries, STATIC_ARRAY_SIZE(entries));
    }
}


void lpf_test_single_write(const char* ctext, Lpf_Test_Entry test_entry, Lpf_Write_Options options)
{
    String_Builder into = {allocator_get_scratch()};

    Lpf_Entry entry = {0};
    entry.label = string_make(test_entry.label);
    entry.type = string_make(test_entry.type);
    entry.value = string_make(test_entry.value);
    entry.comment = string_make(test_entry.comment);
    entry.kind = test_entry.kind;

    lpf_write_line_custom(&into, entry, &options);

    String expected = string_make(ctext);
    String obtained = string_from_builder(into);
    TEST_MSG(string_is_equal(expected, obtained), 
        "expected: '\n"STRING_FMT"\n'\n"
        "obtained: '\n"STRING_FMT"\n'", 
        STRING_PRINT(expected),
        STRING_PRINT(obtained)
    );

    array_deinit(&into);
}


void test_format_lpf()
{
    {
        Lpf_Test_Entry entry = {LPF_KIND_ENTRY, "label", "type", "val", "comment"};
        Lpf_Write_Options options = {0};
        const char* text = "label type:val#comment\n";
        lpf_test_single_write(text, entry, options);
    }
    

    {
        Lpf_Test_Entry entry = {LPF_KIND_CONTINUATION, "label", "type", "val", "comment"};
        Lpf_Write_Options options = {0};
        const char* text = "        ,val #comment\n";
        options.put_space_before_comment = true;
        options.line_indentation = 3;
        options.pad_prefix_to = 5;

        lpf_test_single_write(text, entry, options);
    }
    
    {
        Lpf_Test_Entry entry = {LPF_KIND_ENTRY, "lab#:", "t pe", "val", "comment"};
        Lpf_Write_Options options = {0};
        const char* text = "lab__ t_pe:val#comment\n";
        lpf_test_single_write(text, entry, options);
    }

    {
        Lpf_Test_Entry entry = {LPF_KIND_ENTRY, "label", "type", "val1\nval2\nval3", "comment"};
        Lpf_Write_Options options = {0};
        const char* text = 
            "label type:val1\n"
            ",val2\n"
            ",val3#comment\n"
            ;
        lpf_test_single_write(text, entry, options);
    }

    {
        Lpf_Test_Entry entry = {LPF_KIND_ENTRY, "label", "type", "val1\nval2\nval3", "comment#"};
        Lpf_Write_Options options = {0};
        options.pad_continuations = true;
        options.line_indentation = 3;
        const char* text = 
            "   label type:val1\n"
            "             ,val2\n"
            "             ,val3\n"
            "             #comment#\n"
            ;
        lpf_test_single_write(text, entry, options);
    }

    
    {
        Lpf_Test_Entry entry = {LPF_KIND_ENTRY, "label", "type", "val1long\nval2\nval3long", "comment#"};
        Lpf_Write_Options options = {0};
        options.pad_continuations = true;
        options.line_indentation = 3;
        options.max_line_size = 4;
        const char* text = 
            "   label type:val1\n"
            "             ;long\n"
            "             ,val2\n"
            "             ,val3\n"
            "             ;long\n"
            "             #comm\n"
            "             #ent#\n"
            ;
        lpf_test_single_write(text, entry, options);
    }
    
    {
        const char* text = 
            "label type:val\n"
            "          #comment\n";
        Lpf_Test_Entry entry = {LPF_KIND_ENTRY, "label", "type", "val", "comment"};
        Lpf_Write_Options options = {0};
        options.max_line_size = 7;
        options.pad_continuations = true;
        lpf_test_single_write(text, entry, options);
    }
    
    {
        Lpf_Test_Entry entry = {LPF_KIND_COMMENT, "label", "type", "val", "comment##"};
        Lpf_Write_Options options = {0};
        const char* text = "#comment##\n";
        lpf_test_single_write(text, entry, options);
    }
    
    {
        Lpf_Test_Entry entry = {LPF_KIND_COLLECTION_START, "label", "type", "val", "comment"};
        Lpf_Write_Options options = {0};
        const char* text = "label type{#comment\n";
        lpf_test_single_write(text, entry, options);
    }
    
    {
        Lpf_Test_Entry entry = {LPF_KIND_COLLECTION_START, "label", "type", "val", "comment"};
        Lpf_Write_Options options = {0};
        options.put_space_before_comment = true;
        options.put_space_before_marker = true;
        options.put_space_before_value = true;
        const char* text = "label type { #comment\n";
        lpf_test_single_write(text, entry, options);
    }
    
    {
        Lpf_Test_Entry entry = {LPF_KIND_COLLECTION_END, "label", "type", "val", "comment"};
        Lpf_Write_Options options = {0};
        options.put_space_before_comment = true;
        options.put_space_before_marker = true;
        options.put_space_before_value = true;
        const char* text = "} #comment\n";
        lpf_test_single_write(text, entry, options);
    }
}