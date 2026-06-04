#include <stdio.h>

#include "utfcluster.h"

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

static void expect_u32(const char *name, uint32_t got, uint32_t want)
{
   if (got != want)
   {
      fprintf(stderr, "%s: got U+%04X want U+%04X\n", name, got, want);
      failures++;
   }
}

static TextCluster cluster_after_leading_ascii(const CHARTYPE *line,
                                               size_t len)
{
   return textpos_cluster_at_boundary(line, len,
                                      textpos_next_cluster(line, len,
                                                           textpos_begin()));
}

static void expect_class(const char *name, const CHARTYPE *line, size_t len,
                         Utf8TerminalClass want)
{
   TextCluster cluster = cluster_after_leading_ascii(line, len);

   expect_int(name, utf8_cluster_classify(line, len, cluster), want);
}

static void test_keycap_facts(void)
{
   static const CHARTYPE keycap[] = { 'A', '1',
                                      0xEF, 0xB8, 0x8F,
                                      0xE2, 0x83, 0xA3, 'B' };
   TextCluster cluster;
   Utf8ClusterFacts facts;
   uint32_t base = 0;

   cluster = cluster_after_leading_ascii(keycap, sizeof(keycap));
   expect_int("keycap.collect",
              utf8_cluster_collect_facts(keycap, sizeof(keycap),
                                         cluster, &facts),
              1);
   expect_int("keycap.class", facts.feature_class, UTF8_TERM_CLASS_KEYCAP);
   expect_size("keycap.codepoints", facts.codepoint_count, 3);
   expect_size("keycap.stored", facts.stored_codepoint_count, 3);
   expect_u32("keycap.first", facts.first_codepoint, '1');
   expect_int("keycap.logical.width", facts.logical_width, 1);
   expect_int("keycap.flag.keycap",
              (facts.flags & UTF8_CLUSTER_FACT_CONTAINS_KEYCAP) != 0, 1);
   expect_int("keycap.flag.variation",
              (facts.flags & UTF8_CLUSTER_FACT_CONTAINS_EMOJI_VARIATION) != 0,
              1);
   expect_int("keycap.base.present", utf8_cluster_keycap_base(&facts, &base), 1);
   expect_u32("keycap.base", base, '1');
}

static void test_codepoint_helpers(void)
{
   expect_int("helper.keycap.mark.yes",
              utf8_cluster_codepoint_is_keycap_mark(0x20E3u), 1);
   expect_int("helper.keycap.mark.no",
              utf8_cluster_codepoint_is_keycap_mark(0x0301u), 0);
   expect_int("helper.regional.yes",
              utf8_cluster_codepoint_is_regional(0x1F1E6u), 1);
   expect_int("helper.regional.no",
              utf8_cluster_codepoint_is_regional('A'), 0);
}

static void test_existing_terminal_classes(void)
{
#ifdef USE_UTF8PROC
   static const CHARTYPE flag[] = { 'A',
                                    0xF0, 0x9F, 0x87, 0xAC,
                                    0xF0, 0x9F, 0x87, 0xA7, 'B' };
   static const CHARTYPE regional_indicator[] = { 'A',
                                                  0xF0, 0x9F, 0x87, 0xAC,
                                                  'B' };
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
   static const CHARTYPE text_heart[] = { 'A',
                                          0xE2, 0x99, 0xA5, 'B' };
   static const CHARTYPE text_checkbox[] = { 'A',
                                             0xE2, 0x98, 0x91, 'B' };
   static const CHARTYPE explicit_text_heart[] = { 'A',
                                                   0xE2, 0x99, 0xA5,
                                                   0xEF, 0xB8, 0x8E, 'B' };
   static const CHARTYPE emoji_heart[] = { 'A',
                                           0xE2, 0x99, 0xA5,
                                           0xEF, 0xB8, 0x8F, 'B' };
   static const CHARTYPE default_emoji_check[] = { 'A',
                                                   0xE2, 0x9C, 0x85, 'B' };
#endif
   static const CHARTYPE combining[] = { 'A', 'e', 0xCC, 0x81, 'B' };
   static const CHARTYPE private_use[] = { 'A', 0xEE, 0x80, 0x80, 'B' };

#ifdef USE_UTF8PROC
   expect_class("class.regional.flag", flag, sizeof(flag),
                UTF8_TERM_CLASS_REGIONAL_FLAG);
   expect_class("class.regional.indicator", regional_indicator,
                sizeof(regional_indicator),
                UTF8_TERM_CLASS_REGIONAL_INDICATOR);
   expect_class("class.short.zwj", short_zwj, sizeof(short_zwj),
                UTF8_TERM_CLASS_SHORT_ZWJ);
   expect_class("class.heart.zwj", heart_zwj, sizeof(heart_zwj),
                UTF8_TERM_CLASS_HEART_ZWJ);
   expect_class("class.family.zwj", family_zwj, sizeof(family_zwj),
                UTF8_TERM_CLASS_FAMILY_ZWJ);
   expect_class("class.modifier", modifier, sizeof(modifier),
                UTF8_TERM_CLASS_MODIFIER);
   expect_class("class.text.heart", text_heart, sizeof(text_heart),
                UTF8_TERM_CLASS_AMBIGUOUS);
   expect_class("class.text.checkbox", text_checkbox, sizeof(text_checkbox),
                UTF8_TERM_CLASS_AMBIGUOUS);
   expect_class("class.explicit.text.heart", explicit_text_heart,
                sizeof(explicit_text_heart), UTF8_TERM_CLASS_TEXT_VARIATION);
   expect_class("class.emoji.heart", emoji_heart, sizeof(emoji_heart),
                UTF8_TERM_CLASS_EMOJI_VARIATION);
   expect_class("class.default.emoji.check", default_emoji_check,
                sizeof(default_emoji_check), UTF8_TERM_CLASS_EMOJI);
#endif
   expect_class("class.combining", combining, sizeof(combining),
                UTF8_TERM_CLASS_COMBINING);
   expect_class("class.private", private_use, sizeof(private_use),
                UTF8_TERM_CLASS_PRIVATE_USE);
}

int main(void)
{
   test_keycap_facts();
   test_codepoint_helpers();
   test_existing_terminal_classes();

   if (failures != 0)
   {
      fprintf(stderr, "UTF cluster tests failed: %d\n", failures);
      return 1;
   }
   return 0;
}
