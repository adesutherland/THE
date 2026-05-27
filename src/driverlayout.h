#ifndef THE_DRIVERLAYOUT_H
#define THE_DRIVERLAYOUT_H

#include <stddef.h>

#include "thedriver.h"

int driver_layout_clamp_display_col(int display_col, int window_cols);
int driver_layout_display_col_from_logical(const CHARTYPE *line, size_t len,
                                           int viewport_col, int logical_col);
int driver_layout_logical_col_from_display(const CHARTYPE *line, size_t len,
                                           int viewport_col, int display_col,
                                           TextSnap snap);
int driver_layout_viewport_col_for_logical(const CHARTYPE *line, size_t len,
                                           int current_viewport_col,
                                           int logical_col, int window_cols,
                                           int *display_col, int *visible);
TheDriverCursorTarget driver_layout_filearea_target(
   LogicalCursor cursor, const CHARTYPE *line, size_t len,
   int viewport_col, int window_cols);

#endif
