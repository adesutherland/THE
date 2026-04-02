Parse Arg _pkg in out _rexx_build _curses_build _curses_inc_dir_build _curses_lib_dir_build
_pkg_u = Translate( _pkg )
_rexx_build_u = Translate( _rexx_build )
_curses_build_u = Changestr( '-', Translate( _curses_build ), '_' )
conflicts.regina = 'oorexx rexxtrans'
conflicts.oorexx = 'regina rexxtrans'
conflicts.rexxtrans = 'regina oorexx'
/*
-- dependencies depending on what Rexx interpreter we are building with
*/
rexx_builddepends. = ''
rexx_depends. = ''
-- DEB
rexx_depends.regina.deb = ', libregina3'
rexx_depends.oorexx.deb = ', oorexx'
rexx_depends.rexxtrans.deb = ', librexxtrans'
rexx_builddepends.regina.deb = ', regina-rexx, libregina3, libregina3-dev'
rexx_builddepends.oorexx.deb = ', oorexx'
rexx_builddepends.rexxtrans.deb = ', librexxtrans, librexxtrans-dev'
-- RPM
rexx_depends.regina.rpm = ' libregina3'
rexx_depends.oorexx.rpm = ' oorexx'
rexx_depends.rexxtrans.rpm = ' librexxtrans'
rexx_builddepends.regina.rpm = ' regina-rexx libregina3 libregina3-devel'
rexx_builddepends.oorexx.rpm = ' oorexx'
rexx_builddepends.rexxtrans.rpm = ' librexxtrans libregina3-devel'
-- APK
rexx_depends.regina.apk = ' libregina3'
rexx_depends.oorexx.apk = ' oorexx'
rexx_depends.rexxtrans.apk = ' librexxtrans'
rexx_builddepends.regina.apk = ' regina-rexx libregina3 libregina3-devel'
rexx_builddepends.oorexx.apk = ' oorexx'
rexx_builddepends.rexxtrans.apk = ' librexxtrans libregina3-devel'
/*
-- dependencies depending on what Curses package we are building with
*/
curses_builddepends. = ''
curses_depends. = ''
-- DEB
curses_depends.pdcurses_sdl2w.deb = ', libsdl2-2.0-0, libsdl2-ttf-2.0-0, fonts-dejavu-core'
curses_depends.pdcurses_sdl1w.deb = ', libsdl, libsdl-ttf'
curses_builddepends.pdcurses_sdl2w.deb = ', libsdl2-ttf-dev'
curses_builddepends.pdcurses_sdl1w.deb = ', libsdl-ttf2.0-dev'
curses_builddepends.pdcurses_x11w.deb = ', libxt-dev, libxaw7-dev, libxmu-dev, libx11-dev, libxext-dev, libxpm-dev'
curses_depends.pdcurses_x11w.deb = ', libxt6, libxaw7, libxmu6, libx11-6, libxext6, libxpm4'
curses_builddepends.ncursesw.deb = ', libncurses-dev'
curses_depends.ncursesw.deb = ', libncursesw6'
-- RPM
curses_depends.pdcurses_sdl2w.rpm = ' SDL2 SDL2_ttf dejavu-sans-mono-fonts'
curses_depends.pdcurses_sdl1w.rpm = ' SDL SDL_ttf'
curses_builddepends.pdcurses_sdl2w.rpm = ' SDL2-devel SDL2_ttf-devel'
curses_builddepends.pdcurses_sdl1w.rpm = ' SDL-devel SDL_ttf-devel'
curses_builddepends.pdcurses_x11w.rpm = ' libXaw-devel libXt-devel libXmu-devel libX11-devel libXext-devel libXpm-devel'
curses_depends.pdcurses_x11w.rpm = ' libXaw libXt libXmu libX11 libXext libXpm'
curses_builddepends.ncursesw.rpm = ' ncurses-devel'
curses_depends.ncursesw.rpm = ' ncurses'
-- APK
curses_depends.pdcurses_sdl2w.apk = ' sdl2 sdl2_ttf ttf_dejavu'
curses_depends.pdcurses_sdl1w.apk = '' -- no sdl1 packages on Alpine
curses_builddepends.pdcurses_sdl2w.apk = ' sdl2-dev sdl2_ttf-dev'
curses_builddepends.pdcurses_sdl1w.apk = '' -- no sdl1 packages on Alpine
curses_builddepends.pdcurses_x11w.apk = ' libxaw-dev libxt-dev libxmu-dev libx11-dev libxext-dev libxpm-dev'
curses_depends.pdcurses_x11w.apk = ' libxaw libxt libxmu libx11 libxext libxpm'
curses_builddepends.ncursesw.apk = ' ncurses-dev'
curses_depends.ncursesw.apk = ' ncurses'

Call Stream out, 'C', 'OPEN WRITE REPLACE'
Do While Lines( in ) > 0
   line = Linein( in )
   line = Changestr( '%REXX_BUILD%', line, _rexx_build )
   line = Changestr( '%REXX_BUILD_DEPENDS%', line, rexx_builddepends._rexx_build_u._pkg_u )
   line = Changestr( '%REXX_DEPENDS%', line, rexx_depends._rexx_build_u._pkg_u )
   line = Changestr( '%REXX_CONFLICT_1%', line, Word( conflicts._rexx_build_u, 1 ) )
   line = Changestr( '%REXX_CONFLICT_2%', line, Word( conflicts._rexx_build_u, 2 ) )
   line = Changestr( '%CURSES_BUILD%', line, _curses_build )
   line = Changestr( '%CURSES_INC_DIR_BUILD%', line, _curses_inc_dir_build )
   line = Changestr( '%CURSES_LIB_DIR_BUILD%', line, _curses_lib_dir_build )
   line = Changestr( '%CURSES_BUILD_DEPENDS%', line, curses_builddepends._curses_build_u._pkg_u )
   line = Changestr( '%CURSES_DEPENDS%', line, curses_depends._curses_build_u._pkg_u )
   Call Lineout out, line
End
Call Stream out, 'C', 'CLOSE'
