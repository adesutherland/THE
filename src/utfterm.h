#ifndef THE_UTF8TERM_H
#define THE_UTF8TERM_H

#include <stddef.h>

#include "textpos.h"
#include "utfcluster.h"
#include "utfterm_defaults.h"

/*
 * UTF terminal profiles describe physical terminal behaviour only.
 * Editor commands, text storage, and macro-visible character movement must
 * continue to use the logical TextPos/utf8proc model.
 */

#define UTF8_TERMINAL_PROFILE_INVALID (-1)
#define UTF8_TERMINAL_PROFILE_IGNORED 0
#define UTF8_TERMINAL_PROFILE_APPLIED 1

typedef enum
{
   UTF8_TERM_DISPLAY_UNKNOWN = -1,
   UTF8_TERM_DISPLAY_NORMAL = 0,
   UTF8_TERM_DISPLAY_DECOMPOSED,
   UTF8_TERM_DISPLAY_SINGLE,
   UTF8_TERM_DISPLAY_COUNT
} Utf8TerminalDisplayMode;

typedef enum
{
   UTF8_TERM_OUTPUT_UNKNOWN = -1,
   UTF8_TERM_OUTPUT_NATIVE = 0,
   UTF8_TERM_OUTPUT_EXPANDED,
   UTF8_TERM_OUTPUT_SUBSTITUTE,
   UTF8_TERM_OUTPUT_BASE,
   UTF8_TERM_OUTPUT_COMPONENTS,
   UTF8_TERM_OUTPUT_SANITIZE,
   UTF8_TERM_OUTPUT_COUNT
} Utf8TerminalOutput;

typedef enum
{
   UTF8_TERM_METRICS_UNKNOWN = -1,
   UTF8_TERM_METRICS_AUTO = 0,
   UTF8_TERM_METRICS_PROFILE,
   UTF8_TERM_METRICS_COMPONENTS,
   UTF8_TERM_METRICS_EXPANDED,
   UTF8_TERM_METRICS_OUTPUT,
   UTF8_TERM_METRICS_COUNT
} Utf8TerminalMetrics;

typedef enum
{
   UTF8_TERM_MARK_UNKNOWN = -1,
   UTF8_TERM_MARK_NONE = 0,
   UTF8_TERM_MARK_COMPRESSED,
   UTF8_TERM_MARK_SUBSTITUTED,
   UTF8_TERM_MARK_UNSAFE,
   UTF8_TERM_MARK_COUNT
} Utf8TerminalMark;

typedef enum
{
   UTF8_TERM_STRATEGY_UNKNOWN = -1,
   UTF8_TERM_STRATEGY_CHANGED_CELLS = 0,
   UTF8_TERM_STRATEGY_LINE,
   UTF8_TERM_STRATEGY_CLEAR_CHANGED_SUFFIX_FAST,
   UTF8_TERM_STRATEGY_CLEAR_FROM_ONE_PRIOR_CLUSTER,
   UTF8_TERM_STRATEGY_CLEAR_FROM_FIRST_CLUSTER_FAST,
   UTF8_TERM_STRATEGY_CLEAR_WHOLE_FAST,
   UTF8_TERM_STRATEGY_COUNT
} Utf8TerminalStrategy;

typedef struct
{
   Utf8TerminalClass feature_class;
   Utf8TerminalDisplayMode display_mode;
   Utf8TerminalOutput output_method;
   Utf8TerminalMetrics metric_method;
   uint32_t substitute_codepoint;
   Utf8TerminalMark mark;
   /*
    * width is the user-visible cluster width reported to logical/UI consumers.
    * advance_width is the terminal grid advance used to place following output.
    * repaint_width is the cleanup footprint used when blanking/repainting
    * stale terminal cells.
    */
   int width;
   int advance_width;
   int cursor_width;
   int repaint_width;
   Utf8TerminalStrategy cursor_strategy;
   Utf8TerminalStrategy replacement_strategy;
} Utf8TerminalProfileEntry;

void utf8_terminal_profile_reset(void);
int utf8_terminal_profile_init_from_environment(void);
int utf8_terminal_profile_apply_terminal_identity(const char *term,
                                                  const char *term_program);
int utf8_terminal_profile_apply_apple_terminal(void);
int utf8_terminal_profile_apply_line(const char *line);
int utf8_terminal_profile_apply_file(const char *path, int *settings_loaded);
Utf8TerminalDisplayMode utf8_terminal_display_mode(void);
int utf8_terminal_set_display_mode(Utf8TerminalDisplayMode display);
Utf8TerminalDisplayMode utf8_terminal_toggle_display_mode(void);
int utf8_terminal_strategy_rank(Utf8TerminalStrategy strategy);
Utf8TerminalStrategy utf8_terminal_cursor_transition_strategy(
   const Utf8TerminalProfileEntry *old_entry,
   const Utf8TerminalProfileEntry *new_entry);
const Utf8TerminalProfileEntry *utf8_terminal_profile_lookup(
   Utf8TerminalClass feature_class, Utf8TerminalDisplayMode display);
Utf8TerminalClass utf8_terminal_classify_cluster(const CHARTYPE *line,
                                                 size_t len,
                                                 TextCluster cluster);
const Utf8TerminalProfileEntry *utf8_terminal_profile_lookup_cluster(
   const CHARTYPE *line, size_t len, TextCluster cluster,
   Utf8TerminalDisplayMode preferred_display);
size_t utf8_terminal_profile_entry_count(void);
const Utf8TerminalProfileEntry *utf8_terminal_profile_entry_at(size_t index);
Utf8TerminalOutput utf8_terminal_resolved_output_for_entry(
   const Utf8TerminalProfileEntry *entry);
Utf8TerminalMetrics utf8_terminal_effective_metrics_for_entry(
   const Utf8TerminalProfileEntry *entry);
int utf8_terminal_profile_entry_canonical(const Utf8TerminalProfileEntry *entry,
                                          char *out, size_t out_size);
int utf8_terminal_profile_canonical_rule_at(size_t index,
                                            char *out, size_t out_size);

Utf8TerminalClass utf8_terminal_class_from_name(const char *name);
Utf8TerminalDisplayMode utf8_terminal_display_from_name(const char *name);
Utf8TerminalOutput utf8_terminal_output_from_name(const char *name);
Utf8TerminalMetrics utf8_terminal_metrics_from_name(const char *name);
Utf8TerminalMark utf8_terminal_mark_from_name(const char *name);
Utf8TerminalStrategy utf8_terminal_strategy_from_name(const char *name);

const char *utf8_terminal_class_name(Utf8TerminalClass feature_class);
const char *utf8_terminal_display_name(Utf8TerminalDisplayMode display);
const char *utf8_terminal_output_name(Utf8TerminalOutput output);
const char *utf8_terminal_metrics_name(Utf8TerminalMetrics metrics);
const char *utf8_terminal_mark_name(Utf8TerminalMark mark);
const char *utf8_terminal_strategy_name(Utf8TerminalStrategy strategy);

#endif
