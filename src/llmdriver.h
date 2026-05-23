#ifndef THE_LLMDRIVER_H
#define THE_LLMDRIVER_H

#include <stddef.h>
#include <stdint.h>

#include "inputevent.h"
#include "logcursor.h"
#include "uidriver.h"

#define LLM_DRIVER_MAX_ROWS 256
#define LLM_DRIVER_MAX_COLS 1000
#define LLM_DRIVER_MAX_PREFIX 20
#define LLM_DRIVER_MAX_COMMAND 256
#define LLM_DRIVER_INPUT_QUEUE_MAX THE_INPUT_QUEUE_MAX

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
   UiStyleRun styles[UI_DRIVER_MAX_STYLE_RUNS];
   size_t style_count;
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

#define LLM_DRIVER_INPUT_NONE THE_INPUT_NONE
#define LLM_DRIVER_INPUT_TEXT THE_INPUT_TEXT
#define LLM_DRIVER_INPUT_KEY THE_INPUT_KEY
#define LLM_DRIVER_INPUT_COMMAND THE_INPUT_COMMAND
#define LLM_DRIVER_INPUT_LOGICAL_HIT THE_INPUT_LOGICAL_HIT
#define LLM_DRIVER_INPUT_DEBUG THE_INPUT_DEBUG

#define LLM_DRIVER_DEBUG_NONE THE_INPUT_DEBUG_NONE
#define LLM_DRIVER_DEBUG_DESCRIBE_FOCUS THE_INPUT_DEBUG_DESCRIBE_FOCUS
#define LLM_DRIVER_DEBUG_DESCRIBE_ROW THE_INPUT_DEBUG_DESCRIBE_ROW
#define LLM_DRIVER_DEBUG_LIST_VISIBLE_ROWS THE_INPUT_DEBUG_LIST_VISIBLE_ROWS
#define LLM_DRIVER_DEBUG_DUMP_CURSOR_MAPPING THE_INPUT_DEBUG_DUMP_CURSOR_MAPPING
#define LLM_DRIVER_DEBUG_DUMP_DRIVER_OPS THE_INPUT_DEBUG_DUMP_DRIVER_OPS
#define LLM_DRIVER_DEBUG_EXPLAIN_LAST_RENDER THE_INPUT_DEBUG_EXPLAIN_LAST_RENDER

typedef TheInputKind LlmDriverInputKind;
typedef TheInputDebugCommand LlmDriverDebugCommand;
typedef TheInputLogicalTarget LlmDriverLogicalTarget;
typedef TheInputEvent LlmDriverInput;

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

typedef enum
{
   LLM_DRIVER_VIEW_FULL = 0,
   LLM_DRIVER_VIEW_FILEAREA,
   LLM_DRIVER_VIEW_RESERVED,
   LLM_DRIVER_VIEW_PREFIX,
   LLM_DRIVER_VIEW_FOCUS
} LlmDriverViewMode;

typedef struct
{
   LlmDriverViewMode mode;
   int first_row;
   int row_count;
   int max_text_cols;
   int include_prefix;
   int include_command;
   int include_status;
   int include_cursor;
   int compact;
} LlmDriverFormatOptions;

typedef TheInputQueue LlmDriverInputQueue;

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
void llm_driver_format_options_init(LlmDriverFormatOptions *options);
size_t llm_driver_format_semantic_view(const LlmDriverScreenView *view,
                                       char *out, size_t out_len);
size_t llm_driver_format_semantic_view_with_options(
   const LlmDriverScreenView *view, const LlmDriverFormatOptions *options,
   char *out, size_t out_len);

const char *llm_driver_input_kind_name(LlmDriverInputKind kind);
const char *llm_driver_debug_command_name(LlmDriverDebugCommand command);
LlmDriverInput llm_driver_input_none(void);
int llm_driver_input_from_text(uint32_t codepoint, LlmDriverInput *out);
int llm_driver_input_from_key_name(const char *name, LlmDriverInput *out);
int llm_driver_input_from_legacy_key(int key_code, LlmDriverInput *out);
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
