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
} TheToolRequest;
typedef struct {
   int (*start)(const TheToolRequest *, char *, size_t);
   int (*input)(unsigned, const void *, size_t);
   int (*eof)(unsigned);
   void (*stop)(unsigned);
   void (*poll)(void);
} TheToolAdapter;
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
} TheToolConsole;

void the_tool_host_reset(void);
void the_tool_host_adapter(const TheToolAdapter *adapter);
int the_tool_register(const TheToolDefinition *tool);
const TheToolDefinition *the_tool_find(const char *id);
const TheToolDefinition *the_tool_at(size_t index);
size_t the_tool_count(void);
const TheToolDefinition *the_tool_for_file(const char *filename, const char *role);
int the_tool_launch(const TheToolRequest *request, char *error, size_t capacity);
const TheToolConsole *the_tool_console(void);
void the_tool_started(unsigned id);
void the_tool_waiting(unsigned id, int waiting);
void the_tool_output(unsigned id, int fd, const void *bytes, size_t length);
void the_tool_finish(unsigned id, int runtime_code, const char *status, const char *error,
                     const char *llb);
void the_tool_stop(const char *reason);
int the_tool_input(const void *bytes, size_t length);
int the_tool_eof(void);
void the_tool_clear(void);
void the_tool_profile_output(int, const void *, size_t);
void the_tool_console_visibility(int visible, int focused);
int the_tool_console_key(uint32_t codepoint, int key);
void the_tool_poll(void);
size_t the_tool_format_json(char *out, size_t capacity);
size_t the_tool_console_text(char *out, size_t capacity);
#endif
