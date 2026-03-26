///////////////////////////////////////////////////////////////////////////////
// Alone In The Dark Re-Haunted
// Copyright (C) 2026 Infogrames / Spacefarer Retro Remasters LLC
// Based on FITD by yaz0r, Re-haunted is released under GPL
// Author: Jake Jackson (jake@spacefarergames.com)
//
// Animation action processing and combat mechanics
///////////////////////////////////////////////////////////////////////////////

#include "common.h"
#include "consoleLog.h"
#include <stdio.h>

#define		NO_FRAPPE			0
#define		WAIT_FRAPPE_ANIM	1
#define		FRAPPE_OK			2
#define		DONE_FRAPPE			3
#define		WAIT_TIR_ANIM		4
#define		DO_TIR				5
#define		WAIT_ANIM_THROW		6
#define		WAIT_FRAME_THROW	7
#define		HIT_OBJECT			8
#define		THROW_OBJECT		9
#define		WAIT_FRAPPE_FRAME	10

void GereFrappe(void)
{
    switch(currentProcessedActorPtr->animActionType)
    {
    case WAIT_FRAPPE_ANIM:
        if(currentProcessedActorPtr->ANIM == currentProcessedActorPtr->animActionANIM)
        {
            currentProcessedActorPtr->animActionType = WAIT_FRAPPE_FRAME;
        }
        [[fallthrough]];
    case WAIT_FRAPPE_FRAME:
        if (currentProcessedActorPtr->ANIM != currentProcessedActorPtr->animActionANIM)
        {
            currentProcessedActorPtr->animActionType = NO_FRAPPE;
            return;
        }

        if (currentProcessedActorPtr->frame == currentProcessedActorPtr->animActionFRAME)
        {
            currentProcessedActorPtr->animActionType = FRAPPE_OK;
        }
        return;

    case FRAPPE_OK:
        {
            int x;
            int y;
            int z;
            int range;
            int collision;
            int i;
            ZVStruct rangeZv;

            if(currentProcessedActorPtr->ANIM != currentProcessedActorPtr->animActionANIM)
            {
                currentProcessedActorPtr->animActionType = 0;
            }

            x = currentProcessedActorPtr->roomX + currentProcessedActorPtr->hotPoint.x + currentProcessedActorPtr->stepX;
            y = currentProcessedActorPtr->roomY + currentProcessedActorPtr->hotPoint.y + currentProcessedActorPtr->stepY;
            z = currentProcessedActorPtr->roomZ + currentProcessedActorPtr->hotPoint.z + currentProcessedActorPtr->stepZ;

            range = currentProcessedActorPtr->animActionParam;

            rangeZv.ZVX1 = x - range;
            rangeZv.ZVX2 = x + range;
            rangeZv.ZVY1 = y - range;
            rangeZv.ZVY2 = y + range;
            rangeZv.ZVZ1 = z - range;
            rangeZv.ZVZ2 = z + range;

            if (backgroundMode == backgroundModeEnum_3D) {
                drawZv(rangeZv);
            }
            
            //drawProjectedBox(rangeZv.ZVX1,rangeZv.ZVX2,rangeZv.ZVY1,rangeZv.ZVY2,rangeZv.ZVZ1,rangeZv.ZVZ2,60,255);

            collision = CheckObjectCol(currentProcessedActorIdx,&rangeZv);

            for(i=0;i<collision;i++)
            {
                tObject* actorPtr2;

                currentProcessedActorPtr->HIT = currentProcessedActorPtr->COL[i];
                actorPtr2 = &ListObjets[currentProcessedActorPtr->COL[i]];

                actorPtr2->HIT_BY = currentProcessedActorIdx;
                actorPtr2->hitForce = currentProcessedActorPtr->hitForce;

                if(actorPtr2->objectType & AF_ANIMATED)
                {
                    currentProcessedActorPtr->animActionType = 0;
                    return;
                }
            }
            break;
        }
        case 4: // WAIT_TIR_ANIM
        {
            if(currentProcessedActorPtr->ANIM != currentProcessedActorPtr->animActionANIM)
            {
                return;
            }

            if(currentProcessedActorPtr->frame != currentProcessedActorPtr->animActionFRAME)
            {
                return;
            }

            currentProcessedActorPtr->animActionType = 5;

            break;
        }
    case 5: // DO_TIR (instant raycast)
        {
            int touchedActor;

            int x = currentProcessedActorPtr->roomX + currentProcessedActorPtr->hotPoint.x + currentProcessedActorPtr->stepX;
            int y = currentProcessedActorPtr->roomY + currentProcessedActorPtr->hotPoint.y + currentProcessedActorPtr->stepY;
            int z = currentProcessedActorPtr->roomZ + currentProcessedActorPtr->hotPoint.z + currentProcessedActorPtr->stepZ;

            // Create muzzle flash at fire point
            InitSpecialObjet( 3,
                x, y, z,
                currentProcessedActorPtr->stage,
                currentProcessedActorPtr->room,
                0,
                currentProcessedActorPtr->beta,
                0,
                NULL);

            // Instant raycast along aim direction
            touchedActor = checkLineProjectionWithActors(
                currentProcessedActorIdx,
                x, y, z,
                currentProcessedActorPtr->beta - 0x100,
                currentProcessedActorPtr->room,
                currentProcessedActorPtr->animActionParam);

            if(touchedActor >= 0)
            {
                tObject* hitActorPtr = &ListObjets[touchedActor];

                currentProcessedActorPtr->HIT = touchedActor;
                hitActorPtr->HIT_BY = currentProcessedActorIdx;
                hitActorPtr->hitForce = currentProcessedActorPtr->hitForce;

                playSound(CVars[getCVarsIdx((enumCVars)SAMPLE_CHOC)]);

                // Create impact effect at the hit position (returned in animMoveX/Y/Z)
                InitSpecialObjet( 0,
                    animMoveX, animMoveY, animMoveZ,
                    currentProcessedActorPtr->stage,
                    currentProcessedActorPtr->room,
                    0, 0, 0,
                    &hitActorPtr->zv);
            }

            currentProcessedActorPtr->animActionType = 0;
            break;
        }
    case 6: // WAIT_ANIM_THROW
        {
            if(currentProcessedActorPtr->ANIM == currentProcessedActorPtr->animActionANIM)
            {
                int objIdx = currentProcessedActorPtr->animActionParam;

                tWorldObject* objPtr = &ListWorldObjets[objIdx];

                int x = currentProcessedActorPtr->roomX + currentProcessedActorPtr->hotPoint.x + currentProcessedActorPtr->stepX;
                int y = currentProcessedActorPtr->roomY + currentProcessedActorPtr->hotPoint.y + currentProcessedActorPtr->stepY;
                int z = currentProcessedActorPtr->roomZ + currentProcessedActorPtr->hotPoint.z + currentProcessedActorPtr->stepZ;

                ZVStruct rangeZv;

                GiveZVObjet(HQR_Get(HQ_Bodys, objPtr->body),&rangeZv);

                rangeZv.ZVX1 += x;
                rangeZv.ZVX2 += x;
                rangeZv.ZVY1 += y;
                rangeZv.ZVY2 += y;
                rangeZv.ZVZ1 += z;
                rangeZv.ZVZ2 += z;

                if(AsmCheckListCol(&rangeZv, &roomDataTable[currentProcessedActorPtr->room]))
                {
                    currentProcessedActorPtr->animActionType = 0;
                    PutAtObjet(objIdx, currentProcessedActorPtr->indexInWorld);
                }
                else
                {
                    if(currentProcessedActorPtr->frame == currentProcessedActorPtr->animActionFRAME)
                    {
                        currentProcessedActorPtr->animActionType = 7;

                        int x = currentProcessedActorPtr->roomX + currentProcessedActorPtr->hotPoint.x + currentProcessedActorPtr->stepX;
                        int y = currentProcessedActorPtr->roomY + currentProcessedActorPtr->hotPoint.y + currentProcessedActorPtr->stepY;
                        int z = currentProcessedActorPtr->roomZ + currentProcessedActorPtr->hotPoint.z + currentProcessedActorPtr->stepZ;

                        DeleteInventoryObjet(objIdx);

                        objPtr->x = x;
                        objPtr->y = y;
                        objPtr->z = z;

                        objPtr->room = currentProcessedActorPtr->room;
                        objPtr->stage = currentProcessedActorPtr->stage;
                        objPtr->alpha = currentProcessedActorPtr->alpha;
                        objPtr->beta = currentProcessedActorPtr->beta+0x200; 

                        objPtr->foundFlag &= 0xBFFF;
                        objPtr->flags |= 0x85;
                        objPtr->flags &= 0xFFDF;

                        //FlagGenereActiveList = 1;
                    }
                }
            }
            break;
        }
    case 7: // THROW
        {
            int x;
            int y;
            int z;
            int objIdx;
            int actorIdx;
            tObject* actorPtr;

            currentProcessedActorPtr->animActionType = 0;

            x = currentProcessedActorPtr->roomX + currentProcessedActorPtr->hotPoint.x + currentProcessedActorPtr->stepX;
            y = currentProcessedActorPtr->roomY + currentProcessedActorPtr->hotPoint.y + currentProcessedActorPtr->stepY;
            z = currentProcessedActorPtr->roomZ + currentProcessedActorPtr->hotPoint.z + currentProcessedActorPtr->stepZ;

            objIdx = currentProcessedActorPtr->animActionParam;

            actorIdx = ListWorldObjets[objIdx].objIndex;

            if(actorIdx == -1)
                return;

            actorPtr = &ListObjets[actorIdx];

            actorPtr->roomX = x;
            actorPtr->roomY = y;
            actorPtr->roomZ = z;

            GiveZVObjet(HQR_Get(HQ_Bodys,actorPtr->bodyNum),&actorPtr->zv);

            actorPtr->zv.ZVX1 += x;
            actorPtr->zv.ZVX2 += x;
            actorPtr->zv.ZVY1 += y;
            actorPtr->zv.ZVY2 += y;
            actorPtr->zv.ZVZ1 += z;
            actorPtr->zv.ZVZ2 += z;

            actorPtr->objectType |= AF_ANIMATED;
            actorPtr->objectType &= ~AF_BOXIFY;

            ListWorldObjets[objIdx].x = x;
            ListWorldObjets[objIdx].y = y;
            ListWorldObjets[objIdx].z = z;

            ListWorldObjets[objIdx].alpha = currentProcessedActorPtr->indexInWorld; // original thrower

            actorPtr->dynFlags = 0;
            actorPtr->animActionType = 9;
            actorPtr->animActionParam = 100;
            actorPtr->hitForce = currentProcessedActorPtr->hitForce;
            actorPtr->hotPointID = -1;
            actorPtr->speed = 3000;

            InitRealValue(0, actorPtr->speed, 60, &actorPtr->speedChange);

            break;
        }
    case 8: // HIT_OBJ - Projectile collision checking (used by painting axes/arrows)
        {
            // This case handles continuous collision checking for projectiles
            // that use hit_object(range, force) opcode. The script checks actor_collider
            // to see what was hit and handles the damage/despawn in script.

            int x = currentProcessedActorPtr->roomX + currentProcessedActorPtr->stepX;
            int y = currentProcessedActorPtr->roomY + currentProcessedActorPtr->stepY;
            int z = currentProcessedActorPtr->roomZ + currentProcessedActorPtr->stepZ;

            int range = currentProcessedActorPtr->animActionParam;

            ZVStruct rangeZv;
            rangeZv.ZVX1 = x - range;
            rangeZv.ZVX2 = x + range;
            rangeZv.ZVY1 = y - range;
            rangeZv.ZVY2 = y + range;
            rangeZv.ZVZ1 = z - range;
            rangeZv.ZVZ2 = z + range;

            // Check for collisions with other actors
            int collision = CheckObjectCol(currentProcessedActorIdx, &rangeZv);

            // Process collisions - set HIT_BY and hitForce on hit actors
            // The script will check actor_collider (COL[0]) to handle damage/despawn
            for (int i = 0; i < collision; i++)
            {
                int hitActorIdx = currentProcessedActorPtr->COL[i];
                tObject* hitActorPtr = &ListObjets[hitActorIdx];

                // Set damage info on the hit actor
                currentProcessedActorPtr->HIT = hitActorIdx;
                hitActorPtr->HIT_BY = currentProcessedActorIdx;
                hitActorPtr->hitForce = currentProcessedActorPtr->hitForce;
            }

            break;
        }
    case 9: // during throw (handles both thrown objects AND gun projectiles)
        {
            // For thrown objects: indexInWorld points to worldObject
            // For gun projectiles: indexInWorld is -2 (special object), use current actor directly
            tWorldObject* objPtr = nullptr;
            bool isGunProjectile = (currentProcessedActorPtr->indexInWorld == -2);

            if (!isGunProjectile && currentProcessedActorPtr->indexInWorld >= 0)
            {
                objPtr = &ListWorldObjets[currentProcessedActorPtr->indexInWorld];
            }

            ZVStruct rangeZv;
            ZVStruct rangeZv2;
            int xtemp;
            int ytemp;
            int ztemp;
            int x1;
            int x2;
            int x3;
            int y1;
            int y2;
            int z1;
            int z2;
            int z3;
            int step;

            CopyZV(&currentProcessedActorPtr->zv, &rangeZv);
            CopyZV(&currentProcessedActorPtr->zv, &rangeZv2);

            xtemp = currentProcessedActorPtr->roomX + currentProcessedActorPtr->stepX;
            ytemp = currentProcessedActorPtr->roomY + currentProcessedActorPtr->stepY;
            ztemp = currentProcessedActorPtr->roomZ + currentProcessedActorPtr->stepZ;

            rangeZv2.ZVX1 -= xtemp;
            rangeZv2.ZVX2 -= xtemp;
            rangeZv2.ZVY1 -= ytemp;
            rangeZv2.ZVY2 -= ytemp;
            rangeZv2.ZVZ1 -= ztemp;
            rangeZv2.ZVZ2 -= ztemp;

            // For gun projectiles, use current position; for thrown objects, use world object position
            if (isGunProjectile)
            {
                x1 = currentProcessedActorPtr->roomX;
                x2 = currentProcessedActorPtr->roomX;
                x3 = currentProcessedActorPtr->roomX;
                y1 = currentProcessedActorPtr->roomY;
                y2 = currentProcessedActorPtr->roomY;
                z1 = currentProcessedActorPtr->roomZ;
                z2 = currentProcessedActorPtr->roomZ;
                z3 = currentProcessedActorPtr->roomZ;
            }
            else
            {
                x1 = objPtr->x;
                x2 = objPtr->x;
                x3 = objPtr->x;
                y1 = objPtr->y;
                y2 = objPtr->y;
                z1 = objPtr->z;
                z2 = objPtr->z;
                z3 = objPtr->z;
            }

            step = 0;

            animMoveZ = 0;
            animMoveX = 0;

            do
            {
                int collision;
                sceZoneStruct* ptr;

                walkStep(0,-step,currentProcessedActorPtr->beta);
                step += 100;
                x2 = x1 + animMoveX;
                y2 = y1;
                z2 = z1 + animMoveZ;

                CopyZV(&rangeZv2,&rangeZv);

                rangeZv.ZVX1 = x2 - 200;
                rangeZv.ZVX2 = x2 + 200;
                rangeZv.ZVY1 = y2 - 200;
                rangeZv.ZVY2 = y2 + 200;
                rangeZv.ZVZ1 = z2 - 200;
                rangeZv.ZVZ2 = z2 + 200;

                collision =  CheckObjectCol(currentProcessedActorIdx,&rangeZv);

                if(collision)
                {
                    int collision2 = collision;
                    int i;

                    currentProcessedActorPtr->hotPoint.x = 0;
                    currentProcessedActorPtr->hotPoint.y = 0;
                    currentProcessedActorPtr->hotPoint.z = 0;

                    for(i=0;i<collision;i++)
                    {
                        int currentActorCol = currentProcessedActorPtr->COL[i];

                        // For thrown objects: Don't hit the original thrower
                        // For gun projectiles: Don't hit the original shooter (stored in HIT_BY)
                        if (!isGunProjectile && objPtr && ListObjets[currentActorCol].indexInWorld == objPtr->alpha)
                        {
                            collision2--;
                            continue; // Skip thrower, keep traveling
                        }

                        if (isGunProjectile && currentActorCol == currentProcessedActorPtr->HIT_BY)
                        {
                            collision2--;
                            continue; // Skip shooter, keep traveling
                        }

                        // Handle REVERSE_OBJECT (mirrors/teleporters for thrown objects)
                        if (!isGunProjectile && objPtr && ListObjets[currentActorCol].indexInWorld == CVars[getCVarsIdx((enumCVars)REVERSE_OBJECT)])
                        {
                            objPtr->alpha = CVars[getCVarsIdx((enumCVars)REVERSE_OBJECT)];
                            currentProcessedActorPtr->beta += 0x200;
                            xtemp = x3;
                            ztemp = z3;

                            currentProcessedActorPtr->worldX = currentProcessedActorPtr->roomX = x3;
                            currentProcessedActorPtr->worldY = currentProcessedActorPtr->roomY = y1;
                            currentProcessedActorPtr->worldZ = currentProcessedActorPtr->roomZ = z3;

                            currentProcessedActorPtr->stepX = 0;
                            currentProcessedActorPtr->stepZ = 0;

                            CopyZV(&rangeZv2, &rangeZv);

                            rangeZv.ZVX1 += x3;
                            rangeZv.ZVX2 += x3;
                            rangeZv.ZVY1 += y1;
                            rangeZv.ZVY2 += y1;
                            rangeZv.ZVZ1 += z3;
                            rangeZv.ZVZ2 += z3;

                            CopyZV(&rangeZv, &currentProcessedActorPtr->zv);

                            objPtr->x = xtemp;
                            objPtr->y = ytemp;
                            objPtr->z = ztemp;

                            return;

                        }
                        else
                        {
                            tObject* actorPtr;

                            // Hit an actor! Apply damage
                            currentProcessedActorPtr->HIT = currentActorCol;
                            actorPtr = &ListObjets[currentActorCol];
                            actorPtr->HIT_BY = isGunProjectile ? currentProcessedActorPtr->HIT_BY : currentProcessedActorIdx;
                            actorPtr->hitForce = currentProcessedActorPtr->hitForce;

                            // Gun projectiles stop on first hit (bullets don't continue)
                            if (isGunProjectile)
                            {
                                currentProcessedActorPtr->animActionType = 0;
                                currentProcessedActorPtr->life = -1; // Mark for deletion
                                playSound(CVars[getCVarsIdx((enumCVars)SAMPLE_CHOC)]);
                                return;
                            }
                        }
                    }

                    if(collision2)
                    {
                        playSound(CVars[getCVarsIdx((enumCVars)SAMPLE_CHOC)]);

                        if (isGunProjectile)
                        {
                            // Projectile hit something, mark for deletion
                            currentProcessedActorPtr->animActionType = 0;
                            currentProcessedActorPtr->life = -1;
                            return;
                        }
                        else
                        {
                            throwStoppedAt(x3,z3);
                            return;
                        }
                    }
                }

                ptr = processActor2Sub(x2,y2,z2, &roomDataTable[currentProcessedActorPtr->room]);

                if(ptr)
                {
                    if(ptr->type == 0 || ptr->type == 10)
                    {
                        playSound(CVars[getCVarsIdx((enumCVars)SAMPLE_CHOC)]);

                        if (isGunProjectile)
                        {
                            currentProcessedActorPtr->animActionType = 0;
                            currentProcessedActorPtr->life = -1;
                            return;
                        }
                        else
                        {
                            throwStoppedAt(x3,z3);
                            return;
                        }
                    }
                }

                if(AsmCheckListCol(&rangeZv, &roomDataTable[currentProcessedActorPtr->room]))
                {
                    currentProcessedActorPtr->hotPoint.x = 0;
                    currentProcessedActorPtr->hotPoint.y = 0;
                    currentProcessedActorPtr->hotPoint.z = 0;

                    playSound(CVars[getCVarsIdx((enumCVars)SAMPLE_CHOC)]);

                    if (isGunProjectile)
                    {
                        currentProcessedActorPtr->animActionType = 0;
                        currentProcessedActorPtr->life = -1;
                        return;
                    }
                    else
                    {
                        throwStoppedAt(x3,z3);
                        return;
                    }
                }
            }while(   currentProcessedActorPtr->zv.ZVX1 - 100 > x2 ||
                currentProcessedActorPtr->zv.ZVX2 + 100 < x2 ||
                currentProcessedActorPtr->zv.ZVZ1 - 100 > z2 ||
                currentProcessedActorPtr->zv.ZVZ2 + 100 < z2 );

            if (objPtr)
            {
                objPtr->x = xtemp;
                objPtr->y = ytemp;
                objPtr->z = ztemp;
            }

            break;
        }
#ifdef FITD_DEBUGGER
    default:
        {
            printf(AACT_WARN "Unsupported processAnimAction type %d" CON_RESET "\n",currentProcessedActorPtr->animActionType);
            break;
        }
#endif
    }
}
