#include "the.h"
#include "proto.h"
#include "vars.h"
#include "editorclipboard.h"
#include "editordocument.h"
#include "editgroup.h"
#include "panelui.h"
#include "workbench.h"
#include "thedriver.h"
#include "driverwindow.h"
#include "llm/llmruntime.h"
#ifdef THE_WASM_HIGHLIGHT
#include "wasm/highlighter.h"
#endif
#include <inttypes.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
   VIEW_DETAILS *view;
   FILE_DETAILS *file;
   TheDocumentWriteToken write;
   uint64_t token;
   LINETYPE focus,start,end;
   size_t first_byte,last_byte,padding;
   int selected,window,mark_type,marked_line,marked_col,field_whole;
   size_t field_length; uint64_t field_hash;
   LINETYPE mark_start_line,mark_end_line;
   LENGTHTYPE mark_start_col,mark_end_col;
   LogicalCursorState cursor;
} Capture;
typedef struct {
   Capture context;
   LINE *left,*right,*before_first,*before_last,*after_first,*after_last;
   size_t before_count,after_count;
   LINETYPE total_before;
   unsigned short save_alt,autosave_alt;
   LINETYPE old_current,old_focus;
   LENGTHTYPE old_verify;
   LogicalCursorState old_cursor;
   unsigned char *old_record,*old_base,*new_base;
   size_t old_record_length,old_base_length,new_base_length;
   LINETYPE old_record_line,new_focus;
   int new_cell,published,checkpoint_touched;
} ClipboardEdit;
static Capture captured;
static uint64_t next_token,clipboard_group;
static int journal_window;
static uint64_t journal_field_hash;
static size_t journal_field_length;
static int history_available(void);
static const char *problem="ok";
static short refuse(const char *code,short rc) { problem=code; return rc; }
const char *the_editor_clipboard_error(void) { return problem; }
const char *the_editor_clipboard_error_message(void)
{
   static const struct { const char *code,*message; } messages[]={
      {"ok","Completed."},
      {"no-selection","Select source text before copying or cutting."},
      {"stale-revision","The file, field or caret changed. Try the clipboard action again."},
      {"read-only","This source is read-only. Copy remains available."},
      {"limit-exceeded","The clipboard text exceeds the editor's byte, line or field limit."},
      {"invalid-utf8","The clipboard contains invalid UTF-8 or unsupported control characters."},
      {"multiline-field","Command and prefix fields accept one line. No text was inserted."},
      {"multiple-views","Close the additional views of this file before pasting or cutting source."},
      {"pending-prefix","Complete or clear pending prefix commands before pasting or cutting source."},
      {"line-metadata","The selected records have line names or pending commands. No source was changed."},
      {"unsupported-selection","Clipboard replacement supports stream, CUA and line selections."},
      {"field-selection-unavailable","This field has no partial selection. Copy exports its entire text."},
      {"pending-record-outside-selection","Commit the pending line outside the selection before replacing source."},
      {"busy-modal","Close the active dialog before using the editor clipboard."},
      {"busy-document","Finish the active editor operation before using the clipboard."},
      {"wrong-target","Focus source, command or prefix text before using the editor clipboard."},
      {"improper-cursor","Move the caret into a source record before pasting."},
      {"document-closed","The source buffer was closed. Try the action in the current buffer."},
      {"record-unavailable","The current source record is unavailable. Move the caret and try again."},
      {"invalid-operand","The clipboard request was invalid. No source was changed."}
   };
   size_t i;
   for(i=0;i<sizeof(messages)/sizeof(messages[0]);i++) if(!strcmp(problem,messages[i].code)) return messages[i].message;
   return "The editor could not complete this clipboard action.";
}
void the_editor_clipboard_release(TheEditorClipboardCapture *out)
{ if(out) { free(out->bytes); memset(out,0,sizeof(*out)); } }
static int record(const void **owner,size_t *line,const unsigned char **base,size_t *base_length,
   const unsigned char **pending,size_t *length)
{ return the_workbench_document_record(owner,line,base,base_length,pending,length); }
static const unsigned char *logical_line(FILE_DETAILS *file,LINETYPE n,size_t *length)
{
   LINE *line; const void *owner; size_t r,b,l; const unsigned char *base,*pending;
   if(n<1 || n>file->number_lines) { *length=0; return (const unsigned char *)""; }
   line=lll_find(file->first_line,file->last_line,n,file->number_lines);
   if(!line || line->length<0) return NULL;
   if(record(&owner,&r,&base,&b,&pending,&l) && owner==file && r==(size_t)n)
   { *length=l; return pending; }
   *length=(size_t)line->length; return line->line;
}
static size_t cell_byte(const unsigned char *s,size_t n,LENGTHTYPE col)
{ return textpos_from_cell(s,n,col<1 ? 0 : (int)(col-1),TEXT_SNAP_BACKWARD).byte_offset; }
static unsigned char *field_bytes(int window,size_t *length)
{ *length=(size_t)(window==WINDOW_COMMAND ? cmd_rec_len : pre_rec_len); return window==WINDOW_COMMAND ? cmd_rec : pre_rec; }
static uint64_t field_hash(const unsigned char *bytes,size_t length)
{ uint64_t hash=UINT64_C(1469598103934665603); size_t i; for(i=0;i<length;i++) { hash^=bytes[i]; hash*=UINT64_C(1099511628211); } return hash; }
static int same_context(const Capture *c)
{
   VIEW_DETAILS *v=CURRENT_VIEW; size_t length=0; unsigned char *bytes=NULL;
   if(c->window!=WINDOW_FILEAREA) { bytes=field_bytes(c->window,&length);
      if(length!=c->field_length || field_hash(bytes,length)!=c->field_hash) return 0; }
   return v && v==c->view && CURRENT_FILE==c->file && v->current_window==c->window
      && v->focus_line==c->focus
      && v->logical_cursor.current.zone==c->cursor.current.zone
      && v->logical_cursor.current.line_number==c->cursor.current.line_number
      && v->logical_cursor.current.text.byte_offset==c->cursor.current.text.byte_offset
      && v->logical_cursor.current.text.cell_column==c->cursor.current.text.cell_column
      && v->logical_cursor.current.desired_cell==c->cursor.current.desired_cell
      && (c->window!=WINDOW_FILEAREA || (MARK_VIEW==v && (v->marked_line || v->marked_col))==c->selected)
      && (!c->selected || (v->mark_type==c->mark_type && v->marked_line==c->marked_line
         && v->marked_col==c->marked_col && v->mark_start_line==c->mark_start_line
         && v->mark_end_line==c->mark_end_line && v->mark_start_col==c->mark_start_col
         && v->mark_end_col==c->mark_end_col));
}
static short capture_context(Capture *c)
{
   VIEW_DETAILS *v=CURRENT_VIEW; short rc; size_t n,m; const unsigned char *s,*e;
   memset(c,0,sizeof(*c));
   if(!v || the_panel_active) return refuse("busy-modal",-3);
   if(the_workbench_activation_active()) return refuse("busy-document",-3);
   if(v->current_window!=WINDOW_FILEAREA && v->current_window!=WINDOW_COMMAND
      && v->current_window!=WINDOW_PREFIX) return refuse("wrong-target",5);
   c->view=v; c->file=CURRENT_FILE; c->window=v->current_window; c->focus=v->focus_line;
   c->cursor=v->logical_cursor;
   if((rc=the_edit_group_capture_token(the_editor_document_active(),&c->write)))
      return refuse(the_edit_group_error(),rc);
   if(c->window!=WINDOW_FILEAREA) {
      unsigned char *bytes=field_bytes(c->window,&c->field_length);
      c->field_hash=field_hash(bytes,c->field_length); return 0;
   }
   c->selected=MARK_VIEW==v && (v->marked_line || v->marked_col);
   c->mark_type=v->mark_type; c->marked_line=v->marked_line; c->marked_col=v->marked_col;
   c->mark_start_line=v->mark_start_line; c->mark_end_line=v->mark_end_line;
   c->mark_start_col=v->mark_start_col; c->mark_end_col=v->mark_end_col;
   if(c->selected) {
      LENGTHTYPE a=v->mark_start_col,b=v->mark_end_col;
      c->start=v->mark_start_line; c->end=v->mark_end_line;
      if(v->mark_type!=M_CUA && v->mark_type!=M_STREAM && v->mark_type!=M_LINE)
         return refuse("unsupported-selection",5);
      if(c->start>c->end || (c->start==c->end && a>b)) {
         LINETYPE t=c->start; LENGTHTYPE col=a; c->start=c->end; c->end=t; a=b; b=col;
      }
      if(c->start<1 || c->end>c->file->number_lines) return refuse("invalid-selection",5);
      s=logical_line(c->file,c->start,&n); e=logical_line(c->file,c->end,&m);
      if(!s || !e) return refuse("record-unavailable",-3);
      c->first_byte=v->mark_type==M_LINE ? 0 : cell_byte(s,n,a);
      c->last_byte=v->mark_type==M_LINE ? m : cell_byte(e,m,b);
      if(v->mark_type==M_STREAM && c->last_byte<m) {
         TextPos p=textpos_from_byte(e,m,c->last_byte);
         c->last_byte=textpos_cluster_at_boundary(e,m,p).end.byte_offset;
      }
   }
   if(!c->selected) {
      int cell=v->logical_cursor.current.valid && v->logical_cursor.current.zone==LOGICAL_CURSOR_ZONE_FILEAREA
         ? v->logical_cursor.current.text.cell_column : v->x[WINDOW_FILEAREA]+v->verify_col-1;
      if(!c->file->number_lines) { c->start=1; c->end=0; return 0; }
      if(c->focus<1 || c->focus>c->file->number_lines) return refuse("improper-cursor",5);
      c->start=c->end=c->focus;
      s=logical_line(c->file,c->start,&n); if(!s) return refuse("record-unavailable",-3);
      c->first_byte=c->last_byte=textpos_from_cell(s,n,cell,TEXT_SNAP_BACKWARD).byte_offset;
      if(cell>textpos_from_byte(s,n,n).cell_column)
         c->padding=(size_t)(cell-textpos_from_byte(s,n,n).cell_column);
   }
   return 0;
}
short the_editor_clipboard_capture(int copy,TheEditorClipboardCapture *out)
{
   Capture c; size_t total=0,n,a,b; LINETYPE line; unsigned char *bytes=NULL; short rc;
   llm_runtime_history_lookup(history_available);
   if(!out) return refuse("invalid-operand",5); memset(out,0,sizeof(*out));
   if((rc=capture_context(&c))) return rc;
   if(copy && c.window!=WINDOW_FILEAREA) {
      unsigned char *source=field_bytes(c.window,&total);
      if(total>THE_CLIPBOARD_BYTES_MAX) return refuse("limit-exceeded",94);
      bytes=malloc(total+1); if(!bytes) return refuse("limit-exceeded",94);
      memcpy(bytes,source,total); bytes[total]=0; c.field_whole=1;
   } else if(copy) {
      if(!c.selected) return refuse("no-selection",5);
      for(line=c.start;line<=c.end;line++) {
         const unsigned char *s=logical_line(c.file,line,&n);
         if(!s) return refuse("record-unavailable",-3);
         a=line==c.start ? c.first_byte : 0; b=line==c.end ? c.last_byte : n;
         if(b<a || b-a>THE_CLIPBOARD_BYTES_MAX-total) return refuse("limit-exceeded",94);
         total+=b-a;
         if(line<c.end || c.mark_type==M_LINE) { if(total==THE_CLIPBOARD_BYTES_MAX) return refuse("limit-exceeded",94); total++; }
      }
      bytes=malloc(total+1); if(!bytes) return refuse("limit-exceeded",94);
      total=0;
      for(line=c.start;line<=c.end;line++) {
         const unsigned char *s=logical_line(c.file,line,&n);
         a=line==c.start ? c.first_byte : 0; b=line==c.end ? c.last_byte : n;
         if(b>a) memcpy(bytes+total,s+a,b-a); total+=b-a;
         if(line<c.end || c.mark_type==M_LINE) bytes[total++]='\n';
      }
      bytes[total]=0;
   }
   if(bytes) { size_t i=0;
      while(i<total) { size_t unit=the_workbench_document_unit(bytes+i,total-i);
         if(!bytes[i] || (bytes[i]>=128 && unit==1)) { free(bytes); return refuse("invalid-utf8",5); }
         i+=unit;
      }
   }
   if(next_token==UINT64_MAX) { free(bytes); return refuse("limit-exceeded",94); }
   c.token=++next_token; captured=c;
   out->token=c.token; out->document_id=c.write.document_id; out->write_revision=c.write.write_revision;
   out->bytes=bytes; out->length=total; out->read_only=c.window==WINDOW_FILEAREA && ISREADONLY(c.file);
   out->whole_field=c.window!=WINDOW_FILEAREA; problem="ok"; return 0;
}
static short check_token(uint64_t token)
{
   TheDocumentWriteToken now; short rc;
   if(!token || captured.token!=token || !same_context(&captured)) return refuse("stale-revision",-3);
   if((rc=the_edit_group_token(captured.write.document_id,&now))) return refuse(the_edit_group_error(),rc);
   if(now.write_revision!=captured.write.write_revision) return refuse("stale-revision",-3);
   if(captured.window==WINDOW_FILEAREA && ISREADONLY(captured.file)) return refuse("read-only",12);
   return 0;
}
static void free_lines(LINE *first,LINE *last)
{
   LINE *line=first;
   while(line) { LINE *next=line==last ? NULL : line->next;
      if(line->first_name) ll_free(line->first_name,the_free);
      if(line->line) (*the_free)(line->line);
      if(line->name) (*the_free)(line->name);
      /* Original nodes retain their prefix command objects in the undo chain;
       * normal editor teardown owns the file's pending-prefix list. */
      (*the_free)(line); line=next;
   }
}
static void destroy(void *opaque)
{
   ClipboardEdit *p=opaque;
   if(!p) return;
   if(p->published) free_lines(p->before_first,p->before_last);
   else free_lines(p->after_first,p->after_last);
   free(p->old_record); free(p->old_base); free(p->new_base); free(p);
}
static short validate(void *opaque,int undo)
{
   ClipboardEdit *p=opaque; short rc;
   if(the_panel_active || the_workbench_activation_active()) return refuse("busy-modal",-3);
   if(the_workbench_document_file(p->context.write.document_id)!=p->context.file)
      return refuse("document-closed",-3);
   if(CURRENT_VIEW!=p->context.view || CURRENT_FILE!=p->context.file) return refuse("stale-revision",-3);
   if(ISREADONLY(CURRENT_FILE)) return refuse("read-only",12);
   if(!undo && (rc=check_token(p->context.token))) return rc;
   if(p->left->next!=(undo ? p->after_first : p->before_first)
      && !(undo ? p->after_count==0 : p->before_count==0)) return refuse("stale-revision",-3);
   return 0;
}
static unsigned short increment(unsigned short n) { return n==USHRT_MAX ? n : (unsigned short)(n+1); }
static void publish(void *opaque,int undo)
{
   ClipboardEdit *p=opaque; FILE_DETAILS *file=p->context.file; VIEW_DETAILS *v=p->context.view;
   LINE *first=undo ? p->before_first : p->after_first,*last=undo ? p->before_last : p->after_last;
   unsigned char **base=undo ? &p->old_base : &p->new_base;
   size_t *base_length=undo ? &p->old_base_length : &p->new_base_length;
   LINE *focus;
#ifdef USE_SDSLH
   sdslh_shutdown_file(file);
#endif
   p->left->next=first ? first : p->right; p->right->prev=last ? last : p->left;
   if(first) first->prev=p->left; if(last) last->next=p->right;
   file->number_lines=undo ? p->total_before : p->total_before-(LINETYPE)p->before_count+(LINETYPE)p->after_count;
   file->save_alt=undo ? p->save_alt : increment(p->save_alt);
   file->autosave_alt=undo ? p->autosave_alt : increment(p->autosave_alt);
   v->focus_line=undo ? p->old_focus : p->new_focus;
   v->current_line=undo ? p->old_current : p->new_focus;
   if(undo) { v->verify_col=p->old_verify; v->logical_cursor=p->old_cursor;
      v->marked_line=p->context.marked_line; v->marked_col=p->context.marked_col;
      v->mark_type=p->context.mark_type; v->mark_start_line=p->context.mark_start_line;
      v->mark_end_line=p->context.mark_end_line; v->mark_start_col=p->context.mark_start_col;
      v->mark_end_col=p->context.mark_end_col;
      if(p->context.selected) MARK_VIEW=v;
      if(p->old_record_length) memcpy(rec,p->old_record,p->old_record_length);
      rec_len=(LENGTHTYPE)p->old_record_length;
   } else {
      v->marked_line=v->marked_col=FALSE; if(MARK_VIEW==v) MARK_VIEW=NULL;
      focus=lll_find(file->first_line,file->last_line,v->focus_line,file->number_lines);
      rec_len=focus->length; if(rec_len) memcpy(rec,focus->line,(size_t)rec_len);
      logical_cursor_state_focus(&v->logical_cursor,logical_cursor_from_cell(LOGICAL_CURSOR_ZONE_FILEAREA,
         v->focus_line,v->current_row,rec,(size_t)rec_len,p->new_cell,TEXT_SNAP_BACKWARD,1));
   }
   memset(rec+rec_len,' ',(size_t)max_line_length-(size_t)rec_len);
   the_workbench_document_record_exchange(file,undo ? (size_t)p->old_record_line : (size_t)p->new_focus,base,base_length);
   p->published=!undo; the_workbench_document_changed();
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
   ClipboardEdit *p=opaque;
#ifdef THE_WASM_HIGHLIGHT
   the_wasm_highlight_forget(p->context.file);
#endif
#ifdef USE_SDSLH
   /* Recreate derived parser state only after complete byte publication. The
    * initializer reads the certified pending record as well as stored lines. */
   sdslh_init_file(p->context.file);
#endif
   receipt->parser_available=parser_available(p->context.write.document_id);
   if((undo && p->checkpoint_touched) || (p->context.file->autosave
      && p->context.file->autosave_alt>=p->context.file->autosave && p->context.file->autosave_fname)) {
      p->checkpoint_touched=1;
      receipt->autosave_rc=save_file(p->context.file,p->context.file->autosave_fname,TRUE,
         p->context.file->number_lines,1L,NULL,FALSE,0,max_line_length,TRUE,FALSE,TRUE);
      if(!receipt->autosave_rc) p->context.file->autosave_alt=0;
   }
   build_screen(current_screen);
   (void)execute_move_cursor(current_screen,CURRENT_VIEW,
      undo ? p->old_cursor.current.text.cell_column : p->new_cell);
   display_screen(current_screen); cursor_focus_refresh(current_screen,CURRENT_VIEW);
}
static int append_line(ClipboardEdit *p,const unsigned char *bytes,size_t length,LINE *model)
{
   LINE *line;
   if(p->after_count==THE_CLIPBOARD_LINES_MAX || length>(size_t)max_line_length
      || xedit_truncated_length(p->context.file,(CHARTYPE *)bytes,(LENGTHTYPE)length)<(LENGTHTYPE)length) return 0;
   line=(*the_malloc)(sizeof(*line)); if(!line) return 0; memset(line,0,sizeof(*line));
   line->line=(*the_malloc)(length+1); if(!line->line) { (*the_free)(line); return 0; }
   if(length) memcpy(line->line,bytes,length); line->line[length]=0; line->length=(LENGTHTYPE)length;
   if(model) { line->select=model->select; line->save_select=model->save_select;
      if(!p->after_count) line->flags=model->flags; else line->flags.new_flag=TRUE;
   } else line->flags.new_flag=TRUE;
   line->flags.changed_flag=TRUE; line->prev=p->after_last;
   if(p->after_last) p->after_last->next=line; else p->after_first=line;
   p->after_last=line; p->after_count++; return 1;
}
typedef struct {
   Capture context;
   unsigned char *before,*after;
   size_t before_length,after_length;
   LogicalCursorState cursor;
   int after_cell,old_verify,old_prefix_changed;
} FieldEdit;
static short field_validate(void *opaque,int undo)
{
   FieldEdit *p=opaque; size_t length; unsigned char *bytes;
   if(the_panel_active || !CURRENT_VIEW || CURRENT_VIEW!=p->context.view
      || CURRENT_FILE!=p->context.file || CURRENT_VIEW->current_window!=p->context.window
      || the_workbench_document_file(p->context.write.document_id)!=p->context.file) return refuse("stale-revision",-3);
   bytes=field_bytes(p->context.window,&length);
   if(length!=(undo ? p->after_length : p->before_length)
      || (length && memcmp(bytes,undo ? p->after : p->before,length))) return refuse("stale-revision",-3);
   return undo ? 0 : check_token(p->context.token);
}
static void field_publish(void *opaque,int undo)
{
   FieldEdit *p=opaque; VIEW_DETAILS *v=p->context.view;
   unsigned char *bytes=p->context.window==WINDOW_COMMAND ? cmd_rec : pre_rec;
   size_t length=undo ? p->before_length : p->after_length;
   size_t capacity=p->context.window==WINDOW_COMMAND ? (size_t)max_line_length : MAX_PREFIX_WIDTH;
   if(length) memcpy(bytes,undo ? p->before : p->after,length); memset(bytes+length,' ',capacity-length);
   if(p->context.window==WINDOW_COMMAND) { cmd_rec_len=(LENGTHTYPE)length; if(undo) cmd_verify_col=p->old_verify; }
   else { pre_rec_len=(LENGTHTYPE)length; prefix_changed=undo ? p->old_prefix_changed : TRUE; }
   if(undo) v->logical_cursor=p->cursor;
   else logical_cursor_state_focus(&v->logical_cursor,logical_cursor_from_cell(
      p->context.window==WINDOW_COMMAND ? LOGICAL_CURSOR_ZONE_COMMAND : LOGICAL_CURSOR_ZONE_PREFIX,
      p->context.window==WINDOW_COMMAND ? 0 : v->focus_line,
      p->cursor.current.zone_row,bytes,length,p->after_cell,TEXT_SNAP_BACKWARD,1));
   the_workbench_document_changed();
}
static void field_finish(void *opaque,int undo,TheEditGroupReceipt *receipt)
{
   FieldEdit *p=opaque; int cell=undo ? p->cursor.current.text.cell_column : p->after_cell;
   (void)receipt;
   if(p->context.window==WINDOW_COMMAND) { display_cmdline(current_screen,CURRENT_VIEW); execute_move_cursor(current_screen,CURRENT_VIEW,(LENGTHTYPE)cell); }
   else { display_prefix_line(current_screen,CURRENT_VIEW);
      the_driver->move_window_cursor(driver_current_window(),(short)CURRENT_VIEW->logical_cursor.current.zone_row,(short)cell); }
   cursor_focus_refresh(current_screen,CURRENT_VIEW);
}
static void field_destroy(void *opaque)
{ FieldEdit *p=opaque; if(p) { free(p->before); free(p->after); free(p); } }
static short replace_field(uint64_t token,const unsigned char *bytes,size_t length,int cut)
{
   static const TheEditGroupProvider provider={NULL,NULL,field_validate,field_publish,field_finish,field_destroy,parser_available,the_editor_clipboard_error,NULL};
   FieldEdit *p; TheEditGroupPlan *plan=NULL; short rc; unsigned char *current; size_t old_length,start,padding=0,capacity,i;
   int cell=captured.cursor.current.text.cell_column;
   if((rc=check_token(token))) return rc;
   for(i=0;i<length;i++) if(bytes[i]=='\r' || bytes[i]=='\n') return refuse("multiline-field",5);
   current=field_bytes(captured.window,&old_length);
   capacity=captured.window==WINDOW_COMMAND ? (size_t)max_line_length : (size_t)(CURRENT_VIEW->prefix_width-CURRENT_VIEW->prefix_gap);
   start=cut ? 0 : textpos_from_cell(current,old_length,cell,TEXT_SNAP_BACKWARD).byte_offset;
   if(!cut && cell>textpos_from_byte(current,old_length,old_length).cell_column)
      padding=(size_t)(cell-textpos_from_byte(current,old_length,old_length).cell_column);
   if(length>capacity || padding>capacity-length || (!cut && old_length>capacity-length-padding)) return refuse("limit-exceeded",94);
   p=calloc(1,sizeof(*p)); if(!p) return refuse("limit-exceeded",94);
   p->context=captured; p->before_length=old_length; p->after_length=cut ? 0 : old_length+padding+length;
   p->before=malloc(old_length+1); p->after=malloc(p->after_length+1);
   if(!p->before || !p->after) { field_destroy(p); return refuse("limit-exceeded",94); }
   memcpy(p->before,current,old_length);
   if(!cut) { memcpy(p->after,current,start); memset(p->after+start,' ',padding);
      memcpy(p->after+start+padding,bytes,length); memcpy(p->after+start+padding+length,current+start,old_length-start); }
   p->cursor=CURRENT_VIEW->logical_cursor; p->old_verify=cmd_verify_col; p->old_prefix_changed=prefix_changed;
   p->after_cell=cut ? 0 : textpos_from_byte(p->after,start+padding+length,start+padding+length).cell_column;
   rc=the_edit_group_prepare_owned(captured.write,&provider,p,1,&plan);
   if(rc) { field_destroy(p); return refuse(the_edit_group_error(),rc); }
   rc=the_edit_group_commit(plan,NULL);
   if(rc) { the_edit_group_abort(plan); return refuse(the_edit_group_error(),rc); }
   clipboard_group=the_edit_group_receipt()->group; journal_window=captured.window;
   { unsigned char *field=field_bytes(journal_window,&journal_field_length);
     journal_field_hash=field_hash(field,journal_field_length); }
   captured.token=0; problem="ok"; return 0;
}
static short replace(uint64_t token,const unsigned char *bytes,size_t length)
{
   static const TheEditGroupProvider provider={NULL,NULL,validate,publish,finish,destroy,parser_available,the_editor_clipboard_error,NULL};
   ClipboardEdit *p=NULL; TheEditGroupPlan *plan=NULL; Capture c; short rc; unsigned char *normalized=NULL,*combined=NULL;
   size_t i,j=0,n=0,m=0,prefix,suffix,total,start=0,old_base_length,pending_length,old_record_line;
   const unsigned char *first,*last,*base,*pending; const void *owner;
   LINE *model; size_t final_insert=0; LINETYPE insert_lines=0;
   int delete_records=0,cutting=!length;
   if((rc=check_token(token))) return rc;
   if((!bytes && length) || length>THE_CLIPBOARD_BYTES_MAX) return refuse("limit-exceeded",94);
   normalized=malloc(length+1); if(!normalized) return refuse("limit-exceeded",94);
   for(i=0;i<length;) {
      size_t unit=the_workbench_document_unit(bytes+i,length-i); unsigned char ch=bytes[i];
      if(!ch || (ch<32 && ch!='\t' && ch!='\r' && ch!='\n') || ch==127
         || (ch>=128 && unit==1)) { rc=refuse("invalid-utf8",5); goto done; }
      if(ch=='\r') { normalized[j++]='\n'; i++; if(i<length && bytes[i]=='\n') i++; }
      else { memcpy(normalized+j,bytes+i,unit); j+=unit; i+=unit; }
   }
   normalized[j]=0; length=j; c=captured;
   if(c.window!=WINDOW_FILEAREA) { rc=replace_field(token,normalized,length,0); goto done; }
   if(c.file->file_views>1) { rc=refuse("multiple-views",-3); goto done; }
   if(c.file->first_ppc || (prefix_changed && pre_rec_len)) { rc=refuse("pending-prefix",-3); goto done; }
   if(!c.selected && !length) { problem="ok"; rc=0; goto done; }
   if(!record(&owner,&old_record_line,&base,&old_base_length,&pending,&pending_length)
      || owner!=c.file || pending_length>(size_t)max_line_length) { rc=refuse("record-unavailable",-3); goto done; }
   if(old_record_line>=1 && old_record_line<=(size_t)c.file->number_lines
      && (old_record_line<(size_t)c.start || old_record_line>(size_t)c.end)
      && (pending_length!=old_base_length || (pending_length && memcmp(pending,base,pending_length)))) {
      rc=refuse("pending-record-outside-selection",-3); goto done;
   }
   p=calloc(1,sizeof(*p)); if(!p) { rc=refuse("limit-exceeded",94); goto done; }
   p->context=c; p->total_before=c.file->number_lines;
   p->save_alt=c.file->save_alt; p->autosave_alt=c.file->autosave_alt;
   p->old_focus=c.view->focus_line; p->old_current=c.view->current_line; p->old_cursor=c.view->logical_cursor;
   p->old_verify=c.view->verify_col; p->old_record_line=(LINETYPE)old_record_line;
   p->old_record_length=pending_length; p->old_base_length=old_base_length;
   p->old_record=malloc(pending_length+1); p->old_base=malloc(old_base_length+1);
   if(!p->old_record || !p->old_base) { rc=refuse("limit-exceeded",94); goto done; }
   memcpy(p->old_record,pending,pending_length); memcpy(p->old_base,base,old_base_length);
   first=logical_line(c.file,c.start,&n); last=c.end>=c.start ? logical_line(c.file,c.end,&m) : (const unsigned char *)"";
   prefix=c.first_byte; suffix=m-c.last_byte;
   if(c.selected && c.mark_type==M_LINE && length && normalized[length-1]=='\n') length--;
   delete_records=cutting && c.selected && (c.mark_type==M_LINE
      || (c.start==1 && c.end==c.file->number_lines && !prefix && !suffix));
   if(prefix+c.padding>THE_CLIPBOARD_BYTES_MAX || suffix>THE_CLIPBOARD_BYTES_MAX-prefix-c.padding
      || length>THE_CLIPBOARD_BYTES_MAX-prefix-c.padding-suffix) { rc=refuse("limit-exceeded",94); goto done; }
   total=prefix+c.padding+length+suffix;
   combined=malloc(total+1); if(!combined) { rc=refuse("limit-exceeded",94); goto done; }
   if(prefix) memcpy(combined,first,prefix); memset(combined+prefix,' ',c.padding);
   memcpy(combined+prefix+c.padding,normalized,length);
   if(suffix) memcpy(combined+prefix+c.padding+length,last+c.last_byte,suffix); combined[total]=0;
   p->before_count=c.end>=c.start ? (size_t)(c.end-c.start+1) : 0;
   p->before_first=p->before_count ? lll_find(c.file->first_line,c.file->last_line,c.start,c.file->number_lines) : NULL;
   p->before_last=p->before_count ? lll_find(c.file->first_line,c.file->last_line,c.end,c.file->number_lines) : NULL;
   p->left=p->before_count ? p->before_first->prev : c.file->first_line;
   p->right=p->before_count ? p->before_last->next : c.file->last_line;
   model=p->before_first;
   if(model) { LINE *line=model;
      for(;;) {
         if(line->pre || line->first_name || line->name) { rc=refuse("line-metadata",-3); goto done; }
         if(line==p->before_last) break; line=line->next;
      }
   }
   for(i=0;!delete_records && i<=total;i++) if(i==total || combined[i]=='\n') {
      if(!append_line(p,combined+start,i-start,model)) { rc=refuse("limit-exceeded",94); goto done; }
      start=i+1;
   }
   for(i=0;i<length;i++) { if(normalized[i]=='\n') { insert_lines++; final_insert=i+1; } }
   p->new_focus=c.start+insert_lines;
   if(insert_lines) p->new_cell=textpos_from_byte(normalized+final_insert,length-final_insert,length-final_insert).cell_column;
   else p->new_cell=textpos_from_byte(combined,prefix+c.padding+length,prefix+c.padding+length).cell_column;
   { LINE *focus=p->after_first; LINETYPE k;
      if(delete_records) {
         LINETYPE remaining=p->total_before-(LINETYPE)p->before_count;
         p->new_focus=remaining ? (c.start<=remaining ? c.start : remaining) : 1;
         p->new_cell=0;
         focus=remaining ? (c.start<=remaining ? p->right : p->left) : c.file->last_line;
      } else for(k=0;k<insert_lines;k++) focus=focus->next;
      p->new_base_length=(size_t)focus->length; p->new_base=malloc(p->new_base_length+1);
      if(!p->new_base) { rc=refuse("limit-exceeded",94); goto done; }
      memcpy(p->new_base,focus->line,p->new_base_length);
   }
   rc=the_edit_group_prepare_owned(c.write,&provider,p,p->before_count+p->after_count,&plan);
   if(rc) { problem=the_edit_group_error(); goto done; }
   p=NULL;
   rc=the_edit_group_commit(plan,NULL);
   if(rc) { problem=the_edit_group_error(); the_edit_group_abort(plan); }
   else { clipboard_group=the_edit_group_receipt()->group; journal_window=WINDOW_FILEAREA; captured.token=0; problem="ok"; }
done:
   if(p) destroy(p); free(normalized); free(combined); return rc;
}
short the_editor_clipboard_paste(uint64_t token,const unsigned char *bytes,size_t length)
{
   short rc;
   if(!length) { rc=check_token(token); if(!rc) problem="ok"; return rc; }
   return replace(token,bytes,length);
}
short the_editor_clipboard_cut(uint64_t token)
{
   if(captured.window!=WINDOW_FILEAREA) {
      if(!captured.field_whole) return refuse("no-selection",5);
      return replace_field(token,(const unsigned char *)"",0,1);
   }
   if(!captured.selected) return refuse("no-selection",5);
   return replace(token,(const unsigned char *)"",0);
}
short the_editor_clipboard_select_all(void)
{
   Capture c; short rc; size_t length; const unsigned char *last;
   if((rc=capture_context(&c))) return rc;
   if(c.window!=WINDOW_FILEAREA) return refuse("field-selection-unavailable",5);
   if(!c.file->number_lines) { problem="ok"; return 0; }
   last=logical_line(c.file,c.file->number_lines,&length);
   if(!last) return refuse("record-unavailable",-3);
   MARK_VIEW=c.view; c.view->mark_type=M_CUA; c.view->marked_line=c.view->marked_col=TRUE;
   c.view->mark_start_line=1; c.view->mark_start_col=1;
   c.view->mark_end_line=c.file->number_lines;
   c.view->mark_end_col=(LENGTHTYPE)(textpos_from_byte(last,length,length).cell_column+1);
   /* Selection changes invalidate captured destinations through same_context;
    * they do not change source bytes or discard the preceding Undo group. */
   the_workbench_document_changed();
   build_screen(current_screen); display_screen(current_screen); cursor_focus_refresh(current_screen,CURRENT_VIEW);
   problem="ok"; return 0;
}
static int history_available(void)
{
   TheEditGroupInfo info; size_t length; unsigned char *field;
   if(!CURRENT_VIEW) return 0;
   the_edit_group_info(&info);
   if(!info.undo_available || info.document_id!=the_editor_document_active()) return 0;
   if(CURRENT_VIEW->current_window==WINDOW_FILEAREA) return 1;
   if(info.group!=clipboard_group || journal_window!=CURRENT_VIEW->current_window) return 0;
   field=field_bytes(journal_window,&length);
   return length==journal_field_length && field_hash(field,length)==journal_field_hash;
}
short the_editor_clipboard_undo(int *handled)
{
   TheEditGroupInfo info; TheDocumentWriteToken token; short rc;
   if(handled) *handled=0;
   if(!CURRENT_VIEW || !history_available()) return 0;
   the_edit_group_info(&info);
   if(CURRENT_VIEW->current_window!=WINDOW_FILEAREA
      && (info.group!=clipboard_group || journal_window!=CURRENT_VIEW->current_window)) return 0;
   if(!info.undo_available || info.document_id!=the_editor_document_active()) return 0;
   if(handled) *handled=1;
   if((rc=the_edit_group_token(info.document_id,&token))) return refuse(the_edit_group_error(),rc);
   rc=the_edit_group_undo(info.group,token,NULL); problem=rc ? the_edit_group_error() : "ok"; return rc;
}
static char *json_string(char *out,const unsigned char *text,size_t length)
{
   static const char hex[]="0123456789abcdef"; size_t i;
   *out++='"';
   for(i=0;i<length;i++) { unsigned char c=text[i];
      if(c=='"' || c=='\\') { *out++='\\'; *out++=(char)c; }
      else if(c<32) { *out++='\\'; *out++='u'; *out++='0'; *out++='0'; *out++=hex[c>>4]; *out++=hex[c&15]; }
      else *out++=(char)c;
   }
   *out++='"'; return out;
}
char *the_editor_clipboard_json(const char *operation,const char *request_id,
   uint64_t token,const unsigned char *bytes,size_t length)
{
   TheEditorClipboardCapture out; short rc=5; int exported=0,handled=0; char *json,*p;
   const char *message; size_t capacity=4096,payload=0,strings;
   memset(&out,0,sizeof(out));
   if(!operation || !request_id || strlen(operation)>24 || strlen(request_id)>128) {
      operation="invalid"; request_id="";
   }
   /* Reserve every byte of the response before any source mutation. Copy's
    * export has a known bound; other operations return only fixed metadata. */
   if(!strcmp(operation,"copy")) payload=THE_CLIPBOARD_BYTES_MAX;
   strings=strlen(operation)+strlen(request_id);
   if(payload>SIZE_MAX-strings || payload+strings>(SIZE_MAX-capacity)/6) return NULL;
   capacity+=(payload+strings)*6;
   json=malloc(capacity); if(!json) return NULL;
   if(!strcmp(operation,"capture") || !strcmp(operation,"copy")) {
      rc=the_editor_clipboard_capture(!strcmp(operation,"copy"),&out); exported=!rc;
   } else if(!strcmp(operation,"paste")) rc=the_editor_clipboard_paste(token,bytes,length);
   else if(!strcmp(operation,"cut")) rc=the_editor_clipboard_cut(token);
   else if(!strcmp(operation,"selectAll")) rc=the_editor_clipboard_select_all();
   else if(!strcmp(operation,"undo")) {
      rc=the_editor_clipboard_undo(&handled);
      if(!handled) { rc=Sos_undo((CHARTYPE *)""); problem=rc ? "undo-unavailable" : "ok"; }
   } else problem="invalid-operand";
   message=rc ? the_editor_clipboard_error_message() : "Completed.";
   p=json; p+=sprintf(p,"{\"type\":\"clipboard\",\"operation\":");
   p=json_string(p,(const unsigned char *)operation,strlen(operation));
   p+=sprintf(p,",\"requestId\":"); p=json_string(p,(const unsigned char *)request_id,strlen(request_id));
   p+=sprintf(p,",\"rc\":%d,\"code\":",(int)rc);
   p=json_string(p,(const unsigned char *)(rc ? problem : "ok"),strlen(rc ? problem : "ok"));
   p+=sprintf(p,",\"message\":"); p=json_string(p,(const unsigned char *)message,strlen(message));
   if(exported) {
      p+=sprintf(p,",\"token\":\"%" PRIu64 "\",\"documentId\":\"%" PRIu64 "\",\"writeRevision\":\"%" PRIu64 "\",\"readOnly\":%s",
         out.token,out.document_id,out.write_revision,out.read_only ? "true" : "false");
      p+=sprintf(p,",\"selectionScope\":\"%s\"",out.whole_field ? "field" : "selection");
      if(!strcmp(operation,"copy")) { p+=sprintf(p,",\"text\":"); p=json_string(p,out.bytes,out.length); }
   }
   *p++='}'; *p=0; the_editor_clipboard_release(&out); return json;
}
