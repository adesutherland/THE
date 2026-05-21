#include <stdio.h>
#include <string.h>

#include "getch.h"
#include "llmdriver.h"

static int failures = 0;

static void expect_int(const char *name, int got, int want)
{
   if (got != want)
   {
      fprintf(stderr, "%s: got %d want %d\n", name, got, want);
      failures++;
   }
}

static void expect_contains(const char *name, const char *haystack, const char *needle)
{
   if (strstr(haystack, needle) == NULL)
   {
      fprintf(stderr, "%s: missing \"%s\" in:\n%s\n", name, needle, haystack);
      failures++;
   }
}

static void test_screen_view_format(void)
{
   LlmDriverScreenView view;
   LogicalCursor cursor;
   char out[1024];

   cursor = logical_cursor_make(LOGICAL_CURSOR_ZONE_FILEAREA, 12, 1,
                                textpos_from_cell_virtual(NULL, 0, 5,
                                                          TEXT_SNAP_BACKWARD));
   llm_driver_screen_view_init(&view, 3, 80, cursor);
   llm_driver_screen_view_set_command(&view, "====> next");
   llm_driver_screen_view_set_status(&view, "LINE 12 COL 6");
   llm_driver_screen_view_set_line(&view, 0, 11, 0, "000011", "alpha", 0);
   llm_driver_screen_view_set_line(&view, 1, 12, 1, "000012", "bravo", 1);
   llm_driver_format_screen_view(&view, out, sizeof(out));

   expect_contains("format.screen", out, "screen rows=3 cols=80");
   expect_contains("format.cursor", out, "cursor zone=filearea line=12 row=1 cell=5");
   expect_contains("format.command", out, "command: ====> next");
   expect_contains("format.status", out, "status: LINE 12 COL 6");
   expect_contains("format.current", out, ">0001 line=12 prefix=\"000012\" text=\"bravo\"");
}

static void test_semantic_view_from_frame(void)
{
   UiFrame frame;
   LlmDriverScreenView view;
   LogicalCursor cursor;
   char out[2048];

   ui_frame_init(&frame, 10, 80);
   ui_frame_set_row(&frame, 0, UI_ROW_FILE, 12, 1, 0,
                    (const CHARTYPE *)"bravo", 5, 1);
   ui_frame_set_row(&frame, 1, UI_ROW_EOF, 13, 2, 0,
                    (const CHARTYPE *)"*** Bottom of File ***", 22, 0);
   cursor = logical_cursor_make(LOGICAL_CURSOR_ZONE_FILEAREA, 12, 1,
                                textpos_from_cell_virtual(NULL, 0, 3,
                                                          TEXT_SNAP_BACKWARD));
   ui_frame_set_cursor(&frame, cursor);

   expect_int("semantic.from.frame",
              llm_driver_screen_view_from_frame(&frame, &view), 1);
   llm_driver_screen_view_set_command(&view, "====> next");
   llm_driver_screen_view_set_status(&view, "LINE 12 COL 4");
   llm_driver_format_semantic_view(&view, out, sizeof(out));

   expect_contains("semantic.role.file", out, "\"role\": \"file\"");
   expect_contains("semantic.role.eof", out, "\"role\": \"eof\"");
   expect_contains("semantic.cursor", out, "\"cursor\": 1");
   expect_contains("semantic.command", out, "\"command\": \"====> next\"");
   expect_contains("semantic.text", out, "\"text\": \"bravo\"");
}

static void test_input_mapping(void)
{
   LlmDriverInput input;
   LlmDriverInputQueue queue;
   int key = 0;

   expect_int("input.left.parse", llm_driver_input_from_key_name("left", &input), 1);
   expect_int("input.left.kind", input.kind, LLM_DRIVER_INPUT_KEY);
   expect_int("input.left.key", input.key_code, KEY_LEFT);

   expect_int("input.f5.parse", llm_driver_input_from_key_name("f5", &input), 1);
   expect_int("input.f5.key", input.key_code, KEY_F(5));

   expect_int("input.text.parse", llm_driver_input_from_text('x', &input), 1);
   expect_int("input.text.key", input.key_code, 'x');

   expect_int("input.command.parse", llm_driver_input_from_command("next", &input), 1);
   expect_int("input.command.legacy", llm_driver_input_to_legacy_key(&input, &key), 0);

   expect_int("input.hit.parse",
              llm_driver_input_from_logical_hit(LOGICAL_CURSOR_ZONE_FILEAREA,
                                                12, 1, 4, &input), 1);
   expect_int("input.hit.kind", input.kind, LLM_DRIVER_INPUT_LOGICAL_HIT);
   expect_int("input.hit.cell", input.target.cell, 4);
   expect_int("input.debug.parse",
              llm_driver_input_from_debug_command("cursor-mapping", &input), 1);
   expect_int("input.debug.kind", input.kind, LLM_DRIVER_INPUT_DEBUG);
   expect_int("input.debug.command", input.debug_command,
              LLM_DRIVER_DEBUG_DUMP_CURSOR_MAPPING);

   llm_driver_input_queue_init(&queue);
   llm_driver_input_from_key_name("right", &input);
   expect_int("queue.push.right", llm_driver_input_queue_push(&queue, input), 1);
   llm_driver_input_from_text('a', &input);
   expect_int("queue.push.text", llm_driver_input_queue_push(&queue, input), 1);
   expect_int("queue.pop.right", llm_driver_input_queue_pop_legacy_key(&queue, &key), 1);
   expect_int("queue.pop.right.key", key, KEY_RIGHT);
   expect_int("queue.pop.text", llm_driver_input_queue_pop_legacy_key(&queue, &key), 1);
   expect_int("queue.pop.text.key", key, 'a');
}

static void test_debug_snapshot_format(void)
{
   LlmDriverDebugSnapshot debug;
   LogicalCursor cursor;
   UiDriverOp op;
   char out[2048];

   cursor = logical_cursor_make(LOGICAL_CURSOR_ZONE_FILEAREA, 12, 1,
                                textpos_from_cell_virtual(NULL, 0, 6,
                                                          TEXT_SNAP_BACKWARD));
   llm_driver_debug_snapshot_init(&debug, cursor);
   llm_driver_debug_snapshot_set_cursor_mapping(&debug, 0, 6, 8, 8, 1);
   llm_driver_debug_snapshot_set_last_render(&debug,
                                             "cursor overlay applied after suffix repair");
   memset(&op, 0, sizeof(op));
   op.kind = UI_DRIVER_OP_CURSOR;
   op.role = UI_ROW_FILE;
   op.row = 1;
   op.col = 8;
   op.line_number = 12;
   ui_driver_op_log_add(&debug.driver_ops, op);

   llm_driver_format_debug_snapshot(&debug, out, sizeof(out));
   expect_contains("debug.focus", out, "\"zone\": \"filearea\"");
   expect_contains("debug.mapping", out, "\"raw_display_col\": 8");
   expect_contains("debug.render", out, "suffix repair");
   expect_contains("debug.op", out, "\"kind\": \"cursor\"");
}

int main(void)
{
   test_screen_view_format();
   test_semantic_view_from_frame();
   test_input_mapping();
   test_debug_snapshot_format();

   if (failures != 0)
   {
      fprintf(stderr, "LLM driver tests failed: %d\n", failures);
      return 1;
   }
   return 0;
}
