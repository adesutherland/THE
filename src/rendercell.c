#include "rendercell.h"

#include <string.h>

#include "utflayout.h"

static int render_width_or_one(int width)
{
   return (width > 0) ? width : 1;
}

static uint32_t render_valid_codepoint(uint32_t codepoint)
{
   if ((codepoint >= 0xD800u && codepoint <= 0xDFFFu)
   ||  codepoint > 0x10FFFFu)
      return TEXT_INVALID_CODEPOINT;
   return codepoint;
}

static void render_codepoint_to_wchars(uint32_t codepoint, wchar_t out[3])
{
   codepoint = render_valid_codepoint(codepoint);
#if defined(WCHAR_MAX) && WCHAR_MAX <= 0xFFFFu
   if (codepoint > 0xFFFFu)
   {
      codepoint -= 0x10000u;
      out[0] = (wchar_t)(0xD800u + (codepoint >> 10));
      out[1] = (wchar_t)(0xDC00u + (codepoint & 0x3FFu));
      out[2] = L'\0';
      return;
   }
#endif
   out[0] = (wchar_t)codepoint;
   out[1] = L'\0';
}

void the_render_cluster_init(TheRenderCluster *cluster, TheRenderAttr attr)
{
   if (cluster == NULL)
      return;
   memset(cluster, 0, sizeof(*cluster));
   cluster->attr = attr;
   cluster->fallback_codepoint = '?';
   cluster->fallback_utf8[0] = '?';
   cluster->fallback_length = 1;
   cluster->feature_class = UTF8_TERM_CLASS_UNKNOWN;
   cluster->output_method = UTF8_TERM_OUTPUT_NATIVE;
   cluster->mark = UTF8_TERM_MARK_NONE;
   cluster->logical_width = 1;
   cluster->width = 1;
   cluster->advance_width = 1;
   cluster->cursor_width = 1;
   cluster->repaint_width = 1;
   cluster->repair_strategy = UTF8_TERM_STRATEGY_CHANGED_CELLS;
   cluster->flags = THE_RENDER_CLUSTER_HAS_FALLBACK;
}

void the_render_cluster_set_attr(TheRenderCluster *cluster,
                                 TheRenderAttr attr)
{
   if (cluster != NULL)
      cluster->attr = attr;
}

void the_render_cluster_set_widths(TheRenderCluster *cluster,
                                   int logical_width, int width,
                                   int advance_width, int cursor_width,
                                   int repaint_width)
{
   if (cluster == NULL)
      return;
   cluster->logical_width = render_width_or_one(logical_width);
   cluster->width = render_width_or_one(width);
   cluster->advance_width = render_width_or_one(advance_width);
   cluster->cursor_width = render_width_or_one(cursor_width);
   cluster->repaint_width = render_width_or_one(repaint_width);
}

void the_render_cluster_set_repair_strategy(TheRenderCluster *cluster,
                                            Utf8TerminalStrategy strategy)
{
   if (cluster != NULL)
      cluster->repair_strategy = strategy;
}

int the_render_cluster_add_codepoint(TheRenderCluster *cluster,
                                     uint32_t codepoint)
{
   if (cluster == NULL)
      return 0;
   if (cluster->codepoint_count >= THE_RENDER_MAX_CODEPOINTS)
   {
      cluster->flags |= THE_RENDER_CLUSTER_CODEPOINTS_TRUNCATED;
      return 0;
   }
   cluster->codepoints[cluster->codepoint_count++] =
      render_valid_codepoint(codepoint);
   cluster->flags |= THE_RENDER_CLUSTER_VALID;
   return 1;
}

void the_render_cluster_set_utf8(TheRenderCluster *cluster,
                                 const CHARTYPE *text, size_t len)
{
   size_t copy_len;

   if (cluster == NULL)
      return;
   cluster->utf8_length = 0;
   if (text == NULL || len == 0)
      return;
   copy_len = len;
   if (copy_len > THE_RENDER_MAX_UTF8_BYTES)
   {
      copy_len = THE_RENDER_MAX_UTF8_BYTES;
      cluster->flags |= THE_RENDER_CLUSTER_UTF8_TRUNCATED;
   }
   memcpy(cluster->utf8, text, copy_len);
   cluster->utf8_length = copy_len;
   cluster->flags |= THE_RENDER_CLUSTER_HAS_UTF8;
}

void the_render_cluster_set_fallback_codepoint(TheRenderCluster *cluster,
                                               uint32_t codepoint)
{
   CHARTYPE encoded[4];
   size_t len;

   if (cluster == NULL)
      return;
   codepoint = render_valid_codepoint(codepoint);
   cluster->fallback_codepoint = codepoint;
   len = text_utf8_encode(codepoint, encoded);
   if (len == 0 || len > THE_RENDER_MAX_FALLBACK_BYTES)
   {
      cluster->fallback_utf8[0] = '?';
      cluster->fallback_length = 1;
   }
   else
   {
      memcpy(cluster->fallback_utf8, encoded, len);
      cluster->fallback_length = len;
   }
   cluster->flags |= THE_RENDER_CLUSTER_HAS_FALLBACK;
}

int the_render_cell_from_codepoint(TheRenderCell *cell, uint32_t codepoint,
                                   TheRenderAttr attr)
{
   int width;

   if (cell == NULL)
      return 0;
   the_render_cluster_init(cell, attr);
   the_render_cluster_add_codepoint(cell, codepoint);
   the_render_cluster_set_fallback_codepoint(
      cell, codepoint <= 0x7Fu ? codepoint : '?');
   width = text_codepoint_cell_width(codepoint);
   the_render_cluster_set_widths(cell, render_width_or_one(width),
                                 render_width_or_one(width),
                                 render_width_or_one(width),
                                 render_width_or_one(width),
                                 render_width_or_one(width));
   return 1;
}

int the_render_cluster_from_text_cluster(TheRenderCluster *dest,
                                         const CHARTYPE *line, size_t len,
                                         TextCluster cluster,
                                         TheRenderAttr attr,
                                         int force_expanded)
{
   const Utf8TerminalProfileEntry *entry;
   Utf8ClusterFacts facts;
   int have_facts;
   TextPos pos;

   if (dest == NULL || line == NULL || cluster.byte_length == 0)
      return 0;

   the_render_cluster_init(dest, attr);
   the_render_cluster_set_utf8(dest, line + cluster.pos.byte_offset,
                               cluster.byte_length);

   have_facts = utf8_cluster_collect_facts(line, len, cluster, &facts);
   if (have_facts)
      dest->feature_class = facts.feature_class;

   entry = utf8_layout_cluster_profile(line, len, cluster);
   if (entry != NULL)
   {
      dest->feature_class = entry->feature_class;
      dest->output_method = entry->output_method;
      dest->mark = entry->mark;
      dest->repair_strategy = entry->replacement_strategy;
      the_render_cluster_set_fallback_codepoint(dest,
                                                entry->substitute_codepoint);
      if (entry->output_method == UTF8_TERM_OUTPUT_SUBSTITUTE)
      {
         dest->flags |= THE_RENDER_CLUSTER_SUBSTITUTE;
      }
      else if (entry->output_method == UTF8_TERM_OUTPUT_BASE)
         dest->flags |= THE_RENDER_CLUSTER_BASE;
      else if (entry->output_method == UTF8_TERM_OUTPUT_COMPONENTS)
         dest->flags |= THE_RENDER_CLUSTER_COMPONENTS;
      else if (force_expanded
      ||       entry->output_method == UTF8_TERM_OUTPUT_EXPANDED)
      {
         dest->flags |= THE_RENDER_CLUSTER_EXPANDED;
      }
   }
   else if (force_expanded)
      dest->flags |= THE_RENDER_CLUSTER_EXPANDED;

   pos = cluster.pos;
   while (pos.byte_offset < cluster.end.byte_offset)
   {
      TextCodepoint item = textpos_codepoint_at_boundary(line, len, pos);

      if (item.byte_length == 0)
         break;
      the_render_cluster_add_codepoint(dest, item.codepoint);
      pos.byte_offset += item.byte_length;
      pos.codepoint_index++;
      pos.cell_column += item.cell_width;
   }

   the_render_cluster_set_widths(dest,
      utf8_layout_cluster_logical_width(cluster),
      utf8_layout_cluster_width(line, len, cluster),
      utf8_layout_cluster_advance_width(line, len, cluster),
      utf8_layout_cluster_cursor_width(line, len, cluster),
      utf8_layout_cluster_repaint_width(line, len, cluster));
   return dest->codepoint_count > 0
       ||  (dest->flags & THE_RENDER_CLUSTER_SUBSTITUTE);
}

void the_render_cluster_recolour(TheRenderCluster *cluster,
                                 TheRenderAttr attr)
{
   if (cluster != NULL)
      cluster->attr = attr;
}

static int render_append_codepoint(wchar_t *out, size_t out_size,
                                   size_t *used, uint32_t codepoint)
{
   wchar_t one[3];
   int j;

   if (out == NULL || used == NULL || out_size == 0)
      return 0;
   render_codepoint_to_wchars(codepoint, one);
   for (j = 0; one[j] != L'\0'; j++)
   {
      if (*used >= out_size - 1)
         return 0;
      out[(*used)++] = one[j];
   }
   return 1;
}

static int render_cluster_base_to_wchars(const TheRenderCluster *cluster,
                                         wchar_t *out, size_t out_size,
                                         size_t *used)
{
   size_t i;

   switch (cluster->feature_class)
   {
      case UTF8_TERM_CLASS_KEYCAP:
         for (i = 0; i < cluster->codepoint_count; i++)
         {
            uint32_t codepoint = cluster->codepoints[i];

            if ((codepoint >= '0' && codepoint <= '9')
            ||  codepoint == '#'
            ||  codepoint == '*')
               return render_append_codepoint(out, out_size, used, codepoint);
         }
         return 0;

      case UTF8_TERM_CLASS_REGIONAL_FLAG:
         if (cluster->codepoint_count < 2)
            return 0;
         for (i = 0; i < 2; i++)
         {
            uint32_t codepoint = cluster->codepoints[i];

            if (!utf8_cluster_codepoint_is_regional(codepoint))
               return 0;
            if (!render_append_codepoint(out, out_size, used,
                                         'A' + codepoint - 0x1F1E6u))
               return 0;
         }
         return 1;

      case UTF8_TERM_CLASS_EMOJI_VARIATION:
      case UTF8_TERM_CLASS_TEXT_VARIATION:
      case UTF8_TERM_CLASS_MODIFIER:
         for (i = 0; i < cluster->codepoint_count; i++)
         {
            uint32_t codepoint = cluster->codepoints[i];

            if (codepoint == 0xFE0Eu || codepoint == 0xFE0Fu
            ||  utf8_cluster_codepoint_is_modifier(codepoint))
               continue;
            return render_append_codepoint(out, out_size, used, codepoint);
         }
         return 0;

      default:
         return 0;
   }
}

static int render_cluster_components_to_wchars(const TheRenderCluster *cluster,
                                               wchar_t *out, size_t out_size,
                                               size_t *used)
{
   size_t i;
   int emitted = 0;

   for (i = 0; i < cluster->codepoint_count; i++)
   {
      uint32_t codepoint = cluster->codepoints[i];

      if (codepoint == 0x200Du
      ||  codepoint == 0xFE0Eu
      ||  codepoint == 0xFE0Fu)
         continue;
      if (!render_append_codepoint(out, out_size, used, codepoint))
         return 0;
      emitted = 1;
   }
   return emitted;
}

int the_render_cluster_to_wchars(const TheRenderCluster *cluster,
                                 wchar_t *out, size_t out_size)
{
   size_t i;
   size_t used = 0;

   if (cluster == NULL || out == NULL || out_size == 0)
      return 0;

   if (cluster->flags & THE_RENDER_CLUSTER_SUBSTITUTE)
   {
      if (!render_append_codepoint(out, out_size, &used,
                                   cluster->fallback_codepoint))
         return 0;
      out[used] = L'\0';
      return used > 0;
   }

   if (cluster->flags & THE_RENDER_CLUSTER_BASE)
   {
      if (!render_cluster_base_to_wchars(cluster, out, out_size, &used))
         (void)render_append_codepoint(out, out_size, &used,
                                       cluster->fallback_codepoint);
      out[used] = L'\0';
      return used > 0;
   }

   if ((cluster->flags & THE_RENDER_CLUSTER_COMPONENTS)
   ||  (cluster->flags & THE_RENDER_CLUSTER_EXPANDED))
   {
      if (!render_cluster_components_to_wchars(cluster, out, out_size, &used))
         (void)render_append_codepoint(out, out_size, &used,
                                       cluster->fallback_codepoint);
      out[used] = L'\0';
      return used > 0;
   }

   for (i = 0; i < cluster->codepoint_count; i++)
   {
      if (!render_append_codepoint(out, out_size, &used,
                                   cluster->codepoints[i]))
         return 0;
   }

   if (used == 0)
   {
      if (!render_append_codepoint(out, out_size, &used,
                                   cluster->fallback_codepoint))
         return 0;
   }

   out[used] = L'\0';
   return used > 0;
}
