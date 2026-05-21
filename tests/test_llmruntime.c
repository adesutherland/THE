#include <stdio.h>
#include <string.h>

#include "llmruntime.h"
#include "the.h"

SCREEN_DETAILS screen[MAX_SCREENS];
CHARTYPE *cmd_rec = NULL;
LENGTHTYPE cmd_rec_len = 0;
LENGTHTYPE cmd_verify_col = 1;

static int failures = 0;

static void expect_int(const char *name, int got, int want)
{
   if (got != want)
   {
      fprintf(stderr, "%s: got %d want %d\n", name, got, want);
      failures++;
   }
}

static void expect_contains(const char *name, const char *haystack,
                            const char *needle)
{
   if (strstr(haystack, needle) == NULL)
   {
      fprintf(stderr, "%s: missing \"%s\" in:\n%s\n",
              name, needle, haystack);
      failures++;
   }
}

static void expect_absent(const char *name, const char *haystack,
                          const char *needle)
{
   if (strstr(haystack, needle) != NULL)
   {
      fprintf(stderr, "%s: unexpected \"%s\" in:\n%s\n",
              name, needle, haystack);
      failures++;
   }
}

static void setup_screen(void)
{
   static VIEW_DETAILS view;
   static SHOW_LINE rows[3];
   static CHARTYPE command[] = "command text";
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
   rows[1].line_number = 7;
   rows[1].contents = (CHARTYPE *)"visible file row";
   rows[1].length = 16;
   rows[1].main_enterable = TRUE;
   strcpy((char *)rows[1].prefix, "000007");
   rows[2].line_type = LINE_SCALE;
   rows[2].line_number = 0;
   rows[2].contents = (CHARTYPE *)"....+....1";
   rows[2].length = 10;

   cursor = logical_cursor_make(LOGICAL_CURSOR_ZONE_FILEAREA, 7, 1,
                                textpos_from_cell_virtual(NULL, 0, 8,
                                                          TEXT_SNAP_BACKWARD));
   logical_cursor_state_focus(&view.logical_cursor, cursor);

   cmd_rec = command;
   cmd_rec_len = strlen((char *)command);
   cmd_verify_col = 1;
}

static void test_runtime_view(void)
{
   LlmDriverScreenView view;

   setup_screen();
   expect_int("runtime.view", llm_runtime_screen_view(0, &view), 1);
   expect_int("runtime.rows", (int)view.line_count, 3);
   expect_int("runtime.cursor.row", view.cursor.zone_row, 1);
   expect_contains("runtime.command", view.command_line, "command text");
   expect_contains("runtime.status", view.status, "focus=filearea");
}

static void test_compact_filearea_runtime_format(void)
{
   LlmDriverFormatOptions options;
   char out[2048];

   setup_screen();
   llm_driver_format_options_init(&options);
   options.mode = LLM_DRIVER_VIEW_FILEAREA;
   options.compact = 1;
   options.include_prefix = 0;
   options.include_command = 0;
   options.include_status = 0;
   options.max_text_cols = 7;

   expect_int("runtime.format",
              llm_runtime_format_screen(0, &options, out, sizeof(out)) > 0, 1);
   expect_contains("runtime.format.file", out, "\"t\":\"visible...\"");
   expect_absent("runtime.format.tof", out, "Top of File");
   expect_absent("runtime.format.scale", out, "....+....1");
   expect_absent("runtime.format.command", out, "command text");
   expect_absent("runtime.format.prefix", out, "\"p\"");
}

int main(void)
{
   test_runtime_view();
   test_compact_filearea_runtime_format();

   if (failures != 0)
   {
      fprintf(stderr, "llm runtime tests failed: %d\n", failures);
      return 1;
   }
   return 0;
}
