#include "tools.h"
#include <emscripten.h>
#include <string.h>
EM_JS(int, wasm_start,
      (unsigned id, unsigned source_id, unsigned revision, const char *role, const char *entry,
       const char *image, const char *filename, const char *source, size_t length, int argc,
       const char *const *argv),
      {
         const args = [];
         for (let i = 0; i < argc; i++)
            args.push(UTF8ToString(HEAPU32[(argv >> 2) + i]));
         Module.startTool({
            id,
            sourceId : source_id,
            revision,
            role : UTF8ToString(role),
            entry : UTF8ToString(entry),
            image : UTF8ToString(image),
            filename : UTF8ToString(filename),
            source : HEAPU8.slice(source, source + length),
            arguments : args
         });
         return 1;
      });
EM_JS(int, wasm_input, (unsigned id, const void *bytes, size_t length),
      { return Module.toolInput(id, HEAPU8.slice(bytes, bytes + length)) ? 1 : 0; });
EM_JS(int, wasm_eof, (unsigned id), { return Module.toolEof(id) ? 1 : 0; });
EM_JS(void, wasm_stop, (unsigned id), { Module.stopTool(id); });
static int start(const TheToolRequest *r, char *error, size_t cap) {
   (void)error;
   (void)cap;
   return wasm_start(r->id, r->source_id, r->revision, r->tool.role, r->tool.entry, r->tool.image,
                     r->filename ? r->filename : "", r->source ? r->source : "", r->source_length,
                     r->argc, r->argv);
}
static void capability(const char *id, int enabled) {
   TheToolDefinition t = *the_tool_find(id);
   t.available = enabled;
   the_tool_register(&t);
}
void the_tools_wasm_initialise(int enabled) {
   TheToolAdapter a = {start, wasm_input, wasm_eof, wasm_stop, NULL};
   the_tools_initialise(&a, enabled);
#ifdef THE_WASM_PROFILE_COMPILER
   capability("profile-compiler", 1);
#else
   capability("profile-compiler", 0);
#endif
#ifdef THE_WASM_HIGHLIGHT
   capability("classic-highlight", 1);
#else
   capability("classic-highlight", 0);
#endif
}
