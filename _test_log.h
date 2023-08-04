#pragma once

#include "_test.h"
#include "log.h"
#include "allocator_debug.h"

static void test_log()
{
    Debug_Allocator debug_allocator = {0};
    debug_allocator_init_use(&debug_allocator, DEBUG_ALLOCATOR_DEINIT_LEAK_CHECK);
    {
        LOG_INFO("ANIM", "iterating all entitites");

        log_group_push();
        for(int i = 0; i < 5; i++)
            LOG_INFO("ANIM", 
                "entity id:%d found\n"
                "Hello from entity", i);
        log_group_pop();

        LOG_FATAL("ANIM", 
            "Fatal error encountered!\n"
            "Some more info\n" 
            "%d-%d", 10, 20);

        log_system_deinit();
    }
    debug_allocator_deinit(&debug_allocator);
}

