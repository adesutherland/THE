#include "utfcluster.h"

#include <string.h>

static TextPos utf8_cluster_advance_codepoint_pos(TextPos pos,
                                                  TextCodepoint item)
{
   if (item.byte_length == 0)
      return pos;
   pos.byte_offset += item.byte_length;
   pos.codepoint_index++;
   pos.cell_column += item.cell_width;
   return pos;
}

static int utf8_cluster_codepoint_is_keycap_base(uint32_t codepoint)
{
   return (codepoint >= '0' && codepoint <= '9')
       || codepoint == '#'
       || codepoint == '*';
}

int utf8_cluster_codepoint_is_regional(uint32_t codepoint)
{
   return codepoint >= 0x1F1E6u && codepoint <= 0x1F1FFu;
}

int utf8_cluster_codepoint_is_tag(uint32_t codepoint)
{
   return codepoint >= 0xE0020u && codepoint <= 0xE007Fu;
}

int utf8_cluster_codepoint_is_modifier(uint32_t codepoint)
{
   return codepoint >= 0x1F3FBu && codepoint <= 0x1F3FFu;
}

int utf8_cluster_codepoint_is_private_use(uint32_t codepoint)
{
   return (codepoint >= 0xE000u && codepoint <= 0xF8FFu)
       || (codepoint >= 0xF0000u && codepoint <= 0xFFFFDu)
       || (codepoint >= 0x100000u && codepoint <= 0x10FFFDu);
}

int utf8_cluster_codepoint_is_emojiish(uint32_t codepoint)
{
   return (codepoint >= 0x1F000u && codepoint <= 0x1FAFFu)
       || (codepoint >= 0x2600u && codepoint <= 0x27BFu);
}

void utf8_cluster_facts_init(Utf8ClusterFacts *facts)
{
   if (facts == NULL)
      return;
   memset(facts, 0, sizeof(*facts));
   facts->feature_class = UTF8_TERM_CLASS_UNKNOWN;
   facts->flags = UTF8_CLUSTER_FACT_ALL_REGIONAL;
}

int utf8_cluster_collect_facts(const CHARTYPE *line, size_t len,
                               TextCluster cluster,
                               Utf8ClusterFacts *facts)
{
   TextPos pos;

   if (facts == NULL)
      return 0;
   utf8_cluster_facts_init(facts);
   facts->cluster = cluster;
   facts->logical_width = (cluster.cell_width > 0) ? cluster.cell_width : 1;

   if (line == NULL || cluster.byte_length == 0)
      return 0;

   pos = cluster.pos;
   while (pos.byte_offset < cluster.end.byte_offset)
   {
      TextCodepoint item = textpos_codepoint_at_boundary(line, len, pos);

      if (item.byte_length == 0)
         break;

      if (facts->codepoint_count == 0)
         facts->first_codepoint = item.codepoint;
      if (facts->stored_codepoint_count < UTF8_CLUSTER_MAX_CODEPOINTS)
      {
         facts->codepoints[facts->stored_codepoint_count] = item.codepoint;
         facts->stored_codepoint_count++;
      }
      else
         facts->flags |= UTF8_CLUSTER_FACT_CODEPOINTS_TRUNCATED;
      facts->codepoint_count++;

      if (item.cell_width > 0)
         facts->spacing_codepoints++;
      if (utf8_cluster_codepoint_is_regional(item.codepoint))
         facts->regional_codepoints++;
      else
         facts->flags &= ~UTF8_CLUSTER_FACT_ALL_REGIONAL;

      if (item.codepoint == 0x200Du)
      {
         facts->zwj_count++;
         facts->flags |= UTF8_CLUSTER_FACT_CONTAINS_ZWJ;
      }
      else if (item.codepoint == 0x20E3u)
         facts->flags |= UTF8_CLUSTER_FACT_CONTAINS_KEYCAP;
      else if (item.codepoint == 0xFE0Eu)
         facts->flags |= UTF8_CLUSTER_FACT_CONTAINS_TEXT_VARIATION;
      else if (item.codepoint == 0xFE0Fu)
         facts->flags |= UTF8_CLUSTER_FACT_CONTAINS_EMOJI_VARIATION;
      else if (utf8_cluster_codepoint_is_modifier(item.codepoint))
         facts->flags |= UTF8_CLUSTER_FACT_CONTAINS_MODIFIER;
      else if (item.codepoint == 0x2764u)
         facts->flags |= UTF8_CLUSTER_FACT_CONTAINS_HEART;
      else if (utf8_cluster_codepoint_is_tag(item.codepoint))
         facts->flags |= UTF8_CLUSTER_FACT_CONTAINS_TAG;

      if (facts->keycap_base == 0
      &&  utf8_cluster_codepoint_is_keycap_base(item.codepoint))
         facts->keycap_base = item.codepoint;

      pos = utf8_cluster_advance_codepoint_pos(pos, item);
   }

   facts->feature_class = utf8_cluster_classify_facts(facts);
   return facts->codepoint_count > 0;
}

Utf8TerminalClass utf8_cluster_classify_facts(
   const Utf8ClusterFacts *facts)
{
   if (facts == NULL || facts->codepoint_count == 0)
      return UTF8_TERM_CLASS_UNKNOWN;
   if (facts->flags & UTF8_CLUSTER_FACT_CONTAINS_KEYCAP)
      return UTF8_TERM_CLASS_KEYCAP;
   if ((facts->flags & UTF8_CLUSTER_FACT_ALL_REGIONAL)
   &&  facts->regional_codepoints == 2)
      return UTF8_TERM_CLASS_REGIONAL_FLAG;
   if ((facts->flags & UTF8_CLUSTER_FACT_ALL_REGIONAL)
   &&  facts->regional_codepoints == 1)
      return UTF8_TERM_CLASS_REGIONAL_INDICATOR;
   if (facts->flags & UTF8_CLUSTER_FACT_CONTAINS_TAG)
      return UTF8_TERM_CLASS_TAG_FLAG;
   if (facts->zwj_count > 0)
   {
      if (facts->flags & UTF8_CLUSTER_FACT_CONTAINS_HEART)
         return UTF8_TERM_CLASS_HEART_ZWJ;
      if (facts->zwj_count >= 2 || facts->spacing_codepoints >= 3)
         return UTF8_TERM_CLASS_FAMILY_ZWJ;
      return UTF8_TERM_CLASS_SHORT_ZWJ;
   }
   if (facts->flags & UTF8_CLUSTER_FACT_CONTAINS_MODIFIER)
      return UTF8_TERM_CLASS_MODIFIER;
   if (facts->flags & UTF8_CLUSTER_FACT_CONTAINS_EMOJI_VARIATION)
      return UTF8_TERM_CLASS_EMOJI_VARIATION;
   if (facts->flags & UTF8_CLUSTER_FACT_CONTAINS_TEXT_VARIATION)
      return UTF8_TERM_CLASS_TEXT_VARIATION;
   if (facts->codepoint_count > 1)
   {
      if (facts->cluster.cell_width <= 1)
      {
         if (facts->codepoint_count > 2)
            return UTF8_TERM_CLASS_COMBINING_STACK;
         return UTF8_TERM_CLASS_COMBINING;
      }
      if (utf8_cluster_codepoint_is_emojiish(facts->first_codepoint))
         return UTF8_TERM_CLASS_EMOJI;
      return UTF8_TERM_CLASS_WIDE;
   }
   if (utf8_cluster_codepoint_is_private_use(facts->first_codepoint))
      return UTF8_TERM_CLASS_PRIVATE_USE;
   if (facts->cluster.cell_width >= 2)
   {
      if (utf8_cluster_codepoint_is_emojiish(facts->first_codepoint))
         return UTF8_TERM_CLASS_EMOJI;
      return UTF8_TERM_CLASS_WIDE;
   }
   if (facts->first_codepoint < 0x80u)
      return UTF8_TERM_CLASS_ASCII;
   if (facts->cluster.cell_width == 0)
      return UTF8_TERM_CLASS_COMBINING;
   return UTF8_TERM_CLASS_AMBIGUOUS;
}

Utf8TerminalClass utf8_cluster_classify(const CHARTYPE *line, size_t len,
                                        TextCluster cluster)
{
   Utf8ClusterFacts facts;

   if (!utf8_cluster_collect_facts(line, len, cluster, &facts))
      return UTF8_TERM_CLASS_UNKNOWN;
   return facts.feature_class;
}

int utf8_cluster_keycap_base(const Utf8ClusterFacts *facts,
                             uint32_t *base_codepoint)
{
   if (facts == NULL
   ||  !(facts->flags & UTF8_CLUSTER_FACT_CONTAINS_KEYCAP)
   ||  facts->keycap_base == 0)
      return 0;
   if (base_codepoint != NULL)
      *base_codepoint = facts->keycap_base;
   return 1;
}
