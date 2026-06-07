#include "utfterm.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "utfcluster.h"

#define UTF8_TERM_TOKEN_MAX 96
#define UTF8_TERM_MAX_TOKENS 32

typedef struct
{
   const char *name;
   Utf8TerminalClass value;
} ClassName;

typedef struct
{
   const char *name;
   Utf8TerminalDisplayMode value;
} DisplayName;

typedef struct
{
   const char *name;
   Utf8TerminalOutput value;
} OutputName;

typedef struct
{
   const char *name;
   Utf8TerminalMetrics value;
} MetricsName;

typedef struct
{
   const char *name;
   Utf8TerminalMark value;
} MarkName;

typedef struct
{
   const char *name;
   Utf8TerminalDisplayStrategy value;
} DisplayStrategyName;

typedef struct
{
   const char *name;
   Utf8TerminalStrategy value;
} StrategyName;

static const ClassName class_names[] =
{
   { "ascii", UTF8_TERM_CLASS_ASCII },
   { "combining", UTF8_TERM_CLASS_COMBINING },
   { "combining-stack", UTF8_TERM_CLASS_COMBINING_STACK },
   { "wide", UTF8_TERM_CLASS_WIDE },
   { "ambiguous", UTF8_TERM_CLASS_AMBIGUOUS },
   { "emoji", UTF8_TERM_CLASS_EMOJI },
   { "text-variation", UTF8_TERM_CLASS_TEXT_VARIATION },
   { "emoji-variation", UTF8_TERM_CLASS_EMOJI_VARIATION },
   { "modifier", UTF8_TERM_CLASS_MODIFIER },
   { "keycap", UTF8_TERM_CLASS_KEYCAP },
   { "regional-indicator", UTF8_TERM_CLASS_REGIONAL_INDICATOR },
   { "regional-flag", UTF8_TERM_CLASS_REGIONAL_FLAG },
   { "short-zwj", UTF8_TERM_CLASS_SHORT_ZWJ },
   { "heart-zwj", UTF8_TERM_CLASS_HEART_ZWJ },
   { "family-zwj", UTF8_TERM_CLASS_FAMILY_ZWJ },
   { "tag-flag", UTF8_TERM_CLASS_TAG_FLAG },
   { "private-use", UTF8_TERM_CLASS_PRIVATE_USE },
   { NULL, UTF8_TERM_CLASS_UNKNOWN }
};

static const DisplayName display_names[] =
{
   { "normal", UTF8_TERM_DISPLAY_NORMAL },
   { "decomposed", UTF8_TERM_DISPLAY_DECOMPOSED },
   { "single", UTF8_TERM_DISPLAY_SINGLE },
   { NULL, UTF8_TERM_DISPLAY_UNKNOWN }
};

static const OutputName output_names[] =
{
   { "native", UTF8_TERM_OUTPUT_NATIVE },
   { "expanded", UTF8_TERM_OUTPUT_EXPANDED },
   { "substitute", UTF8_TERM_OUTPUT_SUBSTITUTE },
   { "base", UTF8_TERM_OUTPUT_BASE },
   { "components", UTF8_TERM_OUTPUT_COMPONENTS },
   { "sanitize", UTF8_TERM_OUTPUT_SANITIZE },
   { NULL, UTF8_TERM_OUTPUT_UNKNOWN }
};

static const MetricsName metrics_names[] =
{
   { "auto", UTF8_TERM_METRICS_AUTO },
   { "profile", UTF8_TERM_METRICS_PROFILE },
   { "components", UTF8_TERM_METRICS_COMPONENTS },
   { "expanded", UTF8_TERM_METRICS_EXPANDED },
   { "output", UTF8_TERM_METRICS_OUTPUT },
   { "fixed", UTF8_TERM_METRICS_PROFILE },
   { "native", UTF8_TERM_METRICS_PROFILE },
   { NULL, UTF8_TERM_METRICS_UNKNOWN }
};

static const MarkName mark_names[] =
{
   { "none", UTF8_TERM_MARK_NONE },
   { "compressed", UTF8_TERM_MARK_COMPRESSED },
   { "substituted", UTF8_TERM_MARK_SUBSTITUTED },
   { "unsafe", UTF8_TERM_MARK_UNSAFE },
   { NULL, UTF8_TERM_MARK_UNKNOWN }
};

static const DisplayStrategyName display_strategy_names[] =
{
   { "inline", UTF8_TERM_DISPLAY_STRATEGY_INLINE },
   { "default", UTF8_TERM_DISPLAY_STRATEGY_INLINE },
   { "isolate", UTF8_TERM_DISPLAY_STRATEGY_ISOLATE },
   { NULL, UTF8_TERM_DISPLAY_STRATEGY_UNKNOWN }
};

static const StrategyName strategy_names[] =
{
   { "cells", UTF8_TERM_STRATEGY_CHANGED_CELLS },
   { "line", UTF8_TERM_STRATEGY_LINE },
   { "suffix", UTF8_TERM_STRATEGY_CLEAR_CHANGED_SUFFIX_FAST },
   { "prev", UTF8_TERM_STRATEGY_CLEAR_FROM_ONE_PRIOR_CLUSTER },
   { "first", UTF8_TERM_STRATEGY_CLEAR_FROM_FIRST_CLUSTER_FAST },
   { "whole", UTF8_TERM_STRATEGY_CLEAR_WHOLE_FAST },
   { NULL, UTF8_TERM_STRATEGY_UNKNOWN }
};

#define UTF8_TERM_DEFAULT_ENTRY(feature_class, feature_class_name, display_mode, display_mode_name, output_method, output_method_name, substitute_codepoint, width, advance_width, cursor_width, repaint_width, cursor_strategy, cursor_strategy_name, replacement_strategy, replacement_strategy_name) \
   { feature_class, display_mode, output_method, UTF8_TERM_METRICS_AUTO, substitute_codepoint, (output_method == UTF8_TERM_OUTPUT_SUBSTITUTE) ? UTF8_TERM_MARK_SUBSTITUTED : UTF8_TERM_MARK_NONE, UTF8_TERM_DISPLAY_STRATEGY_INLINE, width, advance_width, cursor_width, repaint_width, cursor_strategy, replacement_strategy },

static const Utf8TerminalProfileEntry default_entries[] =
{
   UTF8_TERMINAL_DEFAULT_PROFILE_ENTRIES(UTF8_TERM_DEFAULT_ENTRY)
};

#undef UTF8_TERM_DEFAULT_ENTRY

static Utf8TerminalProfileEntry profile_entries[
   sizeof(default_entries) / sizeof(default_entries[0])
];
static int profile_initialised = 0;
static Utf8TerminalDisplayMode display_mode = UTF8_TERM_DISPLAY_NORMAL;

static int ascii_equal_ci(const char *left, const char *right)
{
   unsigned char lc;
   unsigned char rc;

   if (left == NULL || right == NULL)
      return 0;
   while (*left != '\0' && *right != '\0')
   {
      lc = (unsigned char)*left++;
      rc = (unsigned char)*right++;
      if (tolower(lc) != tolower(rc))
         return 0;
   }
   return *left == '\0' && *right == '\0';
}

static void ensure_profile_initialised(void)
{
   if (!profile_initialised)
      utf8_terminal_profile_reset();
}

void utf8_terminal_profile_reset(void)
{
   memcpy(profile_entries, default_entries, sizeof(default_entries));
   display_mode = UTF8_TERM_DISPLAY_NORMAL;
   profile_initialised = 1;
}

Utf8TerminalDisplayMode utf8_terminal_display_mode(void)
{
   ensure_profile_initialised();
   return display_mode;
}

int utf8_terminal_set_display_mode(Utf8TerminalDisplayMode display)
{
   ensure_profile_initialised();
   if (display != UTF8_TERM_DISPLAY_NORMAL
   &&  display != UTF8_TERM_DISPLAY_DECOMPOSED
   &&  display != UTF8_TERM_DISPLAY_SINGLE)
      return UTF8_TERMINAL_PROFILE_INVALID;
   display_mode = display;
   return UTF8_TERMINAL_PROFILE_APPLIED;
}

Utf8TerminalDisplayMode utf8_terminal_toggle_display_mode(void)
{
   ensure_profile_initialised();
   if (display_mode == UTF8_TERM_DISPLAY_NORMAL)
      display_mode = UTF8_TERM_DISPLAY_DECOMPOSED;
   else if (display_mode == UTF8_TERM_DISPLAY_DECOMPOSED)
      display_mode = UTF8_TERM_DISPLAY_SINGLE;
   else
      display_mode = UTF8_TERM_DISPLAY_NORMAL;
   return display_mode;
}

int utf8_terminal_strategy_rank(Utf8TerminalStrategy strategy)
{
   switch (strategy)
   {
      case UTF8_TERM_STRATEGY_CHANGED_CELLS:
         return 0;
      case UTF8_TERM_STRATEGY_LINE:
         return 1;
      case UTF8_TERM_STRATEGY_CLEAR_CHANGED_SUFFIX_FAST:
         return 2;
      case UTF8_TERM_STRATEGY_CLEAR_FROM_ONE_PRIOR_CLUSTER:
         return 3;
      case UTF8_TERM_STRATEGY_CLEAR_FROM_FIRST_CLUSTER_FAST:
         return 4;
      case UTF8_TERM_STRATEGY_CLEAR_WHOLE_FAST:
         return 5;
      default:
         return 0;
   }
}

Utf8TerminalStrategy utf8_terminal_cursor_transition_strategy(
   const Utf8TerminalProfileEntry *old_entry,
   const Utf8TerminalProfileEntry *new_entry)
{
   Utf8TerminalStrategy old_strategy = UTF8_TERM_STRATEGY_CHANGED_CELLS;
   Utf8TerminalStrategy new_strategy = UTF8_TERM_STRATEGY_CHANGED_CELLS;

   if (old_entry != NULL)
      old_strategy = old_entry->cursor_strategy;
   if (new_entry != NULL)
      new_strategy = new_entry->cursor_strategy;
   if (utf8_terminal_strategy_rank(new_strategy)
    >  utf8_terminal_strategy_rank(old_strategy))
      return new_strategy;
   return old_strategy;
}

size_t utf8_terminal_profile_entry_count(void)
{
   return sizeof(profile_entries) / sizeof(profile_entries[0]);
}

const Utf8TerminalProfileEntry *utf8_terminal_profile_entry_at(size_t index)
{
   ensure_profile_initialised();
   if (index >= utf8_terminal_profile_entry_count())
      return NULL;
   return &profile_entries[index];
}

Utf8TerminalOutput utf8_terminal_resolved_output_for_entry(
   const Utf8TerminalProfileEntry *entry)
{
   if (entry == NULL)
      return UTF8_TERM_OUTPUT_NATIVE;
   if (entry->output_method != UTF8_TERM_OUTPUT_SANITIZE)
      return entry->output_method;

   switch (entry->feature_class)
   {
      case UTF8_TERM_CLASS_KEYCAP:
      case UTF8_TERM_CLASS_REGIONAL_INDICATOR:
      case UTF8_TERM_CLASS_REGIONAL_FLAG:
      case UTF8_TERM_CLASS_TEXT_VARIATION:
      case UTF8_TERM_CLASS_EMOJI_VARIATION:
      case UTF8_TERM_CLASS_MODIFIER:
         return UTF8_TERM_OUTPUT_BASE;

      case UTF8_TERM_CLASS_SHORT_ZWJ:
      case UTF8_TERM_CLASS_HEART_ZWJ:
      case UTF8_TERM_CLASS_FAMILY_ZWJ:
         return UTF8_TERM_OUTPUT_COMPONENTS;

      case UTF8_TERM_CLASS_TAG_FLAG:
         return UTF8_TERM_OUTPUT_SUBSTITUTE;

      default:
         return UTF8_TERM_OUTPUT_NATIVE;
   }
}

Utf8TerminalMetrics utf8_terminal_effective_metrics_for_entry(
   const Utf8TerminalProfileEntry *entry)
{
   if (entry == NULL)
      return UTF8_TERM_METRICS_PROFILE;
   if (entry->metric_method != UTF8_TERM_METRICS_AUTO)
      return entry->metric_method;
   if (entry->output_method == UTF8_TERM_OUTPUT_SANITIZE)
      return UTF8_TERM_METRICS_OUTPUT;
   if (entry->output_method == UTF8_TERM_OUTPUT_COMPONENTS
   ||  entry->output_method == UTF8_TERM_OUTPUT_EXPANDED)
      return UTF8_TERM_METRICS_COMPONENTS;
   return UTF8_TERM_METRICS_PROFILE;
}

static Utf8TerminalProfileEntry *profile_entry_for(Utf8TerminalClass feature_class,
                                                   Utf8TerminalDisplayMode display)
{
   size_t i;

   ensure_profile_initialised();
   for (i = 0; i < utf8_terminal_profile_entry_count(); i++)
   {
      if (profile_entries[i].feature_class == feature_class
      &&  profile_entries[i].display_mode == display)
         return &profile_entries[i];
   }
   return NULL;
}

const Utf8TerminalProfileEntry *utf8_terminal_profile_lookup(
   Utf8TerminalClass feature_class, Utf8TerminalDisplayMode display)
{
   return profile_entry_for(feature_class, display);
}

Utf8TerminalClass utf8_terminal_classify_cluster(const CHARTYPE *line,
                                                 size_t len,
                                                 TextCluster cluster)
{
   return utf8_cluster_classify(line, len, cluster);
}

const Utf8TerminalProfileEntry *utf8_terminal_profile_lookup_cluster(
   const CHARTYPE *line, size_t len, TextCluster cluster,
   Utf8TerminalDisplayMode preferred_display)
{
   Utf8TerminalClass feature_class;
   Utf8TerminalDisplayMode display = preferred_display;
   const Utf8TerminalProfileEntry *entry;

   feature_class = utf8_terminal_classify_cluster(line, len, cluster);
   if (feature_class == UTF8_TERM_CLASS_UNKNOWN)
      return NULL;
   if (display == UTF8_TERM_DISPLAY_UNKNOWN)
      display = UTF8_TERM_DISPLAY_NORMAL;

   entry = utf8_terminal_profile_lookup(feature_class, display);
   if (entry != NULL)
      return entry;
   return NULL;
}

static int parse_positive_int(const char *token, int *out)
{
   char *end = NULL;
   long parsed;

   if (token == NULL || *token == '\0' || out == NULL)
      return 0;
   parsed = strtol(token, &end, 10);
   if (end == token || *end != '\0' || parsed <= 0 || parsed > 1024)
      return 0;
   *out = (int)parsed;
   return 1;
}

static int parse_codepoint(const char *token, uint32_t *out)
{
   const char *p = token;
   char *end = NULL;
   unsigned long parsed;

   if (token == NULL || *token == '\0' || out == NULL)
      return 0;
   if ((p[0] == 'U' || p[0] == 'u') && p[1] == '+')
      p += 2;
   else if (p[0] == '0' && (p[1] == 'x' || p[1] == 'X'))
      p += 2;
   parsed = strtoul(p, &end, 16);
   if (end == p || *end != '\0' || parsed == 0 || parsed > 0x10FFFFul)
      return 0;
   if (parsed >= 0xD800ul && parsed <= 0xDFFFul)
      return 0;
   *out = (uint32_t)parsed;
   return 1;
}

static int tokenize_line(const char *line,
                         char tokens[UTF8_TERM_MAX_TOKENS][UTF8_TERM_TOKEN_MAX])
{
   char quoted[512];
   const unsigned char *p = (const unsigned char *)line;
   int count = 0;

   if (line == NULL)
      return UTF8_TERMINAL_PROFILE_IGNORED;
   while (*p != '\0' && isspace(*p))
      p++;
   if (*p == '\0' || *p == '*' || *p == '#'
   ||  (p[0] == '/' && p[1] == '*'))
      return UTF8_TERMINAL_PROFILE_IGNORED;
   if (*p == '\'' || *p == '"')
   {
      unsigned char quote = *p++;
      size_t len = 0;

      while (*p != '\0' && *p != quote)
      {
         if (len >= sizeof(quoted) - 1)
            return UTF8_TERMINAL_PROFILE_INVALID;
         quoted[len++] = (char)*p++;
      }
      if (*p != quote)
         return UTF8_TERMINAL_PROFILE_INVALID;
      quoted[len] = '\0';
      p = (const unsigned char *)quoted;
      while (*p != '\0' && isspace(*p))
         p++;
      if (*p == '\0')
         return UTF8_TERMINAL_PROFILE_IGNORED;
   }

   while (*p != '\0')
   {
      int len = 0;

      if (count >= UTF8_TERM_MAX_TOKENS)
         return UTF8_TERMINAL_PROFILE_INVALID;
      while (*p != '\0' && !isspace(*p))
      {
         if (len >= UTF8_TERM_TOKEN_MAX - 1)
            return UTF8_TERMINAL_PROFILE_INVALID;
         tokens[count][len++] = (char)*p++;
      }
      tokens[count][len] = '\0';
      count++;
      while (*p != '\0' && isspace(*p))
         p++;
   }
   return count;
}

Utf8TerminalClass utf8_terminal_class_from_name(const char *name)
{
   size_t i;

   for (i = 0; class_names[i].name != NULL; i++)
   {
      if (ascii_equal_ci(class_names[i].name, name))
         return class_names[i].value;
   }
   return UTF8_TERM_CLASS_UNKNOWN;
}

Utf8TerminalDisplayMode utf8_terminal_display_from_name(const char *name)
{
   size_t i;

   for (i = 0; display_names[i].name != NULL; i++)
   {
      if (ascii_equal_ci(display_names[i].name, name))
         return display_names[i].value;
   }
   return UTF8_TERM_DISPLAY_UNKNOWN;
}

Utf8TerminalOutput utf8_terminal_output_from_name(const char *name)
{
   size_t i;

   for (i = 0; output_names[i].name != NULL; i++)
   {
      if (ascii_equal_ci(output_names[i].name, name))
         return output_names[i].value;
   }
   return UTF8_TERM_OUTPUT_UNKNOWN;
}

Utf8TerminalMetrics utf8_terminal_metrics_from_name(const char *name)
{
   size_t i;

   for (i = 0; metrics_names[i].name != NULL; i++)
   {
      if (ascii_equal_ci(metrics_names[i].name, name))
         return metrics_names[i].value;
   }
   return UTF8_TERM_METRICS_UNKNOWN;
}

Utf8TerminalMark utf8_terminal_mark_from_name(const char *name)
{
   size_t i;

   for (i = 0; mark_names[i].name != NULL; i++)
   {
      if (ascii_equal_ci(mark_names[i].name, name))
         return mark_names[i].value;
   }
   return UTF8_TERM_MARK_UNKNOWN;
}

Utf8TerminalDisplayStrategy utf8_terminal_display_strategy_from_name(
   const char *name)
{
   size_t i;

   for (i = 0; display_strategy_names[i].name != NULL; i++)
   {
      if (ascii_equal_ci(display_strategy_names[i].name, name))
         return display_strategy_names[i].value;
   }
   return UTF8_TERM_DISPLAY_STRATEGY_UNKNOWN;
}

Utf8TerminalStrategy utf8_terminal_strategy_from_name(const char *name)
{
   size_t i;

   for (i = 0; strategy_names[i].name != NULL; i++)
   {
      if (ascii_equal_ci(strategy_names[i].name, name))
         return strategy_names[i].value;
   }
   return UTF8_TERM_STRATEGY_UNKNOWN;
}

const char *utf8_terminal_class_name(Utf8TerminalClass feature_class)
{
   size_t i;

   for (i = 0; class_names[i].name != NULL; i++)
   {
      if (class_names[i].value == feature_class)
         return class_names[i].name;
   }
   return "unknown";
}

const char *utf8_terminal_display_name(Utf8TerminalDisplayMode display)
{
   size_t i;

   for (i = 0; display_names[i].name != NULL; i++)
   {
      if (display_names[i].value == display)
         return display_names[i].name;
   }
   return "unknown";
}

const char *utf8_terminal_output_name(Utf8TerminalOutput output)
{
   size_t i;

   for (i = 0; output_names[i].name != NULL; i++)
   {
      if (output_names[i].value == output)
         return output_names[i].name;
   }
   return "unknown";
}

const char *utf8_terminal_metrics_name(Utf8TerminalMetrics metrics)
{
   size_t i;

   for (i = 0; metrics_names[i].name != NULL; i++)
   {
      if (metrics_names[i].value == metrics)
         return metrics_names[i].name;
   }
   return "unknown";
}

const char *utf8_terminal_mark_name(Utf8TerminalMark mark)
{
   size_t i;

   for (i = 0; mark_names[i].name != NULL; i++)
   {
      if (mark_names[i].value == mark)
         return mark_names[i].name;
   }
   return "unknown";
}

const char *utf8_terminal_display_strategy_name(
   Utf8TerminalDisplayStrategy strategy)
{
   size_t i;

   for (i = 0; display_strategy_names[i].name != NULL; i++)
   {
      if (display_strategy_names[i].value == strategy)
         return display_strategy_names[i].name;
   }
   return "unknown";
}

const char *utf8_terminal_strategy_name(Utf8TerminalStrategy strategy)
{
   switch (strategy)
   {
      case UTF8_TERM_STRATEGY_CHANGED_CELLS:
         return "cells";
      case UTF8_TERM_STRATEGY_LINE:
         return "line";
      case UTF8_TERM_STRATEGY_CLEAR_CHANGED_SUFFIX_FAST:
         return "suffix";
      case UTF8_TERM_STRATEGY_CLEAR_FROM_ONE_PRIOR_CLUSTER:
         return "prev";
      case UTF8_TERM_STRATEGY_CLEAR_FROM_FIRST_CLUSTER_FAST:
         return "first";
      case UTF8_TERM_STRATEGY_CLEAR_WHOLE_FAST:
         return "whole";
      default:
         return "unknown";
   }
}

static Utf8TerminalOutput coerce_output_for_display(Utf8TerminalDisplayMode display,
                                                   Utf8TerminalOutput output)
{
   if (output == UTF8_TERM_OUTPUT_SUBSTITUTE)
      return UTF8_TERM_OUTPUT_SUBSTITUTE;
   if (output == UTF8_TERM_OUTPUT_BASE)
      return output;
   if (output == UTF8_TERM_OUTPUT_SANITIZE
   &&  display != UTF8_TERM_DISPLAY_SINGLE)
      return output;
   if (display == UTF8_TERM_DISPLAY_NORMAL)
   {
      if (output == UTF8_TERM_OUTPUT_NATIVE
      ||  output == UTF8_TERM_OUTPUT_EXPANDED
      ||  output == UTF8_TERM_OUTPUT_COMPONENTS)
         return output;
      return UTF8_TERM_OUTPUT_NATIVE;
   }
   if (display == UTF8_TERM_DISPLAY_DECOMPOSED)
   {
      if (output == UTF8_TERM_OUTPUT_NATIVE
      ||  output == UTF8_TERM_OUTPUT_EXPANDED
      ||  output == UTF8_TERM_OUTPUT_COMPONENTS)
         return output;
      return UTF8_TERM_OUTPUT_EXPANDED;
   }
   if (display == UTF8_TERM_DISPLAY_SINGLE)
   {
      if (output == UTF8_TERM_OUTPUT_NATIVE
      ||  output == UTF8_TERM_OUTPUT_BASE)
         return output;
      return UTF8_TERM_OUTPUT_SUBSTITUTE;
   }
   return UTF8_TERM_OUTPUT_NATIVE;
}

static int display_accepts_width(Utf8TerminalDisplayMode display, int width)
{
   return display != UTF8_TERM_DISPLAY_SINGLE || width == 1;
}

static int display_accepts_metrics(Utf8TerminalDisplayMode display,
                                   Utf8TerminalMetrics metrics)
{
   if (metrics == UTF8_TERM_METRICS_UNKNOWN)
      return 0;
   return display != UTF8_TERM_DISPLAY_SINGLE
       || metrics == UTF8_TERM_METRICS_AUTO
       || metrics == UTF8_TERM_METRICS_PROFILE
       || metrics == UTF8_TERM_METRICS_OUTPUT;
}

static void apply_substitute_defaults(Utf8TerminalProfileEntry *entry)
{
   entry->metric_method = UTF8_TERM_METRICS_AUTO;
   entry->width = 1;
   entry->advance_width = 1;
   entry->cursor_width = 1;
   entry->repaint_width = 1;
   entry->cursor_strategy = UTF8_TERM_STRATEGY_CHANGED_CELLS;
   entry->replacement_strategy = UTF8_TERM_STRATEGY_CHANGED_CELLS;
   entry->mark = UTF8_TERM_MARK_SUBSTITUTED;
}

static int apply_output(Utf8TerminalProfileEntry *entry, Utf8TerminalOutput output,
                        uint32_t substitute_codepoint,
                        int has_substitute_codepoint)
{
   Utf8TerminalOutput coerced_output;

   if (entry == NULL || output == UTF8_TERM_OUTPUT_UNKNOWN)
      return UTF8_TERMINAL_PROFILE_INVALID;
   if (has_substitute_codepoint && output != UTF8_TERM_OUTPUT_SUBSTITUTE)
      return UTF8_TERMINAL_PROFILE_INVALID;
   coerced_output = coerce_output_for_display(entry->display_mode, output);
   if (has_substitute_codepoint && coerced_output != UTF8_TERM_OUTPUT_SUBSTITUTE)
      return UTF8_TERMINAL_PROFILE_INVALID;
   entry->output_method = coerced_output;
   if (entry->output_method == UTF8_TERM_OUTPUT_SUBSTITUTE)
   {
      apply_substitute_defaults(entry);
      if (has_substitute_codepoint)
         entry->substitute_codepoint = substitute_codepoint;
   }
   else if (entry->output_method == UTF8_TERM_OUTPUT_SANITIZE)
   {
      if (entry->display_mode == UTF8_TERM_DISPLAY_NORMAL)
         entry->metric_method = UTF8_TERM_METRICS_OUTPUT;
      if (entry->mark == UTF8_TERM_MARK_SUBSTITUTED)
         entry->mark = UTF8_TERM_MARK_NONE;
   }
   else if (entry->mark == UTF8_TERM_MARK_SUBSTITUTED)
      entry->mark = UTF8_TERM_MARK_NONE;
   return UTF8_TERMINAL_PROFILE_APPLIED;
}

static int token_is_setting_keyword(const char *token)
{
   return ascii_equal_ci(token, "output")
       || ascii_equal_ci(token, "metrics")
       || ascii_equal_ci(token, "width")
       || ascii_equal_ci(token, "advance")
       || ascii_equal_ci(token, "cursor")
       || ascii_equal_ci(token, "repaint")
       || ascii_equal_ci(token, "mark")
       || ascii_equal_ci(token, "displaystrategy")
       || ascii_equal_ci(token, "cursorstrategy")
       || ascii_equal_ci(token, "replacestrategy");
}

static int parse_output_setting(
   Utf8TerminalProfileEntry *entry,
   char tokens[UTF8_TERM_MAX_TOKENS][UTF8_TERM_TOKEN_MAX],
   int count, int *index)
{
   Utf8TerminalOutput output;
   uint32_t substitute_codepoint = 0;
   int has_substitute_codepoint = 0;
   int method_index;

   if (entry == NULL || tokens == NULL || index == NULL)
      return UTF8_TERMINAL_PROFILE_INVALID;
   method_index = *index + 1;
   if (method_index >= count)
      return UTF8_TERMINAL_PROFILE_INVALID;

   if (ascii_equal_ci(tokens[method_index], "replacement"))
   {
      output = UTF8_TERM_OUTPUT_SUBSTITUTE;
      *index = method_index + 1;
      if (*index < count && !token_is_setting_keyword(tokens[*index]))
      {
         if (ascii_equal_ci(tokens[*index], "default"))
            (*index)++;
         else if (ascii_equal_ci(tokens[*index], "base"))
         {
            output = UTF8_TERM_OUTPUT_BASE;
            (*index)++;
         }
         else if (parse_codepoint(tokens[*index], &substitute_codepoint))
         {
            has_substitute_codepoint = 1;
            (*index)++;
         }
         else
            return UTF8_TERMINAL_PROFILE_INVALID;
      }
   }
   else if (ascii_equal_ci(tokens[method_index], "characters"))
   {
      output = UTF8_TERM_OUTPUT_COMPONENTS;
      *index = method_index + 1;
   }
   else
   {
      output = utf8_terminal_output_from_name(tokens[method_index]);
      if (output == UTF8_TERM_OUTPUT_UNKNOWN)
         return UTF8_TERMINAL_PROFILE_INVALID;
      *index = method_index + 1;
      if (output == UTF8_TERM_OUTPUT_SUBSTITUTE)
      {
         if (*index < count && !token_is_setting_keyword(tokens[*index]))
         {
            if (!parse_codepoint(tokens[*index], &substitute_codepoint))
               return UTF8_TERMINAL_PROFILE_INVALID;
            has_substitute_codepoint = 1;
            (*index)++;
         }
      }
      else if (output == UTF8_TERM_OUTPUT_SANITIZE)
      {
         while (*index < count && !token_is_setting_keyword(tokens[*index]))
            (*index)++;
      }
      else if (*index < count && !token_is_setting_keyword(tokens[*index]))
         return UTF8_TERMINAL_PROFILE_INVALID;
   }

   return apply_output(entry, output, substitute_codepoint,
                       has_substitute_codepoint);
}

static int apply_profile_settings_to_entry(
   Utf8TerminalProfileEntry *entry,
   char tokens[UTF8_TERM_MAX_TOKENS][UTF8_TERM_TOKEN_MAX],
   int count, int index)
{
   int settings = 0;

   if (entry == NULL || tokens == NULL || index >= count)
      return UTF8_TERMINAL_PROFILE_INVALID;

   while (index < count)
   {
      if (ascii_equal_ci(tokens[index], "output"))
      {
         int rc = parse_output_setting(entry, tokens, count, &index);

         if (rc != UTF8_TERMINAL_PROFILE_APPLIED)
            return rc;
         settings++;
         continue;
      }
      if (ascii_equal_ci(tokens[index], "metrics"))
      {
         Utf8TerminalMetrics metrics;

         if (index + 1 >= count)
            return UTF8_TERMINAL_PROFILE_INVALID;
         metrics = utf8_terminal_metrics_from_name(tokens[index + 1]);
         if (!display_accepts_metrics(entry->display_mode, metrics))
            return UTF8_TERMINAL_PROFILE_INVALID;
         entry->metric_method = metrics;
         index += 2;
         settings++;
         continue;
      }
      if (ascii_equal_ci(tokens[index], "width"))
      {
         int width;

         if (index + 1 >= count
         ||  !parse_positive_int(tokens[index + 1], &width)
         ||  !display_accepts_width(entry->display_mode, width))
            return UTF8_TERMINAL_PROFILE_INVALID;
         if (index + 2 < count && ascii_equal_ci(tokens[index + 2], "advance"))
         {
            int advance_width;
            int cursor_width;
            int repaint_width;

            if (index + 7 >= count
            ||  !ascii_equal_ci(tokens[index + 4], "cursor")
            ||  !ascii_equal_ci(tokens[index + 6], "repaint")
            ||  !parse_positive_int(tokens[index + 3], &advance_width)
            ||  !parse_positive_int(tokens[index + 5], &cursor_width)
            ||  !parse_positive_int(tokens[index + 7], &repaint_width))
               return UTF8_TERMINAL_PROFILE_INVALID;
            entry->width = width;
            entry->advance_width = advance_width;
            entry->cursor_width = cursor_width;
            entry->repaint_width = repaint_width;
            index += 8;
         }
         else
         {
            entry->width = width;
            index += 2;
         }
         settings++;
         continue;
      }
      if (ascii_equal_ci(tokens[index], "advance"))
      {
         int advance_width;

         if (index + 1 >= count
         ||  !parse_positive_int(tokens[index + 1], &advance_width))
            return UTF8_TERMINAL_PROFILE_INVALID;
         entry->advance_width = advance_width;
         index += 2;
         settings++;
         continue;
      }
      if (ascii_equal_ci(tokens[index], "cursor"))
      {
         int cursor_width;

         if (index + 1 >= count
         ||  !parse_positive_int(tokens[index + 1], &cursor_width))
            return UTF8_TERMINAL_PROFILE_INVALID;
         entry->cursor_width = cursor_width;
         index += 2;
         settings++;
         continue;
      }
      if (ascii_equal_ci(tokens[index], "repaint"))
      {
         int repaint_width;

         if (index + 1 >= count
         ||  !parse_positive_int(tokens[index + 1], &repaint_width))
            return UTF8_TERMINAL_PROFILE_INVALID;
         entry->repaint_width = repaint_width;
         index += 2;
         settings++;
         continue;
      }
      if (ascii_equal_ci(tokens[index], "mark"))
      {
         Utf8TerminalMark mark;

         if (index + 1 >= count)
            return UTF8_TERMINAL_PROFILE_INVALID;
         mark = utf8_terminal_mark_from_name(tokens[index + 1]);
         if (mark == UTF8_TERM_MARK_UNKNOWN)
            return UTF8_TERMINAL_PROFILE_INVALID;
         entry->mark = mark;
         index += 2;
         settings++;
         continue;
      }
      if (ascii_equal_ci(tokens[index], "displaystrategy"))
      {
         Utf8TerminalDisplayStrategy strategy;

         if (index + 1 >= count)
            return UTF8_TERMINAL_PROFILE_INVALID;
         strategy = utf8_terminal_display_strategy_from_name(
                       tokens[index + 1]);
         if (strategy == UTF8_TERM_DISPLAY_STRATEGY_UNKNOWN)
            return UTF8_TERMINAL_PROFILE_INVALID;
         entry->display_strategy = strategy;
         index += 2;
         settings++;
         continue;
      }
      if (ascii_equal_ci(tokens[index], "cursorstrategy"))
      {
         Utf8TerminalStrategy strategy;

         if (index + 1 >= count)
            return UTF8_TERMINAL_PROFILE_INVALID;
         strategy = utf8_terminal_strategy_from_name(tokens[index + 1]);
         if (strategy == UTF8_TERM_STRATEGY_UNKNOWN)
            return UTF8_TERMINAL_PROFILE_INVALID;
         entry->cursor_strategy = strategy;
         index += 2;
         settings++;
         continue;
      }
      if (ascii_equal_ci(tokens[index], "replacestrategy"))
      {
         Utf8TerminalStrategy strategy;

         if (index + 1 >= count)
            return UTF8_TERMINAL_PROFILE_INVALID;
         strategy = utf8_terminal_strategy_from_name(tokens[index + 1]);
         if (strategy == UTF8_TERM_STRATEGY_UNKNOWN)
            return UTF8_TERMINAL_PROFILE_INVALID;
         entry->replacement_strategy = strategy;
         index += 2;
         settings++;
         continue;
      }
      return UTF8_TERMINAL_PROFILE_INVALID;
   }

   return settings > 0 ? UTF8_TERMINAL_PROFILE_APPLIED
                       : UTF8_TERMINAL_PROFILE_INVALID;
}

static int apply_profile_settings_to_selector(
   Utf8TerminalDisplayMode display, Utf8TerminalClass feature_class,
   int selector_any,
   char tokens[UTF8_TERM_MAX_TOKENS][UTF8_TERM_TOKEN_MAX],
   int count, int index)
{
   Utf8TerminalProfileEntry backup[
      sizeof(default_entries) / sizeof(default_entries[0])
   ];
   int rc = UTF8_TERMINAL_PROFILE_APPLIED;
   int class_index;

   memcpy(backup, profile_entries, sizeof(profile_entries));
   for (class_index = 0; class_index < UTF8_TERM_CLASS_COUNT; class_index++)
   {
      Utf8TerminalProfileEntry *entry;
      Utf8TerminalClass current_class = (Utf8TerminalClass)class_index;

      if (!selector_any && current_class != feature_class)
         continue;
      entry = profile_entry_for(current_class, display);
      if (entry == NULL)
      {
         rc = UTF8_TERMINAL_PROFILE_INVALID;
         break;
      }
      rc = apply_profile_settings_to_entry(entry, tokens, count, index);
      if (rc != UTF8_TERMINAL_PROFILE_APPLIED)
         break;
   }
   if (rc != UTF8_TERMINAL_PROFILE_APPLIED)
      memcpy(profile_entries, backup, sizeof(profile_entries));
   return rc;
}

static int parse_class_selector(const char *token,
                                Utf8TerminalClass *feature_class,
                                int *selector_any)
{
   if (feature_class != NULL)
      *feature_class = UTF8_TERM_CLASS_UNKNOWN;
   if (selector_any != NULL)
      *selector_any = 0;
   if (token == NULL)
      return 0;
   if (ascii_equal_ci(token, "any"))
   {
      if (selector_any != NULL)
         *selector_any = 1;
      return 1;
   }
   if (feature_class != NULL)
      *feature_class = utf8_terminal_class_from_name(token);
   return feature_class != NULL
       && *feature_class != UTF8_TERM_CLASS_UNKNOWN;
}

int utf8_terminal_profile_apply_line(const char *line)
{
   char tokens[UTF8_TERM_MAX_TOKENS][UTF8_TERM_TOKEN_MAX];
   int count;
   int index = 0;
   Utf8TerminalClass feature_class;
   Utf8TerminalDisplayMode display = UTF8_TERM_DISPLAY_NORMAL;
   Utf8TerminalProfileEntry *entry;

   ensure_profile_initialised();
   count = tokenize_line(line, tokens);
   if (count <= 0)
      return count;

   if (ascii_equal_ci(tokens[0], "address")
   ||  ascii_equal_ci(tokens[0], "options"))
      return UTF8_TERMINAL_PROFILE_IGNORED;
   if (index < count && ascii_equal_ci(tokens[index], "set"))
      index++;
   if (index < count && ascii_equal_ci(tokens[index], "utf"))
      index++;
   if (index < count && ascii_equal_ci(tokens[index], "display"))
   {
      Utf8TerminalDisplayMode requested_display;

      if (index + 1 >= count)
         return UTF8_TERMINAL_PROFILE_INVALID;
      if (ascii_equal_ci(tokens[index + 1], "toggle"))
      {
         if (index + 2 != count)
            return UTF8_TERMINAL_PROFILE_INVALID;
         (void)utf8_terminal_toggle_display_mode();
         return UTF8_TERMINAL_PROFILE_APPLIED;
      }
      requested_display = utf8_terminal_display_from_name(tokens[index + 1]);
      if (requested_display == UTF8_TERM_DISPLAY_UNKNOWN)
         return UTF8_TERMINAL_PROFILE_INVALID;
      if (index + 2 == count)
         return utf8_terminal_set_display_mode(requested_display);
      if (index + 4 > count || !ascii_equal_ci(tokens[index + 2], "class"))
         return UTF8_TERMINAL_PROFILE_INVALID;
      {
         Utf8TerminalClass display_class;
         int selector_any;

         if (!parse_class_selector(tokens[index + 3], &display_class,
                                   &selector_any))
            return UTF8_TERMINAL_PROFILE_INVALID;
         return apply_profile_settings_to_selector(requested_display,
                                                   display_class,
                                                   selector_any,
                                                   tokens, count,
                                                   index + 4);
      }
   }
   if (index >= count || !ascii_equal_ci(tokens[index], "terminal"))
      return UTF8_TERMINAL_PROFILE_INVALID;
   index++;
   if (index >= count || !ascii_equal_ci(tokens[index], "class"))
      return UTF8_TERMINAL_PROFILE_INVALID;
   index++;
   if (index >= count)
      return UTF8_TERMINAL_PROFILE_INVALID;

   {
      int selector_any;

      if (!parse_class_selector(tokens[index], &feature_class, &selector_any)
      ||  selector_any)
         return UTF8_TERMINAL_PROFILE_INVALID;
   }
   index++;

   if (index < count && ascii_equal_ci(tokens[index], "display"))
   {
      if (index + 1 >= count)
         return UTF8_TERMINAL_PROFILE_INVALID;
      display = utf8_terminal_display_from_name(tokens[index + 1]);
      if (display == UTF8_TERM_DISPLAY_UNKNOWN)
         return UTF8_TERMINAL_PROFILE_INVALID;
      index += 2;
   }

   entry = profile_entry_for(feature_class, display);
   if (entry == NULL || index >= count)
      return UTF8_TERMINAL_PROFILE_INVALID;
   return apply_profile_settings_to_entry(entry, tokens, count, index);
}

static const char *canonical_display_name(Utf8TerminalDisplayMode display)
{
   switch (display)
   {
      case UTF8_TERM_DISPLAY_NORMAL:
         return "NORMAL";
      case UTF8_TERM_DISPLAY_DECOMPOSED:
         return "DECOMPOSED";
      case UTF8_TERM_DISPLAY_SINGLE:
         return "SINGLE";
      default:
         return "UNKNOWN";
   }
}

static const char *canonical_output_name(Utf8TerminalOutput output)
{
   switch (output)
   {
      case UTF8_TERM_OUTPUT_NATIVE:
         return "NATIVE";
      case UTF8_TERM_OUTPUT_EXPANDED:
         return "EXPANDED";
      case UTF8_TERM_OUTPUT_SUBSTITUTE:
         return "SUBSTITUTE";
      case UTF8_TERM_OUTPUT_BASE:
         return "BASE";
      case UTF8_TERM_OUTPUT_COMPONENTS:
         return "COMPONENTS";
      case UTF8_TERM_OUTPUT_SANITIZE:
         return "SANITIZE";
      default:
         return "UNKNOWN";
   }
}

static const char *canonical_metrics_name(Utf8TerminalMetrics metrics)
{
   switch (metrics)
   {
      case UTF8_TERM_METRICS_AUTO:
         return "AUTO";
      case UTF8_TERM_METRICS_PROFILE:
         return "PROFILE";
      case UTF8_TERM_METRICS_COMPONENTS:
         return "COMPONENTS";
      case UTF8_TERM_METRICS_EXPANDED:
         return "EXPANDED";
      case UTF8_TERM_METRICS_OUTPUT:
         return "OUTPUT";
      default:
         return "UNKNOWN";
   }
}

static const char *canonical_mark_name(Utf8TerminalMark mark)
{
   switch (mark)
   {
      case UTF8_TERM_MARK_NONE:
         return "NONE";
      case UTF8_TERM_MARK_COMPRESSED:
         return "COMPRESSED";
      case UTF8_TERM_MARK_SUBSTITUTED:
         return "SUBSTITUTED";
      case UTF8_TERM_MARK_UNSAFE:
         return "UNSAFE";
      default:
         return "UNKNOWN";
   }
}

static const char *canonical_display_strategy_name(
   Utf8TerminalDisplayStrategy strategy)
{
   switch (strategy)
   {
      case UTF8_TERM_DISPLAY_STRATEGY_INLINE:
         return "INLINE";
      case UTF8_TERM_DISPLAY_STRATEGY_ISOLATE:
         return "ISOLATE";
      default:
         return "UNKNOWN";
   }
}

static const char *canonical_strategy_name(Utf8TerminalStrategy strategy)
{
   switch (strategy)
   {
      case UTF8_TERM_STRATEGY_CHANGED_CELLS:
         return "CELLS";
      case UTF8_TERM_STRATEGY_LINE:
         return "LINE";
      case UTF8_TERM_STRATEGY_CLEAR_CHANGED_SUFFIX_FAST:
         return "SUFFIX";
      case UTF8_TERM_STRATEGY_CLEAR_FROM_ONE_PRIOR_CLUSTER:
         return "PREV";
      case UTF8_TERM_STRATEGY_CLEAR_FROM_FIRST_CLUSTER_FAST:
         return "FIRST";
      case UTF8_TERM_STRATEGY_CLEAR_WHOLE_FAST:
         return "WHOLE";
      default:
         return "UNKNOWN";
   }
}

int utf8_terminal_profile_entry_canonical(const Utf8TerminalProfileEntry *entry,
                                          char *out, size_t out_size)
{
   int written;
   int used;

   if (entry == NULL || out == NULL || out_size == 0)
      return 0;
   if (entry->output_method == UTF8_TERM_OUTPUT_SUBSTITUTE)
      written = snprintf(out, out_size,
         "SET UTF DISPLAY %s CLASS %s OUTPUT %s U+%04X",
         canonical_display_name(entry->display_mode),
         utf8_terminal_class_name(entry->feature_class),
         canonical_output_name(entry->output_method),
         entry->substitute_codepoint);
   else
      written = snprintf(out, out_size,
         "SET UTF DISPLAY %s CLASS %s OUTPUT %s",
         canonical_display_name(entry->display_mode),
         utf8_terminal_class_name(entry->feature_class),
         canonical_output_name(entry->output_method));
   if (written < 0 || (size_t)written >= out_size)
      return 0;
   used = written;

   written = snprintf(out + used, out_size - (size_t)used,
      " METRICS %s MARK %s WIDTH %d ADVANCE %d CURSOR %d REPAINT %d"
      " DISPLAYSTRATEGY %s CURSORSTRATEGY %s REPLACESTRATEGY %s",
      canonical_metrics_name(entry->metric_method),
      canonical_mark_name(entry->mark),
      entry->width, entry->advance_width, entry->cursor_width,
      entry->repaint_width,
      canonical_display_strategy_name(entry->display_strategy),
      canonical_strategy_name(entry->cursor_strategy),
      canonical_strategy_name(entry->replacement_strategy));
   if (written < 0 || (size_t)written >= out_size - (size_t)used)
      return 0;
   return 1;
}

int utf8_terminal_profile_canonical_rule_at(size_t index,
                                            char *out, size_t out_size)
{
   const Utf8TerminalProfileEntry *entry =
      utf8_terminal_profile_entry_at(index);

   return utf8_terminal_profile_entry_canonical(entry, out, out_size);
}

int utf8_terminal_profile_apply_file(const char *path, int *settings_loaded)
{
   FILE *fp;
   char line[512];
   int loaded = 0;

   if (settings_loaded != NULL)
      *settings_loaded = 0;
   if (path == NULL || *path == '\0')
      return UTF8_TERMINAL_PROFILE_INVALID;

   fp = fopen(path, "r");
   if (fp == NULL)
      return UTF8_TERMINAL_PROFILE_INVALID;

   while (fgets(line, sizeof(line), fp) != NULL)
   {
      int rc = utf8_terminal_profile_apply_line(line);

      if (rc == UTF8_TERMINAL_PROFILE_INVALID)
      {
         fclose(fp);
         return UTF8_TERMINAL_PROFILE_INVALID;
      }
      if (rc == UTF8_TERMINAL_PROFILE_APPLIED)
         loaded++;
   }
   fclose(fp);
   if (settings_loaded != NULL)
      *settings_loaded = loaded;
   return UTF8_TERMINAL_PROFILE_APPLIED;
}

int utf8_terminal_profile_apply_apple_terminal(void)
{
   /*
    * Keep compiled defaults generic. Apple Terminal policy lives in
    * system-osx.the so hand-tuned platform overrides are visible in one
    * profile and can be replaced without rebuilding THE.
    */
   return 0;
}

int utf8_terminal_profile_apply_terminal_identity(const char *term,
                                                  const char *term_program)
{
   (void)term;
   if (term_program != NULL && ascii_equal_ci(term_program, "Apple_Terminal"))
      return utf8_terminal_profile_apply_apple_terminal();
   return 0;
}

int utf8_terminal_profile_init_from_environment(void)
{
   const char *profile_path;
   int loaded = 0;
   int rc;

   utf8_terminal_profile_reset();
   rc = utf8_terminal_profile_apply_terminal_identity(getenv("TERM"),
                                                      getenv("TERM_PROGRAM"));
   if (rc < 0)
      return rc;
   loaded += rc;

   profile_path = getenv("THE_UTF_TERMINAL_PROFILE");
   if (profile_path != NULL && *profile_path != '\0')
   {
      int file_settings = 0;

      rc = utf8_terminal_profile_apply_file(profile_path, &file_settings);
      if (rc < 0)
         return rc;
      loaded += file_settings;
   }
   return loaded;
}
