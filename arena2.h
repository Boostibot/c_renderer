#pragma once

#include "lib/allocator.h"

typedef struct Arena2 {
    i32 stack_depht;
    i32 stack_max_depth;
    isize* stack;
    isize used_to;

    u8* data;
    isize size;
} Arena2;

void arena2_init(Arena2* arena, void* data, isize size, isize stack_max_depth_or_zero)
{
    memset(arena, 0, sizeof *arena);
    isize stack_max_depth = stack_max_depth_or_zero;

    u8* aligned_data = (u8*) align_forward(data, sizeof(isize));
    isize aligned_size = size - (aligned_data - (u8*) data);
    isize stack_max_depth_that_fits = aligned_size / sizeof(isize);

    if(stack_max_depth <= 0)
        stack_max_depth = 64;
    if(stack_max_depth > stack_max_depth_that_fits)
        stack_max_depth = stack_max_depth_that_fits;

    u8* remaining_data = aligned_data + stack_max_depth * sizeof(isize);
    isize remaining_size = size - (remaining_data - (u8*) data);
    ASSERT(remaining_size >= 0);

    arena->data = remaining_data;
    arena->size = remaining_size;
    arena->stack = (isize*) aligned_data;
    arena->stack_max_depth = (i32) stack_max_depth;
}

void* arena2_push(Arena2* arena, i32 depth, isize size, isize align)
{
    ASSERT(depth >= 0);
    if(arena->stack_depht != depth)
    {
        ASSERT(arena->stack_depht <= arena->stack_max_depth);

        i32 to_depth = MIN(depth, arena->stack_max_depth);
        for(i32 i = arena->stack_depht; i < to_depth; i++)
            arena->stack[i] = arena->used_to;

        arena->stack_depht = to_depth;
    }
    
    u8* data = (u8*) align_forward(arena->data + arena->used_to, align);
    u8* data_end = data + size;
    if(data_end > arena->data + arena->size)
    {
        //LOG_ERROR("arena", "Arena out of memory: using " MEMORY_FMT " out of " MEMORY_FMT, MEMORY_PRINT(arena->used_to), MEMORY_PRINT(arena->size));
        return NULL;
    }

    arena->used_to = (isize) data_end - (isize) arena->data;
    return data;
}

void arena2_pop(Arena2* arena, i32 depth)
{
    if(arena->stack_depht < depth)
        return;

    isize new_used_to = 0;
    if(depth > 0)
        new_used_to = arena->stack[depth - 1];

    ASSERT(arena->used_to >= new_used_to);
    arena->used_to = new_used_to;
    arena->stack_depht = MAX(depth, 0);
}

i32 arena2_get_level(Arena2* arena)
{
    i32 new_depth = arena->stack_depht + 1;
    if(new_depth > arena->stack_max_depth)
    {
        //LOG_ERROR("arena", "depth %d too much! Maximum %d", new_depth, arena->stack_max_depth);
        new_depth = arena->stack_max_depth;
    }

    arena->stack[new_depth - 1] = arena->used_to;
    arena->stack_depht = new_depth;
    return new_depth;
}

char* arena2_push_string(Arena2* arena, i32 level, const char* string)
{
    isize len = string ? strlen(string) : 0;   
    char* pat1 = (char*) arena2_push(arena, level, len + 1, 1);
    memcpy(pat1, string, len);
    pat1[len] = 0;

    return pat1;
}


void test_arena2(f64 time)
{
    (void) time;
    Arena2 arena = {0};
    void* block = malloc(MEBI_BYTE);

    arena2_init(&arena, block, MEBI_BYTE, 0);
    
    #define PATTERN1 ">HelloWorld(Pattern1)"
    #define PATTERN2 ">GoodbyeWorld(Pattern2)"
    #define PATTERN3 ">****(Pattern3)"

    i32 level1 = arena2_get_level(&arena);
    {
        char* pat1 = arena2_push_string(&arena, level1, PATTERN1);

        i32 level2 = arena2_get_level(&arena);
        {
            char* pat2 = arena2_push_string(&arena, level2, PATTERN2);

            i32 level3 = level2 + 10;
            {
                char* pat3 = arena2_push_string(&arena, level3, PATTERN3); (void) pat3;
                pat1 = arena2_push_string(&arena, level1, PATTERN1);
            }
            arena2_pop(&arena, level3);

            TEST(memcmp(pat2, PATTERN2, sizeof PATTERN2 - 1) == 0);
        }

        TEST(memcmp(pat1, PATTERN1, sizeof PATTERN1 - 1) == 0);
        //! No free
    }
    arena2_pop(&arena, level1);

    TEST(arena.used_to < sizeof PATTERN1 - 1);
}