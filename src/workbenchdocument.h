#ifndef THE_WORKBENCHDOCUMENT_H
#define THE_WORKBENCHDOCUMENT_H

#include <stddef.h>
#include <stdint.h>

#define THE_WORKBENCH_DOCUMENT_LINES_MAX 8
#define THE_WORKBENCH_DOCUMENT_BYTES_MAX 8192
#define THE_WORKBENCH_DOCUMENT_RECORD_MAX (1024u * 1024u)

typedef enum {
   THE_WORKBENCH_DOCUMENT_OK = 0,
   THE_WORKBENCH_DOCUMENT_MISSING,
   THE_WORKBENCH_DOCUMENT_STALE,
   THE_WORKBENCH_DOCUMENT_BUSY,
   THE_WORKBENCH_DOCUMENT_INVALID,
   THE_WORKBENCH_DOCUMENT_LIMIT,
   THE_WORKBENCH_DOCUMENT_NOMEM
} TheWorkbenchDocumentResult;

typedef struct {
   uint64_t id, revision;
   size_t lines;
   int available, dirty;
   const char *reason;
} TheWorkbenchDocumentInfo;

typedef struct {
   size_t line, offset, length, original_length, data_offset;
} TheWorkbenchDocumentSlice;

/* Fully copied data: no LINE, rec, file or borrowed text pointers escape. */
typedef struct {
   TheWorkbenchDocumentInfo info;
   TheWorkbenchDocumentSlice slices[THE_WORKBENCH_DOCUMENT_LINES_MAX];
   unsigned char bytes[THE_WORKBENCH_DOCUMENT_BYTES_MAX];
   size_t count, length, next_line, next_offset;
   int more;
} TheWorkbenchDocumentPage;

TheWorkbenchDocumentResult the_workbench_document_info(uint64_t id, TheWorkbenchDocumentInfo *out);
TheWorkbenchDocumentResult the_workbench_document_read(uint64_t id, uint64_t expected_revision,
   size_t first_line, size_t byte_offset, size_t lines, size_t bytes, TheWorkbenchDocumentPage *out);
const char *the_workbench_document_result_name(TheWorkbenchDocumentResult result);
/* Internal scalar scanner; malformed octets have size one. */
size_t the_workbench_document_unit(const unsigned char *bytes, size_t length);
/* Panel ID resolves only its immutable document binding. All JSON tokens are
 * decimal strings; text is display-safe UTF-8 and bytesHex preserves raw bytes. */
short the_workbench_document_json(char *out, size_t capacity, const char *panel,
   uint64_t expected_revision, size_t line, size_t offset, size_t lines, size_t bytes);

/* Editor-owner provider and lifecycle seam. Callbacks never dispatch/yield. */
typedef struct {
   int (*info)(const void *file, size_t *lines, int *dirty);
   int (*line)(const void *file, size_t number, const unsigned char **bytes, size_t *length);
   int (*pending)(const unsigned char **bytes, size_t *length);
   int (*blocked)(void);
   /* Certifies an active sentinel record by comparing actual rec bytes with
    * TOF/EOF; this is not permission to overlay an unowned working record. */
   int (*no_pending)(const void *file);
   int (*owns_list)(const void *file, const void *first_line);
} TheWorkbenchDocumentProvider;
void the_workbench_document_provider(const TheWorkbenchDocumentProvider *provider);
uint64_t the_workbench_document_identify(const void *file);
void the_workbench_document_forget(const void *file);
void the_workbench_document_enter(void);
void the_workbench_document_leave(void);
/* Resolution remains busy until the dispatcher proves a read-only builtin;
 * it does not invalidate a token merely by resolving that inspection. */
void the_workbench_document_resolving(int entering);
void the_workbench_document_changed(void);
void the_workbench_document_record_bind(const void *file, size_t line, const void *base, size_t length);
void the_workbench_document_record_unknown(void);
/* Certifies a real pending overlay for a particular file/line without
 * inferring ownership from the current UI focus or committing the record. */
int the_workbench_document_pending_owned(const void *file, size_t line);
void the_workbench_document_structure(const void *first_line);
/* Teardown forgets handles, never resets the identity counter. */
void the_workbench_document_reset(void);

#endif
