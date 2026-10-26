#ifndef LIBDEFER_H
#define LIBDEFER_H

#define DEFER_CONCAT_IMPL(a, b) a ## b
#define DEFER_CONCAT(a, b) DEFER_CONCAT_IMPL(a, b)

typedef void (*_defer_fn_t)(void *);

typedef struct {
    _defer_fn_t fn;
    void *arg;
} _defer_entry;

static inline void _defer_run(_defer_entry *e) {
    if (e && e->fn) {
        e->fn(e->arg);
    }
}

#define defer(_func, _val) \
    _defer_entry DEFER_CONCAT(_defer_var_, __COUNTER__) \
        __attribute__((cleanup(_defer_run))) \
        __attribute__((unused)) = { (_defer_fn_t)(_func), (void *)(_val) }

typedef struct {
    _defer_fn_t fn;
    void *arg;
    const volatile int *err_state;
} _errdefer_entry;

static inline void _errdefer_run(_errdefer_entry *e) {
    if (e && e->err_state && *(e->err_state) && e->fn) {
        e->fn(e->arg);
    }
}

#define DEFER_ERR_INIT volatile int _defer_err_state = 1

#define errdefer(_func, _val) \
    _errdefer_entry DEFER_CONCAT(_errdefer_var_, __COUNTER__) \
        __attribute__((cleanup(_errdefer_run))) \
        __attribute__((unused)) = { (_defer_fn_t)(_func), (void *)(_val), &_defer_err_state }

#define defer_return(...) \
    do { _defer_err_state = 0; return __VA_ARGS__; } while (0)

#endif
