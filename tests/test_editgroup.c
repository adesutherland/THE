#include "the.h"
#include "proto.h"
#include "vars.h"
#include "panelui.h"
#include "editorgroup.h"
#include "workbenchdocument.h"

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

VIEW_DETAILS *vd_current;
LENGTHTYPE max_line_length=1000,rec_len;
CHARTYPE *rec;
bool the_readonly;
int READONLYx;
ThePanelState *the_panel_active;
void *(*the_malloc)(size_t);
void (*the_free)(void *)=free;
static long fail_at=-1,allocations;
static int failures,shutdowns,autosaves,autosave_error,activation;
static unsigned char checkpoint[10000];
static size_t checkpoint_length;
static uint64_t document_id;
static FILE_DETAILS file;
static VIEW_DETAILS view;
static LINE records[130];
static unsigned char working[2001];

void *the_edit_group_test_malloc(size_t n)
{ if(fail_at>=0 && allocations++==fail_at) return NULL; return malloc(n); }
void *the_edit_group_test_calloc(size_t n,size_t size)
{ if(fail_at>=0 && allocations++==fail_at) return NULL; return calloc(n,size); }
static void check(int yes,const char *message)
{ if(!yes) { fprintf(stderr,"FAIL %s\n",message); failures++; } }
LINE *lll_find(LINE *first,LINE *last,LINETYPE n,LINETYPE total)
{ (void)last; if(n<0 || n>total+1) return NULL; while(n-- && first) first=first->next; return first; }
LENGTHTYPE xedit_truncated_length(FILE_DETAILS *f,const CHARTYPE *bytes,LENGTHTYPE n)
{ (void)bytes; return f->truncation_column && n>f->truncation_column ? f->truncation_column : n; }
int the_workbench_activation_active(void) { return activation; }
uint64_t the_editor_document_active(void) { return document_id; }
#ifdef USE_SDSLH
void sdslh_shutdown_file(FILE_DETAILS *f) { shutdowns++; f->cb=NULL; f->sdslh_comm=NULL; f->sdslh_parser=NULL; }
#endif
short save_file(FILE_DETAILS *f,CHARTYPE *name,bool force,LINETYPE count,LINETYPE start,LINETYPE *num,
   bool append,LENGTHTYPE a,LENGTHTYPE b,bool scope,bool based,bool autosave)
{
   const void *owner; size_t number,length,base_length,i; const unsigned char *base,*pending;
   int known=the_workbench_document_record(&owner,&number,&base,&base_length,&pending,&length);
   (void)name;(void)force;(void)count;(void)start;(void)num;(void)append;(void)a;(void)b;(void)scope;(void)based;(void)autosave;
   autosaves++; the_edit_group_changed(); checkpoint_length=0;
   if(autosave_error) return (short)autosave_error;
   for(i=1;i<=(size_t)f->number_lines;i++) {
      const LINE *line=lll_find(f->first_line,f->last_line,(LINETYPE)i,f->number_lines);
      size_t n=known && owner==f && number==i ? length : (size_t)line->length;
      memcpy(checkpoint+checkpoint_length,known && owner==f && number==i ? pending : line->line,n);
      checkpoint_length+=n; checkpoint[checkpoint_length++]='\n';
   }
   return 0;
}
static int info(const void *opaque,size_t *lines,int *dirty)
{ const FILE_DETAILS *f=opaque; *lines=(size_t)f->number_lines; *dirty=f->save_alt!=0; return 1; }
static int line(const void *opaque,size_t n,const unsigned char **bytes,size_t *length)
{ const FILE_DETAILS *f=opaque; const LINE *r=lll_find(f->first_line,f->last_line,(LINETYPE)n,f->number_lines); if(!r) return 0; *bytes=r->line; *length=(size_t)r->length; return 1; }
static int pending(const unsigned char **bytes,size_t *length)
{ *bytes=rec; *length=(size_t)rec_len; return rec_len>=0; }
static void bind(void)
{ the_workbench_document_record_bind(&file,1,records[1].line,(size_t)records[1].length); }
static void setup(void)
{
   static const TheWorkbenchDocumentProvider provider={info,line,pending,NULL,NULL,NULL};
   size_t i;
   the_workbench_document_reset();
   for(i=0;i<130;i++) free(records[i].line);
   memset(&file,0,sizeof(file)); memset(&view,0,sizeof(view)); memset(records,0,sizeof(records));
   for(i=0;i<4;i++) {
      const char *text=i==1 ? "alpha" : i==2 ? "beta" : "";
      records[i].line=malloc(strlen(text)+1); strcpy((char *)records[i].line,text); records[i].length=(LENGTHTYPE)strlen(text);
      if(i) records[i].prev=records+i-1; if(i<3) records[i].next=records+i+1;
   }
   file.first_line=records; file.last_line=records+3; file.number_lines=2; file.readonly=READONLY_OFF;
   view.file_for_view=&file; view.focus_line=1; view.current_window=WINDOW_COMMAND; view.x[WINDOW_COMMAND]=7;
   vd_current=&view; rec=working; memcpy(rec,"PENDING",7); rec_len=7; max_line_length=1000;
   the_malloc=the_edit_group_test_malloc; the_free=free; the_panel_active=NULL;
   the_workbench_document_provider(&provider); document_id=the_workbench_document_identify(&file); bind();
   the_editor_edit_group_initialize(); fail_at=-1; autosaves=shutdowns=autosave_error=activation=0;
}
static TheDocumentWriteToken token(void)
{ TheDocumentWriteToken t={0}; check(!the_edit_group_token(document_id,&t),"capture certified token"); return t; }
static TheDocumentLineEdit change(size_t n,const char *before,const char *after)
{ TheDocumentLineEdit e={n,(const unsigned char *)before,(const unsigned char *)after,strlen(before),strlen(after)}; return e; }
int main(void)
{
   TheDocumentLineEdit edits[129]; TheEditGroupPlan *plan=NULL; TheEditGroupReceipt receipt;
   TheEditGroupInfo previous,now; TheDocumentWriteToken t; VIEW_DETAILS original; LINE first;
   long i,needed; uint64_t revision; int count;
   static CHARTYPE label[]="label";
   setup(); original=view; first=records[1]; records[1].select=7; records[1].flags.tag_flag=1;
#ifdef USE_SDSLH
   { static PARSER_DETAILS parser; parser.is_sdslh_parser=1; file.parser=&parser; file.colouring=TRUE; file.cb=(CodeBuffer *)(uintptr_t)1; }
#endif
   records[1].name=label; records[1].pre=(THE_PPC *)(uintptr_t)123;
   edits[0]=change(1,"PENDING","one"); edits[1]=change(2,"beta","two"); t=token();
   fail_at=LONG_MAX; allocations=0;
   check(!the_edit_group_prepare(t,edits,2,&plan),"prepare complete group"); needed=allocations; fail_at=-1; the_edit_group_abort(plan);
   for(i=0;i<needed;i++) {
      fail_at=i; allocations=0; plan=NULL;
      check(the_edit_group_prepare(t,edits,2,&plan)==94 && !plan,"each allocation fails atomically");
      check(records[1].line==first.line && records[2].length==4 && !file.save_alt && rec_len==7 && !memcmp(rec,"PENDING",7) && !shutdowns,"failure preserves LINE/rec/dirty/parser");
   }
   fail_at=-1; check(!the_edit_group_prepare(t,edits,2,&plan),"prepare after allocation failures");
   max_line_length=3;
   check(the_edit_group_commit(plan,&receipt)==-3 && receipt.rc==-3,"WIDTH change refuses before publish");
   max_line_length=1000; file.readonly=READONLY_ON;
   check(the_edit_group_commit(plan,&receipt)==12 && !strcmp(receipt.code,"read-only"),"readonly commit refusal receipt");
   file.readonly=READONLY_OFF;
   check(!the_edit_group_commit(plan,&receipt) && receipt.changed_lines==2 && file.save_alt==1,"atomic group dirty once");
   check(!memcmp(rec,"one",3) && records[1].select==7 && records[1].flags.tag_flag && records[1].name==label && records[1].pre==(THE_PPC *)(uintptr_t)123,"prefix/tag/select/name metadata preserved");
   check(!memcmp(&original,&view,sizeof(view)),"commit never moves caret or focus");
   the_edit_group_info(&previous); t=token();
   edits[0]=change(1,"one","next"); edits[1]=change(2,"two","tail"); count=shutdowns;
   for(i=0;i<needed;i++) {
      fail_at=i; allocations=0; plan=NULL;
      check(the_edit_group_prepare(t,edits,2,&plan)==94 && !plan,"allocation refusal after previous group");
      the_edit_group_info(&now);
      check(now.group==previous.group && now.undo_available && file.save_alt==1 && shutdowns==count,"allocation refusal preserves undo/dirty/parser");
   }
   fail_at=-1;
   check(!the_edit_group_prepare(t,edits,2,&plan),"prepare candidate while previous undo remains live");
   file.readonly=READONLY_ON;
   check(the_edit_group_commit(plan,&receipt)==12,"late readonly refusal preserves prior group");
   the_edit_group_info(&now); check(now.group==previous.group && now.undo_available && shutdowns==count,"failed commit preserves journal/parser");
   file.readonly=READONLY_OFF; the_edit_group_abort(plan);
   edits[0]=change(1,"one","next"); edits[1]=change(2,"WRONG","bad");
   check(the_edit_group_prepare(t,edits,2,&plan)==-3,"bad last before-image refuses whole group");
   the_edit_group_info(&now); check(now.group==previous.group && now.undo_available,"failed prepare preserves prior undo");
   check(!the_edit_group_undo(previous.group,t,&receipt),"one conditional undo restores group");
   check(records[1].line==first.line && records[1].length==5 && !memcmp(rec,"PENDING",7) && rec_len==7 && !file.save_alt && !memcmp(&original,&view,sizeof(view)),"undo restores physical LINE plus exact dirty pending rec/caret");
   setup(); file.autosave=1; file.autosave_fname=(CHARTYPE *)"checkpoint";
   edits[0]=change(1,"PENDING","one"); edits[1]=change(2,"beta","two");
   check(!the_edit_group_prepare(token(),edits,2,&plan) && !the_edit_group_commit(plan,&receipt),"autosave threshold commit");
   check(autosaves==1 && checkpoint_length==8 && !memcmp(checkpoint,"one\ntwo\n",8),"checkpoint sees entire committed group");
   check(token().write_revision==receipt.after_revision,"own autosave cannot invalidate receipt");
   check(!the_edit_group_undo(receipt.group,token(),&receipt) && autosaves==2 && checkpoint_length==13 && !memcmp(checkpoint,"PENDING\nbeta\n",13),"undo rewrites checkpoint with pending before-image");
   setup(); file.autosave=1; file.autosave_fname=(CHARTYPE *)"checkpoint"; autosave_error=100;
   check(!the_edit_group_prepare(token(),edits,2,&plan) && !the_edit_group_commit(plan,&receipt)
      && receipt.autosave_rc==100 && receipt.rc==0 && file.save_alt==1,"post-publication checkpoint failure does not reverse successful edit");
   check(!the_edit_group_undo(receipt.group,token(),&receipt) && receipt.autosave_rc==100
      && !memcmp(rec,"PENDING",7) && !file.save_alt,"undo remains atomic when optional checkpoint repair fails");
   setup(); file.save_alt=file.autosave_alt=USHRT_MAX;
   edits[0]=change(1,"PENDING","one");
   check(!the_edit_group_prepare(token(),edits,1,&plan) && !the_edit_group_commit(plan,&receipt) && file.save_alt==USHRT_MAX && file.autosave_alt==USHRT_MAX,"dirty counters saturate");
   the_edit_group_info(&previous); revision=token().write_revision;
   edits[0]=change(1,"one","one");
#ifdef USE_SDSLH
   { static PARSER_DETAILS parser; parser.is_sdslh_parser=1; file.parser=&parser; file.colouring=TRUE; file.cb=NULL; }
#endif
   count=shutdowns;
   check(!the_edit_group_prepare(token(),edits,1,&plan) && !the_edit_group_commit(plan,&receipt) && !receipt.changed_lines && token().write_revision==revision && shutdowns==count,"no-op preserves bytes/token/parser");
#ifdef USE_SDSLH
   check(!receipt.parser_available,"no-op reports actual inactive parser");
#endif
   the_edit_group_info(&now); check(now.group==previous.group && now.undo_available,"no-op preserves latest undo");
   t=token(); the_edit_group_mutation_enter(); the_edit_group_mutation_enter(); the_edit_group_mutation_leave();
   check(the_edit_group_prepare(t,edits,1,&plan)==-3,"nested mutating admission stales token"); the_edit_group_mutation_leave();
   setup(); rec_len=max_line_length+1;
   edits[0]=change(2,"beta","new");
   check(the_edit_group_prepare(token(),edits,1,&plan)==-3,"oversized pending record refuses before pointer arithmetic");
   rec_len=5; memcpy(rec,"alpha",5); the_workbench_document_record_unknown();
   check(the_edit_group_token(document_id,&t)==-3 && !strcmp(the_edit_group_error(),"record-unavailable"),"unknown clean/dirty record cannot impersonate certified ownership");
   memcpy(rec,"PENDING",7); rec_len=7; bind(); t=token(); edits[0]=change(1,"PENDING","x"); edits[1]=edits[0];
   check(the_edit_group_prepare(t,edits,2,&plan)==5,"duplicate logical line refused");
   edits[0].after=(const unsigned char *)"\xc0\x80"; edits[0].after_length=2;
   check(the_edit_group_prepare(t,edits,1,&plan)==5,"malformed UTF8 refused");
   edits[0].after=(const unsigned char *)"\n"; edits[0].after_length=1;
   check(the_edit_group_prepare(t,edits,1,&plan)==5,"newline introduction refused");
   edits[0].after=(const unsigned char *)(uintptr_t)1; edits[0].after_length=THE_EDIT_GROUP_BYTES_MAX+1;
   check(the_edit_group_prepare(t,edits,1,&plan)==94,"oversized span refuses before reading pointer");
   check(the_edit_group_prepare(t,edits,129,&plan)==5,"129 logical lines refuse");
   the_panel_active=(ThePanelState *)(uintptr_t)1;
   check(the_edit_group_token(document_id,&t)==-3 && !strcmp(the_edit_group_error(),"busy-modal"),"modal owner refuses write admission");
   the_panel_active=NULL; activation=1;
   check(the_edit_group_token(document_id,&t)==-3 && !strcmp(the_edit_group_error(),"activation-pending"),"activation cannot write editor bytes"); activation=0;
   edits[0]=change(1,"PENDING","PENDING"); edits[1]=change(2,"beta","new"); first=records[1];
   check(!the_edit_group_prepare(token(),edits,2,&plan) && !the_edit_group_commit(plan,&receipt)
      && receipt.changed_lines==1 && records[1].line==first.line && records[1].length==5
      && !memcmp(rec,"PENDING",7),"mixed no-op line retains pending physical record and metadata");
   check(!the_edit_group_undo(receipt.group,token(),&receipt),"mixed no-op group undo");
   setup();
   for(i=4;i<130;i++) { records[i].line=calloc(1,1); records[i].length=0; }
   for(i=0;i<130;i++) { records[i].prev=i ? records+i-1 : NULL; records[i].next=i<129 ? records+i+1 : NULL; }
   file.last_line=records+129; file.number_lines=128; the_edit_group_changed();
   for(i=0;i<128;i++) edits[i]=change((size_t)i+1,i==0 ? "PENDING" : i==1 ? "beta" : "","x");
   check(!the_edit_group_prepare(token(),edits,128,&plan) && !the_edit_group_commit(plan,&receipt)
      && receipt.changed_lines==128 && file.save_alt==1,"128 distinct lines publish as one group");
   check(!the_edit_group_undo(receipt.group,token(),&receipt) && records[128].length==0 && !file.save_alt,"128-line undo restores whole group");
   setup();
   {
      size_t n=THE_EDIT_GROUP_BYTES_MAX/4; unsigned char *after=malloc(n+1);
      free(records[2].line); records[2].line=malloc(n+1); memset(records[2].line,'a',n); records[2].line[n]=0; records[2].length=(LENGTHTYPE)n;
      memset(after,'b',n+1); max_line_length=(LENGTHTYPE)n+1; the_edit_group_changed();
      edits[0].line=2; edits[0].before=records[2].line; edits[0].before_length=n; edits[0].after=after; edits[0].after_length=n;
      check(!the_edit_group_prepare(token(),edits,1,&plan) && !the_edit_group_commit(plan,&receipt),"exact 1MiB retained before/after bytes admitted");
      check(!the_edit_group_undo(receipt.group,token(),&receipt),"byte-boundary group undo");
      edits[0].before=records[2].line; edits[0].after_length=n+1;
      check(the_edit_group_prepare(token(),edits,1,&plan)==94,"one extra after byte exceeds retained budget without mutation");
      free(after);
   }
   the_workbench_document_forget(&file);
   check(the_edit_group_token(document_id,&t)==-3 && !strcmp(the_edit_group_error(),"document-closed"),"close invalidates identity");
   the_workbench_document_reset(); for(i=0;i<130;i++) free(records[i].line);
   if(failures) return 1; puts("edit groups: atomic allocation/commit/undo, pending rec, metadata, checkpoint and bounds passed"); return 0;
}
