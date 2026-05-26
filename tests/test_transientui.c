#include <stdio.h>
#include <string.h>

#include "transientui.h"

static int failures = 0;

static void expect_int(const char *name, int got, int want)
{
   if (got != want)
   {
      fprintf(stderr, "%s: got %d want %d\n", name, got, want);
      failures++;
   }
}

static void expect_str(const char *name, const char *got, const char *want)
{
   if (strcmp(got, want) != 0)
   {
      fprintf(stderr, "%s: got \"%s\" want \"%s\"\n", name, got, want);
      failures++;
   }
}

static void expect_contains(const char *name, const char *haystack,
                            const char *needle)
{
   if (strstr(haystack, needle) == NULL)
   {
      fprintf(stderr, "%s: missing \"%s\" in:\n%s\n", name, needle, haystack);
      failures++;
   }
}

static void test_readv_snapshot_and_editing(void)
{
   TransientUiReadvState state;
   TransientUiSnapshot snapshot;
   TransientUiHitTarget hit;
   char out[4096];

   transient_ui_readv_state_init(&state, "abc", 1, 0, 20);
   expect_int("readv.cursor.init", state.cursor_cell, 1);
   expect_int("readv.insert", transient_ui_readv_insert_text(&state, "XY"), 1);
   expect_str("readv.text.insert", state.text, "aXYbc");
   expect_int("readv.cursor.insert", state.cursor_cell, 3);
   transient_ui_readv_handle_key(&state, TRANSIENT_UI_KEY_LEFT);
   transient_ui_readv_handle_key(&state, TRANSIENT_UI_KEY_BACKSPACE);
   expect_str("readv.text.backspace", state.text, "aYbc");
   expect_int("readv.cursor.backspace", state.cursor_cell, 1);

   transient_ui_snapshot_build_readv(&snapshot, 10, 2, 20, &state);
   expect_int("readv.kind", snapshot.kind, TRANSIENT_UI_KIND_READV);
   expect_int("readv.focus", snapshot.focus, TRANSIENT_UI_FOCUS_READV_EDIT);
   expect_int("readv.rows", (int)snapshot.row_count, 1);
   expect_int("readv.hits", (int)snapshot.hit_count, 1);
   expect_int("readv.hit", transient_ui_hit_test(&snapshot, 0, 5, &hit), 1);
   expect_int("readv.hit.kind", hit.kind, TRANSIENT_UI_HIT_EDIT);

   transient_ui_format_snapshot(&snapshot, out, sizeof(out));
   expect_contains("readv.format.kind", out, "\"kind\":\"readv\"");
   expect_contains("readv.format.edit", out, "\"edit_text\":\"aYbc\"");
   expect_contains("readv.format.hit", out, "\"label\":\"readv-edit\"");
}

static void test_dialog_snapshot_navigation_and_hits(void)
{
   const char *prompt[] = { "Choose a mode", "Then confirm" };
   TransientUiButtonSpec buttons[] =
   {
      { " OK ", 7, 4, 4 },
      { " CANCEL ", 7, 14, 8 }
   };
   TransientUiDialogState state;
   TransientUiSnapshot snapshot;
   char out[8192];

   transient_ui_dialog_state_init(&state, 1, 2, -1, "alpha");
   expect_int("dialog.focus.init", state.focus, TRANSIENT_UI_FOCUS_DIALOG_EDIT);
   transient_ui_snapshot_build_dialog(&snapshot, 4, 10, 10, 30, "DIALOG",
                                      prompt, 2, state.edit.text,
                                      state.edit.cursor_cell, 1, buttons, 2,
                                      &state);
   expect_int("dialog.kind", snapshot.kind, TRANSIENT_UI_KIND_DIALOG);
   expect_int("dialog.rows.title", snapshot.rows[0].role,
              TRANSIENT_UI_ROW_TITLE);
   expect_int("dialog.rows.prompt", snapshot.rows[1].role,
              TRANSIENT_UI_ROW_PROMPT);
   expect_int("dialog.rows.edit", snapshot.rows[3].role,
              TRANSIENT_UI_ROW_EDIT);
   expect_int("dialog.buttons", (int)snapshot.button_count, 2);
   expect_int("dialog.hit.edit",
              transient_ui_dialog_handle_hit(&state, &snapshot, 5, 6),
              TRANSIENT_UI_ACTION_FOCUS_CHANGED);
   expect_int("dialog.edit.cursor.hit", state.edit.cursor_cell, 4);

   expect_int("dialog.tab",
              transient_ui_dialog_handle_key(&state, TRANSIENT_UI_KEY_TAB),
              TRANSIENT_UI_ACTION_FOCUS_CHANGED);
   expect_int("dialog.focus.button", state.focus,
              TRANSIENT_UI_FOCUS_DIALOG_BUTTON);
   expect_int("dialog.active.ok", state.active_button, 0);
   expect_int("dialog.tab.cancel",
              transient_ui_dialog_handle_key(&state, TRANSIENT_UI_KEY_TAB),
              TRANSIENT_UI_ACTION_FOCUS_CHANGED);
   expect_int("dialog.active.cancel", state.active_button, 1);

   transient_ui_snapshot_build_dialog(&snapshot, 4, 10, 10, 30, "DIALOG",
                                      prompt, 2, state.edit.text,
                                      state.edit.cursor_cell, 1, buttons, 2,
                                      &state);
   expect_int("dialog.hit.button",
              transient_ui_dialog_handle_hit(&state, &snapshot, 7, 16),
              TRANSIENT_UI_ACTION_ACCEPT);
   expect_int("dialog.selected.cancel", state.selected_button, 1);

   transient_ui_snapshot_build_dialog(&snapshot, 4, 10, 10, 30, "DIALOG",
                                      prompt, 2, state.edit.text,
                                      state.edit.cursor_cell, 1, buttons, 2,
                                      &state);
   transient_ui_format_snapshot(&snapshot, out, sizeof(out));
   expect_contains("dialog.format.kind", out, "\"kind\":\"dialog\"");
   expect_contains("dialog.format.title", out, "\"title\":\"DIALOG\"");
   expect_contains("dialog.format.prompt", out, "\"role\":\"prompt\"");
   expect_contains("dialog.format.button", out, "\"text\":\" CANCEL \"");
   expect_contains("dialog.format.hit", out, "\"kind\":\"button\"");
}

static void test_popup_snapshot_navigation_and_hits(void)
{
   const char *items[] =
   {
      "Alpha",
      "-----",
      "Bravo",
      "Charlie",
      "Delta",
      "Echo"
   };
   TransientUiPopupState state;
   TransientUiSnapshot snapshot;
   char out[8192];

   transient_ui_popup_state_init(&state, 5, 20, 6, 30, 1, 6, items);
   expect_int("popup.highlight.init", state.highlighted_item, 0);
   expect_int("popup.down",
              transient_ui_popup_handle_key(&state, items,
                                            TRANSIENT_UI_KEY_DOWN),
              TRANSIENT_UI_ACTION_NONE);
   expect_int("popup.skips.separator", state.highlighted_item, 2);
   expect_int("popup.right",
              transient_ui_popup_handle_key(&state, items,
                                            TRANSIENT_UI_KEY_RIGHT),
              TRANSIENT_UI_ACTION_NONE);
   expect_int("popup.x.offset", state.x_offset, 1);
   transient_ui_popup_handle_key(&state, items, TRANSIENT_UI_KEY_PAGEDOWN);
   expect_int("popup.highlight.page", state.highlighted_item, 5);
   expect_int("popup.y.offset.visible", state.y_offset, 3);

   transient_ui_snapshot_build_popup(&snapshot, 2, 5, &state, items);
   expect_int("popup.kind", snapshot.kind, TRANSIENT_UI_KIND_POPUP);
   expect_int("popup.viewport.row", snapshot.viewport_row_offset, 3);
   expect_int("popup.viewport.col", snapshot.viewport_col_offset, 1);
   expect_int("popup.visible.rows", (int)snapshot.row_count, 3);
   expect_int("popup.first.visible.index", snapshot.rows[0].index, 3);
   expect_int("popup.active.echo", snapshot.rows[2].active, 1);

   expect_int("popup.hit.accept",
              transient_ui_popup_handle_hit(&state, &snapshot, 2, 3),
              TRANSIENT_UI_ACTION_ACCEPT);
   expect_int("popup.selected.delta", state.selected_item, 4);

   transient_ui_snapshot_build_popup(&snapshot, 2, 5, &state, items);
   transient_ui_format_snapshot(&snapshot, out, sizeof(out));
   expect_contains("popup.format.kind", out, "\"kind\":\"popup\"");
   expect_contains("popup.format.viewport", out,
                   "\"viewport\":{\"row\":3,\"col\":1}");
   expect_contains("popup.format.item", out, "\"role\":\"popup-item\"");
   expect_contains("popup.format.hit", out, "\"kind\":\"popup-item\"");
}

int main(void)
{
   test_readv_snapshot_and_editing();
   test_dialog_snapshot_navigation_and_hits();
   test_popup_snapshot_navigation_and_hits();

   if (failures != 0)
   {
      fprintf(stderr, "Transient UI tests failed: %d\n", failures);
      return 1;
   }
   return 0;
}
