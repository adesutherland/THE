/***********************************************************************/
/* trace.c - Debugging and tracing functions.                          */
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


#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <time.h>
#include <string.h>

void trace_initialise(void);
void trace_function(char *);
void trace_return(void);
void trace_string(char *,...);
void trace_constant(char *);

static char trace_save_str[40];
static char trace_env[40];
static FILE *trace_fp;
static short trace_number=0,trace_level=(-1);
/***********************************************************************/
void trace_initialise(void)
/***********************************************************************/
{
   char *trace_env_ptr=getenv( "THE_TRACE" );

   trace_fp = NULL;
   if ( trace_env_ptr == NULL )
      return;
   strcpy( trace_env, trace_env_ptr );
   trace_number = trace_level = 0;
   strcpy( trace_save_str, "" );
   return;
}
/***********************************************************************/
void trace_function(char *trace_str)
/***********************************************************************/
{
   register int i;
   time_t timer;
   struct tm *tblock=NULL;

   if ( trace_level == (-1) )
      return;
   trace_level++;
   trace_fp = fopen( trace_env, "a" );
   if ( trace_fp == NULL )
   return;
   for ( i = 0; i < trace_level; i++ )
      fprintf( trace_fp, "  " );
   fprintf( trace_fp, "(%2.2d)%-s", trace_level, trace_str );
   for ( i = 0; i < 100-(trace_level*2) - strlen( trace_str ); i++ )
      fprintf( trace_fp, " " );
/*
   time( &timer );
   tblock = localtime( &timer );
   fprintf( trace_fp, "%2.2d:%2.2d:%2.2d\n", tblock->tm_hour, tblock->tm_min, tblock->tm_sec );
*/
   fprintf( trace_fp, "\n");
   fclose( trace_fp );
   return;
}
/***********************************************************************/
void trace_return(void)
/***********************************************************************/
{
   if ( trace_level == (-1) )
      return;
   trace_level--;
   trace_fp = fopen( trace_env, "a" );
   if ( trace_fp == NULL )
      return;
   if ( trace_level < 0 )
   {
      fprintf( trace_fp, "****** trace level below zero ********\n" );
   }
   fclose( trace_fp );
   return;
}
/***********************************************************************/
void trace_string(char *fmt,...)
/***********************************************************************/
{
   va_list args;

   va_start( args, fmt );
   trace_fp = fopen( trace_env, "a" );
   if ( trace_fp == NULL )
   {
      va_end( args );
      return;
   }
   vfprintf( trace_fp, fmt, args );
   fclose( trace_fp );
   va_end( args );
   return;
}
/***********************************************************************/
void trace_constant(char *str)
/***********************************************************************/
{
   trace_fp = fopen( trace_env, "a" );
   if ( trace_fp == NULL )
      return;
   fprintf( trace_fp, "%s", str );
   fclose( trace_fp );
   return;
}
