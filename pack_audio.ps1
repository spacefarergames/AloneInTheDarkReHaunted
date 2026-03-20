###############################################################################
# pack_audio.ps1
# Packs all MP3 and WAV files from a game directory into a single .hda archive.
#
# Usage:
#   powershell -ExecutionPolicy Bypass -File pack_audio.ps1 <game_folder> [output_file]
#
# Example:
#   powershell -ExecutionPolicy Bypass -File pack_audio.ps1 "C:\Game\Release" "C:\Game\Release\audio.hda"
#
# If output_file is omitted, creates audio.hda in the game folder.
# Archive uses the same HDBG format as pack_backgrounds.ps1.
###############################################################################

param(
    [Parameter(Mandatory=$true)]
    [string]$InputFolder,

    [Parameter(Mandatory=$false)]
    [string]$OutputFile
)

if (-not (Test-Path $InputFolder -PathType Container)) {
    Write-Error "Input folder not found: $InputFolder"
    exit 1
}

if (-not $OutputFile) {
    $OutputFile = Join-Path $InputFolder "audio.hda"
}

# Enumerate MP3 and WAV files (non-recursive, they're in the root)
$allFiles = Get-ChildItem $InputFolder -File | Where-Object { $_.Extension -match "\.(mp3|wav|ogg)$" } | Sort-Object Name
Write-Host "Found $($allFiles.Count) audio files in $InputFolder"

if ($allFiles.Count -eq 0) {
    Write-Error "No audio files found in $InputFolder"
    exit 1
}

# Build TOC entries with just the filename (no path prefix)
$tocEntries = @()
foreach ($file in $allFiles) {
    $relPath = $file.Name  # just the filename, e.g. "01.mp3", "Back.wav"
    $tocEntries += [PSCustomObject]@{
        Path     = $relPath
        FullPath = $file.FullName
        Size     = $file.Length
        Offset   = [uint64]0  # filled in below
    }
}

# Calculate offsets
# Header: 4 (magic) + 4 (version) + 4 (count) = 12 bytes
# Each TOC entry: 2 (pathLen) + pathLen + 8 (offset) + 8 (size)
$headerSize = 12
$tocSize = 0
foreach ($entry in $tocEntries) {
    $pathBytes = [System.Text.Encoding]::UTF8.GetBytes($entry.Path)
    $tocSize += 2 + $pathBytes.Length + 8 + 8
}

$dataStart = $headerSize + $tocSize
$currentOffset = $dataStart

foreach ($entry in $tocEntries) {
    $entry.Offset = [uint64]$currentOffset
    $currentOffset += $entry.Size
}

Write-Host "Header: $headerSize bytes, TOC: $tocSize bytes, Data starts at: $dataStart"
Write-Host "Total archive size: $([math]::Round($currentOffset / 1MB, 2)) MB"

# Write archive
$fs = [System.IO.File]::Create($OutputFile)
$bw = New-Object System.IO.BinaryWriter($fs)

# Header
$bw.Write([uint32]0x47424448)  # "HDBG" magic
$bw.Write([uint32]1)           # version
$bw.Write([uint32]$tocEntries.Count)

# TOC
foreach ($entry in $tocEntries) {
    $pathBytes = [System.Text.Encoding]::UTF8.GetBytes($entry.Path)
    $bw.Write([uint16]$pathBytes.Length)
    $bw.Write($pathBytes)
    $bw.Write([uint64]$entry.Offset)
    $bw.Write([uint64]$entry.Size)
}

# Data
$buffer = New-Object byte[] (1024 * 1024)  # 1MB copy buffer
$fileIdx = 0
foreach ($entry in $tocEntries) {
    $fileIdx++
    Write-Host "  [$fileIdx/$($tocEntries.Count)] $($entry.Path) ($([math]::Round($entry.Size / 1KB, 1)) KB)"
    
    $srcStream = [System.IO.File]::OpenRead($entry.FullPath)
    while ($true) {
        $bytesRead = $srcStream.Read($buffer, 0, $buffer.Length)
        if ($bytesRead -eq 0) { break }
        $bw.Write($buffer, 0, $bytesRead)
    }
    $srcStream.Close()
}

$bw.Close()
$fs.Close()

$finalSize = (Get-Item $OutputFile).Length
Write-Host "`nCreated $OutputFile"
Write-Host "Archive size: $([math]::Round($finalSize / 1MB, 2)) MB ($($tocEntries.Count) entries)"
