
#include "lib/string.h"
#include "lib/lpf.h"



enum {
    MDUMP_FLAG_PTR = 1,
    MDUMP_FLAG_ARRAY = 2,
    MDUMP_FLAG_LIST = 4,
    MDUMP_FLAG_ENUM = 8,
    MDUMP_FLAG_ENUM_SIGNED = 8,
};

typedef struct Mdump_Member_User Mdump_Member_User;

typedef struct Mdump_String {
    u64 ptr;
    isize size;
} Mdump_String;

typedef struct Mdump_Array {
    u64 ptr;
    isize size;
} Mdump_Array;

typedef enum Mdump_Type_ID {
    MDUMP_TYPE_NONE = 0,
    MDUMP_TYPE_STRING,

    MDUMP_TYPE_CHAR,
    MDUMP_TYPE_RUNE,
    MDUMP_TYPE_BOOL,

    MDUMP_TYPE_U8,
    MDUMP_TYPE_U16,
    MDUMP_TYPE_U32,
    MDUMP_TYPE_U64,

    MDUMP_TYPE_I8,
    MDUMP_TYPE_I16,
    MDUMP_TYPE_I32,
    MDUMP_TYPE_I64,
    
    MDUMP_TYPE_F8,
    MDUMP_TYPE_F16,
    MDUMP_TYPE_F32,
    MDUMP_TYPE_F64,

    MDUMP_TYPE_MAX_RESERVED = 32,
} Mdump_Type_ID;


typedef enum Mdump_Action {
    MDUMP_READ,
    MDUMP_WRITE,
} Mdump_Action;

typedef struct Mdump_Type_File {
    Mdump_String name;
    Mdump_Array members;
    u32 size;
    u32 align;
    u32 flags;
} Mdump_Type_File;

typedef struct Mdump_Member_File {
    Mdump_String name;
    u32 offset;
    u32 flags;
    u32 size;
    Mdump_Type_ID id;
} Mdump_Member_File;

typedef struct Mdump_Header_File {
    u64 magic;
    u64 data_offset;

    i32 version;
    i64 creation_time;
    Mdump_String schema_name;
    Mdump_String description;

    u64 types_ptr;
    u64 root_ptr;
    Mdump_Type_ID root_type;

    u64 next_header_offset;
    u64 checksum;
} Mdump_Header_File;

typedef struct Mdump {
    Arena_Stack file_arena;
    Arena* user_arena;
} Mdump;

void* mdump_file_allocate(Mdump* mdump, isize size, isize align, u64* ptr);

void* mdump_get(Mdump* mdump, u64 ptr, isize size)
{
    if(size <= 0 || ptr + size > mdump->file_arena.size || ptr == 0)
        return NULL;

    return mdump->file_arena.data + ptr;
}

void mdump_init(Mdump* mdump, Arena* user_arena)
{
    arena_init(&mdump->file_arena, 0, 0);

}

#define MDUMP_GET_ARRAY(mdump, mdump_array, Type) ((Type*) mdump_get((mdump), (mdump_array).ptr, (mdump_array).size * sizeof(Type)))
#define MDUMP_GET(mdump, ptr, Type)             ((Type*) mdump_get((mdump), (ptr), sizeof(Type)))
#define MDUMP_MEMBER_PTR(ptr, Type, member)     (ptr + offsetof(Type, member))

u64 mdump_file_allocate(Mdump* mdump, isize size, isize align, void* out_ptr_or_null)
{
    Arena arena = {&mdump->file_arena, 1};
    u8* out_ptr = (u8*) arena_push(&arena, size, align);
    i64 offset = out_ptr - mdump->file_arena.data;
    ASSERT(offset >= 0);
    
    if(out_ptr_or_null)
        *(void**) out_ptr_or_null = out_ptr;

    return offset;
}

Mdump_Array mdump_file_array_allocate(Mdump* mdump, isize item_count, isize item_size, void* out_ptr_or_null)
{
    Mdump_Array out = {0};
    out.ptr = mdump_file_allocate(mdump, item_count*item_size, 8, out_ptr_or_null);
    out.size = item_count;
    return out;
}

void* mdump_user_allocate(Mdump* mdump, isize size, isize align)
{
    return arena_push(mdump->user_arena, size, align);;
}

bool mdump_string(Mdump* mdump, String* user_string, Mdump_String* file_string, Mdump_Action action)
{
    if(action == MDUMP_READ)
    {
        char* data = MDUMP_GET_ARRAY(mdump, *file_string, char);
        if(data == NULL && file_string->size != 0)
        {
            memset(user_string, 0, sizeof* user_string);
            return false;
        }
        else
        {
            String str = {data, file_string->size};
            *user_string = lpf_string_duplicate(mdump->user_arena, str);
            return true;
        }
    }
    else
    {
        void* adress = NULL;
        file_string->ptr = mdump_file_allocate(mdump, user_string->size + 1, 1, &adress);
        file_string->size = user_string->size;
        memcpy(adress, user_string->data, user_string->size);
        return true;
    }
}


bool mdump_raw(Mdump* mdump, void* user, void* file, isize size, Mdump_Action action)
{
    if(action == MDUMP_READ)
        memcpy(user, file, size);
    else
        memcpy(file, user, size);

    return true;
}

bool mdump_array_custom(Mdump* mdump, void** items, isize* item_count, isize item_size, isize align, Mdump_Array* array, Mdump_Action action)
{
    if(action == MDUMP_READ)
    {
        void* data = mdump_get(mdump, array->ptr, array->size * item_size);
        if(data == NULL && array->size != 0)
        {
            *items = NULL;
            *item_count = 0;
            return false;
        }
        else
        {
            void* adress = mdump_user_allocate(mdump, array->size*item_size, align);
            memcpy(adress, data, array->size*item_size);
            *items = adress;
            *item_count = array->size;
            return true;
        }
    }
    else
    {
        void* adress = NULL;
        array->ptr = mdump_file_allocate(mdump, *item_count*item_size, align, &adress);
        array->size = *item_count;
        memcpy(adress, *items, *item_count*item_size);
        return true;
    }
}

bool mdump_array(Mdump* mdump, void** items, isize* item_count, isize item_size, Mdump_Array* array, Mdump_Action action)
{
    return mdump_array_custom(mdump, items, item_count, item_size, 8, array, action);
}

bool mdump_bool(Mdump* mdump, bool* user, bool* file, Mdump_Action action) {return mdump_raw(mdump, user, file, sizeof *user, action);}
bool mdump_char(Mdump* mdump, char* user, char* file, Mdump_Action action) {return mdump_raw(mdump, user, file, sizeof *user, action);}
bool mdump_rune(Mdump* mdump, u32* user, u32* file, Mdump_Action action) {return mdump_raw(mdump, user, file, sizeof *user, action);}

bool mdump_u8(Mdump* mdump, u8* user, u8* file, Mdump_Action action)    {return mdump_raw(mdump, user, file, sizeof *user, action);}
bool mdump_u16(Mdump* mdump, u16* user, u16* file, Mdump_Action action) {return mdump_raw(mdump, user, file, sizeof *user, action);}
bool mdump_u32(Mdump* mdump, u32* user, u32* file, Mdump_Action action) {return mdump_raw(mdump, user, file, sizeof *user, action);}
bool mdump_u64(Mdump* mdump, u64* user, u64* file, Mdump_Action action) {return mdump_raw(mdump, user, file, sizeof *user, action);}

bool mdump_i8(Mdump* mdump, i8* user, i8* file, Mdump_Action action)    {return mdump_raw(mdump, user, file, sizeof *user, action);}
bool mdump_i16(Mdump* mdump, i16* user, i16* file, Mdump_Action action) {return mdump_raw(mdump, user, file, sizeof *user, action);}
bool mdump_i32(Mdump* mdump, i32* user, i32* file, Mdump_Action action) {return mdump_raw(mdump, user, file, sizeof *user, action);}
bool mdump_i64(Mdump* mdump, i64* user, i64* file, Mdump_Action action) {return mdump_raw(mdump, user, file, sizeof *user, action);}

bool mdump_f32(Mdump* mdump, f32* user, f32* file, Mdump_Action action) {return mdump_raw(mdump, user, file, sizeof *user, action);}
bool mdump_f64(Mdump* mdump, f64* user, f64* file, Mdump_Action action) {return mdump_raw(mdump, user, file, sizeof *user, action);}

typedef struct Mdump_Type_User {
    const char* name;
    u32 size;
    u32 align;
    Mdump_Member_User* members;
    u32 member_count;
    u32 flags;
} Mdump_Type_User;

typedef Mdump_Type_User (*Mdump_Type_User_Query)();

typedef struct Mdump_Member_User {
    const char* name;
    Mdump_Type_User_Query type;

    u32 offset;
    u32 flags;
    u64 enum_value;
} Mdump_Member_User;

//Builtins
Mdump_Type_User mdump_type_char() { return BRACE_INIT(Mdump_Type_User){"char", sizeof(char), sizeof(char)}; }
Mdump_Type_User mdump_type_u32() { return BRACE_INIT(Mdump_Type_User){"u32", sizeof(u32), sizeof(u32)}; }
Mdump_Type_User mdump_type_u64() { return BRACE_INIT(Mdump_Type_User){"u64", sizeof(u64), sizeof(u64)}; }
Mdump_Type_User mdump_type_i32() { return BRACE_INIT(Mdump_Type_User){"i32", sizeof(i32), sizeof(i32)}; }
Mdump_Type_User mdump_type_i64() { return BRACE_INIT(Mdump_Type_User){"i64", sizeof(i64), sizeof(i64)}; }
Mdump_Type_User mdump_type_string() { return BRACE_INIT(Mdump_Type_User){"string", sizeof(Mdump_String), 8}; }

typedef struct Mdump_Type_File {
    Mdump_String name;
    Mdump_Array members;
    u32 size;
    u32 align;
    u32 member_count;
    u32 flags;
} Mdump_Type_File;

typedef struct Mdump_Member_File {
    Mdump_String name;
    u32 offset;
    u32 flags;
    u32 size;
    u64 enum_value;
    Mdump_Type_ID id;
} Mdump_Member_File;


//this can be simplified a lot using some defines...
#define DEFINE_MDUMP_TYPE_ALIGNED(Type, mdum_type_func, align, ...) \
    Mdump_Type_User mdum_type_func() \
    { \
        typedef Type T; /* so that MDUMP_MEMBER can use it */\
        static Mdump_Member_User members[] = {  \
            __VA_ARGS__ \
        }; \
        \
        Mdump_Type_User out = {#Type, sizeof(Type), align, members, STATIC_ARRAY_SIZE(members)}; \
        return out; \
    } \
    
#define DEFINE_MDUMP_TYPE(Type, mdum_type_func, ...) \
    DEFINE_MDUMP_TYPE_ALIGNED(Type, mdum_type_func, 8, ##__VA_ARGS__) 

#define MDUMP_MEMBER(name, type, ...) {#name, type, offsetof(T, name), ##__VA_ARGS__}, 

DEFINE_MDUMP_TYPE(Mdump_Member_File, mdump_type_mdump_member, 
    MDUMP_MEMBER(name, mdump_type_string)
    MDUMP_MEMBER(offset, mdump_type_u32)
    MDUMP_MEMBER(flags, mdump_type_u32)
    MDUMP_MEMBER(size, mdump_type_u32)
    MDUMP_MEMBER(id, mdump_type_u32)
);

DEFINE_MDUMP_TYPE(Mdump_Type_File, mdump_type_mdump_type, 
    MDUMP_MEMBER(name, mdump_type_string)
    MDUMP_MEMBER(members, mdump_type_mdump_member)
    MDUMP_MEMBER(size, mdump_type_u32)
    MDUMP_MEMBER(align, mdump_type_u32)
    MDUMP_MEMBER(member_count, mdump_type_u32)
    MDUMP_MEMBER(flags, mdump_type_u32)
);


Mdump_Type_User mdum_enum_type_func() \
{ 
    u32 flag = MDUMP_FLAG_ENUM;
    typedef Type T;
    static Mdump_Member_User members[] = {  
        {"ENUM_VALUE", NULL, 0, MDUMP_FLAG_ENUM, 0x648454},
        {"ENUM_VALUE", NULL, 0, MDUMP_FLAG_ENUM, 0x648454},
        {"ENUM_VALUE", NULL, 0, MDUMP_FLAG_ENUM, 0x648454},
        {"ENUM_VALUE", NULL, 0, MDUMP_FLAG_ENUM, 0x648454},
    }; \
    \
    Mdump_Type_User out = {#Type, sizeof(Type), align, members, STATIC_ARRAY_SIZE(members), MDUMP_FLAG_ENUM}; \
    return out; \
}

bool mdump_mdump_type_member(Mdump* mdump, Mdump_Member_User* user, Mdump_Member_File* file, Mdump_Action action)
{
    
}

bool mdump_mdump_type(Mdump* mdump, Mdump_Type_User* type, Mdump_Type_File* type_file, Mdump_Action action)
{
    if(action == MDUMP_READ)
    {
        memset(type, 0, sizeof *type);

        Mdump_Member_File* members_file = MDUMP_GET_ARRAY(mdump, type_file->members, Mdump_Member_File);
        if(members_file == NULL)
            return false;

        String name = {0};
        if(mdump_string(mdump, &name, &type_file->name, MDUMP_READ) == false)
            return false;
    
        Mdump_Member_User* members_user = (Mdump_Member_User*) mdump_user_allocate(mdump, type_file->members.size, 8);
        for(isize i = 0; i < type_file->members.size; i++)
            mdump_mdump_type_member(mdump, &members_user[i], &members_file[i], MDUMP_READ);

        type->name = name.data;
        type->members = members_user;
        type->size = type_file->size;
        type->align = type_file->align;
        type->member_count = type_file->members.size;
        type->flags = type_file->flags;
        return true;
    }
    else
    {
        memset(type_file, 0, sizeof *type_file);

        String name = string_make(type->name);
        mdump_string(mdump, &name, &type_file->name, MDUMP_WRITE);
        
        Mdump_Member_File* members_file = NULL;
        type_file->members = mdump_file_array_allocate(mdump, type->member_count, sizeof(Mdump_Member_User), &members_file);
        for(isize i = 0; i < type_file->members.size; i++)
            mdump_mdump_type_member(mdump, &type->members[i], &members_file[i], MDUMP_WRITE);

        type_file->size = type->size;
        type_file->align = type->align;
        type_file->flags = type->flags;
        return true;
    }
}