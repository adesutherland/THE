#include "utflayout.h"

#include <limits.h>

static int max_int(int left, int right)
{
   return (left > right) ? left : right;
}

static int positive_or_one(int value)
{
   return (value > 0) ? value : 1;
}

static TextPos utf8_layout_advance_codepoint_pos(TextPos pos,
                                                 TextCodepoint item)
{
   if (item.byte_length == 0)
      return pos;
   pos.byte_offset += item.byte_length;
   pos.codepoint_index++;
   pos.cell_column += item.cell_width;
   return pos;
}

static void utf8_layout_metrics_set(Utf8LayoutClusterMetrics *metrics,
                                    int width, int advance_width,
                                    int cursor_width, int repaint_width)
{
   if (metrics == NULL)
      return;
   metrics->width = positive_or_one(width);
   metrics->advance_width = positive_or_one(advance_width);
   metrics->cursor_width = positive_or_one(cursor_width);
   metrics->repaint_width = positive_or_one(repaint_width);
}

static void utf8_layout_metrics_set_all(Utf8LayoutClusterMetrics *metrics,
                                        int width)
{
   utf8_layout_metrics_set(metrics, width, width, width, width);
}

static void utf8_layout_metrics_from_entry(
   Utf8LayoutClusterMetrics *metrics, const Utf8TerminalProfileEntry *entry,
   int fallback_width)
{
   if (entry == NULL)
   {
      utf8_layout_metrics_set_all(metrics, fallback_width);
      return;
   }
   utf8_layout_metrics_set(metrics, entry->width, entry->advance_width,
                           entry->cursor_width, entry->repaint_width);
}

static void utf8_layout_metrics_add(Utf8LayoutClusterMetrics *total,
                                    const Utf8LayoutClusterMetrics *part)
{
   if (total == NULL || part == NULL)
      return;
   total->width += part->width;
   total->advance_width += part->advance_width;
   total->cursor_width += part->cursor_width;
   total->repaint_width += part->repaint_width;
}

static void utf8_layout_metrics_apply_entry_deltas(
   Utf8LayoutClusterMetrics *metrics, const Utf8TerminalProfileEntry *entry)
{
   int advance_extra;
   int cursor_extra;
   int repaint_extra;

   if (metrics == NULL || entry == NULL)
      return;
   advance_extra = entry->advance_width - entry->width;
   cursor_extra = entry->cursor_width - entry->advance_width;
   repaint_extra = entry->repaint_width - entry->advance_width;
   metrics->advance_width =
      positive_or_one(metrics->advance_width + advance_extra);
   metrics->cursor_width =
      positive_or_one(metrics->cursor_width + advance_extra + cursor_extra);
   metrics->repaint_width =
      positive_or_one(metrics->repaint_width + advance_extra + repaint_extra);
}

static Utf8TerminalClass utf8_layout_component_class(uint32_t codepoint)
{
   int width;

   if (utf8_cluster_codepoint_is_regional(codepoint))
      return UTF8_TERM_CLASS_REGIONAL_INDICATOR;
   if (codepoint < 0x80u)
      return UTF8_TERM_CLASS_ASCII;
   if (utf8_cluster_codepoint_is_private_use(codepoint))
      return UTF8_TERM_CLASS_PRIVATE_USE;

   width = text_codepoint_cell_width(codepoint);
   if (width <= 0)
      return UTF8_TERM_CLASS_COMBINING;
   if (width >= 2)
   {
      if (utf8_cluster_codepoint_is_emojiish(codepoint))
         return UTF8_TERM_CLASS_EMOJI;
      return UTF8_TERM_CLASS_WIDE;
   }
   return UTF8_TERM_CLASS_AMBIGUOUS;
}

static uint32_t utf8_layout_component_display_codepoint(uint32_t codepoint,
                                                        int show_markers)
{
   if (codepoint < 32 || codepoint == 0x7Fu)
      return 0;
   if (codepoint == 0x200Du || utf8_cluster_codepoint_is_tag(codepoint))
      return 0;
   switch (codepoint)
   {
      case 0xFE0Eu:
         return show_markers ? 'T' : 0;
      case 0xFE0Fu:
         return show_markers ? 'E' : 0;
      default:
         if (utf8_cluster_codepoint_is_keycap_mark(codepoint))
            return 'K';
         return codepoint;
   }
}

static void utf8_layout_component_metrics(uint32_t original_codepoint,
                                          uint32_t display_codepoint,
                                          Utf8LayoutClusterMetrics *metrics)
{
   Utf8TerminalClass feature_class;
   const Utf8TerminalProfileEntry *entry;
   int width;

   if (metrics == NULL)
      return;
   width = text_codepoint_cell_width(display_codepoint);
   if (width <= 0)
   {
      utf8_layout_metrics_set_all(metrics, 1);
      return;
   }
   if (display_codepoint == original_codepoint)
   {
      feature_class = utf8_layout_component_class(display_codepoint);
      entry = utf8_terminal_profile_lookup(feature_class,
                                           UTF8_TERM_DISPLAY_NORMAL);
      if (entry != NULL
      &&  entry->output_method == UTF8_TERM_OUTPUT_NATIVE)
      {
         utf8_layout_metrics_from_entry(metrics, entry, width);
         return;
      }
   }
   utf8_layout_metrics_set_all(metrics, width);
}

static void utf8_layout_component_class_metrics(
   Utf8TerminalClass feature_class, int fallback_width,
   Utf8LayoutClusterMetrics *metrics)
{
   const Utf8TerminalProfileEntry *entry;

   if (metrics == NULL)
      return;
   if (fallback_width <= 0)
      fallback_width = 1;
   entry = utf8_terminal_profile_lookup(feature_class,
                                        UTF8_TERM_DISPLAY_NORMAL);
   if (entry != NULL && entry->output_method == UTF8_TERM_OUTPUT_NATIVE)
   {
      utf8_layout_metrics_from_entry(metrics, entry, fallback_width);
      return;
   }
   utf8_layout_metrics_set_all(metrics, fallback_width);
}

static int utf8_layout_base_display_codepoint(
   const Utf8ClusterFacts *facts, uint32_t *display_codepoint)
{
   size_t i;

   if (display_codepoint != NULL)
      *display_codepoint = 0;
   if (facts == NULL || display_codepoint == NULL)
      return 0;

   switch (facts->feature_class)
   {
      case UTF8_TERM_CLASS_KEYCAP:
         return utf8_cluster_keycap_base(facts, display_codepoint);

      case UTF8_TERM_CLASS_REGIONAL_INDICATOR:
         if (facts->codepoint_count < 1
         ||  !utf8_cluster_codepoint_is_regional(facts->codepoints[0]))
            return 0;
         *display_codepoint = 'A' + facts->codepoints[0] - 0x1F1E6u;
         return 1;

      case UTF8_TERM_CLASS_TEXT_VARIATION:
      case UTF8_TERM_CLASS_EMOJI_VARIATION:
      case UTF8_TERM_CLASS_MODIFIER:
         for (i = 0; i < facts->codepoint_count; i++)
         {
            uint32_t codepoint = facts->codepoints[i];

            if (codepoint == 0xFE0Eu || codepoint == 0xFE0Fu
            ||  utf8_cluster_codepoint_is_modifier(codepoint))
               continue;
            *display_codepoint = codepoint;
            return 1;
         }
         return 0;

      default:
         return 0;
   }
}

static int utf8_layout_base_metrics(
   const CHARTYPE *line, size_t len, TextCluster cluster,
   const Utf8TerminalProfileEntry *entry,
   Utf8LayoutClusterMetrics *metrics)
{
   Utf8ClusterFacts facts;
   uint32_t display_codepoint;

   if (metrics == NULL
   ||  line == NULL
   ||  !utf8_cluster_collect_facts(line, len, cluster, &facts))
      return 0;

   if (facts.feature_class == UTF8_TERM_CLASS_REGIONAL_FLAG)
   {
      if (facts.codepoint_count < 2
      ||  !utf8_cluster_codepoint_is_regional(facts.codepoints[0])
      ||  !utf8_cluster_codepoint_is_regional(facts.codepoints[1]))
         return 0;
      utf8_layout_metrics_set_all(metrics, 2);
      utf8_layout_metrics_apply_entry_deltas(metrics, entry);
      return 1;
   }

   if (!utf8_layout_base_display_codepoint(&facts, &display_codepoint))
      return 0;
   utf8_layout_component_metrics(display_codepoint, display_codepoint,
                                 metrics);
   utf8_layout_metrics_apply_entry_deltas(metrics, entry);
   return 1;
}

static int utf8_layout_substitute_metrics(
   const Utf8TerminalProfileEntry *entry, Utf8LayoutClusterMetrics *metrics)
{
   int width;

   if (entry == NULL || metrics == NULL)
      return 0;
   width = text_codepoint_cell_width(entry->substitute_codepoint);
   if (width <= 0)
      width = 1;
   utf8_layout_metrics_set_all(metrics, width);
   utf8_layout_metrics_apply_entry_deltas(metrics, entry);
   return 1;
}

static int utf8_layout_components_metrics(
   const CHARTYPE *line, size_t len, TextCluster cluster,
   const Utf8TerminalProfileEntry *entry,
   Utf8LayoutClusterMetrics *metrics)
{
   Utf8ClusterFacts facts;
   TextPos pos;
   Utf8LayoutClusterMetrics total = { 0, 0, 0, 0 };
   int components = 0;
   int show_markers;

   if (metrics == NULL
   ||  line == NULL
   ||  !utf8_cluster_collect_facts(line, len, cluster, &facts))
      return 0;

   if (facts.feature_class == UTF8_TERM_CLASS_KEYCAP)
   {
      utf8_layout_metrics_set_all(metrics, 3);
      utf8_layout_metrics_apply_entry_deltas(metrics, entry);
      return 1;
   }

   show_markers = (facts.flags & UTF8_CLUSTER_FACT_CONTAINS_ZWJ) == 0;
   pos = cluster.pos;
   while (pos.byte_offset < cluster.end.byte_offset)
   {
      TextCodepoint item = textpos_codepoint_at_boundary(line, len, pos);
      uint32_t display_codepoint;
      Utf8LayoutClusterMetrics part;
      int zero_width;

      if (item.byte_length == 0)
         break;
      display_codepoint = utf8_layout_component_display_codepoint(
         item.codepoint, show_markers);
      if (display_codepoint != 0)
      {
         zero_width = text_codepoint_cell_width(display_codepoint) <= 0;
         if (!zero_width && components > 0)
         {
            utf8_layout_metrics_set_all(&part, 1);
            utf8_layout_metrics_add(&total, &part);
         }
         utf8_layout_component_metrics(item.codepoint, display_codepoint,
                                       &part);
         utf8_layout_metrics_add(&total, &part);
         components++;
      }
      pos = utf8_layout_advance_codepoint_pos(pos, item);
   }

   if (components == 0)
      return 0;
   *metrics = total;
   utf8_layout_metrics_apply_entry_deltas(metrics, entry);
   return 1;
}

static int utf8_layout_expanded_metric_ignores(uint32_t codepoint)
{
   return codepoint < 32
       || codepoint == 0x7Fu
       || codepoint == 0x200Du
       || codepoint == 0xFE0Eu
       || codepoint == 0xFE0Fu
       || utf8_cluster_codepoint_is_tag(codepoint)
       || utf8_cluster_codepoint_is_keycap_mark(codepoint);
}

static int utf8_layout_class_is_zwj(Utf8TerminalClass feature_class)
{
   return feature_class == UTF8_TERM_CLASS_SHORT_ZWJ
       || feature_class == UTF8_TERM_CLASS_HEART_ZWJ
       || feature_class == UTF8_TERM_CLASS_FAMILY_ZWJ;
}

static int utf8_layout_expanded_metrics(
   const CHARTYPE *line, size_t len, TextCluster cluster,
   const Utf8TerminalProfileEntry *entry,
   Utf8LayoutClusterMetrics *metrics)
{
   TextPos pos;
   Utf8LayoutClusterMetrics total = { 0, 0, 0, 0 };
   int components = 0;
   int suppress_zwj_variation;

   if (metrics == NULL || line == NULL)
      return 0;

   suppress_zwj_variation = entry != NULL
                          && utf8_layout_class_is_zwj(entry->feature_class);
   pos = cluster.pos;
   while (pos.byte_offset < cluster.end.byte_offset)
   {
      TextCodepoint item = textpos_codepoint_at_boundary(line, len, pos);
      TextPos next;
      Utf8TerminalClass feature_class;
      Utf8LayoutClusterMetrics part;
      int fallback_width;

      if (item.byte_length == 0)
         break;
      next = utf8_layout_advance_codepoint_pos(pos, item);
      if (utf8_layout_expanded_metric_ignores(item.codepoint))
      {
         pos = next;
         continue;
      }

      fallback_width = text_codepoint_cell_width(item.codepoint);
      if (fallback_width <= 0)
      {
         pos = next;
         continue;
      }

      feature_class = utf8_layout_component_class(item.codepoint);
      while (next.byte_offset < cluster.end.byte_offset)
      {
         TextCodepoint suffix = textpos_codepoint_at_boundary(line, len, next);

         if (suffix.byte_length == 0)
            break;
         if (suffix.codepoint == 0xFE0Eu)
         {
            if (!suppress_zwj_variation)
               feature_class = UTF8_TERM_CLASS_TEXT_VARIATION;
            next = utf8_layout_advance_codepoint_pos(next, suffix);
            continue;
         }
         if (suffix.codepoint == 0xFE0Fu)
         {
            if (!suppress_zwj_variation)
               feature_class = UTF8_TERM_CLASS_EMOJI_VARIATION;
            next = utf8_layout_advance_codepoint_pos(next, suffix);
            continue;
         }
         if (utf8_cluster_codepoint_is_modifier(suffix.codepoint))
         {
            feature_class = UTF8_TERM_CLASS_MODIFIER;
            next = utf8_layout_advance_codepoint_pos(next, suffix);
            continue;
         }
         break;
      }

      utf8_layout_component_class_metrics(feature_class, fallback_width, &part);
      utf8_layout_metrics_add(&total, &part);
      components++;
      pos = next;
   }

   if (components == 0)
      return 0;
   *metrics = total;
   utf8_layout_metrics_apply_entry_deltas(metrics, entry);
   return 1;
}

static int utf8_layout_output_metrics(
   const CHARTYPE *line, size_t len, TextCluster cluster,
   const Utf8TerminalProfileEntry *entry,
   Utf8LayoutClusterMetrics *metrics)
{
   Utf8TerminalOutput output;

   if (entry == NULL || metrics == NULL)
      return 0;
   output = utf8_terminal_resolved_output_for_entry(entry);
   switch (output)
   {
      case UTF8_TERM_OUTPUT_NATIVE:
         utf8_layout_metrics_from_entry(metrics, entry,
                                        utf8_layout_cluster_logical_width(cluster));
         return 1;

      case UTF8_TERM_OUTPUT_BASE:
         return utf8_layout_base_metrics(line, len, cluster, entry, metrics);

      case UTF8_TERM_OUTPUT_SUBSTITUTE:
         return utf8_layout_substitute_metrics(entry, metrics);

      case UTF8_TERM_OUTPUT_COMPONENTS:
         return utf8_layout_components_metrics(line, len, cluster, entry,
                                               metrics);

      case UTF8_TERM_OUTPUT_EXPANDED:
         return utf8_layout_expanded_metrics(line, len, cluster, entry,
                                             metrics);

      case UTF8_TERM_OUTPUT_SANITIZE:
      case UTF8_TERM_OUTPUT_UNKNOWN:
      case UTF8_TERM_OUTPUT_COUNT:
      default:
         return 0;
   }
}

const Utf8TerminalProfileEntry *utf8_layout_cluster_profile(
   const CHARTYPE *line, size_t len, TextCluster cluster)
{
   return utf8_terminal_profile_lookup_cluster(line, len, cluster,
                                               utf8_terminal_display_mode());
}

int utf8_layout_cluster_logical_width(TextCluster cluster)
{
   return (cluster.cell_width > 0) ? cluster.cell_width : 1;
}

int utf8_layout_cluster_metrics(const CHARTYPE *line, size_t len,
                                TextCluster cluster,
                                Utf8LayoutClusterMetrics *metrics)
{
   const Utf8TerminalProfileEntry *entry;
   int logical_width = utf8_layout_cluster_logical_width(cluster);
   Utf8TerminalMetrics metric_method;

   if (metrics == NULL)
      return 0;
   entry = utf8_layout_cluster_profile(line, len, cluster);
   utf8_layout_metrics_from_entry(metrics, entry, logical_width);
   if (entry == NULL)
      return 0;
   metric_method = entry->metric_method;
   if (metric_method == UTF8_TERM_METRICS_AUTO)
      metric_method = utf8_terminal_effective_metrics_for_entry(entry);
   if (metric_method == UTF8_TERM_METRICS_COMPONENTS)
   {
      if (utf8_layout_components_metrics(line, len, cluster, entry, metrics))
         return 1;
   }
   else if (metric_method == UTF8_TERM_METRICS_EXPANDED)
   {
      if (utf8_layout_expanded_metrics(line, len, cluster, entry, metrics))
         return 1;
   }
   else if (metric_method == UTF8_TERM_METRICS_OUTPUT)
   {
      if (utf8_layout_output_metrics(line, len, cluster, entry, metrics))
         return 1;
   }
   if (entry->display_mode == UTF8_TERM_DISPLAY_SINGLE)
      metrics->width = 1;
   return 1;
}

int utf8_layout_cluster_width(const CHARTYPE *line, size_t len,
                              TextCluster cluster)
{
   Utf8LayoutClusterMetrics metrics;

   if (utf8_layout_cluster_metrics(line, len, cluster, &metrics))
      return metrics.width;
   return utf8_layout_cluster_logical_width(cluster);
}

int utf8_layout_cluster_advance_width(const CHARTYPE *line, size_t len,
                                      TextCluster cluster)
{
   Utf8LayoutClusterMetrics metrics;

   if (utf8_layout_cluster_metrics(line, len, cluster, &metrics))
      return metrics.advance_width;
   return utf8_layout_cluster_width(line, len, cluster);
}

int utf8_layout_cluster_cursor_width(const CHARTYPE *line, size_t len,
                                     TextCluster cluster)
{
   Utf8LayoutClusterMetrics metrics;

   if (utf8_layout_cluster_metrics(line, len, cluster, &metrics))
      return metrics.cursor_width;
   return utf8_layout_cluster_advance_width(line, len, cluster);
}

int utf8_layout_cluster_repaint_width(const CHARTYPE *line, size_t len,
                                    TextCluster cluster)
{
   Utf8LayoutClusterMetrics metrics;

   if (utf8_layout_cluster_metrics(line, len, cluster, &metrics))
      return metrics.repaint_width;
   return utf8_layout_cluster_advance_width(line, len, cluster);
}

int utf8_layout_display_col_from_logical(const CHARTYPE *line, size_t len,
                                         int viewport_col, int logical_col)
{
   TextPos pos;
   int display_col = 0;
   int last_logical_col;

   if (logical_col <= viewport_col)
      return 0;

   pos = textpos_from_cell(line, len, viewport_col, TEXT_SNAP_BACKWARD);
   last_logical_col = viewport_col;
   while (pos.byte_offset < len)
   {
      TextCluster cluster = textpos_cluster_at_boundary(line, len, pos);
      int logical_start;
      int logical_end;

      if (cluster.byte_length == 0)
         break;

      logical_start = cluster.pos.cell_column;
      logical_end = cluster.end.cell_column;
      if (logical_end <= viewport_col)
      {
         pos = cluster.end;
         last_logical_col = logical_end;
         continue;
      }
      if (logical_start >= logical_col)
         break;

      if (logical_start < viewport_col)
      {
         int clipped_end = (logical_col < logical_end) ? logical_col : logical_end;
         display_col += clipped_end - viewport_col;
         last_logical_col = clipped_end;
      }
      else if (logical_end <= logical_col)
      {
         display_col += utf8_layout_cluster_advance_width(line, len, cluster);
         last_logical_col = logical_end;
      }
      else
      {
         display_col += logical_col - logical_start;
         last_logical_col = logical_col;
      }

      if (logical_end >= logical_col)
         break;
      pos = cluster.end;
   }

   if (logical_col > last_logical_col)
      display_col += logical_col - last_logical_col;
   return display_col;
}

int utf8_layout_logical_col_from_display(const CHARTYPE *line, size_t len,
                                         int viewport_col, int display_col,
                                         TextSnap snap)
{
   TextPos pos;
   int screen_col = 0;
   int last_logical_col;

   if (viewport_col < 0)
      viewport_col = 0;
   if (display_col <= 0)
      return viewport_col;

   pos = textpos_from_cell(line, len, viewport_col, TEXT_SNAP_BACKWARD);
   last_logical_col = viewport_col;
   while (pos.byte_offset < len)
   {
      TextCluster cluster = textpos_cluster_at_boundary(line, len, pos);
      int logical_start;
      int logical_end;
      int logical_width;
      int advance_width;

      if (cluster.byte_length == 0)
         break;

      logical_start = cluster.pos.cell_column;
      logical_end = cluster.end.cell_column;
      logical_width = (cluster.cell_width > 0) ? cluster.cell_width : 1;
      advance_width = utf8_layout_cluster_advance_width(line, len, cluster);
      if (advance_width <= 0)
         advance_width = logical_width;

      if (logical_end <= viewport_col)
      {
         pos = cluster.end;
         last_logical_col = logical_end;
         continue;
      }

      if (logical_start < viewport_col)
      {
         int clipped_width = logical_end - viewport_col;

         if (clipped_width < 0)
            clipped_width = 0;
         if (display_col < screen_col + clipped_width)
            return viewport_col;
         screen_col += clipped_width;
         pos = cluster.end;
         last_logical_col = logical_end;
         continue;
      }

      if (display_col < screen_col + advance_width)
      {
         int offset = display_col - screen_col;

         if (snap == TEXT_SNAP_FORWARD)
            return logical_end;
         if (snap == TEXT_SNAP_NEAREST && offset * 2 >= advance_width)
            return logical_end;
         return logical_start;
      }

      screen_col += advance_width;
      last_logical_col = logical_end;
      pos = cluster.end;
   }

   if (display_col > screen_col)
      last_logical_col += display_col - screen_col;
   return last_logical_col;
}

int utf8_layout_width_col_from_logical(const CHARTYPE *line, size_t len,
                                       int logical_col)
{
   TextPos pos;
   int width_col = 0;
   int last_logical_col = 0;

   if (logical_col <= 0)
      return 0;

   pos = textpos_begin();
   while (pos.byte_offset < len)
   {
      TextCluster cluster = textpos_cluster_at_boundary(line, len, pos);
      int logical_start;
      int logical_end;

      if (cluster.byte_length == 0)
         break;

      logical_start = cluster.pos.cell_column;
      logical_end = cluster.end.cell_column;
      if (logical_start >= logical_col)
         break;
      if (logical_col < logical_end)
         break;

      width_col += utf8_layout_cluster_width(line, len, cluster);
      last_logical_col = logical_end;
      pos = cluster.end;
   }

   if (logical_col > last_logical_col)
      width_col += logical_col - last_logical_col;
   return width_col;
}

int utf8_layout_logical_col_from_width(const CHARTYPE *line, size_t len,
                                       int width_col, TextSnap snap)
{
   TextPos pos;
   int current_width_col = 0;
   int last_logical_col = 0;

   if (width_col <= 0)
      return 0;

   pos = textpos_begin();
   while (pos.byte_offset < len)
   {
      TextCluster cluster = textpos_cluster_at_boundary(line, len, pos);
      int logical_start;
      int logical_end;
      int cluster_width;

      if (cluster.byte_length == 0)
         break;

      logical_start = cluster.pos.cell_column;
      logical_end = cluster.end.cell_column;
      cluster_width = utf8_layout_cluster_width(line, len, cluster);
      if (cluster_width <= 0)
         cluster_width = utf8_layout_cluster_logical_width(cluster);

      if (width_col < current_width_col + cluster_width)
      {
         int offset = width_col - current_width_col;

         if (snap == TEXT_SNAP_FORWARD)
            return logical_end;
         if (snap == TEXT_SNAP_NEAREST && offset * 2 >= cluster_width)
            return logical_end;
         return logical_start;
      }

      current_width_col += cluster_width;
      last_logical_col = logical_end;
      pos = cluster.end;
   }

   if (width_col > current_width_col)
      last_logical_col += width_col - current_width_col;
   return last_logical_col;
}

TextCellSlice utf8_layout_slice_width(const CHARTYPE *line, size_t len,
                                      int start_width_col, int width_cols)
{
   TextCellSlice slice;
   TextPos pos;
   int end_width_col;
   int current_width_col = 0;
   int found = 0;

   slice.start = textpos_begin();
   slice.end = textpos_begin();
   slice.leading_cells = 0;
   slice.content_cells = 0;
   slice.trailing_cells = width_cols < 0 ? 0 : width_cols;

   if (width_cols <= 0)
      return slice;
   if (start_width_col < 0)
      start_width_col = 0;
   if (width_cols > INT_MAX - start_width_col)
      end_width_col = INT_MAX;
   else
      end_width_col = start_width_col + width_cols;

   pos = textpos_begin();
   while (pos.byte_offset < len)
   {
      TextCluster cluster = textpos_cluster_at_boundary(line, len, pos);
      int cluster_width;
      int next_width_col;

      if (cluster.byte_length == 0)
         break;

      cluster_width = utf8_layout_cluster_width(line, len, cluster);
      if (cluster_width <= 0)
         cluster_width = utf8_layout_cluster_logical_width(cluster);
      if (cluster_width > INT_MAX - current_width_col)
         next_width_col = INT_MAX;
      else
         next_width_col = current_width_col + cluster_width;

      if (next_width_col <= start_width_col)
      {
         current_width_col = next_width_col;
         pos = cluster.end;
         continue;
      }
      if (current_width_col >= end_width_col)
         break;

      if (!found)
      {
         slice.start = cluster.pos;
         if (start_width_col > current_width_col)
            slice.leading_cells = start_width_col - current_width_col;
         found = 1;
      }
      slice.end = cluster.end;
      slice.content_cells += cluster_width;
      current_width_col = next_width_col;
      pos = cluster.end;
   }

   if (!found)
   {
      int logical_col = utf8_layout_logical_col_from_width(
                           line, len, start_width_col, TEXT_SNAP_BACKWARD);

      slice.start = textpos_from_cell_virtual(line, len, logical_col,
                                              TEXT_SNAP_BACKWARD);
      slice.end = slice.start;
      slice.trailing_cells = width_cols;
      return slice;
   }

   slice.trailing_cells =
      current_width_col < end_width_col ? end_width_col - current_width_col : 0;
   return slice;
}

Utf8LayoutViewport utf8_layout_viewport_for_logical_col(
   const CHARTYPE *line, size_t len, int current_viewport_col,
   int logical_col, int visible_cols)
{
   Utf8LayoutViewport target;
   int preferred_display_col;
   int low;
   int high;
   int best;

   current_viewport_col = max_int(current_viewport_col, 0);
   logical_col = max_int(logical_col, 0);
   target.viewport_col = current_viewport_col;
   target.display_col = utf8_layout_display_col_from_logical(
      line, len, current_viewport_col, logical_col);
   target.visible = logical_col >= current_viewport_col
                 && visible_cols > 0
                 && target.display_col < visible_cols;
   if (target.visible || visible_cols <= 0)
      return target;

   preferred_display_col = visible_cols / 2 - 1;
   if (preferred_display_col < 0)
      preferred_display_col = 0;

   low = 0;
   high = logical_col;
   best = logical_col;
   while (low <= high)
   {
      int mid = low + (high - low) / 2;
      int display_col = utf8_layout_display_col_from_logical(line, len, mid,
                                                             logical_col);

      if (display_col <= preferred_display_col)
      {
         best = mid;
         high = mid - 1;
      }
      else
      {
         low = mid + 1;
      }
   }

   target.viewport_col = best;
   target.display_col = utf8_layout_display_col_from_logical(line, len, best,
                                                             logical_col);
   target.visible = target.display_col < visible_cols;
   return target;
}
