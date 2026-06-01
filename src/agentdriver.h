#ifndef THE_AGENTDRIVER_H
#define THE_AGENTDRIVER_H

#include <stddef.h>

#include "inputevent.h"
#include "llmdriver.h"

#define AGENT_DRIVER_STATUS_MAX 256
#define AGENT_DRIVER_PATH_MAX 1024
#define AGENT_DRIVER_SEARCH_MAX 256
#define AGENT_DRIVER_HISTORY_MAX 32
#define AGENT_DRIVER_BUFFER_MAX 8
#define AGENT_DRIVER_PROJECT_MAX 32
#define AGENT_DRIVER_PROJECT_NAME_MAX 128

typedef struct
{
   CHARTYPE *text;
   size_t len;
   size_t cap;
   char prefix_command[LLM_DRIVER_MAX_PREFIX + 1];
} AgentDriverLine;

typedef struct
{
   char *text;
   char path[AGENT_DRIVER_PATH_MAX];
   int dirty;
   size_t cursor_line;
   size_t top_line;
   int cursor_cell;
   int desired_cell;
   char search_text[AGENT_DRIVER_SEARCH_MAX + 1];
} AgentDriverSnapshot;

typedef struct
{
   int used;
   AgentDriverSnapshot snapshot;
} AgentDriverBuffer;

typedef struct
{
   AgentDriverLine *lines;
   size_t line_count;
   size_t line_cap;
   char path[AGENT_DRIVER_PATH_MAX];
   int dirty;
   int rows;
   int cols;
   LogicalCursorZone focus_zone;
   size_t top_line;
   size_t cursor_line;
   int cursor_cell;
   int desired_cell;
   int command_cursor_cell;
   char command_line[LLM_DRIVER_MAX_COMMAND + 1];
   char status[AGENT_DRIVER_STATUS_MAX + 1];
   char search_text[AGENT_DRIVER_SEARCH_MAX + 1];
   int selection_active;
   size_t selection_start_line;
   int selection_start_cell;
   size_t selection_end_line;
   int selection_end_cell;
   char clipboard[LLM_DRIVER_MAX_COMMAND + 1];
   AgentDriverSnapshot undo[AGENT_DRIVER_HISTORY_MAX];
   size_t undo_count;
   AgentDriverSnapshot redo[AGENT_DRIVER_HISTORY_MAX];
   size_t redo_count;
   AgentDriverBuffer buffers[AGENT_DRIVER_BUFFER_MAX];
   size_t buffer_count;
   size_t current_buffer;
   char project_root[AGENT_DRIVER_PATH_MAX];
   size_t project_file_count;
   char project_files[AGENT_DRIVER_PROJECT_MAX][AGENT_DRIVER_PROJECT_NAME_MAX + 1];
   int previous_view_valid;
   LlmDriverScreenView previous_view;
} AgentDriver;

void agent_driver_init(AgentDriver *driver, int rows, int cols);
void agent_driver_free(AgentDriver *driver);
int agent_driver_load_file(AgentDriver *driver, const char *path);
int agent_driver_save_file(AgentDriver *driver, const char *path);
int agent_driver_screen_view(const AgentDriver *driver,
                             LlmDriverScreenView *view);
size_t agent_driver_format(const AgentDriver *driver,
                           const LlmDriverFormatOptions *options,
                           char *out, size_t out_len);
size_t agent_driver_format_delta(AgentDriver *driver,
                                 const LlmDriverFormatOptions *options,
                                 char *out, size_t out_len);
int agent_driver_apply_input(AgentDriver *driver, const TheInputEvent *input);
int agent_driver_set_text(AgentDriver *driver, const char *text);
const char *agent_driver_status(const AgentDriver *driver);

#endif
