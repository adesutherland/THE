#ifndef THE_UIDRIVER_H
#define THE_UIDRIVER_H

#include <stddef.h>

#include "logcursor.h"

#define UI_DRIVER_MAX_ROWS 256
#define UI_DRIVER_MAX_OPS 1024
#define UI_DRIVER_MAX_STYLE_RUNS 128

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
   UI_ROW_PROMPT,
   UI_ROW_DIVIDER,
   UI_ROW_WINDOW
} UiRowRole;

typedef enum
{
   UI_SYNTAX_NONE = 0,
   UI_SYNTAX_COMMENT,
   UI_SYNTAX_STRING,
   UI_SYNTAX_NUMBER,
   UI_SYNTAX_KEYWORD,
   UI_SYNTAX_IDENTIFIER,
   UI_SYNTAX_PREPROCESSOR,
   UI_SYNTAX_HEADER,
   UI_SYNTAX_INCOMPLETE_STRING,
   UI_SYNTAX_MARKUP,
   UI_SYNTAX_MATCH,
   UI_SYNTAX_OPERATOR,
   UI_SYNTAX_PAREN,
   UI_SYNTAX_TYPE,
   UI_SYNTAX_CONSTANT,
   UI_SYNTAX_PUNCTUATION,
   UI_SYNTAX_FUNCTION,
   UI_SYNTAX_DIRECTORY,
   UI_SYNTAX_LINK,
   UI_SYNTAX_EXECUTABLE,
   UI_SYNTAX_ALT_KEYWORD_1,
   UI_SYNTAX_ALT_KEYWORD_2,
   UI_SYNTAX_ALT_KEYWORD_3,
   UI_SYNTAX_ALT_KEYWORD_4,
   UI_SYNTAX_ALT_KEYWORD_5,
   UI_SYNTAX_ALT_KEYWORD_6,
   UI_SYNTAX_ALT_KEYWORD_7,
   UI_SYNTAX_ALT_KEYWORD_8,
   UI_SYNTAX_ALT_KEYWORD_9,
   UI_SYNTAX_UNKNOWN
} UiSyntaxStyle;

typedef struct
{
   int start_cell;
   int cell_count;
   UiSyntaxStyle style;
} UiStyleRun;

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
   UiStyleRun styles[UI_DRIVER_MAX_STYLE_RUNS];
   size_t style_count;
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
const char *ui_syntax_style_name(UiSyntaxStyle style);
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
int ui_frame_add_row_style(UiFrame *frame, size_t index, int start_cell,
                           int cell_count, UiSyntaxStyle style);
int ui_frame_find_cursor_row(const UiFrame *frame, LogicalCursor cursor,
                             size_t *index);
int ui_frame_cursor_for_row(const UiFrame *frame, UiRowRole role,
                            LINETYPE line_number, int screen_row,
                            LogicalCursor *cursor);
int ui_frame_cursor_screen_cell(const UiFrame *frame, UiRowRole role,
                                LINETYPE line_number, int screen_row,
                                int viewport_col, int *screen_cell,
                                LogicalCursor *cursor);
int ui_frame_cursor_text_target(const UiFrame *frame,
                                const CHARTYPE **text, size_t *text_len,
                                int *cell);
int ui_frame_set_cursor(UiFrame *frame, LogicalCursor cursor);

void ui_driver_op_log_init(UiDriverOpLog *log);
int ui_driver_op_log_add(UiDriverOpLog *log, UiDriverOp op);
int ui_fake_driver_materialize(const UiFrame *frame, UiDriverOpLog *log);

#endif
