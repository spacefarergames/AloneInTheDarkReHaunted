///////////////////////////////////////////////////////////////////////////////
// Alone In The Dark Re-Haunted
// Copyright (C) 2026 Infogrames / Spacefarer Retro Remasters LLC
// Author: Jake Jackson (jake@spacefarergames.com)
//
// Game timing, chronometer, and frame rate management
///////////////////////////////////////////////////////////////////////////////

#include "common.h"

int timerSaved = false;
unsigned int timerSavedValue = 0;

void SaveTimerAnim(void)
{
    if(timerSaved==0)
    {
        timerSavedValue = timeGlobal;
    }
	timerSaved++;
}

void RestoreTimerAnim(void)
{
	assert(timerSaved);
	timerSaved--;
    if(timerSaved == 0)
    {
        timeGlobal = timerSavedValue;
		timer = timeGlobal;
    }
}
