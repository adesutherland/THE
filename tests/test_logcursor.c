#include <stdio.h>
#include <string.h>

#include "logcursor.h"

static int failures = 0;

static void expect_int(const char *name, int got, int want)
{
   if (got != want)
   {
      fprintf(stderr, "%s: got %d want %d\n", name, got, want);
      failures++;
   }
}

static void expect_size(const char *name, size_t got, size_t want)
{
   if (got != want)
   {
      fprintf(stderr, "%s: got %zu want %zu\n", name, got, want);
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

static void expect_cursor(const char *name, LogicalCursor cursor,
                          LogicalCursorZone zone, LINETYPE line_number,
                          int zone_row, size_t byte_offset,
                          size_t codepoint_index, size_t cluster_index,
                          int cell_column, int desired_cell)
{
   char field[128];

   snprintf(field, sizeof(field), "%s.valid", name);
   expect_int(field, cursor.valid, 1);
   snprintf(field, sizeof(field), "%s.zone", name);
   expect_int(field, cursor.zone, zone);
   snprintf(field, sizeof(field), "%s.line", name);
   expect_int(field, (int)cursor.line_number, (int)line_number);
   snprintf(field, sizeof(field), "%s.row", name);
   expect_int(field, cursor.zone_row, zone_row);
   snprintf(field, sizeof(field), "%s.byte", name);
   expect_size(field, cursor.text.byte_offset, byte_offset);
   snprintf(field, sizeof(field), "%s.codepoint", name);
   expect_size(field, cursor.text.codepoint_index, codepoint_index);
   snprintf(field, sizeof(field), "%s.cluster", name);
   expect_size(field, cursor.text.cluster_index, cluster_index);
   snprintf(field, sizeof(field), "%s.cell", name);
   expect_int(field, cursor.text.cell_column, cell_column);
   snprintf(field, sizeof(field), "%s.desired", name);
   expect_int(field, cursor.desired_cell, desired_cell);
}

static void test_zones(void)
{
   expect_str("zone.none", logical_cursor_zone_name(LOGICAL_CURSOR_ZONE_NONE), "none");
   expect_str("zone.file", logical_cursor_zone_name(LOGICAL_CURSOR_ZONE_FILEAREA), "filearea");
   expect_str("zone.prefix", logical_cursor_zone_name(LOGICAL_CURSOR_ZONE_PREFIX), "prefix");
   expect_str("zone.command", logical_cursor_zone_name(LOGICAL_CURSOR_ZONE_COMMAND), "command");
   expect_str("zone.prompt", logical_cursor_zone_name(LOGICAL_CURSOR_ZONE_PROMPT), "prompt");
   expect_str("zone.status", logical_cursor_zone_name(LOGICAL_CURSOR_ZONE_STATUS), "status");
}

static void test_filearea_virtual_movement(void)
{
   static const CHARTYPE s[] = { 'A', 0xF0, 0x9F, 0x98, 0x80, 'B' };
   LogicalCursor cursor;

   cursor = logical_cursor_from_cell(LOGICAL_CURSOR_ZONE_FILEAREA, 10, 3,
                                     s, sizeof(s), 8, TEXT_SNAP_BACKWARD, 1);
   expect_cursor("file.virtual", cursor, LOGICAL_CURSOR_ZONE_FILEAREA,
                 10, 3, 6, 7, 7, 8, 8);
   expect_int("file.virtual.flag", logical_cursor_is_virtual(cursor, s, sizeof(s)), 1);

   cursor = logical_cursor_move_left(cursor, s, sizeof(s), 1);
   expect_cursor("file.virtual.left", cursor, LOGICAL_CURSOR_ZONE_FILEAREA,
                 10, 3, 6, 6, 6, 7, 7);

   cursor = logical_cursor_from_cell(LOGICAL_CURSOR_ZONE_FILEAREA, 10, 3,
                                     s, sizeof(s), 3, TEXT_SNAP_BACKWARD, 1);
   cursor = logical_cursor_move_left(cursor, s, sizeof(s), 1);
   expect_cursor("file.after.emoji.left", cursor, LOGICAL_CURSOR_ZONE_FILEAREA,
                 10, 3, 1, 1, 1, 1, 1);

   cursor = logical_cursor_move_right(cursor, s, sizeof(s), 1);
   expect_cursor("file.emoji.right", cursor, LOGICAL_CURSOR_ZONE_FILEAREA,
                 10, 3, 5, 2, 2, 3, 3);

   cursor = logical_cursor_from_cell(LOGICAL_CURSOR_ZONE_FILEAREA, 10, 3,
                                     s, sizeof(s), 4, TEXT_SNAP_BACKWARD, 1);
   cursor = logical_cursor_move_right(cursor, s, sizeof(s), 1);
   expect_cursor("file.eol.right", cursor, LOGICAL_CURSOR_ZONE_FILEAREA,
                 10, 3, 6, 4, 4, 5, 5);
}

static void test_command_and_prefix_positions(void)
{
   static const CHARTYPE cmd[] = { 'a', 'b', 'c' };
   static const CHARTYPE prefix[] = { '=', '=', '=', '>' };
   LogicalCursor command;
   LogicalCursor pref;

   command = logical_cursor_from_cell(LOGICAL_CURSOR_ZONE_COMMAND, 0, 0,
                                      cmd, sizeof(cmd), 2, TEXT_SNAP_BACKWARD, 1);
   expect_cursor("command.cell", command, LOGICAL_CURSOR_ZONE_COMMAND,
                 0, 0, 2, 2, 2, 2, 2);

   logical_cursor_set_desired_cell(&command, 6);
   expect_int("command.desired", command.desired_cell, 6);

   pref = logical_cursor_from_cell(LOGICAL_CURSOR_ZONE_PREFIX, 27, 4,
                                   prefix, sizeof(prefix), 3,
                                   TEXT_SNAP_BACKWARD, 0);
   expect_cursor("prefix.cell", pref, LOGICAL_CURSOR_ZONE_PREFIX,
                 27, 4, 3, 3, 3, 3, 3);
}

static void test_focus_state(void)
{
   LogicalCursorState state;
   LogicalCursor command;
   LogicalCursor file;

   logical_cursor_state_init(&state);
   expect_int("state.current.invalid", state.current.valid, 0);
   expect_int("state.previous.invalid", state.previous.valid, 0);

   command = logical_cursor_make(LOGICAL_CURSOR_ZONE_COMMAND, 0, 0, textpos_begin());
   logical_cursor_state_focus(&state, command);
   expect_cursor("state.current.command", state.current, LOGICAL_CURSOR_ZONE_COMMAND,
                 0, 0, 0, 0, 0, 0, 0);
   expect_int("state.previous.after.first", state.previous.valid, 0);

   file = logical_cursor_make(LOGICAL_CURSOR_ZONE_FILEAREA, 5, 2,
                              textpos_from_cell_virtual(NULL, 0, 3,
                                                        TEXT_SNAP_BACKWARD));
   logical_cursor_state_focus(&state, file);
   expect_cursor("state.current.file", state.current, LOGICAL_CURSOR_ZONE_FILEAREA,
                 5, 2, 0, 3, 3, 3, 3);
   expect_cursor("state.previous.command", state.previous, LOGICAL_CURSOR_ZONE_COMMAND,
                 0, 0, 0, 0, 0, 0, 0);
}

int main(void)
{
   test_zones();
   test_filearea_virtual_movement();
   test_command_and_prefix_positions();
   test_focus_state();

   if (failures != 0)
   {
      fprintf(stderr, "logical cursor tests failed: %d\n", failures);
      return 1;
   }

   return 0;
}
