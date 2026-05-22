#include "uidriver.h"

#include <string.h>

const char *ui_row_role_name(UiRowRole role)
{
   switch (role)
   {
      case UI_ROW_FILE:
         return "file";
      case UI_ROW_PREFIX:
         return "prefix";
      case UI_ROW_COMMAND:
         return "command";
      case UI_ROW_TOF:
         return "tof";
      case UI_ROW_EOF:
         return "eof";
      case UI_ROW_RESERVED:
         return "reserved";
      case UI_ROW_BOUNDS:
         return "bounds";
      case UI_ROW_SCALE:
         return "scale";
      case UI_ROW_TABLINE:
         return "tabline";
      case UI_ROW_SHADOW:
         return "shadow";
      case UI_ROW_HEX:
         return "hex";
      case UI_ROW_OUT_OF_BOUNDS:
         return "out-of-bounds";
      case UI_ROW_STATUS:
         return "status";
      case UI_ROW_PROMPT:
         return "prompt";
      case UI_ROW_EMPTY:
      default:
         return "empty";
   }
}

int ui_row_role_allows_cursor(UiRowRole role)
{
   switch (role)
   {
      case UI_ROW_FILE:
      case UI_ROW_PREFIX:
      case UI_ROW_COMMAND:
      case UI_ROW_PROMPT:
      case UI_ROW_TOF:
      case UI_ROW_EOF:
         return 1;
      case UI_ROW_EMPTY:
      case UI_ROW_RESERVED:
      case UI_ROW_BOUNDS:
      case UI_ROW_SCALE:
      case UI_ROW_TABLINE:
      case UI_ROW_SHADOW:
      case UI_ROW_HEX:
      case UI_ROW_OUT_OF_BOUNDS:
      case UI_ROW_STATUS:
      default:
         return 0;
   }
}

static int ui_row_role_displays_filearea_cursor(UiRowRole role)
{
   return role == UI_ROW_FILE
       || role == UI_ROW_TOF
       || role == UI_ROW_EOF;
}

UiRowRole ui_row_role_from_cursor_zone(LogicalCursorZone zone)
{
   switch (zone)
   {
      case LOGICAL_CURSOR_ZONE_FILEAREA:
         return UI_ROW_FILE;
      case LOGICAL_CURSOR_ZONE_PREFIX:
         return UI_ROW_PREFIX;
      case LOGICAL_CURSOR_ZONE_COMMAND:
         return UI_ROW_COMMAND;
      case LOGICAL_CURSOR_ZONE_PROMPT:
         return UI_ROW_PROMPT;
      case LOGICAL_CURSOR_ZONE_STATUS:
      case LOGICAL_CURSOR_ZONE_NONE:
      default:
         return UI_ROW_EMPTY;
   }
}

void ui_frame_init(UiFrame *frame, int rows, int cols)
{
   if (frame == NULL)
      return;
   memset(frame, 0, sizeof(*frame));
   frame->rows = rows;
   frame->cols = cols;
}

int ui_frame_set_row(UiFrame *frame, size_t index, UiRowRole role,
                     LINETYPE line_number, int screen_row,
                     int logical_start_col, const CHARTYPE *text,
                     size_t text_len, int editable)
{
   UiFrameRow *row;

   if (frame == NULL || index >= UI_DRIVER_MAX_ROWS)
      return 0;
   row = &frame->row[index];
   row->role = role;
   row->line_number = line_number;
   row->screen_row = screen_row;
   row->logical_start_col = logical_start_col;
   row->text = text;
   row->text_len = text_len;
   row->editable = editable && ui_row_role_allows_cursor(role);
   if (index >= frame->row_count)
      frame->row_count = index + 1;
   return 1;
}

int ui_frame_set_row_prefix(UiFrame *frame, size_t index,
                            const CHARTYPE *prefix, size_t prefix_len,
                            int editable)
{
   UiFrameRow *row;

   if (frame == NULL || index >= frame->row_count)
      return 0;
   row = &frame->row[index];
   row->prefix = prefix;
   row->prefix_len = prefix_len;
   row->prefix_editable = editable != 0;
   return 1;
}

int ui_frame_find_cursor_row(const UiFrame *frame, LogicalCursor cursor,
                             size_t *index)
{
   size_t i;
   UiRowRole role;

   if (frame == NULL || !cursor.valid)
      return 0;
   role = ui_row_role_from_cursor_zone(cursor.zone);
   if (!ui_row_role_allows_cursor(role))
      return 0;

   for (i = 0; i < frame->row_count; i++)
   {
      const UiFrameRow *row = &frame->row[i];

      if (row->screen_row != cursor.zone_row)
         continue;
      if (role == UI_ROW_PREFIX)
      {
         if (row->role != UI_ROW_PREFIX && !row->prefix_editable)
            continue;
         if (row->line_number != cursor.line_number)
            continue;
         if (index != NULL)
            *index = i;
         return 1;
      }
      if (!ui_row_role_allows_cursor(row->role))
         continue;
      if (role == UI_ROW_FILE)
      {
         if (!ui_row_role_displays_filearea_cursor(row->role))
            continue;
      }
      else if (row->role != role)
      {
         continue;
      }
      if (role == UI_ROW_FILE && row->line_number != cursor.line_number)
         continue;
      if (index != NULL)
         *index = i;
      return 1;
   }
   return 0;
}

int ui_frame_cursor_for_row(const UiFrame *frame, UiRowRole role,
                            LINETYPE line_number, int screen_row,
                            LogicalCursor *cursor)
{
   size_t index;
   const UiFrameRow *row;
   UiRowRole cursor_role;

   if (cursor != NULL)
      *cursor = logical_cursor_invalid();
   if (frame == NULL
   ||  !frame->cursor.valid
   ||  !ui_frame_find_cursor_row(frame, frame->cursor.cursor, &index))
      return 0;

   row = &frame->row[index];
   cursor_role = ui_row_role_from_cursor_zone(frame->cursor.cursor.zone);
   if (cursor_role == UI_ROW_FILE)
   {
      if (!ui_row_role_displays_filearea_cursor(role))
         return 0;
      if (row->role != role)
         return 0;
   }
   else if (cursor_role != role)
   {
      return 0;
   }
   if (row->screen_row != screen_row)
      return 0;
   if (((cursor_role == UI_ROW_FILE && ui_row_role_displays_filearea_cursor(role))
    ||  role == UI_ROW_PREFIX)
   &&  row->line_number != line_number)
      return 0;
   if (cursor != NULL)
      *cursor = frame->cursor.cursor;
   return 1;
}

int ui_frame_set_cursor(UiFrame *frame, LogicalCursor cursor)
{
   if (!ui_frame_find_cursor_row(frame, cursor, NULL))
      return 0;
   frame->cursor.valid = 1;
   frame->cursor.cursor = cursor;
   return 1;
}

void ui_driver_op_log_init(UiDriverOpLog *log)
{
   if (log != NULL)
      memset(log, 0, sizeof(*log));
}

int ui_driver_op_log_add(UiDriverOpLog *log, UiDriverOp op)
{
   if (log == NULL || log->count >= UI_DRIVER_MAX_OPS)
      return 0;
   log->op[log->count++] = op;
   return 1;
}

int ui_fake_driver_materialize(const UiFrame *frame, UiDriverOpLog *log)
{
   size_t i;
   UiDriverOp op;

   if (frame == NULL || log == NULL)
      return 0;
   for (i = 0; i < frame->row_count; i++)
   {
      memset(&op, 0, sizeof(op));
      op.kind = UI_DRIVER_OP_ROW;
      op.role = frame->row[i].role;
      op.row = frame->row[i].screen_row;
      op.line_number = frame->row[i].line_number;
      if (!ui_driver_op_log_add(log, op))
         return 0;
   }
   if (frame->cursor.valid)
   {
      memset(&op, 0, sizeof(op));
      op.kind = UI_DRIVER_OP_CURSOR;
      op.role = ui_row_role_from_cursor_zone(frame->cursor.cursor.zone);
      op.row = frame->cursor.cursor.zone_row;
      op.col = frame->cursor.cursor.text.cell_column;
      op.line_number = frame->cursor.cursor.line_number;
      if (!ui_driver_op_log_add(log, op))
         return 0;
   }
   memset(&op, 0, sizeof(op));
   op.kind = UI_DRIVER_OP_REFRESH;
   return ui_driver_op_log_add(log, op);
}
