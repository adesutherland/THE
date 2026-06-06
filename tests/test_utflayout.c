#include <stdio.h>

#include "textpos.h"
#include "utflayout.h"
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

static void test_keycap_physical_layout(void)
{
   static const CHARTYPE keycap[] = { 'A', '1',
                                      0xEF, 0xB8, 0x8F,
                                      0xE2, 0x83, 0xA3, 'B' };
   TextCluster cluster;

   utf8_terminal_profile_reset();
   utf8_terminal_profile_apply_line("SET UTF TERMINAL CLASS keycap WIDTH 2 ADVANCE 2 CURSOR 2 REPAINT 2");

   cluster = textpos_cluster_at_boundary(keycap, sizeof(keycap),
                                         textpos_from_cell(keycap, sizeof(keycap),
                                                           1, TEXT_SNAP_BACKWARD));
   expect_int("keycap.logical.width", utf8_layout_cluster_logical_width(cluster), 1);
   expect_int("keycap.profile.width",
              utf8_layout_cluster_width(keycap, sizeof(keycap), cluster), 2);
   expect_int("keycap.display.width",
              utf8_layout_cluster_advance_width(keycap, sizeof(keycap), cluster), 2);
   expect_int("keycap.cursor.width",
              utf8_layout_cluster_cursor_width(keycap, sizeof(keycap), cluster), 2);
   expect_int("keycap.repaint.width",
              utf8_layout_cluster_repaint_width(keycap, sizeof(keycap), cluster), 2);
   expect_int("keycap.logical.to.display.A",
              utf8_layout_display_col_from_logical(keycap, sizeof(keycap), 0, 1), 1);
   expect_int("keycap.logical.to.display.B",
              utf8_layout_display_col_from_logical(keycap, sizeof(keycap), 0, 2), 3);
   expect_int("keycap.display.to.logical.inside",
              utf8_layout_logical_col_from_display(keycap, sizeof(keycap), 0, 2,
                                                   TEXT_SNAP_BACKWARD), 1);
   expect_int("keycap.display.to.logical.forward",
              utf8_layout_logical_col_from_display(keycap, sizeof(keycap), 0, 2,
                                                   TEXT_SNAP_FORWARD), 2);

   utf8_terminal_profile_apply_line(
      "SET UTF TERMINAL CLASS keycap WIDTH 2 ADVANCE 2 CURSOR 2 REPAINT 4");
   expect_int("keycap.repaint.override.display",
              utf8_layout_cluster_advance_width(keycap, sizeof(keycap), cluster), 2);
   expect_int("keycap.repaint.override.cursor",
              utf8_layout_cluster_cursor_width(keycap, sizeof(keycap), cluster), 2);
   expect_int("keycap.repaint.override.repaint",
              utf8_layout_cluster_repaint_width(keycap, sizeof(keycap), cluster), 4);
   expect_int("keycap.repaint.override.logical.to.display.B",
              utf8_layout_display_col_from_logical(keycap, sizeof(keycap), 0, 2), 3);

   utf8_terminal_profile_apply_line(
      "SET UTF TERMINAL CLASS keycap WIDTH 2 ADVANCE 2 CURSOR 4 REPAINT 2");
   expect_int("keycap.cursor.override.cursor",
              utf8_layout_cluster_cursor_width(keycap, sizeof(keycap), cluster), 4);
   expect_int("keycap.cursor.override.repaint",
              utf8_layout_cluster_repaint_width(keycap, sizeof(keycap), cluster), 2);
   expect_int("keycap.cursor.override.logical.to.display.B",
              utf8_layout_display_col_from_logical(keycap, sizeof(keycap), 0, 2), 3);
}

static void test_keycap_viewport_uses_physical_width(void)
{
   static const CHARTYPE keycaps[] = {
      'A', '1', 0xEF, 0xB8, 0x8F, 0xE2, 0x83, 0xA3, 'B',
      'A', '2', 0xEF, 0xB8, 0x8F, 0xE2, 0x83, 0xA3, 'B',
      'A', '3', 0xEF, 0xB8, 0x8F, 0xE2, 0x83, 0xA3, 'B'
   };
   TextPos end;
   Utf8LayoutViewport target;
   int logical_col;
   int logical_visible_cols = 15;

   utf8_terminal_profile_reset();
   utf8_terminal_profile_apply_line("SET UTF TERMINAL CLASS keycap WIDTH 2 ADVANCE 2 CURSOR 2 REPAINT 2");

   end = textpos_from_byte(keycaps, sizeof(keycaps), sizeof(keycaps));
   expect_int("keycaps.line.logical.width", end.cell_column, 9);
   expect_int("keycaps.line.physical.width",
              utf8_layout_display_col_from_logical(keycaps, sizeof(keycaps),
                                                   0, end.cell_column),
              12);

   logical_col = 14;
   expect_int("keycaps.logical.target.in.old.viewport",
              logical_col < logical_visible_cols, 1);
   expect_int("keycaps.physical.target.outside.old.viewport",
              utf8_layout_display_col_from_logical(keycaps, sizeof(keycaps),
                                                   0, logical_col)
                 >= logical_visible_cols,
              1);

   target = utf8_layout_viewport_for_logical_col(keycaps, sizeof(keycaps),
                                                 0, logical_col,
                                                 logical_visible_cols);
   expect_int("keycaps.viewport.shifted", target.viewport_col > 0, 1);
   expect_int("keycaps.viewport.visible", target.visible, 1);
   expect_int("keycaps.viewport.physical.in.range",
              target.display_col < logical_visible_cols, 1);
}

static void test_keycap_trailing_cells_stay_logical(void)
{
   static const CHARTYPE keycap_tail[] = {
      'A', '1', 0xEF, 0xB8, 0x8F, 0xE2, 0x83, 0xA3, 'B',
      ' ', ' ', ' ', ' ', ' '
   };
   TextPos end;

   utf8_terminal_profile_reset();
   utf8_terminal_profile_apply_line("SET UTF TERMINAL CLASS keycap WIDTH 2 ADVANCE 2 CURSOR 2 REPAINT 2");

   end = textpos_from_byte(keycap_tail, sizeof(keycap_tail), sizeof(keycap_tail));
   expect_int("keycap.tail.logical.width", end.cell_column, 8);
   expect_int("keycap.tail.physical.width",
              utf8_layout_display_col_from_logical(keycap_tail,
                                                   sizeof(keycap_tail),
                                                   0, end.cell_column),
              9);
   expect_int("keycap.tail.display.fifth-space",
              utf8_layout_logical_col_from_display(keycap_tail,
                                                   sizeof(keycap_tail),
                                                   0, 8,
                                                   TEXT_SNAP_BACKWARD),
              7);
   expect_int("keycap.tail.after-line",
              utf8_layout_logical_col_from_display(keycap_tail,
                                                   sizeof(keycap_tail),
                                                   0, 9,
                                                   TEXT_SNAP_BACKWARD),
              8);
}

static void test_terminal_profile_does_not_change_logical_positions(void)
{
   static const CHARTYPE keycap[] = { 'A', '1',
                                      0xEF, 0xB8, 0x8F,
                                      0xE2, 0x83, 0xA3, 'B' };
   TextPos end;

   utf8_terminal_profile_reset();
   utf8_terminal_profile_apply_line("SET UTF TERMINAL CLASS keycap WIDTH 2 ADVANCE 2 CURSOR 2 REPAINT 2");
   end = textpos_from_byte(keycap, sizeof(keycap), sizeof(keycap));
   expect_int("profile.width2.logical.end", end.cell_column, 3);
   expect_int("profile.width2.physical.end",
              utf8_layout_display_col_from_logical(keycap, sizeof(keycap),
                                                   0, end.cell_column),
              4);

   utf8_terminal_profile_apply_line("SET UTF TERMINAL CLASS keycap WIDTH 1 ADVANCE 1 CURSOR 1 REPAINT 1");
   end = textpos_from_byte(keycap, sizeof(keycap), sizeof(keycap));
   expect_int("profile.width1.logical.end", end.cell_column, 3);
   expect_int("profile.width1.physical.end",
              utf8_layout_display_col_from_logical(keycap, sizeof(keycap),
                                                   0, end.cell_column),
              3);
}

static void test_profile_width_is_user_column_model(void)
{
   static const CHARTYPE keycap[] = { 'A', '1',
                                      0xEF, 0xB8, 0x8F,
                                      0xE2, 0x83, 0xA3, 'B' };
   TextPos end;

   utf8_terminal_profile_reset();
   utf8_terminal_profile_apply_line(
      "SET UTF TERMINAL CLASS keycap WIDTH 2 ADVANCE 4 CURSOR 4 REPAINT 4");
   end = textpos_from_byte(keycap, sizeof(keycap), sizeof(keycap));

   expect_int("profile.width.columns.end",
              utf8_layout_width_col_from_logical(keycap, sizeof(keycap),
                                                 end.cell_column),
              4);
   expect_int("profile.width.columns.physical.end",
              utf8_layout_display_col_from_logical(keycap, sizeof(keycap),
                                                   0, end.cell_column),
              6);
   expect_int("profile.width.inside.backward",
              utf8_layout_logical_col_from_width(keycap, sizeof(keycap), 2,
                                                 TEXT_SNAP_BACKWARD),
              1);
   expect_int("profile.width.inside.forward",
              utf8_layout_logical_col_from_width(keycap, sizeof(keycap), 2,
                                                 TEXT_SNAP_FORWARD),
              2);
   expect_int("profile.width.after.cluster",
              utf8_layout_logical_col_from_width(keycap, sizeof(keycap), 3,
                                                 TEXT_SNAP_BACKWARD),
              2);
}

static void test_width_slice_returns_whole_clusters(void)
{
   static const CHARTYPE wide[] = {
      'A', 0xE4, 0xB8, 0xAD, 'B'
   };
   static const CHARTYPE keycap[] = {
      'A', '1', 0xEF, 0xB8, 0x8F, 0xE2, 0x83, 0xA3, 'B'
   };
   TextCellSlice slice;

   utf8_terminal_profile_reset();
   slice = utf8_layout_slice_width(wide, sizeof(wide), 2, 1);
   expect_int("width.slice.wide.inside.start.cell", slice.start.cell_column, 1);
   expect_int("width.slice.wide.inside.end.cell", slice.end.cell_column, 3);
   expect_int("width.slice.wide.inside.start.byte",
              (int)slice.start.byte_offset, 1);
   expect_int("width.slice.wide.inside.end.byte",
              (int)slice.end.byte_offset, 4);
   expect_int("width.slice.wide.inside.leading", slice.leading_cells, 1);
   expect_int("width.slice.wide.inside.content", slice.content_cells, 2);
   expect_int("width.slice.wide.inside.trailing", slice.trailing_cells, 0);

   slice = utf8_layout_slice_width(wide, sizeof(wide), 1, 2);
   expect_int("width.slice.wide.exact.start.cell", slice.start.cell_column, 1);
   expect_int("width.slice.wide.exact.end.cell", slice.end.cell_column, 3);
   expect_int("width.slice.wide.exact.end.byte",
              (int)slice.end.byte_offset, 4);

   slice = utf8_layout_slice_width(wide, sizeof(wide), 3, 1);
   expect_int("width.slice.wide.boundary.start.cell", slice.start.cell_column,
              3);
   expect_int("width.slice.wide.boundary.end.cell", slice.end.cell_column, 4);
   expect_int("width.slice.wide.boundary.start.byte",
              (int)slice.start.byte_offset, 4);

   slice = utf8_layout_slice_width(wide, sizeof(wide), 8, 2);
   expect_int("width.slice.beyond.start.byte", (int)slice.start.byte_offset,
              (int)sizeof(wide));
   expect_int("width.slice.beyond.end.byte", (int)slice.end.byte_offset,
              (int)sizeof(wide));
   expect_int("width.slice.beyond.content", slice.content_cells, 0);
   expect_int("width.slice.beyond.trailing", slice.trailing_cells, 2);

   utf8_terminal_profile_apply_line(
      "SET UTF TERMINAL CLASS keycap WIDTH 2 ADVANCE 4 CURSOR 4 REPAINT 4");
   slice = utf8_layout_slice_width(keycap, sizeof(keycap), 2, 1);
   expect_int("width.slice.keycap.inside.start.cell", slice.start.cell_column,
              1);
   expect_int("width.slice.keycap.inside.end.cell", slice.end.cell_column, 2);
   expect_int("width.slice.keycap.inside.start.byte",
              (int)slice.start.byte_offset, 1);
   expect_int("width.slice.keycap.inside.end.byte",
              (int)slice.end.byte_offset, 8);
   expect_int("width.slice.keycap.inside.leading", slice.leading_cells, 1);
   expect_int("width.slice.keycap.inside.content", slice.content_cells, 2);

   slice = utf8_layout_slice_width(keycap, sizeof(keycap), 1, 2);
   expect_int("width.slice.keycap.exact.end.byte",
              (int)slice.end.byte_offset, 8);
   slice = utf8_layout_slice_width(keycap, sizeof(keycap), 3, 1);
   expect_int("width.slice.keycap.boundary.start.cell",
              slice.start.cell_column, 2);
   expect_int("width.slice.keycap.boundary.start.byte",
              (int)slice.start.byte_offset, 8);
}

static void test_zwj_display_mapping_snaps_to_cluster_start(void)
{
   static const CHARTYPE short_zwj[] = {
      'A',
      0xF0, 0x9F, 0x91, 0xA9, 0xE2, 0x80, 0x8D,
      0xF0, 0x9F, 0x92, 0xBB,
      'B'
   };
   static const CHARTYPE heart_zwj[] = {
      'A',
      0xF0, 0x9F, 0x91, 0xA9, 0xE2, 0x80, 0x8D,
      0xE2, 0x9D, 0xA4, 0xEF, 0xB8, 0x8F,
      0xE2, 0x80, 0x8D, 0xF0, 0x9F, 0x91, 0xA8,
      'B'
   };
   TextCluster cluster;
   TextPos snapped;
   int raw_cell;

   utf8_terminal_profile_reset();
   cluster = textpos_cluster_at_boundary(
      short_zwj, sizeof(short_zwj),
      textpos_from_cell(short_zwj, sizeof(short_zwj), 1,
                        TEXT_SNAP_BACKWARD));
   expect_int("short-zwj.logical.start", cluster.pos.cell_column, 1);
   expect_int("short-zwj.logical.end", cluster.end.cell_column, 5);
   expect_int("short-zwj.normal.display.width",
              utf8_layout_cluster_advance_width(short_zwj, sizeof(short_zwj),
                                                cluster),
              2);
   expect_int("short-zwj.normal.display.inside",
              utf8_layout_logical_col_from_display(short_zwj,
                                                   sizeof(short_zwj), 0, 2,
                                                   TEXT_SNAP_BACKWARD),
              1);

   utf8_terminal_set_display_mode(UTF8_TERM_DISPLAY_DECOMPOSED);
   expect_int("short-zwj.components.display.width",
              utf8_layout_cluster_advance_width(short_zwj, sizeof(short_zwj),
                                                cluster),
              5);
   raw_cell = utf8_layout_logical_col_from_display(short_zwj,
                                                  sizeof(short_zwj), 0, 3,
                                                  TEXT_SNAP_BACKWARD);
   snapped = textpos_from_cell(short_zwj, sizeof(short_zwj), raw_cell,
                               TEXT_SNAP_BACKWARD);
   expect_int("short-zwj.components.display-inside", raw_cell, 1);
   expect_int("short-zwj.components.snapped-start", snapped.cell_column, 1);

   utf8_terminal_profile_reset();
   cluster = textpos_cluster_at_boundary(
      heart_zwj, sizeof(heart_zwj),
      textpos_from_cell(heart_zwj, sizeof(heart_zwj), 1,
                        TEXT_SNAP_BACKWARD));
   expect_int("heart-zwj.logical.start", cluster.pos.cell_column, 1);
   expect_int("heart-zwj.logical.end", cluster.end.cell_column, 6);
   expect_int("heart-zwj.normal.display.inside",
              utf8_layout_logical_col_from_display(heart_zwj,
                                                   sizeof(heart_zwj), 0, 2,
                                                   TEXT_SNAP_BACKWARD),
              1);
   utf8_terminal_set_display_mode(UTF8_TERM_DISPLAY_DECOMPOSED);
   expect_int("heart-zwj.components.display.inside",
              utf8_layout_logical_col_from_display(heart_zwj,
                                                   sizeof(heart_zwj), 0, 4,
                                                   TEXT_SNAP_BACKWARD),
              1);
}

static void test_dynamic_zwj_component_widths(void)
{
   static const CHARTYPE triple_zwj[] = {
      'A',
      0xF0, 0x9F, 0xA7, 0x91, 0xE2, 0x80, 0x8D,
      0xF0, 0x9F, 0x9A, 0x80, 0xE2, 0x80, 0x8D,
      0xF0, 0x9F, 0x92, 0xBB,
      'B'
   };
   static const CHARTYPE flag[] = {
      'A',
      0xF0, 0x9F, 0x87, 0xAC,
      0xF0, 0x9F, 0x87, 0xA7,
      'B'
   };
   static const CHARTYPE heart_zwj[] = {
      'A',
      0xF0, 0x9F, 0x91, 0xA9, 0xE2, 0x80, 0x8D,
      0xE2, 0x9D, 0xA4, 0xEF, 0xB8, 0x8F,
      0xE2, 0x80, 0x8D,
      0xF0, 0x9F, 0x91, 0xA8,
      'B'
   };
   static const CHARTYPE family_five[] = {
      'A',
      0xF0, 0x9F, 0x91, 0xA8, 0xE2, 0x80, 0x8D,
      0xF0, 0x9F, 0x91, 0xA9, 0xE2, 0x80, 0x8D,
      0xF0, 0x9F, 0x91, 0xA7, 0xE2, 0x80, 0x8D,
      0xF0, 0x9F, 0x91, 0xA6, 0xE2, 0x80, 0x8D,
      0xF0, 0x9F, 0x91, 0xB6,
      'B'
   };
   TextCluster cluster;

   utf8_terminal_profile_reset();
   cluster = textpos_cluster_at_boundary(
      triple_zwj, sizeof(triple_zwj),
      textpos_from_cell(triple_zwj, sizeof(triple_zwj), 1,
                        TEXT_SNAP_BACKWARD));
   expect_int("zwj.triple.normal.advance",
              utf8_layout_cluster_advance_width(triple_zwj,
                                                sizeof(triple_zwj),
                                                cluster),
              2);
   utf8_terminal_set_display_mode(UTF8_TERM_DISPLAY_DECOMPOSED);
   expect_int("zwj.triple.decomposed.width",
              utf8_layout_cluster_width(triple_zwj, sizeof(triple_zwj),
                                        cluster),
              8);
   expect_int("zwj.triple.decomposed.advance",
              utf8_layout_cluster_advance_width(triple_zwj,
                                                sizeof(triple_zwj),
                                                cluster),
              8);
   utf8_terminal_set_display_mode(UTF8_TERM_DISPLAY_SINGLE);
   expect_int("zwj.triple.single.width",
              utf8_layout_cluster_width(triple_zwj, sizeof(triple_zwj),
                                        cluster),
              1);
   expect_int("zwj.triple.single.advance",
              utf8_layout_cluster_advance_width(triple_zwj,
                                                sizeof(triple_zwj),
                                                cluster),
              1);

   utf8_terminal_profile_reset();
   cluster = textpos_cluster_at_boundary(
      family_five, sizeof(family_five),
      textpos_from_cell(family_five, sizeof(family_five), 1,
                        TEXT_SNAP_BACKWARD));
   utf8_terminal_set_display_mode(UTF8_TERM_DISPLAY_DECOMPOSED);
   expect_int("zwj.family5.decomposed.width",
              utf8_layout_cluster_width(family_five, sizeof(family_five),
                                        cluster),
              14);
   expect_int("zwj.family5.decomposed.advance",
              utf8_layout_cluster_advance_width(family_five,
                                                sizeof(family_five),
                                                cluster),
              14);

   utf8_terminal_profile_reset();
   expect_int("zwj.family5.normal.components.apply",
              utf8_terminal_profile_apply_line(
                 "SET UTF TERMINAL CLASS family-zwj DISPLAY normal OUTPUT components"),
              UTF8_TERMINAL_PROFILE_APPLIED);
   expect_int("zwj.family5.normal.components.width",
              utf8_layout_cluster_width(family_five, sizeof(family_five),
                                        cluster),
              14);
   expect_int("zwj.family5.normal.components.advance",
              utf8_layout_cluster_advance_width(family_five,
                                                sizeof(family_five),
                                                cluster),
              14);

   utf8_terminal_profile_reset();
   expect_int("zwj.family5.normal.expanded.metrics.apply",
              utf8_terminal_profile_apply_line(
                 "SET UTF TERMINAL CLASS family-zwj DISPLAY normal METRICS expanded"),
              UTF8_TERMINAL_PROFILE_APPLIED);
   expect_int("zwj.family5.normal.expanded.metrics.width",
              utf8_layout_cluster_width(family_five, sizeof(family_five),
                                        cluster),
              10);
   expect_int("zwj.family5.normal.expanded.metrics.advance",
              utf8_layout_cluster_advance_width(family_five,
                                                sizeof(family_five),
                                                cluster),
              10);

   utf8_terminal_profile_reset();
   expect_int("apple.profile.flag.apply",
              utf8_terminal_profile_apply_line(
                 "SET UTF TERMINAL CLASS regional-flag DISPLAY normal WIDTH 2 ADVANCE 3 CURSOR 3 REPAINT 3"),
              UTF8_TERMINAL_PROFILE_APPLIED);
   expect_int("apple.profile.short.metrics.apply",
              utf8_terminal_profile_apply_line(
                 "SET UTF TERMINAL CLASS short-zwj DISPLAY normal METRICS expanded"),
              UTF8_TERMINAL_PROFILE_APPLIED);
   expect_int("apple.profile.heart.metrics.apply",
              utf8_terminal_profile_apply_line(
                 "SET UTF TERMINAL CLASS heart-zwj DISPLAY normal METRICS expanded"),
              UTF8_TERMINAL_PROFILE_APPLIED);
   expect_int("apple.profile.family.metrics.apply",
              utf8_terminal_profile_apply_line(
                 "SET UTF TERMINAL CLASS family-zwj DISPLAY normal METRICS expanded"),
              UTF8_TERMINAL_PROFILE_APPLIED);
   cluster = textpos_cluster_at_boundary(
      flag, sizeof(flag),
      textpos_from_cell(flag, sizeof(flag), 1, TEXT_SNAP_BACKWARD));
   expect_int("apple.flag.normal.native.width",
              utf8_layout_cluster_width(flag, sizeof(flag), cluster), 2);
   expect_int("apple.flag.normal.native.advance",
              utf8_layout_cluster_advance_width(flag, sizeof(flag), cluster),
              3);
   cluster = textpos_cluster_at_boundary(
      triple_zwj, sizeof(triple_zwj),
      textpos_from_cell(triple_zwj, sizeof(triple_zwj), 1,
                        TEXT_SNAP_BACKWARD));
   expect_int("apple.zwj.triple.normal.expanded.metrics.width",
              utf8_layout_cluster_width(triple_zwj, sizeof(triple_zwj),
                                        cluster),
              6);
   expect_int("apple.zwj.triple.normal.expanded.metrics.advance",
              utf8_layout_cluster_advance_width(triple_zwj,
                                                sizeof(triple_zwj),
                                                cluster),
              6);
   cluster = textpos_cluster_at_boundary(
      heart_zwj, sizeof(heart_zwj),
      textpos_from_cell(heart_zwj, sizeof(heart_zwj), 1,
                        TEXT_SNAP_BACKWARD));
   expect_int("apple.heart.normal.expanded.metrics.width",
              utf8_layout_cluster_width(heart_zwj, sizeof(heart_zwj),
                                        cluster),
              5);
   expect_int("apple.heart.normal.expanded.metrics.advance",
              utf8_layout_cluster_advance_width(heart_zwj,
                                                sizeof(heart_zwj),
                                                cluster),
              5);
   cluster = textpos_cluster_at_boundary(
      family_five, sizeof(family_five),
      textpos_from_cell(family_five, sizeof(family_five), 1,
                        TEXT_SNAP_BACKWARD));
   expect_int("apple.family5.normal.expanded.metrics.width",
              utf8_layout_cluster_width(family_five, sizeof(family_five),
                                        cluster),
              10);
   expect_int("apple.family5.normal.expanded.metrics.advance",
              utf8_layout_cluster_advance_width(family_five,
                                                sizeof(family_five),
                                                cluster),
              10);

   utf8_terminal_set_display_mode(UTF8_TERM_DISPLAY_DECOMPOSED);
   utf8_terminal_profile_apply_line(
      "SET UTF TERMINAL CLASS family-zwj DISPLAY decomposed WIDTH 11 ADVANCE 12 CURSOR 13 REPAINT 14");
   expect_int("zwj.family5.delta.width",
              utf8_layout_cluster_width(family_five, sizeof(family_five),
                                        cluster),
              14);
   expect_int("zwj.family5.delta.advance",
              utf8_layout_cluster_advance_width(family_five,
                                                sizeof(family_five),
                                                cluster),
              15);
   expect_int("zwj.family5.delta.cursor",
              utf8_layout_cluster_cursor_width(family_five,
                                               sizeof(family_five),
                                               cluster),
              16);
   expect_int("zwj.family5.delta.repaint",
              utf8_layout_cluster_repaint_width(family_five,
                                                sizeof(family_five),
                                                cluster),
              17);
}

static void test_symbol_presentation_widths(void)
{
   static const CHARTYPE line[] = {
      'A',
      0xE2, 0x99, 0xA5,
      'B', ' ', 'A',
      0xE2, 0x99, 0xA5, 0xEF, 0xB8, 0x8F,
      'B'
   };
   TextCluster text_heart;
   TextCluster emoji_heart;

   utf8_terminal_profile_reset();
   text_heart = textpos_cluster_at_boundary(
      line, sizeof(line),
      textpos_from_cell(line, sizeof(line), 1, TEXT_SNAP_BACKWARD));
   emoji_heart = textpos_cluster_at_boundary(
      line, sizeof(line),
      textpos_from_cell(line, sizeof(line), 5, TEXT_SNAP_BACKWARD));

   expect_int("symbol.text.logical.width", text_heart.cell_width, 1);
   expect_int("symbol.text.display.width",
              utf8_layout_cluster_advance_width(line, sizeof(line),
                                                text_heart),
              1);
   expect_int("symbol.text.next.display",
              utf8_layout_display_col_from_logical(line, sizeof(line), 0, 2),
              2);
   expect_int("symbol.emoji.logical.width", emoji_heart.cell_width, 1);
   expect_int("symbol.emoji.display.width",
              utf8_layout_cluster_advance_width(line, sizeof(line),
                                                emoji_heart),
              2);
   expect_int("symbol.emoji.next.display",
              utf8_layout_display_col_from_logical(line, sizeof(line), 0, 6),
              7);
}

int main(void)
{
   test_keycap_physical_layout();
   test_keycap_viewport_uses_physical_width();
   test_keycap_trailing_cells_stay_logical();
   test_terminal_profile_does_not_change_logical_positions();
   test_profile_width_is_user_column_model();
   test_width_slice_returns_whole_clusters();
   test_zwj_display_mapping_snaps_to_cluster_start();
   test_dynamic_zwj_component_widths();
   test_symbol_presentation_widths();

   if (failures != 0)
   {
      fprintf(stderr, "UTF layout tests failed: %d\n", failures);
      return 1;
   }
   return 0;
}
