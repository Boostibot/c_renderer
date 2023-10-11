#pragma once

#include "time.h"
#include "platform.h"
#include "defines.h"

typedef struct Clock {
    f64 start;
    f64 end;
    f64 time_scale;
} Clock;

EXPORT Clock clock_start(f64 time_scale);
EXPORT Clock clock_make(f64 start, f64 time_scale);
EXPORT Clock clock_make_range(f64 start, f64 end,f64 time_scale);

EXPORT bool clock_is_up(Clock clock, f64 curr_global_time);
EXPORT f64 clock_get_local_time(Clock clock, f64 global_time);
EXPORT f64 clock_get_global_time(Clock clock, f64 local_time);
EXPORT f64 clock_get_clamped_local_time(Clock clock, f64 global_time);
EXPORT f64 clock_get_looped_local_time(Clock clock, f64 global_time);


EXPORT Clock clock_start(f64 time_scale);
EXPORT Clock clock_make(f64 start, f64 time_scale);
EXPORT Clock clock_make_range(f64 start, f64 end,f64 time_scale);

EXPORT bool clock_is_up(Clock clock, f64 curr_global_time);
EXPORT f64 clock_get_local_time(Clock clock, f64 global_time);
EXPORT f64 clock_get_global_time(Clock clock, f64 local_time);
EXPORT f64 clock_get_clamped_local_time(Clock clock, f64 global_time);
EXPORT f64 clock_get_looped_local_time(Clock clock, f64 global_time);