#include "the.h"
#include "proto.h"
#include "cursesdriver.h"

#include <stdio.h>
#include <string.h>

#ifdef USE_UTF8
# include <wchar.h>
# include "utflayout.h"
#endif

int curses_driver_clamp_display_col(int display_col, int window_cols)
{
   if (display_col < 0)
      return 0;
   if (window_cols > 0 && display_col >= window_cols)
      return window_cols - 1;
   return display_col;
}

int curses_driver_display_col_from_logical(const CHARTYPE *line, size_t len,
                                           int viewport_col, int logical_col)
{
#ifdef USE_UTF8
   return utf8_layout_display_col_from_logical(line, len, viewport_col,
                                               logical_col);
#else
   INTENTIONALLY_UNUSED_VARIABLE(line);
   INTENTIONALLY_UNUSED_VARIABLE(len);
   if (logical_col <= viewport_col)
      return 0;
   return logical_col - viewport_col;
#endif
}

int curses_driver_logical_col_from_display(const CHARTYPE *line, size_t len,
                                           int viewport_col, int display_col,
                                           TextSnap snap)
{
#ifdef USE_UTF8
   return utf8_layout_logical_col_from_display(line, len, viewport_col,
                                               display_col, snap);
#else
   INTENTIONALLY_UNUSED_VARIABLE(line);
   INTENTIONALLY_UNUSED_VARIABLE(len);
   INTENTIONALLY_UNUSED_VARIABLE(snap);
   if (viewport_col < 0)
      viewport_col = 0;
   if (display_col < 0)
      display_col = 0;
   return viewport_col + display_col;
#endif
}

int curses_driver_viewport_col_for_logical(const CHARTYPE *line, size_t len,
                                           int current_viewport_col,
                                           int logical_col, int window_cols,
                                           int *display_col, int *visible)
{
#ifdef USE_UTF8
   Utf8LayoutViewport target;

   current_viewport_col = current_viewport_col < 0 ? 0 : current_viewport_col;
   logical_col = logical_col < 0 ? 0 : logical_col;
   target = utf8_layout_viewport_for_logical_col(line, len,
                                                 current_viewport_col,
                                                 logical_col, window_cols);
   if (display_col != NULL)
      *display_col = target.display_col;
   if (visible != NULL)
      *visible = target.visible;
   return target.viewport_col;
#else
   int target_display_col;
   int target_visible;
   int preferred_display_col;

   INTENTIONALLY_UNUSED_VARIABLE(line);
   INTENTIONALLY_UNUSED_VARIABLE(len);
   if (current_viewport_col < 0)
      current_viewport_col = 0;
   if (logical_col < 0)
      logical_col = 0;
   target_display_col = logical_col - current_viewport_col;
   target_visible = target_display_col >= 0
                 && window_cols > 0
                 && target_display_col < window_cols;
   if (target_visible || window_cols <= 0)
   {
      if (display_col != NULL)
         *display_col = target_display_col;
      if (visible != NULL)
         *visible = target_visible;
      return current_viewport_col;
   }

   preferred_display_col = window_cols / 2 - 1;
   if (preferred_display_col < 0)
      preferred_display_col = 0;
   current_viewport_col = logical_col - preferred_display_col;
   if (current_viewport_col < 0)
      current_viewport_col = 0;
   target_display_col = logical_col - current_viewport_col;
   if (display_col != NULL)
      *display_col = target_display_col;
   if (visible != NULL)
      *visible = window_cols > 0 && target_display_col < window_cols;
   return current_viewport_col;
#endif
}

static int curses_driver_valid_screen(CHARTYPE scrno)
{
   return scrno < MAX_SCREENS;
}

static int curses_driver_valid_view_role(short role)
{
   return role >= 0 && role < VIEW_WINDOWS;
}

static WINDOW **curses_driver_screen_role_window_slot(CHARTYPE scrno,
                                                      short role)
{
   if (!curses_driver_valid_screen(scrno)
   ||  !curses_driver_valid_view_role(role))
      return NULL;
   return &screen[scrno].win[role];
}

static WINDOW *curses_driver_screen_role_window(CHARTYPE scrno, short role)
{
   WINDOW **slot = curses_driver_screen_role_window_slot(scrno, role);

   return (slot == NULL) ? NULL : *slot;
}

static WINDOW *curses_driver_screen_active_window(CHARTYPE scrno)
{
   VIEW_DETAILS *view;

   if (!curses_driver_valid_screen(scrno))
      return NULL;
   view = screen[scrno].screen_view;
   if (view == NULL || !curses_driver_valid_view_role(view->current_window))
      return NULL;
   return screen[scrno].win[view->current_window];
}

static WINDOW *curses_driver_screen_previous_window(CHARTYPE scrno)
{
   VIEW_DETAILS *view;

   if (!curses_driver_valid_screen(scrno))
      return NULL;
   view = screen[scrno].screen_view;
   if (view == NULL || !curses_driver_valid_view_role(view->previous_window))
      return NULL;
   return screen[scrno].win[view->previous_window];
}

static WINDOW *curses_driver_current_active_window(void)
{
   return curses_driver_screen_active_window(current_screen);
}

static WINDOW *curses_driver_current_previous_window(void)
{
   return curses_driver_screen_previous_window(current_screen);
}

static WINDOW *curses_driver_current_role_window(short role)
{
   return curses_driver_screen_role_window(current_screen, role);
}

static WINDOW *curses_driver_global_window(CursesDriverGlobalWindowRole role)
{
   switch (role)
   {
      case CURSES_DRIVER_GLOBAL_STATAREA:
         return statarea;
      case CURSES_DRIVER_GLOBAL_ERROR:
         return error_window;
      case CURSES_DRIVER_GLOBAL_DIVIDER:
         return divider;
      case CURSES_DRIVER_GLOBAL_FILETABS:
         return filetabs;
   }
   return NULL;
}

static WINDOW **curses_driver_global_window_slot(CursesDriverGlobalWindowRole role)
{
   switch (role)
   {
      case CURSES_DRIVER_GLOBAL_STATAREA:
         return &statarea;
      case CURSES_DRIVER_GLOBAL_ERROR:
         return &error_window;
      case CURSES_DRIVER_GLOBAL_DIVIDER:
         return &divider;
      case CURSES_DRIVER_GLOBAL_FILETABS:
         return &filetabs;
   }
   return NULL;
}

CursorShape current_cursor_shape(void)
{
   return INSERTMODEx ? cursorstyle_insert_shape : cursorstyle_over_shape;
}

CursorBlink current_cursor_blink(void)
{
   return INSERTMODEx ? cursorstyle_insert_blink : cursorstyle_over_blink;
}

CursorPresentation current_cursor_presentation(void)
{
#ifdef USE_UTF8
   if (CURRENT_VIEW != NULL
   &&  (CURRENT_VIEW->current_window == WINDOW_FILEAREA
     || CURRENT_VIEW->current_window == WINDOW_PREFIX
     || CURRENT_VIEW->current_window == WINDOW_COMMAND))
      return CURSOR_PRESENTATION_SOFTWARE;
#endif
   return CURSOR_PRESENTATION_HARDWARE;
}

bool current_cursor_uses_software(void)
{
   return current_cursor_presentation() == CURSOR_PRESENTATION_SOFTWARE;
}

static void curses_driver_apply_cursor_visibility(bool visible)
{
   TRACE_FUNCTION("cursesdriver.c: curses_driver_apply_cursor_visibility");
#ifdef HAVE_CURS_SET
   if (visible)
   {
      CursorShape shape;
      CursorBlink blink;

      if (current_cursor_uses_software())
      {
         curs_set(0);
         TRACE_RETURN();
         return;
      }

      shape = current_cursor_shape();
      blink = current_cursor_blink();

#ifdef USE_NCURSES
      int seq = 1;
      if (shape == CURSOR_BLOCK && blink == CURSOR_BLINK) seq = 1;
      else if (shape == CURSOR_BLOCK && blink == CURSOR_STEADY) seq = 2;
      else if (shape == CURSOR_UNDERLINE && blink == CURSOR_BLINK) seq = 3;
      else if (shape == CURSOR_UNDERLINE && blink == CURSOR_STEADY) seq = 4;
      else if (shape == CURSOR_IBEAM && blink == CURSOR_BLINK) seq = 5;
      else if (shape == CURSOR_IBEAM && blink == CURSOR_STEADY) seq = 6;

      printf("\033[%d q", seq);
      fflush(stdout);
#endif

      if (shape == CURSOR_BLOCK)
      {
         curs_set(1);
         curs_set(2);
      }
      else
         curs_set(1);
   }
   else
      curs_set(0);
#else
   INTENTIONALLY_UNUSED_VARIABLE(visible);
#endif
   TRACE_RETURN();
}

chtype curses_driver_software_cursor_attr(CHARTYPE scrno, chtype base,
                                          CursorShape shape)
{
   if (shape == CURSOR_BLOCK)
      return set_colour(SCREEN_FILE(scrno)->attr+ATTR_CBLOCK);
#ifdef A_UNDERLINE
   return base | A_UNDERLINE;
#else
   return set_colour(SCREEN_FILE(scrno)->attr+ATTR_CBLOCK);
#endif
}

void curses_driver_draw_software_chtype_cell(CHARTYPE scrno, WINDOW *win,
                                             short row, int col, chtype base,
                                             CursorShape shape)
{
   chtype cell;
   chtype ch;
   int maxy;
   int maxx;

   if (win == NULL)
      return;
   maxy = getmaxy(win);
   maxx = getmaxx(win);
   if (row < 0 || row >= maxy || col < 0 || col >= maxx)
      return;

   cell = mvwinch(win, row, col);
   ch = cell & A_CHARTEXT;
   {
      chtype attrs = cell & A_ATTRIBUTES;

#ifdef A_UNDERLINE
      if (shape == CURSOR_UNDERLINE)
         attrs &= ~A_UNDERLINE;
#endif
      if (attrs != 0)
         base = attrs;
   }
   if (ch == 0)
      ch = ' ';
   wattrset(win, curses_driver_software_cursor_attr(scrno, base, shape));
   mvwaddch(win, row, col, ch);
   wattrset(win, base);
}

void curses_driver_draw_software_blank_cell(CHARTYPE scrno, WINDOW *win,
                                            short row, int col, chtype base,
                                            CursorShape shape)
{
   int maxy;
   int maxx;

   if (win == NULL)
      return;
   maxy = getmaxy(win);
   maxx = getmaxx(win);
   if (row < 0 || row >= maxy || col < 0 || col >= maxx)
      return;

   wattrset(win, curses_driver_software_cursor_attr(scrno, base, shape));
   mvwaddch(win, row, col, ' ');
   wattrset(win, base);
}

#ifdef USE_UTF8
static void curses_driver_codepoint_to_wchars(uint32_t ch, wchar_t wch[3])
{
   if ((ch >= 0xD800u && ch <= 0xDFFFu) || ch > 0x10FFFFu)
      ch = TEXT_INVALID_CODEPOINT;
# if defined(WCHAR_MAX) && WCHAR_MAX <= 0xFFFFu
   if (ch > 0xFFFFu)
   {
      ch -= 0x10000u;
      wch[0] = (wchar_t)(0xD800u + (ch >> 10));
      wch[1] = (wchar_t)(0xDC00u + (ch & 0x3FFu));
      wch[2] = L'\0';
      return;
   }
# endif
   wch[0] = (wchar_t)ch;
   wch[1] = L'\0';
}

void curses_driver_set_cchar_codepoint(cchar_t *dest, uint32_t ch,
                                       chtype colour)
{
   wchar_t wch[3];

   if (dest == NULL)
      return;
   curses_driver_codepoint_to_wchars(ch, wch);
   setcchar(dest, wch, colour, PAIR_NUMBER(colour & A_COLOR), NULL);
}

void curses_driver_recolour_cchar(cchar_t *cell, chtype colour)
{
   wchar_t wch[6];
   const wchar_t *pwch = (const wchar_t *)&wch;
   attr_t attrs;
   short pair;

   if (cell == NULL)
      return;
   getcchar(cell, wch, &attrs, &pair, NULL);
   setcchar(cell, pwch, 0, PAIR_NUMBER(colour & A_COLOR), NULL);
}

void curses_driver_write_wide_string_at(WINDOW *win, int row, int col,
                                        const wchar_t *text, chtype colour,
                                        int expected_width)
{
   int next_col;
   int maxx;

   if (win == NULL || text == NULL)
      return;
   maxx = getmaxx(win);
   if (row < 0 || col >= maxx)
      return;
   if (col < 0)
      col = 0;

   wmove(win, row, col);
   wattrset(win, colour);
   waddwstr(win, text);

   next_col = col + ((expected_width > 0) ? expected_width : 1);
   if (next_col >= maxx)
      next_col = maxx - 1;
   if (next_col >= 0)
      wmove(win, row, next_col);
   wattrset(win, A_NORMAL);
}

void curses_driver_fill_cells_at(WINDOW *win, int row, int col, int width,
                                 chtype colour)
{
   int maxy;
   int maxx;
   int i;

   if (win == NULL || width <= 0)
      return;
   maxy = getmaxy(win);
   maxx = getmaxx(win);
   if (row < 0 || row >= maxy || col >= maxx)
      return;
   if (col < 0)
   {
      width += col;
      col = 0;
   }
   if (width <= 0)
      return;
   if (col + width > maxx)
      width = maxx - col;

   wattrset(win, colour);
   wmove(win, row, col);
   for (i = 0; i < width; i++)
      waddch(win, ' ');
   wattrset(win, A_NORMAL);
}

void curses_driver_write_ascii_cells_at(WINDOW *win, int row, int col,
                                        const char *text, int width,
                                        chtype colour)
{
   int maxy;
   int maxx;
   int i;

   if (win == NULL || text == NULL || width <= 0)
      return;
   maxy = getmaxy(win);
   maxx = getmaxx(win);
   if (row < 0 || row >= maxy || col >= maxx)
      return;
   if (col < 0)
   {
      text -= col;
      width += col;
      col = 0;
   }
   if (width <= 0)
      return;
   if (col + width > maxx)
      width = maxx - col;

   wattrset(win, colour);
   wmove(win, row, col);
   for (i = 0; i < width && text[i] != '\0'; i++)
      waddch(win, (unsigned char)text[i]);
   wattrset(win, A_NORMAL);
}
#endif

CursesDriverWindowCursor curses_driver_capture_window_cursor(WINDOW *win)
{
   CursesDriverWindowCursor cursor;

   cursor.row = 0;
   cursor.col = 0;
   cursor.valid = 0;
   if (win == NULL)
      return cursor;

   getyx(win, cursor.row, cursor.col);
   cursor.valid = 1;
   return cursor;
}

CursesDriverWindowOrigin curses_driver_window_origin(WINDOW *win)
{
   CursesDriverWindowOrigin origin;

   origin.row = 0;
   origin.col = 0;
   origin.valid = 0;
   if (win == NULL)
      return origin;

   getbegyx(win, origin.row, origin.col);
   origin.valid = 1;
   return origin;
}

CursesDriverWindowSize curses_driver_window_size(WINDOW *win)
{
   CursesDriverWindowSize size;

   size.rows = 0;
   size.cols = 0;
   size.valid = 0;
   if (win == NULL)
      return size;

   size.rows = (short)getmaxy(win);
   size.cols = (short)getmaxx(win);
   size.valid = 1;
   return size;
}

CursesDriverScreenPoint curses_driver_window_cursor_screen_point(WINDOW *win)
{
   CursesDriverScreenPoint point;
   CursesDriverWindowCursor cursor;
   CursesDriverWindowOrigin origin;

   point.row = -1;
   point.col = -1;
   point.valid = 0;
   cursor = curses_driver_capture_window_cursor(win);
   origin = curses_driver_window_origin(win);
   if (!cursor.valid || !origin.valid)
      return point;

   point.row = (short)(cursor.row + origin.row);
   point.col = (short)(cursor.col + origin.col);
   point.valid = 1;
   return point;
}

int curses_driver_current_window_is_role(short role)
{
   if (CURRENT_VIEW == NULL)
      return 0;
   return CURRENT_VIEW->current_window == role;
}

int curses_driver_current_window_exists(void)
{
   return curses_driver_current_active_window() != NULL;
}

int curses_driver_screen_window_is_role(CHARTYPE scrno, short role)
{
   VIEW_DETAILS *view;

   if (!curses_driver_valid_screen(scrno))
      return 0;
   view = screen[scrno].screen_view;
   if (view == NULL)
      return 0;
   return view->current_window == role;
}

int curses_driver_current_role_exists(short role)
{
   return curses_driver_current_role_window(role) != NULL;
}

int curses_driver_screen_role_exists(CHARTYPE scrno, short role)
{
   return curses_driver_screen_role_window(scrno, role) != NULL;
}

int curses_driver_global_window_exists(CursesDriverGlobalWindowRole role)
{
   return curses_driver_global_window(role) != NULL;
}

void curses_driver_delete_global_window(CursesDriverGlobalWindowRole role)
{
   WINDOW **slot = curses_driver_global_window_slot(role);

   if (slot == NULL || *slot == NULL)
      return;
   curses_driver_delete_window(*slot);
   *slot = NULL;
}

CursesDriverWindowCursor curses_driver_capture_current_window_cursor(void)
{
   return curses_driver_capture_window_cursor(curses_driver_current_active_window());
}

CursesDriverWindowCursor curses_driver_capture_current_previous_window_cursor(void)
{
   return curses_driver_capture_window_cursor(
      curses_driver_current_previous_window());
}

CursesDriverWindowCursor curses_driver_capture_current_role_cursor(short role)
{
   return curses_driver_capture_window_cursor(
      curses_driver_current_role_window(role));
}

CursesDriverWindowCursor curses_driver_capture_screen_window_cursor(CHARTYPE scrno)
{
   return curses_driver_capture_window_cursor(
      curses_driver_screen_active_window(scrno));
}

CursesDriverWindowCursor curses_driver_capture_screen_role_cursor(CHARTYPE scrno,
                                                                 short role)
{
   return curses_driver_capture_window_cursor(
      curses_driver_screen_role_window(scrno, role));
}

CursesDriverWindowOrigin curses_driver_current_window_origin(void)
{
   return curses_driver_window_origin(curses_driver_current_active_window());
}

CursesDriverWindowSize curses_driver_current_window_size(void)
{
   return curses_driver_window_size(curses_driver_current_active_window());
}

CursesDriverWindowSize curses_driver_current_role_size(short role)
{
   return curses_driver_window_size(curses_driver_current_role_window(role));
}

CursesDriverWindowSize curses_driver_screen_role_size(CHARTYPE scrno,
                                                      short role)
{
   return curses_driver_window_size(curses_driver_screen_role_window(scrno,
                                                                     role));
}

CursesDriverWindowRoleSave curses_driver_save_current_role_window(short role)
{
   WINDOW **slot = curses_driver_screen_role_window_slot(current_screen, role);
   CursesDriverWindowRoleSave saved;

   saved.window = NULL;
   saved.slot_valid = 0;
   if (slot == NULL)
      return saved;
   saved.window = *slot;
   saved.slot_valid = 1;
   return saved;
}

int curses_driver_replace_current_role_with_relative_window(
   short role, WINDOW *parent, int rows, int cols, int row, int col,
   CursesDriverWindowRoleSave *saved)
{
   WINDOW **slot = curses_driver_screen_role_window_slot(current_screen, role);

   if (saved != NULL)
   {
      saved->window = NULL;
      saved->slot_valid = 0;
   }
   if (slot == NULL)
      return 0;
   if (saved != NULL)
   {
      saved->window = *slot;
      saved->slot_valid = 1;
   }
   *slot = curses_driver_create_relative_window(parent, rows, cols, row, col);
   return *slot != NULL;
}

void curses_driver_restore_current_role_window(
   short role, CursesDriverWindowRoleSave saved)
{
   WINDOW **slot;

   if (!saved.slot_valid)
      return;
   slot = curses_driver_screen_role_window_slot(current_screen, role);
   if (slot == NULL)
      return;
   *slot = (WINDOW *)saved.window;
}

void curses_driver_delete_current_role_window(short role)
{
   WINDOW **slot = curses_driver_screen_role_window_slot(current_screen, role);

   if (slot == NULL || *slot == NULL)
      return;
   curses_driver_delete_window(*slot);
   *slot = NULL;
}

void curses_driver_clear_current_screen_roles(void)
{
   short i;

   if (!curses_driver_valid_screen(current_screen))
      return;
   for (i = 0; i < VIEW_WINDOWS; i++)
      screen[current_screen].win[i] = NULL;
}

WINDOW *curses_driver_create_window(int rows, int cols, int row, int col)
{
   return newwin(rows, cols, row, col);
}

WINDOW *curses_driver_create_pad(int rows, int cols)
{
#ifdef HAVE_NEWPAD
   return newpad(rows, cols);
#else
   INTENTIONALLY_UNUSED_VARIABLE(rows);
   INTENTIONALLY_UNUSED_VARIABLE(cols);
   return NULL;
#endif
}

WINDOW *curses_driver_create_relative_window(WINDOW *parent, int rows,
                                             int cols, int row, int col)
{
#ifdef HAVE_DERWIN
   return derwin(parent, rows, cols, row, col);
#else
   CursesDriverWindowOrigin origin;

   origin = curses_driver_window_origin(parent);
   if (!origin.valid)
      return NULL;
   return subwin(parent, rows, cols, origin.row + row, origin.col + col);
#endif
}

void curses_driver_delete_window(WINDOW *win)
{
   if (win == NULL)
      return;
   delwin(win);
}

void curses_driver_enable_keypad(WINDOW *win, bool enabled)
{
   if (win == NULL)
      return;
#ifdef HAVE_KEYPAD
   keypad(win, enabled ? TRUE : FALSE);
#else
   INTENTIONALLY_UNUSED_VARIABLE(enabled);
#endif
}

void curses_driver_move_window_cursor(WINDOW *win, short row, short col)
{
   if (win == NULL)
      return;
   wmove(win, row, col);
}

void curses_driver_restore_window_cursor(WINDOW *win,
                                         CursesDriverWindowCursor cursor)
{
   if (win == NULL || !cursor.valid)
      return;
   curses_driver_move_window_cursor(win, cursor.row, cursor.col);
}

void curses_driver_move_current_window_cursor(short row, short col)
{
   curses_driver_move_window_cursor(curses_driver_current_active_window(), row,
                                    col);
}

void curses_driver_move_current_previous_window_cursor(short row, short col)
{
   curses_driver_move_window_cursor(curses_driver_current_previous_window(),
                                    row, col);
}

void curses_driver_move_current_role_cursor(short role, short row, short col)
{
   curses_driver_move_window_cursor(curses_driver_current_role_window(role),
                                    row, col);
}

void curses_driver_move_screen_window_cursor(CHARTYPE scrno, short row,
                                             short col)
{
   curses_driver_move_window_cursor(curses_driver_screen_active_window(scrno),
                                    row, col);
}

void curses_driver_move_screen_role_cursor(CHARTYPE scrno, short role,
                                           short row, short col)
{
   curses_driver_move_window_cursor(curses_driver_screen_role_window(scrno,
                                                                     role),
                                    row, col);
}

void curses_driver_restore_current_window_cursor(CursesDriverWindowCursor cursor)
{
   curses_driver_restore_window_cursor(curses_driver_current_active_window(),
                                       cursor);
}

void curses_driver_restore_current_role_cursor(short role,
                                              CursesDriverWindowCursor cursor)
{
   curses_driver_restore_window_cursor(curses_driver_current_role_window(role),
                                       cursor);
}

chtype curses_driver_read_window_cell(WINDOW *win)
{
   if (win == NULL)
      return 0;
   return (chtype)winch(win);
}

chtype curses_driver_read_current_window_cell(void)
{
   return curses_driver_read_window_cell(curses_driver_current_active_window());
}

chtype curses_driver_read_current_window_cell_attr_at(short row, short col)
{
   WINDOW *win = curses_driver_current_active_window();

   if (win == NULL)
      return 0;
#if defined(USE_EXTCURSES)
   return win->_a[row][col];
#elif defined(VMS) && defined(_BSD44_CURSES)
   INTENTIONALLY_UNUSED_VARIABLE(row);
   INTENTIONALLY_UNUSED_VARIABLE(col);
   return win->lines[win->cury]->line[win->curx].attr;
#else
   INTENTIONALLY_UNUSED_VARIABLE(row);
   INTENTIONALLY_UNUSED_VARIABLE(col);
   return curses_driver_read_window_cell(win) & A_ATTRIBUTES;
#endif
}

void curses_driver_put_char_current_window(chtype ch, CHARTYPE add_ins)
{
   put_char(curses_driver_current_active_window(), ch, add_ins);
}

void curses_driver_set_window_attr(WINDOW *win, chtype colour)
{
   if (win == NULL)
      return;
   wattrset(win, colour);
}

void curses_driver_set_current_window_attr(chtype colour)
{
   curses_driver_set_window_attr(curses_driver_current_active_window(), colour);
}

void curses_driver_set_current_role_attr(short role, chtype colour)
{
   curses_driver_set_window_attr(curses_driver_current_role_window(role),
                                 colour);
}

void curses_driver_set_screen_role_attr(CHARTYPE scrno, short role,
                                        chtype colour)
{
   curses_driver_set_window_attr(curses_driver_screen_role_window(scrno, role),
                                 colour);
}

void curses_driver_set_global_window_attr(CursesDriverGlobalWindowRole role,
                                          chtype colour)
{
   curses_driver_set_window_attr(curses_driver_global_window(role), colour);
}

void curses_driver_set_window_background(WINDOW *win, chtype colour)
{
   if (win == NULL)
      return;
#ifdef HAVE_WBKGD
   wbkgd(win, colour);
#else
   wattrset(win, colour);
   wmove(win, 0, 0);
   wclrtobot(win);
#endif
}

void curses_driver_clear_window(WINDOW *win)
{
   if (win == NULL)
      return;
   wclear(win);
}

void curses_driver_clear_current_role(short role)
{
   curses_driver_clear_window(curses_driver_current_role_window(role));
}

void curses_driver_clear_window_to_bottom(WINDOW *win)
{
   if (win == NULL)
      return;
   wclrtobot(win);
}

void curses_driver_clear_to_eol(WINDOW *win)
{
   if (win == NULL)
      return;
   my_wclrtoeol(win);
}

void curses_driver_clear_current_role_to_eol(short role)
{
   curses_driver_clear_to_eol(curses_driver_current_role_window(role));
}

void curses_driver_clear_screen_role_to_eol(CHARTYPE scrno, short role)
{
   curses_driver_clear_to_eol(curses_driver_screen_role_window(scrno, role));
}

void curses_driver_touch_window(WINDOW *win)
{
   if (win == NULL)
      return;
   touchwin(win);
}

void curses_driver_touch_current_window(void)
{
   curses_driver_touch_window(curses_driver_current_active_window());
}

void curses_driver_touch_current_role(short role)
{
   curses_driver_touch_window(curses_driver_current_role_window(role));
}

void curses_driver_touch_screen_role(CHARTYPE scrno, short role)
{
   curses_driver_touch_window(curses_driver_screen_role_window(scrno, role));
}

void curses_driver_touch_global_window(CursesDriverGlobalWindowRole role)
{
   curses_driver_touch_window(curses_driver_global_window(role));
}

void curses_driver_touch_and_refresh_current_role(short role)
{
   curses_driver_touch_current_role(role);
   curses_driver_refresh_current_role(role);
}

void curses_driver_touch_and_refresh_screen_role(CHARTYPE scrno, short role)
{
   curses_driver_touch_screen_role(scrno, role);
   curses_driver_refresh_screen_role(scrno, role);
}

void curses_driver_touch_and_refresh_global_window(
   CursesDriverGlobalWindowRole role)
{
   curses_driver_touch_global_window(role);
   curses_driver_refresh_global_window(role);
}

void curses_driver_touch_line(WINDOW *win, int start, int count)
{
   if (win == NULL)
      return;
   touchline(win, start, count);
}

void curses_driver_clear_line_at(WINDOW *win, short row, chtype colour)
{
   if (win == NULL)
      return;
   curses_driver_move_window_cursor(win, row, 0);
   curses_driver_set_window_attr(win, colour);
   my_wclrtoeol(win);
}

void curses_driver_refresh_window(WINDOW *win)
{
   if (win == NULL)
      return;
   wnoutrefresh(win);
}

void curses_driver_refresh_window_now(WINDOW *win)
{
   if (win == NULL)
      return;
   wrefresh(win);
}

void curses_driver_refresh_current_window(void)
{
   curses_driver_refresh_window(curses_driver_current_active_window());
}

void curses_driver_refresh_current_window_now(void)
{
   curses_driver_refresh_window_now(curses_driver_current_active_window());
}

void curses_driver_refresh_current_role(short role)
{
   curses_driver_refresh_window(curses_driver_current_role_window(role));
}

void curses_driver_refresh_current_role_now(short role)
{
   curses_driver_refresh_window_now(curses_driver_current_role_window(role));
}

void curses_driver_refresh_screen_window(CHARTYPE scrno)
{
   curses_driver_refresh_window(curses_driver_screen_active_window(scrno));
}

void curses_driver_refresh_screen_role(CHARTYPE scrno, short role)
{
   curses_driver_refresh_window(curses_driver_screen_role_window(scrno, role));
}

void curses_driver_refresh_global_window(CursesDriverGlobalWindowRole role)
{
   curses_driver_refresh_window(curses_driver_global_window(role));
}

void curses_driver_refresh_global_window_now(CursesDriverGlobalWindowRole role)
{
   curses_driver_refresh_window_now(curses_driver_global_window(role));
}

void curses_driver_refresh_standard_screen(void)
{
   refresh();
}

void curses_driver_refresh_pad(WINDOW *pad, int pad_row, int pad_col,
                               int screen_top, int screen_left,
                               int screen_bottom, int screen_right)
{
   if (pad == NULL)
      return;
#ifdef HAVE_PREFRESH
   prefresh(pad, pad_row, pad_col, screen_top, screen_left,
            screen_bottom, screen_right);
#else
   INTENTIONALLY_UNUSED_VARIABLE(pad_row);
   INTENTIONALLY_UNUSED_VARIABLE(pad_col);
   INTENTIONALLY_UNUSED_VARIABLE(screen_top);
   INTENTIONALLY_UNUSED_VARIABLE(screen_left);
   INTENTIONALLY_UNUSED_VARIABLE(screen_bottom);
   INTENTIONALLY_UNUSED_VARIABLE(screen_right);
#endif
}

void curses_driver_update(void)
{
   doupdate();
}

void curses_driver_present_cursor(bool visible)
{
   curses_driver_apply_cursor_visibility(visible);
}

void curses_driver_set_window_timeout(WINDOW *win, int milliseconds)
{
   if (win == NULL)
      return;
   wtimeout(win, milliseconds);
}

void curses_driver_draw_box(WINDOW *win)
{
   if (win == NULL)
      return;
#if defined(HAVE_BOX)
   box(win, 0, 0);
#endif
}

void curses_driver_draw_vertical_line(WINDOW *win, chtype ch, int len)
{
#ifndef HAVE_WVLINE
   CursesDriverWindowCursor cursor;
   int i;
#endif

   if (win == NULL || len <= 0)
      return;
#ifdef HAVE_WVLINE
   wvline(win, ch, len);
#else
   cursor = curses_driver_capture_window_cursor(win);
   if (!cursor.valid)
      return;
   for (i = 0; i < len; i++)
   {
      curses_driver_move_window_cursor(win, (short)(cursor.row + i), cursor.col);
      curses_driver_add_chtype(win, ch);
   }
#endif
}

void curses_driver_add_string(WINDOW *win, const char *text)
{
   if (win == NULL || text == NULL)
      return;
   waddstr(win, text);
}

void curses_driver_add_string_at(WINDOW *win, short row, short col,
                                 const char *text)
{
   if (win == NULL || text == NULL)
      return;
   curses_driver_move_window_cursor(win, row, col);
   curses_driver_add_string(win, text);
}

void curses_driver_add_global_string_at(CursesDriverGlobalWindowRole role,
                                        short row, short col,
                                        const char *text)
{
   curses_driver_add_string_at(curses_driver_global_window(role), row, col,
                               text);
}

void curses_driver_add_chtype_at(WINDOW *win, short row, short col, chtype ch)
{
   if (win == NULL)
      return;
   curses_driver_move_window_cursor(win, row, col);
   waddch(win, ch);
}

void curses_driver_draw_horizontal_line(WINDOW *win, chtype ch, int len)
{
   int i;

   if (win == NULL || len <= 0)
      return;
#ifdef HAVE_WHLINE
   whline(win, ch, len);
#else
   for (i = 0; i < len; i++)
      waddch(win, ch);
#endif
}

static int curses_driver_translate_input_key(int key)
{
#ifdef KEY_MOUSE
   if (key == KEY_MOUSE)
      return THE_KEY_MOUSE;
#endif
   return key;
}

int curses_driver_mouse_key_code(void)
{
   return THE_KEY_MOUSE;
}

int curses_driver_is_mouse_key(int key)
{
   if (key == THE_KEY_MOUSE)
      return 1;
#ifdef KEY_MOUSE
   if (key == KEY_MOUSE)
      return 1;
#endif
   return 0;
}

int curses_driver_read_window_key(WINDOW *win)
{
   if (win == NULL)
      return ERR;
   return curses_driver_translate_input_key(my_getch(win));
}

int curses_driver_read_current_window_key(void)
{
   return curses_driver_read_window_key(curses_driver_current_active_window());
}

int curses_driver_read_standard_key(void)
{
   return curses_driver_translate_input_key(my_getch(stdscr));
}

int curses_driver_read_raw_window_key(WINDOW *win)
{
   if (win == NULL)
      return ERR;
   return curses_driver_translate_input_key(wgetch(win));
}

int curses_driver_read_raw_standard_key(void)
{
   return curses_driver_read_raw_window_key(stdscr);
}

#if defined(PDCURSES_MOUSE_ENABLED) || defined(NCURSES_MOUSE_VERSION)
static int curses_driver_last_mouse_col = -1;
static int curses_driver_last_mouse_row = -1;
#endif

#if defined(NCURSES_MOUSE_VERSION)
static MEVENT curses_driver_ncurses_mouse_event;
#endif

void curses_driver_reset_mouse_position(void)
{
#if defined(PDCURSES_MOUSE_ENABLED) || defined(NCURSES_MOUSE_VERSION)
   curses_driver_last_mouse_col = -1;
   curses_driver_last_mouse_row = -1;
#endif
}

void curses_driver_saved_mouse_position(int *row, int *col)
{
   if (row != NULL)
      *row = -1;
   if (col != NULL)
      *col = -1;
#if defined(PDCURSES_MOUSE_ENABLED) || defined(NCURSES_MOUSE_VERSION)
   if (row != NULL)
      *row = curses_driver_last_mouse_row;
   if (col != NULL)
      *col = curses_driver_last_mouse_col;
#endif
}

void curses_driver_mouse_position(WINDOW *win, int *row, int *col)
{
   CursesDriverWindowOrigin origin;
   CursesDriverWindowSize size;

   if (row != NULL)
      *row = -1;
   if (col != NULL)
      *col = -1;
   if (win == NULL || row == NULL || col == NULL)
      return;
#if defined(PDCURSES_MOUSE_ENABLED) || defined(NCURSES_MOUSE_VERSION)
   origin = curses_driver_window_origin(win);
   size = curses_driver_window_size(win);
   if (!origin.valid
   ||  !size.valid
   ||  curses_driver_last_mouse_row < origin.row
   ||  curses_driver_last_mouse_col < origin.col
   ||  curses_driver_last_mouse_row >= origin.row + size.rows
   ||  curses_driver_last_mouse_col >= origin.col + size.cols)
      return;
   *row = curses_driver_last_mouse_row - origin.row;
   *col = curses_driver_last_mouse_col - origin.col;
#else
   INTENTIONALLY_UNUSED_VARIABLE(origin);
   INTENTIONALLY_UNUSED_VARIABLE(size);
#endif
}

void curses_driver_mouse_position_for_screen_role(CHARTYPE scrno, short role,
                                                  int *row, int *col)
{
   curses_driver_mouse_position(curses_driver_screen_role_window(scrno, role),
                                row, col);
}

void curses_driver_mouse_position_for_global(CursesDriverGlobalWindowRole role,
                                             int *row, int *col)
{
   curses_driver_mouse_position(curses_driver_global_window(role), row, col);
}

int curses_driver_read_mouse_event(WINDOW *win, CursesDriverMouseEvent *event)
{
   int button = 0;
   int action = 0;
   int modifier = 0;

   if (event != NULL)
   {
      memset(event, 0, sizeof(*event));
      event->row = -1;
      event->col = -1;
   }
   if (event == NULL)
      return 0;
   if (!curses_driver_read_mouse_button(&button, &action, &modifier))
      return 0;
   event->button = button;
   event->modifier = modifier;
   event->valid = 1;
   if (action == CURSES_DRIVER_MOUSE_BUTTON_PRESSED)
      event->action = CURSES_DRIVER_MOUSE_ACTION_PRESSED;
   else if (action == CURSES_DRIVER_MOUSE_BUTTON_RELEASED)
      event->action = CURSES_DRIVER_MOUSE_ACTION_RELEASED;
   else if (action == CURSES_DRIVER_MOUSE_BUTTON_CLICKED)
      event->action = CURSES_DRIVER_MOUSE_ACTION_CLICKED;
   else
      event->action = CURSES_DRIVER_MOUSE_ACTION_OTHER;
   curses_driver_mouse_position(win, &event->row, &event->col);
   event->inside = event->row != -1 && event->col != -1;
   return 1;
}

#if defined(PDCURSES_MOUSE_ENABLED) || defined(NCURSES_MOUSE_VERSION)
static void curses_driver_clear_mouse_button(int *button, int *action,
                                             int *modifier)
{
   if (button != NULL)
      *button = 0;
   if (action != NULL)
      *action = 0;
   if (modifier != NULL)
      *modifier = 0;
}
#endif

#if defined(PDCURSES_MOUSE_ENABLED)
static int curses_driver_read_pdc_mouse_button(int *button, int *action,
                                               int *modifier)
{
   int rc = RC_OK;

   TRACE_FUNCTION("cursesdriver.c: curses_driver_read_pdc_mouse_button");
   request_mouse_pos();
   curses_driver_last_mouse_col = MOUSE_X_POS;
   curses_driver_last_mouse_row = MOUSE_Y_POS;
   mouse_trace_message("pdc-raw",
                       "x=%d y=%d changed=%d moved=%d",
                       MOUSE_X_POS, MOUSE_Y_POS, A_BUTTON_CHANGED,
                       MOUSE_MOVED);
   if (A_BUTTON_CHANGED)
   {
      if (BUTTON_CHANGED(1))
         *button = 1;
      else if (BUTTON_CHANGED(2))
         *button = 2;
      else if (BUTTON_CHANGED(3))
         *button = 3;
      else
      {
         TRACE_RETURN();
         return 1;
      }
      if (BUTTON_STATUS(*button) & BUTTON_SHIFT)
         *modifier = CURSES_DRIVER_MOUSE_MODIFIER_SHIFT;
# if defined(BUTTON_CONTROL)
      else if (BUTTON_STATUS(*button) & BUTTON_CONTROL)
         *modifier = CURSES_DRIVER_MOUSE_MODIFIER_CONTROL;
# elif defined(BUTTON_CTRL)
      else if (BUTTON_STATUS(*button) & BUTTON_CTRL)
         *modifier = CURSES_DRIVER_MOUSE_MODIFIER_CONTROL;
# endif
      else if (BUTTON_STATUS(*button) & BUTTON_ALT)
         *modifier = CURSES_DRIVER_MOUSE_MODIFIER_ALT;
      else
         *modifier = CURSES_DRIVER_MOUSE_MODIFIER_NONE;
      if (MOUSE_MOVED)
         *action = CURSES_DRIVER_MOUSE_BUTTON_MOVED;
      else
         *action = BUTTON_STATUS(*button) & BUTTON_ACTION_MASK;
   }
# if defined(MOUSE_WHEEL_UP) && defined(WHEEL_SCROLLED)
   else if (MOUSE_WHEEL_UP)
   {
      *action = CURSES_DRIVER_MOUSE_WHEEL_SCROLLED;
      *button = 4;
      *modifier = CURSES_DRIVER_MOUSE_MODIFIER_NONE;
   }
# endif
# if defined(MOUSE_WHEEL_DOWN) && defined(WHEEL_SCROLLED)
   else if (MOUSE_WHEEL_DOWN)
   {
      *action = CURSES_DRIVER_MOUSE_WHEEL_SCROLLED;
      *button = 5;
      *modifier = CURSES_DRIVER_MOUSE_MODIFIER_NONE;
   }
# endif
# if defined(MOUSE_WHEEL_LEFT) && defined(WHEEL_SCROLLED)
   else if (MOUSE_WHEEL_LEFT)
   {
      *action = CURSES_DRIVER_MOUSE_WHEEL_SCROLLED;
      *button = 6;
      *modifier = CURSES_DRIVER_MOUSE_MODIFIER_NONE;
   }
# endif
# if defined(MOUSE_WHEEL_RIGHT) && defined(WHEEL_SCROLLED)
   else if (MOUSE_WHEEL_RIGHT)
   {
      *action = CURSES_DRIVER_MOUSE_WHEEL_SCROLLED;
      *button = 7;
      *modifier = CURSES_DRIVER_MOUSE_MODIFIER_NONE;
   }
# endif
   else
   {
      curses_driver_clear_mouse_button(button, action, modifier);
      rc = RC_INVALID_OPERAND;
   }
   mouse_trace_message("pdc-decode",
                       "rc=%d button=%d action=%d modifier=%d x=%d y=%d",
                       rc, *button, *action, *modifier,
                       curses_driver_last_mouse_col,
                       curses_driver_last_mouse_row);
   TRACE_RETURN();
   return rc == RC_OK;
}
#endif

#if defined(NCURSES_MOUSE_VERSION)
static int curses_driver_read_ncurses_mouse_button(int *button, int *action,
                                                  int *modifier)
{
   int getmouse_rc = OK;
   int rc = RC_OK;

   TRACE_FUNCTION("cursesdriver.c: curses_driver_read_ncurses_mouse_button");
   getmouse_rc = getmouse(&curses_driver_ncurses_mouse_event);
   mouse_trace_message("ncurses-getmouse",
                       "rc=%d id=%ld x=%d y=%d z=%d bstate=0x%lx",
                       getmouse_rc,
                       (long)curses_driver_ncurses_mouse_event.id,
                       curses_driver_ncurses_mouse_event.x,
                       curses_driver_ncurses_mouse_event.y,
                       curses_driver_ncurses_mouse_event.z,
                       (unsigned long)curses_driver_ncurses_mouse_event.bstate);
   if (getmouse_rc != OK)
   {
      curses_driver_clear_mouse_button(button, action, modifier);
      mouse_trace_message("ncurses-decode",
                          "rc=%d button=%d action=%d modifier=%d",
                          RC_INVALID_OPERAND, *button, *action, *modifier);
      TRACE_RETURN();
      return 0;
   }

   curses_driver_last_mouse_col = curses_driver_ncurses_mouse_event.x;
   curses_driver_last_mouse_row = curses_driver_ncurses_mouse_event.y;

   if (curses_driver_ncurses_mouse_event.bstate & BUTTON1_RELEASED
   ||  curses_driver_ncurses_mouse_event.bstate & BUTTON1_PRESSED
   ||  curses_driver_ncurses_mouse_event.bstate & BUTTON1_CLICKED
   ||  curses_driver_ncurses_mouse_event.bstate & BUTTON1_DOUBLE_CLICKED)
      *button = 1;
   else if (curses_driver_ncurses_mouse_event.bstate & BUTTON2_RELEASED
        ||  curses_driver_ncurses_mouse_event.bstate & BUTTON2_PRESSED
        ||  curses_driver_ncurses_mouse_event.bstate & BUTTON2_CLICKED
        ||  curses_driver_ncurses_mouse_event.bstate & BUTTON2_DOUBLE_CLICKED)
      *button = 2;
   else if (curses_driver_ncurses_mouse_event.bstate & BUTTON3_RELEASED
        ||  curses_driver_ncurses_mouse_event.bstate & BUTTON3_PRESSED
        ||  curses_driver_ncurses_mouse_event.bstate & BUTTON3_CLICKED
        ||  curses_driver_ncurses_mouse_event.bstate & BUTTON3_DOUBLE_CLICKED)
      *button = 3;
   else
   {
      curses_driver_clear_mouse_button(button, action, modifier);
      mouse_trace_message("ncurses-decode",
                          "rc=%d button=%d action=%d modifier=%d",
                          RC_INVALID_OPERAND, *button, *action, *modifier);
      TRACE_RETURN();
      return 0;
   }

   if (curses_driver_ncurses_mouse_event.bstate & BUTTON_SHIFT)
      *modifier = CURSES_DRIVER_MOUSE_MODIFIER_SHIFT;
   else if (curses_driver_ncurses_mouse_event.bstate & BUTTON_CTRL)
      *modifier = CURSES_DRIVER_MOUSE_MODIFIER_CONTROL;
   else if (curses_driver_ncurses_mouse_event.bstate & BUTTON_ALT)
      *modifier = CURSES_DRIVER_MOUSE_MODIFIER_ALT;
   else
      *modifier = CURSES_DRIVER_MOUSE_MODIFIER_NONE;

   if (curses_driver_ncurses_mouse_event.bstate & BUTTON1_RELEASED
   ||  curses_driver_ncurses_mouse_event.bstate & BUTTON2_RELEASED
   ||  curses_driver_ncurses_mouse_event.bstate & BUTTON3_RELEASED)
      *action = CURSES_DRIVER_MOUSE_BUTTON_RELEASED;
   else if (curses_driver_ncurses_mouse_event.bstate & BUTTON1_PRESSED
        ||  curses_driver_ncurses_mouse_event.bstate & BUTTON2_PRESSED
        ||  curses_driver_ncurses_mouse_event.bstate & BUTTON3_PRESSED)
      *action = CURSES_DRIVER_MOUSE_BUTTON_PRESSED;
   else if (curses_driver_ncurses_mouse_event.bstate & BUTTON1_CLICKED
        ||  curses_driver_ncurses_mouse_event.bstate & BUTTON2_CLICKED
        ||  curses_driver_ncurses_mouse_event.bstate & BUTTON3_CLICKED)
      *action = CURSES_DRIVER_MOUSE_BUTTON_CLICKED;
   else if (curses_driver_ncurses_mouse_event.bstate & BUTTON1_DOUBLE_CLICKED
        ||  curses_driver_ncurses_mouse_event.bstate & BUTTON2_DOUBLE_CLICKED
        ||  curses_driver_ncurses_mouse_event.bstate & BUTTON3_DOUBLE_CLICKED)
      *action = CURSES_DRIVER_MOUSE_BUTTON_DOUBLE_CLICKED;

   mouse_trace_message("ncurses-decode",
                       "rc=%d button=%d action=%d modifier=%d x=%d y=%d",
                       rc, *button, *action, *modifier,
                       curses_driver_last_mouse_col,
                       curses_driver_last_mouse_row);
   TRACE_RETURN();
   return 1;
}
#endif

int curses_driver_read_mouse_button(int *button, int *action, int *modifier)
{
#if defined(PDCURSES_MOUSE_ENABLED) || defined(NCURSES_MOUSE_VERSION)
   int raw_button = 0;
   int raw_action = 0;
   int raw_modifier = 0;
   int ok = 0;

# if defined(PDCURSES_MOUSE_ENABLED)
   ok = curses_driver_read_pdc_mouse_button(&raw_button, &raw_action,
                                            &raw_modifier);
# elif defined(NCURSES_MOUSE_VERSION)
   ok = curses_driver_read_ncurses_mouse_button(&raw_button, &raw_action,
                                                &raw_modifier);
# endif
   if (!ok)
      return 0;
   if (button != NULL)
      *button = raw_button;
   if (action != NULL)
      *action = raw_action;
   if (modifier != NULL)
      *modifier = raw_modifier;
   return 1;
#else
   INTENTIONALLY_UNUSED_VARIABLE(button);
   INTENTIONALLY_UNUSED_VARIABLE(action);
   INTENTIONALLY_UNUSED_VARIABLE(modifier);
   return 0;
#endif
}

void curses_driver_prepare_standard_screen_for_shell(void)
{
   attrset(A_NORMAL);
   clear();
   wmove(stdscr, 1, 0);
   wrefresh(stdscr);
}

void curses_driver_force_background_and_refresh(WINDOW *win)
{
#if defined(HAVE_BROKEN_SYSVR4_CURSES)
   CursesDriverWindowCursor cursor;

   cursor = curses_driver_capture_window_cursor(win);
   force_curses_background();
   curses_driver_restore_window_cursor(win, cursor);
   curses_driver_refresh_standard_screen();
#else
   INTENTIONALLY_UNUSED_VARIABLE(win);
#endif
}

void curses_driver_clear_standard_window(void)
{
   wclear(stdscr);
}

void curses_driver_erase_standard_window(void)
{
   erase();
}

void curses_driver_set_standard_attr(chtype colour)
{
   attrset(colour);
}

void curses_driver_add_standard_string_at(short row, short col,
                                          const char *text)
{
   if (text == NULL)
      return;
   mvaddstr(row, col, text);
}

void curses_driver_move_standard_cursor(short row, short col)
{
   move(row, col);
}

void curses_driver_add_standard_ch(chtype ch)
{
   addch(ch);
}

#ifdef HAVE_WADDCHNSTR
void curses_driver_write_chtype_span(WINDOW *win, const chtype *text, int len)
{
   if (win == NULL || text == NULL || len <= 0)
      return;
   waddchnstr(win, text, len);
}

# ifdef USE_UTF8
void curses_driver_write_cchar_span(WINDOW *win, const cchar_t *text, int len)
{
   if (win == NULL || text == NULL || len <= 0)
      return;
   wadd_wchnstr(win, text, len);
}
# endif
#endif

void curses_driver_add_chtype(WINDOW *win, chtype ch)
{
   if (win == NULL)
      return;
   waddch(win, ch);
}

#ifdef USE_UTF8
void curses_driver_add_cchar(WINDOW *win, const cchar_t *ch)
{
   if (win == NULL || ch == NULL)
      return;
   wadd_wch(win, ch);
}
#endif

void curses_driver_redraw_window(WINDOW *win)
{
   short i;
   short j;
   chtype ch;
   CursesDriverWindowCursor cursor;

   if (win == NULL)
      return;

   cursor = curses_driver_capture_window_cursor(win);
   for (i = 0; i < getmaxx(win); i++)
   {
      for (j = 0; j < getmaxy(win); j++)
      {
         curses_driver_move_window_cursor(win, j, i);
         ch = curses_driver_read_window_cell(win);
#ifndef VMS
         ch &= A_CHARTEXT;
#endif
         put_char(win, ch, ADDCHAR);
      }
   }
   curses_driver_restore_window_cursor(win, cursor);
}

void curses_driver_redraw_current_role(short role)
{
   curses_driver_redraw_window(curses_driver_current_role_window(role));
}

void curses_driver_redraw_screen_role(CHARTYPE scrno, short role)
{
   curses_driver_redraw_window(curses_driver_screen_role_window(scrno, role));
}

void curses_driver_redraw_global_window(CursesDriverGlobalWindowRole role)
{
   curses_driver_redraw_window(curses_driver_global_window(role));
}

short curses_driver_refresh_cursor(CHARTYPE scrno)
{
   INTENTIONALLY_UNUSED_VARIABLE(scrno);
   show_statarea();
   curses_driver_update();
   curses_driver_present_cursor(TRUE);
   return RC_OK;
}

short curses_driver_redraw_screen_cursor(CHARTYPE scrno, struct view_details *view)
{
   if (!curses_started || view == NULL)
      return RC_OK;

   build_screen(scrno);
   display_screen(scrno);
   return curses_driver_refresh_cursor(scrno);
}

void curses_driver_move_prefix_cursor(CHARTYPE scrno, short row, short col)
{
   if (scrno >= MAX_SCREENS || SCREEN_WINDOW_PREFIX(scrno) == NULL)
      return;
   curses_driver_move_window_cursor(SCREEN_WINDOW_PREFIX(scrno), row, col);
}

CursesDriverCursorTarget curses_driver_filearea_target(
   LogicalCursor cursor, const CHARTYPE *line, size_t len,
   int viewport_col, int window_cols)
{
   CursesDriverCursorTarget target;

   target.logical = cursor;
   target.viewport_col = viewport_col;
   target.window_cols = window_cols;
   target.raw_display_col = 0;
   target.display_col = 0;
   target.visible = 0;
   if (!cursor.valid)
      return target;
   target.raw_display_col = curses_driver_display_col_from_logical(
      line, len, viewport_col, cursor.text.cell_column);
   target.visible = cursor.text.cell_column >= viewport_col
                 && (window_cols <= 0 || target.raw_display_col < window_cols);
   target.display_col = curses_driver_clamp_display_col(target.raw_display_col,
                                                        window_cols);
   return target;
}

short curses_driver_move_filearea_cursor(CHARTYPE scrno, struct view_details *view,
                                         const CHARTYPE *line, size_t len,
                                         short row, int logical_col)
{
   LogicalCursor cursor;
   CursesDriverCursorTarget target;

   if (view == NULL || SCREEN_WINDOW_FILEAREA(scrno) == NULL)
      return RC_OK;
   cursor = logical_cursor_from_cell(LOGICAL_CURSOR_ZONE_FILEAREA,
                                     view->focus_line, row, line, len,
                                     logical_col, TEXT_SNAP_BACKWARD, 1);
   target = curses_driver_filearea_target(cursor, line, len,
                                          (int)view->verify_col - 1,
                                          screen[scrno].cols[WINDOW_FILEAREA]);
   logical_cursor_state_focus(&view->logical_cursor, cursor);
   wmove(SCREEN_WINDOW_FILEAREA(scrno), row, target.display_col);
   return RC_OK;
}

short curses_driver_filearea_cursor_transition(CHARTYPE scrno,
                                               struct view_details *view,
                                               const CHARTYPE *line, size_t len,
                                               short old_row,
                                               int old_logical_cell,
                                               int new_logical_cell,
                                               LENGTHTYPE old_verify_col)
{
   short new_row = 0;
   short new_col = 0;
   int viewport_col;

   if (!current_cursor_uses_software())
      return RC_OK;

   getyx(SCREEN_WINDOW_FILEAREA(scrno), new_row, new_col);
   INTENTIONALLY_UNUSED_VARIABLE(new_col);
   if (view == NULL
   ||  view->verify_col != old_verify_col
   ||  new_row != old_row)
   {
      return curses_driver_redraw_screen_cursor(scrno, view);
   }

   viewport_col = (int)view->verify_col - 1;
#ifdef USE_UTF8
   show_utf8_filearea_cursor_transition(scrno, old_row,
                                        old_logical_cell - viewport_col,
                                        new_logical_cell - viewport_col);
#else
   INTENTIONALLY_UNUSED_VARIABLE(old_logical_cell);
#endif
   curses_driver_move_filearea_cursor(scrno, view, line, len,
                                      new_row, new_logical_cell);
   return curses_driver_refresh_cursor(scrno);
}

const TheDriverOps the_curses_driver_ops = {
   .clamp_display_col = curses_driver_clamp_display_col,
   .display_col_from_logical = curses_driver_display_col_from_logical,
   .logical_col_from_display = curses_driver_logical_col_from_display,
   .viewport_col_for_logical = curses_driver_viewport_col_for_logical,
   .software_cursor_attr = curses_driver_software_cursor_attr,
   .current_window_is_role = curses_driver_current_window_is_role,
   .current_window_exists = curses_driver_current_window_exists,
   .screen_window_is_role = curses_driver_screen_window_is_role,
   .current_role_exists = curses_driver_current_role_exists,
   .screen_role_exists = curses_driver_screen_role_exists,
   .global_window_exists = curses_driver_global_window_exists,
   .delete_global_window = curses_driver_delete_global_window,
   .capture_current_window_cursor =
      curses_driver_capture_current_window_cursor,
   .capture_current_previous_window_cursor =
      curses_driver_capture_current_previous_window_cursor,
   .capture_current_role_cursor = curses_driver_capture_current_role_cursor,
   .capture_screen_window_cursor = curses_driver_capture_screen_window_cursor,
   .capture_screen_role_cursor = curses_driver_capture_screen_role_cursor,
   .current_window_origin = curses_driver_current_window_origin,
   .current_window_size = curses_driver_current_window_size,
   .current_role_size = curses_driver_current_role_size,
   .screen_role_size = curses_driver_screen_role_size,
   .save_current_role_window = curses_driver_save_current_role_window,
   .restore_current_role_window = curses_driver_restore_current_role_window,
   .delete_current_role_window = curses_driver_delete_current_role_window,
   .clear_current_screen_roles = curses_driver_clear_current_screen_roles,
   .move_current_window_cursor = curses_driver_move_current_window_cursor,
   .move_current_previous_window_cursor =
      curses_driver_move_current_previous_window_cursor,
   .move_current_role_cursor = curses_driver_move_current_role_cursor,
   .move_screen_window_cursor = curses_driver_move_screen_window_cursor,
   .move_screen_role_cursor = curses_driver_move_screen_role_cursor,
   .restore_current_window_cursor =
      curses_driver_restore_current_window_cursor,
   .restore_current_role_cursor = curses_driver_restore_current_role_cursor,
   .read_current_window_cell = curses_driver_read_current_window_cell,
   .read_current_window_cell_attr_at =
      curses_driver_read_current_window_cell_attr_at,
   .put_char_current_window = curses_driver_put_char_current_window,
   .set_current_window_attr = curses_driver_set_current_window_attr,
   .set_current_role_attr = curses_driver_set_current_role_attr,
   .set_screen_role_attr = curses_driver_set_screen_role_attr,
   .set_global_window_attr = curses_driver_set_global_window_attr,
   .clear_current_role = curses_driver_clear_current_role,
   .clear_current_role_to_eol = curses_driver_clear_current_role_to_eol,
   .clear_screen_role_to_eol = curses_driver_clear_screen_role_to_eol,
   .touch_current_window = curses_driver_touch_current_window,
   .touch_current_role = curses_driver_touch_current_role,
   .touch_screen_role = curses_driver_touch_screen_role,
   .touch_global_window = curses_driver_touch_global_window,
   .touch_and_refresh_current_role =
      curses_driver_touch_and_refresh_current_role,
   .touch_and_refresh_screen_role =
      curses_driver_touch_and_refresh_screen_role,
   .touch_and_refresh_global_window =
      curses_driver_touch_and_refresh_global_window,
   .refresh_current_window = curses_driver_refresh_current_window,
   .refresh_current_window_now = curses_driver_refresh_current_window_now,
   .refresh_current_role = curses_driver_refresh_current_role,
   .refresh_current_role_now = curses_driver_refresh_current_role_now,
   .refresh_screen_window = curses_driver_refresh_screen_window,
   .refresh_screen_role = curses_driver_refresh_screen_role,
   .refresh_global_window = curses_driver_refresh_global_window,
   .refresh_global_window_now = curses_driver_refresh_global_window_now,
   .refresh_standard_screen = curses_driver_refresh_standard_screen,
   .update = curses_driver_update,
   .present_cursor = curses_driver_present_cursor,
   .add_global_string_at = curses_driver_add_global_string_at,
   .read_current_window_key = curses_driver_read_current_window_key,
   .read_standard_key = curses_driver_read_standard_key,
   .read_raw_standard_key = curses_driver_read_raw_standard_key,
   .is_mouse_key = curses_driver_is_mouse_key,
   .mouse_key_code = curses_driver_mouse_key_code,
   .mouse_position_for_screen_role =
      curses_driver_mouse_position_for_screen_role,
   .mouse_position_for_global = curses_driver_mouse_position_for_global,
   .saved_mouse_position = curses_driver_saved_mouse_position,
   .reset_mouse_position = curses_driver_reset_mouse_position,
   .read_mouse_button = curses_driver_read_mouse_button,
   .prepare_standard_screen_for_shell =
      curses_driver_prepare_standard_screen_for_shell,
   .clear_standard_window = curses_driver_clear_standard_window,
   .erase_standard_window = curses_driver_erase_standard_window,
   .set_standard_attr = curses_driver_set_standard_attr,
   .add_standard_string_at = curses_driver_add_standard_string_at,
   .move_standard_cursor = curses_driver_move_standard_cursor,
   .add_standard_ch = curses_driver_add_standard_ch,
   .redraw_current_role = curses_driver_redraw_current_role,
   .redraw_screen_role = curses_driver_redraw_screen_role,
   .redraw_global_window = curses_driver_redraw_global_window,
   .refresh_cursor = curses_driver_refresh_cursor,
   .redraw_screen_cursor = curses_driver_redraw_screen_cursor,
   .move_prefix_cursor = curses_driver_move_prefix_cursor,
   .filearea_target = curses_driver_filearea_target,
   .move_filearea_cursor = curses_driver_move_filearea_cursor,
   .filearea_cursor_transition = curses_driver_filearea_cursor_transition
};
