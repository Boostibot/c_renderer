#pragma once

#include "_test.h"
#include "hash_table.h"

#include <string.h>

void test_hash_table_stress(f64 time)
{
	enum Action {
		INIT,
		INIT_BACKED,
		DEINIT,
		//FIND, //We test find after every iteration
		INSERT,
		REMOVE,
		REHASH,
		RESERVE,

		ACTION_ENUM_COUNT
	};

	i32 probabilities[ACTION_ENUM_COUNT] = {0};
	probabilities[INIT]			= 1;
	probabilities[INIT_BACKED]  = 1;
	probabilities[DEINIT]		= 1;
	probabilities[INSERT]		= 100;
	probabilities[REMOVE]		= 50;
	probabilities[REHASH]		= 10;
	probabilities[RESERVE]		= 10;

	enum {
		MAX_ITERS = 1000*1000*10,
		MIN_ITERS = 100,
		BACKING = 1000,
		MAX_CAPACITY = 1000*10,

		NON_EXISTANT_KEYS_CHECKS = 100,
	};

	u64_Array truth_val_array = {0};
	u64_Array truth_key_array = {0};
	
	char buffer[BACKING] = {0};
	Hash_Table64 table = {0};

	Discrete_Distribution dist = random_discrete_make(probabilities, ACTION_ENUM_COUNT);
	//*random_state() = random_state_from_seed(0);

	u64 monotonic_key = 0;
	f64 start = clock_s();
	for(isize i = 0; i < MAX_ITERS; i++)
	{
		i32 action = random_discrete(dist);

		switch(action)
		{
			case INIT: {

				break;
			}

			case INIT_BACKED: {

				break;
			}

			case DEINIT: {

				break;
			}

			case INSERT: {
				u64 val = random_u64();

				//for the purposes of this test we need to ensure that 
				//key will always be unique so we generate only half random
				//and use monotonically increasing iteger for its higher half
				u64 key_half = random_u64();
				u64 key = (monotonic_key++ << 32) | (key_half >> 32);

				array_push(&truth_key_array, key);
				array_push(&truth_val_array, val);

				Hash_Found64 inserted = hash_table64_insert(&table, key, val);
				Hash_Found64 found = hash_table64_find(table, key);
				
				TEST(table.data != NULL);
				TEST(memcmp(&inserted, &found, sizeof found) == 0 && "The inserted value must be findable");

				break;
			}

			case REMOVE: {
				u64 removed_index = random_range(0, truth_val_array.size);

				TODO
				break;
			}

			case REHASH: {

				break;
			}

			case RESERVE: {

				break;
			}

		}
	
		//Test integrity of all current keys
		ASSERT(truth_key_array.size == truth_val_array.size);
		for(isize j = 0; j < truth_key_array.size; j++)
		{
			u64 key = truth_key_array.data[j];
			u64 val = truth_val_array.data[j];

			Hash_Found64 found = hash_table64_find(table, key);
			TEST(table.data != NULL);
			TEST(0 <= found.hash_index && found.hash_index < table.capacity && "The returned index must be valid");
			Hash_Table64_Entry entry = table.data[found.hash_index];
				
			TEST(entry.hash == key && entry.value == val && "The entry must be inserted properly");
			TEST(found.value == val);
		}

		//Test integrity of some non existant keys
		for(isize k = 0; k < NON_EXISTANT_KEYS_CHECKS; k++)
		{
			u64 key = random_u64();

			bool is_in_array = false;
			for(isize j = 0; j < truth_key_array.size; j++)
			{
				if(truth_key_array.data[j] == key)
				{
					is_in_array = true;
					break;
				}
			}

			if(is_in_array == false)
			{
				Hash_Found64 found = hash_table64_find(table, key);
				TEST(found.hash_index == -1 && found.value == 0 && "must not be found");
			}
		}
	}

	random_discrete_deinit(&dist);
	hash_table64_deinit(&table);
	array_deinit(&truth_key_array);
	array_deinit(&truth_val_array);
}

void test_hash_table()
{
	test_hash_table_stress(3.0);
}