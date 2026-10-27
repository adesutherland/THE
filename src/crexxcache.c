#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "crexxcache.h"
#ifdef _WIN32
#include <windows.h>
#endif

static const char *value(const char *name)
{
    const char *s=getenv(name);
    return s && *s ? s : NULL;
}

char *the_crexx_cache_directory(void)
{
    const char *base=value("CREXXSAA_CACHE_DIR"), *suffix="";
    char *result;
    size_t size;
    if (!base) base=value("THE_CREXX_CACHE_DIR");
    if (!base) {
#ifdef _WIN32
        base=value("LOCALAPPDATA");
        suffix="/THE-cREXX/Cache/crexxsaa";
        if (!base) { base=value("USERPROFILE"); suffix="/AppData/Local/THE-cREXX/Cache/crexxsaa"; }
#elif defined(__APPLE__)
        base=value("HOME"); suffix="/Library/Caches/org.crexx.the/crexxsaa";
#else
        base=value("XDG_CACHE_HOME"); suffix="/the/crexxsaa";
        if (!base) { base=value("HOME"); suffix="/.cache/the/crexxsaa"; }
#endif
    }
    if (!base) return NULL;
    size=strlen(base)+strlen(suffix)+1;
    result=malloc(size);
    if (result) snprintf(result,size,"%s%s",base,suffix);
    return result;
}

int the_crexx_configure_cache(void)
{
    char *directory=the_crexx_cache_directory();
    int rc=0;
    if (!directory) return -1;
    /* Child cREXX processes, including F9, inherit the same private default. */
    if (!value("CREXXSAA_CACHE_DIR")) {
#ifdef _WIN32
        rc=_putenv_s("CREXXSAA_CACHE_DIR",directory);
#else
        rc=setenv("CREXXSAA_CACHE_DIR",directory,1);
#endif
    }
    free(directory);
    return rc;
}
