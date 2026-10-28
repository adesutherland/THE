#include "the.h"
#include "vars.h"
#include "proto.h"
#include "editorsource.h"
#include "frontendpolicy.h"
#include "macroruntime.h"
#include "tools.h"
#include <ctype.h>
#include <errno.h>
#include <sys/stat.h>

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
static int initialised, available;
static char profile_ready[THE_TOOL_PATH], profile_active[THE_TOOL_PATH];
static unsigned profile_source, profile_revision;
int the_crexx_console_capture(int);
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
}
int the_tools_backend_available(void) { return available; }
static void ensure(void) {
#ifndef __EMSCRIPTEN__
   the_tools_native_initialise();
#endif
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
static short launch(const char *id, int argc, const char *const *argv, const char *image) {
   TheToolRequest r = {0};
   const TheEditorSource *s;
   const TheToolDefinition *t;
   char name[THE_TOOL_PATH], error[1024];
   ensure();
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
      char command[THE_TOOL_PATH + 8];
      if (!the_macro_uses_crexx())
         return RC_INVALID_ENVIRON;
      snprintf(command, sizeof(command), "\"%s\"", r.tool.image);
      if (!the_crexx_console_capture(1))
         return RC_INVALID_ENVIRON;
      rc = execute_macro((CHARTYPE *)command, FALSE, &macro_rc);
      the_crexx_console_capture(0);
      return rc == RC_OK ? macro_rc : rc;
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
   if (!the_tool_launch(&r, error, sizeof(error))) {
      display_error(0, (CHARTYPE *)error, FALSE);
      return RC_INVALID_OPERAND;
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
static void registry_restore(void);
static short activate(void) {
   short rc, macro_rc = 0;
   char command[THE_TOOL_PATH + 16];
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
   snprintf(command, sizeof(command), "\"%s\"", profile_ready);
   if (!the_crexx_console_capture(1))
      return RC_INVALID_ENVIRON;
   rc = execute_macro((CHARTYPE *)command, FALSE, &macro_rc);
   the_crexx_console_capture(0);
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
int the_tools_profile_restore(void) {
   char path[THE_TOOL_PATH], command[THE_TOOL_PATH + 8], cohort[100];
   FILE *f;
   short macro_rc = 0, rc;
   ensure();
   registry_restore();
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
   snprintf(command, sizeof(command), "\"%s\"", path);
   if (!the_crexx_console_capture(1))
      return RC_INVALID_ENVIRON;
   rc = execute_macro((CHARTYPE *)command, FALSE, &macro_rc);
   the_crexx_console_capture(0);
   if (rc == RC_OK && !macro_rc) {
      snprintf(profile_active, sizeof(profile_active), "%s", path);
      return 1;
   }
   return 0;
}
static short action(PendingTool *p) {
   const char *a = p->action;
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
#ifdef __EMSCRIPTEN__
      return RC_OK;
#else
      return the_tools_native_language(t) ? RC_OK : RC_INVALID_ENVIRON;
#endif
   }
   if (!strcmp(a, "register")) {
      TheToolDefinition t = {0};
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
   } else if (n && !strcmp(words[0], "console")) {
      const TheToolConsole *c = the_tool_console();
      int off = n > 1 && !strcmp(words[1], "off");
      the_tool_console_visibility(!off, off ? 0 : !c->focused);
      rc = RC_OK;
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
static void registry_restore(void) {
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
#ifndef __EMSCRIPTEN__
      if (!strcmp(t.role, "language"))
         the_tools_native_language(&t);
#endif
   }
   fclose(f);
}
