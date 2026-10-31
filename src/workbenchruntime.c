#include "workbenchruntime.h"
#include "workbenchdocument.h"

#include <inttypes.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static TheWorkbenchModelState models[THE_WORKBENCH_LOG_MAX];
static size_t model_count;
static TheWorkbenchPanelState states[THE_WORKBENCH_PANEL_MAX];
static size_t state_count;
static unsigned long state_revision;
static TheWorkbenchFocusRequest focus_request;

struct TheWorkbenchRouteLease
{
   uint64_t job;
   struct { char model[THE_WORKBENCH_ID_MAX+1]; uint64_t generation; } streams[2];
};
static TheWorkbenchRouteLease *active_lease;
/* A reset invalidates a binding but its observer still owns the allocation
 * until end(). Do not reuse that lifetime or free underneath the host. */
static TheWorkbenchRouteLease *retired_lease;

static int model_index(const char *id)
{ size_t i; for (i=0;i<model_count;i++) if (!strcmp(models[i].id,id)) return (int)i; return -1; }
static int state_index(const char *id)
{ size_t i; for (i=0;i<state_count;i++) if (!strcmp(states[i].id,id)) return (int)i; return -1; }
static short fail(const char *code, short rc, const char *message)
{ return the_workbench_result(code,rc,message); }

size_t the_workbench_model_count(void) { return model_count; }
const TheWorkbenchModelState *the_workbench_model_at(size_t i)
{ return i < model_count ? models+i : NULL; }
const TheWorkbenchModelState *the_workbench_model_state(const char *id)
{ int i=id ? model_index(id) : -1; return i < 0 ? NULL : models+i; }
const TheWorkbenchPanelState *the_workbench_panel_state(const char *id)
{ int i=id ? state_index(id) : -1; return i < 0 ? NULL : states+i; }
unsigned long the_workbench_panel_state_revision(void) { return state_revision; }
const TheWorkbenchFocusRequest *the_workbench_focus_request(void) { return &focus_request; }

static void return_focus(const char *id, unsigned long generation)
{
   focus_request.serial++;
   snprintf(focus_request.previous_panel,sizeof(focus_request.previous_panel),"%s",id);
   focus_request.previous_generation=generation;
   focus_request.panel[0]=0; focus_request.generation=0; focus_request.restoring=1;
}

short the_workbench_runtime_publish(TheWorkbenchPanel *panels, size_t count)
{
   TheWorkbenchPanelState next[THE_WORKBENCH_PANEL_MAX];
   size_t saved_models=model_count, i, j;
   short rc=0;
   if (count > THE_WORKBENCH_PANEL_MAX) return fail("limit-exceeded",94,"Panel count limit exceeded");
   memset(next,0,sizeof(next));
   for (i=0;i<count;i++)
   {
      int index;
      if (panels[i].kind==THE_WORKBENCH_PANEL_DOCUMENT)
      {
         TheWorkbenchDocumentInfo info;
         const TheWorkbenchPanel *previous=the_workbench_panel_find(panels[i].id);
         if (the_workbench_document_info(panels[i].document_id,&info)==THE_WORKBENCH_DOCUMENT_MISSING
         && (!previous || previous->kind!=THE_WORKBENCH_PANEL_DOCUMENT || previous->document_id!=panels[i].document_id))
         { rc=fail("document-closed",-3,"Candidate document closed before publication"); break; }
         panels[i].model_generation=0;
         goto panel_state;
      }
      index=model_index(panels[i].model);
      if (index < 0)
      {
         const TheWorkbenchLogInfo *log;
         if (model_count == THE_WORKBENCH_LOG_MAX)
         { rc=fail("limit-exceeded",94,"Retained log model limit exceeded"); break; }
         if (the_workbench_log_find(panels[i].model))
         { rc=fail("policy-denied",-3,"Log has no matching workbench owner binding"); break; }
         if (the_workbench_log_create(panels[i].model) != THE_WORKBENCH_LOG_OK)
         { rc=fail("limit-exceeded",94,"Unable to create the candidate log model"); break; }
         log=the_workbench_log_find(panels[i].model);
         index=(int)model_count++;
         memset(models+index,0,sizeof(models[index]));
         strcpy(models[index].id,panels[i].model); strcpy(models[index].owner,panels[i].owner);
         models[index].generation=log->generation;
      }
      if (strcmp(models[index].owner,panels[i].owner))
      { rc=fail("policy-denied",-3,"Panel cannot attach another owner's log"); break; }
      if (!the_workbench_log_find(models[index].id)
      || the_workbench_log_find(models[index].id)->generation != models[index].generation)
      { rc=fail("missing-object",5,"Retained log generation is unavailable"); break; }
      panels[i].model_generation=models[index].generation;
panel_state:
      strcpy(next[i].id,panels[i].id); next[i].generation=panels[i].generation;
      strcpy(next[i].reason,"hidden");
      for (j=0;j<state_count;j++) if (!strcmp(states[j].id,panels[i].id))
      {
         const TheWorkbenchPanel *previous=the_workbench_panel_find(panels[i].id);
         if (previous && previous->kind==panels[i].kind
         && previous->document_id==panels[i].document_id && !strcmp(previous->model,panels[i].model))
         { next[i]=states[j]; next[i].generation=panels[i].generation; }
         break;
      }
   }
   if (rc)
   {
      while (model_count > saved_models)
      { TheWorkbenchModelState *m=models+--model_count; the_workbench_log_remove(m->id,m->generation); memset(m,0,sizeof(*m)); }
      return rc;
   }
   for (i=0;i<state_count;i++) if (states[i].focused)
   {
      for (j=0;j<count;j++) if (!strcmp(states[i].id,next[j].id) && next[j].focused) break;
      if (j == count || states[i].generation != next[j].generation)
      {
         /* Renderers retain the selected generation. Return to their saved
          * origin before publishing a replacement, even with the same model. */
         if (j < count) next[j].focused=0;
         return_focus(states[i].id,states[i].generation);
      }
   }
   if (count != state_count || memcmp(states,next,count*sizeof(next[0]))) state_revision++;
   memcpy(states,next,sizeof(states)); state_count=count;
   return 0;
}

short the_workbench_runtime_panel(const char *id, unsigned long generation,
                                  TheWorkbenchPanelOperation operation)
{
   int index=id ? state_index(id) : -1;
   TheWorkbenchPanelState *state;
   size_t i;
   if (index < 0) return fail("missing-object",5,"No published panel has that ID");
   state=states+index;
   if (generation && generation != state->generation) return fail("stale-generation",-3,"Panel generation changed");
   if (operation == THE_WORKBENCH_PANEL_SHOW)
   {
      if (!state->visible)
      { state->visible=1; strcpy(state->reason,"unavailable-renderer"); state_revision++; }
   }
   else if (operation == THE_WORKBENCH_PANEL_HIDE)
   {
      if (state->focused) { return_focus(state->id,state->generation); state->focused=0; }
      if (state->visible)
      { state->visible=0; state->effective=THE_WORKBENCH_PLACE_NONE; strcpy(state->reason,"hidden"); state_revision++; }
   }
   else if (operation == THE_WORKBENCH_PANEL_FOCUS)
   {
      if (!state->visible) return fail("capability-unavailable",-3,"Show the panel before focusing it");
      focus_request.previous_panel[0]=0; focus_request.previous_generation=0;
      for (i=0;i<state_count;i++) if (states[i].focused)
      {
         strcpy(focus_request.previous_panel,states[i].id);
         focus_request.previous_generation=states[i].generation; states[i].focused=0;
      }
      state->focused=1; focus_request.serial++; strcpy(focus_request.panel,state->id);
      focus_request.generation=state->generation; focus_request.restoring=0; state_revision++;
   }
   else if (operation == THE_WORKBENCH_PANEL_RETURN)
   {
      if (!state->focused) return fail("object-in-use",-3,"Panel does not own the workbench focus");
      return_focus(state->id,state->generation); state->focused=0; state_revision++;
   }
   else return fail("invalid-operand",5,"Invalid panel operation");
   return fail("ok",0,"");
}

int the_workbench_panel_effective(const char *id, unsigned long generation,
   TheWorkbenchPlacement placement, const char *reason)
{
   int i=id ? state_index(id) : -1;
   const char *resolved=reason ? reason : "ok";
   if (i < 0 || states[i].generation != generation || !states[i].visible
   || placement < THE_WORKBENCH_PLACE_NONE || placement > THE_WORKBENCH_PLACE_TAB
   || strlen(resolved) >= sizeof(states[i].reason)) return 0;
   if (states[i].effective != placement || strcmp(states[i].reason,resolved))
   { states[i].effective=placement; strcpy(states[i].reason,resolved); state_revision++; }
   return 1;
}

static TheWorkbenchRouteLease *bind_fail(char *error,size_t capacity,const char *code,short rc,const char *message)
{
   if (error && capacity) snprintf(error,capacity,"%s: %s",code,message);
   fail(code,rc,message); return NULL;
}
TheWorkbenchRouteLease *the_workbench_routes_bind(const char *tool, uint64_t job,
   char *error, size_t capacity)
{
   TheWorkbenchToolInfo info;
   TheWorkbenchRouteLease *lease;
   size_t i;
   if (active_lease || retired_lease) return bind_fail(error,capacity,"busy-job",-3,"A routed program job is already active");
   if (!job || !tool || !the_workbench_tool_info(tool,&info))
      return bind_fail(error,capacity,"missing-object",5,"Unknown route source tool");
   if (!info.available) return bind_fail(error,capacity,"capability-unavailable",-3,"Route source tool is unavailable");
   if (!info.role || (strcmp(info.role,"program") && strcmp(info.role,"runner") && strcmp(info.role,"compiler")))
      return bind_fail(error,capacity,"unsupported-feature",-3,"Tool role cannot supply program log routes");
   lease=calloc(1,sizeof(*lease));
   if (!lease) return bind_fail(error,capacity,"limit-exceeded",94,"Unable to allocate route bindings");
   lease->job=job;
   for (i=0;i<the_workbench_route_count();i++)
   {
      const TheWorkbenchRoute *route=the_workbench_route_at(i);
      const TheWorkbenchModelState *model;
      if (strcmp(route->tool,tool)) continue;
      model=the_workbench_model_state(route->model);
      if (!model || strcmp(model->owner,route->owner)
      || !the_workbench_log_find(model->id)
      || the_workbench_log_find(model->id)->generation != model->generation)
      { free(lease); return bind_fail(error,capacity,"missing-object",5,"Route log model is unavailable"); }
      strcpy(lease->streams[route->stream-1].model,model->id);
      lease->streams[route->stream-1].generation=model->generation;
   }
   for (i=0;i<2;i++) if (lease->streams[i].model[0])
      models[model_index(lease->streams[i].model)].leases++;
   active_lease=lease;
   return lease;
}

TheWorkbenchLogResult the_workbench_route_output(TheWorkbenchRouteLease *lease,
   int fd, uint64_t sequence, const void *bytes, size_t length)
{
   TheWorkbenchLogResult result;
   int index;
   if (lease && lease == retired_lease) return THE_WORKBENCH_LOG_STALE;
   if (!lease || lease != active_lease || (fd != 1 && fd != 2) || !sequence)
      return THE_WORKBENCH_LOG_INVALID;
   if (!lease->streams[fd-1].model[0]) return THE_WORKBENCH_LOG_OK;
   result=the_workbench_log_append(lease->streams[fd-1].model,lease->streams[fd-1].generation,
                                  lease->job,fd,sequence,bytes,length);
   if (result != THE_WORKBENCH_LOG_OK && (index=model_index(lease->streams[fd-1].model)) >= 0)
   {
      models[index].last_error=result; models[index].error_job=lease->job;
      models[index].error_stream=fd; models[index].error_sequence=sequence; state_revision++;
   }
   return result;
}
void the_workbench_routes_release(TheWorkbenchRouteLease *lease)
{
   size_t i;
   if (!lease) return;
   if (lease == active_lease)
   {
      for (i=0;i<2;i++) if (lease->streams[i].model[0])
      { int index=model_index(lease->streams[i].model); if (index >= 0 && models[index].leases) models[index].leases--; }
      active_lease=NULL;
   }
   else if (lease == retired_lease) retired_lease=NULL;
   else return; /* End after reset or a repeated release is harmless. */
   free(lease);
}
void the_workbench_runtime_reset(void)
{
   if (active_lease) { retired_lease=active_lease; active_lease=NULL; }
   memset(models,0,sizeof(models)); model_count=0;
   memset(states,0,sizeof(states)); state_count=0;
   memset(&focus_request,0,sizeof(focus_request)); state_revision=0;
   the_workbench_log_reset();
}

/* Explicit-length JSON keeps embedded NUL and terminal controls as data. */
typedef struct { char *out; size_t capacity, used; int good; } LogJson;
static void js(LogJson *j,const char *format,...)
{
   va_list ap; int n;
   if (!j->good) return;
   va_start(ap,format); n=vsnprintf(j->out+j->used,j->capacity-j->used,format,ap); va_end(ap);
   if (n < 0 || (size_t)n >= j->capacity-j->used) { j->good=0; return; }
   j->used+=(size_t)n;
}
static void quote(LogJson *j,const char *bytes,size_t length)
{
   size_t i; js(j,"\"");
   for (i=0;i<length;i++)
   {
      unsigned char c=(unsigned char)bytes[i];
      if (c == '"' || c == '\\') js(j,"\\%c",c);
      else if (c < 32 || c == 127) js(j,"\\u%04x",c);
      else js(j,"%c",c);
   }
   js(j,"\"");
}
short the_workbench_log_json(char *out,size_t capacity,const char *id,uint64_t generation,
   uint64_t cursor,size_t offset,size_t records,size_t bytes)
{
   TheWorkbenchLogPage page;
   TheWorkbenchLogResult result;
   LogJson j; size_t i;
   if (!out || !capacity || !bytes || bytes > THE_WORKBENCH_LOG_JSON_BYTES_MAX)
      return fail("invalid-operand",5,"Invalid log inspection byte budget");
   out[0]=0;
   result=the_workbench_log_page(id,generation,cursor,offset,records,bytes,&page);
   if (result != THE_WORKBENCH_LOG_OK)
      return fail(result == THE_WORKBENCH_LOG_STALE ? "stale-generation" : result == THE_WORKBENCH_LOG_MISSING ? "missing-object" : result == THE_WORKBENCH_LOG_LIMIT || result == THE_WORKBENCH_LOG_NOMEM ? "limit-exceeded" : "invalid-operand",
                  result == THE_WORKBENCH_LOG_STALE ? -3 : result == THE_WORKBENCH_LOG_LIMIT || result == THE_WORKBENCH_LOG_NOMEM ? 94 : 5,"Log inspection was refused");
   j.out=out; j.capacity=capacity; j.used=0; j.good=1;
   js(&j,"{\"schemaVersion\":1,\"revision\":%lu,\"section\":\"log\",\"id\":",the_workbench_revision());
   quote(&j,id,strlen(id));
   js(&j,",\"generation\":\"%" PRIu64 "\",\"contentRevision\":\"%" PRIu64 "\",\"firstCursor\":\"%" PRIu64 "\",\"nextCursor\":\"%" PRIu64 "\",\"nextOffset\":%lu,\"more\":%s,\"omitted\":%s,\"omittedBytes\":\"%" PRIu64 "\",\"omittedRecords\":\"%" PRIu64 "\",\"records\":[",
      page.log.generation,page.log.revision,page.log.first_cursor,page.next_cursor,(unsigned long)page.next_offset,
      page.more ? "true" : "false",page.omitted ? "true" : "false",page.log.omitted_bytes,page.log.omitted_records);
   for (i=0;i<page.count;i++)
   {
      TheWorkbenchLogSlice *s=page.slices+i;
      js(&j,"%s{\"cursor\":\"%" PRIu64 "\",\"job\":\"%" PRIu64 "\",\"stream\":%d,\"sequence\":\"%" PRIu64 "\",\"offset\":%lu,\"length\":%lu,\"originalLength\":%lu,\"text\":",
         i ? "," : "",s->cursor,s->job,s->stream,s->sequence,(unsigned long)s->offset,(unsigned long)s->length,(unsigned long)s->original_length);
      quote(&j,s->text,s->length); js(&j,"}");
   }
   js(&j,"]}");
   if (!j.good) { out[0]=0; return fail("limit-exceeded",94,"Log JSON capacity exceeded"); }
   return 0; /* Inspection preserves the previous operation receipt. */
}

static void document_quote(LogJson *j,const unsigned char *bytes,size_t length)
{
   size_t i=0,n,k; js(j,"\"");
   while (i<length)
   {
      unsigned char c=bytes[i]; n=the_workbench_document_unit(bytes+i,length-i);
      if (c>=0x80 && n==1) js(j,"\\ufffd");
      else if (c=='"' || c=='\\') js(j,"\\%c",c);
      else if (c<32 || c==127) js(j,"\\u%04x",c);
      else for (k=0;k<n;k++) js(j,"%c",bytes[i+k]);
      i+=n;
   }
   js(j,"\"");
}
short the_workbench_document_json(char *out,size_t capacity,const char *id,uint64_t expected,
   size_t line,size_t offset,size_t lines,size_t bytes)
{
   const TheWorkbenchPanel *panel=the_workbench_panel_find(id);
   TheWorkbenchDocumentPage page;
   TheWorkbenchDocumentResult result;
   LogJson j; size_t i,k;
   if (!out || !capacity) return fail("invalid-operand",5,"Missing document JSON buffer");
   out[0]=0;
   if (!panel || panel->kind!=THE_WORKBENCH_PANEL_DOCUMENT)
      return fail("missing-object",5,"No published DOCUMENT panel has that ID");
   if (!bytes || bytes>THE_WORKBENCH_DOCUMENT_BYTES_MAX) return fail("invalid-operand",5,"Invalid document byte budget");
   /* Reserve room for worst-case JSON escaping plus exact hexadecimal bytes. */
   result=the_workbench_document_read(panel->document_id,expected,line,offset,lines,bytes>7168 ? 7168 : bytes,&page);
   if (result!=THE_WORKBENCH_DOCUMENT_OK)
   {
      const char *reason=result==THE_WORKBENCH_DOCUMENT_BUSY && page.info.reason ? page.info.reason : the_workbench_document_result_name(result);
      return fail(reason,result==THE_WORKBENCH_DOCUMENT_INVALID ? 5 : result==THE_WORKBENCH_DOCUMENT_LIMIT || result==THE_WORKBENCH_DOCUMENT_NOMEM ? 94 : -3,"Document inspection was refused");
   }
   j.out=out; j.capacity=capacity; j.used=0; j.good=1;
   js(&j,"{\"schemaVersion\":1,\"revision\":%lu,\"section\":\"document\",\"panel\":",the_workbench_revision()); quote(&j,id,strlen(id));
   js(&j,",\"panelGeneration\":\"%lu\",\"documentId\":\"%" PRIu64 "\",\"documentRevision\":\"%" PRIu64 "\",\"lineCount\":%lu,\"dirty\":%s,\"nextLine\":%lu,\"nextOffset\":%lu,\"more\":%s,\"records\":[",
      panel->generation,page.info.id,page.info.revision,(unsigned long)page.info.lines,page.info.dirty ? "true" : "false",(unsigned long)page.next_line,(unsigned long)page.next_offset,page.more ? "true" : "false");
   for (i=0;i<page.count;i++)
   {
      const TheWorkbenchDocumentSlice *s=page.slices+i; const unsigned char *data=page.bytes+s->data_offset;
      js(&j,"%s{\"line\":%lu,\"offset\":%lu,\"length\":%lu,\"originalLength\":%lu,\"text\":",i ? "," : "",(unsigned long)s->line,(unsigned long)s->offset,(unsigned long)s->length,(unsigned long)s->original_length);
      document_quote(&j,data,s->length); js(&j,",\"bytesHex\":\"");
      for (k=0;k<s->length;k++) js(&j,"%02x",data[k]); js(&j,"\"}");
   }
   js(&j,"]}");
   if (!j.good) { out[0]=0; return fail("limit-exceeded",94,"Document JSON capacity exceeded"); }
   return 0;
}
