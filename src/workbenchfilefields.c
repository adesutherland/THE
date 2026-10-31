/* Retained CTLCHAR file projections. Only the editor owner calls mutations. */
#include "workbenchfilefields.h"
#include "workbenchruntime.h"
#include "workbenchdocument.h"
#include "textpos.h"
#include <inttypes.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct { unsigned char *bytes; size_t length; int cells; } Record;
typedef struct {
   TheWorkbenchFileFieldsState state;
   TheWorkbenchFileFieldsConfig config;
   Record records[THE_WORKBENCH_FILEFIELDS_RECORDS_MAX];
   TheWorkbenchFileFieldSpan spans[THE_WORKBENCH_FILEFIELDS_SPANS_MAX];
   size_t retained;
} Projection;
struct TheWorkbenchFileFieldsPublication { Projection *items[THE_WORKBENCH_FILEFIELDS_MAX]; size_t count; unsigned char fresh[THE_WORKBENCH_FILEFIELDS_MAX]; unsigned long generations[THE_WORKBENCH_FILEFIELDS_MAX]; };
static TheWorkbenchFileFieldsPublication live;
static TheWorkbenchFileFieldsProvider provider;
static TheWorkbenchFileFieldsReceipt receipt;
static uint64_t revision;
static short fail(const char *code,short rc,const char *message) { return the_workbench_result(code,rc,message); }
static short provider_error(short rc) { return fail(provider.error ? provider.error() : "record-unavailable",rc,"The complete logical source is unavailable"); }
static int index_of(const char *id) { size_t i; for(i=0;id && i<live.count;i++) if(!strcmp(live.items[i]->state.panel,id)) return (int)i; return -1; }
const TheWorkbenchFileFieldsState *the_workbench_filefields_state(const char *id) { int i=index_of(id); return i<0 ? NULL : &live.items[i]->state; }
const TheWorkbenchFileFieldSpan *the_workbench_filefields_span(const char *id,size_t n) { int i=index_of(id); return i<0 || n>=live.items[i]->state.spans ? NULL : live.items[i]->spans+n; }
const TheWorkbenchFileFieldsReceipt *the_workbench_filefields_receipt(void) { return &receipt; }
uint64_t the_workbench_filefields_state_revision(void) { return revision; }
void the_workbench_filefields_provider(const TheWorkbenchFileFieldsProvider *p) { if(p) provider=*p; else memset(&provider,0,sizeof(provider)); }
static void destroy(Projection *p) { size_t i; if(!p) return; for(i=0;i<p->state.records;i++) free(p->records[i].bytes); free(p); }
void the_workbench_filefields_reset(void) { size_t i; for(i=0;i<live.count;i++) destroy(live.items[i]); memset(&live,0,sizeof(live)); memset(&receipt,0,sizeof(receipt)); revision=0; }
size_t the_workbench_filefields_span_count(void) { size_t i,n=0; for(i=0;i<live.count;i++) n+=live.items[i]->state.spans; return n; }
short the_workbench_filefields_close(uint64_t id) { size_t i; for(i=0;i<live.count;i++) if(live.items[i]->state.active && live.items[i]->state.document_id==id) return fail("object-in-use",-3,"Cancel or remove the file-field pane before closing its buffer"); return 0; }
static int config_equal(const TheWorkbenchFileFieldsConfig *a,const TheWorkbenchFileFieldsConfig *b) { return a->escape==b->escape && a->width==b->width && a->truncation==b->truncation && a->enabled==b->enabled && !memcmp(a->tags,b->tags,sizeof(a->tags)) && !memcmp(a->editable,b->editable,sizeof(a->editable)); }
static short add_span(Projection *p,size_t line,size_t start,size_t n,int cell,int editable) {
   TheWorkbenchFileFieldSpan *s; int width;
   if(!n) return 0;
   if(n>THE_WORKBENCH_FIELD_VALUE_MAX || p->state.spans==THE_WORKBENCH_FILEFIELDS_SPANS_MAX) return fail("limit-exceeded",94,"File-field span limit exceeded");
   if(!the_workbench_field_value_valid((const char *)p->records[line-1].bytes+start,n,1024)) return fail("unsupported-source",5,"File fields require printable UTF-8 text and supported CTLCHAR markers");
   width=textpos_from_byte(p->records[line-1].bytes+start,n,n).cell_column;
   if(width<1) return fail("unsupported-source",5,"A field must contain a positive fixed cell capacity");
   s=p->spans+p->state.spans; snprintf(s->id,sizeof(s->id),"s%lu",(unsigned long)p->state.spans+1);
   s->line=line; s->byte_start=start; s->length=n; s->cell=cell; s->maxcells=width; s->editable=editable;
   memcpy(s->value,p->records[line-1].bytes+start,n); s->value[n]=0; p->state.spans++;
   if(n>THE_WORKBENCH_FILEFIELDS_BYTES_MAX-p->retained) return fail("limit-exceeded",94,"File-field capture byte limit exceeded"); p->retained+=n;
   return 0;
}
static short parse_record(Projection *p,size_t line,const unsigned char *bytes,size_t n) {
   size_t pos=0,start=0; int cell=0,editable=0; short rc; Record *r=p->records+line-1;
   if(n>THE_WORKBENCH_FILEFIELDS_BYTES_MAX-p->retained || !(r->bytes=malloc(n+1))) return fail("limit-exceeded",94,"File-field capture byte limit exceeded");
   if(n) memcpy(r->bytes,bytes,n); r->bytes[n]=0; r->length=n; p->retained+=n;
   while(pos<=n) {
      int marker=pos+1<n && bytes[pos]==p->config.escape && p->config.tags[bytes[pos+1]];
      if(pos==n || marker) {
         size_t length=pos-start;
         if((rc=add_span(p,line,start,length,cell,editable))) return rc;
         if(length) cell+=p->spans[p->state.spans-1].maxcells;
         if(pos==n) break;
         editable=p->config.editable[bytes[pos+1]]; pos+=2; start=pos;
      } else pos++;
   }
   if(cell>8192) return fail("limit-exceeded",94,"A complete file-field row exceeds the inspection limit");
   { size_t i,total=0; for(i=0;i<p->state.spans;i++) if(p->spans[i].line==line) total+=strlen(p->spans[i].value); if(total>8192) return fail("limit-exceeded",94,"A complete file-field row exceeds the inspection byte limit"); }
   r->cells=cell; return 0;
}
static short capture(const TheWorkbenchPanel *panel,uint64_t draft,Projection **out) {
   Projection *p; TheDocumentWriteToken token,end; size_t lines,i; short rc; int editable=0;
   *out=NULL;
   if(!provider.config || !provider.info || !provider.line) return fail("capability-unavailable",-3,"No file-field owner provider is available");
   if((rc=the_edit_group_capture_token(panel->document_id,&token))) return fail(the_edit_group_error(),rc,"File-field capture is unavailable");
   p=calloc(1,sizeof(*p)); if(!p) return fail("limit-exceeded",94,"Unable to allocate file-field capture");
   strcpy(p->state.panel,panel->id); p->state.generation=panel->generation; p->state.document_id=panel->document_id;
   p->state.write_revision=token.write_revision; p->state.draft_revision=draft; p->state.active=1;
   if((rc=provider.config(panel->document_id,&p->config)) || (rc=provider.info(panel->document_id,&lines))) { provider_error(rc); goto refused; }
   if(!p->config.enabled || p->config.escape<32 || p->config.escape>=127) { rc=fail("unsupported-configuration",-3,"Enable FILECTL and CTLCHAR with an ASCII escape before capturing file fields"); goto refused; }
   for(i=0;i<256;i++) if(p->config.tags[i] && (i<32 || i>=127)) { rc=fail("unsupported-configuration",-3,"File-field CTLCHAR tags must be printable ASCII"); goto refused; }
   if(!lines || lines>THE_WORKBENCH_FILEFIELDS_RECORDS_MAX) { rc=fail("limit-exceeded",94,"File-field logical record limit exceeded"); goto refused; }
   p->state.records=lines;
   for(i=1;i<=lines;i++) { const unsigned char *bytes; size_t n;
      if((rc=provider.line(panel->document_id,i,&bytes,&n))) { provider_error(rc); goto refused; }
      if((rc=parse_record(p,i,bytes,n))) goto refused;
   }
   for(i=0;i<p->state.spans;i++) if(p->spans[i].editable) editable=1;
   if(!editable) { rc=fail("no-editable-fields",-3,"The complete source has no nonempty editable CTLCHAR fields"); goto refused; }
   if((rc=the_edit_group_capture_token(panel->document_id,&end))) { fail(the_edit_group_error(),rc,"Capture changed"); goto refused; }
   if(end.write_revision!=token.write_revision) { rc=fail("stale-revision",-3,"Source changed during capture"); goto refused; }
   *out=p; return 0;
refused: destroy(p); return rc;
}
short the_workbench_filefields_prepare(const TheWorkbenchPanel *panels,size_t count,size_t named,TheWorkbenchFileFieldsPublication **out) {
   TheWorkbenchFileFieldsPublication *p; size_t i,j,total=0,fields=named; short rc;
   if(!out) return 5; *out=NULL;
   if(count>THE_WORKBENCH_PANEL_MAX) return fail("limit-exceeded",94,"File-field projection limit exceeded");
   for(i=0;i<live.count;i++) {
      for(j=0;j<count;j++) if(!strcmp(live.items[i]->state.panel,panels[j].id) && panels[j].kind==THE_WORKBENCH_PANEL_FIELDS && panels[j].document_id==live.items[i]->state.document_id) break;
      if(j==count && live.items[i]->state.dirty) return fail("draft-in-use",-3,"Cancel staged file fields before removing or rebinding their pane");
   }
   p=calloc(1,sizeof(*p)); if(!p) return fail("limit-exceeded",94,"Unable to prepare file-field publication");
   for(i=0;i<count;i++) if(panels[i].kind==THE_WORKBENCH_PANEL_FIELDS && panels[i].document_id) {
      int previous=index_of(panels[i].id); Projection *item=NULL;
      if(previous>=0 && live.items[previous]->state.document_id==panels[i].document_id) item=live.items[previous];
      else { if((rc=capture(panels+i,1,&item))) { the_workbench_filefields_abort(p); return rc; } p->fresh[p->count]=1; }
      p->generations[p->count]=panels[i].generation; p->items[p->count++]=item; total+=item->retained; fields+=item->state.spans;
      if(total>THE_WORKBENCH_FILEFIELDS_SESSION_MAX || fields>THE_WORKBENCH_FIELD_MAX) { the_workbench_filefields_abort(p); return fail("limit-exceeded",94,"Shared file-field retention limit exceeded"); }
   }
   *out=p; return 0;
}
void the_workbench_filefields_abort(TheWorkbenchFileFieldsPublication *p) { size_t i; if(!p) return; for(i=0;i<p->count;i++) if(p->fresh[i]) destroy(p->items[i]); free(p); }
void the_workbench_filefields_commit(TheWorkbenchFileFieldsPublication *p) {
   size_t i,j; if(!p) return;
   for(i=0;i<live.count;i++) { for(j=0;j<p->count;j++) if(live.items[i]==p->items[j]) break; if(j==p->count) destroy(live.items[i]); }
   for(i=0;i<p->count;i++) p->items[i]->state.generation=p->generations[i];
   live=*p; free(p); revision++;
}
static void update_dirty(Projection *p) { size_t i; p->state.dirty=0;
   for(i=0;i<p->state.spans;i++) { TheWorkbenchFileFieldSpan *s=p->spans+i; const Record *r=p->records+s->line-1; size_t n=strlen(s->value);
      s->dirty=n!=s->length || memcmp(s->value,r->bytes+s->byte_start,n); if(s->dirty) p->state.dirty=1;
   }
}
static short current_config(Projection *p,int writing) {
   TheWorkbenchFileFieldsConfig now; short rc;
   if(!provider.config || (rc=provider.config(p->state.document_id,&now))) return provider_error(provider.config ? rc : -3);
   if(writing && now.readonly) return fail("read-only",12,"The source buffer is read-only");
   if(!config_equal(&p->config,&now)) return fail("stale-configuration",-3,"CTLCHAR, WIDTH or TRUNC policy changed; reload explicitly");
   return 0;
}
static short padded(const Projection *p,const TheWorkbenchFileFieldSpan *s,const char *value,size_t n,char *out,size_t *length) {
   int cells;
   if(!the_workbench_field_value_valid(value,n,s->maxcells)) return fail("invalid-operand",5,"Invalid UTF-8 field value or fixed cell capacity");
   /* Never allow a staged value to create or alter CTLCHAR markup. */
   if(memchr(value,p->config.escape,n)) return fail("marker-injection",5,"A staged field value cannot contain the CTLCHAR escape");
   cells=textpos_from_byte((const CHARTYPE *)value,n,n).cell_column;
   if((size_t)(s->maxcells-cells)>THE_WORKBENCH_FIELD_VALUE_MAX-n) return fail("limit-exceeded",94,"Padded fixed-cell field exceeds byte limit");
   memcpy(out,value,n); memset(out+n,' ',(size_t)(s->maxcells-cells)); *length=n+(size_t)(s->maxcells-cells); out[*length]=0; return 0;
}
static short edit_span(Projection *p,TheWorkbenchFileFieldSpan *s,const TheWorkbenchFileFieldsRequest *r,char *out,size_t *n,int *caret) {
   TextPos left,right; size_t a,b,encoded_length=0,old=strlen(s->value); CHARTYPE encoded[4]; int end,width=0; char unpadded[THE_WORKBENCH_FIELD_VALUE_MAX+1]; short rc;
   if(r->cell<0 || r->cell>s->maxcells) return fail("invalid-operand",5,"Invalid fixed-cell caret");
   left=textpos_from_cell((const CHARTYPE *)s->value,old,r->cell,TEXT_SNAP_BACKWARD); right=left;
   if(left.cell_column!=r->cell) return fail("invalid-operand",5,"Caret splits a text cluster");
   if(r->edit==THE_WORKBENCH_FORM_CLEAR) { unpadded[0]=0; *caret=0; return padded(p,s,unpadded,0,out,n); }
   if(r->edit==THE_WORKBENCH_FORM_INSERT) {
      if(r->codepoint>0x10ffff || (r->codepoint>=0xd800 && r->codepoint<=0xdfff) || r->codepoint<32 || (r->codepoint>=127 && r->codepoint<160)) return fail("invalid-operand",5,"Invalid printable Unicode scalar");
      width=text_codepoint_cell_width(r->codepoint); encoded_length=text_utf8_encode(r->codepoint,encoded);
      if(!encoded_length || width<0 || r->cell+width>s->maxcells) return fail("invalid-operand",5,"Codepoint exceeds field capacity");
      end=r->cell+width; right=textpos_from_cell((const CHARTYPE *)s->value,old,end,TEXT_SNAP_BACKWARD);
      if(right.cell_column<end) right=textpos_next_cluster((const CHARTYPE *)s->value,old,right);
   } else if(r->edit==THE_WORKBENCH_FORM_BACKSPACE) left=textpos_prev_cluster((const CHARTYPE *)s->value,old,left);
   else if(r->edit==THE_WORKBENCH_FORM_DELETE) right=textpos_next_cluster((const CHARTYPE *)s->value,old,right);
   else return fail("invalid-operand",5,"Invalid fixed-cell edit");
   a=left.byte_offset; b=right.byte_offset;
   if(a+encoded_length+old-b>THE_WORKBENCH_FIELD_VALUE_MAX) return fail("limit-exceeded",94,"Field byte limit exceeded");
   memcpy(unpadded,s->value,a); memcpy(unpadded+a,encoded,encoded_length); memcpy(unpadded+a+encoded_length,s->value+b,old-b); unpadded[a+encoded_length+old-b]=0;
   rc=padded(p,s,unpadded,a+encoded_length+old-b,out,n); if(!rc) *caret=left.cell_column+width; return rc;
}
static short apply(Projection *p,Projection **replacement,uint64_t *group) {
   TheDocumentLineEdit edits[THE_WORKBENCH_FILEFIELDS_RECORDS_MAX]; Projection *next; TheEditGroupPlan *plan=NULL; TheEditGroupReceipt result; size_t row,i; short rc;
   TheDocumentWriteToken expected={p->state.document_id,p->state.write_revision};
   if((rc=current_config(p,1))) return rc;
   next=calloc(1,sizeof(*next)); if(!next) return fail("limit-exceeded",94,"Unable to prepare complete file-field result");
   next->config=p->config; next->state=p->state; next->state.spans=0; next->state.dirty=0; next->state.draft_revision++;
   memset(edits,0,sizeof(edits));
   for(row=0;row<p->state.records;row++) {
      Record *r=p->records+row; unsigned char *after; size_t n=r->length,source=0,target=0;
      for(i=0;i<p->state.spans;i++) if(p->spans[i].line==row+1 && p->spans[i].editable) n=n-p->spans[i].length+strlen(p->spans[i].value);
      if(n>THE_WORKBENCH_FILEFIELDS_BYTES_MAX || !(after=malloc(n+1))) { rc=fail("limit-exceeded",94,"Unable to prepare complete file-field records"); goto failed; }
      for(i=0;i<p->state.spans;i++) { TheWorkbenchFileFieldSpan *s=p->spans+i; size_t length;
         if(s->line!=row+1 || !s->editable) continue;
         length=s->byte_start-source; memcpy(after+target,r->bytes+source,length); target+=length;
         length=strlen(s->value); memcpy(after+target,s->value,length); target+=length; source=s->byte_start+s->length;
      }
      memcpy(after+target,r->bytes+source,r->length-source); after[n]=0;
      rc=parse_record(next,row+1,after,n); free(after); if(rc) goto failed;
      edits[row].line=row+1; edits[row].before=r->bytes; edits[row].before_length=r->length; edits[row].after=next->records[row].bytes; edits[row].after_length=n;
   }
   if(next->state.spans!=p->state.spans) { rc=fail("marker-injection",5,"Apply changed the CTLCHAR partition"); goto failed; }
   for(i=0;i<p->state.spans;i++) if(next->spans[i].line!=p->spans[i].line || next->spans[i].cell!=p->spans[i].cell || next->spans[i].maxcells!=p->spans[i].maxcells || next->spans[i].editable!=p->spans[i].editable) { rc=fail("marker-injection",5,"Apply changed the CTLCHAR partition"); goto failed; }
   rc=the_edit_group_prepare(expected,edits,p->state.records,&plan);
   if(!rc) { rc=the_edit_group_commit(plan,&result); if(!rc) plan=NULL; }
   the_edit_group_abort(plan);
   if(rc) { fail(the_edit_group_error(),rc,"File-field apply refused; staged values retained"); goto failed; }
   next->state.write_revision=result.after_revision; *group=result.group; *replacement=next; return 0;
failed: destroy(next); return rc;
}
static short request(const TheWorkbenchFileFieldsRequest *r,int *caret,uint64_t *group) {
   int index; Projection *p,*next=NULL; const TheWorkbenchPanel *panel; size_t i; short rc;
   if(!r || !memchr(r->panel,0,sizeof(r->panel)) || !memchr(r->field,0,sizeof(r->field))) return fail("invalid-operand",5,"Invalid file-field request");
   panel=the_workbench_panel_find(r->panel); index=index_of(r->panel);
   if(!panel || panel->kind!=THE_WORKBENCH_PANEL_FIELDS || !panel->document_id || index<0) return fail("missing-object",5,"No published file-field pane has that ID");
   p=live.items[index];
   if(!r->panel_generation || r->panel_generation!=panel->generation) return fail("stale-generation",-3,"File-field pane definition changed");
   if(!r->draft_revision || r->draft_revision!=p->state.draft_revision) return fail("stale-revision",-3,"File-field draft changed");
   if(the_workbench_activation_active()) return fail("activation-pending",-3,"File fields cannot change during profile activation");
   /* Token admission checks modal input and certified pending ownership, even
    * for Cancel/Return; those operations never flush the working record. */
   {
      TheDocumentWriteToken token; rc=the_edit_group_token(p->state.document_id,&token);
      if(rc && ((r->operation!=THE_WORKBENCH_FILEFIELDS_CANCEL && r->operation!=THE_WORKBENCH_FILEFIELDS_RETURN) || !strcmp(the_edit_group_error(),"busy-modal") || !strcmp(the_edit_group_error(),"activation-pending"))) return fail(the_edit_group_error(),rc,"File-field owner is unavailable");
   }
   if(r->operation==THE_WORKBENCH_FILEFIELDS_RETURN) return the_workbench_runtime_panel(r->panel,panel->generation,THE_WORKBENCH_PANEL_RETURN);
   if(revision==UINT64_MAX || p->state.draft_revision==UINT64_MAX) return fail("limit-exceeded",94,"File-field revision limit exceeded");
   if(r->operation==THE_WORKBENCH_FILEFIELDS_CANCEL) {
      for(i=0;i<p->state.spans;i++) { TheWorkbenchFileFieldSpan *s=p->spans+i; memcpy(s->value,p->records[s->line-1].bytes+s->byte_start,s->length); s->value[s->length]=0; s->dirty=0; }
      p->retained=0;
      for(i=0;i<p->state.records;i++) p->retained+=p->records[i].length;
      for(i=0;i<p->state.spans;i++) p->retained+=p->spans[i].length;
      p->state.dirty=0; p->state.active=0; p->state.draft_revision++; revision++;
   } else if(r->operation==THE_WORKBENCH_FILEFIELDS_RELOAD) {
      if((rc=capture(panel,p->state.draft_revision+1,&next))) return rc;
      if(next->state.spans+the_workbench_filefields_span_count()-p->state.spans+the_workbench_field_count()>THE_WORKBENCH_FIELD_MAX) { destroy(next); return fail("limit-exceeded",94,"Shared field limit exceeded"); }
      live.items[index]=next; destroy(p); revision++;
   } else if(!p->state.active) return fail("projection-released",-3,"Reload the cancelled file-field pane before editing");
   else if(r->operation==THE_WORKBENCH_FILEFIELDS_SET || r->operation==THE_WORKBENCH_FILEFIELDS_EDIT) {
      char value[THE_WORKBENCH_FIELD_VALUE_MAX+1]; size_t n; TheWorkbenchFileFieldSpan *s=NULL;
      if((rc=current_config(p,1))) return rc;
      for(i=0;i<p->state.spans;i++) if(!strcmp(p->spans[i].id,r->field)) { s=p->spans+i; break; }
      if(!s) return fail("missing-object",5,"No file field has that ID");
      if(!s->editable) return fail("field-protected",12,"This CTLCHAR span is protected");
      if(r->operation==THE_WORKBENCH_FILEFIELDS_SET) { if(r->length>THE_WORKBENCH_FIELD_VALUE_MAX) return fail("limit-exceeded",94,"Field byte limit exceeded"); rc=padded(p,s,r->value,r->length,value,&n); }
      else {
         TheWorkbenchFileFieldsRequest mapped=*r;
         if(mapped.cell==-1) {
            size_t byte=0,units=0,length=strlen(s->value);
            while(byte<length && units<r->utf16_offset) { size_t unit=the_workbench_document_unit((const unsigned char *)s->value+byte,length-byte); units+=unit==4 ? 2 : 1; byte+=unit; }
            if(units!=r->utf16_offset) return fail("invalid-operand",5,"UTF-16 offset splits a scalar or exceeds the value");
            { TextPos position=textpos_from_byte((const CHARTYPE *)s->value,length,byte); TextPos boundary=textpos_from_cell((const CHARTYPE *)s->value,length,position.cell_column,TEXT_SNAP_BACKWARD);
              if(boundary.byte_offset!=byte) return fail("invalid-operand",5,"UTF-16 offset splits a text cluster"); mapped.cell=position.cell_column; }
         }
         rc=edit_span(p,s,&mapped,value,&n,caret);
      }
      if(rc) return rc;
      if(strcmp(s->value,value)) {
         size_t old=strlen(s->value),total=0,row_bytes=0;
         for(i=0;i<live.count;i++) total+=live.items[i]->retained;
         for(i=0;i<p->state.spans;i++) if(p->spans[i].line==s->line) row_bytes+=strlen(p->spans[i].value);
         if(n>old && (n-old>THE_WORKBENCH_FILEFIELDS_BYTES_MAX-p->retained || n-old>THE_WORKBENCH_FILEFIELDS_SESSION_MAX-total || n-old>8192-row_bytes)) return fail("limit-exceeded",94,"Staged field exceeds projection, session or complete-row retention limit");
         p->retained=p->retained-old+n;
         memcpy(s->value,value,n+1); p->state.draft_revision++; revision++; update_dirty(p);
      }
   } else if(r->operation==THE_WORKBENCH_FILEFIELDS_APPLY) {
      if((rc=apply(p,&next,group))) return rc;
      live.items[index]=next; destroy(p); revision++;
   } else return fail("invalid-operand",5,"Invalid file-field operation");
   return fail("ok",0,"");
}
short the_workbench_filefields_request(const TheWorkbenchFileFieldsRequest *r) {
   int caret=-1; uint64_t group=0; short rc; const TheWorkbenchFileFieldsState *s;
   if(receipt.serial==UINT64_MAX) return fail("limit-exceeded",94,"File-field receipt limit exceeded");
   rc=request(r,&caret,&group); s=r && memchr(r->panel,0,sizeof(r->panel)) ? the_workbench_filefields_state(r->panel) : NULL;
   receipt.serial++; receipt.request_id=r ? r->request_id : 0; receipt.operation=r ? r->operation : 0; receipt.panel[0]=0;
   if(r && memchr(r->panel,0,sizeof(r->panel))) snprintf(receipt.panel,sizeof(receipt.panel),"%s",r->panel);
   receipt.panel_generation=s ? s->generation : 0; receipt.draft_revision=s ? s->draft_revision : 0; receipt.group=group; receipt.rc=rc; receipt.caret=rc ? -1 : caret; receipt.utf16_caret=-1;
   if(!rc && caret>=0 && r && s) {
      size_t i;
      for(i=0;i<s->spans;i++) { const TheWorkbenchFileFieldSpan *span=the_workbench_filefields_span(r->panel,i); if(!strcmp(span->id,r->field)) {
         size_t n=strlen(span->value),end=textpos_from_cell((const CHARTYPE *)span->value,n,caret,TEXT_SNAP_BACKWARD).byte_offset,byte=0; int units=0;
         while(byte<end) { size_t unit=the_workbench_document_unit((const unsigned char *)span->value+byte,n-byte); units+=unit==4 ? 2 : 1; byte+=unit; }
         receipt.utf16_caret=units; break;
      } }
   }
   snprintf(receipt.code,sizeof(receipt.code),"%s",the_workbench_last_result()->code); return rc;
}

typedef struct { char *out; size_t capacity,used; int good; } Writer;
static void emit(Writer *w,const char *fmt,...) { va_list ap; int n; if(!w->good) return; va_start(ap,fmt); n=vsnprintf(w->out+w->used,w->capacity-w->used,fmt,ap); va_end(ap); if(n<0 || (size_t)n>=w->capacity-w->used) w->good=0; else w->used+=(size_t)n; }
static void quote(Writer *w,const char *s) { const unsigned char *p=(const unsigned char *)s; emit(w,"\""); for(;*p;p++) { if(*p=='\"' || *p=='\\') emit(w,"\\%c",*p); else if(*p<32 || *p==127) emit(w,"\\u%04x",*p); else emit(w,"%c",*p); } emit(w,"\""); }
short the_workbench_filefields_json(char *out,size_t capacity,const char *id,size_t offset,size_t limit,size_t bytes) {
   int index=index_of(id); Projection *p; size_t row,i,count=0,used=0; Writer w={out,capacity,0,1};
   if(!out || !capacity) return 5; out[0]=0;
   if(index<0) return fail("missing-object",5,"No file-field projection has that ID"); p=live.items[index];
   if(!limit || limit>8 || !bytes || bytes>8192 || offset>p->state.records) return fail("invalid-operand",5,"Invalid file-field page budget");
   emit(&w,"{\"schemaVersion\":1,\"section\":\"file-fields\",\"panel\":"); quote(&w,id);
   emit(&w,",\"panelGeneration\":\"%lu\",\"documentId\":\"%" PRIu64 "\",\"writeRevision\":\"%" PRIu64 "\",\"draftRevision\":\"%" PRIu64 "\",\"dirty\":%s,\"active\":%s,\"total\":%lu,\"offset\":%lu,\"records\":[",p->state.generation,p->state.document_id,p->state.write_revision,p->state.draft_revision,p->state.dirty ? "true" : "false",p->state.active ? "true" : "false",(unsigned long)p->state.records,(unsigned long)offset);
   for(row=offset;row<p->state.records && count<limit;row++) {
      size_t n=0,spans=0; for(i=0;i<p->state.spans;i++) if(p->spans[i].line==row+1) n+=strlen(p->spans[i].value);
      if(n>bytes-used) break;
      emit(&w,"%s{\"line\":%lu,\"cells\":%d,\"spans\":[",count ? "," : "",(unsigned long)row+1,p->records[row].cells);
      for(i=0;i<p->state.spans;i++) { TheWorkbenchFileFieldSpan *s=p->spans+i; if(s->line!=row+1) continue;
         emit(&w,"%s{\"id\":",spans++ ? "," : ""); quote(&w,s->id); emit(&w,",\"cell\":%d,\"maxcells\":%d,\"editable\":%s,\"dirty\":%s,\"value\":",s->cell,s->maxcells,s->editable ? "true" : "false",s->dirty ? "true" : "false"); quote(&w,s->value); emit(&w,"}"); }
      emit(&w,"]}"); used+=n; count++;
   }
   if(!count && offset<p->state.records) { out[0]=0; return fail("limit-exceeded",94,"Complete file-field row exceeds page byte budget"); }
   emit(&w,"],\"nextOffset\":"); if(row<p->state.records) emit(&w,"%lu",(unsigned long)row); else emit(&w,"null"); emit(&w,"}");
   if(!w.good) { out[0]=0; return fail("limit-exceeded",94,"File-field JSON capacity exceeded"); } return 0;
}
