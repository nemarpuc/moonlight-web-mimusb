# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (C) 2026 the moonlight-web-mimhid contributors
<#
Put the add-on build in place of the installed MoonlightWeb.

  powershell -ExecutionPolicy Bypass -File install.ps1 -Source <dist folder or release zip>
                                                       [-InstallDir "C:\Program Files\MoonlightWeb"]

What it does, in order:
  1. asks for administrator rights if it has none;
  2. stops MoonlightWeb;
  3. keeps each file it is about to replace as <name>.official -- once, so the
     .official copy is always the genuine one, however often this runs;
  4. copies the new files over;
  5. copies rollback.ps1 into <InstallDir>\mimhid-tools and puts a shortcut to it on
     the desktop.

Upstream's own autostart and stream-worker tasks already point at InstallDir, so
they now start this build. Upstream's update button installs the official build
again: run this script again after it.
#>
param(
    [Parameter(Mandatory = $true)][string]$Source,
    [string]$InstallDir = "$env:ProgramFiles\MoonlightWeb"
)
$ErrorActionPreference = 'Stop'

$admin = ([Security.Principal.WindowsPrincipal][Security.Principal.WindowsIdentity]::GetCurrent()
    ).IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)
if (-not $admin) {
    $argList = @('-NoProfile', '-ExecutionPolicy', 'Bypass', '-File', "`"$PSCommandPath`"",
        '-Source', "`"$Source`"", '-InstallDir', "`"$InstallDir`"")
    Start-Process powershell -Verb RunAs -ArgumentList $argList -Wait
    return
}

if (-not (Test-Path (Join-Path $InstallDir 'MoonlightWeb.exe'))) {
    throw "MoonlightWeb is not installed in $InstallDir (install the official build first)"
}

# A release zip is unpacked beside the script; a folder is used as it is.
$staging = $null
if ((Get-Item $Source).PSIsContainer) {
    $dist = (Resolve-Path $Source).Path
} else {
    # Not under %TEMP%: that path can be spelled in 8.3 short form (USERNA~1), and
    # a short/long mismatch would cut the relative paths below at the wrong place.
    # The folder is locked to Administrators and SYSTEM BEFORE anything is extracted
    # into it: what is copied below into Program Files must be exactly the zip's
    # content, and ProgramData's default lets ordinary users add files to a new folder.
    $staging = Join-Path $env:ProgramData ("mimhid-" + [guid]::NewGuid().ToString('N'))
    $acl = New-Object Security.AccessControl.DirectorySecurity
    $acl.SetAccessRuleProtection($true, $false)
    foreach ($sid in @('S-1-5-32-544', 'S-1-5-18')) {   # Administrators, SYSTEM
        $acl.AddAccessRule((New-Object Security.AccessControl.FileSystemAccessRule(
            (New-Object Security.Principal.SecurityIdentifier $sid), 'FullControl',
            'ContainerInherit,ObjectInherit', 'None', 'Allow')))
    }
    # Created with the ACL in place, so there is no moment when the folder is open to others.
    # Windows PowerShell 5.1 (.NET Framework) has Directory.CreateDirectory(path, security);
    # PowerShell 7 (.NET) has the FileSystemAclExtensions form.
    try {
        [void][IO.Directory]::CreateDirectory($staging, $acl)
    } catch {
        [void][IO.FileSystemAclExtensions]::Create((New-Object IO.DirectoryInfo $staging), $acl)
    }
    # Checked anyway, in case the folder name already existed: it must be empty and
    # owned by Administrators or SYSTEM.
    if (Get-ChildItem $staging -Force) { throw "$staging was not empty after it was locked" }
    $owner = (Get-Acl $staging).GetOwner([Security.Principal.SecurityIdentifier]).Value
    if ($owner -notin @('S-1-5-32-544', 'S-1-5-18')) { throw "$staging is owned by $owner" }
    Expand-Archive -Path $Source -DestinationPath $staging
    $dist = $staging
}
if (-not (Test-Path (Join-Path $dist 'MoonlightWeb.exe'))) { throw "no MoonlightWeb.exe in $dist" }

Write-Host 'Stopping MoonlightWeb...'
Get-Process MoonlightWeb -ErrorAction SilentlyContinue | Stop-Process -Force
Start-Sleep -Seconds 2

$manifestPath = Join-Path $InstallDir 'mimhid-install.json'
$added = @()
$ours = $null   # relative path -> SHA-256 of the file this script installed last time
if (Test-Path $manifestPath) {
    $m = Get-Content $manifestPath | ConvertFrom-Json
    $added = @($m.added)
    $ours = $m.hashes
}
$hashes = [ordered]@{}

# A file in place is "ours" only if it is byte for byte what this script installed.
# Anything else is an official file (for instance after upstream's update button
# put a newer official build there), and it becomes the new *.official copy:
# rolling back must never bring back an older official version.
function Test-Ours($rel, $path) {
    if (-not $ours) { return $false }
    $prop = $ours.PSObject.Properties[$rel]
    return ($prop -and $prop.Value -eq (Get-FileHash $path -Algorithm SHA256).Hash)
}

$files = Get-ChildItem $dist -Recurse -File -Force
$folders = Get-ChildItem $dist -Recurse -Directory -Force
foreach ($d in $folders) {
    if ($d.Attributes -band [IO.FileAttributes]::ReparsePoint) { throw "refusing $($d.FullName): a link" }
}
# Nothing may sit outside the folder it was unpacked into (a zip entry with "..", a link).
foreach ($f in $files) {
    if (-not $f.FullName.StartsWith($dist.TrimEnd('\') + '\', [StringComparison]::OrdinalIgnoreCase) -or
        ($f.Attributes -band [IO.FileAttributes]::ReparsePoint)) {
        throw "refusing $($f.FullName): it is not a plain file inside $dist"
    }
}
foreach ($f in $files) {
    $rel = $f.FullName.Substring($dist.Length).TrimStart('\')
    $dest = Join-Path $InstallDir $rel
    New-Item -ItemType Directory -Force (Split-Path $dest) | Out-Null
    if (Test-Path $dest) {
        if (($added -notcontains $rel) -and -not (Test-Ours $rel $dest)) {
            Copy-Item $dest "$dest.official" -Force
        }
    } elseif ($added -notcontains $rel) {
        $added += $rel
    }
    Copy-Item $f.FullName $dest -Force
    $hashes[$rel] = (Get-FileHash $dest -Algorithm SHA256).Hash
}
# What was copied must be where the program is: the exe, byte for byte.
$srcExe = Join-Path $dist 'MoonlightWeb.exe'
$dstExe = Join-Path $InstallDir 'MoonlightWeb.exe'
if ((Get-Item $srcExe).Length -ne (Get-Item $dstExe).Length) { throw 'the installed MoonlightWeb.exe is not the new one' }
@{ added = $added; hashes = $hashes; installed = (Get-Date).ToString('s') } | ConvertTo-Json | Set-Content $manifestPath

# Emergency tools live next to the program, with shortcuts on the desktop.
$tools = Join-Path $InstallDir 'mimhid-tools'
New-Item -ItemType Directory -Force $tools | Out-Null
Copy-Item (Join-Path $PSScriptRoot 'rollback.ps1') $tools -Force
# An earlier release also installed a "start without physical HID" shortcut that did
# not work (the stream worker never saw its environment variable): remove it.
Remove-Item (Join-Path $tools 'start-without-mimhid.ps1') -Force -ErrorAction SilentlyContinue
$shell = New-Object -ComObject WScript.Shell
$desktop = [Environment]::GetFolderPath('CommonDesktopDirectory')
Remove-Item (Join-Path $desktop 'MoonlightWeb - start without physical HID.lnk') -Force -ErrorAction SilentlyContinue
foreach ($s in @(
        @{ Name = 'MoonlightWeb - back to official build'; Script = 'rollback.ps1' })) {
    $lnk = $shell.CreateShortcut((Join-Path $desktop ($s.Name + '.lnk')))
    $lnk.TargetPath = 'powershell.exe'
    $lnk.Arguments = "-NoProfile -ExecutionPolicy Bypass -File `"$(Join-Path $tools $s.Script)`""
    $lnk.IconLocation = (Join-Path $InstallDir 'MoonlightWeb.exe')
    $lnk.Save()
}

if ($staging) { Remove-Item $staging -Recurse -Force }
Write-Host "Installed. Start MoonlightWeb as usual (or sign out and in)."
Write-Host "Settings: mimhid.json beside settings.json. Emergency: the desktop shortcut."
