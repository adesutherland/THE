#ifndef THE_THEDRIVER_H
#define THE_THEDRIVER_H

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include <wchar.h>

#include "inputevent.h"
#include "logcursor.h"
#include "rendercell.h"
#include "thedefs.h"

#ifndef THE_CURSOR_PRESENTATION_TYPES_DEFINED
#define THE_CURSOR_PRESENTATION_TYPES_DEFINED
typedef enum { CURSOR_BLOCK, CURSOR_UNDERLINE, CURSOR_IBEAM } CursorShape;
typedef enum { CURSOR_STEADY, CURSOR_BLINK } CursorBlink;
typedef enum { CURSOR_PRESENTATION_HARDWARE, CURSOR_PRESENTATION_SOFTWARE } CursorPresentation;
#endif

struct view_details;
typedef struct TheDriverWindow TheDriverWindow;

typedef TheRenderAttr TheDriverAttr;
typedef uint64_t TheDriverCell;

typedef struct
{
   LogicalCursor logical;
   int viewport_col;
   int raw_display_col;
   int display_col;
   int window_cols;
   int visible;
} TheDriverCursorTarget;

typedef struct
{
   short row;
   short col;
   int valid;
} TheDriverWindowCursor;

typedef struct
{
   short row;
   short col;
   int valid;
} TheDriverWindowOrigin;

typedef struct
{
   short rows;
   short cols;
   int valid;
} TheDriverWindowSize;

typedef struct
{
   short row;
   short col;
   int valid;
} TheDriverScreenPoint;

typedef enum
{
   THE_DRIVER_MOUSE_ACTION_NONE = 0,
   THE_DRIVER_MOUSE_ACTION_PRESSED,
   THE_DRIVER_MOUSE_ACTION_RELEASED,
   THE_DRIVER_MOUSE_ACTION_CLICKED,
   THE_DRIVER_MOUSE_ACTION_OTHER
} TheDriverMouseAction;

enum
{
   THE_DRIVER_MOUSE_MODIFIER_NONE = 0,
   THE_DRIVER_MOUSE_MODIFIER_SHIFT = 0010,
   THE_DRIVER_MOUSE_MODIFIER_CONTROL = 0020,
   THE_DRIVER_MOUSE_MODIFIER_ALT = 0040
};

enum
{
   THE_DRIVER_MOUSE_BUTTON_RELEASED = 0,
   THE_DRIVER_MOUSE_BUTTON_PRESSED = 1,
   THE_DRIVER_MOUSE_BUTTON_CLICKED = 2,
   THE_DRIVER_MOUSE_BUTTON_DOUBLE_CLICKED = 3,
   THE_DRIVER_MOUSE_BUTTON_MOVED = 5,
   THE_DRIVER_MOUSE_WHEEL_SCROLLED = 6
};

typedef struct
{
   int button;
   TheDriverMouseAction action;
   int modifier;
   int row;
   int col;
   int inside;
   int valid;
} TheDriverMouseEvent;

typedef struct
{
   TheDriverWindow *window;
   int slot_valid;
} TheDriverWindowRoleSave;

typedef enum
{
   THE_DRIVER_REPAIR_ACTIVE_SURFACE = 0,
   THE_DRIVER_REPAIR_TERMINAL_SCREEN
} TheDriverTerminalRepairTarget;

typedef enum
{
   THE_DRIVER_GLOBAL_STATAREA = 0,
   THE_DRIVER_GLOBAL_ERROR,
   THE_DRIVER_GLOBAL_DIVIDER,
   THE_DRIVER_GLOBAL_FILETABS
} TheDriverGlobalWindowRole;

typedef struct TheDriverOps TheDriverOps;

struct TheDriverOps
{
   TheDriverAttr (*software_cursor_attr)(CHARTYPE scrno, TheDriverAttr base,
                                         CursorShape shape);
   TheDriverWindow *(*create_window)(int rows, int cols, int row, int col);
   void (*delete_window)(TheDriverWindow *win);
   TheDriverWindowCursor (*capture_window_cursor)(TheDriverWindow *win);
   TheDriverWindowOrigin (*window_origin)(TheDriverWindow *win);
   TheDriverWindowSize (*window_size)(TheDriverWindow *win);
   void (*clear_current_screen_roles)(void);
   void (*move_window_cursor)(TheDriverWindow *win, short row, short col);
   void (*restore_window_cursor)(TheDriverWindow *win,
                                 TheDriverWindowCursor cursor);
   void (*set_window_attr)(TheDriverWindow *win, TheDriverAttr colour);
   void (*set_current_role_attr)(short role, TheDriverAttr colour);
   void (*set_screen_role_attr)(CHARTYPE scrno, short role,
                                TheDriverAttr colour);
   void (*set_global_window_attr)(TheDriverGlobalWindowRole role,
                                  TheDriverAttr colour);
   void (*set_window_background)(TheDriverWindow *win, TheDriverAttr colour);
   void (*clear_line_at)(TheDriverWindow *win, short row,
                         TheDriverAttr colour);
   void (*clear_current_role_to_eol)(short role);
   void (*touch_window)(TheDriverWindow *win);
   void (*touch_line)(TheDriverWindow *win, int start, int count);
   void (*refresh_window)(TheDriverWindow *win);
   void (*refresh_window_now)(TheDriverWindow *win);
   void (*sync_terminal_screen)(void);
   void (*clear_terminal_screen)(void);
   void (*begin_terminal_report)(void);
   void (*write_terminal_report_text)(short row, short col,
                                      TheDriverAttr attr,
                                      const char *text, size_t len);
   void (*end_terminal_report)(void);
   void (*update)(void);
   void (*present_cursor)(bool visible);
   void (*set_current_window_timeout)(int milliseconds);
   void (*draw_box)(TheDriverWindow *win);
   void (*draw_vertical_line)(TheDriverWindow *win, TheDriverCell ch,
                              int len);
   void (*add_string_at)(TheDriverWindow *win, short row, short col,
                         const char *text);
   void (*add_global_string_at)(TheDriverGlobalWindowRole role, short row,
                                short col, const char *text);
   void (*add_cell_at)(TheDriverWindow *win, short row, short col,
                       TheDriverCell ch);
   void (*draw_horizontal_line)(TheDriverWindow *win, TheDriverCell ch,
                                int len);
   void (*add_cell)(TheDriverWindow *win, TheDriverCell ch);
   void (*insert_cell)(TheDriverWindow *win, TheDriverCell ch);
   void (*delete_cell)(TheDriverWindow *win);
   void (*write_cell_span)(TheDriverWindow *win, const TheDriverCell *text,
                           int len);
   void (*write_render_cells)(TheDriverWindow *win,
                              const TheRenderCell *text, int len);
   void (*write_render_cluster_at)(TheDriverWindow *win, int row, int col,
                                   const TheRenderCluster *cluster);
   void (*fill_cells_at)(TheDriverWindow *win, int row, int col, int width,
                         TheDriverAttr colour);
   void (*write_ascii_cells_at)(TheDriverWindow *win, int row, int col,
                                const char *text, int width,
                                TheDriverAttr colour);
   int (*read_input_event)(TheInputEvent *event);
   void (*prepare_for_shell_escape)(void);
   void (*repair_terminal_background)(
      TheDriverTerminalRepairTarget target);
   void (*redraw_window)(TheDriverWindow *win);
   void (*draw_software_cell)(CHARTYPE scrno, TheDriverWindow *win,
                              short row, int col, TheDriverCell base,
                              CursorShape shape);
   void (*draw_software_blank_cell)(CHARTYPE scrno, TheDriverWindow *win,
                                    short row, int col, TheDriverAttr base,
                                    CursorShape shape);
   short (*refresh_cursor)(CHARTYPE scrno);
   short (*redraw_screen_cursor)(CHARTYPE scrno, struct view_details *view);
   void (*move_prefix_cursor)(CHARTYPE scrno, short row, short col);
   short (*move_filearea_cursor)(CHARTYPE scrno, struct view_details *view,
                                 const CHARTYPE *line, size_t len,
                                 short row, int logical_col);
   short (*filearea_cursor_transition)(CHARTYPE scrno,
                                       struct view_details *view,
                                       const CHARTYPE *line, size_t len,
                                       short old_row, int old_logical_cell,
                                       int new_logical_cell,
                                       LENGTHTYPE old_verify_col);
};

extern const TheDriverOps *the_driver;

void the_driver_select(const TheDriverOps *ops);
int the_driver_use_curses(void);
int the_driver_use_headless(void);
int the_driver_read_legacy_key(void);

#endif
