#include <stdio.h>
#include <string.h>

#include "screenframe.h"
#include "the.h"

SCREEN_DETAILS screen[MAX_SCREENS];

static int failures = 0;

static void expect_int(const char *name, int got, int want)
{
   if (got != want)
   {
      fprintf(stderr, "%s: got %d want %d\n", name, got, want);
      failures++;
   }
}

static void expect_strn(const char *name, const CHARTYPE *got, size_t got_len,
                        const char *want)
{
   size_t want_len = strlen(want);

   if (got == NULL || got_len != want_len
   ||  memcmp(got, want, want_len) != 0)
   {
      fprintf(stderr, "%s: got \"%.*s\" len %zu want \"%s\" len %zu\n",
              name, (int)got_len, got == NULL ? "" : (const char *)got,
              got_len, want, want_len);
      failures++;
   }
}

static void test_role_mapping(void)
{
   expect_int("role.line", screenframe_role_from_line_type(LINE_LINE), UI_ROW_FILE);
   expect_int("role.tof", screenframe_role_from_line_type(LINE_TOF), UI_ROW_TOF);
   expect_int("role.eof", screenframe_role_from_line_type(LINE_EOF), UI_ROW_EOF);
   expect_int("role.scale", screenframe_role_from_line_type(LINE_SCALE), UI_ROW_SCALE);
   expect_int("role.tabline", screenframe_role_from_line_type(LINE_TABLINE), UI_ROW_TABLINE);
   expect_int("role.bounds", screenframe_role_from_line_type(LINE_BOUNDS), UI_ROW_BOUNDS);
   expect_int("role.reserved", screenframe_role_from_line_type(LINE_RESERVED), UI_ROW_RESERVED);
   expect_int("role.shadow", screenframe_role_from_line_type(LINE_SHADOW), UI_ROW_SHADOW);
   expect_int("role.hex", screenframe_role_from_line_type(LINE_HEXSHOW), UI_ROW_HEX);
   expect_int("role.out", screenframe_role_from_line_type(LINE_OUT_OF_BOUNDS_ABOVE),
              UI_ROW_OUT_OF_BOUNDS);
}

static void test_builds_filearea_frame(void)
{
   VIEW_DETAILS view;
   SHOW_LINE rows[3];
   UiFrame frame;
   LogicalCursor cursor;

   memset(screen, 0, sizeof(screen));
   memset(&view, 0, sizeof(view));
   memset(rows, 0, sizeof(rows));

   view.tofeof = TRUE;
   view.verify_col = 1;
   screen[0].screen_view = &view;
   screen[0].sl = rows;
   screen[0].rows[WINDOW_FILEAREA] = 3;
   screen[0].cols[WINDOW_FILEAREA] = 80;

   rows[0].line_type = LINE_TOF;
   rows[0].line_number = 0;
   rows[1].line_type = LINE_LINE;
   rows[1].line_number = 42;
   rows[1].contents = (CHARTYPE *)"bravo";
   rows[1].length = 5;
   rows[1].main_enterable = TRUE;
   rows[1].prefix_enterable = TRUE;
   strcpy((char *)rows[1].prefix, "000042");
   rows[2].line_type = LINE_EOF;
   rows[2].line_number = 43;

   cursor = logical_cursor_make(LOGICAL_CURSOR_ZONE_FILEAREA, 42, 1,
                                textpos_from_cell_virtual(NULL, 0, 3,
                                                          TEXT_SNAP_BACKWARD));
   logical_cursor_state_focus(&view.logical_cursor, cursor);

   expect_int("screenframe.build", screenframe_build(0, &frame), 1);
   expect_int("screenframe.rows", (int)frame.row_count, 3);
   expect_int("screenframe.tof.role", frame.row[0].role, UI_ROW_TOF);
   expect_strn("screenframe.tof.text", frame.row[0].text, frame.row[0].text_len,
               "*** Top of File ***");
   expect_int("screenframe.file.role", frame.row[1].role, UI_ROW_FILE);
   expect_strn("screenframe.file.text", frame.row[1].text, frame.row[1].text_len,
               "bravo");
   expect_strn("screenframe.file.prefix", frame.row[1].prefix,
               frame.row[1].prefix_len, "000042");
   expect_int("screenframe.file.prefix.editable",
              frame.row[1].prefix_editable, 1);
   expect_int("screenframe.cursor.valid", frame.cursor.valid, 1);
   expect_int("screenframe.cursor.row", frame.cursor.cursor.zone_row, 1);
   expect_int("screenframe.eof.role", frame.row[2].role, UI_ROW_EOF);
}

int main(void)
{
   test_role_mapping();
   test_builds_filearea_frame();

   if (failures != 0)
   {
      fprintf(stderr, "screenframe tests failed: %d\n", failures);
      return 1;
   }
   return 0;
}
