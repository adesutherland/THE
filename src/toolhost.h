#ifndef THE_TOOLHOST_H
#define THE_TOOLHOST_H
#include <stddef.h>
#include <stdint.h>

#define THE_TOOL_LIMIT 48
#define THE_TOOL_ARGS 128
#define THE_TOOL_PATH 2048
#define THE_TOOL_OUTPUT 262144u
#define THE_TOOL_OUTPUT_LIMIT 8388608u

typedef struct {
   char id[64], label[96], role[16], entry[24];
   char image[THE_TOOL_PATH], extensions[128];
   int available;
} TheToolDefinition;
typedef struct {
   unsigned id, source_id, revision;
   TheToolDefinition tool;
   const char *filename, *source;
   size_t source_length;
   int argc;
   const char *const *argv;
   int finite_input; /* Host-owned: require complete queued stdin and EOF. */
} TheToolRequest;
typedef struct {
   int (*start)(const TheToolRequest *, char *, size_t);
   int (*input)(unsigned, const void *, size_t);
   int (*eof)(unsigned);
   void (*stop)(unsigned);
   void (*poll)(void);
} TheToolAdapter;
/* Optional session-owned output observer. begin runs after job ID allocation,
 * before backend.start, and may refuse the launch. It cleans up its own partial
 * state on failure. Every successful begin gets exactly one end, including a
 * backend start failure. A successful begin may return a NULL binding.
 * text receives complete, decoded UTF-8 program stdout/stderr only, with a
 * job-global sequence starting at 1 across both streams. Final decoder flushes
 * precede end. Profile output, stdin echoes and condition/status text are not
 * program output. Callbacks must not reenter the tool host. Borrowed text is
 * valid only during the callback; the binding belongs to the observer. */
typedef struct {
   int (*begin)(unsigned job_id, const char *tool_id, void **binding,
                char *error, size_t capacity);
   void (*text)(void *binding, unsigned job_id, int fd, uint64_t sequence,
                const void *bytes, size_t length);
   void (*end)(void *binding, unsigned job_id);
} TheToolObserver;
/* Optional exact-byte IO owner, independent of the decoded log observer.
 * begin runs before the backend starts and may supply immutable finite stdin.
 * raw runs before decoding. end is called exactly once after every successful
 * begin, including failed start/Stop/reset; success requires drained streams,
 * runtime/status zero and no condition. It must release the binding and may
 * refuse publication. No callback may reenter this host. */
typedef struct {
   int (*begin)(unsigned,const char *,void **,const void **,size_t *,int *,char *,size_t);
   int (*raw)(void *,unsigned,int,const void *,size_t,char *,size_t);
   int (*end)(void *,unsigned,int,char *,size_t);
} TheToolIO;
typedef struct {
   int fd;
   char *text;
   size_t length;
} TheToolChunk;
typedef struct {
   unsigned id, version, source_id, revision;
   int running, waiting, eof, visible, focused, omitted;
   char state[160], filename[THE_TOOL_PATH], tool[64], status[32];
   char input[4097];
   size_t input_length;
   TheToolChunk chunks[128];
   size_t count, bytes, total;
   /* Successfully appended display UTF-8 bytes, before bounded eviction.
    * Includes replacements/echoes/captures; resets on clear or a new job.
    * Raw output total and its limit are independent of this reader progress. */
   uint64_t displayed;
   /* Changes on either lane clear, even if subsequent text is identical.
    * Preserved with lane history; readers also bind to the job ID. */
   uint64_t history_epoch;
} TheToolConsole;

void the_tool_host_reset(void);
void the_tool_host_adapter(const TheToolAdapter *adapter);
/* All three callbacks are required. NULL disables observation. Changes are
 * refused while a job/binding is active; reset stops and releases before
 * clearing the observer configuration. */
int the_tool_host_observer(const TheToolObserver *observer);
int the_tool_host_io(const TheToolIO *io);
int the_tool_register(const TheToolDefinition *tool);
/* Owner-only profile rollback; saved definitions must come from this catalog. */
int the_tool_remove(const char *id);
int the_tool_catalog_restore(const TheToolDefinition *,size_t);
const TheToolDefinition *the_tool_find(const char *id);
const TheToolDefinition *the_tool_at(size_t index);
size_t the_tool_count(void);
const TheToolDefinition *the_tool_for_file(const char *filename, const char *role);
int the_tool_launch(const TheToolRequest *request, char *error, size_t capacity);
const TheToolConsole *the_tool_console(void);
/* Active backend lifecycle remains above; frontends retain independent output
 * lanes through these accessors. Compiler/macro captures never erase Program. */
const TheToolConsole *the_tool_program_console(void);
const TheToolConsole *the_tool_build_console(void);
int the_tool_build_capture_begin(const char *,const char *,unsigned,unsigned);
void the_tool_build_capture_end(short);
int the_tool_program_capture_begin(const char *,const char *,unsigned,unsigned);
void the_tool_program_capture_end(short);
void the_tool_started(unsigned id);
void the_tool_waiting(unsigned id, int waiting);
void the_tool_output(unsigned id, int fd, const void *bytes, size_t length);
void the_tool_finish(unsigned id, int runtime_code, const char *status, const char *error,
                     const char *llb);
void the_tool_stop(const char *reason);
int the_tool_input(const void *bytes, size_t length);
int the_tool_eof(void);
void the_tool_clear(void);
void the_tool_clear_program(void);
void the_tool_profile_output(int, const void *, size_t);
void the_tool_console_visibility(int visible, int focused);
/* Program presentation stays independent while a compiler/Build lane is active. */
void the_tool_program_console_visibility(int visible, int focused);
int the_tool_console_key(uint32_t codepoint, int key);
void the_tool_poll(void);
size_t the_tool_format_json(char *out, size_t capacity);
size_t the_tool_console_text(char *out, size_t capacity);
#endif
