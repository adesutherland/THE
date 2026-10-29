#ifndef THE_GTKINPUT_H
#define THE_GTKINPUT_H

#include <gdk/gdk.h>
#include "inputevent.h"

/* Text goes through GtkIMContext; this maps physical editor keys only. */
int the_gtk_input_key(guint keyval, GdkModifierType modifiers,
                       TheInputEvent *event);
int the_gtk_input_text(const char **cursor, TheInputEvent *event);

#endif
