#pragma once
#include "lib/string.h"
#include "lib/hash_string.h"
#include "lib/guid.h"

#define RESOURCE_NAME_SIZE 55

typedef struct Name {
    char data[RESOURCE_NAME_SIZE + 1];
    u64 hash;
} Name;

Name name_from_hashed(Hash_String hstring)
{
    Name name = {0};
    isize min_size = MIN(RESOURCE_NAME_SIZE, hstring.string.size);
    memset(name.data, 0, sizeof name.data);
    memmove(name.data, hstring.string.data, min_size);
    name.hash = hstring.hash;
    return name;
}

Name name_make(String str)
{
    return name_from_hashed(hash_string_make(str));
}

bool name_is_equal(const Name* a, const Name* b)
{
    if(a->hash != b->hash)
        return false;

    return strncmp(a->data, b->data, RESOURCE_NAME_SIZE);
}