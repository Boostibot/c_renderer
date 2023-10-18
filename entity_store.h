#pragma once

#include "entity.h"
#include "hash_index.h"



#define ENTITY_EPHEMERAL_SLOTS 64
typedef struct Entity_Store {

} Entity_Store;

typedef enum Entity_Duration {
    ENTITY_DURATION_MANAGED = 0,      //cleaned up when dereferenced
    ENTITY_DURATION_TIME,             //cleaned up when time is up
    ENTITY_DURATION_SINGLE_FRAME,     //cleaned up automatically when this frame ends
    ENTITY_DURATION_PERSISTANT,       //cleaned up when explicitly demanded
    ENTITY_DURATION_EPHEMERAL,        //cleaned up on some subsequent call to this function. Should be immediately used or coppied
} Entity_Duration;
