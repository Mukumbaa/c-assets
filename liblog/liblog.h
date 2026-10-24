#ifndef LIBLOG_H
#define LIBLOG_H

#include <stdio.h>
#include <time.h>
#include <string.h>
#include <stdbool.h>

typedef enum{
  LOG_TRACE,
  LOG_DEBUG,
  LOG_INFO,
  LOG_WARN,
  LOG_ERROR,
  LOG_FATAL,
}log_type;

typedef void (*log_LockFn)(bool lock, void *udata);

typedef struct {
  log_type min_log;
  log_type min_log_file;
  bool quiet;
  const char *output_file;
  FILE *file_handle;
  log_LockFn lock_function;
  void *udata;
} log_state;


extern void log_set_level(log_type lt);
extern void log_set_quiet(bool b);
extern void log_set_level_file(log_type lt);
extern void log_add_file(const char *filename, log_type lt);
extern void log_close_file(void);
extern void log_internal(log_type lt, const char *file, int line, const char *func, const char *fmt, ...);
#define log_trace(...) log_internal(LOG_TRACE,  __FILE__, __LINE__, __func__, __VA_ARGS__)
#define log_debug(...) log_internal(LOG_DEBUG,  __FILE__, __LINE__, __func__, __VA_ARGS__)
#define log_info(...) log_internal(LOG_INFO,  __FILE__, __LINE__, __func__, __VA_ARGS__)
#define log_warn(...) log_internal(LOG_WARN,  __FILE__, __LINE__, __func__, __VA_ARGS__)
#define log_error(...) log_internal(LOG_ERROR,  __FILE__, __LINE__, __func__, __VA_ARGS__)
#define log_fatal(...) log_internal(LOG_FATAL,  __FILE__, __LINE__, __func__, __VA_ARGS__)

extern void log_set_lock(log_LockFn function, void *udata);

#endif

