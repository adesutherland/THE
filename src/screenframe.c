#include "screenframe.h"

#include <string.h>

#include "the.h"

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
                                          const SHOW_LINE *row)
{
   size_t start;
   size_t pos = 0;

   if (frame == NULL || row == NULL || row->highlight_type == NULL)
      return;
   while (pos < row->length)
   {
      UiSyntaxStyle style;

      style = screenframe_syntax_style_from_ecolour(row->highlight_type[pos]);
      if (style == UI_SYNTAX_NONE)
      {
         pos++;
         continue;
      }
      start = pos++;
      while (pos < row->length
      &&     screenframe_syntax_style_from_ecolour(row->highlight_type[pos]) == style)
      {
         pos++;
      }
      ui_frame_add_row_style(frame, row_index, (int)start,
                             (int)(pos - start), style);
   }
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
      screenframe_add_syntax_styles(frame, i, show_row);
   }

   ui_frame_set_cursor_rebased(frame, view->logical_cursor.current);
   return 1;
}
