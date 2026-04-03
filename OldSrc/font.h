///////////////////////////////////////////////////////////////////////////////
// Alone In The Dark Re-Haunted
// Copyright (C) 2026 Infogrames / Spacefarer Retro Remasters LLC
// Based on FITD by yaz0r, Re-haunted is released under GPL
// Author: Jake Jackson (jake@spacefarergames.com)
//
// Font rendering declarations
///////////////////////////////////////////////////////////////////////////////


extern int fontHeight;
void SetFont(char* fontData, int color);
void SetFontSpace(int interWordSpace, int interLetterSpace);
int ExtGetSizeFont(u8* string);
void PrintFont(int x, int y, char* surface, u8* string);
void SelectedMessage(int x, int y, int index, int color1, int color2);
void SimpleMessage(int x, int y, int index, int color);
