///////////////////////////////////////////////////////////////////////////////
// Alone In The Dark Re-Haunted
// Copyright (C) 2026 Infogrames / Spacefarer Retro Remasters LLC
// Based on FITD by yaz0r, Re-haunted is released under GPL
// Author: Jake Jackson (jake@spacefarergames.com)
//
// Status screen, inventory UI, and character sheet
///////////////////////////////////////////////////////////////////////////////

// seg 4

#include "common.h"

#ifdef PCLIKE
#include "SDL.h"
#endif


//////////// stuff to move


void blitScreenTatou(void)
{
	/*
    int i;

    for(i=0;i<45120;i++)
    {
        frontBuffer[i] = backbuffer[i];
    }
	*/
}

void FastCopyScreen(void* source, void* dest)
{
	memcpy(dest, source, 64000);
}

void FadeInPhys(int step,int start)
{
    SaveTimerAnim();

    if(fadeState == 2) // only used for the ending ?
    {
    }
    else
    {
        for(int i=0;i<256;i+=step)
        {
			process_events();
            palette_t localPalette;
            SetLevelDestPal(currentGamePalette,localPalette,i);
            setPalette(localPalette);
			osystem_refreshFrontTextureBuffer();
			osystem_drawBackground();
        }
    }

    fadeState = 1;

    RestoreTimerAnim();
}

void FadeOutPhys(int step, int var2)
{
    SaveTimerAnim();

    for(int i=256;i>=0;i-=step)
    {
		process_events();
        palette_t localPalette;
		SetLevelDestPal(currentGamePalette,localPalette,i);
		setPalette(localPalette);
		osystem_refreshFrontTextureBuffer();
		osystem_drawBackground();
    }

    RestoreTimerAnim();
}

#ifdef PCLIKE
void process_events( void )
{
    /* Our SDL event placeholder. */
    SDL_Event event;

    /* Grab all the events off the queue. */
    while( SDL_PollEvent( &event ) ) {

        switch( event.type ) {
        case SDL_KEYDOWN:
            /* Handle key presses. */
            //            handle_key_down( &event.key.keysym );
            break;
        case SDL_QUIT:
            /* Handle quit requests (like Ctrl-c). */
            exit( 0 );
            break;
        }

    }

}
#else
void process_events( void )
{
    osystem_endOfFrame();
	u32 timeIncrease = osystem_startOfFrame();
    assert(timeIncrease);
#ifdef FITD_DEBUGGER
	if(debuggerVar_fastForward)
	{
		timeGlobal+=8;
	}
	else
#endif
	{
        timeGlobal += timeIncrease;
	}
	timer=timeGlobal;
}
#endif

void startChrono(unsigned int* chrono)
{
    *chrono = timer;
}

int evalChrono(unsigned int* chrono)
{
    return(timer-*chrono);
}

// bp = x, bx = y, cx = z
// out
// xOut = dx, yOut = ax
void Rotate(unsigned int x, unsigned int y, unsigned int z, int* xOut, int* yOut)
{
    if(x)
    {
        int var1 = (((cosTable[(x+0x100)&0x3FF] * y) << 1) &0xFFFF0000) -  (((cosTable[x&0x3FF] *z) <<1) & 0xFFFF0000);
        int var2 = (((cosTable[x&0x3FF] * y) << 1) & 0xFFFF0000) + (((cosTable[(x+0x100)&0x3FF] *z) <<1) & 0xFFFF0000);

        *yOut = var1>>16;
        *xOut = var2>>16;
    }
    else
    {
        *xOut = z;
        *yOut = y;
    }
}

void setCameraTarget(int x,int y,int z,int alpha,int beta,int gamma,int time)
{
    int x1;
    int y1;
    int x2;
    int y2;

    Rotate(alpha+0x200,-time,0,&x1,&y1);
    Rotate(beta+0x200,y1,0,&x2,&y2);

    SetPosCamera(x2+x,-x1+y,y2+z);
    SetAngleCamera(alpha,beta,gamma);
}

void playSound(int num)
{
	if(num == -1)
		return;

	if(!soundEnabled)
		return;

	// Skip re-triggering the same sample if it is still playing.
	// Life script opcodes (LM_SAMPLE, LM_ANIM_SAMPLE) fire every game tick,
	// so without this guard the sound restarts from the beginning each frame.
	if(num == LastSample && osystem_isSamplePlaying())
		return;

	char sampleFileName[256] = "";
	if (g_gameId == TIMEGATE)
	{
		strcpy(sampleFileName, "SAMPLES");
	}
	else
	{
		strcpy(sampleFileName, "LISTSAMP");
	}

	int size = getPakSize(sampleFileName,num);
	if(!size)
		return;

	char* ptr = HQR_Get(listSamp,num);
	if(!ptr)
		return;

	osystem_playSample(ptr, size);
	LastSample = num;
}

// LM_SAMPLE_THEN / LM_SAMPLE_THEN_REPEAT: play a sample, then another once it has finished
// (e.g. LIFE 100: the library door, then the laugh). `next` with 0x4000 repeats.
// The original engine (INDARK.EXE) keeps `next` in DS:8375; its per-frame sound update
// 1693:0373, called from PlayWorld (132d:034b), plays it once no sample is playing, through
// the REP_SAMPLE routine when bit 0x4000 is set, and resets it to -1. FITD and ReHaunted only
// stored it (REHAUNTED_FIXES.md, "sound_then"). Sound effects overlap here, so the next sample
// waits for the first one's own voice instead of for silence.
static unsigned int s_sampleThenVoice = 0;

void playSoundThen(int num, int next)
{
	const unsigned int before = osystem_lastSampleVoice();
	playSound(num);
	const unsigned int after = osystem_lastSampleVoice();
	s_sampleThenVoice = (after != before) ? after : 0;   // not started (muted, already playing): no wait
	nextSample = next;
}

// Called every gameplay frame (mainLoop).
void processNextSample()
{
	if (nextSample == -1)
		return;
	if (osystem_isSampleVoicePlaying(s_sampleThenVoice))
		return;
	const int next = nextSample;
	nextSample = -1;
	s_sampleThenVoice = 0;
	if (next & 0x4000)
		playSoundLooping(next & ~0x4000);
	else
		playSound(next);
}

// A looping sample (LM_REP_SAMPLE, e.g. the talisman chimes of LIFE 186) belongs to the
// room it was started in. The scripts never stop one (no script uses LM_STOP_SAMPLE); the
// original DOS game stops it when the player leaves the room, not when the item is taken
// (tested in the original game under DOSBox).
static int s_lastLoopingSample = -1;
static int s_loopFloor = -1;
static int s_loopRoom = -1;

void stopLoopingSounds()
{
	osystem_stopLoopingSamples();
	s_lastLoopingSample = -1;
	s_loopFloor = s_loopRoom = -1;
}

// Called every gameplay frame.
void stopLoopingSoundsOnRoomChange()
{
	if (s_lastLoopingSample != -1 && (g_currentFloor != s_loopFloor || currentRoom != s_loopRoom))
		stopLoopingSounds();
}

void playSoundLooping(int num)
{
	if(num == -1)
		return;

	if(!soundEnabled)
		return;

	// Skip re-triggering the same looping sample if it is still playing.
	if(num == s_lastLoopingSample && osystem_isSamplePlaying())
		return;

	char sampleFileName[256] = "";
	if (g_gameId == TIMEGATE)
	{
		strcpy(sampleFileName, "SAMPLES");
	}
	else
	{
		strcpy(sampleFileName, "LISTSAMP");
	}

	int size = getPakSize(sampleFileName,num);
	if(!size)
		return;

	char* ptr = HQR_Get(listSamp,num);
	if(!ptr)
		return;

	osystem_playLoopingSample(ptr, size);
	s_lastLoopingSample = num;
	s_loopFloor = g_currentFloor;
	s_loopRoom = currentRoom;
}

////////////////////////

