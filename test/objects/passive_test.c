// vexspoke/test/objects/passive_test.c — the Passive class _test.
//
// Lazy getter/setter hooks: reads and writes route through the injected hooks,
// and a null hook degrades to 0 / no-op.

#include <stdio.h>

#include "objects/passive.h"

static int g_failures = 0;

#define CHECK(cond)                                                        \
    do {                                                                   \
        if (!(cond)) {                                                     \
            printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #cond);          \
            g_failures++;                                                  \
        }                                                                  \
    } while (0)

static uint64_t g_store = 0;
// Supplies the external value used by the Passive getter hook.
static uint64_t storeGet(void *userdata) {
    (void) userdata;
    return g_store;
}
// Updates the external value used by the Passive setter hook.
static void storeSet(uint64_t value, void *userdata) {
    (void) userdata;
    g_store = value;
}

// Checks hooked and cached Passive access, including absent hooks and nulls.
int main(void) {
    int token = 1;
    Passive *p = Passive_3(storeGet, storeSet, &token);
    CHECK(p != nullptr);

    g_store = 11;
    CHECK(Passive_get(p) == 11);
    Passive_set(p, 22);
    CHECK(g_store == 22);
    CHECK(Passive_get(p) == 22);

    Passive_free(p);

    // Null hooks: get returns the cache; set caches (Passive always caches).
    Passive *q = Passive_3(nullptr, nullptr, nullptr);
    CHECK(q != nullptr);
    CHECK(Passive_get(q) == 0);
    Passive_set(q, 5);
    CHECK(Passive_get(q) == 5);
    Passive_free(q);

    // Null-safety.
    CHECK(Passive_get(nullptr) == 0);
    Passive_set(nullptr, 1);
    Passive_free(nullptr);

    if (g_failures == 0) {
        printf("passive_test: all assertions held\n");
        return 0;
    }
    printf("passive_test: %d FAILURES\n", g_failures);
    return 1;
}
