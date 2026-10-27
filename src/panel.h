#ifndef THE_PANEL_H
#define THE_PANEL_H
#include "thedefs.h"
short THERead(CHARTYPE *params);
int the_read_queue_depth(void);
short the_read_queue_push(const char *text);
#endif
