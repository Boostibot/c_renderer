#pragma once
#include "_test.h"
#include "format_lpf.h"
#include "string.h"

typedef struct Lpf_Test_Entry {
    Lpf_Kind kind;

    const char* label;
    const char* type;
    const char* value;
    const char* comment;

    Lpf_Error error;
} Lpf_Test_Entry;

Lpf_Test_Entry lpf_test_entry_error(Lpf_Kind kind, Lpf_Error error)
{
    Lpf_Test_Entry entry = {kind, "", "", "", "", error};
    return entry;
}

Lpf_Test_Entry lpf_test_entry(Lpf_Kind kind, const char* label, const char* type, const char* value, const char* comment)
{
    Lpf_Test_Entry entry = {kind, label, type, value, comment};
    return entry;
}

void lpf_test_single_read(const char* ctext, Lpf_Test_Entry test_entry)
{
    String text = string_make(ctext);
    Lpf_Entry entry = {0};
    isize finished_at = lpf_read_line(text, &entry);
    (void) finished_at;

    //TEST(finished_at == text.size);
    TEST(entry.error == test_entry.error);
    if(entry.error == LPF_ERROR_NONE)
    {
        TEST(string_is_equal(entry.label,   string_make(test_entry.label)));
        TEST(string_is_equal(entry.type,    string_make(test_entry.type)));
        TEST(string_is_equal(entry.value,   string_make(test_entry.value)));
        TEST(string_is_equal(entry.comment, string_make(test_entry.comment)));
    }
}

void lpf_test_read()
{
    //Okay values
    lpf_test_single_read(":hello world!",                  lpf_test_entry(LPF_KIND_ENTRY, "", "", "hello world!", ""));
    lpf_test_single_read("  ;hello world!#",               lpf_test_entry(LPF_KIND_ESCAPED_CONTINUATION, "", "", "hello world!", ""));
    lpf_test_single_read("  ,hello world!",                lpf_test_entry(LPF_KIND_CONTINUATION, "", "", "hello world!", ""));
    lpf_test_single_read("label:...value...\n814814\n",    lpf_test_entry(LPF_KIND_ENTRY, "label", "", "...value...", ""));
    lpf_test_single_read("label type:...value...#comment", lpf_test_entry(LPF_KIND_ENTRY, "label", "type", "...value...", "comment"));

    lpf_test_single_read("#this is a texture declaration##\n", lpf_test_entry(LPF_KIND_COMMENT, "", "", "", "this is a texture declaration##"));
    lpf_test_single_read(" {   ",                          lpf_test_entry(LPF_KIND_COLLECTION_START, "", "", "", ""));
    lpf_test_single_read(" map {   ",                      lpf_test_entry(LPF_KIND_COLLECTION_START, "map", "", "", ""));
    lpf_test_single_read("texture   TEX { #some comment",  lpf_test_entry(LPF_KIND_COLLECTION_START, "texture", "TEX", "", "some comment"));
    lpf_test_single_read(" }",                             lpf_test_entry(LPF_KIND_COLLECTION_END, "", "", "", ""));
    lpf_test_single_read(" } #some comment",               lpf_test_entry(LPF_KIND_COLLECTION_END, "", "", "", "some comment"));

    lpf_test_single_read("",                               lpf_test_entry(LPF_KIND_BLANK, "", "", "", ""));
    lpf_test_single_read("  \t \v \f",                     lpf_test_entry(LPF_KIND_BLANK, "", "", "", ""));
    
    //Errors 
    lpf_test_single_read("label ",                         lpf_test_entry_error(LPF_KIND_BLANK, LPF_ERROR_ENTRY_MISSING_START));
    lpf_test_single_read("label t",                        lpf_test_entry_error(LPF_KIND_BLANK, LPF_ERROR_ENTRY_MISSING_START));
    lpf_test_single_read("label t1 t2:",                   lpf_test_entry_error(LPF_KIND_ENTRY, LPF_ERROR_ENTRY_MULTIPLE_TYPES));
    lpf_test_single_read("label ,",                        lpf_test_entry_error(LPF_KIND_CONTINUATION, LPF_ERROR_ENTRY_CONTINUNATION_HAS_LABEL));
    lpf_test_single_read("label t2 ,",                     lpf_test_entry_error(LPF_KIND_CONTINUATION, LPF_ERROR_ENTRY_CONTINUNATION_HAS_LABEL));
    lpf_test_single_read("label t2 ;",                     lpf_test_entry_error(LPF_KIND_CONTINUATION, LPF_ERROR_ENTRY_CONTINUNATION_HAS_LABEL));
    lpf_test_single_read("label t2 t3 ;",                  lpf_test_entry_error(LPF_KIND_CONTINUATION, LPF_ERROR_ENTRY_CONTINUNATION_HAS_LABEL));

    lpf_test_single_read("texture TEX 12 { #some comment", lpf_test_entry_error(LPF_KIND_COLLECTION_START, LPF_ERROR_COLLECTION_MULTIPLE_TYPES));
    lpf_test_single_read("texture TEX { val ",             lpf_test_entry_error(LPF_KIND_COLLECTION_START, LPF_ERROR_COLLECTION_CONTENT_AFTER_START));
    lpf_test_single_read("} # #val ",                      lpf_test_entry_error(LPF_KIND_COLLECTION_END, LPF_ERROR_COLLECTION_CONTENT_AFTER_END));
    lpf_test_single_read(" some_label } #comment",         lpf_test_entry_error(LPF_KIND_COLLECTION_END, LPF_ERROR_COLLECTION_END_HAS_LABEL));
    lpf_test_single_read(" some_label a } #comment",       lpf_test_entry_error(LPF_KIND_COLLECTION_END, LPF_ERROR_COLLECTION_END_HAS_LABEL));
    lpf_test_single_read(" some_label a b c}",             lpf_test_entry_error(LPF_KIND_COLLECTION_END, LPF_ERROR_COLLECTION_END_HAS_LABEL));
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

void lpf_test_write()
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
            "             ,val3#comment:hash:\n"
            ;
        lpf_test_single_write(text, entry, options);
    }

    {
        Lpf_Test_Entry entry = {LPF_KIND_ENTRY, "label", "type", "val1long\nval2\nval3long", "comment#"};
        Lpf_Write_Options options = {0};
        options.pad_continuations = true;
        options.line_indentation = 3;
        options.max_value_size = 4;
        const char* text = 
            "   label type:val1\n"
            "             ;long\n"
            "             ,val2\n"
            "             ,val3\n"
            "             ;long#comment:hash:\n"
            ;
        lpf_test_single_write(text, entry, options);
    }
    
    {
        Lpf_Test_Entry entry = {LPF_KIND_ENTRY, "label", "type", "val1long\nval2\nval3long", ""};
        Lpf_Write_Options options = {0};
        options.pad_continuations = true;
        options.comment_terminate_line = true;
        options.max_value_size = 4;
        const char* text = 
            "label type:val1#\n"
            "          ;long#\n"
            "          ,val2#\n"
            "          ,val3#\n"
            "          ;long#\n"
            ;
        lpf_test_single_write(text, entry, options);
    }
    
    {
        Lpf_Test_Entry entry = {LPF_KIND_COMMENT, "label", "type", "val", "comment##"};
        Lpf_Write_Options options = {0};
        const char* text = "#comment##\n";
        lpf_test_single_write(text, entry, options);
    }
    
    {
        Lpf_Test_Entry entry = {LPF_KIND_COMMENT, "label", "type", "val", "comment## with\nnewlines\nand long lines"};
        Lpf_Write_Options options = {0};
        options.comment_indentation = 3;
        options.max_comment_size = 8;
        const char* text = 
            "#   comment#\n"
            "#   # with\n"
            "#   newlines\n"
            "#   and long\n"
            "#   lines\n"
            ;
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

void test_format_lpf()
{
    lpf_test_write();
    lpf_test_read();
}