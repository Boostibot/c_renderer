#ifndef LIB_ALLOCATOR
#define LIB_ALLOCATOR

#include "defines.h"

// This module introduces a framework for dealing with memory and allocation used by every other system.
// It makes very little assumptions about the use case making it very portable to other projects.
//
// Since memory allocation is such a prevelent problem we try to maximize profiling and locality. 
// 
// We do this firstly by introducing a concept of Allocator. Allocators are structs that know how to allocate with the 
// advantage over malloc that they can be local and distinct for distinct tasks. This makes them faster and safer then malloc
// because we can localy see when something goes wrong. They can also be composed where allocators get their 
// memory from allocators above them (their 'parents'). This is especially useful for hierarchical resource management. 
// 
// By using a hierarchies we can guarantee that all memory will get freed by simply freeing the highest allocator. 
// This will work even if the lower allocators/systems leak, unlike malloc or other global alloctator systems where every 
// level has to be perfect.
// 
// Secondly we pass Source_Info to every allocation procedure. This means we on-the-fly swap our current allocator with debug allocator
// that for example logs all allocations and checks their correctness.
//
// We also keep two global allocator pointers. These are called 'default' and 'scratch' allocators. Each system requiring memory
// should use one of these two allocators for initialization (and then continue using that saved off pointer). 
// By convention scratch allocator is used for "internal" allocation and default allocator is used for comunicating with the outside world.
// Given a function that does some useful compuatation and returns an allocated reuslt it typically uses a fast scratch allocator internally
// and only allocates using the default allocator the returned result.
//
// This convention ensures that all allocation is predictable and fast (scratch allocators are most often stack allocators that 
// are perfectly suited for fast allocation - deallocation pairs). This approach also stacks. In each function we can simply upon entry
// instal the scratch allocator as the default allocator so that all internal functions will also comunicate to us using the fast scratch 
// allocator.

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
    Allocator* previous_default;
    Allocator* previous_scratch;
} Allocator_Swap;

#define DEF_ALIGN 8
#define SOURCE_INFO() BRACE_INIT(Source_Info){__LINE__, __FILE__, __FUNCTION__}

EXPORT Allocator_Out_Of_Memory_Func allocator_get_out_of_memory();
EXPORT void allocator_set_out_of_memory(Allocator_Out_Of_Memory_Func new_handler);

//Attempts to call the realloc funtion of the from_allocator. Can return nullptr indicating failiure
EXPORT void* allocator_try_reallocate(Allocator* from_allocator, isize new_size, void* old_ptr, isize old_size, isize align, Source_Info called_from);
//Calls the realloc function of from_allocator. If fails calls the currently installed Allocator_Out_Of_Memory_Func (panics). This should be used most of the time
EXPORT void* allocator_reallocate(Allocator* from_allocator, isize new_size, void* old_ptr, isize old_size, isize align, Source_Info called_from);
//Calls the realloc function of from_allocator to allocate, if fails panics
EXPORT void* allocator_allocate(Allocator* from_allocator, isize new_size, isize align, Source_Info called_from);
//Calls the realloc function of from_allocator to deallocate
EXPORT void allocator_deallocate(Allocator* from_allocator, void* old_ptr,isize old_size, isize align, Source_Info called_from);
//Retrieves stats from the allocator. The stats can be only partially filled.
EXPORT Allocator_Stats allocator_get_stats(Allocator* self);

EXPORT Allocator* allocator_get_default();
EXPORT Allocator* allocator_get_scratch();

EXPORT Allocator_Swap allocator_push_default(Allocator* new_default);
EXPORT Allocator_Swap allocator_push_scratch(Allocator* new_scratch);
EXPORT Allocator_Swap allocator_push_both(Allocator* new_allocator);
EXPORT void allocator_pop(Allocator_Swap popped); 

static inline bool  is_power_of_two(isize num);
static inline bool  is_power_of_two_zero(isize num);
static inline void* align_forward(void* ptr, isize align_to);
static inline void* align_backward(void* ptr, isize align_to);
static inline void* stack_allocate(isize bytes, isize align_to) {(void) align_to; (void) bytes; return NULL;}

#define PAGE_BYTES ((int64_t) 4096)
#define KIBI_BYTE ((int64_t) 1 << 10)
#define MEBI_BYTE ((int64_t) 1 << 20)
#define GIBI_BYTE ((int64_t) 1 << 30)
#define TEBI_BYTE ((int64_t) 1 << 40)

static inline bool is_power_of_two_zero(isize num) 
{
    usize n = (usize) num;
    return ((n & (n-1)) == 0);
}

static inline bool is_power_of_two(isize num) 
{
    return (num>0 && is_power_of_two_zero(num));
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

#endif

#if (defined(LIB_ALL_IMPL) || defined(LIB_ALLOCATOR_IMPL)) && !defined(LIB_ALLOCATOR_HAS_IMPL)
#define LIB_ALLOCATOR_HAS_IMPL

    #include <string.h>
    #include "array.h"
    #include "allocator_malloc.h"

    #include <stdlib.h>
    #include <stdarg.h>
    #include <stdio.h>

    static Allocator* _mask_allocator_bits(Allocator* self)
    {
        //mask off the lower 2 bits of allocator since allocators are required to be at least 4 aligned.
        //Those bits can be used internally to store some extra state (for example if deallocation is necessary)
        usize mask = ~(usize) 3;
        self = (Allocator*)((usize) self & mask);
        return self;
    }

    EXPORT void* allocator_try_reallocate(Allocator* from_allocator, isize new_size, void* old_ptr, isize old_size, isize align, Source_Info called_from)
    {
        Allocator* masked = _mask_allocator_bits(from_allocator);
        ASSERT(masked != NULL && masked->allocate != NULL);
        return masked->allocate(masked, new_size, old_ptr, old_size, align, called_from);
    }

    EXPORT void* allocator_reallocate(Allocator* from_allocator, isize new_size, void* old_ptr, isize old_size, isize align, Source_Info called_from)
    {
        void* obtained = allocator_try_reallocate(from_allocator, new_size, old_ptr, old_size, align, called_from);
        if(obtained == NULL && new_size != 0)
        {
            Allocator_Out_Of_Memory_Func out_of_memory = allocator_get_out_of_memory();
            out_of_memory(from_allocator, new_size, old_ptr, old_size, align, called_from, "");
        }

        return obtained;
    }

    EXPORT void* allocator_allocate(Allocator* from_allocator, isize new_size, isize align, Source_Info called_from)
    {
        return allocator_reallocate(from_allocator, new_size, NULL, 0, align, called_from);
    }

    EXPORT void allocator_deallocate(Allocator* from_allocator, void* old_ptr,isize old_size, isize align, Source_Info called_from)
    {
        allocator_reallocate(from_allocator, 0, old_ptr, old_size, align, called_from);
    }

    EXPORT Allocator_Stats allocator_get_stats(Allocator* self)
    {
        Allocator* masked = _mask_allocator_bits(self);
        ASSERT(masked != NULL && masked->get_stats != NULL);
        return masked->get_stats(masked);
    }

    static void default_out_of_memory_handler(struct Allocator* allocator, isize new_size, void* old_ptr, isize old_size, isize align, struct Source_Info called_from, const char*format_string, ...);

    //@TODO: make thread local
    THREAD_LOCAL Allocator_Out_Of_Memory_Func out_of_memory_func = default_out_of_memory_handler;
    THREAD_LOCAL Allocator* default_allocator = (Allocator*) (void*) &global_malloc_allocator;
    THREAD_LOCAL Allocator* scratch_allocator = (Allocator*) (void*) &global_malloc_allocator;

    EXPORT Allocator* allocator_get_default()
    {
        return default_allocator;
    }
    EXPORT Allocator* allocator_get_scratch()
    {
        return scratch_allocator;
    }

    EXPORT Allocator_Swap allocator_push_default(Allocator* new_default)
    {
        Allocator_Swap out = {0};
        out.previous_default = default_allocator;
        default_allocator = new_default;
        return out;
    }
    EXPORT Allocator_Swap allocator_push_scratch(Allocator* new_scratch)
    {
        Allocator_Swap out = {0};
        out.previous_scratch = scratch_allocator;
        scratch_allocator = new_scratch;
        return out;
    }
    EXPORT Allocator_Swap allocator_push_both(Allocator* new_allocator)
    {
        Allocator_Swap out = {0};
        out.previous_default = default_allocator;
        out.previous_scratch = scratch_allocator;
        default_allocator = new_allocator;
        scratch_allocator = new_allocator;
        return out;
    }

    EXPORT void allocator_pop(Allocator_Swap popped)
    {
        if(popped.previous_default != NULL)
            default_allocator = popped.previous_default;

        if(popped.previous_scratch != NULL)
            scratch_allocator = popped.previous_scratch;
    }

    static void default_out_of_memory_handler(struct Allocator* allocator, isize new_size, void* old_ptr, isize old_size, isize align, struct Source_Info called_from, 
                    const char* format_string, ...)
    {
        typedef long long int lli;
        Allocator_Stats stats = {0};
        if(allocator != NULL && allocator->get_stats != NULL)
            stats = allocator_get_stats(allocator);
        
        if(stats.type_name == NULL)
            stats.type_name = "<no type name>";

        if(stats.name == NULL)
            stats.name = "<no name>";

        fprintf(stderr, 
            "Allocator %s %s ran out of memory\n"
            "new_size:    %lld B\n"
            "old_size:    %lld B\n"
            "called from: \"%s\" %s() (%lld)\n",
            stats.type_name, stats.name, 
            (lli) new_size, (lli) old_size, 
            called_from.file, called_from.function, (lli) called_from.line
        );

        fprintf(stderr, 
            "Allocator_Stats:{\n"
            "  bytes_allocated: %lld\n"
            "  max_bytes_allocated: %lld\n"
            "  allocation_count: %lld\n"
            "  deallocation_count: %lld\n"
            "  reallocation_count: %lld\n"
            "}", 
            (lli) stats.bytes_allocated, (lli) stats.max_bytes_allocated, 
            (lli) stats.allocation_count, (lli) stats.deallocation_count, (lli) stats.reallocation_count
        );
    
        va_list args;
        va_start(args, format_string);
        vfprintf(stderr, format_string, args);
        va_end(args);

        abort();
    }

    EXPORT Allocator_Out_Of_Memory_Func allocator_get_out_of_memory()
    {
        return out_of_memory_func;
    }

    EXPORT void allocator_set_out_of_memory(Allocator_Out_Of_Memory_Func new_handler)
    {
        out_of_memory_func = new_handler;
    }
#endif