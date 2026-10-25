param(
    [Parameter(Mandatory=$true)][string]$Installer,
    [Parameter(Mandatory=$true)][string]$ReginaLibrary,
    [Parameter(Mandatory=$true)][string]$MacroDirectory
)
$ErrorActionPreference = 'Stop'
$prefix = Join-Path $env:RUNNER_TEMP 'THE installed candidate'
$pathBefore = [Environment]::GetEnvironmentVariable('Path', 'User')
$python = (Get-Command python).Source
function Install-Candidate([string]$Runtime = 'BUNDLED') {
    $process = Start-Process -FilePath $Installer -ArgumentList "/S /CREXX=$Runtime /D=$prefix" -Wait -PassThru
    if ($process.ExitCode -ne 0) { throw "Installer failed: $($process.ExitCode)" }
    if (!(Test-Path -LiteralPath "$prefix\bin\the.cmd")) { throw 'Launcher missing after install' }
}
try {
    Install-Candidate
    if ((Get-Content "$prefix\share\the\runtime-preference" -Raw).Trim() -ne 'BUNDLED') { throw 'Bundled runtime was not the installer default' }
    & $python "$PSScriptRoot\..\tests\test_installed_package.py" $prefix --regina $ReginaLibrary --macros $MacroDirectory
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
    if (!(Test-Path -LiteralPath "$prefix\keep-user-file.txt")) { throw 'Uninstall removed a user file' }
    if ([Environment]::GetEnvironmentVariable('Path', 'User') -ne $pathBefore) { throw 'User PATH was not restored' }
    if (Test-Path 'HKCU:\Software\Microsoft\Windows\CurrentVersion\Uninstall\THE-cREXX') { throw 'Uninstall registry entry survived' }
    Write-Output 'Windows candidate install, engine selection, reinstall and uninstall passed'
} finally {
    # Only this test's own disposable prefix is removed.
    if (Test-Path -LiteralPath $prefix) { Remove-Item -LiteralPath $prefix -Recurse -Force }
}
