#ifndef THE_CURSESDRIVER_H
#define THE_CURSESDRIVER_H

#include <stddef.h>

#include "logcursor.h"

struct view_details;

typedef struct
{
   LogicalCursor logical;
   int viewport_col;
   int raw_display_col;
   int display_col;
   int window_cols;
   int visible;
} CursesDriverCursorTarget;

typedef struct
{
   short row;
   short col;
   int valid;
} CursesDriverWindowCursor;

typedef struct
{
   short row;
   short col;
   int valid;
} CursesDriverWindowOrigin;

typedef struct
{
   short rows;
   short cols;
   int valid;
} CursesDriverWindowSize;

typedef struct
{
   short row;
   short col;
   int valid;
} CursesDriverScreenPoint;

typedef enum
{
   CURSES_DRIVER_MOUSE_ACTION_NONE = 0,
   CURSES_DRIVER_MOUSE_ACTION_PRESSED,
   CURSES_DRIVER_MOUSE_ACTION_RELEASED,
   CURSES_DRIVER_MOUSE_ACTION_CLICKED,
   CURSES_DRIVER_MOUSE_ACTION_OTHER
} CursesDriverMouseAction;

typedef struct
{
   int button;
   CursesDriverMouseAction action;
   int modifier;
   int row;
   int col;
   int inside;
   int valid;
} CursesDriverMouseEvent;

int curses_driver_clamp_display_col(int display_col, int window_cols);
int curses_driver_display_col_from_logical(const CHARTYPE *line, size_t len,
                                           int viewport_col, int logical_col);
int curses_driver_logical_col_from_display(const CHARTYPE *line, size_t len,
                                           int viewport_col, int display_col,
                                           TextSnap snap);
int curses_driver_viewport_col_for_logical(const CHARTYPE *line, size_t len,
                                           int current_viewport_col,
                                           int logical_col, int window_cols,
                                           int *display_col, int *visible);
chtype curses_driver_software_cursor_attr(CHARTYPE scrno, chtype base,
                                          CursorShape shape);
void curses_driver_draw_software_chtype_cell(CHARTYPE scrno, WINDOW *win,
                                             short row, int col, chtype base,
                                             CursorShape shape);
void curses_driver_draw_software_blank_cell(CHARTYPE scrno, WINDOW *win,
                                            short row, int col, chtype base,
                                            CursorShape shape);
#ifdef USE_UTF8
void curses_driver_set_cchar_codepoint(cchar_t *dest, uint32_t ch,
                                       chtype colour);
void curses_driver_recolour_cchar(cchar_t *cell, chtype colour);
void curses_driver_write_wide_string_at(WINDOW *win, int row, int col,
                                        const wchar_t *text, chtype colour,
                                        int expected_width);
void curses_driver_fill_cells_at(WINDOW *win, int row, int col, int width,
                                 chtype colour);
void curses_driver_write_ascii_cells_at(WINDOW *win, int row, int col,
                                        const char *text, int width,
                                        chtype colour);
#endif
CursesDriverWindowCursor curses_driver_capture_window_cursor(WINDOW *win);
CursesDriverWindowOrigin curses_driver_window_origin(WINDOW *win);
CursesDriverWindowSize curses_driver_window_size(WINDOW *win);
CursesDriverScreenPoint curses_driver_window_cursor_screen_point(WINDOW *win);
WINDOW *curses_driver_create_window(int rows, int cols, int row, int col);
WINDOW *curses_driver_create_pad(int rows, int cols);
WINDOW *curses_driver_create_relative_window(WINDOW *parent, int rows,
                                             int cols, int row, int col);
void curses_driver_delete_window(WINDOW *win);
void curses_driver_enable_keypad(WINDOW *win, bool enabled);
void curses_driver_move_window_cursor(WINDOW *win, short row, short col);
void curses_driver_restore_window_cursor(WINDOW *win,
                                         CursesDriverWindowCursor cursor);
chtype curses_driver_read_window_cell(WINDOW *win);
void curses_driver_set_window_attr(WINDOW *win, chtype colour);
void curses_driver_set_window_background(WINDOW *win, chtype colour);
void curses_driver_clear_window(WINDOW *win);
void curses_driver_clear_window_to_bottom(WINDOW *win);
void curses_driver_clear_to_eol(WINDOW *win);
void curses_driver_touch_window(WINDOW *win);
void curses_driver_touch_line(WINDOW *win, int start, int count);
void curses_driver_clear_line_at(WINDOW *win, short row, chtype colour);
void curses_driver_refresh_window(WINDOW *win);
void curses_driver_refresh_window_now(WINDOW *win);
void curses_driver_refresh_standard_screen(void);
void curses_driver_refresh_pad(WINDOW *pad, int pad_row, int pad_col,
                               int screen_top, int screen_left,
                               int screen_bottom, int screen_right);
void curses_driver_update(void);
void curses_driver_present_cursor(bool visible);
void curses_driver_set_window_timeout(WINDOW *win, int milliseconds);
void curses_driver_draw_box(WINDOW *win);
void curses_driver_add_string(WINDOW *win, const char *text);
void curses_driver_add_string_at(WINDOW *win, short row, short col,
                                 const char *text);
void curses_driver_add_chtype_at(WINDOW *win, short row, short col, chtype ch);
void curses_driver_draw_horizontal_line(WINDOW *win, chtype ch, int len);
int curses_driver_read_window_key(WINDOW *win);
int curses_driver_read_standard_key(void);
int curses_driver_read_raw_standard_key(void);
void curses_driver_mouse_position(WINDOW *win, int *row, int *col);
int curses_driver_read_mouse_event(WINDOW *win, CursesDriverMouseEvent *event);
void curses_driver_prepare_standard_screen_for_shell(void);
void curses_driver_force_background_and_refresh(WINDOW *win);
void curses_driver_clear_standard_window(void);
void curses_driver_set_standard_attr(chtype colour);
void curses_driver_add_standard_string_at(short row, short col,
                                          const char *text);
void curses_driver_move_standard_cursor(short row, short col);
void curses_driver_add_standard_ch(chtype ch);
#ifdef HAVE_WADDCHNSTR
void curses_driver_write_chtype_span(WINDOW *win, const chtype *text, int len);
# ifdef USE_UTF8
void curses_driver_write_cchar_span(WINDOW *win, const cchar_t *text, int len);
# endif
#endif
void curses_driver_add_chtype(WINDOW *win, chtype ch);
#ifdef USE_UTF8
void curses_driver_add_cchar(WINDOW *win, const cchar_t *ch);
#endif
void curses_driver_redraw_window(WINDOW *win);
short curses_driver_refresh_cursor(CHARTYPE scrno);
short curses_driver_redraw_screen_cursor(CHARTYPE scrno, struct view_details *view);
void curses_driver_move_prefix_cursor(CHARTYPE scrno, short row, short col);
CursesDriverCursorTarget curses_driver_filearea_target(
   LogicalCursor cursor, const CHARTYPE *line, size_t len,
   int viewport_col, int window_cols);
short curses_driver_move_filearea_cursor(CHARTYPE scrno, struct view_details *view,
                                         const CHARTYPE *line, size_t len,
                                         short row, int logical_col);
short curses_driver_filearea_cursor_transition(CHARTYPE scrno,
                                               struct view_details *view,
                                               const CHARTYPE *line, size_t len,
                                               short old_row,
                                               int old_logical_cell,
                                               int new_logical_cell,
                                               LENGTHTYPE old_verify_col);

#endif
