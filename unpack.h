///////////////////////////////////////////////////////////////////////////////
// Alone In The Dark Re-Haunted
// Copyright (C) 2026 Infogrames / Spacefarer Retro Remasters LLC
// Author: Jake Jackson (jake@spacefarergames.com)
//
// Data decompression declarations
///////////////////////////////////////////////////////////////////////////////

#ifndef _UNPACK_DAMIEN_H_
#define _UNPACK_DAMIEN_H_

int PAK_deflate(unsigned char * srcBuffer, unsigned char * dstBuffer, unsigned int compressedSize, unsigned int uncompressedSize);
int PAK_explode(unsigned char * srcBuffer, unsigned char * dstBuffer, unsigned int compressedSize, unsigned int uncompressedSize, unsigned short flags);

#endif
