#ifndef THE_UTF8TERM_H
#define THE_UTF8TERM_H

#include <stddef.h>

#include "textpos.h"
#include "utf8term_defaults.h"

/*
 * UTF-8 terminal profiles describe physical terminal behaviour only.
 * Editor commands, text storage, and macro-visible character movement must
 * continue to use the logical TextPos/utf8proc model.
 */

#define UTF8_TERMINAL_PROFILE_INVALID (-1)
#define UTF8_TERMINAL_PROFILE_IGNORED 0
#define UTF8_TERMINAL_PROFILE_APPLIED 1

typedef enum
{
   UTF8_TERM_CLASS_UNKNOWN = -1,
   UTF8_TERM_CLASS_ASCII = 0,
   UTF8_TERM_CLASS_COMBINING,
   UTF8_TERM_CLASS_COMBINING_STACK,
   UTF8_TERM_CLASS_WIDE,
   UTF8_TERM_CLASS_AMBIGUOUS,
   UTF8_TERM_CLASS_EMOJI,
   UTF8_TERM_CLASS_TEXT_VARIATION,
   UTF8_TERM_CLASS_EMOJI_VARIATION,
   UTF8_TERM_CLASS_MODIFIER,
   UTF8_TERM_CLASS_KEYCAP,
   UTF8_TERM_CLASS_REGIONAL_FLAG,
   UTF8_TERM_CLASS_SHORT_ZWJ,
   UTF8_TERM_CLASS_HEART_ZWJ,
   UTF8_TERM_CLASS_FAMILY_ZWJ,
   UTF8_TERM_CLASS_TAG_FLAG,
   UTF8_TERM_CLASS_PRIVATE_USE,
   UTF8_TERM_CLASS_COUNT
} Utf8TerminalClass;

typedef enum
{
   UTF8_TERM_INTENT_UNKNOWN = -1,
   UTF8_TERM_INTENT_NORMAL = 0,
   UTF8_TERM_INTENT_GROUP,
   UTF8_TERM_INTENT_COMPONENTS,
   UTF8_TERM_INTENT_COUNT
} Utf8TerminalIntent;

typedef enum
{
   UTF8_TERM_OUTPUT_UNKNOWN = -1,
   UTF8_TERM_OUTPUT_NATIVE = 0,
   UTF8_TERM_OUTPUT_EXPANDED,
   UTF8_TERM_OUTPUT_SUBSTITUTE,
   UTF8_TERM_OUTPUT_COUNT
} Utf8TerminalOutput;

typedef enum
{
   UTF8_TERM_STRATEGY_UNKNOWN = -1,
   UTF8_TERM_STRATEGY_CHANGED_CELLS = 0,
   UTF8_TERM_STRATEGY_LINE,
   UTF8_TERM_STRATEGY_CLEAR_CHANGED_SUFFIX_FAST,
   UTF8_TERM_STRATEGY_CLEAR_FROM_FIRST_CLUSTER_FAST,
   UTF8_TERM_STRATEGY_CLEAR_FROM_FIRST_CLUSTER_PAUSE,
   UTF8_TERM_STRATEGY_CLEAR_WHOLE_FAST,
   UTF8_TERM_STRATEGY_CLEAR_FROM_ONE_PRIOR_CLUSTER,
   UTF8_TERM_STRATEGY_COUNT
} Utf8TerminalStrategy;

typedef struct
{
   Utf8TerminalClass feature_class;
   Utf8TerminalIntent display_intent;
   Utf8TerminalOutput output_method;
   uint32_t substitute_codepoint;
   int layout_width;
   int cursor_width;
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
Utf8TerminalIntent utf8_terminal_display_intent(void);
int utf8_terminal_set_display_intent(Utf8TerminalIntent intent);
Utf8TerminalIntent utf8_terminal_toggle_display_intent(void);
int utf8_terminal_strategy_rank(Utf8TerminalStrategy strategy);
Utf8TerminalStrategy utf8_terminal_cursor_transition_strategy(
   const Utf8TerminalProfileEntry *old_entry,
   const Utf8TerminalProfileEntry *new_entry);
const Utf8TerminalProfileEntry *utf8_terminal_profile_lookup(
   Utf8TerminalClass feature_class, Utf8TerminalIntent intent);
Utf8TerminalClass utf8_terminal_classify_cluster(const CHARTYPE *line,
                                                 size_t len,
                                                 TextCluster cluster);
const Utf8TerminalProfileEntry *utf8_terminal_profile_lookup_cluster(
   const CHARTYPE *line, size_t len, TextCluster cluster,
   Utf8TerminalIntent preferred_intent);
size_t utf8_terminal_profile_entry_count(void);
const Utf8TerminalProfileEntry *utf8_terminal_profile_entry_at(size_t index);

Utf8TerminalClass utf8_terminal_class_from_name(const char *name);
Utf8TerminalIntent utf8_terminal_intent_from_name(const char *name);
Utf8TerminalOutput utf8_terminal_output_from_name(const char *name);
Utf8TerminalStrategy utf8_terminal_strategy_from_name(const char *name);

const char *utf8_terminal_class_name(Utf8TerminalClass feature_class);
const char *utf8_terminal_intent_name(Utf8TerminalIntent intent);
const char *utf8_terminal_output_name(Utf8TerminalOutput output);
const char *utf8_terminal_strategy_name(Utf8TerminalStrategy strategy);

#endif
