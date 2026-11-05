// vexspoke/test/objects/choice_test.c — the Choice class _test.
//
// Immutable branch dispatcher: option objects + callbacks; trigger routes to the
// option's callback. Proves length/getObject bounds, trigger dispatch, hostile
// index, and null-safety.

#include <stdint.h>
#include <stdio.h>

#include "objects/choice.h"

static int g_failures = 0;

#define CHECK(cond)                                                        \
    do {                                                                   \
        if (!(cond)) {                                                     \
            printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #cond);          \
            g_failures++;                                                  \
        }                                                                  \
    } while (0)

static uint64_t g_lastObject = 0;
static void *g_lastUserdata = (void*) 0;
static int g_fired = 0;

// Captures the object and caller context delivered by Choice_trigger.
static void cb(uint64_t objectPtr, void *userdata) {
    g_lastObject = objectPtr;
    g_lastUserdata = userdata;
    g_fired++;
}

// Checks Choice indexing, callback dispatch, invalid indices, and null safety.
int main(void) {
    uint64_t objects[3] = { 100, 200, 300 };
    ChoiceCallback callbacks[3] = { cb, cb, cb };

    Choice *c = Choice_3(objects, callbacks, 3);
    CHECK(c != nullptr);
    CHECK(Choice_length(c) == 3);
    CHECK(Choice_getObject(c, 0) == 100);
    CHECK(Choice_getObject(c, 2) == 300);
    CHECK(Choice_getObject(c, 3) == 0);   // out of range
    CHECK(Choice_getObject(c, SIZE_MAX) == 0);

    int token = 7;
    Choice_trigger(c, 1, &token);
    CHECK(g_fired == 1);
    CHECK(g_lastObject == 200);
    CHECK(g_lastUserdata == &token);

    Choice_trigger(c, 9, nullptr);        // out of range: no fire
    Choice_trigger(c, SIZE_MAX, nullptr);
    CHECK(g_fired == 1);

    // Null-safety.
    CHECK(Choice_length(nullptr) == 0);
    CHECK(Choice_getObject(nullptr, 0) == 0);
    Choice_trigger(nullptr, 0, nullptr);
    CHECK(g_fired == 1);
    Choice_free(nullptr);

    Choice_free(c);

    if (g_failures == 0) {
        printf("choice_test: all assertions held\n");
        return 0;
    }
    printf("choice_test: %d FAILURES\n", g_failures);
    return 1;
}
