/* PDCurses */

#include "pdcwin.h"

#include <stdlib.h>
#include <string.h>
#include <assert.h>

#ifdef PDC_WIDE
   #define USE_UNICODE_ACS_CHARS 1
#else
   #define USE_UNICODE_ACS_CHARS 0
#endif

#include "../common/acs_defs.h"

DWORD pdc_last_blink;
static bool blinked_off = FALSE;
static bool in_italic = FALSE;

/* position hardware cursor at (y, x) */

void PDC_gotoyx(int row, int col)
{
    COORD coord;

    PDC_LOG(("PDC_gotoyx() - called: row %d col %d from row %d col %d\n",
             row, col, SP->cursrow, SP->curscol));

    coord.X = (SHORT)col;
    coord.Y = (SHORT)row;

    SetConsoleCursorPosition(pdc_con_out, coord);
}

static void _set_ansi_color(short f, short b, attr_t attr)
{
    char esc[64], *p;
    short tmp, underline;
    bool italic, set_transparent_bg;
    size_t initial_len;

    if (f < 16 && !pdc_color[f].mapped)
        f = pdc_curstoansi[f];

    if (b < 16 && !pdc_color[b].mapped)
        b = pdc_curstoansi[b];

    if (attr & A_REVERSE)
    {
        tmp = f;
        f = b;
        b = tmp;
    }
    attr &= SP->termattrs;
    italic = !!(attr & A_ITALIC);
    underline = !!(attr & A_UNDERLINE);

    p = esc + sprintf(esc, "\x1b[");

    set_transparent_bg = (b == 0 && b != pdc_oldb);
    if (set_transparent_bg)
    {
        p += sprintf(p, "m\x1b[");
        pdc_oldb = b;
    }
    initial_len = strlen(esc);

    if (f != pdc_oldf || set_transparent_bg)
    {
        if (f < 8 && !pdc_color[f].mapped)
            p += sprintf(p, "%d", f + 30);
        else if (f < 16 && !pdc_color[f].mapped)
            p += sprintf(p, "%d", f + 82);
        else if (f < 256 && !pdc_color[f].mapped)
            p += sprintf(p, "38;5;%d", f);
        else
        {
            short red = DIVROUND(pdc_color[f].r * 255, 1000);
            short green = DIVROUND(pdc_color[f].g * 255, 1000);
            short blue = DIVROUND(pdc_color[f].b * 255, 1000);

            p += sprintf(p, "38;2;%d;%d;%d", red, green, blue);
        }

        pdc_oldf = f;
    }

    if (b != pdc_oldb)
    {
        if (strlen(esc) > initial_len)
            p += sprintf(p, ";");

        if (b < 8 && !pdc_color[b].mapped)
            p += sprintf(p, "%d", b + 40);
        else if (b < 16 && !pdc_color[b].mapped)
            p += sprintf(p, "%d", b + 92);
        else if (b < 256 && !pdc_color[b].mapped)
            p += sprintf(p, "48;5;%d", b);
        else
        {
            short red = DIVROUND(pdc_color[b].r * 255, 1000);
            short green = DIVROUND(pdc_color[b].g * 255, 1000);
            short blue = DIVROUND(pdc_color[b].b * 255, 1000);

            p += sprintf(p, "48;2;%d;%d;%d", red, green, blue);
        }

        pdc_oldb = b;
    }

    if (italic != in_italic || set_transparent_bg)
    {
        if (strlen(esc) > initial_len )
            p += sprintf(p, ";");

        if (italic)
            p += sprintf(p, "3");
        else
            p += sprintf(p, "23");

        in_italic = italic;
    }

    if (underline != pdc_oldu || set_transparent_bg)
    {
        if (strlen(esc) > initial_len )
            p += sprintf(p, ";");

        if (underline)
            p += sprintf(p, "4");
        else
            p += sprintf(p, "24");

        pdc_oldu = underline;
    }

    if (strlen(esc) > 2)
    {
        sprintf(p, "m");
        if (!pdc_conemu)
            SetConsoleMode(pdc_con_out, 0x0015);

        WriteConsoleA(pdc_con_out, esc, (DWORD)strlen(esc), NULL, NULL);

        if (!pdc_conemu)
            SetConsoleMode(pdc_con_out, 0x0010);
    }
}

/* see 'addch.c' for an explanation of how combining chars are handled. */

#ifdef USING_COMBINING_CHARACTER_SCHEME
   int PDC_expand_combined_characters( const cchar_t c, cchar_t *added);  /* addch.c */
#endif

#ifdef PDC_WIDE
#define IS_SUPPLEMENTAL_MULTILINGUAL_PLANE( c) ((c) & 0x1f0000)

#ifdef USING_COMBINING_CHARACTER_SCHEME
static WCHAR *_wincon_combined_text(chtype ch, int *units,
                                    chtype *base, chtype *last)
{
    cchar_t root = ch, added, *points;
    WCHAR *text;
    int count = 1, i;

    while (root > DUMMY_CHAR_NEXT_TO_FULLWIDTH)
    {
        root = PDC_expand_combined_characters(root, &added);
        count++;
    }
    *base = (chtype)root;
    *last = (chtype)root;
    points = (cchar_t *)malloc((size_t)count * sizeof(*points));
    text = (WCHAR *)malloc((size_t)count * 2 * sizeof(*text));
    if (!points || !text)
    {
        free(points);
        free(text);
        return NULL;
    }
    root = ch;
    for (i = count - 1; i > 0; i--)
        root = PDC_expand_combined_characters(root, &points[i]);
    points[0] = root;
    *last = (chtype)points[count - 1];
    *units = 0;
    for (i = 0; i < count; i++)
    {
        cchar_t cp = points[i];
        if (cp > 0xffff && cp < 0x110000)
        {
            text[(*units)++] = (WCHAR)(((cp - 0x10000) >> 10) | 0xd800);
            text[(*units)++] = (WCHAR)((cp & 0x3ff) | 0xdc00);
        }
        else
            text[(*units)++] = (WCHAR)cp;
    }
    free(points);
    return text;
}
#endif

/* CHAR_INFO has one UTF-16 unit per grid cell.  Expanding a combined PDC
   cell into several CHAR_INFOs moves every following cell, and the console
   then disagrees with curses' physical-screen cache.  Prefer a precomposed
   display scalar without changing the original logical cell.  A standalone
   mark (the status preview uses a space as its base) occupies its own cell.
   Other combinations without a single-scalar representation retain the base;
   the legacy console cannot represent an arbitrary cluster in one cell. */
static chtype _wincon_display_character(chtype ch)
{
#ifdef USING_COMBINING_CHARACTER_SCHEME
    if (ch > DUMMY_CHAR_NEXT_TO_FULLWIDTH)
    {
        WCHAR *text, *composed;
        int units = 0, folded = 0;
        chtype base, last, display;

        text = _wincon_combined_text(ch, &units, &base, &last);
        if (!text)
            return base;
        composed = (WCHAR *)malloc((size_t)units * sizeof(*composed));
        if (composed)
            folded = FoldStringW(MAP_PRECOMPOSED, text, units, composed, units);
        display = (base == ' ' ? last : base);
        if (folded == 1)
            display = composed[0];
        else if (folded == 2 && IS_HIGH_SURROGATE(composed[0])
                               && IS_LOW_SURROGATE(composed[1]))
            display = 0x10000 + ((composed[0] - 0xd800) << 10)
                                + composed[1] - 0xdc00;
        free(text);
        free(composed);
        return display;
    }
#endif
    return ch;
}
#endif

static void _show_run_of_ansi_characters( const attr_t attr,
                           const int fore, const int back, const bool blink,
                           const int lineno, const int x, const chtype *srcp, const int len)
{
#ifdef PDC_WIDE
    WCHAR buffer[MAX_PACKET_LEN * 2];
    int run_start = 0;
#ifdef USING_COMBINING_CHARACTER_SCHEME
    static bool repairing_columns = FALSE;
#endif
#else
    char buffer[MAX_PACKET_LEN];
#endif
    int j, n_out;

    _set_ansi_color( (short)fore, (short)back, attr);
    for (j = n_out = 0; j < len; j++)
    {
        chtype ch = srcp[j];

        if( _is_altcharset( ch))
            ch = acs_map[ch & 0x7f];

        if (blink && blinked_off)
            ch = ' ';

#ifdef PDC_WIDE
        ch &= A_CHARTEXT;
#ifdef USING_COMBINING_CHARACTER_SCHEME
        if (ch > DUMMY_CHAR_NEXT_TO_FULLWIDTH && !repairing_columns)
        {
            int units = 0;
            chtype base, last;
            WCHAR *text = _wincon_combined_text(ch, &units, &base, &last);
            CONSOLE_SCREEN_BUFFER_INFO after;
            int next_column = x + j + 1;

            if (next_column < SP->cols
                && (curscr->_y[lineno][next_column] & A_CHARTEXT)
                    == DUMMY_CHAR_NEXT_TO_FULLWIDTH)
                next_column++;

            if (n_out)
            {
                PDC_gotoyx(lineno, x + run_start);
                WriteConsoleW(pdc_con_out, buffer, n_out, NULL, NULL);
                n_out = 0;
            }
            PDC_gotoyx(lineno, x + j);
            if (text)
            {
                /* VT terminals receive the complete sequence.  Anchor the
                   following run to its logical column even on older hosts
                   which advance for a combining mark. */
                WriteConsoleW(pdc_con_out, text, units, NULL, NULL);
                free(text);
            }
            else
            {
                WCHAR fallback = (WCHAR)base;
                WriteConsoleW(pdc_con_out, &fallback, 1, NULL, NULL);
            }
            if (GetConsoleScreenBufferInfo(pdc_con_out, &after)
                && after.dwCursorPosition.Y == lineno
                && after.dwCursorPosition.X > next_column
                && next_column < SP->cols)
            {
                int spill = after.dwCursorPosition.X - next_column;

                if (spill > SP->cols - next_column)
                    spill = SP->cols - next_column;

                /* Some older console hosts give combining marks a column.
                   Repair only the overwritten neighbours from the logical
                   screen, including unchanged cells absent from this packet.
                   Unsupported clusters encountered during repair use the
                   bounded legacy representation, avoiding recursive spills. */
                repairing_columns = TRUE;
                PDC_transform_line_sliced(lineno, next_column, spill,
                                           curscr->_y[lineno] + next_column);
                repairing_columns = FALSE;
                _set_ansi_color((short)fore, (short)back, attr);
            }
            continue;
        }
        if (repairing_columns)
            ch = _wincon_display_character(ch);
#endif
        if( ch < MAX_UNICODE)
        {
            if (!n_out)
                run_start = j;
            if( IS_SUPPLEMENTAL_MULTILINGUAL_PLANE( ch))
            {
                buffer[n_out++] = (WCHAR)((ch - 0x10000) >> 10 | 0xD800); /* first UTF-16 unit */
                buffer[n_out++] = (WCHAR)(ch & 0x3FF) | 0xDC00;   /* second UTF-16 unit */
                if (j + 1 < len && (srcp[j + 1] & A_CHARTEXT) == ' ')
                    j++;  /* THE reserves a second cell for this scalar. */
            }
        else
            buffer[n_out++] = (WCHAR)ch;
        }
#else
        buffer[n_out++] = (char)( ch & A_CHARTEXT);
#endif
    }

#ifdef PDC_WIDE
    PDC_gotoyx(lineno, x + run_start);
    WriteConsoleW(pdc_con_out, buffer, n_out, NULL, NULL);
#else
    PDC_gotoyx(lineno, x);
    WriteConsoleA(pdc_con_out, buffer, n_out, NULL, NULL);
#endif
}

static void _show_run_of_nonansi_characters( attr_t attr,
                           int fore, int back, const bool blink,
                           const int lineno, const int x, const chtype *srcp, const int len)
{
    CHAR_INFO buffer[MAX_PACKET_LEN];
    COORD bufSize, bufPos;
    SMALL_RECT sr;
    WORD mapped_attr;
    int j;

    fore = pdc_curstoreal[fore];
    back = pdc_curstoreal[back];

    if (attr & A_REVERSE)
        mapped_attr = (WORD)( back | (fore << 4));
    else
        mapped_attr = (WORD)( fore | (back << 4));

    if (attr & A_UNDERLINE)
        mapped_attr |= 0x8000; /* COMMON_LVB_UNDERSCORE */
    if (attr & A_LEFT)
        mapped_attr |= 0x0800; /* COMMON_LVB_GRID_LVERTICAL */
    if (attr & A_RIGHT)
        mapped_attr |= 0x1000; /* COMMON_LVB_GRID_RVERTICAL */

    for (j = 0; j < len; j++)
    {
        buffer[j].Char.UnicodeChar = ' ';
        buffer[j].Attributes = mapped_attr;
    }
    for (j = 0; j < len; j++)
    {
        chtype ch = srcp[j];

        if( _is_altcharset( ch))
            ch = acs_map[ch & 0x7f];

        if (blink && blinked_off)
            ch = ' ';

#ifdef PDC_WIDE
        ch &= A_CHARTEXT;
        ch = _wincon_display_character(ch);
        if( ch < MAX_UNICODE)
        {
            if( IS_SUPPLEMENTAL_MULTILINGUAL_PLANE( ch))
            {
                if (j + 1 < len && ((srcp[j + 1] & A_CHARTEXT) == DUMMY_CHAR_NEXT_TO_FULLWIDTH
                                || (srcp[j + 1] & A_CHARTEXT) == ' '))
                {
                    buffer[j].Char.UnicodeChar = (WCHAR)(((ch - 0x10000) >> 10) | 0xd800);
                    buffer[j].Attributes |= COMMON_LVB_LEADING_BYTE;
                    buffer[++j].Char.UnicodeChar = (WCHAR)((ch & 0x3ff) | 0xdc00);
                    buffer[j].Attributes |= COMMON_LVB_TRAILING_BYTE;
                }
                else
                    buffer[j].Char.UnicodeChar = 0xfffd;
            }
            else
            {
                buffer[j].Char.UnicodeChar = (WCHAR)ch;
                if (j + 1 < len && (srcp[j + 1] & A_CHARTEXT) == DUMMY_CHAR_NEXT_TO_FULLWIDTH)
                {
                    buffer[j].Attributes |= COMMON_LVB_LEADING_BYTE;
                    buffer[++j].Char.UnicodeChar = (WCHAR)ch;
                    buffer[j].Attributes |= COMMON_LVB_TRAILING_BYTE;
                }
            }
        }
#else
        buffer[j].Char.UnicodeChar = (WCHAR)( ch & A_CHARTEXT);
#endif

    }

    bufPos.X = bufPos.Y = 0;
    bufSize.X = (SHORT)len;
    bufSize.Y = 1;

    sr.Top = sr.Bottom = (SHORT)lineno;
    sr.Left = (SHORT)x;
    sr.Right = (SHORT)( x + len - 1);

    WriteConsoleOutput(pdc_con_out, buffer, bufSize, bufPos, &sr);
}

static void _new_packet( attr_t attr, const int lineno,
                                 int x, int len, const chtype *srcp)
{
    int fore, back;
    bool blink, ansi;

    assert( len >= 0);
    assert( len < MAX_PACKET_LEN);
    if (pdc_ansi && (lineno == (SP->lines - 1)) && ((x + len) == SP->cols))
    {
        len--;
        if (len)
            _new_packet(attr, lineno, x, len, srcp);
        pdc_ansi = FALSE;
        _new_packet(attr, lineno, x + len, 1, srcp + len);
        pdc_ansi = TRUE;
        return;
    }

    extended_pair_content( (int)PAIR_NUMBER(attr), &fore, &back);
    ansi = pdc_ansi || (fore >= 16 || back >= 16);
    blink = (SP->termattrs & A_BLINK) && (attr & A_BLINK);

    if (blink)
    {
        attr &= ~A_BLINK;
        if (blinked_off)
            attr &= ~(A_UNDERLINE | A_RIGHT | A_LEFT);
    }

    if (attr & A_BOLD)
        fore |= 8;
    if (attr & A_BLINK)
        back |= 8;

    if (ansi)
        _show_run_of_ansi_characters( attr, fore, back, blink, lineno, x, srcp, len);
    else
        _show_run_of_nonansi_characters( attr, fore, back, blink, lineno, x, srcp, len);
}

/* update the given physical line to look like the corresponding line in
   curscr */

void PDC_transform_line(int lineno, int x, int len, const chtype *srcp)
{
    attr_t old_attr, attr;
    int i, j;

    PDC_LOG(("PDC_transform_line() - called: lineno=%d\n", lineno));

    old_attr = *srcp & (A_ATTRIBUTES | A_ALTCHARSET);

    for (i = 1, j = 1; j < len; i++, j++)
    {
        attr = srcp[i] & (A_ATTRIBUTES | A_ALTCHARSET);

        if (attr != old_attr)
        {
            _new_packet(old_attr, lineno, x, i, srcp);
            old_attr = attr;
            srcp += i;
            x += i;
            i = 0;
        }
    }

    _new_packet(old_attr, lineno, x, i, srcp);
}

void PDC_blink_text(void)
{
    CONSOLE_CURSOR_INFO cci;
    int i, j, k;
    bool oldvis;

    GetConsoleCursorInfo(pdc_con_out, &cci);
    oldvis = (bool)cci.bVisible;
    if (oldvis)
    {
        cci.bVisible = FALSE;
        SetConsoleCursorInfo(pdc_con_out, &cci);
    }

    if (!(SP->termattrs & A_BLINK))
        blinked_off = FALSE;
    else
        blinked_off = !blinked_off;

    for (i = 0; i < SP->lines; i++)
    {
        const chtype *srcp = curscr->_y[i];

        for (j = 0; j < SP->cols; j++)
            if (srcp[j] & A_BLINK)
            {
                k = j;
                while (k < SP->cols && (srcp[k] & A_BLINK))
                    k++;
                PDC_transform_line_sliced( i, j, k - j, srcp + j);
                j = k;
            }
    }

    PDC_gotoyx(SP->cursrow, SP->curscol);
    if (oldvis)
    {
        cci.bVisible = TRUE;
        SetConsoleCursorInfo(pdc_con_out, &cci);
    }

    pdc_last_blink = GetTickCount();
}

void PDC_doupdate(void)
{
}
