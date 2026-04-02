dnl
dnl include our own common routines
dnl
sinclude(common/accommon.m4)dnl

dnl ---------------------------------------------------------------------------
dnl Determine if curses library supports various functions
dnl ---------------------------------------------------------------------------
AC_DEFUN([MH_CURSES_FUNCS],
[
dnl
dnl Ensure that when compiling with XCurses, we have set the
dnl required extern for program name.
dnl
cat > xxxxx.h <<EOF
#ifdef XCURSES
   char *XCursesProgramName="test";
#endif
EOF
dnl
dnl Include before curses.h any include files required. This is
dnl often stdarg.h
dnl
for incfile in $mh_pre_curses_h_include ; do
   echo "#include <$incfile.h>" >> xxxxx.h
done

mh_save_libs="$LIBS"
mh_save_cflags="$CFLAGS"

if test "$with_curses" = "pdcurses-x11" -o "$with_curses" = "pdcurses-x11w" -o "$with_curses" = "pdcurses-x11w8"; then
   CFLAGS="-DXCURSES $MH_CURSES_INC $SYS_DEFS"
   LIBS="$LIBS $MH_CURSES_LIB $MH_XLIBS $MH_EXTRA_LIBS"
   have_sb=yes
else
   CFLAGS="$MH_CURSES_INC $SYS_DEFS"
   LIBS="$LIBS $MH_CURSES_LIB $MH_EXTRA_LIBS"
   have_sb=no
fi

AC_MSG_CHECKING(for initscr in $curses_l library)
mh_cv_func_initscr=$1
if test $1 = no; then
   AC_TRY_LINK(
[#include "xxxxx.h"]
[#include <$curses_h>],
[initscr()],
   [mh_cv_func_initscr=yes],
   [mh_cv_func_initscr=no]
   )
fi
if test "$mh_cv_func_initscr" = no ; then
   AC_MSG_ERROR(Unable to find suitable curses library; THE cannot be configured)
else
   AC_MSG_RESULT($mh_cv_func_initscr)
fi


AC_MSG_CHECKING(for System V curses)
AC_CACHE_VAL(
[mh_cv_sysv_curses],
[
AC_TRY_COMPILE(
[#include "xxxxx.h"]
[#include <$curses_h>],
[long xxx=(long)A_NORMAL],
[mh_cv_sysv_curses=yes],
[mh_cv_sysv_curses=no]
)
])dnl
AC_MSG_RESULT($mh_cv_sysv_curses)
if test "$mh_cv_sysv_curses" = no ; then
   AC_DEFINE(HAVE_BSD_CURSES)
fi

if test "$mh_cv_sysv_curses" = no ; then
   if test "$with_extcurses" = no ; then
      MH_CURSES_LIB="$MH_CURSES_LIB -ltermcap"
      LIBS="$LIBS -ltermcap"
   fi
fi

dnl
dnl If HAVE_BSD_CURSES, define BSD_STANDOUT as one of:
dnl _STANDOUT, __WSTANDOUT or __STANDOUT
dnl
if test "$mh_cv_sysv_curses" = no ; then
   bsd_standouts="_STANDOUT __WSTANDOUT __STANDOUT"
   for sout in $bsd_standouts ; do
AC_TRY_COMPILE(
[#include "xxxxx.h"]
[#include <$curses_h>],
[int xxx=$sout],
   [mh_bsd_sout=yes],
   [mh_bsd_sout=no]
   )
      if test "$mh_bsd_sout" = yes ; then
         AC_DEFINE_UNQUOTED(BSD_STANDOUT,$sout)
         break 2
      fi
   done
fi

AC_MSG_CHECKING(for wattrset in $curses_l library)
mh_cv_func_wattrset=$1
if test $1 = no; then
   AC_TRY_LINK(
[#include "xxxxx.h"]
[#include <$curses_h>],
[wattrset(stdscr,0)],
   [mh_cv_func_wattrset=yes],
   [mh_cv_func_wattrset=no]
   )
fi
AC_MSG_RESULT($mh_cv_func_wattrset)
if test "$mh_cv_func_wattrset" = yes ; then
   AC_DEFINE(HAVE_WATTRSET)
fi

AC_MSG_CHECKING(for keypad in $curses_l library)
mh_cv_func_keypad=$1
if test $1 = no; then
   AC_TRY_LINK(
[#include "xxxxx.h"]
[#include <$curses_h>],
[keypad(stdscr,TRUE)],
   [mh_cv_func_keypad=yes],
   [mh_cv_func_keypad=no]
   )
fi
AC_MSG_RESULT($mh_cv_func_keypad)
if test "$mh_cv_func_keypad" = yes ; then
   AC_DEFINE(HAVE_KEYPAD)
fi

AC_MSG_CHECKING(for derwin in $curses_l library)
mh_cv_func_derwin=$1
if test $1 = no; then
   AC_TRY_LINK(
[#include "xxxxx.h"]
[#include <$curses_h>],
[WINDOW *w = derwin(stdscr,1,1,1,1)],
   [mh_cv_func_derwin=yes],
   [mh_cv_func_derwin=no]
   )
fi
AC_MSG_RESULT($mh_cv_func_derwin)
if test "$mh_cv_func_derwin" = yes ; then
   AC_DEFINE(HAVE_DERWIN)
fi

AC_MSG_CHECKING(for newpad in $curses_l library)
mh_cv_func_newpad=$1
if test $1 = no; then
   AC_TRY_LINK(
[#include "xxxxx.h"]
[#include <$curses_h>],
[WINDOW *w = newpad(1,1)],
   [mh_cv_func_newpad=yes],
   [mh_cv_func_newpad=no]
   )
fi
AC_MSG_RESULT($mh_cv_func_newpad)
if test "$mh_cv_func_newpad" = yes ; then
   AC_DEFINE(HAVE_NEWPAD)
fi

AC_MSG_CHECKING(for prefresh in $curses_l library)
mh_cv_func_prefresh=$1
if test $1 = no; then
   AC_TRY_LINK(
[#include "xxxxx.h"]
[#include <$curses_h>],
[WINDOW *w; prefresh(w,1,1,1,1,1,1)],
   [mh_cv_func_prefresh=yes],
   [mh_cv_func_prefresh=no]
   )
fi
AC_MSG_RESULT($mh_cv_func_prefresh)
if test "$mh_cv_func_prefresh" = yes ; then
   AC_DEFINE(HAVE_PREFRESH)
fi

AC_MSG_CHECKING(for beep in $curses_l library)
mh_cv_func_beep=$1
if test $1 = no; then
   AC_TRY_LINK(
[#include "xxxxx.h"]
[#include <$curses_h>],
[beep()],
   [mh_cv_func_beep=yes],
   [mh_cv_func_beep=no]
   )
fi
AC_MSG_RESULT($mh_cv_func_beep)
if test "$mh_cv_func_beep" = yes ; then
   AC_DEFINE(HAVE_BEEP)
fi

AC_MSG_CHECKING(for box in $curses_l library)
mh_cv_func_box=$1
if test $1 = no; then
   AC_TRY_LINK(
[#include "xxxxx.h"]
[#include <$curses_h>],
[box(stdscr,0,0)],
   [mh_cv_func_box=yes],
   [mh_cv_func_box=no]
   )
fi
AC_MSG_RESULT($mh_cv_func_box)
if test "$mh_cv_func_box" = yes ; then
   AC_DEFINE(HAVE_BOX)
fi

AC_MSG_CHECKING(for whline in $curses_l library)
mh_cv_func_whline=$1
if test $1 = no; then
   AC_TRY_LINK(
[#include "xxxxx.h"]
[#include <$curses_h>],
[whline(stdscr,0,1)],
   [mh_cv_func_whline=yes],
   [mh_cv_func_whline=no]
   )
fi
AC_MSG_RESULT($mh_cv_func_whline)
if test "$mh_cv_func_whline" = yes ; then
   AC_DEFINE(HAVE_WHLINE)
fi

AC_MSG_CHECKING(for curs_set in $curses_l library)
mh_cv_func_curs_set=$1
if test $1 = no; then
   AC_TRY_LINK(
[#include "xxxxx.h"]
[#include <$curses_h>],
[curs_set(0)],
   [mh_cv_func_curs_set=yes],
   [mh_cv_func_curs_set=no]
   )
fi
AC_MSG_RESULT($mh_cv_func_curs_set)
if test "$mh_cv_func_curs_set" = yes ; then
   AC_DEFINE(HAVE_CURS_SET)
fi

AC_MSG_CHECKING(for touchline in $curses_l library)
mh_cv_func_touchline=$1
if test $1 = no; then
   AC_TRY_LINK(
[#include "xxxxx.h"]
[#include <$curses_h>],
[touchline(stdscr,1,1)],
   [mh_cv_func_touchline=yes],
   [mh_cv_func_touchline=no]
   )
fi
AC_MSG_RESULT($mh_cv_func_touchline)
if test "$mh_cv_func_touchline" = yes ; then
   AC_DEFINE(HAVE_TOUCHLINE)
fi

AC_MSG_CHECKING(for typeahead in $curses_l library)
mh_cv_func_typeahead=$1
if test $1 = no; then
   AC_TRY_LINK(
[#include "xxxxx.h"]
[#include <$curses_h>],
[typeahead(1)],
   [mh_cv_func_typeahead=yes],
   [mh_cv_func_typeahead=no]
   )
fi
AC_MSG_RESULT($mh_cv_func_typeahead)
if test "$mh_cv_func_typeahead" = yes ; then
   AC_DEFINE(HAVE_TYPEAHEAD)
fi

AC_MSG_CHECKING(for notimeout in $curses_l library)
mh_cv_func_notimeout=$1
if test $1 = no; then
   AC_TRY_LINK(
[#include "xxxxx.h"]
[#include <$curses_h>],
[notimeout(stdscr,TRUE)],
   [mh_cv_func_notimeout=yes],
   [mh_cv_func_notimeout=no]
   )
fi
AC_MSG_RESULT($mh_cv_func_notimeout)
if test "$mh_cv_func_notimeout" = yes ; then
   AC_DEFINE(HAVE_NOTIMEOUT)
fi

AC_MSG_CHECKING(for ungetch in $curses_l library)
mh_cv_func_ungetch=$1
if test $1 = no; then
   AC_TRY_LINK(
[#include "xxxxx.h"]
[#include <$curses_h>],
[ungetch(10)],
   [mh_cv_func_ungetch=yes],
   [mh_cv_func_ungetch=no]
   )
fi
AC_MSG_RESULT($mh_cv_func_ungetch)
if test "$mh_cv_func_ungetch" = yes ; then
   AC_DEFINE(HAVE_UNGETCH)
fi

AC_MSG_CHECKING(for nodelay in $curses_l library)
mh_cv_func_nodelay=$1
if test $1 = no; then
   AC_TRY_LINK(
[#include "xxxxx.h"]
[#include <$curses_h>],
[nodelay(stdscr,TRUE)],
   [mh_cv_func_nodelay=yes],
   [mh_cv_func_nodelay=no]
   )
fi
AC_MSG_RESULT($mh_cv_func_nodelay)
if test "$mh_cv_func_nodelay" = yes ; then
   AC_DEFINE(HAVE_NODELAY)
fi

AC_MSG_CHECKING(for raw in $curses_l library)
mh_cv_func_raw=$1
if test $1 = no; then
   AC_TRY_LINK(
[#include "xxxxx.h"]
[#include <$curses_h>],
[raw()],
   [mh_cv_func_raw=yes],
   [mh_cv_func_raw=no]
   )
fi
AC_MSG_RESULT($mh_cv_func_raw)
if test "$mh_cv_func_raw" = yes ; then
   AC_DEFINE(HAVE_RAW)
fi

AC_MSG_CHECKING(for cbreak in $curses_l library)
mh_cv_func_cbreak=$1
if test $1 = no; then
   AC_TRY_LINK(
[#include "xxxxx.h"]
[#include <$curses_h>],
[cbreak()],
   [mh_cv_func_cbreak=yes],
   [mh_cv_func_cbreak=no]
   )
fi
AC_MSG_RESULT($mh_cv_func_cbreak)
if test "$mh_cv_func_cbreak" = yes ; then
   AC_DEFINE(HAVE_CBREAK)
fi

AC_MSG_CHECKING(for nocbreak in $curses_l library)
mh_cv_func_nocbreak=$1
if test $1 = no; then
   AC_TRY_LINK(
[#include "xxxxx.h"]
[#include <$curses_h>],
[nocbreak()],
   [mh_cv_func_nocbreak=yes],
   [mh_cv_func_nocbreak=no]
   )
fi
AC_MSG_RESULT($mh_cv_func_nocbreak)
if test "$mh_cv_func_nocbreak" = yes ; then
   AC_DEFINE(HAVE_NOCBREAK)
fi

AC_MSG_CHECKING(for waddchnstr in $curses_l library)
mh_cv_func_waddchnstr=$1
if test $1 = no; then
   AC_TRY_LINK(
[#include "xxxxx.h"]
[#include <$curses_h>],
[waddchnstr(stdscr,(chtype*)"A",1)],
   [mh_cv_func_waddchnstr=yes],
   [mh_cv_func_waddchnstr=no]
   )
fi
AC_MSG_RESULT($mh_cv_func_waddchnstr)
if test "$mh_cv_func_waddchnstr" = yes ; then
   AC_DEFINE(HAVE_WADDCHNSTR)
fi

AC_MSG_CHECKING(for wbkgd in $curses_l library)
mh_cv_func_wbkgd=$1
if test $1 = no; then
   AC_TRY_LINK(
[#include "xxxxx.h"]
[#include <$curses_h>],
[wbkgd(stdscr,0)],
   [mh_cv_func_wbkgd=yes],
   [mh_cv_func_wbkgd=no]
   )
fi
AC_MSG_RESULT($mh_cv_func_wbkgd)
if test "$mh_cv_func_wbkgd" = yes ; then
   AC_DEFINE(HAVE_WBKGD)
fi


AC_MSG_CHECKING(for wnoutrefresh in $curses_l library)
mh_cv_func_wnoutrefresh=$1
if test $1 = no; then
   AC_TRY_LINK(
[#include "xxxxx.h"]
[#include <$curses_h>],
[wnoutrefresh(stdscr)],
   [mh_cv_func_wnoutrefresh=yes],
   [mh_cv_func_wnoutrefresh=no]
   )
fi
AC_MSG_RESULT($mh_cv_func_wnoutrefresh)
if test "$mh_cv_func_wnoutrefresh" = yes ; then
   AC_DEFINE(HAVE_WNOUTREFRESH)
fi

AC_MSG_CHECKING(for doupdate in $curses_l library)
mh_cv_func_doupdate=$1
if test $1 = no; then
   AC_TRY_LINK(
[#include "xxxxx.h"]
[#include <$curses_h>],
[doupdate()],
   [mh_cv_func_doupdate=yes],
   [mh_cv_func_doupdate=no]
   )
fi
AC_MSG_RESULT($mh_cv_func_doupdate)
if test "$mh_cv_func_doupdate" = yes ; then
   AC_DEFINE(HAVE_DOUPDATE)
fi

AC_MSG_CHECKING(for reset_shell_mode in $curses_l library)
mh_cv_func_reset_shell_mode=$1
if test $1 = no; then
   AC_TRY_LINK(
[#include "xxxxx.h"]
[#include <$curses_h>],
[reset_shell_mode()],
   [mh_cv_func_reset_shell_mode=yes],
   [mh_cv_func_reset_shell_mode=no]
   )
fi
AC_MSG_RESULT($mh_cv_func_reset_shell_mode)
if test "$mh_cv_func_reset_shell_mode" = yes ; then
   AC_DEFINE(HAVE_RESET_SHELL_MODE)
fi

AC_MSG_CHECKING(for reset_prog_mode in $curses_l library)
mh_cv_func_reset_prog_mode=$1
if test $1 = no; then
   AC_TRY_LINK(
[#include "xxxxx.h"]
[#include <$curses_h>],
[reset_prog_mode()],
   [mh_cv_func_reset_prog_mode=yes],
   [mh_cv_func_reset_prog_mode=no]
   )
fi
AC_MSG_RESULT($mh_cv_func_reset_prog_mode)
if test "$mh_cv_func_reset_prog_mode" = yes ; then
   AC_DEFINE(HAVE_RESET_PROG_MODE)
fi

AC_MSG_CHECKING(for slk_init in $curses_l library)
mh_cv_func_slk_init=$1
if test $1 = no; then
   AC_TRY_LINK(
[#include "xxxxx.h"]
[#include <$curses_h>],
[slk_init(0)],
   [mh_cv_func_slk_init=yes],
   [mh_cv_func_slk_init=no]
   )
fi
AC_MSG_RESULT($mh_cv_func_slk_init)
if test "$mh_cv_func_slk_init" = yes ; then
   AC_DEFINE(HAVE_SLK_INIT)
fi

AC_MSG_CHECKING(for slk_attrset in $curses_l library)
mh_cv_func_slk_attrset=$1
if test $1 = no; then
   AC_TRY_LINK(
[#include "xxxxx.h"]
[#include <$curses_h>],
[slk_attrset(0)],
   [mh_cv_func_slk_attrset=yes],
   [mh_cv_func_slk_attrset=no]
   )
fi
AC_MSG_RESULT($mh_cv_func_slk_attrset)
if test "$mh_cv_func_slk_attrset" = yes ; then
   AC_DEFINE(HAVE_SLK_ATTRSET)
fi

AC_MSG_CHECKING(for sb_init in $curses_l library)
mh_cv_func_sb_init=$have_sb
if test $have_sb = no; then
   AC_TRY_LINK(
[#include "xxxxx.h"]
[#include <$curses_h>],
[sb_init()],
   [mh_cv_func_sb_init=yes],
   [mh_cv_func_sb_init=no]
   )
fi
AC_MSG_RESULT($mh_cv_func_sb_init)
if test "$mh_cv_func_sb_init" = yes ; then
   AC_DEFINE(HAVE_SB_INIT)
fi

AC_MSG_CHECKING(for resize_term in $curses_l library)
mh_cv_func_resize_term=$1
if test $1 = no; then
   AC_TRY_LINK(
[#include "xxxxx.h"]
[#include <$curses_h>],
[resize_term(0,0)],
   [mh_cv_func_resize_term=yes],
   [mh_cv_func_resize_term=no]
   )
fi
AC_MSG_RESULT($mh_cv_func_resize_term)
if test "$mh_cv_func_resize_term" = yes ; then
   AC_DEFINE(HAVE_RESIZE_TERM)
fi

AC_MSG_CHECKING(for resizeterm in $curses_l library)
# PDCurses does not have resizeterm()
mh_cv_func_resizeterm=no
if test $1 = no; then
   AC_TRY_LINK(
[#include "xxxxx.h"]
[#include <$curses_h>],
[resizeterm(0,0)],
   [mh_cv_func_resizeterm=yes],
   [mh_cv_func_resizeterm=no]
   )
fi
AC_MSG_RESULT($mh_cv_func_resizeterm)
if test "$mh_cv_func_resizeterm" = yes ; then
   AC_DEFINE(HAVE_RESIZETERM)
fi

AC_MSG_CHECKING(for wvline in $curses_l library)
mh_cv_func_wvline=$1
if test $1 = no; then
   AC_TRY_LINK(
[#include "xxxxx.h"]
[#include <$curses_h>],
[wvline(stdscr,0,1)],
   [mh_cv_func_wvline=yes],
   [mh_cv_func_wvline=no]
   )
fi
AC_MSG_RESULT($mh_cv_func_wvline)
if test "$mh_cv_func_wvline" = yes ; then
   AC_DEFINE(HAVE_WVLINE)
fi

AC_MSG_CHECKING(for PDC_set_function_key in $curses_l library)
mh_cv_func_pdc_sfk=$1
if test $1 = no; then
   AC_TRY_LINK(
[#include "xxxxx.h"]
[#include <$curses_h>],
[PDC_set_function_key(0,0)],
   [mh_cv_func_pdc_sfk=yes],
   [mh_cv_func_pdc_sfk=no]
   )
fi
AC_MSG_RESULT($mh_cv_func_pdc_sfk)
if test "$mh_cv_func_pdc_sfk" = yes ; then
   AC_DEFINE(HAVE_PDC_SET_FUNCTION_KEY)
fi

AC_MSG_CHECKING(for curses_version in $curses_l library)
mh_cv_func_curses_version=$1
if test $1 = no; then
   AC_TRY_LINK(
[#include "xxxxx.h"]
[#include <$curses_h>],
[char *fred=curses_version()],
   [mh_cv_func_curses_version=yes],
   [mh_cv_func_curses_version=no]
   )
fi
AC_MSG_RESULT($mh_cv_func_curses_version)
if test "$mh_cv_func_curses_version" = yes ; then
   AC_DEFINE(HAVE_CURSES_VERSION)
fi

AC_MSG_CHECKING(if $curses_h WINDOW structure contains _begy)
AC_CACHE_VAL(
[mh_cv_have_underscore_begy],
[
AC_TRY_COMPILE(
[#include "xxxxx.h"]
[#include <$curses_h>],
[int xxx;WINDOW *w;xxx=w->_begy;],
[mh_cv_have_underscore_begy=yes],
[mh_cv_have_underscore_begy=no]
)
])dnl
AC_MSG_RESULT($mh_cv_have_underscore_begy)
if test "$mh_cv_have_underscore_begy" = yes ; then
   AC_DEFINE(HAVE_UNDERSCORE_BEGY)
fi

AC_MSG_CHECKING(if $curses_h WINDOW structure contains begy)
AC_CACHE_VAL(
[mh_cv_have_begy],
[
AC_TRY_COMPILE(
[#include "xxxxx.h"]
[#include <$curses_h>],
[int xxx;WINDOW *w;xxx=w->begy;],
[mh_cv_have_begy=yes],
[mh_cv_have_begy=no]
)
])dnl
AC_MSG_RESULT($mh_cv_have_begy)
if test "$mh_cv_have_begy" = yes ; then
   AC_DEFINE(HAVE_BEGY)
fi

AC_MSG_CHECKING(if $curses_h WINDOW structure contains _maxy)
AC_CACHE_VAL(
[mh_cv_have_underscore_maxy],
[
AC_TRY_COMPILE(
[#include "xxxxx.h"]
[#include <$curses_h>],
[int xxx;WINDOW *w;xxx=w->_maxy;],
[mh_cv_have_underscore_maxy=yes],
[mh_cv_have_underscore_maxy=no]
)
])dnl
AC_MSG_RESULT($mh_cv_have_underscore_maxy)
if test "$mh_cv_have_underscore_maxy" = yes ; then
   AC_DEFINE(HAVE_UNDERSCORE_MAXY)
fi

AC_MSG_CHECKING(if $curses_h WINDOW structure contains maxy)
AC_CACHE_VAL(
[mh_cv_have_maxy],
[
AC_TRY_COMPILE(
[#include "xxxxx.h"]
[#include <$curses_h>],
[int xxx;WINDOW *w;xxx=w->maxy;],
[mh_cv_have_maxy=yes],
[mh_cv_have_maxy=no]
)
])dnl
AC_MSG_RESULT($mh_cv_have_maxy)
if test "$mh_cv_have_maxy" = yes ; then
   AC_DEFINE(HAVE_MAXY)
fi

rm -f xxxxx.h
LIBS="$mh_save_libs"
CFLAGS="$mh_save_cflags"
])dnl

AC_DEFUN([MH_XCURSES_FUNCS],
[
AC_DEFINE(HAVE_BEEP)
AC_DEFINE(HAVE_BOX)
AC_DEFINE(HAVE_WHLINE)
AC_DEFINE(HAVE_CBREAK)
AC_DEFINE(HAVE_CURS_SET)
AC_DEFINE(HAVE_DOUPDATE)
AC_DEFINE(HAVE_DERWIN)
AC_DEFINE(HAVE_KEYPAD)
AC_DEFINE(HAVE_NEWPAD)
AC_DEFINE(HAVE_NOCBREAK)
AC_DEFINE(HAVE_NODELAY)
AC_DEFINE(HAVE_NOTIMEOUT)
AC_DEFINE(HAVE_PREFRESH)
AC_DEFINE(HAVE_UNGETCH)
AC_DEFINE(HAVE_PROTO)
AC_DEFINE(HAVE_RAW)
AC_DEFINE(HAVE_RESET_PROG_MODE)
AC_DEFINE(HAVE_RESET_SHELL_MODE)
AC_DEFINE(HAVE_RESIZE_TERM)
AC_DEFINE(HAVE_SLK_INIT)
AC_DEFINE(HAVE_SLK_ATTRSET)
if test "$THETYPE" = "X11"; then
AC_DEFINE(HAVE_SB_INIT)
fi
AC_DEFINE(HAVE_TOUCHLINE)
AC_DEFINE(HAVE_TYPEAHEAD)
AC_DEFINE(HAVE_WATTRSET)
AC_DEFINE(HAVE_WADDCHNSTR)
AC_DEFINE(HAVE_WBKGD)
AC_DEFINE(HAVE_WNOUTREFRESH)
AC_DEFINE(HAVE_WVLINE)
AC_DEFINE(HAVE_CURSES_VERSION)
])

dnl ---------------------------------------------------------------------------
dnl Determine if curses defines "chtype"
dnl ---------------------------------------------------------------------------
AC_DEFUN([MH_CHECK_CHTYPE],
[
mh_save_cflags="$CFLAGS"

cat > xxxxx.h <<EOF
#ifdef XCURSES
   char *XCursesProgramName="test";
#endif
EOF
dnl
dnl Include before curses.h any include files required. This is
dnl often stdarg.h
dnl
for incfile in $mh_pre_curses_h_include ; do
   echo "#include <$incfile.h>" >> xxxxx.h
done

if test $builtin_curses = yes; then
   CFLAGS="$BUILTIN_PDCURSES_INC $MH_CURSES_INC $SYS_DEFS"
else
   CFLAGS="$MH_CURSES_INC $SYS_DEFS"
fi

AC_MSG_CHECKING(if $curses_h defines "chtype")
AC_TRY_COMPILE(
[#include "xxxxx.h"]
[#include <$curses_h>],
[chtype xxx],
[mh_have_chtype=yes],
[mh_have_chtype=no;AC_DEFINE(chtype,int)]
)
rm -f xxxxx.h
AC_MSG_RESULT($mh_have_chtype)
CFLAGS="$mh_save_cflags"
])dnl

dnl ---------------------------------------------------------------------------
dnl Check for broken SYSVR4 curses implementations
dnl ---------------------------------------------------------------------------
AC_DEFUN([MH_CHECK_BROKEN_SYSVR4_CURSES],
[
AC_MSG_CHECKING(if $curses_l is a broken SYSVR4 curses)
dnl
dnl Known platform is Solaris 2.5+
dnl
case "$target" in
   *solaris2.5*)
      if test "$curses_l" = "curses" ; then
         mh_broken_sysvr4_curses=yes
      else
         mh_broken_sysvr4_curses=no
      fi
      ;;
   *solaris2.6*)
      if test "$curses_l" = "curses" ; then
         mh_broken_sysvr4_curses=yes
      else
         mh_broken_sysvr4_curses=no
      fi
      ;;
   *)mh_broken_sysvr4_curses=no
esac
if test "$mh_broken_sysvr4_curses" = yes ; then
   AC_DEFINE(HAVE_BROKEN_SYSVR4_CURSES)
fi
AC_MSG_RESULT($mh_broken_sysvr4_curses)
])dnl

dnl ---------------------------------------------------------------------------
dnl Check if __sighandler_t is defined
dnl ---------------------------------------------------------------------------
AC_DEFUN([MH_CHECK__SIGHANDLER_T],
[
AC_MSG_CHECKING(whether __sighandler_t is defined)

AC_CACHE_VAL(
[mh_cv__sighandler_t],
[
AC_TRY_COMPILE(
[#include <sys/types.h>]
[#include <signal.h>],
[__sighandler_t fred],
[mh_cv__sighandler_t=yes],
[mh_cv__sighandler_t=no]
)
])
AC_MSG_RESULT($mh_cv__sighandler_t)
if test "$mh_cv__sighandler_t" = yes ; then
        AC_DEFINE(HAVE__SIGHANDLER_T)
fi
])

dnl ---------------------------------------------------------------------------
dnl Check location of xterm for XCURSES version
dnl ---------------------------------------------------------------------------
AC_DEFUN([MH_FIND_XTERM],
[
mh_xterm_found=no
AC_MSG_CHECKING(for location of xterm)
mh_xterms="\
    /usr/X11R6/bin/xterm      \
    /usr/bin/X11R6/xterm      \
    /usr/X11R5/bin/xterm      \
    /usr/bin/X11R5/xterm      \
    /usr/X11/bin/xterm        \
    /usr/openwin/bin/xterm    \
    /usr/bin/X11/xterm        \
    /usr/local/bin/xterm      \
    /usr/contrib/bin/xterm"
for sout in $mh_xterms ; do
   if test -x $sout ; then
         mh_xterm_found=yes
      AC_DEFINE_UNQUOTED(XTERM_PROGRAM,"$sout")
      AC_MSG_RESULT(found in $sout)
      break 2
   fi
done
if test "$mh_xterm_found" = no ; then
   AC_DEFINE_UNQUOTED(XTERM_PROGRAM,"N/A")
   AC_MSG_RESULT(not found. You will need to run SET XTERMINAL before invoking a shell command within THE)
fi
])dnl

dnl ---------------------------------------------------------------------------
dnl Check for acl_get function under AIX
dnl ---------------------------------------------------------------------------
AC_DEFUN([MH_FUNC_ACL_GET],
[
mh_save_libs="$LIBS"
LIBS="$LIBS -ls"
mh_save_cflags="$CFLAGS"
CFLAGS="$CFLAGS -D_ALL_SOURCE"
AC_MSG_CHECKING(for function acl_get)
AC_CACHE_VAL(
[mh_cv_func_acl_get],
[
   AC_TRY_LINK(
[#include <sys/acl.h>],
[char *ptr=(char *)acl_get("XXX")],
   [mh_cv_func_acl_get=yes],
   [mh_cv_func_acl_get=no]
   )
])dnl
LIBS="$mh_save_libs"
CFLAGS="$mh_save_cflags"
AC_MSG_RESULT($mh_cv_func_acl_get)
if test "$mh_cv_func_acl_get" = yes ; then
   AC_DEFINE(HAVE_ACL_GET)
#  MH_EXTRA_LIBS="-ls"
#  AC_SUBST(MH_EXTRA_LIBS)
fi
])dnl

dnl ---------------------------------------------------------------------------
dnl Check for Checker
dnl ---------------------------------------------------------------------------
AC_DEFUN([MH_CHECK_CHECKER],
[
AC_CACHE_VAL(
[mh_cv_checker],
[
AC_MSG_CHECKING(for Checker)
if (checkergcc) >/dev/null 2>/dev/null; then
   mh_cv_checker=yes
   AC_MSG_RESULT(yes)
else
   mh_cv_checker=no
   AC_MSG_RESULT(no)
fi
])
])dnl
dnl ---------------------------------------------------------------------------
dnl Shows where THE header/libraries are
dnl Called from MH_SHOW_STATUS
dnl ---------------------------------------------------------------------------
AC_DEFUN([MH_SHOW_THE],
[
CURSES_LIB=""
if test "$with_curses" = "extcurses" ; then
   CURSES_LIB="Extended Curses"
elif test "x$CURSES_BUILD" = "x" ; then
   CURSES_LIB="Default - Not specified"
else
   CURSES_LIB="$CURSES_BUILD"
fi

if test "x$CURSES_CONFIG" = "x"; then
   cpopts="$CURSES_LIB"
else
   cpopts="$CURSES_LIB (using $CURSES_CONFIG)"
fi
echo "                   Curses package: $cpopts"
if test "$with_cursesincdir" != no ; then
echo "                curses headers in: $with_cursesincdir"
fi
if test "$with_curseslibdir" != no ; then
echo "              curses libraries in: $with_curseslibdir"
fi

myopts=""
if test "$with_trace" = yes ; then
   myopts="$myopts TRACE"
fi
if test "$with_xaw3d" = yes ; then
   myopts="$myopts XAW3D"
fi
if test "$with_nextaw" = yes ; then
   myopts="$myopts NEXTAW"
fi
if test "$ac_cv_prog_dw_config" = yes; then
   myopts="$myopts Using dw-config"
fi
if test "$mh_cv_checker" = yes ; then
   myopts="$myopts CHECKER"
fi
if test "$with_bounds_checking" = yes; then
   myopts="$myopts BOUNDS-CHECKING"
fi
])
