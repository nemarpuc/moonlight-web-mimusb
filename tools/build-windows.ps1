# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (C) 2026 the moonlight-web-mimusb contributors
<#
Build the add-on for Windows from a tree tools/prepare.py made.

  powershell -File tools\build-windows.ps1 -Work work [-QtDir C:\Qt\6.11.3\msvc2022_64]

Output: <Work>\dist\  (MoonlightWeb.exe and the Qt DLLs it needs)
#>
param(
    [Parameter(Mandatory = $true)][string]$Work,
    [string]$QtDir = $env:QTDIR,
    [int]$Jobs = 4
)
$ErrorActionPreference = 'Stop'

$Work = (Resolve-Path $Work).Path
$info = Get-Content (Join-Path $Work 'BUILD_INFO.json') | ConvertFrom-Json
if (-not $QtDir) { throw 'Qt not found: pass -QtDir or set QTDIR (a Qt 6 MSVC 2022 64-bit kit)' }

# MSVC environment, unless the caller is already in one.
if (-not $env:VSINSTALLDIR) {
    $vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
    $vs = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
    if (-not $vs) { throw 'Visual Studio with the C++ tools was not found' }
    $bat = Join-Path $vs 'VC\Auxiliary\Build\vcvars64.bat'
    cmd /c "`"$bat`" >nul && set" | ForEach-Object {
        if ($_ -match '^([^=]+)=(.*)$') { Set-Item -Path "env:$($Matches[1])" -Value $Matches[2] }
    }
}

$build = Join-Path $Work 'build'
cmake -S (Join-Path $info.source 'backend') -B $build -G Ninja -DCMAKE_BUILD_TYPE=Release `
    "-DCMAKE_PREFIX_PATH=$QtDir" "-DMW_VERSION=$($info.version)" `
    -DMW_MIMUSB=ON "-DMW_MIMUSB_DIR=$($info.libmimusb)"
if ($LASTEXITCODE -ne 0) { throw 'configure failed' }
cmake --build $build --target mw-server -j $Jobs
if ($LASTEXITCODE -ne 0) { throw 'build failed' }

# The exe and the DLLs it needs. The Qt DLLs ship with it: Qt runs a newer DLL
# under an older build, not the other way round.
$dist = Join-Path $Work 'dist'
if (Test-Path $dist) { Remove-Item $dist -Recurse -Force }
New-Item -ItemType Directory $dist | Out-Null
Copy-Item (Join-Path $build 'MoonlightWeb.exe') $dist
foreach ($dll in 'libcrypto-3-x64.dll', 'libssl-3-x64.dll') {
    $p = Join-Path $build $dll
    if (Test-Path $p) { Copy-Item $p $dist }
}
& (Join-Path $QtDir 'bin\windeployqt.exe') --release --no-translations --no-system-d3d-compiler `
    --no-opengl-sw (Join-Path $dist 'MoonlightWeb.exe')
if ($LASTEXITCODE -ne 0) { throw 'windeployqt failed' }

# windeployqt ships only the schannel/certonly TLS backends; upstream also
# force-copies the OpenSSL one (its release workflow explains why).
$tls = Join-Path $QtDir 'plugins\tls\qopensslbackend.dll'
if (Test-Path $tls) {
    New-Item -ItemType Directory -Force (Join-Path $dist 'tls') | Out-Null
    Copy-Item $tls (Join-Path $dist 'tls') -Force
}
"built $($info.upstream_tag) -> $dist"
