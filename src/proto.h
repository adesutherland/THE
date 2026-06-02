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
$Id: proto.h,v 1.66 2022/07/06 06:39:20 mark Exp $
*/

struct TheInputLogicalTarget;

#include "driverwindow.h"

                                                         /* commutil.c */
CHARTYPE *get_key_name (int,int *);
CHARTYPE *get_key_definition (int,int,bool,bool);
short function_key (int,int,bool);
bool is_modifier_key (int);
CHARTYPE *build_default_key_definition (int, CHARTYPE *);
CHARTYPE *build_synonym_definition ( DEFINE *, CHARTYPE *, CHARTYPE *, bool );
short display_all_keys (void);
int set_rexx_variables_for_all_keys (int,int *);
short command_line (CHARTYPE *,bool);
void cleanup_command_line (void);
void split_command (CHARTYPE *,CHARTYPE *,CHARTYPE *);
short param_split (CHARTYPE *, CHARTYPE *[], int, CHARTYPE *, CHARTYPE, CHARTYPE *, bool);
short quoted_param_split (CHARTYPE *, CHARTYPE *[], int, CHARTYPE *, CHARTYPE, CHARTYPE *, bool, CHARTYPE *);
short command_split (CHARTYPE *, CHARTYPE *[], int, CHARTYPE *, CHARTYPE *);
LINETYPE get_true_line (bool);
LENGTHTYPE get_true_column (bool);
CHARTYPE next_char (LINE *,long *,LENGTHTYPE);
short add_define (DEFINE **,DEFINE **,int,CHARTYPE *,bool,CHARTYPE *,CHARTYPE);
short remove_define (DEFINE **,DEFINE **,int,CHARTYPE *);
short append_define (DEFINE **,DEFINE **,int,short,CHARTYPE *,CHARTYPE *,int,CHARTYPE *,CHARTYPE);
short find_command (CHARTYPE *,bool);
void init_command (void);
void add_command (CHARTYPE *);
CHARTYPE *get_next_command (short, int);
bool valid_command_to_save (CHARTYPE *);
bool is_tab_col (LENGTHTYPE);
LENGTHTYPE find_next_tab_col (LENGTHTYPE);
LENGTHTYPE find_prev_tab_col (LENGTHTYPE);
short tabs_convert (LINE *,bool,bool,bool);
short convert_hex_strings (CHARTYPE *);
short marked_block (bool);
short suspend_curses (void);
short resume_curses (void);
short restore_THE (void);
short execute_set_sos_command (bool,CHARTYPE *);
short valid_command_type (bool,CHARTYPE *);
short allocate_temp_space (LENGTHTYPE,CHARTYPE);
void free_temp_space (CHARTYPE);
CHARTYPE calculate_actual_row (short, short, ROWTYPE, bool);
short get_valid_macro_file_name (CHARTYPE *,CHARTYPE *,CHARTYPE *,short *);
bool define_command (CHARTYPE *);
int find_key_name (CHARTYPE *);
int readv_cmdline (CHARTYPE *, TheDriverWindow *, int);
short execute_mouse_commands (int);
short validate_n_m (CHARTYPE *,short *,short *);
void ResetOrDeleteCUABlock ( int );
short execute_locate (CHARTYPE *,bool,bool,bool *);
void adjust_other_screen_shadow_lines ( void );
int is_file_in_ring ( CHARTYPE *fpath, CHARTYPE *fname );
int save_lastop ( int idx, CHARTYPE *lastop );
CHARTYPE *get_command_name (int idx, bool *set_command, bool *sos_command);

                                                            /* print.c */
#ifdef WIN32
void StartTextPrnt (void);
void StopTextPrnt (void);
#endif
void print_line (bool ,LINETYPE,LINETYPE ,short ,CHARTYPE *,CHARTYPE *,short);
short setprintername (char*);
short setfontname (char*);
short setfontcpi (int);
short setfontlpi (int);
short setorient (char);
short setpagesize (int);
                                                           /* target.c */
short split_change_params (CHARTYPE *,CHARTYPE **,CHARTYPE **,TARGET *,LINETYPE *,LINETYPE *);
short parse_target (CHARTYPE *,LINETYPE,TARGET *,long,bool,bool,bool);
void initialise_target (TARGET *);
void free_target (TARGET *);
short find_target (TARGET *,LINETYPE,bool,bool);
short find_column_target (CHARTYPE *,LENGTHTYPE,TARGET *,LENGTHTYPE,bool,bool);
THELIST *find_line_name ( LINE *curr, CHARTYPE *name );
LINE *find_named_line (CHARTYPE *,LINETYPE *,bool);
short find_string_target (LINE *,RTARGET *, LENGTHTYPE, int);
short find_rtarget_target (LINE *,TARGET *,LINETYPE,LINETYPE,LINETYPE *);
bool find_rtarget_column_target (CHARTYPE *,LENGTHTYPE,TARGET *,LENGTHTYPE,LENGTHTYPE,LINETYPE *);
LINETYPE find_next_in_scope (VIEW_DETAILS *,LINE *,LINETYPE,short);
LINETYPE find_last_not_in_scope (VIEW_DETAILS *,LINE *,LINETYPE,short);
short validate_target (CHARTYPE *,TARGET *,long,LINETYPE,bool,bool);
void calculate_scroll_values ( CHARTYPE, VIEW_DETAILS *, short *, LINETYPE *, LINETYPE *, bool *, bool *, bool *, short );
short find_first_focus_line ( CHARTYPE, unsigned short * );
short find_last_focus_line ( CHARTYPE, unsigned short * );
CHARTYPE find_unique_char (CHARTYPE *);
                                                         /* reserved.c */
RESERVED *add_reserved_line (CHARTYPE *,CHARTYPE *,short,short,COLOUR_ATTR *,bool);
RESERVED *find_reserved_line (CHARTYPE,bool,ROWTYPE,short,short);
short delete_reserved_line (short,short);
#ifdef CTLCHAR
TheDriverAttr *apply_ctlchar_to_reserved_line (RESERVED *);
#endif
                                                              /* box.c */
void box_operations (short ,CHARTYPE ,bool ,CHARTYPE );
void box_paste_from_clipboard ( LINE *, LINETYPE, LINETYPE );
                                                          /* execute.c */
short execute_os_command (CHARTYPE *,bool ,bool );
short execute_change_command (CHARTYPE *,bool );
short insert_new_line (CHARTYPE, VIEW_DETAILS *,CHARTYPE *,LENGTHTYPE,LINETYPE,LINETYPE,bool,bool,bool,CHARTYPE,bool,bool);
short execute_makecurr ( CHARTYPE, VIEW_DETAILS *, LINETYPE );
short execute_shift_command ( CHARTYPE, VIEW_DETAILS *, bool, LENGTHTYPE, LINETYPE, LINETYPE, bool, long, bool, bool );
short execute_set_lineflag ( unsigned int, unsigned int, unsigned int, LINETYPE, LINETYPE, bool, long );
short do_actual_change_case (LINETYPE, LINETYPE,CHARTYPE,bool,short,LENGTHTYPE,LENGTHTYPE);
short execute_change_case (CHARTYPE *,CHARTYPE);
short rearrange_line_blocks (CHARTYPE,CHARTYPE,LINETYPE,LINETYPE,LINETYPE,long,VIEW_DETAILS*,VIEW_DETAILS*,bool,LINETYPE *);
short execute_set_point ( CHARTYPE, VIEW_DETAILS *, CHARTYPE *, LINETYPE, bool );
short execute_wrap_word (LENGTHTYPE);
short execute_split_join (short,bool,bool);
short execute_put (CHARTYPE *,bool);
short execute_macro (CHARTYPE *,bool, short*);
short write_macro (CHARTYPE *);
short execute_set_on_off (CHARTYPE *,bool *, bool);
short execute_set_row_position (CHARTYPE *,short *,short *);
short processable_line (VIEW_DETAILS *,LINETYPE,LINE *);
short execute_expand_compress (CHARTYPE *,bool,bool,bool,bool);
short execute_select (CHARTYPE *,bool,short);
short execute_move_cursor ( CHARTYPE, VIEW_DETAILS *, LENGTHTYPE );
short execute_find_command (CHARTYPE *,long);
short execute_modify_command (CHARTYPE *);
LENGTHTYPE calculate_rec_len (short,CHARTYPE*,LENGTHTYPE,LENGTHTYPE,LINETYPE,short);
short execute_editv (short,bool,CHARTYPE *);
short prepare_dialog (CHARTYPE *,bool,CHARTYPE *);
short execute_dialog (CHARTYPE *,CHARTYPE *,CHARTYPE *,bool,short,short,CHARTYPE *,short,bool);
short prepare_popup (CHARTYPE *);
short execute_popup (int, int, int, int , int , int , int , int , CHARTYPE **, int);
short execute_preserve (VIEW_DETAILS *, PRESERVED_VIEW_DETAILS **, FILE_DETAILS *, PRESERVED_FILE_DETAILS **);
short execute_restore (VIEW_DETAILS *, PRESERVED_VIEW_DETAILS **, FILE_DETAILS *, PRESERVED_FILE_DETAILS **, bool);
                                                          /* default.c */
void set_global_defaults (void);
void set_global_look_defaults (void);
void set_global_feel_defaults (void);
void set_file_defaults (FILE_DETAILS *);
void set_view_defaults (VIEW_DETAILS *);
short get_profile (CHARTYPE *,CHARTYPE *);
short get_startup_profiles (void);
short defaults_for_first_file (void);
short defaults_for_other_files (VIEW_DETAILS *);
short default_file_attributes (FILE_DETAILS *);
void set_screen_defaults (void);
short set_THE_key_defaults (int,int);
short set_XEDIT_key_defaults (int,int);
short set_ISPF_key_defaults (int,int);
short set_KEDIT_key_defaults (int,int);
short construct_default_parsers (void);
short destroy_all_parsers (void);
short construct_default_parser_mapping (void);
CHARTYPE *find_default_parser (CHARTYPE *);
                                                             /* edit.c */
void editor (void);
int process_key (int,bool);
short EditFile (CHARTYPE *,bool);
                                                            /* error.c */
int display_error (unsigned short ,CHARTYPE *,bool);
int message_history_count (void);
const CHARTYPE *message_history_get (int,LENGTHTYPE *);
void clear_msgline (int);
void display_prompt (CHARTYPE *);
int expose_msgline (void);
                                                             /* file.c */
short get_file (CHARTYPE *);
#ifdef USE_SDSLH
void sdslh_init_file(FILE_DETAILS *);
void sdslh_shutdown_file(FILE_DETAILS *);
#endif
LINE *read_file ( FILE *, LINE *, CHARTYPE *, LINETYPE, LINETYPE, bool );
LINE *read_fixed_file (FILE *,LINE *,CHARTYPE *,LINETYPE,LINETYPE);
short save_file (FILE_DETAILS *,CHARTYPE *,bool,LINETYPE,LINETYPE,LINETYPE *,bool,LENGTHTYPE,LENGTHTYPE,bool,bool,bool);
void increment_alt (FILE_DETAILS *);
CHARTYPE *new_filename (CHARTYPE *,CHARTYPE *,CHARTYPE *,CHARTYPE *);
short remove_aus_file (FILE_DETAILS *);
short free_view_memory (bool,bool);
void free_a_view (void);
short free_file_memory (bool);
short read_directory (void);
VIEW_DETAILS *find_file (CHARTYPE *,CHARTYPE *);
VIEW_DETAILS *find_pseudo_file (CHARTYPE);
short execute_command_file (FILE *);
CHARTYPE *read_file_into_memory (CHARTYPE *,int *);
                                                            /* getch.c */
int my_getch  (TheDriverWindow *);
                                                          /* nonansi.c */
short file_readable (CHARTYPE *);
short file_writable (CHARTYPE *);
short file_exists (CHARTYPE *);
short remove_file (CHARTYPE *);
short splitpath (CHARTYPE *);
#ifndef HAVE_RENAME
short rename (CHARTYPE *,CHARTYPE *);
#endif
LINE *getclipboard (LINE *, int);
short setclipboard (FILE_DETAILS *,CHARTYPE *,bool,LINETYPE,LINETYPE,LINETYPE,LINETYPE *,bool,LENGTHTYPE,LENGTHTYPE,bool,bool,int);
CursorShape current_cursor_shape (void);
CursorBlink current_cursor_blink (void);
CursorPresentation current_cursor_presentation (void);
bool current_cursor_uses_software (void);
int is_a_dir_stat (ATTR_TYPE);
int is_a_dir_dir (ATTR_TYPE);
                                                           /* parser.c */
short parse_line (CHARTYPE,FILE_DETAILS *,SHOW_LINE *,short);
short parse_paired_comments (CHARTYPE,FILE_DETAILS *);
short construct_parser (CHARTYPE *, int, PARSER_DETAILS **,CHARTYPE *,CHARTYPE *);
short destroy_parser (PARSER_DETAILS *);
bool find_parser_mapping (FILE_DETAILS *, PARSER_MAPPING *);
PARSER_DETAILS *find_auto_parser (FILE_DETAILS *);
short parse_reserved_line (RESERVED *);
                                                           /* prefix.c */
short execute_prefix_commands (void);
void clear_pending_prefix_command (THE_PPC *,FILE_DETAILS *,LINE *);
THE_PPC *delete_pending_prefix_command (THE_PPC *,FILE_DETAILS *,LINE *);
void add_prefix_command (CHARTYPE, VIEW_DETAILS *,LINE *,LINETYPE,bool,bool);
short add_prefix_synonym (CHARTYPE *,CHARTYPE *);
CHARTYPE *find_prefix_synonym (CHARTYPE *);
CHARTYPE *find_prefix_oldname (CHARTYPE *);
CHARTYPE *get_prefix_command (LINETYPE);
CHARTYPE get_syntax_element (CHARTYPE, int, int);
                                                             /* show.c */
void prepare_idline (CHARTYPE);
void show_heading (CHARTYPE);
void show_statarea (void);
void clear_statarea (void);
void display_filetabs (VIEW_DETAILS *);
void build_screen (CHARTYPE);
void display_screen (CHARTYPE);
void display_cmdline ( CHARTYPE, VIEW_DETAILS * );
void display_prefix_line ( CHARTYPE, VIEW_DETAILS * );
#ifdef USE_UTF8
int show_utf8_display_col_from_logical (const CHARTYPE *, size_t, int, int);
int show_utf8_logical_col_from_display (const CHARTYPE *, size_t, int, int, TextSnap);
void show_utf8_note_line_replacement (LINETYPE, const CHARTYPE *, LENGTHTYPE);
void show_utf8_filearea_cursor_transition (CHARTYPE, short, int, int);
#endif
void show_marked_block (void);
void redraw_window (TheDriverWindow *);
void repaint_screen (void);
void touch_screen (CHARTYPE);
void refresh_screen (CHARTYPE);
void redraw_screen (CHARTYPE);
bool line_in_view (CHARTYPE,LINETYPE);
bool column_in_view (CHARTYPE,LENGTHTYPE);
LINETYPE find_next_current_line (LINETYPE,short);
short get_row_for_focus_line (CHARTYPE,LINETYPE,short);
LINETYPE get_focus_line_in_view ( CHARTYPE, LINETYPE, ROWTYPE);
LINETYPE calculate_focus_line (LINETYPE,LINETYPE);
char *get_current_position (CHARTYPE,LINETYPE *,LENGTHTYPE *);
void calculate_new_column ( CHARTYPE, VIEW_DETAILS *, COLTYPE, LENGTHTYPE, LENGTHTYPE, COLTYPE *, LENGTHTYPE * );
short prepare_view (CHARTYPE);
short advance_view (VIEW_DETAILS *,short);
short THE_Resize (int,int);
                                                           /* scroll.c */
short scroll_page (short,LINETYPE,bool);
short scroll_line ( CHARTYPE, VIEW_DETAILS *, short, LINETYPE, bool, short );
                                                              /* the.c */
void init_colour_pairs (void);
int setup_profile_files (CHARTYPE *);
void cleanup (void);
int allocate_working_memory (void);
char **StringToArgv ( int *, char* );
#if !defined(HAVE_STRICMP) && !defined(HAVE_STRCMPI) && !defined(HAVE_STRCASECMP)
LENGTHTYPE my_stricmp ( DEFCHAR *,DEFCHAR * );
#endif
                                                             /* util.c */
CHARTYPE *ebc2asc (CHARTYPE *, int, int, int);
CHARTYPE *asc2ebc (CHARTYPE *, int, int, int);
LENGTHTYPE memreveq (CHARTYPE *, CHARTYPE, LENGTHTYPE);
LENGTHTYPE memrevne (CHARTYPE *, CHARTYPE, LENGTHTYPE);
CHARTYPE *meminschr (CHARTYPE *, CHARTYPE, LENGTHTYPE, LENGTHTYPE ,LENGTHTYPE);
CHARTYPE *meminsmem (CHARTYPE *, CHARTYPE *, LENGTHTYPE, LENGTHTYPE, LENGTHTYPE, LENGTHTYPE);
CHARTYPE *memdeln (CHARTYPE *, LENGTHTYPE, LENGTHTYPE, LENGTHTYPE);
CHARTYPE *strdelchr (CHARTYPE *, CHARTYPE);
CHARTYPE *memrmdup (CHARTYPE *, LENGTHTYPE *, CHARTYPE);
CHARTYPE *strrmdup (CHARTYPE *, CHARTYPE, bool);
LENGTHTYPE strzne (CHARTYPE *, CHARTYPE);
CHARTYPE *my_strdup (CHARTYPE *);
LENGTHTYPE memne (CHARTYPE *, CHARTYPE, LENGTHTYPE);
LENGTHTYPE strzrevne (CHARTYPE *, CHARTYPE);
LENGTHTYPE strzreveq (CHARTYPE *, CHARTYPE);
CHARTYPE *strtrunc (CHARTYPE *);
CHARTYPE *MyStrip (CHARTYPE *, char, char);
LENGTHTYPE memfind (CHARTYPE *, CHARTYPE *, LENGTHTYPE, LENGTHTYPE, bool, bool, CHARTYPE, CHARTYPE, LENGTHTYPE *);
void memrev (CHARTYPE *, CHARTYPE *, LENGTHTYPE);
LENGTHTYPE memcmpi (CHARTYPE *, CHARTYPE *, LENGTHTYPE);
CHARTYPE *make_upper (CHARTYPE *);
bool equal (CHARTYPE *,CHARTYPE *,LENGTHTYPE);
bool valid_integer (CHARTYPE *);
bool valid_positive_integer (CHARTYPE *);
short valid_positive_integer_against_maximum ( CHARTYPE *, LENGTHTYPE );
LENGTHTYPE strzeq (CHARTYPE *,CHARTYPE);
CHARTYPE *strtrans (CHARTYPE *,CHARTYPE,CHARTYPE);
LINE *add_LINE ( LINE *, LINE *, CHARTYPE *, LENGTHTYPE, SELECTTYPE, bool );
LINE *append_LINE (LINE *, CHARTYPE *, LENGTHTYPE);
LINE *delete_LINE (LINE **, LINE **, LINE *, short, bool);
void put_string (TheDriverWindow *, ROWTYPE, COLTYPE, CHARTYPE *, LENGTHTYPE);
void put_char (TheDriverWindow *, TheDriverCell, CHARTYPE);
short set_up_windows (short);
short draw_divider (void);
short create_statusline_window (void);
short create_filetabs_window (void);
void pre_process_line (VIEW_DETAILS *, LINETYPE, LINE *);
short post_process_line (VIEW_DETAILS *, LINETYPE, LINE *, bool);
bool blank_field (CHARTYPE *);
void adjust_marked_lines (bool, LINETYPE, LINETYPE);
void adjust_pending_prefix (VIEW_DETAILS *, bool, LINETYPE, LINETYPE);
CHARTYPE case_translate (CHARTYPE );
void add_to_recovery_list (CHARTYPE *, LENGTHTYPE);
void get_from_recovery_list (short);
void free_recovery_list (void);
short my_wmove (TheDriverWindow *, short, short, short, short);
short my_isalphanum (CHARTYPE);
short get_row_for_tof_eof (short, CHARTYPE);
void set_compare_exact (bool);
int search_query_item_array (void *, size_t, size_t, const char *, int);
int split_function_name (CHARTYPE *, int *);
char *thetmpnam (char *);
VIEW_DETAILS *find_filetab (int);
VIEW_DETAILS *find_next_file (VIEW_DETAILS *,short);

#if THIS_APPEARS_TO_NOT_BE_USED
TheDriverWindow *adjust_window (TheDriverWindow *,short ,short ,short ,short );
#endif

short my_wclrtoeol (TheDriverWindow *);
short my_wdelch (TheDriverWindow *);
short get_word (CHARTYPE *, LENGTHTYPE, LENGTHTYPE, LENGTHTYPE *, LENGTHTYPE *);
short get_fieldword (CHARTYPE *, LENGTHTYPE, LENGTHTYPE, LENGTHTYPE *, LENGTHTYPE *);

                                                           /* linked.c */
THELIST *ll_add ( THELIST *first, THELIST *curr, unsigned short size );
THELIST *ll_del ( THELIST **first, THELIST **last, THELIST *curr, short direction, THELIST_DEL delfunc );
THELIST *ll_free ( THELIST *first, THELIST_DEL delfunc );
LINE *lll_add (LINE *,LINE *,unsigned short );
LINE *lll_del (LINE **,LINE **,LINE *,short );
LINE *lll_free (LINE *);
LINE *lll_find (LINE *,LINE *,LINETYPE,LINETYPE);
LINE *lll_locate (LINE *,CHARTYPE *);
VIEW_DETAILS *vll_add (VIEW_DETAILS *,VIEW_DETAILS *,unsigned short );
VIEW_DETAILS *vll_del (VIEW_DETAILS **,VIEW_DETAILS **,VIEW_DETAILS *,short );
DEFINE *dll_add (DEFINE *,DEFINE *,unsigned short );
DEFINE *dll_del (DEFINE **,DEFINE **,DEFINE *,short );
DEFINE *dll_free (DEFINE *);
THE_PPC *pll_add (THE_PPC **,unsigned short, LINETYPE );
THE_PPC *pll_del (THE_PPC **,THE_PPC **,THE_PPC *,short );
THE_PPC *pll_free (THE_PPC *);
THE_PPC *pll_find (THE_PPC *,LINETYPE);
RESERVED *rll_add (RESERVED *,RESERVED *,unsigned short );
RESERVED *rll_del (RESERVED **,RESERVED **,RESERVED *,short );
RESERVED *rll_free (RESERVED *);
RESERVED *rll_find (RESERVED *,short);
PARSER_DETAILS *parserll_add (PARSER_DETAILS *,PARSER_DETAILS *,unsigned short );
PARSER_DETAILS *parserll_del (PARSER_DETAILS **,PARSER_DETAILS **,PARSER_DETAILS *,short );
PARSER_DETAILS *parserll_free (PARSER_DETAILS *);
PARSER_DETAILS *parserll_find (PARSER_DETAILS *,CHARTYPE *);
PARSE_KEYWORDS *parse_keywordll_add (PARSE_KEYWORDS *,PARSE_KEYWORDS *,unsigned short );
PARSE_KEYWORDS *parse_keywordll_del (PARSE_KEYWORDS **,PARSE_KEYWORDS **,PARSE_KEYWORDS *,short );
PARSE_KEYWORDS *parse_keywordll_free (PARSE_KEYWORDS *);
PARSE_FUNCTIONS *parse_functionll_add (PARSE_FUNCTIONS *,PARSE_FUNCTIONS *,unsigned short );
PARSE_FUNCTIONS *parse_functionll_del (PARSE_FUNCTIONS **,PARSE_FUNCTIONS **,PARSE_FUNCTIONS *,short );
PARSE_FUNCTIONS *parse_functionll_free (PARSE_FUNCTIONS *);
PARSE_HEADERS *parse_headerll_add (PARSE_HEADERS *,PARSE_HEADERS *,unsigned short );
PARSE_HEADERS *parse_headerll_free (PARSE_HEADERS *);
PARSER_MAPPING *mappingll_add (PARSER_MAPPING *,PARSER_MAPPING *,unsigned short );
PARSER_MAPPING *mappingll_del (PARSER_MAPPING **,PARSER_MAPPING **,PARSER_MAPPING *,short );
PARSER_MAPPING *mappingll_free (PARSER_MAPPING *);
PARSER_MAPPING *mappingll_find (PARSER_MAPPING *,CHARTYPE *,CHARTYPE *);
PARSE_COMMENTS *parse_commentsll_add (PARSE_COMMENTS *,PARSE_COMMENTS *,unsigned short );
PARSE_COMMENTS *parse_commentsll_del (PARSE_COMMENTS **,PARSE_COMMENTS **,PARSE_COMMENTS *,short );
PARSE_COMMENTS *parse_commentsll_free (PARSE_COMMENTS *);
PARSE_COMMENTS *parse_commentsll_find (PARSE_COMMENTS *,CHARTYPE *);
PARSE_POSTCOMPARE *parse_postcomparell_add (PARSE_POSTCOMPARE *,PARSE_POSTCOMPARE *,unsigned short );
PARSE_POSTCOMPARE *parse_postcomparell_del (PARSE_POSTCOMPARE **,PARSE_POSTCOMPARE **,PARSE_POSTCOMPARE *,short );
PARSE_POSTCOMPARE *parse_postcomparell_free (PARSE_POSTCOMPARE *);
PARSE_EXTENSION *parse_extensionll_add (PARSE_EXTENSION *,PARSE_EXTENSION *,unsigned short );
PARSE_EXTENSION *parse_extensionll_del (PARSE_EXTENSION **,PARSE_EXTENSION **,PARSE_EXTENSION *,short );
PARSE_EXTENSION *parse_extensionll_free (PARSE_EXTENSION *);
                                                             /* rexx.c */
unsigned long MyRexxRegisterFunctionExe (CHARTYPE *);
unsigned long MyRexxDeregisterFunction (CHARTYPE *);
short initialise_rexx (void);
short finalise_rexx (void);
short execute_macro_file (CHARTYPE *,CHARTYPE *,short *,bool);
short execute_macro_instore (CHARTYPE *,short *,CHARTYPE **,int *,int *,int);
short get_rexx_variable (CHARTYPE *,CHARTYPE **,int *);
short set_rexx_variable (CHARTYPE *,CHARTYPE *,LENGTHTYPE,int);
CHARTYPE *get_rexx_interpreter_version (CHARTYPE *);
                                                           /* crexx.c */
short initialise_crexx (void);
short finalise_crexx (void);
short execute_crexx_macro_file (CHARTYPE *,CHARTYPE *,short *,bool);
short execute_crexx_macro_instore (CHARTYPE *,short *,CHARTYPE **,int *,int *,int);
short set_crexx_variable (CHARTYPE *,CHARTYPE *,LENGTHTYPE,int);
short get_crexx_variable (CHARTYPE *,CHARTYPE **,int *);
CHARTYPE *get_crexx_interpreter_version (CHARTYPE *);
                                                            /* query.c */
short find_query_item (CHARTYPE *,int,CHARTYPE *);
short show_status (void);
short save_status (CHARTYPE *);
short set_extract_variables (short);
short get_number_dynamic_items (int);
short get_item_values (int,short,CHARTYPE *,CHARTYPE,LINETYPE,CHARTYPE *,LINETYPE);
int number_query_item ( void );
int number_function_item ( void );
void format_options ( CHARTYPE * );
                                                         /* directry.c */
short set_dirtype (CHARTYPE *);
CHARTYPE *get_dirtype (CHARTYPE *);
                                                          /* thematch.c */
int thematch (char *,char *,int);
                                                             /* sort.c */
short execute_sort (CHARTYPE *);
                                                           /* cursor.c */
short THEcursor_cmdline ( CHARTYPE, VIEW_DETAILS *, short );
void cursor_focus_redraw (CHARTYPE, VIEW_DETAILS *);
void cursor_focus_refresh (CHARTYPE, VIEW_DETAILS *);
void cursor_focus_sync_current (CHARTYPE, VIEW_DETAILS *);
void cursor_focus_present (CHARTYPE);
short cursor_focus_enter_command (CHARTYPE, VIEW_DETAILS *, short, bool);
short THEcursor_column (void);
short THEcursor_down ( CHARTYPE, VIEW_DETAILS *, short );
short THEcursor_file (bool,LINETYPE,LENGTHTYPE);
short THEcursor_home ( CHARTYPE, VIEW_DETAILS *, bool );
short THEcursor_left (short,bool);
short THEcursor_right (short,bool);
short THEcursor_up (short);
short THEcursor_sdown ( CHARTYPE, VIEW_DETAILS *, short );
short THEcursor_move ( CHARTYPE, VIEW_DETAILS *, bool, bool, short, short );
short THEcursor_goto (LINETYPE,LENGTHTYPE);
short THEcursor_mouse (void);
long where_now (void);
long what_current_now (void);
long what_other_now (void);
long where_next (long,long,long);
long where_before (long,long,long);
bool enterable_field (long);
short go_to_new_field (long,long);
void get_cursor_position (LINETYPE *, LENGTHTYPE *, LINETYPE *, LENGTHTYPE *);
short advance_focus_line (LINETYPE);
short advance_current_line (LINETYPE);
short advance_current_or_focus_line (LINETYPE);
void resolve_current_and_focus_lines ( CHARTYPE, VIEW_DETAILS *, LINETYPE, LINETYPE , short, bool , bool );
                                                           /* colour.c */
TheDriverAttr set_colour (const COLOUR_ATTR *);
short parse_colours (CHARTYPE *,COLOUR_ATTR *,CHARTYPE **,bool,bool*);
short parse_modifiers (CHARTYPE *,COLOUR_ATTR *);
TheDriverAttr merge_curline_colour (COLOUR_ATTR *, COLOUR_ATTR *);
void set_up_default_colours (FILE_DETAILS *,COLOUR_ATTR *,int);
void set_up_default_ecolours (FILE_DETAILS *);
CHARTYPE *get_colour_strings (COLOUR_ATTR *);
int is_valid_colour ( CHARTYPE *colour );
                                                           /* column.c */
short column_command (CHARTYPE *,int);
                                                           /* mouse.c */
short THEMouse (CHARTYPE *);
void which_window_is_mouse_in (CHARTYPE *,int *);
void reset_saved_mouse_pos (void);
void get_saved_mouse_pos (int *, int *);
int get_saved_mouse_target (struct TheInputLogicalTarget *);
int read_pending_mouse_definition_key (int *);
int read_transient_current_role_mouse_event (short, TheDriverMouseEvent *);
int read_transient_window_mouse_event (struct TheDriverWindow *,
                                       TheDriverMouseEvent *);
void initialise_mouse_commands (void);
int mouse_info_to_key (int,int,int,int);
void mouse_trace_message (const char *, const char *, ...);
CHARTYPE *mouse_key_number_to_name (int,CHARTYPE *,int *);
int find_mouse_key_value (CHARTYPE *);
int find_mouse_key_value_in_window (CHARTYPE *,CHARTYPE *);
short ScrollbarHorz (CHARTYPE *);
short ScrollbarVert (CHARTYPE *);
                                                           /* memory.c */
void init_memory_table ( void );
void free_memory_flists ( void );
void *get_a_block ( size_t );
void give_a_block ( void * );
void *resize_a_block ( void *, size_t );
void the_free_flists  ( void );
                                                           /* single.c */
int initialise_fifo ( LINE *first_file_name, LINETYPE startup_line, LENGTHTYPE startup_column, bool ro );
int process_fifo_input ( int key );
void close_fifo ( void );
                                                            /* comm*.c */
short Add (CHARTYPE *);
short Alert (CHARTYPE *);
short All (CHARTYPE *);
short Alt (CHARTYPE *);
short Arbchar (CHARTYPE *);
short Autocolour (CHARTYPE *);
short Autosave (CHARTYPE *);
short Autoscroll (CHARTYPE *);
short Backup (CHARTYPE *);
short Backward (CHARTYPE *);
short BeepSound (CHARTYPE *);
short Bottom (CHARTYPE *);
short Boundmark (CHARTYPE *);
short Cappend (CHARTYPE *);
short Cancel (CHARTYPE *);
short Case (CHARTYPE *);
short Ccancel (CHARTYPE *);
short Cdelete (CHARTYPE *);
short Cfirst (CHARTYPE *);
short Change (CHARTYPE *);
short Cinsert (CHARTYPE *);
short Clast (CHARTYPE *);
short THEClipboard (CHARTYPE *);
short Clearerrorkey (CHARTYPE *);
short Clearscreen (CHARTYPE *);
short Clocate (CHARTYPE *);
short Clock (CHARTYPE *);
short Cmatch (CHARTYPE *);
short Cmdarrows (CHARTYPE *);
short Cmdline (CHARTYPE *);
short Cmsg (CHARTYPE *);
short Colour (CHARTYPE *);
short Colouring (CHARTYPE *);
short Compat (CHARTYPE *);
short Compress (CHARTYPE *);
short THECommand (CHARTYPE *);
short ControlChar (CHARTYPE *);
short Copy (CHARTYPE *);
short Coverlay (CHARTYPE *);
short Creplace (CHARTYPE *);
short Ctlchar (CHARTYPE *);
short Curline (CHARTYPE *);
short Cursor (CHARTYPE *);
short CursorStay (CHARTYPE *);
short Cursorstyle (CHARTYPE *);
short Define (CHARTYPE *);
short Defsort (CHARTYPE *);
short DeleteLine (CHARTYPE *);
short Dialog (CHARTYPE *);
short Directory (CHARTYPE *);
short Dirinclude (CHARTYPE *);
short Display (CHARTYPE *);
short Duplicate (CHARTYPE *);
short Ecolour (CHARTYPE *);
short Emsg (CHARTYPE *);
short THEEditv (CHARTYPE *);
short Enter (CHARTYPE *);
short Eolout (CHARTYPE *);
short Equivchar (CHARTYPE *);
short Errorformat (CHARTYPE *);
short Erroroutput (CHARTYPE *);
short Etmode (CHARTYPE *);
short Expand (CHARTYPE *);
short Extract (CHARTYPE *);
short Ffile (CHARTYPE *);
short File (CHARTYPE *);
short Filectlchar (CHARTYPE *);
short THEFiletabs (CHARTYPE *);
short Fillbox (CHARTYPE *);
short Find (CHARTYPE *);
short Findup (CHARTYPE *);
short Fext (CHARTYPE *);
short Fdisplay (CHARTYPE *);
short Filename (CHARTYPE *);
short Fmode (CHARTYPE *);
short Fname (CHARTYPE *);
short Forward (CHARTYPE *);
short Fpath (CHARTYPE *);
short Fullfname (CHARTYPE *);
short Get (CHARTYPE *);
short THEHeader (CHARTYPE *);
short Help (CHARTYPE *);
short Hex (CHARTYPE *);
short Hexdisplay (CHARTYPE *);
short Hexshow (CHARTYPE *);
short Highlight (CHARTYPE *);
short Hit (CHARTYPE *);
short Idline (CHARTYPE *);
short Impmacro (CHARTYPE *);
short Impos (CHARTYPE *);
short Input (CHARTYPE *);
short Inputstem (CHARTYPE *);
short Inputmode (CHARTYPE *);
short Insertmode (CHARTYPE *);
short THEInterface (CHARTYPE *);
short Join (CHARTYPE *);
short Lastop (CHARTYPE *);
short Left (CHARTYPE *);
short Lineflag (CHARTYPE *);
short Linend (CHARTYPE *);
short Locate (CHARTYPE *);
short Lowercase (CHARTYPE *);
short Macro (CHARTYPE *);
short SetMacro (CHARTYPE *);
short Macroext (CHARTYPE *);
short Macropath (CHARTYPE *);
short Margins (CHARTYPE *);
short Mark (CHARTYPE *);
short Modify (CHARTYPE *);
short Mouse (CHARTYPE *);
short Mouseclick (CHARTYPE *);
short THEMove (CHARTYPE *);
short Msg (CHARTYPE *);
short Msgline (CHARTYPE *);
short Msgmode (CHARTYPE *);
short Newlines (CHARTYPE *);
short THENext (CHARTYPE *);
short Nextwindow (CHARTYPE *);
short Nfind (CHARTYPE *);
short Nfindup (CHARTYPE *);
short Nomsg (CHARTYPE *);
short Nondisp (CHARTYPE *);
short Nop (CHARTYPE *);
short Number (CHARTYPE *);
short Overlaybox (CHARTYPE *);
short Os (CHARTYPE *);
short Osnowait (CHARTYPE *);
short Osquiet (CHARTYPE *);
short Osredir (CHARTYPE *);
short Pagewrap (CHARTYPE *);
short Parser (CHARTYPE *);
short Pending (CHARTYPE *);
short Point (CHARTYPE *);
short Popup (CHARTYPE *);
short Position (CHARTYPE *);
short Prefix (CHARTYPE *);
short Preserve (CHARTYPE *);
short Prevwindow (CHARTYPE *);
short Print (CHARTYPE *);
short Pscreen (CHARTYPE *);
short THEPrinter (CHARTYPE *);
short Put (CHARTYPE *);
short Putd (CHARTYPE *);
short Qquit (CHARTYPE *);
short Quit (CHARTYPE *);
short Query (CHARTYPE *);
short THEReadonly (CHARTYPE *);
short Readv (CHARTYPE *);
short THERecord (CHARTYPE *);
short Recover (CHARTYPE *);
short Reexecute (CHARTYPE *);
short Redit (CHARTYPE *);
short Redraw (CHARTYPE *);
short THERefresh (CHARTYPE *);
short Regexp (CHARTYPE *);
short Repeat (CHARTYPE *);
short Replace (CHARTYPE *);
short Reprofile (CHARTYPE *);
short Reserved (CHARTYPE *);
short Reset (CHARTYPE *);
short Restore (CHARTYPE *);
short Retrieve (CHARTYPE *);
short Rexxhalt (CHARTYPE *);
short Rexxoutput (CHARTYPE *);
short THERexx (CHARTYPE *);
short Rgtleft (CHARTYPE *);
short Right (CHARTYPE *);
short Save (CHARTYPE *);
short Scope (CHARTYPE *);
short Scale (CHARTYPE *);
short THESearch (CHARTYPE *);
short Sdslh (CHARTYPE *);
short Sdslhwait (CHARTYPE *);
short Select (CHARTYPE *);
short Set (CHARTYPE *);
short Schange (CHARTYPE *);
short Slk (CHARTYPE *);
short THEScreen (CHARTYPE *);
short Shadow (CHARTYPE *);
short Shift (CHARTYPE *);
short ShowKey (CHARTYPE *);
short Sort (CHARTYPE *);
short Sos (CHARTYPE *);
short Sos_addline (CHARTYPE *);
short Sos_blockend (CHARTYPE *);
short Sos_blockstart (CHARTYPE *);
short Sos_bottomedge (CHARTYPE *);
short Sos_cuadelback (CHARTYPE *);
short Sos_cuadelchar (CHARTYPE *);
short Sos_current (CHARTYPE *);
short do_Sos_current ( CHARTYPE *, CHARTYPE, VIEW_DETAILS * );
short Sos_cursoradj (CHARTYPE *);
short Sos_cursorshift (CHARTYPE *);
short Sos_delback (CHARTYPE *);
short Sos_delchar (CHARTYPE *);
short Sos_delend (CHARTYPE *);
short Sos_delline (CHARTYPE *);
short Sos_delword (CHARTYPE *);
short Sos_doprefix (CHARTYPE *);
short Sos_edit (CHARTYPE *);
short Sos_endchar (CHARTYPE *);
short Sos_execute (CHARTYPE *);
short Sos_firstchar (CHARTYPE *);
short Sos_firstcol (CHARTYPE *);
short Sos_instab (CHARTYPE *);
short Sos_lastcol (CHARTYPE *);
short Sos_leftedge (CHARTYPE *);
short Sos_makecurr (CHARTYPE *);
short Sos_marginl (CHARTYPE *);
short Sos_marginr (CHARTYPE *);
short Sos_pastecmdline (CHARTYPE *);
short Sos_parindent (CHARTYPE *);
short Sos_prefix (CHARTYPE *);
short do_Sos_prefix ( CHARTYPE *, CHARTYPE, VIEW_DETAILS * );
short Sos_qcmnd (CHARTYPE *);
short Sos_rightedge (CHARTYPE *);
short Sos_settab (CHARTYPE *);
short Sos_startendchar (CHARTYPE *);
short Sos_tabb (CHARTYPE *);
short Sos_tabf (CHARTYPE *);
short Sos_tabfieldb (CHARTYPE *);
short Sos_tabfieldf (CHARTYPE *);
short Sos_tabwordb (CHARTYPE *);
short Sos_tabwordf (CHARTYPE *);
short Sos_topedge (CHARTYPE *);
short Sos_undo (CHARTYPE *);
#ifdef USE_SDSLH
short Sos_toggle_fold (CHARTYPE *);
#endif
short Span (CHARTYPE *);
short Spill (CHARTYPE *);
short Split (CHARTYPE *);
short Spltjoin (CHARTYPE *);
short Ssave (CHARTYPE *);
short Statopt (CHARTYPE *);
short Status (CHARTYPE *);
short Statusline (CHARTYPE *);
short Stay (CHARTYPE *);
short Suspend (CHARTYPE *);
short Synonym (CHARTYPE *);
short Tabfile (CHARTYPE *);
short Tabkey (CHARTYPE *);
short Tabline (CHARTYPE *);
short Tabpre (CHARTYPE *);
short Tabs (CHARTYPE *);
short Tabsin (CHARTYPE *);
short Tabsout (CHARTYPE *);
short Tag (CHARTYPE *);
short Targetsave (CHARTYPE *);
short Text (CHARTYPE *);
short THighlight (CHARTYPE *);
short Timecheck (CHARTYPE *);
short Toascii (CHARTYPE *);
short Tofeof (CHARTYPE *);
short Top (CHARTYPE *);
short Trailing (CHARTYPE *);
short Trunc (CHARTYPE *);
short THETypeahead (CHARTYPE *);
short Undoing (CHARTYPE *);
short Untaa (CHARTYPE *);
short Up (CHARTYPE *);
short Uppercase (CHARTYPE *);
short Utf (CHARTYPE *);
short Validtarget (CHARTYPE *);
short Verify (CHARTYPE *);
short Width (CHARTYPE *);
short Word (CHARTYPE *);
short Wordwrap (CHARTYPE *);
short Wrap (CHARTYPE *);
short Xedit (CHARTYPE *);
short Xterminal (CHARTYPE *);
short Zone (CHARTYPE *);
