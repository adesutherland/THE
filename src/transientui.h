#ifndef THE_TRANSIENTUI_H
#define THE_TRANSIENTUI_H

#include <stddef.h>
#include <stdint.h>

#include "thedefs.h"

#define TRANSIENT_UI_MAX_ROWS 256
#define TRANSIENT_UI_MAX_BUTTONS 4
#define TRANSIENT_UI_MAX_HITS 320
#define TRANSIENT_UI_MAX_TEXT 1000
#define TRANSIENT_UI_MAX_LABEL 32

typedef enum
{
   TRANSIENT_UI_KIND_NONE = 0,
   TRANSIENT_UI_KIND_READV,
   TRANSIENT_UI_KIND_DIALOG,
   TRANSIENT_UI_KIND_POPUP
} TransientUiKind;

typedef enum
{
   TRANSIENT_UI_FOCUS_NONE = 0,
   TRANSIENT_UI_FOCUS_READV_EDIT,
   TRANSIENT_UI_FOCUS_DIALOG_EDIT,
   TRANSIENT_UI_FOCUS_DIALOG_BUTTON,
   TRANSIENT_UI_FOCUS_POPUP_ITEM
} TransientUiFocus;

typedef enum
{
   TRANSIENT_UI_ROW_EMPTY = 0,
   TRANSIENT_UI_ROW_TITLE,
   TRANSIENT_UI_ROW_PROMPT,
   TRANSIENT_UI_ROW_EDIT,
   TRANSIENT_UI_ROW_BUTTON,
   TRANSIENT_UI_ROW_POPUP_ITEM,
   TRANSIENT_UI_ROW_POPUP_SEPARATOR
} TransientUiRowRole;

typedef enum
{
   TRANSIENT_UI_HIT_NONE = 0,
   TRANSIENT_UI_HIT_EDIT,
   TRANSIENT_UI_HIT_BUTTON,
   TRANSIENT_UI_HIT_POPUP_ITEM,
   TRANSIENT_UI_HIT_BORDER,
   TRANSIENT_UI_HIT_OUTSIDE
} TransientUiHitKind;

typedef enum
{
   TRANSIENT_UI_KEY_NONE = 0,
   TRANSIENT_UI_KEY_TAB,
   TRANSIENT_UI_KEY_BACKTAB,
   TRANSIENT_UI_KEY_UP,
   TRANSIENT_UI_KEY_DOWN,
   TRANSIENT_UI_KEY_LEFT,
   TRANSIENT_UI_KEY_RIGHT,
   TRANSIENT_UI_KEY_PAGEUP,
   TRANSIENT_UI_KEY_PAGEDOWN,
   TRANSIENT_UI_KEY_HOME,
   TRANSIENT_UI_KEY_END,
   TRANSIENT_UI_KEY_ENTER,
   TRANSIENT_UI_KEY_ESCAPE,
   TRANSIENT_UI_KEY_BACKSPACE,
   TRANSIENT_UI_KEY_DELETE,
   TRANSIENT_UI_KEY_QUIT
} TransientUiKey;

typedef enum
{
   TRANSIENT_UI_ACTION_NONE = 0,
   TRANSIENT_UI_ACTION_FOCUS_CHANGED,
   TRANSIENT_UI_ACTION_ACCEPT,
   TRANSIENT_UI_ACTION_CANCEL
} TransientUiAction;

typedef struct
{
   int top;
   int left;
   int rows;
   int cols;
} TransientUiGeometry;

typedef struct
{
   TransientUiRowRole role;
   int row;
   int col;
   int width;
   int index;
   int viewport_index;
   int active;
   int selected;
   int focused;
   char text[TRANSIENT_UI_MAX_TEXT + 1];
} TransientUiRow;

typedef struct
{
   int index;
   int row;
   int col;
   int width;
   int active;
   int selected;
   char text[TRANSIENT_UI_MAX_TEXT + 1];
} TransientUiButton;

typedef struct
{
   TransientUiHitKind kind;
   int row_start;
   int row_end;
   int col_start;
   int col_end;
   int index;
   int screen_row;
   int screen_col;
   char label[TRANSIENT_UI_MAX_LABEL + 1];
} TransientUiHitTarget;

typedef struct
{
   TransientUiKind kind;
   TransientUiGeometry geometry;
   TransientUiFocus focus;
   int viewport_row_offset;
   int viewport_col_offset;
   int selected_index;
   int active_index;
   int edit_cursor_cell;
   char title[TRANSIENT_UI_MAX_TEXT + 1];
   char prompt[TRANSIENT_UI_MAX_TEXT + 1];
   char edit_text[TRANSIENT_UI_MAX_TEXT + 1];
   TransientUiRow rows[TRANSIENT_UI_MAX_ROWS];
   size_t row_count;
   TransientUiButton buttons[TRANSIENT_UI_MAX_BUTTONS];
   size_t button_count;
   TransientUiHitTarget hits[TRANSIENT_UI_MAX_HITS];
   size_t hit_count;
} TransientUiSnapshot;

typedef struct
{
   char text[TRANSIENT_UI_MAX_TEXT + 1];
   int cursor_cell;
   int start_col;
   int cols;
} TransientUiReadvState;

typedef struct
{
   int has_editfield;
   int button_count;
   int active_button;
   int selected_button;
   TransientUiFocus focus;
   TransientUiReadvState edit;
} TransientUiDialogState;

typedef struct
{
   int height;
   int width;
   int pad_height;
   int pad_width;
   int item_count;
   int highlighted_item;
   int selected_item;
   int y_offset;
   int x_offset;
   int escape_key_index;
} TransientUiPopupState;

typedef struct
{
   const char *text;
   int row;
   int col;
   int width;
} TransientUiButtonSpec;

const char *transient_ui_kind_name(TransientUiKind kind);
const char *transient_ui_focus_name(TransientUiFocus focus);
const char *transient_ui_row_role_name(TransientUiRowRole role);
const char *transient_ui_hit_kind_name(TransientUiHitKind kind);

void transient_ui_snapshot_init(TransientUiSnapshot *snapshot,
                                TransientUiKind kind,
                                int top, int left, int rows, int cols);
int transient_ui_hit_test(const TransientUiSnapshot *snapshot, int row, int col,
                          TransientUiHitTarget *hit);
size_t transient_ui_format_snapshot(const TransientUiSnapshot *snapshot,
                                    char *out, size_t out_len);

void transient_ui_readv_state_init(TransientUiReadvState *state,
                                   const char *initial, int cursor_cell,
                                   int start_col, int cols);
TransientUiAction transient_ui_readv_handle_key(TransientUiReadvState *state,
                                                TransientUiKey key);
int transient_ui_readv_insert_text(TransientUiReadvState *state,
                                   const char *text);
void transient_ui_snapshot_build_readv(TransientUiSnapshot *snapshot,
                                       int top, int left, int cols,
                                       const TransientUiReadvState *state);

void transient_ui_dialog_state_init(TransientUiDialogState *state,
                                    int has_editfield, int button_count,
                                    int active_button, const char *edit_text);
TransientUiAction transient_ui_dialog_handle_key(
   TransientUiDialogState *state, TransientUiKey key);
TransientUiAction transient_ui_dialog_handle_hit(
   TransientUiDialogState *state, const TransientUiSnapshot *snapshot,
   int row, int col);
void transient_ui_snapshot_build_dialog(
   TransientUiSnapshot *snapshot, int top, int left, int rows, int cols,
   const char *title, const char * const *prompt_lines, size_t prompt_count,
   const char *edit_text, int edit_cursor_cell, int has_editfield,
   const TransientUiButtonSpec *buttons, size_t button_count,
   const TransientUiDialogState *state);

void transient_ui_popup_state_init(TransientUiPopupState *state, int height,
                                   int width, int pad_height, int pad_width,
                                   int initial, int item_count,
                                   const char * const *items);
TransientUiAction transient_ui_popup_handle_key(TransientUiPopupState *state,
                                                const char * const *items,
                                                TransientUiKey key);
TransientUiAction transient_ui_popup_handle_hit(TransientUiPopupState *state,
                                                const TransientUiSnapshot *snapshot,
                                                int row, int col);
void transient_ui_snapshot_build_popup(TransientUiSnapshot *snapshot, int top,
                                       int left,
                                       const TransientUiPopupState *state,
                                       const char * const *items);

#endif
