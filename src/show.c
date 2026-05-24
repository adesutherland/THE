/***********************************************************************/
/* SHOW.C - Functions involving displaying the data.                   */
/***********************************************************************/
/*
 * THE - The Hessling Editor. A text editor similar to VM/CMS xedit.
 * Copyright (C) 1991-2026 Mark Hessling
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to:
 *
 *    The Free Software Foundation, Inc.
 *    675 Mass Ave,
 *    Cambridge, MA 02139 USA.
 *
 *
 * If you make modifications to this software that you feel increases
 * it usefulness for the rest of the community, please email the
 * changes, enhancements, bug fixes as well as any and all ideas to me.
 * This software is going to be maintained and enhanced as deemed
 * necessary by the community.
 *
 * Mark Hessling, mark@rexx.org  http://www.rexx.org/
 */


/* NOTE: Most of the time of the program is spend for displaying the screen.
 *       The improvement of screen operations rely on the curses functionality
 *       and the used routines below.
 *       Therefore we use some ugly tricks to make the whole stuff as fast
 *       as possible. The fastest routine may be waddchnstr and friends.
 *       This routine is only found in newer curses variants. To hold the
 *       source readable and to improve the speed under this conditions, we
 *       use the global line buffers linebuf (char or unsigned char) and
 *       linebufch (chtype). Each buffer has max(COLS,THE_MAX_SCREEN_WIDTH)+1 elements. We
 *       use a module global loop variable which is reset by the
 *       INIT_LINE_OUTPUT macro. Each line part may be added by the
 *       ADD_LINE_OUTPUT or FILE_LINE_OUTPUT macros. These macros should be
 *       called with so many characters that a complete window line is EXACTLY
 *                                                                     =======
 *       filled in the sum. Then we call END_LINE_OUTPUT to do the displaying.
 *       The calling sequence
 *       for each line is:
 *        1.   INIT_LINE_OUTPUT
 *        2.   ADD_LINE_OUTPUT
 *       [3.   ADD_LINE_OUTPUT or FILL_LINE_OUTPUT
 *        ...
 *       [n.   ADD_LINE_OUTPUT or FILL_LINE_OUTPUT ]
 *        n+1. END_LINE_OUTPUT
 *       Be careful, no overflow checking of the buffers are performed.
 *       Pseudo prototypes of the macros:
 *       void INIT_LINE_OUTPUT(WINDOW *window,int line);
 *       void ADD_LINE_OUTPUT(CHARTYPE *string,int stringlength,chtype colour);
 *       void FILL_LINE_OUTPUT(CHARTYPE c,int filllength,chtype colour);
 *       void END_LINE_OUTPUT(void);
 *
 *       In case of doubt about your modifications try:
 *       #define PARANOIA_TEST
 *       This enables some tests of buffer overflow although you get the
 *       message after the buffer is corrupted in most cases.
 *       FGC
 */

#include <the.h>
#include <proto.h>

#if defined(USE_EXTCURSES)
# include <cur04.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "cursesdriver.h"
#ifdef USE_UTF8
# include <wchar.h>
# include "screenframe.h"
# include "utflayout.h"
# include "utfrepair.h"
# include "utfterm.h"
#endif

/*------------------------ function definitions -----------------------*/
static void build_lines(CHARTYPE,short,LINE *,short,short);
static void build_lines_for_display(CHARTYPE,short,short,short);
static void show_lines(CHARTYPE);
#ifdef USE_UTF8
static void show_a_line(CHARTYPE,short,SHOW_LINE *, const UiFrame *);
#else
static void show_a_line(CHARTYPE,short,SHOW_LINE *);
#endif
static void set_prefix_contents(CHARTYPE,LINE *,short,LINETYPE,bool);
static void show_hex_line(CHARTYPE,short);
static LINETYPE displayed_max_line_length = 0; /* max length of displayed line */
static LINE *hexshow_curr=NULL; /* module global for historical reasons? */

#if defined ( WIN32 )
# define __func__ __FUNCTION__
#endif

#ifdef DEBUG1
/* if you want to debug lots of detail in debug for UTF8 changes, change the DEBUGDUMPDETAIL to the same as DEBUGDUMP macro */
# define DEBUGDUMP(x) {x;}
# define DEBUGDUMPDETAIL(x) {}
#else
# define DEBUGDUMP(x) {}
# define DEBUGDUMPDETAIL(x) {}
#endif

#ifdef DEBUG_SYNTAX_HIGHLIGHTING
# define SHOW_HIGHLIGHTED_LINE(x,y,z) show_highlighted_line(x,y,z)
#else
# define SHOW_HIGHLIGHTED_LINE(x,y,z) {}
#endif

/* Make a chtype value from a character and a colour. This may be wrong if
 * the format of the chtype is incompatible. Please check this first if
 * you get strange results with your curses implementation.
 */
#ifndef USE_UTF8
# define make_chtype(ch,col) (etmode_flag[ch])?((chtype) etmode_table[ch]):(((chtype) etmode_table[ch]) | ((chtype) col))
#endif

/* Set up the paranoia test macros if wanted */
#ifdef PARANOIA_TEST
static int _fast_maxx = 0,_fast_pos;
# define PARATEST_INIT_LINE(win,line) {                                        \
                          if (line >= getmaxy(win))                           \
                          {                                                 \
                             fprintf(stderr,"\nINIT_LINE_OUTPUT in %s: "      \
                                                   "line %d doesn't exist\n", \
                                            __FILE__,line);                   \
                             exit(3);                                         \
                          }                                                 \
                          _fast_maxx = getmaxx(win);                          \
                          _fast_pos = 0; }
# define PARATEST_ADD_LINE(num,string) {                                       \
                         if (((_fast_pos += num) > _fast_maxx) || (num < 0))  \
                         {                                                  \
                            fprintf(stderr,"\n%s in %s: line overrun (%d,%d)\n",\
                                           string,__FILE__,_fast_pos-num,num);\
                            exit(3);                                          \
                         } }
#else
# define PARATEST_INIT_LINE(win,line)
# define PARATEST_ADD_LINE(num,string)
#endif

#ifdef USE_UTF8
static void show_codepoint_to_wchars(uint32_t ch, wchar_t wch[3])
{
   if ((ch >= 0xD800u && ch <= 0xDFFFu) || ch > 0x10FFFFu)
      ch = TEXT_INVALID_CODEPOINT;
# if defined(WCHAR_MAX) && WCHAR_MAX <= 0xFFFFu
   if (ch > 0xFFFFu)
   {
      ch -= 0x10000u;
      wch[0] = (wchar_t)(0xD800u + (ch >> 10));
      wch[1] = (wchar_t)(0xDC00u + (ch & 0x3FFu));
      wch[2] = L'\0';
      return;
   }
# endif
   wch[0] = (wchar_t)ch;
   wch[1] = L'\0';
}

static TextPos show_utf8_advance_codepoint_pos(TextPos pos, TextCodepoint item)
{
   if (item.byte_length == 0)
      return pos;
   pos.byte_offset += item.byte_length;
   pos.codepoint_index++;
   pos.cell_column += item.cell_width;
   return pos;
}

static int show_utf8_line_is_ascii(const CHARTYPE *line, size_t len)
{
   size_t i;

   if (line == NULL)
      return TRUE;
   for (i = 0; i < len; i++)
   {
      if (((unsigned char)line[i]) >= 0x80u)
         return FALSE;
   }
   return TRUE;
}

static int show_utf8_ascii_profile_fast_path_ok(void)
{
   const Utf8TerminalProfileEntry *entry;

   entry = utf8_terminal_profile_lookup(UTF8_TERM_CLASS_ASCII,
                                        UTF8_TERM_DISPLAY_NORMAL);
   return entry != NULL
       && entry->output_method == UTF8_TERM_OUTPUT_NATIVE
       && entry->layout_width == 1
       && entry->cursor_width == 1
       && entry->cursor_strategy == UTF8_TERM_STRATEGY_CHANGED_CELLS
       && entry->replacement_strategy == UTF8_TERM_STRATEGY_CHANGED_CELLS;
}

typedef struct
{
   LINETYPE line_number;
   CHARTYPE *line;
   LENGTHTYPE length;
} Utf8LineReplacementHint;

static Utf8LineReplacementHint utf8_line_replacement_hint = { -1L, NULL, 0 };

static void show_utf8_clear_line_replacement_hint(void)
{
   if (utf8_line_replacement_hint.line != NULL)
      (*the_free)(utf8_line_replacement_hint.line);
   utf8_line_replacement_hint.line = NULL;
   utf8_line_replacement_hint.length = 0;
   utf8_line_replacement_hint.line_number = -1L;
}

void show_utf8_note_line_replacement(LINETYPE line_number, const CHARTYPE *line,
                                     LENGTHTYPE length)
{
   show_utf8_clear_line_replacement_hint();
   if (line_number < 0L || line == NULL || length <= 0)
      return;
   if (show_utf8_line_is_ascii(line, (size_t)length)
   &&  show_utf8_ascii_profile_fast_path_ok())
      return;
   utf8_line_replacement_hint.line = (CHARTYPE *)(*the_malloc)((size_t)length);
   if (utf8_line_replacement_hint.line == NULL)
      return;
   memcpy(utf8_line_replacement_hint.line, line, (size_t)length);
   utf8_line_replacement_hint.length = length;
   utf8_line_replacement_hint.line_number = line_number;
}

static int show_utf8_line_replacement_hint_matches(SHOW_LINE *scurr)
{
   return utf8_line_replacement_hint.line != NULL
       && scurr != NULL
       && scurr->line_number == utf8_line_replacement_hint.line_number;
}

static int show_utf8_copy_status_text(char field[21], int offset, const char *text)
{
   int available;
   int len;
   int truncated = FALSE;

   if (offset < 0)
      offset = 0;
   if (offset >= 20)
      return TRUE;

   available = 20 - offset;
   len = (int)strlen(text);
   if (len > available)
   {
      len = available;
      truncated = TRUE;
   }
   if (len > 0)
      memcpy(field + offset, text, (size_t)len);
   if (truncated && available >= 3)
   {
      field[17] = '.';
      field[18] = '.';
      field[19] = '.';
   }
   field[20] = '\0';
   return truncated;
}

static const Utf8TerminalProfileEntry *show_utf8_cluster_profile(
   const CHARTYPE *line, size_t len, TextCluster cluster)
{
   return utf8_layout_cluster_profile(line, len, cluster);
}

static int show_utf8_cluster_logical_width(TextCluster cluster)
{
   return utf8_layout_cluster_logical_width(cluster);
}

static int show_utf8_cluster_display_width(const CHARTYPE *line, size_t len,
                                           TextCluster cluster)
{
   return utf8_layout_cluster_display_width(line, len, cluster);
}

static int show_utf8_cluster_cursor_width(const CHARTYPE *line, size_t len,
                                          TextCluster cluster)
{
   return utf8_layout_cluster_cursor_width(line, len, cluster);
}

static int show_utf8_cluster_paint_width(const CHARTYPE *line, size_t len,
                                         TextCluster cluster)
{
   return utf8_layout_cluster_paint_width(line, len, cluster);
}

int show_utf8_display_col_from_logical(const CHARTYPE *line, size_t len,
                                       int viewport_col, int logical_col)
{
   return utf8_layout_display_col_from_logical(line, len, viewport_col,
                                               logical_col);
}

int show_utf8_logical_col_from_display(const CHARTYPE *line, size_t len,
                                       int viewport_col, int display_col,
                                       TextSnap snap)
{
   return utf8_layout_logical_col_from_display(line, len, viewport_col,
                                               display_col, snap);
}

#ifndef CCHARW_MAX
# define CCHARW_MAX 5
#endif

static int show_cluster_to_wchars(const CHARTYPE *line, size_t len,
                                  TextCluster cluster, wchar_t wch[CCHARW_MAX])
{
   TextPos pos = cluster.pos;
   const Utf8TerminalProfileEntry *entry;
   int out = 0;
   int spacing_codepoints = 0;

   entry = show_utf8_cluster_profile(line, len, cluster);
   if (entry != NULL && entry->output_method == UTF8_TERM_OUTPUT_SUBSTITUTE)
   {
      show_codepoint_to_wchars(entry->substitute_codepoint, wch);
      return 1;
   }

   while (pos.byte_offset < cluster.end.byte_offset)
   {
      TextCodepoint item = textpos_codepoint_at_boundary(line, len, pos);
      wchar_t one[3];
      int i;

      if (item.byte_length == 0)
         break;
      if (entry != NULL
      &&  entry->output_method == UTF8_TERM_OUTPUT_EXPANDED
      &&  item.codepoint == 0x200Du)
      {
         pos = show_utf8_advance_codepoint_pos(pos, item);
         continue;
      }

      if (item.cell_width > 0)
      {
         spacing_codepoints++;
         if (spacing_codepoints > 1)
            return 0;
      }

      if (item.codepoint < 256 && etmode_flag[item.codepoint])
         item.codepoint = (uint32_t)etmode_table[(CHARTYPE)item.codepoint];

      show_codepoint_to_wchars(item.codepoint, one);
      for (i = 0; one[i] != L'\0'; i++)
      {
         if (out >= CCHARW_MAX - 1)
            return 0;
         wch[out++] = one[i];
      }
      pos = show_utf8_advance_codepoint_pos(pos, item);
   }

   if (out == 0)
      return 0;
   wch[out] = L'\0';
   return 1;
}

static int show_utf8_class_is_zwj(Utf8TerminalClass feature_class)
{
   return feature_class == UTF8_TERM_CLASS_SHORT_ZWJ
       || feature_class == UTF8_TERM_CLASS_HEART_ZWJ
       || feature_class == UTF8_TERM_CLASS_FAMILY_ZWJ;
}

static int show_status_cluster_force_expanded(const CHARTYPE *line, size_t len,
                                              TextCluster cluster)
{
   const Utf8TerminalProfileEntry *entry;
   Utf8TerminalClass feature_class;

   if (utf8_terminal_display_mode() != UTF8_TERM_DISPLAY_COMPONENTS)
      return FALSE;
   entry = show_utf8_cluster_profile(line, len, cluster);
   if (entry == NULL || entry->output_method != UTF8_TERM_OUTPUT_NATIVE)
      return FALSE;
   feature_class = utf8_terminal_classify_cluster(line, len, cluster);
   return show_utf8_class_is_zwj(feature_class);
}

static int show_cluster_to_wide_string(const CHARTYPE *line, size_t len,
                                       TextCluster cluster, wchar_t *wch,
                                       size_t wch_size,
                                       int force_expanded)
{
   TextPos pos = cluster.pos;
   const Utf8TerminalProfileEntry *entry;
   size_t out = 0;

   if (wch_size == 0)
      return 0;
   entry = show_utf8_cluster_profile(line, len, cluster);
   if (entry != NULL && entry->output_method == UTF8_TERM_OUTPUT_SUBSTITUTE)
   {
      if (wch_size < 2)
         return 0;
      show_codepoint_to_wchars(entry->substitute_codepoint, wch);
      return 1;
   }
   while (pos.byte_offset < cluster.end.byte_offset)
   {
      TextCodepoint item = textpos_codepoint_at_boundary(line, len, pos);
      wchar_t one[3];
      int i;

      if (item.byte_length == 0)
         break;
      if ((force_expanded
      ||   (entry != NULL
      &&    entry->output_method == UTF8_TERM_OUTPUT_EXPANDED))
      &&  item.codepoint == 0x200Du)
      {
         pos = show_utf8_advance_codepoint_pos(pos, item);
         continue;
      }
      if (item.codepoint < 256 && etmode_flag[item.codepoint])
         item.codepoint = (uint32_t)etmode_table[(CHARTYPE)item.codepoint];
      show_codepoint_to_wchars(item.codepoint, one);
      for (i = 0; one[i] != L'\0'; i++)
      {
         if (out >= wch_size - 1)
            return 0;
         wch[out++] = one[i];
      }
      pos = show_utf8_advance_codepoint_pos(pos, item);
   }
   if (out == 0)
      return 0;
   wch[out] = L'\0';
   return 1;
}

static void show_write_utf8_cluster_at(WINDOW *win, int row, int col,
                                       const CHARTYPE *line, size_t len,
                                       TextCluster cluster, chtype colour,
                                       int expected_width)
{
   wchar_t wch[THE_MAX_SCREEN_WIDTH + 1];

   if (show_cluster_to_wide_string(line, len, cluster, wch,
                                   sizeof(wch) / sizeof(wch[0]), FALSE))
      curses_driver_write_wide_string_at(win, row, col, wch, colour,
                                         expected_width);
}

static void show_write_utf8_status_cluster_at(WINDOW *win, int row, int col,
                                              const CHARTYPE *line, size_t len,
                                              TextCluster cluster,
                                              chtype colour,
                                              int expected_width)
{
   wchar_t wch[THE_MAX_SCREEN_WIDTH + 1];

   if (show_cluster_to_wide_string(line, len, cluster, wch,
                                   sizeof(wch) / sizeof(wch[0]),
                                   show_status_cluster_force_expanded(line, len,
                                                                      cluster)))
      curses_driver_write_wide_string_at(win, row, col, wch, colour,
                                         expected_width);
}

static void show_fill_cells_at(WINDOW *win, int row, int col, int width, chtype colour)
{
   curses_driver_fill_cells_at(win, row, col, width, colour);
}

static void show_write_ascii_cells_at(WINDOW *win, int row, int col,
                                      const char *text, int width,
                                      chtype colour)
{
   curses_driver_write_ascii_cells_at(win, row, col, text, width, colour);
}

#define mysetchar(dest, ch, colour) curses_driver_set_cchar_codepoint((dest), (uint32_t)(ch), (colour))
#else
#define mysetchar(dest, ch, colour ) {                   \
                  setcchar( dest, &ch, colour, 0, NULL ); \
                  }
#endif

#ifdef HAVE_WADDCHNSTR
/* Use fast line output routines. We are faster with one simple function call
 * in opposite of many wmove/wattrset/waddch calls.
 */
static int _fast_col; /* Used elements of linebufch */
static WINDOW *_fast_win; /* buffered for waddchnstr */
# define INIT_LINE_OUTPUT(win,line) {                  \
                        _fast_win = win;              \
                        _fast_col = 0;                \
                        PARATEST_INIT_LINE(win,line); \
                        curses_driver_move_window_cursor(_fast_win,line,0); \
DEBUGDUMPDETAIL(fprintf(stderr,"%s %d(%s): INIT_LINE_OUTPUT: line: %d\n", __FILE__,__LINE__,__func__,line );) \
                        }
# ifdef USE_UTF8
static void show_add_utf8_codepoint(uint32_t ch, chtype colour)
{
   if (ch < 256 && etmode_flag[ch])
   {
      CHARTYPE cc = (CHARTYPE)ch;
      ch = (uint32_t)etmode_table[cc];
      colour = (etmode_table[cc] & A_COLOR);
   }
   curses_driver_set_cchar_codepoint(linebufch + _fast_col, ch, colour);
   _fast_col++;
}

/* length MUST be number of characters: u8_strlen(); NOT bytes: strlen() */
#  define ADD_LINE_OUTPUT(line,length,colour) {                \
                       LENGTHTYPE l = length;                  \
                       CHARTYPE *src;                          \
                       chtype color,hi1; /* beware of slow arg! */ \
                       cchar_t *dest;                          \
                       char cc;                                \
                       u_int32_t ch;                           \
                       int pos=0;                              \
                       PARATEST_ADD_LINE(l,"ADD_LINE_OUTPUT"); \
                       dest = linebufch + _fast_col;           \
                       _fast_col += l;                         \
                       src = line;                             \
                       color = colour;                         \
DEBUGDUMPDETAIL(fprintf(stderr,"  %s %d(%s): ADD_LINE_OUTPUT: length %d: ", __FILE__,__LINE__,__func__,length );) \
                       while (l--) {                           \
                          ch = u8_nextchar( (char *)src, &pos );       \
                          if (ch < 256 && etmode_flag[ch])     \
                          {                                    \
                             cc = (char)ch;                    \
                             ch = etmode_table[cc];            \
                             hi1 = (etmode_table[cc] & A_COLOR); \
                             mysetchar( dest, ch, hi1 );       \
                          }                                    \
                          else                                 \
                          {                                    \
                             mysetchar( dest, ch, color );     \
                          }                                    \
DEBUGDUMPDETAIL(fprintf(stderr,"x%x@%d ", ch, pos );) \
                          dest++;                              \
                       }                                       \
DEBUGDUMPDETAIL(fprintf(stderr,"\n");)                                          \
                        }
#  define ADD_SYNTAX_LINE_OUTPUT(line,length,highlight) {      \
                       LENGTHTYPE l = length;                  \
                       CHARTYPE *src;                          \
                       chtype *highl,hi1;                      \
                       cchar_t *dest;                          \
                       char cc;                                \
                       u_int32_t ch;                           \
                       int pos=0;                              \
                       PARATEST_ADD_LINE(l,"ADD_SYNTAX_LINE_OUTPUT"); \
                       dest = linebufch + _fast_col;           \
                       _fast_col += l;                         \
                       src = line;                             \
                       highl = highlight;                      \
DEBUGDUMPDETAIL(fprintf(stderr,"  %s %d(%s): ADD_SYNTAX_LINE_OUTPUT: length %d: ", __FILE__,__LINE__,__func__,length );) \
                       while (l--) {                           \
                          ch = u8_nextchar( (char *)src, &pos );       \
DEBUGDUMPDETAIL(fprintf(stderr,"x%x@%d ", ch, pos );) \
                          if (ch < 256 && etmode_flag[ch])     \
                          {                                    \
                             cc = (char)ch;                    \
                             ch = etmode_table[cc];            \
                             hi1 = (etmode_table[cc] & A_COLOR); \
DEBUGDUMPDETAIL(fprintf(stderr,": 0X%x 0X%x ", ch, hi1 );)            \
                             mysetchar( dest, ch, hi1 );       \
                          }                                    \
                          else                                 \
                          {                                    \
DEBUGDUMPDETAIL(fprintf(stderr,": x%x %x ", ch, *highl );) \
                             mysetchar( dest, ch, *highl );       \
                          }                                    \
                          dest++;                              \
                          highl++;                             \
                       }                                       \
DEBUGDUMPDETAIL(fprintf(stderr,"\n");)                                          \
                        }
#  define FILL_LINE_OUTPUT(c,length,colour) {                    \
                        cchar_t *dest;                           \
                        u_int32_t ch=c;                           \
                        LENGTHTYPE l = length;                   \
                        PARATEST_ADD_LINE(l,"FILL_LINE_OUTPUT"); \
                        dest = linebufch + _fast_col;            \
                        _fast_col += l;                          \
DEBUGDUMPDETAIL(fprintf(stderr,"  %s %d(%s): FILL_LINE_OUTPUT: length %d: ", __FILE__,__LINE__,__func__,length );) \
                        while (l--) {                            \
DEBUGDUMPDETAIL(fprintf(stderr,"x%x ", c );) \
                          if (ch < 256 && etmode_flag[ch])     \
                             ch = etmode_table[ch];            \
                          mysetchar( dest, ch, colour );          \
                          dest++;                                \
                       }                                       \
DEBUGDUMPDETAIL(fprintf(stderr,"\n");)                                          \
                        }
#  define END_LINE_OUTPUT() { curses_driver_write_cchar_span(_fast_win, \
                                     linebufch,                  \
                                     _fast_col);                 \
DEBUGDUMPDETAIL(fprintf(stderr,"%s %d(%s): END_LINE_OUTPUT\n", __FILE__,__LINE__,__func__);) \
                          }
# else
#  define ADD_LINE_OUTPUT(line,length,colour) {                  \
                       LENGTHTYPE l = length;                  \
                       CHARTYPE *src;                          \
                       chtype color, /* beware of slow arg! */ \
                              *dest;                           \
                       PARATEST_ADD_LINE(l,"ADD_LINE_OUTPUT"); \
                       dest = linebufch + _fast_col;           \
                       _fast_col += l;                         \
                       src = line;                             \
                       color = colour;                         \
                       while (l--) {                           \
                          *dest++ = make_chtype(*src,color);   \
                          src++;                               \
                       } }
#  define ADD_SYNTAX_LINE_OUTPUT(line,length,highlight) {        \
                       LENGTHTYPE l = length;                  \
                       CHARTYPE *src;                          \
                       chtype *dest,*highl;            \
                       PARATEST_ADD_LINE(l,"ADD_SYNTAX_LINE_OUTPUT"); \
                       dest = linebufch + _fast_col;           \
                       _fast_col += l;                         \
                       src = line;                             \
                       highl = highlight;                      \
                       while (l--) {                           \
                          *dest++ = make_chtype(*src,*highl);  \
                          src++;                               \
                          highl++;                             \
                       } }
#  define FILL_LINE_OUTPUT(c,length,colour) {                      \
                        chtype *dest,C = make_chtype(c,colour);  \
                        LENGTHTYPE l = length;                   \
                        PARATEST_ADD_LINE(l,"FILL_LINE_OUTPUT"); \
                        dest = linebufch + _fast_col;            \
                        _fast_col += l;                          \
                        while (l--)                              \
                           *dest++ = C;                          \
                        }
#  define END_LINE_OUTPUT() { curses_driver_write_chtype_span(_fast_win, \
                                     linebufch,                  \
                                     _fast_col);                 \
                          }
# endif
#else
/* don't use waddchnstr */
static WINDOW *_fast_win; /* buffered for waddch/wattrset */
static chtype _fast_colour = (chtype) -1l; /* buffering prevents unnecessary
                                              wattrset */
# define INIT_LINE_OUTPUT(win,line) {                  \
                        _fast_win = win;              \
                        _fast_colour = (chtype) -1;   \
                        PARATEST_INIT_LINE(win,line); \
                        curses_driver_move_window_cursor(_fast_win,line,0); }
# ifdef USE_UTF8
static void show_add_utf8_codepoint(uint32_t ch, chtype colour)
{
   cchar_t out;

   if (ch < 256 && etmode_flag[ch])
   {
      CHARTYPE cc = (CHARTYPE)ch;
      ch = (uint32_t)etmode_table[cc];
      colour = (etmode_table[cc] & A_COLOR);
   }
   curses_driver_set_cchar_codepoint(&out, ch, colour);
   curses_driver_add_cchar(_fast_win, &out);
}

#  define ADD_LINE_OUTPUT(line,length,colour) {                  \
                       chtype col = colour;                    \
                       LENGTHTYPE l = length;                  \
                       CHARTYPE *src;                          \
                       u_int32_t ch;                           \
                       int pos=0;                              \
                       PARATEST_ADD_LINE(l,"ADD_LINE_OUTPUT"); \
                   /*    if (col != _fast_colour)   */             \
                   /*    {   */                                  \
                          _fast_colour = col;                  \
                 /*         curses_driver_set_window_attr(_fast_win,col);   */          \
                  /*     } */                                    \
                       src = line;                             \
                       while (l--) {                             \
                          ch = u8_nextchar( src, &pos );       \
                          show_add_utf8_codepoint(ch,_fast_colour); \
                       } }
#  define ADD_SYNTAX_LINE_OUTPUT(line,length,highlight) {        \
                       LENGTHTYPE l = length;                  \
                       CHARTYPE *src;                          \
                       chtype *highl;            \
                       u_int32_t ch;                           \
                       int pos=0;                              \
                       PARATEST_ADD_LINE(l,"ADD_SYNTAX_LINE_OUTPUT"); \
                       src = line;                             \
                       highl = highlight;                      \
                       while (l--) {                           \
                          if (*highl != _fast_colour)          \
                          {                                    \
                             _fast_colour = *highl;            \
                             curses_driver_set_window_attr(_fast_win,*highl);       \
                          }                                    \
                          ch = u8_nextchar( src, &pos );       \
                          show_add_utf8_codepoint(ch,*highl);  \
                          highl++;                             \
                       } }
#  define FILL_LINE_OUTPUT(c,length,colour) {                      \
                        chtype col = colour,C = c;               \
                       LENGTHTYPE l = length;                  \
                        PARATEST_ADD_LINE(l,"FILL_LINE_OUTPUT"); \
                        if (col != _fast_colour)                 \
                          {                                      \
                           _fast_colour = col;                   \
                           curses_driver_set_window_attr(_fast_win,col);              \
                          }                                      \
                        while (l--)                              \
                           show_add_utf8_codepoint((uint32_t)C,col); \
                        }
# else
#  define ADD_LINE_OUTPUT(line,length,colour) {                  \
                       chtype col = colour;                    \
                       LENGTHTYPE l = length;                  \
                       CHARTYPE *src;                          \
                       PARATEST_ADD_LINE(l,"ADD_LINE_OUTPUT"); \
                       if (col != _fast_colour)                \
                       {                                     \
                          _fast_colour = col;                  \
                          curses_driver_set_window_attr(_fast_win,col);             \
                       }                                     \
                       src = line;                             \
                       while (l--)                             \
                          curses_driver_add_chtype(_fast_win,*src++); \
                       }
#  define ADD_SYNTAX_LINE_OUTPUT(line,length,highlight) {        \
                       LENGTHTYPE l = length;                  \
                       CHARTYPE *src;                          \
                       chtype *highl;            \
                       PARATEST_ADD_LINE(l,"ADD_SYNTAX_LINE_OUTPUT"); \
                       src = line;                             \
                       highl = highlight;                      \
                       while (l--) {                           \
                          if (*highl != _fast_colour)          \
                          {                                    \
                             _fast_colour = *highl;            \
                             curses_driver_set_window_attr(_fast_win,*highl);       \
                          }                                    \
                          curses_driver_add_chtype(_fast_win,*src);   \
                          src++;                               \
                          highl++;                             \
                       } }
#  define FILL_LINE_OUTPUT(c,length,colour) {                      \
                        chtype col = colour,C = c;               \
                       LENGTHTYPE l = length;                  \
                        PARATEST_ADD_LINE(l,"FILL_LINE_OUTPUT"); \
                        if (col != _fast_colour)                 \
                          {                                      \
                           _fast_colour = col;                   \
                           curses_driver_set_window_attr(_fast_win,col);              \
                          }                                      \
                        while (l--)                              \
                           curses_driver_add_chtype(_fast_win,C);      \
                        }
# endif
# define END_LINE_OUTPUT()
#endif

#if defined(USE_REGINA)
# define REXX_INT_CHAR         'R'
#elif defined(USE_OREXX) || defined(USE_OOREXX)
# define REXX_INT_CHAR         'O'
#elif defined(USE_OS2REXX)
# define REXX_INT_CHAR         'O'
#elif defined(USE_WINREXX)
# define REXX_INT_CHAR         'W'
#elif defined(USE_QUERCUS)
# define REXX_INT_CHAR         'Q'
#elif defined(USE_UNIREXX)
# define REXX_INT_CHAR         'U'
#elif defined(USE_REXX6000)
# define REXX_INT_CHAR         '6'
#elif defined(USE_REXXIMC)
# define REXX_INT_CHAR         'I'
#elif defined(USE_REXXTRANS)
# define REXX_INT_CHAR         'T'
#else
# define REXX_INT_CHAR         ' '
#endif


#ifdef DEBUG_SYNTAX_HIGHLIGHTING
static void show_highlighted_line( int lineno, SHOW_LINE *scurr, char *msg )
{
   int i;
   fprintf(stderr,"=====================================================\n" );
   fprintf(stderr, "%s %d: %s\n",__FILE__,lineno,msg );

   if ( scurr )
   {
      if ( scurr->contents)
      {
         fprintf(stderr,"%s\n",scurr->contents);
         for( i =0; i < scurr->length; i++ )
         {
            fprintf(stderr,"%c", scurr->highlight_type[i]);
         }
         fprintf(stderr,"\n\n");
      }
   }
   else
   {
      scurr = screen[current_screen].sl;
      for( i = 0 ; i < screen[current_screen].rows[WINDOW_FILEAREA]; i++ )
      {
         if ( scurr->contents )
         {
            fprintf(stderr,"Row: %3.3d:%.*s\n      ==>",i,scurr->length,scurr->contents);
            fwrite( scurr->highlight_type, sizeof(char), scurr->length, stderr );
            fprintf( stderr, "\n" );
         }
         scurr++;
      }
   }
}
#endif

/* small helper routines *****************************************************/
#if ( defined(USE_XCURSES) || defined(USE_SDLCURSES) || defined(USE_WINGUICURSES) ) && PDC_BUILD >= 2501
static int is_column_being_shown(CHARTYPE scrno,COLTYPE col)
{
   COLTYPE lcol = SCREEN_VIEW(scrno)->verify_col-1;
   COLTYPE rcol = lcol + screen[scrno].cols[WINDOW_FILEAREA]-1;
   if ( col >= lcol && col <= rcol )
      return 1;
   else
      return 0;
}
#endif
/***********************************************************************/
static void display_line_left( WINDOW *win, chtype colour, CHARTYPE *str, int lenstr, int line, int width )
/***********************************************************************/
{
   int linelength;

   if ( ( linelength = lenstr ) > width)
      linelength = width;

   INIT_LINE_OUTPUT( win, line );
   if ( linelength )
      ADD_LINE_OUTPUT( str, linelength, colour );
   if ( ( linelength = width - linelength ) != 0 )
      FILL_LINE_OUTPUT( ' ', linelength, colour );
   END_LINE_OUTPUT();
}

/***********************************************************************/
static void display_syntax_line_left(WINDOW *win, chtype colour, CHARTYPE *str,
                              chtype *high, int line, int width)
/***********************************************************************/
{
   int linelength;

   if ((linelength = strlen((DEFCHAR*)str)) > width)
      linelength = width;

   INIT_LINE_OUTPUT(win,line);
   if (linelength)
      ADD_SYNTAX_LINE_OUTPUT( str, linelength, high );
   if ((linelength = width - linelength) != 0)
      FILL_LINE_OUTPUT(' ',linelength,colour);
   END_LINE_OUTPUT();
}

#ifdef USE_UTF8
static int show_frame_cursor_col(const UiFrame *frame, UiRowRole role,
                                 LINETYPE line_number, short row,
                                 int viewport_col, int *col,
                                 CursorShape *shape)
{
   LogicalCursor cursor;

   if (frame == NULL
   ||  !ui_frame_cursor_for_row(frame, role, line_number, row, &cursor))
      return FALSE;
   if (col != NULL)
      *col = cursor.text.cell_column - viewport_col;
   if (shape != NULL)
      *shape = current_cursor_shape();
   return TRUE;
}

static int show_filearea_cursor_col(const UiFrame *frame, CHARTYPE scrno,
                                    short row, LINETYPE line_number,
                                    int viewport_col, int *col,
                                    CursorShape *shape)
{
   if (frame != NULL)
      return show_frame_cursor_col(frame, UI_ROW_FILE, line_number, row,
                                   viewport_col, col, shape);
   return cursor_focus_filearea_cursor(scrno, row, col, shape);
}

static int show_filearea_cursor_display_col(const UiFrame *frame, CHARTYPE scrno,
                                            short row, LINETYPE line_number,
                                            const CHARTYPE *line, size_t len,
                                            int viewport_col, int *col)
{
   LogicalCursor cursor;

   if (frame != NULL)
   {
      if (!ui_frame_cursor_for_row(frame, UI_ROW_FILE, line_number, row,
                                   &cursor))
         return FALSE;
      if (col != NULL)
         *col = show_utf8_display_col_from_logical(line, len, viewport_col,
                                                   cursor.text.cell_column);
      return TRUE;
   }
   return cursor_focus_filearea_display_cursor(scrno, row, col);
}

static void show_draw_filearea_marker_cursor(const UiFrame *frame, CHARTYPE scrno,
                                             short row, LINETYPE line_number,
                                             UiRowRole role, chtype normal)
{
   int cursor_col = 0;
   CursorShape cursor_shape = CURSOR_BLOCK;

   if (frame != NULL)
   {
      if (!show_frame_cursor_col(frame, role, line_number, row,
                                 (int)SCREEN_VIEW(scrno)->verify_col - 1,
                                 &cursor_col, &cursor_shape))
         return;
   }
   else if (!cursor_focus_filearea_cursor(scrno, row, &cursor_col, &cursor_shape))
      return;

   curses_driver_draw_software_chtype_cell(scrno, SCREEN_WINDOW_FILEAREA(scrno),
                                           row, cursor_col, normal,
                                           cursor_shape);
}

static void show_draw_software_command_cursor(CHARTYPE scrno, VIEW_DETAILS *view)
{
   short row = 0;
   short col = 0;
   CursorShape shape = CURSOR_BLOCK;
   chtype base;

   if (view == NULL
   ||  !cursor_focus_command_cursor(scrno, &row, &col, &shape))
      return;

   base = set_colour(view->file_for_view->attr + (inDIALOG ? ATTR_DIA_EDITFIELD : ATTR_CMDLINE));
   curses_driver_draw_software_chtype_cell(scrno, SCREEN_WINDOW_COMMAND(scrno),
                                           row, col, base, shape);
}

static void show_draw_software_prefix_cursor(CHARTYPE scrno, short row,
                                             const UiFrame *frame)
{
   int col = 0;
   CursorShape shape = CURSOR_BLOCK;

   if (frame != NULL)
   {
      SHOW_LINE *show_row = &screen[scrno].sl[row];

      if (!show_frame_cursor_col(frame, UI_ROW_PREFIX, show_row->line_number,
                                 row, 0, &col, &shape))
         return;
   }
   else if (!cursor_focus_prefix_cursor(scrno, row, &col, &shape))
      return;
   curses_driver_draw_software_chtype_cell(
      scrno, SCREEN_WINDOW_PREFIX(scrno), row, col,
      set_colour(SCREEN_FILE(scrno)->attr + ATTR_PREFIX), shape);
}
#endif

/***********************************************************************/
static void display_line_center(WINDOW *win, chtype colour, CHARTYPE *str,
                              int line, int width, int fillchar)
/***********************************************************************/
{
   int linelength,first;

   if ((linelength = strlen((DEFCHAR*)str)) > width)
      linelength = width;
   first = (width - linelength) >> 1;

   INIT_LINE_OUTPUT(win,line);

   if (first)
      FILL_LINE_OUTPUT(fillchar,first,colour);
   if (linelength)
      ADD_LINE_OUTPUT(str,linelength,colour);
   if ((linelength = width - linelength - first) != 0)
      FILL_LINE_OUTPUT(fillchar,linelength,colour);
   END_LINE_OUTPUT();
}

/* real stuff ****************************************************************/


/***********************************************************************/
void prepare_idline(CHARTYPE scrno)
/***********************************************************************/
{
   short fpath_len=0,max_name=0;
   LENGTHTYPE x=0;
   LINETYPE line_number=0L;
   CHARTYPE _THE_FAR buffer[120]; /* should be large enough for very long values */
   CHARTYPE _THE_FAR display_path[MAX_FILE_NAME+1];
#ifdef __PDCURSES__
   CHARTYPE _THE_FAR title[MAX_FILE_NAME+1];
   static CHARTYPE _THE_FAR old_title[MAX_FILE_NAME+1];
#endif
   CHARTYPE *fpath = display_path;
   short num_to_delete=0,num_to_start=0;
   VIEW_DETAILS *screen_view = SCREEN_VIEW(scrno);
   FILE_DETAILS *screen_file = SCREEN_FILE(scrno);
   int buflen;
   char *pos_string=NULL;

   TRACE_FUNCTION("show.c:    prepare_idline");
   /*
    * Determine content of window title. This can be display whether IDLINE is ON or OFF
    */
#if defined(MULTIPLE_PSEUDO_FILES)
   strcpy( (DEFCHAR *)display_path, (DEFCHAR *)screen_file->fpath );
   strcat( (DEFCHAR *)display_path, (DEFCHAR *)screen_file->fname );
#else
   if ( strlen( (DEFCHAR *)screen_file->display_name ) > 0 )
   {
      strcpy( (DEFCHAR *)display_path, (DEFCHAR *)screen_file->display_name );
   }
   else
   {
      switch( screen_file->pseudo_file )
      {
         case PSEUDO_DIR:
            strcpy( (DEFCHAR *)display_path, "DIR: " );
            strcat( (DEFCHAR *)display_path, (DEFCHAR *)dir_path );
            strcat( (DEFCHAR *)display_path, (DEFCHAR *)dir_files );
            break;
         case PSEUDO_REXX:
            strcpy( (DEFCHAR *)display_path, "Output from: " );
            strcat( (DEFCHAR *)display_path, (DEFCHAR *)rexx_macro_name );
            break;
         case PSEUDO_KEY:
            strcpy( (DEFCHAR *)display_path, "Key definitions:" );
            break;
         default:
            if ( screen_file->display_actual_filename )
            {
               strcpy( (DEFCHAR *)display_path, (DEFCHAR *)screen_file->fpath );
               strcat( (DEFCHAR *)display_path, (DEFCHAR *)screen_file->fname );
            }
            else
            {
               strcpy( (DEFCHAR *)display_path, (DEFCHAR *)screen_file->fname );
            }
            break;
      }
   }
#endif
#if defined(__PDCURSES__) & !defined(MSWIN)
   if ( curses_started )
   {
      sprintf( (DEFCHAR *)title, "THE %s - %s", the_version, (DEFCHAR *)display_path );
      /* only display the title if different from previous one */
      if ( strcmp( (DEFCHAR *)title, (DEFCHAR *)old_title ) != 0 )
      {
         PDC_set_title( (DEFCHAR *)title );
         strcpy( (DEFCHAR *)old_title, (DEFCHAR *)title );
      }
   }
#endif
   /*
    * Get line,col values only if POSITION is ON...
    */
   if ( screen_view->position_status )
      pos_string = get_current_position( scrno, &line_number, &x );
   /*
    * Set up buffer for line,col,size and alt values for vertical screens.
    */
   if ( display_screens != 1 && !horizontal )
   {
      if ( screen_view->position_status )
      {
         switch ( compatible_look )
         {
            case COMPAT_XEDIT:
               sprintf( (DEFCHAR *)buffer, "S=%lu L=%lu C=%lu A=%u,%u",
                        screen_file->number_lines,
                        line_number,
                        x,
                        screen_file->autosave_alt,
                        screen_file->save_alt );
               break;
            case COMPAT_ISPF:
               if ( pos_string == NULL )
               {
                  sprintf( (DEFCHAR *)buffer, "S=%lu L=%lu C=%lu A=%u,%u",
                           screen_file->number_lines,
                           line_number,
                           x,
                           screen_file->autosave_alt,
                           screen_file->save_alt );
               }
               else
               {
                  sprintf( (DEFCHAR *)buffer, "S=%lu L=%s C=%lu A=%u,%u",
                           screen_file->number_lines,
                           pos_string,
                           x,
                           screen_file->autosave_alt,
                           screen_file->save_alt );
               }
               break;
            default:
               sprintf( (DEFCHAR *)buffer, "L=%lu C=%lu S=%lu A=%u,%u",
                        line_number,
                        x,
                        screen_file->number_lines,
                        screen_file->autosave_alt,
                        screen_file->save_alt );
              break;
         }
      }
      else
      {
         sprintf( (DEFCHAR *)buffer, "S=%lu A=%u,%u",
                  screen_file->number_lines,
                  screen_file->autosave_alt,
                  screen_file->save_alt );
      }
      max_name = max( 0, (screen[scrno].screen_cols - 1 ) - strlen( (DEFCHAR *)buffer ) );
   }
   else
   {
      if ( screen_view->position_status )
      {
         switch ( compatible_look )
         {
            case COMPAT_XEDIT:
               sprintf( (DEFCHAR *)buffer, "Size=%-6lu Line=%-6lu Col=%-3lu Alt=%u,%u",
                        screen_file->number_lines,
                        line_number,
                        x,
                        screen_file->autosave_alt,
                        screen_file->save_alt );
              break;
            case COMPAT_ISPF:
               if ( pos_string == NULL )
               {
                  sprintf( (DEFCHAR *)buffer, "Size=%-6lu Line=%-6lu Col=%-3lu Alt=%u,%u",
                           screen_file->number_lines,
                           line_number,
                           x,
                           screen_file->autosave_alt,
                           screen_file->save_alt );
               }
               else
               {
                  sprintf( (DEFCHAR *)buffer, "Size=%-6lu Line=%s Col=%-3lu Alt=%u,%u",
                           screen_file->number_lines,
                           pos_string,
                           x,
                           screen_file->autosave_alt,
                           screen_file->save_alt );
               }
               break;
            default:
               sprintf((DEFCHAR *)buffer,"Line=%-6lu Col=%-4lu Size=%-5lu Alt=%u,%u",
                                    line_number,
                                    x,
                                    screen_file->number_lines,
                                    screen_file->autosave_alt,
                                    screen_file->save_alt );
               break;
         }
         max_name = max( 0, (screen[scrno].screen_cols - 47 ) );
      }
      else
      {
         if ( compatible_look == COMPAT_XEDIT )
         {
            sprintf( (DEFCHAR *)buffer, "Size=%-9lu%sAlt=%u,%u",
                     screen_file->number_lines,
                     "                  ", /* speed up! */
                     screen_file->autosave_alt,
                     screen_file->save_alt );
            max_name = max(0,(screen[scrno].screen_cols-47));
         }
         else
         {
            sprintf( (DEFCHAR *)buffer, "Size=%-5lu Alt=%u,%u",
                     screen_file->number_lines,
                     screen_file->autosave_alt,
                     screen_file->save_alt );
            max_name = max( 0, (screen[scrno].screen_cols - 26 ) );
         }
      }
   }
   /*
    * Determine which portion of filename can be displayed.
    */
   /* fpath = strrmdup(strtrans(display_path,ISLASH,ESLASH),ESLASH,TRUE); */
   fpath = strtrans( display_path, ISLASH,ESLASH );
   fpath_len = strlen( (DEFCHAR *)fpath );
   if ( fpath_len > max_name )
   {
      num_to_delete = fpath_len - max_name + 2;
      num_to_start = max( 0, (long)((strlen( (DEFCHAR *)fpath ) / 2 ) - (num_to_delete / 2) ) );
      memcpy( linebuf, fpath, num_to_start );
      strcpy( (DEFCHAR*)linebuf + num_to_start, "<>" );
      strcat( (DEFCHAR*)linebuf + num_to_start + 2, (DEFCHAR*)fpath + num_to_start + num_to_delete );
   }
   else
   {
      strcpy( (DEFCHAR*)linebuf, (DEFCHAR*)fpath );
      memset( linebuf + fpath_len, ' ', max_name - fpath_len );
   }
   buflen = screen[scrno].screen_cols - max_name - 1;
   sprintf( (DEFCHAR*)linebuf + max_name, " %-*.*s", buflen, buflen, buffer );
   TRACE_RETURN();
   return;
}

/***********************************************************************/
void show_heading(CHARTYPE scrno)
/***********************************************************************/
{
   FILE_DETAILS *screen_file = SCREEN_FILE(scrno);
   WINDOW *screen_window_idline = SCREEN_WINDOW_IDLINE(scrno);

   TRACE_FUNCTION("show.c:    show_heading");

   prepare_idline( scrno );

   /* display the stuff */
   INIT_LINE_OUTPUT( screen_window_idline, 0 );
   ADD_LINE_OUTPUT( linebuf,
                    strlen( (DEFCHAR*)linebuf ),
                    set_colour( screen_file->attr + ATTR_IDLINE ) );
   END_LINE_OUTPUT();

   curses_driver_refresh_window( screen_window_idline );
   TRACE_RETURN();
   return;
}
/***********************************************************************/
void show_statarea(void)
/***********************************************************************/
{
   int x=0;
#ifdef USE_UTF8
   int charpos = 0;
   int draw_status_cluster = FALSE;
   int status_field_width = 0;
   int status_cluster_offset = 0;
   int status_cluster_display_width = 1;
   char status_field[21];
   const CHARTYPE *status_cluster_line = NULL;
   size_t status_cluster_len = 0;
   TextCluster status_cluster = {0};
   LogicalCursor logical_status_cursor;
#endif
   int key=0;
   time_t timer;
   struct tm *tblock=NULL;
   int length;
   char _THE_FAR buffer[THE_MAX_SCREEN_WIDTH+10];

   TRACE_FUNCTION("show.c:    show_statarea");
   /*
    * Reset parser severity at the start of every status line refresh.
    * It will be updated by extract_pmsg if it is called via format_options.
    */
   current_parser_severity = 0;
   /*
    * If the status line is off, just exit...
    */   if ( STATUSLINEx == 'O' || !curses_started || CURRENT_VIEW == NULL )
   {
      TRACE_RETURN();
      return;
   }
   /*
    * If GUI option set for status line...
    */
   if (STATUSLINEx == 'G')
   {
#ifdef MSWIN
      Show_GUI_footing();
      TRACE_RETURN();
      return;
#endif
      TRACE_RETURN();
      return;
   }
   /*
    * Display THE version.
    */
   sprintf((DEFCHAR*)linebuf,"THE %-9s",the_version);
   memset((DEFCHAR *)linebuf+10,' ',max(0,COLS-9));
   /*
    * Display number of files or copyright on startup.
    */
   if (initial)
   {
      strcpy((DEFCHAR*)buffer,"   ");
      strcat((DEFCHAR*)buffer,(DEFCHAR*)the_copyright);
   }
   else
   {
      /*
       * Display any pending prefix command warning
       */
      if (CURRENT_FILE->first_ppc != NULL
      &&  CURRENT_FILE->first_ppc->ppc_cmd_idx != (-1)
      &&  CURRENT_FILE->first_ppc->ppc_cmd_idx != (-2))
      {
         sprintf(buffer,"'%s' pending...",get_prefix_command(CURRENT_FILE->first_ppc->ppc_cmd_idx) );
      }
      else if ( record_fp )
      {
         strcpy( buffer, (DEFCHAR *)record_status );
      }
      else
      {
         memset(buffer,' ',min(COLS,sizeof(buffer)));
         format_options((CHARTYPE *)buffer);
      }
   }
   length = strlen(buffer);
   if (STATAREA_OFFSET+length < max(0,COLS-27))
      memset((DEFCHAR *)linebuf+STATAREA_OFFSET+length,' ',max(0,(COLS-27)-STATAREA_OFFSET+length));
   memcpy((DEFCHAR*)linebuf+STATAREA_OFFSET,buffer,length);
   /*
    * Display CLOCK.
    */
   if (CLOCKx)
   {
      timer = time(NULL);
      tblock = localtime(&timer);
      sprintf((DEFCHAR*)linebuf+max(0,(COLS-((HEXDISPLAYx) ? 36 : 27))),"%2d:%02d%s ",
           (tblock->tm_hour > 12) ? (tblock->tm_hour-12) : (tblock->tm_hour),
            tblock->tm_min,
           (tblock->tm_hour >= 12) ? ("pm") : ("am"));
   }
   else
      strcpy((DEFCHAR*)linebuf+max(0,(COLS-((HEXDISPLAYx) ? 36 : 27))),"        ");
   /*
    * Display HEXDISPLAY.
    */
   if (HEXDISPLAYx)
   {
      CursesDriverWindowCursor cursor =
         curses_driver_capture_window_cursor(CURRENT_WINDOW);

#ifdef USE_UTF8
      logical_status_cursor = CURRENT_VIEW->logical_cursor.current;
#endif
      x = cursor.valid ? cursor.col : 0;
      switch(CURRENT_VIEW->current_window)
      {
         case WINDOW_FILEAREA:
#ifdef USE_UTF8
         {
            int logical_cell = show_utf8_logical_col_from_display(rec, rec_len,
                                                                  CURRENT_VIEW->verify_col - 1,
                                                                  x,
                                                                  TEXT_SNAP_BACKWARD);

            if (logical_status_cursor.valid
            &&  logical_status_cursor.zone == LOGICAL_CURSOR_ZONE_FILEAREA
            &&  logical_status_cursor.line_number == CURRENT_VIEW->focus_line)
               logical_cell = logical_status_cursor.text.cell_column;
            status_cluster_line = rec;
            status_cluster_len = rec_len;
            status_cluster = textpos_cluster_at(status_cluster_line, status_cluster_len,
               textpos_from_cell(status_cluster_line, status_cluster_len,
                                 logical_cell,
                                 TEXT_SNAP_BACKWARD));
         }
#else
# ifdef VMS
            key = (short)( *(rec+CURRENT_VIEW->verify_col-1+x) );
# else
            key = (short)( *(rec+CURRENT_VIEW->verify_col-1+x) & A_CHARTEXT );
# endif
#endif
            break;
         case WINDOW_COMMAND:
#ifdef USE_UTF8
         {
            int logical_cell = x + cmd_verify_col - 1;

            if (logical_status_cursor.valid
            &&  logical_status_cursor.zone == LOGICAL_CURSOR_ZONE_COMMAND)
               logical_cell = logical_status_cursor.text.cell_column;
            status_cluster_line = cmd_rec;
            status_cluster_len = cmd_rec_len;
            status_cluster = textpos_cluster_at(status_cluster_line, status_cluster_len,
               textpos_from_cell(status_cluster_line, status_cluster_len,
                                 logical_cell,
                                 TEXT_SNAP_BACKWARD));
         }
#else
# ifdef VMS
            key = (short)( *(cmd_rec+x+cmd_verify_col-1) );
# else
            key = (short)( *(cmd_rec+x+cmd_verify_col-1) & A_CHARTEXT );
# endif
#endif
            break;
         case WINDOW_PREFIX:
#ifdef USE_UTF8
         {
            int logical_cell = x;

            if (logical_status_cursor.valid
            &&  logical_status_cursor.zone == LOGICAL_CURSOR_ZONE_PREFIX
            &&  logical_status_cursor.line_number == CURRENT_VIEW->focus_line)
               logical_cell = logical_status_cursor.text.cell_column;
            status_cluster_line = pre_rec;
            status_cluster_len = pre_rec_len;
            status_cluster = textpos_cluster_at(status_cluster_line, status_cluster_len,
               textpos_from_cell(status_cluster_line, status_cluster_len,
                                 logical_cell,
                                 TEXT_SNAP_BACKWARD));
         }
#else
# ifdef VMS
            key = (short)( *(pre_rec+x) );
# else
            key = (short)( *(pre_rec+x) & A_CHARTEXT );
# endif
#endif
            break;
      }
#ifdef USE_UTF8
   status_field[0] = '\0';
   charpos = max(0,(COLS-27));
   {
      char codebuf[128];
      TextPos pos;
      size_t code_len = 0;
      int field_truncated = FALSE;
      int glyph_cells = 1;
      int code_col = 0;

      status_field_width = min(20, max(0, (COLS-7) - charpos));
      codebuf[0] = '\0';
      pos = status_cluster.pos;
         while (status_cluster_line != NULL
         &&     pos.byte_offset < status_cluster.end.byte_offset)
         {
            TextCodepoint item = textpos_codepoint_at_boundary(status_cluster_line,
                                                               status_cluster_len,
                                                               pos);
            char one[24];

            if (item.byte_length == 0)
               break;
            snprintf(one, sizeof(one), "%sU+%X", (code_len == 0) ? "" : "+", item.codepoint);
            if (code_len + strlen(one) < sizeof(codebuf))
            {
               strcpy(codebuf + code_len, one);
               code_len += strlen(one);
            }
            else
            {
               field_truncated = TRUE;
            }
            pos = show_utf8_advance_codepoint_pos(pos, item);
         }

         if (codebuf[0] == '\0')
            strcpy(codebuf, "U+0");

      memset(status_field, ' ', sizeof(status_field));
      status_field[20] = '\0';
      if (status_cluster_line == NULL || status_cluster.byte_length == 0)
      {
         status_field[0] = '[';
         status_field[1] = ' ';
         status_field[2] = ']';
         status_field[3] = ' ';
         field_truncated = show_utf8_copy_status_text(status_field, 4, codebuf);
      }
      else if (status_cluster.codepoint_count == 1)
      {
         TextCodepoint item = textpos_codepoint_at_boundary(status_cluster_line,
                                                            status_cluster_len,
                                                            status_cluster.pos);
         if (item.codepoint >= 32 && item.codepoint < 127)
         {
            status_field[0] = '[';
            status_field[1] = (char)item.codepoint;
            status_field[2] = ']';
            status_field[3] = ' ';
            field_truncated = show_utf8_copy_status_text(status_field, 4, codebuf);
         }
         else
         {
            glyph_cells = show_utf8_cluster_display_width(status_cluster_line,
                                                          status_cluster_len,
                                                          status_cluster);
            glyph_cells = min(glyph_cells, 8);
            status_cluster_display_width = glyph_cells;
            status_cluster_offset = 1;
            status_field[0] = '[';
            if (1 + glyph_cells < 20)
               status_field[1 + glyph_cells] = ']';
            if (2 + glyph_cells < 20)
               status_field[2 + glyph_cells] = ' ';
            code_col = min(19, glyph_cells + 3);
            field_truncated = show_utf8_copy_status_text(status_field, code_col, codebuf);
            draw_status_cluster = TRUE;
         }
      }
      else
      {
         glyph_cells = show_utf8_cluster_display_width(status_cluster_line,
                                                       status_cluster_len,
                                                       status_cluster);
         glyph_cells = min(glyph_cells, 8);
         status_cluster_display_width = glyph_cells;
         status_cluster_offset = 1;
         status_field[0] = '[';
         if (1 + glyph_cells < 20)
            status_field[1 + glyph_cells] = ']';
         if (2 + glyph_cells < 20)
            status_field[2 + glyph_cells] = ' ';
         code_col = min(19, glyph_cells + 3);
         field_truncated = show_utf8_copy_status_text(status_field, code_col, codebuf);
         draw_status_cluster = TRUE;
      }
      if (field_truncated)
      {
         status_field[17] = '.';
         status_field[18] = '.';
         status_field[19] = '.';
         status_field[20] = '\0';
      }
      if (status_field_width > 0)
      {
         memset((DEFCHAR *)linebuf + charpos, ' ', (size_t)status_field_width);
         memcpy((DEFCHAR *)linebuf + charpos, status_field,
                min(status_field_width, (int)strlen(status_field)));
      }
   }
#else
      {
         sprintf((DEFCHAR*)linebuf+max(0,(COLS-19)),"'%c'=%02X/%03d  ",
                       (unsigned char) ((key == 0) ? ' ' : key),key,key);
      }
#endif
   }
   else
      strcpy((DEFCHAR*)linebuf+max(0,(COLS-19)),"            ");
   /*
    * Display colour setting.
    */
#ifdef A_COLOR
   linebuf[max(0,(COLS-7))] = (colour_support) ? 'C' : 'c';
#else
   linebuf[max(0,(COLS-7))] = 'M';
#endif
   /*
    * Display REXX support character.
    */
   linebuf[max(0,(COLS-6))] = (rexx_support) ? REXX_INT_CHAR : ' ';
   /*
    * Display INSERTMODE toggle.
    */
   strcpy( (DEFCHAR*)linebuf + max( 0, (COLS-5) ), (INSERTMODEx) ? " INS " : "     " );
   /*
    * Refresh the STATUS LINE.
    */
   int stat_attr = ATTR_STATAREA;
#ifdef USE_SDSLH
   if (current_parser_severity == CB_ERROR) {
       stat_attr = ATTR_PMSGERROR;
   } else if (current_parser_severity == CB_WARNING) {
       stat_attr = ATTR_PMSGWARN;
   } else if (current_parser_severity == CB_INFORMATION) {
       stat_attr = ATTR_PMSGINFO;
   }
#endif

   chtype status_colour = set_colour( CURRENT_FILE->attr+stat_attr );

   INIT_LINE_OUTPUT( statarea, 0 );
   ADD_LINE_OUTPUT( linebuf, strlen( (DEFCHAR*)linebuf ), status_colour );
   END_LINE_OUTPUT();
#ifdef USE_UTF8
   if ( status_field_width > 0 )
   {
      show_fill_cells_at(statarea, 0, charpos, status_field_width, status_colour);
      show_write_ascii_cells_at(statarea, 0, charpos, status_field,
                                status_field_width, status_colour);
   }
   if ( draw_status_cluster )
   {
      show_write_utf8_status_cluster_at(statarea, 0,
                                        charpos + status_cluster_offset,
                                        status_cluster_line,
                                        status_cluster_len, status_cluster,
                                        status_colour,
                                        status_cluster_display_width);
   }
#endif
   curses_driver_refresh_window( statarea );
   TRACE_RETURN();
   return;
}
/***********************************************************************/
void clear_statarea(void)
/***********************************************************************/
{
   TRACE_FUNCTION("show.c:    clear_statarea");
   /*
    * If the status line is not displayed, don't do anything.
    */
   switch( STATUSLINEx )
   {
      case 'T':
      case 'B':
         {
             int stat_attr = ATTR_STATAREA;
#ifdef USE_SDSLH
             if (current_parser_severity == CB_ERROR) {
                 stat_attr = ATTR_PMSGERROR;
             } else if (current_parser_severity == CB_WARNING) {
                 stat_attr = ATTR_PMSGWARN;
             } else if (current_parser_severity == CB_INFORMATION) {
                 stat_attr = ATTR_PMSGINFO;
             }
#endif
             INIT_LINE_OUTPUT( statarea, 0 );
             FILL_LINE_OUTPUT(' ', COLS,
                              (CURRENT_VIEW == NULL || CURRENT_FILE == NULL) ? A_NORMAL :
                                     set_colour( CURRENT_FILE->attr+stat_attr ) );
             END_LINE_OUTPUT();
         }
         break;
      default:
         break;
   }
   TRACE_RETURN();
   return;
}
/***********************************************************************/
void display_filetabs( VIEW_DETAILS *start)
/***********************************************************************/
{
   VIEW_DETAILS *curr;
   FILE_DETAILS *first_view_file=NULL;
   bool process_view=FALSE;
   register int j=0;
   chtype normal, high;
   int fname_len, fill_len = COLS-2, extras;
   bool first = TRUE, more = FALSE;

   TRACE_FUNCTION("show.c:    display_filetabs");
   /*
    * If filetabs is not displayed, don't do anything.
    * Also, if scale line is not on, we don't display filetabs
    */
   if ( FILETABSx )
   {
      normal = set_colour( CURRENT_FILE->attr+ATTR_FILETABS );
      high = set_colour( CURRENT_FILE->attr+ATTR_FILETABSDIV );
      INIT_LINE_OUTPUT( filetabs, 0 );

      if ( start )
         curr = start;
      else
      {
         if ( filetabs_start_view == NULL )
            curr = vd_current;
         else
            curr = filetabs_start_view;
      }
      if ( number_of_files > 1 )
      {
         for ( j = 0; j < number_of_files; )
         {
            process_view = TRUE;
            if ( curr && curr->file_for_view && curr->file_for_view->file_views > 1 )
            {
               if ( first_view_file == curr->file_for_view )
                  process_view = FALSE;
               else
                  first_view_file = curr->file_for_view;
            }
            if ( process_view )
            {
               j++;
               if ( curr != CURRENT_VIEW
               &&   curr->file_for_view )
               {
                  fname_len = strlen( (DEFCHAR *)curr->file_for_view->fname );
                  if ( fname_len + 2 + DEFAULT_FILETABS_GAP_WIDTH > fill_len )
                  {
                     more = TRUE;
                     break;
                  }
                  if ( first )
                  {
                     first = FALSE;
                     extras = 0;
                     filetabs_start_view = curr;
                  }
                  else
                  {
                     char buf[DEFAULT_FILETABS_GAP_WIDTH + 1];
                     buf[0] = '|';
                     buf[1] = '\0';
                     ADD_LINE_OUTPUT( (CHARTYPE *)buf, DEFAULT_FILETABS_GAP_WIDTH, high );
                     extras = DEFAULT_FILETABS_GAP_WIDTH;
                  }

                  ADD_LINE_OUTPUT( curr->file_for_view->fname, fname_len, normal );
                  fill_len = fill_len - fname_len - extras;
               }
            }
            curr = curr->next;
            if (curr == NULL)
               curr = vd_first;
         }
      }
      FILL_LINE_OUTPUT( ' ', fill_len, normal );
      if ( more )
      {
         ADD_LINE_OUTPUT( (CHARTYPE *)"<>", 2, normal );
      }
      else
      {
         ADD_LINE_OUTPUT( (CHARTYPE *)"  ", 2, normal );
      }
      END_LINE_OUTPUT();
      curses_driver_refresh_window( filetabs );
   }
   TRACE_RETURN();
   return;
}
/***********************************************************************/
void redraw_window(WINDOW *win)
/***********************************************************************/
{
   TRACE_FUNCTION( "show.c:    redraw_window" );
   curses_driver_redraw_window(win);
   TRACE_RETURN();
   return;
}
#if NOT_USED
/***********************************************************************/
void repaint_screen(void)
/***********************************************************************/
{
   CursesDriverWindowCursor cursor;
   short y=0;

   TRACE_FUNCTION("show.c:    repaint_screen");

   cursor = curses_driver_capture_window_cursor(CURRENT_WINDOW);
   y = get_row_for_focus_line(current_screen,CURRENT_VIEW->focus_line,
                              CURRENT_VIEW->current_row);
   if (cursor.valid && cursor.col > CURRENT_SCREEN.cols[WINDOW_FILEAREA])
      cursor.col = 0;
   pre_process_line(CURRENT_VIEW,CURRENT_VIEW->focus_line,(LINE *)NULL);
   build_screen(current_screen);
   display_screen(current_screen);
   /* show_heading();*/
   curses_driver_move_window_cursor(CURRENT_WINDOW, y,
                                    cursor.valid ? cursor.col : 0);

   TRACE_RETURN();
   return;
}
#endif

/***********************************************************************/
void build_screen(CHARTYPE scrno)
/***********************************************************************/
{
   LINE *curr=NULL;
   LINE *save_curr=NULL;
   short crow = SCREEN_VIEW(scrno)->current_row;
   LINETYPE cline = SCREEN_VIEW(scrno)->current_line;

   TRACE_FUNCTION("show.c:    build_screen");
   hexshow_curr = save_curr = curr = lll_find(SCREEN_FILE(scrno)->first_line,SCREEN_FILE(scrno)->last_line,
                                              cline,SCREEN_FILE(scrno)->number_lines);
   displayed_max_line_length = 0;
   /*
    * Build the file contents from the current line to the bottom of the
    * window.
    */
   build_lines(scrno,DIRECTION_FORWARD,curr,(short)(screen[scrno].rows[WINDOW_FILEAREA]-crow),crow);
   /*
    * Build the file contents from the current line to the top of the
    * window.
    */
   curr = save_curr->prev;
   build_lines(scrno,DIRECTION_BACKWARD,curr,crow,(short)(crow-1));
   TRACE_RETURN();
   return;
}
/***********************************************************************/
void display_screen(CHARTYPE scrno)
/***********************************************************************/
{
   CursesDriverWindowCursor screen_cursor;
   CursesDriverWindowCursor previous_cursor = { 0, 0, 0 };
   short crow;

   TRACE_FUNCTION("show.c:    display_screen");
   /*
    * We don't display the screen if we are in a macro, running in batch,
    * running REPEAT command, or curses hasn't started yet...
    */
   if ( !interactive_in_macro )
   {
      if ( batch_only || in_macro || !curses_started )
      {
         TRACE_RETURN();
         return;
      }
   }
   first_screen_display = TRUE;
   /*
    * Turn off the cursor.
    */
   curses_driver_present_cursor(FALSE);
   /*
    * Display the IDLINE window...
    */
   show_heading(scrno);
   /*
    * Display the ARROW and CMDLINE if on...
    */
   if (SCREEN_WINDOW_ARROW(scrno) != NULL)
   {
      curses_driver_set_window_attr(SCREEN_WINDOW_ARROW(scrno),set_colour(SCREEN_FILE(scrno)->attr+ATTR_ARROW));
      redraw_window(SCREEN_WINDOW_ARROW(scrno));
      curses_driver_touch_window(SCREEN_WINDOW_ARROW(scrno));
      curses_driver_refresh_window(SCREEN_WINDOW_ARROW(scrno));
   }
   if (SCREEN_WINDOW_COMMAND(scrno) != NULL)
   {
      curses_driver_set_window_attr(SCREEN_WINDOW_COMMAND(scrno),set_colour(SCREEN_FILE(scrno)->attr+ATTR_CMDLINE));
      redraw_window(SCREEN_WINDOW_COMMAND(scrno));
      curses_driver_touch_window(SCREEN_WINDOW_COMMAND(scrno));
      curses_driver_refresh_window(SCREEN_WINDOW_COMMAND(scrno));
   }
   /*
    * Save the position of previous window if on command line.
    */
   if (SCREEN_VIEW(scrno)->current_window == WINDOW_COMMAND)
      previous_cursor = curses_driver_capture_window_cursor(
         SCREEN_PREV_WINDOW(scrno));
   screen_cursor = curses_driver_capture_window_cursor(SCREEN_WINDOW(scrno));
   cursor_focus_capture(scrno);
#ifdef USE_UTF8
   if (SCREEN_VIEW(scrno)->current_window == WINDOW_COMMAND)
      display_cmdline(scrno, SCREEN_VIEW(scrno));
#endif
   /*
    * Display the built lines...
    */
   crow = SCREEN_VIEW(scrno)->current_row;
   build_lines_for_display(scrno,DIRECTION_FORWARD,(short)(screen[scrno].rows[WINDOW_FILEAREA]-crow),crow);
   build_lines_for_display(scrno,DIRECTION_BACKWARD,crow,(short)(crow-1));
   /*
    * Check for nested comments if using a parser
    */
   if (SCREEN_FILE(scrno)->parser
   &&  SCREEN_FILE(scrno)->parser->have_paired_comments
   &&  SCREEN_VIEW(scrno)->syntax_headers & HEADER_COMMENT )
   {
      SHOW_HIGHLIGHTED_LINE( __LINE__, NULL, "Before paired comments" );
      parse_paired_comments(scrno,SCREEN_FILE(scrno));
      SHOW_HIGHLIGHTED_LINE( __LINE__, NULL, "After paired comments" );
   }

   show_lines(scrno);
   /*
    * If we have the file tabs window show it
    */
   if ( FILETABSx )
      display_filetabs( NULL );
   /*
    * Refresh the windows.
    */
   if (SCREEN_WINDOW_PREFIX(scrno) != NULL)
      curses_driver_refresh_window(SCREEN_WINDOW_PREFIX(scrno));
   if (SCREEN_WINDOW_GAP(scrno) != NULL)
      curses_driver_refresh_window(SCREEN_WINDOW_GAP(scrno));
   curses_driver_refresh_window(SCREEN_WINDOW_FILEAREA(scrno));
   /*
    * Lastly, turn the cursor back on again.
    */
   curses_driver_present_cursor(TRUE);
   /*
    * Restore the position of previous window if on command line.
    */
   if (SCREEN_VIEW(scrno)->current_window == WINDOW_COMMAND)
      curses_driver_restore_window_cursor(SCREEN_PREV_WINDOW(scrno),
                                          previous_cursor);
   curses_driver_restore_window_cursor(SCREEN_WINDOW(scrno), screen_cursor);
#if defined(HAVE_SB_INIT)
   if (SBx
   && scrno == current_screen)
   {
      sb_set_vert( 2 + CURRENT_FILE->number_lines + CURRENT_SCREEN.rows[WINDOW_FILEAREA],
                   CURRENT_SCREEN.rows[WINDOW_FILEAREA],
                   CURRENT_VIEW->current_line );
      sb_set_horz( displayed_max_line_length /* + CURRENT_SCREEN.cols[WINDOW_FILEAREA] */,
                   CURRENT_SCREEN.cols[WINDOW_FILEAREA],
                   CURRENT_VIEW->verify_col );
      sb_refresh();
   }
#endif
   TRACE_RETURN();
   return;
}
/***********************************************************************/
void display_cmdline( CHARTYPE curr_screen, VIEW_DETAILS *curr_view )
/***********************************************************************/
{
   CursesDriverWindowCursor command_cursor;

   TRACE_FUNCTION("show.c:    display_cmdline");
   if ( batch_only
   ||  !curses_started )
   {
      TRACE_RETURN();
      return;
   }
   if ( SCREEN_WINDOW_COMMAND(curr_screen) != NULL )
   {
      /*
       * Clear the cmdline from the beginning to the end
       * Display the contents of the cmdline from the cmd_verify_col
       */
      command_cursor = curses_driver_capture_window_cursor(
         SCREEN_WINDOW_COMMAND(curr_screen));
#ifdef USE_UTF8
      if (curr_screen == current_screen
      &&  curr_view->current_window == WINDOW_COMMAND)
         cursor_focus_capture(curr_screen);
#endif
      if ( inDIALOG )
         display_line_left( SCREEN_WINDOW_COMMAND(curr_screen), set_colour( curr_view->file_for_view->attr+ATTR_DIA_EDITFIELD), cmd_rec+cmd_verify_col-1, cmd_rec_len, 0, screen[curr_screen].cols[WINDOW_COMMAND] );
      else
         display_line_left( SCREEN_WINDOW_COMMAND(curr_screen), set_colour( curr_view->file_for_view->attr+ATTR_CMDLINE), cmd_rec+cmd_verify_col-1, cmd_rec_len, 0, screen[curr_screen].cols[WINDOW_COMMAND] );
#ifdef USE_UTF8
      show_draw_software_command_cursor(curr_screen, curr_view);
#endif
      curses_driver_refresh_window( SCREEN_WINDOW_COMMAND(curr_screen) );
      curses_driver_restore_window_cursor(SCREEN_WINDOW_COMMAND(curr_screen),
                                          command_cursor);
   }
   /* TODO */
   TRACE_RETURN();
   return;
}
/***********************************************************************/
void display_prefix_line( CHARTYPE curr_screen, VIEW_DETAILS *curr_view )
/***********************************************************************/
{
   CursesDriverWindowCursor prefix_cursor;
   LogicalCursor logical;
   int width;
   short row;

   TRACE_FUNCTION("show.c:    display_prefix_line");
   if ( batch_only
   ||  !curses_started
   ||  curr_view == NULL
   ||  SCREEN_WINDOW_PREFIX(curr_screen) == NULL )
   {
      TRACE_RETURN();
      return;
   }

   prefix_cursor = curses_driver_capture_window_cursor(
      SCREEN_WINDOW_PREFIX(curr_screen));
   row = prefix_cursor.valid ? prefix_cursor.row : 0;
   logical = curr_view->logical_cursor.current;
   if (curr_view->current_window == WINDOW_PREFIX
   &&  logical.valid
   &&  logical.zone == LOGICAL_CURSOR_ZONE_PREFIX
   &&  logical.zone_row >= 0
   &&  logical.zone_row < screen[curr_screen].rows[WINDOW_FILEAREA])
      row = (short)logical.zone_row;
   cursor_focus_capture(curr_screen);
   width = curr_view->prefix_width - curr_view->prefix_gap;
   display_line_left( SCREEN_WINDOW_PREFIX(curr_screen),
                      set_colour(curr_view->file_for_view->attr + ATTR_PENDING),
                      pre_rec,
                      width,
                      row,
                      width );
#ifdef USE_UTF8
   show_draw_software_prefix_cursor(curr_screen, row, NULL);
#endif
   curses_driver_refresh_window( SCREEN_WINDOW_PREFIX(curr_screen) );
   curses_driver_restore_window_cursor(SCREEN_WINDOW_PREFIX(curr_screen),
                                       prefix_cursor);
   TRACE_RETURN();
   return;
}
/***********************************************************************/
static void build_lines(CHARTYPE scrno,short direction,LINE *curr,
                         short rows,short start_row)
/***********************************************************************/
{
  /* BE CAREFUL! This function and his friend build_lines_for_display below
   * should always be changed in conjunction. EVER!
   * This function is been called by build_screen, an often called function.
   * Therefore needless things for RUNNING should be moved to
   * build_screen_for_display which is called before a true display. Put
   * things there for DISPLAYING. ...for_display relies on informations
   * computed here.
   */
   RESERVED *curr_rsrvd;
   LINETYPE num_shadow_lines=0;
   short tab_actual_row;
   short scale_actual_row;
   short hexshow_actual_start_row=0;
   SHOW_LINE *scurr;
   VIEW_DETAILS *screen_view;
   LINETYPE cline;
   FILE_DETAILS *screen_file;
   int display_rec,isTOForEOF,is_hexshow_on,has_reserveds,is_shadow,
       is_tab_on,is_scale_on;

   TRACE_FUNCTION("show.c:    build_lines");
   /*
    * These only need to be calculated once.
    */
   screen_view = SCREEN_VIEW(scrno);
   cline = screen_view->current_line;
   screen_file = SCREEN_FILE(scrno);
   is_hexshow_on = (screen_view->hexshow_on != FALSE);
   has_reserveds = (screen_file->first_reserved != NULL);
   is_shadow = (screen_view->shadow != FALSE);
   is_tab_on = (screen_view->tab_on != FALSE);
   is_scale_on = (screen_view->scale_on != FALSE);
   tab_actual_row=calculate_actual_row(screen_view->tab_base,screen_view->tab_off,screen[scrno].rows[WINDOW_FILEAREA],TRUE);
   scale_actual_row=calculate_actual_row(screen_view->scale_base,screen_view->scale_off,screen[scrno].rows[WINDOW_FILEAREA],TRUE);
   if (is_hexshow_on)
      hexshow_actual_start_row=calculate_actual_row(screen_view->hexshow_base,screen_view->hexshow_off,screen[scrno].rows[WINDOW_FILEAREA],TRUE);
   /*
    * Determine if the contents of "rec" should be used to display the
    * focus line.
    */
   if (display_screens > 1)
   {
      if (scrno == current_screen
      ||  SCREEN_FILE(current_screen) == SCREEN_FILE(other_screen))
         display_rec = 1;
      else
         display_rec = 0;
   }
   else
      display_rec = 1;
   /*
    * Determine the row that is the focus line.
    */
   if (direction == DIRECTION_BACKWARD)
      cline--;
   num_shadow_lines = 0;
   scurr = screen[scrno].sl + start_row;
   /*
    * Now, for each row to be displayed...
    */
   while(rows)
   {
      scurr->number_lines_excluded = 0;
      /*
       * If HEXSHOW is ON...
       */
      if (is_hexshow_on)
      {
         if (hexshow_actual_start_row == start_row
         ||  hexshow_actual_start_row+1 == start_row)
         {
            scurr->line_type = LINE_HEXSHOW;
            scurr->line_number = (-1L);
            scurr->main_enterable = FALSE;
            scurr->prefix_enterable = FALSE;
            scurr->highlight = FALSE;
            if (scrno == current_screen)
            {
               if (screen_view->current_line == screen_view->focus_line)
               {
                  scurr->contents = rec;
                  scurr->length = rec_len;
               }
               else
               {
                  scurr->contents = hexshow_curr->line;
                  scurr->length = hexshow_curr->length;
               }
            }
            else
            {
               if (screen_view->current_line == SCREEN_VIEW(current_screen)->focus_line
               &&  display_rec)
               {
                  scurr->contents = rec;
                  scurr->length = rec_len;
               }
               else
               {
                  scurr->contents = hexshow_curr->line;
                  scurr->length = hexshow_curr->length;
               }
            }
            /* other_start_col is used to determine if upper or lower line.
             * Doing this here allows ignoring hexshow_actual_start_row later.
             */
            if (hexshow_actual_start_row == start_row)
               scurr->other_start_col = 0;
            else
               scurr->other_start_col = 1;
            start_row += direction;
            scurr += direction;
            rows--;
            continue;
         }
      }
      /*
       * If the current line is a reserved line...
       */
      if (has_reserveds)                   /* at least one reserved line */
      {
         if ((curr_rsrvd = find_reserved_line(scrno,TRUE,start_row,0,0)) != NULL)
         {
            scurr->other_start_col = scurr->other_end_col = (LENGTHTYPE) -1;
            scurr->line_type = LINE_RESERVED;
            scurr->line_number = (-1L);
            scurr->current = (LINE *)NULL;
            scurr->main_enterable = FALSE;
            scurr->prefix_enterable = FALSE;
            scurr->highlight = FALSE;
            /* Save for later use, already correct if no prefix */
            scurr->contents = curr_rsrvd->disp;
            scurr->rsrvd = curr_rsrvd;
            scurr->length = curr_rsrvd->disp_length;
            scurr->normal_colour = set_colour(curr_rsrvd->attr);
            start_row += direction;
            scurr += direction;
            rows--;
            continue;
         }
      }
      /*
       * If the current line is the scale or tab line...
       */
      if ((is_scale_on && scale_actual_row == start_row)
      || (is_tab_on && tab_actual_row == start_row))
      {
         scurr->contents = NULL;
         scurr->line_number = (-1L);
         scurr->current = (LINE *)NULL;
         if ( compatible_feel == COMPAT_ISPF )
         {
            scurr->main_enterable = TRUE;
            scurr->prefix_enterable = TRUE;
         }
         else
         {
            scurr->main_enterable = FALSE;
            scurr->prefix_enterable = FALSE;
         }
         scurr->highlight = FALSE;
         scurr->line_type = LINE_LINE;
         if (is_tab_on && tab_actual_row == start_row)
            scurr->line_type |= LINE_TABLINE;
         if (is_scale_on && scale_actual_row == start_row)
            scurr->line_type |= LINE_SCALE;
         start_row += direction;
         scurr += direction;
         rows--;
         continue;
      }
      /*
       * If the current line is above or below TOF or EOF, set all to blank.
       */
      if (curr == NULL)
      {
         scurr->contents = NULL;
         scurr->line_type = (direction == DIRECTION_BACKWARD) ? LINE_OUT_OF_BOUNDS_ABOVE : LINE_OUT_OF_BOUNDS_BELOW;
         scurr->line_number = (-1L);
         scurr->current = (LINE *)NULL;
         scurr->main_enterable = FALSE;
         scurr->prefix_enterable = FALSE;
         scurr->highlight = FALSE;
         start_row += direction;
         scurr += direction;
         rows--;
         continue;
      }
      /*
       * If the current line is excluded, increment a running total.
       * Ignore the line if on TOF or BOF.
       */
      if (curr->next != NULL                         /* Bottom of file */
      &&  curr->prev != NULL)                           /* Top of file */
      {
         if (IN_SCOPE(screen_view,curr)
         || cline == screen_view->current_line)
/*       || curr->pre != NULL) */
            isTOForEOF = 0;
         else
         {
            if (num_shadow_lines == 0
            && direction == DIRECTION_FORWARD)
            {
               scurr->line_number = cline;
               scurr->current = curr;
            }
            num_shadow_lines++;
            cline += (LINETYPE)direction;
            isTOForEOF = 0;
            /* At this point we may reduce the runtime to about 60%
             * if we use a fast loop. This will prevent MUCH if any
             * of the above ifs will produce more overhead, e.g.
             * has_reserveds. Keep in mind that none of the above
             * ifs will ever do something useful at this point.
             * We do the necessary stuff only and for each direction.
             * This produces more code but it's REALLY worth.
             */
            if (direction == DIRECTION_FORWARD)
            {
               curr = curr->next; /* belonging to above shadow */
               for (;;)
               { /* like above useful checks */
                  if (curr->next == NULL)
                  {
                     isTOForEOF = 1;
                     break;
                  }
                  if (IN_SCOPE(screen_view,curr)
                  || cline == screen_view->current_line)
/*                  || curr->pre != NULL)*/
                     break;
                  num_shadow_lines++;
                  cline++;
                  curr = curr->next;
               }
            }
            else
            {
               curr = curr->prev; /* belonging to above shadow */
               for (;;)
               { /* like above useful checks */
                  if (curr->prev == NULL)
                  {
                     isTOForEOF = 1;
                     break;
                  }
                  if (IN_SCOPE(screen_view,curr)
                  || cline == screen_view->current_line)
/*                  || curr->pre != NULL)*/
                     break;
                  num_shadow_lines++;
                  cline--;
                  curr = curr->prev;
               }
            }
         }
      }
      else
         isTOForEOF = 1;
      /*
       * If we get here, we have to determine if a shadow line is to be
       * displayed or not.
       */
      if (is_shadow && num_shadow_lines > 0)
      {
         scurr->length = 0;
         if (direction != DIRECTION_FORWARD)
         {
            scurr->line_number = cline+1;
#if 1
            scurr->current = curr;
#else
            scurr->current = curr->next;
#endif
         }
         scurr->main_enterable = TRUE;
         scurr->prefix_enterable = TRUE;
         scurr->highlight = FALSE;
         scurr->number_lines_excluded = num_shadow_lines;
         scurr->line_type = LINE_SHADOW;
         if (compatible_feel == COMPAT_XEDIT)
            scurr->main_enterable = FALSE;
         num_shadow_lines = 0;
         start_row += direction;
         scurr += direction;
         rows--;
         continue;
      }
      /*
       * The remainder is for lines that are to be displayed.
       */
      scurr->line_number = cline;
      scurr->current = curr;
      /*
       * If the current row to be displayed is the focus line, display
       * the working area, rec and rec_len instead of the entry in the LL.
       */
      if (scrno == current_screen)
      {
         if (cline == screen_view->focus_line
         &&  display_rec)
         {
            scurr->contents = rec;
            scurr->length = rec_len;
         }
         else
         {
            scurr->contents = curr->line;
            scurr->length = curr->length;
         }
      }
      else
      {
         if (cline == SCREEN_VIEW(current_screen)->focus_line
         &&  display_rec)
         {
            scurr->contents = rec;
            scurr->length = rec_len;
         }
         else
         {
            scurr->contents = curr->line;
            scurr->length = curr->length;
         }
      }
      /*
       * Determine if the length of this row is longer than our last
       * saved longest line...
       */
      if ( scurr->length > displayed_max_line_length )
         displayed_max_line_length = scurr->length;
      scurr->main_enterable = TRUE;
      scurr->prefix_enterable = TRUE;
      scurr->highlight = FALSE;
      /*
       * Set up TOF and EOF lines...
       */
      if (isTOForEOF)
      {
#ifndef REMOVED_FOR_CONSISTANCY
         if (compatible_feel == COMPAT_XEDIT)
            scurr->main_enterable = FALSE;
#endif
         scurr->line_type = (curr->next==NULL)?LINE_EOF:LINE_TOF; /* MH12 */
      }
      else
      {
         scurr->line_type = LINE_LINE;
         if (screen_view->highlight)
         {
            switch(screen_view->highlight)
            {
               case HIGHLIGHT_TAG:
                  if (curr->flags.tag_flag)
                     scurr->highlight = TRUE;
                  break;
               case HIGHLIGHT_ALT:
                  if (curr->flags.new_flag
                  ||  curr->flags.changed_flag)
                     scurr->highlight = TRUE;
                  break;
               case HIGHLIGHT_SELECT:
                  if (curr->select >= screen_view->highlight_low
                  &&  curr->select <= screen_view->highlight_high)
                     scurr->highlight = TRUE;
                  break;
               default:
                  break;
            }
         }
      }
      start_row += direction;
      scurr += direction;
      rows--;

      cline += (LINETYPE)direction;
      if (direction == DIRECTION_FORWARD)
         curr = curr->next;
      else
         curr = curr->prev;
   }
   TRACE_RETURN();
   return;
}
/***********************************************************************/
static chtype merge_filectlchar_colour(chtype base, COLOUR_ATTR *ctlattr)
/***********************************************************************/
{
#ifdef A_COLOR
   int base_pair = PAIR_NUMBER(base & A_COLOR);
   int bg = BACKFROMPAIR(base_pair);
   int fg = FOREFROMPAIR(ctlattr->pair);
   int pair = ATTR2PAIR(fg,bg);

   return((base & ~A_COLOR) | COLOR_PAIR(pair) | ctlattr->mod);
#else
   return(base | ctlattr->mono);
#endif
}
/***********************************************************************/
static bool apply_ctlchar_to_file_line(FILE_DETAILS *screen_file, SHOW_LINE *scurr)
/***********************************************************************/
{
   LENGTHTYPE source=0,target=0;
   chtype current_colour;
   bool found;
   int j;

   if ( !screen_file->filectlchar
   ||   !CTLCHARx
   ||   scurr->contents == NULL
   ||   scurr->length < 1 )
      return(FALSE);

   current_colour = scurr->normal_colour;
   while( source < scurr->length
   &&     target < THE_MAX_SCREEN_WIDTH )
   {
      if ( scurr->contents[source] == ctlchar_escape
      &&   source + 1 < scurr->length )
      {
         found = FALSE;
         for ( j = 0; j < MAX_CTLCHARS; j++ )
         {
            if ( ctlchar_char[j] == scurr->contents[source+1] )
            {
               if ( ctlchar_attr[j].pair == -1 )
                  current_colour = scurr->normal_colour;
               else
                  current_colour = merge_filectlchar_colour(scurr->normal_colour,&ctlchar_attr[j]);
               source += 2;
               found = TRUE;
               break;
            }
         }
         if ( found )
            continue;
         source++;
         continue;
      }
      scurr->filectlchar_disp[target] = scurr->contents[source];
      scurr->highlighting[target] = current_colour;
      source++;
      target++;
   }
   scurr->filectlchar_disp[target] = '\0';
   scurr->contents = scurr->filectlchar_disp;
   scurr->length = target;
   scurr->is_highlighting = TRUE;
   return(TRUE);
}
/***********************************************************************/
static void build_lines_for_display(CHARTYPE scrno,short direction,
                                    short rows,short start_row)
/***********************************************************************/
{
   /*
    * should always be changed in conjunction. EVER!
    * This function is been called by display_screen only before a true screen
    * update. Therefore needless things for DISPLAYING should be placed here.
    * Put things needed for RUNNING must be placed in build_lines. This function
    * relies on informations computed in build_lines.
    */
   int marked = 0,is_cursor_line,is_cursor_line_filearea_different;
   bool current;
   int widthnogap,gap,h,len;
   SHOW_LINE *scurr;
   VIEW_DETAILS *screen_view;
   FILE_DETAILS *screen_file;
   int is_prefix_on;
   LINETYPE cline,off=0;
   chtype attr_block,
          attr_cblock,
          attr_filearea,
          attr_gap,
          attr_highlight,
          attr_chighlight,
          attr_prefix,
          attr_shadow,
          attr_curline,
          attr_cursor;
   bool line_parseable=FALSE;
   LINETYPE mark_start_line=0L;
   LINETYPE mark_end_line=0L;
   LENGTHTYPE mark_start_col=0;
   LENGTHTYPE mark_end_col=0;

   TRACE_FUNCTION("show.c:    build_lines_for_display");
#ifdef USE_SDSLH
   LINETYPE match_line1 = -1L;
   LENGTHTYPE match_col1 = -1;
   LINETYPE match_line2 = -1L;
   LENGTHTYPE match_col2 = -1;
   
   if (SCREEN_FILE(scrno)->colouring
   &&  SCREEN_FILE(scrno)->parser
   &&  SCREEN_FILE(scrno)->parser->is_sdslh_parser
   &&  SCREEN_FILE(scrno)->cb
   &&  CURRENT_VIEW == SCREEN_VIEW(scrno)) {
       LINETYPE screen_line=0;
       LENGTHTYPE screen_column=0;
       LINETYPE current_file_line=(-1L);
       LENGTHTYPE current_file_column=(-1);
       get_cursor_position(&screen_line, &screen_column, &current_file_line, &current_file_column);
       if (current_file_line > 0 && current_file_line <= (LINETYPE)SCREEN_FILE(scrno)->cb->line_count) {
           enter_codeblock_critical_section();
           CodeBufferLine *line = &SCREEN_FILE(scrno)->cb->lines[current_file_line - 1];
           if (current_file_column > 0 && current_file_column <= line->length) {
               CodeBufferCharacter *c = &line->characters[current_file_column - 1];
               if (c->node && (c->token_type == LEXER_LH_CODEBLOCK || c->token_type == LEXER_RH_CODEBLOCK || 
                               c->token_type == LEXER_LH_EXPR || c->token_type == LEXER_RH_EXPR || 
                               c->token_type == LEXER_LH_BLOCK || c->token_type == LEXER_RH_BLOCK || 
                               c->token_type == LEXER_SEPARATOR)) {
                   CB_Node *matched = cb_find_matching_bracket(c->node);
                   if (matched) {
                       size_t m_line = 0, m_col = 0;
                       get_code_buffer_part(SCREEN_FILE(scrno)->cb, matched->pos, 1, &m_line, &m_col, NULL);
                       if (m_line > 0) {
                           match_line1 = current_file_line - 1;
                           match_col1 = current_file_column - 1;
                           match_line2 = (LINETYPE)(m_line - 1);
                           match_col2 = (LENGTHTYPE)m_col;
                       }
                   }
               }
           }
           exit_codeblock_critical_section();
       }
   }
#endif
   /*
    * Determine the row that is the focus line.
    */
   scurr = screen[scrno].sl + start_row;
   /*
    * These only need to be calculated once.
    */
   screen_view = SCREEN_VIEW(scrno);
   screen_file = SCREEN_FILE(scrno);
   is_prefix_on = (screen_view->prefix != 0);
   /* commonly used attrs */
   attr_block     = set_colour(screen_file->attr+ATTR_BLOCK);
   attr_cblock    = set_colour(screen_file->attr+ATTR_CBLOCK);
   attr_filearea  = set_colour(screen_file->attr+ATTR_FILEAREA);
   attr_gap       = set_colour(screen_file->attr+ATTR_GAP);
   attr_highlight = set_colour(screen_file->attr+ATTR_HIGHLIGHT);
   attr_chighlight = set_colour(screen_file->attr+ATTR_CHIGHLIGHT);
   attr_prefix    = set_colour(screen_file->attr+ATTR_PREFIX);
   attr_shadow    = set_colour(screen_file->attr+ATTR_SHADOW);
   attr_curline   = set_colour(screen_file->attr+ATTR_CURLINE);
   attr_cursor    = set_colour(screen_file->attr+ATTR_CURSORLINE);
   gap = screen_view->prefix_gap;
   widthnogap = screen_view->prefix_width-gap;

   /*
    * Now, for each row to be displayed...
    */
   while( rows )
   {
      scurr->is_highlighting = FALSE;
      line_parseable = FALSE;
      scurr->is_current_line = FALSE;
      /*
       * Remove the highlight_type memory
       */
      if ( scurr->highlight_type )
      {
         (*the_free)(scurr->highlight_type);
         scurr->highlight_type = NULL;
      }
      /*
       * If this line is a hexshow line...
       */
      if ( scurr->line_type == LINE_HEXSHOW )
      {
         scurr->normal_colour = attr_shadow;
         if ( is_prefix_on )
         {
            scurr->prefix[0] = '\0';
            scurr->prefix_colour = attr_prefix;
            if ( gap )
            {
               scurr->gap_colour = attr_gap;
               scurr->gap[0] = '\0';
            }
         }
         start_row += direction;
         scurr += direction;
         rows--;
         continue;
      }
      /*
       * If the current line is a reserved line...
       */
      if ( scurr->line_type == LINE_RESERVED )
      {
         if ( CTLCHARx )
            scurr->is_highlighting = TRUE;
         /*
          * If the reserved line is to scroll with the filearea contents
          */
         if ( scurr->rsrvd->autoscroll )
         {
            if ( is_prefix_on )
            {
               scurr->prefix_colour = scurr->gap_colour = scurr->normal_colour;
               scurr->prefix[0] = '\0';
               scurr->prefix_colour = attr_prefix;
               scurr->gap_colour = attr_gap;
               scurr->gap[0] = '\0';
            }
            /*
             * For autoscroll reserved lines, we use the reserved line "highlighting"
             * structure member directly as it can be > THE_MAX_SCREEN_WIDTH character.
             * So we don't copy this data into the SHOW_LINE "highlighting" structure
             * member
             */
         }
         else
         {
            if ( is_prefix_on )
            {
               scurr->prefix_colour = scurr->gap_colour = scurr->normal_colour;
               len = scurr->length;
               if ( ( screen_view->prefix & PREFIX_LOCATION_MASK ) == PREFIX_LEFT )
               {
                  /* fill prefix with reserved line contents */
                  h = min( len, widthnogap );
                  memcpy( scurr->prefix, scurr->contents, h );
                  memcpy( scurr->prefix_highlighting, scurr->rsrvd->highlighting, h*sizeof(chtype) );
                  off = h; /* off now points to highlighting for gap */
                  scurr->prefix[h] = '\0';
                  scurr->contents += h;
                  len -= h;
                  /* fill gap with reserved line contents */
                  h = min( len, gap );
                  memcpy( scurr->gap, scurr->contents, h );
                  memcpy( scurr->gap_highlighting, scurr->rsrvd->highlighting+off, h * sizeof(chtype) );
                  off += h; /* off now points to highlighting for filearea */
                  scurr->gap[h] = '\0';
                  /* remainer of line goes in filearea */
                  if ((len -= h) == 0)
                     scurr->contents = NULL;
                  else
                  {
                     scurr->contents += h;
                     memcpy(scurr->highlighting,scurr->rsrvd->highlighting+off,len*sizeof(chtype));
                     scurr->length = len;
                  }
               }
               else /* prefix on right */
               {
                  scurr->length = min(len,screen[scrno].cols[WINDOW_FILEAREA]);
                  len -= scurr->length;
                  if (gap)
                  {
                     h = min(len,gap);
                     memcpy(scurr->gap,scurr->contents+scurr->length,h);
                     memcpy(scurr->gap_highlighting,scurr->rsrvd->highlighting+scurr->length,h*sizeof(chtype));
                     scurr->gap[h] = '\0';
                  }
                  else
                     h = 0;
                  /* now copy the rest to prefix if any */
                  len = min(len,widthnogap);
                  memcpy(scurr->prefix,scurr->contents+scurr->length+h,len);
                  memcpy(scurr->prefix_highlighting,scurr->rsrvd->highlighting+scurr->length+h,len*sizeof(chtype));
                  scurr->prefix[len] = '\0';
                  memcpy(scurr->highlighting,scurr->rsrvd->highlighting,scurr->length*sizeof(chtype));
               }
            }
            else
               memcpy(scurr->highlighting,scurr->rsrvd->highlighting,scurr->length*sizeof(chtype));
         }
         start_row += direction;
         scurr += direction;
         rows--;
         continue;
      }
      /*
       * If the current line is the scale line...
       */
      if (scurr->line_type & LINE_SCALE)
      {
         if (is_prefix_on)
         {
            if ( compatible_look == COMPAT_ISPF )
            {
               strcpy( (DEFCHAR *)scurr->prefix, "=COLS>" );
            }
            else
               scurr->prefix[0] = '\0';
            scurr->prefix_colour = attr_prefix;
            scurr->gap_colour = attr_gap;
            scurr->gap[0] = '\0';
         }
         scurr->normal_colour = set_colour(screen_file->attr+ATTR_SCALE);
         start_row += direction;
         scurr += direction;
         rows--;
         continue;
      }
      /*
       * If the current line is the tab line...
       */
      if (scurr->line_type & LINE_TABLINE)
      {
         if (is_prefix_on)
         {
            if ( compatible_look == COMPAT_ISPF )
            {
               strcpy( (DEFCHAR *)scurr->prefix, "=TABS>" );
            }
            else
               scurr->prefix[0] = '\0';
            scurr->prefix_colour = attr_prefix;
            scurr->gap_colour = attr_gap;
            scurr->gap[0] = '\0';
         }
         scurr->normal_colour = set_colour(screen_file->attr+ATTR_TABLINE);
         start_row += direction;
         scurr += direction;
         rows--;
         continue;
      }
      /*
       * If this line is above TOF or below EOF...
       */
      if ((scurr->line_type == LINE_OUT_OF_BOUNDS_ABOVE) ||
          (scurr->line_type == LINE_OUT_OF_BOUNDS_BELOW))
      {
         scurr->length = 0;
         scurr->normal_colour = attr_filearea;
         if (is_prefix_on)
         {
            scurr->prefix[0] = '\0';
            scurr->prefix_colour = attr_prefix;
            scurr->gap_colour = attr_gap;
            scurr->gap[0] = '\0';
         }
         start_row += direction;
         scurr += direction;
         rows--;
         continue;
      }
      cline = scurr->line_number;
      /*
       * If this line is a shadow line...
       */
      if (scurr->line_type == LINE_SHADOW)
      {
         if (direction == DIRECTION_FORWARD)
           set_prefix_contents(scrno,scurr->current,start_row,cline,FALSE);
         else
           set_prefix_contents(scrno,scurr->current->next,start_row,cline,FALSE);
         scurr->normal_colour = attr_shadow;
         start_row += direction;
         scurr += direction;
         rows--;
         continue;
      }
      /*
       * Determine if line being processed is the focus line.
       * Focus line determination out-ranks current line processing
       * later.
       */
      if ( cline == screen_view->focus_line )
      {
         is_cursor_line = 1;
         scurr->is_cursor_line = 1;
      }
      else
      {
         is_cursor_line = 0;
         scurr->is_cursor_line = 0;
      }
      if ( attr_filearea != attr_cursor )
      {
         is_cursor_line_filearea_different = 1;
         scurr->is_cursor_line_filearea_different = 1;
      }
      else
      {
         is_cursor_line_filearea_different = 0;
         scurr->is_cursor_line_filearea_different = 0;
      }
      /*
       * Determine if line being processed is the current line.
       */
      if (cline == screen_view->current_line)
         scurr->is_current_line = current = TRUE;
      else
         scurr->is_current_line = current = FALSE;
      /*
       * Determine if line being processed is in a marked block.
       */
      if (MARK_VIEW != (VIEW_DETAILS *)NULL
      &&  MARK_VIEW == screen_view)
      {
         if ( MARK_VIEW->mark_type == M_CUA )
         {
            if ( (MARK_VIEW->mark_start_line * max_line_length ) + MARK_VIEW->mark_start_col < (MARK_VIEW->mark_end_line * max_line_length ) + MARK_VIEW->mark_end_col )
            {
               mark_start_line = MARK_VIEW->mark_start_line;
               mark_end_line = MARK_VIEW->mark_end_line;
               mark_start_col = MARK_VIEW->mark_start_col;
               mark_end_col = MARK_VIEW->mark_end_col;
            }
            else
            {
               mark_start_line = MARK_VIEW->mark_end_line;
               mark_end_line = MARK_VIEW->mark_start_line;
               mark_start_col = MARK_VIEW->mark_end_col;
               mark_end_col = MARK_VIEW->mark_start_col;
            }
            if (cline >= mark_start_line
            &&  cline <= mark_end_line)
               marked = 1;
            else
               marked = 0;
         }
         else
         {
            if (cline >= MARK_VIEW->mark_start_line
            &&  cline <= MARK_VIEW->mark_end_line)
               marked = 1;
            else
               marked = 0;
         }
      }
      set_prefix_contents(scrno,scurr->current,start_row,cline,current);
      /*
       * If this line is TOF or EOF...
       */
      if ((scurr->line_type == LINE_TOF)
      ||  (scurr->line_type == LINE_EOF))
      {
         if ( is_cursor_line
         &&   is_cursor_line_filearea_different )
            scurr->normal_colour = attr_cursor;
         else
            scurr->normal_colour = (current) ? set_colour(screen_file->attr+ATTR_CTOFEOF)
                                             : set_colour(screen_file->attr+ATTR_TOFEOF);
      }
      else
      {
         /*
          * We have a LINE_LINE, so allocate space for our highlight_type
          */
         scurr->highlight_type = (unsigned char *)(*the_malloc)(scurr->length);
         if ( scurr->highlight_type )
            memset( scurr->highlight_type, ECOLOUR_NONE, scurr->length );
         if (marked)
         {
            switch(MARK_VIEW->mark_type)
            {
               case M_LINE:
                  scurr->normal_colour = (!current) ? attr_block : attr_cblock;
                  break;
               case M_BOX:
               case M_COLUMN:
               case M_WORD:
                  scurr->other_start_col = MARK_VIEW->mark_start_col - 1;
                  scurr->other_end_col = MARK_VIEW->mark_end_col - 1;
                  if (scurr->highlight)
                     scurr->normal_colour = (!current) ? attr_highlight : attr_chighlight;
                  else
                  {
                     if ( is_cursor_line
                     &&   is_cursor_line_filearea_different )
                        scurr->normal_colour = attr_cursor;
                     else
                        scurr->normal_colour = (!current) ? attr_filearea : attr_curline;
                  }
                  scurr->other_colour = (!current) ? attr_block : attr_cblock;
                  line_parseable = TRUE;
                  break;
               case M_STREAM:
                  if (scurr->highlight)
                     scurr->normal_colour = (!current) ? attr_highlight : attr_chighlight;
                  else
                  {
                     if ( is_cursor_line
                     &&   is_cursor_line_filearea_different )
                        scurr->normal_colour = attr_cursor;
                     else
                        scurr->normal_colour = (!current) ? attr_filearea : attr_curline;
                  }
                  scurr->other_colour = (!current) ? attr_block : attr_cblock;
                  scurr->other_end_col = MAX_INT;
                  scurr->other_start_col = 0;
                  if (cline == MARK_VIEW->mark_start_line)
                     scurr->other_start_col = MARK_VIEW->mark_start_col - 1;
                  if (cline == MARK_VIEW->mark_end_line)
                     scurr->other_end_col = MARK_VIEW->mark_end_col - 1;
                  if (cline > MARK_VIEW->mark_start_line
                  &&  cline < MARK_VIEW->mark_end_line)
                  {
                     scurr->normal_colour = (!current) ? attr_block : attr_cblock;
                  }
                  /*
                   * This can be more accurate. Only set true when the line
                   * is fully marked.
                   */
                  line_parseable = TRUE;
                  break;
               case M_CUA:
                  if (scurr->highlight)
                     scurr->normal_colour = (!current) ? attr_highlight : attr_chighlight;
                  else
                  {
                     if ( is_cursor_line
                     &&   is_cursor_line_filearea_different )
                        scurr->normal_colour = attr_cursor;
                     else
                        scurr->normal_colour = (!current) ? attr_filearea : attr_curline;
                  }
                  scurr->other_colour = (!current) ? attr_block : attr_cblock;
                  scurr->other_end_col = MAX_INT;
                  scurr->other_start_col = 0;

                  if (cline == mark_start_line)
                     scurr->other_start_col = mark_start_col - 1;
                  if (cline == mark_end_line)
                     scurr->other_end_col = mark_end_col - 1;
                  if (cline > mark_start_line
                  &&  cline < mark_end_line)
                  {
                     scurr->normal_colour = (!current) ? attr_block : attr_cblock;
                  }
                  /*
                   * This can be more accurate. Only set true when the line
                   * is fully marked.
                   */
                  line_parseable = TRUE;
                  break;
            }
         }
         else
         {
            scurr->other_start_col = scurr->other_end_col = (LENGTHTYPE) -1;
            if (scurr->highlight)
            {
               scurr->normal_colour = (!current) ? attr_highlight : attr_chighlight;
               scurr->other_colour = scurr->normal_colour;
            }
            else
            {
               if ( is_cursor_line
               &&   is_cursor_line_filearea_different )
                  scurr->normal_colour = attr_cursor;
               else
                  scurr->normal_colour = (!current) ? attr_filearea : attr_curline;
               scurr->other_colour = scurr->normal_colour;
            }
            line_parseable = TRUE;
         }
      }
      /*
       * If FILECTLCHAR is ON, interpret CTLCHAR sequences in normal file
       * lines for display and let that display markup take precedence over
       * parser-based syntax highlighting.
       */
      if (line_parseable
      &&  apply_ctlchar_to_file_line(SCREEN_FILE(scrno),scurr))
         line_parseable = FALSE;
      /*
       * If we are using colouring and we are not using the NULL parser and
       * the line has been determined as parseable, build the colours in
       * the highlighting array based on the line's contents.
       */
#ifdef USE_SDSLH
      if (line_parseable
      &&  SCREEN_FILE(scrno)->colouring
      &&  SCREEN_FILE(scrno)->parser
      &&  SCREEN_FILE(scrno)->parser->is_sdslh_parser
      &&  SCREEN_FILE(scrno)->cb
      &&  scurr->length > 0) {
          enter_codeblock_critical_section();
          LINETYPE cb_line_idx = scurr->line_number - 1;
          if (cb_line_idx >= 0 && cb_line_idx < (LINETYPE)SCREEN_FILE(scrno)->cb->line_count) {
              CodeBufferLine *cb_line = &SCREEN_FILE(scrno)->cb->lines[cb_line_idx];
              if (!scurr->highlight_type) {
                  scurr->highlight_type = (unsigned char *)(*the_malloc)(scurr->length);
              }
              if (scurr->highlight_type) {
                  scurr->is_highlighting = TRUE;
                  memset(scurr->highlight_type, ECOLOUR_NONE, scurr->length);
                  
                  chtype normal_colour;
                  if (scurr->is_cursor_line && scurr->is_cursor_line_filearea_different)
                      normal_colour = set_colour(SCREEN_FILE(scrno)->attr+ATTR_CURSORLINE);
                  else if (scurr->is_current_line)
                      normal_colour = set_colour(SCREEN_FILE(scrno)->attr+ATTR_CURLINE);
                  else
                      normal_colour = set_colour(SCREEN_FILE(scrno)->attr+ATTR_FILEAREA);
                      
                  for (size_t i = 0; i < THE_MAX_SCREEN_WIDTH; i++) {
                      scurr->highlighting[i] = normal_colour;
                  }

                  LENGTHTYPE vcol = SCREEN_VIEW(scrno)->verify_col - 1;
                  size_t min_len = scurr->length < cb_line->length ? scurr->length : cb_line->length;
                  for (size_t i = 0; i < min_len; i++) {
                      int type = LEXER_TOKEN;
                      int severity = CB_NONE;
                      
                      if (cb_line->characters != NULL) {
                          type = cb_line->characters[i].token_type;
                          severity = cb_line->characters[i].severity;
                      }

                      int ecolour_idx = ECOLOUR_NONE;
                      chtype current_colour = normal_colour;
                      
                      switch(type) {
                          case LEXER_COMMENT: 
                              ecolour_idx = ECOLOUR_COMMENTS;
                              break;
                          case LEXER_STRING_LITERAL: 
                              ecolour_idx = ECOLOUR_STRINGS;
                              break;
                          case LEXER_NUMBER_LITERAL: 
                              ecolour_idx = ECOLOUR_NUMBERS;
                              break;
                          case LEXER_KEYWORD:
                              ecolour_idx = ECOLOUR_KEYWORDS;
                              break;
                          case LEXER_PREPROCESSOR:
                              ecolour_idx = ECOLOUR_PREDIR;
                              break;
                          case LEXER_TYPE_IDENTIFIER:
                              ecolour_idx = ECOLOUR_TYPES;
                              break;
                          case LEXER_FUNCTION_IDENTIFIER:
                          case PARSE_TREE_FUNCTION:
                              ecolour_idx = ECOLOUR_FUNCTIONS;
                              break;
                          case LEXER_CONSTANT_IDENTIFIER:
                              ecolour_idx = ECOLOUR_CONSTANTS;
                              break;
                          case LEXER_IDENTIFIER:
                              ecolour_idx = ECOLOUR_LABEL;
                              break;
                          case LEXER_OPERATOR:
                          case LEXER_OPERATOR_ASSIGN:
                          case LEXER_OPERATOR_ARITHMETIC:
                          case LEXER_OPERATOR_LOGICAL:
                              ecolour_idx = ECOLOUR_OPERATOR;
                              break;
                          case LEXER_SEPARATOR:
                          case LEXER_STATEMENT_SEPARATOR:
                              ecolour_idx = ECOLOUR_PUNCTUATION;
                              break;
                          case LEXER_LH_BLOCK:
                          case LEXER_RH_BLOCK:
                          case LEXER_LH_CODEBLOCK:
                          case LEXER_RH_CODEBLOCK:
                          case LEXER_LH_EXPR:
                          case LEXER_RH_EXPR:
                              ecolour_idx = ECOLOUR_PAREN;
                              break;
                          default:
                              break;
                      }
                      
                      if (ecolour_idx != ECOLOUR_NONE) {
                          if (scurr->is_cursor_line && scurr->is_cursor_line_filearea_different)
                              current_colour = merge_curline_colour(SCREEN_FILE(scrno)->attr+ATTR_CURSORLINE, SCREEN_FILE(scrno)->ecolour+ecolour_idx);
                          else if (scurr->is_current_line)
                              current_colour = merge_curline_colour(SCREEN_FILE(scrno)->attr+ATTR_CURLINE, SCREEN_FILE(scrno)->ecolour+ecolour_idx);
                          else
                              current_colour = set_colour(SCREEN_FILE(scrno)->ecolour+ecolour_idx);
                      }

#ifdef USE_SDSLH
                       if (severity == CB_ERROR) {
                           current_colour = set_colour(SCREEN_FILE(scrno)->attr+ATTR_CBERROR);
                       } else if (severity == CB_WARNING) {
                           current_colour = set_colour(SCREEN_FILE(scrno)->attr+ATTR_CBWARN);
                       } else if (severity == CB_INFORMATION) {
                           current_colour = set_colour(SCREEN_FILE(scrno)->attr+ATTR_CBINFO);
                       }

                       if ((cb_line_idx == match_line1 && (LENGTHTYPE)i == match_col1) ||
                           (cb_line_idx == match_line2 && (LENGTHTYPE)i == match_col2)) {
                           current_colour |= A_REVERSE;
                       }
#endif                      
                      scurr->highlight_type[i] = ecolour_idx;
                      if (i >= vcol && i - vcol < THE_MAX_SCREEN_WIDTH) {
                          scurr->highlighting[i - vcol] = current_colour;
                      }
                  }
              }
          }
          exit_codeblock_critical_section();
      } else
#endif
      if (line_parseable
      &&  SCREEN_FILE(scrno)->colouring
      &&  SCREEN_FILE(scrno)->parser
      &&  scurr->length > 0 )
      {
         parse_line(scrno,SCREEN_FILE(scrno),scurr,start_row); /* test for error return */
         scurr->is_highlighting = TRUE;
         SHOW_HIGHLIGHTED_LINE( __LINE__, scurr, "Line parsing" );
      }
      start_row += direction;
      scurr += direction;
      rows--;
   }
   TRACE_RETURN();
   return;
}
/***********************************************************************/
static void show_lines(CHARTYPE scrno)
/***********************************************************************/
{
   short i=0;
   LENGTHTYPE j=0;
   LENGTHTYPE true_col=0;
   LENGTHTYPE off=0,num_tens=0;
   CHARTYPE tens[30];
   int gap = SCREEN_VIEW(scrno)->prefix_gap;
   int width = SCREEN_VIEW(scrno)->prefix_width-gap;
   LENGTHTYPE filearea_cols = screen[scrno].cols[WINDOW_FILEAREA];
   LENGTHTYPE max_cols = min(filearea_cols,SCREEN_VIEW(scrno)->verify_end-SCREEN_VIEW(scrno)->verify_start+1);
   WINDOW *screen_window_filearea = SCREEN_WINDOW_FILEAREA(scrno);
   char _THE_FAR buffer[60];
   CHARTYPE *ptr;
   SHOW_LINE *scurr = screen[scrno].sl;
#ifdef USE_UTF8
   UiFrame frame;
   const UiFrame *cursor_frame = NULL;
#endif

   TRACE_FUNCTION("show.c:    show_lines");
#ifdef USE_UTF8
   if (scrno == current_screen
   &&  current_cursor_uses_software()
   &&  screenframe_build(scrno, &frame))
      cursor_frame = &frame;
#endif
   for (i=0,scurr=screen[scrno].sl;i<screen[scrno].rows[WINDOW_FILEAREA];i++,scurr++)
   {
      /*
       * Display the contents of the prefix area (if on).
       */
      if (SCREEN_VIEW(scrno)->prefix)
      {
         if (scurr->line_type == LINE_RESERVED)
         {
            display_syntax_line_left(SCREEN_WINDOW_PREFIX(scrno),
                           scurr->prefix_colour,
                           scurr->prefix,
                           scurr->prefix_highlighting,
                           i,
                           width);
            if (SCREEN_VIEW(scrno)->prefix_gap)
               display_syntax_line_left(SCREEN_WINDOW_GAP(scrno),
                              scurr->gap_colour,
                              scurr->gap,
                              scurr->gap_highlighting,
                              i,
                              gap);
         }
         else
         {
            /* not a reserved line */
            display_line_left(SCREEN_WINDOW_PREFIX(scrno),
                           scurr->prefix_colour,
                           scurr->prefix,
#ifdef USE_UTF8
                           u8_strlen( (DEFCHAR *)scurr->prefix ),
#else
                           strlen( (DEFCHAR *)scurr->prefix ),
#endif
                           i,
                           width);
            if (SCREEN_VIEW(scrno)->prefix_gap)
            {
               char tmp_gap[21];
               /* as this is NOT a reserved line, nothing is displayed in the gap */
               /* except for a LINE if required */
               /* no need to use UTF-8 length here */
#ifdef ACS_VLINE
               memset( tmp_gap, ' ', SCREEN_VIEW(scrno)->prefix_gap );
               tmp_gap[SCREEN_VIEW(scrno)->prefix_gap] = '\0';
               display_line_left(SCREEN_WINDOW_GAP(scrno),
                              (SCREEN_VIEW(scrno)->prefix_gap_line ? scurr->gap_colour|A_ALTCHARSET|ACS_VLINE : scurr->gap_colour),
                              (CHARTYPE *)tmp_gap,
                              SCREEN_VIEW(scrno)->prefix_gap,
                              i,
                              gap);
#else
               memset( tmp_gap, (SCREEN_VIEW(scrno)->prefix_gap_line) ? '|' : ' ', SCREEN_VIEW(scrno)->prefix_gap );
               tmp_gap[SCREEN_VIEW(scrno)->prefix_gap] = '\0';
               display_line_left(SCREEN_WINDOW_GAP(scrno),
                              scurr->gap_colour,
                              (CHARTYPE *)tmp_gap,
                              SCREEN_VIEW(scrno)->prefix_gap,
                              i,
                              gap);
#endif
            }
         }
#ifdef USE_UTF8
         show_draw_software_prefix_cursor(scrno, i, cursor_frame);
#endif
      }
      /*
       * Display any shadow line. No need to test to see if SHADOW is ON as
       * number_excluded_lines would not be > 0 if SHADOW OFF.
       */
      if (scurr->number_lines_excluded > 0)
      {
         /* the following "%d" will be kedit compatible, the previous one
          * was "%4d" and NOT really centered (placed on (max/2)-14) width
          * excludes > 999.
          */
         sprintf(buffer," %lu",(unsigned long) scurr->number_lines_excluded);
         strcat(buffer + 2 /* at least */," line(s) not displayed ");
         /* no need to use UTF-8 length here */
         display_line_center(screen_window_filearea,
                             scurr->normal_colour,
                             (CHARTYPE *)buffer,
                             i,
                             filearea_cols,
                             '-');
         continue;
      }
      /*
       * Display SCALE and/or TABLINE...
       */
      if (scurr->line_type & LINE_SCALE
      ||  scurr->line_type & LINE_TABLINE)
      {
         true_col = SCREEN_VIEW(scrno)->verify_col-1;
         for (j=0,ptr=linebuf;j<max_cols;j++,ptr++,true_col++)
         {
            /*
             * Display '|' in current column position if this is the scale line.
             */
            if (scurr->line_type & LINE_SCALE
            &&  SCREEN_VIEW(scrno)->current_column == true_col+1)
            {
               *ptr = '|';
               continue;
            }
            /*
             * Display 'T' in each tab column. This overrides all other characters
             * except column position.
             */
            if (scurr->line_type & LINE_TABLINE)
            {
               if (is_tab_col(true_col+1))
               {
                  *ptr = '|';
                  continue;
               }
            }
            /*
             * Only display the following if it is a scale line...
             */
            if (scurr->line_type & LINE_SCALE)
            {
               if (SCREEN_VIEW(scrno)->margin_left-1 == true_col)
               {
                  *ptr = '[';
                  continue;
               }
               if (SCREEN_VIEW(scrno)->margin_right-1 == true_col)
               {
                  *ptr = ']';
                  continue;
               }
               if (SCREEN_VIEW(scrno)->margin_indent_offset_status
               &&  SCREEN_VIEW(scrno)->margin_left+SCREEN_VIEW(scrno)->margin_indent-1 == true_col)
               {
                  *ptr = 'p';
                  continue;
               }
               if (!SCREEN_VIEW(scrno)->margin_indent_offset_status
               &&  SCREEN_VIEW(scrno)->margin_indent-1 == true_col)
               {
                  *ptr = 'p';
                  continue;
               }
               if (SCREEN_VIEW(scrno)->zone_start-1 == true_col)
               {
                  *ptr = '<';
                  continue;
               }
               if (SCREEN_VIEW(scrno)->zone_end-1 == true_col)
               {
                  *ptr = '>';
                  continue;
               }
               if (true_col % 10 == 9)
               {
                  /* this is still not perfect, see "marg 1 100" with
                   * "verify 40 110". We should build the scale first.
                   */
                  sprintf((DEFCHAR *)tens,"%ld",(true_col / 10) + 1);
                  num_tens = strlen((DEFCHAR *)tens);
                  for (off = num_tens - 1;off >= 0;off--)
                  {
                     if (ptr - off < linebuf)
                        continue;
                     if ((off == 0) || (ptr[-off] == ' ') || (ptr[-off] == '.'))
                        ptr[-off] = tens[num_tens - off - 1];
                  }
                  continue;
               }
               if (true_col % 5 == 4)
               {
                  *ptr = '+';
                  continue;
               }
               *ptr = '.';
            }
            else /* only TABLINE */
              *ptr = ' ';
         }
         if (max_cols < filearea_cols)
            memset(ptr,' ',filearea_cols - max_cols);
         INIT_LINE_OUTPUT(screen_window_filearea,i);
         /* no need to use UTF-8 length here */
         ADD_LINE_OUTPUT(linebuf,
                         filearea_cols,
                         scurr->normal_colour);
         END_LINE_OUTPUT();
         continue;
      }
      /*
       * Display HEXSHOW line...
       */
      if (scurr->line_type & LINE_HEXSHOW)
      {
         curses_driver_clear_line_at(screen_window_filearea, i,
                                     scurr->normal_colour);
         show_hex_line(scrno,i);
         continue;
      }
      /*
       * Display TOF or EOF line.
       */
      if (scurr->line_type == LINE_TOF)
      {
         /* no need to use UTF-8 length here */
         display_line_left(screen_window_filearea,
                           scurr->normal_colour,
                           (SCREEN_VIEW(scrno)->tofeof) ? TOP_OF_FILE : (CHARTYPE*)"",
                           (SCREEN_VIEW(scrno)->tofeof) ? strlen( (DEFCHAR *)TOP_OF_FILE ) : 0,
                           i,
                           filearea_cols);
#ifdef USE_UTF8
         show_draw_filearea_marker_cursor(cursor_frame, scrno, i,
                                          scurr->line_number, UI_ROW_TOF,
                                          scurr->normal_colour);
#endif
         continue;
      }
      if (scurr->line_type == LINE_EOF)
      {
         /* no need to use UTF-8 length here */
         display_line_left(screen_window_filearea,
                           scurr->normal_colour,
                           (SCREEN_VIEW(scrno)->tofeof) ? BOTTOM_OF_FILE : (CHARTYPE*)"",
                           (SCREEN_VIEW(scrno)->tofeof) ? strlen( (DEFCHAR *)BOTTOM_OF_FILE ) : 0,
                           i,
                           filearea_cols);
#ifdef USE_UTF8
         show_draw_filearea_marker_cursor(cursor_frame, scrno, i,
                                          scurr->line_number, UI_ROW_EOF,
                                          scurr->normal_colour);
#endif
         continue;
      }
      /*
       * Display marked LINE block line(s).
       */
#ifdef USE_UTF8
      show_a_line(scrno,i,scurr,cursor_frame);
#else
      show_a_line(scrno,i,scurr);
#endif
   }
   if (SCREEN_WINDOW_PREFIX(scrno) != NULL)
      curses_driver_set_window_attr(SCREEN_WINDOW_PREFIX(scrno),set_colour(SCREEN_FILE(scrno)->attr+ATTR_PENDING));
   if (SCREEN_WINDOW_GAP(scrno) != NULL)
      curses_driver_set_window_attr(SCREEN_WINDOW_GAP(scrno),set_colour(SCREEN_FILE(scrno)->attr+ATTR_GAP));
   curses_driver_set_window_attr(screen_window_filearea,set_colour(SCREEN_FILE(scrno)->attr+ATTR_FILEAREA));
   TRACE_RETURN();
   return;
}
#define TMP_EXTRA 1
#ifdef USE_UTF8
static int show_utf8_cells_overlap(TextCluster cluster, LENGTHTYPE start_col, LENGTHTYPE end_col)
{
   int item_start;
   int item_end;

   if (end_col < start_col || end_col < 0)
      return 0;

   item_start = cluster.pos.cell_column;
   item_end = item_start + ((cluster.cell_width > 0) ? cluster.cell_width : 1) - 1;
   return (item_start <= end_col && item_end >= start_col);
}

static int show_utf8_byte_range_overlap(TextCluster cluster, LENGTHTYPE start, LENGTHTYPE length)
{
   size_t item_start;
   size_t item_end;
   size_t range_start;
   size_t range_end;

   if (length <= 0 || start < 0)
      return 0;

   item_start = cluster.pos.byte_offset;
   item_end = item_start + cluster.byte_length;
   range_start = (size_t)start;
   range_end = range_start + (size_t)length;
   return (item_start < range_end && item_end > range_start);
}

static int show_utf8_target_highlight_applies(CHARTYPE scrno, SHOW_LINE *scurr, TextCluster cluster)
{
   int i;

   if ( !SCREEN_VIEW(scrno)->thighlight_on
   ||   !SCREEN_VIEW(scrno)->thighlight_active
   ||   SCREEN_VIEW(scrno)->thighlight_target.true_line != scurr->line_number )
      return 0;

   for (i = 0; i < SCREEN_VIEW(scrno)->thighlight_target.num_targets; i++)
   {
      if ( SCREEN_VIEW(scrno)->thighlight_target.rt[i].found
      &&   !SCREEN_VIEW(scrno)->thighlight_target.rt[i].not_target
      &&   show_utf8_byte_range_overlap(cluster,
               SCREEN_VIEW(scrno)->thighlight_target.rt[i].start,
               SCREEN_VIEW(scrno)->thighlight_target.rt[i].found_length) )
      {
         return 1;
      }
   }
   return 0;
}

static void show_a_line_utf8_cells(CHARTYPE scrno, short row, SHOW_LINE *scurr,
                                   chtype *high, const UiFrame *frame)
{
   SHOW_LINE *current = &(screen[scrno].sl[row]);
   CHARTYPE *line = current->contents;
   LENGTHTYPE blength = current->length;
   LENGTHTYPE cvcol;
   LENGTHTYPE vlen;
   COLTYPE ccols = screen[scrno].cols[WINDOW_FILEAREA];
   int visible_cols;
   TextCellSlice slice;
   TextPos pos;
   int screen_col;
   chtype normal = current->normal_colour;
   chtype other = current->other_colour;
   chtype target_colour = set_colour(SCREEN_FILE(scrno)->attr + ATTR_THIGHLIGHT);
   int cursor_col = 0;
   int cursor_display_col = -1;
   int cursor_visible = FALSE;
   int cursor_drawn = FALSE;
   CursorShape cursor_shape = CURSOR_BLOCK;
   Utf8RepairPlan replacement_plan;
   Utf8RepairPlan old_replacement_plan;
   int replacement_clear_col = 0;

   if ( current->line_type == LINE_RESERVED
   &&   !current->rsrvd->autoscroll )
   {
      cvcol = 0;
      vlen = ccols;
   }
   else
   {
      cvcol = SCREEN_VIEW(scrno)->verify_col - 1;
      vlen = SCREEN_VIEW(scrno)->verify_end - SCREEN_VIEW(scrno)->verify_start + 1;
   }

   if ( ccols > vlen )
   {
      visible_cols = vlen;
   }
   else
   {
      visible_cols = ccols;
   }

   cursor_visible = show_filearea_cursor_col(frame, scrno, row,
                                             current->line_number,
                                             (int)cvcol, &cursor_col,
                                             &cursor_shape);
   if (cursor_visible
   &&  !show_filearea_cursor_display_col(frame, scrno, row,
                                         current->line_number, line, blength,
                                         (int)cvcol, &cursor_display_col))
      cursor_display_col = -1;

   /* Avoid shifting text when a viewport edge cuts through a wide code point. */
   slice = textpos_slice_cells(line, blength, (int)cvcol, visible_cols);
   replacement_plan = utf8_repair_plan_for_replacement(
                         line, blength, (int)cvcol, slice,
                         utf8_terminal_display_mode());
   replacement_clear_col = show_utf8_display_col_from_logical(
                              line, blength, (int)cvcol,
                              replacement_plan.start_pos.cell_column);
   if (show_utf8_line_replacement_hint_matches(current))
   {
      TextCellSlice old_slice = textpos_slice_cells(
                                   utf8_line_replacement_hint.line,
                                   utf8_line_replacement_hint.length,
                                   (int)cvcol, visible_cols);
      int old_clear_col;

      old_replacement_plan = utf8_repair_plan_for_replacement(
                                 utf8_line_replacement_hint.line,
                                 utf8_line_replacement_hint.length, (int)cvcol,
                                 old_slice, utf8_terminal_display_mode());
      old_clear_col = show_utf8_display_col_from_logical(
                         utf8_line_replacement_hint.line,
                         utf8_line_replacement_hint.length, (int)cvcol,
                         old_replacement_plan.start_pos.cell_column);
      if (utf8_repair_plan_prefer(&old_replacement_plan, old_clear_col,
                                  &replacement_plan, replacement_clear_col))
      {
         replacement_plan = old_replacement_plan;
         replacement_clear_col = old_clear_col;
      }
   }
   if (replacement_plan.extent == UTF8_REPAIR_EXTENT_SUFFIX)
   {
      int clear_col = replacement_clear_col;

      if (clear_col < 0)
         clear_col = 0;
      if (clear_col < ccols)
      {
         show_fill_cells_at(SCREEN_WINDOW_FILEAREA(scrno), row, clear_col,
                            ccols - clear_col, normal);
         curses_driver_touch_line(SCREEN_WINDOW_FILEAREA(scrno), row, 1);
         if (replacement_plan.flush != UTF8_REPAIR_FLUSH_NONE)
         {
            curses_driver_refresh_window(SCREEN_WINDOW_FILEAREA(scrno));
            curses_driver_update();
         }
      }
   }
   show_fill_cells_at(SCREEN_WINDOW_FILEAREA(scrno), row, 0, ccols, normal);

   pos = slice.start;
   screen_col = slice.leading_cells;
   while (pos.byte_offset < slice.end.byte_offset)
   {
      TextCluster cluster = textpos_cluster_at_boundary(line, blength, pos);
      chtype colour = normal;
      int item_width;
      int item_logical_screen_col;
      int item_screen_col;
      int item_display_width;
      int item_cursor_width;
      int item_paint_width;
      int clear_width;
      int cursor_logical_hit = FALSE;
      int cursor_display_hit = FALSE;

      if (cluster.byte_length == 0)
         break;

      item_width = (cluster.cell_width > 0) ? cluster.cell_width : 0;
      item_logical_screen_col = cluster.pos.cell_column - (int)cvcol;
      item_screen_col = screen_col;
      item_display_width = show_utf8_cluster_display_width(line, blength, cluster);
      item_cursor_width = show_utf8_cluster_cursor_width(line, blength, cluster);
      item_paint_width = show_utf8_cluster_paint_width(line, blength, cluster);
      if (item_cursor_width <= 0)
         item_cursor_width = (item_display_width > 0) ? item_display_width
                           : ((item_width > 0) ? item_width : 1);
      if (item_logical_screen_col < 0 || item_logical_screen_col >= visible_cols
      ||  item_screen_col >= ccols)
      {
         pos = cluster.end;
         screen_col += (item_display_width > 0) ? item_display_width : 1;
         continue;
      }

      if ( show_utf8_cells_overlap(cluster, current->other_start_col, current->other_end_col) )
      {
         colour = other;
      }
      else if ( current->is_highlighting
      &&        !current->highlight
      &&        high != NULL
      &&        cluster.pos.codepoint_index < THE_MAX_SCREEN_WIDTH )
      {
         colour = high[cluster.pos.codepoint_index];
      }

      if ( show_utf8_target_highlight_applies(scrno, scurr, cluster) )
         colour = target_colour;

      if ( cursor_visible
      &&   item_width > 0
      &&   cursor_col >= item_logical_screen_col
      &&   cursor_col < item_logical_screen_col + item_width )
         cursor_logical_hit = TRUE;
      if ( cursor_visible
      &&   cursor_display_col >= 0
      &&   cursor_display_col >= item_screen_col
      &&   cursor_display_col < item_screen_col + item_cursor_width )
         cursor_display_hit = TRUE;

      if ( cursor_logical_hit || cursor_display_hit )
      {
         colour = curses_driver_software_cursor_attr(scrno, colour,
                                                     cursor_shape);
         cursor_drawn = TRUE;
      }

      if (item_paint_width > 0)
         PARATEST_ADD_LINE(item_paint_width, "ADD_UTF8_CLUSTER_OUTPUT");
      clear_width = (item_paint_width > 0) ? item_paint_width : 1;
      if (item_screen_col + clear_width > ccols)
         clear_width = ccols - item_screen_col;
      show_fill_cells_at(SCREEN_WINDOW_FILEAREA(scrno), row, item_screen_col,
                         clear_width, colour);
      show_write_utf8_cluster_at(SCREEN_WINDOW_FILEAREA(scrno), row, item_screen_col,
                                 line, blength, cluster, colour,
                                 item_display_width);
      pos = cluster.end;
      screen_col += (item_display_width > 0) ? item_display_width : 1;
   }

   if ( cursor_visible
   &&  !cursor_drawn
   &&   cursor_col >= 0
   &&   cursor_col < ccols )
   {
      if (cursor_display_col < 0)
         cursor_display_col = show_utf8_display_col_from_logical(
                                 line, blength, (int)cvcol,
                                 (int)cvcol + cursor_col);
      if (cursor_display_col >= 0 && cursor_display_col < ccols)
         curses_driver_draw_software_blank_cell(scrno,
                                                SCREEN_WINDOW_FILEAREA(scrno),
                                                row, cursor_display_col,
                                                normal, cursor_shape);
   }
   if (replacement_plan.extent == UTF8_REPAIR_EXTENT_LINE)
      curses_driver_touch_line(SCREEN_WINDOW_FILEAREA(scrno), row, 1);
   if (show_utf8_line_replacement_hint_matches(current))
      show_utf8_clear_line_replacement_hint();
}

static chtype show_utf8_filearea_cluster_colour(CHARTYPE scrno, SHOW_LINE *scurr,
                                                TextCluster cluster, chtype *high)
{
   chtype colour = scurr->normal_colour;

   if ( show_utf8_cells_overlap(cluster, scurr->other_start_col, scurr->other_end_col) )
   {
      colour = scurr->other_colour;
   }
   else if ( scurr->is_highlighting
   &&        !scurr->highlight
   &&        high != NULL
   &&        cluster.pos.codepoint_index < THE_MAX_SCREEN_WIDTH )
   {
      colour = high[cluster.pos.codepoint_index];
   }

   if ( show_utf8_target_highlight_applies(scrno, scurr, cluster) )
      colour = set_colour(SCREEN_FILE(scrno)->attr + ATTR_THIGHLIGHT);

   return colour;
}

static void show_utf8_repaint_filearea_target(CHARTYPE scrno, short row,
                                              int logical_screen_col,
                                              bool cursor, CursorShape shape)
{
   SHOW_LINE *current;
   CHARTYPE *line;
   LENGTHTYPE blength;
   LENGTHTYPE cvcol;
   TextPos line_end;
   TextPos pos;
   TextCluster cluster;
   chtype normal;
   chtype colour;
   chtype *high;
   int display_col;
   int display_width;
   int paint_width;
   int ccols;

   if (SCREEN_WINDOW_FILEAREA(scrno) == NULL
   ||  row < 0
   ||  row >= screen[scrno].rows[WINDOW_FILEAREA])
      return;

   ccols = screen[scrno].cols[WINDOW_FILEAREA];
   if (logical_screen_col < 0 || logical_screen_col >= ccols)
      return;

   current = &(screen[scrno].sl[row]);
   normal = current->normal_colour;
   line = current->contents;
   if (line == NULL)
   {
      if (cursor)
         curses_driver_draw_software_blank_cell(scrno,
                                                SCREEN_WINDOW_FILEAREA(scrno),
                                                row, logical_screen_col,
                                                normal, shape);
      else
         show_fill_cells_at(SCREEN_WINDOW_FILEAREA(scrno), row,
                            logical_screen_col, 1, normal);
      return;
   }

   if ( current->line_type == LINE_RESERVED
   &&   current->rsrvd->autoscroll )
      high = current->rsrvd->highlighting + SCREEN_VIEW(scrno)->verify_col - 1;
   else
      high = current->highlighting;

   blength = current->length;
   cvcol = SCREEN_VIEW(scrno)->verify_col - 1;
   line_end = textpos_from_byte(line, blength, blength);
   if ((int)cvcol + logical_screen_col >= line_end.cell_column)
   {
      display_col = show_utf8_display_col_from_logical(line, blength,
                                                       (int)cvcol,
                                                       (int)cvcol + logical_screen_col);
      if (display_col >= 0 && display_col < ccols)
      {
         if (cursor)
            curses_driver_draw_software_blank_cell(
               scrno, SCREEN_WINDOW_FILEAREA(scrno), row, display_col, normal,
               shape);
         else
            show_fill_cells_at(SCREEN_WINDOW_FILEAREA(scrno), row,
                               display_col, 1, normal);
      }
      return;
   }

   pos = textpos_from_cell(line, blength, (int)cvcol + logical_screen_col,
                           TEXT_SNAP_BACKWARD);
   cluster = textpos_cluster_at_boundary(line, blength, pos);
   if (cluster.byte_length == 0)
      return;

   colour = show_utf8_filearea_cluster_colour(scrno, current, cluster, high);
   if (cursor)
      colour = curses_driver_software_cursor_attr(scrno, colour, shape);

   display_col = show_utf8_display_col_from_logical(line, blength, (int)cvcol,
                                                    cluster.pos.cell_column);
   display_width = show_utf8_cluster_display_width(line, blength, cluster);
   if (display_width <= 0)
      display_width = (cluster.cell_width > 0) ? cluster.cell_width : 1;
   paint_width = show_utf8_cluster_paint_width(line, blength, cluster);
   if (paint_width <= 0)
      paint_width = display_width;
   if (display_col < 0 || display_col >= ccols)
      return;
   if (display_col + paint_width > ccols)
      paint_width = ccols - display_col;

   if (paint_width > display_width)
   {
      TextPos repaint_pos = cluster.pos;
      int span_end = display_col + paint_width;

      show_fill_cells_at(SCREEN_WINDOW_FILEAREA(scrno), row, display_col,
                         paint_width, normal);
      while (repaint_pos.byte_offset < blength)
      {
         TextCluster repaint_cluster;
         chtype repaint_colour;
         int repaint_display_col;
         int repaint_display_width;

         repaint_cluster = textpos_cluster_at_boundary(line, blength, repaint_pos);
         if (repaint_cluster.byte_length == 0)
            break;
         repaint_display_col = show_utf8_display_col_from_logical(line, blength,
                                                                  (int)cvcol,
                                                                  repaint_cluster.pos.cell_column);
         if (repaint_display_col >= span_end || repaint_display_col >= ccols)
            break;
         repaint_display_width = show_utf8_cluster_display_width(line, blength,
                                                                 repaint_cluster);
         if (repaint_display_width <= 0)
            repaint_display_width = (repaint_cluster.cell_width > 0)
                                  ? repaint_cluster.cell_width : 1;
         if (repaint_display_col + repaint_display_width > ccols)
            repaint_display_width = ccols - repaint_display_col;
         repaint_colour = show_utf8_filearea_cluster_colour(scrno, current,
                                                            repaint_cluster, high);
         if (cursor
         &&  repaint_cluster.pos.byte_offset == cluster.pos.byte_offset)
            repaint_colour = curses_driver_software_cursor_attr(
               scrno, repaint_colour, shape);
         show_write_utf8_cluster_at(SCREEN_WINDOW_FILEAREA(scrno), row,
                                    repaint_display_col, line, blength,
                                    repaint_cluster, repaint_colour,
                                    repaint_display_width);
         repaint_pos = repaint_cluster.end;
      }
      return;
   }

   show_fill_cells_at(SCREEN_WINDOW_FILEAREA(scrno), row, display_col,
                      display_width, colour);
   show_write_utf8_cluster_at(SCREEN_WINDOW_FILEAREA(scrno), row, display_col,
                              line, blength, cluster, colour,
                              display_width);
}

static int show_utf8_target_cluster(CHARTYPE *line, LENGTHTYPE blength,
                                    LENGTHTYPE cvcol, int logical_screen_col,
                                    TextCluster *cluster,
                                    const Utf8TerminalProfileEntry **entry,
                                    int *display_col)
{
   TextPos line_end;
   TextPos pos;
   TextCluster target;

   if (cluster != NULL)
      memset(cluster, 0, sizeof(*cluster));
   if (entry != NULL)
      *entry = NULL;
   if (display_col != NULL)
      *display_col = -1;
   if (line == NULL || logical_screen_col < 0)
      return FALSE;

   line_end = textpos_from_byte(line, blength, blength);
   if ((int)cvcol + logical_screen_col >= line_end.cell_column)
      return FALSE;

   pos = textpos_from_cell(line, blength, (int)cvcol + logical_screen_col,
                           TEXT_SNAP_BACKWARD);
   target = textpos_cluster_at_boundary(line, blength, pos);
   if (target.byte_length == 0)
      return FALSE;

   if (cluster != NULL)
      *cluster = target;
   if (entry != NULL)
      *entry = show_utf8_cluster_profile(line, blength, target);
   if (display_col != NULL)
      *display_col = show_utf8_display_col_from_logical(line, blength,
                                                        (int)cvcol,
                                                        target.pos.cell_column);
   return TRUE;
}

static void show_utf8_repaint_filearea_suffix(CHARTYPE scrno, short row,
                                              SHOW_LINE *current,
                                              chtype *high,
                                              TextPos start_pos)
{
   CHARTYPE *line = current->contents;
   LENGTHTYPE blength = current->length;
   LENGTHTYPE cvcol = SCREEN_VIEW(scrno)->verify_col - 1;
   COLTYPE ccols = screen[scrno].cols[WINDOW_FILEAREA];
   TextPos pos = start_pos;

   while (pos.byte_offset < blength)
   {
      TextCluster cluster = textpos_cluster_at_boundary(line, blength, pos);
      chtype colour;
      int display_col;
      int display_width;
      int paint_width;
      int clear_width;

      if (cluster.byte_length == 0)
         break;
      display_col = show_utf8_display_col_from_logical(line, blength,
                                                       (int)cvcol,
                                                       cluster.pos.cell_column);
      if (display_col >= ccols)
         break;
      if (display_col < 0)
      {
         pos = cluster.end;
         continue;
      }

      display_width = show_utf8_cluster_display_width(line, blength, cluster);
      if (display_width <= 0)
         display_width = (cluster.cell_width > 0) ? cluster.cell_width : 1;
      paint_width = show_utf8_cluster_paint_width(line, blength, cluster);
      if (paint_width <= 0)
         paint_width = display_width;
      clear_width = paint_width;
      if (display_col + clear_width > ccols)
         clear_width = ccols - display_col;

      colour = show_utf8_filearea_cluster_colour(scrno, current, cluster, high);

      show_fill_cells_at(SCREEN_WINDOW_FILEAREA(scrno), row, display_col,
                         clear_width, colour);
      show_write_utf8_cluster_at(SCREEN_WINDOW_FILEAREA(scrno), row,
                                 display_col, line, blength, cluster,
                                 colour, display_width);
      pos = cluster.end;
   }
}

static int show_utf8_filearea_cursor_strategy_repaint(CHARTYPE scrno, short row,
                                                      int old_logical_screen_col,
                                                      int new_logical_screen_col,
                                                      CursorShape shape)
{
   SHOW_LINE *current;
   CHARTYPE *line;
   LENGTHTYPE blength;
   LENGTHTYPE cvcol;
   chtype normal;
   chtype *high;
   TextCluster old_cluster;
   TextCluster new_cluster;
   const Utf8TerminalProfileEntry *old_entry;
   const Utf8TerminalProfileEntry *new_entry;
   Utf8TerminalStrategy strategy;
   Utf8RepairPlan plan;
   int old_valid;
   int new_valid;
   int old_display_col = -1;
   int new_display_col = -1;
   TextPos start_pos;
   int start_display_col;
   int ccols;
   int line_end_display_col;
   int old_target_display_col;
   int new_target_display_col;
   int clear_end_col;
   int clear_width;

   if (SCREEN_WINDOW_FILEAREA(scrno) == NULL
   ||  row < 0
   ||  row >= screen[scrno].rows[WINDOW_FILEAREA])
      return FALSE;

   current = &(screen[scrno].sl[row]);
   line = current->contents;
   if (line == NULL)
      return FALSE;

   blength = current->length;
   cvcol = SCREEN_VIEW(scrno)->verify_col - 1;
   old_valid = show_utf8_target_cluster(line, blength, cvcol,
                                        old_logical_screen_col, &old_cluster,
                                        &old_entry, &old_display_col);
   new_valid = show_utf8_target_cluster(line, blength, cvcol,
                                        new_logical_screen_col, &new_cluster,
                                        &new_entry, &new_display_col);
   plan = utf8_repair_plan_for_cursor(line, blength, (int)cvcol,
                                      (int)cvcol + old_logical_screen_col,
                                      old_cluster, old_valid, old_entry,
                                      (int)cvcol + new_logical_screen_col,
                                      new_cluster, new_valid, new_entry);
   strategy = plan.strategy;
   if (plan.extent == UTF8_REPAIR_EXTENT_CHANGED_CELLS)
   {
      return FALSE;
   }

   if ( current->line_type == LINE_RESERVED
   &&   current->rsrvd->autoscroll )
      high = current->rsrvd->highlighting + SCREEN_VIEW(scrno)->verify_col - 1;
   else
      high = current->highlighting;

   if (plan.extent == UTF8_REPAIR_EXTENT_LINE)
   {
      show_a_line_utf8_cells(scrno, row, current, high, NULL);
      curses_driver_touch_line(SCREEN_WINDOW_FILEAREA(scrno), row, 1);
      return TRUE;
   }

   normal = current->normal_colour;
   ccols = screen[scrno].cols[WINDOW_FILEAREA];
   start_pos = plan.start_pos;
   start_display_col = show_utf8_display_col_from_logical(line, blength,
                                                          (int)cvcol,
                                                          start_pos.cell_column);
   if (start_display_col < 0)
      start_display_col = 0;
   if (start_display_col >= ccols)
   {
      return FALSE;
   }

   if (old_display_col >= 0 && old_display_col < start_display_col)
      show_utf8_repaint_filearea_target(scrno, row, old_logical_screen_col,
                                        FALSE, shape);
   else if (!old_valid)
      show_utf8_repaint_filearea_target(scrno, row, old_logical_screen_col,
                                        FALSE, shape);
   if (new_display_col >= 0 && new_display_col < start_display_col)
      show_utf8_repaint_filearea_target(scrno, row, new_logical_screen_col,
                                        TRUE, shape);

   line_end_display_col = show_utf8_display_col_from_logical(
      line, blength, (int)cvcol,
      textpos_from_byte(line, blength, blength).cell_column);
   old_target_display_col = show_utf8_display_col_from_logical(
      line, blength, (int)cvcol, (int)cvcol + old_logical_screen_col);
   new_target_display_col = show_utf8_display_col_from_logical(
      line, blength, (int)cvcol, (int)cvcol + new_logical_screen_col);
   clear_end_col = line_end_display_col;
   if (old_target_display_col + 1 > clear_end_col)
      clear_end_col = old_target_display_col + 1;
   if (new_target_display_col + 1 > clear_end_col)
      clear_end_col = new_target_display_col + 1;
   clear_end_col += 2;
   if (clear_end_col > ccols)
      clear_end_col = ccols;
   if (clear_end_col <= start_display_col)
      clear_end_col = start_display_col + 1;
   if (clear_end_col > ccols)
      clear_end_col = ccols;
   clear_width = clear_end_col - start_display_col;
   if (clear_width < 1)
      clear_width = 1;

   show_fill_cells_at(SCREEN_WINDOW_FILEAREA(scrno), row, start_display_col,
                      clear_width, normal);
   curses_driver_touch_line(SCREEN_WINDOW_FILEAREA(scrno), row, 1);
   if (plan.flush != UTF8_REPAIR_FLUSH_NONE)
   {
      curses_driver_refresh_window(SCREEN_WINDOW_FILEAREA(scrno));
      curses_driver_update();
   }

   show_utf8_repaint_filearea_suffix(scrno, row, current, high, start_pos);
   if (!new_valid || new_display_col >= start_display_col)
      show_utf8_repaint_filearea_target(scrno, row, new_logical_screen_col,
                                        TRUE, shape);
   return TRUE;
}

void show_utf8_filearea_cursor_transition(CHARTYPE scrno, short row,
                                          int old_logical_screen_col,
                                          int new_logical_screen_col)
{
   CursorShape shape = current_cursor_shape();

   if (show_utf8_filearea_cursor_strategy_repaint(scrno, row,
                                                  old_logical_screen_col,
                                                  new_logical_screen_col,
                                                  shape))
   {
      curses_driver_refresh_window(SCREEN_WINDOW_FILEAREA(scrno));
      return;
   }

   show_utf8_repaint_filearea_target(scrno, row, old_logical_screen_col,
                                     FALSE, shape);
   show_utf8_repaint_filearea_target(scrno, row, new_logical_screen_col,
                                     TRUE, shape);
   curses_driver_refresh_window(SCREEN_WINDOW_FILEAREA(scrno));
}
#endif
/***********************************************************************/
static void show_a_line(CHARTYPE scrno,short row, SHOW_LINE *scurr
#ifdef USE_UTF8
                        , const UiFrame *frame
#endif
)
/***********************************************************************/
{
   LENGTHTYPE vend,vlen,blanks_after_length;
   LENGTHTYPE blength,clength;
   LENGTHTYPE bvcol,cvcol;
   COLTYPE bcols,ccols;
   LENGTHTYPE bother_start_col,bother_end_col;
   LENGTHTYPE cother_start_col,cother_end_col;
   CHARTYPE *line;
   SHOW_LINE *current;
   int fillverify;
   chtype normal,other,*high;

   TRACE_FUNCTION("show.c:    show_a_line");
   /*
    * If the line to be displayed is a reserved line, set the columns to
    * be displayed so that the full line is displayed.
    */
   current = &(screen[scrno].sl[row]);
   line = current->contents;
   ccols = screen[scrno].cols[WINDOW_FILEAREA]; /* number of character columns displayed */
   normal = current->normal_colour;
   /*
    * If the RESERVED line is set to autoscroll, use the highlighting from the
    * reserved line structure, not the highlighting in the SHOW_LINE structure.
    * The reserved line "highlighting" has the complete line, the SHOW_LINE
    * "highlighting" only has up to THE_MAX_SCREEN_WIDTH characters
    */
   if ( current->line_type == LINE_RESERVED
   &&   current->rsrvd->autoscroll )
      high = current->rsrvd->highlighting+SCREEN_VIEW(scrno)->verify_col-1;
   else
      high = current->highlighting;
   blanks_after_length = 0;
   /*
    * If the contents are NULL then clear to eol in normal colour.
    */
   if ( line == NULL )
   {
      INIT_LINE_OUTPUT(SCREEN_WINDOW_FILEAREA(scrno),row);
      FILL_LINE_OUTPUT(' ',ccols,normal);
      END_LINE_OUTPUT();
#ifdef USE_UTF8
      {
         int cursor_col = 0;
         CursorShape cursor_shape = CURSOR_BLOCK;
         if (show_filearea_cursor_col(frame, scrno, row,
                                      current->line_number, 0,
                                      &cursor_col, &cursor_shape))
            curses_driver_draw_software_blank_cell(
               scrno, SCREEN_WINDOW_FILEAREA(scrno), row, cursor_col, normal,
               cursor_shape);
      }
#endif
      TRACE_RETURN();
      return;
   }

#ifdef USE_UTF8
   if (!show_utf8_line_is_ascii(line, current->length)
   ||  show_utf8_line_replacement_hint_matches(current)
   ||  !show_utf8_ascii_profile_fast_path_ok())
   {
      show_a_line_utf8_cells(scrno, row, scurr, high, frame);
      TRACE_RETURN();
      return;
   }
#endif

   if ( current->line_type == LINE_RESERVED
   &&   !current->rsrvd->autoscroll )
   {
      cvcol = 0;
      vend = ccols;
      vlen = ccols;
   }
   else
   {
      cvcol = SCREEN_VIEW(scrno)->verify_col - 1;
      vend = SCREEN_VIEW(scrno)->verify_end - 1;
      vlen = SCREEN_VIEW(scrno)->verify_end - SCREEN_VIEW(scrno)->verify_start + 1;
   }
   blength = current->length; /* length of line in bytes; ie actual space used */
#ifdef USE_UTF8
   bvcol = u8_offset( (char *)line, cvcol ); /* byte position in line; > or = cvcol */
   clength = u8_charnum( (char *)line, blength); /* length of line in UTF-8 characters; equal to or less than blength */
#else
   clength = blength;
   bvcol = cvcol;
#endif

#if defined( USE_UTF8 ) && defined(DEBUG1)
{
int ii,pos=0;
for(ii=0;pos<blength;ii++)
{
DEBUGDUMPDETAIL(fprintf(stderr,"%s %d: ii %d pos %d\n",__FILE__,__LINE__,ii,pos);)
u8_inc(line,&pos);
}
pos = 0;
for(ii=0;ii<blength;ii++)
{
DEBUGDUMPDETAIL(fprintf(stderr,"%s %d: ii %d off %d\n",__FILE__,__LINE__,ii,u8_offset((char *)line,ii));)
}
}
#endif
   other = current->other_colour;

   INIT_LINE_OUTPUT(SCREEN_WINDOW_FILEAREA(scrno),row);
   /*
    * If there is something past the VERIFY END column, fill it up with
    * blanks in normal colour first and adjust cols.
    */
   if ( ccols > vlen )
   {
      fillverify = ccols - vlen;
      ccols = vlen;
   }
   else
      fillverify = 0;

   cother_start_col = current->other_start_col;
   cother_end_col = current->other_end_col;
   line += bvcol; /* line now points to the first character of the verify column */

   /* This optimization will only work if memset and memcpy are
    * inline functions with fast assembler variants. This is
    * true in most cases with modern machines and compilers.
    */
   if ( cvcol >= clength ) /* line empty after cvcol? */
      memset( linebuf, ' ', ccols );
   else if ( cvcol + ccols <= clength ) /* line fills at least filearea? */
   {
#ifdef USE_UTF8
      bcols = u8_offset( (char *)line, ccols );
#else
      bcols = ccols;
#endif
      /*
       * Copy to our working buffer from the first byte of the verify column;
       * line points to that first byte
       */
      memcpy( linebuf, line, bcols + TMP_EXTRA);
DEBUGDUMPDETAIL(fprintf(stderr,"%s %d: Line exceeds window width: clength %d blength %d cvcol %d bvcol %d ccols %d bcols %d\n",__FILE__,__LINE__,clength,blength,cvcol,bvcol,ccols,bcols);)
   }
   else /* line must be padded with blanks */
   {
      blength -= bvcol;
      clength -= cvcol;
      memcpy( linebuf, line, blength );
      memset( linebuf + blength, ' ', ccols - clength + 1);
      blanks_after_length = ccols - clength;
DEBUGDUMPDETAIL(fprintf(stderr,"%s %d: Partial line: clength %d (cvcol %d) blength %d (bvcol %d) blanks_after_length %d\n",__FILE__,__LINE__,clength,cvcol,blength,bvcol,ccols - clength);)
   }

   if ( ( cvcol > cother_end_col )
   ||   ( cvcol + ccols - 1 < cother_start_col ) )
   {
      /*
       * To get here we are only displaying in one colour for the whole
       * line. ie no box block, but it could be a line block.
       */
      if ( current->is_highlighting
      && ( !current->highlight ) )
      {
         ADD_SYNTAX_LINE_OUTPUT(linebuf,ccols-blanks_after_length,high);
         FILL_LINE_OUTPUT(' ',blanks_after_length,normal);
      }
      else
      {
         ADD_LINE_OUTPUT(linebuf,ccols,normal);
      }
   }
   else
   {
#ifdef USE_UTF8
      bother_start_col = u8_offset( (char *)linebuf, cother_start_col );
      bother_end_col = u8_offset( (char *)linebuf, cother_end_col );
#else
      bother_start_col = cother_start_col;
      bother_end_col = cother_end_col;
#endif
DEBUGDUMPDETAIL(fprintf(stderr,"%s %d:  bother_start_col %d bother_end_col %d\n",__FILE__,__LINE__,bother_start_col,bother_end_col);)
DEBUGDUMPDETAIL(fprintf(stderr,"%s %d:  cother_start_col %d cother_end_col %d\n",__FILE__,__LINE__,cother_start_col,cother_end_col);)
      /* something must be displayed in another colour.
       * 1. display normal chars up to the start of other
       * 2. display another chars up to the end of other
       * 3. display normal chars until the end
       * first, setup stuff
       */
      cother_end_col -= cvcol;
      cother_end_col++;
      if ( cother_end_col > ccols )
         cother_end_col = ccols;
      /* other_end_col normalized to cols */
DEBUGDUMPDETAIL(fprintf(stderr,"%s %d:  bother_start_col %d bother_end_col %d\n",__FILE__,__LINE__,bother_start_col,bother_end_col);)
DEBUGDUMPDETAIL(fprintf(stderr,"%s %d:  cother_start_col %d cother_end_col %d\n",__FILE__,__LINE__,cother_start_col,cother_end_col);)

      line = linebuf; /* var line is unused now, will be incremented */
      /* other_start_col -= vcol; NOT allowed: vcol may be > other_start_col! */

      if ( cvcol < cother_start_col )
      {
         /*
          * Display the columns BEFORE the marked block
          * We can do syntax highlighting here
          */
         cother_start_col -= cvcol; /* don't do this above:
                                   * vcol may be > other_start_col! This leads
                                   * to an undetected underflow on unsigned's
                                   * which leads to a crash.
                                   */
         if ( current->is_highlighting )
         {
            ADD_SYNTAX_LINE_OUTPUT( line, cother_start_col, high );
            high          += cother_start_col;
         }
         else
         {
            ADD_LINE_OUTPUT(line,cother_start_col,normal);
         }
         ccols          -= cother_start_col;
         cother_end_col -= cother_start_col;
#ifdef USE_UTF8
         bother_start_col = u8_offset( (char *)line, cother_start_col );
#else
         bother_start_col = cother_start_col;
#endif
         line           += bother_start_col;
      }
#ifdef USE_UTF8
      bother_end_col = u8_offset( (char *)line, cother_end_col );
#else
      bother_end_col = cother_end_col;
#endif

      /*
       * Display the columns IN the the marked block
       * We DON'T do syntax highlighting here
       * other_end_col always exists
       */
      ADD_LINE_OUTPUT(line,cother_end_col,other);
      ccols          -= cother_end_col;
      line           += bother_end_col;
      if ( current->is_highlighting )
      {
         high          += cother_end_col;
      }
DEBUGDUMPDETAIL(fprintf(stderr,"%s %d: ccols %d cother_end_col %d bother_end_col %d line %c\n",__FILE__,__LINE__,ccols,cother_end_col,bother_end_col,*line);)
      if ( ccols )
      {
         /*
          * Display the columns AFTER the marked block
          * We can do syntax highlighting here
          */
         if (current->is_highlighting)
         {
            ADD_SYNTAX_LINE_OUTPUT( line, ccols, high );
         }
         else
         {
            ADD_LINE_OUTPUT(line,ccols,normal);
         }
      }
   }
   if ( fillverify )
      FILL_LINE_OUTPUT(' ',fillverify,normal);
#if ( defined(USE_XCURSES) || defined(USE_SDLCURSES) || defined(USE_WINGUICURSES) ) && PDC_BUILD >= 2501
   /*
    * PDCurses 2.5 XCurses port allows for the display of lines
    * between characters.  This is controlled by SET BOUNDMARK
    */
   if ( SCREEN_VIEW(scrno)->boundmark != BOUNDMARK_OFF
   &&   current->line_type != LINE_RESERVED )
   {
      int filearea_cols = screen[scrno].cols[WINDOW_FILEAREA],j;
      int bbm;
      short max_cols = min( filearea_cols, SCREEN_VIEW(scrno)->verify_end-SCREEN_VIEW(scrno)->verify_start+1), true_col;
      switch( SCREEN_VIEW(scrno)->boundmark )
      {
         case BOUNDMARK_ZONE:
            /* display left zone column */
            if ( is_column_being_shown(scrno, SCREEN_VIEW(scrno)->zone_start-1 ) )
            {
               bbm = SCREEN_VIEW(scrno)->zone_start-1-cvcol;
               linebufch[bbm] |= A_LEFTLINE;
            }
            /* display right zone column */
            if ( is_column_being_shown(scrno, SCREEN_VIEW(scrno)->zone_end-1 ) )
            {
               bbm = SCREEN_VIEW(scrno)->zone_end-1-cvcol;
               linebufch[bbm] |= A_RIGHTLINE;
            }
            break;
         case BOUNDMARK_TABS:
            true_col = SCREEN_VIEW(scrno)->verify_col-1;
            for ( j = 0; j < max_cols; j++, true_col++ )
            {
               if ( is_tab_col( true_col + 1 ) )
               {
                  bbm = true_col;
                  linebufch[bbm] |= A_LEFTLINE;
               }
            }
            break;
         case BOUNDMARK_MARGINS:
            if ( is_column_being_shown(scrno, SCREEN_VIEW(scrno)->margin_left-1 ) )
            {
               bbm = SCREEN_VIEW(scrno)->margin_left-1-cvcol;
               linebufch[bbm] |= A_LEFTLINE;
            }
            if ( is_column_being_shown(scrno, SCREEN_VIEW(scrno)->margin_right-1 ) )
            {
               bbm = SCREEN_VIEW(scrno)->margin_right-1-cvcol;
               linebufch[bbm] |= A_RIGHTLINE;
            }
            break;
         default:
            break;
      }
   }
#endif

   /*
    * If THIGHLIGHT is on and active and the line contains the target, display
    * it over the top of everything else.
    */
   if ( SCREEN_VIEW(scrno)->thighlight_on
   &&   SCREEN_VIEW(scrno)->thighlight_active
   &&   SCREEN_VIEW(scrno)->thighlight_target.true_line == scurr->line_number )
   {
      int i,j,idx;

      other = set_colour( SCREEN_FILE(scrno)->attr+ATTR_THIGHLIGHT );
      for (i = 0; i < SCREEN_VIEW(scrno)->thighlight_target.num_targets; i++)
      {
         /*
          * Change the colour of the text if the rtarget was found and it wasn't a
          * NOT boolean.
          */
         if ( SCREEN_VIEW(scrno)->thighlight_target.rt[i].found & !SCREEN_VIEW(scrno)->thighlight_target.rt[i].not_target )
         {
#ifdef USE_UTF8
            LENGTHTYPE cstart;
            cstart = u8_charnum( (char *)current->contents, SCREEN_VIEW(scrno)->thighlight_target.rt[i].start ); /* column position from start of line where thighlight starts */
            for ( j = 0; j < SCREEN_VIEW(scrno)->thighlight_target.rt[i].found_length; j++ )
            {
               idx = cstart-cvcol+j;
               if ( idx <= (vend-cvcol)
               &&   idx >= 0 )
               {
                  /*
                   * Get the current character at the column position and change its colour
                   */
                  curses_driver_recolour_cchar(&linebufch[idx], other);
               }
            }
#else
            if ( SCREEN_VIEW(scrno)->thighlight_target.rt[i].start < (bvcol+linebuf_size)
            &&  (SCREEN_VIEW(scrno)->thighlight_target.rt[i].start + SCREEN_VIEW(scrno)->thighlight_target.rt[i].found_length) > bvcol )
            {
               line = linebuf + SCREEN_VIEW(scrno)->thighlight_target.rt[i].start-bvcol;
               for ( j = 0; j < SCREEN_VIEW(scrno)->thighlight_target.rt[i].found_length; j++, line++ )
               {
                  idx = SCREEN_VIEW(scrno)->thighlight_target.rt[i].start-cvcol+j;
                  if ( idx <= (vend-cvcol)
                  &&   idx >= 0 )
                     linebufch[idx] = make_chtype( *line, other );
               }
            }
#endif
         }
      }
   }
   END_LINE_OUTPUT();
#ifdef USE_UTF8
   {
      int cursor_col = 0;
      CursorShape cursor_shape = CURSOR_BLOCK;

      if (show_filearea_cursor_col(frame, scrno, row,
                                   current->line_number,
                                   (int)SCREEN_VIEW(scrno)->verify_col - 1,
                                   &cursor_col, &cursor_shape))
         curses_driver_draw_software_chtype_cell(
            scrno, SCREEN_WINDOW_FILEAREA(scrno), row, cursor_col, normal,
            cursor_shape);
   }
#endif
   TRACE_RETURN();
   return;
}
/***********************************************************************/
static void set_prefix_contents(CHARTYPE scrno,LINE *curr,short start_row,LINETYPE cline,bool is_current)
/***********************************************************************/
{
   CHARTYPE *ptr=NULL;
   VIEW_DETAILS *screen_view = SCREEN_VIEW(scrno);
   FILE_DETAILS *screen_file;
   SHOW_LINE *scurr;
   int width;

   TRACE_FUNCTION("show.c:    set_prefix_contents");

   if (screen_view->prefix)
   {
      screen_file = SCREEN_FILE(scrno);
      scurr = screen[scrno].sl + start_row;
      ptr = scurr->prefix;
      width = screen_view->prefix_width-screen_view->prefix_gap;
      if (curr->pre != NULL)                /* prefix command pending... */
/*  && !blank_field(curr->pre->ppc_command))*/    /* ... and not blank */
      {
         strcpy( (DEFCHAR *)ptr, (DEFCHAR *)curr->pre->ppc_orig_command );
         scurr->prefix_colour = set_colour(screen_file->attr+ATTR_PENDING);
      }
      else                             /* no prefix command on this line */
      {
         scurr->prefix_colour = (is_current) ? set_colour(screen_file->attr+ATTR_CPREFIX)
                                             : set_colour(screen_file->attr+ATTR_PREFIX);
         if (screen_view->number)
         {
            if ((screen_view->prefix&PREFIX_STATUS_MASK) == PREFIX_ON)
               sprintf((DEFCHAR *)ptr,"%*.*ld", width, width, cline);
            else
               sprintf((DEFCHAR *)ptr,"%*ld", width, cline);
         }
         else if ((screen_view->prefix&PREFIX_STATUS_MASK) == PREFIX_ON)
         {
            memset(ptr,'=',width);
            scurr->prefix[width] = '\0';
         }
         else
            scurr->prefix[0] = '\0';
      }
      /*
       * clear the gap
       */
      scurr->gap[0] = '\0';
      scurr->gap_colour = (is_current) ? set_colour(screen_file->attr+ATTR_CGAP)
                                       : set_colour(screen_file->attr+ATTR_GAP);
   }
   TRACE_RETURN();
   return;
}
/***********************************************************************/
static void show_hex_line(CHARTYPE scrno,short row)
/***********************************************************************/
{
   register short i=0;
   LENGTHTYPE vcol=0,vlen=0;
   LENGTHTYPE length=0;
   CHARTYPE *line=NULL,*lptr;
   int upper_nibble = screen[scrno].sl[row].other_start_col == 0;
   COLTYPE cols;
   chtype normal;
   unsigned char c;
   SHOW_LINE *current;
   static char hexchars[] = "0123456789ABCDEF";

   TRACE_FUNCTION("show.c:    show_hex_line");
   /*
    * Set up columns to display...
    */
   vcol = SCREEN_VIEW(scrno)->verify_col - 1;
   vlen = SCREEN_VIEW(scrno)->verify_end - SCREEN_VIEW(scrno)->verify_start + 1;
   current = &(screen[scrno].sl[row]);
   length = current->length;
   line = current->contents;
   normal = current->normal_colour;

   cols = screen[scrno].cols[WINDOW_FILEAREA];
   /* adjust line and length to vcol */
   if (length < vcol)
      length = 0;
   else
   {
      length -= vcol;
      line += vcol;
   }
   /* don't display characters after VERIFY END or end of filearea */
   if (length > vlen)
      length = vlen;

   if (length > cols)
      length = cols;
   INIT_LINE_OUTPUT(SCREEN_WINDOW_FILEAREA(scrno),row);
   if (upper_nibble)
   {
      for (i=0,lptr=linebuf;i<length;i++,line++)
         *lptr++ = hexchars[(((unsigned) (*line))>>4) & 0x0F];
   }
   else
   {
      for (i=0,lptr=linebuf;i<length;i++)
         *lptr++ = hexchars[*line++ & 0x0F];
   }
   ADD_LINE_OUTPUT(linebuf,length,normal);
   if (length < cols)
   {
      c = ' ';
      if (upper_nibble)
         c >>= 4;
      FILL_LINE_OUTPUT(hexchars[(int)(c & 0x0F)],cols - length,normal);
   }
   END_LINE_OUTPUT();
   TRACE_RETURN();
   return;
}

/***********************************************************************/
void touch_screen(CHARTYPE scrno)
/***********************************************************************/
{
   register int i=0;
   WINDOW *win;

   TRACE_FUNCTION("commutil.c:touch_screen");
   for (i=0;i<VIEW_WINDOWS;i++)
   {
      win = screen[scrno].win[i];
      if (win != (WINDOW *)NULL)
         curses_driver_touch_window(win);
   }
   TRACE_RETURN();
   return;
}
/***********************************************************************/
void refresh_screen(CHARTYPE scrno)
/***********************************************************************/
{
   TRACE_FUNCTION("commutil.c:refresh_screen");
   /*
    * Turn off the cursor.
    */
   show_heading(scrno);
   if (SCREEN_WINDOW_FILEAREA(scrno) != SCREEN_WINDOW(scrno))
      curses_driver_refresh_window(SCREEN_WINDOW_FILEAREA(scrno));
   if (SCREEN_WINDOW_PREFIX(scrno) != (WINDOW *)NULL
   &&  SCREEN_WINDOW_PREFIX(scrno) != SCREEN_WINDOW(scrno))
      curses_driver_refresh_window(SCREEN_WINDOW_PREFIX(scrno));
   if (SCREEN_WINDOW_GAP(scrno) != (WINDOW *)NULL)
      curses_driver_refresh_window(SCREEN_WINDOW_GAP(scrno));
   if (SCREEN_WINDOW_ARROW(scrno) != (WINDOW *)NULL)
   {
      curses_driver_touch_window(SCREEN_WINDOW_ARROW(scrno));
      curses_driver_refresh_window(SCREEN_WINDOW_ARROW(scrno));
   }
   if (SCREEN_WINDOW_COMMAND(scrno) != (WINDOW *)NULL
   &&  SCREEN_WINDOW_COMMAND(scrno) != SCREEN_WINDOW(scrno))
      curses_driver_refresh_window(SCREEN_WINDOW_COMMAND(scrno));
   curses_driver_refresh_window(SCREEN_WINDOW(scrno));
   /*
    * Turn on the cursor.
    */
   TRACE_RETURN();
   return;
}
/***********************************************************************/
void redraw_screen(CHARTYPE scrno)
/***********************************************************************/
{
   TRACE_FUNCTION("commutil.c:redraw_screen");
   if (curses_started)
   {
      /*
       * Turn off the cursor. - no MH
       * MH    draw_cursor(FALSE);
       */
      if (SCREEN_WINDOW_COMMAND(scrno) != NULL)
      {
         curses_driver_set_window_attr(SCREEN_WINDOW_COMMAND(scrno),set_colour(SCREEN_FILE(scrno)->attr+ATTR_CMDLINE));
         curses_driver_touch_window(SCREEN_WINDOW_COMMAND(scrno));
         curses_driver_refresh_window(SCREEN_WINDOW_COMMAND(scrno));
      }
      if (SCREEN_WINDOW_ARROW(scrno) != NULL)
      {
         curses_driver_set_window_attr(SCREEN_WINDOW_ARROW(scrno),set_colour(SCREEN_FILE(scrno)->attr+ATTR_ARROW));
         curses_driver_touch_window(SCREEN_WINDOW_ARROW(scrno));
         curses_driver_refresh_window(SCREEN_WINDOW_ARROW(scrno));
      }
      if (SCREEN_WINDOW_IDLINE(scrno) != NULL)
      {
         curses_driver_set_window_attr(SCREEN_WINDOW_IDLINE(scrno),set_colour(SCREEN_FILE(scrno)->attr+ATTR_IDLINE));
         curses_driver_touch_window(SCREEN_WINDOW_IDLINE(scrno));
         curses_driver_refresh_window(SCREEN_WINDOW_IDLINE(scrno));
      }
      if (SCREEN_WINDOW_PREFIX(scrno) != NULL)
      {
         curses_driver_touch_window(SCREEN_WINDOW_PREFIX(scrno));
         curses_driver_refresh_window(SCREEN_WINDOW_PREFIX(scrno));
      }
      if (SCREEN_WINDOW_GAP(scrno) != NULL)
      {
         curses_driver_touch_window(SCREEN_WINDOW_GAP(scrno));
         curses_driver_refresh_window(SCREEN_WINDOW_GAP(scrno));
      }
      curses_driver_touch_window(SCREEN_WINDOW_FILEAREA(scrno));
      curses_driver_refresh_window(SCREEN_WINDOW_FILEAREA(scrno));
      /*
       * Turn on the cursor. - no MH
       * MH    draw_cursor(TRUE);
       */
   }
   TRACE_RETURN();
   return;
}

/***********************************************************************/
bool line_in_view(CHARTYPE scrno,LINETYPE line_number)
/***********************************************************************/
{
   register short i,max=screen[scrno].rows[WINDOW_FILEAREA];
   bool result=FALSE;
   SHOW_LINE *scurr;

   TRACE_FUNCTION("show.c:    line_in_view");
   scurr = screen[scrno].sl;
   for (i=0;i<max;i++,scurr++)
   {
      if (scurr->line_number == line_number)
      {
         result = TRUE;
         break;
      }
   }
   TRACE_RETURN();
   return(result);
}
/***********************************************************************/
bool column_in_view(CHARTYPE scrno,LENGTHTYPE column_number)
/***********************************************************************/
{
   bool result=FALSE;
   LENGTHTYPE min_file_col=0,max_file_col=0;

   TRACE_FUNCTION( "show.c:    column_in_view" );
   /*
    * This function is only valid in FILEAREA or CMDLINE
    */
   if ( screen[scrno].screen_view->current_window == WINDOW_COMMAND )
   {
      min_file_col = cmd_verify_col - 1;
      max_file_col = min_file_col + screen[scrno].cols[WINDOW_COMMAND] - 1;
   }
   else
   {
      min_file_col = screen[scrno].screen_view->verify_col - 1;
      max_file_col = min_file_col + screen[scrno].cols[WINDOW_FILEAREA] - 1;
   }

   if ( column_number >= min_file_col
   &&   column_number <= max_file_col )            /* new column in display */
      result = TRUE;
   TRACE_RETURN();
   return(result);
}
/***********************************************************************/
LINETYPE find_next_current_line(LINETYPE num_pages,short direction)
/***********************************************************************/
{
   register short i=0;
   LINETYPE cline = CURRENT_VIEW->current_line;
   short rows=0,num_display_lines=0,num_shadow_lines=0;
   LINE *curr=NULL;
   RESERVED *curr_reserved=CURRENT_FILE->first_reserved;
   short tab_actual_row=calculate_actual_row(CURRENT_VIEW->tab_base,CURRENT_VIEW->tab_off,CURRENT_SCREEN.rows[WINDOW_FILEAREA],TRUE);
   short scale_actual_row=calculate_actual_row(CURRENT_VIEW->scale_base,CURRENT_VIEW->scale_off,CURRENT_SCREEN.rows[WINDOW_FILEAREA],TRUE);

   TRACE_FUNCTION("show.c:    find_next_current_line");
   /*
    * Determine the number of file lines displayed...
    */
   num_display_lines = (CURRENT_SCREEN.rows[WINDOW_FILEAREA]) - 1;
   for (i=0;curr_reserved!=NULL;i++)
       curr_reserved = curr_reserved->next;
   num_display_lines -= i;
   if (CURRENT_VIEW->scale_on)
      num_display_lines--;
   if (CURRENT_VIEW->tab_on)
      num_display_lines--;
   if (CURRENT_VIEW->hexshow_on)
      num_display_lines = num_display_lines - 2;
   if (CURRENT_VIEW->scale_on
   &&  CURRENT_VIEW->tab_on
   &&  tab_actual_row == scale_actual_row)
      num_display_lines++;

   curr = lll_find(CURRENT_FILE->first_line,CURRENT_FILE->last_line,cline,CURRENT_FILE->number_lines);
   while(num_pages)
   {
      rows = num_display_lines;
      while(rows)
      {
         /*
          * If the current line is above or below TOF or EOF, set all to blank.
          */
         if (curr == NULL)
         {
            cline = (direction == DIRECTION_FORWARD) ? CURRENT_FILE->number_lines + 1L : 0L;
            num_pages = 1L;
            break;
         }
         /*
          * If the current line is excluded, increment a running total.
          * Ignore the line if on TOF or BOF.
          */
         if (curr->next != NULL                           /* Bottom of file */
         &&  curr->prev != NULL)                             /* Top of file */
         {
            if (!IN_SCOPE(CURRENT_VIEW,curr))
            {
               num_shadow_lines++;
               cline += (LINETYPE)direction;
               if (direction == DIRECTION_FORWARD)
                  curr = curr->next;
               else
                  curr = curr->prev;
               continue;
            }
         }
         /*
          * If we get here, we have to determine if a shadow line is to be
          * displayed or not.
          */
         if (CURRENT_VIEW->shadow
         && num_shadow_lines > 0)
         {
            num_shadow_lines = 0;
            rows--;
            continue;
         }
         rows--;
         cline += (LINETYPE)direction;
         if (direction == DIRECTION_FORWARD)
            curr = curr->next;
         else
            curr = curr->prev;
      }
      num_pages--;
   }
   if (direction == DIRECTION_FORWARD
   &&  cline > CURRENT_FILE->number_lines+1L)
      cline = CURRENT_FILE->number_lines+1L;
   if (direction == DIRECTION_BACKWARD
   &&  cline < 0L)
      cline = 0L;
   cline = find_next_in_scope(CURRENT_VIEW,(LINE *)NULL,cline,direction);
   TRACE_RETURN();
   return(cline);
}
/***********************************************************************/
short get_row_for_focus_line(CHARTYPE scrno,LINETYPE fl,short cr)
/***********************************************************************/
{
   /*
    * Returns the row within the main window where the focus line is
    * placed. If the focus line is off the screen, or out of bounds of the
    * current size of the file; <0 or >number_lines, this returns the
    * current row.
    */
   register short i=0,max=screen[scrno].rows[WINDOW_FILEAREA];
   SHOW_LINE *scurr;

   TRACE_FUNCTION("show.c:    get_row_for_focus_line");
   scurr = screen[scrno].sl;
   for (i=0;i<max;i++,scurr++)
   {
      if (scurr->line_number == fl)
      {
         TRACE_RETURN();
         return(i);
      }
   }
   TRACE_RETURN();
   return(cr);
}
/***********************************************************************/
LINETYPE get_focus_line_in_view(CHARTYPE scrno,LINETYPE fl,ROWTYPE row)
/***********************************************************************/
{
   /*
    * Returns a new focus line if the specified focus line is no longer
    * in view, or the same line number if that line is still in view.
    */
   ROWTYPE i,max=screen[scrno].rows[WINDOW_FILEAREA];
   SHOW_LINE *scurr;

   TRACE_FUNCTION("show.c:    get_focus_line_in_view");
   scurr = screen[scrno].sl + row;
   for (i=row;i<max;i++,scurr++)
   {
      if (scurr->line_number != (-1L))
      {
         TRACE_RETURN();
         return(scurr->line_number);
      }
   }
   scurr = screen[scrno].sl + row;
   for (i=row;i>0;i--,scurr--)
   {
      if (scurr->line_number != (-1L))
      {
         TRACE_RETURN();
         return(scurr->line_number);
      }
   }
   /*
    * We should never get here as there would be no editable lines in view
    */
   TRACE_RETURN();
   return(fl);
}
/***********************************************************************/
LINETYPE calculate_focus_line(LINETYPE fl,LINETYPE cl)
/***********************************************************************/
{
   /*
    * Returns the new focus line. If the focus line is still in the
    * window, it stays as is. If not,the focus   line becomes the current
    * line.
    */
   LINETYPE new_fl=(-1L);
   ROWTYPE i,max=CURRENT_SCREEN.rows[WINDOW_FILEAREA];
   SHOW_LINE *scurr;

   TRACE_FUNCTION("show.c:    calculate_focus_line");
   scurr = CURRENT_SCREEN.sl;
   for (i=0;i<max;i++,scurr++)
   {
      if (scurr->line_number == fl
      &&  (scurr->line_type == LINE_LINE
       || scurr->line_type == LINE_TOF   /* MH12 */
       || scurr->line_type == LINE_EOF)) /* MH12 */
      {
         new_fl = fl;
         break;
      }
   }
   if (new_fl == (-1L))
      new_fl = cl;
   TRACE_RETURN();
   return(new_fl);
}
/***********************************************************************/
char *get_current_position(CHARTYPE scrno,LINETYPE *line,LENGTHTYPE *col)
/***********************************************************************/
{
   short y=0,x=0;
   char *ret=NULL;
   SHOW_LINE *scurr;
   VIEW_DETAILS *view;
   LogicalCursor logical;

   TRACE_FUNCTION("show.c:    get_current_position");
   view = SCREEN_VIEW(scrno);
   logical = (view != NULL) ? view->logical_cursor.current
                            : logical_cursor_invalid();
   if (logical.valid)
   {
      switch (logical.zone)
      {
         case LOGICAL_CURSOR_ZONE_COMMAND:
            if (view->current_window == WINDOW_COMMAND)
            {
               *line = view->current_line;
               *col = (LENGTHTYPE)logical.text.cell_column + 1;
               TRACE_RETURN();
               return ret;
            }
            break;
         case LOGICAL_CURSOR_ZONE_FILEAREA:
            if (view->current_window == WINDOW_FILEAREA
            &&  screen[scrno].sl != NULL
            &&  logical.line_number == view->focus_line
            &&  logical.zone_row >= 0
            &&  logical.zone_row < screen[scrno].rows[WINDOW_FILEAREA])
            {
               scurr = screen[scrno].sl + logical.zone_row;
               *line = view->focus_line;
               *col = (LENGTHTYPE)logical.text.cell_column + 1;
               if (compatible_look == COMPAT_ISPF)
               {
                  if (scurr->line_type & LINE_TABLINE)
                     ret = "TABS";
                  else if (scurr->line_type & LINE_SCALE)
                     ret = "COLS";
                  else if (scurr->line_type & LINE_BOUNDS)
                     ret = "BNDS";
               }
               TRACE_RETURN();
               return ret;
            }
            break;
         case LOGICAL_CURSOR_ZONE_PREFIX:
            if (view->current_window == WINDOW_PREFIX
            &&  screen[scrno].sl != NULL
            &&  logical.line_number == view->focus_line
            &&  logical.zone_row >= 0
            &&  logical.zone_row < screen[scrno].rows[WINDOW_FILEAREA])
            {
               scurr = screen[scrno].sl + logical.zone_row;
               *line = view->focus_line;
               *col = (LENGTHTYPE)logical.text.cell_column + 1;
               if (compatible_look == COMPAT_ISPF)
               {
                  if (scurr->line_type & LINE_TABLINE)
                     ret = "TABS";
                  else if (scurr->line_type & LINE_SCALE)
                     ret = "COLS";
                  else if (scurr->line_type & LINE_BOUNDS)
                     ret = "BNDS";
               }
               TRACE_RETURN();
               return ret;
            }
            break;
         default:
            break;
      }
   }
   if ( curses_started )
   {
      CursesDriverWindowCursor cursor =
         curses_driver_capture_window_cursor(SCREEN_WINDOW(scrno));

      if (cursor.valid)
      {
         y = cursor.row;
         x = cursor.col;
      }
   }
   scurr = screen[scrno].sl + y;
   switch( SCREEN_VIEW(scrno)->current_window )
   {
      case WINDOW_COMMAND:
         *line = SCREEN_VIEW(scrno)->current_line;
         *col = (LENGTHTYPE)(x + 1 + cmd_verify_col - 1);
         break;
      case WINDOW_FILEAREA:
         *line = SCREEN_VIEW(scrno)->focus_line;
         *col = (LENGTHTYPE)x + SCREEN_VIEW(scrno)->verify_col;
         if ( compatible_look == COMPAT_ISPF )
         {
            if ( scurr->line_type & LINE_TABLINE )
               ret = "TABS";
            else if ( scurr->line_type & LINE_SCALE )
               ret = "COLS";
            else if ( scurr->line_type & LINE_BOUNDS )
               ret = "BNDS";
         }
         break;
      case WINDOW_PREFIX:
         *line = SCREEN_VIEW(scrno)->focus_line;
         *col = (LENGTHTYPE)(x + 1);
         if ( compatible_look == COMPAT_ISPF )
         {
            if ( scurr->line_type & LINE_TABLINE )
               ret = "TABS";
            else if ( scurr->line_type & LINE_SCALE )
               ret = "COLS";
            else if ( scurr->line_type & LINE_BOUNDS )
               ret = "BNDS";
         }
         break;
   }
   TRACE_RETURN();
   return ret;
}
/***********************************************************************/
void calculate_new_column( CHARTYPE curr_screen, VIEW_DETAILS *curr_view, COLTYPE current_screen_col, LENGTHTYPE current_verify_col, LENGTHTYPE new_file_col, COLTYPE *new_screen_col, LENGTHTYPE *new_verify_col )
/***********************************************************************/
{
   LINETYPE x=0;

   TRACE_FUNCTION( "show.c:    calculate_new_column" );
   if ( column_in_view( curr_screen, new_file_col ) )
   {
      *new_screen_col = (LENGTHTYPE)(new_file_col - (current_verify_col - 1));
      *new_verify_col = current_verify_col;
      TRACE_RETURN();
      return;
   }
   /*
    * To get here, we have new verify column...
    */
   x = screen[curr_screen].cols[curr_view->current_window] / 2;
   *new_verify_col = (LENGTHTYPE)max( 1L, (LINETYPE)new_file_col - x + 2L );
   *new_screen_col = (LENGTHTYPE)((*new_verify_col == 1) ? new_file_col : x - 1);
   TRACE_RETURN();
   return;
}
/***********************************************************************/
short prepare_view(CHARTYPE scrn)
/***********************************************************************/
{
   VIEW_DETAILS *screen_view = SCREEN_VIEW(scrn);

   TRACE_FUNCTION("show.c:    prepare_view");
   screen_view->current_row = calculate_actual_row(screen_view->current_base,
                                    screen_view->current_off,
                                    screen[scrn].rows[WINDOW_FILEAREA],TRUE);
   build_screen(scrn);
   if (!line_in_view(scrn,screen_view->focus_line))
   {
      screen_view->focus_line = screen_view->current_line;
      pre_process_line(screen_view,screen_view->focus_line,(LINE *)NULL);
      build_screen(scrn);
   }
   if (curses_started)
   {
      CursesDriverWindowCursor cursor =
         curses_driver_capture_window_cursor(SCREEN_WINDOW_FILEAREA(scrn));

      /* ensure column from WINDOW is in view */
      curses_driver_move_window_cursor(
         SCREEN_WINDOW_FILEAREA(scrn),
         get_row_for_focus_line(scrn, screen_view->focus_line,
                                screen_view->current_row),
         cursor.valid ? cursor.col : 0);
   }

   TRACE_RETURN();
   return(RC_OK);
}
/***********************************************************************/
short advance_view(VIEW_DETAILS *next_view,short direction)
/***********************************************************************/
{
   VIEW_DETAILS *save_current_view=next_view; /* point to passed view */
   CHARTYPE save_prefix=0;
   ROWTYPE save_cmd_line=0;
   short save_gap=0,save_prefix_width=0;
   bool save_id_line=0;
   short rc=RC_OK;

   TRACE_FUNCTION("show.c:    advance_view");
   /*
    * If this is the only file, ignore the command...
    */
   if (number_of_files < 2)
   {
      TRACE_RETURN();
      return(RC_OK);
   }
   /*
    * Reset the filetabs view
    */
   filetabs_start_view = NULL;
   /*
    * If we already have a current view, save some details of it...
    */
   if (CURRENT_VIEW)
   {
      save_prefix=CURRENT_VIEW->prefix;
      save_prefix_width = CURRENT_VIEW->prefix_width;
      save_gap=CURRENT_VIEW->prefix_gap;
      save_cmd_line=CURRENT_VIEW->cmd_line;
      save_id_line=CURRENT_VIEW->id_line;
     }
   memset(cmd_rec,' ',max_line_length);
   cmd_rec_len = 0;
   /*
    * If we have not passed a "next" view determine what the next view
    * will be...
    */
   if (!save_current_view)
   {
      post_process_line(CURRENT_VIEW,CURRENT_VIEW->focus_line,(LINE *)NULL,TRUE);
      /*
       * Get a temporary pointer to the "next" view in the linked list.
       */
      if (direction == DIRECTION_FORWARD)
      {
         if (CURRENT_VIEW->next == (VIEW_DETAILS *)NULL)
            save_current_view = vd_first;
         else
            save_current_view = CURRENT_VIEW->next;
      }
      else
      {
         if (CURRENT_VIEW->prev == (VIEW_DETAILS *)NULL)
            save_current_view = vd_last;
         else
            save_current_view = CURRENT_VIEW->prev;
      }
   }
   /*
    * Save the position of the cursor for the current view before getting
    * the contents of the new file...
    */
   if (curses_started)
   {
      if (CURRENT_WINDOW_COMMAND != NULL)
      {
         curses_driver_move_window_cursor(CURRENT_WINDOW_COMMAND, 0, 0);
         my_wclrtoeol(CURRENT_WINDOW_COMMAND);
      }
      {
         CursesDriverWindowCursor cursor =
            curses_driver_capture_window_cursor(CURRENT_WINDOW_FILEAREA);

         if (cursor.valid)
         {
            CURRENT_VIEW->y[WINDOW_FILEAREA] = cursor.row;
            CURRENT_VIEW->x[WINDOW_FILEAREA] = cursor.col;
         }
      }
      if (CURRENT_WINDOW_PREFIX != NULL)
      {
         CursesDriverWindowCursor cursor =
            curses_driver_capture_window_cursor(CURRENT_WINDOW_PREFIX);

         if (cursor.valid)
         {
            CURRENT_VIEW->y[WINDOW_PREFIX] = cursor.row;
            CURRENT_VIEW->x[WINDOW_PREFIX] = cursor.col;
         }
      }
   }
   /*
    * If more than one screen is displayed and the file displayed in each
    * screen is the same, remove the 'current' view from the linked list,
    * making the next view the current one. Only do this is the "next"
    * view is not the view in the other screen.
    */
   if (display_screens > 1)
   {
      if (CURRENT_SCREEN.screen_view->file_for_view == OTHER_SCREEN.screen_view->file_for_view)
      {
         if (CURRENT_VIEW->file_for_view == save_current_view->file_for_view)
         {
            if (direction == DIRECTION_FORWARD)
            {
               if (save_current_view->next == (VIEW_DETAILS *)NULL)
                  save_current_view = vd_first;
               else
                  save_current_view = save_current_view->next;
            }
            else
            {
               if (save_current_view->prev == (VIEW_DETAILS *)NULL)
                  save_current_view = vd_last;
               else
                  save_current_view = save_current_view->prev;
            }
         }
         free_a_view();
         CURRENT_VIEW = CURRENT_SCREEN.screen_view = save_current_view;
         OTHER_FILE->file_views--;
      }
      else
      {
         /*
          * First check if the file in the next view is the same as the file
          * being displayed in the other screen...
          */
         if (save_current_view->file_for_view == OTHER_FILE)
         {
            CURRENT_VIEW = CURRENT_SCREEN.screen_view = save_current_view;
            if ((rc = defaults_for_other_files(OTHER_VIEW)) != RC_OK)
            {
               TRACE_RETURN();
               return(rc);
            }
            CURRENT_SCREEN.screen_view = CURRENT_VIEW;
            CURRENT_FILE = CURRENT_SCREEN.screen_view->file_for_view = OTHER_FILE;
            CURRENT_FILE->file_views++;
         }
         else
            CURRENT_VIEW = CURRENT_SCREEN.screen_view = save_current_view;
      }
   }
   else  /* only one screen being displayed...less hassle */
   {
      /*
       * Make the current view the "next" one determined above.
       */
      CURRENT_VIEW = CURRENT_SCREEN.screen_view = save_current_view;
   }
   /*
    * If the position of the prefix or command line for the new view is
    * different from the previous view, rebuild the windows...
    */
   if ((save_prefix&PREFIX_LOCATION_MASK) != (CURRENT_VIEW->prefix&PREFIX_LOCATION_MASK)
   ||  save_gap != CURRENT_VIEW->prefix_gap
   ||  save_prefix_width != CURRENT_VIEW->prefix_width
   ||  save_cmd_line != CURRENT_VIEW->cmd_line
   ||  save_id_line != CURRENT_VIEW->id_line)
   {
      set_screen_defaults();
      if (curses_started)
      {
         if (set_up_windows(current_screen) != RC_OK)
         {
           TRACE_RETURN();
           return(RC_OK);
         }
      }
   }
   /*
    * Re-calculate CURLINE for the new view in case the CURLINE is no
    * longer in the display area.
    */
   prepare_view(current_screen);

   pre_process_line(CURRENT_VIEW,CURRENT_VIEW->focus_line,(LINE *)NULL);
   build_screen(current_screen);
   display_screen(current_screen);
   if (curses_started)
   {
      if (statarea != NULL)
      {
         int stat_attr = ATTR_STATAREA;
#ifdef USE_SDSLH
         if (current_parser_severity == CB_ERROR) {
             stat_attr = ATTR_PMSGERROR;
         } else if (current_parser_severity == CB_WARNING) {
             stat_attr = ATTR_PMSGWARN;
         } else if (current_parser_severity == CB_INFORMATION) {
             stat_attr = ATTR_PMSGINFO;
         }
#endif
         curses_driver_set_window_attr(statarea,set_colour(CURRENT_FILE->attr+stat_attr));
         redraw_window(statarea);
         curses_driver_touch_window(statarea);
      }

      if (divider != NULL)
      {
         if (display_screens > 1
         && !horizontal)
            curses_driver_set_window_attr(divider,set_colour(CURRENT_FILE->attr+ATTR_DIVIDER));
         curses_driver_touch_window(divider);
         curses_driver_refresh_window(divider);
      }
      curses_driver_move_window_cursor(CURRENT_WINDOW_FILEAREA,
                                       CURRENT_VIEW->y[WINDOW_FILEAREA],
                                       CURRENT_VIEW->x[WINDOW_FILEAREA]);
      if (CURRENT_WINDOW_PREFIX != NULL)
         curses_driver_move_window_cursor(CURRENT_WINDOW_PREFIX,
                                          CURRENT_VIEW->y[WINDOW_PREFIX],
                                          CURRENT_VIEW->x[WINDOW_PREFIX]);
      curses_driver_restore_window_cursor(
         CURRENT_WINDOW, curses_driver_capture_window_cursor(CURRENT_WINDOW));
   }
   TRACE_RETURN();
   return(RC_OK);
}

#if defined(CAN_RESIZE) || defined(OS2) || defined(WIN32)
/***********************************************************************/
short THE_Resize(int rows, int cols)
/***********************************************************************/
{
   short i=0;
   int length;
   int rc=RC_OK;
   TRACE_FUNCTION("show.c:    THE_Resize");
   /*
    * This function is called as the result of a screen resize.
    */
#if defined(SIGWINCH) && defined(USE_NCURSES) && defined(HAVE_RESIZETERM)
   if ( rows && cols )
      resizeterm(rows,cols);
   endwin();
   curses_driver_update();  /* make ncurses set LINES and COLS properly */
   curses_driver_refresh_window( stdscr );
   /* curses_driver_refresh_window( curscr ); */
   ncurses_screen_resized = FALSE;
#elif defined(HAVE_RESIZE_TERM)
   resize_term(rows,cols);
#endif
   terminal_lines = LINES;
   terminal_cols = COLS;
#ifdef HAVE_BSD_CURSES
   terminal_lines--;
#endif
   length = COLS + 1;
   if ( length > linebuf_size )
   {
      /* only resize linebuf and linebufch if the new terminal width is > the current size */
      linebuf_size = length;
      if ((linebuf = (CHARTYPE *)(*the_realloc)(linebuf,linebuf_size)) == NULL)
      {
         cleanup();
         TRACE_RETURN();
         return(30);
      }
   #ifdef USE_UTF8
      if ((linebufch = (cchar_t *)(*the_realloc)(linebufch, (linebuf_size * sizeof(cchar_t)))) == NULL)
   #else
      if ((linebufch = (chtype *)(*the_realloc)(linebufch, (linebuf_size * sizeof(chtype)))) == NULL)
   #endif
      {
         cleanup();
         TRACE_RETURN();
         return(30);
      }
   }
   if (screen_rows[0] != 0)
   {
      int offset = (STATUSLINEON()) ? 1 : 0;
      /*
       * 2 screens are displayed with different sizes. Attempt to
       * maintain the same ratio between the two.
       */
      screen_rows[0] = ((terminal_lines - offset) * screen_rows[0]) / (screen_rows[0] + screen_rows[1]);
      screen_rows[1] = (terminal_lines - offset) - screen_rows[0];
   }
   if (screen_cols[0] != 0)
   {
      /*
       * 2 screens are displayed with different sizes. Attempt to
       * maintain the same ratio between the two.
       */
      screen_cols[0] = (terminal_cols * screen_cols[0]) / (screen_cols[0] + screen_cols[1]);
      screen_cols[1] = terminal_cols - screen_cols[0];
   }
   set_screen_defaults();
   if (curses_started)
   {
      for (i=0;i<display_screens;i++)
      {
         if ((rc = set_up_windows(i)) != RC_OK)
         {
            TRACE_RETURN();
            return(rc);
         }
      }
   }
   create_statusline_window();
   create_filetabs_window();
#if defined(SIGWINCH) && defined(USE_NCURSES)
  /* restore_THE();  */
#endif
   TRACE_RETURN();
   return (RC_OK);
}
#endif

#if defined(HAVE_BROKEN_SYSVR4_CURSES)
/***********************************************************************/
short force_curses_background(void)
/***********************************************************************/
{
   int rc=RC_OK;
   short fg=0,bg=0;

   TRACE_FUNCTION("show.c:    force_curses_background");
   /*
    * This function is called to ensure that the background colour of the
    * first line on the screen is set to that which THE requests.  Some
    * curses implementations, notably Solaris 2.5, fail to set the
    * background to black.
    */
   if (colour_support)
   {
      pair_content(1,&fg,&bg);
      init_pair(1,COLOR_BLACK,COLOR_WHITE);
      move(0,0);
      attrset(COLOR_PAIR(1));
      addch(' ');
      init_pair(1,fg,bg);
   }
   TRACE_RETURN();
   return (RC_OK);
}
#endif
