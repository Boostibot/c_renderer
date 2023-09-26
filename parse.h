#pragma once
#include "defines.h"
#include "string.h"

typedef enum Match_Kind
{
    MATCH_NORMAL,
    MATCH_INVERTED,
} Match_Kind;

EXPORT bool char_is_space(char c);
EXPORT bool char_is_digit(char c);
EXPORT bool char_is_lowercase(char c);
EXPORT bool char_is_uppercase(char c);
EXPORT bool char_is_alphabetic(char c);
EXPORT bool char_is_id(char c);

EXPORT bool match_char_custom(String str, isize* index, char c, Match_Kind match);
EXPORT bool match_any_of_custom(String str, isize* index, String any_of, Match_Kind match);
EXPORT bool match_whitespace_custom(String str, isize* index, Match_Kind match);
EXPORT bool match_char(String str, isize* index, char c);
EXPORT bool match_any_of(String str, isize* index, String any_of);
EXPORT bool match_sequence(String str, isize* index, String sequence);
EXPORT bool match_whitespace(String str, isize* index);
EXPORT bool match_id(String str, isize* index); //starts with _, [a-z], [A-Z] or _ then is followed by any number of [0-9], _, [a-z], [A-Z]

EXPORT bool match_decimal_u64(String str, isize* index, i64* out); //"00113000" -> 113000
EXPORT bool match_decimal_i64(String str, isize* index, i64* out); //"-00113000" -> -113000
EXPORT bool match_decimal_i32(String str, isize* index, i32* out); //"-00113000" -> -113000
EXPORT bool match_decimal_f32(String str, isize* index, f32* out); //"-0011.0300" -> -11.03000


EXPORT bool char_is_space(char c)
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

EXPORT bool char_is_digit(char c)
{
    return '0' <= c && c <= '9';
}

EXPORT bool char_is_lowercase(char c)
{
    return 'a' <= c && c <= 'z';
}

EXPORT bool char_is_uppercase(char c)
{
    return 'A' <= c && c <= 'Z';
}

EXPORT bool char_is_alphabetic(char c)
{
    //return ('a' <= c && c <= 'z') || ('A' <= c && c <= 'Z');

    //this is just a little flex of doing the two range checks in one
    //using the fact that all uppercase to lowercase leters are 32 appart.
    //That means we can just maks the fift bit and test once.
    //You can simply test this works by comparing the result of both approaches on all char values.
    char diff = c - 'A';
    char masked = diff & ~(1 << 5);
    bool is_letter = 0 <= masked && masked <= ('Z' - 'A');

    return is_letter;
}

//all characters permitted inside a common programming language id. [0-9], _, [a-z], [A-Z]
EXPORT bool char_is_id(char c)
{
    return char_is_digit(c) || char_is_alphabetic(c) || c == '_';
}


EXPORT bool match_char_custom(String str, isize* index, char c, Match_Kind match)
{
    if(*index < str.size && (str.data[*index] == c) == (match == MATCH_NORMAL))
    {
        *index += 1;
        return true;
    }
    return false;
}

EXPORT bool match_char(String str, isize* index, char c)
{
    return match_char_custom(str, index, c, MATCH_NORMAL);
}

EXPORT bool match_any_of_custom(String str, isize* index, String any_of, Match_Kind match)
{
    isize i = *index;
    for(; i < str.size; i++)
    {
        char current = str.data[i];
        bool found = false;
        for(isize j = 0; j < any_of.size; j++)
            if(any_of.data[j] == current)
            {
                found = true;
                break;
            }

        if(found != (match == MATCH_NORMAL))
            break;
    }

    bool matched = i != *index;
    *index = i;
    return matched;
}

EXPORT bool match_any_of(String str, isize* index, String any_of)
{
    return match_any_of_custom(str, index, any_of, MATCH_NORMAL);
}

EXPORT bool match_sequence(String str, isize* index, String sequence)
{
    if(string_has_substring_at(str, *index, sequence))
    {
        *index += sequence.size;
        return true;
    }
    return false;   
}

EXPORT bool match_whitespace_custom(String str, isize* index, Match_Kind match)
{
    isize i = *index;
    for(; i < str.size; i++)
    {
        if(char_is_space(str.data[i]) != (match == MATCH_NORMAL))
            break;
    }

    bool matched = i != *index;
    *index = i;
    return matched;
}


EXPORT bool match_whitespace(String str, isize* index)
{
    return match_whitespace_custom(str, index, MATCH_NORMAL);
}

EXPORT bool match_id_chars(String str, isize* index)
{
    isize i = *index;
    for(; i < str.size; i++)
    {
        if(char_is_id(str.data[i]) == false)
            break;
    }

    bool matched = i != *index;
    *index = i;
    return matched;
}

//starts with _, [a-z], [A-Z] or _ then is followed by any number of [0-9], _, [a-z], [A-Z]
EXPORT bool match_id(String str, isize* index)
{
    if(*index < str.size)
    {
        if(str.data[*index] == '_' || char_is_alphabetic(str.data[*index]))
        {
            *index += 1;
            for(; *index < str.size; *index += 1)
            {
                if(char_is_id(str.data[*index]) == false)
                    break;
            }

            return true;
        }
    }

    return false;
}

//matches a sequence of digits in decimal: "00113000" -> 113000
EXPORT bool match_decimal_u64(String str, isize* index, i64* out)
{
    u64 parsed = 0;
    isize i = *index;
    for(; i < str.size; i++)
    {
        u64 digit_value = (u64) str.data[i] - (u64) '0';
        if(digit_value > 9)
            break;

        u64 new_parsed = parsed*10 + digit_value;

        //Correctly handle overflow. This is correct because unsigned numbers
        //have defined overflow. We still however limit the number on INT64_MAX
        //to make our life easier when we eventually use signed.
        if(new_parsed < parsed)
            parsed = INT64_MAX;
        else
            parsed = new_parsed;

    }

    bool matched = i != *index;
    *index = i;

    ASSERT(parsed <= INT64_MAX);
    *out = (i64) parsed;
    return matched;
}

//matches a sequence of signed digits in decimal: "-00113000" -> -113000
EXPORT bool match_decimal_i64(String str, isize* index, i64* out)
{
    bool has_minus = match_char(str, index, '-');
    i64 uout = 0;
    bool matched_number = match_decimal_u64(str, index, &uout);
    if(has_minus)
        *out = -uout;
    else
        *out = uout;

    return matched_number;
}

EXPORT bool match_decimal_i32(String str, isize* index, i32* out)
{
    bool has_minus = match_char(str, index, '-');
    i64 uout = 0;
    bool matched_number = match_decimal_u64(str, index, &uout);
    if(has_minus)
        *out = (i32) -uout;
    else
        *out = (i32) uout;

    return matched_number;
}

INTERNAL f32 _quick_pow10f(isize power)
{
    switch(power)
    {
        case 0: return 1.0f;
        case 1: return 10.0f;
        case 2: return 100.0f;
        case 3: return 1000.0f;
        case 4: return 10000.0f;
        case 5: return 100000.0f;
        default: return powf(10.0f, (f32) power);
    };
}

INTERNAL f64 _quick_pow10lf(isize power)
{
    switch(power)
    {
        case 0: return 1.0;
        case 1: return 10.0;
        case 2: return 100.0;
        case 3: return 1000.0;
        case 4: return 10000.0;
        case 5: return 100000.0;
        default: return pow(10.0, (f64) power);
    };
}

EXPORT bool match_decimal_f32(String str, isize* index, f32* out)
{
    i64 before_dot = 0;
    i64 after_dot = 0;
    
    bool has_minus = match_char(str, index, '-');
    bool matched_before_dot = match_decimal_u64(str, index, &before_dot);
    if(!matched_before_dot)
        return false;

    match_char(str, index, '.');

    isize decimal_index = *index;
    match_decimal_u64(str, index, &after_dot);

    isize decimal_size = *index - decimal_index;
    f32 decimal_pow = _quick_pow10f(decimal_size);
    f32 result = (f32) before_dot + (f32) after_dot / decimal_pow;
    if(has_minus)
        result = -result;

    *out = result;
    return true;
}


INTERNAL void test_match()
{
    {
        isize index = 0;
        TEST(match_whitespace(STRING("   "), &index));
        TEST(index == 3);
    }
    
    {
        isize index = 0;
        TEST(match_whitespace(STRING("   \n \r \t "), &index));
        TEST(index == 9);
    }
    
    {
        isize index = 0;
        TEST(match_whitespace(STRING("a "), &index) == false);
        TEST(index == 0);
    }
    
    {
        isize index = 0;
        TEST(match_whitespace_custom(STRING("a "), &index, MATCH_INVERTED));
        TEST(index == 1);
    }
    
    {
        isize index = 0;
        TEST(match_whitespace_custom(STRING("a"), &index, MATCH_INVERTED));
        TEST(index == 1);
    }

    {
        f32 test_f32 = 0;
        isize index = 0;
        TEST(match_decimal_f32(STRING("12"), &index, &test_f32));
        TEST(is_near_scaledf(12.0f, test_f32, EPSILON));
    }
    
    {
        f32 test_f32 = 0;
        isize index = 0;
        TEST(match_decimal_f32(STRING("-12"), &index, &test_f32));
        TEST(is_near_scaledf(-12.0f, test_f32, EPSILON));
    }
    
    {
        f32 test_f32 = 0;
        isize index = 0;
        TEST(match_decimal_f32(STRING("-12.05"), &index, &test_f32));
        TEST(is_near_scaledf(-12.05f, test_f32, EPSILON));
    }
}