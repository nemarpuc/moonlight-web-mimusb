# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (C) 2026 the moonlight-web-mimhid contributors
<#
Put the official MoonlightWeb files back.

  powershell -ExecutionPolicy Bypass -File rollback.ps1 [-InstallDir "C:\Program Files\MoonlightWeb"]

Every <name>.official that install.ps1 kept is copied over <name>; files that
install.ps1 added are removed. The .official copies stay, so installing again
later still has a genuine original to return to. mimhid.json is not touched.
#>
param(
    [string]$InstallDir = "$env:ProgramFiles\MoonlightWeb"
)
$ErrorActionPreference = 'Stop'

$admin = ([Security.Principal.WindowsPrincipal][Security.Principal.WindowsIdentity]::GetCurrent()
    ).IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)
if (-not $admin) {
    Start-Process powershell -Verb RunAs -Wait -ArgumentList @('-NoProfile', '-ExecutionPolicy', 'Bypass',
        '-File', "`"$PSCommandPath`"", '-InstallDir', "`"$InstallDir`"")
    return
}

Write-Host 'Stopping MoonlightWeb...'
Get-Process MoonlightWeb -ErrorAction SilentlyContinue | Stop-Process -Force
Start-Sleep -Seconds 2

$restored = 0
Get-ChildItem $InstallDir -Recurse -Filter '*.official' -File | ForEach-Object {
    Copy-Item $_.FullName $_.FullName.Substring(0, $_.FullName.Length - '.official'.Length) -Force
    $restored++
}
$manifestPath = Join-Path $InstallDir 'mimhid-install.json'
if (Test-Path $manifestPath) {
    foreach ($rel in @((Get-Content $manifestPath | ConvertFrom-Json).added)) {
        $p = Join-Path $InstallDir $rel
        if (Test-Path $p) { Remove-Item $p -Force }
    }
    # Folders the install created, once empty.
    Get-ChildItem $InstallDir -Recurse -Directory | Sort-Object { $_.FullName.Length } -Descending |
        Where-Object { -not (Get-ChildItem $_.FullName -Force) -and $_.Name -ne 'mimhid-tools' } |
        Remove-Item -Force
    Remove-Item $manifestPath -Force
}
# Not started from here: this script runs elevated, and a program started by it would too.
Write-Host "Restored $restored file(s). Start MoonlightWeb as usual (or sign out and in)."
