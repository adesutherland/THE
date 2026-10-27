#include "the.h"
#include "vars.h"
#include "panelui.h"
#include "source.h"

#define SOURCE_LIMIT (2u * 1024u * 1024u)
typedef struct SourceDocument {
    FILE_DETAILS *file;
    TheWasmSource source;
    struct SourceDocument *next;
} SourceDocument;
static SourceDocument *documents;
static const TheWasmSource *current;
static unsigned next_id;
static const char *problem = "No active buffer";

const TheWasmSource *the_wasm_source_current(void) { return current; }
const char *the_wasm_source_error(void) { return problem; }

void the_wasm_source_forget(FILE_DETAILS *file)
{
    SourceDocument **link = &documents, *doc;
    while ((doc = *link) != NULL) {
        if (doc->file != file) { link = &doc->next; continue; }
        if (current == &doc->source) current = NULL;
        *link = doc->next;
        free((void *)doc->source.bytes); free(doc);
        return;
    }
}

const TheWasmSource *the_wasm_source_prepare(void)
{
    SourceDocument *doc;
    LINE *line;
    size_t length = 0, count = 0, offset = 0;
    char *bytes;
    current = NULL;
    problem = "No active buffer";
    if (!CURRENT_VIEW || !CURRENT_FILE || !CURRENT_FILE->first_line) return NULL;
    problem = "Finish the macro or panel before running a program";
    if (the_panel_active || in_macro) return NULL;
    for (line = CURRENT_FILE->first_line->next; line && line->next; line = line->next) {
        size_t n = ++count == (size_t)CURRENT_VIEW->focus_line ? rec_len : line->length;
        if (n >= SOURCE_LIMIT - length) { problem = "Rexx source exceeds the 2 MiB browser limit"; return NULL; }
        length += n + 1;
    }
    bytes = malloc(length + 1);
    if (!bytes) { problem = "Unable to allocate program snapshot"; return NULL; }
    count = 0;
    for (line = CURRENT_FILE->first_line->next; line && line->next; line = line->next) {
        const CHARTYPE *text = ++count == (size_t)CURRENT_VIEW->focus_line ? rec : line->line;
        size_t n = count == (size_t)CURRENT_VIEW->focus_line ? rec_len : line->length;
        memcpy(bytes + offset, text, n); offset += n; bytes[offset++] = '\n';
    }
    bytes[length] = 0;
    if (memchr(bytes, 0, length)) { free(bytes); problem = "Rexx source contains NUL bytes"; return NULL; }
    for (doc = documents; doc && doc->file != CURRENT_FILE; doc = doc->next) {}
    if (!doc) {
        doc = calloc(1, sizeof(*doc));
        if (!doc) { free(bytes); problem = "Unable to allocate source identity"; return NULL; }
        doc->file = CURRENT_FILE; doc->source.id = ++next_id;
        doc->next = documents; documents = doc;
    }
    if (!doc->source.bytes || doc->source.length != length || memcmp(doc->source.bytes, bytes, length)) {
        free((void *)doc->source.bytes);
        doc->source.bytes = bytes; doc->source.length = length; doc->source.lines = count;
        doc->source.revision++;
    } else free(bytes);
    current = &doc->source; problem = "";
    return current;
}
