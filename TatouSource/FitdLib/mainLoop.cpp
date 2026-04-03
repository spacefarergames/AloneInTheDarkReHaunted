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
#include "jobSystem.h"

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
    if (currentCameraTargetActor < 0 || currentCameraTargetActor >= NUM_MAX_OBJECT)
        return;

    tObject* player = &ListObjets[currentCameraTargetActor];
    if (player->indexInWorld < 0)
        return;

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

// Hide the rocking horse object on CAMERA00_003 to avoid visual artifacts.
// The object is restored automatically when the camera changes away.
static int s_hiddenRockingHorseActorIdx = -1;
static s16 s_savedRockingHorseBodyNum = -1;

static int getAbsoluteCameraIndex()
{
    if (NumCamera < 0 || currentRoom < 0 || currentRoom >= (int)roomDataTable.size())
        return -1;
    if (NumCamera >= (int)roomDataTable[currentRoom].cameraIdxTable.size())
        return -1;
    return roomDataTable[currentRoom].cameraIdxTable[NumCamera];
}

static void restoreRockingHorse()
{
    if (s_hiddenRockingHorseActorIdx >= 0 && s_hiddenRockingHorseActorIdx < NUM_MAX_OBJECT)
    {
        ListObjets[s_hiddenRockingHorseActorIdx].bodyNum = s_savedRockingHorseBodyNum;
        s_hiddenRockingHorseActorIdx = -1;
        s_savedRockingHorseBodyNum = -1;
    }
}

static void updateRockingHorseVisibility()
{
    bool shouldHide = (g_currentFloor == 0 && getAbsoluteCameraIndex() == 3);

    if (shouldHide)
    {
        if (s_hiddenRockingHorseActorIdx >= 0)
            return; // already hidden

        for (int i = 0; i < NUM_MAX_OBJECT; i++)
        {
            tObject* obj = &ListObjets[i];
            if (obj->indexInWorld < 0 || obj->bodyNum < 0)
                continue;

            int worldIdx = obj->indexInWorld;
            if (worldIdx < 0 || worldIdx >= (int)ListWorldObjets.size())
                continue;

            textEntryStruct* entry = getTextFromIdx(ListWorldObjets[worldIdx].foundName);
            if (entry && entry->textPtr)
            {
                if (_stricmp((const char*)entry->textPtr, "ROCKING HORSE") == 0)
                {
                    s_hiddenRockingHorseActorIdx = i;
                    s_savedRockingHorseBodyNum = obj->bodyNum;
                    obj->bodyNum = -1;
                    return;
                }
            }
        }
    }
    else
    {
        restoreRockingHorse();
    }
}

// Hotspot overlay detection - shows magnifying glass when near interactive objects
static float g_hotspotOverlayOpacity = 0.0f;
static const int HOTSPOT_INTERACTION_DISTANCE = 1000; // Units for interaction proximity
static const float HOTSPOT_FADE_SPEED = 4.0f; // ~0.25s full transition
static u32 s_lastHotspotUpdateTime = 0;

bool isHotspotOverlayVisible()
{
    return g_hotspotOverlayOpacity > 0.0f;
}

float getHotspotOverlayOpacity()
{
    return g_hotspotOverlayOpacity;
}

// Compute distance from a point to the nearest edge of a zone volume (XZ plane).
// For spawned actors the ZV is in world space (actor position + local ZV bounds).
// Returns 0 if the point is inside the ZV.
static int distanceToZV(int px, int pz, const ZVStruct& zv)
{
    int dx = 0;
    if (px < zv.ZVX1)
        dx = zv.ZVX1 - px;
    else if (px > zv.ZVX2)
        dx = px - zv.ZVX2;

    int dz = 0;
    if (pz < zv.ZVZ1)
        dz = zv.ZVZ1 - pz;
    else if (pz > zv.ZVZ2)
        dz = pz - zv.ZVZ2;

    // Manhattan distance (consistent with GiveDistance2D)
    return dx + dz;
}

static bool detectNearbyInteractiveHotspot()
{
    // Only detect during active gameplay
    if (currentCameraTargetActor < 0 || currentCameraTargetActor >= NUM_MAX_OBJECT)
        return false;

    tObject* player = &ListObjets[currentCameraTargetActor];
    if (player->indexInWorld < 0)
        return false;

    int playerX = player->worldX;
    int playerZ = player->worldZ;
    int playerRoom = player->room;

    static u32 s_lastDiagTime = 0;
    u32 now = SDL_GetTicks();
    bool doDiag = (now - s_lastDiagTime > 3000);
    if (doDiag) s_lastDiagTime = now;

    int candidateCount = 0;
    int closestDist = 999999;
    int closestIdx = -1;

    // Iterate all world objects (includes static interactable objects that are not spawned as actors)
    int numWorldObjects = (int)ListWorldObjets.size();
    for (int i = 0; i < numWorldObjects; i++)
    {
        tWorldObject* worldObj = &ListWorldObjets[i];

        // Skip objects not in the player's room
        if (worldObj->room != playerRoom)
            continue;

        // Skip objects already found/in inventory
        if (worldObj->foundFlag & 0x8000)
            continue;

        // Check if the object is interactive (has a found body, found life, or action flag)
        bool isInteractive = (worldObj->foundBody != -1) ||
                             (worldObj->foundLife != -1) ||
                             (worldObj->foundFlag & 0x4000);
        if (!isInteractive)
            continue;

        candidateCount++;

        // Compute distance: for spawned actors use the ZV bounding box (nearest edge),
        // otherwise fall back to point-to-point distance from the world object position.
        int distance;
        if (worldObj->objIndex != -1 && worldObj->objIndex < NUM_MAX_OBJECT)
        {
            tObject* actor = &ListObjets[worldObj->objIndex];
            if (actor->indexInWorld >= 0)
            {
                // Actor ZV is in world space already (updated each frame by the engine)
                distance = distanceToZV(playerX, playerZ, actor->zv);
            }
            else
            {
                distance = GiveDistance2D(playerX, playerZ, worldObj->x, worldObj->z);
            }
        }
        else
        {
            distance = GiveDistance2D(playerX, playerZ, worldObj->x, worldObj->z);
        }

        if (distance < closestDist)
        {
            closestDist = distance;
            closestIdx = i;
        }

        if (distance < HOTSPOT_INTERACTION_DISTANCE)
        {
            if (doDiag)
            {
                printf(MLOOP_TAG "Hotspot HIT: world %d dist=%d player=(%d,%d) room=%d\n",
                       i, distance, playerX, playerZ, playerRoom);
            }
            return true;
        }
    }

    // Check type 9 hard collision zones (searchable scenario trigger areas)
    if (playerRoom >= 0 && playerRoom < (int)roomDataTable.size())
    {
        roomDataStruct& roomData = roomDataTable[playerRoom];
        for (int i = 0; i < roomData.numHardCol; i++)
        {
            if (roomData.hardColTable[i].type != 9)
                continue;

            int dist = distanceToZV(playerX, playerZ, roomData.hardColTable[i].zv);
            if (dist < HOTSPOT_INTERACTION_DISTANCE)
            {
                if (doDiag)
                {
                    printf(MLOOP_TAG "Hotspot HIT: hardcol zone %d (type 9) dist=%d player=(%d,%d) room=%d\n",
                           i, dist, playerX, playerZ, playerRoom);
                }
                return true;
            }

            if (dist < closestDist)
            {
                closestDist = dist;
                closestIdx = 1000 + i; // offset to distinguish from world object indices
            }
        }
    }

    if (doDiag)
    {
        printf(MLOOP_TAG "Hotspot scan: room=%d player=(%d,%d) candidates=%d closest=%d (idx %d)\n",
               playerRoom, playerX, playerZ, candidateCount, closestDist, closestIdx);
    }

    return false;
}

void PlayWorld(int allowSystemMenu, int deltaTime)
{
    bool bLoop = true;

    while(bLoop)
    {
        // Process pending job callbacks from background threads
        JobSystem::instance().processPendingCallbacks();

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

            if(localKey == 0x0F) // TAB / SELECT button - open map
            {
                if(allowSystemMenu && statusScreenAllowed)
                {
                    while(key==0x0F)
                    {
                        process_events();
                    }

                    // Pause HD background animation when entering map
                    pauseCurrentAnimatedHDBackground();

                    // Notify TTF that we're entering the map screen
                    notifyTTFMenuStateChanged(true, true);

                    processMapScreen();

                    // Notify TTF that we're exiting the map screen
                    notifyTTFMenuStateChanged(false, false);

                    // Resume HD background animation when exiting map
                    resumeCurrentAnimatedHDBackground();

                    while(key==0x0F || key == 0x1B)
                    {
                        process_events();
                        localKey = key;
                    }
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
                bool isDark = (CVars[getCVarsIdx(LIGHT_OBJECT)] != -1);
                if (isDark != g_roomIsDark)
                {
                    g_roomIsDark = isDark;
                    osystem_setDarkRoom(isDark);
                    // Reload camera to switch between normal and _DARK HD background
                    InitView();
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
            // Reset dark room state before loading new floor
            if (g_roomIsDark)
            {
                g_roomIsDark = false;
                osystem_setDarkRoom(false);
            }
            LoadEtage(NewNumEtage);
        }

        if(FlagChangeSalle)
        {
			ChangeSalle(NewNumSalle);
            InitView();

            // Fix: warp player out of walls after any room transition
            fixPlayerStuckInWall();

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
            updateRockingHorseVisibility();
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

        // Update hotspot overlay visibility with fade in/out
        // Don't show hotspot indicator during intro, game over, or ending scenes
        {
            bool detected = (allowSystemMenu && !FlagGameOver && g_remasterConfig.graphics.enableHints) ? detectNearbyInteractiveHotspot() : false;
            u32 now = SDL_GetTicks();
            float dt = (s_lastHotspotUpdateTime > 0) ? (now - s_lastHotspotUpdateTime) / 1000.0f : 0.0f;
            s_lastHotspotUpdateTime = now;
            if (dt > 0.1f) dt = 0.1f; // clamp large spikes
            if (detected)
            {
                g_hotspotOverlayOpacity += HOTSPOT_FADE_SPEED * dt;
                if (g_hotspotOverlayOpacity > 1.0f) g_hotspotOverlayOpacity = 1.0f;
            }
            else
            {
                g_hotspotOverlayOpacity -= HOTSPOT_FADE_SPEED * dt;
                if (g_hotspotOverlayOpacity < 0.0f) g_hotspotOverlayOpacity = 0.0f;
            }
        }

        AllRedraw(flagRedraw);

        updatePendingEvents();
    }

    shakeVar1 = 0;
    shakingAmplitude = 0;

    stopShaking();
    //  stopSounds();
}

