/* Exercise the child's real CRT argv parser, including an empty argument. */
#include <stdio.h>
#include "../packaging/windows/crexxsaa-spawn.h"

int main(int argc, char **argv)
{
    const char *values[] = { "plain", "space value", "", "a\"b",
        "C:\\folder with space\\", "one\\\"two", "literal&percent%caret^", NULL };
    const char *child[10];
    size_t i;
    if (argc > 1 && !strcmp(argv[1], "--child")) {
        if (argc != 9) return 1;
        for (i = 0; values[i]; ++i)
            if (strcmp(argv[i + 2], values[i])) {
                fprintf(stderr, "Argument %zu changed: [%s]\n", i, argv[i + 2]);
                return 2;
            }
        return 0;
    }
    child[0] = argv[0];
    child[1] = "--child";
    for (i = 0; values[i]; ++i) child[i + 2] = values[i];
    child[i + 2] = NULL;
    if (the_crexx_spawnvp(argv[0], child) != 0) return 3;
    puts("Windows compiler argv round trip passed");
    return 0;
}
