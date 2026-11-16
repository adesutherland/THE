#ifndef THE_LLMSESSION_H
#define THE_LLMSESSION_H

#include <stddef.h>
#include <stdio.h>

#include "transientui.h"

int llm_session_run_protocol(void);
/* Lazily owned stream shared by startup/nested READ panels and the protocol
 * loop. Returns NULL if duplicating/opening stdin fails. */
FILE *llm_session_protocol_input(void);
/* One narrow typed form line; emits its receipt, including modal refusal. */
int llm_session_handle_form_input(char *line);
int llm_session_begin_readv_continuation(const char *initial, int start_col,
                                         int cols);
int llm_session_begin_dialog_continuation(
   const char *stemname, const char *title,
   const char * const *prompt_lines, size_t prompt_count,
   const char *initial, int has_editfield,
   const TransientUiButtonSpec *buttons, size_t button_count,
   int active_button, int top, int left, int rows, int cols);
int llm_session_begin_popup_continuation(
   int top, int left, int height, int width, int pad_height, int pad_width,
   int initial, int item_count, const char * const *items);

#endif
