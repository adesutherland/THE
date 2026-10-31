#include "workbenchdocument.h"
#include "workbenchruntime.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct { unsigned char line[3][10000]; size_t length[3],lines; int dirty; } File;
static unsigned char pending[10000];
static size_t pending_length;
static int failures,blocked,change_during_read,certified_empty;
int the_workbench_restricted(void) { return 0; }
const char *the_workbench_action_reason(const TheWorkbenchAction *a) { return a ? "ok" : "missing-action"; }
static void check(int yes,const char *what) { if (!yes) { fprintf(stderr,"FAIL %s\n",what); failures++; } }
static int info(const void *opaque,size_t *lines,int *dirty)
{ const File *f=opaque; *lines=f->lines; *dirty=f->dirty; return 1; }
static int line(const void *opaque,size_t number,const unsigned char **bytes,size_t *length)
{
   const File *f=opaque;
   if (!number || number>f->lines) return 0;
   *bytes=f->line[number-1]; *length=f->length[number-1];
   if (change_during_read) { change_during_read=0; the_workbench_document_changed(); }
   return 1;
}
static int record(const unsigned char **bytes,size_t *length) { *bytes=pending; *length=pending_length; return 1; }
static int busy(void) { return blocked; }
static int no_pending(const void *file) { (void)file; return certified_empty; }
static void bind(File *f,size_t n)
{ memcpy(pending,f->line[n-1],f->length[n-1]); pending_length=f->length[n-1]; the_workbench_document_record_bind(f,n,f->line[n-1],f->length[n-1]); }
int main(void)
{
   TheWorkbenchDocumentProvider provider={info,line,record,busy,no_pending};
   TheWorkbenchDocumentInfo metadata,after;
   TheWorkbenchDocumentPage page;
   TheWorkbenchPanel pane={0};
   File f={0},saved,other={0};
   uint64_t id,reopened,revision;
   char json[THE_WORKBENCH_JSON_MAX];
   unsigned char data[]={0xe2,0x82,0xac,0,27,'<',0xff};
   f.lines=3; memcpy(f.line[0],"alpha",5); f.length[0]=5; memcpy(f.line[1],"base",4); f.length[1]=4;
   saved=f;
   the_workbench_document_provider(&provider); bind(&f,2);
   id=the_workbench_document_identify(&f);
   check(id && the_workbench_document_identify(&f)==id,"live identity is stable");
   memcpy(pending,data,sizeof(data)); pending_length=sizeof(data);
   check(the_workbench_document_pending_owned(&f,2) && !the_workbench_document_pending_owned(&f,1),"pending ownership is explicit file and line");
   check(the_workbench_document_info(id,&metadata)==THE_WORKBENCH_DOCUMENT_OK && metadata.dirty && metadata.lines==3,"metadata includes pending dirty record");
   revision=metadata.revision;
   check(the_workbench_document_read(id,revision,1,0,8,8192,&page)==THE_WORKBENCH_DOCUMENT_OK && page.count==3,"copy complete logical lines including empty");
   check(page.slices[1].length==sizeof(data) && !memcmp(page.bytes+page.slices[1].data_offset,data,sizeof(data)),"pending explicit-length bytes copied");
   pending[0]='X';
   check(page.bytes[page.slices[1].data_offset]==0xe2,"page owns its bytes"); pending[0]=0xe2;
   check(!memcmp(&f,&saved,sizeof(f)),"inspection never commits pending record or dirties file");
   the_workbench_document_info(id,&after); check(after.revision==revision,"inspection does not advance revision");
   check(the_workbench_document_read(id,revision,2,0,8,1,&page)==THE_WORKBENCH_DOCUMENT_LIMIT,"small scalar budget refuses");
   check(the_workbench_document_read(id,revision,2,1,8,100,&page)==THE_WORKBENCH_DOCUMENT_INVALID,"scalar interior offset refuses");
   check(the_workbench_document_read(id,revision,1,0,9,100,&page)==THE_WORKBENCH_DOCUMENT_INVALID,"line budget bounded");
   strcpy(pane.id,"public.document"); pane.kind=THE_WORKBENCH_PANEL_DOCUMENT; pane.document_id=id;
   the_workbench_begin("public.docs"); check(the_workbench_put_panel(&pane)==0,"document candidate accepted");
   check(!the_workbench_panel_count() && !the_workbench_log_count(),"document candidate is inert");
   check(the_workbench_commit()==0,"publish document binding");
   strcpy(pane.id,"public.second"); the_workbench_begin("public.docs"); the_workbench_put_panel(&pane); the_workbench_commit();
   check(the_workbench_panel_at(0)->document_id==the_workbench_panel_at(1)->document_id,"two panes share immutable document");
   strcpy(pane.id,"other.shared"); the_workbench_begin("other.docs");
   check(the_workbench_put_panel(&pane)==0 && the_workbench_commit()==0,"different owner may view the same live document");
   other.lines=1; other.line[0][0]='Z'; other.length[0]=1;
   pane.document_id=the_workbench_document_identify(&other); strcpy(pane.id,"other.distinct");
   the_workbench_begin("other.docs");
   check(the_workbench_put_panel(&pane)==0 && the_workbench_commit()==0,"different owner may view another document");
   pane.document_id=id; strcpy(pane.id,"public.second");
   check(the_workbench_document_json(json,sizeof(json),pane.id,revision,2,0,8,100)==0 && strstr(json,"\\u0000") && strstr(json,"\\u001b") && strstr(json,"\\ufffd") && strstr(json,"e282ac001b3cff"),"JSON display text and exact raw bytes");
   check(the_workbench_document_json(json,12,pane.id,revision,1,0,8,100)==94 && !*json,"no truncated document JSON");
   the_workbench_document_enter(); the_workbench_document_enter();
   check(the_workbench_document_info(id,&metadata)==THE_WORKBENCH_DOCUMENT_BUSY,"nested execution refuses reads");
   the_workbench_document_leave(); check(the_workbench_document_read(id,0,1,0,8,100,&page)==THE_WORKBENCH_DOCUMENT_BUSY,"inner leave does not release outer scope");
   the_workbench_document_leave();
   check(the_workbench_document_read(id,revision,1,0,8,100,&page)==THE_WORKBENCH_DOCUMENT_STALE,"old token stales conservatively");
   the_workbench_document_info(id,&metadata); revision=metadata.revision;
   blocked=1; check(the_workbench_document_read(id,revision,1,0,8,100,&page)==THE_WORKBENCH_DOCUMENT_BUSY,"modal/macro provider refuses"); blocked=0;
   the_workbench_document_record_unknown();
   check(the_workbench_document_info(id,&metadata)==THE_WORKBENCH_DOCUMENT_BUSY && !strcmp(metadata.reason,"record-unavailable"),"scratch ownership refusal");
   certified_empty=1;
   check(the_workbench_document_read(id,0,1,0,8,100,&page)==THE_WORKBENCH_DOCUMENT_OK && !memcmp(page.bytes,"alpha",5),"certified TOF record permits underlying bounded read");
   f.lines=0;
   check(the_workbench_document_read(id,0,1,0,8,100,&page)==THE_WORKBENCH_DOCUMENT_OK && !page.count && !page.more,"certified empty buffer reads no sentinel bytes");
   f.lines=3; certified_empty=0;
   bind(&f,2); f.line[1][0]='D';
   check(the_workbench_document_info(id,&metadata)==THE_WORKBENCH_DOCUMENT_BUSY,"direct LINE rewrite cannot receive stale rec overlay");
   bind(&f,2);
   memset(f.line[0],'a',9000); f.length[0]=9000; memcpy(f.line[0]+8190,"\xe2\x82\xac",3); the_workbench_document_changed();
   the_workbench_document_info(id,&metadata); revision=metadata.revision;
   check(the_workbench_document_read(id,revision,1,0,8,8192,&page)==THE_WORKBENCH_DOCUMENT_OK && page.length==8190 && page.next_line==1 && page.next_offset==8190 && page.more,"long line stops before scalar");
   check(the_workbench_document_read(id,revision,page.next_line,page.next_offset,8,4,&page)==THE_WORKBENCH_DOCUMENT_OK && page.length==4 && page.bytes[0]==0xe2 && page.next_offset==8194,"continuation makes scalar-safe progress");
   change_during_read=1;
   check(the_workbench_document_read(id,0,1,0,8,100,&page)==THE_WORKBENCH_DOCUMENT_STALE && !page.count,"reentrant provider mutation publishes no page");
   memset(f.line[0],0,9000); the_workbench_document_changed();
   check(the_workbench_document_json(json,sizeof(json),pane.id,0,1,0,8,8192)==0
         && strlen(json)<sizeof(json) && strstr(json,"\"length\":7168") && strstr(json,"\"more\":true"),"worst-case raw and escaped representations fit bridge capacity");
   the_workbench_document_forget(&f);
   check(the_workbench_document_info(id,&metadata)==THE_WORKBENCH_DOCUMENT_MISSING,"close invalidates document");
   reopened=the_workbench_document_identify(&f); bind(&f,2);
   check(reopened>id,"same-address reopen gets a fresh identity");
   check(the_workbench_document_json(json,sizeof(json),pane.id,0,1,0,8,100)==-3 && !strcmp(the_workbench_last_result()->code,"document-closed"),"old pane never rebinds to reopened file");
   pane.document_id=reopened; the_workbench_begin("public.docs"); the_workbench_put_panel(&pane); the_workbench_commit();
   check(the_workbench_document_json(json,sizeof(json),pane.id,0,1,0,8,100)==0,"explicit replacement binds new document");
   the_workbench_document_reset();
   check(the_workbench_document_identify(&f)>reopened,"reset never reuses identity");
   the_workbench_document_reset(); the_workbench_reset();
   if (failures) return 1;
   puts("workbench document tests passed"); return 0;
}
