#ifndef THE_WORKBENCHFORM_H
#define THE_WORKBENCHFORM_H

#include "workbench.h"

typedef struct {
   char id[THE_WORKBENCH_ID_MAX+1];
   unsigned long generation;
   uint64_t draft_revision, result_revision;
   size_t fields;
   int dirty, submitted;
} TheWorkbenchFormState;

typedef struct {
   char id[THE_WORKBENCH_ID_MAX+1], form[THE_WORKBENCH_ID_MAX+1];
   unsigned long generation;
   char value[THE_WORKBENCH_FIELD_VALUE_MAX+1];
   size_t length;
   int dirty;
} TheWorkbenchFieldState;

typedef enum { THE_WORKBENCH_FORM_SET=1, THE_WORKBENCH_FORM_EDIT,
   THE_WORKBENCH_FORM_SUBMIT, THE_WORKBENCH_FORM_CANCEL } TheWorkbenchFormOperation;
typedef enum { THE_WORKBENCH_FORM_INSERT=1, THE_WORKBENCH_FORM_BACKSPACE,
   THE_WORKBENCH_FORM_DELETE, THE_WORKBENCH_FORM_CLEAR } TheWorkbenchFormEdit;

/* Runtime values are data, never interpolated commands. Expected tokens are
 * mandatory for typed requests. Empty panel means explicit headless form use. */
typedef struct {
   TheWorkbenchFormOperation operation;
   uint64_t request_id, draft_revision;
   unsigned long form_generation, panel_generation;
   char form[THE_WORKBENCH_ID_MAX+1], field[THE_WORKBENCH_ID_MAX+1], panel[THE_WORKBENCH_ID_MAX+1];
   char value[THE_WORKBENCH_FIELD_VALUE_MAX+1];
   size_t length;
   TheWorkbenchFormEdit edit;
   int cell;
   uint32_t codepoint;
} TheWorkbenchFormRequest;

typedef struct {
   uint64_t serial, request_id, draft_revision, result_revision;
   unsigned long form_generation;
   TheWorkbenchFormOperation operation;
   char form[THE_WORKBENCH_ID_MAX+1];
   short rc;
   char code[64]; /* Receipt-owned; unrelated lastResult updates cannot alter it. */
   int caret; /* EDIT's admitted logical cell, otherwise -1. */
} TheWorkbenchFormReceipt;

/* Borrowed until the next form mutation/publication. All mutation runs on the
 * editor owner at the normalized input boundary, never from renderer callbacks. */
const TheWorkbenchFormState *the_workbench_form_state(const char *id);
const TheWorkbenchFieldState *the_workbench_field_state(const char *form,const char *field);
const TheWorkbenchFormReceipt *the_workbench_form_receipt(void);
uint64_t the_workbench_form_state_revision(void);
short the_workbench_form_request(const TheWorkbenchFormRequest *request);
int the_workbench_field_value_valid(const char *value,size_t length,int maxcells);
/* Complete fields, <=8 records and <=8192 value bytes; no truncated values. */
short the_workbench_form_json(char *out,size_t capacity,const char *form,
   size_t offset,size_t limit,size_t bytes,int submitted);

/* Registry-only atomic publication preparation. No runtime values become live
 * until commit; a dirty draft refuses definition replacement/removal. */
typedef struct TheWorkbenchFormPublication TheWorkbenchFormPublication;
short the_workbench_forms_prepare(const TheWorkbenchForm *forms,size_t form_count,
   const TheWorkbenchField *fields,size_t field_count,TheWorkbenchFormPublication **out);
void the_workbench_forms_commit(TheWorkbenchFormPublication *publication);
void the_workbench_forms_abort(TheWorkbenchFormPublication *publication);
void the_workbench_forms_reset(void);
void the_workbench_forms_modal_lookup(int (*lookup)(void));

#endif
