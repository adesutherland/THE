#include "cursesdriver.h"

#include "the.h"
#include "proto.h"

#ifdef USE_UTF8
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

short curses_driver_refresh_cursor(CHARTYPE scrno)
{
   INTENTIONALLY_UNUSED_VARIABLE(scrno);
   show_statarea();
   doupdate();
   draw_cursor(TRUE);
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

CursesDriverCursorTarget curses_driver_filearea_target(
   LogicalCursor cursor, const CHARTYPE *line, size_t len,
   int viewport_col, int window_cols)
{
   CursesDriverCursorTarget target;

   target.logical = cursor;
   target.viewport_col = viewport_col;
   target.window_cols = window_cols;
   target.display_col = 0;
   target.visible = 0;
   if (!cursor.valid)
      return target;
   target.display_col = curses_driver_display_col_from_logical(
      line, len, viewport_col, cursor.text.cell_column);
   target.visible = cursor.text.cell_column >= viewport_col
                 && (window_cols <= 0 || target.display_col < window_cols);
   target.display_col = curses_driver_clamp_display_col(target.display_col,
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
      return curses_driver_redraw_screen_cursor(scrno, view);

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
