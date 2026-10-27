/* Emscripten's virtual POSIX filesystem supplies cREXX's file lookup. The
 * upstream platform selector currently enumerates Linux/macOS for this query. */
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include "platform.h"

static int candidate(const char *name, const char *type, const char *directory, size_t directory_len)
{
    char path[MAXFILEPATH];
    size_t n = strlen(name), t = strlen(type);
    int has_type = !t || (n > t && name[n-t-1] == '.' && strcmp(name+n-t,type) == 0);
    int length = snprintf(path,sizeof(path),"%.*s%s%s%s%s",(int)directory_len,directory,
        directory_len ? "/" : "",name,has_type ? "" : ".",has_type ? "" : type);
    return length >= 0 && (size_t)length < sizeof(path) && access(path,F_OK) == 0;
}

int __wrap_fileexists(char *name, char *type, char *directories)
{
    const char *start, *end;
    if (!name || !type) return 0;
    if (strchr(name,'/') || strchr(name,'\\') || !directories || !*directories)
        return candidate(name,type,"",0);
    start = directories;
    do {
        end = strchr(start,';');
        size_t length = end ? (size_t)(end-start) : strlen(start);
        if (length && length < MAXFILEPATH && candidate(name,type,start,length)) return 1;
        start = end ? end+1 : NULL;
    } while (start);
    return 0;
}
