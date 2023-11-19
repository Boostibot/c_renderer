#pragma once

#include "lib/string.h"
#include "lib/serialize.h"

typedef enum Mdump_Type {
    MDUMP_UNASSIGNED = 0, //Invalid type

    MDUMP_BLANK,         //Zero sized type. Should be translated into json or similar as blank space used for separation and readability
    MDUMP_STRING,        //Null terminated string with size (size does not include null termination)
    MDUMP_COMMENT,       //structure same as MDUMP_STRING. Its value should be used for comments
    MDUMP_BINARY,        //Ptr and size to some uknown data

    MDUMP_BOOL,
    MDUMP_CHAR,
    MDUMP_RUNE,     //Single unicode codepoint (32 bits)
    MDUMP_PTR,      //8 byte value holding address within Mdump_File (works for both disk and memroy representation)

    MDUMP_U8,
    MDUMP_U16,
    MDUMP_U32,
    MDUMP_U64,
    
    MDUMP_I8,
    MDUMP_I16,
    MDUMP_I32,
    MDUMP_I64,

    MDUMP_F8,
    MDUMP_F16,
    MDUMP_F32,
    MDUMP_F64,

    MDUMP_RESERVED_MAX = 64,
} Mdump_Type;

typedef enum Mdump_Action {
    MDUMP_READ = 0,
    MDUMP_WRITE = 1,
    MDUMP_VERIFY = 2,
    MDUMP_REGISTER = 3,
    MDUMP_QUERY = 3,
} Mdump_Action;

typedef struct Mdump_Ptr {
    u32 block;
    u32 offset;
} Mdump_Ptr;

typedef union {
    char text[9];
    u64 val;
} _Mdump_Magic;

#define MDUMP_ARRAY_TYPE_BIT    (1 << 31)
#define MDUMP_MAGIC_FILE        BRACE_INIT(_Mdump_Magic){"mdump"}.val
#define MDUMP_MAGIC_BLOCK       BRACE_INIT(_Mdump_Magic){"mdumpblk"}.val
#define MDUMP_MAGIC_TYPE_BLOCK  BRACE_INIT(_Mdump_Magic){"mdumptyp"}.val

#define MDUMP_FLAG_NEEDS_AT_LEAST_ONE_MEMBER 1
#define MDUMP_FLAG_NEEDS_EXACTLY_ONE_MEMBER  2
#define MDUMP_FLAG_OPTIONAL                  4
#define MDUMP_FLAG_TRANSPARENT               8
#define MDUMP_FLAG_CHECKSUM                  16

typedef struct Mdump_Array {
    u32 size;
    bool is_address;
    union {
        Mdump_Ptr ptr;
        void* address;
    };
} Mdump_Array;

typedef struct Mdump_Block_Internal {
    u64 magic;
} Mdump_Block_Internal;

typedef struct Mdump_File_Internal {
    u64 magic_file;
    i64 dump_creation_epoch_time;
    u32 dump_version;
    Mdump_Array dump_namespace;
    Mdump_Array dump_description;

    Mdump_Array types;
    
    Mdump_Ptr root;
    Mdump_Type root_type;

    u64 magic_block;
    u32 block_count;
} Mdump_File_Internal;

typedef struct Mdump_Block {
    u32 size;
    u32 used_to;
    Mdump_Block_Internal* address;
} Mdump_Block;

typedef struct Mdump_File {
    Mdump_File_Internal info;
    Allocator* alloc;
    u32 type_capacity;
    u32 new_block_size;
    u32 new_block_align;

    u32 blocks_count;
    u32 blocks_capacity;
    u32 free_block_i1;
    Mdump_Block* blocks;
} Mdump_File;

//@TODO: maybe include size of type to increase verification 
typedef struct Mdump_Type_Member {
    String name;
    Mdump_Type type;
    u32 offset;
    const void* default_value;
    u32 flags;
} Mdump_Type_Member;

typedef struct Mdump_Type_Description {
    String type_name;
    u32 size;
    const Mdump_Type_Member* members;
    u32 members_size;
    u32 flags;
} Mdump_Type_Description;

typedef struct Mdump_Type_Member_Rep {
    Mdump_Type type;
    Mdump_Ptr default_value;
    u32 offset;
    Mdump_Array name;
    u32 flags;
} Mdump_Type_Member_Rep;

typedef struct Mdump_Type_Description_Rep {
    Mdump_Type type;
    u32 size;
    Mdump_Array type_name;
    Mdump_Array members;
    u32 flags;
} Mdump_Type_Description_Rep;


Mdump_Ptr _mdump_allocate(Mdump_File* file, isize size, isize align, void** addr, bool second_try)
{
    Mdump_Ptr out = {0};
    ASSERT((file->blocks_capacity == 0) == (file->blocks == NULL));
    if(file->free_block_i1 <= 0)
    {
        if(file->alloc == NULL)
            file->alloc = allocator_get_default();

        if(file->blocks_capacity <= file->blocks_count)
        {
            isize new_size = 32;
            isize big_at_least = file->blocks_count;
            if(big_at_least < size)
                big_at_least = size;

            while(new_size <= file->blocks_count)
                new_size *= 2;

            Mdump_Block* new_data  = (Mdump_Block*) allocator_reallocate(file->alloc, new_size*sizeof(Mdump_Block), file->blocks, file->blocks_capacity*sizeof(Mdump_Block), DEF_ALIGN, SOURCE_INFO());
            memset(new_data + file->blocks_capacity, 0, (new_size - file->blocks_capacity) * sizeof(Mdump_Block));
            file->blocks = new_data;
            file->blocks_capacity = new_size;
        }        

        if(file->new_block_align < 8)
            file->new_block_align = 64;

        if(file->new_block_size <= sizeof(Mdump_Block_Internal))
            file->new_block_size = 4096*4;

        ASSERT(file->blocks_count < file->blocks_capacity);
        ASSERT(file->new_block_size > sizeof(Mdump_Block_Internal));

        Mdump_Block* added = &file->blocks[file->blocks_count++];
        added->size = file->new_block_size;
        added->address = (Mdump_Block_Internal*) allocator_allocate(file->alloc, added->size*sizeof(Mdump_Block), file->new_block_align, SOURCE_INFO());
        added->used_to = sizeof(Mdump_Block_Internal);
        added->address->magic = MDUMP_MAGIC_BLOCK;
        file->free_block_i1 = file->blocks_count;
    }

    ASSERT(0 < file->free_block_i1 && file->free_block_i1 <= file->blocks_count);
    ASSERT(file->blocks_count <= file->blocks_capacity);
    
    Mdump_Block* into = &file->blocks[file->free_block_i1 - 1];
    u8* block_end = (u8*) into->address + into->size;
    u8* free_from = (u8*) into->address + into->used_to;
    u8* aligned_from = (u8*) align_forward(free_from, align);
    if(block_end - aligned_from >= size)
    {
        out.block = file->free_block_i1 - 1;
        out.offset = (u32) (free_from - (u8*) into->address);
        memset(aligned_from, 0, size);
        *addr = aligned_from;
    }
    else
    {
        file->free_block_i1 = 0;
        ASSERT_MSG(second_try == false, "Must not get stuck in infinite recursion!");
        out = _mdump_allocate(file, size, align, addr, true);
    }

    return out;
}

Mdump_Ptr mdump_allocate_width_address(Mdump_File* file, isize size, isize align, void** addr)
{
    return _mdump_allocate(file, size, align, addr, false);
}

Mdump_Ptr mdump_allocate(Mdump_File* file, isize size, isize align)
{
    void* addr = NULL;
    return _mdump_allocate(file, size, align, &addr, false);
}

void* mdump_get(Mdump_File* file, Mdump_Ptr ptr)
{
    if(ptr.offset <= sizeof(Mdump_Block_Internal))
        return NULL;

    ASSERT(0 <= ptr.block && ptr.block < file->info.block_count);
    Mdump_Block* block = &file->blocks[ptr.block];
    ASSERT(ptr.offset < block->size);

    return (u8*) block->address + ptr.offset;
}

void* mdump_get_safe(Mdump_File* file, Mdump_Ptr ptr)
{
    if(0 <= ptr.block && ptr.block < file->info.block_count)
    {
        Mdump_Block* block = &file->blocks[ptr.block];
        if(ptr.offset < block->size && ptr.offset > sizeof(Mdump_Block_Internal))
            return (u8*) block->address + ptr.offset;
    }

    return NULL;
}

Mdump_Ptr mdump_copy(Mdump_File* file, const void* data, isize size, isize align)
{
    void* address = NULL;
    Mdump_Ptr ptr = mdump_allocate_width_address(file, size, align, &address);

    ASSERT(address != NULL);
    memcpy(address, data, size);

    return ptr;
}

void* mdump_array_data(Mdump_File* file, Mdump_Array arr)
{
    if(arr.is_address)
        return arr.address;
    else
        return mdump_get_safe(file, arr.ptr);
}

Mdump_Type mdump_array_type(Mdump_Type type)
{
    return (Mdump_Type) ((u32) type | MDUMP_ARRAY_TYPE_BIT);
}

Mdump_Type mdump_value_type(Mdump_Type type)
{
    return (Mdump_Type) ((u32) type & ~MDUMP_ARRAY_TYPE_BIT);
}

bool mdump_is_array_type(Mdump_Type type)
{
    return type == mdump_array_type(type);
}

Mdump_Type_Description_Rep _mdump_make_type_description(Mdump_Type type, u32 size, const char* name, u32 flags)
{
    Mdump_Type_Description_Rep decription = {type};
    decription.size = size;
    decription.type_name.is_address = true;
    decription.type_name.size = strlen(name);
    decription.type_name.address = (void*) name;
    decription.flags = flags;

    return decription;
}

Mdump_Type_Description_Rep mdump_get_type_description(Mdump_File* file, Mdump_Type type)
{
    Mdump_Type_Description_Rep empty = {MDUMP_UNASSIGNED};
    if(type > MDUMP_RESERVED_MAX)
    {
        u32 offset = type - MDUMP_RESERVED_MAX;
        if(offset >= file->info.types.size)
            return empty;

        Mdump_Type_Description_Rep* descriptions = (Mdump_Type_Description_Rep*) mdump_array_data(file, file->info.types);
        return descriptions[offset];
    }
    else
    {
        switch(type)
        {
            default:
            case MDUMP_UNASSIGNED: return _mdump_make_type_description(MDUMP_UNASSIGNED, 0, "unassigned", 0); 
            case MDUMP_BLANK: return _mdump_make_type_description(MDUMP_BLANK, 0, "blank", 0);

            case MDUMP_COMMENT: return _mdump_make_type_description(MDUMP_COMMENT, sizeof(Mdump_Array), "comment", 0);
            case MDUMP_STRING: return _mdump_make_type_description(MDUMP_STRING, sizeof(Mdump_Array), "string", 0);
            case MDUMP_BINARY: return _mdump_make_type_description(MDUMP_BINARY, sizeof(Mdump_Array), "binary", 0);

            case MDUMP_BOOL: return _mdump_make_type_description(MDUMP_BOOL, 1, "bool", 0);
            case MDUMP_CHAR: return _mdump_make_type_description(MDUMP_CHAR, 1, "char", 0);
            case MDUMP_RUNE: return _mdump_make_type_description(MDUMP_RUNE, 4, "rune", 0);
            case MDUMP_PTR: return _mdump_make_type_description(MDUMP_PTR, 8, "ptr", 0);

            case MDUMP_U8: return _mdump_make_type_description(MDUMP_U8, 1, "u8", 0);
            case MDUMP_U16: return _mdump_make_type_description(MDUMP_U16, 2, "u16", 0);
            case MDUMP_U32: return _mdump_make_type_description(MDUMP_U32, 4, "u32", 0);
            case MDUMP_U64: return _mdump_make_type_description(MDUMP_U64, 8, "u64", 0);
    
            case MDUMP_I8: return _mdump_make_type_description(MDUMP_I8, 1, "i8", 0);
            case MDUMP_I16: return _mdump_make_type_description(MDUMP_I16, 2, "i16", 0);
            case MDUMP_I32: return _mdump_make_type_description(MDUMP_I32, 4, "i32", 0);
            case MDUMP_I64: return _mdump_make_type_description(MDUMP_I64, 8, "i64", 0);

            case MDUMP_F8: return _mdump_make_type_description(MDUMP_F8, 1, "f8", 0);
            case MDUMP_F16: return _mdump_make_type_description(MDUMP_F16, 2, "f16", 0);
            case MDUMP_F32: return _mdump_make_type_description(MDUMP_F32, 4, "f32", 0);
            case MDUMP_F64: return _mdump_make_type_description(MDUMP_F64, 8, "f64", 0);
        }
    }
}

String mdump_string_to_string(Mdump_File* file, Mdump_Array mdump_string)
{
    String out = {0};
    out.data = (char*) mdump_array_data(file, mdump_string);
    out.size = mdump_string.size;

    return out;
}

Mdump_Array mdump_string_from_string(Mdump_File* file, String string)
{
    Mdump_Array str = {0};

    void* addr = NULL;

    //Extra space for null termination!
    str.ptr = mdump_allocate_width_address(file, string.size + 1, 1, &addr);
    str.size = string.size;

    ASSERT(addr != NULL);
    memcpy(addr, string.data, string.size);

    return str;
}

Mdump_Type mdump_type_by_name(Mdump_File* file, String type_name)
{
    if(type_name.size > 0)
    {
        switch(type_name.data[0])
        {
            case 'u': {
                if(string_is_equal(type_name, STRING("u32")))
                    return MDUMP_U32;
                
                if(string_is_equal(type_name, STRING("u64")))
                    return MDUMP_U64;
            
                if(string_is_equal(type_name, STRING("u8")))
                    return MDUMP_U8;

                if(string_is_equal(type_name, STRING("u16")))
                    return MDUMP_U16;
            } break;
            
            case 'i': {
                if(string_is_equal(type_name, STRING("i32")))
                    return MDUMP_I32;
                
                if(string_is_equal(type_name, STRING("i64")))
                    return MDUMP_I64;
            
                if(string_is_equal(type_name, STRING("i8")))
                    return MDUMP_I8;

                if(string_is_equal(type_name, STRING("i16")))
                    return MDUMP_I16;
            } break;
        
            case 'f': {
                if(string_is_equal(type_name, STRING("f32")))
                    return MDUMP_F32;
                
                if(string_is_equal(type_name, STRING("f64")))
                    return MDUMP_F64;
            
                if(string_is_equal(type_name, STRING("f8")))
                    return MDUMP_F8;

                if(string_is_equal(type_name, STRING("f16")))
                    return MDUMP_F16;
            } break;
        
            case 'c': {
                if(string_is_equal(type_name, STRING("char")))
                    return MDUMP_CHAR;
                
                if(string_is_equal(type_name, STRING("comment")))
                    return MDUMP_COMMENT;
            } break;
        
            case 's': {
                if(string_is_equal(type_name, STRING("string")))
                    return MDUMP_STRING;
        
            } break;

            case 'b': {
                if(string_is_equal(type_name, STRING("bool")))
                    return MDUMP_BOOL;
                
                if(string_is_equal(type_name, STRING("binary")))
                    return MDUMP_BINARY;
                
                if(string_is_equal(type_name, STRING("blank")))
                    return MDUMP_BLANK;
            } break;
        
            case 'p': {
                if(string_is_equal(type_name, STRING("ptr")))
                    return MDUMP_PTR;
            } break;

            case 'r': {
                if(string_is_equal(type_name, STRING("rune")))
                    return MDUMP_RUNE;
            } break;
        }
    }
    
    Mdump_Type_Description_Rep* types = (Mdump_Type_Description_Rep*) mdump_array_data(file, file->info.types);
    for(isize i = 0; i < file->info.types.size; i++)
    {
        Mdump_Type_Description_Rep* type = &types[i];
        String found_type_name = mdump_string_to_string(file, type->type_name);
        if(type->type != MDUMP_UNASSIGNED && string_is_equal(found_type_name, type_name))
        {
            Mdump_Type out_type = (Mdump_Type) ((u32) MDUMP_RESERVED_MAX + i + 1);
            return out_type;
        }
    }

    return MDUMP_UNASSIGNED;
}

//@TODO: logging
bool mdump_verify_type_against(Mdump_File* file, Mdump_Type_Description description, Mdump_Type against)
{
    Mdump_Type_Description_Rep desc_rep = mdump_get_type_description(file, against);
    if(desc_rep.size != description.size || desc_rep.members.size != description.members_size || desc_rep.flags != description.flags)
        return false;
            
    Mdump_Type_Member_Rep* member_reps = (Mdump_Type_Member_Rep*) mdump_array_data(file, desc_rep.members);
    for(isize i = 0; i < description.members_size; i++)
    {
        Mdump_Type_Member_Rep* member_rep = &member_reps[i];
        const Mdump_Type_Member* member = &description.members[i];
            
        if(member_rep->offset != member->offset || member_rep->flags != member->flags || member_rep->type != member->type)
            return false;

        String name_rep = mdump_string_to_string(file, member_rep->name);
        if(string_is_equal(name_rep, member->name) == false)
            return false;

        void* def_val = mdump_get_safe(file, member_rep->default_value);
        if((member->default_value == NULL) != (def_val == NULL))
            return false;
                
        //@TODO: There is a problem here that we dont know how to compare types. ANything containing ptr may not be compared with memcmp.
        if(0)
        {
            Mdump_Type_Description_Rep member_description = mdump_get_type_description(file, member->type);
            if(member_description.type != member->type)
                return false;

            if(memcmp(def_val, member->default_value, member_description.size) != 0)
                return false;
        }
    }

    return true;
}

bool mdump_verify_type(Mdump_File* file, Mdump_Type_Description description)
{
    Mdump_Type found = mdump_type_by_name(file, description.type_name);
    if(found == MDUMP_UNASSIGNED)
        return false;

    return mdump_verify_type_against(file, description, found);
}

Mdump_Type mdump_add_type(Mdump_File* file, Mdump_Type_Description description)
{
    //if already exists then check if is the same 
    Mdump_Type found = mdump_type_by_name(file, description.type_name);
    if(found != MDUMP_UNASSIGNED)
    {
        //If it is the same then the add succeeded 
        if(mdump_verify_type_against(file, description, found))
            return found;
        //else it failed
        else
            return MDUMP_UNASSIGNED;
    }

    Mdump_Type_Description_Rep* types = (Mdump_Type_Description_Rep*) mdump_array_data(file, file->info.types);
    if(file->info.types.size >= file->type_capacity)
    {
        u32 new_cap = 8;
        while(new_cap <= file->info.types.size)
            new_cap *= 2;

        void* new_data = NULL;
        Mdump_Ptr new_ptr = mdump_allocate_width_address(file, new_cap * sizeof(Mdump_Type_Description_Rep), DEF_ALIGN, &new_data);
        memcpy(new_data, types, file->info.types.size * sizeof(Mdump_Type_Description_Rep));
        
        file->info.types.ptr = new_ptr;
        file->info.types.is_address = false;
        file->type_capacity = new_cap;
        types = (Mdump_Type_Description_Rep*) new_data;
    }

    ASSERT(file->info.types.size < file->type_capacity);
    Mdump_Type type = (Mdump_Type) ((u32) MDUMP_RESERVED_MAX + file->info.types.size + 1);
    Mdump_Type_Description_Rep* added_type = &types[file->info.types.size++];

    added_type->flags = description.flags;
    added_type->size = description.size;
    added_type->type = type;
    added_type->type_name = mdump_string_from_string(file, description.type_name);

    Mdump_Type_Member_Rep* member_reps = NULL;
    added_type->members.ptr = mdump_allocate_width_address(file, description.members_size * sizeof *member_reps, DEF_ALIGN, (void**) &member_reps);
    added_type->members.size = description.members_size;

    for(isize i = 0; i < description.members_size; i++)
    {
        Mdump_Type_Member_Rep* member_rep = &member_reps[i];
        const Mdump_Type_Member* member = &description.members[i];

        member_rep->flags = member->flags;
        member_rep->offset = member->offset;
        member_rep->type = member->type;
        member_rep->name = mdump_string_from_string(file, member->name);
        if(member->default_value)
        {
            Mdump_Type_Description_Rep description_rep = mdump_get_type_description(file, member->type);
            if(description_rep.type != MDUMP_UNASSIGNED)
                member_rep->default_value = mdump_copy(file, member->default_value, description_rep.size, DEF_ALIGN);
        }
    }

    return type;
}

bool mdump_remove_type(Mdump_File* file, Mdump_Type type);


typedef bool (*Mdump_Lpf_Serialize_Func)(Lpf_Dyn_Entry* entry, Mdump_File* file, void* data, const void* def, Read_Or_Write action);

Mdump_Lpf_Serialize_Func serialize_func_by_mdump_type(Mdump_Type type)
{
    (void) type;
    return NULL;
}

bool _serialize_mdumped(Lpf_Dyn_Entry* entry, Mdump_File* file, Mdump_Type type, void* data, Read_Or_Write action, isize depth)
{
    if(depth > 1000)
        return false;

    bool state = true;
    if(mdump_is_array_type(type))
    {
        Mdump_Array type_array = {0};
        memcpy(&type_array, data, sizeof(type_array));
        for(isize i = 0; i < type_array.size && state; i++)
            state = _serialize_mdumped(entry, file, type, data, action, depth);
    }
    else
    {
        Mdump_Lpf_Serialize_Func serialize = serialize_func_by_mdump_type(type); //some lookup function
        if(serialize)
            return serialize(entry, file, data, NULL, action);
        else
        {
            Mdump_Type_Description_Rep description = mdump_get_type_description(file, type);
            Mdump_Type_Member_Rep* memebers = (Mdump_Type_Member_Rep*) mdump_array_data(file, description.members);
            bool state = true;

            isize memebers_successfull = 0;
            if(description.members.size > 0)
            {
                for(isize j = 0; j < description.members.size && state; j++)
                {
                    Mdump_Type_Member_Rep member = memebers[j];
                    void* member_addr = (u8*) data + member.offset;
                    String label = mdump_string_to_string(file, member.name);
                    Lpf_Dyn_Entry* located = serialize_locate_any(entry, LPF_KIND_ENTRY, (Lpf_Kind) (-1), label, STRING(""), SERIALIZE_WRITE);

                    bool member_state = _serialize_mdumped(located, file, member.type, member_addr, action, depth + 1);
                    if(member_state)
                        memebers_successfull += 1;

                    if((member.flags & MDUMP_FLAG_OPTIONAL) == 0)
                        state = member_state;
                }
                
                if(description.flags & MDUMP_FLAG_NEEDS_AT_LEAST_ONE_MEMBER)
                    state = state && memebers_successfull > 0;
                    
                if(description.flags & MDUMP_FLAG_NEEDS_EXACTLY_ONE_MEMBER)
                    state = state && memebers_successfull == 1;

                if(state)
                {
                    String type = mdump_string_to_string(file, description.type_name);
                    serialize_entry_set_identity(entry, type, STRING(""), LPF_KIND_SCOPE_START, 0);
                }
            }
            else
            {
                switch(description.type)
                {
                    case MDUMP_F32: {
                        state = serialize_f32(entry, (f32*) data, 0, action);
                    } break;
                    
                    case MDUMP_F64: {
                        state = serialize_f64(entry, (f64*) data, 0, action);
                    } break;

                    //...

                    //default to base64
                    default: {
                        String unknown_data = {(const char*) data, description.size};
                        String type = STRING("TODO");
                        state = serialize_write_base64(entry, unknown_data, type);
                    } break;
                }
            }
        }
    }

    return state;
}