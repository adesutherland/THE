#include <stdio.h>
#include <string.h>

#include "textpos.h"
#include "utfrepair.h"
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

static void expect_size(const char *name, size_t got, size_t want)
{
   if (got != want)
   {
      fprintf(stderr, "%s: got %zu want %zu\n", name, got, want);
      failures++;
   }
}

static TextCluster cluster_at_cell(const CHARTYPE *line, size_t len, int cell)
{
   TextPos pos = textpos_from_cell(line, len, cell, TEXT_SNAP_BACKWARD);

   return textpos_cluster_at_boundary(line, len, pos);
}

static const Utf8TerminalProfileEntry *entry_for_cluster(const CHARTYPE *line,
                                                         size_t len,
                                                         TextCluster cluster)
{
   return utf8_terminal_profile_lookup_cluster(line, len, cluster,
                                               utf8_terminal_display_mode());
}

static void test_cursor_keycap_first_feature(void)
{
#ifdef USE_UTF8PROC
   static const CHARTYPE line[] = {
      'A', '1', 0xEF, 0xB8, 0x8F, 0xE2, 0x83, 0xA3, 'B', ' ',
      'A', '#', 0xEF, 0xB8, 0x8F, 0xE2, 0x83, 0xA3, 'B'
   };
   TextCluster old_cluster;
   TextCluster new_cluster;
   Utf8RepairPlan plan;

   utf8_terminal_profile_reset();
   old_cluster = cluster_at_cell(line, sizeof(line), 0);
   new_cluster = cluster_at_cell(line, sizeof(line), 1);
   plan = utf8_repair_plan_for_cursor(
      line, sizeof(line), 0,
      old_cluster, 1, entry_for_cluster(line, sizeof(line), old_cluster),
      new_cluster, 1, entry_for_cluster(line, sizeof(line), new_cluster));

   expect_int("cursor.keycap.strategy", plan.strategy,
              UTF8_TERM_STRATEGY_CLEAR_FROM_FIRST_CLUSTER_FAST);
   expect_int("cursor.keycap.extent", plan.extent, UTF8_REPAIR_EXTENT_SUFFIX);
   expect_int("cursor.keycap.flush", plan.flush, UTF8_REPAIR_FLUSH_FAST);
   expect_int("cursor.keycap.class", plan.feature_class,
              UTF8_TERM_CLASS_KEYCAP);
   expect_int("cursor.keycap.start.valid", plan.start_valid, 1);
   expect_int("cursor.keycap.start.cell", plan.start_pos.cell_column, 1);
   expect_size("cursor.keycap.start.byte", plan.start_pos.byte_offset, 1);
#endif
}

static void test_cursor_ascii_can_use_first_feature(void)
{
   static const CHARTYPE line[] = "ABCDE";
   size_t len = strlen((const char *)line);
   TextCluster old_cluster;
   TextCluster new_cluster;
   Utf8RepairPlan plan;

   utf8_terminal_profile_reset();
   expect_int("cursor.ascii.apply",
              utf8_terminal_profile_apply_line(
                 "SET UTF TERMINAL CLASS ascii CURSORSTRATEGY first"),
              UTF8_TERMINAL_PROFILE_APPLIED);

   old_cluster = cluster_at_cell(line, len, 3);
   new_cluster = cluster_at_cell(line, len, 4);
   plan = utf8_repair_plan_for_cursor(
      line, len, 1,
      old_cluster, 1, entry_for_cluster(line, len, old_cluster),
      new_cluster, 1, entry_for_cluster(line, len, new_cluster));

   expect_int("cursor.ascii.strategy", plan.strategy,
              UTF8_TERM_STRATEGY_CLEAR_FROM_FIRST_CLUSTER_FAST);
   expect_int("cursor.ascii.class", plan.feature_class, UTF8_TERM_CLASS_ASCII);
   expect_int("cursor.ascii.start.cell", plan.start_pos.cell_column, 1);
   expect_size("cursor.ascii.start.byte", plan.start_pos.byte_offset, 1);
}

static void test_replacement_one_prior_cluster(void)
{
#ifdef USE_UTF8PROC
   static const CHARTYPE line[] = {
      'A', 'B', '1', 0xEF, 0xB8, 0x8F, 0xE2, 0x83, 0xA3, 'C'
   };
   TextCellSlice slice;
   Utf8RepairPlan plan;

   utf8_terminal_profile_reset();
   expect_int("replace.oneprior.apply",
              utf8_terminal_profile_apply_line(
                 "SET UTF TERMINAL CLASS keycap REPLACESTRATEGY prev"),
              UTF8_TERMINAL_PROFILE_APPLIED);

   slice = textpos_slice_cells(line, sizeof(line), 0, 10);
   plan = utf8_repair_plan_for_replacement(
      line, sizeof(line), 0, slice, utf8_terminal_display_mode());

   expect_int("replace.oneprior.strategy", plan.strategy,
              UTF8_TERM_STRATEGY_CLEAR_FROM_ONE_PRIOR_CLUSTER);
   expect_int("replace.oneprior.extent", plan.extent,
              UTF8_REPAIR_EXTENT_SUFFIX);
   expect_int("replace.oneprior.start.cell", plan.start_pos.cell_column, 1);
   expect_size("replace.oneprior.start.byte", plan.start_pos.byte_offset, 1);
#endif
}

static void test_replacement_old_line_can_dominate(void)
{
#ifdef USE_UTF8PROC
   static const CHARTYPE old_line[] = {
      'A', '1', 0xEF, 0xB8, 0x8F, 0xE2, 0x83, 0xA3, 'B'
   };
   static const CHARTYPE new_line[] = "AxxB";
   TextCellSlice old_slice;
   TextCellSlice new_slice;
   Utf8RepairPlan old_plan;
   Utf8RepairPlan new_plan;

   utf8_terminal_profile_reset();
   expect_int("replace.old.apply",
              utf8_terminal_profile_apply_line(
                 "SET UTF TERMINAL CLASS keycap REPLACESTRATEGY first"),
              UTF8_TERMINAL_PROFILE_APPLIED);

   old_slice = textpos_slice_cells(old_line, sizeof(old_line), 0, 10);
   new_slice = textpos_slice_cells(new_line, sizeof(new_line) - 1, 0, 10);
   old_plan = utf8_repair_plan_for_replacement(
      old_line, sizeof(old_line), 0, old_slice, utf8_terminal_display_mode());
   new_plan = utf8_repair_plan_for_replacement(
      new_line, sizeof(new_line) - 1, 0, new_slice,
      utf8_terminal_display_mode());

   expect_int("replace.old.strategy", old_plan.strategy,
              UTF8_TERM_STRATEGY_CLEAR_FROM_FIRST_CLUSTER_FAST);
   expect_int("replace.old.start.cell", old_plan.start_pos.cell_column, 1);
   expect_int("replace.new.strategy", new_plan.strategy,
              UTF8_TERM_STRATEGY_CHANGED_CELLS);
   expect_int("replace.old.prefer",
              utf8_repair_plan_prefer(&old_plan, old_plan.start_pos.cell_column,
                                      &new_plan, new_plan.start_pos.cell_column),
              1);
#endif
}

static void test_replacement_ascii_can_use_first_feature(void)
{
   static const CHARTYPE line[] = "ABCDE";
   size_t len = strlen((const char *)line);
   TextCellSlice slice;
   Utf8RepairPlan plan;

   utf8_terminal_profile_reset();
   expect_int("replace.ascii.apply",
              utf8_terminal_profile_apply_line(
                 "SET UTF TERMINAL CLASS ascii REPLACESTRATEGY first"),
              UTF8_TERMINAL_PROFILE_APPLIED);

   slice = textpos_slice_cells(line, len, 1, 3);
   plan = utf8_repair_plan_for_replacement(
      line, len, 1, slice, utf8_terminal_display_mode());

   expect_int("replace.ascii.strategy", plan.strategy,
              UTF8_TERM_STRATEGY_CLEAR_FROM_FIRST_CLUSTER_FAST);
   expect_int("replace.ascii.class", plan.feature_class,
              UTF8_TERM_CLASS_ASCII);
   expect_int("replace.ascii.start.cell", plan.start_pos.cell_column, 1);
   expect_size("replace.ascii.start.byte", plan.start_pos.byte_offset, 1);
}

int main(void)
{
   test_cursor_keycap_first_feature();
   test_cursor_ascii_can_use_first_feature();
   test_replacement_one_prior_cluster();
   test_replacement_old_line_can_dominate();
   test_replacement_ascii_can_use_first_feature();

   if (failures != 0)
   {
      fprintf(stderr, "%d UTF repair tests failed\n", failures);
      return 1;
   }
   return 0;
}
