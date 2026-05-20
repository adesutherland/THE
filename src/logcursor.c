#include "logcursor.h"

static TextPos line_end_pos(const CHARTYPE *line, size_t len)
{
   return textpos_from_cluster(line, len, (size_t)-1);
}

const char *logical_cursor_zone_name(LogicalCursorZone zone)
{
   switch (zone)
   {
      case LOGICAL_CURSOR_ZONE_FILEAREA:
         return "filearea";
      case LOGICAL_CURSOR_ZONE_PREFIX:
         return "prefix";
      case LOGICAL_CURSOR_ZONE_COMMAND:
         return "command";
      case LOGICAL_CURSOR_ZONE_PROMPT:
         return "prompt";
      case LOGICAL_CURSOR_ZONE_STATUS:
         return "status";
      case LOGICAL_CURSOR_ZONE_NONE:
      default:
         return "none";
   }
}

LogicalCursor logical_cursor_invalid(void)
{
   LogicalCursor cursor;

   cursor.zone = LOGICAL_CURSOR_ZONE_NONE;
   cursor.line_number = 0;
   cursor.zone_row = 0;
   cursor.text = textpos_begin();
   cursor.desired_cell = 0;
   cursor.valid = 0;
   return cursor;
}

LogicalCursor logical_cursor_make(LogicalCursorZone zone, LINETYPE line_number,
                                  int zone_row, TextPos text)
{
   LogicalCursor cursor;

   cursor.zone = zone;
   cursor.line_number = line_number;
   cursor.zone_row = zone_row;
   cursor.text = text;
   cursor.desired_cell = text.cell_column;
   cursor.valid = 1;
   return cursor;
}

LogicalCursor logical_cursor_from_cell(LogicalCursorZone zone, LINETYPE line_number,
                                       int zone_row, const CHARTYPE *line,
                                       size_t len, int cell_column,
                                       TextSnap snap, int allow_virtual)
{
   TextPos text;

   if (allow_virtual)
      text = textpos_from_cell_virtual(line, len, cell_column, snap);
   else
      text = textpos_from_cell(line, len, cell_column, snap);
   return logical_cursor_make(zone, line_number, zone_row, text);
}

LogicalCursor logical_cursor_move_left(LogicalCursor cursor, const CHARTYPE *line,
                                       size_t len, int allow_virtual)
{
   TextPos end;

   if (!cursor.valid || cursor.text.cell_column <= 0)
      return cursor;

   end = line_end_pos(line, len);
   if (allow_virtual && cursor.text.cell_column > end.cell_column)
   {
      cursor.text = textpos_from_cell_virtual(line, len,
                                             cursor.text.cell_column - 1,
                                             TEXT_SNAP_BACKWARD);
   }
   else
   {
      cursor.text = textpos_prev_cluster(line, len, cursor.text);
   }
   cursor.desired_cell = cursor.text.cell_column;
   return cursor;
}

LogicalCursor logical_cursor_move_right(LogicalCursor cursor, const CHARTYPE *line,
                                        size_t len, int allow_virtual)
{
   TextPos end;

   if (!cursor.valid)
      return cursor;

   end = line_end_pos(line, len);
   if (allow_virtual && cursor.text.cell_column >= end.cell_column)
   {
      cursor.text = textpos_from_cell_virtual(line, len,
                                             cursor.text.cell_column + 1,
                                             TEXT_SNAP_BACKWARD);
   }
   else
   {
      cursor.text = textpos_next_cluster(line, len, cursor.text);
   }
   cursor.desired_cell = cursor.text.cell_column;
   return cursor;
}

void logical_cursor_set_desired_cell(LogicalCursor *cursor, int desired_cell)
{
   if (cursor == NULL)
      return;
   cursor->desired_cell = desired_cell;
}

int logical_cursor_is_virtual(LogicalCursor cursor, const CHARTYPE *line, size_t len)
{
   TextPos end;

   if (!cursor.valid)
      return 0;
   end = line_end_pos(line, len);
   return cursor.text.cell_column > end.cell_column;
}

void logical_cursor_state_init(LogicalCursorState *state)
{
   if (state == NULL)
      return;
   state->current = logical_cursor_invalid();
   state->previous = logical_cursor_invalid();
}

void logical_cursor_state_focus(LogicalCursorState *state, LogicalCursor cursor)
{
   if (state == NULL)
      return;
   state->previous = state->current;
   state->current = cursor;
}
