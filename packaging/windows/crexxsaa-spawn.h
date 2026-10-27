/* SPDX-License-Identifier: MIT
 * Copyright (c) 2026 Adrian Sutherland
 * Packaging correction for cREXX ae1607b8: _spawnvp does not
 * quote argv for the child's CRT parser. Keep empty arguments, quotes and
 * trailing backslashes intact, without routing anything through a shell.
 * https://learn.microsoft.com/en-us/cpp/c-runtime-library/spawn-wspawn-functions
 */
#ifndef THE_CREXXSAA_SPAWN_H
#define THE_CREXXSAA_SPAWN_H
#include <errno.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#include <process.h>
#endif

static char *the_windows_quote_argument(const char *argument)
{
    size_t length = strlen(argument), slashes = 0, count;
    const char *input;
    char *output, *next;
    if (length > (SIZE_MAX - 3) / 2) { errno = ENOMEM; return NULL; }
    output = malloc(length * 2 + 3);
    if (!output) return NULL;
    next = output;
    *next++ = '"';
    for (input = argument; *input; ++input) {
        if (*input == '\\') { ++slashes; continue; }
        count = *input == '"' ? slashes * 2 + 1 : slashes;
        while (count--) *next++ = '\\';
        *next++ = *input;
        slashes = 0;
    }
    count = slashes * 2;
    while (count--) *next++ = '\\';
    *next++ = '"';
    *next = '\0';
    return output;
}

#ifdef _WIN32
static intptr_t the_crexx_spawnvp(const char *executable, const char *const argv[])
{
    size_t argc = 0, i;
    char **quoted;
    intptr_t result = -1;
    int saved_errno;
    while (argv[argc]) ++argc;
    quoted = calloc(argc + 1, sizeof(*quoted));
    if (!quoted) return -1;
    for (i = 0; i < argc; ++i) {
        quoted[i] = the_windows_quote_argument(argv[i]);
        if (!quoted[i]) goto done;
    }
    result = _spawnvp(_P_WAIT, executable, (const char *const *)quoted);
done:
    saved_errno = errno;
    for (i = 0; i < argc; ++i) free(quoted[i]);
    free(quoted);
    errno = saved_errno;
    return result;
}
#endif
#endif
