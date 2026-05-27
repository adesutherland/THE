#include <stdio.h>
#include <string.h>

#include "headlessdriver.h"

static int failures = 0;

static void expect_int(const char *name, int got, int want)
{
   if (got != want)
   {
      fprintf(stderr, "%s: got %d want %d\n", name, got, want);
      failures++;
   }
}

static void expect_long(const char *name, long got, long want)
{
   if (got != want)
   {
      fprintf(stderr, "%s: got %ld want %ld\n", name, got, want);
      failures++;
   }
}

static void expect_ptr(const char *name, const void *got, const void *want)
{
   if (got != want)
   {
      fprintf(stderr, "%s: got %p want %p\n", name, got, want);
      failures++;
   }
}

static void expect_nonnull(const char *name, const void *got)
{
   if (got == NULL)
   {
      fprintf(stderr, "%s: got NULL\n", name);
      failures++;
   }
}

static void expect_str(const char *name, const char *got, const char *want)
{
   if (got == NULL || strcmp(got, want) != 0)
   {
      fprintf(stderr, "%s: got %s want %s\n", name,
              got == NULL ? "(null)" : got, want);
      failures++;
   }
}

static void test_vtable_complete(void)
{
   size_t count;
   size_t i;
   const void *const *ops;

   expect_long("ops.size.remainder",
               (long)(sizeof(TheDriverOps) % sizeof(void (*)(void))), 0);
   count = sizeof(TheDriverOps) / sizeof(void (*)(void));
   expect_long("ops.count", (long)count, 145);
   ops = (const void *const *)(const void *)&the_headless_driver_ops;
   for (i = 0; i < count; i++)
   {
      if (ops[i] == NULL)
      {
         fprintf(stderr, "ops[%zu] is NULL\n", i);
         failures++;
      }
   }
}

static void test_selection(void)
{
   expect_ptr("selection.initial", the_driver, NULL);
   expect_int("selection.curses.unlinked", the_driver_use_curses(), 0);
   expect_int("selection.headless", the_driver_use_headless(), 1);
   expect_ptr("selection.current", the_driver, &the_headless_driver_ops);
   the_driver_select(NULL);
   expect_ptr("selection.clear", the_driver, NULL);
   the_driver_select(&the_headless_driver_ops);
   expect_ptr("selection.explicit", the_driver, &the_headless_driver_ops);
}

static void test_fake_window_and_cursor_state(void)
{
   const TheDriverOps *ops = &the_headless_driver_ops;
   TheDriverWindow *win;
   TheDriverWindow *explicit_win;
   TheDriverWindow *global;
   TheDriverWindowSize size;
   TheDriverWindowOrigin origin;
   TheDriverWindowCursor cursor;
   TheDriverWindowCursor saved;
   TheDriverScreenPoint point;

   headless_driver_reset();
   headless_driver_set_current_screen(0);
   headless_driver_set_screen_current_role(0, 0);
   win = headless_driver_create_screen_role(0, 0, 3, 4, 5, 6);
   expect_nonnull("role.win", win);
   expect_int("role.current.exists", ops->current_window_exists(), 1);
   expect_int("role.current.is.filearea", ops->current_window_is_role(0), 1);
   expect_int("role.screen.exists", ops->screen_role_exists(0, 0), 1);
   expect_int("role.screen.is.filearea", ops->screen_window_is_role(0, 0), 1);

   size = ops->current_window_size();
   expect_int("role.size.valid", size.valid, 1);
   expect_int("role.size.rows", size.rows, 3);
   expect_int("role.size.cols", size.cols, 4);
   origin = ops->current_window_origin();
   expect_int("role.origin.valid", origin.valid, 1);
   expect_int("role.origin.row", origin.row, 5);
   expect_int("role.origin.col", origin.col, 6);

   ops->move_current_window_cursor(1, 2);
   cursor = ops->capture_current_window_cursor();
   expect_int("role.cursor.valid", cursor.valid, 1);
   expect_int("role.cursor.row", cursor.row, 1);
   expect_int("role.cursor.col", cursor.col, 2);
   point = ops->current_window_cursor_screen_point();
   expect_int("role.point.valid", point.valid, 1);
   expect_int("role.point.row", point.row, 6);
   expect_int("role.point.col", point.col, 8);

   saved = cursor;
   ops->move_current_window_cursor(2, 3);
   ops->restore_current_window_cursor(saved);
   cursor = ops->capture_current_window_cursor();
   expect_int("role.restore.row", cursor.row, 1);
   expect_int("role.restore.col", cursor.col, 2);

   ops->add_cell_at(win, 0, 1, 'X');
   ops->move_window_cursor(win, 0, 1);
   expect_int("role.read.cell", (int)ops->read_window_cell(win), 'X');

   explicit_win = ops->create_window(2, 5, 4, 7);
   expect_nonnull("explicit.win", explicit_win);
   size = ops->window_size(explicit_win);
   origin = ops->window_origin(explicit_win);
   expect_int("explicit.rows", size.rows, 2);
   expect_int("explicit.cols", size.cols, 5);
   expect_int("explicit.origin.row", origin.row, 4);
   expect_int("explicit.origin.col", origin.col, 7);
   ops->delete_window(explicit_win);

   global = headless_driver_create_global_window(THE_DRIVER_GLOBAL_ERROR, 2,
                                                 10, 20, 30);
   expect_nonnull("global.win", global);
   expect_int("global.exists",
              ops->global_window_exists(THE_DRIVER_GLOBAL_ERROR), 1);
   ops->move_global_window_cursor(THE_DRIVER_GLOBAL_ERROR, 1, 4);
   cursor = ops->capture_global_window_cursor(THE_DRIVER_GLOBAL_ERROR);
   expect_int("global.cursor.row", cursor.row, 1);
   expect_int("global.cursor.col", cursor.col, 4);
   ops->delete_global_window(THE_DRIVER_GLOBAL_ERROR);
   expect_int("global.deleted",
              ops->global_window_exists(THE_DRIVER_GLOBAL_ERROR), 0);
}

static void test_operation_log(void)
{
   const TheDriverOps *ops = &the_headless_driver_ops;
   TheDriverWindow *win;

   headless_driver_reset();
   headless_driver_set_current_screen(0);
   headless_driver_set_screen_current_role(0, 0);
   win = headless_driver_create_screen_role(0, 0, 3, 4, 1, 2);
   expect_nonnull("log.win", win);
   headless_driver_create_global_window(THE_DRIVER_GLOBAL_STATAREA, 1, 10,
                                        0, 0);
   headless_driver_clear_log();

   ops->touch_window(win);
   ops->refresh_window(win);
   ops->update();
   ops->touch_and_refresh_current_role(0);
   ops->touch_global_window(THE_DRIVER_GLOBAL_STATAREA);
   ops->refresh_global_window(THE_DRIVER_GLOBAL_STATAREA);

   expect_long("log.count", (long)headless_driver_log_count(), 6);
   expect_str("log.0", headless_driver_log_entry(0), "touch:window:1");
   expect_str("log.1", headless_driver_log_entry(1), "refresh:window:1");
   expect_str("log.2", headless_driver_log_entry(2), "update");
   expect_str("log.3", headless_driver_log_entry(3),
              "touch-refresh:current-role:0");
   expect_str("log.4", headless_driver_log_entry(4), "touch:global:0");
   expect_str("log.5", headless_driver_log_entry(5), "refresh:global:0");
}

static void test_input_and_mouse_fakes(void)
{
   const TheDriverOps *ops = &the_headless_driver_ops;
   TheDriverMouseEvent event;
   int row = -1;
   int col = -1;
   int button = 0;
   int action = 0;
   int modifier = 0;

   headless_driver_reset();
   headless_driver_set_current_screen(0);
   headless_driver_set_screen_current_role(0, 0);
   headless_driver_create_screen_role(0, 0, 3, 4, 5, 6);

   headless_driver_queue_key('A');
   headless_driver_queue_key(ops->mouse_key_code());
   expect_int("input.key", ops->read_current_window_key(), 'A');
   expect_int("input.mouse.key", ops->is_mouse_key(ops->read_standard_key()),
              1);
   expect_int("input.empty", ops->read_standard_key(), -1);

   headless_driver_set_mouse_position(6, 8);
   ops->mouse_position_for_screen_role(0, 0, &row, &col);
   expect_int("mouse.role.row", row, 1);
   expect_int("mouse.role.col", col, 2);
   ops->saved_mouse_position(&row, &col);
   expect_int("mouse.saved.row", row, 6);
   expect_int("mouse.saved.col", col, 8);
   ops->reset_mouse_position();
   ops->saved_mouse_position(&row, &col);
   expect_int("mouse.reset.row", row, -1);
   expect_int("mouse.reset.col", col, -1);

   headless_driver_set_mouse_position(6, 8);
   headless_driver_set_mouse_button(THE_DRIVER_MOUSE_BUTTON_PRESSED,
                                    THE_DRIVER_MOUSE_ACTION_PRESSED,
                                    THE_DRIVER_MOUSE_MODIFIER_SHIFT);
   expect_int("mouse.button.read",
              ops->read_mouse_button(&button, &action, &modifier), 1);
   expect_int("mouse.button", button, THE_DRIVER_MOUSE_BUTTON_PRESSED);
   expect_int("mouse.action", action, THE_DRIVER_MOUSE_ACTION_PRESSED);
   expect_int("mouse.modifier", modifier, THE_DRIVER_MOUSE_MODIFIER_SHIFT);
   expect_int("mouse.event.read",
              ops->read_current_role_mouse_event(0, &event), 1);
   expect_int("mouse.event.row", event.row, 1);
   expect_int("mouse.event.col", event.col, 2);
   expect_int("mouse.event.inside", event.inside, 1);
}

int main(void)
{
   test_vtable_complete();
   test_selection();
   test_fake_window_and_cursor_state();
   test_operation_log();
   test_input_and_mouse_fakes();
   headless_driver_reset();

   if (failures != 0)
   {
      fprintf(stderr, "headless driver tests failed: %d\n", failures);
      return 1;
   }
   return 0;
}
