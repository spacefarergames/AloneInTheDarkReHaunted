///////////////////////////////////////////////////////////////////////////////
// Alone In The Dark Re-Haunted
// Copyright (C) 2026 Infogrames / Spacefarer Retro Remasters LLC
// Author: Jake Jackson (jake@spacefarergames.com)
//
// File I/O utilities and path management
///////////////////////////////////////////////////////////////////////////////

#include "common.h"
#include "embedded/embeddedData.h"
#include <cstring>
// seg 20
void fatalError(int type, const char* name)
{
    //  freeScene();
    freeAll();
    printf("Error: %s\n", name);
    assert(0);
}

extern "C" {
    extern char homePath[512];
}

char* loadFromItd(const char* name)
{
    // Check embedded data first
    {
        const unsigned char* embData = nullptr;
        size_t embSize = 0;
        if (getEmbeddedFile(name, &embData, &embSize))
        {
            fileSize = (int)embSize;
            char* ptr = (char*)malloc(fileSize);
            if (!ptr)
            {
                fatalError(1, name);
                return NULL;
            }
            memcpy(ptr, embData, fileSize);
            return ptr;
        }
    }

    FILE* fHandle;
    char* ptr;

    char filePath[512];
    strcpy(filePath, homePath);
    strcat(filePath, name);

    fHandle = fopen(filePath,"rb");
    if(!fHandle)
    {
        fatalError(0,name);
        return NULL;
    }
    fseek(fHandle,0,SEEK_END);
    fileSize = ftell(fHandle);
    fseek(fHandle,0,SEEK_SET);
    ptr = (char*)malloc(fileSize);

    if(!ptr)
    {
        fatalError(1,name);
        return NULL;
    }
    fread(ptr,fileSize,1,fHandle);
    fclose(fHandle);
    return(ptr);
}

char* CheckLoadMallocPak(const char* name, int index)
{
    char* ptr;
    ptr = loadPak(name, index);
    if(!ptr)
    {
        fatalError(0,name);
    }
    return ptr;
}
