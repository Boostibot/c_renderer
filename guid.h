#ifndef LIB_GUID
#define LIB_GUID

#include "defines.h"
#include "platform.h"

//64 bit globally-unique-identifier
typedef enum {__GUID_VAL__ = 0}* Guid;  

//We use pointer to a custom type for three reasons:
//  1: pointers are comparable with =, <, >, ... (unlike structs)
//  2: type is distinct and thus type safe (unlike typedef u64 Guid)
//  3: pointers are castable (Guid guid = (Guid) index)

EXPORT Guid guid_generate();

#endif

#if (defined(LIB_ALL_IMPL) || defined(LIB_GUID_IMPL)) && !defined(LIB_GUID_HAS_IMPL)
#define LIB_GUID_HAS_IMPL

EXPORT Guid guid_generate()
{
    static i64 counter = 0;
    Guid id = (Guid) platform_interlocked_increment64(&counter);
    return id;
}

#endif