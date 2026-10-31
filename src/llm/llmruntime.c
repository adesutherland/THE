#include "llmruntime.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "screenframe.h"
#include "panelui.h"
#include "parserdiagnostics.h"
#include "the.h"
#include "proto.h"
#include "vars.h"
#include "editorsource.h"

static void llm_runtime_diagnostic_presentation(LlmDriverScreenView *view)
{
   TheParserDiagnostic diagnostic;
   LINE *option;
   int severity;
   if (!view || !CURRENT_FILE) return;
   view->diagnostic_colours_valid = 1;
   view->diagnostic_colours[0] = set_colour(CURRENT_FILE->attr + ATTR_CBERROR);
   view->diagnostic_colours[1] = set_colour(CURRENT_FILE->attr + ATTR_CBWARN);
   view->diagnostic_colours[2] = set_colour(CURRENT_FILE->attr + ATTR_CBINFO);
   if (!view->status_line_visible || !the_parser_diagnostic_at_cursor(&diagnostic, &severity)) return;
   for (option = first_option; option; option = option->next)
   {
      size_t offset, length = 0, limit;
      const char *message = the_parser_diagnostic_display_message(&diagnostic);
      if (!option->name || strcmp((char *)option->name, "PMSG.1")) continue;
      offset = STATAREA_OFFSET + option->select + (option->line ? strlen((char *)option->line) : 0);
      if (offset >= strlen(view->status_line)) continue;
      limit = strlen(message);
      if (option->save_select && (size_t)option->save_select < limit) limit = option->save_select;
      /* Later profile fields, the clock or a narrow viewport can overwrite
       * the tail. Decorate only message bytes still present in the row. */
      while (length < limit && view->status_line[offset + length]
             && view->status_line[offset + length] == message[length]) length++;
      if (!length) continue;
      snprintf(view->status_pmsg, sizeof(view->status_pmsg), "%s", message);
      snprintf(view->status_pmsg_severity, sizeof(view->status_pmsg_severity), "%s", diagnostic.severity);
      view->status_pmsg_start = (int)textpos_count_codepoints((CHARTYPE *)view->status_line, offset);
      view->status_pmsg_length = (int)textpos_count_codepoints((CHARTYPE *)view->status_line + offset, length);
      view->status_pmsg_colour = set_colour(CURRENT_FILE->attr +
         (severity == '3' ? ATTR_PMSGERROR : severity == '2' ? ATTR_PMSGWARN
                                                       : severity == '1' ? ATTR_PMSGINFO : ATTR_STATAREA));
      break;
   }
}

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
   const char *message = "";

   if (out == NULL || out_len == 0)
      return;
   out[0] = '\0';
   if (scrno >= MAX_SCREENS || view == NULL)
      return;

   screen_view = screen[scrno].screen_view;
   if (screen_view == NULL)
      return;
   if (last_message != NULL)
      message = (const char *)last_message;
   snprintf(out, out_len,
            "focus=%s line=%ld row=%d cell=%d verify=%ld message=%s",
            logical_cursor_zone_name(view->cursor.zone),
            (long)view->cursor.line_number,
            view->cursor.zone_row,
            view->cursor.text.cell_column,
            (long)screen_view->verify_col,
            message);
}

static void llm_runtime_file_path(const FILE_DETAILS *file,
                                  char *out, size_t out_len)
{
   if (out == NULL || out_len == 0)
      return;
   out[0] = '\0';
   if (file == NULL)
      return;
   if (file->fpath != NULL && file->fname != NULL)
      snprintf(out, out_len, "%s%s", (const char *)file->fpath,
               (const char *)file->fname);
   else if (file->actualfname != NULL)
      snprintf(out, out_len, "%s", (const char *)file->actualfname);
   else if (file->fname != NULL)
      snprintf(out, out_len, "%s", (const char *)file->fname);
}

static void llm_runtime_buffer_metadata(LlmDriverScreenView *view)
{
   VIEW_DETAILS *vd;
   char path[LLM_DRIVER_MAX_PATH + 1];

   if (view == NULL)
      return;
   if (vd_current != NULL && CURRENT_FILE != NULL)
   {
      llm_runtime_file_path(CURRENT_FILE, path, sizeof(path));
      llm_driver_screen_view_set_buffer(view, path,
                                        the_editor_buffer_dirty(CURRENT_FILE),
                                        (size_t)CURRENT_FILE->number_lines);
   }
   for (vd = vd_first; vd != NULL; vd = vd->next)
   {
      FILE_DETAILS *file = vd->file_for_view;

      if (file == NULL)
         continue;
      llm_runtime_file_path(file, path, sizeof(path));
      llm_driver_screen_view_add_buffer_info(view, path,
                                             the_editor_buffer_dirty(file),
                                             (size_t)file->number_lines,
                                             vd == CURRENT_VIEW);
   }
}

static void llm_runtime_selection_metadata(LlmDriverScreenView *view)
{
   int active;

   if (view == NULL || CURRENT_VIEW == NULL)
      return;
   active = CURRENT_VIEW->mark_type != M_NONE
         && (CURRENT_VIEW->marked_line || CURRENT_VIEW->marked_col);
   llm_driver_screen_view_set_selection(view, active,
                                        CURRENT_VIEW->mark_start_line,
                                        (int)CURRENT_VIEW->mark_start_col,
                                        CURRENT_VIEW->mark_end_line,
                                        (int)CURRENT_VIEW->mark_end_col,
                                        "");
}

static void llm_runtime_parser_diagnostics(LlmDriverScreenView *view)
{
   TheParserDiagnostic *diagnostics = NULL;
   int count = 0;
   int i;

   if (view == NULL)
      return;
   if (the_parser_diagnostics_collect(&diagnostics, &count) != RC_OK)
      return;
   for (i = 0; i < count; i++)
   {
      llm_driver_screen_view_add_diagnostic(view, diagnostics[i].line,
                                            diagnostics[i].column,
                                            diagnostics[i].severity,
                                            diagnostics[i].code,
                                            diagnostics[i].message);
   }
   the_parser_diagnostics_free(diagnostics);
}

static void llm_runtime_overlay_active_prefix(CHARTYPE scrno, UiFrame *frame)
{
   size_t i;

   if (frame == NULL || scrno != current_screen || CURRENT_VIEW == NULL
   ||  CURRENT_VIEW->current_window != WINDOW_PREFIX || pre_rec_len == 0)
      return;
   for (i = 0; i < frame->row_count; i++)
   {
      if (frame->row[i].line_number == CURRENT_VIEW->focus_line)
      {
         ui_frame_set_row_prefix(frame, i, pre_rec, (size_t)pre_rec_len,
                                 frame->row[i].prefix_editable);
         return;
      }
   }
}

int llm_runtime_screen_view(CHARTYPE scrno, LlmDriverScreenView *view)
{
   UiFrame *frame;
   char command[LLM_DRIVER_MAX_COMMAND + 1];
   char status[LLM_DRIVER_MAX_COMMAND + 1];
   CHARTYPE status_line[LLM_DRIVER_MAX_COLS + 1];

   if (view == NULL)
      return 0;
   frame = (UiFrame *)malloc(sizeof(*frame));
   if (frame == NULL)
      return 0;
   if (!screenframe_build(scrno, frame))
   {
      free(frame);
      return 0;
   }
   llm_runtime_overlay_active_prefix(scrno, frame);
   if (!llm_driver_screen_view_from_frame(frame, view))
   {
      free(frame);
      return 0;
   }
   {
      size_t row;
      int columns=min(view->cols,LLM_DRIVER_MAX_COLS);
      for (row=0;row<view->line_count;row++)
         if ((view->lines[row].role==UI_ROW_SCALE || view->lines[row].role==UI_ROW_TABLINE)
             && !view->lines[row].text[0])
         {
            screenframe_format_ruler(SCREEN_VIEW(scrno),screen[scrno].sl[row].line_type,
                                    (CHARTYPE *)view->lines[row].text,columns);
            view->lines[row].text[columns]=0;
         }
   }
   if (!view->cursor.valid && CURRENT_VIEW != NULL
   &&  CURRENT_VIEW->logical_cursor.current.valid)
   {
      view->cursor = CURRENT_VIEW->logical_cursor.current;
      view->cursor_screen_row = view->cursor.zone_row;
      view->cursor_screen_col = view->cursor.text.cell_column;
   }
   free(frame);

   llm_runtime_visible_command(command, sizeof(command));
   llm_driver_screen_view_set_command(view, command);
   llm_runtime_status(scrno, view, status, sizeof(status));
   llm_driver_screen_view_set_status(view, status);
   llm_driver_screen_view_set_status_line(view,
      format_statarea(status_line, sizeof(status_line))
         ? (const char *)status_line : NULL);
   llm_runtime_diagnostic_presentation(view);
   llm_driver_screen_view_set_last_message(view, last_message_serial,
                                           (const char *)last_message);
   llm_runtime_buffer_metadata(view);
   llm_runtime_selection_metadata(view);
   llm_runtime_parser_diagnostics(view);
   return 1;
}

size_t llm_runtime_format_screen(CHARTYPE scrno,
                                 const LlmDriverFormatOptions *options,
                                 char *out, size_t out_len)
{
   LlmDriverScreenView *view;
   size_t formatted;

   if (out == NULL || out_len == 0)
      return 0;
   out[0] = '\0';
   view = (LlmDriverScreenView *)malloc(sizeof(*view));
   if (view == NULL)
      return 0;
   if (!llm_runtime_screen_view(scrno, view))
   {
      free(view);
      return 0;
   }
   formatted = llm_driver_format_semantic_view_with_options(view, options,
                                                            out, out_len);
   free(view);
   if (the_panel_active && formatted > 0)
   {
      size_t end = strlen(out), n;
      while (end && (out[end-1]=='\n' || out[end-1]=='\r' || out[end-1]==' ')) end--;
      if (end && out[end-1]=='}' && out_len-end>16)
      {
         end--;
         memcpy(out+end, ",\"panel\":", 9); end+=9;
         n=panel_ui_json(the_panel_active,out+end,out_len-end-2);
         if (n) { end+=n; out[end++]='}'; out[end]=0; formatted=end; }
         else { out[0]=0; formatted=0; }
      }
   }
   return formatted;
}
