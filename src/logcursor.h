#ifndef THE_LOGCURSOR_H
#define THE_LOGCURSOR_H

#include <stddef.h>

#include "textpos.h"

typedef enum
{
   LOGICAL_CURSOR_ZONE_NONE = 0,
   LOGICAL_CURSOR_ZONE_FILEAREA,
   LOGICAL_CURSOR_ZONE_PREFIX,
   LOGICAL_CURSOR_ZONE_COMMAND,
   LOGICAL_CURSOR_ZONE_PROMPT,
   LOGICAL_CURSOR_ZONE_STATUS
} LogicalCursorZone;

typedef struct
{
   LogicalCursorZone zone;
   LINETYPE line_number;
   int zone_row;
   TextPos text;
   int desired_cell;
   int valid;
} LogicalCursor;

typedef struct
{
   LogicalCursor current;
   LogicalCursor previous;
} LogicalCursorState;

const char *logical_cursor_zone_name(LogicalCursorZone zone);
LogicalCursor logical_cursor_invalid(void);
LogicalCursor logical_cursor_make(LogicalCursorZone zone, LINETYPE line_number,
                                  int zone_row, TextPos text);
LogicalCursor logical_cursor_from_cell(LogicalCursorZone zone, LINETYPE line_number,
                                       int zone_row, const CHARTYPE *line,
                                       size_t len, int cell_column,
                                       TextSnap snap, int allow_virtual);
LogicalCursor logical_cursor_move_left(LogicalCursor cursor, const CHARTYPE *line,
                                       size_t len, int allow_virtual);
LogicalCursor logical_cursor_move_right(LogicalCursor cursor, const CHARTYPE *line,
                                        size_t len, int allow_virtual);
void logical_cursor_set_desired_cell(LogicalCursor *cursor, int desired_cell);
int logical_cursor_is_virtual(LogicalCursor cursor, const CHARTYPE *line, size_t len);
void logical_cursor_state_init(LogicalCursorState *state);
void logical_cursor_state_focus(LogicalCursorState *state, LogicalCursor cursor);

#endif
