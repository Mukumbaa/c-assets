#include <stdio.h>
#include <stdint.h>

#define LIBTEST_IMPLEMENTATION
#include "../../libtest/standalone_lib/libtest.h"

#define LIBMAP_IMPLEMENTATION
#include "../standalone_lib/libmap.h"

typedef struct {
    int id;
    int score;
} Player;

static void count_callback(const char *key, void *value, void *user_data) {
    (void)key;
    (void)value;
    int *count = (int *)user_data;
    (*count)++;
}

TEST(HashMapInit, DefaultAndCustom) {
    hashmap hm;
    hashmap_init(&hm);

    ASSERT_EQ(hashmap_count(&hm), 0);
    ASSERT_EQ(hm.capacity, 32);
    ASSERT_TRUE(hashmap_is_empty(&hm));
    ASSERT_NE(hm.he, NULL);

    hashmap_destroy(&hm);
    ASSERT_EQ(hashmap_count(&hm), 0);
    ASSERT_EQ(hm.capacity, 0);
    ASSERT_EQ(hm.he, NULL);

    hashmap_init_wcapacity(&hm, 64);
    ASSERT_EQ(hm.capacity, 64);
    ASSERT_EQ(hashmap_count(&hm), 0);
    ASSERT_TRUE(hashmap_is_empty(&hm));

    hashmap_destroy(&hm);
}

TEST(HashMapOperations, SetGetHas) {
    hashmap hm;
    hashmap_init(&hm);

    int val1 = 100;
    int val2 = 200;

    ASSERT_EQ(hashmap_set(&hm, "alpha", &val1), 0);
    ASSERT_EQ(hashmap_set(&hm, "beta", &val2), 0);
    ASSERT_EQ(hashmap_count(&hm), 2);
    ASSERT_FALSE(hashmap_is_empty(&hm));

    ASSERT_EQ(hashmap_has(&hm, "alpha"), 1);
    ASSERT_EQ(hashmap_has(&hm, "beta"), 1);
    ASSERT_EQ(hashmap_has(&hm, "gamma"), 0);

    int *got1 = (int *)hashmap_get(&hm, "alpha");
    int *got2 = (int *)hashmap_get(&hm, "beta");
    void *got3 = hashmap_get(&hm, "gamma");

    ASSERT_NE(got1, NULL);
    ASSERT_NE(got2, NULL);
    ASSERT_EQ(got3, NULL);

    ASSERT_EQ(*got1, 100);
    ASSERT_EQ(*got2, 200);

    int val3 = 300;
    ASSERT_EQ(hashmap_set(&hm, "alpha", &val3), 0);
    ASSERT_EQ(hashmap_count(&hm), 2);

    int *got_updated = (int *)hashmap_get(&hm, "alpha");
    ASSERT_NE(got_updated, NULL);
    ASSERT_EQ(*got_updated, 300);

    hashmap_destroy(&hm);
}

TEST(HashMapOperations, TypedMacros) {
    hashmap hm;
    hashmap_init(&hm);

    Player p1 = { .id = 1, .score = 99 };
    Player p_default = { .id = 0, .score = 0 };

    hashmap_set(&hm, "player1", &p1);

    Player *p_got = hashmap_get_type(&hm, "player1", Player);
    ASSERT_NE(p_got, NULL);
    ASSERT_EQ(p_got->id, 1);
    ASSERT_EQ(p_got->score, 99);

    Player *p_fallback = hashmap_get_or_type(&hm, "player2", Player, &p_default);
    ASSERT_NE(p_fallback, NULL);
    ASSERT_EQ(p_fallback->id, 0);
    ASSERT_EQ(p_fallback->score, 0);

    Player *p_exists = hashmap_get_or_type(&hm, "player1", Player, &p_default);
    ASSERT_NE(p_exists, NULL);
    ASSERT_EQ(p_exists->id, 1);
    ASSERT_EQ(p_exists->score, 99);

    hashmap_destroy(&hm);
}

TEST(HashMapOperations, RemoveAndTombstones) {
    hashmap hm;
    hashmap_init(&hm);

    int val1 = 10;
    int val2 = 20;
    int val3 = 30;

    hashmap_set(&hm, "k1", &val1);
    hashmap_set(&hm, "k2", &val2);
    hashmap_set(&hm, "k3", &val3);

    ASSERT_EQ(hashmap_count(&hm), 3);

    ASSERT_EQ(hashmap_remove(&hm, "k2"), 0);
    ASSERT_EQ(hashmap_count(&hm), 2);
    ASSERT_EQ(hashmap_has(&hm, "k2"), 0);
    ASSERT_EQ(hashmap_get(&hm, "k2"), NULL);

    ASSERT_EQ(hashmap_remove(&hm, "k2"), 1);
    ASSERT_EQ(hashmap_remove(&hm, "non_existent"), 1);

    int val2_new = 222;
    ASSERT_EQ(hashmap_set(&hm, "k2", &val2_new), 0);
    ASSERT_EQ(hashmap_count(&hm), 3);
    ASSERT_EQ(hashmap_has(&hm, "k2"), 1);

    int *got2 = (int *)hashmap_get(&hm, "k2");
    ASSERT_NE(got2, NULL);
    ASSERT_EQ(*got2, 222);

    hashmap_destroy(&hm);
}

TEST(HashMapRehash, LargeInsertions) {
    hashmap hm;
    hashmap_init_wcapacity(&hm, 16);

    int values[150];
    char keys[150][32];

    for (int i = 0; i < 150; i++) {
        values[i] = i * 10;
        snprintf(keys[i], sizeof(keys[i]), "key_%d", i);
        ASSERT_EQ(hashmap_set(&hm, keys[i], &values[i]), 0);
    }

    ASSERT_EQ(hashmap_count(&hm), 150);
    ASSERT_TRUE(hm.capacity >= 200);

    for (int i = 0; i < 150; i++) {
        ASSERT_EQ(hashmap_has(&hm, keys[i]), 1);
        int *val = (int *)hashmap_get(&hm, keys[i]);
        ASSERT_NE(val, NULL);
        ASSERT_EQ(*val, i * 10);
    }

    hashmap_destroy(&hm);
}

TEST(HashMapIteration, ForEachMacro) {
    hashmap hm;
    hashmap_init(&hm);

    int v1 = 1, v2 = 2, v3 = 3;
    hashmap_set(&hm, "one", &v1);
    hashmap_set(&hm, "two", &v2);
    hashmap_set(&hm, "three", &v3);

    int visited = 0;
    int sum = 0;

    const char *key = NULL;
    int *val = NULL;
    hashmap_for_each(&hm, it, key, val) {
        ASSERT_NE(key, NULL);
        ASSERT_NE(val, NULL);
        visited++;
        sum += *val;
    }

    ASSERT_EQ(visited, 3);
    ASSERT_EQ(sum, 6);

    hashmap_destroy(&hm);
}

TEST(HashMapIteration, CallbackForeach) {
    hashmap hm;
    hashmap_init(&hm);

    int dummy = 42;
    hashmap_set(&hm, "a", &dummy);
    hashmap_set(&hm, "b", &dummy);
    hashmap_set(&hm, "c", &dummy);
    hashmap_set(&hm, "d", &dummy);

    int count = 0;
    hashmap_foreach(&hm, count_callback, &count);
    ASSERT_EQ(count, 4);

    hashmap_destroy(&hm);
}

TEST(HashMapEdgeCases, NullAndSafety) {
    hashmap hm;
    hashmap_init(&hm);

    int val = 10;
    ASSERT_EQ(hashmap_set(NULL, "key", &val), 1);
    ASSERT_EQ(hashmap_set(&hm, NULL, &val), 1);
    ASSERT_EQ(hashmap_get(NULL, "key"), NULL);
    ASSERT_EQ(hashmap_get(&hm, NULL), NULL);
    ASSERT_EQ(hashmap_has(NULL, "key"), 0);
    ASSERT_EQ(hashmap_has(&hm, NULL), 0);
    ASSERT_EQ(hashmap_remove(NULL, "key"), 1);
    ASSERT_EQ(hashmap_remove(&hm, NULL), 1);

    hashmap_destroy(NULL);

    ASSERT_EQ(hashmap_next(NULL, NULL, NULL, NULL), 0);
    hashmap_foreach(NULL, count_callback, NULL);
    hashmap_foreach(&hm, NULL, NULL);

    hashmap_destroy(&hm);
}

int main(void) {
    return test_run();
}
