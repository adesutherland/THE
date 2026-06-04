#include "utflayout.h"

#include <limits.h>

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

int utf8_layout_cluster_width(const CHARTYPE *line, size_t len,
                              TextCluster cluster)
{
   const Utf8TerminalProfileEntry *entry;

   entry = utf8_layout_cluster_profile(line, len, cluster);
   if (entry != NULL && entry->width > 0)
      return entry->width;
   return utf8_layout_cluster_logical_width(cluster);
}

int utf8_layout_cluster_advance_width(const CHARTYPE *line, size_t len,
                                      TextCluster cluster)
{
   const Utf8TerminalProfileEntry *entry;

   entry = utf8_layout_cluster_profile(line, len, cluster);
   if (entry != NULL && entry->advance_width > 0)
      return entry->advance_width;
   return utf8_layout_cluster_width(line, len, cluster);
}

int utf8_layout_cluster_cursor_width(const CHARTYPE *line, size_t len,
                                     TextCluster cluster)
{
   const Utf8TerminalProfileEntry *entry;

   entry = utf8_layout_cluster_profile(line, len, cluster);
   if (entry != NULL && entry->cursor_width > 0)
      return entry->cursor_width;
   return utf8_layout_cluster_advance_width(line, len, cluster);
}

int utf8_layout_cluster_repaint_width(const CHARTYPE *line, size_t len,
                                    TextCluster cluster)
{
   const Utf8TerminalProfileEntry *entry;
   int repaint_width = utf8_layout_cluster_advance_width(line, len, cluster);

   entry = utf8_layout_cluster_profile(line, len, cluster);
   if (entry != NULL && entry->repaint_width > 0)
      repaint_width = entry->repaint_width;
   if (repaint_width <= 0)
      repaint_width = utf8_layout_cluster_logical_width(cluster);
   return repaint_width;
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
         display_col += utf8_layout_cluster_advance_width(line, len, cluster);
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
      int advance_width;

      if (cluster.byte_length == 0)
         break;

      logical_start = cluster.pos.cell_column;
      logical_end = cluster.end.cell_column;
      logical_width = (cluster.cell_width > 0) ? cluster.cell_width : 1;
      advance_width = utf8_layout_cluster_advance_width(line, len, cluster);
      if (advance_width <= 0)
         advance_width = logical_width;

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

      if (display_col < screen_col + advance_width)
      {
         int offset = display_col - screen_col;

         if (snap == TEXT_SNAP_FORWARD)
            return logical_end;
         if (snap == TEXT_SNAP_NEAREST && offset * 2 >= advance_width)
            return logical_end;
         return logical_start;
      }

      screen_col += advance_width;
      last_logical_col = logical_end;
      pos = cluster.end;
   }

   if (display_col > screen_col)
      last_logical_col += display_col - screen_col;
   return last_logical_col;
}

int utf8_layout_width_col_from_logical(const CHARTYPE *line, size_t len,
                                       int logical_col)
{
   TextPos pos;
   int width_col = 0;
   int last_logical_col = 0;

   if (logical_col <= 0)
      return 0;

   pos = textpos_begin();
   while (pos.byte_offset < len)
   {
      TextCluster cluster = textpos_cluster_at_boundary(line, len, pos);
      int logical_start;
      int logical_end;

      if (cluster.byte_length == 0)
         break;

      logical_start = cluster.pos.cell_column;
      logical_end = cluster.end.cell_column;
      if (logical_start >= logical_col)
         break;
      if (logical_col < logical_end)
         break;

      width_col += utf8_layout_cluster_width(line, len, cluster);
      last_logical_col = logical_end;
      pos = cluster.end;
   }

   if (logical_col > last_logical_col)
      width_col += logical_col - last_logical_col;
   return width_col;
}

int utf8_layout_logical_col_from_width(const CHARTYPE *line, size_t len,
                                       int width_col, TextSnap snap)
{
   TextPos pos;
   int current_width_col = 0;
   int last_logical_col = 0;

   if (width_col <= 0)
      return 0;

   pos = textpos_begin();
   while (pos.byte_offset < len)
   {
      TextCluster cluster = textpos_cluster_at_boundary(line, len, pos);
      int logical_start;
      int logical_end;
      int cluster_width;

      if (cluster.byte_length == 0)
         break;

      logical_start = cluster.pos.cell_column;
      logical_end = cluster.end.cell_column;
      cluster_width = utf8_layout_cluster_width(line, len, cluster);
      if (cluster_width <= 0)
         cluster_width = utf8_layout_cluster_logical_width(cluster);

      if (width_col < current_width_col + cluster_width)
      {
         int offset = width_col - current_width_col;

         if (snap == TEXT_SNAP_FORWARD)
            return logical_end;
         if (snap == TEXT_SNAP_NEAREST && offset * 2 >= cluster_width)
            return logical_end;
         return logical_start;
      }

      current_width_col += cluster_width;
      last_logical_col = logical_end;
      pos = cluster.end;
   }

   if (width_col > current_width_col)
      last_logical_col += width_col - current_width_col;
   return last_logical_col;
}

TextCellSlice utf8_layout_slice_width(const CHARTYPE *line, size_t len,
                                      int start_width_col, int width_cols)
{
   TextCellSlice slice;
   TextPos pos;
   int end_width_col;
   int current_width_col = 0;
   int found = 0;

   slice.start = textpos_begin();
   slice.end = textpos_begin();
   slice.leading_cells = 0;
   slice.content_cells = 0;
   slice.trailing_cells = width_cols < 0 ? 0 : width_cols;

   if (width_cols <= 0)
      return slice;
   if (start_width_col < 0)
      start_width_col = 0;
   if (width_cols > INT_MAX - start_width_col)
      end_width_col = INT_MAX;
   else
      end_width_col = start_width_col + width_cols;

   pos = textpos_begin();
   while (pos.byte_offset < len)
   {
      TextCluster cluster = textpos_cluster_at_boundary(line, len, pos);
      int cluster_width;
      int next_width_col;

      if (cluster.byte_length == 0)
         break;

      cluster_width = utf8_layout_cluster_width(line, len, cluster);
      if (cluster_width <= 0)
         cluster_width = utf8_layout_cluster_logical_width(cluster);
      if (cluster_width > INT_MAX - current_width_col)
         next_width_col = INT_MAX;
      else
         next_width_col = current_width_col + cluster_width;

      if (next_width_col <= start_width_col)
      {
         current_width_col = next_width_col;
         pos = cluster.end;
         continue;
      }
      if (current_width_col >= end_width_col)
         break;

      if (!found)
      {
         slice.start = cluster.pos;
         if (start_width_col > current_width_col)
            slice.leading_cells = start_width_col - current_width_col;
         found = 1;
      }
      slice.end = cluster.end;
      slice.content_cells += cluster_width;
      current_width_col = next_width_col;
      pos = cluster.end;
   }

   if (!found)
   {
      int logical_col = utf8_layout_logical_col_from_width(
                           line, len, start_width_col, TEXT_SNAP_BACKWARD);

      slice.start = textpos_from_cell_virtual(line, len, logical_col,
                                              TEXT_SNAP_BACKWARD);
      slice.end = slice.start;
      slice.trailing_cells = width_cols;
      return slice;
   }

   slice.trailing_cells =
      current_width_col < end_width_col ? end_width_col - current_width_col : 0;
   return slice;
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
