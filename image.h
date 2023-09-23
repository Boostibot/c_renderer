#ifndef LIB_IMAGE
#define LIB_IMAGE

#include "string.h"

typedef enum Image_Pixel_Format {
    PIXEL_FORMAT_U8 = -1,
    PIXEL_FORMAT_U16 = -2,
    PIXEL_FORMAT_F32 = -3,
} Image_Pixel_Format;

typedef struct Image
{
    String_Builder path;
    u8* pixels; //uses allocator from path string
    u16 pixel_size;
    Image_Pixel_Format format;

    i32 width;
    i32 height;
} Image;

typedef struct Image_View {
    u8* pixels;
    u16 pixel_size;
    Image_Pixel_Format format;

    i32 containing_width;
    i32 containing_height;

    i32 from_x;
    i32 from_y;
    
    i32 width;
    i32 height;
} Image_View;

EXPORT isize image_pixel_format_size(Image_Pixel_Format format);

EXPORT void image_init(Image* image, Allocator* alloc);
EXPORT void image_deinit(Image* image);
EXPORT void image_resize(Image* image, i32 width, i32 height);
EXPORT Allocator* image_get_allocator(const Image* image);
EXPORT isize image_channel_count(const Image* image);
EXPORT isize image_pixel_count(const Image* image);
EXPORT isize image_byte_stride(const Image* image);
EXPORT isize image_all_pixels_size(const Image* image);

EXPORT Image_View image_view_from_image(const Image* image);
EXPORT void* image_view_at(Image_View image, i32 x, i32 y);
EXPORT isize image_view_channel_count(Image_View image);
EXPORT isize image_view_pixel_count(Image_View image);
EXPORT isize image_view_byte_stride(Image_View image);

EXPORT Image_View image_view_portion(Image_View view, i32 from_x, i32 from_y, i32 width, i32 height);
EXPORT Image_View image_view_range(Image_View view, i32 from_x, i32 from_y, i32 to_x, i32 to_y);
EXPORT void image_view_copy(Image_View* to_image, Image_View image, i32 offset_x, i32 offset_y);

#endif

#if (defined(LIB_ALL_IMPL) || defined(LIB_IMAGE_IMPL)) && !defined(LIB_IMAGE_HAS_IMPL)
#define LIB_IMAGE_HAS_IMPL

EXPORT isize image_pixel_format_size(Image_Pixel_Format format)
{
    switch(format)
    {
        default:                return 0;
        case PIXEL_FORMAT_U8:   return 1;
        case PIXEL_FORMAT_U16:  return 2;
        case PIXEL_FORMAT_F32:  return 4;
    }
}

EXPORT isize image_channel_count(const Image* image)
{
    isize format_size = MAX(image_pixel_format_size(image->format), 1);
    isize out = image->pixel_size / format_size;
    return out;
}

EXPORT isize image_pixel_count(const Image* image)
{
    return image->width * image->height;
}

EXPORT isize image_byte_stride(const Image* image)
{
    isize byte_stride = image->pixel_size * image->width;
    return byte_stride;
}

EXPORT isize image_all_pixels_size(const Image* image)
{
    isize pixel_count = image_pixel_count(image);
    return image->pixel_size * pixel_count;
}

EXPORT Allocator* image_get_allocator(const Image* image)
{
    return array_get_allocator(image->path);
}

EXPORT void image_deinit(Image* image)
{
    Allocator* used_allocator = image_get_allocator(image);

    isize total_size = image_all_pixels_size(image);
    allocator_deallocate(used_allocator, image->pixels, total_size, DEF_ALIGN, SOURCE_INFO());

    array_deinit(&image->path);
    memset(image, 0, sizeof *image);
}

EXPORT void image_init(Image* image, Allocator* alloc)
{
    image_deinit(image);
    array_init(&image->path, alloc);
}

EXPORT isize image_view_channel_count(Image_View image)
{
    isize format_size = MAX(image_pixel_format_size(image.format), 1);
    isize out = image.pixel_size / format_size;
    return out;
}

EXPORT isize image_view_byte_stride(Image_View image)
{
    isize stride = image.containing_width * image.pixel_size;

    return stride;
}

EXPORT isize image_view_pixel_count(Image_View image)
{
    return image.width * image.height;
}

EXPORT Image_View image_view_from_image(const Image* image)
{
    Image_View view = {0};
    view.pixels = image->pixels;
    view.pixel_size = image->pixel_size;
    view.format = image->format;

    view.containing_width = image->width;
    view.containing_height = image->height;

    view.from_x = 0;
    view.from_y = 0;
    view.width = image->width;
    view.height = image->height;

    return view;
}

EXPORT Image_View image_view_range(Image_View view, i32 from_x, i32 from_y, i32 to_x, i32 to_y)
{
    Image_View out = view;
    CHECK_BOUNDS(from_x, out.width + 1);
    CHECK_BOUNDS(from_y, out.height + 1);
    CHECK_BOUNDS(to_x, out.width + 1);
    CHECK_BOUNDS(to_y, out.height + 1);

    ASSERT(from_x <= to_x);
    ASSERT(from_y <= to_y);

    out.from_x = from_x;
    out.from_y = from_y;
    out.width = to_x - from_x;
    out.height = to_y - from_y;

    return out;
}

EXPORT Image_View image_view_portion(Image_View view, i32 from_x, i32 from_y, i32 width, i32 height)
{
    Image_View out = image_view_range(view, from_x, from_y, from_x + width, from_y + height);
    return out;
}

EXPORT void* image_view_at(Image_View view, i32 x, i32 y)
{
    CHECK_BOUNDS(x, view.width);
    CHECK_BOUNDS(y, view.height);

    i32 containing_x = x + view.from_x;
    i32 containing_y = y + view.from_y;
    
    isize byte_stride = image_view_byte_stride(view);

    u8* data = (u8*) view.pixels;
    isize offset = containing_x*view.pixel_size + containing_y*byte_stride;
    u8* pixel = data + offset;

    return pixel;
}

EXPORT void image_view_copy(Image_View* to_image, Image_View from_image, i32 offset_x, i32 offset_y)
{
    //Simple implementation
    i32 copy_width = from_image.width;
    i32 copy_height = from_image.height;
    if(copy_width == 0 || copy_height == 0)
        return;

    Image_View to_portion = image_view_portion(*to_image, offset_x, offset_y, copy_width, copy_height);
    ASSERT_MSG(from_image.format == to_image->format, "formats must match!");
    ASSERT_MSG(from_image.pixel_size == to_image->pixel_size, "formats must match!");

    isize to_image_stride = image_view_byte_stride(*to_image); 
    isize from_image_stride = image_view_byte_stride(from_image); 
    isize row_byte_size = copy_width * from_image.pixel_size;

    u8* to_image_ptr = (u8*) image_view_at(to_portion, 0, 0);
    u8* from_image_ptr = (u8*) image_view_at(from_image, 0, 0);

    //Copy in the right order so we dont override any data
    if(from_image_ptr >= to_image_ptr)
    {
        for(isize y = 0; y < copy_height; y++)
        { 
            memmove(to_image_ptr, from_image_ptr, row_byte_size);

            to_image_ptr += to_image_stride;
            from_image_ptr += from_image_stride;
        }
    }
    else
    {
        //Reverse order copy
        to_image_ptr += to_image_stride + copy_height*to_image_stride;
        from_image_ptr += from_image_stride + copy_height*from_image_stride;

        for(isize y = 0; y < copy_height; y++)
        { 
            to_image_ptr -= to_image_stride;
            from_image_ptr -= from_image_stride;

            memmove(to_image_ptr, from_image_ptr, row_byte_size);
        }
    }
}

EXPORT void image_resize(Image* image, i32 width, i32 height)
{
    ASSERT(image != NULL);
    ASSERT(width >= 0 && height >= 0);
    Allocator* alloc = image_get_allocator(image);

    isize old_byte_size = image_all_pixels_size(image);
    isize new_byte_size = (isize) width * height * image->pixel_size;

    Image new_image = {0};
    
    if(alloc == NULL)
    {
        alloc = allocator_get_default();
        array_init(&new_image.path, alloc);
    }

    new_image.path = image->path;
    new_image.width = width;
    new_image.height = height;
    new_image.format = image->format;
    new_image.pixel_size = image->pixel_size;
    new_image.pixels = (u8*) allocator_allocate(alloc, new_byte_size, DEF_ALIGN, SOURCE_INFO());
    memset(new_image.pixels, 0, new_byte_size);

    Image_View to_view = image_view_from_image(&new_image);
    Image_View from_view = image_view_from_image(image);

    image_view_copy(&to_view, from_view, 0, 0);

    allocator_deallocate(alloc, image->pixels, old_byte_size, DEF_ALIGN, SOURCE_INFO());

    *image = new_image;
}

#endif