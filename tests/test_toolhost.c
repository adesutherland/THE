#include "toolhost.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static unsigned running, stopped;
static size_t received;
static int start_failure, start_output, require_begin;
static unsigned starts, begins, ends, observed_job;
static int begin_failure, empty_binding, bound, binding_token;
static uint64_t last_sequence;
typedef struct {
   unsigned job;
   int fd;
   uint64_t sequence;
   size_t length;
   char bytes[32];
} ObservedText;
static ObservedText observed[32];
static size_t observed_count;
static int start(const TheToolRequest *r, char *error, size_t cap) {
   starts++;
   if (require_begin) assert(bound && observed_job == r->id);
   running = r->id;
   if (start_output) the_tool_output(r->id, 1, "s\xe2", 2);
   if (start_failure) {
      snprintf(error, cap, "Injected backend failure");
      return 0;
   }
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
static int observe_begin(unsigned id, const char *tool, void **binding, char *error, size_t cap) {
   assert(!bound && !strcmp(tool, "classic-rexx"));
   begins++;
   if (begin_failure) {
      snprintf(error, cap, "Injected route refusal");
      return 0;
   }
   observed_job = id;
   last_sequence = 0;
   *binding = empty_binding ? NULL : &binding_token;
   bound = 1;
   return 1;
}
static void observe_text(void *binding, unsigned id, int fd, uint64_t sequence,
                          const void *bytes, size_t length) {
   ObservedText *text;
   assert(bound && binding == (empty_binding ? NULL : &binding_token));
   assert(id == observed_job && (fd == 1 || fd == 2));
   assert(sequence == last_sequence + 1);
   assert(observed_count < sizeof(observed) / sizeof(*observed));
   assert(length && length <= sizeof(observed[0].bytes));
   last_sequence = sequence;
   text = observed + observed_count++;
   text->job = id;
   text->fd = fd;
   text->sequence = sequence;
   text->length = length;
   memcpy(text->bytes, bytes, length);
}
static void observe_end(void *binding, unsigned id) {
   assert(bound && binding == (empty_binding ? NULL : &binding_token));
   assert(id == observed_job);
   bound = 0;
   ends++;
}

static void observer_contract(const TheToolRequest *request) {
   TheToolAdapter adapter = {start, input, eof, stop, NULL};
   TheToolObserver observer = {observe_begin, observe_text, observe_end};
   TheToolObserver incomplete = {observe_begin, NULL, observe_end};
   char error[256];
   unsigned id, prior_starts, prior_begins, prior_ends;
   const char control[] = {'x', 0, 'y'};
   the_tool_host_reset();
   the_tool_host_adapter(&adapter);
   assert(!the_tool_host_observer(&incomplete));
   assert(the_tool_host_observer(&observer));
   require_begin = 1;
   assert(the_tool_launch(request, error, sizeof(error)));
   id = running;
   assert(begins == 1 && !ends && bound);
   assert(!the_tool_host_observer(NULL) && !the_tool_host_observer(&observer));
   prior_starts = starts;
   assert(!the_tool_launch(request, error, sizeof(error)));
   assert(starts == prior_starts && begins == 1);
   the_tool_output(id + 100, 1, "stale", 5);
   the_tool_output(id, 0, "not stdout", 10);
   the_tool_output(id, 3, "not stderr", 10);
   the_tool_output(id, 1, "\xf0\x9f", 2);
   assert(!observed_count);
   the_tool_output(id, 2, "err", 3);
   the_tool_profile_output(1, "profile", 7);
   assert(the_tool_input("input\n", 6));
   assert(the_tool_eof());
   assert(observed_count == 1 && observed[0].fd == 2 && observed[0].sequence == 1);
   assert(!memcmp(observed[0].bytes, "err", 3));
   the_tool_console_visibility(0, 0);
   the_tool_clear();
   assert(the_tool_console()->running && !ends && bound);
   the_tool_output(id, 1, "\x98\x80\n", 3);
   assert(observed_count == 2 && observed[1].fd == 1 && observed[1].length == 5);
   assert(!memcmp(observed[1].bytes, "\xf0\x9f\x98\x80\n", 5));
   the_tool_output(id, 2, control, sizeof(control));
   assert(observed_count == 3 && observed[2].length == sizeof(control));
   assert(!memcmp(observed[2].bytes, control, sizeof(control)));
   the_tool_output(id, 1, "\xff", 1);
   assert(observed_count == 4 && observed[3].fd == 1 && observed[3].length == 3);
   assert(!memcmp(observed[3].bytes, "\xef\xbf\xbd", 3));
   the_tool_output(id, 2, "\xe2", 1);
   the_tool_finish(id + 100, 0, "0", NULL, NULL);
   assert(!ends && bound);
   the_tool_finish(id, 0, "0", NULL,
       "LLB1\nindicator=E\nhasPrimary=1\nhasStatus=0\nstatus=0\nprimaryNameHex="
       "4552524F52\nprimaryMessageHex=626164\nprimaryCode=0\nprimaryModule=0\nprimaryAddress="
       "0\nhasValue=0\nvalueHex=\nhasCleanup=0\ncleanupNameHex=\ncleanupMessageHex=\ncleanupCode="
       "0\ncleanupModule=0\ncleanupAddress=0\ntruncated=0\n");
   assert(observed_count == 5 && observed[4].fd == 2 && observed[4].sequence == 5);
   assert(observed[4].length == 3 && !memcmp(observed[4].bytes, "\xef\xbf\xbd", 3));
   assert(ends == 1 && !bound && !strcmp(the_tool_console()->state, "Runtime condition"));
   the_tool_finish(id, 0, "0", NULL, NULL);
   the_tool_stop("Stopped");
   assert(ends == 1 && observed_count == 5);

   /* Empty route sets can succeed without a lease pointer. Stop flushes
    * previously admitted partial text and ends exactly once. */
   empty_binding = 1;
   observed_count = 0;
   assert(the_tool_launch(request, error, sizeof(error)));
   id = running;
   the_tool_output(id, 1, "\xe2", 1);
   the_tool_stop("Stopped");
   assert(stopped == id && observed_count == 1 && observed[0].sequence == 1);
   assert(observed[0].length == 3 && !memcmp(observed[0].bytes, "\xef\xbf\xbd", 3));
   assert(ends == 2 && !bound);
   the_tool_finish(id, -1, NULL, "late failure", NULL);
   assert(ends == 2 && observed_count == 1);
   empty_binding = 0;

   /* A refused route never starts the backend. Its failed begin owns cleanup. */
   begin_failure = 1;
   prior_starts = starts;
   prior_ends = ends;
   assert(!the_tool_launch(request, error, sizeof(error)));
   assert(starts == prior_starts && ends == prior_ends && !bound);
   assert(!strcmp(error, "Injected route refusal") && !strcmp(the_tool_console()->state, "Failed"));
   begin_failure = 0;

   /* Synchronous backend output is already observed; start failure still
    * flushes and releases a successful binding exactly once. */
   observed_count = 0;
   start_failure = start_output = 1;
   assert(!the_tool_launch(request, error, sizeof(error)));
   assert(ends == prior_ends + 1 && !bound && observed_count == 2);
   assert(observed[0].sequence == 1 && observed[0].length == 1 && observed[0].bytes[0] == 's');
   assert(observed[1].sequence == 2 && observed[1].length == 3);
   assert(!strcmp(error, "Injected backend failure"));
   the_tool_stop("Stopped");
   assert(ends == prior_ends + 1);
   start_failure = start_output = 0;

   /* The existing hard output limit rejects the entire over-limit chunk;
    * only previously admitted decoded text reaches the observer. */
   observed_count = 0;
   assert(the_tool_launch(request, error, sizeof(error)));
   id = running;
   the_tool_output(id, 1, "x", 1);
   prior_ends = ends;
   the_tool_output(id, 2, "x", THE_TOOL_OUTPUT_LIMIT);
   assert(observed_count == 1 && observed[0].sequence == 1);
   assert(ends == prior_ends + 1 && !bound);
   assert(!strcmp(the_tool_console()->state, "Output limit reached"));

   assert(the_tool_host_observer(NULL));
   prior_begins = begins;
   prior_ends = ends;
   require_begin = 0;
   assert(the_tool_launch(request, error, sizeof(error)));
   the_tool_output(running, 1, "plain", 5);
   the_tool_finish(running, -1, NULL, "error is not program text", NULL);
   assert(begins == prior_begins && ends == prior_ends && observed_count == 1);

   assert(the_tool_host_observer(&observer));
   require_begin = 1;
   assert(the_tool_launch(request, error, sizeof(error)));
   prior_ends = ends;
   the_tool_host_reset();
   assert(ends == prior_ends + 1 && !bound);
   the_tool_host_reset();
   assert(ends == prior_ends + 1);
   the_tool_host_adapter(&adapter);
   require_begin = 0;
   prior_begins = begins;
   assert(the_tool_launch(request, error, sizeof(error)));
   the_tool_finish(running, 0, "0", NULL, NULL);
   assert(begins == prior_begins);
}
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
   observer_contract(&request);
   free(json);
   the_tool_host_reset();
   puts("tool host contracts pass");
   return 0;
}
