///////////////////////////////////////////////////////////////////////////////
// Alone In The Dark Re-Haunted
// Copyright (C) 2026 Infogrames / Spacefarer Retro Remasters LLC
// Based on FITD by yaz0r, Re-haunted is released under GPL
// Author: Jake Jackson (jake@spacefarergames.com)
//
// 2D animation declarations
///////////////////////////////////////////////////////////////////////////////

#pragma once

#include "hybrid.h"

struct sAnim2d {
    uint16_t frame;
    uint32_t time;
    uint16_t flagEndInterAnim;
    std::vector<sHybrid_Anim>::iterator pAnim;
    sHybrid_Animation* pAnimation;
    int16_t ScreenXMin;
    int16_t ScreenYMin;
    int16_t ScreenXMax;
    int16_t ScreenYMax;
};

extern std::array<sAnim2d, 20> TabAnim2d;
extern int NbAnim2D;
extern sHybrid* PtrAnim2D;

void resetAnim2D();
void load2dAnims(int cameraIdx);
void startAnim2d(int index);
void handleAnim2d();