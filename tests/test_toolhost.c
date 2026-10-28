#include "toolhost.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static unsigned running, stopped;
static size_t received;
static int start(const TheToolRequest *r, char *error, size_t cap) {
   (void)error;
   (void)cap;
   running = r->id;
   the_tool_started(r->id);
   return 1;
}
static int input(unsigned id, const void *p, size_t n) {
   assert(id == running);
   assert(p);
   received += n;
   return 1;
}
static int eof(unsigned id) { return id == running; }
static void stop(unsigned id) { stopped = id; }
int main(void) {
   TheToolAdapter adapter = {start, input, eof, stop, NULL};
   TheToolDefinition definition = {0};
   TheToolRequest request = {0};
   char error[256], text[1000];
   char *json = malloc(2097152);
   unsigned old;
   the_tool_host_reset();
   the_tool_host_adapter(&adapter);
   strcpy(definition.id, "classic-rexx");
   strcpy(definition.label, "Classic Rexx");
   strcpy(definition.role, "runner");
   strcpy(definition.extensions, ".rexx,.rex");
   definition.available = 1;
   assert(the_tool_register(&definition));
   assert(the_tool_count() == 1);
   assert(the_tool_for_file("/work/hello.rexx", "runner"));
   request.tool = definition;
   request.filename = "/work/hello.rexx";
   request.source = "say 'hello'\n";
   request.source_length = strlen(request.source);
   request.source_id = 7;
   request.revision = 3;
   assert(the_tool_launch(&request, error, sizeof(error)));
   old = running;
   assert(the_tool_console()->source_id == 7 && the_tool_console()->revision == 3);
   assert(!the_tool_launch(&request, error, sizeof(error)));
   the_tool_output(old, 1, "\xf0\x9f", 2);
   the_tool_output(old, 2, "err", 3);
   the_tool_output(old, 1, "\x98\x80\n", 3);
   assert(the_tool_console()->count == 2);
   assert(!strcmp(the_tool_console()->chunks[0].text, "err"));
   assert(!strcmp(the_tool_console()->chunks[1].text, "😀\n"));
   assert(the_tool_input("Zoë\n", 5));
   assert(received == 5);
   assert(the_tool_eof());
   assert(!the_tool_input("x", 1));
   the_tool_stop("Stopped");
   assert(stopped == old);
   the_tool_finish(old, 0, "0", NULL, NULL);
   assert(!strcmp(the_tool_console()->state, "Stopped"));
   assert(the_tool_launch(&request, error, sizeof(error)));
   the_tool_output(old, 1, "stale", 5);
   assert(the_tool_console()->bytes == 0);
   the_tool_finish(
       running, 0, NULL, NULL,
       "LLB1\nindicator=N\nhasPrimary=0\nhasStatus=1\nstatus=9223372036854775807\nprimaryNameHex="
       "\nprimaryMessageHex=\nprimaryCode=0\nprimaryModule=0\nprimaryAddress=0\nhasValue="
       "0\nvalueHex=\nhasCleanup=0\ncleanupNameHex=\ncleanupMessageHex=\ncleanupCode="
       "0\ncleanupModule=0\ncleanupAddress=0\ntruncated=0\n");
   assert(!strcmp(the_tool_console()->status, "9223372036854775807"));
   assert(the_tool_launch(&request, error, sizeof(error)));
   the_tool_finish(
       running, 0, "0", NULL,
       "LLB1\nindicator=E\nhasPrimary=1\nhasStatus=0\nstatus=0\nprimaryNameHex="
       "4552524F52\nprimaryMessageHex=626164\nprimaryCode=0\nprimaryModule=0\nprimaryAddress="
       "0\nhasValue=0\nvalueHex=\nhasCleanup=0\ncleanupNameHex=\ncleanupMessageHex=\ncleanupCode="
       "0\ncleanupModule=0\ncleanupAddress=0\ntruncated=0\n");
   assert(!strcmp(the_tool_console()->state, "Runtime condition"));
   the_tool_console_text(text, sizeof(text));
   assert(strstr(text, "ERROR: bad"));
   assert(the_tool_format_json(json, 2097152));
   assert(strstr(json, "\"catalog\""));
   assert(the_tool_launch(&request, error, sizeof(error)));
   {
      char *large = malloc(THE_TOOL_OUTPUT + 100);
      memset(large, 'x', THE_TOOL_OUTPUT + 100);
      the_tool_output(running, 1, large, THE_TOOL_OUTPUT + 100);
      free(large);
      assert(the_tool_console()->bytes == THE_TOOL_OUTPUT);
      assert(the_tool_console()->omitted);
   }
   the_tool_output(running, 1, "x", THE_TOOL_OUTPUT_LIMIT);
   assert(!strcmp(the_tool_console()->state, "Output limit reached"));
   the_tool_clear();
   assert(the_tool_console()->bytes == 0);
   free(json);
   the_tool_host_reset();
   puts("tool host contracts pass");
   return 0;
}
