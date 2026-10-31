#ifndef THE_WEBSESSION_H
#define THE_WEBSESSION_H

#include "inputevent.h"
#include "workbench.h"
#include "workbenchform.h"
#include "workbenchfilefields.h"

#define THE_WEB_WORKBENCH_RECEIPTS 256
#define THE_WEB_WORKBENCH_REQUEST_ID_MAX 64
#define THE_WEB_WORKBENCH_NONCE_MAX 24
typedef struct
{
   char request_id[THE_WEB_WORKBENCH_REQUEST_ID_MAX + 1];
   char code[48];
   char message[256];
   short rc;
   unsigned long revision;
   char action[THE_WORKBENCH_ID_MAX + 1];
   char operation[24];
   char argument[THE_WORKBENCH_OPERAND_MAX + 1];
   unsigned long generation;
   uint64_t form_request_id, filefield_request_id, draft_revision, result_revision, group;
   int caret, utf16_caret;
   int has_argument;
   int pending;
} TheWebWorkbenchReceipt;
typedef struct
{
   TheWebWorkbenchReceipt entries[THE_WEB_WORKBENCH_RECEIPTS];
   size_t count;
   size_t next_slot;
   unsigned long long high_water;
   char nonce[THE_WEB_WORKBENCH_NONCE_MAX + 1];
   int pending_index;
   unsigned long dispatch_serial;
   uint64_t form_serial;
   uint64_t filefield_serial;
} TheWebWorkbenchReceipts;
typedef struct
{
   size_t count;
   char reasons[THE_WORKBENCH_ACTION_MAX][32];
} TheWebWorkbenchReasonState;

/* Transport-neutral browser behavior. Only the editor's execution owner calls
 * these functions; transports decode messages without maintaining editor state. */
size_t the_web_format_snapshot(char *out, size_t capacity);
size_t the_web_format_actions(char *out, size_t capacity);
/* Publish all three immutable workbench definition pages for one revision. */
void the_web_publish_workbench(void (*send)(const char *, size_t, void *),
                               void *context, unsigned long publication);
int the_web_workbench_reasons_changed(TheWebWorkbenchReasonState *state);
size_t the_web_format_workbench_page(char *out, size_t capacity,
                                     const char *section, size_t offset,
                                     size_t limit);
size_t the_web_format_panel_state(char *out, size_t capacity);
size_t the_web_format_log_page(char *out, size_t capacity,
                               const char *id, uint64_t generation,
                               uint64_t cursor, size_t offset,
                               size_t records, size_t bytes);
size_t the_web_format_document_page(char *out, size_t capacity,
                                    const char *panel, uint64_t revision,
                                    size_t line, size_t offset,
                                    size_t records, size_t bytes);
size_t the_web_format_form_page(char *out,size_t capacity,const char *form,
                                size_t offset,size_t limit,size_t bytes,int submitted);
size_t the_web_format_filefield_page(char *out,size_t capacity,const char *panel,
                                    size_t offset,size_t limit,size_t bytes);
int the_web_workbench_form_begin(TheWebWorkbenchReceipts *receipts,
   const char *request_id,TheWorkbenchFormRequest *request,TheInputEvent *event);
int the_web_workbench_filefield_begin(TheWebWorkbenchReceipts *receipts,
   const char *request_id,TheWorkbenchFileFieldsRequest *request,TheInputEvent *event);
int the_web_parse_u64(const char *text, uint64_t *value);
int the_web_panel_layout(const char *id, uint64_t generation,
                         const char *placement, int cols, int rows);
void the_web_workbench_receipts_init(TheWebWorkbenchReceipts *receipts,
                                     const char *nonce);
/* 1 new, 0 replay, -1 invalid ID, -3 busy, -4 conflict, -5 operand,
 * -6 unknown/evicted receipt. */
int the_web_workbench_receipt_begin(TheWebWorkbenchReceipts *receipts,
                                    const char *request_id, const char *operation,
                                    const char *action,
                                    unsigned long generation,
                                    const char *argument);
const TheWebWorkbenchReceipt *the_web_workbench_receipt_find(
   const TheWebWorkbenchReceipts *receipts, const char *request_id);
void the_web_workbench_receipt_refuse(TheWebWorkbenchReceipts *receipts,
                                      const char *code, short rc,
                                      const char *message);
void the_web_workbench_receipt_dispatched(TheWebWorkbenchReceipts *receipts);
const TheWebWorkbenchReceipt *the_web_workbench_receipt_complete(
   TheWebWorkbenchReceipts *receipts);
int the_web_format_workbench_receipt(char *out, size_t capacity,
                                     const TheWebWorkbenchReceipt *receipt);
int the_web_queue_request(TheInputQueue *queue, const char *type,
                          const char *value, const char *argument,
                          long line, int row, int cell, int rows, int cols);
#endif
