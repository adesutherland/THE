#ifndef THE_LLMDRIVER_H
#define THE_LLMDRIVER_H

#include <stddef.h>
#include <stdint.h>

#include "logcursor.h"
#include "uidriver.h"

#define LLM_DRIVER_MAX_ROWS 256
#define LLM_DRIVER_MAX_COLS 1000
#define LLM_DRIVER_MAX_PREFIX 20
#define LLM_DRIVER_MAX_COMMAND 256
#define LLM_DRIVER_INPUT_QUEUE_MAX 128

typedef struct
{
   LINETYPE line_number;
   int logical_row;
   UiRowRole role;
   int logical_start_col;
   int editable;
   int current;
   int cursor;
   char prefix[LLM_DRIVER_MAX_PREFIX + 1];
   char text[LLM_DRIVER_MAX_COLS + 1];
} LlmDriverScreenLine;

typedef struct
{
   int rows;
   int cols;
   LogicalCursor cursor;
   int cursor_screen_row;
   int cursor_screen_col;
   size_t line_count;
   LlmDriverScreenLine lines[LLM_DRIVER_MAX_ROWS];
   char command_line[LLM_DRIVER_MAX_COMMAND + 1];
   char status[LLM_DRIVER_MAX_COMMAND + 1];
} LlmDriverScreenView;

typedef enum
{
   LLM_DRIVER_INPUT_NONE = 0,
   LLM_DRIVER_INPUT_TEXT,
   LLM_DRIVER_INPUT_KEY,
   LLM_DRIVER_INPUT_COMMAND,
   LLM_DRIVER_INPUT_LOGICAL_HIT,
   LLM_DRIVER_INPUT_DEBUG
} LlmDriverInputKind;

typedef enum
{
   LLM_DRIVER_DEBUG_NONE = 0,
   LLM_DRIVER_DEBUG_DESCRIBE_FOCUS,
   LLM_DRIVER_DEBUG_DESCRIBE_ROW,
   LLM_DRIVER_DEBUG_LIST_VISIBLE_ROWS,
   LLM_DRIVER_DEBUG_DUMP_CURSOR_MAPPING,
   LLM_DRIVER_DEBUG_DUMP_DRIVER_OPS,
   LLM_DRIVER_DEBUG_EXPLAIN_LAST_RENDER
} LlmDriverDebugCommand;

typedef struct
{
   LogicalCursorZone zone;
   LINETYPE line_number;
   int row;
   int cell;
} LlmDriverLogicalTarget;

typedef struct
{
   LlmDriverInputKind kind;
   uint32_t codepoint;
   int key_code;
   char command[LLM_DRIVER_MAX_COMMAND + 1];
   LlmDriverLogicalTarget target;
   LlmDriverDebugCommand debug_command;
} LlmDriverInput;

typedef struct
{
   int viewport_col;
   int logical_cell;
   int raw_display_col;
   int display_col;
   int visible;
} LlmDriverCursorMapping;

typedef struct
{
   LogicalCursor focus;
   LlmDriverCursorMapping cursor_mapping;
   UiDriverOpLog driver_ops;
   char last_render[LLM_DRIVER_MAX_COLS + 1];
} LlmDriverDebugSnapshot;

typedef struct
{
   LlmDriverInput items[LLM_DRIVER_INPUT_QUEUE_MAX];
   size_t head;
   size_t tail;
   size_t count;
} LlmDriverInputQueue;

void llm_driver_screen_view_init(LlmDriverScreenView *view, int rows, int cols,
                                 LogicalCursor cursor);
int llm_driver_screen_view_set_line(LlmDriverScreenView *view, size_t index,
                                    LINETYPE line_number, int logical_row,
                                    const char *prefix, const char *text,
                                    int current);
int llm_driver_screen_view_set_row(LlmDriverScreenView *view, size_t index,
                                   UiRowRole role, LINETYPE line_number,
                                   int logical_row, int logical_start_col,
                                   const char *prefix, const char *text,
                                   int editable, int current);
int llm_driver_screen_view_from_frame(const UiFrame *frame,
                                      LlmDriverScreenView *view);
void llm_driver_screen_view_set_command(LlmDriverScreenView *view,
                                        const char *command_line);
void llm_driver_screen_view_set_status(LlmDriverScreenView *view,
                                       const char *status);
size_t llm_driver_format_screen_view(const LlmDriverScreenView *view,
                                     char *out, size_t out_len);
size_t llm_driver_format_semantic_view(const LlmDriverScreenView *view,
                                       char *out, size_t out_len);

const char *llm_driver_input_kind_name(LlmDriverInputKind kind);
const char *llm_driver_debug_command_name(LlmDriverDebugCommand command);
LlmDriverInput llm_driver_input_none(void);
int llm_driver_input_from_text(uint32_t codepoint, LlmDriverInput *out);
int llm_driver_input_from_key_name(const char *name, LlmDriverInput *out);
int llm_driver_input_from_command(const char *command, LlmDriverInput *out);
int llm_driver_input_from_logical_hit(LogicalCursorZone zone,
                                      LINETYPE line_number, int row,
                                      int cell, LlmDriverInput *out);
int llm_driver_input_from_debug_command(const char *name,
                                        LlmDriverInput *out);
int llm_driver_input_to_legacy_key(const LlmDriverInput *input, int *key_code);
void llm_driver_input_queue_init(LlmDriverInputQueue *queue);
int llm_driver_input_queue_push(LlmDriverInputQueue *queue, LlmDriverInput input);
int llm_driver_input_queue_pop_legacy_key(LlmDriverInputQueue *queue, int *key_code);

void llm_driver_debug_snapshot_init(LlmDriverDebugSnapshot *debug,
                                    LogicalCursor focus);
void llm_driver_debug_snapshot_set_cursor_mapping(
   LlmDriverDebugSnapshot *debug, int viewport_col, int logical_cell,
   int raw_display_col, int display_col, int visible);
void llm_driver_debug_snapshot_set_last_render(LlmDriverDebugSnapshot *debug,
                                               const char *last_render);
size_t llm_driver_format_debug_snapshot(const LlmDriverDebugSnapshot *debug,
                                        char *out, size_t out_len);

#endif
