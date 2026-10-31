#include "editgroup.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#ifdef THE_EDIT_GROUP_TEST_ALLOC
void *the_edit_group_test_malloc(size_t);
void *the_edit_group_test_calloc(size_t,size_t);
#define malloc the_edit_group_test_malloc
#define calloc the_edit_group_test_calloc
#endif

struct TheEditGroupPlan {
   TheDocumentWriteToken expected;
   TheEditGroupProvider provider;
   void *prepared;
   size_t changed_lines;
   uint64_t group, after_revision;
};
static TheEditGroupProvider provider;
static TheEditGroupPlan *latest;
static TheEditGroupReceipt receipt;
static uint64_t mutation_epoch=1, next_group;
static size_t mutation_depth;
static int exhausted, publishing, finishing;
static const char *problem="ok";

static short fail(const char *code,short rc)
{ problem=code; receipt.rc=rc; snprintf(receipt.code,sizeof(receipt.code),"%s",code); return rc; }
static short provider_fail(const TheEditGroupProvider *p,short rc)
{ return fail(p->error ? p->error() : rc==12 ? "read-only" : rc==94 ? "limit-exceeded" : rc==-3 ? "stale-revision" : "invalid-operand",rc); }
static short refused_out(short rc,TheEditGroupReceipt *out) { if(out) *out=receipt; return rc; }
const char *the_edit_group_error(void) { return problem; }
const TheEditGroupReceipt *the_edit_group_receipt(void) { return &receipt; }
void the_edit_group_changed(void)
{ if (finishing) return; if (mutation_epoch==UINT64_MAX) exhausted=1; else mutation_epoch++; }
void the_edit_group_mutation_enter(void)
{ mutation_depth++; the_edit_group_changed(); }
void the_edit_group_mutation_leave(void)
{ if (mutation_depth) mutation_depth--; }
void the_edit_group_provider(const TheEditGroupProvider *p)
{ if (p) provider=*p; else memset(&provider,0,sizeof(provider)); }
void the_edit_group_abort(TheEditGroupPlan *p)
{ if(p) { if(p->prepared && p->provider.destroy) p->provider.destroy(p->prepared); free(p); } }
void the_edit_group_reset(void)
{ the_edit_group_abort(latest); latest=NULL; mutation_depth=0; memset(&receipt,0,sizeof(receipt)); the_edit_group_changed(); }
void the_edit_group_forget(uint64_t id)
{ if(latest && latest->expected.document_id==id) { the_edit_group_abort(latest); latest=NULL; } the_edit_group_changed(); }
void the_edit_group_info(TheEditGroupInfo *out)
{
   if(!out) return;
   memset(out,0,sizeof(*out));
   if(latest) {
      out->group=latest->group; out->document_id=latest->expected.document_id;
      out->after_revision=latest->after_revision; out->changed_lines=latest->changed_lines;
      out->undo_available=!exhausted && latest->after_revision==mutation_epoch;
   }
}
short the_edit_group_token(uint64_t id,TheDocumentWriteToken *out)
{
   short rc;
   if(!out || !id) return fail("invalid-operand",5);
   memset(out,0,sizeof(*out));
   if(publishing) return fail("busy-document",-3);
   if(exhausted) return fail("limit-exceeded",94);
   if(!provider.available) return fail("capability-unavailable",-3);
   if((rc=provider.available(id))) return provider_fail(&provider,rc);
   out->document_id=id; out->write_revision=mutation_epoch; problem="ok"; return 0;
}
static int utf8(const unsigned char *p,size_t length)
{
   size_t i=0,n,j; unsigned value;
   if(!p && length) return 0;
   while(i<length) {
      unsigned c=p[i];
      if(!c || c=='\r' || c=='\n') return 0;
      if(c<0x80) { i++; continue; }
      if(c>=0xc2 && c<=0xdf) { n=2; value=c&31; }
      else if(c>=0xe0 && c<=0xef) { n=3; value=c&15; }
      else if(c>=0xf0 && c<=0xf4) { n=4; value=c&7; } else return 0;
      if(n>length-i) return 0;
      for(j=1;j<n;j++) { c=p[i+j]; if((c&0xc0)!=0x80) return 0; value=value*64+(c&63); }
      if((n==3 && value<0x800) || (n==4 && value<0x10000) || value>0x10ffff || (value>=0xd800 && value<=0xdfff)) return 0;
      i+=n;
   }
   return 1;
}
short the_edit_group_prepare(TheDocumentWriteToken expected,const TheDocumentLineEdit *edits,size_t count,TheEditGroupPlan **out)
{
   TheEditGroupPlan *p; TheDocumentWriteToken current; size_t i,j,total=0,retained=0,changed=0; short rc;
   if(!out) return fail("invalid-operand",5); *out=NULL;
   if(!edits || !count || count>THE_EDIT_GROUP_LINES_MAX || !expected.write_revision) return fail("invalid-operand",5);
   if((rc=the_edit_group_token(expected.document_id,&current))) return rc;
   if(expected.write_revision!=current.write_revision) return fail("stale-revision",-3);
   for(i=0;i<count;i++) {
      if(edits[i].before_length>THE_EDIT_GROUP_BYTES_MAX-total) return fail("limit-exceeded",94);
      total+=edits[i].before_length;
      if(edits[i].after_length>THE_EDIT_GROUP_BYTES_MAX-total) return fail("limit-exceeded",94);
      total+=edits[i].after_length;
      if(!edits[i].line || !utf8(edits[i].before,edits[i].before_length) || !utf8(edits[i].after,edits[i].after_length)) return fail("invalid-operand",5);
      for(j=0;j<i;j++) if(edits[i].line==edits[j].line) return fail("invalid-operand",5);
      if(edits[i].before_length!=edits[i].after_length || (edits[i].before_length && memcmp(edits[i].before,edits[i].after,edits[i].before_length))) changed++;
   }
   if(!provider.prepare || !provider.validate || !provider.publish || !provider.destroy) return fail("capability-unavailable",-3);
   p=calloc(1,sizeof(*p)); if(!p) return fail("limit-exceeded",94);
   p->expected=expected; p->provider=provider; p->changed_lines=changed;
   rc=p->provider.prepare(expected.document_id,edits,count,&p->prepared,&retained);
   if(rc || retained>THE_EDIT_GROUP_BYTES_MAX) { the_edit_group_abort(p); return rc ? provider_fail(&provider,rc) : fail("limit-exceeded",94); }
   if(mutation_epoch!=expected.write_revision) { the_edit_group_abort(p); return fail("stale-revision",-3); }
   *out=p; problem="ok"; return 0;
}
static void completed(TheEditGroupPlan *p,uint64_t before,int undo,TheEditGroupReceipt *out)
{
   memset(&receipt,0,sizeof(receipt)); receipt.group=p->group; receipt.document_id=p->expected.document_id;
   receipt.before_revision=before; receipt.after_revision=mutation_epoch; receipt.changed_lines=p->changed_lines;
   receipt.undo_available=!undo; receipt.parser_available=p->provider.parser_available ? p->provider.parser_available(p->expected.document_id) : 1; strcpy(receipt.code,"ok");
   finishing=1;
   if(p->provider.finish) p->provider.finish(p->prepared,undo,&receipt);
   finishing=0;
   if(out) *out=receipt;
}
short the_edit_group_commit(TheEditGroupPlan *p,TheEditGroupReceipt *out)
{
   short rc; uint64_t before;
   if(!p || p==latest) return refused_out(fail("invalid-operand",5),out);
   if(publishing) return refused_out(fail("busy-document",-3),out);
   if(exhausted || (p->changed_lines && (mutation_epoch==UINT64_MAX || next_group==UINT64_MAX))) return refused_out(fail("limit-exceeded",94),out);
   if(p->expected.write_revision!=mutation_epoch) return refused_out(fail("stale-revision",-3),out);
   if((rc=p->provider.validate(p->prepared,0))) return refused_out(provider_fail(&p->provider,rc),out);
   before=mutation_epoch;
   if(!p->changed_lines) { memset(&receipt,0,sizeof(receipt)); receipt.document_id=p->expected.document_id; receipt.before_revision=receipt.after_revision=before; receipt.parser_available=p->provider.parser_available ? p->provider.parser_available(p->expected.document_id) : 1; strcpy(receipt.code,"ok"); if(out) *out=receipt; the_edit_group_abort(p); problem="ok"; return 0; }
   publishing=1;
   p->provider.publish(p->prepared,0);
   the_edit_group_changed(); p->group=++next_group; p->after_revision=mutation_epoch;
   the_edit_group_abort(latest); latest=p;
   publishing=0; problem="ok"; completed(p,before,0,out); return 0;
}
short the_edit_group_undo(uint64_t group,TheDocumentWriteToken expected,TheEditGroupReceipt *out)
{
   short rc; TheEditGroupPlan *p=latest; uint64_t before=mutation_epoch;
   if(publishing) return refused_out(fail("busy-document",-3),out);
   if(!p || !group || group!=p->group || expected.document_id!=p->expected.document_id) return refused_out(fail("undo-unavailable",-3),out);
   if(exhausted || mutation_epoch==UINT64_MAX) return refused_out(fail("limit-exceeded",94),out);
   if(expected.write_revision!=mutation_epoch || p->after_revision!=mutation_epoch) return refused_out(fail("stale-revision",-3),out);
   if((rc=p->provider.validate(p->prepared,1))) return refused_out(provider_fail(&p->provider,rc),out);
   publishing=1; p->provider.publish(p->prepared,1); the_edit_group_changed(); latest=NULL; publishing=0;
   problem="ok"; completed(p,before,1,out); the_edit_group_abort(p); return 0;
}
