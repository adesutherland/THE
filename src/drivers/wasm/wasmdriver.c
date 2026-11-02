#include <emscripten.h>
#include <stdio.h>
#include <limits.h>
#include <string.h>
#include <errno.h>
#include <sys/stat.h>

#include "the.h"
#include "proto.h"
#include "vars.h"
#include "headlessdriver.h"
#include "websession.h"
#include "workbench.h"
#include "workbenchruntime.h"
#include "workbenchdocument.h"
#include "editorsource.h"
#include "tools.h"
#include "frontendpolicy.h"
void the_tools_wasm_initialise(int);
#ifdef THE_WASM_HIGHLIGHT
#include "wasm/highlighter.h"
#endif

static TheInputQueue inputs;
static TheDriverOps operations;
static int input_timeout = -1;
static void (*base_update)(void);
static char snapshot[262144];
static unsigned long workbench_revision;
static unsigned long workbench_publication;
static TheWebWorkbenchReasonState workbench_reasons;
static TheWebWorkbenchReceipts workbench_receipts;
static char workbench_panel_state[32768];

int main(int argc, char **argv);
EMSCRIPTEN_KEEPALIVE int the_wasm_has_highlighter(void)
{
#ifdef THE_WASM_HIGHLIGHT
   return 1;
#else
   return 0;
#endif
}
EMSCRIPTEN_KEEPALIVE int the_wasm_has_crexx(void)
{
#ifdef USE_CREXX
   return 1;
#else
   return 0;
#endif
}
EMSCRIPTEN_KEEPALIVE int the_wasm_has_interpreter(void)
{
#ifdef THE_WASM_INTERPRETER
   return 1;
#else
   return 0;
#endif
}

EMSCRIPTEN_KEEPALIVE int the_wasm_main(void)
{
   char *argv[] = {"crxthe", "--driver", "web", "-s", "/workspace/Welcome.txt", NULL};
   return main(5, argv);
}

short the_wasm_prepare_last_close(void)
{
   static unsigned long untitled;
   char path[THE_FRONTEND_POLICY_PATH_MAX + 1];
   char current[THE_FRONTEND_POLICY_PATH_MAX + 1];
   const char *root = the_frontend_policy_root_at(0, NULL);
   struct stat info;
   VIEW_DETAILS *closing;
   short rc;
   int length, attempts;

   if (number_of_files != 1 || CURRENT_VIEW == NULL) return RC_OK;
   if (root == NULL) return RC_INVALID_ENVIRON;
   closing = CURRENT_VIEW;
   current[0] = '\0';
   if (CURRENT_FILE->fpath != NULL && CURRENT_FILE->fname != NULL)
      snprintf(current, sizeof(current), "%s%s", (char *)CURRENT_FILE->fpath,
               (char *)CURRENT_FILE->fname);
   else if (CURRENT_FILE->actualfname != NULL)
      snprintf(current, sizeof(current), "%s", (char *)CURRENT_FILE->actualfname);
   /* Prepare the replacement first so allocation/open failure retains the
    * file the user is closing. Never reuse an existing or unsaved pathname. */
   for (attempts = 0; attempts < 1024; attempts++)
   {
      if (untitled == ULONG_MAX) break;
      length = snprintf(path, sizeof(path), "%s/Untitled-%lu.txt",
                         root, ++untitled);
      if (length < 0 || (size_t)length >= sizeof(path)) return RC_INVALID_ENVIRON;
      if (strcmp(path, current) == 0) continue;
      errno = 0;
      if (lstat(path, &info) != 0 && errno == ENOENT)
      {
         rc = EditFile((CHARTYPE *)path, FALSE);
         if (rc != RC_OK) return rc;
         CURRENT_VIEW = CURRENT_SCREEN.screen_view = closing;
         return RC_OK;
      }
   }
   display_error(0, (CHARTYPE *)"Unable to prepare a new browser buffer", FALSE);
   return RC_INVALID_ENVIRON;
}

EM_JS(void, browser_send, (const char *text), {
   Module.sendEditorMessage(JSON.parse(UTF8ToString(text)));
});

static void publish_panel_state(void)
{
   char current[32768];
   size_t length = the_web_format_panel_state(current,sizeof(current));
   if (length && strcmp(current,workbench_panel_state) != 0)
   {
      memcpy(workbench_panel_state,current,length + 1);
      browser_send(current);
   }
}

static void browser_send_workbench_page(const char *text, size_t length,
                                        void *context)
{
   (void)length; (void)context;
   browser_send(text);
}

static void publish_workbench(void)
{
   unsigned long current = the_workbench_revision();
   int reasons_changed = the_web_workbench_reasons_changed(&workbench_reasons);
   if (current != workbench_revision || reasons_changed)
   {
      the_web_publish_workbench(browser_send_workbench_page, NULL,
                                ++workbench_publication);
      workbench_revision = current;
   }
}

static void browser_send_workbench_receipt(const TheWebWorkbenchReceipt *receipt)
{
   char message[1024];
   if (the_web_format_workbench_receipt(message,sizeof(message),receipt))
      browser_send(message);
}

static void browser_send_workbench_refusal(const char *request_id,
   const char *code, const char *message)
{
   TheWebWorkbenchReceipt receipt;
   memset(&receipt,0,sizeof(receipt));
   snprintf(receipt.request_id,sizeof(receipt.request_id),"%s",request_id ? request_id : "");
   snprintf(receipt.code,sizeof(receipt.code),"%s",code);
   snprintf(receipt.message,sizeof(receipt.message),"%s",message);
   receipt.rc = -3;
   receipt.revision = the_workbench_revision();
   browser_send_workbench_receipt(&receipt);
}
EM_JS(void, browser_source, (int request, unsigned id, unsigned revision,
                           const char *source, size_t length, const char *path,
                           const char *name, const char *error), {
   Module.sendEditorMessage({type: 'run-source', request, id, revision,
      source: HEAPU8.slice(source, source + length),
      filename: UTF8ToString(path) + UTF8ToString(name), error: UTF8ToString(error)});
});

EM_ASYNC_JS(void,browser_import,(),{await Module.importTool(Module.editorRequest);});

EM_ASYNC_JS(int, browser_wait, (int timeout), {
   Module.editorRequest = await Module.nextEditorRequest(timeout);
   return Module.editorRequest ? 1 : 0;
});

EM_JS(int, browser_string, (const char *field, char *out, int capacity), {
   const value = Module.editorRequest[UTF8ToString(field)];
   if (value === undefined) { HEAPU8[out] = 0; return 1; }
   if (typeof value !== 'string' || lengthBytesUTF8(value) >= capacity) return 0;
   stringToUTF8(value, out, capacity);
   return 1;
});

EM_JS(int, browser_integer, (const char *field, int fallback), {
   const value = Module.editorRequest[UTF8ToString(field)];
   return Number.isSafeInteger(value) && value >= -2147483648 && value <= 2147483647 ? value : fallback;
});

EM_JS(char *,browser_payload,(const char *field,int limit),{
 const value=Module.editorRequest[UTF8ToString(field)];if(typeof value!=='string')return 0;const n=lengthBytesUTF8(value)+1;if(n>limit)return 0;const p=_malloc(n);if(p)stringToUTF8(value,p,n);return p;
});
EM_JS(int,browser_workbench_string_valid,(const char *field,int optional),{
 const value=Module.editorRequest[UTF8ToString(field)];
 return value===undefined ? optional : typeof value==='string' && !value.includes('\0');
});
EM_JS(int,browser_form_value_valid,(),{
 const value=Module.editorRequest.value;
 if(typeof value!=='string'||value.includes('\0'))return 0;
 for(let i=0;i<value.length;i++){
   const code=value.charCodeAt(i);
   if(code>=0xd800&&code<=0xdbff){const next=value.charCodeAt(++i);if(!(next>=0xdc00&&next<=0xdfff))return 0;}
   else if(code>=0xdc00&&code<=0xdfff)return 0;
 }
 return 1;
});
EM_JS(void,browser_workbench_nonce,(char *out,int capacity),{
 const bytes=new Uint8Array(12);
 globalThis.crypto.getRandomValues(bytes);
 stringToUTF8(Array.from(bytes,b=>b.toString(16).padStart(2,'0')).join(""),out,capacity);
});
EM_JS(void,browser_output,(unsigned id,int fd),{const b=Module.editorRequest.bytes;if(!(b instanceof Uint8Array)||b.length>1048576)return;const p=_malloc(b.length);if(p){HEAPU8.set(b,p);_the_tools_wasm_output(id,fd,p,b.length);_free(p);}});
EMSCRIPTEN_KEEPALIVE void the_tools_wasm_output(unsigned id,int fd,const void *b,size_t n){the_tool_output(id,fd,b,n);}
#ifdef THE_WASM_HIGHLIGHT
EM_JS(char *, browser_parser_payload, (), {
   const value = Module.editorRequest.payload;
   if (typeof value !== 'string' || value.length > 8388608) return 0;
   const size = lengthBytesUTF8(value) + 1;
   if (size > 8388609) return 0;
   const pointer = _malloc(size);
   if (pointer) stringToUTF8(value, pointer, size);
   return pointer;
});
#endif

EM_ASYNC_JS(int, the_wasm_flush_workspace, (), {
   try { await Module.flushWorkspace(); return 0; }
   catch (error) { return 1; }
});

static void publish(void)
{
   const TheEditorSource *source = NULL;
   const TheWebWorkbenchReceipt *completed =
      the_web_workbench_receipt_complete(&workbench_receipts);
   if (completed) browser_send_workbench_receipt(completed);
#if defined(THE_WASM_HIGHLIGHT) || defined(THE_WASM_INTERPRETER)
   source = the_editor_source_prepare();
#endif
#ifdef THE_WASM_HIGHLIGHT
   the_wasm_highlight_prepare();
#endif
   publish_workbench();
   publish_panel_state();
   if (the_web_format_snapshot(snapshot, sizeof(snapshot)))
   {
      size_t used = strlen(snapshot);
      if (source && used && snapshot[used-1] == '}') {
         char metadata[128];
         int added = snprintf(metadata, sizeof(metadata),
                  ",\"source\":{\"id\":%u,\"revision\":%u}}", source->id, source->revision);
         if (added > 0 && used + added <= sizeof(snapshot))
            memcpy(snapshot + used - 1, metadata, added + 1);
      }
#ifdef THE_WASM_HIGHLIGHT
      size_t length = strlen(snapshot);
      if (length && snapshot[length-1] == '}') {
         char metadata[256];
         int added = the_wasm_highlight_metadata(metadata, sizeof(metadata));
         if (added > 0 && (size_t)added < sizeof(metadata)
             && length + added + 2 <= sizeof(snapshot)) {
            snapshot[length-1] = ',';
            memcpy(snapshot + length, metadata, added);
            strcpy(snapshot + length + added, "}");
         }
      }
#endif
      browser_send(snapshot);
      {static char tools[3600000];if(the_tools_format_json(tools,sizeof(tools)))browser_send(tools);}
   }
}

static int activate(void)
{
   char nonce[THE_WEB_WORKBENCH_NONCE_MAX + 1];
   the_tools_wasm_initialise(the_wasm_has_interpreter());
   the_input_queue_init(&inputs);
   browser_workbench_nonce(nonce,sizeof(nonce));
   the_web_workbench_receipts_init(&workbench_receipts,nonce);
   workbench_revision = 0;
   workbench_publication = 0;
   memset(&workbench_reasons, 0, sizeof(workbench_reasons));
   workbench_panel_state[0] = '\0';
   headless_driver_reset();
   return 1;
}

static int startup(const TheDriverStartupOptions *options, char *error, size_t capacity)
{
   char catalog[4096];
   char hello[256];
   (void)options; (void)error; (void)capacity;
   snprintf(hello,sizeof(hello),
      "{\"type\":\"hello\",\"protocol\":1,\"driver\":\"wasm\","
      "\"workbenchSession\":\"%s\",\"workbenchNextRequest\":%llu}",
      workbench_receipts.nonce,workbench_receipts.high_water + 1);
   browser_send(hello);
   if (the_wasm_has_interpreter()) browser_send("{\"type\":\"interpreter\",\"available\":true}");
   if (the_web_format_actions(catalog, sizeof(catalog))) browser_send(catalog);
   publish_workbench();
   publish_panel_state();
   return 1;
}

static int read_input(TheInputEvent *event)
{
   for (;;)
   {
      char type[32], value[1024], argument[THE_FRONTEND_ACTION_ARGUMENT_MAX + 1];
      const char *field;
      int queued;
      if (the_input_queue_pop(&inputs, event)) {
         if (event->kind == THE_INPUT_WORKBENCH_INVOKE
         || event->kind == THE_INPUT_WORKBENCH_PANEL
         || event->kind == THE_INPUT_WORKBENCH_FORM
         || event->kind == THE_INPUT_WORKBENCH_FILEFIELDS)
            the_web_workbench_receipt_dispatched(&workbench_receipts);
         return 1;
      }
      if (!browser_wait(input_timeout)) return 0;
      if (!browser_string("type", type, sizeof(type))) continue;
      if (strcmp(type,"workbench") == 0) {
         char operation[32], section[32], request_id[THE_WEB_WORKBENCH_REQUEST_ID_MAX + 2];
         int version = browser_integer("version",0);
         if (!browser_string("operation",operation,sizeof(operation)) || version != 1) {
            browser_send_workbench_refusal("","invalid-operand","Invalid workbench operation");
            continue;
         }
         if (!strcmp(operation,"list")) {
            char page[THE_WORKBENCH_JSON_MAX + 80];
            int offset = browser_integer("offset",0), limit = browser_integer("limit",8);
            if (browser_string("section",section,sizeof(section)) && offset >= 0
            && limit >= 1 && limit <= 8
            && the_web_format_workbench_page(page,sizeof(page),
                  section[0] ? section : "SUMMARY",(size_t)offset,(size_t)limit))
               browser_send(page);
            else browser_send_workbench_refusal("","invalid-operand","Invalid workbench list request");
            continue;
         }
         if (!strcmp(operation,"invoke")) {
            char action[THE_WORKBENCH_ID_MAX + 2];
            char *operand = browser_payload("argument",THE_WORKBENCH_OPERAND_MAX + 1);
            int generation = browser_integer("expectGeneration",0);
            int have_id = browser_string("requestId",request_id,sizeof(request_id));
            int have_action = browser_string("action",action,sizeof(action));
            int request_id_ok = browser_workbench_string_valid("requestId",0);
            int operands_ok = browser_workbench_string_valid("action",0)
               && browser_workbench_string_valid("argument",1);
            int accepted = request_id_ok ? the_web_workbench_receipt_begin(
               &workbench_receipts,have_id ? request_id : NULL,"invoke",
               have_action ? action : NULL,
               generation > 0 ? (unsigned long)generation : 0,operand) : -1;
            if (accepted == 1) {
               TheInputEvent next;
               if (!operands_ok || !have_action || generation <= 0
               || !the_input_event_from_workbench_invoke(action,
                     (unsigned long)generation,operand,&next)
               || !the_input_queue_push(&inputs,next))
                  the_web_workbench_receipt_refuse(&workbench_receipts,
                     "invalid-operand",5,"Invalid workbench action or input queue full");
            }
            if (accepted == 1 || accepted == 0 || accepted == -3)
               browser_send_workbench_receipt(the_web_workbench_receipt_find(
                  &workbench_receipts,request_id));
            else browser_send_workbench_refusal(have_id ? request_id : "",
               accepted == -4 ? "request-id-conflict" : accepted == -6
                  ? "receipt-unknown" : accepted == -5
                  ? "invalid-operand" : "invalid-request-id",
               accepted == -4 ? "Request ID was already used for another action"
                  : accepted == -6 ? "Receipt expired or unknown; reconcile before retrying"
                  : accepted == -5 ? "Invalid workbench action or argument"
                  : "Invalid workbench request ID");
            free(operand);
            continue;
         }
         if (!strcmp(operation,"log.list")) {
            char model[THE_WORKBENCH_ID_MAX + 2], generation_text[32], cursor_text[32];
            uint64_t generation = 0, cursor = 0;
            int offset = browser_integer("offset",-1), records = browser_integer("records",-1);
            int bytes = browser_integer("bytes",-1);
            int valid = browser_workbench_string_valid("model",0)
               && browser_workbench_string_valid("generation",0)
               && browser_workbench_string_valid("cursor",0)
               && browser_string("model",model,sizeof(model))
               && browser_string("generation",generation_text,sizeof(generation_text))
               && browser_string("cursor",cursor_text,sizeof(cursor_text))
               && the_web_parse_u64(generation_text,&generation) && generation
               && the_web_parse_u64(cursor_text,&cursor)
               && offset >= 0 && offset <= 262144 && records >= 1 && records <= 8
               && bytes >= 1 && bytes <= THE_WORKBENCH_LOG_JSON_BYTES_MAX;
            if (valid) {
               static char page[THE_WORKBENCH_JSON_MAX + 80];
               if (the_web_format_log_page(page,sizeof(page),model,generation,cursor,
                     (size_t)offset,(size_t)records,(size_t)bytes)) {
                  browser_send(page);
                  continue;
               }
            }
            {
               const TheWorkbenchResult *result = valid ? the_workbench_last_result() : NULL;
               browser_send_workbench_refusal("",
                  result && result->code && strcmp(result->code,"ok") ? result->code : "invalid-operand",
                  result && result->code && strcmp(result->code,"ok") ? result->message : "Invalid log page request");
            }
            continue;
         }
         if (!strcmp(operation,"form.list")) {
            char form[THE_WORKBENCH_ID_MAX+2],page[THE_WORKBENCH_JSON_MAX+80];
            int offset=browser_integer("offset",-1),limit=browser_integer("records",-1),bytes=browser_integer("bytes",-1);
            if (browser_workbench_string_valid("form",0) && browser_string("form",form,sizeof(form))
            && offset>=0 && limit>=1 && limit<=8 && bytes>=1 && bytes<=8192
            && the_web_format_form_page(page,sizeof(page),form,(size_t)offset,(size_t)limit,(size_t)bytes,0)) browser_send(page);
            else browser_send_workbench_refusal("","invalid-operand","Invalid form page request");
            continue;
         }
         if (!strcmp(operation,"filefield.list")) {
            char panel[THE_WORKBENCH_ID_MAX+2],generation_text[32],revision_text[32];
            char page[THE_WORKBENCH_JSON_MAX+80];
            uint64_t generation=0,revision=0;
            int offset=browser_integer("offset",-1),limit=browser_integer("records",-1),bytes=browser_integer("bytes",-1);
            const TheWorkbenchPanel *definition;
            int valid=browser_workbench_string_valid("panel",0)
               && browser_workbench_string_valid("expectGeneration",0)
               && browser_workbench_string_valid("expectRevision",0)
               && browser_string("panel",panel,sizeof(panel))
               && browser_string("expectGeneration",generation_text,sizeof(generation_text))
               && browser_string("expectRevision",revision_text,sizeof(revision_text))
               && the_web_parse_u64(generation_text,&generation) && generation && generation<=ULONG_MAX
               && the_web_parse_u64(revision_text,&revision) && revision
               && offset>=0 && offset<=THE_WORKBENCH_FILEFIELDS_RECORDS_MAX
               && limit>=1 && limit<=8 && bytes>=1 && bytes<=8192;
            definition=valid ? the_workbench_panel_find(panel) : NULL;
            if (valid && definition && definition->generation==generation) {
               const TheWorkbenchFileFieldsState *state=the_workbench_filefields_state(panel);
               if (state && state->draft_revision==revision
               && the_web_format_filefield_page(page,sizeof(page),panel,(size_t)offset,(size_t)limit,(size_t)bytes)) {
                  browser_send(page); continue;
               }
            }
            {
               const TheWorkbenchFileFieldsState *state=valid ? the_workbench_filefields_state(panel) : NULL;
               const TheWorkbenchResult *result=valid && state && state->draft_revision==revision
                  && definition && definition->generation==generation ? the_workbench_last_result() : NULL;
               browser_send_workbench_refusal("",!valid ? "invalid-operand"
                  : !definition || definition->generation!=generation ? "stale-generation"
                  : !state || state->draft_revision!=revision ? "stale-revision"
                  : result && result->code ? result->code : "invalid-operand",
                  result && result->message ? result->message : !valid ? "Invalid file-field page request"
                  : "File fields changed; reload projection");
            }
            continue;
         }
         if (!strcmp(operation,"filefield.set") || !strcmp(operation,"filefield.edit")
         || !strcmp(operation,"filefield.apply")
         || !strcmp(operation,"filefield.cancel") || !strcmp(operation,"filefield.reload")) {
            TheWorkbenchFileFieldsRequest request; TheInputEvent event;
            char generation_text[32],revision_text[32],edit_name[16]; uint64_t generation=0;
            int valid,accepted,have_id=browser_string("requestId",request_id,sizeof(request_id));
            memset(&request,0,sizeof(request));
            request.operation=!strcmp(operation,"filefield.set") ? THE_WORKBENCH_FILEFIELDS_SET
               : !strcmp(operation,"filefield.edit") ? THE_WORKBENCH_FILEFIELDS_EDIT
               : !strcmp(operation,"filefield.apply") ? THE_WORKBENCH_FILEFIELDS_APPLY
               : !strcmp(operation,"filefield.cancel") ? THE_WORKBENCH_FILEFIELDS_CANCEL : THE_WORKBENCH_FILEFIELDS_RELOAD;
            valid=browser_workbench_string_valid("panel",0)
               && browser_string("panel",request.panel,sizeof(request.panel))
               && browser_workbench_string_valid("expectGeneration",0)
               && browser_string("expectGeneration",generation_text,sizeof(generation_text))
               && browser_workbench_string_valid("expectRevision",0)
               && browser_string("expectRevision",revision_text,sizeof(revision_text))
               && the_web_parse_u64(generation_text,&generation) && generation && generation<=ULONG_MAX
               && the_web_parse_u64(revision_text,&request.draft_revision) && request.draft_revision;
            if (valid && request.operation==THE_WORKBENCH_FILEFIELDS_SET)
               valid=browser_workbench_string_valid("field",0)
                  && browser_string("field",request.field,sizeof(request.field))
                  && browser_workbench_string_valid("value",0) && browser_form_value_valid()
                  && browser_string("value",request.value,sizeof(request.value));
            if (valid && request.operation==THE_WORKBENCH_FILEFIELDS_EDIT) {
               int offset=browser_integer("offset",-1),codepoint=browser_integer("codepoint",-1);
               valid=browser_workbench_string_valid("field",0)
                  && browser_string("field",request.field,sizeof(request.field))
                  && browser_workbench_string_valid("edit",0)
                  && browser_string("edit",edit_name,sizeof(edit_name))
                  && offset>=0 && offset<=THE_WORKBENCH_FIELD_VALUE_MAX;
               request.edit=!valid ? 0 : !strcmp(edit_name,"insert") ? THE_WORKBENCH_FORM_INSERT
                  : !strcmp(edit_name,"backspace") ? THE_WORKBENCH_FORM_BACKSPACE
                  : !strcmp(edit_name,"delete") ? THE_WORKBENCH_FORM_DELETE
                  : !strcmp(edit_name,"clear") ? THE_WORKBENCH_FORM_CLEAR : 0;
               valid=valid && request.edit && (request.edit!=THE_WORKBENCH_FORM_INSERT
                  || codepoint>=0 && codepoint<=0x10ffff
                     && !(codepoint>=0xd800 && codepoint<=0xdfff));
               request.cell=-1; request.utf16_offset=(size_t)offset;
               request.codepoint=codepoint<0 ? 0u : (uint32_t)codepoint;
            }
            request.panel_generation=(unsigned long)generation;
            request.length=strlen(request.value);
            accepted=have_id && browser_workbench_string_valid("requestId",0)
               ? valid ? the_web_workbench_filefield_begin(&workbench_receipts,request_id,&request,&event) : -5 : -1;
            if (accepted==1 && !the_input_queue_push(&inputs,event))
               the_web_workbench_receipt_refuse(&workbench_receipts,"invalid-operand",5,"File-field input queue full");
            if (accepted==1 || accepted==0 || accepted==-3)
               browser_send_workbench_receipt(the_web_workbench_receipt_find(&workbench_receipts,request_id));
            else browser_send_workbench_refusal(have_id ? request_id : "",
               accepted==-4 ? "request-id-conflict" : accepted==-6 ? "receipt-unknown"
                  : accepted==-5 ? "invalid-operand" : "invalid-request-id",
               "Invalid or conflicting file-field request");
            continue;
         }
         if (!strcmp(operation,"form.set") || !strcmp(operation,"form.submit") || !strcmp(operation,"form.cancel")) {
            TheWorkbenchFormRequest request; TheInputEvent event;
            char generation[32],revision[32],panel_generation[32]; uint64_t gen=0,pgen=0;
            int valid,accepted,have_id=browser_string("requestId",request_id,sizeof(request_id));
            memset(&request,0,sizeof(request));
            request.operation=!strcmp(operation,"form.set") ? THE_WORKBENCH_FORM_SET
               : !strcmp(operation,"form.submit") ? THE_WORKBENCH_FORM_SUBMIT : THE_WORKBENCH_FORM_CANCEL;
            valid=browser_workbench_string_valid("form",0) && browser_string("form",request.form,sizeof(request.form))
               && browser_workbench_string_valid("expectGeneration",0) && browser_string("expectGeneration",generation,sizeof(generation))
               && browser_workbench_string_valid("expectRevision",0) && browser_string("expectRevision",revision,sizeof(revision))
               && browser_workbench_string_valid("panel",1) && browser_string("panel",request.panel,sizeof(request.panel))
               && browser_workbench_string_valid("expectPanelGeneration",1) && browser_string("expectPanelGeneration",panel_generation,sizeof(panel_generation))
               && the_web_parse_u64(generation,&gen) && gen && gen<=ULONG_MAX
               && the_web_parse_u64(revision,&request.draft_revision) && request.draft_revision
               && (!request.panel[0] || the_web_parse_u64(panel_generation,&pgen) && pgen && pgen<=ULONG_MAX);
            if (valid && request.operation==THE_WORKBENCH_FORM_SET)
               valid=browser_workbench_string_valid("field",0) && browser_string("field",request.field,sizeof(request.field))
                  && browser_form_value_valid() && browser_string("value",request.value,sizeof(request.value));
            request.form_generation=(unsigned long)gen;request.panel_generation=(unsigned long)pgen;
            request.length=strlen(request.value);
            accepted=have_id && browser_workbench_string_valid("requestId",0)
               ? valid ? the_web_workbench_form_begin(&workbench_receipts,request_id,&request,&event) : -5 : -1;
            if (accepted==1 && !the_input_queue_push(&inputs,event))
               the_web_workbench_receipt_refuse(&workbench_receipts,"invalid-operand",5,"Form input queue full");
            if (accepted==1 || accepted==0 || accepted==-3)
               browser_send_workbench_receipt(the_web_workbench_receipt_find(&workbench_receipts,request_id));
            else browser_send_workbench_refusal(have_id ? request_id : "",
               accepted==-4 ? "request-id-conflict" : accepted==-6 ? "receipt-unknown" : accepted==-5 ? "invalid-operand" : "invalid-request-id",
               "Invalid or conflicting form request");
            continue;
         }
         if (!strcmp(operation,"document.list")) {
            char panel[THE_WORKBENCH_ID_MAX + 2], generation_text[32], revision_text[32];
            uint64_t generation = 0, revision = 0;
            int line = browser_integer("line",-1), offset = browser_integer("offset",-1);
            int records = browser_integer("records",-1), bytes = browser_integer("bytes",-1);
            int valid = browser_workbench_string_valid("panel",0)
               && browser_workbench_string_valid("expectGeneration",0)
               && browser_workbench_string_valid("expectRevision",0)
               && browser_string("panel",panel,sizeof(panel))
               && browser_string("expectGeneration",generation_text,sizeof(generation_text))
               && browser_string("expectRevision",revision_text,sizeof(revision_text))
               && the_web_parse_u64(generation_text,&generation) && generation
               && the_web_parse_u64(revision_text,&revision) && revision
               && line >= 1 && offset >= 0 && offset <= THE_WORKBENCH_DOCUMENT_RECORD_MAX
               && records >= 1 && records <= THE_WORKBENCH_DOCUMENT_LINES_MAX
               && bytes >= 1 && bytes <= THE_WORKBENCH_DOCUMENT_BYTES_MAX;
            const TheWorkbenchPanel *definition = valid ? the_workbench_panel_find(panel) : NULL;
            int stale = valid && (!definition || definition->generation != generation);
            if (valid && !stale) {
               static char page[THE_WORKBENCH_JSON_MAX + 80];
               if (the_web_format_document_page(page,sizeof(page),panel,revision,
                     (size_t)line,(size_t)offset,(size_t)records,(size_t)bytes)) {
                  browser_send(page);
                  continue;
               }
            }
            {
               const TheWorkbenchResult *result = valid && !stale ? the_workbench_last_result() : NULL;
               browser_send_workbench_refusal("",stale ? "stale-generation"
                  : result && result->code && strcmp(result->code,"ok") ? result->code : "invalid-operand",
                  stale ? "Panel generation is stale" : result && result->code && strcmp(result->code,"ok")
                     ? result->message : "Invalid document page request");
            }
            continue;
         }
         if (!strcmp(operation,"panel.layout")) {
            char panel[THE_WORKBENCH_ID_MAX + 2], generation_text[32], placement[32];
            uint64_t generation = 0;
            int cols = browser_integer("cols",-1), rows = browser_integer("rows",-1);
            if (browser_workbench_string_valid("panel",0)
            && browser_workbench_string_valid("expectGeneration",0)
            && browser_workbench_string_valid("placement",0)
            && browser_string("panel",panel,sizeof(panel))
            && browser_string("expectGeneration",generation_text,sizeof(generation_text))
            && browser_string("placement",placement,sizeof(placement))
            && the_web_parse_u64(generation_text,&generation)
            && cols >= 0 && cols <= 10000 && rows >= 0 && rows <= 10000)
               the_web_panel_layout(panel,generation,placement,cols,rows);
            publish_panel_state();
            continue;
         }
         if (!strcmp(operation,"panel.show") || !strcmp(operation,"panel.hide")
         || !strcmp(operation,"panel.focus") || !strcmp(operation,"panel.return")) {
            char panel[THE_WORKBENCH_ID_MAX + 2], generation_text[32];
            uint64_t generation = 0;
            int have_id = browser_string("requestId",request_id,sizeof(request_id));
            int have_panel = browser_string("panel",panel,sizeof(panel));
            int have_generation = browser_string("expectGeneration",generation_text,sizeof(generation_text));
            int operands_ok = browser_workbench_string_valid("panel",0)
               && browser_workbench_string_valid("expectGeneration",0)
               && have_panel && have_generation
               && the_web_parse_u64(generation_text,&generation)
               && generation > 0 && generation <= ULONG_MAX;
            TheWorkbenchPanelOperation panel_operation =
               !strcmp(operation,"panel.show") ? THE_WORKBENCH_PANEL_SHOW
             : !strcmp(operation,"panel.hide") ? THE_WORKBENCH_PANEL_HIDE
             : !strcmp(operation,"panel.focus") ? THE_WORKBENCH_PANEL_FOCUS
             : THE_WORKBENCH_PANEL_RETURN;
            int accepted = browser_workbench_string_valid("requestId",0)
               ? the_web_workbench_receipt_begin(&workbench_receipts,
                    have_id ? request_id : NULL,operation,
                    have_panel ? panel : NULL,
                    operands_ok ? (unsigned long)generation : 0,NULL) : -1;
            if (accepted == 1) {
               TheInputEvent next;
               if (!operands_ok || !the_input_event_from_workbench_panel(panel,
                     (unsigned long)generation,panel_operation,&next)
               || !the_input_queue_push(&inputs,next))
                  the_web_workbench_receipt_refuse(&workbench_receipts,
                     "invalid-operand",5,"Invalid panel request or input queue full");
            }
            if (accepted == 1 || accepted == 0 || accepted == -3)
               browser_send_workbench_receipt(the_web_workbench_receipt_find(
                  &workbench_receipts,request_id));
            else browser_send_workbench_refusal(have_id ? request_id : "",
               accepted == -4 ? "request-id-conflict" : accepted == -6
                  ? "receipt-unknown" : accepted == -5
                  ? "invalid-operand" : "invalid-request-id",
               accepted == -4 ? "Request ID was already used for another operation"
                  : accepted == -6 ? "Receipt expired or unknown; reconcile before retrying"
                  : accepted == -5 ? "Invalid panel ID" : "Invalid workbench request ID");
            continue;
         }
         browser_send_workbench_refusal("","invalid-operand","Invalid workbench operation");
         continue;
      }
#ifdef THE_WASM_INTERPRETER
      if (strcmp(type, "run-source") == 0) {
         const TheEditorSource *source = the_editor_source_prepare();
         browser_source(browser_integer("id", 0), source ? source->id : 0,
                        source ? source->revision : 0, source ? source->bytes : "",
                        source ? source->length : 0,
                        CURRENT_FILE ? (char *)CURRENT_FILE->fpath : "",
                        CURRENT_FILE ? (char *)CURRENT_FILE->fname : "",
                        source ? "" : the_editor_source_error());
         publish();
         continue;
      }
#endif
#ifdef THE_WASM_HIGHLIGHT
      if (strcmp(type, "parser-restart") == 0) {
         the_wasm_highlight_restart();
         publish();
         continue;
      }
      if (strcmp(type, "parser-result") == 0) {
         char *payload = browser_parser_payload();
         the_wasm_highlight_reply(browser_integer("id", 0), browser_integer("revision", 0),
                                  payload, browser_integer("failed", 0));
         free(payload);
         publish();
         continue;
      }
#endif
      if(!strcmp(type,"tool-import")){browser_import();publish();continue;}
      if(!strcmp(type,"tool")){
         char a[40],t[64],generation[32],*text=browser_payload("text",1048577),*args=browser_payload("arguments",1048577);TheInputEvent e;
         if(browser_string("generation",generation,sizeof(generation))&&browser_string("action",a,sizeof(a))&&browser_string("tool",t,sizeof(t))&&the_tools_prepare_lines_bound(a,t,text?text:"",args?args:"",browser_integer("sourceId",0),browser_integer("revision",0),generation,&e))the_input_queue_push(&inputs,e);
         free(text);free(args);continue;
      }
      if(!strcmp(type,"tool-event")){
         char kind[32];unsigned id=browser_integer("job",0);browser_string("event",kind,sizeof(kind));
         if(!strcmp(kind,"output"))browser_output(id,browser_integer("fd",1));
         else if(!strcmp(kind,"started"))the_tool_started(id);
         else if(!strcmp(kind,"waiting"))the_tool_waiting(id,browser_integer("waiting",0));
         else if(!strcmp(kind,"built")){char path[THE_TOOL_PATH];if(browser_string("path",path,sizeof(path)))the_tools_profile_built(id,path,browser_integer("sourceId",0),browser_integer("revision",0));}
         else if(!strcmp(kind,"finish")){char *wire=browser_payload("wire",3145729),*error=browser_payload("error",4097),status[32];browser_string("status",status,sizeof(status));the_tool_finish(id,browser_integer("code",-1),status,error,wire);free(wire);free(error);}
         publish();continue;
      }
      if (strcmp(type, "snapshot") == 0) { publish(); continue; }
      field = strcmp(type,"key")==0 ? "key" : strcmp(type,"text")==0 ? "text"
            : strcmp(type,"command")==0 ? "command" : strcmp(type,"action")==0 ? "action" : "target";
      queued = browser_string(field, value, sizeof(value))
            && browser_string("argument", argument, sizeof(argument))
            && the_web_queue_request(&inputs, type, value, argument,
                 browser_integer("line",0), browser_integer("row",-1),
                 browser_integer("cell",-1), browser_integer("rows",0),
                 browser_integer("cols",0));
      if (!queued) browser_send("{\"type\":\"error\",\"message\":\"Invalid browser input or input queue full\"}");
   }
}

static void update(void)
{
   if (base_update) base_update();
   publish();
}

static void set_timeout(int milliseconds) { input_timeout = milliseconds; }
static const char *ui_version(void) { return "web/wasm"; }
static CursorPresentation cursor_presentation(void) { return CURSOR_PRESENTATION_SOFTWARE; }
static bool software_cursor(void) { return TRUE; }
static void shutdown_driver(int prompt) { (void)prompt; headless_driver_reset(); }

const TheDriverModuleLifecycle the_wasm_driver_lifecycle = {
   .name = "web", .activate = activate, .start = startup,
   .shutdown = shutdown_driver,
   .set_current_screen = headless_driver_set_current_screen,
   .set_screen_current_role = headless_driver_set_screen_current_role,
   .create_screen_role = headless_driver_create_screen_role,
   .create_global_window = headless_driver_create_global_window,
   .log_count = headless_driver_log_count,
   .log_entry = headless_driver_log_entry,
   .ui_version = ui_version,
   .current_cursor_presentation = cursor_presentation,
   .current_cursor_uses_software = software_cursor
};

const TheDriverOps *the_wasm_driver_operations(void)
{
   operations = the_headless_driver_ops;
   base_update = operations.update;
   operations.read_input_event = read_input;
   operations.set_current_window_timeout = set_timeout;
   operations.update = update;
   return &operations;
}
