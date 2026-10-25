#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

#define LIBTEST_IMPLEMENTATION
#include "../../libtest/standalone_lib/libtest.h"

#define LIBSTR_IMPLEMENTATION
#include "../standalone_lib/libstr.h"


TEST(str_lifecycle, init_and_empty) {
    String s1 = str_init("Hello");
    ASSERT_EQ(s1.len, 5);
    ASSERT_STR_EQ(s1.str, "Hello");
    ASSERT_TRUE(s1.capacity >= 16);
    str_destroy(&s1);

    String s2 = str_init(NULL);
    ASSERT_EQ(s2.len, 0);
    ASSERT_STR_EQ(s2.str, "");
    ASSERT_TRUE(str_is_empty(&s2));
    str_destroy(&s2);

    String s3 = str_empty();
    ASSERT_EQ(s3.len, 0);
    ASSERT_STR_EQ(s3.str, "");
    ASSERT_TRUE(str_is_empty(&s3));
    str_destroy(&s3);
}

TEST(str_lifecycle, init_with_capacity) {
    String s = str_init_with_cap(64);
    ASSERT_EQ(s.len, 0);
    ASSERT_TRUE(s.capacity >= 64);
    ASSERT_STR_EQ(s.str, "");
    str_destroy(&s);
}

TEST(str_lifecycle, duplication) {
    String orig = str_init("Clone me");
    String copy = str_dup(&orig);

    ASSERT_EQ(copy.len, orig.len);
    ASSERT_STR_EQ(copy.str, orig.str);
    ASSERT_TRUE(copy.str != orig.str);

    str_destroy(&orig);
    str_destroy(&copy);
}

TEST(str_lifecycle, formatted_construction) {
    String s = str_from_fmt("Value: %d, Text: %s", 42, "answer");
    ASSERT_STR_EQ(s.str, "Value: 42, Text: answer");
    ASSERT_EQ(s.len, 23);
    str_destroy(&s);
}

TEST(str_lifecycle, clear_and_reset) {
    String s = str_init("Persistent buffer");
    size_t old_cap = s.capacity;

    str_clear(&s);
    ASSERT_EQ(s.len, 0);
    ASSERT_STR_EQ(s.str, "");
    ASSERT_EQ(s.capacity, old_cap);

    str_append_cstr(&s, "Reused");
    ASSERT_STR_EQ(s.str, "Reused");

    str_reset(&s);
    ASSERT_EQ(s.len, 0);
    ASSERT_STR_EQ(s.str, "");

    str_destroy(&s);
}


TEST(str_capacity, reserve_and_shrink) {
    String s = str_init("shrinkable");
    ASSERT_TRUE(str_reserve(&s, 128));
    ASSERT_TRUE(s.capacity >= 128);
    ASSERT_STR_EQ(s.str, "shrinkable");

    ASSERT_TRUE(str_shrink_to_fit(&s));
    ASSERT_EQ(s.capacity, s.len + 1);
    ASSERT_STR_EQ(s.str, "shrinkable");

    str_destroy(&s);
}


TEST(str_concat, push_and_pop_char) {
    String s = str_empty();

    ASSERT_TRUE(str_push_char(&s, 'A'));
    ASSERT_TRUE(str_push_char(&s, 'B'));
    ASSERT_TRUE(str_push_char(&s, 'C'));
    ASSERT_STR_EQ(s.str, "ABC");
    ASSERT_EQ(s.len, 3);

    char out = '\0';
    ASSERT_TRUE(str_pop(&s, &out));
    ASSERT_EQ(out, 'C');
    ASSERT_STR_EQ(s.str, "AB");
    ASSERT_EQ(s.len, 2);

    ASSERT_TRUE(str_pop(&s, NULL));
    ASSERT_STR_EQ(s.str, "A");
    ASSERT_EQ(s.len, 1);

    ASSERT_TRUE(str_pop(&s, NULL));
    ASSERT_FALSE(str_pop(&s, NULL));

    str_destroy(&s);
}

TEST(str_concat, append_types) {
    String s = str_init("Hello");
    String suffix = str_init(" World");

    ASSERT_TRUE(str_append(&s, &suffix));
    ASSERT_STR_EQ(s.str, "Hello World");

    ASSERT_TRUE(str_append_cstr(&s, "!"));
    ASSERT_STR_EQ(s.str, "Hello World!");

    ASSERT_TRUE(str_append_fmt(&s, " Number: %d", 100));
    ASSERT_STR_EQ(s.str, "Hello World! Number: 100");

    str_destroy(&suffix);
    str_destroy(&s);
}

TEST(str_concat, insert_at_index) {
    String s = str_init("ac");
    String b = str_init("b");

    ASSERT_TRUE(str_cat(&s, &b, 1));
    ASSERT_STR_EQ(s.str, "abc");

    String head = str_init("start: ");
    ASSERT_TRUE(str_cat(&s, &head, 0));
    ASSERT_STR_EQ(s.str, "start: abc");

    str_destroy(&head);
    str_destroy(&b);
    str_destroy(&s);
}

TEST(str_concat, self_append_aliasing) {
    String s = str_init("A");

    for (int i = 0; i < 6; i++) {
        ASSERT_TRUE(str_append(&s, &s));
    }

    ASSERT_EQ(s.len, 64);
    for (size_t i = 0; i < s.len; i++) {
        ASSERT_EQ(s.str[i], 'A');
    }

    str_destroy(&s);
}

TEST(str_concat, copy_overwrite) {
    String dest = str_init("Long initial destination text");
    String src = str_init("Short");

    ASSERT_TRUE(str_cpy(&dest, &src));
    ASSERT_STR_EQ(dest.str, "Short");
    ASSERT_EQ(dest.len, 5);

    str_destroy(&dest);
    str_destroy(&src);
}


TEST(str_edit, substring) {
    String s = str_init("The quick brown fox");

    String sub = str_substr(&s, 4, 5);
    ASSERT_STR_EQ(sub.str, "quick");
    ASSERT_EQ(sub.len, 5);
    str_destroy(&sub);

    String tail = str_substr(&s, 16, 50);
    ASSERT_STR_EQ(tail.str, "fox");
    ASSERT_EQ(tail.len, 3);
    str_destroy(&tail);

    String empty = str_substr(&s, 100, 5);
    ASSERT_TRUE(str_is_empty(&empty));
    str_destroy(&empty);

    str_destroy(&s);
}

TEST(str_edit, remove_range) {
    String s = str_init("Hello Beautiful World");

    ASSERT_TRUE(str_remove(&s, 6, 10));
    ASSERT_STR_EQ(s.str, "Hello World");
    ASSERT_EQ(s.len, 11);

    ASSERT_TRUE(str_remove(&s, 5, 20));
    ASSERT_STR_EQ(s.str, "Hello");
    ASSERT_EQ(s.len, 5);

    str_destroy(&s);
}

TEST(str_edit, replace_occurrences) {
    String s = str_init("apple, banana, apple, orange");

    ASSERT_TRUE(str_replace(&s, "apple", "strawberry"));
    ASSERT_STR_EQ(s.str, "strawberry, banana, strawberry, orange");

    ASSERT_TRUE(str_replace(&s, "strawberry", "fig"));
    ASSERT_STR_EQ(s.str, "fig, banana, fig, orange");

    ASSERT_TRUE(str_replace(&s, "pineapple", "mango"));
    ASSERT_STR_EQ(s.str, "fig, banana, fig, orange");

    str_destroy(&s);
}

TEST(str_transform, trimming) {
    String s1 = str_init("   \t  leading");
    str_ltrim(&s1);
    ASSERT_STR_EQ(s1.str, "leading");
    str_destroy(&s1);

    String s2 = str_init("trailing  \n  ");
    str_rtrim(&s2);
    ASSERT_STR_EQ(s2.str, "trailing");
    str_destroy(&s2);

    String s3 = str_init("  \r\n  both ends   \t ");
    str_trim(&s3);
    ASSERT_STR_EQ(s3.str, "both ends");
    str_destroy(&s3);
}

TEST(str_transform, case_conversions) {
    String s = str_init("HeLLo, WoRLd! 123");

    str_to_lower(&s);
    ASSERT_STR_EQ(s.str, "hello, world! 123");

    str_to_upper(&s);
    ASSERT_STR_EQ(s.str, "HELLO, WORLD! 123");

    str_destroy(&s);
}

TEST(str_transform, reverse) {
    String s = str_init("step on no pets");
    str_reverse(&s);
    ASSERT_STR_EQ(s.str, "step on no pets");
    str_destroy(&s);

    String s2 = str_init("12345");
    str_reverse(&s2);
    ASSERT_STR_EQ(s2.str, "54321");
    str_destroy(&s2);
}


TEST(str_query, equality_and_comparisons) {
    String a = str_init("alpha");
    String b = str_init("beta");
    String a_copy = str_init("alpha");

    ASSERT_TRUE(str_equals(&a, &a_copy));
    ASSERT_FALSE(str_equals(&a, &b));
    ASSERT_TRUE(str_equals_cstr(&a, "alpha"));
    ASSERT_FALSE(str_equals_cstr(&a, "gamma"));

    ASSERT_TRUE(str_cmp(&a, &b) < 0);
    ASSERT_TRUE(str_cmp(&b, &a) > 0);
    ASSERT_EQ(str_cmp(&a, &a_copy), 0);

    str_destroy(&a);
    str_destroy(&b);
    str_destroy(&a_copy);
}

TEST(str_query, prefix_and_suffix) {
    String s = str_init("libstr.so.1.0");

    ASSERT_TRUE(str_starts_with(&s, "lib"));
    ASSERT_TRUE(str_starts_with(&s, "libstr.so"));
    ASSERT_FALSE(str_starts_with(&s, "str"));

    ASSERT_TRUE(str_ends_with(&s, ".0"));
    ASSERT_TRUE(str_ends_with(&s, "1.0"));
    ASSERT_FALSE(str_ends_with(&s, ".so"));

    str_destroy(&s);
}

TEST(str_query, contains_and_find_last) {
    String s = str_init("The quick brown Fox jumps over the lazy fox");

    ASSERT_EQ(str_contains_cstr(&s, "Fox", CASE_SENSITIVE), 16);
    ASSERT_EQ(str_contains_cstr(&s, "fox", CASE_SENSITIVE), 40);
    ASSERT_EQ(str_contains_cstr(&s, "elephant", CASE_SENSITIVE), -1);

    ASSERT_EQ(str_contains_cstr(&s, "fox", NO_CASE_SENSITIVE), 16);

    ASSERT_EQ(str_find_last(&s, "the", NO_CASE_SENSITIVE), 31);
    ASSERT_EQ(str_find_last(&s, "The", CASE_SENSITIVE), 0);
    ASSERT_EQ(str_find_last(&s, "not_found", CASE_SENSITIVE), -1);

    str_destroy(&s);
}

TEST(str_tokenization, split_basic) {
    String s = str_init("one,two,three,four");
    String *tokens = NULL;

    int count = str_split(&tokens, &s, ',');
    ASSERT_EQ(count, 4);
    ASSERT_TRUE(tokens != NULL);

    ASSERT_STR_EQ(tokens[0].str, "one");
    ASSERT_STR_EQ(tokens[1].str, "two");
    ASSERT_STR_EQ(tokens[2].str, "three");
    ASSERT_STR_EQ(tokens[3].str, "four");

    str_free_split(tokens, count);
    str_destroy(&s);
}

TEST(str_tokenization, split_no_delimiter) {
    String s = str_init("single_token");
    String *tokens = NULL;

    int count = str_split(&tokens, &s, ',');
    ASSERT_EQ(count, 1);
    ASSERT_STR_EQ(tokens[0].str, "single_token");

    str_free_split(tokens, count);
    str_destroy(&s);
}

TEST(str_tokenization, join_strings) {
    String tokens[3];
    tokens[0] = str_init("2026");
    tokens[1] = str_init("03");
    tokens[2] = str_init("30");

    String joined = str_empty();
    ASSERT_TRUE(str_join(&joined, tokens, 3, "-"));
    ASSERT_STR_EQ(joined.str, "2026-03-30");

    String no_sep = str_empty();
    ASSERT_TRUE(str_join(&no_sep, tokens, 3, ""));
    ASSERT_STR_EQ(no_sep.str, "20260330");

    for (int i = 0; i < 3; i++) {
        str_destroy(&tokens[i]);
    }
    str_destroy(&joined);
    str_destroy(&no_sep);
}

int main(void) {
    printf("=== TEST SUITE: libstr.h (via libtest.h) ===\n\n");
    test_run();
    return 0;
}
