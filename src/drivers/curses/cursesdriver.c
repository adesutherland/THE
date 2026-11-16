#include "cursesdriver.h"
#include "the.h"
#include "proto.h"
#include "driverlayout.h"
#include "curseskeymap.h"
#include "tools.h"
#include "panelui.h"
#include "workbench.h"
#include "workbenchruntime.h"
#include "workbenchdocument.h"
#include "workbenchform.h"
#include "workbenchfilefields.h"

/* ncurses always provides batched refresh. Some generated configurations
 * leave HAVE_WNOUTREFRESH unset; the legacy wrefresh fallback would then
 * present each background pane before its menu overlay on every job poll. */
#if defined(USE_NCURSES) && !defined(HAVE_WNOUTREFRESH)
# undef wnoutrefresh
#endif

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#ifdef _WIN32
#include <windows.h>
#else
#include <dirent.h>
#endif

#ifdef USE_UTF8
# include <wchar.h>
#endif

static TheWorkbenchControl workbench_control = THE_WORKBENCH_EDITOR;
static int native_log_focused(void);
static int native_program_focused(void);
static int native_tools_has_focus(void);

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

/* THE uses a stable BGR logical order. ncurses and some PDCurses builds use
 * RGB order; native constants, rather than numeric IDs, own that boundary. */
static short curses_driver_basic_colour(int colour)
{
#ifdef A_COLOR
   static const short basic[] = { COLOR_BLACK, COLOR_BLUE, COLOR_GREEN, COLOR_CYAN,
      COLOR_RED, COLOR_MAGENTA, COLOR_YELLOW, COLOR_WHITE };
   if (colour >= THE_COLOR_BLACK && colour <= THE_COLOR_WHITE) return basic[colour];
#endif
   return (short)colour;
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
      return curses_driver_basic_colour(colour);
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

static int curses_driver_cluster_uses_replacement_output(
   const TheRenderCluster *cluster)
{
   if (cluster == NULL)
      return 0;
   if (cluster->flags & (THE_RENDER_CLUSTER_SUBSTITUTE
                      |  THE_RENDER_CLUSTER_BASE
                      |  THE_RENDER_CLUSTER_COMPONENTS))
      return 1;
   switch (cluster->output_method)
   {
      case UTF8_TERM_OUTPUT_SUBSTITUTE:
      case UTF8_TERM_OUTPUT_BASE:
      case UTF8_TERM_OUTPUT_COMPONENTS:
         return 1;
      default:
         return 0;
   }
}

static TheRenderAttr curses_driver_cluster_attr(
   const TheRenderCluster *cluster)
{
   TheRenderAttr attr;

   if (cluster == NULL)
      return THE_RENDER_ATTR_NORMAL;
   attr = cluster->attr;
   /*
    * Replacement output is a UTF display fact, including NORMAL sanitize
    * output. Flag it so the user can tell the visible cells are not native
    * stored text.
    */
   if (curses_driver_cluster_uses_replacement_output(cluster))
      attr = the_render_attr_merge_style(attr, THE_STYLE_REVERSE);
   return attr;
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
   if (native_program_focused()) return cursorstyle_insert_shape;
   return INSERTMODEx ? cursorstyle_insert_shape : cursorstyle_over_shape;
}

static CursorBlink curses_driver_current_cursor_blink(void)
{
   if (native_program_focused()) return cursorstyle_insert_blink;
   return INSERTMODEx ? cursorstyle_insert_blink : cursorstyle_over_blink;
}

static CursorPresentation curses_driver_current_cursor_presentation(void)
{
   if (native_program_focused() || native_tools_has_focus() || native_log_focused() ||
       workbench_control != THE_WORKBENCH_EDITOR)
      return CURSOR_PRESENTATION_HARDWARE;
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

static void curses_driver_overlay_cell_attrs_at(WINDOW *win, int row, int col,
                                                int width,
                                                TheDriverAttr colour)
{
   int maxy;
   int maxx;
   attr_t attrs;
   short pair;

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

   attrs = (attr_t)curses_driver_style_to_attr(the_render_attr_style(colour));
   pair = curses_driver_pair_for_attr(colour);
   mvwchgat(win, row, col, width, attrs, pair, NULL);
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

static WINDOW *console_window, *console_divider;
static int console_rows, console_cols, console_top, console_left;
static int console_timeout = -1, console_toolbar_focus, console_control;
static int console_scroll_offset, console_scroll_lines, console_scroll_page;
static unsigned console_scroll_id;
static uint64_t console_scroll_epoch;
static uint64_t console_scroll_displayed;
static int console_scroll_unread;
static int native_tools_visible, native_tools_focused, native_tools_control, native_tools_section;
static int native_aux_suppressed;
static TheToolsFileSnapshot native_file;
static struct { int offset, lines, page, unread; unsigned id; uint64_t epoch, displayed; } native_build_reader;
static int console_cached_mouse, console_mouse_button, console_mouse_action;
static int console_mouse_modifier;
typedef struct { size_t line, offset; } NativeDocumentCursor;
typedef struct
{
   NativeDocumentCursor first, next, previous[32];
   size_t previous_count, total_lines;
   int more, request, budget_lines, initialized, tail;
   uint64_t revision;
   char text[THE_WORKBENCH_DOCUMENT_BYTES_MAX * 4 + 65], reason[64];
   size_t length;
} NativeDocumentView;
typedef struct
{
   unsigned long generation;
   uint64_t revision, pending, receipt;
   int active, cell, top, blocked;
   char error[64];
} NativeFormView;
typedef struct
{
   uint64_t revision, pending, receipt;
   int initialized, active, cell, top, left, follow;
   char error[96];
} NativeFileFieldsView;
static uint64_t native_form_request_id;
static struct
{
   char selected[THE_WORKBENCH_ID_MAX + 1];
   unsigned long selected_generation, focus_serial;
   int focused, origin_program, origin_toolbar, origin_control;
   TheWorkbenchControl origin_chrome;
   struct
   {
      char id[THE_WORKBENCH_ID_MAX + 1];
      unsigned long generation;
      int scroll, lines, width;
      uint64_t revision, anchor_cursor;
      size_t anchor_offset;
      NativeDocumentView document;
      NativeFormView form;
      NativeFileFieldsView filefields;
   } views[THE_WORKBENCH_PANEL_MAX];
   size_t count;
} log_ui;

static int native_log_focused(void) { return log_ui.focused; }
static int native_program_focused(void)
{ return !native_aux_suppressed && !native_tools_visible && !log_ui.focused && the_tool_program_console()->focused; }
static int native_tools_has_focus(void) { return native_tools_visible && native_tools_focused && !native_aux_suppressed; }
static void native_editor_focus(void)
{
   native_tools_focused = console_toolbar_focus = 0;
   the_tool_console_visibility(the_tool_console()->visible, 0);
   the_tool_program_console_visibility(the_tool_program_console()->visible, 0);
   workbench_control = THE_WORKBENCH_EDITOR;
}
static void native_show_tools(int section, int toggle)
{
   int hide = toggle && native_tools_visible && !native_aux_suppressed;
   native_editor_focus();
   log_ui.focused = 0;
   native_tools_visible = !hide;
   native_aux_suppressed = hide;
   if (!hide) { native_tools_section = section; native_tools_focused = 1; }
}
static void native_show_program(int toggle)
{
   int hide = toggle && !native_aux_suppressed && !native_tools_visible
      && !log_ui.selected[0] && the_tool_program_console()->visible;
   native_editor_focus();
   log_ui.focused = 0; log_ui.selected[0] = 0;
   native_tools_visible = 0; native_aux_suppressed = hide;
   the_tool_program_console_visibility(!hide, !hide);
}
static int native_text_panel(const TheWorkbenchPanel *panel)
{ return panel->kind == THE_WORKBENCH_PANEL_LOG || panel->kind == THE_WORKBENCH_PANEL_DOCUMENT || panel->kind == THE_WORKBENCH_PANEL_FIELDS; }

static void native_filefields_sync(const TheWorkbenchPanel *panel, NativeFileFieldsView *view)
{
   const TheWorkbenchFileFieldsState *state = the_workbench_filefields_state(panel->id);
   const TheWorkbenchFileFieldsReceipt *receipt = the_workbench_filefields_receipt();
   if (!state) { strcpy(view->error, "File fields unavailable"); return; }
   if (!view->initialized)
   {
      view->initialized = view->follow = 1; view->revision = state->draft_revision;
      while (view->active < (int)state->spans && !the_workbench_filefields_span(panel->id, view->active)->editable) view->active++;
   }
   if (view->pending && receipt && receipt->request_id == view->pending && receipt->serial != view->receipt && !strcmp(receipt->panel, panel->id))
   {
      view->receipt = receipt->serial; view->pending = 0;
      if (!receipt->rc)
      {
         view->revision = receipt->draft_revision; view->error[0] = 0;
         if (receipt->caret >= 0) { view->cell = receipt->caret; view->follow = 1; }
         if (receipt->operation == THE_WORKBENCH_FILEFIELDS_RELOAD)
         { view->active = 0; view->cell = view->top = view->left = 0; view->follow = 1;
           while (view->active < (int)state->spans && !the_workbench_filefields_span(panel->id, view->active)->editable) view->active++; }
      }
      else snprintf(view->error, sizeof(view->error), "%s; Reload or Cancel", receipt->code);
   }
   if (!view->pending && state->draft_revision != view->revision)
      strcpy(view->error, "stale-revision; Reload or Cancel");
   if (view->active > (int)state->spans + 4) view->active = (int)state->spans;
}

static const TheWorkbenchField *native_form_field(const char *form, size_t index)
{
   size_t i;
   for (i = 0; i < the_workbench_field_count(); i++)
   {
      const TheWorkbenchField *field = the_workbench_field_at(i);
      if (!strcmp(field->form, form) && !index--) return field;
   }
   return NULL;
}

static void native_form_sync(const TheWorkbenchPanel *panel, NativeFormView *view, int focus)
{
   const TheWorkbenchFormState *state = the_workbench_form_state(panel->form);
   const TheWorkbenchFormReceipt *receipt = the_workbench_form_receipt();
   if (!state) { view->blocked = 1; strcpy(view->error, "missing-form"); return; }
   if (focus || !view->generation)
   {
      if (view->generation != state->generation) { view->active = 0; view->cell = 0; view->top = 0; }
      view->generation = state->generation; view->revision = state->draft_revision;
      view->pending = 0; view->blocked = 0; view->error[0] = 0;
      if (view->active > (int)state->fields + 2) view->active = 0;
      while (view->active < (int)state->fields && native_form_field(panel->form, view->active)->access != THE_WORKBENCH_FIELD_EDIT) view->active++;
   }
   if (view->pending && receipt && receipt->request_id == view->pending && receipt->serial != view->receipt)
   {
      view->receipt = receipt->serial; view->pending = 0;
      if (!receipt->rc)
      {
         view->revision = receipt->draft_revision; view->error[0] = 0;
         if (receipt->caret >= 0) view->cell = receipt->caret;
      }
      else
      {
         snprintf(view->error, sizeof(view->error), "%s", receipt->code);
         view->blocked = !strcmp(receipt->code, "stale-revision") || !strcmp(receipt->code, "stale-generation");
      }
   }
   if (state->generation != view->generation || (!view->pending && state->draft_revision != view->revision))
   { view->blocked = 1; strcpy(view->error, "stale-revision; refocus form"); }
}

static size_t native_log_count(void)
{
   size_t i, count = 0;
   for (i = 0; i < the_workbench_panel_count(); i++)
   {
      const TheWorkbenchPanel *panel = the_workbench_panel_at(i);
      const TheWorkbenchPanelState *state = the_workbench_panel_state(panel->id);
      if (native_text_panel(panel) && state && state->visible) count++;
   }
   return count;
}

static const TheWorkbenchPanel *native_log_at(size_t index)
{
   size_t i;
   for (i = 0; i < the_workbench_panel_count(); i++)
   {
      const TheWorkbenchPanel *panel = the_workbench_panel_at(i);
      const TheWorkbenchPanelState *state = the_workbench_panel_state(panel->id);
      if (native_text_panel(panel) && state && state->visible)
      { if (!index--) return panel; }
   }
   return NULL;
}

static const TheWorkbenchPanel *native_selected_log(void)
{
   if (native_tools_visible || native_aux_suppressed) return NULL;
   const TheWorkbenchPanel *panel = the_workbench_panel_find(log_ui.selected);
   const TheWorkbenchPanelState *state = panel ? the_workbench_panel_state(panel->id) : NULL;
   return panel && panel->generation == log_ui.selected_generation && state && state->visible ? panel : NULL;
}

static int native_log_event(const TheWorkbenchPanel *panel, TheWorkbenchPanelOperation operation,
                            TheInputEvent *event)
{
   return panel && the_input_event_from_workbench_panel(panel->id, panel->generation, operation, event);
}

static void native_logs_sync(void)
{
   const TheWorkbenchFocusRequest *focus = the_workbench_focus_request();
   const TheWorkbenchPanel *panel;
   size_t i, j;
   for (i = 0; i < log_ui.count; )
   {
      panel = the_workbench_panel_find(log_ui.views[i].id);
      if (!panel || panel->generation != log_ui.views[i].generation)
      { memmove(log_ui.views + i, log_ui.views + i + 1, (--log_ui.count - i) * sizeof(log_ui.views[0])); continue; }
      i++;
   }
   for (i = 0; i < the_workbench_panel_count(); i++)
   {
      panel = the_workbench_panel_at(i);
      if (!native_text_panel(panel)) continue;
      for (j = 0; j < log_ui.count && strcmp(log_ui.views[j].id, panel->id); j++);
      if (j == log_ui.count && log_ui.count < THE_WORKBENCH_PANEL_MAX)
      {
         memset(log_ui.views + j, 0, sizeof(log_ui.views[j]));
         strcpy(log_ui.views[j].id, panel->id); log_ui.views[j].generation = panel->generation;
         log_ui.count++;
      }
   }
   if (focus && focus->serial != log_ui.focus_serial)
   {
      log_ui.focus_serial = focus->serial;
      panel = the_workbench_panel_find(focus->panel);
      if (panel && panel->generation == focus->generation)
      {
         native_aux_suppressed = native_tools_visible = native_tools_focused = 0;
         if (!log_ui.focused)
         {
            log_ui.origin_program = the_tool_program_console()->focused;
            log_ui.origin_toolbar = console_toolbar_focus; log_ui.origin_control = console_control;
            log_ui.origin_chrome = workbench_control;
         }
         strcpy(log_ui.selected, panel->id); log_ui.selected_generation = panel->generation;
         log_ui.focused = 1; workbench_control = THE_WORKBENCH_EDITOR;
         if (panel->kind == THE_WORKBENCH_PANEL_FIELDS)
            for (i = 0; i < log_ui.count; i++) if (!strcmp(log_ui.views[i].id, panel->id))
            {
               if (panel->document_id) native_filefields_sync(panel, &log_ui.views[i].filefields);
               else native_form_sync(panel, &log_ui.views[i].form, 1);
            }
         if (the_tool_program_console()->focused)
            the_tool_program_console_visibility(the_tool_program_console()->visible, 0);
      }
      else if (!focus->panel[0] && log_ui.focused)
      {
         log_ui.focused = 0;
         console_toolbar_focus = log_ui.origin_toolbar; console_control = log_ui.origin_control;
         workbench_control = log_ui.origin_chrome;
         if (log_ui.origin_program && the_tool_program_console()->visible)
         {
            log_ui.selected[0] = '\0';
            the_tool_program_console_visibility(1, 1);
         }
      }
   }
   if (!log_ui.focused && native_program_focused()) log_ui.selected[0] = '\0';
   if (!native_selected_log())
   {
      log_ui.selected[0] = '\0';
      if (!native_tools_visible && !native_aux_suppressed && !the_tool_program_console()->visible && (panel = native_log_at(0)) != NULL)
      { strcpy(log_ui.selected, panel->id); log_ui.selected_generation = panel->generation; }
   }
   if (console_control > 4 && !native_log_count()) console_control = 0;
}

static int native_log_view(const TheWorkbenchPanel *panel)
{
   size_t i;
   for (i = 0; i < log_ui.count; i++)
      if (!strcmp(log_ui.views[i].id, panel->id) && log_ui.views[i].generation == panel->generation) return (int)i;
   return -1;
}
static struct
{
   int active, program_focused, toolbar_focused, toolbar_control, tools_focused;
   TheWorkbenchControl chrome;
} workbench_menu_origin;

enum { CONSOLE_EDITOR_MIN_COLS = 72, CONSOLE_MIN_COLS = 44 };

static const char *const console_controls[] = { "Run", "Stop", "Clear", "Hide", "Editor", "Logs" };
static int console_control_count(void) { return native_log_count() ? 6 : 5; }
static const char *console_control_label(int control)
{
   size_t i;
   if (!control) return *native_file.actions[1].tool ? native_file.actions[1].label : "Tools";
   if (control == 5)
      for (i = 0; i < native_log_count(); i++)
         if (native_log_at(i)->kind == THE_WORKBENCH_PANEL_DOCUMENT) return "Panes";
   return console_controls[control];
}

static void workbench_set_focus(TheWorkbenchControl control)
{
   workbench_control = FILETABSx && filetabs ? control : THE_WORKBENCH_EDITOR;
   if (workbench_control != THE_WORKBENCH_EDITOR)
   {
      native_tools_focused = 0;
      console_toolbar_focus = 0;
      the_tool_program_console_visibility(the_tool_program_console()->visible, 0);
   }
   if (FILETABSx && filetabs) display_filetabs(NULL);
   /* The file-tabs repaint must not replace the editor's saved caret. */
   if (workbench_control == THE_WORKBENCH_EDITOR &&
       curses_driver_current_active_window())
      wnoutrefresh(curses_driver_current_active_window());
}

static void workbench_remember_menu_origin(void)
{
   workbench_menu_origin.active = 1;
   workbench_menu_origin.program_focused = native_program_focused();
   workbench_menu_origin.tools_focused = native_tools_has_focus();
   workbench_menu_origin.toolbar_focused = console_toolbar_focus;
   workbench_menu_origin.toolbar_control = console_control;
   workbench_menu_origin.chrome = workbench_control;
}

static void workbench_restore_cancelled_menu(void)
{
   if (!workbench_menu_origin.active) return;
   workbench_menu_origin.active = 0;
   if (workbench_menu_origin.program_focused)
   {
      console_toolbar_focus = workbench_menu_origin.toolbar_focused;
      console_control = workbench_menu_origin.toolbar_control;
      the_tool_program_console_visibility(the_tool_program_console()->visible, 1);
   }
   else if (workbench_menu_origin.tools_focused) native_tools_focused = 1;
   else if (workbench_menu_origin.chrome != THE_WORKBENCH_EDITOR)
      workbench_set_focus(workbench_menu_origin.chrome);
}

static TheWorkbenchControl workbench_traverse(TheWorkbenchControl control,
                                              int reverse)
{
   if (reverse)
      return control == THE_WORKBENCH_RUN ? THE_WORKBENCH_CONSOLE
         : control == THE_WORKBENCH_CONSOLE ? THE_WORKBENCH_MENU
         : THE_WORKBENCH_RUN;
   return control == THE_WORKBENCH_MENU ? THE_WORKBENCH_CONSOLE
      : control == THE_WORKBENCH_CONSOLE ? THE_WORKBENCH_RUN
      : THE_WORKBENCH_MENU;
}

static TheWorkbenchControl curses_driver_workbench_focus(bool enter)
{
   if (enter) workbench_set_focus(THE_WORKBENCH_CONSOLE);
   if (!FILETABSx || !filetabs) workbench_control = THE_WORKBENCH_EDITOR;
   return workbench_control;
}

/* Presentation reads C-owned roles and actions. Invocation binds the exact
 * source/action tokens that supplied the visible control. */
static int native_file_action(const TheToolsFileSnapshot *snapshot, unsigned index, TheInputEvent *event)
{
   static const char *const names[] = {"file-build", "file-run", "file-activate"};
   const TheToolsFileActionSnapshot *action;
   char generation[32];
   if (index >= THE_TOOLS_FILE_ACTION_COUNT || in_macro || the_panel_active) return 0;
   action = snapshot->actions + index;
   if (!action->enabled) return 0;
   snprintf(generation, sizeof(generation), "%lu", action->generation);
   if (!the_tools_prepare_lines_bound(names[index], action->tool, "", "",
       snapshot->source_id, snapshot->revision, generation, event)) return 0;
   if (index == 1) native_show_program(0); else native_show_tools(2, 0);
   return 1;
}

static void native_tools_sync(void)
{
   static unsigned program_id, build_id;
   static int program_visible, program_focused;
   const TheToolConsole *program = the_tool_program_console(), *build = the_tool_build_console();
   the_tools_file_snapshot(&native_file);
   if (build->id != build_id)
   { build_id = build->id; if (build_id) native_show_tools(2, 0); }
   if (program->id != program_id)
   { program_id = program->id; if (program_id) native_show_program(0); }
   else if ((program->visible && !program_visible) || (program->focused && !program_focused))
   { native_tools_visible = native_aux_suppressed = 0; }
   program_visible = program->visible; program_focused = program->focused;
}

static void console_delete_windows(void)
{
   if (console_window) delwin(console_window);
   if (console_divider) delwin(console_divider);
   console_window = console_divider = NULL;
}

static chtype console_theme_attr(int role, TheRenderStyle style)
{
   TheRenderAttr attr = CURRENT_VIEW && CURRENT_FILE
      ? set_colour(CURRENT_FILE->attr + role) : THE_RENDER_ATTR_NORMAL;

   return curses_driver_lower_attr(the_render_attr_merge_style(attr, style));
}

static TheWorkbenchPlacement native_log_placement(const TheWorkbenchPanel *panel)
{
   size_t i;
   int right = COLS >= CONSOLE_EDITOR_MIN_COLS + 1 + max(CONSOLE_MIN_COLS, panel->mincols)
      && LINES >= max(16, panel->minrows);
   int below_rows = max(LINES >= 24 ? 8 : 6, panel->minrows);
   int below = COLS >= max(32, panel->mincols) && LINES >= 16 && LINES - below_rows - 1 >= 9;
   for (i = 0; i <= panel->fallback_count; i++)
   {
      TheWorkbenchPlacement candidate = i ? panel->fallbacks[i - 1] : panel->placement;
      if ((candidate == THE_WORKBENCH_PLACE_RIGHT && right) ||
          (candidate == THE_WORKBENCH_PLACE_BELOW && below) ||
          (candidate == THE_WORKBENCH_PLACE_TAB && (right || below))) return candidate;
   }
   return THE_WORKBENCH_PLACE_NONE;
}

static int console_is_right(void)
{
   const TheWorkbenchPanel *panel = native_selected_log();
   if (panel)
   {
      TheWorkbenchPlacement place = native_log_placement(panel);
      if (place == THE_WORKBENCH_PLACE_BELOW || place == THE_WORKBENCH_PLACE_NONE) return 0;
      if (place == THE_WORKBENCH_PLACE_RIGHT) return 1;
      if (place == THE_WORKBENCH_PLACE_TAB)
         return COLS >= CONSOLE_EDITOR_MIN_COLS + 1 + max(CONSOLE_MIN_COLS, panel->mincols);
   }
   return COLS >= CONSOLE_EDITOR_MIN_COLS + 1 + CONSOLE_MIN_COLS;
}

static int console_height(void)
{
   return !native_aux_suppressed && (native_tools_visible || the_tool_program_console()->visible || native_log_count()) && LINES >= 16 && COLS >= 32
      ? (console_is_right() ? LINES : max(LINES >= 24 ? 8 : 6,
          native_selected_log() ? min(native_selected_log()->minrows, LINES - 10) : 0)) : 0;
}

static int console_width(void)
{
   if (!console_height()) return 0;
   return console_is_right() ? max(max(CONSOLE_MIN_COLS, COLS / 3),
      native_selected_log() ? native_selected_log()->mincols : 0) : COLS;
}

static int console_layout_changed(void)
{
   return console_height() != console_rows || console_width() != console_cols;
}

static int console_control_event(int control, TheInputEvent *event)
{
   const TheToolConsole *c = the_tool_program_console();

   switch (control)
   {
      case 0:
         if (!*native_file.actions[1].tool) { native_show_tools(0, 0); break; }
         console_toolbar_focus = 0;
         return native_file_action(&native_file, 1, event);
      case 1:
         return c->running ? the_input_event_from_command("rx stop", event) : 0;
      case 2: the_tool_clear_program(); break;
      case 3:
         native_show_program(1); break;
      case 4:
         native_editor_focus(); break;
      case 5: return native_log_event(native_log_at(0), THE_WORKBENCH_PANEL_FOCUS, event);
   }
   curses_driver_update();
   return 0;
}

/* Bound every physical row in cells. A long program line must not wrap into
 * the editor's response row, input field or following terminal line. */
static int curses_write_text(WINDOW *window, int row, int col, const char *text, size_t length,
                             int cells)
{
   TextPos pos = textpos_begin();
   int used = 0;

   wmove(window, row, col);
   while (pos.byte_offset < length && used < cells)
   {
      TextCluster cluster = textpos_cluster_at_boundary(
         (const CHARTYPE *)text, length, pos);
      int width = cluster.valid ? max(1, cluster.cell_width) : 1;
      size_t bytes = cluster.byte_length;

      if (!bytes) break;
      if (text[pos.byte_offset] == '\t')
      {
         if (used + 2 > cells) break;
         waddstr(window, "  ");
         used += 2;
      }
      else if ((unsigned char)text[pos.byte_offset] >= 32
            && text[pos.byte_offset] != 127)
      {
         if (used + width > cells) break;
         if (cluster.valid && cluster.cell_width > 0)
            waddnstr(window, text + pos.byte_offset, (int)bytes);
         else
            waddch(window, '?');
         used += width;
      }
      pos = cluster.end;
   }
   return used;
}

static int console_write_text(int row, int col, const char *text, size_t length, int cells)
{
   return curses_write_text(console_window, row, col, text, length, cells);
}

/* Count and paint the same visual rows, so tailing and resize use exactly the
 * same cell boundaries. The last column remains unused to avoid curses wrap. */
static int console_output_rows(const char *text, const unsigned char *streams,
                               size_t length, int cells, int skip,
                               int first_row, int paint_rows, size_t *row_start, int target_row)
{
   TextPos pos = textpos_begin();
   int row = 0, col = 0, pending_row = 0, last_stream = -1;
   if (row_start) *row_start = 0;

   while (pos.byte_offset < length)
   {
      TextCluster cluster = textpos_cluster_at_boundary(
         (const CHARTYPE *)text, length, pos);
      unsigned char ch = (unsigned char)text[pos.byte_offset];
      int width;

      if (!cluster.byte_length) break;
      if (ch == '\n' || (ch == '\r' && cluster.byte_length == 2
                       && text[pos.byte_offset + 1] == '\n'))
      {
         row++;
         if (row_start && row == target_row) *row_start = cluster.end.byte_offset;
         col = pending_row = 0;
         last_stream = -1;
      }
      else if (ch == '\t' || (ch >= 32 && ch != 127))
      {
         width = ch == '\t' ? 2
            : cluster.valid ? max(1, cluster.cell_width) : 1;
         /* Even an oversized grapheme must make progress in a narrow pane. */
         if (width > cells) width = 1;
         if (col && col + width > cells)
         {
            row++;
            if (row_start && row == target_row) *row_start = pos.byte_offset;
            col = 0;
            last_stream = -1;
         }
         if (row >= skip && row - skip < paint_rows)
         {
            int stream = streams[pos.byte_offset];

            if (stream != last_stream)
            {
               wattrset(console_window, console_theme_attr(
                  stream == 2 ? ATTR_CONSOLEERROR : ATTR_CONSOLEBODY, 0));
               last_stream = stream;
            }
            wmove(console_window, first_row + row - skip, col);
            if (ch == '\t') waddstr(console_window, "  ");
            else if (cluster.valid && cluster.cell_width > 0
                  && cluster.cell_width <= cells)
               waddnstr(console_window, text + pos.byte_offset,
                        (int)cluster.byte_length);
            else waddch(console_window, '?');
         }
         col += width;
         pending_row = 1;
      }
      pos = cluster.end;
   }
   /* A trailing newline completes a row without appending an empty one. */
   return row + pending_row;
}

#define NATIVE_LOG_RENDER_MAX (THE_WORKBENCH_LOG_TEXT_MAX * 4u)
static char log_render_text[NATIVE_LOG_RENDER_MAX + 1];
static unsigned char log_render_streams[NATIVE_LOG_RENDER_MAX];
static struct { uint64_t cursor; size_t start; } log_render_records[THE_WORKBENCH_LOG_RECORD_MAX];
static size_t log_render_record_count;
static uint64_t log_render_revision;

/* Logs are already UTF-8 text, with explicit lengths. Only LF is interpreted;
 * all other terminal controls, including NUL, ESC and C1, remain visible data. */
static size_t native_log_text(const TheWorkbenchPanel *panel, uint64_t *omitted)
{
   TheWorkbenchLogPage page;
   uint64_t cursor = 0;
   size_t offset = 0, used = 0, i, j;
   *omitted = 0; log_render_record_count = 0; log_render_revision = 0;
   do
   {
      if (the_workbench_log_page(panel->model, panel->model_generation, cursor, offset,
          THE_WORKBENCH_LOG_PAGE_MAX, THE_WORKBENCH_LOG_TEXT_MAX, &page) != THE_WORKBENCH_LOG_OK) break;
      *omitted = page.log.omitted_bytes; log_render_revision = page.log.revision;
      for (i = 0; i < page.count; i++)
      {
         const TheWorkbenchLogSlice *slice = page.slices + i;
         if (log_render_record_count < THE_WORKBENCH_LOG_RECORD_MAX &&
             (!log_render_record_count || log_render_records[log_render_record_count - 1].cursor != slice->cursor))
         {
            log_render_records[log_render_record_count].cursor = slice->cursor;
            log_render_records[log_render_record_count++].start = used;
         }
         for (j = 0; j < slice->length && used < NATIVE_LOG_RENDER_MAX; j++)
         {
            unsigned char c = (unsigned char)slice->text[j];
            char escape[8]; const char *bytes = slice->text + j; size_t n = 1;
            if ((c < 32 && c != '\n') || c == 127)
            { snprintf(escape, sizeof(escape), "\\x%02X", c); bytes = escape; n = 4; }
            else if (c == 0xc2 && j + 1 < slice->length &&
                     (unsigned char)slice->text[j + 1] >= 0x80 && (unsigned char)slice->text[j + 1] <= 0x9f)
            { snprintf(escape, sizeof(escape), "\\u00%02X", (unsigned char)slice->text[++j]); bytes = escape; n = 6; }
            if (n > NATIVE_LOG_RENDER_MAX - used) break;
            memcpy(log_render_text + used, bytes, n);
            memset(log_render_streams + used, slice->stream, n); used += n;
         }
      }
      cursor = page.next_cursor; offset = page.next_offset;
   } while (page.more && used < NATIVE_LOG_RENDER_MAX);
   log_render_text[used] = '\0';
   return used;
}

static const char *native_log_error_name(TheWorkbenchLogResult result)
{
   switch (result)
   {
      case THE_WORKBENCH_LOG_INVALID: return "invalid-output";
      case THE_WORKBENCH_LOG_MISSING: return "missing-model";
      case THE_WORKBENCH_LOG_STALE: return "stale-model";
      case THE_WORKBENCH_LOG_LIMIT: return "limit-exceeded";
      case THE_WORKBENCH_LOG_NOMEM: return "out-of-memory";
      default: return "ok";
   }
}

static void native_document_refresh(const TheWorkbenchPanel *panel, NativeDocumentView *view)
{
   TheWorkbenchDocumentInfo info;
   TheWorkbenchDocumentPage page;
   TheWorkbenchDocumentResult result = the_workbench_document_info(panel->document_id, &info);
   NativeDocumentCursor cursor;
   char text[sizeof(view->text)];
   size_t used = 0, raw = 0, lines = 0, i;
   int budget = max(1, min(64, console_rows - 3)), ending = view->request == 2;
   if (result != THE_WORKBENCH_DOCUMENT_OK || !info.available)
   {
      snprintf(view->reason, sizeof(view->reason), "%s", info.reason ? info.reason : the_workbench_document_result_name(result));
      if (result == THE_WORKBENCH_DOCUMENT_MISSING) { view->length = 0; view->initialized = 0; }
      return; /* A busy owner retains the last bounded copy. */
   }
   view->reason[0] = 0;
   view->total_lines = info.lines;
   if (!view->first.line) view->first.line = 1;
   if (view->first.line > max((size_t)1, info.lines))
   { view->first.line = max((size_t)1, info.lines); view->first.offset = 0; }
   if (view->initialized && view->revision == info.revision && !view->request && view->budget_lines == budget) return;
   if (view->request)
   {
      view->tail = ending;
      if (view->request == -2) { view->first.line = 1; view->first.offset = 0; view->previous_count = 0; }
      else if (view->request == -1)
      {
         if (view->previous_count) view->first = view->previous[--view->previous_count];
         else { view->first.line = view->first.line > (size_t)budget ? view->first.line - budget : 1; view->first.offset = 0; }
      }
      else if (view->request == 1 && view->more)
      {
         if (view->previous_count == 32)
         { memmove(view->previous, view->previous + 1, 31 * sizeof(view->previous[0])); view->previous_count--; }
         view->previous[view->previous_count++] = view->first; view->first = view->next;
      }
      else if (view->request == 2)
      {
         view->first.line = max((size_t)1, info.lines); view->first.offset = 0;
         if (the_workbench_document_read(panel->document_id, info.revision, view->first.line, 0, 1,
             THE_WORKBENCH_DOCUMENT_BYTES_MAX, &page) == THE_WORKBENCH_DOCUMENT_OK && page.count)
            view->first.offset = page.slices[0].original_length > THE_WORKBENCH_DOCUMENT_BYTES_MAX
               ? page.slices[0].original_length - THE_WORKBENCH_DOCUMENT_BYTES_MAX : 0;
      }
      view->request = 0;
   }
   cursor = view->first;
   do
   {
      unsigned retry = 0;
      do {
         result = the_workbench_document_read(panel->document_id, info.revision, cursor.line, cursor.offset,
            min((size_t)THE_WORKBENCH_DOCUMENT_LINES_MAX, (size_t)budget - lines),
            THE_WORKBENCH_DOCUMENT_BYTES_MAX - raw, &page);
         /* Align forward: a backwards adjustment would omit EOF bytes from
          * this fixed-size tail window. */
         if (result != THE_WORKBENCH_DOCUMENT_INVALID || !cursor.offset || !ending || retry++ == 3) break;
         cursor.offset++; view->first = cursor;
      } while (1);
      if (result == THE_WORKBENCH_DOCUMENT_INVALID && cursor.offset && view->revision != info.revision)
      {
         cursor.offset = 0; view->first = cursor;
         result = the_workbench_document_read(panel->document_id, info.revision, cursor.line, 0,
            min((size_t)THE_WORKBENCH_DOCUMENT_LINES_MAX, (size_t)budget - lines), THE_WORKBENCH_DOCUMENT_BYTES_MAX - raw, &page);
      }
      if (result != THE_WORKBENCH_DOCUMENT_OK) break;
      for (i = 0; i < page.count; i++)
      {
         const TheWorkbenchDocumentSlice *slice = page.slices + i;
         const CHARTYPE *bytes = page.bytes + slice->data_offset;
         TextPos pos = textpos_begin();
         while (pos.byte_offset < slice->length)
         {
            TextCodepoint cp = textpos_codepoint_at_boundary(bytes, slice->length, pos);
            size_t n = cp.valid ? cp.byte_length : 1;
            if (!cp.valid || cp.codepoint < 32 || cp.codepoint == 127)
               used += snprintf(text + used, sizeof(text) - used, "\\x%02X", bytes[pos.byte_offset]);
            else if (cp.codepoint >= 128 && cp.codepoint <= 159)
               used += snprintf(text + used, sizeof(text) - used, "\\u%04X", (unsigned)cp.codepoint);
            else { memcpy(text + used, bytes + pos.byte_offset, n); used += n; }
            pos.byte_offset += n;
         }
         if (slice->offset + slice->length == slice->original_length) text[used++] = '\n';
      }
      raw += page.length; lines += page.count;
      cursor.line = page.next_line; cursor.offset = page.next_offset;
      view->more = page.more;
   } while (page.more && lines < (size_t)budget && raw < THE_WORKBENCH_DOCUMENT_BYTES_MAX);
   if (result == THE_WORKBENCH_DOCUMENT_OK || (raw && result == THE_WORKBENCH_DOCUMENT_LIMIT))
   {
      text[used] = 0; memcpy(view->text, text, used + 1); view->length = used;
      view->next = cursor; view->revision = info.revision; view->budget_lines = budget; view->initialized = 1;
   }
   else snprintf(view->reason, sizeof(view->reason), "%s", the_workbench_document_result_name(result));
}

static void native_document_draw(const TheWorkbenchPanel *panel)
{
   int index = native_log_view(panel), rows = console_rows - 3, lines;
   NativeDocumentView *view;
   char heading[THE_WORKBENCH_LABEL_MAX + 80], status[160];
   if (index < 0) return;
   view = &log_ui.views[index].document;
   if (native_log_placement(panel) == THE_WORKBENCH_PLACE_NONE)
   {
      const char *reason = "Document collapsed: insufficient space. Esc returns; H hides.";
      console_write_text(0, 0, reason, strlen(reason), console_cols - 1);
      wnoutrefresh(console_window); return;
   }
   native_document_refresh(panel, view);
   wattrset(console_window, console_theme_attr(log_ui.focused ? ATTR_CONSOLESELECTED : ATTR_CONSOLEHEADER, 0));
   snprintf(heading, sizeof(heading), " Document: %s | Read-only%s", panel->title, log_ui.focused ? " | Esc Return" : "");
   console_write_text(0, 0, heading, strlen(heading), console_cols - 1);
   wattrset(console_window, console_theme_attr(ATTR_CONSOLECONTROL, 0));
   console_write_text(1, 1, "Tab panes  PgUp/PgDn Home/End  H Hide", strlen("Tab panes  PgUp/PgDn Home/End  H Hide"), console_cols - 2);
   memset(log_render_streams, 1, view->length);
   lines = console_output_rows(view->text, log_render_streams, view->length, console_cols - 1, 0, 2, 0, NULL, 0);
   if (view->tail) log_ui.views[index].scroll = max(0, lines - rows);
   log_ui.views[index].scroll = min(log_ui.views[index].scroll, max(0, lines - rows));
   log_ui.views[index].lines = lines;
   console_output_rows(view->text, log_render_streams, view->length, console_cols - 1, log_ui.views[index].scroll, 2, rows, NULL, 0);
   wattrset(console_window, console_theme_attr(ATTR_CONSOLESTATUS, 0));
   if (view->reason[0]) snprintf(status, sizeof(status), " Read-only | %s", view->reason);
   else snprintf(status, sizeof(status), " Read-only | Line %lu/%lu%s", (unsigned long)view->first.line,
      (unsigned long)view->total_lines, view->first.offset ? " (continued)" : "");
   console_write_text(console_rows - 1, 0, status, strlen(status), console_cols - 1);
   wmove(console_window, 0, 1); leaveok(console_window, log_ui.focused ? FALSE : TRUE);
   wnoutrefresh(console_window);
}

static int native_form_event(const TheWorkbenchPanel *panel, NativeFormView *view,
   TheWorkbenchFormOperation operation, TheWorkbenchFormEdit edit, uint32_t codepoint, TheInputEvent *event)
{
   TheWorkbenchFormRequest request = {0};
   const TheWorkbenchField *field = native_form_field(panel->form, view->active);
   native_form_sync(panel, view, 0);
   if (view->pending || view->blocked) return 0;
   request.operation = operation; request.edit = edit; request.cell = view->cell; request.codepoint = codepoint;
   request.request_id = ++native_form_request_id; request.form_generation = view->generation; request.draft_revision = view->revision;
   strcpy(request.panel, panel->id); request.panel_generation = panel->generation; strcpy(request.form, panel->form);
   if (operation == THE_WORKBENCH_FORM_EDIT)
   { if (!field) return 0; strcpy(request.field, field->id); }
   if (!the_input_event_from_workbench_form(&request, event)) return 0;
   view->pending = request.request_id;
   return 1;
}

static int native_filefields_event(const TheWorkbenchPanel *panel, NativeFileFieldsView *view,
   TheWorkbenchFileFieldsOperation operation, TheWorkbenchFormEdit edit, uint32_t codepoint, TheInputEvent *event)
{
   TheWorkbenchFileFieldsRequest request = {0};
   const TheWorkbenchFileFieldsState *state;
   const TheWorkbenchFileFieldSpan *span;
   native_filefields_sync(panel, view);
   state = the_workbench_filefields_state(panel->id);
   if (!state || view->pending) return 0;
   if (operation != THE_WORKBENCH_FILEFIELDS_RELOAD && operation != THE_WORKBENCH_FILEFIELDS_CANCEL && state->draft_revision != view->revision) return 0;
   request.operation = operation; request.edit = edit; request.cell = view->cell; request.codepoint = codepoint;
   request.request_id = ++native_form_request_id; request.panel_generation = panel->generation;
   request.draft_revision = state->draft_revision; strcpy(request.panel, panel->id);
   if (operation == THE_WORKBENCH_FILEFIELDS_EDIT)
   {
      span = the_workbench_filefields_span(panel->id, view->active);
      if (!span || !span->editable) return 0;
      strcpy(request.field, span->id);
   }
   if (!the_input_event_from_workbench_filefields(&request, event)) return 0;
   view->pending = request.request_id;
   return 1;
}

static const char *native_filefields_controls[] = { "Apply", "Cancel", "Reload", "Hide", "Return" };
static const int native_filefields_control_cells[] = { 0, 6, 13, 20, 25 };

static void native_filefields_draw(const TheWorkbenchPanel *panel)
{
   int index = native_log_view(panel), rows = max(1, console_rows - 3), i, cursor_row = 1, cursor_col = 1;
   const TheWorkbenchFileFieldsState *state = the_workbench_filefields_state(panel->id);
   const TheWorkbenchFileFieldSpan *active;
   NativeFileFieldsView *view;
   char heading[THE_WORKBENCH_LABEL_MAX + 40], status[192];
   if (index < 0) return;
   view = &log_ui.views[index].filefields; native_filefields_sync(panel, view);
   wattrset(console_window, console_theme_attr(log_ui.focused ? ATTR_CONSOLESELECTED : ATTR_CONSOLEHEADER, 0));
   snprintf(heading, sizeof(heading), " File fields: %s", panel->title);
   console_write_text(0, 0, heading, strlen(heading), console_cols - 1);
   for (i = 0; i < 5; i++)
   {
      wattrset(console_window, console_theme_attr(state && view->active == (int)state->spans + i ? ATTR_CONSOLESELECTED : ATTR_CONSOLECONTROL, 0));
      console_write_text(1, native_filefields_control_cells[i], native_filefields_controls[i], strlen(native_filefields_controls[i]), console_cols - native_filefields_control_cells[i] - 1);
   }
   active = state && view->active < (int)state->spans ? the_workbench_filefields_span(panel->id, view->active) : NULL;
   if (active && view->follow)
   {
      int caret = active->cell + min(view->cell, max(0, active->maxcells - 1));
      if ((int)active->line - 1 < view->top) view->top = (int)active->line - 1;
      if ((int)active->line - 1 >= view->top + rows) view->top = (int)active->line - rows;
      if (caret < view->left) view->left = caret;
      if (caret >= view->left + console_cols - 2) view->left = caret - console_cols + 3;
   }
   view->follow = 0;
   if (state && native_log_placement(panel) != THE_WORKBENCH_PLACE_NONE)
      for (i = 0; i < (int)state->spans; i++)
      {
         const TheWorkbenchFileFieldSpan *span = the_workbench_filefields_span(panel->id, i);
         int row, first, width, column;
         TextCellSlice slice;
         if (!span) continue;
         row = (int)span->line - 1 - view->top + 2;
         if (row < 2 || row >= console_rows - 1) continue;
         first = max(0, view->left - span->cell); column = max(1, span->cell - view->left + 1);
         width = min(span->maxcells - first, console_cols - column - 1);
         if (width <= 0) continue;
         wattrset(console_window, console_theme_attr(span->editable ? ATTR_CONSOLEINPUT : ATTR_CONSOLEBODY, 0));
         mvwhline(console_window, row, column, ' ', width);
         slice = textpos_slice_cells((const CHARTYPE *)span->value, strlen(span->value), first, width);
         /* Preserve whole clusters when the left viewport crosses a wide glyph. */
         column += max(0, slice.start.cell_column - first);
         console_write_text(row, column, span->value + slice.start.byte_offset, slice.end.byte_offset - slice.start.byte_offset, console_cols - column - 1);
      }
   if (state && view->active >= (int)state->spans) cursor_col = native_filefields_control_cells[view->active - (int)state->spans];
   else if (active)
   {
      cursor_row = (int)active->line - 1 - view->top + 2;
      cursor_col = active->cell + min(view->cell, max(0, active->maxcells - 1)) - view->left + 1;
      if (cursor_row < 2 || cursor_row >= console_rows - 1 || cursor_col < 1 || cursor_col >= console_cols - 1) { cursor_row = 0; cursor_col = 1; }
   }
   wattrset(console_window, console_theme_attr(ATTR_CONSOLESTATUS, 0));
   if (view->error[0]) snprintf(status, sizeof(status), " %s", view->error);
   else if (native_log_placement(panel) == THE_WORKBENCH_PLACE_NONE) snprintf(status, sizeof(status), " Collapsed; Tab controls; Esc Return");
   else snprintf(status, sizeof(status), " %s | Tab fields/buttons F12 panes Esc Return | PgUp/PgDn scroll", state && state->active ? state->dirty ? "Draft" : "Ready" : "Cancelled; Reload to edit");
   console_write_text(console_rows - 1, 0, status, strlen(status), console_cols - 1);
   wmove(console_window, cursor_row, min(console_cols - 2, cursor_col)); leaveok(console_window, log_ui.focused ? FALSE : TRUE);
   wnoutrefresh(console_window);
}

static int native_filefields_key(const TheWorkbenchPanel *panel, int key, TheInputEvent *event)
{
   int index = native_log_view(panel), step, maximum;
   const TheWorkbenchFileFieldsState *state = the_workbench_filefields_state(panel->id);
   const TheWorkbenchFileFieldSpan *span;
   NativeFileFieldsView *view;
   TheInputEvent input = *event;
   if (key == 27) return native_log_event(panel, THE_WORKBENCH_PANEL_RETURN, event);
   if (index < 0 || !state) return 0;
   view = &log_ui.views[index].filefields; native_filefields_sync(panel, view);
   if (key == 9 || key == THE_KEY_BTAB)
   {
      step = key == 9 ? 1 : -1;
      do { view->active = (view->active + (int)state->spans + 5 + step) % ((int)state->spans + 5); }
      while (view->active < (int)state->spans && !the_workbench_filefields_span(panel->id, view->active)->editable);
      view->cell = 0; view->follow = 1; curses_driver_update(); return 0;
   }
   if (key == THE_KEY_UP || key == THE_KEY_DOWN || key == THE_KEY_PPAGE || key == THE_KEY_NPAGE)
   {
      maximum = max(0, (int)state->records - max(1, console_rows - 3));
      step = key == THE_KEY_UP || key == THE_KEY_DOWN ? 1 : max(1, console_rows - 4);
      if (key == THE_KEY_UP || key == THE_KEY_PPAGE) step = -step;
      view->top = min(maximum, max(0, view->top + step)); curses_driver_update(); return 0;
   }
   if (view->active >= (int)state->spans)
   {
      if (key == THE_KEY_LEFT || key == THE_KEY_RIGHT) { view->left = max(0, view->left + (key == THE_KEY_LEFT ? -8 : 8)); curses_driver_update(); return 0; }
      if (key != 10 && key != 13 && key != THE_KEY_ENTER && key != ' ') return 0;
      step = view->active - (int)state->spans;
      if (step == 3 || step == 4) return native_log_event(panel, step == 3 ? THE_WORKBENCH_PANEL_HIDE : THE_WORKBENCH_PANEL_RETURN, event);
      return native_filefields_event(panel, view, step == 0 ? THE_WORKBENCH_FILEFIELDS_APPLY : step == 1 ? THE_WORKBENCH_FILEFIELDS_CANCEL : THE_WORKBENCH_FILEFIELDS_RELOAD, 0, 0, event);
   }
   span = the_workbench_filefields_span(panel->id, view->active);
   if (!span) return 0;
   if (key == THE_KEY_LEFT || key == THE_KEY_RIGHT || key == THE_KEY_HOME || key == THE_KEY_END)
   {
      size_t length = strlen(span->value);
      TextPos at = textpos_from_cell((const CHARTYPE *)span->value, length, view->cell, TEXT_SNAP_BACKWARD);
      if (key == THE_KEY_LEFT) view->cell = textpos_prev_cluster((const CHARTYPE *)span->value, length, at).cell_column;
      else if (key == THE_KEY_RIGHT) view->cell = textpos_next_cluster((const CHARTYPE *)span->value, length, at).cell_column;
      else view->cell = key == THE_KEY_HOME ? 0 : span->maxcells;
      view->follow = 1; curses_driver_update(); return 0;
   }
   if (key == 8 || key == 127 || key == THE_KEY_BACKSPACE) return native_filefields_event(panel, view, THE_WORKBENCH_FILEFIELDS_EDIT, THE_WORKBENCH_FORM_BACKSPACE, 0, event);
   if (key == THE_KEY_DC) return native_filefields_event(panel, view, THE_WORKBENCH_FILEFIELDS_EDIT, THE_WORKBENCH_FORM_DELETE, 0, event);
   if (key == 21) return native_filefields_event(panel, view, THE_WORKBENCH_FILEFIELDS_EDIT, THE_WORKBENCH_FORM_CLEAR, 0, event);
   if (input.kind == THE_INPUT_TEXT && input.codepoint >= 32) return native_filefields_event(panel, view, THE_WORKBENCH_FILEFIELDS_EDIT, THE_WORKBENCH_FORM_INSERT, input.codepoint, event);
   if (key >= 32 && key < 127) return native_filefields_event(panel, view, THE_WORKBENCH_FILEFIELDS_EDIT, THE_WORKBENCH_FORM_INSERT, (uint32_t)key, event);
   return 0;
}

static void native_form_draw(const TheWorkbenchPanel *panel)
{
   int index = native_log_view(panel), visible = max(1, (console_rows - 3) / 2), row, n, cursor_row = 1, cursor_col = 1;
   const TheWorkbenchFormState *state = the_workbench_form_state(panel->form);
   NativeFormView *view;
   char heading[THE_WORKBENCH_LABEL_MAX + 32], status[160];
   if (index < 0) return;
   view = &log_ui.views[index].form; native_form_sync(panel, view, 0);
   wattrset(console_window, console_theme_attr(log_ui.focused ? ATTR_CONSOLESELECTED : ATTR_CONSOLEHEADER, 0));
   snprintf(heading, sizeof(heading), " Form: %s", panel->title);
   console_write_text(0, 0, heading, strlen(heading), console_cols - 1);
   wattrset(console_window, console_theme_attr(ATTR_CONSOLECONTROL, 0));
   console_write_text(1, 1, "Submit  Cancel  Return", 22, console_cols - 2);
   if (state)
   {
      if (view->active < (int)state->fields)
      {
         if (view->active < view->top) view->top = view->active;
         if (view->active >= view->top + visible) view->top = view->active - visible + 1;
      }
      for (n = view->top, row = 2; n < (int)state->fields && n < view->top + visible && row + 1 < console_rows - 1; n++, row += 2)
      {
         const TheWorkbenchField *field = native_form_field(panel->form, n);
         const TheWorkbenchFieldState *value = field ? the_workbench_field_state(panel->form, field->id) : NULL;
         char label[THE_WORKBENCH_LABEL_MAX + 32];
         int left = n == view->active ? max(0, view->cell - console_cols + 4) : 0;
         TextCellSlice slice;
         if (!field || !value) continue;
         snprintf(label, sizeof(label), "%s%s", field->label, field->access == THE_WORKBENCH_FIELD_READONLY ? " [Read-only]" : "");
         wattrset(console_window, console_theme_attr(ATTR_CONSOLESTATUS, 0));
         console_write_text(row, 1, label, strlen(label), console_cols - 2);
         wattrset(console_window, console_theme_attr(field->access == THE_WORKBENCH_FIELD_READONLY ? ATTR_CONSOLESTATUS : ATTR_CONSOLEINPUT, 0));
         mvwhline(console_window, row + 1, 1, ' ', console_cols - 2);
         slice = textpos_slice_cells((const CHARTYPE *)value->value, value->length, left, console_cols - 3);
         console_write_text(row + 1, 1, value->value + slice.start.byte_offset,
            slice.end.byte_offset - slice.start.byte_offset, console_cols - 3);
         if (n == view->active) { cursor_row = row + 1; cursor_col = min(console_cols - 2, 1 + view->cell - left); }
      }
      if (view->active >= (int)state->fields) cursor_col = 1 + 8 * (view->active - (int)state->fields);
   }
   wattrset(console_window, console_theme_attr(ATTR_CONSOLESTATUS, 0));
   if (view->error[0]) snprintf(status, sizeof(status), " %s", view->error);
   else snprintf(status, sizeof(status), " %s | Tab fields  F12 panes  Esc Return", state && state->dirty ? "Draft" : state && state->submitted ? "Submitted" : "Ready");
   console_write_text(console_rows - 1, 0, status, strlen(status), console_cols - 1);
   wmove(console_window, cursor_row, cursor_col); leaveok(console_window, log_ui.focused ? FALSE : TRUE);
   wnoutrefresh(console_window);
}

static int native_form_key(const TheWorkbenchPanel *panel, int key, TheInputEvent *event)
{
   int index = native_log_view(panel), step;
   const TheWorkbenchFormState *state = the_workbench_form_state(panel->form);
   NativeFormView *view;
   const TheWorkbenchField *field;
   const TheWorkbenchFieldState *value;
   TheInputEvent input = *event;
   if (index < 0 || !state) return 0;
   view = &log_ui.views[index].form; native_form_sync(panel, view, 0);
   if (key == 27) return native_log_event(panel, THE_WORKBENCH_PANEL_RETURN, event);
   if (key == 9 || key == THE_KEY_BTAB)
   {
      step = key == 9 ? 1 : -1;
      do { view->active = (view->active + (int)state->fields + 3 + step) % ((int)state->fields + 3); }
      while (view->active < (int)state->fields && native_form_field(panel->form, view->active)->access != THE_WORKBENCH_FIELD_EDIT);
      view->cell = 0; curses_driver_update(); return 0;
   }
   if (view->active >= (int)state->fields)
   {
      if (key != 10 && key != 13 && key != THE_KEY_ENTER && key != ' ') return 0;
      step = view->active - (int)state->fields;
      if (step == 2) return native_log_event(panel, THE_WORKBENCH_PANEL_RETURN, event);
      return native_form_event(panel, view, step ? THE_WORKBENCH_FORM_CANCEL : THE_WORKBENCH_FORM_SUBMIT, 0, 0, event);
   }
   field = native_form_field(panel->form, view->active);
   value = field ? the_workbench_field_state(panel->form, field->id) : NULL;
   if (!value) return 0;
   if (key == THE_KEY_LEFT || key == THE_KEY_RIGHT || key == THE_KEY_HOME || key == THE_KEY_END)
   {
      TextPos at = textpos_from_cell((const CHARTYPE *)value->value, value->length, view->cell, TEXT_SNAP_BACKWARD);
      if (key == THE_KEY_LEFT) view->cell = textpos_prev_cluster((const CHARTYPE *)value->value, value->length, at).cell_column;
      else if (key == THE_KEY_RIGHT) view->cell = textpos_next_cluster((const CHARTYPE *)value->value, value->length, at).cell_column;
      else view->cell = key == THE_KEY_HOME ? 0 : textpos_from_byte((const CHARTYPE *)value->value, value->length, value->length).cell_column;
      curses_driver_update(); return 0;
   }
   if (key == 8 || key == 127 || key == THE_KEY_BACKSPACE)
      return native_form_event(panel, view, THE_WORKBENCH_FORM_EDIT, THE_WORKBENCH_FORM_BACKSPACE, 0, event);
   if (key == THE_KEY_DC) return native_form_event(panel, view, THE_WORKBENCH_FORM_EDIT, THE_WORKBENCH_FORM_DELETE, 0, event);
   if (key == 21) { view->cell = 0; return native_form_event(panel, view, THE_WORKBENCH_FORM_EDIT, THE_WORKBENCH_FORM_CLEAR, 0, event); }
   if (input.kind == THE_INPUT_TEXT && input.codepoint >= 32)
      return native_form_event(panel, view, THE_WORKBENCH_FORM_EDIT, THE_WORKBENCH_FORM_INSERT, input.codepoint, event);
   if (key >= 32 && key < 127)
      return native_form_event(panel, view, THE_WORKBENCH_FORM_EDIT, THE_WORKBENCH_FORM_INSERT, (uint32_t)key, event);
   return 0;
}

static void native_log_draw(const TheWorkbenchPanel *panel)
{
   uint64_t omitted;
   size_t length = native_log_text(panel, &omitted);
   int view = native_log_view(panel), rows = console_rows - 3, lines, scroll = 0, skip;
   size_t anchor = 0, i;
   char heading[THE_WORKBENCH_LABEL_MAX + 100], status[160];
   if (view < 0) return;
   if (native_log_placement(panel) == THE_WORKBENCH_PLACE_NONE)
   {
      const char *reason = "Log collapsed: insufficient space. Esc returns; H hides.";
      console_write_text(0, 0, reason, strlen(reason), console_cols - 1);
      wnoutrefresh(console_window); return;
   }
   wattrset(console_window, console_theme_attr(log_ui.focused ? ATTR_CONSOLESELECTED : ATTR_CONSOLEHEADER, 0));
   snprintf(heading, sizeof(heading), " Log: %s%s", panel->title,
            log_ui.focused ? " | Esc Return" : " | read-only");
   console_write_text(0, 0, heading, strlen(heading), console_cols - 1);
   wattrset(console_window, console_theme_attr(ATTR_CONSOLECONTROL, 0));
   {
      const char *controls = "Tab panes  PgUp/PgDn Home/End  H Hide";
      console_write_text(1, 1, controls, strlen(controls), console_cols - 2);
   }
   lines = console_output_rows(log_render_text, log_render_streams, length, console_cols - 1, 0, 2, 0, NULL, 0);
   if (log_ui.views[view].scroll && log_ui.views[view].anchor_cursor &&
       (log_ui.views[view].revision != log_render_revision || log_ui.views[view].width != console_cols))
   {
      /* Retained record cursors anchor the viewport even when a new append
       * evicts old records and leaves the total retained row count unchanged. */
      skip = 0;
      for (i = 0; i < log_render_record_count; i++)
         if (log_render_records[i].cursor == log_ui.views[view].anchor_cursor)
         {
            TextCluster cluster;
            anchor = min(length, log_render_records[i].start + log_ui.views[view].anchor_offset);
            cluster = textpos_cluster_at_boundary((const CHARTYPE *)log_render_text, length,
               textpos_from_byte((const CHARTYPE *)log_render_text, length, anchor));
            skip = max(0, console_output_rows(log_render_text, log_render_streams,
               min(length, cluster.end.byte_offset), console_cols - 1, 0, 2, 0, NULL, 0) - 1);
            break;
         }
      log_ui.views[view].scroll = max(0, lines - rows - skip);
   }
   log_ui.views[view].lines = lines; log_ui.views[view].revision = log_render_revision;
   log_ui.views[view].width = console_cols;
   log_ui.views[view].scroll = min(log_ui.views[view].scroll, max(0, lines - rows));
   scroll = log_ui.views[view].scroll; skip = max(0, lines - rows - scroll);
   console_output_rows(log_render_text, log_render_streams, length, console_cols - 1,
                       skip, 2, rows, &anchor, skip);
   log_ui.views[view].anchor_cursor = 0;
   for (i = 0; i < log_render_record_count; i++)
      if (log_render_records[i].start <= anchor)
      {
         log_ui.views[view].anchor_cursor = log_render_records[i].cursor;
         log_ui.views[view].anchor_offset = anchor - log_render_records[i].start;
      }
   wattrset(console_window, console_theme_attr(ATTR_CONSOLESTATUS, 0));
   if (the_workbench_model_state(panel->model) && the_workbench_model_state(panel->model)->last_error != THE_WORKBENCH_LOG_OK)
      snprintf(status, sizeof(status), " Log output error: %s", native_log_error_name(the_workbench_model_state(panel->model)->last_error));
   else if (omitted) snprintf(status, sizeof(status), " History omitted: %llu bytes", (unsigned long long)omitted);
   else snprintf(status, sizeof(status), " Read-only | %lu visible log%s", (unsigned long)native_log_count(), native_log_count() == 1 ? "" : "s");
   console_write_text(console_rows - 1, 0, status, strlen(status), console_cols - 1);
   wmove(console_window, 0, 1); leaveok(console_window, log_ui.focused ? FALSE : TRUE);
   wnoutrefresh(console_window);
}

static const char *const native_tools_controls[] = {"Profiles", "Tools", "Build", "Edit", "Hide", "Editor"};
static int native_tools_top;

static int native_tools_control_event(int control, TheInputEvent *event)
{
   if (control < 3) { native_tools_section = control; native_tools_top = 0; }
   else if (control == 3)
   {
      native_editor_focus();
      return the_tools_prepare_input("profile", "", "", 0, NULL, event);
   }
   else if (control == 4) native_show_tools(native_tools_section, 1);
   else native_editor_focus();
   return 0;
}

static const char *native_path_name(const char *path)
{
   const char *name = strrchr(path, '/');
#ifdef _WIN32
   const char *backslash = strrchr(path, '\\');
   if (backslash && (!name || backslash > name)) name = backslash;
#endif
   return name ? name + 1 : path;
}

static void native_tools_draw(void)
{
   const TheToolConsole *build = the_tool_build_console();
   static char output[THE_TOOL_OUTPUT + 1];
   static unsigned char streams[THE_TOOL_OUTPUT];
   char line[THE_TOOL_PATH + 300];
   int row, col = 1, w = console_cols, h = console_rows;
   size_t i, length = 0;
   const char *section = native_tools_section == 0 ? "Profiles" : native_tools_section == 1 ? "File types & tools" : "Build results";
   wattrset(console_window, console_theme_attr(ATTR_CONSOLEHEADER, 0));
   mvwhline(console_window, 0, 0, ' ', w);
   snprintf(line, sizeof(line), " Tools & Profiles | %s", section);
   console_write_text(0, 0, line, strlen(line), w - 1);
   for (i = 0; i < sizeof(native_tools_controls) / sizeof(native_tools_controls[0]); i++)
   {
      wattrset(console_window, console_theme_attr(native_tools_focused && native_tools_control == (int)i
         ? ATTR_CONSOLESELECTED : ATTR_CONSOLECONTROL, THE_STYLE_BOLD));
      col += console_write_text(1, col, native_tools_controls[i], strlen(native_tools_controls[i]), w - col - 1) + 1;
   }
   wattrset(console_window, console_theme_attr(ATTR_CONSOLESTATUS, 0));
   if (native_tools_section == 2)
   {
      const char *state = native_file.result.serial && strcmp(native_file.result.action, "run")
         ? native_file.result.message : build->id ? build->state : "Build the current source; details appear here.";
      console_write_text(2, 1, state, strlen(state), w - 2);
      for (i = 0; i < build->count && length < THE_TOOL_OUTPUT; i++)
      {
         size_t bytes = min(build->chunks[i].length, THE_TOOL_OUTPUT - length);
         memcpy(output + length, build->chunks[i].text, bytes);
         memset(streams + length, build->chunks[i].fd, bytes); length += bytes;
      }
      output[length] = 0;
      row = console_output_rows(output, streams, length, w - 1, 0, 3, 0, NULL, 0);
      if (native_build_reader.id != build->id || native_build_reader.epoch != build->history_epoch)
      {
         native_build_reader.id = build->id; native_build_reader.epoch = build->history_epoch;
         native_build_reader.offset = native_build_reader.unread = 0;
      }
      else if (native_build_reader.offset)
      {
         if (build->displayed > native_build_reader.displayed) native_build_reader.unread = 1;
         if (row > native_build_reader.lines) native_build_reader.offset += row - native_build_reader.lines;
      }
      native_build_reader.displayed = build->displayed;
      native_build_reader.lines = row; native_build_reader.page = max(1, h - 5);
      native_build_reader.offset = min(native_build_reader.offset, max(0, row - (h - 4)));
      if (!native_build_reader.offset) native_build_reader.unread = 0;
      console_output_rows(output, streams, length, w - 1,
         max(0, row - (h - 4) - native_build_reader.offset), 3, h - 4, NULL, 0);
   }
   else if (native_tools_section == 1)
   {
      for (row = 2, i = (size_t)native_tools_top; row < h - 1 && i < the_tool_count(); row++, i++)
      {
         const TheToolDefinition *tool = the_tool_at(i);
         snprintf(line, sizeof(line), "%s: %s (%s; %s)", *tool->extensions ? tool->extensions : tool->id,
            tool->label, tool->role, tool->available ? "available" : "unavailable");
         console_write_text(row, 1, line, strlen(line), w - 2);
      }
      if (!the_tool_count())
      {
         const char *note = "No file tools registered. Edit profile to define them.";
         console_write_text(2, 1, note, strlen(note), w - 2);
      }
   }
   else
   {
      const TheToolsProfileSnapshot *profile = &native_file.profile;
      snprintf(line, sizeof(line), "Active profile: %s", *profile->active.image
         ? native_path_name(*profile->active.source_path ? profile->active.source_path : profile->active.image) : "Stock defaults");
      console_write_text(2, 1, line, strlen(line), w - 2);
      snprintf(line, sizeof(line), "%s", *profile->active.image
         ? profile->active.startup_saved ? "Applied; startup selection saved" : "Applied; startup selection not saved" : "No user profile applied");
      console_write_text(3, 1, line, strlen(line), w - 2);
      snprintf(line, sizeof(line), "Editable source: %s", native_path_name(profile->source_path));
      console_write_text(4, 1, line, strlen(line), w - 2);
      if (h > 7)
      {
         snprintf(line, sizeof(line), "Latest build: %s%s%s", profile->build.state,
            *profile->build.name ? " | " : "", profile->build.name);
         console_write_text(5, 1, line, strlen(line), w - 2);
      }
      if (h > 8)
      {
         const char *note = "Edit profile, then Build profile and Apply profile from F10.";
         console_write_text(6, 1, note, strlen(note), w - 2);
      }
   }
   wattrset(console_window, console_theme_attr(ATTR_CONSOLESTATUS, 0));
   {
      const char *hint = native_tools_section == 2 ? native_build_reader.offset
         ? native_build_reader.unread ? " New output | End: Latest" : " Earlier output | End: Latest"
         : " Read-only | PgUp/PgDn Home/End | F10 actions"
         : native_tools_section == 1 ? " Edit profile: associations | PgUp/PgDn"
         : " F10: file actions | Edit: profile source";
      console_write_text(h - 1, 0, hint, strlen(hint), w - 1);
   }
   col = 1;
   for (i = 0; i < (size_t)native_tools_control; i++) col += (int)strlen(native_tools_controls[i]) + 1;
   wmove(console_window, 1, min(w - 1, col));
   leaveok(console_window, native_tools_focused ? FALSE : TRUE);
   wnoutrefresh(console_window);
}

static void console_draw(void)
{
   const TheToolConsole *c = the_tool_program_console();
   static char text[THE_TOOL_OUTPUT + 1];
   static unsigned char streams[THE_TOOL_OUTPUT];
   char heading[384], context[THE_TOOL_PATH + 160], state[128];
   const char *prompt = c->running && !c->eof ? "Program input> " : "Input closed> ";
   int h = console_rows, w = console_cols, row = h >= 8 ? 4 : 3;
   int output_rows = h - row - 1, lines, input_col, input_cells;
   int i, col = 1;
   size_t length = 0, chunk;
   TextCellSlice input_slice;

   if (!h) return;
   if (!console_window)
   { console_window = newwin(h, w, console_top, console_left); if (console_window) keypad(console_window, TRUE); }
   if (!console_window) return;
   if (!console_divider)
      console_divider = console_left
         ? newwin(h, 1, console_top, console_left - 1)
         : newwin(1, w, console_top - 1, 0);
   if (console_divider)
   {
      wattrset(console_divider, console_theme_attr(ATTR_CONSOLEDIVIDER, 0));
      wmove(console_divider, 0, 0);
      if (console_left) wvline(console_divider, '|', h);
      else whline(console_divider, '-', w);
      leaveok(console_divider, TRUE);
      wnoutrefresh(console_divider);
   }
   werase(console_window);
   /* Some supported curses configurations lack wbkgd(). Paint the blanks
    * explicitly so the pane never inherits the terminal's default colours. */
   wattrset(console_window, console_theme_attr(ATTR_CONSOLEBODY, 0));
   for (i = 0; i < h; i++) mvwhline(console_window, i, 0, ' ', w);
   if (native_tools_visible) { native_tools_draw(); return; }
   if (native_selected_log())
   {
      if (native_selected_log()->kind == THE_WORKBENCH_PANEL_FIELDS && native_selected_log()->document_id) native_filefields_draw(native_selected_log());
      else if (native_selected_log()->kind == THE_WORKBENCH_PANEL_FIELDS) native_form_draw(native_selected_log());
      else if (native_selected_log()->kind == THE_WORKBENCH_PANEL_DOCUMENT) native_document_draw(native_selected_log());
      else native_log_draw(native_selected_log());
      return;
   }
   snprintf(heading, sizeof(heading), " Program console | %s",
      c->focused ? (w < 40 ? "Esc Cmd" : "Esc Cmd F6 File")
         : (w >= 64 ? "RX CONSOLE: input" : "F10: Menu"));
   wattrset(console_window, console_theme_attr(ATTR_CONSOLEHEADER, 0));
   mvwhline(console_window, 0, 0, ' ', w);
   console_write_text(0, 0, heading, strlen(heading), w - 1);
   for (i = 0; i < console_control_count(); i++)
   {
      int enabled = i == 0 ? !*native_file.actions[1].tool || native_file.actions[1].enabled : i == 1 ? c->running : 1;
      int selected = c->focused && console_toolbar_focus && console_control == i;

      wattrset(console_window, console_theme_attr(
         selected ? ATTR_CONSOLESELECTED
                  : enabled ? ATTR_CONSOLECONTROL : ATTR_CONSOLESTATUS,
         enabled && !selected ? THE_STYLE_BOLD : 0));
      col += console_write_text(1, col, console_control_label(i), strlen(console_control_label(i)), w - col - 1);
      col++;
   }
   wattrset(console_window, console_theme_attr(ATTR_CONSOLESTATUS, 0));
   snprintf(state, sizeof(state), "%s%s", c->state,
      c->running && w < 64 ? " | ^D EOF ^C stop" : "");
   console_write_text(2, 1, state, strlen(state), w - 2);
   if (h >= 8)
   {
      wattrset(console_window, console_theme_attr(ATTR_CONSOLESTATUS, 0));
      if (w < 64)
         snprintf(context, sizeof(context), " Source: %s | PgUp/PgDn", *c->filename ? native_path_name(c->filename) : "none");
      else
         snprintf(context, sizeof(context), " %s | %s | source %u revision %u | PgUp/PgDn scroll",
            c->tool, c->filename, c->source_id, c->revision);
      console_write_text(3, 0, context, strlen(context), w - 1);
   }
   for (chunk = 0; chunk < c->count && length < THE_TOOL_OUTPUT; chunk++)
   {
      size_t bytes = min(c->chunks[chunk].length, THE_TOOL_OUTPUT - length);

      memcpy(text + length, c->chunks[chunk].text, bytes);
      memset(streams + length, c->chunks[chunk].fd, bytes);
      length += bytes;
   }
   text[length] = 0;
   lines = console_output_rows(text, streams, length, w - 1, 0, row, 0, NULL, 0);
   if (c->id != console_scroll_id || c->history_epoch != console_scroll_epoch)
   {
      console_scroll_id = c->id;
      console_scroll_epoch = c->history_epoch;
      console_scroll_offset = console_scroll_unread = 0;
   }
   else if (console_scroll_offset)
   {
      if (c->displayed > console_scroll_displayed) console_scroll_unread = 1;
      if (lines > console_scroll_lines) console_scroll_offset += lines - console_scroll_lines;
   }
   console_scroll_displayed = c->displayed;
   console_scroll_lines = lines;
   console_scroll_page = max(1, output_rows - 1);
   console_scroll_offset = min(console_scroll_offset, max(0, lines - output_rows));
   if (!console_scroll_offset) console_scroll_unread = 0;
   if (console_scroll_offset)
   {
      const char *hint = console_scroll_unread ? " New output | End: Latest" : " Earlier output | End: Latest";
      /* A six-row dock has no context row: put recovery in its header and
       * retain the independent completion/status row and Program input. */
      int hint_row = h >= 8 ? 3 : 0;
      wattrset(console_window, console_theme_attr(ATTR_CONSOLESTATUS, 0));
      mvwhline(console_window, hint_row, 0, ' ', w);
      console_write_text(hint_row, 0, hint, strlen(hint), w - 1);
   }
   console_output_rows(text, streams, length, w - 1,
                       max(0, lines - output_rows - console_scroll_offset),
                       row, output_rows, NULL, 0);
   wattrset(console_window, console_theme_attr(ATTR_CONSOLEINPUT, 0));
   mvwhline(console_window, h - 1, 0, ' ', w);
   input_col = console_write_text(h - 1, 0, prompt, strlen(prompt), w - 2);
   input_cells = w - input_col - 1;
   input_slice = textpos_slice_cells((const CHARTYPE *)c->input, c->input_length,
      max(0, textpos_from_byte((const CHARTYPE *)c->input, c->input_length,
         c->input_length).cell_column - input_cells + 1), input_cells - 1);
   input_col += console_write_text(h - 1, input_col,
      c->input + input_slice.start.byte_offset,
      input_slice.end.byte_offset - input_slice.start.byte_offset, input_cells - 1);
   if (!c->input_length)
   {
      const char *hint = console_toolbar_focus ? "Arrows/Enter"
         : c->running && !c->eof && w >= 64
            ? "Tab: controls  Ctrl-D: EOF  Ctrl-C: stop" : "Tab: controls";
      if (w - (int)strlen(hint) - 2 >= input_col)
         console_write_text(h - 1, w - (int)strlen(hint) - 2,
            hint, strlen(hint), (int)strlen(hint));
   }
   if (c->focused && console_toolbar_focus)
   {
      col = 1;
      for (i = 0; i < console_control; i++) col += (int)strlen(console_control_label(i)) + 1;
      wmove(console_window, 1, col);
   }
   else wmove(console_window, h - 1, input_col);
   leaveok(console_window, c->focused ? FALSE : TRUE);
   wnoutrefresh(console_window);
}
/* Labels are UTF-8 data; neither POPUP delimiters nor command syntax are
 * involved. The copied action identity survives a later graph publication. */
static struct
{
   WINDOW *window;
   int active, selected, first, count, rows, cols, top, left, prompting;
   unsigned long revision;
   char argument[THE_WORKBENCH_OPERAND_MAX + 1];
   int path_count, path_selected, path_first, path_more;
   TheToolsFileSnapshot file;
   size_t path_base_length;
   struct { char path[THE_WORKBENCH_OPERAND_MAX + 1]; int directory; } paths[128];
   struct
   {
      char item[THE_WORKBENCH_ID_MAX + 1], action[THE_WORKBENCH_ID_MAX + 1];
      char label[THE_WORKBENCH_LABEL_MAX * 2 + 5];
      unsigned long generation;
      int separator, kind;
   } entries[THE_WORKBENCH_ITEM_MAX + 12];
} native_menu;

enum { NATIVE_MENU_GRAPH, NATIVE_MENU_BUILD, NATIVE_MENU_RUN, NATIVE_MENU_APPLY,
       NATIVE_MENU_TOOLS, NATIVE_MENU_PROGRAM, NATIVE_MENU_HIDE_PROGRAM,
       NATIVE_MENU_EDIT_PROFILE, NATIVE_MENU_BUILD_RESULTS, NATIVE_MENU_CLEAR_PROGRAM };

static void native_menu_local(int kind, const char *id, const char *label, const char *selected)
{
   int entry = native_menu.count++;
   memset(native_menu.entries + entry, 0, sizeof(native_menu.entries[entry]));
   native_menu.entries[entry].kind = kind;
   snprintf(native_menu.entries[entry].item, sizeof(native_menu.entries[entry].item), "%s", id);
   snprintf(native_menu.entries[entry].label, sizeof(native_menu.entries[entry].label), "%s", label);
   if (!strcmp(selected, id)) native_menu.selected = entry;
}

static void native_menu_file_actions(const char *selected)
{
   size_t i;
   char label[THE_WORKBENCH_LABEL_MAX + 16], id[32];
   for (i = 0; i < THE_TOOLS_FILE_ACTION_COUNT; i++)
      if (*native_menu.file.actions[i].tool)
      {
         snprintf(label, sizeof(label), "Tools: %s", native_menu.file.actions[i].label);
         snprintf(id, sizeof(id), "curses.file.%lu", (unsigned long)i);
         native_menu_local(NATIVE_MENU_BUILD + (int)i, id, label, selected);
      }
   native_menu_local(NATIVE_MENU_EDIT_PROFILE, "curses.profile", "Tools: Edit profile", selected);
   native_menu_local(NATIVE_MENU_BUILD_RESULTS, "curses.build-results", "Tools: Build results", selected);
}

/* Only lower the stock shared-menu binding. Profiles retain authority to
 * redefine F10; modal READ and active macros always keep their own keys. */
static int shared_menu_key_binding(void)
{
   const char *definition = (const char *)get_key_definition(THE_KEY_F(10), THE_KEY_DEFINE_RAW, TRUE, FALSE);
   return definition && (!my_stricmp((DEFCHAR *)definition, "macro workbenchmenu")
      || !my_stricmp((DEFCHAR *)definition, "macro workbenchmenu_regina"));
}

static void native_menu_close(int canceled)
{
   if (native_menu.window) delwin(native_menu.window);
   native_menu.window = NULL;
   native_menu.active = native_menu.prompting = 0;
   if (canceled) workbench_restore_cancelled_menu();
   else workbench_menu_origin.active = 0;
   touchwin(stdscr);
   if (curses_driver_current_active_window())
      touchwin(curses_driver_current_active_window());
   display_screen(current_screen);
   /* The status row is global rather than part of display_screen(). */
   show_statarea();
}

static void native_menu_sync(void)
{
   size_t menus[THE_WORKBENCH_MENU_MAX], items[THE_WORKBENCH_ITEM_MAX];
   size_t nm = the_workbench_menu_count(), ni = the_workbench_item_count();
   size_t i, j, k, n;
   int view_added = 0, tools_added = 0;
   char selected[THE_WORKBENCH_ID_MAX + 1] = "";
   if (native_menu.count && native_menu.selected < native_menu.count)
      strcpy(selected, native_menu.entries[native_menu.selected].item);
   for (i = 0; i < nm; i++)
   {
      menus[i] = i;
      for (j = i; j && the_workbench_menu_at(menus[j - 1])->order >
                          the_workbench_menu_at(menus[j])->order; j--)
      { k = menus[j]; menus[j] = menus[j - 1]; menus[j - 1] = k; }
   }
   native_menu.count = 0;
   native_menu.selected = 0;
   native_menu.file = native_file;
   for (i = 0; i < nm; i++)
   {
      const TheWorkbenchMenu *menu = the_workbench_menu_at(menus[i]);
      if (!view_added && menu->order >= 30)
      { native_menu_local(NATIVE_MENU_TOOLS, "curses.tools", "View: Tools & Profiles", selected); view_added = 1; }
      if (!tools_added && menu->order >= 40)
      { native_menu_file_actions(selected); tools_added = 1; }
      n = 0;
      for (j = 0; j < ni; j++)
         if (!strcmp(the_workbench_item_at(j)->menu, menu->id))
         {
            items[n++] = j;
            for (k = n - 1; k && the_workbench_item_at(items[k - 1])->order >
                                  the_workbench_item_at(items[k])->order; k--)
            { size_t swap = items[k]; items[k] = items[k - 1]; items[k - 1] = swap; }
         }
      for (j = 0; j < n; j++)
      {
         const TheWorkbenchItem *item = the_workbench_item_at(items[j]);
         const TheWorkbenchAction *action = the_workbench_action_find(item->action);
         /* Adapt the exact stock generic Run only. Replaced/custom actions
          * retain their profile-owned identity and command semantics. */
         if (action && !strcmp(action->id, "profile.action.8")
         && action->kind == THE_WORKBENCH_ACTION_COMMAND && !strcmp(action->operand, "rx menu-run")) continue;
         int entry = native_menu.count++;
         memset(native_menu.entries + entry, 0, sizeof(native_menu.entries[entry]));
         strcpy(native_menu.entries[entry].item, item->id);
         strcpy(native_menu.entries[entry].action, item->action);
         native_menu.entries[entry].generation = action ? action->generation : 0;
         native_menu.entries[entry].separator = item->separator;
         if (action && action->kind == THE_WORKBENCH_ACTION_COMMAND)
         {
            if (!strcmp(action->id, "profile.action.9") && !strcmp(action->operand, "rx console on")) native_menu.entries[entry].kind = NATIVE_MENU_PROGRAM;
            if (!strcmp(action->id, "profile.action.10") && !strcmp(action->operand, "rx console off")) native_menu.entries[entry].kind = NATIVE_MENU_HIDE_PROGRAM;
            if (!strcmp(action->id, "profile.action.12") && !strcmp(action->operand, "rx clear")) native_menu.entries[entry].kind = NATIVE_MENU_CLEAR_PROGRAM;
         }
         snprintf(native_menu.entries[entry].label, sizeof(native_menu.entries[entry].label),
                  "%s: %s", menu->label, item->separator ? "--------" : item->label);
         if (!strcmp(selected, item->id)) native_menu.selected = entry;
      }
   }
   if (!view_added) native_menu_local(NATIVE_MENU_TOOLS, "curses.tools", "View: Tools & Profiles", selected);
   if (!tools_added) native_menu_file_actions(selected);
   native_menu.revision = the_workbench_revision();
}

static void native_path_entry(const char *directory, const char *base,
                                const char *prefix, const char *name, int is_directory)
{
   int n = native_menu.path_count;
   if (!strcmp(name, ".") || strncmp(name, prefix, strlen(prefix))) return;
   if (n == (int)(sizeof(native_menu.paths) / sizeof(native_menu.paths[0])))
   { native_menu.path_more = 1; return; }
   if (strlen(base) + strlen(name) + 2 > sizeof(native_menu.paths[n].path)) return;
   (void)directory;
   snprintf(native_menu.paths[n].path, sizeof(native_menu.paths[n].path), "%s%s%s",
            base, name, is_directory ? "/" : "");
   native_menu.paths[n].directory = is_directory;
   native_menu.path_count++;
}

static int native_path_compare(const void *a, const void *b)
{
   /* path is the first member of both bounded completion records. */
   return strcmp((const char *)a, (const char *)b);
}

static void native_path_refresh(void)
{
   char directory[THE_WORKBENCH_OPERAND_MAX + 1], base[THE_WORKBENCH_OPERAND_MAX + 1];
   const char *last = strrchr(native_menu.argument, '/'), *prefix;
   size_t length;
#ifdef _WIN32
   const char *backslash = strrchr(native_menu.argument, '\\');
   if (backslash && (!last || backslash > last)) last = backslash;
#endif
   length = last ? (size_t)(last - native_menu.argument) + 1 : 0;
   memcpy(base, native_menu.argument, length); base[length] = '\0';
   strcpy(directory, length ? base : ".");
   prefix = native_menu.argument + length;
   native_menu.path_base_length = length;
   native_menu.path_count = native_menu.path_more = native_menu.path_first = 0;
   native_menu.path_selected = -1;
#ifdef _WIN32
   {
      char pattern[THE_WORKBENCH_OPERAND_MAX + 4];
      WCHAR wide[THE_WORKBENCH_OPERAND_MAX + 4];
      WIN32_FIND_DATAW info;
      HANDLE search;
      snprintf(pattern, sizeof(pattern), "%s/*", directory);
      if (!MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, pattern, -1, wide,
                               sizeof(wide) / sizeof(wide[0]))) return;
      search = FindFirstFileW(wide, &info);
      if (search == INVALID_HANDLE_VALUE) return;
      do
      {
         char name[THE_WORKBENCH_OPERAND_MAX + 1];
         if (WideCharToMultiByte(CP_UTF8, 0, info.cFileName, -1, name, sizeof(name), NULL, NULL))
            native_path_entry(directory, base, prefix, name, !!(info.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY));
         if (native_menu.path_more) break;
      } while (FindNextFileW(search, &info));
      FindClose(search);
   }
#else
   {
      DIR *listing = opendir(directory);
      struct dirent *entry;
      if (!listing) return;
      while ((entry = readdir(listing)) != NULL)
      {
         struct stat info;
         char path[THE_WORKBENCH_OPERAND_MAX + 1];
         if (snprintf(path, sizeof(path), "%s/%s", directory, entry->d_name) >= (int)sizeof(path)) continue;
         native_path_entry(directory, base, prefix, entry->d_name,
                           !stat(path, &info) && S_ISDIR(info.st_mode));
         if (native_menu.path_more) break;
      }
      closedir(listing);
   }
#endif
   qsort(native_menu.paths, (size_t)native_menu.path_count, sizeof(native_menu.paths[0]), native_path_compare);
}

static int native_action_needs_argument(const TheWorkbenchAction *action)
{
   const TheFrontendActionDefinition *definition = action && action->kind == THE_WORKBENCH_ACTION_BUILTIN
      ? the_frontend_action_definition(action->builtin) : NULL;
   return definition && definition->requires_argument && !action->operand[0];
}

static const char *native_menu_reason(int entry)
{
   int kind = native_menu.entries[entry].kind;
   if (kind >= NATIVE_MENU_BUILD && kind <= NATIVE_MENU_APPLY)
   {
      const TheToolsFileActionSnapshot *action = native_menu.file.actions + kind - NATIVE_MENU_BUILD;
      return action->enabled ? "ok" : action->reason;
   }
   return kind ? "ok" : the_workbench_action_reason(the_workbench_action_find(native_menu.entries[entry].action));
}

static int native_menu_local_invoke(int kind, TheInputEvent *event)
{
   if (kind >= NATIVE_MENU_BUILD && kind <= NATIVE_MENU_APPLY)
      return native_file_action(&native_menu.file, (unsigned)(kind - NATIVE_MENU_BUILD), event);
   if (kind == NATIVE_MENU_TOOLS) native_show_tools(native_tools_section, 1);
   else if (kind == NATIVE_MENU_BUILD_RESULTS) native_show_tools(2, 0);
   else if (kind == NATIVE_MENU_PROGRAM) native_show_program(0);
   else if (kind == NATIVE_MENU_HIDE_PROGRAM)
   {
      the_tool_program_console_visibility(0, 0);
      if (!native_tools_visible && !native_selected_log())
      { native_aux_suppressed = 1; native_editor_focus(); }
   }
   else if (kind == NATIVE_MENU_CLEAR_PROGRAM) the_tool_clear_program();
   else if (kind == NATIVE_MENU_EDIT_PROFILE)
   { native_editor_focus(); return the_tools_prepare_input("profile", "", "", 0, NULL, event); }
   return 0;
}

static void native_menu_draw(void)
{
   int row, height, width, page;
   const char *hint = "Arrows/Tab: select  Enter: invoke  Esc: return";
   if (!native_menu.active) return;
   native_menu_sync();
   /* terminal_lines/cols describe editor space after console reservation.
    * A modal menu is a physical overlay, not an auxiliary editor pane. */
   height = min(getmaxy(stdscr) - 2, native_menu.count + 4);
   width = min(getmaxx(stdscr), 78);
   if (height < 5 || width < 12) { native_menu_close(1); return; }
   if (!native_menu.window || height != native_menu.rows || width != native_menu.cols)
   {
      if (native_menu.window) delwin(native_menu.window);
      native_menu.rows = height; native_menu.cols = width;
      native_menu.top = 1; native_menu.left = 0;
      native_menu.window = newwin(height, width, native_menu.top, native_menu.left);
      keypad(native_menu.window, TRUE);
   }
   werase(native_menu.window);
   wattrset(native_menu.window, console_theme_attr(ATTR_POPUP, THE_STYLE_NORMAL));
   box(native_menu.window, 0, 0);
   {
      const char *title = native_menu.prompting ? (native_menu.path_more
         ? " Choose path (128+ matches; type to filter) "
         : " Choose path (type, arrows, Tab, Enter; Esc cancels) ") : " Menu ";
      curses_write_text(native_menu.window, 0, 2, title, strlen(title), width - 4);
   }
   page = height - 3;
   if (native_menu.selected < native_menu.first) native_menu.first = native_menu.selected;
   if (native_menu.selected >= native_menu.first + page)
      native_menu.first = native_menu.selected - page + 1;
   if (native_menu.prompting)
   {
      if (native_menu.path_selected >= native_menu.path_first + page)
         native_menu.path_first = native_menu.path_selected - page + 1;
      if (native_menu.path_selected >= 0 && native_menu.path_selected < native_menu.path_first)
         native_menu.path_first = native_menu.path_selected;
      for (row = 0; row < page && native_menu.path_first + row < native_menu.path_count; row++)
      {
         int entry = native_menu.path_first + row;
         wattrset(native_menu.window, console_theme_attr(entry == native_menu.path_selected
            ? ATTR_POP_CURLINE : ATTR_POPUP, THE_STYLE_NORMAL));
         const char *name = native_menu.paths[entry].path + native_menu.path_base_length;
         curses_write_text(native_menu.window, row + 1, 2, name, strlen(name), width - 4);
      }
   }
   else for (row = 0; row < page && native_menu.first + row < native_menu.count; row++)
   {
      int entry = native_menu.first + row;
      const TheWorkbenchAction *action = the_workbench_action_find(native_menu.entries[entry].action);
      const char *reason = native_menu_reason(entry);
      int promptable = native_action_needs_argument(action);
      wattrset(native_menu.window, console_theme_attr(entry == native_menu.selected
         ? ATTR_POP_CURLINE : ATTR_POPUP, (reason && strcmp(reason, "ok") && !promptable) ? THE_STYLE_DIM : THE_STYLE_NORMAL));
      mvwhline(native_menu.window, row + 1, 1, ' ', width - 2);
      curses_write_text(native_menu.window, row + 1, 2, native_menu.entries[entry].label,
                        strlen(native_menu.entries[entry].label), width - 4);
   }
   wattrset(native_menu.window, console_theme_attr(ATTR_POPUP, THE_STYLE_NORMAL));
   if (native_menu.prompting)
   {
      char path[THE_WORKBENCH_OPERAND_MAX + 12];
      const char *tail = native_menu.argument;
      size_t bytes = strlen(tail);
      int clipped = 0;
      /* Keep the part being edited visible even for a deep directory. */
      while (bytes > (size_t)max(1, width - 12))
      {
         TextCluster cluster = textpos_cluster_at_boundary((const CHARTYPE *)tail, bytes, textpos_begin());
         size_t skip = cluster.byte_length ? cluster.byte_length : 1;
         tail += skip; bytes -= skip; clipped = 1;
      }
      snprintf(path, sizeof(path), "Path: %s%s", clipped ? "..." : "", tail);
      curses_write_text(native_menu.window, height - 2, 1, path, strlen(path), width - 2);
   }
   else
   {
      if (native_menu.count)
      {
         const char *reason = native_menu_reason(native_menu.selected);
         if (strcmp(reason, "ok") && strcmp(reason, "argument-required")) hint = reason;
      }
      curses_write_text(native_menu.window, height - 2, 1, hint, strlen(hint), width - 2);
   }
   if (native_menu.prompting)
      wmove(native_menu.window, height - 2, min(width - 2, 7 + (int)strlen(native_menu.argument)));
   else wmove(native_menu.window, min(height - 2, native_menu.selected - native_menu.first + 1), 1);
   wnoutrefresh(native_menu.window);
}

static int native_menu_open(void)
{
   if (!workbench_menu_origin.active) workbench_remember_menu_origin();
   native_menu.active = 1; native_menu.prompting = 0;
   native_menu.selected = native_menu.count = native_menu.first = 0;
   native_menu.argument[0] = '\0'; native_menu.revision = 0;
   native_menu_sync();
   curses_driver_update();
   return 0;
}

static int native_menu_key(int key, const TheInputEvent *input, TheInputEvent *event)
{
   int step = 0, tries;
   if (key == THE_KEY_RESIZE || key == 27)
   { native_menu_close(1); curses_driver_update(); return key == THE_KEY_RESIZE; }
   if (native_menu.prompting)
   {
      size_t length = strlen(native_menu.argument);
      if (key == THE_KEY_UP || key == THE_KEY_DOWN)
      {
         if (native_menu.path_count)
            native_menu.path_selected = (native_menu.path_selected + native_menu.path_count +
               (key == THE_KEY_UP ? -1 : 1)) % native_menu.path_count;
      }
      else if (key == 9 && native_menu.path_count)
      {
         strcpy(native_menu.argument, native_menu.paths[max(0, native_menu.path_selected)].path);
         native_path_refresh();
      }
      else if (key == 21)
      { native_menu.argument[0] = '\0'; native_path_refresh(); }
      else if (key == 8 || key == 127 || key == THE_KEY_BACKSPACE)
      {
         if (length) { do { length--; } while (length && ((unsigned char)native_menu.argument[length] & 0xc0) == 0x80); native_menu.argument[length] = '\0'; }
         native_path_refresh();
      }
      else if (key == 10 || key == 13 || key == THE_KEY_ENTER)
      {
         if (native_menu.path_selected >= 0)
         {
            int directory = native_menu.paths[native_menu.path_selected].directory;
            strcpy(native_menu.argument, native_menu.paths[native_menu.path_selected].path);
            native_path_refresh();
            if (directory) { curses_driver_update(); return 0; }
         }
         if (*native_menu.argument) goto invoke;
      }
      else if (input->kind == THE_INPUT_TEXT)
      {
         CHARTYPE bytes[4];
         size_t n = text_utf8_encode(input->codepoint, bytes);
         if (input->codepoint >= 32 && n && length + n <= THE_WORKBENCH_OPERAND_MAX)
         { memcpy(native_menu.argument + length, bytes, n); native_menu.argument[length + n] = '\0'; native_path_refresh(); }
      }
   }
   else if (key == THE_KEY_UP || key == THE_KEY_BTAB || key == THE_KEY_LEFT) step = -1;
   else if (key == THE_KEY_DOWN || key == 9 || key == THE_KEY_RIGHT) step = 1;
   else if (key == THE_KEY_HOME) native_menu.selected = 0;
   else if (key == THE_KEY_END) native_menu.selected = max(0, native_menu.count - 1);
   else if (key == THE_KEY_MOUSE)
   {
      int button, action, modifier, row, col;
      if (curses_driver_read_pending_mouse_button(&button, &action, &modifier))
      {
         curses_driver_current_mouse_screen_position(&row, &col);
         row -= native_menu.top + 1;
         if (button == 1 && !modifier && row >= 0 && row < native_menu.rows - 3 &&
             col >= native_menu.left && col < native_menu.left + native_menu.cols &&
             row + native_menu.first < native_menu.count)
         {
            native_menu.selected = row + native_menu.first;
            if (action == CURSES_DRIVER_MOUSE_BUTTON_RELEASED || action == CURSES_DRIVER_MOUSE_BUTTON_CLICKED)
               goto activate;
         }
      }
   }
   else if (key == 10 || key == 13 || key == THE_KEY_ENTER || key == ' ')
   {
activate:
      if (native_menu.count && !native_menu.entries[native_menu.selected].separator)
      {
         int kind = native_menu.entries[native_menu.selected].kind;
         const TheWorkbenchAction *action = the_workbench_action_find(native_menu.entries[native_menu.selected].action);
         const char *reason = native_menu_reason(native_menu.selected);
         if (kind)
         {
            if (!reason || !strcmp(reason, "ok"))
            {
               int ready = native_menu_local_invoke(kind, event);
               native_menu_close(0); curses_driver_update(); return ready;
            }
            curses_driver_update(); return 0;
         }
         if (native_action_needs_argument(action) && (!reason || !strcmp(reason, "ok") || !strcmp(reason, "argument-required")))
         {
            native_menu.prompting = 1;
            snprintf(native_menu.argument, sizeof(native_menu.argument), "%s", CURRENT_FILE ? (char *)CURRENT_FILE->fpath : "");
            native_path_refresh();
         }
         else if (!reason || !strcmp(reason, "ok"))
         {
invoke:
            if (the_input_event_from_workbench_invoke(native_menu.entries[native_menu.selected].action,
               native_menu.entries[native_menu.selected].generation,
               native_menu.prompting ? native_menu.argument : NULL, event))
            { native_menu_close(0); curses_driver_update(); return 1; }
         }
      }
   }
   if (step && native_menu.count)
      for (tries = 0; tries < native_menu.count; tries++)
      {
         native_menu.selected = (native_menu.selected + native_menu.count + step) % native_menu.count;
         if (!native_menu.entries[native_menu.selected].separator) break;
      }
   curses_driver_update();
   return 0;
}

void curses_driver_update(void)
{
   static int cursor_focus_state = -1;
   int focus_state;
   native_tools_sync();
   native_logs_sync();
   focus_state = log_ui.focused || native_tools_has_focus() ? 3 : native_program_focused() ? 2
      : workbench_control != THE_WORKBENCH_EDITOR ? 1 : 0;

   console_draw();
   if (workbench_control != THE_WORKBENCH_EDITOR && FILETABSx && filetabs
       && !in_macro && !the_panel_active)
   {
      WINDOW *chrome = curses_driver_window_from_driver(filetabs);
      int col = workbench_control == THE_WORKBENCH_CONSOLE
         ? THE_NATIVE_MENU_WIDTH + 1
         : workbench_control == THE_WORKBENCH_RUN
            ? THE_NATIVE_MENU_WIDTH + THE_NATIVE_CONSOLE_WIDTH + 1 : 1;
      wmove(chrome, 0, col);
      leaveok(chrome, FALSE);
      wnoutrefresh(chrome);
   }
   else if (!log_ui.focused && !native_tools_has_focus() && !native_program_focused() && curses_driver_current_active_window())
      wnoutrefresh(curses_driver_current_active_window());
   if (!in_macro && !the_panel_active && focus_state != cursor_focus_state)
   {
      cursor_focus_state = focus_state;
      curses_driver_apply_cursor_visibility(TRUE);
   }
   {
      size_t i;
      for (i = 0; i < the_workbench_panel_count(); i++)
      {
         const TheWorkbenchPanel *panel = the_workbench_panel_at(i);
         const TheWorkbenchPanelState *state = the_workbench_panel_state(panel->id);
         TheWorkbenchPlacement place = !state || !state->visible || !console_rows ? THE_WORKBENCH_PLACE_NONE
            : native_selected_log() == panel ? native_log_placement(panel) : THE_WORKBENCH_PLACE_TAB;
         the_workbench_panel_effective(panel->id, panel->generation, place,
            !state || !state->visible ? "hidden" : !console_rows || place == THE_WORKBENCH_PLACE_NONE ? "insufficient-space" : place == THE_WORKBENCH_PLACE_TAB ? "auxiliary-tab" : "");
      }
   }
   native_menu_draw();
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
   console_timeout=milliseconds;
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

static int curses_driver_read_raw_window_key(WINDOW *win);

static int curses_driver_utf8_lead_len(int key)
{
   unsigned char ch;

   if (key < 0 || key > 255)
      return 0;
   ch = (unsigned char)key;
   if (ch >= 0xC2u && ch <= 0xDFu)
      return 2;
   if (ch >= 0xE0u && ch <= 0xEFu)
      return 3;
   if (ch >= 0xF0u && ch <= 0xF4u)
      return 4;
   return 0;
}

static int curses_driver_utf8_continuation(int key)
{
   return key >= 0x80 && key <= 0xBF;
}

static int curses_driver_decode_utf8_bytes(const unsigned char *bytes,
                                           int len, uint32_t *codepoint)
{
   uint32_t cp;
   int i;

   if (bytes == NULL || codepoint == NULL || len < 2 || len > 4)
      return 0;
   if (len == 2)
      cp = (uint32_t)(bytes[0] & 0x1Fu);
   else if (len == 3)
      cp = (uint32_t)(bytes[0] & 0x0Fu);
   else
      cp = (uint32_t)(bytes[0] & 0x07u);
   for (i = 1; i < len; i++)
   {
      if ((bytes[i] & 0xC0u) != 0x80u)
         return 0;
      cp = (cp << 6) | (uint32_t)(bytes[i] & 0x3Fu);
   }
   if ((len == 2 && cp < 0x80u)
   ||  (len == 3 && cp < 0x800u)
   ||  (len == 4 && cp < 0x10000u)
   ||  (cp >= 0xD800u && cp <= 0xDFFFu)
   ||  cp > 0x10FFFFu)
      return 0;
   *codepoint = cp;
   return 1;
}

static int curses_driver_read_utf8_text_event(WINDOW *input_window, int first_key,
                                              TheInputEvent *event)
{
   unsigned char bytes[4];
   uint32_t codepoint;
   int len;
   int i;

   len = curses_driver_utf8_lead_len(first_key);
   if (len == 0)
      return 0;
   bytes[0] = (unsigned char)first_key;
   for (i = 1; i < len; i++)
   {
      int next_key = curses_driver_read_raw_window_key(input_window);

      if (!curses_driver_utf8_continuation(next_key))
         return 0;
      bytes[i] = (unsigned char)next_key;
   }
   if (!curses_driver_decode_utf8_bytes(bytes, len, &codepoint))
      return 0;
   return the_input_event_from_text(codepoint, event);
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

int curses_driver_peek_raw_driver_window_key(TheDriverWindow *win)
{
   WINDOW *native=curses_driver_window_from_driver(win);
   int key;
   if (native == NULL) return ERR;
   nodelay(native,TRUE);
   key=wgetch(native);
   nodelay(native,FALSE);
   if (key != ERR) ungetch(key);
   return key;
}

/* Inspect each mouse packet once. Editor packets remain available to the
 * editor's normal mouse bindings through read_pending_mouse_button(). */
static int console_mouse_event(TheInputEvent *event)
{
   const TheToolConsole *c = the_tool_program_console();
   int button, action, modifier, row, col, i, start = 1;

   if (!curses_driver_read_pending_mouse_button(&button, &action, &modifier))
      return 0;
   curses_driver_current_mouse_screen_position(&row, &col);
   if (native_tools_visible && row >= console_top && row < console_top + console_rows &&
       col >= console_left && col < console_left + console_cols)
   {
      if (button == 1 && !modifier && (action == CURSES_DRIVER_MOUSE_BUTTON_PRESSED || action == CURSES_DRIVER_MOUSE_BUTTON_CLICKED))
      {
         native_tools_focused = 1; workbench_control = THE_WORKBENCH_EDITOR;
         if (row == console_top + 1)
            for (i = 0; i < 6; i++)
            {
               if (col - console_left >= start && col - console_left < start + (int)strlen(native_tools_controls[i]))
               {
                  int ready;
                  native_tools_control = i; ready = native_tools_control_event(i, event);
                  curses_driver_update(); return ready;
               }
               start += (int)strlen(native_tools_controls[i]) + 1;
            }
      }
      curses_driver_update();
      return 0;
   }
   if (native_selected_log() && row >= console_top && row < console_top + console_rows &&
       col >= console_left && col < console_left + console_cols)
   {
      if (button == 1 && !modifier &&
          (action == CURSES_DRIVER_MOUSE_BUTTON_PRESSED || action == CURSES_DRIVER_MOUSE_BUTTON_CLICKED))
      {
         const TheWorkbenchPanel *panel = native_selected_log();
         const TheWorkbenchFileFieldsState *state = panel->kind == THE_WORKBENCH_PANEL_FIELDS && panel->document_id ? the_workbench_filefields_state(panel->id) : NULL;
         int index = native_log_view(panel);
         if (state && index >= 0)
         {
            NativeFileFieldsView *view = &log_ui.views[index].filefields;
            int local_row = row - console_top, local_col = col - console_left;
            if (local_row == 1)
               for (i = 0; i < 5; i++)
                  if (local_col >= native_filefields_control_cells[i] && local_col < native_filefields_control_cells[i] + (int)strlen(native_filefields_controls[i]))
                  {
                     view->active = (int)state->spans + i;
                     if (i >= 3) return native_log_event(panel, i == 3 ? THE_WORKBENCH_PANEL_HIDE : THE_WORKBENCH_PANEL_RETURN, event);
                     return native_filefields_event(panel, view, i == 0 ? THE_WORKBENCH_FILEFIELDS_APPLY : i == 1 ? THE_WORKBENCH_FILEFIELDS_CANCEL : THE_WORKBENCH_FILEFIELDS_RELOAD, 0, 0, event);
                  }
            if (local_row >= 2 && local_row < console_rows - 1)
               for (i = 0; i < (int)state->spans; i++)
               {
                  const TheWorkbenchFileFieldSpan *span = the_workbench_filefields_span(panel->id, i);
                  int cell = local_col - 1 + view->left;
                  if (span && span->editable && (int)span->line - 1 == local_row - 2 + view->top && cell >= span->cell && cell < span->cell + span->maxcells)
                  {
                     view->active = i; view->cell = textpos_from_cell((const CHARTYPE *)span->value, strlen(span->value), cell - span->cell, TEXT_SNAP_BACKWARD).cell_column;
                     view->follow = 1; break;
                  }
               }
         }
         return native_log_event(native_selected_log(), THE_WORKBENCH_PANEL_FOCUS, event);
      }
      return 0;
   }
   if (FILETABSx && row == 0 && col >= 0 && col < THE_NATIVE_CONTROLS_WIDTH)
   {
      /* Wait for the completed click. Opening on press would let the
       * release outside the popup dismiss it immediately. */
      if (button == 1 && !modifier &&
          (action == CURSES_DRIVER_MOUSE_BUTTON_RELEASED ||
           action == CURSES_DRIVER_MOUSE_BUTTON_CLICKED ||
           action == CURSES_DRIVER_MOUSE_BUTTON_DOUBLE_CLICKED))
      {
         if (col < THE_NATIVE_MENU_WIDTH)
            workbench_remember_menu_origin();
         workbench_set_focus(THE_WORKBENCH_EDITOR);
         if (col < THE_NATIVE_MENU_WIDTH) return native_menu_open();
         if (col < THE_NATIVE_MENU_WIDTH + THE_NATIVE_CONSOLE_WIDTH) native_show_program(1);
         else native_show_tools(native_tools_section, 1);
         curses_driver_update(); return 0;
      }
      return 0;
   }
   if ((console_left && col == console_left - 1)
   ||  (!console_left && row == console_top - 1)) return 0;
   if (button == 1 && workbench_control != THE_WORKBENCH_EDITOR)
      workbench_set_focus(THE_WORKBENCH_EDITOR);
   if (row < console_top || row >= console_top + console_rows ||
       col < console_left || col >= console_left + console_cols)
   {
      console_cached_mouse = 1;
      console_mouse_button = button;
      console_mouse_action = action;
      console_mouse_modifier = modifier;
      if (button == 1 && c->focused)
      {
         console_toolbar_focus = 0;
         the_tool_program_console_visibility(c->visible, 0);
      }
      if (button == 1) native_tools_focused = 0;
      return the_input_event_from_legacy_key(THE_KEY_MOUSE, event);
   }
   if (button != 1 || modifier ||
       (action != CURSES_DRIVER_MOUSE_BUTTON_PRESSED &&
        action != CURSES_DRIVER_MOUSE_BUTTON_CLICKED &&
        action != CURSES_DRIVER_MOUSE_BUTTON_DOUBLE_CLICKED)) return 0;
   row -= console_top;
   col -= console_left;
   if (row == 1)
   {
      for (i = 0; i < console_control_count(); i++)
      {
         if (col >= start && col < start + (int)strlen(console_control_label(i)))
         {
            console_control = i;
            return console_control_event(i, event);
         }
         start += (int)strlen(console_control_label(i)) + 1;
      }
   }
   console_toolbar_focus = 0;
   if (c->focused)
   {
      curses_driver_update();
      return 0;
   }
   native_show_program(0); curses_driver_update(); return 0;
}

static int native_tools_key(int key, TheInputEvent *event)
{
   if (key == 27 || key == THE_KEY_F(6))
   { native_editor_focus(); return 1; }
   if (key == 9 || key == THE_KEY_RIGHT || key == THE_KEY_LEFT || key == THE_KEY_BTAB)
   {
      if (key == THE_KEY_BTAB && !native_tools_control) workbench_set_focus(THE_WORKBENCH_TOOLS);
      else native_tools_control = (native_tools_control + 6 + ((key == THE_KEY_BTAB || key == THE_KEY_LEFT) ? -1 : 1)) % 6;
   }
   else if (key == 10 || key == 13 || key == THE_KEY_ENTER || key == ' ')
   {
      int ready = native_tools_control_event(native_tools_control, event);
      curses_driver_update(); return ready;
   }
   else if (native_tools_section == 2)
   {
      int maximum = max(0, native_build_reader.lines - (console_rows - 4));
      if (key == THE_KEY_HOME) native_build_reader.offset = maximum;
      else if (key == THE_KEY_END) native_build_reader.offset = 0;
      else if (key == THE_KEY_PPAGE || key == THE_KEY_UP) native_build_reader.offset = min(maximum,
         native_build_reader.offset + (key == THE_KEY_UP ? 1 : native_build_reader.page));
      else if (key == THE_KEY_NPAGE || key == THE_KEY_DOWN) native_build_reader.offset = max(0,
         native_build_reader.offset - (key == THE_KEY_DOWN ? 1 : native_build_reader.page));
   }
   else if (native_tools_section == 1)
   {
      if (key == THE_KEY_HOME) native_tools_top = 0;
      else if (key == THE_KEY_END) native_tools_top = max(0, (int)the_tool_count() - (console_rows - 3));
      else if (key == THE_KEY_PPAGE || key == THE_KEY_UP) native_tools_top = max(0,
         native_tools_top - (key == THE_KEY_UP ? 1 : console_rows - 3));
      else if (key == THE_KEY_NPAGE || key == THE_KEY_DOWN) native_tools_top = min(max(0, (int)the_tool_count() - 1),
         native_tools_top + (key == THE_KEY_DOWN ? 1 : console_rows - 3));
   }
   curses_driver_update(); return 0;
}

static int native_log_key(int key, TheInputEvent *event)
{
   const TheWorkbenchPanel *panel = native_selected_log(), *next;
   int view = panel ? native_log_view(panel) : -1;
   size_t i, count = native_log_count();
   if (!panel) return 0;
   if (panel->kind == THE_WORKBENCH_PANEL_FIELDS)
   { if (key != THE_KEY_F(12)) return panel->document_id ? native_filefields_key(panel, key, event) : native_form_key(panel, key, event); key = 9; }
   if (key == 27) return native_log_event(panel, THE_WORKBENCH_PANEL_RETURN, event);
   if (key == 'h' || key == 'H' || key == 23) return native_log_event(panel, THE_WORKBENCH_PANEL_HIDE, event);
   if (key == 9 || key == THE_KEY_BTAB)
   {
      for (i = 0; i < count && native_log_at(i) != panel; i++);
      if ((key == 9 && i + 1 == count) || (key == THE_KEY_BTAB && !i))
      {
         if (the_tool_program_console()->visible)
         {
            log_ui.origin_program = 1; log_ui.origin_toolbar = 0; log_ui.origin_chrome = THE_WORKBENCH_EDITOR;
            return native_log_event(panel, THE_WORKBENCH_PANEL_RETURN, event);
         }
      }
      next = native_log_at(key == 9 ? (i + 1) % count : (i + count - 1) % count);
      return native_log_event(next, THE_WORKBENCH_PANEL_FOCUS, event);
   }
   if (view >= 0)
   {
      int page = max(1, console_rows - 4), maximum = max(0, log_ui.views[view].lines - (console_rows - 3));
      if (panel->kind == THE_WORKBENCH_PANEL_DOCUMENT)
      {
         NativeDocumentView *document = &log_ui.views[view].document;
         if (key == THE_KEY_UP || key == THE_KEY_DOWN || key == THE_KEY_PPAGE || key == THE_KEY_NPAGE || key == THE_KEY_HOME)
            document->tail = 0;
         if (key == THE_KEY_HOME) { document->request = -2; log_ui.views[view].scroll = 0; }
         else if (key == THE_KEY_END) { document->request = 2; log_ui.views[view].scroll = 0; }
         else if (key == THE_KEY_DOWN || key == THE_KEY_NPAGE)
         {
            if (log_ui.views[view].scroll < maximum)
               log_ui.views[view].scroll = min(maximum, log_ui.views[view].scroll + (key == THE_KEY_DOWN ? 1 : page));
            else if (document->more) { document->request = 1; log_ui.views[view].scroll = 0; }
         }
         else if (key == THE_KEY_UP || key == THE_KEY_PPAGE)
         {
            if (log_ui.views[view].scroll)
               log_ui.views[view].scroll = max(0, log_ui.views[view].scroll - (key == THE_KEY_UP ? 1 : page));
            else { document->request = -1; log_ui.views[view].scroll = 0; }
         }
         curses_driver_update(); return 0;
      }
      if (key == THE_KEY_HOME) log_ui.views[view].scroll = maximum;
      else if (key == THE_KEY_END) log_ui.views[view].scroll = 0;
      else if (key == THE_KEY_UP || key == THE_KEY_PPAGE)
         log_ui.views[view].scroll = min(maximum, log_ui.views[view].scroll + (key == THE_KEY_UP ? 1 : page));
      else if (key == THE_KEY_DOWN || key == THE_KEY_NPAGE)
         log_ui.views[view].scroll = max(0, log_ui.views[view].scroll - (key == THE_KEY_DOWN ? 1 : page));
   }
   curses_driver_update(); return 0;
}

int curses_driver_read_input_event(TheInputEvent *event)
{
   int key, ok;
   char command[80];
   const TheToolConsole *c;
   WINDOW *input_window;

   if (!event) return 0;
   /* A menu selection has returned to ordinary input without Escape. */
   if (!native_menu.active) workbench_menu_origin.active = 0;
   /* The prior event has now been dispatched. A custom binding may have
    * ignored its mouse packet; never let it replace a later physical click. */
   console_cached_mouse = 0;
   the_tools_native_initialise();
   the_tools_native_poll();
   native_tools_sync();
   native_logs_sync();
   c = the_tool_program_console();
   if (log_ui.focused && c->focused && native_selected_log())
   {
      log_ui.origin_program = 1; log_ui.origin_toolbar = 0; log_ui.origin_chrome = THE_WORKBENCH_EDITOR;
      return native_log_event(native_selected_log(), THE_WORKBENCH_PANEL_RETURN, event);
   }
   if (the_tools_take_menu_run(event)) return 1;
   if (console_layout_changed())
   {
      if (native_menu.active) native_menu_close(1);
      return the_input_event_from_resize(0, 0, event);
   }
   if (!c->focused) console_toolbar_focus = 0;
   if ((native_program_focused() || native_tools_has_focus()) && !console_rows && !in_macro && !the_panel_active)
   { native_editor_focus(); curses_driver_update(); return 0; }
   /* wgetch() refreshes its window and caret. The visible menu owns input;
    * polling the underlying Program/editor window would move the terminal
    * caret away from the menu on every active-job timeout. */
   input_window = native_menu.active && native_menu.window && !in_macro && !the_panel_active
      ? native_menu.window
      : (log_ui.focused || native_tools_has_focus() || native_program_focused()) && console_window && !in_macro && !the_panel_active ? console_window
      : workbench_control != THE_WORKBENCH_EDITOR && FILETABSx && filetabs
        && !in_macro && !the_panel_active
         ? curses_driver_window_from_driver(filetabs) : curses_driver_current_active_window();
   curses_driver_set_window_timeout(input_window, console_timeout);
   if (the_tool_console()->running)
      curses_driver_set_window_timeout(input_window, 50);
   key = curses_driver_read_window_key(input_window);
   if (the_tool_console()->running)
      curses_driver_set_window_timeout(input_window, console_timeout);
   if (key == ERR)
   {
      the_tools_native_poll();
      curses_driver_update();
      return 0;
   }
   *event = the_input_event_none();
   ok = curses_driver_read_utf8_text_event(input_window, key, event);
   if (!ok) ok = the_input_event_from_legacy_key(key, event);
   if (!in_macro && !the_panel_active)
   {
      if (native_menu.active)
      {
         TheInputEvent input = *event;
         return native_menu_key(key, &input, event);
      }
      if (key == THE_KEY_F(10) && shared_menu_key_binding()) return native_menu_open();
      if (native_tools_has_focus() && key != THE_KEY_RESIZE && key != THE_KEY_MOUSE) return native_tools_key(key, event);
      if (log_ui.focused && key != THE_KEY_RESIZE && key != THE_KEY_MOUSE) return native_log_key(key, event);
      if (key == THE_KEY_MOUSE) return console_mouse_event(event);
      if (workbench_control != THE_WORKBENCH_EDITOR)
      {
         if (key == THE_KEY_F(10) || key == THE_KEY_F(6) ||
             key == THE_KEY_RESIZE)
         {
            if (key == THE_KEY_F(10)) workbench_remember_menu_origin();
            workbench_set_focus(THE_WORKBENCH_EDITOR);
            return ok;
         }
         if (key == 27)
         {
            workbench_set_focus(THE_WORKBENCH_EDITOR);
            return ok;
         }
         else if (key == 9 || key == THE_KEY_BTAB || key == THE_KEY_LEFT || key == THE_KEY_RIGHT)
            workbench_set_focus(workbench_traverse(workbench_control,
               key == THE_KEY_BTAB || key == THE_KEY_LEFT));
         else if (key == 10 || key == 13 || key == THE_KEY_ENTER || key == ' ')
         {
            TheWorkbenchControl control = workbench_control;
            if (control == THE_WORKBENCH_MENU)
               workbench_remember_menu_origin();
            workbench_set_focus(THE_WORKBENCH_EDITOR);
            if (control == THE_WORKBENCH_MENU) return native_menu_open();
            if (control == THE_WORKBENCH_TOOLS) native_show_tools(native_tools_section, 1);
            else native_show_program(1);
            curses_driver_update(); return 0;
         }
         curses_driver_update();
         return 0;
      }
      if (native_program_focused())
      {
         /* Route editor shortcuts through their active profile after leaving
          * Program input. The unsent input stays in the console model. */
         if (key == THE_KEY_F(10) || key == THE_KEY_F(6) || key == 27)
         {
            if (key == THE_KEY_F(10)) workbench_remember_menu_origin();
            console_toolbar_focus = 0;
            the_tool_program_console_visibility(c->visible, 0);
            return ok;
         }
         if (key == THE_KEY_RESIZE) return ok;
         if (key == THE_KEY_PPAGE || key == THE_KEY_NPAGE ||
             (!console_toolbar_focus &&
              (key == THE_KEY_HOME || key == THE_KEY_END)))
         {
            int maximum = max(0, console_scroll_lines - console_scroll_page - 1);
            if (key == THE_KEY_HOME) console_scroll_offset = maximum;
            else if (key == THE_KEY_END) console_scroll_offset = 0;
            else if (key == THE_KEY_PPAGE)
               console_scroll_offset = min(maximum,
                  console_scroll_offset + console_scroll_page);
            else console_scroll_offset = max(0,
                  console_scroll_offset - console_scroll_page);
            curses_driver_update();
            return 0;
         }
         if (key == 9 || key == THE_KEY_BTAB)
         {
            if (key == THE_KEY_BTAB && console_toolbar_focus && FILETABSx)
               workbench_set_focus(THE_WORKBENCH_CONSOLE);
            else console_toolbar_focus = !console_toolbar_focus;
            curses_driver_update();
            return 0;
         }
         if (key == 27)
         {
            console_toolbar_focus = 0;
            strcpy(command, "rx key escape");
         }
         else if (key == 3) strcpy(command, "rx key stop");
         else if (console_toolbar_focus)
         {
            if (key == THE_KEY_LEFT) console_control = (console_control + console_control_count() - 1) % console_control_count();
            else if (key == THE_KEY_RIGHT) console_control = (console_control + 1) % console_control_count();
            else if (key == THE_KEY_HOME) console_control = 0;
            else if (key == THE_KEY_END) console_control = console_control_count() - 1;
            else if (key == 10 || key == 13 || key == THE_KEY_ENTER || key == ' ')
               return console_control_event(console_control, event);
            curses_driver_update();
            return 0;
         }
         else if (key == 10 || key == 13 || key == THE_KEY_ENTER) strcpy(command, "rx key enter");
         else if (key == 8 || key == 127 || key == THE_KEY_BACKSPACE) strcpy(command, "rx key backspace");
         else if (key == 4) strcpy(command, "rx key eof");
         else if (event->kind == THE_INPUT_TEXT)
            snprintf(command, sizeof(command), "rx type %u", event->codepoint);
         else return 0;
         return the_input_event_from_command(command, event);
      }
   }
   return ok;
}

int curses_driver_read_terminal_legacy_key(void)
{
   int key = curses_driver_read_window_key(stdscr);

   if (workbench_menu_origin.active)
   {
      if (key == 27) workbench_restore_cancelled_menu();
      else if (key == 10 || key == 13 || key == THE_KEY_ENTER ||
               key == THE_KEY_MOUSE)
         workbench_menu_origin.active = 0;
   }
   return key;
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
                       (Mouse_status.changes & PDC_MOUSE_MOVED));
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
      if (Mouse_status.changes & PDC_MOUSE_MOVED)
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
   if (console_cached_mouse)
   {
      console_cached_mouse = 0;
      if (button) *button = console_mouse_button;
      if (action) *action = console_mouse_action;
      if (modifier) *modifier = console_mouse_modifier;
      return 1;
   }
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
   {
      TheRenderAttr attr = curses_driver_cluster_attr(cluster);

      pair = curses_driver_pair_for_attr(attr);
      attrs = curses_driver_style_to_attr(the_render_attr_style(attr));
   }
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
                                        row, col, wch,
                                        curses_driver_cluster_attr(cluster),
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

static void curses_driver_ops_overlay_cell_attrs_at(
   TheDriverWindow *win, int row, int col, int width, TheDriverAttr colour)
{
#ifdef USE_UTF8
   curses_driver_overlay_cell_attrs_at(curses_driver_window_from_driver(win),
                                       row, col, width, colour);
#else
   INTENTIONALLY_UNUSED_VARIABLE(win);
   INTENTIONALLY_UNUSED_VARIABLE(row);
   INTENTIONALLY_UNUSED_VARIABLE(col);
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
   .overlay_cell_attrs_at = curses_driver_ops_overlay_cell_attrs_at,
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
   .filearea_cursor_transition = curses_driver_filearea_cursor_transition,
   .workbench_focus = curses_driver_workbench_focus
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
   console_delete_windows();
   console_rows=console_height();
   console_cols=console_width();
   console_left=console_rows && console_is_right() ? COLS-console_cols : 0;
   console_top=console_rows && !console_is_right() ? LINES-console_rows : 0;
   terminal_lines = console_rows && !console_is_right() ? console_top-1 : LINES;
   terminal_cols = console_left ? console_left-1 : COLS;
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
#if defined(UNIX) || defined(WIN32)
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
#if defined(UNIX) || defined(WIN32)
# if defined(USE_EXTCURSES)
   cresetty(FALSE);
# else
#  if defined(WIN32)
   /* Quiet OSQ does not suspend curses. Preserve its current dimensions,
    * mouse and caret settings rather than restoring the startup snapshot. */
   if (!isendwin())
      def_prog_mode();
#  endif
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
   /* Core may detect SIGWINCH before it asks the driver for another key. */
   if (native_menu.active) native_menu_close(1);
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
   if(console_layout_changed())return 1;
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
   init_color(curses_driver_basic_colour(color), (short)red, (short)green, (short)blue);
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

int the_driver_module_abi_version(void)
{
   return THE_DRIVER_MODULE_ABI_VERSION;
}

const TheDriverOps *the_driver_module_ops(void)
{
   return &the_curses_driver_ops;
}

const TheDriverModuleLifecycle *the_driver_module_lifecycle(void)
{
   return &the_curses_driver_lifecycle;
}
