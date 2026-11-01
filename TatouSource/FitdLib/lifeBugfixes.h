#pragma once

// AITD1 LIFE bytecode bug fixes (see REHAUNTED_FIXES.md, "Game scripts").
// The fixes are applied to the loaded LIFE bytecode, exactly as a randomizer
// would patch LISTLIFE, and the affected scripts are forced through the bytecode
// interpreter so ReHaunted's generated native copies cannot bypass them.
char* lifeBugfixPatchLife(int lifeNum, char* lifeData);
bool lifeBugfixForcesBytecode(int lifeNum);

// AITD1 TRACK bytecode bug fixes, applied in place to the loaded track data.
char* trackBugfixPatchTrack(int trackNum, char* trackData);
