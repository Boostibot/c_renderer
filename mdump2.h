
#include "lib/string.h"
#include "lib/arena.h"
#include "lib/lpf.h"

enum {
    MDUMP_FLAG_PTR = 1,
    MDUMP_FLAG_ARRAY = 2,
    MDUMP_FLAG_LIST = 4,
    MDUMP_FLAG_ENUM = 8,
    MDUMP_FLAG_ENUM_UNSIGNED = 16,
};

typedef struct Mdump_Member_User Mdump_Member_User;

typedef u64 Mdump_Ptr;

typedef struct Mdump_String {
    Mdump_Ptr data;
    isize size;
} Mdump_String;

typedef struct Mdump_Array {
    Mdump_Ptr data;
    isize size;
} Mdump_Array;

typedef struct Mdump_Node {
    Mdump_Ptr next;
    Mdump_Ptr prev;
    u8 data[];
} Mdump_Node;

typedef struct Mdump_List {
    Mdump_Ptr first;
    Mdump_Ptr last;
    isize size;
} Mdump_List;


typedef enum Mdump_Action {
    MDUMP_READ,
    MDUMP_WRITE,
} Mdump_Action;

typedef union {
    char text[9];
    u64 value;
} Mdump_Magic_64;

#define MDUMP_MAGIC         BRACE_INIT(Mdump_Magic_64){"mdumphdr"}.value
#define MDUMP_MAGIC_BLOCK   BRACE_INIT(Mdump_Magic_64){"mdumpblk"}.value
#define MDUMP_MAGIC_META    BRACE_INIT(Mdump_Magic_64){"mdumpmta"}.value

typedef struct Mdump_Header_File {
    u64 magic;
    u64 data_offset;

    i32 version;
    u32 _padding1;
    i64 creation_time;
    char schema_name[32];
    //Mdump_String schema_name;
    Mdump_String description;

    u64 types_ptr;
    u64 root_ptr;
    Mdump_Type_ID root_type;
    u32 _padding2;

    u64 next_header_offset;
    u64 checksum;
} Mdump_Header_File;

typedef struct Mdump {
    Arena_Stack file_arena_stack;
    Arena file_arena;
    Arena* user_arena;
} Mdump;


// 
//
#define NIL 0

void* mdump_get(Mdump* mdump, Mdump_Ptr ptr, isize size)
{
    if(size < 0 || (isize) ptr + size > mdump->file_arena_stack.size || ptr == 0)
        return NULL;

    return mdump->file_arena_stack.data + ptr;
}

void* mdump_get_assert(Mdump* mdump, Mdump_Ptr ptr, isize size)
{
    if(ptr == 0)
        return NULL;

    ASSERT(size >= 0 && (isize) ptr + size <= mdump->file_arena_stack.size);
    return mdump->file_arena_stack.data + ptr;
}

Mdump_Ptr mdump_unget(Mdump* mdump, const void* addr, isize size)
{
    Mdump_Ptr ptr = (u8*) addr - (u8*) mdump->file_arena_stack.data;
    if(size < 0 || (isize) ptr + size > mdump->file_arena_stack.size || ptr == 0)
        return NIL;

    return ptr;
}

void mdump_init(Mdump* mdump, Arena* user_arena)
{
    mdump->user_arena = user_arena;
    arena_init(&mdump->file_arena_stack, 0, 0, "mdump arena");
    mdump->file_arena = arena_acquire(&mdump->file_arena_stack);
}

#define MDUMP_GET_ARRAY(mdump, mdump_array, Type) ((Type*) mdump_get((mdump), (mdump_array).data, (mdump_array).size * sizeof(Type)))
#define MDUMP_GET(mdump, ptr, Type)             ((Type*) mdump_get((mdump), (ptr), sizeof(Type)))
#define MDUMP_GET_ASSERT(mdump, ptr, Type)      ((Type*) mdump_get_assert((mdump), (ptr), sizeof(Type)))
#define MDUMP_MEMBER_PTR(ptr, Type, member)     (ptr + offsetof(Type, member))

u64 mdump_file_allocate(Mdump* mdump, isize size, isize align, void* out_ptr_or_null)
{
    
    u8* out_ptr = (u8*) arena_push(&mdump->file_arena, size, align);
    i64 offset = out_ptr - mdump->file_arena_stack.data;
    ASSERT(offset >= 0);
    
    if(out_ptr_or_null)
        *(void**) out_ptr_or_null = out_ptr;

    return offset;
}

Mdump_Array mdump_file_array_allocate(Mdump* mdump, isize item_count, isize item_size, void* out_ptr_or_null)
{
    Mdump_Array out = {0};
    out.data = mdump_file_allocate(mdump, item_count*item_size, 8, out_ptr_or_null);
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
        file_string->data = mdump_file_allocate(mdump, user_string->size + 1, 1, &adress);
        file_string->size = user_string->size;
        memcpy(adress, user_string->data, user_string->size);
        return true;
    }
}


bool mdump_raw(Mdump* mdump, void* user, void* file, isize size, Mdump_Action action)
{
    (void) mdump;
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
        void* data = mdump_get(mdump, array->data, array->size * item_size);
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
        array->data = mdump_file_allocate(mdump, *item_count*item_size, align, &adress);
        array->size = *item_count;
        memcpy(adress, *items, *item_count*item_size);
        return true;
    }
}

bool mdump_array(Mdump* mdump, void** items, isize* item_count, isize item_size, Mdump_Array* array, Mdump_Action action)
{
    return mdump_array_custom(mdump, items, item_count, item_size, 8, array, action);
}

bool mdump_is_properly_linked(Mdump* mdump, Mdump_Ptr node_ptr)
{
    if(node_ptr == NIL)
        return true;

    Mdump_Node* node = MDUMP_GET_ASSERT(mdump, node_ptr, Mdump_Node);
    if(node == NULL)
        return false;

    Mdump_Node* next = MDUMP_GET_ASSERT(mdump, node->next, Mdump_Node);
    Mdump_Node* prev = MDUMP_GET_ASSERT(mdump, node->prev, Mdump_Node);
    if(prev && prev->next != node_ptr)
        return false;
     if(next && next->prev != node_ptr)
        return false;
        
    return true;
}

#define MDUMP_NODE_GET(mdump, node) MDUMP_GET((mdump), (node), Mdump_Node)

Mdump_Node* mdump_list_add_node(Mdump* mdump, Mdump_Ptr node_ptr, Mdump_Ptr* first_ptr, Mdump_Ptr* last_ptr, Mdump_Ptr after_or_null_ptr)
{
    Mdump_Node* node = MDUMP_GET_ASSERT(mdump, node_ptr, Mdump_Node);
    Mdump_Node* first = MDUMP_GET_ASSERT(mdump, *first_ptr, Mdump_Node);
    Mdump_Node* last = MDUMP_GET_ASSERT(mdump, *last_ptr, Mdump_Node);
    Mdump_Node* after = MDUMP_GET_ASSERT(mdump, after_or_null_ptr, Mdump_Node);

    if(node == NULL)
        return NULL;

    ASSERT("node must not be null and izolated, after must be properly linked, list must be valid"
        && (first == NULL) == (last == NULL)
        && node->prev == NIL 
        && node->next == NIL
        && mdump_is_properly_linked(mdump, *first_ptr)
        && mdump_is_properly_linked(mdump, *last_ptr)
        && mdump_is_properly_linked(mdump, after_or_null_ptr)
    );

    if(first == NULL)
    {
        *first_ptr = node_ptr;
        *last_ptr = node_ptr;                                          
        node->next = NIL;                                                
        node->prev = NIL;    
    }
    else if(after_or_null_ptr)
    {
        node->prev = NIL;                                            
        node->next = *first_ptr;                                         
        if(first != NULL)                                              
            first->prev = node_ptr;                                
        *first_ptr = node_ptr;
    }
    else
    {
        if(after->next != NIL)
            MDUMP_GET_ASSERT(mdump, after->next, Mdump_Node)->prev = node_ptr;   
            
        node->next = after->next;                                   
        node->prev = (after_or_null_ptr),                                         
        after->next = (node_ptr);                                         
        if(after == last)                                              
            *last_ptr = node_ptr;
    }

    return node;
}

Mdump_Node* mdump_list_remove_node(Mdump* mdump, Mdump_Ptr node_ptr, Mdump_Ptr* first_ptr, Mdump_Ptr* last_ptr)
{
    Mdump_Node* node = MDUMP_GET_ASSERT(mdump, node_ptr, Mdump_Node);
    Mdump_Node* first = MDUMP_GET_ASSERT(mdump, *first_ptr, Mdump_Node);
    Mdump_Node* last = MDUMP_GET_ASSERT(mdump, *last_ptr, Mdump_Node);

    if(node == NULL)
        return NULL;

    ASSERT("node must not be null and izolated, after must be properly linked, list must be valid"
        && (first == NULL) == (last == NULL)
        && mdump_is_properly_linked(mdump, *first_ptr)
        && mdump_is_properly_linked(mdump, *last_ptr)
        && mdump_is_properly_linked(mdump, node_ptr)
    );

    if(first == node)
    {
        *first_ptr = first->next;
        first = MDUMP_GET_ASSERT(mdump, *first_ptr, Mdump_Node);
        if(first == NULL)
            *last_ptr = NIL;
        else
            first->prev = NIL;
    }
    else if(first == node)
    {
        *last_ptr = last->prev;
        last = MDUMP_GET_ASSERT(mdump, *first_ptr, Mdump_Node);
        if(last == NULL)
            *first_ptr = NIL;
        else
            last->prev = NIL;
    }
    else
    {
        if(node->next != NIL)
            MDUMP_GET_ASSERT(mdump, node->next, Mdump_Node)->prev = (node)->prev;
        if(node->prev != NIL)
            MDUMP_GET_ASSERT(mdump, node->prev, Mdump_Node)->next = (node)->next;
    }

    node->prev = NIL;
    node->next = NIL;
    return node;
}

Mdump_Ptr mdump_list_add(Mdump* mdump, isize item_size, void* data_or_null, Mdump_List* list, Mdump_Ptr after_or_null_ptr)
{
    Mdump_Node* node = NULL;
    Mdump_Ptr node_ptr = mdump_file_allocate(mdump, isizeof(Mdump_Node) + item_size, DEF_ALIGN, &node);
    if(data_or_null)
        memcpy(node->data, data_or_null, item_size);

    mdump_list_add_node(mdump, node_ptr, &list->first, &list->last, after_or_null_ptr);
    list->size += 1;
    return node_ptr;
}

Mdump_Node* mdump_list_remove(Mdump* mdump, Mdump_Ptr ptr, Mdump_List* list)
{
    if(mdump_list_remove_node(mdump, ptr, &list->first, &list->last) != NULL)
        list->size -= 1;
}

typedef struct Mdump_List_Iterator {
    Mdump_Ptr node;
    Mdump_Node* ptr;
    i32 iter;
    bool is_last_iter;
    bool _padding[3];
} Mdump_List_Iterator;

//Iterates the section [first, last] starting from first and going to next_or_prev. Note that list can be sublist.
#define _MDUMP_LIST_FOR_EACH(mdump, it, first, last, next_or_prev) \
    for(Mdump_List_Iterator it = {(first)}; \
        \
        it.ptr = MDUMP_GET((mdump), it.node, Mdump_Node), \
        it.is_last_iter ? it.ptr = NULL : 0, /* if was already last iter last time around stop */ \
        it.is_last_iter = it.node == (last), /* set last iter if is indeed the last iter*/ \
        it.ptr != NULL; \
        \
        it.node = it.ptr->next_or_prev, it.iter ++) \

#define MDUMP_LIST_FOR_EACH(mdump, it, list)            _MDUMP_LIST_FOR_EACH((mdump), it, (list).first, (list).last, next)
#define MDUMP_LIST_FOR_EACH_BACKWARDS(mdump, it, list)  _MDUMP_LIST_FOR_EACH((mdump), it, (list).last, (list).first, prev)

bool mdump_list(Mdump* mdump, void** items, isize* item_count, isize item_size, Mdump_List* list, Mdump_Action action)
{
    if(action == MDUMP_READ)
    {
        void* adress = mdump_user_allocate(mdump, list->size*item_size, DEF_ALIGN);
        MDUMP_LIST_FOR_EACH(mdump, it, *list)
        {
            memcpy((u8*) adress + it.iter*item_size, it.ptr->data, item_size);
            *item_count += 1;
        }

        if(*item_count == list->size)
            return true;
        else
            return false;
    }
    else
    {
        isize node_size = isizeof(Mdump_Node) + item_size;
        u64 first_node = mdump_file_allocate(mdump, *item_count*node_size, DEF_ALIGN, NULL);
        for(isize i = 0; i < *item_count; i++)
        {
            Mdump_Node* item = mdump_list_add_node(mdump, first_node + i*node_size, &list->first, &list->last, list->last);
            if(items)
                memcpy(item->data, items[i], item_size);
        }

        list->size += *item_count;
        return true;
    }
}

typedef struct Mdump_Type_File {
    Mdump_String name;
    Mdump_Array members;
    u32 size;
    u32 align;
    u32 flags;
    Mdump_Type_ID id;
} Mdump_Type_File;

typedef struct Mdump_Member_File {
    Mdump_String name;
    Mdump_Type_ID id;
    u32 offset;
    u32 size;
    u32 flags;
    u64 enum_value;
} Mdump_Member_File;

typedef struct Mdump_Type_User Mdump_Type_User;
typedef Mdump_Type_User (*Mdump_Type_User_Query)();

typedef struct Mdump_Member_User {
    String name;
    Mdump_Type_User_Query type_query;
    Mdump_Type_ID         type_id;

    u32 offset;
    u32 size;
    u32 flags;
    u64 enum_value;
} Mdump_Member_User;

typedef struct Mdump_Type_User {
    String name;
    Mdump_Type_User_Query query;
    Mdump_Type_ID         id;
    u32 size;
    u32 align;

    Mdump_Member_User* members;
    isize member_count;
    u32 flags;
    u32 depth;
} Mdump_Type_User;


typedef enum Mdump_Type_ID {
    MDUMP_TYPE_NONE = 0,

    MDUMP_TYPE_BOOL,
    MDUMP_TYPE_CHAR,
    MDUMP_TYPE_CODEPOINT,

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

    MDUMP_TYPE_STRING,
    MDUMP_TYPE_MAX_RESERVED = 32,
} Mdump_Type_ID;

#define _DEFINE_MDUMP_BUILTIN_TYPE(name, type, ID) \
    Mdump_Type_User mdump_type_##name() { \
        return BRACE_INIT(Mdump_Type_User){STRING(#name), mdump_type_##name, ID, sizeof(type), __alignof(char)}; \
    } \

#define _DECLARE_MDUMP_BUILTINT_TYPE(name, type) \
    void mdump_##name(Mdump* mdump, type* user, type* file, Mdump_Action action) { \
        mdump_raw(mdump, user, file, sizeof *user, action); \
    } \

#define _DEFINE_AND_DECLARE_MDUMP_BUILTIN(name, type, ID) \
    _DEFINE_MDUMP_BUILTIN_TYPE(name, type, ID) \
    _DECLARE_MDUMP_BUILTINT_TYPE(name, type) \

//Bool is define so we have to do this manually for it in case of C
Mdump_Type_User mdump_type_bool() { return BRACE_INIT(Mdump_Type_User){STRING("bool"), mdump_type_bool, MDUMP_TYPE_BOOL, sizeof(bool), __alignof(bool)}; }
void mdump_bool(Mdump* mdump, bool* user, bool* file, Mdump_Action action) { mdump_raw(mdump, user, file, sizeof *user, action); }

//_DEFINE_AND_DECLARE_MDUMP_BUILTIN(bool, bool, MDUMP_TYPE_BOOL)
_DEFINE_AND_DECLARE_MDUMP_BUILTIN(char, char, MDUMP_TYPE_CHAR)
_DEFINE_AND_DECLARE_MDUMP_BUILTIN(codepoint, u32, MDUMP_TYPE_CODEPOINT)

_DEFINE_AND_DECLARE_MDUMP_BUILTIN(u8,  u8,  MDUMP_TYPE_U8)
_DEFINE_AND_DECLARE_MDUMP_BUILTIN(u16, u16, MDUMP_TYPE_U16)
_DEFINE_AND_DECLARE_MDUMP_BUILTIN(u32, u32, MDUMP_TYPE_U32)
_DEFINE_AND_DECLARE_MDUMP_BUILTIN(u64, u64, MDUMP_TYPE_U64)

_DEFINE_AND_DECLARE_MDUMP_BUILTIN(i8,  i8,  MDUMP_TYPE_I8)
_DEFINE_AND_DECLARE_MDUMP_BUILTIN(i16, i16, MDUMP_TYPE_I16)
_DEFINE_AND_DECLARE_MDUMP_BUILTIN(i32, i32, MDUMP_TYPE_I32)
_DEFINE_AND_DECLARE_MDUMP_BUILTIN(i64, i64, MDUMP_TYPE_I64)

_DEFINE_AND_DECLARE_MDUMP_BUILTIN(f8,  u8,  MDUMP_TYPE_F8)
_DEFINE_AND_DECLARE_MDUMP_BUILTIN(f16, u16, MDUMP_TYPE_F16)
_DEFINE_AND_DECLARE_MDUMP_BUILTIN(f32, f32, MDUMP_TYPE_F32)
_DEFINE_AND_DECLARE_MDUMP_BUILTIN(f64, f64, MDUMP_TYPE_F64)

_DEFINE_MDUMP_BUILTIN_TYPE(string, Mdump_String, MDUMP_TYPE_STRING)

Mdump_Type_User_Query mdump_type_int(isize size)
{
    switch(size)
    {
        case 1: return mdump_type_i8;
        case 2: return mdump_type_i16;
        case 4: return mdump_type_i32;
        case 8: return mdump_type_i64;
        
        default: ASSERT(false); return mdump_type_i8;
    }
}

Mdump_Type_User_Query mdump_type_uint(isize size)
{
    switch(size)
    {
        case 1: return mdump_type_u8;
        case 2: return mdump_type_u16;
        case 4: return mdump_type_u32;
        case 8: return mdump_type_u64;
        
        default: ASSERT(false); return mdump_type_u8;
    }
}


Mdump_Type_User_Query mdump_type_float(isize size)
{
    switch(size)
    {
        case 1: return mdump_type_f8;
        case 2: return mdump_type_f16;
        case 4: return mdump_type_f32;
        case 8: return mdump_type_f64;
        
        default: ASSERT(false); return mdump_type_f32;
    }
}

//this can be simplified a lot using some defines...
#define DEFINE_MDUMP_TYPE_ALIGNED(Type, mdum_type_func, _align, ...) \
    Mdump_Type_User mdum_type_func() \
    { \
        typedef Type T; /* so that MDUMP_MEMBER can use it */\
        static Mdump_Member_User members[] = {  \
            __VA_ARGS__ \
        }; \
        \
        Mdump_Type_User out = {0}; \
        out.name = STRING(#Type); \
        out.size = sizeof(Type); \
        out.align = _align; \
        out.members = members; \
        out.member_count = STATIC_ARRAY_SIZE(members); \
        return out; \
    } \

#define DEFINE_MDUMP_TYPE(Type, mdum_type_func, ...) DEFINE_MDUMP_TYPE_ALIGNED(Type, mdum_type_func, __alignof(Type), ##__VA_ARGS__) 
#define MDUMP_MEMBER(name, type, ...)               {STRING(#name), type, MDUMP_TYPE_NONE, offsetof(T, name), sizeof(((T*)0)->name), ##__VA_ARGS__}, 

#define MDUMP_ENUM_VALUE_NAMED(name, value) {STRING(#name), NULL, MDUMP_TYPE_NONE, 0, 0, MDUMP_FLAG_ENUM, (value)},
#define MDUMP_ENUM_VALUE(value)             MDUMP_ENUM_VALUE_NAMED(#value, (value))
    
#define _IS_UNSIGNED(a) ((a) >= 0 && ~(a) >= 0) 

#define DEFINE_MDUMP_ENUM(Type, mdum_type_func, ...) \
    Mdump_Type_User mdum_type_func() \
    { \
        static bool types_init = false; \
        static Mdump_Member_User members[] = {  \
            __VA_ARGS__ \
        }; \
        bool is_unsigned = _IS_UNSIGNED((Type) 0); \
        if(types_init == false) \
        { \
            types_init = true; \
            for(int i = 0; i < STATIC_ARRAY_SIZE(members); i++) \
            { \
                members[i].size = sizeof(Type); \
                if(is_unsigned) \
                    members[i].type_query = mdump_type_uint(sizeof(Type)); \
                else \
                    members[i].type_query = mdump_type_int(sizeof(Type)); \
            } \
        } \
        \
        u32 flags = is_unsigned ? MDUMP_FLAG_ENUM_UNSIGNED : MDUMP_FLAG_ENUM; \
        Mdump_Type_User out = {0}; \
        out.name = STRING(#Type); \
        out.size = sizeof(Type); \
        out.align = sizeof(Type); \
        out.members = members; \
        out.member_count = STATIC_ARRAY_SIZE(members); \
        out.flags = flags; \
        return out; \
    } \

void mdump_zero(void* user, isize user_size, void* serialized, isize serilaized_size, Mdump_Action action)
{
    if(action == MDUMP_READ)
        memset(user, 0, user_size);
    else
        memset(serialized, 0, serilaized_size);
}

bool mdump_array_prepare(Mdump* mdump, Mdump_Array* array, 
    void** user_items, isize* user_item_count, isize user_item_size, isize user_align,
    void** file_items, isize file_item_size, isize file_align,
    bool copy, Mdump_Action action)
{
    if(action == MDUMP_READ)
    {
        void* data = mdump_get(mdump, array->data, array->size * file_item_size);
        *file_items = data;
        if(data == NULL && array->size != 0)
        {
            *user_items = NULL;
            *user_item_count = 0;
            return false;
        }
        else
        {
            void* adress = mdump_user_allocate(mdump, array->size*user_item_size, user_align);
            *user_items = adress;
            *user_item_count = array->size;
            if(copy)
                memcpy(adress, data, array->size*user_item_size);
            return true;
        }
    }
    else
    {
        void* adress = NULL;
        array->data = mdump_file_allocate(mdump, *user_item_count*file_item_size, file_align, &adress);
        array->size = *user_item_count;
        if(copy)
            memcpy(adress, *user_items, *user_item_count*file_item_size);
        return true;
    }
}
bool mdump_array_prepare2(Mdump* mdump, Generic_Array user, Mdump_Array* file, isize file_item_size, isize file_align, 
    bool copy, Mdump_Action action)
{
    if(action == MDUMP_READ)
    {
        void* data = mdump_get(mdump, file->data, file->size * file_item_size);
        if(data == NULL && file->size != 0)
        {
            generic_array_resize(user, 0, true);
            return false;
        }
        else
        {
            generic_array_resize(user, 0, true);
            if(copy)
                memcpy(user.array->data, data, file->size*user.item_size);
            return true;
        }
    }
    else
    {
        void* adress = NULL;
        file->data = mdump_file_allocate(mdump, user.array->size*file_item_size, file_align, &adress);
        file->size = user.array->size;
        if(copy)
            memcpy(adress, user.array->data, user.array->size*file_item_size);
        return true;
    }
}

bool mdump_list_prepare(Mdump* mdump, Generic_Array user, Mdump_List* file, isize file_item_size, isize file_align,
    bool copy, Mdump_Action action)
{
    if(action == MDUMP_READ)
    {
        generic_array_reserve(user, file->size);
        if(copy)
        {
            MDUMP_LIST_FOR_EACH(mdump, it, *file)
            {
                generic_array_append(user, it.ptr->data, 1);
            }

            return user.array->size == file->size;
        }

        return true;
    }
    else
    {
        isize node_size = isizeof(Mdump_Node) + file_item_size;
        u64 first_node = mdump_file_allocate(mdump, file_item_size*node_size, file_align, NULL);
        for(isize i = 0; i < user.array->size; i++)
        {
            Mdump_Node* item = mdump_list_add_node(mdump, first_node + i*node_size, &file->first, &file->last, file->last);
            if(copy)
                memcpy(item->data, (u8*) user.array->data + i*user.item_size, user.item_size);
        }

        file->size += user.array->size;
        return true;
    }
}

#define MDUMP_ARRAY_PREP(mdump, file_array_ptr, user_ptr, user_ptr_size, file_ptr, action) \
        mdump_array_prepare((mdump), (file_array_ptr), \
            (void**) (user_ptr), (user_ptr_size), sizeof **(user_ptr), DEF_ALIGN, \
            (void**) (file_ptr), sizeof **(file_ptr), DEF_ALIGN, false, (action)) \
            
//#define MDUMP_LIST_PREP(mdump, file_list_ptr, user_ptr, user_ptr_size, file_ptr, action) \
//        mdump_list_prepare((mdump), (file_list_ptr), \
//            (void**) (user_ptr), (user_ptr_size), sizeof **(user_ptr), DEF_ALIGN, \
//            (void**) (file_ptr), sizeof **(file_ptr), DEF_ALIGN, false, (action)) \

#define MDUMP_ARRAY_IT(mdump, file_array_ptr, Type_User, Type_File, it, action) \
    struct { \
        Type_User* user; \
        Type_File* file; \
        isize size; \
        bool okay; \
        bool _padding[7]; \
    } it = {0}; \
    it.okay = mdump_array_prepare((mdump), (file_array_ptr), \
        (void**) &it.user, &it.size, sizeof(Type_User), __alignof(Type_User), \
        (void**) &it.file, sizeof(Type_File), __alignof(Type_File), false, (action)) \
        
DEFINE_MDUMP_ENUM(Mdump_Type_ID, mdump_type_mdump_type_id, 
    MDUMP_ENUM_VALUE(MDUMP_TYPE_NONE)
    MDUMP_ENUM_VALUE(MDUMP_TYPE_BOOL)
    MDUMP_ENUM_VALUE(MDUMP_TYPE_CODEPOINT)

    MDUMP_ENUM_VALUE(MDUMP_TYPE_U8)
    MDUMP_ENUM_VALUE(MDUMP_TYPE_U16)
    MDUMP_ENUM_VALUE(MDUMP_TYPE_U32)
    MDUMP_ENUM_VALUE(MDUMP_TYPE_U64)
    
    MDUMP_ENUM_VALUE(MDUMP_TYPE_I8)
    MDUMP_ENUM_VALUE(MDUMP_TYPE_I16)
    MDUMP_ENUM_VALUE(MDUMP_TYPE_I32)
    MDUMP_ENUM_VALUE(MDUMP_TYPE_I64)
    
    MDUMP_ENUM_VALUE(MDUMP_TYPE_F8)
    MDUMP_ENUM_VALUE(MDUMP_TYPE_F16)
    MDUMP_ENUM_VALUE(MDUMP_TYPE_F32)
    MDUMP_ENUM_VALUE(MDUMP_TYPE_F64)

    MDUMP_ENUM_VALUE(MDUMP_TYPE_STRING)
    MDUMP_ENUM_VALUE(MDUMP_TYPE_MAX_RESERVED)
)

DEFINE_MDUMP_TYPE(Mdump_Member_File, mdump_type_mdump_member, 
    MDUMP_MEMBER(name, mdump_type_string)
    MDUMP_MEMBER(id, mdump_type_mdump_type_id)
    MDUMP_MEMBER(offset, mdump_type_u32)
    MDUMP_MEMBER(flags, mdump_type_u32)
    MDUMP_MEMBER(size, mdump_type_u32)
    MDUMP_MEMBER(enum_value, mdump_type_u64)
)

DEFINE_MDUMP_TYPE(Mdump_Type_File, mdump_type_mdump_type, 
    MDUMP_MEMBER(name, mdump_type_string)
    MDUMP_MEMBER(members, mdump_type_mdump_member, MDUMP_FLAG_ARRAY)
    MDUMP_MEMBER(size, mdump_type_u32)
    MDUMP_MEMBER(align, mdump_type_u32)
    MDUMP_MEMBER(flags, mdump_type_u32)
    MDUMP_MEMBER(id, mdump_type_mdump_type_id)
)

void mdump_mdump_type_id(Mdump* mdump, Mdump_Type_ID* user, Mdump_Type_ID* file, Mdump_Action action)
{
    mdump_raw(mdump, user, file, sizeof *user, action);
}

bool mdump_mdump_type_member(Mdump* mdump, Mdump_Member_User* user, Mdump_Member_File* file, Mdump_Action action)
{
    mdump_zero(user, sizeof* user, file, sizeof *file, action);
    bool state = mdump_string(mdump, &user->name, &file->name, action);
    mdump_mdump_type_id(mdump, &user->type_id, &file->id, action);
    mdump_u32(mdump, &user->offset, &file->offset, action);
    mdump_u32(mdump, &user->flags, &file->flags, action);
    mdump_u32(mdump, &user->size, &file->size, action);
    mdump_u64(mdump, &user->enum_value, &file->enum_value, action);

    return state;
}

bool mdump_mdump_type(Mdump* mdump, Mdump_Type_User* type, Mdump_Type_File* type_file, Mdump_Action action)
{
    mdump_zero(type, sizeof* type, type_file, sizeof *type_file, action);
    bool state = mdump_string(mdump, &type->name, &type_file->name, action);
    mdump_u32(mdump, &type->size, &type_file->size, action);
    mdump_u32(mdump, &type->align, &type_file->align, action);
    mdump_u32(mdump, &type->flags, &type_file->flags, action);
    mdump_mdump_type_id(mdump, &type->id, &type_file->id, action);
    
    Mdump_Member_File* members_file = NULL;
    state = state && MDUMP_ARRAY_PREP(mdump, &type_file->members, &type->members, &type->member_count, &members_file, action);
    for(isize i = 0; i < type->member_count && state; i++)
        state = mdump_mdump_type_member(mdump, &type->members[i], &members_file[i], action);

    return state;
}

typedef Array(Mdump_Type_User) Mdump_Type_User_Array;
typedef Array(Mdump_Member_User) Mdump_Member_User_Array;

typedef struct Mdump_Type_Info {
    Arena arena;
    Mdump_Type_User_Array types;
} Mdump_Type_Info;

String string_dup(Arena* arena, String str)
{
    char* duped = (char*) arena_push_nonzero(arena, str.size + 1, 1);
    memcpy(duped, str.data, str.size);
    duped[str.size] = '\0';
    String out = {duped, str.size};
    return out;
}

String cstring_dup(Arena* arena, const char* str)
{
    return string_dup(arena, string_make(str));
}

isize _mdump_type_by_id(Mdump_Type_User_Array* types, Mdump_Type_ID id, Mdump_Type_User_Query or_by_query_or_null)
{
    for(isize i = 0; i < types->size; i++)
    {
        Mdump_Type_User* type = {0};
        if(type->id == id || (type->query == or_by_query_or_null && or_by_query_or_null != NULL))
           return i;
    }
    return -1;
}

#include "lib/log.h"
void mdump_types_add(Mdump_Type_Info* info, Mdump_Type_User user_type)
{
    //Only add the type if not already present!
    if(_mdump_type_by_id(&info->types, user_type.id, user_type.query) == -1)
    {
        Arena scratch = scratch_arena_acquire();

        //Gather all dependent types & check for cycles or uknowns
        Mdump_Type_User_Array add_stack = {&scratch.allocator};
        array_push(&add_stack, user_type);
    
        Mdump_Type_User_Array path_types = {&scratch.allocator};
        isize_Array path_iters = {&scratch.allocator};
        array_push(&path_types, user_type);
        array_push(&path_iters, 0);

        for(isize depth = 0; depth < path_iters.size; depth ++)
        {
            isize* i = &path_iters.data[depth];
            Mdump_Type_User* type = &path_types.data[depth];

            bool recursing = false;
            for(; *i < type->member_count; *i += 1)
            {
                Mdump_Member_User* member = &type->members[*i];
                bool found = false;

                //Search for the type in already existing types
                found = found || _mdump_type_by_id(&info->types, member->type_id, member->type_query) != -1;
            
                //Search for it within added types
                found = found || _mdump_type_by_id(&add_stack, member->type_id, member->type_query) != -1;

                //if not found try to add it
                if(found == false)
                {
                    //If it has no query => warn
                    if(member->type_query == NULL)
                        LOG_WARN("mdump", "Found an unknown type (missing type query) while evaluation type '%.*s' member '%.*s' with type id %i", 
                            STRING_PRINT(type->name), STRING_PRINT(member->name), (int) member->type_id);
                    else
                    {
                        //If it is in upstream path than a cycle occured => error
                        bool cycle_found = _mdump_type_by_id(&path_types, member->type_id, member->type_query) != -1;
                        if(cycle_found)
                        {
                            LOG_ERROR("mdump", "Found a type cycle while evaluation type '%.*s' member '%.*s' with type id %i", 
                                    STRING_PRINT(type->name), STRING_PRINT(member->name), (int) member->type_id);
                        }
                        else
                        {
                            Mdump_Type_User new_type = member->type_query();

                            //If everythings right add it.
                            array_push(&add_stack, new_type);
                            array_push(&path_types, new_type);
                            array_push(&path_iters, 0);
                            
                            recursing = true;
                            break;
                        }
                    }
                }
            }

            if(recursing == false)
            {
                array_pop(&path_types);
                array_pop(&path_iters);
            }
        }
        
        array_deinit(&path_types);
        array_deinit(&path_iters);

        //Add all of the gathered types
        for(isize type_i = 0; type_i < add_stack.size; type_i ++)
        {
            Mdump_Type_User type = add_stack.data[type_i];
            Mdump_Type_User copy = type;
            copy.name = string_dup(&info->arena, type.name);
            copy.members = ARENA_PUSH(&info->arena, copy.member_count, Mdump_Member_User);
            
            for(isize member_i = 0; member_i < type.member_count; member_i++)
            {
                Mdump_Member_User* copy_member = &copy.members[member_i];
                Mdump_Member_User* type_member = &type.members[member_i];

                *copy_member = *type_member;
                copy_member->name = string_dup(&info->arena, type_member->name);
            }

            array_push(&info->types, copy);
        }

        array_deinit(&add_stack);
        arena_release(&scratch); 
    }
}

void mdump_types_add_builtins(Mdump_Type_Info* info)
{
    mdump_types_add(info, mdump_type_bool());
    mdump_types_add(info, mdump_type_char());
    mdump_types_add(info, mdump_type_codepoint());

    mdump_types_add(info, mdump_type_i8());
    mdump_types_add(info, mdump_type_i16());
    mdump_types_add(info, mdump_type_i32());
    mdump_types_add(info, mdump_type_i64());
    
    mdump_types_add(info, mdump_type_u8());
    mdump_types_add(info, mdump_type_u16());
    mdump_types_add(info, mdump_type_u32());
    mdump_types_add(info, mdump_type_u64());

    mdump_types_add(info, mdump_type_f8());
    mdump_types_add(info, mdump_type_f16());
    mdump_types_add(info, mdump_type_f32());
    mdump_types_add(info, mdump_type_f64());

    mdump_types_add(info, mdump_type_string());
}

bool mdump_mdump_types(Mdump* mdump, Mdump_Type_Info* user, Mdump_List* file, Mdump_Action action)
{
    //@NOTE: append only. @TODO: make oevrwite
    bool state = mdump_list_prepare(mdump, array_make_generic(&user->types), file, sizeof(Mdump_Type_File), __alignof(Mdump_Type_File), false, action);
    MDUMP_LIST_FOR_EACH(mdump, it, *file) 
    {
        state = state && mdump_mdump_type(mdump, &user->types.data[it.iter], (Mdump_Type_File*) it.ptr->data, action);
        if(!state)
            break;
    }

    return state;
}

void mdump_write(String_Builder* into, Mdump* mdump, Mdump_Ptr root_ptr, String path, String schema_name)
{
    Mdump_Header_File header = {0};
    header.magic = MDUMP_MAGIC;
    header.creation_time = platform_epoch_time();
    memcpy(header.schema_name, schema_name.data, MIN(schema_name.size, sizeof(header.schema_name) - 1));
    header.version = 1;
    header.root_ptr = root_ptr;

    builder_append(into, BRACE_INIT(String){(char*) &header, sizeof(header)});
}


void test_mdump2()
{
    Arena arena = scratch_arena_acquire();

    Mdump dump = {0};
    mdump_init(&dump, &arena);

    Mdump_Type_Info info = {0};
    info.types.allocator = &arena.allocator;
    mdump_types_add_builtins(&info);

    Mdump_List* list = {0};
    Mdump_Ptr root = mdump_file_allocate(&dump, sizeof(*list), DEF_ALIGN, &list); 
    mdump_mdump_types(&dump, &info, list, MDUMP_WRITE);

    String_Builder builder = {&arena.allocator};
    mdump_write(&builder, &dump, root, STRING("dump1.mdump"), STRING("test schema"));



}
