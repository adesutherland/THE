#include <stdio.h>
#include <stdlib.h>

#include "textpos.h"
#include "utf8term.h"

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

static const Utf8TerminalProfileEntry *expect_entry(
   const char *name, Utf8TerminalClass feature_class, Utf8TerminalIntent intent)
{
   const Utf8TerminalProfileEntry *entry = utf8_terminal_profile_lookup(feature_class, intent);

   if (entry == NULL)
   {
      fprintf(stderr, "%s: missing profile entry\n", name);
      failures++;
   }
   return entry;
}

static void expect_profile(const char *name,
                           Utf8TerminalClass feature_class,
                           Utf8TerminalIntent intent,
                           Utf8TerminalOutput output,
                           int layout_width,
                           int cursor_width,
                           Utf8TerminalStrategy cursor_strategy,
                           Utf8TerminalStrategy replacement_strategy)
{
   const Utf8TerminalProfileEntry *entry = expect_entry(name, feature_class, intent);

   if (entry == NULL)
      return;
   expect_int(name, entry->output_method, output);
   expect_int(name, entry->layout_width, layout_width);
   expect_int(name, entry->cursor_width, cursor_width);
   expect_int(name, entry->cursor_strategy, cursor_strategy);
   expect_int(name, entry->replacement_strategy, replacement_strategy);
}

static TextCluster cluster_after_leading_ascii(const CHARTYPE *line, size_t len)
{
   TextPos pos = textpos_next_cluster(line, len, textpos_begin());

   return textpos_cluster_at_boundary(line, len, pos);
}

static void test_coded_defaults(void)
{
   utf8_terminal_profile_reset();
   expect_size("entry.count", utf8_terminal_profile_entry_count(), 19);
   expect_profile("default.ascii", UTF8_TERM_CLASS_ASCII, UTF8_TERM_INTENT_NORMAL,
                  UTF8_TERM_OUTPUT_NATIVE, 1, 1,
                  UTF8_TERM_STRATEGY_CHANGED_CELLS,
                  UTF8_TERM_STRATEGY_CHANGED_CELLS);
   expect_profile("default.keycap", UTF8_TERM_CLASS_KEYCAP, UTF8_TERM_INTENT_NORMAL,
                  UTF8_TERM_OUTPUT_NATIVE, 2, 2,
                  UTF8_TERM_STRATEGY_CLEAR_FROM_FIRST_CLUSTER_FAST,
                  UTF8_TERM_STRATEGY_CLEAR_WHOLE_FAST);
   expect_profile("default.flag", UTF8_TERM_CLASS_REGIONAL_FLAG, UTF8_TERM_INTENT_NORMAL,
                  UTF8_TERM_OUTPUT_NATIVE, 3, 3,
                  UTF8_TERM_STRATEGY_CHANGED_CELLS,
                  UTF8_TERM_STRATEGY_CLEAR_CHANGED_SUFFIX_FAST);
   expect_profile("default.short.zwj.group", UTF8_TERM_CLASS_SHORT_ZWJ,
                  UTF8_TERM_INTENT_GROUP, UTF8_TERM_OUTPUT_NATIVE, 2, 2,
                  UTF8_TERM_STRATEGY_LINE, UTF8_TERM_STRATEGY_LINE);
}

static void test_line_parser(void)
{
   utf8_terminal_profile_reset();
   expect_int("line.layout",
              utf8_terminal_profile_apply_line(
                 "SET UTF8 TERMINAL CLASS keycap LAYOUT 9 CURSOR 8"),
              UTF8_TERMINAL_PROFILE_APPLIED);
   expect_profile("line.keycap.layout", UTF8_TERM_CLASS_KEYCAP,
                  UTF8_TERM_INTENT_NORMAL, UTF8_TERM_OUTPUT_NATIVE, 9, 8,
                  UTF8_TERM_STRATEGY_CLEAR_FROM_FIRST_CLUSTER_FAST,
                  UTF8_TERM_STRATEGY_CLEAR_WHOLE_FAST);

   expect_int("line.replacement",
              utf8_terminal_profile_apply_line(
                 "terminal class keycap replacestrategy clear_from_first_cluster_fast"),
              UTF8_TERMINAL_PROFILE_APPLIED);
   expect_profile("line.keycap.replacement", UTF8_TERM_CLASS_KEYCAP,
                  UTF8_TERM_INTENT_NORMAL, UTF8_TERM_OUTPUT_NATIVE, 9, 8,
                  UTF8_TERM_STRATEGY_CLEAR_FROM_FIRST_CLUSTER_FAST,
                  UTF8_TERM_STRATEGY_CLEAR_FROM_FIRST_CLUSTER_FAST);

   expect_int("line.invalid.class",
              utf8_terminal_profile_apply_line(
                 "SET UTF8 TERMINAL CLASS made-up LAYOUT 1 CURSOR 1"),
              UTF8_TERMINAL_PROFILE_INVALID);
}

static void test_profile_files(const char *defaults_path, const char *macos_path)
{
   int loaded = 0;

   utf8_terminal_profile_reset();
   expect_int("defaults.file",
              utf8_terminal_profile_apply_file(defaults_path, &loaded),
              UTF8_TERMINAL_PROFILE_APPLIED);
   expect_int("defaults.loaded", loaded, 63);
   expect_profile("defaults.file.keycap", UTF8_TERM_CLASS_KEYCAP,
                  UTF8_TERM_INTENT_NORMAL, UTF8_TERM_OUTPUT_NATIVE, 2, 2,
                  UTF8_TERM_STRATEGY_CLEAR_FROM_FIRST_CLUSTER_FAST,
                  UTF8_TERM_STRATEGY_CLEAR_WHOLE_FAST);

   expect_int("macos.file",
              utf8_terminal_profile_apply_file(macos_path, &loaded),
              UTF8_TERMINAL_PROFILE_APPLIED);
   expect_int("macos.loaded", loaded, 45);
   expect_profile("macos.modifier", UTF8_TERM_CLASS_MODIFIER,
                  UTF8_TERM_INTENT_NORMAL, UTF8_TERM_OUTPUT_NATIVE, 4, 4,
                  UTF8_TERM_STRATEGY_CHANGED_CELLS,
                  UTF8_TERM_STRATEGY_LINE);
   expect_profile("macos.keycap", UTF8_TERM_CLASS_KEYCAP,
                  UTF8_TERM_INTENT_NORMAL, UTF8_TERM_OUTPUT_NATIVE, 2, 2,
                  UTF8_TERM_STRATEGY_CLEAR_FROM_FIRST_CLUSTER_FAST,
                  UTF8_TERM_STRATEGY_CLEAR_FROM_FIRST_CLUSTER_FAST);
   expect_profile("macos.short.group", UTF8_TERM_CLASS_SHORT_ZWJ,
                  UTF8_TERM_INTENT_GROUP, UTF8_TERM_OUTPUT_SUBSTITUTE, 1, 1,
                  UTF8_TERM_STRATEGY_CHANGED_CELLS,
                  UTF8_TERM_STRATEGY_CHANGED_CELLS);
   expect_profile("macos.short.components", UTF8_TERM_CLASS_SHORT_ZWJ,
                  UTF8_TERM_INTENT_COMPONENTS, UTF8_TERM_OUTPUT_NATIVE, 4, 4,
                  UTF8_TERM_STRATEGY_CHANGED_CELLS,
                  UTF8_TERM_STRATEGY_LINE);
}

static void test_terminal_identity(void)
{
   utf8_terminal_profile_reset();
   expect_int("identity.apple",
              utf8_terminal_profile_apply_terminal_identity(
                 "xterm-256color", "Apple_Terminal"),
              45);
   expect_profile("identity.apple.keycap", UTF8_TERM_CLASS_KEYCAP,
                  UTF8_TERM_INTENT_NORMAL, UTF8_TERM_OUTPUT_NATIVE, 2, 2,
                  UTF8_TERM_STRATEGY_CLEAR_FROM_FIRST_CLUSTER_FAST,
                  UTF8_TERM_STRATEGY_CLEAR_FROM_FIRST_CLUSTER_FAST);
}

static void test_cluster_classification(void)
{
#ifdef USE_UTF8PROC
   static const CHARTYPE keycap[] = { 'A', '1',
                                      0xEF, 0xB8, 0x8F,
                                      0xE2, 0x83, 0xA3, 'B' };
   static const CHARTYPE flag[] = { 'A',
                                    0xF0, 0x9F, 0x87, 0xAC,
                                    0xF0, 0x9F, 0x87, 0xA7, 'B' };
   static const CHARTYPE short_zwj[] = { 'A',
                                         0xF0, 0x9F, 0x91, 0xA9,
                                         0xE2, 0x80, 0x8D,
                                         0xF0, 0x9F, 0x92, 0xBB, 'B' };
   static const CHARTYPE heart_zwj[] = { 'A',
                                         0xF0, 0x9F, 0x91, 0xA9,
                                         0xE2, 0x80, 0x8D,
                                         0xE2, 0x9D, 0xA4,
                                         0xEF, 0xB8, 0x8F,
                                         0xE2, 0x80, 0x8D,
                                         0xF0, 0x9F, 0x91, 0xA8, 'B' };
   static const CHARTYPE family_zwj[] = { 'A',
                                          0xF0, 0x9F, 0x91, 0xA8,
                                          0xE2, 0x80, 0x8D,
                                          0xF0, 0x9F, 0x91, 0xA9,
                                          0xE2, 0x80, 0x8D,
                                          0xF0, 0x9F, 0x91, 0xA7,
                                          0xE2, 0x80, 0x8D,
                                          0xF0, 0x9F, 0x91, 0xA6, 'B' };
   static const CHARTYPE modifier[] = { 'A',
                                        0xF0, 0x9F, 0x91, 0x8D,
                                        0xF0, 0x9F, 0x8F, 0xBB, 'B' };

   expect_int("class.keycap",
              utf8_terminal_classify_cluster(
                 keycap, sizeof(keycap),
                 cluster_after_leading_ascii(keycap, sizeof(keycap))),
              UTF8_TERM_CLASS_KEYCAP);
   expect_int("class.regional.flag",
              utf8_terminal_classify_cluster(
                 flag, sizeof(flag),
                 cluster_after_leading_ascii(flag, sizeof(flag))),
              UTF8_TERM_CLASS_REGIONAL_FLAG);
   expect_int("class.short.zwj",
              utf8_terminal_classify_cluster(
                 short_zwj, sizeof(short_zwj),
                 cluster_after_leading_ascii(short_zwj, sizeof(short_zwj))),
              UTF8_TERM_CLASS_SHORT_ZWJ);
   expect_int("class.heart.zwj",
              utf8_terminal_classify_cluster(
                 heart_zwj, sizeof(heart_zwj),
                 cluster_after_leading_ascii(heart_zwj, sizeof(heart_zwj))),
              UTF8_TERM_CLASS_HEART_ZWJ);
   expect_int("class.family.zwj",
              utf8_terminal_classify_cluster(
                 family_zwj, sizeof(family_zwj),
                 cluster_after_leading_ascii(family_zwj, sizeof(family_zwj))),
              UTF8_TERM_CLASS_FAMILY_ZWJ);
   expect_int("class.modifier",
              utf8_terminal_classify_cluster(
                 modifier, sizeof(modifier),
                 cluster_after_leading_ascii(modifier, sizeof(modifier))),
              UTF8_TERM_CLASS_MODIFIER);
#endif
}

static void test_cluster_profile_lookup(void)
{
#ifdef USE_UTF8PROC
   static const CHARTYPE short_zwj[] = { 'A',
                                         0xF0, 0x9F, 0x91, 0xA9,
                                         0xE2, 0x80, 0x8D,
                                         0xF0, 0x9F, 0x92, 0xBB, 'B' };
   TextCluster cluster;
   const Utf8TerminalProfileEntry *entry;

   cluster = cluster_after_leading_ascii(short_zwj, sizeof(short_zwj));
   utf8_terminal_profile_reset();
   entry = utf8_terminal_profile_lookup_cluster(short_zwj, sizeof(short_zwj),
                                                cluster,
                                                UTF8_TERM_INTENT_GROUP);
   if (entry == NULL)
   {
      fprintf(stderr, "lookup.short.group: missing profile entry\n");
      failures++;
   }
   else
   {
      expect_int("lookup.short.group.output", entry->output_method,
                 UTF8_TERM_OUTPUT_NATIVE);
      expect_int("lookup.short.group.layout", entry->layout_width, 2);
   }

   expect_int("lookup.apple.apply",
              utf8_terminal_profile_apply_apple_terminal(), 45);
   entry = utf8_terminal_profile_lookup_cluster(short_zwj, sizeof(short_zwj),
                                                cluster,
                                                UTF8_TERM_INTENT_GROUP);
   if (entry == NULL)
   {
      fprintf(stderr, "lookup.apple.short.group: missing profile entry\n");
      failures++;
   }
   else
   {
      expect_int("lookup.apple.short.group.output", entry->output_method,
                 UTF8_TERM_OUTPUT_SUBSTITUTE);
      expect_int("lookup.apple.short.group.layout", entry->layout_width, 1);
   }

   entry = utf8_terminal_profile_lookup_cluster(short_zwj, sizeof(short_zwj),
                                                cluster,
                                                UTF8_TERM_INTENT_COMPONENTS);
   if (entry == NULL)
   {
      fprintf(stderr, "lookup.apple.short.components: missing profile entry\n");
      failures++;
   }
   else
   {
      expect_int("lookup.apple.short.components.output", entry->output_method,
                 UTF8_TERM_OUTPUT_NATIVE);
      expect_int("lookup.apple.short.components.layout", entry->layout_width, 4);
   }
#endif
}

static void test_physical_policy_does_not_change_logical_textpos(void)
{
#ifdef USE_UTF8PROC
   static const CHARTYPE keycap[] = { 'A', '1',
                                      0xEF, 0xB8, 0x8F,
                                      0xE2, 0x83, 0xA3, 'B' };
   TextPos pos;
   TextCluster cluster;

   utf8_terminal_profile_reset();
   expect_int("logical.profile.apply",
              utf8_terminal_profile_apply_line(
                 "SET UTF8 TERMINAL CLASS keycap LAYOUT 9 CURSOR 9"),
              UTF8_TERMINAL_PROFILE_APPLIED);

   pos = textpos_next_cluster(keycap, sizeof(keycap), textpos_begin());
   cluster = textpos_cluster_at(keycap, sizeof(keycap), pos);
   expect_size("logical.keycap.cluster.count",
               textpos_count_clusters(keycap, sizeof(keycap)), 3);
   expect_int("logical.keycap.cluster.width", cluster.cell_width, 1);
#endif
}

int main(int argc, char **argv)
{
   if (argc != 3)
   {
      fprintf(stderr, "usage: %s defaults-poc34.the macos-overrides.the\n", argv[0]);
      return 2;
   }

   test_coded_defaults();
   test_line_parser();
   test_profile_files(argv[1], argv[2]);
   test_terminal_identity();
   test_cluster_classification();
   test_cluster_profile_lookup();
   test_physical_policy_does_not_change_logical_textpos();

   if (failures != 0)
   {
      fprintf(stderr, "utf8term tests failed: %d\n", failures);
      return 1;
   }
   return 0;
}
