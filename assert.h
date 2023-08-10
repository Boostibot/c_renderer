#ifndef LIB_ASSERT
#define LIB_ASSERT

#include "platform.h"
#include <stdlib.h>

//Locally enables/disables asserts. If we wish to disable for part of
// code we simply undefine them then redefine them after.
#define DO_ASSERTS       /* enables assertions */
#define DO_ASSERTS_SLOW  /* enables slow assertions - expensive assertions or once that change the time complexity of an algorhitm */
#define DO_BOUNDS_CHECKS /* checks bounds prior to lookup */

//If x evaluates to false executes assertion_report(). 
#define TEST(x)                TEST_MSG(x, "")              /* executes always (even in release) */
#define ASSERT(x)              ASSERT_MSG(x, "")            /* is enabled by DO_ASSERTS */
#define ASSERT_SLOW(x)         ASSERT_SLOW_MSG(x, "")       /* is enabled by DO_ASSERTS_SLOW */
#define CHECK_BOUNDS(i, upper) CHECK_BOUNDS_EX(i, 0, upper) /* if i is not within [0, upper) panics. is enabled by DO_BOUNDS_CHECKS*/

#define TEST_MSG(x, msg)                    while(!(x)) {platform_trap(); assertion_report(#x, msg, __FILE__, __LINE__); platform_abort();}
#define ASSERT_MSG(x, msg)                  PP_IF(DO_ASSERTS,       TEST_MSG)(x, msg)
#define ASSERT_SLOW_MSG(x, msg)             PP_IF(DO_ASSERTS_SLOW,  TEST_MSG)(x, msg)
#define CHECK_BOUNDS_EX(i, lower, upper)    PP_IF(DO_ASSERTS,       TEST_MSG)((lower) <= (i) && (i) <= (upper), "Bounds check failed!")

//Doesnt do anything (failed branch) but still properly expands x and msg so it can be type checked.
//Dissabled asserts expand to this.
#define NO_ASSERT_MSG(x, msg)               while(0) {x; const char* _msg = (msg);}
#define NO_CHECK_BOUNDS_EX(i, lower, upper) while(0) {(lower) <= (i) && (i) <= (upper); }

//If dissabled expand to this
#define _IF_NOT_DO_ASSERTS(ignore)         NO_ASSERT_MSG
#define _IF_NOT_ASSERT_SLOW_MSG(ignore)    NO_ASSERT_MSG
#define _IF_NOT_CHECK_BOUNDS_EX(ignore)    NO_CHECK_BOUNDS_EX

//If fails does not compile. 
//x must be a valid compile time exception. 
//Is useful for validating if compile time settings are correct
#define STATIC_ASSERT(x) typedef char PP_CONCAT(__static_assertion_, __LINE__)[(x) ? 1 : -1]

//Gets called when assertion fails. 
//Does not have to terminate process since that is done at call site 
// (for easier debugging) by the assert macro itself.
//It is left unimplemented
void assertion_report(const char* expression, const char* message, const char* file, int line);

//Pre-Processor (PP) utils
#define PP_CONCAT2(a, b) a ## b
#define PP_CONCAT(a, b) PP_CONCAT2(a, b)
#define PP_ID(x) x

//if CONDITION_DEFINE is defined: expands to x, 
//else: expands to _IF_NOT_##CONDITION_DEFINE(x). See above how to use this.
//The reason for its use is that simply all other things I have tried either didnt
// work or failed to compose for obscure reasons
#define PP_IF(CONDITION_DEFINE, x)         PP_CONCAT(_IF_NOT_, CONDITION_DEFINE)(x)
#define _IF_NOT_(x) x

#endif