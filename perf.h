#ifndef LIB_PERF
#define LIB_PERF

#include "platform.h"
#include "defines.h"
#include "assert.h"
#include <math.h>

typedef struct Perf_Counter {
	i64 counter;
	i64 runs;
	i64 frquency;
	i64 mean_estimate;
	i64 sum_of_squared_offset_counters;
	i64 max_counter;
	i64 min_counter;
} Perf_Counter;

typedef struct Perf_Counter_Running {
	i64 start;
} Perf_Counter_Running;

typedef struct Perf_Counter_Stats {
	i64 runs;
	i64 batch_size;

	f64 total_s;
	f64 average_s;
	f64 min_s;
	f64 max_s;
	f64 standard_deviation_s;
	f64 normalized_standard_deviation_s; //(σ/μ)
} Perf_Counter_Stats;

EXPORT Perf_Counter_Running perf_counter_start();
EXPORT void					perf_counter_end(Perf_Counter* counter, Perf_Counter_Running running);
EXPORT void					perf_counter_end_interlocked(Perf_Counter* counter, Perf_Counter_Running running);
EXPORT i64					perf_counter_end_interlocked_custom(Perf_Counter* counter, Perf_Counter_Running running, bool detailed);
EXPORT f64					perf_counter_get_ellapsed(Perf_Counter_Running running);
EXPORT Perf_Counter_Stats	perf_counter_get_stats(Perf_Counter counter, i64 batch_size);
#endif

#if (defined(LIB_ALL_IMPL) || defined(LIB_PERF_IMPL)) && !defined(LIB_PERF_HAS_IMPL)
#define LIB_PERF_HAS_IMPL

	EXPORT Perf_Counter_Running perf_counter_start()
	{
		Perf_Counter_Running running = {platform_perf_counter()};
		return running;
	}
	
	EXPORT f64 perf_counter_get_ellapsed(Perf_Counter_Running running)
	{
		i64 delta = platform_perf_counter() - running.start;
		static f64 freq = 0;
		if(freq == 0)
			freq = (f64) platform_perf_counter_frequency();

		return (f64) delta / freq;
	}

	EXPORT void perf_counter_end(Perf_Counter* counter, Perf_Counter_Running running)
	{
		i64 delta = platform_perf_counter() - running.start;
		ASSERT(counter != NULL && delta >= 0 && "invalid Global_Perf_Counter_Running submitted");

		counter->runs += 1; 
		if(counter->runs == 1)
		{
			counter->frquency = platform_perf_counter_frequency();
			counter->max_counter = INT64_MIN;
			counter->min_counter = INT64_MAX;
			counter->mean_estimate = delta;
		}
	
		i64 offset_delta = delta - counter->mean_estimate;
		counter->counter += delta;
		counter->sum_of_squared_offset_counters += offset_delta*offset_delta;
		counter->min_counter = MIN(counter->min_counter, delta);
		counter->max_counter = MAX(counter->max_counter, delta);
	}
	
	EXPORT i64 perf_counter_end_interlocked_custom(Perf_Counter* counter, Perf_Counter_Running running, bool detailed)
	{
		i64 delta = platform_perf_counter() - running.start;

		ASSERT(counter != NULL && delta >= 0 && "invalid Global_Perf_Counter_Running submitted");
		i64 runs = platform_interlocked_increment64(&counter->runs); 
		
		//only save the stats that dont need to be updated on the first run
		if(runs == 1)
		{
			counter->frquency = platform_perf_counter_frequency();
			counter->max_counter = INT64_MIN;
			counter->min_counter = INT64_MAX;
			counter->mean_estimate = delta;
		}
	
		platform_interlocked_add64(&counter->counter, delta);

		if(detailed)
		{
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

		return runs;
	}
	
	EXPORT void perf_counter_end_interlocked(Perf_Counter* counter, Perf_Counter_Running running)
	{
		perf_counter_end_interlocked_custom(counter, running, true);
	}
	EXPORT Perf_Counter_Stats perf_counter_get_stats(Perf_Counter counter, i64 batch_size)
	{
		if(batch_size <= 0)
			batch_size = 1;

		if(counter.frquency == 0)
			counter.frquency = platform_perf_counter_frequency();

		ASSERT_MSG(counter.min_counter * counter.runs <= counter.counter, "min must be smaller than sum");
		ASSERT_MSG(counter.max_counter * counter.runs >= counter.counter, "max must be bigger than sum");
        
		//batch_size is in case we 'batch' our tested function: 
		// ie instead of running the tested function once we run it 100 times
		// this just means that each run is multiplied batch_size times
		i64 iters = batch_size * (counter.runs);
        
		f64 batch_deviation_s = 0;
		if(counter.runs > 1)
		{
			f64 n = (f64) counter.runs;
			f64 sum = (f64) counter.counter;
			f64 sum2 = (f64) counter.sum_of_squared_offset_counters;
            
			//Welford's algorithm for calculating varience
			f64 varience_ns = (sum2 - (sum * sum) / n) / (n - 1.0);

			//deviation = sqrt(varience) and deviation is unit dependent just like mean is
			batch_deviation_s = sqrt(fabs(varience_ns)) / (f64) counter.frquency;
		}

		f64 total_s = 0.0;
		f64 mean_s = 0.0;
		f64 min_s = 0.0;
		f64 max_s = 0.0;

		ASSERT(counter.min_counter * counter.runs <= counter.counter);
		ASSERT(counter.max_counter * counter.runs >= counter.counter);
		if(counter.frquency != 0)
		{
			total_s = (f64) counter.counter / (f64) counter.frquency;
			min_s = (f64) counter.min_counter / (f64) (batch_size * counter.frquency);
			max_s = (f64) counter.max_counter / (f64) (batch_size * counter.frquency);
		}
		if(iters != 0)
			mean_s = total_s / iters;

		ASSERT(mean_s >= 0 && min_s >= 0 && max_s >= 0);

		//We assume that summing all running times in a batch 
		// (and then dividing by its size = making an average)
		// is equivalent to picking random samples from the original distribution
		// => Central limit theorem applies which states:
		// deviation_sampling = deviation / sqrt(samples)
        
		// We use this to obtain the original deviation
		// => deviation = deviation_sampling * sqrt(samples)
        
		// but since we also need to take the average of each batch
		// to get the deviation of a single element we get:
		// deviation_element = deviation_sampling * sqrt(samples) / samples
		//                   = deviation_sampling / sqrt(samples)

		f64 sqrt_batch_size = sqrt((f64) batch_size);
		Perf_Counter_Stats stats = {0};

		//since min and max are also somewhere within the confidence interval
		// keeping the same confidence in them requires us to also apply the same correction
		// to the distance from the mean (this time * sqrt_batch_size because we already 
		// divided by batch_size when calculating min_s)
		stats.min_s = mean_s + (min_s - mean_s) * sqrt_batch_size; 
		stats.max_s = mean_s + (max_s - mean_s) * sqrt_batch_size; 

		//the above correction can push min to be negative 
		// happens mostly with noop and generally is not a problem
		if(stats.min_s < 0.0)
			stats.min_s = 0.0;
		if(stats.max_s < 0.0)
			stats.max_s = 0.0;
	
		stats.total_s = total_s;
		stats.standard_deviation_s = batch_deviation_s / sqrt_batch_size;
		stats.average_s = mean_s; 
		stats.batch_size = batch_size;
		stats.runs = iters;

		if(stats.average_s > 0)
			stats.normalized_standard_deviation_s = stats.standard_deviation_s / stats.average_s;

		//statss must be plausible
		ASSERT(stats.runs >= 0);
		ASSERT(stats.batch_size >= 0);
		ASSERT(stats.total_s >= 0.0);
		ASSERT(stats.average_s >= 0.0);
		ASSERT(stats.min_s >= 0.0);
		ASSERT(stats.max_s >= 0.0);
		ASSERT(stats.standard_deviation_s >= 0.0);
		ASSERT(stats.normalized_standard_deviation_s >= 0.0);

		return stats;
	}
#endif