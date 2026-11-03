#ifndef THE_EDITORCLIPBOARD_H
#define THE_EDITORCLIPBOARD_H
#include <stddef.h>
#include <stdint.h>
#define THE_CLIPBOARD_BYTES_MAX (1024u * 1024u)
#define THE_CLIPBOARD_LINES_MAX 65536u
/* Editor-owner calls only. The caller owns exported bytes until release.
 * Capture(copy=0) pins a paste destination; copy=1 exports the selection.
 * Command/prefix Copy exports the entire field (whole_field=1); their Paste
 * accepts one line without executing it.
 * A token is invalidated by another capture or any context/revision change.
 * Cut is deliberately separate: call only after the OS clipboard write succeeds. */
typedef struct {
   uint64_t token, document_id, write_revision;
   unsigned char *bytes;
   size_t length;
   int read_only, whole_field;
} TheEditorClipboardCapture;
short the_editor_clipboard_capture(int copy, TheEditorClipboardCapture *out);
void the_editor_clipboard_release(TheEditorClipboardCapture *capture);
short the_editor_clipboard_paste(uint64_t token,const unsigned char *bytes,size_t length);
short the_editor_clipboard_cut(uint64_t token);
short the_editor_clipboard_select_all(void);
short the_editor_clipboard_undo(int *handled);
const char *the_editor_clipboard_error(void);
const char *the_editor_clipboard_error_message(void);
/* Same bounded protocol for native/LLM/browser bridges; returned JSON is owned. */
char *the_editor_clipboard_json(const char *operation,const char *request_id,
   uint64_t token,const unsigned char *bytes,size_t length);
#endif
