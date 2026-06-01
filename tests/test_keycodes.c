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
   expect_int("key.tab", KEY_TAB, THE_KEY_TAB);
   expect_int("key.tab.ascii", THE_KEY_TAB, 0x09);
   expect_int("key.backtab", KEY_BTAB, THE_KEY_BTAB);
   expect_int("key.shift-tab.alias", KEY_S_TAB, THE_KEY_BTAB);
   expect_nonzero("key.shift-tab.distinct", KEY_S_TAB != KEY_TAB);

   expect_int("key.enter", KEY_ENTER, THE_KEY_ENTER);
   expect_int("key.return", KEY_RETURN, THE_KEY_RETURN);
   expect_int("key.numenter", KEY_NUMENTER, THE_KEY_NUMENTER);
   expect_int("key.padenter", PADENTER, THE_KEY_PADENTER);
   expect_int("key.backspace", KEY_BACKSPACE, THE_KEY_BACKSPACE);
   expect_int("key.bksp", KEY_BKSP, THE_KEY_BKSP);
}

static void test_arrows(void)
{
   expect_int("key.up", KEY_UP, THE_KEY_UP);
   expect_int("key.down", KEY_DOWN, THE_KEY_DOWN);
   expect_int("key.left", KEY_LEFT, THE_KEY_LEFT);
   expect_int("key.right", KEY_RIGHT, THE_KEY_RIGHT);
   expect_int("key.home", KEY_HOME, THE_KEY_HOME);
   expect_int("key.end", KEY_END, THE_KEY_END);
   expect_int("key.pageup", KEY_PPAGE, THE_KEY_PPAGE);
   expect_int("key.pagedown", KEY_NPAGE, THE_KEY_NPAGE);

   expect_int("key.shift-home", KEY_SHOME, THE_KEY_SHOME);
   expect_int("key.shift-end", KEY_SEND, THE_KEY_SEND);
   expect_int("key.shift-left", KEY_SLEFT, THE_KEY_SLEFT);
   expect_int("key.shift-right", KEY_SRIGHT, THE_KEY_SRIGHT);
   expect_int("key.shift-up", KEY_SUP, THE_KEY_SUP);
   expect_int("key.shift-down", KEY_SDOWN, THE_KEY_SDOWN);
   expect_int("key.shift-curu.alias", KEY_S_CURU, KEY_SUP);
   expect_int("key.shift-curd.alias", KEY_S_CURD, KEY_SDOWN);
   expect_int("key.shift-curl.alias", KEY_S_CURL, KEY_SLEFT);
   expect_int("key.shift-curr.alias", KEY_S_CURR, KEY_SRIGHT);
}

static void test_function_key_ranges(void)
{
   int i;

   for (i = 1; i <= 63; i++)
      expect_int("key.function.range", KEY_F(i), THE_KEY_F(i));

   expect_int("key.f1", KEY_F1, KEY_F(1));
   expect_int("key.f12", KEY_F12, KEY_F(12));
   expect_int("key.shift-f1", KEY_S_F1, KEY_F(13));
   expect_int("key.shift-f12", KEY_S_F12, KEY_F(24));
   expect_int("key.control-f1", KEY_C_F1, KEY_F(25));
   expect_int("key.control-f12", KEY_C_F12, KEY_F(36));
   expect_int("key.alt-f1", KEY_A_F1, KEY_F(37));
   expect_int("key.alt-f12", KEY_A_F12, KEY_F(48));
   expect_int("key.f20", KEY_F20, KEY_F(56));
   expect_int("key.shift-f19", KEY_S_F19, KEY_F(63));
}

static void test_pf_and_keypad_aliases(void)
{
   expect_int("key.pf1", KEY_PF1, THE_KEY_PF1);
   expect_int("key.pf2", KEY_PF2, THE_KEY_PF2);
   expect_int("key.pf3", KEY_PF3, THE_KEY_PF3);
   expect_int("key.pf4", KEY_PF4, THE_KEY_PF4);

   expect_int("key.padcomma", KEY_PadComma, THE_KEY_VT_PADCOMMA);
   expect_int("key.padminus.vt", KEY_PadMinus, THE_KEY_VT_PADMINUS);
   expect_int("key.padperiod", KEY_PadPeriod, THE_KEY_VT_PADPERIOD);
   expect_int("key.padplus.vt", KEY_PadPlus, THE_KEY_VT_PADPLUS);
   expect_int("key.padstar.vt", KEY_PadStar, THE_KEY_VT_PADSTAR);
   expect_int("key.padslash.vt", KEY_PadSlash, THE_KEY_VT_PADSLASH);
   expect_int("key.pad0.alias", KEY_Pad0, THE_KEY_PAD0);

   expect_int("key.padslash", PADSLASH, THE_KEY_PADSLASH);
   expect_int("key.padstop", PADSTOP, THE_KEY_PADSTOP);
   expect_int("key.padstar", PADSTAR, THE_KEY_PADSTAR);
   expect_int("key.padminus", PADMINUS, THE_KEY_PADMINUS);
   expect_int("key.padplus", PADPLUS, THE_KEY_PADPLUS);
   expect_int("key.pad0", PAD0, THE_KEY_PAD0);
}

static void test_alt_and_mouse_button_ranges(void)
{
   expect_int("key.alt-a", ALT_A, THE_KEY_ALT_A);
   expect_int("key.alt-z", ALT_Z, THE_KEY_ALT_Z);
   expect_int("key.alt-0", ALT_0, THE_KEY_ALT_0);
   expect_int("key.alt-9", ALT_9, THE_KEY_ALT_9);
   expect_int("key.alt-minus", ALT_MINUS, THE_KEY_ALT_MINUS);
   expect_int("key.alt-bslash", ALT_BSLASH, THE_KEY_ALT_BSLASH);
   expect_int("key.alt-bksp", ALT_BKSP, THE_KEY_ALT_BKSP);

   expect_int("key.press-button1", KEY_PB1, THE_KEY_PB1);
   expect_int("key.shift-press-button3", KEY_S_PB3, THE_KEY_S_PB3);
   expect_int("key.control-release-button2", KEY_C_RB2, THE_KEY_C_RB2);
   expect_int("key.alt-double-button1", KEY_A_DB1, THE_KEY_A_DB1);
}

static void test_logical_mouse_and_parser_keys(void)
{
   expect_int("key.mouse", KEY_MOUSE, THE_KEY_MOUSE);
   expect_int("key.parse-complete", KEY_PARSE_COMPLETE,
              THE_KEY_PARSE_COMPLETE);
#ifdef USE_SDSLH
   expect_nonzero("key.parse-complete.sdslh", KEY_PARSE_COMPLETE > KEY_MAX);
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
