#ifndef LIB_IMAGE_LOADER
#define LIB_IMAGE_LOADER

#include "image.h"
#include "string.h"
#include "file.h"

#include "allocator_wrapper.h"

typedef enum Image_File_Format {
    IMAGE_LOAD_FILE_FORMAT_NONE = 0,
    IMAGE_LOAD_FILE_FORMAT_PNG,
    IMAGE_LOAD_FILE_FORMAT_BMP,
    IMAGE_LOAD_FILE_FORMAT_TGA,
    IMAGE_LOAD_FILE_FORMAT_JPG,
    IMAGE_LOAD_FILE_FORMAT_HDR,
    IMAGE_LOAD_FILE_FORMAT_PPM,
    IMAGE_LOAD_FILE_FORMAT_PAM,
    IMAGE_LOAD_FILE_FORMAT_PFM,
} Image_File_Format;


enum {
    IMAGE_LOAD_FLAG_FLIP_Y = 1,
    IMAGE_LOAD_FLAG_FLIP_X = 2,
};

EXPORT Image_File_Format image_file_format_from_extension(String extension_without_dot);

EXPORT Error image_read_from_memory(Image_Builder* image, String data, isize desired_channels, Image_Pixel_Format format, i32 flags);
EXPORT Error image_read_from_file(Image_Builder* image, String path, isize desired_channels, Image_Pixel_Format format, i32 flags);

EXPORT Error image_write_to_memory(Image image, String_Builder* into, Image_File_Format format);
EXPORT Error image_write_to_file_formatted(Image image, String path, Image_File_Format format);
EXPORT Error image_write_to_file(Image image, String path);

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

#include "format_netbpm.h"
#include "extrenal/include/stb/stb_image.h"
#include "extrenal/include/stb/stb_image_write.h"

#include "log.h"

INTERNAL String _image_loader_translate_error(u32 code, void* context)
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

INTERNAL Error _image_loader_to_error(const char* error_string)
{
    static u32 error_module = 0;
    static Hash_Index error_hash = {0};
    if(error_module == 0)
    {
        hash_index_init(&error_hash, allocator_get_static());
        error_module = error_system_register_module(_image_loader_translate_error, STRING("image_loader.h"), &error_hash);
    }

    //We assume no hash collisions.
    u32 hashed = hash64_to32((u64) error_string);
    isize found_code = hash_index_find_or_insert(&error_hash, hashed, 0);

    if(error_hash.entries[found_code].value != 0)
        ASSERT(strcmp((const char*) error_hash.entries[found_code].value, error_string) == 0);

    error_hash.entries[found_code].value = (u64) error_string;
    return error_make(error_module, hashed);
}


EXPORT Error image_read_from_memory(Image_Builder* image, String data, isize desired_channels, Image_Pixel_Format format, i32 flags)
{

    Error error = {0};
    
    Netbpm_Format netbpm_format = netbpm_format_classify(data);
    if(netbpm_format != NETBPM_FORMAT_NONE)
    {
        switch(netbpm_format)
        {
            case NETBPM_FORMAT_PPM: error = netbpm_format_ppm_read_into(image, data); break;
            case NETBPM_FORMAT_PGM: error = netbpm_format_pgm_read_into(image, data); break;
            case NETBPM_FORMAT_PFM: error = netbpm_format_pfm_read_into(image, data); break;
            case NETBPM_FORMAT_PFMG: error = netbpm_format_pfmg_read_into(image, data); break;
            case NETBPM_FORMAT_PAM: error = netbpm_format_pam_read_into(image, data); break;

            case NETBPM_FORMAT_NONE:
            case NETBPM_FORMAT_PBM_ASCII:
            case NETBPM_FORMAT_PGM_ASCII:
            case NETBPM_FORMAT_PPM_ASCII:
            case NETBPM_FORMAT_PBM:
            default: error = _image_loader_to_error("unsupported netbpm format"); break;
        }
    }
    else
    {
        int width = 0;
        int height = 0;
        int channels = 0;
        void* allocated = NULL;
        stbi_set_flip_vertically_on_load((flags & IMAGE_LOAD_FLAG_FLIP_Y) > 0);

        switch(format)
        {
            case PIXEL_FORMAT_U8:
                allocated = stbi_load_from_memory((const u8*) data.data, (int) data.size, &width, &height, &channels, (int) desired_channels);
                break;

            case PIXEL_FORMAT_U16:
                allocated = stbi_load_16_from_memory((const u8*) data.data, (int) data.size, &width, &height, &channels, (int) desired_channels);
                break;

            case PIXEL_FORMAT_F32:
                allocated = stbi_loadf_from_memory((const u8*) data.data, (int) data.size, &width, &height, &channels, (int) desired_channels);
                break;

            case PIXEL_FORMAT_U24:
            case PIXEL_FORMAT_U32:
            default: 
                ASSERT(false);
                break;
        }
        
        if(allocated)
        {
            image_builder_init(image, wrapper_allocator_get_default(), channels, format);
            image->pixels = (u8*) allocated;
            image->width = (i32) width;
            image->height = (i32) height;
        }
        else
        {
            error = _image_loader_to_error(stbi_failure_reason());
        }
    }


    return error;
}

EXPORT Error image_read_from_file(Image_Builder* image, String path, isize desired_channels, Image_Pixel_Format format, i32 flags)
{
    String_Builder file_content = {allocator_get_scratch()};
    Error file_error = file_read_entire(path, &file_content);
    Error parse_error = ERROR_OR(file_error) image_read_from_memory(image, string_from_builder(file_content), desired_channels, format, flags);
    
    if(!error_is_ok(parse_error))
    {
        LOG_ERROR("ASSET", "Failed to load an image: \"" STRING_FMT "\"\n"
            "error: " STRING_FMT, STRING_PRINT(path), STRING_PRINT(error_code(parse_error)));
    }

    array_deinit(&file_content);
    return parse_error;
}

EXPORT INTERNAL void _stbi_write_to_memory(void* context, void* data, int size)
{
    String_Builder* append_into = (String_Builder*) context;
    array_append(append_into, (char*) data, size);
}

EXPORT Error image_write_to_memory(Image image, String_Builder* into, Image_File_Format file_format)
{
    const char* error_msg_unspecfied =          "Unspecified error while formatting into memory (Zero sized image?)";
    const char* error_msg_bad_type =            "Output format does not support the representation format data type";
    const char* error_msg_bad_chanel_count =    "Output format does not support the representation format number of channels";
    const char* error_msg_bad_file_format =     "Invalid output format (IMAGE_LOAD_FILE_FORMAT_NONE or bad value)";

    int jpg_compression_quality = 50; //[0, 100]

    array_clear(into);

    Image_Builder contiguous = {0};

    //not contigous in memory => make contiguous copy
    if(image_is_contiguous(image) == false)
    {
        image_builder_init_from_image(&contiguous, allocator_get_scratch(), image);
        image = image_from_builder(contiguous);
    }

    Error out_error = {0};
    isize channel_count = image_channel_count(image);
    bool had_internal_error = false;
    switch(file_format)
    {
        case IMAGE_LOAD_FILE_FORMAT_PNG: {
            isize stride = image_byte_stride(image);;
            if(image.pixel_format != PIXEL_FORMAT_U8)
                out_error = _image_loader_to_error(error_msg_bad_type);
            else
                had_internal_error = !stbi_write_png_to_func(_stbi_write_to_memory, into, (int) image.width, (int) image.height, (int) channel_count, image.pixels, (int) stride);
        } break;
        
        case IMAGE_LOAD_FILE_FORMAT_BMP: {
            if(image.pixel_format != PIXEL_FORMAT_U8)
                out_error = _image_loader_to_error(error_msg_bad_type);
            else
                had_internal_error = !stbi_write_bmp_to_func(_stbi_write_to_memory, into, (int) image.width, (int) image.height, (int) channel_count, image.pixels);
        } break;
        
        case IMAGE_LOAD_FILE_FORMAT_TGA: {
            if(image.pixel_format != PIXEL_FORMAT_U8)
                out_error = _image_loader_to_error(error_msg_bad_type);
            else
                had_internal_error = !stbi_write_tga_to_func(_stbi_write_to_memory, into, (int) image.width, (int) image.height, (int) channel_count, image.pixels);
        } break;
        
        case IMAGE_LOAD_FILE_FORMAT_JPG: {
            if(image.pixel_format != PIXEL_FORMAT_U8)
                out_error = _image_loader_to_error(error_msg_bad_type);
            else if(image_channel_count(image) > 3)
                out_error = _image_loader_to_error(error_msg_bad_chanel_count);
            else
                had_internal_error = !stbi_write_jpg_to_func(_stbi_write_to_memory, into, (int) image.width, (int) image.height, (int) channel_count, image.pixels, jpg_compression_quality);
        } break;
        
        case IMAGE_LOAD_FILE_FORMAT_HDR: {
            if(image.pixel_format != PIXEL_FORMAT_F32)
                out_error = _image_loader_to_error(error_msg_bad_type);
            else if(image_channel_count(image) > 3)
                out_error = _image_loader_to_error(error_msg_bad_chanel_count);
            else
                had_internal_error = !stbi_write_hdr_to_func(_stbi_write_to_memory, into, (int) image.width, (int) image.height, (int) channel_count, (float*) (void*) image.pixels);
        } break;

        case IMAGE_LOAD_FILE_FORMAT_PFM: {
            if(image.pixel_format != PIXEL_FORMAT_F32)
                out_error = _image_loader_to_error(error_msg_bad_type);
            else if(image_channel_count(image) > 3)
                out_error = _image_loader_to_error(error_msg_bad_chanel_count);
            else
                out_error = netbpm_format_pfm_write_into(into, image, 1.0f);
        } break;

        case IMAGE_LOAD_FILE_FORMAT_PPM: {
            if(image.pixel_format != PIXEL_FORMAT_U8)
                out_error = _image_loader_to_error(error_msg_bad_type);
            else if(image_channel_count(image) > 3)
                out_error = _image_loader_to_error(error_msg_bad_chanel_count);
            else
                out_error = netbpm_format_ppm_write_into(into, image);
        } break;
            
        case IMAGE_LOAD_FILE_FORMAT_PAM: {
            out_error = netbpm_format_pam_write_into(into, image);
        } break;

        case IMAGE_LOAD_FILE_FORMAT_NONE:
        default: 
            out_error = _image_loader_to_error(error_msg_bad_file_format);
        break;
    }

    if(had_internal_error)
        out_error = ERROR_OR(out_error) _image_loader_to_error(error_msg_unspecfied);

    image_builder_deinit(&contiguous);
    return out_error;
}

EXPORT Error image_write_to_file_formatted(Image image, String path, Image_File_Format file_format)
{
    String_Builder formatted = {allocator_get_scratch()};
    Error format_error = image_write_to_memory(image, &formatted, file_format);
    Error output_error = ERROR_OR(format_error) file_write_entire(path, string_from_builder(formatted));

    array_deinit(&formatted);

    return output_error;
}

EXPORT Error image_write_to_file(Image image, String path)
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
    else if(string_is_equal(ext, STRING("ppm")) || string_is_equal(ext, STRING("PPM")))
        return IMAGE_LOAD_FILE_FORMAT_PPM;
    else if(string_is_equal(ext, STRING("pfm")) || string_is_equal(ext, STRING("PFM")))
        return IMAGE_LOAD_FILE_FORMAT_PFM;
    else if(string_is_equal(ext, STRING("pam")) || string_is_equal(ext, STRING("PAM")))
        return IMAGE_LOAD_FILE_FORMAT_PAM;
    else
        return IMAGE_LOAD_FILE_FORMAT_NONE;
}

#endif