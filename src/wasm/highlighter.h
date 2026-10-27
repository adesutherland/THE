#ifndef THE_WASM_HIGHLIGHTER_H
#define THE_WASM_HIGHLIGHTER_H

/* Included after the editor's legacy the.h declarations. */
#include "uidriver.h"
#include "parserdiagnostics.h"

void the_wasm_highlight_prepare(void);
void the_wasm_highlight_forget(FILE_DETAILS *file);
void the_wasm_highlight_restart(void);
int the_wasm_highlight_row(FILE_DETAILS *file, UiFrame *frame, size_t row);
int the_wasm_highlight_metadata(char *out, size_t capacity);
short the_wasm_highlight_diagnostics(TheParserDiagnostic **out, int *count);
void the_wasm_highlight_reply(unsigned id, unsigned revision, const char *payload, int failed);

#endif
