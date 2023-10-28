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

void lpf_test_lowlevel_read(const char* ctext, Lpf_Test_Entry test_entry)
{
    String text = string_make(ctext);
    Lpf_Entry entry = {0};
    isize finished_at = lpf_read_lowlevel_entry(text, &entry);
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

void lpf_test_lowlevel_write(Lpf_Write_Options options, Lpf_Test_Entry test_entry, const char* ctext)
{
    String_Builder into = {allocator_get_scratch()};

    Lpf_Entry entry = {0};
    entry.label = string_make(test_entry.label);
    entry.type = string_make(test_entry.type);
    entry.value = string_make(test_entry.value);
    entry.comment = string_make(test_entry.comment);
    entry.kind = test_entry.kind;

    lpf_write_lowlevel_entry(&into, entry, &options);

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

void lpf_test_read_lowlevel_entry()
{
    //Okay values
    lpf_test_lowlevel_read(":hello world!",                  lpf_test_entry(LPF_KIND_ENTRY, "", "", "hello world!", ""));
    lpf_test_lowlevel_read("  ;hello world!#",               lpf_test_entry(LPF_KIND_ESCAPED_CONTINUATION, "", "", "hello world!", ""));
    lpf_test_lowlevel_read("  ,hello world!",                lpf_test_entry(LPF_KIND_CONTINUATION, "", "", "hello world!", ""));
    lpf_test_lowlevel_read("label:...value...\n814814\n",    lpf_test_entry(LPF_KIND_ENTRY, "label", "", "...value...", ""));
    lpf_test_lowlevel_read("label type:...value...#comment", lpf_test_entry(LPF_KIND_ENTRY, "label", "type", "...value...", "comment"));

    lpf_test_lowlevel_read("#this is a texture declaration##\n", lpf_test_entry(LPF_KIND_COMMENT, "", "", "", "this is a texture declaration##"));
    lpf_test_lowlevel_read(" {   ",                          lpf_test_entry(LPF_KIND_SCOPE_START, "", "", "", ""));
    lpf_test_lowlevel_read(" map {   ",                      lpf_test_entry(LPF_KIND_SCOPE_START, "map", "", "", ""));
    lpf_test_lowlevel_read("texture   TEX { #some comment",  lpf_test_entry(LPF_KIND_SCOPE_START, "texture", "TEX", "", "some comment"));
    lpf_test_lowlevel_read(" }",                             lpf_test_entry(LPF_KIND_SCOPE_END, "", "", "", ""));
    lpf_test_lowlevel_read(" } #some comment",               lpf_test_entry(LPF_KIND_SCOPE_END, "", "", "", "some comment"));

    lpf_test_lowlevel_read("",                               lpf_test_entry(LPF_KIND_BLANK, "", "", "", ""));
    lpf_test_lowlevel_read("  \t \v \f",                     lpf_test_entry(LPF_KIND_BLANK, "", "", "", ""));
    
    //Errors 
    lpf_test_lowlevel_read("label ",                         lpf_test_entry_error(LPF_KIND_BLANK, LPF_ERROR_ENTRY_MISSING_START));
    lpf_test_lowlevel_read("label t",                        lpf_test_entry_error(LPF_KIND_BLANK, LPF_ERROR_ENTRY_MISSING_START));
    lpf_test_lowlevel_read("label t1 t2:",                   lpf_test_entry_error(LPF_KIND_ENTRY, LPF_ERROR_ENTRY_MULTIPLE_TYPES));
    lpf_test_lowlevel_read("label ,",                        lpf_test_entry_error(LPF_KIND_CONTINUATION, LPF_ERROR_ENTRY_CONTINUNATION_HAS_LABEL));
    lpf_test_lowlevel_read("label t2 ,",                     lpf_test_entry_error(LPF_KIND_CONTINUATION, LPF_ERROR_ENTRY_CONTINUNATION_HAS_LABEL));
    lpf_test_lowlevel_read("label t2 ;",                     lpf_test_entry_error(LPF_KIND_CONTINUATION, LPF_ERROR_ENTRY_CONTINUNATION_HAS_LABEL));
    lpf_test_lowlevel_read("label t2 t3 ;",                  lpf_test_entry_error(LPF_KIND_CONTINUATION, LPF_ERROR_ENTRY_CONTINUNATION_HAS_LABEL));

    lpf_test_lowlevel_read("texture TEX 12 { #some comment", lpf_test_entry_error(LPF_KIND_SCOPE_START, LPF_ERROR_SCOPE_MULTIPLE_TYPES));
    lpf_test_lowlevel_read("texture TEX { val ",             lpf_test_entry_error(LPF_KIND_SCOPE_START, LPF_ERROR_SCOPE_CONTENT_AFTER_START));
    lpf_test_lowlevel_read("} # #val ",                      lpf_test_entry_error(LPF_KIND_SCOPE_END, LPF_ERROR_SCOPE_CONTENT_AFTER_END));
    lpf_test_lowlevel_read(" some_label } #comment",         lpf_test_entry_error(LPF_KIND_SCOPE_END, LPF_ERROR_SCOPE_END_HAS_LABEL));
    lpf_test_lowlevel_read(" some_label a } #comment",       lpf_test_entry_error(LPF_KIND_SCOPE_END, LPF_ERROR_SCOPE_END_HAS_LABEL));
    lpf_test_lowlevel_read(" some_label a b c}",             lpf_test_entry_error(LPF_KIND_SCOPE_END, LPF_ERROR_SCOPE_END_HAS_LABEL));
}

void lpf_test_write_lowlevel_entry()
{
    #if 0
    Lpf_Write_Options def_options = {0};

    lpf_test_lowlevel_write(def_options, 
        lpf_test_entry(LPF_KIND_ENTRY, "label", "type", "val", "comment"), 
        "label type:val#comment\n");
    
    lpf_test_lowlevel_write(def_options, 
        lpf_test_entry(LPF_KIND_ENTRY, "", "type", "val", "comment"), 
        "_ type:val#comment\n");

    lpf_test_lowlevel_write(def_options, 
        lpf_test_entry(LPF_KIND_CONTINUATION, "label", "type", "valval", "comment with #"), 
        ",valval#comment with :hash:\n");
    
    lpf_test_lowlevel_write(def_options, 
        lpf_test_entry(LPF_KIND_ESCAPED_CONTINUATION, "label", "type", "valval", "comment with # and \n   newline"), 
        ";valval#comment with :hash: and     newline\n");

    lpf_test_lowlevel_write(def_options, 
        lpf_test_entry(LPF_KIND_COMMENT, "label", "type", "val", "comment##"), 
        "#comment##\n");
        
    lpf_test_lowlevel_write(def_options, 
        lpf_test_entry(LPF_KIND_SCOPE_START, "label", "type", "val", "comment"), 
        "label type{#comment\n");
        
    lpf_test_lowlevel_write(def_options, 
        lpf_test_entry(LPF_KIND_SCOPE_END, "label", "type", "val", "comment"), 
        "}#comment\n");

    {
        
        Lpf_Write_Options options = {0};
        options.put_space_before_comment = true;
        options.line_indentation = 3;
        options.pad_prefix_to = 5;
        lpf_test_lowlevel_write(options, 
            lpf_test_entry(LPF_KIND_CONTINUATION, "label", "type", "val", "comment"), 
            "        ,val #comment\n");
    }
    
    {
        
        lpf_test_lowlevel_write(def_options, 
            lpf_test_entry(LPF_KIND_ENTRY, "lab#:", "t pe", "val", "comment"), 
            "lab__ t_pe:val#comment\n");
    }

    {
        
        lpf_test_lowlevel_write(def_options, 
            lpf_test_entry(LPF_KIND_ENTRY, "label", "type", "val1\nval2\nval3", "comment"), 
            "label type:val1\n"
            ",val2\n"
            ",val3#comment\n");
    }

    {
        
        Lpf_Write_Options options = {0};
        options.pad_continuations = true;
        options.line_indentation = 3;
        lpf_test_lowlevel_write(options, 
            lpf_test_entry(LPF_KIND_ENTRY, "label", "type", "val1\nval2\nval3", "comment#"), 
            "   label type:val1\n"
            "             ,val2\n"
            "             ,val3#comment:hash:\n");
    }

    {
        
        Lpf_Write_Options options = {0};
        options.pad_continuations = true;
        options.line_indentation = 3;
        options.max_value_size = 4;
        lpf_test_lowlevel_write(options, 
            lpf_test_entry(LPF_KIND_ENTRY, "label", "type", "val1long\nval2\nval3long", "comment#"), 
            "   label type:val1\n"
            "             ;long\n"
            "             ,val2\n"
            "             ,val3\n"
            "             ;long#comment:hash:\n");
    }
    
    {
        
        Lpf_Write_Options options = {0};
        options.pad_continuations = true;
        options.comment_terminate_value = true;
        options.max_value_size = 4;
        lpf_test_lowlevel_write(options, 
            lpf_test_entry(LPF_KIND_ENTRY, "label", "type", "val1long\nval2\nval3long", ""), 
            "label type:val1#\n"
            "          ;long#\n"
            "          ,val2#\n"
            "          ,val3#\n"
            "          ;long#\n");
    }
    
    {
        
        Lpf_Write_Options options = {0};
        options.comment_indentation = 3;
        options.max_comment_size = 8;
        lpf_test_lowlevel_write(options, 
            lpf_test_entry(LPF_KIND_COMMENT, "label", "type", "val", "comment## with\nnewlines\nand long lines"), 
            "#   comment#\n"
            "#   # with\n"
            "#   newlines\n"
            "#   and long\n"
            "#   lines\n");
    }
    
    {
        
        Lpf_Write_Options options = {0};
        options.put_space_before_comment = true;
        options.put_space_before_marker = true;
        options.put_space_before_value = true;
        lpf_test_lowlevel_write(options, 
            lpf_test_entry(LPF_KIND_SCOPE_START, "label", "type", "val", "comment"), 
            "label type { #comment\n");
    }
    
    {
        Lpf_Write_Options options = {0};
        options.put_space_before_comment = true;
        options.put_space_before_marker = true;
        lpf_test_lowlevel_write(options, 
            lpf_test_entry(LPF_KIND_SCOPE_START, "", "", "val", "comment"), 
            "{ #comment\n");
    }

    {
        
        Lpf_Write_Options options = {0};
        options.put_space_before_comment = true;
        options.put_space_before_marker = true;
        options.put_space_before_value = true;
        lpf_test_lowlevel_write(options, lpf_test_entry(LPF_KIND_SCOPE_END, "label", "type", "val", "comment"), "} #comment\n");
    }
    #endif
}

/*

    bool okay = lpf_read_comment(reader, &comment);

    reader.skip_comments = true;

    //this is bad because it depends on the order of the thing.

    lpf_read_i64_or(reader, &val, 0);
    lpf_read_i64_or(reader, &val, 1);
    
    Entry_Array entries = {0};
    Scope_Array scopes = {0};
    lpf_read(reader, &entries, &scopes);

    for(Lpf_Scope_Iterator it = {0}; lpf_scope_get(&it, 1, &scopes); )
    {
        isize found = lpf_find_entry_from(entries, scope.entries, "label", "type", 0);
        
        Entry* resolution = lpf_get_entry(entries, scope.entries, "resolution");
        Entry* scale = lpf_get_entry(entries, scope.entries, "scale");
        Entry* offset = lpf_get_entry(entries, scope.entries, "offset");
        Entry* path = lpf_get_entry(entries, scope.entries, "path");

        read_texture(it.i1

    }

*/


void test_format_lpf()
{
    //@TODO: make the functions for reading writing all common types
    //@TODO: Rethink the lowlevel interface
    //@TODO: make truly loseless with trailing inline comments
    //@TODO: simplify writing code
    //@TODO: rename stuff
    //@TODO: renew the tests
    //@TODO: make a few large functionality tests
    //@TODO: make error handling consistant
    //@TODO: remove the pushing to part of readers / writers
    //@TODO: init deinit + allocator stuff
    //@TODO: put the escaping back into non low level
    //@TODO: measure perf

    Lpf_Dyn_Entry_Array read = {0};
    Lpf_Dyn_Entry_Array read_all = {0};
    Lpf_Dyn_Entry_Array read_meaningful = {0};

    String_Builder written = {0};

    String text = STRING(
        "#this is a texture!\n"
        "\n"
        "res i: 256\n"
        "texture TEX { #inline\n"
        "   \n"
        "   res i: 256\n"
        "   offset 6f: 0 0 0\n"
        "            , 1 1 1\n"
        "   scale: 0 0 0\n"
        "} #end comment \n"
        //",error continuation \n"
        "#hello after"
        );

    Lpf_Error error = lpf_read(&read, text);
    lpf_write(&written, &read);
    LOG_INFO("LPF", STRING_FMT, STRING_PRINT(written));
    array_clear(&written);
    
    Lpf_Error error_all = lpf_read_all(&read_all, text);
    lpf_write(&written, &read_all);
    LOG_INFO("LPF", STRING_FMT, STRING_PRINT(written));
    array_clear(&written);
    
    Lpf_Error error_meaningfull = lpf_read_meaningful(&read_meaningful, text);
    lpf_write(&written, &read_meaningful);
    LOG_INFO("LPF", STRING_FMT, STRING_PRINT(written));
    array_clear(&written);
    
    lpf_write_meaningful(&written, &read_all);
    LOG_INFO("LPF", STRING_FMT, STRING_PRINT(written));
    array_clear(&written);

    ASSERT(error == LPF_ERROR_NONE);
    ASSERT(error_all == LPF_ERROR_NONE);
    ASSERT(error_meaningfull == LPF_ERROR_NONE);

    lpf_test_write_lowlevel_entry();
    lpf_test_read_lowlevel_entry();
}