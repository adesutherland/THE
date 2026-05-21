#include "llmdriver.h"

#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "getch.h"

#ifdef KEY_TAB
# define LLM_DRIVER_KEY_TAB KEY_TAB
#else
# define LLM_DRIVER_KEY_TAB 0x9
#endif

typedef struct
{
   const char *name;
   int key_code;
} LlmKeyName;

typedef struct
{
   const char *name;
   LlmDriverDebugCommand command;
} LlmDebugName;

static const LlmKeyName llm_key_names[] =
{
   { "left", KEY_LEFT },
   { "right", KEY_RIGHT },
   { "up", KEY_UP },
   { "down", KEY_DOWN },
   { "home", KEY_HOME },
   { "end", KEY_END },
   { "pageup", KEY_PPAGE },
   { "pgup", KEY_PPAGE },
   { "pagedown", KEY_NPAGE },
   { "pgdn", KEY_NPAGE },
   { "enter", KEY_ENTER },
   { "return", KEY_RETURN },
   { "esc", KEY_ESC },
   { "escape", KEY_ESC },
   { "tab", LLM_DRIVER_KEY_TAB },
   { "backtab", KEY_BTAB },
   { "btab", KEY_BTAB },
   { "backspace", KEY_BACKSPACE },
   { "delete", KEY_DC },
   { "del", KEY_DC },
   { "insert", KEY_IC },
   { NULL, 0 }
};

static const LlmDebugName llm_debug_names[] =
{
   { "describe-focus", LLM_DRIVER_DEBUG_DESCRIBE_FOCUS },
   { "focus", LLM_DRIVER_DEBUG_DESCRIBE_FOCUS },
   { "describe-row", LLM_DRIVER_DEBUG_DESCRIBE_ROW },
   { "row", LLM_DRIVER_DEBUG_DESCRIBE_ROW },
   { "list-visible-rows", LLM_DRIVER_DEBUG_LIST_VISIBLE_ROWS },
   { "visible-rows", LLM_DRIVER_DEBUG_LIST_VISIBLE_ROWS },
   { "dump-cursor-mapping", LLM_DRIVER_DEBUG_DUMP_CURSOR_MAPPING },
   { "cursor-mapping", LLM_DRIVER_DEBUG_DUMP_CURSOR_MAPPING },
   { "dump-driver-ops", LLM_DRIVER_DEBUG_DUMP_DRIVER_OPS },
   { "driver-ops", LLM_DRIVER_DEBUG_DUMP_DRIVER_OPS },
   { "explain-last-render", LLM_DRIVER_DEBUG_EXPLAIN_LAST_RENDER },
   { "last-render", LLM_DRIVER_DEBUG_EXPLAIN_LAST_RENDER },
   { NULL, LLM_DRIVER_DEBUG_NONE }
};

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

static int ascii_equal_ci(const char *left, const char *right)
{
   if (left == NULL || right == NULL)
      return 0;
   while (*left != '\0' && *right != '\0')
   {
      if (tolower((unsigned char)*left) != tolower((unsigned char)*right))
         return 0;
      left++;
      right++;
   }
   return *left == '\0' && *right == '\0';
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
   copy_text(line->prefix, sizeof(line->prefix), prefix);
   copy_text(line->text, sizeof(line->text), text);
   if (index >= view->line_count)
      view->line_count = index + 1;
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
      line->prefix[0] = '\0';
      copy_text_n(line->text, sizeof(line->text), (const char *)row->text,
                  row->text_len);
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
   for (i = 0; i < view->line_count; i++)
   {
      const LlmDriverScreenLine *line = &view->lines[i];

      appendf(out, out_len, &used, "%c%04d line=%ld prefix=\"%s\" text=\"%s\"\n",
              line->current ? '>' : ' ',
              line->logical_row, (long)line->line_number,
              line->prefix, line->text);
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
         appendf(out, out_len, &used, ",\"t\":");
         append_json_string_limited(out, out_len, &used, line->text,
                                    options->max_text_cols);
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
      appendf(out, out_len, &used, ", \"text\": ");
      append_json_string_limited(out, out_len, &used, line->text,
                                 options->max_text_cols);
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

const char *llm_driver_input_kind_name(LlmDriverInputKind kind)
{
   switch (kind)
   {
      case LLM_DRIVER_INPUT_TEXT:
         return "text";
      case LLM_DRIVER_INPUT_KEY:
         return "key";
      case LLM_DRIVER_INPUT_COMMAND:
         return "command";
      case LLM_DRIVER_INPUT_LOGICAL_HIT:
         return "logical-hit";
      case LLM_DRIVER_INPUT_DEBUG:
         return "debug";
      case LLM_DRIVER_INPUT_NONE:
      default:
         return "none";
   }
}

const char *llm_driver_debug_command_name(LlmDriverDebugCommand command)
{
   switch (command)
   {
      case LLM_DRIVER_DEBUG_DESCRIBE_FOCUS:
         return "describe-focus";
      case LLM_DRIVER_DEBUG_DESCRIBE_ROW:
         return "describe-row";
      case LLM_DRIVER_DEBUG_LIST_VISIBLE_ROWS:
         return "list-visible-rows";
      case LLM_DRIVER_DEBUG_DUMP_CURSOR_MAPPING:
         return "dump-cursor-mapping";
      case LLM_DRIVER_DEBUG_DUMP_DRIVER_OPS:
         return "dump-driver-ops";
      case LLM_DRIVER_DEBUG_EXPLAIN_LAST_RENDER:
         return "explain-last-render";
      case LLM_DRIVER_DEBUG_NONE:
      default:
         return "none";
   }
}

LlmDriverInput llm_driver_input_none(void)
{
   LlmDriverInput input;

   memset(&input, 0, sizeof(input));
   input.kind = LLM_DRIVER_INPUT_NONE;
   input.key_code = -1;
   return input;
}

int llm_driver_input_from_text(uint32_t codepoint, LlmDriverInput *out)
{
   if (out == NULL)
      return 0;
   *out = llm_driver_input_none();
   out->kind = LLM_DRIVER_INPUT_TEXT;
   out->codepoint = codepoint;
   if (codepoint <= 0x7Fu)
      out->key_code = (int)codepoint;
   return 1;
}

int llm_driver_input_from_key_name(const char *name, LlmDriverInput *out)
{
   size_t i;

   if (out == NULL || name == NULL)
      return 0;
   *out = llm_driver_input_none();
   if ((name[0] == 'f' || name[0] == 'F') && isdigit((unsigned char)name[1]))
   {
      char *end = NULL;
      long number = strtol(name + 1, &end, 10);

      if (end != NULL && *end == '\0' && number >= 1 && number <= 64)
      {
         out->kind = LLM_DRIVER_INPUT_KEY;
         out->key_code = KEY_F((int)number);
         return 1;
      }
   }

   for (i = 0; llm_key_names[i].name != NULL; i++)
   {
      if (ascii_equal_ci(name, llm_key_names[i].name))
      {
         out->kind = LLM_DRIVER_INPUT_KEY;
         out->key_code = llm_key_names[i].key_code;
         return 1;
      }
   }
   return 0;
}

int llm_driver_input_from_command(const char *command, LlmDriverInput *out)
{
   if (out == NULL)
      return 0;
   *out = llm_driver_input_none();
   out->kind = LLM_DRIVER_INPUT_COMMAND;
   copy_text(out->command, sizeof(out->command), command);
   return out->command[0] != '\0';
}

int llm_driver_input_from_logical_hit(LogicalCursorZone zone,
                                      LINETYPE line_number, int row,
                                      int cell, LlmDriverInput *out)
{
   if (out == NULL)
      return 0;
   *out = llm_driver_input_none();
   out->kind = LLM_DRIVER_INPUT_LOGICAL_HIT;
   out->target.zone = zone;
   out->target.line_number = line_number;
   out->target.row = row;
   out->target.cell = cell;
   return zone != LOGICAL_CURSOR_ZONE_NONE && row >= 0 && cell >= 0;
}

int llm_driver_input_from_debug_command(const char *name,
                                        LlmDriverInput *out)
{
   size_t i;

   if (out == NULL || name == NULL)
      return 0;
   *out = llm_driver_input_none();
   for (i = 0; llm_debug_names[i].name != NULL; i++)
   {
      if (ascii_equal_ci(name, llm_debug_names[i].name))
      {
         out->kind = LLM_DRIVER_INPUT_DEBUG;
         out->debug_command = llm_debug_names[i].command;
         return 1;
      }
   }
   return 0;
}

int llm_driver_input_to_legacy_key(const LlmDriverInput *input, int *key_code)
{
   if (input == NULL || key_code == NULL)
      return 0;
   if (input->kind != LLM_DRIVER_INPUT_TEXT
   &&  input->kind != LLM_DRIVER_INPUT_KEY)
      return 0;
   if (input->key_code < 0)
      return 0;
   *key_code = input->key_code;
   return 1;
}

void llm_driver_input_queue_init(LlmDriverInputQueue *queue)
{
   if (queue != NULL)
      memset(queue, 0, sizeof(*queue));
}

int llm_driver_input_queue_push(LlmDriverInputQueue *queue, LlmDriverInput input)
{
   if (queue == NULL || queue->count >= LLM_DRIVER_INPUT_QUEUE_MAX)
      return 0;
   queue->items[queue->tail] = input;
   queue->tail = (queue->tail + 1) % LLM_DRIVER_INPUT_QUEUE_MAX;
   queue->count++;
   return 1;
}

int llm_driver_input_queue_pop_legacy_key(LlmDriverInputQueue *queue, int *key_code)
{
   LlmDriverInput input;

   if (queue == NULL || key_code == NULL || queue->count == 0)
      return 0;
   input = queue->items[queue->head];
   if (!llm_driver_input_to_legacy_key(&input, key_code))
      return 0;
   queue->head = (queue->head + 1) % LLM_DRIVER_INPUT_QUEUE_MAX;
   queue->count--;
   return 1;
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
