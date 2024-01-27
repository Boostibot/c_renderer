//#pragma once
//#include "lib/allocator.h"

#include <stdint.h>
#include <stdbool.h>
#include <string.h>

typedef int64_t isize;
typedef int64_t i64;
typedef int32_t i32;
typedef uint64_t u64;
typedef uint32_t u32;
typedef uint8_t u8;

#ifndef ASSERT
#include "assert.h"
#define ASSERT(x) assert(x)
#define ASSERT_MSG(x, format, ...) assert(x && format)

#define ARENA_NAKED

#define MIN(a, b)   ((a) < (b) ? (a) : (b))
#define MAX(a, b)   ((a) > (b) ? (a) : (b))
#define CLAMP(value, low, high) MAX(low, MIN(value, high))
#define DIV_ROUND_UP(value, div_by) (((value) + (div_by) - 1) / (div_by))
#define MODULO(val, range) (((val) % (range) + (range)) % (range))


#if defined(_MSC_VER)
    #define MODIFIER_NO_INLINE  __declspec(noinline)
#elif defined(__GNUC__) || defined(__clang__)
    #define MODIFIER_NO_INLINE  __attribute__((noinline))
#else
    #define MODIFIER_NO_INLINE
#endif
#endif

typedef isize (*Arena_Commit_Func)(void* addr, isize size, isize reserved_size, bool commit);

#define ARENA_DEF_STACK_SIZE   64
#define ARENA_DEF_RESERVE_SIZE 64 * GIBI_BYTE
#define ARENA_DEF_COMMIT_SIZE  8 * MEBI_BYTE

//We need to be explicit about stack data memory locations if we are to use virtual memory!
//That means [STACK][DATA]
// so we have just one ptr data and other end stack offset. we ensure that used_to >= stack offset

typedef struct Arena {
    i32 stack_depht;
    i32 stack_max_depth;
    isize used_to;

    u8* data;
    isize size;

    isize reserved_size;
    Arena_Commit_Func commit;
} Arena;

#ifdef NDEBUG
#define ARENA_DEBUG 0
#else
#define ARENA_DEBUG 1
#endif
#define ARENA_DEBUG_DATA_PATTERN 0x55
#define ARENA_DEBUG_DATA_SIZE 64
#define ARENA_DEBUG_STACK_PATTERN 0x66

void _arena_debug_check_invarinats(Arena* arena);
void _arena_debug_fill_stack(Arena* arena);
void _arena_debug_fill_data(Arena* arena, isize size);
void* _align_backward(const void* data, isize align);
void* _align_forward(const void* data, isize align);

void arena_deinit(Arena* arena)
{
    _arena_debug_check_invarinats(arena);
    if(arena->commit && arena->data)
       arena->commit(arena->data, arena->size, arena->reserved_size, false);
       
    memset(arena, 0, sizeof *arena);
}
void arena_init_custom(Arena* arena, void* data, isize size, isize reserved_size, Arena_Commit_Func commit_or_null, isize stack_max_depth_or_zero)
{
    arena_deinit(arena);
    isize stack_max_depth = stack_max_depth_or_zero;

    isize* stack = (isize*) data;
    u8* aligned_data = (u8*) _align_forward(data, sizeof *stack);
    isize aligned_size = size - (aligned_data - (u8*) data);
    isize stack_max_depth_that_fits = aligned_size / sizeof *stack;

    if(stack_max_depth <= 0)
        stack_max_depth = ARENA_DEF_STACK_SIZE;
    if(stack_max_depth > stack_max_depth_that_fits)
        stack_max_depth = stack_max_depth_that_fits;

    arena->used_to = stack_max_depth * sizeof *stack;
    arena->data = aligned_data;
    arena->size = aligned_size;
    arena->stack_max_depth = (i32) stack_max_depth;
    arena->commit = commit_or_null;
    arena->reserved_size = reserved_size;

    _arena_debug_fill_stack(arena);
    _arena_debug_fill_data(arena, INT64_MAX);
    _arena_debug_check_invarinats(arena);
}

#ifndef ARENA_NAKED
isize arena_def_commit_func(void* addr, isize size, isize reserved_size, bool commit)
{
    isize commit_to = 0;
    if(commit == false)
    {
        platform_virtual_reallocate((u8*) addr, reserved_size, PLATFORM_VIRTUAL_ALLOC_RELEASE, PLATFORM_MEMORY_PROT_READ_WRITE);
    }
    else
    {
        commit_to = MIN(size + ARENA_DEF_COMMIT_SIZE, reserved_size);
        if(commit_to - size > 0)
            platform_virtual_reallocate((u8*) addr + size, commit_to - size, PLATFORM_VIRTUAL_ALLOC_COMMIT, PLATFORM_MEMORY_PROT_READ_WRITE);

    }
    return commit_to;
}

void arena_init(Arena* arena, isize reserve_size_or_zero, isize stack_max_depth_or_zero)
{
    if(reserve_size_or_zero <= 0)
        reserve_size_or_zero = ARENA_DEF_RESERVE_SIZE;

    void* data = platform_virtual_reallocate(NULL, reserve_size_or_zero, PLATFORM_VIRTUAL_ALLOC_RESERVE, PLATFORM_MEMORY_PROT_READ_WRITE);
    isize size = arena_def_commit_func(data, 0, reserve_size_or_zero, true);
    arena_init_custom(arena, data, size, reserve_size_or_zero, arena_def_commit_func, stack_max_depth_or_zero);
}
#endif

MODIFIER_NO_INLINE void* arena_unusual_push(Arena* arena, i32 depth, isize size, isize align)
{
    if(arena->stack_depht != depth)
    {
        ASSERT(arena->stack_depht < arena->stack_max_depth);

        i32 to_depth = MIN(depth, arena->stack_max_depth);
        isize* stack = (isize*) (void*) arena->data;
        for(i32 i = arena->stack_depht; i < to_depth; i++)
            stack[i] = arena->used_to;

        arena->stack_depht = to_depth;
        _arena_debug_fill_stack(arena);
        _arena_debug_check_invarinats(arena);
    }

    u8* data = (u8*) _align_forward(arena->data + arena->used_to, align);
    u8* data_end = data + size;
    if(data_end > arena->data + arena->size)
    {
        if(arena->commit)
        {
            isize new_size = arena->commit(arena->data, arena->size, arena->reserved_size, true);
            if(new_size > arena->size)
            {
                _arena_debug_fill_data(arena, new_size - arena->size);
                arena->size = new_size;
                ASSERT(new_size < arena->reserved_size);
                return arena_unusual_push(arena, depth, size, align);
            }
        }

        return NULL;
    }
    
    arena->used_to = (isize) data_end - (isize) arena->data;
    _arena_debug_check_invarinats(arena);
    return data;
}

void* arena_push(Arena* arena, i32 depth, isize size, isize align)
{
    _arena_debug_check_invarinats(arena);
    ASSERT(depth > 0);
    if(arena->stack_depht != depth || arena->used_to + size + align > arena->size)
        return arena_unusual_push(arena, depth, size, align);
    
    u8* data = (u8*) _align_forward(arena->data + arena->used_to, align);
    arena->used_to = (isize) (data + size) - (isize) arena->data;
    ASSERT(arena->used_to <= arena->size);
    
    return data;
}

void arena_pop(Arena* arena, i32 depth)
{
    ASSERT(depth > 0);
    _arena_debug_check_invarinats(arena);
    if(arena->stack_depht < depth)
        return;

    isize* stack = (isize*) (void*) arena->data;
    isize new_used_to = stack[depth - 1];

    ASSERT(arena->stack_max_depth * (isize) sizeof *stack <= new_used_to && new_used_to <= arena->used_to);
    arena->used_to = new_used_to;
    arena->stack_depht = depth;

    _arena_debug_fill_data(arena, ARENA_DEBUG_DATA_SIZE);
    _arena_debug_check_invarinats(arena);
}

i32 arena_get_level(Arena* arena)
{
    _arena_debug_check_invarinats(arena);
    i32 new_depth = arena->stack_depht + 1;
    isize* stack = (isize*) (void*) arena->data;
    if(new_depth <= arena->stack_max_depth)
    {
        stack[new_depth - 1] = arena->used_to;
        arena->stack_depht = new_depth;
    }

    return new_depth;
}

char* arena_push_string(Arena* arena, i32 level, const char* string)
{
    isize len = string ? strlen(string) : 0;   
    char* pat1 = (char*) arena_push(arena, level, len + 1, 1);
    memcpy(pat1, string, len);
    pat1[len] = 0;

    return pat1;
}

int memcmp_byte(const void* ptr, int byte, isize size)
{
    isize i = 0;
    char* text = (char*) ptr;

    if((isize) ptr % 8 == 0)
    {
        //pattern is 8 repeats of byte
        u64 pattern = 0x0101010101010101ULL * (u64) byte;
        for(isize k = 0; k < size/8; k++)
            if(*(u64*) ptr != pattern)
                return (int) (k*8);
        
        i = size/8*8;
    }

    for(; i < size; i++)
        if(text[i] != (char) byte)
            return (int) i;
        
    return 0;
}

void _arena_debug_check_invarinats(Arena* arena)
{
    if(ARENA_DEBUG)
    {
        isize* stack = (isize*) (void*) arena->data;
        ASSERT_MSG(arena->stack_max_depth * (isize)sizeof *stack <= arena->used_to && arena->used_to <= arena->size, "used_to needs to be within [0, size]");
        ASSERT_MSG(0 <= arena->stack_depht && arena->stack_depht <= arena->stack_max_depth, "used_to stack_depht to be within [0, stack_max_depth]");

        ASSERT_MSG((arena->data == NULL) == (arena->size == 0), "only permitted to be NULL when zero sized");

        isize till_end = arena->size - arena->used_to;
        isize check_size = MIN(till_end, ARENA_DEBUG_DATA_SIZE);
        ASSERT_MSG(memcmp_byte(arena->data + arena->used_to, ARENA_DEBUG_DATA_PATTERN, check_size) == 0, "The memory after the arena needs not be corrupted!");
        ASSERT_MSG(memcmp_byte(stack + arena->stack_depht, ARENA_DEBUG_STACK_PATTERN, (arena->stack_max_depth - arena->stack_depht) * sizeof *stack) == 0, "The memory after stack needs to be valid");
    }
}

void _arena_debug_fill_stack(Arena* arena)
{
    isize* stack = (isize*) (void*) arena->data;
    if(ARENA_DEBUG)
        memset(stack + arena->stack_depht, ARENA_DEBUG_STACK_PATTERN, (arena->stack_max_depth - arena->stack_depht) * sizeof *stack);
}

void _arena_debug_fill_data(Arena* arena, isize size)
{
    if(ARENA_DEBUG)
    {
        isize till_end = arena->size - arena->used_to;
        isize check_size = MIN(till_end, size);
        memset(arena->data + arena->used_to, ARENA_DEBUG_DATA_PATTERN, check_size);
    }
}

void* _align_backward(const void* data, isize align)
{
    u64 mask = (u64) (align - 1);
    u64 aligned = (u64) data & ~mask;
    return (void*) aligned;
}

void* _align_forward(const void* data, isize align)
{
    return _align_backward((u8*) data + align - 1, align);
}

#ifndef ARENA_NAKED
void test_arena(f64 time)
{
    (void) time;
    Arena arena = {0};

    arena_init(&arena, 0, 0);
    
    #define PATTERN1 ">HelloWorld(Pattern1)"
    #define PATTERN2 ">GoodbyeWorld(Pattern2)"
    #define PATTERN3 ">****(Pattern3)"

    i32 level1 = arena_get_level(&arena);
    {
        char* pat1 = arena_push_string(&arena, level1, PATTERN1);

        i32 level2 = arena_get_level(&arena);
        {
            char* pat2 = arena_push_string(&arena, level2, PATTERN2);

            i32 level3 = level2 + 10;
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
}
#endif