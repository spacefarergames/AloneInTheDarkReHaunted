///////////////////////////////////////////////////////////////////////////////
// Alone In The Dark Re-Haunted
// Copyright (C) 2026 Infogrames / Spacefarer Retro Remasters LLC
// Based on FITD by yaz0r, Re-haunted is released under GPL
// Author: Jake Jackson (jake@spacefarergames.com)
//
// Renderer interface declarations
///////////////////////////////////////////////////////////////////////////////

#ifndef _RENDERER_H_
#define _RENDERER_H_

#include <string>

extern int BBox3D1;
extern int BBox3D2;
extern int BBox3D3;
extern int BBox3D4;

#define NUM_MAX_POINT_IN_POINT_BUFFER 800
#define NUM_MAX_BONES 50

extern std::array<point3dStruct, NUM_MAX_POINT_IN_POINT_BUFFER> pointBuffer;
extern int numOfPoints;

// Track which body's primitives are currently in primTable[]
// External code (like lantern glow detection) can check this to verify
// they're scanning the correct body's primitives, not some other body
extern int g_currentPrimTableBodyNum;

void transformPoint(float* ax, float* bx, float* cx);

int AffObjet(int x, int y, int z, int alpha, int beta, int gamma, sBody* pBody);

// AITD1 cave water (LIFE `water`, opcode 0x4D): while it is on, model vertices below room
// y -600 are not drawn, so anyone standing in the water is cut at the waist. The original
// patches this threshold into its vertex transform (INDARK.EXE 2493:10ef: -600 on,
// 10000 off) and switches it off while the inventory and found-object screens are shown.
extern int g_waterCutSuspended;
struct WaterCutSuspend
{
    WaterCutSuspend() { g_waterCutSuspended++; }
    ~WaterCutSuspend() { g_waterCutSuspended--; }
};

void setCurrentBodyNum(int bodyNum, sBody* pBody, const std::string& hqrName);

struct ModelAtlasData;
void setCurrentAtlas(ModelAtlasData* atlas);

void computeScreenBox(int x, int y, int z, int alpha, int beta, int gamma, sBody* bodyPtr);

void drawBlobShadow(int x, int y, int z, int alpha, int beta, int gamma, sBody* bodyPtr);

void drawPlanarShadow(int x, int y, int z, int alpha, int beta, int gamma, sBody* bodyPtr);

void drawWallPlanarShadow(int x, int y, int z, int alpha, int beta, int gamma, sBody* bodyPtr, int actorRoom);

#endif
