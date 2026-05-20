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
/* ------------------------------------------------------------------- */
/* WARNING: This enum MUST remain in strictly ALPHABETICAL order       */
/* because THE relies on bsearch() to map string names to these items. */
/* ------------------------------------------------------------------- */
typedef enum {
    ITEM_ALT                            = 0,
    ITEM_ARBCHAR,
    ITEM_AUTOCOLOR,
    ITEM_AUTOCOLOUR,
    ITEM_AUTOSAVE,
    ITEM_AUTOSCROLL,
    ITEM_BACKUP,
    ITEM_BEEP,
    ITEM_BLOCK,
    ITEM_CASE,
    ITEM_CLEARERRORKEY,
    ITEM_CLEARSCREEN,
    ITEM_CLOCK,
    ITEM_CMDARROWS,
    ITEM_CMDLINE,
    ITEM_COLOR,
    ITEM_COLORING,
    ITEM_COLOUR,
    ITEM_COLOURING,
    ITEM_COLUMN,
    ITEM_COMPAT,
    ITEM_CTLCHAR,
    ITEM_CURLINE,
    ITEM_CURSOR,
    ITEM_CURSORSTAY,
    ITEM_CURSORSTYLE,
    ITEM_DEFINE,
    ITEM_DEFSORT,
    ITEM_DIRFILEID,
    ITEM_DIRINCLUDE,
    ITEM_DISPLAY,
    ITEM_ECOLOR,
    ITEM_ECOLOUR,
    ITEM_EFILEID,
    ITEM_EOF,
    ITEM_EOLOUT,
    ITEM_EQUIVCHAR,
    ITEM_ERRORFORMAT,
    ITEM_ERROROUTPUT,
    ITEM_ETMODE,
    ITEM_FDISPLAY,
    ITEM_FEXT,
    ITEM_FIELD,
    ITEM_FIELDWORD,
    ITEM_FILENAME,
    ITEM_FILESTATUS,
    ITEM_FILECTLCHAR,
    ITEM_FILETABS,
    ITEM_FMODE,
    ITEM_FNAME,
    ITEM_FPATH,
    ITEM_FTYPE,
    ITEM_FULLFNAME,
    ITEM_GETENV,
    ITEM_HEADER,
    ITEM_HEX,
    ITEM_HEXDISPLAY,
    ITEM_HEXSHOW,
    ITEM_HIGHLIGHT,
    ITEM_IDLINE,
    ITEM_IMPMACRO,
    ITEM_IMPOS,
    ITEM_INPUTMODE,
    ITEM_INSERTMODE,
    ITEM_INTERFACE,
    ITEM_LASTKEY,
    ITEM_LASTMSG,
    ITEM_LASTOP,
    ITEM_LASTRC,
    ITEM_LENGTH,
    ITEM_LINE,
    ITEM_LINEFLAG,
    ITEM_LINEND,
    ITEM_LSCREEN,
    ITEM_MACRO,
    ITEM_MACROEXT,
    ITEM_MACROPATH,
    ITEM_MARGINS,
    ITEM_MONITOR,
    ITEM_MOUSE,
    ITEM_MOUSECLICK,
    ITEM_MSGLINE,
    ITEM_MSGMODE,
    ITEM_NBFILE,
    ITEM_NBSCOPE,
    ITEM_NEWLINES,
    ITEM_NONDISP,
    ITEM_NUMBER,
    ITEM_PAGEWRAP,
    ITEM_PARSER,
    ITEM_PENDING,
    ITEM_PMSG,
    ITEM_POINT,
    ITEM_POSITION,
    ITEM_PREFIX,
    ITEM_PRINTER,
    ITEM_PROFILE,
    ITEM_PSCREEN,
    ITEM_READONLY,
    ITEM_READV,
    ITEM_REGEXP,
    ITEM_REPROFILE,
    ITEM_RESERVED,
    ITEM_REXX,
    ITEM_REXXHALT,
    ITEM_REXXOUTPUT,
    ITEM_RING,
    ITEM_SCALE,
    ITEM_SCOPE,
    ITEM_SCREEN,
    ITEM_SELECT,
    ITEM_SHADOW,
    ITEM_SHOWKEY,
    ITEM_SIZE,
    ITEM_SLK,
    ITEM_STATOPT,
    ITEM_STATUSLINE,
    ITEM_STAY,
    ITEM_SYNELEM,
    ITEM_SYNONYM,
    ITEM_TABKEY,
    ITEM_TABLINE,
    ITEM_TABS,
    ITEM_TABSIN,
    ITEM_TABSOUT,
    ITEM_TARGETSAVE,
    ITEM_TERMINAL,
    ITEM_THIGHLIGHT,
    ITEM_TIMECHECK,
    ITEM_TOF,
    ITEM_TOFEOF,
    ITEM_TRAILING,
    ITEM_TYPEAHEAD,
    ITEM_UI,
    ITEM_UNDOING,
    ITEM_UNTAA,
    ITEM_UTF,
    ITEM_VARIANT,
    ITEM_VERIFY,
    ITEM_VERSHIFT,
    ITEM_VERSION,
    ITEM_WIDTH,
    ITEM_WORD,
    ITEM_WORDWRAP,
    ITEM_WRAP,
    ITEM_XTERMINAL,
    ITEM_ZONE,
} QueryItemIndex;

/*---------------------------------------------------------------------*/
/* The following are item number defines for the boolean functions.    */
/*---------------------------------------------------------------------*/
/* ------------------------------------------------------------------- */
/* WARNING: This enum MUST remain in strictly ALPHABETICAL order       */
/* because THE relies on bsearch() to map string names to these items. */
/* ------------------------------------------------------------------- */
typedef enum {
    ITEM_AFTER_FUNCTION                 = 0,
    ITEM_ALTKEY_FUNCTION,
    ITEM_ALT_FUNCTION,
    ITEM_BATCH_FUNCTION,
    ITEM_BEFORE_FUNCTION,
    ITEM_BLANK_FUNCTION,
    ITEM_BLOCK_FUNCTION,
    ITEM_BOTTOMEDGE_FUNCTION,
    ITEM_COMMAND_FUNCTION,
    ITEM_CTRL_FUNCTION,
    ITEM_CURRENT_FUNCTION,
    ITEM_DIR_FUNCTION,
    ITEM_END_FUNCTION,
    ITEM_EOF_FUNCTION,
    ITEM_FIRST_FUNCTION,
    ITEM_FOCUSEOF_FUNCTION,
    ITEM_FOCUSTOF_FUNCTION,
    ITEM_INBLOCK_FUNCTION,
    ITEM_INCOMMAND_FUNCTION,
    ITEM_INITIAL_FUNCTION,
    ITEM_INPREFIX_FUNCTION,
    ITEM_INSERTMODE_FUNCTION,
    ITEM_LEFTEDGE_FUNCTION,
    ITEM_MODIFIABLE_FUNCTION,
    ITEM_RIGHTEDGE_FUNCTION,
    ITEM_RUN_OS_FUNCTION,
    ITEM_SHADOW_FUNCTION,
    ITEM_SHIFT_FUNCTION,
    ITEM_SPACECHAR_FUNCTION,
    ITEM_TOF_FUNCTION,
    ITEM_TOPEDGE_FUNCTION,
    ITEM_VALID_TARGET_FUNCTION,
    ITEM_VERONE_FUNCTION,
} QueryFunctionIndex;

/*---------------------------------------------------------------------*/
/* The following are item number defines for the 'other' functions.    */
/*---------------------------------------------------------------------*/
