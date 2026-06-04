#include "llmdriver.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#ifdef USE_UTF8
# include "utflayout.h"
# include "utfterm.h"
#endif

static void copy_text(char *dest, size_t dest_len, const char *src)
{
   size_t len;

   if (dest == NULL || dest_len == 0)
      return;
   if (src == NULL)
      src = "";
   len = strlen(src);
   if (len >= dest_len)
      len = dest_len - 1;
   if (len > 0)
      memcpy(dest, src, len);
   dest[len] = '\0';
}

static void copy_text_n(char *dest, size_t dest_len, const char *src, size_t src_len)
{
   size_t len;

   if (dest == NULL || dest_len == 0)
      return;
   if (src == NULL)
   {
      dest[0] = '\0';
      return;
   }
   len = src_len;
   if (len >= dest_len)
      len = dest_len - 1;
   if (len > 0)
      memcpy(dest, src, len);
   dest[len] = '\0';
}

static int appendf(char *out, size_t out_len, size_t *used, const char *fmt, ...)
{
   va_list args;
   int written;

   if (out == NULL || used == NULL || *used >= out_len)
      return 0;
   va_start(args, fmt);
   written = vsnprintf(out + *used, out_len - *used, fmt, args);
   va_end(args);
   if (written < 0)
      return 0;
   if ((size_t)written >= out_len - *used)
   {
      *used = out_len - 1;
      return 0;
   }
   *used += (size_t)written;
   return 1;
}

static int append_json_string(char *out, size_t out_len, size_t *used,
                              const char *text)
{
   const unsigned char *ptr;

   if (!appendf(out, out_len, used, "\""))
      return 0;
   if (text == NULL)
      text = "";
   for (ptr = (const unsigned char *)text; *ptr != '\0'; ptr++)
   {
      switch (*ptr)
      {
         case '\\':
            if (!appendf(out, out_len, used, "\\\\"))
               return 0;
            break;
         case '"':
            if (!appendf(out, out_len, used, "\\\""))
               return 0;
            break;
         case '\n':
            if (!appendf(out, out_len, used, "\\n"))
               return 0;
            break;
         case '\r':
            if (!appendf(out, out_len, used, "\\r"))
               return 0;
            break;
         case '\t':
            if (!appendf(out, out_len, used, "\\t"))
               return 0;
            break;
         default:
            if (*ptr < 0x20)
            {
               if (!appendf(out, out_len, used, "\\u%04x", *ptr))
                  return 0;
            }
            else
            {
               if (!appendf(out, out_len, used, "%c", *ptr))
                  return 0;
            }
            break;
      }
   }
   return appendf(out, out_len, used, "\"");
}

static int append_json_string_n(char *out, size_t out_len, size_t *used,
                                const char *text, size_t text_len)
{
   size_t i;

   if (!appendf(out, out_len, used, "\""))
      return 0;
   if (text == NULL)
      text_len = 0;
   for (i = 0; i < text_len; i++)
   {
      unsigned char ch = (unsigned char)text[i];

      switch (ch)
      {
         case '\\':
            if (!appendf(out, out_len, used, "\\\\"))
               return 0;
            break;
         case '"':
            if (!appendf(out, out_len, used, "\\\""))
               return 0;
            break;
         case '\n':
            if (!appendf(out, out_len, used, "\\n"))
               return 0;
            break;
         case '\r':
            if (!appendf(out, out_len, used, "\\r"))
               return 0;
            break;
         case '\t':
            if (!appendf(out, out_len, used, "\\t"))
               return 0;
            break;
         default:
            if (ch < 0x20)
            {
               if (!appendf(out, out_len, used, "\\u%04x", ch))
                  return 0;
            }
            else
            {
               if (!appendf(out, out_len, used, "%c", ch))
                  return 0;
            }
            break;
      }
   }
   return appendf(out, out_len, used, "\"");
}

static int append_json_string_limited(char *out, size_t out_len, size_t *used,
                                      const char *text, int max_cols)
{
   char limited[LLM_DRIVER_MAX_COLS + 4];
   size_t len;
   size_t keep;

   if (text == NULL || max_cols <= 0)
      return append_json_string(out, out_len, used, text);
   len = strlen(text);
   if ((int)len <= max_cols)
      return append_json_string(out, out_len, used, text);
   keep = (size_t)max_cols;
   if (keep > sizeof(limited) - 4)
      keep = sizeof(limited) - 4;
   memcpy(limited, text, keep);
   limited[keep++] = '.';
   limited[keep++] = '.';
   limited[keep++] = '.';
   limited[keep] = '\0';
   return append_json_string(out, out_len, used, limited);
}

static int visible_style_run_count(const UiStyleRun *styles, size_t style_count,
                                   int max_text_cols)
{
   size_t i;
   int count = 0;

   for (i = 0; i < style_count; i++)
   {
      const UiStyleRun *run = &styles[i];

      if (run->style == UI_SYNTAX_NONE
      ||  run->start_cell < 0
      ||  run->cell_count <= 0)
      {
         continue;
      }
      if (max_text_cols > 0 && run->start_cell >= max_text_cols)
         continue;
      count++;
   }
   return count;
}

static int style_run_visible_len(const UiStyleRun *run, int max_text_cols)
{
   int len;

   if (run == NULL || run->cell_count <= 0)
      return 0;
   len = run->cell_count;
   if (max_text_cols > 0 && run->start_cell + len > max_text_cols)
      len = max_text_cols - run->start_cell;
   return len > 0 ? len : 0;
}

static int append_style_runs(char *out, size_t out_len, size_t *used,
                             const UiStyleRun *styles, size_t style_count,
                             int max_text_cols, int compact)
{
   size_t i;
   int emitted = 0;

   if (visible_style_run_count(styles, style_count, max_text_cols) == 0)
      return 1;
   if (!appendf(out, out_len, used, compact ? ",\"s\":[" : ", \"styles\": ["))
      return 0;
   for (i = 0; i < style_count; i++)
   {
      const UiStyleRun *run = &styles[i];
      int len = style_run_visible_len(run, max_text_cols);

      if (run->style == UI_SYNTAX_NONE || run->start_cell < 0 || len <= 0)
         continue;
      if (compact)
      {
         if (!appendf(out, out_len, used, "%s[%d,%d,",
                      emitted > 0 ? "," : "", run->start_cell, len))
            return 0;
         if (!append_json_string(out, out_len, used,
                                 ui_syntax_style_name(run->style)))
            return 0;
         if (!appendf(out, out_len, used, "]"))
            return 0;
      }
      else
      {
         if (!appendf(out, out_len, used,
                      "%s{\"start\": %d, \"len\": %d, \"style\": ",
                      emitted > 0 ? ", " : "", run->start_cell, len))
            return 0;
         if (!append_json_string(out, out_len, used,
                                 ui_syntax_style_name(run->style)))
            return 0;
         if (!appendf(out, out_len, used, "}"))
            return 0;
      }
      emitted++;
   }
   return appendf(out, out_len, used, "]");
}

static int utf_metadata_visible(const LlmDriverScreenLine *line,
                                const LlmDriverUtfClusterInfo *info,
                                int max_text_cols, int include_all_utf)
{
   int row_cell;

   if (line == NULL || info == NULL)
      return 0;
   if (!include_all_utf && !info->default_visible)
      return 0;
   row_cell = info->cell - line->logical_start_col;
   if (max_text_cols > 0 && row_cell >= max_text_cols)
      return 0;
   return 1;
}

static int visible_utf_metadata_count(const LlmDriverScreenLine *line,
                                      int max_text_cols, int include_all_utf)
{
   size_t i;
   int count = 0;

   if (line == NULL)
      return 0;
   for (i = 0; i < line->utf_cluster_count; i++)
   {
      if (utf_metadata_visible(line, &line->utf_clusters[i], max_text_cols,
                               include_all_utf))
         count++;
   }
   return count;
}

#ifdef USE_UTF8
static int append_utf_cluster_codepoints(char *out, size_t out_len,
                                         size_t *used,
                                         const CHARTYPE *text, size_t len,
                                         const LlmDriverUtfClusterInfo *info);
#endif

static int append_utf_metadata(char *out, size_t out_len, size_t *used,
                               const LlmDriverScreenLine *line,
                               int max_text_cols, int compact,
                               int include_all_utf)
{
   size_t i;
   int emitted = 0;
#ifdef USE_UTF8
   const CHARTYPE *line_text = line != NULL ? (const CHARTYPE *)line->text : NULL;
   size_t line_len = line != NULL ? strlen(line->text) : 0;
#endif

   if (visible_utf_metadata_count(line, max_text_cols, include_all_utf) == 0)
      return 1;
   if (!appendf(out, out_len, used, compact ? ",\"u\":[" : ", \"utf\": ["))
      return 0;
   for (i = 0; i < line->utf_cluster_count; i++)
   {
      const LlmDriverUtfClusterInfo *info = &line->utf_clusters[i];

      if (!utf_metadata_visible(line, info, max_text_cols, include_all_utf))
         continue;
      if (compact)
      {
         if (!appendf(out, out_len, used, "%s[%d,%d,%d,%d,%d,%d,",
                      emitted > 0 ? "," : "", info->cell,
                      info->logical_width, info->width, info->advance_width,
                      info->cursor_width, info->repaint_width))
            return 0;
         if (!append_json_string(out, out_len, used, info->feature_class))
            return 0;
         if (!appendf(out, out_len, used, ","))
            return 0;
         if (!append_json_string(out, out_len, used, info->output))
            return 0;
         if (!appendf(out, out_len, used, ","))
            return 0;
         if (!append_json_string(out, out_len, used, info->mark))
            return 0;
         if (!appendf(out, out_len, used, ",%d,%d]",
                      info->compressed, info->substituted))
            return 0;
      }
      else
      {
         const char *cluster_text = NULL;
         size_t cluster_text_len = 0;

#ifdef USE_UTF8
         if (line != NULL && info->byte_offset <= line_len)
         {
            cluster_text = line->text + info->byte_offset;
            cluster_text_len = info->byte_length;
            if (info->byte_offset + cluster_text_len > line_len)
               cluster_text_len = line_len - info->byte_offset;
         }
#endif
         if (!appendf(out, out_len, used,
                      "%s{\"row\": %d, \"cell\": %d, \"screen_cell\": %d, \"byte_offset\": %zu, \"cluster_index\": %zu, \"text\": ",
                      emitted > 0 ? ", " : "", info->row, info->cell,
                      info->screen_cell, info->byte_offset,
                      info->cluster_index))
            return 0;
         if (!append_json_string_n(out, out_len, used, cluster_text,
                                   cluster_text_len))
            return 0;
         if (!appendf(out, out_len, used,
                      ", \"codepoints\": "))
            return 0;
#ifdef USE_UTF8
         if (!append_utf_cluster_codepoints(out, out_len, used, line_text,
                                            line_len, info))
            return 0;
#else
         if (!append_json_string(out, out_len, used, ""))
            return 0;
#endif
         if (!appendf(out, out_len, used,
                      ", \"logical_width\": %d, \"width\": %d, \"advance_width\": %d, \"cursor_width\": %d, \"repaint_width\": %d, \"class\": ",
                      info->logical_width, info->width,
                      info->advance_width, info->cursor_width,
                      info->repaint_width))
            return 0;
         if (!append_json_string(out, out_len, used, info->feature_class))
            return 0;
         if (!appendf(out, out_len, used, ", \"output\": "))
            return 0;
         if (!append_json_string(out, out_len, used, info->output))
            return 0;
         if (!appendf(out, out_len, used, ", \"mark\": "))
            return 0;
         if (!append_json_string(out, out_len, used, info->mark))
            return 0;
         if (!appendf(out, out_len, used,
                      ", \"compressed\": %d, \"substituted\": %d}",
                      info->compressed, info->substituted))
            return 0;
      }
      emitted++;
   }
   return appendf(out, out_len, used, "]");
}

static int append_diagnostics(char *out, size_t out_len, size_t *used,
                              const LlmDriverScreenView *view, int compact)
{
   size_t i;

   if (view == NULL || view->diagnostic_count == 0)
      return 1;
   if (!appendf(out, out_len, used,
                compact ? ",\"diagnostics\":[" : "  \"diagnostics\": ["))
      return 0;
   for (i = 0; i < view->diagnostic_count; i++)
   {
      const LlmDriverParserDiagnostic *diagnostic = &view->diagnostics[i];

      if (!appendf(out, out_len, used,
                   "%s{\"line\":%ld,\"column\":%ld,\"severity\":",
                   i == 0 ? "" : ",", (long)diagnostic->line,
                   (long)diagnostic->column))
         return 0;
      if (!append_json_string(out, out_len, used, diagnostic->severity))
         return 0;
      if (!appendf(out, out_len, used, ",\"code\":"))
         return 0;
      if (!append_json_string(out, out_len, used, diagnostic->code))
         return 0;
      if (!appendf(out, out_len, used, ",\"message\":"))
         return 0;
      if (!append_json_string(out, out_len, used, diagnostic->message))
         return 0;
      if (!appendf(out, out_len, used, "}"))
         return 0;
   }
   return appendf(out, out_len, used, compact ? "]" : "],\n");
}

static const char *llm_driver_view_mode_name(LlmDriverViewMode mode)
{
   switch (mode)
   {
      case LLM_DRIVER_VIEW_FILEAREA:
         return "filearea";
      case LLM_DRIVER_VIEW_RESERVED:
         return "reserved";
      case LLM_DRIVER_VIEW_PREFIX:
         return "prefix";
      case LLM_DRIVER_VIEW_FOCUS:
         return "focus";
      case LLM_DRIVER_VIEW_FULL:
      default:
         return "full";
   }
}

static int llm_driver_row_is_reserved(UiRowRole role)
{
   switch (role)
   {
      case UI_ROW_TOF:
      case UI_ROW_EOF:
      case UI_ROW_RESERVED:
      case UI_ROW_BOUNDS:
      case UI_ROW_SCALE:
      case UI_ROW_TABLINE:
      case UI_ROW_STATUS:
      case UI_ROW_PROMPT:
      case UI_ROW_DIVIDER:
      case UI_ROW_WINDOW:
         return 1;
      case UI_ROW_EMPTY:
      case UI_ROW_FILE:
      case UI_ROW_PREFIX:
      case UI_ROW_COMMAND:
      default:
         return 0;
   }
}

static int llm_driver_row_matches_options(const LlmDriverScreenLine *line,
                                          const LlmDriverFormatOptions *options)
{
   if (line == NULL || options == NULL)
      return 0;
   if (options->first_row >= 0 && line->logical_row < options->first_row)
      return 0;
   if (options->row_count >= 0
   &&  options->first_row >= 0
   &&  line->logical_row >= options->first_row + options->row_count)
      return 0;

   switch (options->mode)
   {
      case LLM_DRIVER_VIEW_FILEAREA:
         return line->role == UI_ROW_FILE;
      case LLM_DRIVER_VIEW_RESERVED:
         return llm_driver_row_is_reserved(line->role);
      case LLM_DRIVER_VIEW_PREFIX:
         return line->prefix[0] != '\0' || line->role == UI_ROW_PREFIX;
      case LLM_DRIVER_VIEW_FOCUS:
         return line->current || line->cursor;
      case LLM_DRIVER_VIEW_FULL:
      default:
         return 1;
   }
}

#ifdef USE_UTF8
static int append_utf_cluster_codepoints(char *out, size_t out_len,
                                         size_t *used,
                                         const CHARTYPE *text, size_t len,
                                         const LlmDriverUtfClusterInfo *info)
{
   TextPos pos;
   size_t end_byte;
   int emitted = 0;

   if (text == NULL || info == NULL)
      return append_json_string(out, out_len, used, "");
   if (!appendf(out, out_len, used, "\""))
      return 0;

   pos = textpos_from_byte(text, len, info->byte_offset);
   end_byte = info->byte_offset + info->byte_length;
   if (end_byte > len)
      end_byte = len;
   while (pos.byte_offset < end_byte)
   {
      TextCodepoint item = textpos_codepoint_at_boundary(text, len, pos);

      if (item.byte_length == 0)
         break;
      if (!appendf(out, out_len, used, "%sU+%X",
                   emitted ? " " : "", item.codepoint))
         return 0;
      emitted = 1;
      pos.byte_offset += item.byte_length;
      pos.codepoint_index++;
      pos.cell_column += item.cell_width;
   }
   return appendf(out, out_len, used, "\"");
}
#endif

static void llm_driver_collect_utf_metadata(LlmDriverScreenLine *line)
{
#ifdef USE_UTF8
   const CHARTYPE *text;
   size_t len;
   TextPos pos;

   if (line == NULL)
      return;
   line->utf_cluster_count = 0;
   text = (const CHARTYPE *)line->text;
   len = strlen(line->text);
   pos = textpos_begin();
   while (pos.byte_offset < len
   &&     line->utf_cluster_count < LLM_DRIVER_MAX_UTF_CLUSTERS)
   {
      TextCluster cluster = textpos_cluster_at_boundary(text, len, pos);
      Utf8TerminalClass feature_class;
      const Utf8TerminalProfileEntry *entry;
      LlmDriverUtfClusterInfo *info;
      Utf8TerminalOutput output = UTF8_TERM_OUTPUT_NATIVE;
      Utf8TerminalMark mark = UTF8_TERM_MARK_NONE;

      if (!cluster.valid || cluster.byte_length == 0)
         break;
      feature_class = utf8_terminal_classify_cluster(text, len, cluster);
      entry = utf8_terminal_profile_lookup_cluster(
                 text, len, cluster, utf8_terminal_display_mode());
      if (entry != NULL)
      {
         feature_class = entry->feature_class;
         output = entry->output_method;
         mark = entry->mark;
      }
      info = &line->utf_clusters[line->utf_cluster_count++];
      info->row = line->logical_row;
      info->cell = line->logical_start_col + cluster.pos.cell_column;
      info->screen_cell = cluster.pos.cell_column;
      info->byte_offset = cluster.pos.byte_offset;
      info->byte_length = cluster.byte_length;
      info->cluster_index = cluster.pos.cluster_index;
      info->logical_width = cluster.cell_width > 0 ? cluster.cell_width : 1;
      info->width = utf8_layout_cluster_width(text, len, cluster);
      info->advance_width = utf8_layout_cluster_advance_width(text, len, cluster);
      info->cursor_width = utf8_layout_cluster_cursor_width(text, len, cluster);
      info->repaint_width = utf8_layout_cluster_repaint_width(text, len, cluster);
      copy_text(info->feature_class, sizeof(info->feature_class),
                utf8_terminal_class_name(feature_class));
      copy_text(info->output, sizeof(info->output),
                utf8_terminal_output_name(output));
      copy_text(info->mark, sizeof(info->mark),
                utf8_terminal_mark_name(mark));
      info->compressed = mark == UTF8_TERM_MARK_COMPRESSED;
      info->substituted = output == UTF8_TERM_OUTPUT_SUBSTITUTE
                       || mark == UTF8_TERM_MARK_SUBSTITUTED;
      info->default_visible =
         (feature_class != UTF8_TERM_CLASS_ASCII
      &&  feature_class != UTF8_TERM_CLASS_UNKNOWN)
      || (entry != NULL
      &&  (entry->output_method != UTF8_TERM_OUTPUT_NATIVE
       ||  entry->mark != UTF8_TERM_MARK_NONE));
      pos = cluster.end;
   }
#else
   if (line != NULL)
      line->utf_cluster_count = 0;
#endif
}

void llm_driver_screen_view_init(LlmDriverScreenView *view, int rows, int cols,
                                 LogicalCursor cursor)
{
   if (view == NULL)
      return;
   memset(view, 0, sizeof(*view));
   view->rows = rows;
   view->cols = cols;
   view->cursor = cursor;
   view->cursor_screen_row = cursor.zone_row;
   view->cursor_screen_col = cursor.text.cell_column;
}

int llm_driver_screen_view_set_line(LlmDriverScreenView *view, size_t index,
                                    LINETYPE line_number, int logical_row,
                                    const char *prefix, const char *text,
                                    int current)
{
   return llm_driver_screen_view_set_row(view, index, UI_ROW_FILE,
                                         line_number, logical_row, 0,
                                         prefix, text, 1, current);
}

int llm_driver_screen_view_set_row(LlmDriverScreenView *view, size_t index,
                                   UiRowRole role, LINETYPE line_number,
                                   int logical_row, int logical_start_col,
                                   const char *prefix, const char *text,
                                   int editable, int current)
{
   LlmDriverScreenLine *line;

   if (view == NULL || index >= LLM_DRIVER_MAX_ROWS)
      return 0;
   line = &view->lines[index];
   line->line_number = line_number;
   line->logical_row = logical_row;
   line->role = role;
   line->logical_start_col = logical_start_col;
   line->editable = editable;
   line->current = current;
   line->cursor = view->cursor.valid
               && view->cursor.zone_row == logical_row
               && ui_row_role_from_cursor_zone(view->cursor.zone) == role;
   line->style_count = 0;
   copy_text(line->prefix, sizeof(line->prefix), prefix);
   copy_text(line->text, sizeof(line->text), text);
   llm_driver_collect_utf_metadata(line);
   if (index >= view->line_count)
      view->line_count = index + 1;
   return 1;
}

int llm_driver_screen_view_set_prefix_command(LlmDriverScreenView *view,
                                              size_t index,
                                              const char *command)
{
   if (view == NULL || index >= view->line_count
   ||  index >= LLM_DRIVER_MAX_ROWS)
      return 0;
   copy_text(view->lines[index].prefix_command,
             sizeof(view->lines[index].prefix_command), command);
   return 1;
}

int llm_driver_screen_view_from_frame(const UiFrame *frame,
                                      LlmDriverScreenView *view)
{
   size_t i;

   if (frame == NULL || view == NULL)
      return 0;
   llm_driver_screen_view_init(view, frame->rows, frame->cols,
                               frame->cursor.valid
                               ? frame->cursor.cursor
                               : logical_cursor_invalid());
   for (i = 0; i < frame->row_count && i < LLM_DRIVER_MAX_ROWS; i++)
   {
      const UiFrameRow *row = &frame->row[i];
      LlmDriverScreenLine *line = &view->lines[i];
      size_t cursor_index = 0;
      int has_cursor = frame->cursor.valid
                    && ui_frame_find_cursor_row(frame, frame->cursor.cursor,
                                                &cursor_index);

      line->line_number = row->line_number;
      line->logical_row = row->screen_row;
      line->role = row->role;
      line->logical_start_col = row->logical_start_col;
      line->editable = row->editable;
      line->current = has_cursor && cursor_index == i;
      line->cursor = line->current;
      line->style_count = row->style_count;
      if (line->style_count > UI_DRIVER_MAX_STYLE_RUNS)
         line->style_count = UI_DRIVER_MAX_STYLE_RUNS;
      if (line->style_count > 0)
         memcpy(line->styles, row->styles,
                line->style_count * sizeof(line->styles[0]));
      copy_text_n(line->prefix, sizeof(line->prefix),
                  (const char *)row->prefix, row->prefix_len);
      copy_text_n(line->text, sizeof(line->text), (const char *)row->text,
                  row->text_len);
      llm_driver_collect_utf_metadata(line);
      view->line_count = i + 1;
   }
   return 1;
}

void llm_driver_screen_view_set_command(LlmDriverScreenView *view,
                                        const char *command_line)
{
   if (view != NULL)
      copy_text(view->command_line, sizeof(view->command_line), command_line);
}

void llm_driver_screen_view_set_status(LlmDriverScreenView *view,
                                       const char *status)
{
   if (view != NULL)
      copy_text(view->status, sizeof(view->status), status);
}

void llm_driver_screen_view_set_buffer(LlmDriverScreenView *view,
                                       const char *path, int dirty,
                                       size_t line_count)
{
   if (view == NULL)
      return;
   copy_text(view->buffer_path, sizeof(view->buffer_path), path);
   view->buffer_dirty = dirty ? 1 : 0;
   view->buffer_line_count = line_count;
   view->buffer_valid = 1;
}

void llm_driver_screen_view_set_history(LlmDriverScreenView *view,
                                        int undo_available,
                                        int redo_available)
{
   if (view == NULL)
      return;
   view->undo_available = undo_available ? 1 : 0;
   view->redo_available = redo_available ? 1 : 0;
}

void llm_driver_screen_view_set_selection(LlmDriverScreenView *view,
                                          int active, LINETYPE start_line,
                                          int start_cell, LINETYPE end_line,
                                          int end_cell,
                                          const char *clipboard)
{
   if (view == NULL)
      return;
   view->selection.active = active ? 1 : 0;
   view->selection.start_line = start_line;
   view->selection.start_cell = start_cell;
   view->selection.end_line = end_line;
   view->selection.end_cell = end_cell;
   copy_text(view->selection.clipboard, sizeof(view->selection.clipboard),
             clipboard);
}

int llm_driver_screen_view_add_buffer_info(LlmDriverScreenView *view,
                                           const char *path, int dirty,
                                           size_t line_count, int current)
{
   LlmDriverBufferInfo *buffer;

   if (view == NULL || view->buffer_count >= LLM_DRIVER_MAX_BUFFERS)
      return 0;
   buffer = &view->buffers[view->buffer_count++];
   copy_text(buffer->path, sizeof(buffer->path), path);
   buffer->dirty = dirty ? 1 : 0;
   buffer->line_count = line_count;
   buffer->current = current ? 1 : 0;
   return 1;
}

void llm_driver_screen_view_set_project_root(LlmDriverScreenView *view,
                                             const char *root)
{
   if (view != NULL)
      copy_text(view->project.root, sizeof(view->project.root), root);
}

int llm_driver_screen_view_add_project_file(LlmDriverScreenView *view,
                                            const char *path)
{
   if (view == NULL
   ||  view->project.file_count >= LLM_DRIVER_MAX_PROJECT_FILES)
      return 0;
   copy_text(view->project.files[view->project.file_count],
             sizeof(view->project.files[view->project.file_count]), path);
   view->project.file_count++;
   return 1;
}

int llm_driver_screen_view_add_diagnostic(LlmDriverScreenView *view,
                                          LINETYPE line, LENGTHTYPE column,
                                          const char *severity,
                                          const char *code,
                                          const char *message)
{
   LlmDriverParserDiagnostic *diagnostic;

   if (view == NULL || view->diagnostic_count >= LLM_DRIVER_MAX_DIAGNOSTICS)
      return 0;
   diagnostic = &view->diagnostics[view->diagnostic_count++];
   diagnostic->line = line;
   diagnostic->column = column;
   copy_text(diagnostic->severity, sizeof(diagnostic->severity), severity);
   copy_text(diagnostic->code, sizeof(diagnostic->code), code);
   copy_text(diagnostic->message, sizeof(diagnostic->message), message);
   return 1;
}

size_t llm_driver_format_screen_view(const LlmDriverScreenView *view,
                                     char *out, size_t out_len)
{
   size_t used = 0;
   size_t i;

   if (out == NULL || out_len == 0)
      return 0;
   out[0] = '\0';
   if (view == NULL)
      return 0;

   appendf(out, out_len, &used, "screen rows=%d cols=%d\n",
           view->rows, view->cols);
   appendf(out, out_len, &used,
           "cursor zone=%s line=%ld row=%d cell=%d screen_row=%d screen_col=%d\n",
           logical_cursor_zone_name(view->cursor.zone),
           (long)view->cursor.line_number,
           view->cursor.zone_row,
           view->cursor.text.cell_column,
           view->cursor_screen_row,
           view->cursor_screen_col);
   if (view->command_line[0] != '\0')
      appendf(out, out_len, &used, "command: %s\n", view->command_line);
   if (view->status[0] != '\0')
      appendf(out, out_len, &used, "status: %s\n", view->status);
   if (view->buffer_valid)
      appendf(out, out_len, &used, "buffer path=\"%s\" dirty=%d lines=%zu\n",
              view->buffer_path, view->buffer_dirty,
              view->buffer_line_count);
   appendf(out, out_len, &used, "history undo=%d redo=%d\n",
           view->undo_available, view->redo_available);
   appendf(out, out_len, &used,
           "selection active=%d start=%ld:%d end=%ld:%d clipboard=\"%s\"\n",
           view->selection.active, (long)view->selection.start_line,
           view->selection.start_cell, (long)view->selection.end_line,
           view->selection.end_cell, view->selection.clipboard);
   for (i = 0; i < view->diagnostic_count; i++)
   {
      const LlmDriverParserDiagnostic *diagnostic = &view->diagnostics[i];

      appendf(out, out_len, &used,
              "diagnostic line=%ld column=%ld severity=%s code=%s message=%s\n",
              (long)diagnostic->line, (long)diagnostic->column,
              diagnostic->severity, diagnostic->code, diagnostic->message);
   }
   for (i = 0; i < view->line_count; i++)
   {
      const LlmDriverScreenLine *line = &view->lines[i];

      appendf(out, out_len, &used,
              "%c%04d line=%ld prefix=\"%s\" text=\"%s\"",
              line->current ? '>' : ' ',
              line->logical_row, (long)line->line_number,
              line->prefix, line->text);
      if (line->prefix_command[0] != '\0')
         appendf(out, out_len, &used, " prefix_command=\"%s\"",
                 line->prefix_command);
      appendf(out, out_len, &used, "\n");
   }
   return used;
}

void llm_driver_format_options_init(LlmDriverFormatOptions *options)
{
   if (options == NULL)
      return;
   memset(options, 0, sizeof(*options));
   options->mode = LLM_DRIVER_VIEW_FULL;
   options->first_row = -1;
   options->row_count = -1;
   options->max_text_cols = 0;
   options->include_prefix = 1;
   options->include_command = 1;
   options->include_status = 1;
   options->include_cursor = 1;
   options->include_all_utf = 0;
   options->compact = 0;
}

size_t llm_driver_format_semantic_view_with_options(
   const LlmDriverScreenView *view, const LlmDriverFormatOptions *options,
   char *out, size_t out_len)
{
   size_t used = 0;
   size_t i;
   LlmDriverFormatOptions local_options;
   size_t emitted = 0;

   if (out == NULL || out_len == 0)
      return 0;
   out[0] = '\0';
   if (view == NULL)
      return 0;
   if (options == NULL)
   {
      llm_driver_format_options_init(&local_options);
      options = &local_options;
   }

   if (options->compact)
   {
      appendf(out, out_len, &used,
              "{\"mode\":");
      append_json_string(out, out_len, &used,
                         llm_driver_view_mode_name(options->mode));
      appendf(out, out_len, &used,
              ",\"rows\":%d,\"cols\":%d", view->rows, view->cols);
      if (options->include_cursor)
      {
         appendf(out, out_len, &used,
                 ",\"focus\":{\"zone\":");
         append_json_string(out, out_len, &used,
                            logical_cursor_zone_name(view->cursor.zone));
         appendf(out, out_len, &used,
                 ",\"line\":%ld,\"row\":%d,\"cell\":%d}",
                 (long)view->cursor.line_number,
                 view->cursor.zone_row,
                 view->cursor.text.cell_column);
      }
      if (options->include_command)
      {
         appendf(out, out_len, &used, ",\"command\":");
         append_json_string(out, out_len, &used, view->command_line);
      }
      if (options->include_status)
      {
         appendf(out, out_len, &used, ",\"status\":");
         append_json_string(out, out_len, &used, view->status);
      }
      if (view->buffer_valid)
      {
         appendf(out, out_len, &used, ",\"buffer\":{\"path\":");
         append_json_string(out, out_len, &used, view->buffer_path);
         appendf(out, out_len, &used, ",\"dirty\":%d,\"lines\":%zu}",
                 view->buffer_dirty, view->buffer_line_count);
      }
      appendf(out, out_len, &used,
              ",\"history\":{\"undo\":%d,\"redo\":%d}",
              view->undo_available, view->redo_available);
      appendf(out, out_len, &used,
              ",\"selection\":{\"active\":%d,\"start_line\":%ld,"
              "\"start_cell\":%d,\"end_line\":%ld,\"end_cell\":%d,"
              "\"clipboard\":",
              view->selection.active, (long)view->selection.start_line,
              view->selection.start_cell, (long)view->selection.end_line,
              view->selection.end_cell);
      append_json_string(out, out_len, &used, view->selection.clipboard);
      appendf(out, out_len, &used, "}");
      appendf(out, out_len, &used, ",\"buffers\":[");
      for (i = 0; i < view->buffer_count; i++)
      {
         const LlmDriverBufferInfo *buffer = &view->buffers[i];

         appendf(out, out_len, &used, "%s{\"index\":%zu,\"path\":",
                 i == 0 ? "" : ",", i);
         append_json_string(out, out_len, &used, buffer->path);
         appendf(out, out_len, &used,
                 ",\"dirty\":%d,\"lines\":%zu,\"current\":%d}",
                 buffer->dirty, buffer->line_count, buffer->current);
      }
      appendf(out, out_len, &used, "]");
      if (view->project.root[0] != '\0')
      {
         appendf(out, out_len, &used, ",\"project\":{\"root\":");
         append_json_string(out, out_len, &used, view->project.root);
         appendf(out, out_len, &used, ",\"files\":[");
         for (i = 0; i < view->project.file_count; i++)
         {
            appendf(out, out_len, &used, "%s", i == 0 ? "" : ",");
            append_json_string(out, out_len, &used, view->project.files[i]);
         }
         appendf(out, out_len, &used, "]}");
      }
      append_diagnostics(out, out_len, &used, view, 1);
      appendf(out, out_len, &used, ",\"screen_rows\":[");
      for (i = 0; i < view->line_count; i++)
      {
         const LlmDriverScreenLine *line = &view->lines[i];

         if (!llm_driver_row_matches_options(line, options))
            continue;
         appendf(out, out_len, &used, "%s{\"r\":%d,\"role\":",
                 (emitted > 0) ? "," : "", line->logical_row);
         append_json_string(out, out_len, &used, ui_row_role_name(line->role));
         appendf(out, out_len, &used,
                 ",\"line\":%ld,\"cur\":%d",
                 (long)line->line_number, line->cursor);
         if (options->include_prefix)
         {
            appendf(out, out_len, &used, ",\"p\":");
            append_json_string_limited(out, out_len, &used, line->prefix,
                                       options->max_text_cols);
         }
         if (line->prefix_command[0] != '\0')
         {
            appendf(out, out_len, &used, ",\"pc\":");
            append_json_string(out, out_len, &used, line->prefix_command);
         }
         appendf(out, out_len, &used, ",\"t\":");
         append_json_string_limited(out, out_len, &used, line->text,
                                    options->max_text_cols);
         append_style_runs(out, out_len, &used, line->styles,
                           line->style_count, options->max_text_cols, 1);
         append_utf_metadata(out, out_len, &used, line,
                             options->max_text_cols, 1,
                             options->include_all_utf);
         appendf(out, out_len, &used, "}");
         emitted++;
      }
      appendf(out, out_len, &used, "]}\n");
      return used;
   }

   appendf(out, out_len, &used,
           "{\n  \"mode\": ");
   append_json_string(out, out_len, &used,
                      llm_driver_view_mode_name(options->mode));
   appendf(out, out_len, &used,
           ",\n  \"screen\": {\"rows\": %d, \"cols\": %d},\n",
           view->rows, view->cols);
   appendf(out, out_len, &used,
           "  \"focus\": {\"zone\": ");
   append_json_string(out, out_len, &used,
                      logical_cursor_zone_name(view->cursor.zone));
   appendf(out, out_len, &used,
           ", \"line\": %ld, \"row\": %d, \"cell\": %d, \"desired_cell\": %d},\n",
           (long)view->cursor.line_number,
           view->cursor.zone_row,
           view->cursor.text.cell_column,
           view->cursor.desired_cell);
   if (options->include_command)
   {
      appendf(out, out_len, &used, "  \"command\": ");
      append_json_string(out, out_len, &used, view->command_line);
      appendf(out, out_len, &used, ",\n");
   }
   if (options->include_status)
   {
      appendf(out, out_len, &used, "  \"status\": ");
      append_json_string(out, out_len, &used, view->status);
      appendf(out, out_len, &used, ",\n");
   }
   if (view->buffer_valid)
   {
      appendf(out, out_len, &used,
              "  \"buffer\": {\"path\": ");
      append_json_string(out, out_len, &used, view->buffer_path);
      appendf(out, out_len, &used,
              ", \"dirty\": %d, \"lines\": %zu},\n",
              view->buffer_dirty, view->buffer_line_count);
   }
   appendf(out, out_len, &used,
           "  \"history\": {\"undo\": %d, \"redo\": %d},\n",
           view->undo_available, view->redo_available);
   appendf(out, out_len, &used,
           "  \"selection\": {\"active\": %d, \"start_line\": %ld, "
           "\"start_cell\": %d, \"end_line\": %ld, \"end_cell\": %d, "
           "\"clipboard\": ",
           view->selection.active, (long)view->selection.start_line,
           view->selection.start_cell, (long)view->selection.end_line,
           view->selection.end_cell);
   append_json_string(out, out_len, &used, view->selection.clipboard);
   appendf(out, out_len, &used, "},\n");
   appendf(out, out_len, &used, "  \"buffers\": [");
   for (i = 0; i < view->buffer_count; i++)
   {
      const LlmDriverBufferInfo *buffer = &view->buffers[i];

      appendf(out, out_len, &used, "%s{\"index\": %zu, \"path\": ",
              i == 0 ? "" : ", ", i);
      append_json_string(out, out_len, &used, buffer->path);
      appendf(out, out_len, &used,
              ", \"dirty\": %d, \"lines\": %zu, \"current\": %d}",
              buffer->dirty, buffer->line_count, buffer->current);
   }
   appendf(out, out_len, &used, "],\n");
   if (view->project.root[0] != '\0')
   {
      appendf(out, out_len, &used, "  \"project\": {\"root\": ");
      append_json_string(out, out_len, &used, view->project.root);
      appendf(out, out_len, &used, ", \"files\": [");
      for (i = 0; i < view->project.file_count; i++)
      {
         appendf(out, out_len, &used, "%s", i == 0 ? "" : ", ");
         append_json_string(out, out_len, &used, view->project.files[i]);
      }
      appendf(out, out_len, &used, "]},\n");
   }
   append_diagnostics(out, out_len, &used, view, 0);
   appendf(out, out_len, &used, "  \"rows\": [\n");
   for (i = 0; i < view->line_count; i++)
   {
      const LlmDriverScreenLine *line = &view->lines[i];

      if (!llm_driver_row_matches_options(line, options))
         continue;
      appendf(out, out_len, &used,
              "    %s{\"index\": %zu, \"role\": ",
              (emitted > 0) ? "," : "", i);
      append_json_string(out, out_len, &used,
                         ui_row_role_name(line->role));
      appendf(out, out_len, &used,
              ", \"line\": %ld, \"screen_row\": %d, \"start_cell\": %d, "
              "\"editable\": %d, \"current\": %d, \"cursor\": %d, "
              "\"prefix\": ",
              (long)line->line_number,
              line->logical_row,
              line->logical_start_col,
              line->editable,
              line->current,
              line->cursor);
      if (options->include_prefix)
         append_json_string_limited(out, out_len, &used, line->prefix,
                                    options->max_text_cols);
      else
         append_json_string(out, out_len, &used, "");
      if (line->prefix_command[0] != '\0')
      {
         appendf(out, out_len, &used, ", \"prefix_command\": ");
         append_json_string(out, out_len, &used, line->prefix_command);
      }
      appendf(out, out_len, &used, ", \"text\": ");
      append_json_string_limited(out, out_len, &used, line->text,
                                 options->max_text_cols);
      append_style_runs(out, out_len, &used, line->styles,
                        line->style_count, options->max_text_cols, 0);
      append_utf_metadata(out, out_len, &used, line,
                          options->max_text_cols, 0,
                          options->include_all_utf);
      appendf(out, out_len, &used, "}\n");
      emitted++;
   }
   appendf(out, out_len, &used, "  ]\n}\n");
   return used;
}

size_t llm_driver_format_semantic_view(const LlmDriverScreenView *view,
                                       char *out, size_t out_len)
{
   LlmDriverFormatOptions options;

   llm_driver_format_options_init(&options);
   return llm_driver_format_semantic_view_with_options(view, &options,
                                                       out, out_len);
}

static int llm_driver_line_changed(const LlmDriverScreenLine *left,
                                   const LlmDriverScreenLine *right)
{
   if (left == NULL || right == NULL)
      return left != right;
   return left->line_number != right->line_number
       || left->logical_row != right->logical_row
       || left->role != right->role
       || left->cursor != right->cursor
       || strcmp(left->prefix, right->prefix) != 0
       || strcmp(left->prefix_command, right->prefix_command) != 0
       || strcmp(left->text, right->text) != 0
       || left->utf_cluster_count != right->utf_cluster_count
       || memcmp(left->utf_clusters, right->utf_clusters,
                 left->utf_cluster_count * sizeof(left->utf_clusters[0])) != 0;
}

size_t llm_driver_format_delta_view(const LlmDriverScreenView *previous,
                                    const LlmDriverScreenView *current,
                                    const LlmDriverFormatOptions *options,
                                    char *out, size_t out_len)
{
   size_t used = 0;
   size_t i;
   size_t emitted = 0;
   LlmDriverFormatOptions local_options;

   if (out == NULL || out_len == 0)
      return 0;
   out[0] = '\0';
   if (current == NULL)
      return 0;
   if (options == NULL)
   {
      llm_driver_format_options_init(&local_options);
      options = &local_options;
   }

   appendf(out, out_len, &used,
           "{\"mode\":\"delta\",\"baseline\":%d,\"focus_changed\":%d,"
           "\"status_changed\":%d,\"buffer_changed\":%d,\"rows\":[",
           previous != NULL,
           previous == NULL
        || previous->cursor.zone != current->cursor.zone
        || previous->cursor.line_number != current->cursor.line_number
        || previous->cursor.zone_row != current->cursor.zone_row
        || previous->cursor.text.cell_column != current->cursor.text.cell_column,
           previous == NULL
        || strcmp(previous->status, current->status) != 0,
           previous == NULL
        || previous->buffer_dirty != current->buffer_dirty
        || previous->buffer_line_count != current->buffer_line_count
        || strcmp(previous->buffer_path, current->buffer_path) != 0);

   for (i = 0; i < current->line_count; i++)
   {
      const LlmDriverScreenLine *line = &current->lines[i];
      const LlmDriverScreenLine *old =
         previous != NULL && i < previous->line_count
       ? &previous->lines[i] : NULL;

      if (!llm_driver_row_matches_options(line, options))
         continue;
      if (previous != NULL && !llm_driver_line_changed(old, line))
         continue;
      appendf(out, out_len, &used, "%s{\"r\":%d,\"role\":",
              emitted > 0 ? "," : "", line->logical_row);
      append_json_string(out, out_len, &used, ui_row_role_name(line->role));
      appendf(out, out_len, &used, ",\"line\":%ld,\"cur\":%d",
              (long)line->line_number, line->cursor);
      if (options->include_prefix)
      {
         appendf(out, out_len, &used, ",\"p\":");
         append_json_string_limited(out, out_len, &used, line->prefix,
                                    options->max_text_cols);
      }
      if (line->prefix_command[0] != '\0')
      {
         appendf(out, out_len, &used, ",\"pc\":");
         append_json_string(out, out_len, &used, line->prefix_command);
      }
      appendf(out, out_len, &used, ",\"t\":");
      append_json_string_limited(out, out_len, &used, line->text,
                                 options->max_text_cols);
      append_utf_metadata(out, out_len, &used, line,
                          options->max_text_cols, 1,
                          options->include_all_utf);
      appendf(out, out_len, &used, "}");
      emitted++;
   }
   appendf(out, out_len, &used, "]}\n");
   return used;
}

const char *llm_driver_input_kind_name(LlmDriverInputKind kind)
{
   return the_input_kind_name(kind);
}

const char *llm_driver_debug_command_name(LlmDriverDebugCommand command)
{
   return the_input_debug_command_name(command);
}

const char *llm_driver_logical_target_kind_name(LlmDriverLogicalTargetKind kind)
{
   return the_input_logical_target_kind_name(kind);
}

LlmDriverInput llm_driver_input_none(void)
{
   return the_input_event_none();
}

int llm_driver_input_from_text(uint32_t codepoint, LlmDriverInput *out)
{
   return the_input_event_from_text(codepoint, out);
}

int llm_driver_input_from_key_name(const char *name, LlmDriverInput *out)
{
   return the_input_event_from_key_name(name, out);
}

int llm_driver_input_from_legacy_key(int key_code, LlmDriverInput *out)
{
   return the_input_event_from_legacy_key(key_code, out);
}

int llm_driver_input_from_command(const char *command, LlmDriverInput *out)
{
   return the_input_event_from_command(command, out);
}

int llm_driver_input_from_logical_target(LlmDriverLogicalTargetKind kind,
                                         LINETYPE line_number, int row,
                                         int cell, int screen, int window_id,
                                         LlmDriverInput *out)
{
   return the_input_event_from_logical_target(kind, line_number, row, cell,
                                             screen, window_id, out);
}

int llm_driver_input_from_logical_hit(LogicalCursorZone zone,
                                      LINETYPE line_number, int row,
                                      int cell, LlmDriverInput *out)
{
   return the_input_event_from_logical_hit(zone, line_number, row, cell, out);
}

int llm_driver_input_from_debug_command(const char *name,
                                        LlmDriverInput *out)
{
   return the_input_event_from_debug_command(name, out);
}

int llm_driver_input_to_legacy_key(const LlmDriverInput *input, int *key_code)
{
   return the_input_event_to_legacy_key(input, key_code);
}

void llm_driver_input_queue_init(LlmDriverInputQueue *queue)
{
   the_input_queue_init(queue);
}

int llm_driver_input_queue_push(LlmDriverInputQueue *queue, LlmDriverInput input)
{
   return the_input_queue_push(queue, input);
}

int llm_driver_input_queue_pop_legacy_key(LlmDriverInputQueue *queue, int *key_code)
{
   return the_input_queue_pop_legacy_key(queue, key_code);
}

void llm_driver_debug_snapshot_init(LlmDriverDebugSnapshot *debug,
                                    LogicalCursor focus)
{
   if (debug == NULL)
      return;
   memset(debug, 0, sizeof(*debug));
   debug->focus = focus;
   debug->cursor_mapping.logical_cell = focus.text.cell_column;
   ui_driver_op_log_init(&debug->driver_ops);
}

void llm_driver_debug_snapshot_set_cursor_mapping(
   LlmDriverDebugSnapshot *debug, int viewport_col, int logical_cell,
   int raw_display_col, int display_col, int visible)
{
   if (debug == NULL)
      return;
   debug->cursor_mapping.viewport_col = viewport_col;
   debug->cursor_mapping.logical_cell = logical_cell;
   debug->cursor_mapping.raw_display_col = raw_display_col;
   debug->cursor_mapping.display_col = display_col;
   debug->cursor_mapping.visible = visible;
}

void llm_driver_debug_snapshot_set_last_render(LlmDriverDebugSnapshot *debug,
                                               const char *last_render)
{
   if (debug != NULL)
      copy_text(debug->last_render, sizeof(debug->last_render), last_render);
}

static const char *driver_op_kind_name(UiDriverOpKind kind)
{
   switch (kind)
   {
      case UI_DRIVER_OP_ROW:
         return "row";
      case UI_DRIVER_OP_CURSOR:
         return "cursor";
      case UI_DRIVER_OP_REFRESH:
         return "refresh";
      case UI_DRIVER_OP_NONE:
      default:
         return "none";
   }
}

size_t llm_driver_format_debug_snapshot(const LlmDriverDebugSnapshot *debug,
                                        char *out, size_t out_len)
{
   size_t used = 0;
   size_t i;

   if (out == NULL || out_len == 0)
      return 0;
   out[0] = '\0';
   if (debug == NULL)
      return 0;

   appendf(out, out_len, &used,
           "{\n  \"focus\": {\"zone\": ");
   append_json_string(out, out_len, &used,
                      logical_cursor_zone_name(debug->focus.zone));
   appendf(out, out_len, &used,
           ", \"line\": %ld, \"row\": %d, \"cell\": %d},\n",
           (long)debug->focus.line_number,
           debug->focus.zone_row,
           debug->focus.text.cell_column);
   appendf(out, out_len, &used,
           "  \"cursor_mapping\": {\"viewport_col\": %d, "
           "\"logical_cell\": %d, \"raw_display_col\": %d, "
           "\"display_col\": %d, \"visible\": %d},\n",
           debug->cursor_mapping.viewport_col,
           debug->cursor_mapping.logical_cell,
           debug->cursor_mapping.raw_display_col,
           debug->cursor_mapping.display_col,
           debug->cursor_mapping.visible);
   appendf(out, out_len, &used, "  \"last_render\": ");
   append_json_string(out, out_len, &used, debug->last_render);
   appendf(out, out_len, &used, ",\n  \"driver_ops\": [\n");
   for (i = 0; i < debug->driver_ops.count; i++)
   {
      const UiDriverOp *op = &debug->driver_ops.op[i];

      appendf(out, out_len, &used,
              "    {\"index\": %zu, \"kind\": ", i);
      append_json_string(out, out_len, &used, driver_op_kind_name(op->kind));
      appendf(out, out_len, &used,
              ", \"role\": ");
      append_json_string(out, out_len, &used, ui_row_role_name(op->role));
      appendf(out, out_len, &used,
              ", \"row\": %d, \"col\": %d, \"line\": %ld}%s\n",
              op->row, op->col, (long)op->line_number,
              (i + 1 < debug->driver_ops.count) ? "," : "");
   }
   appendf(out, out_len, &used, "  ]\n}\n");
   return used;
}
