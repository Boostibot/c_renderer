#pragma once
#include <stdint.h>

int64_t perf_counter();            //returns the current value of performance counter
int64_t perf_counter_base();       //returns the value of performence conuter at the first time this function was called which is taken as the startup time
int64_t perf_counter_frequency();  //returns the frequency of the performance counter
double  perf_counter_frequency_d(); //returns the frequency of the performance counter as double (saves expensive integer to float cast)

//Returns the time from the startup time in nanoseconds
static inline int64_t clock_ns()
{
    int64_t freq = perf_counter_frequency();
    int64_t counter = perf_counter() - perf_counter_base();

    int64_t sec_to_nanosec = 1'000'000'000;
    //We assume _perf_counter_base is set to some reasonable thing so this will not overflow
    // (with this we are only able to represent 1e12 secons (A LOT) without overflowing)
    return counter * sec_to_nanosec / freq;
}

//Returns the time from the startup time in seconds
static inline double clock_s()
{
    double freq = perf_counter_frequency_d();
    double counter = (double) (perf_counter() - perf_counter_base());
    return counter / freq;
}

//@NOTE:
//We might be rightfully scared that after some ammount of time the clock_s() will get sufficiently large and
// we will start loosing pression. This is however not a problem. If we assume the perf_counter_frequency is equal to 
// 10Mhz = 1e7 (which is very common) then we would like the clock_s to have enough precision to represent
// 1 / 10Mhz = 1e-7. This precission is held up untill 1e9 secons have passed which is roughly 31 years. 
// => Please dont run your program for more than 31 years or you will loose precision

#define SECOND_MILISECONDS  ((int64_t) 1000)
#define SECOND_MIRCOSECONDS ((int64_t) 1000000)
#define SECOND_NANOSECONDS  ((int64_t) 1000000000)
#define SECOND_PICOSECONDS  ((int64_t) 1000000000000)
#define MILISECOND_NANOSECONDS (SECOND_NANOSECONDS / SECOND_MILISECONDS)

#define MINUTE_SECONDS ((int64_t) 60)
#define HOUR_SECONDS ((int64_t) 60 * MINUTE_SECONDS)
#define DAY_SECONDS ((int64_t) 24 * MINUTE_SECONDS)
#define WEEK_SECONDS ((int64_t) 7 * DAY_SECONDS)


