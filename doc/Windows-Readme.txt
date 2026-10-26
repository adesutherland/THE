CRXTHE 0.9.0 - Windows installation and first use
Based on Mark Hessling's THE 4.0; this fork is maintained separately.

These instructions describe the CRXTHE candidate supplied for your test.
The new CRXTHE 0.9.0 installer has not yet been published. Older THE 4.0
prereleases do not contain the current profile and navigation changes.

INSTALL

1. Use the Windows x64 installer (the file ending in -setup.exe).
   Use the installer supplied with the candidate you are testing; published
   pre-release installers must be signed.
2. Keep the suggested installation folder and "Use bundled cREXX (default)".
   You do not need to install Rexx, Python, a compiler or build tools.
   If you prefer an existing compatible cREXX, select the other runtime option.
   CRXTHE automatically uses its bundle if the selected installation is absent
   or incompatible. It does not replace your standalone cREXX installation.
3. Click Finish. "Open CRXTHE" starts the editor. "Read Getting Started"
   opens these instructions. Both are also available under Start > CRXTHE.

The default folder is:
  %LOCALAPPDATA%\Programs\THE-cREXX
That legacy folder name is intentional; the product is CRXTHE.
Installation is for your Windows account and normally needs no administrator.

OPEN A FILE

The Start menu shortcut starts in your Documents folder. With no filename,
THE shows a directory listing. Press F6, type this command, then press Enter:
  edit notes.txt
An existing file opens; a missing file is created when you save it.

Or open a NEW PowerShell / Windows Terminal window after installing:
  Set-Location ([Environment]::GetFolderPath('MyDocuments'))
  crxthe notes.txt

For a path containing spaces, quote the path in PowerShell:
  crxthe 'C:\My files\notes.txt'

FIRST KEYS TO TRY

  F6            Toggle between the command line and file editing.
  Esc           Go to the command line. In the browser, Esc again returns.
  Ctrl-S        Save and keep editing.
  F3            Save and close the current file.
  Ctrl-Q        Close the current file, protecting unsaved changes.
  F1            Help.                 F2  Next open file.
  F4            Prefix field.         F5  Insert/overwrite mode.
  F10           All current key definitions.
  Tab           Insert spaces up to the next eight-column tab stop.

From the first text column, Left enters that record's prefix field.
From the first prefix column, Left goes to the previous visible record's end.
With the prefix off, Left at the start of a record goes to the previous record.
Left at the first record's first prefix column stays there. Shift-Left remains
text selection. A laptop may require Fn together with a function key.

The bottom guide shows the main keys. A retained tab byte displays as an arrow;
saving preserves it. In the browser use its Open/Save controls; the same F6
toggle works when the browser delivers F6. Esc is the reliable command shortcut.

If a directory listing remains after closing your file, Ctrl-Q closes it too.

IF THE WRONG VERSION STARTS

Use the CRXTHE shortcut or the crxthe command. An old command named the may
still select an older installation. In a new PowerShell window check:
  Get-Command crxthe -All
  where.exe crxthe

For the default install folder, this always selects the installed launcher:
  & "$env:LOCALAPPDATA\Programs\THE-cREXX\bin\crxthe.cmd" --version
It should identify CRXTHE 0.9.0. For a custom folder, adjust that path.

Open a new terminal after installation. Already-running terminals and IDEs
can retain their old PATH. Do not run the-env.ps1 or set developer paths for
an installed package; its launcher configures its own resources and runtime.

If you previously used a development build, old THE_HOME_DIR, THE_DRIVER_PATH,
THE_PROFILE_FILE, THE_HELP_FILE, THE_MACRO_PATH or THE_CREXX_* environment
variables can override an installed package. Inspect them in PowerShell:
  Get-ChildItem Env:THE_*
Keep intentional custom profiles, runtime/library paths and cache overrides.
Core and UI drivers must come from the same build. An incompatible-driver
message usually means an old THE_DRIVER_PATH or an incomplete copied folder.
For a temporary test using stock cREXX defaults in THIS PowerShell only:
  Get-ChildItem Env:THE_* | Remove-Item
  & "$env:LOCALAPPDATA\Programs\THE-cREXX\bin\crxthe.cmd" notes.txt
Closing that shell restores your usual inherited environment. This command
has not changed saved user/system environment variables or removed files.

OPTIONAL: REGINA

You can use CRXTHE without Regina. For Classic Rexx macros, install a Windows
x64 Regina separately from its official project:
  https://regina-rexx.sourceforge.io/
Then select it for this PowerShell session (replace the example DLL path):
  $env:THE_REXX_INTERPRETER = 'REGINA'
  $env:THE_REGINA_LIBRARY = 'C:\path\to\Regina\regina.dll'
  & "$env:LOCALAPPDATA\Programs\THE-cREXX\bin\crxthe.cmd" notes.txt

CRXTHE selects its Regina default profile. To return to bundled cREXX in this
shell, remove the engine choice and choose the bundled runtime explicitly:
  Remove-Item Env:THE_REXX_INTERPRETER -ErrorAction SilentlyContinue
  $env:THE_CREXX_RUNTIME = 'BUNDLED'

ADVANCED / PORTABLE / REMOVAL

The portable ZIP must be fully extracted. Run its bin\crxthe.cmd; it has no
installer, Start menu entry or automatic PATH update. Keep the whole extracted
folder together; an isolated libexec\the\the.exe is not the public launcher.

For runtime/path diagnostics using the default install folder:
  $crxtheInstall = "$env:LOCALAPPDATA\Programs\THE-cREXX"
  & "$crxtheInstall\libexec\the\the-launcher.exe" --info $crxtheInstall
For the exact candidate label and source revision:
  Get-Content "$crxtheInstall\manifest.json" -Raw | ConvertFrom-Json | Select-Object version,source_commit
The output identifies the selected runtime and cache. The default source cache
is %LOCALAPPDATA%\THE-cREXX\Cache\crexxsaa. Explicit cache overrides are retained.

Remove CRXTHE through Windows Settings > Apps > Installed apps, or use its
Uninstall.exe. The uninstaller removes its own payload and PATH entry, leaving
files you added and separate cREXX/Regina installations. A legacy installation
may still contain old macro files until upgrade cleanup is qualified.

Report CRXTHE issues to https://github.com/adesutherland/THE/issues and include
the version, installation path and steps to reproduce. F8/F9 development
compile/run helpers remain experimental and are not part of the stock profile.
