///////////////////////////////////////////////////////////////////////////////
// Alone In The Dark Re-Haunted
// Copyright (C) 2026 Infogrames / Spacefarer Retro Remasters LLC
// Based on FITD by yaz0r, Re-haunted is released under GPL
// Author: Jake Jackson (jake@spacefarergames.com)
//
// Main game loop, frame processing, and event dispatch
///////////////////////////////////////////////////////////////////////////////

#include "common.h"
#include "consoleLog.h"
#include "anim2d.h"
#include "fontTTF.h"
#include "hdBackgroundRenderer.h"

#ifndef WIN32
#include <sys/time.h>
#endif

int mainLoopSwitch = 0;

void updatePendingEvents(void)
{
    // TODO: miss pending events here

    if(currentMusic!=-1)
    {
        if(currentMusic==-2)
        {
            if(evalChrono(&musicChrono)>180)
            {
                playMusic(nextMusic);
            }
        }
        else
        {
			/*
            if(fadeMusic(0,0,0x10)==-1)
            {
                currentMusic = -1;

                if(nextMusic != -1)
                {
                    playMusic(nextMusic);
                    nextMusic = -1;
                }
            }
			*/
        }
    }
}

// Fix for player getting stuck in hard collision zones after floor/room transitions
// Checks if player's bounding box overlaps any wall and tries to push them out
static void fixPlayerStuckInWall()
{
    if (currentCameraTargetActor < 0)
        return;

    tObject* player = &ListObjets[currentCameraTargetActor];
    if (player->room < 0 || player->room >= (int)roomDataTable.size())
        return;

    ZVStruct testZv = player->zv;
    if (AsmCheckListCol(&testZv, &roomDataTable[player->room]) == 0)
        return; // not stuck

    printf(MLOOP_WARN "Player stuck in wall at (%d, %d, %d) on floor %d camera %d, attempting warp..." CON_RESET "\n",
           player->roomX, player->roomY, player->roomZ, g_currentFloor, NumCamera);

    // Try incremental offsets in cardinal directions to find clear position
    static const int dirs[][2] = { {1,0}, {-1,0}, {0,1}, {0,-1}, {1,1}, {-1,1}, {1,-1}, {-1,-1} };
    for (int offset = 100; offset <= 1000; offset += 100)
    {
        for (int d = 0; d < 8; d++)
        {
            ZVStruct tryZv = player->zv;
            int dx = dirs[d][0] * offset;
            int dz = dirs[d][1] * offset;
            tryZv.ZVX1 += dx; tryZv.ZVX2 += dx;
            tryZv.ZVZ1 += dz; tryZv.ZVZ2 += dz;

            if (AsmCheckListCol(&tryZv, &roomDataTable[player->room]) == 0)
            {
                player->roomX += (s16)dx;
                player->roomZ += (s16)dz;
                player->worldX += (s16)dx;
                player->worldZ += (s16)dz;
                player->zv = tryZv;
                player->stepX = 0;
                player->stepZ = 0;
                printf(MLOOP_TAG "Player warped by (%d, %d) to (%d, %d, %d)\n",
                       dx, dz, player->roomX, player->roomY, player->roomZ);
                return;
            }
        }
    }
    printf(MLOOP_WARN "Could not find walkable position for stuck player" CON_RESET "\n");
}

void PlayWorld(int allowSystemMenu, int deltaTime)
{
    bool bLoop = true;

    while(bLoop)
    {
		process_events();
        
        localKey = key;
        localJoyD = JoyD;
        localClick = Click;

        if(localKey)
        {
            if(localKey == 0x1B)
            {
                // Freeze the scene snapshot NOW, before the ESC-draining loop.
                // The snapshot still holds the last game frame (with 3D objects).
                // Without this, each process_events() in the loop would overwrite
                // the snapshot with a cleared framebuffer (no 3D objects).
                osystem_captureScenePreview();

                while(key==0x1B)
                {
                    process_events();
                }

                // Pause HD background animation when entering menu
                pauseCurrentAnimatedHDBackground();

                // Notify TTF that we're entering the menu
                notifyTTFMenuStateChanged(true, true);

                processSystemMenu();

                // Notify TTF that we're exiting the menu
                notifyTTFMenuStateChanged(false, false);

                // Resume HD background animation when exiting menu
                resumeCurrentAnimatedHDBackground();

                while(key==0x1B || key == 0x1C)
                {
                    process_events();
                    localKey = key;
                }
            }

            if(localKey == 0x1C || localKey == 0x17)
            {
                if(allowSystemMenu == 0)
                {
                    break;
                }

                if(statusScreenAllowed)
                {
                    // Pause HD background animation when entering inventory
                    pauseCurrentAnimatedHDBackground();

                    // Notify TTF that we're entering the inventory/status screen
                    notifyTTFMenuStateChanged(true, true);

                    processInventory();

                    // Notify TTF that we're exiting the inventory
                    notifyTTFMenuStateChanged(false, false);

                    // Resume HD background animation when exiting inventory
                    resumeCurrentAnimatedHDBackground();
                }
            }
        }
        else
        {
            //      input5 = 0;
        }

        if(localClick)
        {
            if(!allowSystemMenu)
            {
                break;
            }

            action = 0x2000;
        }
        else
        {
            action = 0;
        }

        executeFoundLife(inHandTable[currentInventory]);

        if(FlagChangeEtage == 0)
        {
            if(g_gameId == AITD1)
            {
                if(CVars[getCVarsIdx(LIGHT_OBJECT)] == -1)
                {
                    //        mainVar2 = 2000;
                    //        mainVar3 = 2000;
                }
            }

            for(currentProcessedActorIdx = 0; currentProcessedActorIdx < NUM_MAX_OBJECT; currentProcessedActorIdx++)
            {
                currentProcessedActorPtr = &ListObjets[currentProcessedActorIdx];
                if(currentProcessedActorPtr->indexInWorld >= 0)
                {
                    currentProcessedActorPtr->COL_BY = -1;
                    currentProcessedActorPtr->HIT_BY = -1;
                    currentProcessedActorPtr->HIT = -1;
                    currentProcessedActorPtr->HARD_DEC = -1;
                    currentProcessedActorPtr->HARD_COL = -1;
                }
            }

            for(currentProcessedActorIdx = 0; currentProcessedActorIdx < NUM_MAX_OBJECT; currentProcessedActorIdx++)
            {
                currentProcessedActorPtr = &ListObjets[currentProcessedActorIdx];
                if(currentProcessedActorPtr->indexInWorld >= 0)
                {
                    int flag = currentProcessedActorPtr->objectType;

                    if((flag & AF_ANIMATED) || (g_gameId >= AITD2 && flag & 0x200))
                    {
                        GereAnim();
                    }
                    if(flag & AF_TRIGGER)
                    {
                        GereDec();
                    }

                    if(currentProcessedActorPtr->animActionType)
                    {
                        GereFrappe();
                    }
                }
            }

            
            for(currentProcessedActorIdx = 0; currentProcessedActorIdx < NUM_MAX_OBJECT; currentProcessedActorIdx++)
            {
                currentProcessedActorPtr = &ListObjets[currentProcessedActorIdx];
                if(currentProcessedActorPtr->indexInWorld >= 0)
                {
                    if(currentProcessedActorPtr->life != -1)
                    {
                        switch(g_gameId)
                        {
                        case AITD2:
                        case AITD3:
                        case TIMEGATE:
                            {
                                if(currentProcessedActorPtr->lifeMode&3)
                                    if(!(currentProcessedActorPtr->lifeMode&4))
                                        processLife(currentProcessedActorPtr->life, false);
                                break;
                            }
						case JACK:
                        case AITD1:
                            {
                                if(currentProcessedActorPtr->life != -1)
                                    if(currentProcessedActorPtr->lifeMode != -1)
                                        processLife(currentProcessedActorPtr->life, false);
                                break;
                            }
                        }
                    }
                }

                if(FlagChangeEtage)
                    break;
            }

            if(FlagGameOver)
            {
                // Pause animated HD background when game over screen appears
                pauseCurrentAnimatedHDBackground();
                break;
            }
        }

        if(FlagChangeEtage)
        {
            LoadEtage(NewNumEtage);
        }

        if(FlagChangeSalle)
        {
			ChangeSalle(NewNumSalle);
            InitView();

            // Fix: warp player out of walls after floor transition to CAMERA05_000 (caves)
            if (g_currentFloor == 5 && NumCamera == 0)
            {
                fixPlayerStuckInWall();
            }

            continue;
        }

        GereSwitchCamera();

        // Handle 2d objects
        //if (g_gameId >= AITD2) // no need for this test since it would do anything if there are 2d objects
        {
            int memocam = NumCamera;

            NumCamera = NewNumCamera;

            for (currentProcessedActorIdx = 0; currentProcessedActorIdx < NUM_MAX_OBJECT; currentProcessedActorIdx++)
            {
                currentProcessedActorPtr = &ListObjets[currentProcessedActorIdx];
                if (currentProcessedActorPtr->indexInWorld >= 0)
                {
                    if (currentProcessedActorPtr->life != -1)
                    {
                        if (currentProcessedActorPtr->objectType & AF_OBJ_2D)
                        {
                            if (currentProcessedActorPtr->lifeMode & 3)
                                if (!(currentProcessedActorPtr->lifeMode & 4))
                                {
                                    processLife(currentProcessedActorPtr->life, false);
                                    FlagGenereAffList = 1;
                                }
                        }
                    }
                }

                if (FlagChangeEtage)
                    break;
            }

            if (FlagGameOver)
            {
                // Pause animated HD background when game over screen appears
                pauseCurrentAnimatedHDBackground();
                break;
            }

            NumCamera = memocam;
        }
        if (FlagInitView
#ifdef FITD_DEBUGGER
            || debuggerVar_topCamera
#endif
            )
        {
            InitView();
        }

        //    if(FlagGenereActiveList)
        {
            GenereActiveList();
        } 

        //    if(actorTurnedToObj)
        {
            GenereAffList();
        }

        sortActorList();

        handleAnim2d();

        //    if(FlagRefreshAux2)
        {
            //      setupCameraSub4();
        }

        //    mainLoopSub1();

        //osystem_delay(100);

        updateShaking();

        AllRedraw(flagRedraw);

        updatePendingEvents();
    }

    shakeVar1 = 0;
    shakingAmplitude = 0;

    stopShaking();
    //  stopSounds();
}

