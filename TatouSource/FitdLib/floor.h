///////////////////////////////////////////////////////////////////////////////
// Alone In The Dark Re-Haunted
// Copyright (C) 2026 Infogrames / Spacefarer Retro Remasters LLC
// Based on FITD by yaz0r, Re-haunted is released under GPL
// Author: Jake Jackson (jake@spacefarergames.com)
//
// Floor and level loading declarations
///////////////////////////////////////////////////////////////////////////////

#ifndef _FLOOR_H_
#define _FLOOR_H_

extern std::vector<cameraDataStruct> g_currentFloorCameraData;
extern u32 g_currentFloorRoomRawDataSize;

void LoadEtage(int floorNumber);

#endif

