#ifndef THE_UTFLAYOUT_H
#define THE_UTFLAYOUT_H

#include "textpos.h"
#include "utfterm.h"

typedef struct
{
   int viewport_col;
   int display_col;
   int visible;
} Utf8LayoutViewport;

/*
 * UTF layout helpers translate logical editor cell columns into physical
 * terminal display columns using the active UTF terminal profile. They must not
 * mutate or redefine TextPos.cell_column.
 */
const Utf8TerminalProfileEntry *utf8_layout_cluster_profile(
   const CHARTYPE *line, size_t len, TextCluster cluster);
int utf8_layout_cluster_logical_width(TextCluster cluster);
int utf8_layout_cluster_width(const CHARTYPE *line, size_t len,
                              TextCluster cluster);
int utf8_layout_cluster_advance_width(const CHARTYPE *line, size_t len,
                                      TextCluster cluster);
int utf8_layout_cluster_cursor_width(const CHARTYPE *line, size_t len,
                                     TextCluster cluster);
int utf8_layout_cluster_repaint_width(const CHARTYPE *line, size_t len,
                                    TextCluster cluster);
int utf8_layout_display_col_from_logical(const CHARTYPE *line, size_t len,
                                         int viewport_col, int logical_col);
int utf8_layout_logical_col_from_display(const CHARTYPE *line, size_t len,
                                         int viewport_col, int display_col,
                                         TextSnap snap);
int utf8_layout_width_col_from_logical(const CHARTYPE *line, size_t len,
                                       int logical_col);
int utf8_layout_logical_col_from_width(const CHARTYPE *line, size_t len,
                                       int width_col, TextSnap snap);
Utf8LayoutViewport utf8_layout_viewport_for_logical_col(
   const CHARTYPE *line, size_t len, int current_viewport_col,
   int logical_col, int visible_cols);

#endif
