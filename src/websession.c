#include <ctype.h>
#include <limits.h>
#include <inttypes.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "websession.h"
#include "frontendaction.h"
#include "workbench.h"
#include "workbenchruntime.h"
#include "workbenchdocument.h"
#include "headlessdriver.h"
#include "llmdriver.h"
#include "llmruntime.h"
#include "panelui.h"
#include "the.h"
#include "proto.h"
#include "driverlayout.h"
#include "driverwindow.h"
#include "thedriver.h"
#include "vars.h"

static void web_driver_seed_filearea_focus(int row, int display_col)
{
   SHOW_LINE *show_row;
   const CHARTYPE *line;
   size_t len;
   int logical_col;
   LogicalCursor cursor;

   if (CURRENT_VIEW == NULL || screen[current_screen].sl == NULL
   ||  row < 0 || row >= screen[current_screen].rows[WINDOW_FILEAREA])
      return;
   show_row = &screen[current_screen].sl[row];
   if (show_row->line_type & (LINE_OUT_OF_BOUNDS_ABOVE
                            | LINE_OUT_OF_BOUNDS_BELOW))
      return;
   if (show_row->line_type == LINE_TOF || show_row->line_type == LINE_EOF)
   {
      line = (const CHARTYPE *)"";
      len = 0;
   }
   else
   {
      line = show_row->contents != NULL ? show_row->contents : rec;
      len = show_row->contents != NULL ? show_row->length : rec_len;
   }
   logical_col = driver_layout_logical_col_from_display(
      line, len, (int)CURRENT_VIEW->verify_col - 1, display_col,
      TEXT_SNAP_BACKWARD);
   cursor = logical_cursor_from_cell(
      LOGICAL_CURSOR_ZONE_FILEAREA, show_row->line_number, row, line, len,
      logical_col, TEXT_SNAP_BACKWARD, 1);
   logical_cursor_set_desired_cell(
      &cursor, driver_layout_width_col_from_logical(line, len,
                                                    cursor.text.cell_column));
   logical_cursor_state_focus(&CURRENT_VIEW->logical_cursor, cursor);
}

static void web_driver_sync_focus(void)
{
   LogicalCursor logical;
   TheDriverWindow *window;
   int row = 0;
   int col = 0;

   if (CURRENT_VIEW == NULL)
      return;
   logical = CURRENT_VIEW->logical_cursor.current;
   if (!logical.valid && CURRENT_VIEW->current_window == WINDOW_COMMAND)
   {
      CURRENT_VIEW->previous_window = CURRENT_VIEW->current_window;
      CURRENT_VIEW->current_window = WINDOW_FILEAREA;
   }
   the_driver_set_current_screen(current_screen);
   the_driver_set_screen_current_role(current_screen,
                                      CURRENT_VIEW->current_window);
   if (!logical.valid)
   {
      window = driver_current_window();
      if (CURRENT_VIEW->current_window == WINDOW_FILEAREA
      ||  CURRENT_VIEW->current_window == WINDOW_PREFIX)
      {
         row = get_row_for_focus_line(current_screen,
                                      CURRENT_VIEW->focus_line,
                                      CURRENT_VIEW->current_row);
         if (row < 0)
            row = CURRENT_VIEW->current_row;
         col = CURRENT_VIEW->x[CURRENT_VIEW->current_window];
      }
      else if (CURRENT_VIEW->current_window == WINDOW_COMMAND)
         col = CURRENT_VIEW->cmdline_col;
      if (window != NULL)
         the_driver->move_window_cursor(window, (short)row, (short)col);
      if (CURRENT_VIEW->current_window == WINDOW_FILEAREA)
         web_driver_seed_filearea_focus(row, col);
   }
   cursor_focus_sync_current(current_screen, CURRENT_VIEW);
}

size_t the_web_format_actions(char *catalog, size_t capacity)
{
   size_t used = 0;
   size_t i;
   int len;

   len = snprintf(catalog, capacity,
                  "{\"type\":\"actions\",\"actions\":[");
   if (len < 0 || (size_t)len >= capacity)
      return 0;
   used = (size_t)len;
   for (i = 0; i < the_frontend_action_definition_count(); i++)
   {
      const TheFrontendActionDefinition *definition =
         the_frontend_action_definition_at(i);

      len = snprintf(catalog + used, capacity - used,
                     "%s{\"id\":\"%s\",\"menu\":%s%s%s,"
                     "\"label\":\"%s\",\"requires_argument\":%s}",
                     i == 0 ? "" : ",", definition->name,
                     definition->menu != NULL ? "\"" : "null",
                     definition->menu != NULL ? definition->menu : "",
                     definition->menu != NULL ? "\"" : "",
                     definition->label,
                     definition->requires_argument ? "true" : "false");
      if (len < 0 || (size_t)len >= capacity - used)
         return 0;
      used += (size_t)len;
   }
   len = snprintf(catalog + used, capacity - used, "]}");
   if (len < 0 || (size_t)len >= capacity - used)
      return 0;
   used += (size_t)len;
   return used;
}

void the_web_publish_workbench(void (*send)(const char *, size_t, void *),
                               void *context, unsigned long publication)
{
   static const char *const sections[] = { "ACTIONS", "MENUS", "ITEMS", "PANELS" };
   char page[THE_WORKBENCH_JSON_MAX];
   char message[THE_WORKBENCH_JSON_MAX + 80];
   size_t totals[4], section, offset;
   int prefix;

   if (send == NULL)
      return;
   totals[0] = the_workbench_action_count();
   totals[1] = the_workbench_menu_count();
   totals[2] = the_workbench_item_count();
   totals[3] = the_workbench_panel_count();
   prefix = snprintf(message, sizeof(message),
                     "{\"type\":\"workbench\",\"operation\":\"list\",\"publication\":%lu,\"data\":",
                     publication);
   if (prefix <= 0 || (size_t)prefix >= sizeof(message))
      return;
   for (section = 0; section < 4; section++)
   {
      for (offset = 0; offset < totals[section] || offset == 0;
           offset += THE_WORKBENCH_PAGE_MAX)
      {
         size_t length;
         if (!the_workbench_json(page, sizeof(page), sections[section],
                                 offset, THE_WORKBENCH_PAGE_MAX, 0))
            return;
         length = strlen(page);
         if ((size_t)prefix + length + 2 > sizeof(message))
            return;
         memcpy(message + prefix, page, length);
         message[prefix + length] = '}';
         message[prefix + length + 1] = '\0';
         send(message, (size_t)prefix + length + 1, context);
      }
   }
}

int the_web_workbench_reasons_changed(TheWebWorkbenchReasonState *state)
{
   TheWebWorkbenchReasonState current;
   size_t index;
   if (state == NULL) return 0;
   memset(&current, 0, sizeof(current));
   current.count = the_workbench_action_count();
   if (current.count > THE_WORKBENCH_ACTION_MAX) return 0;
   for (index = 0; index < current.count; index++)
      snprintf(current.reasons[index], sizeof(current.reasons[index]), "%s",
               the_workbench_action_reason(the_workbench_action_at(index)));
   if (current.count == state->count
   && memcmp(current.reasons, state->reasons, sizeof(current.reasons)) == 0)
      return 0;
   *state = current;
   return 1;
}

size_t the_web_format_workbench_page(char *out, size_t capacity,
                                     const char *section, size_t offset,
                                     size_t limit)
{
   char page[THE_WORKBENCH_JSON_MAX];
   char upper[32];
   size_t i, length;
   int prefix;
   if (out == NULL || section == NULL || strlen(section) >= sizeof(upper))
      return 0;
   for (i = 0; section[i]; i++)
      upper[i] = (char)toupper((unsigned char)section[i]);
   upper[i] = '\0';
   if (!the_workbench_json(page, sizeof(page), upper, offset, limit, 0))
      return 0;
   prefix = snprintf(out, capacity,
                     "{\"type\":\"workbench\",\"operation\":\"list\",\"data\":");
   length = strlen(page);
   if (prefix < 0 || (size_t)prefix + length + 2 > capacity)
      return 0;
   memcpy(out + prefix, page, length);
   out[prefix + length] = '}';
   out[prefix + length + 1] = '\0';
   return (size_t)prefix + length + 1;
}

void the_web_workbench_receipts_init(TheWebWorkbenchReceipts *receipts,
                                     const char *nonce)
{
   if (receipts == NULL) return;
   memset(receipts, 0, sizeof(*receipts));
   receipts->pending_index = -1;
   if (nonce != NULL && strlen(nonce) <= THE_WEB_WORKBENCH_NONCE_MAX)
      strcpy(receipts->nonce, nonce);
}

const TheWebWorkbenchReceipt *the_web_workbench_receipt_find(
   const TheWebWorkbenchReceipts *receipts, const char *request_id)
{
   size_t i;
   if (receipts == NULL || request_id == NULL) return NULL;
   for (i = 0; i < receipts->count; i++)
      if (strcmp(receipts->entries[i].request_id, request_id) == 0)
         return &receipts->entries[i];
   return NULL;
}

int the_web_workbench_receipt_begin(TheWebWorkbenchReceipts *receipts,
                                    const char *request_id, const char *operation,
                                    const char *action,
                                    unsigned long generation,
                                    const char *argument)
{
   const unsigned char *p;
   TheWebWorkbenchReceipt *entry;
   const TheWebWorkbenchReceipt *previous;
   unsigned long long sequence = 0;
   size_t nonce_length, request_length, index;
   if (receipts == NULL || request_id == NULL || !receipts->nonce[0]
   || (request_length = strlen(request_id)) > THE_WEB_WORKBENCH_REQUEST_ID_MAX) return -1;
   nonce_length = strlen(receipts->nonce);
   if (request_length <= nonce_length
   || strncmp(request_id, receipts->nonce, nonce_length) != 0
   || request_id[nonce_length] != '.') return -1;
   p = (const unsigned char *)request_id + nonce_length + 1;
   if (!*p || (*p == '0' && p[1])) return -1;
   for (; *p; p++)
   {
      if (*p < '0' || *p > '9'
      || sequence > (ULLONG_MAX - (*p - '0')) / 10) return -1;
      sequence = sequence * 10 + (*p - '0');
   }
   if (!sequence) return -1;
   if (operation == NULL || !*operation || strlen(operation) >= sizeof(entry->operation)
   || action == NULL || strlen(action) > THE_WORKBENCH_ID_MAX
   || argument != NULL && strlen(argument) > THE_WORKBENCH_OPERAND_MAX)
      return -5;
   previous = the_web_workbench_receipt_find(receipts, request_id);
   if (previous)
      return previous->generation == generation
         && previous->has_argument == (argument != NULL)
         && strcmp(previous->operation, operation) == 0
         && strcmp(previous->action, action) == 0
         && (argument == NULL || strcmp(previous->argument, argument) == 0)
         ? 0 : -4;
   if (sequence <= receipts->high_water) return -6;
   receipts->high_water = sequence;
   if (receipts->count < THE_WEB_WORKBENCH_RECEIPTS)
      index = receipts->count++;
   else
   {
      index = receipts->next_slot;
      if ((int)index == receipts->pending_index)
         index = (index + 1) % THE_WEB_WORKBENCH_RECEIPTS;
   }
   receipts->next_slot = (index + 1) % THE_WEB_WORKBENCH_RECEIPTS;
   entry = &receipts->entries[index];
   memset(entry, 0, sizeof(*entry));
   strcpy(entry->request_id, request_id);
   strcpy(entry->operation, operation);
   strcpy(entry->action, action);
   if (argument != NULL) strcpy(entry->argument, argument);
   entry->has_argument = argument != NULL;
   entry->generation = generation;
   entry->revision = the_workbench_revision();
   if (receipts->pending_index >= 0)
   {
      strcpy(entry->code, "busy");
      strcpy(entry->message, "Another workbench action is pending");
      entry->rc = -3;
      return -3;
   }
   strcpy(entry->code, "queued");
   strcpy(entry->message, "queued");
   entry->pending = 1;
   receipts->pending_index = (int)index;
   return 1;
}

void the_web_workbench_receipt_refuse(TheWebWorkbenchReceipts *receipts,
                                      const char *code, short rc,
                                      const char *message)
{
   TheWebWorkbenchReceipt *entry;
   if (receipts == NULL || receipts->pending_index < 0) return;
   entry = &receipts->entries[receipts->pending_index];
   snprintf(entry->code, sizeof(entry->code), "%s", code ? code : "invalid-operand");
   snprintf(entry->message, sizeof(entry->message), "%s", message ? message : "Invalid workbench request");
   entry->rc = rc;
   entry->revision = the_workbench_revision();
   entry->pending = 0;
   receipts->pending_index = -1;
}

void the_web_workbench_receipt_dispatched(TheWebWorkbenchReceipts *receipts)
{
   if (receipts != NULL && receipts->pending_index >= 0)
   {
      const TheWorkbenchFormReceipt *form=the_workbench_form_receipt();
      receipts->dispatch_serial = the_workbench_invocation_serial();
      receipts->form_serial = form ? form->serial : 0;
   }
}

const TheWebWorkbenchReceipt *the_web_workbench_receipt_complete(
   TheWebWorkbenchReceipts *receipts)
{
   TheWebWorkbenchReceipt *entry;
   const TheWorkbenchResult *result;
   const TheWorkbenchFormReceipt *form=NULL;
   if (receipts == NULL || receipts->pending_index < 0) return NULL;
   entry = &receipts->entries[receipts->pending_index];
   if (entry->form_request_id)
   {
      form=the_workbench_form_receipt();
      if (!form || form->serial==receipts->form_serial || form->request_id!=entry->form_request_id
      || strcmp(form->form,entry->action)) return NULL;
      entry->draft_revision=form->draft_revision;
      entry->result_revision=form->result_revision;
      entry->caret=form->caret;
   }
   else if (the_workbench_invocation_serial() == receipts->dispatch_serial) return NULL;
   result = the_workbench_last_result();
   snprintf(entry->code, sizeof(entry->code), "%s", form ? form->code : result->code);
   snprintf(entry->message, sizeof(entry->message), "%s", result->message);
   entry->rc = form ? form->rc : result->rc;
   entry->revision = result->revision;
   entry->pending = 0;
   receipts->pending_index = -1;
   return entry;
}

static size_t web_json_string(char *out, size_t capacity, const char *value)
{
   const unsigned char *p = (const unsigned char *)(value ? value : "");
   size_t used = 0;
   if (capacity < 3) return 0;
   out[used++] = '"';
   for (; *p; p++)
   {
      if (used + 7 >= capacity) return 0;
      if (*p == '"' || *p == '\\') { out[used++] = '\\'; out[used++] = (char)*p; }
      else if (*p < 0x20) used += (size_t)snprintf(out + used, capacity - used,
                                                  "\\u%04x", (unsigned)*p);
      else out[used++] = (char)*p;
   }
   out[used++] = '"'; out[used] = '\0';
   return used;
}

static int web_append(char *out, size_t capacity, size_t *used,
                      const char *format, ...)
{
   va_list arguments;
   int length;
   if (*used >= capacity) return 0;
   va_start(arguments, format);
   length = vsnprintf(out + *used, capacity - *used, format, arguments);
   va_end(arguments);
   if (length < 0 || (size_t)length >= capacity - *used) return 0;
   *used += (size_t)length;
   return 1;
}

static int web_append_string(char *out, size_t capacity, size_t *used,
                             const char *value)
{
   size_t length;
   if (*used >= capacity) return 0;
   length = web_json_string(out + *used, capacity - *used, value);
   if (!length) return 0;
   *used += length;
   return 1;
}

static const char *web_placement_name(TheWorkbenchPlacement placement)
{
   switch (placement)
   {
      case THE_WORKBENCH_PLACE_RIGHT: return "right";
      case THE_WORKBENCH_PLACE_BELOW: return "below";
      case THE_WORKBENCH_PLACE_TAB: return "tab";
      default: return "none";
   }
}

static const char *web_log_error_name(TheWorkbenchLogResult result)
{
   switch (result)
   {
      case THE_WORKBENCH_LOG_OK: return "ok";
      case THE_WORKBENCH_LOG_MISSING: return "missing-object";
      case THE_WORKBENCH_LOG_STALE: return "stale-generation";
      case THE_WORKBENCH_LOG_LIMIT:
      case THE_WORKBENCH_LOG_NOMEM: return "limit-exceeded";
      default: return "invalid-operand";
   }
}

size_t the_web_format_panel_state(char *out, size_t capacity)
{
   const TheWorkbenchFocusRequest *focus = the_workbench_focus_request();
   size_t used = 0, index;
   if (out == NULL || capacity < 128 || focus == NULL) return 0;
   if (!web_append(out,capacity,&used,
      "{\"type\":\"workbench\",\"operation\":\"panel-state\",\"revision\":%lu,\"stateRevision\":\"%lu\",\"focus\":{\"serial\":\"%lu\",\"panel\":",
      the_workbench_revision(),the_workbench_panel_state_revision(),focus->serial)
   || !web_append_string(out,capacity,&used,focus->panel)
   || !web_append(out,capacity,&used,",\"generation\":\"%lu\",\"previousPanel\":",focus->generation)
   || !web_append_string(out,capacity,&used,focus->previous_panel)
   || !web_append(out,capacity,&used,
      ",\"previousGeneration\":\"%lu\",\"restoring\":%s},\"panels\":[",
      focus->previous_generation,focus->restoring ? "true" : "false")) return 0;
   for (index = 0; index < the_workbench_panel_count(); index++)
   {
      const TheWorkbenchPanel *panel = the_workbench_panel_at(index);
      const TheWorkbenchPanelState *state = panel ? the_workbench_panel_state(panel->id) : NULL;
      const TheWorkbenchLogInfo *log = panel ? the_workbench_log_find(panel->model) : NULL;
      const TheWorkbenchModelState *model = panel ? the_workbench_model_state(panel->model) : NULL;
      TheWorkbenchDocumentInfo document;
      if (panel == NULL) return 0;
      if (!web_append(out,capacity,&used,"%s{\"id\":",index ? "," : "")
      || !web_append_string(out,capacity,&used,panel->id)
      || !web_append(out,capacity,&used,
         ",\"generation\":\"%lu\",\"visible\":%s,\"focused\":%s,\"effectivePlacement\":",
         panel->generation,state && state->visible ? "true" : "false",
         state && state->focused ? "true" : "false")
      || !web_append_string(out,capacity,&used,
         web_placement_name(state ? state->effective : THE_WORKBENCH_PLACE_NONE))
      || !web_append(out,capacity,&used,",\"reason\":")
      || !web_append_string(out,capacity,&used,state ? state->reason : "unavailable-renderer")
      || !web_append(out,capacity,&used,",\"model\":")
      || !web_append_string(out,capacity,&used,panel->model)
      || !web_append(out,capacity,&used,
         ",\"modelGeneration\":\"%" PRIu64 "\",\"contentRevision\":\"%" PRIu64 "\",\"omittedBytes\":\"%" PRIu64 "\",\"omittedRecords\":\"%" PRIu64 "\",\"modelError\":",
         panel->model_generation,log ? log->revision : 0,
         log ? log->omitted_bytes : 0,log ? log->omitted_records : 0)
      || !web_append_string(out,capacity,&used,
         model ? web_log_error_name(model->last_error) : "ok")) return 0;
      if (panel->kind == THE_WORKBENCH_PANEL_DOCUMENT)
      {
         memset(&document,0,sizeof(document));
         the_workbench_document_info(panel->document_id,&document);
         if (!web_append(out,capacity,&used,
            ",\"kind\":\"document\",\"readonly\":true,\"documentId\":\"%" PRIu64 "\",\"documentRevision\":\"%" PRIu64 "\",\"documentAvailable\":%s,\"lineCount\":%zu,\"documentReason\":",
            panel->document_id,document.revision,document.available ? "true" : "false",document.lines)
         || !web_append_string(out,capacity,&used,document.reason ? document.reason : "unavailable-document")) return 0;
      }
      if (panel->kind == THE_WORKBENCH_PANEL_FIELDS)
      {
         const TheWorkbenchFormState *form=the_workbench_form_state(panel->form);
         if (!web_append(out,capacity,&used,",\"kind\":\"fields\",\"form\":")
         || !web_append_string(out,capacity,&used,panel->form)
         || !web_append(out,capacity,&used,",\"formGeneration\":\"%lu\",\"draftRevision\":\"%" PRIu64 "\",\"resultRevision\":\"%" PRIu64 "\",\"formDirty\":%s,\"submitted\":%s,\"fieldCount\":%zu",
            panel->form_generation,form ? form->draft_revision : 0,form ? form->result_revision : 0,
            form && form->dirty ? "true" : "false",form && form->submitted ? "true" : "false",form ? form->fields : 0)) return 0;
      }
      if (!web_append(out,capacity,&used,"}")) return 0;
   }
   if (!web_append(out,capacity,&used,"]}")) return 0;
   return used;
}

size_t the_web_format_log_page(char *out, size_t capacity,
                               const char *id, uint64_t generation,
                               uint64_t cursor, size_t offset,
                               size_t records, size_t bytes)
{
   char page[THE_WORKBENCH_JSON_MAX];
   size_t used = 0, length;
   if (out == NULL || !id || capacity < 128
   || the_workbench_log_json(page,sizeof(page),id,generation,cursor,
                             offset,records,bytes) != 0) return 0;
   length = strlen(page);
   if (!web_append(out,capacity,&used,
       "{\"type\":\"workbench\",\"operation\":\"log\",\"data\":")
   || used + length + 2 > capacity) return 0;
   memcpy(out + used,page,length); used += length;
   out[used++] = '}'; out[used] = '\0';
   return used;
}

size_t the_web_format_document_page(char *out, size_t capacity,
                                    const char *panel, uint64_t revision,
                                    size_t line, size_t offset,
                                    size_t records, size_t bytes)
{
   char page[THE_WORKBENCH_JSON_MAX];
   size_t used = 0, length;
   if (out == NULL || !panel || capacity < 128
   || the_workbench_document_json(page,sizeof(page),panel,revision,line,offset,
                                  records,bytes) != 0) return 0;
   length = strlen(page);
   if (!web_append(out,capacity,&used,
       "{\"type\":\"workbench\",\"operation\":\"document\",\"data\":")
   || used + length + 2 > capacity) return 0;
   memcpy(out + used,page,length); used += length;
   out[used++] = '}'; out[used] = '\0';
   return used;
}

size_t the_web_format_form_page(char *out,size_t capacity,const char *form,
                                size_t offset,size_t limit,size_t bytes,int submitted)
{
   char page[THE_WORKBENCH_JSON_MAX]; size_t used=0,length;
   if (!out || !form || the_workbench_form_json(page,sizeof(page),form,offset,limit,bytes,submitted)) return 0;
   length=strlen(page);
   if (!web_append(out,capacity,&used,"{\"type\":\"workbench\",\"operation\":\"form\",\"data\":")
   || used+length+2>capacity) return 0;
   memcpy(out+used,page,length); used+=length; out[used++]='}'; out[used]=0;
   return used;
}

int the_web_workbench_form_begin(TheWebWorkbenchReceipts *receipts,
   const char *request_id,TheWorkbenchFormRequest *request,TheInputEvent *event)
{
   char argument[THE_WORKBENCH_OPERAND_MAX+1]; const char *operation,*sequence;
   int length,accepted;
   if (!request_id || !request || !event || !(sequence=strrchr(request_id,'.'))
   || !the_web_parse_u64(sequence+1,&request->request_id) || !request->request_id) return -1;
   if (!the_input_event_from_workbench_form(request,event)) return -5;
   operation=request->operation==THE_WORKBENCH_FORM_SET ? "form.set"
      : request->operation==THE_WORKBENCH_FORM_EDIT ? "form.edit"
      : request->operation==THE_WORKBENCH_FORM_SUBMIT ? "form.submit" : "form.cancel";
   length=snprintf(argument,sizeof(argument),"%lu:%" PRIu64 ":%lu:%zu:%d:%d:%" PRIu32 ":%s:%s:%s",
      request->form_generation,request->draft_revision,request->panel_generation,request->length,
      (int)request->edit,request->cell,request->codepoint,request->field,request->panel,request->value);
   if (length<0 || (size_t)length>=sizeof(argument)) return -5;
   accepted=the_web_workbench_receipt_begin(receipts,request_id,operation,request->form,
      request->form_generation,argument);
   if (accepted==1)
   {
      TheWebWorkbenchReceipt *entry=&receipts->entries[receipts->pending_index];
      entry->form_request_id=request->request_id;
      entry->draft_revision=request->draft_revision;
      entry->caret=-1;
   }
   return accepted;
}

int the_web_parse_u64(const char *value, uint64_t *out)
{
   uint64_t number = 0;
   const unsigned char *p = (const unsigned char *)value;
   if (value == NULL || out == NULL || !*p || (*p == '0' && p[1])) return 0;
   for (; *p; p++)
   {
      if (*p < '0' || *p > '9' || number > (UINT64_MAX - (*p - '0')) / 10)
         return 0;
      number = number * 10 + (*p - '0');
   }
   *out = number;
   return 1;
}

int the_web_panel_layout(const char *id, uint64_t generation,
                         const char *placement, int cols, int rows)
{
   const TheWorkbenchPanel *panel = id ? the_workbench_panel_find(id) : NULL;
   const TheWorkbenchPanelState *state = panel ? the_workbench_panel_state(id) : NULL;
   TheWorkbenchPlacement chosen = THE_WORKBENCH_PLACE_NONE;
   size_t index;
   int allowed = 0;
   if (panel == NULL || state == NULL || !state->visible
   || generation == 0 || generation > ULONG_MAX
   || panel->generation != (unsigned long)generation
   || placement == NULL || cols < 0 || rows < 0
   || cols > 10000 || rows > 10000) return 0;
   if (!strcmp(placement,"inactive-tab"))
      return !state->focused && the_workbench_panel_effective(id,
         (unsigned long)generation,THE_WORKBENCH_PLACE_TAB,"inactive-tab");
   if (!strcmp(placement,"right")) chosen = THE_WORKBENCH_PLACE_RIGHT;
   else if (!strcmp(placement,"below")) chosen = THE_WORKBENCH_PLACE_BELOW;
   else if (!strcmp(placement,"tab")) chosen = THE_WORKBENCH_PLACE_TAB;
   else if (strcmp(placement,"none")) return 0;
   if (chosen == THE_WORKBENCH_PLACE_NONE)
      return the_workbench_panel_effective(id,(unsigned long)generation,
                  chosen,"unavailable-renderer");
   if (chosen == panel->placement) allowed = 1;
   for (index = 0; index < panel->fallback_count; index++)
      if (chosen == panel->fallbacks[index]) allowed = 1;
   if (!allowed) return 0;
   if (cols < panel->mincols || rows < panel->minrows)
      return the_workbench_panel_effective(id,(unsigned long)generation,
                  THE_WORKBENCH_PLACE_NONE,"unavailable-renderer");
   return the_workbench_panel_effective(id,(unsigned long)generation,chosen,"ok");
}

int the_web_format_workbench_receipt(char *out, size_t capacity,
                                     const TheWebWorkbenchReceipt *receipt)
{
   size_t used;
   int length;
   if (out == NULL || receipt == NULL || capacity < 100) return 0;
   length = snprintf(out, capacity,
      "{\"type\":\"workbench\",\"operation\":\"result\",\"requestId\":");
   if (length < 0 || (size_t)length >= capacity) return 0;
   used = (size_t)length;
   length = (int)web_json_string(out + used, capacity - used, receipt->request_id);
   if (!length) return 0;
   used += (size_t)length;
   length = snprintf(out + used, capacity - used, ",\"state\":\"%s\",\"code\":",
                     receipt->pending ? "queued" : "complete");
   if (length < 0 || (size_t)length >= capacity - used) return 0;
   used += (size_t)length;
   length = (int)web_json_string(out + used, capacity - used, receipt->code);
   if (!length) return 0;
   used += (size_t)length;
   length = snprintf(out + used, capacity - used, ",\"rc\":%d,\"revision\":%lu,\"message\":",
                     receipt->rc, receipt->revision);
   if (length < 0 || (size_t)length >= capacity - used) return 0;
   used += (size_t)length;
   length = (int)web_json_string(out + used, capacity - used, receipt->message);
   if (!length) return 0;
   used += (size_t)length;
   if (receipt->form_request_id)
   {
      length=snprintf(out+used,capacity-used,",\"form\":\"%s\",\"formGeneration\":\"%lu\",\"draftRevision\":\"%" PRIu64 "\",\"resultRevision\":\"%" PRIu64 "\",\"caret\":%d",
         receipt->action,receipt->generation,receipt->draft_revision,receipt->result_revision,receipt->caret);
      if (length<0 || (size_t)length>=capacity-used) return 0;
      used+=(size_t)length;
   }
   if (used + 2 > capacity) return 0;
   out[used++] = '}'; out[used] = '\0';
   return (int)used;
}

static size_t web_driver_format_console_colours(char *out, size_t capacity)
{
   static const char *const names[] = {
      "body", "header", "input", "control", "selected", "divider", "error", "status"
   };
   static const uint32_t basic[] = {
      0x000000, 0x000080, 0x008000, 0x008080, 0x800000, 0x800080, 0x808000, 0xc0c0c0
   };
   size_t i, used = 0;

   for (i = 0; i < sizeof(names) / sizeof(names[0]); i++)
   {
      TheRenderAttr attr = set_colour(CURRENT_FILE->attr + ATTR_CONSOLEBODY + i);
      int added = snprintf(out + used, capacity - used,
         "%s\"%s\":{\"style\":%u", i ? "," : "", names[i],
         (unsigned)the_render_attr_style(attr));
      if (added < 0 || (size_t)added >= capacity - used) return 0;
      used += (size_t)added;
      if (the_render_attr_has_color(attr))
      {
         int fg = the_render_attr_fg(attr), bg = the_render_attr_bg(attr);
         uint32_t foreground = fg < THE_RENDER_COLOR_RGB_FIRST ? basic[fg]
            : the_render_color_rgb24(fg);
         uint32_t background = bg < THE_RENDER_COLOR_RGB_FIRST ? basic[bg]
            : the_render_color_rgb24(bg);
         added = snprintf(out + used, capacity - used,
            ",\"foreground\":\"#%06X\",\"background\":\"#%06X\"",
            (unsigned)foreground, (unsigned)background);
         if (added < 0 || (size_t)added >= capacity - used) return 0;
         used += (size_t)added;
      }
      if (capacity - used < 2) return 0;
      out[used++] = '}';
      out[used] = '\0';
   }
   return used;
}

size_t the_web_format_snapshot(char *out, size_t capacity)
{
   LlmDriverFormatOptions options;
   char console_colours[1400];
   size_t length;

   if (out == NULL || capacity == 0 || CURRENT_VIEW == NULL || CURRENT_FILE == NULL)
      return 0;

   web_driver_sync_focus();
   /* Buffer switches (including HELP) can build rows without preparing their
    * displayed prefixes. Keep each published frame complete. */
   if (CURRENT_VIEW != NULL && CURRENT_FILE != NULL
   &&  the_panel_active == NULL && !in_macro)
   {
      build_screen(current_screen);
      prepare_screen_for_display(current_screen);
   }
   llm_driver_format_options_init(&options);
   options.compact = 1;
   options.max_text_cols = LLM_DRIVER_MAX_COLS;
   options.include_all_utf = 0;
   length = llm_runtime_format_screen(
      current_screen, &options, out,
      capacity);
   while (length > 0
   && (out[length - 1] == '\n'
    || out[length - 1] == '\r'))
      out[--length] = '\0';
   if (length > 0 && out[length-1] == '}')
   {
      size_t offset = length-1;
      if (!web_driver_format_console_colours(console_colours, sizeof(console_colours)))
         return 0;
      int appended = snprintf(out+offset, capacity-offset,
         ",\"viewport\":{\"rows\":%d,\"cols\":%d,\"reserved_rows\":%d,\"prefix_cols\":%d,\"prefix_gap\":%d},\"console_colours\":{%s}}",
         terminal_lines, terminal_cols,
         terminal_lines-CURRENT_SCREEN.rows[WINDOW_FILEAREA],
         CURRENT_VIEW->prefix ? CURRENT_VIEW->prefix_width-CURRENT_VIEW->prefix_gap : 0,
         CURRENT_VIEW->prefix ? CURRENT_VIEW->prefix_gap : 0, console_colours);
      if (appended <= 0 || (size_t)appended >= capacity-offset) return 0;
      length = offset+(size_t)appended;
   }
   return length;
}

static int web_driver_decode_utf8(const unsigned char *text, size_t len,
                                  size_t *used, uint32_t *codepoint)
{
   uint32_t value;
   size_t count;
   size_t i;

   if (text == NULL || len == 0 || used == NULL || codepoint == NULL)
      return 0;
   if (text[0] < 0x80)
   {
      *used = 1;
      *codepoint = text[0];
      return 1;
   }
   if ((text[0] & 0xe0) == 0xc0)
   {
      count = 2;
      value = text[0] & 0x1f;
      if (value < 2)
         return 0;
   }
   else if ((text[0] & 0xf0) == 0xe0)
   {
      count = 3;
      value = text[0] & 0x0f;
   }
   else if ((text[0] & 0xf8) == 0xf0)
   {
      count = 4;
      value = text[0] & 0x07;
   }
   else
      return 0;
   if (count > len)
      return 0;
   for (i = 1; i < count; i++)
   {
      if ((text[i] & 0xc0) != 0x80)
         return 0;
      value = (value << 6) | (uint32_t)(text[i] & 0x3f);
   }
   if ((count == 3 && value < 0x800)
   ||  (count == 4 && value < 0x10000)
   ||  value > 0x10ffff
   ||  (value >= 0xd800 && value <= 0xdfff))
      return 0;
   *used = count;
   *codepoint = value;
   return 1;
}

static int web_driver_queue_text(TheInputQueue *queue, const char *text)
{
   uint32_t codepoints[THE_INPUT_QUEUE_MAX];
   const unsigned char *bytes = (const unsigned char *)text;
   size_t len = text != NULL ? strlen(text) : 0;
   size_t offset = 0;
   size_t count = 0;
   size_t i;

   while (offset < len)
   {
      size_t used = 0;

      if (count >= THE_INPUT_QUEUE_MAX
      ||  !web_driver_decode_utf8(bytes + offset, len - offset, &used,
                                  &codepoints[count]))
         return 0;
      offset += used;
      count++;
   }
   if (count > THE_INPUT_QUEUE_MAX - queue->count)
      return 0;
   for (i = 0; i < count; i++)
   {
      TheInputEvent event;

      if (!the_input_event_from_text(codepoints[i], &event)
      ||  !the_input_queue_push(queue, event))
         return 0;
   }
   return count > 0;
}

int the_web_queue_request(TheInputQueue *queue, const char *type,
                          const char *value, const char *argument,
                          long line, int row, int cell, int rows, int cols)
{
   TheInputEvent event;
   int valid = 0;
   if (!queue || !type) return 0;
   if (!value) value = "";
   if (strcmp(type, "text") == 0)
      return web_driver_queue_text(queue, value);
   if (strcmp(type, "key") == 0)
      valid = the_input_event_from_key_name(value, &event);
   else if (strcmp(type, "command") == 0)
      valid = the_input_event_from_restricted_command(value, &event);
   else if (strcmp(type, "action") == 0)
      valid = the_input_event_from_action(value, argument ? argument : "", &event);
   else if (strcmp(type, "resize") == 0)
      valid = rows >= 12 && rows <= 256 && cols >= 40 && cols <= 1000
           && the_input_event_from_resize(rows, cols, &event);
   else if (strcmp(type, "hit") == 0)
   {
      TheInputLogicalTargetKind kind;
      valid = the_input_logical_target_kind_from_name(value, &kind)
           && the_input_event_from_logical_target(kind, (LINETYPE)line, row, cell, 0, -1, &event);
   }
   return valid && the_input_queue_push(queue, event);
}
