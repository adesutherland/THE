/*
 * QUERY2.C - Functions related to QUERY,STATUS and EXTRACT - M-Z
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
short extract_prefix_settings(short,CHARTYPE *,CHARTYPE);
short extract_colour_settings(short,CHARTYPE *,CHARTYPE,CHARTYPE *,bool,bool);
short extract_autocolour_settings(short,CHARTYPE *,CHARTYPE,CHARTYPE *,bool);
void get_etmode(CHARTYPE *,CHARTYPE *);
short set_boolean_value(bool flag, short num);
short set_on_off_value(bool flag, short num);
void set_key_values(int key, bool mouse_key);
THE_PPC *in_range( THE_PPC *found_ppc, THE_PPC *curr_ppc, LINETYPE first_in_range, LINETYPE last_in_range );

extern CHARTYPE _THE_FAR *block_name[];

extern CHARTYPE query_num1[20];
extern CHARTYPE query_num2[20];
extern CHARTYPE query_num3[40];
extern CHARTYPE query_num4[40];
extern CHARTYPE query_num5[10];
extern CHARTYPE query_num6[10];
extern CHARTYPE query_num7[10];
extern CHARTYPE query_num8[10];
extern CHARTYPE query_rsrvd[MAX_FILE_NAME+100];
static LINE *curr;

static LENGTHTYPE query2_filearea_cursor_cell(void)
{
   LogicalCursor logical;

   logical = CURRENT_VIEW->logical_cursor.current;
   if (logical.valid
   &&  logical.zone == LOGICAL_CURSOR_ZONE_FILEAREA
   &&  logical.line_number == CURRENT_VIEW->focus_line
   &&  logical.text.cell_column >= 0)
      return (LENGTHTYPE)logical.text.cell_column;
   return (CURRENT_VIEW->current_column > 0)
        ? CURRENT_VIEW->current_column - 1
        : 0;
}

static int query2_filearea_cursor_row(short *row)
{
   LogicalCursor logical;
   short y;

   if (CURRENT_VIEW->current_window != WINDOW_FILEAREA)
      return FALSE;
   logical = CURRENT_VIEW->logical_cursor.current;
   if (logical.valid
   &&  logical.zone == LOGICAL_CURSOR_ZONE_FILEAREA
   &&  logical.line_number == CURRENT_VIEW->focus_line
   &&  logical.zone_row >= 0
   &&  logical.zone_row < CURRENT_SCREEN.rows[WINDOW_FILEAREA])
   {
      if (row != NULL)
         *row = (short)logical.zone_row;
      return TRUE;
   }
   if (!line_in_view(current_screen,CURRENT_VIEW->focus_line))
      return FALSE;
   y = get_row_for_focus_line(current_screen,CURRENT_VIEW->focus_line,
                              CURRENT_VIEW->current_row);
   if (y < 0 || y >= CURRENT_SCREEN.rows[WINDOW_FILEAREA])
      return FALSE;
   if (row != NULL)
      *row = y;
   return TRUE;
}

static int query2_filearea_cursor_screen_position(short *row, short *col)
{
   short y;
   LENGTHTYPE screen_col;

   if (!query2_filearea_cursor_row(&y))
      return FALSE;
   screen_col = query2_filearea_cursor_cell() - (CURRENT_VIEW->verify_col - 1);
   if (screen_col < 0 || screen_col >= CURRENT_SCREEN.cols[WINDOW_FILEAREA])
      return FALSE;
   if (row != NULL)
      *row = y;
   if (col != NULL)
      *col = (short)screen_col;
   return TRUE;
}

static int query2_filearea_cursor_is_shadow(void)
{
   short row=0;

   if (!query2_filearea_cursor_row(&row))
      return FALSE;
   return (CURRENT_SCREEN.sl[row].line_type == LINE_SHADOW);
}

/***********************************************************************/
short extract_macro(short number_variables,short itemno,CHARTYPE *itemargs,CHARTYPE query_type,LINETYPE argc,CHARTYPE *arg,LINETYPE arglen)
/***********************************************************************/
{
   return set_on_off_value(CURRENT_VIEW->macro,1);
}
/***********************************************************************/
short extract_macroext(short number_variables,short itemno,CHARTYPE *itemargs,CHARTYPE query_type,LINETYPE argc,CHARTYPE *arg,LINETYPE arglen)
/***********************************************************************/
{
   if (strlen((DEFCHAR *)macro_suffix) == 0)
      item_values[1].value = (CHARTYPE *)macro_suffix;
   else
      item_values[1].value = (CHARTYPE *)macro_suffix+1;
   item_values[1].len = strlen((DEFCHAR *)item_values[1].value);
   return number_variables;
}
/***********************************************************************/
short extract_macropath(short number_variables,short itemno,CHARTYPE *itemargs,CHARTYPE query_type,LINETYPE argc,CHARTYPE *arg,LINETYPE arglen)
/***********************************************************************/
{
   item_values[1].value = (CHARTYPE *)the_macro_path;
   item_values[1].len = strlen((DEFCHAR *)the_macro_path);
   return number_variables;
}
/***********************************************************************/
short extract_margins(short number_variables,short itemno,CHARTYPE *itemargs,CHARTYPE query_type,LINETYPE argc,CHARTYPE *arg,LINETYPE arglen)
/***********************************************************************/
{
   sprintf((DEFCHAR *)query_num1,"%ld",CURRENT_VIEW->margin_left);
   item_values[1].value = query_num1;
   item_values[1].len = strlen((DEFCHAR *)query_num1);
   sprintf((DEFCHAR *)query_num2,"%ld",CURRENT_VIEW->margin_right);
   item_values[2].value = query_num2;
   item_values[2].len = strlen((DEFCHAR *)query_num2);
   if (CURRENT_VIEW->margin_indent_offset_status)
      sprintf((DEFCHAR *)query_num3,"%+ld",CURRENT_VIEW->margin_indent);
   else
      sprintf((DEFCHAR *)query_num3,"%ld",CURRENT_VIEW->margin_indent);
   item_values[3].value = query_num3;
   item_values[3].len = strlen((DEFCHAR *)query_num3);
   return number_variables;
}
/***********************************************************************/
short extract_modifiable_function(short number_variables,short itemno,CHARTYPE *itemargs,CHARTYPE query_type,LINETYPE argc,CHARTYPE *arg,LINETYPE arglen)
/***********************************************************************/
{
   bool bool_flag=FALSE;

   switch(CURRENT_VIEW->current_window)
   {
      case WINDOW_FILEAREA:
         if (batch_only)
         {
             bool_flag = FALSE;
             break;
         }
         if (FOCUS_TOF
         ||  FOCUS_BOF
         ||  query2_filearea_cursor_is_shadow())
             bool_flag = FALSE;
         else
             bool_flag = TRUE;
         break;
      default:
         bool_flag = TRUE;
         break;
   }
   return set_boolean_value((bool)bool_flag,(short)1);
}
/***********************************************************************/
short extract_monitor(short number_variables,short itemno,CHARTYPE *itemargs,CHARTYPE query_type,LINETYPE argc,CHARTYPE *arg,LINETYPE arglen)
/***********************************************************************/
{
#ifdef A_COLOR
   if (colour_support)
   {
      item_values[1].value = (CHARTYPE *)"COLOR";
      item_values[1].len = 5;
   }
   else
   {
      item_values[1].value = (CHARTYPE *)"MONO";
      item_values[1].len = 4;
   }
   item_values[2].value = (CHARTYPE *)"COLOR";
   item_values[2].len = 5;
#else
   item_values[1].value = (CHARTYPE *)"MONO";
   item_values[1].len = 4;
   item_values[2].value = (CHARTYPE *)"MONO";
   item_values[2].len = 4;
#endif
   return number_variables;
}
/***********************************************************************/
short extract_mouse(short number_variables,short itemno,CHARTYPE *itemargs,CHARTYPE query_type,LINETYPE argc,CHARTYPE *arg,LINETYPE arglen)
/***********************************************************************/
{
   return set_on_off_value(MOUSEx,1);
}
/***********************************************************************/
short extract_mouseclick(short number_variables,short itemno,CHARTYPE *itemargs,CHARTYPE query_type,LINETYPE argc,CHARTYPE *arg,LINETYPE arglen)
/***********************************************************************/
{
   int mouseclick = -1;
#if defined(PDCURSES_MOUSE_ENABLED) || defined(NCURSES_MOUSE_VERSION)
   mouseclick = mouseinterval( -1 );
#endif
   sprintf((DEFCHAR *)query_num3,"%d",mouseclick);
   item_values[1].value = query_num3;
   item_values[1].len = strlen((DEFCHAR *)query_num3);
   return number_variables;
}
/***********************************************************************/
short extract_msgline(short number_variables,short itemno,CHARTYPE *itemargs,CHARTYPE query_type,LINETYPE argc,CHARTYPE *arg,LINETYPE arglen)
/***********************************************************************/
{
   item_values[1].value = (CHARTYPE *)"ON";
   item_values[1].len = 2;
   if (CURRENT_VIEW->msgline_base == POSITION_MIDDLE)
      sprintf((DEFCHAR *)query_rsrvd,"M%+d",CURRENT_VIEW->msgline_off);
   else
      sprintf((DEFCHAR *)query_rsrvd,"%d",CURRENT_VIEW->msgline_off);
   item_values[2].value = query_rsrvd;
   item_values[2].len = strlen((DEFCHAR *)query_rsrvd);
   if ( CURRENT_VIEW->msgline_rows )
   {
      sprintf((DEFCHAR *)query_num1,"%d",CURRENT_VIEW->msgline_rows);
      item_values[3].value = query_num1;
   }
   else
      item_values[3].value = (CHARTYPE *)"*";
   item_values[3].len = strlen((DEFCHAR *)query_num1);
   item_values[4].value = (CHARTYPE *)"OVERLAY";
   item_values[4].len = 7;
   return number_variables;
}
/***********************************************************************/
short extract_msgmode(short number_variables,short itemno,CHARTYPE *itemargs,CHARTYPE query_type,LINETYPE argc,CHARTYPE *arg,LINETYPE arglen)
/***********************************************************************/
{
   set_on_off_value( CURRENT_VIEW->msgmode_status, 1 );
   item_values[2].value = (CHARTYPE *)"LONG";
   item_values[2].len = 4;
   return number_variables;
}
/***********************************************************************/
short extract_messages(short number_variables,short itemno,CHARTYPE *itemargs,CHARTYPE query_type,LINETYPE argc,CHARTYPE *arg,LINETYPE arglen)
/***********************************************************************/
{
   int count = message_history_count();
   int limit = count;
   int start = 0;
   int i;
   short rc = RC_OK;
   CHARTYPE num[20];

   INTENTIONALLY_UNUSED_VARIABLE(argc);
   INTENTIONALLY_UNUSED_VARIABLE(arg);
   INTENTIONALLY_UNUSED_VARIABLE(arglen);
   INTENTIONALLY_UNUSED_VARIABLE(number_variables);

   if (itemargs != NULL
   &&  !blank_field(itemargs)
   &&  strcmp((DEFCHAR *)itemargs, "*") != 0)
   {
      if (!valid_positive_integer(itemargs))
      {
         display_error(1,itemargs,FALSE);
         return EXTRACT_ARG_ERROR;
      }
      limit = atoi((DEFCHAR *)itemargs);
      if (limit > count)
         limit = count;
   }
   start = count - limit;
   if (start < 0)
      start = 0;

   if (query_type == QUERY_QUERY)
   {
      if (CURRENT_VIEW != NULL && limit > 0)
      {
         ROWTYPE save_msgline_rows = CURRENT_VIEW->msgline_rows;
         bool save_msgmode_status = CURRENT_VIEW->msgmode_status;

         CURRENT_VIEW->msgline_rows = (terminal_lines > 1)
                                    ? min(terminal_lines - 1, limit)
                                    : limit;
         CURRENT_VIEW->msgmode_status = TRUE;
         rc = expose_msgline();
         CURRENT_VIEW->msgline_rows = save_msgline_rows;
         CURRENT_VIEW->msgmode_status = save_msgmode_status;
      }
      INTENTIONALLY_UNUSED_VARIABLE(rc);
      return EXTRACT_VARIABLES_SET;
   }

   if (query_type == QUERY_EXTRACT)
   {
      sprintf((DEFCHAR *)num,"%d",limit);
      rc = set_rexx_variable(query_item[itemno].name,num,
                             strlen((DEFCHAR *)num),0);
      for (i = 0; rc == RC_OK && i < limit; i++)
      {
         LENGTHTYPE len = 0;
         const CHARTYPE *message = message_history_get(start + i, &len);

         if (message == NULL)
         {
            message = (const CHARTYPE *)"";
            len = 0;
         }
         rc = set_rexx_variable(query_item[itemno].name,
                                (CHARTYPE *)message,len,i + 1);
      }
      if (rc == RC_SYSTEM_ERROR)
      {
         display_error(54,(CHARTYPE *)"",FALSE);
         return EXTRACT_ARG_ERROR;
      }
      return EXTRACT_VARIABLES_SET;
   }

   return limit;
}
/***********************************************************************/
short extract_nbfile(short number_variables,short itemno,CHARTYPE *itemargs,CHARTYPE query_type,LINETYPE argc,CHARTYPE *arg,LINETYPE arglen)
/***********************************************************************/
{
   sprintf((DEFCHAR *)query_num1,"%ld",number_of_files);
   item_values[1].value = query_num1;
   item_values[1].len = strlen((DEFCHAR *)query_num1);
   return number_variables;
}
/***********************************************************************/
short extract_nbscope(short number_variables,short itemno,CHARTYPE *itemargs,CHARTYPE query_type,LINETYPE argc,CHARTYPE *arg,LINETYPE arglen)
/***********************************************************************/
{
   LINETYPE number_lines = 0;

   if ( CURRENT_VIEW->scope_all )
   {
      sprintf((DEFCHAR *)query_num1,"%ld",CURRENT_FILE->number_lines);
   }
   else
   {
      for( number_lines=0L, curr=CURRENT_FILE->first_line; curr!=NULL; curr=curr->next )
      {
        if ( curr->prev == NULL
        ||   curr->next == NULL )
           continue;
         if ( IN_SCOPE( CURRENT_VIEW, curr) )
            number_lines++;
      }
      sprintf( (DEFCHAR *)query_num1, "%ld", number_lines );
   }

   sprintf( (DEFCHAR *)query_num2, "%ld", get_true_line( TRUE ) );
   item_values[1].value = query_num1;
   item_values[1].len = strlen( (DEFCHAR *)query_num1 );
   item_values[2].value = query_num2;
   item_values[2].len = strlen( (DEFCHAR *)query_num2 );
   return number_variables;
}
/***********************************************************************/
short extract_newlines(short number_variables,short itemno,CHARTYPE *itemargs,CHARTYPE query_type,LINETYPE argc,CHARTYPE *arg,LINETYPE arglen)
/***********************************************************************/
{
   if (CURRENT_VIEW->newline_aligned)
   {
      item_values[1].value = (CHARTYPE *)"ALIGNED";
      item_values[1].len = 7;
   }
   else
   {
      item_values[1].value = (CHARTYPE *)"LEFT";
      item_values[1].len = 4;
   }
   return number_variables;
}
/***********************************************************************/
short extract_nondisp(short number_variables,short itemno,CHARTYPE *itemargs,CHARTYPE query_type,LINETYPE argc,CHARTYPE *arg,LINETYPE arglen)
/***********************************************************************/
{
   query_num1[0] = NONDISPx;
   query_num1[1] ='\0';
   item_values[1].value = query_num1;
   item_values[1].len = 1;
   return number_variables;
}
/***********************************************************************/
short extract_number(short number_variables,short itemno,CHARTYPE *itemargs,CHARTYPE query_type,LINETYPE argc,CHARTYPE *arg,LINETYPE arglen)
/***********************************************************************/
{
   return set_on_off_value(CURRENT_VIEW->number,1);
}
/***********************************************************************/
short extract_pagewrap(short pagewrap_variables,short itemno,CHARTYPE *itemargs,CHARTYPE query_type,LINETYPE argc,CHARTYPE *arg,LINETYPE arglen)
/***********************************************************************/
{
   return set_on_off_value(PAGEWRAPx,1);
}
/***********************************************************************/
short extract_parser(short number_variables,short itemno,CHARTYPE *itemargs,CHARTYPE query_type,LINETYPE argc,CHARTYPE *arg,LINETYPE arglen)
/***********************************************************************/
{
   short rc=RC_OK;
   register int i=0;
   int off=0;
   bool found=FALSE;
   ROWTYPE save_msgline_rows = CURRENT_VIEW->msgline_rows;
   bool save_msgmode_status = CURRENT_VIEW->msgmode_status;
   CHARTYPE *ptr_filename=NULL;
   PARSER_DETAILS *curr;

   if (itemargs == NULL
   ||  blank_field(itemargs)
   ||  strcmp((DEFCHAR*)itemargs,"*") == 0)
   {
      if (query_type == QUERY_QUERY)
      {
         for (i=0,curr=first_parser;curr!=NULL;curr=curr->next,i++);
         CURRENT_VIEW->msgline_rows   = min(terminal_lines-1,i);
         CURRENT_VIEW->msgmode_status = TRUE;
      }
      else
         number_variables = 0;
      for (curr=first_parser;curr!=NULL;curr=curr->next)
      {
         sprintf((DEFCHAR *)query_rsrvd,"%s%s %s",
           (query_type == QUERY_QUERY) ? (DEFCHAR *)"parser " : "",
           curr->parser_name,
           curr->filename);

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
   }
   else
   {
      if (query_type == QUERY_QUERY)
      {
         CURRENT_VIEW->msgline_rows   = 1;
         CURRENT_VIEW->msgmode_status = TRUE;
      }
      /*
       * Find a match for the supplied mask or magic number
       */
      for (curr=first_parser;curr!=NULL;curr=curr->next)
      {
         if (my_stricmp((DEFCHAR *)itemargs,(DEFCHAR *)curr->parser_name) == 0)
         {
            ptr_filename = curr->filename;
            found = TRUE;
            break;
         }
      }
      if (!found)
      {
         ptr_filename = (CHARTYPE *)"";
      }
      if (query_type == QUERY_QUERY)
      {
         sprintf((DEFCHAR *)query_rsrvd,"%s%s %s",
            (query_type == QUERY_QUERY) ? (DEFCHAR *)"parser " : "",
            itemargs,
            ptr_filename);
         display_error(0,query_rsrvd,TRUE);
      }
      else
      {
         item_values[1].value = itemargs;
         item_values[1].len = strlen((DEFCHAR *)itemargs);
         item_values[2].value = ptr_filename;
         item_values[2].len = strlen((DEFCHAR *)ptr_filename);
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

/*
                                 +-- * ---+
<---+---------+--+-----------+---+- name -+---+---------------------------------+--->
    +- BLOCK -+  +- OLDNAME -+                |   +--- :1 ---+   +--- * ----+   |
                                              +---+- target -+---+- target -+---+
*/
/***********************************************************************/
short extract_pmsg(short number_variables,short itemno,CHARTYPE *itemargs,CHARTYPE query_type,LINETYPE argc,CHARTYPE *arg,LINETYPE arglen)
/***********************************************************************/
{
   LINETYPE screen_line=0;
   LENGTHTYPE screen_column=0;
   LINETYPE current_file_line=(-1L);
   LENGTHTYPE current_file_column=(-1);

   strcpy((DEFCHAR *)query_rsrvd, "");
   current_parser_severity = 0;
   get_cursor_position(&screen_line, &screen_column, &current_file_line, &current_file_column);

#ifdef USE_SDSLH
   enter_codeblock_critical_section();
   if (CURRENT_FILE && CURRENT_FILE->cb && current_file_line > 0 && current_file_line <= (LINETYPE)CURRENT_FILE->cb->line_count) {
       CodeBufferLine *line = &CURRENT_FILE->cb->lines[current_file_line - 1];
       if (current_file_column > 0 && current_file_column <= line->length) {
           CodeBufferCharacter *ch = &line->characters[current_file_column - 1];
           if (ch->node && ch->node->message) {
               strncpy((DEFCHAR *)query_rsrvd, ch->node->message, sizeof(query_rsrvd) - 1);
               query_rsrvd[sizeof(query_rsrvd) - 1] = '\0';
               current_parser_severity = ch->node->severity;
           }
       }
   }
   exit_codeblock_critical_section();
#endif
   item_values[1].value = query_rsrvd;
   item_values[1].len = strlen((DEFCHAR *)query_rsrvd);
   
   return number_variables;
}

/***********************************************************************/

#ifdef USE_SDSLH
typedef struct SdslhParserMessage
{
   const CB_Node *node;
   LINETYPE line;
   LENGTHTYPE column;
   CB_Severity severity;
   CHARTYPE *code;
   CHARTYPE *message;
} SdslhParserMessage;

typedef struct PmsgsCollectContext
{
   CodeBuffer *cb;
   SdslhParserMessage **messages;
   int *count;
   int *capacity;
   short rc;
} PmsgsCollectContext;

static const char *pmsgs_severity_name(CB_Severity severity)
{
   switch(severity)
   {
      case CB_INFORMATION:
         return "INFORMATION";
      case CB_WARNING:
         return "WARNING";
      case CB_ERROR:
         return "ERROR";
      case CB_NONE:
      default:
         return "NONE";
   }
}

static CHARTYPE *pmsgs_duplicate_field(const char *value,const char *fallback)
{
   const char *source=(value != NULL && value[0] != '\0') ? value : fallback;
   size_t len=strlen(source);
   CHARTYPE *copy=(CHARTYPE *)(*the_malloc)((len + 1) * sizeof(CHARTYPE));

   if (copy == NULL)
      return NULL;

   memcpy(copy,source,len + 1);
   return copy;
}

static void pmsgs_free(SdslhParserMessage *messages,int count)
{
   int i;

   if (messages == NULL)
      return;

   for (i = 0; i < count; i++)
   {
      if (messages[i].code != NULL)
         (*the_free)(messages[i].code);
      if (messages[i].message != NULL)
         (*the_free)(messages[i].message);
   }
   (*the_free)(messages);
}

static short pmsgs_append(SdslhParserMessage **messages,int *count,int *capacity,
                          const CB_Node *node,LINETYPE line,LENGTHTYPE column)
{
   SdslhParserMessage *resized=NULL;
   SdslhParserMessage *entry=NULL;
   int next_capacity;

   if (*count == *capacity)
   {
      next_capacity = (*capacity == 0) ? 8 : (*capacity * 2);
      if (*messages == NULL)
         resized = (SdslhParserMessage *)(*the_malloc)(next_capacity * sizeof(SdslhParserMessage));
      else
         resized = (SdslhParserMessage *)(*the_realloc)(*messages,next_capacity * sizeof(SdslhParserMessage));

      if (resized == NULL)
         return RC_SYSTEM_ERROR;

      *messages = resized;
      *capacity = next_capacity;
   }

   entry = &(*messages)[*count];
   entry->node = node;
   entry->line = line;
   entry->column = column;
   entry->severity = node->severity;
   entry->code = pmsgs_duplicate_field(node->message_code,"-");
   entry->message = pmsgs_duplicate_field(node->message,"");

   if (entry->code == NULL || entry->message == NULL)
   {
      if (entry->code != NULL)
         (*the_free)(entry->code);
      if (entry->message != NULL)
         (*the_free)(entry->message);
      entry->code = NULL;
      entry->message = NULL;
      return RC_SYSTEM_ERROR;
   }

   (*count)++;
   return RC_OK;
}

static void pmsgs_node_location(CodeBuffer *cb,const CB_Node *node,LINETYPE *line,LENGTHTYPE *column)
{
   size_t found_line=0;
   size_t found_col=0;
   size_t current_pos=0;
   size_t line_idx;
   size_t length;
   size_t offset;

   *line = 0;
   *column = 0;

   if (cb == NULL || node == NULL || cb->line_count == 0)
      return;

   length = (node->length > 0) ? node->length : 1;
   if (get_code_buffer_part(cb,node->pos,length,&found_line,&found_col,NULL) != NULL)
   {
      *line = (LINETYPE)found_line;
      *column = (LENGTHTYPE)found_col + 1;
      return;
   }

   for (line_idx = 0; line_idx < cb->line_count; line_idx++)
   {
      size_t line_length=cb->lines[line_idx].length;
      size_t span=line_length + 1;

      if (node->pos >= current_pos && node->pos <= current_pos + span)
      {
         offset = node->pos - current_pos;
         if (offset > line_length)
            offset = line_length;
         *line = (LINETYPE)line_idx + 1;
         *column = (LENGTHTYPE)offset + 1;
         return;
      }
      current_pos += span;
   }

   *line = (LINETYPE)cb->line_count;
   *column = (LENGTHTYPE)cb->lines[cb->line_count - 1].length + 1;
}

static void pmsgs_collect_node(CB_Node *node,size_t depth,void *user_data)
{
   PmsgsCollectContext *context=(PmsgsCollectContext *)user_data;
   LINETYPE line=0;
   LENGTHTYPE column=0;

   INTENTIONALLY_UNUSED_VARIABLE(depth);

   if (context == NULL || context->rc != RC_OK || node == NULL)
      return;

   if (node->message == NULL
   ||  node->message[0] == '\0'
   ||  node->severity == CB_NONE)
      return;

   pmsgs_node_location(context->cb,node,&line,&column);
   context->rc = pmsgs_append(context->messages,context->count,context->capacity,
                              node,line,column);
}

static short pmsgs_collect(SdslhParserMessage **messages,int *count)
{
   short rc=RC_OK;
   int capacity=0;
   CodeBuffer *cb=NULL;
   PmsgsCollectContext context;

   *messages = NULL;
   *count = 0;

   if (CURRENT_FILE == NULL || CURRENT_FILE->cb == NULL)
      return RC_OK;

   if (enter_codeblock_critical_section() != 0)
      return RC_SYSTEM_ERROR;

   cb = CURRENT_FILE->cb;
   if (cb->parse_tree != NULL)
   {
      context.cb = cb;
      context.messages = messages;
      context.count = count;
      context.capacity = &capacity;
      context.rc = RC_OK;
      cb_walk_tree_top_down(cb->parse_tree,pmsgs_collect_node,&context);
      rc = context.rc;
   }

   if (exit_codeblock_critical_section() != 0 && rc == RC_OK)
      rc = RC_SYSTEM_ERROR;

   if (rc != RC_OK)
   {
      pmsgs_free(*messages,*count);
      *messages = NULL;
      *count = 0;
   }

   return rc;
}

static CHARTYPE *pmsgs_make_record(const SdslhParserMessage *message,bool query_record,int index)
{
   const char *severity=pmsgs_severity_name(message->severity);
   const char *code=(const char *)message->code;
   const char *text=(const char *)message->message;
   size_t size=strlen(severity) + strlen(code) + strlen(text) + 128;
   CHARTYPE *record=(CHARTYPE *)(*the_malloc)(size * sizeof(CHARTYPE));

   if (record == NULL)
      return NULL;

   if (query_record)
   {
      snprintf((DEFCHAR *)record,size,
               "pmsgs.%d line=%ld column=%ld severity=%s code=%s message=%s",
               index,(long)message->line,(long)message->column,severity,code,text);
   }
   else
   {
      snprintf((DEFCHAR *)record,size,"%ld %ld %s %s %s",
               (long)message->line,(long)message->column,severity,code,text);
   }

   return record;
}
#endif

/***********************************************************************/
short extract_pmsgs(short number_variables,short itemno,CHARTYPE *itemargs,CHARTYPE query_type,LINETYPE argc,CHARTYPE *arg,LINETYPE arglen)
/***********************************************************************/
{
   short rc=RC_OK;
   CHARTYPE num[20];
#ifdef USE_SDSLH
   SdslhParserMessage *messages=NULL;
   int count=0;
   int i;
   CHARTYPE *record=NULL;
#endif

   INTENTIONALLY_UNUSED_VARIABLE(argc);
   INTENTIONALLY_UNUSED_VARIABLE(arg);
   INTENTIONALLY_UNUSED_VARIABLE(arglen);
   INTENTIONALLY_UNUSED_VARIABLE(number_variables);

   if (itemargs != NULL && !blank_field(itemargs))
   {
      display_error(1,itemargs,FALSE);
      return EXTRACT_ARG_ERROR;
   }

#ifndef USE_SDSLH
   if (query_type == QUERY_QUERY)
   {
      display_error(0,(CHARTYPE *)"SDSLH support not compiled in",TRUE);
      return EXTRACT_VARIABLES_SET;
   }

   if (query_type == QUERY_EXTRACT)
   {
      strcpy((DEFCHAR *)num,"0");
      rc = set_rexx_variable(query_item[itemno].name,num,strlen((DEFCHAR *)num),0);
      if (rc == RC_SYSTEM_ERROR)
      {
         display_error(54,(CHARTYPE *)"",FALSE);
         return EXTRACT_ARG_ERROR;
      }
      return EXTRACT_VARIABLES_SET;
   }

   return 0;
#else
   rc = pmsgs_collect(&messages,&count);
   if (rc != RC_OK)
   {
      display_error(54,(CHARTYPE *)"",FALSE);
      return EXTRACT_ARG_ERROR;
   }

   if (query_type == QUERY_QUERY)
   {
      if (count == 0)
      {
         display_error(0,(CHARTYPE *)"No SDSLH parser messages",TRUE);
      }
      else
      {
         for (i = 0; i < count; i++)
         {
            record = pmsgs_make_record(&messages[i],TRUE,i + 1);
            if (record == NULL)
            {
               pmsgs_free(messages,count);
               display_error(54,(CHARTYPE *)"",FALSE);
               return EXTRACT_ARG_ERROR;
            }
            display_error(0,record,TRUE);
            (*the_free)(record);
         }
      }

      pmsgs_free(messages,count);
      return EXTRACT_VARIABLES_SET;
   }

   if (query_type == QUERY_EXTRACT)
   {
      sprintf((DEFCHAR *)num,"%d",count);
      rc = set_rexx_variable(query_item[itemno].name,num,strlen((DEFCHAR *)num),0);
      for (i = 0; rc == RC_OK && i < count; i++)
      {
         record = pmsgs_make_record(&messages[i],FALSE,i + 1);
         if (record == NULL)
         {
            rc = RC_SYSTEM_ERROR;
            break;
         }
         rc = set_rexx_variable(query_item[itemno].name,record,
                                strlen((DEFCHAR *)record),i + 1);
         (*the_free)(record);
      }

      pmsgs_free(messages,count);
      if (rc == RC_SYSTEM_ERROR)
      {
         display_error(54,(CHARTYPE *)"",FALSE);
         return EXTRACT_ARG_ERROR;
      }
      return EXTRACT_VARIABLES_SET;
   }

   pmsgs_free(messages,count);
   return count;
#endif
}

/***********************************************************************/

short extract_pending(short number_variables,short itemno,CHARTYPE *itemargs,CHARTYPE query_type,LINETYPE argc,CHARTYPE *arg,LINETYPE arglen)
/***********************************************************************/
{
#define PEN_PARAMS  6
#define STATE_START    0
#define STATE_OLDNAME  1
#define STATE_NAME     2
#define STATE_TARGET1  3
#define STATE_TARGET2  4
   CHARTYPE *word[PEN_PARAMS+1];
   CHARTYPE strip[PEN_PARAMS];
   unsigned short num_params=0;
   bool find_block=FALSE;
   bool find_oldname=FALSE;
   CHARTYPE *name=NULL;
   THE_PPC *curr_ppc=NULL;
   LINE *curr;
   LINETYPE first_in_range=0L;
   LINETYPE last_in_range=CURRENT_FILE->number_lines+1;
   THE_PPC *found_ppc=NULL;
   short target_type=TARGET_ABSOLUTE|TARGET_RELATIVE;
   TARGET target;
   short rc;
   short state=STATE_START;
   int i,j;

   strip[0]=STRIP_BOTH;
   strip[1]=STRIP_BOTH;
   strip[2]=STRIP_BOTH;
   strip[3]=STRIP_BOTH;
   strip[4]=STRIP_BOTH;
   strip[5]=STRIP_BOTH;
   num_params = param_split(itemargs,word,PEN_PARAMS,WORD_DELIMS,TEMP_PARAM,strip,FALSE);
   if ( num_params == 0 )
   {
      /*
       * No parameters, error.
       */
      display_error(3,(CHARTYPE *)"",FALSE);
      return EXTRACT_ARG_ERROR;
   }

   i = 0;
   for ( ; ; )
   {
      if ( i == num_params )
         break;
      switch( state )
      {
         case STATE_START:
            if ( equal( (CHARTYPE *)"block", word[i], 5 ) )
            {
               find_block = TRUE;
               state = STATE_OLDNAME;
               i++;
               break;
            }
            if ( equal( (CHARTYPE *)"oldname", word[i], 7 ) )
            {
               find_oldname = TRUE;
               state = STATE_NAME;
               i++;
               break;
            }
            state = STATE_NAME;
            break;
         case STATE_OLDNAME:
            if ( equal( (CHARTYPE *)"oldname", word[i], 7 ) )
            {
               find_oldname = TRUE;
               state = STATE_NAME;
               i++;
               break;
            }
            state = STATE_NAME;
            break;
         case STATE_NAME:
            name = word[i];
            state = STATE_TARGET1;
            i++;
            break;
         case STATE_TARGET1:
            initialise_target(&target);
            rc = validate_target(word[i],&target,target_type,0L,FALSE,FALSE);
            if (rc == RC_OK)
            {
               first_in_range = target.rt[0].numeric_target;
               i++;
               state = STATE_TARGET2;
            }
            else
            {
               /* error */
               number_variables = EXTRACT_ARG_ERROR;
            }
            free_target(&target);
            break;
         case STATE_TARGET2:
            if ( equal( (CHARTYPE *)"*", word[i], 1 ) )
            {
               /*
                * If the second target is *, then default last_in_range to be
                * 1 more than the number of lines in the file; same as if no
                * second target was specified.
                */
               last_in_range = CURRENT_FILE->number_lines + 1;
               i++;
               state = STATE_TARGET2;
            }
            else
            {
               initialise_target(&target);
               rc = validate_target(word[i],&target,target_type,first_in_range,FALSE,FALSE);
               if (rc == RC_OK)
               {
                  last_in_range = target.rt[0].numeric_target;
                  i++;
                  state = STATE_TARGET2;
               }
               else
               {
                  /* error */
                  number_variables = EXTRACT_ARG_ERROR;
               }
               free_target(&target);
            }
            break;
      }
      if ( number_variables == EXTRACT_ARG_ERROR )
         break;
   }
   /*
    * If the validation of parameters is successful...
    */
   if (number_variables >= 0)
   {
      /*
       * No pending prefix commands, return 0.
       */
      if (CURRENT_FILE->first_ppc == NULL)
         number_variables = 0;
      else
      {
         /*
          * If we are to look for OLDNAME, find a synonym for it if one exists..
          */
         if (find_oldname)
            name = find_prefix_oldname(name);
         /*
          * For each pending prefix command...
          */
         curr_ppc = CURRENT_FILE->first_ppc;
         for ( ; ; )
         {
            if (curr_ppc == NULL)
               break;
            /*
             * Ignore the pending prefix if we have already processed it
             */
            if ( curr_ppc->ppc_processed
            ||   curr_ppc->ppc_current_command )
            {
               curr_ppc = curr_ppc->next;
               continue;
            }
            /*
             * To imitate XEDIT behaviour, ignore a prefix command if the line on which the prefix command
             * has been entered is not in scope.
             */
            curr = lll_find( CURRENT_FILE->first_line, CURRENT_FILE->last_line, curr_ppc->ppc_line_number, CURRENT_FILE->number_lines );
            if ( !( IN_SCOPE( CURRENT_VIEW, curr )
            ||   CURRENT_VIEW->scope_all
            ||   curr_ppc->ppc_shadow_line ) )
            {
               curr_ppc = curr_ppc->next;
               continue;
            }
            /*
             * If we want to match on any name...
             */
            if (strcmp((DEFCHAR *)name,"*") == 0)
            {
               /*
                * Are we matching on any BLOCK command...
                */
               if (find_block)
               {
                  if (curr_ppc->ppc_block_command)
                  {
                     /*
                      * We have found the first BLOCK command with any name.
                      */
                     if ( found_ppc == NULL )
                        found_ppc = curr_ppc;
                     else
                     {
                        if ( curr_ppc->ppc_line_number < found_ppc->ppc_line_number )
                           found_ppc = curr_ppc;
                     }
                  }
                  /*
                   * Go back and look for another either because we didn't
                   * find a block command, or because we did, but it may not
                   * be the one with the smallest line number.
                   */
                  curr_ppc = curr_ppc->next;
                  continue;
               }
               else
               {
                  /*
                   * We have found the first command with any name.
                   */
                  found_ppc = in_range( found_ppc, curr_ppc, first_in_range, last_in_range );
                  /*
                   * Go back and look for another because it may not
                   * be the one with the smallest line number.
                   */
                  curr_ppc = curr_ppc->next;
                  continue;
               }
            }
            /*
             * We want to find a specific command...
             */
            if ( my_stricmp( (DEFCHAR *)curr_ppc->ppc_command, (DEFCHAR *)name ) == 0 )
            {
               /*
                * Are we looking for a specific BLOCK command...
                */
               if ( find_block )
               {
                  if ( curr_ppc->ppc_block_command )
                  {
                     /*
                      * We have found the first specific BLOCK command.
                      */
                     found_ppc = in_range( found_ppc, curr_ppc, first_in_range, last_in_range );
                  }
               }
               else
               {
                  /*
                   * We have found the first specific command.
                   */
                  found_ppc = in_range( found_ppc, curr_ppc, first_in_range, last_in_range );
               }
               /*
                * Go back and look for another because it may not
                * be the one with the smallest line number.
                */
               curr_ppc = curr_ppc->next;
               continue;
            }
            curr_ppc = curr_ppc->next;
         }
         /*
          * Did we find a matching pending prefix command ?
          */
         if (found_ppc == NULL)
            number_variables = 0;
         else
         {
            /*
             * Yes we did. Set all of the REXX variables to the correct values...
             */
            sprintf( (DEFCHAR *)query_num1, "%ld", found_ppc->ppc_line_number );
            item_values[1].value = query_num1;
            item_values[1].len = strlen( (DEFCHAR *)query_num1 );
            item_values[2].value = found_ppc->ppc_command;
            item_values[2].len = strlen( (DEFCHAR *)item_values[2].value );
            item_values[3].value = find_prefix_synonym( found_ppc->ppc_command );
            item_values[3].len = strlen( (DEFCHAR *)item_values[3].value );
            if (found_ppc->ppc_block_command)
               item_values[4].value = (CHARTYPE *)"BLOCK";
            else
               item_values[4].value = (CHARTYPE *)"";
            item_values[4].len = strlen( (DEFCHAR *)item_values[4].value );
            for ( i = 0; i < PPC_OPERANDS; i++ )
            {
               j = i+5;
               item_values[j].value = found_ppc->ppc_op[i];
               item_values[j].len = strlen( (DEFCHAR *)item_values[j].value );
            }
            number_variables = PPC_OPERANDS + 4;
         }
      }
   }
   return number_variables;
}
/***********************************************************************/
short extract_point(short number_variables,short itemno,CHARTYPE *itemargs,CHARTYPE query_type,LINETYPE argc,CHARTYPE *arg,LINETYPE arglen)
/***********************************************************************/
{
   LINETYPE true_line=0;
   int len,total_len=0;
   THELIST *curr_name;

   switch(query_type)
   {
      case QUERY_QUERY:
      case QUERY_MODIFY:
         true_line = (compatible_feel==COMPAT_XEDIT)?CURRENT_VIEW->current_line:get_true_line(TRUE);
         curr = lll_find(CURRENT_FILE->first_line,CURRENT_FILE->last_line,true_line,CURRENT_FILE->number_lines);
         if (curr->first_name == NULL)  /* line not named */
         {
            item_values[1].value = (CHARTYPE *)"";
            item_values[1].len = 0;
         }
         else
         {
            strcpy( (DEFCHAR *)query_rsrvd, "" );
            curr_name = curr->first_name;
            while( curr_name )
            {
               len = strlen( (DEFCHAR *)curr_name->data );
               if ( total_len + len + 1 > sizeof(query_rsrvd) )
               {
                  break;
               }
               total_len += len+1;
               strcat( (DEFCHAR *)query_rsrvd, " " );
               strcat( (DEFCHAR *)query_rsrvd, (DEFCHAR *)curr_name->data );
               curr_name = curr_name->next;
            }
            item_values[1].value = query_rsrvd;
            item_values[1].len = total_len;
         }
         break;
      default:
         number_variables = extract_point_settings(itemno,itemargs);
         break;
   }
   return number_variables;
}
/***********************************************************************/
short extract_position(short number_variables,short itemno,CHARTYPE *itemargs,CHARTYPE query_type,LINETYPE argc,CHARTYPE *arg,LINETYPE arglen)
/***********************************************************************/
{
   LINETYPE true_line=0;
   LENGTHTYPE col=0;

   set_on_off_value(CURRENT_VIEW->position_status,1);
   if ((query_type == QUERY_EXTRACT
     ||  query_type == QUERY_FUNCTION)
   &&  !batch_only)
   {
      get_current_position(current_screen,&true_line,&col);
      sprintf((DEFCHAR *)query_num1,"%ld",true_line);
      item_values[2].value = query_num1;
      item_values[2].len = strlen((DEFCHAR *)query_num1);
      sprintf((DEFCHAR *)query_num2,"%ld",col);
      item_values[3].value = query_num2;
      item_values[3].len = strlen((DEFCHAR *)query_num2);
   }
   else
      number_variables = 1;

   return number_variables;
}
/***********************************************************************/
short extract_prefix(short number_variables,short itemno,CHARTYPE *itemargs,CHARTYPE query_type,LINETYPE argc,CHARTYPE *arg,LINETYPE arglen)
/***********************************************************************/
{
   /*
    * Simply handle QUERY PREFIX here...
    */
   if ( strcmp( (DEFCHAR *)itemargs, "" ) == 0 )
   {
      if (CURRENT_VIEW->prefix == PREFIX_OFF)
      {
         item_values[1].value = (CHARTYPE *)"OFF";
         item_values[1].len = 3;
         item_values[2].value = (CHARTYPE *)"";  /* this set to empty deliberately */
         item_values[2].len = 0;
         return 1;
      }
      if ((CURRENT_VIEW->prefix&PREFIX_STATUS_MASK) == PREFIX_ON)
      {
         item_values[1].value = (CHARTYPE *)"ON";
         item_values[1].len = 2;
      }
      else
      {
         item_values[1].value = (CHARTYPE *)"NULLS";
         item_values[1].len = 5;
      }
      if ((CURRENT_VIEW->prefix&PREFIX_LOCATION_MASK) == PREFIX_LEFT)
      {
         item_values[2].value = (CHARTYPE *)"LEFT";
         item_values[2].len = 4;
      }
      else
      {
         item_values[2].value = (CHARTYPE *)"RIGHT";
         item_values[2].len = 5;
      }
      sprintf((DEFCHAR *)query_num1,"%d",CURRENT_VIEW->prefix_width);
      item_values[3].value = query_num1;
      item_values[3].len = strlen((DEFCHAR *)query_num1);
      sprintf((DEFCHAR *)query_num2,"%d",CURRENT_VIEW->prefix_gap);
      item_values[4].value = query_num2;
      item_values[4].len = strlen((DEFCHAR *)query_num2);
      number_variables = 4;
   }
   else
   {
      /*
       * ...but for QUERY PREFIX SYNONYM, its more complicated...
       */
      number_variables = extract_prefix_settings( itemno, itemargs, query_type );
   }
   return number_variables;
}
/***********************************************************************/
short extract_printer(short number_variables,short itemno,CHARTYPE *itemargs,CHARTYPE query_type,LINETYPE argc,CHARTYPE *arg,LINETYPE arglen)
/***********************************************************************/
{
   item_values[1].value = (CHARTYPE *)spooler_name;
   item_values[1].len = strlen((DEFCHAR *)spooler_name);
   return number_variables;
}
/***********************************************************************/
short extract_profile(short number_variables,short itemno,CHARTYPE *itemargs,CHARTYPE query_type,LINETYPE argc,CHARTYPE *arg,LINETYPE arglen)
/***********************************************************************/
{
   if ( local_prf == NULL )
   {
      item_values[1].value = (CHARTYPE *)"";
      item_values[1].len = 0;
   }
   else
   {
      item_values[1].value = (CHARTYPE *)local_prf;
      item_values[1].len = strlen((DEFCHAR *)local_prf);
   }
   return number_variables;
}
/***********************************************************************/
short extract_pscreen(short number_variables,short itemno,CHARTYPE *itemargs,CHARTYPE query_type,LINETYPE argc,CHARTYPE *arg,LINETYPE arglen)
/***********************************************************************/
{
   item_values[1].len = sprintf( (DEFCHAR *)query_num1,"%d",LINES );
   item_values[1].value = query_num1;
   item_values[2].len = sprintf( (DEFCHAR *)query_num2,"%d",COLS );
   item_values[2].value = query_num2;
   return number_variables;
}
/***********************************************************************/
short extract_reprofile(short number_variables,short itemno,CHARTYPE *itemargs,CHARTYPE query_type,LINETYPE argc,CHARTYPE *arg,LINETYPE arglen)
/***********************************************************************/
{
   return set_on_off_value(REPROFILEx,1);
}
/***********************************************************************/
short extract_readonly(short number_variables,short itemno,CHARTYPE *itemargs,CHARTYPE query_type,LINETYPE argc,CHARTYPE *arg,LINETYPE arglen)
/***********************************************************************/
{
   if ( READONLYx == READONLY_FORCE )
   {
      item_values[1].value = (CHARTYPE *)"FORCE";
      item_values[1].len = 5;
   }
   else if ( ISREADONLY(CURRENT_FILE) )
   {
      item_values[1].value = (CHARTYPE *)"ON";
      item_values[1].len = 2;
   }
   else
   {
      item_values[1].value = (CHARTYPE *)"OFF";
      item_values[1].len = 3;
   }
   return number_variables;
}
/***********************************************************************/
short extract_regexp(short number_variables,short itemno,CHARTYPE *itemargs,CHARTYPE query_type,LINETYPE argc,CHARTYPE *arg,LINETYPE arglen)
/***********************************************************************/
{
   int i;
   item_values[1].value = (CHARTYPE *)"";
   for ( i = 0; regexp_syntaxes[i].name != NULL; i++ )
   {
      if ( regexp_syntaxes[i].value == REGEXPx )
      {
         item_values[1].value = (CHARTYPE *)regexp_syntaxes[i].name;
         break;
      }
   }
   item_values[1].len = strlen((DEFCHAR *)item_values[1].value);
   return number_variables;
}
/***********************************************************************/
short extract_readv(short number_variables,short itemno,CHARTYPE *itemargs,CHARTYPE query_type,LINETYPE argc,CHARTYPE *arg,LINETYPE arglen)
/***********************************************************************/
{
   int key=0;
   bool mouse_key=FALSE;

   if (batch_only)
   {
      item_values[1].value = (CHARTYPE *)"0";
      item_values[1].len = 1;
      return 1;
   }
   while(1)
   {
#ifdef CAN_RESIZE
      if (is_termresized())
      {
         (void)THE_Resize(0,0);
         (void)THERefresh((CHARTYPE *)"");
      }
#endif
      key = curses_driver_read_window_key( CURRENT_WINDOW );
#ifdef CAN_RESIZE
      if (is_termresized())
         continue;
#endif
#if defined (PDCURSES_MOUSE_ENABLED) || defined(NCURSES_MOUSE_VERSION)
      if (key == KEY_MOUSE)
      {
#if 0
         int b,ba,bm,w;
         CHARTYPE scrn;
         if (get_mouse_info(&b,&ba,&bm) != RC_OK)
            continue;
         which_window_is_mouse_in(&scrn,&w);
         key = mouse_info_to_key(w,b,ba,bm);
#endif
         mouse_key = TRUE;
      }
      else
         mouse_key = FALSE;
#endif
      break;
   }
   if (current_key == -1)
      current_key = 0;
   else
   {
      if ( current_key == 7 )
         current_key = 0;
      else
         current_key++;
   }
   lastkeys[current_key] = key;
   set_key_values( key, mouse_key );
   return number_variables;
}
/***********************************************************************/
short extract_reserved(short number_variables,short itemno,CHARTYPE *itemargs,CHARTYPE query_type,LINETYPE argc,CHARTYPE *arg,LINETYPE arglen)
/***********************************************************************/
{
   bool line_numbers_only=TRUE;
   RESERVED *curr_rsrvd;
   CHARTYPE *tmpbuf=NULL,*attr_string=NULL;
   short rc=RC_OK;
   short y=0,x=0;

   if (query_type == QUERY_EXTRACT
   &&  strcmp((DEFCHAR *)itemargs,"*") == 0)
      line_numbers_only = FALSE;

   number_variables = 0;
   curr_rsrvd = CURRENT_FILE->first_reserved;
   strcpy( (DEFCHAR *)query_rsrvd, "" );
   while( curr_rsrvd != NULL )
   {
      if ( line_numbers_only )
      {
         y = strlen( (DEFCHAR *)curr_rsrvd->spec ) + 1;
         if ( (x + y) > sizeof( query_rsrvd ) )
            break;
         strcat( (DEFCHAR *)query_rsrvd, (DEFCHAR *)curr_rsrvd->spec );
         strcat( (DEFCHAR *)query_rsrvd, " ");
         x += y;
      }
      else
      {
         attr_string = get_colour_strings( curr_rsrvd->attr );
         if ( attr_string == (CHARTYPE *)NULL )
         {
            return(EXTRACT_ARG_ERROR);
         }
         tmpbuf = (CHARTYPE *)(*the_malloc)(sizeof(CHARTYPE)*(strlen((DEFCHAR *)attr_string)+strlen((DEFCHAR *)curr_rsrvd->line)+strlen((DEFCHAR *)curr_rsrvd->spec)+13));
         if ( tmpbuf == (CHARTYPE *)NULL )
         {
            display_error( 30, (CHARTYPE *)"", FALSE );
            return(EXTRACT_ARG_ERROR);
         }
         if ( curr_rsrvd->autoscroll )
         {
            strcpy( (DEFCHAR *)tmpbuf, "AUTOSCROLL " );
            strcat( (DEFCHAR *)tmpbuf, (DEFCHAR *)curr_rsrvd->spec );
         }
         else
         {
            strcpy( (DEFCHAR *)tmpbuf, (DEFCHAR *)curr_rsrvd->spec );
         }
         strcat( (DEFCHAR *)tmpbuf, " " );
         strcat( (DEFCHAR *)tmpbuf, (DEFCHAR *)attr_string );
         (*the_free)( attr_string );
         strcat( (DEFCHAR *)tmpbuf, (DEFCHAR *)curr_rsrvd->line );
         rc = set_rexx_variable( query_item[itemno].name, tmpbuf, strlen((DEFCHAR *)tmpbuf), ++number_variables );
         (*the_free)(tmpbuf);
         if ( rc == RC_SYSTEM_ERROR )
         {
            display_error( 54, (CHARTYPE *)"", FALSE );
            return(EXTRACT_ARG_ERROR);
         }
      }
      curr_rsrvd = curr_rsrvd->next;
   }
   if ( line_numbers_only )
   {
      if ( x == 0 )
         number_variables = 0;
      else
      {
         number_variables = 1;
         item_values[1].value = query_rsrvd;
         item_values[1].len = strlen( (DEFCHAR *)query_rsrvd );
      }
   }
   else
   {
      sprintf( (DEFCHAR *)query_rsrvd, "%d", number_variables );
      rc = set_rexx_variable( query_item[itemno].name, query_rsrvd, strlen( (DEFCHAR *)query_rsrvd ), 0 );
      if ( rc == RC_SYSTEM_ERROR )
      {
         display_error( 54, (CHARTYPE *)"", FALSE );
         number_variables = EXTRACT_ARG_ERROR;
      }
      else
         number_variables = EXTRACT_VARIABLES_SET;
   }
   return number_variables;
}
/***********************************************************************/
short extract_rexx(short number_variables,short itemno,CHARTYPE *itemargs,CHARTYPE query_type,LINETYPE argc,CHARTYPE *arg,LINETYPE arglen)
/***********************************************************************/
{
   item_values[1].value = get_rexx_interpreter_version(query_rsrvd);
   item_values[1].len = strlen((DEFCHAR *)query_rsrvd);
   return number_variables;
}
/***********************************************************************/
short extract_rexxhalt(short number_variables,short itemno,CHARTYPE *itemargs,CHARTYPE query_type,LINETYPE argc,CHARTYPE *arg,LINETYPE arglen)
/***********************************************************************/
{
   if ( COMMANDCALLSx == 0 )
   {
      item_values[1].value = (CHARTYPE *)"OFF";
      item_values[1].len = 3;
   }
   else
   {
      item_values[1].len = sprintf((DEFCHAR *)query_num1,"%d",COMMANDCALLSx);
      item_values[1].value = query_num1;
   }
   if ( FUNCTIONCALLSx == 0 )
   {
      item_values[2].value = (CHARTYPE *)"OFF";
      item_values[2].len = 3;
   }
   else
   {
      item_values[2].len = sprintf((DEFCHAR *)query_num1,"%d",FUNCTIONCALLSx);
      item_values[2].value = query_num1;
   }
   return number_variables;
}
/***********************************************************************/
short extract_rexxoutput(short number_variables,short itemno,CHARTYPE *itemargs,CHARTYPE query_type,LINETYPE argc,CHARTYPE *arg,LINETYPE arglen)
/***********************************************************************/
{
   if (CAPREXXOUTx)
   {
      item_values[1].value = (CHARTYPE *)"FILE";
      item_values[1].len = 4;
   }
   else
   {
      item_values[1].value = (CHARTYPE *)"DISPLAY";
      item_values[1].len = 7;
   }
   sprintf((DEFCHAR *)query_num1,"%ld",CAPREXXMAXx);
   item_values[2].value = query_num1;
   item_values[2].len = strlen((DEFCHAR *)query_num1);
   return number_variables;
}
/***********************************************************************/
short extract_rightedge_function(short number_variables,short itemno,CHARTYPE *itemargs,CHARTYPE query_type,LINETYPE argc,CHARTYPE *arg,LINETYPE arglen)
/***********************************************************************/
{
   LENGTHTYPE screen_col;

   if (batch_only)
   {
      item_values[1].value = (CHARTYPE *)"0";
      item_values[1].len = 1;
      return number_variables;
   }
   screen_col = query2_filearea_cursor_cell() - (CURRENT_VIEW->verify_col - 1);
   return set_boolean_value((bool)(CURRENT_VIEW->current_window == WINDOW_FILEAREA
                                && screen_col == CURRENT_SCREEN.cols[WINDOW_FILEAREA]-1),
                            (short)1);
}
/***********************************************************************/
short extract_ring(short number_variables,short itemno,CHARTYPE *itemargs,CHARTYPE query_type,LINETYPE argc,CHARTYPE *arg,LINETYPE arglen)
/***********************************************************************/
{
   VIEW_DETAILS *curr;
   FILE_DETAILS *first_view_file=NULL;
   bool process_view=FALSE;
   LINETYPE lineno=0L;
   LENGTHTYPE col=0;
   register int i=0,j=0;
   int offset=0,off=0;
   bool view_being_displayed=FALSE;
   ROWTYPE save_msgline_rows = CURRENT_VIEW->msgline_rows;
   bool save_msgmode_status = CURRENT_VIEW->msgmode_status;

   if (compatible_feel == COMPAT_XEDIT)
      offset = 1;

   if (query_type == QUERY_QUERY)
   {
      CURRENT_VIEW->msgline_rows   = min(terminal_lines-1,number_of_files);
      CURRENT_VIEW->msgmode_status = TRUE;
   }
   else
      number_variables = offset;

   curr = vd_current;
   for (j=0;j<number_of_files;)
   {
      process_view = TRUE;
      if (curr->file_for_view->file_views > 1)
      {
         if (first_view_file == curr->file_for_view)
            process_view = FALSE;
         else
            first_view_file = curr->file_for_view;
      }
      if (process_view)
      {
         j++;
         view_being_displayed=FALSE;
         for (i=0;i<display_screens;i++)
         {
            if (SCREEN_VIEW(i) == curr)
            {
               view_being_displayed = TRUE;
               get_current_position((CHARTYPE)i,&lineno,&col);
            }
         }
         if (!view_being_displayed)
         {
            lineno = (curr->current_window==WINDOW_COMMAND)?curr->current_line:curr->focus_line;
            col = curr->current_column;
         }
         if (compatible_look == COMPAT_XEDIT)
            sprintf((DEFCHAR *)query_rsrvd,"%s%s Size=%ld Line=%ld Col=%ld Alt=%d,%d",
                  curr->file_for_view->fpath,
                  curr->file_for_view->fname,
                  curr->file_for_view->number_lines,
                  lineno,col,
                  curr->file_for_view->autosave_alt,
                  curr->file_for_view->save_alt);
         else
            sprintf((DEFCHAR *)query_rsrvd,"%s%s Line=%ld Col=%ld Size=%ld Alt=%d,%d",
                  curr->file_for_view->fpath,
                  curr->file_for_view->fname,
                  lineno,col,
                  curr->file_for_view->number_lines,
                  curr->file_for_view->autosave_alt,
                  curr->file_for_view->save_alt);
         if (query_type == QUERY_QUERY)
         {
            display_error(0,query_rsrvd,TRUE);
         }
         else
         {
            number_variables++;
            item_values[number_variables].len = strlen((DEFCHAR *)query_rsrvd);
            memcpy((DEFCHAR*)trec+off,(DEFCHAR*)query_rsrvd,(item_values[number_variables].len)+1);
            item_values[number_variables].value = trec+off;
            off += (item_values[number_variables].len)+1;
         }
      }
      curr = curr->next;
      if (curr == NULL)
         curr = vd_first;
   }

   if (query_type == QUERY_QUERY)
   {
      CURRENT_VIEW->msgline_rows   = save_msgline_rows;
      CURRENT_VIEW->msgmode_status = save_msgmode_status;
      number_variables = EXTRACT_VARIABLES_SET;
   }
   else
   {
      if ( offset )
      {
         sprintf( (DEFCHAR *)query_num1, "%d", number_variables - 1 );
         item_values[1].value = query_num1;
         item_values[1].len = strlen( (DEFCHAR *)query_num1 );
      }
   }

   return number_variables;
}
/***********************************************************************/
short extract_scale(short number_variables,short itemno,CHARTYPE *itemargs,CHARTYPE query_type,LINETYPE argc,CHARTYPE *arg,LINETYPE arglen)
/***********************************************************************/
{
   set_on_off_value(CURRENT_VIEW->scale_on,1);
   if (CURRENT_VIEW->scale_base == POSITION_MIDDLE)
      sprintf((DEFCHAR *)query_rsrvd,"M%+d",CURRENT_VIEW->scale_off);
   else
      sprintf((DEFCHAR *)query_rsrvd,"%d",CURRENT_VIEW->scale_off);
   item_values[2].value = query_rsrvd;
   item_values[2].len = strlen((DEFCHAR *)query_rsrvd);
   return number_variables;
}
/***********************************************************************/
short extract_scope(short number_variables,short itemno,CHARTYPE *itemargs,CHARTYPE query_type,LINETYPE argc,CHARTYPE *arg,LINETYPE arglen)
/***********************************************************************/
{
   if (CURRENT_VIEW->scope_all)
   {
      item_values[1].value = (CHARTYPE *)"ALL";
      item_values[1].len = 3;
   }
   else
   {
      item_values[1].value = (CHARTYPE *)"DISPLAY";
      item_values[1].len = 7;
   }
   return number_variables;
}
/***********************************************************************/
short extract_screen(short number_variables,short itemno,CHARTYPE *itemargs,CHARTYPE query_type,LINETYPE argc,CHARTYPE *arg,LINETYPE arglen)
/***********************************************************************/
{
   sprintf((DEFCHAR *)query_num1,"%d",display_screens);
   item_values[1].value = query_num1;
   item_values[1].len = strlen((DEFCHAR *)query_num1);
   if (horizontal)
   {
      item_values[2].value = (CHARTYPE *)"HORIZONTAL";
      item_values[2].len = 10;
   }
   else
   {
      item_values[2].value = (CHARTYPE *)"VERTICAL";
      item_values[2].len = 8;
   }
   return number_variables;
}
/***********************************************************************/
short extract_select(short number_variables,short itemno,CHARTYPE *itemargs,CHARTYPE query_type,LINETYPE argc,CHARTYPE *arg,LINETYPE arglen)
/***********************************************************************/
{
   LINE *curr=NULL;
   unsigned short x=0;

   curr = lll_find(CURRENT_FILE->first_line,CURRENT_FILE->last_line,
               (compatible_feel==COMPAT_XEDIT)?CURRENT_VIEW->current_line:get_true_line(TRUE),
               CURRENT_FILE->number_lines);
   sprintf((DEFCHAR *)query_num1,"%d",curr->select);
   item_values[1].value = query_num1;
   item_values[1].len = strlen((DEFCHAR *)query_num1);
   x = 0;
   curr = lll_find(CURRENT_FILE->first_line,CURRENT_FILE->last_line,1L,CURRENT_FILE->number_lines);
   while(curr->next != NULL)
   {
      if (curr->select > x)
         x = curr->select;
      curr = curr->next;
   }
   item_values[2].len = sprintf((DEFCHAR *)query_num2,"%d",x);
   item_values[2].value = query_num2;
   item_values[3].len = sprintf((DEFCHAR *)query_num3,"%d",MAX_SELECT_LEVEL);
   item_values[3].value = query_num3;
   return number_variables;
}
/***********************************************************************/
short extract_shadow(short number_variables,short itemno,CHARTYPE *itemargs,CHARTYPE query_type,LINETYPE argc,CHARTYPE *arg,LINETYPE arglen)
/***********************************************************************/
{
   return set_on_off_value(CURRENT_VIEW->shadow,1);
}
/***********************************************************************/
short extract_shadow_function(short number_variables,short itemno,CHARTYPE *itemargs,CHARTYPE query_type,LINETYPE argc,CHARTYPE *arg,LINETYPE arglen)
/***********************************************************************/
{
   bool bool_flag=FALSE;

   switch(CURRENT_VIEW->current_window)
   {
      case WINDOW_FILEAREA:
         if (batch_only)
         {
            bool_flag = FALSE;
            break;
         }
         if (query2_filearea_cursor_is_shadow())
            bool_flag = TRUE;
         else
            bool_flag = FALSE;
         break;
      default:
         bool_flag = FALSE;
         break;
   }
   return set_boolean_value((bool)bool_flag,(short)1);
}
/***********************************************************************/
short extract_shift_function(short number_variables,short itemno,CHARTYPE *itemargs,CHARTYPE query_type,LINETYPE argc,CHARTYPE *arg,LINETYPE arglen)
/***********************************************************************/
{
   int shift=0;

   get_key_name(lastkeys[current_key],&shift);
   return set_boolean_value((bool)(shift & SHIFT_SHIFT),(short)1);
}
/***********************************************************************/
short extract_showkey(short number_variables,short itemno,CHARTYPE *itemargs,CHARTYPE query_type,LINETYPE argc,CHARTYPE *arg,LINETYPE arglen)
/***********************************************************************/
{
   int key=0;

   if (strcmp((DEFCHAR *)itemargs,"") == 0
   || ((key = find_key_name(itemargs)) == -1))
   {
      item_values[1].value = (CHARTYPE *)"INVALID KEY";
      item_values[1].len = strlen((DEFCHAR *)item_values[1].value);
   }
   else
   {
      function_key(key,OPTION_EXTRACT,FALSE);
      number_variables = EXTRACT_VARIABLES_SET;
   }
   return number_variables;
}
/***********************************************************************/
short extract_size(short number_variables,short itemno,CHARTYPE *itemargs,CHARTYPE query_type,LINETYPE argc,CHARTYPE *arg,LINETYPE arglen)
/***********************************************************************/
{
   sprintf((DEFCHAR *)query_num1,"%ld",CURRENT_FILE->number_lines);
   item_values[1].value = query_num1;
   item_values[1].len = strlen((DEFCHAR *)query_num1);
   return number_variables;
}
/***********************************************************************/
short extract_slk( short number_variables, short itemno, CHARTYPE *itemargs, CHARTYPE query_type, LINETYPE argc, CHARTYPE *arg, LINETYPE arglen )
/***********************************************************************/
{
   int off=0;
   int item;

   TRACE_FUNCTION("query.c:   extract_slk");
#if defined(HAVE_SLK_INIT)
   if ( blank_field( itemargs ) )
   {
      if ( max_slk_labels == 0
      &&   query_type != QUERY_MODIFY )
      {
         item_values[1].value = (CHARTYPE *)"UNAVAILABLE";
         item_values[1].len = 11;
         number_variables = 2;
      }
      else if ( SLKx )
      {
         item_values[1].value = (CHARTYPE *)"ON";
         item_values[1].len = 2;
         number_variables = 2;
      }
      else
      {
         item_values[1].value = (CHARTYPE *)"OFF";
         item_values[1].len = 3;
         number_variables = 2;
      }
      if ( query_type == QUERY_MODIFY
      ||   query_type == QUERY_STATUS )
         number_variables = 1;
      else
      {
         item_values[2].len = sprintf( (DEFCHAR *)query_num1,"%d", slk_format_switch );
         item_values[2].value = query_num1;
      }
      TRACE_RETURN();
      return( number_variables );
   }
   else if ( max_slk_labels == 0 )
   {
      display_error( 82, (CHARTYPE*)"- use -k command line switch to enable", FALSE );
      number_variables = EXTRACT_ARG_ERROR;
   }
   else if ( strcmp( (DEFCHAR*)itemargs, "*" ) == 0 )
   {
      if (query_type == QUERY_QUERY)
      {
         display_error( 1, (CHARTYPE *)itemargs, FALSE );
         return EXTRACT_ARG_ERROR;
      }
      for ( number_variables = 0; number_variables < max_slk_labels;  )
      {
         number_variables++;
         strcpy( (DEFCHAR *)query_rsrvd, slk_label( number_variables ) );
         item_values[number_variables].len = strlen((DEFCHAR *)query_rsrvd);
         memcpy((DEFCHAR*)trec+off,(DEFCHAR*)query_rsrvd,(item_values[number_variables].len)+1);
         item_values[number_variables].value = trec+off;
         off += (item_values[number_variables].len)+1;
      }
   }
   else
   {
      if ( valid_positive_integer( itemargs ) )
      {
         /*
          * Try and find a matching slk
          */
         item = atoi( (DEFCHAR *)itemargs );
         if ( item < 1 )
         {
            display_error( 5, (CHARTYPE *)itemargs, FALSE );
            return EXTRACT_ARG_ERROR;
         }
         if ( item > max_slk_labels )
         {
            display_error( 6, (CHARTYPE *)itemargs, FALSE );
            return EXTRACT_ARG_ERROR;
         }

         if (query_type == QUERY_QUERY)
         {
            strcpy( (DEFCHAR *)query_rsrvd, slk_label( item ) );
            sprintf( (DEFCHAR *)trec, "slk %s", query_rsrvd );
            display_error( 0, trec, TRUE );
            number_variables = EXTRACT_VARIABLES_SET;
         }
         else
         {
            /*
             * EXTRACT
             */
            strcpy( (DEFCHAR *)query_rsrvd, slk_label( item ) );
            item_values[1].value = query_rsrvd;
            item_values[1].len = strlen( (DEFCHAR *)query_rsrvd );
            number_variables = 1;
         }
      }
      else
      {
         display_error( 4, (CHARTYPE *)itemargs, FALSE );
         number_variables = EXTRACT_ARG_ERROR;
      }
   }
#else
   display_error(82,(CHARTYPE*)"SLK",FALSE);
   number_variables = EXTRACT_ARG_ERROR;
#endif

   TRACE_RETURN();
   return(number_variables);
}
/***********************************************************************/
short extract_spacechar_function(short number_variables,short itemno,CHARTYPE *itemargs,CHARTYPE query_type,LINETYPE argc,CHARTYPE *arg,LINETYPE arglen)
/***********************************************************************/
{
   CHARTYPE cursor_char=0;

   if (batch_only)
   {
      item_values[1].value = (CHARTYPE *)"0";
      item_values[1].len = 1;
      return 1;
   }
#ifdef VMS
   cursor_char = (CHARTYPE)( winch( CURRENT_WINDOW ) );
#else
   cursor_char = (CHARTYPE)( winch( CURRENT_WINDOW ) & A_CHARTEXT );
#endif
   return set_boolean_value((bool)(cursor_char == ' '),(short)1);
}
/***********************************************************************/
short extract_statopt(short number_variables,short itemno,CHARTYPE *itemargs,CHARTYPE query_type,LINETYPE argc,CHARTYPE *arg,LINETYPE arglen)
/***********************************************************************/
{
   short rc=RC_OK;
   register int i=0;
   int off=0;
   ROWTYPE save_msgline_rows = CURRENT_VIEW->msgline_rows;
   bool save_msgmode_status = CURRENT_VIEW->msgmode_status;
   LINE *curr;

   if ( itemargs == NULL
   ||   blank_field( itemargs )
   ||   strcmp( (DEFCHAR*)itemargs, "*" ) == 0 )
   {
      if ( query_type == QUERY_QUERY )
      {
         for ( i = 0, curr = first_option; curr != NULL; curr = curr->next, i++ );
         CURRENT_VIEW->msgline_rows   = min( terminal_lines - 1, i );
         CURRENT_VIEW->msgmode_status = TRUE;
      }
      else
         number_variables = 0;
      for ( curr = first_option; curr != NULL; curr = curr->next )
      {
         sprintf( (DEFCHAR *)query_rsrvd, "%sON %s %d %d %s",
                  (query_type == QUERY_QUERY) ? (DEFCHAR *)"statopt " : "",
                  curr->name,
                  curr->select+1+STATAREA_OFFSET,
                  curr->save_select,
                  (DEFCHAR *)((curr->line!=NULL) ? (DEFCHAR *)curr->line : "") );

         if ( query_type == QUERY_QUERY )
            display_error( 0, query_rsrvd, TRUE );
         else
         {
            number_variables++;
            item_values[number_variables].len = strlen( (DEFCHAR *)query_rsrvd );
            memcpy( (DEFCHAR*)trec + off, (DEFCHAR*)query_rsrvd, (item_values[number_variables].len) + 1 );
            item_values[number_variables].value = trec + off;
            off += (item_values[number_variables].len) + 1;
         }
      }
   }
   else
   {
      if ( query_type == QUERY_QUERY )
      {
         CURRENT_VIEW->msgline_rows   = 1;
         CURRENT_VIEW->msgmode_status = TRUE;
      }
      /*
       * Find a match for the supplied option
       */
      curr = lll_locate( first_option, make_upper( itemargs ) );
      if ( curr )
      {
         /*
          * We found it
          */
         sprintf( (DEFCHAR *)query_rsrvd, "%sON %s %d %d %s",
                  (query_type == QUERY_QUERY) ? (DEFCHAR *)"statopt " : "",
                  curr->name,
                  curr->select,
                  curr->save_select,
                  (DEFCHAR *)((curr->line!=NULL) ? (DEFCHAR *)curr->line : "") );
      }
      else
      {
         /*
          * We didn't find it
          */
         sprintf( (DEFCHAR *)query_rsrvd, "%sOFF %s",
                  (query_type == QUERY_QUERY) ? (DEFCHAR *)"statopt " : "",
                  itemargs );
      }
      if ( query_type == QUERY_QUERY )
         display_error( 0, query_rsrvd, TRUE );
      else
      {
         item_values[1].value = query_rsrvd;
         item_values[1].len = strlen((DEFCHAR *)query_rsrvd);
         number_variables = 1;
      }
   }

   if ( query_type == QUERY_QUERY )
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
short extract_statusline(short number_variables,short itemno,CHARTYPE *itemargs,CHARTYPE query_type,LINETYPE argc,CHARTYPE *arg,LINETYPE arglen)
/***********************************************************************/
{
   switch(STATUSLINEx)
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
         break;
      case 'G':
         item_values[1].value = (CHARTYPE *)"GUI";
         item_values[1].len = 3;
         break;
      }
   return number_variables;
}
/***********************************************************************/
short extract_stay(short number_variables,short itemno,CHARTYPE *itemargs,CHARTYPE query_type,LINETYPE argc,CHARTYPE *arg,LINETYPE arglen)
/***********************************************************************/
{
   return set_on_off_value(CURRENT_VIEW->stay,1);
}
/***********************************************************************/
short extract_synelem( short number_variables, short itemno, CHARTYPE *itemargs, CHARTYPE query_type, LINETYPE argc, CHARTYPE *arg, LINETYPE arglen )
/***********************************************************************/
{
   short y=0,x=0;
   CHARTYPE syntax_element;
#define SYN_PARAMS  4
   CHARTYPE *word[PEN_PARAMS+1];
   CHARTYPE strip[PEN_PARAMS];
   unsigned short num_params=0;
   LENGTHTYPE row, col;

   TRACE_FUNCTION("query.c:   extract_synelem");
   strip[0]=STRIP_BOTH;
   strip[1]=STRIP_BOTH;
   strip[2]=STRIP_BOTH;
   strip[3]=STRIP_BOTH;
   num_params = param_split( itemargs, word, SYN_PARAMS, WORD_DELIMS, TEMP_PARAM, strip, FALSE );
   if ( num_params == 0
   ||   equal( (CHARTYPE *)"cursor", word[0], 1 ) )
   {
      /*
       * No parameters, cursor
       */
      /*
       * Are we in filearea?
       */
      if ( CURRENT_VIEW->current_window != WINDOW_FILEAREA
      ||   !curses_started )
      {
         item_values[1].value = (CHARTYPE *)"UNKNOWN";
         item_values[1].len = 4;
         TRACE_RETURN();
         return(number_variables);
      }
      /*
       * Determine position of cursor relative to ESCREEN
       * This should result in a direct entry into the highlight_type array
       */
      if (!query2_filearea_cursor_screen_position(&y, &x))
      {
         item_values[1].value = (CHARTYPE *)"UNKNOWN";
         item_values[1].len = 4;
         TRACE_RETURN();
         return(number_variables);
      }
      syntax_element = get_syntax_element( current_screen, y, x );
   }
   else if ( equal( (CHARTYPE *)"file", word[0], 1 ) )
   {
      if ( num_params > 3 )
      {
         display_error( 2, (CHARTYPE *)"", FALSE );
         return EXTRACT_ARG_ERROR;
      }
      else if ( num_params < 3 )
      {
         display_error( 3, (CHARTYPE *)"", FALSE );
         return EXTRACT_ARG_ERROR;
      }
      else
      {
         /*
          * 3 args; file and two numbers
          */
         if ( !valid_positive_integer( word[1] ) )
         {
            display_error( 1, word[1], FALSE );
            return EXTRACT_ARG_ERROR;
         }
         if ( !valid_positive_integer( word[2] ) )
         {
            display_error( 1, word[2], FALSE );
            return EXTRACT_ARG_ERROR;
         }
         /*
          * Now have two numbers, do they specify a file position
          * that is currently viewable
          */
         row = atol( (DEFCHAR *)word[1] );
         col = atol( (DEFCHAR *)word[2] );
         if ( !line_in_view( current_screen, row ) )
         {
            item_values[1].value = (CHARTYPE *)"UNKNOWN";
            item_values[1].len = 4;
            TRACE_RETURN();
            return(number_variables);
         }
         /*
          * If column is not in display, error.
          */
         if ( !column_in_view( current_screen, col - 1 ) )
         {
            item_values[1].value = (CHARTYPE *)"UNKNOWN";
            item_values[1].len = 4;
            TRACE_RETURN();
            return(number_variables);
         }
         x = (LINETYPE)col - (LINETYPE)CURRENT_VIEW->verify_col;
         y = get_row_for_focus_line( current_screen, row, CURRENT_VIEW->current_row );
         syntax_element = get_syntax_element( current_screen, y, x );
      }
   }
   else
   {
      /*
       * Assume it is two numbers
       */
      if ( num_params > 2 )
      {
         display_error( 2, (CHARTYPE *)"", FALSE );
         return EXTRACT_ARG_ERROR;
      }
      else if ( num_params < 2 )
      {
         display_error( 3, (CHARTYPE *)"", FALSE );
         return EXTRACT_ARG_ERROR;
      }
      else
      {
         /*
          * 2 args; and two numbers
          */
         if ( !valid_positive_integer( word[0] ) )
         {
            display_error( 1, word[0], FALSE );
            return EXTRACT_ARG_ERROR;
         }
         if ( !valid_positive_integer( word[1] ) )
         {
            display_error( 1, word[1], FALSE );
            return EXTRACT_ARG_ERROR;
         }
         /*
          * Now have two numbers, do they specify a position
          * that is currently viewable
          */
         y = atoi( (DEFCHAR *)word[0] ) - 1;
         x = atoi( (DEFCHAR *)word[1] ) - 1;
         syntax_element = get_syntax_element( current_screen, y, x );
       }
   }

   switch ( syntax_element )
   {
      case ECOLOUR_NONE:
         item_values[1].value = (CHARTYPE *)"NONE";
         item_values[1].len = 4;
         break;
      case ECOLOUR_COMMENTS:
         item_values[1].value = (CHARTYPE *)"COMMENT";
         item_values[1].len = 7;
         break;
      case ECOLOUR_FUNCTIONS:
         item_values[1].value = (CHARTYPE *)"FUNCTION";
         item_values[1].len = 8;
         break;
      case ECOLOUR_HEADER:
         item_values[1].value = (CHARTYPE *)"HEADER";
         item_values[1].len = 6;
         break;
      case ECOLOUR_INC_STRING:
         item_values[1].value = (CHARTYPE *)"INCOMPLETESTRING";
         item_values[1].len = 16;
         break;
      case ECOLOUR_KEYWORDS:
         item_values[1].value = (CHARTYPE *)"KEYWORD";
         item_values[1].len = 7;
         break;
      case ECOLOUR_LABEL:
         item_values[1].value = (CHARTYPE *)"LABEL";
         item_values[1].len = 5;
         break;
      case ECOLOUR_HTML_TAG:
         item_values[1].value = (CHARTYPE *)"MARKUP";
         item_values[1].len = 6;
         break;
      case ECOLOUR_MATCH:
         item_values[1].value = (CHARTYPE *)"MATCH";
         item_values[1].len = 5;
         break;
      case ECOLOUR_OPERATOR:
         item_values[1].value = (CHARTYPE *)"OPERATOR";
         item_values[1].len = 8;
         break;
      case ECOLOUR_PAREN:
         item_values[1].value = (CHARTYPE *)"PAREN";
         item_values[1].len = 5;
         break;
      case ECOLOUR_TYPES:
         item_values[1].value = (CHARTYPE *)"TYPE";
         item_values[1].len = 4;
         break;
      case ECOLOUR_CONSTANTS:
         item_values[1].value = (CHARTYPE *)"CONSTANT";
         item_values[1].len = 8;
         break;
      case ECOLOUR_PUNCTUATION:
         item_values[1].value = (CHARTYPE *)"PUNCTUATION";
         item_values[1].len = 11;
         break;
      case ECOLOUR_NUMBERS:
         item_values[1].value = (CHARTYPE *)"NUMBER";
         item_values[1].len = 6;
         break;
      case ECOLOUR_PREDIR:
         item_values[1].value = (CHARTYPE *)"PREPROCESSOR";
         item_values[1].len = 12;
         break;
      case ECOLOUR_STRINGS:
         item_values[1].value = (CHARTYPE *)"STRING";
         item_values[1].len = 6;
         break;
      default:
         item_values[1].value = (CHARTYPE *)"UNKNOWN";
         item_values[1].len = 7;
         break;
   }


   TRACE_RETURN();
   return(number_variables);
}
/***********************************************************************/
short extract_synonym( short number_variables, short itemno, CHARTYPE *itemargs, CHARTYPE query_type, LINETYPE argc, CHARTYPE *arg, LINETYPE arglen )
/***********************************************************************/
{
   int off=0;
   CHARTYPE *ptr=NULL;
   DEFINE *curr;

   TRACE_FUNCTION("query.c:   extract_synonym");
   if ( blank_field( itemargs ) )
   {
      if ( CURRENT_VIEW->synonym )
      {
         item_values[1].value = (CHARTYPE *)"ON";
         item_values[1].len = 2;
         number_variables = 1;
      }
      else
      {
         item_values[1].value = (CHARTYPE *)"OFF";
         item_values[1].len = 3;
         number_variables = 1;
      }
      TRACE_RETURN();
      return( number_variables );
   }
   else if ( strcmp( (DEFCHAR*)itemargs, "*" ) == 0 )
   {
      if (query_type == QUERY_QUERY)
      {
         display_error( 1, (CHARTYPE *)itemargs, FALSE );
         return EXTRACT_ARG_ERROR;
      }
      for ( number_variables = 0,curr = first_synonym; curr != NULL; curr = curr->next )
      {
         strcpy( (DEFCHAR *)query_rsrvd, "" );
         ptr = build_synonym_definition( curr, curr->synonym, query_rsrvd, TRUE );

         number_variables++;
         item_values[number_variables].len = strlen((DEFCHAR *)query_rsrvd);
         memcpy((DEFCHAR*)trec+off,(DEFCHAR*)query_rsrvd,(item_values[number_variables].len)+1);
         item_values[number_variables].value = trec+off;
         off += (item_values[number_variables].len)+1;
      }
   }
   else
   {
      /*
       * Try and find a matching synonym
       */
      for ( curr = first_synonym; curr != NULL; curr = curr->next )
      {
         if ( equal( curr->synonym, itemargs, curr->def_funkey ) )
         {
            break;
         }
      }

      if (query_type == QUERY_QUERY)
      {
         strcpy( (DEFCHAR *)query_rsrvd, "" );
         ptr = build_synonym_definition( curr, itemargs, query_rsrvd, TRUE );
         sprintf((DEFCHAR *)trec,"synonym %s", ptr );
         display_error(0,trec,TRUE);
         number_variables = EXTRACT_VARIABLES_SET;
      }
      else
      {
         /*
          * EXTRACT
          */
         if ( curr )
         {
            strcpy( (DEFCHAR *)query_rsrvd, "" );
            ptr = build_synonym_definition( curr, itemargs, query_rsrvd, FALSE );
            item_values[1].value = curr->synonym;
            item_values[1].len = strlen( (DEFCHAR *)curr->synonym );
            item_values[2].len = sprintf( (DEFCHAR *)query_num1,"%d", curr->def_funkey );
            item_values[2].value = query_num1;
            item_values[3].value = ptr;
            item_values[3].len = strlen( (DEFCHAR *)ptr );
            item_values[4].len = sprintf( (DEFCHAR *)query_num2,"%c", curr->linend );
            item_values[4].value = query_num2;
         }
         else
         {
            item_values[1].value = item_values[3].value = itemargs;
            item_values[1].len = item_values[3].len = strlen( (DEFCHAR *)itemargs );
            item_values[2].len = sprintf( (DEFCHAR *)query_num1,"%ld", item_values[1].len );
            item_values[2].value = query_num1;
            item_values[4].value = (CHARTYPE *)"";
            item_values[4].len = 0;
         }
         number_variables = 4;
      }
   }

   TRACE_RETURN();
   return(number_variables);
}
/***********************************************************************/
short extract_tabkey(short number_variables,short itemno,CHARTYPE *itemargs,CHARTYPE query_type,LINETYPE argc,CHARTYPE *arg,LINETYPE arglen)
/***********************************************************************/
{
   if (tabkey_overwrite == 'T')
   {
      item_values[1].value = (CHARTYPE *)"TAB";
      item_values[1].len = 3;
   }
   else
   {
      item_values[1].value = (CHARTYPE *)"CHARACTER";
      item_values[1].len = 9;
   }
   if (tabkey_insert == 'T')
   {
      item_values[2].value = (CHARTYPE *)"TAB";
      item_values[2].len = 3;
   }
   else
   {
      item_values[2].value = (CHARTYPE *)"CHARACTER";
      item_values[2].len = 9;
   }
   return number_variables;
}
/***********************************************************************/
short extract_tabline(short number_variables,short itemno,CHARTYPE *itemargs,CHARTYPE query_type,LINETYPE argc,CHARTYPE *arg,LINETYPE arglen)
/***********************************************************************/
{
   set_on_off_value(CURRENT_VIEW->tab_on,1);
   if (CURRENT_VIEW->tab_base == POSITION_MIDDLE)
      sprintf((DEFCHAR *)query_rsrvd,"M%+d",CURRENT_VIEW->tab_off);
   else
      sprintf((DEFCHAR *)query_rsrvd,"%d",CURRENT_VIEW->tab_off);
   item_values[2].value = query_rsrvd;
   item_values[2].len = strlen((DEFCHAR *)query_rsrvd);
   return number_variables;
}
/***********************************************************************/
short extract_tabs(short number_variables,short itemno,CHARTYPE *itemargs,CHARTYPE query_type,LINETYPE argc,CHARTYPE *arg,LINETYPE arglen)
/***********************************************************************/
{
   register int i=0;

   strcpy((DEFCHAR *)query_rsrvd,"");
   if (CURRENT_VIEW->tabsinc)
   {
      sprintf((DEFCHAR *)query_rsrvd,"INCR %d",CURRENT_VIEW->tabsinc);
   }
   else
   {
      for (i=0;i<CURRENT_VIEW->numtabs;i++)
      {
          sprintf((DEFCHAR *)query_num1,"%ld ",CURRENT_VIEW->tabs[i]);
          strcat((DEFCHAR *)query_rsrvd,(DEFCHAR *)query_num1);
      }
      if (query_type == QUERY_QUERY
      ||  query_type == QUERY_STATUS)
         query_rsrvd[COLS-7] = '\0';
   }
   item_values[1].value = query_rsrvd;
   item_values[1].len = strlen((DEFCHAR *)query_rsrvd);
   return number_variables;
}
/***********************************************************************/
short extract_tabsin(short number_variables,short itemno,CHARTYPE *itemargs,CHARTYPE query_type,LINETYPE argc,CHARTYPE *arg,LINETYPE arglen)
/***********************************************************************/
{
   set_on_off_value(TABI_ONx,1);
   sprintf((DEFCHAR *)query_num1,"%d",TABI_Nx);
   item_values[2].value = query_num1;
   item_values[2].len = strlen((DEFCHAR *)query_num1);
   return number_variables;
}
/***********************************************************************/
short extract_tabsout(short number_variables,short itemno,CHARTYPE *itemargs,CHARTYPE query_type,LINETYPE argc,CHARTYPE *arg,LINETYPE arglen)
/***********************************************************************/
{
   set_on_off_value(CURRENT_FILE->tabsout_on,1);
   sprintf((DEFCHAR *)query_num1,"%d",CURRENT_FILE->tabsout_num);
   item_values[2].value = query_num1;
   item_values[2].len = strlen((DEFCHAR *)query_num1);
   return number_variables;
}
/***********************************************************************/
short extract_targetsave(short number_variables,short itemno,CHARTYPE *itemargs,CHARTYPE query_type,LINETYPE argc,CHARTYPE *arg,LINETYPE arglen)
/***********************************************************************/
{
   int len=0;
   switch( TARGETSAVEx )
   {
      case TARGET_ALL:
         item_values[1].value = (CHARTYPE *)"ALL";
         item_values[1].len = 3;
         break;
      case TARGET_UNFOUND:
         item_values[1].value = (CHARTYPE *)"NONE";
         item_values[1].len = 4;
         break;
      default:
         strcpy( (DEFCHAR *)query_rsrvd, "" );
         if ( TARGETSAVEx & TARGET_STRING )
            strcat( (DEFCHAR *)query_rsrvd, "STRING " );
         if ( TARGETSAVEx & TARGET_REGEXP )
            strcat( (DEFCHAR *)query_rsrvd, "REGEXP " );
         if ( TARGETSAVEx & TARGET_ABSOLUTE )
            strcat( (DEFCHAR *)query_rsrvd, "ABSOLUTE " );
         if ( TARGETSAVEx & TARGET_RELATIVE )
            strcat( (DEFCHAR *)query_rsrvd, "RELATIVE " );
         if ( TARGETSAVEx & TARGET_POINT )
            strcat( (DEFCHAR *)query_rsrvd, "POINT " );
         if ( TARGETSAVEx & TARGET_BLANK )
            strcat( (DEFCHAR *)query_rsrvd, "BLANK " );
         len = strlen( (DEFCHAR *)query_rsrvd );
         if ( query_rsrvd[len-1] == ' ' )
         {
            query_rsrvd[len-1] = '\0';
            len--;
         }
         item_values[1].value = query_rsrvd;
         item_values[1].len = len;
         break;
   }
   return number_variables;
}

/***********************************************************************/
short extract_terminal(short number_variables,short itemno,CHARTYPE *itemargs,CHARTYPE query_type,LINETYPE argc,CHARTYPE *arg,LINETYPE arglen)
/***********************************************************************/
{
   item_values[1].value = term_name;
   item_values[1].len = strlen((DEFCHAR *)term_name);
   item_values[2].len = sprintf((DEFCHAR *)query_num1, "%d", LINES );
   item_values[2].value = query_num1;
   item_values[3].len = sprintf((DEFCHAR *)query_num2, "%d", COLS );
   item_values[3].value = query_num2;
   return number_variables;
}

/***********************************************************************/
short extract_thighlight(short number_variables,short itemno,CHARTYPE *itemargs,CHARTYPE query_type,LINETYPE argc,CHARTYPE *arg,LINETYPE arglen)
/***********************************************************************/
{
   return set_on_off_value(CURRENT_VIEW->thighlight_on,1);
}
/***********************************************************************/
short extract_timecheck(short number_variables,short itemno,CHARTYPE *itemargs,CHARTYPE query_type,LINETYPE argc,CHARTYPE *arg,LINETYPE arglen)
/***********************************************************************/
{
   return set_on_off_value(CURRENT_FILE->timecheck,1);
}
/***********************************************************************/
short extract_tof(short number_variables,short itemno,CHARTYPE *itemargs,CHARTYPE query_type,LINETYPE argc,CHARTYPE *arg,LINETYPE arglen)
/***********************************************************************/
{
   return set_on_off_value((bool)CURRENT_TOF,1);
}
/***********************************************************************/
short extract_tofeof(short number_variables,short itemno,CHARTYPE *itemargs,CHARTYPE query_type,LINETYPE argc,CHARTYPE *arg,LINETYPE arglen)
/***********************************************************************/
{
   return set_on_off_value(CURRENT_VIEW->tofeof,1);
}
/***********************************************************************/
short extract_tof_function(short number_variables,short itemno,CHARTYPE *itemargs,CHARTYPE query_type,LINETYPE argc,CHARTYPE *arg,LINETYPE arglen)
/***********************************************************************/
{
   return set_boolean_value((bool)(FOCUS_TOF && CURRENT_VIEW->current_window != WINDOW_COMMAND),(short)1);
}
/***********************************************************************/
short extract_topedge_function(short number_variables,short itemno,CHARTYPE *itemargs,CHARTYPE query_type,LINETYPE argc,CHARTYPE *arg,LINETYPE arglen)
/***********************************************************************/
{
   short y=0;
   bool bool_flag=FALSE;

   if (batch_only)
   {
      item_values[1].value = (CHARTYPE *)"0";
      item_values[1].len = 1;
      return 1;
   }
   if (CURRENT_VIEW->current_window == WINDOW_FILEAREA)
      bool_flag = (bool)(query2_filearea_cursor_row(&y) && y == 0);
   return set_boolean_value(bool_flag,(short)1);
}
/***********************************************************************/
short extract_trailing(short number_variables,short itemno,CHARTYPE *itemargs,CHARTYPE query_type,LINETYPE argc,CHARTYPE *arg,LINETYPE arglen)
/***********************************************************************/
{
   switch( CURRENT_FILE->trailing )
   {
      case TRAILING_ON:
         item_values[1].value = (CHARTYPE *)"ON";
         item_values[1].len = 2;
         break;
      case TRAILING_OFF:
         item_values[1].value = (CHARTYPE *)"OFF";
         item_values[1].len = 3;
         break;
      case TRAILING_SINGLE:
         item_values[1].value = (CHARTYPE *)"SINGLE";
         item_values[1].len = 6;
         break;
      case TRAILING_EMPTY:
         item_values[1].value = (CHARTYPE *)"EMPTY";
         item_values[1].len = 5;
         break;
      case TRAILING_REMOVE:
         item_values[1].value = (CHARTYPE *)"REMOVE";
         item_values[1].len = 6;
         break;
      default:
         break;
   }
   return number_variables;
}
/***********************************************************************/
short extract_typeahead(short number_variables,short itemno,CHARTYPE *itemargs,CHARTYPE query_type,LINETYPE argc,CHARTYPE *arg,LINETYPE arglen)
/***********************************************************************/
{
   return set_on_off_value(TYPEAHEADx,1);
}
/***********************************************************************/
short extract_ui(short number_variables,short itemno,CHARTYPE *itemargs,CHARTYPE query_type,LINETYPE argc,CHARTYPE *arg,LINETYPE arglen)
/***********************************************************************/
{
#if defined(USE_XCURSES) || defined(PDCURSES)
# if defined(HAVE_CURSES_VERSION)
   static const char *port_text[] = { "X11", "WinCon", "WinGUI",
         "DOS", "OS/2", "SDL1", "SDL2", "VT", "DOSVGA", "Plan9", "LinuxFramebuffer" };
   PDC_VERSION vinfo;
   PDC_get_version( &vinfo);
#   ifdef __PDCURSESMOD__
   if ( vinfo.port < sizeof(port_text) )
      sprintf((DEFCHAR *)query_rsrvd,"%s Build: %d Port: %s", curses_version(), PDC_BUILD, port_text[vinfo.port]);
   else
#   endif
      sprintf((DEFCHAR *)query_rsrvd,"%s Build: %d Port: %s", curses_version(), PDC_BUILD, "Unknown");
# else
   sprintf((DEFCHAR *)query_rsrvd,"PDCurses Build: %d", PDC_BUILD);
# endif
#elif defined(USE_NCURSES)
# if NCURSES_VERSION_MAJOR > 4
   sprintf((DEFCHAR *)query_rsrvd,"%s", curses_version());
# else
   strcpy((DEFCHAR *)query_rsrvd,NCURSES_VERSION);
# endif
#elif defined(USE_EXTCURSES)
   sprintf((DEFCHAR *)query_rsrvd,"Extended Curses");
#else
   sprintf((DEFCHAR *)query_rsrvd,"Standard Curses");
#endif
   item_values[1].value = query_rsrvd;
   item_values[1].len = strlen((DEFCHAR *)query_rsrvd);
   return number_variables;
}
/***********************************************************************/
short extract_undoing(short number_variables,short itemno,CHARTYPE *itemargs,CHARTYPE query_type,LINETYPE argc,CHARTYPE *arg,LINETYPE arglen)
/***********************************************************************/
{
   return set_on_off_value(CURRENT_FILE->undoing,1);
}
/***********************************************************************/
short extract_untaa(short number_variables,short itemno,CHARTYPE *itemargs,CHARTYPE query_type,LINETYPE argc,CHARTYPE *arg,LINETYPE arglen)
/***********************************************************************/
{
   return set_on_off_value(UNTAAx,1);
}

/***********************************************************************/
short extract_utf(short number_variables,short itemno,CHARTYPE *itemargs,CHARTYPE query_type,LINETYPE argc,CHARTYPE *arg,LINETYPE arglen)
/***********************************************************************/
{
#ifdef USE_UTF8
   return set_on_off_value(1,1);
#else
   return set_on_off_value(0,1);
#endif
}
/***********************************************************************/
short extract_variant(short number_variables,short itemno,CHARTYPE *itemargs,CHARTYPE query_type,LINETYPE argc,CHARTYPE *arg,LINETYPE arglen)
/***********************************************************************/
{
   item_values[1].value = (CHARTYPE *)THE_VARIANT;
   item_values[1].len = strlen((DEFCHAR *)THE_VARIANT);
   return number_variables;
}
/***********************************************************************/
short extract_verify(short number_variables,short itemno,CHARTYPE *itemargs,CHARTYPE query_type,LINETYPE argc,CHARTYPE *arg,LINETYPE arglen)
/***********************************************************************/
{
   sprintf((DEFCHAR *)query_num3,"%ld %ld",CURRENT_VIEW->verify_start,CURRENT_VIEW->verify_end);
   item_values[1].value = query_num3;
   item_values[1].len = strlen((DEFCHAR *)query_num3);
   return number_variables;
}
/***********************************************************************/
short extract_vershift(short number_variables,short itemno,CHARTYPE *itemargs,CHARTYPE query_type,LINETYPE argc,CHARTYPE *arg,LINETYPE arglen)
/***********************************************************************/
{
   sprintf((DEFCHAR *)query_rsrvd,"%ld",(LINETYPE)CURRENT_VIEW->verify_col - (LINETYPE)CURRENT_VIEW->verify_start);
   item_values[1].value = query_rsrvd;
   item_values[1].len = strlen((DEFCHAR *)query_rsrvd);
   return number_variables;
}
/***********************************************************************/
short extract_verone_function(short number_variables,short itemno,CHARTYPE *itemargs,CHARTYPE query_type,LINETYPE argc,CHARTYPE *arg,LINETYPE arglen)
/***********************************************************************/
{
   return set_boolean_value((bool)(CURRENT_VIEW->verify_col == 1),(short)1);
}
/***********************************************************************/
short extract_version(short number_variables,short itemno,CHARTYPE *itemargs,CHARTYPE query_type,LINETYPE argc,CHARTYPE *arg,LINETYPE arglen)
/***********************************************************************/
{
   item_values[1].value = (CHARTYPE *)"THE";
   item_values[1].len = 3;
   item_values[2].value = (CHARTYPE *)the_version;
   item_values[2].len = strlen((DEFCHAR *)the_version);
   item_values[3].value = (CHARTYPE *)"???";
#if defined(DOS)
# if defined(EMX)
   if (_osmode == DOS_MODE)
      item_values[3].value = (CHARTYPE *)"DOS";
   else
      item_values[3].value = (CHARTYPE *)"OS2";
#else
   item_values[3].value = (CHARTYPE *)"DOS";
# endif
#endif
#if defined(OS2)
# if defined(EMX)
   if (_osmode == DOS_MODE)
      item_values[3].value = (CHARTYPE *)"DOS";
   else
      item_values[3].value = (CHARTYPE *)"OS2";
#else
   item_values[3].value = (CHARTYPE *)"OS2";
# endif
#endif
#if defined(UNIX)
# if defined(__QNX__)
   item_values[3].value = (CHARTYPE *)"QNX";
# else
   item_values[3].value = (CHARTYPE *)"UNIX";
# endif
#endif
#if defined(USE_XCURSES)
   item_values[3].value = (CHARTYPE *)"X11";
#endif
#if defined(MSWIN)
   item_values[3].value = (CHARTYPE *)"MS-WINDOWS";
#endif
#if defined(_WIN32)
   item_values[3].value = (CHARTYPE *)"WIN32";
#endif
#if defined(_WIN64)
   item_values[3].value = (CHARTYPE *)"WIN64";
#endif
#if defined(AMIGA)
   item_values[3].value = (CHARTYPE *)"AMIGA";
#endif
#if defined(__BEOS__)
   item_values[3].value = (CHARTYPE *)"BEOS";
#endif
   item_values[3].len = strlen((DEFCHAR *)item_values[3].value);
   item_values[4].value = (CHARTYPE *)the_release;
   item_values[4].len = strlen((DEFCHAR *)item_values[4].value);
   item_values[5].value = item_values[3].value;
   item_values[5].len = item_values[3].len;
#ifdef MH_KERNEL_NAME
   item_values[5].value = (CHARTYPE *)MH_KERNEL_NAME;
   item_values[5].len = strlen( (DEFCHAR *)item_values[5].value );
#endif
   return number_variables;
}
/***********************************************************************/
short extract_width(short number_variables,short itemno,CHARTYPE *itemargs,CHARTYPE query_type,LINETYPE argc,CHARTYPE *arg,LINETYPE arglen)
/***********************************************************************/
{
   sprintf((DEFCHAR *)query_num1,"%ld",max_line_length);
   item_values[1].value = query_num1;
   item_values[1].len = strlen((DEFCHAR *)query_num1);
   return number_variables;
}
/***********************************************************************/
short extract_word(short number_variables,short itemno,CHARTYPE *itemargs,CHARTYPE query_type,LINETYPE argc,CHARTYPE *arg,LINETYPE arglen)
/***********************************************************************/
{
   if (CURRENT_VIEW->word == 'A')
   {
      item_values[1].value = (CHARTYPE *)"ALPHANUM";
      item_values[1].len = 8;
   }
   else
   {
      item_values[1].value = (CHARTYPE *)"NONBLANK";
      item_values[1].len = 8;
   }
   return number_variables;
}
/***********************************************************************/
short extract_wordwrap(short number_variables,short itemno,CHARTYPE *itemargs,CHARTYPE query_type,LINETYPE argc,CHARTYPE *arg,LINETYPE arglen)
/***********************************************************************/
{
   return set_on_off_value(CURRENT_VIEW->wordwrap,1);
}
/***********************************************************************/
short extract_wrap(short number_variables,short itemno,CHARTYPE *itemargs,CHARTYPE query_type,LINETYPE argc,CHARTYPE *arg,LINETYPE arglen)
/***********************************************************************/
{
   return set_on_off_value(CURRENT_VIEW->wrap,1);
}
/***********************************************************************/
short extract_xterminal(short number_variables,short itemno,CHARTYPE *itemargs,CHARTYPE query_type,LINETYPE argc,CHARTYPE *arg,LINETYPE arglen)
/***********************************************************************/
{
   item_values[1].value = xterm_program;
   item_values[1].len = strlen((DEFCHAR *)xterm_program);
   return number_variables;
}
/***********************************************************************/
short extract_zone(short number_variables,short itemno,CHARTYPE *itemargs,CHARTYPE query_type,LINETYPE argc,CHARTYPE *arg,LINETYPE arglen)
/***********************************************************************/
{
   sprintf((DEFCHAR *)query_num1,"%ld",CURRENT_VIEW->zone_start);
   item_values[1].value = query_num1;
   item_values[1].len = strlen((DEFCHAR *)query_num1);
   sprintf((DEFCHAR *)query_num2,"%ld",CURRENT_VIEW->zone_end);
   item_values[2].value = query_num2;
   item_values[2].len = strlen((DEFCHAR *)query_num2);
   return number_variables;
}
