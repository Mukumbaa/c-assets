# liblog

A lightweight, robust, single-header logging library for standard C99.

`liblog` provides structured, multi-target logging with automatic contextual metadata (`__FILE__`, `__LINE__`, `__func__`, runtime timestamps), independent threshold filtering for terminal and file outputs, automated stream routing (`stdout` vs `stderr`), high-performance persistent file writes, and cross-platform thread-safety via lock callbacks.

---

## Table of Contents

- [Features](#features)
- [How to Include (Single-Header Pattern)](#how-to-include-single-header-pattern)
- [Log Levels & Severity Scale](#log-levels--severity-scale)
- [Quick Start](#quick-start)
- [API Reference](#api-reference)
  - [Logging Macros](#1-logging-macros)
  - [Terminal Configuration](#2-terminal-configuration)
  - [File Logging Configuration](#3-file-logging-configuration)
  - [Thread-Safety Configuration](#4-thread-safety-configuration)
  - [Internal Core Engine](#5-internal-core-engine)
- [Architecture & Design Details](#architecture--design-details)
  - [Stream Routing (stdout vs stderr)](#stream-routing-stdout-vs-stderr)
  - [Single-Buffer Memory Model](#single-buffer-memory-model)
  - [Persistent File Handle & Flushing](#persistent-file-handle--flushing)
  - [Thread-Safety & The Lock Callback Pattern](#thread-safety--the-lock-callback-pattern)
  - [Automatic Basename Sanitization](#automatic-basename-sanitization)
- [Multi-Threaded Example (POSIX pthreads)](#multi-threaded-example-posix-pthreads)

---

## Features

- **Single-Header Implementation:** Easy to drop into any project without external dependencies.
- **Zero-Dependency Thread Safety:** Built-in lock callback hook guarantees atomic logging across threads without forcing hard dependencies on `<pthread.h>` or `<windows.h>`.
- **Contextual Metadata:** Automatically captures line numbers, calling functions, source filenames, and timestamps (`DD-MM-YYYY HH:MM:SS`).
- **Dual Independent Thresholds:** Set different log level thresholds for terminal output and log files simultaneously.
- **POSIX Stream Separation:** Directs informational logs to `stdout` and warnings/errors to `stderr`.
- **Persistent Disk Buffering:** Keeps log files open across calls with explicit `fflush()` per write, avoiding expensive repeated `fopen`/`fclose` syscalls.
- **Single-Buffer Engine:** Avoids Variable Length Arrays (VLAs) and multiple allocations on the stack by formatting directly through chained offsets.

---

## How to Include (Single-Header Pattern)

`liblog` follows the standard single-header library paradigm (like `stb` libraries).

1. In **exactly one** `.c` file (usually `main.c`), define `LIBLOG` before including the header to compile the implementation:
   ```c
   #define LIBLOG
   #include "liblog.h"
   ```

2. In all other files where you want to log, include the header normally:
   ```c
   #include "liblog.h"
   ```

---

## Log Levels & Severity Scale

Log levels are defined in the `log_type` enum in ascending order of severity:

| Level | Integer | Target Stream | Typical Use Case |
| :--- | :---: | :---: | :--- |
| **`LOG_TRACE`** | `0` | `stdout` | Fine-grained execution tracing and loop internals |
| **`LOG_DEBUG`** | `1` | `stdout` | Diagnostic information needed only during development |
| **`LOG_INFO`** | `2` | `stdout` | Normal runtime events (service started, task completed) |
| **`LOG_WARN`** | `3` | `stderr` | Unexpected non-fatal occurrences or degraded conditions |
| **`LOG_ERROR`** | `4` | `stderr` | Failed operations where the application continues running |
| **`LOG_FATAL`** | `5` | `stderr` | Critical unrecoverable crashes preceding an exit or abort |

> **Filtering Rule:** A message is emitted if and only if:  
> `Message Level >= Minimum Threshold`

---

## Quick Start

```c
#define LIBLOG
#include "liblog.h"

int main(void) {
    // Terminal will show logs from INFO and above
    log_set_level(LOG_INFO);

    log_info("Application started successfully.");
    log_debug("This message is hidden by default.");

    // Switch to debug mode dynamically
    log_set_level(LOG_DEBUG);
    log_debug("Now debugging information is visible!");

    return 0;
}
```

**Terminal Output:**
```text
[INFO ] 16-10-2026 18:30:15 (main.c:main:7) Application started successfully.
[DEBUG] 16-10-2026 18:30:15 (main.c:main:12) Now debugging information is visible!
```

---

## API Reference

### 1. Logging Macros

Logging is performed via variadic macros that transparently inject `__FILE__`, `__LINE__`, and `__func__`.

#### `log_trace(fmt, ...)`
#### `log_debug(fmt, ...)`
#### `log_info(fmt, ...)`
#### `log_warn(fmt, ...)`
#### `log_error(fmt, ...)`
#### `log_fatal(fmt, ...)`

- **Parameters:** Standard `printf`-style format string and optional variadic arguments.
- **Format:** Emits a line structured as:  
  `[LEVEL] DD-MM-YYYY HH:MM:SS (filename:function:line) <formatted_message>\n`

---

### 2. Terminal Configuration

#### `void log_set_level(log_type lt)`
Sets the minimum severity threshold for terminal output. Messages below this level are discarded before any formatting takes place.
- **Parameters:** `lt` – Target threshold level (`LOG_TRACE` through `LOG_FATAL`). Default is `LOG_INFO`.

#### `void log_set_quiet(bool b)`
Toggles quiet mode. When enabled, terminal output is completely suppressed regardless of the active log level. Output to file (if enabled) remains active.
- **Parameters:** `b` – Pass `true` to silence the terminal, `false` to restore normal printing.

---

### 3. File Logging Configuration

#### `void log_add_file(const char *filename, log_type lt)`
Configures and opens a persistent log file in append mode (`"a"`). If a previous file handle was already open, it is safely flushed and closed before opening the new target.
- **Parameters:**
  - `filename` – Path to the destination file on disk.
  - `lt` – Minimum threshold level for messages written to this file.
- **Behavior:** If `fopen` fails, an error is printed to `stdout` and file logging remains inactive without crashing.

#### `void log_set_level_file(log_type lt)`
Updates the minimum severity threshold for the active log file without re-opening the underlying file handle.
- **Parameters:** `lt` – Target threshold level. Default is `LOG_INFO`.

#### `void log_close_file(void)`
Flushes and closes the active file handle (if one is open) and resets the internal handle pointer to `NULL`.
- **Note:** Always call this function before your application exits to ensure clean resource termination.

---

### 4. Thread-Safety Configuration

#### `typedef void (*log_LockFn)(bool lock, void *udata);`
Callback signature for acquiring and releasing synchronization primitives.
- `lock`: `true` indicates the logger is requesting acquisition of the lock; `false` requests release.
- `udata`: Custom user pointer passed through from `log_set_lock()`.

#### `void log_set_lock(log_LockFn fn, void *udata)`
Registers a user-defined mutex/synchronization callback to make `liblog` thread-safe across all concurrent calls.
- **Parameters:**
  - `fn` – Function pointer conforming to `log_LockFn` (or `NULL` to disable locking).
  - `udata` – Arbitrary pointer to the mutex or context passed directly to `fn`.
- **Behavior:** Once registered, every call to `log_*()` will acquire the lock before composing the message and release it immediately after writing to terminal and/or disk.

---

### 5. Internal Core Engine

#### `void log_internal(log_type lt, const char *file, int line, const char *func, const char *fmt, ...)`
The backend routine called by all `log_*` macros.
- **Behavior:**
  1. Checks `lt` against terminal and file thresholds; exits immediately if both criteria fail.
  2. Acquires the registered lock (if active).
  3. Formats header, timestamp, and message into an internal stack buffer (`1024` bytes).
  4. Dispatches the buffer to terminal streams and/or the active disk file.
  5. Releases the lock.

---

## Architecture & Design Details

### Stream Routing (`stdout` vs `stderr`)
`liblog` automatically splits terminal messages based on POSIX conventions:
- `LOG_TRACE`, `LOG_DEBUG`, `LOG_INFO` $\rightarrow$ **`stdout`**
- `LOG_WARN`, `LOG_ERROR`, `LOG_FATAL` $\rightarrow$ **`stderr`**

This allows developers and system administrators to redirect standard output while keeping alerts visible:
```bash
./my_program > output.txt
# Only WARN, ERROR, and FATAL messages will appear in the console!
```

### Single-Buffer Memory Model
Rather than allocating multiple temporary buffers (prefix, body, composite) or using Variable Length Arrays (VLAs) that risk stack overflow, `log_internal` formats everything into a single fixed `1024`-byte stack buffer using **offset chaining**:
1. `snprintf` writes the header and returns the number of written bytes (`offset`).
2. `vsnprintf` writes the user message directly to `buf + offset` bounded by `sizeof(buf) - offset`.
3. Trailing `\n` and `\0` are safely set in place.

### Persistent File Handle & Flushing
Opening and closing a file descriptor for every log line incurs significant operating system overhead (repeated `open`/`close` syscalls).  
`liblog` opens the file **once** via `log_add_file()` and uses `fflush()` immediately after every write. This guarantees both:
- **High throughput:** Near-zero syscall overhead per line.
- **Crash durability:** Data is flushed directly to disk immediately after emission.

### Thread-Safety & The Lock Callback Pattern
In multi-threaded environments, log operations run the risk of race conditions, garbled output (lines from different threads interweaving), and data corruption during file access.

Instead of binding `liblog` to a specific threading implementation like POSIX pthreads (`<pthread.h>`) or Win32 threads (`<windows.h>`), `liblog` relies on the **Lock Callback Pattern**:
- **Zero Overhead in Single-Threaded Code:** If no lock callback is registered, logging incurs zero locking penalties.
- **Universal Portability:** Works seamlessly with POSIX `pthread_mutex_t`, Windows `CRITICAL_SECTION`, C11 `mtx_t`, or custom spinlocks and RTOS mutexes.
- **Clean Call Sites:** Developers never need to manually surround individual `log_*()` statements with locks.

---

## Multi-Threaded Example (POSIX pthreads)

```c
#include <stdio.h>
#include <pthread.h>

#define LIBLOG
#include "liblog.h"

// 1. Define the user callback to bridge liblog with pthreads
static void log_lock_wrapper(bool lock, void *udata) {
    pthread_mutex_t *mutex = (pthread_mutex_t *)udata;
    if (lock) {
        pthread_mutex_lock(mutex);
    } else {
        pthread_mutex_unlock(mutex);
    }
}

void *worker_task(void *arg) {
    int id = *(int *)arg;

    // Threads can log freely without manual locking!
    log_info("Worker thread #%d started working.", id);
    log_debug("Worker #%d performing task calculations...", id);
    log_info("Worker thread #%d finished.", id);

    return NULL;
}

int main(void) {
    // 2. Initialize a local or global mutex
    pthread_mutex_t log_mutex;
    pthread_mutex_init(&log_mutex, NULL);

    // 3. Register the mutex with liblog once at startup
    log_set_lock(log_lock_wrapper, &log_mutex);

    log_set_level(LOG_DEBUG);
    log_add_file("multithread.log", LOG_DEBUG);

    log_info("Spawning worker threads...");

    pthread_t t1, t2;
    int id1 = 1, id2 = 2;

    pthread_create(&t1, NULL, worker_task, &id1);
    pthread_create(&t2, NULL, worker_task, &id2);

    pthread_join(t1, NULL);
    pthread_join(t2, NULL);

    log_info("All workers completed successfully.");

    // 4. Clean up resources
    log_close_file();
    pthread_mutex_destroy(&log_mutex);

    return 0;
}
```
