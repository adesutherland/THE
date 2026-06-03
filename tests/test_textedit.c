#include <stdio.h>
#include <string.h>

#include "textedit.h"
#include "utfterm.h"

static int failures = 0;

static void expect_bytes(const char *name, const CHARTYPE *got,
                         LENGTHTYPE got_len, const CHARTYPE *want,
                         LENGTHTYPE want_len)
{
   if (got_len != want_len || memcmp(got, want, (size_t)want_len) != 0)
   {
      fprintf(stderr, "%s: byte mismatch got_len=%ld want_len=%ld\n",
              name, (long)got_len, (long)want_len);
      failures++;
   }
}

static void copy_bytes(CHARTYPE *dest, const CHARTYPE *src, LENGTHTYPE len)
{
   memcpy(dest, src, (size_t)len);
   dest[len] = '\0';
}

static void test_replace_starts_at_cluster_boundary(void)
{
#ifdef USE_UTF8PROC
   static const CHARTYPE flag[] = { 'A',
                                    0xF0, 0x9F, 0x87, 0xAC,
                                    0xF0, 0x9F, 0x87, 0xA7, 'B' };
   static const CHARTYPE want[] = { 'A', 'X', 'B' };
   CHARTYPE line[64];
   LENGTHTYPE len;

   copy_bytes(line, flag, sizeof(flag));
   len = textedit_replace_utf8(line, sizeof(flag), sizeof(line) - 1,
                               2, (const CHARTYPE *)"X", 1);
   expect_bytes("replace.flag.inside.cell", line, len, want, sizeof(want));
#endif
}

static void test_replace_does_not_split_combining_or_zwj(void)
{
#ifdef USE_UTF8PROC
   static const CHARTYPE combining[] = { 'A', 'e',
                                         0xCC, 0x81, 'B' };
   static const CHARTYPE zwj[] = { 'A',
                                   0xF0, 0x9F, 0x91, 0xA9,
                                   0xE2, 0x80, 0x8D,
                                   0xF0, 0x9F, 0x92, 0xBB, 'B' };
   static const CHARTYPE want_combining[] = { 'A', 'X', 'B' };
   static const CHARTYPE want_zwj[] = { 'A', 'Z', 'B' };
   CHARTYPE line[64];
   LENGTHTYPE len;

   copy_bytes(line, combining, sizeof(combining));
   len = textedit_replace_utf8(line, sizeof(combining), sizeof(line) - 1,
                               1, (const CHARTYPE *)"X", 1);
   expect_bytes("replace.combining.cluster", line, len,
                want_combining, sizeof(want_combining));

   copy_bytes(line, zwj, sizeof(zwj));
   len = textedit_replace_utf8(line, sizeof(zwj), sizeof(line) - 1,
                               1, (const CHARTYPE *)"Z", 1);
   expect_bytes("replace.zwj.cluster", line, len, want_zwj, sizeof(want_zwj));
#endif
}

static void test_insert_inside_cluster_uses_cluster_start(void)
{
#ifdef USE_UTF8PROC
   static const CHARTYPE flag[] = { 'A',
                                    0xF0, 0x9F, 0x87, 0xAC,
                                    0xF0, 0x9F, 0x87, 0xA7, 'B' };
   static const CHARTYPE want[] = { 'A', 'X',
                                    0xF0, 0x9F, 0x87, 0xAC,
                                    0xF0, 0x9F, 0x87, 0xA7, 'B' };
   CHARTYPE line[64];
   LENGTHTYPE len;

   copy_bytes(line, flag, sizeof(flag));
   len = textedit_insert_utf8(line, sizeof(flag), sizeof(line) - 1,
                              2, (const CHARTYPE *)"X", 1);
   expect_bytes("insert.flag.inside.cell", line, len, want, sizeof(want));
#endif
}

static void test_overlay_advances_by_logical_clusters(void)
{
#ifdef USE_UTF8PROC
   static const CHARTYPE keycap[] = { 'A', '1',
                                      0xEF, 0xB8, 0x8F,
                                      0xE2, 0x83, 0xA3, 'B' };
   static const CHARTYPE want[] = { 'A', '1',
                                    0xEF, 0xB8, 0x8F,
                                    0xE2, 0x83, 0xA3, 'Z' };
   CHARTYPE line[64];
   LENGTHTYPE len;

   copy_bytes(line, keycap, sizeof(keycap));
   len = textedit_overlay_utf8(line, sizeof(keycap), sizeof(line) - 1,
                               1, (const CHARTYPE *)" Z", 2);
   expect_bytes("overlay.skip.keycap.cluster", line, len, want, sizeof(want));
#endif
}

static void test_physical_profile_does_not_change_logical_replace(void)
{
#ifdef USE_UTF8PROC
   static const CHARTYPE keycap[] = { 'A', '1',
                                      0xEF, 0xB8, 0x8F,
                                      0xE2, 0x83, 0xA3, 'B' };
   static const CHARTYPE want[] = { 'A', 'X', 'B' };
   CHARTYPE line[64];
   LENGTHTYPE len;

   utf8_terminal_profile_reset();
   utf8_terminal_profile_apply_line(
      "SET UTF TERMINAL CLASS keycap WIDTH 2 ADVANCE 9 CURSOR 9 REPAINT 9");
   copy_bytes(line, keycap, sizeof(keycap));
   len = textedit_replace_utf8(line, sizeof(keycap), sizeof(line) - 1,
                               1, (const CHARTYPE *)"X", 1);
   expect_bytes("replace.ignores.physical.profile", line, len,
                want, sizeof(want));
#endif
}

static void test_physical_profile_does_not_change_logical_insert(void)
{
#ifdef USE_UTF8PROC
   static const CHARTYPE keycap[] = { 'A', '1',
                                      0xEF, 0xB8, 0x8F,
                                      0xE2, 0x83, 0xA3, 'B' };
   static const CHARTYPE want[] = { 'A', 'X', '1',
                                    0xEF, 0xB8, 0x8F,
                                    0xE2, 0x83, 0xA3, 'B' };
   CHARTYPE line[64];
   LENGTHTYPE len;

   utf8_terminal_profile_reset();
   utf8_terminal_profile_apply_line(
      "SET UTF TERMINAL CLASS keycap WIDTH 2 ADVANCE 9 CURSOR 9 REPAINT 9");
   copy_bytes(line, keycap, sizeof(keycap));
   len = textedit_insert_utf8(line, sizeof(keycap), sizeof(line) - 1,
                              1, (const CHARTYPE *)"X", 1);
   expect_bytes("insert.ignores.physical.profile", line, len,
                want, sizeof(want));
#endif
}

static void test_insert_after_eol_materializes_blanks(void)
{
   static const CHARTYPE want[] = { 'A', 'B', ' ', ' ', ' ', 'X' };
   CHARTYPE line[64];
   LENGTHTYPE len;

   copy_bytes(line, (const CHARTYPE *)"AB", 2);
   len = textedit_insert_utf8(line, 2, sizeof(line) - 1,
                              5, (const CHARTYPE *)"X", 1);
   expect_bytes("insert.virtual.eol.pads", line, len, want, sizeof(want));
}

static void test_replace_after_eol_materializes_blanks(void)
{
   static const CHARTYPE keycap[] = { 'A', '1',
                                      0xEF, 0xB8, 0x8F,
                                      0xE2, 0x83, 0xA3, 'B' };
   static const CHARTYPE want[] = { 'A', '1',
                                    0xEF, 0xB8, 0x8F,
                                    0xE2, 0x83, 0xA3, 'B',
                                    ' ', ' ', 'Z' };
   CHARTYPE line[64];
   LENGTHTYPE len;

   copy_bytes(line, keycap, sizeof(keycap));
   len = textedit_replace_utf8(line, sizeof(keycap), sizeof(line) - 1,
                               5, (const CHARTYPE *)"Z", 1);
   expect_bytes("replace.virtual.eol.pads", line, len, want, sizeof(want));
}

int main(void)
{
   test_replace_starts_at_cluster_boundary();
   test_replace_does_not_split_combining_or_zwj();
   test_insert_inside_cluster_uses_cluster_start();
   test_overlay_advances_by_logical_clusters();
   test_physical_profile_does_not_change_logical_replace();
   test_physical_profile_does_not_change_logical_insert();
   test_insert_after_eol_materializes_blanks();
   test_replace_after_eol_materializes_blanks();

   if (failures != 0)
   {
      fprintf(stderr, "textedit tests failed: %d\n", failures);
      return 1;
   }
   return 0;
}
