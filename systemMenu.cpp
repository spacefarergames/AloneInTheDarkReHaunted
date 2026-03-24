///////////////////////////////////////////////////////////////////////////////
// Alone In The Dark Re-Haunted
// Copyright (C) 2026 Infogrames / Spacefarer Retro Remasters LLC
// Based on FITD by yaz0r, Re-haunted is released under GPL
// Author: Jake Jackson (jake@spacefarergames.com)
//
// In-game pause menu, save/load, and options
///////////////////////////////////////////////////////////////////////////////

#include "common.h"
#include "configRemaster.h"
#include "fontTTF.h"
#include "hdBackgroundRenderer.h"
#include "controlsMenu.h"
void SetLevelDestPal(palette_t& inPalette, palette_t& outPalette, int coef);

// Helper function to play menu navigation sounds
void playMenuSound(const char* soundName)
{
    if (soundEnabled)
    {
        osystem_playSampleFromName((char*)soundName);
    }
}

#define	NB_OPTIONS	8
#define SELECT_COUL 0xF
#define MENU_COUL 4
#define	SIZE_FONT 16

// Scale down the game screen and draw it to the preview box
// destX, destY = top-left corner of the AffBigCadre box
void scaleDownImage(int destX, int destY, char* sourceBuffer)
{
    // Preview box is created at position (80,55) with size (120x70)
    // Account for frame border (approximately 3 pixels on each side)
    int frameThickness = 3;
    int previewWidth = 120 - (frameThickness * 2);   // 114 pixels
    int previewHeight = 75 - (frameThickness * 2);   // 64 pixels
    int previewStartX = destX + frameThickness;
    int previewStartY = destY + frameThickness;

    // Source dimensions: 320x200 pixels (game screen)
    float scaleX = 320.0f / (float)previewWidth;
    float scaleY = 200.0f / (float)previewHeight;

    // Draw scaled image into logicalScreen at the preview position
    for(int y = 0; y < previewHeight; y++)
    {
        for(int x = 0; x < previewWidth; x++)
        {
            // Sample from source using nearest neighbor
            int srcX = (int)(x * scaleX);
            int srcY = (int)(y * scaleY);

            // Ensure we don't read outside source buffer
            if(srcX >= 0 && srcX < 320 && srcY >= 0 && srcY < 200)
            {
                char pixel = sourceBuffer[srcY * 320 + srcX];

                // Write to destination position in logicalScreen
                int dstX = previewStartX + x;
                int dstY = previewStartY + y;

                if(dstX >= 0 && dstX < 320 && dstY >= 0 && dstY < 200)
                {
                    logicalScreen[dstY * 320 + dstX] = pixel;
                }
            }
        }
    }
}

// Scale down HD background (RGBA) and draw it to the preview box
// Uses bilinear interpolation for smooth scaling, then converts to palette
// destX, destY = top-left corner of the AffBigCadre box
void scaleDownImageHD(int destX, int destY, unsigned char* sourceBuffer, int srcWidth, int srcHeight)
{
    int frameThickness = 3;
    int previewWidth = 120 - (frameThickness * 2);
    int previewHeight = 75 - (frameThickness * 2);
    int previewStartX = destX + frameThickness;
    int previewStartY = destY + frameThickness;

    float scaleX = (float)(srcWidth - 1) / (float)(previewWidth - 1);
    float scaleY = (float)(srcHeight - 1) / (float)(previewHeight - 1);

    for(int y = 0; y < previewHeight; y++)
    {
        for(int x = 0; x < previewWidth; x++)
        {
            // Calculate source position with sub-pixel precision
            float srcXf = x * scaleX;
            float srcYf = y * scaleY;
            
            int srcX0 = (int)srcXf;
            int srcY0 = (int)srcYf;
            int srcX1 = (srcX0 + 1 < srcWidth) ? srcX0 + 1 : srcX0;
            int srcY1 = (srcY0 + 1 < srcHeight) ? srcY0 + 1 : srcY0;
            
            // Bilinear interpolation weights
            float fx = srcXf - srcX0;
            float fy = srcYf - srcY0;
            float fx1 = 1.0f - fx;
            float fy1 = 1.0f - fy;
            
            // Sample 4 pixels
            int idx00 = (srcY0 * srcWidth + srcX0) * 4;
            int idx10 = (srcY0 * srcWidth + srcX1) * 4;
            int idx01 = (srcY1 * srcWidth + srcX0) * 4;
            int idx11 = (srcY1 * srcWidth + srcX1) * 4;
            
            // Bilinear interpolate each channel
            float r = (sourceBuffer[idx00 + 0] * fx1 + sourceBuffer[idx10 + 0] * fx) * fy1 +
                      (sourceBuffer[idx01 + 0] * fx1 + sourceBuffer[idx11 + 0] * fx) * fy;
            float g = (sourceBuffer[idx00 + 1] * fx1 + sourceBuffer[idx10 + 1] * fx) * fy1 +
                      (sourceBuffer[idx01 + 1] * fx1 + sourceBuffer[idx11 + 1] * fx) * fy;
            float b = (sourceBuffer[idx00 + 2] * fx1 + sourceBuffer[idx10 + 2] * fx) * fy1 +
                      (sourceBuffer[idx01 + 2] * fx1 + sourceBuffer[idx11 + 2] * fx) * fy;
            
            int ri = (int)(r + 0.5f);
            int gi = (int)(g + 0.5f);
            int bi = (int)(b + 0.5f);

            // Find closest palette color
            int bestIdx = 0;
            int bestDist = 999999;
            for(int i = 16; i < 256; i++)
            {
                int pr = currentGamePalette[i][0];
                int pg = currentGamePalette[i][1];
                int pb = currentGamePalette[i][2];
                int dist = (ri - pr) * (ri - pr) + (gi - pg) * (gi - pg) + (bi - pb) * (bi - pb);
                if(dist < bestDist)
                {
                    bestDist = dist;
                    bestIdx = i;
                }
            }

            int dstX = previewStartX + x;
            int dstY = previewStartY + y;

            if(dstX >= 0 && dstX < 320 && dstY >= 0 && dstY < 200)
            {
                logicalScreen[dstY * 320 + dstX] = (char)bestIdx;
            }
        }
    }
}
void AffOption(int n, int num, int selected)
{
    int y = WindowY1 + ((WindowY2 - WindowY1) / 2) - (NB_OPTIONS * SIZE_FONT) / 2 + (n * SIZE_FONT);

    if(n == selected)
    {
        SelectedMessage( 160, y, num, SELECT_COUL, MENU_COUL);
    }
    else
    {
        SimpleMessage( 160, y, num, MENU_COUL);
    }
}

void AffOptionList(int selectedStringNumber)
{
    AffBigCadre(160,100,320,200);

    int backupTop = WindowY1;
    int backupBottom = WindowY2;
    int backupLeft = WindowX1;
    int backupRight = WindowX2;

    // Draw preview box: center (80,55), size 120x70
    // This means top-left corner is at (80-60, 55-35) = (20, 20)
    AffBigCadre(80,55,120,70);

    // Draw the scaled game screen preview at the box's top-left corner (20,20)
    // Prefer GPU-captured scene (includes 3D objects at output resolution),
    // fall back to HD background data, then palettized aux2.
    unsigned char* capturedScene = osystem_getScenePreviewData();
    int capturedW = osystem_getScenePreviewWidth();
    int capturedH = osystem_getScenePreviewHeight();
    if(capturedScene && capturedW > 0 && capturedH > 0)
    {
        scaleDownImageHD(20, 20, capturedScene, capturedW, capturedH);
    }
    else if(g_hdBackgroundPreviewData && g_hdBackgroundPreviewWidth > 0 && g_hdBackgroundPreviewHeight > 0)
    {
        scaleDownImageHD(20, 20, g_hdBackgroundPreviewData, g_hdBackgroundPreviewWidth, g_hdBackgroundPreviewHeight);
    }
    else
    {
        scaleDownImage(20, 20, aux2);
    }

    WindowY1 = backupTop;
    WindowY2 = backupBottom;
    WindowX1 = backupLeft;
    WindowX2 = backupRight;

    SetClip(WindowX1,WindowY1,WindowX2,WindowY2);

    AffOption(0,48,selectedStringNumber);
    AffOption(1,45,selectedStringNumber);
    AffOption(2,46,selectedStringNumber);
    AffOption(3,42-musicEnabled,selectedStringNumber);
    AffOption(4,43+soundEnabled,selectedStringNumber);
    AffOption(5,49+detailLevel,selectedStringNumber);

    // Controls option (no string table entry, draw manually)
    {
        int y = WindowY1 + ((WindowY2 - WindowY1) / 2) - (NB_OPTIONS * SIZE_FONT) / 2 + (6 * SIZE_FONT);
        const char* controlsText = "Controls";
        if (languageNameString == "FRANCAIS")
            controlsText = "Commandes";
        else if (languageNameString == "ITALIANO")
            controlsText = "Comandi";
        else if (languageNameString == "ESPAGNOL")
            controlsText = "Controles";
        else if (languageNameString == "DEUTSCH")
            controlsText = "Steuerung";
        int w = ExtGetSizeFont((u8*)controlsText);
        int tx = 160 - w / 2;

        if (6 == selectedStringNumber)
        {
            // Shadow + foreground like SelectedMessage
            SetFont(PtrFont, MENU_COUL);
            PrintFont(tx, y + 1, logicalScreen, (u8*)controlsText);
            SetFont(PtrFont, SELECT_COUL);
            PrintFont(tx, y, logicalScreen, (u8*)controlsText);
        }
        else
        {
            SetFont(PtrFont, MENU_COUL);
            PrintFont(tx, y + 1, logicalScreen, (u8*)controlsText);
        }
    }

    AffOption(7,47,selectedStringNumber);

    menuWaitVSync();
}

void processSystemMenu(void)
{
    //int entry = -1;
    int exitMenu = 0;
    int currentSelectedEntry;

    // Scene snapshot was already frozen by PlayWorld() before the ESC-draining
    // loop, so s_snapshotTex holds the last game frame (with 3D objects).
    // Pump 2 frames to ensure the GPU readback completes, then finalize.
    process_events(); // frame 1 — readback in flight
    process_events(); // frame 2 — readback completes
    osystem_finalizeScenePreview();

    // Fallback: capture palettized background for non-PP path
    if (g_currentBackgroundIsHD)
    {
        memcpy(aux2, aux, 64000);
    }
    else
    {
        memcpy(aux2, logicalScreen, 64000);
    }

    // Store original palette if blur is enabled
    palette_t originalPalette;
    palette_t darkenedPalette;
    bool useBlurEffect = g_remasterConfig.graphics.enableBlurredMenu;

    if (useBlurEffect)
    {
        copyPalette(currentGamePalette, originalPalette);
        // Darken to create blur-like effect (0-256 scale, use 40% brightness)
        int darkenFactor = (int)(256.0f * (1.0f - g_remasterConfig.graphics.menuBlurAmount / 10.0f));
        if (darkenFactor < 64) darkenFactor = 64; // Minimum 25% brightness
        if (darkenFactor > 192) darkenFactor = 192; // Maximum 75% brightness
        SetLevelDestPal(originalPalette, darkenedPalette, darkenFactor);
    }

    // Fallback from HD background to standard for menu rendering
    // (This happens AFTER we've captured aux2)
    if (g_currentBackgroundIsHD)
    {
        recreateBackgroundTexture(320, 200);
    }

    SaveTimerAnim();
    //pauseShaking();

    if(lightOff)
    {
        //makeBlackPalette();
    }

    //clearScreenSystemMenu(unkScreenVar,aux2);

    currentSelectedEntry = 0;

	while(!exitMenu)
	{
		// Apply darkened palette if blur effect is enabled
		if (useBlurEffect)
		{
			setPalette(darkenedPalette);
		}

		AffOptionList(currentSelectedEntry);
		osystem_CopyBlockPhys((unsigned char*)logicalScreen,0,0,320,200);
		osystem_startFrame();
		process_events();
		flushScreen();
		osystem_drawBackground();

        if(lightOff)
        {
            FadeInPhys(0x40,0);
        }

        //  while(!exitMenu)
        {
            localKey = key;
            localClick = Click;
            localJoyD = JoyD;

            if(!AntiRebond)
            {
                if(localKey == 0x1C || localClick) // enter
                {
                    // Play select sound
                    playMenuSound("Select.wav");

                    switch(currentSelectedEntry)
                    {
                    case 0: // exit menu
                        exitMenu = 1;
                        break;
                    case 1: // save
                        makeSave(45);
                        break;
                    case 2: // load
                        if(restoreSave(46,1))
                        {
                            osystem_releaseScenePreview();
                            FlagInitView = 2;
                            RestoreTimerAnim();
                            //updateShaking();
                            return;
                        }
                        break;
                    case 6: // controls
                        processControlsMenu();
                        break;
                    case 7: // quit to main menu
                        FlagGameOver = 1;
                        exitMenu = 1;
                        break;

                    }
                }
                else
                {
                    if(localKey == 0x1B)
                    {
                        // Play back sound
                        playMenuSound("Back.wav");
                        exitMenu = 1;
                    }
                    if(localJoyD == 1) // up
                    {
                        currentSelectedEntry--;

                        if(currentSelectedEntry<0)
                            currentSelectedEntry = 7;

                        // Play navigation sound
                        playMenuSound("Navigation.wav");

                        // Notify TTF system that menu selection changed
                        notifyTTFMenuSelectionChanged();

                        AntiRebond = 1;
                    }
                    if(localJoyD == 2) // bottom
                    {
                        currentSelectedEntry++;

                        if(currentSelectedEntry>7)
                            currentSelectedEntry = 0;

                        // Play navigation sound
                        playMenuSound("Navigation.wav");

                        // Notify TTF system that menu selection changed
                        notifyTTFMenuSelectionChanged();

                        AntiRebond = 1;
                    }
                    // Handle left/right for music toggle
                    if((localJoyD == 4 || localJoyD == 8) && currentSelectedEntry == 3) // left or right on music option
                    {
                        musicEnabled = !musicEnabled;

                        if(!musicEnabled)
                        {
                            // Stop music by playing track -1
                            playMusic(-1);
                        }

                        // Play navigation sound
                        playMenuSound("Navigation.wav");

                        notifyTTFMenuSelectionChanged();
                        AntiRebond = 1;
                    }
                    // Handle left/right for sound toggle
                    if((localJoyD == 4 || localJoyD == 8) && currentSelectedEntry == 4) // left or right on sound option
                    {
                        soundEnabled = !soundEnabled;
                        // Play navigation sound
                        playMenuSound("Navigation.wav");
                        notifyTTFMenuSelectionChanged();
                        AntiRebond = 1;
                    }
                    // Handle left/right for detail level toggle
                    if((localJoyD == 4 || localJoyD == 8) && currentSelectedEntry == 5) // left or right on detail option
                    {
                        detailLevel = !detailLevel;

                        // Update HD backgrounds setting
                        g_remasterConfig.graphics.enableHDBackgrounds = (detailLevel == 1);

                        // Also toggle TTF fonts with HD graphics
                        g_remasterConfig.font.enableTTF = (detailLevel == 1);

                        // Toggle post-processing with HD graphics
                        g_remasterConfig.postProcessing.enableBloom = (detailLevel == 1);
                        g_remasterConfig.postProcessing.enableFilmGrain = (detailLevel == 1);
                        g_remasterConfig.postProcessing.enableSSAO = (detailLevel == 1);

                        // If switching to LOW detail, force fallback from HD to standard backgrounds
                        if(detailLevel == 0 && g_currentBackgroundIsHD)
                        {
                            recreateBackgroundTexture(320, 200);
                        }

                        // Play navigation sound
                        playMenuSound("Navigation.wav");

                        notifyTTFMenuSelectionChanged();
                        AntiRebond = 1;
                    }
                }
            }
            else
            {
                if(!localKey && !localJoyD)
                {
                    AntiRebond = 0;
                }
            }
        }

		osystem_flip(NULL);
	}

	// Restore original palette if blur was enabled
	if (useBlurEffect)
	{
		setPalette(originalPalette);
	}

	// Re-enable per-frame scene snapshot updates now that the menu is closing
	osystem_releaseScenePreview();

	// Save configuration changes
	saveRemasterConfig();

	//fadeOut(32,2);
	while(key || JoyD || Click)
	{
		process_events();
	}
	localKey = localClick = localJoyD = 0;
	FlagInitView = 2;
	RestoreTimerAnim();
}