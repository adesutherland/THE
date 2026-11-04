#include "screenframe.h"

#include <string.h>
#include <stdio.h>

#include "the.h"
#include "reservedfooter.h"
#ifdef THE_WASM_HIGHLIGHT
#include "wasm/highlighter.h"
#endif

/* The same column markers serve terminal rendering and semantic/web frames. */
void screenframe_format_ruler(const struct view_details *view, short line_type,
                             CHARTYPE *out, int columns)
{
   int j, k;
   if (!view || !out || columns <= 0) return;
   memset(out,' ',columns);
   for (j=0;j<columns;j++)
   {
      long col=(long)view->verify_col-1+j;
      if ((line_type & LINE_SCALE) && view->current_column==col+1)
      { out[j]='|'; continue; }
      if (line_type & LINE_TABLINE)
      {
         for (k=0;k<view->numtabs;k++) if (view->tabs[k]==col+1) break;
         if (k<view->numtabs) { out[j]='|'; continue; }
      }
      if (!(line_type & LINE_SCALE)) continue;
      if (view->margin_left-1==col) { out[j]='['; continue; }
      if (view->margin_right-1==col) { out[j]=']'; continue; }
      if ((view->margin_indent_offset_status && view->margin_left+view->margin_indent-1==col) ||
          (!view->margin_indent_offset_status && view->margin_indent-1==col))
      { out[j]='p'; continue; }
      if (view->zone_start-1==col) { out[j]='<'; continue; }
      if (view->zone_end-1==col) { out[j]='>'; continue; }
      if (col % 10==9)
      {
         char tens[32]; int length, offset;
         snprintf(tens,sizeof(tens),"%ld",col/10+1);
         length=(int)strlen(tens);
         for (offset=length-1;offset>=0;offset--)
            if (j-offset>=0 && (!offset || out[j-offset]==' ' || out[j-offset]=='.'))
               out[j-offset]=tens[length-offset-1];
      }
      else out[j]=col % 5==4 ? '+' : '.';
   }
}

UiRowRole screenframe_role_from_line_type(short line_type)
{
   if (line_type == LINE_TOF)
      return UI_ROW_TOF;
   if (line_type == LINE_EOF)
      return UI_ROW_EOF;
   if (line_type & (LINE_OUT_OF_BOUNDS_ABOVE | LINE_OUT_OF_BOUNDS_BELOW))
      return UI_ROW_OUT_OF_BOUNDS;
   if (line_type & LINE_HEXSHOW)
      return UI_ROW_HEX;
   if (line_type & LINE_RESERVED)
      return UI_ROW_RESERVED;
   if (line_type & LINE_SHADOW)
      return UI_ROW_SHADOW;
   if (line_type & LINE_BOUNDS)
      return UI_ROW_BOUNDS;
   if (line_type & LINE_SCALE)
      return UI_ROW_SCALE;
   if (line_type & LINE_TABLINE)
      return UI_ROW_TABLINE;
   return UI_ROW_FILE;
}

static UiSyntaxStyle screenframe_syntax_style_from_ecolour(unsigned char ecolour)
{
   switch (ecolour)
   {
      case ECOLOUR_COMMENTS:
         return UI_SYNTAX_COMMENT;
      case ECOLOUR_STRINGS:
         return UI_SYNTAX_STRING;
      case ECOLOUR_NUMBERS:
         return UI_SYNTAX_NUMBER;
      case ECOLOUR_KEYWORDS:
         return UI_SYNTAX_KEYWORD;
      case ECOLOUR_LABEL:
         return UI_SYNTAX_IDENTIFIER;
      case ECOLOUR_PREDIR:
         return UI_SYNTAX_PREPROCESSOR;
      case ECOLOUR_HEADER:
         return UI_SYNTAX_HEADER;
      case ECOLOUR_MATCH:
         return UI_SYNTAX_MATCH;
      case ECOLOUR_OPERATOR:
         return UI_SYNTAX_OPERATOR;
      case ECOLOUR_PAREN:
         return UI_SYNTAX_PAREN;
      case ECOLOUR_TYPES:
         return UI_SYNTAX_TYPE;
      case ECOLOUR_CONSTANTS:
         return UI_SYNTAX_CONSTANT;
      case ECOLOUR_MACROS:
         return UI_SYNTAX_MACRO;
      case ECOLOUR_MACRO_VARIABLES:
         return UI_SYNTAX_MACRO_VARIABLE;
      case ECOLOUR_MACRO_CONSTANTS:
         return UI_SYNTAX_MACRO_CONSTANT;
      case ECOLOUR_PUNCTUATION:
         return UI_SYNTAX_PUNCTUATION;
      case ECOLOUR_INC_STRING:
         return UI_SYNTAX_INCOMPLETE_STRING;
      case ECOLOUR_HTML_TAG:
      case ECOLOUR_HTML_CHAR:
         return UI_SYNTAX_MARKUP;
      case ECOLOUR_FUNCTIONS:
         return UI_SYNTAX_FUNCTION;
      case ECOLOUR_DIRECTORY:
         return UI_SYNTAX_DIRECTORY;
      case ECOLOUR_LINK:
         return UI_SYNTAX_LINK;
      case ECOLOUR_EXECUTABLE:
         return UI_SYNTAX_EXECUTABLE;
      case ECOLOUR_ALT_KEYWORD_1:
         return UI_SYNTAX_ALT_KEYWORD_1;
      case ECOLOUR_ALT_KEYWORD_2:
         return UI_SYNTAX_ALT_KEYWORD_2;
      case ECOLOUR_ALT_KEYWORD_3:
         return UI_SYNTAX_ALT_KEYWORD_3;
      case ECOLOUR_ALT_KEYWORD_4:
         return UI_SYNTAX_ALT_KEYWORD_4;
      case ECOLOUR_ALT_KEYWORD_5:
         return UI_SYNTAX_ALT_KEYWORD_5;
      case ECOLOUR_ALT_KEYWORD_6:
         return UI_SYNTAX_ALT_KEYWORD_6;
      case ECOLOUR_ALT_KEYWORD_7:
         return UI_SYNTAX_ALT_KEYWORD_7;
      case ECOLOUR_ALT_KEYWORD_8:
         return UI_SYNTAX_ALT_KEYWORD_8;
      case ECOLOUR_ALT_KEYWORD_9:
         return UI_SYNTAX_ALT_KEYWORD_9;
      case ECOLOUR_NONE:
      default:
         return UI_SYNTAX_NONE;
   }
}

static void screenframe_add_syntax_styles(UiFrame *frame, size_t row_index,
                                          const SHOW_LINE *row,
                                          FILE_DETAILS *file)
{
   size_t start = 0, point = 0;
   TextPos pos = textpos_begin();
   UiSyntaxStyle previous = UI_SYNTAX_NONE;
   if (frame == NULL || row == NULL || row->contents == NULL)
      return;
#ifdef USE_SDSLH
   CodeBufferLine *parser_line = NULL;
   int locked = 0;
   if (file && file->colouring && file->parser && file->parser->is_sdslh_parser
   && file->cb && row && row->line_type == LINE_LINE && row->line_number > 0)
   {
      locked = enter_codeblock_critical_section() == 0;
      if (locked && (size_t)row->line_number <= file->cb->line_count)
         parser_line = &file->cb->lines[row->line_number - 1];
   }
#else
   (void)file;
#endif

   while (pos.byte_offset < row->length)
   {
      UiSyntaxStyle style = row->highlight_type
         ? screenframe_syntax_style_from_ecolour(row->highlight_type[pos.byte_offset])
         : UI_SYNTAX_NONE;
#ifdef USE_SDSLH
      /* Match the native renderer's severity-over-syntax precedence. Parser
       * slots and wire style positions are codepoints, not UTF-8 bytes. */
      if (parser_line && parser_line->characters && point < parser_line->length)
      {
         UiSyntaxStyle diagnostic = ui_diagnostic_style(parser_line->characters[point].severity);
         if (diagnostic != UI_SYNTAX_NONE) style = diagnostic;
      }
#endif
      if (style != previous)
      {
         if (previous != UI_SYNTAX_NONE)
            ui_frame_add_row_style(frame, row_index, (int)start,
                                   (int)(point - start), previous);
         previous = style;
         start = point;
      }
      pos = textpos_next_codepoint(row->contents, row->length, pos);
      point++;
   }
   if (previous != UI_SYNTAX_NONE)
      ui_frame_add_row_style(frame, row_index, (int)start,
                             (int)(point - start), previous);
#ifdef USE_SDSLH
   if (locked) exit_codeblock_critical_section();
#endif
}

static const CHARTYPE *screenframe_text_for_row(VIEW_DETAILS *view,
                                                SHOW_LINE *row,
                                                size_t *len)
{
   if (len != NULL)
      *len = 0;
   if (row == NULL)
      return NULL;
   if (row->line_type == LINE_TOF)
   {
      if (view != NULL && view->tofeof)
      {
         if (len != NULL)
            *len = strlen((const char *)TOP_OF_FILE);
         return TOP_OF_FILE;
      }
      return (const CHARTYPE *)"";
   }
   if (row->line_type == LINE_EOF)
   {
      if (view != NULL && view->tofeof)
      {
         if (len != NULL)
            *len = strlen((const char *)BOTTOM_OF_FILE);
         return BOTTOM_OF_FILE;
      }
      return (const CHARTYPE *)"";
   }
   if (len != NULL)
      *len = row->length;
   return row->contents;
}

static void screenframe_footer(UiFrameRow *row, RESERVED *reserved)
{
   if (!reserved || !reserved->footer[0]) return;
   row->footer = reserved->footer;
   row->footer_text = reserved->disp;
   row->footer_text_len = reserved->disp_length;
   row->footer_after_cmdline = reserved->footer_after_cmdline;
   if (reserved->attr)
   {
      const COLOUR_ATTR *attr = reserved->attr;
      row->footer_attr = attr->fg != THE_COLOR_UNSPECIFIED
                        && attr->bg != THE_COLOR_UNSPECIFIED
         ? the_render_attr_make(attr->fg, attr->bg, attr->style)
         : the_render_attr_from_style(attr->mono_style);
   }
}

int screenframe_build(CHARTYPE scrno, UiFrame *frame)
{
   SCREEN_DETAILS *details;
   VIEW_DETAILS *view;
   size_t i;
   size_t rows;

   if (frame == NULL || scrno >= MAX_SCREENS)
      return 0;

   details = &screen[scrno];
   view = details->screen_view;
   if (view == NULL || details->sl == NULL)
      return 0;

   ui_frame_init(frame, details->rows[WINDOW_FILEAREA],
                 details->cols[WINDOW_FILEAREA]);
   frame->regions.filearea = (UiRegion){details->start_row[WINDOW_FILEAREA],
      details->rows[WINDOW_FILEAREA], details->screen_start_col, details->screen_cols};
   if (details->rows[WINDOW_COMMAND])
      frame->regions.command = (UiRegion){details->start_row[WINDOW_COMMAND], 1,
         details->screen_start_col, details->screen_cols};
   if (details->rows[WINDOW_FOOTER])
      frame->regions.footer = (UiRegion){details->start_row[WINDOW_FOOTER],
         details->rows[WINDOW_FOOTER], details->start_col[WINDOW_FOOTER],
         details->cols[WINDOW_FOOTER]};
   rows = details->rows[WINDOW_FILEAREA];
   if (rows > UI_DRIVER_MAX_ROWS)
      rows = UI_DRIVER_MAX_ROWS;

   for (i = 0; i < rows; i++)
   {
      SHOW_LINE *show_row = &details->sl[i];
      UiRowRole role = screenframe_role_from_line_type(show_row->line_type);
      const CHARTYPE *text;
      size_t text_len = 0;

      text = screenframe_text_for_row(view, show_row, &text_len);
      if (!ui_frame_set_row(frame, i, role, show_row->line_number, (int)i,
                            (int)view->verify_col - 1, text, text_len,
                            show_row->main_enterable))
      {
         return 0;
      }
      ui_frame_set_row_prefix(frame, i, show_row->prefix,
                              strlen((const char *)show_row->prefix),
                              show_row->prefix_enterable);
      frame->row[i].physical_row = details->start_row[WINDOW_FILEAREA] + (int)i;
      if (role == UI_ROW_RESERVED && show_row->rsrvd != NULL
      &&  show_row->rsrvd->footer[0] != '\0')
      {
         screenframe_footer(&frame->row[i], show_row->rsrvd);
      }
#ifdef THE_WASM_HIGHLIGHT
      if (!the_wasm_highlight_row(SCREEN_FILE(scrno), frame, i))
#endif
         screenframe_add_syntax_styles(frame, i, show_row, SCREEN_FILE(scrno));
   }

   for (i = 0; i < (size_t)details->rows[WINDOW_FOOTER]
                && frame->row_count < UI_DRIVER_MAX_ROWS; i++)
   {
      size_t index = frame->row_count;
      int physical_row = details->start_row[WINDOW_FOOTER] + (int)i;
      RESERVED *reserved = reserved_footer_at(view->file_for_view->first_reserved,
         (int)i, details->rows[WINDOW_FILEAREA] + details->rows[WINDOW_FOOTER]);
      if (!reserved) continue;
      ui_frame_set_row(frame, index, UI_ROW_RESERVED, -1,
         physical_row - details->start_row[WINDOW_FILEAREA], 0,
         reserved->disp, reserved->disp_length, FALSE);
      frame->row[index].physical_row = physical_row;
      screenframe_footer(&frame->row[index], reserved);
   }

   ui_frame_set_cursor_rebased(frame, view->logical_cursor.current);
   return 1;
}
