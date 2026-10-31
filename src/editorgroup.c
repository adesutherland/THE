#include "the.h"
#include "proto.h"
#include "vars.h"
#include "editorgroup.h"
#include "editordocument.h"
#include "panelui.h"
#include "workbench.h"
#ifdef THE_WASM_HIGHLIGHT
#include "wasm/highlighter.h"
#endif

#include <inttypes.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#ifdef THE_EDIT_GROUP_TEST_ALLOC
void *the_edit_group_test_malloc(size_t);
void *the_edit_group_test_calloc(size_t,size_t);
#define malloc the_edit_group_test_malloc
#define calloc the_edit_group_test_calloc
#endif

typedef struct {
   size_t number,before_length,after_length;
   LINE *line;
   CHARTYPE *replacement;
   unsigned char *before,*after;
   lineflags flags;
   int changed;
} GroupLine;
typedef struct {
   uint64_t id;
   FILE_DETAILS *file;
   size_t count,rec_line,rec_length,record_base_length;
   GroupLine lines[THE_EDIT_GROUP_LINES_MAX];
   unsigned char *old_rec,*record_base;
   unsigned short save_alt,autosave_alt;
   LENGTHTYPE width,truncation;
   int affected_record, checkpoint_touched;
} EditorGroup;
static const char *problem="ok";
static short refused(const char *code,short rc) { problem=code; return rc; }
static const char *error(void) { return problem; }

static int certified(const void **file,size_t *line,const unsigned char **base,size_t *base_length,
   const unsigned char **pending,size_t *length)
{ return the_workbench_document_record(file,line,base,base_length,pending,length); }
static short capture_available(uint64_t id)
{
   const void *owner; size_t line,base_length,length; const unsigned char *base,*pending;
   if(the_panel_active) return refused("busy-modal",-3);
   if(!the_workbench_document_file(id)) return refused("document-closed",-3);
   if(!certified(&owner,&line,&base,&base_length,&pending,&length)) return refused("record-unavailable",-3);
   problem="ok"; return 0;
}
static short available(uint64_t id)
{
   if(the_panel_active) return refused("busy-modal",-3);
   if(the_workbench_activation_active()) return refused("activation-pending",-3);
   return capture_available(id);
}
static void destroy(void *opaque)
{
   EditorGroup *p=opaque; size_t i;
   if(!p) return;
   for(i=0;i<p->count;i++) {
      if(p->lines[i].replacement) (*the_free)(p->lines[i].replacement);
      free(p->lines[i].before); free(p->lines[i].after);
   }
   free(p->old_rec); free(p->record_base); free(p);
}
static int reserve_bytes(size_t *total,size_t n)
{ if(n>THE_EDIT_GROUP_BYTES_MAX-*total) return 0; *total+=n; return 1; }
static short prepare(uint64_t id,const TheDocumentLineEdit *edits,size_t count,void **out,size_t *retained)
{
   FILE_DETAILS *file=(FILE_DETAILS *)the_workbench_document_file(id);
   EditorGroup *p; const void *owner; const unsigned char *base,*pending;
   size_t rec_line,base_length,pending_length,i,total=0;
   *out=NULL; *retained=0;
   if(!file || !certified(&owner,&rec_line,&base,&base_length,&pending,&pending_length)) return refused("record-unavailable",-3);
   if(pending_length>(size_t)max_line_length) return refused("record-unavailable",-3);
   if(ISREADONLY(file)) return refused("read-only",12);
   p=calloc(1,sizeof(*p)); if(!p) return refused("limit-exceeded",94);
   p->id=id; p->file=file; p->count=count; p->width=max_line_length; p->truncation=file->truncation_column;
   p->save_alt=file->save_alt; p->autosave_alt=file->autosave_alt;
   for(i=0;i<count;i++) {
      GroupLine *s=p->lines+i; const unsigned char *logical; size_t length;
      if(edits[i].line>(size_t)file->number_lines) goto invalid;
      s->number=edits[i].line; s->line=lll_find(file->first_line,file->last_line,(LINETYPE)s->number,file->number_lines);
      if(!s->line || s->line->length<0) goto invalid;
      logical=s->line->line; length=(size_t)s->line->length;
      if(owner==file && rec_line==s->number) { logical=pending; length=pending_length; }
      if(length!=edits[i].before_length || (length && memcmp(logical,edits[i].before,length))) goto stale;
      if(edits[i].after_length>(size_t)max_line_length
      || xedit_truncated_length(file,(CHARTYPE *)edits[i].after,(LENGTHTYPE)edits[i].after_length)<(LENGTHTYPE)edits[i].after_length) goto invalid;
      s->before_length=length; s->after_length=edits[i].after_length; s->flags=s->line->flags;
      s->changed=length!=s->after_length || (length && memcmp(logical,edits[i].after,length));
      if(!reserve_bytes(&total,length) || !reserve_bytes(&total,s->after_length)
      || !reserve_bytes(&total,(size_t)s->line->length) || !reserve_bytes(&total,s->after_length)) goto limit;
      s->before=malloc(length+1); s->after=malloc(s->after_length+1);
      s->replacement=(*the_malloc)(s->after_length+1);
      if(!s->before || !s->after || !s->replacement) goto limit;
      if(length) memcpy(s->before,logical,length); s->before[length]=0;
      if(s->after_length) { memcpy(s->after,edits[i].after,s->after_length); memcpy(s->replacement,edits[i].after,s->after_length); }
      s->after[s->after_length]=s->replacement[s->after_length]=0;
      if(s->changed && owner==file && rec_line==s->number) {
         p->affected_record=1;
         p->rec_line=rec_line; p->rec_length=pending_length; p->record_base_length=s->after_length;
         if(!reserve_bytes(&total,pending_length) || !reserve_bytes(&total,base_length) || !reserve_bytes(&total,s->after_length)) goto limit;
         p->old_rec=malloc(pending_length+1); p->record_base=malloc(s->after_length+1);
         if(!p->old_rec || !p->record_base) goto limit;
         if(pending_length) memcpy(p->old_rec,pending,pending_length); p->old_rec[pending_length]=0;
         if(s->after_length) memcpy(p->record_base,s->after,s->after_length); p->record_base[s->after_length]=0;
      }
   }
   *out=p; *retained=total; return 0;
invalid: destroy(p); return refused("invalid-operand",5);
stale: destroy(p); return refused("stale-revision",-3);
limit: destroy(p); return refused("limit-exceeded",94);
}
static short validate(void *opaque,int undo)
{
   EditorGroup *p=opaque; const void *owner; const unsigned char *base,*pending;
   size_t rec_line,base_length,pending_length,i; short rc=available(p->id);
   if(rc) return rc;
   if(the_workbench_document_file(p->id)!=p->file) return refused("document-closed",-3);
   if(ISREADONLY(p->file)) return refused("read-only",12);
   if(max_line_length!=p->width || p->file->truncation_column!=p->truncation) return refused("stale-revision",-3);
   if(!certified(&owner,&rec_line,&base,&base_length,&pending,&pending_length)) return refused("record-unavailable",-3);
   if(pending_length>(size_t)max_line_length) return refused("record-unavailable",-3);
   for(i=0;i<p->count;i++) {
      GroupLine *s=p->lines+i;
      const unsigned char *logical; size_t length,expected_length=undo ? s->after_length : s->before_length;
      if(s->number>(size_t)p->file->number_lines
      || lll_find(p->file->first_line,p->file->last_line,(LINETYPE)s->number,p->file->number_lines)!=s->line) return refused("stale-revision",-3);
      logical=s->line->line; length=(size_t)s->line->length;
      if(owner==p->file && rec_line==s->number) { logical=pending; length=pending_length; }
      if(length!=expected_length || (length && memcmp(logical,undo ? s->after : s->before,length))) return refused("stale-revision",-3);
      if(!undo && (s->after_length>(size_t)max_line_length
      || xedit_truncated_length(p->file,s->after,(LENGTHTYPE)s->after_length)<(LENGTHTYPE)s->after_length)) return refused("invalid-operand",5);
   }
   if(p->affected_record && (owner!=p->file || rec_line!=p->rec_line)) return refused("stale-revision",-3);
   if(!undo && (p->save_alt!=p->file->save_alt || p->autosave_alt!=p->file->autosave_alt)) return refused("stale-revision",-3);
   return 0;
}
static unsigned short alteration(unsigned short value)
{ return value==USHRT_MAX ? value : (unsigned short)(value+1); }
static void publish(void *opaque,int undo)
{
   EditorGroup *p=opaque; size_t i;
#ifdef USE_SDSLH
   /* Existing parser load APIs can terminate on allocation failure. Retire the
    * old derived snapshot and leave it explicitly inactive after publication. */
   sdslh_shutdown_file(p->file);
#endif
   for(i=0;i<p->count;i++) {
      GroupLine *s=p->lines+i; CHARTYPE *old=s->line->line;
      if(!s->changed) continue;
      s->line->line=s->replacement; s->replacement=old;
      s->line->length=(LENGTHTYPE)(undo ? (s->number==p->rec_line && p->affected_record ? p->record_base_length : s->before_length) : s->after_length);
      if(undo) s->line->flags=s->flags; else s->line->flags.changed_flag=TRUE;
      if(p->affected_record && s->number==p->rec_line) {
         size_t n=undo ? p->rec_length : s->after_length;
         if(n) memcpy(rec,undo ? p->old_rec : s->after,n);
         memset(rec+n,' ',(size_t)max_line_length-n); rec_len=(LENGTHTYPE)n;
         the_workbench_document_record_exchange(p->file,p->rec_line,&p->record_base,&p->record_base_length);
      }
   }
   p->file->save_alt=undo ? p->save_alt : alteration(p->save_alt);
   p->file->autosave_alt=undo ? p->autosave_alt : alteration(p->autosave_alt);
   the_workbench_document_changed();
}
static int parser_available(uint64_t id)
{
#ifdef USE_SDSLH
   const FILE_DETAILS *file=the_workbench_document_file(id);
   return file && (!(file->colouring && file->parser && file->parser->is_sdslh_parser) || file->cb!=NULL);
#else
   (void)id; return 1;
#endif
}
static void finish(void *opaque,int undo,TheEditGroupReceipt *receipt)
{
   EditorGroup *p=opaque; FILE_DETAILS *file=p->file;
   (void)undo;
#ifdef THE_WASM_HIGHLIGHT
   /* This may invoke the worker bridge; publication has already completed. */
   the_wasm_highlight_forget(file);
#endif
   receipt->parser_available=parser_available(p->id);
   if((undo && p->checkpoint_touched) || (file->autosave && file->autosave_alt>=file->autosave && file->autosave_fname)) {
      /* save_file reads current-view formatting and scope. Do not borrow a
       * different buffer's view merely to perform an optional checkpoint. */
      if(!CURRENT_VIEW || CURRENT_FILE!=file) receipt->autosave_rc=-3;
      else {
         p->checkpoint_touched=1;
         receipt->autosave_rc=save_file(file,file->autosave_fname,TRUE,file->number_lines,1L,NULL,FALSE,0,max_line_length,TRUE,FALSE,TRUE);
         if(!receipt->autosave_rc) file->autosave_alt=0;
      }
   }
}
void the_editor_edit_group_initialize(void)
{
   static const TheEditGroupProvider p={available,prepare,validate,publish,finish,destroy,parser_available,error,capture_available};
   the_edit_group_provider(&p);
}
short the_editor_edit_group_json(char *out,size_t cap,uint64_t id)
{
   TheDocumentWriteToken token; TheEditGroupInfo info; const TheEditGroupReceipt *last; short rc; int n;
   the_editor_edit_group_initialize();
   if(!id) id=the_editor_document_active();
   if(!out || !cap) return 5; out[0]=0;
   if((rc=the_edit_group_token(id,&token))) return rc;
   the_edit_group_info(&info); last=the_edit_group_receipt();
   n=snprintf(out,cap,"{\"schemaVersion\":1,\"section\":\"write\",\"documentId\":\"%" PRIu64 "\",\"writeRevision\":\"%" PRIu64 "\",\"group\":\"%" PRIu64 "\",\"undoAvailable\":%s,\"parserAvailable\":%s,\"autosaveRc\":%d}",token.document_id,token.write_revision,info.document_id==id ? info.group : 0,info.document_id==id && info.undo_available ? "true" : "false",parser_available(id) ? "true" : "false",last->document_id==id ? (int)last->autosave_rc : 0);
   if(n<0 || (size_t)n>=cap) { out[0]=0; return 94; } return 0;
}
