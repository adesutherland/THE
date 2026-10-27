#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "websession.h"
#include "frontendaction.h"
#include "headlessdriver.h"
#include "llmdriver.h"
#include "llmruntime.h"
#include "panelui.h"
#include "the.h"
#include "proto.h"
#include "driverlayout.h"
#include "driverwindow.h"
#include "thedriver.h"
#include "vars.h"

static void web_driver_seed_filearea_focus(int row, int display_col)
{
   SHOW_LINE *show_row;
   const CHARTYPE *line;
   size_t len;
   int logical_col;
   LogicalCursor cursor;

   if (CURRENT_VIEW == NULL || screen[current_screen].sl == NULL
   ||  row < 0 || row >= screen[current_screen].rows[WINDOW_FILEAREA])
      return;
   show_row = &screen[current_screen].sl[row];
   if (show_row->line_type & (LINE_OUT_OF_BOUNDS_ABOVE
                            | LINE_OUT_OF_BOUNDS_BELOW))
      return;
   if (show_row->line_type == LINE_TOF || show_row->line_type == LINE_EOF)
   {
      line = (const CHARTYPE *)"";
      len = 0;
   }
   else
   {
      line = show_row->contents != NULL ? show_row->contents : rec;
      len = show_row->contents != NULL ? show_row->length : rec_len;
   }
   logical_col = driver_layout_logical_col_from_display(
      line, len, (int)CURRENT_VIEW->verify_col - 1, display_col,
      TEXT_SNAP_BACKWARD);
   cursor = logical_cursor_from_cell(
      LOGICAL_CURSOR_ZONE_FILEAREA, show_row->line_number, row, line, len,
      logical_col, TEXT_SNAP_BACKWARD, 1);
   logical_cursor_set_desired_cell(
      &cursor, driver_layout_width_col_from_logical(line, len,
                                                    cursor.text.cell_column));
   logical_cursor_state_focus(&CURRENT_VIEW->logical_cursor, cursor);
}

static void web_driver_sync_focus(void)
{
   LogicalCursor logical;
   TheDriverWindow *window;
   int row = 0;
   int col = 0;

   if (CURRENT_VIEW == NULL)
      return;
   logical = CURRENT_VIEW->logical_cursor.current;
   if (!logical.valid && CURRENT_VIEW->current_window == WINDOW_COMMAND)
   {
      CURRENT_VIEW->previous_window = CURRENT_VIEW->current_window;
      CURRENT_VIEW->current_window = WINDOW_FILEAREA;
   }
   the_driver_set_current_screen(current_screen);
   the_driver_set_screen_current_role(current_screen,
                                      CURRENT_VIEW->current_window);
   if (!logical.valid)
   {
      window = driver_current_window();
      if (CURRENT_VIEW->current_window == WINDOW_FILEAREA
      ||  CURRENT_VIEW->current_window == WINDOW_PREFIX)
      {
         row = get_row_for_focus_line(current_screen,
                                      CURRENT_VIEW->focus_line,
                                      CURRENT_VIEW->current_row);
         if (row < 0)
            row = CURRENT_VIEW->current_row;
         col = CURRENT_VIEW->x[CURRENT_VIEW->current_window];
      }
      else if (CURRENT_VIEW->current_window == WINDOW_COMMAND)
         col = CURRENT_VIEW->cmdline_col;
      if (window != NULL)
         the_driver->move_window_cursor(window, (short)row, (short)col);
      if (CURRENT_VIEW->current_window == WINDOW_FILEAREA)
         web_driver_seed_filearea_focus(row, col);
   }
   cursor_focus_sync_current(current_screen, CURRENT_VIEW);
}

size_t the_web_format_actions(char *catalog, size_t capacity)
{
   size_t used = 0;
   size_t i;
   int len;

   len = snprintf(catalog, capacity,
                  "{\"type\":\"actions\",\"actions\":[");
   if (len < 0 || (size_t)len >= capacity)
      return 0;
   used = (size_t)len;
   for (i = 0; i < the_frontend_action_definition_count(); i++)
   {
      const TheFrontendActionDefinition *definition =
         the_frontend_action_definition_at(i);

      len = snprintf(catalog + used, capacity - used,
                     "%s{\"id\":\"%s\",\"menu\":%s%s%s,"
                     "\"label\":\"%s\",\"requires_argument\":%s}",
                     i == 0 ? "" : ",", definition->name,
                     definition->menu != NULL ? "\"" : "null",
                     definition->menu != NULL ? definition->menu : "",
                     definition->menu != NULL ? "\"" : "",
                     definition->label,
                     definition->requires_argument ? "true" : "false");
      if (len < 0 || (size_t)len >= capacity - used)
         return 0;
      used += (size_t)len;
   }
   len = snprintf(catalog + used, capacity - used, "]}");
   if (len < 0 || (size_t)len >= capacity - used)
      return 0;
   used += (size_t)len;
   return used;
}

size_t the_web_format_snapshot(char *out, size_t capacity)
{
   LlmDriverFormatOptions options;
   size_t length;

   if (out == NULL || capacity == 0 || CURRENT_VIEW == NULL || CURRENT_FILE == NULL)
      return 0;

   web_driver_sync_focus();
   /* Buffer switches (including HELP) can build rows without preparing their
    * displayed prefixes. Keep each published frame complete. */
   if (CURRENT_VIEW != NULL && CURRENT_FILE != NULL
   &&  the_panel_active == NULL && !in_macro)
   {
      build_screen(current_screen);
      prepare_screen_for_display(current_screen);
   }
   llm_driver_format_options_init(&options);
   options.compact = 1;
   options.max_text_cols = LLM_DRIVER_MAX_COLS;
   options.include_all_utf = 0;
   length = llm_runtime_format_screen(
      current_screen, &options, out,
      capacity);
   while (length > 0
   && (out[length - 1] == '\n'
    || out[length - 1] == '\r'))
      out[--length] = '\0';
   if (length > 0 && out[length-1] == '}')
   {
      size_t offset = length-1;
      int appended = snprintf(out+offset, capacity-offset,
         ",\"viewport\":{\"rows\":%d,\"cols\":%d,\"reserved_rows\":%d,\"prefix_cols\":%d,\"prefix_gap\":%d}}",
         terminal_lines, terminal_cols,
         terminal_lines-CURRENT_SCREEN.rows[WINDOW_FILEAREA],
         CURRENT_VIEW->prefix ? CURRENT_VIEW->prefix_width-CURRENT_VIEW->prefix_gap : 0,
         CURRENT_VIEW->prefix ? CURRENT_VIEW->prefix_gap : 0);
      if (appended > 0 && (size_t)appended < capacity-offset)
         length = offset+(size_t)appended;
   }
   return length;
}

static int web_driver_decode_utf8(const unsigned char *text, size_t len,
                                  size_t *used, uint32_t *codepoint)
{
   uint32_t value;
   size_t count;
   size_t i;

   if (text == NULL || len == 0 || used == NULL || codepoint == NULL)
      return 0;
   if (text[0] < 0x80)
   {
      *used = 1;
      *codepoint = text[0];
      return 1;
   }
   if ((text[0] & 0xe0) == 0xc0)
   {
      count = 2;
      value = text[0] & 0x1f;
      if (value < 2)
         return 0;
   }
   else if ((text[0] & 0xf0) == 0xe0)
   {
      count = 3;
      value = text[0] & 0x0f;
   }
   else if ((text[0] & 0xf8) == 0xf0)
   {
      count = 4;
      value = text[0] & 0x07;
   }
   else
      return 0;
   if (count > len)
      return 0;
   for (i = 1; i < count; i++)
   {
      if ((text[i] & 0xc0) != 0x80)
         return 0;
      value = (value << 6) | (uint32_t)(text[i] & 0x3f);
   }
   if ((count == 3 && value < 0x800)
   ||  (count == 4 && value < 0x10000)
   ||  value > 0x10ffff
   ||  (value >= 0xd800 && value <= 0xdfff))
      return 0;
   *used = count;
   *codepoint = value;
   return 1;
}

static int web_driver_queue_text(TheInputQueue *queue, const char *text)
{
   uint32_t codepoints[THE_INPUT_QUEUE_MAX];
   const unsigned char *bytes = (const unsigned char *)text;
   size_t len = text != NULL ? strlen(text) : 0;
   size_t offset = 0;
   size_t count = 0;
   size_t i;

   while (offset < len)
   {
      size_t used = 0;

      if (count >= THE_INPUT_QUEUE_MAX
      ||  !web_driver_decode_utf8(bytes + offset, len - offset, &used,
                                  &codepoints[count]))
         return 0;
      offset += used;
      count++;
   }
   if (count > THE_INPUT_QUEUE_MAX - queue->count)
      return 0;
   for (i = 0; i < count; i++)
   {
      TheInputEvent event;

      if (!the_input_event_from_text(codepoints[i], &event)
      ||  !the_input_queue_push(queue, event))
         return 0;
   }
   return count > 0;
}

int the_web_queue_request(TheInputQueue *queue, const char *type,
                          const char *value, const char *argument,
                          long line, int row, int cell, int rows, int cols)
{
   TheInputEvent event;
   int valid = 0;
   if (!queue || !type) return 0;
   if (!value) value = "";
   if (strcmp(type, "text") == 0)
      return web_driver_queue_text(queue, value);
   if (strcmp(type, "key") == 0)
      valid = the_input_event_from_key_name(value, &event);
   else if (strcmp(type, "command") == 0)
      valid = the_input_event_from_restricted_command(value, &event);
   else if (strcmp(type, "action") == 0)
      valid = the_input_event_from_action(value, argument ? argument : "", &event);
   else if (strcmp(type, "resize") == 0)
      valid = rows >= 12 && rows <= 256 && cols >= 40 && cols <= 1000
           && the_input_event_from_resize(rows, cols, &event);
   else if (strcmp(type, "hit") == 0)
   {
      TheInputLogicalTargetKind kind;
      valid = the_input_logical_target_kind_from_name(value, &kind)
           && the_input_event_from_logical_target(kind, (LINETYPE)line, row, cell, 0, -1, &event);
   }
   return valid && the_input_queue_push(queue, event);
}
