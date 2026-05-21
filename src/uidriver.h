#ifndef THE_UIDRIVER_H
#define THE_UIDRIVER_H

#include <stddef.h>

#include "logcursor.h"

#define UI_DRIVER_MAX_ROWS 256
#define UI_DRIVER_MAX_OPS 1024

typedef enum
{
   UI_ROW_EMPTY = 0,
   UI_ROW_FILE,
   UI_ROW_PREFIX,
   UI_ROW_COMMAND,
   UI_ROW_TOF,
   UI_ROW_EOF,
   UI_ROW_RESERVED,
   UI_ROW_BOUNDS,
   UI_ROW_SCALE,
   UI_ROW_TABLINE,
   UI_ROW_SHADOW,
   UI_ROW_HEX,
   UI_ROW_OUT_OF_BOUNDS,
   UI_ROW_STATUS,
   UI_ROW_PROMPT
} UiRowRole;

typedef struct
{
   UiRowRole role;
   LINETYPE line_number;
   int screen_row;
   int logical_start_col;
   const CHARTYPE *prefix;
   size_t prefix_len;
   int prefix_editable;
   const CHARTYPE *text;
   size_t text_len;
   int editable;
} UiFrameRow;

typedef struct
{
   int valid;
   LogicalCursor cursor;
} UiCursorOverlay;

typedef struct
{
   int rows;
   int cols;
   size_t row_count;
   UiFrameRow row[UI_DRIVER_MAX_ROWS];
   UiCursorOverlay cursor;
} UiFrame;

typedef enum
{
   UI_DRIVER_OP_NONE = 0,
   UI_DRIVER_OP_ROW,
   UI_DRIVER_OP_CURSOR,
   UI_DRIVER_OP_REFRESH
} UiDriverOpKind;

typedef struct
{
   UiDriverOpKind kind;
   UiRowRole role;
   int row;
   int col;
   LINETYPE line_number;
} UiDriverOp;

typedef struct
{
   UiDriverOp op[UI_DRIVER_MAX_OPS];
   size_t count;
} UiDriverOpLog;

const char *ui_row_role_name(UiRowRole role);
int ui_row_role_allows_cursor(UiRowRole role);
UiRowRole ui_row_role_from_cursor_zone(LogicalCursorZone zone);
void ui_frame_init(UiFrame *frame, int rows, int cols);
int ui_frame_set_row(UiFrame *frame, size_t index, UiRowRole role,
                     LINETYPE line_number, int screen_row,
                     int logical_start_col, const CHARTYPE *text,
                     size_t text_len, int editable);
int ui_frame_set_row_prefix(UiFrame *frame, size_t index,
                            const CHARTYPE *prefix, size_t prefix_len,
                            int editable);
int ui_frame_find_cursor_row(const UiFrame *frame, LogicalCursor cursor,
                             size_t *index);
int ui_frame_cursor_for_row(const UiFrame *frame, UiRowRole role,
                            LINETYPE line_number, int screen_row,
                            LogicalCursor *cursor);
int ui_frame_set_cursor(UiFrame *frame, LogicalCursor cursor);

void ui_driver_op_log_init(UiDriverOpLog *log);
int ui_driver_op_log_add(UiDriverOpLog *log, UiDriverOp op);
int ui_fake_driver_materialize(const UiFrame *frame, UiDriverOpLog *log);

#endif
