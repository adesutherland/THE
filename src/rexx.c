/***********************************************************************/
/* REXX.C - REXX interface routines.                                   */
/***********************************************************************/
/*
 * THE - The Hessling Editor. A text editor similar to VM/CMS xedit.
 * Copyright (C) 1991-2026 Mark Hessling
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to:
 *
 *    The Free Software Foundation, Inc.
 *    675 Mass Ave,
 *    Cambridge, MA 02139 USA.
 *
 *
 * If you make modifications to this software that you feel increases
 * it usefulness for the rest of the community, please email the
 * changes, enhancements, bug fixes as well as any and all ideas to me.
 * This software is going to be maintained and enhanced as deemed
 * necessary by the community.
 *
 * Mark Hessling, mark@rexx.org  http://www.rexx.org/
 */

#include <the.h>
#include "macroruntime.h"
#include "thedriver.h"
#include "workbench.h"
#include "workbenchdocument.h"
#ifdef THE_HAVE_OOREXX
#include "oorexx.h"
#endif

LINE *rexxout_first_line=NULL;
LINE *rexxout_last_line=NULL;
LINE *rexxout_curr=NULL;
LINETYPE rexxout_number_lines=0L;

#ifdef USE_REGINA
#define initialise_rexx regina_initialise_rexx
#define finalise_rexx regina_finalise_rexx
#define execute_macro_file regina_execute_macro_file
#define execute_macro_instore regina_execute_macro_instore
#define set_rexx_variable regina_set_rexx_variable
#define get_rexx_variable regina_get_rexx_variable
#define get_rexx_interpreter_version regina_get_rexx_interpreter_version
#define MyRexxDeregisterFunction regina_MyRexxDeregisterFunction
#define MyRexxRegisterFunctionExe regina_MyRexxRegisterFunctionExe
#include <proto.h>
#include "classicrexx.inc"
#undef initialise_rexx
#undef finalise_rexx
#undef execute_macro_file
#undef execute_macro_instore
#undef set_rexx_variable
#undef get_rexx_variable
#undef get_rexx_interpreter_version
#undef MyRexxDeregisterFunction
#undef MyRexxRegisterFunctionExe
#else
#include <proto.h>
#endif

static bool macro_initialised=FALSE;

int the_read_queue_depth(void)
{
#ifdef THE_HAVE_OOREXX
   if (macro_initialised && the_macro_uses_oorexx()) return oorexx_read_queue_depth();
#endif
#ifdef USE_REGINA
   ULONG depth=0;
   if (macro_initialised && the_macro_uses_regina())
      return RexxQueryQueue("SESSION", &depth) == 0 ? (int)depth : -1;
#endif
   return 0;
}

short the_read_queue_push(const char *text)
{
#ifdef THE_HAVE_OOREXX
   if (macro_initialised && the_macro_uses_oorexx()) return oorexx_read_queue_push(text);
#endif
#ifdef USE_REGINA
   RXSTRING value;
   if (macro_initialised && the_macro_uses_regina())
   {
      MAKERXSTRING(value,(char *)text,strlen(text));
      return RexxAddQueue("SESSION", &value, RXQUEUE_LIFO) == 0 ? RC_OK : RC_INVALID_ENVIRON;
   }
#endif
   (void)text;
   return RC_OK;
}

/* The public bridge dispatches one engine for the lifetime of this process. */
/***********************************************************************/
short initialise_rexx
      (void)
/***********************************************************************/
{
   short rc=RC_INVALID_ENVIRON;
   if (the_macro_uses_regina())
   {
#ifdef USE_REGINA
      rc=regina_initialise_rexx();
#else
      fprintf(stderr,"THE: Regina support was not compiled into this build.\n");
#endif
   }
   else if (the_macro_uses_oorexx())
   {
#ifdef THE_HAVE_OOREXX
      rc=oorexx_initialise_rexx();
#else
      fprintf(stderr,"THE: ooRexx support was not compiled into this build.\n");
#endif
   }
   else if (the_macro_uses_crexx())
   {
#ifdef USE_CREXX
      rc=initialise_crexx();
#else
      fprintf(stderr,"THE: cREXX support was not compiled into this build.\n");
#endif
   }
   else if (strcmp(the_macro_interpreter(),"NONE")!=0)
      fprintf(stderr,"THE: THE_REXX_INTERPRETER must be CREXX, REGINA, OOREXX or NONE.\n");
   macro_initialised=(rc==RC_OK);
   return rc;
}
/***********************************************************************/
short finalise_rexx
      (void)
/***********************************************************************/
{
 if (!macro_initialised) return RC_OK;
 macro_initialised=FALSE;
#ifdef THE_HAVE_OOREXX
 if (the_macro_uses_oorexx()) return oorexx_finalise_rexx();
#endif
#ifdef USE_REGINA
 if (the_macro_uses_regina()) return regina_finalise_rexx();
#endif
#ifdef USE_CREXX
 return(finalise_crexx());
#else
 return(RC_OK);
#endif
}
/***********************************************************************/
short execute_macro_file
      (CHARTYPE *filename,CHARTYPE *params,short *macrorc,bool interactive)
/***********************************************************************/
{
 short rc=RC_INVALID_ENVIRON;
 if (!macro_initialised) return rc;
 the_workbench_document_enter();
 xedit_cursor_begin();
#ifdef THE_HAVE_OOREXX
 if (the_macro_uses_oorexx()) rc=oorexx_execute_macro_file(filename,params,macrorc,interactive);
 else
#endif
#ifdef USE_REGINA
 if (the_macro_uses_regina()) rc=regina_execute_macro_file(filename,params,macrorc,interactive);
 else
#endif
#ifdef USE_CREXX
 rc=execute_crexx_macro_file(filename,params,macrorc,interactive);
#else
 rc=RC_INVALID_ENVIRON;
#endif
 xedit_cursor_end();
 the_workbench_document_leave();
 return rc;
}
/***********************************************************************/
short execute_macro_instore
      (CHARTYPE *commands,short *macrorc,CHARTYPE **pcode,int *pcode_len,int *tokenised,int macro_ident)
/***********************************************************************/
{
 short rc=RC_INVALID_ENVIRON;
 if (!macro_initialised) return rc;
 the_workbench_document_enter();
 xedit_cursor_begin();
#ifdef THE_HAVE_OOREXX
 if (the_macro_uses_oorexx()) rc=oorexx_execute_macro_instore(commands,macrorc,pcode,pcode_len,tokenised,macro_ident);
 else
#endif
#ifdef USE_REGINA
 if (the_macro_uses_regina()) rc=regina_execute_macro_instore(commands,macrorc,pcode,pcode_len,tokenised,macro_ident);
 else
#endif
#ifdef USE_CREXX
 rc=execute_crexx_macro_instore(commands,macrorc,pcode,pcode_len,tokenised,macro_ident);
#else
 rc=RC_INVALID_ENVIRON;
#endif
 xedit_cursor_end();
 the_workbench_document_leave();
 return rc;
}
/***********************************************************************/
short set_rexx_variable
      (CHARTYPE *name,CHARTYPE *value,LENGTHTYPE value_length,int suffix)
/***********************************************************************/
{
 if (!macro_initialised) return RC_INVALID_ENVIRON;
#ifdef THE_HAVE_OOREXX
 if (the_macro_uses_oorexx()) return oorexx_set_rexx_variable(name,value,value_length,suffix);
#endif
#ifdef USE_REGINA
 if (the_macro_uses_regina()) return regina_set_rexx_variable(name,value,value_length,suffix);
#endif
#ifdef USE_CREXX
 return(set_crexx_variable(name,value,value_length,suffix));
#else
 (void)name;
 (void)value;
 (void)value_length;
 (void)suffix;
 return(RC_OK);
#endif
}

/***********************************************************************/
short get_rexx_variable
      (CHARTYPE *name,CHARTYPE **value,int *value_length)
/***********************************************************************/
{
 if (!macro_initialised) return RC_INVALID_ENVIRON;
#ifdef THE_HAVE_OOREXX
 if (the_macro_uses_oorexx()) return oorexx_get_rexx_variable(name,value,value_length);
#endif
#ifdef USE_REGINA
 if (the_macro_uses_regina()) return regina_get_rexx_variable(name,value,value_length);
#endif
#ifdef USE_CREXX
 return(get_crexx_variable(name,value,value_length));
#else
 (void)name;
 if (value != NULL)
    *value = NULL;
 if (value_length != NULL)
    *value_length = 0;
 return(RC_OK);
#endif
}

/***********************************************************************/
CHARTYPE *get_rexx_interpreter_version
      (CHARTYPE *buf)
/***********************************************************************/
{
 if (!macro_initialised) { strcpy((char *)buf,"NONE"); return buf; }
#ifdef THE_HAVE_OOREXX
 if (the_macro_uses_oorexx()) return oorexx_get_rexx_interpreter_version(buf);
#endif
#ifdef USE_REGINA
 if (the_macro_uses_regina()) return regina_get_rexx_interpreter_version(buf);
#endif
#ifdef USE_CREXX
 return(get_crexx_interpreter_version(buf));
#else
 strcpy((DEFCHAR *)buf,"NONE");
 return(buf);
#endif
}

/***********************************************************************/
unsigned long MyRexxDeregisterFunction
   (
   CHARTYPE *Name)       /* Name of function to be deregistered        */
/***********************************************************************/
{
#ifdef THE_HAVE_OOREXX
 if (the_macro_uses_oorexx()) return oorexx_MyRexxDeregisterFunction(Name);
#endif
#ifdef USE_REGINA
 if (the_macro_uses_regina()) return regina_MyRexxDeregisterFunction(Name);
#endif
   return 0L;
}

/***********************************************************************/
unsigned long MyRexxRegisterFunctionExe
   (
   CHARTYPE *Name)         /* Name of function to be registered        */
/***********************************************************************/
{
#ifdef THE_HAVE_OOREXX
 if (the_macro_uses_oorexx()) return oorexx_MyRexxRegisterFunctionExe(Name);
#endif
#ifdef USE_REGINA
 if (the_macro_uses_regina()) return regina_MyRexxRegisterFunctionExe(Name);
#endif
   return 0L;
}
