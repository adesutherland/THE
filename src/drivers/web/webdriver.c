#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>

#if defined(_WIN32)
# define lstat stat
#endif

#include "headlessdriver.h"
#include "frontendaction.h"
#include "frontendpolicy.h"
#include "inputevent.h"
#include "llmdriver.h"
#include "llmruntime.h"
#include "mongoose.h"
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

static void web_driver_seed_filearea_focus(int row, int display_col)
{
   SHOW_LINE *show_row;
   const CHARTYPE *line;
   size_t len;
   int logical_col;
   LogicalCursor cursor;

   if (CURRENT_VIEW == NULL || screen[current_screen].sl == NULL
   ||  row < 0 || row >= screen[current_screen].rows[WINDOW_FILEAREA])
      return;
   show_row = &screen[current_screen].sl[row];
   if (show_row->line_type & (LINE_OUT_OF_BOUNDS_ABOVE
                            | LINE_OUT_OF_BOUNDS_BELOW))
      return;
   if (show_row->line_type == LINE_TOF || show_row->line_type == LINE_EOF)
   {
      line = (const CHARTYPE *)"";
      len = 0;
   }
   else
   {
      line = show_row->contents != NULL ? show_row->contents : rec;
      len = show_row->contents != NULL ? show_row->length : rec_len;
   }
   logical_col = driver_layout_logical_col_from_display(
      line, len, (int)CURRENT_VIEW->verify_col - 1, display_col,
      TEXT_SNAP_BACKWARD);
   cursor = logical_cursor_from_cell(
      LOGICAL_CURSOR_ZONE_FILEAREA, show_row->line_number, row, line, len,
      logical_col, TEXT_SNAP_BACKWARD, 1);
   logical_cursor_set_desired_cell(
      &cursor, driver_layout_width_col_from_logical(line, len,
                                                    cursor.text.cell_column));
   logical_cursor_state_focus(&CURRENT_VIEW->logical_cursor, cursor);
}

static void web_driver_sync_focus(void)
{
   LogicalCursor logical;
   TheDriverWindow *window;
   int row = 0;
   int col = 0;

   if (CURRENT_VIEW == NULL)
      return;
   logical = CURRENT_VIEW->logical_cursor.current;
   if (!logical.valid && CURRENT_VIEW->current_window == WINDOW_COMMAND)
   {
      CURRENT_VIEW->previous_window = CURRENT_VIEW->current_window;
      CURRENT_VIEW->current_window = WINDOW_FILEAREA;
   }
   the_driver_set_current_screen(current_screen);
   the_driver_set_screen_current_role(current_screen,
                                      CURRENT_VIEW->current_window);
   if (!logical.valid)
   {
      window = driver_current_window();
      if (CURRENT_VIEW->current_window == WINDOW_FILEAREA
      ||  CURRENT_VIEW->current_window == WINDOW_PREFIX)
      {
         row = get_row_for_focus_line(current_screen,
                                      CURRENT_VIEW->focus_line,
                                      CURRENT_VIEW->current_row);
         if (row < 0)
            row = CURRENT_VIEW->current_row;
         col = CURRENT_VIEW->x[CURRENT_VIEW->current_window];
      }
      else if (CURRENT_VIEW->current_window == WINDOW_COMMAND)
         col = CURRENT_VIEW->cmdline_col;
      if (window != NULL)
         the_driver->move_window_cursor(window, (short)row, (short)col);
      if (CURRENT_VIEW->current_window == WINDOW_FILEAREA)
         web_driver_seed_filearea_focus(row, col);
   }
   cursor_focus_sync_current(current_screen, CURRENT_VIEW);
}

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
   const char *candidates[5];
   char executable_root[WEB_DRIVER_PATH_CAPACITY];
   size_t i;

   web_driver_executable_root(options, executable_root,
                              sizeof(executable_root));
   candidates[0] = getenv("THE_WEB_ROOT");
   candidates[1] = executable_root;
   candidates[2] = THE_WEB_BUILD_DIR;
   candidates[3] = THE_WEB_INSTALL_DIR;
   candidates[4] = "web/dist";
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
   size_t used = 0;
   size_t i;
   int len;

   len = snprintf(catalog, sizeof(catalog),
                  "{\"type\":\"actions\",\"actions\":[");
   if (len < 0 || (size_t)len >= sizeof(catalog))
      return;
   used = (size_t)len;
   for (i = 0; i < the_frontend_action_definition_count(); i++)
   {
      const TheFrontendActionDefinition *definition =
         the_frontend_action_definition_at(i);

      len = snprintf(catalog + used, sizeof(catalog) - used,
                     "%s{\"id\":\"%s\",\"menu\":%s%s%s,"
                     "\"label\":\"%s\",\"requires_argument\":%s}",
                     i == 0 ? "" : ",", definition->name,
                     definition->menu != NULL ? "\"" : "null",
                     definition->menu != NULL ? definition->menu : "",
                     definition->menu != NULL ? "\"" : "",
                     definition->label,
                     definition->requires_argument ? "true" : "false");
      if (len < 0 || (size_t)len >= sizeof(catalog) - used)
         return;
      used += (size_t)len;
   }
   len = snprintf(catalog + used, sizeof(catalog) - used, "]}");
   if (len < 0 || (size_t)len >= sizeof(catalog) - used)
      return;
   used += (size_t)len;
   web_driver_send(connection, catalog, used);
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
       && (path[len] == '\0' || path[len] == '/');
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

static int web_driver_send_files(struct mg_connection *connection,
                                 size_t root_index, const char *relative,
                                 long id)
{
   const char *root;
   char candidate[WEB_DRIVER_PATH_CAPACITY];
   char directory[WEB_DRIVER_PATH_CAPACITY];
   char error[256];
   char *response;
   size_t response_len = WEB_DRIVER_SNAPSHOT_CAPACITY;
   size_t used = 0;
   size_t i;
   int root_readonly = 0;
   int resolved_readonly = 0;
   DIR *dir;
   struct dirent *entry;
   int first = 1;

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
   dir = opendir(directory);
   if (dir == NULL)
   {
      web_driver_send_status(connection, "error", id,
                             "workspace location is not a directory");
      return 0;
   }
   response = (char *)malloc(response_len);
   if (response == NULL)
   {
      closedir(dir);
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
   while ((entry = readdir(dir)) != NULL)
   {
      char path[WEB_DRIVER_PATH_CAPACITY];
      char child_relative[WEB_DRIVER_PATH_CAPACITY];
      struct stat info;

      if (strcmp(entry->d_name, ".") == 0
      ||  strcmp(entry->d_name, "..") == 0)
         continue;
      if (snprintf(path, sizeof(path), "%s/%s", directory, entry->d_name)
          >= (int)sizeof(path)
      ||  lstat(path, &info) != 0 || S_ISLNK(info.st_mode)
      ||  (!S_ISDIR(info.st_mode) && !S_ISREG(info.st_mode)))
         continue;
      if (snprintf(child_relative, sizeof(child_relative), "%s%s%s",
                   relative, *relative != '\0' ? "/" : "", entry->d_name)
          >= (int)sizeof(child_relative))
         continue;
      if (!web_driver_append(
             response, response_len, &used,
             "%s{\"name\":%m,\"path\":%m,\"target\":%m,\"type\":\"%s\","
             "\"readonly\":%s}",
             first ? "" : ",", MG_ESC(entry->d_name),
             MG_ESC(child_relative), MG_ESC(path),
             S_ISDIR(info.st_mode) ? "directory" : "file",
             (root_readonly || resolved_readonly) ? "true" : "false"))
         goto too_large;
      first = 0;
   }
   if (!web_driver_append(response, response_len, &used, "]}"))
      goto too_large;
   closedir(dir);
   web_driver_send(connection, response, used);
   free(response);
   return 1;

too_large:
   closedir(dir);
   free(response);
   web_driver_send_status(connection, "error", id,
                          "workspace listing is too large");
   return 0;
}

static void web_driver_format_snapshot(void)
{
   LlmDriverFormatOptions options;

   web_driver_sync_focus();
   llm_driver_format_options_init(&options);
   options.compact = 1;
   options.max_text_cols = LLM_DRIVER_MAX_COLS;
   options.include_all_utf = 0;
   web_state.snapshot_len = llm_runtime_format_screen(
      current_screen, &options, web_state.snapshot,
      sizeof(web_state.snapshot));
   while (web_state.snapshot_len > 0
   && (web_state.snapshot[web_state.snapshot_len - 1] == '\n'
    || web_state.snapshot[web_state.snapshot_len - 1] == '\r'))
      web_state.snapshot[--web_state.snapshot_len] = '\0';
}

static void web_driver_publish_snapshot(void)
{
   if (!web_state.active)
      return;
   web_driver_format_snapshot();
   web_driver_send(web_state.client, web_state.snapshot,
                   web_state.snapshot_len);
}

static int web_driver_queue_event(TheInputEvent event)
{
   return the_input_queue_push(&web_state.inputs, event);
}

static int web_driver_decode_utf8(const unsigned char *text, size_t len,
                                  size_t *used, uint32_t *codepoint)
{
   uint32_t value;
   size_t count;
   size_t i;

   if (text == NULL || len == 0 || used == NULL || codepoint == NULL)
      return 0;
   if (text[0] < 0x80)
   {
      *used = 1;
      *codepoint = text[0];
      return 1;
   }
   if ((text[0] & 0xe0) == 0xc0)
   {
      count = 2;
      value = text[0] & 0x1f;
      if (value < 2)
         return 0;
   }
   else if ((text[0] & 0xf0) == 0xe0)
   {
      count = 3;
      value = text[0] & 0x0f;
   }
   else if ((text[0] & 0xf8) == 0xf0)
   {
      count = 4;
      value = text[0] & 0x07;
   }
   else
      return 0;
   if (count > len)
      return 0;
   for (i = 1; i < count; i++)
   {
      if ((text[i] & 0xc0) != 0x80)
         return 0;
      value = (value << 6) | (uint32_t)(text[i] & 0x3f);
   }
   if ((count == 3 && value < 0x800)
   ||  (count == 4 && value < 0x10000)
   ||  value > 0x10ffff
   ||  (value >= 0xd800 && value <= 0xdfff))
      return 0;
   *used = count;
   *codepoint = value;
   return 1;
}

static int web_driver_queue_text(const char *text)
{
   uint32_t codepoints[THE_INPUT_QUEUE_MAX];
   const unsigned char *bytes = (const unsigned char *)text;
   size_t len = text != NULL ? strlen(text) : 0;
   size_t offset = 0;
   size_t count = 0;
   size_t i;

   while (offset < len)
   {
      size_t used = 0;

      if (count >= THE_INPUT_QUEUE_MAX
      ||  !web_driver_decode_utf8(bytes + offset, len - offset, &used,
                                  &codepoints[count]))
         return 0;
      offset += used;
      count++;
   }
   if (count > THE_INPUT_QUEUE_MAX - web_state.inputs.count)
      return 0;
   for (i = 0; i < count; i++)
   {
      TheInputEvent event;

      if (!the_input_event_from_text(codepoints[i], &event)
      ||  !web_driver_queue_event(event))
         return 0;
   }
   return count > 0;
}

static void web_driver_handle_message(struct mg_connection *connection,
                                      struct mg_str json)
{
   char *type = mg_json_get_str(json, "$.type");
   long id = mg_json_get_long(json, "$.id", 0);
   TheInputEvent event;
   int queued = 0;

   if (type == NULL)
   {
      web_driver_send_status(connection, "error", id, "missing message type");
      return;
   }
   if (strcmp(type, "snapshot") == 0)
   {
      web_driver_publish_snapshot();
      web_driver_send_status(connection, "ack", id, "snapshot sent");
      mg_free(type);
      return;
   }
   if (strcmp(type, "files") == 0)
   {
      long root = mg_json_get_long(json, "$.root", 0);
      char *path = mg_json_get_str(json, "$.path");

      (void)web_driver_send_files(connection, root >= 0 ? (size_t)root :
                                  the_frontend_policy_root_count(),
                                  path != NULL ? path : "", id);
      mg_free(path);
      mg_free(type);
      return;
   }
   if (strcmp(type, "key") == 0)
   {
      char *name = mg_json_get_str(json, "$.key");

      queued = name != NULL
            && the_input_event_from_key_name(name, &event)
            && web_driver_queue_event(event);
      mg_free(name);
   }
   else if (strcmp(type, "text") == 0)
   {
      char *text = mg_json_get_str(json, "$.text");

      queued = text != NULL && web_driver_queue_text(text);
      mg_free(text);
   }
   else if (strcmp(type, "command") == 0)
   {
      char *command = mg_json_get_str(json, "$.command");

      queued = command != NULL
            && the_input_event_from_restricted_command(command, &event)
            && web_driver_queue_event(event);
      mg_free(command);
   }
   else if (strcmp(type, "action") == 0)
   {
      char *name = mg_json_get_str(json, "$.action");
      char *argument = mg_json_get_str(json, "$.argument");

      queued = name != NULL
            && the_input_event_from_action(name,
                                           argument != NULL ? argument : "",
                                           &event)
            && web_driver_queue_event(event);
      mg_free(name);
      mg_free(argument);
   }
   else if (strcmp(type, "hit") == 0)
   {
      char *target = mg_json_get_str(json, "$.target");
      TheInputLogicalTargetKind kind = THE_INPUT_TARGET_NONE;
      long line = mg_json_get_long(json, "$.line", 0);
      long row = mg_json_get_long(json, "$.row", -1);
      long cell = mg_json_get_long(json, "$.cell", -1);

      queued = target != NULL
            && the_input_logical_target_kind_from_name(target, &kind)
            && the_input_event_from_logical_target(
                  kind, (LINETYPE)line, (int)row, (int)cell, 0, -1, &event)
            && web_driver_queue_event(event);
      mg_free(target);
   }
   else
   {
      web_driver_send_status(connection, "error", id,
                             "unsupported message type");
      mg_free(type);
      return;
   }

   web_driver_send_status(connection, queued ? "ack" : "error", id,
                          queued ? "queued" : "invalid input");
   mg_free(type);
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
                     "\"driver\":\"web\",\"rows\":24,\"cols\":80}");
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
      web_state.client = NULL;
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
