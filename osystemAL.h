///////////////////////////////////////////////////////////////////////////////
// Alone In The Dark Re-Haunted
// Copyright (C) 2026 Infogrames / Spacefarer Retro Remasters LLC
// Author: Jake Jackson (jake@spacefarergames.com)
//
// OpenAL audio system declarations
///////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
extern "C" {
#endif
  
void osystemAL_init();
void osystemAL_udpate();

void checkALError();

#ifdef __cplusplus
}
#endif
