#include "the.h"
#include "vars.h"
#include "proto.h"
#include "panelui.h"
#include "editorsource.h"

#define SOURCE_LIMIT (2u * 1024u * 1024u)
typedef struct SourceDocument {
    FILE_DETAILS *file;
    TheEditorSource source;
    struct SourceDocument *next;
} SourceDocument;
static SourceDocument *documents;
static const TheEditorSource *current;
static unsigned next_id;
static const char *problem = "No active buffer";

const TheEditorSource *the_editor_source_current(void) { return current; }
const char *the_editor_source_error(void) { return problem; }

int the_editor_buffer_dirty(const FILE_DETAILS *file)
{
    LINE *line;
    if (file == NULL) return 0;
    if (file->save_alt != 0) return 1;
    /* Typing updates the active core record before post_process_line commits
     * it to the file's line list. Include that record in unsaved-state reports. */
    if (CURRENT_VIEW == NULL || CURRENT_FILE != file || rec == NULL
    || CURRENT_VIEW->focus_line < 1
    || CURRENT_VIEW->focus_line > file->number_lines) return 0;
    line = lll_find(file->first_line, file->last_line,
                    CURRENT_VIEW->focus_line, file->number_lines);
    return line != NULL && (rec_len != line->length
        || memcmp(rec, line->line, line->length) != 0);
}

void the_editor_source_forget(FILE_DETAILS *file)
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

const TheEditorSource *the_editor_source_prepare(void)
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
        if (n >= SOURCE_LIMIT - length) { problem = "Source exceeds the 2 MiB tool limit"; return NULL; }
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
    if (memchr(bytes, 0, length)) { free(bytes); problem = "Source contains NUL bytes"; return NULL; }
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
