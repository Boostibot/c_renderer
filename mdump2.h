#pragma once

#include "lib/string.h"
#include "lib/log.h"
#include "lib/vformat.h"

typedef union {
    char text[9];
    u64 value;
} Mdump_Magic_64;

#define MDUMP_MAGIC         BRACE_INIT(Mdump_Magic_64){"mdump"}.value
#define MDUMP_MAGIC_BLOCK   BRACE_INIT(Mdump_Magic_64){"mdumpblk"}.value
#define MDUMP_MAGIC_META    BRACE_INIT(Mdump_Magic_64){"mdumpmta"}.value

#define MDUMP_BLOCK_ALIGN    8
#define MDUMP_ARRAY_ALIGN    4
#define MDUMP_PTR_ALIGN      4
#define MDUMP_ARRAY_TYPE_BIT    ((u32) 1 << 31)
#define MDUMP_PTR_TYPE_BIT      ((u32) 1 << 30)
#define MDUMP_UNUSED_MEMORY_PATTERN 0x55

#define MDUMP_FLAG_OPTIONAL 1
#define MDUMP_FLAG_TRANSPARENT 2
#define MDUMP_FLAG_DO_NOT_TRANSLATE 8
#define MDUMP_FLAG_MAGIC 16
#define MDUMP_FLAG_CHECKSUM 32
#define MDUMP_FLAG_REQUIRES_AT_LEAST_ONE_MEMBER 64
#define MDUMP_FLAG_REQUIRES_EXACTLY_ONE_MEMBER 128

#define _MDUMP_FLAG_REMOVED_TYPE ((u32) 1 << 31)

typedef enum Mdump_Action {
    MDUMP_READ = 0,
    MDUMP_WRITE = 1,
    MDUMP_REGISTER = 2,
    MDUMP_VALIDATE = 3,
    MDUMP_QUERY_INFO = 4
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
typedef Mdump_Array Mdump_Long_String;
typedef u64         Mdump_Magic;

typedef struct {
    Mdump_Type type;
    u32 offset;
    u32 flags;
    Mdump_String name;
    Mdump_Ptr default_value;
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

    isize type_capacity;
    Mdump_Array types;
    Mdump_Block_Array blocks;
    isize current_block_i1;
    isize current_meta_block_i1;

    isize block_default_size;
} Mdump_File;

typedef struct {
    //Mdump_Type type;
    void* query_func;
    String name;
    u32 offset;
    u32 size;
    u32 flags;
    const void* default_value;
} Mdump_Type_Member;

typedef struct {
    String type_name;
    u32 size;
    u32 align;
    u32 flags;
    const Mdump_Type_Member* members;
    u32 members_size;
} Mdump_Type_Info;

typedef bool (*Mdump_Func)(Mdump_File* file, void* in_memory, void* in_file, Mdump_Type_Info* info, Mdump_Action action);

Mdump_Type_Info mdump_type_info_make(const char* name, u32 size, const Mdump_Type_Member* members, u32 members_size, u32 align_or_zero, u32 flags);

Mdump_Type      mdump_value_type(Mdump_Type type);
Mdump_Type      mdump_array_type(Mdump_Type type);
Mdump_Type      mdump_ptr_type(Mdump_Type type);

bool            mdump_is_value_type(Mdump_Type type);
bool            mdump_is_array_type(Mdump_Type type);
bool            mdump_is_ptr_type(Mdump_Type type);

Mdump_Ptr       mdump_allocate(Mdump_File* file, isize size, isize align, void** addr);

Mdump_String    mdump_string_allocate(Mdump_File* file, isize size);
Mdump_String    mdump_string_from_string(Mdump_File* file, String size);
String          mdump_string_to_string(Mdump_File* file, Mdump_String size);

void* mdump_get(Mdump_File* file, Mdump_Ptr ptr, isize size);
void* mdump_get_array(Mdump_File* file, Mdump_Array array, isize item_size, isize *item_count);
void* mdump_get_array_sure(Mdump_File* file, Mdump_Array array, isize item_size);
char* mdump_get_string(Mdump_File* file, Mdump_String string);

#define MDUMP_GET(file, ptr, Type)                    ((Type*) mdump_get((file), (ptr), sizeof(Type)))
#define MDUMP_GET_ARRAY(file, arr, size_ptr, Type)    ((Type*) mdump_get_array((file), (arr), sizeof(Type), (size_ptr)))
#define MDUMP_GET_ARRAY_SURE(file, arr, Type)         ((Type*) mdump_get_array_sure((file), (arr), sizeof(Type)))


Mdump_Type_Info mdump_type_info_make(const char* name, u32 size, const Mdump_Type_Member* members, u32 members_size, u32 align_or_zero, u32 flags)
{
    Mdump_Type_Info out = {0};
    out.type_name = string_make(name);
    out.size = size;
    out.members = members;
    out.members_size = members_size;
    out.align = align_or_zero;
    out.flags = flags;

    return out;
}

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

bool mdump_is_value_type(Mdump_Type type)
{
    return type == mdump_value_type(type);
}
bool mdump_is_array_type(Mdump_Type type)
{
    return ((u32) type | MDUMP_ARRAY_TYPE_BIT) != 0;
}
bool mdump_is_ptr_type(Mdump_Type type)
{
    return ((u32) type | MDUMP_ARRAY_TYPE_BIT) != 0;
}

typedef enum _Mdump_Allocation {
    _MDUMP_ALLOCATE_MEM,
    _MDUMP_ALLOCATE_META,
} _Mdump_Allocation;

Mdump_Ptr _mdump_allocate_recursive(Mdump_File* file, isize size, isize align, void** addr_or_null, _Mdump_Allocation allocation, bool is_retry)
{
    ASSERT(size >= 0 && is_power_of_two(align));
    
    isize* block_i1 = allocation == _MDUMP_ALLOCATE_MEM ? &file->current_block_i1 : &file->current_meta_block_i1; 

    if(*block_i1 <= 0)
    {
        if(file->block_default_size <= sizeof(Mdump_Magic))
            file->block_default_size = 4096;

        if(file->allocator == NULL)
            file->allocator = allocator_get_default();

        Mdump_Block new_block = {0};
        new_block.size = MAX(file->block_default_size, size + (isize) sizeof(Mdump_Magic));
        new_block.data = (u8*) allocator_allocate(file->allocator, new_block.size, MDUMP_BLOCK_ALIGN, SOURCE_INFO());
        new_block.used_to = sizeof(Mdump_Magic);

        if(allocation == _MDUMP_ALLOCATE_MEM)
            *(Mdump_Magic*) new_block.data = MDUMP_MAGIC_BLOCK;
        else
            *(Mdump_Magic*) new_block.data = MDUMP_MAGIC_META;

        array_push(&file->blocks, new_block);
        *block_i1 = file->blocks.size;
        
        //Fill with marker data (0b01010101)
        #ifdef DO_ASSERTS_SLOW
        memset(new_block.data, MDUMP_UNUSED_MEMORY_PATTERN, new_block.size);
        #endif DO_ASSERTS_SLOW
    }

    Mdump_Block* curr_block = &file->blocks.data[*block_i1 - 1];
    
    ASSERT(*block_i1 > 0);
    ASSERT(file->blocks.size > 0);
    ASSERT(curr_block->used_to >= sizeof(Mdump_Magic));
    if(allocation == _MDUMP_ALLOCATE_MEM)
        ASSERT(*(Mdump_Magic*) curr_block->data == MDUMP_MAGIC_BLOCK);
    else
        ASSERT(*(Mdump_Magic*) curr_block->data == MDUMP_MAGIC_META);

    u8* new_data = (u8*) align_forward(curr_block->data + curr_block->used_to, align);
    isize offset = new_data - curr_block->data;
    if(offset + size > curr_block->size)
    {
        ASSERT(is_retry == false);
        *block_i1 = 0;
        return _mdump_allocate_recursive(file, size, align, addr_or_null, allocation, true);
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

    if(addr_or_null)
        *addr_or_null = new_data;
    return out;
}

Mdump_Ptr _mdump_allocate(Mdump_File* file, isize size, isize align, void** addr_or_null, _Mdump_Allocation allocation)
{
    return _mdump_allocate_recursive(file, size, align, addr_or_null, allocation, false);
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

void* mdump_get_array(Mdump_File* file, Mdump_Array array, isize item_size, isize *item_count)
{
    void* out = mdump_get(file, array.ptr, array.size*item_size);
    if(out != NULL)
        *item_count = array.size;
    else
        *item_count = 0;

    return out;
}

void* mdump_get_array_sure(Mdump_File* file, Mdump_Array array, isize item_size)
{
    void* out = mdump_get(file, array.ptr, array.size*item_size);
    ASSERT(out);
    return out;
}

char* mdump_get_string(Mdump_File* file, Mdump_String string)
{
    return (char*) mdump_get(file, string.ptr, string.size);
}

Mdump_Ptr mdump_allocate(Mdump_File* file, isize size, isize align, void** addr_or_null)
{
    return _mdump_allocate(file, size, align, addr_or_null, _MDUMP_ALLOCATE_MEM);
}

Mdump_String mdump_string_allocate(Mdump_File* file, isize size)
{
    ASSERT(size < UINT32_MAX);
    Mdump_String out = {0};
    out.ptr = mdump_allocate(file, size + 1, 1, NULL);
    out.size = (u32) size;
    return out;
}

Mdump_Array mdump_array_allocate(Mdump_File* file, isize size, isize item_size, isize align)
{
    ASSERT(size*item_size < UINT32_MAX);
    Mdump_Array out = {0};
    out.ptr = mdump_allocate(file, size*item_size, align, NULL);
    out.size = (u32) size;
    return out;
}

Mdump_String _mdump_string_from_string(Mdump_File* file, String string, _Mdump_Allocation allocation)
{
    ASSERT(string.size < UINT32_MAX);
    void* alloced_data = NULL; 
    Mdump_String out = {0};
    out.ptr = _mdump_allocate(file, string.size + 1, 1, &alloced_data, allocation);
    out.size = (u32) string.size;
    memcpy(alloced_data, string.data, string.size);
    return out;
}

Mdump_String mdump_string_from_string(Mdump_File* file, String string)
{
    return _mdump_string_from_string(file, string, _MDUMP_ALLOCATE_MEM);
}

String mdump_string_to_string(Mdump_File* file, Mdump_String mstring)
{
    String out = {0};
    out.data = mdump_get_string(file, mstring);
    if(out.data != NULL)
        out.size = mstring.size;

    return out;
}

Mdump_Type mdump_get_type_by_name(Mdump_File* file, String type_name)
{
    if(type_name.size == 0)
        return MDUMP_TYPE_NONE;

    switch(type_name.data[0])
    {
        break; case 'c': 
            if(string_is_equal(type_name, STRING("char"))) return MDUMP_TYPE_CHAR;
            if(string_is_equal(type_name, STRING("comment"))) return MDUMP_TYPE_COMMENT;

        break; case 'r': 
            if(string_is_equal(type_name, STRING("rune"))) return MDUMP_TYPE_RUNE;

        break; case 'b': 
            if(string_is_equal(type_name, STRING("bool"))) return MDUMP_TYPE_BOOL;
            if(string_is_equal(type_name, STRING("blank"))) return MDUMP_TYPE_BLANK;

        break; case 's': 
            if(string_is_equal(type_name, STRING("string"))) return MDUMP_TYPE_STRING;

        break; case 'l': 
            if(string_is_equal(type_name, STRING("long_string"))) return MDUMP_TYPE_LONG_STRING;

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
        isize type_size = 0;
        Mdump_Type_Info_Rep* info_rep = MDUMP_GET_ARRAY(file, file->types, &type_size,  Mdump_Type_Info_Rep);
        String found_name = mdump_string_to_string(file, info_rep->type_name);
        if(string_is_equal(type_name, found_name))
            return (Mdump_Type) ((i32) i + MDUMP_TYPE_MAX_RESERVED + 1);
    }

    return MDUMP_TYPE_NONE;
}

Mdump_Type_Info _mdump_builtin_type_info(const char* name, isize size, isize align)
{
    Mdump_Type_Info out = {0};
    if(name)
        out.type_name = string_make(name);
    out.size = (u32) size;
    out.align = (u32) align;
    return out;
}

Mdump_Type_Info mdump_get_builtin_type_info(Mdump_Type type)
{
    if(mdump_is_array_type(type))
        return _mdump_builtin_type_info("array", sizeof(Mdump_Array), MDUMP_ARRAY_ALIGN);

    if(mdump_is_ptr_type(type))
        return _mdump_builtin_type_info("ptr", sizeof(Mdump_Ptr), MDUMP_PTR_ALIGN);

    switch(type)
    {
        default:
        case MDUMP_TYPE_MAX_RESERVED:
        case MDUMP_TYPE_NONE:           return _mdump_builtin_type_info(NULL, 0, 0);  
        case MDUMP_TYPE_BLANK:          return _mdump_builtin_type_info("blank", 0, 1);  
        case MDUMP_TYPE_STRING:         return _mdump_builtin_type_info("string", sizeof(Mdump_String), MDUMP_ARRAY_ALIGN);
        case MDUMP_TYPE_COMMENT:        return _mdump_builtin_type_info("comment", sizeof(Mdump_String), MDUMP_ARRAY_ALIGN);
        case MDUMP_TYPE_LONG_STRING:    return _mdump_builtin_type_info("long_string", sizeof(Mdump_Long_String), MDUMP_ARRAY_ALIGN);

        case MDUMP_TYPE_CHAR: return _mdump_builtin_type_info("char", 1, 1);
        case MDUMP_TYPE_BOOL: return _mdump_builtin_type_info("bool", 1, 1);
        case MDUMP_TYPE_RUNE: return _mdump_builtin_type_info("rune", 4, 4);

        case MDUMP_TYPE_U8:   return _mdump_builtin_type_info("u8", 1, 1);  
        case MDUMP_TYPE_U16:  return _mdump_builtin_type_info("u16", 2, 2);
        case MDUMP_TYPE_U32:  return _mdump_builtin_type_info("u32", 4, 4);
        case MDUMP_TYPE_U64:  return _mdump_builtin_type_info("u64", 8, 8);
        
        case MDUMP_TYPE_I8:   return _mdump_builtin_type_info("i8", 1, 1);  
        case MDUMP_TYPE_I16:  return _mdump_builtin_type_info("i16", 2, 2);
        case MDUMP_TYPE_I32:  return _mdump_builtin_type_info("i32", 4, 4);
        case MDUMP_TYPE_I64:  return _mdump_builtin_type_info("i64", 8, 8);
            
        case MDUMP_TYPE_F8:   return _mdump_builtin_type_info("f8", 1, 1);  
        case MDUMP_TYPE_F16:  return _mdump_builtin_type_info("f16", 2, 2);
        case MDUMP_TYPE_F32:  return _mdump_builtin_type_info("f32", 4, 4);
        case MDUMP_TYPE_F64:  return _mdump_builtin_type_info("f64", 8, 8);
    }
}

Mdump_Type_Info_Rep mdump_get_type_info(Mdump_File* file, Mdump_Type type, String *name_or_null)
{
    ASSERT(type == mdump_value_type(type));
    Mdump_Type_Info_Rep info_rep = {MDUMP_TYPE_NONE};;
    if(type > MDUMP_TYPE_MAX_RESERVED)
    {
        isize type_index = (isize) type - MDUMP_TYPE_MAX_RESERVED - 1;
        isize infos_size = 0;
        Mdump_Type_Info_Rep* infos = MDUMP_GET_ARRAY(file, file->types, &infos_size, Mdump_Type_Info_Rep);

        if(type_index < file->types.size && type_index < infos_size)
            info_rep = infos[type_index];
    }
    else
    {
        Mdump_Type_Info info = mdump_get_builtin_type_info(type);

        if(info.type_name.data != NULL)
            info_rep.type = type;

        info_rep.align = info.align;
        info_rep.size = info.size;
    }
    
    if(name_or_null)
        *name_or_null = mdump_string_to_string(file, info_rep.type_name);
    return info_rep;
}

Mdump_Type_Info_Rep mdump_get_type_info_by_name(Mdump_File* file, String type_name)
{
    Mdump_Type type = mdump_get_type_by_name(file, type_name);
    return mdump_get_type_info(file, type, NULL);
}

Mdump_Type_Info mdump_get_type_info_from_query(Mdump_File* file, void* query_func)
{
    Mdump_Type_Info info = {0};
    if(query_func != NULL)
    {
        Mdump_Func func = (Mdump_Func) query_func;
        bool state = func(file, NULL, NULL, &info, MDUMP_QUERY_INFO); (void) state;
        ASSERT(state);
    }

    return info;
}
Mdump_Type mdump_validate_type_against(Mdump_File* file, Mdump_Type_Info info, Mdump_Type against);

Mdump_Type mdump_register_type(Mdump_File* file, Mdump_Type_Info info)
{
    Mdump_Type found = mdump_get_type_by_name(file, info.type_name);
    if(found != MDUMP_TYPE_NONE)
        return mdump_validate_type_against(file, info, found);
        
    //@TODO: allocation safepoint
    //@TODO: auto alignment calculations!
    Mdump_Type_Info_Rep info_rep = {MDUMP_TYPE_NONE};
    info_rep.type = (Mdump_Type) (file->types.size + (u32) MDUMP_TYPE_MAX_RESERVED + 1);
    info_rep.size = info.size;
    info_rep.align = info.align;
    info_rep.flags = info.flags;
    info_rep.type_name = _mdump_string_from_string(file, info.type_name, _MDUMP_ALLOCATE_META);
    info_rep.members.ptr = _mdump_allocate(file, info.members_size * sizeof(Mdump_Type_Member_Rep), DEF_ALIGN, NULL, _MDUMP_ALLOCATE_META);
    info_rep.members.size = info.members_size;
    
    Mdump_Type_Info_Rep* types = MDUMP_GET_ARRAY_SURE(file, file->types, Mdump_Type_Info_Rep);
    Mdump_Type_Member_Rep* members_rep = MDUMP_GET_ARRAY_SURE(file, info_rep.members, Mdump_Type_Member_Rep);

    Mdump_Type out = info_rep.type;
    for(isize i = 0; i < info.members_size; i++)
    {
        Mdump_Type_Member_Rep* member_rep = &members_rep[i];
        const Mdump_Type_Member* member = &info.members[i];

        //@TODO: remove stack recursions!
        //@TODO: remove repeated querries
        Mdump_Type_Info member_info = mdump_get_type_info_from_query(file, member->query_func);
        Mdump_Type registered_type = mdump_register_type(file, member_info);

        if(registered_type == MDUMP_TYPE_NONE)
        {
            LOG_ERROR("mdump", "mdump_register_type error: struct member with invlaid dump function\n"
                "type: "STRING_FMT" member: "STRING_FMT, STRING_PRINT(info.type_name), STRING_PRINT(member->name));
            out = MDUMP_TYPE_NONE;
            break;
        }

        if(member->offset + member->size > info.size)
        {
            LOG_ERROR("mdump", "mdump_register_type error: struct member outside struct memory\n"
                "type: "STRING_FMT" member: "STRING_FMT, STRING_PRINT(info.type_name), STRING_PRINT(member->name));
            out = MDUMP_TYPE_NONE;
            break;
        }

        if(member_info.size != member->size)
        {
            LOG_ERROR("mdump", "mdump_register_type error: struct member size does not match its registered size\n"
                "type: "STRING_FMT" member: "STRING_FMT, STRING_PRINT(info.type_name), STRING_PRINT(member->name));
            out = MDUMP_TYPE_NONE;
            break;
        }
        
        if(member->offset % member_info.align != 0 || is_power_of_two(member_info.align) == false)
        {
            LOG_ERROR("mdump", "mdump_register_type error: struct member is missaligned\n"
                "type: "STRING_FMT" member: "STRING_FMT, STRING_PRINT(info.type_name), STRING_PRINT(member->name));
            out = MDUMP_TYPE_NONE;
            break;
        }

        member_rep->type = registered_type;
        member_rep->offset = member->offset;
        member_rep->flags = member->flags;
        member_rep->name = _mdump_string_from_string(file, member->name, _MDUMP_ALLOCATE_META);

        if(member->default_value)
        {
            void* def_addr = NULL;
            member_rep->default_value = _mdump_allocate(file, member->size, member_info.align, &def_addr, _MDUMP_ALLOCATE_META);
            memcpy(def_addr, member->default_value, member->size);
        }
    }
    
    if(out != MDUMP_TYPE_NONE)
    {
        if(file->types.size >= file->type_capacity)
        {
            u32 new_cap = 8;
            while(new_cap <= file->types.size)
                new_cap *= 2;

            void* new_data = NULL;
            Mdump_Ptr new_ptr = _mdump_allocate(file, new_cap * sizeof(Mdump_Type_Info_Rep), DEF_ALIGN, &new_data, _MDUMP_ALLOCATE_META);
            memcpy(new_data, types, file->types.size * sizeof(Mdump_Type_Info_Rep));
        
            file->types.ptr = new_ptr;
            file->type_capacity = new_cap;
            types = (Mdump_Type_Info_Rep*) new_data;
        }
    }

    types[file->types.size ++] = info_rep;
    return out;

}

Mdump_Type mdump_validate_type_against(Mdump_File* file, Mdump_Type_Info info, Mdump_Type against)
{
    String name = {0};
    Mdump_Type_Info_Rep desc_rep = mdump_get_type_info(file, against, &name);
    if(string_is_equal(name, info.type_name)) 
    {
        LOG_ERROR("mdump", "mdump_validate_type_against error: type name does not match found type name\n"
            "type: "STRING_FMT" found type: "STRING_FMT, name, info.type_name);
        return MDUMP_TYPE_NONE;
    }

    if(desc_rep.size != info.size) 
    {
        LOG_ERROR("mdump", "mdump_validate_type_against error: type size does not match one found\n"
            "type: "STRING_FMT, info.type_name);
        return MDUMP_TYPE_NONE;
    }
    
    if(desc_rep.members.size != info.members_size) 
    {
        LOG_ERROR("mdump", "mdump_validate_type_against error: type memebers do not match ones found\n"
            "type: "STRING_FMT, info.type_name);
        return MDUMP_TYPE_NONE;
    }
    
    if(desc_rep.flags != info.flags) 
    {
        LOG_ERROR("mdump", "mdump_validate_type_against error: type flags do not match ones found\n"
            "type: "STRING_FMT, info.type_name);
        return MDUMP_TYPE_NONE;
    }
            
    isize members_size = 0;
    Mdump_Type_Member_Rep* members_rep = MDUMP_GET_ARRAY(file, desc_rep.members, &members_size, Mdump_Type_Member_Rep);
    for(isize i = 0; i < members_size; i++)
    {
        Mdump_Type_Member_Rep* member_rep = &members_rep[i];
        const Mdump_Type_Member* member = &info.members[i];
        
        Mdump_Type_Info member_info = mdump_get_type_info_from_query(file, member->query_func);
            
        String name_rep = mdump_string_to_string(file, member_rep->name);
        if(member_rep->offset != member->offset || member_rep->flags != member->flags ||  member->size != member_info.size || string_is_equal(name_rep, member->name) == false)
        {
            LOG_ERROR("mdump", "mdump_validate_type_against error: type memebers do not match ones found\n"
                "type: "STRING_FMT, info.type_name);
        
            return false;
        }

        bool def_value_okay = true;
        void* def_val = mdump_get(file, member_rep->default_value, member_info.size);
        if((member->default_value == NULL) != (def_val == NULL))
            def_value_okay = false;
                
        //@TODO: There is a problem here that we dont know how to compare types. ANything containing ptr may not be compared with memcmp.
        //       Value type avare comparison!
        if(def_value_okay)
        {
            if(memcmp(def_val, member->default_value, member_info.size) != 0)
                def_value_okay = false;
        }

        if(def_value_okay == false)
        {
            LOG_ERROR("mdump", "mdump_validate_type_against error: type default value does not match the one found.\n"
                "Not that default value can only be used for types that dont contain pointers"
                "type: "STRING_FMT, info.type_name);
                return false;
        }
    }

    return true;
}

bool mdump_validate_type(Mdump_File* file, Mdump_Type_Info info)
{
    Mdump_Type_Info_Rep found = mdump_get_type_info_by_name(file, info.type_name);
    if(found.type == MDUMP_TYPE_NONE)
        return false;

    return mdump_validate_type_against(file, info, found.type);
}



#define _DEFINE_MDUMP_BUILTINT_POD(MDUMP_ENUM_TYPE, type, Type)   \
    EXPORT bool mdump_##type(Mdump_File* file, Type* in_memory, Type* in_file, Mdump_Type_Info* info, Mdump_Action action)  \
    {   \
        (void) file; \
        if(action == MDUMP_WRITE) \
            *in_file = *in_memory; \
        else if(action == MDUMP_READ) \
            *in_memory = *in_file; \
        else if(action == MDUMP_QUERY_INFO) \
            if(info) *info = mdump_get_builtin_type_info(MDUMP_ENUM_TYPE); \
        return true; \
    } \
    
    //MDUMP_TYPE_LONG_STRING,


_DEFINE_MDUMP_BUILTINT_POD(MDUMP_TYPE_CHAR,  char, char)
_DEFINE_MDUMP_BUILTINT_POD(MDUMP_TYPE_RUNE,  rune, i32)
_DEFINE_MDUMP_BUILTINT_POD(MDUMP_TYPE_BOOL,  bool, bool)

_DEFINE_MDUMP_BUILTINT_POD(MDUMP_TYPE_I8,  i8, i8)
_DEFINE_MDUMP_BUILTINT_POD(MDUMP_TYPE_I16, i16, i16)
_DEFINE_MDUMP_BUILTINT_POD(MDUMP_TYPE_I32, i32, i32)
_DEFINE_MDUMP_BUILTINT_POD(MDUMP_TYPE_I64, i64, i64)

_DEFINE_MDUMP_BUILTINT_POD(MDUMP_TYPE_U8,  u8, u8)
_DEFINE_MDUMP_BUILTINT_POD(MDUMP_TYPE_U16, u16, u16)
_DEFINE_MDUMP_BUILTINT_POD(MDUMP_TYPE_U32, u32, u32)
_DEFINE_MDUMP_BUILTINT_POD(MDUMP_TYPE_U64, u64, u64)

_DEFINE_MDUMP_BUILTINT_POD(MDUMP_TYPE_F8,  f8, u8)
_DEFINE_MDUMP_BUILTINT_POD(MDUMP_TYPE_F16, f16, u16)
_DEFINE_MDUMP_BUILTINT_POD(MDUMP_TYPE_F32, f32, f32)
_DEFINE_MDUMP_BUILTINT_POD(MDUMP_TYPE_F64, f64, f64)

EXPORT bool mdump_blank(Mdump_File* file, void* in_memory, void* in_file, Mdump_Type_Info* info, Mdump_Action action)  
{   
    (void) in_file;
    (void) in_memory;
    (void) file;
    if(action == MDUMP_QUERY_INFO) 
        if(info) *info = mdump_get_builtin_type_info(MDUMP_TYPE_BLANK); 
    return true; 
} 

EXPORT bool mdump_string_like(Mdump_File* file, String_Builder* in_memory, Mdump_String* in_file, Mdump_Type_Info* info, Mdump_Action action, Mdump_Type type)
{
    if(action == MDUMP_WRITE)
        *in_file = mdump_string_from_string(file, string_from_builder(*in_memory));
    else if(action == MDUMP_READ)
    {
        String read = mdump_string_to_string(file, *in_file);
        builder_assign(in_memory, read);
    }
    else if(action == MDUMP_QUERY_INFO)
        if(info) *info = mdump_get_builtin_type_info(type);

    return true;
}

EXPORT bool mdump_string(Mdump_File* file, String_Builder* in_memory, Mdump_String* in_file, Mdump_Type_Info* info, Mdump_Action action)
{
    return mdump_string_like(file, in_memory, in_file, info, action, MDUMP_TYPE_STRING);
}

EXPORT bool mdump_comment(Mdump_File* file, String_Builder* in_memory, Mdump_String* in_file, Mdump_Type_Info* info, Mdump_Action action)
{
    return mdump_string_like(file, in_memory, in_file, info, action, MDUMP_TYPE_COMMENT);
}

EXPORT bool mdump_long_string(Mdump_File* file, String_Builder* in_memory, Mdump_Long_String* in_file, Mdump_Type_Info* info, Mdump_Action action)
{
    if(action == MDUMP_WRITE)
    {
        isize chunk_count = DIV_ROUND_UP(in_memory->size, INT32_MAX);

        *in_file = mdump_array_allocate(file, chunk_count, sizeof(Mdump_String), MDUMP_ARRAY_ALIGN);
        Mdump_String* chunks = MDUMP_GET_ARRAY_SURE(file, *in_file, Mdump_String);
        String remnaining_string = string_from_builder(*in_memory);
        for(isize i = 0; i < chunk_count; i++)
        {
            String chunk = string_safe_head(remnaining_string, INT32_MAX);
            remnaining_string = string_safe_tail(remnaining_string, INT32_MAX);
            chunks[i] = mdump_string_from_string(file, chunk);
        }
    }
    else if(action == MDUMP_READ)
    {
        isize chunk_count = 0;
        Mdump_String* chunks = MDUMP_GET_ARRAY(file, *in_file, &chunk_count, Mdump_String);
        isize total_size = 0;

        for(isize i = 0; i < chunk_count; i++)
            total_size += chunks[i].size;

        array_clear(in_memory);
        array_reserve(in_memory, total_size);
        for(isize i = 0; i < chunk_count; i++)
        {
            String read = mdump_string_to_string(file, chunks[i]);
            builder_append(in_memory, read);
        }
    }
    else if(action == MDUMP_QUERY_INFO)
        if(info) *info = mdump_get_builtin_type_info(MDUMP_TYPE_LONG_STRING);

    return true;
}

#define _MDUMP_TYPECHECK_MEMBER(Member_Of_Type, member, mdump_func) \
    (0 ? mdump_func(NULL, NULL, &((Member_Of_Type*)NULL)->member, NULL, MDUMP_QUERY_INFO) : 0)

#define MDUMP_MEMBER(Member_Of_Type, member, mdump_func, def_value_or_null, flags) \
    BRACE_INIT(Mdump_Type_Member){(void*) mdump_func, STRING(#member), offsetof(Member_Of_Type, member), sizeof ((Member_Of_Type*)NULL)->member, flags, (_MDUMP_TYPECHECK_MEMBER(Member_Of_Type, member, mdump_func), def_value_or_null)}
 
 
typedef struct {
    void* array;
    isize item_size;
} Poly_Array;

#define POLY_ARRAY_MAKE(arr) BRACE_INIT(Poly_Array){&(arr), sizeof *(arr).data}

void* mdump_array(Mdump_File* file, Poly_Array* array, Mdump_Array* in_file, Mdump_Type_Info* info, Mdump_Action action)
{
    void* addr = NULL;
    if(action == MDUMP_WRITE)
    {
        u8_Array* base = (u8_Array*) array->array;
        ASSERT(base->size <= UINT32_MAX);
        in_file->ptr = mdump_allocate(file, base->size * array->item_size, DEF_ALIGN, &addr);
        in_file->size = (u32) base->size;
    }
    else if(action == MDUMP_READ)
    {
       _array_resize(array->array, array->item_size, in_file->size, SOURCE_INFO());
       addr = mdump_get(file, in_file->ptr, in_file->size * array->item_size);
    }
    else
    {
        if(action == MDUMP_REGISTER)
            return mdump_register_type(file, new_info);
        else if(action == MDUMP_VALIDATE)
            return mdump_validate_type(file, new_info);
        else if(action == MDUMP_QUERY_INFO)
            *info = new_info;
    }

    return addr;
}


typedef struct Test_Member {
    String_Builder string;
    i64 epoch_time;
} Test_Member; 

typedef struct Mdumped_Test_Member {
    Mdump_String string;
    i64 epoch_time;
} Mdumped_Test_Member; 

EXPORT bool mdump_test_member(Mdump_File* file, Test_Member* in_memory, Mdumped_Test_Member* in_file, Mdump_Type_Info* info, Mdump_Action action)
{
    if(action == MDUMP_WRITE || action == MDUMP_READ)
    {
        mdump_i64(file, &in_memory->epoch_time, &in_file->epoch_time, info, action);
        mdump_string(file, &in_memory->string, &in_file->string, info, action);
    }
    else
    {
        Mdump_Type_Member members[] = {
            MDUMP_MEMBER(Mdumped_Test_Member, epoch_time, mdump_i64, NULL, 0),
            MDUMP_MEMBER(Mdumped_Test_Member, string, mdump_string, NULL, 0),
        };

        Mdump_Type_Info new_info = mdump_type_info_make("Test_Member", sizeof(Mdumped_Test_Member), members, STATIC_ARRAY_SIZE(members), 0, 0);

        if(action == MDUMP_REGISTER)
            return mdump_register_type(file, new_info);
        else if(action == MDUMP_VALIDATE)
            return mdump_validate_type(file, new_info);
        else if(action == MDUMP_QUERY_INFO)
            *info = new_info;
    }
    return true;
}

DEFINE_ARRAY_TYPE(Test_Member, Test_Member_Array);

typedef struct Test_Structure {
    i64 epoch_time;
    i64 a1;
    i64 a2;
    i64 a3;
    Test_Member_Array members;
} Test_Structure; 

typedef struct {
    i64 epoch_time;
    i64 a1;
    i64 a2;
    i64 a3;
    Mdump_Array members;
} Mdumped_Test_Structure;

EXPORT bool mdump_test_structure(Mdump_File* file, Test_Structure* in_memory, Mdumped_Test_Structure* in_file, Mdump_Type_Info* info, Mdump_Action action)
{
    if(action == MDUMP_WRITE || action == MDUMP_READ)
    {
        mdump_i64(file, &in_memory->epoch_time, &in_file->epoch_time, info, action);
        mdump_i64(file, &in_memory->a1, &in_file->a1, info, action);
        mdump_i64(file, &in_memory->a2, &in_file->a2, info, action);
        mdump_i64(file, &in_memory->a3, &in_file->a3, info, action);
    
        Poly_Array poly = POLY_ARRAY_MAKE(in_memory->members);
        Mdumped_Test_Member* members = (Mdumped_Test_Member*) mdump_array(file, &poly, &in_file->members, action);
        for(isize i = 0; i < in_memory->members.size; i++)
            mdump_test_member(file, &in_memory->members.data[i], &members[i], NULL, action);
    }
    else
    {
        i64 def_value = 1000;
        Mdump_Type_Member members[] = {
            MDUMP_MEMBER(Mdumped_Test_Structure, epoch_time, mdump_i64, &def_value, MDUMP_FLAG_OPTIONAL),
            MDUMP_MEMBER(Mdumped_Test_Structure, a1, mdump_i64, NULL, 0),
            MDUMP_MEMBER(Mdumped_Test_Structure, a2, mdump_i64, NULL, 0),
            MDUMP_MEMBER(Mdumped_Test_Structure, a3, mdump_i64, NULL, 0),
            MDUMP_MEMBER(Mdumped_Test_Structure, members, mdump_array, NULL, 0),
        };
        Mdump_Type_Info new_info = mdump_type_info_make("Test_Structure", sizeof(Mdumped_Test_Structure), members, STATIC_ARRAY_SIZE(members), DEF_ALIGN, 0);

        if(action == MDUMP_REGISTER)
            return mdump_register_type(file, new_info);
        else if(action == MDUMP_VALIDATE)
            return mdump_validate_type(file, new_info);
        else if(action == MDUMP_QUERY_INFO)
            *info = new_info;
    }
    return true;
}

//Mdump_Ptr mdump_allocate_dumped(Mdump_File* file, )
//{
//
//}

void test_mdump()
{
    Mdump_File file = {0};

    Test_Structure s = {0};
    s.epoch_time = 1;
    s.a1 = -1;
    s.a2 = -2;
    s.a3 = -3;

    array_resize(&s.members, 10);
    for(isize i = 0; i < s.members.size; i++)
    {
        Test_Member* member = &s.members.data[i];
        member->epoch_time = i;
        member->string = builder_from_string(format_ephemeral("%lli", i), NULL);
    }

    bool res = mdump_test_structure(&file, NULL, NULL, NULL, MDUMP_REGISTER);
    TEST(res);

    Mdumped_Test_Structure* first_ptr = NULL;
    mdump_allocate(&file, sizeof(Mdumped_Test_Structure), DEF_ALIGN, (void**) &first_ptr);
    res = mdump_test_structure(&file, &s, first_ptr, NULL, MDUMP_WRITE);
}
