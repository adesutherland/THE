/* Compile the production driver into a test-only module so this probe can
 * exercise its real callbacks without a production automation endpoint. */
#define the_driver_module_ops gtk_probe_base_operations
#include "../src/drivers/gtk/gtkdriver.c"
#undef the_driver_module_ops

#include <assert.h>
#include <stdio.h>
#include "editorsource.h"

static unsigned phase;
static unsigned preflight;
static int dirty_quit_checked;
static unsigned workbench_focus_phase;
static unsigned log_probe_phase;
static int log_origin_window;
static LogicalCursor log_origin_cursor;
static char *log_input_draft;
static int workbench_menu_seen;
static int workbench_editor_window;
static LogicalCursor workbench_editor_cursor;
static char sample[4096], other[4096], program[4096];
static const char expected[] = "alpha\nbe\303\251ta\n";
static unsigned palette_job, palette_source, palette_revision;
static char *palette_output;
static GtkTextBuffer *palette_buffer;
static const struct {
   const char *name, *definition;
   unsigned foreground, background;
} palette[] = {
   {"consolebody", "#c1d2e3 on #112233 bold", 0xc1d2e3, 0x112233},
   {"consoleheader", "#f1dfc0 on #283746 bold", 0xf1dfc0, 0x283746},
   {"consoleinput", "#8adaf0 on #173143 underline", 0x8adaf0, 0x173143},
   {"consolecontrol", "#d2e3f4 on #354657", 0xd2e3f4, 0x354657},
   {"consoleselected", "#eac78b on #172435 bold reverse", 0x172435, 0xeac78b},
   {"consoledivider", "#7dd5cf on #0c4b55", 0x7dd5cf, 0x0c4b55},
   {"consoleerror", "#ffb1a3 on #6a2531 bold italic", 0xffb1a3, 0x6a2531},
   {"consolestatus", "#caffc0 on #23452f bold", 0xcaffc0, 0x23452f}
};

static void assert_source(const char *text)
{
   const TheEditorSource *source = the_editor_source_prepare();
   if (!source || source->length != strlen(text)
   || memcmp(source->bytes, text, source->length))
      fprintf(stderr, "GTK probe phase %u source: expected [%s] got [%.*s] (%lu bytes)\n",
              phase, text, source ? (int)source->length : 0,
              source ? source->bytes : "", source ? (unsigned long)source->length : 0);
   assert(source && source->length == strlen(text));
   assert(!memcmp(source->bytes, text, source->length));
}

static void open_file(const char *path)
{
   TheInputEvent event;
   assert(the_input_event_from_action("file.open", path, &event));
   assert(queue_event(event));
}

static void assert_saved(void)
{
   char *bytes = NULL;
   gsize length;
   assert(g_file_get_contents(sample, &bytes, &length, NULL));
   assert(length == sizeof(expected) - 1);
   assert(!memcmp(bytes, expected, length));
   g_free(bytes);
   assert(CURRENT_FILE->save_alt == 0);
}

static char *console_text(void)
{
   GtkTextBuffer *buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(ui.output));
   GtkTextIter first, last;
   gtk_text_buffer_get_bounds(buffer, &first, &last);
   return gtk_text_buffer_get_text(buffer, &first, &last, FALSE);
}

static void await_console(int finished)
{
   gint64 deadline = g_get_monotonic_time() + 15000000;
   while (g_get_monotonic_time() < deadline)
   {
      const TheToolConsole *console;
      char *text;
      int ready;
      the_tools_native_poll();
      sync_console();
      console = the_tool_console();
      text = console_text();
      ready = finished ? !console->running && strstr(text, "Hello ADA") != NULL
                       : console->running && strstr(text, "GTK program ready") != NULL;
      g_free(text);
      if (ready)
         return;
      while (g_main_context_pending(NULL))
         g_main_context_iteration(NULL, FALSE);
      g_usleep(5000);
   }
   fprintf(stderr, "GTK console probe timed out: %s\n%s\n",
           the_tool_console()->state, console_text());
   assert(0);
}

static void frame_painted(GdkFrameClock *clock, gpointer flag)
{
   (void)clock;
   *(int *)flag = 1;
}

static void await_frame(void)
{
   int painted = 0;
   gint64 deadline;
   GdkFrameClock *clock;
   gulong handler;
   /* GtkPaned's new axis and divider position can queue a layout for the
    * frame clock. Capture only after its layout/paint phases have finished. */
   clock = gtk_widget_get_frame_clock(ui.window);
   handler = g_signal_connect(clock, "after-paint", G_CALLBACK(frame_painted), &painted);
   gtk_widget_queue_draw(ui.window);
   deadline = g_get_monotonic_time() + 5000000;
   while (!painted && g_get_monotonic_time() < deadline)
   {
      while (g_main_context_pending(NULL))
         g_main_context_iteration(NULL, FALSE);
      g_usleep(5000);
   }
   g_signal_handler_disconnect(clock, handler);
   assert(painted);
}

static void screenshot(const char *suffix)
{
   const char *path = getenv("THE_GTK_PROBE_SCREENSHOT");
   cairo_surface_t *surface;
   cairo_t *context;
   GtkAllocation allocation;
   char *output;
   if (!path || !*path)
      return;
   await_frame();
   assert(ui.rows == terminal_lines && ui.cols == terminal_cols);
   gtk_widget_get_allocation(ui.window, &allocation);
   output = suffix ? g_strdup_printf("%.*s-%s.png",
      (int)(g_str_has_suffix(path, ".png") ? strlen(path) - 4 : strlen(path)), path, suffix)
                   : g_strdup(path);
   surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32,
                                          allocation.width, allocation.height);
   context = cairo_create(surface);
   gtk_widget_draw(ui.window, context);
   assert(cairo_surface_write_to_png(surface, output) == CAIRO_STATUS_SUCCESS);
   g_free(output);
   cairo_destroy(context);
   cairo_surface_destroy(surface);
}

static void bounds_in_split(GtkWidget *widget, GtkAllocation *bounds)
{
   int border = widget == ui.console
                  ? gtk_container_get_border_width(GTK_CONTAINER(widget)) : 0;
   gtk_widget_get_allocation(widget, bounds);
   assert(gtk_widget_translate_coordinates(widget, ui.split, 0, 0,
                                             &bounds->x, &bounds->y));
   bounds->x -= border;
   bounds->y -= border;
   bounds->width += 2 * border;
   bounds->height += 2 * border;
}

static void resize_window(int width, GtkOrientation orientation)
{
   gint64 deadline = g_get_monotonic_time() + 5000000;
   GtkAllocation window, editor, console;
   gtk_window_resize(GTK_WINDOW(ui.window), width, 820);
   while (g_get_monotonic_time() < deadline)
   {
      while (g_main_context_pending(NULL))
         g_main_context_iteration(NULL, FALSE);
      gtk_widget_get_allocation(ui.window, &window);
      bounds_in_split(ui.editor, &editor);
      bounds_in_split(ui.console, &console);
      if (window.width == width
      && gtk_orientable_get_orientation(GTK_ORIENTABLE(ui.split)) == orientation
      && (orientation == GTK_ORIENTATION_HORIZONTAL ? console.x >= editor.x + editor.width
                                                    : console.y >= editor.y + editor.height))
      {
         await_frame();
         return;
      }
      g_usleep(5000);
   }
   fprintf(stderr, "GTK resize phase %u expected width %d orientation %d; got window %dx%d orientation %d, editor %d,%d %dx%d, console %d,%d %dx%d\n",
      phase, width, orientation, window.width, window.height,
      gtk_orientable_get_orientation(GTK_ORIENTABLE(ui.split)),
      editor.x, editor.y, editor.width, editor.height,
      console.x, console.y, console.width, console.height);
   assert(0 && "GTK responsive layout did not settle");
}

static void assert_layout(GtkOrientation orientation)
{
   GtkAllocation editor, console;
   bounds_in_split(ui.editor, &editor);
   bounds_in_split(ui.console, &console);
   assert(gtk_orientable_get_orientation(GTK_ORIENTABLE(ui.split)) == orientation);
   if (orientation == GTK_ORIENTATION_HORIZONTAL)
   {
      assert(console.x >= editor.x + editor.width);
      assert(console.y == editor.y && console.height == editor.height);
   }
   else
   {
      assert(console.y >= editor.y + editor.height);
      assert(console.x == editor.x && console.width == editor.width);
   }
   assert_source("say 'GTK program ready'\npull name\nsay 'Hello' name\n");
   assert(!the_editor_buffer_dirty(CURRENT_FILE));
   assert(CURRENT_VIEW->current_window == WINDOW_COMMAND);
}

static void assert_rgb(const GdkRGBA *actual, unsigned expected)
{
   assert(ABS(actual->red - ((expected >> 16) & 255) / 255.0) < .001);
   assert(ABS(actual->green - ((expected >> 8) & 255) / 255.0) < .001);
   assert(ABS(actual->blue - (expected & 255) / 255.0) < .001);
   assert(actual->alpha > .999);
}

static void assert_widget_palette(GtkWidget *widget, GtkStateFlags state, int index,
                                   int custom)
{
   GdkRGBA foreground, expected_fg, expected_bg, *background = NULL;
   GtkStyleContext *context = gtk_widget_get_style_context(widget);
   console_colours(set_colour(CURRENT_FILE->attr + ATTR_CONSOLEBODY + index),
                       &expected_fg, &expected_bg);
   gtk_style_context_get_color(context, state, &foreground);
   gtk_style_context_get(context, state, "background-color", &background, NULL);
   assert(background);
   if (custom)
   {
      assert_rgb(&foreground, palette[index].foreground);
      assert_rgb(background, palette[index].background);
   }
   else
   {
      assert(ABS(foreground.red - expected_fg.red) < .001);
      assert(ABS(foreground.green - expected_fg.green) < .001);
      assert(ABS(foreground.blue - expected_fg.blue) < .001);
      assert(ABS(background->red - expected_bg.red) < .001);
      assert(ABS(background->green - expected_bg.green) < .001);
      assert(ABS(background->blue - expected_bg.blue) < .001);
   }
   gdk_rgba_free(background);
}

static unsigned rgba_rgb(const GdkRGBA *colour)
{
   return ((unsigned)(colour->red * 255 + .5) << 16)
        | ((unsigned)(colour->green * 255 + .5) << 8)
        | (unsigned)(colour->blue * 255 + .5);
}

static void assert_default_palette(void)
{
   static const unsigned foreground[] = {
      0xd4d4d4, 0xf1f5fa, 0xd4d4d4, 0xd4d4d4,
      0xe6f7ff, 0x6c7b90, 0xff6347, 0xd4d4d4
   };
   static const unsigned background[] = {
      0x1e1e1e, 0x25415f, 0x2d2d30, 0x1e1e1e,
      0x264f78, 0x1e1e1e, 0x1e1e1e, 0x1e1e1e
   };
   unsigned i;
   await_frame();
   assert(colour_support);
   for (i = 0; i < G_N_ELEMENTS(foreground); i++)
   {
      GdkRGBA fg, bg;
      TheDriverAttr attr = set_colour(CURRENT_FILE->attr + ATTR_CONSOLEBODY + i);
      assert(!(the_render_attr_style(attr) & THE_STYLE_REVERSE));
      console_colours(attr, &fg, &bg);
      assert_rgb(&fg, foreground[i]);
      assert_rgb(&bg, background[i]);
   }
   assert_widget_palette(ui.output, GTK_STATE_FLAG_NORMAL, 0, 0);
   assert_widget_palette(ui.header, GTK_STATE_FLAG_NORMAL, 1, 0);
   assert_widget_palette(ui.state, GTK_STATE_FLAG_NORMAL, 7, 0);
}

static void count_menubars(GtkWidget *widget, gpointer data)
{
   unsigned *count = data;
   if (GTK_IS_MENU_BAR(widget))
      ++*count;
   if (GTK_IS_CONTAINER(widget))
      gtk_container_forall(GTK_CONTAINER(widget), count_menubars, data);
}

static void activate_shared_action(const char *id)
{
   unsigned i;
   sync_menus();
   for (i = 0; i < ui.menu_actions->len; i++)
   {
      GtkMenuAction *entry = g_ptr_array_index(ui.menu_actions, i);
      if (!strcmp(entry->id, id))
      {
         g_action_group_activate_action(G_ACTION_GROUP(ui.application), entry->name, NULL);
         return;
      }
   }
   assert(0);
}

static void assert_application_menus(void)
{
   GMenuModel *bar;
   TheInputEvent queued;
   TheWorkbenchMenu menu = {0};
   TheWorkbenchItem item = {0};
   TheWorkbenchAction action = {0};
   unsigned i, menubars = 0;
   unsigned long generation;
   char previous_name[32] = "";
   gboolean remote = FALSE;
   assert(GTK_IS_APPLICATION_WINDOW(ui.window));
   assert(g_application_get_is_registered(G_APPLICATION(ui.application)));
   assert(gtk_window_get_application(GTK_WINDOW(ui.window)) == ui.application);
   sync_menus();
   bar = gtk_application_get_menubar(ui.application);
   assert(bar && g_menu_model_get_n_items(bar) == the_workbench_menu_count());
   for (i = 0; i < the_workbench_menu_count(); i++)
   {
      char *label = NULL;
      GMenuModel *items = g_menu_model_get_item_link(bar, i, G_MENU_LINK_SUBMENU);
      assert(g_menu_model_get_item_attribute(bar, i, G_MENU_ATTRIBUTE_LABEL, "s", &label));
      assert(items && g_menu_model_get_n_items(items) > 0);
      g_free(label); g_object_unref(items);
   }
   g_object_get(gtk_settings_get_default(), "gtk-shell-shows-menubar", &remote, NULL);
#ifdef GDK_WINDOWING_QUARTZ
   assert(remote);
#endif
   count_menubars(ui.window, &menubars);
   assert(menubars == (remote ? 0 : 1));
   strcpy(menu.id, "gtk.probe.menu"); strcpy(menu.label, "Tools / \"quoted\" Ω"); menu.order = 50;
   strcpy(action.id, "gtk.probe.action"); strcpy(action.label, "Declare / Ω");
   action.kind = THE_WORKBENCH_ACTION_COMMAND; strcpy(action.operand, "msg GTK_DECLARED_ONLY");
   strcpy(item.id, "gtk.probe.item"); strcpy(item.menu, menu.id);
   strcpy(item.action, action.id); strcpy(item.label, "Check / \"quoted\" Ω");
   assert(!the_workbench_begin("gtk.probe"));
   assert(!the_workbench_put_menu(&menu)); assert(!the_workbench_put_action(&action));
   assert(!the_workbench_put_item(&item)); assert(!the_workbench_commit());
   assert(g_queue_is_empty(&ui.inputs));
   sync_menus();
   assert(g_menu_model_get_n_items(bar) == the_workbench_menu_count());
   {
      char *label = NULL;
      assert(g_menu_model_get_item_attribute(bar, the_workbench_menu_count() - 1,
                                           G_MENU_ATTRIBUTE_LABEL, "s", &label));
      assert(!strcmp(label, menu.label)); g_free(label);
   }
   generation = the_workbench_action_find(action.id)->generation;
   for (i = 0; i < ui.menu_actions->len; i++)
   {
      GtkMenuAction *entry = g_ptr_array_index(ui.menu_actions, i);
      if (!strcmp(entry->id, action.id))
      {
         strcpy(previous_name, entry->name);
         g_action_group_activate_action(G_ACTION_GROUP(ui.application), entry->name, NULL);
      }
   }
   assert(pop_input(&queued)); assert(queued.kind == THE_INPUT_WORKBENCH_INVOKE);
   assert(!strcmp(the_input_event_workbench_id(&queued), action.id));
   assert(the_input_event_workbench_generation(&queued) == generation);
   assert(!the_input_event_workbench_argument(&queued));
   assert(g_queue_is_empty(&ui.inputs));
   assert(!the_workbench_begin("gtk.probe"));
   strcpy(item.label, "Replaced / Ω"); assert(!the_workbench_put_item(&item));
   strcpy(action.operand, "msg GTK_REPLACED_ONLY"); assert(!the_workbench_put_action(&action));
   assert(!the_workbench_commit()); sync_menus();
   assert(!g_action_group_has_action(G_ACTION_GROUP(ui.application), previous_name));
   assert(the_workbench_action_find(action.id)->generation != generation);
   assert(the_input_event_workbench_generation(&queued) == generation);
   assert(!the_workbench_begin("gtk.probe"));
   assert(!the_workbench_remove_menu(menu.id)); assert(!the_workbench_remove_action(action.id));
   assert(!the_workbench_commit()); sync_menus();
   assert(g_menu_model_get_n_items(bar) == the_workbench_menu_count());
   g_action_group_activate_action(G_ACTION_GROUP(ui.application), "quit", NULL);
   assert(pop_input(&queued));
   assert(queued.kind == THE_INPUT_COMMAND && !strcmp(queued.command, "cancel"));
   assert(g_queue_is_empty(&ui.inputs));
   puts("GTK_APPLICATION_NATIVE_MENUS_SAFE_QUEUED_ACTIONS_PASS");
   puts("GTK_SHARED_MENU_UTF8_LABEL_REVISION_ID_GENERATION_PASS");
}

static void assert_profile_guide(void)
{
   TheDriverWindow *file = driver_current_role_window(WINDOW_FILEAREA);
   TheDriverWindowSize size = operations.window_size(file);
   TheDriverWindowOrigin origin = operations.window_origin(file);
   int row, col, found = 0;
   assert(size.valid && origin.valid);
   for (row = 0; row < size.rows; row++)
      if (CURRENT_SCREEN.sl[row].line_type == LINE_RESERVED)
      {
         char *text = g_malloc(size.cols + 1);
         for (col = 0; col < size.cols; col++)
         {
            TheRenderCell cell;
            assert(headless_driver_render_cell_at(file, row, col, &cell));
            text[col] = cell.codepoint_count && cell.codepoints[0] < 128
                           ? (char)cell.codepoints[0] : ' ';
         }
         text[col] = 0;
         if (strstr(text, "F10 Menu") && strstr(text, "Ctrl-S Save"))
         {
            assert(origin.row + row < ui.rows);
            found = 1;
         }
         g_free(text);
      }
   assert(found);
}

static void press_workbench_key(guint keyval, GdkModifierType state)
{
   GdkEvent *event = gdk_event_new(GDK_KEY_PRESS);
   GdkEventKey *key = &event->key;
   gboolean handled = FALSE;
   key->window = g_object_ref(gtk_widget_get_window(ui.window));
   key->keyval = keyval;
   key->state = state;
   gdk_event_set_device(event, gdk_seat_get_keyboard(gdk_display_get_default_seat(
      gtk_widget_get_display(ui.window))));
   g_signal_emit_by_name(ui.window, "key-press-event", key, &handled);
   gdk_event_free(event);
   assert(handled);
}

static void assert_profile_menu(void)
{
   assert(ui.native_menu && GTK_IS_MENU(ui.native_menu));
   assert(!in_macro);
   assert(ui.menu_revision == the_workbench_revision());
   ++workbench_menu_seen;
   screenshot("workbench-menu");
}

static void assert_workbench_focus_cycle(void)
{
   LogicalCursorState cursor = CURRENT_VIEW->logical_cursor;
   LINETYPE focus_line = CURRENT_VIEW->focus_line;
   int current_window = CURRENT_VIEW->current_window;
   char *draft = g_strdup(gtk_entry_get_text(GTK_ENTRY(ui.input)));
   TheInputEvent tab, expected_tab;
   GdkEventKey key = {0};
   gtk_entry_set_text(GTK_ENTRY(ui.input), "Workbench unsent draft");
   gtk_widget_grab_focus(ui.editor);
   press_workbench_key(GDK_KEY_ISO_Left_Tab, GDK_SHIFT_MASK);
   assert(gtk_window_get_focus(GTK_WINDOW(ui.window)) ==
      (gtk_widget_get_sensitive(ui.input) ? ui.input : ui.output));
   press_workbench_key(GDK_KEY_ISO_Left_Tab, GDK_SHIFT_MASK);
   assert(gtk_window_get_focus(GTK_WINDOW(ui.window)) == ui.first_control);
   press_workbench_key(GDK_KEY_ISO_Left_Tab, GDK_SHIFT_MASK);
   assert(gtk_window_get_focus(GTK_WINDOW(ui.window)) == ui.editor);
   gtk_widget_grab_focus(gtk_widget_get_sensitive(ui.input) ? ui.input : ui.output);
   press_workbench_key(GDK_KEY_Escape, 0);
   assert(gtk_window_get_focus(GTK_WINDOW(ui.window)) == ui.editor);
   assert(!strcmp(gtk_entry_get_text(GTK_ENTRY(ui.input)), "Workbench unsent draft"));
   gtk_widget_hide(ui.console);
   press_workbench_key(GDK_KEY_ISO_Left_Tab, GDK_SHIFT_MASK);
   assert(gtk_window_get_focus(GTK_WINDOW(ui.window)) == ui.first_control);
   press_workbench_key(GDK_KEY_Escape, 0);
   assert(gtk_window_get_focus(GTK_WINDOW(ui.window)) == ui.editor);
   gtk_widget_show(ui.console);
   assert(g_queue_is_empty(&ui.inputs));
   key.keyval = GDK_KEY_Tab;
   assert(key_pressed(ui.editor, &key, NULL));
   assert(pop_input(&tab));
   assert(the_input_event_from_key_name("tab", &expected_tab));
   assert(tab.kind == expected_tab.kind && tab.key_code == expected_tab.key_code);
   assert(g_queue_is_empty(&ui.inputs));
   assert(!memcmp(&cursor, &CURRENT_VIEW->logical_cursor, sizeof(cursor)));
   assert(CURRENT_VIEW->focus_line == focus_line);
   assert(CURRENT_VIEW->current_window == current_window);
   assert(!strcmp(gtk_button_get_label(GTK_BUTTON(ui.run)), "Run Rexx"));
   gtk_entry_set_text(GTK_ENTRY(ui.input), draft);
   g_free(draft);
}

static void click_noneditable_row(int reserved)
{
   GtkWidget *focus = gtk_window_get_focus(GTK_WINDOW(ui.window));
   TheDriverWindow *status = driver_global_window(THE_DRIVER_GLOBAL_STATAREA);
   TheDriverWindowOrigin origin = operations.window_origin(status);
   TheDriverWindow *file = driver_current_role_window(WINDOW_FILEAREA);
   GdkEventButton hit = {0};
   int row;
   assert(status && origin.valid);
   hit.button = 1;
   row = 0;
   if (reserved)
   {
      origin = operations.window_origin(file);
      for (; row < CURRENT_SCREEN.rows[WINDOW_FILEAREA]; row++)
         if (CURRENT_SCREEN.sl[row].line_type == LINE_RESERVED)
            break;
      assert(row < CURRENT_SCREEN.rows[WINDOW_FILEAREA]);
   }
   hit.x = (origin.col + 3) * ui.cell_width + 1;
   hit.y = (origin.row + row) * ui.cell_height + 1;
   clicked(ui.editor, &hit, NULL);
   assert(g_queue_is_empty(&ui.inputs));
   assert(gtk_window_get_focus(GTK_WINDOW(ui.window)) == focus);
}

static void assert_original_saved(void)
{
   char *bytes = NULL;
   gsize length;
   const char original[] = "alpha\nbeta\n";
   assert(g_file_get_contents(sample, &bytes, &length, NULL));
   assert(length == sizeof(original) - 1);
   assert(!memcmp(bytes, original, length));
   g_free(bytes);
}

static void assert_program_input_keeps_focus(void)
{
   const char source[] = "say 'GTK program ready'\npull name\nsay 'Hello' name\n";
   LINETYPE focus_line = CURRENT_VIEW->focus_line;
   int current_window = CURRENT_VIEW->current_window;
   int cell_column = CURRENT_VIEW->logical_cursor.current.text.cell_column;
   GdkEventKey key = {0};
   char *saved = NULL;
   gsize length;
   assert(gtk_widget_get_sensitive(ui.input));
   gtk_entry_set_text(GTK_ENTRY(ui.input), "Program draft");
   gtk_widget_grab_focus(ui.input);
   gtk_editable_set_position(GTK_EDITABLE(ui.input), -1);
   assert(gtk_window_get_focus(GTK_WINDOW(ui.window)) == ui.input);
   click_noneditable_row(0);
   assert(gtk_window_get_focus(GTK_WINDOW(ui.window)) == ui.input);
   key.type = GDK_KEY_PRESS;
   key.window = gtk_widget_get_window(ui.input);
   key.keyval = GDK_KEY_x;
   key.string = "x";
   key.length = 1;
   assert(gtk_window_propagate_key_event(GTK_WINDOW(ui.window), &key));
   assert(!strcmp(gtk_entry_get_text(GTK_ENTRY(ui.input)), "Program draftx"));
   click_noneditable_row(1);
   assert(gtk_window_get_focus(GTK_WINDOW(ui.window)) == ui.input);
   key.keyval = GDK_KEY_y;
   key.string = "y";
   assert(gtk_window_propagate_key_event(GTK_WINDOW(ui.window), &key));
   assert(!strcmp(gtk_entry_get_text(GTK_ENTRY(ui.input)), "Program draftxy"));
   assert_source(source);
   assert(!the_editor_buffer_dirty(CURRENT_FILE));
   assert(CURRENT_VIEW->focus_line == focus_line);
   assert(CURRENT_VIEW->current_window == current_window);
   assert(CURRENT_VIEW->logical_cursor.current.text.cell_column == cell_column);
   assert(g_queue_is_empty(&ui.inputs));
   assert(g_file_get_contents(program, &saved, &length, NULL));
   assert(length == sizeof(source) - 1 && !memcmp(saved, source, length));
   g_free(saved);
   screenshot("console-input-focus");
   puts("GTK_PROGRAM_INPUT_FOCUS_STATUS_RESERVED_CLICKS_PASS");
   gtk_entry_set_text(GTK_ENTRY(ui.input), "");
   gtk_widget_grab_focus(ui.editor);
}

static void assert_divider_palette(void)
{
   GtkAllocation allocation, editor, console;
   GdkRGBA fg, bg;
   cairo_surface_t *surface;
   cairo_t *cr;
   unsigned char *pixels;
   int stride, x, y, found_fg = 0, found_bg = 0;
   int horizontal = gtk_orientable_get_orientation(GTK_ORIENTABLE(ui.split))
                      == GTK_ORIENTATION_HORIZONTAL;
   console_colours(set_colour(CURRENT_FILE->attr + ATTR_CONSOLEDIVIDER), &fg, &bg);
   gtk_widget_get_allocation(ui.split, &allocation);
   bounds_in_split(ui.editor, &editor);
   bounds_in_split(ui.console, &console);
   surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, allocation.width, allocation.height);
   cr = cairo_create(surface);
   gtk_widget_draw(ui.split, cr);
   cairo_surface_flush(surface);
   pixels = cairo_image_surface_get_data(surface);
   stride = cairo_image_surface_get_stride(surface);
   for (y = horizontal ? allocation.height / 2 : editor.y + editor.height;
        y < (horizontal ? allocation.height / 2 + 1 : console.y); y++)
      for (x = horizontal ? editor.x + editor.width : allocation.width / 2;
           x < (horizontal ? console.x : allocation.width / 2 + 1); x++)
      {
         unsigned pixel = *(uint32_t *)(pixels + y * stride + x * 4) & 0xffffff;
         found_fg |= pixel == rgba_rgb(&fg);
         found_bg |= pixel == rgba_rgb(&bg);
      }
   assert(found_fg && found_bg);
   cairo_destroy(cr);
   cairo_surface_destroy(surface);
}

static void assert_palette(int custom)
{
   GtkTextTag *error_tag;
   GdkRGBA *fg = NULL, *bg = NULL;
   int weight;
   PangoStyle style;
   await_frame();
   assert(ui.console_colours_valid);
   assert_widget_palette(ui.output, GTK_STATE_FLAG_NORMAL, 0, custom);
   assert_widget_palette(ui.header, GTK_STATE_FLAG_NORMAL, 1, custom);
   assert_widget_palette(ui.input, GTK_STATE_FLAG_NORMAL, 2, custom);
   assert_widget_palette(ui.send, GTK_STATE_FLAG_NORMAL, 3, custom);
   assert_widget_palette(ui.send, GTK_STATE_FLAG_PRELIGHT, 4, custom);
   assert_widget_palette(ui.state, GTK_STATE_FLAG_NORMAL, 7, custom);
   assert_divider_palette();
   error_tag = gtk_text_tag_table_lookup(gtk_text_buffer_get_tag_table(palette_buffer), "stderr");
   g_object_get(error_tag, "foreground-rgba", &fg, "background-rgba", &bg,
                   "weight", &weight, "style", &style, NULL);
   assert(fg && bg);
   if (custom)
   {
      assert_rgb(fg, palette[6].foreground);
      assert_rgb(bg, palette[6].background);
      assert(weight == PANGO_WEIGHT_BOLD && style == PANGO_STYLE_ITALIC);
   }
   else
   {
      GdkRGBA expected_fg, expected_bg;
      console_colours(set_colour(CURRENT_FILE->attr + ATTR_CONSOLEERROR),
                        &expected_fg, &expected_bg);
      assert(rgba_rgb(fg) == rgba_rgb(&expected_fg));
      assert(rgba_rgb(bg) == rgba_rgb(&expected_bg));
   }
   gdk_rgba_free(fg);
   gdk_rgba_free(bg);
}

static void assert_palette_preserves_job(void)
{
   GtkTextIter first, last;
   char *text = console_text();
   const TheToolConsole *console = the_tool_console();
   assert(console->id == palette_job && console->running);
   assert(console->source_id == palette_source && console->revision == palette_revision);
   assert(!strcmp(text, palette_output));
   g_free(text);
   assert(gtk_text_view_get_buffer(GTK_TEXT_VIEW(ui.output)) == palette_buffer);
   assert(gtk_text_buffer_get_selection_bounds(palette_buffer, &first, &last));
   assert(gtk_text_iter_get_offset(&first) == 0 && gtk_text_iter_get_offset(&last) == 5);
   assert(!strcmp(gtk_entry_get_text(GTK_ENTRY(ui.input)), "unsent draft"));
}

static void change_palette(void)
{
   GtkTextIter first, last;
   const TheToolConsole *console = the_tool_console();
   size_t i;
   palette_job = console->id;
   palette_source = console->source_id;
   palette_revision = console->revision;
   the_tool_output(console->id, 2, "GTK stderr colour probe\n", 24);
   sync_console();
   palette_output = console_text();
   palette_buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(ui.output));
   gtk_text_buffer_get_iter_at_offset(palette_buffer, &first, 0);
   gtk_text_buffer_get_iter_at_offset(palette_buffer, &last, 5);
   gtk_text_buffer_select_range(palette_buffer, &last, &first);
   gtk_entry_set_text(GTK_ENTRY(ui.input), "unsent draft");
   for (i = 0; i < G_N_ELEMENTS(palette); i++)
   {
      char command[128];
      g_snprintf(command, sizeof(command), "set color %s %s", palette[i].name,
                     palette[i].definition);
      queue_command(command);
   }
}

static void replace_focused_log(int place)
{
   const TheWorkbenchPanel *panel = the_workbench_panel_find("gtk.logone");
   TheWorkbenchPanel replacement = *panel;
   const TheWorkbenchLogInfo *info = the_workbench_log_find(panel->model);
   size_t retained_bytes = info->bytes;
   assert(the_workbench_panel_state(panel->id)->focused);
   assert(!the_workbench_begin("gtk.logs"));
   if (place)
      assert(!the_workbench_place_panel(panel->id, replacement.placement,
         replacement.fallbacks, replacement.fallback_count, replacement.mincols));
   else
   {
      strcpy(replacement.title, "Replacement / \"log\" Ω");
      assert(!the_workbench_put_panel(&replacement));
   }
   assert(!the_workbench_commit());
   panel = the_workbench_panel_find(replacement.id);
   assert(panel->generation != replacement.generation);
   assert(panel->model_generation == replacement.model_generation);
   assert(the_workbench_log_find(panel->model)->bytes == retained_bytes);
   assert(the_workbench_panel_state(panel->id)->visible);
   assert(!the_workbench_panel_state(panel->id)->focused);
   sync_logs(); await_frame();
   assert(!ui.focused_log[0]);
   assert(!find_log(panel->id, replacement.generation));
   assert(find_log(panel->id, panel->generation));
}

static void assert_log_program_draft(void)
{
   int first, last;
   assert(gtk_window_get_focus(GTK_WINDOW(ui.window)) == ui.input);
   assert(!strcmp(gtk_entry_get_text(GTK_ENTRY(ui.input)), "LOG unsent draft"));
   assert(gtk_editable_get_selection_bounds(GTK_EDITABLE(ui.input), &first, &last));
   assert(first == 3 && last == 11 && gtk_editable_get_position(GTK_EDITABLE(ui.input)) == 3);
}

static void named_log_step(void)
{
   const TheWorkbenchPanel *panel;
   GtkLogPane *pane;
   GtkTextBuffer *buffer;
   GtkTextIter first, last;
   char *text;
   switch (log_probe_phase++)
   {
      case 0:
      {
         TheWorkbenchPanel value = {0};
         log_origin_window = CURRENT_VIEW->current_window;
         log_origin_cursor = CURRENT_VIEW->logical_cursor.current;
         log_input_draft = g_strdup(gtk_entry_get_text(GTK_ENTRY(ui.input)));
         gtk_entry_set_text(GTK_ENTRY(ui.input), "LOG unsent draft");
         gtk_widget_grab_focus(ui.input);
         gtk_editable_select_region(GTK_EDITABLE(ui.input), 11, 3);
         the_tool_console_visibility(1, 1);
         assert(!the_workbench_begin("gtk.logs"));
         strcpy(value.id, "gtk.logone"); strcpy(value.title, "Named / \"log\" Ω");
         strcpy(value.model, "gtk.stdout"); value.kind = THE_WORKBENCH_PANEL_LOG;
         assert(!the_workbench_put_panel(&value));
         strcpy(value.id, "gtk.logtwo"); strcpy(value.title, "Second log"); strcpy(value.model, "gtk.stderr");
         assert(!the_workbench_put_panel(&value)); assert(!the_workbench_commit());
         panel = the_workbench_panel_find("gtk.logone");
         {
            GString *lines = g_string_new(NULL);
            unsigned i;
            for (i = 1; i <= 80; i++) g_string_append_printf(lines, "LOG-LINE %u\n", i);
            g_string_append_len(lines, "NUL:\0 ESC:\033 C1:\302\233 Ω\n", sizeof("NUL:\0 ESC:\033 C1:\302\233 Ω\n") - 1);
            assert(the_workbench_log_append(panel->model, panel->model_generation, 1001, 1, 1,
                                            lines->str, lines->len) == THE_WORKBENCH_LOG_OK);
            g_string_free(lines, TRUE);
         }
         sync_logs(); pane = find_log(panel->id, panel->generation); assert(pane);
         assert(queue_panel(pane, THE_WORKBENCH_PANEL_SHOW));
         break;
      }
      case 1:
         panel = the_workbench_panel_find("gtk.logtwo"); sync_logs();
         assert(gtk_window_get_focus(GTK_WINDOW(ui.window)) == ui.input);
         assert(queue_panel(find_log(panel->id, panel->generation), THE_WORKBENCH_PANEL_SHOW));
         break;
      case 2:
         panel = the_workbench_panel_find("gtk.logone"); pane = find_log(panel->id, panel->generation);
         assert(gtk_window_get_focus(GTK_WINDOW(ui.window)) == ui.input);
         assert(gtk_notebook_get_nth_page(GTK_NOTEBOOK(ui.auxiliary),
            gtk_notebook_get_current_page(GTK_NOTEBOOK(ui.auxiliary))) == ui.console);
         resize_window(1366, GTK_ORIENTATION_HORIZONTAL);
         assert(queue_panel(pane, THE_WORKBENCH_PANEL_FOCUS));
         break;
      case 3:
      {
         GtkAdjustment *adjustment;
         double scroll;
         int a, b;
         panel = the_workbench_panel_find("gtk.logone"); pane = find_log(panel->id, panel->generation);
         assert(gtk_window_get_focus(GTK_WINDOW(ui.window)) == pane->view);
         assert(the_workbench_panel_state(panel->id)->effective == THE_WORKBENCH_PLACE_RIGHT);
         buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(pane->view));
         gtk_text_buffer_get_bounds(buffer, &first, &last);
         text = gtk_text_buffer_get_text(buffer, &first, &last, FALSE);
         assert(strstr(text, "NUL:\\x00 ESC:\\x1B C1:\\u009B Ω")); g_free(text);
         gtk_text_buffer_get_iter_at_offset(buffer, &first, 3); gtk_text_buffer_get_iter_at_offset(buffer, &last, 9);
         gtk_text_buffer_select_range(buffer, &last, &first);
         adjustment = gtk_scrolled_window_get_vadjustment(GTK_SCROLLED_WINDOW(pane->scroller));
         gtk_adjustment_set_value(adjustment, 100); await_frame(); scroll = gtk_adjustment_get_value(adjustment);
         assert(the_workbench_log_append(panel->model, panel->model_generation, 1001, 2, 2,
                                         "APPENDED stderr\n", 16) == THE_WORKBENCH_LOG_OK);
         sync_logs(); await_frame();
         assert(gtk_window_get_focus(GTK_WINDOW(ui.window)) == pane->view);
         assert(gtk_text_buffer_get_selection_bounds(buffer, &first, &last));
         a = gtk_text_iter_get_offset(&first); b = gtk_text_iter_get_offset(&last);
         assert(a == 3 && b == 9);
         assert(gtk_adjustment_get_value(adjustment) >= scroll - ui.cell_height &&
                gtk_adjustment_get_value(adjustment) <= scroll + ui.cell_height);
         assert_profile_guide(); screenshot("named-log-right");
         press_workbench_key(GDK_KEY_Escape, 0);
         break;
      }
      case 4:
      {
         int first_offset, last_offset;
         assert(gtk_window_get_focus(GTK_WINDOW(ui.window)) == ui.input);
         assert(!strcmp(gtk_entry_get_text(GTK_ENTRY(ui.input)), "LOG unsent draft"));
         assert(gtk_editable_get_selection_bounds(GTK_EDITABLE(ui.input), &first_offset, &last_offset));
         assert(first_offset == 3 && last_offset == 11 && gtk_editable_get_position(GTK_EDITABLE(ui.input)) == 3);
         resize_window(900, GTK_ORIENTATION_VERTICAL);
         gtk_widget_grab_focus(ui.stop);
         panel = the_workbench_panel_find("gtk.logtwo");
         assert(queue_panel(find_log(panel->id, panel->generation), THE_WORKBENCH_PANEL_FOCUS));
         break;
      }
      case 5:
         panel = the_workbench_panel_find("gtk.logtwo"); pane = find_log(panel->id, panel->generation);
         assert(gtk_window_get_focus(GTK_WINDOW(ui.window)) == pane->view);
         assert(the_workbench_panel_state(panel->id)->effective == THE_WORKBENCH_PLACE_BELOW);
         assert_profile_guide(); screenshot("named-log-below");
         press_workbench_key(GDK_KEY_h, 0);
         break;
      case 6:
         assert(gtk_window_get_focus(GTK_WINDOW(ui.window)) == ui.stop);
         assert(!the_workbench_panel_state("gtk.logtwo")->visible);
         gtk_widget_grab_focus(ui.editor);
         panel = the_workbench_panel_find("gtk.logone");
         assert(queue_panel(find_log(panel->id, panel->generation), THE_WORKBENCH_PANEL_FOCUS));
         break;
      case 7:
         panel = the_workbench_panel_find("gtk.logone"); pane = find_log(panel->id, panel->generation);
         assert(gtk_window_get_focus(GTK_WINDOW(ui.window)) == pane->view);
         press_workbench_key(GDK_KEY_Escape, 0);
         break;
      case 8:
         assert(gtk_window_get_focus(GTK_WINDOW(ui.window)) == ui.editor);
         assert(CURRENT_VIEW->current_window == log_origin_window);
         assert(!memcmp(&CURRENT_VIEW->logical_cursor.current, &log_origin_cursor, sizeof(log_origin_cursor)));
         gtk_widget_grab_focus(ui.input);
         gtk_editable_select_region(GTK_EDITABLE(ui.input), 11, 3);
         panel = the_workbench_panel_find("gtk.logone");
         assert(queue_panel(find_log(panel->id, panel->generation), THE_WORKBENCH_PANEL_FOCUS));
         break;
      case 9:
         replace_focused_log(0);  /* Same-model PUT must return to the saved Program origin. */
         assert_log_program_draft();
         panel = the_workbench_panel_find("gtk.logone");
         assert(queue_panel(find_log(panel->id, panel->generation), THE_WORKBENCH_PANEL_FOCUS));
         break;
      case 10:
         panel = the_workbench_panel_find("gtk.logone"); pane = find_log(panel->id, panel->generation);
         assert(gtk_window_get_focus(GTK_WINDOW(ui.window)) == pane->view);
         assert(ui.focused_log_generation == panel->generation);
         press_workbench_key(GDK_KEY_Escape, 0);
         break;
      case 11:
         assert_log_program_draft();
         gtk_widget_grab_focus(ui.editor);
         panel = the_workbench_panel_find("gtk.logone");
         assert(queue_panel(find_log(panel->id, panel->generation), THE_WORKBENCH_PANEL_FOCUS));
         break;
      case 12:
         replace_focused_log(1);  /* PLACE also replaces the focused generation. */
         assert(gtk_window_get_focus(GTK_WINDOW(ui.window)) == ui.editor);
         assert(CURRENT_VIEW->current_window == log_origin_window);
         assert(!memcmp(&CURRENT_VIEW->logical_cursor.current, &log_origin_cursor, sizeof(log_origin_cursor)));
         assert(!strcmp(gtk_entry_get_text(GTK_ENTRY(ui.input)), "LOG unsent draft"));
         panel = the_workbench_panel_find("gtk.logone");
         assert(queue_panel(find_log(panel->id, panel->generation), THE_WORKBENCH_PANEL_FOCUS));
         break;
      case 13:
         panel = the_workbench_panel_find("gtk.logone"); pane = find_log(panel->id, panel->generation);
         assert(gtk_window_get_focus(GTK_WINDOW(ui.window)) == pane->view);
         assert(ui.focused_log_generation == panel->generation);
         press_workbench_key(GDK_KEY_Escape, 0);
         break;
      case 14:
         assert(gtk_window_get_focus(GTK_WINDOW(ui.window)) == ui.editor);
         assert(CURRENT_VIEW->current_window == log_origin_window);
         assert(!memcmp(&CURRENT_VIEW->logical_cursor.current, &log_origin_cursor, sizeof(log_origin_cursor)));
         assert(!the_workbench_begin("gtk.logs"));
         assert(!the_workbench_reset_owner("gtk.logs")); assert(!the_workbench_commit());
         sync_logs(); assert(!ui.logs->len);
         gtk_entry_set_text(GTK_ENTRY(ui.input), log_input_draft); g_free(log_input_draft); log_input_draft = NULL;
         assert_source("say 'GTK program ready'\npull name\nsay 'Hello' name\n");
         puts("GTK_NAMED_LOG_CONTROL_ESCAPE_SCROLL_FOCUS_PROGRAM_DRAFT_RETURN_PASS");
         puts("GTK_LOG_PUT_PLACE_GENERATION_PROGRAM_EDITOR_FOCUS_RETURN_PASS");
         queue_command("nop");
         break;
      default: assert(0);
   }
}

static int probe_read_input(TheInputEvent *event)
{
   GdkEventKey key;
   GdkEventButton hit;
   TheDriverWindow *window;
   TheDriverWindowOrigin origin;
   TheRenderCell cell;
   int row;
   const char *directory = getenv("THE_GTK_PROBE_DIR");
   assert(directory);
   /* Startup profile errors can page messages before the interactive editor.
    * Dismiss the page; the contract checks begin once startup has completed. */
   if (in_macro || !been_interactive)
      return the_input_event_from_key_name("enter", event);
   if (the_tools_take_menu_run(event)) return 1;
   while (g_main_context_pending(NULL))
      g_main_context_iteration(NULL, FALSE);
   if (!g_queue_is_empty(&ui.inputs) || ui.rows != terminal_lines || ui.cols != terminal_cols)
      return read_input(event);
   if (preflight < 9)
   {
      g_snprintf(sample, sizeof(sample), "%s/sample.txt", directory);
      switch (preflight++)
      {
         case 0:
            assert_source("alpha\nbeta\n");
            assert(!the_editor_buffer_dirty(CURRENT_FILE));
            assert_default_palette();
            assert_application_menus();
            assert_workbench_focus_cycle();
            puts("GTK_WORKBENCH_SHIFT_TAB_ESC_CARET_DRAFT_TAB_PASS");
            assert_profile_guide();
            assert(gtk_orientable_get_orientation(GTK_ORIENTABLE(ui.split))
                   == GTK_ORIENTATION_HORIZONTAL);
            screenshot("default-launch");
            resize_window(1366, GTK_ORIENTATION_HORIZONTAL);
            queue_command("cursor cmdline");
            break;
         case 1:
            screenshot("default-wide");
            window = driver_current_role_window(WINDOW_FILEAREA);
            origin = operations.window_origin(window);
            for (row = 0; row < CURRENT_SCREEN.rows[WINDOW_FILEAREA]; row++)
               if (CURRENT_SCREEN.sl[row].line_number == 1)
                  break;
            assert(row < CURRENT_SCREEN.rows[WINDOW_FILEAREA]);
            memset(&hit, 0, sizeof(hit));
            hit.button = 1;
            hit.x = origin.col * ui.cell_width + 1;
            hit.y = (origin.row + row) * ui.cell_height + 1;
            clicked(ui.editor, &hit, NULL);
            break;
         case 2:
            assert(CURRENT_VIEW->current_window == WINDOW_FILEAREA);
            assert(CURRENT_VIEW->focus_line == 1);
            assert(CURRENT_VIEW->logical_cursor.current.text.cell_column == 0);
            click_noneditable_row(0);
            im_commit(ui.im, "X", NULL);
            break;
         case 3:
            assert_source("Xalpha\nbeta\n");
            assert(the_editor_buffer_dirty(CURRENT_FILE));
            assert(g_str_has_prefix(gtk_window_get_title(GTK_WINDOW(ui.window)), "* "));
            assert_original_saved();
            assert(CURRENT_VIEW->focus_line == 1);
            assert(CURRENT_VIEW->logical_cursor.current.text.cell_column == 1);
            screenshot("status-click-typed");
            memset(&key, 0, sizeof(key));
            key.keyval = GDK_KEY_BackSpace;
            assert(key_pressed(ui.editor, &key, NULL));
            break;
         case 4:
            assert_source("alpha\nbeta\n");
            assert(!the_editor_buffer_dirty(CURRENT_FILE));
            click_noneditable_row(1);
            assert(CURRENT_VIEW->focus_line == 1);
            assert(CURRENT_VIEW->logical_cursor.current.text.cell_column == 0);
            im_commit(ui.im, "Y", NULL);
            break;
         case 5:
            assert_source("Yalpha\nbeta\n");
            assert(the_editor_buffer_dirty(CURRENT_FILE));
            assert(CURRENT_VIEW->current_window == WINDOW_FILEAREA);
            assert(CURRENT_VIEW->focus_line == 1);
            assert(CURRENT_VIEW->logical_cursor.current.text.cell_column == 1);
            assert_original_saved();
            screenshot("reserved-click-typed");
            activate_shared_action("the.edit.undo");
            break;
         case 6:
            assert_source("alpha\nbeta\n");
            assert(!the_editor_buffer_dirty(CURRENT_FILE));
            activate_shared_action("the.file.save");
            break;
         case 7:
            assert_original_saved();
            assert(!the_editor_buffer_dirty(CURRENT_FILE));
            resize_window(900, GTK_ORIENTATION_VERTICAL);
            queue_command("cursor cmdline");
            break;
         case 8:
            assert_profile_guide();
            screenshot("default-narrow");
            puts("GTK_PROFILE_GUIDE_VISIBLE_WIDE_AND_NARROW_PASS");
            puts("GTK_DEFAULT_PALETTE_STATUS_RESERVED_TYPE_BACKSPACE_UNDO_SAVE_PASS");
            queue_command("cursor cmdline");
            break;
      }
      return read_input(event);
   }
   switch (phase++)
   {
      case 0:
         g_snprintf(sample, sizeof(sample), "%s/sample.txt", directory);
         g_snprintf(other, sizeof(other), "%s/other.txt", directory);
         g_snprintf(program, sizeof(program), "%s/program.rexx", directory);
         assert(!the_driver_is_curses() && !the_driver_is_headless() && curses_started);
         assert_source("alpha\nbeta\n");
         assert(driver_global_window_exists(THE_DRIVER_GLOBAL_STATAREA));
         window = driver_current_role_window(WINDOW_FILEAREA);
         assert(window && ui.surfaces);
         for (row = 0; row < CURRENT_SCREEN.rows[WINDOW_FILEAREA]; row++)
            if (CURRENT_SCREEN.sl[row].line_number == 2)
               break;
         assert(row < CURRENT_SCREEN.rows[WINDOW_FILEAREA]);
         assert(headless_driver_render_cell_at(window, row, 0, &cell));
         assert(cell.codepoint_count && cell.codepoints[0] == 'b');
         origin = operations.window_origin(window);
         memset(&hit, 0, sizeof(hit));
         hit.button = 1;
         hit.x = (origin.col + 2) * ui.cell_width + 1;
         hit.y = (origin.row + row) * ui.cell_height + 1;
         clicked(ui.editor, &hit, NULL);
         break;
      case 1:
         assert(CURRENT_VIEW->focus_line == 2);
         assert(CURRENT_VIEW->logical_cursor.current.text.cell_column == 2);
         im_commit(ui.im, "\303\251", NULL);
         break;
      case 2:
         assert_source(expected);
         assert(the_editor_buffer_dirty(CURRENT_FILE));
         assert(g_str_has_prefix(gtk_window_get_title(GTK_WINDOW(ui.window)), "* "));
         assert(CURRENT_VIEW->logical_cursor.current.text.cell_column == 3);
         if (!dirty_quit_checked)
         {
            dirty_quit_checked = 1;
            g_action_group_activate_action(G_ACTION_GROUP(ui.application), "quit", NULL);
            --phase;
            break;
         }
         assert(ui.active && !strcmp((char *)CURRENT_FILE->fname, "sample.txt"));
         assert_original_saved();
         puts("GTK_NATIVE_QUIT_RETAINS_UNSAVED_TEXT_AND_FILE_BYTES_PASS");
         memset(&key, 0, sizeof(key));
         key.keyval = GDK_KEY_s;
         key.state = GDK_CONTROL_MASK;
         assert(key_pressed(ui.editor, &key, NULL));
         break;
      case 3:
         assert_saved();
         open_file(other);
         break;
      case 4:
         assert(!strcmp((char *)CURRENT_FILE->fname, "other.txt"));
         open_file(sample);
         break;
      case 5:
         assert_source(expected);
         file_action(NULL, "file.close");
         break;
      case 6:
         assert(!strcmp((char *)CURRENT_FILE->fname, "other.txt"));
         open_file(sample);
         break;
      case 7:
         assert_source(expected);
         assert_saved();
         puts("GTK_OPEN_EDIT_CURSOR_DIRTY_SAVE_REOPEN_PASS");
         open_file(program);
         break;
      case 8:
         assert(!strcmp((char *)CURRENT_FILE->fname, "program.rexx"));
         if (the_tools_backend_available())
            activate_shared_action("profile.action.8");
         else
         {
            assert(!getenv("THE_GTK_REQUIRE_PROGRAM"));
            puts("GTK_PROGRAM_CONSOLE_NOT_CONFIGURED");
            phase = 14;
            queue_command("cursor cmdline");
         }
         break;
      case 9:
         await_console(0);
         if (log_probe_phase < 15) { named_log_step(); --phase; break; }
         if (workbench_focus_phase == 0)
         {
            assert_program_input_keeps_focus();
            assert_workbench_focus_cycle();
            workbench_editor_window = CURRENT_VIEW->current_window;
            workbench_editor_cursor = CURRENT_VIEW->logical_cursor.current;
            assert(workbench_editor_window == WINDOW_COMMAND || workbench_editor_window == WINDOW_FILEAREA);
            gtk_entry_set_text(GTK_ENTRY(ui.input), "F6 unsent draft");
         }
         assert_source("say 'GTK program ready'\npull name\nsay 'Hello' name\n");
         assert(!the_editor_buffer_dirty(CURRENT_FILE));
         if (workbench_focus_phase < 3)
         {
            if (workbench_focus_phase)
            {
               assert(!strcmp(gtk_entry_get_text(GTK_ENTRY(ui.input)), "F6 unsent draft"));
               assert(gtk_window_get_focus(GTK_WINDOW(ui.window)) == ui.editor);
               assert(CURRENT_VIEW->current_window == (workbench_focus_phase == 1
                  ? (workbench_editor_window == WINDOW_COMMAND ? WINDOW_FILEAREA : WINDOW_COMMAND)
                  : workbench_editor_window));
            }
            if (workbench_focus_phase < 2)
            {
               gtk_widget_grab_focus(workbench_focus_phase == 0 ? ui.input : ui.stop);
               press_workbench_key(GDK_KEY_F6, 0);
               ++workbench_focus_phase;
               --phase;
               break;
            }
            puts("GTK_WORKBENCH_PROFILE_F6_INPUT_CONTROL_DRAFT_CARET_PASS");
            gtk_entry_set_text(GTK_ENTRY(ui.input), "F10 unsent draft");
            gtk_widget_grab_focus(ui.input);
            gtk_editable_select_region(GTK_EDITABLE(ui.input), 11, 3);
            workbench_focus_phase = 3;
         }
         else
         {
            GtkWidget *expected_focus = workbench_focus_phase < 5 ? ui.input
               : workbench_focus_phase == 5 ? ui.first_control : ui.editor;
            int first, last;
            assert(workbench_menu_seen == (int)workbench_focus_phase - 2);
            assert(gtk_window_get_focus(GTK_WINDOW(ui.window)) == expected_focus);
            assert(!strcmp(gtk_entry_get_text(GTK_ENTRY(ui.input)), "F10 unsent draft"));
            if (workbench_focus_phase == 3)
            {
               assert(gtk_editable_get_selection_bounds(GTK_EDITABLE(ui.input), &first, &last));
               assert(first == 3 && last == 11);
               assert(gtk_editable_get_position(GTK_EDITABLE(ui.input)) == 3);
               screenshot("workbench-menu-input-return");
            }
            else
            {
               assert(!gtk_editable_get_selection_bounds(GTK_EDITABLE(ui.input), &first, &last));
               assert(gtk_editable_get_position(GTK_EDITABLE(ui.input)) == 5);
            }
            assert(CURRENT_VIEW->current_window == workbench_editor_window);
            assert(CURRENT_VIEW->logical_cursor.current.zone == workbench_editor_cursor.zone);
            assert(CURRENT_VIEW->logical_cursor.current.line_number == workbench_editor_cursor.line_number);
            assert(CURRENT_VIEW->logical_cursor.current.text.cell_column == workbench_editor_cursor.text.cell_column);
            assert(g_queue_is_empty(&ui.inputs));
            if (workbench_focus_phase++ == 6)
            {
               puts("GTK_WORKBENCH_PROFILE_F10_MENU_ESC_DRAFT_CARET_PASS");
               puts("GTK_WORKBENCH_F10_CANCEL_RESTORES_INPUT_SELECTION_CARET_CONTROL_EDITOR_PASS");
               change_palette();
               break;
            }
            gtk_widget_grab_focus(workbench_focus_phase == 4 ? ui.input
               : workbench_focus_phase == 5 ? ui.first_control : ui.editor);
            if (workbench_focus_phase == 4)
               gtk_editable_set_position(GTK_EDITABLE(ui.input), 5);
         }
         press_workbench_key(GDK_KEY_F10, 0);
         assert_profile_menu();
         queue_command("nop");
         gtk_menu_shell_deactivate(GTK_MENU_SHELL(ui.native_menu));
         --phase;
         break;
      case 10:
         assert_palette(1);
         assert_palette_preserves_job();
         open_file(other);
         break;
      case 11:
         assert(!strcmp((char *)CURRENT_FILE->fname, "other.txt"));
         assert_palette(0);
         assert_palette_preserves_job();
         open_file(program);
         break;
      case 12:
         assert(!strcmp((char *)CURRENT_FILE->fname, "program.rexx"));
         assert_palette(1);
         assert_palette_preserves_job();
         g_free(palette_output);
         palette_output = NULL;
         puts("GTK_PROFILE_PALETTE_LIVE_FILE_SWITCH_JOB_SELECTION_PASS");
         gtk_entry_set_text(GTK_ENTRY(ui.input), "Ada");
         send_input(ui.input, NULL);
         break;
      case 13:
         await_console(1);
         assert(gtk_widget_get_sensitive(ui.run));
         assert(!gtk_widget_get_sensitive(ui.stop));
         assert(CURRENT_FILE->save_alt == 0);
         assert_saved();
         puts("GTK_PROGRAM_OUTPUT_STDIN_COMPLETION_PASS");
         queue_command("cursor cmdline");
         break;
      case 14:
         assert(CURRENT_VIEW->current_window == WINDOW_COMMAND);
         resize_window(1366, GTK_ORIENTATION_HORIZONTAL);
         queue_command("cursor cmdline");
         break;
      case 15:
         assert_layout(GTK_ORIENTATION_HORIZONTAL);
         screenshot(NULL);
         resize_window(900, GTK_ORIENTATION_VERTICAL);
         queue_command("cursor cmdline");
         break;
      case 16:
         assert_layout(GTK_ORIENTATION_VERTICAL);
         screenshot("narrow");
         g_action_group_activate_action(G_ACTION_GROUP(ui.application), "quit", NULL);
         puts("GTK_RESPONSIVE_CONSOLE_RIGHT_AND_BELOW_PASS");
         puts("GTK_VIRTUAL_SURFACES_COMMAND_LINE_PASS");
         fflush(stdout);
         break;
      default:
         assert(0);
   }
   return read_input(event);
}

const TheDriverOps *the_driver_module_ops(void)
{
   gtk_probe_base_operations();
   operations.read_input_event = probe_read_input;
   return &operations;
}
