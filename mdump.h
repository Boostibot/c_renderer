#pragma once

#include "lib/string.h"
#include "lib/log.h"
#include "lib/vformat.h"

typedef union {
    char text[9];
    u64 value;
} Mdump_Magic_64;

#define MDUMP_MAGIC         BINIT(Mdump_Magic_64){"mdump"}.value
#define MDUMP_MAGIC_BLOCK   BINIT(Mdump_Magic_64){"mdumpblk"}.value
#define MDUMP_MAGIC_META    BINIT(Mdump_Magic_64){"mdumpmta"}.value

#define MDUMP_BLOCK_ALIGN    8
#define MDUMP_ARRAY_ALIGN    4
#define MDUMP_PTR_ALIGN      4
#define MDUMP_UNUSED_MEMORY_PATTERN 0x55

//@TODO: Linked list structure?
//@TODO: move Mdump_Type_Info into reserved?
#define MDUMP_FLAG_ARRAY 0x1
#define MDUMP_FLAG_PTR 0x2
#define MDUMP_FLAG_OPTIONAL 0x4 
#define MDUMP_FLAG_ARRAY_MAGIC 0x20
#define MDUMP_FLAG_ARRAY_CHECKSUM 0x40
#define MDUMP_FLAG_REQUIRES_AT_LEAST_ONE_MEMBER 0x80
#define MDUMP_FLAG_REQUIRES_EXACTLY_ONE_MEMBER 0x100
#define MDUMP_FLAG_STRINGIFY_BINARY 0x200
#define MDUMP_FLAG_STRINGIFY_COMMENT 0x400
#define MDUMP_FLAG_STRINGIFY_NONE 0x800
#define MDUMP_FLAG_STRINGIFY_TRANSPARENT 0x8

#define _MDUMP_FLAG_REMOVED_TYPE ((u32) 1 << 31)

typedef enum Mdump_Action {
    MDUMP_READ = 0,
    MDUMP_WRITE = 1,
} Mdump_Action;

typedef enum Mdump_Type {
    MDUMP_TYPE_NONE = 0,
    MDUMP_TYPE_BLANK,
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

    MDUMP_TYPE_MAX_RESERVED = 32,
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
    isize size;
    isize used_to;
    u8* data;
} Mdump_Block;

typedef Array(Mdump_Block) Mdump_Block_Array;

typedef struct Mdump_Type_Member Mdump_Type_Member;

typedef Array(Mdump_Type_Member) Mdump_Type_Member_Array;

typedef struct Mdump_Type_Info {
    String_Builder type_name;
    u32 size;
    u32 align;
    u32 flags;
    b32 is_referenced_only;
    Mdump_Type_Member_Array members;
} Mdump_Type_Info;

typedef Array(Mdump_Type_Info) Mdump_Type_Info_Array;

typedef struct {
    Allocator* allocator;
    Mdump_Type_Info_Array types;
} Mdump_Types;

//@TODO: block offsets and combining!
typedef struct {
    Allocator* block_allocator;
    Mdump_Block_Array blocks;
    isize current_block_i1;
    isize block_default_size;

    u64 magic_number;
} Mdump_Blocks;

typedef Mdump_Type_Info (*Mdump_Info_Func)(Mdump_Types* types);

typedef struct Mdump_Type_Member {
    String_Builder name;
    Mdump_Type type;

    u32 offset;
    u32 size;
    u32 flags;

    String_Builder default_value;
    Mdump_Info_Func info_func;
} Mdump_Type_Member;

#define MDUMP_GET(blocks, ptr, ok_or_null_ptr, Type)    ((Type*) mdump_get((blocks), (ptr), sizeof(Type), (ok_or_null_ptr))
#define MDUMP_GET_ARRAY(blocks, arr, size_ptr, Type)    ((Type*) mdump_get_array((blocks), (arr), sizeof(Type), (size_ptr)))
#define MDUMP_GET_ARRAY_SURE(blocks, arr, Type)         ((Type*) mdump_get_array_sure((blocks), (arr), sizeof(Type)))

void _mdump_check_overwrites(Mdump_Blocks* blocks)
{
    ASSERT(blocks->current_block_i1 >= 0);
    if(blocks->current_block_i1 == 0)
        return;
    
    ASSERT(blocks->blocks.size > 0);
    Mdump_Block* curr_block = &blocks->blocks.data[blocks->current_block_i1 - 1];
    ASSERT(curr_block->used_to >= sizeof(Mdump_Magic));
    ASSERT(*(Mdump_Magic*) curr_block->data == blocks->magic_number);

    #ifdef DO_ASSERTS_SLOW
        for(isize i = curr_block->used_to; i < curr_block->size; i++)
        {
            ASSERT_SLOW(curr_block->data[i] == MDUMP_UNUSED_MEMORY_PATTERN, 
                "Unused memory was found to be corrupeted. Check all functions for overrides");
        }
    #endif // DO_ASSERTS_SLOW
}

Mdump_Ptr _mdump_allocate_recursive(Mdump_Blocks* blocks, isize size, isize align, void** addr_or_null, bool is_retry)
{
    ASSERT(size >= 0 && is_power_of_two(align));
    
    if(blocks->current_block_i1 <= 0)
    {
        if(blocks->block_default_size <= sizeof(Mdump_Magic))
            blocks->block_default_size = 4096;

        if(blocks->block_allocator == NULL)
            blocks->block_allocator = allocator_get_default();
            
        if(blocks->magic_number == 0)
            blocks->magic_number = MDUMP_MAGIC;

        Mdump_Block new_block = {0};
        new_block.size = MAX(blocks->block_default_size, size + (isize) sizeof(Mdump_Magic));
        new_block.data = (u8*) allocator_allocate(blocks->block_allocator, new_block.size, MDUMP_BLOCK_ALIGN);
        new_block.used_to = sizeof(Mdump_Magic);
        
        //Fill with marker data (0b01010101)
        #ifdef DO_ASSERTS_SLOW
        memset(new_block.data, MDUMP_UNUSED_MEMORY_PATTERN, new_block.size);
        #endif DO_ASSERTS_SLOW

        *(Mdump_Magic*) new_block.data = blocks->magic_number;

        array_push(&blocks->blocks, new_block);
        blocks->current_block_i1 = blocks->blocks.size;
    }

    Mdump_Block* curr_block = &blocks->blocks.data[blocks->current_block_i1 - 1];
    _mdump_check_overwrites(blocks);

    u8* new_data = (u8*) align_forward(curr_block->data + curr_block->used_to, align);
    isize offset = new_data - curr_block->data;
    if(offset + size > curr_block->size)
    {
        ASSERT(is_retry == false);
        blocks->current_block_i1 = 0;
        return _mdump_allocate_recursive(blocks, size, align, addr_or_null, true);
    }

    curr_block->used_to = offset + size;
    memset(new_data, 0, size);

    Mdump_Ptr out = {0};
    out.block = (u32) blocks->current_block_i1 - 1;
    out.offset = (u32) offset;

    if(addr_or_null)
        *addr_or_null = new_data;
    return out;
}

void* mdump_get(Mdump_Blocks* blocks, Mdump_Ptr ptr, isize size, bool* ok_or_null)
{
    ASSERT(size >= 0);
    void* out = NULL;
    bool state = true;
    do {
        if(ptr.offset >= sizeof(Mdump_Magic))
        {
            if(ptr.block < blocks->blocks.size)
            {
                Mdump_Block* block = &blocks->blocks.data[ptr.block];
                if(ptr.offset + size <= block->size)
                {
                    out = block->data + ptr.offset;
                    break;
                }
            }

            state = false;
        }
    } while(false);
    
    if(ok_or_null)
        *ok_or_null = state;
    return out;
}

void* mdump_get_array(Mdump_Blocks* blocks, Mdump_Array array, isize item_size, isize *item_count)
{
    void* out = mdump_get(blocks, array.ptr, array.size*item_size, NULL);
    if(out != NULL)
        *item_count = array.size;
    else
        *item_count = 0;

    return out;
}

void* mdump_get_array_sure(Mdump_Blocks* blocks, Mdump_Array array, isize item_size)
{
    bool ok = false;
    void* out = mdump_get(blocks, array.ptr, array.size*item_size, &ok);
    ASSERT(ok);
    return out;
}

Mdump_Ptr mdump_allocate(Mdump_Blocks* blocks, isize size, isize align, void** addr_or_null)
{
    return _mdump_allocate_recursive(blocks, size, align, addr_or_null, false);
}

Mdump_String mdump_string_allocate(Mdump_Blocks* blocks, isize size)
{
    ASSERT(size < UINT32_MAX);
    Mdump_String out = {0};
    out.ptr = mdump_allocate(blocks, size + 1, 1, NULL);
    out.size = (u32) size;
    return out;
}

Mdump_Array mdump_array_allocate(Mdump_Blocks* blocks, isize size, isize item_size, isize align)
{
    ASSERT(size*item_size < UINT32_MAX);
    Mdump_Array out = {0};
    out.ptr = mdump_allocate(blocks, size*item_size, align, NULL);
    out.size = (u32) size;
    return out;
}

Mdump_String mdump_string_from_string(Mdump_Blocks* blocks, String string)
{
    ASSERT(string.size < UINT32_MAX);
    void* alloced_data = NULL; 
    Mdump_String out = {0};
    out.ptr = mdump_allocate(blocks, string.size + 1, 1, &alloced_data);
    out.size = (u32) string.size;
    memcpy(alloced_data, string.data, string.size);
    return out;
}

String mdump_string_to_string(Mdump_Blocks* blocks, Mdump_String mstring, bool* ok_or_null)
{
    String out = {0};
    out.data = (const char*) mdump_get_array(blocks, mstring, 1, &out.size);
    if(ok_or_null)
        *ok_or_null = out.size == mstring.size;

    return out;
}

void mdump_types_deinit(Mdump_Types* types)
{
    //ASSERT(false);
    (void) types;
}

void mdump_types_init(Mdump_Types* types, Allocator* alloc)
{
    mdump_types_deinit(types);
    types->allocator = alloc;
    if(types->allocator == NULL)
        types->allocator = allocator_get_default();
    
    typedef struct Builtin_Type_Info {
        const char* name;
        Mdump_Type type;
        u32 size; 
        u32 align;
        u32 flags;
    } Builtin_Type_Info;

    Builtin_Type_Info infos[] = {
        {"none", MDUMP_TYPE_NONE, 0, 1}, 
        {"blank", MDUMP_TYPE_BLANK, 0, 1}, 
        
        {"string", MDUMP_TYPE_STRING, sizeof(Mdump_String), MDUMP_ARRAY_ALIGN}, 
        {"long_string", MDUMP_TYPE_LONG_STRING, sizeof(Mdump_Long_String), MDUMP_ARRAY_ALIGN}, 

        {"char", MDUMP_TYPE_CHAR, 1}, 
        {"bool", MDUMP_TYPE_BOOL, 1}, 
        {"rune", MDUMP_TYPE_RUNE, 4}, 

        {"u8", MDUMP_TYPE_U8, 1}, 
        {"u16", MDUMP_TYPE_U16, 2}, 
        {"u32", MDUMP_TYPE_U32, 4}, 
        {"u64", MDUMP_TYPE_U64, 8},
        
        {"i8", MDUMP_TYPE_I8, 1}, 
        {"i16", MDUMP_TYPE_I16, 2}, 
        {"i32", MDUMP_TYPE_I32, 4}, 
        {"i64", MDUMP_TYPE_I64, 8},
        
        {"f8", MDUMP_TYPE_F8, 1}, 
        {"f16", MDUMP_TYPE_F16, 2}, 
        {"f32", MDUMP_TYPE_F32, 4}, 
        {"f64", MDUMP_TYPE_F64, 8},
    };
    
    array_resize(&types->types, MDUMP_TYPE_MAX_RESERVED);
    for(isize i = 0; i < ARRAY_SIZE(infos); i++)
    {
        Builtin_Type_Info builtin_info = infos[i];
        Mdump_Type_Info* info = &types->types.data[builtin_info.type];

        info->type_name = builder_from_cstring(types->allocator, builtin_info.name);
        info->size = builtin_info.size;
        info->align = builtin_info.align;
        info->flags = builtin_info.flags;
        if(info->align == 0)
            info->align = info->size;
    }
}

Mdump_Type mdump_get_type_by_name(Mdump_Types* types, String type_name)
{
    ASSERT(types->allocator != NULL);
    for(isize i = 0; i < types->types.size; i++)
    {
        Mdump_Type_Info* type = &types->types.data[i];
        if(string_is_equal(type->type_name.string, type_name))
            return (Mdump_Type) i;
    }

    return MDUMP_TYPE_NONE;
}

Mdump_Type_Info* mdump_get_type_info(Mdump_Types* types, Mdump_Type type)
{
    ASSERT(types->allocator != NULL);
    
    if(type <= 0 || type >= types->types.size)
        type = MDUMP_TYPE_NONE;

    return &types->types.data[type];
}

Mdump_Type mdump_validate_type_against(Mdump_Types* types, Mdump_Type_Info info, Mdump_Type against);

Mdump_Type mdump_register_type(Mdump_Types* types, Mdump_Type_Info info)
{
    LOG_DEBUG("mdump", "registering type: %.*s", STRING_PRINT(info.type_name));
    log_indent();

    Mdump_Type out_type = MDUMP_TYPE_NONE;
    Mdump_Type found = mdump_get_type_by_name(types, info.type_name.string);
    if(found != MDUMP_TYPE_NONE)
        out_type = mdump_validate_type_against(types, info, found);
    else
    {
        isize i = 0;
        for(; i < info.members.size; i++)
        {
            Mdump_Type_Member* member = &info.members.data[i];

            Mdump_Type_Info member_info = {0};
            if(member->info_func != NULL)
                member_info = member->info_func(types);

            Mdump_Type registered_type = mdump_register_type(types, member_info);

            u32 member_size = member_info.size;
            u32 member_align = member_info.align;
            if(member->flags & MDUMP_FLAG_ARRAY)
            {
                member_size = sizeof(Mdump_Array);
                member_align = MDUMP_ARRAY_ALIGN;
            }
            else if(member->flags & MDUMP_FLAG_PTR)
            {
                member_size = sizeof(Mdump_Ptr);
                member_align = MDUMP_PTR_ALIGN;
            }

            if(registered_type == MDUMP_TYPE_NONE)
            {
                LOG_ERROR("mdump", "mdump_register_type error: struct member with invlaid dump function\n"
                    "type: %s member: %s", info.type_name.data, member->name.data);
                break;
            }

            if(member->offset + member->size > info.size)
            {
                LOG_ERROR("mdump", "mdump_register_type error: struct member outside struct memory\n"
                    "type: %s member: %s", info.type_name.data, member->name.data);
                break;
            }

            if(member_size != member->size)
            {
                LOG_ERROR("mdump", "mdump_register_type error: struct member size does not match its registered size\n"
                    "type: %s member: %s", info.type_name.data, member->name.data);
                break;
            }
        
            if(member->offset % member_align != 0 || is_power_of_two(member_align) == false)
            {
                LOG_ERROR("mdump", "mdump_register_type error: struct member is missaligned\n"
                    "type: %s member: %s", info.type_name.data, member->name.data);
                break;
            }

            member->type = registered_type;
        }
    
        if(i == info.members.size)
        {
            array_push(&types->types, info);
            out_type = (Mdump_Type)(types->types.size - 1);
        }
    }
    
    log_outdent();
    return out_type;
}

Mdump_Type mdump_validate_type_against(Mdump_Types* types, Mdump_Type_Info info, Mdump_Type against)
{
    LOG_DEBUG("mdump", "validating type: %.*s", STRING_PRINT(info.type_name));

    Mdump_Type_Info* found_info = mdump_get_type_info(types, against);
    if(builder_is_equal(found_info->type_name, info.type_name) == false) 
    {
        LOG_ERROR("mdump", "mdump_validate_type_against error: type name does not match found type name\n"
            "type: %.*s found type: %.*s", STRING_PRINT(found_info->type_name), STRING_PRINT(info.type_name));
        return MDUMP_TYPE_NONE;
    }

    if(found_info->size != info.size) 
    {
        LOG_ERROR("mdump", "mdump_validate_type_against error: type size does not match one found\n"
            "type: %.*s", STRING_PRINT(info.type_name));
        return MDUMP_TYPE_NONE;
    }
    
    if(found_info->members.size != info.members.size) 
    {
        LOG_ERROR("mdump", "mdump_validate_type_against error: type memebers do not match ones found\n"
            "type: %.*s", STRING_PRINT(info.type_name));
        return MDUMP_TYPE_NONE;
    }
    
    if(found_info->flags != info.flags) 
    {
        LOG_ERROR("mdump", "mdump_validate_type_against error: type flags do not match ones found\n"
            "type: %.*s", STRING_PRINT(info.type_name));
        return MDUMP_TYPE_NONE;
    }
            
    for(isize i = 0; i < info.members.size; i++)
    {
        Mdump_Type_Member* member = &info.members.data[i];
        Mdump_Type_Member* found_member = &found_info->members.data[i];
        
        Mdump_Type_Info member_info = member->info_func(types);
            
        if(found_member->offset != member->offset || found_member->flags != member->flags ||  member->size != member_info.size || builder_is_equal(found_member->name, member->name) == false)
        {
            LOG_ERROR("mdump", "mdump_validate_type_against error: type memebers do not match ones found\n"
                "type: %.*s", STRING_PRINT(info.type_name));
            return MDUMP_TYPE_NONE;
        }

        if(builder_is_equal(member->default_value, found_member->default_value) == false)
        {
            LOG_ERROR("mdump", "mdump_validate_type_against error: type default value does not match the one found.\n"
                "Not that default value can only be used for types that dont contain pointers"
                "type: %.*s", STRING_PRINT(info.type_name));
            return MDUMP_TYPE_NONE;
        }
    }

    return against;
}

Mdump_Type_Info mdump_get_builtin_type_info(Mdump_Types* types, Mdump_Type type)
{
    Mdump_Type_Info* info = mdump_get_type_info(types, type);
    Mdump_Type_Info out = {0};
    out.type_name = builder_from_string(types->allocator, info->type_name.string);
    out.size = info->size;
    out.align = info->align;
    out.flags = info->flags;
    return out;
}

#define _DEFINE_MDUMP_BUILTINT_POD(MDUMP_ENUM_TYPE, type, Type)   \
    EXTERNAL void mdump_##type(Mdump_Blocks* blocks, Type* in_memory, Type* in_file, Mdump_Action action)  \
    {   \
        (void) blocks; \
        if(action == MDUMP_READ) \
            *in_memory = *in_file; \
        else \
            *in_file = *in_memory; \
    } \
    \
    EXTERNAL Mdump_Type_Info mdump_info_##type(Mdump_Types* types) \
    {   \
        return mdump_get_builtin_type_info(types, MDUMP_ENUM_TYPE);  \
    }   \

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

EXTERNAL Mdump_Type_Info mdump_info_blank(Mdump_Types* types)
{
    return mdump_get_builtin_type_info(types, MDUMP_TYPE_BLANK);
}

EXTERNAL void mdump_blank(Mdump_Blocks* blocks, void* in_memory, void* in_file, Mdump_Action action)  
{   
    (void) in_file;
    (void) in_memory;
    (void) blocks;
    (void) action;
} 

EXTERNAL bool mdump_string(Mdump_Blocks* blocks, String_Builder* in_memory, Mdump_String* in_file, Mdump_Action action)
{
    bool ok = true;
    if(action == MDUMP_READ)
    {
        String read = mdump_string_to_string(blocks, *in_file, &ok);
        builder_assign(in_memory, read);
    }
    else
        *in_file = mdump_string_from_string(blocks, in_memory->string);

    return ok;
}

EXTERNAL Mdump_Type_Info mdump_info_string(Mdump_Types* types)
{
    return mdump_get_builtin_type_info(types, MDUMP_TYPE_STRING);
}

EXTERNAL bool mdump_long_string(Mdump_Blocks* blocks, String_Builder* in_memory, Mdump_Long_String* in_file, Mdump_Action action)
{
    bool ok = true;
    if(action == MDUMP_READ)
    {
        isize chunk_count = 0;
        Mdump_String* chunks = MDUMP_GET_ARRAY(blocks, *in_file, &chunk_count, Mdump_String);
        if(chunk_count != in_file->size)
            ok = false;

        isize total_size = 0;

        for(isize i = 0; i < chunk_count; i++)
            total_size += chunks[i].size;

        builder_clear(in_memory);
        builder_reserve(in_memory, total_size);
        for(isize i = 0; i < chunk_count && ok; i++)
        {
            String read = mdump_string_to_string(blocks, chunks[i], &ok);
            builder_append(in_memory, read);
        }
    }
    else
    {
        isize chunk_count = DIV_CEIL(in_memory->size, INT32_MAX);

        *in_file = mdump_array_allocate(blocks, chunk_count, sizeof(Mdump_String), MDUMP_ARRAY_ALIGN);
        Mdump_String* chunks = MDUMP_GET_ARRAY_SURE(blocks, *in_file, Mdump_String);
        String remnaining_string = in_memory->string;
        for(isize i = 0; i < chunk_count; i++)
        {
            String chunk = string_safe_head(remnaining_string, INT32_MAX);
            remnaining_string = string_safe_tail(remnaining_string, INT32_MAX);
            chunks[i] = mdump_string_from_string(blocks, chunk);
        }
    }

    return ok;
}

EXTERNAL Mdump_Type_Info mdump_info_long_string(Mdump_Types* types)
{
    return mdump_get_builtin_type_info(types, MDUMP_TYPE_LONG_STRING);
}

bool mdump_array(Mdump_Blocks* blocks, void* array, isize item_size, Mdump_Array* in_file, Mdump_Action action, void** out_address_or_null, u32 flags)
{
    (void) flags;
    void* addr = NULL;
    bool state = true;
    if(action == MDUMP_READ)
    {
        u8_Array* base = (u8_Array*) array;
        ASSERT(base->size <= UINT32_MAX);
        in_file->ptr = mdump_allocate(blocks, base->size * item_size, DEF_ALIGN, &addr);
        in_file->size = (u32) base->size;
    }
    else
    {
       isize item_count = 0;
       addr = mdump_get_array(blocks, *in_file, item_size, &item_count);
       _array_resize((Generic_Array*) array, item_size, item_count, true);
       state = item_count == in_file->size;
    }

    if(out_address_or_null)
        *out_address_or_null = addr;
    return state;
}

EXTERNAL Mdump_Type_Info mdump_type_info_make(Mdump_Types* types, const char* name, u32 size, u32 align, u32 flags)
{
    ASSERT(types->allocator != NULL);

    Mdump_Type_Info out = {0};
    array_init(&out.members, types->allocator);

    out.type_name = builder_from_cstring(types->allocator, name);
    out.align = align;
    out.size = size;
    out.flags = flags;
    return out;
}

EXTERNAL void _mdump_type_member_push(Mdump_Type_Info* parent, Mdump_Info_Func info_func, const char* name, u32 offset, u32 size, u32 flags, String default_value)
{
    Mdump_Type_Member out = {0};
    Allocator* alloc = parent->members.allocator;
    ASSERT(alloc, "Parent must already be filled!");

    out.name = builder_from_cstring(alloc, name);
    out.offset = offset;
    out.size = size;
    out.flags = flags;
    out.info_func = info_func;

    if(default_value.size != 0)
        out.default_value = builder_from_string(alloc, default_value);

    array_push(&parent->members, out);
}

#define mdump_type_member_push_optional(Member_Of_Type, info_ptr, member, mdump_func, flags, def_value) \
    _mdump_type_member_push((info_ptr), (mdump_func), #member, offsetof(Member_Of_Type, member), sizeof ((Member_Of_Type*)NULL)->member, (flags) | MDUMP_FLAG_OPTIONAL, BINIT(String){(const char*) def_value, sizeof *def_value})

#define mdump_type_member_push(Member_Of_Type, info_ptr, member, mdump_func, flags) \
    _mdump_type_member_push((info_ptr), (mdump_func), #member, offsetof(Member_Of_Type, member), sizeof ((Member_Of_Type*)NULL)->member, (flags), STRING(""))
    
typedef struct {
    Mdump_Type type;
    u32 offset;
    u32 size;
    u32 flags;
    Mdump_String name;
    Mdump_String default_value;
} Mdump_Type_Member_Mdumped;

typedef struct {
    Mdump_Type type;
    u32 size;
    u32 align;
    u32 flags;
    Mdump_String type_name;
    Mdump_Array members;
} Mdump_Type_Info_Mdumped;

EXTERNAL Mdump_Type_Info mdump_info_type_member(Mdump_Types* types)
{
    Mdump_Type_Info info = mdump_type_info_make(types, "Mdump_Type_Member", sizeof(Mdump_Type_Member_Mdumped), DEF_ALIGN, 0);

    mdump_type_member_push(Mdump_Type_Member_Mdumped, &info, name, mdump_info_string, 0);
    mdump_type_member_push(Mdump_Type_Member_Mdumped, &info, offset, mdump_info_u32, 0);
    mdump_type_member_push(Mdump_Type_Member_Mdumped, &info, size, mdump_info_u32, 0);
    mdump_type_member_push(Mdump_Type_Member_Mdumped, &info, flags, mdump_info_u32, 0);
    mdump_type_member_push(Mdump_Type_Member_Mdumped, &info, default_value, mdump_info_string, MDUMP_FLAG_STRINGIFY_BINARY); 

    return info;
}

EXTERNAL bool mdump_type_member(Mdump_Blocks* blocks, Mdump_Type_Member* in_memory, Mdump_Type_Member_Mdumped* in_file, Mdump_Action action)
{
    mdump_u32(blocks, &in_memory->offset, &in_file->offset, action);
    mdump_u32(blocks, &in_memory->size, &in_file->size, action);
    mdump_u32(blocks, &in_memory->flags, &in_file->flags, action);
    
    bool state = true
        && mdump_string(blocks, &in_memory->name, &in_file->name, action)
        && mdump_string(blocks, &in_memory->default_value, &in_file->default_value, action);

    return state;
}

EXTERNAL Mdump_Type_Info mdump_info_type_info(Mdump_Types* types)
{
    Mdump_Type_Info info = mdump_type_info_make(types, "Mdump_Type_Info", sizeof(Mdump_Type_Info_Mdumped), DEF_ALIGN, 0);

    mdump_type_member_push(Mdump_Type_Info_Mdumped, &info, type_name, mdump_info_string, 0);
    mdump_type_member_push(Mdump_Type_Info_Mdumped, &info, size, mdump_info_u32, 0);
    mdump_type_member_push(Mdump_Type_Info_Mdumped, &info, align, mdump_info_u32, 0);
    mdump_type_member_push(Mdump_Type_Info_Mdumped, &info, flags, mdump_info_u32, 0);
    mdump_type_member_push(Mdump_Type_Info_Mdumped, &info, members, mdump_info_type_member, MDUMP_FLAG_ARRAY);
    
    return info;
}

EXTERNAL bool mdump_type_info(Mdump_Blocks* blocks, Mdump_Type_Info* in_memory, Mdump_Type_Info_Mdumped* in_file, Mdump_Action action)
{
    mdump_u32(blocks, &in_memory->align, &in_file->align, action);
    mdump_u32(blocks, &in_memory->size, &in_file->size, action);
    mdump_u32(blocks, &in_memory->flags, &in_file->flags, action);
    
    bool state = mdump_string(blocks, &in_memory->type_name, &in_file->type_name, action);

    Mdump_Type_Member_Mdumped* dumped_members = NULL;
    state = state && mdump_array(blocks, &in_memory->members, sizeof *in_memory->members.data, &in_file->members, action, (void**) &dumped_members, 0);
    for(isize i = 0; i < in_memory->members.size && state; i++)
        state = mdump_type_member(blocks, &in_memory->members.data[i], dumped_members, action);

    return state;
}

void mdump_print_type_hierarchy(Mdump_Types* types)
{
    LOG_INFO("mdump", "mdump types:");
    for(isize j = 0; j < types->types.size; j++)
    {
        Mdump_Type_Info type = types->types.data[j];
        if(type.type_name.size == 0)
            continue;

        if(type.members.size > 0)
        {
            LOG_INFO(">mdump", "%s (size: %lli align: %lli) {", type.type_name.data, (lli) type.size, (lli) type.align);
            for(isize i = 0; i < type.members.size; i++)
            {
                Mdump_Type_Member member = type.members.data[i];
                Mdump_Type_Info* member_info = mdump_get_type_info(types, member.type);
                const char* decoration = "";
                if(member.flags & MDUMP_FLAG_ARRAY)
                    decoration = "[]";
                else if(member.flags & MDUMP_FLAG_PTR)
                    decoration = "*";
                LOG_INFO(">>mdump", "%s: %s%s (offset: %lli)", member.name.data, member_info->type_name.data, decoration, (lli) member.offset);
            }
            LOG_INFO(">mdump", "}");
        }
        else
        {
            LOG_INFO(">mdump", "%s (size: %lli align: %lli)", type.type_name.data, (lli) type.size, (lli) type.align);
        }
    }
}

void test_mdump()
{
    Mdump_Blocks blocks = {0};
    Mdump_Types types = {0};

    mdump_types_init(&types, NULL);

    Mdump_Type_Info info = mdump_info_type_info(&types);
    Mdump_Type type = mdump_register_type(&types, info);
    TEST(type != MDUMP_TYPE_NONE);
    mdump_print_type_hierarchy(&types);

    Mdump_Type_Info_Mdumped* written = NULL;
    mdump_allocate(&blocks, sizeof *written, DEF_ALIGN, (void**) &written);
    TEST(mdump_type_info(&blocks, &info, written, MDUMP_WRITE));

    Mdump_Type_Info read = {0};
    TEST(mdump_type_info(&blocks, &read, written, MDUMP_READ));

    TEST(read.align == info.align);
    TEST(read.size == info.size);
    TEST(read.flags == info.flags);
    TEST(read.members.size == info.members.size);
    TEST(builder_is_equal(read.type_name, info.type_name));
}
