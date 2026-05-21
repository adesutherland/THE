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
                              strlen((const char *)show_row->prefix));
   }

   ui_frame_set_cursor(frame, view->logical_cursor.current);
   return 1;
}
