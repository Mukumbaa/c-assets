/*
gcc -Wall -Wextra -Werror -g test_argv.c -o test_argv && \
valgrind --leak-check=full --show-leak-kinds=all --track-origins=yes --error-exitcode=1 ./test_argv
*/
#define _POSIX_C_SOURCE 200112L

#define LIBARGV_IMPLEMENTATION
#include "standalone_lib/libargv.h"

#define LIBTEST_IMPLEMENTATION
#include "../libtest/standalone_lib/libtest.h"

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

int main(void) {
  return test_run();
}
