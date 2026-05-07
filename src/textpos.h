#ifndef THE_TEXTPOS_H
#define THE_TEXTPOS_H

#include <stddef.h>
#include <stdint.h>

#include "thedefs.h"

#define TEXT_INVALID_CODEPOINT UINT32_C(0xFFFD)

typedef enum
{
   TEXT_SNAP_BACKWARD = 0,
   TEXT_SNAP_FORWARD,
   TEXT_SNAP_NEAREST
} TextSnap;

typedef struct
{
   size_t byte_offset;
   size_t codepoint_index;
   size_t cluster_index;
   int cell_column;
} TextPos;

typedef struct
{
   LINETYPE line_number;
   TextPos text;
} FilePos;

typedef struct
{
   short row;
   short col;
} ScreenPos;

typedef struct
{
   FilePos file;
   ScreenPos screen;
} EditorPos;

typedef struct
{
   TextPos pos;
   uint32_t codepoint;
   size_t byte_length;
   int cell_width;
   int valid;
} TextCodepoint;

TextPos textpos_begin(void);
TextPos textpos_from_byte(const CHARTYPE *line, size_t len, size_t byte_offset);
TextPos textpos_from_codepoint(const CHARTYPE *line, size_t len, size_t codepoint_index);
TextPos textpos_from_cell(const CHARTYPE *line, size_t len, int cell_column, TextSnap snap);
TextPos textpos_next_codepoint(const CHARTYPE *line, size_t len, TextPos pos);
TextPos textpos_prev_codepoint(const CHARTYPE *line, size_t len, TextPos pos);
TextCodepoint textpos_codepoint_at(const CHARTYPE *line, size_t len, TextPos pos);
size_t textpos_count_codepoints(const CHARTYPE *line, size_t len);
int text_codepoint_cell_width(uint32_t codepoint);
size_t text_utf8_encode(uint32_t codepoint, CHARTYPE out[4]);

#endif
