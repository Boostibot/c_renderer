#ifndef LIB_DEFINES
#define LIB_DEFINES

#include <stdint.h>
#include <stddef.h>
#include <assert.h>

#ifndef __cplusplus
#include <stdbool.h>
#endif

typedef int64_t    isize;
typedef uint64_t     usize;
//typedef ptrdiff_t   isize;
//typedef size_t      usize;

typedef uint8_t     u8;
typedef uint16_t    u16;
typedef uint32_t    u32;
typedef uint64_t    u64;

typedef int8_t      i8;
typedef int16_t     i16;
typedef int32_t     i32;
typedef int64_t     i64;

typedef bool        b8;
typedef uint16_t    b16;
typedef uint32_t    b32;
typedef uint64_t    b64;

typedef float       f32;
typedef double      f64;

#define MIN(a, b)   ((a) < (b) ? (a) : (b))
#define MAX(a, b)   ((a) > (b) ? (a) : (b))
#define CLAMP(value, low, high) MAX(low, MIN(value, high))
#define DIV_ROUND_UP(value, div_by) (((value) + (div_by) - 1) / (div_by))
#define SWAP(a_ptr, b_ptr, Type) \
    do { \
         Type temp = *(a_ptr); \
         *(a_ptr) = *(b_ptr); \
         *(b_ptr) = temp; \
    } while(0) \

#define PP_CONCAT2(a, b) a ## b
#define PP_CONCAT(a, b) PP_CONCAT2(a, b)
#define STATIC_ASSERT(c)    enum { PP_CONCAT(__STATIC_ASSERT__, __LINE__) = 1 / (int)(c) }

#define ASSERT(c)           assert(c)
#define ASSERT_CHEAP(c)     assert(c)
#define ASSERT_EXPENSIVE(c) assert(c)

//Locally enables/disables bounds checks. Can be set on per function basis
#define DO_BOUNDS_CHECKS true

#define _CHECK_BOUNDS_EX_true(i, lower, upper) ASSERT((lower) <= (i) && (i) <= (upper))
#define _CHECK_BOUNDS_EX_false(i, lower, upper) /*nothing*/

#define _CHECK_BOUNDS_EX_1(i, lower, upper) _CHECK_BOUNDS_EX_true(i, lower, upper)
#define _CHECK_BOUNDS_EX_0(i, lower, upper) _CHECK_BOUNDS_EX_false(i, lower, upper)

#define CHECK_BOUNDS_EX(i, lower, upper) PP_CONCAT(_CHECK_BOUNDS_EX_, DO_BOUNDS_CHECKS)(i, lower, upper)
#define CHECK_BOUNDS(i, upper) CHECK_BOUNDS_EX(i, 0, upper);

#ifdef __cplusplus
    #define BRACE_INIT(Struct_Type) Struct_Type
#else
    #define BRACE_INIT(Struct_Type) (Struct_Type)
#endif 

//@TODO
#ifndef EXPORT
    #define EXPORT
#endif
#ifndef INTERNAL
    #define INTERNAL static
#endif

#if defined(__GNUC__) || defined(__clang__)
    #define RESTRICT __restrict__
    #define FORCE_INLINE __attribute__((always_inline))
    #define NO_INLINE __attribute__((noinline))
    #define THREAD_LOCAL __thread
#elif defined(_MSC_VER)
    #define RESTRICT __restrict
    #define FORCE_INLINE __forceinline
    #define NO_INLINE __declspec(noinline)
    #define THREAD_LOCAL __declspec(thread)
#else
    #define RESTRICT
    #define FORCE_INLINE
    #define NO_INLINE
    #define THREAD_LOCAL __thread
#endif

#ifndef _MSC_VER
    #define __FUNCTION__ __func__
#endif

#endif