/* XEDIT-style READ panels, shared by terminal, web and protocol drivers. */
#include "the.h"
#include "proto.h"
#include "vars.h"
#include "panel.h"
#include "panelui.h"
#include "textpos.h"
#include "thedriver.h"
#include "driverwindow.h"
#include "thekeys.h"
#include "llm/llmruntime.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

static int append_fields(ThePanelState *p, const CHARTYPE *raw, size_t length,
                           int row, int cell, int width, int markup,
                           int editable, char kind, LINETYPE line)
{
   size_t pos=0, start=0;
   int protect=(kind=='R' || markup), col=0;
   while (pos<=length && col<width)
   {
      int attr=-1, i;
      if (markup && pos+1<length && raw[pos]==ctlchar_escape)
         for (i=0;i<MAX_CTLCHARS;i++)
            if (ctlchar_char[i] && ctlchar_char[i]==raw[pos+1]) { attr=i; break; }
      if (pos==length || attr>=0)
      {
         size_t n=pos-start;
         int span=textpos_from_byte(raw+start,n,n).cell_column;
         if (span>width-col)
         {
            TextPos end=textpos_from_cell(raw+start,n,width-col,TEXT_SNAP_BACKWARD);
            n=end.byte_offset; span=end.cell_column;
         }
         if (pos==length && !protect) span=width-col;
         if (span && !panel_ui_add(p,row,cell+col,span,editable && !protect,kind,line,(const char *)raw+start,n,start)) return 0;
         col+=span;
         if (pos==length) break;
         protect=ctlchar_protect[attr];
         pos+=2; start=pos;
      }
      else pos++;
   }
   return 1;
}

static int build_panel(ThePanelState *p, int mode)
{
   int row, prefix=CURRENT_VIEW->prefix ? CURRENT_VIEW->prefix_width : 0;
   int cols=CURRENT_SCREEN.cols[WINDOW_FILEAREA];
   panel_ui_init(p,CURRENT_SCREEN.rows[WINDOW_FILEAREA]+1,cols+prefix);
   for (row=0;row<p->rows-1;row++)
   {
      SHOW_LINE *s=&CURRENT_SCREEN.sl[row];
      RESERVED *r=find_reserved_line(current_screen,TRUE,row,0,0);
      if (r)
      {
         if (!append_fields(p,r->line,r->length,row,prefix,cols,CTLCHARx,mode!=0,'R',0)) return 0;
      }
      else if (s->line_type==LINE_LINE && s->line_number>0 && s->line_number<=CURRENT_FILE->number_lines)
      {
         LINE *l=lll_find(CURRENT_FILE->first_line,CURRENT_FILE->last_line,s->line_number,CURRENT_FILE->number_lines);
         size_t begin=0, end=l->length;
         int oldcount=p->count, i;
         if (prefix && !panel_ui_add(p,row,0,prefix,mode!=0,'P',s->line_number,"",0,0)) return 0;
         if (!CURRENT_FILE->filectlchar)
         {
            begin=textpos_from_cell(l->line,l->length,(int)CURRENT_VIEW->verify_col-1,TEXT_SNAP_BACKWARD).byte_offset;
            end=begin+textpos_from_cell(l->line+begin,l->length-begin,cols,TEXT_SNAP_BACKWARD).byte_offset;
         }
         if (!append_fields(p,l->line+begin,end-begin,row,prefix,cols,CURRENT_FILE->filectlchar && CTLCHARx,
                             mode!=0 && !ISREADONLY(CURRENT_FILE),'F',s->line_number)) return 0;
         for (i=oldcount;i<p->count;i++) if (p->fields[i].kind=='F') p->fields[i].source_start+=begin;
      }
   }
   return panel_ui_add(p,p->rows-1,0,p->cols,1,'C',0,"",0,0);
}

static void publish_panel(void)
{
   char *out=malloc(1024*1024);
   LlmDriverFormatOptions options;
   if (!out) return;
   llm_driver_format_options_init(&options);
   options.compact=1;
   llm_runtime_format_screen(current_screen,&options,out,1024*1024);
   puts(out); fflush(stdout); free(out);
}

/* A synchronous macro stays on its stack while this small protocol input loop
 * feeds the same field model. Editor/OS commands cannot run during READ. */
static void protocol_read(ThePanelState *p)
{
   char input[8192];
   publish_panel();
   while (!p->done && fgets(input,sizeof(input),stdin))
   {
      TheInputEvent event;
      input[strcspn(input,"\r\n")]=0;
      if (!strncmp(input,"text ",5) || !strncmp(input,"type ",5))
      {
         const CHARTYPE *s=(CHARTYPE *)input+5;
         size_t len=strlen((const char *)s);
         TextPos pos=textpos_begin();
         while (pos.byte_offset<len)
         {
            TextCodepoint cp=textpos_codepoint_at(s,len,pos);
            if (!cp.byte_length) break;
            the_input_event_from_text(cp.codepoint,&event); panel_ui_event(p,&event);
            pos=textpos_next_codepoint(s,len,pos);
         }
      }
      else if (!strncmp(input,"key ",4))
      {
         if (the_input_event_from_key_name(input+4,&event)) panel_ui_event(p,&event);
      }
      else if (!strncmp(input,"hit ",4))
      { int row,cell; if (sscanf(input+4,"%d %d",&row,&cell)==2) panel_ui_hit(p,row,cell); }
      else if (!strcmp(input,"cancel") || !strcmp(input,"quit")) p->done=p->cancelled=1;
      /* look/delta and rejected commands return current state without effects. */
      if (!p->done) publish_panel();
   }
   if (!p->done) p->done=p->cancelled=1;
}

static void driver_read(ThePanelState *p)
{
   TheDriverWindow *window=NULL;
   int curses=the_driver_is_curses(), i;
   if (curses)
   {
      TheDriverWindowOrigin origin=the_driver->window_origin(driver_current_role_window(WINDOW_FILEAREA));
      int prefix=CURRENT_VIEW->prefix ? CURRENT_VIEW->prefix_width : 0;
      window=the_driver->create_window(p->rows,p->cols,origin.row,origin.col-prefix);
      if (!window) { p->done=p->cancelled=1; return; }
   }
   while (!p->done)
   {
      TheInputEvent event;
      if (window)
      {
         for (i=0;i<p->rows;i++) the_driver->clear_line_at(window,i,set_colour(CURRENT_FILE->attr+ATTR_RESERVED));
         for (i=0;i<p->count;i++)
         {
            ThePanelField *f=&p->fields[i];
            the_driver->set_window_attr(window,set_colour(CURRENT_FILE->attr+(f->editable ? ATTR_CMDLINE : ATTR_RESERVED)));
            the_driver->add_string_at(window,f->row,f->cell,f->text);
         }
         if (p->active>=0)
         {
            ThePanelField *f=&p->fields[p->active];
            the_driver->move_window_cursor(window,f->row,f->cell+(p->cursor<f->width?p->cursor:f->width-1));
         }
         the_driver->refresh_window_now(window);
      }
      else the_driver->update();
      if (!the_driver->read_input_event(&event))
      {
         /* Browser polls may time out; a disconnected/closing driver cancels
          * through Escape or the normal shutdown event. */
         p->done=p->cancelled=1;
         continue;
      }
      if (event.kind==THE_INPUT_KEY && the_input_legacy_key_is_mouse(event.key_code))
      {
         TheDriverMouseEvent mouse;
         if (window && the_driver_read_transient_mouse_event(window,&mouse) && mouse.inside)
            panel_ui_hit(p,mouse.row,mouse.col);
      }
      else panel_ui_event(p,&event);
   }
   if (window) { the_driver->delete_window(window); the_driver->redraw_window(driver_current_role_window(WINDOW_FILEAREA)); }
}

static size_t trimmed_length(const char *text)
{
   size_t n=strlen(text);
   while (n && text[n-1]==' ') n--;
   return n;
}

static short results(ThePanelState *p, int mode, int tag, int number)
{
   char **records=calloc((size_t)p->count+1,sizeof(char *));
   char **updates=calloc((size_t)p->count,sizeof(char *));
   int count=0, i, j;
   char buffer[THE_PANEL_TEXT+100], key[THE_PANEL_TEXT+80];
   const char *definition=(const char *)get_key_definition(p->key,THE_KEY_DEFINE_RAW,TRUE,FALSE);
   if (!definition) definition="";
   short rc=RC_OK;
   if (!records || !updates) { rc=RC_OUT_OF_MEMORY; goto done; }
   if (p->key>=THE_KEY_F(1) && p->key<=THE_KEY_F(24)) snprintf(key,sizeof(key),"PFK %d %s",p->key-THE_KEY_F0,definition);
   else if (p->key>=THE_KEY_PA1 && p->key<=THE_KEY_PA3) snprintf(key,sizeof(key),"PAK %d %s",p->key-THE_KEY_PA1+1,definition);
   else snprintf(key,sizeof(key),"ETK %s",definition);
   records[count++]=(char *)my_strdup((CHARTYPE *)(tag?key:definition));
   for (i=0;i<p->count;i++)
   {
      ThePanelField *f=&p->fields[i]; size_t length=trimmed_length(f->text);
      int offset=0;
      if (!f->editable || !f->changed) continue;
      if (tag)
      {
         if (f->kind=='C') offset=snprintf(buffer,sizeof(buffer),"CMD ");
         else offset=snprintf(buffer,sizeof(buffer),"%s %d %d ",f->kind=='R'?"RES":f->kind=='P'?"PRF":"FIL",f->row+1,f->cell+1);
         if (number && (f->kind=='F' || f->kind=='P')) offset+=snprintf(buffer+offset,sizeof(buffer)-(size_t)offset,"%ld ",(long)f->line);
      }
      else if (number && f->kind=='F') offset=snprintf(buffer,sizeof(buffer),"%ld ",(long)f->line);
      memcpy(buffer+offset,f->text,length); buffer[offset+length]=0;
      records[count++]=(char *)my_strdup((CHARTYPE *)buffer);
      if (mode==1 && f->kind=='F')
      {
         LINE *l=lll_find(CURRENT_FILE->first_line,CURRENT_FILE->last_line,f->line,CURRENT_FILE->number_lines);
         size_t used=0, source=0;
         char *out=(*the_malloc)(max_line_length+1);
         if (!out) { rc=RC_OUT_OF_MEMORY; goto done; }
         /* Stage each line once, retaining CTLCHAR bytes and offscreen text. */
         for (j=0;j<i;j++) if (updates[j] && p->fields[j].line==f->line) break;
         if (j<i) { (*the_free)(out); continue; }
         for (j=i;j<p->count;j++)
         {
            ThePanelField *g=&p->fields[j]; size_t n;
            if (g->kind!='F' || g->line!=f->line || !g->changed) continue;
            n=g->source_start-source;
            if (used+n>max_line_length) { (*the_free)(out); rc=RC_INVALID_OPERAND; goto done; }
            memcpy(out+used,l->line+source,n); used+=n;
            n=(g->source_start+g->source_length<l->length)?strlen(g->text):trimmed_length(g->text);
            if (used+n>max_line_length) { (*the_free)(out); rc=RC_INVALID_OPERAND; goto done; }
            memcpy(out+used,g->text,n); used+=n;
            source=g->source_start+g->source_length;
         }
         if (used+l->length-source>max_line_length) { (*the_free)(out); rc=RC_INVALID_OPERAND; goto done; }
         memcpy(out+used,l->line+source,l->length-source); used+=l->length-source; out[used]=0;
         if (used>max_line_length || xedit_truncated_length(CURRENT_FILE,(CHARTYPE *)out,used)<used)
         { (*the_free)(out); rc=RC_INVALID_OPERAND; goto done; }
         updates[i]=out;
      }
   }
   for (i=0;i<count;i++) if (!records[i]) { rc=RC_OUT_OF_MEMORY; goto done; }
   for (i=0;i<p->count;i++) if (updates[i])
   {
      ThePanelField *f=&p->fields[i];
      LINE *l=lll_find(CURRENT_FILE->first_line,CURRENT_FILE->last_line,f->line,CURRENT_FILE->number_lines);
      /* Preallocated replacement makes the multi-line commit allocation-free. */
      add_to_recovery_list(l->line,l->length);
      (*the_free)(l->line); l->line=(CHARTYPE *)updates[i]; updates[i]=NULL;
      l->length=strlen((const char *)l->line); l->flags.changed_flag=TRUE;
      increment_alt(CURRENT_FILE);
#ifdef USE_SDSLH
      if (CURRENT_FILE->sdslh_comm && CURRENT_FILE->cb) cb_sync_line(CURRENT_FILE->cb,f->line-1,(const char *)l->line);
#endif
   }
   for (i=0;i<p->count;i++) if (mode==1 && p->fields[i].kind=='P' && p->fields[i].changed)
   {
      char command[THE_PANEL_TEXT+80]; ThePanelField *f=&p->fields[i];
      snprintf(command,sizeof(command),":%ld set pending on %.*s",(long)f->line,(int)trimmed_length(f->text),f->text);
      command_line((CHARTYPE *)command,COMMAND_ONLY_FALSE);
   }
   snprintf(buffer,sizeof(buffer),"%d",count);
   if (in_macro) rc=set_rexx_variable((CHARTYPE *)"READ",(CHARTYPE *)buffer,strlen(buffer),0);
   for (i=0;i<count && rc==RC_OK;i++)
      if (in_macro) rc=set_rexx_variable((CHARTYPE *)"READ",(CHARTYPE *)records[i],strlen(records[i]),i+1);
   if (in_macro && rc==RC_OK)
      for (i=count-1;i>=0 && rc==RC_OK;i--) rc=the_read_queue_push(records[i]);
done:
   if (records) { for (i=0;i<count;i++) if (records[i]) (*the_free)(records[i]); free(records); }
   if (updates) { for (i=0;i<p->count;i++) if (updates[i]) (*the_free)(updates[i]); free(updates); }
   return rc;
}

short THERead(CHARTYPE *params)
{
   char copy[200], *word; int mode=0, tag=0, number=0;
   ThePanelState *p; short rc; LogicalCursor cursor;
   LINETYPE old_focus, old_current; CHARTYPE old_window;
   if (in_prefix_macro || the_panel_active) return 4;
   if (strlen((char *)params)>=sizeof(copy)) return RC_INVALID_OPERAND;
   strcpy(copy,(char *)params);
   for (word=strtok(copy," \t");word;word=strtok(NULL," \t"))
   {
      char *s; for (s=word;*s;s++) *s=(char)toupper((unsigned char)*s);
      if (!strcmp(word,"ALL")) mode=1;
      else if (!strcmp(word,"NOCHANGE")) mode=2;
      else if (!strcmp(word,"CMDLINE")) mode=0;
      else if (!strcmp(word,"TAG")) tag=1;
      else if (!strcmp(word,"NOTAG")) tag=0;
      else if (!strcmp(word,"NUMBER")) number=1;
      else return RC_INVALID_OPERAND;
   }
   if (in_macro && the_read_queue_depth()!=0) return 8;
   if (CURRENT_FILE->filectlchar && CURRENT_VIEW->verify_col!=1) return RC_INVALID_ENVIRON;
   post_process_line(CURRENT_VIEW,CURRENT_VIEW->focus_line,NULL,TRUE);
   build_screen(current_screen);
   p=malloc(sizeof(*p)); if (!p) return RC_OUT_OF_MEMORY;
   if (!build_panel(p,mode)) { free(p); return RC_INVALID_OPERAND; }
   cursor=CURRENT_VIEW->logical_cursor.current;
   old_focus=CURRENT_VIEW->focus_line; old_current=CURRENT_VIEW->current_line; old_window=CURRENT_VIEW->current_window;
   the_panel_active=p;
   if (!the_driver_is_curses() && strncmp(the_driver_ui_version(),"web/",4)!=0) protocol_read(p);
   else driver_read(p);
   rc=p->cancelled ? 4 : results(p,mode,tag,number);
   if (p->cancelled && in_macro) set_rexx_variable((CHARTYPE *)"READ",(CHARTYPE *)"0",1,0);
   the_panel_active=NULL; free(p);
   CURRENT_VIEW->logical_cursor.current=cursor;
   CURRENT_VIEW->focus_line=old_focus; CURRENT_VIEW->current_line=old_current; CURRENT_VIEW->current_window=old_window;
   pre_process_line(CURRENT_VIEW,CURRENT_VIEW->focus_line,NULL);
   build_screen(current_screen); display_screen(current_screen);
   if (the_driver_is_headless()) the_driver->update();
   return rc;
}
