#define LIB_ALL_IMPL
#define LIB_MEM_DEBUG

#include "platform.h"
#include "string.h"
#include "hash_index.h"
#include "log.h"
#include "_test_random.h"
#include "_test_array.h"
#include "_test_hash_index.h"
#include "_test_log.h"
#include "_test_hash_table.h"
#include "file.h"

void error_func(void* context, Platform_Sandox_Error error_code);

//all systems are init at this point. 
//Lives inside a sandbox :3
void run_func(void* context)
{
    test_hash_table_stress(3.0);
    LOG_INFO("APP", "All tests passed! uwu");
    return;

    test_log();
    test_array(1.0);
    test_hash_index(1.0);
    test_random();

}

int main()
{
    platform_init();
    
    Allocator* alloc = allocator_get_default();
    log_system_init(alloc, alloc);

    Debug_Allocator debug_allocator = {0};
    debug_allocator_init_use(&debug_allocator, DEBUG_ALLOCATOR_DEINIT_LEAK_CHECK | DEBUG_ALLOCATOR_PRINT | DEBUG_ALLOCATOR_CONTINUOUS);

    {
        String_Builder contents = {0};
        LOG_INFO("APP", STRING_FMT, STRING_PRINT(contents));

        //Platform_Error error = file_read_entire(STRING("main.c"), &contents);
        Platform_Error error = file_read_entire(STRING("main.c"), &contents);
        if(error != 0)
        {
            LOG_ERROR("APP", "%s", platform_translate_error_alloc(error));
        }
        
        LOG_INFO("APP", STRING_FMT, STRING_PRINT(contents));
        
        error = file_append_entire(STRING("main_new.c"), string_from_builder(contents));
        error = file_append_entire(STRING("main_new.c"), string_from_builder(contents));
        if(error != 0)
        {
            LOG_ERROR("APP", "%s", platform_translate_error_alloc(error));
        }
        
        error = file_write_entire(STRING("main_new.c"), string_from_builder(contents));
    }


    Platform_Sandox_Error error = platform_exception_sandbox(
        run_func, NULL, 
        error_func, NULL);

    debug_allocator_deinit(&debug_allocator);

    log_system_deinit();

    platform_deinit();

    return 0;    
}

const char* platform_sandbox_error_to_cstring(Platform_Sandox_Error error)
{
    switch(error)
    {
        case PLATFORM_EXCEPTION_NONE: return "PLATFORM_EXCEPTION_NONE";
        case PLATFORM_EXCEPTION_ACCESS_VIOLATION: return "PLATFORM_EXCEPTION_ACCESS_VIOLATION";
        case PLATFORM_EXCEPTION_DATATYPE_MISALIGNMENT: return "PLATFORM_EXCEPTION_DATATYPE_MISALIGNMENT";
        case PLATFORM_EXCEPTION_FLOAT_DENORMAL_OPERAND: return "PLATFORM_EXCEPTION_FLOAT_DENORMAL_OPERAND";
        case PLATFORM_EXCEPTION_FLOAT_DIVIDE_BY_ZERO: return "PLATFORM_EXCEPTION_FLOAT_DIVIDE_BY_ZERO";
        case PLATFORM_EXCEPTION_FLOAT_INEXACT_RESULT: return "PLATFORM_EXCEPTION_FLOAT_INEXACT_RESULT";
        case PLATFORM_EXCEPTION_FLOAT_INVALID_OPERATION: return "PLATFORM_EXCEPTION_FLOAT_INVALID_OPERATION";
        case PLATFORM_EXCEPTION_FLOAT_OVERFLOW: return "PLATFORM_EXCEPTION_FLOAT_OVERFLOW";
        case PLATFORM_EXCEPTION_FLOAT_UNDERFLOW: return "PLATFORM_EXCEPTION_FLOAT_UNDERFLOW";
        case PLATFORM_EXCEPTION_FLOAT_OTHER: return "PLATFORM_EXCEPTION_FLOAT_OTHER";
        case PLATFORM_EXCEPTION_PAGE_ERROR: return "PLATFORM_EXCEPTION_PAGE_ERROR";
        case PLATFORM_EXCEPTION_INT_DIVIDE_BY_ZERO: return "PLATFORM_EXCEPTION_INT_DIVIDE_BY_ZERO";
        case PLATFORM_EXCEPTION_INT_OVERFLOW: return "PLATFORM_EXCEPTION_INT_OVERFLOW";
        case PLATFORM_EXCEPTION_ILLEGAL_INSTRUCTION: return "PLATFORM_EXCEPTION_ILLEGAL_INSTRUCTION";
        case PLATFORM_EXCEPTION_PRIVILAGED_INSTRUCTION: return "PLATFORM_EXCEPTION_PRIVILAGED_INSTRUCTION";
        case PLATFORM_EXCEPTION_BREAKPOINT: return "PLATFORM_EXCEPTION_BREAKPOINT";
        case PLATFORM_EXCEPTION_BREAKPOINT_SINGLE_STEP: return "PLATFORM_EXCEPTION_BREAKPOINT_SINGLE_STEP";
        case PLATFORM_EXCEPTION_STACK_OVERFLOW: return "PLATFORM_EXCEPTION_STACK_OVERFLOW";
        case PLATFORM_EXCEPTION_ABORT: return "PLATFORM_EXCEPTION_ABORT";
        case PLATFORM_EXCEPTION_TERMINATE: return "PLATFORM_EXCEPTION_TERMINATE";
        case PLATFORM_EXCEPTION_OTHER: return "PLATFORM_EXCEPTION_OTHER";
        default:
            return "PLATFORM_EXCEPTION_OTHER";
    }
}

void error_func(void* context, Platform_Sandox_Error error_code)
{
    const char* msg = platform_sandbox_error_to_cstring(error_code);
    
    LOG_ERROR("APP", "%s exception occured", msg);
    LOG_TRACE("APP", "printing trace:");
    log_group_push();
    log_callstack(STRING("APP"), -1, 1);
    log_group_pop();
}