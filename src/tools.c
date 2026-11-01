#include "the.h"
#include "vars.h"
#include "proto.h"
#include "editorsource.h"
#include "editordocument.h"
#include "frontendpolicy.h"
#include "macroruntime.h"
#include "panelui.h"
#include "tools.h"
#include "workbench.h"
#include "workbenchruntime.h"
#include "workbenchfileio.h"
#include <ctype.h>
#include <errno.h>
#include <sys/stat.h>
#if !defined(_WIN32) && !defined(__EMSCRIPTEN__)
#include <unistd.h>
#endif

typedef struct PendingTool {
   unsigned id;
   char *action, *tool, *text;
   int argc;
   char **argv;
   struct PendingTool *next;
} PendingTool;
#ifndef THE_TOOL_COHORT
#define THE_TOOL_COHORT "unqualified"
#endif
static PendingTool *pending;
static unsigned next_request, pending_count;
static int deferred_menu_run;
static int initialised, available;
static char profile_ready[THE_TOOL_PATH], profile_active[THE_TOOL_PATH];
static unsigned profile_source, profile_revision;
static TheToolDefinition restored_languages[THE_TOOL_LIMIT];
static size_t restored_language_count;
static short file_launch_rc;
static const char *workbench_native_dependency(const TheWorkbenchTool *tool) {
#if defined(__APPLE__) && (defined(__arm64__) || defined(__aarch64__))
   struct stat info;
   if(!tool || tool->executable[0]!='/' || stat(tool->executable,&info)
   || !S_ISREG(info.st_mode) || access(tool->executable,X_OK)) return "capability-unavailable";
   return "ok";
#else
   (void)tool;return "unsupported-host";
#endif
}
static int workbench_source_identity(const char *id,uint64_t *document) {
   const TheWorkbenchAction *action=the_workbench_action_find(id);
   if(!action || action->kind!=THE_WORKBENCH_ACTION_TOOL) return 0;
   *document=the_editor_document_active();return 1;
}
int the_crexx_console_capture(int);
static int workbench_file_authority(const char *canonical,int writing,int published,char *error,size_t capacity) {
   VIEW_DETAILS *view;
   (void)published;
   if(the_frontend_policy_enabled()) { snprintf(error,capacity,"Native FILE routes are unavailable in restricted sessions");return 0; }
   if(writing && (the_readonly || READONLYx==READONLY_FORCE)) { snprintf(error,capacity,"Session is readonly");return 0; }
#if !defined(_WIN32) && !defined(__EMSCRIPTEN__)
   if(writing) for(view=vd_first;view;view=view->next) {
      FILE_DETAILS *file=view->file_for_view; char path[4096],parent[4096],resolved[4096],*slash;
      if(!file || !file->fpath || !file->fname) continue;
      if(snprintf(path,sizeof(path),"%s%s",(char *)file->fpath,(char *)file->fname)>=(int)sizeof(path)) continue;
      /* A new clean editor buffer can name an absent file. Resolve its parent
       * too; output refuses every open target until reload/conflict state is
       * available, preserving pending and staged edits without side effects. */
      if(!realpath(path,resolved)) {
         strcpy(parent,path); slash=strrchr(parent,'/'); if(!slash) continue; *slash=0;
         if(!realpath(parent,resolved)) continue;
         if(strlen(resolved)+strlen(slash+1)+2>=sizeof(path)) continue;
         snprintf(path,sizeof(path),"%s/%s",resolved,slash+1);strcpy(resolved,path);
      }
      if(!strcmp(resolved,canonical)) { snprintf(error,capacity,"Output target is open in the editor; close it before running this route");return 0; }
   }
#else
   (void)view;
#endif
   return 1;
}
void the_tools_fileio_initialize(void) { the_workbench_fileio_authority(workbench_file_authority); }
static int workbench_io_begin(unsigned id,const char *tool,void **binding,const void **bytes,size_t *length,int *finite,char *error,size_t capacity) {
   TheWorkbenchFileRequest request={0}; size_t i;
   *binding=NULL;*bytes=NULL;*length=0;*finite=0;
   for(i=0;i<the_workbench_route_count();i++) {
      const TheWorkbenchRoute *r=the_workbench_route_at(i);
      if(strcmp(r->tool,tool)) continue;
      if(r->kind==THE_WORKBENCH_ROUTE_FILE_INPUT) request.input=r->path;
      if(r->kind==THE_WORKBENCH_ROUTE_FILE_OUTPUT) { request.output=r->path;request.stream=r->stream; }
   }
   if(!request.input && !request.output) return 1;
   if(the_frontend_policy_enabled() || !the_workbench_fileio_supported()) { snprintf(error,capacity,"unsupported-feature: Native FILE routes are unavailable in this host");file_launch_rc=RC_INVALID_ENVIRON;return 0; }
   *binding=the_workbench_fileio_begin(id,&request,error,capacity);
   if(!*binding) { const TheWorkbenchFileReceipt *r=the_workbench_fileio_receipt();file_launch_rc=!strcmp(r->code,"limit-exceeded") ? 94 : !strcmp(r->code,"invalid-operand") ? RC_INVALID_OPERAND : RC_INVALID_ENVIRON;the_workbench_result(r->code,file_launch_rc,r->message);return 0; }
   if(request.input) { *finite=1;*bytes=the_workbench_fileio_input(*binding,length); }
   return 1;
}
static int workbench_io_raw(void *binding,unsigned id,int fd,const void *bytes,size_t length,char *error,size_t capacity) {
   (void)id;return !binding || the_workbench_fileio_output(binding,fd,bytes,length,error,capacity);
}
static int workbench_io_end(void *binding,unsigned id,int success,char *error,size_t capacity) {
   int good; (void)id;
   good=the_workbench_fileio_end(binding,success,error,capacity);
   if(!good) { const TheWorkbenchFileReceipt *r=the_workbench_fileio_receipt();the_workbench_result(r->code,-3,r->message); }
   return good;
}
static int workbench_tool_lookup(const char *id, TheWorkbenchToolInfo *info, void *context) {
   const TheToolDefinition *tool = the_tool_find(id);
   (void)context;
   if (!tool || !info)
      return 0;
   info->role = tool->role;
   info->available = tool->available;
   return 1;
}
static int workbench_job_begin(unsigned id, const char *tool, void **binding,
                                char *error, size_t capacity) {
   *binding = the_workbench_routes_bind(tool, id, error, capacity);
   return *binding != NULL;
}
static void workbench_job_text(void *binding, unsigned id, int fd, uint64_t sequence,
                               const void *bytes, size_t length) {
   (void)id;
   /* The core lease records an inspectable route error on failure. Observers
    * remain non-reentrant; log publication never implicitly stops a program. */
   (void)the_workbench_route_output(binding, fd, sequence, bytes, length);
}
static void workbench_job_end(void *binding, unsigned id) {
   (void)id;
   the_workbench_routes_release(binding);
}
static void workbench_tools_register(void) {
   const TheToolObserver value = {workbench_job_begin, workbench_job_text, workbench_job_end};
   const TheToolIO io={workbench_io_begin,workbench_io_raw,workbench_io_end};
   the_tools_fileio_initialize();
   the_workbench_set_tool_lookup(workbench_tool_lookup, NULL);
   the_workbench_set_native_tool_resolver(workbench_native_dependency);
   the_input_workbench_source_lookup(workbench_source_identity);
   /* An active job retains its already installed observer and pinned routes. */
   (void)the_tool_host_observer(&value);
   (void)the_tool_host_io(&io);
}
static char *copy(const char *s) {
   size_t n = strlen(s ? s : "");
   char *p = malloc(n + 1);
   if (p)
      memcpy(p, s ? s : "", n + 1);
   return p;
}
static void dispose(PendingTool *p) {
   int i;
   free(p->action);
   free(p->tool);
   free(p->text);
   if (p->argv)
      for (i = 0; i < p->argc; i++)
         free(p->argv[i]);
   free(p->argv);
   free(p);
}
static void builtin(const char *id, const char *label, const char *role, const char *entry,
                    const char *image, const char *extensions) {
   TheToolDefinition t = {0};
   snprintf(t.id, sizeof(t.id), "%s", id);
   snprintf(t.label, sizeof(t.label), "%s", label);
   snprintf(t.role, sizeof(t.role), "%s", role);
   snprintf(t.entry, sizeof(t.entry), "%s", entry);
   snprintf(t.image, sizeof(t.image), "%s", image);
   snprintf(t.extensions, sizeof(t.extensions), "%s", extensions);
   t.available = available;
   the_tool_register(&t);
}
void the_tools_initialise(const TheToolAdapter *a, int enabled) {
   if (initialised)
      return;
   initialised = 1;
   available = enabled;
   the_tool_host_reset();
   the_tool_host_adapter(a);
   builtin("classic-rexx", "Classic Rexx", "runner", "classic-source", "@classic", ".rexx,.rex");
   builtin("rxbin", "RXBIN program", "program", "program", "", ".rxbin");
   builtin("profile-compiler", "Profile compiler", "compiler", "profile-source", "@compiler",
           ".the,.crexx");
   builtin("classic-highlight", "Classic Rexx highlighter", "language", "dslsh",
           "@classic-highlight", ".rexx,.rex");
   workbench_tools_register();
}
int the_tools_backend_available(void) { return available; }
static void ensure(void) {
#ifndef __EMSCRIPTEN__
   the_tools_native_initialise();
#endif
}
void the_tools_workbench_initialize(void) { ensure();workbench_tools_register(); }
short the_tools_workbench_run(const char *id,const TheWorkbenchArguments *arguments) {
   const TheWorkbenchTool *tool;
   const TheEditorSource *source;
   const char *reason,*argv[THE_WORKBENCH_ARG_MAX];
   TheToolRequest request={0};
   char filename[THE_TOOL_PATH];
   static char error[1024];
   unsigned i;
   ensure();workbench_tools_register();
   reason=the_workbench_native_host_reason();
   if(strcmp(reason,"ok")) return the_workbench_result(reason,RC_INVALID_ENVIRON,"Native process execution is unavailable in this host/session");
   if(the_workbench_activation_active()) return the_workbench_result("activation-pending",RC_INVALID_ENVIRON,"Tool execution requires completed profile activation");
   if(in_macro || the_panel_active) return the_workbench_result("busy-modal",RC_INVALID_ENVIRON,"Finish the macro or modal READ before running a source tool");
   if(the_tool_console()->running) return the_workbench_result("busy-job",RC_INVALID_ENVIRON,"Stop the current program first");
   tool=the_workbench_tool_find(id);
   if(!tool) return the_workbench_result("missing-object",RC_INVALID_OPERAND,"No published native tool has that ID");
   reason=the_workbench_native_tool_reason(tool);
   if(strcmp(reason,"ok")) return the_workbench_result(reason,RC_INVALID_ENVIRON,"Configured native executable is unavailable");
   if(!the_workbench_arguments_valid(arguments)) return the_workbench_result("invalid-operand",RC_INVALID_OPERAND,"Invalid native argument vector");
   source=the_editor_source_prepare();
   if(!source) return the_workbench_result("invalid-operand",RC_INVALID_OPERAND,the_editor_source_error());
   strcpy(request.tool.id,tool->id);strcpy(request.tool.label,tool->id);
   strcpy(request.tool.role,"runner");strcpy(request.tool.entry,"native-process");
   strcpy(request.tool.image,tool->executable);request.tool.available=1;
   request.source=source->bytes;request.source_length=source->length;
   request.source_id=source->id;request.revision=source->revision;
   snprintf(filename,sizeof(filename),"%s%s",(char *)CURRENT_FILE->fpath,(char *)CURRENT_FILE->fname);
   request.filename=filename;request.argc=(int)arguments->argc;request.argv=argv;
   for(i=0;i<arguments->argc;i++) argv[i]=arguments->bytes+arguments->offsets[i];
   file_launch_rc=0;
   if(!the_tool_launch(&request,error,sizeof(error))) {
      if(file_launch_rc) return file_launch_rc;
      reason=the_workbench_native_tool_reason(tool);
      return the_workbench_result(strcmp(reason,"ok")?reason:"host-failed",strcmp(reason,"ok")?RC_INVALID_ENVIRON:RC_SYSTEM_ERROR,error);
   }
   return the_workbench_result("ok",RC_OK,"");
}
int the_tools_prepare_input(const char *action, const char *tool, const char *text, int argc,
                            const char *const *argv, TheInputEvent *event) {
   PendingTool *p;
   int i;
   size_t total = 0;
   char command[80];
   if (pending_count >= 128 || argc < 0 || argc > 128 || !action || strlen(action) > 32 ||
       strlen(tool ? tool : "") > 63 || strlen(text ? text : "") > 1048576)
      return 0;
   for (i = 0; i < argc; i++) {
      size_t n = strlen(argv[i]);
      if (n > 1048576 - total)
         return 0;
      total += n;
   }
   p = calloc(1, sizeof(*p));
   if (!p)
      return 0;
   p->argc = argc;
   p->argv = calloc(argc ? argc : 1, sizeof(char *));
   p->action = copy(action);
   p->tool = copy(tool);
   p->text = copy(text);
   p->id = ++next_request;
   if (!p->argv || !p->action || !p->tool || !p->text) {
      dispose(p);
      return 0;
   }
   for (i = 0; i < argc; i++)
      if (!(p->argv[i] = copy(argv[i]))) {
         dispose(p);
         return 0;
      }
   snprintf(command, sizeof(command), "rx dispatch %u", p->id);
   if (!the_input_event_from_command(command, event)) {
      dispose(p);
      return 0;
   }
   p->next = pending;
   pending = p;
   pending_count++;
   return 1;
}
int the_tools_take_menu_run(TheInputEvent *event) {
   if (!deferred_menu_run || in_macro || the_panel_active ||
       !the_input_event_from_command("rx run", event))
      return 0;
   deferred_menu_run = 0;
   return 1;
}
static int safe_image(const char *image, char *out, size_t cap) {
   char error[256];
   int readonly;
   if (!image || !*image)
      return 0;
   if (image[0] == '@') {
      snprintf(out, cap, "%s", image);
      return 1;
   }
   if (!the_frontend_policy_resolve_path(image, 0, out, cap, &readonly, error, sizeof(error))) {
      display_error(0, (CHARTYPE *)error, FALSE);
      return 0;
   }
   return 1;
}
/* Profile execution has an outer definition scope even when invoked through
 * a compiled tool. Its stable source identity survives immutable build names. */
static short run_profile_image(const char *image, const char *identity, short *macro_rc)
{
   char command[THE_TOOL_PATH + 8];
   short rc, finish;
   workbench_tools_register();
#if defined(THE_WASM)
   rc = the_workbench_activation_begin_untrusted(identity);
#else
   rc = the_frontend_policy_enabled() ? the_workbench_activation_begin_untrusted(identity)
                                      : the_workbench_activation_begin(identity);
#endif
   if (rc != RC_OK) return rc;
   if (!the_crexx_console_capture(1))
   {
      the_workbench_activation_end(0);
      return RC_INVALID_ENVIRON;
   }
   snprintf(command,sizeof(command),"\"%s\"",image);
   rc = execute_macro((CHARTYPE *)command,FALSE,macro_rc);
   the_crexx_console_capture(0);
   finish = the_workbench_activation_end(rc == RC_OK && *macro_rc == 0);
   return rc != RC_OK ? rc : *macro_rc ? *macro_rc : finish;
}
static short launch(const char *id, int argc, const char *const *argv, const char *image) {
   TheToolRequest r = {0};
   const TheEditorSource *s;
   const TheToolDefinition *t;
   char name[THE_TOOL_PATH], error[1024];
   ensure();
   workbench_tools_register();
   t = the_tool_find(id && *id ? id : "classic-rexx");
   if (!t) {
      display_error(0, (CHARTYPE *)"Unknown tool", FALSE);
      return RC_INVALID_OPERAND;
   }
   r.tool = *t;
   if (!strcmp(r.tool.role, "language")) {
      display_error(0, (CHARTYPE *)"Select a language service with RX LANGUAGE", FALSE);
      return RC_INVALID_OPERAND;
   }
   if (!strcmp(r.tool.role, "profile")) {
      short macro_rc = 0, rc;
      if (!the_macro_uses_crexx())
         return RC_INVALID_ENVIRON;
      rc = run_profile_image(r.tool.image,r.tool.image,&macro_rc);
      return rc;
   }
   if (image && *image && !safe_image(image, r.tool.image, sizeof(r.tool.image)))
      return RC_INVALID_OPERAND;
   if (!strcmp(r.tool.role, "runner") || !strcmp(r.tool.role, "compiler")) {
      s = the_editor_source_prepare();
      if (!s) {
         display_error(0, (CHARTYPE *)the_editor_source_error(), FALSE);
         return RC_INVALID_OPERAND;
      }
      r.source = s->bytes;
      r.source_length = s->length;
      r.source_id = s->id;
      r.revision = s->revision;
      snprintf(name, sizeof(name), "%s%s", CURRENT_FILE->fpath, CURRENT_FILE->fname);
      r.filename = name;
   } else
      r.filename = r.tool.image;
   r.argc = argc;
   r.argv = argv;
   file_launch_rc=0;
   if (!the_tool_launch(&r, error, sizeof(error))) {
      display_error(0, (CHARTYPE *)error, FALSE);
      return file_launch_rc ? file_launch_rc : RC_INVALID_OPERAND;
   }
   return RC_OK;
}
const char *the_tools_profile_path(void) {
   static char path[THE_TOOL_PATH];
   const char *root = getenv("THE_TOOL_CONFIG_DIR");
   const char *home;
   if (root && *root)
      snprintf(path, sizeof(path), "%s/profile.the", root);
   else {
      home = getenv("HOME");
      if (!home || !*home)
         home = getenv("USERPROFILE");
      snprintf(path, sizeof(path), "%s/.crxthe-tools/profile.the", home ? home : ".");
   }
   return path;
}
void the_tools_profile_built(unsigned id, const char *path, unsigned source_id, unsigned revision) {
   if (the_tool_console()->id != id)
      return;
   snprintf(profile_ready, sizeof(profile_ready), "%s", path);
   profile_source = source_id;
   profile_revision = revision;
}
static void profile_directory(void);
static void profile_selection(void);
#ifdef __EMSCRIPTEN__
int the_wasm_flush_workspace(void);
#endif
static void registry_save(void);
static void registry_restore_metadata(void);
static void registry_activate_languages(void);
static void registry_skip_language(const char *id) {
   size_t i;
   for (i = 0; i < restored_language_count; i++)
      if (!strcmp(restored_languages[i].id, id))
         restored_languages[i].id[0] = 0;
}
static short activate(void) {
   short rc, macro_rc = 0;
   const TheEditorSource *s = the_editor_source_prepare();
   if (!*profile_ready) {
      display_error(0, (CHARTYPE *)"Build a profile before activating it", FALSE);
      return RC_INVALID_OPERAND;
   }
   if (!s || s->id != profile_source || s->revision != profile_revision) {
      display_error(
          0, (CHARTYPE *)"The profile changed; build the current source before activation", FALSE);
      return RC_INVALID_OPERAND;
   }
   if (!the_macro_uses_crexx()) {
      display_error(0, (CHARTYPE *)"This profile requires cREXX", FALSE);
      return RC_INVALID_ENVIRON;
   }
   rc = run_profile_image(profile_ready,the_tools_profile_path(),&macro_rc);
   if (rc == RC_OK && macro_rc == 0) {
      snprintf(profile_active, sizeof(profile_active), "%s", profile_ready);
      profile_selection();
#ifdef __EMSCRIPTEN__
      the_wasm_flush_workspace();
#endif
      display_error(0, (CHARTYPE *)"Profile activated", FALSE);
   } else
      display_error(0, (CHARTYPE *)"Profile activation failed; previous selection retained", FALSE);
   return rc;
}
static int profile_file(char *out, size_t cap, const char *name) {
   char *slash;
   snprintf(out, cap, "%s", the_tools_profile_path());
   slash = strrchr(out, '/');
   if (!slash)
      return 0;
   snprintf(slash + 1, cap - (size_t)(slash + 1 - out), "%s", name);
   return 1;
}
static void profile_directory(void) {
   char path[THE_TOOL_PATH], *slash;
   snprintf(path, sizeof(path), "%s", the_tools_profile_path());
   slash = strrchr(path, '/');
   if (!slash)
      return;
   *slash = 0;
#ifdef _WIN32
   _mkdir(path);
#else
   mkdir(path, 0700);
#endif
}
static void profile_selection(void) {
   char path[THE_TOOL_PATH], temporary[THE_TOOL_PATH + 8];
   FILE *f;
   if (!profile_file(path, sizeof(path), "active"))
      return;
   snprintf(temporary, sizeof(temporary), "%s.tmp", path);
   f = fopen(temporary, "wb");
   if (!f)
      return;
   if (fprintf(f, "%s\n%s\n", profile_active, THE_TOOL_COHORT) < 0 || fclose(f)) {
      remove(temporary);
      return;
   }
   rename(temporary, path);
}
void the_tools_restore_metadata(void) {
   ensure();
   if (initialised)
      registry_restore_metadata();
}
int the_tools_profile_restore(void) {
   char path[THE_TOOL_PATH], cohort[100];
   FILE *f;
   short macro_rc = 0, rc;
   the_tools_restore_metadata();
   registry_activate_languages();
   if (available)
      the_crexx_console_capture(2);
   if (!the_macro_uses_crexx() || !profile_file(path, sizeof(path), "active"))
      return 0;
   f = fopen(path, "rb");
   if (!f)
      return 0;
   if (!fgets(path, sizeof(path), f)) {
      fclose(f);
      return 0;
   }
   if (!fgets(cohort, sizeof(cohort), f)) {
      fclose(f);
      return 0;
   }
   fclose(f);
   cohort[strcspn(cohort, "\r\n")] = 0;
   if (strcmp(cohort, THE_TOOL_COHORT))
      return 0;
   path[strcspn(path, "\r\n")] = 0;
   /* Only an immutable build inside this configuration directory is restored. */
   {
      char base[THE_TOOL_PATH], *slash;
      profile_file(base, sizeof(base), "");
      slash = strrchr(path, '/');
      if (strncmp(path, base, strlen(base)) || !slash || strncmp(slash + 1, "build-", 6) ||
          strchr(slash + 1, '\\'))
         return 0;
   }
   rc = run_profile_image(path,the_tools_profile_path(),&macro_rc);
   if (rc == RC_OK && !macro_rc) {
      snprintf(profile_active, sizeof(profile_active), "%s", path);
      return 1;
   }
   return 0;
}
static short action(PendingTool *p) {
   const char *a = p->action;
   if (!strcmp(a, "menu-run")) {
      /* The native popup is a macro. Launch on the next editor input event,
       * after the macro has returned and the source snapshot is safe. */
      if (in_macro) {
         deferred_menu_run = 1;
         return RC_OK;
      }
      return launch("", 0, NULL, NULL);
   }
   if (!strcmp(a, "run")) {
      if (*p->tool && !the_tool_find(p->tool))
         return launch("rxbin", p->argc, (const char *const *)p->argv, p->tool);
      return launch(p->tool, p->argc, (const char *const *)p->argv, NULL);
   }
   if (!strcmp(a, "rxbin"))
      return launch("rxbin", p->argc, (const char *const *)p->argv, p->text);
   if (!strcmp(a, "build"))
      return launch("profile-compiler", 0, NULL, NULL);
   if (!strcmp(a, "activate"))
      return activate();
   if (!strcmp(a, "stop")) {
      the_tool_stop("Stopped");
      return RC_OK;
   }
   if (!strcmp(a, "clear")) {
      the_tool_clear();
      return RC_OK;
   }
   if (!strcmp(a, "input")) {
      size_t n = strlen(p->text);
      char *line = malloc(n + 1);
      int ok;
      if (!line)
         return RC_OUT_OF_MEMORY;
      memcpy(line, p->text, n);
      line[n] = '\n';
      ok = the_tool_input(line, n + 1);
      free(line);
      return ok ? RC_OK : RC_INVALID_ENVIRON;
   }
   if (!strcmp(a, "eof"))
      return the_tool_eof() ? RC_OK : RC_INVALID_ENVIRON;
   if (!strcmp(a, "profile")) {
      char command[THE_TOOL_PATH + 16];
      FILE *f;
      profile_directory();
      f = fopen(the_tools_profile_path(), "rb");
      if (f)
         fclose(f);
      else {
         f = fopen(the_tools_profile_path(), "wb");
         if (!f)
            return RC_ACCESS_DENIED;
         fputs("/* THE user profile: build, then activate. */\noptions levelb\naddress THE\n'set "
               "number on';\nsay 'User profile activated'\nreturn 0\n",
               f);
         fclose(f);
      }
      snprintf(command, sizeof(command), "edit \"%s\"", the_tools_profile_path());
      return command_line((CHARTYPE *)command, COMMAND_ONLY_FALSE);
   }
   if (!strcmp(a, "language")) {
      const TheToolDefinition *t = the_tool_find(p->tool);
      if (!t || strcmp(t->role, "language"))
         return RC_INVALID_OPERAND;
      registry_skip_language(t->id);
#ifdef __EMSCRIPTEN__
      return RC_OK;
#else
      return the_tools_native_language(t) ? RC_OK : RC_INVALID_ENVIRON;
#endif
   }
   if (!strcmp(a, "register")) {
      TheToolDefinition t = {0};
      if(p->argc && the_workbench_tool_find(p->argv[0])) return RC_INVALID_OPERAND;
      if (p->argc < 3 || strlen(p->argv[0]) > 30 ||
          (strcmp(p->argv[1], "program") && strcmp(p->argv[1], "profile") &&
           strcmp(p->argv[1], "language")) ||
          (p->argc > 3 && strlen(p->argv[3]) >= sizeof(t.extensions)) ||
          (p->argc > 4 && strlen(p->argv[4]) >= sizeof(t.label)))
         return RC_INVALID_OPERAND;
      snprintf(t.id, sizeof(t.id), "%s", p->argv[0]);
      snprintf(t.label, sizeof(t.label), "%s", p->argc > 4 ? p->argv[4] : p->argv[0]);
      snprintf(t.role, sizeof(t.role), "%s", p->argv[1]);
      if (!safe_image(p->argv[2], t.image, sizeof(t.image)))
         return RC_INVALID_OPERAND;
      snprintf(t.extensions, sizeof(t.extensions), "%s", p->argc > 3 ? p->argv[3] : "");
      snprintf(t.entry, sizeof(t.entry), "%s",
               !strcmp(t.role, "language")  ? "dslsh"
               : !strcmp(t.role, "profile") ? "address-the"
                                            : "program");
      t.available = available ||
                    (!strcmp(t.role, "language") && the_tool_find("classic-highlight")->available);
      if (strpbrk(t.image, "\t\r\n\"") || strpbrk(t.label, "\t\r\n") ||
          strpbrk(t.extensions, "\t\r\n"))
         return RC_INVALID_OPERAND;
      if (!the_tool_register(&t))
         return RC_INVALID_OPERAND;
      registry_skip_language(t.id);
#ifndef __EMSCRIPTEN__
      if (!strcmp(t.role, "language") && !the_tools_native_language(&t))
         return RC_INVALID_ENVIRON;
#endif
      registry_save();
#ifdef __EMSCRIPTEN__
      the_wasm_flush_workspace();
#endif
      return RC_OK;
   }
   return RC_INVALID_OPERAND;
}
short Rx(CHARTYPE *parameters) {
   char *words[132], *buffer = copy((char *)parameters), *p = buffer;
   int n = 0;
   short rc = RC_INVALID_OPERAND;
   PendingTool direct = {0};
   ensure();
   if (!buffer)
      return RC_OUT_OF_MEMORY;
   while (*p && n < 132) {
      char quote = 0, *out;
      while (isspace((unsigned char)*p))
         p++;
      if (!*p)
         break;
      words[n++] = out = p;
      if (*p == '"' || *p == '\'')
         quote = *p++;
      while (*p && (quote ? *p != quote : !isspace((unsigned char)*p)))
         *out++ = *p++;
      if (quote && *p == quote)
         p++;
      if (*p && isspace((unsigned char)*p))
         p++;
      *out = 0;
   }
   /* Command keywords follow THE's case-insensitive convention. Preserve
    * registered IDs, filenames, program arguments and console input bytes. */
   if (n) {
      for (p = words[0]; *p; p++)
         *p = (char)tolower((unsigned char)*p);
      if (n > 1 && (!strcmp(words[0], "console") || !strcmp(words[0], "key")))
         for (p = words[1]; *p; p++)
            *p = (char)tolower((unsigned char)*p);
   }
   if (n && !strcmp(words[0], "dispatch") && n == 2) {
      unsigned id = (unsigned)strtoul(words[1], NULL, 10);
      PendingTool **link = &pending, *found;
      while (*link && (*link)->id != id)
         link = &(*link)->next;
      found = *link;
      if (found) {
         *link = found->next;
         pending_count--;
         rc = action(found);
         dispose(found);
      }
   } else if (n == 1 && !strcmp(words[0], "controls")) {
      rc = the_driver_workbench_focus(TRUE) != THE_WORKBENCH_EDITOR
         ? RC_OK : RC_INVALID_ENVIRON;
   } else if (n && !strcmp(words[0], "console")) {
      const TheToolConsole *c = the_tool_console();
      if (n == 1) {
         /* Keep the historical focus toggle for typed RX CONSOLE. */
         the_tool_console_visibility(1, !c->focused);
         rc = RC_OK;
      } else if (n == 2 && !strcmp(words[1], "on")) {
         the_tool_console_visibility(1, 1);
         rc = RC_OK;
      } else if (n == 2 && !strcmp(words[1], "off")) {
         the_tool_console_visibility(0, 0);
         rc = RC_OK;
      } else {
         display_error(1, (CHARTYPE *)(n > 1 ? words[1] : ""), FALSE);
         rc = RC_INVALID_OPERAND;
      }
   } else if (n && !strcmp(words[0], "type") && n == 2) {
      rc = the_tool_console_key((uint32_t)strtoul(words[1], NULL, 10), 0) ? RC_OK
                                                                          : RC_INVALID_OPERAND;
   } else if (n && !strcmp(words[0], "key") && n == 2) {
      int key = !strcmp(words[1], "enter")       ? 13
                : !strcmp(words[1], "backspace") ? 8
                : !strcmp(words[1], "escape")    ? 27
                : !strcmp(words[1], "eof")       ? 4
                                                 : 3;
      rc = the_tool_console_key(0, key) ? RC_OK : RC_INVALID_OPERAND;
   } else if (n && !strcmp(words[0], "status")) {
      display_error(0, (CHARTYPE *)the_tool_console()->state, FALSE);
      rc = RC_OK;
   } else if (n) {
      direct.action = words[0];
      direct.tool = n > 1 ? words[1] : "";
      direct.text = "";
      direct.argc = n > 2 ? n - 2 : 0;
      direct.argv = words + 2;
      if (!strcmp(words[0], "rxbin")) {
         direct.text = n > 1 ? words[1] : "";
         direct.tool = "rxbin";
      }
      if (!strcmp(words[0], "register")) {
         direct.argc = n - 1;
         direct.argv = words + 1;
      }
      if (!strcmp(words[0], "input")) {
         direct.text = (char *)parameters + 5;
         while (isspace((unsigned char)*direct.text))
            direct.text++;
      }
      rc = action(&direct);
   }
   free(buffer);
   return rc;
}

int the_tools_prepare_lines(const char *a, const char *t, const char *text, const char *arguments,
                            TheInputEvent *e) {
   char *buffer = copy(arguments), *p, *args[128];
   int n = 0, ok;
   if (!buffer)
      return 0;
   p = buffer;
   if (*p) {
      args[n++] = p;
      while (*p) {
         if (*p == '\n') {
            *p = 0;
            if (n == 128) {
               free(buffer);
               return 0;
            }
            args[n++] = p + 1;
         }
         p++;
      }
   }
   ok = the_tools_prepare_input(a, t, text, n, (const char *const *)args, e);
   free(buffer);
   return ok;
}

static void registry_save(void) {
   char path[THE_TOOL_PATH], tmp[THE_TOOL_PATH + 8];
   size_t i;
   FILE *f;
   profile_directory();
   if (!profile_file(path, sizeof(path), "tools.registry"))
      return;
   snprintf(tmp, sizeof(tmp), "%s.tmp", path);
   f = fopen(tmp, "wb");
   if (!f)
      return;
   for (i = 0; i < the_tool_count(); i++) {
      const TheToolDefinition *t = the_tool_at(i);
      if (!*t->image || t->image[0] == '@')
         continue;
      fprintf(f, "%s\t%s\t%s\t%s\t%s\t%s\n", t->id, t->label, t->role, t->entry, t->image,
              t->extensions);
   }
   if (fclose(f))
      remove(tmp);
   else
      rename(tmp, path);
}
static void registry_restore_metadata(void) {
   char path[THE_TOOL_PATH], line[4096];
   FILE *f;
   static int done;
   ensure();
   if (done)
      return;
   done = 1;
   if (!profile_file(path, sizeof(path), "tools.registry"))
      return;
   f = fopen(path, "rb");
   if (!f)
      return;
   while (fgets(line, sizeof(line), f)) {
      char *p = line, *fields[6];
      int i;
      TheToolDefinition t = {0};
      line[strcspn(line, "\r\n")] = 0;
      for (i = 0; i < 6; i++) {
         fields[i] = p;
         p = strchr(p, '\t');
         if (!p)
            break;
         *p++ = 0;
      }
      if (i != 5)
         continue;
      snprintf(t.id, sizeof(t.id), "%s", fields[0]);
      snprintf(t.label, sizeof(t.label), "%s", fields[1]);
      snprintf(t.role, sizeof(t.role), "%s", fields[2]);
      snprintf(t.entry, sizeof(t.entry), "%s", fields[3]);
      snprintf(t.image, sizeof(t.image), "%s", fields[4]);
      snprintf(t.extensions, sizeof(t.extensions), "%s", fields[5]);
      t.available = available ||
                    (!strcmp(t.role, "language") && the_tool_find("classic-highlight")->available);
      if (t.image[0] == '@' || strpbrk(t.image, "\"") || strlen(t.id) > 30)
         continue;
      if (!the_tool_register(&t))
         continue;
      if (!strcmp(t.role, "language") && restored_language_count < THE_TOOL_LIMIT)
         restored_languages[restored_language_count++] = t;
   }
   fclose(f);
}
static void registry_activate_languages(void) {
#ifndef __EMSCRIPTEN__
   size_t i;
   static int done;
   if (done)
      return;
   done = 1;
   for (i = 0; i < restored_language_count; i++) {
      const TheToolDefinition *saved = restored_languages + i;
      const TheToolDefinition *current = the_tool_find(saved->id);
      /* Startup profiles may explicitly replace/select these definitions.
       * Deferred activation never restores old metadata over their changes. */
      if (current && !strcmp(current->role, saved->role) &&
          !strcmp(current->label, saved->label) && !strcmp(current->entry, saved->entry) &&
          !strcmp(current->image, saved->image) && !strcmp(current->extensions, saved->extensions) &&
          current->available == saved->available)
         the_tools_native_language(current);
   }
#endif
}
