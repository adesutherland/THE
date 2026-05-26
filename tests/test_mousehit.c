#include <stdio.h>
#include <string.h>

#include "mousehit.h"

static int failures = 0;

static void expect_int(const char *name, int got, int want)
{
   if (got != want)
   {
      fprintf(stderr, "%s: got %d want %d\n", name, got, want);
      failures++;
   }
}

static void expect_str(const char *name, const char *got, const char *want)
{
   if (strcmp(got, want) != 0)
   {
      fprintf(stderr, "%s: got %s want %s\n", name, got, want);
      failures++;
   }
}

static void expect_area(const char *name, TheMouseHitArea area,
                        TheInputLogicalTargetKind kind,
                        LogicalCursorZone zone)
{
   TheInputEvent input;
   char label[96];

   expect_int(name, the_mouse_hit_target_kind(area), kind);
   snprintf(label, sizeof(label), "%s.parse", name);
   expect_int(label,
              the_mouse_hit_event_from_area(area, 42, 3, 7, 1, 9, &input),
              kind == THE_INPUT_TARGET_NONE ? 0 : 1);
   if (kind == THE_INPUT_TARGET_NONE)
      return;
   snprintf(label, sizeof(label), "%s.input.kind", name);
   expect_int(label, input.kind, THE_INPUT_LOGICAL_HIT);
   snprintf(label, sizeof(label), "%s.target.kind", name);
   expect_int(label, input.target.kind, kind);
   snprintf(label, sizeof(label), "%s.zone", name);
   expect_int(label, input.target.zone, zone);
   snprintf(label, sizeof(label), "%s.line", name);
   expect_int(label, (int)input.target.line_number, 42);
   snprintf(label, sizeof(label), "%s.row", name);
   expect_int(label, input.target.row, 3);
   snprintf(label, sizeof(label), "%s.cell", name);
   expect_int(label, input.target.cell, 7);
   snprintf(label, sizeof(label), "%s.screen", name);
   expect_int(label, input.target.screen, 1);
   snprintf(label, sizeof(label), "%s.window", name);
   expect_int(label, input.target.window_id, 9);
}

static void test_area_mapping(void)
{
   expect_str("name.filearea",
              the_mouse_hit_area_name(THE_MOUSE_HIT_AREA_FILEAREA),
              "filearea");
   expect_area("map.filearea", THE_MOUSE_HIT_AREA_FILEAREA,
               THE_INPUT_TARGET_FILEAREA, LOGICAL_CURSOR_ZONE_FILEAREA);
   expect_area("map.prefix", THE_MOUSE_HIT_AREA_PREFIX,
               THE_INPUT_TARGET_PREFIX, LOGICAL_CURSOR_ZONE_PREFIX);
   expect_area("map.command", THE_MOUSE_HIT_AREA_COMMAND,
               THE_INPUT_TARGET_COMMAND, LOGICAL_CURSOR_ZONE_COMMAND);
   expect_area("map.status", THE_MOUSE_HIT_AREA_STATUS,
               THE_INPUT_TARGET_STATUS, LOGICAL_CURSOR_ZONE_STATUS);
   expect_area("map.filetabs", THE_MOUSE_HIT_AREA_FILETABS,
               THE_INPUT_TARGET_TABLINE, LOGICAL_CURSOR_ZONE_NONE);
   expect_area("map.divider", THE_MOUSE_HIT_AREA_DIVIDER,
               THE_INPUT_TARGET_DIVIDER, LOGICAL_CURSOR_ZONE_NONE);
   expect_area("map.window", THE_MOUSE_HIT_AREA_WINDOW,
               THE_INPUT_TARGET_WINDOW, LOGICAL_CURSOR_ZONE_NONE);
   expect_area("map.none", THE_MOUSE_HIT_AREA_NONE,
               THE_INPUT_TARGET_NONE, LOGICAL_CURSOR_ZONE_NONE);
}

int main(void)
{
   test_area_mapping();
   if (failures != 0)
   {
      fprintf(stderr, "mouse hit tests failed: %d\n", failures);
      return 1;
   }
   return 0;
}
