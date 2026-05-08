///////////////////////////////////////////////////////////////////////////////
// Alone In The Dark Re-Haunted
// Copyright (C) 2026 Infogrames / Spacefarer Retro Remasters LLC
// Based on FITD by yaz0r, Re-haunted is released under GPL
// Author: Jake Jackson (jake@spacefarergames.com)
//
// Jack in the Dark game-specific logic and startup
///////////////////////////////////////////////////////////////////////////////

#include "common.h"

#include "hdBackground.h"
#include "hdBackgroundRenderer.h"

// ITD_RESS mapping
#define JACK_CADRE_SPF					0
#define JACK_ITDFONT					1
#define JACK_LIVRE						2
#define JACK_IM_EXT_JACK				3

void startJACK()
{
	fontHeight = 16; // TODO: check
	startGame(16,1,1);
}

void JACK_ReadBook(int index, int type)
{
	switch(type)
	{
	case 1: // READ_BOOK
		{
			unsigned char* pImage = (unsigned char*)loadPak("ITD_RESS", JACK_LIVRE);
			memcpy(aux, pImage, 320*200);
			palette_t lpalette;
			copyPalette(pImage + 320*200, lpalette);
			convertPaletteIfRequired(lpalette);
			copyPalette(lpalette,currentGamePalette);
			setPalette(lpalette);
			free(pImage);

			// Try HD replacement: JACKBOOK_000.png
			HDBackgroundInfo* hdBg = loadHDBackground("JACKBOOK", 0);
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

			turnPageFlag = 1;
			Lire(index, 60, 10, 245, 190, 0, 124, 124);

			setCurrentAnimatedHDBackground(nullptr);
			break;
		}
	default:
		assert(0);
	}
}