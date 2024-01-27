#pragma once

#include "lib/allocator.h"

#define ARENA_DEBUG
#define ARENA_MAGIC "ArnHead"

typedef struct Arena_Level {
    u32 prev_offset_x8;
    i32 level;
    
    #ifdef ARENA_DEBUG
    u8 magic[sizeof(ARENA_MAGIC)];
    #endif
} Arena_Level;

typedef struct Arena {
    u8* data;
    isize size;
    isize used_to;
    isize last_level_i;
    i32 last_level;
} Arena;

void arena_init(Arena* arena, void* data, isize data_size)
{
    memset(arena, 0, sizeof *arena);
    arena->data = (u8*) data;
    arena->size = data_size;
    
    #ifdef ARENA_DEBUG
        memset(arena->data, 0x55, data_size);
    #endif
}

isize arena_pop_level(Arena* arena, i32 to_level)
{
    if(to_level <= 0)
    {
        arena->last_level_i = 0;
        arena->last_level -= 1;
        return 0;
    }

    isize last_popped_i = arena->last_level_i;    
    while(arena->last_level >= to_level)
    {
        Arena_Level* level = (Arena_Level*) (arena->data + arena->last_level_i);
        ASSERT(level != NULL);
        #ifdef ARENA_DEBUG
            ASSERT(memcmp(level->magic, ARENA_MAGIC, sizeof ARENA_MAGIC) == 0);
        #endif
        ASSERT(level->level == arena->last_level);

        last_popped_i = arena->last_level_i;
        arena->last_level_i -= (isize) level->prev_offset_x8 * 8;
        arena->last_level -= 1;

        ASSERT(arena->last_level_i >= 0);
        ASSERT(arena->last_level >= 0);
        ASSERT(arena->used_to >= 0);
    }

    return last_popped_i;
}

void arena_push_level(Arena* arena, i32 to_level)
{
    ASSERT(to_level >= 0);
    if(arena->last_level >= to_level)
        return;

    //@TODO: make better remove goto
    u8* aligned = (u8*) align_forward(arena->data + arena->used_to, 8);
    isize new_used_to = aligned - arena->data;

    if(new_used_to > arena->size)
        goto out_of_memory;

    arena->used_to = new_used_to;    
    while(arena->last_level < to_level)
    {
        Arena_Level* level = (Arena_Level*) (arena->data + arena->used_to);
        ASSERT_MSG(align_forward(level, 8) == level, "Should be already aligned!"); 

        if(arena->used_to + (isize) sizeof(Arena_Level) > arena->size)
            goto out_of_memory;

        level->level = arena->last_level + 1;
        level->prev_offset_x8 = (u32) ((arena->used_to - arena->last_level_i) / 8);
        #ifdef ARENA_DEBUG
        memcpy(level->magic, ARENA_MAGIC, sizeof ARENA_MAGIC);
        #endif

        arena->last_level_i = arena->used_to;
        arena->last_level += 1;
        arena->used_to += sizeof(Arena_Level);
        
        ASSERT(arena->last_level_i >= 0);
        ASSERT(arena->last_level >= 0);
        ASSERT(arena->used_to >= 0);
    }

    return;

    out_of_memory:
        LOG_ERROR("arena", "Arena out of memory: using " MEMORY_FMT " out of " MEMORY_FMT, MEMORY_PRINT(arena->used_to), MEMORY_PRINT(arena->size));
}

void* arena_push(Arena* arena, i32 level, isize size, isize align)
{
    ASSERT(level >= 0);
    ASSERT(size >= 0);
    ASSERT(is_power_of_two(align));

    if(arena->last_level != level)
    {
        arena_pop_level(arena, level + 1);
        arena_push_level(arena, level);
    }

    u8* data = (u8*) align_forward(arena->data + arena->used_to, align);
    u8* data_end = data + size;
    if(data_end > arena->data + arena->size)
    {
        LOG_ERROR("arena", "Arena out of memory: using " MEMORY_FMT " out of " MEMORY_FMT, MEMORY_PRINT(arena->used_to), MEMORY_PRINT(arena->size));
        return NULL;
    }

    arena->used_to = (isize) data_end - (isize) arena->data;
    return data;
}

void arena_pop(Arena* arena, i32 level)
{
    if(arena->last_level < level)
        return;

    ASSERT(level >= 0);
    isize used_to_be_last = arena_pop_level(arena, level);

    //Now the current level should be at the furthest position. We simply cut it off.
    ASSERT(used_to_be_last <= arena->used_to);
    ASSERT(arena->used_to >= 0);
    
    //Debug only: We fill the popped space with 0x55
    #ifdef ARENA_DEBUG
    memset(arena->data + used_to_be_last, 0x55, arena->used_to - used_to_be_last);
    #endif
    arena->used_to = used_to_be_last;
}

i32 arena_get_level(Arena* arena)
{
    i32 level = arena->last_level + 1;
    arena_push_level(arena, level);
    return level;
}

char* arena_push_string(Arena* arena, i32 level, const char* string)
{
    isize len = string ? strlen(string) : 0;   
    char* pat1 = (char*) arena_push(arena, level, len + 1, 1);
    memcpy(pat1, string, len);
    pat1[len] = 0;

    return pat1;
}

void test_arena(f64 time)
{
    (void) time;
    Arena arena = {0};
    void* block = malloc(MEBI_BYTE);

    arena_init(&arena, block, MEBI_BYTE);
    
    #define PATTERN1 ">HelloWorld(Pattern1)"
    #define PATTERN2 ">GoodbyeWorld(Pattern2)"
    #define PATTERN3 ">****(Pattern3)"

    i32 level1 = 1;
    {
        char* pat1 = arena_push_string(&arena, level1, PATTERN1);

        i32 level2 = 4;
        {
            char* pat2 = arena_push_string(&arena, level2, PATTERN2);

            i32 level3 = level2 + 1;
            {
                char* pat3 = arena_push_string(&arena, level3, PATTERN3); (void) pat3;
                pat1 = arena_push_string(&arena, level1, PATTERN1);
            }
            arena_pop(&arena, level3);

            TEST(memcmp(pat2, PATTERN2, sizeof PATTERN2 - 1) == 0);
        }

        TEST(memcmp(pat1, PATTERN1, sizeof PATTERN1 - 1) == 0);
        //! No free
    }
    arena_pop(&arena, level1);

    TEST(arena.used_to < sizeof PATTERN1 - 1);
}