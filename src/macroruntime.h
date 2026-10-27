/* Optional macro runtimes. Handles remain loaded for the editor's lifetime. */
#ifndef THE_MACRORUNTIME_H
#define THE_MACRORUNTIME_H
#include <stddef.h>
void *the_macro_library_open(const char *path,char *error,size_t error_size);
void *the_macro_library_symbol(void *library,const char *name);
void the_macro_library_close(void *library);
const char *the_macro_interpreter(void);
int the_macro_uses_regina(void);
int the_macro_uses_crexx(void);
#endif
