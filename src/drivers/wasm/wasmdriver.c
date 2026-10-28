#include <emscripten.h>
#include <stdio.h>
#include <string.h>

#include "the.h"
#include "proto.h"
#include "vars.h"
#include "headlessdriver.h"
#include "websession.h"
#include "editorsource.h"
#include "tools.h"
void the_tools_wasm_initialise(int);
#ifdef THE_WASM_HIGHLIGHT
#include "wasm/highlighter.h"
#endif

static TheInputQueue inputs;
static TheDriverOps operations;
static int input_timeout = -1;
static void (*base_update)(void);
static char snapshot[262144];

int main(int argc, char **argv);
EMSCRIPTEN_KEEPALIVE int the_wasm_has_highlighter(void)
{
#ifdef THE_WASM_HIGHLIGHT
   return 1;
#else
   return 0;
#endif
}
EMSCRIPTEN_KEEPALIVE int the_wasm_has_crexx(void)
{
#ifdef USE_CREXX
   return 1;
#else
   return 0;
#endif
}
EMSCRIPTEN_KEEPALIVE int the_wasm_has_interpreter(void)
{
#ifdef THE_WASM_INTERPRETER
   return 1;
#else
   return 0;
#endif
}

EMSCRIPTEN_KEEPALIVE int the_wasm_main(void)
{
   char *argv[] = {"crxthe", "--driver", "web", "-s", "/workspace/Welcome.txt", NULL};
   return main(5, argv);
}

EM_JS(void, browser_send, (const char *text), {
   Module.sendEditorMessage(JSON.parse(UTF8ToString(text)));
});
EM_JS(void, browser_source, (int request, unsigned id, unsigned revision,
                           const char *source, size_t length, const char *path,
                           const char *name, const char *error), {
   Module.sendEditorMessage({type: 'run-source', request, id, revision,
      source: HEAPU8.slice(source, source + length),
      filename: UTF8ToString(path) + UTF8ToString(name), error: UTF8ToString(error)});
});

EM_ASYNC_JS(void,browser_import,(),{await Module.importTool(Module.editorRequest);});

EM_ASYNC_JS(int, browser_wait, (int timeout), {
   Module.editorRequest = await Module.nextEditorRequest(timeout);
   return Module.editorRequest ? 1 : 0;
});

EM_JS(int, browser_string, (const char *field, char *out, int capacity), {
   const value = Module.editorRequest[UTF8ToString(field)];
   if (value === undefined) { HEAPU8[out] = 0; return 1; }
   if (typeof value !== 'string' || lengthBytesUTF8(value) >= capacity) return 0;
   stringToUTF8(value, out, capacity);
   return 1;
});

EM_JS(int, browser_integer, (const char *field, int fallback), {
   const value = Module.editorRequest[UTF8ToString(field)];
   return Number.isSafeInteger(value) && value >= -2147483648 && value <= 2147483647 ? value : fallback;
});

EM_JS(char *,browser_payload,(const char *field,int limit),{
 const value=Module.editorRequest[UTF8ToString(field)];if(typeof value!=='string')return 0;const n=lengthBytesUTF8(value)+1;if(n>limit)return 0;const p=_malloc(n);if(p)stringToUTF8(value,p,n);return p;
});
EM_JS(void,browser_output,(unsigned id,int fd),{const b=Module.editorRequest.bytes;if(!(b instanceof Uint8Array)||b.length>1048576)return;const p=_malloc(b.length);if(p){HEAPU8.set(b,p);_the_tools_wasm_output(id,fd,p,b.length);_free(p);}});
EMSCRIPTEN_KEEPALIVE void the_tools_wasm_output(unsigned id,int fd,const void *b,size_t n){the_tool_output(id,fd,b,n);}
#ifdef THE_WASM_HIGHLIGHT
EM_JS(char *, browser_parser_payload, (), {
   const value = Module.editorRequest.payload;
   if (typeof value !== 'string' || value.length > 8388608) return 0;
   const size = lengthBytesUTF8(value) + 1;
   if (size > 8388609) return 0;
   const pointer = _malloc(size);
   if (pointer) stringToUTF8(value, pointer, size);
   return pointer;
});
#endif

EM_ASYNC_JS(int, the_wasm_flush_workspace, (), {
   try { await Module.flushWorkspace(); return 0; }
   catch (error) { return 1; }
});

static void publish(void)
{
   const TheEditorSource *source = NULL;
#if defined(THE_WASM_HIGHLIGHT) || defined(THE_WASM_INTERPRETER)
   source = the_editor_source_prepare();
#endif
#ifdef THE_WASM_HIGHLIGHT
   the_wasm_highlight_prepare();
#endif
   if (the_web_format_snapshot(snapshot, sizeof(snapshot)))
   {
      size_t used = strlen(snapshot);
      if (source && used && snapshot[used-1] == '}') {
         char metadata[128];
         int added = snprintf(metadata, sizeof(metadata),
                  ",\"source\":{\"id\":%u,\"revision\":%u}}", source->id, source->revision);
         if (added > 0 && used + added <= sizeof(snapshot))
            memcpy(snapshot + used - 1, metadata, added + 1);
      }
#ifdef THE_WASM_HIGHLIGHT
      size_t length = strlen(snapshot);
      if (length && snapshot[length-1] == '}') {
         char metadata[256];
         int added = the_wasm_highlight_metadata(metadata, sizeof(metadata));
         if (added > 0 && (size_t)added < sizeof(metadata)
             && length + added + 2 <= sizeof(snapshot)) {
            snapshot[length-1] = ',';
            memcpy(snapshot + length, metadata, added);
            strcpy(snapshot + length + added, "}");
         }
      }
#endif
      browser_send(snapshot);
      {static char tools[1800000];if(the_tool_format_json(tools,sizeof(tools)))browser_send(tools);}
   }
}

static int activate(void)
{
   the_tools_wasm_initialise(the_wasm_has_interpreter());
   the_input_queue_init(&inputs);
   headless_driver_reset();
   return 1;
}

static int startup(const TheDriverStartupOptions *options, char *error, size_t capacity)
{
   char catalog[4096];
   (void)options; (void)error; (void)capacity;
   browser_send("{\"type\":\"hello\",\"protocol\":1,\"driver\":\"wasm\"}");
   if (the_wasm_has_interpreter()) browser_send("{\"type\":\"interpreter\",\"available\":true}");
   if (the_web_format_actions(catalog, sizeof(catalog))) browser_send(catalog);
   return 1;
}

static int read_input(TheInputEvent *event)
{
   for (;;)
   {
      char type[32], value[1024], argument[THE_FRONTEND_ACTION_ARGUMENT_MAX + 1];
      const char *field;
      int queued;
      if (the_input_queue_pop(&inputs, event)) return 1;
      if (!browser_wait(input_timeout)) return 0;
      if (!browser_string("type", type, sizeof(type))) continue;
#ifdef THE_WASM_INTERPRETER
      if (strcmp(type, "run-source") == 0) {
         const TheEditorSource *source = the_editor_source_prepare();
         browser_source(browser_integer("id", 0), source ? source->id : 0,
                        source ? source->revision : 0, source ? source->bytes : "",
                        source ? source->length : 0,
                        CURRENT_FILE ? (char *)CURRENT_FILE->fpath : "",
                        CURRENT_FILE ? (char *)CURRENT_FILE->fname : "",
                        source ? "" : the_editor_source_error());
         publish();
         continue;
      }
#endif
#ifdef THE_WASM_HIGHLIGHT
      if (strcmp(type, "parser-restart") == 0) {
         the_wasm_highlight_restart();
         publish();
         continue;
      }
      if (strcmp(type, "parser-result") == 0) {
         char *payload = browser_parser_payload();
         the_wasm_highlight_reply(browser_integer("id", 0), browser_integer("revision", 0),
                                  payload, browser_integer("failed", 0));
         free(payload);
         publish();
         continue;
      }
#endif
      if(!strcmp(type,"tool-import")){browser_import();publish();continue;}
      if(!strcmp(type,"tool")){
         char a[40],t[64],*text=browser_payload("text",1048577),*args=browser_payload("arguments",1048577);TheInputEvent e;
         if(browser_string("action",a,sizeof(a))&&browser_string("tool",t,sizeof(t))&&the_tools_prepare_lines(a,t,text?text:"",args?args:"",&e))the_input_queue_push(&inputs,e);
         free(text);free(args);continue;
      }
      if(!strcmp(type,"tool-event")){
         char kind[32];unsigned id=browser_integer("job",0);browser_string("event",kind,sizeof(kind));
         if(!strcmp(kind,"output"))browser_output(id,browser_integer("fd",1));
         else if(!strcmp(kind,"started"))the_tool_started(id);
         else if(!strcmp(kind,"waiting"))the_tool_waiting(id,browser_integer("waiting",0));
         else if(!strcmp(kind,"built")){char path[THE_TOOL_PATH];if(browser_string("path",path,sizeof(path)))the_tools_profile_built(id,path,browser_integer("sourceId",0),browser_integer("revision",0));}
         else if(!strcmp(kind,"finish")){char *wire=browser_payload("wire",3145729),*error=browser_payload("error",4097),status[32];browser_string("status",status,sizeof(status));the_tool_finish(id,browser_integer("code",-1),status,error,wire);free(wire);free(error);}
         publish();continue;
      }
      if (strcmp(type, "snapshot") == 0) { publish(); continue; }
      field = strcmp(type,"key")==0 ? "key" : strcmp(type,"text")==0 ? "text"
            : strcmp(type,"command")==0 ? "command" : strcmp(type,"action")==0 ? "action" : "target";
      queued = browser_string(field, value, sizeof(value))
            && browser_string("argument", argument, sizeof(argument))
            && the_web_queue_request(&inputs, type, value, argument,
                 browser_integer("line",0), browser_integer("row",-1),
                 browser_integer("cell",-1), browser_integer("rows",0),
                 browser_integer("cols",0));
      if (!queued) browser_send("{\"type\":\"error\",\"message\":\"Invalid browser input or input queue full\"}");
   }
}

static void update(void)
{
   if (base_update) base_update();
   publish();
}

static void set_timeout(int milliseconds) { input_timeout = milliseconds; }
static const char *ui_version(void) { return "web/wasm"; }
static CursorPresentation cursor_presentation(void) { return CURSOR_PRESENTATION_SOFTWARE; }
static bool software_cursor(void) { return TRUE; }
static void shutdown_driver(int prompt) { (void)prompt; headless_driver_reset(); }

const TheDriverModuleLifecycle the_wasm_driver_lifecycle = {
   .name = "web", .activate = activate, .start = startup,
   .shutdown = shutdown_driver,
   .set_current_screen = headless_driver_set_current_screen,
   .set_screen_current_role = headless_driver_set_screen_current_role,
   .create_screen_role = headless_driver_create_screen_role,
   .create_global_window = headless_driver_create_global_window,
   .log_count = headless_driver_log_count,
   .log_entry = headless_driver_log_entry,
   .ui_version = ui_version,
   .current_cursor_presentation = cursor_presentation,
   .current_cursor_uses_software = software_cursor
};

const TheDriverOps *the_wasm_driver_operations(void)
{
   operations = the_headless_driver_ops;
   base_update = operations.update;
   operations.read_input_event = read_input;
   operations.set_current_window_timeout = set_timeout;
   operations.update = update;
   return &operations;
}
