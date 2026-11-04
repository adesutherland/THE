#ifndef THE_RESERVEDFOOTER_H
#define THE_RESERVEDFOOTER_H

#ifdef USE_UTF8
#include "utflayout.h"

/* Bounded decoding, logical VERIFY scrolling and physical-cell clipping.
 * The reserved highlighting array is byte-indexed, like disp itself. */
static inline int reserved_footer_next_cluster(const RESERVED *reserved,
   int verify_col, int columns, TextPos *next, TextCluster *cluster,
   int *display_col, int *advance_width)
{
   int viewport = reserved->autoscroll && verify_col > 1 ? verify_col - 1 : 0;
   size_t length = reserved->disp_length > 0 ? (size_t)reserved->disp_length : 0;
   while (reserved->disp && next->byte_offset < length)
   {
      int occupied, width, repaint;
      *cluster = textpos_cluster_at_boundary(reserved->disp, length, *next);
      if (!cluster->byte_length) return 0;
      *next = cluster->end;
      if (cluster->pos.cell_column < viewport) continue;
      *display_col = utf8_layout_display_col_from_logical(reserved->disp,
         length, viewport, cluster->pos.cell_column);
      if (*display_col < 0) continue;
      *advance_width = utf8_layout_cluster_advance_width(reserved->disp, length, *cluster);
      if (*advance_width < 1) *advance_width = 1;
      width = utf8_layout_cluster_width(reserved->disp, length, *cluster);
      repaint = utf8_layout_cluster_repaint_width(reserved->disp, length, *cluster);
      occupied = width > *advance_width ? width : *advance_width;
      if (repaint > occupied) occupied = repaint;
      if (*display_col >= columns || occupied > columns - *display_col) return 0;
      return 1;
   }
   return 0;
}

typedef void (*ReservedFooterClusterWriter)(TheDriverWindow *, int, int,
   const CHARTYPE *, size_t, TextCluster, TheDriverAttr, int);

static inline void reserved_footer_paint_utf8(const RESERVED *reserved,
   TheDriverWindow *window, int row, int columns, int verify_col,
   TheDriverAttr normal, const TheDriverOps *driver,
   ReservedFooterClusterWriter write_cluster)
{
   TextPos next = textpos_begin();
   TextCluster cluster;
   int col, advance;
   driver->fill_cells_at(window, row, 0, columns, normal);
   while (reserved_footer_next_cluster(reserved, verify_col, columns,
      &next, &cluster, &col, &advance))
   {
      TheDriverAttr colour = reserved->highlighting
         ? reserved->highlighting[cluster.pos.byte_offset] : normal;
      write_cluster(window, row, col, reserved->disp,
         reserved->disp_length, cluster, colour, advance);
   }
}
#endif

/* Explicit profile placement only: footer names never determine geometry. */
static inline int reserved_footer_count(const RESERVED *first)
{
   int count = 0;
   for (; first; first = first->next)
      if (first->footer_after_cmdline && first->footer[0]) count++;
   return count;
}

static inline int reserved_footer_rows(const VIEW_DETAILS *view, int screen_rows)
{
   int available, count;
   if (!view || !view->file_for_view) return 0;
   available = screen_rows - (view->id_line ? 1 : 0)
               - (view->cmd_line == 'O' ? 0 : 1) - 1;
   count = reserved_footer_count(view->file_for_view->first_reserved);
   return available > 0 ? (count < available ? count : available) : 0;
}

static inline int reserved_footer_position(const RESERVED *row, int rows)
{
   if (row->base == POSITION_TOP) return row->off - 1;
   if (row->base == POSITION_BOTTOM) return rows + row->off;
   return rows / 2 + row->off - 1;
}

/* Compact selected rows into their declared screen-position order. */
static inline RESERVED *reserved_footer_at(RESERVED *first, int index, int rows)
{
   RESERVED *candidate, *other;
   for (candidate = first; candidate; candidate = candidate->next)
   {
      int rank = 0, before = 1;
      int position;
      if (!candidate->footer_after_cmdline || !candidate->footer[0]) continue;
      position = reserved_footer_position(candidate, rows);
      for (other = first; other; other = other->next)
      {
         int other_position;
         if (other == candidate) { before = 0; continue; }
         if (!other->footer_after_cmdline || !other->footer[0]) continue;
         other_position = reserved_footer_position(other, rows);
         if (other_position < position || (other_position == position && before)) rank++;
      }
      if (rank == index) return candidate;
   }
   return NULL;
}

#endif
