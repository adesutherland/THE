#define _POSIX_C_SOURCE 200809L
#define _XOPEN_SOURCE 700
#define _DARWIN_C_SOURCE 1
#include "workbenchfileio.h"
#include <errno.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char workspace[THE_WORKBENCH_FILE_PATH_MAX+1];
static TheWorkbenchFileAuthority authority;
static TheWorkbenchFileReceipt receipt;
static const char *failure_code="io-failure";
static char unresolved[THE_WORKBENCH_FILE_PATH_MAX+1];
#ifdef THE_WORKBENCH_FILEIO_TEST
extern int the_workbench_fileio_test_failure(int point);
#define storage_failure(point) the_workbench_fileio_test_failure(point)
#else
#define storage_failure(point) 0
#endif
const char *the_workbench_fileio_workspace(void) { return workspace; }
void the_workbench_fileio_authority(TheWorkbenchFileAuthority value) { authority=value; }
const TheWorkbenchFileReceipt *the_workbench_fileio_receipt(void) { return &receipt; }

#if !defined(_WIN32) && !defined(__EMSCRIPTEN__)
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

struct TheWorkbenchFileJob {
   unsigned id;
   unsigned char *input;
   size_t input_length, output_length;
   int stream, directory, staging, staging_directory;
   char operand[THE_WORKBENCH_FILE_PATH_MAX+1];
   char output[THE_WORKBENCH_FILE_PATH_MAX+1], parent[THE_WORKBENCH_FILE_PATH_MAX+1];
   char leaf[THE_WORKBENCH_FILE_PATH_MAX+1], temporary[80];
   struct stat parent_info;
   struct stat staging_info;
};
static int workspace_directory=-1;
static struct stat workspace_info;
static int same_version(const struct stat *a,const struct stat *b) {
#if defined(__APPLE__)
   return a->st_dev==b->st_dev && a->st_ino==b->st_ino && a->st_size==b->st_size
      && a->st_mtimespec.tv_sec==b->st_mtimespec.tv_sec && a->st_mtimespec.tv_nsec==b->st_mtimespec.tv_nsec
      && a->st_ctimespec.tv_sec==b->st_ctimespec.tv_sec && a->st_ctimespec.tv_nsec==b->st_ctimespec.tv_nsec;
#else
   return a->st_dev==b->st_dev && a->st_ino==b->st_ino && a->st_size==b->st_size
      && a->st_mtim.tv_sec==b->st_mtim.tv_sec && a->st_mtim.tv_nsec==b->st_mtim.tv_nsec
      && a->st_ctim.tv_sec==b->st_ctim.tv_sec && a->st_ctim.tv_nsec==b->st_ctim.tv_nsec;
#endif
}
static int fail(char *error,size_t capacity,const char *code,const char *message) {
   failure_code=code;
   snprintf(receipt.code,sizeof(receipt.code),"%s",code);
   snprintf(receipt.message,sizeof(receipt.message),"%s",message);
   if(error && capacity) snprintf(error,capacity,"%s: %s",code,message);
   return 0;
}
int the_workbench_fileio_supported(void) { return workspace[0]!=0; }
int the_workbench_fileio_initialize(const char *root) {
   char resolved[4096]; struct stat info;
   if(workspace[0]) return 1;
   if(!realpath(root && *root ? root : ".",resolved) || strlen(resolved)>THE_WORKBENCH_FILE_PATH_MAX
   || stat(resolved,&info) || !S_ISDIR(info.st_mode)) return 0;
   workspace_directory=open(resolved,O_RDONLY|O_DIRECTORY|O_NOFOLLOW|O_CLOEXEC);
   if(workspace_directory<0 || fstat(workspace_directory,&workspace_info)
   || workspace_info.st_dev!=info.st_dev || workspace_info.st_ino!=info.st_ino) {
      if(workspace_directory>=0) close(workspace_directory);workspace_directory=-1;return 0;
   }
   strcpy(workspace,resolved); return 1;
}
static int inside(const char *path) {
   size_t n=strlen(workspace);
   return n && (!strcmp(workspace,"/") || (!strncmp(path,workspace,n) && (path[n]=='/' || !path[n])));
}
static int candidate(const char *path,char *out,size_t capacity) {
   int n;
   if(!path || !*path || strlen(path)>THE_WORKBENCH_FILE_PATH_MAX) return 0;
   n=path[0]=='/' ? snprintf(out,capacity,"%s",path) : snprintf(out,capacity,"%s/%s",workspace,path);
   return n>0 && (size_t)n<capacity;
}
static int check_authority(const char *path,int writing,int published,char *error,size_t capacity) {
   char reason[192]="File authority denied";
   if(authority && !authority(path,writing,published,reason,sizeof(reason)))
      return fail(error,capacity,"policy-denied",reason);
   return 1;
}
/* Walk from the startup directory descriptor. O_NOFOLLOW on only the final
 * absolute pathname component does not protect a swapped ancestor symlink. */
static int open_parent(const char *parent,const struct stat *expected,char *error,size_t capacity) {
   const char *p;int directory,next;struct stat info;char component[THE_WORKBENCH_FILE_PATH_MAX+1];
   if(!inside(parent) || workspace_directory<0) {fail(error,capacity,"policy-denied","Parent is outside the startup workspace");return -1;}
   directory=dup(workspace_directory);
   if(directory<0) {fail(error,capacity,"io-failure","Cannot hold workspace directory");return -1;}
   p=parent+strlen(workspace);while(*p=='/')p++;
   while(*p) {
      const char *slash=strchr(p,'/');size_t n=slash?(size_t)(slash-p):strlen(p);
      memcpy(component,p,n);component[n]=0;
      next=openat(directory,component,O_RDONLY|O_DIRECTORY|O_NOFOLLOW|O_CLOEXEC);
      close(directory);directory=next;
      if(directory<0) {fail(error,capacity,"stale-revision","Workspace parent changed or contains a symlink");return -1;}
      p+=n;while(*p=='/')p++;
   }
   if(fstat(directory,&info) || info.st_dev!=expected->st_dev || info.st_ino!=expected->st_ino) {
      close(directory);fail(error,capacity,"stale-revision","Workspace parent identity changed");return -1;
   }
   return directory;
}
static int private_directory(int parent,char *name,size_t capacity,struct stat *created) {
   unsigned char random[16];char hex[33];size_t offset=0,i;struct stat info;int fd=open("/dev/urandom",O_RDONLY|O_CLOEXEC);
   if(fd<0)return -1;
   while(offset<sizeof(random)) {
      ssize_t n=read(fd,random+offset,sizeof(random)-offset);
      if(n<0 && errno==EINTR)continue;
      if(n<=0){close(fd);return -1;}offset+=(size_t)n;
   }
   close(fd);for(i=0;i<sizeof(random);i++)snprintf(hex+2*i,3,"%02x",random[i]);
   snprintf(name,capacity,".the-stage-%s",hex);
   if(mkdirat(parent,name,0700)) {name[0]=0;return -1;}
   if(fstatat(parent,name,created,AT_SYMLINK_NOFOLLOW) || !S_ISDIR(created->st_mode)
   || created->st_uid!=geteuid() || (created->st_mode&0077))return -1;
   fd=storage_failure(4)?-1:openat(parent,name,O_RDONLY|O_DIRECTORY|O_NOFOLLOW|O_CLOEXEC);
   if(fd>=0 && (fstat(fd,&info) || info.st_dev!=created->st_dev || info.st_ino!=created->st_ino)) {close(fd);fd=-1;}
   return fd;
}
/* Resolve the existing parent and preserve the final directory entry. CREATE
 * never follows a final symlink, including a dangling one. */
static int resolve(const char *path,char *out,char *parent,char *leaf,
   struct stat *directory,char *error,size_t capacity) {
   char joined[4096], resolved[4096], *slash; struct stat info;
   if(!the_workbench_fileio_supported()) return fail(error,capacity,"unsupported-feature","Native FILE routes are unavailable");
   if(!candidate(path,joined,sizeof(joined)) || !(slash=strrchr(joined,'/')) || !slash[1]
   || !strcmp(slash+1,".") || !strcmp(slash+1,".."))
      return fail(error,capacity,"invalid-operand","Invalid workspace file path");
   strcpy(leaf,slash+1); *slash=0;
   if(!realpath(*joined ? joined : "/",resolved) || stat(resolved,&info) || !S_ISDIR(info.st_mode))
      return fail(error,capacity,"io-failure","File parent is unavailable");
   if(!inside(resolved)) return fail(error,capacity,"policy-denied","File path is outside the startup workspace");
   if(strlen(resolved)>THE_WORKBENCH_FILE_PATH_MAX
   || snprintf(out,THE_WORKBENCH_FILE_PATH_MAX+1,"%s%s%s",resolved,!strcmp(resolved,"/") ? "" : "/",leaf)>THE_WORKBENCH_FILE_PATH_MAX)
      return fail(error,capacity,"limit-exceeded","Canonical file path is too long");
   strcpy(parent,resolved); if(directory) *directory=info; return 1;
}
const char *the_workbench_fileio_check(const char *path,int writing,int ready,char *error,size_t capacity) {
   TheWorkbenchFileReceipt saved=receipt;const char *code="ok";
   char canonical[THE_WORKBENCH_FILE_PATH_MAX+1],parent[THE_WORKBENCH_FILE_PATH_MAX+1],leaf[THE_WORKBENCH_FILE_PATH_MAX+1];
   struct stat directory_info,info;int directory=-1,exists;
   if(!resolve(path,canonical,parent,leaf,&directory_info,error,capacity)) {code=failure_code;goto done;}
   directory=open_parent(parent,&directory_info,error,capacity);
   if(directory<0) {code=failure_code;goto done;}
   exists=!fstatat(directory,leaf,&info,AT_SYMLINK_NOFOLLOW);
   if(!exists && errno!=ENOENT) {fail(error,capacity,"io-failure","File entry is inaccessible");code=failure_code;goto done;}
   if(exists && !S_ISREG(info.st_mode)) {fail(error,capacity,"policy-denied","FILE routes require a regular leaf without a symlink");code=failure_code;goto done;}
   if(ready) {
      if(!check_authority(canonical,writing,0,error,capacity)) {code=failure_code;goto done;}
      if(writing && (exists || !(directory_info.st_mode&0222) || unresolved[0])) {fail(error,capacity,unresolved[0]?"publication-unknown":exists?"stale-revision":"policy-denied","CREATE target is not currently writable and absent");code=failure_code;goto done;}
      if(!writing && (!exists || info.st_size<0 || (uintmax_t)info.st_size>THE_WORKBENCH_FILE_BYTES_MAX)) {fail(error,capacity,exists?"limit-exceeded":"missing-object","FILE input is unavailable or exceeds 8 MiB");code=failure_code;goto done;}
   }
done:
   if(directory>=0)close(directory);receipt=saved;return code;
}
static void token(const struct stat *info,char *out,size_t capacity) {
#if defined(__APPLE__)
   long nanos=info->st_mtimespec.tv_nsec, cnanos=info->st_ctimespec.tv_nsec;
#else
   long nanos=info->st_mtim.tv_nsec, cnanos=info->st_ctim.tv_nsec;
#endif
   snprintf(out,capacity,"f-%jx-%jx-%jx-%jx-%lx-%jx-%lx",(uintmax_t)info->st_dev,
      (uintmax_t)info->st_ino,(uintmax_t)info->st_size,(uintmax_t)info->st_mtime,
      nanos,(uintmax_t)info->st_ctime,cnanos);
}
int the_workbench_fileio_inspect(const char *path,TheWorkbenchFileInfo *out,char *error,size_t capacity) {
   TheWorkbenchFileInfo info; char parent[THE_WORKBENCH_FILE_PATH_MAX+1],leaf[THE_WORKBENCH_FILE_PATH_MAX+1]; struct stat st,directory_info;int directory;
   memset(&info,0,sizeof(info));
   if(!out || !resolve(path,info.path,parent,leaf,&directory_info,error,capacity)
   || !check_authority(info.path,0,0,error,capacity)) return 0;
   directory=open_parent(parent,&directory_info,error,capacity);if(directory<0)return 0;
   if(fstatat(directory,leaf,&st,AT_SYMLINK_NOFOLLOW)) {
      int saved=errno;close(directory);
      if(saved!=ENOENT) return fail(error,capacity,"io-failure","Cannot inspect workspace file");
      strcpy(info.token,"absent");
   } else {
      close(directory);
      if(!S_ISREG(st.st_mode)) return fail(error,capacity,"policy-denied","FILE routes require a regular file, without a final symlink");
      info.exists=1; info.readonly=!(st.st_mode & 0222); info.length=(size_t)st.st_size; token(&st,info.token,sizeof(info.token));
   }
   if(authority) {char reason[192];if(!authority(info.path,1,0,reason,sizeof(reason)))info.readonly=1;}
   if(!strcmp(unresolved,info.path)) unresolved[0]=0;
   *out=info; return 1;
}
static void destroy(TheWorkbenchFileJob *job) {
   if(!job) return;
   if(job->staging>=0) close(job->staging);
   if(job->staging_directory>=0) {
      struct stat held,named;
      unlinkat(job->staging_directory,"output",0);
      if(job->directory>=0 && job->temporary[0] && !fstat(job->staging_directory,&held)
      && !fstatat(job->directory,job->temporary,&named,AT_SYMLINK_NOFOLLOW)
      && held.st_dev==named.st_dev && held.st_ino==named.st_ino)
         unlinkat(job->directory,job->temporary,AT_REMOVEDIR);
      close(job->staging_directory);
   } else if(job->directory>=0 && job->temporary[0] && job->staging_info.st_ino) {
      struct stat named;
      if(!fstatat(job->directory,job->temporary,&named,AT_SYMLINK_NOFOLLOW)
      && named.st_dev==job->staging_info.st_dev && named.st_ino==job->staging_info.st_ino)
         unlinkat(job->directory,job->temporary,AT_REMOVEDIR);
   }
   if(job->directory>=0) close(job->directory);
   free(job->input); free(job);
}
TheWorkbenchFileJob *the_workbench_fileio_begin(unsigned id,const TheWorkbenchFileRequest *r,char *error,size_t capacity) {
   TheWorkbenchFileJob *job; struct stat info,after,directory_info; int fd=-1,input_directory=-1; char path[THE_WORKBENCH_FILE_PATH_MAX+1],parent[THE_WORKBENCH_FILE_PATH_MAX+1],leaf[THE_WORKBENCH_FILE_PATH_MAX+1];
   memset(&receipt,0,sizeof(receipt)); receipt.job=id; strcpy(receipt.code,"preparing");
   if(!id || !r || (!r->input && !r->output) || (r->output && r->stream!=1 && r->stream!=2)) { fail(error,capacity,"invalid-operand","Invalid FILE route request"); return NULL; }
   job=calloc(1,sizeof(*job));
   if(!job) { fail(error,capacity,"limit-exceeded","Cannot allocate FILE route job"); return NULL; }
   job->id=id; job->directory=job->staging=job->staging_directory=-1;
   if(r->input) {
      size_t offset=0;
      char after_path[THE_WORKBENCH_FILE_PATH_MAX+1],after_parent[THE_WORKBENCH_FILE_PATH_MAX+1],after_leaf[THE_WORKBENCH_FILE_PATH_MAX+1];
      if(!resolve(r->input,path,parent,leaf,&directory_info,error,capacity) || !check_authority(path,0,0,error,capacity)) goto failed;
      input_directory=open_parent(parent,&directory_info,error,capacity);if(input_directory<0)goto failed;
      fd=openat(input_directory,leaf,O_RDONLY|O_NOFOLLOW|O_CLOEXEC|O_NONBLOCK);
      if(fd<0 || fstat(fd,&info) || !S_ISREG(info.st_mode)) { fail(error,capacity,"io-failure","Cannot open regular FILE input"); goto failed; }
      if(info.st_size<0 || (uintmax_t)info.st_size>THE_WORKBENCH_FILE_BYTES_MAX) { fail(error,capacity,"limit-exceeded","FILE input exceeds 8 MiB"); goto failed; }
      job->input_length=(size_t)info.st_size;
      job->input=malloc(job->input_length+1);
      if(!job->input) { fail(error,capacity,"limit-exceeded","Cannot allocate FILE input snapshot"); goto failed; }
      while(offset<job->input_length) {
         ssize_t n=read(fd,job->input+offset,job->input_length-offset);
         if(n<0 && errno==EINTR) continue;
         if(n<=0) { fail(error,capacity,"stale-revision","FILE input changed during capture"); goto failed; }
         offset+=(size_t)n;
      }
      if(fstat(fd,&after) || !same_version(&info,&after)
      || fstatat(input_directory,leaf,&after,AT_SYMLINK_NOFOLLOW) || !same_version(&info,&after)
      || !resolve(r->input,after_path,after_parent,after_leaf,&after,error,capacity)
      || strcmp(path,after_path) || directory_info.st_dev!=after.st_dev || directory_info.st_ino!=after.st_ino) { fail(error,capacity,"stale-revision","FILE input or parent changed during capture"); goto failed; }
      close(fd);fd=-1;close(input_directory);input_directory=-1;receipt.input_bytes=job->input_length;
   }
   if(r->output) {
      if(!resolve(r->output,job->output,job->parent,job->leaf,&job->parent_info,error,capacity)
      || !check_authority(job->output,1,0,error,capacity)) goto failed;
      if(unresolved[0]) { fail(error,capacity,"publication-unknown","Inspect the prior uncertain output target before writing again");goto failed; }
      if(r->input && !strcmp(path,job->output)) { fail(error,capacity,"object-in-use","FILE input cannot also be an output target"); goto failed; }
      if(lstat(job->output,&info)==0 || errno!=ENOENT) { fail(error,capacity,"stale-revision","CREATE output target already exists or is inaccessible"); goto failed; }
      if(!(job->parent_info.st_mode & 0222)) { fail(error,capacity,"policy-denied","Output parent is readonly"); goto failed; }
      strcpy(job->operand,r->output); strcpy(receipt.path,job->output); job->stream=r->stream;
      job->directory=open_parent(job->parent,&job->parent_info,error,capacity);
      if(job->directory<0 || fstat(job->directory,&after) || after.st_dev!=job->parent_info.st_dev || after.st_ino!=job->parent_info.st_ino) { fail(error,capacity,"stale-revision","Output parent changed during launch"); goto failed; }
      job->staging_directory=private_directory(job->directory,job->temporary,sizeof(job->temporary),&job->staging_info);
      if(job->staging_directory>=0) job->staging=openat(job->staging_directory,"output",O_WRONLY|O_CREAT|O_EXCL|O_CLOEXEC|O_NOFOLLOW,0600);
      if(job->staging<0) {fail(error,capacity,"io-failure","Cannot create private output staging file");goto failed;}
   }
   strcpy(receipt.code,"running"); return job;
failed:
   if(fd>=0) close(fd);if(input_directory>=0)close(input_directory);destroy(job);return NULL;
}
const void *the_workbench_fileio_input(const TheWorkbenchFileJob *job,size_t *length) {
   if(length) *length=job ? job->input_length : 0;
   return job ? job->input : NULL;
}
int the_workbench_fileio_output(TheWorkbenchFileJob *job,int fd,const void *bytes,size_t length,char *error,size_t capacity) {
   const unsigned char *p=bytes; size_t offset=0;
   if(!job || (length && !bytes)) return fail(error,capacity,"invalid-operand","Invalid raw FILE output");
   if(!job->stream || fd!=job->stream) return 1;
   if(storage_failure(1)) return fail(error,capacity,"io-failure","Cannot write output staging file");
   if(length>THE_WORKBENCH_FILE_BYTES_MAX-job->output_length) return fail(error,capacity,"limit-exceeded","FILE output exceeds 8 MiB");
   while(offset<length) {
      ssize_t n=write(job->staging,p+offset,length-offset);
      if(n<0 && errno==EINTR) continue;
      if(n<=0) return fail(error,capacity,"io-failure","Cannot write output staging file");
      offset+=(size_t)n;
   }
   job->output_length+=length; receipt.output_bytes=job->output_length; return 1;
}
int the_workbench_fileio_end(TheWorkbenchFileJob *job,int success,char *error,size_t capacity) {
   int good=1; char path[THE_WORKBENCH_FILE_PATH_MAX+1],parent[THE_WORKBENCH_FILE_PATH_MAX+1],leaf[THE_WORKBENCH_FILE_PATH_MAX+1]; struct stat st,staged;
   if(!job) return 1;
   if(!success) { if(!strcmp(receipt.code,"running")) strcpy(receipt.code,"discarded"); destroy(job); return 1; }
   if(job->stream) {
      strcpy(receipt.code,"publishing");
      if(!resolve(job->operand,path,parent,leaf,&st,error,capacity) || strcmp(path,job->output)
      || st.st_dev!=job->parent_info.st_dev || st.st_ino!=job->parent_info.st_ino) {
         good=fail(error,capacity,"stale-revision","Output parent or path changed before publication"); goto done;
      }
      if(!(st.st_mode&0222) || !check_authority(job->output,1,0,error,capacity)) { good=fail(error,capacity,"policy-denied","Output target is no longer writable"); goto done; }
      if(storage_failure(2) || fsync(job->staging) || fstat(job->staging,&staged)) { good=fail(error,capacity,"io-failure","Cannot sync output staging file"); goto done; }
      if(fstatat(job->staging_directory,"output",&st,AT_SYMLINK_NOFOLLOW) || !same_version(&st,&staged)) {good=fail(error,capacity,"stale-revision","Private staging entry changed before publication");goto done;}
      /* linkat is an atomic no-clobber publication: any existing entry,
       * including a racing external writer or symlink, wins unchanged. */
      if(linkat(job->staging_directory,"output",job->directory,job->leaf,0)) { good=fail(error,capacity,errno==EEXIST ? "stale-revision" : "io-failure","CREATE publication failed without replacing the target"); goto done; }
      if(storage_failure(3) || fsync(job->directory) || fstatat(job->directory,job->leaf,&st,AT_SYMLINK_NOFOLLOW)
      || st.st_dev!=staged.st_dev || st.st_ino!=staged.st_ino
      || !resolve(job->operand,path,parent,leaf,&st,error,capacity) || strcmp(path,job->output)
      || st.st_dev!=job->parent_info.st_dev || st.st_ino!=job->parent_info.st_ino) {
         strcpy(unresolved,job->output); good=fail(error,capacity,"publication-unknown","CREATE committed but storage acknowledgement or path verification failed"); goto done;
      }
      /* Removing the staging hard link changes ctime. Read the final token
       * only after that change so it matches immediate target inspection. */
      if(unlinkat(job->staging_directory,"output",0) || fstat(job->staging,&st)) { strcpy(unresolved,job->output);good=fail(error,capacity,"publication-unknown","CREATE committed but cleanup or token read-back failed");goto done; }
      receipt.published=1; token(&st,receipt.token,sizeof(receipt.token));
      if(authority) { char ignored[192]; (void)authority(job->output,1,1,ignored,sizeof(ignored)); }
   }
   strcpy(receipt.code,"ok"); receipt.message[0]=0;
done:
   destroy(job); return good;
}
#else
struct TheWorkbenchFileJob { int unused; };
int the_workbench_fileio_initialize(const char *root) { (void)root; return 0; }
int the_workbench_fileio_supported(void) { return 0; }
const char *the_workbench_fileio_check(const char *p,int w,int r,char *e,size_t c) { (void)p;(void)w;(void)r;if(e&&c)snprintf(e,c,"Native FILE routes are unavailable");return "unsupported-feature"; }
static int unavailable(char *error,size_t capacity) { if(error && capacity) snprintf(error,capacity,"unsupported-feature: Native atomic FILE routes are not qualified on this host"); return 0; }
int the_workbench_fileio_inspect(const char *p,TheWorkbenchFileInfo *i,char *e,size_t c) { (void)p;(void)i;return unavailable(e,c); }
TheWorkbenchFileJob *the_workbench_fileio_begin(unsigned j,const TheWorkbenchFileRequest *r,char *e,size_t c) { (void)j;(void)r;unavailable(e,c);return NULL; }
const void *the_workbench_fileio_input(const TheWorkbenchFileJob *j,size_t *n) { (void)j;if(n)*n=0;return NULL; }
int the_workbench_fileio_output(TheWorkbenchFileJob *j,int fd,const void *b,size_t n,char *e,size_t c) { (void)j;(void)fd;(void)b;(void)n;return unavailable(e,c); }
int the_workbench_fileio_end(TheWorkbenchFileJob *j,int s,char *e,size_t c) { (void)j;(void)s;(void)e;(void)c;return 1; }
#endif

static int quoted(char *out,size_t capacity,const char *text) {
   size_t used=0; const unsigned char *p=(const unsigned char *)text;
   if(!capacity) return 0; out[used++]='"';
   while(*p) {
      char encoded[8]; size_t n;
      if(*p=='"' || *p=='\\') { encoded[0]='\\';encoded[1]=(char)*p;n=2; }
      else if(*p<32) { snprintf(encoded,sizeof(encoded),"\\u%04x",*p);n=6; }
      else { encoded[0]=(char)*p;n=1; }
      if(n>=capacity-used) return 0; memcpy(out+used,encoded,n);used+=n;p++;
   }
   if(used+2>capacity) return 0;out[used++]='"';out[used]=0;return 1;
}
int the_workbench_fileio_inspect_json(const char *path,char *out,size_t capacity) {
   TheWorkbenchFileInfo info; char error[256],q[THE_WORKBENCH_FILE_PATH_MAX*6+3]; int n;
   if(!the_workbench_fileio_inspect(path,&info,error,sizeof(error)) || !quoted(q,sizeof(q),info.path)) return 0;
   n=snprintf(out,capacity,"{\"schemaVersion\":1,\"section\":\"FILE\",\"path\":%s,\"exists\":%s,\"readonly\":%s,\"bytes\":%lu,\"token\":\"%s\"}",q,info.exists?"true":"false",info.readonly?"true":"false",(unsigned long)info.length,info.token);
   return n>=0 && (size_t)n<capacity;
}
int the_workbench_fileio_json(char *out,size_t capacity) {
   char root[THE_WORKBENCH_FILE_PATH_MAX*6+3],path[THE_WORKBENCH_FILE_PATH_MAX*6+3],unknown[THE_WORKBENCH_FILE_PATH_MAX*6+3],message[1200]; int n;
   if(!quoted(root,sizeof(root),workspace) || !quoted(path,sizeof(path),receipt.path) || !quoted(unknown,sizeof(unknown),unresolved) || !quoted(message,sizeof(message),receipt.message)) return 0;
   n=snprintf(out,capacity,"{\"schemaVersion\":1,\"section\":\"FILEIO\",\"available\":%s,\"workspace\":%s,\"inputBytesLimit\":8388608,\"outputBytesLimit\":8388608,\"create\":%s,\"replace\":false,\"append\":false,\"openOutputTargets\":false,\"reconciliationRequired\":%s,\"unresolvedPath\":%s,\"job\":%u,\"code\":\"%s\",\"message\":%s,\"path\":%s,\"inputBytes\":%lu,\"outputBytes\":%lu,\"published\":%s,\"token\":\"%s\"}",the_workbench_fileio_supported()?"true":"false",root,the_workbench_fileio_supported()?"true":"false",unresolved[0]?"true":"false",unknown,receipt.job,receipt.code[0]?receipt.code:"idle",message,path,(unsigned long)receipt.input_bytes,(unsigned long)receipt.output_bytes,receipt.published?"true":"false",receipt.token);
   return n>=0 && (size_t)n<capacity;
}
