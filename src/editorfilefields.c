#include "the.h"
#include "proto.h"
#include "vars.h"
#include "workbenchfilefields.h"
#include "workbenchdocument.h"
#include "editorgroup.h"
#include <string.h>
static const char *problem="ok";
static short config(uint64_t id,TheWorkbenchFileFieldsConfig *out) {
   FILE_DETAILS *file=(FILE_DETAILS *)the_workbench_document_file(id); int i;
   if(!file) { problem="document-closed"; return -3; }
   memset(out,0,sizeof(*out)); out->enabled=file->filectlchar && CTLCHARx; out->escape=ctlchar_escape;
   out->width=(size_t)max_line_length; out->truncation=(size_t)file->truncation_column; out->readonly=ISREADONLY(file);
   for(i=0;i<MAX_CTLCHARS;i++) if(ctlchar_char[i]) { out->tags[(unsigned char)ctlchar_char[i]]=1; out->editable[(unsigned char)ctlchar_char[i]]=!ctlchar_protect[i]; }
   problem="ok"; return 0;
}
static short info(uint64_t id,size_t *records) {
   FILE_DETAILS *file=(FILE_DETAILS *)the_workbench_document_file(id);
   if(!file || file->number_lines<0) { problem="document-closed"; return -3; }
   *records=(size_t)file->number_lines; return 0;
}
static short line(uint64_t id,size_t number,const unsigned char **bytes,size_t *length) {
   FILE_DETAILS *file=(FILE_DETAILS *)the_workbench_document_file(id); const void *owner; size_t pending_line,base_length,pending_length; const unsigned char *base,*pending; LINE *l;
   if(!file || !number || number>(size_t)file->number_lines) { problem="document-closed"; return -3; }
   if(!the_workbench_document_record(&owner,&pending_line,&base,&base_length,&pending,&pending_length)) { problem="record-unavailable"; return -3; }
   if(owner==file && pending_line==number) { *bytes=pending; *length=pending_length; return 0; }
   l=lll_find(file->first_line,file->last_line,(LINETYPE)number,file->number_lines);
   if(!l || l->length<0) { problem="record-unavailable"; return -3; }
   *bytes=l->line; *length=(size_t)l->length; return 0;
}
static const char *error(void) { return problem; }
void the_editor_filefields_initialize(void) {
   static const TheWorkbenchFileFieldsProvider p={config,info,line,error};
   the_editor_edit_group_initialize(); the_workbench_filefields_provider(&p);
}

short the_editor_filefields_close(int all_buffers) {
   VIEW_DETAILS *view; short rc;
   if(!CURRENT_VIEW) return 0;
   the_editor_filefields_initialize();
   if(!all_buffers) rc=the_workbench_filefields_close(the_workbench_document_identify(CURRENT_FILE));
   else {
      rc=0;
      for(view=vd_first;view;view=view->next)
         if((rc=the_workbench_filefields_close(the_workbench_document_identify(view->file_for_view)))) break;
   }
   if(rc) display_error(0,(CHARTYPE *)the_workbench_last_result()->message,FALSE);
   return rc;
}
