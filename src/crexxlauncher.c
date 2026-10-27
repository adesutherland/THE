/* Relocatable launcher: choose a complete cREXX cohort before starting THE. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <sys/stat.h>
#include <crexxsaa.h>
#include "crexxapi.h"
#include "crexxcache.h"
#ifdef _WIN32
#include <windows.h>
#include <direct.h>
#include <io.h>
#include <process.h>
#include "../packaging/windows/crexxsaa-spawn.h"
#define SEP ';'
#define EXE ".exe"
#define mkdir_one(p) _mkdir(p)
#define dup _dup
#define dup2 _dup2
#define close _close
#define fileno _fileno
#else
#include <unistd.h>
#include <dlfcn.h>
#include <sys/wait.h>
#define SEP ':'
#define EXE ""
#define mkdir_one(p) mkdir(p,0700)
#endif

typedef struct {
#define FIELD(name,result,args) result (*name) args;
    THE_CREXX_FUNCTIONS(FIELD)
#undef FIELD
} api;
static const char *self_path;

static const char *env(const char *key) { const char *s=getenv(key); return s && *s ? s : NULL; }
static char *copy(const char *s) { size_t n=strlen(s)+1; char *p=malloc(n); if (p) memcpy(p,s,n); return p; }
static char *join(const char *a,const char *b) {
    size_t n=strlen(a)+strlen(b)+2; char *p=malloc(n);
    if (p) snprintf(p,n,"%s/%s",a,b); return p;
}
static int same(const char *a,const char *b) {
    while (*a && *b) { if (toupper((unsigned char)*a++) != toupper((unsigned char)*b++)) return 0; }
    return *a==*b;
}
static int set(const char *key,const char *value) {
#ifdef _WIN32
    return _putenv_s(key,value);
#else
    return setenv(key,value,1);
#endif
}
static void default_env(const char *key,const char *value) { if (!env(key)) set(key,value); }
static void default_path(const char *key,const char *root,const char *name) {
    char *p=join(root,name); if (p) { default_env(key,p); free(p); }
}
static int file(const char *path) { struct stat st; return stat(path,&st)==0 && (st.st_mode&S_IFMT)==S_IFREG; }
static int make_dirs(const char *path) {
    char *p=copy(path), *s; int rc=0;
    if (!p) return -1;
    for (s=p+1; *s; ++s) if (*s=='/' || *s=='\\') {
        char saved=*s; *s=0;
        if (!(strlen(p)==2 && p[1]==':') && mkdir_one(p) && errno!=EEXIST) { rc=-1; break; }
        *s=saved;
    }
    if (!rc && mkdir_one(p) && errno!=EEXIST) rc=-1;
    free(p); return rc;
}

static int probe_in_process(const char *directory,const char *library) {
    api a; void *module=NULL; crexxsaa_context *ctx=NULL;
    char *cache=NULL,*source=NULL,*standard=NULL,*rxc=NULL,*rxas=NULL;
    FILE *output=NULL,*null_file=NULL; int saved_out=-1,saved_err=-1,rc=-1,program=0;
    static const char program_text[]="options levelb\nreturn 27\n";
    null_file=fopen(
#ifdef _WIN32
        "NUL",
#else
        "/dev/null",
#endif
        "w");
    if (null_file) {
        fflush(stdout); fflush(stderr);
        saved_out=dup(fileno(stdout)); saved_err=dup(fileno(stderr));
        dup2(fileno(null_file),fileno(stdout)); dup2(fileno(null_file),fileno(stderr));
    }
#ifdef _WIN32
    module=(void *)LoadLibraryExA(library,NULL,LOAD_WITH_ALTERED_SEARCH_PATH);
#define SYMBOL(name) (void *)GetProcAddress((HMODULE)module,name)
#else
    module=dlopen(library,RTLD_NOW|RTLD_LOCAL);
#define SYMBOL(name) dlsym(module,name)
#endif
    if (!module) goto done;
#define LOAD(name,result,args) a.name=(result (*) args)SYMBOL(#name); if (!a.name) goto done;
    THE_CREXX_FUNCTIONS(LOAD)
#undef LOAD
#undef SYMBOL
    standard=join(directory,"library.rxbin"); rxc=join(directory,"rxc" EXE); rxas=join(directory,"rxas" EXE);
    cache=the_crexx_cache_directory();
    if (!standard || !rxc || !rxas || !cache || make_dirs(cache)) goto done;
    source=join(cache,"the-runtime-check.the");
    if (!source) goto done;
    /* Avoid source mtime churn on successful repeated probes. */
    {
        char text[sizeof(program_text)]={0};
        FILE *input=fopen(source,"rb");
        if (input) { fread(text,1,sizeof(text)-1,input); fclose(input); }
        if (strcmp(text,program_text)) {
            output=fopen(source,"wb");
            if (!output) goto done;
            if (fwrite(program_text,1,sizeof(program_text)-1,output)!=sizeof(program_text)-1) goto done;
            fclose(output); output=NULL;
        }
    }
    if (a.crexxsaa_create(directory,standard,&ctx) || !ctx) goto done;
    if (a.crexxsaa_set_compiler(ctx,rxc,rxas,directory) || a.crexxsaa_set_cache_dir(ctx,cache)) goto done;
    rc=a.crexxsaa_run_source(ctx,source,"THE-runtime-check",0,0,NULL,&program);
    if (rc || program!=27) rc=-1;
done:
    if (output) fclose(output);
    if (ctx) a.crexxsaa_destroy(ctx);
    if (module) {
#ifdef _WIN32
        FreeLibrary((HMODULE)module);
#else
        dlclose(module);
#endif
    }
    free(cache); free(source); free(standard); free(rxc); free(rxas);
    fflush(stdout); fflush(stderr);
    if (saved_out!=-1) { dup2(saved_out,fileno(stdout)); close(saved_out); }
    if (saved_err!=-1) { dup2(saved_err,fileno(stderr)); close(saved_err); }
    if (null_file) fclose(null_file);
    return rc==0;
}

static int probe(const char *directory,const char *library) {
    /* An installed runtime must not be able to crash the editor's launcher. */
#ifdef _WIN32
    const char *arguments[]={self_path,"--check-cohort",directory,library,NULL};
    return the_crexx_spawnvp(self_path,arguments)==0;
#else
    int status; pid_t child=fork();
    if (child==0) _exit(probe_in_process(directory,library) ? 0 : 1);
    if (child<0) return 0;
    while (waitpid(child,&status,0)<0) if (errno!=EINTR) return 0;
    return WIFEXITED(status) && WEXITSTATUS(status)==0;
#endif
}

static char *try_directory(const char *directory,const char *bundle,char **library) {
    char canonical[4096], *path;
    const char *required[]={"rxc" EXE,"rxas" EXE,"crexx" EXE,"library.rxbin","rxcexits.rxbin",NULL};
    const char *libraries[]={THE_CREXX_RUNTIME_NAME,
#ifdef _WIN32
        "crexxsaa.dll","libcrexxsaa.dll",
#elif defined(__APPLE__)
        "libcrexxsaa.dylib",
#else
        "libcrexxsaa.so",
#endif
        NULL};
    size_t i;
    if (!directory || !*directory) return NULL;
#ifdef _WIN32
    if (!_fullpath(canonical,directory,sizeof(canonical))) return NULL;
    for (i=0; canonical[i]; ++i) if (canonical[i]=='\\') canonical[i]='/';
    if (same(canonical,bundle)) return NULL;
#else
    if (!realpath(directory,canonical)) return NULL;
    if (!strcmp(canonical,bundle)) return NULL;
#endif
    /* Do not mistake another THE bundle on PATH for a standalone installation. */
    if (strstr(canonical,"/libexec/the/crexx")) return NULL;
    for (i=0; required[i]; ++i) {
        path=join(canonical,required[i]);
        if (!path || !file(path)) { free(path); return NULL; }
        free(path);
    }
    for (i=0; libraries[i]; ++i) {
        path=join(canonical,libraries[i]);
        if (path && file(path) && probe(canonical,path)) { *library=path; return copy(canonical); }
        free(path);
    }
    return NULL;
}
static char *try_root(const char *root,const char *bundle,char **library) {
    char *bin,*result;
    if (!root || !*root) return NULL;
    bin=join(root,"bin"); result=bin ? try_directory(bin,bundle,library) : NULL; free(bin);
    return result ? result : try_directory(root,bundle,library);
}
static char *find_installed(const char *bundle,char **library) {
    char *result=NULL,*paths,*item,*next;
    const char *home=env("THE_CREXX_HOME");
    if (home) return try_root(home,bundle,library); /* An explicit location does not select some other installation. */
    result=try_root(env("CREXX_HOME"),bundle,library);
#ifdef _WIN32
    if (!result) {
        HKEY keys[]={HKEY_CURRENT_USER,HKEY_LOCAL_MACHINE}; size_t i;
        for (i=0; i<2 && !result; ++i) {
            char root[4096]; DWORD bytes=sizeof(root);
            if (RegGetValueA(keys[i],"Software\\CREXX\\CREXX","InstallDir",RRF_RT_REG_SZ|RRF_SUBKEY_WOW6464KEY,NULL,root,&bytes)==ERROR_SUCCESS)
                result=try_root(root,bundle,library);
        }
    }
#endif
    paths=copy(env("PATH") ? env("PATH") : "");
    item=paths;
    while (!result && item && *item) {
        next=strchr(item,SEP); if (next) *next++=0;
        if (*item=='"' && strlen(item)>1 && item[strlen(item)-1]=='"') { item[strlen(item)-1]=0; ++item; }
        result=try_directory(item,bundle,library); item=next;
    }
    free(paths);
#ifndef _WIN32
    if (!result && env("HOME")) {
        char *local=join(env("HOME"),".local"); result=try_root(local,bundle,library); free(local);
    }
    if (!result) result=try_root("/usr/local/lib/crexx",bundle,library);
    if (!result) result=try_root("/usr/local",bundle,library);
    if (!result) result=try_root("/opt/homebrew",bundle,library);
#endif
    return result;
}
static void json_string(const char *value) {
    const unsigned char *s=(const unsigned char *)(value ? value : ""); putchar('"');
    for (; *s; ++s) { if (*s=='"' || *s=='\\') putchar('\\'); if (*s<32) printf("\\u%04x",*s); else putchar(*s); }
    putchar('"');
}

int main(int argc,char **argv) {
    int info=argc>1 && !strcmp(argv[1],"--info"), root_index=info ? 2 : 1, i,installed=0;
    char preference[32]="BUNDLED", *bundle,*runtime=NULL,*library=NULL,*config,*path,*cache,*editor;
    const char *configured=env("THE_CREXX_RUNTIME"), *engine=env("THE_REXX_INTERPRETER");
    char **arguments; FILE *input;
#ifdef _WIN32
    SetErrorMode(SEM_FAILCRITICALERRORS|SEM_NOOPENFILEERRORBOX);
#endif
    self_path=argv[0];
    if (argc==4 && !strcmp(argv[1],"--check-cohort"))
        return !the_crexx_configure_cache() && probe_in_process(argv[2],argv[3]) ? 0 : 1;
    if (argc<=root_index) { fprintf(stderr,"Usage: the-launcher [--info] PACKAGE_ROOT [editor arguments]\n"); return 2; }
    bundle=join(argv[root_index],"libexec/the/crexx");
    config=join(argv[root_index],"share/the/runtime-preference");
    if (!bundle || !config) return 1;
    input=fopen(config,"r"); if (input) { if (fscanf(input,"%31s",preference)!=1) strcpy(preference,"BUNDLED"); fclose(input); }
    if (configured) snprintf(preference,sizeof(preference),"%s",configured);
    if (!same(preference,"BUNDLED") && !same(preference,"INSTALLED")) { fprintf(stderr,"THE: THE_CREXX_RUNTIME must be BUNDLED or INSTALLED.\n"); return 2; }
    if (the_crexx_configure_cache() && (!engine || same(engine,"CREXX"))) {
        fprintf(stderr,"THE: Set THE_CREXX_CACHE_DIR to a writable cache directory.\n"); return 1;
    }
    if (same(preference,"INSTALLED") && (!engine || same(engine,"CREXX"))) {
        runtime=find_installed(bundle,&library);
        if (runtime) installed=1;
        else if (!info) fprintf(stderr,"THE: Installed cREXX unavailable; using bundled cREXX.\n");
    }
    if (!runtime) { runtime=copy(bundle); library=join(runtime,THE_CREXX_RUNTIME_NAME); }
    if (!runtime || !library) return 1;
    default_path("THE_HOME_DIR",argv[root_index],"share/the");
    default_path("THE_DRIVER_PATH",argv[root_index],"lib/the/drivers");
    default_env("THE_CREXX_LIBRARY",library);
    default_env("THE_CREXX_LOCATION",runtime); default_env("THE_CREXX_IMPORT_DIR",runtime);
    default_path("THE_CREXX_RXC",runtime,"rxc" EXE); default_path("THE_CREXX_RXAS",runtime,"rxas" EXE);
    default_path("THE_CREXX_LIBRARY_RXBIN",runtime,"library.rxbin");
    path=malloc(strlen(runtime)+strlen(env("PATH") ? env("PATH") : "")+2);
    if (!path) return 1;
    sprintf(path,"%s%c%s",runtime,SEP,env("PATH") ? env("PATH") : ""); set("PATH",path); free(path);
    cache=the_crexx_cache_directory();
    if (info) {
        printf("{\"preference\":"); json_string(preference);
        printf(",\"selected\":\"%s\",\"directory\":",installed ? "INSTALLED" : "BUNDLED"); json_string(runtime);
        printf(",\"cache\":"); json_string(cache); printf("}\n"); return 0;
    }
    free(cache); free(bundle); free(config); free(library); free(runtime);
    editor=join(argv[root_index],"libexec/the/the" EXE);
    arguments=calloc((size_t)(argc-root_index+1),sizeof(*arguments));
    if (!editor || !arguments) return 1;
    arguments[0]=editor;
    for (i=root_index+1; i<argc; ++i) arguments[i-root_index]=argv[i];
#ifdef _WIN32
    return (int)the_crexx_spawnvp(editor,(const char *const *)arguments);
#else
    execv(editor,arguments); perror("THE launcher"); return 1;
#endif
}
