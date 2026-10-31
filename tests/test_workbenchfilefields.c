#include "workbenchfilefields.h"
#include "inputevent.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static unsigned char source[2][2048];
static TheWorkbenchFileFieldsConfig config;
static int modal,readonly;
static const char *problem="ok";
int the_workbench_restricted(void) { return 0; }
const char *the_workbench_action_reason(const TheWorkbenchAction *a) { return a ? "ok" : "missing-action"; }
static short available(uint64_t id) { if(modal) { problem="busy-modal"; return -3; } return id==1 ? 0 : -3; }
static short configuration(uint64_t id,TheWorkbenchFileFieldsConfig *out) { if(id!=1) return -3; *out=config; out->readonly=readonly; return 0; }
static short information(uint64_t id,size_t *records) { if(id!=1) return -3; *records=2; return 0; }
static short line(uint64_t id,size_t n,const unsigned char **bytes,size_t *length) { if(id!=1 || n<1 || n>2) return -3; *bytes=source[n-1]; *length=strlen((char *)*bytes); return 0; }
static const char *error(void) { return problem; }
typedef struct { char before[2][2048],after[2][2048]; } Plan;
static short prepare(uint64_t id,const TheDocumentLineEdit *edits,size_t n,void **out,size_t *retained) {
   size_t i; Plan *p=calloc(1,sizeof(*p)); (void)id;
   if(!p) return 94;
   for(i=0;i<2;i++) strcpy(p->before[i],(char *)source[i]),strcpy(p->after[i],(char *)source[i]);
   for(i=0;i<n;i++) { const TheDocumentLineEdit *e=edits+i; if(e->line<1 || e->line>2 || strlen((char *)source[e->line-1])!=e->before_length || memcmp(source[e->line-1],e->before,e->before_length)) { free(p); problem="stale-revision"; return -3; } memcpy(p->after[e->line-1],e->after,e->after_length); p->after[e->line-1][e->after_length]=0; }
   *out=p; *retained=sizeof(*p); return 0;
}
static short validate(void *p,int undo) { (void)p;(void)undo; return readonly ? 12 : 0; }
static void publish(void *opaque,int undo) { Plan *p=opaque; size_t i; for(i=0;i<2;i++) strcpy((char *)source[i],undo ? p->before[i] : p->after[i]); }
static TheWorkbenchFileFieldsRequest request(int op,const char *field,const char *value) {
   TheWorkbenchFileFieldsRequest r={0}; const TheWorkbenchFileFieldsState *s=the_workbench_filefields_state("public.fields");
   r.operation=(TheWorkbenchFileFieldsOperation)op; strcpy(r.panel,s->panel); r.panel_generation=s->generation; r.draft_revision=s->draft_revision; r.request_id=UINT64_C(9007199254740999);
   if(field) strcpy(r.field,field); if(value) strcpy(r.value,value),r.length=strlen(value); return r;
}
int main(void) {
   TheWorkbenchPanel panel={0}; TheWorkbenchFileFieldsProvider owner={configuration,information,line,error};
   TheEditGroupProvider group={available,prepare,validate,publish,NULL,free,NULL,error};
   TheWorkbenchFileFieldsRequest r,decoded; TheInputEvent input; TheDocumentWriteToken token; TheEditGroupInfo info;
   char json[65536]; uint64_t draft; unsigned long generation;
   strcpy((char *)source[0],"!pLabel !uabc     !p END"); strcpy((char *)source[1],"!pOther !u12 !p Z");
   config.enabled=1; config.escape='!'; config.tags['p']=config.tags['u']=1; config.editable['u']=1; config.width=2048;
   the_workbench_reset(); the_workbench_filefields_provider(&owner); the_edit_group_provider(&group);
   strcpy(panel.id,"public.fields"); panel.kind=THE_WORKBENCH_PANEL_FIELDS; panel.document_id=1;
   assert(!the_workbench_begin("public.fields") && !the_workbench_put_panel(&panel) && !the_workbench_commit());
   assert(!the_workbench_filefields_json(json,sizeof(json),panel.id,0,8,8192) && strstr(json,"Label ") && strstr(json,"abc     "));
   assert(the_workbench_filefields_json(json,sizeof(json),panel.id,0,8,1)==94 && !*json);
   r=request(THE_WORKBENCH_FILEFIELDS_SET,"s1","NO"); assert(the_workbench_filefields_request(&r)==12);
   r=request(THE_WORKBENCH_FILEFIELDS_SET,"s2","中é"); assert(the_input_event_from_workbench_filefields(&r,&input) && the_input_event_workbench_filefields(&input,&decoded));
   assert(decoded.request_id==r.request_id && !the_workbench_filefields_request(&decoded));
   assert(!strcmp(the_workbench_filefields_span(panel.id,1)->value,"中é     "));
   r=request(THE_WORKBENCH_FILEFIELDS_EDIT,"s2",NULL); r.edit=THE_WORKBENCH_FORM_INSERT; r.cell=-1; r.utf16_offset=1; r.codepoint='X';
   assert(!the_workbench_filefields_request(&r)); assert(!strcmp(the_workbench_filefields_span(panel.id,1)->value,"中X     "));
   assert(the_workbench_filefields_receipt()->caret==3 && the_workbench_filefields_receipt()->utf16_caret==2);
   r=request(THE_WORKBENCH_FILEFIELDS_SET,"s2","é       "); assert(!the_workbench_filefields_request(&r));
   r=request(THE_WORKBENCH_FILEFIELDS_EDIT,"s2",NULL); r.edit=THE_WORKBENCH_FORM_INSERT; r.cell=-1; r.utf16_offset=1; r.codepoint='X';
   assert(the_workbench_filefields_request(&r)==5); /* Inside the combining cluster. */
   r=request(THE_WORKBENCH_FILEFIELDS_EDIT,"s2",NULL); r.edit=THE_WORKBENCH_FORM_INSERT; r.cell=0; r.codepoint=0xd800;
   assert(the_workbench_filefields_request(&r)==5);
   r.codepoint=0x110000; assert(the_workbench_filefields_request(&r)==5);
   r=request(THE_WORKBENCH_FILEFIELDS_SET,"s2","!pBREAK"); assert(the_workbench_filefields_request(&r)==5 && !strcmp(the_workbench_filefields_receipt()->code,"marker-injection"));
   r=request(THE_WORKBENCH_FILEFIELDS_SET,"s2","Z"); assert(!the_workbench_filefields_request(&r));
   draft=the_workbench_filefields_state(panel.id)->draft_revision;
   r=request(THE_WORKBENCH_FILEFIELDS_APPLY,NULL,NULL); assert(!the_workbench_filefields_request(&r));
   assert(!strcmp((char *)source[0],"!pLabel !uZ       !p END") && !the_workbench_filefields_state(panel.id)->dirty);
   assert(!the_edit_group_token(1,&token)); the_edit_group_info(&info); assert(info.undo_available && !the_edit_group_undo(info.group,token,NULL));
   assert(!strcmp((char *)source[0],"!pLabel !uabc     !p END"));
   r=request(THE_WORKBENCH_FILEFIELDS_APPLY,NULL,NULL); assert(the_workbench_filefields_request(&r)==-3);
   assert(the_workbench_filefields_state(panel.id)->draft_revision==draft+1);
   r=request(THE_WORKBENCH_FILEFIELDS_RELOAD,NULL,NULL); assert(!the_workbench_filefields_request(&r));
   r=request(THE_WORKBENCH_FILEFIELDS_SET,"s2","Draft"); assert(!the_workbench_filefields_request(&r));
   assert(the_workbench_filefields_close(1)==-3);
   generation=the_workbench_panel_find(panel.id)->generation;
   assert(!the_workbench_begin("public.fields") && !the_workbench_place_panel(panel.id,THE_WORKBENCH_PLACE_BELOW,NULL,0,40) && !the_workbench_commit());
   assert(the_workbench_filefields_state(panel.id)->generation!=generation && the_workbench_filefields_state(panel.id)->dirty);
   r=request(THE_WORKBENCH_FILEFIELDS_SET,"s2","Other"); r.panel_generation=generation; assert(the_workbench_filefields_request(&r)==-3);
   modal=1; r=request(THE_WORKBENCH_FILEFIELDS_CANCEL,NULL,NULL); assert(the_workbench_filefields_request(&r)==-3); modal=0;
   r=request(THE_WORKBENCH_FILEFIELDS_CANCEL,NULL,NULL); assert(!the_workbench_filefields_request(&r) && !the_workbench_filefields_close(1));
   assert(!the_workbench_filefields_state(panel.id)->active && !strcmp(the_workbench_filefields_span(panel.id,1)->value,"abc     "));
   { TheWorkbenchFileFieldsPublication *publication=NULL;
     assert(the_workbench_filefields_prepare(&panel,33,0,&publication)==94 && !publication);
     assert(the_workbench_filefields_prepare(the_workbench_panel_find(panel.id),1,512,&publication)==94 && !publication);
   }
   config.tags[0x80]=1; r=request(THE_WORKBENCH_FILEFIELDS_RELOAD,NULL,NULL); assert(the_workbench_filefields_request(&r)==-3 && !strcmp(the_workbench_filefields_receipt()->code,"unsupported-configuration"));
   assert(!the_workbench_filefields_state(panel.id)->active);
   the_workbench_reset(); the_edit_group_reset(); puts("workbench file-field core tests passed"); return 0;
}
