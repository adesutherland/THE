/*
 * QUERY1.C - Functions related to QUERY,STATUS and EXTRACT - A-L
 *
 *
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

#include "cursesdriver.h"
#include <query.h>

short extract_point_settings(short,CHARTYPE *);
short extract_prefix_settings(short,CHARTYPE *);
short extract_colour_settings(short,CHARTYPE *,CHARTYPE,CHARTYPE *,bool,bool);
short extract_autocolour_settings(short,CHARTYPE *,CHARTYPE,CHARTYPE *,bool);
void get_etmode(CHARTYPE *,CHARTYPE *);
short set_boolean_value(bool flag, short num);
short set_on_off_value(bool flag, short num);
void set_key_values(int key, bool mouse_key);
THE_PPC *in_range( THE_PPC *found_ppc, THE_PPC *curr_ppc, LINETYPE first_in_range, LINETYPE last_in_range );

extern CHARTYPE _THE_FAR *block_name[];

extern CHARTYPE query_num1[50];
extern CHARTYPE query_num2[50];
extern CHARTYPE query_num3[50];
extern CHARTYPE query_num4[50];
extern CHARTYPE query_num5[50];
extern CHARTYPE query_num6[50];
extern CHARTYPE query_num7[50];
extern CHARTYPE query_num8[50];
extern CHARTYPE query_rsrvd[MAX_FILE_NAME+1];
static LINE *curr;

static int query1_capture_active_driver_cursor(CHARTYPE window_id,
                                               short *y, short *x)
{
   CursesDriverWindowCursor cursor;
   WINDOW *win;

   if (y != NULL)
      *y = 0;
   if (x != NULL)
      *x = 0;
   if (window_id < WINDOW_FILEAREA || window_id > WINDOW_FILETABS)
      return FALSE;
   if (!curses_started)
      return FALSE;
   /*
    * This is the active-driver query point for legacy physical cursor
    * snapshots. Curses is the only provider here today; keep callers named
    * for the active driver so this can become a real dispatch point later.
    */
   win = CURRENT_SCREEN.win[window_id];
   if (win == NULL)
      return FALSE;
   cursor = curses_driver_capture_window_cursor(win);
   if (!cursor.valid)
      return FALSE;
   if (y != NULL)
      *y = cursor.row;
   if (x != NULL)
      *x = cursor.col;
   return TRUE;
}

static LENGTHTYPE query1_text_end_cell(const CHARTYPE *line, LENGTHTYPE len)
{
   if (len < 0)
      len = 0;
#ifdef USE_UTF8
   {
      TextPos end = textpos_from_byte(line, (size_t)len, (size_t)len);

      return (LENGTHTYPE)end.cell_column;
   }
#else
   INTENTIONALLY_UNUSED_VARIABLE(line);
   return len;
#endif
}

static LENGTHTYPE query1_text_first_nonblank_cell(CHARTYPE *line,
                                                  LENGTHTYPE len)
{
   LENGTHTYPE first;

   if (len <= 0)
      return -1;
   first = memne(line, ' ', len);
#ifdef USE_UTF8
   if (first >= 0)
   {
      TextPos pos = textpos_from_byte(line, (size_t)len, (size_t)first);
      first = (LENGTHTYPE)pos.cell_column;
   }
#endif
   return first;
}

static LENGTHTYPE query1_text_byte_from_cell(const CHARTYPE *line,
                                             LENGTHTYPE len,
                                             LENGTHTYPE cell)
{
   if (len <= 0 || cell <= 0)
      return 0;
#ifdef USE_UTF8
   {
      TextPos pos = textpos_from_cell(line, (size_t)len, (int)cell,
                                      TEXT_SNAP_BACKWARD);

      return (LENGTHTYPE)pos.byte_offset;
   }
#else
   INTENTIONALLY_UNUSED_VARIABLE(line);
   return (cell > len) ? len : cell;
#endif
}

static LENGTHTYPE query1_text_cell_from_byte(const CHARTYPE *line,
                                             LENGTHTYPE len,
                                             LENGTHTYPE byte_offset)
{
   if (len <= 0 || byte_offset <= 0)
      return 0;
   if (byte_offset > len)
      byte_offset = len;
#ifdef USE_UTF8
   {
      TextPos pos = textpos_from_byte(line, (size_t)len,
                                      (size_t)byte_offset);

      return (LENGTHTYPE)pos.cell_column;
   }
#else
   INTENTIONALLY_UNUSED_VARIABLE(line);
   return byte_offset;
#endif
}

static LENGTHTYPE query1_text_character_at_cell(CHARTYPE *line,
                                                LENGTHTYPE len,
                                                LENGTHTYPE cell,
                                                CHARTYPE *out,
                                                size_t out_size)
{
   LENGTHTYPE out_len;

   if (out != NULL && out_size > 0)
      out[0] = '\0';
   if (line == NULL || len <= 0 || cell < 0 || out == NULL || out_size == 0)
      return 0;
#ifdef USE_UTF8
   {
      TextPos pos = textpos_from_cell(line, (size_t)len, (int)cell,
                                      TEXT_SNAP_BACKWARD);
      TextCluster cluster = textpos_cluster_at_boundary(line, (size_t)len,
                                                        pos);

      if (!cluster.valid || cluster.byte_length == 0)
         return 0;
      out_len = (LENGTHTYPE)cluster.byte_length;
      if ((size_t)out_len >= out_size)
         out_len = (LENGTHTYPE)out_size - 1;
      memcpy(out, line + cluster.pos.byte_offset, (size_t)out_len);
   }
#else
   if (cell >= len)
      return 0;
   out_len = 1;
   out[0] = line[cell];
#endif
   out[out_len] = '\0';
   return out_len;
}

static bool query1_text_cell_is_end(const CHARTYPE *line, LENGTHTYPE len,
                                    LENGTHTYPE cell)
{
   if (len <= 0 || cell < 0)
      return FALSE;
#ifdef USE_UTF8
   {
      TextPos pos = textpos_from_cell(line, (size_t)len, (int)cell,
                                      TEXT_SNAP_BACKWARD);
      TextCluster cluster = textpos_cluster_at_boundary(line, (size_t)len,
                                                        pos);
      LENGTHTYPE end_cell = query1_text_end_cell(line, len);

      return (bool)(cluster.valid
                 && cluster.byte_length > 0
                 && cluster.end.cell_column >= end_cell);
   }
#else
   INTENTIONALLY_UNUSED_VARIABLE(line);
   return (bool)(cell == len - 1);
#endif
}

static int query1_logical_cursor_cell(LogicalCursorZone zone, int check_line,
                                      LINETYPE line_number,
                                      LENGTHTYPE *cell)
{
   LogicalCursor logical;

   logical = CURRENT_VIEW->logical_cursor.current;
   if (!logical.valid
   ||  logical.zone != zone
   ||  logical.text.cell_column < 0)
      return FALSE;
   if (check_line
   &&  logical.line_number != line_number)
      return FALSE;
   if (cell != NULL)
      *cell = (LENGTHTYPE)logical.text.cell_column;
   return TRUE;
}

static LENGTHTYPE query1_filearea_cursor_cell(void)
{
   short y=0,x=0;
   LENGTHTYPE cell;

   if (query1_logical_cursor_cell(LOGICAL_CURSOR_ZONE_FILEAREA, TRUE,
                                  CURRENT_VIEW->focus_line, &cell))
      return cell;
   if (!query1_capture_active_driver_cursor(CURRENT_VIEW->current_window,
                                            &y,&x))
      return (CURRENT_VIEW->current_column > 0)
           ? CURRENT_VIEW->current_column - 1
           : 0;
   INTENTIONALLY_UNUSED_VARIABLE(y);
#ifdef USE_UTF8
   return (LENGTHTYPE)show_utf8_logical_col_from_display(
             rec, rec_len, CURRENT_VIEW->verify_col - 1, x,
             TEXT_SNAP_BACKWARD);
#else
   return x + CURRENT_VIEW->verify_col - 1;
#endif
}

static LENGTHTYPE query1_command_cursor_cell(void)
{
   short y=0,x=0;
   LENGTHTYPE cell;

   if (query1_logical_cursor_cell(LOGICAL_CURSOR_ZONE_COMMAND, FALSE,
                                  0, &cell))
      return cell;
   if (CURRENT_VIEW->cmdline_col >= 0)
      return cmd_verify_col - 1 + CURRENT_VIEW->cmdline_col;
   if (!query1_capture_active_driver_cursor(CURRENT_VIEW->current_window,
                                            &y,&x))
      return 0;
   INTENTIONALLY_UNUSED_VARIABLE(y);
   return x + cmd_verify_col - 1;
}

static LENGTHTYPE query1_prefix_cursor_cell(void)
{
   short y=0,x=0;
   LENGTHTYPE cell;

   if (query1_logical_cursor_cell(LOGICAL_CURSOR_ZONE_PREFIX, TRUE,
                                  CURRENT_VIEW->focus_line, &cell))
      return cell;
   if (!query1_capture_active_driver_cursor(CURRENT_VIEW->current_window,
                                            &y,&x))
      return 0;
   INTENTIONALLY_UNUSED_VARIABLE(y);
   return x;
}

static int query1_current_field(CHARTYPE **line, LENGTHTYPE *len,
                                LENGTHTYPE *cell, CHARTYPE **area,
                                LENGTHTYPE *area_len)
{
   switch(CURRENT_VIEW->current_window)
   {
      case WINDOW_FILEAREA:
         if (line != NULL)
            *line = rec;
         if (len != NULL)
            *len = rec_len;
         if (cell != NULL)
            *cell = query1_filearea_cursor_cell();
         if (area != NULL)
            *area = (CHARTYPE *)"TEXT";
         if (area_len != NULL)
            *area_len = 4;
         return TRUE;
      case WINDOW_PREFIX:
         if (line != NULL)
            *line = pre_rec;
         if (len != NULL)
            *len = pre_rec_len;
         if (cell != NULL)
            *cell = query1_prefix_cursor_cell();
         if (area != NULL)
            *area = (CHARTYPE *)"PREFIX";
         if (area_len != NULL)
            *area_len = 6;
         return TRUE;
      case WINDOW_COMMAND:
         if (line != NULL)
            *line = cmd_rec;
         if (len != NULL)
            *len = cmd_rec_len;
         if (cell != NULL)
            *cell = query1_command_cursor_cell();
         if (area != NULL)
            *area = (CHARTYPE *)"COMMAND";
         if (area_len != NULL)
            *area_len = 7;
         return TRUE;
   }
   return FALSE;
}

/***********************************************************************/
short extract_after_function(short number_variables,short itemno,CHARTYPE *itemargs,CHARTYPE query_type,LINETYPE argc,CHARTYPE *arg,LINETYPE arglen)
/***********************************************************************/
{
   bool bool_flag=FALSE;

   if (batch_only)
   {
      item_values[1].value = (CHARTYPE *)"0";
      item_values[1].len = 1;
   }
   else
   {
      bool_flag = FALSE;
      switch(CURRENT_VIEW->current_window)
      {
         case WINDOW_FILEAREA:
            if (query1_filearea_cursor_cell()
            >=  query1_text_end_cell(rec, rec_len))
               bool_flag = TRUE;
            break;
         case WINDOW_COMMAND:
            if (query1_command_cursor_cell()
            >=  query1_text_end_cell(cmd_rec, cmd_rec_len))
               bool_flag = TRUE;
            break;
         case WINDOW_PREFIX:
            if (query1_prefix_cursor_cell()
            >=  query1_text_end_cell(pre_rec, pre_rec_len))
               bool_flag = TRUE;
            break;
      }
      set_boolean_value(bool_flag,1);
   }
   return 1; /* number of values set */
}

/***********************************************************************/
short extract_alt(short number_variables,short itemno,CHARTYPE *itemargs,CHARTYPE query_type,LINETYPE argc,CHARTYPE *arg,LINETYPE arglen)
/***********************************************************************/
{
   sprintf((DEFCHAR *)query_num1,"%d",CURRENT_FILE->autosave_alt);
   sprintf((DEFCHAR *)query_num2,"%d",CURRENT_FILE->save_alt);
   item_values[1].value = query_num1;
   item_values[2].value = query_num2;
   item_values[1].len = strlen((DEFCHAR *)query_num1);
   item_values[2].len = strlen((DEFCHAR *)query_num2);
   return number_variables;
}

/***********************************************************************/
short extract_alt_function(short number_variables,short itemno,CHARTYPE *itemargs,CHARTYPE query_type,LINETYPE argc,CHARTYPE *arg,LINETYPE arglen)
/***********************************************************************/
{
   return set_boolean_value((bool)(CURRENT_FILE->save_alt != 0),(short)1);
}

/***********************************************************************/
short extract_altkey_function(short number_variables,short itemno,CHARTYPE *itemargs,CHARTYPE query_type,LINETYPE argc,CHARTYPE *arg,LINETYPE arglen)
/***********************************************************************/
{
   int shift=0;

   get_key_name(lastkeys[current_key],&shift);
   return set_boolean_value((bool)(shift & SHIFT_ALT),(short)1);
}

/***********************************************************************/
short extract_batch_function(short number_variables,short itemno,CHARTYPE *itemargs,CHARTYPE query_type,LINETYPE argc,CHARTYPE *arg,LINETYPE arglen)
/***********************************************************************/
{
   return set_boolean_value((bool)batch_only,(short)1);
}

/***********************************************************************/
short extract_arbchar(short number_variables,short itemno,CHARTYPE *itemargs,CHARTYPE query_type,LINETYPE argc,CHARTYPE *arg,LINETYPE arglen)
/***********************************************************************/
{
   set_on_off_value(CURRENT_VIEW->arbchar_status,1);
   query_num1[0] = CURRENT_VIEW->arbchar_multiple;
   query_num1[1] ='\0';
   item_values[2].value = query_num1;
   item_values[2].len = 1;
   query_num2[0] = CURRENT_VIEW->arbchar_single;
   query_num2[1] ='\0';
   item_values[3].value = query_num2;
   item_values[3].len = 1;
   return number_variables;
}

/***********************************************************************/
short extract_autosave(short number_variables,short itemno,CHARTYPE *itemargs,CHARTYPE query_type,LINETYPE argc,CHARTYPE *arg,LINETYPE arglen)
/***********************************************************************/
{
   if (CURRENT_FILE->autosave == 0)
   {
      item_values[1].value = (CHARTYPE *)"OFF";
      item_values[1].len = 3;
   }
   else
   {
      sprintf((DEFCHAR *)query_num1,"%d",CURRENT_FILE->autosave);
      item_values[1].value = query_num1;
      item_values[1].len = strlen((DEFCHAR *)query_num1);
   }
   return number_variables;
}

/***********************************************************************/
short extract_autocolor(short number_variables,short itemno,CHARTYPE *itemargs,CHARTYPE query_type,LINETYPE argc,CHARTYPE *arg,LINETYPE arglen)
/***********************************************************************/
{
   return extract_autocolour_settings(itemno,query_rsrvd,query_type,itemargs,TRUE);
}
/***********************************************************************/
short extract_autocolour(short number_variables,short itemno,CHARTYPE *itemargs,CHARTYPE query_type,LINETYPE argc,CHARTYPE *arg,LINETYPE arglen)
/***********************************************************************/
{
   return extract_autocolour_settings(itemno,query_rsrvd,query_type,itemargs,FALSE);
}

/***********************************************************************/
short extract_autoscroll(short number_variables,short itemno,CHARTYPE *itemargs,CHARTYPE query_type,LINETYPE argc,CHARTYPE *arg,LINETYPE arglen)
/***********************************************************************/
{
   if (CURRENT_VIEW->autoscroll == 0)
   {
      item_values[1].value = (CHARTYPE *)"OFF";
      item_values[1].len = 3;
   }
   else if (CURRENT_VIEW->autoscroll == (-1))
   {
      item_values[1].value = (CHARTYPE *)"HALF";
      item_values[1].len = 4;
   }
   else
   {
      sprintf((DEFCHAR *)query_num1,"%ld",CURRENT_VIEW->autoscroll);
      item_values[1].value = query_num1;
      item_values[1].len = strlen((DEFCHAR *)query_num1);
   }
   return number_variables;
}

/***********************************************************************/
short extract_backup(short number_variables,short itemno,CHARTYPE *itemargs,CHARTYPE query_type,LINETYPE argc,CHARTYPE *arg,LINETYPE arglen)
/***********************************************************************/
{
   switch( CURRENT_FILE->backup )
   {
      case BACKUP_OFF:
         item_values[1].value = (CHARTYPE *)"OFF";
         break;
      case BACKUP_TEMP:
         item_values[1].value = (CHARTYPE *)"TEMP";
         break;
      case BACKUP_KEEP:
         item_values[1].value = (CHARTYPE *)"KEEP";
         break;
      case BACKUP_INPLACE:
         item_values[1].value = (CHARTYPE *)"INPLACE";
         break;
   }
   item_values[1].len = strlen( (DEFCHAR *)item_values[1].value );
   strcpy( (DEFCHAR *)query_rsrvd, (DEFCHAR *)BACKUP_SUFFIXx );
   item_values[2].value = query_rsrvd;
   item_values[2].len = strlen( (DEFCHAR *)item_values[2].value );

   return number_variables;
}

/***********************************************************************/
short extract_beep(short number_variables,short itemno,CHARTYPE *itemargs,CHARTYPE query_type,LINETYPE argc,CHARTYPE *arg,LINETYPE arglen)
/***********************************************************************/
{
   return set_on_off_value(BEEPx,1);
}

/***********************************************************************/
short extract_before_function(short number_variables,short itemno,CHARTYPE *itemargs,CHARTYPE query_type,LINETYPE argc,CHARTYPE *arg,LINETYPE arglen)
/***********************************************************************/
{
   bool bool_flag=FALSE;

   if (batch_only)
   {
      item_values[1].value = (CHARTYPE *)"0";
      item_values[1].len = 1;
   }
   else
   {
      bool_flag = FALSE;
      switch(CURRENT_VIEW->current_window)
      {
         case WINDOW_FILEAREA:
            if (query1_filearea_cursor_cell()
            <   query1_text_first_nonblank_cell(rec, rec_len))
               bool_flag = TRUE;
            break;
         case WINDOW_COMMAND:
            if (query1_command_cursor_cell()
            <   query1_text_first_nonblank_cell(cmd_rec, cmd_rec_len))
               bool_flag = TRUE;
            break;
         case WINDOW_PREFIX:/* cursor can't go before 1st non-blank */
            break;
      }
      set_boolean_value((bool)bool_flag,(short)1);
   }
   return number_variables;
}

/***********************************************************************/
short extract_blank_function(short number_variables,short itemno,CHARTYPE *itemargs,CHARTYPE query_type,LINETYPE argc,CHARTYPE *arg,LINETYPE arglen)
/***********************************************************************/
{
   item_values[1].value = (CHARTYPE *)"0"; /* FALSE by default */
   switch(CURRENT_VIEW->current_window)
   {
      case WINDOW_FILEAREA:
         if (rec_len == 0)
            item_values[1].value = (CHARTYPE *)"1";
         break;
      case WINDOW_PREFIX:
         if (pre_rec_len == 0)
            item_values[1].value = (CHARTYPE *)"1";
         break;
      case WINDOW_COMMAND:
         if (cmd_rec_len == 0)
            item_values[1].value = (CHARTYPE *)"1";
         break;
   }
   item_values[1].len = 1;
   return number_variables;
}
/***********************************************************************/
short extract_block(short number_variables,short itemno,CHARTYPE *itemargs,CHARTYPE query_type,LINETYPE argc,CHARTYPE *arg,LINETYPE arglen)
/***********************************************************************/
{
   if (MARK_VIEW == NULL)
   {
      item_values[1].value = (CHARTYPE *)"NONE";
      item_values[1].len = 4;
      number_variables = 1;
   }
   else
   {
      number_variables = 6;
      item_values[1].value = block_name[MARK_VIEW->mark_type];
      item_values[1].len = strlen((DEFCHAR *)item_values[1].value);
      sprintf((DEFCHAR *)query_num1,"%ld",MARK_VIEW->mark_start_line);
      item_values[2].value = query_num1;
      item_values[2].len = strlen((DEFCHAR *)query_num1);
      sprintf((DEFCHAR *)query_num2,"%ld",MARK_VIEW->mark_start_col);
      item_values[3].value = query_num2;
      item_values[3].len = strlen((DEFCHAR *)query_num2);
      sprintf((DEFCHAR *)query_num3,"%ld",MARK_VIEW->mark_end_line);
      item_values[4].value = query_num3;
      item_values[4].len = strlen((DEFCHAR *)query_num3);
      sprintf((DEFCHAR *)query_num4,"%ld",MARK_VIEW->mark_end_col);
      item_values[5].value = query_num4;
      item_values[5].len = strlen((DEFCHAR *)query_num4);
      strcpy((DEFCHAR *)trec,(DEFCHAR *)MARK_FILE->fpath);
      strcat((DEFCHAR *)trec,(DEFCHAR *)MARK_FILE->fname);
      item_values[6].value = (CHARTYPE *)trec;
      item_values[6].len = strlen((DEFCHAR *)trec);
   }
   return number_variables;
}
/***********************************************************************/
short extract_block_function(short number_variables,short itemno,CHARTYPE *itemargs,CHARTYPE query_type,LINETYPE argc,CHARTYPE *arg,LINETYPE arglen)
/***********************************************************************/
{
   return set_boolean_value((bool)(CURRENT_VIEW == MARK_VIEW),(short)1);
}
/***********************************************************************/
short extract_bottomedge_function(short number_variables,short itemno,CHARTYPE *itemargs,CHARTYPE query_type,LINETYPE argc,CHARTYPE *arg,LINETYPE arglen)
/***********************************************************************/
{
   short y=0,x=0;

   if (batch_only)
   {
      item_values[1].value = (CHARTYPE *)"0";
      item_values[1].len = 1;
      return 1;
   }
   query1_capture_active_driver_cursor(CURRENT_VIEW->current_window,&y,&x);
   INTENTIONALLY_UNUSED_VARIABLE(x);
   return set_boolean_value((bool)(CURRENT_VIEW->current_window == WINDOW_FILEAREA && y == CURRENT_SCREEN.rows[WINDOW_FILEAREA]-1),(short)1);
}
/***********************************************************************/
short extract_case(short number_variables,short itemno,CHARTYPE *itemargs,CHARTYPE query_type,LINETYPE argc,CHARTYPE *arg,LINETYPE arglen)
/***********************************************************************/
{
   switch(CURRENT_VIEW->case_enter)
   {
      case CASE_MIXED:
         item_values[1].value = (CHARTYPE *)"MIXED";
         break;
      case CASE_UPPER:
         item_values[1].value = (CHARTYPE *)"UPPER";
         break;
      case CASE_LOWER:
         item_values[1].value = (CHARTYPE *)"LOWER";
         break;
      default:
         break;
   }
   item_values[1].len = 5;
   switch(CURRENT_VIEW->case_locate)
   {
      case CASE_IGNORE:
         item_values[2].value = (CHARTYPE *)"IGNORE";
         item_values[2].len = 6;
         break;
      case CASE_RESPECT:
         item_values[2].value = (CHARTYPE *)"RESPECT";
         item_values[2].len = 7;
         break;
      default:
         break;
   }
   switch(CURRENT_VIEW->case_change)
   {
      case CASE_IGNORE:
         item_values[3].value = (CHARTYPE *)"IGNORE";
         item_values[3].len = 6;
         break;
      case CASE_RESPECT:
         item_values[3].value = (CHARTYPE *)"RESPECT";
         item_values[3].len = 7;
         break;
      default:
         break;
   }
   switch(CURRENT_VIEW->case_sort)
   {
      case CASE_IGNORE:
         item_values[4].value = (CHARTYPE *)"IGNORE";
         item_values[4].len = 6;
         break;
      case CASE_RESPECT:
         item_values[4].value = (CHARTYPE *)"RESPECT";
         item_values[4].len = 7;
         break;
      default:
         break;
   }
   item_values[5].len = 5;
   switch(CURRENT_VIEW->case_enter_cmdline)
   {
      case CASE_MIXED:
         item_values[5].value = (CHARTYPE *)"MIXED";
         break;
      case CASE_UPPER:
         item_values[5].value = (CHARTYPE *)"UPPER";
         break;
      case CASE_LOWER:
         item_values[5].value = (CHARTYPE *)"LOWER";
         break;
      default:
         break;
   }
   item_values[6].len = 5;
   switch(CURRENT_VIEW->case_enter_prefix)
   {
      case CASE_MIXED:
         item_values[6].value = (CHARTYPE *)"MIXED";
         break;
      case CASE_UPPER:
         item_values[6].value = (CHARTYPE *)"UPPER";
         break;
      case CASE_LOWER:
         item_values[6].value = (CHARTYPE *)"LOWER";
         break;
      default:
         break;
   }
   return number_variables;
}
/***********************************************************************/
short extract_clearerrorkey(short number_variables,short itemno,CHARTYPE *itemargs,CHARTYPE query_type,LINETYPE argc,CHARTYPE *arg,LINETYPE arglen)
/***********************************************************************/
{
   int dummy=0;

   if (CLEARERRORKEYx == -1)
   {
      item_values[1].value = (CHARTYPE *)"*";
      item_values[1].len = 1;
   }
   else
   {
      item_values[1].value = get_key_name(CLEARERRORKEYx,&dummy);
      item_values[1].len = strlen((DEFCHAR*)item_values[1].value);
   }
   return number_variables;
}
/***********************************************************************/
short extract_clearscreen(short number_variables,short itemno,CHARTYPE *itemargs,CHARTYPE query_type,LINETYPE argc,CHARTYPE *arg,LINETYPE arglen)
/***********************************************************************/
{
   return set_on_off_value(CLEARSCREENx,1);
}
/***********************************************************************/
short extract_clock(short number_variables,short itemno,CHARTYPE *itemargs,CHARTYPE query_type,LINETYPE argc,CHARTYPE *arg,LINETYPE arglen)
/***********************************************************************/
{
   return set_on_off_value(CLOCKx,1);
}
/***********************************************************************/
short extract_command_function(short number_variables,short itemno,CHARTYPE *itemargs,CHARTYPE query_type,LINETYPE argc,CHARTYPE *arg,LINETYPE arglen)
/***********************************************************************/
{
   return set_boolean_value((bool)(CURRENT_WINDOW_COMMAND != NULL),(short)1);
}
/***********************************************************************/
short extract_cmdarrows(short number_variables,short itemno,CHARTYPE *itemargs,CHARTYPE query_type,LINETYPE argc,CHARTYPE *arg,LINETYPE arglen)
/***********************************************************************/
{
   if (CMDARROWSTABCMDx)
   {
      item_values[1].value = (CHARTYPE *)"TAB";
      item_values[1].len = 3;
   }
   else
   {
      item_values[1].value = (CHARTYPE *)"RETRIEVE";
      item_values[1].len = 8;
   }
   return number_variables;
}
/***********************************************************************/
short extract_cmdline(short number_variables,short itemno,CHARTYPE *itemargs,CHARTYPE query_type,LINETYPE argc,CHARTYPE *arg,LINETYPE arglen)
/***********************************************************************/
{
   switch(CURRENT_VIEW->cmd_line)
   {
      case 'B':
         item_values[1].value = (CHARTYPE *)"BOTTOM";
         item_values[1].len = 6;
         break;
      case 'T':
         item_values[1].value = (CHARTYPE *)"TOP";
         item_values[1].len = 3;
         break;
      case 'O':
         item_values[1].value = (CHARTYPE *)"OFF";
         item_values[1].len = 3;
         number_variables = 1;
         break;
   }
   if (CURRENT_VIEW->cmd_line == 'O')
      return number_variables;

   if (query_type == QUERY_EXTRACT
   ||  query_type == QUERY_FUNCTION)
   {
      sprintf((DEFCHAR *)query_num1,"%d",CURRENT_SCREEN.start_row[WINDOW_COMMAND]+1);
      item_values[2].value = query_num1;
      item_values[2].len = strlen((DEFCHAR *)query_num1);
      item_values[3].value = cmd_rec;
      item_values[3].len = cmd_rec_len;
      number_variables = 3;
   }
   else
      number_variables = 1;
   return number_variables;
}
/***********************************************************************/
short extract_color(short number_variables,short itemno,CHARTYPE *itemargs,CHARTYPE query_type,LINETYPE argc,CHARTYPE *arg,LINETYPE arglen)
/***********************************************************************/
{
   return extract_colour_settings(itemno,query_rsrvd,query_type,itemargs,TRUE,FALSE);
}
/***********************************************************************/
short extract_colour(short number_variables,short itemno,CHARTYPE *itemargs,CHARTYPE query_type,LINETYPE argc,CHARTYPE *arg,LINETYPE arglen)
/***********************************************************************/
{
   return extract_colour_settings(itemno,query_rsrvd,query_type,itemargs,FALSE,FALSE);
}
/***********************************************************************/
short extract_coloring(short number_variables,short itemno,CHARTYPE *itemargs,CHARTYPE query_type,LINETYPE argc,CHARTYPE *arg,LINETYPE arglen)
/***********************************************************************/
{
   if (CURRENT_FILE->colouring)
   {
      item_values[1].value = (CHARTYPE *)"ON";
      item_values[1].len = 2;
      if (CURRENT_FILE->parser)
      {
         item_values[3].value = CURRENT_FILE->parser->parser_name;
         item_values[3].len = strlen((DEFCHAR *)item_values[3].value);
      }
      else
      {
         item_values[3].value = (CHARTYPE *)"NULL";
         item_values[3].len = 4;
      }
      if (CURRENT_FILE->autocolour)
      {
         item_values[2].value = (CHARTYPE *)"AUTO";
         item_values[2].len = 4;
      }
      else
      {
         item_values[2].value = CURRENT_FILE->parser->parser_name;
         item_values[2].len = strlen((DEFCHAR *)item_values[2].value);
      }
   }
   else
   {
      item_values[1].value = (CHARTYPE *)"OFF";
      item_values[1].len = 3;
      number_variables = 1;
   }
   return number_variables;
}
/***********************************************************************/
short extract_colouring(short number_variables,short itemno,CHARTYPE *itemargs,CHARTYPE query_type,LINETYPE argc,CHARTYPE *arg,LINETYPE arglen)
/***********************************************************************/
{
   if (CURRENT_FILE->colouring)
   {
      item_values[1].value = (CHARTYPE *)"ON";
      item_values[1].len = 2;
      if (CURRENT_FILE->parser)
      {
         item_values[3].value = CURRENT_FILE->parser->parser_name;
         item_values[3].len = strlen((DEFCHAR *)item_values[3].value);
      }
      else
      {
         item_values[3].value = (CHARTYPE *)"NULL";
         item_values[3].len = 4;
      }
      if (CURRENT_FILE->autocolour)
      {
         item_values[2].value = (CHARTYPE *)"AUTO";
         item_values[2].len = 4;
      }
      else
      {
         item_values[2].value = CURRENT_FILE->parser->parser_name;
         item_values[2].len = strlen((DEFCHAR *)item_values[2].value);
      }
   }
   else
   {
      item_values[1].value = (CHARTYPE *)"OFF";
      item_values[1].len = 3;
      number_variables = 1;
   }
   return number_variables;
}
/***********************************************************************/
short extract_column(short number_variables,short itemno,CHARTYPE *itemargs,CHARTYPE query_type,LINETYPE argc,CHARTYPE *arg,LINETYPE arglen)
/***********************************************************************/
{
   if (batch_only
   ||  CURRENT_VIEW->current_window != WINDOW_FILEAREA)
      sprintf((DEFCHAR *)query_num1,"%ld",CURRENT_VIEW->current_column);
   else
      sprintf((DEFCHAR *)query_num1,"%ld",
              query1_filearea_cursor_cell() + 1);
   item_values[1].value = query_num1;
   item_values[1].len = strlen((DEFCHAR *)query_num1);
   return number_variables;
}
/***********************************************************************/
short extract_compat(short number_variables,short itemno,CHARTYPE *itemargs,CHARTYPE query_type,LINETYPE argc,CHARTYPE *arg,LINETYPE arglen)
/***********************************************************************/
{
   switch(compatible_look)
   {
      case COMPAT_THE:
         item_values[1].value = (CHARTYPE *)"THE";
         item_values[1].len = 3;
         break;
      case COMPAT_XEDIT:
         item_values[1].value = (CHARTYPE *)"XEDIT";
         item_values[1].len = 5;
         break;
      case COMPAT_ISPF:
         item_values[1].value = (CHARTYPE *)"ISPF";
         item_values[1].len = 4;
         break;
      case COMPAT_KEDIT:
         item_values[1].value = (CHARTYPE *)"KEDIT";
         item_values[1].len = 5;
         break;
      case COMPAT_KEDITW:
         item_values[1].value = (CHARTYPE *)"KEDITW";
         item_values[1].len = 6;
         break;
   }
   switch(compatible_feel)
   {
      case COMPAT_THE:
         item_values[2].value = (CHARTYPE *)"THE";
         item_values[2].len = 3;
         break;
      case COMPAT_XEDIT:
         item_values[2].value = (CHARTYPE *)"XEDIT";
         item_values[2].len = 5;
         break;
      case COMPAT_ISPF:
         item_values[2].value = (CHARTYPE *)"ISPF";
         item_values[2].len = 4;
         break;
      case COMPAT_KEDIT:
         item_values[2].value = (CHARTYPE *)"KEDIT";
         item_values[2].len = 5;
         break;
      case COMPAT_KEDITW:
         item_values[2].value = (CHARTYPE *)"KEDITW";
         item_values[2].len = 6;
         break;
   }
   switch(compatible_keys)
   {
      case COMPAT_THE:
         item_values[3].value = (CHARTYPE *)"THE";
         item_values[3].len = 3;
         break;
      case COMPAT_XEDIT:
         item_values[3].value = (CHARTYPE *)"XEDIT";
         item_values[3].len = 5;
         break;
      case COMPAT_ISPF:
         item_values[3].value = (CHARTYPE *)"ISPF";
         item_values[3].len = 4;
         break;
      case COMPAT_KEDIT:
         item_values[3].value = (CHARTYPE *)"KEDIT";
         item_values[3].len = 5;
         break;
      case COMPAT_KEDITW:
         item_values[3].value = (CHARTYPE *)"KEDITW";
         item_values[3].len = 6;
         break;
   }
   return number_variables;
}
/***********************************************************************/
short extract_ctlchar(short number_variables,short itemno,CHARTYPE *itemargs,CHARTYPE query_type,LINETYPE argc,CHARTYPE *arg,LINETYPE arglen)
/***********************************************************************/
{
   int i,j;
   bool found=FALSE;

   if (!CTLCHARx)
   {
      item_values[1].value = (CHARTYPE *)"OFF";
      item_values[1].len = 3;
      number_variables = 1;
   }
   else
   {
      if (itemargs == NULL
      ||  blank_field(itemargs)
      ||  strcmp((DEFCHAR*)itemargs,"*") == 0)
      {
         item_values[1].value = (CHARTYPE *)"ON";
         item_values[1].len = 2;
         query_num2[0] = ctlchar_escape;
         query_num2[1] = '\0';
         item_values[2].value = (CHARTYPE *)query_num2;
         item_values[2].len = 1;
         memset(query_rsrvd,' ',MAX_CTLCHARS*2);
         for (i=0,j=0;i<MAX_CTLCHARS;i++)
         {
            if (ctlchar_char[i] != 0)
            {
               query_rsrvd[j*2] = ctlchar_char[i];
               j++;
            }
         }
         query_rsrvd[(j*2)-1] = '\0';
         item_values[3].value = query_rsrvd;
         item_values[3].len = strlen((DEFCHAR *)query_rsrvd);
         number_variables = 3;
      }
      else
      {
         /*
          * Find the matching CTLCHAR
          */
         for (i=0;i<MAX_CTLCHARS;i++)
         {
            if (ctlchar_char[i] == itemargs[0])
            {
               CHARTYPE *attr_string;
               item_values[1].value = (ctlchar_protect[i]) ? (CHARTYPE *)"PROTECT" : (CHARTYPE *)"NOPROTECT";
               item_values[1].len = strlen( (DEFCHAR *)item_values[1].value );
               attr_string = get_colour_strings(&ctlchar_attr[i]);
               strcpy( (DEFCHAR *)query_rsrvd, (DEFCHAR *)attr_string );
               item_values[2].value = query_rsrvd;
               item_values[2].len = strlen( (DEFCHAR *)item_values[2].value );
               (*the_free)(attr_string);
               number_variables = 2;
               found = TRUE;
            }
         }
         if ( found == FALSE )
         {
            display_error( 1, (CHARTYPE *)itemargs, FALSE );
            number_variables = EXTRACT_ARG_ERROR;
         }
      }
   }
   return number_variables;
}

/***********************************************************************/
short extract_ctrl_function(short number_variables,short itemno,CHARTYPE *itemargs,CHARTYPE query_type,LINETYPE argc,CHARTYPE *arg,LINETYPE arglen)
/***********************************************************************/
{
   int shift=0;

   get_key_name(lastkeys[current_key],&shift);
   return set_boolean_value((bool)(shift & SHIFT_CTRL),(short)1);
}
/***********************************************************************/
short extract_curline(short number_variables,short itemno,CHARTYPE *itemargs,CHARTYPE query_type,LINETYPE argc,CHARTYPE *arg,LINETYPE arglen)
/***********************************************************************/
{
   if (CURRENT_VIEW->current_base == POSITION_MIDDLE)
   {
      if (CURRENT_VIEW->current_off == 0)
         strcpy((DEFCHAR *)query_rsrvd,"M");
      else
         sprintf((DEFCHAR *)query_rsrvd,"M%+d",CURRENT_VIEW->current_off);
   }
   else
      sprintf((DEFCHAR *)query_rsrvd,"%d",CURRENT_VIEW->current_off);
   item_values[1].value = query_rsrvd;
   item_values[1].len = strlen((DEFCHAR *)query_rsrvd);
   if (query_type == QUERY_EXTRACT
   ||  query_type == QUERY_FUNCTION)
   {
      sprintf((DEFCHAR *)query_num1,"%d",CURRENT_VIEW->current_row+1);
      item_values[2].value = query_num1;
      item_values[2].len = strlen((DEFCHAR *)query_num1);
      curr = lll_find(CURRENT_FILE->first_line,CURRENT_FILE->last_line,
                     (compatible_feel==COMPAT_XEDIT)?CURRENT_VIEW->current_line:get_true_line(TRUE),
                     CURRENT_FILE->number_lines);
      item_values[3].value = (CHARTYPE *)curr->line;
      item_values[3].len = curr->length;
      item_values[4].value = (curr->flags.new_flag||curr->flags.changed_flag)?(CHARTYPE *)"ON":(CHARTYPE *)"OFF";
      item_values[4].len = strlen((DEFCHAR *)item_values[4].value);
      if (curr->flags.new_flag)
      {
         if (curr->flags.changed_flag)
            item_values[5].value = (CHARTYPE *)"NEW CHANGED";
         else
            item_values[5].value = (CHARTYPE *)"NEW";
      }
      else
      {
         if (curr->flags.changed_flag)
            item_values[5].value = (CHARTYPE *)"OLD CHANGED";
         else
            item_values[5].value = (CHARTYPE *)"OLD";
      }
      item_values[5].len = strlen((DEFCHAR *)item_values[5].value);
      sprintf((DEFCHAR *)query_num2,"%d",curr->select);
      item_values[6].value = query_num2;
      item_values[6].len = strlen((DEFCHAR *)query_num2);
   }
   else
      number_variables = 1;
   return number_variables;
}
/***********************************************************************/
short extract_cursor(short number_variables,short itemno,CHARTYPE *itemargs,CHARTYPE query_type,LINETYPE argc,CHARTYPE *arg,LINETYPE arglen)
/***********************************************************************/
{
   LINETYPE current_screen_line=(-1L);
   LINETYPE current_screen_column=(-1L);
   LINETYPE current_file_line=(-1L);
   LINETYPE current_file_column=(-1L);

   get_cursor_position(&current_screen_line,&current_screen_column,
                       &current_file_line,&current_file_column);
   sprintf((DEFCHAR *)query_num1,"%ld",current_screen_line);
   item_values[1].value = query_num1;
   item_values[1].len = strlen((DEFCHAR *)query_num1);
   sprintf((DEFCHAR *)query_num2,"%ld",current_screen_column);
   item_values[2].value = query_num2;
   item_values[2].len = strlen((DEFCHAR *)query_num2);
   sprintf((DEFCHAR *)query_num3,"%ld",current_file_line);
   item_values[3].value = query_num3;
   item_values[3].len = strlen((DEFCHAR *)query_num3);
   sprintf((DEFCHAR *)query_num4,"%ld",current_file_column);
   item_values[4].value = query_num4;
   item_values[4].len = strlen((DEFCHAR *)query_num4);
   sprintf((DEFCHAR *)query_num5,"%ld",original_screen_line);
   item_values[5].value = query_num5;
   item_values[5].len = strlen((DEFCHAR *)query_num5);
   sprintf((DEFCHAR *)query_num6,"%ld",original_screen_column);
   item_values[6].value = query_num6;
   item_values[6].len = strlen((DEFCHAR *)query_num6);
   sprintf((DEFCHAR *)query_num7,"%ld",original_file_line);
   item_values[7].value = query_num7;
   item_values[7].len = strlen((DEFCHAR *)query_num7);
   sprintf((DEFCHAR *)query_num8,"%ld",original_file_column);
   item_values[8].value = query_num8;
   item_values[8].len = strlen((DEFCHAR *)query_num8);
   return number_variables;
}
/***********************************************************************/
short extract_cursorstay(short number_variables,short itemno,CHARTYPE *itemargs,CHARTYPE query_type,LINETYPE argc,CHARTYPE *arg,LINETYPE arglen)
/***********************************************************************/
{
   return set_on_off_value(scroll_cursor_stay,1);
}
/***********************************************************************/
short extract_cursorstyle(short number_variables,short itemno,CHARTYPE *itemargs,CHARTYPE query_type,LINETYPE argc,CHARTYPE *arg,LINETYPE arglen)
/***********************************************************************/
{
   switch(cursorstyle_insert_shape)
   {
      case CURSOR_BLOCK: item_values[1].value = (CHARTYPE *)"BLOCK"; item_values[1].len = 5; break;
      case CURSOR_UNDERLINE: item_values[1].value = (CHARTYPE *)"UNDERLINE"; item_values[1].len = 9; break;
      case CURSOR_IBEAM: item_values[1].value = (CHARTYPE *)"IBEAM"; item_values[1].len = 5; break;
   }
   if (cursorstyle_insert_blink == CURSOR_BLINK)
   {
      item_values[2].value = (CHARTYPE *)"BLINK"; item_values[2].len = 5;
   }
   else
   {
      item_values[2].value = (CHARTYPE *)"STEADY"; item_values[2].len = 6;
   }
   switch(cursorstyle_over_shape)
   {
      case CURSOR_BLOCK: item_values[3].value = (CHARTYPE *)"BLOCK"; item_values[3].len = 5; break;
      case CURSOR_UNDERLINE: item_values[3].value = (CHARTYPE *)"UNDERLINE"; item_values[3].len = 9; break;
      case CURSOR_IBEAM: item_values[3].value = (CHARTYPE *)"IBEAM"; item_values[3].len = 5; break;
   }
   if (cursorstyle_over_blink == CURSOR_BLINK)
   {
      item_values[4].value = (CHARTYPE *)"BLINK"; item_values[4].len = 5;
   }
   else
   {
      item_values[4].value = (CHARTYPE *)"STEADY"; item_values[4].len = 6;
   }
   return number_variables;
}
/***********************************************************************/
short extract_current_function(short number_variables,short itemno,CHARTYPE *itemargs,CHARTYPE query_type,LINETYPE argc,CHARTYPE *arg,LINETYPE arglen)
/***********************************************************************/
{
   return set_boolean_value((bool)(CURRENT_VIEW->current_window != WINDOW_COMMAND && CURRENT_VIEW->focus_line == CURRENT_VIEW->current_line),(short)1);
}
/***********************************************************************/
short extract_define(short number_variables,short itemno,CHARTYPE *itemargs,CHARTYPE query_type,LINETYPE argc,CHARTYPE *arg,LINETYPE arglen)
/***********************************************************************/
{
   char buf[20];
   int len, number_keys;
   CHARTYPE *keydef;
   int key;
#define DEF_MOUSE_PARAMS  4
   CHARTYPE *word[DEF_MOUSE_PARAMS+1];
   CHARTYPE strip[DEF_MOUSE_PARAMS];
   unsigned short num_params=0;
   bool mouse_key=FALSE;

   if (itemargs == NULL
   ||  blank_field(itemargs)
   ||  strcmp((DEFCHAR*)itemargs,"*") == 0)
   {
      if ( query_type == QUERY_EXTRACT )
      {
         if ( set_rexx_variables_for_all_keys( KEY_TYPE_ALL, &number_keys ) != RC_OK )
         {
            display_error(54,(CHARTYPE *)"",FALSE);
            number_variables = EXTRACT_ARG_ERROR;
         }
         else
         {
            len = sprintf( buf, "%d", number_keys );
            set_rexx_variable( (CHARTYPE *)"define", (CHARTYPE *)buf, len, 0 );
            number_variables = EXTRACT_VARIABLES_SET;
         }
      }
      else
      {
         number_variables = EXTRACT_ARG_ERROR;
      }
   }
   else if ( equal( (CHARTYPE *)"KEY", itemargs, 3 ) )
   {
      if ( query_type == QUERY_EXTRACT )
      {
         if ( set_rexx_variables_for_all_keys( KEY_TYPE_KEY, &number_keys ) != RC_OK )
         {
            display_error(54,(CHARTYPE *)"",FALSE);
            number_variables = EXTRACT_ARG_ERROR;
         }
         else
         {
            len = sprintf( buf, "%d", number_keys );
            set_rexx_variable( (CHARTYPE *)"define", (CHARTYPE *)buf, len, 0 );
            number_variables = EXTRACT_VARIABLES_SET;
         }
      }
      else
      {
         number_variables = EXTRACT_ARG_ERROR;
      }
   }
   else if ( equal( (CHARTYPE *)"MOUSE", itemargs, 5 ) )
   {
      if ( query_type == QUERY_EXTRACT )
      {
         if ( set_rexx_variables_for_all_keys( KEY_TYPE_MOUSE, &number_keys ) != RC_OK )
         {
            display_error(54,(CHARTYPE *)"",FALSE);
            number_variables = EXTRACT_ARG_ERROR;
         }
         else
         {
            len = sprintf( buf, "%d", number_keys );
            set_rexx_variable( (CHARTYPE *)"define", (CHARTYPE *)buf, len, 0 );
            number_variables = EXTRACT_VARIABLES_SET;
         }
      }
      else
      {
         number_variables = EXTRACT_ARG_ERROR;
      }
   }
   else
   {
      /*
       * Find the key value for the named key
       */
      key = find_key_name( itemargs );
      if ( key == -1 )
      {
         /*
          * It's not a key; is it a mouse definition?
          */
         strip[0] = STRIP_BOTH;
         strip[1] = STRIP_BOTH;
         strip[2] = STRIP_BOTH;
         strip[3] = STRIP_NONE;
         num_params = param_split( itemargs, word, DEF_MOUSE_PARAMS, WORD_DELIMS, TEMP_PARAM, strip, FALSE );
         if ( num_params < 3 )
         {
            display_error( 3, (CHARTYPE *)"", FALSE );
            number_variables = EXTRACT_ARG_ERROR;
         }
         else if ( !equal( (CHARTYPE *)"in", word[1], 2 ) )
         {
            display_error( 1, itemargs, FALSE );
            number_variables = EXTRACT_ARG_ERROR;
         }
         else if ( ( key = find_mouse_key_value_in_window( word[0], word[2]) ) == (-1) )
         {
            display_error(1,(CHARTYPE *)itemargs,FALSE);
            number_variables = EXTRACT_ARG_ERROR;
         }
         else
            mouse_key = TRUE;
      }
      else
         mouse_key = FALSE;
      if ( key != -1 )
      {
         if ( query_type == QUERY_EXTRACT )
            keydef = get_key_definition( key, THE_KEY_DEFINE_SHOW, FALSE, mouse_key );
         else
            keydef = get_key_definition( key, THE_KEY_DEFINE_QUERY, FALSE, mouse_key );
         strcpy( (DEFCHAR *)query_rsrvd, (DEFCHAR *)keydef );
         item_values[1].value = query_rsrvd;
         item_values[1].len = strlen( (DEFCHAR*)item_values[1].value );
         number_variables = 1;
      }
      else
      {
         display_error(1,(CHARTYPE *)itemargs,FALSE);
         number_variables = EXTRACT_ARG_ERROR;
      }
   }
   return number_variables;
}
/***********************************************************************/
short extract_defsort(short number_variables,short itemno,CHARTYPE *itemargs,CHARTYPE query_type,LINETYPE argc,CHARTYPE *arg,LINETYPE arglen)
/***********************************************************************/
{
   switch(DEFSORTx)
   {
      case DIRSORT_DIR:
         item_values[1].value = (CHARTYPE *)"DIRECTORY";
         break;
      case DIRSORT_NAME:
         item_values[1].value = (CHARTYPE *)"NAME";
         break;
      case DIRSORT_SIZE:
         item_values[1].value = (CHARTYPE *)"SIZE";
         break;
      case DIRSORT_TIME:
         item_values[1].value = (CHARTYPE *)"TIME";
         break;
      case DIRSORT_DATE:
         item_values[1].value = (CHARTYPE *)"DATE";
         break;
      case DIRSORT_NONE:
         item_values[1].value = (CHARTYPE *)"OFF";
         break;
   }
   item_values[1].len = strlen((DEFCHAR*)item_values[1].value);
   if (DIRORDERx == DIRSORT_ASC)
   {
      item_values[2].value = (CHARTYPE *)"ASCENDING";
      item_values[2].len = 9;
   }
   else
   {
      item_values[2].value = (CHARTYPE *)"DESCENDING";
      item_values[2].len = 10;
   }
   return number_variables;
}
/***********************************************************************/
short extract_dir_function(short number_variables,short itemno,CHARTYPE *itemargs,CHARTYPE query_type,LINETYPE argc,CHARTYPE *arg,LINETYPE arglen)
/***********************************************************************/
{
   return set_boolean_value((bool)(CURRENT_FILE->pseudo_file == PSEUDO_DIR),(short)1);
}
/***********************************************************************/
short extract_dirfileid(short number_variables,short itemno,CHARTYPE *itemargs,CHARTYPE query_type,LINETYPE argc,CHARTYPE *arg,LINETYPE arglen)
/***********************************************************************/
{
   LINETYPE true_line = (-1L);

   if (CURRENT_FILE->pseudo_file == PSEUDO_DIR)
   {
      if (CURRENT_VIEW->current_window == WINDOW_COMMAND)
      {
         if (CURRENT_TOF || CURRENT_BOF)
            true_line = (-1L);
         else
            true_line = CURRENT_VIEW->current_line;
      }
      else
      {
         if (FOCUS_TOF || FOCUS_BOF)
            true_line = (-1L);
         else
            true_line = CURRENT_VIEW->focus_line;
      }
   }
   if (true_line == (-1L))
   {
      item_values[1].value = (CHARTYPE *)dir_path;
      item_values[1].len = strlen((DEFCHAR *)dir_path);
      number_variables = 1;
   }
   else
   {
      curr = lll_find(CURRENT_FILE->first_line,CURRENT_FILE->last_line,true_line,CURRENT_FILE->number_lines);
      item_values[1].value = (CHARTYPE *)dir_path;
      item_values[1].len = strlen((DEFCHAR *)dir_path);
      if (curr->length < FILE_START)
      {
         item_values[2].value = (CHARTYPE *)"";
         item_values[2].len = 0;
      }
      else
      {
         item_values[2].value = (CHARTYPE *)curr->line+FILE_START;
         item_values[2].len = strlen((DEFCHAR *)curr->line+FILE_START);
      }
   }
   return number_variables;
}
/***********************************************************************/
short extract_dirinclude(short number_variables,short itemno,CHARTYPE *itemargs,CHARTYPE query_type,LINETYPE argc,CHARTYPE *arg,LINETYPE arglen)
/***********************************************************************/
{
   item_values[1].value = get_dirtype(query_rsrvd);
   item_values[1].len = strlen((DEFCHAR*)item_values[1].value);
   return number_variables;
}
/***********************************************************************/
short extract_ecolor(short number_variables,short itemno,CHARTYPE *itemargs,CHARTYPE query_type,LINETYPE argc,CHARTYPE *arg,LINETYPE arglen)
/***********************************************************************/
{
   return extract_colour_settings(itemno,query_rsrvd,query_type,itemargs,TRUE,TRUE);
}
/***********************************************************************/
short extract_ecolour(short number_variables,short itemno,CHARTYPE *itemargs,CHARTYPE query_type,LINETYPE argc,CHARTYPE *arg,LINETYPE arglen)
/***********************************************************************/
{
   return extract_colour_settings(itemno,query_rsrvd,query_type,itemargs,FALSE,TRUE);
}
/***********************************************************************/
short extract_end_function(short number_variables,short itemno,CHARTYPE *itemargs,CHARTYPE query_type,LINETYPE argc,CHARTYPE *arg,LINETYPE arglen)
/***********************************************************************/
{
   if (batch_only)
   {
      item_values[1].value = (CHARTYPE *)"0";
      item_values[1].len = 1;
      return 1;
   }
   item_values[1].value = (CHARTYPE *)"0"; /* set FALSE by default */
   switch(CURRENT_VIEW->current_window)
   {
      case WINDOW_FILEAREA:
         if (query1_text_cell_is_end(rec, rec_len,
                                     query1_filearea_cursor_cell()))
            item_values[1].value = (CHARTYPE *)"1";
         break;
      case WINDOW_PREFIX:
         if (query1_text_cell_is_end(pre_rec, pre_rec_len,
                                     query1_prefix_cursor_cell()))
            item_values[1].value = (CHARTYPE *)"1";
         break;
      case WINDOW_COMMAND:
         if (query1_text_cell_is_end(cmd_rec, cmd_rec_len,
                                     query1_command_cursor_cell()))
            item_values[1].value = (CHARTYPE *)"1";
         break;
   }
   item_values[1].len = 1;
   return number_variables;
}
/***********************************************************************/
short extract_display(short number_variables,short itemno,CHARTYPE *itemargs,CHARTYPE query_type,LINETYPE argc,CHARTYPE *arg,LINETYPE arglen)
/***********************************************************************/
{
   sprintf((DEFCHAR *)query_num1,"%d",CURRENT_VIEW->display_low);
   item_values[1].value = query_num1;
   item_values[1].len = strlen((DEFCHAR *)query_num1);
   sprintf((DEFCHAR *)query_num2,"%d",CURRENT_VIEW->display_high);
   item_values[2].value = query_num2;
   item_values[2].len = strlen((DEFCHAR *)query_num2);
   return number_variables;
}
/***********************************************************************/
short extract_eof(short number_variables,short itemno,CHARTYPE *itemargs,CHARTYPE query_type,LINETYPE argc,CHARTYPE *arg,LINETYPE arglen)
/***********************************************************************/
{
   return set_on_off_value((bool)CURRENT_BOF,1);
}
/***********************************************************************/
short extract_eof_function(short number_variables,short itemno,CHARTYPE *itemargs,CHARTYPE query_type,LINETYPE argc,CHARTYPE *arg,LINETYPE arglen)
/***********************************************************************/
{
   return set_boolean_value((bool)(FOCUS_BOF && CURRENT_VIEW->current_window != WINDOW_COMMAND),(short)1);
}
/***********************************************************************/
short extract_eolout(short number_variables,short itemno,CHARTYPE *itemargs,CHARTYPE query_type,LINETYPE argc,CHARTYPE *arg,LINETYPE arglen)
/***********************************************************************/
{
   switch(CURRENT_FILE->eolout)
   {
      case EOLOUT_LF:
         item_values[1].value = (CHARTYPE *)"LF";
         item_values[1].len = 2;
         break;
      case EOLOUT_CR:
         item_values[1].value = (CHARTYPE *)"CR";
         item_values[1].len = 2;
         break;
      case EOLOUT_CRLF:
         item_values[1].value = (CHARTYPE *)"CRLF";
         item_values[1].len = 4;
         break;
      case EOLOUT_NONE:
         item_values[1].value = (CHARTYPE *)"NONE";
         item_values[1].len = 4;
         break;
   }
   return number_variables;
}
/***********************************************************************/
short extract_equivchar(short number_variables,short itemno,CHARTYPE *itemargs,CHARTYPE query_type,LINETYPE argc,CHARTYPE *arg,LINETYPE arglen)
/***********************************************************************/
{
   item_values[1].value = EQUIVCHARstr;
   item_values[1].len = strlen((DEFCHAR *)EQUIVCHARstr);
   return number_variables;
}
/***********************************************************************/
short extract_errorformat(short number_variables,short itemno,CHARTYPE *itemargs,CHARTYPE query_type,LINETYPE argc,CHARTYPE *arg,LINETYPE arglen)
/***********************************************************************/
{
   switch(ERRORFORMATx)
   {
      case 'N':
         item_values[1].value = (CHARTYPE *)"NORMAL";
         item_values[1].len = 6;
         break;
      case 'E':
         item_values[1].value = (CHARTYPE *)"EXTENDED";
         item_values[1].len = 8;
         break;
      default:
         break;
   }
   return number_variables;
}
/***********************************************************************/
short extract_erroroutput(short number_variables,short itemno,CHARTYPE *itemargs,CHARTYPE query_type,LINETYPE argc,CHARTYPE *arg,LINETYPE arglen)
/***********************************************************************/
{
   return set_on_off_value(ERROROUTPUTx,1);
}
/***********************************************************************/
short extract_etmode(short number_variables,short itemno,CHARTYPE *itemargs,CHARTYPE query_type,LINETYPE argc,CHARTYPE *arg,LINETYPE arglen)
/***********************************************************************/
{
   get_etmode(query_num1,query_rsrvd);
   item_values[1].value = query_num1;
   item_values[1].len = strlen((DEFCHAR *)query_num1);
   item_values[2].value = query_rsrvd;
   item_values[2].len = strlen((DEFCHAR *)query_rsrvd);
   return number_variables;
}
/***********************************************************************/
short extract_field(short number_variables,short itemno,CHARTYPE *itemargs,CHARTYPE query_type,LINETYPE argc,CHARTYPE *arg,LINETYPE arglen)
/***********************************************************************/
{
   CHARTYPE *ptr=NULL;
   CHARTYPE *area=NULL;
   LENGTHTYPE len=0;
   LENGTHTYPE cell=0;
   LENGTHTYPE area_len=0;
   LENGTHTYPE char_len=0;

   if (batch_only)
   {
      item_values[1].value = (CHARTYPE *)"0";
      item_values[1].len = 1;
      return 1;
   }
   if (!query1_current_field(&ptr, &len, &cell, &area, &area_len))
   {
      ptr = (CHARTYPE *)"";
      len = 0;
      cell = 0;
      area = (CHARTYPE *)"";
      area_len = 0;
   }
   item_values[1].value = ptr;
   item_values[1].len = len;
   char_len = query1_text_character_at_cell(ptr, len, cell, query_num1,
                                            sizeof(query_num1));
   item_values[2].value = query_num1;
   item_values[2].len = char_len;
   sprintf((DEFCHAR*)query_num2,"%ld",cell+1);
   item_values[3].value = query_num2;
   item_values[3].len = strlen((DEFCHAR*)query_num2);
   item_values[4].value = area;
   item_values[4].len = area_len;
   return number_variables;
}
/***********************************************************************/
short extract_fieldword(short number_variables,short itemno,CHARTYPE *itemargs,CHARTYPE query_type,LINETYPE argc,CHARTYPE *arg,LINETYPE arglen)
/***********************************************************************/
{
   short rc;
   LENGTHTYPE cursor_byte=0;
   LENGTHTYPE cursor_cell=0;
   LENGTHTYPE first_col=0,last_col=0,len=0;
   CHARTYPE *ptr=NULL,save_word;
   LENGTHTYPE word_len;
   CHARTYPE *tmpbuf;

   if (!query1_current_field(&ptr, &len, &cursor_cell, NULL, NULL))
      ptr = (CHARTYPE *)"";
   cursor_byte = query1_text_byte_from_cell(ptr, len, cursor_cell);
   save_word = CURRENT_VIEW->word;
   /*
    * Get the word based on an alphanumeric word setting...
    */
   CURRENT_VIEW->word = 'A';
   if ( len == 0
   ||   get_fieldword( ptr, len, cursor_byte, &first_col, &last_col ) == 0 )
   {
      rc = set_rexx_variable( query_item[itemno].name, (CHARTYPE *)"", 0, 1 );
   }
   else
   {
      word_len = (last_col - first_col) + 1;
      tmpbuf = (CHARTYPE *)(*the_malloc)( sizeof(CHARTYPE)*(word_len+1) );
      if ( tmpbuf == (CHARTYPE *)NULL )
      {
         display_error( 30, (CHARTYPE *)"", FALSE );
         CURRENT_VIEW->word = save_word;
         return( EXTRACT_ARG_ERROR );
      }
      memcpy( (DEFCHAR *)tmpbuf, (DEFCHAR *)ptr+first_col, word_len );
      tmpbuf[word_len] = '\0';
      rc = set_rexx_variable( query_item[itemno].name, tmpbuf, word_len, 1 );
      (*the_free)(tmpbuf);
   }
   if ( rc == RC_SYSTEM_ERROR )
   {
      display_error( 54, (CHARTYPE *)"", FALSE );
      CURRENT_VIEW->word = save_word;
      return( EXTRACT_ARG_ERROR );
   }
   /*
    * Get the word based on a non-blank word setting...
    */
   CURRENT_VIEW->word = 'N';
   if ( len == 0
   ||   get_fieldword( ptr, len, cursor_byte, &first_col, &last_col ) == 0 )
   {
      rc = set_rexx_variable( query_item[itemno].name, (CHARTYPE *)"", 0, 2 );
   }
   else
   {
      word_len = (last_col - first_col) + 1;
      tmpbuf = (CHARTYPE *)(*the_malloc)( sizeof(CHARTYPE)*(word_len+1) );
      if ( tmpbuf == (CHARTYPE *)NULL )
      {
         display_error( 30, (CHARTYPE *)"", FALSE );
         CURRENT_VIEW->word = save_word;
         return( EXTRACT_ARG_ERROR );
      }
      memcpy( (DEFCHAR *)tmpbuf, (DEFCHAR *)ptr+first_col, word_len );
      tmpbuf[word_len] = '\0';
      rc = set_rexx_variable( query_item[itemno].name, tmpbuf, word_len, 2 );
      (*the_free)(tmpbuf);
   }
   if ( rc == RC_SYSTEM_ERROR )
   {
      display_error( 54, (CHARTYPE *)"", FALSE );
      CURRENT_VIEW->word = save_word;
      return( EXTRACT_ARG_ERROR );
   }
   /*
    * Set the starting column
    */
   word_len = sprintf( (DEFCHAR *)query_num1, "%ld",
                       query1_text_cell_from_byte(ptr, len, first_col) + 1 );
   rc = set_rexx_variable( query_item[itemno].name, query_num1, word_len, 3 );
   if ( rc == RC_SYSTEM_ERROR )
   {
      display_error( 54, (CHARTYPE *)"", FALSE );
      CURRENT_VIEW->word = save_word;
      return( EXTRACT_ARG_ERROR );
   }
   /*
    * Set the 0 tail
    */
   rc = set_rexx_variable( query_item[itemno].name, (CHARTYPE *)"3", 1, 0 );
   if ( rc == RC_SYSTEM_ERROR )
   {
      display_error( 54, (CHARTYPE *)"", FALSE );
      CURRENT_VIEW->word = save_word;
      return( EXTRACT_ARG_ERROR );
   }
   CURRENT_VIEW->word = save_word;

   return EXTRACT_VARIABLES_SET;
}
/***********************************************************************/
short extract_first_function(short number_variables,short itemno,CHARTYPE *itemargs,CHARTYPE query_type,LINETYPE argc,CHARTYPE *arg,LINETYPE arglen)
/***********************************************************************/
{
   if (batch_only)
   {
      item_values[1].value = (CHARTYPE *)"0";
      item_values[1].len = 1;
      return 1;
   }
   return set_boolean_value((bool)(CURRENT_VIEW->current_window == WINDOW_FILEAREA
                                && query1_filearea_cursor_cell() == 0),
                            (short)1);
}
/***********************************************************************/
short extract_focuseof_function(short number_variables,short itemno,CHARTYPE *itemargs,CHARTYPE query_type,LINETYPE argc,CHARTYPE *arg,LINETYPE arglen)
/***********************************************************************/
{
   bool bool_flag;
   if ( CURRENT_VIEW->current_window == WINDOW_COMMAND )
      bool_flag = CURRENT_BOF;
   else
      bool_flag = FOCUS_BOF;
   return set_boolean_value( (bool)(bool_flag), (short)1 );
}
/***********************************************************************/
short extract_focustof_function(short number_variables,short itemno,CHARTYPE *itemargs,CHARTYPE query_type,LINETYPE argc,CHARTYPE *arg,LINETYPE arglen)
/***********************************************************************/
{
   bool bool_flag;
   if ( CURRENT_VIEW->current_window == WINDOW_COMMAND )
      bool_flag = CURRENT_TOF;
   else
      bool_flag = FOCUS_TOF;
   return set_boolean_value( (bool)(bool_flag), (short)1 );
}
/***********************************************************************/
short extract_filename(short number_variables,short itemno,CHARTYPE *itemargs,CHARTYPE query_type,LINETYPE argc,CHARTYPE *arg,LINETYPE arglen)
/***********************************************************************/
{
   item_values[1].value = (CHARTYPE *)CURRENT_FILE->fname;
   item_values[1].len = strlen((DEFCHAR *)CURRENT_FILE->fname);
   return number_variables;
}
/***********************************************************************/
short extract_filestatus(short number_variables,short itemno,CHARTYPE *itemargs,CHARTYPE query_type,LINETYPE argc,CHARTYPE *arg,LINETYPE arglen)
/***********************************************************************/
{
   item_values[1].value = (CHARTYPE *)"NONE";
   item_values[1].len = 4;
   if ( ISREADONLY(CURRENT_FILE) )
   {
      item_values[2].value = (CHARTYPE *)"READONLY";
      item_values[2].len = 8;
   }
   else
   {
      item_values[2].value = (CHARTYPE *)"READWRITE";
      item_values[2].len = 9;
   }
   switch(CURRENT_FILE->eolfirst)
   {
      case EOLOUT_LF:
         item_values[3].value = (CHARTYPE *)"LF";
         item_values[3].len = 2;
         break;
      case EOLOUT_CR:
         item_values[3].value = (CHARTYPE *)"CR";
         item_values[3].len = 2;
         break;
      case EOLOUT_CRLF:
         item_values[3].value = (CHARTYPE *)"CRLF";
         item_values[3].len = 4;
         break;
      case EOLOUT_NONE:
         item_values[3].value = (CHARTYPE *)"NONE";
         item_values[3].len = 4;
         break;
   }
   return number_variables;
}
/***********************************************************************/
short extract_filectlchar(short number_variables,short itemno,CHARTYPE *itemargs,CHARTYPE query_type,LINETYPE argc,CHARTYPE *arg,LINETYPE arglen)
/***********************************************************************/
{
   return set_on_off_value(CURRENT_FILE->filectlchar,1);
}
/***********************************************************************/
short extract_filetabs(short number_variables,short itemno,CHARTYPE *itemargs,CHARTYPE query_type,LINETYPE argc,CHARTYPE *arg,LINETYPE arglen)
/***********************************************************************/
{
   return set_on_off_value((bool)FILETABSx,1);
}
/***********************************************************************/
short extract_fmode(short number_variables,short itemno,CHARTYPE *itemargs,CHARTYPE query_type,LINETYPE argc,CHARTYPE *arg,LINETYPE arglen)
/***********************************************************************/
{
#if defined(UNIX)
   item_values[1].value = (CHARTYPE *)"";
   item_values[1].len = 0;
#else
   query_num1[0] = (CHARTYPE)*(CURRENT_FILE->fpath);
   query_num1[1] = '\0';
   item_values[1].value = query_num1;
   item_values[1].len = 1;
#endif
   return number_variables;
}
/***********************************************************************/
short extract_fname(short number_variables,short itemno,CHARTYPE *itemargs,CHARTYPE query_type,LINETYPE argc,CHARTYPE *arg,LINETYPE arglen)
/***********************************************************************/
{
   short x=0;

   strcpy((DEFCHAR*)query_rsrvd,(DEFCHAR*)CURRENT_FILE->fname);
   x = strzreveq(query_rsrvd,'.');
   if (x != (-1))
      query_rsrvd[x] = '\0';
   item_values[1].value = query_rsrvd;
   item_values[1].len = strlen((DEFCHAR *)query_rsrvd);
   return number_variables;
}
/***********************************************************************/
short extract_efileid(short number_variables,short itemno,CHARTYPE *itemargs,CHARTYPE query_type,LINETYPE argc,CHARTYPE *arg,LINETYPE arglen)
/***********************************************************************/
{
   item_values[1].value = (CHARTYPE *)CURRENT_FILE->efileid;
   item_values[1].len = strlen( (DEFCHAR *)CURRENT_FILE->efileid );
   item_values[2].value = (CHARTYPE *)CURRENT_FILE->actualfname;
   item_values[2].len = strlen( (DEFCHAR *)CURRENT_FILE->actualfname );
   return number_variables;
}
/***********************************************************************/
short extract_fpath(short number_variables,short itemno,CHARTYPE *itemargs,CHARTYPE query_type,LINETYPE argc,CHARTYPE *arg,LINETYPE arglen)
/***********************************************************************/
{
   item_values[1].value = (CHARTYPE *)CURRENT_FILE->fpath;
   item_values[1].len = strlen((DEFCHAR *)CURRENT_FILE->fpath);
   return number_variables;
}
/***********************************************************************/
short extract_fdisplay(short number_variables,short itemno,CHARTYPE *itemargs,CHARTYPE query_type,LINETYPE argc,CHARTYPE *arg,LINETYPE arglen)
/***********************************************************************/
{
   item_values[1].value = (CHARTYPE *)CURRENT_FILE->display_name;
   item_values[1].len = strlen( (DEFCHAR *)CURRENT_FILE->display_name );
   return number_variables;
}
/***********************************************************************/
short extract_ftype(short number_variables,short itemno,CHARTYPE *itemargs,CHARTYPE query_type,LINETYPE argc,CHARTYPE *arg,LINETYPE arglen)
/***********************************************************************/
{
   short x=0;

   x = strzreveq(CURRENT_FILE->fname,'.');
   if (x == (-1))
   {
      item_values[1].value = (CHARTYPE *)"";
      item_values[1].len = 0;
   }
   else
   {
      item_values[1].value = (CHARTYPE *)CURRENT_FILE->fname+x+1;
      item_values[1].len = strlen((DEFCHAR *)CURRENT_FILE->fname+x+1);
   }
   return number_variables;
}

/***********************************************************************/
short extract_fullfname(short number_variables,short itemno,CHARTYPE *itemargs,CHARTYPE query_type,LINETYPE argc,CHARTYPE *arg,LINETYPE arglen)
/***********************************************************************/
{
   return set_on_off_value(CURRENT_FILE->display_actual_filename,1);
}
/***********************************************************************/
short extract_getenv(short number_variables,short itemno,CHARTYPE *itemargs,CHARTYPE query_type,LINETYPE argc,CHARTYPE *arg,LINETYPE arglen)
/***********************************************************************/
{
   CHARTYPE *tmpbuf=NULL;

   if (itemargs == NULL
   ||  strlen((DEFCHAR *)itemargs) == 0)
   {
      item_values[1].value = (CHARTYPE *)"***invalid***";
      item_values[1].len = strlen((DEFCHAR *)item_values[1].value);
      return number_variables;
   }
#if defined(UNIX)
   if (query_type == QUERY_FUNCTION)
      tmpbuf = (CHARTYPE *)getenv((DEFCHAR *)arg);
   else
      tmpbuf = (CHARTYPE *)getenv((DEFCHAR *)itemargs);
#else
   if (query_type == QUERY_FUNCTION)
      tmpbuf = (CHARTYPE *)getenv((DEFCHAR *)make_upper(arg));
   else
      tmpbuf = (CHARTYPE *)getenv((DEFCHAR *)make_upper(itemargs));
#endif
   if (tmpbuf == NULL)
      item_values[1].value = (CHARTYPE *)"***invalid***";
   else
      item_values[1].value = tmpbuf;
   item_values[1].len = strlen((DEFCHAR *)item_values[1].value);
   return number_variables;
}
/***********************************************************************/
short extract_header(short number_variables,short itemno,CHARTYPE *itemargs,CHARTYPE query_type,LINETYPE argc,CHARTYPE *arg,LINETYPE arglen)
/***********************************************************************/
{
   short rc=RC_OK;
   register int i=0,off=0;
   ROWTYPE save_msgline_rows = CURRENT_VIEW->msgline_rows;
   bool save_msgmode_status = CURRENT_VIEW->msgmode_status;

   TRACE_FUNCTION("query.c:   extract_header");
   if (query_type == QUERY_QUERY)
   {
      for ( i = 0; thm[i].the_header != HEADER_ALL; i++ )
      {
         ;
      }
      CURRENT_VIEW->msgline_rows   = min(terminal_lines-1,i);
      CURRENT_VIEW->msgmode_status = TRUE;
   }

   for ( i = 0; thm[i].the_header != HEADER_ALL; i++ )
   {
      sprintf((DEFCHAR *)query_rsrvd,"%s %s",
        thm[i].the_header_name,
        ( CURRENT_VIEW->syntax_headers & thm[i].the_header ) ? "ON" : "OFF" );

      if (query_type == QUERY_QUERY)
         display_error(0,query_rsrvd,TRUE);
      else
      {
         number_variables++;
         item_values[number_variables].len = strlen((DEFCHAR *)query_rsrvd);
         memcpy((DEFCHAR*)trec+off,(DEFCHAR*)query_rsrvd,(item_values[number_variables].len)+1);
         item_values[number_variables].value = trec+off;
         off += (item_values[number_variables].len)+1;
      }
   }

   if (query_type == QUERY_QUERY)
   {
      CURRENT_VIEW->msgline_rows   = save_msgline_rows;
      CURRENT_VIEW->msgmode_status = save_msgmode_status;
      rc = EXTRACT_VARIABLES_SET;
   }
   else
      rc = number_variables;

   TRACE_RETURN();
   return(rc);
}
/***********************************************************************/
short extract_hex(short number_variables,short itemno,CHARTYPE *itemargs,CHARTYPE query_type,LINETYPE argc,CHARTYPE *arg,LINETYPE arglen)
/***********************************************************************/
{
   return set_on_off_value(CURRENT_VIEW->hex,1);
}
/***********************************************************************/
short extract_hexdisplay(short number_variables,short itemno,CHARTYPE *itemargs,CHARTYPE query_type,LINETYPE argc,CHARTYPE *arg,LINETYPE arglen)
/***********************************************************************/
{
   return set_on_off_value(HEXDISPLAYx,1);
}
/***********************************************************************/
short extract_hexshow(short number_variables,short itemno,CHARTYPE *itemargs,CHARTYPE query_type,LINETYPE argc,CHARTYPE *arg,LINETYPE arglen)
/***********************************************************************/
{
   set_on_off_value(CURRENT_VIEW->hexshow_on,1);
   if (CURRENT_VIEW->hexshow_base == POSITION_MIDDLE)
      sprintf((DEFCHAR *)query_rsrvd,"M%+d",CURRENT_VIEW->hexshow_off);
   else
      sprintf((DEFCHAR *)query_rsrvd,"%d",CURRENT_VIEW->hexshow_off);
   item_values[2].value = query_rsrvd;
   item_values[2].len = strlen((DEFCHAR *)query_rsrvd);
   return number_variables;
}
/***********************************************************************/
short extract_highlight(short number_variables,short itemno,CHARTYPE *itemargs,CHARTYPE query_type,LINETYPE argc,CHARTYPE *arg,LINETYPE arglen)
/***********************************************************************/
{
   switch (CURRENT_VIEW->highlight)
   {
      case HIGHLIGHT_NONE:
         item_values[1].value = (CHARTYPE *)"OFF";
         break;
      case HIGHLIGHT_ALT:
         item_values[1].value = (CHARTYPE *)"ALTERED";
         break;
      case HIGHLIGHT_TAG:
         item_values[1].value = (CHARTYPE *)"TAGGED";
         break;
      case HIGHLIGHT_SELECT:
         item_values[1].value = (CHARTYPE *)"SELECT";
         sprintf((DEFCHAR *)query_num1,"%d",CURRENT_VIEW->highlight_low);
         item_values[2].value = query_num1;
         item_values[2].len = strlen((DEFCHAR *)query_num1);
         sprintf((DEFCHAR *)query_num2,"%d",CURRENT_VIEW->highlight_high);
         item_values[3].value = query_num2;
         item_values[3].len = strlen((DEFCHAR *)query_num2);
         number_variables = 3;
         break;
      default:
         break;
   }
   item_values[1].len = strlen((DEFCHAR *)item_values[1].value);
   return number_variables;
}
/***********************************************************************/
short extract_idline(short number_variables,short itemno,CHARTYPE *itemargs,CHARTYPE query_type,LINETYPE argc,CHARTYPE *arg,LINETYPE arglen)
/***********************************************************************/
{
   if ( query_type == QUERY_EXTRACT
   &&   !blank_field( itemargs ) )
   {
      prepare_idline( current_screen );
      /*
       * Only time we supply 2 values; 'EXTRACT /IDLINE *'
       */
      if ( CURRENT_VIEW->id_line )
      {
         item_values[1].value = (CHARTYPE *)"ON";
         item_values[1].len = 2;
      }
      else
      {
         item_values[1].value = (CHARTYPE *)"OFF";
         item_values[1].len = 3;
      }
      item_values[2].value = linebuf;
      item_values[2].len = strlen( (DEFCHAR *)item_values[2].value );
      number_variables = 2;
   }
   else
   {
      return set_on_off_value( CURRENT_VIEW->id_line, 1 );
   }
   return number_variables;
}
/***********************************************************************/
short extract_impmacro(short number_variables,short itemno,CHARTYPE *itemargs,CHARTYPE query_type,LINETYPE argc,CHARTYPE *arg,LINETYPE arglen)
/***********************************************************************/
{
   return set_on_off_value(CURRENT_VIEW->imp_macro,1);
}
/***********************************************************************/
short extract_impos(short number_variables,short itemno,CHARTYPE *itemargs,CHARTYPE query_type,LINETYPE argc,CHARTYPE *arg,LINETYPE arglen)
/***********************************************************************/
{
   return set_on_off_value(CURRENT_VIEW->imp_os,1);
}
/***********************************************************************/
short extract_inblock_function(short number_variables,short itemno,CHARTYPE *itemargs,CHARTYPE query_type,LINETYPE argc,CHARTYPE *arg,LINETYPE arglen)
/***********************************************************************/
{
   bool bool_flag=FALSE;
   LENGTHTYPE cursor_col=0;

   if (batch_only)
   {
      item_values[1].value = (CHARTYPE *)"0";
      item_values[1].len = 1;
      return 1;
   }
   bool_flag = FALSE;
   if (CURRENT_VIEW == MARK_VIEW
   &&  CURRENT_VIEW->current_window == WINDOW_FILEAREA)
   {
      cursor_col = query1_filearea_cursor_cell() + 1;
      switch(MARK_VIEW->mark_type)
      {
         case M_LINE:
            if ((CURRENT_VIEW->focus_line >= MARK_VIEW->mark_start_line)
            &&  (CURRENT_VIEW->focus_line <= MARK_VIEW->mark_end_line))
               bool_flag = TRUE;
            break;
         case M_BOX:
         case M_WORD:
         case M_COLUMN:
            if ((CURRENT_VIEW->focus_line >= MARK_VIEW->mark_start_line)
            &&  (CURRENT_VIEW->focus_line <= MARK_VIEW->mark_end_line)
            &&  (cursor_col >= MARK_VIEW->mark_start_col)
            &&  (cursor_col <= MARK_VIEW->mark_end_col))
               bool_flag = TRUE;
            break;
         case M_STREAM:
            if ((CURRENT_VIEW->focus_line < MARK_VIEW->mark_start_line)
            ||  (CURRENT_VIEW->focus_line > MARK_VIEW->mark_end_line))
            {
               bool_flag = FALSE;
               break;
            }
            if (MARK_VIEW->mark_start_line == MARK_VIEW->mark_end_line)
            {
               /*
                * Single-line block
                */
               if (cursor_col >= MARK_VIEW->mark_start_col
               &&  cursor_col <= MARK_VIEW->mark_end_col)
               {
                  bool_flag = TRUE;
                  break;
               }
               else
               {
                  bool_flag = FALSE;
                  break;
               }
            }
            /*
             * Multi-line stream block
             */
            if (CURRENT_VIEW->focus_line == MARK_VIEW->mark_start_line
            &&  cursor_col >= MARK_VIEW->mark_start_col)
            {
               bool_flag = TRUE;
               break;
            }
            if (CURRENT_VIEW->focus_line == MARK_VIEW->mark_end_line
            &&  cursor_col <= MARK_VIEW->mark_end_col)
            {
               bool_flag = TRUE;
               break;
            }
            if (CURRENT_VIEW->focus_line != MARK_VIEW->mark_start_line
            &&  CURRENT_VIEW->focus_line != MARK_VIEW->mark_end_line)
            {
               bool_flag = TRUE;
               break;
            }
            bool_flag = FALSE;
            break;
         default:
            break;
      }
   }
   return set_boolean_value((bool)bool_flag,(short)1);
}
/***********************************************************************/
short extract_incommand_function(short number_variables,short itemno,CHARTYPE *itemargs,CHARTYPE query_type,LINETYPE argc,CHARTYPE *arg,LINETYPE arglen)
/***********************************************************************/
{
   return set_boolean_value((bool)(CURRENT_VIEW->current_window == WINDOW_COMMAND),(short)1);
}
/***********************************************************************/
short extract_initial_function(short number_variables,short itemno,CHARTYPE *itemargs,CHARTYPE query_type,LINETYPE argc,CHARTYPE *arg,LINETYPE arglen)
/***********************************************************************/
{
   return set_boolean_value((bool)in_profile,(short)1);
}
/***********************************************************************/
short extract_inprefix_function(short number_variables,short itemno,CHARTYPE *itemargs,CHARTYPE query_type,LINETYPE argc,CHARTYPE *arg,LINETYPE arglen)
/***********************************************************************/
{
   return set_boolean_value((bool)(CURRENT_VIEW->current_window == WINDOW_PREFIX),(short)1);
}
/***********************************************************************/
short extract_inputmode(short number_variables,short itemno,CHARTYPE *itemargs,CHARTYPE query_type,LINETYPE argc,CHARTYPE *arg,LINETYPE arglen)
/***********************************************************************/
{
   if (CURRENT_VIEW->inputmode == INPUTMODE_OFF)
   {
      item_values[1].value = (CHARTYPE *)"OFF";
      item_values[1].len = 3;
   }
   else if (CURRENT_VIEW->inputmode == INPUTMODE_FULL)
   {
      item_values[1].value = (CHARTYPE *)"FULL";
      item_values[1].len = 4;
   }
   else if (CURRENT_VIEW->inputmode == INPUTMODE_LINE)
   {
      item_values[1].value = (CHARTYPE *)"LINE";
      item_values[1].len = 4;
   }
   return number_variables;
}
/***********************************************************************/
short extract_insertmode(short number_variables,short itemno,CHARTYPE *itemargs,CHARTYPE query_type,LINETYPE argc,CHARTYPE *arg,LINETYPE arglen)
/***********************************************************************/
{
   return set_on_off_value(INSERTMODEx,1);
}
/***********************************************************************/
short extract_interface(short number_variables,short itemno,CHARTYPE *itemargs,CHARTYPE query_type,LINETYPE argc,CHARTYPE *arg,LINETYPE arglen)
/***********************************************************************/
{
   if ( INTERFACEx == INTERFACE_CLASSIC )
   {
      item_values[1].value = (CHARTYPE *)"CLASSIC";
      item_values[1].len = 7;
   }
   else if ( INTERFACEx == INTERFACE_CUA )
   {
      item_values[1].value = (CHARTYPE *)"CUA";
      item_values[1].len = 3;
   }
   return number_variables;
}
/***********************************************************************/
short extract_insertmode_function(short number_variables,short itemno,CHARTYPE *itemargs,CHARTYPE query_type,LINETYPE argc,CHARTYPE *arg,LINETYPE arglen)
/***********************************************************************/
{
   return set_boolean_value((bool)(INSERTMODEx),(short)1);
}
/***********************************************************************/
short extract_lastkey(short number_variables,short itemno,CHARTYPE *itemargs,CHARTYPE query_type,LINETYPE argc,CHARTYPE *arg,LINETYPE arglen)
/***********************************************************************/
{
   int keynum=0;

   if ( blank_field( itemargs ) )
      set_key_values( (current_key==-1) ? -1 : lastkeys[current_key], lastkeys_is_mouse[current_key] );
   else
   {
      if ( valid_positive_integer( itemargs ) )
      {
         keynum = atoi( (DEFCHAR *)itemargs );
         if ( keynum > 8 )
            return( EXTRACT_ARG_ERROR );
         if ( current_key == -1 )
            set_key_values( -1, FALSE );
         else
         {
            keynum--;  /* 0 base the number */
            if ( keynum <= current_key )
               keynum = current_key - keynum;
            else
               keynum = 8 - (keynum - current_key);
            set_key_values( lastkeys[keynum], lastkeys_is_mouse[keynum] );
         }
      }
      else
         return( EXTRACT_ARG_ERROR );
   }
   return number_variables;
}
/***********************************************************************/
short extract_lastmsg(short number_variables,short itemno,CHARTYPE *itemargs,CHARTYPE query_type,LINETYPE argc,CHARTYPE *arg,LINETYPE arglen)
/***********************************************************************/
{
   if (last_message == NULL)
   {
      item_values[1].value = (CHARTYPE *)"";
      item_values[1].len = 0;
   }
   else
   {
      item_values[1].value = (CHARTYPE *)last_message;
      item_values[1].len = strlen((DEFCHAR *)last_message);
   }
   return number_variables;
}
/***********************************************************************/
short extract_lastop(short number_variables,short itemno,CHARTYPE *itemargs,CHARTYPE query_type,LINETYPE argc,CHARTYPE *arg,LINETYPE arglen)
/***********************************************************************/
{
   short rc=RC_OK;
   register int i=0;
   bool found=FALSE;
   ROWTYPE save_msgline_rows = CURRENT_VIEW->msgline_rows;
   bool save_msgmode_status = CURRENT_VIEW->msgmode_status;
   CHARTYPE *ptr_lastop=NULL;
   int off=0;

   if (itemargs == NULL
   ||  blank_field(itemargs)
   ||  strcmp((DEFCHAR*)itemargs,"*") == 0)
   {
      if (query_type == QUERY_QUERY)
      {
         CURRENT_VIEW->msgline_rows   = min(terminal_lines-1,LASTOP_MAX);
         CURRENT_VIEW->msgmode_status = TRUE;
      }
      else
         number_variables = 0;
      for ( i = 0; i < LASTOP_MAX; i++ )
      {
         sprintf((DEFCHAR *)query_rsrvd,"%s%s %s",
           (query_type == QUERY_QUERY) ? (DEFCHAR *)"lastop " : "",
           lastop[i].command,
           (lastop[i].value) ? lastop[i].value : (CHARTYPE *)"" );

         if ( query_type == QUERY_QUERY )
            display_error( 0, query_rsrvd, TRUE);
         else
         {
            number_variables++;
            item_values[number_variables].len = strlen( (DEFCHAR *)query_rsrvd );
            memcpy( (DEFCHAR*)trec+off, (DEFCHAR*)query_rsrvd, (item_values[number_variables].len)+1 );
            item_values[number_variables].value = trec+off;
            off += (item_values[number_variables].len)+1;
         }
      }
   }
   else
   {
      if (query_type == QUERY_QUERY)
      {
         CURRENT_VIEW->msgline_rows   = 1;
         CURRENT_VIEW->msgmode_status = TRUE;
      }
      /*
       * Find a match for the supplied lastop operand
       */
      for ( i = 0; i < LASTOP_MAX; i++ )
      {
         if ( equal( lastop[i].command, (CHARTYPE *)itemargs, lastop[i].min_len ) )
         {
            ptr_lastop = (lastop[i].value) ? lastop[i].value : (CHARTYPE *)"";
            found = TRUE;
            break;
         }
      }
      if (query_type == QUERY_QUERY)
      {
         if (!found)
         {
            display_error( 1, itemargs, TRUE );
            TRACE_RETURN();
            return EXTRACT_ARG_ERROR;
         }
         sprintf((DEFCHAR *)query_rsrvd,"%s%s %s",
            (query_type == QUERY_QUERY) ? (DEFCHAR *)"lastop " : "",
            lastop[i].command,
            ptr_lastop);
         display_error( 0, query_rsrvd, TRUE );
      }
      else
      {
         if (!found)
         {
            TRACE_RETURN();
            return EXTRACT_ARG_ERROR;
         }
         item_values[1].value = lastop[i].command;
         item_values[1].len = strlen((DEFCHAR *)itemargs);
         item_values[2].value = ptr_lastop;
         item_values[2].len = strlen((DEFCHAR *)ptr_lastop);
         number_variables = 2;
      }
   }

   if (query_type == QUERY_QUERY)
   {
      CURRENT_VIEW->msgline_rows   = save_msgline_rows;
      CURRENT_VIEW->msgmode_status = save_msgmode_status;
      rc = EXTRACT_VARIABLES_SET;
   }
   else
      rc = number_variables;

   TRACE_RETURN();
   return rc;

}
/***********************************************************************/
short extract_lastrc(short number_variables,short itemno,CHARTYPE *itemargs,CHARTYPE query_type,LINETYPE argc,CHARTYPE *arg,LINETYPE arglen)
/***********************************************************************/
{
   sprintf((DEFCHAR *)query_num1,"%d",lastrc);
   item_values[1].value = query_num1;
   item_values[1].len = strlen((DEFCHAR *)query_num1);
   return number_variables;
}
/***********************************************************************/
short extract_leftedge_function(short number_variables,short itemno,CHARTYPE *itemargs,CHARTYPE query_type,LINETYPE argc,CHARTYPE *arg,LINETYPE arglen)
/***********************************************************************/
{
   short y=0,x=0;

   if (batch_only)
   {
      item_values[1].value = (CHARTYPE *)"0";
      item_values[1].len = 1;
      return 1;
   }
   query1_capture_active_driver_cursor(CURRENT_VIEW->current_window,&y,&x);
   INTENTIONALLY_UNUSED_VARIABLE(y);
   return set_boolean_value((bool)(CURRENT_VIEW->current_window == WINDOW_FILEAREA && x == 0),(short)1);
}
/***********************************************************************/
short extract_length(short number_variables,short itemno,CHARTYPE *itemargs,CHARTYPE query_type,LINETYPE argc,CHARTYPE *arg,LINETYPE arglen)
/***********************************************************************/
{
   curr = lll_find(CURRENT_FILE->first_line,CURRENT_FILE->last_line,CURRENT_VIEW->current_line,CURRENT_FILE->number_lines);
   sprintf((DEFCHAR *)query_num1,"%ld",curr->length);
   item_values[1].value = query_num1;
   item_values[1].len = strlen((DEFCHAR *)query_num1);
   return number_variables;
}
/***********************************************************************/
short extract_line(short number_variables,short itemno,CHARTYPE *itemargs,CHARTYPE query_type,LINETYPE argc,CHARTYPE *arg,LINETYPE arglen)
/***********************************************************************/
{
   sprintf((DEFCHAR *)query_num1,"%ld",
      (compatible_feel==COMPAT_XEDIT)?CURRENT_VIEW->current_line:get_true_line(TRUE));
   item_values[1].value = query_num1;
   item_values[1].len = strlen((DEFCHAR *)query_num1);
   return number_variables;
}
/***********************************************************************/
short extract_lineflag(short number_variables,short itemno,CHARTYPE *itemargs,CHARTYPE query_type,LINETYPE argc,CHARTYPE *arg,LINETYPE arglen)
/***********************************************************************/
{
   curr = lll_find(CURRENT_FILE->first_line,CURRENT_FILE->last_line,
                   (compatible_feel==COMPAT_XEDIT)?CURRENT_VIEW->current_line:get_true_line(TRUE),
                   CURRENT_FILE->number_lines);
   if (curr->flags.new_flag)
      item_values[1].value = (CHARTYPE *)"NEW";
   else
      item_values[1].value = (CHARTYPE *)"NONEW";
   item_values[1].len = strlen((DEFCHAR *)item_values[1].value);
   if (curr->flags.changed_flag)
      item_values[2].value = (CHARTYPE *)"CHANGE";
   else
      item_values[2].value = (CHARTYPE *)"NOCHANGE";
   item_values[2].len = strlen((DEFCHAR *)item_values[2].value);
   if (curr->flags.tag_flag)
      item_values[3].value = (CHARTYPE *)"TAG";
   else
      item_values[3].value = (CHARTYPE *)"NOTAG";
   item_values[3].len = strlen((DEFCHAR *)item_values[3].value);
   return number_variables;
}
/***********************************************************************/
short extract_linend(short number_variables,short itemno,CHARTYPE *itemargs,CHARTYPE query_type,LINETYPE argc,CHARTYPE *arg,LINETYPE arglen)
/***********************************************************************/
{
   set_on_off_value(CURRENT_VIEW->linend_status,1);
   query_num1[0] = CURRENT_VIEW->linend_value;
   query_num1[1] = '\0';
   item_values[2].value = query_num1;
   item_values[2].len = 1;
   return number_variables;
}
/***********************************************************************/
short extract_lscreen(short number_variables,short itemno,CHARTYPE *itemargs,CHARTYPE query_type,LINETYPE argc,CHARTYPE *arg,LINETYPE arglen)
/***********************************************************************/
{
   sprintf((DEFCHAR *)query_num1,"%d",CURRENT_SCREEN.screen_rows);
   item_values[1].value = query_num1;
   item_values[1].len = strlen((DEFCHAR *)query_num1);
   sprintf((DEFCHAR *)query_num2,"%d",CURRENT_SCREEN.screen_cols);
   item_values[2].value = query_num2;
   item_values[2].len = strlen((DEFCHAR *)query_num2);
   sprintf((DEFCHAR *)query_num3,"%d",CURRENT_SCREEN.screen_start_row+1);
   item_values[3].value = query_num3;
   item_values[3].len = strlen((DEFCHAR *)query_num3);
   sprintf((DEFCHAR *)query_num4,"%d",CURRENT_SCREEN.screen_start_col+1);
   item_values[4].value = query_num4;
   item_values[4].len = strlen((DEFCHAR *)query_num4);
   sprintf((DEFCHAR *)query_num5,"%d",terminal_lines);
   item_values[5].value = query_num5;
   item_values[5].len = strlen((DEFCHAR *)query_num5);
   sprintf((DEFCHAR *)query_num6,"%d",COLS);
   item_values[6].value = query_num6;
   item_values[6].len = strlen((DEFCHAR *)query_num6);
   return number_variables;
}
