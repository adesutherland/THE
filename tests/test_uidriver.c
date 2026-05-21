#include <stdio.h>
#include <string.h>

#include "uidriver.h"

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

static void test_row_roles(void)
{
   expect_str("role.file", ui_row_role_name(UI_ROW_FILE), "file");
   expect_str("role.eof", ui_row_role_name(UI_ROW_EOF), "eof");
   expect_str("role.shadow", ui_row_role_name(UI_ROW_SHADOW), "shadow");
   expect_str("role.hex", ui_row_role_name(UI_ROW_HEX), "hex");
   expect_str("role.out", ui_row_role_name(UI_ROW_OUT_OF_BOUNDS), "out-of-bounds");
   expect_int("role.file.cursor", ui_row_role_allows_cursor(UI_ROW_FILE), 1);
   expect_int("role.prefix.cursor", ui_row_role_allows_cursor(UI_ROW_PREFIX), 1);
   expect_int("role.command.cursor", ui_row_role_allows_cursor(UI_ROW_COMMAND), 1);
   expect_int("role.eof.cursor", ui_row_role_allows_cursor(UI_ROW_EOF), 0);
   expect_int("role.tof.cursor", ui_row_role_allows_cursor(UI_ROW_TOF), 0);
   expect_int("role.status.cursor", ui_row_role_allows_cursor(UI_ROW_STATUS), 0);
   expect_int("role.hex.cursor", ui_row_role_allows_cursor(UI_ROW_HEX), 0);
}

static void test_frame_rejects_eof_cursor(void)
{
   UiFrame frame;
   LogicalCursor cursor;

   ui_frame_init(&frame, 24, 80);
   expect_int("frame.set.file", ui_frame_set_row(&frame, 0, UI_ROW_FILE,
                                                 1, 0, 0,
                                                 (const CHARTYPE *)"abc", 3, 1), 1);
   expect_int("frame.set.eof", ui_frame_set_row(&frame, 1, UI_ROW_EOF,
                                                2, 1, 0,
                                                (const CHARTYPE *)"*** Bottom of File ***",
                                                22, 0), 1);

   cursor = logical_cursor_make(LOGICAL_CURSOR_ZONE_FILEAREA, 2, 1,
                                textpos_from_cell_virtual(NULL, 0, 4,
                                                          TEXT_SNAP_BACKWARD));
   expect_int("frame.eof.cursor.rejected", ui_frame_set_cursor(&frame, cursor), 0);
   expect_int("frame.eof.cursor.valid", frame.cursor.valid, 0);
}

static void test_cursor_must_match_row_role_and_line(void)
{
   UiFrame frame;
   LogicalCursor file_cursor;
   LogicalCursor prefix_cursor;

   ui_frame_init(&frame, 24, 80);
   expect_int("frame.same_row.prefix",
              ui_frame_set_row(&frame, 0, UI_ROW_PREFIX, 41, 3, 0,
                               (const CHARTYPE *)"000041", 6, 1), 1);
   expect_int("frame.same_row.file",
              ui_frame_set_row(&frame, 1, UI_ROW_FILE, 42, 3, 0,
                               (const CHARTYPE *)"bravo", 5, 1), 1);

   file_cursor = logical_cursor_make(LOGICAL_CURSOR_ZONE_FILEAREA, 41, 3,
                                     textpos_from_cell_virtual(NULL, 0, 2,
                                                               TEXT_SNAP_BACKWARD));
   expect_int("frame.file.cursor.wrong.line",
              ui_frame_set_cursor(&frame, file_cursor), 0);

   file_cursor.line_number = 42;
   expect_int("frame.file.cursor.match",
              ui_frame_set_cursor(&frame, file_cursor), 1);
   expect_int("frame.file.cursor.role",
              ui_row_role_from_cursor_zone(file_cursor.zone), UI_ROW_FILE);

   frame.cursor.valid = 0;
   prefix_cursor = logical_cursor_make(LOGICAL_CURSOR_ZONE_PREFIX, 41, 3,
                                       textpos_from_cell_virtual(NULL, 0, 1,
                                                                 TEXT_SNAP_BACKWARD));
   expect_int("frame.prefix.cursor.match",
              ui_frame_set_cursor(&frame, prefix_cursor), 1);
   expect_int("frame.prefix.cursor.role",
              ui_row_role_from_cursor_zone(prefix_cursor.zone), UI_ROW_PREFIX);
}

static void test_cursor_for_row_handles_shared_prefix_row(void)
{
   UiFrame frame;
   LogicalCursor cursor;
   LogicalCursor found;

   ui_frame_init(&frame, 24, 80);
   expect_int("frame.shared.file",
              ui_frame_set_row(&frame, 0, UI_ROW_FILE, 42, 3, 0,
                               (const CHARTYPE *)"bravo", 5, 1), 1);
   expect_int("frame.shared.prefix",
              ui_frame_set_row_prefix(&frame, 0, (const CHARTYPE *)"000042",
                                      6, 1), 1);

   cursor = logical_cursor_make(LOGICAL_CURSOR_ZONE_PREFIX, 42, 3,
                                textpos_from_cell_virtual(NULL, 0, 2,
                                                          TEXT_SNAP_BACKWARD));
   expect_int("frame.shared.prefix.cursor",
              ui_frame_set_cursor(&frame, cursor), 1);
   expect_int("frame.shared.prefix.find",
              ui_frame_cursor_for_row(&frame, UI_ROW_PREFIX, 42, 3, &found), 1);
   expect_int("frame.shared.prefix.cell", found.text.cell_column, 2);
   expect_int("frame.shared.file.miss",
              ui_frame_cursor_for_row(&frame, UI_ROW_FILE, 42, 3, &found), 0);
}

static void test_fake_driver_materializes_cursor_once(void)
{
   UiFrame frame;
   UiDriverOpLog log;
   LogicalCursor cursor;

   ui_frame_init(&frame, 24, 80);
   ui_driver_op_log_init(&log);
   expect_int("frame.set.file", ui_frame_set_row(&frame, 0, UI_ROW_FILE,
                                                 42, 7, 0,
                                                 (const CHARTYPE *)"A1B", 3, 1), 1);
   expect_int("frame.set.status", ui_frame_set_row(&frame, 1, UI_ROW_STATUS,
                                                   0, 23, 0,
                                                   (const CHARTYPE *)"status", 6, 0), 1);

   cursor = logical_cursor_make(LOGICAL_CURSOR_ZONE_FILEAREA, 42, 7,
                                textpos_from_cell_virtual(NULL, 0, 3,
                                                          TEXT_SNAP_BACKWARD));
   expect_int("frame.cursor.accepted", ui_frame_set_cursor(&frame, cursor), 1);
   expect_int("fake.materialize", ui_fake_driver_materialize(&frame, &log), 1);
   expect_int("fake.op.count", (int)log.count, 4);
   expect_int("fake.op0.row", log.op[0].kind, UI_DRIVER_OP_ROW);
   expect_int("fake.op1.status", log.op[1].role, UI_ROW_STATUS);
   expect_int("fake.op2.cursor.kind", log.op[2].kind, UI_DRIVER_OP_CURSOR);
   expect_int("fake.op2.cursor.row", log.op[2].row, 7);
   expect_int("fake.op2.cursor.col", log.op[2].col, 3);
   expect_int("fake.op2.cursor.role", log.op[2].role, UI_ROW_FILE);
   expect_int("fake.op3.refresh", log.op[3].kind, UI_DRIVER_OP_REFRESH);
}

int main(void)
{
   test_row_roles();
   test_frame_rejects_eof_cursor();
   test_cursor_must_match_row_role_and_line();
   test_cursor_for_row_handles_shared_prefix_row();
   test_fake_driver_materializes_cursor_once();

   if (failures != 0)
   {
      fprintf(stderr, "ui driver tests failed: %d\n", failures);
      return 1;
   }
   return 0;
}
