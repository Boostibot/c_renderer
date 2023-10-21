#pragma once


#include "hash_index.h"
#include "array.h"
#include "entity.h"
#include "stable_array2.h"
#include "resources.h"
#include "systems.h"

//We can exploit granularity to allocate blocks of nice size

typedef struct Redender_Compoment {
    Id entity;
    Triangle_Mesh_Ref mesh;
    Transform transform;
    i32 layer;
} Redender_Compoment;

typedef struct Render_World {
    Hash_Ptr hash;
    Stable_Array compoments;
} Render_World;

extern Render_World global_render_world;

void render_world_init(Allocator* alloc);
void render_world_deinit();
Redender_Compoment* render_compoment_get_id(Id id);
Redender_Compoment* render_compoment_add_id(Id id, Triangle_Mesh_Ref mesh, Transform transform, i32 layer);
bool render_compoment_remove_id(Id id);

Render_World global_render_world;

void render_world_notify(Id id, bool added_if_true_removed_if_false, void* context)
{
    (void) context;
    Triangle_Mesh_Ref mesh = {0};
    Transform transf = {0};
    if(added_if_true_removed_if_false)
        render_compoment_add_id(id, mesh, transf, 0);
    else 
        render_compoment_remove_id(id);
}

void render_world_init(Allocator* alloc)
{
    hash_ptr_init(&global_render_world.hash, alloc);
    stable_array_init(&global_render_world.compoments, alloc, sizeof(Redender_Compoment));
    compoment_system_add(STRING("Render world"), SYSTEM_RENDER, render_world_notify, NULL);
}
void render_world_deinit()
{
    hash_ptr_deinit(&global_render_world.hash);
    stable_array_deinit(&global_render_world.compoments);
    compoment_system_remove(SYSTEM_RENDER);
}

Redender_Compoment* render_compoment_get_id(Id id)
{
    isize found = hash_ptr_find(global_render_world.hash, (u64) id);
    if(found == -1)
        return NULL;

    u64 index = global_render_world.hash.entries[found].value;
    Redender_Compoment* comp = (Redender_Compoment*) stable_array_at(&global_render_world.compoments, index - 1);
    return comp;
}

Redender_Compoment* render_compoment_add_id(Id id, Triangle_Mesh_Ref mesh, Transform transform, i32 layer)
{
    isize found = hash_ptr_find_or_insert(&global_render_world.hash, (u64) id, (u64) 0);
    Redender_Compoment* comp = NULL;
    u64* index = &global_render_world.hash.entries[found].value;
    if(*index == (u64) 0)
        *index = stable_array_insert(&global_render_world.compoments, (void**) &comp) + 1;
    else
        comp = (Redender_Compoment*) stable_array_at(&global_render_world.compoments, *index);

    comp->entity = id;
    comp->layer = layer;
    comp->mesh = mesh;
    comp->transform = transform;
    return comp;
}

bool render_compoment_remove_id(Id id)
{
    isize found = hash_ptr_find(global_render_world.hash, (u64) id);
    if(found == -1)
        return false;

    Hash_Ptr_Entry removed_entry = hash_ptr_remove(&global_render_world.hash, found);
    return stable_array_remove(&global_render_world.compoments, removed_entry.value - 1);
}

Redender_Compoment* render_compoment_get(Entity entity)
{
    if(entity.compoments & compoment_bit(SYSTEM_RENDER))
        return render_compoment_get_id(entity.id);

    return NULL;
}

Redender_Compoment* render_compoment_add(Entity* entity, Triangle_Mesh_Ref mesh, Transform transform, i32 layer)
{
    entity->compoments |= compoment_bit(SYSTEM_RENDER);
    return render_compoment_add_id(entity->id, mesh, transform, layer);
}

bool render_compoment_remove(Entity* entity)
{
    if(entity->compoments & compoment_bit(SYSTEM_RENDER))
    {
        entity->compoments &= ~compoment_bit(SYSTEM_RENDER);
        return render_compoment_remove_id(entity->id);
    }

    return false;
}
