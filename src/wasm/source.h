#ifndef THE_WASM_SOURCE_H
#define THE_WASM_SOURCE_H

/* Immutable logical source on the editor's input owner. Borrowed until the
 * next prepare/forget; consumers copy bytes before returning to JavaScript. */
typedef struct {
    unsigned id, revision;
    const char *bytes;
    size_t length, lines;
} TheWasmSource;

const TheWasmSource *the_wasm_source_prepare(void);
const TheWasmSource *the_wasm_source_current(void);
const char *the_wasm_source_error(void);
void the_wasm_source_forget(FILE_DETAILS *file);

#endif
