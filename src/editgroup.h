#ifndef THE_EDITGROUP_H
#define THE_EDITGROUP_H

#include <stddef.h>
#include <stdint.h>

#define THE_EDIT_GROUP_LINES_MAX 128
#define THE_EDIT_GROUP_BYTES_MAX (1024u * 1024u)

/* This conservative mutation token is independent of DOCUMENT read paging's
 * epoch. Other editing/navigation/save admissions may stale it. */
typedef struct { uint64_t document_id, write_revision; } TheDocumentWriteToken;
typedef struct {
   size_t line;
   const unsigned char *before, *after;
   size_t before_length, after_length;
} TheDocumentLineEdit;
typedef struct {
   uint64_t group, document_id, before_revision, after_revision;
   size_t changed_lines;
   int undo_available, parser_available;
   short autosave_rc, rc;
   char code[64];
} TheEditGroupReceipt;
typedef struct {
   uint64_t group, document_id, after_revision;
   size_t changed_lines;
   int undo_available;
} TheEditGroupInfo;
typedef struct TheEditGroupPlan TheEditGroupPlan;

short the_edit_group_token(uint64_t document_id, TheDocumentWriteToken *out);
/* Internal copied-capture seam for final profile publication. This observes
 * the same epoch but grants no write admission during activation. */
short the_edit_group_capture_token(uint64_t document_id, TheDocumentWriteToken *out);
short the_edit_group_prepare(TheDocumentWriteToken expected,
   const TheDocumentLineEdit *edits, size_t count, TheEditGroupPlan **out);
/* Success consumes the plan. Refusal leaves it abortable and preserves the
 * preceding undo group. No editor callback/yield occurs inside publication. */
short the_edit_group_commit(TheEditGroupPlan *plan, TheEditGroupReceipt *out);
void the_edit_group_abort(TheEditGroupPlan *plan);
short the_edit_group_undo(uint64_t group, TheDocumentWriteToken expected,
   TheEditGroupReceipt *out);
void the_edit_group_info(TheEditGroupInfo *out);
const TheEditGroupReceipt *the_edit_group_receipt(void);
const char *the_edit_group_error(void);

/* Editor-owner integration. Only publish is required to be infallible;
 * prepare/validate must finish every allocation and possible refusal first.
 * finish runs after the entire byte/dirty/token/journal publication. */
typedef struct {
   short (*available)(uint64_t document_id);
   short (*prepare)(uint64_t document_id, const TheDocumentLineEdit *edits,
      size_t count, void **prepared, size_t *retained_bytes);
   short (*validate)(void *prepared, int undo);
   void (*publish)(void *prepared, int undo);
   void (*finish)(void *prepared, int undo, TheEditGroupReceipt *receipt);
   void (*destroy)(void *prepared);
   int (*parser_available)(uint64_t document_id);
   const char *(*error)(void);
   /* Read-only availability: modal, identity and certified record checks remain;
    * final profile publication may capture while writes are activation-blocked. */
   short (*capture_available)(uint64_t document_id);
} TheEditGroupProvider;
/* Adopt a fully allocated structural editor transaction. On success the plan
 * owns prepared; on refusal ownership stays with the caller. */
short the_edit_group_prepare_owned(TheDocumentWriteToken expected,
   const TheEditGroupProvider *owner, void *prepared, size_t changed_lines,
   TheEditGroupPlan **out);
void the_edit_group_provider(const TheEditGroupProvider *provider);
void the_edit_group_mutation_enter(void);
void the_edit_group_mutation_leave(void);
void the_edit_group_changed(void);
void the_edit_group_forget(uint64_t document_id);
void the_edit_group_reset(void);

#endif
