#pragma once

#include "_test.h"
#include "log.h"
#include "allocator_debug.h"

static void test_log()
{
    log_system_deinit();

    Debug_Allocator debug_allocator = {0};
    debug_allocator_init_use(&debug_allocator, DEBUG_ALLOCATOR_DEINIT_LEAK_CHECK);
    {
        log_system_init(&debug_allocator.allocator, &debug_allocator.allocator);
        LOG_INFO("TEST_LOG", "iterating all entitites");

        log_group_push();
        for(int i = 0; i < 5; i++)
            LOG_INFO("TEST_LOG", 
                "entity id:%d found\n"
                "Hello from entity", i);
        log_group_pop();

        LOG_FATAL("TEST_LOG", 
            "Fatal error encountered!\n"
            "Some more info\n" 
            "%d-%d", 10, 20);
            
        log_system_deinit();
    }
    debug_allocator_deinit(&debug_allocator);
}

