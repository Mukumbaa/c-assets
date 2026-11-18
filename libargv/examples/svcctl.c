#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "libargv.h"

typedef struct {
  bool quiet;
  int verbosity;
  const char *context;
} GlobalOpts;

static int cmd_deploy(const GlobalOpts *g, int argc, const char *const *argv);
static int cmd_logs(const GlobalOpts *g, int argc, const char *const *argv);
static int cmd_config(const GlobalOpts *g, int argc, const char *const *argv);

int main(int argc, char **argv) {
  GlobalOpts global = {false, 0, NULL};
  const char *command = NULL;

  CliParser root;
  argv_init(&root, argv[0]);
  argv_set_output(&root, stdout);
  argv_set_version(&root, "1.4.0", "V", "Build 2026.11 (C99)");
  argv_set_description(&root, "Service deployment and operations CLI.");
  argv_set_epilog(&root, "Run 'svcctl <command> --help' for command-specific options.");
  argv_set_help_width(&root, 60);
  argv_set_env_prefix(&root, "SVCCTL");

  // Stop parsing global flags once the subcommand positional is reached
  argv_set_stop_at_positional(&root, true);

  // Global options
  argv_bind_bool(&root, "q", "quiet", &global.quiet, "Suppress informational output");
  argv_bind_count(&root, "v", "verbose", &global.verbosity, "Increase log verbosity (-v, -vv)");
  argv_bind_str(&root, "C", "context", &global.context, false, NULL, "Target cluster context");
  argv_set_placeholder(&root, "context", "name");
  argv_set_default(&root, "context", "default");

  // Subcommand selector
  argv_bind_pos(&root, "command", &command, true, "deploy,logs,config", "Subcommand to execute");
  argv_set_choice_help(&root, "command", "deploy", "Deploy a service and expose ports");
  argv_set_choice_help(&root, "command", "logs", "Inspect or stream service logs");
  argv_set_choice_help(&root, "command", "config", "Read or update CLI configuration");

  CliStatus st = argv_parse(&root, argc, (const char *const *)argv);
  if (st == ARGV_HELP_REQUESTED || st == ARGV_VERSION_REQUESTED) {
    argv_free(&root);
    return 0;
  }
  if (st != ARGV_OK) {
    fprintf(stderr, "svcctl: %s\n", argv_get_error(&root));
    if (st == ARGV_ERR_MISSING_REQUIRED) {
      fprintf(stderr, "\n");
      argv_set_output(&root, stderr);
      argv_print_help(&root);
    }
    argv_free(&root);
    return 1;
  }

  // Prepare argv slice for the selected subcommand (argv[0] = subcommand name)
  int rem_argc = 0;
  const char *const *rem_argv = argv_remaining_args(&root, &rem_argc);
  const char *const empty_argv[] = {command};
  int sub_argc = (rem_argc > 0) ? (rem_argc + 1) : 1;
  const char *const *sub_argv = (rem_argc > 0) ? (rem_argv - 1) : empty_argv;

  int exit_code = 0;
  if (strcmp(command, "deploy") == 0) {
    exit_code = cmd_deploy(&global, sub_argc, sub_argv);
  } else if (strcmp(command, "logs") == 0) {
    exit_code = cmd_logs(&global, sub_argc, sub_argv);
  } else if (strcmp(command, "config") == 0) {
    exit_code = cmd_config(&global, sub_argc, sub_argv);
  }

  // Free root only after subcommands finish using command / rem_argv
  argv_free(&root);
  return exit_code;
}

static int cmd_deploy(const GlobalOpts *g, int argc, const char *const *argv) {
  bool dry_run = false;
  bool health_check = true; // Default true -> displays --[no-]health-check in help
  char mode = 'r';
  int port = 0;
  int offset = 0;
  double traffic = 0.0;
  const char *strategy = NULL;
  const char *output_path = NULL;

  bool encrypt = false;
  bool plain = false;
  const char *key_file = NULL;
  bool debug_hooks = false;

  const char **env_vars = NULL;
  size_t env_count = 0;
  int *exposed_ports = NULL;
  size_t exposed_count = 0;
  double *weights = NULL;
  size_t weight_count = 0;

  const char *service = NULL;
  const char **artifacts = NULL;
  size_t artifact_count = 0;

  CliParser p;
  argv_init(&p, "svcctl deploy");
  argv_set_output(&p, stdout);
  argv_set_version(&p, "1.4.0", "V", "Subcommand: deploy");
  argv_set_description(&p, "Deploy a service bundle to the selected cluster context.");
  argv_set_epilog(&p,
      "Examples:\n"
      "  svcctl -vv deploy --plain --no-health-check -p 9090 -E ENV=prod -P 80 -P 443 api ./dist.tar.gz");
  argv_set_help_width(&p, 0); // 0 = auto-detect from COLUMNS or fallback to 50
  argv_set_env_prefix(&p, "SVCCTL_DEPLOY");

  // Scalar options
  argv_bind_bool(&p, "d", "dry-run", &dry_run, "Simulate deployment without applying changes");
  argv_bind_bool(&p, "H", "health-check", &health_check, "Wait for readiness probes after rollout");
  argv_bind_char(&p, "m", "mode", &mode, false, "r,b,c", "Rollout mode");
  argv_bind_int(&p, "p", "port", &port, false, NULL, "Primary container listen port");
  argv_bind_int(&p, "O", "offset", &offset, false, NULL, "Canary skew offset (allows negative values)");
  argv_bind_float(&p, "t", "traffic", &traffic, false, NULL, "Initial traffic ratio (0.0 - 1.0)");
  argv_bind_str(&p, "s", "strategy", &strategy, false, "fast,safe,canary", "Deployment strategy");
  argv_bind_str(&p, "o", "output", &output_path, false, NULL, "Manifest output path ('-' for stdout)");

  // Grouped security options & hidden diagnostic flag
  argv_bind_bool(&p, "e", "encrypt", &encrypt, "Encrypt bundle in transit");
  argv_bind_bool(&p, NULL, "plain", &plain, "Disable transport encryption");
  argv_bind_str(&p, "k", "key-file", &key_file, false, NULL, "TLS private key path");
  argv_bind_bool(&p, "D", "debug-hooks", &debug_hooks, "Enable internal lifecycle hook tracing");

  // Repeatable list options
  argv_bind_str_list(&p, "E", "env", &env_vars, &env_count, false, NULL, "Environment variables (KEY=VAL)");
  argv_bind_int_list(&p, "P", "expose", &exposed_ports, &exposed_count, false, NULL, "Additional ports to expose");
  argv_bind_float_list(&p, "w", "weight", &weights, &weight_count, false, NULL, "Traffic weights per replica pool");

  // Help customization & environment overrides
  argv_set_placeholder(&p, "output", "path");
  argv_set_placeholder(&p, "key-file", "pem");
  argv_set_placeholder(&p, "env", "K=V");
  argv_set_help_group(&p, "encrypt", "Security Options");
  argv_set_help_group(&p, "plain", "Security Options");
  argv_set_help_group(&p, "key-file", "Security Options");
  argv_set_env(&p, "key-file", "DEPLOY_TLS_KEY");
  argv_set_hidden(&p, "debug-hooks");

  // Explicit defaults
  argv_set_default(&p, "dry-run", "false");
  argv_set_default(&p, "health-check", "true");
  argv_set_default(&p, "mode", "r");
  argv_set_default(&p, "port", "8080");
  argv_set_default(&p, "offset", "0");
  argv_set_default(&p, "traffic", "1.0");
  argv_set_default(&p, "strategy", "safe");
  argv_set_default(&p, "output", "-");

  // Choice documentation
  argv_set_choice_help(&p, "mode", "r", "Rolling update");
  argv_set_choice_help(&p, "mode", "b", "Blue-green swap");
  argv_set_choice_help(&p, "mode", "c", "Canary progressive");
  argv_set_choice_help(&p, "strategy", "fast", "Minimize rollout time");
  argv_set_choice_help(&p, "strategy", "safe", "Verify health checks between batches");
  argv_set_choice_help(&p, "strategy", "canary", "Shift traffic gradually");

  // Cross-flag validation rules:
  // Combining exclusive + any-of enforces "exactly one" transport security flag
  argv_add_exclusive(&p, "encrypt, plain");
  argv_add_any_of(&p, "encrypt, plain");
  argv_add_dependency(&p, "key-file", "encrypt");

  // Positionals: bind all scalar positionals before any rest argument
  argv_bind_pos(&p, "service", &service, true, NULL, "Target service name");
  argv_bind_rest(&p, "artifacts", &artifacts, &artifact_count, false, NULL, "Optional bundle files");
  argv_set_placeholder(&p, "artifacts", "file");

  CliStatus st = argv_parse(&p, argc, argv);
  switch (st) {
    case ARGV_OK:
      break;
    case ARGV_HELP_REQUESTED:
    case ARGV_VERSION_REQUESTED:
      argv_free(&p);
      return 0;
    case ARGV_ERR_UNKNOWN_FLAG:
      fprintf(stderr, "deploy: unrecognized option: %s\n", argv_get_error(&p));
      argv_free(&p);
      return 2;
    case ARGV_ERR_MISSING_VALUE:
    case ARGV_ERR_INVALID_VALUE:
    case ARGV_ERR_MISSING_REQUIRED:
    case ARGV_ERR_MUTUAL_EXCLUSION:
    case ARGV_ERR_DEPENDENCY:
    case ARGV_ERR_ALLOC:
    default:
      fprintf(stderr, "deploy: %s\n", argv_get_error(&p));
      argv_free(&p);
      return 1;
  }

  // Best practice: use argv_is_set to distinguish explicit CLI/env values from defaults
  bool port_explicit = argv_is_set(&p, "port");

  // Use all parsed values (including env-backed strings and lists) before argv_free
  if (!g->quiet) {
    printf("[context=%s verbosity=%d] Deploying service '%s'\n", g->context, g->verbosity, service);
    printf("  dry_run=%s health_check=%s mode=%c port=%d (%s) offset=%d traffic=%.2f strategy=%s output=%s\n",
        dry_run ? "true" : "false", health_check ? "true" : "false",
        mode, port, port_explicit ? "explicit" : "default", offset, traffic, strategy, output_path);
    printf("  encrypt=%s plain=%s key_file=%s debug_hooks=%s\n",
        encrypt ? "true" : "false", plain ? "true" : "false",
        key_file ? key_file : "(none)", debug_hooks ? "true" : "false");

    printf("  env (%zu):", env_count);
    for (size_t i = 0; i < env_count; i++) printf(" %s", env_vars[i]);
    printf("\n");

    printf("  exposed_ports (%zu):", exposed_count);
    for (size_t i = 0; i < exposed_count; i++) printf(" %d", exposed_ports[i]);
    printf("\n");

    printf("  weights (%zu):", weight_count);
    for (size_t i = 0; i < weight_count; i++) printf(" %.2f", weights[i]);
    printf("\n");

    printf("  artifacts (%zu):", artifact_count);
    for (size_t i = 0; i < artifact_count; i++) printf(" %s", artifacts[i]);
    printf("\n");
  }

  argv_free(&p);
  return 0;
}

static int cmd_logs(const GlobalOpts *g, int argc, const char *const *argv) {
  bool follow = false;
  int lines = 0;
  const char *level = NULL;
  const char *since = NULL;
  const char **filters = NULL;
  size_t filter_count = 0;
  const char *service = NULL;

  CliParser p;
  argv_init(&p, "svcctl logs");
  argv_set_description(&p, "Fetch or stream logs from a running service.");
  argv_set_env_prefix(&p, "SVCCTL_LOGS");

  argv_bind_bool(&p, "f", "follow", &follow, "Follow log output");
  argv_bind_int(&p, "n", "lines", &lines, false, NULL, "Number of recent lines to show");
  argv_bind_str(&p, "l", "level", &level, false, "debug,info,warn,error", "Minimum log severity");
  argv_bind_str(&p, "s", "since", &since, false, NULL, "Show logs newer than duration");
  argv_bind_str_list(&p, "F", "filter", &filters, &filter_count, false, NULL, "Include only matching tags");
  argv_bind_pos(&p, "service", &service, true, NULL, "Target service name");

  argv_set_placeholder(&p, "since", "duration");
  argv_set_placeholder(&p, "filter", "tag");
  argv_set_default(&p, "lines", "100");
  argv_set_default(&p, "level", "info");
  argv_set_default(&p, "since", "15m");

  CliStatus st = argv_parse(&p, argc, argv);
  if (st == ARGV_HELP_REQUESTED || st == ARGV_VERSION_REQUESTED) {
    argv_free(&p);
    return 0;
  }
  if (st != ARGV_OK) {
    fprintf(stderr, "logs: %s\n", argv_get_error(&p));
    argv_free(&p);
    return 1;
  }

  if (!g->quiet) {
    printf("[context=%s] Streaming logs for '%s' (follow=%s lines=%d (%s) level=%s since=%s filters=%zu)\n",
        g->context, service, follow ? "true" : "false",
        lines, argv_is_set(&p, "lines") ? "explicit" : "default",
        level, since, filter_count);
    for (size_t i = 0; i < filter_count; i++) {
      printf("  filter[%zu]=%s\n", i, filters[i]);
    }
  }

  argv_free(&p);
  return 0;
}

static int cmd_config(const GlobalOpts *g, int argc, const char *const *argv) {
  bool global_scope = false;
  const char *format = NULL;
  const char *action = NULL;
  const char **rest_args = NULL;
  size_t rest_count = 0;

  CliParser p;
  argv_init(&p, "svcctl config");
  argv_set_description(&p, "Inspect or modify CLI configuration profiles.");

  argv_bind_bool(&p, "g", "global", &global_scope, "Operate on global configuration");
  argv_bind_str(&p, "f", "format", &format, false, "text,json", "Output serialization format");
  argv_bind_pos(&p, "action", &action, true, "get,set,list", "Configuration action");
  argv_bind_rest(&p, "args", &rest_args, &rest_count, false, NULL, "Action arguments (<key> or <key> <value>)");

  argv_set_default(&p, "format", "text");
  argv_set_choice_help(&p, "action", "get", "Read a configuration key");
  argv_set_choice_help(&p, "action", "set", "Write a configuration key/value pair");
  argv_set_choice_help(&p, "action", "list", "List all configuration entries");

  CliStatus st = argv_parse(&p, argc, argv);
  if (st == ARGV_HELP_REQUESTED || st == ARGV_VERSION_REQUESTED) {
    argv_free(&p);
    return 0;
  }
  if (st != ARGV_OK) {
    fprintf(stderr, "config: %s\n", argv_get_error(&p));
    argv_free(&p);
    return 1;
  }

  // Post-parse domain validation
  if (strcmp(action, "get") == 0 && rest_count != 1) {
    fprintf(stderr, "config: 'get' requires exactly 1 argument (<key>)\n");
    argv_free(&p);
    return 1;
  }
  if (strcmp(action, "set") == 0 && rest_count != 2) {
    fprintf(stderr, "config: 'set' requires exactly 2 arguments (<key> <value>)\n");
    argv_free(&p);
    return 1;
  }

  if (!g->quiet) {
    printf("[context=%s] config action=%s global=%s format=%s",
        g->context, action, global_scope ? "true" : "false", format);
    if (rest_count == 1) printf(" key=%s", rest_args[0]);
    if (rest_count == 2) printf(" key=%s value=%s", rest_args[0], rest_args[1]);
    printf("\n");
  }

  argv_free(&p);
  return 0;
}
