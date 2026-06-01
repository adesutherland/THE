#include <stdio.h>

#include "thedriver.h"

static int failures = 0;

static int default_read_input_event(TheInputEvent *event)
{
   (void)event;
   return 0;
}

const TheDriverOps the_curses_driver_ops = {
   .read_input_event = default_read_input_event
};

const TheDriverModuleLifecycle the_curses_driver_lifecycle = {
   .name = "curses"
};

static void expect_int(const char *name, int got, int want)
{
   if (got != want)
   {
      fprintf(stderr, "%s: got %d want %d\n", name, got, want);
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

int main(void)
{
   expect_ptr("initial.no_driver_until_loaded", the_driver, NULL);
   the_driver_select(NULL);
   expect_ptr("select.clear", the_driver, NULL);
   expect_int("select.curses", the_driver_use_curses(), 1);
   expect_ptr("select.curses.ptr", the_driver, &the_curses_driver_ops);
   expect_int("select.headless.unlinked", the_driver_use_headless(), 0);

   if (failures != 0)
   {
      fprintf(stderr, "curses default selection tests failed: %d\n", failures);
      return 1;
   }
   return 0;
}
