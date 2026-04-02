#
#########################################################################
#
# makefile for The Hessling Editor (THE)
# Innotek GCC OS/2
#
# You need the following environment variable set like:
# THE_SRCDIR=c:\the
# PDCURSES_SRCDIR=c:\pdcurses
# PDCURSES_BINDIR=c:\pdcurses
# REXXTRANS_SRCDIR=c:\rexxtrans
# REXXTRANS_BINDIR=c:\rexxtrans\gcc
#
#########################################################################
#

ifeq ($(AOUT),Y)
LIBEXT=a
OBJCFLAG=
OBJLDFLAG=
OBJLDFLAGOPT=
OBJTYPE=o
BIND=emxbind the
LXLITE=
else
LIBEXT=lib
OBJCFLAG=-Zomf
OBJLDFLAG=-Zomf
OBJLDFLAGOPT=-s
OBJTYPE=obj
BIND=
LXLITE=lxlite $@
endif

PDCURSESLIB = pdc-obj/pdcurses.lib

ifndef $(CURSES)
CURSES=CON
endif

ifndef $(INT)
INT=OS2REXX
endif

ifeq ($(CURSES),CON)
PDC_DIR=os2
THE_BINARY=the-con.exe
PDCCCFLAGS = -I$(SOURCE)/PDCursesMod
LIBCCFLAGS = -I$(SOURCE)/PDCursesMod
CURSINC   = -I$(SOURCE)/PDCursesMod
CURSLIB   = $(PDCURSESLIB)
endif

ifeq ($(CURSES),CONW)
PDC_DIR=os2
THE_BINARY=the-conw.exe
PDCCCFLAGS = -I$(SOURCE)/PDCursesMod -DPDC_WIDE
LIBCCFLAGS = -I$(SOURCE)/PDCursesMod -DPDC_WIDE
CURSINC   = -I$(SOURCE)/PDCursesMod -DUSE_WIDE_CHAR -DPDC_WIDE
CURSLIB   = $(PDCURSESLIB)
endif

ifeq ($(CURSES),SDL1)
PDC_DIR=sdl1
THE_BINARY = the-sdl1.exe
PDCCCFLAGS = -I$(SOURCE)/PDCursesMod -I/@unixroot/usr/include/SDL
LIBCCFLAGS = -I$(SOURCE)/PDCursesMod
CURSINC   = -I$(SOURCE)/PDCursesMod -DUSE_SDLCURSES
CURSLIB   = $(PDCURSESLIB) -lSDL
endif

ifeq ($(CURSES),SDL1W)
PDC_DIR=sdl1
THE_BINARY = the-sdl1w.exe
PDCCCFLAGS = -I$(SOURCE)/PDCursesMod -I/@unixroot/usr/include/SDL -DPDC_WIDE
LIBCCFLAGS = -I$(SOURCE)/PDCursesMod -DPDC_WIDE
CURSINC   = -I$(SOURCE)/PDCursesMod -DUSE_SDLCURSES -DUSE_WIDE_CHAR -DPDC_WIDE
CURSLIB   = $(PDCURSESLIB) -lSDL_ttf -lSDL
endif

ifeq ($(CURSES),SDL2W)
PDC_DIR=sdl2
THE_BINARY = the-sdl2w.exe
PDCCCFLAGS = -I$(SOURCE)/PDCursesMod -I/@unixroot/usr/include/SDL2 -DPDC_WIDE
LIBCCFLAGS = -I$(SOURCE)/PDCursesMod -DPDC_WIDE
CURSINC   = -I$(SOURCE)/PDCursesMod -DUSE_SDLCURSES -DUSE_WIDE_CHAR -DPDC_WIDE
CURSLIB   = $(PDCURSESLIB) -lSDL2_ttf -lSDL2
endif

SRC       = $(THE_SRCDIR)
SOURCE    = $(THE_SRCDIR)/src
CURSBIN   = $(PDC_DIR)


REGINA_REXXLIBS = $(REGINA_BINDIR)\regina.$(LIBEXT)
REGINA_REXXINC = -I$(REGINA_SRCDIR) -DUSE_REGINA
OS2REXX_REXXINC = -DUSE_OS2REXX
REXXTRANS_REXXLIBS = $(REXXTRANS_BINDIR)\rexxtrans.$(LIBEXT)
REXXTRANS_REXXINC = -I$(REXXTRANS_SRCDIR) -DUSE_REXXTRANS

include $(SRC)\the.ver

docdir = $(SRC)\doc
HTML_EXT = .html

COMM = \
	$(SRC)/src/comm1.c \
	$(SRC)/src/comm2.c \
	$(SRC)/src/comm3.c \
	$(SRC)/src/comm4.c \
	$(SRC)/src/comm5.c

COMMSOS = \
	$(SRC)/src/commsos.c

COMMSET = \
	$(SRC)/src/commset1.c \
	$(SRC)/src/commset2.c

QUERY = $(SRC)/src/query.c

APPENDIX1 = $(SRC)/appendix.1
APPENDIX2 = $(SRC)/appendix.2
APPENDIX3 = $(SRC)/appendix.3
APPENDIX4 = $(SRC)/appendix.4
APPENDIX7 = $(SRC)/appendix.7
APPENDIX = $(APPENDIX1) $(APPENDIX2) $(APPENDIX3) $(APPENDIX4) $(APPENDIX7)

GLOSSARY = $(SRC)/glossary
OVERVIEW = $(SRC)/overview

#########################################################################
# EMX compiler on OS/2
#########################################################################

ifeq ($(INT),OS2REXX)
REXXLIB = $(OS2REXX_REXXLIBS)
REXXINC =  $(OS2REXX_REXXINC)
endif
ifeq ($(INT),REXXTRANS)
REXXLIB = $(REXXTRANS_REXXLIBS)
REXXINC =  $(REXXTRANS_REXXINC)
endif
ifeq ($(INT),REGINA)
REXXLIB = $(REGINA_REXXLIBS)
REXXINC =  $(REGINA_REXXINC)
endif

ifeq ($(DEBUG),Y)
CFLAGS    = $(OBJCFLAG) -c -g -Wall -DEMX -DTHE_TRACE -DNOVIO -DSTDC_HEADERS -DHAVE_PROTO -I. -I$(SOURCE) $(CURSINC) $(REXXINC) -o$@
LDEBUG    = -g
TRACE     = the-obj\trace.$(OBJTYPE)
LXLITE    =
DEBUG     = N
else
CFLAGS    = $(OBJCFLAG) -c -O3 -fomit-frame-pointer -Wall -DEMX -DNOVIO -DSTDC_HEADERS -DHAVE_PROTO -I. -I$(SOURCE) $(CURSINC) $(REXXINC) -o$@
LDEBUG    = $(OBJLDFLAGOPT)
TRACE     =
endif

LDFLAGS   = $(OBJLDFLAG) $(LDEBUG)

CC        = gcc
LDTHE     = $(CC) $(LDFLAGS) -o $@ $(OBJS) $(CURSLIB) $(REXXLIB) the.def
LDEXECTHE = $(CC) $(LDFLAGS) -o $@ $(OBJEXECTHE)
RCTHE     = rc $(SRC)\theos2.rc $@
LIBEXE    = ar
LIBFLAGS  = rcv
#########################################################################
#
#
# Object files
#
OBJ1A = the-obj\util.$(OBJTYPE) the-obj\box.$(OBJTYPE) the-obj\colour.$(OBJTYPE) the-obj\comm1.$(OBJTYPE) the-obj\comm2.$(OBJTYPE) the-obj\comm3.$(OBJTYPE) the-obj\comm4.$(OBJTYPE) the-obj\comm5.$(OBJTYPE)
OBJ1B = the-obj\commset1.$(OBJTYPE) the-obj\commset2.$(OBJTYPE) the-obj\commsos.$(OBJTYPE) the-obj\cursor.$(OBJTYPE) the-obj\default.$(OBJTYPE)
OBJ1C = the-obj\edit.$(OBJTYPE) the-obj\error.$(OBJTYPE) the-obj\execute.$(OBJTYPE) the-obj\linked.$(OBJTYPE) the-obj\column.$(OBJTYPE) the-obj\mouse.$(OBJTYPE)
OBJ1D = the-obj\nonansi.$(OBJTYPE) the-obj\prefix.$(OBJTYPE) the-obj\reserved.$(OBJTYPE) the-obj\scroll.$(OBJTYPE) the-obj\show.$(OBJTYPE) the-obj\sort.$(OBJTYPE)
OBJ1E = the-obj\memory.$(OBJTYPE) the-obj\target.$(OBJTYPE) the-obj\the.$(OBJTYPE) the-obj\parser.$(OBJTYPE)
OBJ1 = $(OBJ1A) $(OBJ1B) $(OBJ1C) $(OBJ1D) $(OBJ1E)
OBJ2 = the-obj\commutil.$(OBJTYPE) the-obj\print.$(OBJTYPE) $(TRACE)
OBJ3 = the-obj\getch.$(OBJTYPE)
OBJ4 = the-obj\query.$(OBJTYPE) the-obj\query1.$(OBJTYPE) the-obj\query2.$(OBJTYPE)
OBJ5 = the-obj\thematch.$(OBJTYPE) the-obj\regex.$(OBJTYPE) the-obj\mygetopt.$(OBJTYPE)
OBJ6 = the-obj\directry.$(OBJTYPE) the-obj\file.$(OBJTYPE)
OBJ7 = the-obj\rexx.$(OBJTYPE)
OBJ8 = the-obj\os2eas.$(OBJTYPE)
OBJOTH = $(OBJ2) $(OBJ3) $(OBJ4) $(OBJ5) $(OBJ6) $(OBJ7) $(OBJ8)
OBJS = $(OBJ1) $(OBJ2) $(OBJ3) $(OBJ4) $(OBJ5) $(OBJ6) $(OBJ7) $(OBJ8)
OBJEXECTHE = the-obj\execthe.$(OBJTYPE) the-obj\mygetopt.$(OBJTYPE)

PDCLIBOBJS = pdc-obj\addch.$(OBJTYPE) pdc-obj\addchstr.$(OBJTYPE) pdc-obj\addstr.$(OBJTYPE) pdc-obj\attr.$(OBJTYPE) pdc-obj\beep.$(OBJTYPE) \
pdc-obj\bkgd.$(OBJTYPE) pdc-obj\border.$(OBJTYPE) pdc-obj\clear.$(OBJTYPE) pdc-obj\color.$(OBJTYPE) pdc-obj\delch.$(OBJTYPE) pdc-obj\deleteln.$(OBJTYPE) \
pdc-obj\getch.$(OBJTYPE) pdc-obj\getstr.$(OBJTYPE) pdc-obj\getyx.$(OBJTYPE) pdc-obj\inch.$(OBJTYPE) pdc-obj\inchstr.$(OBJTYPE) \
pdc-obj\initscr.$(OBJTYPE) pdc-obj\inopts.$(OBJTYPE) pdc-obj\insch.$(OBJTYPE) pdc-obj\insstr.$(OBJTYPE) pdc-obj\instr.$(OBJTYPE) pdc-obj\kernel.$(OBJTYPE) \
pdc-obj\keyname.$(OBJTYPE) pdc-obj\mouse.$(OBJTYPE) pdc-obj\move.$(OBJTYPE) pdc-obj\outopts.$(OBJTYPE) pdc-obj\overlay.$(OBJTYPE) pdc-obj\pad.$(OBJTYPE) \
pdc-obj\panel.$(OBJTYPE) pdc-obj\printw.$(OBJTYPE) pdc-obj\refresh.$(OBJTYPE) pdc-obj\scanw.$(OBJTYPE) pdc-obj\scr_dump.$(OBJTYPE) pdc-obj\scroll.$(OBJTYPE) \
pdc-obj\slk.$(OBJTYPE) pdc-obj\termattr.$(OBJTYPE) pdc-obj\touch.$(OBJTYPE) pdc-obj\util.$(OBJTYPE) pdc-obj\window.$(OBJTYPE) pdc-obj\debug.$(OBJTYPE)

PDCOBJS = pdc-obj\pdcclip.$(OBJTYPE) pdc-obj\pdcdisp.$(OBJTYPE) pdc-obj\pdcgetsc.$(OBJTYPE) pdc-obj\pdckbd.$(OBJTYPE) pdc-obj\pdcscrn.$(OBJTYPE) \
pdc-obj\pdcsetsc.$(OBJTYPE) pdc-obj\pdcutil.$(OBJTYPE)

all: ./pdc-obj ./the-obj how thever.h $(THE_BINARY) the.exe dist

how:
	echo make -f $(SRC)/gccos2.mak DEBUG=$(DEBUG) INT=$(INT) CURSES=$(CURSES) > rebuild.cmd
#
#########################################################################

./pdc-obj:
	mkdir pdc-obj

$(PDCURSESLIB): ./pdc-obj $(PDCLIBOBJS) $(PDCOBJS) $(DEFFILE)
	$(LIBEXE) $(LIBFLAGS) $(PDCURSESLIB) $(PDCLIBOBJS) $(PDCOBJS)

./the-obj:
	mkdir the-obj

the.exe: thever.h $(OBJEXECTHE) theos2.ico
	$(LDEXECTHE)
	$(BIND)
	$(RCTHE)
	$(LXLITE)

the-con.exe the-conw.exe the-sdl1.exe the-sdl1w.exe the-sdl2w.exe: thever.h $(PDCURSESLIB) the-obj $(OBJS) $(XTRA_OBJS) theos2.ico
#	echo NAME THE-$(PDC) WINDOWAPI > the.def
	echo NAME THE-$(PDC) WINDOWCOMPAT > the.def
	$(LDTHE)
	$(BIND)
	$(RCTHE)
	$(LXLITE)

theos2.ico:
	cp $(SRC)\theos2.ico .

thever.h: $(SRC)\the.ver
	$(SRC)\ver2verh.cmd $< $(CURSES)

#########################################################################
the-obj\box.$(OBJTYPE):	$(SOURCE)\box.c $(SOURCE)\the.h $(SOURCE)\thedefs.h $(SOURCE)\proto.h
	$(CC) $(CFLAGS) $(SOURCE)\box.c
the-obj\colour.$(OBJTYPE):	$(SOURCE)\colour.c $(SOURCE)\the.h $(SOURCE)\thedefs.h $(SOURCE)\proto.h
	$(CC) $(CFLAGS) $(SOURCE)\colour.c
the-obj\comm1.$(OBJTYPE):	$(SOURCE)\comm1.c $(SOURCE)\the.h $(SOURCE)\thedefs.h $(SOURCE)\proto.h
	$(CC) $(CFLAGS) $(SOURCE)\comm1.c
the-obj\comm2.$(OBJTYPE):	$(SOURCE)\comm2.c $(SOURCE)\the.h $(SOURCE)\thedefs.h $(SOURCE)\proto.h
	$(CC) $(CFLAGS) $(SOURCE)\comm2.c
the-obj\comm3.$(OBJTYPE):	$(SOURCE)\comm3.c $(SOURCE)\the.h $(SOURCE)\thedefs.h $(SOURCE)\proto.h
	$(CC) $(CFLAGS) $(SOURCE)\comm3.c
the-obj\comm4.$(OBJTYPE):	$(SOURCE)\comm4.c $(SOURCE)\the.h $(SOURCE)\thedefs.h $(SOURCE)\proto.h
	$(CC) $(CFLAGS) $(SOURCE)\comm4.c
the-obj\comm5.$(OBJTYPE):	$(SOURCE)\comm5.c $(SOURCE)\the.h $(SOURCE)\thedefs.h $(SOURCE)\proto.h
	$(CC) $(CFLAGS) $(SOURCE)\comm5.c
the-obj\commset1.$(OBJTYPE):	$(SOURCE)\commset1.c $(SOURCE)\the.h $(SOURCE)\thedefs.h $(SOURCE)\proto.h
	$(CC) $(CFLAGS) $(SOURCE)\commset1.c
the-obj\commset2.$(OBJTYPE):	$(SOURCE)\commset2.c $(SOURCE)\the.h $(SOURCE)\thedefs.h $(SOURCE)\proto.h
	$(CC) $(CFLAGS) $(SOURCE)\commset2.c
the-obj\commsos.$(OBJTYPE):	$(SOURCE)\commsos.c $(SOURCE)\the.h $(SOURCE)\thedefs.h $(SOURCE)\proto.h
	$(CC) $(CFLAGS) $(SOURCE)\commsos.c
the-obj\cursor.$(OBJTYPE):	$(SOURCE)\cursor.c $(SOURCE)\the.h $(SOURCE)\thedefs.h $(SOURCE)\proto.h
	$(CC) $(CFLAGS) $(SOURCE)\cursor.c
the-obj\default.$(OBJTYPE):	$(SOURCE)\default.c $(SOURCE)\the.h $(SOURCE)\thedefs.h $(SOURCE)\proto.h
	$(CC) $(CFLAGS) $(SOURCE)\default.c
the-obj\edit.$(OBJTYPE):	$(SOURCE)\edit.c $(SOURCE)\the.h $(SOURCE)\thedefs.h $(SOURCE)\proto.h
	$(CC) $(CFLAGS) $(SOURCE)\edit.c
the-obj\error.$(OBJTYPE):	$(SOURCE)\error.c $(SOURCE)\the.h $(SOURCE)\thedefs.h $(SOURCE)\proto.h
	$(CC) $(CFLAGS) $(SOURCE)\error.c
the-obj\execute.$(OBJTYPE):	$(SOURCE)\execute.c $(SOURCE)\the.h $(SOURCE)\thedefs.h $(SOURCE)\proto.h
	$(CC) $(CFLAGS) $(SOURCE)\execute.c
the-obj\linked.$(OBJTYPE):	$(SOURCE)\linked.c $(SOURCE)\the.h $(SOURCE)\thedefs.h $(SOURCE)\proto.h
	$(CC) $(CFLAGS) $(SOURCE)\linked.c
the-obj\column.$(OBJTYPE):	$(SOURCE)\column.c $(SOURCE)\the.h $(SOURCE)\thedefs.h $(SOURCE)\proto.h
	$(CC) $(CFLAGS) $(SOURCE)\column.c
the-obj\mouse.$(OBJTYPE):	$(SOURCE)\mouse.c $(SOURCE)\the.h $(SOURCE)\thedefs.h $(SOURCE)\proto.h
	$(CC) $(CFLAGS) $(SOURCE)\mouse.c
the-obj\memory.$(OBJTYPE):	$(SOURCE)\memory.c $(SOURCE)\the.h $(SOURCE)\thedefs.h $(SOURCE)\proto.h
	$(CC) $(CFLAGS) $(SOURCE)\memory.c
the-obj\nonansi.$(OBJTYPE):	$(SOURCE)\nonansi.c $(SOURCE)\the.h $(SOURCE)\thedefs.h $(SOURCE)\proto.h
	$(CC) $(CFLAGS) $(SOURCE)\nonansi.c
the-obj\os2eas.$(OBJTYPE):	$(SOURCE)\os2eas.c $(SOURCE)\the.h $(SOURCE)\thedefs.h $(SOURCE)\proto.h
	$(CC) $(CFLAGS) $(SOURCE)\os2eas.c
the-obj\prefix.$(OBJTYPE):	$(SOURCE)\prefix.c $(SOURCE)\the.h $(SOURCE)\thedefs.h $(SOURCE)\proto.h
	$(CC) $(CFLAGS) $(SOURCE)\prefix.c
the-obj\print.$(OBJTYPE):	$(SOURCE)\print.c $(SOURCE)\the.h $(SOURCE)\thedefs.h $(SOURCE)\proto.h
	$(CC) $(CFLAGS) $(SOURCE)\print.c
the-obj\reserved.$(OBJTYPE):	$(SOURCE)\reserved.c $(SOURCE)\the.h $(SOURCE)\thedefs.h $(SOURCE)\proto.h
	$(CC) $(CFLAGS) $(SOURCE)\reserved.c
the-obj\scroll.$(OBJTYPE):	$(SOURCE)\scroll.c $(SOURCE)\the.h $(SOURCE)\thedefs.h $(SOURCE)\proto.h
	$(CC) $(CFLAGS) $(SOURCE)\scroll.c
the-obj\show.$(OBJTYPE):	$(SOURCE)\show.c $(SOURCE)\the.h $(SOURCE)\thedefs.h $(SOURCE)\proto.h
	$(CC) $(CFLAGS) $(SOURCE)\show.c
the-obj\sort.$(OBJTYPE):	$(SOURCE)\sort.c $(SOURCE)\the.h $(SOURCE)\thedefs.h $(SOURCE)\proto.h
	$(CC) $(CFLAGS) $(SOURCE)\sort.c
the-obj\target.$(OBJTYPE):	$(SOURCE)\target.c $(SOURCE)\the.h $(SOURCE)\thedefs.h $(SOURCE)\proto.h
	$(CC) $(CFLAGS) $(SOURCE)\target.c
the-obj\the.$(OBJTYPE):	$(SOURCE)\the.c $(SOURCE)\the.h $(SOURCE)\thedefs.h $(SOURCE)\proto.h
	$(CC) -I. $(CFLAGS) $(SOURCE)/the.c
the-obj\util.$(OBJTYPE):	$(SOURCE)\util.c $(SOURCE)\the.h $(SOURCE)\thedefs.h $(SOURCE)\proto.h
	$(CC) $(CFLAGS) $(SOURCE)\util.c
the-obj\commutil.$(OBJTYPE):	$(SOURCE)\commutil.c $(SOURCE)\the.h $(SOURCE)\command.h $(SOURCE)\thedefs.h $(SOURCE)\proto.h $(SOURCE)\getch.h $(SOURCE)\key.h
	$(CC) $(CFLAGS) $(SOURCE)\commutil.c
the-obj\trace.$(OBJTYPE):	$(SOURCE)\trace.c $(SOURCE)\the.h $(SOURCE)\command.h $(SOURCE)\thedefs.h $(SOURCE)\proto.h $(SOURCE)\getch.h $(SOURCE)\key.h
	$(CC) $(CFLAGS) $(SOURCE)\trace.c
the-obj\getch.$(OBJTYPE):	$(SOURCE)\getch.c $(SOURCE)\getch.h $(SOURCE)\thedefs.h $(SOURCE)\proto.h
	$(CC) $(CFLAGS) $(SOURCE)\getch.c
the-obj\query.$(OBJTYPE):	$(SOURCE)\query.c $(SOURCE)\query.h $(SOURCE)\the.h $(SOURCE)\thedefs.h $(SOURCE)\proto.h
	$(CC) $(CFLAGS) $(SOURCE)\query.c
the-obj\query1.$(OBJTYPE):	$(SOURCE)\query1.c $(SOURCE)\query.h $(SOURCE)\the.h $(SOURCE)\thedefs.h $(SOURCE)\proto.h
	$(CC) $(CFLAGS) $(SOURCE)\query1.c
the-obj\query2.$(OBJTYPE):	$(SOURCE)\query2.c $(SOURCE)\query.h $(SOURCE)\the.h $(SOURCE)\thedefs.h $(SOURCE)\proto.h
	$(CC) $(CFLAGS) $(SOURCE)\query2.c
the-obj\thematch.$(OBJTYPE):	$(SOURCE)\thematch.c $(SOURCE)\the.h $(SOURCE)\thematch.h
	$(CC) $(CFLAGS) $(SOURCE)\thematch.c
the-obj\parser.$(OBJTYPE):	$(SOURCE)\parser.c $(SOURCE)\the.h $(SOURCE)\thedefs.h $(SOURCE)\proto.h
	$(CC) $(CFLAGS) $(SOURCE)\parser.c
the-obj\directry.$(OBJTYPE):	$(SOURCE)\directry.c $(SOURCE)\the.h $(SOURCE)\directry.h $(SOURCE)\thedefs.h $(SOURCE)\proto.h
	$(CC) $(CFLAGS) $(SOURCE)\directry.c
the-obj\file.$(OBJTYPE):	$(SOURCE)\file.c $(SOURCE)\the.h $(SOURCE)\directry.h $(SOURCE)\thedefs.h $(SOURCE)\proto.h
	$(CC) $(CFLAGS) $(SOURCE)\file.c
the-obj\rexx.$(OBJTYPE):	$(SOURCE)\rexx.c $(SOURCE)\the.h $(SOURCE)\therexx.h $(SOURCE)\proto.h $(SOURCE)\thedefs.h $(SOURCE)\query.h
	$(CC) $(CFLAGS) $(SOURCE)\rexx.c
the-obj\regex.$(OBJTYPE):	$(SOURCE)\regex.c $(SOURCE)\regex.h
	$(CC) $(CFLAGS) $(SOURCE)\regex.c
the-obj\mygetopt.$(OBJTYPE):	$(SOURCE)\mygetopt.c $(SOURCE)\mygetopt.h
	$(CC) $(CFLAGS) $(SOURCE)\mygetopt.c

the-obj\execthe.$(OBJTYPE):	$(SOURCE)\execthe.c $(SOURCE)\the.h $(SOURCE)\thedefs.h $(SOURCE)\proto.h
	$(CC) -I. $(CFLAGS) $(SOURCE)\execthe.c
#	$(CC) $(CFLAGS) -DTHE_VERSION=\"$(VER_DOT)\" -DTHE_VERSION_DATE=\"$(VER_DATE)\" $(SOURCE)\execthe.c

#PDCursesMod object files
pdc-obj\addch.$(OBJTYPE): $(SOURCE)/PDCursesMod/pdcurses/addch.c
	$(CC) $(LIBCCFLAGS)  -c -o $@ $<
pdc-obj\addchstr.$(OBJTYPE): $(SOURCE)\PDCursesMod\pdcurses\addchstr.c
	$(CC) $(LIBCCFLAGS)  -c -o $@ $<
pdc-obj\addstr.$(OBJTYPE) : $(SOURCE)\PDCursesMod\pdcurses\addstr.c
	$(CC) $(LIBCCFLAGS)  -c -o $@ $<
pdc-obj\attr.$(OBJTYPE) : $(SOURCE)\PDCursesMod\pdcurses\attr.c
	$(CC) $(LIBCCFLAGS)  -c -o $@ $<
pdc-obj\beep.$(OBJTYPE): $(SOURCE)\PDCursesMod\pdcurses\beep.c
	$(CC) $(LIBCCFLAGS)  -c -o $@ $<
pdc-obj\bkgd.$(OBJTYPE) : $(SOURCE)\PDCursesMod\pdcurses\bkgd.c
	$(CC) $(LIBCCFLAGS)  -c -o $@ $<
pdc-obj\border.$(OBJTYPE) : $(SOURCE)\PDCursesMod\pdcurses\border.c
	$(CC) $(LIBCCFLAGS)  -c -o $@ $<
pdc-obj\clear.$(OBJTYPE) : $(SOURCE)\PDCursesMod\pdcurses\clear.c
	$(CC) $(LIBCCFLAGS)  -c -o $@ $<
pdc-obj\color.$(OBJTYPE) : $(SOURCE)\PDCursesMod\pdcurses\color.c
	$(CC) $(LIBCCFLAGS)  -c -o $@ $<
pdc-obj\delch.$(OBJTYPE) : $(SOURCE)\PDCursesMod\pdcurses\delch.c
	$(CC) $(LIBCCFLAGS)  -c -o $@ $<
pdc-obj\deleteln.$(OBJTYPE): $(SOURCE)\PDCursesMod\pdcurses\deleteln.c
	$(CC) $(LIBCCFLAGS)  -c -o $@ $<
pdc-obj\getch.$(OBJTYPE) : $(SOURCE)\PDCursesMod\pdcurses\getch.c
	$(CC) $(LIBCCFLAGS)  -c -o $@ $<
pdc-obj\getstr.$(OBJTYPE) : $(SOURCE)\PDCursesMod\pdcurses\getstr.c
	$(CC) $(LIBCCFLAGS)  -c -o $@ $<
pdc-obj\getyx.$(OBJTYPE) : $(SOURCE)\PDCursesMod\pdcurses\getyx.c
	$(CC) $(LIBCCFLAGS)  -c -o $@ $<
pdc-obj\inch.$(OBJTYPE) : $(SOURCE)\PDCursesMod\pdcurses\inch.c
	$(CC) $(LIBCCFLAGS)  -c -o $@ $<
pdc-obj\inchstr.$(OBJTYPE): $(SOURCE)\PDCursesMod\pdcurses\inchstr.c
	$(CC) $(LIBCCFLAGS)  -c -o $@ $<
pdc-obj\initscr.$(OBJTYPE) : $(SOURCE)\PDCursesMod\pdcurses\initscr.c
	$(CC) $(LIBCCFLAGS)  -c -o $@ $<
pdc-obj\inopts.$(OBJTYPE) : $(SOURCE)\PDCursesMod\pdcurses\inopts.c
	$(CC) $(LIBCCFLAGS)  -c -o $@ $<
pdc-obj\insch.$(OBJTYPE) : $(SOURCE)\PDCursesMod\pdcurses\insch.c
	$(CC) $(LIBCCFLAGS)  -c -o $@ $<
pdc-obj\insstr.$(OBJTYPE) : $(SOURCE)\PDCursesMod\pdcurses\insstr.c
	$(CC) $(LIBCCFLAGS)  -c -o $@ $<
pdc-obj\instr.$(OBJTYPE) : $(SOURCE)\PDCursesMod\pdcurses\instr.c
	$(CC) $(LIBCCFLAGS)  -c -o $@ $<
pdc-obj\kernel.$(OBJTYPE): $(SOURCE)\PDCursesMod\pdcurses\kernel.c
	$(CC) $(LIBCCFLAGS)  -c -o $@ $<
pdc-obj\keyname.$(OBJTYPE) : $(SOURCE)\PDCursesMod\pdcurses\keyname.c
	$(CC) $(LIBCCFLAGS)  -c -o $@ $<
pdc-obj\mouse.$(OBJTYPE) : $(SOURCE)\PDCursesMod\pdcurses\mouse.c
	$(CC) $(LIBCCFLAGS)  -c -o $@ $<
pdc-obj\move.$(OBJTYPE) : $(SOURCE)\PDCursesMod\pdcurses\move.c
	$(CC) $(LIBCCFLAGS)  -c -o $@ $<
pdc-obj\outopts.$(OBJTYPE) : $(SOURCE)\PDCursesMod\pdcurses\outopts.c
	$(CC) $(LIBCCFLAGS)  -c -o $@ $<
pdc-obj\overlay.$(OBJTYPE) : $(SOURCE)\PDCursesMod\pdcurses\overlay.c
	$(CC) $(LIBCCFLAGS)  -c -o $@ $<
pdc-obj\pad.$(OBJTYPE): $(SOURCE)\PDCursesMod\pdcurses\pad.c
	$(CC) $(LIBCCFLAGS)  -c -o $@ $<
pdc-obj\panel.$(OBJTYPE) : $(SOURCE)\PDCursesMod\pdcurses\panel.c
	$(CC) $(LIBCCFLAGS)  -c -o $@ $<
pdc-obj\printw.$(OBJTYPE) : $(SOURCE)\PDCursesMod\pdcurses\printw.c
	$(CC) $(LIBCCFLAGS)  -c -o $@ $<
pdc-obj\refresh.$(OBJTYPE) : $(SOURCE)\PDCursesMod\pdcurses\refresh.c
	$(CC) $(LIBCCFLAGS)  -c -o $@ $<
pdc-obj\scanw.$(OBJTYPE) : $(SOURCE)\PDCursesMod\pdcurses\scanw.c
	$(CC) $(LIBCCFLAGS)  -c -o $@ $<
pdc-obj\scr_dump.$(OBJTYPE) : $(SOURCE)\PDCursesMod\pdcurses\scr_dump.c
	$(CC) $(LIBCCFLAGS)  -c -o $@ $<
pdc-obj\scroll.$(OBJTYPE): $(SOURCE)\PDCursesMod\pdcurses\scroll.c
	$(CC) $(LIBCCFLAGS)  -c -o $@ $<
pdc-obj\slk.$(OBJTYPE) : $(SOURCE)\PDCursesMod\pdcurses\slk.c
	$(CC) $(LIBCCFLAGS)  -c -o $@ $<
pdc-obj\termattr.$(OBJTYPE) : $(SOURCE)\PDCursesMod\pdcurses\termattr.c
	$(CC) $(LIBCCFLAGS)  -c -o $@ $<
pdc-obj\touch.$(OBJTYPE) : $(SOURCE)\PDCursesMod\pdcurses\touch.c
	$(CC) $(LIBCCFLAGS)  -c -o $@ $<
pdc-obj\util.$(OBJTYPE) : $(SOURCE)\PDCursesMod\pdcurses\util.c
	$(CC) $(LIBCCFLAGS)  -c -o $@ $<
pdc-obj\window.$(OBJTYPE) : $(SOURCE)\PDCursesMod\pdcurses\window.c
	$(CC) $(LIBCCFLAGS)  -c -o $@ $<
pdc-obj\debug.$(OBJTYPE): $(SOURCE)\PDCursesMod\pdcurses\debug.c
	$(CC) $(LIBCCFLAGS)  -c -o $@ $<

pdc-obj\pdcclip.$(OBJTYPE): $(SOURCE)\PDCursesMod\$(PDC_DIR)\pdcclip.c
	$(CC) $(PDCCCFLAGS)  -c -o $@ $<
pdc-obj\pdcdisp.$(OBJTYPE): $(SOURCE)\PDCursesMod\$(PDC_DIR)\pdcdisp.c
	$(CC) $(PDCCCFLAGS)  -c -o $@ $<
pdc-obj\pdcgetsc.$(OBJTYPE): $(SOURCE)\PDCursesMod\$(PDC_DIR)\pdcgetsc.c
	$(CC) $(PDCCCFLAGS)  -c -o $@ $<
pdc-obj\pdckbd.$(OBJTYPE): $(SOURCE)\PDCursesMod\$(PDC_DIR)\pdckbd.c
	$(CC) $(PDCCCFLAGS)  -c -o $@ $<
pdc-obj\pdcscrn.$(OBJTYPE): $(SOURCE)\PDCursesMod\$(PDC_DIR)\pdcscrn.c
	$(CC) $(PDCCCFLAGS)  -c -o $@ $<
pdc-obj\pdcsetsc.$(OBJTYPE): $(SOURCE)\PDCursesMod\$(PDC_DIR)\pdcsetsc.c
	$(CC) $(PDCCCFLAGS)  -c -o $@ $<
pdc-obj\pdcutil.$(OBJTYPE): $(SOURCE)\PDCursesMod\$(PDC_DIR)\pdcutil.c
	$(CC) $(PDCCCFLAGS)  -c -o $@ $<

########################################################################
dist: the.exe
	echo run $(SRC)\makedist.cmd
#########################################################################

manext.exe: manext.o
	$(CC) manext.o -o manext
	emxbind manext

manext.o: $(SOURCE)\manext.c $(SOURCE)\the.h
	$(CC) -c -I$(SOURCE) -o manext.o $(SOURCE)\manext.c

html:	manext.exe
	-del *$(HTML_EXT)
	copy $(SRC)\man2html.rex .\man2html.cmd
	man2html $(HTML_EXT) $(VER_DOT) TOCSTART > index$(HTML_EXT)
	.\manext $(OVERVIEW) > overview.man
	man2html $(HTML_EXT) $(VER_DOT) OVERVIEW overview.man index$(HTML_EXT) > overview$(HTML_EXT)
	.\manext $(COMM) > comm.man
	man2html $(HTML_EXT) $(VER_DOT) COMM comm.man index$(HTML_EXT) > comm$(HTML_EXT)
	.\manext $(COMMSET) > commset.man
	man2html $(HTML_EXT) $(VER_DOT) COMMSET commset.man index$(HTML_EXT) > commset$(HTML_EXT)
	.\manext $(COMMSOS) > commsos.man
	man2html $(HTML_EXT) $(VER_DOT) COMMSOS commsos.man index$(HTML_EXT) > commsos$(HTML_EXT)
	.\manext $(QUERY) > query.man
	man2html $(HTML_EXT) $(VER_DOT) QUERY    query.man index$(HTML_EXT)    > query$(HTML_EXT)
	.\manext $(GLOSSARY) > glossary.man
	man2html $(HTML_EXT) $(VER_DOT) GLOSSARY glossary.man index$(HTML_EXT) > glossary$(HTML_EXT)
	.\manext $(APPENDIX1) > app1.man
	man2html $(HTML_EXT) $(VER_DOT) APPENDIX1 app1.man index$(HTML_EXT) > app1$(HTML_EXT)
	.\manext $(APPENDIX2) > app2.man
	man2html $(HTML_EXT) $(VER_DOT) APPENDIX2 app2.man index$(HTML_EXT) > app2$(HTML_EXT)
	.\manext $(APPENDIX3) > app3.man
	man2html $(HTML_EXT) $(VER_DOT) APPENDIX3 app3.man index$(HTML_EXT) > app3$(HTML_EXT)
	.\manext $(APPENDIX4) > app4.man
	man2html $(HTML_EXT) $(VER_DOT) APPENDIX4 app4.man index$(HTML_EXT) > app4$(HTML_EXT)
	man2html $(HTML_EXT) $(VER_DOT) APPENDIX5 $(docdir)\app5.htm index$(HTML_EXT) > app5$(HTML_EXT)
	man2html $(HTML_EXT) $(VER_DOT) APPENDIX6 $(docdir)\app6.htm index$(HTML_EXT) > app6$(HTML_EXT)
	.\manext $(APPENDIX7) > app7.man
	man2html $(HTML_EXT) $(VER_DOT) APPENDIX7 app7.man index$(HTML_EXT) > app7$(HTML_EXT)
	copy $(SRC)\HISTORY history.man
	man2html $(HTML_EXT) $(VER_DOT) HISTORY history.man index$(HTML_EXT) > history$(HTML_EXT)
	copy $(SRC)\THE_Help.txt quickref.man
	man2html $(HTML_EXT) $(VER_DOT) QUICKREF quickref.man index$(HTML_EXT) > quickref$(HTML_EXT)
	man2html $(HTML_EXT) $(VER_DOT) TOCEND >> index$(HTML_EXT)

helpviewer:	manext.exe
	-rm -f *$(HTML_EXT)
	-rm -f *.man
	-rm -f comm/*
	-rm -f commsos/*
	-rm -f commset/*
	-rm -f misc/*
	-mkdir comm
	-mkdir commsos
	-mkdir commset
	-mkdir misc
	cp $(SRC)/man2hv.rex ./man2hv.cmd
	rexx ./man2hv.cmd $(HTML_EXT) $(VER_DOT) TOCSTART 'junk' index$(HTML_EXT)
	./manext $(OVERVIEW) > overview.man
	rexx ./man2hv.cmd $(HTML_EXT) $(VER_DOT) OVERVIEW overview.man index$(HTML_EXT)
	./manext $(COMM) > comm.man
	rexx ./man2hv.cmd $(HTML_EXT) $(VER_DOT) COMM comm.man index$(HTML_EXT)
	./manext $(COMMSET) > commset.man
	rexx ./man2hv.cmd $(HTML_EXT) $(VER_DOT) COMMSET commset.man index$(HTML_EXT)
	./manext $(COMMSOS) > commsos.man
	rexx ./man2hv.cmd $(HTML_EXT) $(VER_DOT) COMMSOS commsos.man index$(HTML_EXT)
	./manext $(QUERY) > query.man
	rexx ./man2hv.cmd $(HTML_EXT) $(VER_DOT) QUERY    query.man index$(HTML_EXT)
	./manext $(GLOSSARY) > glossary.man
	rexx ./man2hv.cmd $(HTML_EXT) $(VER_DOT) GLOSSARY glossary.man index$(HTML_EXT)
	./manext $(APPENDIX1) > app1.man
	rexx ./man2hv.cmd $(HTML_EXT) $(VER_DOT) APPENDIX1 app1.man index$(HTML_EXT)
	./manext $(APPENDIX2) > app2.man
	rexx ./man2hv.cmd $(HTML_EXT) $(VER_DOT) APPENDIX2 app2.man index$(HTML_EXT)
	./manext $(APPENDIX3) > app3.man
	rexx ./man2hv.cmd $(HTML_EXT) $(VER_DOT) APPENDIX3 app3.man index$(HTML_EXT)
	./manext $(APPENDIX4) > app4.man
	rexx ./man2hv.cmd $(HTML_EXT) $(VER_DOT) APPENDIX4 app4.man index$(HTML_EXT)
	rexx ./man2hv.cmd $(HTML_EXT) $(VER_DOT) APPENDIX5 $(docdir)\app5.htm index$(HTML_EXT)
	rexx ./man2hv.cmd $(HTML_EXT) $(VER_DOT) APPENDIX6 $(docdir)\app6.htm index$(HTML_EXT)
	./manext $(APPENDIX7) > app7.man
	rexx ./man2hv.cmd $(HTML_EXT) $(VER_DOT) APPENDIX7 app7.man index$(HTML_EXT)
	cp $(SRC)\HISTORY history.man
	rexx ./man2hv.cmd $(HTML_EXT) $(VER_DOT) HISTORY history.man index$(HTML_EXT)
	cp $(SRC)\THE_Help.txt quickref.man
	rexx ./man2hv.cmd $(HTML_EXT) $(VER_DOT) QUICKREF quickref.man index$(HTML_EXT)
	rexx ./man2hv.cmd $(HTML_EXT) $(VER_DOT) TOCEND 'junk' index$(HTML_EXT)

clean:
	-rm the-obj/*.obj pdc-obj/*.obj *.exe *.obj
