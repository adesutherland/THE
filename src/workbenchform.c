#include "workbenchform.h"
#include "workbenchruntime.h"
#include "workbenchdocument.h"
#include "textpos.h"

#include <inttypes.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct { TheWorkbenchFieldState state; char baseline[THE_WORKBENCH_FIELD_VALUE_MAX+1]; size_t baseline_length; } FormField;
struct TheWorkbenchFormPublication {
   TheWorkbenchFormState forms[THE_WORKBENCH_FORM_MAX];
   FormField fields[THE_WORKBENCH_FIELD_MAX];
   size_t form_count,field_count;
};
static TheWorkbenchFormPublication live;
static TheWorkbenchFormReceipt receipt;
static uint64_t state_revision;
static int (*modal_lookup)(void);

static int form_index(const TheWorkbenchFormPublication *p,const char *id)
{ size_t i; for(i=0;id && i<p->form_count;i++) if(!strcmp(p->forms[i].id,id)) return (int)i; return -1; }
static int field_index(const TheWorkbenchFormPublication *p,const char *form,const char *id)
{ size_t i; for(i=0;form && id && i<p->field_count;i++) if(!strcmp(p->fields[i].state.form,form) && !strcmp(p->fields[i].state.id,id)) return (int)i; return -1; }
const TheWorkbenchFormState *the_workbench_form_state(const char *id)
{ int i=form_index(&live,id); return i<0 ? NULL : live.forms+i; }
const TheWorkbenchFieldState *the_workbench_field_state(const char *form,const char *id)
{ int i=field_index(&live,form,id); return i<0 ? NULL : &live.fields[i].state; }
const TheWorkbenchFormReceipt *the_workbench_form_receipt(void) { return &receipt; }
uint64_t the_workbench_form_state_revision(void) { return state_revision; }
void the_workbench_forms_modal_lookup(int (*lookup)(void)) { modal_lookup=lookup; }
void the_workbench_forms_reset(void)
{ memset(&live,0,sizeof(live)); memset(&receipt,0,sizeof(receipt)); state_revision=0; }
int the_workbench_field_value_valid(const char *value,size_t length,int maxcells)
{
   size_t i=0;
   if (!value || length>THE_WORKBENCH_FIELD_VALUE_MAX || maxcells<1 || maxcells>1024 || memchr(value,0,length)) return 0;
   while(i<length)
   {
      unsigned char c=(unsigned char)value[i]; size_t n=the_workbench_document_unit((const unsigned char *)value+i,length-i);
      if (c<32 || c==127 || (c>=128 && n==1) || (c==0xc2 && i+1<length && (unsigned char)value[i+1]<0xa0)) return 0;
      i+=n;
   }
   return textpos_from_byte((const CHARTYPE *)value,length,length).cell_column<=maxcells;
}
short the_workbench_forms_prepare(const TheWorkbenchForm *forms,size_t count,const TheWorkbenchField *fields,size_t nfields,TheWorkbenchFormPublication **out)
{
   TheWorkbenchFormPublication *next; size_t i,j; int changed=count!=live.form_count;
   if (!out || count>THE_WORKBENCH_FORM_MAX || nfields>THE_WORKBENCH_FIELD_MAX) return the_workbench_result("limit-exceeded",94,"Form definition limit exceeded");
   *out=NULL;
   for(i=0;i<live.form_count;i++)
   {
      for(j=0;j<count;j++) if(!strcmp(live.forms[i].id,forms[j].id)) break;
      if (j==count || live.forms[i].generation!=forms[j].generation)
      {
         changed=1;
         if (live.forms[i].dirty) return the_workbench_result("draft-in-use",-3,"Submit or cancel staged form values before replacing their definitions");
      }
   }
   if(!changed) return 0;
   if(state_revision==UINT64_MAX || !(next=calloc(1,sizeof(*next)))) return the_workbench_result("limit-exceeded",94,"Unable to prepare form runtime");
   next->form_count=count; next->field_count=nfields;
   for(i=0;i<count;i++)
   {
      int previous=form_index(&live,forms[i].id);
      TheWorkbenchFormState *s=next->forms+i;
      if(previous>=0 && live.forms[previous].generation==forms[i].generation) *s=live.forms[previous];
      else { strcpy(s->id,forms[i].id); s->generation=forms[i].generation; s->draft_revision=1; }
      s->fields=0;
   }
   for(i=0;i<nfields;i++)
   {
      int fi=form_index(next,fields[i].form),old=field_index(&live,fields[i].form,fields[i].id);
      int oldform=form_index(&live,fields[i].form);
      FormField *f=next->fields+i;
      if(fi<0) { free(next); return the_workbench_result("missing-object",5,"Field has no form"); }
      next->forms[fi].fields++;
      if(old>=0 && oldform>=0 && live.forms[oldform].generation==next->forms[fi].generation
      && live.fields[old].state.generation==fields[i].generation) *f=live.fields[old];
      else
      {
         strcpy(f->state.id,fields[i].id); strcpy(f->state.form,fields[i].form); f->state.generation=fields[i].generation;
         strcpy(f->state.value,fields[i].value); strcpy(f->baseline,fields[i].value);
         f->state.length=f->baseline_length=strlen(fields[i].value);
      }
   }
   *out=next; return 0;
}
void the_workbench_forms_commit(TheWorkbenchFormPublication *p)
{ if(p) { live=*p; free(p); state_revision++; } }
void the_workbench_forms_abort(TheWorkbenchFormPublication *p) { free(p); }
static short refused(const char *code,short rc,const char *message)
{ return the_workbench_result(code,rc,message); }
static void dirty(TheWorkbenchFormState *form)
{
   size_t i; form->dirty=0;
   for(i=0;i<live.field_count;i++) if(!strcmp(live.fields[i].state.form,form->id))
   {
      FormField *f=live.fields+i;
      f->state.dirty=f->state.length!=f->baseline_length || memcmp(f->state.value,f->baseline,f->state.length);
      if(f->state.dirty) form->dirty=1;
   }
}
static short edit_value(FormField *f,const TheWorkbenchField *definition,const TheWorkbenchFormRequest *r,char *result,int *caret)
{
   TextPos pos,left,right; size_t a,b,n=0; CHARTYPE encoded[4];
   int width;
   if(r->cell<0 || r->cell>definition->maxcells) return refused("invalid-operand",5,"Invalid form caret cell");
   if(r->edit==THE_WORKBENCH_FORM_CLEAR) { result[0]=0; *caret=0; return 0; }
   width=textpos_from_byte((const CHARTYPE *)f->state.value,f->state.length,f->state.length).cell_column;
   if(r->cell>width) return refused("invalid-operand",5,"Caret is beyond the field value");
   pos=textpos_from_cell((const CHARTYPE *)f->state.value,f->state.length,r->cell,TEXT_SNAP_BACKWARD);
   if(pos.cell_column!=r->cell) return refused("invalid-operand",5,"Caret splits a text cluster");
   left=right=pos;
   if(r->edit==THE_WORKBENCH_FORM_INSERT)
   {
      n=text_utf8_encode(r->codepoint,encoded);
      if(!n || !the_workbench_field_value_valid((const char *)encoded,n,1024)) return refused("invalid-operand",5,"Invalid field codepoint");
   }
   else if(r->edit==THE_WORKBENCH_FORM_BACKSPACE) left=textpos_prev_cluster((const CHARTYPE *)f->state.value,f->state.length,pos);
   else if(r->edit==THE_WORKBENCH_FORM_DELETE) right=textpos_next_cluster((const CHARTYPE *)f->state.value,f->state.length,pos);
   else return refused("invalid-operand",5,"Invalid field edit operation");
   a=left.byte_offset; b=right.byte_offset;
   if(a+n+f->state.length-b>THE_WORKBENCH_FIELD_VALUE_MAX) return refused("limit-exceeded",94,"Field value byte limit exceeded");
   memcpy(result,f->state.value,a); if(n) memcpy(result+a,encoded,n);
   memcpy(result+a+n,f->state.value+b,f->state.length-b); result[a+n+f->state.length-b]=0;
   if(!the_workbench_field_value_valid(result,strlen(result),definition->maxcells)) return refused("limit-exceeded",94,"Field cell limit exceeded");
   *caret=textpos_from_byte((const CHARTYPE *)result,strlen(result),a+n).cell_column;
   return 0;
}
static short request(const TheWorkbenchFormRequest *r,int *caret)
{
   int index; size_t i; TheWorkbenchFormState *s;
   if(!r || !memchr(r->form,0,sizeof(r->form)) || !memchr(r->field,0,sizeof(r->field)) || !memchr(r->panel,0,sizeof(r->panel))) return refused("invalid-operand",5,"Invalid form request");
   if(the_workbench_activation_active()) return refused("activation-pending",-3,"Form values cannot change during profile activation");
   if(modal_lookup && modal_lookup()) return refused("busy-modal",-3,"Modal input retains form ownership");
   index=form_index(&live,r->form);
   if(index<0) return refused("missing-object",5,"No published form has that ID");
   s=live.forms+index;
   if(!r->form_generation || r->form_generation!=s->generation) return refused("stale-generation",-3,"Form definition changed");
   if(!r->draft_revision || r->draft_revision!=s->draft_revision) return refused("stale-revision",-3,"Form draft changed");
   if(r->panel[0])
   {
      const TheWorkbenchPanel *p=the_workbench_panel_find(r->panel);
      const TheWorkbenchPanelState *state=the_workbench_panel_state(r->panel);
      if(!p || p->kind!=THE_WORKBENCH_PANEL_FIELDS || p->generation!=r->panel_generation || strcmp(p->form,r->form) || p->form_generation!=s->generation) return refused("stale-generation",-3,"Form pane binding changed");
      if(!state || !state->visible) return refused("capability-unavailable",-3,"Show the form pane before editing it");
   }
   if(s->draft_revision==UINT64_MAX || state_revision==UINT64_MAX) return refused("limit-exceeded",94,"Form revision limit exceeded");
   if(r->operation==THE_WORKBENCH_FORM_SET || r->operation==THE_WORKBENCH_FORM_EDIT)
   {
      FormField *f; const TheWorkbenchField *definition; char value[THE_WORKBENCH_FIELD_VALUE_MAX+1]; short rc;
      index=field_index(&live,r->form,r->field); definition=the_workbench_field_find(r->form,r->field);
      if(index<0 || !definition || strcmp(definition->form,r->form)) return refused("missing-object",5,"No form field has that ID");
      if(definition->access!=THE_WORKBENCH_FIELD_EDIT) return refused("field-protected",12,"The form field is read-only");
      f=live.fields+index;
      if(r->operation==THE_WORKBENCH_FORM_SET)
      {
         if(r->length>THE_WORKBENCH_FIELD_VALUE_MAX) return refused("limit-exceeded",94,"Field value byte limit exceeded");
         if(!the_workbench_field_value_valid(r->value,r->length,definition->maxcells)) return refused("invalid-operand",5,"Invalid field value or cell width");
         memcpy(value,r->value,r->length); value[r->length]=0;
      }
      else if((rc=edit_value(f,definition,r,value,caret))) return rc;
      if(strcmp(f->state.value,value))
      { strcpy(f->state.value,value); f->state.length=strlen(value); s->draft_revision++; state_revision++; dirty(s); }
   }
   else if(r->operation==THE_WORKBENCH_FORM_SUBMIT || r->operation==THE_WORKBENCH_FORM_CANCEL)
   {
      if(r->operation==THE_WORKBENCH_FORM_SUBMIT && s->result_revision==UINT64_MAX) return refused("limit-exceeded",94,"Form result revision limit exceeded");
      for(i=0;i<live.field_count;i++) if(!strcmp(live.fields[i].state.form,s->id))
      {
         FormField *f=live.fields+i;
         if(r->operation==THE_WORKBENCH_FORM_SUBMIT) { strcpy(f->baseline,f->state.value); f->baseline_length=f->state.length; }
         else { strcpy(f->state.value,f->baseline); f->state.length=f->baseline_length; }
         f->state.dirty=0;
      }
      if(r->operation==THE_WORKBENCH_FORM_SUBMIT) { s->submitted=1; s->result_revision++; }
      s->dirty=0; s->draft_revision++; state_revision++;
   }
   else return refused("invalid-operand",5,"Invalid form operation");
   return the_workbench_result("ok",0,"");
}
short the_workbench_form_request(const TheWorkbenchFormRequest *r)
{
   int caret=-1; short rc; const TheWorkbenchFormState *s;
   if(receipt.serial==UINT64_MAX) return refused("limit-exceeded",94,"Form receipt limit exceeded");
   rc=request(r,&caret); s=r && memchr(r->form,0,sizeof(r->form)) ? the_workbench_form_state(r->form) : NULL;
   receipt.serial++; receipt.request_id=r ? r->request_id : 0; receipt.operation=r ? r->operation : 0;
   receipt.form[0]=0; if(r && memchr(r->form,0,sizeof(r->form))) snprintf(receipt.form,sizeof(receipt.form),"%s",r->form);
   receipt.form_generation=s ? s->generation : 0; receipt.draft_revision=s ? s->draft_revision : 0; receipt.result_revision=s ? s->result_revision : 0;
   receipt.rc=rc; snprintf(receipt.code,sizeof(receipt.code),"%s",the_workbench_last_result()->code); receipt.caret=rc ? -1 : caret; return rc;
}

typedef struct { char *out; size_t cap,used; int good; } Writer;
static void emit(Writer *w,const char *format,...)
{ va_list ap; int n; if(!w->good) return; va_start(ap,format); n=vsnprintf(w->out+w->used,w->cap-w->used,format,ap); va_end(ap); if(n<0 || (size_t)n>=w->cap-w->used) w->good=0; else w->used+=(size_t)n; }
static void quote(Writer *w,const char *s)
{ const unsigned char *p=(const unsigned char *)s; emit(w,"\""); for(;*p;p++) { if(*p=='"' || *p=='\\') emit(w,"\\%c",*p); else if(*p<32 || *p==127) emit(w,"\\u%04x",*p); else emit(w,"%c",*p); } emit(w,"\""); }
short the_workbench_form_json(char *out,size_t capacity,const char *id,size_t offset,size_t limit,size_t bytes,int submitted)
{
   const TheWorkbenchFormState *s=the_workbench_form_state(id); Writer w; size_t i,position=0,count=0,used=0,next=offset;
   if(!out || !capacity) return refused("invalid-operand",5,"Missing form JSON buffer"); out[0]=0;
   if(!s) return refused("missing-object",5,"No published form has that ID");
   if(!limit || limit>8 || !bytes || bytes>8192) return refused("invalid-operand",5,"Invalid form inspection budget");
   if(submitted && !s->submitted) return refused("result-unavailable",-3,"The form has not been submitted");
   w.out=out; w.cap=capacity; w.used=0; w.good=1;
   emit(&w,"{\"schemaVersion\":1,\"revision\":%lu,\"section\":\"%s\",\"form\":",the_workbench_revision(),submitted ? "form-result" : "fields"); quote(&w,id);
   emit(&w,",\"generation\":\"%lu\",\"draftRevision\":\"%" PRIu64 "\",\"resultRevision\":\"%" PRIu64 "\",\"submitted\":%s,\"dirty\":%s,\"total\":%lu,\"offset\":%lu,\"records\":[",s->generation,s->draft_revision,s->result_revision,s->submitted ? "true" : "false",s->dirty ? "true" : "false",(unsigned long)s->fields,(unsigned long)offset);
   for(i=0;i<live.field_count;i++) if(!strcmp(live.fields[i].state.form,id))
   {
      FormField *f=live.fields+i; const TheWorkbenchField *d=the_workbench_field_find(id,f->state.id);
      const char *value=submitted ? f->baseline : f->state.value; size_t length=submitted ? f->baseline_length : f->state.length;
      if(position++<offset) continue;
      if(count==limit || length>bytes-used) break;
      emit(&w,"%s{\"id\":",count ? "," : ""); quote(&w,d->id);
      emit(&w,",\"generation\":\"%lu\",\"label\":",d->generation); quote(&w,d->label);
      emit(&w,",\"access\":\"%s\",\"maxcells\":%d,\"value\":",d->access==THE_WORKBENCH_FIELD_EDIT ? "edit" : "readonly",d->maxcells); quote(&w,value);
      emit(&w,",\"length\":%lu,\"dirty\":%s}",(unsigned long)length,!submitted && f->state.dirty ? "true" : "false");
      used+=length; count++; next=offset+count;
   }
   if(!count && offset<s->fields) { out[0]=0; return refused("limit-exceeded",94,"Form value exceeds the page byte budget"); }
   emit(&w,"],\"nextOffset\":"); if(next<s->fields) emit(&w,"%lu",(unsigned long)next); else emit(&w,"null"); emit(&w,"}");
   if(!w.good) { out[0]=0; return refused("limit-exceeded",94,"Form JSON capacity exceeded"); }
   return 0;
}
