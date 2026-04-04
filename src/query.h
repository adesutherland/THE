/***********************************************************************/
/* QUERY.H -                                                           */
/* This file contains defines   related to QUERY,STATUS and EXTRACT    */
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
 * Mark Hessling,  M.Hessling@qut.edu.au  http://www.lightlink.com/hessling/
 */

/*
$Id: query.h,v 1.27 2022/07/06 06:29:09 mark Exp $
*/


/*---------------------------------------------------------------------*/
/* The order of these items determine the order they appear as a result*/
/* of the STATUS command, so they should be in alphabetical order.     */
/*---------------------------------------------------------------------*/
/* The following are item number defines for EXTRACT/QUERY/STATUS.     */
/*---------------------------------------------------------------------*/
#define ITEM_ALT                            0
#define ITEM_ARBCHAR                        1
#define ITEM_AUTOCOLOR                      2
#define ITEM_AUTOCOLOUR                     3
#define ITEM_AUTOSAVE                       4
#define ITEM_AUTOSCROLL                     5
#define ITEM_BACKUP                         6
#define ITEM_BEEP                           7
#define ITEM_BLOCK                          8
#define ITEM_CASE                           9
#define ITEM_CLEARERRORKEY                  10
#define ITEM_CLEARSCREEN                    11
#define ITEM_CLOCK                          12
#define ITEM_CMDARROWS                      13
#define ITEM_CMDLINE                        14
#define ITEM_COLOR                          15
#define ITEM_COLORING                       16
#define ITEM_COLOUR                         17
#define ITEM_COLOURING                      18
#define ITEM_COLUMN                         19
#define ITEM_COMPAT                         20
#define ITEM_CTLCHAR                        21
#define ITEM_CURLINE                        22
#define ITEM_CURSOR                         23
#define ITEM_CURSORSTAY                     24
#define ITEM_DEFINE                         25
#define ITEM_DEFSORT                        26
#define ITEM_DIRFILEID                      27
#define ITEM_DIRINCLUDE                     28
#define ITEM_DISPLAY                        29
#define ITEM_ECOLOR                         30
#define ITEM_ECOLOUR                        31
#define ITEM_EFILEID                        32
#define ITEM_EOF                            33
#define ITEM_EOLOUT                         34
#define ITEM_EQUIVCHAR                      35
#define ITEM_ERRORFORMAT                    36
#define ITEM_ERROROUTPUT                    37
#define ITEM_ETMODE                         38
#define ITEM_FDISPLAY                       39
#define ITEM_FEXT                           40
#define ITEM_FIELD                          41
#define ITEM_FIELDWORD                      42
#define ITEM_FILENAME                       43
#define ITEM_FILESTATUS                     44
#define ITEM_FILETABS                       45
#define ITEM_FMODE                          46
#define ITEM_FNAME                          47
#define ITEM_FPATH                          48
#define ITEM_FTYPE                          49
#define ITEM_FULLFNAME                      50
#define ITEM_GETENV                         51
#define ITEM_HEADER                         52
#define ITEM_HEX                            53
#define ITEM_HEXDISPLAY                     54
#define ITEM_HEXSHOW                        55
#define ITEM_HIGHLIGHT                      56
#define ITEM_IDLINE                         57
#define ITEM_IMPMACRO                       58
#define ITEM_IMPOS                          59
#define ITEM_INPUTMODE                      60
#define ITEM_INSERTMODE                     61
#define ITEM_INTERFACE                      62
#define ITEM_LASTKEY                        63
#define ITEM_LASTMSG                        64
#define ITEM_LASTOP                         65
#define ITEM_LASTRC                         66
#define ITEM_LENGTH                         67
#define ITEM_LINE                           68
#define ITEM_LINEFLAG                       69
#define ITEM_LINEND                         70
#define ITEM_LSCREEN                        71
#define ITEM_MACRO                          72
#define ITEM_MACROEXT                       73
#define ITEM_MACROPATH                      74
#define ITEM_MARGINS                        75
#define ITEM_MONITOR                        76
#define ITEM_MOUSE                          77
#define ITEM_MOUSECLICK                     78
#define ITEM_MSGLINE                        79
#define ITEM_MSGMODE                        80
#define ITEM_NBFILE                         81
#define ITEM_NBSCOPE                        82
#define ITEM_NEWLINES                       83
#define ITEM_NONDISP                        84
#define ITEM_NUMBER                         85
#define ITEM_PAGEWRAP                       86
#define ITEM_PARSER                         87
#define ITEM_PENDING                        88
#define ITEM_PMSG                           89
#define ITEM_POINT                          90
#define ITEM_POSITION                       91
#define ITEM_PREFIX                         92
#define ITEM_PRINTER                        93
#define ITEM_PROFILE                        94
#define ITEM_PSCREEN                        95
#define ITEM_READONLY                       96
#define ITEM_READV                          97
#define ITEM_REGEXP                         98
#define ITEM_REPROFILE                      99
#define ITEM_RESERVED                       100
#define ITEM_REXX                           101
#define ITEM_REXXHALT                       102
#define ITEM_REXXOUTPUT                     103
#define ITEM_RING                           104
#define ITEM_SCALE                          105
#define ITEM_SCOPE                          106
#define ITEM_SCREEN                         107
#define ITEM_SELECT                         108
#define ITEM_SHADOW                         109
#define ITEM_SHOWKEY                        110
#define ITEM_SIZE                           111
#define ITEM_SLK                            112
#define ITEM_STATOPT                        113
#define ITEM_STATUSLINE                     114
#define ITEM_STAY                           115
#define ITEM_SYNELEM                        116
#define ITEM_SYNONYM                        117
#define ITEM_TABKEY                         118
#define ITEM_TABLINE                        119
#define ITEM_TABS                           120
#define ITEM_TABSIN                         121
#define ITEM_TABSOUT                        122
#define ITEM_TARGETSAVE                     123
#define ITEM_TERMINAL                       124
#define ITEM_THIGHLIGHT                     125
#define ITEM_TIMECHECK                      126
#define ITEM_TOF                            127
#define ITEM_TOFEOF                         128
#define ITEM_TRAILING                       129
#define ITEM_TYPEAHEAD                      130
#define ITEM_UI                             131
#define ITEM_UNDOING                        132
#define ITEM_UNTAA                          133
#define ITEM_UTF8                           134
#define ITEM_VARIANT                        135
#define ITEM_VERIFY                         136
#define ITEM_VERSHIFT                       137
#define ITEM_VERSION                        138
#define ITEM_WIDTH                          139
#define ITEM_WORD                           140
#define ITEM_WORDWRAP                       141
#define ITEM_WRAP                           142
#define ITEM_XTERMINAL                      143
#define ITEM_ZONE                           144

/*---------------------------------------------------------------------*/
/* The following are item number defines for the boolean functions.    */
/*---------------------------------------------------------------------*/
#define ITEM_AFTER_FUNCTION                   0
#define ITEM_ALT_FUNCTION                     1
#define ITEM_ALTKEY_FUNCTION                  2
#define ITEM_BATCH_FUNCTION                   3
#define ITEM_BEFORE_FUNCTION                  4
#define ITEM_BLANK_FUNCTION                   5
#define ITEM_BLOCK_FUNCTION                   6
#define ITEM_BOTTOMEDGE_FUNCTION              7
#define ITEM_COMMAND_FUNCTION                 8
#define ITEM_CTRL_FUNCTION                    9
#define ITEM_CURRENT_FUNCTION                10
#define ITEM_DIR_FUNCTION                    11
#define ITEM_END_FUNCTION                    12
#define ITEM_EOF_FUNCTION                    13
#define ITEM_FIRST_FUNCTION                  14
#define ITEM_FOCUSEOF_FUNCTION               15
#define ITEM_FOCUSTOF_FUNCTION               16
#define ITEM_INBLOCK_FUNCTION                17
#define ITEM_INCOMMAND_FUNCTION              18
#define ITEM_INITIAL_FUNCTION                19
#define ITEM_INPREFIX_FUNCTION               20
#define ITEM_INSERTMODE_FUNCTION             21
#define ITEM_LEFTEDGE_FUNCTION               22
#define ITEM_MODIFIABLE_FUNCTION             23
#define ITEM_RIGHTEDGE_FUNCTION              24
#define ITEM_RUN_OS_FUNCTION                 25
#define ITEM_SHADOW_FUNCTION                 26
#define ITEM_SHIFT_FUNCTION                  27
#define ITEM_SPACECHAR_FUNCTION              28
#define ITEM_TOF_FUNCTION                    29
#define ITEM_TOPEDGE_FUNCTION                30
#define ITEM_VALID_TARGET_FUNCTION           31
#define ITEM_VERONE_FUNCTION                 32
/*---------------------------------------------------------------------*/
/* The following are item number defines for the 'other' functions.    */
/*---------------------------------------------------------------------*/

