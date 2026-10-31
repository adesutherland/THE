#include "workbenchform.h"
#include "workbenchruntime.h"
#include "inputevent.h"
#include <stdio.h>
#include <string.h>

static int failures,modal;
int the_workbench_restricted(void) { return 0; }
const char *the_workbench_action_reason(const TheWorkbenchAction *a) { return a ? "ok" : "missing-action"; }
static int busy(void) { return modal; }
static void check(int ok,const char *why) { if(!ok) { fprintf(stderr,"FAIL %s\n",why); failures++; } }
static TheWorkbenchField field(const char *id,const char *form,const char *value,int access)
{
   TheWorkbenchField f={0}; strcpy(f.id,id); strcpy(f.form,form); strcpy(f.label,"Label / < & >"); strcpy(f.value,value);
   f.maxcells=1024; f.access=(TheWorkbenchFieldAccess)access; return f;
}
static TheWorkbenchFormRequest request(const char *form,int op,const char *id,const char *value)
{
   TheWorkbenchFormRequest r={0}; const TheWorkbenchFormState *s=the_workbench_form_state(form);
   strcpy(r.form,form); if(id) strcpy(r.field,id); if(value) { strcpy(r.value,value); r.length=strlen(value); }
   r.operation=(TheWorkbenchFormOperation)op; r.form_generation=s->generation; r.draft_revision=s->draft_revision;
   r.request_id=UINT64_C(9007199254740997); return r;
}
int main(void)
{
   TheWorkbenchForm f={0}; TheWorkbenchField a,b; TheWorkbenchPanel p={0};
   TheWorkbenchFormRequest r,decoded; TheInputEvent event;
   const TheWorkbenchFormState *s; unsigned long gen,pane_gen; uint64_t rev,serial,definition;
   char json[THE_WORKBENCH_JSON_MAX]; size_t i;
   the_workbench_reset(); the_workbench_forms_modal_lookup(busy);
   strcpy(f.id,"public.person"); a=field("public.name",f.id,"Guest",THE_WORKBENCH_FIELD_EDIT); b=field("public.protected",f.id,"Fixed",THE_WORKBENCH_FIELD_READONLY);
   the_workbench_activation_begin("/failed-form"); the_workbench_begin("profile.forms");
   the_workbench_put_form(&f); the_workbench_put_field(&a); the_workbench_commit();
   check(!the_workbench_form_state(f.id),"profile candidate creates no live values");
   check(the_workbench_activation_end(0)==-3 && !the_workbench_form_count(),"failed profile publishes no form");
   the_workbench_begin("public.forms"); check(!the_workbench_put_form(&f) && !the_workbench_put_field(&a) && !the_workbench_put_field(&b),"stage form and fields");
   strcpy(p.id,"public.pane"); strcpy(p.form,f.id); p.kind=THE_WORKBENCH_PANEL_FIELDS; the_workbench_put_panel(&p);
   check(!the_workbench_commit() && the_workbench_form_count()==1 && the_workbench_field_count()==2,"publish form graph");
   s=the_workbench_form_state(f.id); gen=s->generation; rev=s->draft_revision;
   check(s && !s->dirty && !s->submitted && s->fields==2 && !strcmp(the_workbench_field_state(f.id,a.id)->value,"Guest"),"initial form values");
   check(the_workbench_form_json(json,sizeof(json),f.id,0,8,8192,1)==-3,"result absent before submit");
   r=request(f.id,THE_WORKBENCH_FORM_SET,a.id,"a\" / # input POISON");
   check(the_input_event_from_workbench_form(&r,&event) && the_input_event_workbench_form(&event,&decoded)
         && decoded.request_id==r.request_id && decoded.draft_revision==r.draft_revision && !strcmp(decoded.value,r.value),"ABI3 counted form request roundtrip");
   check(!the_workbench_form_request(&decoded) && the_workbench_form_state(f.id)->dirty,"SET stages literal data");
   check(the_workbench_form_request(&r)==-3 && !strcmp(the_workbench_last_result()->code,"stale-revision"),"duplicate old revision refuses");
   check(the_workbench_form_state(f.id)->generation==gen && the_workbench_form_state_revision()>0,"runtime edit does not replace definitions");
   r=request(f.id,THE_WORKBENCH_FORM_SET,b.id,"NO"); check(the_workbench_form_request(&r)==12,"protected field refuses SET");
   serial=the_workbench_form_receipt()->serial;
   the_workbench_result("unrelated",5,"later result"); check(the_workbench_form_receipt()->serial==serial && !strcmp(the_workbench_form_receipt()->code,"field-protected"),"receipt owns result code");
   r=request(f.id,THE_WORKBENCH_FORM_CANCEL,NULL,NULL); check(!the_workbench_form_request(&r) && !strcmp(the_workbench_field_state(f.id,a.id)->value,"Guest"),"cancel restores initial baseline");
   r=request(f.id,THE_WORKBENCH_FORM_SET,a.id,"Submitted"); the_workbench_form_request(&r);
   r=request(f.id,THE_WORKBENCH_FORM_SUBMIT,NULL,NULL); check(!the_workbench_form_request(&r) && the_workbench_form_state(f.id)->result_revision==1 && !the_workbench_form_state(f.id)->dirty,"submit publishes result only");
   check(!the_workbench_form_json(json,sizeof(json),f.id,0,8,8192,1) && strstr(json,"Submitted") && strstr(json,"form-result"),"bounded result inspection");
   r=request(f.id,THE_WORKBENCH_FORM_SET,a.id,"Discard"); the_workbench_form_request(&r);
   r=request(f.id,THE_WORKBENCH_FORM_CANCEL,NULL,NULL); the_workbench_form_request(&r);
   check(!strcmp(the_workbench_field_state(f.id,a.id)->value,"Submitted"),"cancel restores latest submit");
   r=request(f.id,THE_WORKBENCH_FORM_SET,a.id,"\xe4\xb8\xad" "e\xcc\x81"); check(!the_workbench_form_request(&r),"UTF8 staged value");
   r=request(f.id,THE_WORKBENCH_FORM_EDIT,a.id,NULL); r.edit=THE_WORKBENCH_FORM_BACKSPACE; r.cell=3;
   check(!the_workbench_form_request(&r) && !strcmp(the_workbench_field_state(f.id,a.id)->value,"\xe4\xb8\xad") && the_workbench_form_receipt()->caret==2,"backspace removes complete combining cluster");
   r=request(f.id,THE_WORKBENCH_FORM_EDIT,a.id,NULL); r.edit=THE_WORKBENCH_FORM_INSERT; r.cell=1; r.codepoint='x';
   check(the_workbench_form_request(&r)==5,"wide-cell interior caret refuses");
   modal=1; r=request(f.id,THE_WORKBENCH_FORM_CANCEL,NULL,NULL); check(the_workbench_form_request(&r)==-3 && the_workbench_form_state(f.id)->dirty,"modal refusal retains draft"); modal=0;
   definition=the_workbench_revision(); the_workbench_begin("public.forms"); strcpy(a.label,"Changed"); the_workbench_put_field(&a);
   check(the_workbench_commit()==-3 && the_workbench_revision()==definition && !strcmp(the_workbench_last_result()->code,"draft-in-use"),"definition replacement cannot lose draft"); the_workbench_abort();
   r=request(f.id,THE_WORKBENCH_FORM_CANCEL,NULL,NULL); the_workbench_form_request(&r);
   pane_gen=the_workbench_panel_find(p.id)->generation; the_workbench_runtime_panel(p.id,pane_gen,THE_WORKBENCH_PANEL_SHOW); the_workbench_runtime_panel(p.id,pane_gen,THE_WORKBENCH_PANEL_FOCUS);
   the_workbench_begin("public.forms"); the_workbench_put_field(&a); check(!the_workbench_commit(),"clean field replacement publishes");
   check(the_workbench_form_state(f.id)->generation!=gen && the_workbench_focus_request()->restoring && !the_workbench_panel_state(p.id)->focused,"form generation replacement returns pane focus");
   r=request(f.id,THE_WORKBENCH_FORM_SET,a.id,NULL); r.length=1024; memset(r.value,'"',r.length); r.value[r.length]=0;
   check(the_input_event_from_workbench_form(&r,&event) && !the_workbench_form_request(&r),"full1024 byte field is admitted without truncation");
   check(!the_workbench_form_json(json,sizeof(json),f.id,0,8,8192,0) && strlen(json)<sizeof(json),"worst escaped field JSON fits");
   r.length=1025; check(!the_input_event_from_workbench_form(&r,&event),"event refuses byte overflow");
   r=request(f.id,THE_WORKBENCH_FORM_CANCEL,NULL,NULL); the_workbench_form_request(&r);
   the_workbench_begin("public.forms");
   memset(a.value,'"',1024); a.value[1024]=0; memset(a.label,'"',255); a.label[255]=0; the_workbench_put_field(&a);
   b=field("public.protected",f.id,a.value,THE_WORKBENCH_FIELD_READONLY); strcpy(b.label,a.label); the_workbench_put_field(&b);
   for(i=2;i<10;i++) { char id[40]; snprintf(id,sizeof(id),"public.field%lu",(unsigned long)i); b=field(id,f.id,a.value,THE_WORKBENCH_FIELD_EDIT); strcpy(b.label,a.label); the_workbench_put_field(&b); }
   check(!the_workbench_commit() && !the_workbench_form_json(json,sizeof(json),f.id,0,8,8192,0) && strstr(json,"\"nextOffset\":8"),"counted fields page continues");
   check(strlen(json)<sizeof(json) && strlen(json)>16000,"eight worst-escaped values and labels fit bridge");
   check(the_workbench_form_json(json,sizeof(json),f.id,0,8,1,0)==94 && !*json,"undersized value budget never truncates");
   check(the_workbench_form_json(json,10,f.id,0,8,8192,0)==94 && !*json,"undersized output publishes no JSON");
   the_workbench_begin("other.forms"); strcpy(f.id,"other.person"); the_workbench_put_form(&f); b=field("public.name",f.id,"No",THE_WORKBENCH_FIELD_EDIT);
   check(!the_workbench_put_field(&b) && !the_workbench_commit(),"field IDs are scoped to their form across owners");
   the_workbench_begin("other.forms"); b=field("public.name","public.person","No",THE_WORKBENCH_FIELD_EDIT);
   check(the_workbench_put_field(&b)==-3,"another owner cannot replace a form field"); the_workbench_abort();
   the_workbench_reset(); if(failures) return 1;
   puts("workbench form tests passed"); return 0;
}
