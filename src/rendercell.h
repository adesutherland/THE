#ifndef THE_RENDERCELL_H
#define THE_RENDERCELL_H

#include <stddef.h>
#include <stdint.h>
#include <wchar.h>

#include "textpos.h"
#include "thecolour.h"
#include "utfterm.h"

enum
{
   THE_RENDER_MAX_CODEPOINTS = 32,
   THE_RENDER_MAX_UTF8_BYTES = 128,
   THE_RENDER_MAX_FALLBACK_BYTES = 8
};

typedef enum
{
   THE_RENDER_CLUSTER_VALID = 1u << 0,
   THE_RENDER_CLUSTER_HAS_UTF8 = 1u << 1,
   THE_RENDER_CLUSTER_SUBSTITUTE = 1u << 2,
   THE_RENDER_CLUSTER_EXPANDED = 1u << 3,
   THE_RENDER_CLUSTER_CODEPOINTS_TRUNCATED = 1u << 4,
   THE_RENDER_CLUSTER_UTF8_TRUNCATED = 1u << 5,
   THE_RENDER_CLUSTER_HAS_FALLBACK = 1u << 6
} TheRenderClusterFlag;

typedef struct
{
   uint32_t codepoints[THE_RENDER_MAX_CODEPOINTS];
   size_t codepoint_count;
   CHARTYPE utf8[THE_RENDER_MAX_UTF8_BYTES];
   size_t utf8_length;
   uint32_t fallback_codepoint;
   CHARTYPE fallback_utf8[THE_RENDER_MAX_FALLBACK_BYTES];
   size_t fallback_length;
   TheRenderAttr attr;
   /* Logical editor width from TextCluster/TextPos semantics. */
   int logical_width;
   /* Physical terminal cells reserved for the rendered output. */
   int display_width;
   /* Physical terminal cells covered by cursor presentation. */
   int cursor_width;
   /* Physical terminal cells that may need clearing/repainting. */
   int paint_width;
   Utf8TerminalStrategy repair_strategy;
   unsigned int flags;
} TheRenderCluster;

typedef TheRenderCluster TheRenderCell;

void the_render_cluster_init(TheRenderCluster *cluster, TheRenderAttr attr);
void the_render_cluster_set_attr(TheRenderCluster *cluster,
                                 TheRenderAttr attr);
void the_render_cluster_set_widths(TheRenderCluster *cluster,
                                   int logical_width, int display_width,
                                   int cursor_width, int paint_width);
void the_render_cluster_set_repair_strategy(TheRenderCluster *cluster,
                                            Utf8TerminalStrategy strategy);
int the_render_cluster_add_codepoint(TheRenderCluster *cluster,
                                     uint32_t codepoint);
void the_render_cluster_set_utf8(TheRenderCluster *cluster,
                                 const CHARTYPE *text, size_t len);
void the_render_cluster_set_fallback_codepoint(TheRenderCluster *cluster,
                                               uint32_t codepoint);
int the_render_cell_from_codepoint(TheRenderCell *cell, uint32_t codepoint,
                                   TheRenderAttr attr);
int the_render_cluster_from_text_cluster(TheRenderCluster *dest,
                                         const CHARTYPE *line, size_t len,
                                         TextCluster cluster,
                                         TheRenderAttr attr,
                                         int force_expanded);
void the_render_cluster_recolour(TheRenderCluster *cluster,
                                 TheRenderAttr attr);
int the_render_cluster_to_wchars(const TheRenderCluster *cluster,
                                 wchar_t *out, size_t out_size);

#endif
