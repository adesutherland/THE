#include "cursesdriver.h"
#include "the.h"
#include "proto.h"
#include "driverlayout.h"
#include "curseskeymap.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef USE_UTF8
# include <wchar.h>
#endif

static int curses_driver_valid_screen(CHARTYPE scrno)
{
   return scrno < MAX_SCREENS;
}

static int curses_driver_valid_view_role(short role)
{
   return role >= 0 && role < VIEW_WINDOWS;
}

typedef struct
{
   int used;
   int fg;
   int bg;
   short pair;
} CursesDriverPairCacheEntry;

typedef struct
{
   int used;
   int logical;
   short physical;
} CursesDriverColourCacheEntry;

static CursesDriverPairCacheEntry *curses_driver_pair_cache = NULL;
static int curses_driver_pair_cache_count = 0;
static short curses_driver_next_pair = 1;
static CursesDriverColourCacheEntry *curses_driver_colour_cache = NULL;
static int curses_driver_colour_cache_count = 0;
static short curses_driver_next_colour = -1;

static int curses_driver_physical_pair_limit(void)
{
#ifdef A_COLOR
   return COLOR_PAIRS;
#else
   return 1;
#endif
}

static void curses_driver_reset_pair_cache(void)
{
   int limit = curses_driver_physical_pair_limit();

   free(curses_driver_pair_cache);
   curses_driver_pair_cache = NULL;
   curses_driver_pair_cache_count = 0;
   curses_driver_next_pair = 1;
   if (limit <= 1)
      return;
   curses_driver_pair_cache =
      (CursesDriverPairCacheEntry *)calloc((size_t)limit,
                                           sizeof(*curses_driver_pair_cache));
   if (curses_driver_pair_cache != NULL)
      curses_driver_pair_cache_count = limit;
}

static int curses_driver_physical_colour_limit(void)
{
#ifdef A_COLOR
   return COLORS;
#else
   return 16;
#endif
}

static void curses_driver_reset_colour_cache(void)
{
   int limit = curses_driver_physical_colour_limit();

   free(curses_driver_colour_cache);
   curses_driver_colour_cache = NULL;
   curses_driver_colour_cache_count = 0;
   curses_driver_next_colour = (short)(limit - 1);
   if (limit <= THE_RENDER_COLOR_RGB_FIRST)
      return;
   curses_driver_colour_cache =
      (CursesDriverColourCacheEntry *)calloc(
         (size_t)limit, sizeof(*curses_driver_colour_cache));
   if (curses_driver_colour_cache != NULL)
      curses_driver_colour_cache_count = limit;
}

static short curses_driver_physical_colour_for_logical(int colour)
{
#ifdef A_COLOR
   int i;
   int red;
   int green;
   int blue;
   short physical;

   if (colour < 0)
      return -1;
   if (!the_render_color_is_rgb(colour))
      return (short)colour;
   if (!colour_support || !can_change_color())
      return THE_COLOR_WHITE;
   if (curses_driver_colour_cache == NULL)
      curses_driver_reset_colour_cache();
   if (curses_driver_colour_cache == NULL)
      return THE_COLOR_WHITE;

   for (i = 0; i < curses_driver_colour_cache_count; i++)
   {
      if (curses_driver_colour_cache[i].used
      &&  curses_driver_colour_cache[i].logical == colour)
         return curses_driver_colour_cache[i].physical;
   }
   if (curses_driver_next_colour <= 15
   ||  !the_render_color_rgb(colour, &red, &green, &blue))
      return THE_COLOR_WHITE;

   physical = curses_driver_next_colour--;
   init_color(physical, (short)((red * 1000) / 255),
              (short)((green * 1000) / 255),
              (short)((blue * 1000) / 255));
   curses_driver_colour_cache[physical].used = 1;
   curses_driver_colour_cache[physical].logical = colour;
   curses_driver_colour_cache[physical].physical = physical;
   return physical;
#else
   INTENTIONALLY_UNUSED_VARIABLE(colour);
   return THE_COLOR_WHITE;
#endif
}

static short curses_driver_pair_for_colours(int fg, int bg)
{
#ifdef A_COLOR
   int i;
   short physical_fg;
   short physical_bg;
   short pair;

   if (!colour_support || fg < 0 || bg < 0)
      return 0;
   physical_fg = curses_driver_physical_colour_for_logical(fg);
   physical_bg = curses_driver_physical_colour_for_logical(bg);
   if (physical_fg < 0 || physical_bg < 0)
      return 0;
   if (curses_driver_pair_cache == NULL)
      curses_driver_reset_pair_cache();
   if (curses_driver_pair_cache == NULL)
      return 0;

   for (i = 1; i < curses_driver_next_pair; i++)
   {
      if (curses_driver_pair_cache[i].used
      &&  curses_driver_pair_cache[i].fg == physical_fg
      &&  curses_driver_pair_cache[i].bg == physical_bg)
         return curses_driver_pair_cache[i].pair;
   }
   if (curses_driver_next_pair >= curses_driver_pair_cache_count)
      return 0;

   pair = curses_driver_next_pair++;
   init_pair(pair, physical_fg, physical_bg);
   curses_driver_pair_cache[pair].used = 1;
   curses_driver_pair_cache[pair].fg = physical_fg;
   curses_driver_pair_cache[pair].bg = physical_bg;
   curses_driver_pair_cache[pair].pair = pair;
   return pair;
#else
   INTENTIONALLY_UNUSED_VARIABLE(fg);
   INTENTIONALLY_UNUSED_VARIABLE(bg);
   return 0;
#endif
}

static short curses_driver_pair_for_attr(TheRenderAttr attr)
{
   if (!the_render_attr_has_color(attr))
      return 0;
   return curses_driver_pair_for_colours(the_render_attr_fg(attr),
                                         the_render_attr_bg(attr));
}

static chtype curses_driver_style_to_attr(TheRenderStyle style)
{
   chtype out = A_NORMAL;

#ifdef A_BOLD
   if (style & THE_STYLE_BOLD)
      out |= A_BOLD;
#endif
#ifdef A_REVERSE
   if (style & THE_STYLE_REVERSE)
      out |= A_REVERSE;
#endif
#ifdef A_UNDERLINE
   if (style & THE_STYLE_UNDERLINE)
      out |= A_UNDERLINE;
#endif
#ifdef A_BLINK
   if (style & THE_STYLE_BLINK)
      out |= A_BLINK;
#endif
#ifdef A_DIM
   if (style & THE_STYLE_DIM)
      out |= A_DIM;
#endif
#ifdef A_ITALIC
   if (style & THE_STYLE_ITALIC)
      out |= A_ITALIC;
#endif
#ifdef A_RIGHTLINE
   if (style & THE_STYLE_RIGHTLINE)
      out |= A_RIGHTLINE;
#endif
#ifdef A_LEFTLINE
   if (style & THE_STYLE_LEFTLINE)
      out |= A_LEFTLINE;
#endif
#ifdef A_TOPLINE
   if (style & THE_STYLE_TOPLINE)
      out |= A_TOPLINE;
#endif
#ifdef A_OVERLINE
   if (style & THE_STYLE_OVERLINE)
      out |= A_OVERLINE;
#endif
#ifdef A_STRIKEOUT
   if (style & THE_STYLE_STRIKEOUT)
      out |= A_STRIKEOUT;
#endif
   return out;
}

static chtype curses_driver_lower_attr(TheRenderAttr attr)
{
   chtype out = curses_driver_style_to_attr(the_render_attr_style(attr));
   short pair = curses_driver_pair_for_attr(attr);

#ifdef A_COLOR
   if (pair > 0)
      out |= COLOR_PAIR(pair);
#else
   INTENTIONALLY_UNUSED_VARIABLE(pair);
#endif
   return out;
}

static chtype curses_driver_physical_alternate_cell(uint32_t fallback)
{
   switch (fallback)
   {
      case '^':
#ifdef ACS_UARROW
         return ACS_UARROW;
#else
         return '^';
#endif
      case 'v':
#ifdef ACS_DARROW
         return ACS_DARROW;
#else
         return 'v';
#endif
      case '<':
#ifdef ACS_LARROW
         return ACS_LARROW;
#else
         return '<';
#endif
      case '>':
#ifdef ACS_RARROW
         return ACS_RARROW;
#else
         return '>';
#endif
      case '|':
      default:
#ifdef ACS_VLINE
         return ACS_VLINE;
#else
         return '|';
#endif
   }
}

static chtype curses_driver_lower_cell(TheDriverCell cell)
{
   chtype ch = the_driver_cell_is_alternate(cell)
             ? curses_driver_physical_alternate_cell(
                  the_driver_cell_codepoint(cell))
             : (chtype)the_driver_cell_codepoint(cell);

   return ch | curses_driver_lower_attr(the_driver_cell_attr(cell));
}

static chtype curses_driver_lower_codepoint(uint32_t codepoint,
                                            TheRenderAttr attr)
{
   return (chtype)codepoint | curses_driver_lower_attr(attr);
}

static WINDOW *curses_driver_window_from_driver(TheDriverWindow *win)
{
   return (WINDOW *)win;
}

static TheDriverWindow *curses_driver_window_to_driver(WINDOW *win)
{
   return (TheDriverWindow *)win;
}

static TheDriverWindow **curses_driver_screen_role_window_slot(CHARTYPE scrno,
                                                              short role)
{
   if (!curses_driver_valid_screen(scrno)
   ||  !curses_driver_valid_view_role(role))
      return NULL;
   return &screen[scrno].win[role];
}

static WINDOW *curses_driver_screen_role_window(CHARTYPE scrno, short role)
{
   TheDriverWindow **slot = curses_driver_screen_role_window_slot(scrno, role);

   return (slot == NULL) ? NULL : curses_driver_window_from_driver(*slot);
}

static WINDOW *curses_driver_screen_active_window(CHARTYPE scrno)
{
   VIEW_DETAILS *view;

   if (!curses_driver_valid_screen(scrno))
      return NULL;
   view = screen[scrno].screen_view;
   if (view == NULL || !curses_driver_valid_view_role(view->current_window))
      return NULL;
   return curses_driver_window_from_driver(
      screen[scrno].win[view->current_window]);
}

static WINDOW *curses_driver_screen_previous_window(CHARTYPE scrno)
{
   VIEW_DETAILS *view;

   if (!curses_driver_valid_screen(scrno))
      return NULL;
   view = screen[scrno].screen_view;
   if (view == NULL || !curses_driver_valid_view_role(view->previous_window))
      return NULL;
   return curses_driver_window_from_driver(
      screen[scrno].win[view->previous_window]);
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
         return curses_driver_window_from_driver(statarea);
      case CURSES_DRIVER_GLOBAL_ERROR:
         return curses_driver_window_from_driver(error_window);
      case CURSES_DRIVER_GLOBAL_DIVIDER:
         return curses_driver_window_from_driver(divider);
      case CURSES_DRIVER_GLOBAL_FILETABS:
         return curses_driver_window_from_driver(filetabs);
   }
   return NULL;
}

static TheDriverWindow **curses_driver_global_window_slot(
   CursesDriverGlobalWindowRole role)
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

static CursorShape curses_driver_current_cursor_shape(void)
{
   return INSERTMODEx ? cursorstyle_insert_shape : cursorstyle_over_shape;
}

static CursorBlink curses_driver_current_cursor_blink(void)
{
   return INSERTMODEx ? cursorstyle_insert_blink : cursorstyle_over_blink;
}

static CursorPresentation curses_driver_current_cursor_presentation(void)
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

static bool curses_driver_current_cursor_uses_software(void)
{
   return curses_driver_current_cursor_presentation()
       == CURSOR_PRESENTATION_SOFTWARE;
}

static void curses_driver_apply_cursor_visibility(bool visible)
{
   TRACE_FUNCTION("cursesdriver.c: curses_driver_apply_cursor_visibility");
#ifdef HAVE_CURS_SET
   if (visible)
   {
      CursorShape shape;
      CursorBlink blink;

      if (curses_driver_current_cursor_uses_software())
      {
         curs_set(0);
         TRACE_RETURN();
         return;
      }

      shape = curses_driver_current_cursor_shape();
      blink = curses_driver_current_cursor_blink();

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

TheDriverAttr curses_driver_software_cursor_attr(CHARTYPE scrno,
                                                 TheDriverAttr base,
                                                 CursorShape shape)
{
   if (shape == CURSOR_BLOCK)
      return set_colour(SCREEN_FILE(scrno)->attr+ATTR_CBLOCK);
   return the_render_attr_merge_style(base, THE_STYLE_UNDERLINE);
}

void curses_driver_draw_software_chtype_cell(CHARTYPE scrno, WINDOW *win,
                                             short row, int col,
                                             TheDriverCell base,
                                             CursorShape shape)
{
   chtype cell;
   chtype ch;
   chtype attrs;
   chtype cursor_attrs;
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
   attrs = cell & A_ATTRIBUTES;
   if (ch == 0)
      ch = ' ';
   if (attrs == 0)
      attrs = curses_driver_lower_attr(the_driver_cell_attr(base));
   if (shape == CURSOR_BLOCK)
      cursor_attrs = curses_driver_lower_attr(
         curses_driver_software_cursor_attr(scrno, the_driver_cell_attr(base),
                                            shape));
   else
   {
      cursor_attrs = attrs;
#ifdef A_UNDERLINE
      cursor_attrs |= A_UNDERLINE;
#endif
   }
   wattrset(win, cursor_attrs);
   mvwaddch(win, row, col, ch);
   wattrset(win, attrs);
}

void curses_driver_draw_software_blank_cell(CHARTYPE scrno, WINDOW *win,
                                            short row, int col,
                                            TheDriverAttr base,
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

   wattrset(win, curses_driver_lower_attr(
      curses_driver_software_cursor_attr(scrno, base, shape)));
   mvwaddch(win, row, col, ' ');
   wattrset(win, curses_driver_lower_attr(base));
}

#ifdef USE_UTF8
void curses_driver_write_render_wchars_at(WINDOW *win, int row, int col,
                                          const wchar_t *text,
                                          TheDriverAttr colour,
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
   wattrset(win, curses_driver_lower_attr(colour));
   waddwstr(win, text);

   next_col = col + ((expected_width > 0) ? expected_width : 1);
   if (next_col >= maxx)
      next_col = maxx - 1;
   if (next_col >= 0)
      wmove(win, row, next_col);
   wattrset(win, curses_driver_lower_attr(THE_RENDER_ATTR_NORMAL));
}

void curses_driver_fill_cells_at(WINDOW *win, int row, int col, int width,
                                 TheDriverAttr colour)
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

   wattrset(win, curses_driver_lower_attr(colour));
   wmove(win, row, col);
   for (i = 0; i < width; i++)
      waddch(win, ' ');
   wattrset(win, curses_driver_lower_attr(THE_RENDER_ATTR_NORMAL));
}

void curses_driver_write_ascii_cells_at(WINDOW *win, int row, int col,
                                        const char *text, int width,
                                        TheDriverAttr colour)
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

   wattrset(win, curses_driver_lower_attr(colour));
   wmove(win, row, col);
   for (i = 0; i < width && text[i] != '\0'; i++)
      waddch(win, (unsigned char)text[i]);
   wattrset(win, curses_driver_lower_attr(THE_RENDER_ATTR_NORMAL));
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
   WINDOW *win = newwin(rows, cols, row, col);

#ifdef HAVE_KEYPAD
   if (win != NULL)
      keypad(win, TRUE);
#endif
   return win;
}

void curses_driver_delete_window(WINDOW *win)
{
   if (win == NULL)
      return;
   delwin(win);
}

void curses_driver_configure_standard_input(bool keypad_enabled,
                                            bool notimeout_enabled)
{
#ifdef HAVE_KEYPAD
   keypad(stdscr, keypad_enabled ? TRUE : FALSE);
#else
   INTENTIONALLY_UNUSED_VARIABLE(keypad_enabled);
#endif
#ifdef HAVE_NOTIMEOUT
   notimeout(stdscr, notimeout_enabled ? TRUE : FALSE);
#else
   INTENTIONALLY_UNUSED_VARIABLE(notimeout_enabled);
#endif
}

void curses_driver_set_driver_window_leaveok(TheDriverWindow *win,
                                             bool enabled)
{
   WINDOW *curses_win = curses_driver_window_from_driver(win);

   if (curses_win != NULL)
      leaveok(curses_win, enabled ? TRUE : FALSE);
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

chtype curses_driver_read_window_cell(WINDOW *win)
{
   if (win == NULL)
      return 0;
   return (chtype)winch(win);
}

void curses_driver_set_window_attr(WINDOW *win, TheDriverAttr colour)
{
   if (win == NULL)
      return;
   wattrset(win, curses_driver_lower_attr(colour));
}

void curses_driver_set_current_role_attr(short role, TheDriverAttr colour)
{
   curses_driver_set_window_attr(curses_driver_current_role_window(role),
                                 colour);
}

void curses_driver_set_screen_role_attr(CHARTYPE scrno, short role,
                                        TheDriverAttr colour)
{
   curses_driver_set_window_attr(curses_driver_screen_role_window(scrno, role),
                                 colour);
}

void curses_driver_set_global_window_attr(CursesDriverGlobalWindowRole role,
                                          TheDriverAttr colour)
{
   curses_driver_set_window_attr(curses_driver_global_window(role), colour);
}

void curses_driver_set_window_background(WINDOW *win, TheDriverAttr colour)
{
   chtype physical = curses_driver_lower_attr(colour);

   if (win == NULL)
      return;
#ifdef HAVE_WBKGD
   wbkgd(win, physical);
#else
   wattrset(win, physical);
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
   my_wclrtoeol(curses_driver_window_to_driver(win));
}

void curses_driver_clear_current_role_to_eol(short role)
{
   curses_driver_clear_to_eol(curses_driver_current_role_window(role));
}

void curses_driver_touch_window(WINDOW *win)
{
   if (win == NULL)
      return;
   touchwin(win);
}

void curses_driver_touch_line(WINDOW *win, int start, int count)
{
   if (win == NULL)
      return;
   touchline(win, start, count);
}

void curses_driver_clear_line_at(WINDOW *win, short row, TheDriverAttr colour)
{
   if (win == NULL)
      return;
   curses_driver_move_window_cursor(win, row, 0);
   curses_driver_set_window_attr(win, colour);
   my_wclrtoeol(curses_driver_window_to_driver(win));
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

void curses_driver_sync_terminal_screen(void)
{
   refresh();
}

void curses_driver_clear_terminal_screen(void)
{
   attrset(curses_driver_lower_attr(THE_RENDER_ATTR_NORMAL));
   clear();
   move(0, 0);
}

void curses_driver_begin_terminal_report(void)
{
   wclear(stdscr);
   attrset(curses_driver_lower_attr(THE_RENDER_ATTR_NORMAL));
}

void curses_driver_write_terminal_report_text(short row, short col,
                                              TheDriverAttr attr,
                                              const char *text, size_t len)
{
   size_t i;

   if (text == NULL)
      return;
   attrset(curses_driver_lower_attr(attr));
   move(row, col);
   for (i = 0; i < len; i++)
      addch((chtype)(unsigned char)text[i]);
}

void curses_driver_end_terminal_report(void)
{
   attrset(curses_driver_lower_attr(THE_RENDER_ATTR_NORMAL));
   refresh();
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

void curses_driver_set_current_window_timeout(int milliseconds)
{
   curses_driver_set_window_timeout(curses_driver_current_active_window(),
                                    milliseconds);
}

void curses_driver_draw_box(WINDOW *win)
{
   if (win == NULL)
      return;
#if defined(HAVE_BOX)
   box(win, 0, 0);
#endif
}

void curses_driver_draw_vertical_line(WINDOW *win, TheDriverCell ch, int len)
{
   chtype physical = curses_driver_lower_cell(ch);
#ifndef HAVE_WVLINE
   CursesDriverWindowCursor cursor;
   int i;
#endif

   if (win == NULL || len <= 0)
      return;
#ifdef HAVE_WVLINE
   wvline(win, physical, len);
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

void curses_driver_add_chtype_at(WINDOW *win, short row, short col,
                                 TheDriverCell ch)
{
   if (win == NULL)
      return;
   curses_driver_move_window_cursor(win, row, col);
   waddch(win, curses_driver_lower_cell(ch));
}

void curses_driver_draw_horizontal_line(WINDOW *win, TheDriverCell ch, int len)
{
   int i;
   chtype physical = curses_driver_lower_cell(ch);

   if (win == NULL || len <= 0)
      return;
#ifdef HAVE_WHLINE
   whline(win, physical, len);
#else
   for (i = 0; i < len; i++)
      waddch(win, physical);
#endif
}

static int curses_driver_translate_input_key(int key)
{
#define MAP_DRIVER_KEY(physical, logical) \
   do { if (key == (physical)) return (logical); } while (0);

   if (key >= KEY_F(1) && key <= KEY_F(63))
      return THE_KEY_F(key - KEY_F0);

   THE_CURSES_KEY_TRANSLATION_MAP(MAP_DRIVER_KEY)

#undef MAP_DRIVER_KEY
   return key;
}

static int curses_driver_read_window_key(WINDOW *win)
{
   if (win == NULL)
      return ERR;
#if defined(PDCURSES) && !defined(USE_XCURSES)
   return curses_driver_translate_input_key(wgetch(win));
#else
   return curses_driver_translate_input_key(
      my_getch(curses_driver_window_to_driver(win)));
#endif
}

static int curses_driver_read_current_window_key(void)
{
   return curses_driver_read_window_key(curses_driver_current_active_window());
}

static int curses_driver_read_raw_window_key(WINDOW *win)
{
   if (win == NULL)
      return ERR;
   return wgetch(win);
}

int curses_driver_read_raw_driver_window_key(TheDriverWindow *win)
{
   return curses_driver_read_raw_window_key(
      curses_driver_window_from_driver(win));
}

int curses_driver_read_input_event(TheInputEvent *event)
{
   int key;

   if (event == NULL)
      return 0;
   *event = the_input_event_none();
   key = curses_driver_read_current_window_key();
   if (key == ERR)
      return 0;
   return the_input_event_from_legacy_key(key, event);
}

int curses_driver_read_terminal_legacy_key(void)
{
   return curses_driver_read_window_key(stdscr);
}

#if defined(PDCURSES_MOUSE_ENABLED) || defined(NCURSES_MOUSE_VERSION)
static int curses_driver_last_mouse_col = -1;
static int curses_driver_last_mouse_row = -1;
#endif

#if defined(NCURSES_MOUSE_VERSION)
static MEVENT curses_driver_ncurses_mouse_event;
#endif

void curses_driver_clear_mouse_packet_position(void)
{
#if defined(PDCURSES_MOUSE_ENABLED) || defined(NCURSES_MOUSE_VERSION)
   curses_driver_last_mouse_col = -1;
   curses_driver_last_mouse_row = -1;
#endif
}

void curses_driver_current_mouse_screen_position(int *row, int *col)
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

static void curses_driver_mouse_position(WINDOW *win, int *row, int *col)
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

void curses_driver_current_mouse_screen_role_position(CHARTYPE scrno,
                                                      short role,
                                                      int *row, int *col)
{
   curses_driver_mouse_position(curses_driver_screen_role_window(scrno, role),
                                row, col);
}

void curses_driver_current_mouse_global_position(
   CursesDriverGlobalWindowRole role, int *row, int *col)
{
   curses_driver_mouse_position(curses_driver_global_window(role), row, col);
}

static int curses_driver_read_mouse_event(WINDOW *win,
                                          CursesDriverMouseEvent *event)
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
   if (!curses_driver_read_pending_mouse_button(&button, &action, &modifier))
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

int curses_driver_read_transient_mouse_event(TheDriverWindow *win,
                                             TheDriverMouseEvent *event)
{
   return curses_driver_read_mouse_event(curses_driver_window_from_driver(win),
                                         event);
}

int curses_driver_read_current_role_transient_mouse_event(
   short role, TheDriverMouseEvent *event)
{
   return curses_driver_read_mouse_event(curses_driver_current_role_window(role),
                                         event);
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

int curses_driver_read_pending_mouse_button(int *button, int *action,
                                            int *modifier)
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

void curses_driver_prepare_for_shell_escape(void)
{
   attrset(curses_driver_lower_attr(THE_RENDER_ATTR_NORMAL));
   clear();
   wmove(stdscr, 1, 0);
   wrefresh(stdscr);
}

static void curses_driver_force_terminal_background(void)
{
#if defined(HAVE_BROKEN_SYSVR4_CURSES)
   short fg = 0;
   short bg = 0;

   if (colour_support)
   {
      pair_content(1, &fg, &bg);
      init_pair(1, COLOR_BLACK, COLOR_WHITE);
      move(0, 0);
      attrset(COLOR_PAIR(1));
      addch(' ');
      init_pair(1, fg, bg);
   }
#endif
}

void curses_driver_repair_terminal_background(
   TheDriverTerminalRepairTarget target)
{
#if defined(HAVE_BROKEN_SYSVR4_CURSES)
   CursesDriverWindowCursor cursor;
   WINDOW *win;

   win = target == THE_DRIVER_REPAIR_TERMINAL_SCREEN
       ? stdscr
       : curses_driver_current_active_window();
   cursor = curses_driver_capture_window_cursor(win);
   curses_driver_force_terminal_background();
   curses_driver_restore_window_cursor(win, cursor);
   curses_driver_sync_terminal_screen();
#else
   INTENTIONALLY_UNUSED_VARIABLE(target);
#endif
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

void curses_driver_add_chtype(WINDOW *win, TheDriverCell ch)
{
   if (win == NULL)
      return;
   waddch(win, curses_driver_lower_cell(ch));
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
         ch &= A_CHARTEXT;
         put_char(curses_driver_window_to_driver(win),
                  the_driver_cell_make((uint32_t)ch, THE_RENDER_ATTR_NORMAL),
                  ADDCHAR);
      }
   }
   curses_driver_restore_window_cursor(win, cursor);
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
   WINDOW *win = curses_driver_screen_role_window(scrno, WINDOW_PREFIX);

   if (scrno >= MAX_SCREENS || win == NULL)
      return;
   curses_driver_move_window_cursor(win, row, col);
}

short curses_driver_move_filearea_cursor(CHARTYPE scrno, struct view_details *view,
                                         const CHARTYPE *line, size_t len,
                                         short row, int logical_col)
{
   LogicalCursor cursor;
   TheDriverCursorTarget target;
   WINDOW *win = curses_driver_screen_role_window(scrno, WINDOW_FILEAREA);

   if (view == NULL || win == NULL)
      return RC_OK;
   cursor = logical_cursor_from_cell(LOGICAL_CURSOR_ZONE_FILEAREA,
                                     view->focus_line, row, line, len,
                                     logical_col, TEXT_SNAP_BACKWARD, 1);
   logical_cursor_set_desired_cell(
      &cursor, driver_layout_width_col_from_logical(line, len,
                                                    cursor.text.cell_column));
   target = driver_layout_filearea_target(cursor, line, len,
                                          (int)view->verify_col - 1,
                                          screen[scrno].cols[WINDOW_FILEAREA]);
   logical_cursor_state_focus(&view->logical_cursor, cursor);
   wmove(win, row, target.display_col);
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
   WINDOW *win = curses_driver_screen_role_window(scrno, WINDOW_FILEAREA);

   if (!curses_driver_current_cursor_uses_software())
      return RC_OK;
   if (win == NULL)
      return RC_OK;

   getyx(win, new_row, new_col);
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

static TheDriverWindow *curses_driver_ops_create_window(int rows, int cols,
                                                        int row, int col)
{
   return curses_driver_window_to_driver(
      curses_driver_create_window(rows, cols, row, col));
}

static void curses_driver_ops_delete_window(TheDriverWindow *win)
{
   curses_driver_delete_window(curses_driver_window_from_driver(win));
}

static TheDriverWindowCursor curses_driver_ops_capture_window_cursor(
   TheDriverWindow *win)
{
   return curses_driver_capture_window_cursor(
      curses_driver_window_from_driver(win));
}

static TheDriverWindowOrigin curses_driver_ops_window_origin(
   TheDriverWindow *win)
{
   return curses_driver_window_origin(curses_driver_window_from_driver(win));
}

static TheDriverWindowSize curses_driver_ops_window_size(TheDriverWindow *win)
{
   return curses_driver_window_size(curses_driver_window_from_driver(win));
}

static void curses_driver_ops_move_window_cursor(TheDriverWindow *win,
                                                 short row, short col)
{
   curses_driver_move_window_cursor(curses_driver_window_from_driver(win),
                                    row, col);
}

static void curses_driver_ops_restore_window_cursor(
   TheDriverWindow *win, TheDriverWindowCursor cursor)
{
   curses_driver_restore_window_cursor(curses_driver_window_from_driver(win),
                                       cursor);
}

static TheDriverAttr curses_driver_ops_software_cursor_attr(
   CHARTYPE scrno, TheDriverAttr base, CursorShape shape)
{
   return curses_driver_software_cursor_attr(scrno, base, shape);
}

static void curses_driver_ops_set_window_attr(TheDriverWindow *win,
                                              TheDriverAttr colour)
{
   curses_driver_set_window_attr(curses_driver_window_from_driver(win),
                                 colour);
}

static void curses_driver_ops_set_current_role_attr(short role,
                                                    TheDriverAttr colour)
{
   curses_driver_set_current_role_attr(role, colour);
}

static void curses_driver_ops_set_screen_role_attr(CHARTYPE scrno, short role,
                                                   TheDriverAttr colour)
{
   curses_driver_set_screen_role_attr(scrno, role, colour);
}

static void curses_driver_ops_set_global_window_attr(
   TheDriverGlobalWindowRole role, TheDriverAttr colour)
{
   curses_driver_set_global_window_attr(role, colour);
}

static void curses_driver_ops_set_window_background(TheDriverWindow *win,
                                                    TheDriverAttr colour)
{
   curses_driver_set_window_background(curses_driver_window_from_driver(win),
                                       colour);
}

static void curses_driver_ops_clear_line_at(TheDriverWindow *win, short row,
                                            TheDriverAttr colour)
{
   curses_driver_clear_line_at(curses_driver_window_from_driver(win), row,
                               colour);
}

static void curses_driver_ops_touch_window(TheDriverWindow *win)
{
   curses_driver_touch_window(curses_driver_window_from_driver(win));
}

static void curses_driver_ops_touch_line(TheDriverWindow *win, int start,
                                         int count)
{
   curses_driver_touch_line(curses_driver_window_from_driver(win), start,
                            count);
}

static void curses_driver_ops_refresh_window(TheDriverWindow *win)
{
   curses_driver_refresh_window(curses_driver_window_from_driver(win));
}

static void curses_driver_ops_refresh_window_now(TheDriverWindow *win)
{
   curses_driver_refresh_window_now(curses_driver_window_from_driver(win));
}

static void curses_driver_ops_draw_box(TheDriverWindow *win)
{
   curses_driver_draw_box(curses_driver_window_from_driver(win));
}

static void curses_driver_ops_draw_vertical_line(TheDriverWindow *win,
                                                 TheDriverCell ch, int len)
{
   curses_driver_draw_vertical_line(curses_driver_window_from_driver(win),
                                    ch, len);
}

static void curses_driver_ops_add_string_at(TheDriverWindow *win, short row,
                                            short col, const char *text)
{
   curses_driver_add_string_at(curses_driver_window_from_driver(win), row, col,
                               text);
}

static void curses_driver_ops_add_cell_at(TheDriverWindow *win, short row,
                                          short col, TheDriverCell ch)
{
   curses_driver_add_chtype_at(curses_driver_window_from_driver(win), row, col,
                               ch);
}

static void curses_driver_ops_draw_horizontal_line(TheDriverWindow *win,
                                                   TheDriverCell ch, int len)
{
   curses_driver_draw_horizontal_line(curses_driver_window_from_driver(win),
                                      ch, len);
}

static void curses_driver_ops_add_cell(TheDriverWindow *win, TheDriverCell ch)
{
   curses_driver_add_chtype(curses_driver_window_from_driver(win),
                            ch);
}

static void curses_driver_ops_insert_cell(TheDriverWindow *win,
                                          TheDriverCell ch)
{
   WINDOW *curses_win = curses_driver_window_from_driver(win);

   if (curses_win != NULL)
      winsch(curses_win, curses_driver_lower_cell(ch));
}

static void curses_driver_ops_delete_cell(TheDriverWindow *win)
{
   WINDOW *curses_win = curses_driver_window_from_driver(win);

   if (curses_win != NULL)
      wdelch(curses_win);
}

static void curses_driver_ops_write_cell_span(TheDriverWindow *win,
                                              const TheDriverCell *text,
                                              int len)
{
   WINDOW *curses_win = curses_driver_window_from_driver(win);
   int i;

   if (curses_win == NULL || text == NULL || len <= 0)
      return;
#ifdef HAVE_WADDCHNSTR
   {
      chtype *buf = (chtype *)malloc((size_t)len * sizeof(*buf));

      if (buf != NULL)
      {
         for (i = 0; i < len; i++)
            buf[i] = curses_driver_lower_cell(text[i]);
         curses_driver_write_chtype_span(curses_win, buf, len);
         free(buf);
         return;
      }
   }
#endif
   for (i = 0; i < len; i++)
      curses_driver_add_chtype(curses_win, text[i]);
}

static int curses_driver_render_cluster_to_cchar(cchar_t *dest,
                                                const TheRenderCluster *cluster)
{
#ifdef USE_UTF8
   wchar_t wch[THE_RENDER_MAX_CODEPOINTS * 2 + 3];
   short pair;
   chtype attrs;

   if (dest == NULL || cluster == NULL)
      return 0;
   if (!the_render_cluster_to_wchars(cluster, wch,
                                     sizeof(wch) / sizeof(wch[0])))
      return 0;
   pair = curses_driver_pair_for_attr(cluster->attr);
   attrs = curses_driver_style_to_attr(the_render_attr_style(cluster->attr));
   setcchar(dest, wch, (attr_t)attrs, pair, NULL);
   return 1;
#else
   INTENTIONALLY_UNUSED_VARIABLE(dest);
   INTENTIONALLY_UNUSED_VARIABLE(cluster);
   return 0;
#endif
}

static void curses_driver_ops_write_render_cells(TheDriverWindow *win,
                                                 const TheRenderCell *text,
                                                 int len)
{
#ifdef USE_UTF8
   WINDOW *curses_win = curses_driver_window_from_driver(win);
   int i;

   if (curses_win == NULL || text == NULL || len <= 0)
      return;
# ifdef HAVE_WADDCHNSTR
   {
      cchar_t *buf = (cchar_t *)malloc((size_t)len * sizeof(*buf));

      if (buf != NULL)
      {
         int ok = 1;

         for (i = 0; i < len; i++)
            ok = curses_driver_render_cluster_to_cchar(&buf[i], &text[i])
              && ok;
         if (ok)
         {
            curses_driver_write_cchar_span(curses_win, buf, len);
            free(buf);
            return;
         }
         free(buf);
      }
   }
# endif
   for (i = 0; i < len; i++)
   {
      cchar_t cell;

      if (curses_driver_render_cluster_to_cchar(&cell, &text[i]))
         curses_driver_add_cchar(curses_win, &cell);
   }
#else
   INTENTIONALLY_UNUSED_VARIABLE(win);
   INTENTIONALLY_UNUSED_VARIABLE(text);
   INTENTIONALLY_UNUSED_VARIABLE(len);
#endif
}

static void curses_driver_ops_write_render_cluster_at(
   TheDriverWindow *win, int row, int col, const TheRenderCluster *cluster)
{
#ifdef USE_UTF8
   wchar_t wch[THE_RENDER_MAX_CODEPOINTS * 2 + 3];

   if (cluster == NULL)
      return;
   if (!the_render_cluster_to_wchars(cluster, wch,
                                     sizeof(wch) / sizeof(wch[0])))
      return;
   curses_driver_write_render_wchars_at(curses_driver_window_from_driver(win),
                                        row, col, wch, cluster->attr,
                                        cluster->advance_width);
#else
   INTENTIONALLY_UNUSED_VARIABLE(win);
   INTENTIONALLY_UNUSED_VARIABLE(row);
   INTENTIONALLY_UNUSED_VARIABLE(col);
   INTENTIONALLY_UNUSED_VARIABLE(cluster);
#endif
}

static void curses_driver_ops_fill_cells_at(TheDriverWindow *win, int row,
                                            int col, int width,
                                            TheDriverAttr colour)
{
#ifdef USE_UTF8
   curses_driver_fill_cells_at(curses_driver_window_from_driver(win), row, col,
                               width, colour);
#else
   INTENTIONALLY_UNUSED_VARIABLE(win);
   INTENTIONALLY_UNUSED_VARIABLE(row);
   INTENTIONALLY_UNUSED_VARIABLE(col);
   INTENTIONALLY_UNUSED_VARIABLE(width);
   INTENTIONALLY_UNUSED_VARIABLE(colour);
#endif
}

static void curses_driver_ops_write_ascii_cells_at(
   TheDriverWindow *win, int row, int col, const char *text, int width,
   TheDriverAttr colour)
{
#ifdef USE_UTF8
   curses_driver_write_ascii_cells_at(curses_driver_window_from_driver(win),
                                      row, col, text, width, colour);
#else
   INTENTIONALLY_UNUSED_VARIABLE(win);
   INTENTIONALLY_UNUSED_VARIABLE(row);
   INTENTIONALLY_UNUSED_VARIABLE(col);
   INTENTIONALLY_UNUSED_VARIABLE(text);
   INTENTIONALLY_UNUSED_VARIABLE(width);
   INTENTIONALLY_UNUSED_VARIABLE(colour);
#endif
}

static void curses_driver_ops_redraw_window(TheDriverWindow *win)
{
   curses_driver_redraw_window(curses_driver_window_from_driver(win));
}

static void curses_driver_ops_draw_software_cell(
   CHARTYPE scrno, TheDriverWindow *win, short row, int col,
   TheDriverCell base,
   CursorShape shape)
{
   curses_driver_draw_software_chtype_cell(
      scrno, curses_driver_window_from_driver(win), row, col, base, shape);
}

static void curses_driver_ops_draw_software_blank_cell(
   CHARTYPE scrno, TheDriverWindow *win, short row, int col,
   TheDriverAttr base,
   CursorShape shape)
{
   curses_driver_draw_software_blank_cell(
      scrno, curses_driver_window_from_driver(win), row, col, base, shape);
}

const TheDriverOps the_curses_driver_ops = {
   .software_cursor_attr = curses_driver_ops_software_cursor_attr,
   .create_window = curses_driver_ops_create_window,
   .delete_window = curses_driver_ops_delete_window,
   .capture_window_cursor = curses_driver_ops_capture_window_cursor,
   .window_origin = curses_driver_ops_window_origin,
   .window_size = curses_driver_ops_window_size,
   .clear_current_screen_roles = curses_driver_clear_current_screen_roles,
   .move_window_cursor = curses_driver_ops_move_window_cursor,
   .restore_window_cursor = curses_driver_ops_restore_window_cursor,
   .set_window_attr = curses_driver_ops_set_window_attr,
   .set_current_role_attr = curses_driver_ops_set_current_role_attr,
   .set_screen_role_attr = curses_driver_ops_set_screen_role_attr,
   .set_global_window_attr = curses_driver_ops_set_global_window_attr,
   .set_window_background = curses_driver_ops_set_window_background,
   .clear_line_at = curses_driver_ops_clear_line_at,
   .clear_current_role_to_eol = curses_driver_clear_current_role_to_eol,
   .touch_window = curses_driver_ops_touch_window,
   .touch_line = curses_driver_ops_touch_line,
   .refresh_window = curses_driver_ops_refresh_window,
   .refresh_window_now = curses_driver_ops_refresh_window_now,
   .sync_terminal_screen = curses_driver_sync_terminal_screen,
   .clear_terminal_screen = curses_driver_clear_terminal_screen,
   .begin_terminal_report = curses_driver_begin_terminal_report,
   .write_terminal_report_text = curses_driver_write_terminal_report_text,
   .end_terminal_report = curses_driver_end_terminal_report,
   .update = curses_driver_update,
   .present_cursor = curses_driver_present_cursor,
   .set_current_window_timeout = curses_driver_set_current_window_timeout,
   .draw_box = curses_driver_ops_draw_box,
   .draw_vertical_line = curses_driver_ops_draw_vertical_line,
   .add_string_at = curses_driver_ops_add_string_at,
   .add_global_string_at = curses_driver_add_global_string_at,
   .add_cell_at = curses_driver_ops_add_cell_at,
   .draw_horizontal_line = curses_driver_ops_draw_horizontal_line,
   .add_cell = curses_driver_ops_add_cell,
   .insert_cell = curses_driver_ops_insert_cell,
   .delete_cell = curses_driver_ops_delete_cell,
   .write_cell_span = curses_driver_ops_write_cell_span,
   .write_render_cells = curses_driver_ops_write_render_cells,
   .write_render_cluster_at = curses_driver_ops_write_render_cluster_at,
   .fill_cells_at = curses_driver_ops_fill_cells_at,
   .write_ascii_cells_at = curses_driver_ops_write_ascii_cells_at,
   .read_input_event = curses_driver_read_input_event,
   .prepare_for_shell_escape = curses_driver_prepare_for_shell_escape,
   .repair_terminal_background = curses_driver_repair_terminal_background,
   .redraw_window = curses_driver_ops_redraw_window,
   .draw_software_cell = curses_driver_ops_draw_software_cell,
   .draw_software_blank_cell = curses_driver_ops_draw_software_blank_cell,
   .refresh_cursor = curses_driver_refresh_cursor,
   .redraw_screen_cursor = curses_driver_redraw_screen_cursor,
   .move_prefix_cursor = curses_driver_move_prefix_cursor,
   .move_filearea_cursor = curses_driver_move_filearea_cursor,
   .filearea_cursor_transition = curses_driver_filearea_cursor_transition
};

static void curses_driver_set_error(char *error, size_t error_len,
                                    const char *message)
{
   size_t len;

   if (error == NULL || error_len == 0)
      return;
   if (message == NULL)
      message = "";
   len = strlen(message);
   if (len >= error_len)
      len = error_len - 1;
   if (len > 0)
      memcpy(error, message, len);
   error[len] = '\0';
}

static void curses_driver_refresh_terminal_size(void)
{
   terminal_lines = LINES;
   terminal_cols = COLS;
#ifdef HAVE_BSD_CURSES
   terminal_lines--;
#endif
}

static int curses_driver_start(const TheDriverStartupOptions *options,
                               char *error, size_t error_len)
{
#if defined(USE_XCURSES) && PDC_BUILD >= 2401
   int initscr_argc = options == NULL ? 0 : options->initscr_argc;
   char **initscr_argv = options == NULL ? NULL : options->initscr_argv;
   char *x11_switches = options == NULL ? NULL : options->x11_switches;
#endif
   int slk_format = options == NULL ? 0 : options->slk_format;

   (void)error;
   (void)error_len;

#if defined(HAVE_SLK_INIT)
# if MAX_SLK == 0
   if (SLKx)
      slk_init(1);
# else
   if (SLKx)
      slk_init(slk_format);
# endif
#endif
#if defined(HAVE_SB_INIT)
   if (SBx)
      sb_init();
#endif

#if defined(USE_XCURSES) && PDC_BUILD >= 2401
   if (x11_switches != NULL)
   {
      Xinitscr(initscr_argc, initscr_argv);
      (*the_free)(x11_switches);
   }
   else
   {
      if ((initscr_argv = StringToArgv(&initscr_argc, "")) == NULL)
      {
         curses_driver_set_error(error, error_len, "allocating X11 args");
         return 0;
      }
      Xinitscr(initscr_argc, initscr_argv);
   }
   if (initscr_argc)
      (*the_free)(initscr_argv);
#else
# if defined(USE_WINGUICURSES)
   PDC_set_resize_limits(10, 1000, 10, 10000);
# endif
   initscr();
#endif
   curses_started = TRUE;

#if defined(USE_WINGUICURSES) || defined(USE_SDLCURSES) || defined(USE_XCURSES)
# if defined(HAVE_PDC_SET_FUNCTION_KEY)
   PDC_set_function_key(FUNCTION_KEY_SHUT_DOWN, KEY_EXIT);
   PDC_set_function_key(FUNCTION_KEY_PASTE, 0);
   PDC_set_function_key(FUNCTION_KEY_COPY, 0);
   Define((CHARTYPE *)"EXIT cancel");
# endif
#endif
#if defined(USE_WINGUICURSES)
   {
#if defined(TRYING_DRAG_DROP)
      CLIPFORMAT cf;
      HWND hWindow = (HWND)PDC_get_top_window();
      MyDragDropInit(NULL);
      cf = CF_HDROP;
      MyRegisterDragDrop(hWindow, &cf, 1, WM_NULL, THEDropProc, NULL);
#endif
   }
#endif

   curses_driver_refresh_terminal_size();

   if (colour_support)
   {
      colour_support = FALSE;
#ifdef A_COLOR
      if (has_colors())
      {
         start_color();
         colour_support = TRUE;
# if defined(PDCURSES) && PDC_BUILD >= 3001
         PDC_set_blink(FALSE);
# endif
         curses_driver_reset_pair_cache();
         curses_driver_reset_colour_cache();
         init_colour_pairs();
      }
#endif
#if defined(USE_XCURSES) || defined(USE_WINGUICURSES) || defined(USE_SDLCURSES) || defined(USE_VTCURSES)
      PDC_set_line_color(1);
#endif
   }

   cbreak();
   raw();
#if defined(USE_EXTCURSES)
   extended(FALSE);
#endif
#if defined(PDCURSES)
   raw_output(TRUE);
#endif
#if defined(KEY_SHIFT_L) && defined(PDCURSES)
   PDC_return_key_modifiers(TRUE);
#endif
   nonl();
   noecho();
   curses_driver_configure_standard_input(true, true);
#ifdef CURSES_DRIVER_USE_PROG_MODE
   def_prog_mode();
#endif
   (void)THETypeahead((CHARTYPE *)"OFF");

#if defined(PDCURSES_MOUSE_ENABLED)
   mouse_set(ALL_MOUSE_EVENTS & ~REPORT_MOUSE_POSITION);
#endif
#if defined(NCURSES_MOUSE_VERSION)
   mousemask(ALL_MOUSE_EVENTS, (mmask_t *)NULL);
#endif

#if defined(HAVE_BROKEN_SYSVR4_CURSES)
   curses_driver_repair_terminal_background(THE_DRIVER_REPAIR_TERMINAL_SCREEN);
#endif
   curses_driver_sync_terminal_screen();
#if defined(HAVE_SLK_INIT)
   if (SLKx)
      slk_noutrefresh();
#endif
   return 1;
}

static void curses_driver_shutdown(int prompt_on_error)
{
   if (!curses_started)
      return;
   if (prompt_on_error
   &&  error_on_screen
   &&  driver_global_window_exists(THE_DRIVER_GLOBAL_ERROR))
   {
      display_error(0, (CHARTYPE *)HIT_ANY_KEY, FALSE);
      curses_driver_refresh_window_now(curses_driver_window_from_driver(
         driver_global_window(THE_DRIVER_GLOBAL_ERROR)));
#ifdef KEY_RESIZE
      while (curses_driver_read_terminal_legacy_key() == KEY_RESIZE)
         ;
#else
      (void)curses_driver_read_terminal_legacy_key();
#endif
   }
   INSERTMODEx = FALSE;
#ifdef HAVE_BSD_CURSES
   nl();
   echo();
#endif
   endwin();
   free(curses_driver_pair_cache);
   curses_driver_pair_cache = NULL;
   curses_driver_pair_cache_count = 0;
   curses_driver_next_pair = 1;
   free(curses_driver_colour_cache);
   curses_driver_colour_cache = NULL;
   curses_driver_colour_cache_count = 0;
   curses_driver_next_colour = -1;
   curses_started = FALSE;
}

static void curses_driver_signal_shutdown(void)
{
   if (!curses_started)
      return;
   endwin();
   free(curses_driver_pair_cache);
   curses_driver_pair_cache = NULL;
   curses_driver_pair_cache_count = 0;
   curses_driver_next_pair = 1;
   free(curses_driver_colour_cache);
   curses_driver_colour_cache = NULL;
   curses_driver_colour_cache_count = 0;
   curses_driver_next_colour = -1;
#ifdef USE_XCURSES
   XCursesExit();
#endif
   curses_started = FALSE;
}

static void curses_driver_suspend_terminal(void)
{
#ifdef UNIX
# if defined(USE_EXTCURSES)
   csavetty(FALSE);
   reset_shell_mode();
# else
   endwin();
# endif
#endif

#if WAS_HAVE_BSD_CURSES
   noraw();
   nl();
   echo();
   nocbreak();
#endif
}

static void curses_driver_resume_terminal(void)
{
#ifdef UNIX
# if defined(USE_EXTCURSES)
   cresetty(FALSE);
# else
   reset_prog_mode();
#  ifdef HAVE_BSD_CURSES
   raw();
   nonl();
   noecho();
   cbreak();
#  endif
# endif
#endif
}

static void curses_driver_resize_terminal(int rows, int cols)
{
#if defined(SIGWINCH) && defined(USE_NCURSES) && defined(HAVE_RESIZETERM)
   if (rows && cols)
      resizeterm(rows, cols);
   endwin();
   curses_driver_update();
   curses_driver_sync_terminal_screen();
   ncurses_screen_resized = FALSE;
#elif defined(HAVE_RESIZE_TERM)
   resize_term(rows, cols);
#else
   (void)rows;
   (void)cols;
#endif
   curses_driver_refresh_terminal_size();
}

static int curses_driver_is_terminal_resized(void)
{
#if defined(PDCURSES)
   return is_termresized();
#elif defined(SIGWINCH) && defined(USE_NCURSES)
   return ncurses_screen_resized ? 1 : 0;
#else
   return 0;
#endif
}

static void curses_driver_lifecycle_slk_touch(void)
{
#if defined(HAVE_SLK_INIT)
   slk_touch();
#endif
}

static void curses_driver_lifecycle_slk_noutrefresh(void)
{
#if defined(HAVE_SLK_INIT)
   slk_noutrefresh();
#endif
}

static void curses_driver_lifecycle_slk_clear(void)
{
#if defined(HAVE_SLK_INIT)
   slk_clear();
#endif
}

static void curses_driver_lifecycle_slk_restore(void)
{
#if defined(HAVE_SLK_INIT)
   slk_restore();
#endif
}

static void curses_driver_lifecycle_slk_set(int key, const char *label,
                                            int format)
{
#if defined(HAVE_SLK_INIT)
   slk_set(key, label, format);
#else
   (void)key;
   (void)label;
   (void)format;
#endif
}

static void curses_driver_lifecycle_slk_attrset(TheDriverAttr attr)
{
#if defined(HAVE_SLK_INIT)
   slk_attrset(curses_driver_lower_attr(attr));
#else
   (void)attr;
#endif
}

static int curses_driver_color_pair_count(void)
{
#ifdef A_COLOR
   return COLOR_PAIRS;
#else
   return 1;
#endif
}

static int curses_driver_color_count(void)
{
#ifdef A_COLOR
   return COLORS;
#else
   return 16;
#endif
}

static int curses_driver_can_change_color_lifecycle(void)
{
#ifdef A_COLOR
   return can_change_color();
#else
   return 0;
#endif
}

static void curses_driver_init_pair_lifecycle(int pair, int fg, int bg)
{
#ifdef A_COLOR
   init_pair((short)pair, (short)fg, (short)bg);
#else
   (void)pair;
   (void)fg;
   (void)bg;
#endif
}

static void curses_driver_init_color_lifecycle(int color, int red, int green,
                                               int blue)
{
#ifdef A_COLOR
   init_color((short)color, (short)red, (short)green, (short)blue);
#else
   (void)color;
   (void)red;
   (void)green;
   (void)blue;
#endif
}

static const char *curses_driver_ui_version(void)
{
#if defined(HAVE_CURSES_VERSION) || defined(USE_NCURSES)
   return curses_version();
#elif defined(USE_EXTCURSES)
   return "Extended Curses";
#else
   return "Standard Curses";
#endif
}

static int curses_driver_mouse_interval_lifecycle(int interval)
{
#if defined(PDCURSES_MOUSE_ENABLED) || defined(NCURSES_MOUSE_VERSION)
   return mouseinterval(interval);
#else
   (void)interval;
   return -1;
#endif
}

static void curses_driver_mouse_mask_lifecycle(int enabled)
{
#if defined(PDCURSES_MOUSE_ENABLED)
   mouse_set(enabled ? (ALL_MOUSE_EVENTS & ~REPORT_MOUSE_POSITION) : 0L);
#endif
#if defined(NCURSES_MOUSE_VERSION)
   mousemask(enabled ? ALL_MOUSE_EVENTS : 0, (mmask_t *)NULL);
#endif
}

static void curses_driver_nap_ms_lifecycle(int milliseconds)
{
   napms(milliseconds);
}

static TheDriverCell curses_driver_alternate_cell(TheDriverAltCell cell)
{
   uint32_t fallback;

   switch (cell)
   {
      case THE_DRIVER_ALT_UARROW:
         fallback = '^';
         break;
      case THE_DRIVER_ALT_DARROW:
         fallback = 'v';
         break;
      case THE_DRIVER_ALT_LARROW:
         fallback = '<';
         break;
      case THE_DRIVER_ALT_RARROW:
         fallback = '>';
         break;
      case THE_DRIVER_ALT_VLINE:
      default:
         fallback = '|';
         break;
   }
   return the_driver_cell_make_alternate(fallback, THE_RENDER_ATTR_NORMAL);
}

const TheDriverModuleLifecycle the_curses_driver_lifecycle = {
   .name = "curses",
   .start = curses_driver_start,
   .shutdown = curses_driver_shutdown,
   .signal_shutdown = curses_driver_signal_shutdown,
   .suspend_terminal = curses_driver_suspend_terminal,
   .resume_terminal = curses_driver_resume_terminal,
   .resize_terminal = curses_driver_resize_terminal,
   .refresh_terminal_size = curses_driver_refresh_terminal_size,
   .is_terminal_resized = curses_driver_is_terminal_resized,
   .read_terminal_legacy_key = curses_driver_read_terminal_legacy_key,
   .read_raw_window_key = curses_driver_read_raw_driver_window_key,
   .set_window_leaveok = curses_driver_set_driver_window_leaveok,
   .slk_touch = curses_driver_lifecycle_slk_touch,
   .slk_noutrefresh = curses_driver_lifecycle_slk_noutrefresh,
   .slk_clear = curses_driver_lifecycle_slk_clear,
   .slk_restore = curses_driver_lifecycle_slk_restore,
   .slk_set = curses_driver_lifecycle_slk_set,
   .slk_attrset = curses_driver_lifecycle_slk_attrset,
   .current_mouse_screen_role_position =
      curses_driver_current_mouse_screen_role_position,
   .current_mouse_global_position = curses_driver_current_mouse_global_position,
   .current_mouse_screen_position = curses_driver_current_mouse_screen_position,
   .clear_mouse_packet_position = curses_driver_clear_mouse_packet_position,
   .read_pending_mouse_button = curses_driver_read_pending_mouse_button,
   .read_transient_mouse_event = curses_driver_read_transient_mouse_event,
   .read_current_role_transient_mouse_event =
      curses_driver_read_current_role_transient_mouse_event,
   .color_pair_count = curses_driver_color_pair_count,
   .color_count = curses_driver_color_count,
   .can_change_color = curses_driver_can_change_color_lifecycle,
   .init_pair = curses_driver_init_pair_lifecycle,
   .init_color = curses_driver_init_color_lifecycle,
   .ui_version = curses_driver_ui_version,
   .mouse_interval = curses_driver_mouse_interval_lifecycle,
   .mouse_mask = curses_driver_mouse_mask_lifecycle,
   .nap_ms = curses_driver_nap_ms_lifecycle,
   .alternate_cell = curses_driver_alternate_cell,
   .current_cursor_shape = curses_driver_current_cursor_shape,
   .current_cursor_blink = curses_driver_current_cursor_blink,
   .current_cursor_presentation = curses_driver_current_cursor_presentation,
   .current_cursor_uses_software = curses_driver_current_cursor_uses_software
};

const TheDriverOps *the_driver_module_ops(void)
{
   return &the_curses_driver_ops;
}

const TheDriverModuleLifecycle *the_driver_module_lifecycle(void)
{
   return &the_curses_driver_lifecycle;
}
