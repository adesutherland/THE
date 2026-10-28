#ifndef THE_EDITORSOURCE_H
#define THE_EDITORSOURCE_H

/* Immutable logical source on the editor's input owner. Borrowed until the
 * next prepare/forget; consumers copy bytes before returning to JavaScript. */
typedef struct {
    unsigned id, revision;
    const char *bytes;
    size_t length, lines;
} TheEditorSource;

const TheEditorSource *the_editor_source_prepare(void);
const TheEditorSource *the_editor_source_current(void);
const char *the_editor_source_error(void);
void the_editor_source_forget(FILE_DETAILS *file);

#endif
