#ifndef THE_MOUSEHIT_H
#define THE_MOUSEHIT_H

#include "inputevent.h"

typedef enum
{
   THE_MOUSE_HIT_AREA_NONE = 0,
   THE_MOUSE_HIT_AREA_FILEAREA,
   THE_MOUSE_HIT_AREA_PREFIX,
   THE_MOUSE_HIT_AREA_COMMAND,
   THE_MOUSE_HIT_AREA_STATUS,
   THE_MOUSE_HIT_AREA_FILETABS,
   THE_MOUSE_HIT_AREA_DIVIDER,
   THE_MOUSE_HIT_AREA_WINDOW
} TheMouseHitArea;

const char *the_mouse_hit_area_name(TheMouseHitArea area);
TheInputLogicalTargetKind the_mouse_hit_target_kind(TheMouseHitArea area);
int the_mouse_hit_event_from_area(TheMouseHitArea area, LINETYPE line_number,
                                  int row, int cell, int screen,
                                  int window_id, TheInputEvent *out);

#endif
