#include <stdio.h>
#include <string.h>
#include <wchar.h>

#include "driverlayout.h"
#include "headlessdriver.h"
#include "the.h"
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
   expect_long("ops.count", (long)count, 54);
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
         "SET UTF TERMINAL CLASS keycap WIDTH 2 ADVANCE 2 CURSOR 2 REPAINT 2");
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
   expect_int("render.ascii.width", ascii.width, 1);
   expect_int("render.ascii.display", ascii.advance_width, 1);
   expect_int("render.ascii.cursor", ascii.cursor_width, 1);
   expect_int("render.ascii.repaint", ascii.repaint_width, 1);
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
      static const CHARTYPE text_heart[] = {
         0xE2, 0x99, 0xA5
      };
      static const CHARTYPE explicit_text_heart[] = {
         0xE2, 0x99, 0xA5, 0xEF, 0xB8, 0x8E
      };
      static const CHARTYPE emoji_heart[] = {
         0xE2, 0x99, 0xA5, 0xEF, 0xB8, 0x8F
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
      expect_int("render.combining.display", render.advance_width, 1);
      expect_int("render.combining.cursor", render.cursor_width, 1);
      expect_int("render.combining.repaint", render.repaint_width, 1);

      utf8_terminal_profile_reset();
      expect_int("render.keycap.make",
                 the_render_cluster_from_text_cluster(
                    &render, keycap, sizeof(keycap),
                    test_cluster_at_begin(keycap, sizeof(keycap)),
                    12, 0), 1);
      expect_size("render.keycap.cp.count", render.codepoint_count, 3);
      expect_int("render.keycap.display", render.advance_width, 2);
      expect_int("render.keycap.cursor", render.cursor_width, 2);
      expect_int("render.keycap.repaint", render.repaint_width, 2);
      expect_int("render.keycap.repair", render.repair_strategy,
                 UTF8_TERM_STRATEGY_CHANGED_CELLS);
      expect_int("render.keycap.output", render.output_method,
                 UTF8_TERM_OUTPUT_NATIVE);
      expect_int("render.keycap.mark", render.mark, UTF8_TERM_MARK_NONE);

      expect_int("render.keycap.base.output.apply",
                 utf8_terminal_profile_apply_line(
                    "SET UTF TERMINAL CLASS keycap OUTPUT base"),
                 UTF8_TERMINAL_PROFILE_APPLIED);
      expect_int("render.keycap.base.mark.apply",
                 utf8_terminal_profile_apply_line(
                    "SET UTF TERMINAL CLASS keycap MARK compressed"),
                 UTF8_TERMINAL_PROFILE_APPLIED);
      expect_int("render.keycap.base.advance.apply",
                 utf8_terminal_profile_apply_line(
                    "SET UTF TERMINAL CLASS keycap WIDTH 1 ADVANCE 1 CURSOR 1 REPAINT 3"),
                 UTF8_TERMINAL_PROFILE_APPLIED);
      expect_int("render.keycap.base.cursor.apply",
                 utf8_terminal_profile_apply_line(
                    "SET UTF TERMINAL CLASS keycap CURSORSTRATEGY cells"),
                 UTF8_TERMINAL_PROFILE_APPLIED);
      expect_int("render.keycap.base.replace.apply",
                 utf8_terminal_profile_apply_line(
                    "SET UTF TERMINAL CLASS keycap REPLACESTRATEGY cells"),
                 UTF8_TERMINAL_PROFILE_APPLIED);
      expect_int("render.keycap.base.make",
                 the_render_cluster_from_text_cluster(
                    &render, keycap, sizeof(keycap),
                    test_cluster_at_begin(keycap, sizeof(keycap)),
                    12, 0), 1);
      expect_size("render.keycap.base.cp.count", render.codepoint_count, 3);
      expect_int("render.keycap.base.output", render.output_method,
                 UTF8_TERM_OUTPUT_BASE);
      expect_int("render.keycap.base.mark", render.mark,
                 UTF8_TERM_MARK_COMPRESSED);
      expect_int("render.keycap.base.flag",
                 (render.flags & THE_RENDER_CLUSTER_BASE) != 0, 1);
      expect_int("render.keycap.base.width", render.width, 1);
      expect_int("render.keycap.base.display", render.advance_width, 1);
      expect_int("render.keycap.base.cursor", render.cursor_width, 1);
      expect_int("render.keycap.base.repaint", render.repaint_width, 3);
      expect_int("render.keycap.base.wchars",
                 the_render_cluster_to_wchars(&render, wch,
                                              sizeof(wch) / sizeof(wch[0])), 1);
      expect_int("render.keycap.base.wchar0", (int)wch[0], '1');
      expect_int("render.keycap.base.wchar1", (int)wch[1], 0);

      utf8_terminal_profile_reset();
      utf8_terminal_set_display_mode(UTF8_TERM_DISPLAY_DECOMPOSED);
      expect_int("render.keycap.decomposed.make",
                 the_render_cluster_from_text_cluster(
                    &render, keycap, sizeof(keycap),
                    test_cluster_at_begin(keycap, sizeof(keycap)),
                    12, 0), 1);
      expect_int("render.keycap.decomposed.output", render.output_method,
                 UTF8_TERM_OUTPUT_COMPONENTS);
      expect_int("render.keycap.decomposed.display", render.advance_width, 3);
      expect_int("render.keycap.decomposed.wchars",
                 the_render_cluster_to_wchars(&render, wch,
                                              sizeof(wch) / sizeof(wch[0])), 1);
      expect_int("render.keycap.decomposed.wchar0", (int)wch[0], '1');
      expect_int("render.keycap.decomposed.wchar1", (int)wch[1], ' ');
      expect_int("render.keycap.decomposed.wchar2", (int)wch[2], 0x25A1);
      expect_int("render.keycap.decomposed.wchar3", (int)wch[3], 0);

      utf8_terminal_profile_reset();
      expect_int("render.flag.make",
                 the_render_cluster_from_text_cluster(
                    &render, flag, sizeof(flag),
                    test_cluster_at_begin(flag, sizeof(flag)),
                    13, 0), 1);
      expect_size("render.flag.cp.count", render.codepoint_count, 2);
      expect_int("render.flag.width", render.width, 2);
      expect_int("render.flag.display", render.advance_width, 2);
      expect_int("render.flag.cursor", render.cursor_width, 2);
      expect_int("render.flag.repaint", render.repaint_width, 2);
      expect_int("render.flag.repair", render.repair_strategy,
                 UTF8_TERM_STRATEGY_CHANGED_CELLS);

      utf8_terminal_profile_reset();
      expect_int("render.text.heart.make",
                 the_render_cluster_from_text_cluster(
                    &render, text_heart, sizeof(text_heart),
                    test_cluster_at_begin(text_heart, sizeof(text_heart)),
                    14, 0), 1);
      expect_int("render.text.heart.class", render.feature_class,
                 UTF8_TERM_CLASS_AMBIGUOUS);
      expect_int("render.text.heart.display", render.advance_width, 1);
      expect_int("render.text.heart.cursor", render.cursor_width, 1);
      expect_int("render.text.heart.repaint", render.repaint_width, 1);

      expect_int("render.explicit.text.heart.make",
                 the_render_cluster_from_text_cluster(
                    &render, explicit_text_heart,
                    sizeof(explicit_text_heart),
                    test_cluster_at_begin(explicit_text_heart,
                                          sizeof(explicit_text_heart)),
                    15, 0), 1);
      expect_int("render.explicit.text.heart.class", render.feature_class,
                 UTF8_TERM_CLASS_TEXT_VARIATION);
      expect_int("render.explicit.text.heart.display", render.advance_width, 1);
      expect_int("render.explicit.text.heart.cursor", render.cursor_width, 1);
      expect_int("render.explicit.text.heart.repaint", render.repaint_width, 1);

      expect_int("render.emoji.heart.make",
                 the_render_cluster_from_text_cluster(
                    &render, emoji_heart, sizeof(emoji_heart),
                    test_cluster_at_begin(emoji_heart, sizeof(emoji_heart)),
                    16, 0), 1);
      expect_int("render.emoji.heart.class", render.feature_class,
                 UTF8_TERM_CLASS_EMOJI_VARIATION);
      expect_int("render.emoji.heart.display", render.advance_width, 2);
      expect_int("render.emoji.heart.cursor", render.cursor_width, 2);
      expect_int("render.emoji.heart.repaint", render.repaint_width, 2);

      utf8_terminal_profile_reset();
      utf8_terminal_set_display_mode(UTF8_TERM_DISPLAY_DECOMPOSED);
      expect_int("render.explicit.text.heart.decomposed.make",
                 the_render_cluster_from_text_cluster(
                    &render, explicit_text_heart,
                    sizeof(explicit_text_heart),
                    test_cluster_at_begin(explicit_text_heart,
                                          sizeof(explicit_text_heart)),
                    15, 0), 1);
      expect_int("render.explicit.text.heart.decomposed.display",
                 render.advance_width, 3);
      expect_int("render.explicit.text.heart.decomposed.wchars",
                 the_render_cluster_to_wchars(&render, wch,
                                              sizeof(wch) / sizeof(wch[0])), 1);
      expect_int("render.explicit.text.heart.decomposed.wchar0",
                 (int)wch[0], 0x2665);
      expect_int("render.explicit.text.heart.decomposed.wchar1",
                 (int)wch[1], ' ');
      expect_int("render.explicit.text.heart.decomposed.wchar2",
                 (int)wch[2], 'T');
      expect_int("render.explicit.text.heart.decomposed.wchar3",
                 (int)wch[3], 0);

      expect_int("render.emoji.heart.decomposed.make",
                 the_render_cluster_from_text_cluster(
                    &render, emoji_heart, sizeof(emoji_heart),
                    test_cluster_at_begin(emoji_heart, sizeof(emoji_heart)),
                    16, 0), 1);
      expect_int("render.emoji.heart.decomposed.display",
                 render.advance_width, 3);
      expect_int("render.emoji.heart.decomposed.wchars",
                 the_render_cluster_to_wchars(&render, wch,
                                              sizeof(wch) / sizeof(wch[0])), 1);
      expect_int("render.emoji.heart.decomposed.wchar0",
                 (int)wch[0], 0x2665);
      expect_int("render.emoji.heart.decomposed.wchar1",
                 (int)wch[1], ' ');
      expect_int("render.emoji.heart.decomposed.wchar2",
                 (int)wch[2], 'E');
      expect_int("render.emoji.heart.decomposed.wchar3",
                 (int)wch[3], 0);

      expect_int("render.zwj.make",
                 the_render_cluster_from_text_cluster(
                    &render, zwj, sizeof(zwj),
                    test_cluster_at_begin(zwj, sizeof(zwj)),
                    14, 0), 1);
      expect_size("render.zwj.cp.count", render.codepoint_count, 6);
      expect_int("render.zwj.components",
                 (render.flags & THE_RENDER_CLUSTER_COMPONENTS) != 0, 1);
      expect_int("render.zwj.display", render.advance_width, 7);
      expect_int("render.zwj.cursor", render.cursor_width, 7);
      expect_int("render.zwj.repaint", render.repaint_width, 7);
      expect_int("render.zwj.wchars",
                 the_render_cluster_to_wchars(&render, wch,
                                              sizeof(wch) / sizeof(wch[0])), 1);
#if defined(WCHAR_MAX) && WCHAR_MAX > 0xFFFFu
      expect_int("render.zwj.wchar0", (int)wch[0], 0x1F469);
      expect_int("render.zwj.wchar1", (int)wch[1], ' ');
      expect_int("render.zwj.wchar2", (int)wch[2], 0x2764);
      expect_int("render.zwj.wchar3", (int)wch[3], ' ');
      expect_int("render.zwj.wchar4", (int)wch[4], 0x1F468);
      expect_int("render.zwj.wchar5", (int)wch[5], 0);
#endif

      utf8_terminal_profile_reset();
      expect_int("render.zwj.components.apply",
                 utf8_terminal_profile_apply_line(
                    "SET UTF TERMINAL CLASS heart-zwj DISPLAY decomposed OUTPUT components"),
                 UTF8_TERMINAL_PROFILE_APPLIED);
      utf8_terminal_set_display_mode(UTF8_TERM_DISPLAY_DECOMPOSED);
      expect_int("render.zwj.components.make",
                 the_render_cluster_from_text_cluster(
                    &render, zwj, sizeof(zwj),
                    test_cluster_at_begin(zwj, sizeof(zwj)),
                    14, 0), 1);
      expect_int("render.zwj.components.output", render.output_method,
                 UTF8_TERM_OUTPUT_COMPONENTS);
      expect_int("render.zwj.components.flag",
                 (render.flags & THE_RENDER_CLUSTER_COMPONENTS) != 0, 1);
      expect_int("render.zwj.components.wchars",
                 the_render_cluster_to_wchars(&render, wch,
                                              sizeof(wch) / sizeof(wch[0])), 1);
#if defined(WCHAR_MAX) && WCHAR_MAX > 0xFFFFu
      expect_int("render.zwj.components.wchar0", (int)wch[0], 0x1F469);
      expect_int("render.zwj.components.wchar1", (int)wch[1], ' ');
      expect_int("render.zwj.components.wchar2", (int)wch[2], 0x2764);
      expect_int("render.zwj.components.wchar3", (int)wch[3], ' ');
      expect_int("render.zwj.components.wchar4", (int)wch[4], 0x1F468);
      expect_int("render.zwj.components.wchar5", (int)wch[5], 0);
#endif
      {
         size_t i;

         for (i = 0; wch[i] != L'\0'; i++)
         {
            expect_int("render.zwj.components.no.zwj",
                       (int)wch[i] == 0x200D, 0);
            expect_int("render.zwj.components.no.vs16",
                       (int)wch[i] == 0xFE0F, 0);
         }
      }
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
   TheRenderCell stored;

   headless_driver_reset();
   headless_driver_set_current_screen(0);
   headless_driver_set_screen_current_role(0, 0);
   win = headless_driver_create_screen_role(0, 0, 3, 4, 5, 6);
   expect_nonnull("role.win", win);

   size = ops->window_size(win);
   expect_int("role.size.valid", size.valid, 1);
   expect_int("role.size.rows", size.rows, 3);
   expect_int("role.size.cols", size.cols, 4);
   origin = ops->window_origin(win);
   expect_int("role.origin.valid", origin.valid, 1);
   expect_int("role.origin.row", origin.row, 5);
   expect_int("role.origin.col", origin.col, 6);

   ops->move_window_cursor(win, 1, 2);
   cursor = ops->capture_window_cursor(win);
   expect_int("role.cursor.valid", cursor.valid, 1);
   expect_int("role.cursor.row", cursor.row, 1);
   expect_int("role.cursor.col", cursor.col, 2);
   point.row = (short)(origin.row + cursor.row);
   point.col = (short)(origin.col + cursor.col);
   point.valid = origin.valid && cursor.valid;
   expect_int("role.point.valid", point.valid, 1);
   expect_int("role.point.row", point.row, 6);
   expect_int("role.point.col", point.col, 8);

   saved = cursor;
   ops->move_window_cursor(win, 2, 3);
   ops->restore_window_cursor(win, saved);
   cursor = ops->capture_window_cursor(win);
   expect_int("role.restore.row", cursor.row, 1);
   expect_int("role.restore.col", cursor.col, 2);

   ops->add_cell_at(win, 0, 1, 'X');
   ops->move_window_cursor(win, 0, 1);
   expect_int("role.inspect.cell",
              headless_driver_render_cell_at(win, 0, 1, &stored), 1);
   expect_int("role.inspect.cp", (int)stored.codepoints[0], 'X');

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
   ops->move_window_cursor(global, 1, 4);
   cursor = ops->capture_window_cursor(global);
   expect_int("global.cursor.row", cursor.row, 1);
   expect_int("global.cursor.col", cursor.col, 4);
   ops->delete_window(global);
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
      expect_int("render.headless.cluster.width", stored.width, 2);
      expect_int("render.headless.cluster.display", stored.advance_width, 2);
      expect_int("render.headless.cluster.cursor", stored.cursor_width, 2);
      expect_int("render.headless.cluster.repaint", stored.repaint_width, 2);
      expect_long("render.headless.cluster.log.count",
                  (long)headless_driver_log_count(), 1);
      expect_str("render.headless.cluster.log",
                 headless_driver_log_entry(0),
                 "render-cluster:window:1:1:2:3:1:2:2:2:2");
      utf8_terminal_profile_reset();
   }
#endif
}

static void test_operation_log(void)
{
   const TheDriverOps *ops = &the_headless_driver_ops;
   TheDriverWindow *win;
   TheDriverWindow *global;

   headless_driver_reset();
   headless_driver_set_current_screen(0);
   headless_driver_set_screen_current_role(0, 0);
   win = headless_driver_create_screen_role(0, 0, 3, 4, 1, 2);
   expect_nonnull("log.win", win);
   global = headless_driver_create_global_window(THE_DRIVER_GLOBAL_STATAREA,
                                                 1, 10, 0, 0);
   expect_nonnull("log.global", global);
   headless_driver_clear_log();

   ops->touch_window(win);
   ops->refresh_window(win);
   ops->update();
   ops->touch_window(win);
   ops->refresh_window(win);
   ops->touch_window(global);
   ops->refresh_window(global);

   expect_long("log.count", (long)headless_driver_log_count(), 7);
   expect_str("log.0", headless_driver_log_entry(0), "touch:window:1");
   expect_str("log.1", headless_driver_log_entry(1), "refresh:window:1");
   expect_str("log.2", headless_driver_log_entry(2), "update");
   expect_str("log.3", headless_driver_log_entry(3), "touch:window:1");
   expect_str("log.4", headless_driver_log_entry(4), "refresh:window:1");
   expect_str("log.5", headless_driver_log_entry(5), "touch:window:2");
   expect_str("log.6", headless_driver_log_entry(6), "refresh:window:2");
}

static void test_terminal_report_ops(void)
{
   const TheDriverOps *ops = &the_headless_driver_ops;

   headless_driver_reset();
   headless_driver_clear_log();
   ops->begin_terminal_report();
   ops->write_terminal_report_text(0, 0, 1, "name", 4);
   ops->write_terminal_report_text(0, 5, 0, "value", 5);
   ops->end_terminal_report();
   ops->clear_terminal_screen();
   ops->sync_terminal_screen();
   ops->prepare_for_shell_escape();
   ops->repair_terminal_background(THE_DRIVER_REPAIR_ACTIVE_SURFACE);
   ops->repair_terminal_background(THE_DRIVER_REPAIR_TERMINAL_SCREEN);

   expect_long("terminal.log.count", (long)headless_driver_log_count(), 6);
   expect_str("terminal.log.0", headless_driver_log_entry(0),
              "terminal-report:2");
   expect_str("terminal.log.1", headless_driver_log_entry(1),
              "clear:terminal");
   expect_str("terminal.log.2", headless_driver_log_entry(2),
              "sync:terminal");
   expect_str("terminal.log.3", headless_driver_log_entry(3),
              "prepare:shell");
   expect_str("terminal.log.4", headless_driver_log_entry(4),
              "repair-background:current");
   expect_str("terminal.log.5", headless_driver_log_entry(5),
              "repair-background:terminal");
}

static void test_input_queue_and_adapter(void)
{
   const TheDriverOps *ops = &the_headless_driver_ops;
   TheInputEvent input;

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
   expect_int("input.legacy.adapter", the_driver_read_legacy_key(), 'A');
   expect_int("input.legacy.empty", the_driver_read_legacy_key(), -1);
}

static void test_filearea_cursor_updates_editor_state(void)
{
   const TheDriverOps *ops = &the_headless_driver_ops;
   VIEW_DETAILS view;
   TheDriverWindow *win;
   TheDriverWindowCursor physical;
   static const CHARTYPE line[] = "alpha";

   memset(&view, 0, sizeof(view));
   view.focus_line = 12;
   view.verify_col = 1;
   headless_driver_reset();
   headless_driver_set_current_screen(0);
   headless_driver_set_screen_current_role(0, WINDOW_FILEAREA);
   win = headless_driver_create_screen_role(0, WINDOW_FILEAREA,
                                            3, 20, 0, 0);

   expect_int("filearea.move",
              ops->move_filearea_cursor(0, &view, line,
                                        sizeof(line) - 1, 1, 2), RC_OK);
   expect_int("filearea.logical.valid",
              view.logical_cursor.current.valid, 1);
   expect_long("filearea.logical.line",
               view.logical_cursor.current.line_number, 12);
   expect_int("filearea.logical.cell",
              view.logical_cursor.current.text.cell_column, 2);
   physical = ops->capture_window_cursor(win);
   expect_int("filearea.physical.row", physical.row, 1);
   expect_int("filearea.physical.col", physical.col, 2);
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
   test_terminal_report_ops();
   test_input_queue_and_adapter();
   test_filearea_cursor_updates_editor_state();
   headless_driver_reset();

   if (failures != 0)
   {
      fprintf(stderr, "headless driver tests failed: %d\n", failures);
      return 1;
   }
   return 0;
}
