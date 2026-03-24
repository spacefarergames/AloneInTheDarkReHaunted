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
    else {
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
                AffRect(10, currentY, 309, currentY + 16, 100);
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
            AffRect(10, currentY, 309, currentY + 16, 100);
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

    flushScreen();

    DrawLanguageMenu(availableLanguages, availableCount, 0);

    osystem_startFrame();
    osystem_stopFrame();
    osystem_CopyBlockPhys((unsigned char*)logicalScreen, 0, 0, 320, 200);

    osystem_flip(NULL);
    FadeInPhys(16, 0);

    while (selectedEntry == -1)
    {
        osystem_CopyBlockPhys((unsigned char*)logicalScreen, 0, 0, 320, 200);
        osystem_startFrame();

        process_events();

        if (JoyD & 1) // up key
        {
            currentSelectedEntry--;
            if (currentSelectedEntry < 0)
                currentSelectedEntry = availableCount - 1;

            playMenuSound("Navigation.wav");
            notifyTTFMenuSelectionChanged();

            DrawLanguageMenu(availableLanguages, availableCount, currentSelectedEntry);
            osystem_flip(NULL);

            while (JoyD)
                process_events();
        }

        if (JoyD & 2) // down key
        {
            currentSelectedEntry++;
            if (currentSelectedEntry >= availableCount)
                currentSelectedEntry = 0;

            playMenuSound("Navigation.wav");
            notifyTTFMenuSelectionChanged();

            DrawLanguageMenu(availableLanguages, availableCount, currentSelectedEntry);
            osystem_flip(NULL);

            while (JoyD)
                process_events();
        }

        if (key == 28 || Click != 0) // select
        {
            playMenuSound("Select.wav");
            selectedEntry = currentSelectedEntry;
        }

        osystem_drawControllerHint();

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
        osystem_CopyBlockPhys((unsigned char*)logicalScreen,0,0,320,200);
        osystem_startFrame();

        if(selectedEntry!=-1 || evalChrono(&chrono) > 0x10000)
        {
            break;
        }

        process_events();
		osystem_drawBackground();

        if(JoyD&1) // up key
        {
            currentSelectedEntry--;

            if(currentSelectedEntry<0)
            {
                currentSelectedEntry = 2;
            }

            // Play navigation sound
            playMenuSound("Navigation.wav");

            // Notify TTF system that menu selection changed
            notifyTTFMenuSelectionChanged();

            DrawMenu(currentSelectedEntry);
            osystem_flip(NULL);
            //      menuWaitVSync();

            startChrono(&chrono);

            while(JoyD)
            {
                process_events();
            }
        }


        if(JoyD&2) // down key
        {
            currentSelectedEntry++;

            if(currentSelectedEntry>2)
            {
                currentSelectedEntry = 0;
            }

            // Play navigation sound
            playMenuSound("Navigation.wav");

            // Notify TTF system that menu selection changed
            notifyTTFMenuSelectionChanged();

            DrawMenu(currentSelectedEntry);
            //menuWaitVSync();
            osystem_flip(NULL);

            startChrono(&chrono);

            while(JoyD)
            {
                process_events();
            }
        }

        if(key == 28 || Click != 0) // select current entry
        {
            // Play select sound
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
