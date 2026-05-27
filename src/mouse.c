/***********************************************************************/
/* MOUSE.C - THE mouse handling                                        */
/* This file contains all commands that can be assigned to function    */
/* keys or typed on the command line.                                  */
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
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include "cursesdriver.h"
#include "inputevent.h"
#include "mousehit.h"

/*   3         2         1         0
 * 210987654321098765432109876543210
 *                maaaaabbbbb
 */
/*
 * Button masks
 */
#if 0
#define MOUSE_MODIFIER_MASK(x)  ((x) & 0xE000) /* 1110000000000000 */
#define MOUSE_ACTION_MASK(x)    ((x) & 0x1C00) /* 0001110000000000 */
#define MOUSE_BUTTON_MASK(x)    ((x) & 0x0380) /* 0000001110000000 */
#define MOUSE_WINDOW_MASK(x)    ((x) & 0x007F) /* 0000000001111111 */
#else
#define MOUSE_MODIFIER_MASK(x)  ((x) & 0xE0000) /* 11100000000000000000 */
#define MOUSE_ACTION_MASK(x)    ((x) & 0x1F000) /* 00011111000000000000 */
#define MOUSE_BUTTON_MASK(x)    ((x) & 0x00F80) /* 00000000111110000000 */
#define MOUSE_WINDOW_MASK(x)    ((x) & 0x0007F) /* 00000000000001111111 */
#endif
/*
 * Button modifiers
 */
#define MOUSE_MODIFIER_OFFSET   17 /* was 13 */
#define MOUSE_NORMAL            0
#define MOUSE_SHIFT             ((CURSES_DRIVER_MOUSE_MODIFIER_SHIFT >> 3)   << MOUSE_MODIFIER_OFFSET)
#define MOUSE_CONTROL           ((CURSES_DRIVER_MOUSE_MODIFIER_CONTROL >> 3) << MOUSE_MODIFIER_OFFSET)
#define MOUSE_ALT               ((CURSES_DRIVER_MOUSE_MODIFIER_ALT >> 3)     << MOUSE_MODIFIER_OFFSET)
/*
 * Button actions
 */
#define MOUSE_ACTION_OFFSET     14 /* was 10 */
#define MOUSE_PRESS             (CURSES_DRIVER_MOUSE_BUTTON_PRESSED << MOUSE_ACTION_OFFSET)
#define MOUSE_RELEASE           (CURSES_DRIVER_MOUSE_BUTTON_RELEASED << MOUSE_ACTION_OFFSET)
#define MOUSE_DRAG              (CURSES_DRIVER_MOUSE_BUTTON_MOVED << MOUSE_ACTION_OFFSET)
#define MOUSE_CLICK             (CURSES_DRIVER_MOUSE_BUTTON_CLICKED << MOUSE_ACTION_OFFSET)
#define MOUSE_DOUBLE_CLICK      (CURSES_DRIVER_MOUSE_BUTTON_DOUBLE_CLICKED << MOUSE_ACTION_OFFSET)
#if defined(PDCURSES_MOUSE_ENABLED)
#define MOUSE_SCROLLED          (CURSES_DRIVER_MOUSE_WHEEL_SCROLLED << MOUSE_ACTION_OFFSET)
#endif
/*
 * Button numbers
 */
#define MOUSE_BUTTON_OFFSET     7
#define MOUSE_LEFT              (1 << MOUSE_BUTTON_OFFSET)
#define MOUSE_MIDDLE            (2 << MOUSE_BUTTON_OFFSET)
#define MOUSE_RIGHT             (3 << MOUSE_BUTTON_OFFSET)
#define THE_MOUSE_WHEEL_UP      (4 << MOUSE_BUTTON_OFFSET)
#define THE_MOUSE_WHEEL_DOWN    (5 << MOUSE_BUTTON_OFFSET)
#define THE_MOUSE_WHEEL_LEFT    (6 << MOUSE_BUTTON_OFFSET)
#define THE_MOUSE_WHEEL_RIGHT   (7 << MOUSE_BUTTON_OFFSET)

#define MOUSE_INFO_TO_KEY(w,b,ba,bm) ((w)|(b<<MOUSE_BUTTON_OFFSET)|(ba<<MOUSE_ACTION_OFFSET)|((bm>>3)<<MOUSE_MODIFIER_OFFSET))

static CHARTYPE *button_names[] =
{
   (CHARTYPE *)"-button 0-",
   (CHARTYPE *)"LB", /* left button */
   (CHARTYPE *)"MB", /* middle button */
   (CHARTYPE *)"RB", /* right button */
   (CHARTYPE *)"UW", /* wheel up */
   (CHARTYPE *)"DW", /* wheel down */
   (CHARTYPE *)"LW", /* wheel left */
   (CHARTYPE *)"RW", /* wheel right */
};

static CHARTYPE *button_modifier_names[] =
{
   (CHARTYPE *)"",
   (CHARTYPE *)"S-", /* shift */
   (CHARTYPE *)"C-", /* control */
   (CHARTYPE *)"?",  /* unknown */
   (CHARTYPE *)"A-", /* alt */
};

static CHARTYPE *button_action_names[] =
{
   (CHARTYPE *)"R", /* release */
   (CHARTYPE *)"P", /* press */
   (CHARTYPE *)"C", /* clicked */
   (CHARTYPE *)"2", /* double clicked */
   (CHARTYPE *)"3", /* triple clicked */
   (CHARTYPE *)"D", /* dragged */
   (CHARTYPE *)"S", /* scrolled */
};

static FILE *mouse_trace_file(void)
{
   static short checked=0;
   static FILE *trace=NULL;
   char *path=NULL;

   if (!checked)
   {
      checked = 1;
      path = getenv("THE_MOUSE_TRACE");
      if (path != NULL && *path != '\0')
         trace = fopen(path,"a");
   }
   return trace;
}

void mouse_trace_message(const char *area, const char *format, ...)
{
   FILE *trace=mouse_trace_file();
   va_list args;

   if (trace == NULL)
      return;
   fprintf(trace,"mouse %s",area == NULL ? "trace" : area);
   if (format != NULL && *format != '\0')
   {
      fputc(' ',trace);
      va_start(args,format);
      vfprintf(trace,format,args);
      va_end(args);
   }
   fputc('\n',trace);
   fflush(trace);
}

#if defined(PDCURSES_MOUSE_ENABLED) || defined(NCURSES_MOUSE_VERSION)
/*
 * The logical target saved by each mouse key press.
 */
static TheInputEvent last_mouse_input;

static int mouse_window_position(CHARTYPE scrn, int w, int *row, int *col)
{
   WINDOW *win = NULL;

   if (row != NULL)
      *row = -1;
   if (col != NULL)
      *col = -1;
   if (scrn >= display_screens || row == NULL || col == NULL)
      return FALSE;

   if (w >= 0 && w < VIEW_WINDOWS)
      win = screen[scrn].win[w];
   else if (w == WINDOW_STATAREA)
      win = statarea;
   else if (w == WINDOW_FILETABS)
      win = filetabs;
   else if (w == WINDOW_DIVIDER)
      win = divider;

   curses_driver_mouse_position(win, row, col);
   return (*row != -1 && *col != -1);
}

static int mouse_locate_window(CHARTYPE *scrn, int *w, int *row, int *col)
{
   CHARTYPE i;
   int j;

   if (scrn != NULL)
      *scrn = current_screen;
   if (w != NULL)
      *w = WINDOW_ALL;
   if (row != NULL)
      *row = -1;
   if (col != NULL)
      *col = -1;

   for (i = 0; i < display_screens; i++)
   {
      for (j = 0; j < VIEW_WINDOWS; j++)
      {
         if (screen[i].win[j] != (WINDOW *)NULL
         &&  mouse_window_position(i, j, row, col))
         {
            if (scrn != NULL)
               *scrn = i;
            if (w != NULL)
               *w = j;
            return TRUE;
         }
      }
   }

   if (mouse_window_position(current_screen, WINDOW_STATAREA, row, col))
   {
      if (w != NULL)
         *w = WINDOW_STATAREA;
      return TRUE;
   }

   if (mouse_window_position(current_screen, WINDOW_FILETABS, row, col))
   {
      if (w != NULL)
         *w = WINDOW_FILETABS;
      return TRUE;
   }

   if (display_screens > 1
   &&  !horizontal
   &&  mouse_window_position(current_screen, WINDOW_DIVIDER, row, col))
   {
      if (w != NULL)
         *w = WINDOW_DIVIDER;
      return TRUE;
   }

   return FALSE;
}

static TheMouseHitArea mouse_area_from_window(int w)
{
   switch (w)
   {
      case WINDOW_FILEAREA:
         return THE_MOUSE_HIT_AREA_FILEAREA;
      case WINDOW_PREFIX:
         return THE_MOUSE_HIT_AREA_PREFIX;
      case WINDOW_COMMAND:
         return THE_MOUSE_HIT_AREA_COMMAND;
      case WINDOW_STATAREA:
         return THE_MOUSE_HIT_AREA_STATUS;
      case WINDOW_FILETABS:
         return THE_MOUSE_HIT_AREA_FILETABS;
      case WINDOW_DIVIDER:
         return THE_MOUSE_HIT_AREA_DIVIDER;
      case WINDOW_ARROW:
      case WINDOW_IDLINE:
      case WINDOW_GAP:
         return THE_MOUSE_HIT_AREA_WINDOW;
      default:
         return THE_MOUSE_HIT_AREA_NONE;
   }
}

static LINETYPE mouse_line_number_for_hit(CHARTYPE scrn, TheMouseHitArea area,
                                          int row)
{
   if ((area == THE_MOUSE_HIT_AREA_FILEAREA
     || area == THE_MOUSE_HIT_AREA_PREFIX)
   &&  scrn < display_screens
   &&  screen[scrn].sl != NULL
   &&  row >= 0
   &&  row < screen[scrn].rows[WINDOW_FILEAREA])
      return screen[scrn].sl[row].line_number;
   return 0;
}

static const CHARTYPE *mouse_filearea_line_for_hit(CHARTYPE scrn, int row,
                                                   size_t *len)
{
   SHOW_LINE *show_row;

   if (len != NULL)
      *len = 0;
   if (scrn >= display_screens
   ||  screen[scrn].sl == NULL
   ||  row < 0
   ||  row >= screen[scrn].rows[WINDOW_FILEAREA])
      return (const CHARTYPE *)"";
   show_row = &screen[scrn].sl[row];
   if (show_row->line_type == LINE_TOF || show_row->line_type == LINE_EOF)
      return (const CHARTYPE *)"";
   if (show_row->contents == NULL)
      return (const CHARTYPE *)"";
   if (len != NULL)
      *len = (size_t)show_row->length;
   return show_row->contents;
}

static int mouse_cell_for_hit(CHARTYPE scrn, TheMouseHitArea area, int row,
                              int col)
{
   VIEW_DETAILS *view;
   const CHARTYPE *line;
   size_t len;
   int viewport_col;

   if (col < 0)
      col = 0;
   switch (area)
   {
      case THE_MOUSE_HIT_AREA_FILEAREA:
      {
         int logical_col;

         view = (scrn < display_screens) ? screen[scrn].screen_view : NULL;
         viewport_col = (view != NULL) ? (int)view->verify_col - 1 : 0;
         if (viewport_col < 0)
            viewport_col = 0;
         line = mouse_filearea_line_for_hit(scrn, row, &len);
         logical_col = curses_driver_logical_col_from_display(
            line, len, viewport_col, col, TEXT_SNAP_BACKWARD);
         return (logical_col < 0) ? 0 : logical_col;
      }
      case THE_MOUSE_HIT_AREA_COMMAND:
      {
         int command_cell = cmd_verify_col - 1 + col;

         return (command_cell < 0) ? 0 : command_cell;
      }
      default:
         return col;
   }
}

static int mouse_build_logical_input(TheInputEvent *input, CHARTYPE *scrn,
                                     int *w)
{
   CHARTYPE hit_screen = current_screen;
   int hit_window = WINDOW_ALL;
   int row = -1;
   int col = -1;
   int cell = -1;
   LINETYPE line_number = 0;
   TheMouseHitArea area;

   if (!mouse_locate_window(&hit_screen, &hit_window, &row, &col))
   {
      if (scrn != NULL)
         *scrn = hit_screen;
      if (w != NULL)
         *w = hit_window;
      if (input != NULL)
         *input = the_input_event_none();
      return FALSE;
   }

   area = mouse_area_from_window(hit_window);
   line_number = mouse_line_number_for_hit(hit_screen, area, row);
   cell = mouse_cell_for_hit(hit_screen, area, row, col);
   if (scrn != NULL)
      *scrn = hit_screen;
   if (w != NULL)
      *w = hit_window;
   if (!the_mouse_hit_event_from_area(area, line_number, row, cell,
                                      hit_screen, hit_window, input))
   {
      if (input != NULL)
         *input = the_input_event_none();
      return FALSE;
   }
   return TRUE;
}

/***********************************************************************/
short THEMouse(CHARTYPE *params)
/***********************************************************************/
{
   int w=0;
   CHARTYPE scrn=0;
   short rc=RC_OK;
   int curr_button_action=0;
   int curr_button_modifier=0;
   int curr_button=0;
   int key=0;
   int saved_mouse_x=-1;
   int saved_mouse_y=-1;
   TheInputEvent input;

   TRACE_FUNCTION( "mouse.c:  THEMouse" );
   if (!curses_driver_read_mouse_button(&curr_button,&curr_button_action,
                                        &curr_button_modifier))
   {
      mouse_trace_message("THEMouse-invalid", "rc=%d", RC_INVALID_OPERAND);
      TRACE_RETURN();
      return(RC_INVALID_OPERAND);
   }
   curses_driver_saved_mouse_position(&saved_mouse_y, &saved_mouse_x);
   if (!mouse_build_logical_input(&input, &scrn, &w))
   {
      last_mouse_input = the_input_event_none();
      key = MOUSE_INFO_TO_KEY(WINDOW_ALL, curr_button, curr_button_action,
                              curr_button_modifier);
      mouse_trace_message("THEMouse-target",
                          "unsupported screen=%d window=%d saved_x=%d saved_y=%d key=0x%x",
                          scrn,w,saved_mouse_x,saved_mouse_y,key);
      rc = execute_mouse_commands(key);
      TRACE_RETURN();
      return(rc);
   }
   last_mouse_input = input;
   w = input.target.window_id;
   key = MOUSE_INFO_TO_KEY(w,curr_button,curr_button_action,curr_button_modifier);
//fprintf(stderr, "%s %d:THEMouse button: %d button_action: %d button_modifier: %d window: %d x: %d y: %d key: %x\n",__FILE__,__LINE__,curr_button,curr_button_action,curr_button_modifier,w,Mouse_status.x, Mouse_status.y,key );
   mouse_trace_message("THEMouse-target",
                       "kind=%s line=%ld row=%d cell=%d screen=%d window=%d saved_x=%d saved_y=%d",
                       the_input_logical_target_kind_name(input.target.kind),
                       (long)input.target.line_number,input.target.row,
                       input.target.cell,input.target.screen,
                       input.target.window_id,saved_mouse_x,
                       saved_mouse_y);
   rc = execute_mouse_commands(key);
   mouse_trace_message("THEMouse-dispatch",
                       "rc=%d window=%d button=%d action=%d modifier=%d key=0x%x",
                       rc,w,curr_button,curr_button_action,
                       curr_button_modifier,key);
   TRACE_RETURN();
   return(rc);
}
/***********************************************************************/
void which_window_is_mouse_in(CHARTYPE *scrn,int *w)
/***********************************************************************/
{
   int y=0,x=0;

   TRACE_FUNCTION("mouse.c:  which_window_is_mouse_in");
   if (mouse_locate_window(scrn, w, &y, &x))
   {
      TRACE_RETURN();
      return;
   }
   /*
    * To get here, the mouse is NOT in ANY window. Return an error.
    */
   if (scrn != NULL)
      *scrn = current_screen;
   if (w != NULL)
      *w = WINDOW_ALL /* was (-1) */;
   TRACE_RETURN();
   return;
}
/***********************************************************************/
void reset_saved_mouse_pos(void)
/***********************************************************************/
{
   TRACE_FUNCTION("mouse.c:  reset_saved_mouse_pos");
   curses_driver_reset_mouse_position();
   last_mouse_input = the_input_event_none();
   TRACE_RETURN();
   return;
}
/***********************************************************************/
void get_saved_mouse_pos(int *y, int *x)
/***********************************************************************/
{
   TRACE_FUNCTION("mouse.c:  get_saved_mouse_pos");
   curses_driver_saved_mouse_position(y, x);
   TRACE_RETURN();
   return;
}
/***********************************************************************/
int get_saved_mouse_target(TheInputLogicalTarget *target)
/***********************************************************************/
{
   TRACE_FUNCTION("mouse.c:  get_saved_mouse_target");
   if (target != NULL
   &&  last_mouse_input.kind == THE_INPUT_LOGICAL_HIT)
   {
      *target = last_mouse_input.target;
      TRACE_RETURN();
      return TRUE;
   }
   TRACE_RETURN();
   return FALSE;
}
/***********************************************************************/
void initialise_mouse_commands(void)
/***********************************************************************/
{
   TRACE_FUNCTION("mouse.c:   initialise_mouse_commands");

   /*
    * Default mouse actions in FILEAREA
    */
   add_define(&first_mouse_define,&last_mouse_define,
            WINDOW_FILEAREA|MOUSE_LEFT|MOUSE_CLICK|MOUSE_NORMAL,
            (CHARTYPE *)"CURSOR MOUSE",FALSE,FALSE,0);
#if defined(PDCURSES_MOUSE_ENABLED)
   add_define(&first_mouse_define,&last_mouse_define,
            WINDOW_FILEAREA|THE_MOUSE_WHEEL_UP|MOUSE_SCROLLED,
            (CHARTYPE *)"BACK 5 LINES",FALSE,FALSE,0);
   add_define(&first_mouse_define,&last_mouse_define,
            WINDOW_PREFIX|THE_MOUSE_WHEEL_UP|MOUSE_SCROLLED,
            (CHARTYPE *)"BACK 5 LINES",FALSE,FALSE,0);
   add_define(&first_mouse_define,&last_mouse_define,
            WINDOW_FILEAREA|THE_MOUSE_WHEEL_DOWN|MOUSE_SCROLLED,
            (CHARTYPE *)"FOR 5 LINES",FALSE,FALSE,0);
   add_define(&first_mouse_define,&last_mouse_define,
            WINDOW_PREFIX|THE_MOUSE_WHEEL_DOWN|MOUSE_SCROLLED,
            (CHARTYPE *)"FOR 5 LINES",FALSE,FALSE,0);
   add_define(&first_mouse_define,&last_mouse_define,
            WINDOW_FILEAREA|THE_MOUSE_WHEEL_LEFT|MOUSE_SCROLLED,
            (CHARTYPE *)"LEFT 5",FALSE,FALSE,0);
   add_define(&first_mouse_define,&last_mouse_define,
            WINDOW_FILEAREA|THE_MOUSE_WHEEL_RIGHT|MOUSE_SCROLLED,
            (CHARTYPE *)"RIGHT 5",FALSE,FALSE,0);
#endif
   add_define(&first_mouse_define,&last_mouse_define,
            WINDOW_FILEAREA|MOUSE_LEFT|MOUSE_PRESS|MOUSE_SHIFT,
            (CHARTYPE *)"CURSOR MOUSE#RESET BLOCK#MARK LINE",FALSE,FALSE,0);
   add_define(&first_mouse_define,&last_mouse_define,
            WINDOW_FILEAREA|MOUSE_LEFT|MOUSE_PRESS|MOUSE_CONTROL,
            (CHARTYPE *)"CURSOR MOUSE#RESET BLOCK#MARK BOX",FALSE,FALSE,0);
   add_define(&first_mouse_define,&last_mouse_define,
            WINDOW_FILEAREA|MOUSE_LEFT|MOUSE_DRAG|MOUSE_SHIFT,
            (CHARTYPE *)"CURSOR MOUSE#MARK LINE",FALSE,FALSE,0);
   add_define(&first_mouse_define,&last_mouse_define,
            WINDOW_FILEAREA|MOUSE_LEFT|MOUSE_DRAG|MOUSE_CONTROL,
            (CHARTYPE *)"CURSOR MOUSE#MARK BOX",FALSE,FALSE,0);
   add_define(&first_mouse_define,&last_mouse_define,
            WINDOW_FILEAREA|MOUSE_RIGHT|MOUSE_PRESS|MOUSE_SHIFT,
            (CHARTYPE *)"CURSOR MOUSE#MARK LINE",FALSE,FALSE,0);
   add_define(&first_mouse_define,&last_mouse_define,
            WINDOW_FILEAREA|MOUSE_RIGHT|MOUSE_PRESS|MOUSE_CONTROL,
            (CHARTYPE *)"CURSOR MOUSE#MARK BOX",FALSE,FALSE,0);
   add_define(&first_mouse_define,&last_mouse_define,
            WINDOW_FILEAREA|MOUSE_RIGHT|MOUSE_CLICK|MOUSE_NORMAL,
            (CHARTYPE *)"CURSOR MOUSE#SOS MAKECURR",FALSE,FALSE,0);
   add_define(&first_mouse_define,&last_mouse_define,
            WINDOW_FILEAREA|MOUSE_LEFT|MOUSE_DRAG|MOUSE_SHIFT,
            (CHARTYPE *)"CURSOR MOUSE#MARK LINE",FALSE,FALSE,0);
   add_define(&first_mouse_define,&last_mouse_define,
            WINDOW_FILEAREA|MOUSE_LEFT|MOUSE_DRAG|MOUSE_CONTROL,
            (CHARTYPE *)"CURSOR MOUSE#MARK BOX",FALSE,FALSE,0);
   add_define(&first_mouse_define,&last_mouse_define,
            WINDOW_FILEAREA|MOUSE_LEFT|MOUSE_DOUBLE_CLICK|MOUSE_NORMAL,
            (CHARTYPE *)"CURSOR MOUSE#SOS EDIT",FALSE,FALSE,0);
   add_define(&first_mouse_define,&last_mouse_define,
            WINDOW_FILEAREA|MOUSE_LEFT|MOUSE_CLICK|MOUSE_ALT,
            (CHARTYPE *)"BACKWARD",FALSE,FALSE,0);
   add_define(&first_mouse_define,&last_mouse_define,
            WINDOW_FILEAREA|MOUSE_RIGHT|MOUSE_CLICK|MOUSE_ALT,
            (CHARTYPE *)"FORWARD",FALSE,FALSE,0);
   /*
    * Default mouse actions in PREFIX area
    */
   add_define(&first_mouse_define,&last_mouse_define,
            WINDOW_PREFIX|MOUSE_LEFT|MOUSE_CLICK|MOUSE_NORMAL,
            (CHARTYPE *)"CURSOR MOUSE",FALSE,FALSE,0);
   add_define(&first_mouse_define,&last_mouse_define,
            WINDOW_PREFIX|MOUSE_RIGHT|MOUSE_CLICK|MOUSE_NORMAL,
            (CHARTYPE *)"CURSOR MOUSE#SOS MAKECURR",FALSE,FALSE,0);
   add_define(&first_mouse_define,&last_mouse_define,
            WINDOW_PREFIX|MOUSE_LEFT|MOUSE_DOUBLE_CLICK|MOUSE_NORMAL,
            (CHARTYPE *)"CURSOR MOUSE#SOS EDIT",FALSE,FALSE,0);
   /*
    * Default mouse actions in COMMAND line
    */
   add_define(&first_mouse_define,&last_mouse_define,
            WINDOW_COMMAND|MOUSE_LEFT|MOUSE_CLICK|MOUSE_NORMAL,
            (CHARTYPE *)"CURSOR MOUSE",FALSE,FALSE,0);
   /*
    * Default mouse actions in STATAREA
    */
   add_define(&first_mouse_define,&last_mouse_define,
            WINDOW_STATAREA|MOUSE_LEFT|MOUSE_CLICK|MOUSE_NORMAL,
            (CHARTYPE *)"STATUS",FALSE,FALSE,0);
/*
 * Default mouse actions in FILETABS
 */
   add_define( &first_mouse_define, &last_mouse_define,
               WINDOW_FILETABS|MOUSE_LEFT|MOUSE_CLICK|MOUSE_NORMAL,
               (CHARTYPE *)"TABFILE", FALSE, FALSE, 0 );
   /*
    * Default mouse actions in IDLINE
    */
   add_define(&first_mouse_define,&last_mouse_define,
            WINDOW_IDLINE|MOUSE_LEFT|MOUSE_CLICK|MOUSE_NORMAL,
            (CHARTYPE *)"XEDIT",FALSE,FALSE,0);
   add_define(&first_mouse_define,&last_mouse_define,
            WINDOW_IDLINE|MOUSE_RIGHT|MOUSE_CLICK|MOUSE_NORMAL,
            (CHARTYPE *)"XEDIT -",FALSE,FALSE,0);
   /*
    * Default mouse actions in DIVIDER
    */
   add_define(&first_mouse_define,&last_mouse_define,
            WINDOW_DIVIDER|MOUSE_LEFT|MOUSE_CLICK|MOUSE_NORMAL,
            (CHARTYPE *)"SCREEN 1",FALSE,FALSE,0);

 TRACE_RETURN();
 return;
}
/***********************************************************************/
int mouse_info_to_key(int w, int button, int button_action, int button_modifier)
/***********************************************************************/
{
   TRACE_FUNCTION("mouse.c:   mouse_info_to_key");

   TRACE_RETURN();
   return(MOUSE_INFO_TO_KEY(w,button,button_action,button_modifier));
}
#endif
/***********************************************************************/
CHARTYPE *mouse_key_number_to_name(int key_number, CHARTYPE *key_name, int *shift)
/***********************************************************************/
{
   register int i=0;
   int w=0,b=0,ba=0,bm=0;
   CHARTYPE *win_name=(CHARTYPE *)"*** unknown ***";

   TRACE_FUNCTION("mouse.c:   mouse_key_number_to_name");
   w = MOUSE_WINDOW_MASK(key_number);
   b = (MOUSE_BUTTON_MASK(key_number)>>MOUSE_BUTTON_OFFSET);
   ba = (MOUSE_ACTION_MASK(key_number)>>MOUSE_ACTION_OFFSET);
   bm = (MOUSE_MODIFIER_MASK(key_number)>>MOUSE_MODIFIER_OFFSET);
   *shift = bm;
   if ( w == WINDOW_ALL )
      win_name = (CHARTYPE *)"*";
   else
   {
      for ( i = 0; i < ATTR_MAX; i++ )
      {
         if ( w == valid_areas[i].area_window
         &&   valid_areas[i].actual_window )
         {
            win_name = valid_areas[i].area;
            break;
         }
      }
   }
   sprintf( (DEFCHAR *)key_name, "%s%s%s in %s", button_modifier_names[bm], button_action_names[ba], button_names[b], win_name );
   TRACE_RETURN();
   return( key_name );
}

/***********************************************************************/
int find_mouse_key_value( CHARTYPE *mnemonic )
/***********************************************************************/
/*   Function: find the matching mouse key value for the supplied name */
/* Parameters:                                                         */
/*   mnemonic: the key name to be matched                              */
/*   win_name: the window to be matched                                */
/*    Returns: the mouse button, action and modifier or -1 if error    */
/***********************************************************************/
{
   int key=0,len=0;
   int b=0,ba=0,bm=0;
   CHARTYPE tmp_buf[6];

   TRACE_FUNCTION("mouse.c:   find_mouse_key_value");
   /*
    * Parse the mnemonic for a valid mouse key definition...
    */
   len = strlen((DEFCHAR *)mnemonic);
   if (len == 3)
   {
      strcpy((DEFCHAR *)tmp_buf,"N-");
      strcat((DEFCHAR *)tmp_buf,(DEFCHAR *)mnemonic);
   }
   else
   {
      if (len == 5)
      {
         strcpy((DEFCHAR *)tmp_buf,(DEFCHAR *)mnemonic);
      }
      else
      {
         display_error(1,mnemonic,FALSE);
         TRACE_RETURN();
         return(-1);
      }
   }
   if (tmp_buf[1] != '-'
   || ( tmp_buf[4] != 'B'
      && tmp_buf[4] != 'b'
#if defined(PDCURSES_MOUSE_ENABLED)
      && tmp_buf[4] != 'W'
      && tmp_buf[4] != 'w'
#endif
      ))
   {
      display_error(1,mnemonic,FALSE);
      TRACE_RETURN();
      return(-1);
   }
   /*
    * Validate button modifier
    */
   switch(tmp_buf[0])
   {
      case 'N':
      case 'n':
         bm = 0;
         break;
      case 'S':
      case 's':
         bm = MOUSE_SHIFT;
         break;
      case 'C':
      case 'c':
         bm = MOUSE_CONTROL;
         break;
      case 'A':
      case 'a':
         bm = MOUSE_ALT;
         break;
      default:
         display_error(1,mnemonic,FALSE);
         TRACE_RETURN();
         return(-1);
         break;
   }
   /*
    * Validate button action
    */
   switch(tmp_buf[2])
   {
      case 'P':
      case 'p':
         ba = MOUSE_PRESS;
         break;
      case 'C':
      case 'c':
         ba = MOUSE_CLICK;
         break;
      case 'R':
      case 'r':
         ba = MOUSE_RELEASE;
         break;
      case '2':
         ba = MOUSE_DOUBLE_CLICK;
         break;
      case 'D':
      case 'd':
         ba = MOUSE_DRAG;
         break;
#if defined(PDCURSES_MOUSE_ENABLED)
      case 'S':
      case 's':
         ba = MOUSE_SCROLLED;
         break;
#endif
      default:
         display_error(1,mnemonic,FALSE);
         TRACE_RETURN();
         return(-1);
         break;
   }
   /*
    * Validate button number
    */
   switch(tmp_buf[3])
   {
      case 'L':
      case 'l':
#if defined(PDCURSES_MOUSE_ENABLED)
         if ( ba == MOUSE_SCROLLED )
            b = THE_MOUSE_WHEEL_LEFT;
         else
#endif
            b = MOUSE_LEFT;
         break;
      case 'R':
      case 'r':
#if defined(PDCURSES_MOUSE_ENABLED)
         if ( ba == MOUSE_SCROLLED )
            b = THE_MOUSE_WHEEL_RIGHT;
         else
#endif
            b = MOUSE_RIGHT;
         break;
      case 'M':
      case 'm':
         b = MOUSE_MIDDLE;
         break;
#if defined(PDCURSES_MOUSE_ENABLED)
      case 'U':
      case 'u':
         b = THE_MOUSE_WHEEL_UP;
         break;
      case 'D':
      case 'd':
         b = THE_MOUSE_WHEEL_DOWN;
         break;
#endif
      default:
         display_error(1,mnemonic,FALSE);
         TRACE_RETURN();
         return(-1);
         break;
   }
   key = b|ba|bm;
   TRACE_RETURN();
   return(key);
}

/***********************************************************************/
int find_mouse_key_value_in_window(CHARTYPE *mnemonic,CHARTYPE *win_name)
/***********************************************************************/
/*   Function: find the matching mouse key value for the supplied name */
/*             in the specified window.                                */
/* Parameters:                                                         */
/*   mnemonic: the key name to be matched                              */
/*   win_name: the window to be matched                                */
/*    Returns: the mouse button, action, modifier and window           */
/*             or -1 if error.                                         */
/***********************************************************************/
{
   register short i=0;
   int w=(-1),key=0;
   int mb;

   TRACE_FUNCTION("mouse.c:   find_mouse_key_value_in_window");
   /*
    * Parse the mnemonic for a valid mouse key definition...
    */
   mb = find_mouse_key_value( mnemonic );
   if ( mb == (-1) )
   {
      TRACE_RETURN();
      return mb;
   }
   /*
    * Find a valid window name for win_name...
    */
#if defined(PDCURSES_MOUSE_ENABLED)
   if ( strcmp( "*", (DEFCHAR *)win_name ) == 0 )
      w = WINDOW_ALL;
   else
#endif
   {
      for (i=0;i<ATTR_MAX;i++)
      {
         if (equal(valid_areas[i].area,win_name,valid_areas[i].area_min_len))
         {
            w = valid_areas[i].area_window;
            break;
         }
      }
   }
   if (w == (-1))
   {
      display_error(1,win_name,FALSE);
      TRACE_RETURN();
      return(-1);
   }
   key = w | mb;
   TRACE_RETURN();
   return(key);
}

#if defined(HAVE_SB_INIT)
/***********************************************************************/
short ScrollbarHorz(CHARTYPE *params)
/***********************************************************************/
{
   int cur=0;
   short rc=RC_OK;

   TRACE_FUNCTION("mouse.c:   ScrollbarHorz");
   /*
    * Parse the mnemonic for a valid mouse key definition...
    */
   sb_get_horz(NULL,NULL,&cur);
   if (cur < CURRENT_VIEW->verify_col)
      rc = Left((CHARTYPE*)"FULL");
   else
      rc = Right((CHARTYPE*)"FULL");
   TRACE_RETURN();
   return(rc);
}
/***********************************************************************/
short ScrollbarVert(CHARTYPE *params)
/***********************************************************************/
{
   int cur=0;
   short rc=RC_OK;

   TRACE_FUNCTION("mouse.c:   ScrollbarVert");
   /*
    * Parse the mnemonic for a valid mouse key definition...
    */
   sb_get_vert( NULL, NULL, &cur );
   rc = execute_makecurr( current_screen, CURRENT_VIEW, (LINETYPE)cur );
   TRACE_RETURN();
   return(rc);
}
#endif
