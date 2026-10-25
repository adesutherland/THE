param([Parameter(Mandatory=$true)][string]$Installer)
$ErrorActionPreference = 'Stop'
Import-Module Microsoft.PowerShell.Security
$signature = Get-AuthenticodeSignature -LiteralPath $Installer
if ($signature.Status -ne 'Valid' -or $null -eq $signature.TimeStamperCertificate) {
    throw "Expected a valid timestamped installer signature, got $($signature.Status)"
}
$prefix = Join-Path $env:RUNNER_TEMP 'THE signed installed candidate'
$pathBefore = [Environment]::GetEnvironmentVariable('Path', 'User')
$python = (Get-Command python).Source
function Install-Candidate {
    $process = Start-Process -FilePath $Installer -ArgumentList "/S /D=$prefix" -Wait -PassThru
    if ($process.ExitCode -ne 0) { throw "Installer failed: $($process.ExitCode)" }
    if (!(Test-Path -LiteralPath "$prefix\bin\the.cmd")) { throw 'Launcher missing after install' }
}
try {
    Install-Candidate
    & $python "$PSScriptRoot\..\tests\test_installed_package.py" $prefix
    if ($LASTEXITCODE -ne 0) { throw 'Installed runtime/hash checks failed' }
    Set-Content -LiteralPath "$prefix\keep-user-file.txt" -Value 'retain on uninstall'
    Install-Candidate
    $process = Start-Process -FilePath "$prefix\Uninstall.exe" -ArgumentList "/S _?=$prefix" -Wait -PassThru
    if ($process.ExitCode -ne 0) { throw "Uninstall failed: $($process.ExitCode)" }
    if (Test-Path -LiteralPath "$prefix\libexec\the\the.exe") { throw 'Packaged executable survived uninstall' }
    if (!(Test-Path -LiteralPath "$prefix\keep-user-file.txt")) { throw 'Uninstall removed a user file' }
    if ([Environment]::GetEnvironmentVariable('Path', 'User') -ne $pathBefore) { throw 'User PATH was not restored' }
    if (Test-Path 'HKCU:\Software\Microsoft\Windows\CurrentVersion\Uninstall\THE-cREXX') { throw 'Uninstall registry entry survived' }
    Write-Output 'Valid timestamped signature; payload hashes, CREXX/NONE, browser, reinstall, uninstall and PATH checks passed'
} finally {
    if (Test-Path -LiteralPath $prefix) { Remove-Item -LiteralPath $prefix -Recurse -Force }
}
