// vexspoke/test/c23/fn_test.c — callback and captured-context value contract.

#include <stdio.h>

#include "c23/fn.h"

static int g_failures = 0;

#define CHECK(cond)                                                        \
    do {                                                                   \
        if (!(cond)) {                                                     \
            printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #cond);          \
            g_failures++;                                                  \
        }                                                                  \
    } while (0)

static any addStep(any context, any input) {
    int *step = context;
    int *value = input;
    *value += *step;
    return value;
}

static any subtractStep(any context, any input) {
    int *step = context;
    int *value = input;
    *value -= *step;
    return value;
}

int main(void) {
    Function empty = Function();
    CHECK(Function_getCall(&empty) == nullptr);
    CHECK(Function_getContext(&empty) == nullptr);
    CHECK(Function_run(&empty, nullptr) == nullptr);

    int add = 1;
    int subtract = 2;
    int value = 10;
    Function addOne = Function(addStep);
    CHECK(Function_getCall(&addOne) == addStep);
    CHECK(Function_setContext(&addOne, &add));
    CHECK(Function_run(&addOne, &value) == &value);
    CHECK(value == 11);

    Function subtractTwo = Function(subtractStep, &subtract);
    CHECK(Function_getCall(&subtractTwo) == subtractStep);
    CHECK(Function_getContext(&subtractTwo) == &subtract);
    CHECK(Function_run(&subtractTwo, &value) == &value);
    CHECK(value == 9);

    Function bound = Function_bind(&addOne, &subtract);
    CHECK(Function_getContext(&bound) == &subtract);
    CHECK(Function_getContext(&addOne) == &add);
    CHECK(Function_run(&bound, &value) == &value);
    CHECK(value == 11);

    CHECK(!Function_setCall(nullptr, addStep));
    CHECK(!Function_setContext(nullptr, &add));
    CHECK(Function_getCall(nullptr) == nullptr);
    CHECK(Function_getContext(nullptr) == nullptr);
    CHECK(Function_run(nullptr, &value) == nullptr);
    Function failedBind = Function_bind(nullptr, &add);
    CHECK(Function_getCall(&failedBind) == nullptr);

    if (g_failures == 0) {
        printf("fn_test: all assertions held\n");
        return 0;
    }
    printf("fn_test: %d FAILURES\n", g_failures);
    return 1;
}
