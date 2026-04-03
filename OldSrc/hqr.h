///////////////////////////////////////////////////////////////////////////////
// Alone In The Dark Re-Haunted
// Copyright (C) 2026 Infogrames / Spacefarer Retro Remasters LLC
// Based on FITD by yaz0r, Re-haunted is released under GPL
// Author: Jake Jackson (jake@spacefarergames.com)
//
// HQR resource system declarations and templates
///////////////////////////////////////////////////////////////////////////////

#pragma once

#include <string>
#include <vector>
#include "config.h"

template <typename T>
struct hqrSubEntryStruct
{
    s16 key;
    s16 size;
    unsigned int lastTimeUsed;
    T* ptr;
};

template <typename T>
struct hqrEntryStruct
{
    std::string string;
    u16 maxFreeData;
    u16 sizeFreeData;
    u16 numMaxEntry;
    u16 numUsedEntry;
    std::vector<hqrSubEntryStruct<T>> entries;
};

template <typename T>
T* HQR_Get(hqrEntryStruct<T>* hqrPtr, int index);

hqrEntryStruct<char>* HQR_Init(int size, int numEntry);
int HQ_Malloc(hqrEntryStruct<char>* hqrPtr,int size);
char* HQ_PtrMalloc(hqrEntryStruct<char>* hqrPtr, int index);
void HQ_Free_Malloc(hqrEntryStruct<char>* hqrPtr, int index);

template <typename T>
void HQR_Free(hqrEntryStruct<T>* hqrPtr);

template <typename T>
hqrEntryStruct<T>* HQR_InitRessource(const char* name, int size, int numEntries);

template <typename T>
void HQR_Reset(hqrEntryStruct<T>* hqrPtr);

template <typename T>
void configureHqrHero(hqrEntryStruct<T>* hqrPtr, const char* name);

struct sBody* createBodyFromPtr(void* ptr);


