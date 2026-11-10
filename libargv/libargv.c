#include "libargv.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <errno.h>
#include <limits.h>

static const char *argv_basename(const char *path) {
  if (!path) return "program";
  const char *p1 = strrchr(path, '/');
  const char *p2 = strrchr(path, '\\');
  const char *base = p1;
  if (p2 && (!base || p2 > base)) base = p2;
  return base ? base + 1 : path;
}

static void argv_normalize_short(const char *in, char *out, size_t out_sz) {
  if (!in || in[0] == '\0') {
    out[0] = '\0';
    return;
  }
  if (in[0] == '-') {
    snprintf(out, out_sz, "%s", in);
  } else {
    snprintf(out, out_sz, "-%s", in);
  }
}

static void argv_normalize_long(const char *in, char *out, size_t out_sz) {
  if (!in || in[0] == '\0') {
    out[0] = '\0';
    return;
  }
  if (in[0] == '-' && in[1] == '-') {
    snprintf(out, out_sz, "%s", in);
  } else if (in[0] == '-') {
    snprintf(out, out_sz, "-%s", in);
  } else {
    snprintf(out, out_sz, "--%s", in);
  }
}

extern void argv_set_output(CliParser *p, FILE *out) {
  if (p) p->output = out ? out : stdout;
}

extern void argv_set_env_prefix(CliParser *p, const char *prefix) {
  if (!p) return;
  if (prefix) {
    snprintf(p->env_prefix, sizeof(p->env_prefix), "%s", prefix);
  } else {
    p->env_prefix[0] = '\0';
  }
}

static void argv_get_effective_env(const CliParser *p, const CliArg *arg, char *out, size_t out_sz) {
  if (!out || out_sz == 0) return;
  out[0] = '\0';
  if (!p || !arg) return;

  if (arg->env_var) {
    snprintf(out, out_sz, "%s", arg->env_var);
    return;
  }
  if (!p->env_prefix[0] || !arg->long_flag[0]) {
    return;
  }

  char clean_pfx[64];
  size_t pi = 0;
  for (; p->env_prefix[pi] && pi < sizeof(clean_pfx) - 1; pi++) {
    clean_pfx[pi] = (char)toupper((unsigned char)p->env_prefix[pi]);
  }
  clean_pfx[pi] = '\0';

  const char *start = arg->long_flag;
  while (*start == '-') start++;

  char clean[64];
  size_t i = 0;
  for (; start[i] && i < sizeof(clean) - 1; i++) {
    clean[i] = (start[i] == '-') ? '_' : (char)toupper((unsigned char)start[i]);
  }
  clean[i] = '\0';
  snprintf(out, out_sz, "%s_%s", clean_pfx, clean);
}


extern void argv_init(CliParser *p, const char *program_name) {
  if (p == NULL) return;
  p->program_name = program_name;
  p->version = NULL;
  p->version_short[0] = '\0';
  p->version_extra = NULL;
  p->count = 0;
  p->capacity = ARGV_INIT_CAPACITY;
  p->rules = NULL;
  p->rule_count = 0;
  p->rule_capacity = 0;
  p->error[0] = '\0';
  p->env_prefix[0] = '\0';
  p->output = stdout;
  p->init_failed = false;
  p->args = (CliArg *)malloc(p->capacity * sizeof(CliArg));
  if (p->args == NULL) {
    p->capacity = 0;
    p->init_failed = true;
    snprintf(p->error, sizeof(p->error), "Memory allocation failed");
  }
}

extern void argv_free(CliParser *p) {
  if (p == NULL) return;
  if (p->args != NULL) {
    for (size_t i = 0; i < p->count; i++) {
      if (p->args[i].type == ARGV_TYPE_REST && p->args[i].target != NULL) {
        const char ***arr = (const char ***)p->args[i].target;
        if (arr != NULL && *arr != NULL) {
          free(*arr);
          *arr = NULL;
        }
      }
      if (p->args[i].choice_helps != NULL) {
        free(p->args[i].choice_helps);
        p->args[i].choice_helps = NULL;
      }
    }
    free(p->args);
    p->args = NULL;
  }
  if (p->rules != NULL) {
    free(p->rules);
    p->rules = NULL;
  }
  p->count = 0;
  p->capacity = 0;
  p->rule_count = 0;
  p->rule_capacity = 0;
  p->program_name = NULL; 
  p->version = NULL;
  p->version_short[0] = '\0';
  p->version_extra = NULL;
  p->error[0] = '\0';
  p->init_failed = false;
}

extern const char *argv_get_error(const CliParser *p) {
  return p ? p->error : "Parser null pointer";
}

static bool argv_ensure_capacity(CliParser *p) {
  if (p == NULL) return false;
  if (p->count < p->capacity) return true;

  size_t new_cap = (p->capacity == 0) ? ARGV_INIT_CAPACITY : p->capacity * 2;
  CliArg *new_args = (CliArg *)realloc(p->args, new_cap * sizeof(CliArg));
  if (new_args == NULL) {
    snprintf(p->error, sizeof(p->error), "Memory allocation failed");
    p->init_failed = true;
    return false;
  }
  p->args = new_args;
  p->capacity = new_cap;
  return true;
}

static void argv_add_arg_internal(CliParser *p, const char *name, const char *sh, const char *lng, 
    CliType type, void *target, size_t *count_target, bool required, bool is_pos, 
    const char *choices, const char *help) {
  if (p == NULL) return;
  if (target == NULL) {
    const char *disp = (lng && lng[0]) ? lng : ((sh && sh[0]) ? sh : (name ? name : "argument"));
    snprintf(p->error, sizeof(p->error), "Target pointer for argument '%s' cannot be NULL", disp);
    p->init_failed = true;
    return;
  }
  if (!argv_ensure_capacity(p)) return;

  CliArg *arg = &p->args[p->count++];
  arg->name = name;
  argv_normalize_short(sh, arg->short_flag, sizeof(arg->short_flag));
  argv_normalize_long(lng, arg->long_flag, sizeof(arg->long_flag));
  arg->type = type;
  arg->target = target;
  arg->count_target = count_target;
  arg->required = required;
  arg->is_positional = is_pos;
  arg->choices = choices;
  arg->choice_helps = NULL;
  arg->choice_help_count = 0;
  arg->choice_help_capacity = 0;
  arg->env_var = NULL;
  arg->default_val = NULL;
  arg->is_set = false;
  arg->help = help;
}

extern void argv_bind_bool(CliParser *p, const char *sh, const char *lng, bool *target, const char *help) {
  if (p == NULL) return;
  argv_add_arg_internal(p, NULL, sh, lng, ARGV_TYPE_BOOL, (void *)target, NULL, false, false, NULL, help);
}

extern void argv_bind_count(CliParser *p, const char *sh, const char *lng, int *target, const char *help) {
  if (p == NULL) return;
  argv_add_arg_internal(p, NULL, sh, lng, ARGV_TYPE_COUNT, (void *)target, NULL, false, false, NULL, help);
}

extern void argv_bind_char(CliParser *p, const char *sh, const char *lng, char *target, bool required, const char *choices, const char *help) {
  if (p == NULL) return;
  argv_add_arg_internal(p, NULL, sh, lng, ARGV_TYPE_CHAR, (void *)target, NULL, required, false, choices, help);
}

extern void argv_bind_str(CliParser *p, const char *sh, const char *lng, const char **target, bool required, const char *choices, const char *help) {
  if (p == NULL) return;
  argv_add_arg_internal(p, NULL, sh, lng, ARGV_TYPE_STR, (void *)target, NULL, required, false, choices, help);
}

extern void argv_bind_int(CliParser *p, const char *sh, const char *lng, int *target, bool required, const char *choices, const char *help) {
  if (p == NULL) return;
  argv_add_arg_internal(p, NULL, sh, lng, ARGV_TYPE_INT, (void *)target, NULL, required, false, choices, help);
}

extern void argv_bind_float(CliParser *p, const char *sh, const char *lng, double *target, bool required, const char *choices, const char *help) {
  if (p == NULL) return;
  argv_add_arg_internal(p, NULL, sh, lng, ARGV_TYPE_FLOAT, (void *)target, NULL, required, false, choices, help);
}

extern void argv_bind_pos(CliParser *p, const char *name, const char **target, bool required, const char *choices, const char *help) {
  if (p == NULL) return;
  argv_add_arg_internal(p, name, NULL, NULL, ARGV_TYPE_STR, (void *)target, NULL, required, true, choices, help);
}

extern void argv_bind_rest(CliParser *p, const char *name, const char ***target, size_t *count, bool required, const char *choices, const char *help) {
  if (p == NULL) return;
  if (target == NULL || count == NULL) {
    snprintf(p->error, sizeof(p->error), "Target and count pointers for rest argument '%s' cannot be NULL", name ? name : "rest");
    p->init_failed = true;
    return;
  }
  *target = NULL;
  *count = 0;
  argv_add_arg_internal(p, name, NULL, NULL, ARGV_TYPE_REST, (void *)target, count, required, true, choices, help);
}

static CliArg *argv_find_arg(CliParser *p, const char *ident) {
  if (!p || !ident) return NULL;
  while (*ident == ' ') ident++;
  if (!*ident) return NULL;

  for (size_t i = 0; i < p->count; i++) {
    CliArg *a = &p->args[i];
    if (a->name && strcmp(a->name, ident) == 0) return a;
    if (a->long_flag[0] != '\0') {
      if (strcmp(a->long_flag, ident) == 0) return a;
      if (strncmp(a->long_flag, "--", 2) == 0 && strcmp(a->long_flag + 2, ident) == 0) return a;
    }
    if (a->short_flag[0] != '\0') {
      if (strcmp(a->short_flag, ident) == 0) return a;
      if (a->short_flag[0] == '-' && strcmp(a->short_flag + 1, ident) == 0) return a;
    }
  }
  return NULL;
}

extern void argv_set_env(CliParser *p, const char *flag_or_name, const char *env_var) {
  if (p == NULL || flag_or_name == NULL || env_var == NULL) return;
  CliArg *arg = argv_find_arg(p, flag_or_name);
  if (arg) {
    arg->env_var = env_var;
  }
}

static bool argv_choice_exists_in_list(const CliArg *arg, const char *choice) {
  if (!arg || !arg->choices || !choice) return false;

  if (arg->type == ARGV_TYPE_CHAR && strchr(arg->choices, ',') == NULL && strchr(arg->choices, '|') == NULL) {
    if (choice[0] == '\0' || choice[1] != '\0') return false;
    return strchr(arg->choices, choice[0]) != NULL;
  }

  const char *p = arg->choices;
  while (*p) {
    while (*p == ' ' || *p == ',' || *p == '|') p++;
    if (!*p) break;

    const char *start = p;
    while (*p && *p != ',' && *p != '|' && *p != ' ') p++;
    size_t token_len = (size_t)(p - start);

    if (token_len == strlen(choice) && strncmp(start, choice, token_len) == 0) {
      return true;
    }
  }
  return false;
}

extern bool argv_set_choice_help(CliParser *p, const char *flag_or_name, const char *choice, const char *help) {
  if (!p || !flag_or_name || !choice || !help) return false;

  CliArg *arg = argv_find_arg(p, flag_or_name);
  if (!arg) {
    snprintf(p->error, sizeof(p->error), "Argument '%s' not found for choice help", flag_or_name);
    p->init_failed = true;
    return false;
  }

  if (!arg->choices) {
    snprintf(p->error, sizeof(p->error), "Argument '%s' has no choices defined", flag_or_name);
    p->init_failed = true;
    return false;
  }

  if (!argv_choice_exists_in_list(arg, choice)) {
    snprintf(p->error, sizeof(p->error), "Choice '%s' is not valid for argument '%s' (allowed: [%s])",
        choice, flag_or_name, arg->choices);
    p->init_failed = true;
    return false;
  }

  for (size_t i = 0; i < arg->choice_help_count; i++) {
    if (strcmp(arg->choice_helps[i].choice, choice) == 0) {
      arg->choice_helps[i].help = help;
      return true;
    }
  }

  if (arg->choice_help_count >= arg->choice_help_capacity) {
    size_t new_cap = (arg->choice_help_capacity == 0) ? 4 : arg->choice_help_capacity * 2;
    CliChoiceHelp *new_helps = (CliChoiceHelp *)realloc(arg->choice_helps, new_cap * sizeof(CliChoiceHelp));
    if (!new_helps) {
      snprintf(p->error, sizeof(p->error), "Memory allocation failed");
      p->init_failed = true;
      return false;
    }
    arg->choice_helps = new_helps;
    arg->choice_help_capacity = new_cap;
  }

  arg->choice_helps[arg->choice_help_count].choice = choice;
  arg->choice_helps[arg->choice_help_count].help = help;
  arg->choice_help_count++;
  return true;
}

static bool argv_add_rule_internal(CliParser *p, CliRuleType type, const char *target, const char *requires) {
  if (p == NULL) return false;
  if (p->rule_count >= p->rule_capacity) {
    size_t new_cap = (p->rule_capacity == 0) ? 4 : p->rule_capacity * 2;
    CliRule *new_rules = (CliRule *)realloc(p->rules, new_cap * sizeof(CliRule));
    if (!new_rules) {
      snprintf(p->error, sizeof(p->error), "Memory allocation failed");
      p->init_failed = true;
      return false;
    }
    p->rules = new_rules;
    p->rule_capacity = new_cap;
  }
  p->rules[p->rule_count].type = type;
  p->rules[p->rule_count].target = target;
  p->rules[p->rule_count].requires = requires;
  p->rule_count++;
  return true;
}

extern void argv_add_exclusive(CliParser *p, const char *flags_group) {
  if (p == NULL || flags_group == NULL) return;
  argv_add_rule_internal(p, ARGV_RULE_EXCLUSIVE, flags_group, NULL);
}

extern void argv_add_dependency(CliParser *p, const char *flag, const char *requires_flags) {
  if (p == NULL || flag == NULL || requires_flags == NULL) return;
  argv_add_rule_internal(p, ARGV_RULE_DEPENDENCY, flag, requires_flags);
}

static bool argv_check_choice(const CliArg *arg, const char *val_str, int int_v, double float_v) {
  if (!arg || !arg->choices || !val_str) return true;

  if (arg->type == ARGV_TYPE_CHAR && strchr(arg->choices, ',') == NULL && strchr(arg->choices, '|') == NULL) {
    return val_str[0] != '\0' && val_str[1] == '\0' && strchr(arg->choices, val_str[0]) != NULL;
  }

  const char *p = arg->choices;
  while (*p) {
    while (*p == ' ' || *p == ',' || *p == '|') p++;
    if (!*p) break;

    const char *start = p;
    while (*p && *p != ',' && *p != '|' && *p != ' ') p++;
    size_t token_len = (size_t)(p - start);

    if (arg->type == ARGV_TYPE_STR || arg->type == ARGV_TYPE_CHAR || arg->type == ARGV_TYPE_REST) {
      if (token_len == strlen(val_str) && strncmp(start, val_str, token_len) == 0) {
        return true;
      }
    } else if (arg->type == ARGV_TYPE_INT) {
      char token_buf[32];
      if (token_len < sizeof(token_buf)) {
        memcpy(token_buf, start, token_len);
        token_buf[token_len] = '\0';
        char *endptr;
        long choice_v = strtol(token_buf, &endptr, 10);
        if (*endptr == '\0' && (int)choice_v == int_v) {
          return true;
        }
      }
    } else if (arg->type == ARGV_TYPE_FLOAT) {
      char token_buf[32];
      if (token_len < sizeof(token_buf)) {
        memcpy(token_buf, start, token_len);
        token_buf[token_len] = '\0';
        char *endptr;
        double choice_v = strtod(token_buf, &endptr);
        if (*endptr == '\0' && choice_v == float_v) {
          return true;
        }
      }
    }
  }
  return false;
}

static size_t argv_count_choices(const CliArg *arg) {
  if (!arg || !arg->choices || arg->choices[0] == '\0') return 0;

  if (arg->type == ARGV_TYPE_CHAR && strchr(arg->choices, ',') == NULL && strchr(arg->choices, '|') == NULL) {
    return strlen(arg->choices);
  }

  size_t count = 0;
  const char *p = arg->choices;
  while (*p) {
    while (*p == ' ' || *p == ',' || *p == '|') p++;
    if (!*p) break;
    count++;
    while (*p && *p != ',' && *p != '|' && *p != ' ') p++;
  }
  return count;
}
static void argv_format_type_hint(const CliArg *arg, char *out, size_t out_size) {
  if (!arg || !out || out_size == 0) return;

  if (arg->choices && arg->choice_help_count < argv_count_choices(arg)) {
    snprintf(out, out_size, " {%s}", arg->choices);
    return;
  }
  switch (arg->type) {
    case ARGV_TYPE_STR:   snprintf(out, out_size, " <str>"); break;
    case ARGV_TYPE_INT:   snprintf(out, out_size, " <int>"); break;
    case ARGV_TYPE_FLOAT: snprintf(out, out_size, " <num>"); break;
    case ARGV_TYPE_CHAR:  snprintf(out, out_size, " <char>"); break;
    case ARGV_TYPE_REST:  snprintf(out, out_size, "..."); break;
    default: out[0] = '\0'; break;
  }
}

static void argv_print_wrapped_desc(FILE *stream, const char *left_col, size_t indent, size_t gutter, const char *text, size_t desc_width) {
  size_t left_len = left_col ? strlen(left_col) : 0;
  if (!stream) stream = stdout;

  if (left_col && left_len > 0) {
    fprintf(stream, "%s", left_col);
    if (left_len < indent) {
      fprintf(stream, "%*s", (int)(indent - left_len + gutter), "");
    } else {
      fprintf(stream, "%*s", (int)gutter, "");
    }
  } else {
    fprintf(stream, "%*s", (int)(indent + gutter), "");
  }

  if (!text || text[0] == '\0') {
    fputc('\n', stream);
    return;
  }

  size_t cont_indent = (left_len < indent) ? (indent + gutter) : (left_len + gutter);
  const char *p = text;
  size_t line_len = 0;
  bool first_word = true;

  while (*p) {
    while (*p == ' ') p++;
    if (!*p) break;

    const char *start = p;
    while (*p && *p != ' ') p++;
    size_t wlen = (size_t)(p - start);

    size_t needed = first_word ? wlen : (1 + wlen);
    if (!first_word && (line_len + needed > desc_width)) {
      fprintf(stream, "\n%*s", (int)cont_indent, "");
      line_len = 0;
      first_word = true;
    }

    if (!first_word) {
      fputc(' ', stream);
      line_len++;
    }
    fprintf(stream, "%.*s", (int)wlen, start);
    line_len += wlen;
    first_word = false;
  }
  fputc('\n', stream);
}

static void argv_format_long_display(const CliArg *arg, char *out, size_t out_sz) {
  if (!arg || !out || out_sz == 0) return;
  if (arg->long_flag[0] == '\0') {
    out[0] = '\0';
    return;
  }
  if (arg->type == ARGV_TYPE_BOOL && strncmp(arg->long_flag, "--", 2) == 0 && strncmp(arg->long_flag, "--no-", 5) != 0) {
    if (arg->target && *(bool *)arg->target) {
      snprintf(out, out_sz, "--[no-]%s", arg->long_flag + 2);
      return;
    }
  }
  snprintf(out, out_sz, "%s", arg->long_flag);
}

static void argv_print_single_help(const CliParser *p, const CliArg *arg) {
  if (p == NULL || arg == NULL) return;
  size_t desc_width = 50;
  FILE *out = p->output ? p->output : stdout;

  char eff_env[64] = {0};
  argv_get_effective_env(p, arg, eff_env, sizeof(eff_env));

  if (arg->is_positional) {
    fprintf(out, "%-23s<%s>\n", "Positional argument:", arg->name ? arg->name : "arg");
    fprintf(out, "%-23s%s\n", "Mandatory:", arg->required ? "Yes" : "No");
  } else {
    char long_disp[128] = "None";
    if (arg->long_flag[0] != '\0') {
      if (arg->type == ARGV_TYPE_BOOL && strncmp(arg->long_flag, "--", 2) == 0 && strncmp(arg->long_flag, "--no-", 5) != 0 && arg->target && *(bool *)arg->target) {
        snprintf(long_disp, sizeof(long_disp), "%s (negation: --no-%s)", arg->long_flag, arg->long_flag + 2);
      } else {
        snprintf(long_disp, sizeof(long_disp), "%s", arg->long_flag);
      }
    }
    fprintf(out, "%-23s%s\n", "Short flag:", arg->short_flag[0] ? arg->short_flag : "None");
    fprintf(out, "%-23s%s\n", "Long flag:", long_disp);
    fprintf(out, "%-23s%s\n", "Value required:", (arg->type != ARGV_TYPE_BOOL && arg->type != ARGV_TYPE_COUNT) ? "Yes" : "No");
    fprintf(out, "%-23s%s\n", "Mandatory:", arg->required ? "Yes" : "No");
  }
  if (arg->default_val && arg->default_val[0] != '\0') {
    fprintf(out, "%-23s%s\n", "Default:", arg->default_val);
  }
  if (eff_env[0] != '\0') {
    fprintf(out, "%-23s%s\n", "Environment variable:", eff_env);
  }
  if (arg->choices) {
    fprintf(out, "Allowed choices:\n");
    if (arg->choice_help_count > 0) {
      size_t max_clen = 0;
      for (size_t c = 0; c < arg->choice_help_count; c++) {
        size_t l = strlen(arg->choice_helps[c].choice);
        if (l > max_clen) max_clen = l;
      }
      for (size_t c = 0; c < arg->choice_help_count; c++) {
        char prefix[64];
        snprintf(prefix, sizeof(prefix), "  %-*s:", (int)max_clen, arg->choice_helps[c].choice);
        argv_print_wrapped_desc(out, prefix, strlen(prefix), 1, arg->choice_helps[c].help, desc_width);
      }
    }
  }

  fprintf(out, "\nDescription:\n");
  const char *desc = arg->help ? arg->help : "No documentation available.";
  argv_print_wrapped_desc(out, "", 2, 0, desc, desc_width);
}

extern void argv_print_help(const CliParser *p) {
  if (p == NULL) return;
  FILE *out = p->output ? p->output : stdout;

  size_t desc_width = 50;

  if (p->version && p->version[0] != '\0') {
    fprintf(out, "%s v%s\n\n", argv_basename(p->program_name), p->version);
  }

  fprintf(out, "Usage: %s [OPTIONS]", argv_basename(p->program_name));

  for (size_t i = 0; i < p->count; i++) {
    if (p->args[i].is_positional) {
      char hint[64];
      argv_format_type_hint(&p->args[i], hint, sizeof(hint));
      if (p->args[i].type == ARGV_TYPE_REST) {
        if (p->args[i].required) fprintf(out, " <%s...%s>", p->args[i].name, p->args[i].choices ? hint : "");
        else fprintf(out, " [%s...%s]", p->args[i].name, p->args[i].choices ? hint : "");
      } else {
        if (p->args[i].required) fprintf(out, " <%s%s>", p->args[i].name, p->args[i].choices ? hint : "");
        else fprintf(out, " [%s%s]", p->args[i].name, p->args[i].choices ? hint : "");
      }
    }
  }
  fprintf(out, "\n\n");

  size_t max_len = 20;
  for (size_t i = 0; i < p->count; i++) {
    const CliArg *arg = &p->args[i];
    if (arg->is_positional) continue;

    char long_disp[128];
    argv_format_long_display(arg, long_disp, sizeof(long_disp));

    bool has_short = (arg->short_flag[0] != '\0');
    bool has_long = (long_disp[0] != '\0');

    size_t len = 2;
    if (has_short && has_long) len += strlen(arg->short_flag) + 2 + strlen(long_disp);
    else if (has_short) len += strlen(arg->short_flag);
    else if (has_long) len += 6 + strlen(long_disp);

    char hint[64];
    argv_format_type_hint(arg, hint, sizeof(hint));
    len += strlen(hint);

    if (len > max_len && len < 42) max_len = len;
  }

  fprintf(out, "Options:\n");
  argv_print_wrapped_desc(out, "  -h, --help", max_len, 2, "Display this help message", desc_width);

  if (p->version && p->version[0] != '\0') {
    char v_buf[96] = {0};
    if (p->version_short[0] != '\0') {
      snprintf(v_buf, sizeof(v_buf), "  %s, --version", p->version_short);
    } else {
      snprintf(v_buf, sizeof(v_buf), "      --version");
    }
    argv_print_wrapped_desc(out, v_buf, max_len, 2, "Display version information", desc_width);
  }

  for (size_t i = 0; i < p->count; i++) {
    const CliArg *arg = &p->args[i];
    if (arg->is_positional) continue;

    char long_disp[128];
    argv_format_long_display(arg, long_disp, sizeof(long_disp));

    bool has_short = (arg->short_flag[0] != '\0');
    bool has_long = (long_disp[0] != '\0');

    char buf[256] = {0};
    if (has_short && has_long) snprintf(buf, sizeof(buf), "  %s, %s", arg->short_flag, long_disp);
    else if (has_short) snprintf(buf, sizeof(buf), "  %s", arg->short_flag);
    else if (has_long) snprintf(buf, sizeof(buf), "      %s", long_disp);

    char hint[64];
    argv_format_type_hint(arg, hint, sizeof(hint));
    strncat(buf, hint, sizeof(buf) - strlen(buf) - 1);

    char env_key[64] = {0};
    argv_get_effective_env(p, arg, env_key, sizeof(env_key));

    char full_desc[1024];
    size_t len = 0;
    full_desc[0] = '\0';

    if (arg->help && arg->help[0]) {
      len += snprintf(full_desc + len, sizeof(full_desc) - len, "%s", arg->help);
    }
    if (arg->required && len < sizeof(full_desc)) {
      len += snprintf(full_desc + len, sizeof(full_desc) - len, " [required]");
    }
    if (arg->default_val && arg->default_val[0] && len < sizeof(full_desc)) {
      len += snprintf(full_desc + len, sizeof(full_desc) - len, " [default: %s]", arg->default_val);
    }
    if (env_key[0] && len < sizeof(full_desc)) {
      len += snprintf(full_desc + len, sizeof(full_desc) - len, " [env: %s]", env_key);
    }

    argv_print_wrapped_desc(out, buf, max_len, 2, full_desc, desc_width);

    if (arg->choice_help_count > 0) {
      for (size_t c = 0; c < arg->choice_help_count; c++) {
        char left[128];
        snprintf(left, sizeof(left), "%*s  %s:", (int)max_len, "", arg->choice_helps[c].choice);
        size_t c_indent = strlen(left);
        argv_print_wrapped_desc(out, left, c_indent, 1, arg->choice_helps[c].help, desc_width > 6 ? desc_width - 6 : 30);
      }
    }
  }

  bool has_pos = false;
  for (size_t i = 0; i < p->count; i++) {
    if (p->args[i].is_positional) { has_pos = true; break; }
  }
  if (has_pos) {
    fprintf(out, "\nPositional Arguments:\n");
    for (size_t i = 0; i < p->count; i++) {
      const CliArg *arg = &p->args[i];
      if (!arg->is_positional) continue;
      char buf[256] = {0};
      char hint[64];
      argv_format_type_hint(arg, hint, sizeof(hint));
      if (arg->type == ARGV_TYPE_REST) {
        snprintf(buf, sizeof(buf), "  <%s...%s>", arg->name ? arg->name : "arg", arg->choices ? hint : "");
      } else {
        snprintf(buf, sizeof(buf), "  <%s%s>", arg->name ? arg->name : "arg", arg->choices ? hint : "");
      }

      char env_key[64] = {0};
      argv_get_effective_env(p, arg, env_key, sizeof(env_key));

      char full_desc[1024];
      size_t len = 0;
      full_desc[0] = '\0';

      if (arg->help && arg->help[0]) {
        len += snprintf(full_desc + len, sizeof(full_desc) - len, "%s", arg->help);
      }
      if (arg->required && len < sizeof(full_desc)) {
        len += snprintf(full_desc + len, sizeof(full_desc) - len, " [required]");
      }
      if (arg->default_val && arg->default_val[0] && len < sizeof(full_desc)) {
        len += snprintf(full_desc + len, sizeof(full_desc) - len, " [default: %s]", arg->default_val);
      }
      if (env_key[0] && len < sizeof(full_desc)) {
        len += snprintf(full_desc + len, sizeof(full_desc) - len, " [env: %s]", env_key);
      }

      argv_print_wrapped_desc(out, buf, max_len, 2, full_desc, desc_width);

      if (arg->choice_help_count > 0) {
        for (size_t c = 0; c < arg->choice_help_count; c++) {
          char left[128];
          snprintf(left, sizeof(left), "%*s  %s:", (int)max_len, "", arg->choice_helps[c].choice);
          size_t c_indent = strlen(left);
          argv_print_wrapped_desc(out, left, c_indent, 1, arg->choice_helps[c].help, desc_width > 6 ? desc_width - 6 : 30);
        }
      }
    }
  }
}

static CliArg *argv_find_flag(CliParser *p, const char *flag, bool is_short) {
  if (p == NULL || flag == NULL) return NULL;
  for (size_t j = 0; j < p->count; j++) {
    if (p->args[j].is_positional) continue;
    if (is_short && p->args[j].short_flag[0] != '\0' && strcmp(flag, p->args[j].short_flag) == 0) return &p->args[j];
    if (!is_short && p->args[j].long_flag[0] != '\0' && strcmp(flag, p->args[j].long_flag) == 0) return &p->args[j];
  }
  return NULL;
}

static int argv_parse_bool_str(const char *s) {
  static const char *const yes[] = {"1", "true", "yes", "on"};
  static const char *const no[] = {"0", "false", "no", "off"};
  if (!s) return -1;
  for (size_t i = 0; i < sizeof(yes) / sizeof(yes[0]); i++) {
    const char *a = s, *b = yes[i];
    while (*a && *b && tolower((unsigned char)*a) == *b) { a++; b++; }
    if (!*a && !*b) return 1;
  }
  for (size_t i = 0; i < sizeof(no) / sizeof(no[0]); i++) {
    const char *a = s, *b = no[i];
    while (*a && *b && tolower((unsigned char)*a) == *b) { a++; b++; }
    if (!*a && !*b) return 0;
  }
  return -1;
}

static CliStatus argv_assign_val(CliParser *p, CliArg *arg, const char *val_str, const char *flag_name) {
  if (!p || !arg || !val_str) return ARGV_ERR_INVALID_VALUE;

  bool bool_v = false;
  char char_v = 0;
  int int_v = 0;
  double float_v = 0.0;

  if (arg->type == ARGV_TYPE_BOOL) {
    int pb = argv_parse_bool_str(val_str);
    if (pb < 0) {
      snprintf(p->error, sizeof(p->error), "Invalid boolean value '%s' for flag '%s'", val_str, flag_name);
      return ARGV_ERR_INVALID_VALUE;
    }
    bool_v = (pb == 1);
  } else if (arg->type == ARGV_TYPE_REST) {
    if ((const char ***)arg->target == NULL) {
      snprintf(p->error, sizeof(p->error), "Rest target pointer is NULL");
      return ARGV_ERR_INVALID_VALUE;
    }
  } else if (arg->type == ARGV_TYPE_CHAR) {
    if (val_str[0] == '\0' || val_str[1] != '\0') {
      snprintf(p->error, sizeof(p->error), "Invalid character value '%s' for flag '%s' (expected 1 char)", val_str, flag_name);
      return ARGV_ERR_INVALID_VALUE;
    }
    char_v = val_str[0];
  } else if (arg->type == ARGV_TYPE_INT) {
    char *endptr = NULL;
    errno = 0;
    long v = strtol(val_str, &endptr, 10);
    if (errno != 0 || *endptr != '\0' || endptr == val_str || v < INT_MIN || v > INT_MAX) {
      snprintf(p->error, sizeof(p->error), "Invalid or out-of-range integer value '%s' for flag '%s'", val_str, flag_name);
      return ARGV_ERR_INVALID_VALUE;
    }
    int_v = (int)v;
  } else if (arg->type == ARGV_TYPE_FLOAT) {
    char *endptr = NULL;
    errno = 0;
    double v = strtod(val_str, &endptr);
    if (errno != 0 || *endptr != '\0' || endptr == val_str) {
      snprintf(p->error, sizeof(p->error), "Invalid floating-point value '%s' for flag '%s'", val_str, flag_name);
      return ARGV_ERR_INVALID_VALUE;
    }
    float_v = v;
  }

  if (!argv_check_choice(arg, val_str, int_v, float_v)) {
    snprintf(p->error, sizeof(p->error), "Invalid choice '%s' for %s '%s' (allowed: [%s])",
        val_str,
        arg->is_positional ? "positional argument" : "flag",
        flag_name,
        arg->choices);
    return ARGV_ERR_INVALID_VALUE;
  }

  if (arg->type == ARGV_TYPE_BOOL) {
    *(bool *)(arg->target) = bool_v;
  } else if (arg->type == ARGV_TYPE_REST) {
    const char ***arr = (const char ***)arg->target;
    size_t cur_cnt = arg->count_target ? *(arg->count_target) : 0;
    const char **new_arr = (const char **)realloc(*arr, (cur_cnt + 1) * sizeof(const char *));
    if (!new_arr) {
      snprintf(p->error, sizeof(p->error), "Memory allocation failed");
      return ARGV_ERR_ALLOC;
    }
    new_arr[cur_cnt] = val_str;
    *arr = new_arr;
    if (arg->count_target) *(arg->count_target) = cur_cnt + 1;
  } else if (arg->type == ARGV_TYPE_CHAR) {
    *(char *)(arg->target) = char_v;
  } else if (arg->type == ARGV_TYPE_STR) {
    *(const char **)(arg->target) = val_str;
  } else if (arg->type == ARGV_TYPE_INT) {
    *(int *)(arg->target) = int_v;
  } else if (arg->type == ARGV_TYPE_FLOAT) {
    *(double *)(arg->target) = float_v;
  }

  arg->is_set = true;
  return ARGV_OK;
}

extern void argv_set_default(CliParser *p, const char *flag_or_name, const char *default_val) {
  if (!p || !flag_or_name || !default_val) return;
  CliArg *arg = argv_find_arg(p, flag_or_name);
  if (!arg) {
    snprintf(p->error, sizeof(p->error), "Argument '%s' not found for setting default", flag_or_name);
    p->init_failed = true;
    return;
  }
  arg->default_val = default_val;
  // Assegna il valore di default al target validandolo subito
  argv_assign_val(p, arg, default_val, flag_or_name);
  // Resetta is_set in modo che il default non conti come flag fornita dall'utente
  arg->is_set = false;
}

static bool argv_is_negative_number(const char *str) {
  if (!str || str[0] != '-' || str[1] == '\0') return false;
  if (str[1] >= '0' && str[1] <= '9') return true;
  return (str[1] == '.' && str[2] >= '0' && str[2] <= '9');
}

extern CliStatus argv_parse(CliParser *p, int argc, const char *const *argv) {
  if (p == NULL || argv == NULL || argc <= 0) {
    if (p) snprintf(p->error, sizeof(p->error), "Invalid arguments to argv_parse");
    return ARGV_ERR_INVALID_VALUE;
  }

  if (p->init_failed) {
    return ARGV_ERR_INVALID_VALUE;
  }

  p->error[0] = '\0';
  for (size_t i = 0; i < p->count; i++) {
    p->args[i].is_set = false;
    if (p->args[i].default_val != NULL) {
      argv_assign_val(p, &p->args[i], p->args[i].default_val, p->args[i].name ? p->args[i].name : "default");
      p->args[i].is_set = false;
    }
    if (p->args[i].type == ARGV_TYPE_REST) {
      const char ***arr = (const char ***)p->args[i].target;
      if (arr != NULL && *arr != NULL) {
        free(*arr);
        *arr = NULL;
      }
      if (p->args[i].count_target) *(p->args[i].count_target) = 0;
    }
  }

  FILE *out = p->output ? p->output : stdout;

  int scan_end = argc;
  for (int i = 1; i < argc; i++) {
    if (strcmp(argv[i], "--") == 0) { scan_end = i; break; }
  }

  for (int i = 1; i < scan_end; i++) {
    if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
      const char *inspect = NULL;
      // Ispeziona solo in avanti per evitare di scambiare il valore di una flag precedente per un comando
      if (i + 1 < scan_end && argv[i + 1][0] != '-') {
        inspect = argv[i + 1];
      }

      if (inspect) {
        if (p->version && p->version[0] != '\0' &&
            (strcmp(inspect, "version") == 0 || strcmp(inspect, "--version") == 0 ||
             (p->version_short[0] != '\0' && (strcmp(inspect, p->version_short) == 0 || strcmp(inspect, p->version_short + 1) == 0)))) {
          fprintf(out, "%-23s%s\n", "Short flag:", p->version_short[0] ? p->version_short : "None");
          fprintf(out, "%-23s%s\n", "Long flag:", "--version");
          fprintf(out, "%-23s%s\n", "Value required:", "No");
          fprintf(out, "%-23s%s\n", "Mandatory:", "No");
          fprintf(out, "\nDescription:\n");
          argv_print_wrapped_desc(out, "", 2, 0, "Display version information", 50);
          return ARGV_HELP_REQUESTED;
        }
        CliArg *found = argv_find_arg(p, inspect);
        if (found) {
          argv_print_single_help(p, found);
        } else {
          // Ora questo ramo è raggiungibile se viene passata una flag inesistente
          fprintf(out, "Argument '%s' not recognized.\n\n", inspect);
          argv_print_help(p);
        }
      } else {
        argv_print_help(p);
      }
      return ARGV_HELP_REQUESTED;
    }
  }

  if (p->version && p->version[0] != '\0') {
    for (int i = 1; i < scan_end; i++) {
      bool is_ver = (strcmp(argv[i], "--version") == 0);
      if (!is_ver && p->version_short[0] != '\0' && strcmp(argv[i], p->version_short) == 0) {
        is_ver = true;
      }
      if (is_ver) {
        fprintf(out, "%s %s\n", argv_basename(p->program_name), p->version);
        if (p->version_extra && p->version_extra[0] != '\0') {
          fprintf(out, "%s\n", p->version_extra);
        }
        return ARGV_VERSION_REQUESTED;
      }
    }
  }

  bool stop_flags = false;
  int current_pos_index = 0;

  for (int i = 1; i < argc; i++) {
    const char *token = argv[i];

    if (!stop_flags && strcmp(token, "--") == 0) {
      stop_flags = true;
      continue;
    }

    if (stop_flags || token[0] != '-' || strcmp(token, "-") == 0) {
      CliArg *target_pos = NULL;
      int found_pos = 0;
      for (size_t j = 0; j < p->count; j++) {
        if (p->args[j].is_positional) {
          if (found_pos == current_pos_index) {
            target_pos = &p->args[j];
            break;
          }
          found_pos++;
        }
      }

      if (target_pos != NULL) {
        CliStatus st = argv_assign_val(p, target_pos, token, target_pos->name ? target_pos->name : "argument");
        if (st != ARGV_OK) return st;
        if (target_pos->type != ARGV_TYPE_REST) {
          current_pos_index++;
        }
      } else {
        snprintf(p->error, sizeof(p->error), "Unexpected positional argument '%s'", token);
        return ARGV_ERR_UNKNOWN_FLAG;
      }
      continue;
    }

    if (token[1] == '-') {
      char flag_name[64] = {0};
      const char *inline_val = strchr(token, '=');

      if (inline_val) {
        size_t name_len = (size_t)(inline_val - token);
        if (name_len >= sizeof(flag_name)) name_len = sizeof(flag_name) - 1;
        strncpy(flag_name, token, name_len);
        flag_name[name_len] = '\0';
        inline_val++;
      } else {
        strncpy(flag_name, token, sizeof(flag_name) - 1);
        flag_name[sizeof(flag_name) - 1] = '\0';
      }

      CliArg *matched = argv_find_flag(p, flag_name, false);
      bool is_negated = false;

      if (!matched && strncmp(flag_name, "--no-", 5) == 0) {
        char pos_flag[64];
        snprintf(pos_flag, sizeof(pos_flag), "--%s", flag_name + 5);
        CliArg *alt = argv_find_flag(p, pos_flag, false);
        if (alt && alt->type == ARGV_TYPE_BOOL) {
          matched = alt;
          is_negated = true;
        }
      }

      if (!matched) {
        snprintf(p->error, sizeof(p->error), "Unknown flag '%s'", flag_name);
        return ARGV_ERR_UNKNOWN_FLAG;
      }

      if (matched->type == ARGV_TYPE_BOOL) {
        bool val = true;
        if (is_negated) {
          val = false;
        } else if (inline_val) {
          int pb = argv_parse_bool_str(inline_val);
          if (pb < 0) {
            snprintf(p->error, sizeof(p->error), "Invalid boolean value '%s' for flag '%s'", inline_val, flag_name);
            return ARGV_ERR_INVALID_VALUE;
          }
          val = (pb == 1);
        }
        *(bool *)(matched->target) = val;
        matched->is_set = true;
        continue;
      }

      if (matched->type == ARGV_TYPE_COUNT) {
        (*(int *)(matched->target))++;
        matched->is_set = true;
        continue;
      }

      const char *val_str = inline_val;
      if (!val_str) {
        if (i + 1 >= argc || (argv[i + 1][0] == '-' && strcmp(argv[i + 1], "-") != 0 && !argv_is_negative_number(argv[i + 1]))) {
          snprintf(p->error, sizeof(p->error), "Flag '%s' requires a value", flag_name);
          return ARGV_ERR_MISSING_VALUE;
        }
        val_str = argv[++i];
      }

      CliStatus st = argv_assign_val(p, matched, val_str, flag_name);
      if (st != ARGV_OK) return st;
      continue;
    }

    size_t token_len = strlen(token);
    for (size_t c = 1; c < token_len; c++) {
      char single_sh[3] = {'-', token[c], '\0'};
      CliArg *matched = argv_find_flag(p, single_sh, true);

      if (!matched) {
        snprintf(p->error, sizeof(p->error), "Unknown short flag '%s' in '%s'", single_sh, token);
        return ARGV_ERR_UNKNOWN_FLAG;
      }

      if (matched->type == ARGV_TYPE_BOOL) {
        *(bool *)(matched->target) = true;
        matched->is_set = true;
      } else if (matched->type == ARGV_TYPE_COUNT) {
        (*(int *)(matched->target))++;
        matched->is_set = true;
      } else {
        const char *val_str = NULL;
        if (c + 1 < token_len) {
          val_str = &token[c + 1];
          c = token_len;
        } else {
          if (i + 1 >= argc || (argv[i + 1][0] == '-' && strcmp(argv[i + 1], "-") != 0 && !argv_is_negative_number(argv[i + 1]))) {
            snprintf(p->error, sizeof(p->error), "Flag '%s' requires a value", single_sh);
            return ARGV_ERR_MISSING_VALUE;
          }
          val_str = argv[++i];
        }
        CliStatus st = argv_assign_val(p, matched, val_str, single_sh);
        if (st != ARGV_OK) return st;
        break;
      }
    }
  }

  for (size_t i = 0; i < p->count; i++) {
    CliArg *arg = &p->args[i];
    char env_key[64] = {0};
    argv_get_effective_env(p, arg, env_key, sizeof(env_key));

    if (!arg->is_set && env_key[0] != '\0') {
      const char *env_val = getenv(env_key);
      if (env_val != NULL && env_val[0] != '\0') {
        if (arg->type == ARGV_TYPE_BOOL) {
          int pb = argv_parse_bool_str(env_val);
          if (pb < 0) {
            snprintf(p->error, sizeof(p->error), "Invalid boolean value '%s' in environment variable '%s'", env_val, env_key);
            return ARGV_ERR_INVALID_VALUE;
          }
          *(bool *)(arg->target) = (pb == 1);
          arg->is_set = true;
        } else if (arg->type == ARGV_TYPE_COUNT) {
          char *endptr = NULL;
          long c = strtol(env_val, &endptr, 10);
          if (*endptr == '\0' && endptr != env_val) {
            *(int *)(arg->target) = (int)c;
            arg->is_set = true;
          }
        } else {
          CliStatus st = argv_assign_val(p, arg, env_val, env_key);
          if (st != ARGV_OK) return st;
        }
      }
    }
  }

  for (size_t r = 0; r < p->rule_count; r++) {
    CliRule *rule = &p->rules[r];

    if (rule->type == ARGV_RULE_EXCLUSIVE) {
      int set_count = 0;
      char first_conflict[64] = {0};
      char second_conflict[64] = {0};

      const char *ptr = rule->target;
      while (*ptr) {
        while (*ptr == ' ' || *ptr == ',' || *ptr == '|') ptr++;
        if (!*ptr) break;

        const char *start = ptr;
        while (*ptr && *ptr != ',' && *ptr != '|' && *ptr != ' ') ptr++;
        size_t len = (size_t)(ptr - start);

        char tok[64];
        if (len >= sizeof(tok)) {
          snprintf(p->error, sizeof(p->error), "Flag name too long in exclusive rule group [%s]", rule->target);
          return ARGV_ERR_UNKNOWN_FLAG;
        }
        {
          memcpy(tok, start, len);
          tok[len] = '\0';
          CliArg *a = argv_find_arg(p, tok);

          if (!a) {
            snprintf(p->error, sizeof(p->error), "Unknown flag '%s' in exclusive rule group [%s]", tok, rule->target);
            return ARGV_ERR_UNKNOWN_FLAG;
          }

          if (a->is_set) {
            set_count++;
            const char *disp = a->long_flag[0] ? a->long_flag : (a->short_flag[0] ? a->short_flag : a->name);
            if (set_count == 1) snprintf(first_conflict, sizeof(first_conflict), "%s", disp);
            else if (set_count == 2) snprintf(second_conflict, sizeof(second_conflict), "%s", disp);
          }
        }
      }

      if (set_count > 1) {
        snprintf(p->error, sizeof(p->error),
            "Mutually exclusive flags in group [%s] cannot be used together (conflicting: '%s' and '%s')",
            rule->target, first_conflict, second_conflict);
        return ARGV_ERR_MUTUAL_EXCLUSION;
      }
    } else if (rule->type == ARGV_RULE_DEPENDENCY) {
      CliArg *target = argv_find_arg(p, rule->target);

      if (!target) {
        snprintf(p->error, sizeof(p->error), "Target flag '%s' in dependency rule does not exist", rule->target);
        return ARGV_ERR_UNKNOWN_FLAG;
      }

      const char *ptr = rule->requires;
      while (*ptr) {
        while (*ptr == ' ' || *ptr == ',' || *ptr == '|') ptr++;
        if (!*ptr) break;

        const char *start = ptr;
        while (*ptr && *ptr != ',' && *ptr != '|' && *ptr != ' ') ptr++;
        size_t len = (size_t)(ptr - start);

        char tok[64];
        if (len >= sizeof(tok)) {
          snprintf(p->error, sizeof(p->error), "Flag name too long in dependency rule [%s]", rule->requires);
          return ARGV_ERR_UNKNOWN_FLAG;
        }
        {
          memcpy(tok, start, len);
          tok[len] = '\0';
          CliArg *dep = argv_find_arg(p, tok);

          if (!dep) {
            snprintf(p->error, sizeof(p->error), "Required flag '%s' in dependency rule does not exist", tok);
            return ARGV_ERR_UNKNOWN_FLAG;
          }

          if (target->is_set && !dep->is_set) {
            const char *t_name = target->long_flag[0] ? target->long_flag : (target->short_flag[0] ? target->short_flag : target->name);
            snprintf(p->error, sizeof(p->error), "Flag '%s' requires '%s' to be set", t_name, tok);
            return ARGV_ERR_DEPENDENCY;
          }
        }
      }
    }
  }

  for (size_t i = 0; i < p->count; i++) {
    if (p->args[i].required && !p->args[i].is_set) {
      if (p->args[i].is_positional) {
        snprintf(p->error, sizeof(p->error), "Mandatory positional argument missing: <%s>", p->args[i].name);
      } else {
        const char *flag_name = p->args[i].long_flag[0] ? p->args[i].long_flag : p->args[i].short_flag;
        snprintf(p->error, sizeof(p->error), "Mandatory flag missing: %s", flag_name);
      }
      return ARGV_ERR_MISSING_REQUIRED;
    }
  }

  return ARGV_OK;
}

extern void argv_set_version(CliParser *p, const char *version, const char *sh, const char *extra_info) {
  if (!p) return;
  p->version = version;
  p->version_extra = extra_info;
  if (sh && sh[0] != '\0') {
    argv_normalize_short(sh, p->version_short, sizeof(p->version_short));
  } else {
    p->version_short[0] = '\0';
  }
}
