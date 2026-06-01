#include <stdio.h>

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#if defined(USE_NCURSES)
# include <ncurses.h>
#elif defined(USE_EXTCURSES)
# include <cur00.h>
#elif defined(LOCAL_CURSES)
# include "curses_local.h"
#elif defined(USE_XCURSES)
# include <curses.h>
#else
# include <curses.h>
#endif

#include "thekeys.h"
#include "curseskeymap.h"

static int failures = 0;

static void expect_int(const char *name, int got, int want)
{
   if (got != want)
   {
      fprintf(stderr, "%s: got %d want %d\n", name, got, want);
      failures++;
   }
}

static void expect_nonzero(const char *name, int value)
{
   if (!value)
   {
      fprintf(stderr, "%s: expression was false\n", name);
      failures++;
   }
}

static int translate_physical_key(int key)
{
#define MAP_DRIVER_KEY(physical, logical) \
   do { if (key == (physical)) return (logical); } while (0);

   if (key >= KEY_F(1) && key <= KEY_F(63))
      return THE_KEY_F(key - KEY_F0);

   THE_CURSES_KEY_TRANSLATION_MAP(MAP_DRIVER_KEY)

#undef MAP_DRIVER_KEY
   return key;
}

static void test_complete_map_entries(void)
{
#define EXPECT_DEFINED(physical, logical) \
   do { \
      int physical_value = (physical); \
      int logical_value = (logical); \
      expect_nonzero("map.defined." #physical, \
                     physical_value >= 0 || logical_value >= 0); \
   } while (0);

   THE_CURSES_KEY_TRANSLATION_MAP(EXPECT_DEFINED)

#undef EXPECT_DEFINED
}

static void test_fragile_keys(void)
{
   expect_int("map.tab", translate_physical_key(KEY_TAB), THE_KEY_TAB);
   expect_int("map.shift-tab", translate_physical_key(KEY_BTAB),
              THE_KEY_BTAB);
   expect_int("map.back-tab.alias", translate_physical_key(KEY_S_TAB),
              THE_KEY_BTAB);
   expect_nonzero("map.shift-tab.not-tab",
                  translate_physical_key(KEY_BTAB)
                != translate_physical_key(KEY_TAB));
   if (THE_KEY_BTAB == KEY_C1)
      expect_nonzero("map.shift-tab.single-pass-required",
                     translate_physical_key(KEY_BTAB)
                   != translate_physical_key(THE_KEY_BTAB));

   expect_int("map.enter", translate_physical_key(KEY_ENTER),
              THE_KEY_ENTER);
   expect_int("map.return", translate_physical_key(KEY_RETURN),
              THE_KEY_RETURN);
   expect_int("map.numenter", translate_physical_key(KEY_NUMENTER),
              THE_KEY_NUMENTER);
   expect_int("map.padenter", translate_physical_key(PADENTER),
              THE_KEY_PADENTER);
   expect_int("map.backspace", translate_physical_key(KEY_BACKSPACE),
              THE_KEY_BACKSPACE);
   expect_int("map.bksp", translate_physical_key(KEY_BKSP),
              THE_KEY_BKSP);
}

static void test_arrows_and_function_ranges(void)
{
   int i;

   expect_int("map.up", translate_physical_key(KEY_UP), THE_KEY_UP);
   expect_int("map.down", translate_physical_key(KEY_DOWN), THE_KEY_DOWN);
   expect_int("map.left", translate_physical_key(KEY_LEFT), THE_KEY_LEFT);
   expect_int("map.right", translate_physical_key(KEY_RIGHT), THE_KEY_RIGHT);
   expect_int("map.shift-up", translate_physical_key(KEY_SUP), THE_KEY_SUP);
   expect_int("map.shift-down", translate_physical_key(KEY_SDOWN),
              THE_KEY_SDOWN);
   expect_int("map.shift-left", translate_physical_key(KEY_SLEFT),
              THE_KEY_SLEFT);
   expect_int("map.shift-right", translate_physical_key(KEY_SRIGHT),
              THE_KEY_SRIGHT);

   for (i = 1; i <= 63; i++)
      expect_int("map.function.range", translate_physical_key(KEY_F(i)),
                 THE_KEY_F(i));
}

static void test_logical_mouse_and_parser_keys(void)
{
   expect_int("map.mouse", translate_physical_key(KEY_MOUSE),
              THE_KEY_MOUSE);
   expect_int("map.parse-complete", translate_physical_key(KEY_PARSE_COMPLETE),
              THE_KEY_PARSE_COMPLETE);
}

int main(void)
{
   test_complete_map_entries();
   test_fragile_keys();
   test_arrows_and_function_ranges();
   test_logical_mouse_and_parser_keys();

   if (failures != 0)
   {
      fprintf(stderr, "curses key-map tests failed: %d\n", failures);
      return 1;
   }
   return 0;
}
