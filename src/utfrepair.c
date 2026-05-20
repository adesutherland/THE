#include "utfrepair.h"

#include <string.h>

static Utf8RepairFlush flush_for_strategy(Utf8TerminalStrategy strategy)
{
   switch (strategy)
   {
      case UTF8_TERM_STRATEGY_CLEAR_CHANGED_SUFFIX_FAST:
      case UTF8_TERM_STRATEGY_CLEAR_FROM_FIRST_CLUSTER_FAST:
      case UTF8_TERM_STRATEGY_CLEAR_WHOLE_FAST:
      case UTF8_TERM_STRATEGY_CLEAR_FROM_ONE_PRIOR_CLUSTER:
         return UTF8_REPAIR_FLUSH_FAST;

      default:
         return UTF8_REPAIR_FLUSH_NONE;
   }
}

static Utf8RepairExtent extent_for_strategy(Utf8TerminalStrategy strategy)
{
   if (strategy == UTF8_TERM_STRATEGY_LINE)
      return UTF8_REPAIR_EXTENT_LINE;
   if (strategy == UTF8_TERM_STRATEGY_CHANGED_CELLS)
      return UTF8_REPAIR_EXTENT_CHANGED_CELLS;
   return UTF8_REPAIR_EXTENT_SUFFIX;
}

Utf8RepairPlan utf8_repair_plan_default(TextPos visible_start)
{
   Utf8RepairPlan plan;

   memset(&plan, 0, sizeof(plan));
   plan.strategy = UTF8_TERM_STRATEGY_CHANGED_CELLS;
   plan.feature_class = UTF8_TERM_CLASS_UNKNOWN;
   plan.extent = UTF8_REPAIR_EXTENT_CHANGED_CELLS;
   plan.flush = UTF8_REPAIR_FLUSH_NONE;
   plan.start_pos = visible_start;
   plan.start_valid = 0;
   return plan;
}

TextPos utf8_repair_visible_start_pos(const CHARTYPE *line, size_t len,
                                      int viewport_col)
{
   TextPos pos;
   TextCluster cluster;

   if (viewport_col <= 0)
      return textpos_begin();

   pos = textpos_from_cell(line, len, viewport_col, TEXT_SNAP_BACKWARD);
   cluster = textpos_cluster_at_boundary(line, len, pos);
   if (cluster.byte_length != 0
   &&  cluster.pos.cell_column < viewport_col
   &&  cluster.end.cell_column > viewport_col)
      return cluster.end;
   return pos;
}

TextPos utf8_repair_first_visible_feature_pos(
   const CHARTYPE *line, size_t len, int viewport_col,
   Utf8TerminalClass feature_class, Utf8TerminalDisplayMode display,
   TextPos fallback)
{
   TextPos pos = utf8_repair_visible_start_pos(line, len, viewport_col);

   while (pos.byte_offset < len)
   {
      TextCluster cluster = textpos_cluster_at_boundary(line, len, pos);
      const Utf8TerminalProfileEntry *entry;

      if (cluster.byte_length == 0)
         break;
      entry = utf8_terminal_profile_lookup_cluster(line, len, cluster, display);
      if (entry != NULL && entry->feature_class == feature_class)
         return cluster.pos;
      pos = cluster.end;
   }
   return fallback;
}

static TextCluster earliest_cluster(TextCluster old_cluster, int old_valid,
                                    TextCluster new_cluster, int new_valid,
                                    int *valid)
{
   TextCluster earliest;

   memset(&earliest, 0, sizeof(earliest));
   *valid = 0;
   if (old_valid)
   {
      earliest = old_cluster;
      *valid = 1;
   }
   if (new_valid && (!*valid
   ||  new_cluster.pos.byte_offset < earliest.pos.byte_offset))
   {
      earliest = new_cluster;
      *valid = 1;
   }
   return earliest;
}

static TextCluster selected_strategy_cluster(
   TextCluster old_cluster, int old_valid,
   const Utf8TerminalProfileEntry *old_entry,
   TextCluster new_cluster, int new_valid,
   const Utf8TerminalProfileEntry *new_entry,
   Utf8TerminalStrategy strategy,
   const Utf8TerminalProfileEntry **entry_out,
   int *valid)
{
   TextCluster selected;

   memset(&selected, 0, sizeof(selected));
   *entry_out = NULL;
   *valid = 0;
   if (old_valid && old_entry != NULL && old_entry->cursor_strategy == strategy)
   {
      selected = old_cluster;
      *entry_out = old_entry;
      *valid = 1;
   }
   if (new_valid && new_entry != NULL && new_entry->cursor_strategy == strategy
   &&  (!*valid || new_cluster.pos.byte_offset < selected.pos.byte_offset))
   {
      selected = new_cluster;
      *entry_out = new_entry;
      *valid = 1;
   }
   return selected;
}

static TextCluster visible_context_strategy_cluster(
   const CHARTYPE *line, size_t len, int viewport_col,
   int old_logical_col,
   TextCluster old_cluster, int old_valid,
   int new_logical_col,
   TextCluster new_cluster, int new_valid,
   Utf8TerminalStrategy strategy, Utf8TerminalDisplayMode display,
   const Utf8TerminalProfileEntry **entry_out,
   int *valid)
{
   TextPos pos;
   TextCluster selected;
   int limit_cell = -1;
   int best_rank;

   memset(&selected, 0, sizeof(selected));
   *entry_out = NULL;
   *valid = 0;
   if (old_logical_col >= 0)
      limit_cell = old_logical_col;
   else if (old_valid)
      limit_cell = old_cluster.pos.cell_column;
   if (new_logical_col >= 0 && new_logical_col > limit_cell)
      limit_cell = new_logical_col;
   else if (new_logical_col < 0
   &&       new_valid
   &&       new_cluster.pos.cell_column > limit_cell)
      limit_cell = new_cluster.pos.cell_column;
   if (limit_cell < 0)
      return selected;

   best_rank = utf8_terminal_strategy_rank(strategy);
   pos = utf8_repair_visible_start_pos(line, len, viewport_col);
   while (pos.byte_offset < len)
   {
      TextCluster cluster = textpos_cluster_at_boundary(line, len, pos);
      const Utf8TerminalProfileEntry *entry;
      int rank;

      if (cluster.byte_length == 0
      ||  cluster.pos.cell_column > limit_cell)
         break;
      entry = utf8_terminal_profile_lookup_cluster(line, len, cluster, display);
      if (entry != NULL)
      {
         rank = utf8_terminal_strategy_rank(entry->cursor_strategy);
         if (rank > best_rank)
         {
            selected = cluster;
            *entry_out = entry;
            *valid = 1;
            best_rank = rank;
         }
      }
      pos = cluster.end;
   }
   return selected;
}

static TextPos start_for_strategy(const CHARTYPE *line, size_t len,
                                  int viewport_col,
                                  Utf8TerminalStrategy strategy,
                                  TextCluster anchor_cluster,
                                  int anchor_valid,
                                  Utf8TerminalClass feature_class,
                                  int feature_valid,
                                  Utf8TerminalDisplayMode display)
{
   TextPos visible_start;

   visible_start = utf8_repair_visible_start_pos(line, len, viewport_col);
   if (!anchor_valid)
      return visible_start;

   switch (strategy)
   {
      case UTF8_TERM_STRATEGY_LINE:
      case UTF8_TERM_STRATEGY_CLEAR_WHOLE_FAST:
         return visible_start;

      case UTF8_TERM_STRATEGY_CLEAR_FROM_FIRST_CLUSTER_FAST:
         if (feature_valid)
            return utf8_repair_first_visible_feature_pos(
               line, len, viewport_col, feature_class, display,
               anchor_cluster.pos);
         return anchor_cluster.pos;

      case UTF8_TERM_STRATEGY_CLEAR_FROM_ONE_PRIOR_CLUSTER:
      {
         TextPos prior = textpos_prev_cluster(line, len, anchor_cluster.pos);

         if (prior.byte_offset < visible_start.byte_offset)
            return visible_start;
         if (prior.byte_offset < anchor_cluster.pos.byte_offset)
            return prior;
         return anchor_cluster.pos;
      }

      case UTF8_TERM_STRATEGY_CLEAR_CHANGED_SUFFIX_FAST:
      case UTF8_TERM_STRATEGY_CHANGED_CELLS:
      default:
         return anchor_cluster.pos;
   }
}

static void apply_strategy_to_plan(Utf8RepairPlan *plan,
                                   Utf8TerminalStrategy strategy,
                                   Utf8TerminalClass feature_class,
                                   TextPos start_pos,
                                   int start_valid)
{
   plan->strategy = strategy;
   plan->feature_class = feature_class;
   plan->extent = extent_for_strategy(strategy);
   plan->flush = flush_for_strategy(strategy);
   plan->start_pos = start_pos;
   plan->start_valid = start_valid;
}

Utf8RepairPlan utf8_repair_plan_for_cursor(
   const CHARTYPE *line, size_t len, int viewport_col,
   int old_logical_col,
   TextCluster old_cluster, int old_valid,
   const Utf8TerminalProfileEntry *old_entry,
   int new_logical_col,
   TextCluster new_cluster, int new_valid,
   const Utf8TerminalProfileEntry *new_entry)
{
   TextPos visible_start;
   Utf8RepairPlan plan;
   Utf8TerminalStrategy strategy;
   TextCluster earliest;
   TextCluster selected;
   TextCluster context;
   const Utf8TerminalProfileEntry *selected_entry;
   const Utf8TerminalProfileEntry *context_entry;
   Utf8TerminalClass feature_class = UTF8_TERM_CLASS_UNKNOWN;
   int earliest_valid;
   int selected_valid;
   int context_valid;
   TextPos start_pos;
   int context_selected = 0;

   visible_start = utf8_repair_visible_start_pos(line, len, viewport_col);
   plan = utf8_repair_plan_default(visible_start);

   strategy = utf8_terminal_cursor_transition_strategy(old_entry, new_entry);

   earliest = earliest_cluster(old_cluster, old_valid, new_cluster, new_valid,
                               &earliest_valid);
   selected = selected_strategy_cluster(old_cluster, old_valid, old_entry,
                                        new_cluster, new_valid, new_entry,
                                        strategy, &selected_entry,
                                        &selected_valid);
   context = visible_context_strategy_cluster(
      line, len, viewport_col, old_logical_col, old_cluster, old_valid,
      new_logical_col, new_cluster, new_valid,
      strategy, utf8_terminal_display_mode(), &context_entry, &context_valid);
   if (context_valid && context_entry != NULL)
   {
      strategy = context_entry->cursor_strategy;
      selected = context;
      selected_entry = context_entry;
      selected_valid = 1;
      context_selected = 1;
   }

   if (selected_valid && selected_entry != NULL)
      feature_class = selected_entry->feature_class;
   else if (old_entry != NULL && old_entry->cursor_strategy == strategy)
      feature_class = old_entry->feature_class;
   else if (new_entry != NULL && new_entry->cursor_strategy == strategy)
      feature_class = new_entry->feature_class;

   if (strategy == UTF8_TERM_STRATEGY_CLEAR_FROM_FIRST_CLUSTER_FAST)
   {
      start_pos = start_for_strategy(line, len, viewport_col, strategy,
                                     selected_valid ? selected : earliest,
                                     selected_valid || earliest_valid,
                                     feature_class,
                                     feature_class != UTF8_TERM_CLASS_UNKNOWN,
                                     utf8_terminal_display_mode());
   }
   else
   {
      start_pos = start_for_strategy(line, len, viewport_col, strategy,
                                     context_selected ? selected : earliest,
                                     context_selected ? selected_valid
                                                      : earliest_valid,
                                     feature_class,
                                     feature_class != UTF8_TERM_CLASS_UNKNOWN,
                                     utf8_terminal_display_mode());
   }

   apply_strategy_to_plan(&plan, strategy, feature_class, start_pos,
                          earliest_valid || selected_valid
                       || strategy == UTF8_TERM_STRATEGY_LINE
                       || strategy == UTF8_TERM_STRATEGY_CLEAR_WHOLE_FAST);
   return plan;
}

Utf8RepairPlan utf8_repair_plan_for_replacement(
   const CHARTYPE *line, size_t len, int viewport_col, TextCellSlice slice,
   Utf8TerminalDisplayMode display)
{
   TextPos visible_start;
   TextPos pos;
   Utf8RepairPlan plan;
   TextCluster selected_cluster;
   const Utf8TerminalProfileEntry *selected_entry = NULL;
   Utf8TerminalStrategy selected_strategy = UTF8_TERM_STRATEGY_CHANGED_CELLS;
   int selected_valid = 0;
   TextPos start_pos;

   visible_start = utf8_repair_visible_start_pos(line, len, viewport_col);
   plan = utf8_repair_plan_default(visible_start);
   pos = slice.start;

   memset(&selected_cluster, 0, sizeof(selected_cluster));
   while (pos.byte_offset < slice.end.byte_offset)
   {
      TextCluster cluster = textpos_cluster_at_boundary(line, len, pos);
      const Utf8TerminalProfileEntry *entry;

      if (cluster.byte_length == 0)
         break;
      entry = utf8_terminal_profile_lookup_cluster(line, len, cluster, display);
      if (entry != NULL
      &&  utf8_terminal_strategy_rank(entry->replacement_strategy)
        > utf8_terminal_strategy_rank(selected_strategy))
      {
         selected_strategy = entry->replacement_strategy;
         selected_entry = entry;
         selected_cluster = cluster;
         selected_valid = 1;
      }
      pos = cluster.end;
   }

   if (!selected_valid)
      return plan;

   start_pos = start_for_strategy(line, len, viewport_col, selected_strategy,
                                  selected_cluster, selected_valid,
                                  selected_entry->feature_class, 1, display);
   apply_strategy_to_plan(&plan, selected_strategy,
                          selected_entry->feature_class, start_pos, 1);
   return plan;
}

int utf8_repair_plan_prefer(const Utf8RepairPlan *candidate,
                            int candidate_start_col,
                            const Utf8RepairPlan *current,
                            int current_start_col)
{
   int candidate_rank;
   int current_rank;

   if (candidate == NULL || current == NULL)
      return 0;
   candidate_rank = utf8_terminal_strategy_rank(candidate->strategy);
   current_rank = utf8_terminal_strategy_rank(current->strategy);
   if (candidate_rank > current_rank)
      return 1;
   if (candidate_rank < current_rank)
      return 0;
   if (candidate_start_col < 0)
      return 0;
   if (current_start_col < 0)
      return 1;
   return candidate_start_col < current_start_col;
}
