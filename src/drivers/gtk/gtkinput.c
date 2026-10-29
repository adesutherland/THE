#include "gtkinput.h"

#include <stdio.h>

int the_gtk_input_key(guint keyval, GdkModifierType modifiers,
                       TheInputEvent *event)
{
   const char *name = NULL;
   int shift = (modifiers & GDK_SHIFT_MASK) != 0;
   char function[12];

   if (event == NULL)
      return 0;
   *event = the_input_event_none();
   if (modifiers & (GDK_CONTROL_MASK | GDK_META_MASK))
   {
      guint lower = gdk_keyval_to_lower(keyval);
      if (lower >= GDK_KEY_a && lower <= GDK_KEY_z)
         return the_input_event_from_legacy_key(lower - GDK_KEY_a + 1, event);
      if (keyval >= GDK_KEY_bracketleft && keyval <= GDK_KEY_underscore)
         return the_input_event_from_legacy_key(keyval - GDK_KEY_at, event);
      if (keyval == GDK_KEY_space || keyval == GDK_KEY_at)
         return the_input_event_from_legacy_key(0, event);
      return 0;
   }
   if (modifiers & GDK_MOD1_MASK)
      return 0;
   if (keyval >= GDK_KEY_F1 && keyval <= GDK_KEY_F35)
   {
      snprintf(function, sizeof(function), "f%u", keyval - GDK_KEY_F1 + 1);
      return the_input_event_from_key_name(function, event);
   }
   switch (keyval)
   {
      case GDK_KEY_Left: name = shift ? "shift-left" : "left"; break;
      case GDK_KEY_Right: name = shift ? "shift-right" : "right"; break;
      case GDK_KEY_Up: name = shift ? "shift-up" : "up"; break;
      case GDK_KEY_Down: name = shift ? "shift-down" : "down"; break;
      case GDK_KEY_Home: name = shift ? "shift-home" : "home"; break;
      case GDK_KEY_End: name = shift ? "shift-end" : "end"; break;
      case GDK_KEY_Page_Up: name = "pageup"; break;
      case GDK_KEY_Page_Down: name = "pagedown"; break;
      case GDK_KEY_Return: case GDK_KEY_KP_Enter: name = "enter"; break;
      case GDK_KEY_Escape: name = "esc"; break;
      case GDK_KEY_Tab: name = shift ? "backtab" : "tab"; break;
      case GDK_KEY_ISO_Left_Tab: name = "backtab"; break;
      case GDK_KEY_BackSpace: name = "backspace"; break;
      case GDK_KEY_Delete: case GDK_KEY_KP_Delete: name = "delete"; break;
      case GDK_KEY_Insert: name = "insert"; break;
      default: return 0;
   }
   return the_input_event_from_key_name(name, event);
}

int the_gtk_input_text(const char **cursor, TheInputEvent *event)
{
   gunichar codepoint;
   if (!cursor || !*cursor || !**cursor || !event)
      return 0;
   codepoint = g_utf8_get_char_validated(*cursor, -1);
   if (codepoint == (gunichar)-1 || codepoint == (gunichar)-2)
      return 0;
   *cursor = g_utf8_next_char(*cursor);
   if (codepoint == '\r')
   {
      if (**cursor == '\n')
         (*cursor)++;
      return the_input_event_from_key_name("enter", event);
   }
   if (codepoint == '\n')
      return the_input_event_from_key_name("enter", event);
   if (codepoint == '\t')
      return the_input_event_from_key_name("tab", event);
   return the_input_event_from_text(codepoint, event);
}
