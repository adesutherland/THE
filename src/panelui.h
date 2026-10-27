#ifndef THE_PANELUI_H
#define THE_PANELUI_H

#include "inputevent.h"

#define THE_PANEL_MAX_FIELDS 512
#define THE_PANEL_TEXT 4096

typedef struct
{
   int row, cell, width, editable, changed;
   char kind; /* R reserved, F file, P prefix, C command */
   LINETYPE line;
   size_t source_start, source_length;
   char text[THE_PANEL_TEXT + 1];
   char original[THE_PANEL_TEXT + 1];
} ThePanelField;

typedef struct
{
   int rows, cols, count, active, cursor, done, cancelled;
   int key; /* ENTER, PF1..24 or PA1..3 */
   ThePanelField fields[THE_PANEL_MAX_FIELDS];
} ThePanelState;

extern ThePanelState *the_panel_active;
void panel_ui_init(ThePanelState *panel, int rows, int cols);
int panel_ui_add(ThePanelState *panel, int row, int cell, int width,
                  int editable, char kind, LINETYPE line, const char *text,
                  size_t length, size_t source_start);
int panel_ui_event(ThePanelState *panel, const TheInputEvent *event);
int panel_ui_hit(ThePanelState *panel, int row, int cell);
int panel_ui_cursor(const ThePanelState *panel, int *cell, int *width);
size_t panel_ui_json(const ThePanelState *panel, char *out, size_t size);

#endif
