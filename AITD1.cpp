///////////////////////////////////////////////////////////////////////////////
// Alone In The Dark Re-Haunted
// Copyright (C) 2026 Infogrames / Spacefarer Retro Remasters LLC
// Based on FITD by yaz0r, Re-haunted is released under GPL
// Author: Jake Jackson (jake@spacefarergames.com)
//
// Alone in the Dark 1 game-specific logic and startup sequence
///////////////////////////////////////////////////////////////////////////////

#include "common.h"

#include "hdBackground.h"
#include "hdBackgroundRenderer.h"
#include "fontTTF.h"
#include "startupMenu.h"

// DEMO mapping
/*
#define PALETTE_JEU		0
#define ITDFONT			1
*/

int AITD1KnownCVars[] =
{
    SAMPLE_PAGE,
    BODY_FLAMME,
    MAX_WEIGHT_LOADABLE,
    TEXTE_CREDITS,
    SAMPLE_TONNERRE,
    INTRO_DETECTIVE,
    INTRO_HERITIERE,
    WORLD_NUM_PERSO,
    CHOOSE_PERSO,
    SAMPLE_CHOC,
    SAMPLE_PLOUF,
    REVERSE_OBJECT,
    KILLED_SORCERER,
    LIGHT_OBJECT,
    FOG_FLAG,
    DEAD_PERSO,
    -1
};

enumLifeMacro AITD1LifeMacroTable[] =
{
    LM_DO_MOVE,
    LM_ANIM_ONCE,
    LM_ANIM_ALL_ONCE,
    LM_BODY,
    LM_IF_EGAL,
    LM_IF_DIFFERENT,
    LM_IF_SUP_EGAL,
    LM_IF_SUP,
    LM_IF_INF_EGAL,
    LM_IF_INF,
    LM_GOTO,
    LM_RETURN,
    LM_END,
    LM_ANIM_REPEAT,
    LM_ANIM_MOVE,
    LM_MOVE,
    LM_HIT,
    LM_MESSAGE,
    LM_MESSAGE_VALUE,
    LM_VAR,
    LM_INC,
    LM_DEC,
    LM_ADD,
    LM_SUB,
    LM_LIFE_MODE,
    LM_SWITCH,
    LM_CASE,
    LM_CAMERA,
    LM_START_CHRONO,
    LM_MULTI_CASE,
    LM_FOUND,
    LM_LIFE,
    LM_DELETE,
    LM_TAKE,
    LM_IN_HAND,
    LM_READ,
    LM_ANIM_SAMPLE,
    LM_SPECIAL,
    LM_DO_REAL_ZV,
    LM_SAMPLE,
    LM_TYPE,
    LM_GAME_OVER,
    LM_MANUAL_ROT,
    LM_RND_FREQ,
    LM_MUSIC,
    LM_SET_BETA,
    LM_DO_ROT_ZV,
    LM_STAGE,
    LM_FOUND_NAME,
    LM_FOUND_FLAG,
    LM_FOUND_LIFE,
    LM_CAMERA_TARGET,
    LM_DROP,
    LM_FIRE,
    LM_TEST_COL,
    LM_FOUND_BODY,
    LM_SET_ALPHA,
    LM_STOP_BETA,
    LM_DO_MAX_ZV,
    LM_PUT,
    LM_C_VAR,
    LM_DO_NORMAL_ZV,
    LM_DO_CARRE_ZV,
    LM_SAMPLE_THEN,
    LM_LIGHT,
    LM_SHAKING,
    LM_INVENTORY,
    LM_FOUND_WEIGHT,
    LM_UP_COOR_Y,
    LM_SPEED,
    LM_PUT_AT,
    LM_DEF_ZV,
    LM_HIT_OBJECT,
    LM_GET_HARD_CLIP,
    LM_ANGLE,
    LM_REP_SAMPLE,
    LM_THROW,
    LM_WATER,
    LM_PICTURE,
    LM_STOP_SAMPLE,
    LM_NEXT_MUSIC,
    LM_FADE_MUSIC,
    LM_STOP_HIT_OBJECT,
    LM_COPY_ANGLE,
    LM_END_SEQUENCE,
    LM_SAMPLE_THEN_REPEAT,
    LM_WAIT_GAME_OVER,
};

int makeIntroScreens(void)
{
    char* data;
    unsigned int chrono;

    // Try HD replacement for TITRE
    HDBackgroundInfo* hdBg = loadHDBackground("ITD_RESS", AITD1_TITRE);
    if (hdBg)
    {
        updateBackgroundTextureHD(hdBg->data, hdBg->width, hdBg->height, hdBg->channels);
        if (hdBg->isAnimated)
            setCurrentAnimatedHDBackground(hdBg);
        else
            freeHDBackground(hdBg);
    }

    data = loadPak("ITD_RESS", AITD1_TITRE);
    FastCopyScreen(data + 770, frontBuffer);
    osystem_CopyBlockPhys(frontBuffer, 0, 0, 320, 200);

    // Ensure proper rendering before fade-in
    osystem_startFrame();
    osystem_flip(NULL);

    FadeInPhys(8, 0);
    memcpy(logicalScreen, frontBuffer, 320 * 200);
    osystem_flip(NULL);
    free(data);

    // Hold the title screen for a moment (skippable by key/click)
    startChrono(&chrono);
    do
    {
        process_events();

        if (evalChrono(&chrono) >= 0x80)
            break;

    } while (key == 0 && Click == 0);

    // Try HD replacement for LIVRE
    hdBg = loadHDBackground("ITD_RESS", AITD1_LIVRE);
    if (hdBg)
    {
        updateBackgroundTextureHD(hdBg->data, hdBg->width, hdBg->height, hdBg->channels);
        if (hdBg->isAnimated)
            setCurrentAnimatedHDBackground(hdBg);
        else
            freeHDBackground(hdBg);
    }
    else if (g_currentBackgroundIsHD)
    {
        recreateBackgroundTexture(320, 200);
    }

    LoadPak("ITD_RESS", AITD1_LIVRE, aux);
    startChrono(&chrono);

    // Draw the background and ensure it's rendered
    osystem_startFrame();
    osystem_flip(NULL);

    do
    {
        int time;

        process_events();

        time = evalChrono(&chrono);

        if (time >= 0x30)
            break;

    } while (key == 0 && Click == 0);

    playSound(CVars[getCVarsIdx(SAMPLE_PAGE)]);
    /*  LastSample = -1;
    LastPriority = -1;
    LastSample = -1;
    LastPriority = 0; */
    turnPageFlag = 1;
    Lire(CVars[getCVarsIdx(TEXTE_CREDITS)] + 1, 48, 2, 260, 197, 1, 26, 0);

    setCurrentAnimatedHDBackground(nullptr);
    return(0);
}

void CopyBox_Aux_Log(int x1, int y1, int x2, int y2)
{
    int i;
    int j;

    for (i = y1; i < y2; i++)
    {
        for (j = x1; j < x2; j++)
        {
            *(screenSm3 + i * 320 + j) = *(screenSm1 + i * 320 + j);
        }
    }
}

int ChoosePerso(void)
{
    int choice = 0;
    int firsttime = 1;
    int choiceMade = 0;

    // Notify TTF that we're entering character selection
    notifyTTFMenuStateChanged(true, false);

    // Try HD replacement for character selection background
    HDBackgroundInfo* hdBg = loadHDBackground("ITD_RESS", 10);
    if (hdBg)
    {
        updateBackgroundTextureHD(hdBg->data, hdBg->width, hdBg->height, hdBg->channels);
        if (hdBg->isAnimated)
            setCurrentAnimatedHDBackground(hdBg);
        else
            freeHDBackground(hdBg);
    }

    // Store whether we're using HD background for this screen
    bool usingHDBackground = g_currentBackgroundIsHD;

    uiLayer.fill(0);
    InitCopyBox(aux, logicalScreen);

    g_portraitOverlayChoice = choice;

    while (choiceMade == 0)
    {
        process_events();
        osystem_drawBackground();

        // Stop any playing music
        fadeMusic(0, 0, 0x40);
        currentMusic = -1;

        LoadPak("ITD_RESS", 10, aux);
        FastCopyScreen(aux, logicalScreen);
        FastCopyScreen(logicalScreen, aux2);

        if (choice == 0)
        {
            AffBigCadre(80, 100, 160, 200);
            CopyBox_Aux_Log(10, 10, 149, 190);
        }
        else
        {
            AffBigCadre(240, 100, 160, 200);
            CopyBox_Aux_Log(170, 10, 309, 190);
        }

        // When using HD backgrounds, copy only the selection frame border to the UI layer
        // (exclude BOTH character portrait areas so HD background shows through for both)
        if (usingHDBackground)
        {
            // Define both character portrait regions to always exclude
            int leftPortraitX1 = 10, leftPortraitY1 = 10;
            int leftPortraitX2 = 149, leftPortraitY2 = 190;
            int rightPortraitX1 = 170, rightPortraitY1 = 10;
            int rightPortraitX2 = 309, rightPortraitY2 = 190;

            for (int i = 0; i < 200; i++)
            {
                for (int j = 0; j < 320; j++)
                {
                    // Skip both character portrait regions - let HD background show through
                    bool inLeftPortrait = (i >= leftPortraitY1 && i < leftPortraitY2 && j >= leftPortraitX1 && j < leftPortraitX2);
                    bool inRightPortrait = (i >= rightPortraitY1 && i < rightPortraitY2 && j >= rightPortraitX1 && j < rightPortraitX2);

                    if (inLeftPortrait || inRightPortrait)
                    {
                        continue;
                    }

                    unsigned char pixel = logicalScreen[i * 320 + j];
                    // Copy non-zero pixels (frame border only) to UI layer
                    if (pixel != 0)
                    {
                        uiLayer[i * 320 + j] = pixel;
                    }
                }
            }
        }

        FastCopyScreen(logicalScreen, frontBuffer);
        osystem_CopyBlockPhys(frontBuffer, 0, 0, 320, 200);

        if (firsttime != 0)
        {
            // Render complete frame (background + portrait + UI layer) before fade-in
            osystem_startFrame();
            if (usingHDBackground)
            {
                osystem_drawUILayer();
            }
            osystem_flip(NULL);

            FadeInPhys(0x40, 0);

            do
            {
                process_events();
            } while (Click || key);

            firsttime = 0;
        }

        while ((localKey = key) != 28 && Click == 0) // process input
        {
            process_events();
            osystem_drawBackground();

            if (JoyD & 4) // left
            {
                choice = 0;
                g_portraitOverlayChoice = 0;
                FastCopyScreen(aux2, logicalScreen);
                AffBigCadre(80, 100, 160, 200);
                CopyBox_Aux_Log(10, 10, 149, 190);

                // When using HD backgrounds, copy only the frame border to UI layer
                if (usingHDBackground)
                {
                    uiLayer.fill(0);

                    // Always exclude both character portrait regions
                    int leftPortraitX1 = 10, leftPortraitY1 = 10;
                    int leftPortraitX2 = 149, leftPortraitY2 = 190;
                    int rightPortraitX1 = 170, rightPortraitY1 = 10;
                    int rightPortraitX2 = 309, rightPortraitY2 = 190;

                    for (int i = 0; i < 200; i++)
                    {
                        for (int j = 0; j < 320; j++)
                        {
                            // Skip both character portrait regions
                            bool inLeftPortrait = (i >= leftPortraitY1 && i < leftPortraitY2 && j >= leftPortraitX1 && j < leftPortraitX2);
                            bool inRightPortrait = (i >= rightPortraitY1 && i < rightPortraitY2 && j >= rightPortraitX1 && j < rightPortraitX2);

                            if (inLeftPortrait || inRightPortrait)
                            {
                                continue;
                            }

                            unsigned char pixel = logicalScreen[i * 320 + j];
                            if (pixel != 0)
                            {
                                uiLayer[i * 320 + j] = pixel;
                            }
                        }
                    }
                }

                osystem_CopyBlockPhys((unsigned char*)logicalScreen, 0, 0, 320, 200);
                notifyTTFMenuSelectionChanged();

                while (JoyD != 0)
                {
                    process_events();
                }
            }

            if (JoyD & 8) // right
            {
                choice = 1;
                g_portraitOverlayChoice = 1;
                FastCopyScreen(aux2, logicalScreen);
                AffBigCadre(240, 100, 160, 200);
                CopyBox_Aux_Log(170, 10, 309, 190);

                // When using HD backgrounds, copy only the frame border to UI layer
                if (usingHDBackground)
                {
                    uiLayer.fill(0);

                    // Always exclude both character portrait regions
                    int leftPortraitX1 = 10, leftPortraitY1 = 10;
                    int leftPortraitX2 = 149, leftPortraitY2 = 190;
                    int rightPortraitX1 = 170, rightPortraitY1 = 10;
                    int rightPortraitX2 = 309, rightPortraitY2 = 190;

                    for (int i = 0; i < 200; i++)
                    {
                        for (int j = 0; j < 320; j++)
                        {
                            // Skip both character portrait regions
                            bool inLeftPortrait = (i >= leftPortraitY1 && i < leftPortraitY2 && j >= leftPortraitX1 && j < leftPortraitX2);
                            bool inRightPortrait = (i >= rightPortraitY1 && i < rightPortraitY2 && j >= rightPortraitX1 && j < rightPortraitX2);

                            if (inLeftPortrait || inRightPortrait)
                            {
                                continue;
                            }

                            unsigned char pixel = logicalScreen[i * 320 + j];
                            if (pixel != 0)
                            {
                                uiLayer[i * 320 + j] = pixel;
                            }
                        }
                    }
                }

                osystem_CopyBlockPhys((unsigned char*)logicalScreen, 0, 0, 320, 200);
                notifyTTFMenuSelectionChanged();

                while (JoyD != 0)
                {
                    process_events();
                }
            }

            if (localKey == 1)
            {
                // Clear UI layer and reset HD background when leaving via escape
                setCurrentAnimatedHDBackground(nullptr);
                uiLayer.fill(0);
                g_portraitOverlayChoice = -1;
                notifyTTFMenuStateChanged(false, false);
                if (g_currentBackgroundIsHD)
                {
                    recreateBackgroundTexture(320, 200);
                }
                InitCopyBox(aux2, logicalScreen);
                FadeOutPhys(0x40, 0);
                return(-1);
            }
        }

        FadeOutPhys(0x40, 0);
        turnPageFlag = 0;

        switch (choice)
        {
            case 0:
            {
                // Try HD replacement for character intro reading background
                HDBackgroundInfo* hdBgIntro = loadHDBackground("ITD_RESS", AITD1_FOND_INTRO);
                bool usingHDIntro = (hdBgIntro != nullptr);
                if (hdBgIntro)
                {
                    updateBackgroundTextureHD(hdBgIntro->data, hdBgIntro->width, hdBgIntro->height, hdBgIntro->channels);
                    if (hdBgIntro->isAnimated)
                        setCurrentAnimatedHDBackground(hdBgIntro);
                    else
                        freeHDBackground(hdBgIntro);
                }
                else if (g_currentBackgroundIsHD)
                {
                    recreateBackgroundTexture(320, 200);
                }

                FastCopyScreen(frontBuffer, logicalScreen);
                SetClip(0, 0, 319, 199);
                LoadPak("ITD_RESS", AITD1_FOND_INTRO, aux);

                // Only copy low-res character portrait if not using HD backgrounds
                if (!usingHDIntro)
                {
                    CopyBox_Aux_Log(160, 0, 319, 199);
                }
                else
                {
                    // Clear text side (right half) so only portrait remains for uiLayer overlay
                    for (int y = 0; y < 200; y++)
                        memset(&logicalScreen[y * 320 + 160], 0, 160);
                }

                FastCopyScreen(logicalScreen, aux);
                g_portraitOverlayChoice = 0;
                // Play character intro VO (uses text index as VOC index)
                osystem_playVocByIndex(CVars[getCVarsIdx(INTRO_HERITIERE)]);
                Lire(CVars[getCVarsIdx(INTRO_HERITIERE)] + 1, 165, 5, 314, 194, 2, 15, 0);
                osystem_stopVO();
                CVars[getCVarsIdx(CHOOSE_PERSO)] = 1;
                break;
            }
            case 1:
            {
                // Try HD replacement for character intro reading background
                HDBackgroundInfo* hdBgIntro = loadHDBackground("ITD_RESS", AITD1_FOND_INTRO);
                bool usingHDIntro = (hdBgIntro != nullptr);
                if (hdBgIntro)
                {
                    updateBackgroundTextureHD(hdBgIntro->data, hdBgIntro->width, hdBgIntro->height, hdBgIntro->channels);
                    if (hdBgIntro->isAnimated)
                        setCurrentAnimatedHDBackground(hdBgIntro);
                    else
                        freeHDBackground(hdBgIntro);
                }
                else if (g_currentBackgroundIsHD)
                {
                    recreateBackgroundTexture(320, 200);
                }

                FastCopyScreen(frontBuffer, logicalScreen);
                SetClip(0, 0, 319, 199);
                LoadPak("ITD_RESS", AITD1_FOND_INTRO, aux);

                // Only copy low-res character portrait if not using HD backgrounds
                if (!usingHDIntro)
                {
                    CopyBox_Aux_Log(0, 0, 159, 199);
                }
                else
                {
                    // Clear text side (left half) so only portrait remains for uiLayer overlay
                    for (int y = 0; y < 200; y++)
                        memset(&logicalScreen[y * 320], 0, 160);
                }

                FastCopyScreen(logicalScreen, aux);
                g_portraitOverlayChoice = 1;
                // Play character intro VO (uses text index as VOC index)
                osystem_playVocByIndex(CVars[getCVarsIdx(INTRO_DETECTIVE)]);
                Lire(CVars[getCVarsIdx(INTRO_DETECTIVE)] + 1, 5, 5, 154, 194, 2, 15, 0);
                osystem_stopVO();
                CVars[getCVarsIdx(CHOOSE_PERSO)] = 0;
                break;
            }
        }

        g_portraitOverlayChoice = -1;

        if (localKey == 0x1C)
        {
            choiceMade = 1;
        }

    }

    // Clear UI layer and reset HD background when leaving character selection
    setCurrentAnimatedHDBackground(nullptr);
    uiLayer.fill(0);
    g_portraitOverlayChoice = -1;
    notifyTTFMenuStateChanged(false, false);
    if (g_currentBackgroundIsHD)
    {
        recreateBackgroundTexture(320, 200);
    }

    // Show "Please Wait..." loading screen immediately instead of slow fade-out
    // This gives instant feedback when the user confirms their character choice
    memset(logicalScreen, 0, 320 * 200);
    memset(frontBuffer, 0, 320 * 200);
    clearTTFTextQueue();

    // Update the foreground texture to black
    osystem_CopyBlockPhys((unsigned char*)frontBuffer, 0, 0, 320, 200);

    // Draw loading text centered on screen, translated per language
    const char* loadingText = "Please Wait...";
    if (languageNameString == "FRANCAIS")
        loadingText = "Veuillez Patienter...";
    else if (languageNameString == "ITALIANO")
        loadingText = "Attendere Prego...";
    else if (languageNameString == "ESPAGNOL")
        loadingText = "Por Favor Espere...";
    else if (languageNameString == "DEUTSCH")
        loadingText = "Bitte Warten...";

    SetFont(PtrFont, 15);
    int textWidth = ExtGetSizeFont((u8*)loadingText);
    int textX = (320 - textWidth) / 2;
    int textY = 92;
    PrintFont(textX, textY, logicalScreen, (u8*)loadingText);

    // Redraw background (now black) and present the loading screen
    osystem_drawBackground();
    process_events();

    InitCopyBox(aux2, logicalScreen);
    return(choice);
}

void startAITD1()
{
    fontHeight = 16;
    g_gameUseCDA = true;
    setPalette(currentGamePalette);

#ifndef AITD_UE4
    // Disable post-processing during tatou/credits (2D overlay screens)
    bool savedBloom = g_remasterConfig.postProcessing.enableBloom;
    bool savedFilmGrain = g_remasterConfig.postProcessing.enableFilmGrain;
    bool savedSSAO = g_remasterConfig.postProcessing.enableSSAO;
    g_remasterConfig.postProcessing.enableBloom = false;
    g_remasterConfig.postProcessing.enableFilmGrain = false;
    g_remasterConfig.postProcessing.enableSSAO = false;

    if (!make3dTatou())
    {
        // After tatou animation, ensure we're ready for HD backgrounds in intro screens
        // The tatou may have changed rendering state, so ensure proper initialization
        makeIntroScreens();
    }

    // Restore post-processing before startup menu
    g_remasterConfig.postProcessing.enableBloom = savedBloom;
    g_remasterConfig.postProcessing.enableFilmGrain = savedFilmGrain;
    g_remasterConfig.postProcessing.enableSSAO = savedSSAO;
#endif

    // Reset HD background from tatou before language selection
    if (g_currentBackgroundIsHD)
    {
        recreateBackgroundTexture(320, 200);
    }

    // Show language selection before main menu (skipped if only one language available)
    LanguageSelectionMenu();

    while (1)
    {
        // Reset HD backgrounds before showing main menu
        // (ensures UI is not covered by HD background after game over or returning from game)
        if (g_currentBackgroundIsHD)
        {
            recreateBackgroundTexture(320, 200);
        }

#ifndef AITD_UE4
        int startupMenuResult = MainMenu();
#else
        int startupMenuResult = 0;
#endif
        switch (startupMenuResult)
        {
        case -1: // timeout
        {
            CVars[getCVarsIdx(CHOOSE_PERSO)] = rand() & 1;
            startGame(7, 1, 0);

            // Disable post-processing during tatou/credits
            bool savedBloom2 = g_remasterConfig.postProcessing.enableBloom;
            bool savedFilmGrain2 = g_remasterConfig.postProcessing.enableFilmGrain;
            bool savedSSAO2 = g_remasterConfig.postProcessing.enableSSAO;
            g_remasterConfig.postProcessing.enableBloom = false;
            g_remasterConfig.postProcessing.enableFilmGrain = false;
            g_remasterConfig.postProcessing.enableSSAO = false;

            if (!make3dTatou())
            {
                if (!makeIntroScreens())
                {
                    //makeSlideshow();
                }
            }

            // Restore post-processing before returning to menu
            g_remasterConfig.postProcessing.enableBloom = savedBloom2;
            g_remasterConfig.postProcessing.enableFilmGrain = savedFilmGrain2;
            g_remasterConfig.postProcessing.enableSSAO = savedSSAO2;

            break;
        }
        case 0: // new game
        {
            // here, original would ask for protection

#if !TARGET_OS_IOS && !AITD_UE4
            if(ChoosePerso()!=-1)
#endif
            {
                process_events();
                while (key)
                {
                    process_events();
                }

#if !TARGET_OS_IOS
                startGame(7, 1, 0);
#endif

                // here, original would quit if protection flag was false

                startGame(0, 0, 1);
            }

            break;
        }
        case 1: // continue
        {
            // here, original would ask for protection

            if (restoreSave(12, 0))
            {
                // here, original would quit if protection flag was false

                updateShaking();

                FlagInitView = 2;

                InitView();

                PlayWorld(1, 1);

                //          freeScene();

                FadeOutPhys(8, 0);
            }

            break;
        }
        case 2: // exit
        {
            freeAll();
            exit(-1);

            break;
        }
        }
    }
}

void AITD1_ReadBook(int index, int type, int vocIndex)
{
    int resIdx;
    switch (type)
    {
    case 0: resIdx = AITD1_LETTRE; break;
    case 1: resIdx = AITD1_LIVRE; break;
    case 2: resIdx = AITD1_CARNET; break;
    default: assert(0); return;
    }

    // Try HD replacement for book/letter background
    HDBackgroundInfo* hdBg = loadHDBackground("ITD_RESS", resIdx);
    if (hdBg)
    {
        updateBackgroundTextureHD(hdBg->data, hdBg->width, hdBg->height, hdBg->channels);
        if (hdBg->isAnimated)
            setCurrentAnimatedHDBackground(hdBg);
        else
            freeHDBackground(hdBg);
    }
    else if (g_currentBackgroundIsHD)
    {
        recreateBackgroundTexture(320, 200);
    }

    LoadPak("ITD_RESS", resIdx, aux);

    // Start voice-over for AITD1 CD reading (plays while user reads text)
    if (vocIndex >= 0)
        osystem_playVocByIndex(vocIndex);

    switch (type)
    {
    case 0: // READ_MESSAGE
        turnPageFlag = 0;
        Lire(index, 60, 10, 245, 190, 0, 26, 0);
        break;
    case 1: // READ_BOOK
        turnPageFlag = 1;
        Lire(index, 48, 2, 260, 197, 0, 26, 0);
        break;
    case 2: // READ_CARNET
        turnPageFlag = 0;
        Lire(index, 50, 20, 250, 199, 0, 26, 0);
        break;
    }

    // Stop voice-over when reading ends
    if (vocIndex >= 0)
        osystem_stopVO();

    setCurrentAnimatedHDBackground(nullptr);
}
