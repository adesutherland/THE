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
   utf8_terminal_profile_apply_line("SET UTF TERMINAL CLASS keycap LAYOUT 2 CURSOR 2");

   cluster = textpos_cluster_at_boundary(keycap, sizeof(keycap),
                                         textpos_from_cell(keycap, sizeof(keycap),
                                                           1, TEXT_SNAP_BACKWARD));
   expect_int("keycap.logical.width", utf8_layout_cluster_logical_width(cluster), 1);
   expect_int("keycap.display.width",
              utf8_layout_cluster_display_width(keycap, sizeof(keycap), cluster), 2);
   expect_int("keycap.cursor.width",
              utf8_layout_cluster_cursor_width(keycap, sizeof(keycap), cluster), 2);
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
   utf8_terminal_profile_apply_line("SET UTF TERMINAL CLASS keycap LAYOUT 2 CURSOR 2");

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

int main(void)
{
   test_keycap_physical_layout();
   test_keycap_viewport_uses_physical_width();

   if (failures != 0)
   {
      fprintf(stderr, "UTF layout tests failed: %d\n", failures);
      return 1;
   }
   return 0;
}
