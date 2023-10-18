#ifndef LIB_ENTITY
#define LIB_ENTITY

#include "defines.h"
#include "platform.h"
#include "string.h"
#include "log.h"

#include "guid.h"

// This is a very very simple entity compoment system.
// It is focuses on minimalizing the ammount of state in the system. 
// We store the compoments for each entity inside the entity itself as a simple bitfield.
// This is good because it saves us one lookup while creating/destroying the entity.

#define MAX_SYSTEMS 64
#define COMPOMENT_NAME_MAX 64

typedef u64 Compoment_Mask;

typedef struct Entity {
    Guid id;
    Compoment_Mask compoments;
} Entity;

typedef void(*Compoment_System_Notify)(Guid id, bool added_if_true_removed_if_false, void* context); 

EXPORT Compoment_Mask entity_get_compoments(Entity entity);
EXPORT Compoment_Mask entity_set_compoments(Entity* entity, Compoment_Mask to);
EXPORT Entity         entity_generate(Compoment_Mask compoments);
EXPORT void           entity_remove(Entity* entity);

EXPORT Compoment_Mask compoment_add(Compoment_Mask to, i8 system);
EXPORT Compoment_Mask compoment_system_get_default();
EXPORT Compoment_Mask compoment_system_set_default(Compoment_Mask compoments);

EXPORT void compoment_system_remove(i8 system_bit_index);
EXPORT void compoment_system_add(String name, i8 system_bit_index, Compoment_System_Notify notify, void* context);

#endif

#if (defined(LIB_ALL_IMPL) || defined(LIB_ENTITY_IMPL)) && !defined(LIB_ENTITY_HAS_IMPL)
#define LIB_ENTITY_HAS_IMPL

EXPORT Compoment_Mask compoment_add(Compoment_Mask to, i8 system)
{
    CHECK_BOUNDS(system, MAX_SYSTEMS);
    return to | ((Compoment_Mask) 1 << system);
}

EXPORT Compoment_Mask entity_get_compoments(Entity entity)
{
    return entity.compoments;
}

typedef struct Compoment_System {
    char name[COMPOMENT_NAME_MAX + 1];
    Compoment_System_Notify notify;
    void* context;
    isize alive_compoment_count;
    isize addition_count;
    isize removal_count;
} Compoment_System;

EXPORT Compoment_System _compoment_systems[MAX_SYSTEMS] = {0};
EXPORT Compoment_Mask _compoments_default = 0;

EXPORT Compoment_Mask compoment_system_set_default(Compoment_Mask compoments)
{
    Compoment_Mask prev = _compoments_default;
    _compoments_default = compoments;
    return prev;
}

EXPORT Compoment_Mask compoment_system_get_default()
{
    return _compoments_default;
}

EXPORT void compoment_system_remove(i8 system_bit_index)
{
    CHECK_BOUNDS(system_bit_index, MAX_SYSTEMS);
    memset(&_compoment_systems[system_bit_index], 0, sizeof _compoment_systems[system_bit_index]);
}

EXPORT void compoment_system_add(String name, i8 system_bit_index, Compoment_System_Notify notify, void* context)
{
    CHECK_BOUNDS(system_bit_index, MAX_SYSTEMS);
    ASSERT(notify != NULL);

    if(_compoment_systems[system_bit_index].notify)
    {
        LOG_ERROR("ENTITY", "overiding previously made system %s", _compoment_systems[system_bit_index].name);
        compoment_system_remove(system_bit_index);
    }

    memmove(_compoment_systems[system_bit_index].name, name.data, MIN(sizeof COMPOMENT_NAME_MAX, name.size));
    _compoment_systems[system_bit_index].context = context;
    _compoment_systems[system_bit_index].notify = notify;
}

EXPORT Compoment_Mask entity_set_compoments(Entity* entity, Compoment_Mask to)
{
    Compoment_Mask prev = entity->compoments;
    Compoment_Mask from = entity->compoments;
    Compoment_Mask delta = from ^ to;

    for(isize i = 0; i < MAX_SYSTEMS; i++)
    {
        if(delta == 0)
            break;

        Compoment_Mask compoment_bit = (Compoment_Mask) 1 << i;
        Compoment_System* system = &_compoment_systems[i];
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
    return prev;
}

EXPORT Entity entity_generate(Compoment_Mask compoments)
{
    Entity out = {0};
    Compoment_Mask all_compoments = compoments | compoment_system_set_default();
    out.id = guid_generate();

    entity_set_compoments(&out, all_compoments);
    return out;
}

EXPORT void entity_remove(Entity* entity)
{
    entity_set_compoments(entity, 0);
}

#endif