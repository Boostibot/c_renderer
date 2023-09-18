#ifndef LIB_FORMAT
#define LIB_FORMAT

#include "vformat.h"
#include "base64.h"

#define PREFORMAT_LEAST_BUFFER_SIZE 64

//Extremely performant int to string conversion function. Source: fmtlib format_decimal()
//Writes some number of digits into the buffer. 
//The buffer needs to be at least PREFORMAT_LEAST_BUFFER_SIZE sized.
//Saves range [written_from, written_to) in which the result is stored
EXPORT void preformat_decimal(char* buffer, uint64_t value, int64_t size, uint64_t* written_from, uint64_t* written_to) 
{
    // Converts value in the range [0, 100) to a string.
    #define TWO_DIGITS_TO_STRING(value)             \
        &"0001020304050607080910111213141516171819" \
         "2021222324252627282930313233343536373839" \
         "4041424344454647484950515253545556575859" \
         "6061626364656667686970717273747576777879" \
         "8081828384858687888990919293949596979899"[(value) * 2] \

    char* out = buffer + size;
    while (value >= 100) 
    {
        // Integer division is slow so do it for a group of two digits instead
        // of for every digit. The idea comes from the talk by Alexandrescu
        // "Three Optimization Tips for C++". See speed-test for a comparison.
        out -= 2;
        const char* two_digits = TWO_DIGITS_TO_STRING(value % 100);
        memcpy(out, two_digits, 2);
        value /= 100;
    }

    if (value < 10) 
    {   
        out -= 1;
        *out = (char) ('0' + value);
    }
    else
    {
        out -= 2;
        const char* two_digits = TWO_DIGITS_TO_STRING(value);
        memcpy(out, two_digits, 2);
    }

    #undef TWO_DIGITS_TO_STRING

    *written_from = (uint64_t) (out - buffer);
    *written_to = size;
}

//Writes some number of digits into the buffer. 
//The buffer needs to be at least PREFORMAT_LEAST_BUFFER_SIZE sized.
//Saves range [written_from, written_to) in which the result is stored
EXPORT void preformat_uint(char* buffer, uint64_t num, int64_t size, uint8_t base, const char digits[64], uint64_t* written_from, uint64_t* written_to)
{
    ASSERT(base <= 36 && base >= 2);
    int used_size = 0;
    uint64_t last = num;
    while(true)
    {
        uint64_t div = last / base;
        uint64_t last_digit = last % base;

        buffer[size - 1 - used_size] = digits[last_digit];
        used_size ++;

        last = div;
        if(last == 0)
            break;
    }

    *written_from = size - used_size;
    *written_to = size;
}


//we dont use / and order this differently from base64 standard encoding because / is not filesystem compatible
//the 65th char is the separator. There we stick to customs and use =. 
//(We need 66 chars because C strings are null terminated and I dont want to type each char idividually)
const char CUSTOM_BASE64_DIGITS[66] = "0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ_+=";

EXPORT void format_udecimal_into(String_Builder* into, u64 num)
{
    char buffer[PREFORMAT_LEAST_BUFFER_SIZE + 2]; //explicitly unitilialized
    buffer[PREFORMAT_LEAST_BUFFER_SIZE + 1] = '\0'; //null terminated
    uint64_t from; //explicitly unitilialized
    uint64_t to; //explicitly unitilialized

    preformat_decimal(buffer, num, PREFORMAT_LEAST_BUFFER_SIZE + 1, &from, &to);
    array_append(into, buffer + from, to - from);
}

EXPORT void format_decimal_into(String_Builder* into, i64 num)
{
    char buffer[PREFORMAT_LEAST_BUFFER_SIZE + 2]; //explicitly unitilialized
    buffer[PREFORMAT_LEAST_BUFFER_SIZE + 1] = '\0'; //null terminated
    uint64_t from; //explicitly unitilialized
    uint64_t to; //explicitly unitilialized

    preformat_decimal(buffer, llabs(num), PREFORMAT_LEAST_BUFFER_SIZE + 1, &from, &to);
    if(num < 0)
        buffer[--from] = '-';

    array_append(into, buffer + from, to - from);
}

EXPORT void format_int_into(String_Builder* into, i64 num, u8 base)
{
    if(base == 10)
    {
        format_decimal_into(into, num);
    }
    else
    {
        char buffer[PREFORMAT_LEAST_BUFFER_SIZE + 2]; //explicitly unitilialized
        buffer[PREFORMAT_LEAST_BUFFER_SIZE + 1] = '\0'; //null terminated
        uint64_t from; //explicitly unitilialized
        uint64_t to; //explicitly unitilialized

        preformat_uint(buffer, llabs(num), PREFORMAT_LEAST_BUFFER_SIZE + 1, base, CUSTOM_BASE64_DIGITS, &from, &to);
        if(num < 0)
            buffer[--from] = '-';
        array_append(into, buffer + from, to - from);
    }
}

EXPORT void format_uint_into(String_Builder* into, u64 num, u8 base)
{
    if(base == 10)
    {
        format_udecimal_into(into, num);
    }
    else
    {
        char buffer[PREFORMAT_LEAST_BUFFER_SIZE + 2]; //explicitly unitilialized
        buffer[PREFORMAT_LEAST_BUFFER_SIZE + 1] = '\0'; //null terminated
        uint64_t from; //explicitly unitilialized
        uint64_t to; //explicitly unitilialized

        preformat_uint(buffer, num, PREFORMAT_LEAST_BUFFER_SIZE + 1, base, CUSTOM_BASE64_DIGITS, &from, &to);
        array_append(into, buffer + from, to - from);
    }
}

EXPORT void base64_encode_append_into(String_Builder* into, const void* data, isize len, const uint8_t encoding[BASE64_ENCODING_SIZE])
{
    isize size_before = into->size;
    isize needed = base64_encode_max_output_length(len);
    array_resize(into, size_before + needed);

    isize actual_size = base64_encode(into->data + size_before, data, len, encoding);
    array_resize(into, size_before + actual_size);
}

EXPORT bool base64_decode_append_into(String_Builder* into, const void* data, isize len, const uint8_t decoding[BASE64_DECODING_SIZE])
{
    isize size_before = into->size;
    isize needed = base64_decode_max_output_length(len);
    array_resize(into, size_before + needed);
    
    isize error_at = 0;
    isize actual_size = base64_decode(into->data + size_before, data, len, decoding, &error_at);
    if(error_at == -1)
    {
        array_resize(into, size_before + actual_size);
        return true;
    }
    else
    {
        array_resize(into, size_before);
        return false;
    }
}

EXPORT void base64_encode_into(String_Builder* into, const void* data, isize len, const uint8_t encoding[BASE64_ENCODING_SIZE])
{
    array_clear(into);
    base64_encode_append_into(into, data, len, encoding);
}

EXPORT bool base64_decode_into(String_Builder* into, const void* data, isize len, const uint8_t decoding[BASE64_DECODING_SIZE])
{
    array_clear(into);
    return base64_decode_append_into(into, data, len, decoding);
}

#endif // !LIB_FORMAT

#if (defined(LIB_ALL_IMPL) || defined(LIB_FORMAT_IMPL)) && !defined(LIB_FORMAT_HAS_IMPL)
#define LIB_FORMAT_HAS_IMPL
    
#endif
