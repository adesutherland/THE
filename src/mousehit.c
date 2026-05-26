#include "mousehit.h"

const char *the_mouse_hit_area_name(TheMouseHitArea area)
{
   switch (area)
   {
      case THE_MOUSE_HIT_AREA_FILEAREA:
         return "filearea";
      case THE_MOUSE_HIT_AREA_PREFIX:
         return "prefix";
      case THE_MOUSE_HIT_AREA_COMMAND:
         return "command";
      case THE_MOUSE_HIT_AREA_STATUS:
         return "status";
      case THE_MOUSE_HIT_AREA_FILETABS:
         return "filetabs";
      case THE_MOUSE_HIT_AREA_DIVIDER:
         return "divider";
      case THE_MOUSE_HIT_AREA_WINDOW:
         return "window";
      case THE_MOUSE_HIT_AREA_NONE:
      default:
         return "none";
   }
}

TheInputLogicalTargetKind the_mouse_hit_target_kind(TheMouseHitArea area)
{
   switch (area)
   {
      case THE_MOUSE_HIT_AREA_FILEAREA:
         return THE_INPUT_TARGET_FILEAREA;
      case THE_MOUSE_HIT_AREA_PREFIX:
         return THE_INPUT_TARGET_PREFIX;
      case THE_MOUSE_HIT_AREA_COMMAND:
         return THE_INPUT_TARGET_COMMAND;
      case THE_MOUSE_HIT_AREA_STATUS:
         return THE_INPUT_TARGET_STATUS;
      case THE_MOUSE_HIT_AREA_FILETABS:
         return THE_INPUT_TARGET_TABLINE;
      case THE_MOUSE_HIT_AREA_DIVIDER:
         return THE_INPUT_TARGET_DIVIDER;
      case THE_MOUSE_HIT_AREA_WINDOW:
         return THE_INPUT_TARGET_WINDOW;
      case THE_MOUSE_HIT_AREA_NONE:
      default:
         return THE_INPUT_TARGET_NONE;
   }
}

int the_mouse_hit_event_from_area(TheMouseHitArea area, LINETYPE line_number,
                                  int row, int cell, int screen,
                                  int window_id, TheInputEvent *out)
{
   return the_input_event_from_logical_target(
      the_mouse_hit_target_kind(area), line_number, row, cell, screen,
      window_id, out);
}
