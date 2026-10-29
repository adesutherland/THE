/* GTK is a projection of the real editor's driver surfaces. All editor
 * mutations are queued for the editor owner, including menu and tool actions. */
#include <gtk/gtk.h>
#include <string.h>

#include "the.h"
#include "proto.h"
#include "vars.h"
#include "driverlayout.h"
#include "driverwindow.h"
#include "headlessdriver.h"
#include "editorsource.h"
#include "panelui.h"
#include "tools.h"
#include "gtkinput.h"

#define GTK_TEXT_LIMIT (1024u * 1024u)
#define GTK_QUEUE_LIMIT 4096u
#define GTK_EDITOR_MIN_COLUMNS 80
#define GTK_CONSOLE_COLOURS (ATTR_CONSOLESTATUS - ATTR_CONSOLEBODY + 1)

typedef struct
{
   TheDriverWindow *window;
   int visible;
} GtkSurface;

typedef struct
{
   TheInputEvent event;
   char *text;
   const char *cursor;
} GtkInput;

static struct
{
   GtkWidget *window, *split, *editor, *console, *output, *input, *state;
   GtkWidget *run, *stop, *eof, *send, *header;
   GtkIMContext *im;
   PangoFontDescription *font;
   GList *surfaces;
   GQueue inputs;
   size_t queued_bytes;
   int active, timeout, cell_width, cell_height, rows, cols;
   int cursor_visible, blink_visible;
   int split_positioned;
   unsigned console_version;
   unsigned console_job_id;
   GtkCssProvider *console_css;
   TheDriverAttr console_colours[GTK_CONSOLE_COLOURS];
   int console_colours_valid;
   guint tick, split_layout_idle;
   TheDriverWindow *input_window;
} ui;

static TheDriverOps operations;

static void redraw(void)
{
   if (ui.editor)
      gtk_widget_queue_draw(ui.editor);
}

static void message(const char *text)
{
   GtkWidget *dialog = gtk_message_dialog_new(GTK_WINDOW(ui.window),
      GTK_DIALOG_MODAL, GTK_MESSAGE_INFO, GTK_BUTTONS_CLOSE, "%s", text);
   gtk_dialog_run(GTK_DIALOG(dialog));
   gtk_widget_destroy(dialog);
}

static int queue_event(TheInputEvent event)
{
   GtkInput *input;
   if (g_queue_get_length(&ui.inputs) >= GTK_QUEUE_LIMIT)
   {
      gtk_widget_error_bell(ui.editor);
      return 0;
   }
   input = g_new0(GtkInput, 1);
   input->event = event;
   g_queue_push_tail(&ui.inputs, input);
   return 1;
}

static void queue_text(const char *text)
{
   GtkInput *input;
   size_t length = text ? strlen(text) : 0;
   if (!length)
      return;
   if (length > GTK_TEXT_LIMIT || ui.queued_bytes > GTK_TEXT_LIMIT - length
   || g_queue_get_length(&ui.inputs) >= GTK_QUEUE_LIMIT
   || !g_utf8_validate(text, length, NULL))
   {
      message("Text input must be valid UTF-8 and fit in the 1 MiB input queue.");
      return;
   }
   input = g_new0(GtkInput, 1);
   input->text = g_strdup(text);
   input->cursor = input->text;
   ui.queued_bytes += length;
   g_queue_push_tail(&ui.inputs, input);
}

static int pop_input(TheInputEvent *event)
{
   GtkInput *input = g_queue_peek_head(&ui.inputs);
   if (!input)
      return 0;
   if (input->text)
   {
      if (!the_gtk_input_text(&input->cursor, event))
         *event = the_input_event_none();
      if (*input->cursor)
         return event->kind != THE_INPUT_NONE;
      ui.queued_bytes -= strlen(input->text);
      g_free(input->text);
   }
   else
      *event = input->event;
   g_queue_pop_head(&ui.inputs);
   g_free(input);
   return event->kind != THE_INPUT_NONE;
}

static void queue_command(const char *command)
{
   TheInputEvent event;
   if (the_panel_active || in_macro)
   {
      gtk_widget_error_bell(ui.editor);
      return;
   }
   if (the_input_event_from_command(command, &event))
      queue_event(event);
   gtk_widget_grab_focus(ui.editor);
}

static void command_action(GtkWidget *widget, gpointer command)
{
   (void)widget;
   queue_command(command);
}

static void toggle_console(GtkWidget *widget, gpointer data)
{
   (void)widget; (void)data;
   queue_command(the_tool_console()->visible ? "rx console off" : "rx console");
}

static void tool_action(GtkWidget *widget, gpointer action)
{
   TheInputEvent event;
   (void)widget;
   if (the_panel_active || in_macro)
      return;
   if (the_tools_prepare_input(action, "", "", 0, NULL, &event))
      queue_event(event);
   gtk_widget_grab_focus(ui.editor);
}

static void file_action(GtkWidget *widget, gpointer name)
{
   const char *action = name;
   TheInputEvent event;
   (void)widget;
   if (the_panel_active || in_macro)
      return;
   if (!strcmp(action, "file.open") || !strcmp(action, "file.create"))
   {
      int creating = !strcmp(action, "file.create");
      GtkWidget *dialog = gtk_file_chooser_dialog_new(
         creating ? "New file" : "Open file", GTK_WINDOW(ui.window),
         creating ? GTK_FILE_CHOOSER_ACTION_SAVE : GTK_FILE_CHOOSER_ACTION_OPEN,
         "_Cancel", GTK_RESPONSE_CANCEL, creating ? "_Create" : "_Open",
         GTK_RESPONSE_ACCEPT, NULL);
      if (CURRENT_FILE)
         gtk_file_chooser_set_current_folder(GTK_FILE_CHOOSER(dialog),
                                              (char *)CURRENT_FILE->fpath);
      if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT)
      {
         char *path = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dialog));
         if (the_input_event_from_action(action, path, &event))
            queue_event(event);
         else
            message("This path exceeds the editor limit or contains a quote or control character.");
         g_free(path);
      }
      gtk_widget_destroy(dialog);
   }
   else if (the_input_event_from_action(action, "", &event))
      queue_event(event);
   gtk_widget_grab_focus(ui.editor);
}

static void paste_action(GtkWidget *widget, gpointer data)
{
   char *text;
   (void)widget; (void)data;
   text = gtk_clipboard_wait_for_text(gtk_clipboard_get(GDK_SELECTION_CLIPBOARD));
   queue_text(text);
   g_free(text);
   gtk_widget_grab_focus(ui.editor);
}

static void send_input(GtkWidget *widget, gpointer data)
{
   const char *text = gtk_entry_get_text(GTK_ENTRY(ui.input));
   char *line = g_strconcat(text, "\n", NULL);
   TheInputEvent event;
   (void)widget; (void)data;
   if (the_panel_active || in_macro)
   {
      g_free(line);
      gtk_widget_error_bell(ui.input);
      return;
   }
   if (the_tools_prepare_input("input", "", line, 0, NULL, &event)
   && queue_event(event))
      gtk_entry_set_text(GTK_ENTRY(ui.input), "");
   g_free(line);
}

static gboolean close_window(GtkWidget *widget, GdkEvent *event, gpointer data)
{
   (void)widget; (void)event; (void)data;
   /* CANCEL closes unchanged buffers and leaves changed buffers open. Its
    * save checks include the pending record; it never discards changed text. */
   queue_command("cancel");
   return TRUE;
}

static void im_commit(GtkIMContext *context, const char *text, gpointer data)
{
   (void)context; (void)data;
   queue_text(text);
}

static gboolean key_pressed(GtkWidget *widget, GdkEventKey *key, gpointer data)
{
   TheInputEvent event;
   gunichar character;
   (void)widget; (void)data;
   if (the_gtk_input_key(key->keyval, key->state, &event))
   {
      gtk_im_context_reset(ui.im);
      queue_event(event);
      return TRUE;
   }
   if (gtk_im_context_filter_keypress(ui.im, key))
      return TRUE;
   if (key->state & (GDK_CONTROL_MASK | GDK_META_MASK | GDK_MOD1_MASK))
      return FALSE;
   character = gdk_keyval_to_unicode(key->keyval);
   if (g_unichar_isprint(character)
   && the_input_event_from_text(character, &event))
   {
      queue_event(event);
      return TRUE;
   }
   return FALSE;
}

static gboolean key_released(GtkWidget *widget, GdkEventKey *key, gpointer data)
{
   (void)widget; (void)data;
   return gtk_im_context_filter_keypress(ui.im, key);
}

static void editor_realized(GtkWidget *widget, gpointer data)
{
   (void)data;
   gtk_im_context_set_client_window(ui.im, gtk_widget_get_window(widget));
}

static gboolean editor_focus(GtkWidget *widget, GdkEventFocus *event, gpointer data)
{
   (void)widget; (void)data;
   if (event->in)
      gtk_im_context_focus_in(ui.im);
   else
      gtk_im_context_focus_out(ui.im);
   redraw();
   return FALSE;
}

static void allocated(GtkWidget *widget, GtkAllocation *allocation, gpointer data)
{
   (void)widget; (void)data;
   ui.rows = CLAMP(allocation->height / ui.cell_height, 12, 256);
   ui.cols = CLAMP(allocation->width / ui.cell_width, 40, THE_MAX_SCREEN_WIDTH);
}

static gboolean position_split(gpointer data)
{
   GtkAllocation allocation;
   (void)data;
   ui.split_layout_idle = 0;
   gtk_widget_get_allocation(ui.split, &allocation);
   gtk_paned_set_position(GTK_PANED(ui.split),
      gtk_orientable_get_orientation(GTK_ORIENTABLE(ui.split)) == GTK_ORIENTATION_HORIZONTAL
         ? allocation.width * 2 / 3 : MAX(200, allocation.height - 240));
   return G_SOURCE_REMOVE;
}

static void split_allocated(GtkWidget *widget, GtkAllocation *allocation, gpointer data)
{
   int console_width = 0;
   GtkOrientation orientation;
   /* Fit a useful editor grid beside the console using the current font and
    * theme's real widget minimums. A pixel-only breakpoint needlessly stacks
    * the console below the editor at the default desktop window width. */
   if (ui.console)
      gtk_widget_get_preferred_width(ui.console, &console_width, NULL);
   orientation = allocation->width >= GTK_EDITOR_MIN_COLUMNS * ui.cell_width
                                      + console_width + ui.cell_width
                                 ? GTK_ORIENTATION_HORIZONTAL : GTK_ORIENTATION_VERTICAL;
   (void)data;
   /* Reset the divider only when changing layouts. Resizing within a layout
    * retains the user's divider adjustment and GtkPaned's resize behavior. */
   if (!ui.split_positioned
   || gtk_orientable_get_orientation(GTK_ORIENTABLE(widget)) != orientation)
   {
      gtk_orientable_set_orientation(GTK_ORIENTABLE(widget), orientation);
      /* GtkPaned must finish allocating the new axis before positioning its
       * divider; otherwise its old-axis resize adjustment moves it again. */
      if (!ui.split_layout_idle)
         ui.split_layout_idle = g_idle_add(position_split, NULL);
      ui.split_positioned = 1;
   }
}

static gboolean clicked(GtkWidget *widget, GdkEventButton *button, gpointer data)
{
   int screen_no, role;
   int row = (int)button->y / ui.cell_height;
   int col = (int)button->x / ui.cell_width;
   (void)data;
   if (button->button != 1 || the_panel_active || in_macro)
      return TRUE;
   for (screen_no = 0; screen_no < display_screens; screen_no++)
   {
      for (role = WINDOW_FILEAREA; role <= WINDOW_COMMAND; role++)
      {
         TheDriverWindow *win = driver_screen_role_window(screen_no, role);
         TheDriverWindowOrigin origin = operations.window_origin(win);
         TheDriverWindowSize size = operations.window_size(win);
         TheInputEvent event;
         int cell = col - origin.col;
         int local_row = row - origin.row;
         TheInputLogicalTargetKind kind;
         if (!origin.valid || !size.valid || local_row < 0 || local_row >= size.rows
         || cell < 0 || cell >= size.cols)
            continue;
         kind = role == WINDOW_FILEAREA ? THE_INPUT_TARGET_FILEAREA
              : role == WINDOW_PREFIX ? THE_INPUT_TARGET_PREFIX
              : THE_INPUT_TARGET_COMMAND;
         if (role != WINDOW_COMMAND && screen[screen_no].sl)
         {
            SHOW_LINE *line = &screen[screen_no].sl[local_row];
            int enterable = role == WINDOW_FILEAREA ? line->main_enterable
                                                     : line->prefix_enterable;
            /* Mirror the core's cursorable-row contract. Display-only rows
             * must not pull keyboard focus away from Program input or other
             * controls when the core would reject their cursor target. */
            if (!enterable && line->line_type != LINE_TOF && line->line_type != LINE_EOF)
               return TRUE;
            if (role == WINDOW_FILEAREA)
            {
               const CHARTYPE *text = line->contents ? line->contents : rec;
               size_t length = line->contents ? line->length : rec_len;
               cell = driver_layout_logical_col_from_display(text, length,
                  (int)screen[screen_no].screen_view->verify_col - 1, cell,
                  TEXT_SNAP_BACKWARD);
            }
         }
         if (the_input_event_from_logical_target(kind, -1, local_row,
                                                    cell, screen_no, 0, &event)
         && queue_event(event))
            gtk_widget_grab_focus(widget);
         return TRUE;
      }
   }
   return TRUE;
}

static gboolean scrolled(GtkWidget *widget, GdkEventScroll *event, gpointer data)
{
   (void)widget; (void)data;
   if (event->direction == GDK_SCROLL_UP || event->delta_y < 0)
      queue_command("backward 3");
   else if (event->direction == GDK_SCROLL_DOWN || event->delta_y > 0)
      queue_command("forward 3");
   return TRUE;
}

static unsigned logical_rgb(int logical, int foreground)
{
   static const unsigned palette[] = {
      0x1e1e1e, 0x569cd6, 0x6a9955, 0x4ec9b0,
      0xf48771, 0xc586c0, 0xdcdcaa, 0xd4d4d4
   };
   unsigned rgb = foreground ? 0xd4d4d4 : 0x1e1e1e;
   if (logical >= 0 && logical < 8)
      rgb = palette[logical];
   else if (the_render_color_is_rgb(logical))
      rgb = the_render_color_rgb24(logical);
   return rgb;
}

static void colour(cairo_t *cr, int logical, int foreground)
{
   unsigned rgb = logical_rgb(logical, foreground);
   cairo_set_source_rgb(cr, ((rgb >> 16) & 255) / 255.0,
                        ((rgb >> 8) & 255) / 255.0, (rgb & 255) / 255.0);
}

static void console_colours(TheDriverAttr attr, GdkRGBA *foreground, GdkRGBA *background)
{
   unsigned fg = logical_rgb(the_render_attr_fg(attr), 1);
   unsigned bg = logical_rgb(the_render_attr_bg(attr), 0);
   TheRenderStyle style = the_render_attr_style(attr);
   if (style & THE_STYLE_REVERSE)
   {
      unsigned swap = fg; fg = bg; bg = swap;
   }
   *foreground = (GdkRGBA){((fg >> 16) & 255) / 255.0,
                           ((fg >> 8) & 255) / 255.0, (fg & 255) / 255.0, 1};
   *background = (GdkRGBA){((bg >> 16) & 255) / 255.0,
                           ((bg >> 8) & 255) / 255.0, (bg & 255) / 255.0, 1};
   if (style & THE_STYLE_DIM)
   {
      foreground->red = foreground->red * .65 + background->red * .35;
      foreground->green = foreground->green * .65 + background->green * .35;
      foreground->blue = foreground->blue * .65 + background->blue * .35;
   }
}

static void console_css_rule(GString *css, const char *selector, TheDriverAttr attr,
                               int border)
{
   GdkRGBA fg, bg;
   char *foreground, *background;
   TheRenderStyle style = the_render_attr_style(attr);
   console_colours(attr, &fg, &bg);
   foreground = gdk_rgba_to_string(&fg);
   background = gdk_rgba_to_string(&bg);
   g_string_append_printf(css,
      "%s { color: %s; background-color: %s; background-image: none; transition: none;"
      "font-weight: %s; font-style: %s; text-decoration-line: %s%s%s;",
      selector, foreground, background, style & THE_STYLE_BOLD ? "bold" : "normal",
      style & THE_STYLE_ITALIC ? "italic" : "normal",
      style & THE_STYLE_UNDERLINE ? "underline " : "",
      style & THE_STYLE_STRIKEOUT ? "line-through " : "",
      style & (THE_STYLE_UNDERLINE | THE_STYLE_STRIKEOUT) ? "" : "none");
   if (border)
      g_string_append_printf(css, "border-color: %s;", foreground);
   g_string_append(css, "}\n");
   g_free(foreground);
   g_free(background);
}

static void sync_console_palette(void)
{
   TheDriverAttr colours[GTK_CONSOLE_COLOURS];
   GtkTextTag *error_tag;
   GdkRGBA fg, bg;
   TheRenderStyle style;
   GString *css;
   GError *error = NULL;
   int i;
   if (!ui.console_css || !CURRENT_VIEW || !CURRENT_FILE)
      return;
   for (i = 0; i < GTK_CONSOLE_COLOURS; i++)
      colours[i] = set_colour(CURRENT_FILE->attr + ATTR_CONSOLEBODY + i);
   if (ui.console_colours_valid
   && !memcmp(colours, ui.console_colours, sizeof(colours)))
      return;
   css = g_string_new(NULL);
#define CONSOLE_RULE(selector, role, border) \
   console_css_rule(css, selector, colours[(role) - ATTR_CONSOLEBODY], border)
   CONSOLE_RULE("#the-editor-console-split, #the-program-console, "
                "#the-console-output, #the-console-output text",
                ATTR_CONSOLEBODY, 0);
   CONSOLE_RULE("#the-program-console .the-console-header, "
                "#the-program-console .the-console-header label", ATTR_CONSOLEHEADER, 0);
   CONSOLE_RULE("#the-console-input-row, #the-console-input-row label, #the-console-input",
                ATTR_CONSOLEINPUT, 1);
   CONSOLE_RULE("#the-program-console .the-console-control, "
                "#the-program-console .the-console-control label, "
                "#the-program-run, #the-program-run label", ATTR_CONSOLECONTROL, 1);
   CONSOLE_RULE("#the-console-output text selection, #the-console-input selection, "
                "#the-program-console .the-console-control:hover, "
                "#the-program-console .the-console-control:hover label, "
                "#the-program-console .the-console-control:focus, "
                "#the-program-console .the-console-control:focus label, "
                "#the-program-console .the-console-control:active, "
                "#the-program-console .the-console-control:active label, "
                "#the-program-run:hover, #the-program-run:hover label, "
                "#the-program-run:focus, #the-program-run:focus label, "
                "#the-program-run:active, #the-program-run:active label", ATTR_CONSOLESELECTED, 1);
   CONSOLE_RULE("#the-program-console #the-console-status", ATTR_CONSOLESTATUS, 0);
   CONSOLE_RULE("#the-editor-console-split > separator", ATTR_CONSOLEDIVIDER, 1);
   g_string_append(css,
      "#the-editor-console-split > separator { border-style: solid; border-width: 1px; }\n");
#undef CONSOLE_RULE
   if (!gtk_css_provider_load_from_data(ui.console_css, css->str, css->len, &error))
   {
      g_warning("GTK console palette: %s", error->message);
      g_clear_error(&error);
      g_string_free(css, TRUE);
      return;
   }
   g_string_free(css, TRUE);
   error_tag = gtk_text_tag_table_lookup(gtk_text_buffer_get_tag_table(
      gtk_text_view_get_buffer(GTK_TEXT_VIEW(ui.output))), "stderr");
   style = the_render_attr_style(colours[ATTR_CONSOLEERROR - ATTR_CONSOLEBODY]);
   console_colours(colours[ATTR_CONSOLEERROR - ATTR_CONSOLEBODY], &fg, &bg);
   g_object_set(error_tag, "foreground-rgba", &fg, "background-rgba", &bg,
      "weight", style & THE_STYLE_BOLD ? PANGO_WEIGHT_BOLD : PANGO_WEIGHT_NORMAL,
      "style", style & THE_STYLE_ITALIC ? PANGO_STYLE_ITALIC : PANGO_STYLE_NORMAL,
      "underline", style & THE_STYLE_UNDERLINE ? PANGO_UNDERLINE_SINGLE : PANGO_UNDERLINE_NONE,
      "strikethrough", (style & THE_STYLE_STRIKEOUT) != 0, NULL);
   memcpy(ui.console_colours, colours, sizeof(colours));
   ui.console_colours_valid = 1;
}

static void draw_surface(cairo_t *cr, PangoLayout *layout, GtkSurface *surface)
{
   TheDriverWindowOrigin origin = operations.window_origin(surface->window);
   TheDriverWindowSize size = operations.window_size(surface->window);
   int row, col;
   if (!surface->visible || !origin.valid || !size.valid)
      return;
   cairo_save(cr);
   cairo_rectangle(cr, origin.col * ui.cell_width, origin.row * ui.cell_height,
                       size.cols * ui.cell_width, size.rows * ui.cell_height);
   cairo_clip(cr);
   /* Backgrounds are separate from glyphs: continuation cells of a wide
    * grapheme must not paint over its glyph after Pango has drawn it. */
   for (row = 0; row < size.rows; row++)
      for (col = 0; col < size.cols; col++)
      {
         TheRenderCell cell;
         TheRenderStyle style;
         int fg, bg;
         headless_driver_render_cell_at(surface->window, row, col, &cell);
         style = the_render_attr_style(cell.attr);
         fg = the_render_attr_fg(cell.attr); bg = the_render_attr_bg(cell.attr);
         colour(cr, style & THE_STYLE_REVERSE ? fg : bg,
                     (style & THE_STYLE_REVERSE) != 0);
         cairo_rectangle(cr, (origin.col + col) * ui.cell_width,
            (origin.row + row) * ui.cell_height, ui.cell_width, ui.cell_height);
         cairo_fill(cr);
      }
   for (row = 0; row < size.rows; row++)
      for (col = 0; col < size.cols; col++)
      {
         TheRenderCell cell;
         TheRenderStyle style;
         char text[THE_RENDER_MAX_UTF8_BYTES + 1];
         size_t length = 0;
         int x = (origin.col + col) * ui.cell_width;
         int y = (origin.row + row) * ui.cell_height;
         int width, height;
         if (!headless_driver_render_cell_at(surface->window, row, col, &cell))
            continue;
         style = the_render_attr_style(cell.attr);
         if (cell.flags & THE_RENDER_CLUSTER_HAS_UTF8)
         {
            length = MIN(cell.utf8_length, sizeof(text) - 1);
            memcpy(text, cell.utf8, length);
         }
         else if (cell.flags & THE_RENDER_CLUSTER_HAS_FALLBACK)
         {
            length = MIN(cell.fallback_length, sizeof(text) - 1);
            memcpy(text, cell.fallback_utf8, length);
         }
         else if (cell.codepoint_count && g_unichar_validate(cell.codepoints[0]))
            length = g_unichar_to_utf8(cell.codepoints[0], text);
         text[length] = 0;
         if (!length || !g_utf8_validate(text, length, NULL))
            continue;
         colour(cr, style & THE_STYLE_REVERSE ? the_render_attr_bg(cell.attr)
                                               : the_render_attr_fg(cell.attr), 1);
         pango_font_description_set_weight(ui.font,
            style & THE_STYLE_BOLD ? PANGO_WEIGHT_BOLD : PANGO_WEIGHT_NORMAL);
         pango_font_description_set_style(ui.font,
            style & THE_STYLE_ITALIC ? PANGO_STYLE_ITALIC : PANGO_STYLE_NORMAL);
         pango_layout_set_font_description(layout, ui.font);
         pango_layout_set_text(layout, text, length);
         pango_layout_get_pixel_size(layout, &width, &height);
         cairo_save(cr);
         cairo_rectangle(cr, x, y, MAX(1, cell.advance_width) * ui.cell_width,
                                                    ui.cell_height);
         cairo_clip(cr);
         cairo_move_to(cr, x, y + MAX(0, (ui.cell_height - height) / 2));
         pango_cairo_show_layout(cr, layout);
         if (style & THE_STYLE_UNDERLINE)
         {
            cairo_rectangle(cr, x, y + ui.cell_height - 2,
               MAX(1, cell.advance_width) * ui.cell_width, 1);
            cairo_fill(cr);
         }
         cairo_restore(cr);
      }
   cairo_restore(cr);
}

static CursorShape cursor_shape(void)
{
   return INSERTMODEx ? cursorstyle_insert_shape : cursorstyle_over_shape;
}

static CursorBlink cursor_blink(void)
{
   return INSERTMODEx ? cursorstyle_insert_blink : cursorstyle_over_blink;
}

static gboolean draw_editor(GtkWidget *widget, cairo_t *cr, gpointer data)
{
   GList *item;
   PangoLayout *layout = gtk_widget_create_pango_layout(widget, "");
   TheDriverWindow *win = ui.input_window ? ui.input_window : driver_current_window();
   TheDriverWindowCursor cursor = operations.capture_window_cursor(win);
   TheDriverWindowOrigin origin = operations.window_origin(win);
   (void)data;
   colour(cr, THE_COLOR_UNSPECIFIED, 0);
   cairo_paint(cr);
   for (item = ui.surfaces; item; item = item->next)
      draw_surface(cr, layout, item->data);
   if (ui.cursor_visible && gtk_widget_has_focus(widget) && cursor.valid && origin.valid
   && (cursor_blink() != CURSOR_BLINK || ui.blink_visible))
   {
      int x = (origin.col + cursor.col) * ui.cell_width;
      int y = (origin.row + cursor.row) * ui.cell_height;
      GdkRectangle area = {x, y, ui.cell_width, ui.cell_height};
      gtk_im_context_set_cursor_location(ui.im, &area);
      cairo_set_source_rgba(cr, 0.95, 0.96, 1.0, 0.85);
      switch (cursor_shape())
      {
         case CURSOR_IBEAM: cairo_rectangle(cr, x, y, 2, ui.cell_height); break;
         case CURSOR_UNDERLINE:
            cairo_rectangle(cr, x, y + ui.cell_height - 2, ui.cell_width, 2); break;
         default:
            cairo_set_source_rgba(cr, 0.95, 0.96, 1.0, 0.35);
            cairo_rectangle(cr, x, y, ui.cell_width, ui.cell_height); break;
      }
      cairo_fill(cr);
   }
   g_object_unref(layout);
   return FALSE;
}

static void sync_console(void)
{
   const TheToolConsole *console = the_tool_console();
   size_t i;
   GtkTextBuffer *buffer;
   GtkTextIter end, anchor, insert;
   int selection = 0, anchor_offset = 0, insert_offset = 0;
   if (!ui.console)
      return;
   /* Colours belong to the active file, independently of a job's version.
    * Updating CSS/tags alone leaves output, selection and job state intact. */
   sync_console_palette();
   if (console->version == ui.console_version)
      return;
   ui.console_version = console->version;
   gtk_widget_set_visible(ui.console, console->visible);
   gtk_label_set_text(GTK_LABEL(ui.state), console->state[0] ? console->state :
      the_tools_backend_available() ? "Ready" : "Native RXBIN tool bundle is not configured");
   gtk_widget_set_sensitive(ui.run, the_tools_backend_available() && !console->running);
   gtk_widget_set_sensitive(ui.stop, console->running);
   gtk_widget_set_sensitive(ui.eof, console->running && !console->eof);
   gtk_widget_set_sensitive(ui.input, console->running && !console->eof);
   gtk_widget_set_sensitive(ui.send, console->running && !console->eof);
   buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(ui.output));
   selection = console->id == ui.console_job_id && gtk_text_buffer_get_has_selection(buffer);
   if (selection)
   {
      gtk_text_buffer_get_iter_at_mark(buffer, &anchor, gtk_text_buffer_get_selection_bound(buffer));
      gtk_text_buffer_get_iter_at_mark(buffer, &insert, gtk_text_buffer_get_insert(buffer));
      anchor_offset = gtk_text_iter_get_offset(&anchor);
      insert_offset = gtk_text_iter_get_offset(&insert);
   }
   ui.console_job_id = console->id;
   gtk_text_buffer_set_text(buffer, "", 0);
   for (i = 0; i < console->count; i++)
   {
      char *text = g_utf8_make_valid(console->chunks[i].text, console->chunks[i].length);
      gtk_text_buffer_get_end_iter(buffer, &end);
      gtk_text_buffer_insert_with_tags_by_name(buffer, &end, text, -1,
         console->chunks[i].fd == 2 ? "stderr" : "stdout", NULL);
      g_free(text);
   }
   if (selection)
   {
      int count = gtk_text_buffer_get_char_count(buffer);
      gtk_text_buffer_get_iter_at_offset(buffer, &anchor, MIN(anchor_offset, count));
      gtk_text_buffer_get_iter_at_offset(buffer, &insert, MIN(insert_offset, count));
      gtk_text_buffer_select_range(buffer, &insert, &anchor);
   }
   else
   {
      gtk_text_buffer_get_end_iter(buffer, &end);
      gtk_text_view_scroll_to_iter(GTK_TEXT_VIEW(ui.output), &end, 0, FALSE, 0, 1);
   }
}

static gboolean blink(gpointer data)
{
   (void)data;
   ui.blink_visible = !ui.blink_visible;
   if (cursor_blink() == CURSOR_BLINK)
      redraw();
   return G_SOURCE_CONTINUE;
}

static GtkWidget *button(GtkWidget *box, const char *label,
                          GCallback callback, const char *argument)
{
   GtkWidget *widget = gtk_button_new_with_label(label);
   gtk_style_context_add_class(gtk_widget_get_style_context(widget), "the-console-control");
   gtk_box_pack_start(GTK_BOX(box), widget, FALSE, FALSE, 0);
   g_signal_connect(widget, "clicked", callback, (gpointer)argument);
   return widget;
}

static GtkWidget *menu(GtkWidget *bar, const char *label)
{
   GtkWidget *root = gtk_menu_item_new_with_label(label);
   GtkWidget *items = gtk_menu_new();
   gtk_menu_item_set_submenu(GTK_MENU_ITEM(root), items);
   gtk_menu_shell_append(GTK_MENU_SHELL(bar), root);
   return items;
}

static void menu_item(GtkWidget *items, const char *label,
                       GCallback callback, const char *argument)
{
   GtkWidget *item = gtk_menu_item_new_with_label(label);
   g_signal_connect(item, "activate", callback, (gpointer)argument);
   gtk_menu_shell_append(GTK_MENU_SHELL(items), item);
}

static int activate(void)
{
   memset(&ui, 0, sizeof(ui));
   g_queue_init(&ui.inputs);
   ui.timeout = -1;
   ui.cursor_visible = ui.blink_visible = 1;
   ui.console_version = (unsigned)-1;
   headless_driver_reset();
   return 1;
}

static int startup(const TheDriverStartupOptions *options, char *error, size_t capacity)
{
   GtkWidget *root, *bar, *items, *toolbar, *scroller, *heading, *controls, *entry;
   PangoLayout *layout;
   int width, height;
   (void)options;
   if (!gtk_init_check(NULL, NULL))
   {
      g_snprintf(error, capacity, "GTK 3 could not open a display");
      return 0;
   }
   the_tools_native_initialise();
   ui.window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
   gtk_window_set_title(GTK_WINDOW(ui.window), "CRXTHE — GTK");
   gtk_window_set_default_size(GTK_WINDOW(ui.window), 1120, 820);
   g_signal_connect(ui.window, "delete-event", G_CALLBACK(close_window), NULL);
   root = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
   gtk_container_add(GTK_CONTAINER(ui.window), root);
   bar = gtk_menu_bar_new();
   gtk_box_pack_start(GTK_BOX(root), bar, FALSE, FALSE, 0);
   items = menu(bar, "File");
   menu_item(items, "New…", G_CALLBACK(file_action), "file.create");
   menu_item(items, "Open…", G_CALLBACK(file_action), "file.open");
   menu_item(items, "Save", G_CALLBACK(file_action), "file.save");
   menu_item(items, "Close current file", G_CALLBACK(file_action), "file.close");
   menu_item(items, "Next file", G_CALLBACK(command_action), "next");
   menu_item(items, "Exit (retain unsaved files)", G_CALLBACK(command_action), "cancel");
   items = menu(bar, "Edit");
   menu_item(items, "Undo", G_CALLBACK(file_action), "edit.undo");
   menu_item(items, "Paste clipboard", G_CALLBACK(paste_action), NULL);
   menu_item(items, "Command line", G_CALLBACK(command_action), "cursor cmdline");
   items = menu(bar, "Program");
   menu_item(items, "Run current file", G_CALLBACK(tool_action), "run");
   menu_item(items, "Show / hide console", G_CALLBACK(toggle_console), NULL);
   menu_item(items, "Stop", G_CALLBACK(tool_action), "stop");
   menu_item(items, "End input", G_CALLBACK(tool_action), "eof");
   menu_item(items, "Clear output", G_CALLBACK(tool_action), "clear");
   menu_item(items, "Edit profile", G_CALLBACK(tool_action), "profile");
   menu_item(items, "Build profile", G_CALLBACK(tool_action), "build");
   menu_item(items, "Activate profile", G_CALLBACK(tool_action), "activate");
   items = menu(bar, "Help");
   menu_item(items, "Editor help", G_CALLBACK(command_action), "help");
   menu_item(items, "Key definitions", G_CALLBACK(command_action), "showkey all");
   toolbar = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
   gtk_container_set_border_width(GTK_CONTAINER(toolbar), 6);
   gtk_box_pack_start(GTK_BOX(root), toolbar, FALSE, FALSE, 0);
   button(toolbar, "Open", G_CALLBACK(file_action), "file.open");
   button(toolbar, "Save", G_CALLBACK(file_action), "file.save");
   button(toolbar, "Undo", G_CALLBACK(file_action), "edit.undo");
   button(toolbar, "Paste", G_CALLBACK(paste_action), NULL);
   button(toolbar, "Command", G_CALLBACK(command_action), "cursor cmdline");
   button(toolbar, "Next file", G_CALLBACK(command_action), "next");
   ui.run = button(toolbar, "Run", G_CALLBACK(tool_action), "run");
   gtk_widget_set_name(ui.run, "the-program-run");
   ui.split = gtk_paned_new(GTK_ORIENTATION_VERTICAL);
   gtk_widget_set_name(ui.split, "the-editor-console-split");
   g_signal_connect(ui.split, "size-allocate", G_CALLBACK(split_allocated), NULL);
   gtk_box_pack_start(GTK_BOX(root), ui.split, TRUE, TRUE, 0);
   ui.editor = gtk_drawing_area_new();
   gtk_widget_set_can_focus(ui.editor, TRUE);
   gtk_widget_set_hexpand(ui.editor, TRUE);
   gtk_widget_set_vexpand(ui.editor, TRUE);
   gtk_widget_set_size_request(ui.editor, 400, 200);
   gtk_widget_add_events(ui.editor, GDK_KEY_PRESS_MASK | GDK_KEY_RELEASE_MASK
      | GDK_BUTTON_PRESS_MASK | GDK_SCROLL_MASK | GDK_SMOOTH_SCROLL_MASK | GDK_FOCUS_CHANGE_MASK);
   gtk_paned_pack1(GTK_PANED(ui.split), ui.editor, TRUE, FALSE);
   ui.im = gtk_im_multicontext_new();
   g_signal_connect(ui.im, "commit", G_CALLBACK(im_commit), NULL);
   ui.font = pango_font_description_from_string("Monospace 12");
   layout = gtk_widget_create_pango_layout(ui.editor, "M");
   pango_layout_set_font_description(layout, ui.font);
   pango_layout_get_pixel_size(layout, &width, &height);
   ui.cell_width = MAX(1, width);
   ui.cell_height = MAX(1, height + 2);
   g_object_unref(layout);
   g_signal_connect(ui.editor, "draw", G_CALLBACK(draw_editor), NULL);
   g_signal_connect(ui.editor, "size-allocate", G_CALLBACK(allocated), NULL);
   g_signal_connect(ui.editor, "key-press-event", G_CALLBACK(key_pressed), NULL);
   g_signal_connect(ui.editor, "key-release-event", G_CALLBACK(key_released), NULL);
   g_signal_connect(ui.editor, "button-press-event", G_CALLBACK(clicked), NULL);
   g_signal_connect(ui.editor, "scroll-event", G_CALLBACK(scrolled), NULL);
   g_signal_connect(ui.editor, "realize", G_CALLBACK(editor_realized), NULL);
   g_signal_connect(ui.editor, "focus-in-event", G_CALLBACK(editor_focus), NULL);
   g_signal_connect(ui.editor, "focus-out-event", G_CALLBACK(editor_focus), NULL);
   ui.console = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
   gtk_widget_set_name(ui.console, "the-program-console");
   gtk_container_set_border_width(GTK_CONTAINER(ui.console), 6);
   gtk_paned_pack2(GTK_PANED(ui.split), ui.console, FALSE, FALSE);
   heading = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
   ui.header = heading;
   gtk_style_context_add_class(gtk_widget_get_style_context(heading), "the-console-header");
   gtk_box_pack_start(GTK_BOX(ui.console), heading, FALSE, FALSE, 0);
   controls = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
   gtk_box_pack_start(GTK_BOX(heading), controls, FALSE, FALSE, 0);
   {
      GtkWidget *label = gtk_label_new("Program console");
      gtk_label_set_xalign(GTK_LABEL(label), 0);
      gtk_box_pack_start(GTK_BOX(controls), label, TRUE, TRUE, 0);
   }
   ui.state = gtk_label_new("Ready");
   gtk_widget_set_name(ui.state, "the-console-status");
   gtk_label_set_ellipsize(GTK_LABEL(ui.state), PANGO_ELLIPSIZE_END);
   gtk_label_set_xalign(GTK_LABEL(ui.state), 0);
   gtk_box_pack_start(GTK_BOX(heading), ui.state, FALSE, FALSE, 0);
   ui.stop = button(controls, "Stop", G_CALLBACK(tool_action), "stop");
   ui.eof = button(controls, "EOF", G_CALLBACK(tool_action), "eof");
   button(controls, "Clear", G_CALLBACK(tool_action), "clear");
   button(controls, "Hide", G_CALLBACK(command_action), "rx console off");
   scroller = gtk_scrolled_window_new(NULL, NULL);
   gtk_widget_set_size_request(scroller, -1, 125);
   gtk_box_pack_start(GTK_BOX(ui.console), scroller, TRUE, TRUE, 0);
   ui.output = gtk_text_view_new();
   gtk_widget_set_name(ui.output, "the-console-output");
   gtk_text_view_set_editable(GTK_TEXT_VIEW(ui.output), FALSE);
   gtk_text_view_set_cursor_visible(GTK_TEXT_VIEW(ui.output), FALSE);
   gtk_text_view_set_monospace(GTK_TEXT_VIEW(ui.output), TRUE);
   gtk_container_add(GTK_CONTAINER(scroller), ui.output);
   gtk_text_buffer_create_tag(gtk_text_view_get_buffer(GTK_TEXT_VIEW(ui.output)), "stderr", NULL);
   gtk_text_buffer_create_tag(gtk_text_view_get_buffer(GTK_TEXT_VIEW(ui.output)), "stdout", NULL);
   entry = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
   gtk_widget_set_name(entry, "the-console-input-row");
   gtk_box_pack_start(GTK_BOX(ui.console), entry, FALSE, FALSE, 0);
   gtk_box_pack_start(GTK_BOX(entry), gtk_label_new("Program input"), FALSE, FALSE, 0);
   ui.input = gtk_entry_new();
   gtk_widget_set_name(ui.input, "the-console-input");
   gtk_entry_set_max_length(GTK_ENTRY(ui.input), 4096);
   gtk_box_pack_start(GTK_BOX(entry), ui.input, TRUE, TRUE, 0);
   g_signal_connect(ui.input, "activate", G_CALLBACK(send_input), NULL);
   ui.send = button(entry, "Send", G_CALLBACK(send_input), NULL);
   ui.console_css = gtk_css_provider_new();
   gtk_style_context_add_provider_for_screen(gtk_widget_get_screen(ui.window),
      GTK_STYLE_PROVIDER(ui.console_css), GTK_STYLE_PROVIDER_PRIORITY_APPLICATION + 1);
   gtk_widget_show_all(ui.window);
   gtk_widget_grab_focus(ui.editor);
   the_tool_console_visibility(1, 0);
   sync_console();
   ui.active = 1;
   /* Legacy core spelling for an active interactive display. GTK still
    * renders exclusively through TheDriverOps and never initializes curses. */
   curses_started = TRUE;
   ui.tick = g_timeout_add(500, blink, NULL);
   terminal_lines = ui.rows ? ui.rows : 28;
   terminal_cols = ui.cols ? ui.cols : 100;
   return 1;
}

static TheDriverWindow *track(TheDriverWindow *window)
{
   GtkSurface *surface;
   if (!window)
      return NULL;
   surface = g_new0(GtkSurface, 1);
   surface->window = window;
   ui.surfaces = g_list_append(ui.surfaces, surface);
   return window;
}

static TheDriverWindow *create_window(int rows, int cols, int row, int col)
{
   return track(the_headless_driver_ops.create_window(rows, cols, row, col));
}

static TheDriverWindow *create_role(CHARTYPE scrno, short role,
                                     int rows, int cols, int row, int col)
{
   return track(headless_driver_create_screen_role(scrno, role, rows, cols, row, col));
}

static TheDriverWindow *create_global(TheDriverGlobalWindowRole role,
                                       int rows, int cols, int row, int col)
{
   return track(headless_driver_create_global_window(role, rows, cols, row, col));
}

static void delete_window(TheDriverWindow *window)
{
   GList *item;
   for (item = ui.surfaces; item; item = item->next)
      if (((GtkSurface *)item->data)->window == window)
      {
         g_free(item->data);
         ui.surfaces = g_list_delete_link(ui.surfaces, item);
         break;
      }
   the_headless_driver_ops.delete_window(window);
   redraw();
}

static void refresh_window(TheDriverWindow *window)
{
   GList *item;
   for (item = ui.surfaces; item; item = item->next)
      if (((GtkSurface *)item->data)->window == window)
         ((GtkSurface *)item->data)->visible = 1;
   the_headless_driver_ops.refresh_window(window);
   redraw();
}

static void clear_screen_roles(void)
{
   headless_driver_set_current_screen(current_screen);
   the_headless_driver_ops.clear_current_screen_roles();
}

static void set_role_attr(short role, TheDriverAttr attr)
{
   the_headless_driver_ops.set_window_attr(driver_current_role_window(role), attr);
}

static void clear_role_to_eol(short role)
{
   headless_driver_set_current_screen(current_screen);
   the_headless_driver_ops.clear_current_role_to_eol(role);
}

static void update(void)
{
   the_headless_driver_ops.update();
   if (ui.window && CURRENT_FILE)
   {
      char *title = g_strdup_printf("%s%s — CRXTHE (GTK)",
         the_editor_buffer_dirty(CURRENT_FILE) ? "* " : "", CURRENT_FILE->fname);
      gtk_window_set_title(GTK_WINDOW(ui.window), title);
      g_free(title);
   }
   sync_console();
   redraw();
}

static int read_input(TheInputEvent *event)
{
   gint64 deadline = ui.timeout < 0 ? G_MAXINT64
                                    : g_get_monotonic_time() + ui.timeout * 1000;
   *event = the_input_event_none();
   while (ui.active)
   {
      if (!ui.input_window && !the_panel_active && !in_macro && ui.rows && ui.cols
      && (ui.rows != terminal_lines || ui.cols != terminal_cols))
         return the_input_event_from_resize(ui.rows, ui.cols, event);
      if (pop_input(event))
      {
         ui.blink_visible = 1;
         return 1;
      }
      the_tools_native_poll();
      sync_console();
      while (g_main_context_pending(NULL))
         g_main_context_iteration(NULL, FALSE);
      if (!g_queue_is_empty(&ui.inputs))
         continue;
      if (g_get_monotonic_time() >= deadline)
         return 0;
      g_usleep(5000);
   }
   return 0;
}

static int read_raw(TheDriverWindow *window)
{
   TheInputEvent event;
   int key = THE_KEY_NONE;
   ui.input_window = window;
   redraw();
   if (read_input(&event))
      the_input_event_to_legacy_key(&event, &key);
   ui.input_window = NULL;
   return key;
}

static void set_timeout(int milliseconds) { ui.timeout = milliseconds; }
static void present_cursor(bool visible) { ui.cursor_visible = visible; redraw(); }
static void resize(int rows, int cols)
{
   if (rows > 0) terminal_lines = rows;
   if (cols > 0) terminal_cols = cols;
}
static const char *version(void) { return "gtk3"; }

static void shutdown_driver(int prompt)
{
   GList *item;
   (void)prompt;
   ui.active = 0;
   curses_started = FALSE;
   if (ui.tick) g_source_remove(ui.tick);
   ui.tick = 0;
   if (ui.split_layout_idle) g_source_remove(ui.split_layout_idle);
   ui.split_layout_idle = 0;
   if (ui.im) g_object_unref(ui.im);
   ui.im = NULL;
   if (ui.console_css)
   {
      gtk_style_context_remove_provider_for_screen(gtk_widget_get_screen(ui.window),
                                                    GTK_STYLE_PROVIDER(ui.console_css));
      g_object_unref(ui.console_css);
      ui.console_css = NULL;
   }
   if (ui.window) gtk_widget_destroy(ui.window);
   ui.window = ui.editor = ui.console = NULL;
   if (ui.font) pango_font_description_free(ui.font);
   ui.font = NULL;
   for (item = ui.surfaces; item; item = item->next)
      g_free(item->data);
   g_list_free(ui.surfaces);
   ui.surfaces = NULL;
   while (!g_queue_is_empty(&ui.inputs))
   {
      GtkInput *input = g_queue_pop_head(&ui.inputs);
      g_free(input->text);
      g_free(input);
   }
   headless_driver_reset();
}

static const TheDriverModuleLifecycle lifecycle = {
   .name = "gtk", .activate = activate, .start = startup,
   .shutdown = shutdown_driver, .resize_terminal = resize,
   .set_current_screen = headless_driver_set_current_screen,
   .set_screen_current_role = headless_driver_set_screen_current_role,
   .create_screen_role = create_role, .create_global_window = create_global,
   .read_raw_window_key = read_raw, .ui_version = version,
   .current_cursor_shape = cursor_shape, .current_cursor_blink = cursor_blink,
   .log_count = headless_driver_log_count, .log_entry = headless_driver_log_entry
};

int the_driver_module_abi_version(void) { return THE_DRIVER_MODULE_ABI_VERSION; }
const TheDriverOps *the_driver_module_ops(void)
{
   operations = the_headless_driver_ops;
   operations.create_window = create_window;
   operations.delete_window = delete_window;
   operations.refresh_window = refresh_window;
   operations.refresh_window_now = refresh_window;
   operations.clear_current_screen_roles = clear_screen_roles;
   operations.set_current_role_attr = set_role_attr;
   operations.clear_current_role_to_eol = clear_role_to_eol;
   operations.update = update;
   operations.read_input_event = read_input;
   operations.set_current_window_timeout = set_timeout;
   operations.present_cursor = present_cursor;
   return &operations;
}
const TheDriverModuleLifecycle *the_driver_module_lifecycle(void) { return &lifecycle; }
