<#
.SYNOPSIS
Packages a Windows build of Darkride as a zip and an installer.

.DESCRIPTION
Collects the interface, domain-server and assignment-client from a build folder into one folder, as Tivoli's
releases did, adds the Visual C++ runtime beside them so nothing else needs installing, and makes:
  Darkride-<version>-windows-x64.zip        unzip and run interface.exe
  Darkride-<version>-windows-x64-setup.exe  the installer (tools/installer/darkride.iss, Inno Setup 6)

Build the interface, domain-server and assignment-client targets first (BUILD_WINDOWS.md).

.EXAMPLE
pwsh tools/installer/package-windows.ps1 -BuildDir build -Version 2026.10.1 -OutDir dist
#>
param(
    [Parameter(Mandatory)] [string] $BuildDir,
    [Parameter(Mandatory)] [string] $Version,
    [string] $Config = "Release",
    [string] $OutDir = "dist",
    [string] $Iscc = ""
)
$ErrorActionPreference = "Stop"

$here = $PSScriptRoot
$BuildDir = (Resolve-Path $BuildDir).Path
New-Item -ItemType Directory -Force $OutDir | Out-Null
$OutDir = (Resolve-Path $OutDir).Path
$name = "Darkride-$Version-windows-x64"
$stage = Join-Path $OutDir "stage\Darkride"

# 1. One folder with the interface and both servers. The servers' Qt and other DLLs are the same ones the interface has.
if (Test-Path $stage) { Remove-Item -Recurse -Force $stage }
New-Item -ItemType Directory -Force $stage | Out-Null
foreach ($target in "interface", "domain-server", "assignment-client") {
    $from = Join-Path $BuildDir "$target\$Config"
    if (-not (Test-Path (Join-Path $from "$target.exe"))) { throw "$from\$target.exe not found: build the $target target in $Config first" }
    Write-Host "Copying $from"
    Copy-Item -Recurse -Force "$from\*" $stage
}

# 2. Leave out what only the build needs: debug symbols, import libraries, and linker leftovers
Get-ChildItem $stage -Recurse -File -Include *.pdb, *.exp, *.lib, *.ilk | Remove-Item -Force

# 3. The Visual C++ runtime, from the newest redistributable on this machine (newer runtimes run older builds)
$vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
$vsPaths = & $vswhere -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
$crt = $vsPaths | ForEach-Object { Get-ChildItem "$_\VC\Redist\MSVC\*\x64\Microsoft.VC*.CRT" -Directory -ErrorAction SilentlyContinue } |
    Sort-Object { [version]($_.Parent.Parent.Name -replace '[^0-9.]', '') } | Select-Object -Last 1
if (-not $crt) { throw "No Visual C++ redistributable found under $($vsPaths -join ', ')" }
Write-Host "Adding the Visual C++ runtime from $($crt.FullName)"
Copy-Item -Force "$($crt.FullName)\*.dll" $stage

# 4. The zip
$zip = Join-Path $OutDir "$name.zip"
if (Test-Path $zip) { Remove-Item -Force $zip }
Write-Host "Writing $zip"
# Windows' own tar (bsdtar) writes zips, and is much faster than Compress-Archive with a folder this size
& "$env:SystemRoot\System32\tar.exe" -a -c -f $zip -C (Split-Path $stage) Darkride
if ($LASTEXITCODE) { throw "tar failed" }

# 5. The installer
if (-not $Iscc) {
    $Iscc = @("${env:ProgramFiles(x86)}\Inno Setup 6\ISCC.exe", "$env:LOCALAPPDATA\Programs\Inno Setup 6\ISCC.exe") |
        Where-Object { Test-Path $_ } | Select-Object -First 1
}
if (-not $Iscc) { throw "Inno Setup 6 not found: install it (winget install JRSoftware.InnoSetup) or pass -Iscc" }
& $Iscc /Q "/DAppVersion=$Version" "/DSourceDir=$stage" "/DOutputDir=$OutDir" "/DOutputBaseFilename=$name-setup" (Join-Path $here "darkride.iss")
if ($LASTEXITCODE) { throw "Inno Setup failed" }

Remove-Item -Recurse -Force (Split-Path $stage)
Get-ChildItem $OutDir -File | Where-Object Name -like "$name*" | ForEach-Object { "{0}  {1:N0} MB" -f $_.Name, ($_.Length / 1MB) }
