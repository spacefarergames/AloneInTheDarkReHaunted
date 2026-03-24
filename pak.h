///////////////////////////////////////////////////////////////////////////////
// Alone In The Dark Re-Haunted
// Copyright (C) 2026 Infogrames / Spacefarer Retro Remasters LLC
// Based on FITD by yaz0r, Re-haunted is released under GPL
// Author: Jake Jackson (jake@spacefarergames.com)
//
// PAK archive file format declarations
///////////////////////////////////////////////////////////////////////////////

#ifndef _PAK_
#define _PAK_

char* loadPak(const char* name, int index);
int LoadPak(const char* name, int index, char* ptr);
int getPakSize(const char* name, int index);
unsigned int PAK_getNumFiles(const char* name);
void dumpPak(const char* name);

#endif
