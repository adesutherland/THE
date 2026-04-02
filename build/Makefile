# Makefile for THE
#
# The variable 'srcdir' refers to the source-distribution, and can be set with
# the configure script by "--srcdir=DIR".
#

SHELL		= /bin/sh
THIS		= Makefile



VERSION="4.0"
VERSION_UNQUOTED=4.0
VERSION_BASE_UNQUOTED=4.0
VERSION_NODOT="40"
VER_DATE="29-Dec-2022"
THE_DIR=the-4.0
PACKAGE_RELEASE=260402.0933

srcdir		= /Users/adrian/CLionProjects/THE
abs_srcdir		= /Users/adrian/CLionProjects/THE
prefix		= /usr/local
target		= x86_64-apple-darwin24.6.0
exec_prefix	= ${prefix}
libdir		= /usr/local/lib
bindir		= ${exec_prefix}/bin
includedir		= ${prefix}/include
mandir		= ${datarootdir}/man
datarootdir		= ${prefix}/share
datadir		= ${datarootdir}
docdir		= $(prefix)/share/doc/the-doc
libpdcurses_target = 

sourcedir		= $(srcdir)/src
srcdocdir		= $(srcdir)/doc
imagedir		= $(srcdir)/images
macrodir		= $(srcdir)/macros
contribdir		= $(sourcedir)/contrib
thisdir		=/Users/adrian/CLionProjects/THE/build
packagesdir = /Users/adrian/CLionProjects/THE/packages
rpmtopdir =   
debtopdir =   
apktopdir = 

INSTALL		= $(srcdir)/install-sh

HTML_EXT		= .html

CC		= clang
CFLAGS		= -g -O2 -O3 -Wall -fomit-frame-pointer -fno-strict-aliasing -Wno-char-subscripts
LIBS		= 
DEBUG =            -DNDEBUG
DEBUGGING =        

BASE_INSTALL = installbase
BASE_BINARY = binarybase
THEBIN = the-conw
THETYPE = ncursesw
MYDISTRO=apple-darwin24.6.0
MACH_ARCH=x86_64
REXX_BUILD = regina
CURSES_BUILD = ncursesw
APK_CURSES_BUILD = ncursesw
CURSES_INC_DIR_BUILD = 
CURSES_LIB_DIR_BUILD = 

INCDIR		= $(sourcedir)
CPPFLAGS	= -I. -I$(INCDIR)  -I$(contribdir) $(DEBUG) -DHAVE_CONFIG_H   -DTHE_HOME_DIRECTORY=\"$(datadir)/the/\" -DTHE_VARIANT=\"conw\"

CCFLAGS		= -c $(CFLAGS) $(CPPFLAGS)  -I/usr/local/Cellar/regina-rexx/3.9.7/include -DUSE_WIDE_CHAR

LINK		=  $(CC)
LDFLAGS		=  -flat_namespace -undefined suppress $(DEBUGGING)  -lncurses -L/usr/local/Cellar/regina-rexx/3.9.7/lib -lregina $(LIBS)  -lm 
THELDFLAGS		=  -flat_namespace -undefined suppress $(DEBUGGING)  $(LIBS)  -lm 

################################################################################

COMM = \
 $(sourcedir)/comm1.c \
 $(sourcedir)/comm2.c \
 $(sourcedir)/comm3.c \
 $(sourcedir)/comm4.c \
 $(sourcedir)/comm5.c

COMMSOS = \
 $(sourcedir)/commsos.c

COMMSET = \
 $(sourcedir)/commset1.c \
 $(sourcedir)/commset2.c

QUERY = $(sourcedir)/query.c

APPENDIX1 = $(srcdir)/appendix.1
APPENDIX2 = $(srcdir)/appendix.2
APPENDIX3 = $(srcdir)/appendix.3
APPENDIX4 = $(srcdir)/appendix.4
APPENDIX7 = $(srcdir)/appendix.7
APPENDIX = $(APPENDIX1) $(APPENDIX2) $(APPENDIX3) $(APPENDIX4) $(APPENDIX7)

GLOSSARY = $(srcdir)/glossary
OVERVIEW = $(srcdir)/overview

H0=	./config.h $(sourcedir)/regex.h
H1=	./config.h $(sourcedir)/the.h $(sourcedir)/vars.h $(sourcedir)/thedefs.h $(sourcedir)/proto.h
H2=	./config.h $(sourcedir)/the.h $(sourcedir)/vars.h $(sourcedir)/command.h $(sourcedir)/thedefs.h $(sourcedir)/proto.h $(sourcedir)/getch.h $(sourcedir)/key.h
H3=	./config.h $(sourcedir)/getch.h $(sourcedir)/vars.h $(sourcedir)/thedefs.h $(sourcedir)/proto.h
H4=	./config.h $(sourcedir)/the.h $(sourcedir)/vars.h $(sourcedir)/query.h $(sourcedir)/thedefs.h $(sourcedir)/proto.h
H5=	./config.h $(sourcedir)/the.h $(sourcedir)/vars.h $(sourcedir)/thematch.h
H6=	./config.h $(sourcedir)/the.h $(sourcedir)/vars.h $(sourcedir)/directry.h $(sourcedir)/thedefs.h $(sourcedir)/proto.h
H7=	./config.h $(sourcedir)/the.h $(sourcedir)/vars.h $(sourcedir)/therexx.h $(sourcedir)/proto.h $(sourcedir)/thedefs.h $(sourcedir)/query.h
H8=	./config.h $(sourcedir)/the.h $(sourcedir)/vars.h $(sourcedir)/therexx.h $(sourcedir)/thedefs.h

ARCHIVE_FILES = \
 ./README ./INSTALL ./FAQ ./TODO ./COPYING ./HISTORY ./THE_Help.txt \
 ./overview ./appendix.1 ./appendix.2 ./appendix.3 ./appendix.4 ./appendix.7 ./glossary ./README.OS2 ./README.Amiga \
 ./src/*.h ./src/*.c \
 ./src/contrib/*.c ./src/contrib/*.h \
 ./src/PDCursesMod/*.h \
 ./src/PDCursesMod/common/*.c ./src/PDCursesMod/common/*.h ./src/PDCursesMod/common/*.xpm \
 ./src/PDCursesMod/dos/*.c ./src/PDCursesMod/dos/*.h \
 ./src/PDCursesMod/dosvga/*.c ./src/PDCursesMod/dosvga/*.h \
 ./src/PDCursesMod/fb/*.c ./src/PDCursesMod/fb/*.h \
 ./src/PDCursesMod/os2/*.c ./src/PDCursesMod/os2/*.h \
 ./src/PDCursesMod/plan9/*.c ./src/PDCursesMod/plan9/*.h \
 ./src/PDCursesMod/sdl1/*.c ./src/PDCursesMod/sdl1/*.h \
 ./src/PDCursesMod/sdl2/*.c ./src/PDCursesMod/sdl2/*.h \
 ./src/PDCursesMod/pdcurses/*.c \
 ./src/PDCursesMod/vt/*.c ./src/PDCursesMod/vt/*.h \
 ./src/PDCursesMod/wingui/*.c ./src/PDCursesMod/wingui/*.h \
 ./src/PDCursesMod/wincon/*.c ./src/PDCursesMod/wincon/*.h \
 ./src/PDCursesMod/x11/*.c ./src/PDCursesMod/x11/*.h \
 ./*.rsp ./*.def ./*.diz ./files.rcs ./the*.xbm \
 ./*.ico ./the.res ./*.rc ./the.eas ./doc/THE_Help* ./doc/*.htm ./doc/*.the ./doc/*.gif \
 ./macros/append.the ./macros/comm.the ./macros/uncomm.the ./macros/total.the ./macros/match.the ./macros/fold.the ./macros/rm.the ./macros/build.the ./macros/setbuild.the ./macros/nl.the \
 ./macros/words.the ./macros/l.the ./macros/compile.the ./macros/spell.the ./macros/demo.the ./macros/demo.txt ./macros/cua.the ./macros/config.the ./macros/tags.the \
 ./macros/codecomp.the ./macros/syntax.the ./macros/complete.the ./macros/completer.the ./macros/diff.the ./macros/remote.the \
 ./the.nsi ./the.ver \
 ./*.mak ./*.tld ./thedit.lnk ./the.1 \
 ./common/config.guess ./common/config.sub ./install-sh \
 ./aclocal.m4 ./configure.in ./common/accommon.m4 ./common/*.nsh \
 ./Makefile.in ./config.h.in ./configure \
 ./man2html.rex ./man2hv.rex ./makehtml.rex ./makehv.rex ./makedist.cmd ./tld2c.rex \
 ./THE.spec ./THE.spec.in ./the64.xpm ./images/*.png ./images/*.bmp ./descrip.mms ./build.com \
 ./syntax/*.syntax \
 ./pkgconfig/*.pc ./pkgconfig/README \
 ./fixpackage.rexx ./renameapk.sh \
 ./APKBUILD.in \
 ./haiku/PackageInfo*.in \
 ./debian/compat ./debian/control.in ./debian/copyright ./debian/rules.in ./debian/changelog \
 ./debian/prerm ./debian/postinst

################################################################################
all:	$(libpdcurses_target) $(THEBIN) the the.man THE_Help.txt html helpviewer

the.man:	manext $(OVERVIEW) $(COMM) $(COMMSOS) $(COMMSET) $(QUERY) $(APPENDIX) $(GLOSSARY)
	-chmod u+w *
	-cp $(srcdocdir)/* .
	./manext $(OVERVIEW) $(COMM) $(COMMSOS) $(COMMSET) $(QUERY) $(APPENDIX) $(GLOSSARY) > the.man

THE_Help.txt:	manext $(COMM) $(COMMSET) $(COMMSOS) $(srcdocdir)/THE_Help_head $(srcdocdir)/THE_Help_set $(srcdocdir)/THE_Help_sos $(srcdocdir)/THE_Help_tail
	-chmod u+w *
	-cp $(srcdocdir)/* .
	echo "                         THE Quick Reference" > THE_Help_part01
	echo "                             Version $(VERSION)" > THE_Help_part02
	cp THE_Help_head THE_Help_part03
	./manext -q $(COMM) > THE_Help_part04
	cp THE_Help_set THE_Help_part05
	./manext -q $(COMMSET) > THE_Help_part06
	cp THE_Help_sos THE_Help_part07
	./manext -q $(COMMSOS) > THE_Help_part08
	cp THE_Help_tail THE_Help_part09
	cat THE_Help_part?? > THE_Help.txt
	rm -f THE_Help_*

doc/html:
	mkdir -p doc/html

html:	manext doc/html doc/html/index.html

doc/html/index.html: $(sourcedir)/*.c $(APPENDIX) $(GLOSSARY) $(OVERVIEW)
#	-rm -fr *$(HTML_EXT) *.man
	cp $(imagedir)/the64.png .
	rexx $(srcdir)/makehtml.rex src=$(srcdir) ext=$(HTML_EXT) ver=$(VERSION) dest=./doc/html doc=$(srcdocdir) \
		overview=$(OVERVIEW)  \
		comm=$(COMM)      \
		commset=$(COMMSET)   \
		commsos=$(COMMSOS)   \
		query=$(QUERY)     \
		glossary=$(GLOSSARY)  \
		appendix1=$(APPENDIX1) \
		appendix2=$(APPENDIX2) \
		appendix3=$(APPENDIX3) \
		appendix4=$(APPENDIX4) \
		appendix7=$(APPENDIX7)

doc/helpviewer:
	mkdir -p doc/helpviewer

helpviewer: manext doc/helpviewer doc/helpviewer/index.html

doc/helpviewer/index.html: $(sourcedir)/*.c $(APPENDIX) $(GLOSSARY) $(OVERVIEW)
#	-rm -f *$(HTML_EXT) *.man comm/* commsos/* commset/* misc/*
	cp $(imagedir)/the64.png .
	rexx $(srcdir)/makehv.rex src=$(srcdir) ext=$(HTML_EXT) ver=$(VERSION) dest=./doc/helpviewer doc=$(srcdocdir) \
		overview=$(OVERVIEW)  \
		comm=$(COMM)      \
		commset=$(COMMSET)   \
		commsos=$(COMMSOS)   \
		query=$(QUERY)     \
		glossary=$(GLOSSARY)  \
		appendix1=$(APPENDIX1) \
		appendix2=$(APPENDIX2) \
		appendix3=$(APPENDIX3) \
		appendix4=$(APPENDIX4) \
		appendix7=$(APPENDIX7)

doc/combined:
	mkdir -p doc/combined

combined: doc/combined
	-rm -fr doc/combined/*$(HTML_EXT) doc/combined/*.man doc/combined/comm/* doc/combined/commsos/* doc/combined/commset/* doc/combined/misc/*
	cp $(imagedir)/the64.png doc/combined
	rexx $(srcdir)/makehtml.rex src=$(srcdir) ext=$(HTML_EXT) ver=$(VERSION) dest=doc/combined doc=$(srcdocdir) \
		overview=$(OVERVIEW)  \
		comm=$(COMM)      \
		commset=$(COMMSET)   \
		commsos=$(COMMSOS)   \
		query=$(QUERY)     \
		glossary=$(GLOSSARY)  \
		appendix1=$(APPENDIX1) \
		appendix2=$(APPENDIX2) \
		appendix3=$(APPENDIX3) \
		appendix4=$(APPENDIX4) \
		appendix7=$(APPENDIX7)
	./$(THEBIN) -w 15000 -b -p $(srcdocdir)/preparecomb.the \
		doc/combined/index$(HTML_EXT) \
		doc/combined/overview$(HTML_EXT) \
		doc/combined/comm$(HTML_EXT) \
		doc/combined/commset$(HTML_EXT) \
		doc/combined/commsos$(HTML_EXT) \
		doc/combined/query$(HTML_EXT) \
		doc/combined/glossary$(HTML_EXT) \
		doc/combined/app1$(HTML_EXT) \
		doc/combined/app2$(HTML_EXT) \
		doc/combined/app3$(HTML_EXT) \
		doc/combined/app4$(HTML_EXT) \
		doc/combined/app5$(HTML_EXT) \
		doc/combined/app6$(HTML_EXT) \
		doc/combined/app7$(HTML_EXT) \
		doc/combined/history$(HTML_EXT) \
		doc/combined/quickref$(HTML_EXT) \
		doc/combined/fdl$(HTML_EXT)
	cat doc/combined/index$(HTML_EXT).new \
		doc/combined/overview$(HTML_EXT).new \
		doc/combined/comm$(HTML_EXT).new \
		doc/combined/commset$(HTML_EXT).new \
		doc/combined/commsos$(HTML_EXT).new \
		doc/combined/query$(HTML_EXT).new \
		doc/combined/glossary$(HTML_EXT).new \
		doc/combined/app1$(HTML_EXT).new \
		doc/combined/app2$(HTML_EXT).new \
		doc/combined/app3$(HTML_EXT).new \
		doc/combined/app4$(HTML_EXT).new \
		doc/combined/app5$(HTML_EXT).new \
		doc/combined/app6$(HTML_EXT).new \
		doc/combined/app7$(HTML_EXT).new \
		doc/combined/history$(HTML_EXT).new \
		doc/combined/quickref$(HTML_EXT).new \
		doc/combined/fdl$(HTML_EXT)> \
		doc/combined/the-$(VERSION)$(HTML_EXT).save
	rm -f doc/combined/*.new doc/combined/*.man doc/combined/*.html
	mv doc/combined/the-$(VERSION)$(HTML_EXT).save doc/combined/the-$(VERSION)$(HTML_EXT)

doc/pdf:
	mkdir -p doc/pdf

pdf: doc/pdf
	-rm -fr doc/pdf/*$(HTML_EXT) doc/pdf/*.man doc/pdf/comm/* doc/pdf/commsos/* doc/pdf/commset/* doc/pdf/misc/*
	cp $(srcdocdir)/*.gif doc/pdf
	cp $(srcdocdir)/title.htm doc/pdf/title$(HTML_EXT)
	rexx $(srcdir)/makehtml.rex src=$(srcdir) ext=$(HTML_EXT) ver=$(VERSION) dest=doc/pdf doc=$(srcdocdir) \
		overview=$(OVERVIEW)  \
		comm=$(COMM)      \
		commset=$(COMMSET)   \
		commsos=$(COMMSOS)   \
		query=$(QUERY)     \
		glossary=$(GLOSSARY)  \
		appendix1=$(APPENDIX1) \
		appendix2=$(APPENDIX2) \
		appendix3=$(APPENDIX3) \
		appendix4=$(APPENDIX4) \
		appendix7=$(APPENDIX7)
	./$(THEBIN) -w 15000 -b -p $(srcdocdir)/preparecomb.the -a "pdf $(VERSION)" \
		doc/pdf/title$(HTML_EXT) \
		doc/pdf/overview$(HTML_EXT) \
		doc/pdf/comm$(HTML_EXT) \
		doc/pdf/commset$(HTML_EXT) \
		doc/pdf/commsos$(HTML_EXT) \
		doc/pdf/query$(HTML_EXT) \
		doc/pdf/glossary$(HTML_EXT) \
		doc/pdf/app1$(HTML_EXT) \
		doc/pdf/app2$(HTML_EXT) \
		doc/pdf/app3$(HTML_EXT) \
		doc/pdf/app4$(HTML_EXT) \
		doc/pdf/app5$(HTML_EXT) \
		doc/pdf/app6$(HTML_EXT) \
		doc/pdf/app7$(HTML_EXT) \
		doc/pdf/history$(HTML_EXT) \
		doc/pdf/quickref$(HTML_EXT) \
		doc/pdf/fdl$(HTML_EXT) title$(HTML_EXT)
	-htmldoc -v --firstpage p1 --titlefile doc/pdf/title$(HTML_EXT).new --compression=9 --bodycolor "#FFFFFF" --size universal -f doc/pdf/the-$(VERSION).pdf doc/pdf/overview$(HTML_EXT).new doc/pdf/comm$(HTML_EXT).new doc/pdf/commset$(HTML_EXT).new doc/pdf/commsos$(HTML_EXT).new doc/pdf/query$(HTML_EXT).new doc/pdf/glossary$(HTML_EXT).new doc/pdf/app1$(HTML_EXT).new doc/pdf/app2$(HTML_EXT).new doc/pdf/app3$(HTML_EXT).new doc/pdf/app4$(HTML_EXT).new doc/pdf/app5$(HTML_EXT).new doc/pdf/app6$(HTML_EXT).new doc/pdf/app7$(HTML_EXT).new doc/pdf/history$(HTML_EXT).new doc/pdf/quickref$(HTML_EXT).new doc/pdf/fdl$(HTML_EXT).new
	rm -f doc/pdf/*.new doc/pdf/*.man doc/pdf/*.html doc/pdf/*.gif

website: helpviewer
	-mkdir images
	-cp $(srcdir)/images/*.png images
	-rm thedoc*.zip
	zip -r thedoc$(VERSION_NODOT) index.html images comm commset commsos misc

perminstall:
	-chmod u+x $(INSTALL)

install: perminstall $(BASE_INSTALL) installbinary

uninstall: un$(BASE_INSTALL)

installbase: the the.man THE_Help.txt installrpm installcommon

uninstallbase: uninstallrpm uninstalldoc uninstallcommon

installrpm: perminstall $(THEBIN)
	$(INSTALL) -m 755 -d $(DESTDIR)$(bindir)
#	$(INSTALL) -m 755 -c ./the $(DESTDIR)$(bindir)/the
	$(INSTALL) -m 755 -c $(THEBIN) $(DESTDIR)$(bindir)/$(THEBIN)

uninstallrpm: $(THEBIN)
	-rm -f $(DESTDIR)$(bindir)/the
	-rm -f $(DESTDIR)$(bindir)/$(THEBIN)
	-rmdir $(DESTDIR)$(bindir)

installbinary: $(THEBIN)
	$(INSTALL) -m 755 -d $(DESTDIR)$(bindir)
	$(INSTALL) -m 755 -c ./$(THEBIN) $(DESTDIR)$(bindir)/$(THEBIN)

#installdoc: the.man THE_Help.txt html helpviewer
installdoc:
	-$(INSTALL) -m 755 -d $(DESTDIR)$(docdir)
	-$(INSTALL) -c -m 644 $(srcdir)/images/the64.png $(DESTDIR)$(docdir)/the64.png
	for a in doc/html/*.html; do \
		bn=`basename $$a`; \
		$(INSTALL) -m 644 -c $$a $(DESTDIR)$(docdir)/$$bn; \
	done;

installcommon: perminstall the
	$(INSTALL) -m 755 -d $(DESTDIR)$(bindir)
	$(INSTALL) -m 755 -c ./the $(DESTDIR)$(bindir)/the
	$(INSTALL) -m 755 -d $(DESTDIR)$(mandir)/man1
	$(INSTALL) -m 644 -c $(srcdir)/the.1        $(DESTDIR)$(mandir)/man1/the.1
	gzip -f $(DESTDIR)$(mandir)/man1/the.1
	$(INSTALL) -m 755 -d $(DESTDIR)$(datadir)/the
	$(INSTALL) -m 644 -c ./THE_Help.txt         $(DESTDIR)$(datadir)/the/THE_Help.txt
	$(INSTALL) -m 644 -c $(macrodir)/append.the   $(DESTDIR)$(datadir)/the/append.the
	$(INSTALL) -m 644 -c $(macrodir)/comm.the     $(DESTDIR)$(datadir)/the/comm.the
	$(INSTALL) -m 644 -c $(macrodir)/build.the    $(DESTDIR)$(datadir)/the/build.the
	$(INSTALL) -m 644 -c $(macrodir)/uncomm.the   $(DESTDIR)$(datadir)/the/uncomm.the
	$(INSTALL) -m 644 -c $(macrodir)/total.the    $(DESTDIR)$(datadir)/the/total.the
	$(INSTALL) -m 644 -c $(macrodir)/match.the    $(DESTDIR)$(datadir)/the/match.the
	$(INSTALL) -m 644 -c $(macrodir)/fold.the     $(DESTDIR)$(datadir)/the/fold.the
	$(INSTALL) -m 644 -c $(macrodir)/rm.the       $(DESTDIR)$(datadir)/the/rm.the
	$(INSTALL) -m 644 -c $(macrodir)/nl.the       $(DESTDIR)$(datadir)/the/nl.the
	$(INSTALL) -m 644 -c $(macrodir)/words.the    $(DESTDIR)$(datadir)/the/words.the
	$(INSTALL) -m 644 -c $(macrodir)/l.the        $(DESTDIR)$(datadir)/the/l.the
	$(INSTALL) -m 644 -c $(macrodir)/compile.the  $(DESTDIR)$(datadir)/the/compile.the
	$(INSTALL) -m 644 -c $(macrodir)/spell.the    $(DESTDIR)$(datadir)/the/spell.the
	$(INSTALL) -m 644 -c $(macrodir)/demo.the     $(DESTDIR)$(datadir)/the/demo.the
	$(INSTALL) -m 644 -c $(macrodir)/demo.txt     $(DESTDIR)$(datadir)/the/demo.txt
	$(INSTALL) -m 644 -c $(macrodir)/cua.the      $(DESTDIR)$(datadir)/the/cua.the
	$(INSTALL) -m 644 -c $(macrodir)/config.the   $(DESTDIR)$(datadir)/the/config.the
	$(INSTALL) -m 644 -c $(macrodir)/tags.the     $(DESTDIR)$(datadir)/the/tags.the
	$(INSTALL) -m 644 -c $(macrodir)/codecomp.the $(DESTDIR)$(datadir)/the/codecomp.the
	$(INSTALL) -m 644 -c $(macrodir)/complete.the $(DESTDIR)$(datadir)/the/complete.the
	$(INSTALL) -m 644 -c $(macrodir)/completer.the $(DESTDIR)$(datadir)/the/completer.the
	$(INSTALL) -m 644 -c $(macrodir)/syntax.the   $(DESTDIR)$(datadir)/the/syntax.the
	$(INSTALL) -m 644 -c $(macrodir)/diff.the     $(DESTDIR)$(datadir)/the/diff.the
	$(INSTALL) -m 644 -c $(macrodir)/remote.the   $(DESTDIR)$(datadir)/the/remote.the
	for a in $(srcdir)/*.tld; do \
		bn=`basename $$a`; \
		$(INSTALL) -m 644 -c $$a $(DESTDIR)$(datadir)/the/$$bn; \
	done;
	for a in $(srcdir)/syntax/*.syntax; do \
		bn=`basename $$a`; \
		$(INSTALL) -m 644 -c $$a $(DESTDIR)$(datadir)/the/$$bn; \
	done;

uninstalldoc:

uninstallcommon:
	-rm -f $(DESTDIR)$(bindir)/the
	-rm -f $(DESTDIR)$(mandir)/man1/the.1.gz
	-rm -f $(DESTDIR)$(datadir)/the/THE_Help.txt
	-rm -f $(DESTDIR)$(datadir)/the/append.the
	-rm -f $(DESTDIR)$(datadir)/the/comm.the
	-rm -f $(DESTDIR)$(datadir)/the/build.the
	-rm -f $(DESTDIR)$(datadir)/the/uncomm.the
	-rm -f $(DESTDIR)$(datadir)/the/total.the
	-rm -f $(DESTDIR)$(datadir)/the/match.the
	-rm -f $(DESTDIR)$(datadir)/the/fold.the
	-rm -f $(DESTDIR)$(datadir)/the/rm.the
	-rm -f $(DESTDIR)$(datadir)/the/nl.the
	-rm -f $(DESTDIR)$(datadir)/the/words.the
	-rm -f $(DESTDIR)$(datadir)/the/l.the
	-rm -f $(DESTDIR)$(datadir)/the/compile.the
	-rm -f $(DESTDIR)$(datadir)/the/spell.the
	-rm -f $(DESTDIR)$(datadir)/the/demo.the
	-rm -f $(DESTDIR)$(datadir)/the/demo.txt
	-rm -f $(DESTDIR)$(datadir)/the/cua.the
	-rm -f $(DESTDIR)$(datadir)/the/config.the
	-rm -f $(DESTDIR)$(datadir)/the/tags.the
	-rm -f $(DESTDIR)$(datadir)/the/codecomp.the
	-rm -f $(DESTDIR)$(datadir)/the/syntax.the
	-rm -f $(DESTDIR)$(datadir)/the/diff.the
	-rm -f $(DESTDIR)$(datadir)/the/remote.the
	for a in $(srcdir)/*.tld; do \
		bn=`basename $$a`; \
		rm -f $(DESTDIR)$(datadir)/the/$$bn; \
	done;
	for a in $(srcdir)/syntax/*.syntax; do \
		bn=`basename $$a`; \
		rm -f $(DESTDIR)$(datadir)/the/$$bn; \
	done;
	-rmdir $(DESTDIR)$(datadir)/the

binary: $(BASE_BINARY)

binarybase: $(THEBIN) the the.man THE_Help.txt
	-rm -fr ./tmpdir
	$(INSTALL) -m 755 -d ./tmpdir/bin
	$(INSTALL) -m 755 -d ./tmpdir/man/man1
	$(INSTALL) -m 755 -d ./tmpdir/share/the
	$(INSTALL) -m 755 -c ./$(THEBIN) ./tmpdir/bin/$(THEBIN)
	$(INSTALL) -m 755 -c ./the ./tmpdir/bin/the
	$(INSTALL) -m 644 -c $(srcdir)/the.1 ./tmpdir/man/man1/the.1
	$(INSTALL) -m 644 -c ./THE_Help.txt ./tmpdir/share/the/THE_Help.txt
	$(INSTALL) -m 644 -c $(macrodir)/append.the   ./tmpdir/share/the/append.the
	$(INSTALL) -m 644 -c $(macrodir)/comm.the     ./tmpdir/share/the/comm.the
	$(INSTALL) -m 644 -c $(macrodir)/build.the    ./tmpdir/share/the/build.the
	$(INSTALL) -m 644 -c $(macrodir)/uncomm.the   ./tmpdir/share/the/uncomm.the
	$(INSTALL) -m 644 -c $(macrodir)/total.the    ./tmpdir/share/the/total.the
	$(INSTALL) -m 644 -c $(macrodir)/match.the    ./tmpdir/share/the/match.the
	$(INSTALL) -m 644 -c $(macrodir)/fold.the     ./tmpdir/share/the/fold.the
	$(INSTALL) -m 644 -c $(macrodir)/rm.the       ./tmpdir/share/the/rm.the
	$(INSTALL) -m 644 -c $(macrodir)/nl.the       ./tmpdir/share/the/nl.the
	$(INSTALL) -m 644 -c $(macrodir)/words.the    ./tmpdir/share/the/words.the
	$(INSTALL) -m 644 -c $(macrodir)/l.the        ./tmpdir/share/the/l.the
	$(INSTALL) -m 644 -c $(macrodir)/compile.the  ./tmpdir/share/the/compile.the
	$(INSTALL) -m 644 -c $(macrodir)/spell.the    ./tmpdir/share/the/spell.the
	$(INSTALL) -m 644 -c $(macrodir)/demo.the     ./tmpdir/share/the/demo.the
	$(INSTALL) -m 644 -c $(macrodir)/demo.txt     ./tmpdir/share/the/demo.txt
	$(INSTALL) -m 644 -c $(macrodir)/cua.the      ./tmpdir/share/the/cua.the
	$(INSTALL) -m 644 -c $(macrodir)/config.the   ./tmpdir/share/the/config.the
	$(INSTALL) -m 644 -c $(macrodir)/tags.the     ./tmpdir/share/the/tags.the
	$(INSTALL) -m 644 -c $(macrodir)/codecomp.the ./tmpdir/share/the/codecomp.the
	$(INSTALL) -m 644 -c $(macrodir)/complete.the ./tmpdir/share/the/complete.the
	$(INSTALL) -m 644 -c $(macrodir)/completer.the ./tmpdir/share/the/completer.the
	$(INSTALL) -m 644 -c $(macrodir)/syntax.the   ./tmpdir/share/the/syntax.the
	$(INSTALL) -m 644 -c $(macrodir)/diff.the     ./tmpdir/share/the/diff.the
	$(INSTALL) -m 644 -c $(macrodir)/remote.the   ./tmpdir/share/the/remote.the
	for a in $(srcdir)/*.tld; do \
		bn=`basename $$a`; \
		$(INSTALL) -m 644 -c $$a ./tmpdir/share/the/$$bn; \
	done;
	for a in $(srcdir)/syntax/*.syntax; do \
		bn=`basename $$a`; \
		$(INSTALL) -m 644 -c $$a ./tmpdir/share/the/$$bn; \
	done;
	(cd ./tmpdir; \
	tar cvf - * | gzip > $(packagesdir)/the-$(THETYPE)-regina-$(VERSION)-$(MACH_ARCH)-$(MYDISTRO).tgz )

cygwinbinary: the.exe the.man THE_Help.txt
	-rm -fr ./tmpdir
	$(INSTALL) -m 755 -d ./tmpdir/bin
	$(INSTALL) -m 755 -d ./tmpdir/man/man1
	$(INSTALL) -m 755 -d ./tmpdir/share/the
	$(INSTALL) -m 755 -c ./the.exe ./tmpdir/bin/the.exe
	$(INSTALL) -m 644 -c $(srcdir)/the.1 ./tmpdir/man/man1/the.1
	$(INSTALL) -m 644 -c ./THE_Help.txt ./tmpdir/share/the/THE_Help.txt
	$(INSTALL) -m 644 -c $(macrodir)/append.the   ./tmpdir/share/the/append.the
	$(INSTALL) -m 644 -c $(macrodir)/comm.the     ./tmpdir/share/the/comm.the
	$(INSTALL) -m 644 -c $(macrodir)/build.the    ./tmpdir/share/the/build.the
	$(INSTALL) -m 644 -c $(macrodir)/uncomm.the   ./tmpdir/share/the/uncomm.the
	$(INSTALL) -m 644 -c $(macrodir)/total.the    ./tmpdir/share/the/total.the
	$(INSTALL) -m 644 -c $(macrodir)/match.the    ./tmpdir/share/the/match.the
	$(INSTALL) -m 644 -c $(macrodir)/fold.the     ./tmpdir/share/the/fold.the
	$(INSTALL) -m 644 -c $(macrodir)/rm.the       ./tmpdir/share/the/rm.the
	$(INSTALL) -m 644 -c $(macrodir)/nl.the       ./tmpdir/share/the/nl.the
	$(INSTALL) -m 644 -c $(macrodir)/words.the    ./tmpdir/share/the/words.the
	$(INSTALL) -m 644 -c $(macrodir)/l.the        ./tmpdir/share/the/l.the
	$(INSTALL) -m 644 -c $(macrodir)/compile.the  ./tmpdir/share/the/compile.the
	$(INSTALL) -m 644 -c $(macrodir)/spell.the    ./tmpdir/share/the/spell.the
	$(INSTALL) -m 644 -c $(macrodir)/demo.the     ./tmpdir/share/the/demo.the
	$(INSTALL) -m 644 -c $(macrodir)/demo.the     ./tmpdir/share/the/demo.txt
	$(INSTALL) -m 644 -c $(macrodir)/cua.the      ./tmpdir/share/the/cua.the
	$(INSTALL) -m 644 -c $(macrodir)/config.the   ./tmpdir/share/the/config.the
	$(INSTALL) -m 644 -c $(macrodir)/tags.the     ./tmpdir/share/the/tags.the
	$(INSTALL) -m 644 -c $(macrodir)/codecomp.the ./tmpdir/share/the/codecomp.the
	$(INSTALL) -m 644 -c $(macrodir)/complete.the ./tmpdir/share/the/complete.the
	$(INSTALL) -m 644 -c $(macrodir)/completer.the ./tmpdir/share/the/completer.the
	$(INSTALL) -m 644 -c $(macrodir)/syntax.the   ./tmpdir/share/the/syntax.the
	$(INSTALL) -m 644 -c $(macrodir)/diff.the     ./tmpdir/share/the/diff.the
	$(INSTALL) -m 644 -c $(macrodir)/remote.the   ./tmpdir/share/the/remote.the
	for a in $(srcdir)/*.tld; do \
		bn=`basename $$a`; \
		$(INSTALL) -m 644 -c $$a ./tmpdir/share/the/$$bn; \
	done;
	for a in $(srcdir)/syntax/*.syntax; do \
		bn=`basename $$a`; \
		$(INSTALL) -m 644 -c $$a ./tmpdir/share/the/$$bn; \
	done;
	(cd ./tmpdir; \
	tar cvf - * | gzip > ../the-$(THETYPE)-$(VERSION)-$(target).tgz )

haikuinstall: all
	$(INSTALL) -m 755 -d $(DESTDIR)/bin
	$(INSTALL) -m 755 -d $(DESTDIR)/data/licenses
	$(INSTALL) -m 755 -d $(DESTDIR)/documentation/man/man1
	$(INSTALL) -m 755 -d $(DESTDIR)/data/the
	$(INSTALL) -m 755 -c ./the $(DESTDIR)/bin/the
	$(INSTALL) -m 744 -c $(srcdir)/the.1 $(DESTDIR)/documentation/man/man1/the.1
	$(INSTALL) -m 666 -c $(srcdir)/COPYING $(DESTDIR)/data/licenses/COPYING
	-mv $(DESTDIR)/data/licenses/COPYING "$(DESTDIR)/data/licenses/GNU GPL v2"
	$(INSTALL) -m 644 -c $(macrodir)/append.the   $(DESTDIR)/data/the/append.the
	$(INSTALL) -m 644 -c $(macrodir)/comm.the     $(DESTDIR)/data/the/comm.the
	$(INSTALL) -m 644 -c $(macrodir)/build.the    $(DESTDIR)/data/the/build.the
	$(INSTALL) -m 644 -c $(macrodir)/uncomm.the   $(DESTDIR)/data/the/uncomm.the
	$(INSTALL) -m 644 -c $(macrodir)/total.the    $(DESTDIR)/data/the/total.the
	$(INSTALL) -m 644 -c $(macrodir)/fold.the     $(DESTDIR)/data/the/fold.the
	$(INSTALL) -m 644 -c $(macrodir)/rm.the       $(DESTDIR)/data/the/rm.the
	$(INSTALL) -m 644 -c $(macrodir)/nl.the       $(DESTDIR)/data/the/nl.the
	$(INSTALL) -m 644 -c $(macrodir)/words.the    $(DESTDIR)/data/the/words.the
	$(INSTALL) -m 644 -c $(macrodir)/l.the        $(DESTDIR)/data/the/l.the
	$(INSTALL) -m 644 -c $(macrodir)/compile.the  $(DESTDIR)/data/the/compile.the
	$(INSTALL) -m 644 -c $(macrodir)/spell.the    $(DESTDIR)/data/the/spell.the
	$(INSTALL) -m 644 -c $(macrodir)/demo.the     $(DESTDIR)/data/the/demo.the
	$(INSTALL) -m 644 -c $(macrodir)/demo.txt     $(DESTDIR)/data/the/demo.txt
	$(INSTALL) -m 644 -c $(macrodir)/cua.the      $(DESTDIR)/data/the/cua.the
	$(INSTALL) -m 644 -c $(macrodir)/config.the   $(DESTDIR)/data/the/config.the
	$(INSTALL) -m 644 -c $(macrodir)/tags.the     $(DESTDIR)/data/the/tags.the
	$(INSTALL) -m 644 -c $(macrodir)/codecomp.the $(DESTDIR)/data/the/codecomp.the
	$(INSTALL) -m 644 -c $(macrodir)/complete.the $(DESTDIR)/data/the/complete.the
	$(INSTALL) -m 644 -c $(macrodir)/completer.the $(DESTDIR)/data/the/completer.the
	$(INSTALL) -m 644 -c $(macrodir)/syntax.the   $(DESTDIR)/data/the/syntax.the
	$(INSTALL) -m 644 -c $(macrodir)/diff.the     $(DESTDIR)/data/the/diff.the
	$(INSTALL) -m 644 -c $(macrodir)/remote.the   $(DESTDIR)/data/the/remote.the
	for a in $(srcdir)/*.tld; do \
		bn=`basename $$a`; \
		$(INSTALL) -m 644 -c $$a $(DESTDIR)/data/the/$$bn; \
	done;
	for a in $(srcdir)/syntax/*.syntax; do \
		bn=`basename $$a`; \
		$(INSTALL) -m 644 -c $$a $(DESTDIR)/data/the/$$bn; \
	done;
	$(INSTALL) -m 644 -c ./haiku/PackageInfo $(DESTDIR)/.PackageInfo

haikuinstall-variant: all
	$(INSTALL) -m 755 -d $(DESTDIR)/data/licenses
	$(INSTALL) -m 755 -d $(DESTDIR)/bin
	$(INSTALL) -m 666 -c $(srcdir)/COPYING $(DESTDIR)/data/licenses/COPYING
	-mv $(DESTDIR)/data/licenses/COPYING "$(DESTDIR)/data/licenses/GNU GPL v2"
	$(INSTALL) -m 755 -c ./the-conw $(DESTDIR)/bin/the-conw
	$(INSTALL) -m 644 -c ./haiku/PackageInfo-variant $(DESTDIR)/.PackageInfo

haikuinstall-doc: all
	$(INSTALL) -m 755 -d $(DESTDIR)/documentation/packages/$(THE_DIR)
	-$(INSTALL) -c -m 644 $(srcdir)/images/the64.png $(DESTDIR)/documentation/packages/$(THE_DIR)/the64.png
	for a in doc/html/*.html; do \
		bn=`basename $$a`; \
		$(INSTALL) -m 644 -c $$a $(DESTDIR)/documentation/packages/$(THE_DIR)/$$bn; \
	done;
	$(INSTALL) -m 644 -c ./haiku/PackageInfo-doc $(DESTDIR)/.PackageInfo

clean:
	-rm -f *.o the-obj/*.o pdc-obj/*.o pdc-obj/libpdcurses.a trace $(THEBIN) the manext *.man *$(HTML_EXT) *.new config.cache config.status

distclean: clean
	-rm -f Makefile

docclean:
	-rm -fr *.man *$(HTML_EXT) *.png *.new misc comm commset commsos images THE_Help* doc

mostlyclean: clean

realclean: distclean

#------------------------------------------------------------------------

XTRA_OBJS =  

OBJS =      \
	the-obj/box.o      \
	the-obj/colour.o   \
	the-obj/column.o   \
	the-obj/comm1.o    \
	the-obj/comm2.o    \
	the-obj/comm3.o    \
	the-obj/comm4.o    \
	the-obj/comm5.o    \
	the-obj/commset1.o \
	the-obj/commset2.o \
	the-obj/commsos.o  \
	the-obj/commutil.o \
	the-obj/cursor.o   \
	the-obj/default.o  \
	the-obj/directry.o \
	the-obj/edit.o     \
	the-obj/error.o    \
	the-obj/execute.o  \
	the-obj/file.o     \
	the-obj/thematch.o \
	the-obj/getch.o    \
	the-obj/linked.o   \
	the-obj/mouse.o    \
	the-obj/memory.o   \
	the-obj/nonansi.o  \
	the-obj/parser.o   \
	the-obj/prefix.o   \
	the-obj/print.o    \
	the-obj/query.o    \
	the-obj/query1.o   \
	the-obj/query2.o   \
	the-obj/regex.o    \
	the-obj/reserved.o \
	the-obj/rexx.o     \
	the-obj/scroll.o   \
	the-obj/show.o     \
	the-obj/single.o   \
	the-obj/sort.o     \
	the-obj/target.o   \
	the-obj/the.o      \
	the-obj/mygetopt.o \
	the-obj/util.o

LIBCCFLAGS= -I.   -DHAVE_CONFIG_H $(DEBUG) $(CFLAGS)
PDCCCFLAGS= -I.   -DHAVE_CONFIG_H $(DEBUG) $(CFLAGS)
PDCFONTPATH = 

PDCLIBOBJS = pdc-obj/addch.o pdc-obj/addchstr.o pdc-obj/addstr.o pdc-obj/attr.o pdc-obj/beep.o \
pdc-obj/bkgd.o pdc-obj/border.o pdc-obj/clear.o pdc-obj/color.o pdc-obj/delch.o pdc-obj/deleteln.o \
pdc-obj/getch.o pdc-obj/getstr.o pdc-obj/getyx.o pdc-obj/inch.o pdc-obj/inchstr.o \
pdc-obj/initscr.o pdc-obj/inopts.o pdc-obj/insch.o pdc-obj/insstr.o pdc-obj/instr.o pdc-obj/kernel.o \
pdc-obj/keyname.o pdc-obj/mouse.o pdc-obj/move.o pdc-obj/outopts.o pdc-obj/overlay.o pdc-obj/pad.o \
pdc-obj/panel.o pdc-obj/printw.o pdc-obj/refresh.o pdc-obj/scanw.o pdc-obj/scr_dump.o pdc-obj/scroll.o \
pdc-obj/slk.o pdc-obj/termattr.o pdc-obj/touch.o pdc-obj/util.o pdc-obj/window.o pdc-obj/debug.o

pdc-obj/addch.o: $(sourcedir)/PDCursesMod/pdcurses/addch.c
	$(CC) $(LIBCCFLAGS)  -c -o $@ $<
pdc-obj/addchstr.o: $(sourcedir)/PDCursesMod/pdcurses/addchstr.c
	$(CC) $(LIBCCFLAGS)  -c -o $@ $<
pdc-obj/addstr.o : $(sourcedir)/PDCursesMod/pdcurses/addstr.c
	$(CC) $(LIBCCFLAGS)  -c -o $@ $<
pdc-obj/attr.o : $(sourcedir)/PDCursesMod/pdcurses/attr.c
	$(CC) $(LIBCCFLAGS)  -c -o $@ $<
pdc-obj/beep.o: $(sourcedir)/PDCursesMod/pdcurses/beep.c
	$(CC) $(LIBCCFLAGS)  -c -o $@ $<
pdc-obj/bkgd.o : $(sourcedir)/PDCursesMod/pdcurses/bkgd.c
	$(CC) $(LIBCCFLAGS)  -c -o $@ $<
pdc-obj/border.o : $(sourcedir)/PDCursesMod/pdcurses/border.c
	$(CC) $(LIBCCFLAGS)  -c -o $@ $<
pdc-obj/clear.o : $(sourcedir)/PDCursesMod/pdcurses/clear.c
	$(CC) $(LIBCCFLAGS)  -c -o $@ $<
pdc-obj/color.o : $(sourcedir)/PDCursesMod/pdcurses/color.c
	$(CC) $(LIBCCFLAGS)  -c -o $@ $<
pdc-obj/delch.o : $(sourcedir)/PDCursesMod/pdcurses/delch.c
	$(CC) $(LIBCCFLAGS)  -c -o $@ $<
pdc-obj/deleteln.o: $(sourcedir)/PDCursesMod/pdcurses/deleteln.c
	$(CC) $(LIBCCFLAGS)  -c -o $@ $<
pdc-obj/getch.o : $(sourcedir)/PDCursesMod/pdcurses/getch.c
	$(CC) $(LIBCCFLAGS)  -c -o $@ $<
pdc-obj/getstr.o : $(sourcedir)/PDCursesMod/pdcurses/getstr.c
	$(CC) $(LIBCCFLAGS)  -c -o $@ $<
pdc-obj/getyx.o : $(sourcedir)/PDCursesMod/pdcurses/getyx.c
	$(CC) $(LIBCCFLAGS)  -c -o $@ $<
pdc-obj/inch.o : $(sourcedir)/PDCursesMod/pdcurses/inch.c
	$(CC) $(LIBCCFLAGS)  -c -o $@ $<
pdc-obj/inchstr.o: $(sourcedir)/PDCursesMod/pdcurses/inchstr.c
	$(CC) $(LIBCCFLAGS)  -c -o $@ $<
pdc-obj/initscr.o : $(sourcedir)/PDCursesMod/pdcurses/initscr.c
	$(CC) $(LIBCCFLAGS)  -c -o $@ $<
pdc-obj/inopts.o : $(sourcedir)/PDCursesMod/pdcurses/inopts.c
	$(CC) $(LIBCCFLAGS)  -c -o $@ $<
pdc-obj/insch.o : $(sourcedir)/PDCursesMod/pdcurses/insch.c
	$(CC) $(LIBCCFLAGS)  -c -o $@ $<
pdc-obj/insstr.o : $(sourcedir)/PDCursesMod/pdcurses/insstr.c
	$(CC) $(LIBCCFLAGS)  -c -o $@ $<
pdc-obj/instr.o : $(sourcedir)/PDCursesMod/pdcurses/instr.c
	$(CC) $(LIBCCFLAGS)  -c -o $@ $<
pdc-obj/kernel.o: $(sourcedir)/PDCursesMod/pdcurses/kernel.c
	$(CC) $(LIBCCFLAGS)  -c -o $@ $<
pdc-obj/keyname.o : $(sourcedir)/PDCursesMod/pdcurses/keyname.c
	$(CC) $(LIBCCFLAGS)  -c -o $@ $<
pdc-obj/mouse.o : $(sourcedir)/PDCursesMod/pdcurses/mouse.c
	$(CC) $(LIBCCFLAGS)  -c -o $@ $<
pdc-obj/move.o : $(sourcedir)/PDCursesMod/pdcurses/move.c
	$(CC) $(LIBCCFLAGS)  -c -o $@ $<
pdc-obj/outopts.o : $(sourcedir)/PDCursesMod/pdcurses/outopts.c
	$(CC) $(LIBCCFLAGS)  -c -o $@ $<
pdc-obj/overlay.o : $(sourcedir)/PDCursesMod/pdcurses/overlay.c
	$(CC) $(LIBCCFLAGS)  -c -o $@ $<
pdc-obj/pad.o: $(sourcedir)/PDCursesMod/pdcurses/pad.c
	$(CC) $(LIBCCFLAGS)  -c -o $@ $<
pdc-obj/panel.o : $(sourcedir)/PDCursesMod/pdcurses/panel.c
	$(CC) $(LIBCCFLAGS)  -c -o $@ $<
pdc-obj/printw.o : $(sourcedir)/PDCursesMod/pdcurses/printw.c
	$(CC) $(LIBCCFLAGS)  -c -o $@ $<
pdc-obj/refresh.o : $(sourcedir)/PDCursesMod/pdcurses/refresh.c
	$(CC) $(LIBCCFLAGS)  -c -o $@ $<
pdc-obj/scanw.o : $(sourcedir)/PDCursesMod/pdcurses/scanw.c
	$(CC) $(LIBCCFLAGS)  -c -o $@ $<
pdc-obj/scr_dump.o : $(sourcedir)/PDCursesMod/pdcurses/scr_dump.c
	$(CC) $(LIBCCFLAGS)  -c -o $@ $<
pdc-obj/scroll.o: $(sourcedir)/PDCursesMod/pdcurses/scroll.c
	$(CC) $(LIBCCFLAGS)  -c -o $@ $<
pdc-obj/slk.o : $(sourcedir)/PDCursesMod/pdcurses/slk.c
	$(CC) $(LIBCCFLAGS)  -c -o $@ $<
pdc-obj/termattr.o : $(sourcedir)/PDCursesMod/pdcurses/termattr.c
	$(CC) $(LIBCCFLAGS)  -c -o $@ $<
pdc-obj/touch.o : $(sourcedir)/PDCursesMod/pdcurses/touch.c
	$(CC) $(LIBCCFLAGS)  -c -o $@ $<
pdc-obj/util.o : $(sourcedir)/PDCursesMod/pdcurses/util.c
	$(CC) $(LIBCCFLAGS)  -c -o $@ $<
pdc-obj/window.o : $(sourcedir)/PDCursesMod/pdcurses/window.c
	$(CC) $(LIBCCFLAGS)  -c -o $@ $<
pdc-obj/debug.o: $(sourcedir)/PDCursesMod/pdcurses/debug.c
	$(CC) $(LIBCCFLAGS)  -c -o $@ $<

PDCOBJS = pdc-obj/pdcclip.o pdc-obj/pdcdisp.o pdc-obj/pdcgetsc.o pdc-obj/pdckbd.o pdc-obj/pdcscrn.o \
pdc-obj/pdcsetsc.o pdc-obj/pdcutil.o 

pdc-obj/pdcclip.o: $(sourcedir)/PDCursesMod//pdcclip.c
	$(CC) $(PDCCCFLAGS)  -c -o $@ $<

pdc-obj/pdcdisp.o: $(sourcedir)/PDCursesMod//pdcdisp.c
	$(CC) $(PDCCCFLAGS)  -c -o $@ $<

pdc-obj/pdcgetsc.o: $(sourcedir)/PDCursesMod//pdcgetsc.c
	$(CC) $(PDCCCFLAGS)  -c -o $@ $<

pdc-obj/pdckbd.o: $(sourcedir)/PDCursesMod//pdckbd.c
	$(CC) $(PDCCCFLAGS)  -c -o $@ $<

pdc-obj/pdcscrn.o: $(sourcedir)/PDCursesMod//pdcscrn.c
	$(CC) $(PDCCCFLAGS) $(PDCFONTPATH)  -c -o $@ $<

pdc-obj/pdcsetsc.o: $(sourcedir)/PDCursesMod//pdcsetsc.c
	$(CC) $(PDCCCFLAGS)  -c -o $@ $<

pdc-obj/pdcutil.o: $(sourcedir)/PDCursesMod//pdcutil.c
	$(CC) $(PDCCCFLAGS)  -c -o $@ $<

pdc-obj/sb.o: $(sourcedir)/PDCursesMod//sb.c
	$(CC) $(PDCCCFLAGS)  -c -o $@ $<

pdc-obj/scrlbox.o: $(sourcedir)/PDCursesMod//scrlbox.c
	$(CC) $(PDCCCFLAGS)  -c -o $@ $<

pdc-obj:
	mkdir -p pdc-obj

$(libpdcurses_target): pdc-obj $(PDCLIBOBJS) $(PDCOBJS)
	ar cr $(libpdcurses_target) $(PDCLIBOBJS) $(PDCOBJS)

the-obj:
	mkdir -p the-obj

the: the-obj the-obj/execthe.o the-obj/mygetopt.o
	$(LINK) the-obj/execthe.o the-obj/mygetopt.o -o the $(THELDFLAGS)

the-con the-conw the-conw8 the-x11 the-x11w the-x11w8 the-sdl1 the-sdl1w the-sdl1w8 the-sdl2 the-sdl2w the-sdl2w8 the-vt the-vtw the-vtw8: the-obj $(OBJS) $(XTRA_OBJS) $(libpdcurses_target)
	$(LINK) $(OBJS) $(XTRA_OBJS) -o $@ $(LDFLAGS)

the-obj/box.o: $(sourcedir)/box.c $(H1)
	$(CC) $(CCFLAGS) -o the-obj/box.o $(sourcedir)/box.c

the-obj/colour.o: $(sourcedir)/colour.c $(H1)
	$(CC) $(CCFLAGS) -o the-obj/colour.o $(sourcedir)/colour.c

the-obj/column.o: $(sourcedir)/column.c $(H1)
	$(CC) $(CCFLAGS) -o the-obj/column.o $(sourcedir)/column.c

the-obj/comm1.o: $(sourcedir)/comm1.c $(H1)
	$(CC) $(CCFLAGS) -o the-obj/comm1.o $(sourcedir)/comm1.c

the-obj/comm2.o: $(sourcedir)/comm2.c $(H1)
	$(CC) $(CCFLAGS) -o the-obj/comm2.o $(sourcedir)/comm2.c

the-obj/comm3.o: $(sourcedir)/comm3.c $(H1)
	$(CC) $(CCFLAGS) -o the-obj/comm3.o $(sourcedir)/comm3.c

the-obj/comm4.o: $(sourcedir)/comm4.c $(H1)
	$(CC) $(CCFLAGS) -o the-obj/comm4.o $(sourcedir)/comm4.c

the-obj/comm5.o: $(sourcedir)/comm5.c $(H1)
	$(CC) $(CCFLAGS) -o the-obj/comm5.o $(sourcedir)/comm5.c

the-obj/commset1.o: $(sourcedir)/commset1.c $(H1)
	$(CC) $(CCFLAGS) -o the-obj/commset1.o $(sourcedir)/commset1.c

the-obj/commset2.o: $(sourcedir)/commset2.c $(H1)
	$(CC) $(CCFLAGS) -o the-obj/commset2.o $(sourcedir)/commset2.c

the-obj/commsos.o: $(sourcedir)/commsos.c $(H1)
	$(CC) $(CCFLAGS) -o the-obj/commsos.o $(sourcedir)/commsos.c

the-obj/commutil.o: $(sourcedir)/commutil.c $(H2)
	$(CC) $(CCFLAGS) -o the-obj/commutil.o $(sourcedir)/commutil.c

the-obj/cursor.o: $(sourcedir)/cursor.c $(H1)
	$(CC) $(CCFLAGS) -o the-obj/cursor.o $(sourcedir)/cursor.c

the-obj/default.o: $(sourcedir)/default.c $(H1)
	$(CC) $(CCFLAGS) -o the-obj/default.o $(sourcedir)/default.c

the-obj/directry.o: $(sourcedir)/directry.c $(H6)
	$(CC) $(CCFLAGS) -o the-obj/directry.o $(sourcedir)/directry.c

the-obj/edit.o: $(sourcedir)/edit.c $(H1)
	$(CC) $(CCFLAGS) -o the-obj/edit.o $(sourcedir)/edit.c

the-obj/error.o: $(sourcedir)/error.c $(H1)
	$(CC) $(CCFLAGS) -o the-obj/error.o $(sourcedir)/error.c

the-obj/execute.o: $(sourcedir)/execute.c $(H1)
	$(CC) $(CCFLAGS) -o the-obj/execute.o $(sourcedir)/execute.c

the-obj/file.o: $(sourcedir)/file.c $(H6)
	$(CC) $(CCFLAGS) -o the-obj/file.o $(sourcedir)/file.c

the-obj/thematch.o: $(sourcedir)/thematch.c $(H5)
	$(CC) $(CCFLAGS) -o the-obj/thematch.o $(sourcedir)/thematch.c

the-obj/getch.o: $(sourcedir)/getch.c $(H3)
	$(CC) $(CCFLAGS) -o the-obj/getch.o $(sourcedir)/getch.c

the-obj/mygetopt.o: $(sourcedir)/mygetopt.c $(H1)
	$(CC) $(CCFLAGS) -o the-obj/mygetopt.o $(sourcedir)/mygetopt.c

the-obj/linked.o: $(sourcedir)/linked.c $(H1)
	$(CC) $(CCFLAGS) -o the-obj/linked.o $(sourcedir)/linked.c

the-obj/mouse.o: $(sourcedir)/mouse.c $(H1)
	$(CC) $(CCFLAGS) -o the-obj/mouse.o $(sourcedir)/mouse.c

the-obj/memory.o: $(sourcedir)/memory.c $(H1)
	$(CC) $(CCFLAGS) -o the-obj/memory.o $(sourcedir)/memory.c

the-obj/nonansi.o: $(sourcedir)/nonansi.c $(H1)
	$(CC) $(CCFLAGS) -o the-obj/nonansi.o $(sourcedir)/nonansi.c

the-obj/parser.o: $(sourcedir)/parser.c $(H1)
	$(CC) $(CCFLAGS) -o the-obj/parser.o $(sourcedir)/parser.c

the-obj/os2eas.o: $(sourcedir)/os2eas.c $(H1)
	$(CC) $(CCFLAGS) -o the-obj/os2eas.o $(sourcedir)/os2eas.c

the-obj/prefix.o: $(sourcedir)/prefix.c $(H1)
	$(CC) $(CCFLAGS) -o the-obj/prefix.o $(sourcedir)/prefix.c

the-obj/print.o: $(sourcedir)/print.c $(H1)
	$(CC) $(CCFLAGS) -o the-obj/print.o $(sourcedir)/print.c

the-obj/query.o: $(sourcedir)/query.c $(H4)
	$(CC) $(CCFLAGS) -o the-obj/query.o $(sourcedir)/query.c

the-obj/query1.o: $(sourcedir)/query1.c $(H4)
	$(CC) $(CCFLAGS) -o the-obj/query1.o $(sourcedir)/query1.c

the-obj/query2.o: $(sourcedir)/query2.c $(H4)
	$(CC) $(CCFLAGS) -o the-obj/query2.o $(sourcedir)/query2.c

the-obj/regex.o: $(sourcedir)/regex.c $(H0)
	$(CC) $(CCFLAGS) -o the-obj/regex.o $(sourcedir)/regex.c

the-obj/reserved.o: $(sourcedir)/reserved.c $(H1)
	$(CC) $(CCFLAGS) -o the-obj/reserved.o $(sourcedir)/reserved.c

the-obj/rexx.o: $(sourcedir)/rexx.c $(H7)
	$(CC) $(CCFLAGS) -o the-obj/rexx.o $(sourcedir)/rexx.c

the-obj/scroll.o: $(sourcedir)/scroll.c $(H1)
	$(CC) $(CCFLAGS) -o the-obj/scroll.o $(sourcedir)/scroll.c

the-obj/show.o: $(sourcedir)/show.c $(H1)
	$(CC) $(CCFLAGS) -o the-obj/show.o $(sourcedir)/show.c

the-obj/single.o: $(sourcedir)/single.c $(H1)
	$(CC) $(CCFLAGS) -o the-obj/single.o $(sourcedir)/single.c

the-obj/sort.o: $(sourcedir)/sort.c $(H1)
	$(CC) $(CCFLAGS) -o the-obj/sort.o $(sourcedir)/sort.c

the-obj/target.o: $(sourcedir)/target.c $(H1)
	$(CC) $(CCFLAGS) -o the-obj/target.o $(sourcedir)/target.c

the-obj/the.o: $(sourcedir)/the.c $(H1)
	$(CC) $(CCFLAGS) -DTHE_VERSION=\"$(VERSION)\" -DTHE_VERSION_DATE=\"$(VER_DATE)\" -o the-obj/the.o $(sourcedir)/the.c

the-obj/trace.o: $(sourcedir)/trace.c $(H1)
	$(CC) $(CCFLAGS) -o the-obj/trace.o $(sourcedir)/trace.c

the-obj/util.o: $(sourcedir)/util.c $(H1)
	$(CC) $(CCFLAGS) -o the-obj/util.o $(sourcedir)/util.c

the-obj/alloca.o: $(sourcedir)/alloca.c
	$(CC) $(CCFLAGS) -o the-obj/alloca.o $(sourcedir)/alloca.c

the-obj/utf8.o: $(contribdir)/utf8.c $(H1)
	$(CC) $(CCFLAGS) -o the-obj/utf8.o $(contribdir)/utf8.c

the-obj/execthe.o: $(sourcedir)/execthe.c $(H1)
	$(CC) $(CCFLAGS) -DTHE_VERSION=\"$(VERSION)\" -DTHE_VERSION_DATE=\"$(VER_DATE)\" -o the-obj/execthe.o $(sourcedir)/execthe.c


manext: manext.o
	$(LINK) manext.o -o manext

manext.o: $(sourcedir)/manext.c $(sourcedir)/the.h
	$(CC) $(CCFLAGS) -I$(sourcedir) -o manext.o $(sourcedir)/manext.c


packages_dir:
	-mkdir -p $(packagesdir)

zip:
	(cd $(srcdir); zip packages/thesrc$(VERSION_NODOT) $(ARCHIVE_FILES) )

dist: packages_dir
	(cd $(srcdir);tar --exclude-vcs -cvf - --transform 's,^,$(THE_DIR)/,' $(ARCHIVE_FILES) | gzip > $(packagesdir)/$(THE_DIR).tar.gz )

bzip: packages_dir
	(cd $(srcdir);tar --exclude-vcs -cvf - --transform 's,^,$(THE_DIR)/,' $(ARCHIVE_FILES) | bzip2 > $(packagesdir)/$(THE_DIR).tar.bz2 )

tar:  packages_dir
	(cd $(srcdir);tar --exclude-vcs -cvf - --transform 's,^,$(THE_DIR)/,' $(ARCHIVE_FILES) | compress > $(packagesdir)/$(THE_DIR).tar.Z )

fixrpmspec:
	rexx $(srcdir)/fixpackage.rexx rpm $(srcdir)/THE.spec.in $(srcdir)/THE.spec $(REXX_BUILD) $(CURSES_BUILD)

rpm: fixrpmspec dist $(rpmtopdir)
	cp $(srcdir)/the64.xpm $(rpmtopdir)/SOURCES
	cp $(packagesdir)/$(THE_DIR).tar.gz $(rpmtopdir)/SOURCES
	-rm $(rpmtopdir)/RPMS/*/the-$(THETYPE)-regina-$(VERSION_UNQUOTED)-[0-9]*.rpm
	-rm $(rpmtopdir)/RPMS/*/the-common-$(VERSION_UNQUOTED)-[0-9]*.rpm
	-rm $(rpmtopdir)/RPMS/*/the-doc-$(VERSION_UNQUOTED)-[0-9]*.rpm
	rpmbuild -ba --clean --define "mybinary $(THEBIN)" --define "myversion $(VERSION_UNQUOTED)" --define "myrelease $(PACKAGE_RELEASE)" --define "myrexx regina" --define "mycurses ncursesw" --define "_topdir " $(srcdir)/THE.spec
	rpmbuild -bs --clean --define "mybinary $(THEBIN)" --define "myversion $(VERSION_UNQUOTED)" --define "myrelease $(PACKAGE_RELEASE)" --define "myrexx none" --define "mycurses none" --define "_topdir " $(srcdir)/THE.spec
	mv $(rpmtopdir)/RPMS/*/the-$(THETYPE)-regina-$(VERSION_UNQUOTED)-[0-9]*.rpm `dirname $(rpmtopdir)/RPMS/*/the-$(THETYPE)-regina-$(VERSION_UNQUOTED)-[0-9]*.rpm`/`basename $(rpmtopdir)/RPMS/*/the-$(THETYPE)-regina-$(VERSION_UNQUOTED)-[0-9]*.rpm .rpm`-$(MYDISTRO).rpm
	mv $(rpmtopdir)/RPMS/*/the-common-$(VERSION_UNQUOTED)-[0-9]*.rpm `dirname $(rpmtopdir)/RPMS/*/the-common-$(VERSION_UNQUOTED)-[0-9]*.rpm`/`basename $(rpmtopdir)/RPMS/*/the-common-$(VERSION_UNQUOTED)-[0-9]*.rpm .rpm`-$(MYDISTRO).rpm
	mv $(rpmtopdir)/RPMS/*/the-doc-$(VERSION_UNQUOTED)-[0-9]*.rpm `dirname $(rpmtopdir)/RPMS/*/the-doc-$(VERSION_UNQUOTED)-[0-9]*.rpm`/`basename $(rpmtopdir)/RPMS/*/the-doc-$(VERSION_UNQUOTED)-[0-9]*.rpm .rpm`-$(MYDISTRO).rpm
	ls -l $(rpmtopdir)/RPMS/*/the*rpm

srcrpm: dist $(rpmtopdir)
	cp $(srcdir)/../$(THE_DIR).tar.gz $(rpmtopdir)/SOURCES
	rpmbuild -ba --define "myversion $(VERSION_UNQUOTED)" --define "myrelease $(PACKAGE_RELEASE)" $(srcdir)/the-doc.spec

$(rpmtopdir):
	-mkdir -p $(rpmtopdir)/SOURCES
	-mkdir -p $(rpmtopdir)/SRPMS
	-mkdir -p $(rpmtopdir)/RPMS
	-mkdir -p $(rpmtopdir)/BUILD
	-mkdir -p $(rpmtopdir)/SPECS

$(debtopdir):
	-mkdir -p $(debtopdir)

deb: dist $(debtopdir)
	-rm -fr tmp
	-mkdir tmp
	(cd tmp; \
	tar zxvf $(packagesdir)/$(THE_DIR).tar.gz; \
	cd $(THE_DIR); \
	rexx ./fixpackage.rexx deb debian/rules.in debian/rules $(REXX_BUILD) $(CURSES_BUILD)  $(CURSES_INC_DIR_BUILD) $(CURSES_LIB_DIR_BUILD); \
	rexx ./fixpackage.rexx deb debian/control.in debian/control $(REXX_BUILD) $(CURSES_BUILD); \
	dpkg-buildpackage -rfakeroot; \
	cd ..; \
	mv the-$(THETYPE)-regina_$(VERSION_BASE_UNQUOTED)-[0-9]*.deb $(debtopdir)/`basename the-$(THETYPE)-regina_$(VERSION_BASE_UNQUOTED)-[0-9]*.deb .deb`-$(MYDISTRO).deb; \
	mv the-common_$(VERSION_BASE_UNQUOTED)-[0-9]*.deb $(debtopdir)/`basename the-common_$(VERSION_BASE_UNQUOTED)-[0-9]*.deb .deb`-$(MYDISTRO).deb; \
	mv the-doc_$(VERSION_BASE_UNQUOTED)-[0-9]*.deb $(debtopdir)/`basename the-doc_$(VERSION_BASE_UNQUOTED)-[0-9]*.deb .deb`-$(MYDISTRO).deb; )
	ls -l $(debtopdir)/*.deb

fixapkbuild:
	rexx $(srcdir)/fixpackage.rexx apk $(srcdir)/APKBUILD $(srcdir)/APKBUILD $(REXX_BUILD) $(CURSES_BUILD)

$(apktopdir):
	-mkdir -p $(apktopdir)

apk: fixapkbuild dist $(apktopdir)
	cp $(packagesdir)/$(THE_DIR).tar.gz .
# we need to copy these files from the alpine directory even though they will be
# overridden by the scripts in APKBUILD because abuild expects them to be in the
# source directory as part of the pre-build sanity check
#	cp $(srcdir)/alpine/$(PACKAGE).post-install .
#	cp $(srcdir)/alpine/$(PACKAGE).pre-deinstall .
	abuild checksum
	abuild -P $(apktopdir)
	sh $(srcdir)/renameapk.sh $(apktopdir) $(abs_srcdir) $(VERSION_UNQUOTED) $(MYDISTRO) the the-doc the-ncursesw_regina

hpkg:
# need to rename the config.h in the current directory as it seems to be used instead
# of the one generated by the following configure
	-mv config.h config.h.save
	-rm -fr $(packagesdir)/HPKG/build
	mkdir -p $(packagesdir)/HPKG/build/the_common
	mkdir -p $(packagesdir)/HPKG/build/the_conw_regina
	mkdir -p $(packagesdir)/HPKG/build/the_doc
	(cd $(packagesdir)/HPKG/build;$(abs_srcdir)/configure --prefix=/boot/system --datarootdir=/boot/system/data --with-rexx=regina --with-curses=ncursesw ; \
	make DESTDIR=$(packagesdir)/HPKG/build/the_common haikuinstall; \
	make DESTDIR=$(packagesdir)/HPKG/build/the_conw_regina haikuinstall-variant; \
	make DESTDIR=$(packagesdir)/HPKG/build/the_doc haikuinstall-doc; )
	(cd $(packagesdir)/HPKG/build/the_common;package create ../../the-4.0-1-x86_64.hpkg;)
	(cd $(packagesdir)/HPKG/build/the_doc;package create ../../the_doc-4.0-1.hpkg;)
	(cd $(packagesdir)/HPKG/build/the_conw_regina;package create ../../the_conw_regina-4.0-1-x86_64.hpkg;)
	-mv config.h.save config.h

check:
# dummy target for Alpine Linux build package; abuild -r
