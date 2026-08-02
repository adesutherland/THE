#include "frontendpolicy.h"

#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#if defined(_WIN32)
# include <direct.h>
# define frontend_getcwd _getcwd
# define frontend_realpath(path, out) _fullpath((out), (path), THE_FRONTEND_POLICY_PATH_MAX)
# define FRONTEND_PATH_SEPARATOR ';'
#else
# include <unistd.h>
# define frontend_getcwd getcwd
# define frontend_realpath(path, out) realpath((path), (out))
# define FRONTEND_PATH_SEPARATOR ':'
#endif

#include "the.h"
#include "vars.h"

typedef struct
{
   char path[THE_FRONTEND_POLICY_PATH_MAX + 1];
   int readonly;
} TheFrontendPolicyRoot;

static struct
{
   int enabled;
   TheFrontendPolicyRoot roots[THE_FRONTEND_POLICY_ROOT_MAX];
   size_t root_count;
} frontend_policy;

static int frontend_policy_equal_ci(const char *left, const char *right)
{
   if (left == NULL || right == NULL)
      return 0;
   while (*left != '\0' && *right != '\0')
   {
      if (tolower((unsigned char)*left) != tolower((unsigned char)*right))
         return 0;
      left++;
      right++;
   }
   return *left == '\0' && *right == '\0';
}

static void frontend_policy_error(char *error, size_t error_len,
                                  const char *message)
{
   if (error != NULL && error_len > 0)
      snprintf(error, error_len, "%s", message != NULL ? message : "");
}

static int frontend_policy_truthy(const char *value)
{
   return value != NULL
       && (strcmp(value, "1") == 0
        || frontend_policy_equal_ci(value, "true")
        || frontend_policy_equal_ci(value, "yes")
        || frontend_policy_equal_ci(value, "on"));
}

static void frontend_policy_trim_root(char *path)
{
   size_t len;

   if (path == NULL)
      return;
   len = strlen(path);
   while (len > 1 && (path[len - 1] == '/' || path[len - 1] == '\\'))
      path[--len] = '\0';
}

static int frontend_policy_add_root(const char *path, int readonly,
                                    char *error, size_t error_len)
{
   char resolved[THE_FRONTEND_POLICY_PATH_MAX + 1];
   struct stat info;
   size_t i;

   if (path == NULL || *path == '\0'
   ||  frontend_realpath(path, resolved) == NULL
   ||  stat(resolved, &info) != 0 || !S_ISDIR(info.st_mode))
   {
      char message[THE_FRONTEND_POLICY_PATH_MAX + 80];

      snprintf(message, sizeof(message), "invalid web workspace root: %s",
               path != NULL ? path : "");
      frontend_policy_error(error, error_len, message);
      return 0;
   }
   frontend_policy_trim_root(resolved);
   for (i = 0; i < frontend_policy.root_count; i++)
   {
      if (strcmp(frontend_policy.roots[i].path, resolved) == 0)
      {
         if (readonly)
            frontend_policy.roots[i].readonly = 1;
         return 1;
      }
   }
   if (frontend_policy.root_count >= THE_FRONTEND_POLICY_ROOT_MAX)
   {
      frontend_policy_error(error, error_len,
                            "too many web workspace roots");
      return 0;
   }
   snprintf(frontend_policy.roots[frontend_policy.root_count].path,
            sizeof(frontend_policy.roots[frontend_policy.root_count].path),
            "%s", resolved);
   frontend_policy.roots[frontend_policy.root_count].readonly = readonly;
   frontend_policy.root_count++;
   return 1;
}

void the_frontend_policy_disable(void)
{
   memset(&frontend_policy, 0, sizeof(frontend_policy));
}

int the_frontend_policy_configure_from_environment(char *error,
                                                   size_t error_len)
{
   const char *workspace = getenv("THE_WEB_WORKSPACE");
   const char *readonly_roots = getenv("THE_WEB_READONLY_ROOTS");
   char current[THE_FRONTEND_POLICY_PATH_MAX + 1];
   char roots[THE_FRONTEND_POLICY_PATH_MAX * 2 + 1];
   char separator[2] = { FRONTEND_PATH_SEPARATOR, '\0' };
   char *cursor;

   the_frontend_policy_disable();
   frontend_policy_error(error, error_len, "");
   if (workspace == NULL || *workspace == '\0')
   {
      if (frontend_getcwd(current, sizeof(current)) == NULL)
      {
         frontend_policy_error(error, error_len,
                               "unable to determine web workspace");
         return 0;
      }
      workspace = current;
   }
   if (!frontend_policy_add_root(
          workspace, frontend_policy_truthy(
                        getenv("THE_WEB_WORKSPACE_READONLY")),
          error, error_len))
      return 0;

   if (readonly_roots != NULL && *readonly_roots != '\0')
   {
      if (strlen(readonly_roots) >= sizeof(roots))
      {
         frontend_policy_error(error, error_len,
                               "THE_WEB_READONLY_ROOTS is too long");
         return 0;
      }
      strcpy(roots, readonly_roots);
      cursor = strtok(roots, separator);
      while (cursor != NULL)
      {
         if (!frontend_policy_add_root(cursor, 1, error, error_len))
            return 0;
         cursor = strtok(NULL, separator);
      }
   }
   frontend_policy.enabled = 1;
   return 1;
}

int the_frontend_policy_enabled(void)
{
   return frontend_policy.enabled;
}

size_t the_frontend_policy_root_count(void)
{
   return frontend_policy.root_count;
}

const char *the_frontend_policy_root_at(size_t index, int *readonly)
{
   if (index >= frontend_policy.root_count)
      return NULL;
   if (readonly != NULL)
      *readonly = frontend_policy.roots[index].readonly;
   return frontend_policy.roots[index].path;
}

static int frontend_policy_path_in_root(const char *path, const char *root)
{
   size_t len;

   if (path == NULL || root == NULL)
      return 0;
   if (strcmp(root, "/") == 0)
      return path[0] == '/';
   len = strlen(root);
   return strncmp(path, root, len) == 0
       && (path[len] == '\0' || path[len] == '/' || path[len] == '\\');
}

static int frontend_policy_classify_path(const char *path, int *readonly)
{
   size_t i;
   int found = 0;
   int path_readonly = 0;

   for (i = 0; i < frontend_policy.root_count; i++)
   {
      if (frontend_policy_path_in_root(path, frontend_policy.roots[i].path))
      {
         found = 1;
         if (frontend_policy.roots[i].readonly)
            path_readonly = 1;
      }
   }
   if (readonly != NULL)
      *readonly = path_readonly;
   return found;
}

static int frontend_policy_make_candidate(const char *path, char *candidate,
                                          size_t candidate_len)
{
   int len;

   if (path == NULL || *path == '\0' || candidate == NULL
   ||  candidate_len == 0 || frontend_policy.root_count == 0)
      return 0;
#if defined(_WIN32)
   if ((isalpha((unsigned char)path[0]) && path[1] == ':')
   ||  path[0] == '/' || path[0] == '\\')
#else
   if (path[0] == '/')
#endif
      len = snprintf(candidate, candidate_len, "%s", path);
   else
      len = snprintf(candidate, candidate_len, "%s/%s",
                     frontend_policy.roots[0].path, path);
   return len > 0 && (size_t)len < candidate_len;
}

int the_frontend_policy_resolve_path(const char *path, int allow_missing,
                                     char *out, size_t out_len,
                                     int *readonly, char *error,
                                     size_t error_len)
{
   char candidate[THE_FRONTEND_POLICY_PATH_MAX + 1];
   char resolved[THE_FRONTEND_POLICY_PATH_MAX + 1];
   char parent[THE_FRONTEND_POLICY_PATH_MAX + 1];
   char *leaf;
   int len;

   frontend_policy_error(error, error_len, "");
   if (!frontend_policy.enabled)
   {
      len = snprintf(out, out_len, "%s", path != NULL ? path : "");
      return len >= 0 && (size_t)len < out_len;
   }
   if (!frontend_policy_make_candidate(path, candidate, sizeof(candidate)))
   {
      frontend_policy_error(error, error_len, "invalid file path");
      return 0;
   }
   if (frontend_realpath(candidate, resolved) == NULL)
   {
      struct stat candidate_info;

      if (!allow_missing || errno != ENOENT)
      {
         frontend_policy_error(error, error_len,
                               "file path does not exist or is inaccessible");
         return 0;
      }
#if !defined(_WIN32)
      if (lstat(candidate, &candidate_info) == 0
      &&  S_ISLNK(candidate_info.st_mode))
      {
         frontend_policy_error(error, error_len,
                               "new file path cannot be a symbolic link");
         return 0;
      }
#else
      (void)candidate_info;
#endif
      snprintf(parent, sizeof(parent), "%s", candidate);
      leaf = strrchr(parent, '/');
#if defined(_WIN32)
      {
         char *backslash = strrchr(parent, '\\');
         if (backslash != NULL && (leaf == NULL || backslash > leaf))
            leaf = backslash;
      }
#endif
      if (leaf == NULL || leaf[1] == '\0'
      ||  strcmp(leaf + 1, ".") == 0 || strcmp(leaf + 1, "..") == 0)
      {
         frontend_policy_error(error, error_len, "invalid new file path");
         return 0;
      }
      *leaf++ = '\0';
      if (frontend_realpath(parent[0] != '\0' ? parent : "/", resolved)
          == NULL)
      {
         frontend_policy_error(error, error_len,
                               "new file parent does not exist");
         return 0;
      }
      len = snprintf(candidate, sizeof(candidate), "%s/%s", resolved, leaf);
      if (len <= 0 || (size_t)len >= sizeof(candidate))
      {
         frontend_policy_error(error, error_len, "file path is too long");
         return 0;
      }
      snprintf(resolved, sizeof(resolved), "%s", candidate);
   }
   frontend_policy_trim_root(resolved);
   if (!frontend_policy_classify_path(resolved, readonly))
   {
      frontend_policy_error(error, error_len,
                            "file path is outside the web workspace");
      return 0;
   }
   len = snprintf(out, out_len, "%s", resolved);
   if (len < 0 || (size_t)len >= out_len)
   {
      frontend_policy_error(error, error_len, "file path is too long");
      return 0;
   }
   return 1;
}

int the_frontend_policy_prepare_action(TheFrontendAction *action,
                                       char *error, size_t error_len)
{
   char resolved[THE_FRONTEND_POLICY_PATH_MAX + 1];
   int readonly = 0;
   int allow_missing;
   struct stat info;

   if (!frontend_policy.enabled)
      return 1;
   if (action == NULL)
      return 0;
   switch (action->id)
   {
      case THE_FRONTEND_ACTION_FILE_OPEN:
      case THE_FRONTEND_ACTION_BUFFER_SWITCH:
      case THE_FRONTEND_ACTION_FILE_CREATE:
         allow_missing = action->id == THE_FRONTEND_ACTION_FILE_CREATE;
         if (!the_frontend_policy_resolve_path(
                action->argument, allow_missing, resolved, sizeof(resolved),
                &readonly, error, error_len))
            return 0;
         if (action->id == THE_FRONTEND_ACTION_FILE_CREATE
         &&  stat(resolved, &info) == 0)
         {
            frontend_policy_error(error, error_len,
                                  "file already exists; use Open");
            return 0;
         }
         if (action->id == THE_FRONTEND_ACTION_FILE_CREATE && readonly)
         {
            frontend_policy_error(error, error_len,
                                  "cannot create a file in a read-only root");
            return 0;
         }
         if (strlen(resolved) > THE_FRONTEND_ACTION_ARGUMENT_MAX)
         {
            frontend_policy_error(error, error_len, "file path is too long");
            return 0;
         }
         strcpy(action->argument, resolved);
         return 1;
      case THE_FRONTEND_ACTION_FILE_SAVE:
         return the_frontend_policy_current_file_writable(error, error_len);
      case THE_FRONTEND_ACTION_FILE_CLOSE:
      case THE_FRONTEND_ACTION_EDIT_UNDO:
         return 1;
      case THE_FRONTEND_ACTION_NONE:
      default:
         frontend_policy_error(error, error_len,
                               "action is unavailable in this session");
         return 0;
   }
}

static int frontend_policy_word(const char **cursor, char *out,
                                size_t out_len)
{
   const char *start;
   size_t len;

   if (cursor == NULL || *cursor == NULL || out == NULL || out_len == 0)
      return 0;
   while (isspace((unsigned char)**cursor))
      (*cursor)++;
   start = *cursor;
   while (**cursor != '\0' && !isspace((unsigned char)**cursor))
      (*cursor)++;
   len = (size_t)(*cursor - start);
   if (len == 0 || len >= out_len)
      return 0;
   memcpy(out, start, len);
   out[len] = '\0';
   for (len = 0; out[len] != '\0'; len++)
      out[len] = (char)tolower((unsigned char)out[len]);
   return 1;
}

static int frontend_policy_word_in_list(const char *word,
                                        const char *const *items,
                                        size_t count)
{
   size_t i;

   for (i = 0; i < count; i++)
   {
      if (strcmp(word, items[i]) == 0)
         return 1;
   }
   return 0;
}

int the_frontend_policy_command_allowed(const char *command)
{
   static const char *const allowed_commands[] =
   {
      "backward", "bottom", "change", "clocate", "copy", "delete",
      "down", "duplicate", "find", "forward", "input", "left",
      "locate", "lowercase", "next", "previous", "recover", "replace",
      "right", "shift", "sort", "sos", "top", "uppercase", "up"
   };
   static const char *const allowed_set_operands[] =
   {
      "autoscroll", "case", "display", "hex", "highlight", "insertmode",
      "number", "position", "prefix", "scale", "scope", "tabs",
      "tabline", "verify", "wordwrap", "wrap", "zone"
   };
   static const char *const allowed_sos_operands[] =
   {
      "addline", "blockend", "blockstart", "bottomedge", "cuadelback",
      "cuadelchar", "current", "cursoradj", "cursorshift", "delback",
      "delchar", "delend", "delline", "delword", "doprefix", "endchar",
      "firstchar", "firstcol", "instab", "lastcol", "leftedge",
      "makecurr", "marginl", "marginr", "parindent", "prefix",
      "rightedge", "settab", "startendchar", "tabb", "tabf",
      "tabfieldb", "tabfieldf", "tabwordb", "tabwordf", "togglefold",
      "topedge", "undo"
   };
   const char *cursor = command;
   char verb[64];
   char operand[64];

   if (!frontend_policy.enabled)
      return 1;
   if (!frontend_policy_word(&cursor, verb, sizeof(verb)))
      return 0;
   if (strcmp(verb, "set") == 0)
      return frontend_policy_word(&cursor, operand, sizeof(operand))
          && frontend_policy_word_in_list(
                operand, allowed_set_operands,
                sizeof(allowed_set_operands) /
                sizeof(allowed_set_operands[0]));
   if (strcmp(verb, "sos") == 0)
      return frontend_policy_word(&cursor, operand, sizeof(operand))
          && frontend_policy_word_in_list(
                operand, allowed_sos_operands,
                sizeof(allowed_sos_operands) /
                sizeof(allowed_sos_operands[0]));
   return frontend_policy_word_in_list(
      verb, allowed_commands,
      sizeof(allowed_commands) / sizeof(allowed_commands[0]));
}

static int frontend_policy_current_file_path(char *out, size_t out_len)
{
   int len;

   if (vd_current == NULL || CURRENT_FILE == NULL
   ||  out == NULL || out_len == 0)
      return 0;
   if (CURRENT_FILE->fpath != NULL && CURRENT_FILE->fname != NULL)
      len = snprintf(out, out_len, "%s%s", (char *)CURRENT_FILE->fpath,
                     (char *)CURRENT_FILE->fname);
   else if (CURRENT_FILE->actualfname != NULL)
      len = snprintf(out, out_len, "%s", (char *)CURRENT_FILE->actualfname);
   else
      return 0;
   return len > 0 && (size_t)len < out_len;
}

int the_frontend_policy_current_file_writable(char *error,
                                              size_t error_len)
{
   char path[THE_FRONTEND_POLICY_PATH_MAX + 1];
   char resolved[THE_FRONTEND_POLICY_PATH_MAX + 1];
   int readonly = 0;

   if (!frontend_policy.enabled)
      return 1;
   if (!frontend_policy_current_file_path(path, sizeof(path))
   ||  !the_frontend_policy_resolve_path(path, 1, resolved,
                                         sizeof(resolved), &readonly,
                                         error, error_len))
      return 0;
   if (readonly)
   {
      frontend_policy_error(error, error_len,
                            "current file is in a read-only root");
      return 0;
   }
   return 1;
}

void the_frontend_policy_apply_current_file(void)
{
   char path[THE_FRONTEND_POLICY_PATH_MAX + 1];
   char resolved[THE_FRONTEND_POLICY_PATH_MAX + 1];
   int readonly = 0;

   if (!frontend_policy.enabled || vd_current == NULL || CURRENT_FILE == NULL
   ||  !frontend_policy_current_file_path(path, sizeof(path))
   ||  !the_frontend_policy_resolve_path(path, 1, resolved,
                                         sizeof(resolved), &readonly,
                                         NULL, 0))
      return;
   if (readonly)
      CURRENT_FILE->readonly = READONLY_FORCE;
}
