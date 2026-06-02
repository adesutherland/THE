#ifndef THE_LLMSESSION_H
#define THE_LLMSESSION_H

#include <stddef.h>

#include "transientui.h"

int llm_session_run_protocol(void);
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
