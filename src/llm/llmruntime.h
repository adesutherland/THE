#ifndef THE_LLMRUNTIME_H
#define THE_LLMRUNTIME_H

#include "llmdriver.h"

void llm_runtime_history_lookup(int (*lookup)(void));

int llm_runtime_screen_view(CHARTYPE scrno, LlmDriverScreenView *view);
size_t llm_runtime_format_screen(CHARTYPE scrno,
                                 const LlmDriverFormatOptions *options,
                                 char *out, size_t out_len);

#endif
