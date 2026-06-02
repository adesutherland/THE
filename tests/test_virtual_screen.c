#include <stdio.h>
#include <string.h>

#include "llmdriver.h"
#ifdef USE_UTF8
# include "utfterm.h"
#endif

enum
{
   VROW_TABLINE = 0,
   VROW_ALPHA,
   VROW_KEYCAP,
   VROW_ZWJ,
   VROW_DIVIDER,
   VROW_WINDOW,
   VROW_COMMAND,
   VROW_STATUS,
   VROW_COUNT
};

static int failures = 0;

static const CHARTYPE keycap_row[] =
{
   'A', '1', 0xef, 0xb8, 0x8f, 0xe2, 0x83, 0xa3,
   'B', ' ', 'k', 'e', 'y', 'c', 'a', 'p', 0
};

static const CHARTYPE keycap_space_row[] =
{
   'A', '1', 0xef, 0xb8, 0x8f, 0xe2, 0x83, 0xa3,
   'B', '#', 0xef, 0xb8, 0x8f, 0xe2, 0x83, 0xa3,
   'C', ' ', ' '
};

static const CHARTYPE zwj_row[] =
{
   'A', 0xf0, 0x9f, 0x91, 0xa9, 0xe2, 0x80, 0x8d,
   0xe2, 0x9d, 0xa4, 0xef, 0xb8, 0x8f, 0xe2, 0x80,
   0x8d, 0xf0, 0x9f, 0x91, 0xa8, 'B', ' ', 'z', 'w',
   'j', 0
};

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

static void expect_contains(const char *name, const char *haystack,
                            const char *needle)
{
   if (strstr(haystack, needle) == NULL)
   {
      fprintf(stderr, "%s: missing \"%s\" in:\n%s\n", name, needle, haystack);
      failures++;
   }
}

static void expect_text_slice(const char *name, const CHARTYPE *got,
                              size_t got_len, const char *want)
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

static void expect_not_contains(const char *name, const char *haystack,
                                const char *needle)
{
   if (strstr(haystack, needle) != NULL)
   {
      fprintf(stderr, "%s: unexpected \"%s\" in:\n%s\n", name, needle, haystack);
      failures++;
   }
}

static LogicalCursor virtual_cursor(LogicalCursorZone zone, LINETYPE line,
                                    int row, int cell)
{
   return logical_cursor_make(zone, line, row,
                              textpos_from_cell_virtual(NULL, 0, cell,
                                                        TEXT_SNAP_BACKWARD));
}

static LogicalCursor virtual_line_cursor(LogicalCursorZone zone, LINETYPE line_no,
                                         int row, const CHARTYPE *line,
                                         size_t len, int cell)
{
   return logical_cursor_from_cell(zone, line_no, row, line, len, cell,
                                   TEXT_SNAP_BACKWARD, 1);
}

static int build_virtual_frame(UiFrame *frame, LogicalCursor cursor)
{
   int ok = 1;
   const char *tabline = "[0] main.the | [1] utf-render.txt";
   const char *alpha = "alpha = 1";
   const char *divider = "|| split divider ||";
   const char *window = "window 1 active";
   const char *command = "====> next";
   const char *status = "LINE 2 COL 3";

   ui_frame_init(frame, VROW_COUNT, 96);
   ok = ok && ui_frame_set_row(frame, VROW_TABLINE, UI_ROW_TABLINE, 0,
                               VROW_TABLINE, 0, (const CHARTYPE *)tabline,
                               strlen(tabline), 0);
   ok = ok && ui_frame_set_row(frame, VROW_ALPHA, UI_ROW_FILE, 1,
                               VROW_ALPHA, 0, (const CHARTYPE *)alpha,
                               strlen(alpha), 1);
   ok = ok && ui_frame_set_row_prefix(frame, VROW_ALPHA,
                                      (const CHARTYPE *)"000001", 6, 1);
   ok = ok && ui_frame_add_row_style(frame, VROW_ALPHA, 0, 5,
                                     UI_SYNTAX_IDENTIFIER);
   ok = ok && ui_frame_add_row_style(frame, VROW_ALPHA, 8, 1,
                                     UI_SYNTAX_NUMBER);

   ok = ok && ui_frame_set_row(frame, VROW_KEYCAP, UI_ROW_FILE, 2,
                               VROW_KEYCAP, 0, keycap_row,
                               sizeof(keycap_row) - 1, 1);
   ok = ok && ui_frame_set_row_prefix(frame, VROW_KEYCAP,
                                      (const CHARTYPE *)"PEND  ", 6, 1);
   ok = ok && ui_frame_add_row_style(frame, VROW_KEYCAP, 0, 2,
                                     UI_SYNTAX_CONSTANT);

   ok = ok && ui_frame_set_row(frame, VROW_ZWJ, UI_ROW_FILE, 3,
                               VROW_ZWJ, 0, zwj_row, sizeof(zwj_row) - 1, 1);
   ok = ok && ui_frame_set_row_prefix(frame, VROW_ZWJ,
                                      (const CHARTYPE *)"000003", 6, 1);

   ok = ok && ui_frame_set_row(frame, VROW_DIVIDER, UI_ROW_DIVIDER, 0,
                               VROW_DIVIDER, 0, (const CHARTYPE *)divider,
                               strlen(divider), 0);
   ok = ok && ui_frame_set_row(frame, VROW_WINDOW, UI_ROW_WINDOW, 0,
                               VROW_WINDOW, 0, (const CHARTYPE *)window,
                               strlen(window), 0);
   ok = ok && ui_frame_set_row(frame, VROW_COMMAND, UI_ROW_COMMAND, 0,
                               VROW_COMMAND, 0, (const CHARTYPE *)command,
                               strlen(command), 1);
   ok = ok && ui_frame_set_row(frame, VROW_STATUS, UI_ROW_STATUS, 0,
                               VROW_STATUS, 0, (const CHARTYPE *)status,
                               strlen(status), 0);
   if (ok && cursor.valid)
      ok = ui_frame_set_cursor(frame, cursor);
   return ok;
}

static int view_from_frame(const UiFrame *frame, LlmDriverScreenView *view)
{
   int ok;

   ok = llm_driver_screen_view_from_frame(frame, view);
   llm_driver_screen_view_set_command(view, "====> next");
   llm_driver_screen_view_set_status(view, "LINE 2 COL 3");
   return ok;
}

static void test_virtual_frame_semantic_rows(void)
{
   UiFrame frame;
   LlmDriverScreenView view;
   LogicalCursor cursor;
   char out[8192];

#ifdef USE_UTF8
   utf8_terminal_profile_reset();
   expect_int("virtual.utf.output.apply",
              utf8_terminal_profile_apply_line(
                 "SET UTF TERMINAL CLASS keycap OUTPUT base"),
              UTF8_TERMINAL_PROFILE_APPLIED);
   expect_int("virtual.utf.mark.apply",
              utf8_terminal_profile_apply_line(
                 "SET UTF TERMINAL CLASS keycap MARK compressed"),
              UTF8_TERMINAL_PROFILE_APPLIED);
#endif

   cursor = virtual_cursor(LOGICAL_CURSOR_ZONE_FILEAREA, 2, VROW_KEYCAP, 2);
   expect_int("virtual.build.file", build_virtual_frame(&frame, cursor), 1);
   expect_int("virtual.view.file", view_from_frame(&frame, &view), 1);

   expect_int("virtual.view.rows", (int)view.line_count, VROW_COUNT);
   expect_int("virtual.tab.role", view.lines[VROW_TABLINE].role, UI_ROW_TABLINE);
   expect_int("virtual.keycap.role", view.lines[VROW_KEYCAP].role, UI_ROW_FILE);
   expect_int("virtual.keycap.current", view.lines[VROW_KEYCAP].current, 1);
   expect_int("virtual.keycap.cursor", view.lines[VROW_KEYCAP].cursor, 1);
   expect_str("virtual.keycap.prefix", view.lines[VROW_KEYCAP].prefix, "PEND  ");
   expect_int("virtual.divider.role", view.lines[VROW_DIVIDER].role,
              UI_ROW_DIVIDER);
   expect_int("virtual.window.role", view.lines[VROW_WINDOW].role,
              UI_ROW_WINDOW);
   expect_str("virtual.command.row", view.lines[VROW_COMMAND].text, "====> next");
   expect_str("virtual.status.row", view.lines[VROW_STATUS].text,
              "LINE 2 COL 3");

   llm_driver_format_semantic_view(&view, out, sizeof(out));
   expect_contains("semantic.tabline", out, "\"role\": \"tabline\"");
   expect_contains("semantic.divider", out, "\"role\": \"divider\"");
   expect_contains("semantic.window", out, "\"role\": \"window\"");
   expect_contains("semantic.command", out, "\"role\": \"command\"");
   expect_contains("semantic.status", out, "\"role\": \"status\"");
   expect_contains("semantic.keycap", out, "keycap");
   expect_contains("semantic.zwj", out, "zwj");
   expect_contains("semantic.cursor", out, "\"cursor\": 1");
   expect_contains("semantic.focus.cell", out, "\"cell\": 2");
   expect_contains("semantic.utf.array", out, "\"utf\": [");
   expect_contains("semantic.utf.output", out, "\"output\": \"base\"");
   expect_contains("semantic.utf.mark", out, "\"mark\": \"compressed\"");
   expect_contains("semantic.utf.compressed", out, "\"compressed\": 1");
   expect_int("semantic.no.physical.width",
              strstr(out, "display_width") == NULL, 1);
   expect_contains("semantic.style.constant", out, "\"style\": \"constant\"");
#ifdef USE_UTF8
   utf8_terminal_profile_reset();
#endif
}

static void test_prefix_and_command_cursor_overlays(void)
{
   UiFrame frame;
   LlmDriverScreenView view;
   LlmDriverFormatOptions options;
   LogicalCursor cursor;
   char out[4096];

   cursor = virtual_cursor(LOGICAL_CURSOR_ZONE_PREFIX, 2, VROW_KEYCAP, 3);
   expect_int("virtual.build.prefix", build_virtual_frame(&frame, cursor), 1);
   expect_int("virtual.view.prefix", view_from_frame(&frame, &view), 1);
   llm_driver_format_options_init(&options);
   options.mode = LLM_DRIVER_VIEW_FOCUS;
   options.compact = 1;
   llm_driver_format_semantic_view_with_options(&view, &options,
                                                out, sizeof(out));
   expect_contains("prefix.focus.zone", out, "\"zone\":\"prefix\"");
   expect_contains("prefix.focus.cell", out, "\"cell\":3");
   expect_contains("prefix.focus.row", out, "\"role\":\"file\"");
   expect_contains("prefix.focus.text", out, "\"p\":\"PEND  \"");

   cursor = virtual_cursor(LOGICAL_CURSOR_ZONE_COMMAND, 0, VROW_COMMAND, 7);
   expect_int("virtual.build.command", build_virtual_frame(&frame, cursor), 1);
   expect_int("virtual.view.command", view_from_frame(&frame, &view), 1);
   llm_driver_format_semantic_view_with_options(&view, &options,
                                                out, sizeof(out));
   expect_contains("command.focus.zone", out, "\"zone\":\"command\"");
   expect_contains("command.focus.cell", out, "\"cell\":7");
   expect_contains("command.focus.row", out, "\"role\":\"command\"");
   expect_contains("command.focus.text", out, "\"t\":\"====> next\"");
}

static void test_frame_backed_renderer_cursor_targets(void)
{
   UiFrame frame;
   LogicalCursor cursor;
   LogicalCursor found;
   int screen_cell = -1;

   cursor = virtual_cursor(LOGICAL_CURSOR_ZONE_FILEAREA, 2, VROW_KEYCAP, 5);
   expect_int("renderer.file.build", build_virtual_frame(&frame, cursor), 1);
   expect_int("renderer.file.target",
              ui_frame_cursor_screen_cell(&frame, UI_ROW_FILE, 2,
                                          VROW_KEYCAP, 1, &screen_cell,
                                          &found), 1);
   expect_int("renderer.file.screen.cell", screen_cell, 4);
   expect_int("renderer.file.logical.cell", found.text.cell_column, 5);
   expect_int("renderer.file.no.prefix",
              ui_frame_cursor_screen_cell(&frame, UI_ROW_PREFIX, 2,
                                          VROW_KEYCAP, 0, &screen_cell,
                                          &found), 0);
   expect_int("renderer.file.no.status",
              ui_frame_cursor_screen_cell(&frame, UI_ROW_STATUS, 0,
                                          VROW_STATUS, 0, &screen_cell,
                                          &found), 0);
   expect_int("renderer.file.no.row",
              ui_frame_cursor_screen_cell(&frame, UI_ROW_FILE, 3,
                                          VROW_ZWJ, 1, &screen_cell,
                                          &found), 0);

   cursor = virtual_cursor(LOGICAL_CURSOR_ZONE_PREFIX, 2, VROW_KEYCAP, 3);
   expect_int("renderer.prefix.build", build_virtual_frame(&frame, cursor), 1);
   expect_int("renderer.prefix.target",
              ui_frame_cursor_screen_cell(&frame, UI_ROW_PREFIX, 2,
                                          VROW_KEYCAP, 0, &screen_cell,
                                          &found), 1);
   expect_int("renderer.prefix.screen.cell", screen_cell, 3);
   expect_int("renderer.prefix.no.file",
              ui_frame_cursor_screen_cell(&frame, UI_ROW_FILE, 2,
                                          VROW_KEYCAP, 0, &screen_cell,
                                          &found), 0);

   cursor = virtual_cursor(LOGICAL_CURSOR_ZONE_COMMAND, 0, VROW_COMMAND, 7);
   expect_int("renderer.command.build", build_virtual_frame(&frame, cursor), 1);
   expect_int("renderer.command.target",
              ui_frame_cursor_screen_cell(&frame, UI_ROW_COMMAND, 0,
                                          VROW_COMMAND, 0, &screen_cell,
                                          &found), 1);
   expect_int("renderer.command.screen.cell", screen_cell, 7);
   expect_int("renderer.command.no.status",
              ui_frame_cursor_screen_cell(&frame, UI_ROW_STATUS, 0,
                                          VROW_STATUS, 0, &screen_cell,
                                          &found), 0);

   expect_int("renderer.no.frame",
              ui_frame_cursor_screen_cell(NULL, UI_ROW_FILE, 2,
                                          VROW_KEYCAP, 0, &screen_cell,
                                          &found), 0);
}

static void test_frame_backed_targeted_redraw_rows(void)
{
   UiFrame frame;
   LogicalCursor cursor;
   LogicalCursor found;
   int screen_row = -1;

   cursor = virtual_cursor(LOGICAL_CURSOR_ZONE_PREFIX, 2, VROW_KEYCAP, 3);
   expect_int("redraw.prefix.build", build_virtual_frame(&frame, cursor), 1);
   expect_int("redraw.prefix.row",
              ui_frame_cursor_screen_row(&frame, UI_ROW_PREFIX, 2,
                                         &screen_row, &found), 1);
   expect_int("redraw.prefix.screen.row", screen_row, VROW_KEYCAP);
   expect_int("redraw.prefix.logical.cell", found.text.cell_column, 3);
   expect_int("redraw.prefix.no.physical.row",
              ui_frame_cursor_screen_row(&frame, UI_ROW_PREFIX, 1,
                                         &screen_row, &found), 0);
   expect_int("redraw.prefix.clears.row", screen_row, -1);

   cursor = virtual_cursor(LOGICAL_CURSOR_ZONE_FILEAREA, 2, VROW_KEYCAP, 5);
   expect_int("redraw.file.build", build_virtual_frame(&frame, cursor), 1);
   expect_int("redraw.file.row",
              ui_frame_cursor_screen_row(&frame, UI_ROW_FILE, 2,
                                         &screen_row, &found), 1);
   expect_int("redraw.file.screen.row", screen_row, VROW_KEYCAP);
   expect_int("redraw.file.no.prefix",
              ui_frame_cursor_screen_row(&frame, UI_ROW_PREFIX, 2,
                                         &screen_row, &found), 0);

   expect_int("redraw.no.frame",
              ui_frame_cursor_screen_row(NULL, UI_ROW_PREFIX, 2,
                                         &screen_row, &found), 0);
}

static void test_frame_backed_status_text_targets(void)
{
   UiFrame frame;
   LogicalCursor cursor;
   const CHARTYPE *text = NULL;
   size_t text_len = 0;
   int cell = -1;

   cursor = virtual_cursor(LOGICAL_CURSOR_ZONE_FILEAREA, 1, VROW_ALPHA, 2);
   expect_int("status.file.build", build_virtual_frame(&frame, cursor), 1);
   expect_int("status.file.target",
              ui_frame_cursor_text_target(&frame, &text, &text_len, &cell), 1);
   expect_text_slice("status.file.text", text, text_len, "alpha = 1");
   expect_int("status.file.cell", cell, 2);

   cursor = virtual_cursor(LOGICAL_CURSOR_ZONE_PREFIX, 2, VROW_KEYCAP, 3);
   expect_int("status.prefix.build", build_virtual_frame(&frame, cursor), 1);
   expect_int("status.prefix.target",
              ui_frame_cursor_text_target(&frame, &text, &text_len, &cell), 1);
   expect_text_slice("status.prefix.text", text, text_len, "PEND  ");
   expect_int("status.prefix.cell", cell, 3);

   cursor = virtual_cursor(LOGICAL_CURSOR_ZONE_COMMAND, 0, VROW_COMMAND, 7);
   expect_int("status.command.build", build_virtual_frame(&frame, cursor), 1);
   expect_int("status.command.target",
              ui_frame_cursor_text_target(&frame, &text, &text_len, &cell), 1);
   expect_text_slice("status.command.text", text, text_len, "====> next");
   expect_int("status.command.cell", cell, 7);

   expect_int("status.no.frame",
              ui_frame_cursor_text_target(NULL, &text, &text_len, &cell), 0);
   cursor = logical_cursor_invalid();
   expect_int("status.no.cursor.build", build_virtual_frame(&frame, cursor), 1);
   expect_int("status.no.cursor.target",
              ui_frame_cursor_text_target(&frame, &text, &text_len, &cell), 0);
}

static void test_logical_view_switch_cursor_restoration(void)
{
   UiFrame frame;
   UiDriverOpLog log;
   LogicalCursor saved;
   LogicalCursor rebased;
   LogicalCursor found;
   int screen_cell = -1;

   saved = virtual_cursor(LOGICAL_CURSOR_ZONE_FILEAREA, 2, VROW_KEYCAP, 5);
   expect_int("viewswitch.file.build",
              build_virtual_frame(&frame, logical_cursor_invalid()), 1);
   frame.row[VROW_ALPHA].screen_row = VROW_KEYCAP;
   frame.row[VROW_KEYCAP].screen_row = VROW_ALPHA;
   expect_int("viewswitch.file.stale.row",
              ui_frame_set_cursor(&frame, saved), 0);
   expect_int("viewswitch.file.rebase",
              ui_frame_rebase_cursor(&frame, saved, &rebased), 1);
   expect_int("viewswitch.file.rebased.row", rebased.zone_row, VROW_ALPHA);
   expect_int("viewswitch.file.restore",
              ui_frame_set_cursor_rebased(&frame, saved), 1);
   expect_int("viewswitch.file.restored.row",
              frame.cursor.cursor.zone_row, VROW_ALPHA);
   expect_int("viewswitch.file.target",
              ui_frame_cursor_screen_cell(&frame, UI_ROW_FILE, 2,
                                          VROW_ALPHA, 1, &screen_cell,
                                          &found), 1);
   expect_int("viewswitch.file.screen.cell", screen_cell, 4);
   ui_driver_op_log_init(&log);
   expect_int("viewswitch.file.materialize",
              ui_fake_driver_materialize(&frame, &log), 1);
   expect_int("viewswitch.file.cursor.row",
              log.op[VROW_COUNT].row, VROW_ALPHA);

   saved = virtual_cursor(LOGICAL_CURSOR_ZONE_PREFIX, 2, VROW_KEYCAP, 3);
   expect_int("viewswitch.prefix.build",
              build_virtual_frame(&frame, logical_cursor_invalid()), 1);
   frame.row[VROW_ALPHA].screen_row = VROW_KEYCAP;
   frame.row[VROW_KEYCAP].screen_row = VROW_ALPHA;
   expect_int("viewswitch.prefix.stale.row",
              ui_frame_set_cursor(&frame, saved), 0);
   expect_int("viewswitch.prefix.restore",
              ui_frame_set_cursor_rebased(&frame, saved), 1);
   expect_int("viewswitch.prefix.restored.row",
              frame.cursor.cursor.zone_row, VROW_ALPHA);
   expect_int("viewswitch.prefix.target",
              ui_frame_cursor_screen_cell(&frame, UI_ROW_PREFIX, 2,
                                          VROW_ALPHA, 0, &screen_cell,
                                          &found), 1);
   expect_int("viewswitch.prefix.screen.cell", screen_cell, 3);
   expect_int("viewswitch.prefix.old.row",
              ui_frame_cursor_screen_cell(&frame, UI_ROW_PREFIX, 2,
                                          VROW_KEYCAP, 0, &screen_cell,
                                          &found), 0);
}

static void test_compact_virtual_views(void)
{
   UiFrame frame;
   LlmDriverScreenView view;
   LlmDriverFormatOptions options;
   LogicalCursor cursor;
   char out[8192];

   cursor = virtual_cursor(LOGICAL_CURSOR_ZONE_FILEAREA, 2, VROW_KEYCAP, 2);
   expect_int("virtual.build.compact", build_virtual_frame(&frame, cursor), 1);
   expect_int("virtual.view.compact", view_from_frame(&frame, &view), 1);

   llm_driver_format_options_init(&options);
   options.mode = LLM_DRIVER_VIEW_FILEAREA;
   options.compact = 1;
   options.include_prefix = 0;
   options.include_command = 0;
   options.include_status = 0;
   options.max_text_cols = 2;
   llm_driver_format_semantic_view_with_options(&view, &options,
                                                out, sizeof(out));
   expect_contains("compact.filearea.mode", out, "\"mode\":\"filearea\"");
   expect_contains("compact.filearea.role", out, "\"role\":\"file\"");
   expect_contains("compact.filearea.keycap", out, "\"t\":\"A1...\"");
   expect_not_contains("compact.filearea.no.prefix", out, "\"p\"");
   expect_not_contains("compact.filearea.no.tabline", out, "tabline");
   expect_not_contains("compact.filearea.no.divider", out, "divider");
   expect_not_contains("compact.filearea.no.window", out, "window");
   expect_not_contains("compact.filearea.no.command", out, "====> next");

   llm_driver_format_options_init(&options);
   options.mode = LLM_DRIVER_VIEW_PREFIX;
   options.compact = 1;
   options.include_command = 0;
   options.include_status = 0;
   llm_driver_format_semantic_view_with_options(&view, &options,
                                                out, sizeof(out));
   expect_contains("compact.prefix.mode", out, "\"mode\":\"prefix\"");
   expect_contains("compact.prefix.pending", out, "\"p\":\"PEND  \"");
   expect_not_contains("compact.prefix.no.command", out, "\"role\":\"command\"");

   llm_driver_format_options_init(&options);
   options.mode = LLM_DRIVER_VIEW_RESERVED;
   options.compact = 1;
   options.include_cursor = 0;
   options.include_command = 0;
   options.include_status = 0;
   llm_driver_format_semantic_view_with_options(&view, &options,
                                                out, sizeof(out));
   expect_contains("compact.reserved.mode", out, "\"mode\":\"reserved\"");
   expect_contains("compact.reserved.tabline", out, "\"role\":\"tabline\"");
   expect_contains("compact.reserved.divider", out, "\"role\":\"divider\"");
   expect_contains("compact.reserved.window", out, "\"role\":\"window\"");
   expect_contains("compact.reserved.status", out, "\"role\":\"status\"");
   expect_not_contains("compact.reserved.no.file", out, "alpha = 1");
}

static void test_fake_driver_debug_log(void)
{
   UiFrame frame;
   UiDriverOpLog log;
   LlmDriverDebugSnapshot debug;
   LogicalCursor cursor;
   char out[8192];

   cursor = virtual_cursor(LOGICAL_CURSOR_ZONE_FILEAREA, 2, VROW_KEYCAP, 2);
   expect_int("virtual.build.fake", build_virtual_frame(&frame, cursor), 1);
   ui_driver_op_log_init(&log);
   expect_int("virtual.fake.materialize",
              ui_fake_driver_materialize(&frame, &log), 1);
   expect_int("virtual.fake.count", (int)log.count, VROW_COUNT + 2);
   expect_int("virtual.fake.tabline", log.op[VROW_TABLINE].role,
              UI_ROW_TABLINE);
   expect_int("virtual.fake.divider", log.op[VROW_DIVIDER].role,
              UI_ROW_DIVIDER);
   expect_int("virtual.fake.window", log.op[VROW_WINDOW].role,
              UI_ROW_WINDOW);
   expect_int("virtual.fake.cursor.kind", log.op[VROW_COUNT].kind,
              UI_DRIVER_OP_CURSOR);
   expect_int("virtual.fake.cursor.row", log.op[VROW_COUNT].row, VROW_KEYCAP);
   expect_int("virtual.fake.cursor.col", log.op[VROW_COUNT].col, 2);
   expect_int("virtual.fake.refresh", log.op[VROW_COUNT + 1].kind,
              UI_DRIVER_OP_REFRESH);

   llm_driver_debug_snapshot_init(&debug, cursor);
   llm_driver_debug_snapshot_set_cursor_mapping(&debug, 1, 2, 3, 3, 1);
   llm_driver_debug_snapshot_set_last_render(&debug,
                                             "virtual frame materialized");
   debug.driver_ops = log;
   llm_driver_format_debug_snapshot(&debug, out, sizeof(out));
   expect_contains("debug.driver.ops", out, "\"driver_ops\"");
   expect_contains("debug.driver.divider", out, "\"role\": \"divider\"");
   expect_contains("debug.driver.cursor", out, "\"kind\": \"cursor\"");
   expect_contains("debug.driver.refresh", out, "\"kind\": \"refresh\"");
   expect_contains("debug.last.render", out, "virtual frame materialized");
}

static void test_keycap_space_after_eol_demonstrator(void)
{
#ifdef USE_UTF8PROC
   static const int requested_cells[] = { 5, 6, 7, 8 };
   UiFrame frame;
   UiDriverOpLog log;
   TextPos end;
   size_t i;
   const size_t len = sizeof(keycap_space_row);

   end = textpos_from_byte(keycap_space_row, len, len);
   expect_int("keycap.demo.end.cell", end.cell_column, 7);

   for (i = 0; i < sizeof(requested_cells) / sizeof(requested_cells[0]); i++)
   {
      LogicalCursor cursor;
      LogicalCursor found;
      int screen_cell = -1;

      ui_frame_init(&frame, 1, 32);
      expect_int("keycap.demo.row",
                 ui_frame_set_row(&frame, 0, UI_ROW_FILE, 42, 0, 0,
                                  keycap_space_row, len, 1),
                 1);
      cursor = virtual_line_cursor(LOGICAL_CURSOR_ZONE_FILEAREA, 42, 0,
                                   keycap_space_row, len, requested_cells[i]);
      expect_int("keycap.demo.cursor.set",
                 ui_frame_set_cursor(&frame, cursor), 1);
      expect_int("keycap.demo.target",
                 ui_frame_cursor_screen_cell(&frame, UI_ROW_FILE, 42, 0, 0,
                                             &screen_cell, &found),
                 1);
      expect_int("keycap.demo.logical.cell",
                 found.text.cell_column, requested_cells[i]);
      expect_int("keycap.demo.screen.cell", screen_cell, requested_cells[i]);

      ui_driver_op_log_init(&log);
      expect_int("keycap.demo.materialize",
                 ui_fake_driver_materialize(&frame, &log), 1);
      expect_int("keycap.demo.op.count", (int)log.count, 3);
      expect_int("keycap.demo.op.row", log.op[0].kind, UI_DRIVER_OP_ROW);
      expect_int("keycap.demo.op.cursor", log.op[1].kind,
                 UI_DRIVER_OP_CURSOR);
      expect_int("keycap.demo.op.cursor.row", log.op[1].row, 0);
      expect_int("keycap.demo.op.cursor.col",
                 log.op[1].col, requested_cells[i]);
      expect_int("keycap.demo.op.refresh", log.op[2].kind,
                 UI_DRIVER_OP_REFRESH);
   }
#endif
}

static void test_logical_hit_targets(void)
{
   LlmDriverInput input;

   expect_int("hit.prefix.parse",
              llm_driver_input_from_logical_hit(LOGICAL_CURSOR_ZONE_PREFIX,
                                                2, VROW_KEYCAP, 3, &input), 1);
   expect_int("hit.prefix.kind", input.target.kind, LLM_DRIVER_TARGET_PREFIX);
   expect_str("hit.prefix.name",
              llm_driver_logical_target_kind_name(input.target.kind),
              "prefix");

   expect_int("hit.command.parse",
              llm_driver_input_from_logical_hit(LOGICAL_CURSOR_ZONE_COMMAND,
                                                0, VROW_COMMAND, 7, &input), 1);
   expect_int("hit.command.kind", input.target.kind, LLM_DRIVER_TARGET_COMMAND);

   expect_int("hit.status.parse",
              llm_driver_input_from_logical_target(LLM_DRIVER_TARGET_STATUS,
                                                   0, VROW_STATUS, 4, 0, -1,
                                                   &input), 1);
   expect_int("hit.status.zone", input.target.zone,
              LOGICAL_CURSOR_ZONE_STATUS);
   expect_str("hit.status.name",
              llm_driver_logical_target_kind_name(input.target.kind),
              "status");

   expect_int("hit.tabline.parse",
              llm_driver_input_from_logical_target(LLM_DRIVER_TARGET_TABLINE,
                                                   0, VROW_TABLINE, 5, 0, -1,
                                                   &input), 1);
   expect_int("hit.tabline.zone", input.target.zone, LOGICAL_CURSOR_ZONE_NONE);
   expect_str("hit.tabline.name",
              llm_driver_logical_target_kind_name(input.target.kind),
              "tabline");

   expect_int("hit.divider.parse",
              llm_driver_input_from_logical_target(LLM_DRIVER_TARGET_DIVIDER,
                                                   0, VROW_DIVIDER, 0, 0, -1,
                                                   &input), 1);
   expect_int("hit.divider.zone", input.target.zone, LOGICAL_CURSOR_ZONE_NONE);

   expect_int("hit.window.parse",
              llm_driver_input_from_logical_target(LLM_DRIVER_TARGET_WINDOW,
                                                   0, VROW_WINDOW, 0, 1, 0,
                                                   &input), 1);
   expect_int("hit.window.screen", input.target.screen, 1);
   expect_int("hit.window.id", input.target.window_id, 0);
   expect_str("hit.window.name",
              llm_driver_logical_target_kind_name(input.target.kind),
              "window");
}

int main(void)
{
   test_virtual_frame_semantic_rows();
   test_prefix_and_command_cursor_overlays();
   test_frame_backed_renderer_cursor_targets();
   test_frame_backed_targeted_redraw_rows();
   test_frame_backed_status_text_targets();
   test_logical_view_switch_cursor_restoration();
   test_compact_virtual_views();
   test_fake_driver_debug_log();
   test_keycap_space_after_eol_demonstrator();
   test_logical_hit_targets();

   if (failures != 0)
   {
      fprintf(stderr, "virtual screen tests failed: %d\n", failures);
      return 1;
   }
   return 0;
}
