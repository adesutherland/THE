#include <stdio.h>

#include "thedriver.h"

static int failures = 0;

static int default_clamp(int display_col, int window_cols)
{
   (void)window_cols;
   return display_col;
}

const TheDriverOps the_curses_driver_ops = {
   .clamp_display_col = default_clamp
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
   expect_ptr("default.curses", the_driver, &the_curses_driver_ops);
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
