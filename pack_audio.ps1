# pack_audio.ps1
# Packs audio files (mp3, ogg, wav, voc) into a single HDBG-format archive (audio.hda)
# Usage: powershell -File pack_audio.ps1 [-SourceDir <path>] [-OutputFile <path>] [-VocDir <path>]

param(
    [string]$SourceDir  = "D:\FITD\build\vs2026\Fitd\Release",
    [string]$OutputFile = "D:\FITD\build\vs2026\Fitd\Release\audio.hda",
    [string]$VocDir     = "E:\INDARK"
)

$audioFiles = @()
$audioFiles += Get-ChildItem $SourceDir -Filter "*.mp3" -File
$audioFiles += Get-ChildItem $SourceDir -Filter "*.ogg" -File
$audioFiles += Get-ChildItem $SourceDir -Filter "*.wav" -File
if ($VocDir -and (Test-Path $VocDir)) {
    $vocFiles = Get-ChildItem $VocDir -Filter "*.voc" -File
    if ($vocFiles) {
        $audioFiles += $vocFiles
        Write-Host "Found $($vocFiles.Count) VOC files in $VocDir"
    }
} else {
    Write-Host "VOC directory '$VocDir' not found, skipping VOC files"
}
$audioFiles = $audioFiles | Sort-Object Name

if ($audioFiles.Count -eq 0) {
    Write-Host "No audio files found in $SourceDir"
    exit 1
}

Write-Host "Packing $($audioFiles.Count) audio files into $OutputFile ..."

$tocSize = 0
foreach ($f in $audioFiles) {
    $nameBytes = [System.Text.Encoding]::ASCII.GetBytes($f.Name)
    $tocSize += 2 + $nameBytes.Length + 8 + 8
}
$headerSize = 4 + 4 + 4
$dataOffset = $headerSize + $tocSize

$entries = @()
$currentOffset = $dataOffset
foreach ($f in $audioFiles) {
    $nameBytes = [System.Text.Encoding]::ASCII.GetBytes($f.Name)
    $entries += [PSCustomObject]@{
        NameBytes = $nameBytes
        Offset    = [uint64]$currentOffset
        Size      = [uint64]$f.Length
        FullPath  = $f.FullName
    }
    $currentOffset += $f.Length
}

try {
    $fs = [System.IO.File]::Create($OutputFile)
    $bw = New-Object System.IO.BinaryWriter($fs)

    # HDBG header
    $bw.Write([uint32]0x47424448)
    $bw.Write([uint32]1)
    $bw.Write([uint32]$audioFiles.Count)

    # TOC entries
    foreach ($e in $entries) {
        $bw.Write([uint16]$e.NameBytes.Length)
        $bw.Write($e.NameBytes)
        $bw.Write([uint64]$e.Offset)
        $bw.Write([uint64]$e.Size)
    }

    # File data
    $written = 0
    foreach ($e in $entries) {
        $data = [System.IO.File]::ReadAllBytes($e.FullPath)
        $bw.Write($data)
        $written++
        if ($written % 100 -eq 0) {
            Write-Host "  ... $written / $($entries.Count) files written"
        }
    }

    $bw.Flush()
    $bw.Close()
    $fs.Close()

    $finalSize = (Get-Item $OutputFile).Length
    Write-Host "Done! $OutputFile ($finalSize bytes, $($entries.Count) entries)"
}
catch {
    Write-Host "ERROR: $_"
    if ($bw) { $bw.Close() }
    if ($fs) { $fs.Close() }
    exit 1
}
