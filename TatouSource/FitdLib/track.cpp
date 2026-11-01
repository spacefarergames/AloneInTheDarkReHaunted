///////////////////////////////////////////////////////////////////////////////
// Alone In The Dark Re-Haunted
// Copyright (C) 2026 Infogrames / Spacefarer Retro Remasters LLC
// Based on FITD by yaz0r, Re-haunted is released under GPL
// Author: Jake Jackson (jake@spacefarergames.com)
//
// Actor movement tracking and pathfinding
///////////////////////////////////////////////////////////////////////////////

#include "common.h"
#include "lifeBugfixes.h"
#include "consoleLog.h"
#include "math.h"

#define	TL_INIT_COOR		0
#define	TL_GOTO			1
#define	TL_END			2
#define	TL_REPEAT		3
#define	TL_MARK			4
#define	TL_WALK			5 // removed in AITD2+
#define	TL_RUN			6 // removed in AITD2+
#define	TL_STOP			7
#define	TL_BACK			8 // removed in AITD2+
#define	TL_SET_ANGLE		9
#define	TL_COL_OFF		10
#define	TL_COL_ON		11
#define	TL_SET_DIST		12
#define	TL_DEC_OFF		13
#define	TL_DEC_ON		14
#define	TL_GOTO_3D		15
#define	TL_MEMO_COOR		16
#define	TL_GOTO_3DX		17
#define	TL_GOTO_3DZ		18
#define	TL_ANGLE		19
#define	TL_CLOSE		20

// AITD2+ has removed a few entries, so covert the enum to match
s16 convertTrackMacro(s16 inTrackMacro) {
    if (g_gameId == AITD1) {
        return inTrackMacro;
    }

    if (inTrackMacro <= 4)
        return inTrackMacro;

    if (inTrackMacro >= 6)
        return inTrackMacro + 3;

    assert(inTrackMacro == 5);
    return TL_STOP;
}

int makeProportional(int x1, int x2, int y1, int y2)
{
    if(y1 == 0)
        return x1;
    return x1 + ((x2 - x1) * y2) / y1;
}

int computeAngleModificatorToPositionSub1(int ax)
{
    int xOut;
    int yOut;

    Rotate(ax,0,1000,&xOut,&yOut);

    yOut *= angleCompZ;
    xOut *= angleCompX;

    yOut -= xOut;

    if(yOut==0)
        return(0);

    if(yOut>0)
        return(1);
    else
        return(-1);
}

int CapObjet(int x1,int z1, int beta, int x2, int z2)
{
    int resultMin;
    int resultMax;

    angleCompX = x2 - x1;
    angleCompZ = z2 - z1;
    angleCompBeta = beta;

    resultMin = computeAngleModificatorToPositionSub1(beta - 4);
    resultMax = computeAngleModificatorToPositionSub1(beta + 4);

    if(resultMax == -1 && resultMin == 1) // in the middle
    {
        return(computeAngleModificatorToPositionSub1(beta));
    }
    else
    {
        return(((resultMax+resultMin)+1)>>1);
    }
}

void GereManualRot(int param)
{
    // Quick turn left (180 degrees) - only when standing still
    if((localJoyD&0x10) && currentProcessedActorPtr->speed == 0)
    {
        if(currentProcessedActorPtr->direction != 2)
        {
            currentProcessedActorPtr->rotate.numSteps = 0;
        }
        currentProcessedActorPtr->direction = 2;
        if(currentProcessedActorPtr->rotate.numSteps == 0)
        {
            int oldBeta = currentProcessedActorPtr->beta;
            InitRealValue(oldBeta, oldBeta + 0x200, 15, &currentProcessedActorPtr->rotate);
        }
        currentProcessedActorPtr->beta = updateActorRotation(&currentProcessedActorPtr->rotate);
        return;
    }
    // Quick turn right (180 degrees) - only when standing still
    if((localJoyD&0x20) && currentProcessedActorPtr->speed == 0)
    {
        if(currentProcessedActorPtr->direction != -2)
        {
            currentProcessedActorPtr->rotate.numSteps = 0;
        }
        currentProcessedActorPtr->direction = -2;
        if(currentProcessedActorPtr->rotate.numSteps == 0)
        {
            int oldBeta = currentProcessedActorPtr->beta;
            InitRealValue(oldBeta, oldBeta - 0x200, 15, &currentProcessedActorPtr->rotate);
        }
        currentProcessedActorPtr->beta = updateActorRotation(&currentProcessedActorPtr->rotate);
        return;
    }
    if(localJoyD&4)
    {
        if(currentProcessedActorPtr->direction != 1)
        {
            currentProcessedActorPtr->rotate.numSteps = 0;
        }

        currentProcessedActorPtr->direction = 1;

        if(currentProcessedActorPtr->rotate.numSteps == 0)
        {
            int oldBeta = currentProcessedActorPtr->beta;

            if(currentProcessedActorPtr->speed == 0)
            {
                InitRealValue(oldBeta,oldBeta+0x100,param/2,&currentProcessedActorPtr->rotate);
            }
            else
            {
                InitRealValue(oldBeta,oldBeta+0x100,param,&currentProcessedActorPtr->rotate);
            }
        }

        currentProcessedActorPtr->beta = updateActorRotation(&currentProcessedActorPtr->rotate);
    }
    if(localJoyD&8)
    {
        if(currentProcessedActorPtr->direction != -1)
        {
            currentProcessedActorPtr->rotate.numSteps = 0;
        }

        currentProcessedActorPtr->direction = -1;

        if(currentProcessedActorPtr->rotate.numSteps == 0)
        {
            int oldBeta = currentProcessedActorPtr->beta;

            if(currentProcessedActorPtr->speed == 0)
            {
                InitRealValue(oldBeta,oldBeta-0x100,param/2,&currentProcessedActorPtr->rotate);
            }
            else
            {
                InitRealValue(oldBeta,oldBeta-0x100,param,&currentProcessedActorPtr->rotate);
            }
        }

        currentProcessedActorPtr->beta = updateActorRotation(&currentProcessedActorPtr->rotate);
    }
    if(!(localJoyD&0x3C))
    {
        currentProcessedActorPtr->direction = 0;
        currentProcessedActorPtr->rotate.numSteps = 0;
    }
}

#define DISTANCE_TO_POINT_TRESSHOLD 400

// Per-actor stuck detection for TL_GOTO waypoints.
// When an actor is blocked by hard collision and cannot reach its waypoint,
// these counters let us skip past the unreachable waypoint after a timeout
// so the track can continue (prevents permanent freeze, e.g. zombie chicken
// trying to pass through a narrow window).
static unsigned int s_gotoLastDistance[NUM_MAX_OBJECT] = {};
static unsigned int s_gotoStuckFrames[NUM_MAX_OBJECT] = {};
#define GOTO_STUCK_FRAME_LIMIT 180  // ~3 seconds at 60fps

static bool isValidRoomIndex(int room)
{
    return room >= 0 && room < (int)roomDataTable.size();
}

static bool isValidWorldObjectIndex(int index)
{
    return index >= 0 && index < (int)ListWorldObjets.size();
}

static bool isValidTrackNumber(int trackNumber)
{
    static int cachedGameId = -1;
    static unsigned int cachedTrackCount = 0;
    if (cachedGameId != g_gameId)
    {
        cachedGameId = g_gameId;
        cachedTrackCount = PAK_getNumFiles("LISTTRAK");
    }
    return trackNumber >= 0 && (unsigned int)trackNumber < cachedTrackCount;
}

static void resetVerticalTrackMotion(tObject* actor)
{
    actor->YHandler.numSteps = 0;
    actor->YHandler.startValue = 0;
    actor->YHandler.endValue = 0;
    actor->hidePlanarShadow = false;
}

static void stopInvalidTrack(const char* reason)
{
    printf(TRACK_WARN "Stopping invalid track %d for actor %d: %s" CON_RESET "\n",
        currentProcessedActorPtr->trackNumber, currentProcessedActorIdx, reason);
    s_gotoStuckFrames[currentProcessedActorIdx] = 0;
    s_gotoLastDistance[currentProcessedActorIdx] = 0;
    resetVerticalTrackMotion(currentProcessedActorPtr);
    currentProcessedActorPtr->speed = 0;
    currentProcessedActorPtr->direction = 0;
    currentProcessedActorPtr->trackNumber = -1;
    InitDeplacement(0, 0);
    if (currentProcessedActorIdx == currentCameraTargetActor)
        osystem_endLetterboxWithCooldown();
}

static int getLoadedTrackSize(int trackNumber)
{
    if (!listTrack)
        return 0;

    for (int i = 0; i < listTrack->numUsedEntry; ++i)
    {
        const hqrSubEntryStruct<char>& entry = listTrack->entries[i];
        if (entry.ptr && entry.key == trackNumber)
            return entry.size;
    }
    return 0;
}

// Size of each implemented opcode in 16-bit words, including the opcode.
// Keeping this table beside the interpreter prevents a bad cursor or truncated
// LISTTRAK entry from turning the next operand into an opcode.
static int getTrackMacroWordCount(s16 trackMacro)
{
    switch (trackMacro)
    {
    case TL_INIT_COOR: return 5;
    case TL_GOTO: return 4;
    case TL_END:
    case TL_REPEAT:
    case TL_WALK:
    case TL_RUN:
    case TL_STOP:
    case TL_BACK:
    case TL_COL_OFF:
    case TL_COL_ON:
    case TL_DEC_OFF:
    case TL_DEC_ON:
    case TL_MEMO_COOR: return 1;
    case TL_MARK:
    case TL_SET_ANGLE: return 2;
    case TL_GOTO_3D: return 6;
    case TL_GOTO_3DX:
    case TL_GOTO_3DZ:
    case TL_ANGLE: return 4;
    default: return 0;
    }
}

unsigned int lastTimeForward = 0;

char* getRoomLink(unsigned int room1, unsigned int room2)
{
    int i;
    s16 numOfZones;
    if (!isValidRoomIndex((int)room1) || !isValidRoomIndex((int)room2))
        return nullptr;

    char* zoneData = (char*)getRoomData(room1);
    if (!zoneData)
        return nullptr;
    char* bestZone = nullptr;

    zoneData += *(s16*)(zoneData);
    numOfZones = *(s16*)zoneData;
    zoneData+=2;

    for(i=0;i<numOfZones;i++)
    {
        if(*(s16*)(zoneData+14) == 4)
        {
            bestZone = zoneData;

            if(*(s16*)(zoneData+12) == room2)
            {
                return bestZone;
            }
        }

        zoneData += 16;
    }

    return bestZone;
}

void processTrack(void)
{
    switch(currentProcessedActorPtr->trackMode)
    {
    case 1: // manual
        {
            GereManualRot(40); // reduced from 60 for snappier rotation during combat
            if(localJoyD&1) // forward
            {
                if((localJoyD&0x40) || (timer - lastTimeForward < 10 && currentProcessedActorPtr->speed != 4)) // run button held OR double-tap forward
                {
                    currentProcessedActorPtr->speed = 5;
                }
                else
                {
                    if(currentProcessedActorPtr->speed == 0 || currentProcessedActorPtr->speed == -1)
                    {
                        currentProcessedActorPtr->speed = 4;
                    }
                }

                lastTimeForward = timer;
            }
            else
            {
                if((currentProcessedActorPtr->speed > 0) && (currentProcessedActorPtr->speed<=4))
                {
                    currentProcessedActorPtr->speed = 0; // instant stop for more responsive movement
                }
                else
                {
                    currentProcessedActorPtr->speed = 0;
                }
            }

            if(localJoyD&2) // backward
            {
                if(currentProcessedActorPtr->speed == 0 || currentProcessedActorPtr->speed >= 4)
                    currentProcessedActorPtr->speed = -1;

                if(currentProcessedActorPtr->speed == 5)
                    currentProcessedActorPtr->speed = 0;
            }

            break;
        }
    case 2: // follow
        {
            if (!isValidWorldObjectIndex(currentProcessedActorPtr->trackNumber))
            {
                currentProcessedActorPtr->trackMode = 0;
                currentProcessedActorPtr->direction = 0;
                currentProcessedActorPtr->speed = 0;
                break;
            }
            int followedActorIdx = ListWorldObjets[currentProcessedActorPtr->trackNumber].objIndex;

            if(followedActorIdx < 0 || followedActorIdx >= NUM_MAX_OBJECT)
            {
                // Target was deleted - stop following and switch to idle.
                // Without this, the enemy stays in trackMode 2 forever with
                // direction=0/speed=0, permanently frozen.
                currentProcessedActorPtr->trackMode = 0;
                currentProcessedActorPtr->direction = 0;
                currentProcessedActorPtr->speed = 0;
            }
            else
            {
                tObject* followedActorPtr = &ListObjets[followedActorIdx];

                int targetRoomNumber = followedActorPtr->room;
                int targetX = followedActorPtr->roomX;
                int targetY = followedActorPtr->roomY;
                int targetZ = followedActorPtr->roomZ;
                int angleModif;

                if (g_gameId == AITD1) {
                    // in AITD1, if the target entity is in a different room, the entity would aim for the center of the trigger zone to get to that room
                    if (currentProcessedActorPtr->room != targetRoomNumber &&
                        isValidRoomIndex(currentProcessedActorPtr->room) &&
                        isValidRoomIndex(targetRoomNumber))
                    {
                        char* link = getRoomLink(currentProcessedActorPtr->room, targetRoomNumber);
                        if (link)
                        {
                            targetX = *(s16*)(link + 0) + (((*(s16*)(link + 2)) - (*(s16*)(link + 0))) / 2);
                            targetY = *(s16*)(link + 4) + (((*(s16*)(link + 6)) - (*(s16*)(link + 4))) / 2);
                            targetZ = *(s16*)(link + 8) + (((*(s16*)(link + 10)) - (*(s16*)(link + 8))) / 2);
                        }
                    }
                }
                else {
                    // This was changed from AITD2+ to aim for the target in world-space
                    if (isValidRoomIndex(followedActorPtr->room) &&
                        isValidRoomIndex(currentProcessedActorPtr->room))
                    {
                        const auto& targetRoom = roomDataTable[followedActorPtr->room];
                        const auto& sourceRoom = roomDataTable[currentProcessedActorPtr->room];

                        targetX -= (sourceRoom.worldX - targetRoom.worldX) * 10;
                        targetZ += (sourceRoom.worldZ - targetRoom.worldZ) * 10;
                    }
                }

                angleModif = CapObjet( currentProcessedActorPtr->roomX + currentProcessedActorPtr->stepX,
                    currentProcessedActorPtr->roomZ + currentProcessedActorPtr->stepZ,
                    currentProcessedActorPtr->beta, targetX, targetZ);

                if( (currentProcessedActorPtr->rotate.numSteps == 0) || (currentProcessedActorPtr->direction != angleModif) )
                {
                    InitRealValue( currentProcessedActorPtr->beta, currentProcessedActorPtr->beta - (angleModif * 256), 60, &currentProcessedActorPtr->rotate);
                }

                currentProcessedActorPtr->direction = angleModif;

                if( currentProcessedActorPtr->direction == 0 )
                {
                    currentProcessedActorPtr->rotate.numSteps = 0;
                }
                else
                {
                    currentProcessedActorPtr->beta = updateActorRotation(&currentProcessedActorPtr->rotate);
                }

                currentProcessedActorPtr->speed = 4;

            }
            break;
        }
    case 3: // track
        {
            if (!isValidTrackNumber(currentProcessedActorPtr->trackNumber))
            {
                stopInvalidTrack("track number outside LISTTRAK archive");
                break;
            }
            char* trackData = HQR_Get(listTrack,currentProcessedActorPtr->trackNumber);
            const int trackSize = getLoadedTrackSize(currentProcessedActorPtr->trackNumber);
            const int position = currentProcessedActorPtr->positionInTrack;
            if (!trackData || trackSize < 2 || position < 0 || position > (trackSize / 2) - 1)
            {
                stopInvalidTrack("missing resource or cursor outside LISTTRAK entry");
                break;
            }

            trackData = trackBugfixPatchTrack(currentProcessedActorPtr->trackNumber, trackData);
            char* trackPtr = trackData + position * 2;

            s16 trackMacro = convertTrackMacro(*(s16*)trackPtr);
            const int macroWords = getTrackMacroWordCount(trackMacro);
            if (macroWords == 0)
            {
                stopInvalidTrack("unsupported opcode");
                break;
            }
            if (position > (trackSize / 2) - macroWords)
            {
                stopInvalidTrack("truncated opcode operands");
                break;
            }
            trackPtr += 2;

            //printf("Track macro %X\n",trackMacro);

            switch(trackMacro)
            {
            case TL_INIT_COOR: // warp
                {
                    int roomNumber = *(s16*)(trackPtr);
                    trackPtr += 2;

                    if (!isValidRoomIndex(roomNumber) || !isValidRoomIndex(currentRoom))
                    {
                        stopInvalidTrack("TL_INIT_COOR references an invalid room");
                        break;
                    }

                    if(currentProcessedActorPtr->room != roomNumber)
                    {
                        if(currentCameraTargetActor == currentProcessedActorIdx)
                        {
                            FlagChangeSalle = 1;
                            NewNumSalle = roomNumber;
                        }

                        currentProcessedActorPtr->room = roomNumber;
                    }

                    currentProcessedActorPtr->zv.ZVX1 -= currentProcessedActorPtr->roomX + currentProcessedActorPtr->stepX;
                    currentProcessedActorPtr->zv.ZVX2 -= currentProcessedActorPtr->roomX + currentProcessedActorPtr->stepX;
                    currentProcessedActorPtr->zv.ZVY1 -= currentProcessedActorPtr->roomY + currentProcessedActorPtr->stepY;
                    currentProcessedActorPtr->zv.ZVY2 -= currentProcessedActorPtr->roomY + currentProcessedActorPtr->stepY;
                    currentProcessedActorPtr->zv.ZVZ1 -= currentProcessedActorPtr->roomZ + currentProcessedActorPtr->stepZ;
                    currentProcessedActorPtr->zv.ZVZ2 -= currentProcessedActorPtr->roomZ + currentProcessedActorPtr->stepZ;

                    currentProcessedActorPtr->worldX = currentProcessedActorPtr->roomX = *(s16*)(trackPtr);
                    trackPtr += 2;
                    currentProcessedActorPtr->worldY = currentProcessedActorPtr->roomY = *(s16*)(trackPtr);
                    trackPtr += 2;
                    currentProcessedActorPtr->worldZ = currentProcessedActorPtr->roomZ = *(s16*)(trackPtr);
                    trackPtr += 2;

                    currentProcessedActorPtr->worldX -= (s16)((roomDataTable[currentRoom].worldX - roomDataTable[currentProcessedActorPtr->room].worldX) * 10);
                    currentProcessedActorPtr->worldY += (s16)((roomDataTable[currentRoom].worldY - roomDataTable[currentProcessedActorPtr->room].worldY) * 10);
                    currentProcessedActorPtr->worldZ += (s16)((roomDataTable[currentRoom].worldZ - roomDataTable[currentProcessedActorPtr->room].worldZ) * 10);

                    currentProcessedActorPtr->zv.ZVX1 += currentProcessedActorPtr->roomX + currentProcessedActorPtr->stepX;
                    currentProcessedActorPtr->zv.ZVX2 += currentProcessedActorPtr->roomX + currentProcessedActorPtr->stepX;
                    currentProcessedActorPtr->zv.ZVY1 += currentProcessedActorPtr->roomY + currentProcessedActorPtr->stepY;
                    currentProcessedActorPtr->zv.ZVY2 += currentProcessedActorPtr->roomY + currentProcessedActorPtr->stepY;
                    currentProcessedActorPtr->zv.ZVZ1 += currentProcessedActorPtr->roomZ + currentProcessedActorPtr->stepZ;
                    currentProcessedActorPtr->zv.ZVZ2 += currentProcessedActorPtr->roomZ + currentProcessedActorPtr->stepZ;

                    currentProcessedActorPtr->speed = 0;
                    currentProcessedActorPtr->direction = 0;
                    currentProcessedActorPtr->rotate.numSteps = 0;
                    currentProcessedActorPtr->positionInTrack += 5;

                    break;
                }
            case TL_GOTO: // goToPosition
                {
                    int roomNumber = *(s16*)(trackPtr);
                    int x;
                    int y;
                    int z;
                    unsigned int distanceToPoint;

                    trackPtr += 2;

                    x = *(s16*)(trackPtr);
                    trackPtr += 2;
                    y = 0;
                    z = *(s16*)(trackPtr);
                    trackPtr += 2;

                    if (!isValidRoomIndex(currentProcessedActorPtr->room) || !isValidRoomIndex(roomNumber))
                    {
                        stopInvalidTrack("TL_GOTO references an invalid room");
                        break;
                    }

                    if(roomNumber != currentProcessedActorPtr->room)
                    {
                        // TODO: fix bug here...
                        x -= (roomDataTable[currentProcessedActorPtr->room].worldX - roomDataTable[roomNumber].worldX) * 10;
                        z += (roomDataTable[currentProcessedActorPtr->room].worldZ - roomDataTable[roomNumber].worldZ) * 10;
                    }

                    distanceToPoint = GiveDistance2D( currentProcessedActorPtr->roomX + currentProcessedActorPtr->stepX,
                        currentProcessedActorPtr->roomZ + currentProcessedActorPtr->stepZ,
                        x,z );


                    if(distanceToPoint >= DISTANCE_TO_POINT_TRESSHOLD) // not yet at position
                    {
                        // Stuck detection: if the actor hasn't gotten closer to the
                        // waypoint for GOTO_STUCK_FRAME_LIMIT frames (e.g. blocked
                        // by hard collision at a narrow window), skip the waypoint
                        // so the rest of the track can proceed.
                        if (distanceToPoint < s_gotoLastDistance[currentProcessedActorIdx])
                        {
                            s_gotoStuckFrames[currentProcessedActorIdx] = 0;
                        }
                        else
                        {
                            s_gotoStuckFrames[currentProcessedActorIdx]++;
                        }
                        s_gotoLastDistance[currentProcessedActorIdx] = distanceToPoint;

                        if (s_gotoStuckFrames[currentProcessedActorIdx] >= GOTO_STUCK_FRAME_LIMIT)
                        {
                            // Timed out — skip this unreachable waypoint
                            s_gotoStuckFrames[currentProcessedActorIdx] = 0;
                            s_gotoLastDistance[currentProcessedActorIdx] = 0;
                            currentProcessedActorPtr->positionInTrack += 4;
                            break;
                        }

                        // If speed was zeroed (e.g. by TL_INIT_COOR, TL_STOP, or a track that
                        // starts directly with TL_GOTO without a preceding TL_WALK), the actor
                        // will turn toward the waypoint but never move, causing a permanent stuck.
                        // Restore walk speed so the waypoint can actually be reached.
                        if (currentProcessedActorPtr->speed == 0)
                            currentProcessedActorPtr->speed = 4;

                        int angleModif = CapObjet( currentProcessedActorPtr->roomX + currentProcessedActorPtr->stepX,
                            currentProcessedActorPtr->roomZ + currentProcessedActorPtr->stepZ,
                            currentProcessedActorPtr->beta,
                            x,z );

                        if((currentProcessedActorPtr->rotate.numSteps == 0) || (currentProcessedActorPtr->direction != angleModif))
                        {
                            InitRealValue(currentProcessedActorPtr->beta, currentProcessedActorPtr->beta - (angleModif * 64), 15, &currentProcessedActorPtr->rotate);
                        }

                        currentProcessedActorPtr->direction = angleModif;

                        if(!angleModif)
                        {
                            currentProcessedActorPtr->rotate.numSteps = 0;
                        }
                        else
                        {
                            currentProcessedActorPtr->beta = updateActorRotation(&currentProcessedActorPtr->rotate);
                        }
                    }
                    else // reached position
                    {
                        s_gotoStuckFrames[currentProcessedActorIdx] = 0;
                        s_gotoLastDistance[currentProcessedActorIdx] = 0;
                        currentProcessedActorPtr->positionInTrack += 4;
                    }

                    break;
                }
            case TL_GOTO_3D: // goToPosition
            {
                int roomNumber = *(s16*)(trackPtr);
                int x;
                int y;
                int z;
                trackPtr += 2;

                x = *(s16*)(trackPtr);
                trackPtr += 2;
                y = *(s16*)(trackPtr);
                trackPtr += 2;
                z = *(s16*)(trackPtr);
                trackPtr += 2;
                int time = *(s16*)(trackPtr);
                trackPtr += 2;

                if (!isValidRoomIndex(currentProcessedActorPtr->room) || !isValidRoomIndex(roomNumber))
                {
                    stopInvalidTrack("TL_GOTO_3D references an invalid room");
                    break;
                }

                if (roomNumber != currentProcessedActorPtr->room)
                {
                    // TODO: fix bug here...
                    x -= (roomDataTable[currentProcessedActorPtr->room].worldX - roomDataTable[roomNumber].worldX) * 10;
                    y += (roomDataTable[currentProcessedActorPtr->room].worldY - roomDataTable[roomNumber].worldY) * 10;
                    z += (roomDataTable[currentProcessedActorPtr->room].worldZ - roomDataTable[roomNumber].worldZ) * 10;
                }

                // reached position? Use tolerance like TL_GOTO_3DX/3DZ (exact match can
                // fail due to integer rounding in Y interpolation, causing permanent stuck)
                if (abs(y - currentProcessedActorPtr->roomY) <= 100)
                {
                    int distance = GiveDistance2D(currentProcessedActorPtr->roomX + currentProcessedActorPtr->stepX, currentProcessedActorPtr->roomZ + currentProcessedActorPtr->stepZ, x, z);
                    if (distance < DISTANCE_TO_POINT_TRESSHOLD) {
                        s_gotoStuckFrames[currentProcessedActorIdx] = 0;
                        s_gotoLastDistance[currentProcessedActorIdx] = 0;
                        resetVerticalTrackMotion(currentProcessedActorPtr);
                        currentProcessedActorPtr->positionInTrack += 6;
                        break;
                    }
                }

                {
                    // Stuck detection (same logic as TL_GOTO)
                    unsigned int dist3d = GiveDistance2D(
                        currentProcessedActorPtr->roomX + currentProcessedActorPtr->stepX,
                        currentProcessedActorPtr->roomZ + currentProcessedActorPtr->stepZ, x, z);
                    dist3d += (unsigned int)abs(y -
                        (currentProcessedActorPtr->roomY + currentProcessedActorPtr->stepY));
                    if (s_gotoLastDistance[currentProcessedActorIdx] == 0 ||
                        dist3d < s_gotoLastDistance[currentProcessedActorIdx])
                    {
                        s_gotoStuckFrames[currentProcessedActorIdx] = 0;
                    }
                    else
                    {
                        s_gotoStuckFrames[currentProcessedActorIdx]++;
                    }
                    s_gotoLastDistance[currentProcessedActorIdx] = dist3d;

                    if (s_gotoStuckFrames[currentProcessedActorIdx] >= GOTO_STUCK_FRAME_LIMIT)
                    {
                        s_gotoStuckFrames[currentProcessedActorIdx] = 0;
                        s_gotoLastDistance[currentProcessedActorIdx] = 0;
                        resetVerticalTrackMotion(currentProcessedActorPtr);
                        currentProcessedActorPtr->positionInTrack += 6;
                        break;
                    }

                    // Recover speed if zeroed before this 3D goto, same reason as TL_GOTO.
                    if (currentProcessedActorPtr->speed == 0)
                        currentProcessedActorPtr->speed = 4;

                    int direction = CapObjet(
                        currentProcessedActorPtr->roomX + currentProcessedActorPtr->stepX,
                        currentProcessedActorPtr->roomZ + currentProcessedActorPtr->stepZ,
                        currentProcessedActorPtr->beta,
                        x, z);

                    if (currentProcessedActorPtr->YHandler.numSteps == 0)
                    {
                        InitRealValue(0, y - (currentProcessedActorPtr->roomY + currentProcessedActorPtr->stepY), time, &currentProcessedActorPtr->YHandler);
                        currentProcessedActorPtr->hidePlanarShadow = true;
                    }

                    if ((currentProcessedActorPtr->rotate.numSteps == 0) || (currentProcessedActorPtr->direction != direction))
                    {
                        InitRealValue(currentProcessedActorPtr->beta, currentProcessedActorPtr->beta - (direction * 256), 60, &currentProcessedActorPtr->rotate);
                    }

                    currentProcessedActorPtr->direction = direction;

                    if (!direction)
                    {
                        currentProcessedActorPtr->rotate.numSteps = 0;
                    }
                    else
                    {
                        currentProcessedActorPtr->beta = updateActorRotation(&currentProcessedActorPtr->rotate);
                    }
                }

                break;
            }
            case TL_END: // stop
                {
                    s_gotoStuckFrames[currentProcessedActorIdx] = 0;
                    s_gotoLastDistance[currentProcessedActorIdx] = 0;
                    currentProcessedActorPtr->speed = 0;
                    currentProcessedActorPtr->trackNumber = -1;
                    InitDeplacement(0,0);
                    break;
                }
            case TL_REPEAT:
                {
                    s_gotoStuckFrames[currentProcessedActorIdx] = 0;
                    s_gotoLastDistance[currentProcessedActorIdx] = 0;
                    currentProcessedActorPtr->positionInTrack = 0;
                    break;
                }
            case TL_MARK: // MARK
                {
                    currentProcessedActorPtr->MARK = *(s16*)(trackPtr);
                    trackPtr += 2;
                    currentProcessedActorPtr->positionInTrack += 2;
                    break;
                }
            case TL_WALK:
                {
                    currentProcessedActorPtr->speed = 4;
                    currentProcessedActorPtr->positionInTrack++;
                    break;
                }
            case TL_RUN:
                {
                    currentProcessedActorPtr->speed = 5;
                    currentProcessedActorPtr->positionInTrack++;
                    break;
                }
            case TL_STOP:
                {
                    currentProcessedActorPtr->speed = 0;
                    currentProcessedActorPtr->positionInTrack++;
                    break;
                }
            case TL_BACK:
                {
                    currentProcessedActorPtr->speed = -1;
                    currentProcessedActorPtr->positionInTrack++;
                    break;
                }
            case TL_SET_ANGLE: // TL_SET_ANGLE
                {
                    // Track angles are stored as signed 16-bit values but actor
                    // angles are normalized to 0..1023 after every update.  An
                    // authored -1/1024-style angle would therefore rotate to the
                    // right orientation and then wait forever on an impossible
                    // raw-value equality unless the target is normalized too.
                    int betaDif = (*(s16*)(trackPtr)) & 0x3FF;
                    trackPtr += 2;

                    if(((currentProcessedActorPtr->beta - betaDif)&1023) > 512)
                    {
                        currentProcessedActorPtr->direction = 1; // left
                    }
                    else
                    {
                        currentProcessedActorPtr->direction = -1; // right
                    }

                    if(!currentProcessedActorPtr->rotate.numSteps)
                    {
                        InitRealValue(currentProcessedActorPtr->beta, betaDif, 120, &currentProcessedActorPtr->rotate);
                    }

                    currentProcessedActorPtr->beta = updateActorRotation(&currentProcessedActorPtr->rotate);

                    if(currentProcessedActorPtr->beta == betaDif)
                    {
                        currentProcessedActorPtr->direction = 0;

                        currentProcessedActorPtr->positionInTrack+=2;
                    }

                    break;
                }
            case TL_COL_OFF:
                {
                    currentProcessedActorPtr->dynFlags &= ~1;
                    currentProcessedActorPtr->positionInTrack++;
                    break;
                }
            case TL_COL_ON:
                {
                    currentProcessedActorPtr->dynFlags |= 1;
                    currentProcessedActorPtr->positionInTrack++;
                    break;
                }
            case TL_DEC_OFF: // background collision off
                {
                    currentProcessedActorPtr->objectType &= ~AF_TRIGGER;
                    currentProcessedActorPtr->positionInTrack++;
                    break;
                }
            case TL_DEC_ON: // background collision on
                {
                    currentProcessedActorPtr->objectType |= AF_TRIGGER;
                    currentProcessedActorPtr->positionInTrack++;
                    break;
                }
            case TL_MEMO_COOR:
                {
                    int objNum = currentProcessedActorPtr->indexInWorld;

                    if (!isValidWorldObjectIndex(objNum))
                    {
                        stopInvalidTrack("TL_MEMO_COOR references an invalid world object");
                        break;
                    }

                    ListWorldObjets[objNum].x = currentProcessedActorPtr->roomX + currentProcessedActorPtr->stepX;
                    ListWorldObjets[objNum].y = currentProcessedActorPtr->roomY + currentProcessedActorPtr->stepY;
                    ListWorldObjets[objNum].z = currentProcessedActorPtr->roomZ + currentProcessedActorPtr->stepZ;

                    currentProcessedActorPtr->positionInTrack++;

                    break;
                }
            case TL_GOTO_3DX: // walk up/down stairs on X
                {
                    int x;
                    int y;
                    int z;
                    int objX;
                    int objY;
                    int objZ;

                    x = *(s16*)(trackPtr);
                    trackPtr += 2;
                    y = *(s16*)(trackPtr);
                    trackPtr += 2;
                    z = *(s16*)(trackPtr);
                    trackPtr += 2;

                    if (!isValidWorldObjectIndex(currentProcessedActorPtr->indexInWorld))
                    {
                        stopInvalidTrack("TL_GOTO_3DX references an invalid world object");
                        break;
                    }

                    objX = ListWorldObjets[currentProcessedActorPtr->indexInWorld].x;
                    objY = ListWorldObjets[currentProcessedActorPtr->indexInWorld].y;
                    objZ = ListWorldObjets[currentProcessedActorPtr->indexInWorld].z;

                    if(   currentProcessedActorPtr->roomY + currentProcessedActorPtr->stepY < y - 100
                        ||  currentProcessedActorPtr->roomY + currentProcessedActorPtr->stepY > y + 100)
                    {
                        // Show letterbox when player starts climbing stairs
                        if (currentProcessedActorIdx == currentCameraTargetActor)
                        {
                            osystem_startLetterbox();
                        }

                        // Recover speed if zeroed before this stair goto, same reason as TL_GOTO.
                        if (currentProcessedActorPtr->speed == 0)
                            currentProcessedActorPtr->speed = 4;

                        // AITD1's post-sorcerer crawl back into the cellar uses
                        // track 43 and TL_GOTO_3DX.  If the saved bridge/cave
                        // state leaves the interpolation origin unusable, the
                        // actor can make no progress and NUM_TRACK never reaches
                        // -1, permanently withholding player control.  Apply the
                        // same bounded waypoint recovery used by TL_GOTO.
                        const unsigned int distanceToEnd =
                            (unsigned int)abs(x - (currentProcessedActorPtr->roomX + currentProcessedActorPtr->stepX)) +
                            (unsigned int)abs(y - (currentProcessedActorPtr->roomY + currentProcessedActorPtr->stepY));
                        if (s_gotoLastDistance[currentProcessedActorIdx] == 0 ||
                            distanceToEnd < s_gotoLastDistance[currentProcessedActorIdx])
                        {
                            s_gotoStuckFrames[currentProcessedActorIdx] = 0;
                        }
                        else
                        {
                            ++s_gotoStuckFrames[currentProcessedActorIdx];
                        }
                        s_gotoLastDistance[currentProcessedActorIdx] = distanceToEnd;

                        if (s_gotoStuckFrames[currentProcessedActorIdx] >= GOTO_STUCK_FRAME_LIMIT)
                        {
                            const int difX = x - (currentProcessedActorPtr->roomX + currentProcessedActorPtr->stepX);
                            const int difY = y - (currentProcessedActorPtr->roomY + currentProcessedActorPtr->stepY);
                            currentProcessedActorPtr->worldX += difX;
                            currentProcessedActorPtr->roomX += difX;
                            currentProcessedActorPtr->zv.ZVX1 += difX;
                            currentProcessedActorPtr->zv.ZVX2 += difX;
                            currentProcessedActorPtr->worldY += difY;
                            currentProcessedActorPtr->roomY += difY;
                            currentProcessedActorPtr->zv.ZVY1 += difY;
                            currentProcessedActorPtr->zv.ZVY2 += difY;
                            currentProcessedActorPtr->stepX = 0;
                            currentProcessedActorPtr->stepY = 0;
                            currentProcessedActorPtr->positionInTrack += 4;
                            s_gotoStuckFrames[currentProcessedActorIdx] = 0;
                            s_gotoLastDistance[currentProcessedActorIdx] = 0;
                            if (currentProcessedActorIdx == currentCameraTargetActor)
                                osystem_endLetterboxWithCooldown();
                            break;
                        }

                        int propX = makeProportional(objY, y, x - objX, (currentProcessedActorPtr->roomX + currentProcessedActorPtr->stepX) - objX);

                        int difY = propX - currentProcessedActorPtr->worldY;
                        int angleModif;

                        currentProcessedActorPtr->worldY += difY;
                        currentProcessedActorPtr->roomY += difY;
                        currentProcessedActorPtr->zv.ZVY1 += difY;
                        currentProcessedActorPtr->zv.ZVY2 += difY;

                        angleModif = CapObjet( currentProcessedActorPtr->roomX + currentProcessedActorPtr->stepX,
                            currentProcessedActorPtr->roomZ + currentProcessedActorPtr->stepZ,
                            currentProcessedActorPtr->beta,
                            x,z );

                        if(!currentProcessedActorPtr->rotate.numSteps || currentProcessedActorPtr->direction != angleModif)
                        {
                            InitRealValue(currentProcessedActorPtr->beta, currentProcessedActorPtr->beta - (angleModif<<8), 60, &currentProcessedActorPtr->rotate);
                        }

                        currentProcessedActorPtr->direction = angleModif;

                        if(angleModif)
                        {
                            currentProcessedActorPtr->beta = updateActorRotation(&currentProcessedActorPtr->rotate);
                        }
                        else
                        {
                            currentProcessedActorPtr->rotate.numSteps = 0;
                        }

                    }
                    else
                    {
                        s_gotoStuckFrames[currentProcessedActorIdx] = 0;
                        s_gotoLastDistance[currentProcessedActorIdx] = 0;
                        int difY = y - currentProcessedActorPtr->worldY;

                        currentProcessedActorPtr->stepY = 0;
                        currentProcessedActorPtr->worldY += difY;
                        currentProcessedActorPtr->roomY += difY;
                        currentProcessedActorPtr->zv.ZVY1 += difY;
                        currentProcessedActorPtr->zv.ZVY2 += difY;

                        currentProcessedActorPtr->positionInTrack +=4;

                        // Hide letterbox when player finishes climbing stairs
                        if (currentProcessedActorIdx == currentCameraTargetActor)
                        {
                            osystem_endLetterboxWithCooldown();
                        }
                    }

                    break;
                }
            case TL_GOTO_3DZ:
                {
                    int x;
                    int y;
                    int z;
                    int objX;
                    int objY;
                    int objZ;

                    x = *(s16*)(trackPtr);
                    trackPtr += 2;
                    y = *(s16*)(trackPtr);
                    trackPtr += 2;
                    z = *(s16*)(trackPtr);
                    trackPtr += 2;

                    if (!isValidWorldObjectIndex(currentProcessedActorPtr->indexInWorld))
                    {
                        stopInvalidTrack("TL_GOTO_3DZ references an invalid world object");
                        break;
                    }

                    objX = ListWorldObjets[currentProcessedActorPtr->indexInWorld].x;
                    objY = ListWorldObjets[currentProcessedActorPtr->indexInWorld].y;
                    objZ = ListWorldObjets[currentProcessedActorPtr->indexInWorld].z;

                    if(   currentProcessedActorPtr->roomY + currentProcessedActorPtr->stepY < y - 100
                        ||  currentProcessedActorPtr->roomY + currentProcessedActorPtr->stepY > y + 100)
                    {
                        // Show letterbox when player starts climbing stairs
                        if (currentProcessedActorIdx == currentCameraTargetActor)
                        {
                            osystem_startLetterbox();
                        }

                        // Recover speed if zeroed before this stair goto, same reason as TL_GOTO.
                        if (currentProcessedActorPtr->speed == 0)
                            currentProcessedActorPtr->speed = 4;

                        const unsigned int distanceToEnd =
                            (unsigned int)abs(z - (currentProcessedActorPtr->roomZ + currentProcessedActorPtr->stepZ)) +
                            (unsigned int)abs(y - (currentProcessedActorPtr->roomY + currentProcessedActorPtr->stepY));
                        if (s_gotoLastDistance[currentProcessedActorIdx] == 0 ||
                            distanceToEnd < s_gotoLastDistance[currentProcessedActorIdx])
                        {
                            s_gotoStuckFrames[currentProcessedActorIdx] = 0;
                        }
                        else
                        {
                            ++s_gotoStuckFrames[currentProcessedActorIdx];
                        }
                        s_gotoLastDistance[currentProcessedActorIdx] = distanceToEnd;

                        if (s_gotoStuckFrames[currentProcessedActorIdx] >= GOTO_STUCK_FRAME_LIMIT)
                        {
                            const int difZ = z - (currentProcessedActorPtr->roomZ + currentProcessedActorPtr->stepZ);
                            const int difY = y - (currentProcessedActorPtr->roomY + currentProcessedActorPtr->stepY);
                            currentProcessedActorPtr->worldZ += difZ;
                            currentProcessedActorPtr->roomZ += difZ;
                            currentProcessedActorPtr->zv.ZVZ1 += difZ;
                            currentProcessedActorPtr->zv.ZVZ2 += difZ;
                            currentProcessedActorPtr->worldY += difY;
                            currentProcessedActorPtr->roomY += difY;
                            currentProcessedActorPtr->zv.ZVY1 += difY;
                            currentProcessedActorPtr->zv.ZVY2 += difY;
                            currentProcessedActorPtr->stepZ = 0;
                            currentProcessedActorPtr->stepY = 0;
                            currentProcessedActorPtr->positionInTrack += 4;
                            s_gotoStuckFrames[currentProcessedActorIdx] = 0;
                            s_gotoLastDistance[currentProcessedActorIdx] = 0;
                            if (currentProcessedActorIdx == currentCameraTargetActor)
                                osystem_endLetterboxWithCooldown();
                            break;
                        }

                        int propZ = makeProportional(objY, y, z - objZ, (currentProcessedActorPtr->roomZ + currentProcessedActorPtr->stepZ) - objZ);

                        int difY = propZ - currentProcessedActorPtr->worldY;

                        int angleModif;

                        currentProcessedActorPtr->worldY += difY;
                        currentProcessedActorPtr->roomY += difY;
                        currentProcessedActorPtr->zv.ZVY1 += difY;
                        currentProcessedActorPtr->zv.ZVY2 += difY;

                        angleModif = CapObjet( currentProcessedActorPtr->roomX + currentProcessedActorPtr->stepX,
                            currentProcessedActorPtr->roomZ + currentProcessedActorPtr->stepZ,
                            currentProcessedActorPtr->beta,
                            x,z );

                        if(!currentProcessedActorPtr->rotate.numSteps || currentProcessedActorPtr->direction != angleModif)
                        {
                            InitRealValue(currentProcessedActorPtr->beta, currentProcessedActorPtr->beta - (angleModif<<8), 60, &currentProcessedActorPtr->rotate);
                        }

                        currentProcessedActorPtr->direction = angleModif;

                        if(angleModif)
                        {
                            currentProcessedActorPtr->beta = updateActorRotation(&currentProcessedActorPtr->rotate);
                        }
                        else
                        {
                            currentProcessedActorPtr->rotate.numSteps = 0;
                        }

                    }
                    else
                    {
                        s_gotoStuckFrames[currentProcessedActorIdx] = 0;
                        s_gotoLastDistance[currentProcessedActorIdx] = 0;
                        int difY = y - currentProcessedActorPtr->worldY;

                        currentProcessedActorPtr->stepY = 0;
                        currentProcessedActorPtr->worldY += difY;
                        currentProcessedActorPtr->roomY += difY;
                        currentProcessedActorPtr->zv.ZVY1 += difY;
                        currentProcessedActorPtr->zv.ZVY2 += difY;

                        currentProcessedActorPtr->positionInTrack +=4;

                        // Hide letterbox when player finishes climbing stairs
                        if (currentProcessedActorIdx == currentCameraTargetActor)
                        {
                            osystem_endLetterboxWithCooldown();
                        }
                    }

                    break;
                }
            case TL_ANGLE:
                {
                    currentProcessedActorPtr->alpha = *(s16*)(trackPtr);
                    trackPtr += 2;
                    currentProcessedActorPtr->beta = *(s16*)(trackPtr);
                    trackPtr += 2;
                    currentProcessedActorPtr->gamma = *(s16*)(trackPtr);
                    trackPtr += 2;

                    currentProcessedActorPtr->direction = 0;

                    currentProcessedActorPtr->positionInTrack +=4;

                    break;
                }
            default:
                {
                    stopInvalidTrack("unknown opcode");
                    break;
                }
            }

            break;
        }
    }

    currentProcessedActorPtr->beta &= 0x3FF;
}

void resetTrackStuckCounters(int actorIdx)
{
    if (actorIdx >= 0 && actorIdx < NUM_MAX_OBJECT)
    {
        s_gotoStuckFrames[actorIdx] = 0;
        s_gotoLastDistance[actorIdx] = 0;
    }
}

