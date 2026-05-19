#ifndef THE_TEXTEDIT_H
#define THE_TEXTEDIT_H

#include "textpos.h"

/*
 * Logical UTF-8 editing helpers. These operate on byte buffers, but all
 * positions and replacement spans are derived from TextPos grapheme clusters.
 */

LENGTHTYPE textedit_append_utf8(CHARTYPE *line, LENGTHTYPE line_len,
                                LENGTHTYPE max_len,
                                const CHARTYPE *text, LENGTHTYPE text_len);
LENGTHTYPE textedit_insert_utf8(CHARTYPE *line, LENGTHTYPE line_len,
                                LENGTHTYPE max_len, LENGTHTYPE logical_col,
                                const CHARTYPE *text, LENGTHTYPE text_len);
LENGTHTYPE textedit_replace_utf8(CHARTYPE *line, LENGTHTYPE line_len,
                                 LENGTHTYPE max_len, LENGTHTYPE logical_col,
                                 const CHARTYPE *text, LENGTHTYPE text_len);
LENGTHTYPE textedit_overlay_utf8(CHARTYPE *line, LENGTHTYPE line_len,
                                 LENGTHTYPE max_len, LENGTHTYPE logical_col,
                                 const CHARTYPE *text, LENGTHTYPE text_len);

#endif
