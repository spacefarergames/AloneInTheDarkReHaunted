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

void transformPoint(float* ax, float* bx, float* cx);

int AffObjet(int x, int y, int z, int alpha, int beta, int gamma, sBody* pBody);

void setCurrentBodyNum(int bodyNum, sBody* pBody, const std::string& hqrName);

void computeScreenBox(int x, int y, int z, int alpha, int beta, int gamma, sBody* bodyPtr);

void drawBlobShadow(int x, int y, int z, int alpha, int beta, int gamma, sBody* bodyPtr);

void drawPlanarShadow(int x, int y, int z, int alpha, int beta, int gamma, sBody* bodyPtr);

void drawWallPlanarShadow(int x, int y, int z, int alpha, int beta, int gamma, sBody* bodyPtr, int actorRoom);

#endif
