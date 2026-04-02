/* config.h.  Generated from config.h.in by configure.  */
/* config.h.in.  Generated automatically from configure.in by autoheader.  */

/* Define this always; build number.  */
/* When next build requires configure to be run again, increment this */
/* Format is xxyy where xx is version, yy build number */
#define BUILD3001 1

/* Define value of BSD_STANDOUT */
/* #undef BSD_STANDOUT */

/* Define to int if no chtype defined in curses header.  */
#ifndef chtype
/* #undef chtype */
#endif

/* Define to empty if the keyword does not work.  */
/* #undef const */

/* Define if curses library has this function */
#define HAVE_BEEP 1

/* Define if curses library has this function */
#define HAVE_BOX 1

/* Define if curses library has this function */
#define HAVE_WHLINE 1

/* Define if the SVR4 curses is broken */
/* #undef HAVE_BROKEN_SYSVR4_CURSES */

/* Define if linking with BSD curses variant */
/* #undef HAVE_BSD_CURSES */

/* Define if curses library has this function */
#define HAVE_CBREAK 1

/* Define if curses library has this function */
#define HAVE_CURS_SET 1

/* Define if curses library has this function */
#define HAVE_DOUPDATE 1

/* Define if curses library has this function */
#define HAVE_DERWIN 1

/* Define if curses library has this function */
#define HAVE_KEYPAD 1

/* Define if curses library has this function */
#define HAVE_NEWPAD 1

/* Define if curses library has this function */
#define HAVE_NOCBREAK 1

/* Define if curses library has this function */
#define HAVE_NODELAY 1

/* Define if curses library has this function */
#define HAVE_NOTIMEOUT 1

/* Define if curses library has this function */
#define HAVE_PREFRESH 1

/* Define if curses library has this function */
#define HAVE_UNGETCH 1

/* Define if your compiler supports ANSI prototypes  */
#define HAVE_PROTO 1

/* Define if curses library has this function */
#define HAVE_RAW 1

/* Define if curses library has this function */
#define HAVE_RESET_PROG_MODE 1

/* Define if curses library has this function */
#define HAVE_RESET_SHELL_MODE 1

/* Define if curses library has this function */
#define HAVE_RESIZE_TERM 1

/* Define if curses library has this function */
#define HAVE_RESIZETERM 1

/* Define if curses library has this function */
#define HAVE_SLK_INIT 1

/* Define if curses library has this function */
#define HAVE_SLK_ATTRSET 1

/* Define if curses library has this function */
/* #undef HAVE_SB_INIT */

/* Define if curses library has this function */
#define HAVE_TOUCHLINE 1

/* Define if curses library has this function */
#define HAVE_TYPEAHEAD 1

/* Define if curses library has this  */
#define HAVE_WATTRSET 1

/* Define if curses library has this  */
#define HAVE_WADDCHNSTR 1

/* Define if curses library has this  */
#define HAVE_WBKGD 1

/* Define if curses library has this function */
#define HAVE_WNOUTREFRESH 1

/* Define if curses library has this function */
#define HAVE_WVLINE 1

/* Define if not linking with REXX support  */
/* #undef NOREXX */

/* Define if you need to in order for stat and other things to work.  */
/* #undef _POSIX_SOURCE */

/* Define as the return type of signal handlers (int or void).  */
#define RETSIGTYPE void

/* Define if __sighandler_t is defined.  */
/* #undef HAVE__SIGHANDLER_T */

/* Define to `unsigned' if <sys/types.h> doesn't define.  */
/* #undef size_t */

/* Define if you have the ANSI C header files.  */
#define STDC_HEADERS 1

/* Define if you can safely include both <sys/time.h> and <time.h>.  */
#define TIME_WITH_SYS_TIME 1

/* Define if your <sys/time.h> declares struct tm.  */
/* #undef TM_IN_SYS_TIME */

/* Define if compiling with TRACE information */
/* #undef THE_TRACE */

/* Define if running on UNIX system.  */
#define UNIX 1

/* Define if linking with Extended Curses.  */
/* #undef USE_EXTCURSES */

/* Define if linking with ncurses.  */
#define USE_NCURSES 1

/* Define if linking with ncursesw.  */
/* #undef USE_NCURSESW */

/* Define if linking with Regina REXX.  */
#define USE_REGINA 1

/* Define if linking with Rexx/Trans.  */
/* #undef USE_REXXTRANS */

/* Define if linking with REXX/6000.  */
/* #undef USE_REXX6000 */

/* Define if linking with REXX/imc.  */
/* #undef USE_REXXIMC */

/* Define if linking with Object Rexx.  */
/* #undef USE_OREXX */

/* Define if linking with Open Object Rexx 4.0.  */
/* #undef OOREXX_40 */

/* Define if linking with Open Object Rexx.  */
/* #undef USE_OOREXX */

/* Define if linking with uni-REXX.  */
/* #undef USE_UNIREXX */

/* Define if linking with dwindows.  */
/* #undef USE_DWINDOWS */

/* Define if linking with XCurses.  */
/* #undef USE_XCURSES */

/* Define if linking with PDCurses built with SDL.  */
/* #undef USE_SDLCURSES */

/* Define if linking with PDCurses built with SDL.  */
/* #undef USE_VTCURSES */

/* Define if linking with Curses include dir specified --with-cursesincdir.  */
/* #undef LOCAL_CURSES */

/* Define if linking with PDCurses.  */
/* #undef USE_PDCURSES */

/* Define a value for the xterm program */
/* #undef XTERM_PROGRAM */

/* Define if you have this function */
/* #undef HAVE_ACL_GET */

/* Define if you have this function */
#define HAVE_FORK 1

/* Define if you have this function */
/* #undef HAVE_GETACL */

/* Define if you have this function */
#define HAVE_REALPATH 1

/* Define if you have this function */
#define HAVE_SETLOCALE 1

/* Define if you have the sigaction function.  */
#define HAVE_SIGACTION 1

/* Define if you have the siginterrupt function.  */
#define HAVE_SIGINTERRUPT 1

/* Define if you have the strcasecmp function.  */
#define HAVE_STRCASECMP 1

/* Define if you have the stricmp function.  */
/* #undef HAVE_STRICMP */

/* Define if you have the strcmpi function.  */
/* #undef HAVE_STRCMPI */

/* Define if you have the readlink function.  */
#define HAVE_READLINK 1

/* Define if you have the select function.  */
#define HAVE_SELECT 1

/* Define if you have the mkfifo function.  */
#define HAVE_MKFIFO 1

/* Define if you have the rename function.  */
#define HAVE_RENAME 1

/* Define if you have the lstat function.  */
#define HAVE_LSTAT 1

/* Define if you have the chown function.  */
#define HAVE_CHOWN 1

/* Define if you have the mkstemp function.  */
#define HAVE_MKSTEMP 1

/* Define if you have the <ctype.h> header file.  */
#define HAVE_CTYPE_H 1

/* Define if you have the <wctype.h> header file.  */
#define HAVE_WCTYPE_H 1

/* Define if you have the <errno.h> header file.  */
#define HAVE_ERRNO_H 1

/* Define if you have the <alloca.h> header file.  */
#define HAVE_ALLOCA_H 1

/* Define if you have the <dirent.h> header file.  */
#define HAVE_DIRENT_H 1

/* Define if you have the <fcntl.h> header file.  */
#define HAVE_FCNTL_H 1

/* Define if you have the <locale.h> header file.  */
#define HAVE_LOCALE_H 1

/* Define if you have the <memory.h> header file.  */
#define HAVE_MEMORY_H 1

/* Define if you have the <stat.h> header file.  */
/* #undef HAVE_STAT_H */

/* Define if you have the <stdarg.h> header file.  */
#define HAVE_STDARG_H 1

/* Define if you have the <stdlib.h> header file.  */
#define HAVE_STDLIB_H 1

/* Define if you have the <stdint.h> header file.  */
#define HAVE_STDINT_H 1

/* Define if you have the <inttypes.h> header file.  */
#define HAVE_INTTYPES_H 1

/* Define if you have the <string.h> header file.  */
#define HAVE_STRING_H 1

/* Define if you have the <time.h> header file.  */
#define HAVE_TIME_H 1

/* Define if you have the <sys/acl.h> header file.  */
#define HAVE_SYS_ACL_H 1

/* Define if you have the <sys/mode.h> header file.  */
/* #undef HAVE_SYS_MODE_H */

/* Define if you have the <sys/file.h> header file.  */
#define HAVE_SYS_FILE_H 1

/* Define if you have the <sys/stat.h> header file.  */
#define HAVE_SYS_STAT_H 1

/* Define if you have the <sys/socket.h> header file.  */
#define HAVE_SYS_SOCKET_H 1

/* Define if you have the <sys/time.h> header file.  */
#define HAVE_SYS_TIME_H 1

/* Define if you have the <sys/types.h> header file.  */
#define HAVE_SYS_TYPES_H 1

/* Define if you have the <sys/wait.h> header file.  */
#define HAVE_SYS_WAIT_H 1

/* Define if you have the <sys/select.h> header file.  */
#define HAVE_SYS_SELECT_H 1

/* Define if you have the <select.h> header file.  */
/* #undef HAVE_SELECT_H */

/* Define if select() is in <time.h> header file.  */
/* #undef HAVE_SELECT_IN_TIME_H */

/* Define if you have the <unistd.h> header file.  */
#define HAVE_UNISTD_H 1

/* Define if are on Digital Unix 4.0.  */
/* #undef HAVE_BROKEN_SYS_ACL_H */

/* Define if WINDOW->_begy exists */
/* #undef HAVE_UNDERSCORE_BEGY */

/* Define if WINDOW->begy exists */
/* #undef HAVE_BEGY */

/* Define if WINDOW->_maxy exists */
/* #undef HAVE_UNDERSCORE_MAXY */

/* Define if WINDOW->maxy exists */
/* #undef HAVE_MAXY */

/* Define if g++ can't compile <string.h> */
/* #undef HAVE_BROKEN_CXX_WITH_STRING_H */

/* Define if you have curses_version() */
#define HAVE_CURSES_VERSION 1

/* Defines the kernel name */
#define MH_KERNEL_NAME "Darwin"

/* Defines a global profile name */
/* #undef THE_GLOBAL_PROFILE */

/* Defines home directory */
/* #undef THE_HOME_DIR */

/* define if you have PDC_set_function_key */
/* #undef HAVE_PDC_SET_FUNCTION_KEY */

/* the following are added so that embedded PDCursesMod will be configured correctly */

/* Define if you have the <DECkeySym.h> header file */
/* #undef HAVE_DECKEYSYM_H */

/* Define to 1 if you have the `poll' function. */
#define HAVE_POLL 1

/* Define if you have the <Sunkeysym.h> header file */
/* #undef HAVE_SUNKEYSYM_H */

/* Define if you have the <XF86keysym.h> header file */
/* #undef HAVE_XF86KEYSYM_H */

/* Define to 1 if you have the `usleep' function. */
#define HAVE_USLEEP 1

/* Define to 1 if you have the `snprintf' function. */
#define HAVE_SNPRINTF 1

/* Define to 1 if you have the `vsnprintf' function. */
#define HAVE_VSNPRINTF 1

/* Define to 1 if you have the `vsscanf' function. */
#define HAVE_VSSCANF 1

/* Define if you have the <xpm.h> header file */
/* #undef HAVE_XPM_H */

/* Define if you want to use neXtaw library */
/* #undef USE_NEXTAW */

/* Define if you want to use Xaw3d library */
/* #undef USE_XAW3D */

/* Define XPointer is typedefed in X11/Xlib.h */
/* #undef XPOINTER_TYPEDEFED */
