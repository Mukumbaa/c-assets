/* 
 * Single-header library
 * 
 * In EXACTLY ONE source file, define LIBLOG_IMPLEMENTATION before including this file:
 * #define LIBLOG_IMPLEMENTATION
 * #include "liblog.h"
 */

#ifndef LIBLOG_DEF
#ifdef __cplusplus
#define LIBLOG_DEF extern "C"
#else
#define LIBLOG_DEF extern
#endif
#endif

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


LIBLOG_DEF void log_set_level(log_type lt);
LIBLOG_DEF void log_set_quiet(bool b);
LIBLOG_DEF void log_set_level_file(log_type lt);
LIBLOG_DEF void log_add_file(const char *filename, log_type lt);
LIBLOG_DEF void log_close_file(void);
LIBLOG_DEF void log_internal(log_type lt, const char *file, int line, const char *func, const char *fmt, ...);
#define log_trace(...) log_internal(LOG_TRACE,  __FILE__, __LINE__, __func__, __VA_ARGS__)
#define log_debug(...) log_internal(LOG_DEBUG,  __FILE__, __LINE__, __func__, __VA_ARGS__)
#define log_info(...) log_internal(LOG_INFO,  __FILE__, __LINE__, __func__, __VA_ARGS__)
#define log_warn(...) log_internal(LOG_WARN,  __FILE__, __LINE__, __func__, __VA_ARGS__)
#define log_error(...) log_internal(LOG_ERROR,  __FILE__, __LINE__, __func__, __VA_ARGS__)
#define log_fatal(...) log_internal(LOG_FATAL,  __FILE__, __LINE__, __func__, __VA_ARGS__)

LIBLOG_DEF void log_set_lock(log_LockFn function, void *udata);

#endif

#ifdef LIBLOG_IMPLEMENTATION
#ifndef LIBLOG_IMPLEMENTED
#define LIBLOG_IMPLEMENTED

#include <stdarg.h>

static log_state ls = {
  .min_log = LOG_INFO,
  .min_log_file = LOG_INFO,
  .quiet = false,
  .output_file = NULL,
  .file_handle = NULL,

};

LIBLOG_DEF void log_set_level(log_type lt){
  ls.min_log = lt;
}

LIBLOG_DEF void log_set_quiet(bool b){
  ls.quiet = b;
}

LIBLOG_DEF void log_set_level_file(log_type lt){
  ls.min_log_file = lt;
}

LIBLOG_DEF void log_add_file(const char *filename, log_type lt){
  ls.min_log_file = lt;
  ls.output_file =  filename;
  FILE *fd = fopen(filename, "a");
  if(fd == NULL){
    printf("[ERROR] Could not open %s file for logs.\n", ls.output_file);
    return;
  }
  ls.file_handle = fd;
}

LIBLOG_DEF void log_close_file(void){
  if(ls.file_handle != NULL){
    fclose(ls.file_handle);
    ls.file_handle = NULL;
  }
}

static const char *log_level_to_string(log_type lt){
  switch (lt){
    case LOG_TRACE:
      return "[TRACE]";
    case LOG_DEBUG:
      return "[DEBUG]";
    case LOG_INFO:
      return "[INFO ]";
    case LOG_WARN:
      return "[WARN ]";
    case LOG_ERROR:
      return "[ERROR]";
    case LOG_FATAL:
      return "[FATAL]";
    default:
      return "[]";
  }
}

LIBLOG_DEF void log_set_lock(log_LockFn function, void *udata) {
    ls.lock_function = function;
    ls.udata = udata;
}

LIBLOG_DEF void log_internal(log_type lt, const char *file, int line, const char *func, const char *fmt, ...){
  if (ls.lock_function) {
    ls.lock_function(true, ls.udata);
  }
  if (lt < ls.min_log && lt < ls.min_log_file) {
    return;
  }
  if (!fmt) return;

  char buf[1024];

  time_t now = time(NULL);
  struct tm *t = localtime(&now);
  char buffer_time[32];
  strftime(buffer_time, sizeof(buffer_time), "%d-%m-%Y %H:%M:%S", t);
  const char *p1 = strrchr(file, '/');
  const char *p2 = strrchr(file, '\\');
  const char *base = (p1 > p2) ? p1 : p2;
  const char *filename = base ? base + 1 : file;

  int offset = snprintf(buf, sizeof(buf), "%s %s (%s:%s:%d) ", 
      log_level_to_string(lt), buffer_time, filename, func, line);

  if (offset < 0) return;
  if ((size_t)offset >= sizeof(buf)) {
    offset = sizeof(buf) - 1;
  }

  va_list args;
  va_start(args, fmt);
  vsnprintf(buf + offset, sizeof(buf) - offset, fmt, args);
  va_end(args);

  size_t len = strlen(buf);
  if (len < sizeof(buf) - 1) {
    buf[len] = '\n';
    buf[len + 1] = '\0';
  }

  if (lt >= ls.min_log && !ls.quiet) {
    if(lt >= LOG_TRACE && lt <= LOG_INFO){
      fprintf(stdout, "%s", buf);
    }else{
      fprintf(stderr, "%s", buf);
    }
  }

  if (lt >= ls.min_log_file && ls.file_handle != NULL) {
    fprintf(ls.file_handle, "%s", buf);
    fflush(ls.file_handle);
  }
  if (ls.lock_function) {
    ls.lock_function(false, ls.udata); // Rilascia il lock
  }
}

#endif // LIBLOG_IMPLEMENTED
#endif // LIBLOG_IMPLEMENTATION
