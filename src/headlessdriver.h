#ifndef THE_HEADLESSDRIVER_H
#define THE_HEADLESSDRIVER_H

#include <stddef.h>

#include "thedriver.h"

#define HEADLESS_DRIVER_OP_LOG_CAPACITY 256

extern const TheDriverOps the_headless_driver_ops;

void headless_driver_reset(void);
void headless_driver_clear_log(void);
size_t headless_driver_log_count(void);
const char *headless_driver_log_entry(size_t index);

void headless_driver_set_current_screen(CHARTYPE scrno);
void headless_driver_set_screen_current_role(CHARTYPE scrno, short role);
void headless_driver_set_screen_previous_role(CHARTYPE scrno, short role);
TheDriverWindow *headless_driver_create_screen_role(CHARTYPE scrno,
                                                    short role, int rows,
                                                    int cols, int row,
                                                    int col);
TheDriverWindow *headless_driver_create_global_window(
   TheDriverGlobalWindowRole role, int rows, int cols, int row, int col);
int headless_driver_render_cell_at(TheDriverWindow *win, int row, int col,
                                   TheRenderCell *out);

void headless_driver_queue_key(int key);
int headless_driver_queue_input_event(TheInputEvent event);
void headless_driver_set_mouse_position(int row, int col);
void headless_driver_set_mouse_button(int button, int action, int modifier);
void headless_driver_set_mouse_event(int button, TheDriverMouseAction action,
                                     int modifier, int row, int col);

#endif
