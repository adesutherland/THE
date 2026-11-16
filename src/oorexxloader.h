/* Included only after ooRexx's rexx.h. Its PFN typedefs retain the SDK's
 * pointer widths and REXXENTRY calling convention, including Win32 stdcall. */
#include "macroruntime.h"
/* Some SDK PFN typedefs spell the optional user area char * while the
 * function declarations use CONSTANT_STRING. Match the declarations. */
typedef RexxReturnCode (REXXENTRY *THE_PFN_OOREXX_REGISTER)(CONSTANT_STRING,REXXPFN,CONSTANT_STRING);
#define THE_OOREXX_INTERPRETER_FUNCTIONS(X) \
 X(RexxStart,PFNREXXSTART) \
 X(RexxVariablePool,PFNREXXVARIABLEPOOL) \
 X(RexxSetHalt,PFNREXXSETHALT)
#define THE_OOREXX_API_FUNCTIONS(X) \
 X(RexxRegisterSubcomExe,THE_PFN_OOREXX_REGISTER) \
 X(RexxDeregisterSubcom,PFNREXXDEREGISTERSUBCOM) \
 X(RexxRegisterExitExe,THE_PFN_OOREXX_REGISTER) \
 X(RexxDeregisterExit,PFNREXXDEREGISTEREXIT) \
 X(RexxRegisterFunctionExe,PFNREXXREGISTERFUNCTIONEXE) \
 X(RexxDeregisterFunction,PFNREXXDEREGISTERFUNCTION) \
 X(RexxQueryQueue,PFNREXXQUERYQUEUE) \
 X(RexxAddQueue,PFNREXXADDQUEUE) \
 X(RexxAllocateMemory,PFNREXXALLOCATEMEMORY) \
 X(RexxFreeMemory,PFNREXXFREEMEMORY)
#define DECLARE_OOREXX(name,type) static type the_##name;
THE_OOREXX_INTERPRETER_FUNCTIONS(DECLARE_OOREXX)
THE_OOREXX_API_FUNCTIONS(DECLARE_OOREXX)
#undef DECLARE_OOREXX

static int load_oorexx_runtime(void)
{
   static void *library,*api_library;
   const char *path=getenv("THE_OOREXX_LIBRARY");
   const char *api_path=getenv("THE_OOREXX_API_LIBRARY");
   const char *slash,*backslash;
   char error[1024],companion[4096];
   size_t directory;
   if (library && api_library) return 1;
   if (!path || !*path) path=THE_OOREXX_LIBRARY;
   if (!api_path || !*api_path)
   {
      slash=strrchr(path,'/');
      backslash=strrchr(path,'\\');
      if (backslash && (!slash || backslash > slash)) slash=backslash;
      directory=slash ? (size_t)(slash-path)+1 : 0;
      if (directory+strlen(THE_OOREXX_API_LIBRARY)+1 > sizeof(companion))
      {
         snprintf(error,sizeof(error),"companion API library path is too long");
         goto failed;
      }
      memcpy(companion,path,directory);
      strcpy(companion+directory,THE_OOREXX_API_LIBRARY);
      api_path=companion;
   }
   /* Load the companion first: on macOS librexx may depend on its @rpath
    * install name even when both library overrides are absolute paths. */
   api_library=the_macro_library_open(api_path,error,sizeof(error));
   if (!api_library) goto failed;
   library=the_macro_library_open(path,error,sizeof(error));
   if (!library) goto failed;
#define LOAD_OOREXX_FROM(handle,name,type) \
   the_##name=(type)the_macro_library_symbol(handle,#name); \
   if (!the_##name) { snprintf(error,sizeof(error),"missing %s",#name); goto failed; }
#define LOAD_OOREXX_INTERPRETER(name,type) LOAD_OOREXX_FROM(library,name,type)
   THE_OOREXX_INTERPRETER_FUNCTIONS(LOAD_OOREXX_INTERPRETER)
#undef LOAD_OOREXX_INTERPRETER
#define LOAD_OOREXX_API(name,type) LOAD_OOREXX_FROM(api_library,name,type)
   THE_OOREXX_API_FUNCTIONS(LOAD_OOREXX_API)
#undef LOAD_OOREXX_API
#undef LOAD_OOREXX_FROM
   return 1;
failed:
   fprintf(stderr,"THE: Unable to load ooRexx runtime %s: %s\nSet THE_OOREXX_LIBRARY to its full library path and THE_OOREXX_API_LIBRARY if its API library is in another directory.\n",path,error);
   if (library) the_macro_library_close(library);
   if (api_library) the_macro_library_close(api_library);
   library=api_library=NULL;
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
