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
         return 1;
      case UI_ROW_EMPTY:
      case UI_ROW_TOF:
      case UI_ROW_EOF:
      case UI_ROW_RESERVED:
      case UI_ROW_BOUNDS:
      case UI_ROW_SCALE:
      case UI_ROW_TABLINE:
      case UI_ROW_STATUS:
      default:
         return 0;
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

int ui_frame_set_cursor(UiFrame *frame, LogicalCursor cursor)
{
   size_t i;

   if (frame == NULL || !cursor.valid)
      return 0;
   for (i = 0; i < frame->row_count; i++)
   {
      UiFrameRow *row = &frame->row[i];

      if (!ui_row_role_allows_cursor(row->role))
         continue;
      if (row->screen_row == cursor.zone_row)
      {
         frame->cursor.valid = 1;
         frame->cursor.cursor = cursor;
         return 1;
      }
   }
   return 0;
}

static UiRowRole ui_row_role_from_cursor_zone(LogicalCursorZone zone)
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
