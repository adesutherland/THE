#include "utf8term.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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
   Utf8TerminalIntent value;
} IntentName;

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

static const IntentName intent_names[] =
{
   { "normal", UTF8_TERM_INTENT_NORMAL },
   { "group", UTF8_TERM_INTENT_GROUP },
   { "components", UTF8_TERM_INTENT_COMPONENTS },
   { NULL, UTF8_TERM_INTENT_UNKNOWN }
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
   { "changed_cells", UTF8_TERM_STRATEGY_CHANGED_CELLS },
   { "cell", UTF8_TERM_STRATEGY_CHANGED_CELLS },
   { "line", UTF8_TERM_STRATEGY_LINE },
   { "clear_changed_suffix_fast", UTF8_TERM_STRATEGY_CLEAR_CHANGED_SUFFIX_FAST },
   { "suffix", UTF8_TERM_STRATEGY_CLEAR_CHANGED_SUFFIX_FAST },
   { "clear_from_first_cluster_fast", UTF8_TERM_STRATEGY_CLEAR_FROM_FIRST_CLUSTER_FAST },
   { "flashfirstfast", UTF8_TERM_STRATEGY_CLEAR_FROM_FIRST_CLUSTER_FAST },
   { "clearfirstfast", UTF8_TERM_STRATEGY_CLEAR_FROM_FIRST_CLUSTER_FAST },
   { "clear_from_first_cluster_pause", UTF8_TERM_STRATEGY_CLEAR_FROM_FIRST_CLUSTER_PAUSE },
   { "flashfirstcluster", UTF8_TERM_STRATEGY_CLEAR_FROM_FIRST_CLUSTER_PAUSE },
   { "clear_whole_fast", UTF8_TERM_STRATEGY_CLEAR_WHOLE_FAST },
   { "flashwhole", UTF8_TERM_STRATEGY_CLEAR_WHOLE_FAST },
   { "clear_from_one_prior_cluster", UTF8_TERM_STRATEGY_CLEAR_FROM_ONE_PRIOR_CLUSTER },
   { "flashbackcluster1", UTF8_TERM_STRATEGY_CLEAR_FROM_ONE_PRIOR_CLUSTER },
   { NULL, UTF8_TERM_STRATEGY_UNKNOWN }
};

static const Utf8TerminalProfileEntry default_entries[] =
{
   { UTF8_TERM_CLASS_ASCII, UTF8_TERM_INTENT_NORMAL, UTF8_TERM_OUTPUT_NATIVE, 1, 1,
     UTF8_TERM_STRATEGY_CHANGED_CELLS, UTF8_TERM_STRATEGY_CHANGED_CELLS },
   { UTF8_TERM_CLASS_COMBINING, UTF8_TERM_INTENT_NORMAL, UTF8_TERM_OUTPUT_NATIVE, 1, 1,
     UTF8_TERM_STRATEGY_LINE, UTF8_TERM_STRATEGY_LINE },
   { UTF8_TERM_CLASS_COMBINING_STACK, UTF8_TERM_INTENT_NORMAL, UTF8_TERM_OUTPUT_NATIVE, 1, 1,
     UTF8_TERM_STRATEGY_LINE, UTF8_TERM_STRATEGY_LINE },
   { UTF8_TERM_CLASS_WIDE, UTF8_TERM_INTENT_NORMAL, UTF8_TERM_OUTPUT_NATIVE, 2, 2,
     UTF8_TERM_STRATEGY_CHANGED_CELLS, UTF8_TERM_STRATEGY_CHANGED_CELLS },
   { UTF8_TERM_CLASS_AMBIGUOUS, UTF8_TERM_INTENT_NORMAL, UTF8_TERM_OUTPUT_NATIVE, 1, 1,
     UTF8_TERM_STRATEGY_CHANGED_CELLS, UTF8_TERM_STRATEGY_CHANGED_CELLS },
   { UTF8_TERM_CLASS_EMOJI, UTF8_TERM_INTENT_NORMAL, UTF8_TERM_OUTPUT_NATIVE, 2, 2,
     UTF8_TERM_STRATEGY_LINE, UTF8_TERM_STRATEGY_LINE },
   { UTF8_TERM_CLASS_TEXT_VARIATION, UTF8_TERM_INTENT_NORMAL, UTF8_TERM_OUTPUT_NATIVE, 1, 1,
     UTF8_TERM_STRATEGY_LINE, UTF8_TERM_STRATEGY_LINE },
   { UTF8_TERM_CLASS_EMOJI_VARIATION, UTF8_TERM_INTENT_NORMAL, UTF8_TERM_OUTPUT_NATIVE, 2, 2,
     UTF8_TERM_STRATEGY_LINE, UTF8_TERM_STRATEGY_LINE },
   { UTF8_TERM_CLASS_MODIFIER, UTF8_TERM_INTENT_NORMAL, UTF8_TERM_OUTPUT_NATIVE, 2, 2,
     UTF8_TERM_STRATEGY_LINE, UTF8_TERM_STRATEGY_LINE },
   { UTF8_TERM_CLASS_KEYCAP, UTF8_TERM_INTENT_NORMAL, UTF8_TERM_OUTPUT_NATIVE, 2, 2,
     UTF8_TERM_STRATEGY_CLEAR_FROM_FIRST_CLUSTER_FAST,
     UTF8_TERM_STRATEGY_CLEAR_WHOLE_FAST },
   { UTF8_TERM_CLASS_REGIONAL_FLAG, UTF8_TERM_INTENT_NORMAL, UTF8_TERM_OUTPUT_NATIVE, 3, 3,
     UTF8_TERM_STRATEGY_CHANGED_CELLS,
     UTF8_TERM_STRATEGY_CLEAR_CHANGED_SUFFIX_FAST },
   { UTF8_TERM_CLASS_SHORT_ZWJ, UTF8_TERM_INTENT_GROUP, UTF8_TERM_OUTPUT_NATIVE, 2, 2,
     UTF8_TERM_STRATEGY_LINE, UTF8_TERM_STRATEGY_LINE },
   { UTF8_TERM_CLASS_SHORT_ZWJ, UTF8_TERM_INTENT_COMPONENTS, UTF8_TERM_OUTPUT_EXPANDED, 4, 4,
     UTF8_TERM_STRATEGY_LINE, UTF8_TERM_STRATEGY_LINE },
   { UTF8_TERM_CLASS_HEART_ZWJ, UTF8_TERM_INTENT_GROUP, UTF8_TERM_OUTPUT_NATIVE, 6, 6,
     UTF8_TERM_STRATEGY_LINE, UTF8_TERM_STRATEGY_LINE },
   { UTF8_TERM_CLASS_HEART_ZWJ, UTF8_TERM_INTENT_COMPONENTS, UTF8_TERM_OUTPUT_EXPANDED, 6, 6,
     UTF8_TERM_STRATEGY_LINE, UTF8_TERM_STRATEGY_LINE },
   { UTF8_TERM_CLASS_FAMILY_ZWJ, UTF8_TERM_INTENT_GROUP, UTF8_TERM_OUTPUT_NATIVE, 6, 6,
     UTF8_TERM_STRATEGY_LINE, UTF8_TERM_STRATEGY_LINE },
   { UTF8_TERM_CLASS_FAMILY_ZWJ, UTF8_TERM_INTENT_COMPONENTS, UTF8_TERM_OUTPUT_EXPANDED, 8, 8,
     UTF8_TERM_STRATEGY_LINE, UTF8_TERM_STRATEGY_LINE },
   { UTF8_TERM_CLASS_TAG_FLAG, UTF8_TERM_INTENT_NORMAL, UTF8_TERM_OUTPUT_NATIVE, 2, 2,
     UTF8_TERM_STRATEGY_LINE, UTF8_TERM_STRATEGY_LINE },
   { UTF8_TERM_CLASS_PRIVATE_USE, UTF8_TERM_INTENT_NORMAL, UTF8_TERM_OUTPUT_NATIVE, 1, 1,
     UTF8_TERM_STRATEGY_LINE, UTF8_TERM_STRATEGY_LINE }
};

static Utf8TerminalProfileEntry profile_entries[
   sizeof(default_entries) / sizeof(default_entries[0])
];
static int profile_initialised = 0;

static const char *apple_terminal_overrides[] =
{
   "SET UTF8 TERMINAL CLASS combining LAYOUT 1 CURSOR 1",
   "SET UTF8 TERMINAL CLASS combining CURSORSTRATEGY changed_cells",
   "SET UTF8 TERMINAL CLASS combining REPLACESTRATEGY changed_cells",
   "SET UTF8 TERMINAL CLASS combining-stack LAYOUT 1 CURSOR 1",
   "SET UTF8 TERMINAL CLASS combining-stack CURSORSTRATEGY changed_cells",
   "SET UTF8 TERMINAL CLASS combining-stack REPLACESTRATEGY changed_cells",
   "SET UTF8 TERMINAL CLASS wide LAYOUT 2 CURSOR 2",
   "SET UTF8 TERMINAL CLASS wide CURSORSTRATEGY changed_cells",
   "SET UTF8 TERMINAL CLASS wide REPLACESTRATEGY line",
   "SET UTF8 TERMINAL CLASS emoji LAYOUT 2 CURSOR 2",
   "SET UTF8 TERMINAL CLASS emoji CURSORSTRATEGY changed_cells",
   "SET UTF8 TERMINAL CLASS emoji REPLACESTRATEGY line",
   "SET UTF8 TERMINAL CLASS text-variation LAYOUT 1 CURSOR 1",
   "SET UTF8 TERMINAL CLASS text-variation CURSORSTRATEGY changed_cells",
   "SET UTF8 TERMINAL CLASS text-variation REPLACESTRATEGY changed_cells",
   "SET UTF8 TERMINAL CLASS emoji-variation LAYOUT 2 CURSOR 2",
   "SET UTF8 TERMINAL CLASS emoji-variation CURSORSTRATEGY changed_cells",
   "SET UTF8 TERMINAL CLASS emoji-variation REPLACESTRATEGY line",
   "SET UTF8 TERMINAL CLASS modifier LAYOUT 4 CURSOR 4",
   "SET UTF8 TERMINAL CLASS modifier CURSORSTRATEGY changed_cells",
   "SET UTF8 TERMINAL CLASS modifier REPLACESTRATEGY line",
   "SET UTF8 TERMINAL CLASS keycap LAYOUT 2 CURSOR 2",
   "SET UTF8 TERMINAL CLASS keycap CURSORSTRATEGY clear_from_first_cluster_fast",
   "SET UTF8 TERMINAL CLASS keycap REPLACESTRATEGY clear_from_first_cluster_fast",
   "SET UTF8 TERMINAL CLASS short-zwj INTENT group OUTPUT substitute",
   "SET UTF8 TERMINAL CLASS short-zwj INTENT components OUTPUT native",
   "SET UTF8 TERMINAL CLASS short-zwj INTENT components LAYOUT 4 CURSOR 4",
   "SET UTF8 TERMINAL CLASS short-zwj INTENT components CURSORSTRATEGY changed_cells",
   "SET UTF8 TERMINAL CLASS short-zwj INTENT components REPLACESTRATEGY line",
   "SET UTF8 TERMINAL CLASS heart-zwj INTENT group OUTPUT substitute",
   "SET UTF8 TERMINAL CLASS heart-zwj INTENT components OUTPUT expanded",
   "SET UTF8 TERMINAL CLASS heart-zwj INTENT components LAYOUT 6 CURSOR 6",
   "SET UTF8 TERMINAL CLASS heart-zwj INTENT components CURSORSTRATEGY changed_cells",
   "SET UTF8 TERMINAL CLASS heart-zwj INTENT components REPLACESTRATEGY line",
   "SET UTF8 TERMINAL CLASS family-zwj INTENT group OUTPUT substitute",
   "SET UTF8 TERMINAL CLASS family-zwj INTENT components OUTPUT expanded",
   "SET UTF8 TERMINAL CLASS family-zwj INTENT components LAYOUT 8 CURSOR 8",
   "SET UTF8 TERMINAL CLASS family-zwj INTENT components CURSORSTRATEGY changed_cells",
   "SET UTF8 TERMINAL CLASS family-zwj INTENT components REPLACESTRATEGY line",
   "SET UTF8 TERMINAL CLASS tag-flag LAYOUT 2 CURSOR 2",
   "SET UTF8 TERMINAL CLASS tag-flag CURSORSTRATEGY changed_cells",
   "SET UTF8 TERMINAL CLASS tag-flag REPLACESTRATEGY line",
   "SET UTF8 TERMINAL CLASS private-use LAYOUT 1 CURSOR 1",
   "SET UTF8 TERMINAL CLASS private-use CURSORSTRATEGY changed_cells",
   "SET UTF8 TERMINAL CLASS private-use REPLACESTRATEGY changed_cells"
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
   profile_initialised = 1;
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
                                                   Utf8TerminalIntent intent)
{
   size_t i;

   ensure_profile_initialised();
   for (i = 0; i < utf8_terminal_profile_entry_count(); i++)
   {
      if (profile_entries[i].feature_class == feature_class
      &&  profile_entries[i].display_intent == intent)
         return &profile_entries[i];
   }
   return NULL;
}

const Utf8TerminalProfileEntry *utf8_terminal_profile_lookup(
   Utf8TerminalClass feature_class, Utf8TerminalIntent intent)
{
   return profile_entry_for(feature_class, intent);
}

static TextPos utf8_terminal_advance_codepoint_pos(TextPos pos,
                                                   TextCodepoint item)
{
   if (item.byte_length == 0)
      return pos;
   pos.byte_offset += item.byte_length;
   pos.codepoint_index++;
   pos.cell_column += item.cell_width;
   return pos;
}

static int utf8_terminal_codepoint_is_regional(uint32_t codepoint)
{
   return codepoint >= 0x1F1E6u && codepoint <= 0x1F1FFu;
}

static int utf8_terminal_codepoint_is_tag(uint32_t codepoint)
{
   return codepoint >= 0xE0020u && codepoint <= 0xE007Fu;
}

static int utf8_terminal_codepoint_is_modifier(uint32_t codepoint)
{
   return codepoint >= 0x1F3FBu && codepoint <= 0x1F3FFu;
}

static int utf8_terminal_codepoint_is_private_use(uint32_t codepoint)
{
   return (codepoint >= 0xE000u && codepoint <= 0xF8FFu)
       || (codepoint >= 0xF0000u && codepoint <= 0xFFFFDu)
       || (codepoint >= 0x100000u && codepoint <= 0x10FFFDu);
}

static int utf8_terminal_codepoint_is_emojiish(uint32_t codepoint)
{
   return (codepoint >= 0x1F000u && codepoint <= 0x1FAFFu)
       || (codepoint >= 0x2600u && codepoint <= 0x27BFu);
}

Utf8TerminalClass utf8_terminal_classify_cluster(const CHARTYPE *line,
                                                 size_t len,
                                                 TextCluster cluster)
{
   TextPos pos = cluster.pos;
   uint32_t first_codepoint = 0;
   int codepoints = 0;
   int spacing_codepoints = 0;
   int regional_codepoints = 0;
   int all_regional = 1;
   int zwj_count = 0;
   int contains_keycap = 0;
   int contains_text_variation = 0;
   int contains_emoji_variation = 0;
   int contains_modifier = 0;
   int contains_heart = 0;
   int contains_tag = 0;

   if (line == NULL || cluster.byte_length == 0)
      return UTF8_TERM_CLASS_UNKNOWN;

   while (pos.byte_offset < cluster.end.byte_offset)
   {
      TextCodepoint item = textpos_codepoint_at_boundary(line, len, pos);

      if (item.byte_length == 0)
         break;
      if (codepoints == 0)
         first_codepoint = item.codepoint;
      codepoints++;
      if (item.cell_width > 0)
         spacing_codepoints++;
      if (utf8_terminal_codepoint_is_regional(item.codepoint))
         regional_codepoints++;
      else
         all_regional = 0;
      if (item.codepoint == 0x200Du)
         zwj_count++;
      else if (item.codepoint == 0x20E3u)
         contains_keycap = 1;
      else if (item.codepoint == 0xFE0Eu)
         contains_text_variation = 1;
      else if (item.codepoint == 0xFE0Fu)
         contains_emoji_variation = 1;
      else if (utf8_terminal_codepoint_is_modifier(item.codepoint))
         contains_modifier = 1;
      else if (item.codepoint == 0x2764u)
         contains_heart = 1;
      else if (utf8_terminal_codepoint_is_tag(item.codepoint))
         contains_tag = 1;
      pos = utf8_terminal_advance_codepoint_pos(pos, item);
   }

   if (codepoints == 0)
      return UTF8_TERM_CLASS_UNKNOWN;
   if (contains_keycap)
      return UTF8_TERM_CLASS_KEYCAP;
   if (all_regional && regional_codepoints == 2)
      return UTF8_TERM_CLASS_REGIONAL_FLAG;
   if (contains_tag)
      return UTF8_TERM_CLASS_TAG_FLAG;
   if (zwj_count > 0)
   {
      if (contains_heart)
         return UTF8_TERM_CLASS_HEART_ZWJ;
      if (zwj_count >= 2 || spacing_codepoints >= 3)
         return UTF8_TERM_CLASS_FAMILY_ZWJ;
      return UTF8_TERM_CLASS_SHORT_ZWJ;
   }
   if (contains_modifier)
      return UTF8_TERM_CLASS_MODIFIER;
   if (contains_emoji_variation)
      return UTF8_TERM_CLASS_EMOJI_VARIATION;
   if (contains_text_variation)
      return UTF8_TERM_CLASS_TEXT_VARIATION;
   if (codepoints > 1)
   {
      if (cluster.cell_width <= 1)
      {
         if (codepoints > 2)
            return UTF8_TERM_CLASS_COMBINING_STACK;
         return UTF8_TERM_CLASS_COMBINING;
      }
      if (utf8_terminal_codepoint_is_emojiish(first_codepoint))
         return UTF8_TERM_CLASS_EMOJI;
      return UTF8_TERM_CLASS_WIDE;
   }
   if (utf8_terminal_codepoint_is_private_use(first_codepoint))
      return UTF8_TERM_CLASS_PRIVATE_USE;
   if (first_codepoint < 0x80u)
      return UTF8_TERM_CLASS_ASCII;
   if (cluster.cell_width == 0)
      return UTF8_TERM_CLASS_COMBINING;
   if (utf8_terminal_codepoint_is_emojiish(first_codepoint))
      return UTF8_TERM_CLASS_EMOJI;
   if (cluster.cell_width >= 2)
      return UTF8_TERM_CLASS_WIDE;
   return UTF8_TERM_CLASS_AMBIGUOUS;
}

const Utf8TerminalProfileEntry *utf8_terminal_profile_lookup_cluster(
   const CHARTYPE *line, size_t len, TextCluster cluster,
   Utf8TerminalIntent preferred_intent)
{
   Utf8TerminalClass feature_class;
   Utf8TerminalIntent intent = preferred_intent;
   const Utf8TerminalProfileEntry *entry;

   feature_class = utf8_terminal_classify_cluster(line, len, cluster);
   if (feature_class == UTF8_TERM_CLASS_UNKNOWN)
      return NULL;
   if (feature_class != UTF8_TERM_CLASS_SHORT_ZWJ
   &&  feature_class != UTF8_TERM_CLASS_HEART_ZWJ
   &&  feature_class != UTF8_TERM_CLASS_FAMILY_ZWJ)
   {
      intent = UTF8_TERM_INTENT_NORMAL;
   }
   else if (intent == UTF8_TERM_INTENT_UNKNOWN
        ||  intent == UTF8_TERM_INTENT_NORMAL)
   {
      intent = UTF8_TERM_INTENT_GROUP;
   }

   entry = utf8_terminal_profile_lookup(feature_class, intent);
   if (entry != NULL)
      return entry;
   if (intent != UTF8_TERM_INTENT_NORMAL)
      return utf8_terminal_profile_lookup(feature_class,
                                          UTF8_TERM_INTENT_NORMAL);
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

static int tokenize_line(const char *line,
                         char tokens[UTF8_TERM_MAX_TOKENS][UTF8_TERM_TOKEN_MAX])
{
   const unsigned char *p = (const unsigned char *)line;
   int count = 0;

   if (line == NULL)
      return UTF8_TERMINAL_PROFILE_IGNORED;
   while (*p != '\0' && isspace(*p))
      p++;
   if (*p == '\0' || *p == '*' || *p == '#')
      return UTF8_TERMINAL_PROFILE_IGNORED;

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

Utf8TerminalIntent utf8_terminal_intent_from_name(const char *name)
{
   size_t i;

   for (i = 0; intent_names[i].name != NULL; i++)
   {
      if (ascii_equal_ci(intent_names[i].name, name))
         return intent_names[i].value;
   }
   return UTF8_TERM_INTENT_UNKNOWN;
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

const char *utf8_terminal_intent_name(Utf8TerminalIntent intent)
{
   size_t i;

   for (i = 0; intent_names[i].name != NULL; i++)
   {
      if (intent_names[i].value == intent)
         return intent_names[i].name;
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
         return "changed_cells";
      case UTF8_TERM_STRATEGY_LINE:
         return "line";
      case UTF8_TERM_STRATEGY_CLEAR_CHANGED_SUFFIX_FAST:
         return "clear_changed_suffix_fast";
      case UTF8_TERM_STRATEGY_CLEAR_FROM_FIRST_CLUSTER_FAST:
         return "clear_from_first_cluster_fast";
      case UTF8_TERM_STRATEGY_CLEAR_FROM_FIRST_CLUSTER_PAUSE:
         return "clear_from_first_cluster_pause";
      case UTF8_TERM_STRATEGY_CLEAR_WHOLE_FAST:
         return "clear_whole_fast";
      case UTF8_TERM_STRATEGY_CLEAR_FROM_ONE_PRIOR_CLUSTER:
         return "clear_from_one_prior_cluster";
      default:
         return "unknown";
   }
}

static Utf8TerminalOutput coerce_output_for_intent(Utf8TerminalIntent intent,
                                                   Utf8TerminalOutput output)
{
   if (intent == UTF8_TERM_INTENT_NORMAL)
      return UTF8_TERM_OUTPUT_NATIVE;
   if (intent == UTF8_TERM_INTENT_GROUP)
   {
      if (output == UTF8_TERM_OUTPUT_NATIVE
      ||  output == UTF8_TERM_OUTPUT_SUBSTITUTE)
         return output;
      return UTF8_TERM_OUTPUT_NATIVE;
   }
   if (intent == UTF8_TERM_INTENT_COMPONENTS)
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

static int apply_output(Utf8TerminalProfileEntry *entry, Utf8TerminalOutput output)
{
   if (entry == NULL || output == UTF8_TERM_OUTPUT_UNKNOWN)
      return UTF8_TERMINAL_PROFILE_INVALID;
   entry->output_method = coerce_output_for_intent(entry->display_intent, output);
   if (entry->output_method == UTF8_TERM_OUTPUT_SUBSTITUTE)
      apply_substitute_defaults(entry);
   return UTF8_TERMINAL_PROFILE_APPLIED;
}

static Utf8TerminalIntent legacy_intent_for_output(Utf8TerminalOutput output)
{
   if (output == UTF8_TERM_OUTPUT_EXPANDED)
      return UTF8_TERM_INTENT_COMPONENTS;
   return UTF8_TERM_INTENT_GROUP;
}

int utf8_terminal_profile_apply_line(const char *line)
{
   char tokens[UTF8_TERM_MAX_TOKENS][UTF8_TERM_TOKEN_MAX];
   int count;
   int index = 0;
   Utf8TerminalClass feature_class;
   Utf8TerminalIntent intent = UTF8_TERM_INTENT_NORMAL;
   Utf8TerminalProfileEntry *entry;

   ensure_profile_initialised();
   count = tokenize_line(line, tokens);
   if (count <= 0)
      return count;

   if (index < count && ascii_equal_ci(tokens[index], "set"))
      index++;
   if (index < count && ascii_equal_ci(tokens[index], "utf8"))
      index++;
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
      entry = profile_entry_for(feature_class, legacy_intent_for_output(output));
      return apply_output(entry, output);
   }

   if (index < count && ascii_equal_ci(tokens[index], "intent"))
   {
      if (index + 1 >= count)
         return UTF8_TERMINAL_PROFILE_INVALID;
      intent = utf8_terminal_intent_from_name(tokens[index + 1]);
      if (intent == UTF8_TERM_INTENT_UNKNOWN)
         return UTF8_TERMINAL_PROFILE_INVALID;
      index += 2;
   }

   entry = profile_entry_for(feature_class, intent);
   if (entry == NULL || index >= count)
      return UTF8_TERMINAL_PROFILE_INVALID;

   if (ascii_equal_ci(tokens[index], "output"))
   {
      Utf8TerminalOutput output;

      if (index + 2 != count)
         return UTF8_TERMINAL_PROFILE_INVALID;
      output = utf8_terminal_output_from_name(tokens[index + 1]);
      return apply_output(entry, output);
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

   profile_path = getenv("THE_UTF8_TERMINAL_PROFILE");
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
