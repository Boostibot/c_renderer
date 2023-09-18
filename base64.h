#ifndef LIB_BASE64
#define LIB_BASE64

#include <stdint.h>

#ifndef ASSERT
#include <assert.h>
#define ASSERT(x) assert(x)
#endif

#ifndef EXPORT
    #define EXPORT
#endif

#define BASE64_ENCODING_SIZE            66  /* the required minimum size for the encoding table*/
#define BASE64_DECODING_SIZE            259 /* the required minimum size for the decoding table*/

//Returns the needed maximu output length of the output given the input_legth
EXPORT int64_t base64_encode_max_output_length(int64_t input_length);

//Returns the needed maximu output length of the output given the input_legth
EXPORT int64_t base64_decode_max_output_length(int64_t input_length);

//Encodes input_length bytes from data into out. 
//Out needs to be at least base64_encode_max_output_length() bytes!
//Returns the exact ammount of bytes written to out.
EXPORT int64_t base64_encode(void* out, const void* data, int64_t input_length, const uint8_t character_encoding[BASE64_ENCODING_SIZE]);

//Decodes input_length bytes from data into out. 
//If the input data is malformed (contains other characters than BASE64_DIGITS 
// or the pad '=' character at wrong places) saves index of the wrong data to error_at_or_null.
//error_at_or_null can be NULL and in that case nothing is written there. The function still stops at first error.
//Unlike other other base64 decoders we allow for multiple concatenated base64 blocks.
//That means it is valid for base64 stream to this function to contain '=' inside it as long as it makes sense
// within the previous block. The next block starts right after that normally.
//Out needs to be at least base64_encode_max_output_length() bytes!
//Returns the exact ammount of bytes written to out.
EXPORT int64_t base64_decode(void* out, const void* data, int64_t input_length, const uint8_t character_decoding[BASE64_DECODING_SIZE], int64_t* error_at_or_null);

#define BASE64_ENCODING_PAD_CHAR_I      64
#define BASE64_ENCODING_DO_PAD_CHAR_I   65 /* wheter or not to pad the encoded sequence with _PAD_CHAR (usually =). Specify P for pad other chars for no pad*/


#define BASE64_DECODING_PAD_CHAR_I      256
#define BASE64_DECODING_FORCE_PAD_I     257
#define BASE64_DECODING_FORCE_ENDING_I  258
#define BASE64_DECODING_ERROR_VALUE     255

//The encoding tables are effcitively [0, 63] -> char functions
//  Further the pad char is specified on the BASE64_ENCODING_PAD_CHAR_I (64) index.
// 
//The decoding tables are effcitively char -> [0, 63] functions. 
//  We also use BASE64_DECODING_ERROR_VALUE (255) to mark invalid entries not part of the encoding.
//  Further the pad char and additional settings can be set using the last few items fo the table. 
//  See below for explanation

//Url/filesystem safe encoding. We use this for everything. Formally RFC 4648 / Base64URL
static const uint8_t BASE64_ENCODING_URL[BASE64_ENCODING_SIZE + 1] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_=P";
static const uint8_t BASE64_ENCODING_URL_NO_PAD[BASE64_ENCODING_SIZE + 1] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_=N";

//common internet encoding
static const uint8_t BASE64_ENCODING_UTF8[BASE64_ENCODING_SIZE + 1] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/=P";

#define E BASE64_DECODING_ERROR_VALUE

//Common decoding that should work for *most* base64 encodings.
static const uint8_t BASE64_DECODING_UNIVERSAL[BASE64_DECODING_SIZE] = { 
    E, E, E, E, E, E, E, E, 
    E, E, E, E, E, E, E, E, 
    E, E, E, E, E, E, E, E, 
    E, E, E, E, E, E, E, E, 
    E, E, E, E, E, E, E, E, 
    //       '+' ',' '-' '.' '/'
    E, E, E, 62, 63, 62, 62, 63,
    
  //0   1   2   3   4   5   6   7
    52, 53, 54, 55, 56, 57, 58, 59,

 // 8   9
    60, 61, E, E, E, E, E, E, 

  //   A  B  C  ...
    E, 0, 1, 2, 3, 4, 5, 6, 
    7, 8, 9, 10, 11, 12, 13, 14, 
    15, 16, 17, 18, 19, 20, 21, 22, 

// ...  Y   Z               '_'
    23, 24, 25, E, E, E, E, 63, 

//     a   b   c   ...
    E, 26, 27, 28, 29, 30, 31, 32, 
    33, 34, 35, 36, 37, 38, 39, 40, 
    41, 42, 43, 44, 45, 46, 47, 48, 

//  ... y   z
    49, 50, 51, E, E, E, E, E, 
    E, E, E, E, E, E, E, E, 
    E, E, E, E, E, E, E, E, 
    E, E, E, E, E, E, E, E, 
    E, E, E, E, E, E, E, E, 
    E, E, E, E, E, E, E, E, 
    E, E, E, E, E, E, E, E, 
    E, E, E, E, E, E, E, E, 
    E, E, E, E, E, E, E, E, 
    E, E, E, E, E, E, E, E, 
    E, E, E, E, E, E, E, E, 
    E, E, E, E, E, E, E, E, 
    E, E, E, E, E, E, E, E, 
    E, E, E, E, E, E, E, E, 
    E, E, E, E, E, E, E, E, 
    E, E, E, E, E, E, E, E, 
    E, E, E, E, E, E, E, E,
    '=',   //separator character
    false, //wheter or not is ending required to have = or ==
    true,  //wheter or not to report an error if the stream ends with a single character
    
    //example: 
    //"YQ=="    -> "a" - corect
    //"YQ="     -> "a" - correct only if [BASE64_DECODING_FORCE_PAD_I] == false
    //"YQ"      -> "a" - correct only if [BASE64_DECODING_FORCE_PAD_I] == false
    //"Y"       -> ""  - correct only if [BASE64_DECODING_FORCE_ENDING_I] == false
    //""        -> ""  - correct

    //[BASE64_DECODING_FORCE_ENDING_I] should be true by default because the sequences which it enables lose data.
    
};

//Feel free enable to define tables for specific types 
#ifdef BASE64_SPECIFIC_DECONDINGS

static const uint8_t BASE64_DECODING_UTF8[BASE64_DECODING_SIZE] = { 
    E, E, E, E, E, E, E, E, 
    E, E, E, E, E, E, E, E, 
    E, E, E, E, E, E, E, E, 
    E, E, E, E, E, E, E, E, 
    E, E, E, E, E, E, E, E, 
    E, E, E, 62 /* '+' */, E, E, E, 63, /* '/' */
    52, 53, 54, 55, 56, 57, 58, 59, 
    60, 61, E, E, E, E, E, E, 
    E, 0, 1, 2, 3, 4, 5, 6, 
    7, 8, 9, 10, 11, 12, 13, 14, 
    15, 16, 17, 18, 19, 20, 21, 22, 
    23, 24, 25, E, E, E, E, E, 
    E, 26, 27, 28, 29, 30, 31, 32, 
    33, 34, 35, 36, 37, 38, 39, 40, 
    41, 42, 43, 44, 45, 46, 47, 48, 
    49, 50, 51, E, E, E, E, E, 
    E, E, E, E, E, E, E, E, 
    E, E, E, E, E, E, E, E, 
    E, E, E, E, E, E, E, E, 
    E, E, E, E, E, E, E, E, 
    E, E, E, E, E, E, E, E, 
    E, E, E, E, E, E, E, E, 
    E, E, E, E, E, E, E, E, 
    E, E, E, E, E, E, E, E, 
    E, E, E, E, E, E, E, E, 
    E, E, E, E, E, E, E, E, 
    E, E, E, E, E, E, E, E, 
    E, E, E, E, E, E, E, E, 
    E, E, E, E, E, E, E, E, 
    E, E, E, E, E, E, E, E, 
    E, E, E, E, E, E, E, E, 
    E, E, E, E, E, E, E, E,
    '=',   //separator character
    false, //wheter or not is ending required to have = or ==
    true,  //wheter or not to report an error if the stream ends with a single character
};


static const uint8_t BASE64_DECODING_URL[BASE64_DECODING_SIZE] = { 
    E, E, E, E, E, E, E, E, 
    E, E, E, E, E, E, E, E, 
    E, E, E, E, E, E, E, E, 
    E, E, E, E, E, E, E, E, 
    E, E, E, E, E, E, E, E, 
    E, E, E, E, E, 62, /* '-' */E, E, 
    52, 53, 54, 55, 56, 57, 58, 59, 
    60, 61, E, E, E, E, E, E, 
    E, 0, 1, 2, 3, 4, 5, 6, 
    7, 8, 9, 10, 11, 12, 13, 14, 
    15, 16, 17, 18, 19, 20, 21, 22, 
    23, 24, 25, E, E, E, E, 63, /* '_' */
    E, 26, 27, 28, 29, 30, 31, 32, 
    33, 34, 35, 36, 37, 38, 39, 40, 
    41, 42, 43, 44, 45, 46, 47, 48, 
    49, 50, 51, E, E, E, E, E, 
    E, E, E, E, E, E, E, E, 
    E, E, E, E, E, E, E, E, 
    E, E, E, E, E, E, E, E, 
    E, E, E, E, E, E, E, E, 
    E, E, E, E, E, E, E, E, 
    E, E, E, E, E, E, E, E, 
    E, E, E, E, E, E, E, E, 
    E, E, E, E, E, E, E, E, 
    E, E, E, E, E, E, E, E, 
    E, E, E, E, E, E, E, E, 
    E, E, E, E, E, E, E, E, 
    E, E, E, E, E, E, E, E, 
    E, E, E, E, E, E, E, E, 
    E, E, E, E, E, E, E, E, 
    E, E, E, E, E, E, E, E, 
    E, E, E, E, E, E, E, E,
    '=',   //separator character
    false, //wheter or not is ending required to have = or ==
    true,  //wheter or not to report an error if the stream ends with a single character
};

#endif

#undef E


#endif

#if (defined(LIB_ALL_IMPL) || defined(LIB_BASE64_IMPL)) && !defined(LIB_BASE64_HAS_IMPL)
#define LIB_BASE64_HAS_IMPL

EXPORT int64_t base64_encode_max_output_length(int64_t input_length)
{
    ASSERT(input_length >= 0);
    int64_t olen = (input_length + 2) / 3 * 4;
    ASSERT(olen >= input_length && "integer overflow!");
    return olen;
}

EXPORT int64_t base64_encode(void* _out, const void* _data, int64_t len, const uint8_t character_encoding[BASE64_ENCODING_SIZE])
{
    ASSERT(_out != NULL && _data != NULL);
    uint8_t* src = (uint8_t*) _data;
    uint8_t* end = src + len;
    uint8_t* in = src;
    uint8_t* out = (uint8_t*) _out;
    bool do_pad = character_encoding[BASE64_ENCODING_DO_PAD_CHAR_I] == 'P';
    uint8_t pad_char = character_encoding[BASE64_ENCODING_PAD_CHAR_I];

    #ifndef NDEBUG
    int64_t olen = base64_encode_max_output_length(len);
    #endif // !NDEBUG

    while (end - in >= 3) {
        *out++ = character_encoding[in[0] >> 2];
        *out++ = character_encoding[((in[0] & 0x03) << 4) | (in[1] >> 4)];
        *out++ = character_encoding[((in[1] & 0x0f) << 2) | (in[2] >> 6)];
        *out++ = character_encoding[in[2] & 0x3f];
        in += 3;
    }

    if (end - in) {
        *out++ = character_encoding[in[0] >> 2];
        if (end - in == 1) {
            *out++ = character_encoding[(in[0] & 0x03) << 4];
            if(do_pad)
                *out++ = pad_char;
        }
        else {
            *out++ = character_encoding[((in[0] & 0x03) << 4) |
                (in[1] >> 4)];
                *out++ = character_encoding[(in[1] & 0x0f) << 2];
        }
        if(do_pad)
            *out++ = pad_char;
    }

    int64_t written = (int64_t)(out - (uint8_t*) _out);
    ASSERT(0 <= written && written <= olen);
    return written;
}

//this is an upper estimate! We cannot know how many '=' are in the sequence so this simply doesnt take them into account.
EXPORT int64_t base64_decode_max_output_length(int64_t input_length)
{
    ASSERT(input_length >= 0);
    int64_t out_size = (input_length + 3) / 4 * 3;
    return out_size;
}

EXPORT int64_t base64_decode(void* _out, const void* _data, int64_t input_length, const uint8_t character_decoding[BASE64_DECODING_SIZE], int64_t* error_at)
{
    ASSERT(_out != NULL && _data != NULL);
    #define E BASE64_DECODING_ERROR_VALUE /* Error value */

    uint8_t pad_char = character_decoding[BASE64_DECODING_PAD_CHAR_I];
    bool force_pad = character_decoding[BASE64_DECODING_FORCE_PAD_I];
    bool force_ending = character_decoding[BASE64_DECODING_FORCE_ENDING_I];

    uint8_t* data = (uint8_t*) _data;
    uint8_t* out = (uint8_t*) _out;
    if(error_at)
        *error_at = -1;

    #ifndef NDEBUG
    int64_t max_length = base64_decode_max_output_length(input_length);
    #endif // !NDEBUG

    int64_t in_i = 0;
    int64_t out_i = 0;
    for(; in_i < input_length; in_i ++)
    {
        uint8_t values[4] = {0};

        for (; in_i + 4 <= input_length; in_i += 4)
        {
            //simply translate all values within a block
            values[0] = character_decoding[data[in_i + 0]];
            values[1] = character_decoding[data[in_i + 1]];
            values[2] = character_decoding[data[in_i + 2]];
            values[3] = character_decoding[data[in_i + 3]];

            //Check if it contains any strange characters (all of those were given the value E in the list)
            const uint32_t combined_by_bytes = values[0] << 24 | values[1] << 16 | values[2] << 8 | values[3] << 0;
            const uint32_t error_mask = (uint32_t) E << 24 | (uint32_t) E << 16 | (uint32_t) E << 8 | (uint32_t) E << 0;
            const uint32_t masked_combined = combined_by_bytes ^ error_mask;
            
            //https://graphics.stanford.edu/~seander/bithacks.html#ValueInWord
            #define haszero(v) (((v) - 0x01010101UL) & ~(v) & 0x80808080UL)

            bool has_error_value = haszero(masked_combined);

            #undef haszero

            if(has_error_value)
            {
                ASSERT(values[0] == E 
                    || values[1] == E
                    || values[2] == E
                    || values[3] == E);
                break;
            }

            //join them together
            uint32_t n = values[0] << 18 | values[1] << 12 | values[2] << 6 | values[3] << 0;

            //simply bit splice it into the data;
            out[out_i++] = (uint8_t) (n >> 16);
            out[out_i++] = (uint8_t) (n >> 8 & 0xFF);
            out[out_i++] = (uint8_t) (n & 0xFF);
        }

        if(in_i >= input_length)
            break;

        //find the exact location of the '=' character (or error) in the next 4 bytes 
        int64_t max_pad_i = 4;
        if(max_pad_i + in_i > input_length)
        {
            //If padding is forced the text must be in exact
            //chunks of 4 always
            if(force_pad)
            {
                if(error_at)
                    *error_at = input_length;
                break;
            }

            max_pad_i = input_length - in_i;
        }

        int64_t pad_at = 0;
        for(; pad_at < max_pad_i; pad_at++)
        {
            uint8_t curr = data[in_i + pad_at];
            uint8_t value = character_decoding[curr];
            values[pad_at] = value;

            //if found the problematic value break;
            if(value == E)
            {
                //if found something that is not pad_char then it is really an error
                //set error at and return.
                if(curr != pad_char)
                {
                    if(error_at)
                        *error_at = pad_at;
                    in_i = input_length;
                }
                break;
            }
        }
        
        uint32_t n = values[0] << 18 | values[1] << 12;
        switch(pad_at)
        {
            default:
                ASSERT(false && "unreachable!"); 
                break;

            case 0:
                //Nothing. On correctly structured data this
                //shoudl be impossible but we alow extra = when it doesnt
                //change the meaning of the data.
                break;

            case 1: 
                //This is incorrect. The first output byte would then have only 6 bits of data.
                //However we signal an error only of force_ending is on
                if(force_ending)
                {
                    if(error_at)
                        *error_at = in_i + pad_at;
                    in_i = input_length - pad_at;
                }
                break;
                
            case 2: 
                out[out_i++] = (uint8_t) (n >> 16);
                break;

            case 3: 
                n |= values[2] << 6;
                out[out_i++] = (uint8_t) (n >> 16);
                out[out_i++] = (uint8_t) (n >> 8 & 0xFF);
                break;
        }
        
        in_i += pad_at;
    }
    
    #ifndef NDEBUG
    ASSERT(0 <= out_i && out_i <= max_length);
    ASSERT(0 <= in_i && in_i <= input_length + 4);
    #endif // !NDEBUG

    #undef E
    return out_i;
}


#endif