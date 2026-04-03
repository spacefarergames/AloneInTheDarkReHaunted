#Requires -Version 3.0

<#
.SYNOPSIS
    Rebuild the backgrounds_hd.hda archive from source PNG files

.DESCRIPTION
    This script rebuilds the backgrounds_hd.hda archive using the build_hda_archive tool.
    The tool must be built first from the Visual Studio solution.

.PARAMETER Configuration
    Build configuration to use (Debug, Release, MinSizeRel, RelWithDebInfo)
    Default: Release

.EXAMPLE
    .\rebuild_hda_archive.ps1 -Configuration Release
    
.EXAMPLE
    .\rebuild_hda_archive.ps1
#>

param(
    [ValidateSet("Debug", "Release", "MinSizeRel", "RelWithDebInfo")]
    [string]$Configuration = "Release"
)

$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$BuildDir = Join-Path $ScriptDir "build" "vs2026"
$SourceDir = Join-Path $ScriptDir "Shipping" "backgrounds_hd"
$OutputDir = Join-Path $ScriptDir "Shipping"
$ToolExe = Join-Path $BuildDir "tools" $Configuration "build_hda_archive.exe"
$OutputArchive = Join-Path $OutputDir "backgrounds_hda.hda"

Write-Host ""
Write-Host "====================================================================" -ForegroundColor Cyan
Write-Host "   HD Background Archive Rebuilder" -ForegroundColor Cyan
Write-Host "====================================================================" -ForegroundColor Cyan
Write-Host "Configuration: $Configuration"
Write-Host "Build directory: $BuildDir"
Write-Host "Source directory: $SourceDir"
Write-Host "Output archive: $OutputArchive"
Write-Host ""

# Check if source directory exists
if (-not (Test-Path $SourceDir -PathType Container))
{
    Write-Host "Error: Source directory not found: $SourceDir" -ForegroundColor Red
    exit 1
}

# Check if build tool exists
if (-not (Test-Path $ToolExe -PathType Leaf))
{
    Write-Host "Error: Build tool not found: $ToolExe" -ForegroundColor Red
    Write-Host ""
    Write-Host "Please build the project first with CMake:" -ForegroundColor Yellow
    Write-Host "  1. Open $BuildDir\FITD.sln in Visual Studio"
    Write-Host "  2. Build the build_hda_archive target in $Configuration configuration"
    Write-Host "  3. Run this script again"
    exit 1
}

# Backup existing archive if it exists
if (Test-Path $OutputArchive -PathType Leaf)
{
    $BackupFile = "$OutputArchive.backup"
    Write-Host "Backing up existing archive to: $BackupFile"
    Move-Item -Path $OutputArchive -Destination $BackupFile -Force | Out-Null
}

# Run the builder
Write-Host ""
Write-Host "Running archive builder..." -ForegroundColor Cyan
& $ToolExe $SourceDir $OutputArchive

if ($LASTEXITCODE -ne 0)
{
    Write-Host ""
    Write-Host "Error: Failed to build archive!" -ForegroundColor Red
    exit 1
}

Write-Host ""
Write-Host "====================================================================" -ForegroundColor Green
Write-Host "Archive rebuilt successfully!" -ForegroundColor Green
Write-Host "Output: $OutputArchive" -ForegroundColor Green
Write-Host "====================================================================" -ForegroundColor Green
Write-Host ""
