#
#########################################################################
#
# makefile for The Hessling Editor (THE)
#
#########################################################################
#
# You need the following environment variables set like:
# THE_SRCDIR=c:\the
# PDCURSES_SRCDIR=c:\pdcurses
# PDCURSES_BINDIR=c:\pdc\vc
# If building with Regina...
#   REGINA_BINDIR=c:\regina\vc
#   REGINA_SRCDIR=c:\regina
# If building with Object Rexx...
#   OREXX_BINDIR=c:\objrexx\api
# If building with Open Object Rexx...
#   REXX_HOME is set
# If building with Rexx/Trans...
#   REXXTRANS_BINDIR=c:\rexxtrans
#   REXXTRANS_SRCDIR=c:\rexxtrans
# If building with WinRexx...
#   WINREXX_BINDIR=c:\winrexx
#   WINREXX_SRCDIR=c:\winrexx
# If building with Quercus Rexx...
#   QUERCUS_BINDIR=c:\quercus
#   QUERCUS_SRCDIR=c:\quercus
# If building with uni-Rexx...
#   UNIREXX_BINDIR=c:\unirexx
#   UNIREXX_SRCDIR=c:\unirexx
#
# MSVCDIR=c:\program files\microsoft visual studio\vc98
#
#########################################################################
#
!if "$(TARGET_CPU)" == ""
TARGET_CPU=x86
!endif

!if "$(TARGET_CPU)" == "x86"
LARCH=w32
BARCH=W32
WARCH=win32
MACH=x86
!else
LARCH=w64
BARCH=W64
WARCH=win64
MACH=x64
!endif

SRCDIR=$(THE_SRCDIR)

#
# Use STATIC linking to PDCurses ?????
# Using the PDCurses DLL, then "the -1" will refresh the newly edited
# file correctly. The STATIC linking will NOT refresh the newly edited
# file correctly; you will need to hit a key for it to refresh.
#
#!if "$(STATIC)" == "N"
#PDCURSES_LIB=pdcurses.lib
#PDCURSES_DEFINES=-DPDC_DLL_BUILD
#CURSESDLLINNSIS=/DCURSESDLL
#STATIC=N
#!else
#PDCURSES_LIB=pdcurses.lib
#PDCURSES_DEFINES=
#CURSESDLLINNSIS=
#STATIC=Y
#!endif

!if "$(UTF8)" == "Y"
UTF8_FLAG=-DUSE_UTF8 -DPDC_FORCE_UTF8
UTF8=Y
!else
UTF8_FLAG=
UTF8=N
!endif

SRC       = $(THE_SRCDIR)
SOURCE    = $(SRC)\src
PDCSRC    = $(SOURCE)\PDCursesMod
osdir     = $(PDCSRC)\wingui
CURSCON   = pdc-conw
CURSGUI   = pdc-guiw
CURSSDL2   = pdc-sdl2w
!if "$(TARGET_CPU)" == "x86"
REXXTRANSBIN = d:\bin32
PDCURSESBIN = d:\bin32
!else
REXXTRANSBIN = d:\bin64
PDCURSESBIN = d:\bin64
!endif
PDCURSES_LIB=pdcurses.lib
INCGUIW = -DPDC_WIDE -I$(PDCSRC) -I$(PDCSRC)\wingui #-I$(PDCURSES_SRCDIR_WINGUI)\wingui
INCSDL2W = -DPDC_WIDE -I$(PDCSRC) -I$(PDCSRC)\sdl2 #-I$(PDCURSES_SRCDIR_SDL2)\sdl2
INCSDLH = -I$(SDLBASE)\include\SDL2 -I$(TTFBASE)\include
INCCONW = -DPDC_WIDE -I$(PDCSRC) -I$(PDCSRC)\wincon
CURSINC   = $(PDCURSES_DEFINES)

#SDL2_LIB = $(SDLBASE)\lib\$(MACH)\SDL2.lib
#SDL2_LIBMAIN = $(SDLBASE)\lib\$(MACH)\SDL2main.lib
#TTF_LIB = $(TTFBASE)\lib\$(MACH)\SDL2_ttf.lib
#EXTRALIBSDL = $(SDL2_LIBMAIN) $(SDL2_LIB) $(TTF_LIB)

SDL2_LIB = $(SDLBASE)\lib\SDL2.lib
TTF_LIB = $(TTFBASE)\lib\SDL2_ttf.lib
EXTRALIBSDL = $(SDL2_LIB) $(TTF_LIB)

!ifdef MSVCDIR
# this env variable for VC 6
SETARGV   = "$(MSVCDIR)\lib\setargv.obj"
!else
# The following for Visual C++ 10
!if "$(LARCH)" == "w32"
SETARGV   = "$(VCTOOLSINSTALLDIR)\lib\x86\setargv.obj"
!else
SETARGV   = "$(VCTOOLSINSTALLDIR)\lib\x64\setargv.obj"
!endif
!endif

REGINA_BIN = "$(REGINA_HOME)"
REGINA_REXXLIBS = "$(REGINA_HOME)\lib\regina.lib"
REGINA_REXXINC = "-I$(REGINA_HOME)\include" -DUSE_REGINA
OOREXX_REXXLIBS = "$(REXX_HOME)\api\rexx.lib" "$(REXX_HOME)\api\rexxapi.lib"
OOREXX_REXXINC = "-I$(REXX_HOME)\api" -DUSE_OOREXX
REXXTRANS_REXXLIBS = $(REXXTRANS_BINDIR)\rexxtrans.lib
REXXTRANS_REXXINC = -I$(REXXTRANS_SRCDIR) -DUSE_REXXTRANS

OREXX_REXXLIBS = $(OREXX_BINDIR)\api\rexx.lib $(OREXX_BINDIR)\api\rexxapi.lib
OREXX_REXXINC = -I$(OREXX_BINDIR)\api -DUSE_OREXX
WINREXX_REXXLIBS = $(WINREXX_BINDIR)\api\rxrexx.lib
WINREXX_REXXINC = -I$(WINREXX_BINDIR)\api -DUSE_WINREXX
QUERCUS_REXXLIBS = $(QUERCUS_BINDIR)\api\wrexx32.lib
QUERCUS_REXXINC = -I$(QUERCUS_BINDIR)\api -DUSE_QUERCUS
UNIREXX_REXXLIBS = $(UNIREXX_BINDIR)\rxx.lib
UNIREXX_REXXINC = -I$(UNIREXX_BINDIR) -DUSE_UNIREXX

.SUFFIXES:
#
# Following included file provides VER, VER_DOT, VER_DATE
#
!include $(SRC)\the.ver

#########################################################################
# MS VC++ compiler on Windows
#########################################################################
PROJ      = the.exe
OBJ       = obj
CC        = cl
LIBEXE    = lib.exe

!if "$(INT)" == "REGINA"
REXXLIB = $(REGINA_REXXLIBS)
REXXINC =  $(REGINA_REXXINC)
REXXINT = "Regina"
!elseif "$(INT)" == "OREXX"
REXXLIB = $(OREXX_REXXLIBS)
REXXINC =  $(OREXX_REXXINC)
!elseif "$(INT)" == "OOREXX"
REXXLIB = $(OOREXX_REXXLIBS)
REXXINC =  $(OOREXX_REXXINC)
REXXINT = "ooRexx"
!elseif "$(INT)" == "WINREXX"
REXXLIB = $(WINREXX_REXXLIBS)
REXXINC =  $(WINREXX_REXXINC)
!elseif "$(INT)" == "QUERCUS"
REXXLIB = $(QUERCUS_REXXLIBS)
REXXINC =  $(QUERCUS_REXXINC)
!elseif "$(INT)" == "UNIREXX"
REXXLIB = $(UNIREXX_REXXLIBS)
REXXINC =  $(UNIREXX_REXXINC)
!elseif "$(INT)" == "REXXTRANS"
REXXLIB = $(REXXTRANS_REXXLIBS)
REXXINC =  $(REXXTRANS_REXXINC)
REXXINT = "RexxTrans"
!else
!message Rexx Interpreter NOT specified via INT macro
!message Valid values are: REGINA OOREXX REXXTRANS OREXX WINREXX QUERCUS UNIREXX
!error Make aborted!
!endif

#!if "$(PDC)" == "SDL"
#PDC_FLAGS = -DUSE_SDLCURSES
#CURSLIB   = $(CURSSDL)\$(PDCURSES_LIB)
#THEOBJ = "sdl"
#!else
#!if "$(PDC)" == "WINGUI"
#PDC_FLAGS = -DUSE_WINGUICURSES -I$(SRC)\contrib
##DRAGDROPOBJ = DragAndDrop.obj
##EXTRALIB = ole32.lib
#LDEXTRA = "-subsystem:windows"
#CURSLIB   = $(CURSGUI)\$(PDCURSES_LIB)
#THEOBJ = "gui"
#!else
#PDC_FLAGS =
#CURSLIB   = $(CURSCON)\$(PDCURSES_LIB)
#PDC = "CONSOLE"
#THEOBJ = "console"
#!endif
#!endif

!if "$(TRACE)" == "Y"
TRACE_FLAGS = -DTHE_TRACE
GUI_TRACE_OBJ = the-guiw\trace.obj
CONSOLE_TRACE_OBJ = the-conw\trace.obj
SDL2_TRACE_OBJ = the-sdl2w\trace.obj
!else
TRACE_FLAGS =
GUI_TRACE_OBJ =
CONSOLE_TRACE_OBJ =
SDL2_TRACE_OBJ =
TRACE=N
!endif

!if "$(DEBUG)" == "Y"
CFLAGS    = -nologo -c -Od -Z7 -FR -DDEBUG -DWIN32 $(UTF8_FLAG) -DSTDC_HEADERS -DHAVE_PROTO $(TRACE_FLAGS) $(PDC_FLAGS) -I$(SOURCE) -I$(SOURCE)\contrib $(CURSINC) $(REXXINC) -Fo$@
LDEBUG     = -debug -map:the.map
DIST=
!else
CFLAGS    = -nologo -c -Ox -DWIN32 $(UTF8_FLAG) -DSTDC_HEADERS -DHAVE_PROTO $(TRACE_FLAGS) $(PDC_FLAGS) -I$(SOURCE) -I$(SOURCE)\contrib $(CURSINC) $(REXXINC) -Fo$@
LDEBUG     = -release
DEBUG=N
!if "$(INT)" == "REXXTRANS"
DIST=dist
!else
DIST=
!endif
!endif

PDCLIBCCFLAGS = -nologo -c -Ox -Fo$@

VER_DEFS = -DTHE_VERSION=\"$(VER_DOT)\" -DTHE_VERSION_DATE=\"$(VER_DATE)\"

LD        = link
XTRAOBJ   = mygetopt.obj
MAN       = manext.exe
THERC     = $(SRC)\the$(LARCH).rc
THERES    = the$(LARCH).res
THEWINOBJ = the$(LARCH).obj
docdir = doc
#########################################################################
#
# THE Object files
#
THEGUIWOBJS = the-guiw\box.obj the-guiw\colour.obj the-guiw\comm1.obj the-guiw\comm2.obj the-guiw\comm3.obj the-guiw\comm4.obj the-guiw\comm5.obj \
	the-guiw\commset1.obj the-guiw\commset2.obj the-guiw\commsos.obj the-guiw\cursor.obj the-guiw\default.obj the-guiw\utf8.obj \
	the-guiw\edit.obj the-guiw\error.obj the-guiw\execute.obj the-guiw\linked.obj the-guiw\column.obj the-guiw\mouse.obj the-guiw\memory.obj \
	the-guiw\nonansi.obj the-guiw\prefix.obj the-guiw\reserved.obj the-guiw\scroll.obj the-guiw\show.obj the-guiw\single.obj the-guiw\sort.obj \
	the-guiw\target.obj the-guiw\the.obj the-guiw\util.obj the-guiw\parser.obj the-guiw\regex.obj the-guiw\commutil.obj the-guiw\print.obj \
	the-guiw\getch.obj the-guiw\query.obj the-guiw\query1.obj the-guiw\query2.obj the-guiw\thematch.obj the-guiw\directry.obj the-guiw\file.obj the-guiw\rexx.obj \
	the-guiw\mygetopt.obj $(GUI_TRACE_OBJ) #$(DRAGDROPOBJ)
#-----------------------------------------------------------------------
THESDL2WOBJS = the-sdl2w\box.obj the-sdl2w\colour.obj the-sdl2w\comm1.obj the-sdl2w\comm2.obj the-sdl2w\comm3.obj the-sdl2w\comm4.obj the-sdl2w\comm5.obj \
	the-sdl2w\commset1.obj the-sdl2w\commset2.obj the-sdl2w\commsos.obj the-sdl2w\cursor.obj the-sdl2w\default.obj the-sdl2w\utf8.obj \
	the-sdl2w\edit.obj the-sdl2w\error.obj the-sdl2w\execute.obj the-sdl2w\linked.obj the-sdl2w\column.obj the-sdl2w\mouse.obj the-sdl2w\memory.obj \
	the-sdl2w\nonansi.obj the-sdl2w\prefix.obj the-sdl2w\reserved.obj the-sdl2w\scroll.obj the-sdl2w\show.obj the-sdl2w\single.obj the-sdl2w\sort.obj \
	the-sdl2w\target.obj the-sdl2w\the.obj the-sdl2w\util.obj the-sdl2w\parser.obj the-sdl2w\regex.obj the-sdl2w\commutil.obj the-sdl2w\print.obj \
	the-sdl2w\getch.obj the-sdl2w\query.obj the-sdl2w\query1.obj the-sdl2w\query2.obj the-sdl2w\thematch.obj the-sdl2w\directry.obj the-sdl2w\file.obj the-sdl2w\rexx.obj \
	the-sdl2w\mygetopt.obj $(SDL2_TRACE_OBJ) #$(DRAGDROPOBJ)
#-----------------------------------------------------------------------
THECONWOBJS = the-conw\box.obj the-conw\colour.obj the-conw\comm1.obj the-conw\comm2.obj the-conw\comm3.obj the-conw\comm4.obj the-conw\comm5.obj \
	the-conw\commset1.obj the-conw\commset2.obj the-conw\commsos.obj the-conw\cursor.obj the-conw\default.obj the-conw\utf8.obj \
	the-conw\edit.obj the-conw\error.obj the-conw\execute.obj the-conw\linked.obj the-conw\column.obj the-conw\mouse.obj the-conw\memory.obj \
	the-conw\nonansi.obj the-conw\prefix.obj the-conw\reserved.obj the-conw\scroll.obj the-conw\show.obj the-conw\single.obj the-conw\sort.obj \
	the-conw\target.obj the-conw\the.obj the-conw\util.obj the-conw\parser.obj the-conw\regex.obj the-conw\commutil.obj the-conw\print.obj \
	the-conw\getch.obj the-conw\query.obj the-conw\query1.obj the-conw\query2.obj the-conw\thematch.obj the-conw\directry.obj the-conw\file.obj the-conw\rexx.obj \
	the-conw\mygetopt.obj $(CONSOLE_TRACE_OBJ) #$(DRAGDROPOBJ)
#-----------------------------------------------------------------------
EXECOBJS = the-guiw\mygetopt.obj .\execthe.obj
#########################################################################
#
# PDCursesMod Object Files
#
PDCCONWLIBOBJS = pdc-conw\addch.obj pdc-conw\addchstr.obj pdc-conw\addstr.obj pdc-conw\attr.obj pdc-conw\beep.obj \
pdc-conw\bkgd.obj pdc-conw\border.obj pdc-conw\clear.obj pdc-conw\color.obj pdc-conw\delch.obj pdc-conw\deleteln.obj \
pdc-conw\getch.obj pdc-conw\getstr.obj pdc-conw\getyx.obj pdc-conw\inch.obj pdc-conw\inchstr.obj \
pdc-conw\initscr.obj pdc-conw\inopts.obj pdc-conw\insch.obj pdc-conw\insstr.obj pdc-conw\instr.obj pdc-conw\kernel.obj \
pdc-conw\keyname.obj pdc-conw\mouse.obj pdc-conw\move.obj pdc-conw\outopts.obj pdc-conw\overlay.obj pdc-conw\pad.obj \
pdc-conw\panel.obj pdc-conw\printw.obj pdc-conw\refresh.obj pdc-conw\scanw.obj pdc-conw\scr_dump.obj pdc-conw\scroll.obj \
pdc-conw\slk.obj pdc-conw\termattr.obj pdc-conw\touch.obj pdc-conw\util.obj pdc-conw\window.obj pdc-conw\debug.obj

PDCCONWOBJS = pdc-conw\pdcclip.obj pdc-conw\pdcdisp.obj pdc-conw\pdcgetsc.obj pdc-conw\pdckbd.obj pdc-conw\pdcscrn.obj \
pdc-conw\pdcsetsc.obj pdc-conw\pdcutil.obj

PDCGUIWLIBOBJS = pdc-guiw\addch.obj pdc-guiw\addchstr.obj pdc-guiw\addstr.obj pdc-guiw\attr.obj pdc-guiw\beep.obj \
pdc-guiw\bkgd.obj pdc-guiw\border.obj pdc-guiw\clear.obj pdc-guiw\color.obj pdc-guiw\delch.obj pdc-guiw\deleteln.obj \
pdc-guiw\getch.obj pdc-guiw\getstr.obj pdc-guiw\getyx.obj pdc-guiw\inch.obj pdc-guiw\inchstr.obj \
pdc-guiw\initscr.obj pdc-guiw\inopts.obj pdc-guiw\insch.obj pdc-guiw\insstr.obj pdc-guiw\instr.obj pdc-guiw\kernel.obj \
pdc-guiw\keyname.obj pdc-guiw\mouse.obj pdc-guiw\move.obj pdc-guiw\outopts.obj pdc-guiw\overlay.obj pdc-guiw\pad.obj \
pdc-guiw\panel.obj pdc-guiw\printw.obj pdc-guiw\refresh.obj pdc-guiw\scanw.obj pdc-guiw\scr_dump.obj pdc-guiw\scroll.obj \
pdc-guiw\slk.obj pdc-guiw\termattr.obj pdc-guiw\touch.obj pdc-guiw\util.obj pdc-guiw\window.obj pdc-guiw\debug.obj

PDCGUIWOBJS = pdc-guiw\pdcclip.obj pdc-guiw\pdcdisp.obj pdc-guiw\pdcgetsc.obj pdc-guiw\pdckbd.obj pdc-guiw\pdcscrn.obj \
pdc-guiw\pdcsetsc.obj pdc-guiw\pdcutil.obj

PDCSDL2WLIBOBJS = pdc-sdl2w\addch.obj pdc-sdl2w\addchstr.obj pdc-sdl2w\addstr.obj pdc-sdl2w\attr.obj pdc-sdl2w\beep.obj \
pdc-sdl2w\bkgd.obj pdc-sdl2w\border.obj pdc-sdl2w\clear.obj pdc-sdl2w\color.obj pdc-sdl2w\delch.obj pdc-sdl2w\deleteln.obj \
pdc-sdl2w\getch.obj pdc-sdl2w\getstr.obj pdc-sdl2w\getyx.obj pdc-sdl2w\inch.obj pdc-sdl2w\inchstr.obj \
pdc-sdl2w\initscr.obj pdc-sdl2w\inopts.obj pdc-sdl2w\insch.obj pdc-sdl2w\insstr.obj pdc-sdl2w\instr.obj pdc-sdl2w\kernel.obj \
pdc-sdl2w\keyname.obj pdc-sdl2w\mouse.obj pdc-sdl2w\move.obj pdc-sdl2w\outopts.obj pdc-sdl2w\overlay.obj pdc-sdl2w\pad.obj \
pdc-sdl2w\panel.obj pdc-sdl2w\printw.obj pdc-sdl2w\refresh.obj pdc-sdl2w\scanw.obj pdc-sdl2w\scr_dump.obj pdc-sdl2w\scroll.obj \
pdc-sdl2w\slk.obj pdc-sdl2w\termattr.obj pdc-sdl2w\touch.obj pdc-sdl2w\util.obj pdc-sdl2w\window.obj pdc-sdl2w\debug.obj

PDCSDL2WOBJS = pdc-sdl2w\pdcclip.obj pdc-sdl2w\pdcdisp.obj pdc-sdl2w\pdcgetsc.obj pdc-sdl2w\pdckbd.obj pdc-sdl2w\pdcscrn.obj \
pdc-sdl2w\pdcsetsc.obj pdc-sdl2w\pdcutil.obj

#########################################################################

COMM = $(SOURCE)\comm1.c $(SOURCE)\comm2.c $(SOURCE)\comm3.c $(SOURCE)\comm4.c $(SOURCE)\comm5.c \
	$(SOURCE)\commsos.c $(SOURCE)\commset1.c $(SOURCE)\commset2.c $(SOURCE)\query.c

APPENDIX = $(SRC)\appendix.1
GLOSSARY = $(SRC)\glossary

all: how the-guiw.exe the-conw.exe the-sdl2w.exe the.exe $(DIST)

how:
	echo nmake -f $(SRC)\vcwin.mak INT=$(INT) DEBUG=$(DEBUG) TRACE=$(TRACE) UTF8=$(UTF8) ^%1 ^%2 > rebuild.bat
#
#########################################################################
# Rules for building the THE launcher
the.exe:	$(EXECOBJS) $(THERES) $(THEWINOBJ)
	$(LD) /subsystem:windows /NOLOGO /VERSION:$(VER_DOT) $(LDEBUG) $(EXECOBJS) $(THEWINOBJ) $(SETARGV) -out:the.exe $(LDEXTRA) kernel32.lib userenv.lib advapi32.lib
#------------------------------------------------------------------------
execthe.obj:	$(SOURCE)\execthe.c
	$(CC) $(CFLAGS) $(VER_DEFS) $(SOURCE)\$(*B).c
#########################################################################
# Rules for building the GUI version of THE
$(CURSGUI)\$(PDCURSES_LIB): pdc-guiw $(PDCGUIWLIBOBJS) $(PDCGUIWOBJS) $(DEFFILE)
	$(LIBEXE) -out:$@ $(PDCGUIWLIBOBJS) $(PDCGUIWOBJS)

the-guiw.exe: the-guiw $(CURSGUI)\$(PDCURSES_LIB) $(THEGUIWOBJS) $(THERES) $(THEWINOBJ)
	$(LD) /subsystem:windows /NOLOGO /VERSION:$(VER_DOT) $(LDEBUG) $(THEGUIWOBJS) $(THEWINOBJ) $(SETARGV) -out:$*.exe $(LDEXTRA) $(CURSGUI)\$(PDCURSES_LIB) $(REXXLIB) user32.lib gdi32.lib comdlg32.lib winspool.lib wsock32.lib shell32.lib advapi32.lib kernel32.lib winmm.lib $(EXTRALIB)
#------------------------------------------------------------------------
#$(osdir)}.c{pdc-guiw\}.obj::
#$(CC) $(PDCLIBCCFLAGS) $(INCGUIW)  $<
pdc-guiw\addch.obj: $(PDCSRC)\pdcurses\addch.c
	$(CC) $(PDCLIBCCFLAGS) $(INCGUIW)  -c $(PDCSRC)\pdcurses\addch.c
pdc-guiw\addchstr.obj: $(PDCSRC)\pdcurses\addchstr.c
	$(CC) $(PDCLIBCCFLAGS) $(INCGUIW)  -c $(PDCSRC)\pdcurses\addchstr.c
pdc-guiw\addstr.obj : $(PDCSRC)\pdcurses\addstr.c
	$(CC) $(PDCLIBCCFLAGS) $(INCGUIW)  -c $(PDCSRC)\pdcurses\addstr.c
pdc-guiw\attr.obj : $(PDCSRC)\pdcurses\attr.c
	$(CC) $(PDCLIBCCFLAGS) $(INCGUIW)  -c $(PDCSRC)\pdcurses\attr.c
pdc-guiw\beep.obj: $(PDCSRC)\pdcurses\beep.c
	$(CC) $(PDCLIBCCFLAGS) $(INCGUIW)  -c $(PDCSRC)\pdcurses\beep.c
pdc-guiw\bkgd.obj : $(PDCSRC)\pdcurses\bkgd.c
	$(CC) $(PDCLIBCCFLAGS) $(INCGUIW)  -c $(PDCSRC)\pdcurses\bkgd.c
pdc-guiw\border.obj : $(PDCSRC)\pdcurses\border.c
	$(CC) $(PDCLIBCCFLAGS) $(INCGUIW)  -c $(PDCSRC)\pdcurses\border.c
pdc-guiw\clear.obj : $(PDCSRC)\pdcurses\clear.c
	$(CC) $(PDCLIBCCFLAGS) $(INCGUIW)  -c $(PDCSRC)\pdcurses\clear.c
pdc-guiw\color.obj : $(PDCSRC)\pdcurses\color.c
	$(CC) $(PDCLIBCCFLAGS) $(INCGUIW)  -c $(PDCSRC)\pdcurses\color.c
pdc-guiw\delch.obj : $(PDCSRC)\pdcurses\delch.c
	$(CC) $(PDCLIBCCFLAGS) $(INCGUIW)  -c $(PDCSRC)\pdcurses\delch.c
pdc-guiw\deleteln.obj: $(PDCSRC)\pdcurses\deleteln.c
	$(CC) $(PDCLIBCCFLAGS) $(INCGUIW)  -c $(PDCSRC)\pdcurses\deleteln.c
pdc-guiw\getch.obj : $(PDCSRC)\pdcurses\getch.c
	$(CC) $(PDCLIBCCFLAGS) $(INCGUIW)  -c $(PDCSRC)\pdcurses\getch.c
pdc-guiw\getstr.obj : $(PDCSRC)\pdcurses\getstr.c
	$(CC) $(PDCLIBCCFLAGS) $(INCGUIW)  -c $(PDCSRC)\pdcurses\getstr.c
pdc-guiw\getyx.obj : $(PDCSRC)\pdcurses\getyx.c
	$(CC) $(PDCLIBCCFLAGS) $(INCGUIW)  -c $(PDCSRC)\pdcurses\getyx.c
pdc-guiw\inch.obj : $(PDCSRC)\pdcurses\inch.c
	$(CC) $(PDCLIBCCFLAGS) $(INCGUIW)  -c $(PDCSRC)\pdcurses\inch.c
pdc-guiw\inchstr.obj: $(PDCSRC)\pdcurses\inchstr.c
	$(CC) $(PDCLIBCCFLAGS) $(INCGUIW)  -c $(PDCSRC)\pdcurses\inchstr.c
pdc-guiw\initscr.obj : $(PDCSRC)\pdcurses\initscr.c
	$(CC) $(PDCLIBCCFLAGS) $(INCGUIW)  -c $(PDCSRC)\pdcurses\initscr.c
pdc-guiw\inopts.obj : $(PDCSRC)\pdcurses\inopts.c
	$(CC) $(PDCLIBCCFLAGS) $(INCGUIW)  -c $(PDCSRC)\pdcurses\inopts.c
pdc-guiw\insch.obj : $(PDCSRC)\pdcurses\insch.c
	$(CC) $(PDCLIBCCFLAGS) $(INCGUIW)  -c $(PDCSRC)\pdcurses\insch.c
pdc-guiw\insstr.obj : $(PDCSRC)\pdcurses\insstr.c
	$(CC) $(PDCLIBCCFLAGS) $(INCGUIW)  -c $(PDCSRC)\pdcurses\insstr.c
pdc-guiw\instr.obj : $(PDCSRC)\pdcurses\instr.c
	$(CC) $(PDCLIBCCFLAGS) $(INCGUIW)  -c $(PDCSRC)\pdcurses\instr.c
pdc-guiw\kernel.obj: $(PDCSRC)\pdcurses\kernel.c
	$(CC) $(PDCLIBCCFLAGS) $(INCGUIW)  -c $(PDCSRC)\pdcurses\kernel.c
pdc-guiw\keyname.obj : $(PDCSRC)\pdcurses\keyname.c
	$(CC) $(PDCLIBCCFLAGS) $(INCGUIW)  -c $(PDCSRC)\pdcurses\keyname.c
pdc-guiw\mouse.obj : $(PDCSRC)\pdcurses\mouse.c
	$(CC) $(PDCLIBCCFLAGS) $(INCGUIW)  -c $(PDCSRC)\pdcurses\mouse.c
pdc-guiw\move.obj : $(PDCSRC)\pdcurses\move.c
	$(CC) $(PDCLIBCCFLAGS) $(INCGUIW)  -c $(PDCSRC)\pdcurses\move.c
pdc-guiw\outopts.obj : $(PDCSRC)\pdcurses\outopts.c
	$(CC) $(PDCLIBCCFLAGS) $(INCGUIW)  -c $(PDCSRC)\pdcurses\outopts.c
pdc-guiw\overlay.obj : $(PDCSRC)\pdcurses\overlay.c
	$(CC) $(PDCLIBCCFLAGS) $(INCGUIW)  -c $(PDCSRC)\pdcurses\overlay.c
pdc-guiw\pad.obj: $(PDCSRC)\pdcurses\pad.c
	$(CC) $(PDCLIBCCFLAGS) $(INCGUIW)  -c $(PDCSRC)\pdcurses\pad.c
pdc-guiw\panel.obj : $(PDCSRC)\pdcurses\panel.c
	$(CC) $(PDCLIBCCFLAGS) $(INCGUIW)  -c $(PDCSRC)\pdcurses\panel.c
pdc-guiw\printw.obj : $(PDCSRC)\pdcurses\printw.c
	$(CC) $(PDCLIBCCFLAGS) $(INCGUIW)  -c $(PDCSRC)\pdcurses\printw.c
pdc-guiw\refresh.obj : $(PDCSRC)\pdcurses\refresh.c
	$(CC) $(PDCLIBCCFLAGS) $(INCGUIW)  -c $(PDCSRC)\pdcurses\refresh.c
pdc-guiw\scanw.obj : $(PDCSRC)\pdcurses\scanw.c
	$(CC) $(PDCLIBCCFLAGS) $(INCGUIW)  -c $(PDCSRC)\pdcurses\scanw.c
pdc-guiw\scr_dump.obj : $(PDCSRC)\pdcurses\scr_dump.c
	$(CC) $(PDCLIBCCFLAGS) $(INCGUIW)  -c $(PDCSRC)\pdcurses\scr_dump.c
pdc-guiw\scroll.obj: $(PDCSRC)\pdcurses\scroll.c
	$(CC) $(PDCLIBCCFLAGS) $(INCGUIW)  -c $(PDCSRC)\pdcurses\scroll.c
pdc-guiw\slk.obj : $(PDCSRC)\pdcurses\slk.c
	$(CC) $(PDCLIBCCFLAGS) $(INCGUIW)  -c $(PDCSRC)\pdcurses\slk.c
pdc-guiw\termattr.obj : $(PDCSRC)\pdcurses\termattr.c
	$(CC) $(PDCLIBCCFLAGS) $(INCGUIW)  -c $(PDCSRC)\pdcurses\termattr.c
pdc-guiw\touch.obj : $(PDCSRC)\pdcurses\touch.c
	$(CC) $(PDCLIBCCFLAGS) $(INCGUIW)  -c $(PDCSRC)\pdcurses\touch.c
pdc-guiw\util.obj : $(PDCSRC)\pdcurses\util.c
	$(CC) $(PDCLIBCCFLAGS) $(INCGUIW)  -c $(PDCSRC)\pdcurses\util.c
pdc-guiw\window.obj : $(PDCSRC)\pdcurses\window.c
	$(CC) $(PDCLIBCCFLAGS) $(INCGUIW)  -c $(PDCSRC)\pdcurses\window.c
pdc-guiw\debug.obj: $(PDCSRC)\pdcurses\debug.c
	$(CC) $(PDCLIBCCFLAGS) $(INCGUIW)  -c $(PDCSRC)\pdcurses\debug.c

pdc-guiw\pdcclip.obj: $(PDCSRC)\wingui\pdcclip.c
	$(CC) $(PDCLIBCCFLAGS) $(INCGUIW)  -c $(PDCSRC)\wingui\pdcclip.c
pdc-guiw\pdcdisp.obj: $(PDCSRC)\wingui\pdcdisp.c
	$(CC) $(PDCLIBCCFLAGS) $(INCGUIW)  -c $(PDCSRC)\wingui\pdcdisp.c
pdc-guiw\pdcgetsc.obj: $(PDCSRC)\wingui\pdcgetsc.c
	$(CC) $(PDCLIBCCFLAGS) $(INCGUIW)  -c $(PDCSRC)\wingui\pdcgetsc.c
pdc-guiw\pdckbd.obj: $(PDCSRC)\wingui\pdckbd.c
	$(CC) $(PDCLIBCCFLAGS) $(INCGUIW)  -c $(PDCSRC)\wingui\pdckbd.c
pdc-guiw\pdcscrn.obj: $(PDCSRC)\wingui\pdcscrn.c
	$(CC) $(PDCLIBCCFLAGS) $(INCGUIW)  -c $(PDCSRC)\wingui\pdcscrn.c
pdc-guiw\pdcsetsc.obj: $(PDCSRC)\wingui\pdcsetsc.c
	$(CC) $(PDCLIBCCFLAGS) $(INCGUIW)  -c $(PDCSRC)\wingui\pdcsetsc.c
pdc-guiw\pdcutil.obj: $(PDCSRC)\wingui\pdcutil.c
	$(CC) $(PDCLIBCCFLAGS) $(INCGUIW)  -c $(PDCSRC)\wingui\pdcutil.c
#------------------------------------------------------------------------
the-guiw\box.obj:	$(SOURCE)\box.c $(SOURCE)\the.h $(SOURCE)\thedefs.h $(SOURCE)\proto.h
	$(CC) -DUSE_WINGUICURSES $(CFLAGS) $(INCGUIW) $(SOURCE)\$(*B).c
the-guiw\colour.obj:	$(SOURCE)\colour.c $(SOURCE)\the.h $(SOURCE)\thedefs.h $(SOURCE)\proto.h
	$(CC) -DUSE_WINGUICURSES $(CFLAGS) $(INCGUIW) $(SOURCE)\$(*B).c
the-guiw\comm1.obj:	$(SOURCE)\comm1.c $(SOURCE)\the.h $(SOURCE)\thedefs.h $(SOURCE)\proto.h
	$(CC) -DUSE_WINGUICURSES $(CFLAGS) $(INCGUIW) $(SOURCE)\$(*B).c
the-guiw\comm2.obj:	$(SOURCE)\comm2.c $(SOURCE)\the.h $(SOURCE)\thedefs.h $(SOURCE)\proto.h
	$(CC) -DUSE_WINGUICURSES $(CFLAGS) $(INCGUIW) $(SOURCE)\$(*B).c
the-guiw\comm3.obj:	$(SOURCE)\comm3.c $(SOURCE)\the.h $(SOURCE)\thedefs.h $(SOURCE)\proto.h
	$(CC) -DUSE_WINGUICURSES $(CFLAGS) $(INCGUIW) $(SOURCE)\$(*B).c
the-guiw\comm4.obj:	$(SOURCE)\comm4.c $(SOURCE)\the.h $(SOURCE)\thedefs.h $(SOURCE)\proto.h
	$(CC) -DUSE_WINGUICURSES $(CFLAGS) $(INCGUIW) $(SOURCE)\$(*B).c
the-guiw\comm5.obj:	$(SOURCE)\comm5.c $(SOURCE)\the.h $(SOURCE)\thedefs.h $(SOURCE)\proto.h
	$(CC) -DUSE_WINGUICURSES $(CFLAGS) $(INCGUIW) $(SOURCE)\$(*B).c
the-guiw\commset1.obj:	$(SOURCE)\commset1.c $(SOURCE)\the.h $(SOURCE)\thedefs.h $(SOURCE)\proto.h
	$(CC) -DUSE_WINGUICURSES $(CFLAGS) $(INCGUIW) $(SOURCE)\$(*B).c
the-guiw\commset2.obj:	$(SOURCE)\commset2.c $(SOURCE)\the.h $(SOURCE)\thedefs.h $(SOURCE)\proto.h
	$(CC) -DUSE_WINGUICURSES $(CFLAGS) $(INCGUIW) $(SOURCE)\$(*B).c
the-guiw\commsos.obj:	$(SOURCE)\commsos.c $(SOURCE)\the.h $(SOURCE)\thedefs.h $(SOURCE)\proto.h
	$(CC) -DUSE_WINGUICURSES $(CFLAGS) $(INCGUIW) $(SOURCE)\$(*B).c
the-guiw\cursor.obj:	$(SOURCE)\cursor.c $(SOURCE)\the.h $(SOURCE)\thedefs.h $(SOURCE)\proto.h
	$(CC) -DUSE_WINGUICURSES $(CFLAGS) $(INCGUIW) $(SOURCE)\$(*B).c
the-guiw\default.obj:	$(SOURCE)\default.c $(SOURCE)\the.h $(SOURCE)\thedefs.h $(SOURCE)\proto.h
	$(CC) -DUSE_WINGUICURSES $(CFLAGS) $(INCGUIW) $(SOURCE)\$(*B).c
the-guiw\edit.obj:	$(SOURCE)\edit.c $(SOURCE)\the.h $(SOURCE)\thedefs.h $(SOURCE)\proto.h
	$(CC) -DUSE_WINGUICURSES $(CFLAGS) $(INCGUIW) $(SOURCE)\$(*B).c
the-guiw\error.obj:	$(SOURCE)\error.c $(SOURCE)\the.h $(SOURCE)\thedefs.h $(SOURCE)\proto.h
	$(CC) -DUSE_WINGUICURSES $(CFLAGS) $(INCGUIW) $(SOURCE)\$(*B).c
the-guiw\execute.obj:	$(SOURCE)\execute.c $(SOURCE)\the.h $(SOURCE)\thedefs.h $(SOURCE)\proto.h
	$(CC) -DUSE_WINGUICURSES $(CFLAGS) $(INCGUIW) $(SOURCE)\$(*B).c
the-guiw\linked.obj:	$(SOURCE)\linked.c $(SOURCE)\the.h $(SOURCE)\thedefs.h $(SOURCE)\proto.h
	$(CC) -DUSE_WINGUICURSES $(CFLAGS) $(INCGUIW) $(SOURCE)\$(*B).c
the-guiw\column.obj:	$(SOURCE)\column.c $(SOURCE)\the.h $(SOURCE)\thedefs.h $(SOURCE)\proto.h
	$(CC) -DUSE_WINGUICURSES $(CFLAGS) $(INCGUIW) $(SOURCE)\$(*B).c
the-guiw\mouse.obj:	$(SOURCE)\mouse.c $(SOURCE)\the.h $(SOURCE)\thedefs.h $(SOURCE)\proto.h
	$(CC) -DUSE_WINGUICURSES $(CFLAGS) $(INCGUIW) $(SOURCE)\$(*B).c
the-guiw\memory.obj:	$(SOURCE)\memory.c $(SOURCE)\the.h $(SOURCE)\thedefs.h $(SOURCE)\proto.h
	$(CC) -DUSE_WINGUICURSES $(CFLAGS) $(INCGUIW) $(SOURCE)\$(*B).c
the-guiw\nonansi.obj:	$(SOURCE)\nonansi.c $(SOURCE)\the.h $(SOURCE)\thedefs.h $(SOURCE)\proto.h $(SOURCE)\directry.h
	$(CC) -DUSE_WINGUICURSES $(CFLAGS) $(INCGUIW) $(SOURCE)\$(*B).c
the-guiw\prefix.obj:	$(SOURCE)\prefix.c $(SOURCE)\the.h $(SOURCE)\thedefs.h $(SOURCE)\proto.h
	$(CC) -DUSE_WINGUICURSES $(CFLAGS) $(INCGUIW) $(SOURCE)\$(*B).c
the-guiw\regex.obj:	$(SOURCE)\regex.c $(SOURCE)\the.h $(SOURCE)\thedefs.h $(SOURCE)\proto.h
	$(CC) -DUSE_WINGUICURSES $(CFLAGS) $(INCGUIW) $(SOURCE)\$(*B).c
the-guiw\alloca.obj:	$(SOURCE)\alloca.c $(SOURCE)\the.h $(SOURCE)\thedefs.h $(SOURCE)\proto.h
	$(CC) -DUSE_WINGUICURSES $(CFLAGS) $(INCGUIW) $(SOURCE)\$(*B).c
the-guiw\parser.obj:	$(SOURCE)\parser.c $(SOURCE)\the.h $(SOURCE)\thedefs.h $(SOURCE)\proto.h
	$(CC) -DUSE_WINGUICURSES $(CFLAGS) $(INCGUIW) $(SOURCE)\$(*B).c
the-guiw\print.obj:	$(SOURCE)\print.c $(SOURCE)\the.h $(SOURCE)\thedefs.h $(SOURCE)\proto.h
	$(CC) -DUSE_WINGUICURSES $(CFLAGS) $(INCGUIW) $(SOURCE)\$(*B).c
the-guiw\reserved.obj:	$(SOURCE)\reserved.c $(SOURCE)\the.h $(SOURCE)\thedefs.h $(SOURCE)\proto.h
	$(CC) -DUSE_WINGUICURSES $(CFLAGS) $(INCGUIW) $(SOURCE)\$(*B).c
the-guiw\scroll.obj:	$(SOURCE)\scroll.c $(SOURCE)\the.h $(SOURCE)\thedefs.h $(SOURCE)\proto.h
	$(CC) -DUSE_WINGUICURSES $(CFLAGS) $(INCGUIW) $(SOURCE)\$(*B).c
the-guiw\show.obj:	$(SOURCE)\show.c $(SOURCE)\the.h $(SOURCE)\thedefs.h $(SOURCE)\proto.h
	$(CC) -DUSE_WINGUICURSES $(CFLAGS) $(INCGUIW) $(SOURCE)\$(*B).c
the-guiw\single.obj:	$(SOURCE)\single.c $(SOURCE)\the.h $(SOURCE)\thedefs.h $(SOURCE)\proto.h
	$(CC) -DUSE_WINGUICURSES $(CFLAGS) $(INCGUIW) $(SOURCE)\$(*B).c
the-guiw\sort.obj:	$(SOURCE)\sort.c $(SOURCE)\the.h $(SOURCE)\thedefs.h $(SOURCE)\proto.h
	$(CC) -DUSE_WINGUICURSES $(CFLAGS) $(INCGUIW) $(SOURCE)\$(*B).c
the-guiw\target.obj:	$(SOURCE)\target.c $(SOURCE)\the.h $(SOURCE)\thedefs.h $(SOURCE)\proto.h
	$(CC) -DUSE_WINGUICURSES $(CFLAGS) $(INCGUIW) $(SOURCE)\$(*B).c
the-guiw\the.obj:	$(SOURCE)\the.c $(SOURCE)\the.h $(SOURCE)\thedefs.h $(SOURCE)\proto.h $(SRC)\the.ver
	$(CC) -DUSE_WINGUICURSES $(CFLAGS) -DHAVE_PDC_SET_FUNCTION_KEY $(INCGUIW) $(VER_DEFS) $(SOURCE)\$(*B).c
the-guiw\util.obj:	$(SOURCE)\util.c $(SOURCE)\the.h $(SOURCE)\thedefs.h $(SOURCE)\proto.h
	$(CC) -DUSE_WINGUICURSES $(CFLAGS) $(INCGUIW) $(SOURCE)\$(*B).c
the-guiw\commutil.obj:	$(SOURCE)\commutil.c $(SOURCE)\the.h $(SOURCE)\command.h $(SOURCE)\thedefs.h $(SOURCE)\proto.h $(SOURCE)\getch.h $(SOURCE)\key.h
	$(CC) -DUSE_WINGUICURSES $(CFLAGS) $(INCGUIW) $(SOURCE)\$(*B).c
the-guiw\trace.obj:	$(SOURCE)\trace.c $(SOURCE)\the.h $(SOURCE)\command.h $(SOURCE)\thedefs.h $(SOURCE)\proto.h $(SOURCE)\getch.h $(SOURCE)\key.h
	$(CC) -DUSE_WINGUICURSES $(CFLAGS) $(INCGUIW) $(SOURCE)\$(*B).c
the-guiw\getch.obj:	$(SOURCE)\getch.c $(SOURCE)\getch.h $(SOURCE)\thedefs.h $(SOURCE)\proto.h
	$(CC) -DUSE_WINGUICURSES $(CFLAGS) $(INCGUIW) $(SOURCE)\$(*B).c
the-guiw\query.obj:	$(SOURCE)\query.c $(SOURCE)\query.h $(SOURCE)\the.h $(SOURCE)\thedefs.h $(SOURCE)\proto.h
	$(CC) -DUSE_WINGUICURSES $(CFLAGS) $(INCGUIW) $(SOURCE)\$(*B).c
the-guiw\query1.obj:	$(SOURCE)\query1.c $(SOURCE)\query.h $(SOURCE)\the.h $(SOURCE)\thedefs.h $(SOURCE)\proto.h
	$(CC) -DUSE_WINGUICURSES $(CFLAGS) $(INCGUIW) $(SOURCE)\$(*B).c
the-guiw\query2.obj:	$(SOURCE)\query2.c $(SOURCE)\query.h $(SOURCE)\the.h $(SOURCE)\thedefs.h $(SOURCE)\proto.h
	$(CC) -DUSE_WINGUICURSES $(CFLAGS) $(INCGUIW) -DTHE_VARIANT=\"GUIW\" $(SOURCE)\$(*B).c
the-guiw\thematch.obj:	$(SOURCE)\thematch.c $(SOURCE)\the.h $(SOURCE)\thematch.h
	$(CC) -DUSE_WINGUICURSES $(CFLAGS) $(INCGUIW) $(SOURCE)\$(*B).c
the-guiw\directry.obj:	$(SOURCE)\directry.c $(SOURCE)\the.h $(SOURCE)\directry.h $(SOURCE)\thedefs.h $(SOURCE)\proto.h
	$(CC) -DUSE_WINGUICURSES $(CFLAGS) $(INCGUIW) $(SOURCE)\$(*B).c
the-guiw\file.obj:	$(SOURCE)\file.c $(SOURCE)\the.h $(SOURCE)\directry.h $(SOURCE)\thedefs.h $(SOURCE)\proto.h
	$(CC) -DUSE_WINGUICURSES $(CFLAGS) $(INCGUIW) $(SOURCE)\$(*B).c
the-guiw\rexx.obj:	$(SOURCE)\rexx.c $(SOURCE)\the.h $(SOURCE)\therexx.h $(SOURCE)\proto.h $(SOURCE)\thedefs.h $(SOURCE)\query.h
	$(CC) -DUSE_WINGUICURSES $(CFLAGS) $(INCGUIW) $(SOURCE)\$(*B).c
the-guiw\mygetopt.obj:	$(SOURCE)\mygetopt.c
	$(CC) -DUSE_WINGUICURSES $(CFLAGS) $(INCGUIW) $(SOURCE)\$(*B).c
the-guiw\trace.obj:	$(SOURCE)\trace.c
	$(CC) -DUSE_WINGUICURSES $(CFLAGS) $(INCGUIW) $(SOURCE)\$(*B).c
the-guiw\DragAndDrop.obj:	$(SOURCE)\contrib\DragAndDrop.c
	$(CC) -DUSE_WINGUICURSES $(CFLAGS) $(INCGUIW) $(SOURCE)\contrib\$(*B).c
the-guiw\utf8.obj:	$(SOURCE)\contrib\utf8.c $(SOURCE)\contrib\utf8.h
	$(CC) -DUSE_WINGUICURSES $(CFLAGS) $(INCGUIW) $(SOURCE)\contrib\$(*B).c
#########################################################################
# Rules for building the SDL2 version of THE
$(CURSSDL2)\$(PDCURSES_LIB): pdc-sdl2w $(PDCSDL2WLIBOBJS) $(PDCSDL2WOBJS) $(DEFFILE)
	$(LIBEXE) -out:$@ $(PDCSDL2WLIBOBJS) $(PDCSDL2WOBJS)

the-sdl2w.exe: the-sdl2w $(CURSSDL2)\$(PDCURSES_LIB) $(THESDL2WOBJS) $(THERES) $(THEWINOBJ)
	$(LD) /subsystem:windows /NOLOGO /VERSION:$(VER_DOT) $(LDEBUG) $(THESDL2WOBJS) $(THEWINOBJ) $(SETARGV) -out:$*.exe $(LDEXTRA) $(CURSSDL2)\$(PDCURSES_LIB) $(REXXLIB) user32.lib gdi32.lib comdlg32.lib winspool.lib wsock32.lib shell32.lib advapi32.lib kernel32.lib winmm.lib  $(EXTRALIBSDL)
#------------------------------------------------------------------------
pdc-sdl2w\addch.obj: $(PDCSRC)\pdcurses\addch.c
	$(CC) $(PDCLIBCCFLAGS) $(INCSDL2W)  -c $(PDCSRC)\pdcurses\addch.c
pdc-sdl2w\addchstr.obj: $(PDCSRC)\pdcurses\addchstr.c
	$(CC) $(PDCLIBCCFLAGS) $(INCSDL2W)  -c $(PDCSRC)\pdcurses\addchstr.c
pdc-sdl2w\addstr.obj : $(PDCSRC)\pdcurses\addstr.c
	$(CC) $(PDCLIBCCFLAGS) $(INCSDL2W)  -c $(PDCSRC)\pdcurses\addstr.c
pdc-sdl2w\attr.obj : $(PDCSRC)\pdcurses\attr.c
	$(CC) $(PDCLIBCCFLAGS) $(INCSDL2W)  -c $(PDCSRC)\pdcurses\attr.c
pdc-sdl2w\beep.obj: $(PDCSRC)\pdcurses\beep.c
	$(CC) $(PDCLIBCCFLAGS) $(INCSDL2W)  -c $(PDCSRC)\pdcurses\beep.c
pdc-sdl2w\bkgd.obj : $(PDCSRC)\pdcurses\bkgd.c
	$(CC) $(PDCLIBCCFLAGS) $(INCSDL2W)  -c $(PDCSRC)\pdcurses\bkgd.c
pdc-sdl2w\border.obj : $(PDCSRC)\pdcurses\border.c
	$(CC) $(PDCLIBCCFLAGS) $(INCSDL2W)  -c $(PDCSRC)\pdcurses\border.c
pdc-sdl2w\clear.obj : $(PDCSRC)\pdcurses\clear.c
	$(CC) $(PDCLIBCCFLAGS) $(INCSDL2W)  -c $(PDCSRC)\pdcurses\clear.c
pdc-sdl2w\color.obj : $(PDCSRC)\pdcurses\color.c
	$(CC) $(PDCLIBCCFLAGS) $(INCSDL2W)  -c $(PDCSRC)\pdcurses\color.c
pdc-sdl2w\delch.obj : $(PDCSRC)\pdcurses\delch.c
	$(CC) $(PDCLIBCCFLAGS) $(INCSDL2W)  -c $(PDCSRC)\pdcurses\delch.c
pdc-sdl2w\deleteln.obj: $(PDCSRC)\pdcurses\deleteln.c
	$(CC) $(PDCLIBCCFLAGS) $(INCSDL2W)  -c $(PDCSRC)\pdcurses\deleteln.c
pdc-sdl2w\getch.obj : $(PDCSRC)\pdcurses\getch.c
	$(CC) $(PDCLIBCCFLAGS) $(INCSDL2W)  -c $(PDCSRC)\pdcurses\getch.c
pdc-sdl2w\getstr.obj : $(PDCSRC)\pdcurses\getstr.c
	$(CC) $(PDCLIBCCFLAGS) $(INCSDL2W)  -c $(PDCSRC)\pdcurses\getstr.c
pdc-sdl2w\getyx.obj : $(PDCSRC)\pdcurses\getyx.c
	$(CC) $(PDCLIBCCFLAGS) $(INCSDL2W)  -c $(PDCSRC)\pdcurses\getyx.c
pdc-sdl2w\inch.obj : $(PDCSRC)\pdcurses\inch.c
	$(CC) $(PDCLIBCCFLAGS) $(INCSDL2W)  -c $(PDCSRC)\pdcurses\inch.c
pdc-sdl2w\inchstr.obj: $(PDCSRC)\pdcurses\inchstr.c
	$(CC) $(PDCLIBCCFLAGS) $(INCSDL2W)  -c $(PDCSRC)\pdcurses\inchstr.c
pdc-sdl2w\initscr.obj : $(PDCSRC)\pdcurses\initscr.c
	$(CC) $(PDCLIBCCFLAGS) $(INCSDL2W)  -c $(PDCSRC)\pdcurses\initscr.c
pdc-sdl2w\inopts.obj : $(PDCSRC)\pdcurses\inopts.c
	$(CC) $(PDCLIBCCFLAGS) $(INCSDL2W)  -c $(PDCSRC)\pdcurses\inopts.c
pdc-sdl2w\insch.obj : $(PDCSRC)\pdcurses\insch.c
	$(CC) $(PDCLIBCCFLAGS) $(INCSDL2W)  -c $(PDCSRC)\pdcurses\insch.c
pdc-sdl2w\insstr.obj : $(PDCSRC)\pdcurses\insstr.c
	$(CC) $(PDCLIBCCFLAGS) $(INCSDL2W)  -c $(PDCSRC)\pdcurses\insstr.c
pdc-sdl2w\instr.obj : $(PDCSRC)\pdcurses\instr.c
	$(CC) $(PDCLIBCCFLAGS) $(INCSDL2W)  -c $(PDCSRC)\pdcurses\instr.c
pdc-sdl2w\kernel.obj: $(PDCSRC)\pdcurses\kernel.c
	$(CC) $(PDCLIBCCFLAGS) $(INCSDL2W)  -c $(PDCSRC)\pdcurses\kernel.c
pdc-sdl2w\keyname.obj : $(PDCSRC)\pdcurses\keyname.c
	$(CC) $(PDCLIBCCFLAGS) $(INCSDL2W)  -c $(PDCSRC)\pdcurses\keyname.c
pdc-sdl2w\mouse.obj : $(PDCSRC)\pdcurses\mouse.c
	$(CC) $(PDCLIBCCFLAGS) $(INCSDL2W)  -c $(PDCSRC)\pdcurses\mouse.c
pdc-sdl2w\move.obj : $(PDCSRC)\pdcurses\move.c
	$(CC) $(PDCLIBCCFLAGS) $(INCSDL2W)  -c $(PDCSRC)\pdcurses\move.c
pdc-sdl2w\outopts.obj : $(PDCSRC)\pdcurses\outopts.c
	$(CC) $(PDCLIBCCFLAGS) $(INCSDL2W)  -c $(PDCSRC)\pdcurses\outopts.c
pdc-sdl2w\overlay.obj : $(PDCSRC)\pdcurses\overlay.c
	$(CC) $(PDCLIBCCFLAGS) $(INCSDL2W)  -c $(PDCSRC)\pdcurses\overlay.c
pdc-sdl2w\pad.obj: $(PDCSRC)\pdcurses\pad.c
	$(CC) $(PDCLIBCCFLAGS) $(INCSDL2W)  -c $(PDCSRC)\pdcurses\pad.c
pdc-sdl2w\panel.obj : $(PDCSRC)\pdcurses\panel.c
	$(CC) $(PDCLIBCCFLAGS) $(INCSDL2W)  -c $(PDCSRC)\pdcurses\panel.c
pdc-sdl2w\printw.obj : $(PDCSRC)\pdcurses\printw.c
	$(CC) $(PDCLIBCCFLAGS) $(INCSDL2W)  -c $(PDCSRC)\pdcurses\printw.c
pdc-sdl2w\refresh.obj : $(PDCSRC)\pdcurses\refresh.c
	$(CC) $(PDCLIBCCFLAGS) $(INCSDL2W)  -c $(PDCSRC)\pdcurses\refresh.c
pdc-sdl2w\scanw.obj : $(PDCSRC)\pdcurses\scanw.c
	$(CC) $(PDCLIBCCFLAGS) $(INCSDL2W)  -c $(PDCSRC)\pdcurses\scanw.c
pdc-sdl2w\scr_dump.obj : $(PDCSRC)\pdcurses\scr_dump.c
	$(CC) $(PDCLIBCCFLAGS) $(INCSDL2W)  -c $(PDCSRC)\pdcurses\scr_dump.c
pdc-sdl2w\scroll.obj: $(PDCSRC)\pdcurses\scroll.c
	$(CC) $(PDCLIBCCFLAGS) $(INCSDL2W)  -c $(PDCSRC)\pdcurses\scroll.c
pdc-sdl2w\slk.obj : $(PDCSRC)\pdcurses\slk.c
	$(CC) $(PDCLIBCCFLAGS) $(INCSDL2W)  -c $(PDCSRC)\pdcurses\slk.c
pdc-sdl2w\termattr.obj : $(PDCSRC)\pdcurses\termattr.c
	$(CC) $(PDCLIBCCFLAGS) $(INCSDL2W)  -c $(PDCSRC)\pdcurses\termattr.c
pdc-sdl2w\touch.obj : $(PDCSRC)\pdcurses\touch.c
	$(CC) $(PDCLIBCCFLAGS) $(INCSDL2W)  -c $(PDCSRC)\pdcurses\touch.c
pdc-sdl2w\util.obj : $(PDCSRC)\pdcurses\util.c
	$(CC) $(PDCLIBCCFLAGS) $(INCSDL2W)  -c $(PDCSRC)\pdcurses\util.c
pdc-sdl2w\window.obj : $(PDCSRC)\pdcurses\window.c
	$(CC) $(PDCLIBCCFLAGS) $(INCSDL2W)  -c $(PDCSRC)\pdcurses\window.c
pdc-sdl2w\debug.obj: $(PDCSRC)\pdcurses\debug.c
	$(CC) $(PDCLIBCCFLAGS) $(INCSDL2W)  -c $(PDCSRC)\pdcurses\debug.c

pdc-sdl2w\pdcclip.obj: $(PDCSRC)\sdl2\pdcclip.c
	$(CC) $(PDCLIBCCFLAGS) $(INCSDL2W) $(INCSDLH) -c $(PDCSRC)\sdl2\pdcclip.c
pdc-sdl2w\pdcdisp.obj: $(PDCSRC)\sdl2\pdcdisp.c
	$(CC) $(PDCLIBCCFLAGS) $(INCSDL2W) $(INCSDLH) -c $(PDCSRC)\sdl2\pdcdisp.c
pdc-sdl2w\pdcgetsc.obj: $(PDCSRC)\sdl2\pdcgetsc.c
	$(CC) $(PDCLIBCCFLAGS) $(INCSDL2W) $(INCSDLH) -c $(PDCSRC)\sdl2\pdcgetsc.c
pdc-sdl2w\pdckbd.obj: $(PDCSRC)\sdl2\pdckbd.c
	$(CC) $(PDCLIBCCFLAGS) $(INCSDL2W) $(INCSDLH) -c $(PDCSRC)\sdl2\pdckbd.c
pdc-sdl2w\pdcscrn.obj: $(PDCSRC)\sdl2\pdcscrn.c
	$(CC) $(PDCLIBCCFLAGS) $(INCSDL2W) $(INCSDLH) -c $(PDCSRC)\sdl2\pdcscrn.c
pdc-sdl2w\pdcsetsc.obj: $(PDCSRC)\sdl2\pdcsetsc.c
	$(CC) $(PDCLIBCCFLAGS) $(INCSDL2W) $(INCSDLH) -c $(PDCSRC)\sdl2\pdcsetsc.c
pdc-sdl2w\pdcutil.obj: $(PDCSRC)\sdl2\pdcutil.c
	$(CC) $(PDCLIBCCFLAGS) $(INCSDL2W) $(INCSDLH) -c $(PDCSRC)\sdl2\pdcutil.c
#------------------------------------------------------------------------
the-sdl2w\box.obj:	$(SOURCE)\box.c $(SOURCE)\the.h $(SOURCE)\thedefs.h $(SOURCE)\proto.h
	$(CC) -DUSE_SDLCURSES $(CFLAGS) $(INCSDL2W) $(SOURCE)\$(*B).c
the-sdl2w\colour.obj:	$(SOURCE)\colour.c $(SOURCE)\the.h $(SOURCE)\thedefs.h $(SOURCE)\proto.h
	$(CC) -DUSE_SDLCURSES $(CFLAGS) $(INCSDL2W) $(SOURCE)\$(*B).c
the-sdl2w\comm1.obj:	$(SOURCE)\comm1.c $(SOURCE)\the.h $(SOURCE)\thedefs.h $(SOURCE)\proto.h
	$(CC) -DUSE_SDLCURSES $(CFLAGS) $(INCSDL2W) $(SOURCE)\$(*B).c
the-sdl2w\comm2.obj:	$(SOURCE)\comm2.c $(SOURCE)\the.h $(SOURCE)\thedefs.h $(SOURCE)\proto.h
	$(CC) -DUSE_SDLCURSES $(CFLAGS) $(INCSDL2W) $(SOURCE)\$(*B).c
the-sdl2w\comm3.obj:	$(SOURCE)\comm3.c $(SOURCE)\the.h $(SOURCE)\thedefs.h $(SOURCE)\proto.h
	$(CC) -DUSE_SDLCURSES $(CFLAGS) $(INCSDL2W) $(SOURCE)\$(*B).c
the-sdl2w\comm4.obj:	$(SOURCE)\comm4.c $(SOURCE)\the.h $(SOURCE)\thedefs.h $(SOURCE)\proto.h
	$(CC) -DUSE_SDLCURSES $(CFLAGS) $(INCSDL2W) $(SOURCE)\$(*B).c
the-sdl2w\comm5.obj:	$(SOURCE)\comm5.c $(SOURCE)\the.h $(SOURCE)\thedefs.h $(SOURCE)\proto.h
	$(CC) -DUSE_SDLCURSES $(CFLAGS) $(INCSDL2W) $(SOURCE)\$(*B).c
the-sdl2w\commset1.obj:	$(SOURCE)\commset1.c $(SOURCE)\the.h $(SOURCE)\thedefs.h $(SOURCE)\proto.h
	$(CC) -DUSE_SDLCURSES $(CFLAGS) $(INCSDL2W) $(SOURCE)\$(*B).c
the-sdl2w\commset2.obj:	$(SOURCE)\commset2.c $(SOURCE)\the.h $(SOURCE)\thedefs.h $(SOURCE)\proto.h
	$(CC) -DUSE_SDLCURSES $(CFLAGS) $(INCSDL2W) $(SOURCE)\$(*B).c
the-sdl2w\commsos.obj:	$(SOURCE)\commsos.c $(SOURCE)\the.h $(SOURCE)\thedefs.h $(SOURCE)\proto.h
	$(CC) -DUSE_SDLCURSES $(CFLAGS) $(INCSDL2W) $(SOURCE)\$(*B).c
the-sdl2w\cursor.obj:	$(SOURCE)\cursor.c $(SOURCE)\the.h $(SOURCE)\thedefs.h $(SOURCE)\proto.h
	$(CC) -DUSE_SDLCURSES $(CFLAGS) $(INCSDL2W) $(SOURCE)\$(*B).c
the-sdl2w\default.obj:	$(SOURCE)\default.c $(SOURCE)\the.h $(SOURCE)\thedefs.h $(SOURCE)\proto.h
	$(CC) -DUSE_SDLCURSES $(CFLAGS) $(INCSDL2W) $(SOURCE)\$(*B).c
the-sdl2w\edit.obj:	$(SOURCE)\edit.c $(SOURCE)\the.h $(SOURCE)\thedefs.h $(SOURCE)\proto.h
	$(CC) -DUSE_SDLCURSES $(CFLAGS) $(INCSDL2W) $(SOURCE)\$(*B).c
the-sdl2w\error.obj:	$(SOURCE)\error.c $(SOURCE)\the.h $(SOURCE)\thedefs.h $(SOURCE)\proto.h
	$(CC) -DUSE_SDLCURSES $(CFLAGS) $(INCSDL2W) $(SOURCE)\$(*B).c
the-sdl2w\execute.obj:	$(SOURCE)\execute.c $(SOURCE)\the.h $(SOURCE)\thedefs.h $(SOURCE)\proto.h
	$(CC) -DUSE_SDLCURSES $(CFLAGS) $(INCSDL2W) $(SOURCE)\$(*B).c
the-sdl2w\linked.obj:	$(SOURCE)\linked.c $(SOURCE)\the.h $(SOURCE)\thedefs.h $(SOURCE)\proto.h
	$(CC) -DUSE_SDLCURSES $(CFLAGS) $(INCSDL2W) $(SOURCE)\$(*B).c
the-sdl2w\column.obj:	$(SOURCE)\column.c $(SOURCE)\the.h $(SOURCE)\thedefs.h $(SOURCE)\proto.h
	$(CC) -DUSE_SDLCURSES $(CFLAGS) $(INCSDL2W) $(SOURCE)\$(*B).c
the-sdl2w\mouse.obj:	$(SOURCE)\mouse.c $(SOURCE)\the.h $(SOURCE)\thedefs.h $(SOURCE)\proto.h
	$(CC) -DUSE_SDLCURSES $(CFLAGS) $(INCSDL2W) $(SOURCE)\$(*B).c
the-sdl2w\memory.obj:	$(SOURCE)\memory.c $(SOURCE)\the.h $(SOURCE)\thedefs.h $(SOURCE)\proto.h
	$(CC) -DUSE_SDLCURSES $(CFLAGS) $(INCSDL2W) $(SOURCE)\$(*B).c
the-sdl2w\nonansi.obj:	$(SOURCE)\nonansi.c $(SOURCE)\the.h $(SOURCE)\thedefs.h $(SOURCE)\proto.h $(SOURCE)\directry.h
	$(CC) -DUSE_SDLCURSES $(CFLAGS) $(INCSDL2W) $(SOURCE)\$(*B).c
the-sdl2w\prefix.obj:	$(SOURCE)\prefix.c $(SOURCE)\the.h $(SOURCE)\thedefs.h $(SOURCE)\proto.h
	$(CC) -DUSE_SDLCURSES $(CFLAGS) $(INCSDL2W) $(SOURCE)\$(*B).c
the-sdl2w\regex.obj:	$(SOURCE)\regex.c $(SOURCE)\the.h $(SOURCE)\thedefs.h $(SOURCE)\proto.h
	$(CC) -DUSE_SDLCURSES $(CFLAGS) $(INCSDL2W) $(SOURCE)\$(*B).c
the-sdl2w\alloca.obj:	$(SOURCE)\alloca.c $(SOURCE)\the.h $(SOURCE)\thedefs.h $(SOURCE)\proto.h
	$(CC) -DUSE_SDLCURSES $(CFLAGS) $(INCSDL2W) $(SOURCE)\$(*B).c
the-sdl2w\parser.obj:	$(SOURCE)\parser.c $(SOURCE)\the.h $(SOURCE)\thedefs.h $(SOURCE)\proto.h
	$(CC) -DUSE_SDLCURSES $(CFLAGS) $(INCSDL2W) $(SOURCE)\$(*B).c
the-sdl2w\print.obj:	$(SOURCE)\print.c $(SOURCE)\the.h $(SOURCE)\thedefs.h $(SOURCE)\proto.h
	$(CC) -DUSE_SDLCURSES $(CFLAGS) $(INCSDL2W) $(SOURCE)\$(*B).c
the-sdl2w\reserved.obj:	$(SOURCE)\reserved.c $(SOURCE)\the.h $(SOURCE)\thedefs.h $(SOURCE)\proto.h
	$(CC) -DUSE_SDLCURSES $(CFLAGS) $(INCSDL2W) $(SOURCE)\$(*B).c
the-sdl2w\scroll.obj:	$(SOURCE)\scroll.c $(SOURCE)\the.h $(SOURCE)\thedefs.h $(SOURCE)\proto.h
	$(CC) -DUSE_SDLCURSES $(CFLAGS) $(INCSDL2W) $(SOURCE)\$(*B).c
the-sdl2w\show.obj:	$(SOURCE)\show.c $(SOURCE)\the.h $(SOURCE)\thedefs.h $(SOURCE)\proto.h
	$(CC) -DUSE_SDLCURSES $(CFLAGS) $(INCSDL2W) $(SOURCE)\$(*B).c
the-sdl2w\single.obj:	$(SOURCE)\single.c $(SOURCE)\the.h $(SOURCE)\thedefs.h $(SOURCE)\proto.h
	$(CC) -DUSE_SDLCURSES $(CFLAGS) $(INCSDL2W) $(SOURCE)\$(*B).c
the-sdl2w\sort.obj:	$(SOURCE)\sort.c $(SOURCE)\the.h $(SOURCE)\thedefs.h $(SOURCE)\proto.h
	$(CC) -DUSE_SDLCURSES $(CFLAGS) $(INCSDL2W) $(SOURCE)\$(*B).c
the-sdl2w\target.obj:	$(SOURCE)\target.c $(SOURCE)\the.h $(SOURCE)\thedefs.h $(SOURCE)\proto.h
	$(CC) -DUSE_SDLCURSES $(CFLAGS) $(INCSDL2W) $(SOURCE)\$(*B).c
the-sdl2w\the.obj:	$(SOURCE)\the.c $(SOURCE)\the.h $(SOURCE)\thedefs.h $(SOURCE)\proto.h $(SRC)\the.ver
	$(CC) -DUSE_SDLCURSES $(CFLAGS) -DHAVE_PDC_SET_FUNCTION_KEY $(INCSDL2W) -Dmain=SDL_main $(VER_DEFS) $(SOURCE)\$(*B).c
the-sdl2w\util.obj:	$(SOURCE)\util.c $(SOURCE)\the.h $(SOURCE)\thedefs.h $(SOURCE)\proto.h
	$(CC) -DUSE_SDLCURSES $(CFLAGS) $(INCSDL2W) $(SOURCE)\$(*B).c
the-sdl2w\commutil.obj:	$(SOURCE)\commutil.c $(SOURCE)\the.h $(SOURCE)\command.h $(SOURCE)\thedefs.h $(SOURCE)\proto.h $(SOURCE)\getch.h $(SOURCE)\key.h
	$(CC) -DUSE_SDLCURSES $(CFLAGS) $(INCSDL2W) $(SOURCE)\$(*B).c
the-sdl2w\trace.obj:	$(SOURCE)\trace.c $(SOURCE)\the.h $(SOURCE)\command.h $(SOURCE)\thedefs.h $(SOURCE)\proto.h $(SOURCE)\getch.h $(SOURCE)\key.h
	$(CC) -DUSE_SDLCURSES $(CFLAGS) $(INCSDL2W) $(SOURCE)\$(*B).c
the-sdl2w\getch.obj:	$(SOURCE)\getch.c $(SOURCE)\getch.h $(SOURCE)\thedefs.h $(SOURCE)\proto.h
	$(CC) -DUSE_SDLCURSES $(CFLAGS) $(INCSDL2W) $(SOURCE)\$(*B).c
the-sdl2w\query.obj:	$(SOURCE)\query.c $(SOURCE)\query.h $(SOURCE)\the.h $(SOURCE)\thedefs.h $(SOURCE)\proto.h
	$(CC) -DUSE_SDLCURSES $(CFLAGS) $(INCSDL2W) $(SOURCE)\$(*B).c
the-sdl2w\query1.obj:	$(SOURCE)\query1.c $(SOURCE)\query.h $(SOURCE)\the.h $(SOURCE)\thedefs.h $(SOURCE)\proto.h
	$(CC) -DUSE_SDLCURSES $(CFLAGS) $(INCSDL2W) $(SOURCE)\$(*B).c
the-sdl2w\query2.obj:	$(SOURCE)\query2.c $(SOURCE)\query.h $(SOURCE)\the.h $(SOURCE)\thedefs.h $(SOURCE)\proto.h
	$(CC) -DUSE_SDLCURSES $(CFLAGS) $(INCSDL2W) -DTHE_VARIANT=\"SDL2W\" $(SOURCE)\$(*B).c
the-sdl2w\thematch.obj:	$(SOURCE)\thematch.c $(SOURCE)\the.h $(SOURCE)\thematch.h
	$(CC) -DUSE_SDLCURSES $(CFLAGS) $(INCSDL2W) $(SOURCE)\$(*B).c
the-sdl2w\directry.obj:	$(SOURCE)\directry.c $(SOURCE)\the.h $(SOURCE)\directry.h $(SOURCE)\thedefs.h $(SOURCE)\proto.h
	$(CC) -DUSE_SDLCURSES $(CFLAGS) $(INCSDL2W) $(SOURCE)\$(*B).c
the-sdl2w\file.obj:	$(SOURCE)\file.c $(SOURCE)\the.h $(SOURCE)\directry.h $(SOURCE)\thedefs.h $(SOURCE)\proto.h
	$(CC) -DUSE_SDLCURSES $(CFLAGS) $(INCSDL2W) $(SOURCE)\$(*B).c
the-sdl2w\rexx.obj:	$(SOURCE)\rexx.c $(SOURCE)\the.h $(SOURCE)\therexx.h $(SOURCE)\proto.h $(SOURCE)\thedefs.h $(SOURCE)\query.h
	$(CC) -DUSE_SDLCURSES $(CFLAGS) $(INCSDL2W) $(SOURCE)\$(*B).c
the-sdl2w\mygetopt.obj:	$(SOURCE)\mygetopt.c
	$(CC) -DUSE_SDLCURSES $(CFLAGS) $(INCSDL2W) $(SOURCE)\$(*B).c
the-sdl2w\trace.obj:	$(SOURCE)\trace.c
	$(CC) -DUSE_SDLCURSES $(CFLAGS) $(INCSDL2W) $(SOURCE)\$(*B).c
the-sdl2w\DragAndDrop.obj:	$(SOURCE)\contrib\DragAndDrop.c
	$(CC) -DUSE_SDLCURSES $(CFLAGS) $(INCSDL2W) $(SOURCE)\contrib\$(*B).c
the-sdl2w\utf8.obj:	$(SOURCE)\contrib\utf8.c $(SOURCE)\contrib\utf8.h
	$(CC) -DUSE_SDLCURSES $(CFLAGS) $(INCSDL2W) $(SOURCE)\contrib\$(*B).c
#########################################################################
# Rules for building the CONSOLE version of THE
$(CURSCON)\$(PDCURSES_LIB): pdc-conw $(PDCCONWLIBOBJS) $(PDCCONWOBJS) $(DEFFILE)
	$(LIBEXE) -out:$@ $(PDCCONWLIBOBJS) $(PDCCONWOBJS)

the-conw.exe: the-conw $(CURSCON)\$(PDCURSES_LIB) $(THECONWOBJS) $(THERES) $(THEWINOBJ)
	$(LD) /NOLOGO /VERSION:$(VER_DOT) $(LDEBUG) $(THECONWOBJS) $(THEWINOBJ) $(SETARGV) -out:$*.exe $(LDEXTRA) $(CURSCON)\$(PDCURSES_LIB) $(REXXLIB) user32.lib gdi32.lib comdlg32.lib winspool.lib wsock32.lib shell32.lib advapi32.lib kernel32.lib winmm.lib  $(EXTRALIB)
#------------------------------------------------------------------------
pdc-conw\addch.obj: $(PDCSRC)\pdcurses\addch.c
	$(CC) $(PDCLIBCCFLAGS) $(INCCONW)  -c $(PDCSRC)\pdcurses\addch.c
pdc-conw\addchstr.obj: $(PDCSRC)\pdcurses\addchstr.c
	$(CC) $(PDCLIBCCFLAGS) $(INCCONW)  -c $(PDCSRC)\pdcurses\addchstr.c
pdc-conw\addstr.obj : $(PDCSRC)\pdcurses\addstr.c
	$(CC) $(PDCLIBCCFLAGS) $(INCCONW)  -c $(PDCSRC)\pdcurses\addstr.c
pdc-conw\attr.obj : $(PDCSRC)\pdcurses\attr.c
	$(CC) $(PDCLIBCCFLAGS) $(INCCONW)  -c $(PDCSRC)\pdcurses\attr.c
pdc-conw\beep.obj: $(PDCSRC)\pdcurses\beep.c
	$(CC) $(PDCLIBCCFLAGS) $(INCCONW)  -c $(PDCSRC)\pdcurses\beep.c
pdc-conw\bkgd.obj : $(PDCSRC)\pdcurses\bkgd.c
	$(CC) $(PDCLIBCCFLAGS) $(INCCONW)  -c $(PDCSRC)\pdcurses\bkgd.c
pdc-conw\border.obj : $(PDCSRC)\pdcurses\border.c
	$(CC) $(PDCLIBCCFLAGS) $(INCCONW)  -c $(PDCSRC)\pdcurses\border.c
pdc-conw\clear.obj : $(PDCSRC)\pdcurses\clear.c
	$(CC) $(PDCLIBCCFLAGS) $(INCCONW)  -c $(PDCSRC)\pdcurses\clear.c
pdc-conw\color.obj : $(PDCSRC)\pdcurses\color.c
	$(CC) $(PDCLIBCCFLAGS) $(INCCONW)  -c $(PDCSRC)\pdcurses\color.c
pdc-conw\delch.obj : $(PDCSRC)\pdcurses\delch.c
	$(CC) $(PDCLIBCCFLAGS) $(INCCONW)  -c $(PDCSRC)\pdcurses\delch.c
pdc-conw\deleteln.obj: $(PDCSRC)\pdcurses\deleteln.c
	$(CC) $(PDCLIBCCFLAGS) $(INCCONW)  -c $(PDCSRC)\pdcurses\deleteln.c
pdc-conw\getch.obj : $(PDCSRC)\pdcurses\getch.c
	$(CC) $(PDCLIBCCFLAGS) $(INCCONW)  -c $(PDCSRC)\pdcurses\getch.c
pdc-conw\getstr.obj : $(PDCSRC)\pdcurses\getstr.c
	$(CC) $(PDCLIBCCFLAGS) $(INCCONW)  -c $(PDCSRC)\pdcurses\getstr.c
pdc-conw\getyx.obj : $(PDCSRC)\pdcurses\getyx.c
	$(CC) $(PDCLIBCCFLAGS) $(INCCONW)  -c $(PDCSRC)\pdcurses\getyx.c
pdc-conw\inch.obj : $(PDCSRC)\pdcurses\inch.c
	$(CC) $(PDCLIBCCFLAGS) $(INCCONW)  -c $(PDCSRC)\pdcurses\inch.c
pdc-conw\inchstr.obj: $(PDCSRC)\pdcurses\inchstr.c
	$(CC) $(PDCLIBCCFLAGS) $(INCCONW)  -c $(PDCSRC)\pdcurses\inchstr.c
pdc-conw\initscr.obj : $(PDCSRC)\pdcurses\initscr.c
	$(CC) $(PDCLIBCCFLAGS) $(INCCONW)  -c $(PDCSRC)\pdcurses\initscr.c
pdc-conw\inopts.obj : $(PDCSRC)\pdcurses\inopts.c
	$(CC) $(PDCLIBCCFLAGS) $(INCCONW)  -c $(PDCSRC)\pdcurses\inopts.c
pdc-conw\insch.obj : $(PDCSRC)\pdcurses\insch.c
	$(CC) $(PDCLIBCCFLAGS) $(INCCONW)  -c $(PDCSRC)\pdcurses\insch.c
pdc-conw\insstr.obj : $(PDCSRC)\pdcurses\insstr.c
	$(CC) $(PDCLIBCCFLAGS) $(INCCONW)  -c $(PDCSRC)\pdcurses\insstr.c
pdc-conw\instr.obj : $(PDCSRC)\pdcurses\instr.c
	$(CC) $(PDCLIBCCFLAGS) $(INCCONW)  -c $(PDCSRC)\pdcurses\instr.c
pdc-conw\kernel.obj: $(PDCSRC)\pdcurses\kernel.c
	$(CC) $(PDCLIBCCFLAGS) $(INCCONW)  -c $(PDCSRC)\pdcurses\kernel.c
pdc-conw\keyname.obj : $(PDCSRC)\pdcurses\keyname.c
	$(CC) $(PDCLIBCCFLAGS) $(INCCONW)  -c $(PDCSRC)\pdcurses\keyname.c
pdc-conw\mouse.obj : $(PDCSRC)\pdcurses\mouse.c
	$(CC) $(PDCLIBCCFLAGS) $(INCCONW)  -c $(PDCSRC)\pdcurses\mouse.c
pdc-conw\move.obj : $(PDCSRC)\pdcurses\move.c
	$(CC) $(PDCLIBCCFLAGS) $(INCCONW)  -c $(PDCSRC)\pdcurses\move.c
pdc-conw\outopts.obj : $(PDCSRC)\pdcurses\outopts.c
	$(CC) $(PDCLIBCCFLAGS) $(INCCONW)  -c $(PDCSRC)\pdcurses\outopts.c
pdc-conw\overlay.obj : $(PDCSRC)\pdcurses\overlay.c
	$(CC) $(PDCLIBCCFLAGS) $(INCCONW)  -c $(PDCSRC)\pdcurses\overlay.c
pdc-conw\pad.obj: $(PDCSRC)\pdcurses\pad.c
	$(CC) $(PDCLIBCCFLAGS) $(INCCONW)  -c $(PDCSRC)\pdcurses\pad.c
pdc-conw\panel.obj : $(PDCSRC)\pdcurses\panel.c
	$(CC) $(PDCLIBCCFLAGS) $(INCCONW)  -c $(PDCSRC)\pdcurses\panel.c
pdc-conw\printw.obj : $(PDCSRC)\pdcurses\printw.c
	$(CC) $(PDCLIBCCFLAGS) $(INCCONW)  -c $(PDCSRC)\pdcurses\printw.c
pdc-conw\refresh.obj : $(PDCSRC)\pdcurses\refresh.c
	$(CC) $(PDCLIBCCFLAGS) $(INCCONW)  -c $(PDCSRC)\pdcurses\refresh.c
pdc-conw\scanw.obj : $(PDCSRC)\pdcurses\scanw.c
	$(CC) $(PDCLIBCCFLAGS) $(INCCONW)  -c $(PDCSRC)\pdcurses\scanw.c
pdc-conw\scr_dump.obj : $(PDCSRC)\pdcurses\scr_dump.c
	$(CC) $(PDCLIBCCFLAGS) $(INCCONW)  -c $(PDCSRC)\pdcurses\scr_dump.c
pdc-conw\scroll.obj: $(PDCSRC)\pdcurses\scroll.c
	$(CC) $(PDCLIBCCFLAGS) $(INCCONW)  -c $(PDCSRC)\pdcurses\scroll.c
pdc-conw\slk.obj : $(PDCSRC)\pdcurses\slk.c
	$(CC) $(PDCLIBCCFLAGS) $(INCCONW)  -c $(PDCSRC)\pdcurses\slk.c
pdc-conw\termattr.obj : $(PDCSRC)\pdcurses\termattr.c
	$(CC) $(PDCLIBCCFLAGS) $(INCCONW)  -c $(PDCSRC)\pdcurses\termattr.c
pdc-conw\touch.obj : $(PDCSRC)\pdcurses\touch.c
	$(CC) $(PDCLIBCCFLAGS) $(INCCONW)  -c $(PDCSRC)\pdcurses\touch.c
pdc-conw\util.obj : $(PDCSRC)\pdcurses\util.c
	$(CC) $(PDCLIBCCFLAGS) $(INCCONW)  -c $(PDCSRC)\pdcurses\util.c
pdc-conw\window.obj : $(PDCSRC)\pdcurses\window.c
	$(CC) $(PDCLIBCCFLAGS) $(INCCONW)  -c $(PDCSRC)\pdcurses\window.c
pdc-conw\debug.obj: $(PDCSRC)\pdcurses\debug.c
	$(CC) $(PDCLIBCCFLAGS) $(INCCONW)  -c $(PDCSRC)\pdcurses\debug.c

pdc-conw\pdcclip.obj: $(PDCSRC)\wincon\pdcclip.c
	$(CC) $(PDCLIBCCFLAGS) $(INCCONW)  -c $(PDCSRC)\wincon\pdcclip.c
pdc-conw\pdcdisp.obj: $(PDCSRC)\wincon\pdcdisp.c
	$(CC) $(PDCLIBCCFLAGS) $(INCCONW)  -c $(PDCSRC)\wincon\pdcdisp.c
pdc-conw\pdcgetsc.obj: $(PDCSRC)\wincon\pdcgetsc.c
	$(CC) $(PDCLIBCCFLAGS) $(INCCONW)  -c $(PDCSRC)\wincon\pdcgetsc.c
pdc-conw\pdckbd.obj: $(PDCSRC)\wincon\pdckbd.c
	$(CC) $(PDCLIBCCFLAGS) $(INCCONW)  -c $(PDCSRC)\wincon\pdckbd.c
pdc-conw\pdcscrn.obj: $(PDCSRC)\wincon\pdcscrn.c
	$(CC) $(PDCLIBCCFLAGS) $(INCCONW)  -c $(PDCSRC)\wincon\pdcscrn.c
pdc-conw\pdcsetsc.obj: $(PDCSRC)\wincon\pdcsetsc.c
	$(CC) $(PDCLIBCCFLAGS) $(INCCONW)  -c $(PDCSRC)\wincon\pdcsetsc.c
pdc-conw\pdcutil.obj: $(PDCSRC)\wincon\pdcutil.c
	$(CC) $(PDCLIBCCFLAGS) $(INCCONW)  -c $(PDCSRC)\wincon\pdcutil.c
#------------------------------------------------------------------------
the-conw\box.obj:	$(SOURCE)\box.c $(SOURCE)\the.h $(SOURCE)\thedefs.h $(SOURCE)\proto.h
	$(CC) $(CFLAGS) $(INCCONW) $(SOURCE)\$(*B).c
the-conw\colour.obj:	$(SOURCE)\colour.c $(SOURCE)\the.h $(SOURCE)\thedefs.h $(SOURCE)\proto.h
	$(CC) $(CFLAGS) $(INCCONW) $(SOURCE)\$(*B).c
the-conw\comm1.obj:	$(SOURCE)\comm1.c $(SOURCE)\the.h $(SOURCE)\thedefs.h $(SOURCE)\proto.h
	$(CC) $(CFLAGS) $(INCCONW) $(SOURCE)\$(*B).c
the-conw\comm2.obj:	$(SOURCE)\comm2.c $(SOURCE)\the.h $(SOURCE)\thedefs.h $(SOURCE)\proto.h
	$(CC) $(CFLAGS) $(INCCONW) $(SOURCE)\$(*B).c
the-conw\comm3.obj:	$(SOURCE)\comm3.c $(SOURCE)\the.h $(SOURCE)\thedefs.h $(SOURCE)\proto.h
	$(CC) $(CFLAGS) $(INCCONW) $(SOURCE)\$(*B).c
the-conw\comm4.obj:	$(SOURCE)\comm4.c $(SOURCE)\the.h $(SOURCE)\thedefs.h $(SOURCE)\proto.h
	$(CC) $(CFLAGS) $(INCCONW) $(SOURCE)\$(*B).c
the-conw\comm5.obj:	$(SOURCE)\comm5.c $(SOURCE)\the.h $(SOURCE)\thedefs.h $(SOURCE)\proto.h
	$(CC) $(CFLAGS) $(INCCONW) $(SOURCE)\$(*B).c
the-conw\commset1.obj:	$(SOURCE)\commset1.c $(SOURCE)\the.h $(SOURCE)\thedefs.h $(SOURCE)\proto.h
	$(CC) $(CFLAGS) $(INCCONW) $(SOURCE)\$(*B).c
the-conw\commset2.obj:	$(SOURCE)\commset2.c $(SOURCE)\the.h $(SOURCE)\thedefs.h $(SOURCE)\proto.h
	$(CC) $(CFLAGS) $(INCCONW) $(SOURCE)\$(*B).c
the-conw\commsos.obj:	$(SOURCE)\commsos.c $(SOURCE)\the.h $(SOURCE)\thedefs.h $(SOURCE)\proto.h
	$(CC) $(CFLAGS) $(INCCONW) $(SOURCE)\$(*B).c
the-conw\cursor.obj:	$(SOURCE)\cursor.c $(SOURCE)\the.h $(SOURCE)\thedefs.h $(SOURCE)\proto.h
	$(CC) $(CFLAGS) $(INCCONW) $(SOURCE)\$(*B).c
the-conw\default.obj:	$(SOURCE)\default.c $(SOURCE)\the.h $(SOURCE)\thedefs.h $(SOURCE)\proto.h
	$(CC) $(CFLAGS) $(INCCONW) $(SOURCE)\$(*B).c
the-conw\edit.obj:	$(SOURCE)\edit.c $(SOURCE)\the.h $(SOURCE)\thedefs.h $(SOURCE)\proto.h
	$(CC) $(CFLAGS) $(INCCONW) $(SOURCE)\$(*B).c
the-conw\error.obj:	$(SOURCE)\error.c $(SOURCE)\the.h $(SOURCE)\thedefs.h $(SOURCE)\proto.h
	$(CC) $(CFLAGS) $(INCCONW) $(SOURCE)\$(*B).c
the-conw\execute.obj:	$(SOURCE)\execute.c $(SOURCE)\the.h $(SOURCE)\thedefs.h $(SOURCE)\proto.h
	$(CC) $(CFLAGS) $(INCCONW) $(SOURCE)\$(*B).c
the-conw\linked.obj:	$(SOURCE)\linked.c $(SOURCE)\the.h $(SOURCE)\thedefs.h $(SOURCE)\proto.h
	$(CC) $(CFLAGS) $(INCCONW) $(SOURCE)\$(*B).c
the-conw\column.obj:	$(SOURCE)\column.c $(SOURCE)\the.h $(SOURCE)\thedefs.h $(SOURCE)\proto.h
	$(CC) $(CFLAGS) $(INCCONW) $(SOURCE)\$(*B).c
the-conw\mouse.obj:	$(SOURCE)\mouse.c $(SOURCE)\the.h $(SOURCE)\thedefs.h $(SOURCE)\proto.h
	$(CC) $(CFLAGS) $(INCCONW) $(SOURCE)\$(*B).c
the-conw\memory.obj:	$(SOURCE)\memory.c $(SOURCE)\the.h $(SOURCE)\thedefs.h $(SOURCE)\proto.h
	$(CC) $(CFLAGS) $(INCCONW) $(SOURCE)\$(*B).c
the-conw\nonansi.obj:	$(SOURCE)\nonansi.c $(SOURCE)\the.h $(SOURCE)\thedefs.h $(SOURCE)\proto.h $(SOURCE)\directry.h
	$(CC) $(CFLAGS) $(INCCONW) $(SOURCE)\$(*B).c
the-conw\prefix.obj:	$(SOURCE)\prefix.c $(SOURCE)\the.h $(SOURCE)\thedefs.h $(SOURCE)\proto.h
	$(CC) $(CFLAGS) $(INCCONW) $(SOURCE)\$(*B).c
the-conw\regex.obj:	$(SOURCE)\regex.c $(SOURCE)\the.h $(SOURCE)\thedefs.h $(SOURCE)\proto.h
	$(CC) $(CFLAGS) $(INCCONW) $(SOURCE)\$(*B).c
the-conw\alloca.obj:	$(SOURCE)\alloca.c $(SOURCE)\the.h $(SOURCE)\thedefs.h $(SOURCE)\proto.h
	$(CC) $(CFLAGS) $(INCCONW) $(SOURCE)\$(*B).c
the-conw\parser.obj:	$(SOURCE)\parser.c $(SOURCE)\the.h $(SOURCE)\thedefs.h $(SOURCE)\proto.h
	$(CC) $(CFLAGS) $(INCCONW) $(SOURCE)\$(*B).c
the-conw\print.obj:	$(SOURCE)\print.c $(SOURCE)\the.h $(SOURCE)\thedefs.h $(SOURCE)\proto.h
	$(CC) $(CFLAGS) $(INCCONW) $(SOURCE)\$(*B).c
the-conw\reserved.obj:	$(SOURCE)\reserved.c $(SOURCE)\the.h $(SOURCE)\thedefs.h $(SOURCE)\proto.h
	$(CC) $(CFLAGS) $(INCCONW) $(SOURCE)\$(*B).c
the-conw\scroll.obj:	$(SOURCE)\scroll.c $(SOURCE)\the.h $(SOURCE)\thedefs.h $(SOURCE)\proto.h
	$(CC) $(CFLAGS) $(INCCONW) $(SOURCE)\$(*B).c
the-conw\show.obj:	$(SOURCE)\show.c $(SOURCE)\the.h $(SOURCE)\thedefs.h $(SOURCE)\proto.h
	$(CC) $(CFLAGS) $(INCCONW) $(SOURCE)\$(*B).c
the-conw\single.obj:	$(SOURCE)\single.c $(SOURCE)\the.h $(SOURCE)\thedefs.h $(SOURCE)\proto.h
	$(CC) $(CFLAGS) $(INCCONW) $(SOURCE)\$(*B).c
the-conw\sort.obj:	$(SOURCE)\sort.c $(SOURCE)\the.h $(SOURCE)\thedefs.h $(SOURCE)\proto.h
	$(CC) $(CFLAGS) $(INCCONW) $(SOURCE)\$(*B).c
the-conw\target.obj:	$(SOURCE)\target.c $(SOURCE)\the.h $(SOURCE)\thedefs.h $(SOURCE)\proto.h
	$(CC) $(CFLAGS) $(INCCONW) $(SOURCE)\$(*B).c
the-conw\the.obj:	$(SOURCE)\the.c $(SOURCE)\the.h $(SOURCE)\thedefs.h $(SOURCE)\proto.h $(SRC)\the.ver
	$(CC) $(CFLAGS) $(INCCONW) -DHAVE_PDC_SET_FUNCTION_KEY $(VER_DEFS) $(SOURCE)\$(*B).c
the-conw\util.obj:	$(SOURCE)\util.c $(SOURCE)\the.h $(SOURCE)\thedefs.h $(SOURCE)\proto.h
	$(CC) $(CFLAGS) $(INCCONW) $(SOURCE)\$(*B).c
the-conw\commutil.obj:	$(SOURCE)\commutil.c $(SOURCE)\the.h $(SOURCE)\command.h $(SOURCE)\thedefs.h $(SOURCE)\proto.h $(SOURCE)\getch.h $(SOURCE)\key.h
	$(CC) $(CFLAGS) $(INCCONW) $(SOURCE)\$(*B).c
the-conw\trace.obj:	$(SOURCE)\trace.c $(SOURCE)\the.h $(SOURCE)\command.h $(SOURCE)\thedefs.h $(SOURCE)\proto.h $(SOURCE)\getch.h $(SOURCE)\key.h
	$(CC) $(CFLAGS) $(INCCONW) $(SOURCE)\$(*B).c
the-conw\getch.obj:	$(SOURCE)\getch.c $(SOURCE)\getch.h $(SOURCE)\thedefs.h $(SOURCE)\proto.h
	$(CC) $(CFLAGS) $(INCCONW) $(SOURCE)\$(*B).c
the-conw\query.obj:	$(SOURCE)\query.c $(SOURCE)\query.h $(SOURCE)\the.h $(SOURCE)\thedefs.h $(SOURCE)\proto.h
	$(CC) $(CFLAGS) $(INCCONW) $(SOURCE)\$(*B).c
the-conw\query1.obj:	$(SOURCE)\query1.c $(SOURCE)\query.h $(SOURCE)\the.h $(SOURCE)\thedefs.h $(SOURCE)\proto.h
	$(CC) $(CFLAGS) $(INCCONW) $(SOURCE)\$(*B).c
the-conw\query2.obj:	$(SOURCE)\query2.c $(SOURCE)\query.h $(SOURCE)\the.h $(SOURCE)\thedefs.h $(SOURCE)\proto.h
	$(CC) $(CFLAGS) $(INCCONW) -DTHE_VARIANT=\"CONW\" $(SOURCE)\$(*B).c
the-conw\thematch.obj:	$(SOURCE)\thematch.c $(SOURCE)\the.h $(SOURCE)\thematch.h
	$(CC) $(CFLAGS) $(INCCONW) $(SOURCE)\$(*B).c
the-conw\directry.obj:	$(SOURCE)\directry.c $(SOURCE)\the.h $(SOURCE)\directry.h $(SOURCE)\thedefs.h $(SOURCE)\proto.h
	$(CC) $(CFLAGS) $(INCCONW) $(SOURCE)\$(*B).c
the-conw\file.obj:	$(SOURCE)\file.c $(SOURCE)\the.h $(SOURCE)\directry.h $(SOURCE)\thedefs.h $(SOURCE)\proto.h
	$(CC) $(CFLAGS) $(INCCONW) $(SOURCE)\$(*B).c
the-conw\rexx.obj:	$(SOURCE)\rexx.c $(SOURCE)\the.h $(SOURCE)\therexx.h $(SOURCE)\proto.h $(SOURCE)\thedefs.h $(SOURCE)\query.h
	$(CC) $(CFLAGS) $(INCCONW) $(SOURCE)\$(*B).c
the-conw\mygetopt.obj:	$(SOURCE)\mygetopt.c
	$(CC) $(CFLAGS) $(INCCONW) $(SOURCE)\$(*B).c
the-conw\trace.obj:	$(SOURCE)\trace.c
	$(CC) $(CFLAGS) $(INCCONW) $(SOURCE)\$(*B).c
the-conw\DragAndDrop.obj:	$(SOURCE)\contrib\DragAndDrop.c
	$(CC) $(CFLAGS) $(INCCONW) $(SOURCE)\contrib\$(*B).c
the-conw\utf8.obj:	$(SOURCE)\contrib\utf8.c $(SOURCE)\contrib\utf8.h
	$(CC) $(CFLAGS) $(INCCONW) $(SOURCE)\contrib\$(*B).c

$(THERES) $(THEWINOBJ): $(THERC)
	copy $(SRC)\thewin.ico
	rc /r /fo$(THERES) /i$(SRC) $(THERC)
	cvtres /MACHINE:$(MACH) /NOLOGO /OUT:$(THEWINOBJ) $(THERES)
#
#########################################################################
manual:	$(MAN) $(SRC)\overview $(COMM) $(APPENDIX) $(GLOSSARY)
	manext $(SRC)\overview $(COMM) $(APPENDIX) $(GLOSSARY) > the.man
#
$(MAN):	$(XTRAOBJ) manext.$(OBJ)
	$(MANLD)
	$(CHMODMAN)

dist: the.exe the-guiw.exe the-conw.exe the-sdl2w.exe
	-mkdir tmpdir
	cd tmpdir
	-del /Q *.*
	copy ..\the.exe .
	copy ..\the-conw.exe .
	copy ..\the-guiw.exe .
	copy ..\the-sdl2w.exe .

#	copy $(BINDIR)\SDL2_ttf.dll .
#	copy $(BINDIR)\SDL2.dll .
#	copy $(BINDIR)\zlib1.dll .
#	copy $(BINDIR)\libfreetype-6.dll .

	copy $(VCPKG_HOME)\bin\SDL2_ttf.dll .
	copy $(VCPKG_HOME)\bin\SDL2.dll .
	copy $(VCPKG_HOME)\bin\freetype.dll .
	copy $(VCPKG_HOME)\bin\zlib1.dll
	copy $(VCPKG_HOME)\bin\bz2.dll
	copy $(VCPKG_HOME)\bin\libpng16.dll
	copy $(VCPKG_HOME)\bin\brotlidec.dll
	copy $(VCPKG_HOME)\bin\brotlicommon.dll

	copy $(SRC)\macros\append.the .
	copy $(SRC)\macros\codecomp.the .
	copy $(SRC)\macros\comm.the .
	copy $(SRC)\macros\compile.the .
	copy $(SRC)\macros\complete.the .
	copy $(SRC)\macros\completer.the .
	copy $(SRC)\macros\cua.the .
	copy $(SRC)\macros\demo.the .
	copy $(SRC)\macros\demo.txt .
	copy $(SRC)\macros\diff.the .
	copy $(SRC)\macros\fold.the .
	copy $(SRC)\macros\l.the .
	copy $(SRC)\macros\match.the .
	copy $(SRC)\macros\mhprf.the .
	copy $(SRC)\macros\nl.the .
	copy $(SRC)\macros\remote.the .
	copy $(SRC)\macros\rm.the .
	copy $(SRC)\macros\spell.the .
	copy $(SRC)\macros\syntax.the .
	copy $(SRC)\macros\tags.the .
	copy $(SRC)\macros\total.the .
	copy $(SRC)\macros\uncomm.the .
	copy $(SRC)\macros\words.the .
	copy $(SRC)\macros\xeditprf.the .

	copy $(SRC)\syntax\rexx.syntax .
	copy $(SRC)\syntax\rexxdw.syntax .
	copy $(SRC)\syntax\rexxeec.syntax .
	copy $(SRC)\syntax\rexxutil.syntax .

	copy $(SRC)\abf.tld
	copy $(SRC)\c.tld
	copy $(SRC)\cdiff.tld
	copy $(SRC)\cobol.tld
	copy $(SRC)\csh.tld
	copy $(SRC)\diff.tld
	copy $(SRC)\dir.tld
	copy $(SRC)\fortran.tld
	copy $(SRC)\html.tld
	copy $(SRC)\java.tld
	copy $(SRC)\js.tld
	copy $(SRC)\m4.tld
	copy $(SRC)\make.tld
	copy $(SRC)\nsi.tld
	copy $(SRC)\objc.tld
	copy $(SRC)\opl.tld
	copy $(SRC)\php.tld
	copy $(SRC)\plsql.tld
	copy $(SRC)\rexx.tld
	copy $(SRC)\rsp.tld
	copy $(SRC)\sh.tld
	copy $(SRC)\spec.tld
	copy $(SRC)\udiff.tld

	copy $(SRC)\README .
	copy $(SRC)\COPYING .
	copy $(SRC)\HISTORY .
	copy $(SRC)\INSTALL.Win .\INSTALL.txt
	copy $(SRC)\TODO .
!if "$(INT)" == "REXXTRANS"
	copy $(REXXTRANSBIN)\rexxtrans.dll .
!endif
!if "$(INT)" == "REGINA"
	copy $(REGINA_BINDIR)\regina.dll .
!endif
	-copy $(PDCURSESBIN)\pdcurses.dll .
	copy $(SRC)\THE_Help.txt .
#	copy $(SRC)\win32.diz file_id.diz
	copy $(SRC)\doc\THE-$(VER_DOT).pdf THE.pdf

	copy $(SRC)\images\mh256.bmp .\the256.bmp

#	the -b -p $(SRC)\fix.diz -a "$(VER) $(VER_DOT) T any available Rexx interpreter" file_id.diz
	zip THE$(VER)$(LARCH)_$(REXXINT) *
	copy $(SRC)\the.nsi .
	makensis $(CURSESDLLINNSIS) /DVERSION=$(VER_DOT) /DNODOTVER=$(VER) /DSRCDIR=$(SRC) /DARCH=$(LARCH) /DMACH=$(MACH) /DINTERPRETER=$(REXXINT) the.nsi
	cd ..

clean:
	-del the-sdl2w\*.obj
	-del the-guiw\*.obj
	-del the-conw\*.obj
	-del pdc-sdl2w\*.obj
	-del pdc-guiw\*.obj
	-del pdc-conw\*.obj
	-del the-guiw.exe
	-del the-conw.exe
	-del the-sdl2w.exe
	-del the.exe

the-guiw:
	-mkdir the-guiw

the-conw:
	-mkdir the-conw

the-sdl2w:
	-mkdir the-sdl2w

pdc-guiw:
	-mkdir pdc-guiw

pdc-conw:
	-mkdir pdc-conw

pdc-sdl2w:
	-mkdir pdc-sdl2w

