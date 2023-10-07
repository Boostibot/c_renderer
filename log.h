#ifndef LIB_LOG
#define LIB_LOG

#include "defines.h"
#include <stdarg.h>

typedef enum Log_Type {
    LOG_TYPE_INFO    = 0,
    LOG_TYPE_WARN    = 1,
    LOG_TYPE_ERROR   = 2,
    LOG_TYPE_FATAL   = 3,
    LOG_TYPE_DEBUG   = 4,
    LOG_TYPE_TRACE   = 5,
} Log_Type;

typedef struct Logger Logger;
typedef void (*Vlog_Func)(Logger* logger, const char* module, Log_Type type, isize indentation, const char* format, va_list args);

typedef struct Logger {
    Vlog_Func log;
} Logger;

//Returns the default used logger
EXPORT Logger* log_system_get_logger();
//Sets the default used logger. Returns a pointer to the previous logger so it can be restored later.
EXPORT Logger* log_system_set_logger(Logger* logger);

EXPORT void log_group_push();
EXPORT void log_group_pop();
EXPORT isize log_group_depth();

EXPORT void log(const char* module, Log_Type type, const char* format, ...);
EXPORT void vlog(const char* module, Log_Type type, const char* format, va_list args);

EXPORT void log_info(const char* module, const char* format, ...);
EXPORT void log_warn(const char* module, const char* format, ...);
EXPORT void log_error(const char* module, const char* format, ...);
EXPORT void log_fatal(const char* module, const char* format, ...);
EXPORT void log_debug(const char* module, const char* format, ...);
EXPORT void log_trace(const char* module, const char* format, ...);

#endif

#if (defined(LIB_ALL_IMPL) || defined(LIB_LOG_IMPL)) && !defined(LIB_LOG_HAS_IMPL)
#define LIB_LOG_HAS_IMPL

static Logger* _global_logger = NULL;
static isize _global_log_group_depth = 0;

EXPORT Logger* log_system_get_logger()
{
    return _global_logger;
}

EXPORT Logger* log_system_set_logger(Logger* logger)
{
    Logger* before = _global_logger;
    _global_logger = logger;
    return before;
}

EXPORT void log_group_push()
{
    _global_log_group_depth ++;
}
EXPORT void log_group_pop()
{
    _global_log_group_depth --;
}
EXPORT isize log_group_depth()
{
    return _global_log_group_depth;
}

EXPORT void vlog(const char* module, Log_Type type, const char* format, va_list args)
{
    if(_global_logger)
        _global_logger->log(_global_logger, module, type, _global_log_group_depth, format, args);
}

#define ARGS_CALL(execute)          \
        va_list args;               \
        va_start(args, format);     \
        execute;                    \
        va_end(args)                \

EXPORT void log(const char* module, Log_Type type, const char* format, ...)
{
    ARGS_CALL(vlog(module, type, format, args));
}

EXPORT void log_info(const char* module, const char* format, ...) {ARGS_CALL(vlog(module, LOG_TYPE_INFO, format, args));}
EXPORT void log_warn(const char* module, const char* format, ...) {ARGS_CALL(vlog(module, LOG_TYPE_WARN, format, args));}
EXPORT void log_error(const char* module, const char* format, ...) {ARGS_CALL(vlog(module, LOG_TYPE_ERROR, format, args));}
EXPORT void log_fatal(const char* module, const char* format, ...) {ARGS_CALL(vlog(module, LOG_TYPE_FATAL, format, args));}
EXPORT void log_debug(const char* module, const char* format, ...) {ARGS_CALL(vlog(module, LOG_TYPE_DEBUG, format, args));}
EXPORT void log_trace(const char* module, const char* format, ...) {ARGS_CALL(vlog(module, LOG_TYPE_TRACE, format, args));}

#undef ARGS_CALL

#endif
