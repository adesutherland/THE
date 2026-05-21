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
   LlmDriverScreenLine *line;

   if (view == NULL || index >= LLM_DRIVER_MAX_ROWS)
      return 0;
   line = &view->lines[index];
   line->line_number = line_number;
   line->logical_row = logical_row;
   line->current = current;
   copy_text(line->prefix, sizeof(line->prefix), prefix);
   copy_text(line->text, sizeof(line->text), text);
   if (index >= view->line_count)
      view->line_count = index + 1;
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
      case LLM_DRIVER_INPUT_NONE:
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
