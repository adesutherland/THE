#include "the.h"
#include "proto.h"
#include "panelui.h"
#include "editordocument.h"
#include "editorgroup.h"
#include "workbenchfilefields.h"

#include <ctype.h>
#include <string.h>

static int document_info(const void *opaque,size_t *lines,int *dirty)
{
   const FILE_DETAILS *file=opaque;
   if (!file || file->number_lines<0) return 0;
   *lines=(size_t)file->number_lines; *dirty=file->save_alt!=0; return 1;
}
static int document_line(const void *opaque,size_t number,const unsigned char **bytes,size_t *length)
{
   const FILE_DETAILS *file=opaque; LINE *line;
   if (!file || !number || number>(size_t)file->number_lines) return 0;
   line=lll_find(file->first_line,file->last_line,(LINETYPE)number,file->number_lines);
   if (!line || line->length<0) return 0;
   *bytes=line->line; *length=(size_t)line->length; return 1;
}
static int document_pending(const unsigned char **bytes,size_t *length)
{ if (!rec || rec_len<0) return 0; *bytes=rec; *length=(size_t)rec_len; return 1; }
static int document_blocked(void)
{ return in_macro || the_panel_active; }
static int document_no_pending(const void *opaque)
{
   const FILE_DETAILS *file=opaque; const LINE *boundary;
   if (!CURRENT_VIEW || CURRENT_FILE!=file || !rec || rec_len<0
   || (CURRENT_VIEW->focus_line>=1 && CURRENT_VIEW->focus_line<=file->number_lines)) return 0;
   boundary=CURRENT_VIEW->focus_line<1 ? file->first_line : file->last_line;
   return boundary && rec_len==boundary->length && (!rec_len || !memcmp(rec,boundary->line,(size_t)rec_len));
}
static int document_owns_list(const void *opaque,const void *first)
{ return ((const FILE_DETAILS *)opaque)->first_line==first; }
static void initialize(void)
{
   static const TheWorkbenchDocumentProvider provider={document_info,document_line,document_pending,document_blocked,document_no_pending,document_owns_list};
   the_workbench_document_provider(&provider);
   the_editor_edit_group_initialize();
   the_editor_filefields_initialize();
}
uint64_t the_editor_document_active(void)
{ initialize(); return CURRENT_VIEW ? the_workbench_document_identify(CURRENT_FILE) : 0; }
void the_editor_document_record_bind(FILE_DETAILS *file,LINETYPE number,LINE *line)
{
   initialize();
   if (!file || !line || number<0 || line->length<0) the_workbench_document_record_unknown();
   else the_workbench_document_record_bind(file,(size_t)number,line->line,(size_t)line->length);
}
static int word(const char **cursor,const char *expected,size_t minimum)
{
   const unsigned char *p=(const unsigned char *)*cursor,*start;
   size_t n=0;
   while (*p==' ' || *p=='\t') p++;
   start=p; while (*p && *p!=' ' && *p!='\t') { p++; n++; }
   if (n<minimum || n>strlen(expected)) return 0;
   while (start<p) if (toupper(*start++) != *expected++) return 0;
   *cursor=(const char *)p; return 1;
}
int the_editor_document_inspection_command(const char *command)
{
   const char *p=command ? command : "",*q=p;
   int linend=number_of_files ? CURRENT_VIEW->linend_status : LINEND_STATUSx;
   CHARTYPE delimiter=number_of_files ? CURRENT_VIEW->linend_value : LINEND_VALUEx;
   if (linend && strchr(p,delimiter)) return 0;
   if(the_editor_edit_group_command(p)) return 1;
   q=p;
   if(word(&q,"SDSLHWAIT",9)) return 1;
   q=p;
   if(word(&q,"SOS",3) && word(&q,"UNDO",4)) { while(*q==' ' || *q=='\t') q++; if(!*q) return 1; }
   q=p;
   if(word(&q,"WORKBENCH",9)) {
      const char *form=q,*operation=q;
      if(word(&operation,"FILE",4) && word(&operation,"INSPECT",7)) return 1;
      operation=q;
      if(word(&operation,"PANEL",5) && (word(&operation,"SET",3) || word(&operation,"APPLY",5) || word(&operation,"CANCEL",6) || word(&operation,"RELOAD",6) || word(&operation,"SHOW",4) || word(&operation,"HIDE",4) || word(&operation,"FOCUS",5))) return 1;
      if(word(&q,"FIELD",5) && word(&q,"SET",3)) return 1;
      q=form;
      if(word(&q,"FORM",4) && (word(&q,"SUBMIT",6) || word(&q,"CANCEL",6))) return 1;
   }
   q=p;
   if (word(&q,"QUERY",1)) return 1;
   q=p;
   return word(&q,"EXTRACT",3);
}
int the_editor_edit_group_command(const char *command)
{
   const char *p=command ? command : "";
   if(!word(&p,"WORKBENCH",9) || !word(&p,"EDIT",4)) return 0;
   return word(&p,"APPLY",5) || word(&p,"UNDO",4);
}
