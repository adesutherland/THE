%define debug_package %{nil}
Summary: The Hessling Editor
%if "%{myrexx}" == "none"
Name: the
%else
Name: the-%{mycurses}-%{myrexx}
%endif
Version: %{myversion}
Release: %{myrelease}
License: GPL
Group: Productivity/Text/Editors
Source: the-%{version}.tar.gz
BuildRoot: %{_tmppath}/%{name}-%{version}-root
Vendor: Mark Hessling
URL: http://hessling-editor.sourceforge.net
#Icon: the64.xpm
%if "%{mycurses}" == "ncurses"
BuildRequires: ncurses-devel
%endif
%if "%{mycurses}" == "ncursesw"
BuildRequires: ncurses-devel
%endif
#Provides: _{name}
%if "%{myrexx}" == "regina"
BuildRequires: regina-rexx libregina3 libregina3-devel
%endif
%if "%{myrexx}" == "oorexx"
BuildRequires: ooRexx
%if 0%{?opensuse_bs}
#BuildRequires:
%endif
Conflicts: the-x11-regina THE-ncurses-regina THE-X11-rexxtrans THE-ncurses-rexxtrans
%endif
%if "%{myrexx}" == "rexxtrans"
BuildRequires: RexxTrans-devel
%if 0%{?opensuse_bs}
BuildRequires: Regina-REXX-lib
BuildRequires: Regina-REXX
%endif
Conflicts: the-X11-regina THE-ncurses-regina THE-X11-oorexx THE-ncurses-oorexx
%endif
#Requires: THE-common
%if "%{mycurses}" == "pdcurses-sdl1"
Requires: SDL
%endif
%if "%{mycurses}" == "pdcurses-sdl1w"
Requires: SDL SDL_ttf
%endif
%if "%{mycurses}" == "pdcurses-sdl2"
Requires: SDL2
%endif
%if "%{mycurses}" == "pdcurses-sdl2w"
Requires: SDL2 SDL2_ttf
%endif

%description
THE is a text editor based on the VM/CMS editor
XEDIT and many features of KEDIT written by Mansfield Software.

THE uses Rexx as its macro language which provides a rich and powerful
capability to extend THE.

%if "%{mycurses}" == "ncurses"
This port executes in text mode and can be run from within an xterm window
or over a telnet connection. It requires ncurses to function.
%endif
%if "%{mycurses}" == "ncursesw"
This port executes in text mode and can be run from within an xterm window
or over a telnet connection. It requires ncursesw to function and supports
files encoded in UTF-8.
%endif
%if "%{mycurses}" == "pdcurses-x11"
This port is a native X11 application.
%endif
%if "%{mycurses}" == "pdcurses-x11w"
This port is a native X11 application and suports files encoded in UTF8.
XCurses to function.
%endif
%if "%{mycurses}" == "pdcurses-sdl1"
This port is a native SDL1 GUI application.
%endif
%if "%{mycurses}" == "pdcurses-sdl1w"
This port is a native SDL1 GUI application, and supports files encoded in UTF8.
%endif
%if "%{mycurses}" == "pdcurses-sdl2"
This port is a native SDL2 GUI application.
%endif
%if "%{mycurses}" == "pdcurses-sdl2w"
This port is a native SDL2 GUI application, and supports files encoded in UTF8.
%endif
For more information on THE, visit http://hessling-editor.sourceforge.net/

For more information on Rexx, visit http://www.rexxla.org

%package -n the-common
Summary: The Hessling Editor Common
Group: Productivity/Text/Editors
%description -n the-common
This package provides an executable which invokes either the text-mode or GUI version
of THE together with sample macros and syntax files.

For more information on THE, visit http://hessling-editor.sourceforge.net/

For more information on Rexx, visit http://www.rexxla.org

%package -n the-doc
Summary: The Hessling Editor Documentation
Group: Productivity/Text/Editors
BuildArch: noarch
%description -n the-doc
This package contains the documentation for THE; The Hessling Editor

THE is a full-screen character mode text editor based on the VM/CMS editor
XEDIT and many features of KEDIT written by Mansfield Software.

THE uses Rexx as its macro language which provides a rich and powerful
capability to extend THE.

For more information on THE, visit http://hessling-editor.sourceforge.net/

For more information on Rexx, visit http://www.rexxla.org

%if 0%{?suse_version} > 910
%define mydocdir %{_prefix}/share/doc/packages/%{name}
%else
%if 0%{?mdkversion} > 2006
%define mydocdir %{_prefix}/share/doc/%{name}
%else
%define mydocdir %{_prefix}/share/doc/%{name}-%{version}
%endif
%endif

%prep
%setup -n the-%{version}

%build
./configure --prefix=%{_prefix} --with-rexx=%{myrexx} --with-curses=%{mycurses}
make
#make the-n the the.man THE_Help.txt helpviewer

%install
rm -fr %{buildroot}
#make DESTDIR=%{buildroot} installrpm installcommon installdoc
make DESTDIR=%{buildroot} installrpm installcommon

%files
%defattr(-,root,root,-)
%if "%{mycurses}" == "ncurses"
 %{_bindir}/the-con
%endif
%if "%{mycurses}" == "ncursesw"
 %{_bindir}/the-conw
%endif
%if "%{mycurses}" == "pdcurses-x11"
 %{_bindir}/the-x11
%endif
%if "%{mycurses}" == "pdcurses-x11w"
 %{_bindir}/the-x11w
%endif
%if "%{mycurses}" == "pdcurses-sdl1"
 %{_bindir}/the-sdl1
%endif
%if "%{mycurses}" == "pdcurses-sdl1w"
 %{_bindir}/the-sdl1w
%endif
%if "%{mycurses}" == "pdcurses-sdl2"
 %{_bindir}/the-sdl2
%endif
%if "%{mycurses}" == "pdcurses-sdl2w"
 %{_bindir}/the-sdl2w
%endif

%post
cd %{_mandir}/man1
%if "%{mycurses}" == "ncurses"
ln -sf ./the.1.gz ./the-con.1.gz
%endif
%if "%{mycurses}" == "ncursesw"
ln -sf ./the.1.gz ./the-conw.1.gz
%endif
%if "%{mycurses}" == "pdcurses-x11"
ln -sf ./the.1.gz ./the-x11.1.gz
%endif
%if "%{mycurses}" == "pdcurses-x11w"
ln -sf ./the.1.gz ./the-x11w.1.gz
%endif
%if "%{mycurses}" == "pdcurses-sdl1"
ln -sf ./the.1.gz ./the-sdl1.1.gz
%endif
%if "%{mycurses}" == "pdcurses-sdl1w"
ln -sf ./the.1.gz ./the-sdl1w.1.gz
%endif
%if "%{mycurses}" == "pdcurses-sdl2"
ln -sf ./the.1.gz ./the-sdl2.1.gz
%endif
%if "%{mycurses}" == "pdcurses-sdl2w"
ln -sf ./the.1.gz ./the-sdl2w.1.gz
%endif

%preun
cd %{_mandir}/man1
if [ "$1" = 0 ] ; then
%if "%{mycurses}" == "ncurses"
   rm -f ./the-con.1.gz
%endif
%if "%{mycurses}" == "ncursesw"
   rm -f ./the-conw.1.gz
%endif
%if "%{mycurses}" == "pdcurses-x11"
   rm -f ./the-x11.1.gz
%endif
%if "%{mycurses}" == "pdcurses-x11w"
   rm -f ./the-x11w.1.gz
%endif
%if "%{mycurses}" == "pdcurses-sdl1"
   rm -f ./the-sdl1.1.gz
%endif
%if "%{mycurses}" == "pdcurses-sdl1w"
   rm -f ./the-sdl1w.1.gz
%endif
%if "%{mycurses}" == "pdcurses-sdl2"
   rm -f ./the-sdl2.1.gz
%endif
%if "%{mycurses}" == "pdcurses-sdl2w"
   rm -f ./the-sdl2w.1.gz
%endif
fi
exit 0

%files -n the-common
%defattr(-,root,root,-)
%{_bindir}/the
%doc %{_mandir}/man1/the.1.gz
%{_datadir}/the


%files -n the-doc
%defattr(-,root,root,-)
%doc COPYING
%doc README
%doc TODO
%doc doc/helpviewer/index.html
%doc doc/helpviewer/misc
%doc doc/helpviewer/commset
%doc doc/helpviewer/commsos
%doc doc/helpviewer/comm
