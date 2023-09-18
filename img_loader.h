#pragma once

#include "string.h"
#include "file.h"

#include "allocator_wrapper.h"

EXPORT void* wrapper_allocator_malloc(Allocator* using_allocator, isize new_size, isize align, Source_Info called_from);
EXPORT void* wrapper_allocator_realloc(void* ptr, isize new_size, Source_Info called_from);
EXPORT void wrapper_allocator_free(void* ptr, Source_Info called_from);

#define STBI_MALLOC(size)           wrapper_allocator_malloc(allocator_get_default(), size, DEF_ALIGN, SOURCE_INFO())
#define STBI_REALLOC(ptr, new_size) wrapper_allocator_realloc(ptr, new_size, SOURCE_INFO())
#define STBI_FREE(ptr)              wrapper_allocator_free(ptr, SOURCE_INFO())
#define STBI_ASSERT(x)              ASSERT(x)
#define STBI_WINDOWS_UTF8

#define STBIW_MALLOC(size)           STBI_MALLOC(size)
#define STBIW_REALLOC(ptr, new_size) STBI_REALLOC(ptr, new_size)
#define STBIW_FREE(ptr)              STBI_FREE(ptr)
#define STBIW_ASSERT(x)              STBI_ASSERT(x)
#define STBIW_WINDOWS_UTF8

#include "extrenal/include/stb/stb_image.h"
#include "extrenal/include/stb/stb_image_write.h"

typedef enum Image_Parse_Format {
    IMG_PARSE_PNG,
    IMG_PARSE_BMP,
    IMG_PARSE_TGA,
    IMG_PARSE_JPG,
    IMG_PARSE_HDR,
} Image_Parse_Format;

typedef enum Image_Pixel_Format {
    IMG_FORMAT_U8 = 0,
    IMG_FORMAT_U16,
    IMG_FORMAT_F32,
} Image_Pixel_Format;

typedef struct Image_Channels {
    i8 channel_count;
    Image_Pixel_Format format;
    c8 channel_layout[6]; 
    //each channel_layout can be one of the following 
    //'r': red 
    //'g': green
    //'b': blue 
    //'a': alpha
    //'d': depth
    //'s': stencil
    
    //or
    //'h':hue
    //'s':saturation
    //'l'/'v':ligtness/value
};

typedef struct Image
{
    u8_Array pixels;
    String_Builder path;
    Image_Channels channels;

    i32 width;
    i32 height;
} Image;

typedef struct Image_View {
    u8* pixels;
    isize pixels_size;
    Image_Channels channels;

    i32 width;
    i32 height;

    i32 from_x;
    i32 from_y;
    
    i32 to_x;
    i32 to_y;
} Image_View;

void img_init(Image* image, Allocator* alloc);
void img_deinit(Image* image);

Error img_read_from_memory(Image* image, String data, isize desired_channels, Image_Pixel_Format format, String path);
Error img_read_from_file(Image* image, String path, isize desired_channels, Image_Pixel_Format format);

Error img_write_to_memory(Image_View image, String_Builder* into, isize desired_channels, Image_Parse_Format format);
Error img_write_to_file_formatted(Image_View image, String path, isize desired_channels, Image_Parse_Format format);
Error img_write_to_file(Image_View image, String path, isize desired_channels);

Image_View img_view_from_image(const Image* image);
Image_View img_view_portion(Image_View view, i32 from_x, i32 from_y, i32 width, i32 height);
Image_View img_view_range(Image_View view, i32 from_x, i32 from_y, i32 to_x, i32 to_y);
void img_paste(Image* to_image, Image_View image, i32 offset_x, i32 offset_y);


// ========================= IMPL =====================

#define STB_IMAGE_WRITE_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION

#include "extrenal/include/stb/stb_image.h"
#include "extrenal/include/stb/stb_image_write.h"

#include "log.h"

void img_init(Image* image, Allocator* alloc)
{
    img_deinit(image);
    array_init(&image->pixels, alloc);
    array_init(&image->path, alloc);
}

void img_deinit(Image* image)
{
    array_deinit(&image->pixels);
    array_deinit(&image->path);
    memset(image, 0, sizeof *image);
}

static Hash_Index error_hash = {0};

INTERNAL u32 _stb_image_error_module()
{
    static u32 error_module = 0;
    if(error_module == 0)
    {
        error_module = error_system_register_module(translate_obj_parser_error, STRING("stb_image.h"), &error_hash);
        hash_index_init(&error_hash, allocator_get_static());
    }

    return error_module;
}


INTERNAL Error _stb_image_push_and_return_last_error(const char* error_string)
{
    u32 module = _stb_image_error_module();

    u32 hashed = hash64_to32((u64) error_string);
    isize found_code = hash_index_find_or_insert(&error_hash, hashed);
    error_hash.entries[found_code].value = (u64) error_string;

    return error_make(module, hashed);
}

INTERNAL String translate_obj_parser_error(u32 code, void* context)
{
    Hash_Index* error_hash = (Hash_Index*) context;
    isize found = hash_index_find(*error_hash, code);
    if(found == -1)
        return STRING("unknown error. This is likely an internal bug.");
    else
    {
        const char* string = (const char*) error_hash->entries[found].value;
        return string_make(string);
    }

}

INTERNAL isize _pixel_format_size(Image_Pixel_Format format)
{
    switch(format)
    {
        default: ASSERT(false);
        case IMG_FORMAT_U8:
            return 1;
            break;

        case IMG_FORMAT_U16:
            return 2;
            break;

        case IMG_FORMAT_F32:
            return 4;
            break;
    }
}

Error img_read_from_memory(Image* image, String data, isize desired_channels, Image_Pixel_Format format, String path)
{
    Allocator* used_allocator = array_get_allocator(image->pixels);
    
    int width = 0;
    int height = 0;
    int channels = 0;

    Error error = {0};
    void* allocated = NULL;
    isize channel_size = _pixel_format_size(format);
    
    bool flip = false;
    stbi_set_flip_vertically_on_load(flip);
    switch(format)
    {
        default: ASSERT(false);
        case IMG_FORMAT_U8:
            allocated = stbi_load_from_memory((const u8*) data.data, data.size, &width, &height, &channels, (int) desired_channels);
            break;

        case IMG_FORMAT_U16:
            allocated = stbi_load_16_from_memory((const u8*) data.data, data.size, &width, &height, &channels, (int) desired_channels);
            break;

        case IMG_FORMAT_F32:
            allocated = stbi_loadf_from_memory((const u8*) data.data, data.size, &width, &height, &channels, (int) desired_channels);
            break;
    }

    if(allocated)
    {
        array_deinit(&image->pixels);
        array_init(&image->pixels, wrapper_allocator_get_default());
        array_clear(&image->path);
        builder_append(&image->path, path);

        image->pixels.data = (u8*) allocated;
        image->pixels.size = (isize) width * (isize) height * channel_size;
        image->pixels.capacity = image->pixels.size; //we are braking the array invariants here :( of course! Time to make the string_builder struct its own thing!
        image->width = width;
        image->height = height;
        image->channels.channel_count = channels;

        const char channel_name_progression[7] = "rgbads";
        for(isize i = 0; i < MIN(channels, 6); i++)
            image->channels.channel_layout[i] = channel_name_progression[i];

        image->channels.format = format;
        error = error_make(_stb_image_error_module(), 0);
    }
    else
    {
        LOG_ERROR("ASSET", "Failed to load an image: \"" STRING_FMT "\"\n"
            "error: %s", STRING_PRINT(path), stbi_failure_reason());
        error = _stb_image_push_and_return_last_error(stbi_failure_reason());
    }

    return error;
}

Error img_read_from_file(Image* image, String path, isize desired_channels, Image_Pixel_Format format)
{
    String_Builder file_content = {allocator_get_scratch()};
    Error file_error = file_read_entire(path, &file_content);
    Error parse_error = ERROR_OR(file_error) img_read_from_memory(image, string_from_builder(file_content), desired_channels, format, path);

    return parse_error;
}

STBIWDEF int stbi_write_png(char const *filename, int w, int h, int comp, const void  *data, int stride_in_bytes);
STBIWDEF int stbi_write_bmp(char const *filename, int w, int h, int comp, const void  *data);
STBIWDEF int stbi_write_tga(char const *filename, int w, int h, int comp, const void  *data);
STBIWDEF int stbi_write_hdr(char const *filename, int w, int h, int comp, const float *data);
STBIWDEF int stbi_write_jpg(char const *filename, int x, int y, int comp, const void  *data, int quality);

Error img_write_to_file_formatted(Image_View image, String path, Image_Parse_Format format)
{
    String_Builder escaped = {0};
    array_init_backed(&escaped, allocator_get_scratch(), 256);
    builder_append(&escaped, path);

    Image_Channels required_channels = {0};
    void* data = NULL;
    switch(format)
    {
        default: ASSERT(false);
        case IMG_PARSE_PNG: {
            required_channels = image.channels; 
        } break;
        case IMG_PARSE_BMP: {
            required_channels = image.channels;
        } break;
        case IMG_PARSE_TGA: {
            required_channels = image.channels;
        } break;
        case IMG_PARSE_JPG: {
            required_channels = image.channels;

        } break;
        case IMG_PARSE_HDR: {

        } break;

    }
}