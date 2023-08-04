#ifndef LIB_ALLOCATOR
#define LIB_ALLOCATOR

#include "defines.h"
#include "assert.h"

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

typedef struct Allocator        Allocator;
typedef struct Allocator_Stats  Allocator_Stats;
typedef struct Source_Info      Source_Info;

typedef void* (*Allocator_Allocate_Func)(Allocator* self, isize new_size, void* old_ptr, isize old_size, isize align, Source_Info called_from);
typedef Allocator_Stats (*Allocator_Get_Stats_Func)(Allocator* self);

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

//Gets called when function requiring to always succeed fails an allocation - most often from allocator_reallocate
//Unless LIB_ALLOCATOR_NAKED is defined is left unimplemented.
//If user wannts some more dynamic system potentially enabling local handlers 
// they can implement it themselves
EXPORT void allocator_out_of_memory_func(
    Allocator* allocator, isize new_size, void* old_ptr, isize old_size, isize align, 
    Source_Info called_from, const char* format_string, ...);

EXPORT Allocator* allocator_get_default();
EXPORT Allocator* allocator_get_scratch();

EXPORT Allocator_Swap allocator_push_default(Allocator* new_default);
EXPORT Allocator_Swap allocator_push_scratch(Allocator* new_scratch);
EXPORT Allocator_Swap allocator_push_both(Allocator* new_allocator);
EXPORT void allocator_pop(Allocator_Swap popped); 

EXPORT bool  is_power_of_two(isize num);
EXPORT bool  is_power_of_two_zero(isize num);
EXPORT void* align_forward(void* ptr, isize align_to);
EXPORT void* align_backward(void* ptr, isize align_to);
EXPORT void* stack_allocate(isize bytes, isize align_to) {(void) align_to; (void) bytes; return NULL;}

#define CACHE_LINE  ((int64_t) 64)
#define PAGE_BYTES  ((int64_t) 4096)
#define KIBI_BYTE   ((int64_t) 1 << 10)
#define MEBI_BYTE   ((int64_t) 1 << 20)
#define GIBI_BYTE   ((int64_t) 1 << 30)
#define TEBI_BYTE   ((int64_t) 1 << 40)

typedef struct Malloc_Allocator
{
    Allocator allocator;
    const char* name;

    isize bytes_allocated;
    isize max_bytes_allocated;

    isize allocation_count;
    isize deallocation_count;
    isize reallocation_count;
} Malloc_Allocator;

EXPORT Malloc_Allocator malloc_allocator_make();

EXPORT void* _malloc_allocator_allocate(Allocator* self_, isize new_size, void* old_ptr, isize old_size, isize align, Source_Info called_from);
EXPORT Allocator_Stats _malloc_allocator_get_stats(Allocator* self_);

extern Malloc_Allocator global_malloc_allocator;

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

    #include "platform.h"

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
            allocator_out_of_memory_func(from_allocator, new_size, old_ptr, old_size, align, called_from, "");

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

    static THREAD_LOCAL Allocator* _default_allocator = &global_malloc_allocator.allocator;
    static THREAD_LOCAL Allocator* _scratch_allocator = &global_malloc_allocator.allocator;

    EXPORT Allocator* allocator_get_default()
    {
        return _default_allocator;
    }
    EXPORT Allocator* allocator_get_scratch()
    {
        return _scratch_allocator;
    }

    EXPORT Allocator_Swap allocator_push_default(Allocator* new_default)
    {
        Allocator_Swap out = {0};
        out.previous_default = _default_allocator;
        _default_allocator = new_default;
        return out;
    }
    EXPORT Allocator_Swap allocator_push_scratch(Allocator* new_scratch)
    {
        Allocator_Swap out = {0};
        out.previous_scratch = _scratch_allocator;
        _scratch_allocator = new_scratch;
        return out;
    }
    EXPORT Allocator_Swap allocator_push_both(Allocator* new_allocator)
    {
        Allocator_Swap out = {0};
        out.previous_default = _default_allocator;
        out.previous_scratch = _scratch_allocator;
        _default_allocator = new_allocator;
        _scratch_allocator = new_allocator;
        return out;
    }

    EXPORT void allocator_pop(Allocator_Swap popped)
    {
        if(popped.previous_default != NULL)
            _default_allocator = popped.previous_default;

        if(popped.previous_scratch != NULL)
            _scratch_allocator = popped.previous_scratch;
    }

    #ifdef LIB_ALLOCATOR_NAKED
    #define _CRT_SECURE_NO_WARNINGS
    #include <stdlib.h>
    #include <stdarg.h>
    #include <stdio.h>
    EXPORT void allocator_out_of_memory_func(
        struct Allocator* allocator, isize new_size, void* old_ptr, isize old_size, isize align, 
        void* context, Source_Info called_from, 
        const char* format_string, ...)
    {
    
        fprintf(stderr, "Allocator run out of memory! with message:" );
        
        va_list args;
        va_start(args, format_string);
        vfprintf(stderr, format_string, args);
        va_end(args);

        abort();
    }
    #endif // !
    
    EXPORT bool is_power_of_two_zero(isize num) 
    {
        usize n = (usize) num;
        return ((n & (n-1)) == 0);
    }

    EXPORT bool is_power_of_two(isize num) 
    {
        return (num>0 && is_power_of_two_zero(num));
    }

    EXPORT void* align_forward(void* ptr, isize align_to)
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

    EXPORT void* align_backward(void* ptr, isize align_to)
    {
        assert(is_power_of_two(align_to));

        usize ualign = (usize) align_to;
        usize mask = ~(ualign - 1);
        usize ptr_num = (usize) ptr;
        ptr_num = ptr_num & mask;

        return (void*) ptr_num;
    }

    Malloc_Allocator global_malloc_allocator = {_malloc_allocator_allocate, _malloc_allocator_get_stats, "global_malloc_allocator"};

    EXPORT Malloc_Allocator malloc_allocator_make()
    {
        Malloc_Allocator malloc = {0};
        malloc.allocator.allocate = _malloc_allocator_allocate;
        malloc.allocator.get_stats = _malloc_allocator_get_stats;
        return malloc;
    }

    EXPORT void* _malloc_allocator_allocate(Allocator* self_, isize new_size, void* old_ptr, isize old_size, isize align, Source_Info called_from)
    {
        Malloc_Allocator* self = (Malloc_Allocator*) (void*) self_;
        isize size_delta = new_size - old_size;
        self->bytes_allocated += size_delta;

        void* out_ptr = NULL;
        if(old_ptr == NULL)
        {
            self->allocation_count += 1;
            out_ptr = platform_heap_reallocate(new_size, NULL, 0, align);
        }
        else if(new_size == 0)
        {
            self->deallocation_count += 1;
            platform_heap_reallocate(0, old_ptr, old_size, align);
            return NULL;                
        }
        else
        {
            self->reallocation_count += 1;
            out_ptr = platform_heap_reallocate(new_size, old_ptr, old_size, align);
        }
    
        if(out_ptr == NULL)
            self->bytes_allocated -= size_delta;
    
        if(self->max_bytes_allocated < self->bytes_allocated)
            self->max_bytes_allocated = self->bytes_allocated;

        return out_ptr;
    }

    EXPORT Allocator_Stats _malloc_allocator_get_stats(Allocator* self_)
    {
        Malloc_Allocator* self = (Malloc_Allocator*) (void*) self_;
        Allocator_Stats out = {0};
        out.type_name = "Malloc_Allocator";
        out.name = self->name;
        out.parent = NULL;
        out.is_top_level = true;
        out.max_bytes_allocated = self->max_bytes_allocated;
        out.bytes_allocated = self->bytes_allocated;
        out.allocation_count = self->allocation_count;
        out.deallocation_count = self->deallocation_count;
        out.reallocation_count = self->reallocation_count;

        return out;
    }
#endif