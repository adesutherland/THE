/* ooRexx is compiled separately from Regina using its own SDK ABI. */
#ifdef THE_HAVE_OOREXX
#undef USE_REGINA
#define USE_OOREXX 1
#include <the.h>
#include "macroruntime.h"
#include "thedriver.h"
#include "workbench.h"
#include "workbenchdocument.h"
#include "oorexx.h"
#define initialise_rexx oorexx_initialise_rexx
#define finalise_rexx oorexx_finalise_rexx
#define execute_macro_file oorexx_execute_macro_file
#define execute_macro_instore oorexx_execute_macro_instore
#define set_rexx_variable oorexx_set_rexx_variable
#define get_rexx_variable oorexx_get_rexx_variable
#define get_rexx_interpreter_version oorexx_get_rexx_interpreter_version
#define MyRexxDeregisterFunction oorexx_MyRexxDeregisterFunction
#define MyRexxRegisterFunctionExe oorexx_MyRexxRegisterFunctionExe
#define THE_Commands oorexx_THE_Commands
#define THE_Function_Exit_Handler oorexx_THE_Function_Exit_Handler
#define THE_SayTrace_Exit_Handler oorexx_THE_SayTrace_Exit_Handler
#define rexx_interpreter_version_exit oorexx_rexx_interpreter_version_exit
#define THE_Function_Handler oorexx_THE_Function_Handler
#include <proto.h>
#include "classicrexx.inc"

int oorexx_read_queue_depth(void)
{
   size_t depth=0;
   if (RexxQueryQueue("SESSION",&depth) != 0 || depth > INT_MAX) return -1;
   return (int)depth;
}

short oorexx_read_queue_push(const char *text)
{
   CONSTRXSTRING value;
   MAKERXSTRING(value,text,strlen(text));
   return RexxAddQueue("SESSION",&value,RXQUEUE_LIFO) == 0 ? RC_OK : RC_INVALID_ENVIRON;
}
#endif
