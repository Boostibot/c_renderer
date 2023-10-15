#ifndef LIB_CLOCK
#define LIB_CLOCK

// Clock represents a linear mapping between global and local timeline.
// It is used by all systems that depend on some notion of time.
// The mapping has enough data to represent any mapping. This means:
// 
// Global                Local 
// [s1, e1]  <~~clock~~> [s2, e2]
// [s1, inf) <~~clock~~> [s2, inf)
// [s1, inf) <~~clock~~> (-inf, e2]
// etc...
//
// It cannot provide mapping between bounded and unbounded range as that mapping
// cannot be linear. The composition of the Clock struct makes these cases 
// not representable.

#include "time.h"

#define INFINITY_F64 (1e300 * 1e300)
#define INFINITY_F32 (1e300f * 1e300f)

typedef struct Clock {
    f64 global_start;
    f64 global_end;
    f64 time_ratio; //is global_druation / local_duration
    f64 local_start;
} Clock;

EXPORT Clock clock_start(f64 time_scale);
EXPORT Clock clock_make(f64 global_start, f64 global_duration, f64 local_start, f64 local_duration);
EXPORT Clock clock_make_infinite(f64 global_edge_point, f64 global_time_scale, f64 local_edge_point, f64 local_time_scale);

EXPORT bool clock_is_up(Clock clock, f64 global_time);
EXPORT f64 clock_get_local_time(Clock clock, f64 global_time);
EXPORT f64 clock_get_global_time(Clock clock, f64 local_time);

#endif

#if (defined(LIB_ALL_IMPL) || defined(LIB_CLOCK_IMPL)) && !defined(LIB_CLOCK_HAS_IMPL)
#define LIB_CLOCK_HAS_IMPL

EXPORT Clock clock_start(f64 time_scale)
{
    Clock clock = {clock_s(), INFINITY_F64, time_scale};
    return clock;
}

EXPORT Clock clock_make(f64 global_start, f64 global_duration, f64 local_start, f64 local_duration)
{
    Clock clock = {0};
    clock.time_ratio = global_duration / local_duration;
    clock.global_start = global_start;
    clock.global_end = global_start + global_duration;
    clock.local_start = local_start;

    return clock;
}

EXPORT Clock clock_make_infinite(f64 global_edge_point, f64 global_time_scale, f64 local_edge_point, f64 local_time_scale)
{
    Clock clock = {0};
    if(global_time_scale > 0)
        clock.global_start = global_edge_point;
    else
        clock.global_end = global_edge_point;

    clock.local_start = local_edge_point;
    clock.time_ratio = global_time_scale / local_time_scale;

    return clock;
}

EXPORT bool clock_is_up(Clock clock, f64 global_time)
{
    return clock.global_end <= global_time;
}
EXPORT f64 clock_get_local_time(Clock clock, f64 global_time)
{
    f64 local_time = (global_time - clock.global_start) / clock.time_ratio + clock.local_start;
    return local_time;
}
EXPORT f64 clock_get_global_time(Clock clock, f64 local_time)
{
    f64 global_time = (local_time - clock.local_start) * clock.time_ratio + clock.global_start;
    return global_time;
}

#endif