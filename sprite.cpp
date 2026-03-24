///////////////////////////////////////////////////////////////////////////////
// Alone In The Dark Re-Haunted
// Copyright (C) 2026 Infogrames / Spacefarer Retro Remasters LLC
// Based on FITD by yaz0r, Re-haunted is released under GPL
// Author: Jake Jackson (jake@spacefarergames.com)
//
// Sprite rendering and blitting
///////////////////////////////////////////////////////////////////////////////

#include "common.h"
#include "sprite.h"

void sHybrid_Sprite::read(uint8_t* buffer, int bufferSize) {
    flags = READ_LE_U16(buffer); buffer+=2;
    dx = READ_LE_U8(buffer); buffer++;
    dy = READ_LE_U8(buffer); buffer++;

    lines.reserve(dy);
    for (int i = 0; i < dy; i++) {
        auto& line = lines.emplace_back();
        int numBlocks = READ_LE_U8(buffer); buffer++;
        line.blocks.reserve(numBlocks);
        for (int j = 0; j < numBlocks; j++) {
            auto& block = line.blocks.emplace_back();
            block.unk0 = READ_LE_U8(buffer); buffer++;
            block.numWords = READ_LE_U8(buffer); buffer++;
            block.numBytes = READ_LE_U8(buffer); buffer++;

            int dataSize = block.numWords * 4 + block.numBytes;
            block.data.resize(dataSize);
            for (int k = 0; k < dataSize; k++) {
                block.data[k] = READ_LE_U8(buffer); buffer++;
            }
        }
        line.unk = READ_LE_U8(buffer); buffer++;
    }
}

void AffSpr(int spriteNumber, int X, int Y, char* screen, std::vector<sHybrid_Sprite>& sprites) {
    if (spriteNumber < 0 || spriteNumber >= (int)sprites.size())
        return;

    sHybrid_Sprite& sprite = sprites[spriteNumber];

    Y -= sprite.dy;
    for (int y = 0; y < sprite.dy; y++) {
        int screenY = y + Y;
        if (screenY < 0 || screenY >= 200)
            continue;

        auto& lineData = sprite.lines[y];

        int lineOffset = screenY * 320 + X;
        char* lineStart = screen + lineOffset;
        for (int i = 0; i < lineData.blocks.size(); i++) {
            auto& block = lineData.blocks[i];
            lineStart += block.unk0;
            int currentOffset = (int)(lineStart - screen);
            int blockSize = (int)block.data.size();
            if (currentOffset >= 0 && currentOffset + blockSize <= 64000)
            {
                memcpy(lineStart, block.data.data(), blockSize);
            }
            lineStart += blockSize;
        }
    }
}
