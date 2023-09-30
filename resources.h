#ifndef LIB_RESOURCES
#define LIB_RESOURCES

#include "shapes.h"
#include "string.h"
#include "image.h"
#include "handle_table.h"

//@TODO: cubemap info!

#define MAX_CHANNELS 4
typedef struct Map_Info {
    Vec2 offset;                
    Vec2 scale;
    Vec2 resolution;

    i32 channels_count; //the number of channels this texture should have. Is in range [0, MAX_CHANNELS] 
    i32 channels_idices1[MAX_CHANNELS]; //One based indeces into the image channels. 
    //If value is 0 then it is assumed to be equal to its index ie.:
    // channels_idices1 = {0, 0, 0, 0} ~~assumed~~> channels_idices1 = {1, 2, 3, 4}

    //linear_color = (contrast_minus_one + 1) * color^(1/gamma) + brigthness;
    //output_color = linear_color^screen_gamma
    f32 gamma;                //default 1
    f32 brigthness;           //default 0
    f32 contrast_minus_one;   //default 0

    bool is_clamped; //wheter or not does this texture not tile. Defaults to tile ie. false;
    bool is_enabled; //whether or not to use this texture. Maybe [0, 1] float?
} Map_Info;

// ========================= Descriptions =========================
// These should perfectly describe the thing they are substituting but have
// a path to the data instead of the data itself.
//
// At the moment descriptions are not part of Resources. This will likely change in the future
// once we have a proper concept of asset.

typedef struct Map_Description {
    String_Builder path;
    Map_Info info;
} Map_Description;

typedef union Cubemap_Description {
    struct {
        Map_Description top;   
        Map_Description bottom;
        Map_Description front; 
        Map_Description back;  
        Map_Description left;  
        Map_Description right; 
    };
    Map_Description faces[6];
} Cubemap_Description;

typedef struct Material_Phong_Description {
    String_Builder name;
    String_Builder path;
    
    Vec3 ambient_color;                     
    Vec3 diffuse_color;                     
    Vec3 specular_color;                    
    f32 specular_exponent;                  
    f32 opacity;      
    
    f32 bump_multiplier_minus_one;                    

    Map_Description map_ambient;           
    Map_Description map_diffuse;   
    Map_Description map_specular_color;    
    Map_Description map_specular_highlight;
    Map_Description map_alpha;             
    Map_Description map_bump;              
    Map_Description map_displacement;      
    Map_Description map_stencil;           
    Map_Description map_normal;           
    Map_Description map_reflection_sphere;
    Cubemap_Description map_reflection_cube;
} Material_Phong_Description;

typedef struct Material_Description {
    String_Builder name;
    String_Builder path;

    Vec3 albedo;
    Vec3 emissive;
    f32 roughness;
    f32 metallic;
    f32 ambient_occlusion;
    f32 opacity;
    f32 emissive_power;

    f32 emissive_power_map;
    f32 bump_multiplier_minus_one;
    
    Map_Description map_albedo;  
    Map_Description map_roughness;           
    Map_Description map_ambient_occlusion;           
    Map_Description map_metallic;           
    Map_Description map_emmisive;
    
    Map_Description map_alpha;  
    Map_Description map_bump;              
    Map_Description map_displacement;      
    Map_Description map_stencil;  
    Map_Description map_normal;
    
    Map_Description map_reflection_sphere;   
    Cubemap_Description map_reflection_cube;
} Material_Description;

typedef struct Object_Group_Description {
    String_Builder name;
    String_Builder material_name;
    String_Builder material_path;

    i32 next_i1;
    i32 child_i1;
    i32 depth;

    i32 triangles_from;
    i32 triangles_to;
} Object_Group_Description;

DEFINE_ARRAY_TYPE(Material_Description, Material_Description_Array);
DEFINE_ARRAY_TYPE(Material_Phong_Description, Material_Phong_Description_Array);
DEFINE_ARRAY_TYPE(Object_Group_Description, Object_Group_Description_Array);

typedef struct Object_Description {
    String_Builder name;
    String_Builder path;
    Object_Group_Description_Array groups;
    String_Builder_Array material_files;
} Object_Description;

void map_info_init(Map_Info* description, Allocator* alloc);
void map_info_deinit(Map_Info* description);

void map_description_init(Map_Description* description, Allocator* alloc);
void map_description_deinit(Map_Description* description);

void cubemap_description_init(Cubemap_Description* description, Allocator* alloc);
void cubemap_description_deinit(Cubemap_Description* description);

void material_phong_description_init(Material_Phong_Description* description, Allocator* alloc);
void material_phong_description_deinit(Material_Phong_Description* description);

void material_description_init(Material_Description* description, Allocator* alloc);
void material_description_deinit(Material_Description* description);

void object_group_description_init(Object_Group_Description* description, Allocator* alloc);
void object_group_description_deinit(Object_Group_Description* description);

void object_description_init(Object_Description* description, Allocator* alloc);
void object_description_deinit(Object_Description* description);

//========================= Engine types =========================
// These represent things the engine recognizes. 
// It uses handles to more complex things instead of the things iteself to enable sharing.



typedef struct Loaded_Image {
    Image_Builder image;
    String_Builder path;
    String_Builder name;
} Loaded_Image;

// Handle means strong owning reference
// Ref    means weak non-owning reference

#define DEFINE_HANDLE_TYPES(Type)                           \
    typedef struct { Handle h; } Type##_Handle;             \
    typedef struct { Handle h; } Type##_Ref;                \
    DEFINE_ARRAY_TYPE(Type##_Handle, Type##_Handle_Array);  \
    DEFINE_ARRAY_TYPE(Type##_Ref, Type##_Ref_Array)         \

DEFINE_HANDLE_TYPES(Loaded_Image);
DEFINE_HANDLE_TYPES(Map);
DEFINE_HANDLE_TYPES(Cubemap);
DEFINE_HANDLE_TYPES(Shape_Assembly);
DEFINE_HANDLE_TYPES(Material);
DEFINE_HANDLE_TYPES(Object);

typedef struct Resources
{
    Allocator* alloc;

    Handle_Table images;
    Handle_Table shapes;
    Handle_Table materials;
    Handle_Table materials_phong;
    Handle_Table objects;
} Resources;

typedef struct Map {
    String_Builder path;
    String_Builder name;
    Loaded_Image_Handle image;
    Map_Info info;
} Map;

typedef union Cubemap {
    struct {
        Map top;   
        Map bottom;
        Map front; 
        Map back;  
        Map left;  
        Map right;
    };

    Map faces[6];
} Cubemap;

typedef struct Material {
    String_Builder name;
    String_Builder path;

    Vec3 albedo;
    Vec3 emissive;
    f32 roughness;
    f32 metallic;
    f32 ambient_occlusion;
    f32 opacity;
    f32 emissive_power;

    f32 emissive_power_map;
    f32 bump_multiplier_minus_one;

    Map map_albedo;  
    Map map_roughness;           
    Map map_ambient_occlusion;           
    Map map_metallic;           
    Map map_emmisive;
    
    Map map_alpha;  
    Map map_bump;              
    Map map_displacement;      
    Map map_stencil;  
    Map map_normal;
    
    Map map_reflection_sphere;   
    Cubemap map_reflection_cube;
} Material;

DEFINE_ARRAY_TYPE(Material, Material_Array);

typedef struct Object_Leaf_Group
{
    String_Builder name;
    Material_Handle material;

    i32 triangles_from;
    i32 triangles_to;
} Object_Leaf_Group;

typedef struct Object_Group
{
    String_Builder name;
    Material_Handle material;

    i32 next_i1;
    i32 child_i1;
    i32 depth;

    i32 triangles_from;
    i32 triangles_to;
} Object_Group;

DEFINE_ARRAY_TYPE(Object_Group, Object_Group_Array);

typedef struct Object
{
    String_Builder name;
    String_Builder path;

    Material_Handle fallback_material;
    Shape_Assembly_Handle shape;

    Object_Group_Array groups;
} Object;
    
#define DEFINE_RESOURCES_RESOURCE_DECL(Type, name, member_name)                         \
    Type##_Handle resources_##name##_add(Resources* resources, Type* item);             \
    void resources_##name##_remove(Resources* resources, Type##_Handle handle);         \
    Type##_Handle resources_##name##_share(Resources* resources, Type##_Handle handle); \
    Type* resources_##name##_get(Resources* resources, Type##_Handle handle);           \
    Type* resources_##name##_get_ref(Resources* resources, Type##_Ref handle);          \
    
    
DEFINE_RESOURCES_RESOURCE_DECL(Loaded_Image, loaded_image, images)
DEFINE_RESOURCES_RESOURCE_DECL(Shape_Assembly, shape, shapes)
DEFINE_RESOURCES_RESOURCE_DECL(Material, material, materials)
DEFINE_RESOURCES_RESOURCE_DECL(Material, material_phong, materials_phong)
DEFINE_RESOURCES_RESOURCE_DECL(Object, object, objects)

void loaded_image_init(Loaded_Image* loaded_image, Allocator* alloc);
void loaded_image_deinit(Loaded_Image* loaded_image);

void resources_init(Resources* resources, Allocator* alloc);
void resources_deinit(Resources* resources);

void map_init(Map* item, Resources* resources);
void map_deinit(Map* item, Resources* resources);

void cubemap_init(Cubemap* item, Resources* resources);
void cubemap_deinit(Cubemap* item, Resources* resources);

void material_init(Material* item, Resources* resources);
void material_deinit(Material* item, Resources* resources);

void object_group_init(Object_Group* item, Resources* resources);
void object_group_deinit(Object_Group* item, Resources* resources);

void object_init(Object* item, Resources* resources);
void object_deinit(Object* item, Resources* resources);

#endif


#if (defined(LIB_ALL_IMPL) || defined(LIB_RESOURCES_IMPL)) && !defined(LIB_RESOURCES_HAS_IMPL)
#define LIB_RESOURCES_HAS_IMPL

void map_info_init(Map_Info* description, Allocator* alloc)
{
    //Nothing so far but we might add dynamic behaviour later
    (void) description;
    (void) alloc;
}
void map_info_deinit(Map_Info* description)
{
    //Nothing so far but we might add dynamic behaviour later
    (void) description;
}

void map_description_init(Map_Description* description, Allocator* alloc)
{
    map_description_deinit(description);
    map_info_init(&description->info, alloc);
    array_init(&description->path, alloc);
}
void map_description_deinit(Map_Description* description)
{
    array_deinit(&description->path);
    map_info_deinit(&description->info);
    memset(description, 0, sizeof *description);
}

void cubemap_description_init(Cubemap_Description* description, Allocator* alloc)
{
    for(isize i = 0; i < 6; i++)
        map_description_init(&description->faces[i], alloc);
}
void cubemap_description_deinit(Cubemap_Description* description)
{
    for(isize i = 0; i < 6; i++)
        map_description_deinit(&description->faces[i]);
}

void material_phong_description_init(Material_Phong_Description* description, Allocator* alloc)
{
    material_phong_description_deinit(description);
    
    array_init(&description->path, alloc);
    array_init(&description->path, alloc);

    map_description_init(&description->map_ambient, alloc);           
    map_description_init(&description->map_diffuse, alloc);   
    map_description_init(&description->map_specular_color, alloc);    
    map_description_init(&description->map_specular_highlight, alloc);
    map_description_init(&description->map_alpha, alloc);             
    map_description_init(&description->map_bump, alloc);              
    map_description_init(&description->map_displacement, alloc);      
    map_description_init(&description->map_stencil, alloc);           
    map_description_init(&description->map_normal, alloc);           
    map_description_init(&description->map_reflection_sphere, alloc);
    cubemap_description_init(&description->map_reflection_cube, alloc);
    
    memset(description, 0, sizeof *description);
}
void material_phong_description_deinit(Material_Phong_Description* description)
{
    array_deinit(&description->path);
    array_deinit(&description->path);

    map_description_deinit(&description->map_ambient);           
    map_description_deinit(&description->map_diffuse);   
    map_description_deinit(&description->map_specular_color);    
    map_description_deinit(&description->map_specular_highlight);
    map_description_deinit(&description->map_alpha);             
    map_description_deinit(&description->map_bump);              
    map_description_deinit(&description->map_displacement);      
    map_description_deinit(&description->map_stencil);           
    map_description_deinit(&description->map_normal);           
    map_description_deinit(&description->map_reflection_sphere);
    cubemap_description_deinit(&description->map_reflection_cube);
    
    memset(description, 0, sizeof *description);
}

void material_description_init(Material_Description* description, Allocator* alloc)
{
    material_description_deinit(description);

    array_init(&description->path, alloc);
    array_init(&description->path, alloc);

    map_description_init(&description->map_alpha, alloc);  
    map_description_init(&description->map_bump, alloc);              
    map_description_init(&description->map_displacement, alloc);      
    map_description_init(&description->map_stencil, alloc);  
    map_description_init(&description->map_roughness, alloc);    
    map_description_init(&description->map_ambient_occlusion, alloc);    
    map_description_init(&description->map_metallic, alloc);        
    map_description_init(&description->map_albedo, alloc);        
    map_description_init(&description->map_emmisive, alloc);           
    map_description_init(&description->map_normal, alloc);             
    map_description_init(&description->map_reflection_sphere, alloc);   
    cubemap_description_init(&description->map_reflection_cube, alloc);

    memset(description, 0, sizeof *description);
}
void material_description_deinit(Material_Description* description)
{
    array_deinit(&description->path);
    array_deinit(&description->path);

    map_description_deinit(&description->map_alpha);  
    map_description_deinit(&description->map_bump);              
    map_description_deinit(&description->map_displacement);      
    map_description_deinit(&description->map_stencil);  
    map_description_deinit(&description->map_roughness);
    map_description_deinit(&description->map_ambient_occlusion);
    map_description_deinit(&description->map_metallic);           
    map_description_deinit(&description->map_albedo);           
    map_description_deinit(&description->map_emmisive);           
    map_description_deinit(&description->map_normal);             
    map_description_deinit(&description->map_reflection_sphere);   
    cubemap_description_deinit(&description->map_reflection_cube);

    memset(description, 0, sizeof *description);
}

void object_group_description_init(Object_Group_Description* description, Allocator* alloc)
{
    object_group_description_deinit(description);

    array_init(&description->name, alloc);
    array_init(&description->material_name, alloc);
    array_init(&description->material_path, alloc);
}
void object_group_description_deinit(Object_Group_Description* description)
{
    array_deinit(&description->name);
    array_deinit(&description->material_name);
    array_deinit(&description->material_path);

    memset(description, 0, sizeof *description);
}

void object_description_init(Object_Description* description, Allocator* alloc)
{
    object_description_deinit(description);

    array_init(&description->name, alloc);
    array_init(&description->path, alloc);
    array_init(&description->groups, alloc);
    array_init(&description->material_files, alloc);
}
void object_description_deinit(Object_Description* description)
{
    array_deinit(&description->name);
    array_deinit(&description->path);

    for(isize i = 0; i < description->groups.size; i++)
        object_group_description_deinit(&description->groups.data[i]);

    builder_array_deinit(&description->material_files);

    memset(description, 0, sizeof *description);
}


#define DEFINE_RESOURCES_RESOURCE(Type, name, member_name)                              \
    Type##_Handle resources_##name##_add(Resources* resources, Type* item)              \
    {                                                                                   \
        Handle h = handle_table_add(&resources->member_name);                           \
        Type* added_ptr = (Type*) handle_table_get(resources->member_name, h);          \
        SWAP(added_ptr, item, Type);                                                    \
                                                                                        \
        Type##_Handle out = {h};                                                        \
        return out;                                                                     \
    }                                                                                   \
                                                                                        \
    Type##_Handle resources_##name##_share(Resources* resources, Type##_Handle handle)  \
    {                                                                                   \
        Handle h = handle_table_share(&resources->member_name, handle.h);               \
        Type##_Handle out = {h};                                                        \
        return out;                                                                     \
    }                                                                                   \
                                                                                        \
    Type* resources_##name##_get(Resources* resources, Type##_Handle handle)            \
    {                                                                                   \
        Type* out = (Type*) handle_table_get(resources->member_name, handle.h);         \
        return out;                                                                     \
    }                                                                                   \
                                                                                        \
    Type* resources_##name##_get_ref(Resources* resources, Type##_Ref handle)           \
    {                                                                                   \
        return (Type*) handle_table_get(resources->member_name, handle.h);              \
    }                                                                                   \

DEFINE_RESOURCES_RESOURCE(Loaded_Image, loaded_image, images)
DEFINE_RESOURCES_RESOURCE(Shape_Assembly, shape, shapes)
DEFINE_RESOURCES_RESOURCE(Material, material, materials)
DEFINE_RESOURCES_RESOURCE(Material, material_phong, materials_phong)
DEFINE_RESOURCES_RESOURCE(Object, object, objects)

//Remove functions have to be done by hand since each type requires its own deinit.

void resources_loaded_image_remove(Resources* resources, Loaded_Image_Handle handle)      
{                                                                               
    Loaded_Image removed = {0};
    if(handle_table_remove(&resources->images, handle.h, &removed))
        loaded_image_deinit(&removed);
}    

void resources_shape_remove(Resources* resources, Shape_Assembly_Handle handle)      
{                                                                               
    Shape_Assembly removed = {0};
    if(handle_table_remove(&resources->shapes, handle.h, &removed))
        shape_assembly_deinit(&removed);
}  
void resources_material_remove(Resources* resources, Material_Handle handle)      
{                                                                               
    Material removed = {0};
    if(handle_table_remove(&resources->materials, handle.h, &removed))
        material_deinit(&removed, resources);
}  
void resources_material_phong_remove(Resources* resources, Material_Handle handle)      
{                                                                               
    Material removed = {0};
    if(handle_table_remove(&resources->materials_phong, handle.h, &removed))
        material_deinit(&removed, resources);
}  
void resources_object_remove(Resources* resources, Object_Handle handle)      
{                                                                               
    Object removed = {0};
    if(handle_table_remove(&resources->objects, handle.h, &removed))
        object_deinit(&removed, resources);
}  

void loaded_image_init(Loaded_Image* loaded_image, Allocator* alloc)
{
    image_builder_init(&loaded_image->image, alloc, 1, PIXEL_FORMAT_U8);
    array_init(&loaded_image->path, alloc);
    array_init(&loaded_image->name, alloc);
}

void loaded_image_deinit(Loaded_Image* loaded_image)
{
    image_builder_deinit(&loaded_image->image);
    array_deinit(&loaded_image->path);
    array_deinit(&loaded_image->name);
}

void resources_init(Resources* resources, Allocator* alloc)
{
    resources_deinit(resources);

    resources->alloc = alloc;

    handle_table_init(&resources->images, alloc,            sizeof(Loaded_Image), DEF_ALIGN);
    handle_table_init(&resources->shapes, alloc,            sizeof(Shape_Assembly), DEF_ALIGN);
    handle_table_init(&resources->materials, alloc,         sizeof(Material), DEF_ALIGN);
    handle_table_init(&resources->materials_phong, alloc,   sizeof(Material), DEF_ALIGN);
    handle_table_init(&resources->objects, alloc,           sizeof(Object), DEF_ALIGN);
}

void resources_deinit(Resources* resources)
{
    HANDLE_TABLE_FOR_EACH_BEGIN(resources->objects, h, Object*, object)
        object_deinit(object, resources);
    HANDLE_TABLE_FOR_EACH_END
    
    HANDLE_TABLE_FOR_EACH_BEGIN(resources->materials, h, Material*, material)
        material_deinit(material, resources);
    HANDLE_TABLE_FOR_EACH_END
    
    HANDLE_TABLE_FOR_EACH_BEGIN(resources->materials_phong, h, Material*, material)
        material_deinit(material, resources);
    HANDLE_TABLE_FOR_EACH_END

    HANDLE_TABLE_FOR_EACH_BEGIN(resources->images, h, Loaded_Image*, loaded_image)
        loaded_image_deinit(loaded_image);
    HANDLE_TABLE_FOR_EACH_END
    
    HANDLE_TABLE_FOR_EACH_BEGIN(resources->shapes, h, Shape_Assembly*, shape_assembly)
        shape_assembly_deinit(shape_assembly);
    HANDLE_TABLE_FOR_EACH_END

    handle_table_deinit(&resources->images);
    handle_table_deinit(&resources->shapes);
    handle_table_deinit(&resources->materials);
    handle_table_deinit(&resources->materials_phong);
    handle_table_deinit(&resources->objects);

    memset(resources, 0, sizeof *resources);
}

void map_init(Map* item, Resources* resources)
{
    map_deinit(item, resources);
    array_init(&item->path, resources->alloc);
    array_init(&item->name, resources->alloc);
    map_info_init(&item->info, resources->alloc);
}

void map_deinit(Map* item, Resources* resources)
{
    array_deinit(&item->path);
    array_deinit(&item->name);
    map_info_deinit(&item->info);
    resources_loaded_image_remove(resources, item->image);
    memset(item, 0, sizeof *item);
}

void cubemap_init(Cubemap* item, Resources* resources)
{
    for(isize i = 0; i < 6; i++)
        map_init(&item->faces[i], resources);
}

void cubemap_deinit(Cubemap* item, Resources* resources)
{
    for(isize i = 0; i < 6; i++)
        map_deinit(&item->faces[i], resources);
}

void material_init(Material* item, Resources* resources)
{
    material_deinit(item, resources);
    array_init(&item->path, resources->alloc);
    array_init(&item->name, resources->alloc);

    map_init(&item->map_alpha, resources);  
    map_init(&item->map_bump, resources);              
    map_init(&item->map_displacement, resources);      
    map_init(&item->map_stencil, resources);  
    map_init(&item->map_roughness, resources);
    map_init(&item->map_ambient_occlusion, resources);
    map_init(&item->map_metallic, resources);     
    map_init(&item->map_albedo, resources);     
    map_init(&item->map_emmisive, resources);           
    map_init(&item->map_normal, resources);             
    map_init(&item->map_reflection_sphere, resources);   
    cubemap_init(&item->map_reflection_cube, resources);
}

void material_deinit(Material* item, Resources* resources)
{
    array_deinit(&item->path);
    array_deinit(&item->name);
    memset(item, 0, sizeof *item);
    
    map_deinit(&item->map_alpha, resources);  
    map_deinit(&item->map_bump, resources);              
    map_deinit(&item->map_displacement, resources);      
    map_deinit(&item->map_stencil, resources);  
    map_deinit(&item->map_roughness, resources);
    map_deinit(&item->map_ambient_occlusion, resources);
    map_deinit(&item->map_metallic, resources);           
    map_deinit(&item->map_albedo, resources);           
    map_deinit(&item->map_emmisive, resources);           
    map_deinit(&item->map_normal, resources);             
    map_deinit(&item->map_reflection_sphere, resources);   
    cubemap_deinit(&item->map_reflection_cube, resources);
}

void object_group_init(Object_Group* item, Resources* resources)
{
    object_group_deinit(item, resources);
    array_init(&item->name, resources->alloc);
}

void object_group_deinit(Object_Group* item, Resources* resources)
{
    array_deinit(&item->name);
    resources_material_remove(resources, item->material);
    memset(item, 0, sizeof *item);
}

void object_init(Object* item, Resources* resources)
{
    object_deinit(item, resources);

    array_init(&item->path, resources->alloc);
    array_init(&item->name, resources->alloc);

    for(isize i = 0; i < item->groups.size; i++)
        object_group_init(&item->groups.data[i], resources);
}

void object_deinit(Object* item, Resources* resources)
{
    array_deinit(&item->path);
    array_deinit(&item->name);
    resources_material_remove(resources, item->fallback_material);
    resources_shape_remove(resources, item->shape);

    for(isize i = 0; i < item->groups.size; i++)
        object_group_deinit(&item->groups.data[i], resources);

    memset(item, 0, sizeof *item);
}





#endif