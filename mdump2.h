#pragma once

#include "lib/string.h"

typedef union {
    char text[9];
    u64 value;
} Mdump_Magic_64;

#define MDUMP_MAGIC         BRACE_INIT(Mdump_Magic_64){"mdump"}.value
#define MDUMP_MAGIC_BLOCK   BRACE_INIT(Mdump_Magic_64){"mdumpblk"}.value

#define MDUMP_BLOCK_ALIGN    8
#define MDUMP_ARRAY_TYPE_BIT    ((u32) 1 << 31)
#define MDUMP_PTR_TYPE_BIT      ((u32) 1 << 30)
#define MDUMP_UNUSED_MEMORY_PATTERN 0x55

#define MDUMP_FLAG_OPTIONAL 1
#define MDUMP_FLAG_TRANSPARENT 2
#define MDUMP_FLAG_DO_NOT_TRANSLATE 8

#define MDUMP_FLAG_MAGIC 2
#define MDUMP_FLAG_CHECKSUM 2

#define MDUMP_FLAG_REQUIRES_AT_LEAST_ONE_MEMBER 2
#define MDUMP_FLAG_REQUIRES_EXACTLY_LEAST_ONE_MEMBER 4

#define _MDUMP_FLAG_REMOVED_TYPE ((u32) 1 << 31)

typedef enum Mdump_Action {
    MDUMP_READ = 0,
    MDUMP_WRITE = 1,
    MDUMP_REGISTER = 2,
    MDUMP_VALIDATE = 3,
} Mdump_Action;

typedef enum Mdump_Type {
    MDUMP_TYPE_NONE = 0,
    MDUMP_TYPE_BLANK,
    MDUMP_TYPE_COMMENT,
    MDUMP_TYPE_STRING,
    MDUMP_TYPE_LONG_STRING,

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

    MDUMP_TYPE_MAX_RESERVED = 64,
} Mdump_Type;

typedef struct {
    u32 block;
    u32 offset;
} Mdump_Ptr;

typedef struct {
    Mdump_Ptr ptr;
    u32 size;
} Mdump_Array;

typedef Mdump_Array Mdump_String;
typedef u64     Mdump_Magic;

typedef struct {
    Mdump_Type type;
    u32 offset;
    u32 size;
    u32 flags;
    String name;
} Mdump_Type_Member;

typedef struct {
    Mdump_Type type;
    u32 size;
    u32 align;
    u32 flags;
    String type_name;
    const Mdump_Type_Member* members;
    u32 members_size;
} Mdump_Type_Info;

typedef struct {
    Mdump_Type type;
    u32 offset;
    u32 size;
    u32 flags;
    Mdump_String name;
} Mdump_Type_Member_Rep;

typedef struct {
    Mdump_Type type;
    u32 size;
    u32 align;
    u32 flags;
    Mdump_String type_name;
    Mdump_Array members;
} Mdump_Type_Info_Rep;

typedef struct {
    isize size;
    isize used_to;
    u8* data;
} Mdump_Block;

DEFINE_ARRAY_TYPE(Mdump_Block, Mdump_Block_Array);

typedef struct {
    Allocator* allocator;

    i64 creation_time;
    i64 version;

    String_Builder name;
    String_Builder description;

    Mdump_Array types;
    Mdump_Block_Array blocks;
    isize current_block_i1;
    isize current_meta_block_i1;

    isize block_default_size;
} Mdump_File;

Mdump_Type      mdump_value_type(Mdump_Type type);
Mdump_Type      mdump_array_type(Mdump_Type type);
Mdump_Type      mdump_ptr_type(Mdump_Type type);
Mdump_Ptr       mdump_allocate_with_addr(Mdump_File* file, isize size, isize align, void** addr);
Mdump_Ptr       mdump_allocate(Mdump_File* file, isize size, isize align);

Mdump_String    mdump_string_allocate(Mdump_File* file, isize size);
Mdump_String    mdump_string_from_string(Mdump_File* file, String size);
String          mdump_string_to_string(Mdump_File* file, Mdump_String size);

void* mdump_get(Mdump_File* file, Mdump_Ptr ptr, isize size);
void* mdump_get_array(Mdump_File* file, Mdump_Array array, isize item_size);
char* mdump_get_string(Mdump_File* file, Mdump_String string);

#define MDUMP_GET(file, ptr, Type)          (Type*) mdump_get((file), (ptr), sizeof(Type))
#define MDUMP_GET_ARRAY(file, arr, Type)    (Type*) mdump_get_array((file), (arr), sizeof(Type))


Mdump_Type mdump_value_type(Mdump_Type type)
{
    return (Mdump_Type) (((u32) type & ~MDUMP_ARRAY_TYPE_BIT) & ~MDUMP_PTR_TYPE_BIT);
}

Mdump_Type mdump_array_type(Mdump_Type type)
{
    return (Mdump_Type) ((u32) mdump_value_type(type) | MDUMP_ARRAY_TYPE_BIT);
}

Mdump_Type mdump_ptr_type(Mdump_Type type)
{
    return (Mdump_Type) ((u32) mdump_value_type(type) | MDUMP_PTR_TYPE_BIT);
}

//@TODO: remove
String _mdump_string_own(Mdump_File* file, String string)
{
    if(string.size == 0)
        return STRING("");

    char* new_data = (char*) allocator_allocate(file->allocator, string.size + 1, 1, SOURCE_INFO());
    new_data[string.size] = '\0';

    String out = {new_data, string.size};

}

void _mdump_string_disown(Mdump_File* file, String string)
{
    if(string.size > 0)
        allocator_deallocate(file->allocator, (void*) string.data, string.size + 1, 1, SOURCE_INFO());
}

Mdump_Ptr _mdump_allocate_with_addr(Mdump_File* file, isize size, isize align, void** addr, isize* block_i1, bool is_retry)
{
    if(*block_i1 == 0)
    {
        if(file->block_default_size == 0)
            file->block_default_size = 4096;

        if(file->allocator == NULL)
            file->allocator = allocator_get_default();

        Mdump_Block new_block = {0};
        new_block.size = MAX(file->block_default_size, size);
        new_block.data = (u8*) allocator_allocate(file->allocator, new_block.size, MDUMP_BLOCK_ALIGN, SOURCE_INFO());

        array_push(&file->blocks, new_block);
        *block_i1 = file->blocks.size;
        
        //Fill with marker data (0b01010101)
        #ifdef DO_ASSERTS_SLOW
        memset(new_block.data, MDUMP_UNUSED_MEMORY_PATTERN, new_block.size);
        #endif DO_ASSERTS_SLOW
    }

    ASSERT(*block_i1 > 0);
    ASSERT(file->blocks.size > 0);

    Mdump_Block* curr_block = &file->blocks.data[*block_i1 - 1];
    u8* new_data = (u8*) align_forward(curr_block->data + curr_block->used_to, align);
    isize offset = new_data - curr_block->data;
    if(offset + size > curr_block->size)
    {
        ASSERT(is_retry == false);
        *block_i1 = 0;
        return _mdump_allocate_with_addr(file, size, align, addr, block_i1, true);
    }
    #ifdef DO_ASSERTS_SLOW
    for(isize i = 0; i < size; i++)
    {
        ASSERT_SLOW_MSG(new_data[i] == MDUMP_UNUSED_MEMORY_PATTERN, 
            "Unused memory was found to be corrupeted. Check all functions for overrides");
    }
    #endif // DO_ASSERTS_SLOW

    curr_block->used_to += size;
    memset(new_data, 0, size);

    Mdump_Ptr out = {0};
    out.block = (u32) *block_i1 - 1;
    out.offset = (u32) offset;
    *addr = new_data;
    return out;
}


void* mdump_get(Mdump_File* file, Mdump_Ptr ptr, isize size)
{
    ASSERT(size > 0);
    if(ptr.offset < sizeof(Mdump_Magic) || ptr.offset >= file->blocks.size)
        return NULL;

    Mdump_Block* block = &file->blocks.data[ptr.block];
    if(ptr.offset + size > block->size)
        return NULL;

    return block->data + ptr.offset;
}

void* mdump_get_array(Mdump_File* file, Mdump_Array array, isize item_size)
{
    return mdump_get(file, array.ptr, array.size*item_size);
}

char* mdump_get_string(Mdump_File* file, Mdump_String string)
{
    return (char*) mdump_get(file, string.ptr, string.size);
}

Mdump_Ptr mdump_allocate_with_addr(Mdump_File* file, isize size, isize align, void** addr)
{
    return _mdump_allocate_with_addr(file, size, align, addr, false, &file->current_block_i1);
}

Mdump_Ptr mdump_allocate(Mdump_File* file, isize size, isize align)
{
    void* addr = NULL;
    return _mdump_allocate_with_addr(file, size, align, &addr, false, &file->current_block_i1);
}

Mdump_Ptr       mdump_meta_allocate_with_addr(Mdump_File* file, isize size, isize align, void** addr)
{
    return _mdump_allocate_with_addr(file, size, align, addr, false, &file->current_meta_block_i1);
}

Mdump_Ptr       mdump_meta_allocate(Mdump_File* file, isize size, isize align)
{
    void* addr = NULL;
    return _mdump_allocate_with_addr(file, size, align, &addr, false, &file->current_meta_block_i1);
}

Mdump_String    mdump_string_allocate(Mdump_File* file, isize size)
{
    Mdump_String out = {0};
    out.ptr = mdump_allocate(file, size + 1, 1);
    out.size = size;
    return out;
}

Mdump_String    mdump_string_from_string(Mdump_File* file, String string, bool is_meta)
{
    void* alloced_data = NULL; 
    Mdump_String out = {0};
    if(is_meta)
        out.ptr = mdump_meta_allocate_with_addr(file, string.size + 1, 1, &alloced_data);
    else
        out.ptr = mdump_allocate_with_addr(file, string.size + 1, 1, &alloced_data);

    out.size = string.size;
    memcpy(alloced_data, string.data, string.size);
    return out;
}

String          mdump_string_to_string(Mdump_File* file, Mdump_String mstring)
{
    String out = {0};
    out.data = mdump_get_string(file, mstring);
    if(out.data != NULL)
        out.size = mstring.size;

    return out;
}

Mdump_Type      mdump_type_by_name(Mdump_File* file, String type_name)
{
    if(type_name.size == 0)
        return MDUMP_TYPE_NONE;

    switch(type_name.data[0])
    {
        break; case 's': if(string_is_equal(type_name, STRING("string"))) return MDUMP_TYPE_STRING;
        break; case 'c': if(string_is_equal(type_name, STRING("char"))) return MDUMP_TYPE_CHAR;
        break; case 'r': if(string_is_equal(type_name, STRING("rune"))) return MDUMP_TYPE_RUNE;
        break; case 'b': if(string_is_equal(type_name, STRING("bool"))) return MDUMP_TYPE_BOOL;

        break; case 'u': 
            if(2 <= type_name.size && type_name.size <= 3)
            {
                if(string_is_equal(type_name, STRING("u8"))) return MDUMP_TYPE_U8;
                if(string_is_equal(type_name, STRING("u16"))) return MDUMP_TYPE_U16;
                if(string_is_equal(type_name, STRING("u32"))) return MDUMP_TYPE_U32;
                if(string_is_equal(type_name, STRING("u64"))) return MDUMP_TYPE_U64;
            }

        break; case 'i':
            if(2 <= type_name.size && type_name.size <= 3)
            {
                if(string_is_equal(type_name, STRING("i8"))) return MDUMP_TYPE_I8;
                if(string_is_equal(type_name, STRING("i16"))) return MDUMP_TYPE_I16;
                if(string_is_equal(type_name, STRING("i32"))) return MDUMP_TYPE_I32;
                if(string_is_equal(type_name, STRING("i64"))) return MDUMP_TYPE_I64;
            }

        break; case 'f':
            if(2 <= type_name.size && type_name.size <= 3)
            {
                if(string_is_equal(type_name, STRING("f8"))) return MDUMP_TYPE_F8;
                if(string_is_equal(type_name, STRING("f16"))) return MDUMP_TYPE_F16;
                if(string_is_equal(type_name, STRING("f32"))) return MDUMP_TYPE_F32;
                if(string_is_equal(type_name, STRING("f64"))) return MDUMP_TYPE_F64;
            }
    }

    for(isize i = 0; i < file->types.size; i++)
    {
        Mdump_Type_Info_Rep* info_rep = MDUMP_GET_ARRAY(file, file->types, Mdump_Type_Info_Rep);
        String found_name = mdump_string_to_string(file, info_rep->type_name);
        if(string_is_equal(type_name, found_name))
            return (Mdump_Type) ((i32) i + MDUMP_TYPE_MAX_RESERVED + 1);
    }

    return MDUMP_TYPE_NONE;
}

Mdump_Type_Info _mdump_builtin_type_info(Mdump_File* file, Mdump_Type type, const char* name, isize size)
{
    (void) file;
    Mdump_Type_Info out = {type};
    out.type_name = string_make(name);
    out.size = size;
    out.align = size;
    return out;
}

Mdump_Type_Info mdump_type_info(Mdump_File* file, Mdump_Type type)
{
    ASSERT(type == mdump_value_type(type));
    if(type > MDUMP_TYPE_MAX_RESERVED)
    {
        isize type_index = (isize) type - MDUMP_TYPE_MAX_RESERVED - 1;
        if(type_index < file->types.size)
        {
            Mdump_Type_Info_Rep* info_rep = MDUMP_GET_ARRAY(file, file->types, Mdump_Type_Info_Rep);
            Mdump_Type_Info info = {info_rep->type};
            info.align = info_rep->align;
            info.size = info_rep->size;
            info.flags = info_rep->flags;
            info.type_name = mdump_string_to_string(file, info_rep->type_name);
            //info.members = MDUMP_GET_ARRAY(file, info_rep->members, MDUMP_)
            //@TODO: members?
            info.size = info_rep->size;
            info.size = info_rep->size;
        }
    }

    switch(type)
    {
        default:
        case MDUMP_TYPE_NONE: return _mdump_builtin_type_info(file, MDUMP_TYPE_NONE, "none", 0);  

        case MDUMP_TYPE_CHAR: return _mdump_builtin_type_info(file, MDUMP_TYPE_CHAR, "char", 1);
        case MDUMP_TYPE_BOOL: return _mdump_builtin_type_info(file, MDUMP_TYPE_BOOL, "bool", 1);
        case MDUMP_TYPE_RUNE: return _mdump_builtin_type_info(file, MDUMP_TYPE_RUNE, "rune", 4);

        case MDUMP_TYPE_U8:   return _mdump_builtin_type_info(file, MDUMP_TYPE_U8, "u8", 1);  
        case MDUMP_TYPE_U16:  return _mdump_builtin_type_info(file, MDUMP_TYPE_U16, "u16", 2);
        case MDUMP_TYPE_U32:  return _mdump_builtin_type_info(file, MDUMP_TYPE_U32, "u32", 4);
        case MDUMP_TYPE_U64:  return _mdump_builtin_type_info(file, MDUMP_TYPE_U64, "u64", 8);
        
        case MDUMP_TYPE_I8:   return _mdump_builtin_type_info(file, MDUMP_TYPE_I8, "i8", 1);  
        case MDUMP_TYPE_I16:  return _mdump_builtin_type_info(file, MDUMP_TYPE_I16, "i16", 2);
        case MDUMP_TYPE_I32:  return _mdump_builtin_type_info(file, MDUMP_TYPE_I32, "i32", 4);
        case MDUMP_TYPE_I64:  return _mdump_builtin_type_info(file, MDUMP_TYPE_I64, "i64", 8);
            
        case MDUMP_TYPE_F8:   return _mdump_builtin_type_info(file, MDUMP_TYPE_F8, "f8", 1);  
        case MDUMP_TYPE_F16:  return _mdump_builtin_type_info(file, MDUMP_TYPE_F16, "f16", 2);
        case MDUMP_TYPE_F32:  return _mdump_builtin_type_info(file, MDUMP_TYPE_F32, "f32", 4);
        case MDUMP_TYPE_F64:  return _mdump_builtin_type_info(file, MDUMP_TYPE_F64, "f64", 8);

        case MDUMP_TYPE_STRING: {
            Mdump_Type_Info out = {MDUMP_TYPE_STRING};
            out.type_name = STRING("string");
            out.size = sizeof(Mdump_String);
            out.align = 4;
            return out;
        }
    }
}

Mdump_Type_Info mdump_type_info_by_name(Mdump_File* file, String type_name)
{
    Mdump_Type type = mdump_type_by_name(file, type_name);
    return mdump_type_info(file, type);
}

Mdump_Type mdump_register_type(Mdump_File* file, Mdump_Type_Info info)
{
    Mdump_Type found = mdump_type_by_name(file, info.type_name);
    if(found != MDUMP_TYPE_NONE)
        return mdump_validate_type_against(file, info, found);

    Mdump_Type_Info_Rep info_rep = {info.type};
    info_rep.size = info.size;
    info_rep.align = info.align;
    info_rep.flags = info.flags;
    info_rep.type_name = mdump_string_from_string(file, info.type_name, true);
    info_rep.members.ptr = mdump_allocate(file, info.members_size * sizeof(Mdump_Type_Member_Rep), DEF_ALIGN);
    info_rep.members.size = info.members_size;

    Mdump_Type_Member_Rep* members_rep = MDUMP_GET_ARRAY(file, info_rep.members, Mdump_Type_Member_Rep);
    ASSERT(members_rep != NULL);

    Mdump_Type out = info.type;
    for(isize i = 0; i < info.members_size; i++)
    {
        Mdump_Type_Member_Rep* member_rep = &members_rep[i];
        const Mdump_Type_Member* member = &info.members[i];

        member_rep->type = member->type;
        member_rep->size = member->size;
        member_rep->offset = member->offset;
        member_rep->flags = member->flags;
        member_rep->name = mdump_string_from_string(file, member->name, true);

        if(member->offset + member->size > info.size)
        {
            out = MDUMP_TYPE_NONE;
            break;
        }

        if()
    }
}

Mdump_Type      mdump_remove_type(Mdump_File* file, Mdump_Type type);
Mdump_Type      mdump_validate_type(Mdump_File* file, Mdump_Type_Info info);
Mdump_Type      mdump_validate_type_against(Mdump_File* file, Mdump_Type_Info info, Mdump_Type against);

//Two options: 
// 1) separately alloced info: 
//    + no wasted space while resizing 
//    + able to remove types or do more sophisticated filtering
//    + faster lookup
//    - slower registering (extra allocs) 
//      -> solved by proper allocator
//    - need to represent 3 types: user friendly non owning, owning, in file representation
//      -> solved by custom strings (use allocator directly instead of String_Builder)
//         thus there is only the one with string (non owning) used as an iterface type
//         one file rep
//      -> still maybe having file rep and memory rep merged would be helpfull (yet to see) 
//      -> dispute can be solved by having "reserved" blocks of temp data. We use these places for
//         allocation of types etc but we dont output them directly. We process them first 
//         and make an export version. We can then fill the missing block sizes by 0 sized blocks
//         => we have two curr block indeces: one for the regular user side allocs and one for
//            custom our side allocs. Most of the time a single block for meta data will more then
//            suffice and will be optimally filled so we can just dump the whole thing. Sometimes
//            we can even process it. Yey! :)
// 2) 
//
//    + no extra allocations => can be used directly with page allocator


