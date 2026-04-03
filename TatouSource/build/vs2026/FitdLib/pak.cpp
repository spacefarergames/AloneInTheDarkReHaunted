///////////////////////////////////////////////////////////////////////////////
// Alone In The Dark Re-Haunted
// Copyright (C) 2026 Infogrames / Spacefarer Retro Remasters LLC
// Based on FITD by yaz0r, Re-haunted is released under GPL
// Author: Jake Jackson (jake@spacefarergames.com)
//
// PAK archive file format reading and extraction
///////////////////////////////////////////////////////////////////////////////

// seg 55

#include "common.h"
#include "consoleLog.h"
#include "embedded/embeddedData.h"
#include "asyncLoader.h"

#ifdef WIN32
#include <direct.h>
#endif

#include <cstring>

extern "C" {
    extern char homePath[512];
}

struct pakInfoStruct // warning: alignment unsafe
{
    s32 discSize;
    s32 uncompressedSize;
    char compressionFlag;
    char info5;
    s16 offset;
};

//#define USE_UNPACKED_DATA

void readPakInfo(pakInfoStruct* pPakInfo, FILE* fileHandle)
{
    fread(&pPakInfo->discSize,4,1,fileHandle);
    fread(&pPakInfo->uncompressedSize,4,1,fileHandle);
    fread(&pPakInfo->compressionFlag,1,1,fileHandle);
    fread(&pPakInfo->info5,1,1,fileHandle);
    fread(&pPakInfo->offset,2,1,fileHandle);

    pPakInfo->discSize = READ_LE_U32(&pPakInfo->discSize);
    pPakInfo->uncompressedSize = READ_LE_U32(&pPakInfo->uncompressedSize);
    pPakInfo->offset = READ_LE_U16(&pPakInfo->offset);
}

// Memory-buffer equivalent of readPakInfo for embedded data
static void readPakInfoFromMem(pakInfoStruct* pPakInfo, const unsigned char* data, size_t* offset)
{
    memcpy(&pPakInfo->discSize, data + *offset, 4); *offset += 4;
    memcpy(&pPakInfo->uncompressedSize, data + *offset, 4); *offset += 4;
    memcpy(&pPakInfo->compressionFlag, data + *offset, 1); *offset += 1;
    memcpy(&pPakInfo->info5, data + *offset, 1); *offset += 1;
    memcpy(&pPakInfo->offset, data + *offset, 2); *offset += 2;

    pPakInfo->discSize = READ_LE_U32(&pPakInfo->discSize);
    pPakInfo->uncompressedSize = READ_LE_U32(&pPakInfo->uncompressedSize);
    pPakInfo->offset = READ_LE_U16(&pPakInfo->offset);
}

// Build the PAK filename used for embedded lookup (e.g. "CAMERA00.PAK")
static void buildPakFilename(char* out, const char* name)
{
    const char* base = name;
    const char* p = name;
    while (*p)
    {
        if (*p == '/' || *p == '\\')
            base = p + 1;
        p++;
    }
    strcpy(out, base);
    strcat(out, ".PAK");
}

unsigned int PAK_getNumFiles(const char* name)
{
    // Try disk file first
    {
        char bufferName[512];
        FILE* fileHandle;
        u32 fileOffset;

        strcpy(bufferName, homePath);
        strcat(bufferName, name);
        strcat(bufferName,".PAK");

        fileHandle = fopen(bufferName,"rb");

        if (fileHandle)
        {
            fseek(fileHandle,4,SEEK_CUR);
            fread(&fileOffset,4,1,fileHandle);
#ifdef MACOSX
            fileOffset = READ_LE_U32(&fileOffset);
#endif
            fclose(fileHandle);
            return((fileOffset/4)-2);
        }
    }

    // Fall back to embedded data
    {
        char pakName[256];
        buildPakFilename(pakName, name);
        const unsigned char* embData = nullptr;
        size_t embSize = 0;
        if (getEmbeddedFile(pakName, &embData, &embSize))
        {
            u32 fileOffset;
            memcpy(&fileOffset, embData + 4, 4);
            fileOffset = READ_LE_U32(&fileOffset);
            return ((fileOffset / 4) - 2);
        }
    }

    return 0;
}

int LoadPak(const char* name, int index, char* ptr)
{
#ifdef USE_UNPACKED_DATA
    char buffer[256];
    FILE* fHandle;
    int size;

    sprintf(buffer,"%s/%04X.OUT",name,index);

    fHandle = fopen(buffer,"rb");

    if(!fHandle)
        return(0);

    fseek(fHandle,0L,SEEK_END);
    size = ftell(fHandle);
    fseek(fHandle,0L,SEEK_SET);

    fread(ptr,size,1,fHandle);
    fclose(fHandle);

    return(1);
#else
    char* lptr;

    lptr = loadPak(name,index);

    if(!lptr)
        return(0);

    memcpy(ptr,lptr,getPakSize(name,index));

    free(lptr);

    return(1);
#endif
}

int getPakSize(const char* name, int index)
{
    // Bounds check - log warning but continue to access data (DOS buffer skipover)
    int numFiles = PAK_getNumFiles(name);
    if(index < 0 || numFiles <= index)
    {
        printf("[PAK] Warning: DOS buffer skipover - getPakSize index %d for '%s' (numFiles=%d)\n", index, name, numFiles);
    }

#ifdef USE_UNPACKED_DATA
    char buffer[256];
    FILE* fHandle;
    int size;

    sprintf(buffer,"%s/%04X.OUT",name,index);

    fHandle = fopen(buffer,"rb");

    if(!fHandle)
        return(0);

    fseek(fHandle,0L,SEEK_END);
    size = ftell(fHandle);
    fseek(fHandle,0L,SEEK_SET);

    fclose(fHandle);

    return (size);
#else
    // Try disk file first
    {
        char bufferName[512];
        FILE* fileHandle;
        s32 fileOffset;
        s32 additionalDescriptorSize;
        pakInfoStruct pakInfo;
        s32 size=0;

        strcpy(bufferName, homePath);
        strcat(bufferName, name);
        strcat(bufferName,".PAK");

        fileHandle = fopen(bufferName,"rb");

        if(fileHandle)
        {
            fseek(fileHandle,(index+1)*4,SEEK_SET);

            fread(&fileOffset,4,1,fileHandle);
            fileOffset = READ_LE_U32(&fileOffset);

            fseek(fileHandle,fileOffset,SEEK_SET);

            fread(&additionalDescriptorSize,4,1,fileHandle);
            additionalDescriptorSize = READ_LE_U32(&additionalDescriptorSize);

            if(additionalDescriptorSize)
            {
                fseek(fileHandle, additionalDescriptorSize-4, SEEK_CUR);
            }

            readPakInfo(&pakInfo,fileHandle);

            fseek(fileHandle,pakInfo.offset,SEEK_CUR);

            // Apply same safety caps as loadPak for DOS buffer skipover consistency
            const s32 MAX_PAK_SIZE = 16 * 1024 * 1024;
            if(pakInfo.discSize > MAX_PAK_SIZE || pakInfo.discSize <= 0) pakInfo.discSize = 4096;
            if(pakInfo.uncompressedSize > MAX_PAK_SIZE || pakInfo.uncompressedSize <= 0) pakInfo.uncompressedSize = 4096;

            if(pakInfo.compressionFlag == 0) // uncompressed
            {
                size = pakInfo.discSize;
            }
            else if(pakInfo.compressionFlag == 1) // compressed
            {
                size = pakInfo.uncompressedSize;
            }
            else if(pakInfo.compressionFlag == 4)
            {
                size = pakInfo.uncompressedSize;
            }

            fclose(fileHandle);
            return size;
        }
    }

    // Fall back to embedded data
    {
        char pakName[256];
        buildPakFilename(pakName, name);
        const unsigned char* embData = nullptr;
        size_t embSize = 0;
        if (getEmbeddedFile(pakName, &embData, &embSize))
        {
            size_t pos = (size_t)(index + 1) * 4;
            s32 fileOffset;
            memcpy(&fileOffset, embData + pos, 4);
            fileOffset = READ_LE_U32(&fileOffset);

            pos = (size_t)fileOffset;
            s32 additionalDescriptorSize;
            memcpy(&additionalDescriptorSize, embData + pos, 4); pos += 4;
            additionalDescriptorSize = READ_LE_U32(&additionalDescriptorSize);
            if (additionalDescriptorSize)
                pos += additionalDescriptorSize - 4;

            pakInfoStruct pakInfo;
            readPakInfoFromMem(&pakInfo, embData, &pos);

            pos += pakInfo.offset;

            // Apply same safety caps as loadPak for consistency
            const s32 MAX_PAK_SIZE = 16 * 1024 * 1024;
            if(pakInfo.discSize > MAX_PAK_SIZE || pakInfo.discSize <= 0) pakInfo.discSize = 4096;
            if(pakInfo.uncompressedSize > MAX_PAK_SIZE || pakInfo.uncompressedSize <= 0) pakInfo.uncompressedSize = 4096;

            if (pakInfo.compressionFlag == 0)
                return pakInfo.discSize;
            else
                return pakInfo.uncompressedSize;
        }
    }

    return 0;
#endif
}

char* loadPak(const char* name, int index)
{
	// Check preload cache first (async preloaded data ready to use)
	char* preloaded = tryGetPreloadedPak(name, index);
	if (preloaded)
		return preloaded;

	// Bounds check - log warning but continue to access data (DOS buffer skipover)
	int numFiles = PAK_getNumFiles(name);
	if(index < 0 || numFiles <= index)
	{
		printf("[PAK] Warning: DOS buffer skipover - loadPak index %d for '%s' (numFiles=%d)\n", index, name, numFiles);
	}

	//dumpPak(name);
#ifdef USE_UNPACKED_DATA
	char buffer[256];
	FILE* fHandle;
	int size;
	char* ptr;

	sprintf(buffer,"%s/%04X.OUT",name,index);

	fHandle = fopen(buffer,"rb");

	if(!fHandle)
		return NULL;

	fseek(fHandle,0L,SEEK_END);
	size = ftell(fHandle);
	fseek(fHandle,0L,SEEK_SET);

	ptr = (char*)malloc(size);

	fread(ptr,size,1,fHandle);
	fclose(fHandle);

	return ptr;
#else
	// Try disk file first
	{
		char bufferName[512];
		FILE* fileHandle;
		u32 fileOffset;
		u32 additionalDescriptorSize;
		pakInfoStruct pakInfo;
		char* ptr=0;

		strcpy(bufferName, homePath);
		strcat(bufferName, name);
		strcat(bufferName,".PAK");

		fileHandle = fopen(bufferName,"rb");

		if(fileHandle)
		{
			char nameBuffer[256] = "";

			fseek(fileHandle,(index+1)*4,SEEK_SET);

			fread(&fileOffset,4,1,fileHandle);
			fileOffset = READ_LE_U32(&fileOffset);

			fseek(fileHandle,fileOffset,SEEK_SET);

			fread(&additionalDescriptorSize,4,1,fileHandle);
			additionalDescriptorSize = READ_LE_U32(&additionalDescriptorSize);

			if(additionalDescriptorSize)
		{
			fseek(fileHandle, additionalDescriptorSize-4, SEEK_CUR);
		}

		readPakInfo(&pakInfo,fileHandle);

		if(pakInfo.offset)
		{
			ASSERT(pakInfo.offset<256);

			fread(nameBuffer,pakInfo.offset,1,fileHandle);
#ifdef FITD_DEBUGGER
			printf(PAK_TAG "Loading %s/%s\n", name,nameBuffer+2);
#endif
		}
		else
		{
			fseek(fileHandle,pakInfo.offset,SEEK_CUR);
		}

		// DOS buffer skipover safety: cap sizes to prevent crash from garbage pakInfo
		const u32 MAX_PAK_SIZE = 16 * 1024 * 1024; // 16MB max
		if(pakInfo.discSize > MAX_PAK_SIZE) pakInfo.discSize = 4096;
		if(pakInfo.uncompressedSize > MAX_PAK_SIZE) pakInfo.uncompressedSize = 4096;
		if(pakInfo.discSize == 0) pakInfo.discSize = 4096;
		if(pakInfo.uncompressedSize == 0) pakInfo.uncompressedSize = 4096;

		switch(pakInfo.compressionFlag)
		{
		case 0:
			{
				ptr = (char*)malloc(pakInfo.discSize);
				if(ptr) fread(ptr,pakInfo.discSize,1,fileHandle);
				break;
			}
		case 1:
			{
				char * compressedDataPtr = (char *) malloc(pakInfo.discSize);
				if(compressedDataPtr)
				{
					fread(compressedDataPtr, pakInfo.discSize, 1, fileHandle);
					ptr = (char *) malloc(pakInfo.uncompressedSize);
					if(ptr)
					{
						PAK_explode((unsigned char*)compressedDataPtr, (unsigned char*)ptr, pakInfo.discSize, pakInfo.uncompressedSize, pakInfo.info5);
					}
					free(compressedDataPtr);
				}
				break;
			}
		case 4:
			{
				char * compressedDataPtr = (char *) malloc(pakInfo.discSize);
				if(compressedDataPtr)
				{
					fread(compressedDataPtr, pakInfo.discSize, 1, fileHandle);
					ptr = (char *) malloc(pakInfo.uncompressedSize);
					if(ptr)
					{
						PAK_deflate((unsigned char*)compressedDataPtr, (unsigned char*)ptr, pakInfo.discSize, pakInfo.uncompressedSize);
					}
					free(compressedDataPtr);
				}
				break;
			}
		default:
			// DOS buffer skipover: unknown compression, allocate dummy buffer
			printf("[PAK] Warning: DOS buffer skipover - unknown compressionFlag %d\n", pakInfo.compressionFlag);
			ptr = (char*)malloc(4096);
			break;
		}

		// DOS buffer skipover: never return NULL, allocate fallback if needed
		if(!ptr)
		{
			printf("[PAK] Warning: DOS buffer skipover - malloc failed, returning dummy buffer\n");
			ptr = (char*)malloc(4096);
			if(ptr) memset(ptr, 0, 4096);
		}

			fclose(fileHandle);
			return ptr;
		}
	}

	// Fall back to embedded data
	{
		char pakName[256];
		buildPakFilename(pakName, name);
		const unsigned char* embData = nullptr;
		size_t embSize = 0;
		if (getEmbeddedFile(pakName, &embData, &embSize))
		{
			size_t pos = (size_t)(index + 1) * 4;
			u32 fileOffset;
			memcpy(&fileOffset, embData + pos, 4);
			fileOffset = READ_LE_U32(&fileOffset);

			pos = (size_t)fileOffset;
			u32 additionalDescriptorSize;
			memcpy(&additionalDescriptorSize, embData + pos, 4); pos += 4;
			additionalDescriptorSize = READ_LE_U32(&additionalDescriptorSize);
			if (additionalDescriptorSize)
				pos += additionalDescriptorSize - 4;

			pakInfoStruct pakInfo;
			readPakInfoFromMem(&pakInfo, embData, &pos);
			pos += pakInfo.offset; // skip name buffer

			char* ptr = nullptr;
				switch (pakInfo.compressionFlag)
				{
				case 0:
					ptr = (char*)malloc(pakInfo.discSize);
					if(ptr) memcpy(ptr, embData + pos, pakInfo.discSize);
					break;
				case 1:
				{
					char* compressedDataPtr = (char*)malloc(pakInfo.discSize);
					if(compressedDataPtr)
					{
						memcpy(compressedDataPtr, embData + pos, pakInfo.discSize);
						ptr = (char*)malloc(pakInfo.uncompressedSize);
						if(ptr)
						{
							PAK_explode((unsigned char*)compressedDataPtr, (unsigned char*)ptr, pakInfo.discSize, pakInfo.uncompressedSize, pakInfo.info5);
						}
						free(compressedDataPtr);
					}
					break;
				}
				case 4:
				{
					char* compressedDataPtr = (char*)malloc(pakInfo.discSize);
					if(compressedDataPtr)
					{
						memcpy(compressedDataPtr, embData + pos, pakInfo.discSize);
						ptr = (char*)malloc(pakInfo.uncompressedSize);
						if(ptr)
						{
							PAK_deflate((unsigned char*)compressedDataPtr, (unsigned char*)ptr, pakInfo.discSize, pakInfo.uncompressedSize);
						}
						free(compressedDataPtr);
					}
					break;
				}
				default:
					assert(false);
					break;
			}
			return ptr;
		}
	}

	return NULL;
#endif
}

void dumpPak(const char* name)
{
#ifdef WIN32 
    unsigned int numEntries = PAK_getNumFiles(name);

    for (unsigned int index = 0; index < numEntries; index++)
    {
        char bufferName[512];
        FILE* fileHandle;
        u32 fileOffset;
        u32 additionalDescriptorSize;
        pakInfoStruct pakInfo;
        char* ptr = 0;


        //makeExtention(bufferName, name, ".PAK");
        strcpy(bufferName, homePath);
        strcat(bufferName, name); // temporary until makeExtention is coded
        strcat(bufferName, ".PAK");

        fileHandle = fopen(bufferName, "rb");

        if (fileHandle) // a bit stupid, should return NULL right away
        {
            char nameBuffer[256] = "";

            fseek(fileHandle, (index + 1) * 4, SEEK_SET);

            fread(&fileOffset, 4, 1, fileHandle);
            fileOffset = READ_LE_U32(&fileOffset);

            fseek(fileHandle, fileOffset, SEEK_SET);

            fread(&additionalDescriptorSize, 4, 1, fileHandle);
            additionalDescriptorSize = READ_LE_U32(&additionalDescriptorSize);

            if (additionalDescriptorSize)
            {
                fseek(fileHandle, additionalDescriptorSize - 4, SEEK_CUR);
            }

            readPakInfo(&pakInfo, fileHandle);

            if (pakInfo.offset)
            {
                ASSERT(pakInfo.offset < 256);

                fread(nameBuffer, pakInfo.offset, 1, fileHandle);
#ifdef FITD_DEBUGGER
                printf(PAK_TAG "Loading %s/%s\n", name, nameBuffer + 2);
#endif
            }
            else
            {
                fseek(fileHandle, pakInfo.offset, SEEK_CUR);
            }

            switch (pakInfo.compressionFlag)
            {
            case 0:
            {
                ptr = (char*)malloc(pakInfo.discSize);
                fread(ptr, pakInfo.discSize, 1, fileHandle);
                break;
            }
            case 1:
            {
                char * compressedDataPtr = (char *)malloc(pakInfo.discSize);
                fread(compressedDataPtr, pakInfo.discSize, 1, fileHandle);
                ptr = (char *)malloc(pakInfo.uncompressedSize);

                int explodeResult = PAK_explode((unsigned char*)compressedDataPtr, (unsigned char*)ptr, pakInfo.discSize, pakInfo.uncompressedSize, pakInfo.info5);

                if(explodeResult != 0)
                {
                    printf(PAK_ERR "PAK_explode failed for %s index %d (discSize=%d, uncompressedSize=%d, flags=%d, result=%d)" CON_RESET "\n", 
                           name, index, pakInfo.discSize, pakInfo.uncompressedSize, pakInfo.info5, explodeResult);
                    // Initialize with zeros to avoid returning uninitialized memory
                    memset(ptr, 0, pakInfo.uncompressedSize);
                }

                free(compressedDataPtr);
                break;
            }
            case 4:
            {
                char * compressedDataPtr = (char *)malloc(pakInfo.discSize);
                fread(compressedDataPtr, pakInfo.discSize, 1, fileHandle);
                ptr = (char *)malloc(pakInfo.uncompressedSize);

                PAK_deflate((unsigned char*)compressedDataPtr, (unsigned char*)ptr, pakInfo.discSize, pakInfo.uncompressedSize);

                free(compressedDataPtr);
                break;
            }
            default:
                assert(false);
                break;
            }
            fclose(fileHandle);

            {
                mkdir(name);
                char outputName[256];
                sprintf(outputName, "%s/%02d_%s", name, index, nameBuffer + 2);
                FILE* foutputHandle = fopen(outputName, "wb+");
                if (foutputHandle)
                {
                    fwrite(ptr, pakInfo.uncompressedSize, 1, foutputHandle);
                    fclose(foutputHandle);
                }
            }
        }
    }
#endif
}
