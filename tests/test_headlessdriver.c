#include <stdio.h>
#include <string.h>

#include "driverlayout.h"
#include "headlessdriver.h"
#include "utfterm.h"

static int failures = 0;

static void expect_int(const char *name, int got, int want)
{
   if (got != want)
   {
      fprintf(stderr, "%s: got %d want %d\n", name, got, want);
      failures++;
   }
}

static void expect_long(const char *name, long got, long want)
{
   if (got != want)
   {
      fprintf(stderr, "%s: got %ld want %ld\n", name, got, want);
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

static void expect_ptr(const char *name, const void *got, const void *want)
{
   if (got != want)
   {
      fprintf(stderr, "%s: got %p want %p\n", name, got, want);
      failures++;
   }
}

static void expect_nonnull(const char *name, const void *got)
{
   if (got == NULL)
   {
      fprintf(stderr, "%s: got NULL\n", name);
      failures++;
   }
}

static void expect_str(const char *name, const char *got, const char *want)
{
   if (got == NULL || strcmp(got, want) != 0)
   {
      fprintf(stderr, "%s: got %s want %s\n", name,
              got == NULL ? "(null)" : got, want);
      failures++;
   }
}

static TextCluster test_cluster_at_begin(const CHARTYPE *line, size_t len)
{
   return textpos_cluster_at_boundary(line, len, textpos_begin());
}

static void test_vtable_complete(void)
{
   size_t count;
   size_t i;
   const void *const *ops;

   expect_long("ops.size.remainder",
               (long)(sizeof(TheDriverOps) % sizeof(void (*)(void))), 0);
   count = sizeof(TheDriverOps) / sizeof(void (*)(void));
   expect_long("ops.count", (long)count, 138);
   ops = (const void *const *)(const void *)&the_headless_driver_ops;
   for (i = 0; i < count; i++)
   {
      if (ops[i] == NULL)
      {
         fprintf(stderr, "ops[%zu] is NULL\n", i);
         failures++;
      }
   }
}

static void test_shared_display_layout(void)
{
   const CHARTYPE ascii[] = "abcdef";
   LogicalCursor cursor;
   TheDriverCursorTarget target;
   int display_col = -1;
   int visible = -1;

   expect_int("layout.clamp.low",
              driver_layout_clamp_display_col(-2, 4), 0);
   expect_int("layout.clamp.high",
              driver_layout_clamp_display_col(8, 4), 3);
   expect_int("layout.ascii.logical.to.display",
              driver_layout_display_col_from_logical(ascii, 6, 2, 5), 3);
   expect_int("layout.ascii.display.to.logical",
              driver_layout_logical_col_from_display(ascii, 6, 2, 3,
                                                     TEXT_SNAP_BACKWARD), 5);
   expect_int("layout.ascii.viewport",
              driver_layout_viewport_col_for_logical(ascii, 6, 0, 12, 6,
                                                     &display_col,
                                                     &visible), 10);
   expect_int("layout.ascii.viewport.display", display_col, 2);
   expect_int("layout.ascii.viewport.visible", visible, 1);

   cursor = logical_cursor_from_cell(LOGICAL_CURSOR_ZONE_FILEAREA, 7, 1,
                                     ascii, 6, 5, TEXT_SNAP_BACKWARD, 1);
   target = driver_layout_filearea_target(cursor, ascii, 6, 2, 4);
   expect_int("layout.target.raw", target.raw_display_col, 3);
   expect_int("layout.target.display", target.display_col, 3);
   expect_int("layout.target.visible", target.visible, 1);

#ifdef USE_UTF8
   {
      static const CHARTYPE keycap[] = {
         'A', '1', 0xEF, 0xB8, 0x8F, 0xE2, 0x83, 0xA3, 'B'
      };

      utf8_terminal_profile_reset();
      utf8_terminal_profile_apply_line(
         "SET UTF TERMINAL CLASS keycap LAYOUT 2 CURSOR 2");
      expect_int("layout.utf.logical.to.display",
                 driver_layout_display_col_from_logical(keycap,
                                                        sizeof(keycap),
                                                        0, 2), 3);
      expect_int("layout.utf.display.to.logical.backward",
                 driver_layout_logical_col_from_display(keycap,
                                                        sizeof(keycap),
                                                        0, 2,
                                                        TEXT_SNAP_BACKWARD),
                 1);
      expect_int("layout.utf.display.to.logical.forward",
                 driver_layout_logical_col_from_display(keycap,
                                                        sizeof(keycap),
                                                        0, 2,
                                                        TEXT_SNAP_FORWARD),
                 2);
      cursor = logical_cursor_from_cell(LOGICAL_CURSOR_ZONE_FILEAREA, 8, 0,
                                        keycap, sizeof(keycap), 2,
                                        TEXT_SNAP_BACKWARD, 1);
      target = driver_layout_filearea_target(cursor, keycap, sizeof(keycap),
                                             0, 4);
      expect_int("layout.utf.target.raw", target.raw_display_col, 3);
      expect_int("layout.utf.target.display", target.display_col, 3);
      expect_int("layout.utf.target.visible", target.visible, 1);
      utf8_terminal_profile_reset();
   }
#endif
}

static void test_render_cell_model(void)
{
   TheRenderCell ascii;
   wchar_t wch[16];

   expect_int("render.ascii.make",
              the_render_cell_from_codepoint(&ascii, 'A', 7), 1);
   expect_size("render.ascii.cp.count", ascii.codepoint_count, 1);
   expect_int("render.ascii.cp", (int)ascii.codepoints[0], 'A');
   expect_int("render.ascii.attr", (int)ascii.attr, 7);
   expect_int("render.ascii.logical", ascii.logical_width, 1);
   expect_int("render.ascii.display", ascii.display_width, 1);
   expect_int("render.ascii.cursor", ascii.cursor_width, 1);
   expect_int("render.ascii.paint", ascii.paint_width, 1);
   expect_int("render.ascii.wchars",
              the_render_cluster_to_wchars(&ascii, wch,
                                           sizeof(wch) / sizeof(wch[0])), 1);
   expect_int("render.ascii.wchar", (int)wch[0], 'A');

#ifdef USE_UTF8
   {
      static const CHARTYPE combining[] = { 'e', 0xCC, 0x81 };
      static const CHARTYPE keycap[] = {
         '1', 0xEF, 0xB8, 0x8F, 0xE2, 0x83, 0xA3
      };
      static const CHARTYPE flag[] = {
         0xF0, 0x9F, 0x87, 0xBA, 0xF0, 0x9F, 0x87, 0xB8
      };
      static const CHARTYPE zwj[] = {
         0xF0, 0x9F, 0x91, 0xA9, 0xE2, 0x80, 0x8D,
         0xE2, 0x9D, 0xA4, 0xEF, 0xB8, 0x8F, 0xE2, 0x80,
         0x8D, 0xF0, 0x9F, 0x91, 0xA8
      };
      TheRenderCluster render;

      utf8_terminal_profile_reset();
      expect_int("render.combining.make",
                 the_render_cluster_from_text_cluster(
                    &render, combining, sizeof(combining),
                    test_cluster_at_begin(combining, sizeof(combining)),
                    11, 0), 1);
      expect_size("render.combining.cp.count", render.codepoint_count, 2);
      expect_int("render.combining.cp0", (int)render.codepoints[0], 'e');
      expect_int("render.combining.cp1", (int)render.codepoints[1], 0x0301);
      expect_int("render.combining.logical", render.logical_width, 1);
      expect_int("render.combining.display", render.display_width, 1);
      expect_int("render.combining.cursor", render.cursor_width, 1);
      expect_int("render.combining.paint", render.paint_width, 1);

      utf8_terminal_profile_reset();
      expect_int("render.keycap.make",
                 the_render_cluster_from_text_cluster(
                    &render, keycap, sizeof(keycap),
                    test_cluster_at_begin(keycap, sizeof(keycap)),
                    12, 0), 1);
      expect_size("render.keycap.cp.count", render.codepoint_count, 3);
      expect_int("render.keycap.display", render.display_width, 2);
      expect_int("render.keycap.cursor", render.cursor_width, 2);
      expect_int("render.keycap.paint", render.paint_width, 2);
      expect_int("render.keycap.repair", render.repair_strategy,
                 UTF8_TERM_STRATEGY_CLEAR_WHOLE_FAST);

      utf8_terminal_profile_reset();
      expect_int("render.flag.make",
                 the_render_cluster_from_text_cluster(
                    &render, flag, sizeof(flag),
                    test_cluster_at_begin(flag, sizeof(flag)),
                    13, 0), 1);
      expect_size("render.flag.cp.count", render.codepoint_count, 2);
      expect_int("render.flag.display", render.display_width, 3);
      expect_int("render.flag.cursor", render.cursor_width, 3);
      expect_int("render.flag.paint", render.paint_width, 3);
      expect_int("render.flag.repair", render.repair_strategy,
                 UTF8_TERM_STRATEGY_CLEAR_CHANGED_SUFFIX_FAST);

      utf8_terminal_profile_reset();
      utf8_terminal_set_display_mode(UTF8_TERM_DISPLAY_COMPONENTS);
      expect_int("render.zwj.make",
                 the_render_cluster_from_text_cluster(
                    &render, zwj, sizeof(zwj),
                    test_cluster_at_begin(zwj, sizeof(zwj)),
                    14, 0), 1);
      expect_size("render.zwj.cp.count", render.codepoint_count, 6);
      expect_int("render.zwj.expanded",
                 (render.flags & THE_RENDER_CLUSTER_EXPANDED) != 0, 1);
      expect_int("render.zwj.display", render.display_width, 6);
      expect_int("render.zwj.cursor", render.cursor_width, 6);
      expect_int("render.zwj.paint", render.paint_width, 6);
      utf8_terminal_profile_reset();
   }
#endif
}

static void test_selection(void)
{
   expect_ptr("selection.initial", the_driver, NULL);
   expect_int("selection.curses.unlinked", the_driver_use_curses(), 0);
   expect_int("selection.headless", the_driver_use_headless(), 1);
   expect_ptr("selection.current", the_driver, &the_headless_driver_ops);
   the_driver_select(NULL);
   expect_ptr("selection.clear", the_driver, NULL);
   the_driver_select(&the_headless_driver_ops);
   expect_ptr("selection.explicit", the_driver, &the_headless_driver_ops);
}

static void test_fake_window_and_cursor_state(void)
{
   const TheDriverOps *ops = &the_headless_driver_ops;
   TheDriverWindow *win;
   TheDriverWindow *explicit_win;
   TheDriverWindow *global;
   TheDriverWindowSize size;
   TheDriverWindowOrigin origin;
   TheDriverWindowCursor cursor;
   TheDriverWindowCursor saved;
   TheDriverScreenPoint point;

   headless_driver_reset();
   headless_driver_set_current_screen(0);
   headless_driver_set_screen_current_role(0, 0);
   win = headless_driver_create_screen_role(0, 0, 3, 4, 5, 6);
   expect_nonnull("role.win", win);
   expect_int("role.current.exists", ops->current_window_exists(), 1);
   expect_int("role.current.is.filearea", ops->current_window_is_role(0), 1);
   expect_int("role.screen.exists", ops->screen_role_exists(0, 0), 1);
   expect_int("role.screen.is.filearea", ops->screen_window_is_role(0, 0), 1);

   size = ops->current_window_size();
   expect_int("role.size.valid", size.valid, 1);
   expect_int("role.size.rows", size.rows, 3);
   expect_int("role.size.cols", size.cols, 4);
   origin = ops->current_window_origin();
   expect_int("role.origin.valid", origin.valid, 1);
   expect_int("role.origin.row", origin.row, 5);
   expect_int("role.origin.col", origin.col, 6);

   ops->move_current_window_cursor(1, 2);
   cursor = ops->capture_current_window_cursor();
   expect_int("role.cursor.valid", cursor.valid, 1);
   expect_int("role.cursor.row", cursor.row, 1);
   expect_int("role.cursor.col", cursor.col, 2);
   point = ops->current_window_cursor_screen_point();
   expect_int("role.point.valid", point.valid, 1);
   expect_int("role.point.row", point.row, 6);
   expect_int("role.point.col", point.col, 8);

   saved = cursor;
   ops->move_current_window_cursor(2, 3);
   ops->restore_current_window_cursor(saved);
   cursor = ops->capture_current_window_cursor();
   expect_int("role.restore.row", cursor.row, 1);
   expect_int("role.restore.col", cursor.col, 2);

   ops->add_cell_at(win, 0, 1, 'X');
   ops->move_window_cursor(win, 0, 1);
   expect_int("role.read.cell", (int)ops->read_window_cell(win), 'X');

   explicit_win = ops->create_window(2, 5, 4, 7);
   expect_nonnull("explicit.win", explicit_win);
   size = ops->window_size(explicit_win);
   origin = ops->window_origin(explicit_win);
   expect_int("explicit.rows", size.rows, 2);
   expect_int("explicit.cols", size.cols, 5);
   expect_int("explicit.origin.row", origin.row, 4);
   expect_int("explicit.origin.col", origin.col, 7);
   ops->delete_window(explicit_win);

   global = headless_driver_create_global_window(THE_DRIVER_GLOBAL_ERROR, 2,
                                                 10, 20, 30);
   expect_nonnull("global.win", global);
   expect_int("global.exists",
              ops->global_window_exists(THE_DRIVER_GLOBAL_ERROR), 1);
   ops->move_global_window_cursor(THE_DRIVER_GLOBAL_ERROR, 1, 4);
   cursor = ops->capture_global_window_cursor(THE_DRIVER_GLOBAL_ERROR);
   expect_int("global.cursor.row", cursor.row, 1);
   expect_int("global.cursor.col", cursor.col, 4);
   ops->delete_global_window(THE_DRIVER_GLOBAL_ERROR);
   expect_int("global.deleted",
              ops->global_window_exists(THE_DRIVER_GLOBAL_ERROR), 0);
}

static void test_headless_render_preserves_clusters(void)
{
   const TheDriverOps *ops = &the_headless_driver_ops;
   TheDriverWindow *win;
   TheRenderCell emoji;
   TheRenderCell stored;

   headless_driver_reset();
   win = headless_driver_create_screen_role(0, 0, 2, 12, 0, 0);
   expect_nonnull("render.headless.win", win);
   ops->move_window_cursor(win, 0, 0);
   the_render_cell_from_codepoint(&emoji, 0x1F600u, 21);
   ops->write_render_cells(win, &emoji, 1);
   ops->move_window_cursor(win, 0, 0);
   expect_int("render.headless.read.cell",
              (int)ops->read_window_cell(win), 0x1F600);
   expect_int("render.headless.inspect",
              headless_driver_render_cell_at(win, 0, 0, &stored), 1);
   expect_size("render.headless.inspect.count", stored.codepoint_count, 1);
   expect_int("render.headless.inspect.cp",
              (int)stored.codepoints[0], 0x1F600);
   expect_int("render.headless.inspect.attr", (int)stored.attr, 21);

#ifdef USE_UTF8
   {
      static const CHARTYPE keycap[] = {
         '1', 0xEF, 0xB8, 0x8F, 0xE2, 0x83, 0xA3
      };
      TheRenderCluster cluster;

      utf8_terminal_profile_reset();
      expect_int("render.headless.cluster.make",
                 the_render_cluster_from_text_cluster(
                    &cluster, keycap, sizeof(keycap),
                    test_cluster_at_begin(keycap, sizeof(keycap)),
                    22, 0), 1);
      headless_driver_clear_log();
      ops->write_render_cluster_at(win, 1, 2, &cluster);
      expect_int("render.headless.cluster.inspect",
                 headless_driver_render_cell_at(win, 1, 2, &stored), 1);
      expect_size("render.headless.cluster.count", stored.codepoint_count, 3);
      expect_int("render.headless.cluster.display", stored.display_width, 2);
      expect_int("render.headless.cluster.cursor", stored.cursor_width, 2);
      expect_int("render.headless.cluster.paint", stored.paint_width, 2);
      expect_long("render.headless.cluster.log.count",
                  (long)headless_driver_log_count(), 1);
      expect_str("render.headless.cluster.log",
                 headless_driver_log_entry(0),
                 "render-cluster:window:1:1:2:3:1:2:2:2");
      utf8_terminal_profile_reset();
   }
#endif
}

static void test_operation_log(void)
{
   const TheDriverOps *ops = &the_headless_driver_ops;
   TheDriverWindow *win;

   headless_driver_reset();
   headless_driver_set_current_screen(0);
   headless_driver_set_screen_current_role(0, 0);
   win = headless_driver_create_screen_role(0, 0, 3, 4, 1, 2);
   expect_nonnull("log.win", win);
   headless_driver_create_global_window(THE_DRIVER_GLOBAL_STATAREA, 1, 10,
                                        0, 0);
   headless_driver_clear_log();

   ops->touch_window(win);
   ops->refresh_window(win);
   ops->update();
   ops->touch_and_refresh_current_role(0);
   ops->touch_global_window(THE_DRIVER_GLOBAL_STATAREA);
   ops->refresh_global_window(THE_DRIVER_GLOBAL_STATAREA);

   expect_long("log.count", (long)headless_driver_log_count(), 6);
   expect_str("log.0", headless_driver_log_entry(0), "touch:window:1");
   expect_str("log.1", headless_driver_log_entry(1), "refresh:window:1");
   expect_str("log.2", headless_driver_log_entry(2), "update");
   expect_str("log.3", headless_driver_log_entry(3),
              "touch-refresh:current-role:0");
   expect_str("log.4", headless_driver_log_entry(4), "touch:global:0");
   expect_str("log.5", headless_driver_log_entry(5), "refresh:global:0");
}

static void test_input_and_mouse_fakes(void)
{
   const TheDriverOps *ops = &the_headless_driver_ops;
   TheDriverMouseEvent event;
   TheInputEvent input;
   int row = -1;
   int col = -1;
   int button = 0;
   int action = 0;
   int modifier = 0;

   headless_driver_reset();
   headless_driver_set_current_screen(0);
   headless_driver_set_screen_current_role(0, 0);
   headless_driver_create_screen_role(0, 0, 3, 4, 5, 6);

   headless_driver_queue_key('T');
   expect_int("input.event.read", ops->read_input_event(&input), 1);
   expect_int("input.event.kind", input.kind, THE_INPUT_TEXT);
   expect_int("input.event.key", input.key_code, 'T');
   expect_int("input.event.empty", ops->read_input_event(&input), 0);

   expect_int("input.event.logical.make",
              the_input_event_from_logical_target(THE_INPUT_TARGET_FILEAREA,
                                                  10, 1, 2, 0, 0,
                                                  &input), 1);
   expect_int("input.event.logical.queue",
              headless_driver_queue_input_event(input), 1);
   input = the_input_event_none();
   expect_int("input.event.logical.read", ops->read_input_event(&input), 1);
   expect_int("input.event.logical.kind", input.kind, THE_INPUT_LOGICAL_HIT);
   expect_int("input.event.logical.cell", input.target.cell, 2);

   headless_driver_queue_key('A');
   headless_driver_queue_key(ops->mouse_key_code());
   expect_int("input.key", ops->read_current_window_key(), 'A');
   expect_int("input.mouse.key", ops->is_mouse_key(ops->read_standard_key()),
              1);
   expect_int("input.empty", ops->read_standard_key(), -1);

   headless_driver_set_mouse_position(6, 8);
   ops->mouse_position_for_screen_role(0, 0, &row, &col);
   expect_int("mouse.role.row", row, 1);
   expect_int("mouse.role.col", col, 2);
   ops->saved_mouse_position(&row, &col);
   expect_int("mouse.saved.row", row, 6);
   expect_int("mouse.saved.col", col, 8);
   ops->reset_mouse_position();
   ops->saved_mouse_position(&row, &col);
   expect_int("mouse.reset.row", row, -1);
   expect_int("mouse.reset.col", col, -1);

   headless_driver_set_mouse_position(6, 8);
   headless_driver_set_mouse_button(THE_DRIVER_MOUSE_BUTTON_PRESSED,
                                    THE_DRIVER_MOUSE_ACTION_PRESSED,
                                    THE_DRIVER_MOUSE_MODIFIER_SHIFT);
   expect_int("mouse.button.read",
              ops->read_mouse_button(&button, &action, &modifier), 1);
   expect_int("mouse.button", button, THE_DRIVER_MOUSE_BUTTON_PRESSED);
   expect_int("mouse.action", action, THE_DRIVER_MOUSE_ACTION_PRESSED);
   expect_int("mouse.modifier", modifier, THE_DRIVER_MOUSE_MODIFIER_SHIFT);
   expect_int("mouse.event.read",
              ops->read_current_role_mouse_event(0, &event), 1);
   expect_int("mouse.event.row", event.row, 1);
   expect_int("mouse.event.col", event.col, 2);
   expect_int("mouse.event.inside", event.inside, 1);
}

int main(void)
{
   test_vtable_complete();
   test_selection();
   test_shared_display_layout();
   test_render_cell_model();
   test_fake_window_and_cursor_state();
   test_headless_render_preserves_clusters();
   test_operation_log();
   test_input_and_mouse_fakes();
   headless_driver_reset();

   if (failures != 0)
   {
      fprintf(stderr, "headless driver tests failed: %d\n", failures);
      return 1;
   }
   return 0;
}
