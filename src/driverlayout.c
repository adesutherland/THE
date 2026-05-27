#include "driverlayout.h"

#ifdef USE_UTF8
# include "utflayout.h"
#endif

int driver_layout_clamp_display_col(int display_col, int window_cols)
{
   if (display_col < 0)
      return 0;
   if (window_cols > 0 && display_col >= window_cols)
      return window_cols - 1;
   return display_col;
}

int driver_layout_display_col_from_logical(const CHARTYPE *line, size_t len,
                                           int viewport_col, int logical_col)
{
#ifdef USE_UTF8
   return utf8_layout_display_col_from_logical(line, len, viewport_col,
                                               logical_col);
#else
   (void)line;
   (void)len;
   if (logical_col <= viewport_col)
      return 0;
   return logical_col - viewport_col;
#endif
}

int driver_layout_logical_col_from_display(const CHARTYPE *line, size_t len,
                                           int viewport_col, int display_col,
                                           TextSnap snap)
{
#ifdef USE_UTF8
   return utf8_layout_logical_col_from_display(line, len, viewport_col,
                                               display_col, snap);
#else
   (void)line;
   (void)len;
   (void)snap;
   if (viewport_col < 0)
      viewport_col = 0;
   if (display_col < 0)
      display_col = 0;
   return viewport_col + display_col;
#endif
}

int driver_layout_viewport_col_for_logical(const CHARTYPE *line, size_t len,
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

   (void)line;
   (void)len;
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

TheDriverCursorTarget driver_layout_filearea_target(
   LogicalCursor cursor, const CHARTYPE *line, size_t len,
   int viewport_col, int window_cols)
{
   TheDriverCursorTarget target;

   target.logical = cursor;
   target.viewport_col = viewport_col;
   target.window_cols = window_cols;
   target.raw_display_col = 0;
   target.display_col = 0;
   target.visible = 0;
   if (!cursor.valid)
      return target;
   target.raw_display_col = driver_layout_display_col_from_logical(
      line, len, viewport_col, cursor.text.cell_column);
   target.visible = cursor.text.cell_column >= viewport_col
                 && (window_cols <= 0 || target.raw_display_col < window_cols);
   target.display_col = driver_layout_clamp_display_col(target.raw_display_col,
                                                        window_cols);
   return target;
}
