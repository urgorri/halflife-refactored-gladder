<#
.SYNOPSIS
    Copies compiled Half-Life: Gladder binaries and assets to a target mod directory.

.DESCRIPTION
    Takes the compiled client and server DLLs from the build output directory
    and copies them to <ModDir>/cl_dlls/client.dll and <ModDir>/dlls/hl.dll.
    Also copies gladder.fgd to the mod directory.

.PARAMETER ModDir
    Target mod directory path (defaults to ../game/mod or E:/Dev/urgorri/game/mod).

.PARAMETER Configuration
    Build configuration to copy from (Release or Debug, defaults to Release).

.EXAMPLE
    .\copy_to_mod.ps1
    .\copy_to_mod.ps1 -ModDir "C:\Program Files (x86)\Steam\steamapps\common\Half-Life\gladder"
#>
[CmdletBinding()]
param(
    [string]$ModDir = "",
    [string]$Configuration = "Release"
)

$ErrorActionPreference = "Stop"

# Determine repository root
$RepoRoot = Split-Path -Parent $MyInvocation.MyCommand.Path

# Default target directory detection
if (-not $ModDir) {
    $PotentialPaths = @(
        (Join-Path $RepoRoot "..\game\mod"),
        "E:\Dev\urgorri\game\mod"
    )
    foreach ($Path in $PotentialPaths) {
        if (Test-Path $Path) {
            $ModDir = (Resolve-Path $Path).Path
            break
        }
    }
    if (-not $ModDir) {
        $ModDir = Join-Path $RepoRoot "..\game\mod"
    }
}

$BuildBinDir = Join-Path $RepoRoot "build\bin\$Configuration"

$ServerDll = Join-Path $BuildBinDir "hl.dll"
$ClientDll = Join-Path $BuildBinDir "client.dll"
$FgdFile   = Join-Path $RepoRoot "gladder.fgd"

Write-Host "=== Deploying Half-Life: Gladder to Mod Directory ===" -ForegroundColor Cyan
Write-Host "Source directory: $BuildBinDir"
Write-Host "Target mod directory: $ModDir"

if (-not (Test-Path $ServerDll)) {
    Write-Error "Server DLL not found at: $ServerDll. Please build the project first."
}

if (-not (Test-Path $ClientDll)) {
    Write-Error "Client DLL not found at: $ClientDll. Please build the project first."
}

# Ensure destination directories exist
$TargetDllsDir   = Join-Path $ModDir "dlls"
$TargetClDllsDir = Join-Path $ModDir "cl_dlls"

if (-not (Test-Path $TargetDllsDir)) {
    New-Item -ItemType Directory -Path $TargetDllsDir -Force | Out-Null
}

if (-not (Test-Path $TargetClDllsDir)) {
    New-Item -ItemType Directory -Path $TargetClDllsDir -Force | Out-Null
}

# Copy server DLL
Write-Host "Copying hl.dll -> $TargetDllsDir..." -ForegroundColor Yellow
Copy-Item -Path $ServerDll -Destination (Join-Path $TargetDllsDir "hl.dll") -Force

# Copy client DLL
Write-Host "Copying client.dll -> $TargetClDllsDir..." -ForegroundColor Yellow
Copy-Item -Path $ClientDll -Destination (Join-Path $TargetClDllsDir "client.dll") -Force

# Copy PDB files if present
$ServerPdb = Join-Path $BuildBinDir "hl.pdb"
if (Test-Path $ServerPdb) {
    Copy-Item -Path $ServerPdb -Destination (Join-Path $TargetDllsDir "hl.pdb") -Force
}

$ClientPdb = Join-Path $BuildBinDir "client.pdb"
if (Test-Path $ClientPdb) {
    Copy-Item -Path $ClientPdb -Destination (Join-Path $TargetClDllsDir "client.pdb") -Force
}

# Copy gladder.fgd
if (Test-Path $FgdFile) {
    Write-Host "Copying gladder.fgd -> $ModDir..." -ForegroundColor Yellow
    Copy-Item -Path $FgdFile -Destination (Join-Path $ModDir "gladder.fgd") -Force
}

Write-Host "=== Deployment completed successfully! ===" -ForegroundColor Green
