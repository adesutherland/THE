#include <stdio.h>
#include <string.h>

#include "getch.h"
#include "llmdriver.h"
#ifdef USE_UTF8
# include "utfterm.h"
#endif

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
   llm_driver_screen_view_set_buffer(&view, "sample.txt", 1, 2);
   llm_driver_screen_view_set_line(&view, 0, 11, 0, "000011", "alpha", 0);
   llm_driver_screen_view_set_line(&view, 1, 12, 1, "000012", "bravo", 1);
   llm_driver_format_screen_view(&view, out, sizeof(out));

   expect_contains("format.screen", out, "screen rows=3 cols=80");
   expect_contains("format.cursor", out, "cursor zone=filearea line=12 row=1 cell=5");
   expect_contains("format.command", out, "command: ====> next");
   expect_contains("format.status", out, "status: LINE 12 COL 6");
   expect_contains("format.buffer", out,
                   "buffer path=\"sample.txt\" dirty=1 lines=2");
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
   ui_frame_add_row_style(&frame, 0, 0, 2, UI_SYNTAX_KEYWORD);
   ui_frame_add_row_style(&frame, 0, 3, 2, UI_SYNTAX_STRING);
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
   expect_contains("semantic.styles", out, "\"styles\": [");
   expect_contains("semantic.style.keyword", out,
                   "{\"start\": 0, \"len\": 2, \"style\": \"keyword\"}");
   expect_contains("semantic.style.string", out,
                   "{\"start\": 3, \"len\": 2, \"style\": \"string\"}");
}

static void test_compact_filearea_view_options(void)
{
   UiFrame frame;
   LlmDriverScreenView view;
   LlmDriverFormatOptions options;
   LogicalCursor cursor;
   char out[2048];

   cursor = logical_cursor_make(LOGICAL_CURSOR_ZONE_FILEAREA, 12, 1,
                                textpos_from_cell_virtual(NULL, 0, 3,
                                                          TEXT_SNAP_BACKWARD));
   ui_frame_init(&frame, 6, 80);
   ui_frame_set_row(&frame, 0, UI_ROW_TOF, 0, 0, 0,
                    (const CHARTYPE *)"*** Top of File ***", 19, 0);
   ui_frame_set_row(&frame, 1, UI_ROW_FILE, 12, 1, 0,
                    (const CHARTYPE *)"bravo-long-line", 15, 1);
   ui_frame_set_row_prefix(&frame, 1, (const CHARTYPE *)"000012", 6, 1);
   ui_frame_add_row_style(&frame, 1, 0, 15, UI_SYNTAX_FUNCTION);
   ui_frame_set_row(&frame, 2, UI_ROW_EOF, 13, 2, 0,
                    (const CHARTYPE *)"*** Bottom of File ***", 22, 0);
   ui_frame_set_cursor(&frame, cursor);
   llm_driver_screen_view_from_frame(&frame, &view);

   llm_driver_format_options_init(&options);
   options.mode = LLM_DRIVER_VIEW_FILEAREA;
   options.compact = 1;
   options.include_prefix = 0;
   options.include_command = 0;
   options.include_status = 0;
   options.max_text_cols = 5;
   llm_driver_format_semantic_view_with_options(&view, &options,
                                                out, sizeof(out));

   expect_contains("compact.mode", out, "\"mode\":\"filearea\"");
   expect_contains("compact.file.text", out, "\"t\":\"bravo...\"");
   expect_contains("compact.styles", out, "\"s\":[[0,5,\"function\"]]");
   expect_int("compact.hides.tof", strstr(out, "Top of File") == NULL, 1);
   expect_int("compact.hides.eof", strstr(out, "Bottom of File") == NULL, 1);
   expect_int("compact.hides.prefix", strstr(out, "\"p\"") == NULL, 1);
}

static void test_reserved_view_options(void)
{
   LlmDriverScreenView view;
   LlmDriverFormatOptions options;
   LogicalCursor cursor;
   char out[2048];

   cursor = logical_cursor_invalid();
   llm_driver_screen_view_init(&view, 6, 80, cursor);
   llm_driver_screen_view_set_row(&view, 0, UI_ROW_FILE, 12, 1, 0,
                                  "000012", "bravo", 1, 0);
   llm_driver_screen_view_set_row(&view, 1, UI_ROW_SCALE, 0, 2, 0,
                                  "", "....+....1", 0, 0);
   llm_driver_screen_view_set_row(&view, 2, UI_ROW_BOUNDS, 0, 3, 0,
                                  "", "<-------->", 0, 0);

   llm_driver_format_options_init(&options);
   options.mode = LLM_DRIVER_VIEW_RESERVED;
   options.compact = 1;
   options.include_cursor = 0;
   options.include_command = 0;
   options.include_status = 0;
   llm_driver_format_semantic_view_with_options(&view, &options,
                                                out, sizeof(out));

   expect_contains("reserved.scale", out, "\"role\":\"scale\"");
   expect_contains("reserved.bounds", out, "\"role\":\"bounds\"");
   expect_int("reserved.hides.file", strstr(out, "bravo") == NULL, 1);
}

static void test_agent_metadata_and_delta(void)
{
   LlmDriverScreenView previous;
   LlmDriverScreenView current;
   LlmDriverFormatOptions options;
   LogicalCursor cursor;
   char out[8192];

   cursor = logical_cursor_make(LOGICAL_CURSOR_ZONE_FILEAREA, 2, 1,
                                textpos_from_cell_virtual(NULL, 0, 1,
                                                          TEXT_SNAP_BACKWARD));
   llm_driver_screen_view_init(&previous, 6, 80, cursor);
   llm_driver_screen_view_set_row(&previous, 0, UI_ROW_FILE, 1, 0, 0,
                                  "     1", "alpha", 1, 0);
   llm_driver_screen_view_set_row(&previous, 1, UI_ROW_FILE, 2, 1, 0,
                                  "     2", "bravo", 1, 1);
   llm_driver_screen_view_set_buffer(&previous, "a.txt", 0, 2);

   current = previous;
   llm_driver_screen_view_set_row(&current, 1, UI_ROW_FILE, 2, 1, 0,
                                  "r edit", "edited", 1, 1);
   llm_driver_screen_view_set_prefix_command(&current, 1, "r edit");
   llm_driver_screen_view_set_history(&current, 1, 0);
   llm_driver_screen_view_set_selection(&current, 1, 2, 1, 2, 4, "ed");
   llm_driver_screen_view_add_buffer_info(&current, "a.txt", 1, 2, 1);
   llm_driver_screen_view_add_buffer_info(&current, "b.txt", 0, 3, 0);
   llm_driver_screen_view_set_project_root(&current, "/tmp/project");
   llm_driver_screen_view_add_project_file(&current, "a.txt");
   llm_driver_screen_view_add_project_file(&current, "b.txt");

   llm_driver_format_options_init(&options);
   options.compact = 1;
   options.mode = LLM_DRIVER_VIEW_FULL;
   llm_driver_format_semantic_view_with_options(&current, &options,
                                                out, sizeof(out));

   expect_contains("metadata.history", out,
                   "\"history\":{\"undo\":1,\"redo\":0}");
   expect_contains("metadata.selection", out,
                   "\"selection\":{\"active\":1");
   expect_contains("metadata.clipboard", out, "\"clipboard\":\"ed\"");
   expect_contains("metadata.buffers", out, "\"buffers\":[{\"index\":0");
   expect_contains("metadata.project", out,
                   "\"project\":{\"root\":\"/tmp/project\"");
   expect_contains("metadata.prefix.command", out, "\"pc\":\"r edit\"");

   llm_driver_format_delta_view(&previous, &current, &options,
                                out, sizeof(out));
   expect_contains("delta.mode", out, "\"mode\":\"delta\"");
   expect_contains("delta.baseline", out, "\"baseline\":1");
   expect_contains("delta.changed.row", out, "\"t\":\"edited\"");
   expect_contains("delta.prefix.command", out, "\"pc\":\"r edit\"");
}

static void test_utf_physical_metadata(void)
{
#ifdef USE_UTF8
   static const CHARTYPE keycap[] = {
      'A', '1', 0xEF, 0xB8, 0x8F, 0xE2, 0x83, 0xA3, 'B', 0
   };
   LlmDriverScreenView view;
   LlmDriverFormatOptions options;
   LogicalCursor cursor;
   char out[4096];

   cursor = logical_cursor_make(LOGICAL_CURSOR_ZONE_FILEAREA, 7, 1,
                                textpos_from_cell_virtual(NULL, 0, 12,
                                                          TEXT_SNAP_BACKWARD));
   utf8_terminal_profile_reset();
   expect_int("utf.meta.base.output.apply",
              utf8_terminal_profile_apply_line(
                 "SET UTF TERMINAL CLASS keycap OUTPUT base"),
              UTF8_TERMINAL_PROFILE_APPLIED);
   expect_int("utf.meta.base.mark.apply",
              utf8_terminal_profile_apply_line(
                 "SET UTF TERMINAL CLASS keycap MARK compressed"),
              UTF8_TERMINAL_PROFILE_APPLIED);
   llm_driver_screen_view_init(&view, 3, 80, cursor);
   llm_driver_screen_view_set_row(&view, 0, UI_ROW_FILE, 7, 1, 10,
                                  "000007", (const char *)keycap, 1, 1);
   expect_int("utf.meta.base.count",
              (int)view.lines[0].utf_cluster_count, 3);
   expect_int("utf.meta.base.cell",
              view.lines[0].utf_clusters[1].cell, 11);
   llm_driver_format_semantic_view(&view, out, sizeof(out));
   expect_contains("utf.meta.full.class", out, "\"class\": \"keycap\"");
   expect_contains("utf.meta.full.row", out, "\"row\": 1");
   expect_contains("utf.meta.full.screen_cell", out, "\"screen_cell\": 1");
   expect_contains("utf.meta.full.text", out, "\"text\": \"1️⃣\"");
   expect_contains("utf.meta.full.codepoints", out,
                   "\"codepoints\": \"U+31 U+FE0F U+20E3\"");
   expect_contains("utf.meta.full.advance.width", out,
                   "\"advance_width\": 2");
   expect_contains("utf.meta.full.cursor.width", out,
                   "\"cursor_width\": 2");
   expect_contains("utf.meta.full.repaint.width", out,
                   "\"repaint_width\": 2");
   expect_contains("utf.meta.full.output", out, "\"output\": \"base\"");
   expect_contains("utf.meta.full.mark", out, "\"mark\": \"compressed\"");
   expect_contains("utf.meta.full.compressed", out, "\"compressed\": 1");
   expect_contains("utf.meta.full.substituted0", out, "\"substituted\": 0");

   llm_driver_format_options_init(&options);
   options.compact = 1;
   options.max_text_cols = 2;
   llm_driver_format_semantic_view_with_options(&view, &options,
                                                out, sizeof(out));
   expect_contains("utf.meta.compact",
                   out, "\"u\":[[11,1,2,2,2,2,\"keycap\",\"base\",\"compressed\",1,0]]");

   utf8_terminal_profile_reset();
   expect_int("utf.meta.substitute.apply",
              utf8_terminal_profile_apply_line(
                 "SET UTF TERMINAL CLASS keycap OUTPUT substitute U+25A1"),
              UTF8_TERMINAL_PROFILE_APPLIED);
   llm_driver_screen_view_set_row(&view, 0, UI_ROW_FILE, 7, 1, 10,
                                  "000007", (const char *)keycap, 1, 1);
   llm_driver_format_semantic_view(&view, out, sizeof(out));
   expect_contains("utf.meta.sub.output", out, "\"output\": \"substitute\"");
   expect_contains("utf.meta.sub.mark", out, "\"mark\": \"substituted\"");
   expect_contains("utf.meta.sub.flag", out, "\"substituted\": 1");
   utf8_terminal_profile_reset();
#endif
}

static void test_utf_all_metadata_representative_clusters(void)
{
#ifdef USE_UTF8
   static const CHARTYPE sample[] = {
      'A', ' ',
      'e', 0xCC, 0x81, ' ',
      0xE4, 0xB8, 0xAD, ' ',
      '1', 0xEF, 0xB8, 0x8F, 0xE2, 0x83, 0xA3, ' ',
      0xF0, 0x9F, 0x87, 0xBA, 0xF0, 0x9F, 0x87, 0xB8, ' ',
      0xF0, 0x9F, 0x91, 0x8D, 0xF0, 0x9F, 0x8F, 0xBD, ' ',
      0xF0, 0x9F, 0x91, 0xA9, 0xE2, 0x80, 0x8D,
      0xF0, 0x9F, 0x92, 0xBB, 0
   };
   static const CHARTYPE heart_zwj[] = {
      0xF0, 0x9F, 0x91, 0xA9, 0xE2, 0x80, 0x8D,
      0xE2, 0x9D, 0xA4, 0xEF, 0xB8, 0x8F,
      0xE2, 0x80, 0x8D, 0xF0, 0x9F, 0x91, 0xA8, 0
   };
   LlmDriverScreenView view;
   LlmDriverFormatOptions options;
   LogicalCursor cursor;
   char out[16384];

   cursor = logical_cursor_make(LOGICAL_CURSOR_ZONE_FILEAREA, 8, 1,
                                textpos_from_cell_virtual(NULL, 0, 0,
                                                          TEXT_SNAP_BACKWARD));
   utf8_terminal_profile_reset();
   expect_int("utf.all.modifier.apply",
              utf8_terminal_profile_apply_line(
                 "SET UTF TERMINAL CLASS modifier WIDTH 2 ADVANCE 4 CURSOR 4 REPAINT 4"),
              UTF8_TERMINAL_PROFILE_APPLIED);
   expect_int("utf.all.short.sub.apply",
              utf8_terminal_profile_apply_line(
                 "SET UTF TERMINAL CLASS short-zwj DISPLAY grouped OUTPUT substitute U+25A1"),
              UTF8_TERMINAL_PROFILE_APPLIED);
   llm_driver_screen_view_init(&view, 3, 80, cursor);
   llm_driver_screen_view_set_row(&view, 0, UI_ROW_FILE, 8, 1, 0,
                                  "000008", (const char *)sample, 1, 1);

   llm_driver_format_options_init(&options);
   options.include_all_utf = 1;
   options.include_prefix = 0;
   options.include_command = 0;
   options.include_status = 0;
   llm_driver_format_semantic_view_with_options(&view, &options,
                                                out, sizeof(out));

   expect_contains("utf.all.ascii.class", out, "\"class\": \"ascii\"");
   expect_contains("utf.all.ascii.codepoints", out,
                   "\"codepoints\": \"U+41\"");
   expect_contains("utf.all.combining.class", out,
                   "\"class\": \"combining\"");
   expect_contains("utf.all.combining.codepoints", out,
                   "\"codepoints\": \"U+65 U+301\"");
   expect_contains("utf.all.wide.class", out, "\"class\": \"wide\"");
   expect_contains("utf.all.wide.width", out, "\"logical_width\": 2");
   expect_contains("utf.all.keycap.class", out, "\"class\": \"keycap\"");
   expect_contains("utf.all.flag.class", out,
                   "\"class\": \"regional-flag\"");
   expect_contains("utf.all.modifier.class", out, "\"class\": \"modifier\"");
   expect_contains("utf.all.modifier.advance", out,
                   "\"advance_width\": 4");
   expect_contains("utf.all.modifier.cursor", out,
                   "\"cursor_width\": 4");
   expect_contains("utf.all.modifier.repaint", out,
                   "\"repaint_width\": 4");
   expect_contains("utf.all.short.zwj.substitute", out,
                   "\"class\": \"short-zwj\", \"output\": \"substitute\"");

   utf8_terminal_profile_reset();
   expect_int("utf.all.heart.components.profile",
              utf8_terminal_profile_apply_line(
                 "SET UTF TERMINAL CLASS heart-zwj DISPLAY components OUTPUT components"),
              UTF8_TERMINAL_PROFILE_APPLIED);
   expect_int("utf.all.components.display",
              utf8_terminal_profile_apply_line("SET UTF DISPLAY components"),
              UTF8_TERMINAL_PROFILE_APPLIED);
   llm_driver_screen_view_set_row(&view, 0, UI_ROW_FILE, 9, 1, 0,
                                  "000009", (const char *)heart_zwj, 1, 1);
   llm_driver_format_semantic_view_with_options(&view, &options,
                                                out, sizeof(out));
   expect_contains("utf.all.heart.components", out,
                   "\"class\": \"heart-zwj\", \"output\": \"components\"");
   expect_contains("utf.all.heart.codepoints", out,
                   "U+1F469 U+200D U+2764 U+FE0F U+200D U+1F468");

   utf8_terminal_profile_reset();
#endif
}

static void test_input_mapping(void)
{
   LlmDriverInput input;
   LlmDriverInputQueue queue;
   int key = 0;

   expect_int("input.left.parse", llm_driver_input_from_key_name("left", &input), 1);
   expect_int("input.left.kind", input.kind, LLM_DRIVER_INPUT_KEY);
   expect_int("input.left.key", input.key_code, THE_KEY_LEFT);

   expect_int("input.f5.parse", llm_driver_input_from_key_name("f5", &input), 1);
   expect_int("input.f5.key", input.key_code, THE_KEY_F(5));

   expect_int("input.text.parse", llm_driver_input_from_text('x', &input), 1);
   expect_int("input.text.key", input.key_code, 'x');
   expect_int("input.legacy.parse",
              llm_driver_input_from_legacy_key(THE_KEY_LEFT, &input), 1);
   expect_int("input.legacy.kind", input.kind, LLM_DRIVER_INPUT_KEY);
   expect_int("input.legacy.key", input.key_code, THE_KEY_LEFT);

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
   expect_int("queue.pop.right.key", key, THE_KEY_RIGHT);
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
   test_compact_filearea_view_options();
   test_reserved_view_options();
   test_agent_metadata_and_delta();
   test_utf_physical_metadata();
   test_utf_all_metadata_representative_clusters();
   test_input_mapping();
   test_debug_snapshot_format();

   if (failures != 0)
   {
      fprintf(stderr, "LLM driver tests failed: %d\n", failures);
      return 1;
   }
   return 0;
}
