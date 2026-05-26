#include "transientui.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

static void transient_copy(char *dest, size_t dest_len, const char *src)
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

static void transient_copy_n(char *dest, size_t dest_len, const char *src,
                             size_t src_len)
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

static int appendf(char *out, size_t out_len, size_t *used,
                   const char *fmt, ...)
{
   va_list args;
   int written;

   if (out == NULL || used == NULL || out_len == 0 || *used >= out_len)
      return 0;
   va_start(args, fmt);
   written = vsnprintf(out + *used, out_len - *used, fmt, args);
   va_end(args);
   if (written < 0)
      return 0;
   if ((size_t)written >= out_len - *used)
   {
      *used = out_len - 1;
      out[*used] = '\0';
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
            else if (!appendf(out, out_len, used, "%c", *ptr))
               return 0;
            break;
      }
   }
   return appendf(out, out_len, used, "\"");
}

static int clamp_int(int value, int low, int high)
{
   if (value < low)
      return low;
   if (value > high)
      return high;
   return value;
}

static TransientUiRow *add_row(TransientUiSnapshot *snapshot,
                               TransientUiRowRole role, int row, int col,
                               int width, int index, int viewport_index,
                               const char *text)
{
   TransientUiRow *out;

   if (snapshot == NULL || snapshot->row_count >= TRANSIENT_UI_MAX_ROWS)
      return NULL;
   out = &snapshot->rows[snapshot->row_count++];
   memset(out, 0, sizeof(*out));
   out->role = role;
   out->row = row;
   out->col = col;
   out->width = width;
   out->index = index;
   out->viewport_index = viewport_index;
   transient_copy(out->text, sizeof(out->text), text);
   return out;
}

static TransientUiHitTarget *add_hit(TransientUiSnapshot *snapshot,
                                     TransientUiHitKind kind, int row_start,
                                     int row_end, int col_start, int col_end,
                                     int index, const char *label)
{
   TransientUiHitTarget *out;

   if (snapshot == NULL || snapshot->hit_count >= TRANSIENT_UI_MAX_HITS)
      return NULL;
   out = &snapshot->hits[snapshot->hit_count++];
   memset(out, 0, sizeof(*out));
   out->kind = kind;
   out->row_start = row_start;
   out->row_end = row_end;
   out->col_start = col_start;
   out->col_end = col_end;
   out->index = index;
   out->screen_row = snapshot->geometry.top + row_start;
   out->screen_col = snapshot->geometry.left + col_start;
   transient_copy(out->label, sizeof(out->label), label);
   return out;
}

static void add_button(TransientUiSnapshot *snapshot, int index, int row,
                       int col, int width, int active, int selected,
                       const char *text)
{
   TransientUiButton *button;
   TransientUiRow *row_entry;

   if (snapshot == NULL || snapshot->button_count >= TRANSIENT_UI_MAX_BUTTONS)
      return;
   button = &snapshot->buttons[snapshot->button_count++];
   memset(button, 0, sizeof(*button));
   button->index = index;
   button->row = row;
   button->col = col;
   button->width = width;
   button->active = active;
   button->selected = selected;
   transient_copy(button->text, sizeof(button->text), text);

   row_entry = add_row(snapshot, TRANSIENT_UI_ROW_BUTTON, row, col, width,
                       index, -1, text);
   if (row_entry != NULL)
   {
      row_entry->active = active;
      row_entry->selected = selected;
      row_entry->focused = active;
   }
   add_hit(snapshot, TRANSIENT_UI_HIT_BUTTON, row, row, col, col + width - 1,
           index, "button");
}

static int is_separator(const char * const *items, int index, int item_count)
{
   if (items == NULL || index < 0 || index >= item_count || items[index] == NULL)
      return 0;
   return items[index][0] == '-';
}

static int next_selectable(const char * const *items, int item_count,
                           int current, int direction)
{
   int i;

   if (item_count <= 0 || direction == 0)
      return -1;
   i = current;
   while (i >= 0 && i < item_count)
   {
      if (!is_separator(items, i, item_count))
         return i;
      i += direction;
   }
   return -1;
}

static int visible_popup_rows(const TransientUiPopupState *state)
{
   if (state == NULL || state->height <= 2)
      return 0;
   return state->height - 2;
}

static void clamp_popup_offsets(TransientUiPopupState *state)
{
   int visible_rows;
   int max_y;
   int max_x;

   if (state == NULL)
      return;
   visible_rows = visible_popup_rows(state);
   max_y = state->item_count - visible_rows;
   max_x = state->pad_width - state->width;
   if (max_y < 0)
      max_y = 0;
   if (max_x < 0)
      max_x = 0;
   state->y_offset = clamp_int(state->y_offset, 0, max_y);
   state->x_offset = clamp_int(state->x_offset, 0, max_x);
}

static void ensure_popup_highlight_visible(TransientUiPopupState *state)
{
   int visible_rows;

   if (state == NULL)
      return;
   visible_rows = visible_popup_rows(state);
   if (visible_rows <= 0)
      return;
   if (state->highlighted_item < state->y_offset)
      state->y_offset = state->highlighted_item;
   else if (state->highlighted_item >= state->y_offset + visible_rows)
      state->y_offset = state->highlighted_item - visible_rows + 1;
   clamp_popup_offsets(state);
}

const char *transient_ui_kind_name(TransientUiKind kind)
{
   switch (kind)
   {
      case TRANSIENT_UI_KIND_READV:
         return "readv";
      case TRANSIENT_UI_KIND_DIALOG:
         return "dialog";
      case TRANSIENT_UI_KIND_POPUP:
         return "popup";
      case TRANSIENT_UI_KIND_NONE:
      default:
         return "none";
   }
}

const char *transient_ui_focus_name(TransientUiFocus focus)
{
   switch (focus)
   {
      case TRANSIENT_UI_FOCUS_READV_EDIT:
         return "readv-edit";
      case TRANSIENT_UI_FOCUS_DIALOG_EDIT:
         return "dialog-edit";
      case TRANSIENT_UI_FOCUS_DIALOG_BUTTON:
         return "dialog-button";
      case TRANSIENT_UI_FOCUS_POPUP_ITEM:
         return "popup-item";
      case TRANSIENT_UI_FOCUS_NONE:
      default:
         return "none";
   }
}

const char *transient_ui_row_role_name(TransientUiRowRole role)
{
   switch (role)
   {
      case TRANSIENT_UI_ROW_TITLE:
         return "title";
      case TRANSIENT_UI_ROW_PROMPT:
         return "prompt";
      case TRANSIENT_UI_ROW_EDIT:
         return "edit";
      case TRANSIENT_UI_ROW_BUTTON:
         return "button";
      case TRANSIENT_UI_ROW_POPUP_ITEM:
         return "popup-item";
      case TRANSIENT_UI_ROW_POPUP_SEPARATOR:
         return "popup-separator";
      case TRANSIENT_UI_ROW_EMPTY:
      default:
         return "empty";
   }
}

const char *transient_ui_hit_kind_name(TransientUiHitKind kind)
{
   switch (kind)
   {
      case TRANSIENT_UI_HIT_EDIT:
         return "edit";
      case TRANSIENT_UI_HIT_BUTTON:
         return "button";
      case TRANSIENT_UI_HIT_POPUP_ITEM:
         return "popup-item";
      case TRANSIENT_UI_HIT_BORDER:
         return "border";
      case TRANSIENT_UI_HIT_OUTSIDE:
         return "outside";
      case TRANSIENT_UI_HIT_NONE:
      default:
         return "none";
   }
}

void transient_ui_snapshot_init(TransientUiSnapshot *snapshot,
                                TransientUiKind kind, int top, int left,
                                int rows, int cols)
{
   if (snapshot == NULL)
      return;
   memset(snapshot, 0, sizeof(*snapshot));
   snapshot->kind = kind;
   snapshot->geometry.top = top;
   snapshot->geometry.left = left;
   snapshot->geometry.rows = rows;
   snapshot->geometry.cols = cols;
   snapshot->focus = TRANSIENT_UI_FOCUS_NONE;
   snapshot->selected_index = -1;
   snapshot->active_index = -1;
   snapshot->edit_cursor_cell = -1;
}

int transient_ui_hit_test(const TransientUiSnapshot *snapshot, int row, int col,
                          TransientUiHitTarget *hit)
{
   size_t i;

   if (snapshot == NULL)
      return 0;
   for (i = 0; i < snapshot->hit_count; i++)
   {
      const TransientUiHitTarget *candidate = &snapshot->hits[i];

      if (row >= candidate->row_start && row <= candidate->row_end
      &&  col >= candidate->col_start && col <= candidate->col_end)
      {
         if (hit != NULL)
            *hit = *candidate;
         return 1;
      }
   }
   if (hit != NULL)
   {
      memset(hit, 0, sizeof(*hit));
      hit->kind = TRANSIENT_UI_HIT_OUTSIDE;
      hit->row_start = row;
      hit->row_end = row;
      hit->col_start = col;
      hit->col_end = col;
      hit->index = -1;
      hit->screen_row = snapshot->geometry.top + row;
      hit->screen_col = snapshot->geometry.left + col;
      transient_copy(hit->label, sizeof(hit->label), "outside");
   }
   return 0;
}

size_t transient_ui_format_snapshot(const TransientUiSnapshot *snapshot,
                                    char *out, size_t out_len)
{
   size_t used = 0;
   size_t i;

   if (out == NULL || out_len == 0)
      return 0;
   out[0] = '\0';
   if (snapshot == NULL)
      return 0;

   appendf(out, out_len, &used,
           "{\"kind\":\"%s\",\"geometry\":{\"top\":%d,\"left\":%d,"
           "\"rows\":%d,\"cols\":%d},\"focus\":\"%s\","
           "\"active\":%d,\"selected\":%d,\"viewport\":{\"row\":%d,"
           "\"col\":%d},\"edit_cursor\":%d",
           transient_ui_kind_name(snapshot->kind),
           snapshot->geometry.top, snapshot->geometry.left,
           snapshot->geometry.rows, snapshot->geometry.cols,
           transient_ui_focus_name(snapshot->focus),
           snapshot->active_index, snapshot->selected_index,
           snapshot->viewport_row_offset, snapshot->viewport_col_offset,
           snapshot->edit_cursor_cell);
   if (snapshot->title[0] != '\0')
   {
      appendf(out, out_len, &used, ",\"title\":");
      append_json_string(out, out_len, &used, snapshot->title);
   }
   if (snapshot->prompt[0] != '\0')
   {
      appendf(out, out_len, &used, ",\"prompt\":");
      append_json_string(out, out_len, &used, snapshot->prompt);
   }
   if (snapshot->edit_text[0] != '\0'
   ||  snapshot->focus == TRANSIENT_UI_FOCUS_READV_EDIT
   ||  snapshot->focus == TRANSIENT_UI_FOCUS_DIALOG_EDIT)
   {
      appendf(out, out_len, &used, ",\"edit_text\":");
      append_json_string(out, out_len, &used, snapshot->edit_text);
   }

   appendf(out, out_len, &used, ",\"rows\":[");
   for (i = 0; i < snapshot->row_count; i++)
   {
      const TransientUiRow *row = &snapshot->rows[i];

      appendf(out, out_len, &used,
              "%s{\"role\":\"%s\",\"row\":%d,\"col\":%d,\"width\":%d,"
              "\"index\":%d,\"viewport_index\":%d,\"active\":%d,"
              "\"selected\":%d,\"focused\":%d,\"text\":",
              i > 0 ? "," : "", transient_ui_row_role_name(row->role),
              row->row, row->col, row->width, row->index,
              row->viewport_index, row->active, row->selected, row->focused);
      append_json_string(out, out_len, &used, row->text);
      appendf(out, out_len, &used, "}");
   }
   appendf(out, out_len, &used, "],\"buttons\":[");
   for (i = 0; i < snapshot->button_count; i++)
   {
      const TransientUiButton *button = &snapshot->buttons[i];

      appendf(out, out_len, &used,
              "%s{\"index\":%d,\"row\":%d,\"col\":%d,\"width\":%d,"
              "\"active\":%d,\"selected\":%d,\"text\":",
              i > 0 ? "," : "", button->index, button->row, button->col,
              button->width, button->active, button->selected);
      append_json_string(out, out_len, &used, button->text);
      appendf(out, out_len, &used, "}");
   }
   appendf(out, out_len, &used, "],\"hits\":[");
   for (i = 0; i < snapshot->hit_count; i++)
   {
      const TransientUiHitTarget *hit = &snapshot->hits[i];

      appendf(out, out_len, &used,
              "%s{\"kind\":\"%s\",\"row_start\":%d,\"row_end\":%d,"
              "\"col_start\":%d,\"col_end\":%d,\"index\":%d,"
              "\"screen_row\":%d,\"screen_col\":%d,\"label\":",
              i > 0 ? "," : "", transient_ui_hit_kind_name(hit->kind),
              hit->row_start, hit->row_end, hit->col_start, hit->col_end,
              hit->index, hit->screen_row, hit->screen_col);
      append_json_string(out, out_len, &used, hit->label);
      appendf(out, out_len, &used, "}");
   }
   appendf(out, out_len, &used, "]}");
   return used;
}

void transient_ui_readv_state_init(TransientUiReadvState *state,
                                   const char *initial, int cursor_cell,
                                   int start_col, int cols)
{
   size_t len;

   if (state == NULL)
      return;
   memset(state, 0, sizeof(*state));
   transient_copy(state->text, sizeof(state->text), initial);
   len = strlen(state->text);
   if (cursor_cell < 0)
      cursor_cell = (int)len;
   state->cursor_cell = clamp_int(cursor_cell, 0, (int)len);
   state->start_col = start_col < 0 ? 0 : start_col;
   state->cols = cols < 0 ? 0 : cols;
}

TransientUiAction transient_ui_readv_handle_key(TransientUiReadvState *state,
                                                TransientUiKey key)
{
   size_t len;

   if (state == NULL)
      return TRANSIENT_UI_ACTION_NONE;
   len = strlen(state->text);
   switch (key)
   {
      case TRANSIENT_UI_KEY_LEFT:
         if (state->cursor_cell > 0)
            state->cursor_cell--;
         break;
      case TRANSIENT_UI_KEY_RIGHT:
         if (state->cursor_cell < (int)len)
            state->cursor_cell++;
         break;
      case TRANSIENT_UI_KEY_HOME:
         state->cursor_cell = 0;
         break;
      case TRANSIENT_UI_KEY_END:
         state->cursor_cell = (int)len;
         break;
      case TRANSIENT_UI_KEY_BACKSPACE:
         if (state->cursor_cell > 0)
         {
            memmove(state->text + state->cursor_cell - 1,
                    state->text + state->cursor_cell,
                    len - (size_t)state->cursor_cell + 1);
            state->cursor_cell--;
         }
         break;
      case TRANSIENT_UI_KEY_DELETE:
         if (state->cursor_cell < (int)len)
            memmove(state->text + state->cursor_cell,
                    state->text + state->cursor_cell + 1,
                    len - (size_t)state->cursor_cell);
         break;
      case TRANSIENT_UI_KEY_ENTER:
         return TRANSIENT_UI_ACTION_ACCEPT;
      case TRANSIENT_UI_KEY_ESCAPE:
      case TRANSIENT_UI_KEY_QUIT:
         return TRANSIENT_UI_ACTION_CANCEL;
      default:
         break;
   }
   return TRANSIENT_UI_ACTION_NONE;
}

int transient_ui_readv_insert_text(TransientUiReadvState *state,
                                   const char *text)
{
   size_t len;
   size_t text_len;

   if (state == NULL || text == NULL)
      return 0;
   len = strlen(state->text);
   text_len = strlen(text);
   if (text_len == 0)
      return 1;
   if (len + text_len >= sizeof(state->text))
      return 0;
   state->cursor_cell = clamp_int(state->cursor_cell, 0, (int)len);
   memmove(state->text + state->cursor_cell + text_len,
           state->text + state->cursor_cell,
           len - (size_t)state->cursor_cell + 1);
   memcpy(state->text + state->cursor_cell, text, text_len);
   state->cursor_cell += (int)text_len;
   return 1;
}

void transient_ui_snapshot_build_readv(TransientUiSnapshot *snapshot,
                                       int top, int left, int cols,
                                       const TransientUiReadvState *state)
{
   TransientUiRow *row;

   transient_ui_snapshot_init(snapshot, TRANSIENT_UI_KIND_READV, top, left,
                              1, cols);
   if (snapshot == NULL || state == NULL)
      return;
   snapshot->focus = TRANSIENT_UI_FOCUS_READV_EDIT;
   snapshot->edit_cursor_cell = state->cursor_cell;
   transient_copy(snapshot->edit_text, sizeof(snapshot->edit_text),
                  state->text);
   row = add_row(snapshot, TRANSIENT_UI_ROW_EDIT, 0, state->start_col,
                 cols > 0 ? cols : (int)strlen(state->text), 0, -1,
                 state->text);
   if (row != NULL)
      row->focused = 1;
   add_hit(snapshot, TRANSIENT_UI_HIT_EDIT, 0, 0, 0,
           cols > 0 ? cols - 1 : TRANSIENT_UI_MAX_TEXT - 1, 0, "readv-edit");
}

void transient_ui_dialog_state_init(TransientUiDialogState *state,
                                    int has_editfield, int button_count,
                                    int active_button, const char *edit_text)
{
   if (state == NULL)
      return;
   memset(state, 0, sizeof(*state));
   state->has_editfield = has_editfield ? 1 : 0;
   state->button_count = button_count < 0 ? 0 : button_count;
   if (state->button_count > TRANSIENT_UI_MAX_BUTTONS)
      state->button_count = TRANSIENT_UI_MAX_BUTTONS;
   state->selected_button = -1;
   transient_ui_readv_state_init(&state->edit, edit_text, -1, 0, 0);
   if (state->has_editfield && active_button < 0)
   {
      state->focus = TRANSIENT_UI_FOCUS_DIALOG_EDIT;
      state->active_button = -1;
   }
   else
   {
      state->focus = TRANSIENT_UI_FOCUS_DIALOG_BUTTON;
      state->active_button = clamp_int(active_button, 0,
                                       state->button_count > 0
                                       ? state->button_count - 1 : 0);
   }
}

TransientUiAction transient_ui_dialog_handle_key(
   TransientUiDialogState *state, TransientUiKey key)
{
   if (state == NULL)
      return TRANSIENT_UI_ACTION_NONE;
   switch (key)
   {
      case TRANSIENT_UI_KEY_TAB:
         if (state->focus == TRANSIENT_UI_FOCUS_DIALOG_EDIT)
         {
            state->focus = TRANSIENT_UI_FOCUS_DIALOG_BUTTON;
            state->active_button = 0;
         }
         else
         {
            state->active_button++;
            if (state->active_button >= state->button_count)
            {
               if (state->has_editfield)
               {
                  state->focus = TRANSIENT_UI_FOCUS_DIALOG_EDIT;
                  state->active_button = -1;
               }
               else
                  state->active_button = 0;
            }
         }
         return TRANSIENT_UI_ACTION_FOCUS_CHANGED;
      case TRANSIENT_UI_KEY_BACKTAB:
         if (state->focus == TRANSIENT_UI_FOCUS_DIALOG_EDIT)
         {
            state->focus = TRANSIENT_UI_FOCUS_DIALOG_BUTTON;
            state->active_button = state->button_count - 1;
         }
         else
         {
            state->active_button--;
            if (state->active_button < 0)
            {
               if (state->has_editfield)
               {
                  state->focus = TRANSIENT_UI_FOCUS_DIALOG_EDIT;
                  state->active_button = -1;
               }
               else
                  state->active_button = state->button_count - 1;
            }
         }
         return TRANSIENT_UI_ACTION_FOCUS_CHANGED;
      case TRANSIENT_UI_KEY_ENTER:
      case TRANSIENT_UI_KEY_QUIT:
         if (state->focus == TRANSIENT_UI_FOCUS_DIALOG_BUTTON
         &&  state->active_button >= 0
         &&  state->active_button < state->button_count)
         {
            state->selected_button = state->active_button;
            return TRANSIENT_UI_ACTION_ACCEPT;
         }
         if (state->focus == TRANSIENT_UI_FOCUS_DIALOG_EDIT)
         {
            state->focus = TRANSIENT_UI_FOCUS_DIALOG_BUTTON;
            state->active_button = 0;
            return TRANSIENT_UI_ACTION_FOCUS_CHANGED;
         }
         break;
      case TRANSIENT_UI_KEY_ESCAPE:
         return TRANSIENT_UI_ACTION_CANCEL;
      default:
         break;
   }
   return TRANSIENT_UI_ACTION_NONE;
}

TransientUiAction transient_ui_dialog_handle_hit(
   TransientUiDialogState *state, const TransientUiSnapshot *snapshot,
   int row, int col)
{
   TransientUiHitTarget hit;

   if (state == NULL || snapshot == NULL)
      return TRANSIENT_UI_ACTION_NONE;
   if (!transient_ui_hit_test(snapshot, row, col, &hit))
      return TRANSIENT_UI_ACTION_NONE;
   if (hit.kind == TRANSIENT_UI_HIT_EDIT && state->has_editfield)
   {
      state->focus = TRANSIENT_UI_FOCUS_DIALOG_EDIT;
      state->active_button = -1;
      state->edit.cursor_cell = col - hit.col_start;
      if (state->edit.cursor_cell < 0)
         state->edit.cursor_cell = 0;
      return TRANSIENT_UI_ACTION_FOCUS_CHANGED;
   }
   if (hit.kind == TRANSIENT_UI_HIT_BUTTON
   &&  hit.index >= 0
   &&  hit.index < state->button_count)
   {
      state->focus = TRANSIENT_UI_FOCUS_DIALOG_BUTTON;
      state->active_button = hit.index;
      state->selected_button = hit.index;
      return TRANSIENT_UI_ACTION_ACCEPT;
   }
   return TRANSIENT_UI_ACTION_NONE;
}

void transient_ui_snapshot_build_dialog(
   TransientUiSnapshot *snapshot, int top, int left, int rows, int cols,
   const char *title, const char * const *prompt_lines, size_t prompt_count,
   const char *edit_text, int edit_cursor_cell, int has_editfield,
   const TransientUiButtonSpec *buttons, size_t button_count,
   const TransientUiDialogState *state)
{
   size_t i;
   TransientUiRow *row;
   TransientUiFocus focus = TRANSIENT_UI_FOCUS_NONE;
   int active_button = -1;
   int selected_button = -1;
   int edit_row;

   transient_ui_snapshot_init(snapshot, TRANSIENT_UI_KIND_DIALOG, top, left,
                              rows, cols);
   if (snapshot == NULL)
      return;
   if (state != NULL)
   {
      focus = state->focus;
      active_button = state->active_button;
      selected_button = state->selected_button;
   }
   snapshot->focus = focus;
   snapshot->active_index = active_button;
   snapshot->selected_index = selected_button;
   snapshot->edit_cursor_cell = edit_cursor_cell;
   transient_copy(snapshot->title, sizeof(snapshot->title), title);
   if (prompt_lines != NULL && prompt_count > 0)
      transient_copy(snapshot->prompt, sizeof(snapshot->prompt),
                     prompt_lines[0]);

   if (title != NULL && title[0] != '\0')
      add_row(snapshot, TRANSIENT_UI_ROW_TITLE, 0, 1, (int)strlen(title),
              0, -1, title);
   for (i = 0; prompt_lines != NULL && i < prompt_count; i++)
   {
      add_row(snapshot, TRANSIENT_UI_ROW_PROMPT, 2 + (int)i, 2,
              (int)strlen(prompt_lines[i]), (int)i, -1, prompt_lines[i]);
   }
   if (has_editfield)
   {
      edit_row = 3 + (int)prompt_count;
      transient_copy(snapshot->edit_text, sizeof(snapshot->edit_text),
                     edit_text);
      row = add_row(snapshot, TRANSIENT_UI_ROW_EDIT, edit_row, 2,
                    cols > 4 ? cols - 4 : 0, 0, -1, edit_text);
      if (row != NULL)
         row->focused = focus == TRANSIENT_UI_FOCUS_DIALOG_EDIT;
      add_hit(snapshot, TRANSIENT_UI_HIT_EDIT, edit_row, edit_row, 2,
              cols > 2 ? cols - 3 : 2, 0, "dialog-edit");
   }
   for (i = 0; buttons != NULL && i < button_count
        && i < TRANSIENT_UI_MAX_BUTTONS; i++)
   {
      add_button(snapshot, (int)i, buttons[i].row, buttons[i].col,
                 buttons[i].width,
                 focus == TRANSIENT_UI_FOCUS_DIALOG_BUTTON
                 && active_button == (int)i,
                 selected_button == (int)i, buttons[i].text);
   }
}

void transient_ui_popup_state_init(TransientUiPopupState *state, int height,
                                   int width, int pad_height, int pad_width,
                                   int initial, int item_count,
                                   const char * const *items)
{
   int highlighted;
   int selectable;

   if (state == NULL)
      return;
   memset(state, 0, sizeof(*state));
   state->height = height;
   state->width = width;
   state->pad_height = pad_height;
   state->pad_width = pad_width;
   state->item_count = item_count < 0 ? 0 : item_count;
   state->selected_item = -1;
   state->escape_key_index = 0;

   highlighted = initial <= 0 ? 0 : initial - 1;
   if (highlighted >= state->item_count)
      highlighted = state->item_count - 1;
   if (highlighted < 0)
      highlighted = 0;
   selectable = next_selectable(items, state->item_count, highlighted, 1);
   if (selectable < 0)
      selectable = next_selectable(items, state->item_count, highlighted, -1);
   state->highlighted_item = selectable >= 0 ? selectable : highlighted;

   if (initial > 0 && visible_popup_rows(state) > 0
   &&  state->highlighted_item + 2 >= state->height)
      state->y_offset = state->highlighted_item - state->height / 2;
   clamp_popup_offsets(state);
   ensure_popup_highlight_visible(state);
}

TransientUiAction transient_ui_popup_handle_key(TransientUiPopupState *state,
                                                const char * const *items,
                                                TransientUiKey key)
{
   int candidate;
   int visible_rows;

   if (state == NULL)
      return TRANSIENT_UI_ACTION_NONE;
   visible_rows = visible_popup_rows(state);
   switch (key)
   {
      case TRANSIENT_UI_KEY_TAB:
      case TRANSIENT_UI_KEY_DOWN:
         candidate = next_selectable(items, state->item_count,
                                     state->highlighted_item + 1, 1);
         if (candidate >= 0)
         {
            state->highlighted_item = candidate;
            ensure_popup_highlight_visible(state);
         }
         break;
      case TRANSIENT_UI_KEY_UP:
         candidate = next_selectable(items, state->item_count,
                                     state->highlighted_item - 1, -1);
         if (candidate >= 0)
         {
            state->highlighted_item = candidate;
            ensure_popup_highlight_visible(state);
         }
         break;
      case TRANSIENT_UI_KEY_PAGEDOWN:
         candidate = state->highlighted_item + visible_rows;
         if (candidate >= state->item_count)
            candidate = state->item_count - 1;
         candidate = next_selectable(items, state->item_count, candidate, 1);
         if (candidate < 0)
            candidate = next_selectable(items, state->item_count,
                                        state->item_count - 1, -1);
         if (candidate >= 0)
         {
            state->highlighted_item = candidate;
            state->y_offset += visible_rows;
            ensure_popup_highlight_visible(state);
         }
         break;
      case TRANSIENT_UI_KEY_PAGEUP:
         candidate = state->highlighted_item - visible_rows;
         if (candidate < 0)
            candidate = 0;
         candidate = next_selectable(items, state->item_count, candidate, 1);
         if (candidate >= 0 && candidate <= state->highlighted_item)
         {
            state->highlighted_item = candidate;
            state->y_offset -= visible_rows;
            ensure_popup_highlight_visible(state);
         }
         break;
      case TRANSIENT_UI_KEY_RIGHT:
         state->x_offset++;
         clamp_popup_offsets(state);
         break;
      case TRANSIENT_UI_KEY_LEFT:
         state->x_offset--;
         clamp_popup_offsets(state);
         break;
      case TRANSIENT_UI_KEY_HOME:
         candidate = next_selectable(items, state->item_count, 0, 1);
         if (candidate >= 0)
         {
            state->highlighted_item = candidate;
            state->y_offset = 0;
         }
         break;
      case TRANSIENT_UI_KEY_END:
         candidate = next_selectable(items, state->item_count,
                                     state->item_count - 1, -1);
         if (candidate >= 0)
         {
            state->highlighted_item = candidate;
            ensure_popup_highlight_visible(state);
         }
         break;
      case TRANSIENT_UI_KEY_ENTER:
         state->selected_item = state->highlighted_item;
         return TRANSIENT_UI_ACTION_ACCEPT;
      case TRANSIENT_UI_KEY_ESCAPE:
      case TRANSIENT_UI_KEY_QUIT:
         return TRANSIENT_UI_ACTION_CANCEL;
      default:
         break;
   }
   return TRANSIENT_UI_ACTION_NONE;
}

TransientUiAction transient_ui_popup_handle_hit(TransientUiPopupState *state,
                                                const TransientUiSnapshot *snapshot,
                                                int row, int col)
{
   TransientUiHitTarget hit;

   if (state == NULL || snapshot == NULL)
      return TRANSIENT_UI_ACTION_NONE;
   if (!transient_ui_hit_test(snapshot, row, col, &hit))
      return TRANSIENT_UI_ACTION_CANCEL;
   if (hit.kind == TRANSIENT_UI_HIT_POPUP_ITEM
   &&  hit.index >= 0
   &&  hit.index < state->item_count)
   {
      state->highlighted_item = hit.index;
      state->selected_item = hit.index;
      return TRANSIENT_UI_ACTION_ACCEPT;
   }
   return TRANSIENT_UI_ACTION_NONE;
}

void transient_ui_snapshot_build_popup(TransientUiSnapshot *snapshot, int top,
                                       int left,
                                       const TransientUiPopupState *state,
                                       const char * const *items)
{
   int visible_rows;
   int row;

   transient_ui_snapshot_init(snapshot, TRANSIENT_UI_KIND_POPUP, top, left,
                              state == NULL ? 0 : state->height,
                              state == NULL ? 0 : state->width);
   if (snapshot == NULL || state == NULL)
      return;
   snapshot->focus = TRANSIENT_UI_FOCUS_POPUP_ITEM;
   snapshot->viewport_row_offset = state->y_offset;
   snapshot->viewport_col_offset = state->x_offset;
   snapshot->active_index = state->highlighted_item;
   snapshot->selected_index = state->selected_item;
   visible_rows = visible_popup_rows(state);
   for (row = 0; row < visible_rows; row++)
   {
      int item_index = state->y_offset + row;
      const char *text = "";
      TransientUiRowRole role = TRANSIENT_UI_ROW_POPUP_ITEM;
      TransientUiRow *entry;

      if (item_index >= state->item_count)
         break;
      if (items != NULL && items[item_index] != NULL)
         text = items[item_index];
      if (is_separator(items, item_index, state->item_count))
         role = TRANSIENT_UI_ROW_POPUP_SEPARATOR;
      entry = add_row(snapshot, role, row + 1, 1,
                      state->width > 2 ? state->width - 2 : 0,
                      item_index, item_index, text);
      if (entry != NULL)
      {
         entry->active = item_index == state->highlighted_item;
         entry->selected = item_index == state->selected_item;
         entry->focused = entry->active;
      }
      if (role == TRANSIENT_UI_ROW_POPUP_ITEM)
         add_hit(snapshot, TRANSIENT_UI_HIT_POPUP_ITEM, row + 1, row + 1,
                 1, state->width > 2 ? state->width - 2 : 1,
                 item_index, "popup-item");
   }
   add_hit(snapshot, TRANSIENT_UI_HIT_BORDER, 0, 0, 0,
           state->width > 0 ? state->width - 1 : 0, -1, "top-border");
   add_hit(snapshot, TRANSIENT_UI_HIT_BORDER,
           state->height > 0 ? state->height - 1 : 0,
           state->height > 0 ? state->height - 1 : 0, 0,
           state->width > 0 ? state->width - 1 : 0, -1, "bottom-border");
   if (state->height > 2)
   {
      add_hit(snapshot, TRANSIENT_UI_HIT_BORDER, 1, state->height - 2, 0, 0,
              -1, "left-border");
      add_hit(snapshot, TRANSIENT_UI_HIT_BORDER, 1, state->height - 2,
              state->width > 0 ? state->width - 1 : 0,
              state->width > 0 ? state->width - 1 : 0, -1, "right-border");
   }
}
