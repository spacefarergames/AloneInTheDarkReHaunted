# generate_embedded.ps1
# Converts binary PAK/ITD files to C++ byte array source files
# Usage: powershell -File generate_embedded.ps1 -DataDir <path_to_data_files>

param(
    [string]$DataDir = "C:\Users\patte\FITD\build\vs2026\Fitd\Release",
    [string]$OutputDir = "C:\Users\patte\FITD\FitdLib\embedded"
)

$copyright = @"
///////////////////////////////////////////////////////////////////////////////
// Alone In The Dark Re-Haunted
// Copyright (C) 2026 Infogrames / Spacefarer Retro Remasters LLC
// Author: Jake Jackson (jake@spacefarergames.com)
//
// Auto-generated embedded data file - DO NOT EDIT
///////////////////////////////////////////////////////////////////////////////
"@

# Collect all PAK files and non-save ITD files
$files = @()
$files += Get-ChildItem $DataDir -Filter "*.PAK" -File
$files += Get-ChildItem $DataDir -Filter "*.ITD" -File | Where-Object { $_.Name -notlike "SAVE*" }

Write-Host "Found $($files.Count) files to embed"

# Ensure output directory exists
New-Item -ItemType Directory -Path $OutputDir -Force | Out-Null

foreach ($file in $files) {
    $safeName = $file.Name -replace '\.', '_'
    $varName = "embdata_$safeName"
    $outFile = Join-Path $OutputDir "embedded_$safeName.cpp"

    Write-Host "  Generating $($file.Name) ($([math]::Round($file.Length/1024,1)) KB)..."

    $bytes = [System.IO.File]::ReadAllBytes($file.FullName)

    $sb = [System.Text.StringBuilder]::new($bytes.Length * 6 + 500)
    [void]$sb.AppendLine($copyright)
    [void]$sb.AppendLine("")
    [void]$sb.AppendLine("extern const unsigned char ${varName}[] = {")

    # Write bytes in rows of 16
    for ($i = 0; $i -lt $bytes.Length; $i += 16) {
        [void]$sb.Append("    ")
        $end = [Math]::Min($i + 16, $bytes.Length)
        for ($j = $i; $j -lt $end; $j++) {
            [void]$sb.Append("0x")
            [void]$sb.Append($bytes[$j].ToString("X2"))
            if ($j -lt $bytes.Length - 1) {
                [void]$sb.Append(",")
            }
            if ($j -lt $end - 1) {
                [void]$sb.Append(" ")
            }
        }
        [void]$sb.AppendLine()
    }

    [void]$sb.AppendLine("};")
    [void]$sb.AppendLine("")
    $fileLen = $bytes.Length
    [void]$sb.AppendLine("extern const unsigned long long ${varName}_size = ${fileLen}ULL;")

    [System.IO.File]::WriteAllText($outFile, $sb.ToString())
}

Write-Host ""
Write-Host "Done! Generated $($files.Count) files in $OutputDir"

# Now generate the registry file
Write-Host "Generating embeddedData.cpp registry..."

$registrySb = [System.Text.StringBuilder]::new()
[void]$registrySb.AppendLine($copyright)
[void]$registrySb.AppendLine("")
[void]$registrySb.AppendLine('#include "embeddedData.h"')
[void]$registrySb.AppendLine('#include <cstring>')
[void]$registrySb.AppendLine('#include <cctype>')
[void]$registrySb.AppendLine("")

# Extern declarations
foreach ($file in $files) {
    $safeName = $file.Name -replace '\.', '_'
    $varName = "embdata_$safeName"
    [void]$registrySb.AppendLine("extern extern const unsigned char ${varName}[];")
    [void]$registrySb.AppendLine("extern extern const unsigned long long ${varName}_size;")
}

[void]$registrySb.AppendLine("")
[void]$registrySb.AppendLine("struct EmbeddedFileEntry {")
[void]$registrySb.AppendLine("    const char* name;")
[void]$registrySb.AppendLine("    const unsigned char* data;")
[void]$registrySb.AppendLine("    unsigned long long size;")
[void]$registrySb.AppendLine("};")
[void]$registrySb.AppendLine("")
[void]$registrySb.AppendLine("static const EmbeddedFileEntry s_embeddedFiles[] = {")

foreach ($file in $files) {
    $safeName = $file.Name -replace '\.', '_'
    $varName = "embdata_$safeName"
    [void]$registrySb.AppendLine("    { `"$($file.Name)`", ${varName}, ${varName}_size },")
}

[void]$registrySb.AppendLine("};")
[void]$registrySb.AppendLine("")
[void]$registrySb.AppendLine("static const int s_numEmbeddedFiles = sizeof(s_embeddedFiles) / sizeof(s_embeddedFiles[0]);")
[void]$registrySb.AppendLine("")

# Helper to extract filename from path
[void]$registrySb.AppendLine("static const char* extractFilename(const char* path)")
[void]$registrySb.AppendLine("{")
[void]$registrySb.AppendLine("    const char* lastSlash = path;")
[void]$registrySb.AppendLine("    for (const char* p = path; *p; p++)")
[void]$registrySb.AppendLine("    {")
[void]$registrySb.AppendLine("        if (*p == '/' || *p == '\\')")
[void]$registrySb.AppendLine("            lastSlash = p + 1;")
[void]$registrySb.AppendLine("    }")
[void]$registrySb.AppendLine("    return lastSlash;")
[void]$registrySb.AppendLine("}")
[void]$registrySb.AppendLine("")

# Case-insensitive string compare
[void]$registrySb.AppendLine("static bool strEqualNoCase(const char* a, const char* b)")
[void]$registrySb.AppendLine("{")
[void]$registrySb.AppendLine("    while (*a && *b)")
[void]$registrySb.AppendLine("    {")
[void]$registrySb.AppendLine("        if (toupper((unsigned char)*a) != toupper((unsigned char)*b))")
[void]$registrySb.AppendLine("            return false;")
[void]$registrySb.AppendLine("        a++; b++;")
[void]$registrySb.AppendLine("    }")
[void]$registrySb.AppendLine("    return *a == *b;")
[void]$registrySb.AppendLine("}")
[void]$registrySb.AppendLine("")

# Lookup function
[void]$registrySb.AppendLine("bool getEmbeddedFile(const char* filename, const unsigned char** outData, size_t* outSize)")
[void]$registrySb.AppendLine("{")
[void]$registrySb.AppendLine("    const char* name = extractFilename(filename);")
[void]$registrySb.AppendLine("    for (int i = 0; i < s_numEmbeddedFiles; i++)")
[void]$registrySb.AppendLine("    {")
[void]$registrySb.AppendLine("        if (strEqualNoCase(name, s_embeddedFiles[i].name))")
[void]$registrySb.AppendLine("        {")
[void]$registrySb.AppendLine("            if (outData) *outData = s_embeddedFiles[i].data;")
[void]$registrySb.AppendLine("            if (outSize) *outSize = (size_t)s_embeddedFiles[i].size;")
[void]$registrySb.AppendLine("            return true;")
[void]$registrySb.AppendLine("        }")
[void]$registrySb.AppendLine("    }")
[void]$registrySb.AppendLine("    return false;")
[void]$registrySb.AppendLine("}")

$registryPath = Join-Path $OutputDir "embeddedData.cpp"
[System.IO.File]::WriteAllText($registryPath, $registrySb.ToString())

Write-Host "Generated embeddedData.cpp"
Write-Host "All done!"
