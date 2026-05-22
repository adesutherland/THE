#include "agentdriver.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "getch.h"

static void agent_copy_text(char *dest, size_t dest_len, const char *src)
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

static void agent_set_status(AgentDriver *driver, const char *status)
{
   if (driver != NULL)
      agent_copy_text(driver->status, sizeof(driver->status), status);
}

static int agent_ascii_equal_ci(const char *left, const char *right)
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

static int agent_ascii_starts_ci(const char *text, const char *prefix)
{
   if (text == NULL || prefix == NULL)
      return 0;
   while (*prefix != '\0')
   {
      if (*text == '\0')
         return 0;
      if (tolower((unsigned char)*text) !=
          tolower((unsigned char)*prefix))
         return 0;
      text++;
      prefix++;
   }
   return 1;
}

static char *agent_trim(char *text)
{
   char *end;

   while (text != NULL && isspace((unsigned char)*text))
      text++;
   if (text == NULL || *text == '\0')
      return text;
   end = text + strlen(text) - 1;
   while (end > text && isspace((unsigned char)*end))
   {
      *end = '\0';
      end--;
   }
   return text;
}

static int agent_line_reserve(AgentDriverLine *line, size_t needed)
{
   CHARTYPE *next;
   size_t cap;

   if (line == NULL)
      return 0;
   if (needed + 1 <= line->cap)
      return 1;
   cap = line->cap == 0 ? 32 : line->cap;
   while (cap < needed + 1)
      cap *= 2;
   next = (CHARTYPE *)realloc(line->text, cap);
   if (next == NULL)
      return 0;
   line->text = next;
   line->cap = cap;
   return 1;
}

static int agent_line_set(AgentDriverLine *line, const char *text, size_t len)
{
   if (line == NULL)
      return 0;
   if (!agent_line_reserve(line, len))
      return 0;
   if (len > 0 && text != NULL)
      memcpy(line->text, text, len);
   line->text[len] = '\0';
   line->len = len;
   return 1;
}

static int agent_line_insert(AgentDriverLine *line, size_t offset,
                             const CHARTYPE *text, size_t len)
{
   if (line == NULL || text == NULL)
      return 0;
   if (offset > line->len)
      offset = line->len;
   if (!agent_line_reserve(line, line->len + len))
      return 0;
   memmove(line->text + offset + len, line->text + offset,
           line->len - offset + 1);
   if (len > 0)
      memcpy(line->text + offset, text, len);
   line->len += len;
   return 1;
}

static int agent_line_delete(AgentDriverLine *line, size_t offset, size_t len)
{
   if (line == NULL || offset >= line->len)
      return 0;
   if (offset + len > line->len)
      len = line->len - offset;
   memmove(line->text + offset, line->text + offset + len,
           line->len - offset - len + 1);
   line->len -= len;
   return 1;
}

static void agent_line_free(AgentDriverLine *line)
{
   if (line == NULL)
      return;
   free(line->text);
   line->text = NULL;
   line->len = 0;
   line->cap = 0;
}

static int agent_reserve_lines(AgentDriver *driver, size_t needed)
{
   AgentDriverLine *next;
   size_t cap;

   if (driver == NULL)
      return 0;
   if (needed <= driver->line_cap)
      return 1;
   cap = driver->line_cap == 0 ? 8 : driver->line_cap;
   while (cap < needed)
      cap *= 2;
   next = (AgentDriverLine *)realloc(driver->lines,
                                     cap * sizeof(AgentDriverLine));
   if (next == NULL)
      return 0;
   memset(next + driver->line_cap, 0,
          (cap - driver->line_cap) * sizeof(AgentDriverLine));
   driver->lines = next;
   driver->line_cap = cap;
   return 1;
}

static int agent_append_line(AgentDriver *driver, const char *text, size_t len)
{
   if (driver == NULL)
      return 0;
   if (!agent_reserve_lines(driver, driver->line_count + 1))
      return 0;
   if (!agent_line_set(&driver->lines[driver->line_count], text, len))
      return 0;
   driver->line_count++;
   return 1;
}

static void agent_clear_lines(AgentDriver *driver)
{
   size_t i;

   if (driver == NULL)
      return;
   for (i = 0; i < driver->line_count; i++)
      agent_line_free(&driver->lines[i]);
   driver->line_count = 0;
   driver->top_line = 0;
   driver->cursor_line = 0;
   driver->cursor_cell = 0;
   driver->desired_cell = 0;
}

static AgentDriverLine *agent_current_line(AgentDriver *driver)
{
   if (driver == NULL || driver->line_count == 0)
      return NULL;
   if (driver->cursor_line >= driver->line_count)
      driver->cursor_line = driver->line_count - 1;
   return &driver->lines[driver->cursor_line];
}

static const AgentDriverLine *agent_current_line_const(const AgentDriver *driver)
{
   if (driver == NULL || driver->line_count == 0)
      return NULL;
   if (driver->cursor_line >= driver->line_count)
      return &driver->lines[driver->line_count - 1];
   return &driver->lines[driver->cursor_line];
}

static int agent_line_end_cell(const AgentDriverLine *line)
{
   if (line == NULL)
      return 0;
   return textpos_from_byte(line->text, line->len, line->len).cell_column;
}

static size_t agent_command_len(const AgentDriver *driver)
{
   if (driver == NULL)
      return 0;
   return strlen(driver->command_line);
}

static int agent_command_end_cell(const AgentDriver *driver)
{
   size_t len;

   if (driver == NULL)
      return 0;
   len = agent_command_len(driver);
   return textpos_from_byte((const CHARTYPE *)driver->command_line, len,
                            len).cell_column;
}

static void agent_clamp_command_cursor(AgentDriver *driver)
{
   int end_cell;

   if (driver == NULL)
      return;
   end_cell = agent_command_end_cell(driver);
   if (driver->command_cursor_cell < 0)
      driver->command_cursor_cell = 0;
   if (driver->command_cursor_cell > end_cell)
      driver->command_cursor_cell = end_cell;
}

static void agent_clamp_cursor(AgentDriver *driver)
{
   if (driver == NULL)
      return;
   if (driver->line_count == 0)
      agent_append_line(driver, "", 0);
   if (driver->cursor_line >= driver->line_count)
      driver->cursor_line = driver->line_count - 1;
   if (driver->cursor_cell < 0)
      driver->cursor_cell = 0;
   driver->desired_cell = driver->cursor_cell;
}

static int agent_content_rows_for_top(const AgentDriver *driver, size_t top_line)
{
   int rows;

   if (driver == NULL)
      return 1;
   rows = driver->rows > 1 ? driver->rows - 1 : 1;
   if (top_line == 0 && rows > 1)
      rows--;
   if (rows < 1)
      rows = 1;
   return rows;
}

static void agent_ensure_visible(AgentDriver *driver)
{
   int file_rows;

   if (driver == NULL || driver->line_count == 0)
      return;
   if (driver->cursor_line < driver->top_line)
      driver->top_line = driver->cursor_line;
   file_rows = agent_content_rows_for_top(driver, driver->top_line);
   if (driver->cursor_line >= driver->top_line + (size_t)file_rows)
      driver->top_line = driver->cursor_line - (size_t)file_rows + 1;
   file_rows = agent_content_rows_for_top(driver, driver->top_line);
   if (driver->cursor_line >= driver->top_line + (size_t)file_rows)
      driver->top_line = driver->cursor_line - (size_t)file_rows + 1;
   if (driver->top_line >= driver->line_count)
      driver->top_line = driver->line_count - 1;
}

static int agent_insert_bytes(AgentDriver *driver, const CHARTYPE *text,
                              size_t len)
{
   AgentDriverLine *line;
   TextPos pos;
   int end_cell;

   if (driver == NULL || text == NULL)
      return 0;
   line = agent_current_line(driver);
   if (line == NULL)
      return 0;
   end_cell = agent_line_end_cell(line);
   if (driver->cursor_cell > end_cell)
   {
      size_t fill = (size_t)(driver->cursor_cell - end_cell);
      size_t i;

      if (!agent_line_reserve(line, line->len + fill))
         return 0;
      for (i = 0; i < fill; i++)
         line->text[line->len + i] = ' ';
      line->len += fill;
      line->text[line->len] = '\0';
   }
   pos = textpos_from_cell_virtual(line->text, line->len,
                                   driver->cursor_cell,
                                   TEXT_SNAP_BACKWARD);
   if (!agent_line_insert(line, pos.byte_offset, text, len))
      return 0;
   driver->cursor_cell = textpos_from_byte(line->text, line->len,
                                           pos.byte_offset + len).cell_column;
   driver->desired_cell = driver->cursor_cell;
   driver->dirty = 1;
   agent_set_status(driver, "inserted");
   return 1;
}

static int agent_insert_command_bytes(AgentDriver *driver, const CHARTYPE *text,
                                      size_t len)
{
   size_t command_len;
   size_t offset;
   size_t keep_len;
   TextPos pos;

   if (driver == NULL || text == NULL)
      return 0;
   command_len = agent_command_len(driver);
   if (command_len >= LLM_DRIVER_MAX_COMMAND)
   {
      agent_set_status(driver, "command line full");
      return 0;
   }
   if (len > LLM_DRIVER_MAX_COMMAND - command_len)
      keep_len = LLM_DRIVER_MAX_COMMAND - command_len;
   else
      keep_len = len;
   agent_clamp_command_cursor(driver);
   pos = textpos_from_cell((const CHARTYPE *)driver->command_line,
                           command_len, driver->command_cursor_cell,
                           TEXT_SNAP_BACKWARD);
   offset = pos.byte_offset;
   memmove(driver->command_line + offset + keep_len,
           driver->command_line + offset,
           command_len - offset + 1);
   if (keep_len > 0)
      memcpy(driver->command_line + offset, text, keep_len);
   driver->command_cursor_cell =
      textpos_from_byte((const CHARTYPE *)driver->command_line,
                        agent_command_len(driver),
                        offset + keep_len).cell_column;
   agent_set_status(driver, "command edited");
   return keep_len == len;
}

static int agent_delete_at_cursor(AgentDriver *driver)
{
   AgentDriverLine *line;
   TextPos pos;
   TextCluster cluster;

   line = agent_current_line(driver);
   if (line == NULL)
      return 0;
   pos = textpos_from_cell(line->text, line->len, driver->cursor_cell,
                           TEXT_SNAP_BACKWARD);
   if (pos.byte_offset >= line->len)
   {
      agent_set_status(driver, "nothing to delete");
      return 1;
   }
   cluster = textpos_cluster_at_boundary(line->text, line->len, pos);
   if (cluster.byte_length == 0)
      return 0;
   agent_line_delete(line, pos.byte_offset, cluster.byte_length);
   driver->dirty = 1;
   agent_set_status(driver, "deleted");
   return 1;
}

static int agent_delete_command_at_cursor(AgentDriver *driver)
{
   size_t command_len;
   TextPos pos;
   TextCluster cluster;

   if (driver == NULL)
      return 0;
   command_len = agent_command_len(driver);
   agent_clamp_command_cursor(driver);
   pos = textpos_from_cell((const CHARTYPE *)driver->command_line,
                           command_len, driver->command_cursor_cell,
                           TEXT_SNAP_BACKWARD);
   if (pos.byte_offset >= command_len)
   {
      agent_set_status(driver, "nothing to delete");
      return 1;
   }
   cluster = textpos_cluster_at_boundary((const CHARTYPE *)driver->command_line,
                                         command_len, pos);
   if (cluster.byte_length == 0)
      return 0;
   memmove(driver->command_line + pos.byte_offset,
           driver->command_line + pos.byte_offset + cluster.byte_length,
           command_len - pos.byte_offset - cluster.byte_length + 1);
   agent_set_status(driver, "command edited");
   return 1;
}

static int agent_backspace(AgentDriver *driver)
{
   AgentDriverLine *line;
   TextPos pos;
   TextPos prev;

   line = agent_current_line(driver);
   if (line == NULL)
      return 0;
   if (driver->cursor_cell <= 0)
   {
      agent_set_status(driver, "start of line");
      return 1;
   }
   pos = textpos_from_cell_virtual(line->text, line->len,
                                   driver->cursor_cell,
                                   TEXT_SNAP_BACKWARD);
   if (pos.byte_offset >= line->len
   &&  driver->cursor_cell > agent_line_end_cell(line))
   {
      driver->cursor_cell--;
      driver->desired_cell = driver->cursor_cell;
      agent_set_status(driver, "moved left in virtual space");
      return 1;
   }
   prev = textpos_prev_cluster(line->text, line->len, pos);
   if (pos.byte_offset > prev.byte_offset)
   {
      agent_line_delete(line, prev.byte_offset, pos.byte_offset - prev.byte_offset);
      driver->cursor_cell = prev.cell_column;
      driver->desired_cell = driver->cursor_cell;
      driver->dirty = 1;
      agent_set_status(driver, "backspaced");
      return 1;
   }
   return 0;
}

static int agent_backspace_command(AgentDriver *driver)
{
   size_t command_len;
   TextPos pos;
   TextPos prev;

   if (driver == NULL)
      return 0;
   command_len = agent_command_len(driver);
   agent_clamp_command_cursor(driver);
   if (driver->command_cursor_cell <= 0)
   {
      agent_set_status(driver, "start of command");
      return 1;
   }
   pos = textpos_from_cell((const CHARTYPE *)driver->command_line,
                           command_len, driver->command_cursor_cell,
                           TEXT_SNAP_BACKWARD);
   prev = textpos_prev_cluster((const CHARTYPE *)driver->command_line,
                               command_len, pos);
   if (pos.byte_offset > prev.byte_offset)
   {
      memmove(driver->command_line + prev.byte_offset,
              driver->command_line + pos.byte_offset,
              command_len - pos.byte_offset + 1);
      driver->command_cursor_cell = prev.cell_column;
      agent_set_status(driver, "command edited");
      return 1;
   }
   return 0;
}

static void agent_move_command_horizontal(AgentDriver *driver, int delta)
{
   LogicalCursor cursor;
   size_t len;

   if (driver == NULL)
      return;
   len = agent_command_len(driver);
   agent_clamp_command_cursor(driver);
   cursor = logical_cursor_from_cell(LOGICAL_CURSOR_ZONE_COMMAND, 0, 0,
                                     (const CHARTYPE *)driver->command_line,
                                     len, driver->command_cursor_cell,
                                     TEXT_SNAP_BACKWARD, 0);
   if (delta < 0)
      cursor = logical_cursor_move_left(cursor,
                                        (const CHARTYPE *)driver->command_line,
                                        len, 0);
   else
      cursor = logical_cursor_move_right(cursor,
                                         (const CHARTYPE *)driver->command_line,
                                         len, 0);
   driver->command_cursor_cell = cursor.text.cell_column;
   agent_set_status(driver, "command cursor moved");
}

static void agent_move_horizontal(AgentDriver *driver, int delta)
{
   AgentDriverLine *line;
   LogicalCursor cursor;

   if (driver != NULL && driver->focus_zone == LOGICAL_CURSOR_ZONE_COMMAND)
   {
      agent_move_command_horizontal(driver, delta);
      return;
   }
   line = agent_current_line(driver);
   if (driver == NULL || line == NULL)
      return;
   cursor = logical_cursor_from_cell(LOGICAL_CURSOR_ZONE_FILEAREA,
                                     (LINETYPE)driver->cursor_line + 1,
                                     0, line->text, line->len,
                                     driver->cursor_cell,
                                     TEXT_SNAP_BACKWARD, 1);
   if (delta < 0)
      cursor = logical_cursor_move_left(cursor, line->text, line->len, 1);
   else
      cursor = logical_cursor_move_right(cursor, line->text, line->len, 1);
   driver->cursor_cell = cursor.text.cell_column;
   driver->desired_cell = driver->cursor_cell;
   agent_set_status(driver, "cursor moved");
}

static void agent_move_vertical(AgentDriver *driver, int delta)
{
   AgentDriverLine *line;

   if (driver == NULL || driver->line_count == 0)
      return;
   if (delta < 0)
   {
      size_t amount = (size_t)(-delta);
      if (amount > driver->cursor_line)
         driver->cursor_line = 0;
      else
         driver->cursor_line -= amount;
   }
   else if (delta > 0)
   {
      driver->cursor_line += (size_t)delta;
      if (driver->cursor_line >= driver->line_count)
         driver->cursor_line = driver->line_count - 1;
   }
   line = agent_current_line(driver);
   if (line != NULL)
   {
      TextPos pos = textpos_from_cell_virtual(line->text, line->len,
                                              driver->desired_cell,
                                              TEXT_SNAP_BACKWARD);
      driver->cursor_cell = pos.cell_column;
   }
   agent_ensure_visible(driver);
   agent_set_status(driver, "cursor moved");
}

static int agent_goto_line(AgentDriver *driver, long line_number)
{
   AgentDriverLine *line;

   if (driver == NULL || line_number < 1 || driver->line_count == 0)
      return 0;
   if ((size_t)line_number > driver->line_count)
      line_number = (long)driver->line_count;
   driver->cursor_line = (size_t)(line_number - 1);
   line = agent_current_line(driver);
   if (line != NULL)
   {
      TextPos pos = textpos_from_cell_virtual(line->text, line->len,
                                              driver->desired_cell,
                                              TEXT_SNAP_BACKWARD);
      driver->cursor_cell = pos.cell_column;
   }
   agent_ensure_visible(driver);
   agent_set_status(driver, "cursor moved");
   return 1;
}

static int agent_set_command_line(AgentDriver *driver, const char *command)
{
   if (driver == NULL)
      return 0;
   agent_copy_text(driver->command_line, sizeof(driver->command_line), command);
   driver->command_cursor_cell = agent_command_end_cell(driver);
   return 1;
}

static int agent_apply_command(AgentDriver *driver, const char *command)
{
   char buffer[THE_INPUT_COMMAND_MAX + 1];
   char *text;

   if (driver == NULL || command == NULL)
      return 0;
   agent_copy_text(buffer, sizeof(buffer), command);
   text = agent_trim(buffer);
   if (text == NULL || *text == '\0')
      return 0;

   if (agent_ascii_equal_ci(text, "focus command")
   ||  agent_ascii_equal_ci(text, "focus commandline")
   ||  agent_ascii_equal_ci(text, "focus cmdline"))
   {
      driver->focus_zone = LOGICAL_CURSOR_ZONE_COMMAND;
      agent_clamp_command_cursor(driver);
      agent_set_status(driver, "command focused");
      return 1;
   }
   if (agent_ascii_equal_ci(text, "focus file")
   ||  agent_ascii_equal_ci(text, "focus filearea"))
   {
      driver->focus_zone = LOGICAL_CURSOR_ZONE_FILEAREA;
      agent_set_status(driver, "filearea focused");
      return 1;
   }

   agent_set_command_line(driver, text);

   if (agent_ascii_equal_ci(text, "look"))
   {
      agent_set_status(driver, "ready");
      return 1;
   }
   if (agent_ascii_equal_ci(text, "left"))
   {
      agent_move_horizontal(driver, -1);
      return 1;
   }
   if (agent_ascii_equal_ci(text, "right"))
   {
      agent_move_horizontal(driver, 1);
      return 1;
   }
   if (agent_ascii_equal_ci(text, "up"))
   {
      agent_move_vertical(driver, -1);
      return 1;
   }
   if (agent_ascii_equal_ci(text, "down"))
   {
      agent_move_vertical(driver, 1);
      return 1;
   }
   if (agent_ascii_equal_ci(text, "home"))
   {
      driver->cursor_cell = 0;
      driver->desired_cell = 0;
      agent_set_status(driver, "cursor moved");
      return 1;
   }
   if (agent_ascii_equal_ci(text, "end"))
   {
      driver->cursor_cell = agent_line_end_cell(agent_current_line(driver));
      driver->desired_cell = driver->cursor_cell;
      agent_set_status(driver, "cursor moved");
      return 1;
   }
   if (agent_ascii_equal_ci(text, "top"))
      return agent_goto_line(driver, 1);
   if (agent_ascii_equal_ci(text, "bottom"))
      return agent_goto_line(driver, (long)driver->line_count);
   if (agent_ascii_equal_ci(text, "delete"))
      return agent_delete_at_cursor(driver);
   if (agent_ascii_equal_ci(text, "backspace"))
      return agent_backspace(driver);
   if (agent_ascii_starts_ci(text, "goto "))
      return agent_goto_line(driver, strtol(text + 5, NULL, 10));
   if (agent_ascii_starts_ci(text, "rows "))
   {
      long rows = strtol(text + 5, NULL, 10);
      if (rows > 0 && rows <= UI_DRIVER_MAX_ROWS)
      {
         driver->rows = (int)rows;
         agent_ensure_visible(driver);
         agent_set_status(driver, "rows changed");
         return 1;
      }
      agent_set_status(driver, "invalid rows");
      return 0;
   }
   if (agent_ascii_starts_ci(text, "cols "))
   {
      long cols = strtol(text + 5, NULL, 10);
      if (cols > 0 && cols <= LLM_DRIVER_MAX_COLS)
      {
         driver->cols = (int)cols;
         agent_set_status(driver, "cols changed");
         return 1;
      }
      agent_set_status(driver, "invalid cols");
      return 0;
   }
   if (agent_ascii_starts_ci(text, "insert "))
   {
      if (driver->focus_zone == LOGICAL_CURSOR_ZONE_COMMAND)
         return agent_insert_command_bytes(driver, (const CHARTYPE *)(text + 7),
                                           strlen(text + 7));
      return agent_insert_bytes(driver, (const CHARTYPE *)(text + 7),
                                strlen(text + 7));
   }
   if (agent_ascii_starts_ci(text, "type "))
   {
      if (driver->focus_zone == LOGICAL_CURSOR_ZONE_COMMAND)
         return agent_insert_command_bytes(driver, (const CHARTYPE *)(text + 5),
                                           strlen(text + 5));
      return agent_insert_bytes(driver, (const CHARTYPE *)(text + 5),
                                strlen(text + 5));
   }
   if (agent_ascii_equal_ci(text, "save")
   ||  agent_ascii_equal_ci(text, "write"))
      return agent_driver_save_file(driver, NULL);
   if (agent_ascii_starts_ci(text, "save "))
      return agent_driver_save_file(driver, agent_trim(text + 5));
   if (agent_ascii_starts_ci(text, "write "))
      return agent_driver_save_file(driver, agent_trim(text + 6));

   agent_set_status(driver, "unknown command");
   return 0;
}

void agent_driver_init(AgentDriver *driver, int rows, int cols)
{
   if (driver == NULL)
      return;
   memset(driver, 0, sizeof(*driver));
   driver->rows = rows > 0 ? rows : 24;
   driver->cols = cols > 0 ? cols : 80;
   driver->focus_zone = LOGICAL_CURSOR_ZONE_FILEAREA;
   agent_append_line(driver, "", 0);
   agent_set_status(driver, "ready");
}

void agent_driver_free(AgentDriver *driver)
{
   if (driver == NULL)
      return;
   agent_clear_lines(driver);
   free(driver->lines);
   memset(driver, 0, sizeof(*driver));
}

int agent_driver_set_text(AgentDriver *driver, const char *text)
{
   const char *start;
   const char *ptr;

   if (driver == NULL)
      return 0;
   agent_clear_lines(driver);
   if (text == NULL)
      text = "";
   start = text;
   for (ptr = text; *ptr != '\0'; ptr++)
   {
      if (*ptr == '\n')
      {
         size_t len = (size_t)(ptr - start);
         if (len > 0 && start[len - 1] == '\r')
            len--;
         if (!agent_append_line(driver, start, len))
            return 0;
         start = ptr + 1;
      }
   }
   if (ptr != start || driver->line_count == 0)
   {
      size_t len = (size_t)(ptr - start);
      if (len > 0 && start[len - 1] == '\r')
         len--;
      if (!agent_append_line(driver, start, len))
         return 0;
   }
   agent_clamp_cursor(driver);
   agent_ensure_visible(driver);
   driver->dirty = 0;
   agent_set_status(driver, "buffer loaded");
   return 1;
}

int agent_driver_load_file(AgentDriver *driver, const char *path)
{
   FILE *fp;
   char *buffer = NULL;
   size_t len = 0;
   size_t cap = 0;
   int ch;

   if (driver == NULL || path == NULL)
      return 0;
   fp = fopen(path, "rb");
   if (fp == NULL)
   {
      agent_driver_set_text(driver, "");
      agent_copy_text(driver->path, sizeof(driver->path), path);
      driver->dirty = 0;
      agent_set_status(driver, "new file");
      return 1;
   }
   while ((ch = fgetc(fp)) != EOF)
   {
      if (len + 1 >= cap)
      {
         char *next;
         cap = cap == 0 ? 1024 : cap * 2;
         next = (char *)realloc(buffer, cap);
         if (next == NULL)
         {
            free(buffer);
            fclose(fp);
            return 0;
         }
         buffer = next;
      }
      buffer[len++] = (char)ch;
   }
   fclose(fp);
   if (buffer == NULL)
   {
      buffer = (char *)malloc(1);
      if (buffer == NULL)
         return 0;
   }
   buffer[len] = '\0';
   if (!agent_driver_set_text(driver, buffer))
   {
      free(buffer);
      return 0;
   }
   free(buffer);
   agent_copy_text(driver->path, sizeof(driver->path), path);
   driver->dirty = 0;
   agent_set_status(driver, "file loaded");
   return 1;
}

int agent_driver_save_file(AgentDriver *driver, const char *path)
{
   FILE *fp;
   size_t i;
   const char *target;

   if (driver == NULL)
      return 0;
   target = (path != NULL && *path != '\0') ? path : driver->path;
   if (target == NULL || *target == '\0')
   {
      agent_set_status(driver, "no path");
      return 0;
   }
   fp = fopen(target, "wb");
   if (fp == NULL)
   {
      agent_set_status(driver, "save failed");
      return 0;
   }
   for (i = 0; i < driver->line_count; i++)
   {
      if (driver->lines[i].len > 0)
         fwrite(driver->lines[i].text, 1, driver->lines[i].len, fp);
      if (i + 1 < driver->line_count)
         fputc('\n', fp);
   }
   fclose(fp);
   agent_copy_text(driver->path, sizeof(driver->path), target);
   driver->dirty = 0;
   agent_set_status(driver, "file saved");
   return 1;
}

int agent_driver_screen_view(const AgentDriver *driver,
                             LlmDriverScreenView *view)
{
   UiFrame frame;
   size_t line_index;
   size_t row_index = 0;
   int content_rows;
   int file_start_row = 0;
   int command_row;
   int status_row;
   char prefixes[UI_DRIVER_MAX_ROWS][LLM_DRIVER_MAX_PREFIX + 1];
   LogicalCursor cursor = logical_cursor_invalid();

   if (driver == NULL || view == NULL)
      return 0;
   ui_frame_init(&frame, driver->rows, driver->cols);
   status_row = driver->rows > 0 ? driver->rows - 1 : 0;
   command_row = driver->rows > 1 ? driver->rows - 2 : 0;
   if (command_row < 0)
      command_row = 0;
   content_rows = driver->rows > 2 ? driver->rows - 2 : 1;

   if (driver->top_line == 0 && content_rows > 1)
   {
      const char *tof = "*** Top of File ***";
      ui_frame_set_row(&frame, row_index, UI_ROW_TOF, 0, 0, 0,
                       (const CHARTYPE *)tof, strlen(tof), 0);
      row_index++;
      file_start_row = 1;
   }

   for (line_index = driver->top_line;
        line_index < driver->line_count && row_index < (size_t)content_rows;
        line_index++, row_index++)
   {
      const AgentDriverLine *line = &driver->lines[line_index];
      int row = (int)row_index;

      snprintf(prefixes[row_index], sizeof(prefixes[row_index]), "%6ld",
               (long)line_index + 1);
      ui_frame_set_row(&frame, row_index, UI_ROW_FILE,
                       (LINETYPE)line_index + 1, row, 0,
                       line->text, line->len, 1);
      ui_frame_set_row_prefix(&frame, row_index,
                              (const CHARTYPE *)prefixes[row_index],
                              strlen(prefixes[row_index]), 1);
      if (line_index == driver->cursor_line)
      {
         cursor = logical_cursor_from_cell(LOGICAL_CURSOR_ZONE_FILEAREA,
                                           (LINETYPE)line_index + 1,
                                           row, line->text, line->len,
                                           driver->cursor_cell,
                                           TEXT_SNAP_BACKWARD, 1);
         logical_cursor_set_desired_cell(&cursor, driver->desired_cell);
      }
   }

   if (row_index < (size_t)content_rows)
   {
      const char *eof = "*** Bottom of File ***";
      ui_frame_set_row(&frame, row_index, UI_ROW_EOF, 0, (int)row_index, 0,
                       (const CHARTYPE *)eof, strlen(eof), 0);
      row_index++;
   }

   if (driver->rows > 1 && row_index < UI_DRIVER_MAX_ROWS)
   {
      ui_frame_set_row(&frame, row_index, UI_ROW_COMMAND, 0, command_row, 0,
                       (const CHARTYPE *)driver->command_line,
                       strlen(driver->command_line), 1);
      row_index++;
   }

   if (driver->rows > 2 && row_index < UI_DRIVER_MAX_ROWS)
   {
      ui_frame_set_row(&frame, row_index, UI_ROW_STATUS, 0, status_row, 0,
                       (const CHARTYPE *)driver->status,
                       strlen(driver->status), 0);
   }

   if (driver->focus_zone == LOGICAL_CURSOR_ZONE_COMMAND)
   {
      cursor = logical_cursor_from_cell(LOGICAL_CURSOR_ZONE_COMMAND, 0,
                                        command_row,
                                        (const CHARTYPE *)driver->command_line,
                                        strlen(driver->command_line),
                                        driver->command_cursor_cell,
                                        TEXT_SNAP_BACKWARD, 0);
   }

   if (!cursor.valid)
   {
      size_t visible_offset = driver->cursor_line >= driver->top_line
                            ? driver->cursor_line - driver->top_line
                            : 0;
      const AgentDriverLine *line = agent_current_line_const(driver);
      int row = file_start_row + (int)visible_offset;
      cursor = logical_cursor_from_cell(LOGICAL_CURSOR_ZONE_FILEAREA,
                                        (LINETYPE)driver->cursor_line + 1,
                                        row, line != NULL ? line->text : NULL,
                                        line != NULL ? line->len : 0,
                                        driver->cursor_cell,
                                        TEXT_SNAP_BACKWARD, 1);
   }
   ui_frame_set_cursor(&frame, cursor);
   llm_driver_screen_view_from_frame(&frame, view);
   llm_driver_screen_view_set_command(view, driver->command_line);
   llm_driver_screen_view_set_status(view, driver->status);
   return 1;
}

size_t agent_driver_format(const AgentDriver *driver,
                           const LlmDriverFormatOptions *options,
                           char *out, size_t out_len)
{
   LlmDriverScreenView view;

   if (!agent_driver_screen_view(driver, &view))
   {
      if (out != NULL && out_len > 0)
         out[0] = '\0';
      return 0;
   }
   return llm_driver_format_semantic_view_with_options(&view, options,
                                                       out, out_len);
}

int agent_driver_apply_input(AgentDriver *driver, const TheInputEvent *input)
{
   CHARTYPE bytes[4];
   size_t len;

   if (driver == NULL || input == NULL)
      return 0;
   switch (input->kind)
   {
      case THE_INPUT_TEXT:
         len = text_utf8_encode(input->codepoint, bytes);
         if (len == 0)
            return 0;
         if (driver->focus_zone == LOGICAL_CURSOR_ZONE_COMMAND)
            return agent_insert_command_bytes(driver, bytes, len);
         return agent_insert_bytes(driver, bytes, len);
      case THE_INPUT_KEY:
         switch (input->key_code)
         {
            case KEY_LEFT:
               agent_move_horizontal(driver, -1);
               return 1;
            case KEY_RIGHT:
               agent_move_horizontal(driver, 1);
               return 1;
            case KEY_UP:
               agent_move_vertical(driver, -1);
               return 1;
            case KEY_DOWN:
               agent_move_vertical(driver, 1);
               return 1;
            case KEY_HOME:
               if (driver->focus_zone == LOGICAL_CURSOR_ZONE_COMMAND)
                  driver->command_cursor_cell = 0;
               else
               {
                  driver->cursor_cell = 0;
                  driver->desired_cell = 0;
               }
               agent_set_status(driver, "cursor moved");
               return 1;
            case KEY_END:
               if (driver->focus_zone == LOGICAL_CURSOR_ZONE_COMMAND)
                  driver->command_cursor_cell = agent_command_end_cell(driver);
               else
               {
                  driver->cursor_cell = agent_line_end_cell(agent_current_line(driver));
                  driver->desired_cell = driver->cursor_cell;
               }
               agent_set_status(driver, "cursor moved");
               return 1;
            case KEY_BACKSPACE:
               if (driver->focus_zone == LOGICAL_CURSOR_ZONE_COMMAND)
                  return agent_backspace_command(driver);
               return agent_backspace(driver);
            case KEY_DC:
               if (driver->focus_zone == LOGICAL_CURSOR_ZONE_COMMAND)
                  return agent_delete_command_at_cursor(driver);
               return agent_delete_at_cursor(driver);
            case KEY_ENTER:
            case KEY_RETURN:
            {
               char command[LLM_DRIVER_MAX_COMMAND + 1];
               if (driver->focus_zone != LOGICAL_CURSOR_ZONE_COMMAND)
               {
                  agent_set_status(driver, "enter ignored");
                  return 1;
               }
               agent_copy_text(command, sizeof(command), driver->command_line);
               driver->focus_zone = LOGICAL_CURSOR_ZONE_FILEAREA;
               return agent_apply_command(driver, command);
            }
            case KEY_ESC:
               driver->focus_zone = LOGICAL_CURSOR_ZONE_FILEAREA;
               agent_set_status(driver, "filearea focused");
               return 1;
            case KEY_PPAGE:
               agent_move_vertical(driver, -(driver->rows > 1 ? driver->rows - 1 : 1));
               return 1;
            case KEY_NPAGE:
               agent_move_vertical(driver, driver->rows > 1 ? driver->rows - 1 : 1);
               return 1;
            default:
               agent_set_status(driver, "unsupported key");
               return 0;
         }
      case THE_INPUT_COMMAND:
         return agent_apply_command(driver, input->command);
      case THE_INPUT_LOGICAL_HIT:
         if (input->target.zone == LOGICAL_CURSOR_ZONE_FILEAREA)
         {
            driver->focus_zone = LOGICAL_CURSOR_ZONE_FILEAREA;
            if (input->target.line_number > 0)
               driver->cursor_line = (size_t)input->target.line_number - 1;
            else if (input->target.row >= 0)
               driver->cursor_line = driver->top_line + (size_t)input->target.row;
            if (driver->cursor_line >= driver->line_count)
               driver->cursor_line = driver->line_count - 1;
            driver->cursor_cell = input->target.cell;
            driver->desired_cell = driver->cursor_cell;
            agent_ensure_visible(driver);
            agent_set_status(driver, "cursor moved");
            return 1;
         }
         if (input->target.zone == LOGICAL_CURSOR_ZONE_COMMAND)
         {
            driver->focus_zone = LOGICAL_CURSOR_ZONE_COMMAND;
            driver->command_cursor_cell = input->target.cell;
            agent_clamp_command_cursor(driver);
            agent_set_status(driver, "command focused");
            return 1;
         }
         agent_set_status(driver, "unsupported target");
         return 0;
      case THE_INPUT_DEBUG:
         agent_set_status(driver,
                          the_input_debug_command_name(input->debug_command));
         return 1;
      case THE_INPUT_NONE:
      default:
         return 0;
   }
}

const char *agent_driver_status(const AgentDriver *driver)
{
   if (driver == NULL)
      return "";
   return driver->status;
}
