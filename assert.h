#ifndef LIB_ASSERT
#define LIB_ASSERT

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

#define TEST_MSG(x, msg)                 PP_CONCAT(_TEST_, 1)(x, msg)
#define ASSERT_MSG(x, msg)               PP_CONCAT(_TEST_, DO_ASSERTS)(x, msg)
#define ASSERT_SLOW_MSG(x, msg)          PP_CONCAT(_TEST_, DO_ASSERTS_SLOW)(x, msg)
#define CHECK_BOUNDS_EX(i, lower, upper) PP_CONCAT(_TEST_, DO_BOUNDS_CHECKS)((lower) <= (i) && (i) <= (upper), "Bounds check failed!")

//If fails does not compile. 
//c must be a valid compile time exception. 
//Is useful for validating
#define STATIC_ASSERT(c)    enum { PP_CONCAT(__STATIC_ASSERT__, __LINE__) = 1 / (int)(c) }

//Gets called when assertion fails. 
//Does not have to terminate process since that is done at call site 
// (for easier debugging) by the assert macro itself.
//Is implemented inside log.h
void assertion_reporter_default_func(const char* expression, const char* message, const char* file, int line);

#define PP_CONCAT2(a, b) a ## b
#define PP_CONCAT(a, b) PP_CONCAT2(a, b)

//============ IMPLEMENTATION =============================
//For C
#define _TEST_1(x, msg) while(!(x)) {assertion_reporter_default_func(#x, msg, __FILE__, __LINE__); abort();}
#define _TEST_0(x, msg) /*nothing*/
#define _TEST_(x, msg)  /*nothing*/

//For c++
#define _TEST_true(x, msg) _TEST_1(x, msg)
#define _TEST_false(x, msg) _TEST_0(x, msg)


#endif