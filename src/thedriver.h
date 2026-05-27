#ifndef THE_THEDRIVER_H
#define THE_THEDRIVER_H

#include <stdbool.h>
#include <stddef.h>

#include "logcursor.h"
#include "thedefs.h"

#ifndef THE_CURSOR_PRESENTATION_TYPES_DEFINED
#define THE_CURSOR_PRESENTATION_TYPES_DEFINED
typedef enum { CURSOR_BLOCK, CURSOR_UNDERLINE, CURSOR_IBEAM } CursorShape;
typedef enum { CURSOR_STEADY, CURSOR_BLINK } CursorBlink;
typedef enum { CURSOR_PRESENTATION_HARDWARE, CURSOR_PRESENTATION_SOFTWARE } CursorPresentation;
#endif

struct view_details;

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
   void *window;
   int slot_valid;
} TheDriverWindowRoleSave;

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
   int (*clamp_display_col)(int display_col, int window_cols);
   int (*display_col_from_logical)(const CHARTYPE *line, size_t len,
                                   int viewport_col, int logical_col);
   int (*logical_col_from_display)(const CHARTYPE *line, size_t len,
                                   int viewport_col, int display_col,
                                   TextSnap snap);
   int (*viewport_col_for_logical)(const CHARTYPE *line, size_t len,
                                   int current_viewport_col,
                                   int logical_col, int window_cols,
                                   int *display_col, int *visible);
   chtype (*software_cursor_attr)(CHARTYPE scrno, chtype base,
                                  CursorShape shape);
   int (*current_window_is_role)(short role);
   int (*current_window_exists)(void);
   int (*screen_window_is_role)(CHARTYPE scrno, short role);
   int (*current_role_exists)(short role);
   int (*screen_role_exists)(CHARTYPE scrno, short role);
   int (*global_window_exists)(TheDriverGlobalWindowRole role);
   void (*delete_global_window)(TheDriverGlobalWindowRole role);
   TheDriverWindowCursor (*capture_current_window_cursor)(void);
   TheDriverWindowCursor (*capture_current_previous_window_cursor)(void);
   TheDriverWindowCursor (*capture_current_role_cursor)(short role);
   TheDriverWindowCursor (*capture_screen_window_cursor)(CHARTYPE scrno);
   TheDriverWindowCursor (*capture_screen_role_cursor)(CHARTYPE scrno,
                                                       short role);
   TheDriverWindowOrigin (*current_window_origin)(void);
   TheDriverWindowSize (*current_window_size)(void);
   TheDriverWindowSize (*current_role_size)(short role);
   TheDriverWindowSize (*screen_role_size)(CHARTYPE scrno, short role);
   TheDriverWindowRoleSave (*save_current_role_window)(short role);
   void (*restore_current_role_window)(short role,
                                       TheDriverWindowRoleSave saved);
   void (*delete_current_role_window)(short role);
   void (*clear_current_screen_roles)(void);
   void (*move_current_window_cursor)(short row, short col);
   void (*move_current_previous_window_cursor)(short row, short col);
   void (*move_current_role_cursor)(short role, short row, short col);
   void (*move_screen_window_cursor)(CHARTYPE scrno, short row, short col);
   void (*move_screen_role_cursor)(CHARTYPE scrno, short role, short row,
                                   short col);
   void (*restore_current_window_cursor)(TheDriverWindowCursor cursor);
   void (*restore_current_role_cursor)(short role,
                                       TheDriverWindowCursor cursor);
   chtype (*read_current_window_cell)(void);
   chtype (*read_current_window_cell_attr_at)(short row, short col);
   void (*put_char_current_window)(chtype ch, CHARTYPE add_ins);
   void (*set_current_window_attr)(chtype colour);
   void (*set_current_role_attr)(short role, chtype colour);
   void (*set_screen_role_attr)(CHARTYPE scrno, short role, chtype colour);
   void (*set_global_window_attr)(TheDriverGlobalWindowRole role,
                                  chtype colour);
   void (*clear_current_role)(short role);
   void (*clear_current_role_to_eol)(short role);
   void (*clear_screen_role_to_eol)(CHARTYPE scrno, short role);
   void (*touch_current_window)(void);
   void (*touch_current_role)(short role);
   void (*touch_screen_role)(CHARTYPE scrno, short role);
   void (*touch_global_window)(TheDriverGlobalWindowRole role);
   void (*touch_and_refresh_current_role)(short role);
   void (*touch_and_refresh_screen_role)(CHARTYPE scrno, short role);
   void (*touch_and_refresh_global_window)(TheDriverGlobalWindowRole role);
   void (*refresh_current_window)(void);
   void (*refresh_current_window_now)(void);
   void (*refresh_current_role)(short role);
   void (*refresh_current_role_now)(short role);
   void (*refresh_screen_window)(CHARTYPE scrno);
   void (*refresh_screen_role)(CHARTYPE scrno, short role);
   void (*refresh_global_window)(TheDriverGlobalWindowRole role);
   void (*refresh_global_window_now)(TheDriverGlobalWindowRole role);
   void (*refresh_standard_screen)(void);
   void (*update)(void);
   void (*present_cursor)(bool visible);
   void (*add_global_string_at)(TheDriverGlobalWindowRole role, short row,
                                short col, const char *text);
   int (*read_current_window_key)(void);
   int (*read_standard_key)(void);
   int (*read_raw_standard_key)(void);
   int (*is_mouse_key)(int key);
   int (*mouse_key_code)(void);
   void (*mouse_position_for_screen_role)(CHARTYPE scrno, short role,
                                          int *row, int *col);
   void (*mouse_position_for_global)(TheDriverGlobalWindowRole role,
                                     int *row, int *col);
   void (*saved_mouse_position)(int *row, int *col);
   void (*reset_mouse_position)(void);
   int (*read_mouse_button)(int *button, int *action, int *modifier);
   void (*prepare_standard_screen_for_shell)(void);
   void (*clear_standard_window)(void);
   void (*erase_standard_window)(void);
   void (*set_standard_attr)(chtype colour);
   void (*add_standard_string_at)(short row, short col, const char *text);
   void (*move_standard_cursor)(short row, short col);
   void (*add_standard_ch)(chtype ch);
   void (*redraw_current_role)(short role);
   void (*redraw_screen_role)(CHARTYPE scrno, short role);
   void (*redraw_global_window)(TheDriverGlobalWindowRole role);
   short (*refresh_cursor)(CHARTYPE scrno);
   short (*redraw_screen_cursor)(CHARTYPE scrno, struct view_details *view);
   void (*move_prefix_cursor)(CHARTYPE scrno, short row, short col);
   TheDriverCursorTarget (*filearea_target)(LogicalCursor cursor,
                                            const CHARTYPE *line, size_t len,
                                            int viewport_col,
                                            int window_cols);
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

#endif
