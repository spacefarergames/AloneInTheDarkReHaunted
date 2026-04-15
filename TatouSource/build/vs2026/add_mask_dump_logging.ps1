# PowerShell script to add comprehensive logging to mask dump system
# Step 1: Instrument buildMaskFilename()

$filePath = "D:\FITD\FitdLib\rendererBGFX.cpp"
$content = Get-Content -Path $filePath -Raw

# Original function
$oldFunction = @"
static void buildMaskFilename(char* buf, size_t bufSize, const char* folder, int actualRoomNumber, int maskId)
{
    // Organise by floor/camera/room so masks are uniquely identified:
    //   <folder>/FLOOR##/CAM###/ROOM##_MASK##.png
    // actualRoomNumber is the real room number (not the camera-relative index).
    char floorDir[512];
    snprintf(floorDir, sizeof(floorDir), "%s/FLOOR%02d", folder, (int)g_currentFloor);
    ensureMaskDirectory(floorDir);

    char camDir[512];
    snprintf(camDir, sizeof(camDir), "%s/FLOOR%02d/CAM%03d", folder, (int)g_currentFloor, (int)NumCamera);
    ensureMaskDirectory(camDir);

    snprintf(buf, bufSize, "%s/FLOOR%02d/CAM%03d/ROOM%02d_MASK%02d.png", folder, (int)g_currentFloor, (int)NumCamera, actualRoomNumber, maskId);
}
"@

# New function with logging
$newFunction = @"
static void buildMaskFilename(char* buf, size_t bufSize, const char* folder, int actualRoomNumber, int maskId)
{
    // Organise by floor/camera/room so masks are uniquely identified:
    //   <folder>/FLOOR##/CAM###/ROOM##_MASK##.png
    // actualRoomNumber is the real room number (not the camera-relative index).
    
    // Parameter validation and logging
    if (!buf || bufSize < 256)
    {
        printf(RBGFX_WARN "[MASK_DUMP] buildMaskFilename: Invalid buffer (buf=%p, bufSize=%zu)" CON_RESET "\n", 
               buf, bufSize);
        return;
    }
    
    if (!folder)
    {
        printf(RBGFX_WARN "[MASK_DUMP] buildMaskFilename: NULL folder parameter" CON_RESET "\n");
        return;
    }
    
    // Log function entry with all parameters
    printf(RBGFX_TAG "[MASK_DUMP] buildMaskFilename ENTRY:\n");
    printf(RBGFX_TAG "  Input folder: %s\n", folder);
    printf(RBGFX_TAG "  Input actualRoomNumber: %d\n", actualRoomNumber);
    printf(RBGFX_TAG "  Input maskId: %d\n", maskId);
    printf(RBGFX_TAG "  Global g_currentFloor: %d\n", (int)g_currentFloor);
    printf(RBGFX_TAG "  Global NumCamera: %d\n", (int)NumCamera);
    
    // Validation: check for out-of-range values
    if (actualRoomNumber < 0 || actualRoomNumber > 99)
    {
        printf(RBGFX_WARN "[MASK_DUMP] WARNING: actualRoomNumber out of range (0-99): %d" CON_RESET "\n", 
               actualRoomNumber);
    }
    if (maskId < 0 || maskId > 99)
    {
        printf(RBGFX_WARN "[MASK_DUMP] WARNING: maskId out of range (0-99): %d" CON_RESET "\n", 
               maskId);
    }
    
    char floorDir[512];
    snprintf(floorDir, sizeof(floorDir), "%s/FLOOR%02d", folder, (int)g_currentFloor);
    printf(RBGFX_TAG "[MASK_DUMP]  Floor dir: %s\n", floorDir);
    ensureMaskDirectory(floorDir);

    char camDir[512];
    snprintf(camDir, sizeof(camDir), "%s/FLOOR%02d/CAM%03d", folder, (int)g_currentFloor, (int)NumCamera);
    printf(RBGFX_TAG "[MASK_DUMP]  Camera dir: %s\n", camDir);
    ensureMaskDirectory(camDir);

    snprintf(buf, bufSize, "%s/FLOOR%02d/CAM%03d/ROOM%02d_MASK%02d.png", folder, (int)g_currentFloor, (int)NumCamera, actualRoomNumber, maskId);
    printf(RBGFX_TAG "[MASK_DUMP]  Final path: %s" CON_RESET "\n", buf);
}
"@

# Replace the function
$content = $content.Replace($oldFunction, $newFunction)

# Write the modified content back
Set-Content -Path $filePath -Value $content -Encoding UTF8

Write-Host "Step 1 Complete: Added comprehensive logging to buildMaskFilename()"
