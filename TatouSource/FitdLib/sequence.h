///////////////////////////////////////////////////////////////////////////////
// Alone In The Dark Re-Haunted
// Copyright (C) 2026 Infogrames / Spacefarer Retro Remasters LLC
// Based on FITD by yaz0r, Re-haunted is released under GPL
// Author: Jake Jackson (jake@spacefarergames.com)
//
// Cutscene sequence player declarations
///////////////////////////////////////////////////////////////////////////////

#ifndef _SEQUENCE_H_
#define _SEQUENCE_H_

#define NUM_MAX_SEQUENCE_PARAM 30

struct sequenceParamStruct
{
	unsigned int frame;
	unsigned int sample;
};

typedef struct sequenceParamStruct sequenceParamStruct;


extern int numSequenceParam;

extern sequenceParamStruct sequenceParams[NUM_MAX_SEQUENCE_PARAM];

#endif // _SEQUENCE_H_
