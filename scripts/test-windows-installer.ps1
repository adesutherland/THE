param(
    [Parameter(Mandatory=$true)][string]$Installer,
    [Parameter(Mandatory=$true)][string]$ReginaLibrary
)
$ErrorActionPreference = 'Stop'
$prefix = Join-Path $env:RUNNER_TEMP 'THE installed candidate'
$pathBefore = [Environment]::GetEnvironmentVariable('Path', 'User')
$python = (Get-Command python).Source
function Install-Candidate([string]$Runtime = 'BUNDLED') {
    $process = Start-Process -FilePath $Installer -ArgumentList "/S /CREXX=$Runtime /D=$prefix" -Wait -PassThru
    if ($process.ExitCode -ne 0) { throw "Installer failed: $($process.ExitCode)" }
    foreach ($launcher in @('the.cmd', 'crxthe.cmd')) {
        if (!(Test-Path -LiteralPath "$prefix\bin\$launcher")) { throw "Launcher missing after install: $launcher" }
    }
}
try {
    Install-Candidate
    if ((Get-Content "$prefix\share\the\runtime-preference" -Raw).Trim() -ne 'BUNDLED') { throw 'Bundled runtime was not the installer default' }
    $startMenu = Join-Path ([Environment]::GetFolderPath('Programs')) 'CRXTHE'
    $shell = New-Object -ComObject WScript.Shell
    $editorShortcut = $shell.CreateShortcut((Join-Path $startMenu 'CRXTHE.lnk'))
    if ($editorShortcut.TargetPath -ine "$prefix\bin\crxthe.cmd") { throw 'Start menu shortcut has the wrong launcher' }
    if ($editorShortcut.WorkingDirectory -ine [Environment]::GetFolderPath('MyDocuments')) { throw 'Start menu shortcut does not start in Documents' }
    $guideShortcut = $shell.CreateShortcut((Join-Path $startMenu 'Getting Started.lnk'))
    if ($guideShortcut.TargetPath -ine "$prefix\share\doc\the\Windows-Readme.txt") { throw 'Getting Started shortcut has the wrong target' }
    $testArguments = @("$PSScriptRoot\..\tests\test_installed_package.py", $prefix, '--regina', $ReginaLibrary)
    & $python @testArguments
    if ($LASTEXITCODE -ne 0) { throw 'Installed runtime checks failed' }
    Set-Content -LiteralPath "$prefix\keep-user-file.txt" -Value 'retain on uninstall'
    Install-Candidate 'INSTALLED'
    if ((Get-Content "$prefix\share\the\runtime-preference" -Raw).Trim() -ne 'INSTALLED') { throw 'Installed runtime choice was not saved' }
    & $python "$PSScriptRoot\..\tests\test_runtime_selection.py" --prefix $prefix
    if ($LASTEXITCODE -ne 0) { throw 'Installed runtime selection/cache checks failed' }
    Install-Candidate 'BUNDLED'
    $process = Start-Process -FilePath "$prefix\Uninstall.exe" -ArgumentList "/S _?=$prefix" -Wait -PassThru
    if ($process.ExitCode -ne 0) { throw "Uninstall failed: $($process.ExitCode)" }
    if (Test-Path -LiteralPath "$prefix\libexec\the\the.exe") { throw 'Packaged executable survived uninstall' }
    if (Test-Path -LiteralPath (Join-Path $startMenu 'CRXTHE.lnk')) { throw 'Editor shortcut survived uninstall' }
    if (Test-Path -LiteralPath (Join-Path $startMenu 'Getting Started.lnk')) { throw 'Getting Started shortcut survived uninstall' }
    if (!(Test-Path -LiteralPath "$prefix\keep-user-file.txt")) { throw 'Uninstall removed a user file' }
    if ([Environment]::GetEnvironmentVariable('Path', 'User') -ne $pathBefore) { throw 'User PATH was not restored' }
    if (Test-Path 'HKCU:\Software\Microsoft\Windows\CurrentVersion\Uninstall\THE-cREXX') { throw 'Uninstall registry entry survived' }
    Write-Output 'Windows candidate install, engine selection, reinstall and uninstall passed'
} finally {
    # Only this test's own disposable prefix is removed.
    if (Test-Path -LiteralPath $prefix) { Remove-Item -LiteralPath $prefix -Recurse -Force }
}
