#pragma once

#include "lib/chase_lev_queue.h"
#include "lib/platform.h"
#include "lib/assert.h"
#include "lib/log.h"
#include "lib/random.h"

typedef struct Thread_Pool_Job {
    void (*func)(void*);
    void* data; //TODO: maybe increase this??
} Thread_Pool_Job;

typedef struct Thread_Pool Thread_Pool;

typedef struct Thread_Pool_Thread {
    CL_Queue queue;
    uint64_t rng_state;
    int32_t id;
    Thread_Pool* pool;
} Thread_Pool_Thread;

typedef struct Thread_Pool {
    Thread_Pool_Thread* threads;
    uint64_t threads_count;
    isize base_queue_capacity;
    isize max_queue_capacity;

    CL_QUEUE_ATOMIC(int32_t) assigned_thread_ids;
    CL_QUEUE_ATOMIC(uint32_t) closed; //hard closed - close immedietely, soft close - prevent pushes allow pops until empty then quit
    CL_QUEUE_ATOMIC(uint64_t) jobs_queued;
} Thread_Pool;


uint64_t _thread_pool_random_splitmix(uint64_t* state) 
{
	uint64_t z = (*state += 0x9e3779b97f4a7c15);
	z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9;
	z = (z ^ (z >> 27)) * 0x94d049bb133111eb;
	return z ^ (z >> 31);
}

void explicit_thread_pool_queue_job(Thread_Pool* pool, Thread_Pool_Thread* thread, void (*func)(void*), void* data) 
{
    Thread_Pool_Job job = {func, data};
    cl_queue_push(&thread->queue, &job, sizeof job);

    int64_t jobs_queued = atomic_fetch_add_explicit(&pool->jobs_queued, 2, memory_order_release);
    if((uint64_t) jobs_queued & 1)
        platform_futex_wake_all(&pool->jobs_queued);
}

void _thread_pool_launch_job(Thread_Pool* pool, Thread_Pool_Thread* thread, Thread_Pool_Job job)
{
    job.func(job.data);
    atomic_fetch_sub_explicit(&pool->jobs_queued, 2, memory_order_release);
}

void explicit_thread_pool_worker_func(Thread_Pool* pool, Thread_Pool_Thread* thread)
{
    //init thread

    cl_queue_take_ownership(thread->queue, "worker");
    
    int32_t id = thread->id;
    uint64_t threads_count = pool->threads_count;

    int64_t counter = atomic_load_explicit(&pool->jobs_counter, memory_order_acquire);
    for(;;) {
        uint32_t closed = atomic_load_explicit(&pool->closed, memory_order_relaxed);
        if(closed)
            break;

        isize completed_jobs = 0;
        int64_t jobs_queued = 0;
        Thread_Pool_Job job = {0};
        while(cl_queue_pop_back(&thread->queue, &job, sizeof job))
        {
            _thread_pool_launch_job(pool, thread, job);
            completed_jobs += 1;
        }

        for(uint64_t i = 0; i < threads_count; i++) {
            if(atomic_load_explicit(&pool->jobs_queued, memory_order_acquire)/2 == 0)
                break;
            else {
                uint64_t other_slot = ((uint64_t) id + i) % threads_count;
                Thread_Pool_Thread* other_thread = &pool->threads[other_slot];
                if(cl_queue_pop_weak(&thread->queue, &job, sizeof job) == CL_QUEUE_POP_OK)
                {
                    _thread_pool_launch_job(pool, thread, job);
                    completed_jobs += 1;
                    break;
                }
            }
        }
        
        for(;;) {
            int64_t jobs_queued = atomic_load_explicit(&pool->jobs_queued, memory_order_acquire)/2;
            if(jobs_queued != 0)
                break;

            atomic_fetch_or_explicit(&pool->jobs_queued, 1, memory_order_release);
            platform_futex_wait(&pool->jobs_queued, (uint32_t) 1, -1);
        }
    }
}

bool explicit_thread_pool_complete_one(Thread_Pool* pool, Thread_Pool_Thread* thread)
{
    int32_t id = thread->id;
    uint64_t threads_count = pool->threads_count;
    Thread_Pool_Job job = {0};
    isize completed_jobs = 0;
    if(atomic_load_explicit(&pool->jobs_queued, memory_order_acquire) == 0)
    {
        if(cl_queue_pop_back(&my_thread->queue, &job, sizeof job))
        {
            _thread_pool_launch_job(pool, thread, job);
            completed_jobs += 1;
        }
        else
        {
            for(uint64_t i = 0; i < threads_count; i++) {
                if(atomic_load_explicit(&pool->jobs_queued, memory_order_acquire)/2 == 0)
                    break;
                else {
                    uint64_t other_slot = ((uint64_t) id + i) % threads_count;
                    Thread_Pool_Thread* other_thread = &pool->threads[other_slot];
                    if(cl_queue_pop_weak(&my_thread->queue, &job, sizeof job) == CL_QUEUE_POP_OK)
                    {
                        _thread_pool_launch_job(pool, thread, job);
                        completed_jobs += 1;
                        break;
                    }
                }
            }
        }
    }

    return completed_jobs > 0;
}

void thread_pool_wait_and_idly_execute(volatile void* addr, uint32_t undesired, double timeout)
{
    CL_QUEUE_ATOMIC(uint32_t)* futex = (CL_QUEUE_ATOMIC(uint32_t)*) (void*) addr;

    for(;;) {
        uint32_t curr = atomic_load_explicit(futex, memory_order_relaxed);
        if(curr != undesired)
            break;

        //if nothing was completed the job queue is empty thus we sleep
        if(thread_pool_complete_one() == false)
            platform_futex_wait(addr, undesired, timeout);
    }
}

void thread_pool_synchronize(Thread_Pool* pool)
{
    
}

static _Thread_local Thread_Pool_Thread* current_worker = NULL; 
void thread_pool_queue_job(void (*func)(void*), void* data)
{
    ASSERT(current_worker != NULL, "needs to be called from worker thread!");
    explicit_thread_pool_queue_job(current_worker->pool, current_worker, func, data);
}

bool thread_pool_complete_one()
{
    ASSERT(current_worker != NULL, "needs to be called from worker thread!");
    explicit_thread_pool_complete_one(current_worker->pool, current_worker);
}

void _thread_pool_thread_init(void* context)
{
    Thread_Pool* pool = (Thread_Pool*) context;
    int32_t id = atomic_fetch_add(&pool->assigned_thread_ids, 1);

    Thread_Pool_Thread* thread = &pool->threads[id];
    thread->pool = pool;
    thread->id = id;
    thread->rng_state = thread->id;
    
    cl_queue_init(&thread->queue, sizeof(Thread_Pool_Job), pool->max_queue_capacity, "worker");
    cl_queue_reserve(&thread->queue, pool->base_queue_capacity);
}

#include "lib/defines.h"
void thread_pool_init(Thread_Pool* pool, isize num_threads_or_minus_one, isize initial_queue_capacity, isize max_queue_capacity_or_minus_one)
{
    memset(pool, 0, sizeof *pool);
    isize thread_count = num_threads_or_minus_one >= 0 ? num_threads_or_minus_one : platform_thread_get_proccessor_count(); 
    pool->threads = cl_queue_aligned_alloc(thread_count * sizeof(Thread_Pool_Thread), CL_QUEUE_CACHE_LINE);
    pool->threads_count = thread_count;
    pool->base_queue_capacity = initial_queue_capacity;
    pool->max_queue_capacity = max_queue_capacity_or_minus_one;

    for(isize i = 0; i < thread_count; i++)
        platform_thread_launch(NULL, 8*MB, _thread_pool_thread_init, pool);
}