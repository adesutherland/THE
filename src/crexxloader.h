#ifdef THE_CREXX_STATIC
static int load_crexx_runtime(void) { return 1; }
#else
/* Included after crexxsaa.h; no direct calls/imports to the runtime DLL. */
#include "macroruntime.h"
#include "crexxapi.h"
#define DECLARE_CREXX(name,result,args) static result (*the_##name) args;
THE_CREXX_FUNCTIONS(DECLARE_CREXX)
#undef DECLARE_CREXX

static int load_crexx_runtime(void)
{
   static void *library;
   const char *path=getenv("THE_CREXX_LIBRARY");
   char error[1024];
   if (library) return 1;
   if (!path || !*path) path=THE_CREXX_SHARED_LIBRARY;
   library=the_macro_library_open(path,error,sizeof(error));
   if (!library)
   {
      fprintf(stderr,"THE: Unable to load cREXX: %s\nSet THE_CREXX_LIBRARY to its full library path.\n",error);
      return 0;
   }
#define LOAD_CREXX(name,result,args) \
   the_##name=(result (*) args)the_macro_library_symbol(library,#name); \
   if (!the_##name) { snprintf(error,sizeof(error),"missing %s",#name); goto failed; }
   THE_CREXX_FUNCTIONS(LOAD_CREXX)
#undef LOAD_CREXX
   return 1;
failed:
   fprintf(stderr,"THE: Invalid cREXX library %s: %s\n",path,error);
   the_macro_library_close(library);
   library=NULL;
   return 0;
}

#define crexxsaa_create the_crexxsaa_create
#define crexxsaa_destroy the_crexxsaa_destroy
#define crexxsaa_set_compiler the_crexxsaa_set_compiler
#define crexxsaa_set_cache_dir the_crexxsaa_set_cache_dir
#define crexxsaa_register_address_environment the_crexxsaa_register_address_environment
#define crexxsaa_set_address_environment the_crexxsaa_set_address_environment
#define crexxsaa_run_rxbin the_crexxsaa_run_rxbin
#define crexxsaa_run_source the_crexxsaa_run_source
#define crexxsaa_last_error the_crexxsaa_last_error
#define crexxsaa_address_variable_set the_crexxsaa_address_variable_set
#define crexxsaa_address_variable_get_alloc the_crexxsaa_address_variable_get_alloc
#define crexxsaa_free the_crexxsaa_free

#endif
