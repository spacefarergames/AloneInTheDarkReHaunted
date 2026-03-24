///////////////////////////////////////////////////////////////////////////////
// Alone In The Dark Re-Haunted
// Copyright (C) 2026 Infogrames / Spacefarer Retro Remasters LLC
// Based on FITD by yaz0r, Re-haunted is released under GPL
// Author: Jake Jackson (jake@spacefarergames.com)
//
// Zone volume (bounding box) declarations
///////////////////////////////////////////////////////////////////////////////

#ifndef _ZV_H_
#define _ZV_H_
void getZvCube(sBody* bodyPtr, ZVStruct* zvPtr);
void GiveZVObjet(sBody* bodyPtr, ZVStruct* zvPtr);
void getZvMax(sBody* bodyPtr, ZVStruct* zvPtr);
void getZvRot(sBody* bodyPtr, ZVStruct* zvPtr, int alpha, int beta, int gamma);
void makeDefaultZV(ZVStruct* zvPtr);
#endif
