/* Runtime selection does not imply a link-time dependency on either engine. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "macroruntime.h"
#ifdef _WIN32
#include <windows.h>
#else
#include <dlfcn.h>
#endif

void *the_macro_library_open(const char *path,char *error,size_t size)
{
#ifdef _WIN32
   /* A full runtime path also locates its companion DLLs in that directory.
    * Preserve normal loader search semantics for names and relative paths. */
   int absolute=(isalpha((unsigned char)path[0]) && path[1]==':'
      && (path[2]=='/' || path[2]=='\\'))
      || ((path[0]=='/' || path[0]=='\\')
      && (path[1]=='/' || path[1]=='\\'));
   void *library=absolute
      ? (void *)LoadLibraryExA(path,NULL,LOAD_WITH_ALTERED_SEARCH_PATH)
      : (void *)LoadLibraryA(path);
   if (!library)
      snprintf(error,size,"%s (Windows loader error %lu)",path,(unsigned long)GetLastError());
#else
   void *library=dlopen(path,RTLD_NOW|RTLD_LOCAL);
   if (!library)
      snprintf(error,size,"%s",dlerror());
#endif
   return library;
}

void *the_macro_library_symbol(void *library,const char *name)
{
#ifdef _WIN32
   return (void *)GetProcAddress((HMODULE)library,name);
#else
   return dlsym(library,name);
#endif
}

void the_macro_library_close(void *library)
{
#ifdef _WIN32
   FreeLibrary((HMODULE)library);
#else
   dlclose(library);
#endif
}

const char *the_macro_interpreter(void)
{
   static char selected[16];
   const char *configured;
   size_t i,len;
   if (selected[0]) return selected;
   configured=getenv("THE_REXX_INTERPRETER");
   if (!configured || !*configured) configured=THE_DEFAULT_REXX_INTERPRETER;
   len=strlen(configured);
   if (len>=sizeof(selected)) return "INVALID";
   for (i=0;i<=len;i++) selected[i]=(char)toupper((unsigned char)configured[i]);
   return selected;
}

int the_macro_uses_regina(void) { return strcmp(the_macro_interpreter(),"REGINA")==0; }
int the_macro_uses_crexx(void) { return strcmp(the_macro_interpreter(),"CREXX")==0; }

int the_macro_uses_oorexx(void) { return strcmp(the_macro_interpreter(),"OOREXX")==0; }
