; Per-user THE installer with a private cREXX runtime; Regina is user-supplied.
Unicode true
RequestExecutionLevel user
SetCompressor /SOLID lzma
ManifestDPIAware true
!include "MUI2.nsh"
!include "LogicLib.nsh"
!include "x64.nsh"
!include "WinMessages.nsh"


Name "THE cREXX Edition ${THE_VERSION}"
OutFile "${THE_OUTPUT}"
InstallDir "$LOCALAPPDATA\Programs\THE-cREXX"
InstallDirRegKey HKCU "Software\THE-cREXX" "InstallDir"
VIProductVersion "${THE_FILE_VERSION}"
VIAddVersionKey "ProductName" "THE cREXX Edition"
VIAddVersionKey "FileDescription" "THE cREXX Edition Windows x64 Installer"
VIAddVersionKey "FileVersion" "${THE_VERSION}"
VIAddVersionKey "LegalCopyright" "THE cREXX Edition contributors"
!define MUI_ABORTWARNING
!insertmacro MUI_PAGE_DIRECTORY
!insertmacro MUI_PAGE_INSTFILES
!insertmacro MUI_UNPAGE_CONFIRM
!insertmacro MUI_UNPAGE_INSTFILES
!insertmacro MUI_LANGUAGE "English"

Function .onInit
  ${IfNot} ${RunningX64}
    MessageBox MB_ICONSTOP "THE cREXX Edition requires 64-bit Windows."
    Abort
  ${EndIf}
  SetRegView 64
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

Section "THE cREXX Edition"
  SetOutPath "$INSTDIR"
  File /r "${THE_PAYLOAD}\*"
  WriteUninstaller "$INSTDIR\Uninstall.exe"
  WriteRegStr HKCU "Software\THE-cREXX" "InstallDir" "$INSTDIR"
  WriteRegStr HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\THE-cREXX" "DisplayName" "THE cREXX Edition ${THE_VERSION}"
  WriteRegStr HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\THE-cREXX" "DisplayVersion" "${THE_VERSION}"
  WriteRegStr HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\THE-cREXX" "UninstallString" '"$INSTDIR\Uninstall.exe"'
  WriteRegStr HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\THE-cREXX" "QuietUninstallString" '"$INSTDIR\Uninstall.exe" /S'
  CreateDirectory "$SMPROGRAMS\THE cREXX Edition"
  CreateShortCut "$SMPROGRAMS\THE cREXX Edition\THE.lnk" "$INSTDIR\bin\the.cmd"
  !insertmacro UpdatePath add
SectionEnd

Section "Uninstall"
  !insertmacro UpdatePath remove
  Delete "$SMPROGRAMS\THE cREXX Edition\THE.lnk"
  RMDir "$SMPROGRAMS\THE cREXX Edition"
  !include "${THE_UNINSTALL_FILES}"
  Delete "$INSTDIR\Uninstall.exe"
  RMDir "$INSTDIR"
  DeleteRegKey HKCU "Software\THE-cREXX"
  DeleteRegKey HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\THE-cREXX"
SectionEnd
