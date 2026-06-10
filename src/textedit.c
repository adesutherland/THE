#include "textedit.h"

#include <string.h>

static void terminate_line(CHARTYPE *line, LENGTHTYPE line_len,
                           LENGTHTYPE max_len)
{
   if (line != NULL && line_len >= 0 && line_len <= max_len)
      line[line_len] = '\0';
}

LENGTHTYPE textedit_safe_prefix_utf8(const CHARTYPE *text,
                                     LENGTHTYPE text_len,
                                     LENGTHTYPE max_bytes)
{
   TextPos pos;
   LENGTHTYPE safe_len = 0;

   if (text == NULL || text_len <= 0 || max_bytes <= 0)
      return 0;
   if (text_len <= max_bytes)
      return text_len;

   pos = textpos_begin();
   while (pos.byte_offset < (size_t)text_len)
   {
      TextCluster cluster = textpos_cluster_at_boundary(text, (size_t)text_len,
                                                        pos);

      if (cluster.byte_length == 0)
         break;
      if ((LENGTHTYPE)cluster.end.byte_offset > max_bytes)
         break;
      safe_len = (LENGTHTYPE)cluster.end.byte_offset;
      pos = cluster.end;
   }
   return safe_len;
}

static LENGTHTYPE logical_end_cell(const CHARTYPE *line, LENGTHTYPE line_len)
{
   TextPos end;

   if (line == NULL || line_len <= 0)
      return 0;
   end = textpos_from_byte(line, (size_t)line_len, (size_t)line_len);
   return (LENGTHTYPE)end.cell_column;
}

static LENGTHTYPE pad_to_logical_col(CHARTYPE *line, LENGTHTYPE line_len,
                                     LENGTHTYPE max_len,
                                     LENGTHTYPE logical_col)
{
   LENGTHTYPE end_col;

   if (line == NULL)
      return 0;
   if (line_len < 0)
      line_len = 0;
   if (line_len > max_len)
      line_len = max_len;
   if (logical_col < 0)
      logical_col = 0;

   end_col = logical_end_cell(line, line_len);
   while (end_col < logical_col && line_len < max_len)
   {
      line[line_len++] = ' ';
      end_col++;
   }
   terminate_line(line, line_len, max_len);
   return line_len;
}

static TextPos pos_from_logical_col(CHARTYPE *line, LENGTHTYPE line_len,
                                    LENGTHTYPE logical_col)
{
   if (logical_col < 0)
      logical_col = 0;
   return textpos_from_cell(line, (size_t)line_len, (int)logical_col,
                            TEXT_SNAP_BACKWARD);
}

static TextPos advance_clusters(const CHARTYPE *line, LENGTHTYPE line_len,
                                TextPos pos, size_t cluster_count)
{
   size_t i;

   for (i = 0; i < cluster_count && pos.byte_offset < (size_t)line_len; i++)
   {
      TextCluster cluster = textpos_cluster_at_boundary(line,
                                                        (size_t)line_len, pos);

      if (cluster.byte_length == 0)
         break;
      pos = cluster.end;
   }
   return pos;
}

static LENGTHTYPE replace_clusters_at(CHARTYPE *line, LENGTHTYPE line_len,
                                      LENGTHTYPE max_len,
                                      LENGTHTYPE logical_col,
                                      const CHARTYPE *text,
                                      LENGTHTYPE text_len,
                                      size_t delete_clusters)
{
   TextPos start;
   TextPos end;
   LENGTHTYPE start_byte;
   LENGTHTYPE end_byte;
   LENGTHTYPE delete_len;
   LENGTHTYPE insert_len;
   LENGTHTYPE available;
   LENGTHTYPE tail_len;
   LENGTHTYPE new_len;

   if (line == NULL || max_len <= 0)
      return 0;
   if (line_len < 0)
      line_len = 0;
   if (line_len > max_len)
      line_len = max_len;
   if (text == NULL)
      text_len = 0;
   if (text_len < 0)
      text_len = 0;

   line_len = pad_to_logical_col(line, line_len, max_len, logical_col);
   start = pos_from_logical_col(line, line_len, logical_col);
   end = advance_clusters(line, line_len, start, delete_clusters);
   start_byte = (LENGTHTYPE)start.byte_offset;
   end_byte = (LENGTHTYPE)end.byte_offset;
   delete_len = end_byte - start_byte;
   if (delete_len < 0)
      delete_len = 0;

   available = max_len - (line_len - delete_len);
   insert_len = textedit_safe_prefix_utf8(text, text_len, available);
   tail_len = line_len - end_byte;
   if (tail_len < 0)
      tail_len = 0;

   if (insert_len != delete_len && tail_len > 0)
   {
      memmove(line + start_byte + insert_len, line + end_byte,
              (size_t)tail_len);
   }
   if (insert_len > 0)
      memcpy(line + start_byte, text, (size_t)insert_len);

   new_len = line_len - delete_len + insert_len;
   terminate_line(line, new_len, max_len);
   return new_len;
}

LENGTHTYPE textedit_append_utf8(CHARTYPE *line, LENGTHTYPE line_len,
                                LENGTHTYPE max_len,
                                const CHARTYPE *text, LENGTHTYPE text_len)
{
   return replace_clusters_at(line, line_len, max_len,
                              logical_end_cell(line, line_len),
                              text, text_len, 0);
}

LENGTHTYPE textedit_insert_utf8(CHARTYPE *line, LENGTHTYPE line_len,
                                LENGTHTYPE max_len, LENGTHTYPE logical_col,
                                const CHARTYPE *text, LENGTHTYPE text_len)
{
   return replace_clusters_at(line, line_len, max_len, logical_col,
                              text, text_len, 0);
}

LENGTHTYPE textedit_replace_utf8(CHARTYPE *line, LENGTHTYPE line_len,
                                 LENGTHTYPE max_len, LENGTHTYPE logical_col,
                                 const CHARTYPE *text, LENGTHTYPE text_len)
{
   return replace_clusters_at(line, line_len, max_len, logical_col,
                              text, text_len,
                              textpos_count_clusters(text, (size_t)text_len));
}

static int cluster_is_single_ascii(const CHARTYPE *text, TextCluster cluster,
                                   CHARTYPE ch)
{
   return cluster.byte_length == 1
       && text[cluster.pos.byte_offset] == ch;
}

LENGTHTYPE textedit_overlay_utf8(CHARTYPE *line, LENGTHTYPE line_len,
                                 LENGTHTYPE max_len, LENGTHTYPE logical_col,
                                 const CHARTYPE *text, LENGTHTYPE text_len)
{
   static const CHARTYPE blank[] = { ' ' };
   TextPos pos;

   if (text == NULL || text_len <= 0)
      return line_len;
   if (logical_col < 0)
      logical_col = 0;

   pos = textpos_begin();
   while (pos.byte_offset < (size_t)text_len)
   {
      TextCluster cluster = textpos_cluster_at_boundary(text, (size_t)text_len,
                                                        pos);
      LENGTHTYPE advance;

      if (cluster.byte_length == 0)
         break;
      advance = (cluster.cell_width > 0) ? cluster.cell_width : 1;
      if (cluster_is_single_ascii(text, cluster, ' '))
      {
         logical_col += advance;
         pos = cluster.end;
         continue;
      }
      if (cluster_is_single_ascii(text, cluster, '_'))
      {
         line_len = replace_clusters_at(line, line_len, max_len, logical_col,
                                        blank, 1, 1);
         logical_col++;
         pos = cluster.end;
         continue;
      }

      line_len = replace_clusters_at(line, line_len, max_len, logical_col,
                                     text + cluster.pos.byte_offset,
                                     (LENGTHTYPE)cluster.byte_length, 1);
      logical_col += advance;
      pos = cluster.end;
   }
   return line_len;
}
