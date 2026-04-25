/***********************************************************************/
/* CREXX.C - CREXX hosted profile/macro driver                         */
/***********************************************************************/
/*
 * THE - The Hessling Editor. A text editor similar to VM/CMS xedit.
 * Copyright (C) 1991-2026 Mark Hessling
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or any later version.
 */

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <the.h>
#include <proto.h>

#ifdef USE_CREXX
# include <crexxsaa.h>

# ifndef THE_CREXX_RXC
#  define THE_CREXX_RXC "rxc"
# endif
# ifndef THE_CREXX_RXAS
#  define THE_CREXX_RXAS "rxas"
# endif
# ifndef THE_CREXX_IMPORT_DIR
#  define THE_CREXX_IMPORT_DIR "."
# endif
# ifndef THE_CREXX_LIBRARY_RXBIN
#  define THE_CREXX_LIBRARY_RXBIN "library.rxbin"
# endif

static crexxsaa_context *the_crexx_context=NULL;

static const char *crexx_config_value(const char *env_name,const char *fallback)
{
   const char *value=getenv(env_name);

   if (value != NULL && *value != '\0')
      return(value);
   return(fallback);
}

static int crexx_same_char(int left,int right)
{
   return(tolower((unsigned char)left) == tolower((unsigned char)right));
}

static int crexx_has_suffix(const char *value,const char *suffix)
{
   size_t value_len=0,suffix_len=0,i=0;

   if (value == NULL || suffix == NULL)
      return(0);
   value_len = strlen(value);
   suffix_len = strlen(suffix);
   if (value_len < suffix_len)
      return(0);
   value += value_len - suffix_len;
   for (i=0;i<suffix_len;i++)
   {
      if (!crexx_same_char(value[i],suffix[i]))
         return(0);
   }
   return(1);
}

static char *crexx_alloc_path_with_suffix(const char *base,const char *suffix)
{
   char *path=NULL;
   size_t len=0;

   len = strlen(base) + strlen(suffix) + 1;
   path = (char *)(*the_malloc)(len);
   if (path == NULL)
      return(NULL);
   strcpy(path,base);
   strcat(path,suffix);
   return(path);
}

static void crexx_remove_and_free_path(char *path)
{
   if (path != NULL)
   {
      remove(path);
      (*the_free)(path);
   }
}

static void crexx_display_error(const char *prefix,const char *detail)
{
   CHARTYPE message[1024];

   if (detail == NULL)
      detail = "";
   snprintf((char *)message,sizeof(message),"%s%s%s",
            prefix,
            (*detail == '\0') ? "" : ": ",
            detail);
   display_error(0,message,FALSE);
}

static int the_crexx_address_callback(
   const crexxsaa_address_request *request,
   crexxsaa_address_response *response,
   void *userdata)
{
   CHARTYPE *command=NULL;
   size_t len=0;
   short rc=RC_OK;

   (void)userdata;

   if (response != NULL)
      response->rc = RC_SYSTEM_ERROR;
   if (request == NULL || response == NULL || request->command == NULL)
      return(-1);

   len = strlen(request->command) + 1;
   command = (CHARTYPE *)(*the_malloc)(len);
   if (command == NULL)
   {
      response->rc = RC_OUT_OF_MEMORY;
      return(-1);
   }
   strcpy((char *)command,request->command);
   rc = command_line(command,COMMAND_ONLY_FALSE);
   (*the_free)(command);

   response->rc = rc;
   return(0);
}

short initialise_crexx(void)
{
   const char *location=NULL;
   const char *library_rxbin=NULL;
   const char *rxc=NULL;
   const char *rxas=NULL;
   const char *import_dir=NULL;
   const char *cache_dir=NULL;

   TRACE_FUNCTION("crexx.c:   initialise_crexx");

   if (the_crexx_context != NULL)
   {
      TRACE_RETURN();
      return(RC_OK);
   }

   location = crexx_config_value("THE_CREXX_LOCATION",THE_CREXX_IMPORT_DIR);
   library_rxbin = crexx_config_value("THE_CREXX_LIBRARY_RXBIN",THE_CREXX_LIBRARY_RXBIN);
   rxc = crexx_config_value("THE_CREXX_RXC",THE_CREXX_RXC);
   rxas = crexx_config_value("THE_CREXX_RXAS",THE_CREXX_RXAS);
   import_dir = crexx_config_value("THE_CREXX_IMPORT_DIR",THE_CREXX_IMPORT_DIR);
   cache_dir = getenv("THE_CREXX_CACHE_DIR");

   if (crexxsaa_create(location,library_rxbin,&the_crexx_context) != 0)
   {
      crexx_display_error("Unable to initialise CREXXSAA",NULL);
      TRACE_RETURN();
      return(RC_INVALID_ENVIRON);
   }
   if (crexxsaa_set_compiler(the_crexx_context,rxc,rxas,import_dir) != 0)
   {
      crexx_display_error("Unable to configure CREXX compiler",
                          crexxsaa_last_error(the_crexx_context));
      crexxsaa_destroy(the_crexx_context);
      the_crexx_context = NULL;
      TRACE_RETURN();
      return(RC_INVALID_ENVIRON);
   }
   if (cache_dir != NULL && *cache_dir != '\0'
       && crexxsaa_set_cache_dir(the_crexx_context,cache_dir) != 0)
   {
      crexx_display_error("Unable to configure CREXX cache",
                          crexxsaa_last_error(the_crexx_context));
      crexxsaa_destroy(the_crexx_context);
      the_crexx_context = NULL;
      TRACE_RETURN();
      return(RC_INVALID_ENVIRON);
   }
   if (crexxsaa_register_address_environment(
          the_crexx_context,
          "THE",
          the_crexx_address_callback,
          NULL) != 0)
   {
      crexx_display_error("Unable to register CREXX ADDRESS THE",
                          crexxsaa_last_error(the_crexx_context));
      crexxsaa_destroy(the_crexx_context);
      the_crexx_context = NULL;
      TRACE_RETURN();
      return(RC_INVALID_ENVIRON);
   }
   if (crexxsaa_set_address_environment(the_crexx_context,"THE") != 0)
   {
      crexx_display_error("Unable to select CREXX ADDRESS THE",
                          crexxsaa_last_error(the_crexx_context));
      crexxsaa_destroy(the_crexx_context);
      the_crexx_context = NULL;
      TRACE_RETURN();
      return(RC_INVALID_ENVIRON);
   }

   TRACE_RETURN();
   return(RC_OK);
}

short finalise_crexx(void)
{
   TRACE_FUNCTION("crexx.c:   finalise_crexx");

   if (the_crexx_context != NULL)
   {
      crexxsaa_destroy(the_crexx_context);
      the_crexx_context = NULL;
   }

   TRACE_RETURN();
   return(RC_OK);
}

static short execute_crexx_macro_path(
   CHARTYPE *filename,
   CHARTYPE *params,
   short *macrorc,
   bool interactive,
   unsigned flags)
{
   const char *argv[1];
   int argc=0;
   int program_rc=0;
   int run_rc=0;
   short rc=RC_OK;

   if (macrorc != NULL)
      *macrorc = 0;
   if (filename == NULL || strcmp((char *)filename,"") == 0)
      return(RC_INVALID_OPERAND);
   if (the_crexx_context == NULL)
   {
      rc = initialise_crexx();
      if (rc != RC_OK)
         return(rc);
   }

   strncpy((char *)rexx_macro_name,(char *)filename,MAX_FILE_NAME);
   rexx_macro_name[MAX_FILE_NAME] = '\0';
   if (params == NULL || strcmp((char *)params,"") == 0)
      strcpy((char *)rexx_macro_parameters,"");
   else
   {
      strncpy((char *)rexx_macro_parameters,(char *)params,MAX_FILE_NAME);
      rexx_macro_parameters[MAX_FILE_NAME] = '\0';
      argv[0] = (char *)params;
      argc = 1;
   }

   if (interactive)
   {
# if defined(OS2) || defined(WIN32)
      execute_os_command((CHARTYPE *)"REM",TRUE,FALSE);
# endif
# ifdef UNIX
      execute_os_command((CHARTYPE *)"echo",TRUE,FALSE);
# endif
   }

   if (crexx_has_suffix((char *)filename,".rxbin"))
      run_rc = crexxsaa_run_rxbin(the_crexx_context,(char *)filename,argc,argv,&program_rc);
   else
      run_rc = crexxsaa_run_source(the_crexx_context,(char *)filename,"THE",flags,argc,argv,&program_rc);

   if (macrorc != NULL)
      *macrorc = (short)program_rc;
   if (run_rc != 0)
   {
      crexx_display_error("Unable to run CREXX macro",
                          crexxsaa_last_error(the_crexx_context));
      rc = RC_SYSTEM_ERROR;
   }
   return(rc);
}

short execute_crexx_macro_file(CHARTYPE *filename,CHARTYPE *params,short *macrorc,bool interactive)
{
   short rc=RC_OK;

   TRACE_FUNCTION("crexx.c:   execute_crexx_macro_file");
   rc = execute_crexx_macro_path(filename,params,macrorc,interactive,0);
   TRACE_RETURN();
   return(rc);
}

short execute_crexx_macro_instore(
   CHARTYPE *commands,
   short *macrorc,
   CHARTYPE **pcode,
   int *pcode_len,
   int *tokenised,
   int macro_ident)
{
   char *source_base=NULL,*source_path=NULL;
   FILE *outfile=NULL;
   short rc=RC_OK;

   (void)macro_ident;

   TRACE_FUNCTION("crexx.c:   execute_crexx_macro_instore");

   if (pcode != NULL)
      *pcode = NULL;
   if (pcode_len != NULL)
      *pcode_len = 0;
   if (tokenised != NULL)
      *tokenised = 0;
   if (commands == NULL)
   {
      TRACE_RETURN();
      return(RC_INVALID_OPERAND);
   }

   source_base = thetmpnam("CRXINS");
   if (source_base == NULL)
   {
      TRACE_RETURN();
      return(RC_OUT_OF_MEMORY);
   }
   source_path = crexx_alloc_path_with_suffix(source_base,".rexx");
   if (source_path == NULL)
   {
      crexx_remove_and_free_path(source_base);
      TRACE_RETURN();
      return(RC_OUT_OF_MEMORY);
   }
   remove(source_base);

   outfile = fopen(source_path,"w");
   if (outfile == NULL)
      rc = RC_ACCESS_DENIED;
   else
   {
      if (fputs((char *)commands,outfile) == EOF)
         rc = RC_SYSTEM_ERROR;
      if (fclose(outfile) != 0)
         rc = RC_SYSTEM_ERROR;
   }

   if (rc == RC_OK)
      rc = execute_crexx_macro_path((CHARTYPE *)source_path,NULL,macrorc,FALSE,CREXXSAA_CACHE_DISABLE);

   crexx_remove_and_free_path(source_path);
   crexx_remove_and_free_path(source_base);
   TRACE_RETURN();
   return(rc);
}

CHARTYPE *get_crexx_interpreter_version(CHARTYPE *buf)
{
   strcpy((char *)buf,"CREXXSAA");
   return(buf);
}

#else

short initialise_crexx(void)
{
   return(RC_INVALID_ENVIRON);
}

short finalise_crexx(void)
{
   return(RC_OK);
}

short execute_crexx_macro_file(CHARTYPE *filename,CHARTYPE *params,short *macrorc,bool interactive)
{
   (void)filename;
   (void)params;
   (void)interactive;
   if (macrorc != NULL)
      *macrorc = 0;
   return(RC_INVALID_ENVIRON);
}

short execute_crexx_macro_instore(
   CHARTYPE *commands,
   short *macrorc,
   CHARTYPE **pcode,
   int *pcode_len,
   int *tokenised,
   int macro_ident)
{
   (void)commands;
   (void)pcode;
   (void)pcode_len;
   (void)tokenised;
   (void)macro_ident;
   if (macrorc != NULL)
      *macrorc = 0;
   return(RC_INVALID_ENVIRON);
}

CHARTYPE *get_crexx_interpreter_version(CHARTYPE *buf)
{
   strcpy((char *)buf,"CREXX unavailable");
   return(buf);
}

#endif
