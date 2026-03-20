///////////////////////////////////////////////////////////////////////////////
// Alone In The Dark Re-Haunted
// Copyright (C) 2026 Infogrames / Spacefarer Retro Remasters LLC
// Author: Jake Jackson (jake@spacefarergames.com)
//
// System pause menu declarations
///////////////////////////////////////////////////////////////////////////////

#ifndef _SYSTEMMENU_H_
#define _SYSTEMMENU_H_

void processSystemMenu(void);
void scaleDownImage(int destX, int destY, char* sourceBuffer);
void scaleDownImageHD(int destX, int destY, unsigned char* sourceBuffer, int srcWidth, int srcHeight);

#endif

