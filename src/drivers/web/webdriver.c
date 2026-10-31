#include "tools.h"
#include <stdio.h>
#include <limits.h>
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
#include "workbench.h"
#include "workbenchruntime.h"
#include "workbenchdocument.h"
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
   unsigned long workbench_revision;
   unsigned long workbench_publication;
   TheWebWorkbenchReasonState workbench_reasons;
   TheWebWorkbenchReceipts workbench_receipts;
   char workbench_panel_state[32768];
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

static void web_driver_send_workbench_page(const char *text, size_t length,
                                           void *context)
{
   web_driver_send((struct mg_connection *)context, text, length);
}

static void web_driver_send_workbench_receipt(
   struct mg_connection *connection, const TheWebWorkbenchReceipt *receipt)
{
   char message[1024];
   int length = the_web_format_workbench_receipt(message, sizeof(message), receipt);
   if (length > 0) web_driver_send(connection, message, (size_t)length);
}

static void web_driver_send_workbench_refusal(struct mg_connection *connection,
   const char *request_id, const char *code, const char *message)
{
   TheWebWorkbenchReceipt receipt;
   memset(&receipt, 0, sizeof(receipt));
   snprintf(receipt.request_id, sizeof(receipt.request_id), "%s",
            request_id ? request_id : "");
   snprintf(receipt.code, sizeof(receipt.code), "%s", code);
   snprintf(receipt.message, sizeof(receipt.message), "%s", message);
   receipt.rc = -3;
   receipt.revision = the_workbench_revision();
   web_driver_send_workbench_receipt(connection, &receipt);
}

/* Mongoose returns decoded C strings; reject JSON NUL escapes before those
 * strings can be truncated into a different ID or operand. */
static int web_driver_workbench_string_field(struct mg_str json,
                                             const char *path, int optional)
{
   int token_len = 0, offset = mg_json_get(json,path,&token_len);
   size_t i, length;
   const char *token;
   if (offset < 0) return optional;
   if (token_len < 2 || json.buf[offset] != '"') return 0;
   token = json.buf + offset;
   length = (size_t)token_len;
   for (i = 1; i + 1 < length; i++)
   {
      if (token[i] == '\0') return 0;
      if (token[i] == '\\')
      {
         if (i + 1 >= length - 1) return 0;
         if (token[i+1] == 'u' && i + 5 < length - 1
         && token[i+2] == '0' && token[i+3] == '0'
         && token[i+4] == '0' && token[i+5] == '0') return 0;
         if (token[i+1] == 'u') i += 5;
         else i++;
      }
   }
   return 1;
}

static void web_driver_publish_workbench(void)
{
   unsigned long current = the_workbench_revision();
   int reasons_changed = the_web_workbench_reasons_changed(&web_state.workbench_reasons);
   if (web_state.client != NULL
   && (current != web_state.workbench_revision || reasons_changed))
   {
      the_web_publish_workbench(web_driver_send_workbench_page, web_state.client,
                                ++web_state.workbench_publication);
      web_state.workbench_revision = current;
   }
}

static void web_driver_publish_panel_state(void)
{
   char current[32768];
   size_t length;
   if (web_state.client == NULL) return;
   length = the_web_format_panel_state(current,sizeof(current));
   if (length && strcmp(current,web_state.workbench_panel_state) != 0)
   {
      memcpy(web_state.workbench_panel_state,current,length + 1);
      web_driver_send(web_state.client,current,length);
   }
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

static void web_driver_publish_tools(void){
 static char data[1800000];static unsigned version;static struct mg_connection*client;unsigned current=the_tool_console()->version;
 if(client!=web_state.client){client=web_state.client;version=0;}
 if(web_state.client&&current!=version){size_t n=the_tool_format_json(data,sizeof(data));if(n){web_driver_send(web_state.client,data,n);version=current;}}
}
static void web_driver_publish_snapshot(void)
{
   const TheWebWorkbenchReceipt *completed;
   if (!web_state.active)
      return;
   completed = the_web_workbench_receipt_complete(&web_state.workbench_receipts);
   if (completed != NULL)
      web_driver_send_workbench_receipt(web_state.client, completed);
   the_tools_native_initialise();
   web_driver_publish_workbench();
   web_driver_publish_panel_state();
   web_driver_publish_tools();
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
   if (strcmp(type,"workbench")==0)
   {
      char *operation = mg_json_get_str(json,"$.operation");
      long version = mg_json_get_long(json,"$.version",0);
      if (version == 1 && operation && strcmp(operation,"list") == 0)
      {
         char *section = mg_json_get_str(json,"$.section");
         long offset = mg_json_get_long(json,"$.offset",0);
         long limit = mg_json_get_long(json,"$.limit",8);
         char page[THE_WORKBENCH_JSON_MAX + 80];
         size_t length = offset >= 0 && limit >= 1 && limit <= 8
            ? the_web_format_workbench_page(page,sizeof(page),
                section ? section : "SUMMARY",(size_t)offset,(size_t)limit) : 0;
         if (length) web_driver_send(connection,page,length);
         else web_driver_send_status(connection,"error",id,"invalid workbench list request");
         mg_free(section);
      }
      else if (version == 1 && operation && strcmp(operation,"invoke") == 0)
      {
         char *request_id = mg_json_get_str(json,"$.requestId");
         char *action = mg_json_get_str(json,"$.action");
         char *operand = mg_json_get_str(json,"$.argument");
         long generation = mg_json_get_long(json,"$.expectGeneration",0);
         int request_id_ok = web_driver_workbench_string_field(json,"$.requestId",0);
         int operands_ok = web_driver_workbench_string_field(json,"$.action",0)
            && web_driver_workbench_string_field(json,"$.argument",1);
         int accepted = request_id_ok ? the_web_workbench_receipt_begin(
            &web_state.workbench_receipts, request_id,"invoke",action,
            generation > 0 ? (unsigned long)generation : 0,operand) : -1;
         if (accepted == 1)
         {
            TheInputEvent event;
            if (!operands_ok || !action || generation <= 0
            || !the_input_event_from_workbench_invoke(action,
                   (unsigned long)generation,operand,&event)
            || !the_input_queue_push(&web_state.inputs,event))
               the_web_workbench_receipt_refuse(&web_state.workbench_receipts,
                  "invalid-operand",5,"Invalid workbench action or input queue full");
         }
         if (accepted == 1 || accepted == 0 || accepted == -3)
            web_driver_send_workbench_receipt(connection,
               the_web_workbench_receipt_find(&web_state.workbench_receipts,request_id));
         else web_driver_send_workbench_refusal(connection,request_id,
            accepted == -4 ? "request-id-conflict" : accepted == -6
               ? "receipt-unknown" : accepted == -5
               ? "invalid-operand" : "invalid-request-id",
            accepted == -4 ? "Request ID was already used for another action"
               : accepted == -6 ? "Receipt expired or unknown; reconcile before retrying"
               : accepted == -5 ? "Invalid workbench action or argument"
               : "Invalid workbench request ID");
         mg_free(request_id); mg_free(action); mg_free(operand);
      }
      else if (version == 1 && operation && !strcmp(operation,"log.list"))
      {
         char *model = mg_json_get_str(json,"$.model");
         char *generation_text = mg_json_get_str(json,"$.generation");
         char *cursor_text = mg_json_get_str(json,"$.cursor");
         uint64_t generation = 0, cursor = 0;
         long offset = mg_json_get_long(json,"$.offset",-1);
         long records = mg_json_get_long(json,"$.records",-1);
         long bytes = mg_json_get_long(json,"$.bytes",-1);
         char page[THE_WORKBENCH_JSON_MAX + 80];
         size_t length = 0;
         int valid = web_driver_workbench_string_field(json,"$.model",0)
         && web_driver_workbench_string_field(json,"$.generation",0)
         && web_driver_workbench_string_field(json,"$.cursor",0)
         && model && generation_text && cursor_text
         && the_web_parse_u64(generation_text,&generation) && generation
         && the_web_parse_u64(cursor_text,&cursor)
         && offset >= 0 && offset <= 262144 && records >= 1 && records <= 8
         && bytes >= 1 && bytes <= THE_WORKBENCH_LOG_JSON_BYTES_MAX;
         if (valid)
            length = the_web_format_log_page(page,sizeof(page),model,generation,
               cursor,(size_t)offset,(size_t)records,(size_t)bytes);
         if (length) web_driver_send(connection,page,length);
         else
         {
            const TheWorkbenchResult *result = valid ? the_workbench_last_result() : NULL;
            web_driver_send_workbench_refusal(connection,"",
               result && result->code && strcmp(result->code,"ok")
                  ? result->code : "invalid-operand",
               result && result->code && strcmp(result->code,"ok")
                  ? result->message : "Invalid log page request");
         }
         mg_free(model); mg_free(generation_text); mg_free(cursor_text);
      }
      else if (version == 1 && operation && !strcmp(operation,"form.list"))
      {
         char *form=mg_json_get_str(json,"$.form");
         long offset=mg_json_get_long(json,"$.offset",-1),limit=mg_json_get_long(json,"$.records",-1);
         long bytes=mg_json_get_long(json,"$.bytes",-1);
         char page[THE_WORKBENCH_JSON_MAX+80];
         size_t length=web_driver_workbench_string_field(json,"$.form",0)
            && offset>=0 && limit>=1 && limit<=8 && bytes>=1 && bytes<=8192
            ? the_web_format_form_page(page,sizeof(page),form,(size_t)offset,(size_t)limit,(size_t)bytes,0) : 0;
         if (length) web_driver_send(connection,page,length);
         else web_driver_send_workbench_refusal(connection,"","invalid-operand","Invalid form page request");
         mg_free(form);
      }
      else if (version == 1 && operation && !strcmp(operation,"filefield.list"))
      {
         char *panel=mg_json_get_str(json,"$.panel");
         char *generation_text=mg_json_get_str(json,"$.expectGeneration");
         char *revision_text=mg_json_get_str(json,"$.expectRevision");
         uint64_t generation=0,revision=0;
         long offset=mg_json_get_long(json,"$.offset",-1),limit=mg_json_get_long(json,"$.records",-1);
         long bytes=mg_json_get_long(json,"$.bytes",-1);
         char page[THE_WORKBENCH_JSON_MAX+80]; size_t length=0;
         const TheWorkbenchPanel *definition=panel ? the_workbench_panel_find(panel) : NULL;
         int valid=web_driver_workbench_string_field(json,"$.panel",0)
            && web_driver_workbench_string_field(json,"$.expectGeneration",0)
            && web_driver_workbench_string_field(json,"$.expectRevision",0)
            && the_web_parse_u64(generation_text,&generation) && generation && generation<=ULONG_MAX
            && the_web_parse_u64(revision_text,&revision) && revision
            && offset>=0 && offset<=THE_WORKBENCH_FILEFIELDS_RECORDS_MAX
            && limit>=1 && limit<=8 && bytes>=1 && bytes<=8192;
         if (valid && definition && definition->generation==generation)
         {
            const TheWorkbenchFileFieldsState *state=the_workbench_filefields_state(panel);
            if (state && state->draft_revision==revision)
               length=the_web_format_filefield_page(page,sizeof(page),panel,(size_t)offset,(size_t)limit,(size_t)bytes);
         }
         if (length) web_driver_send(connection,page,length);
         else
         {
            const TheWorkbenchFileFieldsState *state=valid ? the_workbench_filefields_state(panel) : NULL;
            const TheWorkbenchResult *result=valid && state && state->draft_revision==revision
               && definition && definition->generation==generation ? the_workbench_last_result() : NULL;
            web_driver_send_workbench_refusal(connection,"",!valid ? "invalid-operand"
               : !definition || definition->generation!=generation ? "stale-generation"
               : !state || state->draft_revision!=revision ? "stale-revision"
               : result && result->code ? result->code : "invalid-operand",
               result && result->message ? result->message : !valid ? "Invalid file-field page request"
               : "File fields changed; reload projection");
         }
         mg_free(panel);mg_free(generation_text);mg_free(revision_text);
      }
      else if (version == 1 && operation && (!strcmp(operation,"filefield.set")
         || !strcmp(operation,"filefield.edit")
         || !strcmp(operation,"filefield.apply") || !strcmp(operation,"filefield.cancel")
         || !strcmp(operation,"filefield.reload")))
      {
         TheWorkbenchFileFieldsRequest request; TheInputEvent event;
         char *request_id=mg_json_get_str(json,"$.requestId"),*panel=mg_json_get_str(json,"$.panel");
         char *field=mg_json_get_str(json,"$.field"),*value=mg_json_get_str(json,"$.value");
         char *edit_name=mg_json_get_str(json,"$.edit");
         char *generation_text=mg_json_get_str(json,"$.expectGeneration");
         char *revision_text=mg_json_get_str(json,"$.expectRevision");
         uint64_t generation=0; int accepted,valid;
         memset(&request,0,sizeof(request));
         request.operation=!strcmp(operation,"filefield.set") ? THE_WORKBENCH_FILEFIELDS_SET
            : !strcmp(operation,"filefield.edit") ? THE_WORKBENCH_FILEFIELDS_EDIT
            : !strcmp(operation,"filefield.apply") ? THE_WORKBENCH_FILEFIELDS_APPLY
            : !strcmp(operation,"filefield.cancel") ? THE_WORKBENCH_FILEFIELDS_CANCEL : THE_WORKBENCH_FILEFIELDS_RELOAD;
         valid=web_driver_workbench_string_field(json,"$.panel",0)
            && web_driver_workbench_string_field(json,"$.expectGeneration",0)
            && web_driver_workbench_string_field(json,"$.expectRevision",0)
            && panel && strlen(panel)<=THE_WORKBENCH_ID_MAX
            && the_web_parse_u64(generation_text,&generation) && generation && generation<=ULONG_MAX
            && the_web_parse_u64(revision_text,&request.draft_revision) && request.draft_revision;
         if (valid && request.operation==THE_WORKBENCH_FILEFIELDS_SET)
            valid=web_driver_workbench_string_field(json,"$.field",0)
               && web_driver_workbench_string_field(json,"$.value",0)
               && field && strlen(field)<=THE_WORKBENCH_ID_MAX
               && value && strlen(value)<=THE_WORKBENCH_FIELD_VALUE_MAX;
         if (valid && request.operation==THE_WORKBENCH_FILEFIELDS_EDIT)
         {
            long offset=mg_json_get_long(json,"$.offset",-1);
            long codepoint=mg_json_get_long(json,"$.codepoint",-1);
            valid=web_driver_workbench_string_field(json,"$.field",0)
               && web_driver_workbench_string_field(json,"$.edit",0)
               && field && strlen(field)<=THE_WORKBENCH_ID_MAX
               && edit_name && offset>=0 && offset<=THE_WORKBENCH_FIELD_VALUE_MAX;
            request.edit=!valid ? 0 : !strcmp(edit_name,"insert") ? THE_WORKBENCH_FORM_INSERT
               : !strcmp(edit_name,"backspace") ? THE_WORKBENCH_FORM_BACKSPACE
               : !strcmp(edit_name,"delete") ? THE_WORKBENCH_FORM_DELETE
               : !strcmp(edit_name,"clear") ? THE_WORKBENCH_FORM_CLEAR : 0;
            valid=valid && request.edit && (request.edit!=THE_WORKBENCH_FORM_INSERT
               || codepoint>=0 && codepoint<=0x10ffff
                  && !(codepoint>=0xd800 && codepoint<=0xdfff));
            request.cell=-1; request.utf16_offset=(size_t)offset;
            request.codepoint=codepoint<0 ? 0u : (uint32_t)codepoint;
         }
         if (valid)
         {
            strcpy(request.panel,panel);request.panel_generation=(unsigned long)generation;
            if (request.operation==THE_WORKBENCH_FILEFIELDS_SET)
            { strcpy(request.field,field);strcpy(request.value,value);request.length=strlen(value); }
            if (request.operation==THE_WORKBENCH_FILEFIELDS_EDIT) strcpy(request.field,field);
         }
         accepted=web_driver_workbench_string_field(json,"$.requestId",0)
            ? valid ? the_web_workbench_filefield_begin(&web_state.workbench_receipts,request_id,&request,&event) : -5 : -1;
         if (accepted==1 && !the_input_queue_push(&web_state.inputs,event))
            the_web_workbench_receipt_refuse(&web_state.workbench_receipts,"invalid-operand",5,"File-field input queue full");
         if (accepted==1 || accepted==0 || accepted==-3)
            web_driver_send_workbench_receipt(connection,the_web_workbench_receipt_find(&web_state.workbench_receipts,request_id));
         else web_driver_send_workbench_refusal(connection,request_id,
            accepted==-4 ? "request-id-conflict" : accepted==-6 ? "receipt-unknown"
               : accepted==-5 ? "invalid-operand" : "invalid-request-id",
            "Invalid or conflicting file-field request");
         mg_free(request_id);mg_free(panel);mg_free(field);mg_free(value);mg_free(edit_name);
         mg_free(generation_text);mg_free(revision_text);
      }
      else if (version == 1 && operation && (!strcmp(operation,"form.set")
         || !strcmp(operation,"form.submit") || !strcmp(operation,"form.cancel")))
      {
         TheWorkbenchFormRequest request; TheInputEvent event;
         char *request_id=mg_json_get_str(json,"$.requestId"),*form=mg_json_get_str(json,"$.form");
         char *field=mg_json_get_str(json,"$.field"),*value=mg_json_get_str(json,"$.value");
         char *panel=mg_json_get_str(json,"$.panel"),*generation=mg_json_get_str(json,"$.expectGeneration");
         char *revision=mg_json_get_str(json,"$.expectRevision"),*panel_generation=mg_json_get_str(json,"$.expectPanelGeneration");
         uint64_t gen=0,pgen=0; int accepted,valid;
         memset(&request,0,sizeof(request));
         request.operation=!strcmp(operation,"form.set") ? THE_WORKBENCH_FORM_SET
            : !strcmp(operation,"form.submit") ? THE_WORKBENCH_FORM_SUBMIT : THE_WORKBENCH_FORM_CANCEL;
         valid=web_driver_workbench_string_field(json,"$.form",0)
            && web_driver_workbench_string_field(json,"$.expectGeneration",0)
            && web_driver_workbench_string_field(json,"$.expectRevision",0)
            && web_driver_workbench_string_field(json,"$.panel",1)
            && web_driver_workbench_string_field(json,"$.expectPanelGeneration",1)
            && form && strlen(form)<=THE_WORKBENCH_ID_MAX
            && the_web_parse_u64(generation,&gen) && gen && gen<=ULONG_MAX
            && the_web_parse_u64(revision,&request.draft_revision) && request.draft_revision
            && (!panel || strlen(panel)<=THE_WORKBENCH_ID_MAX && panel_generation
               && the_web_parse_u64(panel_generation,&pgen) && pgen && pgen<=ULONG_MAX);
         if (valid && request.operation==THE_WORKBENCH_FORM_SET)
            valid=web_driver_workbench_string_field(json,"$.field",0)
               && web_driver_workbench_string_field(json,"$.value",0)
               && field && strlen(field)<=THE_WORKBENCH_ID_MAX && value && strlen(value)<=THE_WORKBENCH_FIELD_VALUE_MAX;
         if (valid)
         {
            strcpy(request.form,form); request.form_generation=(unsigned long)gen;
            if (panel) { strcpy(request.panel,panel); request.panel_generation=(unsigned long)pgen; }
            if (request.operation==THE_WORKBENCH_FORM_SET)
            { strcpy(request.field,field); strcpy(request.value,value); request.length=strlen(value); }
         }
         accepted=web_driver_workbench_string_field(json,"$.requestId",0)
            ? valid ? the_web_workbench_form_begin(&web_state.workbench_receipts,request_id,&request,&event) : -5 : -1;
         if (accepted==1 && !the_input_queue_push(&web_state.inputs,event))
            the_web_workbench_receipt_refuse(&web_state.workbench_receipts,"invalid-operand",5,"Form input queue full");
         if (accepted==1 || accepted==0 || accepted==-3)
            web_driver_send_workbench_receipt(connection,the_web_workbench_receipt_find(&web_state.workbench_receipts,request_id));
         else web_driver_send_workbench_refusal(connection,request_id,
            accepted==-4 ? "request-id-conflict" : accepted==-6 ? "receipt-unknown" : accepted==-5 ? "invalid-operand" : "invalid-request-id",
            "Invalid or conflicting form request");
         mg_free(request_id);mg_free(form);mg_free(field);mg_free(value);mg_free(panel);
         mg_free(generation);mg_free(revision);mg_free(panel_generation);
      }
      else if (version == 1 && operation && !strcmp(operation,"document.list"))
      {
         char *panel = mg_json_get_str(json,"$.panel");
         char *generation_text = mg_json_get_str(json,"$.expectGeneration");
         char *revision_text = mg_json_get_str(json,"$.expectRevision");
         uint64_t generation = 0, revision = 0;
         long line = mg_json_get_long(json,"$.line",-1);
         long offset = mg_json_get_long(json,"$.offset",-1);
         long records = mg_json_get_long(json,"$.records",-1);
         long bytes = mg_json_get_long(json,"$.bytes",-1);
         char page[THE_WORKBENCH_JSON_MAX + 80];
         size_t length = 0;
         const TheWorkbenchPanel *definition = panel ? the_workbench_panel_find(panel) : NULL;
         int valid = web_driver_workbench_string_field(json,"$.panel",0)
         && web_driver_workbench_string_field(json,"$.expectGeneration",0)
         && web_driver_workbench_string_field(json,"$.expectRevision",0)
         && the_web_parse_u64(generation_text,&generation) && generation
         && the_web_parse_u64(revision_text,&revision) && revision
         && line >= 1 && offset >= 0 && offset <= THE_WORKBENCH_DOCUMENT_RECORD_MAX
         && records >= 1 && records <= THE_WORKBENCH_DOCUMENT_LINES_MAX
         && bytes >= 1 && bytes <= THE_WORKBENCH_DOCUMENT_BYTES_MAX;
         int stale = valid && (!definition || definition->generation != generation);
         if (valid && !stale)
            length = the_web_format_document_page(page,sizeof(page),panel,revision,
               (size_t)line,(size_t)offset,(size_t)records,(size_t)bytes);
         if (length) web_driver_send(connection,page,length);
         else
         {
            const TheWorkbenchResult *result = valid && !stale ? the_workbench_last_result() : NULL;
            web_driver_send_workbench_refusal(connection,"",stale ? "stale-generation"
               : result && result->code && strcmp(result->code,"ok") ? result->code : "invalid-operand",
               stale ? "Panel generation is stale" : result && result->code && strcmp(result->code,"ok")
                  ? result->message : "Invalid document page request");
         }
         mg_free(panel); mg_free(generation_text); mg_free(revision_text);
      }
      else if (version == 1 && operation && !strcmp(operation,"panel.layout"))
      {
         char *panel = mg_json_get_str(json,"$.panel");
         char *generation_text = mg_json_get_str(json,"$.expectGeneration");
         char *placement = mg_json_get_str(json,"$.placement");
         uint64_t generation = 0;
         long cols = mg_json_get_long(json,"$.cols",-1);
         long rows = mg_json_get_long(json,"$.rows",-1);
         if (web_driver_workbench_string_field(json,"$.panel",0)
         && web_driver_workbench_string_field(json,"$.expectGeneration",0)
         && web_driver_workbench_string_field(json,"$.placement",0)
         && the_web_parse_u64(generation_text,&generation)
         && cols >= 0 && cols <= 10000 && rows >= 0 && rows <= 10000)
            the_web_panel_layout(panel,generation,placement,(int)cols,(int)rows);
         web_driver_publish_panel_state();
         mg_free(panel); mg_free(generation_text); mg_free(placement);
      }
      else if (version == 1 && operation &&
         (!strcmp(operation,"panel.show") || !strcmp(operation,"panel.hide")
         || !strcmp(operation,"panel.focus") || !strcmp(operation,"panel.return")))
      {
         char *request_id = mg_json_get_str(json,"$.requestId");
         char *panel = mg_json_get_str(json,"$.panel");
         char *generation_text = mg_json_get_str(json,"$.expectGeneration");
         uint64_t generation = 0;
         TheWorkbenchPanelOperation panel_operation =
            !strcmp(operation,"panel.show") ? THE_WORKBENCH_PANEL_SHOW
          : !strcmp(operation,"panel.hide") ? THE_WORKBENCH_PANEL_HIDE
          : !strcmp(operation,"panel.focus") ? THE_WORKBENCH_PANEL_FOCUS
          : THE_WORKBENCH_PANEL_RETURN;
         int operands_ok = web_driver_workbench_string_field(json,"$.panel",0)
            && web_driver_workbench_string_field(json,"$.expectGeneration",0)
            && the_web_parse_u64(generation_text,&generation)
            && generation > 0 && generation <= ULONG_MAX;
         int accepted = web_driver_workbench_string_field(json,"$.requestId",0)
            ? the_web_workbench_receipt_begin(&web_state.workbench_receipts,
               request_id,operation,panel,operands_ok ? (unsigned long)generation : 0,NULL)
            : -1;
         if (accepted == 1)
         {
            TheInputEvent event;
            if (!operands_ok || !the_input_event_from_workbench_panel(panel,
                 (unsigned long)generation,panel_operation,&event)
            || !the_input_queue_push(&web_state.inputs,event))
               the_web_workbench_receipt_refuse(&web_state.workbench_receipts,
                  "invalid-operand",5,"Invalid panel request or input queue full");
         }
         if (accepted == 1 || accepted == 0 || accepted == -3)
            web_driver_send_workbench_receipt(connection,
               the_web_workbench_receipt_find(&web_state.workbench_receipts,request_id));
         else web_driver_send_workbench_refusal(connection,request_id,
            accepted == -4 ? "request-id-conflict" : accepted == -6
               ? "receipt-unknown" : accepted == -5
               ? "invalid-operand" : "invalid-request-id",
            accepted == -4 ? "Request ID was already used for another operation"
               : accepted == -6 ? "Receipt expired or unknown; reconcile before retrying"
               : accepted == -5 ? "Invalid panel ID" : "Invalid workbench request ID");
         mg_free(request_id); mg_free(panel); mg_free(generation_text);
      }
      else web_driver_send_status(connection,"error",id,"invalid workbench operation");
      mg_free(operation); mg_free(type); return;
   }
   if (strcmp(type,"tool")==0) {
      TheInputEvent event;char*a=mg_json_get_str(json,"$.action"),*t=mg_json_get_str(json,"$.tool"),*text=mg_json_get_str(json,"$.text"),*args=mg_json_get_str(json,"$.arguments");
      queued=the_tools_prepare_lines(a?a:"",t?t:"",text?text:"",args?args:"",&event)&&the_input_queue_push(&web_state.inputs,event);
      web_driver_send_status(connection,queued?"ack":"error",id,queued?"queued":"invalid tool request");
      mg_free(a);mg_free(t);mg_free(text);mg_free(args);mg_free(type);return;
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
      web_state.workbench_revision = 0;
      memset(&web_state.workbench_reasons, 0, sizeof(web_state.workbench_reasons));
      web_state.workbench_panel_state[0] = '\0';
      len = snprintf(hello, sizeof(hello),
                     "{\"type\":\"hello\",\"protocol\":1,"
                     "\"driver\":\"web\",\"rows\":%d,\"cols\":%d,"
                     "\"workbenchSession\":\"%s\",\"workbenchNextRequest\":%llu}",
                     terminal_lines, terminal_cols, web_state.workbench_receipts.nonce,
                     web_state.workbench_receipts.high_water + 1);
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
   {
      char nonce[THE_WEB_WORKBENCH_NONCE_MAX + 1];
      mg_random_str(nonce, sizeof(nonce));
      the_web_workbench_receipts_init(&web_state.workbench_receipts, nonce);
   }
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
      {
         if (event->kind == THE_INPUT_WORKBENCH_INVOKE
         || event->kind == THE_INPUT_WORKBENCH_PANEL
         || event->kind == THE_INPUT_WORKBENCH_FORM
         || event->kind == THE_INPUT_WORKBENCH_FILEFIELDS)
            the_web_workbench_receipt_dispatched(&web_state.workbench_receipts);
         return event->kind != THE_INPUT_NONE;
      }
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
      the_tools_native_poll();
      web_driver_publish_tools();
      web_driver_publish_panel_state();
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
