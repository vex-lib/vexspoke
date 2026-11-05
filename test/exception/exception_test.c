// vexspoke/test/exception/exception_test.c — owner test for Exception.
//
// Proves constructor arities, formatted message ownership, detail replacement,
// and idempotent release of owned diagnostic strings.

#include <stdio.h>
#include <string.h>

#include "exception/exception.h"

static int g_failures = 0;

#define CHECK(cond)                                                        \
    do {                                                                   \
        if (!(cond)) {                                                     \
            printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #cond);          \
            g_failures++;                                                  \
        }                                                                  \
    } while (0)

int main(void) {
    Exception empty = Exception();
    CHECK(empty.type == TYPE_EXCEPTION_SINGLETON);
    CHECK(empty.message == nullptr);
    CHECK(empty.details == nullptr);
    Exception_free(&empty);

    Exception literal = Exception(EXCEPTION_RUNTIME, "open-document", __FILE__,
                                  __LINE__, "missing document");
    CHECK(literal.category == EXCEPTION_RUNTIME);
    CHECK(strcmp(literal.message, "missing document") == 0);

    Exception formatted = Exception(EXCEPTION_IO, "read-document", __FILE__,
                                    __LINE__, "could not read %s (%d)",
                                    "document-path-goes-here", 3);
    CHECK(formatted.category == EXCEPTION_IO);
    CHECK(strcmp(formatted.message,
        "could not read document-path-goes-here (3)") == 0);
    Exception_setDetails(&formatted, "attempt=%d", 1);
    CHECK(strcmp(formatted.details, "attempt=1") == 0);
    Exception_setDetails(&formatted, "retryable=%s", "true");
    CHECK(strcmp(formatted.details, "retryable=true") == 0);
    Exception_free(&formatted);
    CHECK(formatted.message == nullptr);
    CHECK(formatted.details == nullptr);
    Exception_free(&formatted);
    Exception_free(&literal);

    if (g_failures == 0) {
        printf("exception_test: all assertions held\n");
        return 0;
    }
    printf("exception_test: %d FAILURES\n", g_failures);
    return 1;
}
