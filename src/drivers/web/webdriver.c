#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#if !defined(_WIN32)
#include <dirent.h>
#endif
#include <sys/stat.h>

#include "headlessdriver.h"
#include "frontendaction.h"
#include "frontendpolicy.h"
#include "inputevent.h"
#include "websession.h"
#include "panelui.h"
#include "llmdriver.h"
#include "llmruntime.h"
#include "mongoose.h"
#if defined(_WIN32)
# include <windows.h>
#endif
#include "the.h"
#include "proto.h"
#include "driverlayout.h"
#include "driverwindow.h"
#include "thedriver.h"
#include "vars.h"

#ifndef THE_WEB_BUILD_DIR
# define THE_WEB_BUILD_DIR ""
#endif

#ifndef THE_WEB_INSTALL_DIR
# define THE_WEB_INSTALL_DIR ""
#endif

#define WEB_DRIVER_SNAPSHOT_CAPACITY 262144
#define WEB_DRIVER_PATH_CAPACITY 2048
#define WEB_DRIVER_TOKEN_CAPACITY 129

typedef struct
{
   struct mg_mgr manager;
   struct mg_connection *listener;
   struct mg_connection *client;
   TheInputQueue inputs;
   int active;
   int input_timeout_ms;
   unsigned short port;
   char token[WEB_DRIVER_TOKEN_CAPACITY];
   char root[WEB_DRIVER_PATH_CAPACITY];
   char snapshot[WEB_DRIVER_SNAPSHOT_CAPACITY];
   size_t snapshot_len;
} WebDriverState;

static WebDriverState web_state;
static TheDriverOps web_driver_ops;
static void (*headless_update)(void);
static void (*headless_set_timeout)(int milliseconds);

static int web_driver_file_exists(const char *root, const char *leaf)
{
   char path[WEB_DRIVER_PATH_CAPACITY];
   FILE *file;

   if (root == NULL || *root == '\0' || leaf == NULL)
      return 0;
   if (snprintf(path, sizeof(path), "%s/%s", root, leaf)
      >= (int)sizeof(path))
      return 0;
   file = fopen(path, "rb");
   if (file == NULL)
      return 0;
   fclose(file);
   return 1;
}

static void web_driver_executable_root(const TheDriverStartupOptions *options,
                                       char *out, size_t out_len)
{
   const char *argv0;
   const char *slash;
   const char *backslash;
   size_t dir_len;

   if (out == NULL || out_len == 0)
      return;
   out[0] = '\0';
   if (options == NULL || options->program_path == NULL)
      return;
   argv0 = options->program_path;
   slash = strrchr(argv0, '/');
   backslash = strrchr(argv0, '\\');
   if (backslash != NULL && (slash == NULL || backslash > slash))
      slash = backslash;
   if (slash == NULL)
      return;
   dir_len = (size_t)(slash - argv0);
   if (dir_len == 0)
      dir_len = 1;
   if (dir_len + sizeof("/web") > out_len)
      return;
   memcpy(out, argv0, dir_len);
   out[dir_len] = '\0';
   strcat(out, "/web");
}

static int web_driver_select_root(const TheDriverStartupOptions *options)
{
   const char *candidates[6];
   char executable_root[WEB_DRIVER_PATH_CAPACITY];
   char installed_root[WEB_DRIVER_PATH_CAPACITY]={0};
   const char *home=getenv("THE_HOME_DIR");
   size_t i;

   web_driver_executable_root(options, executable_root,
                              sizeof(executable_root));
   if (home && *home && strlen(home)+5<sizeof(installed_root))
   {
      strcpy(installed_root,home);
      strcat(installed_root,"/web");
   }
   candidates[0] = getenv("THE_WEB_ROOT");
   candidates[1] = installed_root;
   candidates[2] = executable_root;
   candidates[3] = THE_WEB_BUILD_DIR;
   candidates[4] = THE_WEB_INSTALL_DIR;
   candidates[5] = "web/dist";
   for (i = 0; i < sizeof(candidates) / sizeof(candidates[0]); i++)
   {
      if (web_driver_file_exists(candidates[i], "index.html"))
      {
         snprintf(web_state.root, sizeof(web_state.root), "%s",
                  candidates[i]);
         return 1;
      }
   }
   return 0;
}

static void web_driver_send(struct mg_connection *connection,
                            const char *text, size_t len)
{
   if (connection == NULL || text == NULL || len == 0)
      return;
   (void)mg_ws_send(connection, text, len, WEBSOCKET_OP_TEXT);
}

static void web_driver_send_status(struct mg_connection *connection,
                                   const char *type, long id,
                                   const char *message)
{
   char response[512];
   int len;

   len = snprintf(response, sizeof(response),
                  "{\"type\":\"%s\",\"id\":%ld,\"message\":\"%s\"}",
                  type != NULL ? type : "error", id,
                  message != NULL ? message : "");
   if (len > 0 && len < (int)sizeof(response))
      web_driver_send(connection, response, (size_t)len);
}

static void web_driver_send_action_catalog(struct mg_connection *connection)
{
   char catalog[4096];
   size_t length = the_web_format_actions(catalog, sizeof(catalog));
   web_driver_send(connection, catalog, length);
}

static int web_driver_path_in_root(const char *path, const char *root)
{
   size_t len;

   if (path == NULL || root == NULL)
      return 0;
   if (strcmp(root, "/") == 0)
      return path[0] == '/';
   len = strlen(root);
   return strncmp(path, root, len) == 0
       && ((len>0 && (root[len-1]=='/' || root[len-1]=='\\'))
           || path[len] == '\0' || path[len] == '/' || path[len] == '\\');
}

static const char *web_driver_root_name(size_t index, const char *root)
{
   const char *name;

   if (index == 0)
      return "Workspace";
   name = strrchr(root, '/');
   return name != NULL && name[1] != '\0' ? name + 1 : root;
}

static int web_driver_append(char *out, size_t out_len, size_t *used,
                             const char *format, ...)
{
   va_list args;
   size_t len;

   if (out == NULL || used == NULL || *used >= out_len)
      return 0;
   va_start(args, format);
   len = mg_vsnprintf(out + *used, out_len - *used, format, &args);
   va_end(args);
   if (len >= out_len - *used)
      return 0;
   *used += len;
   return 1;
}

/* Mongoose defines its own Windows opendir/readdir with a private dirent
 * layout. Never mix those symbols with MinGW's public dirent structure. */
static int web_driver_list_directory(const char *directory,
                                      int (*entry)(const char *,void *), void *data)
{
#if defined(_WIN32)
   char pattern[WEB_DRIVER_PATH_CAPACITY];
   WCHAR wide[WEB_DRIVER_PATH_CAPACITY];
   WIN32_FIND_DATAW info;
   HANDLE search;
   if (snprintf(pattern,sizeof(pattern),"%s/*",directory)>=(int)sizeof(pattern)
   || !MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,pattern,-1,wide,WEB_DRIVER_PATH_CAPACITY)) return 0;
   search=FindFirstFileW(wide,&info);
   if (search==INVALID_HANDLE_VALUE) return 0;
   do
   {
      char name[WEB_DRIVER_PATH_CAPACITY];
      if (WideCharToMultiByte(CP_UTF8,0,info.cFileName,-1,name,sizeof(name),NULL,NULL)
      && !entry(name,data)) { FindClose(search); return -1; }
   } while (FindNextFileW(search,&info));
   FindClose(search);
#else
   DIR *dir=opendir(directory);
   struct dirent *item;
   if (!dir) return 0;
   while ((item=readdir(dir))!=NULL)
      if (!entry(item->d_name,data)) { closedir(dir); return -1; }
   closedir(dir);
#endif
   return 1;
}

static int web_driver_workspace_kind(const char *path)
{
#if defined(_WIN32)
   WCHAR wide[WEB_DRIVER_PATH_CAPACITY];
   DWORD attributes;
   if (!MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,path,-1,wide,WEB_DRIVER_PATH_CAPACITY)) return 0;
   attributes=GetFileAttributesW(wide);
   if (attributes==INVALID_FILE_ATTRIBUTES || (attributes & FILE_ATTRIBUTE_REPARSE_POINT)) return 0;
   return (attributes & FILE_ATTRIBUTE_DIRECTORY) ? 2 : 1;
#else
   struct stat info;
   if (lstat(path,&info)!=0 || S_ISLNK(info.st_mode)) return 0;
   return S_ISDIR(info.st_mode) ? 2 : S_ISREG(info.st_mode) ? 1 : 0;
#endif
}

typedef struct
{
   char *response;
   size_t size, used;
   const char *directory, *relative;
   int readonly, first;
} WebFileListing;

static int web_driver_list_entry(const char *name, void *data)
{
   WebFileListing *listing=(WebFileListing *)data;
   char path[WEB_DRIVER_PATH_CAPACITY], relative[WEB_DRIVER_PATH_CAPACITY];
   int kind;
   if (strcmp(name,".")==0 || strcmp(name,"..")==0) return 1;
   if (snprintf(path,sizeof(path),"%s/%s",listing->directory,name)>=(int)sizeof(path)) return 1;
   kind=web_driver_workspace_kind(path);
   if (!kind) return 1;
   if (snprintf(relative,sizeof(relative),"%s%s%s",listing->relative,
                *listing->relative ? "/" : "",name)>=(int)sizeof(relative)) return 1;
   if (!web_driver_append(listing->response,listing->size,&listing->used,
         "%s{\"name\":%m,\"path\":%m,\"target\":%m,\"type\":\"%s\",\"readonly\":%s}",
         listing->first ? "" : ",",MG_ESC(name),MG_ESC(relative),MG_ESC(path),
         kind==2 ? "directory" : "file",listing->readonly ? "true" : "false")) return 0;
   listing->first=0;
   return 1;
}

static int web_driver_send_files(struct mg_connection *connection,
                                 size_t root_index, const char *relative,
                                 long id)
{
   const char *root;
   char candidate[WEB_DRIVER_PATH_CAPACITY];
   char directory[WEB_DRIVER_PATH_CAPACITY];
   char error[256]={0};
   char *response;
   size_t response_len = WEB_DRIVER_SNAPSHOT_CAPACITY;
   size_t used = 0;
   size_t i;
   int root_readonly = 0;
   int resolved_readonly = 0;
   WebFileListing listing;
   int listed;

   root = the_frontend_policy_root_at(root_index, &root_readonly);
   if (root == NULL || relative == NULL || relative[0] == '/')
   {
      web_driver_send_status(connection, "error", id,
                             "invalid workspace location");
      return 0;
   }
   if (snprintf(candidate, sizeof(candidate), "%s%s%s", root,
                *relative != '\0' ? "/" : "", relative)
       >= (int)sizeof(candidate)
   ||  !the_frontend_policy_resolve_path(
          candidate, 0, directory, sizeof(directory), &resolved_readonly,
          error, sizeof(error))
   ||  !web_driver_path_in_root(directory, root))
   {
      web_driver_send_status(connection, "error", id,
                             error[0] != '\0' ? error
                                              : "invalid workspace location");
      return 0;
   }
   if (web_driver_workspace_kind(directory)!=2)
   {
      web_driver_send_status(connection, "error", id,
                             "workspace location is not a directory");
      return 0;
   }
   response = (char *)malloc(response_len);
   if (response == NULL)
   {
      web_driver_send_status(connection, "error", id, "out of memory");
      return 0;
   }
   if (!web_driver_append(response, response_len, &used,
                          "{\"type\":\"files\",\"id\":%ld,\"root\":%lu,"
                          "\"path\":%m,\"roots\":[",
                          id, (unsigned long)root_index, MG_ESC(relative)))
      goto too_large;
   for (i = 0; i < the_frontend_policy_root_count(); i++)
   {
      const char *listed_root;
      int readonly = 0;

      listed_root = the_frontend_policy_root_at(i, &readonly);
      if (!web_driver_append(
             response, response_len, &used,
             "%s{\"id\":%lu,\"name\":%m,\"readonly\":%s}",
             i == 0 ? "" : ",", (unsigned long)i,
             MG_ESC(web_driver_root_name(i, listed_root)),
             readonly ? "true" : "false"))
         goto too_large;
   }
   if (!web_driver_append(response, response_len, &used, "],\"entries\":["))
      goto too_large;
   listing.response=response; listing.size=response_len; listing.used=used;
   listing.directory=directory; listing.relative=relative;
   listing.readonly=root_readonly || resolved_readonly; listing.first=1;
   listed=web_driver_list_directory(directory,web_driver_list_entry,&listing);
   used=listing.used;
   if (listed<0) goto too_large;
   if (!listed)
   {
      free(response);
      web_driver_send_status(connection,"error",id,"workspace directory is inaccessible");
      return 0;
   }
   if (!web_driver_append(response, response_len, &used, "]}"))
      goto too_large;
   web_driver_send(connection, response, used);
   free(response);
   return 1;

too_large:
   free(response);
   web_driver_send_status(connection, "error", id,
                          "workspace listing is too large");
   return 0;
}

static void web_driver_publish_snapshot(void)
{
   if (!web_state.active)
      return;
   web_state.snapshot_len = the_web_format_snapshot(web_state.snapshot, sizeof(web_state.snapshot));
   web_driver_send(web_state.client, web_state.snapshot,
                   web_state.snapshot_len);
}

static void web_driver_handle_message(struct mg_connection *connection, struct mg_str json)
{
   char *type = mg_json_get_str(json, "$.type");
   long id = mg_json_get_long(json, "$.id", 0);
   const char *field = "$.type";
   char *value, *argument;
   int queued;
   if (!type) { web_driver_send_status(connection, "error", id, "missing message type"); return; }
   if (strcmp(type,"snapshot")==0)
   {
      web_driver_publish_snapshot();
      web_driver_send_status(connection,"ack",id,"snapshot sent");
      mg_free(type); return;
   }
   if (strcmp(type,"files")==0)
   {
      long root=mg_json_get_long(json,"$.root",0);
      char *path=mg_json_get_str(json,"$.path");
      web_driver_send_files(connection, root>=0 ? (size_t)root : the_frontend_policy_root_count(),path ? path : "",id);
      mg_free(path); mg_free(type); return;
   }
   if (strcmp(type,"key")==0) field="$.key";
   else if (strcmp(type,"text")==0) field="$.text";
   else if (strcmp(type,"command")==0) field="$.command";
   else if (strcmp(type,"action")==0) field="$.action";
   else if (strcmp(type,"hit")==0) field="$.target";
   value=mg_json_get_str(json,field);
   argument=mg_json_get_str(json,"$.argument");
   queued=the_web_queue_request(&web_state.inputs,type,value,argument,
      mg_json_get_long(json,"$.line",0),mg_json_get_long(json,"$.row",-1),
      mg_json_get_long(json,"$.cell",-1),mg_json_get_long(json,"$.rows",0),
      mg_json_get_long(json,"$.cols",0));
   web_driver_send_status(connection, queued ? "ack" : "error", id, queued ? "queued" : "invalid input");
   mg_free(value); mg_free(argument); mg_free(type);
}

static int web_driver_authorized(const struct mg_http_message *message)
{
   char token[WEB_DRIVER_TOKEN_CAPACITY];

   if (message == NULL)
      return 0;
   return mg_http_get_var(&message->query, "token", token, sizeof(token)) > 0
       && strcmp(token, web_state.token) == 0;
}

static void web_driver_http_event(struct mg_connection *connection, int event,
                                  void *event_data)
{
   if (event == MG_EV_HTTP_MSG)
   {
      struct mg_http_message *message =
         (struct mg_http_message *)event_data;

      if (mg_match(message->uri, mg_str("/ws"), NULL))
      {
         if (!web_driver_authorized(message))
         {
            mg_http_reply(connection, 403, "Content-Type: text/plain\r\n",
                          "Forbidden\n");
            return;
         }
         mg_ws_upgrade(connection, message, NULL);
         return;
      }
      {
         struct mg_http_serve_opts options;

         memset(&options, 0, sizeof(options));
         options.root_dir = web_state.root;
         options.extra_headers =
            "Content-Security-Policy: default-src 'self'; "
            "connect-src 'self' ws://127.0.0.1:*; "
            "style-src 'self' 'unsafe-inline'\r\n"
            "X-Content-Type-Options: nosniff\r\n";
         mg_http_serve_dir(connection, message, &options);
      }
   }
   else if (event == MG_EV_WS_OPEN)
   {
      char hello[256];
      int len;

      if (web_state.client != NULL && web_state.client != connection)
      {
         web_driver_send_status(connection, "error", 0,
                                "another client controls this session");
         connection->is_draining = 1;
         return;
      }
      web_state.client = connection;
      len = snprintf(hello, sizeof(hello),
                     "{\"type\":\"hello\",\"protocol\":1,"
                     "\"driver\":\"web\",\"rows\":%d,\"cols\":%d}", terminal_lines, terminal_cols);
      if (len > 0 && len < (int)sizeof(hello))
         web_driver_send(connection, hello, (size_t)len);
      web_driver_send_action_catalog(connection);
      web_driver_publish_snapshot();
   }
   else if (event == MG_EV_WS_MSG)
   {
      struct mg_ws_message *message = (struct mg_ws_message *)event_data;

      if (web_state.client == connection)
         web_driver_handle_message(connection, message->data);
   }
   else if (event == MG_EV_CLOSE && web_state.client == connection)
   {
      web_state.client = NULL;
      if (the_panel_active)
      {
         TheInputEvent cancel;
         if (the_input_event_from_key_name("esc", &cancel)) the_input_queue_push(&web_state.inputs, cancel);
      }
   }
}

static int web_driver_activate(void)
{
   memset(&web_state, 0, sizeof(web_state));
   the_input_queue_init(&web_state.inputs);
   web_state.input_timeout_ms = -1;
   headless_driver_reset();
   return 1;
}

static int web_driver_start(const TheDriverStartupOptions *options,
                            char *error, size_t error_len)
{
   const char *configured_port = getenv("THE_WEB_PORT");
   const char *configured_bind = getenv("THE_WEB_BIND");
   const char *configured_token = getenv("THE_WEB_TOKEN");
   const char *display_host;
   char listen_url[128];

   if (!web_driver_select_root(options))
   {
      snprintf(error, error_len,
               "web assets not found; set THE_WEB_ROOT to the built UI");
      return 0;
   }
   mg_log_set(MG_LL_ERROR);
   mg_mgr_init(&web_state.manager);
   if (configured_token != NULL && *configured_token != '\0')
   {
      if (strlen(configured_token) >= sizeof(web_state.token))
      {
         snprintf(error, error_len,
                  "THE_WEB_TOKEN exceeds %lu characters",
                  (unsigned long)sizeof(web_state.token) - 1);
         mg_mgr_free(&web_state.manager);
         return 0;
      }
      snprintf(web_state.token, sizeof(web_state.token), "%s",
               configured_token);
   }
   else
      mg_random_str(web_state.token, sizeof(web_state.token));
   if (configured_bind == NULL || *configured_bind == '\0')
      configured_bind = "127.0.0.1";
   if (snprintf(listen_url, sizeof(listen_url), "http://%s:%s",
                configured_bind,
            configured_port != NULL && *configured_port != '\0'
               ? configured_port : "0") >= (int)sizeof(listen_url))
   {
      snprintf(error, error_len, "web listen address is too long");
      mg_mgr_free(&web_state.manager);
      return 0;
   }
   web_state.listener = mg_http_listen(&web_state.manager, listen_url,
                                       web_driver_http_event, NULL);
   if (web_state.listener == NULL)
   {
      snprintf(error, error_len, "unable to start web listener on %s",
               listen_url);
      mg_mgr_free(&web_state.manager);
      return 0;
   }
   web_state.port = mg_ntohs(web_state.listener->loc.port);
   web_state.active = 1;
   display_host = strcmp(configured_bind, "0.0.0.0") == 0
                ? "127.0.0.1" : configured_bind;
   fprintf(stdout, "THE web UI: http://%s:%u/?token=%s\n", display_host,
           (unsigned int)web_state.port, web_state.token);
   fflush(stdout);
   return 1;
}

static void web_driver_shutdown(int prompt_on_error)
{
   (void)prompt_on_error;
   if (web_state.active)
      mg_mgr_free(&web_state.manager);
   web_state.active = 0;
   web_state.listener = NULL;
   web_state.client = NULL;
   headless_driver_reset();
}

static int web_driver_read_input_event(TheInputEvent *event)
{
   uint64_t started = mg_millis();

   if (event == NULL)
      return 0;
   *event = the_input_event_none();
   for (;;)
   {
      int poll_ms = 50;

      if (the_input_queue_pop(&web_state.inputs, event))
         return event->kind != THE_INPUT_NONE;
      if (!web_state.active)
         return 0;
      if (web_state.input_timeout_ms >= 0)
      {
         uint64_t elapsed = mg_millis() - started;

         if (elapsed >= (uint64_t)web_state.input_timeout_ms)
            return 0;
         if ((uint64_t)poll_ms
            > (uint64_t)web_state.input_timeout_ms - elapsed)
            poll_ms = web_state.input_timeout_ms - (int)elapsed;
      }
      (void)mg_mgr_poll(&web_state.manager, poll_ms);
   }
}

static void web_driver_set_current_window_timeout(int milliseconds)
{
   web_state.input_timeout_ms = milliseconds;
   if (headless_set_timeout != NULL)
      headless_set_timeout(milliseconds);
}

static void web_driver_update(void)
{
   if (headless_update != NULL)
      headless_update();
   web_driver_publish_snapshot();
}

static const char *web_driver_ui_version(void)
{
   return "web/mongoose";
}

static CursorPresentation web_driver_cursor_presentation(void)
{
   return CURSOR_PRESENTATION_SOFTWARE;
}

static bool web_driver_cursor_uses_software(void)
{
   return true;
}

static const TheDriverModuleLifecycle web_driver_lifecycle = {
   .name = "web",
   .activate = web_driver_activate,
   .start = web_driver_start,
   .shutdown = web_driver_shutdown,
   .set_current_screen = headless_driver_set_current_screen,
   .set_screen_current_role = headless_driver_set_screen_current_role,
   .create_screen_role = headless_driver_create_screen_role,
   .create_global_window = headless_driver_create_global_window,
   .log_count = headless_driver_log_count,
   .log_entry = headless_driver_log_entry,
   .ui_version = web_driver_ui_version,
   .current_cursor_presentation = web_driver_cursor_presentation,
   .current_cursor_uses_software = web_driver_cursor_uses_software
};

int the_driver_module_abi_version(void)
{
   return THE_DRIVER_MODULE_ABI_VERSION;
}

const TheDriverOps *the_driver_module_ops(void)
{
   web_driver_ops = the_headless_driver_ops;
   headless_update = web_driver_ops.update;
   headless_set_timeout = web_driver_ops.set_current_window_timeout;
   web_driver_ops.read_input_event = web_driver_read_input_event;
   web_driver_ops.set_current_window_timeout =
      web_driver_set_current_window_timeout;
   web_driver_ops.update = web_driver_update;
   return &web_driver_ops;
}

const TheDriverModuleLifecycle *the_driver_module_lifecycle(void)
{
   return &web_driver_lifecycle;
}
