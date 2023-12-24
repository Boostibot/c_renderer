#pragma once
#include "lib/string.h"
#include "lib/hash_string.h"

#define RESOURCE_NAME_SIZE 55

typedef struct Name {
    u64 hash;
    char data[RESOURCE_NAME_SIZE + 1];
} Name;

bool name_from_hash_string(Name* name, Hash_String string)
{
    isize min_size = MIN(RESOURCE_NAME_SIZE, string.size);
    memset(name->data, 0, RESOURCE_NAME_SIZE + 1);
    memmove(name->data, string.data, min_size);
    name->hash = string.hash;
    return min_size == string.size;
}

bool name_from_string(Name* name, String string)
{
    return name_from_hash_string(name, hash_string_from_string(string));
}
    
bool name_from_cstring(Name* name, const char* str)
{
    return name_from_string(name, string_make(str));
}
    
bool name_from_builder(Name* name, String_Builder builder)
{
    return name_from_string(name, string_from_builder(builder));
}

bool name_is_equal(const Name* a, const Name* b)
{
    if(a->hash != b->hash)
        return false;

    return strncmp(a->data, b->data, RESOURCE_NAME_SIZE);
}
    
void name_copy(Name* to, const Name* from)
{
    to->hash = from->hash;
    memmove(to->data, from->data, sizeof to->data);
}