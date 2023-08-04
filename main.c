#define _CRT_SECURE_NO_WARNINGS
#define LIB_ALL_IMPL
#define LIB_MEM_DEBUG

#include "platform.h"
#include "string.h"
#include "hash_table.h"
#include "_test_random.h"
#include "_test_array.h"
#include "_test_hash_table.h"
#include "_test_log.h"

//@TODO: add capturing stack to debug allocator

void func1()
{
    #define STACK_SIZE 16
    Platform_Stack_Trace_Entry entries[STACK_SIZE] = {0};
    void* stack[STACK_SIZE] = {0};


    isize stack_size = platform_debug_capture_stack(stack, STACK_SIZE, 1);
    platform_debug_translate_stack(entries, stack, stack_size);
    
    LOG_INFO("TEST", "printing adresses");
    LOG_GROUP_PUSH();
    for(isize i = 0; i < stack_size; i++)
        LOG_INFO("TEST", "%p", stack[i]);
    LOG_GROUP_POP();

    LOG_INFO("TEST", "printing translations");
    LOG_GROUP_PUSH();
    for(isize i = 0; i < stack_size; i++)
    {
        LOG_INFO("TEST", "%s %s %s %d", entries[i].module, entries[i].function, entries[i].file, (int) entries[i].line);
    }
    LOG_GROUP_POP();
}

void func2()
{
    func1();
}

void func3()
{
    func2();
}

int main()
{
    func3();

    //allocator_allocate(allocator_get_default(), 1000000000000, 8, SOURCE_INFO());

    TEST(false);
    test_log();
    LOG_INFO("TEST", "All tests passed");

    test_array(3.0);
    test_hash_table(3.0);
    test_random();


    return 0;    
}