///////////////////////////////////////////////////////////////////////////////
// Alone In The Dark Re-Haunted
// Copyright (C) 2026 Infogrames / Spacefarer Retro Remasters LLC
// Based on FITD by yaz0r, Re-haunted is released under GPL
// Author: Jake Jackson (jake@spacefarergames.com)
//
// FMV cutscene sequence player
///////////////////////////////////////////////////////////////////////////////

#include "common.h"
#include "consoleLog.h"
#include "configRemaster.h"

// stb_image_write for sequence frame dumping (static to avoid linker conflicts)
#define STB_IMAGE_WRITE_STATIC
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "../ThirdParty/bgfx.cmake/bimg/3rdparty/stb/stb_image_write.h"

// stb_image for loading HD replacement sequence frames
#define STB_IMAGE_STATIC
#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_PNG
#include "../ThirdParty/bgfx.cmake/bimg/3rdparty/stb/stb_image.h"

#ifdef _WIN32
#include <direct.h>
#else
#include <sys/stat.h>
#endif

const char* sequenceListAITD2[]=
{
    "BATL",
    "GRAP",
    "CLE1",
    "CLE2",
    "COOK",
    "EXPL",
    "FALA",
    "FAL2",
    "GLIS",
    "GREN",
    "JEND",
    "MANI",
    "MER",
    "TORD",
    "PANT",
    "VERE",
    "PL21",
    "PL22",
    "ENDX",
    "SORT",
    "EFER",
    "STAR",
    "MEDU",
    "PROL",
    "GRAS",
    "STRI",
    "ITRO",
    "BILL",
    "PIRA",
    "PIR2",
    "VENT",
    "FIN",
    "LAST"
};

void unapckSequenceFrame(unsigned char* source,unsigned char* dest)
{
    unsigned char byteCode;

    byteCode = *(source++);

    while(byteCode)
    {
        if(!(--byteCode)) // change pixel or skip pixel
        {
            unsigned char changeColor;

            changeColor = *(source++);

            if(changeColor)
            {
                *(dest++) = changeColor;
            }
            else
            {
                dest++;
            }
        }
        else
            if(!(--byteCode)) // change 2 pixels or skip 2 pixels
            {
                unsigned char changeColor;

                changeColor = *(source++);

                if(changeColor)
                {
                    *(dest++) = changeColor;
                    *(dest++) = changeColor;
                }
                else
                {
                    dest+=2;
                }
            }
            else
                if(!(--byteCode)) // fill or skip
                {
                    unsigned char size;
                    unsigned char fillColor;

                    size = *(source++);
                    fillColor = *(source++);

                    if(fillColor)
                    {
                        int i;

                        for(i=0;i<size;i++)
                        {
                            *(dest++) = fillColor;
                        }
                    }
                    else
                    {
                        dest+=size;
                    }
                }
                else // large fill of skip
                {
                    u16 size;
                    unsigned char fillColor;

                    size = READ_LE_U16(source);
                    source+=2;
                    fillColor = *(source++);

                    if(fillColor)
                    {
                        int i;

                        for(i=0;i<size;i++)
                        {
                            *(dest++) = fillColor;
                        }
                    }
                    else
                    {
                        dest+=size;
                    }
                }

                byteCode = *(source++);
    }
}

extern "C" { extern char homePath[512]; }

static void ensureSequenceDirectory(const char* dir)
{
#ifdef _WIN32
    _mkdir(dir);
#else
    mkdir(dir, 0755);
#endif
}

static void dumpSequenceFrame(const char* seqName, int frameId, const char* auxBuffer, const palette_t& palette)
{
    char dirPath[512];
    snprintf(dirPath, sizeof(dirPath), "%ssequences_dump", homePath);
    ensureSequenceDirectory(dirPath);

    snprintf(dirPath, sizeof(dirPath), "%ssequences_dump/%s", homePath, seqName);
    ensureSequenceDirectory(dirPath);

    char filePath[512];
    snprintf(filePath, sizeof(filePath), "%ssequences_dump/%s/%s_%04d.png", homePath, seqName, seqName, frameId);

    // Convert paletted 320x200 aux buffer to RGBA
    unsigned char* rgba = (unsigned char*)malloc(320 * 200 * 4);
    if (!rgba) return;

    for (int i = 0; i < 320 * 200; i++)
    {
        unsigned char idx = (unsigned char)auxBuffer[i];
        rgba[i * 4 + 0] = palette[idx][0];
        rgba[i * 4 + 1] = palette[idx][1];
        rgba[i * 4 + 2] = palette[idx][2];
        rgba[i * 4 + 3] = 255;
    }

    if (stbi_write_png(filePath, 320, 200, 4, rgba, 320 * 4))
    {
        printf(SEQ_OK "Dumped sequence frame: %s" CON_RESET "\n", filePath);
    }
    else
    {
        printf(SEQ_ERR "Failed to dump sequence frame: %s" CON_RESET "\n", filePath);
    }

    free(rgba);
}

static unsigned char* tryLoadHDSequenceFrame(const char* seqName, int frameId, int* outWidth, int* outHeight)
{
    char filePath[512];
    snprintf(filePath, sizeof(filePath), "%ssequences_hd/%s/%s_%04d.png", homePath, seqName, seqName, frameId);

    int channels = 0;
    unsigned char* data = stbi_load(filePath, outWidth, outHeight, &channels, 4);
    if (data)
    {
        printf(SEQ_OK "Loaded HD sequence frame: %s (%dx%d)" CON_RESET "\n", filePath, *outWidth, *outHeight);
    }
    return data;
}

void playSequence(int sequenceIdx, int fadeStart, int fadeOutVar)
{

    int frames=0;                   /* Number of frames displayed */

    int var_4 = 1;
    int quitPlayback = 0;
    int nextFrame = 1;
    palette_t seqPalette;           // Palette at outer scope for dump/load access

    char buffer[256];
    if (g_gameId == AITD2)
    {
        strcpy(buffer, sequenceListAITD2[sequenceIdx]);
    }
    else if (g_gameId == AITD3)
    {
        sprintf(buffer, "AN%d", sequenceIdx);
    }

    printf(SEQ_TAG "Playing sequence: %s" CON_RESET "\n", buffer);

    int numMaxFrames = PAK_getNumFiles(buffer);

    while(!quitPlayback)
    {
        int currentFrameId = 0;
        int sequenceParamIdx;

        while(currentFrameId < nextFrame)
        {
            frames++;

            timer = timeGlobal;

            if (currentFrameId >= numMaxFrames)
            {
                quitPlayback = 1;
                break;
            }

            if(!LoadPak(buffer,currentFrameId,logicalScreen))
            {
                fatalError(0,buffer);
            }

            if(!currentFrameId) // first frame
            {
                copyPalette(logicalScreen, seqPalette);  // copy palette
                memcpy(aux,logicalScreen+0x300,64000);
                nextFrame = READ_LE_U16(logicalScreen+64768);

                convertPaletteIfRequired(seqPalette);

                if(var_4 != 0)
                {
                    /*      if(fadeStart & 1)
                    {
                    fadeOut(0x10,0);
                    }
                    if(fadeStart & 4)
                    {
                    //memset(palette,0,0); // fade from black
                    fadeInSub1(seqPalette);
                    flipOtherPalette(palette);
                    } */

                    osystem_setPalette(&seqPalette);
                    copyPalette(seqPalette,currentGamePalette);
                }
            }
            else // not first frame
            {
                U32 frameSize;

                frameSize = READ_LE_U32(logicalScreen);

                if(frameSize < 64000) // key frame
                {
                    unapckSequenceFrame((unsigned char*)logicalScreen+4,(unsigned char*)aux);
                }
                else // delta frame
                {
                    FastCopyScreen(logicalScreen,aux);
                }
            }

            for(sequenceParamIdx = 0; sequenceParamIdx < numSequenceParam; sequenceParamIdx++)
            {
                if(sequenceParams[sequenceParamIdx].frame == currentFrameId)
                {
                    playSound(sequenceParams[sequenceParamIdx].sample);
                }
            }

			// TODO: here, timming management
			// TODO: fade management

			// Dump decoded frame to PNG if enabled
			if (g_remasterConfig.sequences.dumpEnabled)
			{
				dumpSequenceFrame(buffer, currentFrameId, aux, seqPalette);
			}

			// Try loading HD replacement frame
			bool usedHDFrame = false;
			if (g_remasterConfig.sequences.loadEnabled)
			{
				int hdW = 0, hdH = 0;
				unsigned char* hdFrame = tryLoadHDSequenceFrame(buffer, currentFrameId, &hdW, &hdH);
				if (hdFrame)
				{
					osystem_setSequenceHDFrame(hdFrame, hdW, hdH);
					stbi_image_free(hdFrame);
					usedHDFrame = true;
				}
			}

			if (!usedHDFrame)
			{
				osystem_CopyBlockPhys((unsigned char*)aux,0,0,320,200);
			}

			osystem_drawBackground();

            currentFrameId++;

			for(int i=0;i<5;i++) // display the frame 5 times (original seems to wait 5 sync)
			{
				process_events();
			}

			if(key)
			{
				//stopSample();
				quitPlayback = 1;
				break;
			}

        }

        fadeOutVar--;

        if(fadeOutVar==0)
        {
            quitPlayback=1;
        }
    }

	osystem_clearSequenceHDFrame();
	FlagInitView = 2;
}
