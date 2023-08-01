#ifndef LIB_RANDOM
#define LIB_RANDOM

#include <stdint.h>

//Generates next random value
//Seed can be any value
//Taken from: https://prng.di.unimi.it/splitmix64.c
uint64_t random_splitmix(uint64_t* state);

//Generates next random value
//Seed must not be anywhere zero. Lower 3 bits have bias. Good for floating point
//Taken from: https://prng.di.unimi.it/xoshiro256plus.c
uint64_t random_xiroshiro256(uint64_t state[4]);

//generates random bool
bool     random_bool();	
//generates random float in range [0, 1)
float    random_f32();
//generates random double in range [0, 1)
double   random_f64(); 
//generates random u64 in range [0, U64_MAX] 
uint64_t random_u64();  
//generates random u64 in range [I64_MIN, U64_MAX] 
int64_t  random_i64();  
//generates unbiased random integer in range [from, to)
int64_t  random_range(int64_t from, int64_t to); 
//Randomly shuffles the provided array
void     random_shuffle(void* elements, int64_t element_count, int64_t element_size); 
//Swaps two values of the given size
void     swap_any(void* a, void* b, int64_t size); 

typedef struct {
	uint64_t seed;
	uint64_t state_splitmix[1];
	uint64_t state_xiroshiro256[4];
} Random_State;

//Generates a random seed using system clock and internal state
uint64_t     random_clock_seed();
//initializes Random_State using given seed
Random_State random_state_from_seed(uint64_t seed); 
//initialized Random_State using system clock
Random_State random_state_from_clock();

//Returns pointer to the global state currently used
Random_State* random_state();

//generates random bool
bool     random_state_bool(Random_State* state);	
//generates random float in range [0, 1)
float    random_state_f32(Random_State* state);
//generates random double in range [0, 1)
double   random_state_f64(Random_State* state); 
//generates random u64 in range [0, U64_MAX] 
uint64_t random_state_u64(Random_State* state);  
//generates random u64 in range [I64_MIN, U64_MAX] 
int64_t  random_state_i64(Random_State* state);  
//generates unbiased random integer in range [from, to)
int64_t  random_state_range(Random_State* state, int64_t from, int64_t to); 
//Randomly shuffles the provided array
void     random_state_shuffle(Random_State* state, void* elements, int64_t element_count, int64_t element_size); 

#endif

#if (defined(LIB_ALL_IMPL) || defined(LIB_RANDOM_IMPL)) && !defined(LIB_RANDOM_HAS_IMPL)
	#define LIB_RANDOM_HAS_IMPL
	#include <assert.h>
	#include <time.h>
	#include <string.h>

	#ifndef __cplusplus
	#include <stdbool.h>
	#endif

	#if defined(__GNUC__) || defined(__clang__)
		#define _RAND_THREAD_LOCAL __thread

		static uint32_t _count_leading_zeros(uint64_t number)
		{
			return (uint32_t) __builtin_clzll(number);
		}

	#elif defined(_MSC_VER)
		#define _RAND_THREAD_LOCAL __declspec(thread)

		#include <intrin.h>
		static uint32_t _count_leading_zeros(uint64_t number)
		{
			unsigned long index = 0;
			_BitScanReverse64(&index, number);
			uint32_t zeros = 63 - index;
			return (uint32_t) zeros;
		}
	#else
		#define _RAND_THREAD_LOCAL

		static uint32_t _count_leading_zeros(uint64_t number);
		#error "add a custom implementation for this compiler!"
	#endif

	_RAND_THREAD_LOCAL Random_State _random_state = {0};
	_RAND_THREAD_LOCAL uint64_t _random_clock_state = 0;
	_RAND_THREAD_LOCAL bool _random_clock_state_init = false;
	
	uint64_t random_splitmix(uint64_t* state) 
	{
		uint64_t z = (*state += 0x9e3779b97f4a7c15);
		z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9;
		z = (z ^ (z >> 27)) * 0x94d049bb133111eb;
		return z ^ (z >> 31);
	}

	static uint64_t _random_rotl(const uint64_t x, int k) {
		return (x << k) | (x >> (64 - k));
	}

	uint64_t random_xiroshiro256(uint64_t state[4]) 
	{
		const uint64_t result = state[0] + state[3];

		const uint64_t t = state[1] << 17;

		state[2] ^= state[0];
		state[3] ^= state[1];
		state[1] ^= state[2];
		state[0] ^= state[3];

		state[2] ^= t;

		state[3] = _random_rotl(state[3], 45);

		return result;
	}

	//@NOTE: Maybe having internal state in this function defeats its purpose since then its identical to random_u64()
	uint64_t random_clock_seed()
	{
		//if the test fails this does not compile
		int _static_assert_[sizeof(clock_t) <= sizeof(uint64_t) ? 1 : 0] = {0};
		uint64_t clock1 = 0;
		uint64_t clock2 = 0;
		
		uint64_t* clock_state = &_random_clock_state;
		bool* clock_init = &_random_clock_state_init;
		if(*clock_init == false)
		{
			*clock_init = true;
			*clock_state = (uint64_t) clock_state;
		}

		//uint64_t thread_hash = (uint64_t) clock_state;
		//thread_hash = _random_rotl(thread_hash, 16);

		* (clock_t*) (void*) &clock1 = clock();
		* (clock_t*) (void*) &clock2 = clock();

		*clock_state = clock1 ^ (clock2 << 32) ^ *clock_state;
		uint64_t seed = random_splitmix(clock_state);
		return seed;
	}
	
	Random_State random_state_from_seed(uint64_t seed)
	{
		Random_State out = {0};
		out.seed = seed;
		out.state_splitmix[0] = seed;

		//We use seed to generate longer seed ensuring none of the ints are ever 0
		for(uint64_t i = 0; i < 4; i++)
		{
			while(true)
			{
				out.state_xiroshiro256[i] = random_splitmix(out.state_splitmix); 
				if(out.state_xiroshiro256[i] != 0)
					break;
			}
		}

		return out;
	}

	Random_State random_state_from_clock()
	{
		uint64_t seed = random_clock_seed();
		Random_State out = random_state_from_seed(seed);
		return out;
	}

	static double _make_f64(uint64_t sign, uint64_t expoment, uint64_t mantissa)
	{
		uint64_t composite = (sign << 63) | (expoment << 52) | mantissa;
		double out = *(double*) (void*) &composite;
		return out;
	}

	static float _make_f32(uint32_t sign, uint32_t expoment, uint32_t mantissa)
	{
		uint32_t composite = (sign << 31) | (expoment << 23) | mantissa;
		float out = *(float*) (void*) &composite;
		return out;
	}

	float random_state_f32(Random_State* state)
	{
		uint64_t random = random_xiroshiro256(state->state_xiroshiro256);
		uint64_t mantissa = random >> (64 - 23);
		float random_f32 = _make_f32(0, 127, (uint32_t) mantissa) - 1;
		return random_f32;
	}

	double random_state_f64(Random_State* state)
	{
		uint64_t random = random_xiroshiro256(state->state_xiroshiro256);
		uint64_t mantissa = random >> (64 - 52);
		double random_f64 = _make_f64(0, 1023, mantissa) - 1;
		return random_f64;
	}

	bool random_state_bool(Random_State* state)
	{
		uint64_t random = random_splitmix(state->state_splitmix);
		bool out = random % 2 == 0;
		return out;
	}
	
	int64_t random_state_i64(Random_State* state)
	{
		int64_t random_i64 = (int64_t) random_splitmix(state->state_splitmix);
		return random_i64;
	}

	uint64_t random_state_u64(Random_State* state)
	{
		uint64_t random_u64 = random_splitmix(state->state_splitmix);
		return random_u64;
	}

	static uint64_t _random_bounded(Random_State* state, uint64_t range) 
	{
		uint64_t mask = ~(uint64_t) 0;

		--range;
		uint64_t index = _count_leading_zeros(range | 1);
		mask >>= index;
		uint64_t x = 0;
		do 
		{
			x = random_splitmix(state->state_splitmix) & mask;
		} 
		while (x > range);
		return x;
	}

	int64_t random_state_range(Random_State* state, int64_t from, int64_t to)
	{
		assert(to > from);
		uint64_t range = (uint64_t) (to - from);
		uint64_t bounded = _random_bounded(state, range);
		int64_t out = (int64_t) bounded + from;
		return out;
	}

	void swap_any(void* a, void* b, int64_t size)
	{
		enum {LOCAL = 64};
		char temp[LOCAL] = {0};
	
		int64_t repeats = size / LOCAL;
		int64_t remainder = size % LOCAL;
		int64_t exact = size - remainder;

		char* elemsi = (char*) a;
		char* elemsj = (char*) b;

		for(int64_t k = 0; k < repeats; k ++)
		{
			memcpy(temp,             elemsi + k*LOCAL, LOCAL);
			memcpy(elemsi + k*LOCAL, elemsj + k*LOCAL, LOCAL);
			memcpy(elemsj + k*LOCAL, temp,             LOCAL);
		}
			
		memcpy(temp,             elemsi + exact, remainder);
		memcpy(elemsi + exact,   elemsj + exact, remainder);
		memcpy(elemsj + exact,   temp,           remainder);
	}

	void random_state_shuffle(Random_State* state, void* elements, int64_t element_count, int64_t element_size)
	{
		enum {LOCAL = 256};
		char temp[LOCAL] = {0};
		char* elems = (char*) elements;
		int64_t s = element_size;
		int64_t c = element_count;

		if(element_size <= LOCAL)
		{
			for (int64_t i = 0; i < element_count - 1; i++)
			{
				uint64_t offset = _random_bounded(state, (uint64_t) (element_count - i));
				uint64_t j = offset + i;

				memcpy(temp,        elems + i*s, s);
				memcpy(elems + i*s, elems + j*s, s);
				memcpy(elems + j*s, temp,        s);
			}
		}
		else
		{
			for (int64_t i = 0; i < element_count - 1; i++)
			{
				uint64_t offset = _random_bounded(state, (uint64_t) (element_count - i));
				uint64_t j = offset + i;

				swap_any(elems + i*s, elems + j*s, s);
			}
		}
	}
	
	Random_State* random_state()
	{
		//If uninit initializes to random
		Random_State* state = &_random_state;
		if(state->state_xiroshiro256[0] == 0)
			*state = random_state_from_clock();

		return state;
	}

	void random_set_state(Random_State state)
	{
		Random_State* state_ptr = random_state();
		*state_ptr = state;
	}
	
	Random_State random_get_state()
	{
		Random_State state = *random_state();
		return state;
	}

	bool     random_bool() { return random_state_bool(random_state()); }	
	float    random_f32()  { return random_state_f32(random_state()); }
	double   random_f64()  { return random_state_f64(random_state()); } 
	uint64_t random_u64()  { return random_state_u64(random_state()); }  
	int64_t  random_i64()  { return random_state_i64(random_state()); }  

	int64_t  random_range(int64_t from, int64_t to) 
	{ 
		return random_state_range(random_state(), from, to); 
	}
	void     random_shuffle(void* elements, int64_t element_count, int64_t element_size) 
	{ 
		random_state_shuffle(random_state(), elements, element_count, element_size); 
	}

#endif