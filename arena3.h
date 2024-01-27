#pragma once

#include "lib/allocator.h"

#define STACK_BLOCK_SIZE 32

typedef struct Arena_Stack_Block {
    struct Arena_Stack_Block* prev;
    isize stack[STACK_BLOCK_SIZE];
} Arena_Stack_Block;

typedef struct Arena3 {
    i32 stack_depht;
    i32 num_blocks;

    Arena_Stack_Block* stack_top;
    isize used_to;

    u8* data;
    isize size;
} Arena3;


void arena3_push_stack_block(Arena3* arena, i32 to_level, bool first)
{
    if(arena->stack_depht >= to_level && !first)
        return;

    while(arena->num_blocks*STACK_BLOCK_SIZE <= to_level || arena->num_blocks < 1)
    {
        Arena_Stack_Block* block = (Arena_Stack_Block*) align_forward(arena->data + arena->used_to, sizeof(isize));
        isize used_to_after = (u8*) block + sizeof(Arena_Stack_Block) - arena->data;
        if(used_to_after > arena->size)
        {
            //LOG_ERROR("arena", "Arena out of memory: using " MEMORY_FMT " out of " MEMORY_FMT, MEMORY_PRINT(arena->used_to), MEMORY_PRINT(arena->size));
            return;
        }

        block->prev = arena->stack_top;
        for(i32 i = 0; i < STACK_BLOCK_SIZE; i++)
            block->stack[i] = used_to_after;

        arena->num_blocks += 1;
        arena->stack_top = block;
        arena->used_to = used_to_after;
    }

    ASSERT(arena->stack_top != NULL);
    i32 remaining_depth = to_level % STACK_BLOCK_SIZE;
    for(isize i = 0; i < remaining_depth; i++)
        arena->stack_top->stack[i] = arena->used_to;
        
    arena->stack_depht = to_level;
    //LOG_DEBUG("arena", "Stack size grown to: %d blocks: %d", arena->stack_depht, arena->num_blocks);
}

isize arena3_pop_stack_block(Arena3* arena, i32 to_level)
{
    to_level = MAX(to_level, 0);
    if(arena->stack_depht <= to_level)
        return arena->used_to;

    while(arena->num_blocks*STACK_BLOCK_SIZE > to_level && arena->num_blocks > 1)
    {
        arena->stack_top = arena->stack_top->prev;
        arena->num_blocks -= 1;
    }

    isize out = arena->stack_top->stack[to_level % STACK_BLOCK_SIZE];
    arena->stack_depht = to_level;
    //LOG_DEBUG("arena", "Stack size shrinked to: %d blocks: %d", arena->stack_depht, arena->num_blocks);
    return out;
}

void arena3_init(Arena3* arena, void* data, isize size)
{
    memset(arena, 0, sizeof *arena);

    arena->data = (u8*) data;
    arena->size = size;

    arena3_push_stack_block(arena, 0, true);
}

void* arena3_push(Arena3* arena, i32 depth, isize size, isize align)
{
    ASSERT(depth >= 0);
    if(arena->stack_depht != depth)
    {
        arena3_push_stack_block(arena, depth, false);
        arena3_pop_stack_block(arena, depth);
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

void arena3_pop(Arena3* arena, i32 depth)
{

    isize new_used_to = arena3_pop_stack_block(arena, depth - 1);
    ASSERT(arena->used_to >= new_used_to);
    arena->used_to = new_used_to;
}

i32 arena3_get_level(Arena3* arena)
{
    i32 depth = arena->stack_depht + 1;
    arena3_push_stack_block(arena, depth, false);
    return depth;
}

char* arena3_push_string(Arena3* arena, i32 level, const char* string)
{
    isize len = string ? strlen(string) : 0;   
    char* pat1 = (char*) arena3_push(arena, level, len + 1, 1);
    if(pat1 != NULL)
    {
        memcpy(pat1, string, len);
        pat1[len] = 0;
    }

    return pat1;
}


void test_arena3(f64 time)
{
    (void) time;
    Arena3 arena = {0};
    void* block = malloc(KIBI_BYTE * 3/4);

    arena3_init(&arena, block, KIBI_BYTE * 3/4);
    
    #define PATTERN1 ">HelloWorld(Pattern1)"
    #define PATTERN2 ">GoodbyeWorld(Pattern2)"
    #define PATTERN3 ">****(Pattern3)"

    i32 level1 = arena3_get_level(&arena);
    {
        char* pat1 = arena3_push_string(&arena, level1, PATTERN1);

        i32 level2 = arena3_get_level(&arena) + 64;
        {
            char* pat2 = arena3_push_string(&arena, level2, PATTERN2);

            i32 level3 = level2 + 10;
            {
                char* pat3 = arena3_push_string(&arena, level3, PATTERN3); (void) pat3;
                pat1 = arena3_push_string(&arena, level1, PATTERN1);
            }
            arena3_pop(&arena, level3);

            TEST(memcmp(pat2, PATTERN2, sizeof PATTERN2 - 1) == 0);
        }

        TEST(memcmp(pat1, PATTERN1, sizeof PATTERN1 - 1) == 0);
        //! No free
    }
    arena3_pop(&arena, level1);
}