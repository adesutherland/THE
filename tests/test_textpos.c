#include <stdio.h>

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

static void expect_int(const char *name, int got, int want)
{
   if (got != want)
   {
      fprintf(stderr, "%s: got %d want %d\n", name, got, want);
      failures++;
   }
}

static void expect_u32(const char *name, uint32_t got, uint32_t want)
{
   if (got != want)
   {
      fprintf(stderr, "%s: got U+%04X want U+%04X\n", name, got, want);
      failures++;
   }
}

static void expect_pos(const char *name, TextPos pos, size_t byte_offset,
                       size_t codepoint_index, int cell_column)
{
   char field[128];

   snprintf(field, sizeof(field), "%s.byte", name);
   expect_size(field, pos.byte_offset, byte_offset);
   snprintf(field, sizeof(field), "%s.codepoint", name);
   expect_size(field, pos.codepoint_index, codepoint_index);
   snprintf(field, sizeof(field), "%s.cluster", name);
   expect_size(field, pos.cluster_index, codepoint_index);
   snprintf(field, sizeof(field), "%s.cell", name);
   expect_int(field, pos.cell_column, cell_column);
}

static void expect_slice(const char *name, TextCellSlice slice,
                         size_t start_byte, size_t end_byte,
                         int leading_cells, int content_cells, int trailing_cells)
{
   char field[128];

   snprintf(field, sizeof(field), "%s.start", name);
   expect_size(field, slice.start.byte_offset, start_byte);
   snprintf(field, sizeof(field), "%s.end", name);
   expect_size(field, slice.end.byte_offset, end_byte);
   snprintf(field, sizeof(field), "%s.leading", name);
   expect_int(field, slice.leading_cells, leading_cells);
   snprintf(field, sizeof(field), "%s.content", name);
   expect_int(field, slice.content_cells, content_cells);
   snprintf(field, sizeof(field), "%s.trailing", name);
   expect_int(field, slice.trailing_cells, trailing_cells);
}

static void test_ascii(void)
{
   static const CHARTYPE s[] = { 'a', 'b', 'c' };
   expect_pos("ascii.byte1", textpos_from_byte(s, sizeof(s), 1), 1, 1, 1);
   expect_pos("ascii.cp2", textpos_from_codepoint(s, sizeof(s), 2), 2, 2, 2);
   expect_pos("ascii.cell2", textpos_from_cell(s, sizeof(s), 2, TEXT_SNAP_NEAREST), 2, 2, 2);
   expect_size("ascii.count", textpos_count_codepoints(s, sizeof(s)), 3);
}

static void test_accent_and_cjk(void)
{
   static const CHARTYPE s[] = { 'A', 0xC3, 0xA9, 0xE6, 0xBC, 0xA2, 'B' };
   TextCodepoint item;

   expect_pos("accent.cjk.cp1", textpos_from_codepoint(s, sizeof(s), 1), 1, 1, 1);
   item = textpos_codepoint_at(s, sizeof(s), textpos_from_codepoint(s, sizeof(s), 1));
   expect_u32("accent.cp", item.codepoint, 0x00E9u);
   expect_int("accent.width", item.cell_width, 1);

   expect_pos("accent.cjk.cp2", textpos_from_codepoint(s, sizeof(s), 2), 3, 2, 2);
   item = textpos_codepoint_at(s, sizeof(s), textpos_from_codepoint(s, sizeof(s), 2));
   expect_u32("cjk.cp", item.codepoint, 0x6F22u);
   expect_int("cjk.width", item.cell_width, 2);

   expect_pos("accent.cjk.end", textpos_from_byte(s, sizeof(s), sizeof(s)), 7, 4, 5);
}

static void test_emoji_snap(void)
{
   static const CHARTYPE s[] = { 'A', 0xF0, 0x9F, 0x98, 0x80, 'B' };
   TextCodepoint item;

   expect_pos("emoji.start", textpos_from_codepoint(s, sizeof(s), 1), 1, 1, 1);
   item = textpos_codepoint_at(s, sizeof(s), textpos_from_codepoint(s, sizeof(s), 1));
   expect_u32("emoji.cp", item.codepoint, 0x1F600u);
   expect_int("emoji.width", item.cell_width, 2);
   expect_pos("emoji.after", textpos_from_codepoint(s, sizeof(s), 2), 5, 2, 3);
   expect_pos("emoji.end", textpos_from_byte(s, sizeof(s), sizeof(s)), 6, 3, 4);
   expect_pos("emoji.midbyte", textpos_from_byte(s, sizeof(s), 3), 1, 1, 1);
   expect_pos("emoji.prev.midbyte", textpos_prev_codepoint(s, sizeof(s), textpos_from_byte(s, sizeof(s), 3)), 0, 0, 0);
   expect_pos("emoji.cell1.back", textpos_from_cell(s, sizeof(s), 1, TEXT_SNAP_BACKWARD), 1, 1, 1);
   expect_pos("emoji.cell1.forward", textpos_from_cell(s, sizeof(s), 1, TEXT_SNAP_FORWARD), 5, 2, 3);
   expect_pos("emoji.cell2.nearest", textpos_from_cell(s, sizeof(s), 2, TEXT_SNAP_NEAREST), 5, 2, 3);
}

static void test_cell_slices(void)
{
   static const CHARTYPE s[] = { 'A', 0xF0, 0x9F, 0x98, 0x80, 'B' };
   static const CHARTYPE combining[] = { 'e', 0xCC, 0x81, 'x' };

   expect_slice("slice.all", textpos_slice_cells(s, sizeof(s), 0, 4), 0, 6, 0, 4, 0);
   expect_slice("slice.before.clipped.emoji", textpos_slice_cells(s, sizeof(s), 0, 2), 0, 1, 0, 1, 1);
   expect_slice("slice.emoji.exact", textpos_slice_cells(s, sizeof(s), 1, 2), 1, 5, 0, 2, 0);
   expect_slice("slice.inside.emoji", textpos_slice_cells(s, sizeof(s), 2, 2), 5, 6, 1, 1, 0);
   expect_slice("slice.past.end", textpos_slice_cells(s, sizeof(s), 8, 3), 6, 6, 0, 0, 3);
   expect_slice("slice.combining", textpos_slice_cells(combining, sizeof(combining), 0, 1), 0, 3, 0, 1, 0);
}

static void test_combining_phase1_invariant(void)
{
   static const CHARTYPE s[] = { 'e', 0xCC, 0x81, 'x' };
   TextCodepoint item;

   expect_pos("combining.mark", textpos_from_codepoint(s, sizeof(s), 1), 1, 1, 1);
   item = textpos_codepoint_at(s, sizeof(s), textpos_from_codepoint(s, sizeof(s), 1));
   expect_u32("combining.cp", item.codepoint, 0x0301u);
   expect_int("combining.width", item.cell_width, 0);
   expect_pos("combining.after", textpos_from_codepoint(s, sizeof(s), 2), 3, 2, 1);
   expect_pos("combining.end", textpos_from_byte(s, sizeof(s), sizeof(s)), 4, 3, 2);
}

static void test_invalid_utf8_progress(void)
{
   static const CHARTYPE s[] = { 'A', 0xF0, 0x28, 0x8C, 0x28, 'B' };
   static const CHARTYPE bad_continuations[] = { 0xF0, 0x80, 0x80, 'x' };
   TextCodepoint item;

   item = textpos_codepoint_at(s, sizeof(s), textpos_from_codepoint(s, sizeof(s), 1));
   expect_u32("invalid.cp", item.codepoint, TEXT_INVALID_CODEPOINT);
   expect_size("invalid.byte_length", item.byte_length, 1);
   expect_int("invalid.valid", item.valid, 0);
   expect_pos("invalid.next", textpos_next_codepoint(s, sizeof(s), textpos_from_codepoint(s, sizeof(s), 1)), 2, 2, 2);
   expect_pos("invalid.cont.byte2", textpos_from_byte(bad_continuations, sizeof(bad_continuations), 2), 2, 2, 2);
   expect_pos("invalid.cont.prev", textpos_prev_codepoint(bad_continuations, sizeof(bad_continuations),
              textpos_from_byte(bad_continuations, sizeof(bad_continuations), 2)), 1, 1, 1);
}

static void test_encode(void)
{
   CHARTYPE out[4];
   expect_size("encode.emoji.len", text_utf8_encode(0x1F600u, out), 4);
   expect_int("encode.emoji.0", out[0], 0xF0);
   expect_int("encode.emoji.1", out[1], 0x9F);
   expect_int("encode.emoji.2", out[2], 0x98);
   expect_int("encode.emoji.3", out[3], 0x80);

   expect_size("encode.invalid.len", text_utf8_encode(0x110000u, out), 3);
   expect_int("encode.invalid.0", out[0], 0xEF);
   expect_int("encode.invalid.1", out[1], 0xBF);
   expect_int("encode.invalid.2", out[2], 0xBD);
}

int main(void)
{
   test_ascii();
   test_accent_and_cjk();
   test_emoji_snap();
   test_cell_slices();
   test_combining_phase1_invariant();
   test_invalid_utf8_progress();
   test_encode();

   if (failures != 0)
   {
      fprintf(stderr, "textpos tests failed: %d\n", failures);
      return 1;
   }

   return 0;
}
