#include <stdio.h>
#include <string.h>

#include "utfinput.h"

static int failures = 0;

static void expect_int(const char *name, int got, int want)
{
   if (got != want)
   {
      fprintf(stderr, "%s: got %d want %d\n", name, got, want);
      failures++;
   }
}

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

static UtfInputParseStatus parse_status(const char *params)
{
   CHARTYPE out[64];
   LENGTHTYPE out_len = 0;
   UtfInputParseError error;

   if (utfinput_parse_command((const CHARTYPE *)params, out,
                              sizeof(out) - 1, &out_len, &error))
      return UTFINPUT_PARSE_OK;
   return error.status;
}

static void test_literal_codepoints(void)
{
   static const CHARTYPE want[] = { 'A', 0xE4, 0xB8, 0xAD, 'B' };
   CHARTYPE out[64];
   LENGTHTYPE out_len = 0;
   UtfInputParseError error;

   expect_int("parse.literal",
              utfinput_parse_command(
                 (const CHARTYPE *)"U+41 U+4E2D U+42",
                 out, sizeof(out) - 1, &out_len, &error), 1);
   expect_bytes("bytes.literal", out, out_len, want, sizeof(want));
}

static void test_chained_cluster(void)
{
   static const CHARTYPE want[] = { 0xF0, 0x9F, 0x87, 0xBA,
                                    0xF0, 0x9F, 0x87, 0xB8 };
   CHARTYPE out[64];
   LENGTHTYPE out_len = 0;
   UtfInputParseError error;

   expect_int("parse.chain",
              utfinput_parse_command(
                 (const CHARTYPE *)"CODES U+1F1FA+1F1F8",
                 out, sizeof(out) - 1, &out_len, &error), 1);
   expect_bytes("bytes.chain", out, out_len, want, sizeof(want));
}

static void test_rejections(void)
{
   expect_int("reject.method", parse_status("utf8 A"),
              UTFINPUT_PARSE_MALFORMED_CODE);
   expect_int("reject.empty", parse_status(""),
              UTFINPUT_PARSE_MISSING_CODE);
   expect_int("reject.empty.alias", parse_status("codes"),
              UTFINPUT_PARSE_MISSING_CODE);
   expect_int("reject.malformed", parse_status("U+"),
              UTFINPUT_PARSE_MALFORMED_CODE);
   expect_int("reject.empty.component", parse_status("U+41+"),
              UTFINPUT_PARSE_MALFORMED_CODE);
   expect_int("reject.range", parse_status("U+110000"),
              UTFINPUT_PARSE_RANGE);
   expect_int("reject.surrogate", parse_status("U+D800"),
              UTFINPUT_PARSE_SURROGATE);
   expect_int("reject.control", parse_status("U+0A"),
              UTFINPUT_PARSE_CONTROL);
}

int main(void)
{
   test_literal_codepoints();
   test_chained_cluster();
   test_rejections();
   return failures == 0 ? 0 : 1;
}
