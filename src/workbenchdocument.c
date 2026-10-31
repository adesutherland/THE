#include "workbenchdocument.h"

#include <stdlib.h>
#include <string.h>

typedef struct Document {
   const void *file;
   uint64_t id;
   struct Document *next;
} Document;
static Document *documents;
static TheWorkbenchDocumentProvider provider;
static uint64_t next_id, revision=1;
static size_t executing;
static int exhausted, record_known;
static const void *record_file;
static size_t record_line, base_length;
static unsigned char *base;

void the_workbench_document_provider(const TheWorkbenchDocumentProvider *value)
{ if (value) provider=*value; else memset(&provider,0,sizeof(provider)); }
void the_workbench_document_changed(void)
{ if (revision == UINT64_MAX) exhausted=1; else revision++; }
void the_workbench_document_enter(void)
{ the_workbench_document_changed(); executing++; }
void the_workbench_document_leave(void)
{ the_workbench_document_changed(); if (executing) executing--; }
void the_workbench_document_resolving(int entering)
{ if (entering) executing++; else if (executing) executing--; }
void the_workbench_document_record_unknown(void)
{ record_known=0; record_file=NULL; record_line=0; }
void the_workbench_document_structure(const void *first)
{
   Document *doc; int changed=0;
   if (!first || !provider.owns_list) return;
   if (record_known && provider.owns_list(record_file,first))
   { the_workbench_document_record_unknown(); changed=1; }
   for (doc=documents;doc;doc=doc->next) if (provider.owns_list(doc->file,first)) { changed=1; break; }
   if (changed) the_workbench_document_changed();
}
void the_workbench_document_record_bind(const void *file,size_t line,const void *bytes,size_t length)
{
   unsigned char *copy;
   the_workbench_document_record_unknown();
   if (!file || (!bytes && length) || length>THE_WORKBENCH_DOCUMENT_RECORD_MAX) return;
   copy=realloc(base,length ? length : 1);
   if (!copy) return;
   base=copy; if (length) memcpy(base,bytes,length);
   base_length=length; record_file=file; record_line=line; record_known=1;
}
uint64_t the_workbench_document_identify(const void *file)
{
   Document *doc;
   if (!file || !provider.info || !provider.line) return 0;
   for (doc=documents;doc;doc=doc->next) if (doc->file==file) return doc->id;
   if (next_id==UINT64_MAX || !(doc=calloc(1,sizeof(*doc)))) return 0;
   doc->file=file; doc->id=++next_id; doc->next=documents; documents=doc;
   return doc->id;
}
void the_workbench_document_forget(const void *file)
{
   Document **link=&documents,*doc;
   if (record_file==file) the_workbench_document_record_unknown();
   while ((doc=*link)!=NULL)
   {
      if (doc->file!=file) { link=&doc->next; continue; }
      *link=doc->next; free(doc); the_workbench_document_changed(); return;
   }
}
void the_workbench_document_reset(void)
{
   while (documents) { Document *doc=documents; documents=doc->next; free(doc); }
   free(base); base=NULL; base_length=0; executing=0;
   the_workbench_document_record_unknown(); the_workbench_document_changed();
}
static Document *find(uint64_t id)
{ Document *doc; for (doc=documents;doc;doc=doc->next) if (doc->id==id) return doc; return NULL; }

/* A valid scalar is indivisible; malformed octets remain individually
 * addressable raw bytes. No inspection repairs the editor's stored data. */
size_t the_workbench_document_unit(const unsigned char *s,size_t length)
{
   size_t n,i; unsigned c;
   if (!length || s[0]<0x80) return length ? 1 : 0;
   if (s[0]>=0xc2 && s[0]<=0xdf) { n=2; c=s[0]&31; }
   else if (s[0]>=0xe0 && s[0]<=0xef) { n=3; c=s[0]&15; }
   else if (s[0]>=0xf0 && s[0]<=0xf4) { n=4; c=s[0]&7; }
   else return 1;
   if (length<n) return 1;
   for(i=1;i<n;i++) { if ((s[i]&0xc0)!=0x80) return 1; c=(c<<6)|(s[i]&63); }
   if ((n==3 && c<0x800) || (n==4 && c<0x10000) || c>0x10ffff || (c>=0xd800 && c<=0xdfff)) return 1;
   return n;
}
static int pending_base_matches(const void *file,size_t lines)
{
   const unsigned char *bytes; size_t length;
   if (file!=record_file || !record_line || record_line>lines) return 1;
   return provider.line(file,record_line,&bytes,&length) && length==base_length
      && (!length || !memcmp(bytes,base,length));
}
int the_workbench_document_pending_owned(const void *file,size_t line)
{
   const unsigned char *bytes; size_t length,lines; int dirty;
   if (!record_known || !file || record_file!=file || !line || line!=record_line
   || !provider.info || !provider.pending || !provider.info(file,&lines,&dirty)
   || line>lines || !pending_base_matches(file,lines) || !provider.pending(&bytes,&length)
   || (!bytes && length) || length>THE_WORKBENCH_DOCUMENT_RECORD_MAX) return 0;
   return length!=base_length || (length && memcmp(bytes,base,length));
}
TheWorkbenchDocumentResult the_workbench_document_info(uint64_t id,TheWorkbenchDocumentInfo *out)
{
   Document *doc=find(id);
   const unsigned char *pending; size_t length;
   if (!out) return THE_WORKBENCH_DOCUMENT_INVALID;
   memset(out,0,sizeof(*out)); out->id=id; out->revision=revision; out->reason="document-closed";
   if (!doc || !provider.info || !provider.info(doc->file,&out->lines,&out->dirty)) return THE_WORKBENCH_DOCUMENT_MISSING;
   out->reason="limit-exceeded";
   if (exhausted) return THE_WORKBENCH_DOCUMENT_LIMIT;
   out->reason="busy-document";
   if (executing || (provider.blocked && provider.blocked())) return THE_WORKBENCH_DOCUMENT_BUSY;
   out->reason="record-unavailable";
   if (provider.pending && ((!record_known && (!provider.no_pending || !provider.no_pending(doc->file)))
   || (record_known && !pending_base_matches(doc->file,out->lines)))) return THE_WORKBENCH_DOCUMENT_BUSY;
   if (provider.pending && record_file==doc->file && record_line && record_line<=out->lines)
   {
      if (!provider.pending(&pending,&length) || (!pending && length) || length>THE_WORKBENCH_DOCUMENT_RECORD_MAX) return THE_WORKBENCH_DOCUMENT_BUSY;
      if (length!=base_length || (length && memcmp(pending,base,length))) out->dirty=1;
   }
   out->available=1; out->reason="ok";
   return THE_WORKBENCH_DOCUMENT_OK;
}
TheWorkbenchDocumentResult the_workbench_document_read(uint64_t id,uint64_t expected,
   size_t line,size_t offset,size_t lines,size_t bytes,TheWorkbenchDocumentPage *out)
{
   TheWorkbenchDocumentResult result;
   Document *doc;
   if (!out) return THE_WORKBENCH_DOCUMENT_INVALID;
   memset(out,0,sizeof(*out));
   if (!line || !lines || lines>THE_WORKBENCH_DOCUMENT_LINES_MAX || !bytes || bytes>THE_WORKBENCH_DOCUMENT_BYTES_MAX) return THE_WORKBENCH_DOCUMENT_INVALID;
   result=the_workbench_document_info(id,&out->info);
   if (result==THE_WORKBENCH_DOCUMENT_MISSING) return result;
   if (expected && expected!=out->info.revision) return THE_WORKBENCH_DOCUMENT_STALE;
   if (result!=THE_WORKBENCH_DOCUMENT_OK) return result;
   doc=find(id);
   if (line>out->info.lines && (line-out->info.lines!=1 || offset)) return THE_WORKBENCH_DOCUMENT_INVALID;
   out->next_line=line; out->next_offset=offset;
   while (line<=out->info.lines && out->count<lines)
   {
      const unsigned char *text; size_t length,n=0,pos=0,budget=bytes-out->length;
      TheWorkbenchDocumentSlice *slice;
      if (!provider.line(doc->file,line,&text,&length) || (!text && length)) { result=THE_WORKBENCH_DOCUMENT_MISSING; goto failed; }
      if (provider.pending && record_file==doc->file && record_line==line)
         if (!provider.pending(&text,&length) || (!text && length)) { result=THE_WORKBENCH_DOCUMENT_BUSY; goto failed; }
      if (offset>length) { result=THE_WORKBENCH_DOCUMENT_INVALID; goto failed; }
      while(pos<offset) pos+=the_workbench_document_unit(text+pos,length-pos);
      if (pos!=offset) { result=THE_WORKBENCH_DOCUMENT_INVALID; goto failed; }
      while (offset+n<length)
      {
         size_t size=the_workbench_document_unit(text+offset+n,length-offset-n);
         if (size>budget-n) break;
         n+=size;
      }
      if (!n && offset<length)
      { if (out->count) break; result=THE_WORKBENCH_DOCUMENT_LIMIT; goto failed; }
      slice=out->slices+out->count++;
      slice->line=line; slice->offset=offset; slice->length=n; slice->original_length=length; slice->data_offset=out->length;
      if (n) memcpy(out->bytes+out->length,text+offset,n);
      out->length+=n;
      if (offset+n==length) { line++; offset=0; } else { offset+=n; }
      out->next_line=line; out->next_offset=offset;
      if (out->length==bytes || offset) break;
   }
   if (revision!=out->info.revision) { result=THE_WORKBENCH_DOCUMENT_STALE; goto failed; }
   if (executing || (provider.blocked && provider.blocked())) { result=THE_WORKBENCH_DOCUMENT_BUSY; goto failed; }
   out->more=line<=out->info.lines;
   return THE_WORKBENCH_DOCUMENT_OK;
failed:
   out->count=out->length=0; out->more=0;
   return result;
}
const char *the_workbench_document_result_name(TheWorkbenchDocumentResult result)
{
   switch(result) {
      case THE_WORKBENCH_DOCUMENT_OK:return "ok";
      case THE_WORKBENCH_DOCUMENT_MISSING:return "document-closed";
      case THE_WORKBENCH_DOCUMENT_STALE:return "stale-revision";
      case THE_WORKBENCH_DOCUMENT_BUSY:return "busy-document";
      case THE_WORKBENCH_DOCUMENT_LIMIT:case THE_WORKBENCH_DOCUMENT_NOMEM:return "limit-exceeded";
      default:return "invalid-operand";
   }
}
