#ifndef THE_LLMDRIVER_H
#define THE_LLMDRIVER_H

#include <stddef.h>
#include <stdint.h>

#include "logcursor.h"

#define LLM_DRIVER_MAX_ROWS 256
#define LLM_DRIVER_MAX_COLS 1000
#define LLM_DRIVER_MAX_PREFIX 20
#define LLM_DRIVER_MAX_COMMAND 256
#define LLM_DRIVER_INPUT_QUEUE_MAX 128

typedef struct
{
   LINETYPE line_number;
   int logical_row;
   int current;
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
   LLM_DRIVER_INPUT_COMMAND
} LlmDriverInputKind;

typedef struct
{
   LlmDriverInputKind kind;
   uint32_t codepoint;
   int key_code;
   char command[LLM_DRIVER_MAX_COMMAND + 1];
} LlmDriverInput;

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
void llm_driver_screen_view_set_command(LlmDriverScreenView *view,
                                        const char *command_line);
void llm_driver_screen_view_set_status(LlmDriverScreenView *view,
                                       const char *status);
size_t llm_driver_format_screen_view(const LlmDriverScreenView *view,
                                     char *out, size_t out_len);

const char *llm_driver_input_kind_name(LlmDriverInputKind kind);
LlmDriverInput llm_driver_input_none(void);
int llm_driver_input_from_text(uint32_t codepoint, LlmDriverInput *out);
int llm_driver_input_from_key_name(const char *name, LlmDriverInput *out);
int llm_driver_input_from_command(const char *command, LlmDriverInput *out);
int llm_driver_input_to_legacy_key(const LlmDriverInput *input, int *key_code);
void llm_driver_input_queue_init(LlmDriverInputQueue *queue);
int llm_driver_input_queue_push(LlmDriverInputQueue *queue, LlmDriverInput input);
int llm_driver_input_queue_pop_legacy_key(LlmDriverInputQueue *queue, int *key_code);

#endif
