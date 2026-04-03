///////////////////////////////////////////////////////////////////////////////
// Alone In The Dark Re-Haunted
// Copyright (C) 2026 Infogrames / Spacefarer Retro Remasters LLC
// Based on FITD by yaz0r, Re-haunted is released under GPL
// Author: Jake Jackson (jake@spacefarergames.com)
//
// File access utility declarations
///////////////////////////////////////////////////////////////////////////////

#ifndef _FILE_ACCESS_
#define _FILE_ACCESS_
void fatalError(int type, const char* name);
char* loadFromItd(const char* name);
char* CheckLoadMallocPak(const char* name, int index);
#endif
