///////////////////////////////////////////////////////////////////////////////
// Alone In The Dark Re-Haunted
// Copyright (C) 2026 Infogrames / Spacefarer Retro Remasters LLC
// Author: Jake Jackson (jake@spacefarergames.com)
//
// Auto-generated embedded data file - DO NOT EDIT
///////////////////////////////////////////////////////////////////////////////

#include "embeddedData.h"
#include <cstring>
#include <cctype>

extern const unsigned char embdata_jack_ANIM16_PAK[];
extern const unsigned long long embdata_jack_ANIM16_PAK_size;
extern const unsigned char embdata_jack_CAMERA16_PAK[];
extern const unsigned long long embdata_jack_CAMERA16_PAK_size;
extern const unsigned char embdata_jack_DEUTSCH_PAK[];
extern const unsigned long long embdata_jack_DEUTSCH_PAK_size;
extern const unsigned char embdata_jack_ENGLISH_PAK[];
extern const unsigned long long embdata_jack_ENGLISH_PAK_size;
extern const unsigned char embdata_jack_ESPAGNOL_PAK[];
extern const unsigned long long embdata_jack_ESPAGNOL_PAK_size;
extern const unsigned char embdata_jack_ETAGE16_PAK[];
extern const unsigned long long embdata_jack_ETAGE16_PAK_size;
extern const unsigned char embdata_jack_FRANCAIS_PAK[];
extern const unsigned long long embdata_jack_FRANCAIS_PAK_size;
extern const unsigned char embdata_jack_ITALIANO_PAK[];
extern const unsigned long long embdata_jack_ITALIANO_PAK_size;
extern const unsigned char embdata_jack_ITD_RESS_PAK[];
extern const unsigned long long embdata_jack_ITD_RESS_PAK_size;
extern const unsigned char embdata_jack_LISTANIM_PAK[];
extern const unsigned long long embdata_jack_LISTANIM_PAK_size;
extern const unsigned char embdata_jack_LISTBODY_PAK[];
extern const unsigned long long embdata_jack_LISTBODY_PAK_size;
extern const unsigned char embdata_jack_LISTHYB_PAK[];
extern const unsigned long long embdata_jack_LISTHYB_PAK_size;
extern const unsigned char embdata_jack_LISTLIFE_PAK[];
extern const unsigned long long embdata_jack_LISTLIFE_PAK_size;
extern const unsigned char embdata_jack_LISTMAT_PAK[];
extern const unsigned long long embdata_jack_LISTMAT_PAK_size;
extern const unsigned char embdata_jack_LISTMUS_PAK[];
extern const unsigned long long embdata_jack_LISTMUS_PAK_size;
extern const unsigned char embdata_jack_LISTSAMP_PAK[];
extern const unsigned long long embdata_jack_LISTSAMP_PAK_size;
extern const unsigned char embdata_jack_LISTTRAK_PAK[];
extern const unsigned long long embdata_jack_LISTTRAK_PAK_size;
extern const unsigned char embdata_jack_MASK16_PAK[];
extern const unsigned long long embdata_jack_MASK16_PAK_size;
extern const unsigned char embdata_jack_PERE_PAK[];
extern const unsigned long long embdata_jack_PERE_PAK_size;
extern const unsigned char embdata_jack_DEFINES_ITD[];
extern const unsigned long long embdata_jack_DEFINES_ITD_size;
extern const unsigned char embdata_jack_OBJETS_ITD[];
extern const unsigned long long embdata_jack_OBJETS_ITD_size;
extern const unsigned char embdata_jack_PRIORITY_ITD[];
extern const unsigned long long embdata_jack_PRIORITY_ITD_size;
extern const unsigned char embdata_jack_VARS_ITD[];
extern const unsigned long long embdata_jack_VARS_ITD_size;

struct EmbeddedFileEntry {
    const char* name;
    const unsigned char* data;
    unsigned long long size;
};

static const EmbeddedFileEntry s_embeddedFiles[] = {
    { "ANIM16.PAK", embdata_jack_ANIM16_PAK, embdata_jack_ANIM16_PAK_size },
    { "CAMERA16.PAK", embdata_jack_CAMERA16_PAK, embdata_jack_CAMERA16_PAK_size },
    { "DEUTSCH.PAK", embdata_jack_DEUTSCH_PAK, embdata_jack_DEUTSCH_PAK_size },
    { "ENGLISH.PAK", embdata_jack_ENGLISH_PAK, embdata_jack_ENGLISH_PAK_size },
    { "ESPAGNOL.PAK", embdata_jack_ESPAGNOL_PAK, embdata_jack_ESPAGNOL_PAK_size },
    { "ETAGE16.PAK", embdata_jack_ETAGE16_PAK, embdata_jack_ETAGE16_PAK_size },
    { "FRANCAIS.PAK", embdata_jack_FRANCAIS_PAK, embdata_jack_FRANCAIS_PAK_size },
    { "ITALIANO.PAK", embdata_jack_ITALIANO_PAK, embdata_jack_ITALIANO_PAK_size },
    { "ITD_RESS.PAK", embdata_jack_ITD_RESS_PAK, embdata_jack_ITD_RESS_PAK_size },
    { "LISTANIM.PAK", embdata_jack_LISTANIM_PAK, embdata_jack_LISTANIM_PAK_size },
    { "LISTBODY.PAK", embdata_jack_LISTBODY_PAK, embdata_jack_LISTBODY_PAK_size },
    { "LISTHYB.PAK", embdata_jack_LISTHYB_PAK, embdata_jack_LISTHYB_PAK_size },
    { "LISTLIFE.PAK", embdata_jack_LISTLIFE_PAK, embdata_jack_LISTLIFE_PAK_size },
    { "LISTMAT.PAK", embdata_jack_LISTMAT_PAK, embdata_jack_LISTMAT_PAK_size },
    { "LISTMUS.PAK", embdata_jack_LISTMUS_PAK, embdata_jack_LISTMUS_PAK_size },
    { "LISTSAMP.PAK", embdata_jack_LISTSAMP_PAK, embdata_jack_LISTSAMP_PAK_size },
    { "LISTTRAK.PAK", embdata_jack_LISTTRAK_PAK, embdata_jack_LISTTRAK_PAK_size },
    { "MASK16.PAK", embdata_jack_MASK16_PAK, embdata_jack_MASK16_PAK_size },
    { "PERE.PAK", embdata_jack_PERE_PAK, embdata_jack_PERE_PAK_size },
    { "DEFINES.ITD", embdata_jack_DEFINES_ITD, embdata_jack_DEFINES_ITD_size },
    { "OBJETS.ITD", embdata_jack_OBJETS_ITD, embdata_jack_OBJETS_ITD_size },
    { "PRIORITY.ITD", embdata_jack_PRIORITY_ITD, embdata_jack_PRIORITY_ITD_size },
    { "VARS.ITD", embdata_jack_VARS_ITD, embdata_jack_VARS_ITD_size },
};

static const int s_numEmbeddedFiles = sizeof(s_embeddedFiles) / sizeof(s_embeddedFiles[0]);

static const char* extractFilename(const char* path)
{
    const char* lastSlash = path;
    for (const char* p = path; *p; p++)
    {
        if (*p == '/' || *p == '\\')
            lastSlash = p + 1;
    }
    return lastSlash;
}

static bool strEqualNoCase(const char* a, const char* b)
{
    while (*a && *b)
    {
        if (toupper((unsigned char)*a) != toupper((unsigned char)*b))
            return false;
        a++; b++;
    }
    return *a == *b;
}

bool getEmbeddedJackFile(const char* filename, const unsigned char** outData, size_t* outSize)
{
    const char* name = extractFilename(filename);
    for (int i = 0; i < s_numEmbeddedFiles; i++)
    {
        if (strEqualNoCase(name, s_embeddedFiles[i].name))
        {
            if (outData) *outData = s_embeddedFiles[i].data;
            if (outSize) *outSize = (size_t)s_embeddedFiles[i].size;
            return true;
        }
    }
    return false;
}
