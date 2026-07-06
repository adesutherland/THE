param(
    [string]$Prefix = "",
    [switch]$PersistUser,
    [switch]$NoCurrentSession,
    [switch]$CheckOnly
)

Set-StrictMode -Version 2.0
$ErrorActionPreference = "Stop"

function Get-FullPath {
    param([Parameter(Mandatory = $true)][string]$Path)

    $expanded = [Environment]::ExpandEnvironmentVariables($Path)
    return [System.IO.Path]::GetFullPath($expanded)
}

function Convert-ToRuntimePath {
    param([Parameter(Mandatory = $true)][string]$Path)

    return (Get-FullPath $Path).Replace("\", "/")
}

function Get-DefaultPrefix {
    if ($PSScriptRoot) {
        $scriptDir = Get-FullPath $PSScriptRoot
        if ((Split-Path -Leaf $scriptDir) -ieq "bin") {
            $candidate = Split-Path -Parent $scriptDir
            if (Test-Path -LiteralPath (Join-Path $candidate "share\the") -PathType Container) {
                return $candidate
            }
        }
        if ((Split-Path -Leaf $scriptDir) -ieq "the") {
            $shareDir = Split-Path -Parent $scriptDir
            if ((Split-Path -Leaf $shareDir) -ieq "share") {
                return (Split-Path -Parent $shareDir)
            }
        }
    }

    if ($env:USERPROFILE) {
        return (Join-Path $env:USERPROFILE ".local")
    }
    if ($env:HOME) {
        return (Join-Path $env:HOME ".local")
    }

    throw "Cannot infer install prefix; pass -Prefix explicitly."
}

function Normalize-PathEntry {
    param([AllowNull()][string]$Path)

    if ([string]::IsNullOrWhiteSpace($Path)) {
        return ""
    }
    $value = $Path.Trim().Trim('"')
    try {
        $value = Get-FullPath $value
    } catch {
        $value = [Environment]::ExpandEnvironmentVariables($value)
    }
    return $value.TrimEnd("\", "/").ToUpperInvariant()
}

function Add-PathEntry {
    param(
        [AllowNull()][string]$PathValue,
        [Parameter(Mandatory = $true)][string]$Entry
    )

    $entryNorm = Normalize-PathEntry $Entry
    $parts = @()
    if (-not [string]::IsNullOrEmpty($PathValue)) {
        $parts = $PathValue -split ";" | Where-Object {
            -not [string]::IsNullOrWhiteSpace($_) -and
            (Normalize-PathEntry $_) -ne $entryNorm
        }
    }
    return (@($Entry) + $parts) -join ";"
}

function Set-EnvironmentValue {
    param(
        [Parameter(Mandatory = $true)][string]$Name,
        [Parameter(Mandatory = $true)][string]$Value
    )

    if (-not $NoCurrentSession -and -not $CheckOnly) {
        Set-Item -Path ("Env:" + $Name) -Value $Value
    }
    if ($PersistUser -and -not $CheckOnly) {
        [Environment]::SetEnvironmentVariable(
            $Name,
            $Value,
            [EnvironmentVariableTarget]::User)
    }
}

function Send-EnvironmentChangedMessage {
    if (-not ("TheEnv.NativeMethods" -as [type])) {
        Add-Type -TypeDefinition @"
using System;
using System.Runtime.InteropServices;

namespace TheEnv
{
    public static class NativeMethods
    {
        [DllImport("user32.dll", SetLastError = true, CharSet = CharSet.Unicode)]
        public static extern IntPtr SendMessageTimeout(
            IntPtr hWnd,
            UInt32 Msg,
            UIntPtr wParam,
            string lParam,
            UInt32 fuFlags,
            UInt32 uTimeout,
            out UIntPtr lpdwResult);
    }
}
"@
    }

    $result = [UIntPtr]::Zero
    [TheEnv.NativeMethods]::SendMessageTimeout(
        [IntPtr]0xffff,
        0x001A,
        [UIntPtr]::Zero,
        "Environment",
        0x0002,
        5000,
        [ref]$result) | Out-Null
}

if ([string]::IsNullOrWhiteSpace($Prefix)) {
    $Prefix = Get-DefaultPrefix
}

$prefixDir = Get-FullPath $Prefix
$binDir = Get-FullPath (Join-Path $prefixDir "bin")
$theHomeDir = Get-FullPath (Join-Path $prefixDir "share\the")
$theDriverDir = Get-FullPath (Join-Path $prefixDir "lib\the\drivers")

$runtimePrefix = Convert-ToRuntimePath $prefixDir
$runtimeBin = Convert-ToRuntimePath $binDir
$runtimeTheHome = Convert-ToRuntimePath $theHomeDir
$runtimeDriverDir = Convert-ToRuntimePath $theDriverDir

$environment = [ordered]@{
    "CREXX_HOME" = $runtimePrefix
    "REXX_HOME" = $runtimePrefix
    "CREXX" = "$runtimeBin/crexx.exe"
    "THE_BIN" = "$runtimeBin/the.exe"
    "THE_HOME_DIR" = $runtimeTheHome
    "THE_DRIVER_PATH" = $runtimeDriverDir
    "THE_HELP_FILE" = "$runtimeTheHome/THE_Help.txt"
    "THE_MACRO_PATH" = "$runtimeTheHome;."
    "THE_CREXX_RXC" = "$runtimeBin/rxc.exe"
    "THE_CREXX_RXAS" = "$runtimeBin/rxas.exe"
    "THE_CREXX_IMPORT_DIR" = $runtimeBin
    "THE_CREXX_LOCATION" = $runtimeBin
    "THE_CREXX_LIBRARY_RXBIN" = "$runtimeBin/library.rxbin"
}

$checks = @(
    @{ Name = "install prefix"; Path = $prefixDir; Required = $true; Type = "Container" },
    @{ Name = "bin directory"; Path = $binDir; Required = $true; Type = "Container" },
    @{ Name = "THE executable"; Path = (Join-Path $binDir "the.exe"); Required = $true; Type = "Leaf" },
    @{ Name = "THE help"; Path = (Join-Path $theHomeDir "THE_Help.txt"); Required = $true; Type = "Leaf" },
    @{ Name = "THE profile"; Path = (Join-Path $theHomeDir "profile.the"); Required = $true; Type = "Leaf" },
    @{ Name = "THE curses driver"; Path = (Join-Path $theDriverDir "the_driver_curses.dll"); Required = $true; Type = "Leaf" },
    @{ Name = "THE LLM driver"; Path = (Join-Path $theDriverDir "the_driver_llm.dll"); Required = $true; Type = "Leaf" },
    @{ Name = "CREXX driver"; Path = (Join-Path $binDir "crexx.exe"); Required = $true; Type = "Leaf" },
    @{ Name = "CREXX compiler"; Path = (Join-Path $binDir "rxc.exe"); Required = $true; Type = "Leaf" },
    @{ Name = "CREXX assembler"; Path = (Join-Path $binDir "rxas.exe"); Required = $true; Type = "Leaf" },
    @{ Name = "CREXX runtime library"; Path = (Join-Path $binDir "library.rxbin"); Required = $true; Type = "Leaf" },
    @{ Name = "CREXX SAA DLL"; Path = (Join-Path $binDir "libcrexxsaa.dll"); Required = $true; Type = "Leaf" },
    @{ Name = "DSLSH C parser"; Path = (Join-Path $binDir "dslsh-c.exe"); Required = $false; Type = "Leaf" },
    @{ Name = "DSLSH Markdown parser"; Path = (Join-Path $binDir "mdp.exe"); Required = $false; Type = "Leaf" },
    @{ Name = "DSLSH Python parser"; Path = (Join-Path $binDir "pyp.exe"); Required = $false; Type = "Leaf" },
    @{ Name = "DSLSH JavaScript parser"; Path = (Join-Path $binDir "jsp.exe"); Required = $false; Type = "Leaf" }
)

$missingRequired = @()
$missingOptional = @()
foreach ($check in $checks) {
    $exists = Test-Path -LiteralPath $check.Path -PathType $check.Type
    if (-not $exists) {
        if ($check.Required) {
            $missingRequired += $check
        } else {
            $missingOptional += $check
        }
    }
}

if ($missingRequired.Count -gt 0) {
    foreach ($check in $missingRequired) {
        Write-Warning ("Missing {0}: {1}" -f $check.Name, $check.Path)
    }
    if ($CheckOnly) {
        throw "Required installed files are missing under $prefixDir."
    }
}

if (-not $CheckOnly) {
    $newProcessPath = Add-PathEntry $env:PATH $binDir
    if (-not $NoCurrentSession) {
        Set-Item -Path Env:Path -Value $newProcessPath
    }

    if ($PersistUser) {
        $userPath = [Environment]::GetEnvironmentVariable("Path", [EnvironmentVariableTarget]::User)
        $newUserPath = Add-PathEntry $userPath $binDir
        [Environment]::SetEnvironmentVariable(
            "Path",
            $newUserPath,
            [EnvironmentVariableTarget]::User)
    }

    foreach ($entry in $environment.GetEnumerator()) {
        Set-EnvironmentValue -Name $entry.Key -Value $entry.Value
    }

    if ($PersistUser) {
        Send-EnvironmentChangedMessage
    }
}

Write-Host "THE install prefix: $prefixDir"
Write-Host "Added to Path: $binDir"
Write-Host "THE_HOME_DIR: $($environment["THE_HOME_DIR"])"
Write-Host "THE_DRIVER_PATH: $($environment["THE_DRIVER_PATH"])"
Write-Host "CREXX_HOME: $($environment["CREXX_HOME"])"

if ($missingOptional.Count -gt 0) {
    $names = ($missingOptional | ForEach-Object { $_.Name }) -join ", "
    Write-Host "Optional installed tools not found: $names"
}

if ($CheckOnly) {
    Write-Host "Check only; no environment changes were made."
} elseif ($PersistUser) {
    Write-Host "Updated this PowerShell session and the current user's persistent environment."
    Write-Host "Open a new terminal, or restart CLion, for the persisted environment to be inherited."
} elseif ($NoCurrentSession) {
    Write-Host "No current-session changes requested. Re-run with -PersistUser to persist settings."
} else {
    Write-Host "Updated this PowerShell session. Re-run with -PersistUser to make it permanent."
}
