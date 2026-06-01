#include "agentdriver.h"

#include <ctype.h>
#include <dirent.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "getch.h"

#ifdef KEY_TAB
# define AGENT_KEY_TAB KEY_TAB
#else
# define AGENT_KEY_TAB 0x9
#endif

static void agent_clamp_cursor(AgentDriver *driver);
static void agent_ensure_visible(AgentDriver *driver);

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

static void agent_set_statusf(AgentDriver *driver, const char *fmt, ...)
{
   va_list args;

   if (driver == NULL || fmt == NULL)
      return;
   va_start(args, fmt);
   vsnprintf(driver->status, sizeof(driver->status), fmt, args);
   va_end(args);
   driver->status[sizeof(driver->status) - 1] = '\0';
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
   line->prefix_command[0] = '\0';
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

static int agent_insert_line_at(AgentDriver *driver, size_t index,
                                const char *text, size_t len)
{
   AgentDriverLine line;

   if (driver == NULL)
      return 0;
   memset(&line, 0, sizeof(line));
   if (!agent_line_set(&line, text, len))
      return 0;
   if (index > driver->line_count)
      index = driver->line_count;
   if (!agent_reserve_lines(driver, driver->line_count + 1))
   {
      agent_line_free(&line);
      return 0;
   }
   memmove(&driver->lines[index + 1], &driver->lines[index],
           (driver->line_count - index) * sizeof(driver->lines[0]));
   driver->lines[index] = line;
   driver->line_count++;
   return 1;
}

static int agent_delete_line_at(AgentDriver *driver, size_t index)
{
   if (driver == NULL || driver->line_count == 0 || index >= driver->line_count)
      return 0;
   agent_line_free(&driver->lines[index]);
   if (index + 1 < driver->line_count)
      memmove(&driver->lines[index], &driver->lines[index + 1],
              (driver->line_count - index - 1) * sizeof(driver->lines[0]));
   driver->line_count--;
   memset(&driver->lines[driver->line_count], 0, sizeof(driver->lines[0]));
   if (driver->line_count == 0)
      return agent_append_line(driver, "", 0);
   return 1;
}

static char *agent_serialize_text(const AgentDriver *driver)
{
   char *text;
   size_t len = 0;
   size_t offset = 0;
   size_t i;

   if (driver == NULL)
      return NULL;
   for (i = 0; i < driver->line_count; i++)
      len += driver->lines[i].len + (i + 1 < driver->line_count ? 1 : 0);
   text = (char *)malloc(len + 1);
   if (text == NULL)
      return NULL;
   for (i = 0; i < driver->line_count; i++)
   {
      if (driver->lines[i].len > 0)
      {
         memcpy(text + offset, driver->lines[i].text, driver->lines[i].len);
         offset += driver->lines[i].len;
      }
      if (i + 1 < driver->line_count)
         text[offset++] = '\n';
   }
   text[offset] = '\0';
   return text;
}

static size_t agent_text_line_count(const char *text)
{
   size_t count = 1;

   if (text == NULL || *text == '\0')
      return 1;
   while (*text != '\0')
   {
      if (*text == '\n')
         count++;
      text++;
   }
   return count;
}

static void agent_snapshot_clear(AgentDriverSnapshot *snapshot)
{
   if (snapshot == NULL)
      return;
   free(snapshot->text);
   memset(snapshot, 0, sizeof(*snapshot));
}

static int agent_snapshot_capture(const AgentDriver *driver,
                                  AgentDriverSnapshot *snapshot)
{
   if (driver == NULL || snapshot == NULL)
      return 0;
   memset(snapshot, 0, sizeof(*snapshot));
   snapshot->text = agent_serialize_text(driver);
   if (snapshot->text == NULL)
      return 0;
   agent_copy_text(snapshot->path, sizeof(snapshot->path), driver->path);
   snapshot->dirty = driver->dirty;
   snapshot->cursor_line = driver->cursor_line;
   snapshot->top_line = driver->top_line;
   snapshot->cursor_cell = driver->cursor_cell;
   snapshot->desired_cell = driver->desired_cell;
   agent_copy_text(snapshot->search_text, sizeof(snapshot->search_text),
                   driver->search_text);
   return 1;
}

static int agent_snapshot_restore(AgentDriver *driver,
                                  const AgentDriverSnapshot *snapshot)
{
   if (driver == NULL || snapshot == NULL || snapshot->text == NULL)
      return 0;
   if (!agent_driver_set_text(driver, snapshot->text))
      return 0;
   agent_copy_text(driver->path, sizeof(driver->path), snapshot->path);
   driver->dirty = snapshot->dirty;
   driver->cursor_line = snapshot->cursor_line;
   driver->top_line = snapshot->top_line;
   driver->cursor_cell = snapshot->cursor_cell;
   driver->desired_cell = snapshot->desired_cell;
   agent_copy_text(driver->search_text, sizeof(driver->search_text),
                   snapshot->search_text);
   agent_clamp_cursor(driver);
   agent_ensure_visible(driver);
   return 1;
}

static void agent_snapshot_array_clear(AgentDriverSnapshot *snapshots,
                                       size_t *count)
{
   size_t i;

   if (snapshots == NULL || count == NULL)
      return;
   for (i = 0; i < *count; i++)
      agent_snapshot_clear(&snapshots[i]);
   *count = 0;
}

static int agent_snapshot_array_push(AgentDriverSnapshot *snapshots,
                                     size_t *count,
                                     AgentDriverSnapshot snapshot)
{
   if (snapshots == NULL || count == NULL)
   {
      agent_snapshot_clear(&snapshot);
      return 0;
   }
   if (*count >= AGENT_DRIVER_HISTORY_MAX)
   {
      agent_snapshot_clear(&snapshots[0]);
      memmove(&snapshots[0], &snapshots[1],
              (AGENT_DRIVER_HISTORY_MAX - 1) * sizeof(snapshots[0]));
      *count = AGENT_DRIVER_HISTORY_MAX - 1;
   }
   snapshots[*count] = snapshot;
   (*count)++;
   return 1;
}

static int agent_content_matches_snapshot(const AgentDriver *driver,
                                          const AgentDriverSnapshot *snapshot)
{
   char *current;
   int matches;

   if (driver == NULL || snapshot == NULL || snapshot->text == NULL)
      return 0;
   current = agent_serialize_text(driver);
   if (current == NULL)
      return 0;
   matches = strcmp(current, snapshot->text) == 0;
   free(current);
   return matches;
}

static void agent_clear_redo(AgentDriver *driver)
{
   if (driver != NULL)
      agent_snapshot_array_clear(driver->redo, &driver->redo_count);
}

static int agent_buffer_sync_current(AgentDriver *driver)
{
   AgentDriverSnapshot snapshot;

   if (driver == NULL || driver->current_buffer >= AGENT_DRIVER_BUFFER_MAX)
      return 0;
   if (!agent_snapshot_capture(driver, &snapshot))
      return 0;
   agent_snapshot_clear(&driver->buffers[driver->current_buffer].snapshot);
   driver->buffers[driver->current_buffer].snapshot = snapshot;
   driver->buffers[driver->current_buffer].used = 1;
   if (driver->buffer_count <= driver->current_buffer)
      driver->buffer_count = driver->current_buffer + 1;
   return 1;
}

static int agent_buffer_restore_current(AgentDriver *driver, size_t index)
{
   if (driver == NULL || index >= driver->buffer_count
   ||  !driver->buffers[index].used)
      return 0;
   if (!agent_snapshot_restore(driver, &driver->buffers[index].snapshot))
      return 0;
   driver->current_buffer = index;
   driver->focus_zone = LOGICAL_CURSOR_ZONE_FILEAREA;
   driver->command_line[0] = '\0';
   driver->command_cursor_cell = 0;
   driver->selection_active = 0;
   agent_snapshot_array_clear(driver->undo, &driver->undo_count);
   agent_snapshot_array_clear(driver->redo, &driver->redo_count);
   return 1;
}

static int agent_buffer_find_path(const AgentDriver *driver, const char *path,
                                  size_t *index)
{
   size_t i;

   if (index != NULL)
      *index = 0;
   if (driver == NULL || path == NULL || *path == '\0')
      return 0;
   for (i = 0; i < driver->buffer_count; i++)
   {
      const AgentDriverSnapshot *snapshot = &driver->buffers[i].snapshot;

      if (driver->buffers[i].used && strcmp(snapshot->path, path) == 0)
      {
         if (index != NULL)
            *index = i;
         return 1;
      }
   }
   return 0;
}

static int agent_buffer_open(AgentDriver *driver, const char *path)
{
   size_t index;

   if (driver == NULL || path == NULL || *path == '\0')
   {
      agent_set_status(driver, "no path");
      return 0;
   }
   if (!agent_buffer_sync_current(driver))
      return 0;
   if (agent_buffer_find_path(driver, path, &index))
   {
      if (!agent_buffer_restore_current(driver, index))
         return 0;
      agent_set_status(driver, "buffer switched");
      return 1;
   }
   if (driver->buffer_count >= AGENT_DRIVER_BUFFER_MAX)
   {
      agent_set_status(driver, "buffer list full");
      return 0;
   }
   driver->current_buffer = driver->buffer_count++;
   memset(&driver->buffers[driver->current_buffer], 0,
          sizeof(driver->buffers[driver->current_buffer]));
   driver->buffers[driver->current_buffer].used = 1;
   if (!agent_driver_load_file(driver, path))
      return 0;
   agent_snapshot_array_clear(driver->undo, &driver->undo_count);
   agent_snapshot_array_clear(driver->redo, &driver->redo_count);
   return agent_buffer_sync_current(driver);
}

static int agent_parse_buffer_index(const AgentDriver *driver,
                                    const char *text, size_t *index)
{
   char *end = NULL;
   long parsed;

   if (index != NULL)
      *index = 0;
   if (driver == NULL || text == NULL || *text == '\0')
      return 0;
   parsed = strtol(text, &end, 10);
   if (end != text && end != NULL && *end == '\0')
   {
      if (parsed < 0 || (size_t)parsed >= driver->buffer_count)
         return 0;
      if (index != NULL)
         *index = (size_t)parsed;
      return 1;
   }
   return agent_buffer_find_path(driver, text, index);
}

static int agent_buffer_switch(AgentDriver *driver, const char *target)
{
   size_t index;

   if (driver == NULL || target == NULL || *target == '\0')
   {
      agent_set_status(driver, "missing buffer");
      return 0;
   }
   if (!agent_buffer_sync_current(driver))
      return 0;
   if (!agent_parse_buffer_index(driver, target, &index))
   {
      agent_set_status(driver, "buffer not found");
      return 0;
   }
   if (!agent_buffer_restore_current(driver, index))
      return 0;
   agent_set_status(driver, "buffer switched");
   return 1;
}

static int agent_buffer_close(AgentDriver *driver, const char *target,
                              int force)
{
   size_t index;
   size_t next;

   if (driver == NULL)
      return 0;
   if (!agent_buffer_sync_current(driver))
      return 0;
   if (target == NULL || *target == '\0')
      index = driver->current_buffer;
   else if (!agent_parse_buffer_index(driver, target, &index))
   {
      agent_set_status(driver, "buffer not found");
      return 0;
   }
   if (index >= driver->buffer_count || !driver->buffers[index].used)
   {
      agent_set_status(driver, "buffer not found");
      return 0;
   }
   if (driver->buffers[index].snapshot.dirty && !force)
   {
      agent_set_status(driver, "unsaved changes");
      return 0;
   }
   agent_snapshot_clear(&driver->buffers[index].snapshot);
   if (index + 1 < driver->buffer_count)
      memmove(&driver->buffers[index], &driver->buffers[index + 1],
              (driver->buffer_count - index - 1) * sizeof(driver->buffers[0]));
   driver->buffer_count--;
   memset(&driver->buffers[driver->buffer_count], 0,
          sizeof(driver->buffers[driver->buffer_count]));
   if (driver->buffer_count == 0)
   {
      driver->buffer_count = 1;
      driver->current_buffer = 0;
      agent_driver_set_text(driver, "");
      driver->path[0] = '\0';
      driver->dirty = 0;
      agent_buffer_sync_current(driver);
      agent_set_status(driver, "buffer closed");
      return 1;
   }
   if (driver->current_buffer > index)
      driver->current_buffer--;
   if (driver->current_buffer >= driver->buffer_count)
      driver->current_buffer = driver->buffer_count - 1;
   next = driver->current_buffer;
   if (index == next && next >= driver->buffer_count)
      next = driver->buffer_count - 1;
   if (!agent_buffer_restore_current(driver, next))
      return 0;
   agent_set_status(driver, "buffer closed");
   return 1;
}

static int agent_project_compare(const void *left, const void *right)
{
   const char *const l = (const char *const)left;
   const char *const r = (const char *const)right;

   return strcmp(l, r);
}

static int agent_project_list(AgentDriver *driver, const char *root)
{
   DIR *dir;
   struct dirent *entry;

   if (driver == NULL)
      return 0;
   if (root == NULL || *root == '\0')
      root = ".";
   dir = opendir(root);
   if (dir == NULL)
   {
      agent_set_status(driver, "project list failed");
      return 0;
   }
   driver->project_file_count = 0;
   agent_copy_text(driver->project_root, sizeof(driver->project_root), root);
   while ((entry = readdir(dir)) != NULL
   &&     driver->project_file_count < AGENT_DRIVER_PROJECT_MAX)
   {
      if (entry->d_name[0] == '.')
         continue;
      agent_copy_text(driver->project_files[driver->project_file_count],
                      sizeof(driver->project_files[driver->project_file_count]),
                      entry->d_name);
      driver->project_file_count++;
   }
   closedir(dir);
   qsort(driver->project_files, driver->project_file_count,
         sizeof(driver->project_files[0]), agent_project_compare);
   agent_set_statusf(driver, "project files %zu", driver->project_file_count);
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

static int agent_visible_file_rows(const AgentDriver *driver)
{
   int rows;

   if (driver == NULL)
      return 1;
   rows = driver->rows > 2 ? driver->rows - 2 : 1;
   if (driver->top_line == 0 && rows > 1)
      rows--;
   if (rows < 1)
      rows = 1;
   return rows;
}

static size_t agent_last_visible_file_line(const AgentDriver *driver)
{
   size_t last;
   int rows;

   if (driver == NULL || driver->line_count == 0)
      return 0;
   rows = agent_visible_file_rows(driver);
   last = driver->top_line + (size_t)rows - 1;
   if (last >= driver->line_count)
      last = driver->line_count - 1;
   return last;
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

static int agent_insert_prefix_bytes(AgentDriver *driver, const CHARTYPE *text,
                                     size_t len)
{
   AgentDriverLine *line;
   size_t prefix_len;
   size_t offset;
   size_t keep_len;
   TextPos pos;

   if (driver == NULL || text == NULL)
      return 0;
   line = agent_current_line(driver);
   if (line == NULL)
      return 0;
   prefix_len = strlen(line->prefix_command);
   if (prefix_len >= LLM_DRIVER_MAX_PREFIX)
   {
      agent_set_status(driver, "prefix full");
      return 0;
   }
   keep_len = len;
   if (keep_len > LLM_DRIVER_MAX_PREFIX - prefix_len)
      keep_len = LLM_DRIVER_MAX_PREFIX - prefix_len;
   pos = textpos_from_cell((const CHARTYPE *)line->prefix_command,
                           prefix_len, driver->cursor_cell,
                           TEXT_SNAP_BACKWARD);
   offset = pos.byte_offset;
   memmove(line->prefix_command + offset + keep_len,
           line->prefix_command + offset,
           prefix_len - offset + 1);
   if (keep_len > 0)
      memcpy(line->prefix_command + offset, text, keep_len);
   driver->cursor_cell =
      textpos_from_byte((const CHARTYPE *)line->prefix_command,
                        strlen(line->prefix_command),
                        offset + keep_len).cell_column;
   driver->desired_cell = driver->cursor_cell;
   agent_set_status(driver, "prefix edited");
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

static int agent_delete_prefix_at_cursor(AgentDriver *driver)
{
   AgentDriverLine *line;
   size_t prefix_len;
   TextPos pos;
   TextCluster cluster;

   if (driver == NULL)
      return 0;
   line = agent_current_line(driver);
   if (line == NULL)
      return 0;
   prefix_len = strlen(line->prefix_command);
   pos = textpos_from_cell((const CHARTYPE *)line->prefix_command,
                           prefix_len, driver->cursor_cell,
                           TEXT_SNAP_BACKWARD);
   if (pos.byte_offset >= prefix_len)
   {
      agent_set_status(driver, "nothing to delete");
      return 1;
   }
   cluster = textpos_cluster_at_boundary((const CHARTYPE *)line->prefix_command,
                                         prefix_len, pos);
   if (cluster.byte_length == 0)
      return 0;
   memmove(line->prefix_command + pos.byte_offset,
           line->prefix_command + pos.byte_offset + cluster.byte_length,
           prefix_len - pos.byte_offset - cluster.byte_length + 1);
   agent_set_status(driver, "prefix edited");
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

static int agent_backspace_prefix(AgentDriver *driver)
{
   AgentDriverLine *line;
   size_t prefix_len;
   TextPos pos;
   TextPos prev;

   if (driver == NULL)
      return 0;
   line = agent_current_line(driver);
   if (line == NULL)
      return 0;
   if (driver->cursor_cell <= 0)
   {
      agent_set_status(driver, "start of prefix");
      return 1;
   }
   prefix_len = strlen(line->prefix_command);
   pos = textpos_from_cell((const CHARTYPE *)line->prefix_command,
                           prefix_len, driver->cursor_cell,
                           TEXT_SNAP_BACKWARD);
   prev = textpos_prev_cluster((const CHARTYPE *)line->prefix_command,
                               prefix_len, pos);
   if (pos.byte_offset > prev.byte_offset)
   {
      memmove(line->prefix_command + prev.byte_offset,
              line->prefix_command + pos.byte_offset,
              prefix_len - pos.byte_offset + 1);
      driver->cursor_cell = prev.cell_column;
      driver->desired_cell = driver->cursor_cell;
      agent_set_status(driver, "prefix edited");
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

static int agent_delete_to_end(AgentDriver *driver)
{
   AgentDriverLine *line;
   TextPos pos;

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
   agent_line_delete(line, pos.byte_offset, line->len - pos.byte_offset);
   driver->dirty = 1;
   agent_set_status(driver, "deleted to end");
   return 1;
}

static int agent_delete_command_to_end(AgentDriver *driver)
{
   size_t command_len;
   TextPos pos;

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
   driver->command_line[pos.byte_offset] = '\0';
   agent_set_status(driver, "command edited");
   return 1;
}

static int agent_first_nonblank_cell(const CHARTYPE *text, size_t len)
{
   size_t offset = 0;

   while (offset < len && (text[offset] == ' ' || text[offset] == '\t'))
      offset++;
   return textpos_from_byte(text, len, offset).cell_column;
}

static void agent_move_to_first_nonblank(AgentDriver *driver)
{
   AgentDriverLine *line;

   if (driver == NULL)
      return;
   if (driver->focus_zone == LOGICAL_CURSOR_ZONE_COMMAND)
   {
      driver->command_cursor_cell =
         agent_first_nonblank_cell((const CHARTYPE *)driver->command_line,
                                   agent_command_len(driver));
      agent_set_status(driver, "command cursor moved");
      return;
   }
   if (driver->focus_zone == LOGICAL_CURSOR_ZONE_PREFIX)
   {
      driver->cursor_cell = 0;
      driver->desired_cell = 0;
      agent_set_status(driver, "prefix cursor moved");
      return;
   }
   line = agent_current_line(driver);
   if (line != NULL)
   {
      driver->cursor_cell = agent_first_nonblank_cell(line->text, line->len);
      driver->desired_cell = driver->cursor_cell;
   }
   agent_set_status(driver, "cursor moved");
}

static int agent_prefix_end_cell(const AgentDriver *driver)
{
   char prefix[LLM_DRIVER_MAX_PREFIX + 1];
   const AgentDriverLine *line;
   size_t len;

   if (driver == NULL)
      return 0;
   line = agent_current_line_const(driver);
   if (line != NULL && line->prefix_command[0] != '\0')
      agent_copy_text(prefix, sizeof(prefix), line->prefix_command);
   else
      snprintf(prefix, sizeof(prefix), "%6ld", (long)driver->cursor_line + 1);
   len = strlen(prefix);
   return textpos_from_byte((const CHARTYPE *)prefix, len, len).cell_column;
}

static int agent_prefix_last_cell(const AgentDriver *driver)
{
   int end_cell = agent_prefix_end_cell(driver);

   return end_cell > 0 ? end_cell - 1 : 0;
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

static void agent_move_prefix_horizontal(AgentDriver *driver, int delta)
{
   int end_cell;

   if (driver == NULL)
      return;
   end_cell = agent_prefix_last_cell(driver);
   driver->cursor_cell += delta;
   if (driver->cursor_cell < 0)
      driver->cursor_cell = 0;
   if (driver->cursor_cell > end_cell)
      driver->cursor_cell = end_cell;
   driver->desired_cell = driver->cursor_cell;
   agent_set_status(driver, "prefix cursor moved");
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
   if (driver != NULL && driver->focus_zone == LOGICAL_CURSOR_ZONE_PREFIX)
   {
      agent_move_prefix_horizontal(driver, delta);
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

static int agent_cluster_is_space(const CHARTYPE *line, size_t len,
                                  TextPos pos)
{
   TextCluster cluster = textpos_cluster_at_boundary(line, len, pos);

   return cluster.byte_length == 1 && line[cluster.pos.byte_offset] == ' ';
}

static int agent_word_delete_range(const CHARTYPE *line, size_t len,
                                   int cell, size_t *start,
                                   size_t *delete_len, int *target_cell)
{
   TextPos first;
   TextPos end;
   TextCluster cluster;
   int current_is_space;

   if (start != NULL)
      *start = 0;
   if (delete_len != NULL)
      *delete_len = 0;
   if (target_cell != NULL)
      *target_cell = 0;
   if (line == NULL || len == 0)
      return 0;

   first = textpos_from_cell(line, len, cell, TEXT_SNAP_BACKWARD);
   if (first.byte_offset >= len)
      return 0;
   cluster = textpos_cluster_at_boundary(line, len, first);
   if (cluster.byte_length == 0)
      return 0;

   current_is_space = agent_cluster_is_space(line, len, first);
   if (current_is_space)
   {
      while (first.byte_offset > 0)
      {
         TextPos prev = textpos_prev_cluster(line, len, first);

         if (!agent_cluster_is_space(line, len, prev))
            break;
         first = prev;
      }
      end = cluster.end;
      while (end.byte_offset < len)
      {
         TextCluster next = textpos_cluster_at_boundary(line, len, end);

         if (next.byte_length == 0
         ||  !agent_cluster_is_space(line, len, next.pos))
            break;
         end = next.end;
      }
   }
   else
   {
      while (first.byte_offset > 0)
      {
         TextPos prev = textpos_prev_cluster(line, len, first);

         if (agent_cluster_is_space(line, len, prev))
            break;
         first = prev;
      }
      end = cluster.end;
      while (end.byte_offset < len)
      {
         TextCluster next = textpos_cluster_at_boundary(line, len, end);

         if (next.byte_length == 0
         ||  agent_cluster_is_space(line, len, next.pos))
            break;
         end = next.end;
      }
      while (end.byte_offset < len)
      {
         TextCluster next = textpos_cluster_at_boundary(line, len, end);

         if (next.byte_length == 0
         ||  !agent_cluster_is_space(line, len, next.pos))
            break;
         end = next.end;
      }
   }

   if (end.byte_offset <= first.byte_offset)
      return 0;
   if (start != NULL)
      *start = first.byte_offset;
   if (delete_len != NULL)
      *delete_len = end.byte_offset - first.byte_offset;
   if (target_cell != NULL)
      *target_cell = first.cell_column;
   return 1;
}

static int agent_delete_word(AgentDriver *driver)
{
   AgentDriverLine *line;
   size_t start;
   size_t delete_len;
   int target_cell;

   line = agent_current_line(driver);
   if (line == NULL)
      return 0;
   if (!agent_word_delete_range(line->text, line->len, driver->cursor_cell,
                                &start, &delete_len, &target_cell))
   {
      agent_set_status(driver, "nothing to delete");
      return 1;
   }
   agent_line_delete(line, start, delete_len);
   driver->cursor_cell = target_cell;
   driver->desired_cell = driver->cursor_cell;
   driver->dirty = 1;
   agent_set_status(driver, "deleted word");
   return 1;
}

static int agent_delete_command_word(AgentDriver *driver)
{
   size_t command_len;
   size_t start;
   size_t delete_len;
   int target_cell;

   if (driver == NULL)
      return 0;
   command_len = agent_command_len(driver);
   agent_clamp_command_cursor(driver);
   if (!agent_word_delete_range((const CHARTYPE *)driver->command_line,
                                command_len, driver->command_cursor_cell,
                                &start, &delete_len, &target_cell))
   {
      agent_set_status(driver, "nothing to delete");
      return 1;
   }
   memmove(driver->command_line + start,
           driver->command_line + start + delete_len,
           command_len - start - delete_len + 1);
   driver->command_cursor_cell = target_cell;
   agent_set_status(driver, "command edited");
   return 1;
}

static int agent_delete_prefix_word(AgentDriver *driver)
{
   AgentDriverLine *line;
   size_t prefix_len;
   size_t start;
   size_t delete_len;
   int target_cell;

   if (driver == NULL)
      return 0;
   line = agent_current_line(driver);
   if (line == NULL)
      return 0;
   prefix_len = strlen(line->prefix_command);
   if (!agent_word_delete_range((const CHARTYPE *)line->prefix_command,
                                prefix_len, driver->cursor_cell,
                                &start, &delete_len, &target_cell))
   {
      agent_set_status(driver, "nothing to delete");
      return 1;
   }
   memmove(line->prefix_command + start,
           line->prefix_command + start + delete_len,
           prefix_len - start - delete_len + 1);
   driver->cursor_cell = target_cell;
   driver->desired_cell = driver->cursor_cell;
   agent_set_status(driver, "prefix edited");
   return 1;
}

static int agent_find_forward_match(const AgentDriver *driver,
                                    const char *needle,
                                    int continue_after_current,
                                    size_t *match_line,
                                    size_t *match_offset)
{
   size_t needle_len;
   size_t line_index;
   size_t scanned;

   if (match_line != NULL)
      *match_line = 0;
   if (match_offset != NULL)
      *match_offset = 0;
   if (driver == NULL || needle == NULL || *needle == '\0'
   ||  driver->line_count == 0)
      return 0;

   needle_len = strlen(needle);
   for (scanned = 0; scanned < driver->line_count; scanned++)
   {
      const AgentDriverLine *line;
      size_t start = 0;
      const char *found;

      line_index = (driver->cursor_line + scanned) % driver->line_count;
      line = &driver->lines[line_index];
      if (line_index == driver->cursor_line)
      {
         TextPos pos = textpos_from_cell(line->text, line->len,
                                         driver->cursor_cell,
                                         TEXT_SNAP_BACKWARD);
         start = pos.byte_offset;
         if (continue_after_current && start < line->len)
         {
            TextCluster cluster =
               textpos_cluster_at_boundary(line->text, line->len, pos);

            if (cluster.byte_length > 0)
               start = cluster.end.byte_offset;
         }
      }
      if (start > line->len)
         start = line->len;
      if (needle_len > line->len - start)
         continue;
      found = strstr((const char *)line->text + start, needle);
      if (found == NULL)
         continue;
      if (match_line != NULL)
         *match_line = line_index;
      if (match_offset != NULL)
         *match_offset = (size_t)(found - (const char *)line->text);
      return 1;
   }
   return 0;
}

static int agent_find_last_before(const AgentDriverLine *line,
                                  const char *needle, size_t limit,
                                  size_t *match_offset)
{
   const char *found;
   const char *next;
   size_t needle_len;

   if (match_offset != NULL)
      *match_offset = 0;
   if (line == NULL || needle == NULL || *needle == '\0')
      return 0;
   needle_len = strlen(needle);
   if (needle_len == 0 || limit < needle_len)
      return 0;
   if (limit > line->len)
      limit = line->len;

   found = NULL;
   next = strstr((const char *)line->text, needle);
   while (next != NULL
   &&     (size_t)(next - (const char *)line->text) + needle_len <= limit)
   {
      found = next;
      next = strstr(next + 1, needle);
   }
   if (found == NULL)
      return 0;
   if (match_offset != NULL)
      *match_offset = (size_t)(found - (const char *)line->text);
   return 1;
}

static int agent_find_backward_match(const AgentDriver *driver,
                                     const char *needle,
                                     size_t *match_line,
                                     size_t *match_offset)
{
   size_t scanned;

   if (match_line != NULL)
      *match_line = 0;
   if (match_offset != NULL)
      *match_offset = 0;
   if (driver == NULL || needle == NULL || *needle == '\0'
   ||  driver->line_count == 0)
      return 0;

   for (scanned = 0; scanned < driver->line_count; scanned++)
   {
      size_t line_index =
         (driver->cursor_line + driver->line_count - scanned)
       % driver->line_count;
      const AgentDriverLine *line = &driver->lines[line_index];
      size_t limit = line->len;
      size_t offset;

      if (line_index == driver->cursor_line)
      {
         TextPos pos = textpos_from_cell(line->text, line->len,
                                         driver->cursor_cell,
                                         TEXT_SNAP_BACKWARD);
         limit = pos.byte_offset;
      }
      if (!agent_find_last_before(line, needle, limit, &offset))
         continue;
      if (match_line != NULL)
         *match_line = line_index;
      if (match_offset != NULL)
         *match_offset = offset;
      return 1;
   }
   return 0;
}

static void agent_move_to_match(AgentDriver *driver, size_t line_index,
                                size_t byte_offset)
{
   AgentDriverLine *line;

   if (driver == NULL || driver->line_count == 0)
      return;
   if (line_index >= driver->line_count)
      line_index = driver->line_count - 1;
   line = &driver->lines[line_index];
   driver->focus_zone = LOGICAL_CURSOR_ZONE_FILEAREA;
   driver->cursor_line = line_index;
   driver->cursor_cell =
      textpos_from_byte(line->text, line->len, byte_offset).cell_column;
   driver->desired_cell = driver->cursor_cell;
   agent_ensure_visible(driver);
}

static int agent_find_text(AgentDriver *driver, const char *needle,
                           int continue_after_current, int backward)
{
   size_t line_index;
   size_t offset;

   if (driver == NULL || needle == NULL || *needle == '\0')
   {
      agent_set_status(driver, "empty search");
      return 0;
   }
   if (needle != driver->search_text)
      agent_copy_text(driver->search_text, sizeof(driver->search_text), needle);
   if (backward)
   {
      if (!agent_find_backward_match(driver, driver->search_text,
                                     &line_index, &offset))
      {
         agent_set_status(driver, "not found");
         return 0;
      }
   }
   else if (!agent_find_forward_match(driver, driver->search_text,
                                      continue_after_current,
                                      &line_index, &offset))
   {
      agent_set_status(driver, "not found");
      return 0;
   }
   agent_move_to_match(driver, line_index, offset);
   agent_set_status(driver, "found");
   return 1;
}

static int agent_replace_next(AgentDriver *driver, const char *needle,
                              const char *replacement)
{
   AgentDriverLine *line;
   size_t line_index;
   size_t offset;
   size_t needle_len;
   size_t replacement_len;

   if (driver == NULL || needle == NULL || *needle == '\0'
   ||  replacement == NULL)
   {
      agent_set_status(driver, "invalid replace");
      return 0;
   }
   if (!agent_find_forward_match(driver, needle, 0, &line_index, &offset))
   {
      agent_set_status(driver, "not found");
      return 0;
   }
   needle_len = strlen(needle);
   replacement_len = strlen(replacement);
   line = &driver->lines[line_index];
   if (!agent_line_delete(line, offset, needle_len))
      return 0;
   if (replacement_len > 0
   &&  !agent_line_insert(line, offset, (const CHARTYPE *)replacement,
                          replacement_len))
      return 0;
   agent_move_to_match(driver, line_index, offset + replacement_len);
   agent_copy_text(driver->search_text, sizeof(driver->search_text), needle);
   driver->dirty = 1;
   agent_set_status(driver, "replaced");
   return 1;
}

static int agent_replace_all(AgentDriver *driver, const char *needle,
                             const char *replacement)
{
   size_t line_index;
   size_t needle_len;
   size_t replacement_len;
   long count = 0;

   if (driver == NULL || needle == NULL || *needle == '\0'
   ||  replacement == NULL)
   {
      agent_set_status(driver, "invalid replace");
      return 0;
   }
   needle_len = strlen(needle);
   replacement_len = strlen(replacement);
   for (line_index = 0; line_index < driver->line_count; line_index++)
   {
      AgentDriverLine *line = &driver->lines[line_index];
      char *found = strstr((char *)line->text, needle);

      while (found != NULL)
      {
         size_t offset = (size_t)(found - (char *)line->text);

         if (!agent_line_delete(line, offset, needle_len))
            return 0;
         if (replacement_len > 0
         &&  !agent_line_insert(line, offset, (const CHARTYPE *)replacement,
                                replacement_len))
            return 0;
         count++;
         found = strstr((char *)line->text + offset + replacement_len,
                        needle);
      }
   }
   if (count == 0)
   {
      agent_set_status(driver, "not found");
      return 0;
   }
   agent_copy_text(driver->search_text, sizeof(driver->search_text), needle);
   driver->dirty = 1;
   agent_set_statusf(driver, "replaced %ld", count);
   return 1;
}

static int agent_parse_replace_args(char *text, char **needle,
                                    char **replacement)
{
   char *split;
   char delimiter;

   if (needle != NULL)
      *needle = NULL;
   if (replacement != NULL)
      *replacement = NULL;
   text = agent_trim(text);
   if (text == NULL || *text == '\0')
      return 0;

   delimiter = *text;
   if (!isalnum((unsigned char)delimiter)
   &&  !isspace((unsigned char)delimiter))
   {
      char *end;

      *needle = text + 1;
      split = strchr(*needle, delimiter);
      if (split == NULL)
         return 0;
      *split = '\0';
      *replacement = split + 1;
      end = strrchr(*replacement, delimiter);
      if (end != NULL)
         *end = '\0';
      return **needle != '\0';
   }

   split = text;
   while (*split != '\0' && !isspace((unsigned char)*split))
      split++;
   if (*split == '\0')
      return 0;
   *split = '\0';
   split++;
   *needle = text;
   *replacement = agent_trim(split);
   return **needle != '\0' && *replacement != NULL;
}

static const char *agent_payload_after(const char *text, const char *command)
{
   size_t len;

   if (text == NULL || command == NULL)
      return NULL;
   len = strlen(command);
   if (!agent_ascii_starts_ci(text, command))
      return NULL;
   if (text[len] == '\0')
      return "";
   if (!isspace((unsigned char)text[len]))
      return NULL;
   return text + len + 1;
}

static int agent_target_line_index(const AgentDriver *driver,
                                   const TheInputLogicalTarget *target,
                                   size_t *line_index)
{
   size_t index;
   int content_rows;
   int file_start_row;
   int row;

   if (line_index != NULL)
      *line_index = 0;
   if (driver == NULL || target == NULL || driver->line_count == 0)
      return 0;
   if (target->line_number > 0)
   {
      if ((size_t)target->line_number > driver->line_count)
         index = driver->line_count - 1;
      else
         index = (size_t)target->line_number - 1;
   }
   else
   {
      row = target->row;
      if (row < 0)
         return 0;
      content_rows = driver->rows > 2 ? driver->rows - 2 : 1;
      file_start_row = (driver->top_line == 0 && content_rows > 1) ? 1 : 0;
      if (row < file_start_row)
         row = file_start_row;
      index = driver->top_line + (size_t)(row - file_start_row);
      if (index >= driver->line_count)
         index = driver->line_count - 1;
   }
   if (line_index != NULL)
      *line_index = index;
   return 1;
}

static int agent_focus_target_line(AgentDriver *driver,
                                   const TheInputLogicalTarget *target)
{
   size_t line_index;

   if (!agent_target_line_index(driver, target, &line_index))
      return 0;
   driver->cursor_line = line_index;
   agent_ensure_visible(driver);
   return 1;
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

static void agent_focus_filearea_at(AgentDriver *driver, size_t line_index,
                                    int cell)
{
   if (driver == NULL)
      return;
   if (driver->line_count == 0)
      agent_append_line(driver, "", 0);
   if (line_index >= driver->line_count)
      line_index = driver->line_count - 1;
   if (cell < 0)
      cell = 0;
   driver->focus_zone = LOGICAL_CURSOR_ZONE_FILEAREA;
   driver->cursor_line = line_index;
   driver->cursor_cell = cell;
   driver->desired_cell = driver->cursor_cell;
   agent_ensure_visible(driver);
}

static void agent_focus_prefix_at(AgentDriver *driver, size_t line_index,
                                  int cell)
{
   int end_cell;

   if (driver == NULL)
      return;
   if (driver->line_count == 0)
      agent_append_line(driver, "", 0);
   if (line_index >= driver->line_count)
      line_index = driver->line_count - 1;
   driver->cursor_line = line_index;
   if (cell < 0)
      cell = 0;
   end_cell = agent_prefix_last_cell(driver);
   if (cell > end_cell)
      cell = end_cell;
   driver->focus_zone = LOGICAL_CURSOR_ZONE_PREFIX;
   driver->cursor_cell = cell;
   driver->desired_cell = driver->cursor_cell;
   agent_ensure_visible(driver);
}

static void agent_move_to_first_col(AgentDriver *driver)
{
   if (driver == NULL)
      return;
   if (driver->focus_zone == LOGICAL_CURSOR_ZONE_COMMAND)
      driver->command_cursor_cell = 0;
   else if (driver->focus_zone == LOGICAL_CURSOR_ZONE_PREFIX)
   {
      driver->cursor_cell = 0;
      driver->desired_cell = 0;
   }
   else
   {
      driver->focus_zone = LOGICAL_CURSOR_ZONE_FILEAREA;
      driver->cursor_cell = 0;
      driver->desired_cell = 0;
   }
   agent_set_status(driver, "cursor moved");
}

static void agent_move_to_left_edge(AgentDriver *driver)
{
   if (driver == NULL)
      return;
   if (driver->focus_zone == LOGICAL_CURSOR_ZONE_PREFIX)
      agent_focus_filearea_at(driver, driver->cursor_line, 0);
   else
      agent_move_to_first_col(driver);
   agent_set_status(driver, "cursor moved");
}

static int agent_current_field_cell(const AgentDriver *driver)
{
   if (driver == NULL)
      return 0;
   if (driver->focus_zone == LOGICAL_CURSOR_ZONE_COMMAND)
      return driver->command_cursor_cell;
   return driver->cursor_cell;
}

static size_t agent_first_visible_file_line(const AgentDriver *driver)
{
   if (driver == NULL || driver->line_count == 0)
      return 0;
   if (driver->top_line >= driver->line_count)
      return driver->line_count - 1;
   return driver->top_line;
}

static void agent_move_to_visible_edge(AgentDriver *driver, int bottom)
{
   size_t line_index;
   int cell;

   if (driver == NULL)
      return;
   line_index = bottom ? agent_last_visible_file_line(driver)
                       : agent_first_visible_file_line(driver);
   if (driver->focus_zone == LOGICAL_CURSOR_ZONE_COMMAND)
   {
      cell = driver->command_cursor_cell;
      agent_focus_filearea_at(driver, line_index, cell);
   }
   else if (driver->focus_zone == LOGICAL_CURSOR_ZONE_PREFIX)
      agent_focus_prefix_at(driver, line_index, driver->cursor_cell);
   else
      agent_focus_filearea_at(driver, line_index, driver->cursor_cell);
   agent_set_status(driver, "cursor moved");
}

static void agent_move_to_last_col(AgentDriver *driver)
{
   if (driver == NULL)
      return;
   if (driver->focus_zone == LOGICAL_CURSOR_ZONE_COMMAND)
      driver->command_cursor_cell = agent_command_end_cell(driver);
   else if (driver->focus_zone == LOGICAL_CURSOR_ZONE_PREFIX)
   {
      driver->cursor_cell = agent_prefix_last_cell(driver);
      driver->desired_cell = driver->cursor_cell;
   }
   else
   {
      driver->focus_zone = LOGICAL_CURSOR_ZONE_FILEAREA;
      driver->cursor_cell = agent_line_end_cell(agent_current_line(driver));
      driver->desired_cell = driver->cursor_cell;
   }
   agent_set_status(driver, "cursor moved");
}

static void agent_move_to_right_edge(AgentDriver *driver)
{
   if (driver == NULL)
      return;
   if (driver->focus_zone == LOGICAL_CURSOR_ZONE_PREFIX)
   {
      agent_focus_filearea_at(driver, driver->cursor_line,
                              agent_line_end_cell(agent_current_line(driver)));
      agent_set_status(driver, "cursor moved");
      return;
   }
   agent_move_to_last_col(driver);
}

static void agent_move_tab_field_forward(AgentDriver *driver)
{
   size_t last_visible;

   if (driver == NULL)
      return;
   agent_ensure_visible(driver);
   last_visible = agent_last_visible_file_line(driver);
   if (driver->focus_zone == LOGICAL_CURSOR_ZONE_COMMAND)
      agent_focus_prefix_at(driver, agent_first_visible_file_line(driver), 0);
   else if (driver->focus_zone == LOGICAL_CURSOR_ZONE_PREFIX)
      agent_focus_filearea_at(driver, driver->cursor_line, 0);
   else if (driver->cursor_line >= last_visible)
   {
      driver->focus_zone = LOGICAL_CURSOR_ZONE_COMMAND;
      driver->command_cursor_cell = 0;
   }
   else
      agent_focus_prefix_at(driver, driver->cursor_line + 1, 0);
   agent_set_status(driver, "cursor moved");
}

static void agent_move_tab_field_backward(AgentDriver *driver)
{
   size_t first_visible;

   if (driver == NULL)
      return;
   if (agent_current_field_cell(driver) != 0)
   {
      agent_move_to_first_col(driver);
      return;
   }
   agent_ensure_visible(driver);
   first_visible = agent_first_visible_file_line(driver);
   if (driver->focus_zone == LOGICAL_CURSOR_ZONE_COMMAND)
      agent_focus_filearea_at(driver, agent_last_visible_file_line(driver), 0);
   else if (driver->focus_zone == LOGICAL_CURSOR_ZONE_FILEAREA)
      agent_focus_prefix_at(driver, driver->cursor_line, 0);
   else if (driver->cursor_line <= first_visible)
   {
      driver->focus_zone = LOGICAL_CURSOR_ZONE_COMMAND;
      driver->command_cursor_cell = 0;
   }
   else
      agent_focus_filearea_at(driver, driver->cursor_line - 1, 0);
   agent_set_status(driver, "cursor moved");
}

static int agent_prefix_target_index(AgentDriver *driver, char **text,
                                     size_t *index)
{
   char *arg;
   char *end = NULL;
   long line_no;

   if (index != NULL)
      *index = 0;
   if (driver == NULL || text == NULL || *text == NULL)
      return 0;
   arg = agent_trim(*text);
   line_no = strtol(arg, &end, 10);
   if (end != arg && end != NULL && (*end == '\0' || isspace((unsigned char)*end)))
   {
      if (line_no < 1 || (size_t)line_no > driver->line_count)
         return 0;
      if (index != NULL)
         *index = (size_t)(line_no - 1);
      *text = agent_trim(end);
      return 1;
   }
   if (index != NULL)
      *index = driver->cursor_line;
   *text = arg;
   return 1;
}

static int agent_prefix_set(AgentDriver *driver, char *args)
{
   size_t index;

   if (driver == NULL || args == NULL)
      return 0;
   if (!agent_prefix_target_index(driver, &args, &index))
   {
      agent_set_status(driver, "invalid prefix line");
      return 0;
   }
   if (index >= driver->line_count)
      return 0;
   agent_copy_text(driver->lines[index].prefix_command,
                   sizeof(driver->lines[index].prefix_command), args);
   agent_set_status(driver, "prefix set");
   return 1;
}

static int agent_prefix_clear(AgentDriver *driver, char *args)
{
   size_t index;
   size_t i;

   if (driver == NULL)
      return 0;
   args = agent_trim(args);
   if (args != NULL && agent_ascii_equal_ci(args, "all"))
   {
      for (i = 0; i < driver->line_count; i++)
         driver->lines[i].prefix_command[0] = '\0';
      agent_set_status(driver, "prefix cleared");
      return 1;
   }
   if (args == NULL || *args == '\0')
      index = driver->cursor_line;
   else if (!agent_prefix_target_index(driver, &args, &index))
   {
      agent_set_status(driver, "invalid prefix line");
      return 0;
   }
   if (index >= driver->line_count)
      return 0;
   driver->lines[index].prefix_command[0] = '\0';
   agent_set_status(driver, "prefix cleared");
   return 1;
}

static int agent_prefix_execute_one(AgentDriver *driver, size_t *index)
{
   char command[LLM_DRIVER_MAX_PREFIX + 1];
   char *text;

   if (driver == NULL || index == NULL || *index >= driver->line_count)
      return 0;
   agent_copy_text(command, sizeof(command),
                   driver->lines[*index].prefix_command);
   text = agent_trim(command);
   if (text == NULL || *text == '\0')
      return 1;

   if (agent_ascii_equal_ci(text, "d")
   ||  agent_ascii_equal_ci(text, "del")
   ||  agent_ascii_equal_ci(text, "delete"))
   {
      if (!agent_delete_line_at(driver, *index))
         return 0;
      if (*index >= driver->line_count)
         *index = driver->line_count == 0 ? 0 : driver->line_count - 1;
      driver->cursor_line = *index;
      driver->cursor_cell = 0;
      driver->desired_cell = 0;
      driver->dirty = 1;
      return 1;
   }
   if (agent_ascii_equal_ci(text, "dup")
   ||  agent_ascii_equal_ci(text, "copy"))
   {
      const AgentDriverLine *line = &driver->lines[*index];

      if (!agent_insert_line_at(driver, *index + 1,
                                (const char *)line->text, line->len))
         return 0;
      driver->lines[*index].prefix_command[0] = '\0';
      (*index)++;
      driver->cursor_line = *index;
      driver->dirty = 1;
      return 1;
   }
   if (agent_ascii_starts_ci(text, "r "))
   {
      const char *payload = agent_trim(text + 2);

      if (!agent_line_set(&driver->lines[*index], payload, strlen(payload)))
         return 0;
      driver->lines[*index].prefix_command[0] = '\0';
      driver->cursor_line = *index;
      driver->cursor_cell = 0;
      driver->desired_cell = 0;
      driver->dirty = 1;
      return 1;
   }
   if (agent_ascii_starts_ci(text, "i "))
   {
      const char *payload = agent_trim(text + 2);

      if (!agent_insert_line_at(driver, *index, payload, strlen(payload)))
         return 0;
      (*index)++;
      driver->lines[*index].prefix_command[0] = '\0';
      driver->cursor_line = *index;
      driver->dirty = 1;
      return 1;
   }
   if (agent_ascii_starts_ci(text, "a "))
   {
      const char *payload = agent_trim(text + 2);

      if (!agent_insert_line_at(driver, *index + 1, payload, strlen(payload)))
         return 0;
      driver->lines[*index].prefix_command[0] = '\0';
      (*index)++;
      driver->cursor_line = *index;
      driver->dirty = 1;
      return 1;
   }

   agent_set_status(driver, "unsupported prefix command");
   return 0;
}

static int agent_prefix_execute(AgentDriver *driver)
{
   size_t i;
   int executed = 0;

   if (driver == NULL)
      return 0;
   for (i = 0; i < driver->line_count; i++)
   {
      if (driver->lines[i].prefix_command[0] == '\0')
         continue;
      if (!agent_prefix_execute_one(driver, &i))
         return 0;
      executed++;
   }
   agent_ensure_visible(driver);
   agent_set_status(driver, executed > 0 ? "prefix executed"
                                         : "no prefix commands");
   return 1;
}

static size_t agent_line_byte_for_cell(const AgentDriverLine *line, int cell)
{
   TextPos pos;

   if (line == NULL)
      return 0;
   if (cell < 0)
      cell = 0;
   pos = textpos_from_cell_virtual(line->text, line->len, cell,
                                   TEXT_SNAP_BACKWARD);
   if (pos.byte_offset > line->len)
      return line->len;
   return pos.byte_offset;
}

static void agent_selection_order(const AgentDriver *driver,
                                  size_t *start_line, int *start_cell,
                                  size_t *end_line, int *end_cell)
{
   size_t left_line;
   size_t right_line;
   int left_cell;
   int right_cell;

   if (driver == NULL)
      return;
   left_line = driver->selection_start_line;
   right_line = driver->selection_end_line;
   left_cell = driver->selection_start_cell;
   right_cell = driver->selection_end_cell;
   if (left_line > right_line
   ||  (left_line == right_line && left_cell > right_cell))
   {
      size_t tmp_line = left_line;
      int tmp_cell = left_cell;

      left_line = right_line;
      left_cell = right_cell;
      right_line = tmp_line;
      right_cell = tmp_cell;
   }
   if (left_line >= driver->line_count)
      left_line = driver->line_count == 0 ? 0 : driver->line_count - 1;
   if (right_line >= driver->line_count)
      right_line = driver->line_count == 0 ? 0 : driver->line_count - 1;
   if (start_line != NULL)
      *start_line = left_line;
   if (start_cell != NULL)
      *start_cell = left_cell;
   if (end_line != NULL)
      *end_line = right_line;
   if (end_cell != NULL)
      *end_cell = right_cell;
}

static int agent_selection_set(AgentDriver *driver, long start_line,
                               long start_cell, long end_line,
                               long end_cell)
{
   if (driver == NULL || driver->line_count == 0
   ||  start_line < 1 || end_line < 1
   ||  (size_t)start_line > driver->line_count
   ||  (size_t)end_line > driver->line_count
   ||  start_cell < 0 || end_cell < 0)
   {
      agent_set_status(driver, "invalid selection");
      return 0;
   }
   driver->selection_active = 1;
   driver->selection_start_line = (size_t)(start_line - 1);
   driver->selection_start_cell = (int)start_cell;
   driver->selection_end_line = (size_t)(end_line - 1);
   driver->selection_end_cell = (int)end_cell;
   driver->cursor_line = driver->selection_end_line;
   driver->cursor_cell = driver->selection_end_cell;
   driver->desired_cell = driver->cursor_cell;
   driver->focus_zone = LOGICAL_CURSOR_ZONE_FILEAREA;
   agent_ensure_visible(driver);
   agent_set_status(driver, "selection set");
   return 1;
}

static char *agent_selection_text(const AgentDriver *driver)
{
   size_t start_line;
   size_t end_line;
   int start_cell;
   int end_cell;
   size_t len = 0;
   size_t offset = 0;
   size_t i;
   char *text;

   if (driver == NULL || !driver->selection_active
   ||  driver->line_count == 0)
      return NULL;
   agent_selection_order(driver, &start_line, &start_cell, &end_line,
                         &end_cell);
   for (i = start_line; i <= end_line; i++)
   {
      const AgentDriverLine *line = &driver->lines[i];
      size_t from = i == start_line
                  ? agent_line_byte_for_cell(line, start_cell) : 0;
      size_t to = i == end_line
                ? agent_line_byte_for_cell(line, end_cell) : line->len;

      if (to > line->len)
         to = line->len;
      if (from > to)
         from = to;
      len += to - from;
      if (i < end_line)
         len++;
   }
   text = (char *)malloc(len + 1);
   if (text == NULL)
      return NULL;
   for (i = start_line; i <= end_line; i++)
   {
      const AgentDriverLine *line = &driver->lines[i];
      size_t from = i == start_line
                  ? agent_line_byte_for_cell(line, start_cell) : 0;
      size_t to = i == end_line
                ? agent_line_byte_for_cell(line, end_cell) : line->len;

      if (to > line->len)
         to = line->len;
      if (from > to)
         from = to;
      if (to > from)
      {
         memcpy(text + offset, line->text + from, to - from);
         offset += to - from;
      }
      if (i < end_line)
         text[offset++] = '\n';
   }
   text[offset] = '\0';
   return text;
}

static int agent_selection_copy(AgentDriver *driver)
{
   char *text;

   if (driver == NULL || !driver->selection_active)
   {
      agent_set_status(driver, "no selection");
      return 0;
   }
   text = agent_selection_text(driver);
   if (text == NULL)
      return 0;
   agent_copy_text(driver->clipboard, sizeof(driver->clipboard), text);
   free(text);
   agent_set_status(driver, "selection copied");
   return 1;
}

static int agent_selection_replace(AgentDriver *driver, const char *replacement)
{
   size_t start_line;
   size_t end_line;
   int start_cell;
   int end_cell;
   size_t start_byte;
   size_t end_byte;
   AgentDriverLine *first;
   AgentDriverLine *last;
   char *next;
   size_t replacement_len;
   size_t next_len;

   if (driver == NULL || !driver->selection_active)
   {
      agent_set_status(driver, "no selection");
      return 0;
   }
   if (replacement == NULL)
      replacement = "";
   agent_selection_order(driver, &start_line, &start_cell, &end_line,
                         &end_cell);
   first = &driver->lines[start_line];
   last = &driver->lines[end_line];
   start_byte = agent_line_byte_for_cell(first, start_cell);
   end_byte = agent_line_byte_for_cell(last, end_cell);
   replacement_len = strlen(replacement);
   if (start_line == end_line)
   {
      if (!agent_line_delete(first, start_byte, end_byte - start_byte))
         return 0;
      if (replacement_len > 0
      &&  !agent_line_insert(first, start_byte, (const CHARTYPE *)replacement,
                             replacement_len))
         return 0;
   }
   else
   {
      size_t suffix_len = last->len - end_byte;

      next_len = start_byte + replacement_len + suffix_len;
      next = (char *)malloc(next_len + 1);
      if (next == NULL)
         return 0;
      memcpy(next, first->text, start_byte);
      memcpy(next + start_byte, replacement, replacement_len);
      memcpy(next + start_byte + replacement_len, last->text + end_byte,
             suffix_len);
      next[next_len] = '\0';
      if (!agent_line_set(first, next, next_len))
      {
         free(next);
         return 0;
      }
      free(next);
      while (end_line > start_line)
      {
         if (!agent_delete_line_at(driver, start_line + 1))
            return 0;
         end_line--;
      }
   }
   driver->selection_active = 0;
   driver->cursor_line = start_line;
   driver->cursor_cell =
      textpos_from_byte(driver->lines[start_line].text,
                        driver->lines[start_line].len,
                        start_byte + replacement_len).cell_column;
   driver->desired_cell = driver->cursor_cell;
   driver->focus_zone = LOGICAL_CURSOR_ZONE_FILEAREA;
   driver->dirty = 1;
   agent_ensure_visible(driver);
   agent_set_status(driver, replacement_len > 0 ? "selection replaced"
                                                : "selection deleted");
   return 1;
}

static int agent_undo(AgentDriver *driver)
{
   AgentDriverSnapshot current;
   AgentDriverSnapshot previous;

   if (driver == NULL || driver->undo_count == 0)
   {
      agent_set_status(driver, "nothing to undo");
      return 0;
   }
   if (!agent_snapshot_capture(driver, &current))
      return 0;
   previous = driver->undo[driver->undo_count - 1];
   memset(&driver->undo[driver->undo_count - 1], 0,
          sizeof(driver->undo[driver->undo_count - 1]));
   driver->undo_count--;
   if (!agent_snapshot_array_push(driver->redo, &driver->redo_count, current))
   {
      agent_snapshot_clear(&previous);
      return 0;
   }
   if (!agent_snapshot_restore(driver, &previous))
   {
      agent_snapshot_clear(&previous);
      return 0;
   }
   agent_snapshot_clear(&previous);
   driver->selection_active = 0;
   agent_buffer_sync_current(driver);
   agent_set_status(driver, "undone");
   return 1;
}

static int agent_redo(AgentDriver *driver)
{
   AgentDriverSnapshot current;
   AgentDriverSnapshot next;

   if (driver == NULL || driver->redo_count == 0)
   {
      agent_set_status(driver, "nothing to redo");
      return 0;
   }
   if (!agent_snapshot_capture(driver, &current))
      return 0;
   next = driver->redo[driver->redo_count - 1];
   memset(&driver->redo[driver->redo_count - 1], 0,
          sizeof(driver->redo[driver->redo_count - 1]));
   driver->redo_count--;
   if (!agent_snapshot_array_push(driver->undo, &driver->undo_count, current))
   {
      agent_snapshot_clear(&next);
      return 0;
   }
   if (!agent_snapshot_restore(driver, &next))
   {
      agent_snapshot_clear(&next);
      return 0;
   }
   agent_snapshot_clear(&next);
   driver->selection_active = 0;
   agent_buffer_sync_current(driver);
   agent_set_status(driver, "redone");
   return 1;
}

static int agent_apply_sos_command(AgentDriver *driver, const char *command)
{
   char buffer[THE_INPUT_COMMAND_MAX + 1];
   char *text;

   if (driver == NULL || command == NULL)
      return 0;
   agent_copy_text(buffer, sizeof(buffer), command);
   text = agent_trim(buffer);
   if (text == NULL || *text == '\0')
   {
      agent_set_status(driver, "unsupported command");
      return 0;
   }

   if (agent_ascii_equal_ci(text, "topedge"))
   {
      agent_move_to_visible_edge(driver, 0);
      return 1;
   }
   if (agent_ascii_equal_ci(text, "bottomedge"))
   {
      agent_move_to_visible_edge(driver, 1);
      return 1;
   }
   if (agent_ascii_equal_ci(text, "leftedge"))
   {
      agent_move_to_left_edge(driver);
      return 1;
   }
   if (agent_ascii_equal_ci(text, "firstcol"))
   {
      agent_move_to_first_col(driver);
      return 1;
   }
   if (agent_ascii_equal_ci(text, "rightedge"))
   {
      agent_move_to_right_edge(driver);
      return 1;
   }
   if (agent_ascii_equal_ci(text, "lastcol")
   ||  agent_ascii_equal_ci(text, "endchar"))
   {
      agent_move_to_last_col(driver);
      return 1;
   }
   if (agent_ascii_equal_ci(text, "firstchar"))
   {
      agent_move_to_first_nonblank(driver);
      return 1;
   }
   if (agent_ascii_equal_ci(text, "delchar")
   ||  agent_ascii_equal_ci(text, "cuadelchar"))
   {
      if (driver->focus_zone == LOGICAL_CURSOR_ZONE_COMMAND)
         return agent_delete_command_at_cursor(driver);
      if (driver->focus_zone == LOGICAL_CURSOR_ZONE_PREFIX)
         return agent_delete_prefix_at_cursor(driver);
      return agent_delete_at_cursor(driver);
   }
   if (agent_ascii_equal_ci(text, "delback")
   ||  agent_ascii_equal_ci(text, "cuadelback"))
   {
      if (driver->focus_zone == LOGICAL_CURSOR_ZONE_COMMAND)
         return agent_backspace_command(driver);
      if (driver->focus_zone == LOGICAL_CURSOR_ZONE_PREFIX)
         return agent_backspace_prefix(driver);
      return agent_backspace(driver);
   }
   if (agent_ascii_equal_ci(text, "delend"))
   {
      if (driver->focus_zone == LOGICAL_CURSOR_ZONE_COMMAND)
         return agent_delete_command_to_end(driver);
      if (driver->focus_zone == LOGICAL_CURSOR_ZONE_PREFIX)
      {
         AgentDriverLine *line = agent_current_line(driver);
         TextPos pos;

         if (line == NULL)
            return 0;
         pos = textpos_from_cell((const CHARTYPE *)line->prefix_command,
                                 strlen(line->prefix_command),
                                 driver->cursor_cell, TEXT_SNAP_BACKWARD);
         line->prefix_command[pos.byte_offset] = '\0';
         agent_set_status(driver, "prefix edited");
         return 1;
      }
      return agent_delete_to_end(driver);
   }
   if (agent_ascii_equal_ci(text, "delword"))
   {
      if (driver->focus_zone == LOGICAL_CURSOR_ZONE_COMMAND)
         return agent_delete_command_word(driver);
      if (driver->focus_zone == LOGICAL_CURSOR_ZONE_PREFIX)
         return agent_delete_prefix_word(driver);
      return agent_delete_word(driver);
   }
   if (agent_ascii_equal_ci(text, "prefix"))
   {
      if (driver->focus_zone != LOGICAL_CURSOR_ZONE_COMMAND)
      {
         agent_focus_prefix_at(driver, driver->cursor_line, 0);
         agent_set_status(driver, "prefix focused");
      }
      else
         agent_set_status(driver, "command focused");
      return 1;
   }
   if (agent_ascii_equal_ci(text, "tabfieldf"))
   {
      agent_move_tab_field_forward(driver);
      return 1;
   }
   if (agent_ascii_equal_ci(text, "tabfieldb"))
   {
      agent_move_tab_field_backward(driver);
      return 1;
   }
   if (agent_ascii_equal_ci(text, "qcmnd"))
   {
      driver->focus_zone = LOGICAL_CURSOR_ZONE_COMMAND;
      driver->command_line[0] = '\0';
      driver->command_cursor_cell = 0;
      agent_set_status(driver, "command focused");
      return 1;
   }
   if (agent_ascii_equal_ci(text, "execute"))
   {
      driver->focus_zone = LOGICAL_CURSOR_ZONE_COMMAND;
      driver->command_cursor_cell = agent_command_end_cell(driver);
      agent_set_status(driver, "command focused");
      return 1;
   }

   agent_set_status(driver, "unsupported command");
   return 0;
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
   if (agent_ascii_equal_ci(text, "focus prefix"))
   {
      agent_focus_prefix_at(driver, driver->cursor_line, 0);
      agent_set_status(driver, "prefix focused");
      return 1;
   }

   if (agent_ascii_equal_ci(text, "undo"))
      return agent_undo(driver);
   if (agent_ascii_equal_ci(text, "redo"))
      return agent_redo(driver);

   if (agent_ascii_starts_ci(text, "prefix-set "))
      return agent_prefix_set(driver, text + 11);
   if (agent_ascii_starts_ci(text, "prefix "))
      return agent_prefix_set(driver, text + 7);
   if (agent_ascii_equal_ci(text, "prefix-clear"))
      return agent_prefix_clear(driver, "");
   if (agent_ascii_starts_ci(text, "prefix-clear "))
      return agent_prefix_clear(driver, text + 13);
   if (agent_ascii_equal_ci(text, "prefix-execute")
   ||  agent_ascii_equal_ci(text, "execute-prefix"))
      return agent_prefix_execute(driver);

   if (agent_ascii_starts_ci(text, "select "))
   {
      long start_line;
      long start_cell;
      long end_line;
      long end_cell;

      if (sscanf(text + 7, "%ld %ld %ld %ld",
                 &start_line, &start_cell, &end_line, &end_cell) != 4)
      {
         agent_set_status(driver, "invalid selection");
         return 0;
      }
      return agent_selection_set(driver, start_line, start_cell,
                                 end_line, end_cell);
   }
   if (agent_ascii_equal_ci(text, "select-clear")
   ||  agent_ascii_equal_ci(text, "selection-clear"))
   {
      driver->selection_active = 0;
      agent_set_status(driver, "selection cleared");
      return 1;
   }
   if (agent_ascii_equal_ci(text, "selection-copy")
   ||  agent_ascii_equal_ci(text, "copy-selection"))
      return agent_selection_copy(driver);
   if (agent_ascii_equal_ci(text, "selection-delete")
   ||  agent_ascii_equal_ci(text, "delete-selection"))
      return agent_selection_replace(driver, "");
   if (agent_ascii_starts_ci(text, "selection-replace "))
      return agent_selection_replace(driver, text + 18);
   if (agent_ascii_starts_ci(text, "replace-selection "))
      return agent_selection_replace(driver, text + 18);

   if (agent_ascii_starts_ci(text, "buffer-open "))
      return agent_buffer_open(driver, agent_trim(text + 12));
   if (agent_ascii_starts_ci(text, "buffer-switch "))
      return agent_buffer_switch(driver, agent_trim(text + 14));
   if (agent_ascii_equal_ci(text, "buffer-list")
   ||  agent_ascii_equal_ci(text, "buffers"))
   {
      agent_buffer_sync_current(driver);
      agent_set_statusf(driver, "buffers %zu current %zu",
                        driver->buffer_count, driver->current_buffer);
      return 1;
   }
   if (agent_ascii_equal_ci(text, "buffer-close"))
      return agent_buffer_close(driver, "", 0);
   if (agent_ascii_equal_ci(text, "buffer-close!"))
      return agent_buffer_close(driver, "", 1);
   if (agent_ascii_starts_ci(text, "buffer-close! "))
      return agent_buffer_close(driver, agent_trim(text + 14), 1);
   if (agent_ascii_starts_ci(text, "buffer-close "))
      return agent_buffer_close(driver, agent_trim(text + 13), 0);

   if (agent_ascii_equal_ci(text, "project-list"))
      return agent_project_list(driver, ".");
   if (agent_ascii_starts_ci(text, "project-list "))
      return agent_project_list(driver, agent_trim(text + 13));

   if (agent_ascii_equal_ci(text, "new")
   ||  agent_ascii_equal_ci(text, "new!"))
   {
      int force = text[strlen(text) - 1] == '!';

      if (driver->dirty && !force)
      {
         agent_set_status(driver, "unsaved changes");
         return 0;
      }
      if (!agent_driver_set_text(driver, ""))
         return 0;
      driver->path[0] = '\0';
      driver->dirty = 0;
      driver->command_line[0] = '\0';
      driver->command_cursor_cell = 0;
      agent_snapshot_array_clear(driver->undo, &driver->undo_count);
      agent_snapshot_array_clear(driver->redo, &driver->redo_count);
      agent_buffer_sync_current(driver);
      agent_set_status(driver, "new file");
      return 1;
   }

   if (agent_ascii_starts_ci(text, "open ")
   ||  agent_ascii_starts_ci(text, "load ")
   ||  agent_ascii_starts_ci(text, "edit ")
   ||  agent_ascii_starts_ci(text, "open! ")
   ||  agent_ascii_starts_ci(text, "load! ")
   ||  agent_ascii_starts_ci(text, "edit! "))
   {
      const char *path;
      int force = strchr(text, '!') != NULL
               && strchr(text, '!') < strchr(text, ' ');

      if (driver->dirty && !force)
      {
         agent_set_status(driver, "unsaved changes");
         return 0;
      }
      path = strchr(text, ' ');
      if (path == NULL)
      {
         agent_set_status(driver, "no path");
         return 0;
      }
      path = agent_trim((char *)path);
      if (path == NULL || *path == '\0')
      {
         agent_set_status(driver, "no path");
         return 0;
      }
      if (!agent_driver_load_file(driver, path))
         return 0;
      driver->command_line[0] = '\0';
      driver->command_cursor_cell = 0;
      agent_snapshot_array_clear(driver->undo, &driver->undo_count);
      agent_snapshot_array_clear(driver->redo, &driver->redo_count);
      agent_buffer_sync_current(driver);
      return 1;
   }

   if (agent_ascii_starts_ci(text, "sos "))
      return agent_apply_sos_command(driver, text + 4);
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
      agent_move_to_first_col(driver);
      return 1;
   }
   if (agent_ascii_equal_ci(text, "end"))
   {
      agent_move_to_last_col(driver);
      return 1;
   }
   if (agent_ascii_equal_ci(text, "top"))
      return agent_goto_line(driver, 1);
   if (agent_ascii_equal_ci(text, "bottom"))
      return agent_goto_line(driver, (long)driver->line_count);
   if (agent_ascii_equal_ci(text, "pageup"))
   {
      agent_move_vertical(driver, -(driver->rows > 1 ? driver->rows - 1 : 1));
      return 1;
   }
   if (agent_ascii_equal_ci(text, "pagedown"))
   {
      agent_move_vertical(driver, driver->rows > 1 ? driver->rows - 1 : 1);
      return 1;
   }
   if (agent_ascii_equal_ci(text, "tab"))
   {
      agent_move_tab_field_forward(driver);
      return 1;
   }
   if (agent_ascii_equal_ci(text, "backtab"))
   {
      agent_move_tab_field_backward(driver);
      return 1;
   }
   if (agent_ascii_equal_ci(text, "delete"))
   {
      if (driver->focus_zone == LOGICAL_CURSOR_ZONE_COMMAND)
         return agent_delete_command_at_cursor(driver);
      if (driver->focus_zone == LOGICAL_CURSOR_ZONE_PREFIX)
         return agent_delete_prefix_at_cursor(driver);
      return agent_delete_at_cursor(driver);
   }
   if (agent_ascii_equal_ci(text, "backspace"))
   {
      if (driver->focus_zone == LOGICAL_CURSOR_ZONE_COMMAND)
         return agent_backspace_command(driver);
      if (driver->focus_zone == LOGICAL_CURSOR_ZONE_PREFIX)
         return agent_backspace_prefix(driver);
      return agent_backspace(driver);
   }
   if (agent_ascii_starts_ci(text, "goto "))
      return agent_goto_line(driver, strtol(text + 5, NULL, 10));
   if (agent_ascii_equal_ci(text, "find-next")
   ||  agent_ascii_equal_ci(text, "search-next"))
   {
      if (driver->search_text[0] == '\0')
      {
         agent_set_status(driver, "empty search");
         return 0;
      }
      return agent_find_text(driver, driver->search_text, 1, 0);
   }
   if (agent_ascii_equal_ci(text, "find-prev")
   ||  agent_ascii_equal_ci(text, "search-prev")
   ||  agent_ascii_equal_ci(text, "find-previous")
   ||  agent_ascii_equal_ci(text, "search-previous"))
   {
      if (driver->search_text[0] == '\0')
      {
         agent_set_status(driver, "empty search");
         return 0;
      }
      return agent_find_text(driver, driver->search_text, 0, 1);
   }
   if (agent_ascii_starts_ci(text, "find "))
      return agent_find_text(driver, agent_trim(text + 5), 0, 0);
   if (agent_ascii_starts_ci(text, "search "))
      return agent_find_text(driver, agent_trim(text + 7), 0, 0);
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
   if (agent_payload_after(text, "setline") != NULL)
   {
      const char *payload = agent_payload_after(text, "setline");
      AgentDriverLine *line = agent_current_line(driver);

      if (line == NULL || !agent_line_set(line, payload, strlen(payload)))
         return 0;
      driver->cursor_cell = 0;
      driver->desired_cell = 0;
      driver->dirty = 1;
      agent_set_status(driver, "line set");
      return 1;
   }
   if (agent_payload_after(text, "insertline") != NULL)
   {
      const char *payload = agent_payload_after(text, "insertline");

      if (!agent_insert_line_at(driver, driver->cursor_line, payload,
                                strlen(payload)))
         return 0;
      driver->focus_zone = LOGICAL_CURSOR_ZONE_FILEAREA;
      driver->cursor_cell = 0;
      driver->desired_cell = 0;
      driver->dirty = 1;
      agent_ensure_visible(driver);
      agent_set_status(driver, "line inserted");
      return 1;
   }
   if (agent_payload_after(text, "appendline") != NULL)
   {
      const char *payload = agent_payload_after(text, "appendline");

      if (!agent_insert_line_at(driver, driver->cursor_line + 1, payload,
                                strlen(payload)))
         return 0;
      driver->cursor_line++;
      driver->focus_zone = LOGICAL_CURSOR_ZONE_FILEAREA;
      driver->cursor_cell = 0;
      driver->desired_cell = 0;
      driver->dirty = 1;
      agent_ensure_visible(driver);
      agent_set_status(driver, "line appended");
      return 1;
   }
   if (agent_ascii_equal_ci(text, "deleteline")
   ||  agent_ascii_equal_ci(text, "delete line"))
   {
      if (!agent_delete_line_at(driver, driver->cursor_line))
         return 0;
      if (driver->cursor_line >= driver->line_count)
         driver->cursor_line = driver->line_count - 1;
      driver->focus_zone = LOGICAL_CURSOR_ZONE_FILEAREA;
      driver->cursor_cell = 0;
      driver->desired_cell = 0;
      driver->dirty = 1;
      agent_ensure_visible(driver);
      agent_set_status(driver, "line deleted");
      return 1;
   }
   if (agent_ascii_equal_ci(text, "duplicateline")
   ||  agent_ascii_equal_ci(text, "duplicate line"))
   {
      const AgentDriverLine *line = agent_current_line_const(driver);

      if (line == NULL
      ||  !agent_insert_line_at(driver, driver->cursor_line + 1,
                                (const char *)line->text, line->len))
         return 0;
      driver->cursor_line++;
      driver->focus_zone = LOGICAL_CURSOR_ZONE_FILEAREA;
      driver->cursor_cell = 0;
      driver->desired_cell = 0;
      driver->dirty = 1;
      agent_ensure_visible(driver);
      agent_set_status(driver, "line duplicated");
      return 1;
   }
   if (agent_ascii_starts_ci(text, "replace-all "))
   {
      char *needle;
      char *replacement;

      if (!agent_parse_replace_args(text + 12, &needle, &replacement))
      {
         agent_set_status(driver, "invalid replace");
         return 0;
      }
      return agent_replace_all(driver, needle, replacement);
   }
   if (agent_ascii_starts_ci(text, "replace "))
   {
      char *needle;
      char *replacement;

      if (!agent_parse_replace_args(text + 8, &needle, &replacement))
      {
         agent_set_status(driver, "invalid replace");
         return 0;
      }
      return agent_replace_next(driver, needle, replacement);
   }
   if (agent_ascii_starts_ci(text, "insert "))
   {
      if (driver->focus_zone == LOGICAL_CURSOR_ZONE_COMMAND)
         return agent_insert_command_bytes(driver, (const CHARTYPE *)(text + 7),
                                           strlen(text + 7));
      if (driver->focus_zone == LOGICAL_CURSOR_ZONE_PREFIX)
         return agent_insert_prefix_bytes(driver, (const CHARTYPE *)(text + 7),
                                          strlen(text + 7));
      return agent_insert_bytes(driver, (const CHARTYPE *)(text + 7),
                                strlen(text + 7));
   }
   if (agent_ascii_starts_ci(text, "type "))
   {
      if (driver->focus_zone == LOGICAL_CURSOR_ZONE_COMMAND)
         return agent_insert_command_bytes(driver, (const CHARTYPE *)(text + 5),
                                           strlen(text + 5));
      if (driver->focus_zone == LOGICAL_CURSOR_ZONE_PREFIX)
         return agent_insert_prefix_bytes(driver, (const CHARTYPE *)(text + 5),
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

   agent_set_status(driver, "unsupported command");
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
   driver->buffer_count = 1;
   driver->current_buffer = 0;
   driver->buffers[0].used = 1;
   agent_buffer_sync_current(driver);
   agent_set_status(driver, "ready");
}

void agent_driver_free(AgentDriver *driver)
{
   size_t i;

   if (driver == NULL)
      return;
   agent_snapshot_array_clear(driver->undo, &driver->undo_count);
   agent_snapshot_array_clear(driver->redo, &driver->redo_count);
   for (i = 0; i < driver->buffer_count && i < AGENT_DRIVER_BUFFER_MAX; i++)
      agent_snapshot_clear(&driver->buffers[i].snapshot);
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
   driver->search_text[0] = '\0';
   driver->selection_active = 0;
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
      agent_buffer_sync_current(driver);
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
   agent_buffer_sync_current(driver);
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
   agent_buffer_sync_current(driver);
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

      if (line->prefix_command[0] != '\0')
         agent_copy_text(prefixes[row_index], sizeof(prefixes[row_index]),
                         line->prefix_command);
      else
         snprintf(prefixes[row_index], sizeof(prefixes[row_index]), "%6ld",
                  (long)line_index + 1);
      ui_frame_set_row(&frame, row_index, UI_ROW_FILE,
                       (LINETYPE)line_index + 1, row, 0,
                       line->text, line->len, 1);
      ui_frame_set_row_prefix(&frame, row_index,
                              (const CHARTYPE *)prefixes[row_index],
                              strlen(prefixes[row_index]), 1);
      if (line_index == driver->cursor_line
      &&  driver->focus_zone == LOGICAL_CURSOR_ZONE_PREFIX)
      {
         cursor = logical_cursor_from_cell(LOGICAL_CURSOR_ZONE_PREFIX,
                                           (LINETYPE)line_index + 1,
                                           row,
                                           (const CHARTYPE *)prefixes[row_index],
                                           strlen(prefixes[row_index]),
                                           driver->cursor_cell,
                                           TEXT_SNAP_BACKWARD, 0);
         logical_cursor_set_desired_cell(&cursor, driver->desired_cell);
      }
      else if (line_index == driver->cursor_line
           &&  driver->focus_zone != LOGICAL_CURSOR_ZONE_COMMAND)
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
   llm_driver_screen_view_set_buffer(view, driver->path, driver->dirty,
                                     driver->line_count);
   llm_driver_screen_view_set_history(view, driver->undo_count > 0,
                                      driver->redo_count > 0);
   llm_driver_screen_view_set_selection(
      view, driver->selection_active,
      driver->selection_active ? (LINETYPE)driver->selection_start_line + 1 : 0,
      driver->selection_active ? driver->selection_start_cell : 0,
      driver->selection_active ? (LINETYPE)driver->selection_end_line + 1 : 0,
      driver->selection_active ? driver->selection_end_cell : 0,
      driver->clipboard);
   for (line_index = 0; line_index < view->line_count; line_index++)
   {
      LINETYPE line_number = view->lines[line_index].line_number;

      if (view->lines[line_index].role == UI_ROW_FILE
      &&  line_number > 0 && (size_t)line_number <= driver->line_count)
         llm_driver_screen_view_set_prefix_command(
            view, line_index,
            driver->lines[(size_t)line_number - 1].prefix_command);
   }
   for (line_index = 0; line_index < driver->buffer_count
        && line_index < LLM_DRIVER_MAX_BUFFERS; line_index++)
   {
      const AgentDriverSnapshot *snapshot =
         &driver->buffers[line_index].snapshot;

      if (!driver->buffers[line_index].used)
         continue;
      if (line_index == driver->current_buffer)
         llm_driver_screen_view_add_buffer_info(
            view, driver->path, driver->dirty, driver->line_count, 1);
      else
         llm_driver_screen_view_add_buffer_info(
            view, snapshot->path, snapshot->dirty,
            agent_text_line_count(snapshot->text), 0);
   }
   if (driver->project_root[0] != '\0')
   {
      llm_driver_screen_view_set_project_root(view, driver->project_root);
      for (line_index = 0; line_index < driver->project_file_count
           && line_index < LLM_DRIVER_MAX_PROJECT_FILES; line_index++)
         llm_driver_screen_view_add_project_file(view,
                                                driver->project_files[line_index]);
   }
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

size_t agent_driver_format_delta(AgentDriver *driver,
                                 const LlmDriverFormatOptions *options,
                                 char *out, size_t out_len)
{
   LlmDriverScreenView current;
   const LlmDriverScreenView *previous;
   size_t used;

   if (driver == NULL)
   {
      if (out != NULL && out_len > 0)
         out[0] = '\0';
      return 0;
   }
   if (!agent_driver_screen_view(driver, &current))
   {
      if (out != NULL && out_len > 0)
         out[0] = '\0';
      return 0;
   }
   previous = driver->previous_view_valid ? &driver->previous_view : NULL;
   used = llm_driver_format_delta_view(previous, &current, options,
                                       out, out_len);
   driver->previous_view = current;
   driver->previous_view_valid = 1;
   return used;
}

static int agent_command_should_track_history(const char *text)
{
   char buffer[THE_INPUT_COMMAND_MAX + 1];
   char *command;

   if (text == NULL)
      return 0;
   agent_copy_text(buffer, sizeof(buffer), text);
   command = agent_trim(buffer);
   if (agent_ascii_equal_ci(command, "undo")
   ||  agent_ascii_equal_ci(command, "redo")
   ||  agent_ascii_equal_ci(command, "save")
   ||  agent_ascii_equal_ci(command, "write")
   ||  agent_ascii_starts_ci(command, "save ")
   ||  agent_ascii_starts_ci(command, "write ")
   ||  agent_ascii_equal_ci(command, "new")
   ||  agent_ascii_equal_ci(command, "new!")
   ||  agent_ascii_starts_ci(command, "open ")
   ||  agent_ascii_starts_ci(command, "open! ")
   ||  agent_ascii_starts_ci(command, "load ")
   ||  agent_ascii_starts_ci(command, "load! ")
   ||  agent_ascii_starts_ci(command, "edit ")
   ||  agent_ascii_starts_ci(command, "edit! ")
   ||  agent_ascii_starts_ci(command, "buffer-")
   ||  agent_ascii_starts_ci(command, "project-list"))
      return 0;
   return 1;
}

static int agent_input_should_track_history(const TheInputEvent *input)
{
   if (input == NULL)
      return 0;
   if (input->kind == THE_INPUT_TEXT)
      return 1;
   if (input->kind == THE_INPUT_KEY)
   {
      return input->key_code == KEY_BACKSPACE
          || input->key_code == KEY_DC;
   }
   if (input->kind != THE_INPUT_COMMAND)
      return 0;
   return agent_command_should_track_history(input->command);
}

int agent_driver_apply_input(AgentDriver *driver, const TheInputEvent *input)
{
   CHARTYPE bytes[4];
   size_t len;
   AgentDriverSnapshot before;
   int have_before = 0;
   int ok = 0;

   if (driver == NULL || input == NULL)
      return 0;
   if (agent_input_should_track_history(input))
      have_before = agent_snapshot_capture(driver, &before);
   switch (input->kind)
   {
      case THE_INPUT_TEXT:
         if (driver->focus_zone == LOGICAL_CURSOR_ZONE_PREFIX)
         {
            len = text_utf8_encode(input->codepoint, bytes);
            ok = len > 0 && agent_insert_prefix_bytes(driver, bytes, len);
            break;
         }
         len = text_utf8_encode(input->codepoint, bytes);
         if (len == 0)
            break;
         if (driver->focus_zone == LOGICAL_CURSOR_ZONE_COMMAND)
            ok = agent_insert_command_bytes(driver, bytes, len);
         else
            ok = agent_insert_bytes(driver, bytes, len);
         break;
      case THE_INPUT_KEY:
         switch (input->key_code)
         {
            case KEY_LEFT:
               agent_move_horizontal(driver, -1);
               ok = 1;
               break;
            case KEY_RIGHT:
               agent_move_horizontal(driver, 1);
               ok = 1;
               break;
            case KEY_UP:
               agent_move_vertical(driver, -1);
               ok = 1;
               break;
            case KEY_DOWN:
               agent_move_vertical(driver, 1);
               ok = 1;
               break;
            case AGENT_KEY_TAB:
               agent_move_tab_field_forward(driver);
               ok = 1;
               break;
            case KEY_BTAB:
               agent_move_tab_field_backward(driver);
               ok = 1;
               break;
            case KEY_HOME:
               if (driver->focus_zone == LOGICAL_CURSOR_ZONE_COMMAND)
                  driver->command_cursor_cell = 0;
               else if (driver->focus_zone == LOGICAL_CURSOR_ZONE_PREFIX)
               {
                  driver->cursor_cell = 0;
                  driver->desired_cell = 0;
               }
               else
               {
                  driver->cursor_cell = 0;
                  driver->desired_cell = 0;
               }
               agent_set_status(driver, "cursor moved");
               ok = 1;
               break;
            case KEY_END:
               if (driver->focus_zone == LOGICAL_CURSOR_ZONE_COMMAND)
                  driver->command_cursor_cell = agent_command_end_cell(driver);
               else if (driver->focus_zone == LOGICAL_CURSOR_ZONE_PREFIX)
               {
                  driver->cursor_cell = agent_prefix_last_cell(driver);
                  driver->desired_cell = driver->cursor_cell;
               }
               else
               {
                  driver->cursor_cell = agent_line_end_cell(agent_current_line(driver));
                  driver->desired_cell = driver->cursor_cell;
               }
               agent_set_status(driver, "cursor moved");
               ok = 1;
               break;
            case KEY_BACKSPACE:
               if (driver->focus_zone == LOGICAL_CURSOR_ZONE_COMMAND)
                  ok = agent_backspace_command(driver);
               if (driver->focus_zone == LOGICAL_CURSOR_ZONE_PREFIX)
                  ok = agent_backspace_prefix(driver);
               if (driver->focus_zone == LOGICAL_CURSOR_ZONE_FILEAREA)
                  ok = agent_backspace(driver);
               break;
            case KEY_DC:
               if (driver->focus_zone == LOGICAL_CURSOR_ZONE_COMMAND)
                  ok = agent_delete_command_at_cursor(driver);
               if (driver->focus_zone == LOGICAL_CURSOR_ZONE_PREFIX)
                  ok = agent_delete_prefix_at_cursor(driver);
               if (driver->focus_zone == LOGICAL_CURSOR_ZONE_FILEAREA)
                  ok = agent_delete_at_cursor(driver);
               break;
            case KEY_ENTER:
            case KEY_RETURN:
            {
               char command[LLM_DRIVER_MAX_COMMAND + 1];
               if (driver->focus_zone != LOGICAL_CURSOR_ZONE_COMMAND)
               {
                  agent_set_status(driver, "enter ignored");
                  ok = 1;
                  break;
               }
               agent_copy_text(command, sizeof(command), driver->command_line);
               if (!have_before && agent_command_should_track_history(command))
                  have_before = agent_snapshot_capture(driver, &before);
               driver->focus_zone = LOGICAL_CURSOR_ZONE_FILEAREA;
               ok = agent_apply_command(driver, command);
               break;
            }
            case KEY_ESC:
               driver->focus_zone = LOGICAL_CURSOR_ZONE_FILEAREA;
               agent_set_status(driver, "filearea focused");
               ok = 1;
               break;
            case KEY_PPAGE:
               agent_move_vertical(driver, -(driver->rows > 1 ? driver->rows - 1 : 1));
               ok = 1;
               break;
            case KEY_NPAGE:
               agent_move_vertical(driver, driver->rows > 1 ? driver->rows - 1 : 1);
               ok = 1;
               break;
            default:
               agent_set_status(driver, "unsupported key");
               ok = 0;
               break;
         }
         break;
      case THE_INPUT_COMMAND:
         ok = agent_apply_command(driver, input->command);
         break;
      case THE_INPUT_LOGICAL_HIT:
         if (input->target.kind == THE_INPUT_TARGET_FILEAREA)
         {
            if (!agent_focus_target_line(driver, &input->target))
               break;
            driver->focus_zone = LOGICAL_CURSOR_ZONE_FILEAREA;
            driver->cursor_cell = input->target.cell;
            driver->desired_cell = driver->cursor_cell;
            agent_set_status(driver, "cursor moved");
            ok = 1;
            break;
         }
         if (input->target.kind == THE_INPUT_TARGET_PREFIX)
         {
            int end_cell;

            if (!agent_focus_target_line(driver, &input->target))
               break;
            driver->focus_zone = LOGICAL_CURSOR_ZONE_PREFIX;
            driver->cursor_cell = input->target.cell;
            end_cell = agent_prefix_last_cell(driver);
            if (driver->cursor_cell > end_cell)
               driver->cursor_cell = end_cell;
            driver->desired_cell = driver->cursor_cell;
            agent_set_status(driver, "prefix focused");
            ok = 1;
            break;
         }
         if (input->target.kind == THE_INPUT_TARGET_COMMAND)
         {
            driver->focus_zone = LOGICAL_CURSOR_ZONE_COMMAND;
            driver->command_cursor_cell = input->target.cell;
            agent_clamp_command_cursor(driver);
            agent_set_status(driver, "command focused");
            ok = 1;
            break;
         }
         if (input->target.kind == THE_INPUT_TARGET_PROMPT)
         {
            agent_set_status(driver, "prompt hit");
            ok = 1;
            break;
         }
         if (input->target.kind == THE_INPUT_TARGET_STATUS)
         {
            agent_set_status(driver, "status hit");
            ok = 1;
            break;
         }
         if (input->target.kind == THE_INPUT_TARGET_TABLINE)
         {
            agent_set_status(driver, "tabline hit");
            ok = 1;
            break;
         }
         if (input->target.kind == THE_INPUT_TARGET_DIVIDER)
         {
            agent_set_status(driver, "divider hit");
            ok = 1;
            break;
         }
         if (input->target.kind == THE_INPUT_TARGET_WINDOW)
         {
            agent_set_status(driver, "window selected");
            ok = 1;
            break;
         }
         agent_set_status(driver, "unsupported target");
         ok = 0;
         break;
      case THE_INPUT_DEBUG:
         agent_set_status(driver,
                          the_input_debug_command_name(input->debug_command));
         ok = 1;
         break;
      case THE_INPUT_NONE:
      default:
         ok = 0;
         break;
   }

   if (have_before)
   {
      if (ok && !agent_content_matches_snapshot(driver, &before)
      &&  agent_snapshot_array_push(driver->undo, &driver->undo_count, before))
      {
         memset(&before, 0, sizeof(before));
         agent_clear_redo(driver);
         agent_buffer_sync_current(driver);
      }
      agent_snapshot_clear(&before);
   }
   return ok;
}

const char *agent_driver_status(const AgentDriver *driver)
{
   if (driver == NULL)
      return "";
   return driver->status;
}
