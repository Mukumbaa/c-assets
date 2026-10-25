#include <stdio.h>

#define LIBTEST_IMPLEMENTATION
#include "../../libtest/standalone_lib/libtest.h"

#define LIBVEC_IMPLEMENTATION
#include "../standalone_lib/libvec.h"


TEST(VectorInitFree, DefaultInit) {
    vector v;
    int res = vec_init(&v, sizeof(int));
    
    ASSERT_EQ(res, 0);
    ASSERT_EQ(vec_len(&v), 0);
    ASSERT_EQ(vec_capacity(&v), VEC_BASIC_CAP);
    ASSERT_TRUE(vec_is_empty(&v));
    ASSERT_NE(vec_data(&v), NULL);

    vec_free(&v);
    ASSERT_EQ(vec_len(&v), 0);
    ASSERT_EQ(vec_capacity(&v), 0);
    ASSERT_EQ(vec_data(&v), NULL);
}

TEST(VectorInitFree, CustomCapacity) {
    vector v;
    int res = vec_init_wcapacity(&v, sizeof(double), 64);
    
    ASSERT_EQ(res, 0);
    ASSERT_EQ(vec_len(&v), 0);
    ASSERT_EQ(vec_capacity(&v), 64);

    vec_free(&v);
}

TEST(VectorInitFree, InvalidInit) {
    vector v;
    ASSERT_NE(vec_init(NULL, sizeof(int)), 0);
    ASSERT_NE(vec_init(&v, 0), 0);
    ASSERT_NE(vec_init_wcapacity(&v, sizeof(int), 0), 0);
}


TEST(VectorAppendPop, BasicPushPop) {
    vector v;
    vec_init(&v, sizeof(int));

    for (int i = 0; i < 5; i++) {
        int res = vec_append(&v, &i);
        ASSERT_EQ(res, 0);
    }

    ASSERT_EQ(vec_len(&v), 5);
    ASSERT_FALSE(vec_is_empty(&v));

    int *first = (int *)vec_first(&v);
    int *last = (int *)vec_last(&v);
    ASSERT_NE(first, NULL);
    ASSERT_NE(last, NULL);
    ASSERT_EQ(*first, 0);
    ASSERT_EQ(*last, 4);

    int out_val = -1;
    int pop_res = vec_pop(&v, &out_val);
    ASSERT_EQ(pop_res, 0);
    ASSERT_EQ(out_val, 4);
    ASSERT_EQ(vec_len(&v), 4);

    pop_res = vec_pop(&v, NULL);
    ASSERT_EQ(pop_res, 0);
    ASSERT_EQ(vec_len(&v), 3);

    vec_free(&v);
}

TEST(VectorAppendPop, PopOnEmpty) {
    vector v;
    vec_init(&v, sizeof(int));

    int val = 0;
    int res = vec_pop(&v, &val);
    ASSERT_NE(res, 0);

    vec_free(&v);
}

TEST(VectorAppendPop, AutoGrowth) {
    vector v;
    vec_init_wcapacity(&v, sizeof(int), 2);

    for (int i = 0; i < 100; i++) {
        vec_append(&v, &i);
    }

    ASSERT_EQ(vec_len(&v), 100);
    ASSERT_TRUE(vec_capacity(&v) >= 100);

    for (size_t i = 0; i < 100; i++) {
        int *val = (int *)vec_get(&v, i);
        ASSERT_NE(val, NULL);
        ASSERT_EQ(*val, (int)i);
    }

    vec_free(&v);
}

TEST(VectorInsert, InsertAtPositions) {
    vector v;
    vec_init(&v, sizeof(int));

    int a = 10, b = 20, c = 30;
    vec_append(&v, &a);
    vec_append(&v, &c);

    int res = vec_insert_at(&v, &b, 1);
    ASSERT_EQ(res, 0);
    ASSERT_EQ(vec_len(&v), 3);

    int *val0 = (int *)vec_get(&v, 0);
    int *val1 = (int *)vec_get(&v, 1);
    int *val2 = (int *)vec_get(&v, 2);

    ASSERT_EQ(*val0, 10);
    ASSERT_EQ(*val1, 20);
    ASSERT_EQ(*val2, 30);

    int zero = 0;
    vec_insert_at(&v, &zero, 0);
    ASSERT_EQ(*(int *)vec_get(&v, 0), 0);
    ASSERT_EQ(vec_len(&v), 4);

    int end = 40;
    vec_insert_at(&v, &end, vec_len(&v));
    ASSERT_EQ(*(int *)vec_last(&v), 40);
    ASSERT_EQ(vec_len(&v), 5);

    int invalid = 999;
    ASSERT_NE(vec_insert_at(&v, &invalid, 100), 0);

    vec_free(&v);
}

TEST(VectorMemoryAndCapacity, ClearAndShrink) {
    vector v;
    vec_init(&v, sizeof(int));

    for (int i = 0; i < 50; i++) {
        vec_append(&v, &i);
    }

    size_t old_cap = vec_capacity(&v);
    ASSERT_TRUE(old_cap >= 50);

    vec_clear(&v);
    ASSERT_EQ(vec_len(&v), 0);
    ASSERT_TRUE(vec_is_empty(&v));
    ASSERT_EQ(vec_capacity(&v), old_cap);

    int res = vec_shrink_to_fit(&v);
    ASSERT_EQ(res, 0);
    ASSERT_EQ(vec_capacity(&v), 0);
    ASSERT_EQ(vec_data(&v), NULL);

    vec_free(&v);
}

TEST(VectorMemoryAndCapacity, Reserve) {
    vector v;
    vec_init(&v, sizeof(int));

    int res = vec_reserve(&v, 128);
    ASSERT_EQ(res, 0);
    ASSERT_TRUE(vec_capacity(&v) >= 128);

    size_t cap = vec_capacity(&v);
    ASSERT_EQ(vec_reserve(&v, 0), 0);
    ASSERT_EQ(vec_capacity(&v), cap);

    vec_free(&v);
}


TEST(VectorSearch, ContainsPrimitive) {
    vector v;
    vec_init(&v, sizeof(int));

    for (int i = 0; i < 10; i++) {
        int val = i * 2;
        vec_append(&v, &val);
    }

    int target_found = 6;
    int target_not_found = 7;

    ASSERT_EQ(vec_contains(&v, &target_found), 1);
    ASSERT_EQ(vec_contains(&v, &target_not_found), 0);

    vec_free(&v);
}


typedef struct {
    int id;
    char code[8];
} Item;

TEST(VectorComplexType, StructHandling) {
    vector v;
    vec_init(&v, sizeof(Item));

    Item item1 = { .id = 1, .code = "ABC" };
    Item item2 = { .id = 2, .code = "XYZ" };

    vec_append(&v, &item1);
    vec_append(&v, &item2);

    ASSERT_EQ(vec_len(&v), 2);

    Item *retrieved = (Item *)vec_get(&v, 1);
    ASSERT_NE(retrieved, NULL);
    ASSERT_EQ(retrieved->id, 2);
    ASSERT_STR_EQ(retrieved->code, "XYZ");

    Item query = { .id = 1, .code = "ABC" };
    ASSERT_EQ(vec_contains(&v, &query), 1);

    vec_free(&v);
}


int main(void) {
    printf("Start tests\n\n");
    return test_run();
}
