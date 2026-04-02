///////////////////////////////////////////////////////////////////////////////
// Alone In The Dark Re-Haunted
// Copyright (C) 2026 Infogrames / Spacefarer Retro Remasters LLC
// Based on FITD by yaz0r, Re-haunted is released under GPL
// Author: Jake Jackson (jake@spacefarergames.com)
//
// Title screen and startup menu system
///////////////////////////////////////////////////////////////////////////////

#include "common.h"
#include "fontTTF.h"
#include "input.h"
#include "configRemaster.h"
#include <SDL.h>
#include <math.h>

// Language entry: PAK base name and display label
struct LanguageEntry
{
    const char* pakName;      // e.g. "ENGLISH"
    const char* displayName;  // e.g. "English"
};

// All supported languages (order matches languageNameTable where relevant)
static const LanguageEntry s_allLanguages[] =
{
    { "ENGLISH",  "English"  },
    { "FRANCAIS", "Fran\x87" "ais" },
    { "ITALIANO", "Italiano" },
    { "ESPAGNOL", "Espa\xA4" "ol"  },
    { "DEUTSCH",  "Deutsch"  },
};
static const int s_allLanguagesCount = sizeof(s_allLanguages) / sizeof(s_allLanguages[0]);

// UI animation state
static u32 s_mainMenuSelTime = 0;
static u32 s_langMenuSelTime = 0;

// Helper function to play menu navigation sounds
static void playMenuSound(const char* soundName)
{
    if (soundEnabled)
    {
        osystem_playSampleFromName((char*)soundName);
    }
}

void DrawMenu(int selectedEntry)
{
    if (g_gameId == AITD3) {
        LoadPak("ITD_RESS", 13, logicalScreen);
    }
    else if (detailLevel == 0) {
        AffBigCadre(160, 100, 320, 80);
    }

    int currentY = 76;

    for(int i=0; i<3; i++)
    {
        if(i == selectedEntry) // highlight selected entry
        {
            if (g_gameId == AITD3) {
                SelectedMessage(160, currentY+1, i + 11, 1, 4);
            }
            else {
                u32 now = (u32)SDL_GetTicks();
                char color = (char)(100 + (int)(fabsf(sinf((float)now * 0.004f)) * 8.0f));
                int pop = 0;
                if (s_mainMenuSelTime)
                {
                    float t = (float)(now - s_mainMenuSelTime) / 250.0f;
                    if (t < 1.0f)
                        pop = (int)(sinf(t * 3.14159f) * 6.0f);
                }
                int x1, x2;
                if (g_remasterConfig.graphics.enableArtwork)
                {
                    // Constrain selector within the artwork's black box area
                    x1 = 65 - pop;
                    x2 = 255 + pop;
                    if (x1 < 63) x1 = 63;
                    if (x2 > 257) x2 = 257;
                }
                else
                {
                    x1 = 10 - pop;
                    x2 = 309 + pop;
                    if (x1 < 8) x1 = 8;
                    if (x2 > 311) x2 = 311;
                }
                AffRect(x1, currentY, x2, currentY+16, color);
                SelectedMessage(160, currentY, i + 11, 15, 4);
            }
        }
        else
        {
            SimpleMessage(160,currentY,i+11,4);
        }

        currentY+=16; // next line
    }
}

// Detect which language PAKs are available, populate availableLanguages, return count
static int detectAvailableLanguages(int* availableLanguages, int maxCount)
{
    int count = 0;
    for (int i = 0; i < s_allLanguagesCount && count < maxCount; i++)
    {
        char pakFile[32];
        strcpy(pakFile, s_allLanguages[i].pakName);
        strcat(pakFile, ".PAK");
        if (fileExists(pakFile))
        {
            availableLanguages[count++] = i;
        }
    }
    return count;
}

static void DrawLanguageMenu(int* availableLanguages, int availableCount, int selectedEntry)
{
    if (detailLevel == 0)
        AffBigCadre(160, 100, 320, availableCount * 16 + 32);

    int totalHeight = availableCount * 16;
    int currentY = 100 - totalHeight / 2;

    for (int i = 0; i < availableCount; i++)
    {
        int langIdx = availableLanguages[i];
        u8* displayName = (u8*)s_allLanguages[langIdx].displayName;
        int textWidth = ExtGetSizeFont(displayName);
        int textX = 160 - textWidth / 2;

        if (i == selectedEntry)
        {
            u32 now = (u32)SDL_GetTicks();
            char color = (char)(100 + (int)(fabsf(sinf((float)now * 0.004f)) * 8.0f));
            int pop = 0;
            if (s_langMenuSelTime)
            {
                float t = (float)(now - s_langMenuSelTime) / 250.0f;
                if (t < 1.0f)
                    pop = (int)(sinf(t * 3.14159f) * 6.0f);
            }
            int boxPad = 00;
            int x1 = textX - boxPad - pop;
            int x2 = textX + textWidth + boxPad + pop;
            if (x1 < textX - boxPad - 6) x1 = textX - boxPad - 6;
            if (x2 > textX + textWidth + boxPad + 6) x2 = textX + textWidth + boxPad + 6;
            AffRect(x1, currentY, x2, currentY+16, color);

            SetFont(PtrFont, 4);
            PrintFont(textX, currentY + 1, logicalScreen, displayName);
            SetFont(PtrFont, 15);
            PrintFont(textX, currentY, logicalScreen, displayName);
        }
        else
        {
            SetFont(PtrFont, 4);
            PrintFont(textX, currentY + 1, logicalScreen, displayName);
        }

        currentY += 16;
    }
}

void LanguageSelectionMenu(void)
{
    int availableLanguages[s_allLanguagesCount];
    int availableCount = detectAvailableLanguages(availableLanguages, s_allLanguagesCount);

    // If 0 or 1 language available, skip the menu
    if (availableCount <= 1)
        return;

    int currentSelectedEntry = 0;
    int selectedEntry = -1;
    int AntiRebond = 0;

    flushScreen();

    DrawLanguageMenu(availableLanguages, availableCount, 0);

    osystem_startFrame();
    osystem_stopFrame();
    osystem_CopyBlockPhys((unsigned char*)logicalScreen, 0, 0, 320, 200);

    osystem_flip(NULL);
    FadeInPhys(16, 0);

    while (selectedEntry == -1)
    {
        flushScreen();
        DrawLanguageMenu(availableLanguages, availableCount, currentSelectedEntry);
        osystem_CopyBlockPhys((unsigned char*)logicalScreen, 0, 0, 320, 200);
        osystem_startFrame();

        if (detailLevel == 1)
            osystem_drawLanguageSelectionBackground();
        process_events();

        // Handle window resize
        if (g_windowWasResized)
        {
            resetWindowResizeFlag();
            DrawLanguageMenu(availableLanguages, availableCount, currentSelectedEntry);
            osystem_CopyBlockPhys((unsigned char*)logicalScreen, 0, 0, 320, 200);
        }

        if(AntiRebond)
        {
            if(!JoyD && !key && !Click)
                AntiRebond = 0;
        }
        else if (JoyD & 1) // up key
        {
            currentSelectedEntry--;
            if (currentSelectedEntry < 0)
                currentSelectedEntry = availableCount - 1;
            s_langMenuSelTime = (u32)SDL_GetTicks();
            playMenuSound("Navigation.wav");
            notifyTTFMenuSelectionChanged();
            AntiRebond = 1;
        }
        else if (JoyD & 2) // down key
        {
            currentSelectedEntry++;
            if (currentSelectedEntry >= availableCount)
                currentSelectedEntry = 0;
            s_langMenuSelTime = (u32)SDL_GetTicks();
            playMenuSound("Navigation.wav");
            notifyTTFMenuSelectionChanged();
            AntiRebond = 1;
        }
        else if (key == 28 || Click != 0) // select
        {
            playMenuSound("Select.wav");
            selectedEntry = currentSelectedEntry;
        }

        osystem_drawControllerHint(20.0f);

        osystem_stopFrame();
        osystem_flip(NULL);
    }

    FadeOutPhys(16, 0);

    while (JoyD)
        process_events();

    // Apply selected language if different from current
    int langIdx = availableLanguages[selectedEntry];
    const char* selectedLang = s_allLanguages[langIdx].pakName;
    if (languageNameString != selectedLang)
    {
        reloadLanguage(selectedLang);
    }
}

int MainMenu(void)
{
    int currentSelectedEntry = 0;
    unsigned int chrono;
    int selectedEntry = -1;
    int AntiRebond = 0;

    flushScreen();

    if (g_gameId == AITD3) {
        LoadPak("ITD_RESS", 47, aux);

        palette_t lpalette;
        copyPalette(aux, lpalette);
        convertPaletteIfRequired(lpalette);
        copyPalette(lpalette, currentGamePalette);
        setPalette(lpalette);
    }

    DrawMenu(0);

    osystem_startFrame();
    osystem_stopFrame();
    osystem_CopyBlockPhys((unsigned char*)logicalScreen,0,0,320,200);

    osystem_flip(NULL);
    FadeInPhys(16,0);
    startChrono(&chrono);

	while(evalChrono(&chrono) <= 0x10000) // exit loop only if time out or if choice made
	{
		flushScreen();
		DrawMenu(currentSelectedEntry);
		osystem_CopyBlockPhys((unsigned char*)logicalScreen,0,0,320,200);
		osystem_startFrame();

		if (detailLevel == 1)
			osystem_drawStartupMenuBackground();
		if(selectedEntry!=-1 || evalChrono(&chrono) > 0x10000)
		{
			break;
		}

		process_events();

		// Handle window resize
		if (g_windowWasResized)
		{
			resetWindowResizeFlag();
			DrawMenu(currentSelectedEntry);
			osystem_CopyBlockPhys((unsigned char*)logicalScreen,0,0,320,200);
		}


		if(AntiRebond)
		{
			if(!JoyD && !key && !Click)
				AntiRebond = 0;
		}
		else if(JoyD&1) // up key
		{
			currentSelectedEntry--;
			if(currentSelectedEntry<0)
				currentSelectedEntry = 2;
			s_mainMenuSelTime = (u32)SDL_GetTicks();
			playMenuSound("Navigation.wav");
			notifyTTFMenuSelectionChanged();
			startChrono(&chrono);
			AntiRebond = 1;
		}
		else if(JoyD&2) // down key
		{
			currentSelectedEntry++;
			if(currentSelectedEntry>2)
				currentSelectedEntry = 0;
			s_mainMenuSelTime = (u32)SDL_GetTicks();
			playMenuSound("Navigation.wav");
			notifyTTFMenuSelectionChanged();
			startChrono(&chrono);
			AntiRebond = 1;
		}
		else if(key == 28 || Click != 0) // select current entry
		{
			playMenuSound("Select.wav");
			selectedEntry = currentSelectedEntry;
		}

        // Draw controller hint image at the bottom of the screen
        osystem_drawControllerHint();

        osystem_stopFrame();
        osystem_flip(NULL);
    }

    if(selectedEntry==2) // if exit game, do not fade
    {
        FadeOutPhys(16,0);
    }

    while(JoyD)
    {
        process_events();
    }

    return(selectedEntry);
}
