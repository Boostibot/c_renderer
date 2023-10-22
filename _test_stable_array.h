#pragma once

#include "stable_array2.h"
#include "stable_array3.h"
#include "profile.h"
#include "random.h"
#include "array.h"
#include "allocator_stack.h"
#include "time.h"

#define _TEST_STABLE_LIN 64
#define _TEST_STABLE_MULT 1.5f

void test_stable_array_benchmark_add(f64 time, bool fake_run)
{
    enum {BATCH_SIZE = 16};
    enum {MAX_ELEMS = 4096 / BATCH_SIZE};

    Malloc_Allocator malloc = {0};
    malloc_allocator_init_use(&malloc, 0);

    isize val = 0;
    {
        Stable_Array a = {0};
        for(f64 start = clock_s(); clock_s() < start + time;)
        {
            stable_array_init_custom(&a, allocator_get_default(), sizeof(isize), DEF_ALIGN, _TEST_STABLE_LIN, _TEST_STABLE_MULT);
            for(isize k = 0; k < MAX_ELEMS; k++)
            {
                PERF_COUNTER_START(v3);
                for(isize i = 0; i < BATCH_SIZE; i++)
                {
                    if(k == 14)
                    {
                        int x = 0; (void) x;
                    }

                    isize* ptr = NULL;
                    stable_array_insert(&a, (void**) &ptr);
                    *ptr = val ++;
                }
                PERF_COUNTER_END_DISCARD(v3, fake_run);
            }

            stable_array_deinit(&a);
        }
    }
    
    {
        Stable_Array2 a = {0};
        for(f64 start = clock_s(); clock_s() < start + time;)
        {
            stable_array2_init_custom(&a, allocator_get_default(), sizeof(isize), DEF_ALIGN, _TEST_STABLE_LIN, _TEST_STABLE_MULT);
            for(isize k = 0; k < MAX_ELEMS; k++)
            {
                PERF_COUNTER_START(v2);
                for(isize i = 0; i < BATCH_SIZE; i++)
                {
                    isize* ptr = NULL;
                    stable_array2_insert(&a, (void**) &ptr);
                    *ptr = val ++;
                }
                PERF_COUNTER_END_DISCARD(v2, fake_run);
            }

            stable_array2_deinit(&a);
        }
    }

    malloc_allocator_deinit(&malloc);
}


void test_stable_array_benchmark_add_remove_random(f64 time, bool fake_run)
{
    enum {BATCH_SIZE = 16};
    enum {MAX_ELEMS = 4096 / BATCH_SIZE};
    enum {FLUX_SIZE = 16};

    Malloc_Allocator malloc = {0};
    malloc_allocator_init_use(&malloc, 0);

    isize val = 0;
    {
        isize* ptr = NULL;
        Stable_Array a = {0};
        stable_array_init_custom(&a, allocator_get_default(), sizeof(isize), DEF_ALIGN, _TEST_STABLE_LIN, _TEST_STABLE_MULT);
        for(isize k = 0; k < MAX_ELEMS * BATCH_SIZE; k++)
        {
            stable_array_insert(&a, (void**) &ptr);
            *ptr = val ++;
        }

        for(f64 start = clock_s(); clock_s() < start + time;)
        {
            PERF_COUNTER_START(v3_remove);
            for(isize k = 0; k < FLUX_SIZE; k++)
            {
                for(isize i = 0; i < BATCH_SIZE; i++)
                    stable_array_remove(&a, random_range(0, a.size));
            }
            PERF_COUNTER_END_DISCARD(v3_remove, fake_run);

            PERF_COUNTER_START(v3_insert);
            for(isize k = 0; k < FLUX_SIZE; k++)
            {
                for(isize i = 0; i < BATCH_SIZE; i++)
                    stable_array_insert(&a, (void**) &ptr);
            }
            PERF_COUNTER_END_DISCARD(v3_insert, fake_run);

        }
        stable_array_deinit(&a);
    }
    
    {
        isize* ptr = NULL;
        Stable_Array2 a = {0};
        stable_array2_init_custom(&a, allocator_get_default(), sizeof(isize), DEF_ALIGN, _TEST_STABLE_LIN, _TEST_STABLE_MULT);
        for(isize k = 0; k < MAX_ELEMS * BATCH_SIZE; k++)
        {
            stable_array2_insert(&a, (void**) &ptr);
            *ptr = val ++;
        }

        for(f64 start = clock_s(); clock_s() < start + time;)
        {
            PERF_COUNTER_START(v2_remove);
            for(isize k = 0; k < FLUX_SIZE; k++)
            {
                for(isize i = 0; i < BATCH_SIZE; i++)
                    stable_array2_remove(&a, random_range(0, a.size));
            }
            PERF_COUNTER_END_DISCARD(v2_remove, fake_run);

            PERF_COUNTER_START(v2_insert);
            for(isize k = 0; k < FLUX_SIZE; k++)
            {
                for(isize i = 0; i < BATCH_SIZE; i++)
                    stable_array2_insert(&a, (void**) &ptr);
            }
            PERF_COUNTER_END_DISCARD(v2_insert, fake_run);
        }
        stable_array2_deinit(&a);
    }

    malloc_allocator_deinit(&malloc);
}

void test_stable_array_benchmark_look(f64 time, bool fake_run)
{
    enum {BATCH_SIZE = 16};
    enum {MAX_ELEMS = 4096 / BATCH_SIZE};

    Malloc_Allocator malloc = {0};
    malloc_allocator_init_use(&malloc, 0);

    isize val = 0;
    {
        Stable_Array a = {0};
        
        isize* ptr = NULL;
        stable_array_init_custom(&a, allocator_get_default(), sizeof(isize), DEF_ALIGN, _TEST_STABLE_LIN, _TEST_STABLE_MULT);
        for(isize k = 0; k < MAX_ELEMS * BATCH_SIZE; k++)
            stable_array_insert(&a, (void**) &ptr);

        for(f64 start = clock_s(); clock_s() < start + time;)
        {
            for(isize k = 0; k < MAX_ELEMS; k++)
            {
                PERF_COUNTER_START(v3);
                for(isize i = 0; i < BATCH_SIZE; i++)
                {
                    isize index = random_range(0, MAX_ELEMS * BATCH_SIZE);
                    //isize index = k * BATCH_SIZE + i;
                    ptr = (isize*) stable_array_at(&a, index);
                    *ptr = val++;
                }

                PERF_COUNTER_END_DISCARD(v3, fake_run);
            }

        }
        stable_array_deinit(&a);
    }
    
    {
        Stable_Array2 a = {0};
        isize* ptr = NULL;
        stable_array2_init_custom(&a, allocator_get_default(), sizeof(isize), DEF_ALIGN, _TEST_STABLE_LIN, _TEST_STABLE_MULT);
        for(isize k = 0; k < MAX_ELEMS * BATCH_SIZE; k++)
            stable_array2_insert(&a, (void**) &ptr);

        for(f64 start = clock_s(); clock_s() < start + time;)
        {
            for(isize k = 0; k < MAX_ELEMS; k++)
            {
                PERF_COUNTER_START(v2);
                for(isize i = 0; i < BATCH_SIZE; i++)
                {
                    isize index = random_range(0, MAX_ELEMS * BATCH_SIZE);
                    //isize index = k * BATCH_SIZE + i;
                    ptr = (isize*) stable_array2_at(&a, index);
                    *ptr = val++;
                }
                
                PERF_COUNTER_END_DISCARD(v2, fake_run);
            }
        }

        stable_array2_deinit(&a);
    }

    malloc_allocator_deinit(&malloc);
}
void test_stable_array_benchmark(f64 time)
{
    //test_stable_array_benchmark_add(0.3f, true);
    //test_stable_array_benchmark_add(time, false);
    //test_stable_array_benchmark_add_remove_random(0.3f, true);
    //test_stable_array_benchmark_add_remove_random(time, false);
    
    test_stable_array_benchmark_look(0.3f, true);
    test_stable_array_benchmark_look(time, false);
    (void) time;
    

}