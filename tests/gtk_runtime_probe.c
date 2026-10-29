/* Compile the production driver into a test-only module so this probe can
 * exercise its real callbacks without a production automation endpoint. */
#define the_driver_module_ops gtk_probe_base_operations
#include "../src/drivers/gtk/gtkdriver.c"
#undef the_driver_module_ops

#include <assert.h>
#include <stdio.h>
#include "editorsource.h"

static unsigned phase;
static char sample[4096], other[4096], program[4096];
static const char expected[] = "alpha\nbe\303\251ta\n";

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
   while (g_main_context_pending(NULL))
      g_main_context_iteration(NULL, FALSE);
   if (!g_queue_is_empty(&ui.inputs) || ui.rows != terminal_lines || ui.cols != terminal_cols)
      return read_input(event);
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
            tool_action(NULL, "run");
         else
         {
            assert(!getenv("THE_GTK_REQUIRE_PROGRAM"));
            puts("GTK_PROGRAM_CONSOLE_NOT_CONFIGURED");
            phase = 11;
            queue_command("cursor cmdline");
         }
         break;
      case 9:
         await_console(0);
         gtk_entry_set_text(GTK_ENTRY(ui.input), "Ada");
         send_input(ui.input, NULL);
         break;
      case 10:
         await_console(1);
         assert(gtk_widget_get_sensitive(ui.run));
         assert(!gtk_widget_get_sensitive(ui.stop));
         assert(CURRENT_FILE->save_alt == 0);
         assert_saved();
         puts("GTK_PROGRAM_OUTPUT_STDIN_COMPLETION_PASS");
         queue_command("cursor cmdline");
         break;
      case 11:
         assert(CURRENT_VIEW->current_window == WINDOW_COMMAND);
         resize_window(1366, GTK_ORIENTATION_HORIZONTAL);
         queue_command("cursor cmdline");
         break;
      case 12:
         assert_layout(GTK_ORIENTATION_HORIZONTAL);
         screenshot(NULL);
         resize_window(900, GTK_ORIENTATION_VERTICAL);
         queue_command("cursor cmdline");
         break;
      case 13:
         assert_layout(GTK_ORIENTATION_VERTICAL);
         screenshot("narrow");
         queue_command("cancel");
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
