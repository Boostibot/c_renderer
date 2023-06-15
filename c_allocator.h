#pragma once
#include "c_defines.h"

struct Allocator;
struct Allocator_Stats;
struct Source_Info;

typedef void* (*Allocator_Allocate_Func)(struct Allocator* self, isize new_size, void* old_ptr, isize old_size, isize align, struct Source_Info called_from);
typedef struct Allocator_Stats (*Allocator_Get_Stats_Func)(struct Allocator* self);
typedef void (*Allocator_Out_Of_Memory_Func)(struct Allocator* self, isize new_size, void* old_ptr, isize old_size, isize align, struct Source_Info called_from, 
                const char* format_string, ...);

typedef struct Allocator
{
    Allocator_Allocate_Func allocate;
    Allocator_Get_Stats_Func get_stats;
} Allocator;

typedef struct Allocator_Stats
{
    //The allocator used to obtain memory reisributed by this allocator.
    //If is_top_level is set this should probably be NULL
    Allocator* parent;
    //Human readable name of the type 
    const char* type_name;
    //Optional human readable name of this specific allocator
    const char* name;
    //if doesnt use any other allocator to obtain its memory. For example malloc allocator or VM memory allocator have this set.
    bool is_top_level; 

    //The number of bytes used by the entire allocator including the size needed for book keeping
    isize bytes_used;
    //The number of bytes given out to the program by this allocator. (does NOT include book keeping bytes).
    //Might not be totally accurate but is required to be localy stable - if we allocate 100B and then deallocate 100B this should not change.
    //This can be used to accurately track memory leaks. (Note that if this field is simply not set and thus is 0 the above property is satisfied)
    isize bytes_allocated;
            
    isize max_bytes_used;       //maximum bytes_used during the enire lifetime of the allocator
    isize max_bytes_allocated;  //maximum bytes_allocated during the enire lifetime of the allocator

    isize allocation_count;     //The number of allocation requests (old_ptr == NULL). Does not include reallocs!
    isize deallocation_count;   //The number of deallocation requests (new_size == 0). Does not include reallocs!
    isize reallocation_count;   //The number of reallocation requests (*else*).
} Allocator_Stats;

typedef struct Source_Info
{
    int64_t line;
    const char* file;
    const char* function;
} Source_Info;

typedef struct Allocator_Swap
{
    Allocator* previous;
    Allocator* current;
    bool is_default;
} Allocator_Swap;

#define DEF_ALIGN 8
#define SOURCE_INFO() BRACE_INIT(Source_Info){__LINE__, __FILE__, __FUNCTION__}

EXPORT Allocator_Out_Of_Memory_Func allocator_get_out_of_memory();
EXPORT void allocator_set_out_of_memory(Allocator_Out_Of_Memory_Func new_handler);

//Attempts to call the realloc funtion of the from_allocator. Can return nullptr indicating failiure
EXPORT void* allocator_try_reallocate(Allocator* from_allocator, isize new_size, void* old_ptr, isize old_size, isize align, Source_Info called_from);
//Calls the realloc function of from_allocator. If fails calls the currently installed Allocator_Out_Of_Memory_Func. This should be used most of the time
EXPORT void* allocator_reallocate(Allocator* from_allocator, isize new_size, void* old_ptr, isize old_size, isize align, Source_Info called_from);
EXPORT void* allocator_allocate(Allocator* from_allocator, isize new_size, isize align, Source_Info called_from);
EXPORT void allocator_deallocate(Allocator* from_allocator, void* old_ptr,isize old_size, isize align, Source_Info called_from);
EXPORT Allocator_Stats allocator_get_stats(Allocator* self);

EXPORT Allocator* allocator_get_default();
EXPORT Allocator* allocator_get_scratch();

EXPORT Allocator_Swap allocator_push_default(Allocator* new_default);
EXPORT Allocator_Swap allocator_push_scratch(Allocator* new_default);
EXPORT void allocator_pop(Allocator_Swap popped); 

static inline bool  is_power_of_two(isize num);
static inline void* align_forward(void* ptr, isize align_to);
static inline void* align_backward(void* ptr, isize align_to);
static inline void* stack_allocate(isize bytes, isize align_to) {(void) align_to; (void) bytes; return NULL;}

#define PAGE_BYTES ((int64_t) 4096)
#define KIBI_BYTE ((int64_t) 1 << 10)
#define MEBI_BYTE ((int64_t) 1 << 20)
#define GIBI_BYTE ((int64_t) 1 << 30)
#define TEBI_BYTE ((int64_t) 1 << 40)

static inline bool is_power_of_two(isize num) 
{
    usize n = (usize) num;
    return (n>0 && ((n & (n-1)) == 0));
}

static inline void* align_forward(void* ptr, isize align_to)
{
    assert(is_power_of_two(align_to));

    //this is a little criptic but according to the iternet should be the fastest way of doing this
    // my benchmarks support this. 
    //(its about 50% faster than using div_round_up would be - even if we supply log2 alignment and bitshifts)
    usize mask = (usize) (align_to - 1);
    isize ptr_num = (isize) ptr;
    ptr_num += (-ptr_num) & mask;

    return (void*) ptr_num;
}

static inline void* align_backward(void* ptr, isize align_to)
{
    assert(is_power_of_two(align_to));

    usize ualign = (usize) align_to;
    usize mask = ~(ualign - 1);
    usize ptr_num = (usize) ptr;
    ptr_num = ptr_num & mask;

    return (void*) ptr_num;
}

#ifdef _MSC_VER
    #define stack_allocate(size, align) \
        ((align) <= 8) ? _alloca((size_t) size) : align_forward(_alloca((size_t) ((size) + (align)), align)
#else
    #define stack_allocate(size, align) \
        __builtin_alloca_with_align((size_t) size, (size_t) align)
#endif