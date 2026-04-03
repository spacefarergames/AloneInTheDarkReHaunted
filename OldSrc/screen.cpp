///////////////////////////////////////////////////////////////////////////////
// Alone In The Dark Re-Haunted
// Copyright (C) 2026 Infogrames / Spacefarer Retro Remasters LLC
// Based on FITD by yaz0r, Re-haunted is released under GPL
// Author: Jake Jackson (jake@spacefarergames.com)
//
// Screen buffer management and copy operations
///////////////////////////////////////////////////////////////////////////////

// seg 048

#include "common.h"

void setupScreen(void)
{
    logicalScreen = (char*)malloc(64800);

    screenBufferSize = 64800;

    unkScreenVar2 = 3;

    // TODO: remain of screen init

}

void flushScreen(void)
{
    for(int i=0;i<200;i++)
    {
        for(int j=0;j<320;j++)
        {
            *(uiLayer.data()+i*320+j) = 0;
            *(logicalScreen + i * 320 + j) = 0;
        }
    }
}
