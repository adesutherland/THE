; Per-user THE installer with a private cREXX runtime; Regina is user-supplied.
Unicode true
RequestExecutionLevel user
SetCompressor /SOLID lzma
ManifestDPIAware true
!include "MUI2.nsh"
!include "LogicLib.nsh"
!include "x64.nsh"
!include "WinMessages.nsh"
!include "nsDialogs.nsh"
!include "FileFunc.nsh"

Var RuntimeChoice
Var BundledRadio
Var InstalledRadio


Name "CRXTHE ${THE_VERSION}"
OutFile "${THE_OUTPUT}"
InstallDir "$LOCALAPPDATA\Programs\THE-cREXX"
InstallDirRegKey HKCU "Software\THE-cREXX" "InstallDir"
VIProductVersion "${THE_FILE_VERSION}"
VIAddVersionKey "ProductName" "CRXTHE"
VIAddVersionKey "FileDescription" "CRXTHE Windows x64 Installer"
VIAddVersionKey "FileVersion" "${THE_VERSION}"
VIAddVersionKey "LegalCopyright" "Original THE copyright Mark Hessling; fork changes CRXTHE contributors"
VIAddVersionKey "Comments" "Independent fork based on THE 4.0; CRXTHE issues: github.com/adesutherland/THE/issues"
!define MUI_ABORTWARNING
!insertmacro MUI_PAGE_DIRECTORY
Page custom RuntimePage RuntimePageLeave
!insertmacro MUI_PAGE_INSTFILES
!insertmacro MUI_UNPAGE_CONFIRM
!insertmacro MUI_UNPAGE_INSTFILES
!insertmacro MUI_LANGUAGE "English"

Function .onInit
  ${IfNot} ${RunningX64}
    MessageBox MB_ICONSTOP "CRXTHE requires 64-bit Windows."
    Abort
  ${EndIf}
  SetRegView 64
  StrCpy $RuntimeChoice "BUNDLED"
  ${GetParameters} $0
  ${GetOptions} $0 "/CREXX=" $1
  ${If} $1 == "INSTALLED"
    StrCpy $RuntimeChoice "INSTALLED"
  ${EndIf}
FunctionEnd

Function RuntimePage
  !insertmacro MUI_HEADER_TEXT "cREXX runtime" "Choose the cREXX installation CRXTHE should use."
  nsDialogs::Create 1018
  Pop $0
  ${If} $0 == error
    Abort
  ${EndIf}
  ${NSD_CreateRadioButton} 0 10u 100% 14u "Use bundled cREXX (default)"
  Pop $BundledRadio
  ${NSD_CreateRadioButton} 0 34u 100% 14u "Use an installed cREXX when available"
  Pop $InstalledRadio
  ${If} $RuntimeChoice == "INSTALLED"
    ${NSD_Check} $InstalledRadio
  ${Else}
    ${NSD_Check} $BundledRadio
  ${EndIf}
  ${NSD_CreateLabel} 0 62u 100% 54u "CRXTHE always includes a bundled copy. If no compatible installed cREXX is available, CRXTHE uses the bundle automatically. Regina is configured separately."
  Pop $0
  nsDialogs::Show
FunctionEnd

Function RuntimePageLeave
  ${NSD_GetState} $InstalledRadio $0
  ${If} $0 == ${BST_CHECKED}
    StrCpy $RuntimeChoice "INSTALLED"
  ${Else}
    StrCpy $RuntimeChoice "BUNDLED"
  ${EndIf}
FunctionEnd

Function un.onInit
  SetRegView 64
FunctionEnd

; Registry API preserves long PATH values and their original representation.
; The directory is environment data, never PowerShell source interpolation.
!macro UpdatePath ACTION
  System::Call 'kernel32::SetEnvironmentVariable(t "THE_INSTALL_BIN", t "$INSTDIR\bin")'
  System::Call 'kernel32::SetEnvironmentVariable(t "THE_PATH_ACTION", t "${ACTION}")'
  nsExec::ExecToStack `"$WINDIR\Sysnative\WindowsPowerShell\v1.0\powershell.exe" -NoProfile -NonInteractive -ExecutionPolicy Bypass -File "$INSTDIR\share\the\installer\update-user-path.ps1"`
  Pop $0
  Pop $1
  System::Call 'kernel32::SetEnvironmentVariable(t "THE_INSTALL_BIN", p 0)'
  System::Call 'kernel32::SetEnvironmentVariable(t "THE_PATH_ACTION", p 0)'
  ${If} $0 != 0
    DetailPrint "PATH update failed: $1"
    SetErrorLevel 1
    Abort
  ${EndIf}
  SendMessage ${HWND_BROADCAST} ${WM_SETTINGCHANGE} 0 "STR:Environment" /TIMEOUT=5000
!macroend

Section "CRXTHE"
  SetOutPath "$INSTDIR"
  File /r "${THE_PAYLOAD}\*"
  FileOpen $0 "$INSTDIR\share\the\runtime-preference" w
  FileWrite $0 "$RuntimeChoice$\r$\n"
  FileClose $0
  ${If} $RuntimeChoice == "INSTALLED"
    DetailPrint "CRXTHE will use an installed cREXX when compatible, otherwise its bundled copy."
  ${Else}
    DetailPrint "CRXTHE will use bundled cREXX."
  ${EndIf}
  WriteUninstaller "$INSTDIR\Uninstall.exe"
  WriteRegStr HKCU "Software\THE-cREXX" "InstallDir" "$INSTDIR"
  WriteRegStr HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\THE-cREXX" "DisplayName" "CRXTHE ${THE_VERSION}"
  WriteRegStr HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\THE-cREXX" "DisplayVersion" "${THE_VERSION}"
  WriteRegStr HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\THE-cREXX" "UninstallString" '"$INSTDIR\Uninstall.exe"'
  WriteRegStr HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\THE-cREXX" "QuietUninstallString" '"$INSTDIR\Uninstall.exe" /S'
  Delete "$SMPROGRAMS\THE cREXX Edition\THE.lnk"
  RMDir "$SMPROGRAMS\THE cREXX Edition"
  CreateDirectory "$SMPROGRAMS\CRXTHE"
  CreateShortCut "$SMPROGRAMS\CRXTHE\CRXTHE.lnk" "$INSTDIR\bin\crxthe.cmd"
  !insertmacro UpdatePath add
SectionEnd

Section "Uninstall"
  !insertmacro UpdatePath remove
  Delete "$SMPROGRAMS\CRXTHE\CRXTHE.lnk"
  RMDir "$SMPROGRAMS\CRXTHE"
  !include "${THE_UNINSTALL_FILES}"
  Delete "$INSTDIR\share\the\runtime-preference"
  RMDir "$INSTDIR\share\the"
  RMDir "$INSTDIR\share"
  Delete "$INSTDIR\Uninstall.exe"
  RMDir "$INSTDIR"
  DeleteRegKey HKCU "Software\THE-cREXX"
  DeleteRegKey HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\THE-cREXX"
SectionEnd
