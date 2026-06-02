#include <stdio.h>

#include "thekeys.h"

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

static void test_basic_editing_keys(void)
{
   expect_int("key.none", THE_KEY_NONE, -1);
   expect_int("key.tab.ascii", THE_KEY_TAB, 0x09);
   expect_nonzero("key.shift-tab.distinct", THE_KEY_BTAB != THE_KEY_TAB);

   expect_int("key.return", THE_KEY_RETURN, THE_KEY_RETURN);
   expect_int("key.enter", THE_KEY_ENTER, 0x157);
#if defined(WIN32) \
 || defined(USE_XCURSES) || defined(USE_SDLCURSES) \
 || defined(USE_VTCURSES)
   expect_int("key.numenter", THE_KEY_NUMENTER, THE_KEY_PADENTER);
#else
   expect_int("key.numenter", THE_KEY_NUMENTER, THE_KEY_ENTER);
#endif
   expect_int("key.backspace", THE_KEY_BACKSPACE, 0x107);
   expect_int("key.bksp", THE_KEY_BKSP, 0x08);
}

static void test_arrows(void)
{
   expect_int("key.up", THE_KEY_UP, 0x103);
   expect_int("key.down", THE_KEY_DOWN, 0x102);
   expect_int("key.left", THE_KEY_LEFT, 0x104);
   expect_int("key.right", THE_KEY_RIGHT, 0x105);
   expect_int("key.home", THE_KEY_HOME, 0x106);
   expect_int("key.end", THE_KEY_END, 0x166);
   expect_int("key.pageup", THE_KEY_PPAGE, 0x153);
   expect_int("key.pagedown", THE_KEY_NPAGE, 0x152);

   expect_int("key.shift-home", THE_KEY_SHOME, 0x184);
   expect_int("key.shift-end", THE_KEY_SEND, 0x180);
   expect_int("key.shift-left", THE_KEY_SLEFT, 0x187);
   expect_int("key.shift-right", THE_KEY_SRIGHT, 0x190);
   expect_int("key.shift-up", THE_KEY_SUP, 0x217);
   expect_int("key.shift-down", THE_KEY_SDOWN, 0x218);
}

static void test_function_key_ranges(void)
{
   int i;

   for (i = 1; i <= 63; i++)
      expect_int("key.function.range", THE_KEY_F(i), THE_KEY_F0 + i);

   expect_int("key.f1", THE_KEY_F(1), 0x109);
   expect_int("key.f12", THE_KEY_F(12), 0x114);
   expect_int("key.shift-f1", THE_KEY_F(13), 0x115);
   expect_int("key.shift-f12", THE_KEY_F(24), 0x120);
   expect_int("key.control-f1", THE_KEY_F(25), 0x121);
   expect_int("key.control-f12", THE_KEY_F(36), 0x12c);
   expect_int("key.alt-f1", THE_KEY_F(37), 0x12d);
   expect_int("key.alt-f12", THE_KEY_F(48), 0x138);
   expect_int("key.f20", THE_KEY_F(56), 0x140);
   expect_int("key.shift-f19", THE_KEY_F(63), 0x147);
}

static void test_pf_and_keypad_aliases(void)
{
   expect_int("key.pf1", THE_KEY_PF1, 0x350);
   expect_int("key.pf2", THE_KEY_PF2, 0x351);
   expect_int("key.pf3", THE_KEY_PF3, 0x352);
   expect_int("key.pf4", THE_KEY_PF4, 0x353);

   expect_int("key.padcomma", THE_KEY_VT_PADCOMMA, 0x354);
   expect_int("key.padminus.vt", THE_KEY_VT_PADMINUS, 0x355);
   expect_int("key.padperiod", THE_KEY_VT_PADPERIOD, 0x356);
   expect_int("key.padplus.vt", THE_KEY_VT_PADPLUS, 0x357);
   expect_int("key.padstar.vt", THE_KEY_VT_PADSTAR, 0x358);
   expect_int("key.padslash.vt", THE_KEY_VT_PADSLASH, 0x359);
   expect_int("key.pad0.alias", THE_KEY_PAD0, 0x1fa);

   expect_int("key.padslash", THE_KEY_PADSLASH, 0x1ca);
   expect_int("key.padstop", THE_KEY_PADSTOP, 0x1ce);
   expect_int("key.padstar", THE_KEY_PADSTAR, 0x1cf);
   expect_int("key.padminus", THE_KEY_PADMINUS, 0x1d0);
   expect_int("key.padplus", THE_KEY_PADPLUS, 0x1d1);
   expect_int("key.pad0", THE_KEY_PAD0, 0x1fa);
}

static void test_alt_and_mouse_button_ranges(void)
{
   expect_int("key.alt-a", THE_KEY_ALT_A, 0x1a1);
   expect_int("key.alt-z", THE_KEY_ALT_Z, 0x1ba);
   expect_int("key.alt-0", THE_KEY_ALT_0, 0x197);
   expect_int("key.alt-9", THE_KEY_ALT_9, 0x1a0);
   expect_int("key.alt-minus", THE_KEY_ALT_MINUS, 0x1e4);
   expect_int("key.alt-bslash", THE_KEY_ALT_BSLASH, 0x210);
   expect_int("key.alt-bksp", THE_KEY_ALT_BKSP, 0x1f8);

   expect_int("key.press-button1", THE_KEY_PB1, 0x400);
   expect_int("key.shift-press-button3", THE_KEY_S_PB3, 0x405);
   expect_int("key.control-release-button2", THE_KEY_C_RB2, 0x417);
   expect_int("key.alt-double-button1", THE_KEY_A_DB1, 0x429);
}

static void test_logical_mouse_and_parser_keys(void)
{
   expect_int("key.mouse", THE_KEY_MOUSE, 0x1001);
   expect_int("key.parse-complete", THE_KEY_PARSE_COMPLETE,
              0x1000);
#ifdef USE_SDSLH
   expect_nonzero("key.parse-complete.sdslh", THE_KEY_PARSE_COMPLETE > THE_KEY_MAX);
#endif
}

int main(void)
{
   test_basic_editing_keys();
   test_arrows();
   test_function_key_ranges();
   test_pf_and_keypad_aliases();
   test_alt_and_mouse_button_ranges();
   test_logical_mouse_and_parser_keys();

   if (failures != 0)
   {
      fprintf(stderr, "key-code tests failed: %d\n", failures);
      return 1;
   }
   return 0;
}
