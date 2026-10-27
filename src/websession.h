#ifndef THE_WEBSESSION_H
#define THE_WEBSESSION_H

#include "inputevent.h"

/* Transport-neutral browser behavior. Only the editor's execution owner calls
 * these functions; transports decode messages without maintaining editor state. */
size_t the_web_format_snapshot(char *out, size_t capacity);
size_t the_web_format_actions(char *out, size_t capacity);
int the_web_queue_request(TheInputQueue *queue, const char *type,
                          const char *value, const char *argument,
                          long line, int row, int cell, int rows, int cols);
#endif
