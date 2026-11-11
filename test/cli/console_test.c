// Bare R2 Console owner (registered as bare_console_test, not R1 Console).
// Compatibility lifecycle is no-op; literal control escaping produces one
// stdout line per call and consumes owned String exactly once. No terminal UI.
#include "cli/console.h"
#include "primitive/vex-string.h"
#include "nio/mem.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
int main(void) {
    FILE *out = tmpfile(), *errors = tmpfile();
    assert(out && errors);
    fflush(stdout); fflush(stderr);
    int savedOut = dup(STDOUT_FILENO), savedErr = dup(STDERR_FILENO);
    assert(savedOut >= 0 && savedErr >= 0);
    assert(dup2(fileno(out), STDOUT_FILENO) >= 0);
    assert(dup2(fileno(errors), STDERR_FILENO) >= 0);
    Console_init(); Console_init();
    Console_log("hello");
    Console_log("");
    Console_log("a\nb\r\x1b\t");
    Console_logString(String("owned"));
    Console_drain(); Console_shutdown(); Console_shutdown();
    fflush(stdout); fflush(stderr);
    rewind(out); rewind(errors);
    char text[256] = {0};
    assert(fread(text, 1, sizeof(text) - 1, out) != 0);
    assert(strcmp(text, "hello\n\na\\x0Ab\\x0D\\x1B\\x09\nowned\n") == 0);
    assert(fgetc(errors) == EOF);
    Console_log(nullptr);
    fflush(stderr);
    rewind(errors);
    memset(text, 0, sizeof(text));
    assert(fread(text, 1, sizeof(text) - 1, errors) != 0);
    assert(strstr(text, "[vex] ") && strstr(text, "null message"));
    bytes lines = 0;
    for (bytes i = 0; text[i] != 0; ++i)
        if (text[i] == '\n')
            ++lines;
    assert(lines == 1);
    assert(dup2(savedOut, STDOUT_FILENO) >= 0 && dup2(savedErr, STDERR_FILENO) >= 0);
    close(savedOut); close(savedErr); fclose(out); fclose(errors);
    Memory_freeAll();
    return 0;
}
