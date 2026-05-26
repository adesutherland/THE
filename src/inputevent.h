#ifndef THE_INPUTEVENT_H
#define THE_INPUTEVENT_H

#include <stddef.h>
#include <stdint.h>

#include "logcursor.h"

#define THE_INPUT_COMMAND_MAX 256
#define THE_INPUT_QUEUE_MAX 128

typedef enum
{
   THE_INPUT_NONE = 0,
   THE_INPUT_TEXT,
   THE_INPUT_KEY,
   THE_INPUT_COMMAND,
   THE_INPUT_LOGICAL_HIT,
   THE_INPUT_DEBUG
} TheInputKind;

typedef enum
{
   THE_INPUT_DEBUG_NONE = 0,
   THE_INPUT_DEBUG_DESCRIBE_FOCUS,
   THE_INPUT_DEBUG_DESCRIBE_ROW,
   THE_INPUT_DEBUG_LIST_VISIBLE_ROWS,
   THE_INPUT_DEBUG_DUMP_CURSOR_MAPPING,
   THE_INPUT_DEBUG_DUMP_DRIVER_OPS,
   THE_INPUT_DEBUG_EXPLAIN_LAST_RENDER
} TheInputDebugCommand;

typedef enum
{
   THE_INPUT_TARGET_NONE = 0,
   THE_INPUT_TARGET_FILEAREA,
   THE_INPUT_TARGET_PREFIX,
   THE_INPUT_TARGET_COMMAND,
   THE_INPUT_TARGET_PROMPT,
   THE_INPUT_TARGET_STATUS,
   THE_INPUT_TARGET_TABLINE,
   THE_INPUT_TARGET_DIVIDER,
   THE_INPUT_TARGET_WINDOW
} TheInputLogicalTargetKind;

typedef struct TheInputLogicalTarget
{
   TheInputLogicalTargetKind kind;
   LogicalCursorZone zone;
   LINETYPE line_number;
   int row;
   int cell;
   int screen;
   int window_id;
} TheInputLogicalTarget;

typedef struct
{
   TheInputKind kind;
   uint32_t codepoint;
   int key_code;
   char command[THE_INPUT_COMMAND_MAX + 1];
   TheInputLogicalTarget target;
   TheInputDebugCommand debug_command;
} TheInputEvent;

typedef struct
{
   TheInputEvent items[THE_INPUT_QUEUE_MAX];
   size_t head;
   size_t tail;
   size_t count;
} TheInputQueue;

const char *the_input_kind_name(TheInputKind kind);
const char *the_input_debug_command_name(TheInputDebugCommand command);
const char *the_input_logical_target_kind_name(TheInputLogicalTargetKind kind);
int the_input_logical_target_kind_from_name(const char *name,
                                            TheInputLogicalTargetKind *kind);
TheInputEvent the_input_event_none(void);
int the_input_event_from_text(uint32_t codepoint, TheInputEvent *out);
int the_input_event_from_key_name(const char *name, TheInputEvent *out);
int the_input_event_from_legacy_key(int key_code, TheInputEvent *out);
int the_input_event_from_command(const char *command, TheInputEvent *out);
int the_input_event_from_logical_target(TheInputLogicalTargetKind kind,
                                        LINETYPE line_number, int row,
                                        int cell, int screen, int window_id,
                                        TheInputEvent *out);
int the_input_event_from_logical_hit(LogicalCursorZone zone,
                                     LINETYPE line_number, int row,
                                     int cell, TheInputEvent *out);
int the_input_event_from_debug_command(const char *name,
                                       TheInputEvent *out);
int the_input_event_to_legacy_key(const TheInputEvent *input, int *key_code);
void the_input_queue_init(TheInputQueue *queue);
int the_input_queue_push(TheInputQueue *queue, TheInputEvent input);
int the_input_queue_pop(TheInputQueue *queue, TheInputEvent *input);
int the_input_queue_pop_legacy_key(TheInputQueue *queue, int *key_code);

#endif
