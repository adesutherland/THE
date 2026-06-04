#ifndef THE_UTFCLUSTER_H
#define THE_UTFCLUSTER_H

#include <stddef.h>
#include <stdint.h>

#include "textpos.h"

enum
{
   UTF8_CLUSTER_MAX_CODEPOINTS = 32
};

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
   UTF8_TERM_CLASS_REGIONAL_INDICATOR,
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
   UTF8_CLUSTER_FACT_NONE = 0u,
   UTF8_CLUSTER_FACT_CONTAINS_KEYCAP = 1u << 0,
   UTF8_CLUSTER_FACT_CONTAINS_TEXT_VARIATION = 1u << 1,
   UTF8_CLUSTER_FACT_CONTAINS_EMOJI_VARIATION = 1u << 2,
   UTF8_CLUSTER_FACT_CONTAINS_MODIFIER = 1u << 3,
   UTF8_CLUSTER_FACT_CONTAINS_HEART = 1u << 4,
   UTF8_CLUSTER_FACT_CONTAINS_TAG = 1u << 5,
   UTF8_CLUSTER_FACT_CONTAINS_ZWJ = 1u << 6,
   UTF8_CLUSTER_FACT_ALL_REGIONAL = 1u << 7,
   UTF8_CLUSTER_FACT_CODEPOINTS_TRUNCATED = 1u << 8
} Utf8ClusterFactFlag;

typedef struct
{
   TextCluster cluster;
   Utf8TerminalClass feature_class;
   uint32_t codepoints[UTF8_CLUSTER_MAX_CODEPOINTS];
   size_t codepoint_count;
   size_t stored_codepoint_count;
   uint32_t first_codepoint;
   uint32_t keycap_base;
   int logical_width;
   int spacing_codepoints;
   int regional_codepoints;
   int zwj_count;
   unsigned int flags;
} Utf8ClusterFacts;

void utf8_cluster_facts_init(Utf8ClusterFacts *facts);
int utf8_cluster_collect_facts(const CHARTYPE *line, size_t len,
                               TextCluster cluster,
                               Utf8ClusterFacts *facts);
Utf8TerminalClass utf8_cluster_classify_facts(
   const Utf8ClusterFacts *facts);
Utf8TerminalClass utf8_cluster_classify(const CHARTYPE *line, size_t len,
                                        TextCluster cluster);
int utf8_cluster_keycap_base(const Utf8ClusterFacts *facts,
                             uint32_t *base_codepoint);

int utf8_cluster_codepoint_is_regional(uint32_t codepoint);
int utf8_cluster_codepoint_is_keycap_mark(uint32_t codepoint);
int utf8_cluster_codepoint_is_tag(uint32_t codepoint);
int utf8_cluster_codepoint_is_modifier(uint32_t codepoint);
int utf8_cluster_codepoint_is_private_use(uint32_t codepoint);
int utf8_cluster_codepoint_is_emojiish(uint32_t codepoint);

#endif
