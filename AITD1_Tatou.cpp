///////////////////////////////////////////////////////////////////////////////
// Alone In The Dark Re-Haunted
// Copyright (C) 2026 Infogrames / Spacefarer Retro Remasters LLC
// Based on FITD by yaz0r, Re-haunted is released under GPL
// Author: Jake Jackson (jake@spacefarergames.com)
//
// Alone in the Dark 1 status screen and inventory display
///////////////////////////////////////////////////////////////////////////////

#include "common.h"

#include "AITD1.h"

#include "hdBackground.h"
#include "hdBackgroundRenderer.h"

void clearScreenTatou(void)
{
	for(int i=0;i<45120;i++)
	{
		frontBuffer[i] = 0;
	}
}

int make3dTatou(void)
{
    int zoom;
    int deltaTime;
    int beta;
    int alpha;
    unsigned int localChrono;
    palette_t tatouPal;
    palette_t paletteBackup;

    // Stop any CD audio before loading resources. On mixed-mode CDs the drive
    // cannot read data while an audio track is playing, which would cause
    // CheckLoadMallocPak to fail and crash.
    osystem_stopTrack();
    currentMusic = -1;

    char* tatou2d = CheckLoadMallocPak("ITD_RESS",AITD1_TATOU_MCG);

    // Load HD background for initial 2D phase (ITD_RESS_002.png - with tatou in background)
    HDBackgroundInfo* hdBg = loadHDBackground("ITD_RESS", AITD1_TATOU_MCG);
    if (hdBg)
    {
        updateBackgroundTextureHD(hdBg->data, hdBg->width, hdBg->height, hdBg->channels);
        if (hdBg->isAnimated)
        {
            setCurrentAnimatedHDBackground(hdBg);
        }
        else
        {
            freeHDBackground(hdBg);
        }
    }

    char* tatou3dRaw = CheckLoadMallocPak("ITD_RESS", AITD1_TATOU_3DO);
    sBody* tatou3d = createBodyFromPtr(tatou3dRaw);

    char* tatouPalRaw = CheckLoadMallocPak("ITD_RESS",AITD1_TATOU_PAL);
    copyPalette(tatouPalRaw, tatouPal);

    zoom = 8920;
    deltaTime = 50;
    beta = 256;
    alpha = 8;

    SetProjection(160,100,128,500,490);

    copyPalette(currentGamePalette,paletteBackup);

    paletteFill(currentGamePalette,0,0,0);

    setPalette(currentGamePalette);

    copyPalette(tatouPal,currentGamePalette);
    FastCopyScreen(tatou2d+770,frontBuffer);
    FastCopyScreen(frontBuffer,aux2);

    osystem_CopyBlockPhys(frontBuffer,0,0,320,200);

    FadeInPhys(8,0);

    startChrono(&localChrono);

    do
    {
        process_events();

		//timeGlobal++;
		timer = timeGlobal;

        if(evalChrono(&localChrono)<=180) 
        {
            // before lightning strike
            if(key || Click)
            {
                break;
            }
        }
        else
        {
            // Lightning strike - prepare for 3D rendering
            // Switch to HD background now that 3D animation is starting
            HDBackgroundInfo* hdBg = loadHDBackground("ITD_RESS", AITD1_TATOU_MCG, "NOTATOU");
            if (hdBg)
            {
                updateBackgroundTextureHD(hdBg->data, hdBg->width, hdBg->height, hdBg->channels);
                if (hdBg->isAnimated)
                {
                    setCurrentAnimatedHDBackground(hdBg);
                }
                else
                {
                    freeHDBackground(hdBg);
                }
            }

            /*  LastSample = -1;
            LastPriority = -1; */

            playSound(CVars[getCVarsIdx(SAMPLE_TONNERRE)]);

            /*     LastSample = -1;
            LastPriority = -1;*/

            paletteFill(currentGamePalette,63,63,63);
            setPalette(currentGamePalette);
            /*  setClipSize(0,0,319,199);*/

            clearScreenTatou();

            setCameraTarget(0,0,0,alpha,beta,0,zoom);

            AffObjet(0,0,0,0,0,0,tatou3d);

            //blitScreenTatou();
            osystem_CopyBlockPhys((unsigned char*)frontBuffer,0,0,320,200);

			process_events();

            copyPalette(tatouPal,currentGamePalette);
            setPalette(currentGamePalette);
			osystem_CopyBlockPhys((unsigned char*)frontBuffer,0,0,320,200);


            while(key==0 && Click == 0 && JoyD == 0)
            {
                // Armadillo rotation loop

                process_events();

                zoom += deltaTime;

                if(zoom>16000)
                    break;

                beta -=8;

                clearScreenTatou();

                setCameraTarget(0,0,0,alpha,beta,0,zoom);

                AffObjet(0,0,0,0,0,0,tatou3d);

                //blitScreenTatou();

                osystem_stopFrame();
            }

            break;
        }
    }while(1);

    // Clean up animated HD background if it was set
    setCurrentAnimatedHDBackground(nullptr);

    free(tatou3dRaw);
    delete tatou3d;

    free(tatou2d);
    free(tatouPalRaw);

    if(key || Click || JoyD)
    {
        while(key)
        {
          process_events();
        }

        FadeOutPhys(32,0);
        copyPalette(paletteBackup,currentGamePalette);
        return(1);
    }
    else
    {
        FadeOutPhys(16,0);
        copyPalette(paletteBackup,currentGamePalette);
        return(0);
    }

    return(0);
}
