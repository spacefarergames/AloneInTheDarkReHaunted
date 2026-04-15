///////////////////////////////////////////////////////////////////////////////
// Alone In The Dark Re-Haunted
// Copyright (C) 2026 Infogrames / Spacefarer Retro Remasters LLC
// Based on FITD by yaz0r, Re-haunted is released under GPL
// Author: Jake Jackson (jake@spacefarergames.com)
//
// Helper functions for authoring native life script replacements.
// These inline functions map bytecode operations to engine C calls,
// allowing native scripts to be written in readable, maintainable C.
///////////////////////////////////////////////////////////////////////////////

#pragma once

#include "common.h"
#include "vars.h"
#include "life.h"
#include "main.h"
#include "anim.h"
#include "zv.h"
#include "track.h"
#include "inventory.h"
#include "tatou.h"
#include "music.h"
#include "consoleLog.h"

//////////////////////////////////////////////////////////////////////////////
// Self-actor field accessors (evalVar equivalents for currentProcessedActorPtr)
// These match the bytecode evalVar field indices exactly.
//////////////////////////////////////////////////////////////////////////////

// 0x00: COL - returns world index of first collision partner, or -1
inline int life_GetCOL()
{
    int temp = currentProcessedActorPtr->COL[0];
    return (temp != -1) ? ListObjets[temp].indexInWorld : -1;
}

// 0x01: HARD_DEC
inline int life_GetHARD_DEC() { return currentProcessedActorPtr->HARD_DEC; }

// 0x02: HARD_COL
inline int life_GetHARD_COL() { return currentProcessedActorPtr->HARD_COL; }

// 0x03: HIT - returns world index of actor we hit, or -1
inline int life_GetHIT()
{
    int temp = currentProcessedActorPtr->HIT;
    return (temp != -1) ? ListObjets[temp].indexInWorld : -1;
}

// 0x04: HIT_BY - returns world index of actor that hit us, or -1
inline int life_GetHIT_BY()
{
    int temp = currentProcessedActorPtr->HIT_BY;
    return (temp != -1) ? ListObjets[temp].indexInWorld : -1;
}

// 0x05: ANIM
inline int life_GetANIM() { return currentProcessedActorPtr->ANIM; }

// 0x06: END_ANIM (flagEndAnim)
inline int life_GetEND_ANIM() { return currentProcessedActorPtr->flagEndAnim; }

// 0x07: FRAME
inline int life_GetFRAME() { return currentProcessedActorPtr->frame; }

// 0x08: END_FRAME
inline int life_GetEND_FRAME() { return currentProcessedActorPtr->END_FRAME; }

// 0x09: BODY
inline int life_GetBODY() { return currentProcessedActorPtr->bodyNum; }

// 0x0A: MARK
inline int life_GetMARK() { return currentProcessedActorPtr->MARK; }

// 0x0B: NUM_TRACK
inline int life_GetNUM_TRACK() { return currentProcessedActorPtr->trackNumber; }

// 0x0C: CHRONO (in 1/60th second units)
inline int life_GetCHRONO() { return evalChrono(&currentProcessedActorPtr->CHRONO) / 60; }

// 0x0D: ROOM_CHRONO
inline int life_GetROOM_CHRONO() { return evalChrono(&currentProcessedActorPtr->ROOM_CHRONO) / 60; }

// 0x0E: DIST - distance to another world object
inline int life_GetDIST(int worldObjIdx)
{
    int actorIdx = ListWorldObjets[worldObjIdx].objIndex;
    if (actorIdx == -1) return 32000;
    return abs(currentProcessedActorPtr->worldX - ListObjets[actorIdx].worldX)
         + abs(currentProcessedActorPtr->worldY - ListObjets[actorIdx].worldY)
         + abs(currentProcessedActorPtr->worldZ - ListObjets[actorIdx].worldZ);
}

// 0x0F: COL_BY - world index of actor that collided with us
inline int life_GetCOL_BY()
{
    return (currentProcessedActorPtr->COL_BY != -1)
        ? ListObjets[currentProcessedActorPtr->COL_BY].indexInWorld : -1;
}

// 0x10: FOUND - check if a world object has been found (picked up)
inline int life_GetFOUND(int worldObjIdx)
{
    return (ListWorldObjets[worldObjIdx].foundFlag & 0x8000) ? 1 : 0;
}

// 0x11: ACTION
inline int life_GetACTION() { return action; }

// 0x12: POSREL - positional relationship to another actor
int getPosRel(tObject* actor1, tObject* actor2);
inline int life_GetPOSREL(int worldObjIdx)
{
    int actorIdx = ListWorldObjets[worldObjIdx].objIndex;
    if (actorIdx == -1) return 0;
    return getPosRel(currentProcessedActorPtr, &ListObjets[actorIdx]);
}

// 0x13: KEYBOARD
inline int life_GetKEYBOARD()
{
    if (localJoyD & 4) return 4;
    if (localJoyD & 8) return 8;
    if (localJoyD & 1) return 1;
    if (localJoyD & 2) return 2;
    return 0;
}

// 0x14: BUTTON (click)
inline int life_GetBUTTON() { return localClick; }

// 0x15: COL_OR_COL_BY - returns world index of COL[0] or COL_BY
inline int life_GetCOL_OR_COL_BY()
{
    int temp = currentProcessedActorPtr->COL[0];
    if (temp == -1)
    {
        temp = currentProcessedActorPtr->COL_BY;
        if (temp == -1) return -1;
    }
    return ListObjets[temp].indexInWorld;
}

// 0x16-0x18: Rotation angles
inline int life_GetALPHA() { return currentProcessedActorPtr->alpha; }
inline int life_GetBETA() { return currentProcessedActorPtr->beta; }
inline int life_GetGAMMA() { return currentProcessedActorPtr->gamma; }

// 0x19: IN_HAND
inline int life_GetIN_HAND() { return inHandTable[currentInventory]; }

// 0x1A: HIT_FORCE
inline int life_GetHIT_FORCE() { return currentProcessedActorPtr->hitForce; }

// 0x1B: CAMERA number
inline int life_GetCAMERA() { return *(u16*)(((NumCamera + 6) * 2) + cameraPtr); }

// 0x1C: RANDOM
inline int life_GetRANDOM(int range) { return rand() % range; }

// 0x1D: FALLING
inline int life_GetFALLING() { return currentProcessedActorPtr->falling; }

// 0x1E: ROOM
inline int life_GetROOM() { return currentProcessedActorPtr->room; }

// 0x1F: LIFE (script number, not health)
inline int life_GetLIFE() { return currentProcessedActorPtr->life; }

// 0x20: OBJ_FOUND - check if object was found or deleted
inline int life_GetOBJ_FOUND(int worldObjIdx)
{
    return (ListWorldObjets[worldObjIdx].foundFlag & 0xC000) ? 1 : 0;
}

// 0x21: ROOM_Y
inline int life_GetROOM_Y() { return currentProcessedActorPtr->roomY; }

// 0x22: TEST_ZV_END_ANIM
int testZvEndAnim(tObject* actorPtr, sAnimation* animPtr, int param);
inline int life_GetTEST_ZV_END_ANIM(int animNum, int param)
{
    return testZvEndAnim(currentProcessedActorPtr, HQR_Get(HQ_Anims, animNum), param);
}

// 0x23: CURRENT_MUSIC
inline int life_GetCURRENT_MUSIC() { return currentMusic; }

// 0x24: C_VAR
inline int life_GetCVAR(int idx) { return CVars[idx]; }

// 0x25: STAGE
inline int life_GetSTAGE() { return currentProcessedActorPtr->stage; }

// 0x26: THROWN - check if object was thrown
inline int life_GetTHROWN(int worldObjIdx)
{
    return (ListWorldObjets[worldObjIdx].foundFlag & 0x1000) ? 1 : 0;
}

//////////////////////////////////////////////////////////////////////////////
// Other-actor field accessors (evalVar with 0x8000 object prefix)
// Use when bytecode reads a field of a different world object.
//////////////////////////////////////////////////////////////////////////////

inline int life_ObjGetField(int worldObjIdx, int field)
{
    int actorIdx = ListWorldObjets[worldObjIdx].objIndex;
    if (actorIdx == -1)
    {
        // Only ROOM (0x1E) and STAGE (0x25) accessible when not loaded
        if (field == 0x1E) return ListWorldObjets[worldObjIdx].room;
        if (field == 0x25) return ListWorldObjets[worldObjIdx].stage;
        return 0;
    }
    tObject* obj = &ListObjets[actorIdx];
    switch (field)
    {
    case 0x00: return (obj->COL[0] != -1) ? ListObjets[obj->COL[0]].indexInWorld : -1;
    case 0x01: return obj->HARD_DEC;
    case 0x02: return obj->HARD_COL;
    case 0x03: return (obj->HIT != -1) ? ListObjets[obj->HIT].indexInWorld : -1;
    case 0x04: return (obj->HIT_BY != -1) ? ListObjets[obj->HIT_BY].indexInWorld : -1;
    case 0x05: return obj->ANIM;
    case 0x06: return obj->flagEndAnim;
    case 0x07: return obj->frame;
    case 0x08: return obj->END_FRAME;
    case 0x09: return obj->bodyNum;
    case 0x0A: return obj->MARK;
    case 0x0B: return obj->trackNumber;
    case 0x0C: return evalChrono(&obj->CHRONO) / 60;
    case 0x0D: return evalChrono(&obj->ROOM_CHRONO) / 60;
    case 0x0F: return (obj->COL_BY != -1) ? ListObjets[obj->COL_BY].indexInWorld : -1;
    case 0x11: return action;
    case 0x16: return obj->alpha;
    case 0x17: return obj->beta;
    case 0x18: return obj->gamma;
    case 0x1A: return obj->hitForce;
    case 0x1D: return obj->falling;
    case 0x1E: return obj->room;
    case 0x1F: return obj->life;
    case 0x21: return obj->roomY;
    case 0x25: return obj->stage;
    default: return 0;
    }
}

//////////////////////////////////////////////////////////////////////////////
// Target actor scope - RAII helper for opcodes that target another actor
// Temporarily swaps currentProcessedActorPtr/Idx like the bytecode interpreter
//////////////////////////////////////////////////////////////////////////////

struct LifeTargetScope
{
    int savedIdx;
    tObject* savedPtr;
    int worldObjIdx;
    bool actorLoaded;

    LifeTargetScope(int worldObj) : worldObjIdx(worldObj)
    {
        savedIdx = currentProcessedActorIdx;
        savedPtr = currentProcessedActorPtr;
        int objIdx = ListWorldObjets[worldObj].objIndex;
        actorLoaded = (objIdx != -1);
        if (actorLoaded)
        {
            currentProcessedActorIdx = objIdx;
            currentProcessedActorPtr = &ListObjets[objIdx];
        }
    }

    ~LifeTargetScope()
    {
        currentProcessedActorIdx = savedIdx;
        currentProcessedActorPtr = savedPtr;
    }
};

//////////////////////////////////////////////////////////////////////////////
// Command helpers - map bytecode commands to engine function calls
// These operate on currentProcessedActorPtr (the "self" actor).
//////////////////////////////////////////////////////////////////////////////

// LM_ANIM_ONCE: Play animation once, then switch to animInfo
inline void life_AnimOnce(int anim, int animInfo)
{
    if (anim == -1)
    {
        currentProcessedActorPtr->ANIM = -1;
        currentProcessedActorPtr->newAnim = -2;
    }
    else
    {
        InitAnim(anim, ANIM_ONCE, animInfo);
    }
}

// LM_ANIM_REPEAT: Play animation in a loop
inline void life_AnimRepeat(int anim)
{
    InitAnim(anim, ANIM_REPEAT, -1);
}

// LM_ANIM_ALL_ONCE: Play animation once, uninterruptable
inline void life_AnimAllOnce(int anim, int animInfo)
{
    InitAnim(anim, ANIM_UNINTERRUPTABLE, animInfo);
}

// LM_ANIM_RESET: Play animation once with reset
inline void life_AnimReset(int anim, int animInfo)
{
    if (anim == -1)
    {
        currentProcessedActorPtr->ANIM = -1;
        currentProcessedActorPtr->newAnim = -2;
    }
    else
    {
        InitAnim(anim, ANIM_ONCE | ANIM_RESET, animInfo);
    }
}

// LM_ANIM_MOVE: Configure movement animations
inline void life_AnimMove(int stand, int walk, int run, int stop, int walkBack, int turnRight, int turnLeft)
{
    animMove(stand, walk, run, stop, walkBack, turnRight, turnLeft);
}

// LM_ANIM_SAMPLE: Play sample on specific animation frame
inline void life_AnimSample(int sampleNum, int animNum, int frameNum)
{
    if (currentProcessedActorPtr->END_FRAME != 0)
    {
        if (currentProcessedActorPtr->ANIM == animNum)
        {
            if (currentProcessedActorPtr->frame == frameNum)
            {
                playSound(sampleNum);
            }
        }
    }
}

// LM_BODY: Change body (model), with animation sync
inline void life_Body(int bodyNum)
{
    ListWorldObjets[currentProcessedActorPtr->indexInWorld].body = bodyNum;

    if (currentProcessedActorPtr->bodyNum != bodyNum)
    {
        currentProcessedActorPtr->bodyNum = bodyNum;

        if (currentProcessedActorPtr->objectType & AF_ANIMATED)
        {
            if ((currentProcessedActorPtr->ANIM != -1) && (currentProcessedActorPtr->bodyNum != -1))
            {
                sAnimation* pAnim = HQR_Get(HQ_Anims, currentProcessedActorPtr->ANIM);
                sBody* pBody = HQR_Get(HQ_Bodys, currentProcessedActorPtr->bodyNum);
                SetInterAnimObjet(currentProcessedActorPtr->frame, pAnim, pBody);
            }
        }
        else
        {
            FlagInitView = 1;
        }
    }
}

// LM_BODY_RESET: Change body and reset animation
inline void life_BodyReset(int bodyNum, int animNum)
{
    ListWorldObjets[currentProcessedActorPtr->indexInWorld].body = bodyNum;
    ListWorldObjets[currentProcessedActorPtr->indexInWorld].anim = animNum;

    currentProcessedActorPtr->bodyNum = bodyNum;

    if (currentProcessedActorPtr->objectType & AF_ANIMATED)
    {
        sAnimation* pAnim = HQR_Get(HQ_Anims, currentProcessedActorPtr->ANIM);
        sBody* pBody = HQR_Get(HQ_Bodys, currentProcessedActorPtr->bodyNum);
        SetAnimObjet(0, pAnim, pBody);
        InitAnim(animNum, 4, -1);
    }
    else
    {
        FlagInitView = 1;
    }
}

// LM_MOVE: Start track movement
inline void life_Move(int trackMode, int trackNumber)
{
    InitDeplacement(trackMode, trackNumber);
}

// LM_DO_MOVE: Process current track
inline void life_DoMove()
{
    processTrack();
}

// LM_CONTINUE_TRACK: Skip marker in track
inline void life_ContinueTrack()
{
    if (currentProcessedActorPtr->trackNumber < 0) return;
    char* ptr = HQR_Get(listTrack, currentProcessedActorPtr->trackNumber);
    if (!ptr) return;
    ptr += currentProcessedActorPtr->positionInTrack * 2;
    if (*(s16*)ptr == 5)
        currentProcessedActorPtr->positionInTrack++;
}

// LM_MANUAL_ROT: Handle manual rotation input
inline void life_ManualRot()
{
    GereManualRot(240); // AITD1 uses 240
}

// LM_RESET_MOVE_MANUAL: Reset manual movement state
void resetRotateParam(void);
inline void life_ResetMoveManual()
{
    resetRotateParam();
}

// LM_SET_BETA: Smooth rotate to target beta angle
inline void life_SetBeta(int targetBeta, int speed)
{
    if (currentProcessedActorPtr->beta != targetBeta)
    {
        if (currentProcessedActorPtr->rotate.numSteps == 0 ||
            currentProcessedActorPtr->rotate.endValue != targetBeta)
        {
            InitRealValue(currentProcessedActorPtr->beta, targetBeta, speed,
                &currentProcessedActorPtr->rotate);
        }
        currentProcessedActorPtr->beta = updateActorRotation(&currentProcessedActorPtr->rotate);
    }
}

// LM_SET_ALPHA: Smooth rotate to target alpha angle
inline void life_SetAlpha(int targetAlpha, int speed)
{
    if (currentProcessedActorPtr->alpha != targetAlpha)
    {
        if (currentProcessedActorPtr->rotate.numSteps == 0 ||
            targetAlpha != currentProcessedActorPtr->rotate.endValue)
        {
            InitRealValue(currentProcessedActorPtr->alpha, targetAlpha, speed,
                &currentProcessedActorPtr->rotate);
        }
        currentProcessedActorPtr->alpha = updateActorRotation(&currentProcessedActorPtr->rotate);
    }
}

// LM_STOP_BETA: Stop rotation
inline void life_StopBeta()
{
    currentProcessedActorPtr->rotate.numSteps = 0;
}

// LM_ANGLE: Set rotation angles directly
inline void life_Angle(int alpha, int beta, int gamma)
{
    currentProcessedActorPtr->alpha = alpha;
    currentProcessedActorPtr->beta = beta;
    currentProcessedActorPtr->gamma = gamma;
}

// LM_COPY_ANGLE: Copy angles from another actor
inline void life_CopyAngle(int worldObjIdx)
{
    int localIdx = ListWorldObjets[worldObjIdx].objIndex;
    if (localIdx == -1)
    {
        currentProcessedActorPtr->alpha = ListWorldObjets[worldObjIdx].alpha;
        currentProcessedActorPtr->beta = ListWorldObjets[worldObjIdx].beta;
        currentProcessedActorPtr->gamma = ListWorldObjets[worldObjIdx].gamma;
    }
    else
    {
        currentProcessedActorPtr->alpha = ListObjets[localIdx].alpha;
        currentProcessedActorPtr->beta = ListObjets[localIdx].beta;
        currentProcessedActorPtr->gamma = ListObjets[localIdx].gamma;
    }
}

// LM_STAGE: Change floor/room/position
inline void life_Stage(int newStage, int newRoom, int x, int y, int z)
{
    setStage(newStage, newRoom, x, y, z);
}

// LM_LIFE: Change this actor's life script
inline void life_SetLife(int newLife)
{
    currentProcessedActorPtr->life = newLife;
}

// LM_STAGE_LIFE: Set floor life script
inline void life_StageLife(int stageLife)
{
    ListWorldObjets[currentProcessedActorPtr->indexInWorld].floorLife = stageLife;
}

// LM_LIFE_MODE: Change life mode
inline void life_LifeMode(int mode)
{
    int currentMode = currentProcessedActorPtr->lifeMode;
    if (mode != currentMode)
        currentProcessedActorPtr->lifeMode = mode;
}

// LM_DELETE: Delete an object
inline void life_Delete(int worldObjIdx)
{
    deleteObject(worldObjIdx);
    if (ListWorldObjets[worldObjIdx].foundBody != -1)
    {
        ListWorldObjets[worldObjIdx].foundFlag &= ~0x8000;
        ListWorldObjets[worldObjIdx].foundFlag |= 0x4000;
    }
}

// LM_TYPE: Set actor type flags
inline void life_Type(int typeFlags)
{
    typeFlags &= AF_MASK;
    currentProcessedActorPtr->objectType = (currentProcessedActorPtr->objectType & ~AF_MASK) + typeFlags;
}

// LM_TEST_COL: Enable/disable collision testing
inline void life_TestCol(int enable)
{
    if (enable)
        currentProcessedActorPtr->dynFlags |= 1;
    else
        currentProcessedActorPtr->dynFlags &= ~1;
}

// LM_HIT: Melee attack
inline void life_Hit(int animNum, int startFrame, int groupNum, int hitBoxSize, int hitForce, int nextAnim)
{
    hit(animNum, startFrame, groupNum, hitBoxSize, hitForce, nextAnim);
}

// LM_FIRE: Ranged attack
inline void life_Fire(int fireAnim, int shootFrame, int emitPoint, int zvSize, int hitForce, int nextAnim)
{
    fire(fireAnim, shootFrame, emitPoint, zvSize, hitForce, nextAnim);
}

// LM_HIT_OBJECT: Enable constant hit-on-contact
inline void life_HitObject(int flags, int force)
{
    currentProcessedActorPtr->animActionType = 8;
    currentProcessedActorPtr->animActionParam = flags;
    currentProcessedActorPtr->hitForce = force;
    currentProcessedActorPtr->hotPointID = -1;
}

// LM_STOP_HIT_OBJECT: Cancel hit-on-contact
inline void life_StopHitObject()
{
    if (currentProcessedActorPtr->animActionType == 8)
    {
        currentProcessedActorPtr->animActionType = 0;
        currentProcessedActorPtr->animActionParam = 0;
        currentProcessedActorPtr->hitForce = 0;
        currentProcessedActorPtr->hotPointID = -1;
    }
}

// LM_THROW: Throw an object
inline void life_Throw(int animThrow, int frameThrow, int param, int objToThrowIdx, int throwRotated, int throwForce, int animNext)
{
    throwObj(animThrow, frameThrow, param, objToThrowIdx, throwRotated, throwForce, animNext);
}

// LM_FOUND: Register object as found/collectible
inline void life_Found(int worldObjIdx)
{
    FoundObjet(worldObjIdx, 1); // AITD1 always uses mode 1
}

// LM_TAKE: Take (pick up) an object
inline void life_Take(int worldObjIdx)
{
    take(worldObjIdx);
}

// LM_IN_HAND: Set object in hand
inline void life_InHand(int objIdx)
{
    inHandTable[currentInventory] = objIdx;
}

// LM_DROP: Drop an object
void drop(int worldIdx, int worldSource);
inline void life_Drop(int worldIdx, int worldSource)
{
    drop(worldIdx, worldSource);
}

// LM_PUT: Place an object at coordinates
inline void life_Put(int idx, int x, int y, int z, int room, int stage, int alpha, int beta, int gamma)
{
    put(x, y, z, room, stage, alpha, beta, gamma, idx);
}

// LM_PUT_AT: Place object at another object's position
inline void life_PutAt(int objIdx1, int objIdx2)
{
    PutAtObjet(objIdx1, objIdx2);
}

// LM_SPECIAL: Trigger special effect (evaporate, blood, smoke)
int InitSpecialObjet(int mode, int X, int Y, int Z, int stage, int room, int alpha, int beta, int gamma, ZVStruct* zvPtr);
inline void life_Special(int type)
{
    switch (type)
    {
    case 0: // evaporate
        InitSpecialObjet(0,
            currentProcessedActorPtr->roomX + currentProcessedActorPtr->stepX,
            currentProcessedActorPtr->roomY + currentProcessedActorPtr->stepY,
            currentProcessedActorPtr->roomZ + currentProcessedActorPtr->stepZ,
            currentProcessedActorPtr->stage, currentProcessedActorPtr->room,
            currentProcessedActorPtr->alpha, currentProcessedActorPtr->beta,
            currentProcessedActorPtr->gamma, &currentProcessedActorPtr->zv);
        break;
    case 1: // blood
        InitSpecialObjet(1,
            currentProcessedActorPtr->roomX + currentProcessedActorPtr->stepX + currentProcessedActorPtr->hotPoint.x,
            currentProcessedActorPtr->roomY + currentProcessedActorPtr->stepY + currentProcessedActorPtr->hotPoint.y,
            currentProcessedActorPtr->roomZ + currentProcessedActorPtr->stepZ + currentProcessedActorPtr->hotPoint.z,
            currentProcessedActorPtr->stage, currentProcessedActorPtr->room,
            0, -currentProcessedActorPtr->beta, 0, NULL);
        break;
    case 4: // smoke
        InitSpecialObjet(4,
            currentProcessedActorPtr->roomX + currentProcessedActorPtr->stepX,
            currentProcessedActorPtr->roomY + currentProcessedActorPtr->stepY,
            currentProcessedActorPtr->roomZ + currentProcessedActorPtr->stepZ,
            currentProcessedActorPtr->stage, currentProcessedActorPtr->room,
            currentProcessedActorPtr->alpha, currentProcessedActorPtr->beta,
            currentProcessedActorPtr->gamma, &currentProcessedActorPtr->zv);
        break;
    }
}

// ZV helpers
inline void life_DoRealZv() { doRealZv(currentProcessedActorPtr); }

inline void life_DoRotZv()
{
    getZvRot(HQR_Get(HQ_Bodys, currentProcessedActorPtr->bodyNum), &currentProcessedActorPtr->zv,
        currentProcessedActorPtr->alpha, currentProcessedActorPtr->beta, currentProcessedActorPtr->gamma);
    currentProcessedActorPtr->zv.ZVX1 += currentProcessedActorPtr->roomX;
    currentProcessedActorPtr->zv.ZVX2 += currentProcessedActorPtr->roomX;
    currentProcessedActorPtr->zv.ZVY1 += currentProcessedActorPtr->roomY;
    currentProcessedActorPtr->zv.ZVY2 += currentProcessedActorPtr->roomY;
    currentProcessedActorPtr->zv.ZVZ1 += currentProcessedActorPtr->roomZ;
    currentProcessedActorPtr->zv.ZVZ2 += currentProcessedActorPtr->roomZ;
}

inline void life_DoMaxZv()
{
    getZvMax(HQR_Get(HQ_Bodys, currentProcessedActorPtr->bodyNum), &currentProcessedActorPtr->zv);
    currentProcessedActorPtr->zv.ZVX1 += currentProcessedActorPtr->roomX;
    currentProcessedActorPtr->zv.ZVX2 += currentProcessedActorPtr->roomX;
    currentProcessedActorPtr->zv.ZVY1 += currentProcessedActorPtr->roomY;
    currentProcessedActorPtr->zv.ZVY2 += currentProcessedActorPtr->roomY;
    currentProcessedActorPtr->zv.ZVZ1 += currentProcessedActorPtr->roomZ;
    currentProcessedActorPtr->zv.ZVZ2 += currentProcessedActorPtr->roomZ;
}

inline void life_DoCarreZv()
{
    getZvCube(HQR_Get(HQ_Bodys, currentProcessedActorPtr->bodyNum), &currentProcessedActorPtr->zv);
    currentProcessedActorPtr->zv.ZVX1 += currentProcessedActorPtr->roomX;
    currentProcessedActorPtr->zv.ZVX2 += currentProcessedActorPtr->roomX;
    currentProcessedActorPtr->zv.ZVY1 += currentProcessedActorPtr->roomY;
    currentProcessedActorPtr->zv.ZVY2 += currentProcessedActorPtr->roomY;
    currentProcessedActorPtr->zv.ZVZ1 += currentProcessedActorPtr->roomZ;
    currentProcessedActorPtr->zv.ZVZ2 += currentProcessedActorPtr->roomZ;
}

inline void life_DefZv(int x1, int x2, int y1, int y2, int z1, int z2)
{
    currentProcessedActorPtr->zv.ZVX1 = currentProcessedActorPtr->roomX + x1 + currentProcessedActorPtr->stepX;
    currentProcessedActorPtr->zv.ZVX2 = currentProcessedActorPtr->roomX + x2 + currentProcessedActorPtr->stepX;
    currentProcessedActorPtr->zv.ZVY1 = currentProcessedActorPtr->roomY + y1 + currentProcessedActorPtr->stepY;
    currentProcessedActorPtr->zv.ZVY2 = currentProcessedActorPtr->roomY + y2 + currentProcessedActorPtr->stepY;
    currentProcessedActorPtr->zv.ZVZ1 = currentProcessedActorPtr->roomZ + z1 + currentProcessedActorPtr->stepZ;
    currentProcessedActorPtr->zv.ZVZ2 = currentProcessedActorPtr->roomZ + z2 + currentProcessedActorPtr->stepZ;
}

inline void life_DefAbsZv(int x1, int x2, int y1, int y2, int z1, int z2)
{
    currentProcessedActorPtr->zv.ZVX1 = x1;
    currentProcessedActorPtr->zv.ZVX2 = x2;
    currentProcessedActorPtr->zv.ZVY1 = y1;
    currentProcessedActorPtr->zv.ZVY2 = y2;
    currentProcessedActorPtr->zv.ZVZ1 = z1;
    currentProcessedActorPtr->zv.ZVZ2 = z2;
}

// LM_GET_HARD_CLIP
inline void life_GetHardClip() { getHardClip(); }

// LM_UP_COOR_Y: Start falling
inline void life_UpCoorY()
{
    InitRealValue(0, -2000, -1, &currentProcessedActorPtr->YHandler);
}

// LM_SPEED: Set movement speed
inline void life_Speed(int speed)
{
    if (currentProcessedActorPtr->speed != speed)
    {
        if (speed == 0)
        {
            currentProcessedActorPtr->speedChange.numSteps = 0;
            currentProcessedActorPtr->speed = 0;
        }
        else
        {
            InitRealValue(0, speed, 30, &currentProcessedActorPtr->speedChange);
        }
    }
}

// LM_START_CHRONO
inline void life_StartChrono() { startChrono(&currentProcessedActorPtr->CHRONO); }

// Sound helpers
inline void life_Sample(int sampleNum) { playSound(sampleNum); }
inline void life_RepSample(int sampleNum) { playSoundLooping(sampleNum); }
inline void life_StopSample() { osystem_stopSample(); }
inline void life_SampleThen(int sample, int nextSamp)
{
    playSound(sample);
    nextSample = nextSamp;
}
inline void life_SampleThenRepeat(int sample, int nextSamp)
{
    playSound(sample);
    nextSample = nextSamp | 0x4000;
}

// Music helpers
inline void life_Music(int musicIdx) { playMusic(musicIdx); }
inline void life_NextMusic(int musicIdx)
{
    if (currentMusic == -1)
        playMusic(musicIdx);
    else
        nextMusic = musicIdx;
}
inline void life_FadeMusic(int musicIdx)
{
    extern unsigned int musicChrono;
    if (currentMusic != -1)
    {
        fadeMusic(0, 0, 0x8000);
        startChrono(&musicChrono);
        currentMusic = -2;
        nextMusic = musicIdx;
    }
    else
    {
        playMusic(musicIdx);
    }
}

// LM_LIGHT
inline void life_Light(int val)
{
    int lightVal = 2 - (val << 1);
    if (!CVars[getCVarsIdx((enumCVars)KILLED_SORCERER)])
    {
        extern s16 lightOff;
        extern int lightVar2;
        if (lightOff != lightVal)
        {
            lightOff = lightVal;
            lightVar2 = 1;
        }
    }
}

// LM_SHAKING
inline void life_Shaking(int amplitude)
{
    extern s16 shakingAmplitude;
    shakingAmplitude = amplitude;
    if (amplitude == 0)
        stopShaking();
    else
        setupShaking(amplitude);
}

// LM_WATER
inline void life_Water(int val)
{
    extern s16 shakeVar1;
    shakeVar1 = val;
    if (val > 0) setupShaking(val * 10);
}

// LM_INVENTORY
inline void life_Inventory(int allow)
{
    extern s16 statusScreenAllowed;
    statusScreenAllowed = allow;
}

// LM_MESSAGE
inline void life_Message(int messageIdx) { makeMessage(messageIdx); }

// LM_FOUND_NAME, FOUND_BODY, FOUND_FLAG, FOUND_WEIGHT, FOUND_LIFE (self-actor versions)
inline void life_FoundName(int name) { ListWorldObjets[currentProcessedActorPtr->indexInWorld].foundName = name; }
inline void life_FoundBody(int body) { ListWorldObjets[currentProcessedActorPtr->indexInWorld].foundBody = body; }
inline void life_FoundFlag(int flag)
{
    ListWorldObjets[currentProcessedActorPtr->indexInWorld].foundFlag &= 0xE000;
    ListWorldObjets[currentProcessedActorPtr->indexInWorld].foundFlag |= flag;
}
inline void life_FoundWeight(int weight) { ListWorldObjets[currentProcessedActorPtr->indexInWorld].positionInTrack = weight; }
inline void life_FoundLife(int life) { ListWorldObjets[currentProcessedActorPtr->indexInWorld].foundLife = life; }

// LM_READ: Read a book/document
void readBook(int index, int type, int vocIndex);
inline void life_Read(int type, int index, int vocIndex)
{
    FadeOutPhys(0x20, 0);
    readBook(index + 1, type, vocIndex);
    FadeOutPhys(4, 0);
    FlagInitView = 2;
}

// LM_CAMERA_TARGET: Change camera focus
inline void life_CameraTarget(int worldObjIdx)
{
    if (worldObjIdx != currentWorldTarget)
    {
        int objIdx = ListWorldObjets[worldObjIdx].objIndex;
        if (objIdx != -1)
        {
            currentWorldTarget = worldObjIdx;
            currentCameraTargetActor = objIdx;
            int newRoom = ListObjets[currentCameraTargetActor].room;
            if (newRoom != currentRoom)
            {
                extern int FlagChangeSalle;
                extern s16 NewNumSalle;
                FlagChangeSalle = 1;
                NewNumSalle = newRoom;
            }
        }
        else // not loaded, may need floor change
        {
            currentWorldTarget = worldObjIdx;
            if (ListWorldObjets[worldObjIdx].stage != g_currentFloor)
            {
                extern int FlagChangeEtage;
                extern s16 NewNumEtage;
                extern s16 NewNumSalle;
                FlagChangeEtage = 1;
                NewNumEtage = ListWorldObjets[worldObjIdx].stage;
                NewNumSalle = ListWorldObjets[worldObjIdx].room;
            }
            else if (currentRoom != ListWorldObjets[worldObjIdx].room)
            {
                extern int FlagChangeSalle;
                extern s16 NewNumSalle;
                FlagChangeSalle = 1;
                NewNumSalle = ListWorldObjets[worldObjIdx].room;
            }
        }
    }
}

// LM_GAME_OVER
inline void life_GameOver()
{
    extern unsigned int musicChrono;
    extern s16 FlagGameOver;
    osystem_startLetterbox();
    fadeMusic(0, 0, 0x8000);
    startChrono(&musicChrono);
    while (evalChrono(&musicChrono) < 120)
    {
        process_events();
        osystem_updateLetterbox();
    }
    FlagGameOver = 1;
}

// LM_END_SEQUENCE
inline void life_EndSequence()
{
    osystem_endLetterbox();
}

//////////////////////////////////////////////////////////////////////////////
// Not-in-floor command helpers
// For targeted commands when the actor is NOT loaded in the current floor.
// These modify ListWorldObjets[] directly instead of ListObjets[].
//////////////////////////////////////////////////////////////////////////////

inline void life_WorldObj_Body(int worldIdx, int bodyNum)
{
    ListWorldObjets[worldIdx].body = bodyNum;
}

inline void life_WorldObj_BodyReset(int worldIdx, int bodyNum, int animNum)
{
    ListWorldObjets[worldIdx].body = bodyNum;
    ListWorldObjets[worldIdx].anim = animNum;
}

inline void life_WorldObj_AnimOnce(int worldIdx, int anim, int animInfo)
{
    ListWorldObjets[worldIdx].anim = anim;
    ListWorldObjets[worldIdx].animInfo = animInfo;
    ListWorldObjets[worldIdx].animType = ANIM_ONCE;
}

inline void life_WorldObj_AnimRepeat(int worldIdx, int anim)
{
    ListWorldObjets[worldIdx].anim = anim;
    ListWorldObjets[worldIdx].animInfo = -1;
    ListWorldObjets[worldIdx].animType = ANIM_REPEAT;
}

inline void life_WorldObj_AnimAllOnce(int worldIdx, int anim, int animInfo)
{
    ListWorldObjets[worldIdx].anim = anim;
    ListWorldObjets[worldIdx].animInfo = animInfo;
    ListWorldObjets[worldIdx].animType = ANIM_ONCE | ANIM_UNINTERRUPTABLE;
}

inline void life_WorldObj_Move(int worldIdx, int trackMode, int trackNumber)
{
    ListWorldObjets[worldIdx].trackMode = trackMode;
    ListWorldObjets[worldIdx].trackNumber = trackNumber;
    ListWorldObjets[worldIdx].positionInTrack = 0;
}

inline void life_WorldObj_Angle(int worldIdx, int alpha, int beta, int gamma)
{
    ListWorldObjets[worldIdx].alpha = alpha;
    ListWorldObjets[worldIdx].beta = beta;
    ListWorldObjets[worldIdx].gamma = gamma;
}

inline void life_WorldObj_Stage(int worldIdx, int stage, int room, int x, int y, int z)
{
    ListWorldObjets[worldIdx].stage = stage;
    ListWorldObjets[worldIdx].room = room;
    ListWorldObjets[worldIdx].x = x;
    ListWorldObjets[worldIdx].y = y;
    ListWorldObjets[worldIdx].z = z;
}

inline void life_WorldObj_TestCol(int worldIdx, int enable)
{
    if (enable)
        ListWorldObjets[worldIdx].flags |= 0x20;
    else
        ListWorldObjets[worldIdx].flags &= 0xFFDF;
}

inline void life_WorldObj_Life(int worldIdx, int life)
{
    ListWorldObjets[worldIdx].life = life;
}

inline void life_WorldObj_LifeMode(int worldIdx, int mode)
{
    if (mode != ListWorldObjets[worldIdx].lifeMode)
        ListWorldObjets[worldIdx].lifeMode = mode;
}

inline void life_WorldObj_Type(int worldIdx, int typeFlags)
{
    int masked = typeFlags & TYPE_MASK;
    ListWorldObjets[worldIdx].flags = (ListWorldObjets[worldIdx].flags & (~TYPE_MASK)) + masked;
}

inline void life_WorldObj_FoundName(int worldIdx, int name)
{
    ListWorldObjets[worldIdx].foundName = name;
}

inline void life_WorldObj_FoundBody(int worldIdx, int body)
{
    ListWorldObjets[worldIdx].foundBody = body;
}

inline void life_WorldObj_FoundFlag(int worldIdx, int flag)
{
    ListWorldObjets[worldIdx].foundFlag &= 0xE000;
    ListWorldObjets[worldIdx].foundFlag |= flag;
}

inline void life_WorldObj_FoundWeight(int worldIdx, int weight)
{
    ListWorldObjets[worldIdx].positionInTrack = weight;
}

inline void life_WorldObj_FoundLife(int worldIdx, int life)
{
    ListWorldObjets[worldIdx].foundLife = life;
}

//////////////////////////////////////////////////////////////////////////////
// Convenience: Execute a command targeting a world object (handles both
// loaded and not-loaded cases automatically, like the bytecode interpreter)
//////////////////////////////////////////////////////////////////////////////

// Target a command - if actor is loaded, uses self-variant with temp actor swap;
// if not loaded, uses WorldObj_ variant. Returns true if actor was loaded.
#define LIFE_TARGET_BEGIN(worldIdx) \
    { \
        int _life_target_worldIdx = (worldIdx); \
        int _life_target_objIdx = ListWorldObjets[_life_target_worldIdx].objIndex; \
        bool _life_target_loaded = (_life_target_objIdx != -1); \
        if (_life_target_loaded) { \
            LifeTargetScope _scope(_life_target_worldIdx);

#define LIFE_TARGET_ELSE \
        } else {

#define LIFE_TARGET_END \
        } \
    }

//////////////////////////////////////////////////////////////////////////////
// Additional helper functions for TODO opcodes
// These implement the remaining bytecode operations not covered above.
//////////////////////////////////////////////////////////////////////////////

// No-op opcodes (do nothing in AITD1)
inline void life_RndFreq(int /*val*/) { /* no-op */ }
inline void life_Pluie(int /*val*/) { /* no-op */ }
inline void life_Protect() { /* no-op - copy protection stub */ }
inline void life_Camera() { /* no-op in AITD1 */ }
inline void life_SetInventory(int /*val*/) { /* no-op */ }
inline void life_DelInventory(int /*val*/) { /* no-op */ }
inline void life_DoNormalZv() { /* no-op - not used in AITD1 */ }
inline void life_GetMatrice(int /*val*/) { /* AITD2 only - no-op */ }
inline void life_FireUpDown(int, int, int, int, int, int) { /* AITD3 only - no-op */ }

// Simple one-liner opcodes
void life_SetGround(int level);
void life_MessageValue(int msg, int /*val*/);
void life_CallInventory();
void life_WaitGameOver();
void life_PlaySequence(int sequenceIdx, int fadeIn, int fadeOut);
void life_DefSequenceSample(int numParams, const int* frameSamplePairs);
void life_AnimHybrideOnce(int anim, int body);
void life_AnimHybrideRepeat(int anim, int body);
void life_2dAnimSample(int, int, int);
void life_Picture(int pictureIndex, int delay, int sampleId);
void life_ReadOnPicture(int picIdx, int textIdx, int x, int y, int width, int height, int arg7, int arg8, int vocIndex);

