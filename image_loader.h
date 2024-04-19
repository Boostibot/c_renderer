#ifndef LIB_IMAGE_LOADER
#define LIB_IMAGE_LOADER

#include "lib/image.h"
#include "lib/string.h"
#include "lib/file.h"

#include "lib/allocator_wrapper.h"

typedef enum Image_File_Format {
    IMAGE_LOAD_FILE_FORMAT_NONE = 0,
    IMAGE_LOAD_FILE_FORMAT_PNG,
    IMAGE_LOAD_FILE_FORMAT_BMP,
    IMAGE_LOAD_FILE_FORMAT_TGA,
    IMAGE_LOAD_FILE_FORMAT_JPG,
    IMAGE_LOAD_FILE_FORMAT_HDR,
} Image_File_Format;


enum {
    IMAGE_LOAD_FLAG_FLIP_Y = 1,
    IMAGE_LOAD_FLAG_FLIP_X = 2,
};

EXPORT Image_File_Format image_file_format_from_extension(String extension_without_dot);

EXPORT bool image_read_from_memory(Image* image, String data, isize desired_channels, Pixel_Type format, i32 flags);
EXPORT bool image_read_from_file(Image* image, String path, isize desired_channels, Pixel_Type format, i32 flags);

EXPORT bool image_write_to_memory(Subimage image, String_Builder* into, Image_File_Format format);
EXPORT bool image_write_to_file_formatted(Subimage image, String path, Image_File_Format format);
EXPORT bool image_write_to_file(Subimage image, String path);

#endif

#if (defined(LIB_ALL_IMPL) || defined(LIB_IMAGE_LOADER_IMPL)) && !defined(LIB_IMAGE_LOADER_HAS_IMPL)
#define LIB_IMAGE_LOADER_HAS_IMPL
// ========================= IMPL =====================

#define STBI_MALLOC(size)           wrapper_allocator_malloc(allocator_get_default(), size, DEF_ALIGN)
#define STBI_REALLOC(ptr, new_size) wrapper_allocator_realloc(allocator_get_default(), ptr, new_size, DEF_ALIGN)
#define STBI_FREE(ptr)              wrapper_allocator_free(ptr)
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

#include "lib/log.h"

EXPORT bool image_read_from_memory(Image* image, String data, isize desired_channels, Pixel_Type format, i32 flags)
{
    bool state = true;
    
    Allocator_Set prev_allocs = {0};
    if(image->allocator)
        prev_allocs = allocator_set_default(image->allocator);

    int width = 0;
    int height = 0;
    int channels = 0;
    void* allocated = NULL;
    stbi_set_flip_vertically_on_load((flags & IMAGE_LOAD_FLAG_FLIP_Y) > 0);

    #pragma warning(disable:4061) //Dissables "'PIXEL_TYPE_I24' in switch of enum 'Pixel_Type' is not explicitly handled by a case label"
    switch(format)
    {
        case PIXEL_TYPE_U8:
            allocated = stbi_load_from_memory((const u8*) data.data, (int) data.size, &width, &height, &channels, (int) desired_channels);
            break;

        case PIXEL_TYPE_U16:
            allocated = stbi_load_16_from_memory((const u8*) data.data, (int) data.size, &width, &height, &channels, (int) desired_channels);
            break;

        case PIXEL_TYPE_F32:
            allocated = stbi_loadf_from_memory((const u8*) data.data, (int) data.size, &width, &height, &channels, (int) desired_channels);
            break;

        case PIXEL_TYPE_U24:
        case PIXEL_TYPE_U32:
        default: 
            ASSERT(false);
            break;
    }
    #pragma warning(default:4061)

    if(allocated)
    {
        image_init(image, wrapper_allocator_get_default(), channels, format);
        image->pixels = (u8*) allocated;
        image->width = (i32) width;
        image->height = (i32) height;
    }
    else
    {
        LOG_ERROR("ASSET", "%s", stbi_failure_reason());
        state = false;
    }

    allocator_set(prev_allocs);
    return state;
}

#include "lib/log_list.h"
EXPORT bool image_read_from_file(Image* image, String path, isize desired_channels, Pixel_Type format, i32 flags)
{
    LOG_INFO("ASSET", "Loading image '%s'", cstring_ephemeral(path));

    bool state = true;
    Arena arena = scratch_arena_acquire();
    {
        Log_List list = {0};
        log_list_init_capture(&list, &arena.allocator);
            String_Builder file_content = {&arena.allocator};
            state = state && file_read_entire(path, &file_content);
            state = state && image_read_from_memory(image, file_content.string, desired_channels, format, flags);
        log_capture_end(&list);
        
        if(state == false)
            LOG_ERROR_CHILD("ASSET", "load error", list.first, "Failed to load an image: '%s'" , cstring_ephemeral(path));
    }
    arena_release(&arena);
    return state;
}

INTERNAL void _stbi_write_to_memory(void* context, void* data, int size)
{
    String_Builder* append_into = (String_Builder*) context;
    builder_append(append_into, string_make(data, size));
}

EXPORT bool image_write_to_memory(Subimage image, String_Builder* into, Image_File_Format file_format)
{
    const char* error_msg_unspecfied =          "Unspecified error while formatting into memory (Zero sized image?)";
    const char* error_msg_bad_type =            "Output format does not support the representation format data type";
    const char* error_msg_bad_chanel_count =    "Output format does not support the representation format number of channels";
    const char* error_msg_bad_file_format =     "Invalid output format (IMAGE_LOAD_FILE_FORMAT_NONE or bad value)";

    int jpg_compression_quality = 50; //[0, 100]
    bool state = false;

    builder_clear(into);
    Arena arena = scratch_arena_acquire();
    {
        Image contiguous = {0};
        //not contigous in memory => make contiguous copy
        if(subimage_is_contiguous(image) == false)
        {
            contiguous = image_from_subimage(image, &arena.allocator);
            image = subimage_make(contiguous);
        }

        isize channel_count = subimage_channel_count(image);
        bool had_internal_error = false;
        switch(file_format)
        {
            case IMAGE_LOAD_FILE_FORMAT_PNG: {
                isize stride = subimage_byte_stride(image);;
                if(image.type != PIXEL_TYPE_U8)
                    LOG_ERROR("Asset", "%s", error_msg_bad_type);
                else
                {
                    state = true;
                    had_internal_error = !stbi_write_png_to_func(_stbi_write_to_memory, into, (int) image.width, (int) image.height, (int) channel_count, image.pixels, (int) stride);
                }
            } break;
            
            case IMAGE_LOAD_FILE_FORMAT_BMP: {
                if(image.type != PIXEL_TYPE_U8)
                    LOG_ERROR("Asset", "%s", error_msg_bad_type);
                else
                {
                    state = true;
                    had_internal_error = !stbi_write_bmp_to_func(_stbi_write_to_memory, into, (int) image.width, (int) image.height, (int) channel_count, image.pixels);
                }
            } break;
            
            case IMAGE_LOAD_FILE_FORMAT_TGA: {
                if(image.type != PIXEL_TYPE_U8)
                    LOG_ERROR("Asset", "%s", error_msg_bad_type);
                else
                {
                    state = true;
                    had_internal_error = !stbi_write_tga_to_func(_stbi_write_to_memory, into, (int) image.width, (int) image.height, (int) channel_count, image.pixels);
                }
            } break;
            
            case IMAGE_LOAD_FILE_FORMAT_JPG: {
                if(image.type != PIXEL_TYPE_U8)
                    LOG_ERROR("Asset", "%s", error_msg_bad_type);
                else if(channel_count > 3)
                    LOG_ERROR("Asset", "%s", error_msg_bad_chanel_count);
                else
                {
                    state = true;
                    had_internal_error = !stbi_write_jpg_to_func(_stbi_write_to_memory, into, (int) image.width, (int) image.height, (int) channel_count, image.pixels, jpg_compression_quality);
                }
            } break;
            
            case IMAGE_LOAD_FILE_FORMAT_HDR: {
                if(image.type != PIXEL_TYPE_F32)
                    LOG_ERROR("Asset", "%s", error_msg_bad_type);
                else if(channel_count > 3)
                    LOG_ERROR("Asset", "%s", error_msg_bad_chanel_count);
                else
                {
                    state = true;
                    had_internal_error = !stbi_write_hdr_to_func(_stbi_write_to_memory, into, (int) image.width, (int) image.height, (int) channel_count, (float*) (void*) image.pixels);
                }
            } break;

            case IMAGE_LOAD_FILE_FORMAT_NONE:
            default: 
                LOG_ERROR("Asset", "%s", error_msg_bad_file_format);
            break;
        }

        state = state && had_internal_error == false;
        if(had_internal_error)
            LOG_ERROR("Asset", "%s", error_msg_unspecfied);
    }
    arena_release(&arena);
    return state;
}

EXPORT bool image_write_to_file_formatted(Subimage image, String path, Image_File_Format file_format)
{
    Arena arena = scratch_arena_acquire();
    String_Builder formatted = {&arena.allocator};

    Log_List list = {0};
    log_list_init_capture(&list, &arena.allocator);
        bool state = image_write_to_memory(image, &formatted, file_format);
        state = state && file_write_entire(path, formatted.string);
    log_capture_end(&list);

    if(state == false)
        LOG_ERROR_CHILD("ASSET", "write error", list.first, "Couldnt write an image to path '%s'", cstring_ephemeral(path));

    arena_release(&arena);
    return state;
}

EXPORT bool image_write_to_file(Subimage image, String path)
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
        return IMAGE_LOAD_FILE_FORMAT_PNG;
    else if(string_is_equal(ext, STRING("bmp")) || string_is_equal(ext, STRING("BMP")))
        return IMAGE_LOAD_FILE_FORMAT_BMP;
    else if(string_is_equal(ext, STRING("tga")) || string_is_equal(ext, STRING("TGA")))
        return IMAGE_LOAD_FILE_FORMAT_TGA;
    else if(string_is_equal(ext, STRING("jpg")) || string_is_equal(ext, STRING("JPG")))
        return IMAGE_LOAD_FILE_FORMAT_JPG;
    else if(string_is_equal(ext, STRING("hdr")) || string_is_equal(ext, STRING("HDR")))
        return IMAGE_LOAD_FILE_FORMAT_HDR;
    else
        return IMAGE_LOAD_FILE_FORMAT_NONE;
}

#endif