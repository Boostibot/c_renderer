#pragma once

#include "defines.h"
#include "array.h"
#include "time.h"
#include "random.h"

#include <stdio.h>
#include <stdlib.h>

#ifndef TEST
	#define TEST(a) while(!(a)) { printf("Test failed! (%s : %d)\n%s\n on line", __FILE__, __LINE__, #a);  abort(); }
#endif

//Maybe move to random or to test
static void random_bits(Random_State* state, void* into, isize size)
{
	isize full_randoms = size / 8;
	isize remainder = size % 8;
	u64* fulls = (u64*) into;
	
	for(isize i = 0; i < full_randoms; i++)
		fulls[i] = random_u64(state);

	u64 last = random_u64(state);
	memcpy(&fulls[full_randoms], &last, remainder);
}

typedef struct Discrete_Distribution
{
	i32_Array prob_table;
	i32 prob_sum;
} Discrete_Distribution;

Discrete_Distribution random_discrete_make(const i32 probabilities[], isize probabilities_size)
{
	i32 prob_sum = 0;
	for(isize i = 0; i < probabilities_size; i++)
		prob_sum += probabilities[i];

	i32_Array prob_table = {0};
	array_resize(&prob_table, prob_sum);
	
	isize k = 0;
	for(isize i = 0; i < probabilities_size; i++)
	{
		isize end = k + probabilities[i];
		for(; k < end; k++)
		{
			CHECK_BOUNDS(k, prob_table.size);
			prob_table.data[k] = i;
		}
	}

	Discrete_Distribution out = {prob_table, prob_sum};
	return out;
}

i32 random_discrete(Discrete_Distribution distribution)
{
	i64 random = random_range(0, distribution.prob_sum);
	CHECK_BOUNDS(random, distribution.prob_table.size);
	i32 index = distribution.prob_table.data[random];
	return index;
}

void random_discrete_deinit(Discrete_Distribution* dist)
{
	array_deinit(&dist->prob_table);
	dist->prob_sum = 0;
}
