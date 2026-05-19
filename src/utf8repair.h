#ifndef THE_UTF8REPAIR_H
#define THE_UTF8REPAIR_H

#include "textpos.h"
#include "utf8term.h"

typedef enum
{
   UTF8_REPAIR_EXTENT_CHANGED_CELLS = 0,
   UTF8_REPAIR_EXTENT_SUFFIX,
   UTF8_REPAIR_EXTENT_LINE
} Utf8RepairExtent;

typedef enum
{
   UTF8_REPAIR_FLUSH_NONE = 0,
   UTF8_REPAIR_FLUSH_FAST,
   UTF8_REPAIR_FLUSH_PAUSE
} Utf8RepairFlush;

typedef struct
{
   Utf8TerminalStrategy strategy;
   Utf8TerminalClass feature_class;
   Utf8RepairExtent extent;
   Utf8RepairFlush flush;
   TextPos start_pos;
   int start_valid;
} Utf8RepairPlan;

Utf8RepairPlan utf8_repair_plan_default(TextPos visible_start);
TextPos utf8_repair_visible_start_pos(const CHARTYPE *line, size_t len,
                                      int viewport_col);
TextPos utf8_repair_first_visible_feature_pos(
   const CHARTYPE *line, size_t len, int viewport_col,
   Utf8TerminalClass feature_class, Utf8TerminalIntent intent,
   TextPos fallback);
Utf8RepairPlan utf8_repair_plan_for_cursor(
   const CHARTYPE *line, size_t len, int viewport_col,
   TextCluster old_cluster, int old_valid,
   const Utf8TerminalProfileEntry *old_entry,
   TextCluster new_cluster, int new_valid,
   const Utf8TerminalProfileEntry *new_entry);
Utf8RepairPlan utf8_repair_plan_for_replacement(
   const CHARTYPE *line, size_t len, int viewport_col, TextCellSlice slice,
   Utf8TerminalIntent intent);
int utf8_repair_plan_prefer(const Utf8RepairPlan *candidate,
                            int candidate_start_col,
                            const Utf8RepairPlan *current,
                            int current_start_col);

#endif
