/* Compile the production driver into a test-only module so this probe can
 * exercise its real callbacks without a production automation endpoint. */
#define the_driver_module_ops gtk_probe_base_operations
#define the_driver_module_lifecycle gtk_probe_base_lifecycle
#include "../src/drivers/gtk/gtkdriver.c"
#undef the_driver_module_ops
#undef the_driver_module_lifecycle

#include <assert.h>
#include <stdio.h>
#include "editorsource.h"
#include "editordocument.h"

static unsigned phase;
static unsigned preflight;
static int dirty_quit_checked;
static unsigned workbench_focus_phase;
static unsigned log_probe_phase;
static unsigned document_probe_phase;
static unsigned form_probe_phase;
static unsigned filefields_probe_phase;
static int filefields_below_pending;
static GtkWidget *filefields_resize_focus;
static unsigned stdin_probe_phase, contextual_phase;
static int reader_probe_done;
static char compiled_program[4096], profile_program[4096], imported_profile[4096];
static const char compiled_source[] = "options levelb\nsay 'GTK compiled ready'\nreturn 0\n";
static const char profile_source[] = "options levelb\naddress THE\n'set number on';\nsay 'GTK profile applied'\nreturn 0\n";
static char *contextual_history, *contextual_draft;
static unsigned contextual_program_id;
static char contextual_active[THE_TOOL_PATH];
static int contextual_window;
static LogicalCursor contextual_cursor;
static char filefields_probe_path[4096];
static char *filefields_input_draft;
static int form_origin_window;
static LogicalCursor form_origin_cursor;
static char *form_input_draft;
static uint64_t document_probe_id, document_probe_revision;
static char document_probe_path[4096];
static int document_origin_window;
static LogicalCursor document_origin_cursor;
static char *document_input_draft;
static int log_origin_window;
static LogicalCursor log_origin_cursor;
static char *log_input_draft;
static int workbench_menu_seen;
static int workbench_editor_window;
static LogicalCursor workbench_editor_cursor;
static char sample[4096], other[4096], program[4096], stdin_program[4096];
static const char expected[] = "alpha\nbe\303\251ta\n";
static const char stdin_source[] =
   "say 'GTK_STDIN_READY'\n"
   "parse pull first\n"
   "say 'GTK_STDIN_1<' || first || '>'\n"
   "parse pull second\n"
   "say 'GTK_STDIN_2<' || second || '>'\n"
   "parse pull third\n"
   "say 'GTK_STDIN_3<' || third || '>'\n"
   "parse pull fourth\n"
   "say 'GTK_STDIN_4<' || fourth || '>'\n"
   "say 'GTK_STDIN_DONE'\n";
static const char *const stdin_lines[] = {
   "GTK_STDIN_READY", "GTK_STDIN_1<alpha>", "GTK_STDIN_2<界Ω>",
   "GTK_STDIN_3<>", "GTK_STDIN_4<omega>", "GTK_STDIN_DONE"
};
static LogicalCursor stdin_cursor;
static int stdin_window;
static LINETYPE stdin_focus_line;
static void activate_shared_action(const char *id);
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
      console = the_tool_program_console();
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
           the_tool_program_console()->state, console_text());
   assert(0);
}

static void assert_stdin_records(const char *text, unsigned count)
{
   const char *line = text;
   unsigned found = 0;
   while (*line)
   {
      const char *end = strchr(line, '\n');
      size_t length = end ? (size_t)(end - line) : strlen(line);
      if (length >= strlen("GTK_STDIN_") && !strncmp(line, "GTK_STDIN_", strlen("GTK_STDIN_")))
      {
         assert(found < count);
         assert(length == strlen(stdin_lines[found]));
         assert(!memcmp(line, stdin_lines[found], length));
         ++found;
      }
      if (!end) break;
      line = end + 1;
   }
   assert(found == count);
}

static void await_stdin_records(unsigned count, int finished)
{
   gint64 deadline = g_get_monotonic_time() + 15000000;
   while (g_get_monotonic_time() < deadline)
   {
      char *text;
      const TheToolConsole *console;
      the_tools_native_poll();
      sync_console();
      console = the_tool_program_console();
      text = console_text();
      if (strstr(text, stdin_lines[count - 1])
      && (finished ? !console->running : console->running))
      {
         assert_stdin_records(text, count);
         g_free(text);
         return;
      }
      g_free(text);
      while (g_main_context_pending(NULL))
         g_main_context_iteration(NULL, FALSE);
      g_usleep(5000);
   }
   fprintf(stderr, "GTK stdin probe timed out at record %u: %s\n%s\n",
           count, the_tool_program_console()->state, console_text());
   assert(0);
}

static void assert_stdin_editor_unchanged(void)
{
   char *bytes = NULL;
   gsize length;
   assert_source(stdin_source);
   assert(!the_editor_buffer_dirty(CURRENT_FILE));
   assert(CURRENT_FILE->save_alt == 0);
   assert(CURRENT_VIEW->current_window == stdin_window);
   assert(CURRENT_VIEW->focus_line == stdin_focus_line);
   assert(CURRENT_VIEW->logical_cursor.current.zone == stdin_cursor.zone);
   assert(CURRENT_VIEW->logical_cursor.current.line_number == stdin_cursor.line_number);
   assert(CURRENT_VIEW->logical_cursor.current.text.cell_column == stdin_cursor.text.cell_column);
   assert(g_file_get_contents(stdin_program, &bytes, &length, NULL));
   assert(length == sizeof(stdin_source) - 1);
   assert(!memcmp(bytes, stdin_source, length));
   g_free(bytes);
}

static void stdin_probe_step(void)
{
   switch (stdin_probe_phase++)
   {
      case 1:
         assert(!strcmp((char *)CURRENT_FILE->fname, "stdin.rexx"));
         assert_source(stdin_source);
         assert(!the_editor_buffer_dirty(CURRENT_FILE));
         activate_shared_action("profile.action.8");
         break;
      case 2:
         await_stdin_records(1, 0);
         stdin_window = CURRENT_VIEW->current_window;
         stdin_focus_line = CURRENT_VIEW->focus_line;
         stdin_cursor = CURRENT_VIEW->logical_cursor.current;
         assert_stdin_editor_unchanged();
         gtk_entry_set_text(GTK_ENTRY(ui.input), "alpha");
         gtk_widget_grab_focus(ui.input);
         g_signal_emit_by_name(ui.input, "activate");
         assert(!strcmp(gtk_entry_get_text(GTK_ENTRY(ui.input)), ""));
         assert(gtk_window_get_focus(GTK_WINDOW(ui.window)) == ui.input);
         break;
      case 3:
         await_stdin_records(2, 0);
         assert_stdin_editor_unchanged();
         gtk_entry_set_text(GTK_ENTRY(ui.input), "界Ω");
         gtk_widget_grab_focus(ui.send);
         g_signal_emit_by_name(ui.send, "clicked");
         assert(!strcmp(gtk_entry_get_text(GTK_ENTRY(ui.input)), ""));
         assert(gtk_window_get_focus(GTK_WINDOW(ui.window)) == ui.input);
         break;
      case 4:
         await_stdin_records(3, 0);
         assert_stdin_editor_unchanged();
         g_signal_emit_by_name(ui.input, "activate");
         assert(!strcmp(gtk_entry_get_text(GTK_ENTRY(ui.input)), ""));
         assert(gtk_window_get_focus(GTK_WINDOW(ui.window)) == ui.input);
         break;
      case 5:
         await_stdin_records(4, 0);
         assert_stdin_editor_unchanged();
         gtk_entry_set_text(GTK_ENTRY(ui.input), "omega");
         gtk_widget_grab_focus(ui.send);
         g_signal_emit_by_name(ui.send, "clicked");
         assert(!strcmp(gtk_entry_get_text(GTK_ENTRY(ui.input)), ""));
         assert(gtk_window_get_focus(GTK_WINDOW(ui.window)) == ui.input);
         break;
      case 6:
         await_stdin_records(6, 1);
         assert_stdin_editor_unchanged();
         puts("GTK_STDIN_ONE_RECORD_PER_ENTER_SEND_UNICODE_EMPTY_PASS");
         stdin_probe_phase = 0;
         open_file(program);
         queue_command("cursor cmdline");
         break;
      default:
         assert(0);
   }
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
   if (!painted) fprintf(stderr, "GTK frame timeout: preflight=%u phase=%u log=%u document=%u\n",
      preflight, phase, log_probe_phase, document_probe_phase);
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
   if (ui.rows != terminal_lines || ui.cols != terminal_cols)
      fprintf(stderr, "GTK screenshot %s phase=%u filefields=%u widget-grid=%dx%d owner-grid=%dx%d\n",
         suffix ? suffix : "final", phase, filefields_probe_phase, ui.rows, ui.cols, terminal_lines, terminal_cols);
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
      bounds_in_split(ui.auxiliary, &console);
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
   bounds_in_split(ui.auxiliary, &console);
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
   int build_visible = gtk_widget_get_visible(ui.build_result);
   int auxiliary_page = gtk_notebook_get_current_page(GTK_NOTEBOOK(ui.auxiliary));
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
   gtk_widget_hide(ui.build_result);
   press_workbench_key(GDK_KEY_ISO_Left_Tab, GDK_SHIFT_MASK);
   assert(gtk_window_get_focus(GTK_WINDOW(ui.window)) == ui.first_control);
   press_workbench_key(GDK_KEY_Escape, 0);
   assert(gtk_window_get_focus(GTK_WINDOW(ui.window)) == ui.editor);
   gtk_widget_show(ui.console);
   gtk_widget_set_visible(ui.build_result, build_visible);
   ui.syncing_logs++;
   gtk_notebook_set_current_page(GTK_NOTEBOOK(ui.auxiliary), auxiliary_page);
   ui.syncing_logs--;
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
   assert(!strcmp(gtk_button_get_label(GTK_BUTTON(ui.run)), ui.file_snapshot.actions[1].label));
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
   const TheToolConsole *console = the_tool_program_console();
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
   const TheToolConsole *console = the_tool_program_console();
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
         the_tool_program_console_visibility(1, 1);
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

static char *document_probe_text(GtkLogPane *pane)
{
   GtkTextIter first, last;
   GtkTextBuffer *buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(pane->view));
   gtk_text_buffer_get_bounds(buffer, &first, &last);
   return gtk_text_buffer_get_text(buffer, &first, &last, FALSE);
}

static void document_step(void)
{
   const TheWorkbenchPanel *panel = the_workbench_panel_find("gtk.document");
   GtkLogPane *pane = panel ? find_log(panel->id, panel->generation) : NULL;
   TheWorkbenchDocumentInfo info;
   char command[4200], *text;
   switch (document_probe_phase++)
   {
      case 0:
      {
         GString *fixture = g_string_new(NULL);
         unsigned i;
         g_snprintf(document_probe_path, sizeof(document_probe_path), "%s/document.txt", getenv("THE_GTK_PROBE_DIR"));
         for (i = 1; i <= 96; i++) g_string_append_printf(fixture, "DOC-LINE %03u Ω < & >\n", i);
         g_string_append(fixture, "/* ");
         for (i = 0; i < 803; i++) g_string_append_c(fixture, 'a');
         g_string_append(fixture, "€");
         for (i = 809; i < 9000 - strlen(" FINAL_UTF8_Z */"); i++) g_string_append_c(fixture, 'a');
         g_string_append(fixture, " FINAL_UTF8_Z */\n");
         assert(g_file_set_contents(document_probe_path, fixture->str, fixture->len, NULL));
         g_string_free(fixture, TRUE);
         g_snprintf(command, sizeof(command), "edit %s", document_probe_path); queue_command(command);
         break;
      }
      case 1:
      {
         TheWorkbenchPanel value = {0};
         assert(!strcmp((char *)CURRENT_FILE->fname, "document.txt"));
         document_probe_id = the_editor_document_active();
         assert(the_workbench_document_info(document_probe_id, &info) == THE_WORKBENCH_DOCUMENT_OK && info.lines == 97);
         document_probe_revision = info.revision;
         strcpy(value.id, "gtk.document"); strcpy(value.title, "Document / Ω");
         value.kind = THE_WORKBENCH_PANEL_DOCUMENT; value.document_id = document_probe_id;
         assert(!the_workbench_begin("gtk.documents")); assert(!the_workbench_put_panel(&value)); assert(!the_workbench_commit());
         g_snprintf(command, sizeof(command), "edit %s", program); queue_command(command);
         break;
      }
      case 2:
         assert_source("say 'GTK program ready'\npull name\nsay 'Hello' name\n");
         document_origin_window = CURRENT_VIEW->current_window; document_origin_cursor = CURRENT_VIEW->logical_cursor.current;
         document_input_draft = g_strdup(gtk_entry_get_text(GTK_ENTRY(ui.input)));
         gtk_entry_set_text(GTK_ENTRY(ui.input), "DOCUMENT unsent draft");
         gtk_widget_grab_focus(ui.input); gtk_editable_select_region(GTK_EDITABLE(ui.input), 11, 3);
         the_tool_program_console_visibility(1, 1); sync_logs();
         panel = the_workbench_panel_find("gtk.document"); pane = find_log(panel->id, panel->generation);
         assert(queue_panel(pane, THE_WORKBENCH_PANEL_SHOW));
         break;
      case 3:
         assert(gtk_window_get_focus(GTK_WINDOW(ui.window)) == ui.input);
         assert(gtk_notebook_get_nth_page(GTK_NOTEBOOK(ui.auxiliary), gtk_notebook_get_current_page(GTK_NOTEBOOK(ui.auxiliary))) == ui.console);
         resize_window(1366, GTK_ORIENTATION_HORIZONTAL);
         assert(queue_panel(pane, THE_WORKBENCH_PANEL_FOCUS));
         break;
      case 4:
         assert(gtk_window_get_focus(GTK_WINDOW(ui.window)) == pane->view);
         assert(the_workbench_panel_state(panel->id)->effective == THE_WORKBENCH_PLACE_RIGHT);
         assert(strstr(gtk_label_get_text(GTK_LABEL(pane->status)), "Read-only document"));
         text = document_probe_text(pane); assert(strstr(text, "DOC-LINE 001 Ω < & >")); g_free(text);
         {
            GdkEventKey key = {0};
            GtkClipboard *clipboard = gtk_clipboard_get(GDK_SELECTION_CLIPBOARD);
            char *saved = gtk_clipboard_wait_for_text(clipboard), *copied;
            key.state = GDK_CONTROL_MASK; key.keyval = GDK_KEY_a;
            assert(!workbench_key_pressed(ui.window, &key, NULL));
            g_signal_emit_by_name(pane->view, "select-all", TRUE);
            assert(gtk_text_buffer_get_has_selection(gtk_text_view_get_buffer(GTK_TEXT_VIEW(pane->view))));
            key.keyval = GDK_KEY_c;
            assert(!workbench_key_pressed(ui.window, &key, NULL));
            g_signal_emit_by_name(pane->view, "copy-clipboard");
            copied = gtk_clipboard_wait_for_text(clipboard); assert(copied && strstr(copied, "DOC-LINE 001 Ω < & >"));
            g_free(copied);
            if (saved) { gtk_clipboard_set_text(clipboard, saved, -1); g_free(saved); }
            else gtk_clipboard_clear(clipboard);
         }
         press_workbench_key(GDK_KEY_x, 0); press_workbench_key(GDK_KEY_BackSpace, 0); press_workbench_key(GDK_KEY_Return, 0);
         assert(g_queue_is_empty(&ui.inputs));
         text = document_probe_text(pane); assert(strstr(text, "DOC-LINE 001 Ω < & >")); g_free(text);
         assert_source("say 'GTK program ready'\npull name\nsay 'Hello' name\n"); assert(!the_editor_buffer_dirty(CURRENT_FILE));
         press_workbench_key(GDK_KEY_Page_Down, 0); await_frame();
         assert(pane->document_first.line > 1);
         screenshot("document-right"); press_workbench_key(GDK_KEY_Escape, 0);
         break;
      case 5:
      {
         int first, last;
         assert(gtk_window_get_focus(GTK_WINDOW(ui.window)) == ui.input);
         assert(!strcmp(gtk_entry_get_text(GTK_ENTRY(ui.input)), "DOCUMENT unsent draft"));
         assert(gtk_editable_get_selection_bounds(GTK_EDITABLE(ui.input), &first, &last));
         assert(first == 3 && last == 11 && gtk_editable_get_position(GTK_EDITABLE(ui.input)) == 3);
         assert(queue_panel(pane, THE_WORKBENCH_PANEL_FOCUS));
         break;
      }
      case 6:
         assert(pane->document_first.line > 1);
         resize_window(900, GTK_ORIENTATION_VERTICAL); sync_logs(); await_frame();
         assert(the_workbench_panel_state(panel->id)->effective == THE_WORKBENCH_PLACE_BELOW);
         press_workbench_key(GDK_KEY_Home, 0); assert(pane->document_first.line == 1);
         press_workbench_key(GDK_KEY_End, 0);
         text = document_probe_text(pane); assert(strstr(text, "FINAL_UTF8_Z")); g_free(text);
         break;
      case 7:
      {
         GtkAdjustment *adjustment = gtk_scrolled_window_get_vadjustment(GTK_SCROLLED_WINDOW(pane->scroller));
         GtkTextBuffer *buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(pane->view));
         GtkTextIter end, first, last;
         GdkRectangle marker, visible;
         await_frame();
         assert(gtk_window_get_focus(GTK_WINDOW(ui.window)) == pane->view);
         assert(gtk_adjustment_get_value(adjustment) >= gtk_adjustment_get_upper(adjustment)
            - gtk_adjustment_get_page_size(adjustment) - ui.cell_height);
         gtk_text_buffer_get_end_iter(buffer, &end);
         assert(gtk_text_iter_backward_search(&end, "FINAL_UTF8_Z", GTK_TEXT_SEARCH_TEXT_ONLY, &first, &last, NULL));
         gtk_text_view_get_iter_location(GTK_TEXT_VIEW(pane->view), &first, &marker);
         gtk_text_view_get_visible_rect(GTK_TEXT_VIEW(pane->view), &visible);
         assert(marker.y >= visible.y && marker.y < visible.y + visible.height);
         assert_profile_guide(); screenshot("document-below"); press_workbench_key(GDK_KEY_Escape, 0);
         break;
      }
      case 8:
         assert(gtk_window_get_focus(GTK_WINDOW(ui.window)) == ui.input);
         assert(CURRENT_VIEW->current_window == document_origin_window);
         assert(!memcmp(&CURRENT_VIEW->logical_cursor.current, &document_origin_cursor, sizeof(document_origin_cursor)));
         gtk_widget_grab_focus(ui.editor);
         g_snprintf(command, sizeof(command), "edit %s", document_probe_path); queue_command(command);
         break;
      case 9:
         assert(the_editor_document_active() == document_probe_id);
         queue_command("cursor goto 1 1"); queue_command("replace DOC-CHANGED Ω"); queue_command("save");
         break;
      case 10:
         assert(the_workbench_document_info(document_probe_id, &info) == THE_WORKBENCH_DOCUMENT_OK);
         assert(info.revision > document_probe_revision);
         document_origin_window = CURRENT_VIEW->current_window; document_origin_cursor = CURRENT_VIEW->logical_cursor.current;
         pane->document_request = -2; sync_logs(); await_frame();
         assert(gtk_window_get_focus(GTK_WINDOW(ui.window)) == ui.editor);
         text = document_probe_text(pane); assert(strstr(text, "DOC-CHANGED Ω")); g_free(text);
         assert(queue_panel(pane, THE_WORKBENCH_PANEL_FOCUS));
         break;
      case 11:
         assert(gtk_window_get_focus(GTK_WINDOW(ui.window)) == pane->view);
         press_workbench_key(GDK_KEY_Escape, 0);
         break;
      case 12:
         assert(gtk_window_get_focus(GTK_WINDOW(ui.window)) == ui.editor);
         assert(CURRENT_VIEW->current_window == document_origin_window);
         assert(!memcmp(&CURRENT_VIEW->logical_cursor.current, &document_origin_cursor, sizeof(document_origin_cursor)));
         queue_command("quit");
         break;
      case 13:
         assert(the_workbench_document_info(document_probe_id, &info) == THE_WORKBENCH_DOCUMENT_MISSING);
         sync_logs(); assert(strstr(gtk_label_get_text(GTK_LABEL(pane->status)), "document-closed"));
         g_snprintf(command, sizeof(command), "edit %s", document_probe_path); queue_command(command);
         break;
      case 14:
         assert(the_editor_document_active() != document_probe_id);
         assert(panel->document_id == document_probe_id);
         assert(the_workbench_document_info(document_probe_id, &info) == THE_WORKBENCH_DOCUMENT_MISSING);
         queue_command("quit");
         break;
      case 15:
         assert(!the_workbench_begin("gtk.documents")); assert(!the_workbench_reset_owner("gtk.documents")); assert(!the_workbench_commit());
         sync_logs(); assert(!ui.logs->len);
         gtk_entry_set_text(GTK_ENTRY(ui.input), document_input_draft); g_free(document_input_draft); document_input_draft = NULL;
         assert_source("say 'GTK program ready'\npull name\nsay 'Hello' name\n"); assert(!the_editor_buffer_dirty(CURRENT_FILE));
         puts("GTK_DOCUMENT_READONLY_PAGING_REVISION_BINDING_FOCUS_RETURN_PASS"); queue_command("nop");
         break;
      default: assert(0);
   }
}

static GtkFormField *probe_form_field(GtkLogPane *pane, const char *id)
{
   guint i;
   for (i = 0; i < pane->form_fields->len; i++)
   {
      GtkFormField *field = g_ptr_array_index(pane->form_fields, i);
      if (!strcmp(field->id, id)) return field;
   }
   assert(0); return NULL;
}

static void form_step(void)
{
   const TheWorkbenchPanel *panel = the_workbench_panel_find("gtk.formpane");
   GtkLogPane *pane = panel ? find_log(panel->id, panel->generation) : NULL;
   const TheWorkbenchFormState *state = the_workbench_form_state("gtk.person");
   switch (form_probe_phase++)
   {
      case 0:
      {
         TheWorkbenchForm form = {0}; TheWorkbenchPanel value = {0}; TheWorkbenchField field = {0}; unsigned i;
         strcpy(form.id, "gtk.person"); assert(!the_workbench_begin("gtk.forms")); assert(!the_workbench_put_form(&form));
         strcpy(field.form, form.id); field.access = THE_WORKBENCH_FIELD_EDIT; field.maxcells = 16;
         strcpy(field.id, "name"); strcpy(field.label, "Name / Ω"); assert(!the_workbench_put_field(&field));
         strcpy(field.id, "locked"); strcpy(field.label, "Protected"); strcpy(field.value, "LOCK Ω"); field.access = THE_WORKBENCH_FIELD_READONLY;
         assert(!the_workbench_put_field(&field));
         strcpy(field.id, "note"); strcpy(field.label, "Note"); strcpy(field.value, "kept"); field.access = THE_WORKBENCH_FIELD_EDIT;
         assert(!the_workbench_put_field(&field)); field.value[0] = 0;
         for (i = 0; i < 5; i++)
         { g_snprintf(field.id, sizeof(field.id), "extra%u", i); g_snprintf(field.label, sizeof(field.label), "Extra %u", i); assert(!the_workbench_put_field(&field)); }
         strcpy(value.id, "gtk.formpane"); strcpy(value.title, "Independent form / Ω"); value.kind = THE_WORKBENCH_PANEL_FIELDS; strcpy(value.form, form.id);
         assert(!the_workbench_put_panel(&value)); assert(!the_workbench_commit()); sync_logs();
         panel = the_workbench_panel_find(value.id); pane = find_log(panel->id, panel->generation);
         form_origin_window = CURRENT_VIEW->current_window; form_origin_cursor = CURRENT_VIEW->logical_cursor.current;
         form_input_draft = g_strdup(gtk_entry_get_text(GTK_ENTRY(ui.input)));
         gtk_entry_set_text(GTK_ENTRY(ui.input), "FORM Program draft"); gtk_widget_grab_focus(ui.input);
         gtk_editable_select_region(GTK_EDITABLE(ui.input), 10, 3); the_tool_program_console_visibility(1, 1);
         assert(queue_panel(pane, THE_WORKBENCH_PANEL_SHOW));
         break;
      }
      case 1:
         assert(gtk_window_get_focus(GTK_WINDOW(ui.window)) == ui.input);
         resize_window(1366, GTK_ORIENTATION_HORIZONTAL); assert(queue_panel(pane, THE_WORKBENCH_PANEL_FOCUS));
         gtk_entry_set_text(GTK_ENTRY(probe_form_field(pane, "name")->entry), "click");
         gtk_entry_set_text(GTK_ENTRY(probe_form_field(pane, "name")->entry), "click typed");
         break;
      case 2:
      {
         GtkFormField *name = probe_form_field(pane, "name");
         assert(gtk_window_get_focus(GTK_WINDOW(ui.window)) == name->entry);
         assert(!strcmp(the_workbench_field_state("gtk.person", "name")->value, "click typed"));
         assert(the_workbench_panel_state(panel->id)->effective == THE_WORKBENCH_PLACE_RIGHT);
         assert(!gtk_editable_get_editable(GTK_EDITABLE(probe_form_field(pane, "locked")->entry)));
         /* Native widget changes can arrive before any core dispatch. */
         gtk_entry_set_text(GTK_ENTRY(name->entry), "h"); gtk_entry_set_text(GTK_ENTRY(name->entry), "hé"); gtk_entry_set_text(GTK_ENTRY(name->entry), "hé h");
         g_signal_emit_by_name(pane->form_submit, "clicked");
         break;
      }
      case 3:
         assert(!strcmp(the_workbench_field_state("gtk.person", "name")->value, "hé h"));
         assert(!strcmp(the_workbench_field_state("gtk.person", "locked")->value, "LOCK Ω"));
         assert(state->submitted && state->result_revision == 1 && !state->dirty);
         assert(!the_panel_active && !the_editor_buffer_dirty(CURRENT_FILE));
         assert_source("say 'GTK program ready'\npull name\nsay 'Hello' name\n");
         screenshot("form-right"); resize_window(900, GTK_ORIENTATION_VERTICAL);
         gtk_entry_set_text(GTK_ENTRY(probe_form_field(pane, "name")->entry), "12345678901234567");
         break;
      case 4:
      {
         unsigned i;
         assert(!strcmp(the_workbench_field_state("gtk.person", "name")->value, "hé h"));
         assert(!strcmp(gtk_entry_get_text(GTK_ENTRY(probe_form_field(pane, "name")->entry)), "hé h"));
         assert(the_workbench_panel_state(panel->id)->effective == THE_WORKBENCH_PLACE_BELOW);
         for (i = 0; i < 6; i++) press_workbench_key(GDK_KEY_Tab, 0);
         assert(gtk_window_get_focus(GTK_WINDOW(ui.window)) == probe_form_field(pane, "extra4")->entry);
         await_frame();
         assert(gtk_adjustment_get_value(gtk_scrolled_window_get_vadjustment(GTK_SCROLLED_WINDOW(pane->scroller))) > 0);
         gtk_entry_set_text(GTK_ENTRY(probe_form_field(pane, "extra4")->entry), "temporary");
         g_signal_emit_by_name(pane->form_cancel, "clicked");
         break;
      }
      case 5:
         assert(!state->dirty && !strcmp(the_workbench_field_state("gtk.person", "extra4")->value, ""));
         assert(!strcmp(the_workbench_field_state("gtk.person", "name")->value, "hé h"));
         gtk_entry_set_text(GTK_ENTRY(probe_form_field(pane, "name")->entry), "RETURN DRAFT");
         press_workbench_key(GDK_KEY_Escape, 0);
         break;
      case 6:
      {
         int first, last;
         assert(gtk_window_get_focus(GTK_WINDOW(ui.window)) == ui.input);
         assert(!strcmp(gtk_entry_get_text(GTK_ENTRY(ui.input)), "FORM Program draft"));
         assert(gtk_editable_get_selection_bounds(GTK_EDITABLE(ui.input), &first, &last));
         assert(first == 3 && last == 10 && gtk_editable_get_position(GTK_EDITABLE(ui.input)) == 3);
         assert(!strcmp(the_workbench_field_state("gtk.person", "name")->value, "RETURN DRAFT"));
         assert(queue_panel(pane, THE_WORKBENCH_PANEL_FOCUS));
         break;
      }
      case 7:
      {
         TheWorkbenchFormRequest external = {0};
         TheInputEvent next;
         gtk_entry_set_text(GTK_ENTRY(probe_form_field(pane, "name")->entry), "QUEUED OLD");
         external.operation = THE_WORKBENCH_FORM_SET; external.request_id = 100000;
         strcpy(external.form, "gtk.person"); strcpy(external.field, "note"); strcpy(external.value, "external"); external.length = 8;
         external.form_generation = state->generation; external.draft_revision = state->draft_revision;
         assert(!the_workbench_form_request(&external));
         assert(the_input_event_from_command("nop", &next)); assert(queue_event(next));
         break;
      }
      case 8:
         assert(gtk_window_get_focus(GTK_WINDOW(ui.window)) == probe_form_field(pane, "name")->entry);
         assert(!strcmp(the_workbench_field_state("gtk.person", "name")->value, "RETURN DRAFT"));
         assert(!strcmp(the_workbench_field_state("gtk.person", "note")->value, "external"));
         assert(strstr(gtk_label_get_text(GTK_LABEL(pane->status)), "stale-revision"));
         screenshot("form-below"); press_workbench_key(GDK_KEY_Escape, 0);
         break;
      case 9:
         gtk_widget_grab_focus(ui.editor);
         assert(queue_panel(pane, THE_WORKBENCH_PANEL_FOCUS));
         break;
      case 10:
         assert(gtk_window_get_focus(GTK_WINDOW(ui.window)) == probe_form_field(pane, "name")->entry);
         press_workbench_key(GDK_KEY_Escape, 0);
         break;
      case 11:
         if (gtk_window_get_focus(GTK_WINDOW(ui.window)) != ui.editor)
         {
            GtkWidget *focus = gtk_window_get_focus(GTK_WINDOW(ui.window));
            fprintf(stderr, "Form editor RETURN focus=%s pane=%s origin=%s program=%d\n",
               focus ? gtk_widget_get_name(focus) : "none", ui.focused_log,
               ui.log_origin ? gtk_widget_get_name(ui.log_origin) : "none", the_tool_program_console()->focused);
         }
         assert(gtk_window_get_focus(GTK_WINDOW(ui.window)) == ui.editor);
         assert(CURRENT_VIEW->current_window == form_origin_window);
         assert(!memcmp(&CURRENT_VIEW->logical_cursor.current, &form_origin_cursor, sizeof(form_origin_cursor)));
         queue_form(pane, THE_WORKBENCH_FORM_CANCEL, NULL, NULL);
         break;
      case 12:
         assert(!state->dirty);
         assert(!the_workbench_begin("gtk.forms")); assert(!the_workbench_reset_owner("gtk.forms")); assert(!the_workbench_commit());
         sync_logs(); assert(!ui.logs->len);
         gtk_entry_set_text(GTK_ENTRY(ui.input), form_input_draft); g_free(form_input_draft); form_input_draft = NULL;
         assert_source("say 'GTK program ready'\npull name\nsay 'Hello' name\n"); assert(!the_editor_buffer_dirty(CURRENT_FILE));
         puts("GTK_FORMS_BURST_PROTECTED_LIMIT_SUBMIT_CANCEL_STALE_RETURN_PASS"); queue_command("nop");
         break;
      default: assert(0);
   }
}

static void filefields_step(void)
{
   const TheWorkbenchPanel *panel = the_workbench_panel_find("gtk.filefields");
   GtkLogPane *pane = panel ? find_log(panel->id, panel->generation) : NULL;
   const TheWorkbenchFileFieldsState *state = the_workbench_filefields_state("gtk.filefields");
   switch (filefields_probe_phase++)
   {
      case 0:
      {
         GString *fixture = g_string_new("!pName: !uGuest     !p LOCK Ω\n");
         unsigned i;
         for (i = 2; i <= 15; i++) g_string_append_printf(fixture, "!pRow %02u !uvalue%02u   !p locked\n", i, i);
         g_snprintf(filefields_probe_path, sizeof(filefields_probe_path), "%s/filefields.txt", getenv("THE_GTK_PROBE_DIR"));
         assert(g_file_set_contents(filefields_probe_path, fixture->str, fixture->len, NULL)); g_string_free(fixture, TRUE);
         open_file(filefields_probe_path); break;
      }
      case 1: queue_command("set ctlchar ! escape"); break;
      case 2: queue_command("set ctlchar p protect white"); break;
      case 3: queue_command("set ctlchar u noprotect green"); break;
      case 4: queue_command("set filectlchar on"); break;
      case 5:
      {
         TheWorkbenchPanel value = {0};
         strcpy(value.id, "gtk.filefields"); strcpy(value.title, "CTLCHAR file fields / Ω");
         value.kind = THE_WORKBENCH_PANEL_FIELDS; value.document_id = the_editor_document_active();
         the_editor_filefields_initialize();
         assert(!the_workbench_begin("gtk.filefields.owner")); assert(!the_workbench_put_panel(&value)); assert(!the_workbench_commit()); sync_logs();
         panel = the_workbench_panel_find(value.id); pane = find_log(panel->id, panel->generation);
         filefields_input_draft = g_strdup(gtk_entry_get_text(GTK_ENTRY(ui.input)));
         gtk_entry_set_text(GTK_ENTRY(ui.input), "FILEFIELDS Program draft"); gtk_widget_grab_focus(ui.input);
         gtk_editable_select_region(GTK_EDITABLE(ui.input), 12, 4); the_tool_program_console_visibility(1, 1);
         assert(queue_panel(pane, THE_WORKBENCH_PANEL_SHOW)); break;
      }
      case 6:
         assert(gtk_window_get_focus(GTK_WINDOW(ui.window)) == ui.input);
         resize_window(1366, GTK_ORIENTATION_HORIZONTAL); assert(queue_panel(pane, THE_WORKBENCH_PANEL_FOCUS)); break;
      case 7:
      {
         GtkFileField *field = g_ptr_array_index(pane->filefields, 0);
         int position = 0;
         if (gtk_window_get_focus(GTK_WINDOW(ui.window)) != field->entry)
         {
            GtkWidget *actual = gtk_window_get_focus(GTK_WINDOW(ui.window));
            fprintf(stderr, "GTK filefields initial focus actual=%s retained=%s page=%d expected-page=%d\n",
               actual ? G_OBJECT_TYPE_NAME(actual) : "none", pane->filefields_focus ? G_OBJECT_TYPE_NAME(pane->filefields_focus) : "none",
               gtk_notebook_get_current_page(GTK_NOTEBOOK(ui.auxiliary)), gtk_notebook_page_num(GTK_NOTEBOOK(ui.auxiliary), pane->page));
         }
         assert(gtk_window_get_focus(GTK_WINDOW(ui.window)) == field->entry);
         assert(state->records == 15 && pane->filefields->len == 15);
         gtk_editable_insert_text(GTK_EDITABLE(field->entry), "界", -1, &position);
         gtk_editable_insert_text(GTK_EDITABLE(field->entry), "é", -1, &position);
         gtk_editable_set_position(GTK_EDITABLE(field->entry), position);
         break;
      }
      case 8:
      {
         const TheWorkbenchFileFieldSpan *span = the_workbench_filefields_span(panel->id, 1);
         assert(span && span->editable && g_str_has_prefix(span->value, "界ést"));
         assert(state->dirty && !the_editor_buffer_dirty(CURRENT_FILE));
         assert(g_str_has_prefix((const char *)CURRENT_FILE->first_line->next->line, "!pName: !uGuest"));
         screenshot("filefields-right"); press_workbench_key(GDK_KEY_Escape, 0); break;
      }
      case 9:
      {
         int first, last;
         assert(gtk_window_get_focus(GTK_WINDOW(ui.window)) == ui.input);
         assert(!strcmp(gtk_entry_get_text(GTK_ENTRY(ui.input)), "FILEFIELDS Program draft"));
         assert(gtk_editable_get_selection_bounds(GTK_EDITABLE(ui.input), &first, &last) && first == 4 && last == 12);
         assert(queue_panel(pane, THE_WORKBENCH_PANEL_FOCUS)); break;
      }
      case 10:
      {
         GtkFileField *field = g_ptr_array_index(pane->filefields, 0);
         if (gtk_window_get_focus(GTK_WINDOW(ui.window)) != field->entry || gtk_editable_get_position(GTK_EDITABLE(field->entry)) != 2)
         {
            GtkWidget *actual = gtk_window_get_focus(GTK_WINDOW(ui.window));
            fprintf(stderr, "GTK filefields return focus actual=%s retained=%s entry-caret=%d saved-caret=%d saved-selected=%d\n",
               actual ? G_OBJECT_TYPE_NAME(actual) : "none", pane->filefields_focus ? G_OBJECT_TYPE_NAME(pane->filefields_focus) : "none",
               gtk_editable_get_position(GTK_EDITABLE(field->entry)), field->caret, field->selected);
         }
         assert(gtk_window_get_focus(GTK_WINDOW(ui.window)) == field->entry);
         assert(gtk_editable_get_position(GTK_EDITABLE(field->entry)) == 2);
         g_signal_emit_by_name(pane->form_submit, "clicked"); break;
      }
      case 11:
      {
         char *saved;
         unsigned i;
         assert(!state->dirty && the_editor_buffer_dirty(CURRENT_FILE));
         assert(g_str_has_prefix((const char *)CURRENT_FILE->first_line->next->line, "!pName: !u界ést"));
         assert(g_file_get_contents(filefields_probe_path, &saved, NULL, NULL)); assert(g_str_has_prefix(saved, "!pName: !uGuest")); g_free(saved);
         if (!filefields_below_pending)
         {
            TheInputEvent event;
            g_set_object(&filefields_resize_focus, gtk_window_get_focus(GTK_WINDOW(ui.window)));
            resize_window(900, GTK_ORIENTATION_VERTICAL);
            await_frame();
            /* The GTK allocation is ready; let the editor admit the queued
             * normalized resize before reading its grid or screenshot. */
            filefields_below_pending = 1; --filefields_probe_phase;
            assert(the_input_event_from_command("nop", &event) && queue_event(event));
            break;
         }
         filefields_below_pending = 0;
         if (gtk_window_get_focus(GTK_WINDOW(ui.window)) != filefields_resize_focus)
         {
            GtkWidget *actual = gtk_window_get_focus(GTK_WINDOW(ui.window));
            GtkAllocation allocation;
            int header_height;
            gtk_widget_get_allocation(pane->page, &allocation);
            gtk_widget_get_preferred_height(gtk_widget_get_parent(pane->status), &header_height, NULL);
            fprintf(stderr, "GTK filefields resize focus actual=%s previous=%s retained=%s scroller-visible=%d pane=%dx%d header=%d rebuild=%d status=%s\n",
               actual ? G_OBJECT_TYPE_NAME(actual) : "none", filefields_resize_focus ? G_OBJECT_TYPE_NAME(filefields_resize_focus) : "none",
               pane->filefields_focus ? G_OBJECT_TYPE_NAME(pane->filefields_focus) : "none", gtk_widget_get_visible(pane->scroller),
               allocation.width, allocation.height, header_height, pane->filefields_rebuild, gtk_label_get_text(GTK_LABEL(pane->status)));
         }
         assert(gtk_window_get_focus(GTK_WINDOW(ui.window)) == filefields_resize_focus);
         g_clear_object(&filefields_resize_focus);
         for (i = 0; i < 14; i++) press_workbench_key(GDK_KEY_Tab, 0);
         assert(gtk_window_get_focus(GTK_WINDOW(ui.window)) == ((GtkFileField *)g_ptr_array_index(pane->filefields, 14))->entry);
         await_frame();
         assert(gtk_adjustment_get_value(gtk_scrolled_window_get_vadjustment(GTK_SCROLLED_WINDOW(pane->scroller))) > 0);
         screenshot("filefields-below");
         g_signal_emit_by_name(pane->form_cancel, "clicked"); break;
      }
      case 12:
         assert(!state->active && !state->dirty);
         g_signal_emit_by_name(pane->filefields_reload, "clicked"); break;
      case 13:
      {
         GtkFileField *field = g_ptr_array_index(pane->filefields, 0);
         int position = 0;
         assert(state->active && !pane->filefields_error[0]);
         the_edit_group_changed();
         gtk_editable_insert_text(GTK_EDITABLE(field->entry), "Z", -1, &position);
         g_signal_emit_by_name(pane->form_submit, "clicked"); break;
      }
      case 14:
         assert(strstr(pane->filefields_error, "stale"));
         assert(g_str_has_prefix(the_workbench_filefields_span(panel->id, 1)->value, "Zést"));
         g_signal_emit_by_name(pane->filefields_reload, "clicked"); break;
      case 15:
         assert(!pane->filefields_error[0] && state->active);
         queue_command("save"); break;
      case 16:
      {
         char *saved;
         assert(g_file_get_contents(filefields_probe_path, &saved, NULL, NULL)); assert(g_str_has_prefix(saved, "!pName: !u界ést")); g_free(saved);
         g_signal_emit_by_name(pane->form_cancel, "clicked"); break;
      }
      case 17:
         assert(!the_workbench_begin("gtk.filefields.owner")); assert(!the_workbench_reset_owner("gtk.filefields.owner")); assert(!the_workbench_commit()); sync_logs();
         gtk_entry_set_text(GTK_ENTRY(ui.input), filefields_input_draft); g_free(filefields_input_draft); filefields_input_draft = NULL;
         queue_command("qquit"); break;
      case 18:
         assert_source("say 'GTK program ready'\npull name\nsay 'Hello' name\n");
         puts("GTK_FILEFIELDS_FIXED_CELLS_PROTECTED_APPLY_CANCEL_RELOAD_STALE_RETURN_SCROLL_SAVE_PASS");
         queue_command("nop"); break;
      default: assert(0);
   }
}

static char *output_text(GtkWidget *view)
{
   GtkTextBuffer *buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(view));
   GtkTextIter first, last;
   gtk_text_buffer_get_bounds(buffer, &first, &last);
   return gtk_text_buffer_get_text(buffer, &first, &last, FALSE);
}

static void assert_program_reader(void)
{
   GtkTextBuffer *buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(ui.output));
   GtkAdjustment *adjustment = gtk_scrolled_window_get_vadjustment(GTK_SCROLLED_WINDOW(ui.program_reader.scroller));
   GtkTextIter first, last;
   GString *lines = g_string_new(NULL);
   const TheToolConsole *program_console = the_tool_program_console();
   LogicalCursor cursor = CURRENT_VIEW->logical_cursor.current;
   int window = CURRENT_VIEW->current_window, start, end;
   int input_selected, input_start, input_end, input_position;
   int dirty = the_editor_buffer_dirty(CURRENT_FILE);
   char *draft = g_strdup(gtk_entry_get_text(GTK_ENTRY(ui.input)));
   double scroll;
   unsigned i;
   for (i = 0; i < 100; i++) g_string_append_printf(lines, "GTK retained output line %03u\n", i);
   the_tool_output(program_console->id, 1, lines->str, lines->len); g_string_free(lines, TRUE);
   sync_console(); await_frame();
   gtk_adjustment_set_value(adjustment, 100); await_frame();
   scroll = gtk_adjustment_get_value(adjustment);
   assert(scroll > 0 && !reader_at_tail(&ui.program_reader));
   gtk_entry_set_text(GTK_ENTRY(ui.input), "READER unsent draft");
   gtk_widget_grab_focus(ui.input); gtk_editable_select_region(GTK_EDITABLE(ui.input), 11, 3);
   the_tool_output(program_console->id, 2, "GTK unread without selection\n", strlen("GTK unread without selection\n"));
   sync_console(); await_frame();
   assert(gtk_adjustment_get_value(adjustment) >= scroll - ui.cell_height && gtk_adjustment_get_value(adjustment) <= scroll + ui.cell_height);
   assert(ui.program_reader.unread && strstr(gtk_button_get_label(GTK_BUTTON(ui.program_reader.latest)), "new lines"));
   assert(gtk_editable_get_selection_bounds(GTK_EDITABLE(ui.input), &start, &end) && start == 3 && end == 11);
   assert(gtk_editable_get_position(GTK_EDITABLE(ui.input)) == 3);
   gtk_text_buffer_get_iter_at_offset(buffer, &first, 3); gtk_text_buffer_get_iter_at_offset(buffer, &last, 9);
   gtk_text_buffer_select_range(buffer, &last, &first);
   /* Selecting output can transfer GTK's primary-selection ownership away
    * from the Entry. Appends must retain the resulting native Entry state. */
   await_frame();
   input_selected = gtk_editable_get_selection_bounds(GTK_EDITABLE(ui.input), &input_start, &input_end);
   input_position = gtk_editable_get_position(GTK_EDITABLE(ui.input));
   the_tool_output(program_console->id, 1, "GTK unread with selection\n", strlen("GTK unread with selection\n"));
   sync_console(); await_frame();
   assert(gtk_text_buffer_get_selection_bounds(buffer, &first, &last));
   assert(gtk_text_iter_get_offset(&first) == 3 && gtk_text_iter_get_offset(&last) == 9);
   assert(gtk_adjustment_get_value(adjustment) >= scroll - ui.cell_height && gtk_adjustment_get_value(adjustment) <= scroll + ui.cell_height);
   assert(gtk_window_get_focus(GTK_WINDOW(ui.window)) == ui.input);
   assert(!strcmp(gtk_entry_get_text(GTK_ENTRY(ui.input)), "READER unsent draft"));
   assert(gtk_editable_get_selection_bounds(GTK_EDITABLE(ui.input), &start, &end) == input_selected);
   if (input_selected) assert(start == input_start && end == input_end);
   assert(gtk_editable_get_position(GTK_EDITABLE(ui.input)) == input_position);
   assert(CURRENT_VIEW->current_window == window && !memcmp(&cursor, &CURRENT_VIEW->logical_cursor.current, sizeof(cursor)));
   assert(the_editor_buffer_dirty(CURRENT_FILE) == dirty);
   screenshot("program-reader-unread");
   for (i = 0; i < 2; i++)
   {
      int tail = gtk_text_buffer_get_char_count(buffer);
      GtkTextIter anchor, insert;
      gtk_text_buffer_get_iter_at_offset(buffer, &first, tail - 8); gtk_text_buffer_get_end_iter(buffer, &last);
      gtk_text_buffer_select_range(buffer, i ? &first : &last, i ? &last : &first);
      the_tool_output(program_console->id, 1, "GTK tail append\n", strlen("GTK tail append\n"));
      sync_console(); await_frame();
      gtk_text_buffer_get_iter_at_mark(buffer, &anchor, gtk_text_buffer_get_selection_bound(buffer));
      gtk_text_buffer_get_iter_at_mark(buffer, &insert, gtk_text_buffer_get_insert(buffer));
      assert(gtk_text_iter_get_offset(&anchor) == (i ? tail : tail - 8));
      assert(gtk_text_iter_get_offset(&insert) == (i ? tail - 8 : tail));
   }
   g_signal_emit_by_name(ui.program_reader.latest, "clicked"); await_frame();
   assert(!ui.program_reader.unread && reader_at_tail(&ui.program_reader));
   assert(!gtk_text_buffer_get_has_selection(buffer));
   assert(gtk_window_get_focus(GTK_WINDOW(ui.window)) == ui.input);
   {
      char *history = console_text(), *repeated = g_malloc(THE_TOOL_OUTPUT);
      GtkTextTag *stderr_tag = gtk_text_tag_table_lookup(gtk_text_buffer_get_tag_table(buffer), "stderr");
      GtkTextTag *stdout_tag = gtk_text_tag_table_lookup(gtk_text_buffer_get_tag_table(buffer), "stdout");
      GdkRectangle record_location;
      for (i = 0; i < THE_TOOL_OUTPUT; i++) repeated[i] = i % 64 == 63 ? '\n' : 'R';
      the_tool_clear_program(); the_tool_output(program_console->id, 1, repeated, THE_TOOL_OUTPUT);
      sync_console(); await_frame();
      gtk_text_buffer_get_iter_at_offset(buffer, &first, 200); gtk_text_buffer_get_iter_at_offset(buffer, &last, 210);
      gtk_text_buffer_select_range(buffer, &last, &first);
      gtk_adjustment_set_value(adjustment, 100); await_frame(); scroll = gtk_adjustment_get_value(adjustment);
      gtk_text_buffer_get_iter_at_offset(buffer, &first, 64);
      gtk_text_view_get_iter_location(GTK_TEXT_VIEW(ui.output), &first, &record_location);
      for (i = 1; i <= 2; i++)
      {
         the_tool_output(program_console->id, 1, repeated, 64); sync_console(); await_frame();
         assert(program_console->omitted && program_console->bytes == THE_TOOL_OUTPUT);
         assert(gtk_text_buffer_get_selection_bounds(buffer, &first, &last));
         assert(gtk_text_iter_get_offset(&first) == 200 - (int)i * 64 && gtk_text_iter_get_offset(&last) == 210 - (int)i * 64);
         assert(ui.program_reader.unread >= i);
         if (ABS(gtk_adjustment_get_value(adjustment) - MAX(0, scroll - i * record_location.y)) > ui.cell_height)
            fprintf(stderr, "GTK reader eviction %u scroll=%g expected=%g initial=%g record-y=%d cell-height=%d upper=%g page=%g pending=%lu restore=%g focus=%s\n",
               i, gtk_adjustment_get_value(adjustment), MAX(0, scroll - i * record_location.y), scroll,
               record_location.y, ui.cell_height, gtk_adjustment_get_upper(adjustment), gtk_adjustment_get_page_size(adjustment),
               ui.program_reader.scroll_handler, ui.program_reader.scroll_value,
               gtk_window_get_focus(GTK_WINDOW(ui.window)) ? G_OBJECT_TYPE_NAME(gtk_window_get_focus(GTK_WINDOW(ui.window))) : "none");
         assert(ABS(gtk_adjustment_get_value(adjustment) - MAX(0, scroll - i * record_location.y)) <= ui.cell_height);
      }
      {
         uint64_t displayed = program_console->displayed;
         unsigned unread_before = ui.program_reader.unread;
         the_tool_output(program_console->id, 1, "\342", 1);
         the_tool_program_console_visibility(1, 1); sync_console(); await_frame();
         assert(program_console->displayed == displayed && ui.program_reader.unread == unread_before);
         assert(gtk_text_buffer_get_selection_bounds(buffer, &first, &last));
         assert(gtk_text_iter_get_offset(&first) == 72 && gtk_text_iter_get_offset(&last) == 82);
         the_tool_output(program_console->id, 1, "\202\254", 2); sync_console(); await_frame();
         assert(program_console->displayed == displayed + 3);
         assert(gtk_text_buffer_get_selection_bounds(buffer, &first, &last));
         assert(gtk_text_iter_get_offset(&first) == 69 && gtk_text_iter_get_offset(&last) == 79);
      }
      the_tool_clear_program(); the_tool_output(program_console->id, 1, "GTK stream equal\n", strlen("GTK stream equal\n"));
      sync_console(); await_frame();
      gtk_text_buffer_get_start_iter(buffer, &first); assert(gtk_text_iter_has_tag(&first, stdout_tag));
      gtk_text_buffer_get_iter_at_offset(buffer, &first, 1); gtk_text_buffer_get_iter_at_offset(buffer, &last, 5);
      gtk_text_buffer_select_range(buffer, &last, &first);
      the_tool_clear_program(); the_tool_output(program_console->id, 2, "GTK stream equal\n", strlen("GTK stream equal\n"));
      sync_console(); await_frame();
      gtk_text_buffer_get_start_iter(buffer, &first);
      assert(gtk_text_iter_has_tag(&first, stderr_tag) && !gtk_text_iter_has_tag(&first, stdout_tag));
      {
         uint64_t epoch;
         gtk_text_buffer_get_iter_at_offset(buffer, &first, 1); gtk_text_buffer_get_iter_at_offset(buffer, &last, 5);
         gtk_text_buffer_select_range(buffer, &last, &first);
         the_tool_output(program_console->id, 2, "GTK stream equal\n", strlen("GTK stream equal\n"));
         sync_console(); await_frame(); assert(ui.program_reader.unread);
         epoch = program_console->history_epoch;
         the_tool_clear_program();
         the_tool_output(program_console->id, 2, "GTK stream equal\nGTK stream equal\n", strlen("GTK stream equal\nGTK stream equal\n"));
         sync_console(); await_frame();
         assert(program_console->history_epoch != epoch && ui.program_reader.history_epoch == program_console->history_epoch);
         assert(!gtk_text_buffer_get_has_selection(buffer) && !ui.program_reader.unread);
      }
      the_tool_clear_program(); the_tool_output(program_console->id, 1, history, strlen(history));
      sync_console(); g_signal_emit_by_name(ui.program_reader.latest, "clicked"); await_frame();
      assert(gtk_window_get_focus(GTK_WINDOW(ui.window)) == ui.input);
      assert(!strcmp(gtk_entry_get_text(GTK_ENTRY(ui.input)), "READER unsent draft"));
      g_free(history); g_free(repeated);
   }
   gtk_entry_set_text(GTK_ENTRY(ui.input), draft); g_free(draft);
   puts("GTK_PROGRAM_READER_SCROLL_SELECTION_UNREAD_LATEST_DRAFT_FOCUS_PASS");
   puts("GTK_READER_TAIL_SELECTION_STREAM_REPLACEMENT_EVICTION_UNREAD_PASS");
}

static void await_lane(int build, int good, const char *expected_text)
{
   gint64 deadline = g_get_monotonic_time() + 15000000;
   while (g_get_monotonic_time() < deadline)
   {
      const TheToolConsole *lane;
      char *text;
      int success;
      the_tools_native_poll(); sync_console(); sync_logs();
      lane = build ? the_tool_build_console() : the_tool_program_console();
      text = output_text(build ? ui.build_output : ui.output);
      success = !strncmp(lane->state, "Completed", 9) && !strcmp(lane->status, "0");
      if (!lane->running && lane->id && success == good && (!expected_text || strstr(text, expected_text)))
      { g_free(text); return; }
      g_free(text);
      while (g_main_context_pending(NULL)) g_main_context_iteration(NULL, FALSE);
      g_usleep(5000);
   }
   fprintf(stderr, "GTK %s lane timeout: %s\n", build ? "build" : "program", build ? the_tool_build_console()->state : the_tool_program_console()->state);
   assert(0);
}

static void assert_contextual_styles(void)
{
   GtkWidget *widgets[] = {ui.build, ui.run, ui.apply};
   size_t i;
   for (i = 0; i < THE_TOOLS_FILE_ACTION_COUNT; i++)
   {
      GValue value = G_VALUE_INIT;
      assert(gtk_widget_get_sensitive(widgets[i]) == ui.file_snapshot.actions[i].enabled);
      gtk_style_context_get_property(gtk_widget_get_style_context(widgets[i]), "opacity", GTK_STATE_FLAG_INSENSITIVE, &value);
      assert(G_VALUE_HOLDS_DOUBLE(&value) && g_value_get_double(&value) < .6);
      g_value_unset(&value);
      assert_widget_palette(widgets[i], GTK_STATE_FLAG_PRELIGHT, ATTR_CONSOLESELECTED - ATTR_CONSOLEBODY, 0);
   }
   puts("GTK_CONTEXTUAL_ACTIONS_DISABLED_SELECTED_PROFILE_STYLE_PASS");
}

static void assert_contextual_history(void)
{
   char *text = console_text();
   assert(the_tool_program_console()->id == contextual_program_id);
   assert(!strcmp(text, contextual_history)); g_free(text);
   assert(!strcmp(gtk_entry_get_text(GTK_ENTRY(ui.input)), contextual_draft));
}

static void assert_compiled_disk(void)
{
   char *bytes;
   gsize length;
   assert(g_file_get_contents(compiled_program, &bytes, &length, NULL));
   assert(length == sizeof(compiled_source) - 1 && !memcmp(bytes, compiled_source, length)); g_free(bytes);
}

static void contextual_step(void)
{
   TheToolsFileSnapshot *snapshot = &ui.file_snapshot;
   const char *directory = getenv("THE_GTK_PROBE_DIR");
   sync_console(); sync_logs();
   switch (contextual_phase++)
   {
      case 0:
      {
         GtkTextBuffer *buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(ui.output));
         GtkTextIter first, last;
         contextual_history = console_text(); contextual_program_id = the_tool_program_console()->id;
         gtk_entry_set_text(GTK_ENTRY(ui.input), "CONTEXT unsent draft");
         contextual_draft = g_strdup(gtk_entry_get_text(GTK_ENTRY(ui.input)));
         gtk_text_buffer_get_iter_at_offset(buffer, &first, 3); gtk_text_buffer_get_iter_at_offset(buffer, &last, 9);
         gtk_text_buffer_select_range(buffer, &last, &first);
         g_snprintf(compiled_program, sizeof(compiled_program), "%s/program.crexx", directory);
         g_snprintf(profile_program, sizeof(profile_program), "%s/User.profile.the", directory);
         open_file(compiled_program); break;
      }
      case 1:
         assert_source(compiled_source);
         assert(!strcmp(gtk_button_get_label(GTK_BUTTON(ui.build)), "Build source"));
         assert(gtk_widget_get_sensitive(ui.build) && !gtk_widget_get_sensitive(ui.run) && !gtk_widget_get_sensitive(ui.apply));
         assert(strstr(gtk_widget_get_tooltip_text(ui.run), "Build"));
         contextual_window = CURRENT_VIEW->current_window; contextual_cursor = CURRENT_VIEW->logical_cursor.current;
         g_signal_emit_by_name(ui.build, "clicked"); break;
      case 2:
      {
         GtkTextBuffer *buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(ui.output));
         GtkTextIter first, last;
         await_lane(1, 1, NULL);
         assert(gtk_widget_get_sensitive(ui.run) && !strcmp(snapshot->profile.build.state, "ready"));
         assert_contextual_history();
         assert(gtk_text_buffer_get_selection_bounds(buffer, &first, &last) && gtk_text_iter_get_offset(&first) == 3 && gtk_text_iter_get_offset(&last) == 9);
         assert(CURRENT_VIEW->current_window == contextual_window && !memcmp(&contextual_cursor, &CURRENT_VIEW->logical_cursor.current, sizeof(contextual_cursor)));
         screenshot("contextual-build-source");
         puts("GTK_BUILD_RESULT_PRESERVES_PROGRAM_HISTORY_SELECTION_DRAFT_CURSOR_PASS");
         g_signal_emit_by_name(ui.run, "clicked"); break;
      }
      case 3:
         await_lane(0, 1, "GTK compiled ready");
         g_free(contextual_history); contextual_history = console_text(); contextual_program_id = the_tool_program_console()->id;
         queue_command("cursor goto 2 1"); queue_command("replace say 'GTK compiled edited'");
         g_signal_emit_by_name(ui.run, "clicked"); break;
      case 4:
         assert(!strcmp(snapshot->result.code, "stale-source"));
         assert_contextual_history(); assert(!gtk_widget_get_sensitive(ui.run));
         assert(the_editor_buffer_dirty(CURRENT_FILE)); assert_compiled_disk();
         queue_command("replace this is invalid !!"); break;
      case 5:
         assert(gtk_widget_get_sensitive(ui.build)); g_signal_emit_by_name(ui.build, "clicked"); break;
      case 6:
         await_lane(1, 0, NULL);
         assert(!strcmp(snapshot->result.code, "failed") && !gtk_widget_get_sensitive(ui.run));
         assert_contextual_history(); assert_compiled_disk(); screenshot("contextual-failed-build");
         queue_command("replace say 'GTK compiled ready'"); queue_command("save"); break;
      case 7:
         assert_source(compiled_source); assert(!the_editor_buffer_dirty(CURRENT_FILE)); assert_compiled_disk();
         open_file(profile_program); break;
      case 8:
         assert_source(profile_source);
         assert(!strcmp(gtk_button_get_label(GTK_BUTTON(ui.build)), "Build profile"));
         assert(!strcmp(gtk_button_get_label(GTK_BUTTON(ui.apply)), "Apply profile"));
         assert(gtk_widget_get_sensitive(ui.build) && !gtk_widget_get_sensitive(ui.apply) && !gtk_widget_get_sensitive(ui.run));
         g_signal_emit_by_name(ui.build, "clicked"); break;
      case 9:
         await_lane(1, 1, NULL); assert(gtk_widget_get_sensitive(ui.apply)); assert_contextual_history();
         g_signal_emit_by_name(ui.apply, "clicked"); break;
      case 10:
      {
         char *text = output_text(ui.build_output);
         if (strcmp(snapshot->result.code, "activated"))
            fprintf(stderr, "GTK profile Apply result code=%s rc=%d message=%s source=%s active=%s build-state=%s\n",
               snapshot->result.code, snapshot->result.rc, snapshot->result.message, snapshot->path,
               snapshot->profile.active.image, snapshot->profile.build.state);
         assert(!strcmp(snapshot->result.code, "activated"));
         assert(g_str_has_suffix(snapshot->profile.active.image, ".profile.rxbin"));
         if (strcmp(snapshot->profile.active.source_path, profile_program))
            fprintf(stderr, "GTK profile source binding actual=%s expected=%s active-kind=%s\n",
               snapshot->profile.active.source_path, profile_program, snapshot->profile.active.kind);
         assert(!strcmp(snapshot->profile.active.source_path, profile_program));
         assert(strstr(text, "GTK profile applied")); g_free(text); assert_contextual_history();
         screenshot("contextual-profile-applied"); g_signal_emit_by_name(ui.edit_profile, "clicked"); break;
      }
      case 11:
         assert(!strcmp((char *)CURRENT_FILE->fname, "User.profile.the")); assert_source(profile_source);
         activate_shared_action("profile.action.15"); break;
      case 12:
      {
         char *bytes;
         gsize length;
         await_lane(1, 1, NULL);
         assert(!strcmp(gtk_label_get_text(GTK_LABEL(ui.build_state)), the_tool_build_console()->state));
         assert_contextual_history();
         activate_shared_action("profile.action.10");
         g_snprintf(imported_profile, sizeof(imported_profile), "%s/Imported.profile.rxbin", directory);
         assert(g_file_get_contents(snapshot->profile.active.image, &bytes, &length, NULL));
         assert(g_file_set_contents(imported_profile, bytes, length, NULL)); g_free(bytes);
         assert(queue_profile_image(imported_profile)); break;
      }
      case 13:
         assert(!the_tool_program_console()->visible && !gtk_widget_get_visible(ui.console));
         activate_shared_action("profile.action.9");
         assert(!strcmp(snapshot->result.code, "applied"));
         assert(!strcmp(snapshot->profile.active.kind, "compiled-image") && !*snapshot->profile.active.source_path);
         assert(strcmp(snapshot->profile.active.image, imported_profile));
         g_strlcpy(contextual_active, snapshot->profile.active.image, sizeof(contextual_active));
         g_snprintf(imported_profile, sizeof(imported_profile), "%s/Invalid.profile.rxbin", directory);
         assert(queue_profile_image(imported_profile)); break;
      case 14:
         assert(the_tool_program_console()->visible && gtk_widget_get_visible(ui.console));
         assert(!strcmp(snapshot->result.code, "invalid-image"));
         assert(!strcmp(snapshot->profile.active.image, contextual_active)); assert_contextual_history();
         puts("GTK_CONTEXTUAL_CREXX_BUILD_RUN_STALE_FAILED_BUILD_REFUSAL_PASS");
         puts("GTK_CONTEXTUAL_PROFILE_BUILD_APPLY_EDIT_IMPORT_INVALID_RETAIN_PASS");
         puts("GTK_LEGACY_MENU_BUILD_TERMINAL_STATUS_PROGRAM_SHOW_HIDE_PASS");
         open_file(program); queue_command("cursor cmdline"); break;
      case 15:
         assert_source("say 'GTK program ready'\npull name\nsay 'Hello' name\n");
         assert_contextual_history(); g_free(contextual_history); g_free(contextual_draft);
         contextual_history = contextual_draft = NULL; queue_command("nop"); break;
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
         {
            sync_console();
            assert(gtk_widget_get_sensitive(ui.run) && !gtk_widget_get_sensitive(ui.build) && !gtk_widget_get_sensitive(ui.apply));
            assert_contextual_styles();
            g_signal_emit_by_name(ui.run, "clicked");
         }
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
         if (!reader_probe_done)
         {
            sync_logs(); await_frame();
            if (ui.rows != terminal_lines || ui.cols != terminal_cols)
            {
               TheInputEvent event;
               assert(the_input_event_from_command("nop", &event) && queue_event(event));
               --phase; break;
            }
            assert_program_reader(); reader_probe_done = 1;
         }
         if (log_probe_phase < 15) { named_log_step(); --phase; break; }
         if (document_probe_phase < 16) { document_step(); --phase; break; }
         if (form_probe_phase < 13) { form_step(); --phase; break; }
         if (filefields_probe_phase < 19) { filefields_step(); --phase; break; }
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
         g_snprintf(stdin_program, sizeof(stdin_program), "%s/stdin.rexx", directory);
         stdin_probe_phase = 1;
         open_file(stdin_program);
         break;
      case 14:
         if (stdin_probe_phase)
         {
            stdin_probe_step();
            --phase;
            break;
         }
         if (the_tools_backend_available() && contextual_phase < 16)
         { contextual_step(); --phase; break; }
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

static void probe_native_event(GdkEvent *event, gpointer data)
{
   (void)data;
   /* The probe supplies keys through real driver callbacks. Ignore unrelated
    * physical input into this temporary window while running on a desktop. */
   switch (event->type)
   {
      case GDK_KEY_PRESS: case GDK_KEY_RELEASE:
      case GDK_BUTTON_PRESS: case GDK_2BUTTON_PRESS: case GDK_3BUTTON_PRESS: case GDK_BUTTON_RELEASE:
      case GDK_MOTION_NOTIFY: case GDK_SCROLL: case GDK_ENTER_NOTIFY: case GDK_LEAVE_NOTIFY:
         return;
      default: gtk_main_do_event(event); break;
   }
}

static int probe_startup(const TheDriverStartupOptions *options, char *error, size_t capacity)
{
   int ready = startup(options, error, capacity);
   if (ready) gdk_event_handler_set(probe_native_event, NULL, NULL);
   return ready;
}

const TheDriverModuleLifecycle *the_driver_module_lifecycle(void)
{
   static TheDriverModuleLifecycle probe;
   probe = *gtk_probe_base_lifecycle();
   probe.start = probe_startup;
   return &probe;
}
