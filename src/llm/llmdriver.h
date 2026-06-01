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
#define LLM_DRIVER_MAX_PATH 1024
#define LLM_DRIVER_MAX_BUFFERS 16
#define LLM_DRIVER_MAX_PROJECT_FILES 32
#define LLM_DRIVER_MAX_PROJECT_NAME 128
#define LLM_DRIVER_MAX_DIAGNOSTICS 64
#define LLM_DRIVER_MAX_DIAGNOSTIC_CODE 64
#define LLM_DRIVER_MAX_DIAGNOSTIC_MESSAGE 512
#define LLM_DRIVER_MAX_DIAGNOSTIC_SEVERITY 24
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
   char prefix_command[LLM_DRIVER_MAX_PREFIX + 1];
   char text[LLM_DRIVER_MAX_COLS + 1];
   UiStyleRun styles[UI_DRIVER_MAX_STYLE_RUNS];
   size_t style_count;
} LlmDriverScreenLine;

typedef struct
{
   int active;
   LINETYPE start_line;
   int start_cell;
   LINETYPE end_line;
   int end_cell;
   char clipboard[LLM_DRIVER_MAX_COMMAND + 1];
} LlmDriverSelectionView;

typedef struct
{
   char path[LLM_DRIVER_MAX_PATH + 1];
   int dirty;
   size_t line_count;
   int current;
} LlmDriverBufferInfo;

typedef struct
{
   char root[LLM_DRIVER_MAX_PATH + 1];
   size_t file_count;
   char files[LLM_DRIVER_MAX_PROJECT_FILES][LLM_DRIVER_MAX_PROJECT_NAME + 1];
} LlmDriverProjectView;

typedef struct
{
   LINETYPE line;
   LENGTHTYPE column;
   char severity[LLM_DRIVER_MAX_DIAGNOSTIC_SEVERITY];
   char code[LLM_DRIVER_MAX_DIAGNOSTIC_CODE];
   char message[LLM_DRIVER_MAX_DIAGNOSTIC_MESSAGE];
} LlmDriverParserDiagnostic;

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
   char buffer_path[LLM_DRIVER_MAX_PATH + 1];
   size_t buffer_line_count;
   int buffer_dirty;
   int buffer_valid;
   int undo_available;
   int redo_available;
   LlmDriverSelectionView selection;
   size_t buffer_count;
   LlmDriverBufferInfo buffers[LLM_DRIVER_MAX_BUFFERS];
   LlmDriverProjectView project;
   size_t diagnostic_count;
   LlmDriverParserDiagnostic diagnostics[LLM_DRIVER_MAX_DIAGNOSTICS];
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

#define LLM_DRIVER_TARGET_NONE THE_INPUT_TARGET_NONE
#define LLM_DRIVER_TARGET_FILEAREA THE_INPUT_TARGET_FILEAREA
#define LLM_DRIVER_TARGET_PREFIX THE_INPUT_TARGET_PREFIX
#define LLM_DRIVER_TARGET_COMMAND THE_INPUT_TARGET_COMMAND
#define LLM_DRIVER_TARGET_PROMPT THE_INPUT_TARGET_PROMPT
#define LLM_DRIVER_TARGET_STATUS THE_INPUT_TARGET_STATUS
#define LLM_DRIVER_TARGET_TABLINE THE_INPUT_TARGET_TABLINE
#define LLM_DRIVER_TARGET_DIVIDER THE_INPUT_TARGET_DIVIDER
#define LLM_DRIVER_TARGET_WINDOW THE_INPUT_TARGET_WINDOW

typedef TheInputKind LlmDriverInputKind;
typedef TheInputDebugCommand LlmDriverDebugCommand;
typedef TheInputLogicalTargetKind LlmDriverLogicalTargetKind;
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
int llm_driver_screen_view_set_prefix_command(LlmDriverScreenView *view,
                                              size_t index,
                                              const char *command);
int llm_driver_screen_view_from_frame(const UiFrame *frame,
                                      LlmDriverScreenView *view);
void llm_driver_screen_view_set_command(LlmDriverScreenView *view,
                                        const char *command_line);
void llm_driver_screen_view_set_status(LlmDriverScreenView *view,
                                       const char *status);
void llm_driver_screen_view_set_buffer(LlmDriverScreenView *view,
                                       const char *path, int dirty,
                                       size_t line_count);
void llm_driver_screen_view_set_history(LlmDriverScreenView *view,
                                        int undo_available,
                                        int redo_available);
void llm_driver_screen_view_set_selection(LlmDriverScreenView *view,
                                          int active, LINETYPE start_line,
                                          int start_cell, LINETYPE end_line,
                                          int end_cell,
                                          const char *clipboard);
int llm_driver_screen_view_add_buffer_info(LlmDriverScreenView *view,
                                           const char *path, int dirty,
                                           size_t line_count, int current);
void llm_driver_screen_view_set_project_root(LlmDriverScreenView *view,
                                             const char *root);
int llm_driver_screen_view_add_project_file(LlmDriverScreenView *view,
                                            const char *path);
int llm_driver_screen_view_add_diagnostic(LlmDriverScreenView *view,
                                          LINETYPE line, LENGTHTYPE column,
                                          const char *severity,
                                          const char *code,
                                          const char *message);
size_t llm_driver_format_screen_view(const LlmDriverScreenView *view,
                                     char *out, size_t out_len);
void llm_driver_format_options_init(LlmDriverFormatOptions *options);
size_t llm_driver_format_semantic_view(const LlmDriverScreenView *view,
                                       char *out, size_t out_len);
size_t llm_driver_format_semantic_view_with_options(
   const LlmDriverScreenView *view, const LlmDriverFormatOptions *options,
   char *out, size_t out_len);
size_t llm_driver_format_delta_view(const LlmDriverScreenView *previous,
                                    const LlmDriverScreenView *current,
                                    const LlmDriverFormatOptions *options,
                                    char *out, size_t out_len);

const char *llm_driver_input_kind_name(LlmDriverInputKind kind);
const char *llm_driver_debug_command_name(LlmDriverDebugCommand command);
const char *llm_driver_logical_target_kind_name(LlmDriverLogicalTargetKind kind);
LlmDriverInput llm_driver_input_none(void);
int llm_driver_input_from_text(uint32_t codepoint, LlmDriverInput *out);
int llm_driver_input_from_key_name(const char *name, LlmDriverInput *out);
int llm_driver_input_from_legacy_key(int key_code, LlmDriverInput *out);
int llm_driver_input_from_command(const char *command, LlmDriverInput *out);
int llm_driver_input_from_logical_target(LlmDriverLogicalTargetKind kind,
                                         LINETYPE line_number, int row,
                                         int cell, int screen, int window_id,
                                         LlmDriverInput *out);
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
