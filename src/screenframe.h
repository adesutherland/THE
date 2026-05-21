#ifndef THE_SCREENFRAME_H
#define THE_SCREENFRAME_H

#include "uidriver.h"

UiRowRole screenframe_role_from_line_type(short line_type);
int screenframe_build(CHARTYPE scrno, UiFrame *frame);

#endif
