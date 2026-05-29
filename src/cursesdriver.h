#ifndef THE_CURSESDRIVER_H
#define THE_CURSESDRIVER_H

#include <stddef.h>

#include "thedriver.h"

struct view_details;

typedef TheDriverWindowCursor CursesDriverWindowCursor;
typedef TheDriverWindowOrigin CursesDriverWindowOrigin;
typedef TheDriverWindowSize CursesDriverWindowSize;
typedef TheDriverScreenPoint CursesDriverScreenPoint;
typedef TheDriverMouseAction CursesDriverMouseAction;

enum
{
   CURSES_DRIVER_MOUSE_ACTION_NONE = THE_DRIVER_MOUSE_ACTION_NONE,
   CURSES_DRIVER_MOUSE_ACTION_PRESSED = THE_DRIVER_MOUSE_ACTION_PRESSED,
   CURSES_DRIVER_MOUSE_ACTION_RELEASED = THE_DRIVER_MOUSE_ACTION_RELEASED,
   CURSES_DRIVER_MOUSE_ACTION_CLICKED = THE_DRIVER_MOUSE_ACTION_CLICKED,
   CURSES_DRIVER_MOUSE_ACTION_OTHER = THE_DRIVER_MOUSE_ACTION_OTHER
};

enum
{
   CURSES_DRIVER_MOUSE_MODIFIER_NONE = THE_DRIVER_MOUSE_MODIFIER_NONE,
   CURSES_DRIVER_MOUSE_MODIFIER_SHIFT = THE_DRIVER_MOUSE_MODIFIER_SHIFT,
   CURSES_DRIVER_MOUSE_MODIFIER_CONTROL = THE_DRIVER_MOUSE_MODIFIER_CONTROL,
   CURSES_DRIVER_MOUSE_MODIFIER_ALT = THE_DRIVER_MOUSE_MODIFIER_ALT
};

enum
{
   CURSES_DRIVER_MOUSE_BUTTON_RELEASED = THE_DRIVER_MOUSE_BUTTON_RELEASED,
   CURSES_DRIVER_MOUSE_BUTTON_PRESSED = THE_DRIVER_MOUSE_BUTTON_PRESSED,
   CURSES_DRIVER_MOUSE_BUTTON_CLICKED = THE_DRIVER_MOUSE_BUTTON_CLICKED,
   CURSES_DRIVER_MOUSE_BUTTON_DOUBLE_CLICKED =
      THE_DRIVER_MOUSE_BUTTON_DOUBLE_CLICKED,
   CURSES_DRIVER_MOUSE_BUTTON_MOVED = THE_DRIVER_MOUSE_BUTTON_MOVED,
   CURSES_DRIVER_MOUSE_WHEEL_SCROLLED =
      THE_DRIVER_MOUSE_WHEEL_SCROLLED
};

typedef TheDriverMouseEvent CursesDriverMouseEvent;
typedef TheDriverWindowRoleSave CursesDriverWindowRoleSave;
typedef TheDriverGlobalWindowRole CursesDriverGlobalWindowRole;

enum
{
   CURSES_DRIVER_GLOBAL_STATAREA = THE_DRIVER_GLOBAL_STATAREA,
   CURSES_DRIVER_GLOBAL_ERROR = THE_DRIVER_GLOBAL_ERROR,
   CURSES_DRIVER_GLOBAL_DIVIDER = THE_DRIVER_GLOBAL_DIVIDER,
   CURSES_DRIVER_GLOBAL_FILETABS = THE_DRIVER_GLOBAL_FILETABS
};

extern const TheDriverOps the_curses_driver_ops;

chtype curses_driver_software_cursor_attr(CHARTYPE scrno, chtype base,
                                          CursorShape shape);
void curses_driver_draw_software_chtype_cell(CHARTYPE scrno, WINDOW *win,
                                             short row, int col, chtype base,
                                             CursorShape shape);
void curses_driver_draw_software_blank_cell(CHARTYPE scrno, WINDOW *win,
                                            short row, int col, chtype base,
                                            CursorShape shape);
#ifdef USE_UTF8
void curses_driver_write_render_wchars_at(WINDOW *win, int row, int col,
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
void curses_driver_clear_current_screen_roles(void);
WINDOW *curses_driver_create_window(int rows, int cols, int row, int col);
void curses_driver_delete_window(WINDOW *win);
void curses_driver_configure_standard_input(bool keypad_enabled,
                                            bool notimeout_enabled);
void curses_driver_set_driver_window_leaveok(TheDriverWindow *win,
                                             bool enabled);
void curses_driver_move_window_cursor(WINDOW *win, short row, short col);
void curses_driver_restore_window_cursor(WINDOW *win,
                                         CursesDriverWindowCursor cursor);
void curses_driver_set_window_attr(WINDOW *win, chtype colour);
void curses_driver_set_current_role_attr(short role, chtype colour);
void curses_driver_set_screen_role_attr(CHARTYPE scrno, short role,
                                        chtype colour);
void curses_driver_set_global_window_attr(CursesDriverGlobalWindowRole role,
                                          chtype colour);
void curses_driver_set_window_background(WINDOW *win, chtype colour);
void curses_driver_clear_window(WINDOW *win);
void curses_driver_clear_window_to_bottom(WINDOW *win);
void curses_driver_clear_to_eol(WINDOW *win);
void curses_driver_clear_current_role_to_eol(short role);
void curses_driver_touch_window(WINDOW *win);
void curses_driver_touch_line(WINDOW *win, int start, int count);
void curses_driver_clear_line_at(WINDOW *win, short row, chtype colour);
void curses_driver_refresh_window(WINDOW *win);
void curses_driver_refresh_window_now(WINDOW *win);
void curses_driver_sync_terminal_screen(void);
void curses_driver_clear_terminal_screen(void);
void curses_driver_begin_terminal_report(void);
void curses_driver_write_terminal_report_text(short row, short col,
                                              TheDriverAttr attr,
                                              const char *text, size_t len);
void curses_driver_end_terminal_report(void);
void curses_driver_update(void);
void curses_driver_present_cursor(bool visible);
void curses_driver_set_window_timeout(WINDOW *win, int milliseconds);
void curses_driver_set_current_window_timeout(int milliseconds);
void curses_driver_draw_box(WINDOW *win);
void curses_driver_draw_vertical_line(WINDOW *win, chtype ch, int len);
void curses_driver_add_string(WINDOW *win, const char *text);
void curses_driver_add_string_at(WINDOW *win, short row, short col,
                                 const char *text);
void curses_driver_add_global_string_at(CursesDriverGlobalWindowRole role,
                                        short row, short col,
                                        const char *text);
void curses_driver_add_chtype_at(WINDOW *win, short row, short col, chtype ch);
void curses_driver_draw_horizontal_line(WINDOW *win, chtype ch, int len);
int curses_driver_read_input_event(TheInputEvent *event);
int curses_driver_read_terminal_legacy_key(void);
void curses_driver_current_mouse_screen_role_position(CHARTYPE scrno,
                                                      short role,
                                                      int *row, int *col);
void curses_driver_current_mouse_global_position(
   CursesDriverGlobalWindowRole role, int *row, int *col);
void curses_driver_current_mouse_screen_position(int *row, int *col);
void curses_driver_clear_mouse_packet_position(void);
int curses_driver_read_pending_mouse_button(int *button, int *action,
                                            int *modifier);
int curses_driver_read_transient_mouse_event(TheDriverWindow *win,
                                             TheDriverMouseEvent *event);
int curses_driver_read_current_role_transient_mouse_event(
   short role, TheDriverMouseEvent *event);
void curses_driver_prepare_for_shell_escape(void);
void curses_driver_repair_terminal_background(
   TheDriverTerminalRepairTarget target);
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
