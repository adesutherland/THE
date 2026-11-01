#define _XOPEN_SOURCE 700
#include "workbenchfileio.h"
#include "toolhost.h"
#include <assert.h>
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static TheWorkbenchFileRequest route;
static unsigned running,starts,stops,eofs,ends,published;
static int fail_start,fail_point,deny,mutate_input;
static int swap_parent;
static char swap_from[4096],swap_to[4096],swap_outside[4096];
static unsigned char received[THE_WORKBENCH_FILE_BYTES_MAX];
static size_t received_length;
static char root[4096],source[4096],target[4096],error[256];
int the_workbench_fileio_test_failure(int point) { return point==fail_point; }
static void put(const char *path,const void *bytes,size_t length) {
   FILE *f=fopen(path,"wb");assert(f);assert(fwrite(bytes,1,length,f)==length);assert(!fclose(f));
}
static void bytes(const char *path,const void *expected,size_t length) {
   unsigned char *buffer=malloc(length+1);FILE *f=fopen(path,"rb");assert(f && buffer);
   assert(fread(buffer,1,length+1,f)==length);assert(!memcmp(buffer,expected,length));assert(!fclose(f));free(buffer);
}
static int exists(const char *path) { struct stat st;return !lstat(path,&st); }
static void no_stage(const char *directory) {
   DIR *d=opendir(directory);struct dirent *entry;assert(d);
   while((entry=readdir(d))) assert(strncmp(entry->d_name,".the-stage-",11));closedir(d);
}
static int authority(const char *path,int writing,int committed,char *out,size_t capacity) {
   (void)path;(void)writing;if(committed) {published++;return 1;}
   if(swap_parent) {swap_parent=0;assert(!rename(swap_from,swap_to));assert(!symlink(swap_outside,swap_from));}
   if(deny) {snprintf(out,capacity,"Injected readonly/policy refusal");return 0;}return 1;
}
static int start(const TheToolRequest *r,char *out,size_t capacity) {
   running=r->id;starts++;
   if(mutate_input) put(source,"changed after capture",21);
   the_tool_started(r->id);
   if(fail_start) {the_tool_output(r->id,1,"partial",7);snprintf(out,capacity,"Injected start failure");return 0;}return 1;
}
static int input(unsigned id,const void *data,size_t length) {
   assert(id==running && length<=sizeof(received));memcpy(received,data,length);received_length=length;return 1;
}
static int eof(unsigned id) {assert(id==running);eofs++;return 1;}
static void stop(unsigned id) {assert(id==running);stops++;}
static int begin(unsigned id,const char *tool,void **binding,const void **data,size_t *length,int *finite,char *out,size_t capacity) {
   (void)tool;*binding=the_workbench_fileio_begin(id,&route,out,capacity);if(!*binding)return 0;
   *finite=route.input!=NULL;*data=the_workbench_fileio_input(*binding,length);return 1;
}
static int raw(void *binding,unsigned id,int fd,const void *data,size_t length,char *out,size_t capacity) {
   (void)id;return the_workbench_fileio_output(binding,fd,data,length,out,capacity);
}
static int end(void *binding,unsigned id,int success,char *out,size_t capacity) {
   (void)id;ends++;return the_workbench_fileio_end(binding,success,out,capacity);
}
static int launch(void) {
   TheToolRequest request={0};strcpy(request.tool.id,"test");strcpy(request.tool.role,"program");request.tool.available=1;
   request.filename="test";request.source="";return the_tool_launch(&request,error,sizeof(error));
}
static void complete(void) {the_tool_finish(running,0,"0",NULL,NULL);}
static void absent(void) {assert(!exists(target));no_stage(root);}
int main(void) {
   char temporary[]="/tmp/THE-file-io-XXXXXX",path[4096],parent[4096],moved[4096],outside[4096],json[65536];
   static const unsigned char binary[]={0,255,226,130,172,10,27,60};
   TheToolAdapter adapter={start,input,eof,stop,NULL};TheToolIO io={begin,raw,end};
   TheWorkbenchFileInfo info;unsigned before;FILE *f;
   assert(mkdtemp(temporary));assert(realpath(temporary,root));
   snprintf(source,sizeof(source),"%s/input bytes",root);snprintf(target,sizeof(target),"%s/output bytes",root);
   assert(the_workbench_fileio_initialize(root));the_workbench_fileio_authority(authority);
   the_tool_host_reset();the_tool_host_adapter(&adapter);assert(the_tool_host_io(&io));
   route.input=source;route.output=target;route.stream=1;
   put(source,binary,sizeof(binary));mutate_input=1;
   assert(launch());mutate_input=0;
   assert(received_length==sizeof(binary) && !memcmp(received,binary,sizeof(binary)) && eofs==1);
   assert(!the_tool_input("interactive",11) && !the_tool_eof());
   the_tool_waiting(running,1);assert(!the_tool_console()->waiting);
   the_tool_output(running,1,binary,3);the_tool_output(running,2,"other stream",12);
   the_tool_output(running,1,binary+3,sizeof(binary)-3);complete();
   bytes(target,binary,sizeof(binary));assert(published==1 && ends==1 && the_workbench_fileio_receipt()->published);
   assert(!strcmp(the_workbench_fileio_receipt()->code,"ok") && the_workbench_fileio_receipt()->token[0]);
   assert(the_workbench_fileio_inspect(target,&info,error,sizeof(error)) && info.exists && info.length==sizeof(binary));
   assert(!strcmp(info.token,the_workbench_fileio_receipt()->token));
   assert(the_workbench_fileio_json(json,sizeof(json)) && strstr(json,"\"openOutputTargets\":false"));
   no_stage(root);unlink(target);

   /* Empty finite input still closes exactly once, and Stop remains available
    * after the finite-input EOF has disabled ordinary console input. */
   put(source,"",0);before=eofs;assert(launch());assert(eofs==before+1 && the_tool_console()->eof);
   before=stops;assert(the_tool_console_key(0,3));assert(stops==before+1);absent();
   assert(!strcmp(the_workbench_fileio_receipt()->code,"discarded"));

   /* All confirmed pre-publication failures retain target absence and remove
    * staging, including output emitted synchronously by a failed start. */
   fail_start=1;before=ends;assert(!launch());fail_start=0;assert(ends==before+1);absent();
   fail_point=4;before=starts;assert(!launch() && starts==before);fail_point=0;absent();
   assert(launch());the_tool_output(running,1,binary,sizeof(binary));the_tool_finish(running,-1,"9","failure",NULL);
   assert(!strcmp(the_tool_console()->status,"9"));absent();
   assert(launch());the_tool_output(running,1,binary,sizeof(binary));the_tool_finish(running,0,"7",NULL,NULL);absent();
   assert(launch());the_tool_finish(running,0,"0",NULL,"LLB1\ninvalid\n");absent();
   assert(launch());the_tool_output(running,1,"x",1);the_tool_output(running,1,"x",THE_TOOL_OUTPUT_LIMIT);absent();
   assert(!strcmp(the_tool_console()->state,"Output limit reached"));
   assert(launch());fail_point=1;the_tool_output(running,1,binary,sizeof(binary));fail_point=0;absent();
   assert(!strcmp(the_tool_console()->state,"Failed"));
   assert(launch());the_tool_output(running,1,binary,sizeof(binary));fail_point=2;complete();fail_point=0;absent();
   assert(!strcmp(the_workbench_fileio_receipt()->code,"io-failure"));

   assert(launch());the_tool_output(running,1,binary,sizeof(binary));put(target,"external",8);complete();
   assert(!strcmp(the_tool_console()->state,"Failed"));bytes(target,"external",8);no_stage(root);unlink(target);
   put(target,"existing",8);before=starts;assert(!launch() && starts==before);bytes(target,"existing",8);unlink(target);
   deny=1;before=starts;assert(!launch() && starts==before);deny=0;absent();
   assert(launch());deny=1;complete();deny=0;absent();

   route.output=source;assert(!launch());route.output=target;
   snprintf(path,sizeof(path),"%s/linked-input",root);assert(!symlink(source,path));route.input=path;
   assert(!launch());unlink(path);route.input=source;
   assert(!symlink(source,target));assert(!launch());assert(exists(target));unlink(target);
   snprintf(outside,sizeof(outside),"%s-outside",root);put(outside,"outside",7);route.input=outside;
   assert(!launch());route.input=source;route.output=outside;assert(!launch());route.output=target;unlink(outside);
   f=fopen(source,"wb");assert(f);assert(!ftruncate(fileno(f),THE_WORKBENCH_FILE_BYTES_MAX+1));fclose(f);
   before=starts;assert(!launch() && starts==before);put(source,binary,sizeof(binary));

   /* Swap a parent after canonical resolution/authority admission. Anchored
    * no-follow traversal must refuse before reading the external leaf. */
   snprintf(swap_from,sizeof(swap_from),"%s/race-parent",root);snprintf(swap_to,sizeof(swap_to),"%s/race-moved",root);
   snprintf(swap_outside,sizeof(swap_outside),"%s-outside-directory",root);assert(!mkdir(swap_from,0700) && !mkdir(swap_outside,0700));
   snprintf(path,sizeof(path),"%s/input",swap_from);put(path,"inside",6);
   snprintf(outside,sizeof(outside),"%s/input",swap_outside);put(outside,"OUTSIDE",7);
   route.input=path;swap_parent=1;before=starts;assert(!launch() && starts==before);
   assert(!unlink(swap_from) && !rename(swap_to,swap_from));unlink(path);rmdir(swap_from);unlink(outside);rmdir(swap_outside);route.input=source;

   snprintf(parent,sizeof(parent),"%s/parent",root);snprintf(moved,sizeof(moved),"%s/moved",root);assert(!mkdir(parent,0700));
   snprintf(path,sizeof(path),"%s/result",parent);route.output=path;assert(launch());
   assert(!rename(parent,moved) && !mkdir(parent,0700));complete();
   assert(!exists(path));snprintf(path,sizeof(path),"%s/result",moved);assert(!exists(path));no_stage(moved);
   rmdir(parent);rmdir(moved);route.output=target;
   assert(!chmod(root,0500));assert(!launch());assert(!chmod(root,0700));absent();

   /* Moving/replacing the private staging directory cannot retarget its held
    * descriptor. The correct original bytes are still published. */
   assert(launch());the_tool_output(running,1,binary,sizeof(binary));
   {
      DIR *d=opendir(root);struct dirent *entry;struct stat st;char stage[4096],relocated[4096],decoy[4096];int found=0;
      assert(d);while((entry=readdir(d))) if(!strncmp(entry->d_name,".the-stage-",11)) {snprintf(stage,sizeof(stage),"%s/%s",root,entry->d_name);found=1;break;}closedir(d);assert(found);
      assert(!stat(stage,&st) && (st.st_mode&0777)==0700);
      snprintf(relocated,sizeof(relocated),"%s/relocated-stage",root);assert(!rename(stage,relocated));assert(!mkdir(stage,0700));
      snprintf(decoy,sizeof(decoy),"%s/output",stage);put(decoy,"wrong bytes",11);
      complete();bytes(target,binary,sizeof(binary));unlink(target);unlink(decoy);rmdir(stage);rmdir(relocated);no_stage(root);
   }

   /* A lost acknowledgement is deliberately uncertain: verify committed
    * bytes, require explicit read-back, then permit a later CREATE. */
   assert(launch());the_tool_output(running,1,binary,sizeof(binary));fail_point=3;complete();fail_point=0;
   assert(!strcmp(the_workbench_fileio_receipt()->code,"publication-unknown"));bytes(target,binary,sizeof(binary));
   assert(!launch());assert(the_workbench_fileio_inspect(target,&info,error,sizeof(error)));unlink(target);
   assert(launch());complete();bytes(target,"",0);unlink(target);
   assert(launch());before=ends;the_tool_host_reset();assert(ends==before+1);absent();
   unlink(source);assert(!rmdir(root));puts("Exact-byte FILE stdin, atomic CREATE and failure preservation pass");return 0;
}
