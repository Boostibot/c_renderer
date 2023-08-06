#define _CRT_SECURE_NO_WARNINGS
#define LIB_ALL_IMPL
#define LIB_MEM_DEBUG

#include "platform.h"
#include "string.h"
#include "hash_table.h"
#include "log.h"
#include "_test_random.h"
#include "_test_array.h"
#include "_test_hash_table.h"
#include "_test_log.h"

void print_stack_trace();

//all systems are init at this point. 
//Lives inside a sandbox :3
void run(void* context)
{
    test_log();
    test_array(1.0);
    test_hash_table(1.0);
    test_random();

    LOG_INFO("APP", "All tests passed! uwu");
}

int main()
{
    platform_init();
    Allocator* alloc = allocator_get_default();
    log_system_init(alloc, alloc);

    Platform_Sandox_Error error = platform_exception_sandbox(
        run, NULL, 
        print_stack_trace, NULL);
    
    log_system_deinit();
    platform_deinit();

    return 0;    
}


#define STACK_SIZE 16
Platform_Stack_Trace_Entry entries[STACK_SIZE] = {0};
void* stack[STACK_SIZE] = {0};

void print_stack_trace()
{
    isize stack_size = platform_capture_call_stack(stack, STACK_SIZE, 1);
    platform_translate_call_stack(entries, stack, stack_size);

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