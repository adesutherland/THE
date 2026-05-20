#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "textpos.h"

static int failures = 0;

static void expect_size(const char *name, size_t got, size_t want)
{
   if (got != want)
   {
      fprintf(stderr, "%s: got %zu want %zu\n", name, got, want);
      failures++;
   }
}

static unsigned char *read_file(const char *path, size_t *len)
{
   FILE *fp = fopen(path, "rb");
   long size;
   unsigned char *data;

   if (fp == NULL)
   {
      perror(path);
      return NULL;
   }
   if (fseek(fp, 0, SEEK_END) != 0)
   {
      perror("fseek");
      fclose(fp);
      return NULL;
   }
   size = ftell(fp);
   if (size < 0)
   {
      perror("ftell");
      fclose(fp);
      return NULL;
   }
   if (fseek(fp, 0, SEEK_SET) != 0)
   {
      perror("fseek");
      fclose(fp);
      return NULL;
   }
   data = (unsigned char *)malloc((size_t)size + 1u);
   if (data == NULL)
   {
      fclose(fp);
      return NULL;
   }
   if (fread(data, 1, (size_t)size, fp) != (size_t)size)
   {
      perror("fread");
      free(data);
      fclose(fp);
      return NULL;
   }
   data[size] = '\0';
   fclose(fp);
   *len = (size_t)size;
   return data;
}

static int contains_bytes(const unsigned char *haystack, size_t haystack_len,
                          const CHARTYPE *needle, size_t needle_len)
{
   size_t i;

   if (needle_len == 0 || needle_len > haystack_len)
      return 0;
   for (i = 0; i + needle_len <= haystack_len; i++)
   {
      if (memcmp(haystack + i, needle, needle_len) == 0)
         return 1;
   }
   return 0;
}

static void expect_contains(const char *name, const unsigned char *data,
                            size_t data_len, const CHARTYPE *needle,
                            size_t needle_len)
{
   if (!contains_bytes(data, data_len, needle, needle_len))
   {
      fprintf(stderr, "%s: fixture is missing required sample\n", name);
      failures++;
   }
}

static void validate_utf8(const CHARTYPE *data, size_t len)
{
   TextPos pos = textpos_begin();

   while (pos.byte_offset < len)
   {
      TextCodepoint item = textpos_codepoint_at(data, len, pos);
      if (item.byte_length == 0)
      {
         fprintf(stderr, "fixture decode stalled at byte %zu\n", pos.byte_offset);
         failures++;
         break;
      }
      if (!item.valid)
      {
         fprintf(stderr, "fixture contains invalid UTF-8 at byte %zu\n", pos.byte_offset);
         failures++;
      }
      pos = textpos_next_codepoint(data, len, pos);
   }
}

static void validate_required_clusters(const unsigned char *data, size_t data_len)
{
   static const CHARTYPE combining[] = { 'A', 'e', 0xCC, 0x81, 'B' };
   static const CHARTYPE flag[] = { 'A', 0xF0, 0x9F, 0x87, 0xBA,
                                    0xF0, 0x9F, 0x87, 0xB8, 'B' };
   static const CHARTYPE keycap[] = { 'A', '1',
                                      0xEF, 0xB8, 0x8F,
                                      0xE2, 0x83, 0xA3, 'B' };
   static const CHARTYPE zwj_2face[] = { 'A', 0xF0, 0x9F, 0x91, 0xA9,
                                         0xE2, 0x80, 0x8D,
                                         0xE2, 0x9D, 0xA4,
                                         0xEF, 0xB8, 0x8F,
                                         0xE2, 0x80, 0x8D,
                                         0xF0, 0x9F, 0x91, 0xA8, 'B' };
   static const CHARTYPE zwj_4face[] = { 'A', 0xF0, 0x9F, 0x91, 0xA8,
                                         0xE2, 0x80, 0x8D,
                                         0xF0, 0x9F, 0x91, 0xA9,
                                         0xE2, 0x80, 0x8D,
                                         0xF0, 0x9F, 0x91, 0xA7,
                                         0xE2, 0x80, 0x8D,
                                         0xF0, 0x9F, 0x91, 0xA6, 'B' };

   expect_contains("fixture.combining", data, data_len, combining, sizeof(combining));
   expect_contains("fixture.flag", data, data_len, flag, sizeof(flag));
   expect_contains("fixture.keycap", data, data_len, keycap, sizeof(keycap));
   expect_contains("fixture.zwj_2face", data, data_len, zwj_2face, sizeof(zwj_2face));
   expect_contains("fixture.zwj_4face", data, data_len, zwj_4face, sizeof(zwj_4face));

   expect_size("cluster.combining.count", textpos_count_clusters(combining, sizeof(combining)), 3);
   expect_size("cluster.flag.count", textpos_count_clusters(flag, sizeof(flag)), 3);
   expect_size("cluster.keycap.count", textpos_count_clusters(keycap, sizeof(keycap)), 3);
   expect_size("cluster.zwj_2face.count", textpos_count_clusters(zwj_2face, sizeof(zwj_2face)), 3);
   expect_size("cluster.zwj_4face.count", textpos_count_clusters(zwj_4face, sizeof(zwj_4face)), 3);
}

int main(int argc, char **argv)
{
   unsigned char *data;
   size_t len = 0;

   if (argc != 2)
   {
      fprintf(stderr, "usage: %s tests/fixtures/utf-render.txt\n", argv[0]);
      return 2;
   }

   data = read_file(argv[1], &len);
   if (data == NULL)
      return 2;

   validate_utf8((const CHARTYPE *)data, len);
   validate_required_clusters(data, len);
   free(data);

   if (failures != 0)
   {
      fprintf(stderr, "UTF-8 fixture tests failed: %d\n", failures);
      return 1;
   }
   return 0;
}
