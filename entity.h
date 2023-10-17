#include "defines.h"
#include "hash.h"
#include "platform.h"
#include "string.h"
#include "log.h"

// This is a very very simple entity compoment system.
// It is focuses on minimalizing the ammount of state in the system. 
// We store the compoments for each entity inside the entity itself as a simple bitfield.
// This is good because it saves us one lookup while destroying the entity.


#define MAX_SYSTEMS 64
#define COMPOMENT_NAME_MAX 64
#define ENTITY_EPHEMERAL_SLOTS 64

typedef struct Uid {
    i64 epoch_time;
    u32 counter;
    u32 thread_id;
} Uid;

typedef u64 Compoment_Mask;

typedef struct Entity {
    Uid id;
    Compoment_Mask compoments;
} Entity;

bool uid_is_null(Uid id)
{
    return id.thread_id == 0;
}

bool entity_is_null(Entity entity)
{
    return uid_is_null(entity.id);
}

Uid uid_generate()
{
    static u32 counter = 0;

    Uid id = {0};
    id.thread_id = platform_thread_get_id() + 1;
    id.epoch_time = platform_universal_epoch_time();
    id.counter = platform_interlocked_increment32((volatile i32*) &counter);
    return id;
}

Entity 

u64 uid_hash64(Uid id)
{
    u64 casted[2] = {0};
    memcpy(casted, &id, sizeof casted);

    u64 hashed = hash64(casted[0]) ^ hash64(casted[1]);
    return hashed;
}

u32 uid_hash32(Uid id)
{
    u64 casted[2] = {0};
    memcpy(casted, &id, sizeof casted);

    u32 hashed = hash64_to32(casted[0]) ^ hash64_to32(casted[1]);
    return hashed;
}

Compoment_Mask compoment_add(Compoment_Mask to, i8 system)
{
    CHECK_BOUNDS(system, MAX_SYSTEMS);
    return to | ((Compoment_Mask) 1 << system);
}

Compoment_Mask entity_get_compoments(Entity entity)
{
    return entity.compoments;
}

typedef void(*Compoment_System_Notify)(Uid id, bool added_if_true_removed_if_false, void* context); 

typedef struct Compoment_System {
    char name[COMPOMENT_NAME_MAX + 1];
    Compoment_System_Notify notify;
    void* context;
    isize alive_compoment_count;
    isize addition_count;
    isize removal_count;
};

Compoment_System systems[MAX_SYSTEMS] = {0};
Compoment_Mask default_mask = 0;

void compoment_system_remove(i8 system_bit_index)
{
    CHECK_BOUNDS(system_bit_index, MAX_SYSTEMS);
    memset(&systems[system_bit_index], 0, sizeof systems[system_bit_index]);
}

void compoment_system_add(String name, i8 system_bit_index, Compoment_System_Notify notify, void* context)
{
    CHECK_BOUNDS(system_bit_index, MAX_SYSTEMS);
    ASSERT(notify != NULL);

    if(systems[system_bit_index].notify)
    {
        LOG_ERROR("ENTITY", "overiding previously made system %s", systems[system_bit_index].name);
        compoment_system_remove(system_bit_index);
    }

    memmove(systems[system_bit_index].name, name.data, MIN(sizeof COMPOMENT_NAME_MAX, name.size));
    systems[system_bit_index].context = context;
    systems[system_bit_index].notify = notify;
}


void entity_set_compoments(Entity* entity, Compoment_Mask to)
{
    Compoment_Mask from = entity->compoments;
    Compoment_Mask delta = from ^ to;

    for(isize i = 0; i < MAX_SYSTEMS; i++)
    {
        if(delta == 0)
            break;

        Compoment_Mask compoment_bit = (Compoment_Mask) 1 << i;
        Compoment_System* system = &systems[i];
        if((delta & compoment_bit) && system->notify)
        {
            bool is_make = !!(to & compoment_bit);
            if(is_make)
            {
                system->alive_compoment_count += 1;
                system->addition_count += 1;
            }
            else
            {
                system->alive_compoment_count -= 1;
                system->removal_count += 1;
            }

            system->notify(entity->id, is_make, system->context);

            //Clear this bit
            delta &= ~compoment_bit;
        }
    }

    entity->compoments = to;
}

void entity_deinit(Entity* entity)
{
    entity_set_compoments(entity, 0);
}






