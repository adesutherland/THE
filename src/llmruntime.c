#include "llmruntime.h"

#include <stdio.h>
#include <string.h>

#include "screenframe.h"
#include "the.h"
#include "vars.h"

static void llm_runtime_visible_command(char *out, size_t out_len)
{
   size_t start;
   size_t len;

   if (out == NULL || out_len == 0)
      return;
   out[0] = '\0';
   if (cmd_rec == NULL || cmd_rec_len <= 0)
      return;

   start = (cmd_verify_col > 0) ? (size_t)cmd_verify_col - 1 : 0;
   if (start >= (size_t)cmd_rec_len)
      return;
   len = (size_t)cmd_rec_len - start;
   if (len >= out_len)
      len = out_len - 1;
   memcpy(out, cmd_rec + start, len);
   out[len] = '\0';
}

static void llm_runtime_status(CHARTYPE scrno, const LlmDriverScreenView *view,
                               char *out, size_t out_len)
{
   VIEW_DETAILS *screen_view;

   if (out == NULL || out_len == 0)
      return;
   out[0] = '\0';
   if (scrno >= MAX_SCREENS || view == NULL)
      return;

   screen_view = screen[scrno].screen_view;
   if (screen_view == NULL)
      return;
   snprintf(out, out_len, "focus=%s line=%ld row=%d cell=%d verify=%ld",
            logical_cursor_zone_name(view->cursor.zone),
            (long)view->cursor.line_number,
            view->cursor.zone_row,
            view->cursor.text.cell_column,
            (long)screen_view->verify_col);
}

int llm_runtime_screen_view(CHARTYPE scrno, LlmDriverScreenView *view)
{
   UiFrame frame;
   char command[LLM_DRIVER_MAX_COMMAND + 1];
   char status[LLM_DRIVER_MAX_COMMAND + 1];

   if (view == NULL)
      return 0;
   if (!screenframe_build(scrno, &frame))
      return 0;
   if (!llm_driver_screen_view_from_frame(&frame, view))
      return 0;

   llm_runtime_visible_command(command, sizeof(command));
   llm_driver_screen_view_set_command(view, command);
   llm_runtime_status(scrno, view, status, sizeof(status));
   llm_driver_screen_view_set_status(view, status);
   return 1;
}

size_t llm_runtime_format_screen(CHARTYPE scrno,
                                 const LlmDriverFormatOptions *options,
                                 char *out, size_t out_len)
{
   LlmDriverScreenView view;

   if (out == NULL || out_len == 0)
      return 0;
   out[0] = '\0';
   if (!llm_runtime_screen_view(scrno, &view))
      return 0;
   return llm_driver_format_semantic_view_with_options(&view, options,
                                                       out, out_len);
}
