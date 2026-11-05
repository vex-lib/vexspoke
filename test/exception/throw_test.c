// Recoverable THROW reports a detected cold failure and returns to the caller.
// Ordinary paths must produce no diagnostic. This POSIX test is wired in both
// Debug and Release; CHECK never disappears under NDEBUG.

#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "exception/throw.h"

static int failures = 0;

#define CHECK(condition) do { \
    if (!(condition)) { \
        fprintf(stdout, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #condition); \
        failures++; \
    } \
} while (0)

static bool getChecked(bool valid) {
    if (valid)
        return true;
    THROW("throw_test: rejected input");
    return false;
}

int main(void) {
    FILE *capture = tmpfile();
    if (capture == nullptr)
        return 1;
    int original = dup(fileno(stderr));
    if (original < 0 || dup2(fileno(capture), fileno(stderr)) < 0) {
        if (original >= 0)
            close(original);
        fclose(capture);
        return 1;
    }

    CHECK(getChecked(true));
    fflush(stderr);
    CHECK(ftell(capture) == 0);
    CHECK(!getChecked(false));
    fflush(stderr);

    CHECK(dup2(original, fileno(stderr)) >= 0);
    close(original);
    rewind(capture);
    char line[512] = {0};
    CHECK(fgets(line, sizeof(line), capture) != nullptr);
    CHECK(strstr(line, "[vex] throw_test.c:") != nullptr);
    CHECK(strstr(line, "throw_test: rejected input") != nullptr);
    CHECK(fgets(line, sizeof(line), capture) == nullptr);
    fclose(capture);

    if (failures != 0)
        return 1;
    puts("throw_test: normal and rejection paths verified");
    return 0;
}
