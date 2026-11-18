/*
gcc -Wall -Wextra -Werror -g libargv_test.c -o libargv_test && valgrind --leak-check=full --error-exitcode=1 ./libargv_test
*/
#define _POSIX_C_SOURCE 200112L

#define LIBARGV_IMPLEMENTATION
#include "../standalone_lib/libargv.h"

#define LIBTEST_IMPLEMENTATION
#include "../../libtest/standalone_lib/libtest.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void read_tmp_output(FILE *tmp, char *buf, size_t sz) {
  rewind(tmp);
  size_t n = fread(buf, 1, sz - 1, tmp);
  buf[n] = '\0';
  fclose(tmp);
}


TEST(Syntax, BoolNegationAndFormats) {
  bool b1 = false, b2 = true, b3 = false, b4 = true;
  CliParser p;
  argv_init(&p, "app");
  argv_bind_bool(&p, "a", "alpha", &b1, "Alpha");
  argv_bind_bool(&p, "b", "beta", &b2, "Beta");
  argv_bind_bool(&p, "c", "gamma", &b3, "Gamma");
  argv_bind_bool(&p, "d", "delta", &b4, "Delta");

  const char *args[] = {"app", "-a", "--no-beta", "--gamma=yes", "--delta=0"};
  ASSERT_EQ(argv_parse(&p, 5, args), ARGV_OK);
  ASSERT_TRUE(b1);
  ASSERT_FALSE(b2);
  ASSERT_TRUE(b3);
  ASSERT_FALSE(b4);

  const char *bad[] = {"app", "--alpha=maybe"};
  ASSERT_EQ(argv_parse(&p, 2, bad), ARGV_ERR_INVALID_VALUE);

  argv_free(&p);
}

TEST(Syntax, ShortClusteringAndAttachedValues) {
  bool flag_a = false, flag_b = false;
  int count = 0;
  int num = 0;
  const char *str = NULL;

  CliParser p;
  argv_init(&p, "app");
  argv_bind_bool(&p, "a", "alpha", &flag_a, "A");
  argv_bind_bool(&p, "b", "beta", &flag_b, "B");
  argv_bind_count(&p, "v", "verbose", &count, "V");
  argv_bind_int(&p, "n", "num", &num, false, NULL, "N");
  argv_bind_str(&p, "s", "str", &str, false, NULL, "S");

  const char *args[] = {"app", "-abvv", "-n42", "-s=hello"};
  ASSERT_EQ(argv_parse(&p, 4, args), ARGV_OK);
  ASSERT_TRUE(flag_a);
  ASSERT_TRUE(flag_b);
  ASSERT_EQ(count, 2);
  ASSERT_EQ(num, 42);
  ASSERT_STR_EQ(str, "hello");

  const char *bad_cluster[] = {"app", "-ax"};
  ASSERT_EQ(argv_parse(&p, 2, bad_cluster), ARGV_ERR_UNKNOWN_FLAG);

  argv_free(&p);
}

TEST(Syntax, NumbersNegativeDotAndStdin) {
  int i = 0;
  double f1 = 0.0, f2 = 0.0;
  const char *out = NULL;

  CliParser p;
  argv_init(&p, "app");
  argv_bind_int(&p, "i", "int", &i, false, NULL, "Int");
  argv_bind_float(&p, "f", "flt", &f1, false, NULL, "Float1");
  argv_bind_float(&p, "g", "flt2", &f2, false, NULL, "Float2");
  argv_bind_str(&p, "o", "out", &out, false, NULL, "Out");

  const char *args[] = {"app", "-i", "-12", "-f", "-.5", "--flt2=-3.25", "-o", "-"};
  ASSERT_EQ(argv_parse(&p, 8, args), ARGV_OK);
  ASSERT_EQ(i, -12);
  ASSERT_FLOAT_EQ(f1, -0.5, 1e-6);
  ASSERT_FLOAT_EQ(f2, -3.25, 1e-6);
  ASSERT_STR_EQ(out, "-");

  const char *overflow[] = {"app", "-i", "99999999999999999999"};
  ASSERT_EQ(argv_parse(&p, 3, overflow), ARGV_ERR_INVALID_VALUE);

  argv_free(&p);
}

TEST(Syntax, PositionalRestAndDoubleDash) {
  const char *pos = NULL;
  const char **rest = NULL;
  size_t rest_cnt = 0;
  bool flag = false;

  CliParser p;
  argv_init(&p, "app");
  argv_bind_bool(&p, "f", "flag", &flag, "Flag");
  argv_bind_pos(&p, "cmd", &pos, true, NULL, "Command");
  argv_bind_rest(&p, "files", &rest, &rest_cnt, false, NULL, "Files");

  const char *args[] = {"app", "run", "--", "--flag", "-h", "--version"};
  ASSERT_EQ(argv_parse(&p, 6, args), ARGV_OK);
  ASSERT_FALSE(flag);
  ASSERT_STR_EQ(pos, "run");
  ASSERT_EQ(rest_cnt, 3);
  ASSERT_STR_EQ(rest[0], "--flag");
  ASSERT_STR_EQ(rest[1], "-h");
  ASSERT_STR_EQ(rest[2], "--version");

  argv_free(&p);
}

TEST(Syntax, MissingValuesAndUnknownFlags) {
  const char *s1 = NULL, *s2 = NULL;
  CliParser p;
  argv_init(&p, "app");
  argv_bind_str(&p, "a", "alpha", &s1, false, NULL, "A");
  argv_bind_str(&p, "b", "beta", &s2, false, NULL, "B");

  const char *eof_val[] = {"app", "--alpha"};
  ASSERT_EQ(argv_parse(&p, 2, eof_val), ARGV_ERR_MISSING_VALUE);

  const char *flag_as_val[] = {"app", "--alpha", "--beta", "ok"};
  ASSERT_EQ(argv_parse(&p, 4, flag_as_val), ARGV_ERR_MISSING_VALUE);

  const char *unknown[] = {"app", "--nonexistent"};
  ASSERT_EQ(argv_parse(&p, 2, unknown), ARGV_ERR_UNKNOWN_FLAG);

  const char *extra_pos[] = {"app", "unexpected"};
  ASSERT_EQ(argv_parse(&p, 2, extra_pos), ARGV_ERR_UNKNOWN_FLAG);

  argv_free(&p);
}


TEST(DefaultsAndEnv, PriorityCliOverEnvOverDefault) {
  int port = 0;
  const char *host = NULL;
  int count = 0;
  const char **inc = NULL;
  size_t inc_cnt = 0;
  const char **rest = NULL;
  size_t rest_cnt = 0;

  CliParser p;
  argv_init(&p, "app");
  argv_set_env_prefix(&p, "  TESTAPP  ");
  argv_bind_int(&p, "p", "port", &port, false, NULL, "Port");
  argv_bind_str(&p, "H", "host", &host, false, NULL, "Host");
  argv_bind_count(&p, "v", "verbose", &count, "Verbose");
  argv_bind_str_list(&p, "I", "include", &inc, &inc_cnt, false, NULL, "Include");
  argv_bind_rest(&p, "rest", &rest, &rest_cnt, false, NULL, "Rest");

  argv_set_default(&p, "port", "8080");
  argv_set_default(&p, "host", "127.0.0.1");
  argv_set_default(&p, "verbose", "2");
  argv_set_default(&p, "include", "/def/inc");
  argv_set_default(&p, "rest", "def.txt");

  const char *a1[] = {"app"};
  ASSERT_EQ(argv_parse(&p, 1, a1), ARGV_OK);
  ASSERT_EQ(port, 8080);
  ASSERT_STR_EQ(host, "127.0.0.1");
  ASSERT_EQ(count, 2);
  ASSERT_EQ(inc_cnt, 1);
  ASSERT_STR_EQ(inc[0], "/def/inc");
  ASSERT_EQ(rest_cnt, 1);
  ASSERT_STR_EQ(rest[0], "def.txt");

  setenv("TESTAPP_PORT", "9090", 1);
  setenv("TESTAPP_HOST", "env.local", 1);
  const char *a2[] = {"app", "--port", "3000", "-I", "/cli/inc", "cli.txt"};
  ASSERT_EQ(argv_parse(&p, 6, a2), ARGV_OK);

  setenv("TESTAPP_HOST", "corrupted.invalid", 1);
  unsetenv("TESTAPP_HOST");

  ASSERT_EQ(port, 3000);
  ASSERT_STR_EQ(host, "env.local");
  ASSERT_EQ(count, 2);
  ASSERT_EQ(inc_cnt, 1);
  ASSERT_STR_EQ(inc[0], "/cli/inc");
  ASSERT_EQ(rest_cnt, 1);
  ASSERT_STR_EQ(rest[0], "cli.txt");

  unsetenv("TESTAPP_PORT");
  argv_free(&p);
}

TEST(DefaultsAndEnv, InvalidEnvCountReturnsError) {
  int count = 0;
  CliParser p;
  argv_init(&p, "app");
  argv_bind_count(&p, "v", "verbose", &count, "Verbose");
  argv_set_env(&p, "verbose", "CUSTOM_VERBOSE");

  setenv("CUSTOM_VERBOSE", "not_an_int", 1);
  const char *args[] = {"app"};
  ASSERT_EQ(argv_parse(&p, 1, args), ARGV_ERR_INVALID_VALUE);

  unsetenv("CUSTOM_VERBOSE");
  argv_free(&p);
}


TEST(Validation, ChoicesForAllTypes) {
  char mode = 0;
  int level = 0;
  double ratio = 0.0;
  const char *fmt = NULL;

  CliParser p;
  argv_init(&p, "app");
  argv_bind_char(&p, "m", "mode", &mode, false, "abc", "Mode");
  argv_bind_int(&p, "l", "level", &level, false, "1, 2 | 3", "Level");
  argv_bind_float(&p, "r", "ratio", &ratio, false, "0.5, 1.5", "Ratio");
  argv_bind_str(&p, "f", "format", &fmt, false, "json|yaml", "Format");

  const char *ok[] = {"app", "-m", "b", "-l", "2", "-r", "1.5", "-f", "yaml"};
  ASSERT_EQ(argv_parse(&p, 9, ok), ARGV_OK);
  ASSERT_EQ(mode, 'b');
  ASSERT_EQ(level, 2);
  ASSERT_FLOAT_EQ(ratio, 1.5, 1e-6);
  ASSERT_STR_EQ(fmt, "yaml");

  const char *bad_char[] = {"app", "-m", "z"};
  ASSERT_EQ(argv_parse(&p, 3, bad_char), ARGV_ERR_INVALID_VALUE);

  const char *bad_int[] = {"app", "-l", "4294967297"}; 
  ASSERT_EQ(argv_parse(&p, 3, bad_int), ARGV_ERR_INVALID_VALUE);

  const char *bad_str[] = {"app", "-f", "xml"};
  ASSERT_EQ(argv_parse(&p, 3, bad_str), ARGV_ERR_INVALID_VALUE);

  argv_free(&p);
}

TEST(Validation, ExclusiveDependencyAndRequired) {
  bool json = false, xml = false, auth = false;
  const char *token = NULL, *user = NULL, *input = NULL;

  CliParser p;
  argv_init(&p, "app");
  argv_bind_bool(&p, "j", "json", &json, "JSON");
  argv_bind_bool(&p, "x", "xml", &xml, "XML");
  argv_bind_bool(&p, "a", "auth", &auth, "Auth");
  argv_bind_str(&p, "t", "token", &token, false, NULL, "Token");
  argv_bind_str(&p, "u", "user", &user, false, NULL, "User");
  argv_bind_pos(&p, "input", &input, true, NULL, "Input");

  argv_add_exclusive(&p, "json | xml");
  argv_add_dependency(&p, "auth", "token, user");

  const char *missing_pos[] = {"app"};
  ASSERT_EQ(argv_parse(&p, 1, missing_pos), ARGV_ERR_MISSING_REQUIRED);

  const char *conflict[] = {"app", "--json", "--xml", "in.txt"};
  ASSERT_EQ(argv_parse(&p, 4, conflict), ARGV_ERR_MUTUAL_EXCLUSION);

  const char *missing_dep[] = {"app", "--auth", "--token", "abc", "in.txt"};
  ASSERT_EQ(argv_parse(&p, 5, missing_dep), ARGV_ERR_DEPENDENCY);

  const char *ok[] = {"app", "--auth", "--token", "abc", "--user", "me", "--json", "in.txt"};
  ASSERT_EQ(argv_parse(&p, 8, ok), ARGV_OK);

  argv_free(&p);
}

TEST(Validation, BadRuleFailsBeforeHelp) {
  bool a = false;
  CliParser p;
  argv_init(&p, "app");
  argv_bind_bool(&p, "a", "alpha", &a, "A");
  argv_add_exclusive(&p, "alpha, ghost_flag");

  const char *args[] = {"app", "-h"};
  ASSERT_EQ(argv_parse(&p, 2, args), ARGV_ERR_UNKNOWN_FLAG);

  argv_free(&p);
}

TEST(Validation, InitErrorsPropagateToParse) {
  int x = 0;
  CliParser p;
  argv_init(&p, "app");
  argv_bind_int(&p, "x", "xval", &x, false, NULL, "X");
  argv_set_env(&p, "nonexistent", "ENV_X");

  const char *args[] = {"app"};
  ASSERT_EQ(argv_parse(&p, 1, args), ARGV_ERR_INVALID_VALUE);
  argv_free(&p);
}


TEST(Output, HelpVersionAndHidden) {
  char mode = 0;
  bool secret = false;
  const char *pos = NULL;

  CliParser p;
  argv_init(&p, "/usr/bin/mytool");
  argv_set_version(&p, "2.5.0", "V", "git-abc123");
  argv_bind_char(&p, "m", "mode", &mode, false, "ab", "Mode selector");
  argv_bind_bool(&p, "s", "secret", &secret, "Secret flag");
  argv_bind_pos(&p, "target", &pos, false, NULL, "Target");

  ASSERT_TRUE(argv_set_choice_help(&p, "mode", "a", "Alpha choice"));
  ASSERT_TRUE(argv_set_choice_help(&p, "mode", "b", "Beta choice"));
  argv_set_hidden(&p, "secret");

  const char *use_hidden[] = {"mytool", "--secret"};
  ASSERT_EQ(argv_parse(&p, 2, use_hidden), ARGV_OK);
  ASSERT_TRUE(secret);

  FILE *tmp = tmpfile();
  ASSERT_TRUE(tmp != NULL);
  argv_set_output(&p, tmp);
  const char *help_after_pos[] = {"mytool", "foo.txt", "-h"};
  ASSERT_EQ(argv_parse(&p, 3, help_after_pos), ARGV_HELP_REQUESTED);
  char buf[2048];
  read_tmp_output(tmp, buf, sizeof(buf));
  ASSERT_TRUE(strstr(buf, "Usage: mytool [OPTIONS]") != NULL);
  ASSERT_TRUE(strstr(buf, "not recognized") == NULL);
  ASSERT_TRUE(strstr(buf, "--secret") == NULL);

  tmp = tmpfile();
  ASSERT_TRUE(tmp != NULL);
  argv_set_output(&p, tmp);
  const char *help_mode[] = {"mytool", "-h", "mode"};
  ASSERT_EQ(argv_parse(&p, 3, help_mode), ARGV_HELP_REQUESTED);
  read_tmp_output(tmp, buf, sizeof(buf));
  ASSERT_TRUE(strstr(buf, "Alpha choice") != NULL);
  ASSERT_TRUE(strstr(buf, "Beta choice") != NULL);

  tmp = tmpfile();
  ASSERT_TRUE(tmp != NULL);
  argv_set_output(&p, tmp);
  const char *help_secret[] = {"mytool", "-h", "secret"};
  ASSERT_EQ(argv_parse(&p, 3, help_secret), ARGV_HELP_REQUESTED);
  read_tmp_output(tmp, buf, sizeof(buf));
  ASSERT_TRUE(strstr(buf, "not recognized") != NULL);

  tmp = tmpfile();
  ASSERT_TRUE(tmp != NULL);
  argv_set_output(&p, tmp);
  const char *ver[] = {"mytool", "-V"};
  ASSERT_EQ(argv_parse(&p, 2, ver), ARGV_VERSION_REQUESTED);
  read_tmp_output(tmp, buf, sizeof(buf));
  ASSERT_TRUE(strstr(buf, "mytool 2.5.0") != NULL);
  ASSERT_TRUE(strstr(buf, "git-abc123") != NULL);

  argv_free(&p);
}

TEST(NewFeatures, PlaceholderGroupsEpilogAndWidth) {
  const char *pdf = NULL;
  const char *fmt = NULL;
  const char *pos = NULL;
  int *ports = NULL;
  size_t port_cnt = 0;

  CliParser p;
  argv_init(&p, "gomdv");
  argv_set_description(&p, "Markdown viewer");
  argv_set_epilog(&p, "Examples:\n  gomdv README.md");
  argv_set_help_width(&p, 35);

  argv_bind_str(&p, "o", "pdf", &pdf, false, NULL, "Export PDF to file");
  argv_bind_str(&p, NULL, "pdf-format", &fmt, false, "A4,Letter", "Page format with a long description to test width wrapping");
  argv_bind_int_list(&p, "p", "port", &ports, &port_cnt, false, NULL, "Ports");
  argv_bind_pos(&p, "target", &pos, false, NULL, "Target file");

  argv_set_placeholder(&p, "pdf", "path");
  argv_set_placeholder(&p, "port", "<port>");
  argv_set_placeholder(&p, "target", "file");
  argv_set_help_group(&p, "pdf", "PDF Options");
  argv_set_help_group(&p, "pdf-format", "PDF Options:");

  FILE *tmp = tmpfile();
  ASSERT_TRUE(tmp != NULL);
  argv_set_output(&p, tmp);
  const char *args[] = {"gomdv", "-h"};
  ASSERT_EQ(argv_parse(&p, 2, args), ARGV_HELP_REQUESTED);

  char buf[4096];
  read_tmp_output(tmp, buf, sizeof(buf));

  ASSERT_TRUE(strstr(buf, "Markdown viewer") != NULL);
  ASSERT_TRUE(strstr(buf, "Usage: gomdv [OPTIONS] [file]") != NULL);
  ASSERT_TRUE(strstr(buf, "-p, --port <port>...") != NULL);
  ASSERT_TRUE(strstr(buf, "\nPDF Options:\n") != NULL);
  ASSERT_TRUE(strstr(buf, "-o, --pdf <path>") != NULL);
  ASSERT_TRUE(strstr(buf, "Examples:\n  gomdv README.md") != NULL);

  argv_set_help_width(&p, 0);
  setenv("COLUMNS", "80", 1);
  tmp = tmpfile();
  ASSERT_TRUE(tmp != NULL);
  argv_set_output(&p, tmp);
  ASSERT_EQ(argv_parse(&p, 2, args), ARGV_HELP_REQUESTED);
  read_tmp_output(tmp, buf, sizeof(buf));
  ASSERT_TRUE(strstr(buf, "PDF Options:") != NULL);
  unsetenv("COLUMNS");

  argv_free(&p);
}

TEST(NewFeatures, DuplicateFlagsAndBoolReset) {
  bool b1 = false, b2 = false;
  CliParser p_dup;
  argv_init(&p_dup, "app");
  argv_bind_bool(&p_dup, "a", "alpha", &b1, "A");
  argv_bind_bool(&p_dup, "a", "other", &b2, "Dup short");
  const char *a_dup[] = {"app"};
  ASSERT_EQ(argv_parse(&p_dup, 1, a_dup), ARGV_ERR_INVALID_VALUE);
  ASSERT_TRUE(strstr(argv_get_error(&p_dup), "Duplicate short flag") != NULL);
  argv_free(&p_dup);

  bool open = true;
  CliParser p;
  argv_init(&p, "app");
  argv_bind_bool(&p, NULL, "open", &open, "Open");

  const char *a1[] = {"app", "--no-open"};
  ASSERT_EQ(argv_parse(&p, 2, a1), ARGV_OK);
  ASSERT_FALSE(open);

  const char *a2[] = {"app"};
  ASSERT_EQ(argv_parse(&p, 1, a2), ARGV_OK);
  ASSERT_TRUE(open);

  argv_free(&p);
}

TEST(NewFeatures, IntAndFloatLists) {
  int *ports = NULL;
  size_t port_cnt = 0;
  double *scales = NULL;
  size_t scale_cnt = 0;

  CliParser p;
  argv_init(&p, "app");
  argv_set_env_prefix(&p, "LISTAPP");
  argv_bind_int_list(&p, "p", "port", &ports, &port_cnt, false, "80, 443, 8080", "Ports");
  argv_bind_float_list(&p, "s", "scale", &scales, &scale_cnt, false, NULL, "Scales");

  argv_set_default(&p, "port", "80");

  const char *a1[] = {"app"};
  ASSERT_EQ(argv_parse(&p, 1, a1), ARGV_OK);
  ASSERT_EQ(port_cnt, 1);
  ASSERT_EQ(ports[0], 80);
  ASSERT_EQ(scale_cnt, 0);

  setenv("LISTAPP_SCALE", "1.25", 1);
  const char *a2[] = {"app", "-p", "443", "-p", "8080"};
  ASSERT_EQ(argv_parse(&p, 5, a2), ARGV_OK);
  ASSERT_EQ(port_cnt, 2);
  ASSERT_EQ(ports[0], 443);
  ASSERT_EQ(ports[1], 8080);
  ASSERT_EQ(scale_cnt, 1);
  ASSERT_FLOAT_EQ(scales[0], 1.25, 1e-6);
  unsetenv("LISTAPP_SCALE");

  const char *bad[] = {"app", "-p", "999"};
  ASSERT_EQ(argv_parse(&p, 3, bad), ARGV_ERR_INVALID_VALUE);

  argv_free(&p);
}

int main(void) {
  return test_run();
}

TEST(NewFeatures, StopAtPositionalSubcommands) {
  int port = 0;
  const char *cmd = NULL;

  CliParser root;
  argv_init(&root, "mytool");
  argv_set_stop_at_positional(&root, true);
  argv_bind_int(&root, "p", "port", &port, false, NULL, "Port");
  argv_bind_pos(&root, "command", &cmd, true, "commit,clone", "Subcommand");

  FILE *tmp = tmpfile();
  ASSERT_TRUE(tmp != NULL);
  argv_set_output(&root, tmp);
  const char *a1[] = {"mytool", "-p", "8080", "--help"};
  ASSERT_EQ(argv_parse(&root, 4, a1), ARGV_HELP_REQUESTED);
  char buf[2048];
  read_tmp_output(tmp, buf, sizeof(buf));
  ASSERT_TRUE(strstr(buf, "Usage: mytool [OPTIONS] <command") != NULL);

  const char *a2[] = {"mytool", "-p", "8080", "commit", "-m", "hello", "--help"};
  ASSERT_EQ(argv_parse(&root, 7, a2), ARGV_OK);
  ASSERT_EQ(port, 8080);
  ASSERT_STR_EQ(cmd, "commit");

  int rem_argc = 0;
  const char *const *rem_argv = argv_remaining_args(&root, &rem_argc);
  ASSERT_EQ(rem_argc, 3);
  ASSERT_STR_EQ(rem_argv[0], "-m");
  ASSERT_STR_EQ(rem_argv[1], "hello");
  ASSERT_STR_EQ(rem_argv[2], "--help");

  const char *a3[] = {"mytool", "clone"};
  ASSERT_EQ(argv_parse(&root, 2, a3), ARGV_OK);
  ASSERT_STR_EQ(cmd, "clone");
  rem_argv = argv_remaining_args(&root, &rem_argc);
  ASSERT_EQ(rem_argc, 0);
  ASSERT_TRUE(rem_argv == NULL);

  argv_free(&root);
}

TEST(NewFeatures, IsSetAndAnyOf) {
  int port = 0;
  int id = 0;
  const char *email = NULL;
  const char *target = NULL;

  CliParser p;
  argv_init(&p, "app");
  argv_set_env_prefix(&p, "ANYAPP");
  argv_bind_int(&p, "p", "port", &port, false, NULL, "Port");
  argv_bind_int(&p, "i", "id", &id, false, NULL, "ID");
  argv_bind_str(&p, "e", "email", &email, false, NULL, "Email");
  argv_bind_pos(&p, "target", &target, false, NULL, "Target");

  argv_set_default(&p, "port", "8080");
  argv_add_exclusive(&p, "id, email");
  argv_add_any_of(&p, "id, email");

  const char *none[] = {"app"};
  ASSERT_EQ(argv_parse(&p, 1, none), ARGV_ERR_MISSING_REQUIRED);

  const char *with_id[] = {"app", "--id", "42", "user1"};
  ASSERT_EQ(argv_parse(&p, 4, with_id), ARGV_OK);
  ASSERT_EQ(port, 8080);
  ASSERT_FALSE(argv_is_set(&p, "port"));
  ASSERT_FALSE(argv_is_set(&p, "-p"));
  ASSERT_TRUE(argv_is_set(&p, "id"));
  ASSERT_TRUE(argv_is_set(&p, "-i"));
  ASSERT_FALSE(argv_is_set(&p, "email"));
  ASSERT_TRUE(argv_is_set(&p, "target"));
  ASSERT_FALSE(argv_is_set(&p, "nonexistent"));

  const char *explicit_def[] = {"app", "-p", "8080", "--email", "a@b.com"};
  ASSERT_EQ(argv_parse(&p, 5, explicit_def), ARGV_OK);
  ASSERT_TRUE(argv_is_set(&p, "port"));
  ASSERT_TRUE(argv_is_set(&p, "--email"));
  ASSERT_FALSE(argv_is_set(&p, "target"));

  setenv("ANYAPP_ID", "7", 1);
  const char *from_env[] = {"app"};
  ASSERT_EQ(argv_parse(&p, 1, from_env), ARGV_OK);
  ASSERT_EQ(id, 7);
  ASSERT_TRUE(argv_is_set(&p, "id"));
  ASSERT_FALSE(argv_is_set(&p, "port"));

  const char *both[] = {"app", "--email", "a@b.com"};
  ASSERT_EQ(argv_parse(&p, 3, both), ARGV_ERR_MUTUAL_EXCLUSION);
  unsetenv("ANYAPP_ID");

  CliParser bad_rule;
  bool ok = false;
  argv_init(&bad_rule, "app");
  argv_bind_bool(&bad_rule, "o", "ok", &ok, "OK");
  argv_add_any_of(&bad_rule, "ok, ghost");
  const char *help_args[] = {"app", "-h"};
  ASSERT_EQ(argv_parse(&bad_rule, 2, help_args), ARGV_ERR_UNKNOWN_FLAG);
  argv_free(&bad_rule);

  argv_free(&p);
}

TEST(NewFeatures, InlineErrorsPosLabelsAndReservedFlags) {
  bool open = true;
  int verbose = 0;
  CliParser p_inline;
  argv_init(&p_inline, "app");
  argv_bind_bool(&p_inline, "o", "open", &open, "Open");
  argv_bind_count(&p_inline, "v", "verbose", &verbose, "Verbose");

  const char *neg_true[] = {"app", "--no-open=true"};
  ASSERT_EQ(argv_parse(&p_inline, 2, neg_true), ARGV_ERR_INVALID_VALUE);

  const char *neg_false[] = {"app", "--no-open=false"};
  ASSERT_EQ(argv_parse(&p_inline, 2, neg_false), ARGV_ERR_INVALID_VALUE);

  const char *cnt_val[] = {"app", "--verbose=2"};
  ASSERT_EQ(argv_parse(&p_inline, 2, cnt_val), ARGV_ERR_INVALID_VALUE);
  argv_free(&p_inline);

  const char *cmd = NULL;
  const char **files = NULL;
  size_t file_cnt = 0;
  CliParser p_pos;
  argv_init(&p_pos, "git");
  argv_bind_pos(&p_pos, "command", &cmd, true, "commit,clone", "Command");
  argv_bind_rest(&p_pos, "files", &files, &file_cnt, false, "a,b", "Files");
  ASSERT_TRUE(argv_set_choice_help(&p_pos, "command", "commit", "Commit"));
  ASSERT_TRUE(argv_set_choice_help(&p_pos, "command", "clone", "Clone"));
  ASSERT_TRUE(argv_set_choice_help(&p_pos, "files", "a", "File A"));
  ASSERT_TRUE(argv_set_choice_help(&p_pos, "files", "b", "File B"));

  FILE *tmp = tmpfile();
  ASSERT_TRUE(tmp != NULL);
  argv_set_output(&p_pos, tmp);
  const char *h_args[] = {"git", "-h"};
  ASSERT_EQ(argv_parse(&p_pos, 2, h_args), ARGV_HELP_REQUESTED);
  char buf[4096];
  read_tmp_output(tmp, buf, sizeof(buf));
  ASSERT_TRUE(strstr(buf, "Usage: git [OPTIONS] <command> [files...]") != NULL);
  ASSERT_TRUE(strstr(buf, "<command <str>>") == NULL);
  ASSERT_TRUE(strstr(buf, "files......") == NULL);
  argv_free(&p_pos);

  bool f = false;
  const char *empty_args[] = {"app"};

  CliParser c1;
  argv_init(&c1, "app");
  argv_bind_bool(&c1, "h", "host", &f, "Host");
  ASSERT_EQ(argv_parse(&c1, 1, empty_args), ARGV_ERR_INVALID_VALUE);
  ASSERT_TRUE(strstr(argv_get_error(&c1), "reserved help flag") != NULL);
  argv_free(&c1);

  CliParser c2;
  argv_init(&c2, "app");
  argv_bind_bool(&c2, "H", "help", &f, "Help");
  ASSERT_EQ(argv_parse(&c2, 1, empty_args), ARGV_ERR_INVALID_VALUE);
  argv_free(&c2);

  CliParser c3;
  argv_init(&c3, "app");
  argv_set_version(&c3, "1.0", "v", NULL);
  argv_bind_bool(&c3, "v", "verbose", &f, "Verbose");
  ASSERT_EQ(argv_parse(&c3, 1, empty_args), ARGV_ERR_INVALID_VALUE);
  ASSERT_TRUE(strstr(argv_get_error(&c3), "reserved version") != NULL);
  argv_free(&c3);

  CliParser c4;
  argv_init(&c4, "app");
  argv_bind_bool(&c4, "v", "verbose", &f, "Verbose");
  argv_set_version(&c4, "1.0", "v", NULL);
  ASSERT_EQ(argv_parse(&c4, 1, empty_args), ARGV_ERR_INVALID_VALUE);
  ASSERT_TRUE(strstr(argv_get_error(&c4), "conflicts with already bound flag") != NULL);
  argv_free(&c4);

  CliParser c5;
  argv_init(&c5, "app");
  argv_set_version(&c5, "1.0", "h", NULL);
  ASSERT_EQ(argv_parse(&c5, 1, empty_args), ARGV_ERR_INVALID_VALUE);
  argv_free(&c5);

  const char *p_help = NULL, *p_ver = NULL;
  CliParser c_pos;
  argv_init(&c_pos, "app");
  argv_bind_pos(&c_pos, "help", &p_help, false, NULL, "Help topic");
  argv_bind_pos(&c_pos, "version", &p_ver, false, NULL, "Version target");
  const char *pos_vals[] = {"app", "topic", "v2"};
  ASSERT_EQ(argv_parse(&c_pos, 3, pos_vals), ARGV_OK);
  ASSERT_STR_EQ(p_help, "topic");
  ASSERT_STR_EQ(p_ver, "v2");
  argv_free(&c_pos);
}

TEST(NewFeatures, CParityFixesAndEdgeCases) {
  const char *mode = NULL;
  int num = 0;
  double flt = 0.0;
  CliParser p;
  argv_init(&p, "app");
  argv_bind_str(&p, "m", "mode", &mode, false, "fast mode, safe mode", "Mode");
  argv_bind_int(&p, "n", "num", &num, false, NULL, "Num");
  argv_bind_float(&p, "f", "flt", &flt, false, NULL, "Float");

  const char *ok_space[] = {"app", "-m", "fast mode"};
  ASSERT_EQ(argv_parse(&p, 3, ok_space), ARGV_OK);
  ASSERT_STR_EQ(mode, "fast mode");

  const char *bad_half[] = {"app", "-m", "fast"};
  ASSERT_EQ(argv_parse(&p, 3, bad_half), ARGV_ERR_INVALID_VALUE);

  const char *lead_int[] = {"app", "-n", " 42"};
  ASSERT_EQ(argv_parse(&p, 3, lead_int), ARGV_ERR_INVALID_VALUE);

  const char *lead_flt[] = {"app", "-f", " 1.5"};
  ASSERT_EQ(argv_parse(&p, 3, lead_flt), ARGV_ERR_INVALID_VALUE);
  argv_free(&p);

  bool a = false, b = false;
  CliParser p_sp;
  argv_init(&p_sp, "app");
  argv_bind_bool(&p_sp, "a", "alpha", &a, "A");
  argv_bind_bool(&p_sp, "b", "beta", &b, "B");
  argv_add_exclusive(&p_sp, "alpha beta");
  const char *a_sp[] = {"app"};
  ASSERT_EQ(argv_parse(&p_sp, 1, a_sp), ARGV_ERR_UNKNOWN_FLAG);
  argv_free(&p_sp);

  int long_val = 0;
  const char *any_str = NULL;
  CliParser p_env;
  argv_init(&p_env, "/usr/local/bin/");
  argv_set_env_prefix(&p_env, "VERY_LONG_MICROSERVICE_PREFIX");
  argv_bind_int(&p_env, NULL, "very-long-configuration-option-name", &long_val, false, NULL, "First line\n\tsecond line");
  argv_bind_str(&p_env, "s", "str", &any_str, false, "", "Empty choices treated as none");

  ASSERT_FALSE(argv_set_choice_help(&p_env, "str", "x", "X"));
  ASSERT_TRUE(strstr(argv_get_error(&p_env), "has no choices defined") != NULL);
  argv_free(&p_env);

  argv_init(&p_env, "");
  argv_set_env_prefix(&p_env, "VERY_LONG_MICROSERVICE_PREFIX");
  argv_bind_int(&p_env, NULL, "very-long-configuration-option-name", &long_val, false, NULL, "First line\n\tsecond line");
  argv_bind_str(&p_env, "s", "str", &any_str, false, "", "Empty choices");

  const char *long_env = "VERY_LONG_MICROSERVICE_PREFIX_VERY_LONG_CONFIGURATION_OPTION_NAME";
  setenv(long_env, "123", 1);
  const char *run_args[] = {"", "-s", "anything"};
  ASSERT_EQ(argv_parse(&p_env, 3, run_args), ARGV_OK);
  ASSERT_EQ(long_val, 123);
  ASSERT_STR_EQ(any_str, "anything");
  unsetenv(long_env);

  FILE *tmp = tmpfile();
  ASSERT_TRUE(tmp != NULL);
  argv_set_output(&p_env, tmp);
  const char *h_args[] = {"", "-h"};
  ASSERT_EQ(argv_parse(&p_env, 2, h_args), ARGV_HELP_REQUESTED);
  char buf[4096];
  read_tmp_output(tmp, buf, sizeof(buf));
  ASSERT_TRUE(strstr(buf, "Usage: program [OPTIONS]") != NULL);
  ASSERT_TRUE(strstr(buf, "First line second line") != NULL);
  ASSERT_TRUE(strstr(buf, "\n\t") == NULL);
  ASSERT_TRUE(strstr(buf, long_env) != NULL);
  argv_free(&p_env);
}
