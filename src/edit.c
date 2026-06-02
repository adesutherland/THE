/***********************************************************************/
/* EDIT.C - The body of the program.                                   */
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
#include <proto.h>
#include "thedriver.h"
#include "inputevent.h"
#ifdef USE_SDSLH
#include "thread_utils.h"
#endif

bool prefix_changed=FALSE;

/***********************************************************************/
void editor(void)
/***********************************************************************/
{
   TheDriverWindowCursor cursor;

   TRACE_FUNCTION("edit.c:    editor");
   /*
    * Reset any command line positioning parameters so only those files
    * edited from the command line take on the line/col, command line
    * position.
    */
   startup_line = startup_column = 0;

   if (display_screens > 1)
      display_screen((CHARTYPE)(other_screen));

   cursor = the_driver->capture_window_cursor(driver_current_window());
   the_driver->restore_window_cursor(driver_current_window(), cursor);
   the_driver->refresh_window(driver_current_window());
   the_driver->update();
   if (error_on_screen)
   {
      if (driver_global_window_exists(THE_DRIVER_GLOBAL_ERROR))
      {
         the_driver->restore_window_cursor(driver_current_window(), cursor);
         the_driver->refresh_window(driver_current_window());
         driver_touch_and_refresh_window(driver_global_window(THE_DRIVER_GLOBAL_ERROR));
         the_driver->update();
      }
   }
   for ( ; ; )
   {
      if ( process_key( -1, FALSE ) != RC_OK )
         break;

#ifdef USE_SDSLH
      if (CURRENT_FILE && CURRENT_FILE->sdslh_comm && CURRENT_FILE->cb) {
         process_delta(CURRENT_FILE->cb);
      }
#endif
   }
   TRACE_RETURN();
   return;
}

/***********************************************************************/
int process_key(int key, bool mouse_details_present)
/***********************************************************************/
{
   TheDriverWindowCursor cursor;
   TheInputEvent input_event;
   short rc=RC_OK;
   CHARTYPE string_key[2];

   TRACE_FUNCTION("edit.c:    process_key");
#if defined(USE_EXTCURSES)
   cursor = the_driver->capture_window_cursor(driver_current_window());
   the_driver->restore_window_cursor(driver_current_window(), cursor);
   the_driver->refresh_window(driver_current_window());
   the_driver->update();
#endif
   string_key[1] = '\0';

#ifdef CAN_RESIZE
   if (the_driver_is_terminal_resized())
   {
      (void)THE_Resize(0,0);
      (void)THERefresh((CHARTYPE *)"");
   }
#endif
#ifdef THE_SINGLE_INSTANCE_ENABLED
   if ( single_instance_server )
      key = process_fifo_input( key );
#endif
   if (key == THE_KEY_NONE)
   {
#ifdef USE_SDSLH
      if (CURRENT_FILE && CURRENT_FILE->sdslh_comm && CURRENT_FILE->cb) {
         if (cb_check_parse_complete_event(CURRENT_FILE->cb) == 1) {
             cb_reset_parse_complete_event(CURRENT_FILE->cb);
              key = THE_KEY_PARSE_COMPLETE;
          } else {
              /* Wait for input with a 200ms timeout to allow background events to trigger a redraw */
              the_driver->set_current_window_timeout(200);
              key = the_driver_read_legacy_key();
              the_driver->set_current_window_timeout(-1); /* Back to blocking */
              if (key == THE_KEY_NONE) {
                  /* Timeout occurred, check event again */
                  if (cb_check_parse_complete_event(CURRENT_FILE->cb) == 1) {
                      cb_reset_parse_complete_event(CURRENT_FILE->cb);
                      key = THE_KEY_PARSE_COMPLETE;
                  } else {
                      key = THE_KEY_NONE; /* No input, no event - return to main loop */
                  }
              }
          }
      } else {
          key = the_driver_read_legacy_key();
      }
#else
      key = the_driver_read_legacy_key();
#endif
   }
   if (the_input_event_from_legacy_key(key, &input_event))
   {
      int normalized_key = key;

      if (the_input_event_to_legacy_key(&input_event, &normalized_key))
         key = normalized_key;
   }
#if defined(THE_MOUSE_ENABLED)
   if (!the_input_legacy_key_is_mouse(key))
   {
      if (!mouse_details_present)
         reset_saved_mouse_pos();
   }
#endif

#ifdef CAN_RESIZE
   if (the_driver_is_terminal_resized())
   {
      TRACE_RETURN();
      return(RC_OK);
   }
#endif

#ifdef THE_KEY_RESIZE
   if ( key == THE_KEY_RESIZE )
   {
      (void)THE_Resize(0,0);
      (void)THERefresh((CHARTYPE *)"");
      TRACE_RETURN();
      return(RC_OK);
   }
#endif
#ifdef USE_SDSLH
   if ( key == THE_KEY_PARSE_COMPLETE )
   {
      build_screen(current_screen);
      display_screen(current_screen);
      show_statarea();
      if (error_on_screen
      &&  driver_global_window_exists(THE_DRIVER_GLOBAL_ERROR))
      {
         cursor = the_driver->capture_window_cursor(driver_current_window());
         driver_touch_and_refresh_window(driver_global_window(THE_DRIVER_GLOBAL_ERROR));
         the_driver->restore_window_cursor(driver_current_window(), cursor);
         the_driver->refresh_window(driver_current_window());
         the_driver->update();
      }
      TRACE_RETURN();
      return(RC_OK);
   }
#endif
   if ( key == THE_KEY_NONE )
   {
      TRACE_RETURN();
      return(RC_OK);
   }

   initial = FALSE;                 /* set first time a key is requested */
   if (error_on_screen)
      clear_msgline(key);
   if (current_key == -1)
      current_key = 0;
   else
   {
      if ( current_key == 7 )
         current_key = 0;
      else
         current_key++;
   }
   /*
    * Save details about the last key pressed
    */
   lastkeys[current_key] = key;
   if (the_input_legacy_key_is_mouse(key))
   {
      lastkeys_is_mouse[current_key] = 1;
   }
   else
      lastkeys_is_mouse[current_key] = 0;
   /*
    * If we are recording a macro, check if the key hit is the end-of-record
    * key.
    */
   if ( record_fp )
   {
      if ( key == record_key )
      {
         char ctime_buf[26];
         time_t now;
         /*
          * Write a comment at the bottom
          */
         now = time( NULL );
         strcpy( ctime_buf, ctime( &now ) );
         ctime_buf[24] = '\0';
         fprintf( record_fp, "/* Recording of macro ended %s */\n", ctime_buf );
         fclose( record_fp );
         (*the_free)( record_status );
         record_fp = NULL;
         record_status = NULL;
         /*
          * Refresh the status area to reflect we are no longer recording
          */
         show_statarea();
         cursor = the_driver->capture_window_cursor(driver_current_window());
         the_driver->restore_window_cursor(driver_current_window(), cursor);
         the_driver->refresh_window(driver_current_window());
         the_driver->update();
         TRACE_RETURN();
         return(RC_OK);
      }
      /*
       * If we are recording a macro, write the key definintion
       * here
       */
      write_macro( get_key_definition( key, THE_KEY_DEFINE_RAW, TRUE, (bool)(the_input_legacy_key_is_mouse(key) ? TRUE : FALSE ) ) );
   }
   save_for_repeat = 0;
   rc = function_key( key, OPTION_NORMAL, mouse_details_present );
   save_for_repeat = 1;
   if ( number_of_files == 0 )
   {
      TRACE_RETURN();
      return(RC_INVALID_ENVIRON);
   }

   if (rc >= RAW_KEY)
   {
      if (rc > RAW_KEY)
         key = rc - (RAW_KEY*2);
      if (key < 256 && key >= 0)
      {
         string_key[0] = (CHARTYPE)key;
         /*
          * If operating in CUA mode, and a CUA block exists, check
          * if the block should be reset or deleted before executing
          * the command.
          */
         if ( INTERFACEx == INTERFACE_CUA
         &&  CURRENT_VIEW->mark_type == M_CUA )
         {
            ResetOrDeleteCUABlock( CUA_DELETE_BLOCK );
         }
         (void)Text(string_key);
      }
   }

   show_statarea();

#ifdef USE_SDSLH
   /* Re-evaluate bracket matching on cursor movement only if a bracket is involved */
   if (CURRENT_FILE && CURRENT_FILE->cb && CURRENT_VIEW->current_window == WINDOW_FILEAREA) {
       static bool was_on_bracket = FALSE;
       bool is_on_bracket = FALSE;
       TheDriverWindowCursor bracket_cursor;
       LINETYPE screen_line=0;
       LENGTHTYPE screen_column=0;
       LINETYPE current_file_line=(-1L);
       LENGTHTYPE current_file_column=(-1);
       bracket_cursor = the_driver->capture_window_cursor(driver_current_window());
       get_cursor_position(&screen_line, &screen_column, &current_file_line, &current_file_column);
       
       if (current_file_line > 0 && current_file_line <= (LINETYPE)CURRENT_FILE->cb->line_count) {
           enter_codeblock_critical_section();
           CodeBufferLine *line = &CURRENT_FILE->cb->lines[current_file_line - 1];
           if (current_file_column > 0 && current_file_column <= line->length) {
               CodeBufferCharacter *c = &line->characters[current_file_column - 1];
               if (c->token_type == LEXER_LH_CODEBLOCK || c->token_type == LEXER_RH_CODEBLOCK || 
                   c->token_type == LEXER_LH_EXPR || c->token_type == LEXER_RH_EXPR ||
                   c->token_type == LEXER_LH_BLOCK || c->token_type == LEXER_RH_BLOCK || 
                   c->token_type == LEXER_SEPARATOR) {
                   is_on_bracket = TRUE;
               }
           }
           exit_codeblock_critical_section();
       }
       
       if (is_on_bracket || was_on_bracket) {
           build_screen(current_screen);
           display_screen(current_screen);
           the_driver->restore_window_cursor(driver_current_window(), bracket_cursor);
           /* Force correct cursor position into virtual screen */
           the_driver->refresh_window(driver_current_window());
       }
       was_on_bracket = is_on_bracket;
   }
#endif

   if (display_screens > 1
   &&  SCREEN_FILE(0) == SCREEN_FILE(1))
   {
      build_screen((CHARTYPE)(other_screen));
      display_screen((CHARTYPE)(other_screen));
/*    refresh_screen(other_screen);*/
      show_heading((CHARTYPE)(other_screen));
   }
   refresh_screen(current_screen);
   if (error_on_screen)
   {
      cursor = the_driver->capture_window_cursor(driver_current_window());
      if (driver_global_window_exists(THE_DRIVER_GLOBAL_ERROR))
      {
         driver_touch_and_refresh_window(driver_global_window(THE_DRIVER_GLOBAL_ERROR));
      }
      the_driver->restore_window_cursor(driver_current_window(), cursor);
      the_driver->refresh_window(driver_current_window());
   }

#ifdef HAVE_BROKEN_SYSVR4_CURSES
   cursor = the_driver->capture_window_cursor(driver_current_window());
   the_driver->restore_window_cursor(driver_current_window(), cursor);
   the_driver->refresh_window(driver_current_window());
   the_driver->update();
#else
   cursor = the_driver->capture_window_cursor(driver_current_window());
   the_driver->restore_window_cursor(driver_current_window(), cursor);
   the_driver->refresh_window(driver_current_window());
   the_driver->update();
#endif
   TRACE_RETURN();
   return(RC_OK);
}

/***********************************************************************/
short EditFile(CHARTYPE *fn, bool external_command_line)
/***********************************************************************/
{
   short rc=RC_OK;
   TheDriverWindowCursor cursor;
   VIEW_DETAILS *save_current_view=NULL;
   VIEW_DETAILS *previous_current_view=NULL;
   CHARTYPE save_prefix=0;
   short save_gap=0;
   ROWTYPE save_cmd_line=0;
   bool save_id_line=0;

   TRACE_FUNCTION("edit.c:    EditFile");
   /*
    * With no arguments, edit the next file in the ring...
    */
   if (strcmp((DEFCHAR *)fn,"") == 0)
   {
      rc = advance_view(NULL,DIRECTION_FORWARD);
      TRACE_RETURN();
      return(rc);
   }
   /*
    * With "-" as argument, edit the previous file in the ring...
    */
   if (strcmp((DEFCHAR *)fn,"-") == 0)
   {
      rc = advance_view(NULL,DIRECTION_BACKWARD);
      TRACE_RETURN();
      return(rc);
   }
   /*
    * If there are still file(s) in the ring, clear the command line and
    * save any changes to the focus line.
    */
   if (number_of_files > 0)
   {
      post_process_line(CURRENT_VIEW,CURRENT_VIEW->focus_line,(LINE *)NULL,TRUE);
      memset(cmd_rec,' ',max_line_length);
      cmd_rec_len = 0;
   }
   previous_current_view = CURRENT_VIEW;
   /*
    * Save the position of the cursor for the current view before getting
    * the contents of the new file...
    */
   if (curses_started
   &&  number_of_files > 0)
   {
      if (driver_current_role_exists(WINDOW_COMMAND))
      {
         the_driver->move_window_cursor(driver_current_role_window(WINDOW_COMMAND), 0, 0);
         the_driver->clear_current_role_to_eol(WINDOW_COMMAND);
      }
      cursor = the_driver->capture_window_cursor(driver_current_role_window(WINDOW_FILEAREA));
      if (cursor.valid)
      {
         CURRENT_VIEW->y[WINDOW_FILEAREA] = cursor.row;
         CURRENT_VIEW->x[WINDOW_FILEAREA] = cursor.col;
      }
      if (driver_current_role_exists(WINDOW_PREFIX))
      {
         cursor = the_driver->capture_window_cursor(driver_current_role_window(WINDOW_PREFIX));
         if (cursor.valid)
         {
            CURRENT_VIEW->y[WINDOW_PREFIX] = cursor.row;
            CURRENT_VIEW->x[WINDOW_PREFIX] = cursor.col;
         }
      }
   }
   if (number_of_files > 0)
   {
      save_prefix=CURRENT_VIEW->prefix;
      save_gap=CURRENT_VIEW->prefix_gap;
      save_cmd_line=CURRENT_VIEW->cmd_line;
      save_id_line=CURRENT_VIEW->id_line;
   }
   /*
    * Read the contents of the new file into memory...
    */
   if ((rc = get_file(strrmdup(strtrans(fn,OSLASH,ISLASH),ISLASH,TRUE))) != RC_OK)
   {
      TRACE_RETURN();
      return(rc);
   }
   /*
    * If more than one screen is displayed, sort out which view is to be
    * displayed...
    */
   if (display_screens > 1)
   {
      save_current_view = CURRENT_VIEW;
      CURRENT_SCREEN.screen_view = CURRENT_VIEW = previous_current_view;
      advance_view(save_current_view,DIRECTION_FORWARD);
   }
   else
   {
      if (number_of_files > 0)
      {
         /*
          * If the position of the prefix or command line for the new view is
          * different from the previous view, rebuild the windows...
          */
         if ((save_prefix&PREFIX_LOCATION_MASK) != (CURRENT_VIEW->prefix&PREFIX_LOCATION_MASK)
         ||  save_gap != CURRENT_VIEW->prefix_gap
         ||  save_cmd_line != CURRENT_VIEW->cmd_line
         ||  save_id_line != CURRENT_VIEW->id_line)
         {
            set_screen_defaults();
            if (curses_started)
            {
               if (set_up_windows(current_screen) != RC_OK)
               {
                  TRACE_RETURN();
                  return(RC_OK);
               }
            }
         }
      }
      /*
       * Re-calculate CURLINE for the new view in case the CURLINE is no
       * longer in the display area.
       */
      prepare_view(current_screen);
   }
   pre_process_line(CURRENT_VIEW,CURRENT_VIEW->focus_line,(LINE *)NULL);
   build_screen(current_screen);
   /*
    * Position the cursor in the main window depending on the type of file
    */
   if (curses_started)
   {
      if (CURRENT_VIEW->in_ring)
      {
         the_driver->move_window_cursor(
            driver_current_role_window(WINDOW_FILEAREA),
            CURRENT_VIEW->y[WINDOW_FILEAREA],
            CURRENT_VIEW->x[WINDOW_FILEAREA]);
         if (driver_current_role_exists(WINDOW_PREFIX))
            the_driver->move_window_cursor(
               driver_current_role_window(WINDOW_PREFIX),
               CURRENT_VIEW->y[WINDOW_PREFIX],
               CURRENT_VIEW->x[WINDOW_PREFIX]);
         cursor = the_driver->capture_window_cursor(driver_current_window());
         the_driver->restore_window_cursor(driver_current_window(), cursor);
      }
      else
      {
         if (CURRENT_FILE->pseudo_file == PSEUDO_DIR)
            the_driver->move_window_cursor(
               driver_current_role_window(WINDOW_FILEAREA),
               CURRENT_VIEW->current_row, FILE_START - 1);
         else
            the_driver->move_window_cursor(
               driver_current_role_window(WINDOW_FILEAREA),
               CURRENT_VIEW->current_row, 0);
      }
   }
   /*
    * Execute any profile file...
    */
   if ((REPROFILEx && CURRENT_VIEW->in_ring == FALSE)
   ||  (in_profile && external_command_line))
   {
      profile_file_executions++;
      in_reprofile = TRUE;
      if (system_prf != (CHARTYPE *)NULL || execute_profile)
      {
         rc = get_startup_profiles();
      }
      in_reprofile = FALSE;
   }
   /*
    * If the result of processing the profile file results in no files
    * in the ring, we need to get out NOW.
    */
   if (number_of_files == 0)
   {
      TRACE_RETURN();
      return(rc);
   }
/* pre_process_line(CURRENT_VIEW,CURRENT_VIEW->focus_line,(LINE *)NULL);*/
   build_screen(current_screen);
   /*
    * If startup values were specified on the command, line, move cursor
    * there...
    */
   if (startup_line != 0
   ||  startup_column != 0)
   {
      THEcursor_goto( startup_line, startup_column );
   }
   filetabs_start_view = NULL;
   /*
    * If curses hasn't started, don't try to use curses functions...
    */
   if (curses_started)
   {
      display_screen(current_screen);
      if (driver_current_role_exists(WINDOW_COMMAND))
         the_driver->move_window_cursor(driver_current_role_window(WINDOW_COMMAND), 0, 0);
      if (driver_current_role_exists(WINDOW_PREFIX))
         the_driver->touch_window(driver_current_role_window(WINDOW_PREFIX));
      if (driver_current_role_exists(WINDOW_GAP))
         the_driver->touch_window(driver_current_role_window(WINDOW_GAP));
      if (driver_current_role_exists(WINDOW_COMMAND))
         the_driver->touch_window(driver_current_role_window(WINDOW_COMMAND));
      if (driver_current_role_exists(WINDOW_IDLINE))
         the_driver->touch_window(driver_current_role_window(WINDOW_IDLINE));
      the_driver->touch_window(driver_current_role_window(WINDOW_FILEAREA));
      show_statarea();
   }
   /*
    * If we have a Rexx interpreter, register a handler for ring.x where
    * x is the number of files in the ring.
    */
   if (rexx_support)
   {
      CHARTYPE tmp[20];
      sprintf( (DEFCHAR *)tmp, "ring.%ld", number_of_files + ( (compatible_feel==COMPAT_XEDIT) ? 1 : 0 ) );
      MyRexxRegisterFunctionExe( tmp );
   }
   TRACE_RETURN();
   return(rc);
}
