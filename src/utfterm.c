#include "utfterm.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "utfcluster.h"

#define UTF8_TERM_TOKEN_MAX 96
#define UTF8_TERM_MAX_TOKENS 12

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
   { "grouped", UTF8_TERM_DISPLAY_GROUPED },
   { "components", UTF8_TERM_DISPLAY_COMPONENTS },
   { NULL, UTF8_TERM_DISPLAY_UNKNOWN }
};

static const OutputName output_names[] =
{
   { "native", UTF8_TERM_OUTPUT_NATIVE },
   { "literal", UTF8_TERM_OUTPUT_NATIVE },
   { "expanded", UTF8_TERM_OUTPUT_EXPANDED },
   { "substitute", UTF8_TERM_OUTPUT_SUBSTITUTE },
   { "placeholder", UTF8_TERM_OUTPUT_SUBSTITUTE },
   { NULL, UTF8_TERM_OUTPUT_UNKNOWN }
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

#define UTF8_TERM_DEFAULT_ENTRY(feature_class, feature_class_name, display_mode, display_mode_name, output_method, output_method_name, substitute_codepoint, layout_width, cursor_width, cursor_strategy, cursor_strategy_name, replacement_strategy, replacement_strategy_name) \
   { feature_class, display_mode, output_method, substitute_codepoint, layout_width, cursor_width, cursor_strategy, replacement_strategy },

static const Utf8TerminalProfileEntry default_entries[] =
{
   UTF8_TERMINAL_DEFAULT_PROFILE_ENTRIES(UTF8_TERM_DEFAULT_ENTRY)
};

#undef UTF8_TERM_DEFAULT_ENTRY

static Utf8TerminalProfileEntry profile_entries[
   sizeof(default_entries) / sizeof(default_entries[0])
];
static int profile_initialised = 0;
static Utf8TerminalDisplayMode display_mode = UTF8_TERM_DISPLAY_GROUPED;

static const char *apple_terminal_overrides[] =
{
   "SET UTF TERMINAL CLASS combining LAYOUT 1 CURSOR 1",
   "SET UTF TERMINAL CLASS combining CURSORSTRATEGY cells",
   "SET UTF TERMINAL CLASS combining REPLACESTRATEGY cells",
   "SET UTF TERMINAL CLASS combining-stack LAYOUT 1 CURSOR 1",
   "SET UTF TERMINAL CLASS combining-stack CURSORSTRATEGY cells",
   "SET UTF TERMINAL CLASS combining-stack REPLACESTRATEGY cells",
   "SET UTF TERMINAL CLASS wide LAYOUT 2 CURSOR 2",
   "SET UTF TERMINAL CLASS wide CURSORSTRATEGY cells",
   "SET UTF TERMINAL CLASS wide REPLACESTRATEGY line",
   "SET UTF TERMINAL CLASS emoji LAYOUT 2 CURSOR 2",
   "SET UTF TERMINAL CLASS emoji CURSORSTRATEGY cells",
   "SET UTF TERMINAL CLASS emoji REPLACESTRATEGY line",
   "SET UTF TERMINAL CLASS text-variation LAYOUT 1 CURSOR 1",
   "SET UTF TERMINAL CLASS text-variation CURSORSTRATEGY cells",
   "SET UTF TERMINAL CLASS text-variation REPLACESTRATEGY cells",
   "SET UTF TERMINAL CLASS emoji-variation LAYOUT 2 CURSOR 2",
   "SET UTF TERMINAL CLASS emoji-variation CURSORSTRATEGY cells",
   "SET UTF TERMINAL CLASS emoji-variation REPLACESTRATEGY line",
   "SET UTF TERMINAL CLASS modifier LAYOUT 4 CURSOR 4",
   "SET UTF TERMINAL CLASS modifier CURSORSTRATEGY cells",
   "SET UTF TERMINAL CLASS modifier REPLACESTRATEGY line",
   "SET UTF TERMINAL CLASS keycap LAYOUT 2 CURSOR 2",
   "SET UTF TERMINAL CLASS keycap CURSORSTRATEGY first",
   "SET UTF TERMINAL CLASS keycap REPLACESTRATEGY first",
   "SET UTF TERMINAL CLASS short-zwj DISPLAY grouped OUTPUT substitute U+0040",
   "SET UTF TERMINAL CLASS short-zwj DISPLAY components OUTPUT native",
   "SET UTF TERMINAL CLASS short-zwj DISPLAY components LAYOUT 4 CURSOR 4",
   "SET UTF TERMINAL CLASS short-zwj DISPLAY components CURSORSTRATEGY cells",
   "SET UTF TERMINAL CLASS short-zwj DISPLAY components REPLACESTRATEGY line",
   "SET UTF TERMINAL CLASS heart-zwj DISPLAY grouped OUTPUT substitute U+0040",
   "SET UTF TERMINAL CLASS heart-zwj DISPLAY components OUTPUT expanded",
   "SET UTF TERMINAL CLASS heart-zwj DISPLAY components LAYOUT 6 CURSOR 6",
   "SET UTF TERMINAL CLASS heart-zwj DISPLAY components CURSORSTRATEGY cells",
   "SET UTF TERMINAL CLASS heart-zwj DISPLAY components REPLACESTRATEGY line",
   "SET UTF TERMINAL CLASS family-zwj DISPLAY grouped OUTPUT substitute U+0040",
   "SET UTF TERMINAL CLASS family-zwj DISPLAY components OUTPUT expanded",
   "SET UTF TERMINAL CLASS family-zwj DISPLAY components LAYOUT 8 CURSOR 8",
   "SET UTF TERMINAL CLASS family-zwj DISPLAY components CURSORSTRATEGY cells",
   "SET UTF TERMINAL CLASS family-zwj DISPLAY components REPLACESTRATEGY line",
   "SET UTF TERMINAL CLASS tag-flag LAYOUT 2 CURSOR 2",
   "SET UTF TERMINAL CLASS tag-flag CURSORSTRATEGY cells",
   "SET UTF TERMINAL CLASS tag-flag REPLACESTRATEGY line",
   "SET UTF TERMINAL CLASS private-use LAYOUT 1 CURSOR 1",
   "SET UTF TERMINAL CLASS private-use CURSORSTRATEGY cells",
   "SET UTF TERMINAL CLASS private-use REPLACESTRATEGY cells"
};

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
   display_mode = UTF8_TERM_DISPLAY_GROUPED;
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
   if (display != UTF8_TERM_DISPLAY_GROUPED
   &&  display != UTF8_TERM_DISPLAY_COMPONENTS)
      return UTF8_TERMINAL_PROFILE_INVALID;
   display_mode = display;
   return UTF8_TERMINAL_PROFILE_APPLIED;
}

Utf8TerminalDisplayMode utf8_terminal_toggle_display_mode(void)
{
   ensure_profile_initialised();
   if (display_mode == UTF8_TERM_DISPLAY_COMPONENTS)
      display_mode = UTF8_TERM_DISPLAY_GROUPED;
   else
      display_mode = UTF8_TERM_DISPLAY_COMPONENTS;
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
   if (display == UTF8_TERM_DISPLAY_UNKNOWN
        ||  display == UTF8_TERM_DISPLAY_NORMAL)
   {
      display = UTF8_TERM_DISPLAY_GROUPED;
   }

   entry = utf8_terminal_profile_lookup(feature_class, display);
   if (entry != NULL)
      return entry;
   if (display != UTF8_TERM_DISPLAY_NORMAL)
      return utf8_terminal_profile_lookup(feature_class,
                                          UTF8_TERM_DISPLAY_NORMAL);
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
   if (display == UTF8_TERM_DISPLAY_NORMAL)
      return UTF8_TERM_OUTPUT_NATIVE;
   if (display == UTF8_TERM_DISPLAY_GROUPED)
   {
      if (output == UTF8_TERM_OUTPUT_NATIVE)
         return output;
      return UTF8_TERM_OUTPUT_NATIVE;
   }
   if (display == UTF8_TERM_DISPLAY_COMPONENTS)
   {
      if (output == UTF8_TERM_OUTPUT_NATIVE
      ||  output == UTF8_TERM_OUTPUT_EXPANDED)
         return output;
      return UTF8_TERM_OUTPUT_EXPANDED;
   }
   return UTF8_TERM_OUTPUT_NATIVE;
}

static void apply_substitute_defaults(Utf8TerminalProfileEntry *entry)
{
   entry->layout_width = 1;
   entry->cursor_width = 1;
   entry->cursor_strategy = UTF8_TERM_STRATEGY_CHANGED_CELLS;
   entry->replacement_strategy = UTF8_TERM_STRATEGY_CHANGED_CELLS;
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
   return UTF8_TERMINAL_PROFILE_APPLIED;
}

static Utf8TerminalDisplayMode legacy_display_for_output(Utf8TerminalOutput output)
{
   if (output == UTF8_TERM_OUTPUT_EXPANDED)
      return UTF8_TERM_DISPLAY_COMPONENTS;
   return UTF8_TERM_DISPLAY_GROUPED;
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

      if (index + 2 != count)
         return UTF8_TERMINAL_PROFILE_INVALID;
      if (ascii_equal_ci(tokens[index + 1], "toggle"))
      {
         (void)utf8_terminal_toggle_display_mode();
         return UTF8_TERMINAL_PROFILE_APPLIED;
      }
      requested_display = utf8_terminal_display_from_name(tokens[index + 1]);
      return utf8_terminal_set_display_mode(requested_display);
   }
   if (index >= count || !ascii_equal_ci(tokens[index], "terminal"))
      return UTF8_TERMINAL_PROFILE_INVALID;
   index++;
   if (index >= count || !ascii_equal_ci(tokens[index], "class"))
      return UTF8_TERMINAL_PROFILE_INVALID;
   index++;
   if (index >= count)
      return UTF8_TERMINAL_PROFILE_INVALID;

   feature_class = utf8_terminal_class_from_name(tokens[index]);
   if (feature_class == UTF8_TERM_CLASS_UNKNOWN)
      return UTF8_TERMINAL_PROFILE_INVALID;
   index++;

   if (index < count && ascii_equal_ci(tokens[index], "zwjdisplay"))
   {
      Utf8TerminalOutput output;

      if (index + 2 != count)
         return UTF8_TERMINAL_PROFILE_INVALID;
      output = utf8_terminal_output_from_name(tokens[index + 1]);
      if (output == UTF8_TERM_OUTPUT_UNKNOWN)
         return UTF8_TERMINAL_PROFILE_INVALID;
      entry = profile_entry_for(feature_class, legacy_display_for_output(output));
      return apply_output(entry, output, 0, 0);
   }

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

   if (ascii_equal_ci(tokens[index], "output"))
   {
      Utf8TerminalOutput output;
      uint32_t substitute_codepoint = 0;
      int has_substitute_codepoint = 0;

      if (index + 2 != count && index + 3 != count)
         return UTF8_TERMINAL_PROFILE_INVALID;
      output = utf8_terminal_output_from_name(tokens[index + 1]);
      if (index + 3 == count)
      {
         if (!parse_codepoint(tokens[index + 2], &substitute_codepoint))
            return UTF8_TERMINAL_PROFILE_INVALID;
         has_substitute_codepoint = 1;
      }
      return apply_output(entry, output, substitute_codepoint,
                          has_substitute_codepoint);
   }
   if (ascii_equal_ci(tokens[index], "layout"))
   {
      int layout_width;
      int cursor_width;

      if (index + 4 != count || !ascii_equal_ci(tokens[index + 2], "cursor"))
         return UTF8_TERMINAL_PROFILE_INVALID;
      if (!parse_positive_int(tokens[index + 1], &layout_width)
      ||  !parse_positive_int(tokens[index + 3], &cursor_width))
         return UTF8_TERMINAL_PROFILE_INVALID;
      entry->layout_width = layout_width;
      entry->cursor_width = cursor_width;
      return UTF8_TERMINAL_PROFILE_APPLIED;
   }
   if (ascii_equal_ci(tokens[index], "cursorstrategy"))
   {
      Utf8TerminalStrategy strategy;

      if (index + 2 != count)
         return UTF8_TERMINAL_PROFILE_INVALID;
      strategy = utf8_terminal_strategy_from_name(tokens[index + 1]);
      if (strategy == UTF8_TERM_STRATEGY_UNKNOWN)
         return UTF8_TERMINAL_PROFILE_INVALID;
      entry->cursor_strategy = strategy;
      return UTF8_TERMINAL_PROFILE_APPLIED;
   }
   if (ascii_equal_ci(tokens[index], "replacestrategy"))
   {
      Utf8TerminalStrategy strategy;

      if (index + 2 != count)
         return UTF8_TERMINAL_PROFILE_INVALID;
      strategy = utf8_terminal_strategy_from_name(tokens[index + 1]);
      if (strategy == UTF8_TERM_STRATEGY_UNKNOWN)
         return UTF8_TERMINAL_PROFILE_INVALID;
      entry->replacement_strategy = strategy;
      return UTF8_TERMINAL_PROFILE_APPLIED;
   }
   return UTF8_TERMINAL_PROFILE_INVALID;
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
   size_t i;
   int loaded = 0;

   for (i = 0; i < sizeof(apple_terminal_overrides) / sizeof(apple_terminal_overrides[0]); i++)
   {
      int rc = utf8_terminal_profile_apply_line(apple_terminal_overrides[i]);

      if (rc == UTF8_TERMINAL_PROFILE_INVALID)
         return UTF8_TERMINAL_PROFILE_INVALID;
      if (rc == UTF8_TERMINAL_PROFILE_APPLIED)
         loaded++;
   }
   return loaded;
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
