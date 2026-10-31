#ifndef THE_WORKBENCHFILEFIELDS_H
#define THE_WORKBENCHFILEFIELDS_H

#include "workbenchform.h"
#include "editgroup.h"

#define THE_WORKBENCH_FILEFIELDS_MAX 32
#define THE_WORKBENCH_FILEFIELDS_RECORDS_MAX 128
#define THE_WORKBENCH_FILEFIELDS_SPANS_MAX 128
#define THE_WORKBENCH_FILEFIELDS_BYTES_MAX (256u * 1024u)
#define THE_WORKBENCH_FILEFIELDS_SESSION_MAX (8u * 1024u * 1024u)

typedef struct {
   char panel[THE_WORKBENCH_ID_MAX+1];
   unsigned long generation;
   uint64_t document_id,write_revision,draft_revision;
   size_t records,spans;
   int dirty,active;
} TheWorkbenchFileFieldsState;
/* Protected text is included. Byte offsets are informational and never UI
 * caret coordinates. Every value is complete printable UTF-8. */
typedef struct {
   char id[THE_WORKBENCH_ID_MAX+1];
   size_t line,byte_start,length;
   int cell,maxcells,editable,dirty;
   char value[THE_WORKBENCH_FIELD_VALUE_MAX+1];
} TheWorkbenchFileFieldSpan;
typedef enum {
   THE_WORKBENCH_FILEFIELDS_SET=1, THE_WORKBENCH_FILEFIELDS_EDIT,
   THE_WORKBENCH_FILEFIELDS_APPLY, THE_WORKBENCH_FILEFIELDS_RELOAD,
   THE_WORKBENCH_FILEFIELDS_CANCEL, THE_WORKBENCH_FILEFIELDS_RETURN
} TheWorkbenchFileFieldsOperation;
typedef struct {
   TheWorkbenchFileFieldsOperation operation;
   uint64_t request_id,draft_revision;
   unsigned long panel_generation;
   char panel[THE_WORKBENCH_ID_MAX+1],field[THE_WORKBENCH_ID_MAX+1];
   char value[THE_WORKBENCH_FIELD_VALUE_MAX+1];
   size_t length;
   size_t utf16_offset; /* cell==-1: owner maps this offset, refusing split clusters. */
   TheWorkbenchFormEdit edit;
   int cell;
   uint32_t codepoint;
} TheWorkbenchFileFieldsRequest;
typedef struct {
   uint64_t serial,request_id,draft_revision,group;
   unsigned long panel_generation;
   TheWorkbenchFileFieldsOperation operation;
   char panel[THE_WORKBENCH_ID_MAX+1];
   short rc;
   char code[64];
   int caret,utf16_caret; /* -1 unless an EDIT admitted a text caret. */
} TheWorkbenchFileFieldsReceipt;

const TheWorkbenchFileFieldsState *the_workbench_filefields_state(const char *panel);
const TheWorkbenchFileFieldSpan *the_workbench_filefields_span(const char *panel,size_t index);
const TheWorkbenchFileFieldsReceipt *the_workbench_filefields_receipt(void);
uint64_t the_workbench_filefields_state_revision(void);
short the_workbench_filefields_request(const TheWorkbenchFileFieldsRequest *request);
/* Pages contain complete logical rows and interpreted protected/editable
 * spans; offset is a zero-based record index, limit 1..8, bytes 1..8192. */
short the_workbench_filefields_json(char *out,size_t capacity,const char *panel,
   size_t offset,size_t limit,size_t bytes);
/* Early close admission; pure and safe before posting rec or saving. */
short the_workbench_filefields_close(uint64_t document_id);
size_t the_workbench_filefields_span_count(void);
void the_workbench_filefields_reset(void);

/* Editor owner supplies current CTLCHAR policy and complete certified logical
 * records. No callback yields or returns ownership to a frontend. */
typedef struct {
   unsigned char escape,tags[256],editable[256];
   size_t width,truncation;
   int enabled,readonly;
} TheWorkbenchFileFieldsConfig;
typedef struct {
   short (*config)(uint64_t document_id,TheWorkbenchFileFieldsConfig *out);
   short (*info)(uint64_t document_id,size_t *records);
   short (*line)(uint64_t document_id,size_t line,const unsigned char **bytes,size_t *length);
   const char *(*error)(void);
} TheWorkbenchFileFieldsProvider;
void the_workbench_filefields_provider(const TheWorkbenchFileFieldsProvider *provider);
typedef struct TheWorkbenchFileFieldsPublication TheWorkbenchFileFieldsPublication;
short the_workbench_filefields_prepare(const TheWorkbenchPanel *panels,size_t count,
   size_t named_fields,TheWorkbenchFileFieldsPublication **out);
void the_workbench_filefields_commit(TheWorkbenchFileFieldsPublication *publication);
void the_workbench_filefields_abort(TheWorkbenchFileFieldsPublication *publication);
void the_editor_filefields_initialize(void);
short the_editor_filefields_close(int all_buffers);

#endif
