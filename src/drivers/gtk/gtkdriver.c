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
#include "workbench.h"
#include "workbenchruntime.h"
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

typedef struct
{
   char id[THE_WORKBENCH_ID_MAX + 1];
   unsigned long generation;
   uint64_t revision, next_cursor;
   GtkWidget *page, *view, *scroller, *status;
   GdkFrameClock *scroll_clock;
   GtkAdjustment *scroll_adjustment;
   gulong scroll_handler;
   double scroll_value;
   struct { uint64_t cursor; int character; } records[THE_WORKBENCH_LOG_RECORD_MAX];
   size_t record_count;
} GtkLogPane;

static struct
{
   GtkWidget *window, *split, *editor, *console, *output, *input, *state;
   GtkWidget *run, *stop, *eof, *send, *header, *first_control, *menu_focus;
   GtkApplication *application;
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
   TheDriverWindow *input_window, *menu_popup;
   int menu_canceled, menu_entry_position, menu_selection_start, menu_selection_end;
   int menu_entry_selected;
   GMenu *menu_model;
   GtkWidget *native_menu;
   unsigned long menu_revision;
   GPtrArray *menu_actions;
   GtkWidget *auxiliary, *log_origin, *last_nonlog_focus;
   GPtrArray *logs;
   char focused_log[THE_WORKBENCH_ID_MAX + 1];
   unsigned long focused_log_generation, log_focus_serial;
   int syncing_logs, log_origin_program, log_entry_position;
   int log_selection_start, log_selection_end, log_entry_selected;
} ui;

static TheDriverOps operations;
static void sync_menus(void);
static void sync_logs(void);
static GtkLogPane *log_for_widget(GtkWidget *widget);
static int queue_panel(GtkLogPane *pane, TheWorkbenchPanelOperation operation);
static void remember_log_origin(GtkWidget *focus);
static void cycle_log(GtkLogPane *pane, int reverse);
static void open_native_menu(GtkWidget *focus, GdkEventKey *trigger);

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
   if (ui.menu_popup && event->kind == THE_INPUT_KEY)
      ui.menu_canceled = event->key_code == 27;
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

static void remember_menu_focus(GtkWidget *focus)
{
   g_set_object(&ui.menu_focus, (focus ? focus : ui.editor));
   ui.menu_popup = NULL;
   ui.menu_canceled = 0;
   ui.menu_entry_selected = 0;
   if (ui.menu_focus == ui.input)
   {
      ui.menu_entry_position = gtk_editable_get_position(GTK_EDITABLE(ui.input));
      ui.menu_entry_selected = gtk_editable_get_selection_bounds(GTK_EDITABLE(ui.input),
         &ui.menu_selection_start, &ui.menu_selection_end);
   }
}

static void finish_menu_focus(void)
{
   if (ui.menu_canceled && ui.menu_focus)
   {
      GtkWidget *focus = gtk_widget_is_drawable(ui.menu_focus)
                     && gtk_widget_is_sensitive(ui.menu_focus) ? ui.menu_focus : ui.editor;
      gtk_widget_grab_focus(focus);
      if (focus == ui.input)
      {
         if (ui.menu_entry_selected)
            gtk_editable_select_region(GTK_EDITABLE(ui.input),
               ui.menu_entry_position == ui.menu_selection_start
                  ? ui.menu_selection_end : ui.menu_selection_start,
               ui.menu_entry_position);
         else
            gtk_editable_set_position(GTK_EDITABLE(ui.input), ui.menu_entry_position);
      }
   }
   g_clear_object(&ui.menu_focus);
   ui.menu_popup = NULL;
   ui.menu_canceled = 0;
}

/* Only lower the stock shared-menu binding. Profiles retain authority to
 * redefine F10; modal READ and active macros always keep their own keys. */
static int shared_menu_key_binding(void)
{
   const char *definition = (const char *)get_key_definition(THE_KEY_F(10), THE_KEY_DEFINE_RAW, TRUE, FALSE);
   return definition && (!my_stricmp((DEFCHAR *)definition, "macro workbenchmenu")
      || !my_stricmp((DEFCHAR *)definition, "macro workbenchmenu_regina"));
}

static gboolean workbench_key_pressed(GtkWidget *widget, GdkEventKey *key, gpointer data)
{
   GtkWidget *focus = gtk_window_get_focus(GTK_WINDOW(ui.window));
   TheInputEvent event;
   (void)widget; (void)data;
   if (!the_panel_active && !in_macro && !ui.input_window)
   {
      GtkLogPane *pane = log_for_widget(focus);
      if (pane)
      {
         if (key->keyval == GDK_KEY_Escape) return queue_panel(pane, THE_WORKBENCH_PANEL_RETURN);
         if (key->keyval == GDK_KEY_h || key->keyval == GDK_KEY_H ||
             ((key->state & GDK_CONTROL_MASK) && key->keyval == GDK_KEY_w))
            return queue_panel(pane, THE_WORKBENCH_PANEL_HIDE);
         if (key->keyval == GDK_KEY_Tab || key->keyval == GDK_KEY_ISO_Left_Tab)
         { cycle_log(pane, key->keyval == GDK_KEY_ISO_Left_Tab || (key->state & GDK_SHIFT_MASK)); return TRUE; }
      }
   }
   if (the_panel_active || in_macro || ui.input_window
   || key->state & (GDK_CONTROL_MASK | GDK_META_MASK | GDK_MOD1_MASK))
      return FALSE;
   if (key->keyval == GDK_KEY_ISO_Left_Tab
   || (key->keyval == GDK_KEY_Tab && key->state & GDK_SHIFT_MASK))
   {
      /* This moves physical widget focus only. The core retains the exact
       * file/command/prefix caret; ordinary Tab still reaches its profile. */
      if (focus == ui.editor && gtk_widget_get_visible(ui.console))
         gtk_widget_grab_focus(gtk_widget_get_sensitive(ui.input) ? ui.input : ui.output);
      else if (focus == ui.editor || (focus && gtk_widget_is_ancestor(focus, ui.console)))
         gtk_widget_grab_focus(ui.first_control);
      else
         gtk_widget_grab_focus(ui.editor);
      return TRUE;
   }
   if (focus != ui.editor && key->keyval == GDK_KEY_Escape)
   {
      gtk_widget_grab_focus(ui.editor);
      return TRUE;
   }
   if (key->keyval == GDK_KEY_F10 && shared_menu_key_binding())
   {
      open_native_menu(focus, key);
      return TRUE;
   }
   if ((key->keyval == GDK_KEY_F10 || (focus != ui.editor && key->keyval == GDK_KEY_F6))
   && the_gtk_input_key(key->keyval, key->state, &event) && queue_event(event))
   {
      gtk_widget_grab_focus(ui.editor);
      return TRUE;
   }
   return FALSE;
}

static gboolean key_pressed(GtkWidget *widget, GdkEventKey *key, gpointer data)
{
   TheInputEvent event;
   gunichar character;
   (void)widget; (void)data;
   if (workbench_key_pressed(widget, key, data))
      return TRUE;
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
   if (ui.auxiliary)
      gtk_widget_get_preferred_width(ui.auxiliary, &console_width, NULL);
   {
      GtkWidget *selected = ui.auxiliary ? gtk_notebook_get_nth_page(GTK_NOTEBOOK(ui.auxiliary),
         gtk_notebook_get_current_page(GTK_NOTEBOOK(ui.auxiliary))) : NULL;
      GtkLogPane *pane = log_for_widget(selected);
      const TheWorkbenchPanel *panel = pane ? the_workbench_panel_find(pane->id) : NULL;
      if (panel) console_width = MAX(console_width, panel->mincols * ui.cell_width);
      orientation = allocation->width >= GTK_EDITOR_MIN_COLUMNS * ui.cell_width
                                         + console_width + ui.cell_width
                                    ? GTK_ORIENTATION_HORIZONTAL : GTK_ORIENTATION_VERTICAL;
      if (panel && panel->placement == THE_WORKBENCH_PLACE_BELOW) orientation = GTK_ORIENTATION_VERTICAL;
   }
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

static GtkLogPane *find_log(const char *id, unsigned long generation)
{
   guint i;
   if (!ui.logs) return NULL;
   for (i = 0; i < ui.logs->len; i++)
   {
      GtkLogPane *pane = g_ptr_array_index(ui.logs, i);
      if (!strcmp(pane->id, id) && pane->generation == generation) return pane;
   }
   return NULL;
}

static GtkLogPane *log_for_widget(GtkWidget *widget)
{
   guint i;
   if (!widget || !ui.logs) return NULL;
   for (i = 0; i < ui.logs->len; i++)
   {
      GtkLogPane *pane = g_ptr_array_index(ui.logs, i);
      if (widget == pane->page || gtk_widget_is_ancestor(widget, pane->page)) return pane;
   }
   return NULL;
}

static int queue_panel(GtkLogPane *pane, TheWorkbenchPanelOperation operation)
{
   TheInputEvent event;
   if (the_panel_active || in_macro || !pane) return 0;
   if (operation == THE_WORKBENCH_PANEL_FOCUS && !ui.focused_log[0] && !ui.log_origin)
      remember_log_origin(gtk_window_get_focus(GTK_WINDOW(ui.window)));
   if (operation == THE_WORKBENCH_PANEL_RETURN && !ui.focused_log[0])
   {
      GtkWidget *target = ui.last_nonlog_focus;
      gtk_widget_grab_focus(target && gtk_widget_is_drawable(target) && gtk_widget_is_sensitive(target) ? target : ui.editor);
      return 1;
   }
   return the_input_event_from_workbench_panel(pane->id, pane->generation, operation, &event) && queue_event(event);
}

static void remember_log_origin(GtkWidget *focus)
{
   if (!focus || focus == ui.auxiliary || log_for_widget(focus))
      focus = ui.last_nonlog_focus ? ui.last_nonlog_focus : ui.editor;
   g_clear_object(&ui.log_origin);
   ui.log_origin = g_object_ref(focus ? focus : ui.editor);
   ui.log_origin_program = the_tool_console()->focused;
   ui.log_entry_selected = 0;
   if (ui.log_origin == ui.input)
   {
      ui.log_entry_position = gtk_editable_get_position(GTK_EDITABLE(ui.input));
      ui.log_entry_selected = gtk_editable_get_selection_bounds(GTK_EDITABLE(ui.input),
         &ui.log_selection_start, &ui.log_selection_end);
   }
}

static void restore_log_origin(void)
{
   GtkWidget *focus = ui.log_origin;
   if (focus && (focus == ui.console || gtk_widget_is_ancestor(focus, ui.console)))
      gtk_notebook_set_current_page(GTK_NOTEBOOK(ui.auxiliary), 0);
   if (!focus || !gtk_widget_is_drawable(focus) || !gtk_widget_is_sensitive(focus)) focus = ui.editor;
   if (ui.log_origin_program && the_tool_console()->visible)
      the_tool_console_visibility(1, 1);
   gtk_widget_grab_focus(focus);
   if (focus == ui.input)
   {
      if (ui.log_entry_selected)
         gtk_editable_select_region(GTK_EDITABLE(ui.input),
            ui.log_entry_position == ui.log_selection_start ? ui.log_selection_end : ui.log_selection_start,
            ui.log_entry_position);
      else gtk_editable_set_position(GTK_EDITABLE(ui.input), ui.log_entry_position);
   }
   g_clear_object(&ui.log_origin);
}

static void select_program_from_log(GtkLogPane *pane)
{
   remember_log_origin(gtk_widget_get_sensitive(ui.input) ? ui.input : ui.output);
   ui.log_origin_program = the_tool_console()->visible;
   queue_panel(pane, THE_WORKBENCH_PANEL_RETURN);
}

static void cycle_log(GtkLogPane *pane, int reverse)
{
   int page = gtk_notebook_page_num(GTK_NOTEBOOK(ui.auxiliary), pane->page);
   int n = gtk_notebook_get_n_pages(GTK_NOTEBOOK(ui.auxiliary)), tries;
   for (tries = 0; tries < n; tries++)
   {
      GtkWidget *next;
      GtkLogPane *log;
      page = (page + n + (reverse ? -1 : 1)) % n;
      next = gtk_notebook_get_nth_page(GTK_NOTEBOOK(ui.auxiliary), page);
      if (!gtk_widget_get_visible(next)) continue;
      if (next == ui.console) { select_program_from_log(pane); return; }
      log = log_for_widget(next);
      if (log) { queue_panel(log, THE_WORKBENCH_PANEL_FOCUS); return; }
   }
}

static void auxiliary_switched(GtkNotebook *notebook, GtkWidget *page, guint number, gpointer data)
{
   GtkLogPane *pane;
   (void)notebook; (void)number; (void)data;
   if (ui.syncing_logs || the_panel_active || in_macro) return;
   pane = log_for_widget(page);
   if (pane) queue_panel(pane, THE_WORKBENCH_PANEL_FOCUS);
   else if (page == ui.console && ui.focused_log[0])
      select_program_from_log(find_log(ui.focused_log, ui.focused_log_generation));
}

static void remember_nonlog_focus(GtkWindow *window, GtkWidget *focus, gpointer data)
{
   (void)window; (void)data;
   if (!focus || focus == ui.auxiliary || log_for_widget(focus)) return;
   g_clear_object(&ui.last_nonlog_focus); ui.last_nonlog_focus = g_object_ref(focus);
}

static gboolean log_button_pressed(GtkWidget *widget, GdkEventButton *event, gpointer data)
{
   (void)widget;
   if (event->button == 1) queue_panel(data, THE_WORKBENCH_PANEL_FOCUS);
   return FALSE;
}

static void log_hide_clicked(GtkWidget *widget, gpointer data)
{ (void)widget; queue_panel(data, THE_WORKBENCH_PANEL_HIDE); }
static void log_return_clicked(GtkWidget *widget, gpointer data)
{ (void)widget; queue_panel(data, THE_WORKBENCH_PANEL_RETURN); }

static void cancel_log_scroll(GtkLogPane *pane)
{
   if (pane->scroll_clock && pane->scroll_handler)
      g_signal_handler_disconnect(pane->scroll_clock, pane->scroll_handler);
   pane->scroll_handler = 0;
   g_clear_object(&pane->scroll_clock); g_clear_object(&pane->scroll_adjustment);
}

static void log_scroll_after_paint(GdkFrameClock *clock, gpointer data)
{
   GtkLogPane *pane = data;
   (void)clock;
   gtk_adjustment_set_value(pane->scroll_adjustment, pane->scroll_value);
   cancel_log_scroll(pane);
}

static void free_log_pane(gpointer data)
{
   GtkLogPane *pane = data;
   cancel_log_scroll(pane); g_free(pane);
}

static GtkLogPane *create_log(const TheWorkbenchPanel *panel)
{
   GtkLogPane *pane = g_new0(GtkLogPane, 1);
   GtkWidget *controls, *hide, *back;
   GtkTextBuffer *buffer;
   strcpy(pane->id, panel->id); pane->generation = panel->generation;
   pane->revision = UINT64_MAX;
   pane->page = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
   gtk_container_set_border_width(GTK_CONTAINER(pane->page), 6);
   controls = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
   pane->status = gtk_label_new("Read-only log");
   gtk_label_set_xalign(GTK_LABEL(pane->status), 0);
   gtk_box_pack_start(GTK_BOX(controls), pane->status, TRUE, TRUE, 0);
   hide = gtk_button_new_with_label("Hide"); back = gtk_button_new_with_label("Return");
   g_signal_connect(hide, "clicked", G_CALLBACK(log_hide_clicked), pane);
   g_signal_connect(back, "clicked", G_CALLBACK(log_return_clicked), pane);
   gtk_box_pack_start(GTK_BOX(controls), hide, FALSE, FALSE, 0);
   gtk_box_pack_start(GTK_BOX(controls), back, FALSE, FALSE, 0);
   gtk_box_pack_start(GTK_BOX(pane->page), controls, FALSE, FALSE, 0);
   pane->scroller = gtk_scrolled_window_new(NULL, NULL);
   gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(pane->scroller), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
   pane->view = gtk_text_view_new();
   g_signal_connect(pane->view, "button-press-event", G_CALLBACK(log_button_pressed), pane);
   gtk_text_view_set_editable(GTK_TEXT_VIEW(pane->view), FALSE);
   gtk_text_view_set_cursor_visible(GTK_TEXT_VIEW(pane->view), FALSE);
   gtk_text_view_set_monospace(GTK_TEXT_VIEW(pane->view), TRUE);
   gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(pane->view), GTK_WRAP_CHAR);
   gtk_widget_set_tooltip_text(pane->view, "Read-only log. Page Up/Down and Home/End scroll; Tab visits panes; H hides; Esc returns.");
   gtk_container_add(GTK_CONTAINER(pane->scroller), pane->view);
   gtk_box_pack_start(GTK_BOX(pane->page), pane->scroller, TRUE, TRUE, 0);
   buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(pane->view));
   gtk_text_buffer_create_tag(buffer, "stdout", NULL);
   gtk_text_buffer_create_tag(buffer, "stderr", "foreground", "tomato", NULL);
   gtk_notebook_append_page(GTK_NOTEBOOK(ui.auxiliary), pane->page, gtk_label_new(panel->title));
   gtk_widget_show_all(pane->page);
   g_ptr_array_add(ui.logs, pane);
   return pane;
}

static void append_log_text(GString *text, const char *bytes, size_t length)
{
   size_t i;
   for (i = 0; i < length; i++)
   {
      unsigned char c = (unsigned char)bytes[i];
      if ((c < 32 && c != '\n') || c == 127) g_string_append_printf(text, "\\x%02X", c);
      else if (c == 0xc2 && i + 1 < length &&
               (unsigned char)bytes[i + 1] >= 0x80 && (unsigned char)bytes[i + 1] <= 0x9f)
         g_string_append_printf(text, "\\u00%02X", (unsigned char)bytes[++i]);
      else g_string_append_c(text, bytes[i]);
   }
}

static void sync_log_text(GtkLogPane *pane, const TheWorkbenchPanel *panel)
{
   const TheWorkbenchLogInfo *info = the_workbench_log_find(panel->model);
   GtkTextBuffer *buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(pane->view));
   GtkAdjustment *adjustment = gtk_scrolled_window_get_vadjustment(GTK_SCROLLED_WINDOW(pane->scroller));
   GtkTextIter insert, anchor, end, first;
   TheWorkbenchLogPage page;
   GString *text;
   uint64_t cursor = 0;
   size_t offset = 0, i, new_count = 0;
   int selected, insert_offset, anchor_offset, removed = 0;
   int follow, incremental = 0;
   double scroll, deleted_y = 0;
   char status[160];
   if (!info || info->generation != panel->model_generation || pane->revision == info->revision) return;
   pane->revision = info->revision;
   selected = gtk_text_buffer_get_has_selection(buffer);
   gtk_text_buffer_get_iter_at_mark(buffer, &insert, gtk_text_buffer_get_insert(buffer));
   gtk_text_buffer_get_iter_at_mark(buffer, &anchor, gtk_text_buffer_get_selection_bound(buffer));
   insert_offset = gtk_text_iter_get_offset(&insert); anchor_offset = gtk_text_iter_get_offset(&anchor);
   scroll = gtk_adjustment_get_value(adjustment);
   follow = !selected && scroll >= gtk_adjustment_get_upper(adjustment) - gtk_adjustment_get_page_size(adjustment) - 2;
   if (pane->record_count && info->first_cursor != pane->records[0].cursor)
   {
      GdkRectangle location;
      removed = gtk_text_buffer_get_char_count(buffer);
      for (i = 0; i < pane->record_count; i++)
         if (pane->records[i].cursor == info->first_cursor) { removed = pane->records[i].character; break; }
      gtk_text_buffer_get_iter_at_offset(buffer, &end, removed);
      gtk_text_view_get_iter_location(GTK_TEXT_VIEW(pane->view), &end, &location); deleted_y = location.y;
   }
   cancel_log_scroll(pane);
   if (pane->record_count && info->records)
   {
      for (i = 0; i < pane->record_count; i++)
         if (pane->records[i].cursor == info->first_cursor)
         {
            size_t retained = pane->record_count - i, j;
            incremental = 1;
            if (removed)
            {
               gtk_text_buffer_get_start_iter(buffer, &first);
               gtk_text_buffer_get_iter_at_offset(buffer, &end, removed);
               gtk_text_buffer_delete(buffer, &first, &end);
            }
            memmove(pane->records, pane->records + i, retained * sizeof(pane->records[0]));
            for (j = 0; j < retained; j++) pane->records[j].character -= removed;
            new_count = retained; cursor = pane->next_cursor;
            break;
         }
   }
   if (!incremental) gtk_text_buffer_set_text(buffer, "", 0);
   text = g_string_new(NULL);
   do
   {
      if (the_workbench_log_page(panel->model, panel->model_generation, cursor, offset,
          THE_WORKBENCH_LOG_PAGE_MAX, THE_WORKBENCH_LOG_TEXT_MAX, &page) != THE_WORKBENCH_LOG_OK) break;
      for (i = 0; i < page.count; i++)
      {
         const TheWorkbenchLogSlice *slice = page.slices + i;
         if (new_count < THE_WORKBENCH_LOG_RECORD_MAX && (!new_count || pane->records[new_count - 1].cursor != slice->cursor))
         {
            pane->records[new_count].cursor = slice->cursor;
            pane->records[new_count++].character = gtk_text_buffer_get_char_count(buffer);
         }
         g_string_truncate(text, 0); append_log_text(text, slice->text, slice->length);
         gtk_text_buffer_get_end_iter(buffer, &end);
         gtk_text_buffer_insert_with_tags_by_name(buffer, &end, text->str, text->len,
                                                 slice->stream == 2 ? "stderr" : "stdout", NULL);
      }
      cursor = page.next_cursor; offset = page.next_offset;
   } while (page.more);
   g_string_free(text, TRUE); pane->record_count = new_count; pane->next_cursor = info->next_cursor;
   if (selected && !incremental)
   {
      int count = gtk_text_buffer_get_char_count(buffer);
      gtk_text_buffer_get_iter_at_offset(buffer, &insert, CLAMP(insert_offset - removed, 0, count));
      gtk_text_buffer_get_iter_at_offset(buffer, &anchor, CLAMP(anchor_offset - removed, 0, count));
      gtk_text_buffer_select_range(buffer, &insert, &anchor);
   }
   if (follow)
   {
      gtk_text_buffer_get_end_iter(buffer, &end);
      gtk_text_view_scroll_to_iter(GTK_TEXT_VIEW(pane->view), &end, 0, FALSE, 0, 1);
   }
   else if (incremental && !removed) gtk_adjustment_set_value(adjustment, scroll);
   else
   {
      /* TextView validates layout and may scroll its insertion mark later.
       * Restore the user's viewport after that frame without moving focus. */
      pane->scroll_value = MAX(0, scroll - deleted_y);
      pane->scroll_clock = gtk_widget_get_frame_clock(pane->view);
      if (pane->scroll_clock)
      {
         g_object_ref(pane->scroll_clock);
         pane->scroll_adjustment = g_object_ref(adjustment);
         pane->scroll_handler = g_signal_connect(pane->scroll_clock, "after-paint", G_CALLBACK(log_scroll_after_paint), pane);
         gtk_widget_queue_draw(pane->view);
      }
      else gtk_adjustment_set_value(adjustment, pane->scroll_value);
   }
   if (info->omitted_bytes) g_snprintf(status, sizeof(status), "History omitted: %llu bytes", (unsigned long long)info->omitted_bytes);
   else g_snprintf(status, sizeof(status), "Read-only log");
   gtk_label_set_text(GTK_LABEL(pane->status), status);
}

static const char *log_error_name(TheWorkbenchLogResult result)
{
   switch (result)
   {
      case THE_WORKBENCH_LOG_INVALID: return "invalid-output";
      case THE_WORKBENCH_LOG_MISSING: return "missing-model";
      case THE_WORKBENCH_LOG_STALE: return "stale-model";
      case THE_WORKBENCH_LOG_LIMIT: return "limit-exceeded";
      case THE_WORKBENCH_LOG_NOMEM: return "out-of-memory";
      default: return "ok";
   }
}

static void sync_logs(void)
{
   const TheWorkbenchFocusRequest *focus;
   guint i;
   int visible = 0;
   if (!ui.auxiliary || !ui.logs) return;
   ui.syncing_logs++;
   for (i = 0; i < ui.logs->len; )
   {
      GtkLogPane *pane = g_ptr_array_index(ui.logs, i);
      const TheWorkbenchPanel *panel = the_workbench_panel_find(pane->id);
      if (!panel || panel->generation != pane->generation)
      { gtk_widget_destroy(pane->page); g_ptr_array_remove_index(ui.logs, i); continue; }
      i++;
   }
   for (i = 0; i < the_workbench_panel_count(); i++)
   {
      const TheWorkbenchPanel *panel = the_workbench_panel_at(i);
      const TheWorkbenchPanelState *state = the_workbench_panel_state(panel->id);
      GtkLogPane *pane;
      if (panel->kind != THE_WORKBENCH_PANEL_LOG) continue;
      pane = find_log(panel->id, panel->generation);
      if (!pane) pane = create_log(panel);
      gtk_widget_set_visible(pane->page, state && state->visible);
      if (state && state->visible)
      {
         const TheWorkbenchModelState *model = the_workbench_model_state(panel->model);
         const TheWorkbenchLogInfo *info = the_workbench_log_find(panel->model);
         char status[160];
         visible++; sync_log_text(pane, panel);
         if (model && model->last_error != THE_WORKBENCH_LOG_OK)
            g_snprintf(status, sizeof(status), "Log output error: %s", log_error_name(model->last_error));
         else if (info && info->omitted_bytes)
            g_snprintf(status, sizeof(status), "History omitted: %llu bytes", (unsigned long long)info->omitted_bytes);
         else g_strlcpy(status, "Read-only log", sizeof(status));
         if (strcmp(gtk_label_get_text(GTK_LABEL(pane->status)), status)) gtk_label_set_text(GTK_LABEL(pane->status), status);
      }
   }
   gtk_notebook_set_show_tabs(GTK_NOTEBOOK(ui.auxiliary), visible > 0);
   gtk_widget_set_visible(ui.auxiliary, visible || the_tool_console()->visible);
   focus = the_workbench_focus_request();
   if (focus && focus->serial != ui.log_focus_serial)
   {
      GtkLogPane *pane = find_log(focus->panel, focus->generation);
      ui.log_focus_serial = focus->serial;
      if (pane && gtk_widget_get_visible(pane->page))
      {
         if (!ui.focused_log[0] && !ui.log_origin) remember_log_origin(gtk_window_get_focus(GTK_WINDOW(ui.window)));
         strcpy(ui.focused_log, pane->id); ui.focused_log_generation = pane->generation;
         if (the_tool_console()->focused) the_tool_console_visibility(the_tool_console()->visible, 0);
         gtk_notebook_set_current_page(GTK_NOTEBOOK(ui.auxiliary), gtk_notebook_page_num(GTK_NOTEBOOK(ui.auxiliary), pane->page));
         gtk_widget_grab_focus(pane->view);
      }
      else if (!focus->panel[0] && ui.focused_log[0])
      { ui.focused_log[0] = '\0'; restore_log_origin(); }
   }
   for (i = 0; i < ui.logs->len; i++)
   {
      GtkLogPane *pane = g_ptr_array_index(ui.logs, i);
      int shown = gtk_widget_get_visible(pane->page);
      int selected_page = gtk_notebook_get_nth_page(GTK_NOTEBOOK(ui.auxiliary), gtk_notebook_get_current_page(GTK_NOTEBOOK(ui.auxiliary))) == pane->page;
      TheWorkbenchPlacement placement = !shown ? THE_WORKBENCH_PLACE_NONE : !selected_page ? THE_WORKBENCH_PLACE_TAB
         : gtk_orientable_get_orientation(GTK_ORIENTABLE(ui.split)) == GTK_ORIENTATION_HORIZONTAL
            ? THE_WORKBENCH_PLACE_RIGHT : THE_WORKBENCH_PLACE_BELOW;
      const TheWorkbenchPanel *panel = the_workbench_panel_find(pane->id);
      GtkAllocation allocation;
      int fits = 1, allowed = 0, controls_height = 0;
      size_t fallback;
      gtk_widget_get_allocation(pane->page, &allocation);
      gtk_widget_get_preferred_height(gtk_widget_get_parent(pane->status), &controls_height, NULL);
      if (shown && selected_page && panel && allocation.width > 1 && allocation.height > 1)
         fits = allocation.width - 12 >= panel->mincols * ui.cell_width &&
            allocation.height - 16 - controls_height >= panel->minrows * ui.cell_height;
      if (panel && shown && selected_page)
      {
         if (panel->placement == placement || panel->placement == THE_WORKBENCH_PLACE_TAB) allowed = 1;
         for (fallback = 0; fallback < panel->fallback_count; fallback++)
            if (panel->fallbacks[fallback] == placement) allowed = 1;
         if (!allowed)
            for (fallback = 0; fallback < panel->fallback_count; fallback++)
               if (panel->fallbacks[fallback] == THE_WORKBENCH_PLACE_TAB) { placement = THE_WORKBENCH_PLACE_TAB; allowed = 1; break; }
         if (panel->placement == THE_WORKBENCH_PLACE_TAB) placement = THE_WORKBENCH_PLACE_TAB;
      }
      else allowed = 1;
      if (!fits || !allowed) placement = THE_WORKBENCH_PLACE_NONE;
      if (shown && selected_page)
      {
         gtk_widget_set_visible(pane->scroller, fits && allowed);
         if (!fits || !allowed) gtk_label_set_text(GTK_LABEL(pane->status), "Log collapsed: insufficient space");
      }
      the_workbench_panel_effective(pane->id, pane->generation, placement,
                                   !shown ? "hidden" : !fits ? "insufficient-space" : !allowed ? "placement-unavailable" : !selected_page ? "auxiliary-tab" : "");
   }
   ui.syncing_logs--;
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

typedef struct
{
   char id[THE_WORKBENCH_ID_MAX + 1];
   char name[32];
   unsigned long generation;
   GMenu *section; /* borrowed from the current menu model */
   int index;
   char label[THE_WORKBENCH_LABEL_MAX + 1], reason[64];
} GtkMenuAction;

static int menu_action_enabled(const TheWorkbenchAction *action)
{
   const char *reason = the_workbench_action_reason(action);
   return !strcmp(reason, "ok") || !strcmp(reason, "argument-required");
}

static void menu_action(GSimpleAction *action, GVariant *parameter, gpointer data)
{
   GtkMenuAction *entry = data;
   const TheWorkbenchAction *found = the_workbench_action_find(entry->id);
   TheInputEvent event;
   char *path = NULL;
   (void)action; (void)parameter;
   if (the_panel_active || in_macro) return;
   /* The chooser supplies data to the typed action. Core invocation still
    * revalidates the copied generation, path and active frontend policy. */
   if (found && found->generation == entry->generation
   && found->kind == THE_WORKBENCH_ACTION_BUILTIN && !found->operand[0]
   && (found->builtin == THE_FRONTEND_ACTION_FILE_OPEN
       || found->builtin == THE_FRONTEND_ACTION_FILE_CREATE
       || found->builtin == THE_FRONTEND_ACTION_BUFFER_SWITCH))
   {
      int creating = found->builtin == THE_FRONTEND_ACTION_FILE_CREATE;
      GtkWidget *dialog = gtk_file_chooser_dialog_new(
         creating ? "New file" : "Open file", GTK_WINDOW(ui.window),
         creating ? GTK_FILE_CHOOSER_ACTION_SAVE : GTK_FILE_CHOOSER_ACTION_OPEN,
         "_Cancel", GTK_RESPONSE_CANCEL, creating ? "_Create" : "_Open",
         GTK_RESPONSE_ACCEPT, NULL);
      if (CURRENT_FILE)
         gtk_file_chooser_set_current_folder(GTK_FILE_CHOOSER(dialog), (char *)CURRENT_FILE->fpath);
      if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT)
         path = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dialog));
      gtk_widget_destroy(dialog);
      if (!path) return;
   }
   if (the_input_event_from_workbench_invoke(entry->id, entry->generation, path, &event)
   && queue_event(event))
   {
      ui.menu_canceled = 0;
      if (!found || found->kind != THE_WORKBENCH_ACTION_PANEL)
         gtk_widget_grab_focus(ui.editor);
   }
   g_free(path);
}

/* GMenu labels use underscores for mnemonics. Escape them so a published
 * label remains literal data, including slash, quotes and Unicode. */
static char *menu_label(const char *label)
{
   GString *escaped = g_string_new(NULL);
   for (; *label; label++)
   {
      if (*label == '_') g_string_append_c(escaped, '_');
      g_string_append_c(escaped, *label);
   }
   return g_string_free(escaped, FALSE);
}

static void sync_menus(void)
{
   size_t menus[THE_WORKBENCH_MENU_MAX], items[THE_WORKBENCH_ITEM_MAX];
   size_t nm, ni, i, j, k, n;
   if (!ui.application || !ui.menu_model) return;
   if (ui.menu_revision != the_workbench_revision())
   {
      if (ui.native_menu) gtk_menu_shell_deactivate(GTK_MENU_SHELL(ui.native_menu));
      for (i = 0; i < ui.menu_actions->len; i++)
      {
         GtkMenuAction *entry = g_ptr_array_index(ui.menu_actions, i);
         g_action_map_remove_action(G_ACTION_MAP(ui.application), entry->name);
      }
      g_ptr_array_set_size(ui.menu_actions, 0);
      g_menu_remove_all(ui.menu_model);
      nm = the_workbench_menu_count(); ni = the_workbench_item_count();
      for (i = 0; i < nm; i++)
      {
         menus[i] = i;
         for (j = i; j && the_workbench_menu_at(menus[j - 1])->order >
                             the_workbench_menu_at(menus[j])->order; j--)
         { k = menus[j]; menus[j] = menus[j - 1]; menus[j - 1] = k; }
      }
      for (i = 0; i < nm; i++)
      {
         const TheWorkbenchMenu *menu = the_workbench_menu_at(menus[i]);
         GMenu *submenu = g_menu_new(), *section = g_menu_new();
         char *label = menu_label(menu->label);
         n = 0;
         for (j = 0; j < ni; j++)
            if (!strcmp(the_workbench_item_at(j)->menu, menu->id))
            {
               items[n++] = j;
               for (k = n - 1; k && the_workbench_item_at(items[k - 1])->order >
                                     the_workbench_item_at(items[k])->order; k--)
               { size_t swap = items[k]; items[k] = items[k - 1]; items[k - 1] = swap; }
            }
         for (j = 0; j < n; j++)
         {
            const TheWorkbenchItem *item = the_workbench_item_at(items[j]);
            const TheWorkbenchAction *action = the_workbench_action_find(item->action);
            if (item->separator)
            {
               if (g_menu_model_get_n_items(G_MENU_MODEL(section)))
                  g_menu_append_section(submenu, NULL, G_MENU_MODEL(section));
               g_object_unref(section); section = g_menu_new();
            }
            else if (action)
            {
               GtkMenuAction *entry = g_new0(GtkMenuAction, 1);
               GSimpleAction *native;
               char *detailed, *item_label = menu_label(item->label);
               strcpy(entry->id, action->id); entry->generation = action->generation;
               strcpy(entry->label, item->label); entry->section = section;
               entry->index = g_menu_model_get_n_items(G_MENU_MODEL(section));
               /* A delayed native-menu event must never resolve a recycled
                * action name to a different published definition. */
               g_snprintf(entry->name, sizeof(entry->name), "wb-%lu-%u",
                          entry->generation, ui.menu_actions->len);
               native = g_simple_action_new(entry->name, NULL);
               g_object_set_data_full(G_OBJECT(native), "the-workbench-entry", entry, g_free);
               g_signal_connect(native, "activate", G_CALLBACK(menu_action), entry);
               g_simple_action_set_enabled(native, menu_action_enabled(action));
               g_action_map_add_action(G_ACTION_MAP(ui.application), G_ACTION(native));
               g_ptr_array_add(ui.menu_actions, entry);
               detailed = g_strconcat("app.", entry->name, NULL);
               g_menu_append(section, item_label, detailed);
               g_free(detailed); g_free(item_label); g_object_unref(native);
            }
         }
         if (g_menu_model_get_n_items(G_MENU_MODEL(section)))
            g_menu_append_section(submenu, NULL, G_MENU_MODEL(section));
         g_menu_append_submenu(ui.menu_model, label, G_MENU_MODEL(submenu));
         g_free(label); g_object_unref(section); g_object_unref(submenu);
      }
      ui.menu_revision = the_workbench_revision();
   }
   for (i = 0; i < ui.menu_actions->len; i++)
   {
      GtkMenuAction *entry = g_ptr_array_index(ui.menu_actions, i);
      GAction *action = g_action_map_lookup_action(G_ACTION_MAP(ui.application), entry->name);
      const TheWorkbenchAction *definition = the_workbench_action_find(entry->id);
      const char *reason = the_workbench_action_reason(definition);
      int enabled = menu_action_enabled(definition);
      g_simple_action_set_enabled(G_SIMPLE_ACTION(action), enabled);
      if (strcmp(entry->reason, reason))
      {
         char *label = menu_label(entry->label);
         char *display = enabled ? g_strdup(label) : g_strdup_printf("%s (%s)", label, reason);
         char *detailed = g_strconcat("app.", entry->name, NULL);
         g_menu_remove(entry->section, entry->index);
         g_menu_insert(entry->section, entry->index, display, detailed);
         g_strlcpy(entry->reason, reason, sizeof(entry->reason));
         g_free(label); g_free(display); g_free(detailed);
      }
   }
}

static void native_menu_deactivated(GtkWidget *widget, gpointer data)
{
   (void)data;
   if (ui.native_menu == widget)
   {
      ui.native_menu = NULL;
      finish_menu_focus();
      gtk_widget_destroy(widget);
   }
}

static void open_native_menu(GtkWidget *focus, GdkEventKey *trigger)
{
   sync_menus();
   if (ui.native_menu) return;
   remember_menu_focus(focus);
   ui.menu_canceled = 1;
   ui.native_menu = gtk_menu_new_from_model(G_MENU_MODEL(ui.menu_model));
   gtk_menu_attach_to_widget(GTK_MENU(ui.native_menu), ui.window, NULL);
   g_signal_connect(ui.native_menu, "deactivate", G_CALLBACK(native_menu_deactivated), NULL);
   gtk_widget_show_all(ui.native_menu);
   gtk_menu_popup_at_widget(GTK_MENU(ui.native_menu), ui.editor,
                           GDK_GRAVITY_NORTH_WEST, GDK_GRAVITY_NORTH_WEST, (GdkEvent *)trigger);
   gtk_menu_shell_select_first(GTK_MENU_SHELL(ui.native_menu), TRUE);
}

static void application_activated(GApplication *application, gpointer data)
{
   (void)application; (void)data;
   if (ui.window)
      gtk_window_present(GTK_WINDOW(ui.window));
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
   GtkWidget *root, *toolbar, *scroller, *heading, *controls, *entry;
   GError *application_error = NULL;
   PangoLayout *layout;
   int width, height;
   (void)options;
   if (!gtk_init_check(NULL, NULL))
   {
      g_snprintf(error, capacity, "GTK 3 could not open a display");
      return 0;
   }
   /* GMenu/GAction lets GtkApplication place the same menu in the macOS
    * system menu bar and provide an in-window fallback on other desktops.
    * NON_UNIQUE preserves the editor's existing process and file ownership. */
   g_set_application_name("CRXTHE");
   ui.application = gtk_application_new("org.crxthe.Editor", G_APPLICATION_NON_UNIQUE);
   g_signal_connect(ui.application, "activate", G_CALLBACK(application_activated), NULL);
#ifdef GDK_WINDOWING_QUARTZ
   /* Quartz uses the session delegate to route Dock/system Quit through
    * app.quit. It must reach CANCEL's unsaved-buffer checks, like the menu. */
   g_object_set(ui.application, "register-session", TRUE, NULL);
#endif
   if (!g_application_register(G_APPLICATION(ui.application), NULL, &application_error))
   {
      g_snprintf(error, capacity, "GTK application registration failed: %s",
                    application_error->message);
      g_clear_error(&application_error);
      g_clear_object(&ui.application);
      return 0;
   }
   the_tools_native_initialise();
   ui.window = gtk_application_window_new(ui.application);
   gtk_window_set_title(GTK_WINDOW(ui.window), "CRXTHE — GTK");
   gtk_window_set_default_size(GTK_WINDOW(ui.window), 1120, 820);
   g_signal_connect(ui.window, "delete-event", G_CALLBACK(close_window), NULL);
   g_signal_connect(ui.window, "key-press-event", G_CALLBACK(workbench_key_pressed), NULL);
   g_signal_connect(ui.window, "set-focus", G_CALLBACK(remember_nonlog_focus), NULL);
   root = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
   gtk_container_add(GTK_CONTAINER(ui.window), root);
   {
      /* Quartz Dock/system Quit is an application lifecycle action. */
      GSimpleAction *quit = g_simple_action_new("quit", NULL);
      g_signal_connect_swapped(quit, "activate", G_CALLBACK(queue_command), "cancel");
      g_action_map_add_action(G_ACTION_MAP(ui.application), G_ACTION(quit));
      g_object_unref(quit);
   }
   ui.menu_model = g_menu_new();
   ui.menu_actions = g_ptr_array_new();
   sync_menus();
   gtk_application_set_menubar(ui.application, G_MENU_MODEL(ui.menu_model));
   toolbar = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
   gtk_container_set_border_width(GTK_CONTAINER(toolbar), 6);
   gtk_box_pack_start(GTK_BOX(root), toolbar, FALSE, FALSE, 0);
   ui.first_control = button(toolbar, "Open", G_CALLBACK(file_action), "file.open");
   gtk_widget_set_tooltip_text(ui.first_control, "Open file. Tab visits controls; Esc returns to the editor.");
   button(toolbar, "Save", G_CALLBACK(file_action), "file.save");
   button(toolbar, "Undo", G_CALLBACK(file_action), "edit.undo");
   button(toolbar, "Paste", G_CALLBACK(paste_action), NULL);
   button(toolbar, "Command", G_CALLBACK(command_action), "cursor cmdline");
   button(toolbar, "Next file", G_CALLBACK(command_action), "next");
   ui.run = button(toolbar, "Run Rexx", G_CALLBACK(tool_action), "run");
   gtk_widget_set_tooltip_text(ui.run, "Run the current Rexx file in the Program console.");
   gtk_widget_set_name(ui.run, "the-program-run");
   ui.split = gtk_paned_new(GTK_ORIENTATION_VERTICAL);
   gtk_widget_set_name(ui.split, "the-editor-console-split");
   g_signal_connect(ui.split, "size-allocate", G_CALLBACK(split_allocated), NULL);
   gtk_box_pack_start(GTK_BOX(root), ui.split, TRUE, TRUE, 0);
   ui.editor = gtk_drawing_area_new();
   gtk_widget_set_can_focus(ui.editor, TRUE);
   gtk_widget_set_tooltip_text(ui.editor, "Shift-Tab: console and workbench controls. F6: command/file. F10: menu. Esc: command line.");
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
   ui.auxiliary = gtk_notebook_new();
   ui.logs = g_ptr_array_new_with_free_func(free_log_pane);
   gtk_notebook_set_show_tabs(GTK_NOTEBOOK(ui.auxiliary), FALSE);
   gtk_notebook_set_show_border(GTK_NOTEBOOK(ui.auxiliary), FALSE);
   gtk_notebook_set_scrollable(GTK_NOTEBOOK(ui.auxiliary), TRUE);
   g_signal_connect(ui.auxiliary, "switch-page", G_CALLBACK(auxiliary_switched), NULL);
   gtk_paned_pack2(GTK_PANED(ui.split), ui.auxiliary, FALSE, FALSE);
   ui.console = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
   gtk_widget_set_name(ui.console, "the-program-console");
   gtk_container_set_border_width(GTK_CONTAINER(ui.console), 6);
   gtk_notebook_append_page(GTK_NOTEBOOK(ui.auxiliary), ui.console, gtk_label_new("Program"));
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
   gtk_widget_set_tooltip_text(ui.input, "Program stdin. Enter sends; F6 returns to command/file; F10 opens the menu; Esc returns to the editor.");
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
   TheDriverWindow *window = track(the_headless_driver_ops.create_window(rows, cols, row, col));
   if (window && ui.menu_focus && in_macro && !ui.menu_popup)
      ui.menu_popup = window;
   return window;
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
   if (window == ui.menu_popup)
      finish_menu_focus();
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
   sync_logs();
   sync_menus();
   redraw();
}

static int read_input(TheInputEvent *event)
{
   gint64 deadline = ui.timeout < 0 ? G_MAXINT64
                                    : g_get_monotonic_time() + ui.timeout * 1000;
   *event = the_input_event_none();
   if (!in_macro && !the_panel_active && the_tools_take_menu_run(event)) return 1;
   while (ui.active)
   {
      if (ui.menu_focus && !ui.menu_popup && !ui.native_menu && !in_macro && g_queue_is_empty(&ui.inputs))
         finish_menu_focus();
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
      sync_logs();
      sync_menus();
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
   if (ui.console_css)
   {
      gtk_style_context_remove_provider_for_screen(gtk_widget_get_screen(ui.window),
                                                    GTK_STYLE_PROVIDER(ui.console_css));
      g_object_unref(ui.console_css);
      ui.console_css = NULL;
   }
   if (ui.native_menu) gtk_menu_shell_deactivate(GTK_MENU_SHELL(ui.native_menu));
   g_clear_object(&ui.menu_focus);
   ui.menu_popup = NULL;
   g_clear_object(&ui.log_origin);
   g_clear_object(&ui.last_nonlog_focus);
   if (ui.window) gtk_widget_destroy(ui.window);
   if (ui.logs) g_ptr_array_unref(ui.logs);
   ui.logs = NULL;
   ui.window = ui.editor = ui.console = NULL;
   g_clear_object(&ui.menu_model);
   if (ui.menu_actions) g_ptr_array_unref(ui.menu_actions);
   ui.menu_actions = NULL;
   if (ui.im) g_object_unref(ui.im);
   ui.im = NULL;
   if (ui.application)
   {
      /* The editor owns the event loop while its windows exist. With the
       * final window destroyed, run() returns immediately and completes
       * GtkApplication's shutdown, native-menu and D-Bus cleanup. */
      g_application_run(G_APPLICATION(ui.application), 0, NULL);
      g_clear_object(&ui.application);
   }
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
