#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "gtkinput.h"

static void matches(guint key, GdkModifierType state, const char *name)
{
   TheInputEvent actual, expected;
   assert(the_gtk_input_key(key, state, &actual));
   assert(the_input_event_from_key_name(name, &expected));
   assert(actual.kind == expected.kind);
   assert(actual.key_code == expected.key_code);
}

int main(void)
{
   const char *text = "a\303\251\360\237\230\200\r\nb\rc\nd\t";
   TheInputEvent event, enter, tab;
   matches(GDK_KEY_Left, 0, "left");
   matches(GDK_KEY_Right, GDK_SHIFT_MASK, "shift-right");
   matches(GDK_KEY_Up, GDK_SHIFT_MASK, "shift-up");
   matches(GDK_KEY_Home, GDK_SHIFT_MASK, "shift-home");
   matches(GDK_KEY_Tab, GDK_SHIFT_MASK, "backtab");
   matches(GDK_KEY_ISO_Left_Tab, 0, "backtab");
   matches(GDK_KEY_Return, 0, "enter");
   matches(GDK_KEY_BackSpace, 0, "backspace");
   matches(GDK_KEY_Delete, 0, "delete");
   matches(GDK_KEY_F10, 0, "f10");
   matches(GDK_KEY_F35, 0, "f35");
   matches(GDK_KEY_s, GDK_CONTROL_MASK, "ctrl-s");
   matches(GDK_KEY_S, GDK_CONTROL_MASK | GDK_SHIFT_MASK, "ctrl-s");
   matches(GDK_KEY_z, GDK_META_MASK, "ctrl-z");
   assert(!the_gtk_input_key(GDK_KEY_a, 0, &event)); /* GtkIMContext owns text. */
   assert(!the_gtk_input_key(GDK_KEY_a, GDK_MOD1_MASK, &event));
   assert(the_input_event_from_key_name("enter", &enter));
   assert(the_input_event_from_key_name("tab", &tab));
   assert(the_gtk_input_text(&text, &event) && event.codepoint == 'a');
   assert(the_gtk_input_text(&text, &event) && event.codepoint == 0xe9);
   assert(the_gtk_input_text(&text, &event) && event.codepoint == 0x1f600);
   assert(the_gtk_input_text(&text, &event) && event.key_code == enter.key_code);
   assert(the_gtk_input_text(&text, &event) && event.codepoint == 'b');
   assert(the_gtk_input_text(&text, &event) && event.key_code == enter.key_code);
   assert(the_gtk_input_text(&text, &event) && event.codepoint == 'c');
   assert(the_gtk_input_text(&text, &event) && event.key_code == enter.key_code);
   assert(the_gtk_input_text(&text, &event) && event.codepoint == 'd');
   assert(the_gtk_input_text(&text, &event) && event.key_code == tab.key_code);
   assert(!*text && !the_gtk_input_text(&text, &event));
   text = "\xff";
   assert(!the_gtk_input_text(&text, &event));
   puts("GTK physical keys and UTF-8/CRLF input passed");
   return 0;
}
