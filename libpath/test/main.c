#include <stdio.h>
#include <stdlib.h>

#define LIBTEST_IMPLEMENTATION
#include "../../libtest/standalone_lib/libtest.h"

#define LIBPATH_IMPLEMENTATION
#include "../standalone_lib/libpath.h"


TEST(path_basic, basename_cases) {
    ASSERT_STR_EQ(path_basename("foo/bar.txt"), "bar.txt");
    ASSERT_STR_EQ(path_basename("bar.txt"), "bar.txt");
    ASSERT_STR_EQ(path_basename("/usr/local/bin/"), "bin/");
    ASSERT_STR_EQ(path_basename("/"), "/");
    ASSERT_STR_EQ(path_basename(""), "");
    ASSERT_STR_EQ(path_basename(NULL), "");
}

TEST(path_basic, dirname_cases) {
    char *d1 = path_dirname("foo/bar.txt");
    ASSERT_STR_EQ(d1, "foo");
    free(d1);

    char *d2 = path_dirname("/usr/bin");
    ASSERT_STR_EQ(d2, "/usr");
    free(d2);

    char *d3 = path_dirname("/usr");
    ASSERT_STR_EQ(d3, "/");
    free(d3);

    char *d4 = path_dirname("file.txt");
    ASSERT_STR_EQ(d4, ".");
    free(d4);

    char *d5 = path_dirname(NULL);
    ASSERT_STR_EQ(d5, ".");
    free(d5);
}


TEST(path_ext, extension_queries) {
    ASSERT_STR_EQ(path_extension("archive.tar.gz"), "gz");
    ASSERT_STR_EQ(path_extension("document.pdf"), "pdf");
    ASSERT_STR_EQ(path_extension("no_extension"), "");
    ASSERT_STR_EQ(path_extension(".gitignore"), "");
    ASSERT_STR_EQ(path_extension("dir.with.dots/file"), "");
}

TEST(path_ext, stem_extraction) {
    char *s1 = path_stem("document.pdf");
    ASSERT_STR_EQ(s1, "document");
    free(s1);

    char *s2 = path_stem("archive.tar.gz");
    ASSERT_STR_EQ(s2, "archive.tar");
    free(s2);

    char *s3 = path_stem("plain_file");
    ASSERT_STR_EQ(s3, "plain_file");
    free(s3);
}

TEST(path_ext, set_extension) {
    char *e1 = path_set_extension("image.png", "jpg");
    ASSERT_STR_EQ(e1, "image.jpg");
    free(e1);

    char *e2 = path_set_extension("image.png", ".jpg");
    ASSERT_STR_EQ(e2, "image.jpg");
    free(e2);

    char *e3 = path_set_extension("document", "pdf");
    ASSERT_STR_EQ(e3, "document.pdf");
    free(e3);

    char *e4 = path_set_extension("archive.tar.gz", NULL);
    ASSERT_STR_EQ(e4, "archive.tar");
    free(e4);
}

TEST(path_manipulation, join_paths) {
    char *j1 = path_join("folder", "file.txt");
    ASSERT_STR_EQ(j1, "folder/file.txt");
    free(j1);

    char *j2 = path_join("folder/", "file.txt");
    ASSERT_STR_EQ(j2, "folder/file.txt");
    free(j2);

    char *j3 = path_join("", "subpath");
    ASSERT_STR_EQ(j3, "subpath");
    free(j3);

    char *j4 = path_join("basepath", "");
    ASSERT_STR_EQ(j4, "basepath");
    free(j4);

    char *j5 = path_join("basepath", "/absolute/path");
    ASSERT_STR_EQ(j5, "/absolute/path");
    free(j5);
}

TEST(path_manipulation, normalize_dots_and_slashes) {
    char *n1 = path_normalize("a/b/../c");
    ASSERT_STR_EQ(n1, "a/c");
    free(n1);

    char *n2 = path_normalize("a/./b/./c");
    ASSERT_STR_EQ(n2, "a/b/c");
    free(n2);

    char *n3 = path_normalize("a//b///c");
    ASSERT_STR_EQ(n3, "a/b/c");
    free(n3);

    char *n4 = path_normalize("/a/b/../../");
    ASSERT_STR_EQ(n4, "/");
    free(n4);

    char *n5 = path_normalize("");
    ASSERT_STR_EQ(n5, ".");
    free(n5);
}


TEST(path_hierarchy, is_descendant) {
    ASSERT_TRUE(path_is_descendant_of("a/b/c", "a/b"));
    ASSERT_TRUE(path_is_descendant_of("a/b/c", "a"));
    ASSERT_TRUE(path_is_descendant_of("a/b", "a/b"));

    ASSERT_FALSE(path_is_descendant_of("a/b", "a/b/c"));
    ASSERT_FALSE(path_is_descendant_of("other/folder", "a/b"));
}

TEST(path_hierarchy, relative_to) {
    char *r1 = path_relative_to("a/b", "a/b/c/d");
    ASSERT_STR_EQ(r1, "c/d");
    free(r1);

    char *r2 = path_relative_to("a/b/c", "a/b");
    ASSERT_STR_EQ(r2, "..");
    free(r2);

    char *r3 = path_relative_to("a/b", "a/b");
    ASSERT_STR_EQ(r3, ".");
    free(r3);
}


TEST(path_system, absolute_and_canonicalize) {
    char *abs_path = path_to_absolute(".");
    ASSERT_TRUE(abs_path != NULL);
    ASSERT_TRUE(abs_path[0] == '/' || abs_path[1] == ':');
    free(abs_path);

    char *real_dir = path_canonicalize(".");
    ASSERT_TRUE(real_dir != NULL);
    free(real_dir);

    char *non_existent = path_canonicalize("this_file_really_does_not_exist_12345.xyz");
    ASSERT_TRUE(non_existent == NULL);
}

int main(void) {
    printf("=== TEST SUITE: libpath.h (via libtest.h) ===\n\n");
    test_run();
    return 0;
}
