#include <stdint.h>
#include <stddef.h>

#define LIBTEST_IMPLEMENTATION
#include "../../libtest/standalone_lib/libtest.h"

#define LIBARENA_IMPLEMENTATION
#include "../standalone_lib/libarena.h"


TEST(Lifecycle, InitSuccess) {
    Arena a;
    int res = arena_init(&a, ARENA_KB(64));
    
    ASSERT_EQ(res, 0);
    ASSERT_NE((uintptr_t)a.buffer, (uintptr_t)NULL);
    ASSERT_EQ(arena_capacity(&a), ARENA_KB(64));
    ASSERT_EQ(arena_used_bytes(&a), 0);
    ASSERT_EQ(arena_free_bytes(&a), ARENA_KB(64));

    arena_free(&a);
    ASSERT_EQ((uintptr_t)a.buffer, (uintptr_t)NULL);
    ASSERT_EQ(arena_capacity(&a), 0);
    ASSERT_EQ(arena_used_bytes(&a), 0);
}

TEST(Lifecycle, InitInvalidParams) {
    Arena a;
    int res = arena_init(&a, 0);
    EXPECT_NE(res, 0);

    res = arena_init(NULL, ARENA_KB(4));
    EXPECT_NE(res, 0);
}

TEST(Lifecycle, InitWithBuffer) {
    uint8_t stack_mem[256];
    Arena a;
    
    int res = arena_init_with_buffer(&a, stack_mem, sizeof(stack_mem));
    ASSERT_EQ(res, 0);
    ASSERT_EQ((uintptr_t)a.buffer, (uintptr_t)stack_mem);
    ASSERT_EQ(arena_capacity(&a), 256);
    ASSERT_EQ(a.can_free, 0);

    void *p = arena_alloc(&a, 64);
    ASSERT_NE((uintptr_t)p, (uintptr_t)NULL);
    ASSERT_EQ(arena_used_bytes(&a), 64);

    arena_free(&a);
    ASSERT_EQ((uintptr_t)a.buffer, (uintptr_t)NULL);
}

TEST(Lifecycle, InitWithBufferNull) {
    Arena a;
    int res = arena_init_with_buffer(&a, NULL, 128);
    EXPECT_NE(res, 0);

    uint8_t buf[16];
    res = arena_init_with_buffer(NULL, buf, 16);
    EXPECT_NE(res, 0);
}


TEST(Allocation, BasicAlloc) {
    Arena a;
    arena_init(&a, ARENA_KB(4));

    void *p1 = arena_alloc(&a, 32);
    ASSERT_NE((uintptr_t)p1, (uintptr_t)NULL);
    ASSERT_EQ(arena_used_bytes(&a), 32);

    void *p2 = arena_alloc(&a, 16);
    ASSERT_NE((uintptr_t)p2, (uintptr_t)NULL);
    ASSERT_EQ(arena_used_bytes(&a), 48);

    ASSERT_TRUE((uintptr_t)p2 >= (uintptr_t)p1 + 32);

    arena_free(&a);
}

TEST(Allocation, ZeroAlloc) {
    Arena a;
    arena_init(&a, ARENA_KB(4));

    size_t size = 128;
    uint8_t *p = (uint8_t *)arena_alloc_zero(&a, size);
    ASSERT_NE((uintptr_t)p, (uintptr_t)NULL);

    bool all_zero = true;
    for (size_t i = 0; i < size; i++) {
        if (p[i] != 0) {
            all_zero = false;
            break;
        }
    }
    ASSERT_TRUE(all_zero);

    arena_free(&a);
}

TEST(Allocation, CustomAlignment) {
    Arena a;
    arena_init(&a, ARENA_KB(4));

    void *p1 = arena_alloc_aligned(&a, 1, 1);
    ASSERT_NE((uintptr_t)p1, (uintptr_t)NULL);

    void *p64 = arena_alloc_aligned(&a, 128, 64);
    ASSERT_NE((uintptr_t)p64, (uintptr_t)NULL);
    ASSERT_EQ((uintptr_t)p64 % 64, 0);

    void *p32 = arena_alloc_aligned(&a, 16, 32);
    ASSERT_NE((uintptr_t)p32, (uintptr_t)NULL);
    ASSERT_EQ((uintptr_t)p32 % 32, 0);

    arena_free(&a);
}

TEST(Allocation, InvalidAlignmentOrSize) {
    Arena a;
    arena_init(&a, ARENA_KB(4));

    void *p = arena_alloc(&a, 0);
    EXPECT_EQ((uintptr_t)p, (uintptr_t)NULL);

    p = arena_alloc_aligned(&a, 16, 0);
    EXPECT_EQ((uintptr_t)p, (uintptr_t)NULL);

    p = arena_alloc_aligned(&a, 16, 3);
    EXPECT_EQ((uintptr_t)p, (uintptr_t)NULL);

    p = arena_alloc_aligned(&a, 16, 15);
    EXPECT_EQ((uintptr_t)p, (uintptr_t)NULL);

    arena_free(&a);
}

TEST(Allocation, OutOfMemory) {
    Arena a;
    arena_init(&a, 100);

    void *p1 = arena_alloc(&a, 60);
    ASSERT_NE((uintptr_t)p1, (uintptr_t)NULL);

    void *p2 = arena_alloc(&a, 50);
    EXPECT_EQ((uintptr_t)p2, (uintptr_t)NULL);

    ASSERT_EQ(arena_used_bytes(&a), 60);

    arena_free(&a);
}


typedef struct {
    int id;
    double val;
    char tag[16];
} Entity;

TEST(Macros, AllocType) {
    Arena a;
    arena_init(&a, ARENA_KB(4));

    Entity *e = arena_alloc_type(&a, Entity);
    ASSERT_NE((uintptr_t)e, (uintptr_t)NULL);
    
    e->id = 42;
    e->val = 3.14159;
    EXPECT_EQ(e->id, 42);
    EXPECT_FLOAT_EQ(e->val, 3.14159, 0.0001);

    arena_free(&a);
}

TEST(Macros, AllocBuffer) {
    Arena a;
    arena_init(&a, ARENA_KB(4));

    int *arr = arena_alloc_buffer(&a, int, 10);
    ASSERT_NE((uintptr_t)arr, (uintptr_t)NULL);

    for (int i = 0; i < 10; i++) {
        arr[i] = i * i;
    }
    EXPECT_EQ(arr[9], 81);

    int *overflow = arena_alloc_buffer(&a, int, SIZE_MAX);
    EXPECT_EQ((uintptr_t)overflow, (uintptr_t)NULL);

    arena_free(&a);
}


TEST(StateManagement, Reset) {
    Arena a;
    arena_init(&a, ARENA_KB(4));

    arena_alloc(&a, 200);
    ASSERT_EQ(arena_used_bytes(&a), 200);

    arena_reset(&a);
    ASSERT_EQ(arena_used_bytes(&a), 0);
    ASSERT_EQ(arena_free_bytes(&a), ARENA_KB(4));

    void *p = arena_alloc(&a, 50);
    ASSERT_NE((uintptr_t)p, (uintptr_t)NULL);
    ASSERT_EQ(arena_used_bytes(&a), 50);

    arena_free(&a);
}

TEST(StateManagement, SaveRestore) {
    Arena a;
    arena_init(&a, ARENA_KB(4));

    arena_alloc(&a, 96);
    ArenaMarker mark = arena_save(&a);
    ASSERT_EQ(mark, 96);

    arena_alloc(&a, 256);
    ASSERT_EQ(arena_used_bytes(&a), 352);

    int res = arena_restore(&a, mark);
    ASSERT_EQ(res, 0);
    ASSERT_EQ(arena_used_bytes(&a), 96);

    res = arena_restore(&a, 9999);
    EXPECT_NE(res, 0);

    arena_free(&a);
}

TEST(StateManagement, TempArenaScope) {
    Arena a;
    arena_init(&a, ARENA_KB(4));

    arena_alloc(&a, 64);
    size_t initial_used = arena_used_bytes(&a);

    TempArena temp = arena_temp_begin(&a);
    {
        void *tmp1 = arena_alloc(&a, 128);
        void *tmp2 = arena_dup_str(&a, "Test di stringa temporanea");
        ASSERT_NE((uintptr_t)tmp1, (uintptr_t)NULL);
        ASSERT_NE((uintptr_t)tmp2, (uintptr_t)NULL);
        ASSERT_TRUE(arena_used_bytes(&a) > initial_used);
    }
    arena_temp_end(temp);

    ASSERT_EQ(arena_used_bytes(&a), initial_used);

    arena_free(&a);
}


TEST(Duplication, DupString) {
    Arena a;
    arena_init(&a, ARENA_KB(4));

    const char *orig = "Hello, LibArena!";
    char *copy = arena_dup_str(&a, orig);

    ASSERT_NE((uintptr_t)copy, (uintptr_t)NULL);
    ASSERT_STR_EQ(copy, orig);
    
    ASSERT_NE((uintptr_t)copy, (uintptr_t)orig);
    ASSERT_EQ(copy[strlen(orig)], '\0'); 
    char *null_copy = arena_dup_str(&a, NULL);
    EXPECT_EQ((uintptr_t)null_copy, (uintptr_t)NULL);

    arena_free(&a);
}

TEST(Duplication, DupData) {
    Arena a;
    arena_init(&a, ARENA_KB(4));

    uint32_t raw_data[4] = {10, 20, 30, 40};
    uint32_t *copy = (uint32_t *)arena_dup_data(&a, raw_data, sizeof(raw_data));

    ASSERT_NE((uintptr_t)copy, (uintptr_t)NULL);
    ASSERT_NE((uintptr_t)copy, (uintptr_t)raw_data);

    for (int i = 0; i < 4; i++) {
        EXPECT_EQ(copy[i], raw_data[i]);
    }

    arena_free(&a);
}


int main(void) {
    return test_run();
}
