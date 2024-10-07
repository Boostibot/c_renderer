#include "asset_descriptions.h"
#include "asset.h"
#include "name.h"
#include "lib/file.h"
#include "lib/serialize.h"

typedef enum Asset_Type_Enum {
    ASSET_TYPE_NONE = 0,
    ASSET_TYPE_FILE,
    ASSET_TYPE_SHADER,
    ASSET_TYPE_IMAGE,
    ASSET_TYPE_CUBEMAP,
    ASSET_TYPE_GEOMETRY,
    ASSET_TYPE_MATERIAL,
    ASSET_TYPE_MODEL,

    ASSET_TYPE_ENUM_COUNT
} Asset_Type_Enum;

typedef u64 Graphics_Handle;

typedef struct _File_Asset_Handle* File_Asset_Handle;
typedef struct File_Asset {
    union {
        File_Asset_Handle handle;
        Asset_Handle uhandle;
        const Asset asset;
    };
    String_Builder contents;
} File_Asset;

void file_asset_init(File_Asset* asset) { 
    builder_init(&asset->contents, asset_allocator());
}
void file_asset_deinit(File_Asset* asset) { 
    builder_deinit(&asset->contents); 
}
void file_asset_copy(File_Asset* to, const File_Asset* from) {
    builder_assign(&to->contents, from->contents.string);
}
void file_asset_type_add() {
    Asset_Type_Description desc = {0};
    desc.size = sizeof(File_Asset);
    desc.name = STRING("File_Asset");
    desc.abbreviation = STRING("FILE");

    desc.init = (Asset_Init) (void*) file_asset_init;
    desc.deinit = (Asset_Deinit) (void*) file_asset_deinit;
    desc.copy = (Asset_Copy) (void*) file_asset_copy;

    TEST(asset_type_add(ASSET_TYPE_FILE, desc) != NULL);
}

File_Asset*       file_asset_get(File_Asset_Handle handle)              { return (File_Asset*)       (void*) asset_get(asset_downcast(ASSET_TYPE_FILE, handle)); }
File_Asset*       file_asset_create(Hash_String path, Hash_String name) { return (File_Asset*) (void*) asset_get(asset_create(ASSET_TYPE_FILE, path, name)); }
File_Asset_Handle file_asset_find(Hash_String path, Hash_String name)   { return (File_Asset_Handle) (void*) asset_find(ASSET_TYPE_FILE, path, name); }
File_Asset_Handle file_asset_find_id(Hash_String id)                    { return (File_Asset_Handle) (void*) asset_find_id(ASSET_TYPE_FILE, id); }

typedef struct _Shader_Asset_Handle* Shader_Asset_Handle;
typedef struct Shader_Asset {
    union {
        Shader_Asset_Handle handle;
        Asset_Handle uhandle;
        const Asset asset;
    };
    File_Asset_Handle vertex;
    File_Asset_Handle fragment;
    File_Asset_Handle geometry;
    File_Asset_Handle compute;
} Shader_Asset;

void shader_asset_get_refs(Shader_Asset* asset, Asset_Handle_Array* handles) {
    array_push(handles, asset_downcast(ASSET_TYPE_FILE, asset->vertex));
    array_push(handles, asset_downcast(ASSET_TYPE_FILE, asset->fragment));
    array_push(handles, asset_downcast(ASSET_TYPE_FILE, asset->geometry));
    array_push(handles, asset_downcast(ASSET_TYPE_FILE, asset->compute));
}

void shader_asset_type_add() {
    Asset_Type_Description desc = {0};
    desc.size = sizeof(Shader_Asset);
    desc.name = STRING("Shader_Asset");
    desc.abbreviation = STRING("SHDR");
    desc.get_refs = (Asset_Get_Refs) (void*) shader_asset_get_refs;

    TEST(asset_type_add(ASSET_TYPE_SHADER, desc) != NULL);
}

Shader_Asset*       shader_asset_get(Shader_Asset_Handle handle)            { return (Shader_Asset*)       (void*) asset_get(asset_downcast(ASSET_TYPE_SHADER, handle)); }
Shader_Asset*       shader_asset_create(Hash_String path, Hash_String name) { return (Shader_Asset*)       (void*) asset_get(asset_create(ASSET_TYPE_SHADER, path, name)); }
Shader_Asset_Handle shader_asset_find(Hash_String path, Hash_String name)   { return (Shader_Asset_Handle) (void*) asset_find(ASSET_TYPE_SHADER, path, name); }
Shader_Asset_Handle shader_asset_find_id(Hash_String id)                    { return (Shader_Asset_Handle) (void*) asset_find_id(ASSET_TYPE_SHADER, id); }

typedef struct _Image_Asset_Handle* Image_Asset_Handle;
typedef struct Image_Asset {
    union {
        Image_Asset_Handle handle;
        Asset_Handle uhandle;
        const Asset asset;
    };
    Map_Info info;
    u32 _;
    Image image;
} Image_Asset;

void image_asset_init(Image_Asset* asset) { 
    //map_info_init(&asset->info, asset_allocator());
    image_init(&asset->image, asset_allocator(), 4, PIXEL_TYPE_U8);
}
void image_asset_deinit(Image_Asset* asset) {
    //map_info_deinit(&asset->info); 
    image_deinit(&asset->image); 
}

void image_asset_copy(Image_Asset* to, const Image_Asset* from) {
    to->info = from->info;
    image_assign(&to->image, subimage_of(from->image));
}

void image_asset_type_add() {
    Asset_Type_Description desc = {0};
    desc.size = sizeof(Image_Asset);
    desc.name = STRING("Image_Asset");
    desc.abbreviation = STRING("IMG");

    desc.init = (Asset_Init) (void*) image_asset_init;
    desc.deinit = (Asset_Deinit) (void*) image_asset_deinit;
    desc.copy = (Asset_Copy) (void*) image_asset_copy;

    TEST(asset_type_add(ASSET_TYPE_IMAGE, desc) != NULL);
}

Image_Asset*       image_asset_get(Image_Asset_Handle handle)             { return (Image_Asset*)       (void*) asset_get(asset_downcast(ASSET_TYPE_IMAGE, handle)); }
Image_Asset*       image_asset_create(Hash_String path, Hash_String name) { return (Image_Asset*)       (void*) asset_get(asset_create(ASSET_TYPE_IMAGE, path, name)); }
Image_Asset_Handle image_asset_find(Hash_String path, Hash_String name)   { return (Image_Asset_Handle) (void*) asset_find(ASSET_TYPE_IMAGE, path, name); }
Image_Asset_Handle image_asset_find_id(Hash_String id)                    { return (Image_Asset_Handle) (void*) asset_find_id(ASSET_TYPE_IMAGE, id); }

#if 0
typedef struct _Cubemap_Asset_Handle* Cubemap_Asset_Handle;
typedef struct Cubemap_Asset {
    const Asset asset;
    union {
        struct {
            Image_Asset_Handle top;   
            Image_Asset_Handle bottom;
            Image_Asset_Handle front; 
            Image_Asset_Handle back;  
            Image_Asset_Handle left;  
            Image_Asset_Handle right;
        };

        Image_Asset_Handle faces[6];
    };
} Cubemap_Asset;

void cubemap_asset_get_refs(Cubemap_Asset* asset, Asset_Handle_Array* handles) {
    for(int i = 0; i < 6; i++)
        array_push(handles, (Asset_Handle*) (void*) asset->faces[i]);
}

void cubemap_asset_type_add() {
    Asset_Type_Description desc = {0};
    desc.size = sizeof(Cubemap_Asset);
    desc.name = STRING("Cubemap_Asset");
    desc.abbreviation = STRING("CUBEM");
    desc.get_refs = (Asset_Get_Refs) (void*) cubemap_asset_get_refs;

    TEST(asset_type_add(ASSET_TYPE_FILE, desc) != NULL);
}
#endif

typedef struct _Geometry_Asset_Handle* Geometry_Asset_Handle;
typedef struct Geometry_Asset {
    union {
        Geometry_Asset_Handle handle;
        Asset_Handle uhandle;
        const Asset asset;
    };
    Shape_Assembly shape;
} Geometry_Asset;

void geometry_asset_init(Geometry_Asset* asset) { 
    shape_assembly_init(&asset->shape, asset_allocator());
}
void geometry_asset_deinit(Geometry_Asset* asset) {
    shape_assembly_deinit(&asset->shape);
}

void geometry_asset_copy(Geometry_Asset* to, const Geometry_Asset* from) {
    (void) to;
    (void) from;
    TODO();
}

void geometry_asset_type_add() {
    Asset_Type_Description desc = {0};
    desc.size = sizeof(Geometry_Asset);
    desc.name = STRING("Geometry_Asset");
    desc.abbreviation = STRING("GEO");

    desc.init = (Asset_Init) (void*) geometry_asset_init;
    desc.deinit = (Asset_Deinit) (void*) geometry_asset_deinit;
    desc.copy = (Asset_Copy) (void*) geometry_asset_copy;

    TEST(asset_type_add(ASSET_TYPE_GEOMETRY, desc) != NULL);
}

Geometry_Asset*       geometry_asset_get(Geometry_Asset_Handle handle)          { return (Geometry_Asset*)       (void*) asset_get(asset_downcast(ASSET_TYPE_GEOMETRY, handle)); }
Geometry_Asset*       geometry_asset_create(Hash_String path, Hash_String name) { return (Geometry_Asset*)       (void*) asset_get(asset_create(ASSET_TYPE_GEOMETRY, path, name)); }
Geometry_Asset_Handle geometry_asset_find(Hash_String path, Hash_String name)   { return (Geometry_Asset_Handle) (void*) asset_find(ASSET_TYPE_GEOMETRY, path, name); }
Geometry_Asset_Handle geometry_asset_find_id(Hash_String id)                    { return (Geometry_Asset_Handle) (void*) asset_find_id(ASSET_TYPE_GEOMETRY, id); }

typedef struct _Material_Asset_Handle* Material_Asset_Handle;
typedef struct Material_Asset {
    union {
        Material_Asset_Handle handle;
        Asset_Handle uhandle;
        const Asset asset;
    };
    Asset_Handle_Array children;
    
    Material_Info info;
    Image_Asset_Handle maps[MAP_TYPE_ENUM_COUNT];
    Image_Asset_Handle cubemaps[CUBEMAP_TYPE_ENUM_COUNT][6];
} Material_Asset;

void material_asset_get_refs(Material_Asset* asset, Asset_Handle_Array* handles) {
    for(int i = 0; i < ARRAY_LEN(asset->maps); i++)
        array_push(handles, asset_downcast(ASSET_TYPE_IMAGE, asset->maps[i]));

    for(int i = 0; i < ARRAY_LEN(asset->cubemaps); i++)
        for(int j = 0; j < 6; j++)
            array_push(handles, asset_downcast(ASSET_TYPE_IMAGE, asset->cubemaps[i][j]));

    array_append(handles, asset->children.data, asset->children.len);
}

void material_asset_type_add() {
    Asset_Type_Description desc = {0};
    desc.size = sizeof(Material_Asset);
    desc.name = STRING("Material_Asset");
    desc.abbreviation = STRING("MATER");
    desc.get_refs = (Asset_Get_Refs) (void*) material_asset_get_refs;

    TEST(asset_type_add(ASSET_TYPE_MATERIAL, desc) != NULL);
}

Material_Asset*       material_asset_get(Material_Asset_Handle handle)          { return (Material_Asset*)       (void*) asset_get(asset_downcast(ASSET_TYPE_MATERIAL, handle)); }
Material_Asset*       material_asset_create(Hash_String path, Hash_String name) { return (Material_Asset*)       (void*) asset_get(asset_create(ASSET_TYPE_MATERIAL, path, name)); }
Material_Asset_Handle material_asset_find(Hash_String path, Hash_String name)   { return (Material_Asset_Handle) (void*) asset_find(ASSET_TYPE_MATERIAL, path, name); }
Material_Asset_Handle material_asset_find_id(Hash_String id)                    { return (Material_Asset_Handle) (void*) asset_find_id(ASSET_TYPE_MATERIAL, id); }

typedef struct _Model_Asset_Handle* Model_Asset_Handle;
typedef struct Model_Asset {
    union {
        Model_Asset_Handle handle;
        Asset_Handle uhandle;
        const Asset asset;
    };

    Asset_Handle_Array children;
    Material_Asset_Handle material;
    
    i32 triangles_from;
    i32 triangles_to;
    Geometry_Asset_Handle geometry;
} Model_Asset;

void model_asset_get_refs(Model_Asset* asset, Asset_Handle_Array* handles) {
    array_push(handles, asset_downcast(ASSET_TYPE_GEOMETRY, asset->geometry));
    array_push(handles, asset_downcast(ASSET_TYPE_GEOMETRY, asset->material));
    array_append(handles, asset->children.data, asset->children.len);
}

void model_asset_type_add() {
    Asset_Type_Description desc = {0};
    desc.size = sizeof(Model_Asset);
    desc.name = STRING("Model_Asset");
    desc.abbreviation = STRING("MODEL");
    desc.get_refs = (Asset_Get_Refs) (void*) model_asset_get_refs;

    TEST(asset_type_add(ASSET_TYPE_MODEL, desc) != NULL);
}

Model_Asset*       model_asset_get(Model_Asset_Handle handle)             { return (Model_Asset*)       (void*) asset_get(asset_downcast(ASSET_TYPE_MODEL, handle)); }
Model_Asset*       model_asset_create(Hash_String path, Hash_String name) { return (Model_Asset*)       (void*) asset_get(asset_create(ASSET_TYPE_MODEL, path, name)); }
Model_Asset_Handle model_asset_find(Hash_String path, Hash_String name)   { return (Model_Asset_Handle) (void*) asset_find(ASSET_TYPE_MODEL, path, name); }
Model_Asset_Handle model_asset_find_id(Hash_String id)                    { return (Model_Asset_Handle) (void*) asset_find_id(ASSET_TYPE_MODEL, id); }

void asset_types_add_all()
{
    file_asset_type_add();
    image_asset_type_add();
    //cubemap_asset_type_add();
    geometry_asset_type_add();
    material_asset_type_add();
    model_asset_type_add();
}
