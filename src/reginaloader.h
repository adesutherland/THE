/* Included after Regina's rexxsaa.h. Preserve its platform calling convention. */
#include "macroruntime.h"
#define THE_REGINA_FUNCTIONS(X) \
 X(RexxStart,APIRET,(LONG,PRXSTRING,PCSZ,PRXSTRING,PCSZ,LONG,PRXSYSEXIT,PSHORT,PRXSTRING)) \
 X(RexxRegisterSubcomExe,APIRET,(PCSZ,RexxSubcomHandler *,PUCHAR)) \
 X(RexxDeregisterSubcom,APIRET,(PCSZ,PCSZ)) \
 X(RexxRegisterExitExe,APIRET,(PCSZ,RexxExitHandler *,PUCHAR)) \
 X(RexxDeregisterExit,APIRET,(PCSZ,PCSZ)) \
 X(RexxRegisterFunctionExe,APIRET,(PCSZ,RexxFunctionHandler *)) \
 X(RexxDeregisterFunction,APIRET,(PCSZ)) \
 X(RexxVariablePool,APIRET,(PSHVBLOCK)) \
 X(RexxQueryQueue,ULONG,(PSZ,ULONG *)) \
 X(RexxAddQueue,ULONG,(PSZ,PRXSTRING,ULONG)) \
 X(RexxSetHalt,APIRET,(LONG,LONG)) \
 X(RexxAllocateMemory,PVOID,(ULONG)) \
 X(RexxFreeMemory,APIRET,(PVOID))
#define DECLARE_REGINA(name,result,args) static result (APIENTRY *the_##name) args;
THE_REGINA_FUNCTIONS(DECLARE_REGINA)
#undef DECLARE_REGINA

static int load_regina_runtime(void)
{
   static void *library;
   const char *path=getenv("THE_REGINA_LIBRARY");
   char error[1024];
   if (library) return 1;
   if (!path || !*path) path=THE_REGINA_LIBRARY;
   library=the_macro_library_open(path,error,sizeof(error));
   if (!library)
   {
      fprintf(stderr,"THE: Unable to load Regina: %s\nSet THE_REGINA_LIBRARY to its full library path.\n",error);
      return 0;
   }
#define LOAD_REGINA(name,result,args) \
   the_##name=(result (APIENTRY *) args)the_macro_library_symbol(library,#name); \
   if (!the_##name) { snprintf(error,sizeof(error),"missing %s",#name); goto failed; }
   THE_REGINA_FUNCTIONS(LOAD_REGINA)
#undef LOAD_REGINA
   return 1;
failed:
   fprintf(stderr,"THE: Invalid Regina library %s: %s\n",path,error);
   the_macro_library_close(library);
   library=NULL;
   return 0;
}

#define RexxStart the_RexxStart
#define RexxRegisterSubcomExe the_RexxRegisterSubcomExe
#define RexxDeregisterSubcom the_RexxDeregisterSubcom
#define RexxRegisterExitExe the_RexxRegisterExitExe
#define RexxDeregisterExit the_RexxDeregisterExit
#define RexxRegisterFunctionExe the_RexxRegisterFunctionExe
#define RexxDeregisterFunction the_RexxDeregisterFunction
#define RexxVariablePool the_RexxVariablePool
#define RexxQueryQueue the_RexxQueryQueue
#define RexxAddQueue the_RexxAddQueue
#define RexxSetHalt the_RexxSetHalt
#define RexxAllocateMemory the_RexxAllocateMemory
#define RexxFreeMemory the_RexxFreeMemory
