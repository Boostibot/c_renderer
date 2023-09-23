#ifndef LIB_PROFILE
#define LIB_PROFILE

#include "defines.h"
#include "platform.h"
#include "assert.h"
#include <math.h>

// This file provides a simple and performant API
// for tracking running time across the whole aplication.
// Does not require any intialization, allocations or locks and
// works across files and compilation units.

// See below for example. 

#ifdef _PROFILE_EXAMPLE
void main()
{
	//========== 1: capture stats ===============
    for(isize i = 0; i < 100000; i++)
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
	for(Perf_Counter* counter = profile_get_counters(); counter != NULL; counter = counter->next)
	{
		printf("total: %09lf ms average: %09lf ms counter \"%s\" : %s : %d\n", 
			profile_get_counter_total_running_time_s(*counter)*1000,
			profile_get_counter_average_running_time_s(*counter)*1000,
			counter->name,
			counter->function,
			(int) counter->line
		);
	}

	//On my machine the output is
	
	//without any optimalizations:
	//00.000131 ms counter "my_counter" : main : 10
    //00.000041 ms counter "my_counter2" : main : 12

	//With -O2 and #define PROFILE_NO_DEBUG:
    //00.000067 ms counter "my_counter" : main : 10
    //00.000020 ms counter "my_counter2" : main : 12

	//With -O2 and #define PROFILE_NO_DEBUG: and #define PROFILE_NO_ATOMICS
    //00.000056 ms counter "my_counter" : main : 10
    //00.000020 ms counter "my_counter2" : main : 12

	//Update: PROFILE_NO_ATOMICS is no longer supported

	//As you can see the profiling incrus minimal overhead. 
	//This is especially noticable with the #define PROFILE_NO_ATOMICS
	//which esentially strips it down to only the bare necessities without 
	//improving the performance in any cosiderable way.
}
#endif

//Dissables counting of currently active perf counters
//#define PROFILE_NO_DEBUG

//Locally enables perf counters (can be toggled just like ASSERT macros)
#define DO_PERF_COUNTERS

#define PROFILE_DO_ONLY_DETAILED_COUNTERS

//typedef struct Perf_Counter Perf_Counter;
//typedef void (*Perf_Counter_User_Format_Func)(const Perf_Counter* counter, String_Builder* into);

typedef struct Perf_Counter
{
	//Sometimes we want to add some extra piece of data we track to our counters
	//such as: number of hash collisions, number of times a certain branch was taken etc.
	//We cannot track this 
	//void* user_format_context;
	//Perf_Counter_User_Format_Func* user_format_func;

	struct Perf_Counter* next;
	i32 line;
	i32 concurrent_running_counters; 
	const char* file;
	const char* function;
	const char* name;

	bool is_detailed;
	i64 counter;
	i64 runs;
	i64 frquency;
	i64 mean_estimate;
	i64 sum_of_squared_offset_counters;
	i64 max_counter;
	i64 min_counter;
	//the number of concurrent running counters actiong upon this counter.
	//Useful for debugging. Is 0 if PROFILE_NO_DEBUG is defines

} Perf_Counter;

typedef struct Perf_Counter_Running
{
	Perf_Counter* my_counter;
	i64 start;
	i64 line;
	const char* file;
	const char* function;
	const char* name;
	bool stopped;
} Perf_Counter_Running;

EXPORT Perf_Counter_Running perf_counter_start(Perf_Counter* my_counter, i32 line, const char* file, const char* function, const char* name);
EXPORT void perf_counter_end(Perf_Counter_Running* running);
EXPORT void perf_counter_end_detailed(Perf_Counter_Running* running);
EXPORT f64 perf_counter_get_ellapsed(Perf_Counter_Running running);

EXPORT Perf_Counter* profile_get_counters();
EXPORT i64 profile_get_total_running_counters_count();
EXPORT f64 profile_get_counter_total_running_time_s(Perf_Counter counter);
EXPORT f64 profile_get_counter_average_running_time_s(Perf_Counter counter);
EXPORT f64 profile_get_counter_min_running_time_s(Perf_Counter counter);
EXPORT f64 profile_get_counter_max_running_time_s(Perf_Counter counter);
EXPORT f64 profile_get_counter_standard_deviation_s(Perf_Counter counter);
EXPORT f64 profile_get_counter_normalized_standard_deviation_s(Perf_Counter counter); //returns the standard deviation divided by average (σ/μ)



#define PERF_COUNTER_START(name) PP_ID(PP_CONCAT(_IF_NOT_PERF_START_, DO_PERF_COUNTERS)(name))
#define PERF_COUNTER_END(name) PP_ID(PP_CONCAT(_IF_NOT_PERF_END_, DO_PERF_COUNTERS)(name))
#define PERF_COUNTER_END_DETAILED(name) PP_ID(PP_CONCAT(_IF_NOT_PERF_END_DETAILED_, DO_PERF_COUNTERS)(name))

#ifdef PROFILE_DO_ONLY_DETAILED_COUNTERS
	#undef PERF_COUNTER_END
	#define PERF_COUNTER_END(name) PERF_COUNTER_END_DETAILED(name)
#endif

// ========= MACRO IMPLMENTATION ==========
	#define _IF_NOT_PERF_START_DO_PERF_COUNTERS(name) Perf_Counter_Running name = {0}
	#define _IF_NOT_PERF_START_(name) \
		ALIGNED(64) static Perf_Counter _##name = {0}; \
		Perf_Counter_Running name = perf_counter_start(&_##name, __LINE__, __FILE__, __FUNCTION__, #name)

	#define _IF_NOT_PERF_END_DO_PERF_COUNTERS(name) (void) name
	#define _IF_NOT_PERF_END_(name) perf_counter_end(&name)
	
	#define _IF_NOT_PERF_END_DETAILED_DO_PERF_COUNTERS(name) (void) name
	#define _IF_NOT_PERF_END_DETAILED_(name) perf_counter_end_detailed(&name)
#endif

#if (defined(LIB_ALL_IMPL) || defined(LIB_PROFILE_IMPL)) && !defined(LIB_PROFILE_HAS_IMPL)
#define LIB_PROFILE_HAS_IMPL

	//Must be correctly sized for optimal performance
	//need to be on separed cache lines to eliminate false sharing
	STATIC_ASSERT(sizeof(Perf_Counter) >= 64);

	static Perf_Counter* perf_counters_linked_list = NULL;
	static i32 perf_counters_running_count = 0;

	EXPORT Perf_Counter_Running perf_counter_start(Perf_Counter* my_counter, i32 line, const char* file, const char* function, const char* name)
	{
		Perf_Counter_Running running = {0};
		running.start = platform_perf_counter();
		running.my_counter = my_counter;
		running.line = line;
		running.file = file;
		running.function = function;
		running.name = name;
	
		#if !defined(PROFILE_NO_DEBUG)
			platform_interlocked_increment32(&perf_counters_running_count);
			platform_interlocked_increment32(&my_counter->concurrent_running_counters);
		#endif

		return running;
	}
	
	EXPORT f64 perf_counter_get_ellapsed(Perf_Counter_Running running)
	{
		i64 delta = platform_perf_counter() - running.start;
		f64 freq = platform_perf_counter_frequency_d();

		return (f64) delta / freq;
	}

	FORCE_INLINE INTERNAL i64 _perf_counter_end(Perf_Counter_Running* running, bool detailed)
	{
		Perf_Counter* counter = running->my_counter;
		i64 delta = platform_perf_counter() - running->start;

		ASSERT(counter != NULL && delta >= 0 && "invalid Perf_Counter_Running submitted");
		ASSERT_MSG(running->stopped == false, "Perf_Counter_Running running counter stopped more than once!");

		bool save_one_time_stats = platform_interlocked_increment64(&counter->runs) == 1; 
		
		//only save the stats that dont need to be updated on the first run
		if(save_one_time_stats)
		{
			//platform_interlocked_excahnge64 sets the value pointed to by the first argument to the second argument and returns the original value
			//We use this to set the head to the newly added counter and save the old previous first node so we can reference it from our counter
			Perf_Counter* prev_head = (Perf_Counter*) platform_interlocked_excahnge64((volatile i64*) (void*) &perf_counters_linked_list, (i64) counter);

			counter->next = prev_head;
			counter->file = running->file;
			counter->line = (i32) running->line;
			counter->function = running->function;
			counter->name = running->name;
			counter->frquency = platform_perf_counter_frequency();
			counter->max_counter = INT64_MIN;
			counter->min_counter = INT64_MAX;
			counter->is_detailed = detailed;
			counter->mean_estimate = delta;
		}
	
		platform_interlocked_add64(&counter->counter, delta);
		running->stopped = true;
		#if !defined(PROFILE_NO_DEBUG)
			platform_interlocked_decrement32(&perf_counters_running_count);
			platform_interlocked_decrement32(&counter->concurrent_running_counters);
			ASSERT(perf_counters_running_count >= 0 && counter->concurrent_running_counters >= 0);
		#endif

		return delta;
	}

	EXPORT void perf_counter_end(Perf_Counter_Running* running)
	{
		_perf_counter_end(running, false);
	}
	
	EXPORT void perf_counter_end_detailed(Perf_Counter_Running* running)
	{
		Perf_Counter* counter = running->my_counter;
		i64 delta = _perf_counter_end(running, true);

		//Also calculates min, max, standard deviation
		i64 offset_delta = delta - counter->mean_estimate;
		platform_interlocked_add64(&counter->sum_of_squared_offset_counters, offset_delta*offset_delta);

		do {
			if(counter->min_counter <= delta)
				break;
		} while(platform_interlocked_compare_and_swap64(&counter->min_counter, counter->min_counter, delta) == false);

		do {
			if(counter->max_counter >= delta)
				break;
		} while(platform_interlocked_compare_and_swap64(&counter->max_counter, counter->max_counter, delta) == false);
	}

	INTERNAL f64 _save_div(f64 num, f64 den, f64 if_zero)
	{
		if(den == 0)
			return if_zero;
		else
			return num/den;
	}
	INTERNAL i64 _profile_get_counter_freq(Perf_Counter counter)
	{
		return counter.frquency ? counter.frquency : platform_perf_counter_frequency();
	}

	EXPORT f64 profile_get_counter_total_running_time_s(Perf_Counter counter)
	{
		return _save_div((f64) counter.counter, (f64) counter.frquency, 0);
	}
	EXPORT f64 profile_get_counter_average_running_time_s(Perf_Counter counter)
	{
		return _save_div((f64) counter.counter, (f64) (counter.frquency * counter.runs), 0);
	}
	
	EXPORT f64 profile_get_counter_min_running_time_s(Perf_Counter counter)
	{
		if(counter.min_counter != INT64_MAX)
			return _save_div((f64) counter.min_counter, (f64) counter.frquency, 0);
		else
			return profile_get_counter_average_running_time_s(counter);
	}
	
	EXPORT f64 profile_get_counter_max_running_time_s(Perf_Counter counter)
	{
		if(counter.max_counter != INT64_MIN)
			return _save_div((f64) counter.max_counter, (f64) counter.frquency, 0);
		else
			return profile_get_counter_average_running_time_s(counter);
	}
	
	EXPORT f64 profile_get_counter_standard_deviation_s(Perf_Counter counter)
	{
		if(counter.is_detailed == false || counter.frquency == 0 || counter.runs <= 1)
			return 0;

		f64 freq = (f64) counter.frquency;
		f64 n = (f64) counter.runs;
        f64 sum = (f64) counter.counter - n*counter.mean_estimate;
        f64 sum2 = (f64) counter.sum_of_squared_offset_counters ;
            
        //Welford's algorithm for calculating varience
        f64 varience_ns = (sum2 - (sum * sum) / n) / (n - 1.0);

        //deviation = sqrt(varience) and deviation is unit dependent just like mean is
        f64 batch_deviation_ms = sqrt(fabs(varience_ns)) / freq;
		return batch_deviation_ms;
	}

	EXPORT f64 profile_get_counter_normalized_standard_deviation_s(Perf_Counter counter)
	{
		f64 stddev = profile_get_counter_standard_deviation_s(counter);
		f64 mean = profile_get_counter_average_running_time_s(counter);
		return _save_div(stddev, mean, 0);
	}
	
	EXPORT Perf_Counter* profile_get_counters()
	{
		return perf_counters_linked_list;
	}

	EXPORT i64 profile_get_total_running_counters_count()
	{
		return perf_counters_running_count;
	}
#endif