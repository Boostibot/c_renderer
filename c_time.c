#include "c_time.h"

#if defined(_WIN32) || defined(_WIN64) || defined(__CYGWIN__)   
#include <windows.h>
#include <profileapi.h>
int64_t perf_counter()
{
    LARGE_INTEGER ticks;
    ticks.QuadPart = 0;
    (void) QueryPerformanceCounter(&ticks);
    return ticks.QuadPart;
}
int64_t perf_counter_base()
{
    static int64_t base = 0;
    if(base == 0)
        base = perf_counter();
    return base;
}
int64_t _query_perf_freq()
{
    LARGE_INTEGER ticks;
    ticks.QuadPart = 0;
    (void) QueryPerformanceFrequency(&ticks);
    return ticks.QuadPart;
}

int64_t perf_counter_frequency()
{
    static int64_t freq = 0;
    if(freq == 0)
        freq = _query_perf_freq(); //doesnt change so we can cache it
    return freq;
}
    
double perf_counter_frequency_d()
{
    static double freq = 0;
    if(freq == 0)
        freq = (double) perf_counter_frequency(); 
    return freq;
}

#else
#include <time.h>
int64_t perf_counter()
{
    struct timespec ts;
    (void) clock_gettime(CLOCK_MONOTONIC_RAW, &ts);

    return ts.tv_nsec;
}
    

int64_t perf_counter_frequency()
{
    return (int64_t) 1000 * 1000 * 1000; //nanoseconds
}
    
double perf_counter_frequency_d()
{
    return 1.0e9; //nanoseconds
}
#endif
