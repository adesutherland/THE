#ifndef THE_SCREENFRAME_H
#define THE_SCREENFRAME_H

#include "uidriver.h"

struct view_details;
void screenframe_format_ruler(const struct view_details *view, short line_type,
                             CHARTYPE *out, int columns);

UiRowRole screenframe_role_from_line_type(short line_type);
int screenframe_build(CHARTYPE scrno, UiFrame *frame);

#endif
