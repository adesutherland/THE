#include "headlessdriver.h"
#include "driverlayout.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum
{
   HEADLESS_MAX_SCREENS = 2,
   HEADLESS_VIEW_WINDOWS = 6,
   HEADLESS_ROLE_FILEAREA = 0,
   HEADLESS_ROLE_PREFIX = 1,
   HEADLESS_GLOBAL_WINDOWS = 4
};

struct TheDriverWindow
{
   int id;
   int rows;
   int cols;
   int origin_row;
   int origin_col;
   int cursor_row;
   int cursor_col;
   int is_pad;
   int dirty;
   int refreshed;
   int immediate_refreshes;
   int keypad_enabled;
   int leaveok_enabled;
   int timeout_ms;
   TheDriverAttr attr;
   TheDriverAttr background;
   TheDriverCell *cells;
   TheDriverAttr *attrs;
   TheRenderCell *render_cells;
   struct TheDriverWindow *next;
};

typedef struct
{
   TheDriverWindow *roles[HEADLESS_MAX_SCREENS][HEADLESS_VIEW_WINDOWS];
   TheDriverWindow *globals[HEADLESS_GLOBAL_WINDOWS];
   TheDriverWindow *standard;
   TheDriverWindow *windows;
   short current_role[HEADLESS_MAX_SCREENS];
   short previous_role[HEADLESS_MAX_SCREENS];
   CHARTYPE current_screen;
   int next_window_id;
   int standard_keypad_enabled;
   int standard_notimeout_enabled;
   int cursor_visible;
   int terminal_report_writes;
   TheInputQueue input_queue;
   char log[HEADLESS_DRIVER_OP_LOG_CAPACITY][96];
   size_t log_count;
} HeadlessDriverState;

static HeadlessDriverState headless_state;

static int headless_valid_screen(CHARTYPE scrno)
{
   return scrno < HEADLESS_MAX_SCREENS;
}

static int headless_valid_role(short role)
{
   return role >= 0 && role < HEADLESS_VIEW_WINDOWS;
}

static int headless_valid_global(TheDriverGlobalWindowRole role)
{
   return role >= THE_DRIVER_GLOBAL_STATAREA
       && role <= THE_DRIVER_GLOBAL_FILETABS;
}

static int headless_nonnegative(int value)
{
   return value < 0 ? 0 : value;
}

static void headless_log(const char *fmt, ...)
{
   va_list args;

   if (headless_state.log_count >= HEADLESS_DRIVER_OP_LOG_CAPACITY)
      return;
   va_start(args, fmt);
   vsnprintf(headless_state.log[headless_state.log_count],
             sizeof(headless_state.log[headless_state.log_count]), fmt, args);
   va_end(args);
   headless_state.log_count++;
}

static size_t headless_cell_index(const TheDriverWindow *win, int row,
                                  int col)
{
   return (size_t)row * (size_t)win->cols + (size_t)col;
}

static int headless_window_contains(const TheDriverWindow *win, int row,
                                    int col)
{
   return win != NULL
       && row >= 0
       && col >= 0
       && row < win->rows
       && col < win->cols;
}

static void headless_clamp_cursor(TheDriverWindow *win)
{
   if (win == NULL)
      return;
   if (win->rows <= 0)
      win->cursor_row = 0;
   else if (win->cursor_row < 0)
      win->cursor_row = 0;
   else if (win->cursor_row >= win->rows)
      win->cursor_row = win->rows - 1;
   if (win->cols <= 0)
      win->cursor_col = 0;
   else if (win->cursor_col < 0)
      win->cursor_col = 0;
   else if (win->cursor_col >= win->cols)
      win->cursor_col = win->cols - 1;
}

static void headless_move_cursor(TheDriverWindow *win, short row, short col)
{
   if (win == NULL)
      return;
   win->cursor_row = row;
   win->cursor_col = col;
   headless_clamp_cursor(win);
   headless_log("cursor:window:%d:%d:%d", win->id, win->cursor_row,
                win->cursor_col);
}

static void headless_restore_cursor(TheDriverWindow *win,
                                    TheDriverWindowCursor cursor)
{
   if (win == NULL || !cursor.valid)
      return;
   headless_move_cursor(win, cursor.row, cursor.col);
}

static TheDriverWindowCursor headless_capture_cursor(TheDriverWindow *win)
{
   TheDriverWindowCursor cursor;

   cursor.row = 0;
   cursor.col = 0;
   cursor.valid = 0;
   if (win == NULL)
      return cursor;
   cursor.row = (short)win->cursor_row;
   cursor.col = (short)win->cursor_col;
   cursor.valid = 1;
   return cursor;
}

static TheDriverWindowOrigin headless_capture_origin(TheDriverWindow *win)
{
   TheDriverWindowOrigin origin;

   origin.row = 0;
   origin.col = 0;
   origin.valid = 0;
   if (win == NULL)
      return origin;
   origin.row = (short)win->origin_row;
   origin.col = (short)win->origin_col;
   origin.valid = 1;
   return origin;
}

static TheDriverWindowSize headless_capture_size(TheDriverWindow *win)
{
   TheDriverWindowSize size;

   size.rows = 0;
   size.cols = 0;
   size.valid = 0;
   if (win == NULL)
      return size;
   size.rows = (short)win->rows;
   size.cols = (short)win->cols;
   size.valid = 1;
   return size;
}

static TheDriverWindow *headless_create_window_internal(int rows, int cols,
                                                       int row, int col,
                                                       int is_pad)
{
   TheDriverWindow *win;
   size_t total;

   win = (TheDriverWindow *)calloc(1, sizeof(*win));
   if (win == NULL)
      return NULL;
   win->id = ++headless_state.next_window_id;
   win->rows = headless_nonnegative(rows);
   win->cols = headless_nonnegative(cols);
   win->origin_row = headless_nonnegative(row);
   win->origin_col = headless_nonnegative(col);
   win->is_pad = is_pad;
   total = (size_t)win->rows * (size_t)win->cols;
   if (total > 0)
   {
      win->cells = (TheDriverCell *)calloc(total, sizeof(*win->cells));
      win->attrs = (TheDriverAttr *)calloc(total, sizeof(*win->attrs));
      win->render_cells =
         (TheRenderCell *)calloc(total, sizeof(*win->render_cells));
      if (win->cells == NULL || win->attrs == NULL
      ||  win->render_cells == NULL)
      {
         free(win->cells);
         free(win->attrs);
         free(win->render_cells);
         free(win);
         return NULL;
      }
   }
   win->next = headless_state.windows;
   headless_state.windows = win;
   return win;
}

static void headless_forget_window(TheDriverWindow *win)
{
   int scrno;
   int role;
   int global;

   if (win == NULL)
      return;
   for (scrno = 0; scrno < HEADLESS_MAX_SCREENS; scrno++)
   {
      for (role = 0; role < HEADLESS_VIEW_WINDOWS; role++)
      {
         if (headless_state.roles[scrno][role] == win)
            headless_state.roles[scrno][role] = NULL;
      }
   }
   for (global = 0; global < HEADLESS_GLOBAL_WINDOWS; global++)
   {
      if (headless_state.globals[global] == win)
         headless_state.globals[global] = NULL;
   }
   if (headless_state.standard == win)
      headless_state.standard = NULL;
}

static void headless_unlink_window(TheDriverWindow *win)
{
   TheDriverWindow **slot;

   slot = &headless_state.windows;
   while (*slot != NULL)
   {
      if (*slot == win)
      {
         *slot = win->next;
         return;
      }
      slot = &(*slot)->next;
   }
}

static void headless_free_window(TheDriverWindow *win)
{
   if (win == NULL)
      return;
   headless_forget_window(win);
   headless_unlink_window(win);
   free(win->cells);
   free(win->attrs);
   free(win->render_cells);
   free(win);
}

static TheDriverWindow *headless_screen_role_window(CHARTYPE scrno,
                                                    short role)
{
   if (!headless_valid_screen(scrno) || !headless_valid_role(role))
      return NULL;
   return headless_state.roles[scrno][role];
}

static TheDriverWindow *headless_current_role_window(short role)
{
   return headless_screen_role_window(headless_state.current_screen, role);
}

static TheDriverWindow *headless_screen_active_window(CHARTYPE scrno)
{
   if (!headless_valid_screen(scrno))
      return NULL;
   return headless_screen_role_window(scrno, headless_state.current_role[scrno]);
}

static TheDriverWindow *headless_screen_previous_window(CHARTYPE scrno)
{
   if (!headless_valid_screen(scrno))
      return NULL;
   return headless_screen_role_window(scrno,
                                      headless_state.previous_role[scrno]);
}

static TheDriverWindow *headless_current_active_window(void)
{
   return headless_screen_active_window(headless_state.current_screen);
}

static TheDriverWindow *headless_current_previous_window(void)
{
   return headless_screen_previous_window(headless_state.current_screen);
}

static TheDriverWindow *headless_global_window(TheDriverGlobalWindowRole role)
{
   if (!headless_valid_global(role))
      return NULL;
   return headless_state.globals[role];
}

static TheDriverWindow *headless_standard_window(void)
{
   if (headless_state.standard == NULL)
      headless_state.standard = headless_create_window_internal(24, 80, 0, 0,
                                                                0);
   return headless_state.standard;
}

static void headless_set_cell_at(TheDriverWindow *win, int row, int col,
                                 TheDriverCell cell)
{
   size_t index;

   if (!headless_window_contains(win, row, col) || win->cells == NULL)
      return;
   index = headless_cell_index(win, row, col);
   win->cells[index] = cell;
   win->attrs[index] = win->attr;
   if (win->render_cells != NULL)
      the_render_cell_from_codepoint(&win->render_cells[index],
                                     (uint32_t)cell,
                                     (TheRenderAttr)win->attr);
   win->dirty = 1;
}

static void headless_set_render_cell_at(TheDriverWindow *win, int row,
                                        int col, const TheRenderCell *cell)
{
   size_t index;
   TheDriverCell stored = '?';

   if (!headless_window_contains(win, row, col) || cell == NULL
   ||  win->cells == NULL)
      return;
   index = headless_cell_index(win, row, col);
   if (cell->codepoint_count > 0)
      stored = (TheDriverCell)cell->codepoints[0];
   else if (cell->flags & THE_RENDER_CLUSTER_HAS_FALLBACK)
      stored = (TheDriverCell)cell->fallback_codepoint;
   win->cells[index] = stored;
   win->attrs[index] = (TheDriverAttr)cell->attr;
   if (win->render_cells != NULL)
      win->render_cells[index] = *cell;
   win->dirty = 1;
}

static TheDriverCell headless_cell_at(const TheDriverWindow *win, int row,
                                      int col)
{
   if (!headless_window_contains(win, row, col) || win->cells == NULL)
      return 0;
   return win->cells[headless_cell_index(win, row, col)];
}

static void headless_add_cell_internal(TheDriverWindow *win,
                                       TheDriverCell cell)
{
   if (win == NULL)
      return;
   headless_set_cell_at(win, win->cursor_row, win->cursor_col, cell);
   if (win->cols > 0)
   {
      win->cursor_col++;
      if (win->cursor_col >= win->cols)
      {
         win->cursor_col = 0;
         if (win->rows > 0 && win->cursor_row < win->rows - 1)
            win->cursor_row++;
      }
   }
}

static void headless_add_render_cell_internal(TheDriverWindow *win,
                                              const TheRenderCell *cell)
{
   int advance;

   if (win == NULL || cell == NULL)
      return;
   headless_set_render_cell_at(win, win->cursor_row, win->cursor_col, cell);
   advance = cell->display_width > 0 ? cell->display_width : 1;
   if (win->cols > 0)
   {
      win->cursor_col += advance;
      while (win->cursor_col >= win->cols)
      {
         win->cursor_col -= win->cols;
         if (win->rows > 0 && win->cursor_row < win->rows - 1)
            win->cursor_row++;
         else
         {
            win->cursor_col = win->cols - 1;
            break;
         }
      }
   }
}

static void headless_write_string_at(TheDriverWindow *win, int row, int col,
                                     const char *text)
{
   int i;

   if (win == NULL || text == NULL)
      return;
   for (i = 0; text[i] != '\0'; i++)
      headless_set_cell_at(win, row, col + i, (unsigned char)text[i]);
}

static void headless_clear_window(TheDriverWindow *win)
{
   size_t total;

   if (win == NULL || win->cells == NULL)
      return;
   total = (size_t)win->rows * (size_t)win->cols;
   memset(win->cells, 0, total * sizeof(*win->cells));
   memset(win->attrs, 0, total * sizeof(*win->attrs));
   if (win->render_cells != NULL)
      memset(win->render_cells, 0, total * sizeof(*win->render_cells));
   win->dirty = 1;
}

static void headless_clear_to_eol(TheDriverWindow *win)
{
   int col;

   if (win == NULL)
      return;
   for (col = win->cursor_col; col < win->cols; col++)
      headless_set_cell_at(win, win->cursor_row, col, ' ');
}

void headless_driver_reset(void)
{
   TheDriverWindow *win;

   win = headless_state.windows;
   while (win != NULL)
   {
      TheDriverWindow *next = win->next;

      free(win->cells);
      free(win->attrs);
      free(win->render_cells);
      free(win);
      win = next;
   }
   memset(&headless_state, 0, sizeof(headless_state));
   the_input_queue_init(&headless_state.input_queue);
   headless_state.cursor_visible = 1;
}

void headless_driver_clear_log(void)
{
   headless_state.log_count = 0;
}

size_t headless_driver_log_count(void)
{
   return headless_state.log_count;
}

const char *headless_driver_log_entry(size_t index)
{
   if (index >= headless_state.log_count)
      return NULL;
   return headless_state.log[index];
}

void headless_driver_set_current_screen(CHARTYPE scrno)
{
   if (headless_valid_screen(scrno))
      headless_state.current_screen = scrno;
}

void headless_driver_set_screen_current_role(CHARTYPE scrno, short role)
{
   if (headless_valid_screen(scrno) && headless_valid_role(role))
      headless_state.current_role[scrno] = role;
}

void headless_driver_set_screen_previous_role(CHARTYPE scrno, short role)
{
   if (headless_valid_screen(scrno) && headless_valid_role(role))
      headless_state.previous_role[scrno] = role;
}

TheDriverWindow *headless_driver_create_screen_role(CHARTYPE scrno,
                                                    short role, int rows,
                                                    int cols, int row,
                                                    int col)
{
   TheDriverWindow *win;

   if (!headless_valid_screen(scrno) || !headless_valid_role(role))
      return NULL;
   win = headless_create_window_internal(rows, cols, row, col, 0);
   if (win != NULL)
      headless_state.roles[scrno][role] = win;
   return win;
}

TheDriverWindow *headless_driver_create_global_window(
   TheDriverGlobalWindowRole role, int rows, int cols, int row, int col)
{
   TheDriverWindow *win;

   if (!headless_valid_global(role))
      return NULL;
   win = headless_create_window_internal(rows, cols, row, col, 0);
   if (win != NULL)
   headless_state.globals[role] = win;
   return win;
}

int headless_driver_render_cell_at(TheDriverWindow *win, int row, int col,
                                   TheRenderCell *out)
{
   size_t index;

   if (out != NULL)
      memset(out, 0, sizeof(*out));
   if (!headless_window_contains(win, row, col)
   ||  win->render_cells == NULL)
      return 0;
   index = headless_cell_index(win, row, col);
   if (out != NULL)
      *out = win->render_cells[index];
   return (win->render_cells[index].flags & THE_RENDER_CLUSTER_VALID) != 0;
}

void headless_driver_queue_key(int key)
{
   TheInputEvent event;

   if (!the_input_event_from_legacy_key(key, &event))
      return;
   (void)the_input_queue_push(&headless_state.input_queue, event);
}

int headless_driver_queue_input_event(TheInputEvent event)
{
   return the_input_queue_push(&headless_state.input_queue, event);
}

static TheDriverAttr headless_driver_software_cursor_attr(
   CHARTYPE scrno, TheDriverAttr base, CursorShape shape)
{
   (void)scrno;
   return base ^ ((TheDriverAttr)shape + 1U);
}

static int headless_driver_current_window_is_role(short role)
{
   return headless_valid_role(role)
       && role == headless_state.current_role[headless_state.current_screen]
       && headless_current_active_window() != NULL;
}

static int headless_driver_current_window_exists(void)
{
   return headless_current_active_window() != NULL;
}

static int headless_driver_screen_window_is_role(CHARTYPE scrno, short role)
{
   return headless_valid_screen(scrno)
       && headless_valid_role(role)
       && role == headless_state.current_role[scrno]
       && headless_screen_active_window(scrno) != NULL;
}

static int headless_driver_current_role_exists(short role)
{
   return headless_current_role_window(role) != NULL;
}

static int headless_driver_screen_role_exists(CHARTYPE scrno, short role)
{
   return headless_screen_role_window(scrno, role) != NULL;
}

static int headless_driver_global_window_exists(
   TheDriverGlobalWindowRole role)
{
   return headless_global_window(role) != NULL;
}

static void headless_driver_delete_global_window(
   TheDriverGlobalWindowRole role)
{
   TheDriverWindow *win;

   win = headless_global_window(role);
   if (win == NULL)
      return;
   headless_log("delete:global:%d", (int)role);
   headless_free_window(win);
}

static TheDriverWindow *headless_driver_create_window(int rows, int cols,
                                                      int row, int col)
{
   return headless_create_window_internal(rows, cols, row, col, 0);
}

static void headless_driver_delete_window(TheDriverWindow *win)
{
   headless_free_window(win);
}

static void headless_driver_enable_keypad(TheDriverWindow *win, bool enabled)
{
   if (win != NULL)
      win->keypad_enabled = enabled ? 1 : 0;
}

static void headless_driver_enable_standard_keypad(bool enabled)
{
   headless_state.standard_keypad_enabled = enabled ? 1 : 0;
}

static void headless_driver_set_standard_notimeout(bool enabled)
{
   headless_state.standard_notimeout_enabled = enabled ? 1 : 0;
}

static void headless_driver_set_window_leaveok(TheDriverWindow *win,
                                               bool enabled)
{
   if (win != NULL)
      win->leaveok_enabled = enabled ? 1 : 0;
}

static TheDriverWindowCursor headless_driver_capture_window_cursor(
   TheDriverWindow *win)
{
   return headless_capture_cursor(win);
}

static TheDriverWindowCursor headless_driver_capture_current_window_cursor(
   void)
{
   return headless_capture_cursor(headless_current_active_window());
}

static TheDriverWindowCursor
headless_driver_capture_current_previous_window_cursor(void)
{
   return headless_capture_cursor(headless_current_previous_window());
}

static TheDriverWindowCursor headless_driver_capture_current_role_cursor(
   short role)
{
   return headless_capture_cursor(headless_current_role_window(role));
}

static TheDriverWindowCursor headless_driver_capture_screen_window_cursor(
   CHARTYPE scrno)
{
   return headless_capture_cursor(headless_screen_active_window(scrno));
}

static TheDriverWindowCursor headless_driver_capture_screen_role_cursor(
   CHARTYPE scrno, short role)
{
   return headless_capture_cursor(headless_screen_role_window(scrno, role));
}

static TheDriverWindowCursor headless_driver_capture_global_window_cursor(
   TheDriverGlobalWindowRole role)
{
   return headless_capture_cursor(headless_global_window(role));
}

static TheDriverWindowOrigin headless_driver_window_origin(
   TheDriverWindow *win)
{
   return headless_capture_origin(win);
}

static TheDriverWindowSize headless_driver_window_size(TheDriverWindow *win)
{
   return headless_capture_size(win);
}

static TheDriverWindowOrigin headless_driver_current_window_origin(void)
{
   return headless_capture_origin(headless_current_active_window());
}

static TheDriverWindowSize headless_driver_current_window_size(void)
{
   return headless_capture_size(headless_current_active_window());
}

static TheDriverWindowSize headless_driver_current_role_size(short role)
{
   return headless_capture_size(headless_current_role_window(role));
}

static TheDriverWindowSize headless_driver_screen_role_size(CHARTYPE scrno,
                                                            short role)
{
   return headless_capture_size(headless_screen_role_window(scrno, role));
}

static TheDriverScreenPoint
headless_driver_current_window_cursor_screen_point(void)
{
   TheDriverScreenPoint point;
   TheDriverWindow *win;

   point.row = 0;
   point.col = 0;
   point.valid = 0;
   win = headless_current_active_window();
   if (win == NULL)
      return point;
   point.row = (short)(win->origin_row + win->cursor_row);
   point.col = (short)(win->origin_col + win->cursor_col);
   point.valid = 1;
   return point;
}

static TheDriverWindowRoleSave headless_driver_save_current_role_window(
   short role)
{
   TheDriverWindowRoleSave saved;

   saved.window = NULL;
   saved.slot_valid = 0;
   if (!headless_valid_role(role)
   ||  !headless_valid_screen(headless_state.current_screen))
      return saved;
   saved.window = headless_state.roles[headless_state.current_screen][role];
   saved.slot_valid = 1;
   return saved;
}

static void headless_driver_restore_current_role_window(
   short role, TheDriverWindowRoleSave saved)
{
   if (!saved.slot_valid
   ||  !headless_valid_screen(headless_state.current_screen)
   ||  !headless_valid_role(role))
      return;
   headless_state.roles[headless_state.current_screen][role] = saved.window;
}

static void headless_driver_delete_current_role_window(short role)
{
   TheDriverWindow *win;

   win = headless_current_role_window(role);
   if (win != NULL)
      headless_free_window(win);
}

static void headless_driver_clear_current_screen_roles(void)
{
   int role;

   if (!headless_valid_screen(headless_state.current_screen))
      return;
   for (role = 0; role < HEADLESS_VIEW_WINDOWS; role++)
      headless_state.roles[headless_state.current_screen][role] = NULL;
}

static void headless_driver_move_window_cursor(TheDriverWindow *win,
                                               short row, short col)
{
   headless_move_cursor(win, row, col);
}

static void headless_driver_move_current_window_cursor(short row, short col)
{
   headless_move_cursor(headless_current_active_window(), row, col);
}

static void headless_driver_move_current_previous_window_cursor(short row,
                                                                short col)
{
   headless_move_cursor(headless_current_previous_window(), row, col);
}

static void headless_driver_move_current_role_cursor(short role, short row,
                                                     short col)
{
   headless_move_cursor(headless_current_role_window(role), row, col);
}

static void headless_driver_move_screen_window_cursor(CHARTYPE scrno,
                                                      short row, short col)
{
   headless_move_cursor(headless_screen_active_window(scrno), row, col);
}

static void headless_driver_move_screen_role_cursor(CHARTYPE scrno,
                                                    short role, short row,
                                                    short col)
{
   headless_move_cursor(headless_screen_role_window(scrno, role), row, col);
}

static void headless_driver_move_global_window_cursor(
   TheDriverGlobalWindowRole role, short row, short col)
{
   headless_move_cursor(headless_global_window(role), row, col);
}

static void headless_driver_restore_window_cursor(
   TheDriverWindow *win, TheDriverWindowCursor cursor)
{
   headless_restore_cursor(win, cursor);
}

static void headless_driver_restore_current_window_cursor(
   TheDriverWindowCursor cursor)
{
   headless_restore_cursor(headless_current_active_window(), cursor);
}

static void headless_driver_restore_current_role_cursor(
   short role, TheDriverWindowCursor cursor)
{
   headless_restore_cursor(headless_current_role_window(role), cursor);
}

static void headless_driver_restore_screen_window_cursor(
   CHARTYPE scrno, TheDriverWindowCursor cursor)
{
   headless_restore_cursor(headless_screen_active_window(scrno), cursor);
}

static void headless_driver_restore_screen_role_cursor(
   CHARTYPE scrno, short role, TheDriverWindowCursor cursor)
{
   headless_restore_cursor(headless_screen_role_window(scrno, role), cursor);
}

static void headless_driver_restore_global_window_cursor(
   TheDriverGlobalWindowRole role, TheDriverWindowCursor cursor)
{
   headless_restore_cursor(headless_global_window(role), cursor);
}

static TheDriverCell headless_driver_read_window_cell(TheDriverWindow *win)
{
   if (win == NULL)
      return 0;
   return headless_cell_at(win, win->cursor_row, win->cursor_col);
}

static TheDriverCell headless_driver_read_current_window_cell(void)
{
   return headless_driver_read_window_cell(headless_current_active_window());
}

static TheDriverAttr headless_driver_read_current_window_cell_attr_at(
   short row, short col)
{
   TheDriverWindow *win;

   win = headless_current_active_window();
   if (!headless_window_contains(win, row, col) || win->attrs == NULL)
      return 0;
   return win->attrs[headless_cell_index(win, row, col)];
}

static void headless_driver_put_char_current_window(TheDriverCell ch,
                                                    CHARTYPE add_ins)
{
   TheDriverWindow *win;

   (void)add_ins;
   win = headless_current_active_window();
   headless_add_cell_internal(win, ch);
}

static void headless_driver_set_window_attr(TheDriverWindow *win,
                                            TheDriverAttr colour)
{
   if (win != NULL)
      win->attr = colour;
}

static void headless_driver_set_current_window_attr(TheDriverAttr colour)
{
   headless_driver_set_window_attr(headless_current_active_window(), colour);
}

static void headless_driver_set_current_role_attr(short role,
                                                  TheDriverAttr colour)
{
   headless_driver_set_window_attr(headless_current_role_window(role), colour);
}

static void headless_driver_set_screen_role_attr(CHARTYPE scrno, short role,
                                                 TheDriverAttr colour)
{
   headless_driver_set_window_attr(headless_screen_role_window(scrno, role),
                                   colour);
}

static void headless_driver_set_global_window_attr(
   TheDriverGlobalWindowRole role, TheDriverAttr colour)
{
   headless_driver_set_window_attr(headless_global_window(role), colour);
}

static void headless_driver_set_window_background(TheDriverWindow *win,
                                                  TheDriverAttr colour)
{
   if (win != NULL)
      win->background = colour;
}

static void headless_driver_clear_line_at(TheDriverWindow *win, short row,
                                          TheDriverAttr colour)
{
   int col;

   if (win == NULL)
      return;
   win->attr = colour;
   for (col = 0; col < win->cols; col++)
      headless_set_cell_at(win, row, col, ' ');
}

static void headless_driver_clear_current_role(short role)
{
   headless_clear_window(headless_current_role_window(role));
}

static void headless_driver_clear_current_role_to_eol(short role)
{
   headless_clear_to_eol(headless_current_role_window(role));
}

static void headless_driver_clear_screen_role_to_eol(CHARTYPE scrno,
                                                     short role)
{
   headless_clear_to_eol(headless_screen_role_window(scrno, role));
}

static void headless_driver_touch_window(TheDriverWindow *win)
{
   if (win == NULL)
      return;
   win->dirty = 1;
   headless_log("touch:window:%d", win->id);
}

static void headless_driver_touch_line(TheDriverWindow *win, int start,
                                       int count)
{
   if (win == NULL)
      return;
   win->dirty = 1;
   headless_log("touch-line:window:%d:%d:%d", win->id, start, count);
}

static void headless_driver_touch_current_window(void)
{
   TheDriverWindow *win = headless_current_active_window();

   if (win != NULL)
      headless_driver_touch_window(win);
}

static void headless_driver_touch_current_role(short role)
{
   TheDriverWindow *win = headless_current_role_window(role);

   if (win != NULL)
      headless_driver_touch_window(win);
}

static void headless_driver_touch_screen_role(CHARTYPE scrno, short role)
{
   TheDriverWindow *win = headless_screen_role_window(scrno, role);

   if (win != NULL)
      headless_driver_touch_window(win);
}

static void headless_driver_touch_global_window(
   TheDriverGlobalWindowRole role)
{
   TheDriverWindow *win = headless_global_window(role);

   if (win == NULL)
      return;
   win->dirty = 1;
   headless_log("touch:global:%d", (int)role);
}

static void headless_driver_touch_and_refresh_current_role(short role)
{
   TheDriverWindow *win = headless_current_role_window(role);

   if (win == NULL)
      return;
   win->dirty = 1;
   win->refreshed = 1;
   headless_log("touch-refresh:current-role:%d", role);
}

static void headless_driver_touch_and_refresh_screen_role(CHARTYPE scrno,
                                                          short role)
{
   TheDriverWindow *win = headless_screen_role_window(scrno, role);

   if (win == NULL)
      return;
   win->dirty = 1;
   win->refreshed = 1;
   headless_log("touch-refresh:screen-role:%d:%d", (int)scrno, role);
}

static void headless_driver_touch_and_refresh_global_window(
   TheDriverGlobalWindowRole role)
{
   TheDriverWindow *win = headless_global_window(role);

   if (win == NULL)
      return;
   win->dirty = 1;
   win->refreshed = 1;
   headless_log("touch-refresh:global:%d", (int)role);
}

static void headless_driver_refresh_window(TheDriverWindow *win)
{
   if (win == NULL)
      return;
   win->refreshed = 1;
   headless_log("refresh:window:%d", win->id);
}

static void headless_driver_refresh_window_now(TheDriverWindow *win)
{
   if (win == NULL)
      return;
   win->refreshed = 1;
   win->immediate_refreshes++;
   headless_log("refresh-now:window:%d", win->id);
}

static void headless_driver_refresh_current_window(void)
{
   headless_driver_refresh_window(headless_current_active_window());
}

static void headless_driver_refresh_current_window_now(void)
{
   headless_driver_refresh_window_now(headless_current_active_window());
}

static void headless_driver_refresh_current_role(short role)
{
   TheDriverWindow *win = headless_current_role_window(role);

   if (win == NULL)
      return;
   win->refreshed = 1;
   headless_log("refresh:current-role:%d", role);
}

static void headless_driver_refresh_current_role_now(short role)
{
   TheDriverWindow *win = headless_current_role_window(role);

   if (win == NULL)
      return;
   win->refreshed = 1;
   win->immediate_refreshes++;
   headless_log("refresh-now:current-role:%d", role);
}

static void headless_driver_refresh_screen_window(CHARTYPE scrno)
{
   TheDriverWindow *win = headless_screen_active_window(scrno);

   if (win == NULL)
      return;
   win->refreshed = 1;
   headless_log("refresh:screen-window:%d", (int)scrno);
}

static void headless_driver_refresh_screen_role(CHARTYPE scrno, short role)
{
   TheDriverWindow *win = headless_screen_role_window(scrno, role);

   if (win == NULL)
      return;
   win->refreshed = 1;
   headless_log("refresh:screen-role:%d:%d", (int)scrno, role);
}

static void headless_driver_refresh_global_window(
   TheDriverGlobalWindowRole role)
{
   TheDriverWindow *win = headless_global_window(role);

   if (win == NULL)
      return;
   win->refreshed = 1;
   headless_log("refresh:global:%d", (int)role);
}

static void headless_driver_refresh_global_window_now(
   TheDriverGlobalWindowRole role)
{
   TheDriverWindow *win = headless_global_window(role);

   if (win == NULL)
      return;
   win->refreshed = 1;
   win->immediate_refreshes++;
   headless_log("refresh-now:global:%d", (int)role);
}

static void headless_driver_sync_terminal_screen(void)
{
   headless_log("sync:terminal");
}

static void headless_driver_clear_terminal_screen(void)
{
   TheDriverWindow *win = headless_standard_window();

   headless_clear_window(win);
   if (win != NULL)
   {
      win->attr = 0;
      win->cursor_row = 0;
      win->cursor_col = 0;
   }
   headless_log("clear:terminal");
}

static void headless_driver_begin_terminal_report(void)
{
   headless_clear_window(headless_standard_window());
   headless_state.terminal_report_writes = 0;
}

static void headless_driver_write_terminal_report_text(short row, short col,
                                                       TheDriverAttr attr,
                                                       const char *text,
                                                       size_t len)
{
   TheDriverWindow *win = headless_standard_window();
   size_t i;

   if (win == NULL || text == NULL)
      return;
   win->attr = attr;
   for (i = 0; i < len; i++)
      headless_set_cell_at(win, row, col + (int)i,
                           (unsigned char)text[i]);
   headless_state.terminal_report_writes++;
}

static void headless_driver_end_terminal_report(void)
{
   headless_log("terminal-report:%d", headless_state.terminal_report_writes);
}

static void headless_driver_update(void)
{
   headless_log("update");
}

static void headless_driver_present_cursor(bool visible)
{
   headless_state.cursor_visible = visible ? 1 : 0;
   headless_log("present-cursor:%d", headless_state.cursor_visible);
}

static void headless_driver_set_current_window_timeout(int milliseconds)
{
   TheDriverWindow *win;

   win = headless_current_active_window();
   if (win != NULL)
      win->timeout_ms = milliseconds;
}

static void headless_driver_draw_box(TheDriverWindow *win)
{
   int row;
   int col;

   if (win == NULL || win->rows <= 0 || win->cols <= 0)
      return;
   for (col = 0; col < win->cols; col++)
   {
      headless_set_cell_at(win, 0, col, '-');
      headless_set_cell_at(win, win->rows - 1, col, '-');
   }
   for (row = 0; row < win->rows; row++)
   {
      headless_set_cell_at(win, row, 0, '|');
      headless_set_cell_at(win, row, win->cols - 1, '|');
   }
   headless_set_cell_at(win, 0, 0, '+');
   headless_set_cell_at(win, 0, win->cols - 1, '+');
   headless_set_cell_at(win, win->rows - 1, 0, '+');
   headless_set_cell_at(win, win->rows - 1, win->cols - 1, '+');
}

static void headless_driver_draw_vertical_line(TheDriverWindow *win,
                                               TheDriverCell ch, int len)
{
   int i;

   if (win == NULL)
      return;
   for (i = 0; i < len; i++)
      headless_set_cell_at(win, win->cursor_row + i, win->cursor_col, ch);
}

static void headless_driver_add_string_at(TheDriverWindow *win, short row,
                                          short col, const char *text)
{
   if (win == NULL)
      return;
   win->cursor_row = row;
   win->cursor_col = col;
   headless_clamp_cursor(win);
   headless_write_string_at(win, row, col, text);
}

static void headless_driver_add_global_string_at(
   TheDriverGlobalWindowRole role, short row, short col, const char *text)
{
   headless_driver_add_string_at(headless_global_window(role), row, col, text);
}

static void headless_driver_add_cell_at(TheDriverWindow *win, short row,
                                        short col, TheDriverCell ch)
{
   if (win == NULL)
      return;
   win->cursor_row = row;
   win->cursor_col = col;
   headless_clamp_cursor(win);
   headless_set_cell_at(win, row, col, ch);
}

static void headless_driver_draw_horizontal_line(TheDriverWindow *win,
                                                 TheDriverCell ch, int len)
{
   int i;

   if (win == NULL)
      return;
   for (i = 0; i < len; i++)
      headless_set_cell_at(win, win->cursor_row, win->cursor_col + i, ch);
}

static void headless_driver_add_cell(TheDriverWindow *win, TheDriverCell ch)
{
   headless_add_cell_internal(win, ch);
}

static void headless_driver_insert_cell(TheDriverWindow *win,
                                        TheDriverCell ch)
{
   int col;

   if (win == NULL || win->cursor_row < 0 || win->cursor_row >= win->rows)
      return;
   for (col = win->cols - 1; col > win->cursor_col; col--)
      headless_set_cell_at(win, win->cursor_row, col,
                           headless_cell_at(win, win->cursor_row, col - 1));
   headless_set_cell_at(win, win->cursor_row, win->cursor_col, ch);
}

static void headless_driver_delete_cell(TheDriverWindow *win)
{
   int col;

   if (win == NULL || win->cursor_row < 0 || win->cursor_row >= win->rows)
      return;
   for (col = win->cursor_col; col < win->cols - 1; col++)
      headless_set_cell_at(win, win->cursor_row, col,
                           headless_cell_at(win, win->cursor_row, col + 1));
   if (win->cols > 0)
      headless_set_cell_at(win, win->cursor_row, win->cols - 1, ' ');
}

static void headless_driver_write_cell_span(TheDriverWindow *win,
                                            const TheDriverCell *text,
                                            int len)
{
   int i;

   if (win == NULL || text == NULL || len <= 0)
      return;
   for (i = 0; i < len; i++)
      headless_add_cell_internal(win, text[i]);
}

static void headless_driver_write_render_cells(TheDriverWindow *win,
                                               const TheRenderCell *text,
                                               int len)
{
   int i;

   if (win == NULL || text == NULL || len <= 0)
      return;
   for (i = 0; i < len; i++)
      headless_add_render_cell_internal(win, &text[i]);
}

static void headless_driver_write_render_cluster_at(
   TheDriverWindow *win, int row, int col, const TheRenderCluster *cluster)
{
   int next_col;

   if (win == NULL || cluster == NULL)
      return;
   headless_set_render_cell_at(win, row, col, cluster);
   next_col = col + (cluster->display_width > 0 ? cluster->display_width : 1);
   if (win->cols > 0 && next_col >= win->cols)
      next_col = win->cols - 1;
   if (row >= 0 && row < win->rows && next_col >= 0)
   {
      win->cursor_row = row;
      win->cursor_col = next_col;
      headless_clamp_cursor(win);
   }
   headless_log("render-cluster:window:%d:%d:%d:%zu:%d:%d:%d:%d",
                win->id, row, col, cluster->codepoint_count,
                cluster->logical_width, cluster->display_width,
                cluster->cursor_width, cluster->paint_width);
}

static void headless_driver_fill_cells_at(TheDriverWindow *win, int row,
                                          int col, int width,
                                          TheDriverAttr colour)
{
   int i;

   if (win == NULL)
      return;
   win->attr = colour;
   for (i = 0; i < width; i++)
      headless_set_cell_at(win, row, col + i, ' ');
}

static void headless_driver_write_ascii_cells_at(TheDriverWindow *win,
                                                 int row, int col,
                                                 const char *text, int width,
                                                 TheDriverAttr colour)
{
   int i;

   if (win == NULL || text == NULL)
      return;
   win->attr = colour;
   for (i = 0; i < width; i++)
   {
      TheDriverCell cell = text[i] == '\0' ? ' ' : (unsigned char)text[i];

      headless_set_cell_at(win, row, col + i, cell);
   }
}

static int headless_driver_read_input_event(TheInputEvent *event)
{
   if (event == NULL)
      return 0;
   *event = the_input_event_none();
   if (!the_input_queue_pop(&headless_state.input_queue, event))
      return 0;
   headless_log("read-input-event:%s", the_input_kind_name(event->kind));
   return event->kind != THE_INPUT_NONE;
}

static void headless_driver_prepare_for_shell_escape(void)
{
   headless_log("prepare:shell");
}

static void headless_driver_repair_terminal_background(
   TheDriverTerminalRepairTarget target)
{
   if (target == THE_DRIVER_REPAIR_TERMINAL_SCREEN)
   {
      headless_log("repair-background:terminal");
      return;
   }
   headless_log("repair-background:current");
}

static void headless_driver_redraw_window(TheDriverWindow *win)
{
   if (win != NULL)
      headless_log("redraw:window:%d", win->id);
}

static void headless_driver_redraw_current_role(short role)
{
   if (headless_current_role_window(role) != NULL)
      headless_log("redraw:current-role:%d", role);
}

static void headless_driver_redraw_screen_role(CHARTYPE scrno, short role)
{
   if (headless_screen_role_window(scrno, role) != NULL)
      headless_log("redraw:screen-role:%d:%d", (int)scrno, role);
}

static void headless_driver_redraw_global_window(
   TheDriverGlobalWindowRole role)
{
   if (headless_global_window(role) != NULL)
      headless_log("redraw:global:%d", (int)role);
}

static void headless_driver_draw_software_cell(
   CHARTYPE scrno, TheDriverWindow *win, short row, int col,
   TheDriverCell base, CursorShape shape)
{
   (void)scrno;
   (void)shape;
   headless_set_cell_at(win, row, col, base);
   if (win != NULL)
      headless_log("software-cell:window:%d:%d:%d", win->id, row, col);
}

static void headless_driver_draw_software_blank_cell(
   CHARTYPE scrno, TheDriverWindow *win, short row, int col,
   TheDriverAttr base, CursorShape shape)
{
   (void)scrno;
   (void)base;
   (void)shape;
   headless_set_cell_at(win, row, col, ' ');
   if (win != NULL)
      headless_log("software-blank:window:%d:%d:%d", win->id, row, col);
}

static short headless_driver_refresh_cursor(CHARTYPE scrno)
{
   headless_log("refresh-cursor:%d", (int)scrno);
   return 0;
}

static short headless_driver_redraw_screen_cursor(CHARTYPE scrno,
                                                  struct view_details *view)
{
   (void)view;
   headless_log("redraw-screen-cursor:%d", (int)scrno);
   return 0;
}

static void headless_driver_move_prefix_cursor(CHARTYPE scrno, short row,
                                               short col)
{
   headless_driver_move_screen_role_cursor(scrno, HEADLESS_ROLE_PREFIX, row,
                                           col);
}

static short headless_driver_move_filearea_cursor(
   CHARTYPE scrno, struct view_details *view, const CHARTYPE *line,
   size_t len, short row, int logical_col)
{
   LogicalCursor cursor;
   TheDriverCursorTarget target;
   TheDriverWindow *win;

   (void)view;
   win = headless_screen_role_window(scrno, HEADLESS_ROLE_FILEAREA);
   cursor = logical_cursor_make(LOGICAL_CURSOR_ZONE_FILEAREA, 0, row,
                                textpos_from_cell_virtual(line, len,
                                                          logical_col,
                                                          TEXT_SNAP_BACKWARD));
   target = driver_layout_filearea_target(cursor, line, len, 0,
                                          win == NULL ? 0 : win->cols);
   if (win != NULL)
      headless_move_cursor(win, row, (short)target.display_col);
   return 0;
}

static short headless_driver_filearea_cursor_transition(
   CHARTYPE scrno, struct view_details *view, const CHARTYPE *line,
   size_t len, short old_row, int old_logical_cell, int new_logical_cell,
   LENGTHTYPE old_verify_col)
{
   (void)view;
   (void)line;
   (void)len;
   headless_log("filearea-cursor-transition:%d:%d:%d:%d:%ld", (int)scrno,
                old_row, old_logical_cell, new_logical_cell,
                (long)old_verify_col);
   return 0;
}

const TheDriverOps the_headless_driver_ops = {
   .software_cursor_attr = headless_driver_software_cursor_attr,
   .current_window_is_role = headless_driver_current_window_is_role,
   .current_window_exists = headless_driver_current_window_exists,
   .screen_window_is_role = headless_driver_screen_window_is_role,
   .current_role_exists = headless_driver_current_role_exists,
   .screen_role_exists = headless_driver_screen_role_exists,
   .global_window_exists = headless_driver_global_window_exists,
   .delete_global_window = headless_driver_delete_global_window,
   .create_window = headless_driver_create_window,
   .delete_window = headless_driver_delete_window,
   .enable_keypad = headless_driver_enable_keypad,
   .enable_standard_keypad = headless_driver_enable_standard_keypad,
   .set_standard_notimeout = headless_driver_set_standard_notimeout,
   .set_window_leaveok = headless_driver_set_window_leaveok,
   .capture_window_cursor = headless_driver_capture_window_cursor,
   .capture_current_window_cursor =
      headless_driver_capture_current_window_cursor,
   .capture_current_previous_window_cursor =
      headless_driver_capture_current_previous_window_cursor,
   .capture_current_role_cursor = headless_driver_capture_current_role_cursor,
   .capture_screen_window_cursor = headless_driver_capture_screen_window_cursor,
   .capture_screen_role_cursor = headless_driver_capture_screen_role_cursor,
   .capture_global_window_cursor = headless_driver_capture_global_window_cursor,
   .window_origin = headless_driver_window_origin,
   .window_size = headless_driver_window_size,
   .current_window_origin = headless_driver_current_window_origin,
   .current_window_size = headless_driver_current_window_size,
   .current_role_size = headless_driver_current_role_size,
   .screen_role_size = headless_driver_screen_role_size,
   .current_window_cursor_screen_point =
      headless_driver_current_window_cursor_screen_point,
   .save_current_role_window = headless_driver_save_current_role_window,
   .restore_current_role_window = headless_driver_restore_current_role_window,
   .delete_current_role_window = headless_driver_delete_current_role_window,
   .clear_current_screen_roles = headless_driver_clear_current_screen_roles,
   .move_window_cursor = headless_driver_move_window_cursor,
   .move_current_window_cursor = headless_driver_move_current_window_cursor,
   .move_current_previous_window_cursor =
      headless_driver_move_current_previous_window_cursor,
   .move_current_role_cursor = headless_driver_move_current_role_cursor,
   .move_screen_window_cursor = headless_driver_move_screen_window_cursor,
   .move_screen_role_cursor = headless_driver_move_screen_role_cursor,
   .move_global_window_cursor = headless_driver_move_global_window_cursor,
   .restore_window_cursor = headless_driver_restore_window_cursor,
   .restore_current_window_cursor =
      headless_driver_restore_current_window_cursor,
   .restore_current_role_cursor = headless_driver_restore_current_role_cursor,
   .restore_screen_window_cursor =
      headless_driver_restore_screen_window_cursor,
   .restore_screen_role_cursor = headless_driver_restore_screen_role_cursor,
   .restore_global_window_cursor =
      headless_driver_restore_global_window_cursor,
   .read_window_cell = headless_driver_read_window_cell,
   .read_current_window_cell = headless_driver_read_current_window_cell,
   .read_current_window_cell_attr_at =
      headless_driver_read_current_window_cell_attr_at,
   .put_char_current_window = headless_driver_put_char_current_window,
   .set_window_attr = headless_driver_set_window_attr,
   .set_current_window_attr = headless_driver_set_current_window_attr,
   .set_current_role_attr = headless_driver_set_current_role_attr,
   .set_screen_role_attr = headless_driver_set_screen_role_attr,
   .set_global_window_attr = headless_driver_set_global_window_attr,
   .set_window_background = headless_driver_set_window_background,
   .clear_line_at = headless_driver_clear_line_at,
   .clear_current_role = headless_driver_clear_current_role,
   .clear_current_role_to_eol = headless_driver_clear_current_role_to_eol,
   .clear_screen_role_to_eol = headless_driver_clear_screen_role_to_eol,
   .touch_window = headless_driver_touch_window,
   .touch_line = headless_driver_touch_line,
   .touch_current_window = headless_driver_touch_current_window,
   .touch_current_role = headless_driver_touch_current_role,
   .touch_screen_role = headless_driver_touch_screen_role,
   .touch_global_window = headless_driver_touch_global_window,
   .touch_and_refresh_current_role =
      headless_driver_touch_and_refresh_current_role,
   .touch_and_refresh_screen_role =
      headless_driver_touch_and_refresh_screen_role,
   .touch_and_refresh_global_window =
      headless_driver_touch_and_refresh_global_window,
   .refresh_window = headless_driver_refresh_window,
   .refresh_window_now = headless_driver_refresh_window_now,
   .refresh_current_window = headless_driver_refresh_current_window,
   .refresh_current_window_now = headless_driver_refresh_current_window_now,
   .refresh_current_role = headless_driver_refresh_current_role,
   .refresh_current_role_now = headless_driver_refresh_current_role_now,
   .refresh_screen_window = headless_driver_refresh_screen_window,
   .refresh_screen_role = headless_driver_refresh_screen_role,
   .refresh_global_window = headless_driver_refresh_global_window,
   .refresh_global_window_now = headless_driver_refresh_global_window_now,
   .sync_terminal_screen = headless_driver_sync_terminal_screen,
   .clear_terminal_screen = headless_driver_clear_terminal_screen,
   .begin_terminal_report = headless_driver_begin_terminal_report,
   .write_terminal_report_text = headless_driver_write_terminal_report_text,
   .end_terminal_report = headless_driver_end_terminal_report,
   .update = headless_driver_update,
   .present_cursor = headless_driver_present_cursor,
   .set_current_window_timeout = headless_driver_set_current_window_timeout,
   .draw_box = headless_driver_draw_box,
   .draw_vertical_line = headless_driver_draw_vertical_line,
   .add_string_at = headless_driver_add_string_at,
   .add_global_string_at = headless_driver_add_global_string_at,
   .add_cell_at = headless_driver_add_cell_at,
   .draw_horizontal_line = headless_driver_draw_horizontal_line,
   .add_cell = headless_driver_add_cell,
   .insert_cell = headless_driver_insert_cell,
   .delete_cell = headless_driver_delete_cell,
   .write_cell_span = headless_driver_write_cell_span,
   .write_render_cells = headless_driver_write_render_cells,
   .write_render_cluster_at = headless_driver_write_render_cluster_at,
   .fill_cells_at = headless_driver_fill_cells_at,
   .write_ascii_cells_at = headless_driver_write_ascii_cells_at,
   .read_input_event = headless_driver_read_input_event,
   .prepare_for_shell_escape = headless_driver_prepare_for_shell_escape,
   .repair_terminal_background = headless_driver_repair_terminal_background,
   .redraw_window = headless_driver_redraw_window,
   .redraw_current_role = headless_driver_redraw_current_role,
   .redraw_screen_role = headless_driver_redraw_screen_role,
   .redraw_global_window = headless_driver_redraw_global_window,
   .draw_software_cell = headless_driver_draw_software_cell,
   .draw_software_blank_cell = headless_driver_draw_software_blank_cell,
   .refresh_cursor = headless_driver_refresh_cursor,
   .redraw_screen_cursor = headless_driver_redraw_screen_cursor,
   .move_prefix_cursor = headless_driver_move_prefix_cursor,
   .move_filearea_cursor = headless_driver_move_filearea_cursor,
   .filearea_cursor_transition = headless_driver_filearea_cursor_transition
};
