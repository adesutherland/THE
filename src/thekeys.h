#ifndef THE_THEKEYS_H
#define THE_THEKEYS_H

/*
 * THE-owned logical key codes. The base System V curses values are retained
 * intentionally so existing key maps and terminal parsers keep their shape,
 * but core code no longer needs curses headers to obtain these constants.
 */

#define THE_KEY_NONE        (-1)
#define THE_KEY_MIN         0x101
#define THE_KEY_BREAK       0x101
#define THE_KEY_DOWN        0x102
#define THE_KEY_UP          0x103
#define THE_KEY_LEFT        0x104
#define THE_KEY_RIGHT       0x105
#define THE_KEY_HOME        0x106
#define THE_KEY_BACKSPACE   0x107
#define THE_KEY_F0          0x108
#define THE_KEY_F(n)        (THE_KEY_F0 + (n))
#define THE_KEY_DL          0x148
#define THE_KEY_IL          0x149
#define THE_KEY_DC          0x14a
#define THE_KEY_IC          0x14b
#define THE_KEY_EIC         0x14c
#define THE_KEY_CLEAR       0x14d
#define THE_KEY_EOS         0x14e
#define THE_KEY_EOL         0x14f
#define THE_KEY_SF          0x150
#define THE_KEY_SR          0x151
#define THE_KEY_NPAGE       0x152
#define THE_KEY_PPAGE       0x153
#define THE_KEY_STAB        0x154
#define THE_KEY_CTAB        0x155
#define THE_KEY_CATAB       0x156
#define THE_KEY_ENTER       0x157
#define THE_KEY_SRESET      0x158
#define THE_KEY_RESET       0x159
#define THE_KEY_PRINT       0x15a
#define THE_KEY_LL          0x15b
#define THE_KEY_ABORT       0x15c
#define THE_KEY_SHELP       0x15d
#define THE_KEY_LHELP       0x15e
#define THE_KEY_BTAB        0x15f
#define THE_KEY_BEG         0x160
#define THE_KEY_CANCEL      0x161
#define THE_KEY_CLOSE       0x162
#define THE_KEY_COMMAND     0x163
#define THE_KEY_COPY        0x164
#define THE_KEY_CREATE      0x165
#define THE_KEY_END         0x166
#define THE_KEY_EXIT        0x167
#define THE_KEY_FIND        0x168
#define THE_KEY_HELP        0x169
#define THE_KEY_MARK        0x16a
#define THE_KEY_MESSAGE     0x16b
#define THE_KEY_MOVE        0x16c
#define THE_KEY_NEXT        0x16d
#define THE_KEY_OPEN        0x16e
#define THE_KEY_OPTIONS     0x16f
#define THE_KEY_PREVIOUS    0x170
#define THE_KEY_REDO        0x171
#define THE_KEY_REFERENCE   0x172
#define THE_KEY_REFRESH     0x173
#define THE_KEY_REPLACE     0x174
#define THE_KEY_RESTART     0x175
#define THE_KEY_RESUME      0x176
#define THE_KEY_SAVE        0x177
#define THE_KEY_SBEG        0x178
#define THE_KEY_SCANCEL     0x179
#define THE_KEY_SCOMMAND    0x17a
#define THE_KEY_SCOPY       0x17b
#define THE_KEY_SCREATE     0x17c
#define THE_KEY_SDC         0x17d
#define THE_KEY_SDL         0x17e
#define THE_KEY_SELECT      0x17f
#define THE_KEY_SEND        0x180
#define THE_KEY_SEOL        0x181
#define THE_KEY_SEXIT       0x182
#define THE_KEY_SFIND       0x183
#define THE_KEY_SHOME       0x184
#define THE_KEY_SIC         0x185
#define THE_KEY_SLEFT       0x187
#define THE_KEY_SMESSAGE    0x188
#define THE_KEY_SMOVE       0x189
#define THE_KEY_SNEXT       0x18a
#define THE_KEY_SOPTIONS    0x18b
#define THE_KEY_SPREVIOUS   0x18c
#define THE_KEY_SPRINT      0x18d
#define THE_KEY_SREDO       0x18e
#define THE_KEY_SREPLACE    0x18f
#define THE_KEY_SRIGHT      0x190
#define THE_KEY_SRSUME      0x191
#define THE_KEY_SSAVE       0x192
#define THE_KEY_SSUSPEND    0x193
#define THE_KEY_SUNDO       0x194
#define THE_KEY_SUSPEND     0x195
#define THE_KEY_UNDO        0x196
#define THE_KEY_A1          0x197
#define THE_KEY_A2          0x198
#define THE_KEY_A3          0x199
#define THE_KEY_B1          0x19a
#define THE_KEY_B2          0x19b
#define THE_KEY_B3          0x19c
#define THE_KEY_C1          0x19d
#define THE_KEY_C2          0x19e
#define THE_KEY_C3          0x19f
#define THE_KEY_MAX         0xfff

#define THE_KEY_DELETE      0x7f
#define THE_KEY_RETURN      0x0d
#define THE_KEY_TAB         0x09
#define THE_KEY_BKSP        0x08
#define THE_KEY_ESC         0x1b
#define THE_KEY_CSI         0233

#define THE_KEY_CTRL_2      0x00
#define THE_KEY_CTRL_A      0x01
#define THE_KEY_CTRL_B      0x02
#define THE_KEY_CTRL_C      0x03
#define THE_KEY_CTRL_D      0x04
#define THE_KEY_CTRL_E      0x05
#define THE_KEY_CTRL_F      0x06
#define THE_KEY_CTRL_G      0x07
#define THE_KEY_CTRL_H      0x08
#define THE_KEY_CTRL_I      0x09
#define THE_KEY_CTRL_J      0x0a
#define THE_KEY_CTRL_K      0x0b
#define THE_KEY_CTRL_L      0x0c
#define THE_KEY_CTRL_M      0x0d
#define THE_KEY_CTRL_N      0x0e
#define THE_KEY_CTRL_O      0x0f
#define THE_KEY_CTRL_P      0x10
#define THE_KEY_CTRL_Q      0x11
#define THE_KEY_CTRL_R      0x12
#define THE_KEY_CTRL_S      0x13
#define THE_KEY_CTRL_T      0x14
#define THE_KEY_CTRL_U      0x15
#define THE_KEY_CTRL_V      0x16
#define THE_KEY_CTRL_W      0x17
#define THE_KEY_CTRL_X      0x18
#define THE_KEY_CTRL_Y      0x19
#define THE_KEY_CTRL_Z      0x1a
#define THE_KEY_CTRL_LBRACKET 0x1b
#define THE_KEY_CTRL_BSLASH   0x1c
#define THE_KEY_CTRL_RBRACKET 0x1d
#define THE_KEY_CTRL_6        0x1e
#define THE_KEY_CTRL_MINUS    0x1f

#define THE_KEY_CTL_LEFT    0x302
#define THE_KEY_CTL_RIGHT   0x303
#define THE_KEY_CTL_UP      0x304
#define THE_KEY_CTL_DOWN    0x305
#define THE_KEY_CTL_HOME    0x306
#define THE_KEY_CTL_END     0x307
#define THE_KEY_CTL_PGUP    0x308
#define THE_KEY_CTL_PGDN    0x309

/* PDCurses-compatible keypad, modifier, and resize ranges used by THE. */
#define THE_KEY_ALT_0       0x197
#define THE_KEY_ALT_1       0x198
#define THE_KEY_ALT_2       0x199
#define THE_KEY_ALT_3       0x19a
#define THE_KEY_ALT_4       0x19b
#define THE_KEY_ALT_5       0x19c
#define THE_KEY_ALT_6       0x19d
#define THE_KEY_ALT_7       0x19e
#define THE_KEY_ALT_8       0x19f
#define THE_KEY_ALT_9       0x1a0
#define THE_KEY_ALT_A       0x1a1
#define THE_KEY_ALT_B       0x1a2
#define THE_KEY_ALT_C       0x1a3
#define THE_KEY_ALT_D       0x1a4
#define THE_KEY_ALT_E       0x1a5
#define THE_KEY_ALT_F       0x1a6
#define THE_KEY_ALT_G       0x1a7
#define THE_KEY_ALT_H       0x1a8
#define THE_KEY_ALT_I       0x1a9
#define THE_KEY_ALT_J       0x1aa
#define THE_KEY_ALT_K       0x1ab
#define THE_KEY_ALT_L       0x1ac
#define THE_KEY_ALT_M       0x1ad
#define THE_KEY_ALT_N       0x1ae
#define THE_KEY_ALT_O       0x1af
#define THE_KEY_ALT_P       0x1b0
#define THE_KEY_ALT_Q       0x1b1
#define THE_KEY_ALT_R       0x1b2
#define THE_KEY_ALT_S       0x1b3
#define THE_KEY_ALT_T       0x1b4
#define THE_KEY_ALT_U       0x1b5
#define THE_KEY_ALT_V       0x1b6
#define THE_KEY_ALT_W       0x1b7
#define THE_KEY_ALT_X       0x1b8
#define THE_KEY_ALT_Y       0x1b9
#define THE_KEY_ALT_Z       0x1ba
#define THE_KEY_PDC_CTL_LEFT   0x1bb
#define THE_KEY_PDC_CTL_RIGHT  0x1bc
#define THE_KEY_PDC_CTL_PGUP   0x1bd
#define THE_KEY_PDC_CTL_PGDN   0x1be
#define THE_KEY_PDC_CTL_HOME   0x1bf
#define THE_KEY_PDC_CTL_END    0x1c0
#define THE_KEY_PADSLASH       0x1ca
#define THE_KEY_PADENTER       0x1cb
#define THE_KEY_CTL_PADENTER   0x1cc
#define THE_KEY_ALT_PADENTER   0x1cd
#define THE_KEY_PADSTOP        0x1ce
#define THE_KEY_PADSTAR        0x1cf
#define THE_KEY_PADMINUS       0x1d0
#define THE_KEY_PADPLUS        0x1d1
#define THE_KEY_CTL_PADSTOP    0x1d2
#define THE_KEY_CTL_PADCENTER  0x1d3
#define THE_KEY_CTL_PADPLUS    0x1d4
#define THE_KEY_CTL_PADMINUS   0x1d5
#define THE_KEY_CTL_PADSLASH   0x1d6
#define THE_KEY_CTL_PADSTAR    0x1d7
#define THE_KEY_ALT_PADPLUS    0x1d8
#define THE_KEY_ALT_PADMINUS   0x1d9
#define THE_KEY_ALT_PADSLASH   0x1da
#define THE_KEY_ALT_PADSTAR    0x1db
#define THE_KEY_ALT_PADSTOP    0x1dc
#define THE_KEY_CTL_INS        0x1dd
#define THE_KEY_ALT_DEL        0x1de
#define THE_KEY_ALT_INS        0x1df
#define THE_KEY_PDC_CTL_UP     0x1e0
#define THE_KEY_PDC_CTL_DOWN   0x1e1
#define THE_KEY_CTL_TAB        0x1e2
#define THE_KEY_ALT_TAB        0x1e3
#define THE_KEY_ALT_MINUS      0x1e4
#define THE_KEY_ALT_EQUAL      0x1e5
#define THE_KEY_ALT_HOME       0x1e6
#define THE_KEY_ALT_PGUP       0x1e7
#define THE_KEY_ALT_PGDN       0x1e8
#define THE_KEY_ALT_END        0x1e9
#define THE_KEY_ALT_UP         0x1ea
#define THE_KEY_ALT_DOWN       0x1eb
#define THE_KEY_ALT_RIGHT      0x1ec
#define THE_KEY_ALT_LEFT       0x1ed
#define THE_KEY_ALT_ENTER      0x1ee
#define THE_KEY_ALT_ESC        0x1ef
#define THE_KEY_ALT_BQUOTE     0x1f0
#define THE_KEY_ALT_LBRACKET   0x1f1
#define THE_KEY_ALT_RBRACKET   0x1f2
#define THE_KEY_ALT_SEMICOLON  0x1f3
#define THE_KEY_ALT_FQUOTE     0x1f4
#define THE_KEY_ALT_COMMA      0x1f5
#define THE_KEY_ALT_STOP       0x1f6
#define THE_KEY_ALT_FSLASH     0x1f7
#define THE_KEY_ALT_BKSP       0x1f8
#define THE_KEY_CTL_BKSP       0x1f9
#define THE_KEY_PAD0           0x1fa
#define THE_KEY_CTL_PAD0       0x1fb
#define THE_KEY_CTL_PAD1       0x1fc
#define THE_KEY_CTL_PAD2       0x1fd
#define THE_KEY_CTL_PAD3       0x1fe
#define THE_KEY_CTL_PAD4       0x1ff
#define THE_KEY_CTL_PAD5       0x200
#define THE_KEY_CTL_PAD6       0x201
#define THE_KEY_CTL_PAD7       0x202
#define THE_KEY_CTL_PAD8       0x203
#define THE_KEY_CTL_PAD9       0x204
#define THE_KEY_ALT_PAD0       0x205
#define THE_KEY_ALT_PAD1       0x206
#define THE_KEY_ALT_PAD2       0x207
#define THE_KEY_ALT_PAD3       0x208
#define THE_KEY_ALT_PAD4       0x209
#define THE_KEY_ALT_PAD5       0x20a
#define THE_KEY_ALT_PAD6       0x20b
#define THE_KEY_ALT_PAD7       0x20c
#define THE_KEY_ALT_PAD8       0x20d
#define THE_KEY_ALT_PAD9       0x20e
#define THE_KEY_CTL_DEL        0x20f
#define THE_KEY_ALT_BSLASH     0x210
#define THE_KEY_CTL_ENTER      0x211
#define THE_KEY_SHF_PADENTER   0x212
#define THE_KEY_SHF_PADSLASH   0x213
#define THE_KEY_SHF_PADSTAR    0x214
#define THE_KEY_SHF_PADPLUS    0x215
#define THE_KEY_SHF_PADMINUS   0x216
#define THE_KEY_SUP            0x217
#define THE_KEY_SDOWN          0x218
#define THE_KEY_SHF_IC         0x219
#define THE_KEY_SHF_DC         0x21a
#define THE_KEY_PHYSICAL_MOUSE 0x21b
#define THE_KEY_SHIFT_L        0x21c
#define THE_KEY_SHIFT_R        0x21d
#define THE_KEY_CONTROL_L      0x21e
#define THE_KEY_CONTROL_R      0x21f
#define THE_KEY_ALT_L_KEY      0x220
#define THE_KEY_ALT_R_KEY      0x221
#define THE_KEY_RESIZE         0x222
#define THE_KEY_APPS           0x225

#define THE_KEY_NEWL           0x0a
#define THE_KEY_SPACE          0x20
#define THE_KEY_BQUOTE         0x60
#define THE_KEY_MINUS          0x2d
#define THE_KEY_EQUAL          0x3d
#define THE_KEY_LBRACKET       0x5b
#define THE_KEY_RBRACKET       0x5d
#define THE_KEY_BSLASH         0x5c
#define THE_KEY_SEMICOLON      0x3b
#define THE_KEY_FQUOTE         0x27
#define THE_KEY_COMMA          0x2c
#define THE_KEY_STOP           0x2e
#define THE_KEY_FSLASH         0x2f

#define THE_KEY_PF1            0x350
#define THE_KEY_PF2            0x351
#define THE_KEY_PF3            0x352
#define THE_KEY_PF4            0x353
#define THE_KEY_VT_PADCOMMA    0x354
#define THE_KEY_VT_PADMINUS    0x355
#define THE_KEY_VT_PADPERIOD   0x356
#define THE_KEY_VT_PADPLUS     0x357
#define THE_KEY_VT_PADSTAR     0x358
#define THE_KEY_VT_PADSLASH    0x359

#define THE_KEY_PB1            0x400
#define THE_KEY_PB2            0x401
#define THE_KEY_PB3            0x402
#define THE_KEY_S_PB1          0x403
#define THE_KEY_S_PB2          0x404
#define THE_KEY_S_PB3          0x405
#define THE_KEY_C_PB1          0x406
#define THE_KEY_C_PB2          0x407
#define THE_KEY_C_PB3          0x408
#define THE_KEY_A_PB1          0x409
#define THE_KEY_A_PB2          0x40a
#define THE_KEY_A_PB3          0x40b
#define THE_KEY_RB1            0x410
#define THE_KEY_RB2            0x411
#define THE_KEY_RB3            0x412
#define THE_KEY_S_RB1          0x413
#define THE_KEY_S_RB2          0x414
#define THE_KEY_S_RB3          0x415
#define THE_KEY_C_RB1          0x416
#define THE_KEY_C_RB2          0x417
#define THE_KEY_C_RB3          0x418
#define THE_KEY_A_RB1          0x419
#define THE_KEY_A_RB2          0x41a
#define THE_KEY_A_RB3          0x41b
#define THE_KEY_DB1            0x420
#define THE_KEY_DB2            0x421
#define THE_KEY_DB3            0x422
#define THE_KEY_S_DB1          0x423
#define THE_KEY_S_DB2          0x424
#define THE_KEY_S_DB3          0x425
#define THE_KEY_C_DB1          0x426
#define THE_KEY_C_DB2          0x427
#define THE_KEY_C_DB3          0x428
#define THE_KEY_A_DB1          0x429
#define THE_KEY_A_DB2          0x42a
#define THE_KEY_A_DB3          0x42b

#if defined(WIN32) \
 || defined(USE_XCURSES) || defined(USE_SDLCURSES) \
 || defined(USE_VTCURSES)
# define THE_KEY_NUMENTER THE_KEY_PADENTER
#else
# define THE_KEY_NUMENTER THE_KEY_ENTER
#endif

#define THE_KEY_PARSE_COMPLETE 0x1000
#define THE_KEY_MOUSE          0x1001

#ifndef KEY_MIN
# define KEY_MIN THE_KEY_MIN
#endif
#ifndef KEY_BREAK
# define KEY_BREAK THE_KEY_BREAK
#endif
#ifndef KEY_DOWN
# define KEY_DOWN THE_KEY_DOWN
#endif
#ifndef KEY_UP
# define KEY_UP THE_KEY_UP
#endif
#ifndef KEY_LEFT
# define KEY_LEFT THE_KEY_LEFT
#endif
#ifndef KEY_RIGHT
# define KEY_RIGHT THE_KEY_RIGHT
#endif
#ifndef KEY_HOME
# define KEY_HOME THE_KEY_HOME
#endif
#ifndef KEY_BACKSPACE
# define KEY_BACKSPACE THE_KEY_BACKSPACE
#endif
#ifndef KEY_F0
# define KEY_F0 THE_KEY_F0
#endif
#ifndef KEY_F
# define KEY_F(n) (KEY_F0 + (n))
#endif
#ifndef KEY_DL
# define KEY_DL THE_KEY_DL
#endif
#ifndef KEY_IL
# define KEY_IL THE_KEY_IL
#endif
#ifndef KEY_DC
# define KEY_DC THE_KEY_DC
#endif
#ifndef KEY_IC
# define KEY_IC THE_KEY_IC
#endif
#ifndef KEY_EIC
# define KEY_EIC THE_KEY_EIC
#endif
#ifndef KEY_CLEAR
# define KEY_CLEAR THE_KEY_CLEAR
#endif
#ifndef KEY_EOS
# define KEY_EOS THE_KEY_EOS
#endif
#ifndef KEY_EOL
# define KEY_EOL THE_KEY_EOL
#endif
#ifndef KEY_SF
# define KEY_SF THE_KEY_SF
#endif
#ifndef KEY_SR
# define KEY_SR THE_KEY_SR
#endif
#ifndef KEY_NPAGE
# define KEY_NPAGE THE_KEY_NPAGE
#endif
#ifndef KEY_PPAGE
# define KEY_PPAGE THE_KEY_PPAGE
#endif
#ifndef KEY_STAB
# define KEY_STAB THE_KEY_STAB
#endif
#ifndef KEY_CTAB
# define KEY_CTAB THE_KEY_CTAB
#endif
#ifndef KEY_CATAB
# define KEY_CATAB THE_KEY_CATAB
#endif
#ifndef KEY_ENTER
# define KEY_ENTER THE_KEY_ENTER
#endif
#ifndef KEY_SRESET
# define KEY_SRESET THE_KEY_SRESET
#endif
#ifndef KEY_RESET
# define KEY_RESET THE_KEY_RESET
#endif
#ifndef KEY_PRINT
# define KEY_PRINT THE_KEY_PRINT
#endif
#ifndef KEY_LL
# define KEY_LL THE_KEY_LL
#endif
#ifndef KEY_ABORT
# define KEY_ABORT THE_KEY_ABORT
#endif
#ifndef KEY_SHELP
# define KEY_SHELP THE_KEY_SHELP
#endif
#ifndef KEY_LHELP
# define KEY_LHELP THE_KEY_LHELP
#endif
#ifndef KEY_BTAB
# define KEY_BTAB THE_KEY_BTAB
#endif
#ifndef KEY_BEG
# define KEY_BEG THE_KEY_BEG
#endif
#ifndef KEY_CANCEL
# define KEY_CANCEL THE_KEY_CANCEL
#endif
#ifndef KEY_CLOSE
# define KEY_CLOSE THE_KEY_CLOSE
#endif
#ifndef KEY_COMMAND
# define KEY_COMMAND THE_KEY_COMMAND
#endif
#ifndef KEY_COPY
# define KEY_COPY THE_KEY_COPY
#endif
#ifndef KEY_CREATE
# define KEY_CREATE THE_KEY_CREATE
#endif
#ifndef KEY_END
# define KEY_END THE_KEY_END
#endif
#ifndef KEY_EXIT
# define KEY_EXIT THE_KEY_EXIT
#endif
#ifndef KEY_FIND
# define KEY_FIND THE_KEY_FIND
#endif
#ifndef KEY_HELP
# define KEY_HELP THE_KEY_HELP
#endif
#ifndef KEY_MARK
# define KEY_MARK THE_KEY_MARK
#endif
#ifndef KEY_MESSAGE
# define KEY_MESSAGE THE_KEY_MESSAGE
#endif
#ifndef KEY_MOVE
# define KEY_MOVE THE_KEY_MOVE
#endif
#ifndef KEY_NEXT
# define KEY_NEXT THE_KEY_NEXT
#endif
#ifndef KEY_OPEN
# define KEY_OPEN THE_KEY_OPEN
#endif
#ifndef KEY_OPTIONS
# define KEY_OPTIONS THE_KEY_OPTIONS
#endif
#ifndef KEY_PREVIOUS
# define KEY_PREVIOUS THE_KEY_PREVIOUS
#endif
#ifndef KEY_REDO
# define KEY_REDO THE_KEY_REDO
#endif
#ifndef KEY_REFERENCE
# define KEY_REFERENCE THE_KEY_REFERENCE
#endif
#ifndef KEY_REFRESH
# define KEY_REFRESH THE_KEY_REFRESH
#endif
#ifndef KEY_REPLACE
# define KEY_REPLACE THE_KEY_REPLACE
#endif
#ifndef KEY_RESTART
# define KEY_RESTART THE_KEY_RESTART
#endif
#ifndef KEY_RESUME
# define KEY_RESUME THE_KEY_RESUME
#endif
#ifndef KEY_SAVE
# define KEY_SAVE THE_KEY_SAVE
#endif
#ifndef KEY_SBEG
# define KEY_SBEG THE_KEY_SBEG
#endif
#ifndef KEY_SCANCEL
# define KEY_SCANCEL THE_KEY_SCANCEL
#endif
#ifndef KEY_SCOMMAND
# define KEY_SCOMMAND THE_KEY_SCOMMAND
#endif
#ifndef KEY_SCOPY
# define KEY_SCOPY THE_KEY_SCOPY
#endif
#ifndef KEY_SCREATE
# define KEY_SCREATE THE_KEY_SCREATE
#endif
#ifndef KEY_SDC
# define KEY_SDC THE_KEY_SDC
#endif
#ifndef KEY_SDL
# define KEY_SDL THE_KEY_SDL
#endif
#ifndef KEY_SELECT
# define KEY_SELECT THE_KEY_SELECT
#endif
#ifndef KEY_SEND
# define KEY_SEND THE_KEY_SEND
#endif
#ifndef KEY_SEOL
# define KEY_SEOL THE_KEY_SEOL
#endif
#ifndef KEY_SEXIT
# define KEY_SEXIT THE_KEY_SEXIT
#endif
#ifndef KEY_SFIND
# define KEY_SFIND THE_KEY_SFIND
#endif
#ifndef KEY_SHOME
# define KEY_SHOME THE_KEY_SHOME
#endif
#ifndef KEY_SIC
# define KEY_SIC THE_KEY_SIC
#endif
#ifndef KEY_SLEFT
# define KEY_SLEFT THE_KEY_SLEFT
#endif
#ifndef KEY_SMESSAGE
# define KEY_SMESSAGE THE_KEY_SMESSAGE
#endif
#ifndef KEY_SMOVE
# define KEY_SMOVE THE_KEY_SMOVE
#endif
#ifndef KEY_SNEXT
# define KEY_SNEXT THE_KEY_SNEXT
#endif
#ifndef KEY_SOPTIONS
# define KEY_SOPTIONS THE_KEY_SOPTIONS
#endif
#ifndef KEY_SPREVIOUS
# define KEY_SPREVIOUS THE_KEY_SPREVIOUS
#endif
#ifndef KEY_SPRINT
# define KEY_SPRINT THE_KEY_SPRINT
#endif
#ifndef KEY_SREDO
# define KEY_SREDO THE_KEY_SREDO
#endif
#ifndef KEY_SREPLACE
# define KEY_SREPLACE THE_KEY_SREPLACE
#endif
#ifndef KEY_SRIGHT
# define KEY_SRIGHT THE_KEY_SRIGHT
#endif
#ifndef KEY_SRSUME
# define KEY_SRSUME THE_KEY_SRSUME
#endif
#ifndef KEY_SSAVE
# define KEY_SSAVE THE_KEY_SSAVE
#endif
#ifndef KEY_SSUSPEND
# define KEY_SSUSPEND THE_KEY_SSUSPEND
#endif
#ifndef KEY_SUNDO
# define KEY_SUNDO THE_KEY_SUNDO
#endif
#ifndef KEY_SUSPEND
# define KEY_SUSPEND THE_KEY_SUSPEND
#endif
#ifndef KEY_UNDO
# define KEY_UNDO THE_KEY_UNDO
#endif
#ifndef KEY_A1
# define KEY_A1 THE_KEY_A1
#endif
#ifndef KEY_A2
# define KEY_A2 THE_KEY_A2
#endif
#ifndef KEY_A3
# define KEY_A3 THE_KEY_A3
#endif
#ifndef KEY_B1
# define KEY_B1 THE_KEY_B1
#endif
#ifndef KEY_B2
# define KEY_B2 THE_KEY_B2
#endif
#ifndef KEY_B3
# define KEY_B3 THE_KEY_B3
#endif
#ifndef KEY_C1
# define KEY_C1 THE_KEY_C1
#endif
#ifndef KEY_C2
# define KEY_C2 THE_KEY_C2
#endif
#ifndef KEY_C3
# define KEY_C3 THE_KEY_C3
#endif
#ifndef KEY_MAX
# define KEY_MAX THE_KEY_MAX
#endif

#ifndef KEY_DELETE
# define KEY_DELETE THE_KEY_DELETE
#endif
#ifndef KEY_RETURN
# define KEY_RETURN THE_KEY_RETURN
#endif
#ifndef KEY_TAB
# define KEY_TAB THE_KEY_TAB
#endif
#ifndef KEY_BKSP
# define KEY_BKSP THE_KEY_BKSP
#endif
#ifndef KEY_ESC
# define KEY_ESC THE_KEY_ESC
#endif
#ifndef KEY_NEWL
# define KEY_NEWL THE_KEY_NEWL
#endif
#ifndef KEY_SPACE
# define KEY_SPACE THE_KEY_SPACE
#endif
#ifndef KEY_BQUOTE
# define KEY_BQUOTE THE_KEY_BQUOTE
#endif
#ifndef KEY_MINUS
# define KEY_MINUS THE_KEY_MINUS
#endif
#ifndef KEY_EQUAL
# define KEY_EQUAL THE_KEY_EQUAL
#endif
#ifndef KEY_LBRACKET
# define KEY_LBRACKET THE_KEY_LBRACKET
#endif
#ifndef KEY_RBRACKET
# define KEY_RBRACKET THE_KEY_RBRACKET
#endif
#ifndef KEY_BSLASH
# define KEY_BSLASH THE_KEY_BSLASH
#endif
#ifndef KEY_SEMICOLON
# define KEY_SEMICOLON THE_KEY_SEMICOLON
#endif
#ifndef KEY_FQUOTE
# define KEY_FQUOTE THE_KEY_FQUOTE
#endif
#ifndef KEY_COMMA
# define KEY_COMMA THE_KEY_COMMA
#endif
#ifndef KEY_STOP
# define KEY_STOP THE_KEY_STOP
#endif
#ifndef KEY_FSLASH
# define KEY_FSLASH THE_KEY_FSLASH
#endif
#ifndef CSI
# define CSI THE_KEY_CSI
#endif

#ifndef KEY_a
# define KEY_a 'a'
# define KEY_b 'b'
# define KEY_c 'c'
# define KEY_d 'd'
# define KEY_e 'e'
# define KEY_f 'f'
# define KEY_g 'g'
# define KEY_h 'h'
# define KEY_i 'i'
# define KEY_j 'j'
# define KEY_k 'k'
# define KEY_l 'l'
# define KEY_m 'm'
# define KEY_n 'n'
# define KEY_o 'o'
# define KEY_p 'p'
# define KEY_q 'q'
# define KEY_r 'r'
# define KEY_s 's'
# define KEY_t 't'
# define KEY_u 'u'
# define KEY_v 'v'
# define KEY_w 'w'
# define KEY_x 'x'
# define KEY_y 'y'
# define KEY_z 'z'
# define KEY_0 '0'
# define KEY_1 '1'
# define KEY_2 '2'
# define KEY_3 '3'
# define KEY_4 '4'
# define KEY_5 '5'
# define KEY_6 '6'
# define KEY_7 '7'
# define KEY_8 '8'
# define KEY_9 '9'
# define KEY_S_a 'A'
# define KEY_S_b 'B'
# define KEY_S_c 'C'
# define KEY_S_d 'D'
# define KEY_S_e 'E'
# define KEY_S_f 'F'
# define KEY_S_g 'G'
# define KEY_S_h 'H'
# define KEY_S_i 'I'
# define KEY_S_j 'J'
# define KEY_S_k 'K'
# define KEY_S_l 'L'
# define KEY_S_m 'M'
# define KEY_S_n 'N'
# define KEY_S_o 'O'
# define KEY_S_p 'P'
# define KEY_S_q 'Q'
# define KEY_S_r 'R'
# define KEY_S_s 'S'
# define KEY_S_t 'T'
# define KEY_S_u 'U'
# define KEY_S_v 'V'
# define KEY_S_w 'W'
# define KEY_S_x 'X'
# define KEY_S_y 'Y'
# define KEY_S_z 'Z'
# define KEY_S_0 ')'
# define KEY_S_1 '!'
# define KEY_S_2 '@'
# define KEY_S_3 '#'
# define KEY_S_4 '$'
# define KEY_S_5 '%'
# define KEY_S_6 '^'
# define KEY_S_7 '&'
# define KEY_S_8 '*'
# define KEY_S_9 '('
# define KEY_S_BQUOTE '~'
# define KEY_S_MINUS '_'
# define KEY_S_EQUAL '+'
# define KEY_S_LBRACKET '{'
# define KEY_S_RBRACKET '}'
# define KEY_S_BSLASH '|'
# define KEY_S_SEMICOLON ':'
# define KEY_S_FQUOTE '"'
# define KEY_S_COMMA '<'
# define KEY_S_STOP '>'
# define KEY_S_FSLASH '?'
#endif

#ifndef KEY_C_a
# define KEY_C_a THE_KEY_CTRL_A
# define KEY_C_b THE_KEY_CTRL_B
# define KEY_C_c THE_KEY_CTRL_C
# define KEY_C_d THE_KEY_CTRL_D
# define KEY_C_e THE_KEY_CTRL_E
# define KEY_C_f THE_KEY_CTRL_F
# define KEY_C_g THE_KEY_CTRL_G
# define KEY_C_h THE_KEY_CTRL_H
# define KEY_C_i THE_KEY_CTRL_I
# define KEY_C_j THE_KEY_CTRL_J
# define KEY_C_k THE_KEY_CTRL_K
# define KEY_C_l THE_KEY_CTRL_L
# define KEY_C_m THE_KEY_CTRL_M
# define KEY_C_n THE_KEY_CTRL_N
# define KEY_C_o THE_KEY_CTRL_O
# define KEY_C_p THE_KEY_CTRL_P
# define KEY_C_q THE_KEY_CTRL_Q
# define KEY_C_r THE_KEY_CTRL_R
# define KEY_C_s THE_KEY_CTRL_S
# define KEY_C_t THE_KEY_CTRL_T
# define KEY_C_u THE_KEY_CTRL_U
# define KEY_C_v THE_KEY_CTRL_V
# define KEY_C_w THE_KEY_CTRL_W
# define KEY_C_x THE_KEY_CTRL_X
# define KEY_C_y THE_KEY_CTRL_Y
# define KEY_C_z THE_KEY_CTRL_Z
# define KEY_C_2 THE_KEY_CTRL_2
# define KEY_C_6 THE_KEY_CTRL_6
# define KEY_C_MINUS THE_KEY_CTRL_MINUS
# define KEY_C_LBRACKET THE_KEY_CTRL_LBRACKET
# define KEY_C_RBRACKET THE_KEY_CTRL_RBRACKET
# define KEY_C_BSLASH THE_KEY_CTRL_BSLASH
#endif

#ifndef CTL_LEFT
# define CTL_LEFT THE_KEY_CTL_LEFT
#endif
#ifndef CTL_RIGHT
# define CTL_RIGHT THE_KEY_CTL_RIGHT
#endif
#ifndef CTL_UP
# define CTL_UP THE_KEY_CTL_UP
#endif
#ifndef CTL_DOWN
# define CTL_DOWN THE_KEY_CTL_DOWN
#endif
#ifndef CTL_HOME
# define CTL_HOME THE_KEY_CTL_HOME
#endif
#ifndef CTL_END
# define CTL_END THE_KEY_CTL_END
#endif
#ifndef CTL_PGUP
# define CTL_PGUP THE_KEY_CTL_PGUP
#endif
#ifndef CTL_PGDN
# define CTL_PGDN THE_KEY_CTL_PGDN
#endif
#ifndef CTL_INS
# define CTL_INS THE_KEY_CTL_INS
#endif
#ifndef CTL_DEL
# define CTL_DEL THE_KEY_CTL_DEL
#endif
#ifndef ALT_HOME
# define ALT_HOME THE_KEY_ALT_HOME
#endif
#ifndef ALT_PGUP
# define ALT_PGUP THE_KEY_ALT_PGUP
#endif
#ifndef ALT_PGDN
# define ALT_PGDN THE_KEY_ALT_PGDN
#endif
#ifndef ALT_END
# define ALT_END THE_KEY_ALT_END
#endif
#ifndef ALT_UP
# define ALT_UP THE_KEY_ALT_UP
#endif
#ifndef ALT_DOWN
# define ALT_DOWN THE_KEY_ALT_DOWN
#endif
#ifndef ALT_RIGHT
# define ALT_RIGHT THE_KEY_ALT_RIGHT
#endif
#ifndef ALT_LEFT
# define ALT_LEFT THE_KEY_ALT_LEFT
#endif
#ifndef ALT_INS
# define ALT_INS THE_KEY_ALT_INS
#endif
#ifndef ALT_DEL
# define ALT_DEL THE_KEY_ALT_DEL
#endif
#ifndef ALT_0
# define ALT_0 THE_KEY_ALT_0
# define ALT_1 THE_KEY_ALT_1
# define ALT_2 THE_KEY_ALT_2
# define ALT_3 THE_KEY_ALT_3
# define ALT_4 THE_KEY_ALT_4
# define ALT_5 THE_KEY_ALT_5
# define ALT_6 THE_KEY_ALT_6
# define ALT_7 THE_KEY_ALT_7
# define ALT_8 THE_KEY_ALT_8
# define ALT_9 THE_KEY_ALT_9
# define ALT_A THE_KEY_ALT_A
# define ALT_B THE_KEY_ALT_B
# define ALT_C THE_KEY_ALT_C
# define ALT_D THE_KEY_ALT_D
# define ALT_E THE_KEY_ALT_E
# define ALT_F THE_KEY_ALT_F
# define ALT_G THE_KEY_ALT_G
# define ALT_H THE_KEY_ALT_H
# define ALT_I THE_KEY_ALT_I
# define ALT_J THE_KEY_ALT_J
# define ALT_K THE_KEY_ALT_K
# define ALT_L THE_KEY_ALT_L
# define ALT_M THE_KEY_ALT_M
# define ALT_N THE_KEY_ALT_N
# define ALT_O THE_KEY_ALT_O
# define ALT_P THE_KEY_ALT_P
# define ALT_Q THE_KEY_ALT_Q
# define ALT_R THE_KEY_ALT_R
# define ALT_S THE_KEY_ALT_S
# define ALT_T THE_KEY_ALT_T
# define ALT_U THE_KEY_ALT_U
# define ALT_V THE_KEY_ALT_V
# define ALT_W THE_KEY_ALT_W
# define ALT_X THE_KEY_ALT_X
# define ALT_Y THE_KEY_ALT_Y
# define ALT_Z THE_KEY_ALT_Z
# define ALT_TAB THE_KEY_ALT_TAB
# define ALT_MINUS THE_KEY_ALT_MINUS
# define ALT_EQUAL THE_KEY_ALT_EQUAL
# define ALT_ENTER THE_KEY_ALT_ENTER
# define ALT_ESC THE_KEY_ALT_ESC
# define ALT_BQUOTE THE_KEY_ALT_BQUOTE
# define ALT_LBRACKET THE_KEY_ALT_LBRACKET
# define ALT_RBRACKET THE_KEY_ALT_RBRACKET
# define ALT_SEMICOLON THE_KEY_ALT_SEMICOLON
# define ALT_FQUOTE THE_KEY_ALT_FQUOTE
# define ALT_COMMA THE_KEY_ALT_COMMA
# define ALT_STOP THE_KEY_ALT_STOP
# define ALT_FSLASH THE_KEY_ALT_FSLASH
# define ALT_BKSP THE_KEY_ALT_BKSP
# define ALT_BSLASH THE_KEY_ALT_BSLASH
#endif

#ifndef PADSLASH
# define PADSLASH THE_KEY_PADSLASH
#endif
#ifndef PADENTER
# define PADENTER THE_KEY_PADENTER
#endif
#ifndef PADSTOP
# define PADSTOP THE_KEY_PADSTOP
#endif
#ifndef PADSTAR
# define PADSTAR THE_KEY_PADSTAR
#endif
#ifndef PADMINUS
# define PADMINUS THE_KEY_PADMINUS
#endif
#ifndef PADPLUS
# define PADPLUS THE_KEY_PADPLUS
#endif
#ifndef PAD0
# define PAD0 THE_KEY_PAD0
#endif
#ifndef CTL_PADENTER
# define CTL_PADENTER THE_KEY_CTL_PADENTER
# define ALT_PADENTER THE_KEY_ALT_PADENTER
# define CTL_PADSTOP THE_KEY_CTL_PADSTOP
# define CTL_PADCENTER THE_KEY_CTL_PADCENTER
# define CTL_PADPLUS THE_KEY_CTL_PADPLUS
# define CTL_PADMINUS THE_KEY_CTL_PADMINUS
# define CTL_PADSLASH THE_KEY_CTL_PADSLASH
# define CTL_PADSTAR THE_KEY_CTL_PADSTAR
# define ALT_PADPLUS THE_KEY_ALT_PADPLUS
# define ALT_PADMINUS THE_KEY_ALT_PADMINUS
# define ALT_PADSLASH THE_KEY_ALT_PADSLASH
# define ALT_PADSTAR THE_KEY_ALT_PADSTAR
# define ALT_PADSTOP THE_KEY_ALT_PADSTOP
# define CTL_PAD0 THE_KEY_CTL_PAD0
# define CTL_PAD1 THE_KEY_CTL_PAD1
# define CTL_PAD2 THE_KEY_CTL_PAD2
# define CTL_PAD3 THE_KEY_CTL_PAD3
# define CTL_PAD4 THE_KEY_CTL_PAD4
# define CTL_PAD5 THE_KEY_CTL_PAD5
# define CTL_PAD6 THE_KEY_CTL_PAD6
# define CTL_PAD7 THE_KEY_CTL_PAD7
# define CTL_PAD8 THE_KEY_CTL_PAD8
# define CTL_PAD9 THE_KEY_CTL_PAD9
# define ALT_PAD0 THE_KEY_ALT_PAD0
# define ALT_PAD1 THE_KEY_ALT_PAD1
# define ALT_PAD2 THE_KEY_ALT_PAD2
# define ALT_PAD3 THE_KEY_ALT_PAD3
# define ALT_PAD4 THE_KEY_ALT_PAD4
# define ALT_PAD5 THE_KEY_ALT_PAD5
# define ALT_PAD6 THE_KEY_ALT_PAD6
# define ALT_PAD7 THE_KEY_ALT_PAD7
# define ALT_PAD8 THE_KEY_ALT_PAD8
# define ALT_PAD9 THE_KEY_ALT_PAD9
# define CTL_TAB THE_KEY_CTL_TAB
# define CTL_BKSP THE_KEY_CTL_BKSP
# define CTL_ENTER THE_KEY_CTL_ENTER
# define SHF_PADENTER THE_KEY_SHF_PADENTER
# define SHF_PADSLASH THE_KEY_SHF_PADSLASH
# define SHF_PADSTAR THE_KEY_SHF_PADSTAR
# define SHF_PADPLUS THE_KEY_SHF_PADPLUS
# define SHF_PADMINUS THE_KEY_SHF_PADMINUS
#endif
#ifndef KEY_NUMENTER
# define KEY_NUMENTER THE_KEY_NUMENTER
#endif
#ifndef KEY_SUP
# define KEY_SUP THE_KEY_SUP
#endif
#ifndef KEY_SDOWN
# define KEY_SDOWN THE_KEY_SDOWN
#endif
#ifndef KEY_RESIZE
# define KEY_RESIZE THE_KEY_RESIZE
#endif
#ifndef KEY_SHIFT_L
# define KEY_SHIFT_L THE_KEY_SHIFT_L
# define KEY_SHIFT_R THE_KEY_SHIFT_R
# define KEY_CONTROL_L THE_KEY_CONTROL_L
# define KEY_CONTROL_R THE_KEY_CONTROL_R
# define KEY_ALT_L THE_KEY_ALT_L_KEY
# define KEY_ALT_R THE_KEY_ALT_R_KEY
#endif

#ifndef KEY_PGUP
# define KEY_PGUP KEY_PPAGE
#endif
#ifndef KEY_PGDN
# define KEY_PGDN KEY_NPAGE
#endif
#ifndef KEY_CURU
# define KEY_CURU KEY_UP
#endif
#ifndef KEY_CURD
# define KEY_CURD KEY_DOWN
#endif
#ifndef KEY_CURL
# define KEY_CURL KEY_LEFT
#endif
#ifndef KEY_CURR
# define KEY_CURR KEY_RIGHT
#endif
#ifndef KEY_INS
# define KEY_INS KEY_IC
#endif
#ifndef KEY_DEL
# define KEY_DEL KEY_DC
#endif
#ifndef KEY_S_TAB
# define KEY_S_TAB KEY_BTAB
#endif
#ifndef KEY_S_CURU
# define KEY_S_CURU KEY_SUP
#endif
#ifndef KEY_S_CURD
# define KEY_S_CURD KEY_SDOWN
#endif
#ifndef KEY_S_CURL
# define KEY_S_CURL KEY_SLEFT
#endif
#ifndef KEY_S_CURR
# define KEY_S_CURR KEY_SRIGHT
#endif
#ifndef KEY_S_INS
# define KEY_S_INS KEY_SIC
#endif
#ifndef KEY_S_DEL
# define KEY_S_DEL KEY_SDC
#endif
#ifndef KEY_S_NUMCENTER
# define KEY_S_NUMCENTER '5'
#endif

#ifndef KEY_F1
# define KEY_F1 KEY_F(1)
#endif
#ifndef KEY_F2
# define KEY_F2 KEY_F(2)
#endif
#ifndef KEY_F3
# define KEY_F3 KEY_F(3)
#endif
#ifndef KEY_F4
# define KEY_F4 KEY_F(4)
#endif
#ifndef KEY_F5
# define KEY_F5 KEY_F(5)
#endif
#ifndef KEY_F6
# define KEY_F6 KEY_F(6)
#endif
#ifndef KEY_F7
# define KEY_F7 KEY_F(7)
#endif
#ifndef KEY_F8
# define KEY_F8 KEY_F(8)
#endif
#ifndef KEY_F9
# define KEY_F9 KEY_F(9)
#endif
#ifndef KEY_F10
# define KEY_F10 KEY_F(10)
#endif
#ifndef KEY_F11
# define KEY_F11 KEY_F(11)
#endif
#ifndef KEY_F12
# define KEY_F12 KEY_F(12)
#endif
#ifndef KEY_S_F1
# define KEY_S_F1 KEY_F(13)
#endif
#ifndef KEY_S_F2
# define KEY_S_F2 KEY_F(14)
#endif
#ifndef KEY_S_F3
# define KEY_S_F3 KEY_F(15)
#endif
#ifndef KEY_S_F4
# define KEY_S_F4 KEY_F(16)
#endif
#ifndef KEY_S_F5
# define KEY_S_F5 KEY_F(17)
#endif
#ifndef KEY_S_F6
# define KEY_S_F6 KEY_F(18)
#endif
#ifndef KEY_S_F7
# define KEY_S_F7 KEY_F(19)
#endif
#ifndef KEY_S_F8
# define KEY_S_F8 KEY_F(20)
#endif
#ifndef KEY_S_F9
# define KEY_S_F9 KEY_F(21)
#endif
#ifndef KEY_S_F10
# define KEY_S_F10 KEY_F(22)
#endif
#ifndef KEY_S_F11
# define KEY_S_F11 KEY_F(23)
#endif
#ifndef KEY_S_F12
# define KEY_S_F12 KEY_F(24)
#endif
#ifndef KEY_C_F1
# define KEY_C_F1 KEY_F(25)
#endif
#ifndef KEY_C_F2
# define KEY_C_F2 KEY_F(26)
#endif
#ifndef KEY_C_F3
# define KEY_C_F3 KEY_F(27)
#endif
#ifndef KEY_C_F4
# define KEY_C_F4 KEY_F(28)
#endif
#ifndef KEY_C_F5
# define KEY_C_F5 KEY_F(29)
#endif
#ifndef KEY_C_F6
# define KEY_C_F6 KEY_F(30)
#endif
#ifndef KEY_C_F7
# define KEY_C_F7 KEY_F(31)
#endif
#ifndef KEY_C_F8
# define KEY_C_F8 KEY_F(32)
#endif
#ifndef KEY_C_F9
# define KEY_C_F9 KEY_F(33)
#endif
#ifndef KEY_C_F10
# define KEY_C_F10 KEY_F(34)
#endif
#ifndef KEY_C_F11
# define KEY_C_F11 KEY_F(35)
#endif
#ifndef KEY_C_F12
# define KEY_C_F12 KEY_F(36)
#endif
#ifndef KEY_A_F1
# define KEY_A_F1 KEY_F(37)
#endif
#ifndef KEY_A_F2
# define KEY_A_F2 KEY_F(38)
#endif
#ifndef KEY_A_F3
# define KEY_A_F3 KEY_F(39)
#endif
#ifndef KEY_A_F4
# define KEY_A_F4 KEY_F(40)
#endif
#ifndef KEY_A_F5
# define KEY_A_F5 KEY_F(41)
#endif
#ifndef KEY_A_F6
# define KEY_A_F6 KEY_F(42)
#endif
#ifndef KEY_A_F7
# define KEY_A_F7 KEY_F(43)
#endif
#ifndef KEY_A_F8
# define KEY_A_F8 KEY_F(44)
#endif
#ifndef KEY_A_F9
# define KEY_A_F9 KEY_F(45)
#endif
#ifndef KEY_A_F10
# define KEY_A_F10 KEY_F(46)
#endif
#ifndef KEY_A_F11
# define KEY_A_F11 KEY_F(47)
#endif
#ifndef KEY_A_F12
# define KEY_A_F12 KEY_F(48)
#endif
#ifndef KEY_F13
# define KEY_F13 KEY_F(49)
#endif
#ifndef KEY_F14
# define KEY_F14 KEY_F(50)
#endif
#ifndef KEY_F15
# define KEY_F15 KEY_F(51)
#endif
#ifndef KEY_F16
# define KEY_F16 KEY_F(52)
#endif
#ifndef KEY_F17
# define KEY_F17 KEY_F(53)
#endif
#ifndef KEY_F18
# define KEY_F18 KEY_F(54)
#endif
#ifndef KEY_F19
# define KEY_F19 KEY_F(55)
#endif
#ifndef KEY_F20
# define KEY_F20 KEY_F(56)
#endif
#ifndef KEY_S_F13
# define KEY_S_F13 KEY_F(57)
#endif
#ifndef KEY_S_F14
# define KEY_S_F14 KEY_F(58)
#endif
#ifndef KEY_S_F15
# define KEY_S_F15 KEY_F(59)
#endif
#ifndef KEY_S_F16
# define KEY_S_F16 KEY_F(60)
#endif
#ifndef KEY_S_F17
# define KEY_S_F17 KEY_F(61)
#endif
#ifndef KEY_S_F18
# define KEY_S_F18 KEY_F(62)
#endif
#ifndef KEY_S_F19
# define KEY_S_F19 KEY_F(63)
#endif

#ifndef KEY_Find
# define KEY_Find KEY_FIND
#endif
#ifndef KEY_InsertHere
# define KEY_InsertHere KEY_IC
#endif
#ifndef KEY_Remove
# define KEY_Remove KEY_REPLACE
#endif
#ifndef KEY_Select
# define KEY_Select KEY_SELECT
#endif
#ifndef KEY_PrevScreen
# define KEY_PrevScreen KEY_PPAGE
#endif
#ifndef KEY_NextScreen
# define KEY_NextScreen KEY_NPAGE
#endif
#ifndef KEY_PF1
# define KEY_PF1 THE_KEY_PF1
#endif
#ifndef KEY_PF2
# define KEY_PF2 THE_KEY_PF2
#endif
#ifndef KEY_PF3
# define KEY_PF3 THE_KEY_PF3
#endif
#ifndef KEY_PF4
# define KEY_PF4 THE_KEY_PF4
#endif
#ifndef KEY_PadComma
# define KEY_PadComma THE_KEY_VT_PADCOMMA
#endif
#ifndef KEY_PadMinus
# define KEY_PadMinus THE_KEY_VT_PADMINUS
#endif
#ifndef KEY_PadPeriod
# define KEY_PadPeriod THE_KEY_VT_PADPERIOD
#endif
#ifndef KEY_PadPlus
# define KEY_PadPlus THE_KEY_VT_PADPLUS
#endif
#ifndef KEY_PadStar
# define KEY_PadStar THE_KEY_VT_PADSTAR
#endif
#ifndef KEY_PadSlash
# define KEY_PadSlash THE_KEY_VT_PADSLASH
#endif
#ifndef KEY_Pad0
# define KEY_Pad0 PAD0
#endif

#ifndef KEY_PB1
# define KEY_PB1 THE_KEY_PB1
# define KEY_PB2 THE_KEY_PB2
# define KEY_PB3 THE_KEY_PB3
# define KEY_S_PB1 THE_KEY_S_PB1
# define KEY_S_PB2 THE_KEY_S_PB2
# define KEY_S_PB3 THE_KEY_S_PB3
# define KEY_C_PB1 THE_KEY_C_PB1
# define KEY_C_PB2 THE_KEY_C_PB2
# define KEY_C_PB3 THE_KEY_C_PB3
# define KEY_A_PB1 THE_KEY_A_PB1
# define KEY_A_PB2 THE_KEY_A_PB2
# define KEY_A_PB3 THE_KEY_A_PB3
# define KEY_RB1 THE_KEY_RB1
# define KEY_RB2 THE_KEY_RB2
# define KEY_RB3 THE_KEY_RB3
# define KEY_S_RB1 THE_KEY_S_RB1
# define KEY_S_RB2 THE_KEY_S_RB2
# define KEY_S_RB3 THE_KEY_S_RB3
# define KEY_C_RB1 THE_KEY_C_RB1
# define KEY_C_RB2 THE_KEY_C_RB2
# define KEY_C_RB3 THE_KEY_C_RB3
# define KEY_A_RB1 THE_KEY_A_RB1
# define KEY_A_RB2 THE_KEY_A_RB2
# define KEY_A_RB3 THE_KEY_A_RB3
# define KEY_DB1 THE_KEY_DB1
# define KEY_DB2 THE_KEY_DB2
# define KEY_DB3 THE_KEY_DB3
# define KEY_S_DB1 THE_KEY_S_DB1
# define KEY_S_DB2 THE_KEY_S_DB2
# define KEY_S_DB3 THE_KEY_S_DB3
# define KEY_C_DB1 THE_KEY_C_DB1
# define KEY_C_DB2 THE_KEY_C_DB2
# define KEY_C_DB3 THE_KEY_C_DB3
# define KEY_A_DB1 THE_KEY_A_DB1
# define KEY_A_DB2 THE_KEY_A_DB2
# define KEY_A_DB3 THE_KEY_A_DB3
#endif

#ifndef KEY_PARSE_COMPLETE
# define KEY_PARSE_COMPLETE THE_KEY_PARSE_COMPLETE
#endif
#ifndef KEY_MOUSE
# define KEY_MOUSE THE_KEY_MOUSE
#endif

#endif
