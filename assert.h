#ifndef LIB_ASSERT
#define LIB_ASSERT

#include "platform.h"
#include <stdlib.h>

//Locally enables/disables some checks use true/false.
//if we want to disable checks for a code block we undef it before the block
// and redefine it after the block
#define DO_ASSERTS       true /* enables assertions */
#define DO_ASSERTS_SLOW  true /* enables slow assertions for example checking if arrays are sorted */
#define DO_BOUNDS_CHECKS true /* checks bounds prior to lookup */

//Just like assert but gets called even in release. If x evaluates to false panics
#define TEST(x)                TEST_MSG(x, "");
#define ASSERT(x)              ASSERT_MSG(x, "");
#define ASSERT_SLOW(x)         ASSERT_SLOW_MSG(x, "");
#define CHECK_BOUNDS(i, upper) CHECK_BOUNDS_EX(i, 0, upper);

#define TEST_MSG(x, msg)                 while(!(x)) {assertion_report(#x, msg, __FILE__, __LINE__); platform_trap(); platform_abort();}
#define ASSERT_MSG(x, msg)               PP_IF_ELSE(DO_ASSERTS,       TEST_MSG(x, msg), 0)
#define ASSERT_SLOW_MSG(x, msg)          PP_IF_ELSE(DO_ASSERTS_SLOW,  TEST_MSG(x, msg), 0)
#define CHECK_BOUNDS_EX(i, lower, upper) PP_IF_ELSE(DO_BOUNDS_CHECKS, TEST_MSG((lower) <= (i) && (i) <= (upper), "Bounds check failed!"), 0)

//If fails does not compile. 
//c must be a valid compile time exception. 
//Is useful for validating
#define STATIC_ASSERT(x) typedef char static_assertion_##MSG[(x) ? 1 : -1]

//Gets called when assertion fails. 
//Does not have to terminate process since that is done at call site 
// (for easier debugging) by the assert macro itself.
//Is implemented inside log.h
void assertion_report(const char* expression, const char* message, const char* file, int line);


//============ IMPLEMENTATION =============================
//For C
#define PP_CONCAT2(a, b) a ## b
#define PP_CONCAT(a, b) PP_CONCAT2(a, b)
#define PP_ID(x) x

#define _PP_IF_1(if_true, if_false)     if_true
#define _PP_IF_true(if_true, if_false)  if_true
#define _PP_IF_0(if_true, if_false)     if_false
#define _PP_IF_(if_true, if_false)      if_false
#define _PP_IF_false(if_true, if_false) if_false

//Expands x only if cond is true/1
//when cond is false/0 or empty does nothing
#define PP_IF_ELSE(cond, if_true, if_false)  PP_CONCAT(_PP_IF_, cond)(if_true, if_false)

#endif