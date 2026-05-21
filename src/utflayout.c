#include "utflayout.h"

static int min_int(int left, int right)
{
   return (left < right) ? left : right;
}

static int max_int(int left, int right)
{
   return (left > right) ? left : right;
}

const Utf8TerminalProfileEntry *utf8_layout_cluster_profile(
   const CHARTYPE *line, size_t len, TextCluster cluster)
{
   return utf8_terminal_profile_lookup_cluster(line, len, cluster,
                                               utf8_terminal_display_mode());
}

int utf8_layout_cluster_logical_width(TextCluster cluster)
{
   return (cluster.cell_width > 0) ? cluster.cell_width : 1;
}

int utf8_layout_cluster_display_width(const CHARTYPE *line, size_t len,
                                      TextCluster cluster)
{
   const Utf8TerminalProfileEntry *entry;

   entry = utf8_layout_cluster_profile(line, len, cluster);
   if (entry != NULL && entry->layout_width > 0)
      return entry->layout_width;
   return utf8_layout_cluster_logical_width(cluster);
}

int utf8_layout_cluster_cursor_width(const CHARTYPE *line, size_t len,
                                     TextCluster cluster)
{
   const Utf8TerminalProfileEntry *entry;

   entry = utf8_layout_cluster_profile(line, len, cluster);
   if (entry != NULL && entry->cursor_width > 0)
      return entry->cursor_width;
   return utf8_layout_cluster_display_width(line, len, cluster);
}

int utf8_layout_cluster_paint_width(const CHARTYPE *line, size_t len,
                                    TextCluster cluster)
{
   int paint_width = utf8_layout_cluster_display_width(line, len, cluster);
   int cursor_width = utf8_layout_cluster_cursor_width(line, len, cluster);

   if (paint_width <= 0)
      paint_width = utf8_layout_cluster_logical_width(cluster);
   if (cursor_width > paint_width)
      paint_width = cursor_width;
   return paint_width;
}

int utf8_layout_display_col_from_logical(const CHARTYPE *line, size_t len,
                                         int viewport_col, int logical_col)
{
   TextPos pos;
   int display_col = 0;
   int last_logical_col;

   if (logical_col <= viewport_col)
      return 0;

   pos = textpos_from_cell(line, len, viewport_col, TEXT_SNAP_BACKWARD);
   last_logical_col = viewport_col;
   while (pos.byte_offset < len)
   {
      TextCluster cluster = textpos_cluster_at_boundary(line, len, pos);
      int logical_start;
      int logical_end;

      if (cluster.byte_length == 0)
         break;

      logical_start = cluster.pos.cell_column;
      logical_end = cluster.end.cell_column;
      if (logical_end <= viewport_col)
      {
         pos = cluster.end;
         last_logical_col = logical_end;
         continue;
      }
      if (logical_start >= logical_col)
         break;

      if (logical_start < viewport_col)
      {
         int clipped_end = (logical_col < logical_end) ? logical_col : logical_end;
         display_col += clipped_end - viewport_col;
         last_logical_col = clipped_end;
      }
      else if (logical_end <= logical_col)
      {
         display_col += utf8_layout_cluster_display_width(line, len, cluster);
         last_logical_col = logical_end;
      }
      else
      {
         display_col += logical_col - logical_start;
         last_logical_col = logical_col;
      }

      if (logical_end >= logical_col)
         break;
      pos = cluster.end;
   }

   if (logical_col > last_logical_col)
      display_col += logical_col - last_logical_col;
   return display_col;
}

int utf8_layout_logical_col_from_display(const CHARTYPE *line, size_t len,
                                         int viewport_col, int display_col,
                                         TextSnap snap)
{
   TextPos pos;
   int screen_col = 0;
   int last_logical_col;

   if (viewport_col < 0)
      viewport_col = 0;
   if (display_col <= 0)
      return viewport_col;

   pos = textpos_from_cell(line, len, viewport_col, TEXT_SNAP_BACKWARD);
   last_logical_col = viewport_col;
   while (pos.byte_offset < len)
   {
      TextCluster cluster = textpos_cluster_at_boundary(line, len, pos);
      int logical_start;
      int logical_end;
      int logical_width;
      int display_width;

      if (cluster.byte_length == 0)
         break;

      logical_start = cluster.pos.cell_column;
      logical_end = cluster.end.cell_column;
      logical_width = (cluster.cell_width > 0) ? cluster.cell_width : 1;
      display_width = utf8_layout_cluster_display_width(line, len, cluster);
      if (display_width <= 0)
         display_width = logical_width;

      if (logical_end <= viewport_col)
      {
         pos = cluster.end;
         last_logical_col = logical_end;
         continue;
      }

      if (logical_start < viewport_col)
      {
         int clipped_width = logical_end - viewport_col;

         if (clipped_width < 0)
            clipped_width = 0;
         if (display_col < screen_col + clipped_width)
            return viewport_col;
         screen_col += clipped_width;
         pos = cluster.end;
         last_logical_col = logical_end;
         continue;
      }

      if (display_col < screen_col + display_width)
      {
         int offset = display_col - screen_col;

         if (display_width == logical_width)
            return logical_start + min_int(offset, logical_width - 1);
         if (snap == TEXT_SNAP_FORWARD)
            return logical_end;
         if (snap == TEXT_SNAP_NEAREST && offset * 2 >= display_width)
            return logical_end;
         return logical_start;
      }

      screen_col += display_width;
      last_logical_col = logical_end;
      pos = cluster.end;
   }

   if (display_col > screen_col)
      last_logical_col += display_col - screen_col;
   return last_logical_col;
}

Utf8LayoutViewport utf8_layout_viewport_for_logical_col(
   const CHARTYPE *line, size_t len, int current_viewport_col,
   int logical_col, int visible_cols)
{
   Utf8LayoutViewport target;
   int preferred_display_col;
   int low;
   int high;
   int best;

   current_viewport_col = max_int(current_viewport_col, 0);
   logical_col = max_int(logical_col, 0);
   target.viewport_col = current_viewport_col;
   target.display_col = utf8_layout_display_col_from_logical(
      line, len, current_viewport_col, logical_col);
   target.visible = logical_col >= current_viewport_col
                 && visible_cols > 0
                 && target.display_col < visible_cols;
   if (target.visible || visible_cols <= 0)
      return target;

   preferred_display_col = visible_cols / 2 - 1;
   if (preferred_display_col < 0)
      preferred_display_col = 0;

   low = 0;
   high = logical_col;
   best = logical_col;
   while (low <= high)
   {
      int mid = low + (high - low) / 2;
      int display_col = utf8_layout_display_col_from_logical(line, len, mid,
                                                             logical_col);

      if (display_col <= preferred_display_col)
      {
         best = mid;
         high = mid - 1;
      }
      else
      {
         low = mid + 1;
      }
   }

   target.viewport_col = best;
   target.display_col = utf8_layout_display_col_from_logical(line, len, best,
                                                             logical_col);
   target.visible = target.display_col < visible_cols;
   return target;
}
