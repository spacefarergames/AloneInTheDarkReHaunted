///////////////////////////////////////////////////////////////////////////////
// Alone In The Dark Re-Haunted
// Copyright (C) 2026 Infogrames / Spacefarer Retro Remasters LLC
// Based on FITD by yaz0r, Re-haunted is released under GPL
// Author: Jake Jackson (jake@spacefarergames.com)
//
// Inventory system declarations
///////////////////////////////////////////////////////////////////////////////


#define INVENTORY_SIZE      30
#define NUM_MAX_INVENTORY	2
extern s16 currentInventory;
extern s16 numObjInInventoryTable[NUM_MAX_INVENTORY];
extern s16 inHandTable[NUM_MAX_INVENTORY];
extern s16 inventoryTable[NUM_MAX_INVENTORY][INVENTORY_SIZE];

extern int statusLeft;
extern int statusTop;
extern int statusRight;
extern int statusBottom;

void processInventory(void);
