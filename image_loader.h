#ifndef LIB_IMAGE_LOADER
#define LIB_IMAGE_LOADER

#include "image.h"
#include "string.h"
#include "file.h"

#include "allocator_wrapper.h"

typedef enum Image_File_Format {
    IMG_FILE_FORMAT_NONE = 0,
    IMG_FILE_FORMAT_PNG,
    IMG_FILE_FORMAT_BMP,
    IMG_FILE_FORMAT_TGA,
    IMG_FILE_FORMAT_JPG,
    IMG_FILE_FORMAT_HDR,
} Image_File_Format;

EXPORT Image_File_Format image_file_format_from_extension(String extension_without_dot);

EXPORT Error image_read_from_memory(Image* image, String data, isize desired_channels, Image_Pixel_Format format, String path);
EXPORT Error image_read_from_file(Image* image, String path, isize desired_channels, Image_Pixel_Format format);

EXPORT Error image_write_to_memory(Image_View image, String_Builder* into, Image_File_Format format);
EXPORT Error image_write_to_file_formatted(Image_View image, String path, Image_File_Format format);
EXPORT Error image_write_to_file(Image_View image, String path);

#endif

#if (defined(LIB_ALL_IMPL) || defined(LIB_IMAGE_LOADER_IMPL)) && !defined(LIB_IMAGE_LOADER_HAS_IMPL)
#define LIB_IMAGE_LOADER_HAS_IMPL
// ========================= IMPL =====================

#define STBI_MALLOC(size)           wrapper_allocator_malloc(allocator_get_default(), size, DEF_ALIGN, SOURCE_INFO())
#define STBI_REALLOC(ptr, new_size) wrapper_allocator_realloc(allocator_get_default(), ptr, new_size, DEF_ALIGN, SOURCE_INFO())
#define STBI_FREE(ptr)              wrapper_allocator_free(ptr, SOURCE_INFO())
#define STBI_ASSERT(x)              ASSERT(x)
#define STBI_WINDOWS_UTF8

#define STBIW_MALLOC(size)           STBI_MALLOC(size)
#define STBIW_REALLOC(ptr, new_size) STBI_REALLOC(ptr, new_size)
#define STBIW_FREE(ptr)              STBI_FREE(ptr)
#define STBIW_ASSERT(x)              STBI_ASSERT(x)
#define STBIW_WINDOWS_UTF8
#define STB_IMAGE_WRITE_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION

#include "extrenal/include/stb/stb_image.h"
#include "extrenal/include/stb/stb_image_write.h"

#include "log.h"


static Hash_Index error_hash = {0};

INTERNAL String translate_stb_error(u32 code, void* context)
{
    (void) context;
    isize found = hash_index_find(error_hash, code);
    if(found == -1)
        return STRING("unknown error. This is likely an internal bug.");
    else
    {
        const char* string = (const char*) error_hash.entries[found].value;
        return string_make(string);
    }
}

INTERNAL u32 _stb_image_error_module()
{
    static u32 error_module = 0;
    if(error_module == 0)
    {
        error_module = error_system_register_module(translate_stb_error, STRING("stb_image.h"), &error_hash);
        hash_index_init(&error_hash, allocator_get_static());
    }

    return error_module;
}

INTERNAL Error _stb_image_push_and_return_last_error(const char* error_string)
{
    u32 module = _stb_image_error_module();

    //We assume no hash collisions.
    u32 hashed = hash64_to32((u64) error_string);
    isize found_code = hash_index_find_or_insert(&error_hash, hashed, 0);

    if(error_hash.entries[found_code].value != 0)
        ASSERT(strcmp((const char*) error_hash.entries[found_code].value, error_string) == 0);

    error_hash.entries[found_code].value = (u64) error_string;
    return error_make(module, hashed);
}

EXPORT Error image_read_from_memory(Image* image, String data, isize desired_channels, Image_Pixel_Format format, String path)
{
    int width = 0;
    int height = 0;
    int channels = 0;

    Error error = {0};
    void* allocated = NULL;
    
    bool flip = false;
    stbi_set_flip_vertically_on_load(flip);
    switch(format)
    {
        default: ASSERT(false);
        case PIXEL_FORMAT_U8:
            allocated = stbi_load_from_memory((const u8*) data.data, (int) data.size, &width, &height, &channels, (int) desired_channels);
            break;

        case PIXEL_FORMAT_U16:
            allocated = stbi_load_16_from_memory((const u8*) data.data, (int) data.size, &width, &height, &channels, (int) desired_channels);
            break;

        case PIXEL_FORMAT_F32:
            allocated = stbi_loadf_from_memory((const u8*) data.data, (int) data.size, &width, &height, &channels, (int) desired_channels);
            break;
    }

    if(allocated)
    {
        image_init(image, wrapper_allocator_get_default());
        builder_assign(&image->path, path);

        image->format = format;
        image->pixels = (u8*) allocated;
        image->width = (i32) width;
        image->height = (i32) height;
        image->pixel_size = (u16) channels * (u16) image_pixel_format_size(format);

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

EXPORT Error image_read_from_file(Image* image, String path, isize desired_channels, Image_Pixel_Format format)
{
    String_Builder file_content = {allocator_get_scratch()};
    Error file_error = file_read_entire(path, &file_content);
    Error parse_error = ERROR_OR(file_error) image_read_from_memory(image, string_from_builder(file_content), desired_channels, format, path);

    return parse_error;
}

EXPORT INTERNAL void _stbi_write_to_memory(void* context, void* data, int size)
{
    String_Builder* append_into = (String_Builder*) context;
    array_append(append_into, (char*) data, size);
}

EXPORT Error image_write_to_memory(Image_View image, String_Builder* into, Image_File_Format file_format)
{
    const char* error_msg_unspecfied =          "Unspecified error while formatting into memory (Zero sized image?)";
    const char* error_msg_bad_type =            "Output format does not support the representation format data type";
    const char* error_msg_bad_chanel_count =    "Output format does not support the representation format number of channels";
    const char* error_msg_bad_file_format =     "Invalid output format (IMG_FILE_FORMAT_NONE or bad value)";

    int jpg_compression_quality = 50; //[0, 100]

    array_clear(into);

    Image contiguous = {allocator_get_scratch()};

    #if 0
    image.width = image.containing_width;
    image.height = image.containing_height;
    image.from_x = 0;
    image.from_y = 0;
    #else
    //not contigous in memory => make contiguous copy
    if(image.from_x != 0 || image.from_y != 0)
    {
        contiguous.format = image.format;
        contiguous.pixel_size = image.pixel_size;
        image_resize(&contiguous, image.width, image.height);
        Image_View to_image = image_view_from_image(&contiguous);
        image_view_copy(&to_image, image, 0, 0);
    
        image = to_image;
    }
    #endif


    Error out_error = {0};
    isize channel_count = image_view_channel_count(image);
    bool had_internal_error = false;
    switch(file_format)
    {
        case IMG_FILE_FORMAT_PNG: {
            isize stride = image_view_byte_stride(image);;
            if(image.format != PIXEL_FORMAT_U8)
                out_error = _stb_image_push_and_return_last_error(error_msg_bad_type);
            else
                had_internal_error = !stbi_write_png_to_func(_stbi_write_to_memory, into, (int) image.width, (int) image.height, (int) channel_count, image.pixels, (int) stride);
        } break;
        
        case IMG_FILE_FORMAT_BMP: {
            if(image.format != PIXEL_FORMAT_U8)
                out_error = _stb_image_push_and_return_last_error(error_msg_bad_type);
            else
                had_internal_error = !stbi_write_bmp_to_func(_stbi_write_to_memory, into, (int) image.width, (int) image.height, (int) channel_count, image.pixels);
        } break;
        
        case IMG_FILE_FORMAT_TGA: {
            if(image.format != PIXEL_FORMAT_U8)
                out_error = _stb_image_push_and_return_last_error(error_msg_bad_type);
            else
                had_internal_error = !stbi_write_tga_to_func(_stbi_write_to_memory, into, (int) image.width, (int) image.height, (int) channel_count, image.pixels);
        } break;
        
        case IMG_FILE_FORMAT_JPG: {
            if(image.format != PIXEL_FORMAT_U8)
                out_error = _stb_image_push_and_return_last_error(error_msg_bad_type);
            else if(image_view_channel_count(image) > 3)
                out_error = _stb_image_push_and_return_last_error(error_msg_bad_chanel_count);
            else
                had_internal_error = !stbi_write_jpg_to_func(_stbi_write_to_memory, into, (int) image.width, (int) image.height, (int) channel_count, image.pixels, jpg_compression_quality);
        } break;
        
        case IMG_FILE_FORMAT_HDR: {
            if(image.format != PIXEL_FORMAT_F32)
                out_error = _stb_image_push_and_return_last_error(error_msg_bad_type);
            else if(image_view_channel_count(image) > 3)
                out_error = _stb_image_push_and_return_last_error(error_msg_bad_chanel_count);
            else
                had_internal_error = !stbi_write_hdr_to_func(_stbi_write_to_memory, into, (int) image.width, (int) image.height, (int) channel_count, (float*) (void*) image.pixels);
        } break;

        case IMG_FILE_FORMAT_NONE:
        default: 
            out_error = _stb_image_push_and_return_last_error(error_msg_bad_file_format);
        break;
    }

    if(had_internal_error)
        out_error = ERROR_OR(out_error) _stb_image_push_and_return_last_error(error_msg_unspecfied);

    image_deinit(&contiguous);
    return out_error;
}

EXPORT Error image_write_to_file_formatted(Image_View image, String path, Image_File_Format file_format)
{
    String_Builder formatted = {allocator_get_scratch()};
    Error format_error = image_write_to_memory(image, &formatted, file_format);
    Error output_error = ERROR_OR(format_error) file_write_entire(path, string_from_builder(formatted));

    array_deinit(&formatted);

    return output_error;
}

EXPORT Error image_write_to_file(Image_View image, String path)
{
    isize last_dot_i = string_find_last_char(path, '.') + 1;
    CHECK_BOUNDS(last_dot_i, path.size + 1);

    String extension = string_tail(path, last_dot_i);
    Image_File_Format file_format = image_file_format_from_extension(extension);

    return image_write_to_file_formatted(image, path, file_format);
} 

EXPORT Image_File_Format image_file_format_from_extension(String extension_without_dot)
{
    String ext = extension_without_dot;
    
    if(false)
    {}
    else if(string_is_equal(ext, STRING("png")) || string_is_equal(ext, STRING("PNG")))
        return IMG_FILE_FORMAT_PNG;
    else if(string_is_equal(ext, STRING("bmp")) || string_is_equal(ext, STRING("BMP")))
        return IMG_FILE_FORMAT_BMP;
    else if(string_is_equal(ext, STRING("tga")) || string_is_equal(ext, STRING("TGA")))
        return IMG_FILE_FORMAT_TGA;
    else if(string_is_equal(ext, STRING("jpg")) || string_is_equal(ext, STRING("JPG")))
        return IMG_FILE_FORMAT_JPG;
    else if(string_is_equal(ext, STRING("hdr")) || string_is_equal(ext, STRING("HDR")))
        return IMG_FILE_FORMAT_HDR;
    else
        return IMG_FILE_FORMAT_NONE;
}

#endif