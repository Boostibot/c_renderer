#ifndef LIB_PROFILE
#define LIB_PROFILE

#include "defines.h"
#include "platform.h"
#include "assert.h"

// This file provides a simple and extremely performant API
// for tracking performance across the whole aplication.
// Does not require any intialization, allocations or locks and
// works across files and compilation units.

// Based on approach in handmade hero.
// See below for example. 

#ifdef _PROFILE_EXAMPLE
void main()
{
	//========== 1: capture stats ===============
    for(isize i = 0; i < 1000; i++)
	{
		PERF_COUNTER_START(my_counter);
        //Run some code
		    PERF_COUNTER_START(my_counter2);
            //Run some code
            //printf("%d ", (int) i);
		    PERF_COUNTER_END(my_counter2);
		PERF_COUNTER_END(my_counter);
	}
    
	//========== 2: print stats ===============
	Perf_Counter* counters = profile_get_counters();
	for(isize i = 0; i < MAX_PERF_COUNTERS; i++)
	{
        if(counters[i].runs > 0)
        {
		    printf("%09lf ms counter %s from %s : %d\n", 
			    profile_get_counter_avg_running_time_s(counters[i])*1000,
			    counters[i].name,
			    counters[i].function,
			    (int) counters[i].line
		    );
        }
	}

	//On my machine the output is
	
	//without any optimalizations:
	//00.000131 ms counter my_counter from main : 10
    //00.000041 ms counter my_counter2 from main : 12

	//With -O2 and #define PROFILE_NO_DEBUG:
    //00.000067 ms counter my_counter from main : 10
    //00.000020 ms counter my_counter2 from main : 12

	//With -O2 and #define PROFILE_NO_DEBUG: and #define PROFILE_NO_ATOMICS
    //00.000056 ms counter my_counter from main : 10
    //00.000020 ms counter my_counter2 from main : 12

	//As you can see the profiling incrus minimal overhead. 
	//This is especially noticable with the #define PROFILE_NO_ATOMICS
	//which esentially strips it down to only the bare necessities without 
	//improving the performance in any cosiderable way.
}
#endif

//Specifies the maximum number of counters in the whole codebase. 
//If the number is exceeded will not compile so increase only when 
// you run into errors
#ifndef MAX_PERF_COUNTERS
#define MAX_PERF_COUNTERS 100
#endif

//Dissables counting of currently active perf counters
//#define PROFILE_NO_DEBUG
//Dissables the use of attomics. ONLY USE WHILE SINGLE THREADED
//#define PROFILE_NO_ATOMICS

typedef struct Perf_Counter
{
	i64 counter;
	i64 runs;
	i64 line;
	const char* file;
	const char* function;
	const char* name;
	i64 frquency;
	i64 _padding;
} Perf_Counter;

typedef struct Perf_Counter_Running
{
	i64 start;
	i32 counter_index;
	i32 line;
	const char* file;
	const char* function;
	const char* name;
} Perf_Counter_Running;

EXPORT Perf_Counter_Running perf_counter_start(i32 index, i32 line, const char* file, const char* function, const char* name);
EXPORT void perf_counter_end(const Perf_Counter_Running running);

EXPORT Perf_Counter* profile_get_counters();
EXPORT i64 profile_get_running_counters_count();
EXPORT f64 profile_get_counter_total_running_time_s(Perf_Counter counter);
EXPORT f64 profile_get_counter_avg_running_time_s(Perf_Counter counter);

//Starts a counter. Doesnt have any effect on global perf counter stats
#define PERF_COUNTER_START(name) \
	enum { PP_CONCAT(__counter__, __LINE__) = __COUNTER__ }; \
	STATIC_ASSERT(PP_CONCAT(__counter__, __LINE__) < MAX_PERF_COUNTERS); \
	Perf_Counter_Running name = perf_counter_start(PP_CONCAT(__counter__, __LINE__), __LINE__, __FILE__, __FUNCTION__, #name)

//Ends a counter and updates the global stats
#define PERF_COUNTER_END(name) perf_counter_end(name)


#endif

#if (defined(LIB_ALL_IMPL) || defined(LIB_PROFILE_IMPL)) && !defined(LIB_PROFILE_HAS_IMPL)
#define LIB_PROFILE_HAS_IMPL

	//Must be correctly sized for optimal performance
	//need to be on separe cache lines to eliminate false sharing
	STATIC_ASSERT(sizeof(Perf_Counter) == 64);

	ALIGNED(64) static i64 perf_counters_running_count = 0;
	ALIGNED(64) static Perf_Counter perf_counters[MAX_PERF_COUNTERS] = {0};

	EXPORT Perf_Counter_Running perf_counter_start(i32 index, i32 line, const char* file, const char* function, const char* name)
	{
		Perf_Counter_Running running = {0};
		running.start = platform_perf_counter();
		running.counter_index = index;
		running.line = line;
		running.file = file;
		running.function = function;
		running.name = name;
	
		#if !defined(PROFILE_NO_DEBUG) && !defined(PROFILE_NO_ATOMICS)
		platform_interlocked_increment64(&perf_counters_running_count);
		#endif // PROFILE_NO_DEBUG

		return running;
	}

	EXPORT void perf_counter_end(const Perf_Counter_Running running)
	{
		i64 index = running.counter_index;
		i64 delta = platform_perf_counter() - running.start;
		ASSERT(delta >= 0);
		bool save_one_time_stats = false;

		#if !defined(PROFILE_NO_ATOMICS)
			platform_interlocked_add64(&perf_counters[index].counter, delta);
			save_one_time_stats = platform_interlocked_increment64(&perf_counters[index].runs) == 1; 
		#else
			perf_counters[index].counter += delta;
			save_one_time_stats = ++perf_counters[index].runs == 1;
		#endif

		if(save_one_time_stats)
		{
			//only save the stats that dont need to be updated on the first run
			perf_counters[index].file = running.file;
			perf_counters[index].line = running.line;
			perf_counters[index].function = running.function;
			perf_counters[index].name = running.name;
			perf_counters[index].frquency = platform_perf_counter_frequency();
		}
	
		#if !defined(PROFILE_NO_DEBUG) && !defined(PROFILE_NO_ATOMICS)
		platform_interlocked_decrement64(&perf_counters_running_count);
		#endif // PROFILE_NO_DEBUG
	}
	
	EXPORT f64 profile_get_counter_total_running_time_s(Perf_Counter counter)
	{
		f64 den = (f64) counter.frquency;
		f64 num = (f64) counter.counter;
		if(den == 0 && num == 0)
			return 0;
		else
			return num / den;
	}
	EXPORT f64 profile_get_counter_avg_running_time_s(Perf_Counter counter)
	{
		f64 den = (f64) (counter.frquency * counter.runs);
		f64 num = (f64) counter.counter;
		if(den == 0 && num == 0)
			return 0;
		else
			return num / den;
	}
	
	EXPORT Perf_Counter* profile_get_counters()
	{
		return perf_counters;
	}
	EXPORT i64 profile_get_running_counters_count()
	{
		return perf_counters_running_count;
	}
#endif