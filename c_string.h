#pragma once

#include <string.h>
#include "c_allocator.h"
#include "c_array.h"

typedef struct String
{
    const char* data;
    isize size;
} String;

DEFINE_ARRAY_TYPE(char, String_Builder);
DEFINE_ARRAY_TYPE(String, String_Array);

int* global_ptr2();
String string_make(const char* cstring); //converts a null terminated cstring into a String
String string_head(String string, isize to); //keeps only charcters to to ( [0, to) interval )
String string_tail(String string, isize from); //keeps only charcters from from ( [from, string.size) interval )
String string_limit(String string, isize max_size); //if the string is longer than max_size trims it to max_size else returns it unchanged 
String string_range(String string, isize from, isize to); //returns a string containing characters staring from from and ending in to ( [from, to) interval )
String string_portion(String string, isize from, isize size); //returns a string containing size characters staring from ( [from, from + size) interval )
bool   string_is_prefixed_with(String string, String prefix); 
bool   string_is_postfixed_with(String string, String postfix);
bool   string_is_equal(String a, String b); //Returns true if the contents and sizes of the strings match
int    string_compare(String a, String b); //Compares sizes and then lexographically the contents. Shorter strings are placed before longer ones.
isize  string_find_first(String string, String looking_for, isize from); 
isize  string_find_last_from(String in_str, String search_for, isize from);
isize  string_find_last(String string, String looking_for); 

//Returns a null terminated string contained in a string builder. The string is always null terminate even when the String_Builder is not yet initialized
const char*     builder_cstring(String_Builder builder); 
//Returns a String contained within string builder. The data portion of the string MIGHT be null and in that case its size == 0
String          builder_string(String_Builder builder); 
void            builder_append(String_Builder* builder, String string); //Appends a string
String_Builder  builder_from_string(String string); //Allocates a String_Builder from String. The String_Builder needs to be deinit just line any other ???_Array type!
String_Builder  builder_from_cstring(const char* cstring); //Allocates a String_Builder from cstring. The String_Builder needs to be deinit just line any other ???_Array type!
String_Builder  builder_from_string_alloc(String string, Allocator* allocator);  //Allocates a String_Builder from String using an allocator. The String_Builder needs to be deinit just line any other ???_Array type!

void string_join_into(String_Builder* append_to, const String* strings, isize strings_count, String separator); //Appends all strings in the strings array to append_to
void string_split_into(String_Array* append_to, String to_split, String split_by); //Splits the to_split string using split_by as a separator and appends the individual split parts into append_to

String_Builder string_join(String a, String b); //Allocates a new String_Builder with the concatenated String's a and b
String_Builder cstring_join(const char* a, const char* b); //Allocates a new String_Builder with the concatenated cstring's a and b
String_Builder string_join_any(const String* strings, isize strings_count, String separator); 
String_Array string_split(String to_split, String split_by);

