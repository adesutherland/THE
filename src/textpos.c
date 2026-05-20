#include "textpos.h"

#ifdef USE_UTF8PROC
# include <utf8proc.h>
#endif

#define UTF8_CONTINUATION(c) ((((unsigned char)(c)) & 0xC0u) == 0x80u)

typedef struct
{
   uint32_t first;
   uint32_t last;
} CodepointRange;

static int codepoint_in_ranges(uint32_t codepoint, const CodepointRange *ranges, size_t count)
{
   size_t low = 0;
   size_t high = count;

   while (low < high)
   {
      size_t mid = low + (high - low) / 2;
      if (codepoint < ranges[mid].first)
         high = mid;
      else if (codepoint > ranges[mid].last)
         low = mid + 1;
      else
         return 1;
   }
   return 0;
}

static size_t clamp_byte_offset(size_t len, size_t byte_offset)
{
   if (byte_offset > len)
      byte_offset = len;
   return byte_offset;
}

static TextPos make_pos(size_t byte_offset, size_t codepoint_index, int cell_column)
{
   TextPos pos;
   pos.byte_offset = byte_offset;
   pos.codepoint_index = codepoint_index;
   pos.cluster_index = codepoint_index;
   pos.cell_column = cell_column;
   return pos;
}

static TextPos make_cluster_pos(size_t byte_offset, size_t codepoint_index,
                                size_t cluster_index, int cell_column)
{
   TextPos pos;
   pos.byte_offset = byte_offset;
   pos.codepoint_index = codepoint_index;
   pos.cluster_index = cluster_index;
   pos.cell_column = cell_column;
   return pos;
}

static TextPos make_virtual_pos(TextPos pos, size_t extra_cells)
{
   pos.codepoint_index += extra_cells;
   pos.cluster_index += extra_cells;
   pos.cell_column += (int)extra_cells;
   return pos;
}

static TextCodepoint decode_at_canonical(const CHARTYPE *line, size_t len, TextPos pos)
{
   TextCodepoint item;
   unsigned char b0;
   uint32_t codepoint = 0;
   size_t need = 0;
   int valid = 1;

   item.pos = pos;
   item.codepoint = 0;
   item.byte_length = 0;
   item.cell_width = 0;
   item.valid = 0;

   if (line == NULL || pos.byte_offset >= len)
      return item;

   b0 = (unsigned char)line[pos.byte_offset];
   if (b0 < 0x80u)
   {
      codepoint = b0;
      need = 1;
   }
   else if ((b0 & 0xE0u) == 0xC0u)
   {
      codepoint = b0 & 0x1Fu;
      need = 2;
   }
   else if ((b0 & 0xF0u) == 0xE0u)
   {
      codepoint = b0 & 0x0Fu;
      need = 3;
   }
   else if ((b0 & 0xF8u) == 0xF0u)
   {
      codepoint = b0 & 0x07u;
      need = 4;
   }
   else
   {
      valid = 0;
      need = 1;
      codepoint = TEXT_INVALID_CODEPOINT;
   }

   if (valid)
   {
      size_t i;
      if (pos.byte_offset + need > len)
         valid = 0;
      for (i = 1; valid && i < need; i++)
      {
         unsigned char bx = (unsigned char)line[pos.byte_offset + i];
         if (!UTF8_CONTINUATION(bx))
            valid = 0;
         else
            codepoint = (codepoint << 6) | (uint32_t)(bx & 0x3Fu);
      }
   }

   if (valid)
   {
      if ((need == 2 && codepoint < 0x80u)
      ||  (need == 3 && codepoint < 0x800u)
      ||  (need == 4 && codepoint < 0x10000u)
      ||  (codepoint >= 0xD800u && codepoint <= 0xDFFFu)
      ||  (codepoint > 0x10FFFFu))
      {
         valid = 0;
      }
   }

   if (!valid)
   {
      codepoint = TEXT_INVALID_CODEPOINT;
      need = 1;
   }

   item.codepoint = codepoint;
   item.byte_length = need;
   item.cell_width = text_codepoint_cell_width(codepoint);
   item.valid = valid;
   return item;
}

static TextPos advance_codepoint(TextPos pos, TextCodepoint item)
{
   pos.byte_offset += item.byte_length;
   pos.codepoint_index++;
   pos.cell_column += item.cell_width;
   return pos;
}

static int is_regional_indicator(uint32_t codepoint)
{
   return codepoint >= 0x1F1E6u && codepoint <= 0x1F1FFu;
}

static int grapheme_break_between(uint32_t left, uint32_t right, int *state, int regional_count)
{
#ifdef USE_UTF8PROC
   utf8proc_int32_t utf8proc_state = (utf8proc_int32_t)*state;
   int breaks = utf8proc_grapheme_break_stateful((utf8proc_int32_t)left,
                                                 (utf8proc_int32_t)right,
                                                 &utf8proc_state);
   *state = (int)utf8proc_state;
   return breaks;
#else
   if (text_codepoint_cell_width(right) == 0)
      return 0;
   if (left == 0x200Du)
      return 0;
   if (is_regional_indicator(left)
   &&  is_regional_indicator(right)
   &&  (regional_count % 2) == 1)
      return 0;
   return 1;
#endif
}

static TextCluster cluster_at_canonical(const CHARTYPE *line, size_t len, TextPos pos)
{
   TextCluster cluster;
   TextCodepoint item;
   TextPos end;
   uint32_t previous = 0;
   int state = 0;
   int regional_count = 0;
   int cell_width = 0;

   cluster.pos = pos;
   cluster.end = pos;
   cluster.byte_length = 0;
   cluster.codepoint_count = 0;
   cluster.cell_width = 0;
   cluster.valid = 0;

   item = decode_at_canonical(line, len, pos);
   if (item.byte_length == 0)
      return cluster;

   while (item.byte_length != 0)
   {
      if (cluster.codepoint_count != 0
      &&  grapheme_break_between(previous, item.codepoint, &state, regional_count))
         break;

      cluster.valid = cluster.valid || item.valid;
      cluster.byte_length += item.byte_length;
      cluster.codepoint_count++;
      cell_width += item.cell_width;
      previous = item.codepoint;

      if (is_regional_indicator(item.codepoint))
         regional_count++;
      else
         regional_count = 0;

      end = advance_codepoint(pos, item);
      pos = end;
      item = decode_at_canonical(line, len, pos);
   }

   cluster.cell_width = cell_width;
   cluster.end = make_cluster_pos(pos.byte_offset,
                                  pos.codepoint_index,
                                  cluster.pos.cluster_index + 1,
                                  cluster.pos.cell_column + cell_width);
   return cluster;
}

TextPos textpos_begin(void)
{
   return make_pos(0, 0, 0);
}

FilePos filepos_make(LINETYPE line_number, TextPos text)
{
   FilePos pos;
   pos.line_number = line_number;
   pos.text = text;
   return pos;
}

ScreenPos screenpos_make(short row, short col)
{
   ScreenPos pos;
   pos.row = row;
   pos.col = col;
   return pos;
}

EditorPos editorpos_make(LINETYPE line_number, TextPos text, short screen_row, short screen_col)
{
   EditorPos pos;
   pos.file = filepos_make(line_number, text);
   pos.screen = screenpos_make(screen_row, screen_col);
   return pos;
}

TextCodepoint textpos_codepoint_at(const CHARTYPE *line, size_t len, TextPos pos)
{
   pos = textpos_from_byte(line, len, pos.byte_offset);
   return decode_at_canonical(line, len, pos);
}

TextCodepoint textpos_codepoint_at_boundary(const CHARTYPE *line, size_t len, TextPos pos)
{
   return decode_at_canonical(line, len, pos);
}

TextPos textpos_from_byte(const CHARTYPE *line, size_t len, size_t byte_offset)
{
   TextPos pos = textpos_begin();
   size_t target = clamp_byte_offset(len, byte_offset);

   while (pos.byte_offset < target)
   {
      TextCodepoint item = decode_at_canonical(line, len, pos);
      TextPos next;
      if (item.byte_length == 0)
         break;

      next = pos;
      next.byte_offset += item.byte_length;
      next.codepoint_index++;
      next.cluster_index = next.codepoint_index;
      next.cell_column += item.cell_width;
      if (next.byte_offset > target)
         break;
      pos = next;
   }

   return pos;
}

TextPos textpos_from_codepoint(const CHARTYPE *line, size_t len, size_t codepoint_index)
{
   TextPos pos = textpos_begin();

   while (pos.byte_offset < len && pos.codepoint_index < codepoint_index)
   {
      TextCodepoint item = decode_at_canonical(line, len, pos);
      if (item.byte_length == 0)
         break;
      pos.byte_offset += item.byte_length;
      pos.codepoint_index++;
      pos.cluster_index = pos.codepoint_index;
      pos.cell_column += item.cell_width;
   }

   return pos;
}

TextPos textpos_from_codepoint_virtual(const CHARTYPE *line, size_t len, size_t codepoint_index)
{
   TextPos pos = textpos_from_codepoint(line, len, codepoint_index);

   if (pos.codepoint_index < codepoint_index)
      pos = make_virtual_pos(pos, codepoint_index - pos.codepoint_index);
   return pos;
}

TextPos textpos_next_codepoint(const CHARTYPE *line, size_t len, TextPos pos)
{
   TextCodepoint item;

   pos = textpos_from_byte(line, len, pos.byte_offset);
   item = decode_at_canonical(line, len, pos);
   if (item.byte_length == 0)
      return pos;

   pos.byte_offset += item.byte_length;
   pos.codepoint_index++;
   pos.cluster_index = pos.codepoint_index;
   pos.cell_column += item.cell_width;
   return pos;
}

TextPos textpos_prev_codepoint(const CHARTYPE *line, size_t len, TextPos pos)
{
   TextPos current = textpos_begin();
   TextPos previous = current;
   size_t target = clamp_byte_offset(len, pos.byte_offset);

   while (current.byte_offset < target)
   {
      TextCodepoint item = decode_at_canonical(line, len, current);
      TextPos next;
      if (item.byte_length == 0)
         break;

      next = current;
      next.byte_offset += item.byte_length;
      next.codepoint_index++;
      next.cluster_index = next.codepoint_index;
      next.cell_column += item.cell_width;
      if (next.byte_offset > target)
         break;
      previous = current;
      current = next;
   }

   return previous;
}

TextPos textpos_from_cell(const CHARTYPE *line, size_t len, int cell_column, TextSnap snap)
{
   TextPos pos = textpos_begin();

   if (cell_column <= 0)
      return pos;

   while (pos.byte_offset < len)
   {
      TextCluster cluster = cluster_at_canonical(line, len, pos);
      int start = pos.cell_column;
      int end = cluster.end.cell_column;

      if (cluster.byte_length == 0)
         break;

      if (cluster.cell_width == 0)
      {
         pos = cluster.end;
         continue;
      }

      if (cell_column < end)
      {
         if (snap == TEXT_SNAP_FORWARD)
            return cluster.end;
         if (snap == TEXT_SNAP_NEAREST
         &&  (cell_column - start) * 2 >= cluster.cell_width)
            return cluster.end;
         return pos;
      }

      pos = cluster.end;
   }

   return pos;
}

TextPos textpos_from_cell_virtual(const CHARTYPE *line, size_t len, int cell_column, TextSnap snap)
{
   TextPos pos = textpos_from_cell(line, len, cell_column, snap);

   if (cell_column > pos.cell_column)
      pos = make_virtual_pos(pos, (size_t)(cell_column - pos.cell_column));
   return pos;
}

TextCluster textpos_cluster_at(const CHARTYPE *line, size_t len, TextPos pos)
{
   pos = textpos_from_cell(line, len, pos.cell_column, TEXT_SNAP_BACKWARD);
   return cluster_at_canonical(line, len, pos);
}

TextCluster textpos_cluster_at_boundary(const CHARTYPE *line, size_t len, TextPos pos)
{
   return cluster_at_canonical(line, len, pos);
}

TextPos textpos_next_cluster(const CHARTYPE *line, size_t len, TextPos pos)
{
   TextCluster cluster;

   pos = textpos_from_cell(line, len, pos.cell_column, TEXT_SNAP_BACKWARD);
   cluster = cluster_at_canonical(line, len, pos);
   if (cluster.byte_length == 0)
      return pos;
   return cluster.end;
}

TextPos textpos_from_cluster(const CHARTYPE *line, size_t len, size_t cluster_index)
{
   TextPos pos = textpos_begin();

   while (pos.byte_offset < len && pos.cluster_index < cluster_index)
   {
      TextCluster cluster = cluster_at_canonical(line, len, pos);
      if (cluster.byte_length == 0)
         break;
      pos = cluster.end;
   }

   return pos;
}

TextPos textpos_from_cluster_virtual(const CHARTYPE *line, size_t len, size_t cluster_index)
{
   TextPos pos = textpos_from_cluster(line, len, cluster_index);

   if (pos.cluster_index < cluster_index)
      pos = make_virtual_pos(pos, cluster_index - pos.cluster_index);
   return pos;
}

TextPos textpos_prev_cluster(const CHARTYPE *line, size_t len, TextPos pos)
{
   TextPos current = textpos_begin();
   TextPos previous = current;
   size_t target = clamp_byte_offset(len, pos.byte_offset);

   while (current.byte_offset < target)
   {
      TextCluster cluster = cluster_at_canonical(line, len, current);
      if (cluster.byte_length == 0)
         break;
      if (cluster.end.byte_offset > target)
         break;
      previous = current;
      current = cluster.end;
   }

   return previous;
}

TextPos textpos_prev_cell_boundary(const CHARTYPE *line, size_t len, TextPos pos)
{
   TextPos end = textpos_from_cluster(line, len, (size_t)-1);

   if (pos.cell_column > end.cell_column)
      return textpos_from_cell_virtual(line, len, pos.cell_column - 1, TEXT_SNAP_BACKWARD);
   return textpos_prev_cluster(line, len, textpos_from_cell(line, len, pos.cell_column, TEXT_SNAP_BACKWARD));
}

TextCellSlice textpos_slice_cells(const CHARTYPE *line, size_t len, int start_cell, int width_cells)
{
   TextCellSlice slice;
   TextPos pos;
   int end_cell;

   slice.start = textpos_begin();
   slice.end = textpos_begin();
   slice.leading_cells = 0;
   slice.content_cells = 0;
   slice.trailing_cells = width_cells < 0 ? 0 : width_cells;

   if (width_cells <= 0)
      return slice;
   if (start_cell < 0)
      start_cell = 0;

   end_cell = start_cell + width_cells;
   pos = textpos_begin();

   while (pos.byte_offset < len)
   {
      TextCluster cluster = cluster_at_canonical(line, len, pos);
      int char_end;

      if (cluster.byte_length == 0)
         break;

      char_end = cluster.end.cell_column;

      if (cluster.cell_width > 0 && start_cell > pos.cell_column && start_cell < char_end)
      {
         slice.leading_cells = char_end - start_cell;
         pos = cluster.end;
         break;
      }
      if (char_end > start_cell || cluster.cell_width == 0)
         break;

      pos = cluster.end;
   }

   slice.start = pos;
   slice.end = pos;

   while (pos.byte_offset < len)
   {
      TextCluster cluster = cluster_at_canonical(line, len, pos);

      if (cluster.byte_length == 0)
         break;

      if (cluster.cell_width > 0 && cluster.end.cell_column > end_cell)
         break;

      slice.end = cluster.end;
      pos = cluster.end;
   }

   slice.content_cells = slice.end.cell_column - slice.start.cell_column;
   if (slice.content_cells < 0)
      slice.content_cells = 0;
   slice.trailing_cells = width_cells - slice.leading_cells - slice.content_cells;
   if (slice.trailing_cells < 0)
      slice.trailing_cells = 0;
   return slice;
}

size_t textpos_count_codepoints(const CHARTYPE *line, size_t len)
{
   return textpos_from_byte(line, len, len).codepoint_index;
}

size_t textpos_count_clusters(const CHARTYPE *line, size_t len)
{
   TextPos pos = textpos_begin();

   while (pos.byte_offset < len)
   {
      TextCluster cluster = cluster_at_canonical(line, len, pos);
      if (cluster.byte_length == 0)
         break;
      pos = cluster.end;
   }
   return pos.cluster_index;
}

int text_codepoint_cell_width(uint32_t codepoint)
{
#ifdef USE_UTF8PROC
   int width = utf8proc_charwidth((utf8proc_int32_t)codepoint);
   return (width < 0) ? 0 : width;
#endif
   static const CodepointRange combining[] = {
      {0x0300u, 0x036Fu}, {0x0483u, 0x0489u}, {0x0591u, 0x05BDu},
      {0x05BFu, 0x05BFu}, {0x05C1u, 0x05C2u}, {0x05C4u, 0x05C5u},
      {0x05C7u, 0x05C7u}, {0x0610u, 0x061Au}, {0x064Bu, 0x065Fu},
      {0x0670u, 0x0670u}, {0x06D6u, 0x06DCu}, {0x06DFu, 0x06E4u},
      {0x06E7u, 0x06E8u}, {0x06EAu, 0x06EDu}, {0x0711u, 0x0711u},
      {0x0730u, 0x074Au}, {0x07A6u, 0x07B0u}, {0x07EBu, 0x07F3u},
      {0x0816u, 0x0819u}, {0x081Bu, 0x0823u}, {0x0825u, 0x0827u},
      {0x0829u, 0x082Du}, {0x0859u, 0x085Bu}, {0x0898u, 0x089Fu},
      {0x08CAu, 0x08E1u}, {0x08E3u, 0x0902u}, {0x093Au, 0x093Au},
      {0x093Cu, 0x093Cu}, {0x0941u, 0x0948u}, {0x094Du, 0x094Du},
      {0x0951u, 0x0957u}, {0x0962u, 0x0963u}, {0x0981u, 0x0981u},
      {0x09BCu, 0x09BCu}, {0x09C1u, 0x09C4u}, {0x09CDu, 0x09CDu},
      {0x09E2u, 0x09E3u}, {0x09FEu, 0x09FEu}, {0x0A01u, 0x0A02u},
      {0x0A3Cu, 0x0A3Cu}, {0x0A41u, 0x0A42u}, {0x0A47u, 0x0A48u},
      {0x0A4Bu, 0x0A4Du}, {0x0A51u, 0x0A51u}, {0x0A70u, 0x0A71u},
      {0x0A75u, 0x0A75u}, {0x0A81u, 0x0A82u}, {0x0ABCu, 0x0ABCu},
      {0x0AC1u, 0x0AC5u}, {0x0AC7u, 0x0AC8u}, {0x0ACDu, 0x0ACDu},
      {0x0AE2u, 0x0AE3u}, {0x0AFAu, 0x0AFFu}, {0x0B01u, 0x0B01u},
      {0x0B3Cu, 0x0B3Cu}, {0x0B3Fu, 0x0B3Fu}, {0x0B41u, 0x0B44u},
      {0x0B4Du, 0x0B4Du}, {0x0B55u, 0x0B56u}, {0x0B62u, 0x0B63u},
      {0x0B82u, 0x0B82u}, {0x0BC0u, 0x0BC0u}, {0x0BCDu, 0x0BCDu},
      {0x0C00u, 0x0C00u}, {0x0C04u, 0x0C04u}, {0x0C3Cu, 0x0C3Cu},
      {0x0C3Eu, 0x0C40u}, {0x0C46u, 0x0C48u}, {0x0C4Au, 0x0C4Du},
      {0x0C55u, 0x0C56u}, {0x0C62u, 0x0C63u}, {0x0C81u, 0x0C81u},
      {0x0CBCu, 0x0CBCu}, {0x0CBFu, 0x0CBFu}, {0x0CC6u, 0x0CC6u},
      {0x0CCCu, 0x0CCDu}, {0x0CE2u, 0x0CE3u}, {0x0D00u, 0x0D01u},
      {0x0D3Bu, 0x0D3Cu}, {0x0D41u, 0x0D44u}, {0x0D4Du, 0x0D4Du},
      {0x0D62u, 0x0D63u}, {0x0D81u, 0x0D81u}, {0x0DCAu, 0x0DCAu},
      {0x0DD2u, 0x0DD4u}, {0x0DD6u, 0x0DD6u}, {0x0E31u, 0x0E31u},
      {0x0E34u, 0x0E3Au}, {0x0E47u, 0x0E4Eu}, {0x0EB1u, 0x0EB1u},
      {0x0EB4u, 0x0EBCu}, {0x0EC8u, 0x0ECEu}, {0x0F18u, 0x0F19u},
      {0x0F35u, 0x0F35u}, {0x0F37u, 0x0F37u}, {0x0F39u, 0x0F39u},
      {0x0F71u, 0x0F7Eu}, {0x0F80u, 0x0F84u}, {0x0F86u, 0x0F87u},
      {0x0F8Du, 0x0F97u}, {0x0F99u, 0x0FBCu}, {0x0FC6u, 0x0FC6u},
      {0x102Du, 0x1030u}, {0x1032u, 0x1037u}, {0x1039u, 0x103Au},
      {0x103Du, 0x103Eu}, {0x1058u, 0x1059u}, {0x105Eu, 0x1060u},
      {0x1071u, 0x1074u}, {0x1082u, 0x1082u}, {0x1085u, 0x1086u},
      {0x108Du, 0x108Du}, {0x109Du, 0x109Du}, {0x135Du, 0x135Fu},
      {0x1712u, 0x1714u}, {0x1732u, 0x1734u}, {0x1752u, 0x1753u},
      {0x1772u, 0x1773u}, {0x17B4u, 0x17B5u}, {0x17B7u, 0x17BDu},
      {0x17C6u, 0x17C6u}, {0x17C9u, 0x17D3u}, {0x17DDu, 0x17DDu},
      {0x180Bu, 0x180Fu}, {0x1885u, 0x1886u}, {0x18A9u, 0x18A9u},
      {0x1920u, 0x1922u}, {0x1927u, 0x1928u}, {0x1932u, 0x1932u},
      {0x1939u, 0x193Bu}, {0x1A17u, 0x1A18u}, {0x1A1Bu, 0x1A1Bu},
      {0x1A56u, 0x1A56u}, {0x1A58u, 0x1A5Eu}, {0x1A60u, 0x1A60u},
      {0x1A62u, 0x1A62u}, {0x1A65u, 0x1A6Cu}, {0x1A73u, 0x1A7Cu},
      {0x1A7Fu, 0x1A7Fu}, {0x1AB0u, 0x1AFFu}, {0x1B00u, 0x1B03u},
      {0x1B34u, 0x1B34u}, {0x1B36u, 0x1B3Au}, {0x1B3Cu, 0x1B3Cu},
      {0x1B42u, 0x1B42u}, {0x1B6Bu, 0x1B73u}, {0x1B80u, 0x1B81u},
      {0x1BA2u, 0x1BA5u}, {0x1BA8u, 0x1BA9u}, {0x1BABu, 0x1BADu},
      {0x1BE6u, 0x1BE6u}, {0x1BE8u, 0x1BE9u}, {0x1BEDu, 0x1BEDu},
      {0x1BEFu, 0x1BF1u}, {0x1C2Cu, 0x1C33u}, {0x1C36u, 0x1C37u},
      {0x1CD0u, 0x1CD2u}, {0x1CD4u, 0x1CE0u}, {0x1CE2u, 0x1CE8u},
      {0x1CEDu, 0x1CEDu}, {0x1CF4u, 0x1CF4u}, {0x1CF8u, 0x1CF9u},
      {0x1DC0u, 0x1DFFu}, {0x200Bu, 0x200Fu}, {0x200Du, 0x200Du},
      {0x202Au, 0x202Eu}, {0x2060u, 0x2064u}, {0x2066u, 0x206Fu},
      {0x20D0u, 0x20FFu}, {0xFE00u, 0xFE0Fu}, {0xFE20u, 0xFE2Fu},
      {0xE0100u, 0xE01EFu}
   };
   static const CodepointRange wide[] = {
      {0x1100u, 0x115Fu}, {0x231Au, 0x231Bu}, {0x2329u, 0x232Au},
      {0x23E9u, 0x23ECu}, {0x23F0u, 0x23F0u}, {0x23F3u, 0x23F3u},
      {0x25FDu, 0x25FEu}, {0x2614u, 0x2615u}, {0x2648u, 0x2653u},
      {0x267Fu, 0x267Fu}, {0x2693u, 0x2693u}, {0x26A1u, 0x26A1u},
      {0x26AAu, 0x26ABu}, {0x26BDu, 0x26BEu}, {0x26C4u, 0x26C5u},
      {0x26CEu, 0x26CEu}, {0x26D4u, 0x26D4u}, {0x26EAu, 0x26EAu},
      {0x26F2u, 0x26F3u}, {0x26F5u, 0x26F5u}, {0x26FAu, 0x26FAu},
      {0x26FDu, 0x26FDu}, {0x2705u, 0x2705u}, {0x270Au, 0x270Bu},
      {0x2728u, 0x2728u}, {0x274Cu, 0x274Cu}, {0x274Eu, 0x274Eu},
      {0x2753u, 0x2755u}, {0x2757u, 0x2757u}, {0x2795u, 0x2797u},
      {0x27B0u, 0x27B0u}, {0x27BFu, 0x27BFu}, {0x2B1Bu, 0x2B1Cu},
      {0x2B50u, 0x2B50u}, {0x2B55u, 0x2B55u}, {0x2E80u, 0xA4CFu},
      {0xAC00u, 0xD7A3u}, {0xF900u, 0xFAFFu}, {0xFE10u, 0xFE19u},
      {0xFE30u, 0xFE6Fu}, {0xFF00u, 0xFF60u}, {0xFFE0u, 0xFFE6u},
      {0x1F004u, 0x1F004u}, {0x1F0CFu, 0x1F0CFu}, {0x1F18Eu, 0x1F18Eu},
      {0x1F191u, 0x1F19Au}, {0x1F200u, 0x1F202u}, {0x1F210u, 0x1F23Bu},
      {0x1F240u, 0x1F248u}, {0x1F250u, 0x1F251u}, {0x1F300u, 0x1FAFFu},
      {0x20000u, 0x3FFFDu}
   };

   if (codepoint == 0)
      return 0;
   if (codepoint_in_ranges(codepoint, combining, sizeof(combining) / sizeof(combining[0])))
      return 0;
   if (codepoint_in_ranges(codepoint, wide, sizeof(wide) / sizeof(wide[0])))
      return 2;
   return 1;
}

size_t text_utf8_encode(uint32_t codepoint, CHARTYPE out[4])
{
   if (out == NULL)
      return 0;
   if ((codepoint >= 0xD800u && codepoint <= 0xDFFFu) || codepoint > 0x10FFFFu)
      codepoint = TEXT_INVALID_CODEPOINT;
   if (codepoint <= 0x7Fu)
   {
      out[0] = (CHARTYPE)codepoint;
      return 1;
   }
   if (codepoint <= 0x7FFu)
   {
      out[0] = (CHARTYPE)(0xC0u | (codepoint >> 6));
      out[1] = (CHARTYPE)(0x80u | (codepoint & 0x3Fu));
      return 2;
   }
   if (codepoint <= 0xFFFFu)
   {
      out[0] = (CHARTYPE)(0xE0u | (codepoint >> 12));
      out[1] = (CHARTYPE)(0x80u | ((codepoint >> 6) & 0x3Fu));
      out[2] = (CHARTYPE)(0x80u | (codepoint & 0x3Fu));
      return 3;
   }
   out[0] = (CHARTYPE)(0xF0u | (codepoint >> 18));
   out[1] = (CHARTYPE)(0x80u | ((codepoint >> 12) & 0x3Fu));
   out[2] = (CHARTYPE)(0x80u | ((codepoint >> 6) & 0x3Fu));
   out[3] = (CHARTYPE)(0x80u | (codepoint & 0x3Fu));
   return 4;
}
