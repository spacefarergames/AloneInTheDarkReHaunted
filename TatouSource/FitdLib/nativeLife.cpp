///////////////////////////////////////////////////////////////////////////////
// Alone In The Dark Re-Haunted
// Copyright (C) 2026 Infogrames / Spacefarer Retro Remasters LLC
// Based on FITD by yaz0r, Re-haunted is released under GPL
// Author: Jake Jackson (jake@spacefarergames.com)
//
// Native life script system implementation
///////////////////////////////////////////////////////////////////////////////

#include "common.h"
#include "nativeLife.h"
#include "consoleLog.h"
#include <map>
#include <set>
#include <string>
#include <vector>
#include <cstdarg>

// Registry of native life script overrides
static std::map<int, NativeLifeFunc> s_nativeLifeScripts;

void registerNativeLifeScript(int lifeNum, NativeLifeFunc func)
{
    s_nativeLifeScripts[lifeNum] = func;
    printf(LIFE_OK "Registered native life script override for Life %d" CON_RESET "\n", lifeNum);
}

NativeLifeFunc getNativeLifeScript(int lifeNum)
{
    auto it = s_nativeLifeScripts.find(lifeNum);
    if (it != s_nativeLifeScripts.end())
        return it->second;
    return nullptr;
}

int getNativeLifeScriptCount()
{
    return (int)s_nativeLifeScripts.size();
}

// Forward declarations for AITD1 native life scripts
extern void registerAITD1NativeLifeScripts();

void initNativeLifeScripts()
{
    s_nativeLifeScripts.clear();

    if (g_gameId == AITD1)
    {
        registerAITD1NativeLifeScripts();
    }

    printf(LIFE_OK "Native life scripts initialized: %d overrides registered" CON_RESET "\n", (int)s_nativeLifeScripts.size());

    // Dump all life scripts to file if debug flag is set
    if (g_remasterConfig.debug.dumpLifeScripts)
    {
        dumpAllLifeScripts();
    }

    // Generate native C code for all life scripts if debug flag is set
    if (g_remasterConfig.debug.generateNativeLifeScripts)
    {
        generateAllNativeLifeScripts();
    }
}

//////////////////////////////////////////////////////////////////////////////
// Life Script Bytecode Decompiler / Dumper
//////////////////////////////////////////////////////////////////////////////

// Decode an evalVar expression from bytecode, advancing ptr.
// Returns a string representation of the expression.
// For AITD1 only (evalVar, not evalVar2).
static std::string decodeEvalVar(const char*& ptr)
{
    s16 var1 = *(s16*)ptr;
    ptr += 2;

    if (var1 == -1)
    {
        s16 val = *(s16*)ptr;
        ptr += 2;
        return std::to_string(val);
    }
    else if (var1 == 0)
    {
        s16 idx = *(s16*)ptr;
        ptr += 2;
        return "vars[" + std::to_string(idx) + "]";
    }
    else
    {
        std::string prefix;
        int field = var1;

        if (var1 & 0x8000)
        {
            s16 objNum = *(s16*)ptr;
            ptr += 2;
            prefix = "worldObj[" + std::to_string(objNum) + "].";
            field = var1 & 0x7FFF;
        }
        else
        {
            prefix = "self.";
        }

        field = (field & 0x7FFF) - 1;

        // Some fields consume extra bytes from the stream
        switch (field)
        {
        case 0x00: return prefix + "COL";
        case 0x01: return prefix + "HARD_DEC";
        case 0x02: return prefix + "HARD_COL";
        case 0x03: return prefix + "HIT";
        case 0x04: return prefix + "HIT_BY";
        case 0x05: return prefix + "ANIM";
        case 0x06: return prefix + "END_ANIM";
        case 0x07: return prefix + "FRAME";
        case 0x08: return prefix + "END_FRAME";
        case 0x09: return prefix + "BODY";
        case 0x0A: return prefix + "MARK";
        case 0x0B: return prefix + "NUM_TRACK";
        case 0x0C: return prefix + "CHRONO";
        case 0x0D: return prefix + "ROOM_CHRONO";
        case 0x0E: // DIST - consumes an extra s16 (object number)
        {
            s16 distObj = *(s16*)ptr;
            ptr += 2;
            return prefix + "DIST(" + std::to_string(distObj) + ")";
        }
        case 0x0F: return prefix + "COL_BY";
        case 0x10: // FOUND - recursive evalVar
        {
            std::string inner = decodeEvalVar(ptr);
            return "FOUND(" + inner + ")";
        }
        case 0x11: return "ACTION";
        case 0x12: // POSREL - consumes an extra s16
        {
            s16 posrelObj = *(s16*)ptr;
            ptr += 2;
            return prefix + "POSREL(" + std::to_string(posrelObj) + ")";
        }
        case 0x13: return "KEYBOARD";
        case 0x14: return "BUTTON";
        case 0x15: return prefix + "COL_OR_COL_BY";
        case 0x16: return prefix + "ALPHA";
        case 0x17: return prefix + "BETA";
        case 0x18: return prefix + "GAMMA";
        case 0x19: return "IN_HAND";
        case 0x1A: return prefix + "HIT_FORCE";
        case 0x1B: return "CAMERA";
        case 0x1C: // RANDOM - consumes an extra s16
        {
            s16 range = *(s16*)ptr;
            ptr += 2;
            return "RANDOM(" + std::to_string(range) + ")";
        }
        case 0x1D: return prefix + "FALLING";
        case 0x1E: return prefix + "ROOM";
        case 0x1F: return prefix + "LIFE";
        case 0x20: // object found flag check - consumes s16
        {
            s16 objNum = *(s16*)ptr;
            ptr += 2;
            return "OBJ_FOUND(" + std::to_string(objNum) + ")";
        }
        case 0x21: return prefix + "ROOM_Y";
        case 0x22: // TEST_ZV_END_ANIM - consumes 2 s16
        {
            s16 anim = *(s16*)ptr; ptr += 2;
            s16 frame = *(s16*)ptr; ptr += 2;
            return "TEST_ZV_END_ANIM(" + std::to_string(anim) + ", " + std::to_string(frame) + ")";
        }
        case 0x23: return "CURRENT_MUSIC";
        case 0x24: // C_VAR - consumes s16
        {
            s16 cvarIdx = *(s16*)ptr;
            ptr += 2;
            return "CVars[" + std::to_string(cvarIdx) + "]";
        }
        case 0x25: return prefix + "STAGE";
        case 0x26: // THROW check - consumes s16
        {
            s16 objNum = *(s16*)ptr;
            ptr += 2;
            return "THROWN(" + std::to_string(objNum) + ")";
        }
        default:
            return prefix + "UNKNOWN_FIELD_0x" + std::to_string(field);
        }
    }
}

// Skip over an evalVar expression without decoding it (for jump target scanning)
static void skipEvalVar(const char*& ptr)
{
    s16 var1 = *(s16*)ptr;
    ptr += 2;
    if (var1 == -1 || var1 == 0) { ptr += 2; }
    else {
        if (var1 & 0x8000) ptr += 2;
        int field = ((var1 & 0x7FFF) - 1) & 0x7FFF;
        switch (field) {
        case 0x0E: case 0x12: case 0x1C: case 0x20: case 0x24: case 0x26: ptr += 2; break;
        case 0x10: skipEvalVar(ptr); break;
        case 0x22: ptr += 4; break;
        default: break;
        }
    }
}

static const char* getLifeMacroNameForDump(int macro)
{
    switch (macro)
    {
    case LM_DO_MOVE: return "DO_MOVE";
    case LM_ANIM_ONCE: return "ANIM_ONCE";
    case LM_ANIM_ALL_ONCE: return "ANIM_ALL_ONCE";
    case LM_BODY: return "BODY";
    case LM_IF_EGAL: return "IF_EGAL";
    case LM_IF_DIFFERENT: return "IF_DIFFERENT";
    case LM_IF_SUP_EGAL: return "IF_SUP_EGAL";
    case LM_IF_SUP: return "IF_SUP";
    case LM_IF_INF_EGAL: return "IF_INF_EGAL";
    case LM_IF_INF: return "IF_INF";
    case LM_GOTO: return "GOTO";
    case LM_RETURN: return "RETURN";
    case LM_END: return "END";
    case LM_ANIM_REPEAT: return "ANIM_REPEAT";
    case LM_ANIM_MOVE: return "ANIM_MOVE";
    case LM_MOVE: return "MOVE";
    case LM_HIT: return "HIT";
    case LM_MESSAGE: return "MESSAGE";
    case LM_MESSAGE_VALUE: return "MESSAGE_VALUE";
    case LM_VAR: return "VAR";
    case LM_INC: return "INC";
    case LM_DEC: return "DEC";
    case LM_ADD: return "ADD";
    case LM_SUB: return "SUB";
    case LM_LIFE_MODE: return "LIFE_MODE";
    case LM_SWITCH: return "SWITCH";
    case LM_CASE: return "CASE";
    case LM_CAMERA: return "CAMERA";
    case LM_START_CHRONO: return "START_CHRONO";
    case LM_MULTI_CASE: return "MULTI_CASE";
    case LM_FOUND: return "FOUND";
    case LM_LIFE: return "LIFE";
    case LM_DELETE: return "DELETE";
    case LM_TAKE: return "TAKE";
    case LM_IN_HAND: return "IN_HAND";
    case LM_READ: return "READ";
    case LM_ANIM_SAMPLE: return "ANIM_SAMPLE";
    case LM_SPECIAL: return "SPECIAL";
    case LM_DO_REAL_ZV: return "DO_REAL_ZV";
    case LM_SAMPLE: return "SAMPLE";
    case LM_TYPE: return "TYPE";
    case LM_GAME_OVER: return "GAME_OVER";
    case LM_MANUAL_ROT: return "MANUAL_ROT";
    case LM_RND_FREQ: return "RND_FREQ";
    case LM_MUSIC: return "MUSIC";
    case LM_SET_BETA: return "SET_BETA";
    case LM_DO_ROT_ZV: return "DO_ROT_ZV";
    case LM_STAGE: return "STAGE";
    case LM_FOUND_NAME: return "FOUND_NAME";
    case LM_FOUND_FLAG: return "FOUND_FLAG";
    case LM_FOUND_LIFE: return "FOUND_LIFE";
    case LM_CAMERA_TARGET: return "CAMERA_TARGET";
    case LM_DROP: return "DROP";
    case LM_FIRE: return "FIRE";
    case LM_TEST_COL: return "TEST_COL";
    case LM_FOUND_BODY: return "FOUND_BODY";
    case LM_SET_ALPHA: return "SET_ALPHA";
    case LM_STOP_BETA: return "STOP_BETA";
    case LM_DO_MAX_ZV: return "DO_MAX_ZV";
    case LM_PUT: return "PUT";
    case LM_C_VAR: return "C_VAR";
    case LM_DO_NORMAL_ZV: return "DO_NORMAL_ZV";
    case LM_DO_CARRE_ZV: return "DO_CARRE_ZV";
    case LM_SAMPLE_THEN: return "SAMPLE_THEN";
    case LM_LIGHT: return "LIGHT";
    case LM_SHAKING: return "SHAKING";
    case LM_INVENTORY: return "INVENTORY";
    case LM_FOUND_WEIGHT: return "FOUND_WEIGHT";
    case LM_UP_COOR_Y: return "UP_COOR_Y";
    case LM_SPEED: return "SPEED";
    case LM_PUT_AT: return "PUT_AT";
    case LM_DEF_ZV: return "DEF_ZV";
    case LM_HIT_OBJECT: return "HIT_OBJECT";
    case LM_GET_HARD_CLIP: return "GET_HARD_CLIP";
    case LM_ANGLE: return "ANGLE";
    case LM_REP_SAMPLE: return "REP_SAMPLE";
    case LM_THROW: return "THROW";
    case LM_WATER: return "WATER";
    case LM_PICTURE: return "PICTURE";
    case LM_STOP_SAMPLE: return "STOP_SAMPLE";
    case LM_NEXT_MUSIC: return "NEXT_MUSIC";
    case LM_FADE_MUSIC: return "FADE_MUSIC";
    case LM_STOP_HIT_OBJECT: return "STOP_HIT_OBJECT";
    case LM_COPY_ANGLE: return "COPY_ANGLE";
    case LM_END_SEQUENCE: return "END_SEQUENCE";
    case LM_SAMPLE_THEN_REPEAT: return "SAMPLE_THEN_REPEAT";
    case LM_WAIT_GAME_OVER: return "WAIT_GAME_OVER";
    case LM_GET_MATRICE: return "GET_MATRICE";
    case LM_STAGE_LIFE: return "STAGE_LIFE";
    case LM_CONTINUE_TRACK: return "CONTINUE_TRACK";
    case LM_ANIM_RESET: return "ANIM_RESET";
    case LM_RESET_MOVE_MANUAL: return "RESET_MOVE_MANUAL";
    case LM_PLUIE: return "PLUIE";
    case LM_ANIM_HYBRIDE_ONCE: return "ANIM_HYBRIDE_ONCE";
    case LM_ANIM_HYBRIDE_REPEAT: return "ANIM_HYBRIDE_REPEAT";
    case LM_MODIF_C_VAR: return "MODIF_C_VAR";
    case LM_CALL_INVENTORY: return "CALL_INVENTORY";
    case LM_BODY_RESET: return "BODY_RESET";
    case LM_DEL_INVENTORY: return "DEL_INVENTORY";
    case LM_SET_INVENTORY: return "SET_INVENTORY";
    case LM_PLAY_SEQUENCE: return "PLAY_SEQUENCE";
    case LM_2D_ANIM_SAMPLE: return "2D_ANIM_SAMPLE";
    case LM_SET_GROUND: return "SET_GROUND";
    case LM_PROTECT: return "PROTECT";
    case LM_DEF_ABS_ZV: return "DEF_ABS_ZV";
    case LM_DEF_SEQUENCE_SAMPLE: return "DEF_SEQUENCE_SAMPLE";
    case LM_READ_ON_PICTURE: return "READ_ON_PICTURE";
    case LM_FIRE_UP_DOWN: return "FIRE_UP_DOWN";
    default: return "UNKNOWN";
    }
}

static FILE* s_dumpFile = nullptr;
static std::string* s_dumpStringBuf = nullptr; // non-null during captureLifeScriptDump

// Strip ANSI escape sequences for clean file output
static std::string stripAnsi(const char* str) {
    std::string result;
    while (*str) {
        if (*str == '\x1b') {
            while (*str && *str != 'm') str++;
            if (*str) str++;
        } else {
            result += *str++;
        }
    }
    return result;
}

static void dumpPrintf(const char* fmt, ...) {
    char buf[4096];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    // Suppress console output during string capture (validation mode)
    if (!s_dumpStringBuf)
        printf("%s", buf);
    if (s_dumpFile || s_dumpStringBuf) {
        std::string clean = stripAnsi(buf);
        if (s_dumpFile)
            fprintf(s_dumpFile, "%s", clean.c_str());
        if (s_dumpStringBuf)
            *s_dumpStringBuf += clean;
    }
}

// Read a single s16 argument from bytecode
static s16 readArg(const char*& ptr)
{
    s16 val = *(s16*)ptr;
    ptr += 2;
    return val;
}

void dumpLifeScript(int lifeNum)
{
    char* basePtr = HQR_Get(listLife, lifeNum);
    if (!basePtr)
    {
        dumpPrintf(LIFE_WARN "Life script %d not found in LISTLIFE" CON_RESET "\n", lifeNum);
        return;
    }

    int scriptSize = getPakSize("LISTLIFE", lifeNum);
    const char* endPtr = basePtr + scriptSize;
    const char* ptr = basePtr;

    dumpPrintf("\n");
    dumpPrintf(LIFE_OK "========== LIFE SCRIPT %d (size: %d bytes) ==========" CON_RESET "\n", lifeNum, scriptSize);
    dumpPrintf("// Decompiled from LISTLIFE entry %d\n", lifeNum);
    dumpPrintf("// void life_%d(int lifeNum, bool callFoundLife)\n\n", lifeNum);

    int indent = 0;
    int instructionNum = 0;

    while (ptr < endPtr)
    {
        int offset = (int)(ptr - basePtr);
        s16 rawOpcode = *(s16*)ptr;
        ptr += 2;

        // Safety: if we read a zero word and we're near the end, likely padding
        if (rawOpcode == 0 && ptr >= endPtr)
            break;

        int targetActor = -1;
        bool hasTarget = false;

        if (rawOpcode & 0x8000)
        {
            hasTarget = true;
            targetActor = *(s16*)ptr;
            ptr += 2;
        }

        int opcodeIdx = rawOpcode & 0x7FFF;
        int opcodeLocated = LM_INVALID;

        if (g_gameId == AITD1)
        {
            opcodeLocated = AITD1LifeMacroTable[opcodeIdx];
        }
        else
        {
            opcodeLocated = AITD2LifeMacroTable[opcodeIdx];
        }

        const char* macroName = getLifeMacroNameForDump(opcodeLocated);

        // Print indentation
        std::string indentStr(indent * 4, ' ');
        std::string targetStr;
        if (hasTarget)
            targetStr = "worldObj[" + std::to_string(targetActor) + "].";

        // Decode based on opcode type
        switch (opcodeLocated)
        {
        case LM_IF_EGAL:
        case LM_IF_DIFFERENT:
        case LM_IF_SUP_EGAL:
        case LM_IF_SUP:
        case LM_IF_INF_EGAL:
        case LM_IF_INF:
        {
            std::string lhs = decodeEvalVar(ptr);
            std::string rhs = decodeEvalVar(ptr);
            s16 jumpOffset = readArg(ptr);

            const char* op = "==";
            if (opcodeLocated == LM_IF_DIFFERENT) op = "!=";
            else if (opcodeLocated == LM_IF_SUP_EGAL) op = ">=";
            else if (opcodeLocated == LM_IF_SUP) op = ">";
            else if (opcodeLocated == LM_IF_INF_EGAL) op = "<=";
            else if (opcodeLocated == LM_IF_INF) op = "<";

            dumpPrintf("%s/* %04X */ if (%s %s %s) { // else jump +%d\n",
                indentStr.c_str(), offset, lhs.c_str(), op, rhs.c_str(), jumpOffset);
            indent++;
            break;
        }
        case LM_GOTO:
        {
            s16 jumpOffset = readArg(ptr);
            if (jumpOffset < 0)
            {
                indent = (indent > 0) ? indent - 1 : 0;
                dumpPrintf("%s/* %04X */ } // end-if (goto %d)\n", std::string(indent * 4, ' ').c_str(), offset, jumpOffset);
            }
            else
            {
                dumpPrintf("%s/* %04X */ goto +%d;\n", indentStr.c_str(), offset, jumpOffset);
            }
            break;
        }
        case LM_SWITCH:
        {
            std::string val = decodeEvalVar(ptr);
            dumpPrintf("%s/* %04X */ switch (%s) {\n", indentStr.c_str(), offset, val.c_str());
            indent++;
            break;
        }
        case LM_CASE:
        {
            s16 caseVal = readArg(ptr);
            s16 jumpOffset = readArg(ptr);
            dumpPrintf("%s/* %04X */ case %d: // else jump +%d\n", indentStr.c_str(), offset, caseVal, jumpOffset);
            indent++;
            break;
        }
        case LM_MULTI_CASE:
        {
            s16 numCases = readArg(ptr);
            std::string cases;
            for (int i = 0; i < numCases; i++)
            {
                if (i > 0) cases += ", ";
                cases += std::to_string(readArg(ptr));
            }
            s16 jumpOffset = readArg(ptr);
            dumpPrintf("%s/* %04X */ case %s: // else jump +%d\n", indentStr.c_str(), offset, cases.c_str(), jumpOffset);
            indent++;
            break;
        }
        case LM_RETURN:
            if (indent > 0) indent--;
            dumpPrintf("%s/* %04X */ return;\n", indentStr.c_str(), offset);
            break;
        case LM_END:
            if (indent > 0) indent--;
            dumpPrintf("%s/* %04X */ end;\n", indentStr.c_str(), offset);
            break;

        // Assignment / variable ops
        case LM_BODY:
        {
            std::string val = decodeEvalVar(ptr);
            dumpPrintf("%s/* %04X */ %sBODY(%s);\n", indentStr.c_str(), offset, targetStr.c_str(), val.c_str());
            break;
        }
        case LM_BODY_RESET:
        {
            std::string body = decodeEvalVar(ptr);
            std::string anim = decodeEvalVar(ptr);
            dumpPrintf("%s/* %04X */ %sBODY_RESET(%s, %s);\n", indentStr.c_str(), offset, targetStr.c_str(), body.c_str(), anim.c_str());
            break;
        }
        case LM_ANIM_ONCE:
        {
            s16 anim = readArg(ptr);
            s16 flags = readArg(ptr);
            dumpPrintf("%s/* %04X */ %sANIM_ONCE(%d, %d);\n", indentStr.c_str(), offset, targetStr.c_str(), anim, flags);
            break;
        }
        case LM_ANIM_REPEAT:
        {
            s16 anim = readArg(ptr);
            dumpPrintf("%s/* %04X */ %sANIM_REPEAT(%d);\n", indentStr.c_str(), offset, targetStr.c_str(), anim);
            break;
        }
        case LM_ANIM_ALL_ONCE:
        {
            s16 anim = readArg(ptr);
            s16 flags = readArg(ptr);
            dumpPrintf("%s/* %04X */ %sANIM_ALL_ONCE(%d, %d);\n", indentStr.c_str(), offset, targetStr.c_str(), anim, flags);
            break;
        }
        case LM_ANIM_RESET:
        {
            s16 anim = readArg(ptr);
            s16 flags = readArg(ptr);
            dumpPrintf("%s/* %04X */ %sANIM_RESET(%d, %d);\n", indentStr.c_str(), offset, targetStr.c_str(), anim, flags);
            break;
        }
        case LM_ANIM_MOVE:
        {
            s16 stand = readArg(ptr);
            s16 walk = readArg(ptr);
            s16 run = readArg(ptr);
            s16 stop = readArg(ptr);
            s16 walkBack = readArg(ptr);
            s16 turnRight = readArg(ptr);
            s16 turnLeft = readArg(ptr);
            dumpPrintf("%s/* %04X */ ANIM_MOVE(stand=%d, walk=%d, run=%d, stop=%d, back=%d, turnR=%d, turnL=%d);\n",
                indentStr.c_str(), offset, stand, walk, run, stop, walkBack, turnRight, turnLeft);
            break;
        }
        case LM_ANIM_SAMPLE:
        {
            std::string sample = decodeEvalVar(ptr);
            s16 anim = readArg(ptr);
            s16 frame = readArg(ptr);
            dumpPrintf("%s/* %04X */ ANIM_SAMPLE(%s, anim=%d, frame=%d);\n", indentStr.c_str(), offset, sample.c_str(), anim, frame);
            break;
        }
        case LM_MOVE:
        {
            s16 trackMode = readArg(ptr);
            s16 trackNum = readArg(ptr);
            dumpPrintf("%s/* %04X */ %sMOVE(%d, %d);\n", indentStr.c_str(), offset, targetStr.c_str(), trackMode, trackNum);
            break;
        }
        case LM_DO_MOVE:
            dumpPrintf("%s/* %04X */ DO_MOVE();\n", indentStr.c_str(), offset);
            break;
        case LM_MANUAL_ROT:
            dumpPrintf("%s/* %04X */ MANUAL_ROT();\n", indentStr.c_str(), offset);
            break;
        case LM_SET_BETA:
        {
            s16 beta = readArg(ptr);
            s16 speed = readArg(ptr);
            dumpPrintf("%s/* %04X */ SET_BETA(%d, %d);\n", indentStr.c_str(), offset, beta, speed);
            break;
        }
        case LM_SET_ALPHA:
        {
            s16 alpha = readArg(ptr);
            s16 speed = readArg(ptr);
            dumpPrintf("%s/* %04X */ SET_ALPHA(%d, %d);\n", indentStr.c_str(), offset, alpha, speed);
            break;
        }
        case LM_ANGLE:
        {
            s16 a = readArg(ptr);
            s16 b = readArg(ptr);
            s16 g = readArg(ptr);
            dumpPrintf("%s/* %04X */ %sANGLE(%d, %d, %d);\n", indentStr.c_str(), offset, targetStr.c_str(), a, b, g);
            break;
        }
        case LM_COPY_ANGLE:
        {
            s16 obj = readArg(ptr);
            dumpPrintf("%s/* %04X */ COPY_ANGLE(%d);\n", indentStr.c_str(), offset, obj);
            break;
        }
        case LM_STAGE:
        {
            s16 stage = readArg(ptr);
            s16 room = readArg(ptr);
            s16 x = readArg(ptr);
            s16 y = readArg(ptr);
            s16 z = readArg(ptr);
            dumpPrintf("%s/* %04X */ %sSTAGE(%d, %d, %d, %d, %d);\n", indentStr.c_str(), offset, targetStr.c_str(), stage, room, x, y, z);
            break;
        }
        case LM_TEST_COL:
        {
            s16 val = readArg(ptr);
            dumpPrintf("%s/* %04X */ %sTEST_COL(%d);\n", indentStr.c_str(), offset, targetStr.c_str(), val);
            break;
        }
        case LM_TYPE:
        {
            s16 type = readArg(ptr);
            dumpPrintf("%s/* %04X */ %sTYPE(0x%X);\n", indentStr.c_str(), offset, targetStr.c_str(), type & 0x1FF);
            break;
        }
        case LM_LIFE:
        {
            s16 life = readArg(ptr);
            dumpPrintf("%s/* %04X */ %sLIFE(%d);\n", indentStr.c_str(), offset, targetStr.c_str(), life);
            break;
        }
        case LM_LIFE_MODE:
        {
            s16 mode = readArg(ptr);
            dumpPrintf("%s/* %04X */ %sLIFE_MODE(%d);\n", indentStr.c_str(), offset, targetStr.c_str(), mode);
            break;
        }
        case LM_STAGE_LIFE:
        {
            s16 stageLife = readArg(ptr);
            dumpPrintf("%s/* %04X */ STAGE_LIFE(%d);\n", indentStr.c_str(), offset, stageLife);
            break;
        }
        case LM_DELETE:
        {
            s16 obj = readArg(ptr);
            dumpPrintf("%s/* %04X */ DELETE(%d);\n", indentStr.c_str(), offset, obj);
            break;
        }
        case LM_SPECIAL:
        {
            s16 type = readArg(ptr);
            dumpPrintf("%s/* %04X */ SPECIAL(%d);\n", indentStr.c_str(), offset, type);
            break;
        }
        case LM_START_CHRONO:
            dumpPrintf("%s/* %04X */ START_CHRONO();\n", indentStr.c_str(), offset);
            break;
        case LM_CAMERA:
        {
            dumpPrintf("%s/* %04X */ CAMERA();\n", indentStr.c_str(), offset);
            break;
        }
        case LM_CAMERA_TARGET:
        {
            s16 target = readArg(ptr);
            dumpPrintf("%s/* %04X */ CAMERA_TARGET(%d);\n", indentStr.c_str(), offset, target);
            break;
        }
        case LM_FOUND:
        {
            s16 obj = readArg(ptr);
            dumpPrintf("%s/* %04X */ FOUND(%d);\n", indentStr.c_str(), offset, obj);
            break;
        }
        case LM_FOUND_NAME:
        {
            s16 name = readArg(ptr);
            dumpPrintf("%s/* %04X */ %sFOUND_NAME(%d);\n", indentStr.c_str(), offset, targetStr.c_str(), name);
            break;
        }
        case LM_FOUND_BODY:
        {
            s16 body = readArg(ptr);
            dumpPrintf("%s/* %04X */ %sFOUND_BODY(%d);\n", indentStr.c_str(), offset, targetStr.c_str(), body);
            break;
        }
        case LM_FOUND_FLAG:
        {
            s16 flag = readArg(ptr);
            dumpPrintf("%s/* %04X */ %sFOUND_FLAG(0x%X);\n", indentStr.c_str(), offset, targetStr.c_str(), flag);
            break;
        }
        case LM_FOUND_WEIGHT:
        {
            s16 weight = readArg(ptr);
            dumpPrintf("%s/* %04X */ %sFOUND_WEIGHT(%d);\n", indentStr.c_str(), offset, targetStr.c_str(), weight);
            break;
        }
        case LM_FOUND_LIFE:
        {
            s16 life = readArg(ptr);
            dumpPrintf("%s/* %04X */ %sFOUND_LIFE(%d);\n", indentStr.c_str(), offset, targetStr.c_str(), life);
            break;
        }
        case LM_TAKE:
        {
            s16 obj = readArg(ptr);
            dumpPrintf("%s/* %04X */ TAKE(%d);\n", indentStr.c_str(), offset, obj);
            break;
        }
        case LM_IN_HAND:
        {
            s16 obj = readArg(ptr);
            dumpPrintf("%s/* %04X */ IN_HAND(%d);\n", indentStr.c_str(), offset, obj);
            break;
        }
        case LM_DROP:
        {
            std::string val = decodeEvalVar(ptr);
            s16 arg2 = readArg(ptr);
            dumpPrintf("%s/* %04X */ DROP(%s, %d);\n", indentStr.c_str(), offset, val.c_str(), arg2);
            break;
        }
        case LM_PUT:
        {
            s16 idx = readArg(ptr);
            s16 x = readArg(ptr);
            s16 y = readArg(ptr);
            s16 z = readArg(ptr);
            s16 room = readArg(ptr);
            s16 stage = readArg(ptr);
            s16 a = readArg(ptr);
            s16 b = readArg(ptr);
            s16 g = readArg(ptr);
            dumpPrintf("%s/* %04X */ PUT(%d, x=%d, y=%d, z=%d, room=%d, stage=%d, a=%d, b=%d, g=%d);\n",
                indentStr.c_str(), offset, idx, x, y, z, room, stage, a, b, g);
            break;
        }
        case LM_PUT_AT:
        {
            s16 obj1 = readArg(ptr);
            s16 obj2 = readArg(ptr);
            dumpPrintf("%s/* %04X */ PUT_AT(%d, %d);\n", indentStr.c_str(), offset, obj1, obj2);
            break;
        }
        case LM_READ:
        {
            s16 type = readArg(ptr);
            s16 index = readArg(ptr);
            s16 vocIndex = -1;
            if (g_gameId == AITD1)
            {
                vocIndex = readArg(ptr);
            }
            dumpPrintf("%s/* %04X */ READ(type=%d, index=%d, voc=%d);\n", indentStr.c_str(), offset, type, index, vocIndex);
            break;
        }
        case LM_READ_ON_PICTURE:
        {
            s16 args[8];
            for (int i = 0; i < 8; i++) args[i] = readArg(ptr);
            s16 vocIdx = -1;
            if (g_gameId == AITD1) vocIdx = readArg(ptr);
            dumpPrintf("%s/* %04X */ READ_ON_PICTURE(%d, %d, %d, %d, %d, %d, %d, %d, voc=%d);\n",
                indentStr.c_str(), offset, args[0], args[1], args[2], args[3], args[4], args[5], args[6], args[7], vocIdx);
            break;
        }
        case LM_HIT:
        {
            s16 anim = readArg(ptr);
            s16 startFrame = readArg(ptr);
            s16 group = readArg(ptr);
            s16 hitBox = readArg(ptr);
            std::string force = decodeEvalVar(ptr);
            s16 nextAnim = readArg(ptr);
            dumpPrintf("%s/* %04X */ HIT(anim=%d, frame=%d, group=%d, hitbox=%d, force=%s, next=%d);\n",
                indentStr.c_str(), offset, anim, startFrame, group, hitBox, force.c_str(), nextAnim);
            break;
        }
        case LM_FIRE:
        {
            s16 fireAnim = readArg(ptr);
            s16 shootFrame = readArg(ptr);
            s16 emitPoint = readArg(ptr);
            s16 zvSize = readArg(ptr);
            s16 hitForce = readArg(ptr);
            s16 nextAnim = readArg(ptr);
            dumpPrintf("%s/* %04X */ FIRE(anim=%d, frame=%d, emit=%d, zv=%d, force=%d, next=%d);\n",
                indentStr.c_str(), offset, fireAnim, shootFrame, emitPoint, zvSize, hitForce, nextAnim);
            break;
        }
        case LM_HIT_OBJECT:
        {
            s16 flags = readArg(ptr);
            s16 force = readArg(ptr);
            dumpPrintf("%s/* %04X */ HIT_OBJECT(%d, %d);\n", indentStr.c_str(), offset, flags, force);
            break;
        }
        case LM_STOP_HIT_OBJECT:
            dumpPrintf("%s/* %04X */ STOP_HIT_OBJECT();\n", indentStr.c_str(), offset);
            break;
        case LM_THROW:
        {
            s16 args[7];
            for (int i = 0; i < 7; i++) args[i] = readArg(ptr);
            dumpPrintf("%s/* %04X */ THROW(%d, %d, %d, %d, %d, %d, %d);\n",
                indentStr.c_str(), offset, args[0], args[1], args[2], args[3], args[4], args[5], args[6]);
            break;
        }
        case LM_SAMPLE:
        {
            std::string sample = decodeEvalVar(ptr);
            dumpPrintf("%s/* %04X */ SAMPLE(%s);\n", indentStr.c_str(), offset, sample.c_str());
            break;
        }
        case LM_REP_SAMPLE:
        {
            std::string sample = decodeEvalVar(ptr);
            s16 freq = readArg(ptr);
            dumpPrintf("%s/* %04X */ REP_SAMPLE(%s, freq=%d);\n", indentStr.c_str(), offset, sample.c_str(), freq);
            break;
        }
        case LM_STOP_SAMPLE:
            dumpPrintf("%s/* %04X */ STOP_SAMPLE();\n", indentStr.c_str(), offset);
            break;
        case LM_SAMPLE_THEN:
        {
            std::string s1 = decodeEvalVar(ptr);
            std::string s2 = decodeEvalVar(ptr);
            dumpPrintf("%s/* %04X */ SAMPLE_THEN(%s, %s);\n", indentStr.c_str(), offset, s1.c_str(), s2.c_str());
            break;
        }
        case LM_SAMPLE_THEN_REPEAT:
        {
            std::string s1 = decodeEvalVar(ptr);
            std::string s2 = decodeEvalVar(ptr);
            dumpPrintf("%s/* %04X */ SAMPLE_THEN_REPEAT(%s, %s);\n", indentStr.c_str(), offset, s1.c_str(), s2.c_str());
            break;
        }
        case LM_MUSIC:
        {
            s16 musicIdx = readArg(ptr);
            dumpPrintf("%s/* %04X */ MUSIC(%d);\n", indentStr.c_str(), offset, musicIdx);
            break;
        }
        case LM_NEXT_MUSIC:
        {
            s16 musicIdx = readArg(ptr);
            dumpPrintf("%s/* %04X */ NEXT_MUSIC(%d);\n", indentStr.c_str(), offset, musicIdx);
            break;
        }
        case LM_FADE_MUSIC:
        {
            s16 musicIdx = readArg(ptr);
            dumpPrintf("%s/* %04X */ FADE_MUSIC(%d);\n", indentStr.c_str(), offset, musicIdx);
            break;
        }
        case LM_RND_FREQ:
        {
            s16 val = readArg(ptr);
            dumpPrintf("%s/* %04X */ RND_FREQ(%d);\n", indentStr.c_str(), offset, val);
            break;
        }
        case LM_LIGHT:
        {
            s16 val = readArg(ptr);
            dumpPrintf("%s/* %04X */ LIGHT(%d);\n", indentStr.c_str(), offset, val);
            break;
        }
        case LM_SHAKING:
        {
            s16 amp = readArg(ptr);
            dumpPrintf("%s/* %04X */ SHAKING(%d);\n", indentStr.c_str(), offset, amp);
            break;
        }
        case LM_WATER:
        {
            s16 val = readArg(ptr);
            dumpPrintf("%s/* %04X */ WATER(%d);\n", indentStr.c_str(), offset, val);
            break;
        }
        case LM_PICTURE:
        {
            s16 picIdx = readArg(ptr);
            s16 delay = readArg(ptr);
            s16 sampleId = readArg(ptr);
            dumpPrintf("%s/* %04X */ PICTURE(%d, delay=%d, sample=%d);\n", indentStr.c_str(), offset, picIdx, delay, sampleId);
            break;
        }
        case LM_VAR:
        {
            s16 idx = readArg(ptr);
            std::string val = decodeEvalVar(ptr);
            dumpPrintf("%s/* %04X */ vars[%d] = %s;\n", indentStr.c_str(), offset, idx, val.c_str());
            break;
        }
        case LM_INC:
        {
            s16 idx = readArg(ptr);
            dumpPrintf("%s/* %04X */ vars[%d]++;\n", indentStr.c_str(), offset, idx);
            break;
        }
        case LM_DEC:
        {
            s16 idx = readArg(ptr);
            dumpPrintf("%s/* %04X */ vars[%d]--;\n", indentStr.c_str(), offset, idx);
            break;
        }
        case LM_ADD:
        {
            s16 idx = readArg(ptr);
            std::string val = decodeEvalVar(ptr);
            dumpPrintf("%s/* %04X */ vars[%d] += %s;\n", indentStr.c_str(), offset, idx, val.c_str());
            break;
        }
        case LM_SUB:
        {
            s16 idx = readArg(ptr);
            std::string val = decodeEvalVar(ptr);
            dumpPrintf("%s/* %04X */ vars[%d] -= %s;\n", indentStr.c_str(), offset, idx, val.c_str());
            break;
        }
        case LM_C_VAR:
        case LM_MODIF_C_VAR:
        {
            s16 idx = readArg(ptr);
            std::string val = decodeEvalVar(ptr);
            dumpPrintf("%s/* %04X */ CVars[%d] = %s;\n", indentStr.c_str(), offset, idx, val.c_str());
            break;
        }
        case LM_MESSAGE:
        {
            s16 msg = readArg(ptr);
            dumpPrintf("%s/* %04X */ MESSAGE(%d);\n", indentStr.c_str(), offset, msg);
            break;
        }
        case LM_MESSAGE_VALUE:
        {
            s16 msg = readArg(ptr);
            s16 val = readArg(ptr);
            dumpPrintf("%s/* %04X */ MESSAGE_VALUE(%d, %d);\n", indentStr.c_str(), offset, msg, val);
            break;
        }
        case LM_DO_REAL_ZV:
            dumpPrintf("%s/* %04X */ DO_REAL_ZV();\n", indentStr.c_str(), offset);
            break;
        case LM_DO_ROT_ZV:
            dumpPrintf("%s/* %04X */ DO_ROT_ZV();\n", indentStr.c_str(), offset);
            break;
        case LM_DO_MAX_ZV:
            dumpPrintf("%s/* %04X */ DO_MAX_ZV();\n", indentStr.c_str(), offset);
            break;
        case LM_DO_NORMAL_ZV:
            dumpPrintf("%s/* %04X */ DO_NORMAL_ZV();\n", indentStr.c_str(), offset);
            break;
        case LM_DO_CARRE_ZV:
            dumpPrintf("%s/* %04X */ DO_CARRE_ZV();\n", indentStr.c_str(), offset);
            break;
        case LM_DEF_ZV:
        {
            s16 args[6];
            for (int i = 0; i < 6; i++) args[i] = readArg(ptr);
            dumpPrintf("%s/* %04X */ DEF_ZV(%d, %d, %d, %d, %d, %d);\n",
                indentStr.c_str(), offset, args[0], args[1], args[2], args[3], args[4], args[5]);
            break;
        }
        case LM_DEF_ABS_ZV:
        {
            s16 args[6];
            for (int i = 0; i < 6; i++) args[i] = readArg(ptr);
            dumpPrintf("%s/* %04X */ DEF_ABS_ZV(%d, %d, %d, %d, %d, %d);\n",
                indentStr.c_str(), offset, args[0], args[1], args[2], args[3], args[4], args[5]);
            break;
        }
        case LM_GET_HARD_CLIP:
            dumpPrintf("%s/* %04X */ GET_HARD_CLIP();\n", indentStr.c_str(), offset);
            break;
        case LM_UP_COOR_Y:
            dumpPrintf("%s/* %04X */ UP_COOR_Y();\n", indentStr.c_str(), offset);
            break;
        case LM_SPEED:
        {
            std::string val = decodeEvalVar(ptr);
            dumpPrintf("%s/* %04X */ SPEED(%s);\n", indentStr.c_str(), offset, val.c_str());
            break;
        }
        case LM_INVENTORY:
        {
            s16 val = readArg(ptr);
            dumpPrintf("%s/* %04X */ INVENTORY(%d);\n", indentStr.c_str(), offset, val);
            break;
        }
        case LM_GAME_OVER:
            dumpPrintf("%s/* %04X */ GAME_OVER();\n", indentStr.c_str(), offset);
            break;
        case LM_WAIT_GAME_OVER:
            dumpPrintf("%s/* %04X */ WAIT_GAME_OVER();\n", indentStr.c_str(), offset);
            break;
        case LM_END_SEQUENCE:
            dumpPrintf("%s/* %04X */ END_SEQUENCE();\n", indentStr.c_str(), offset);
            break;
        case LM_PROTECT:
            dumpPrintf("%s/* %04X */ PROTECT();\n", indentStr.c_str(), offset);
            break;
        case LM_CONTINUE_TRACK:
            dumpPrintf("%s/* %04X */ CONTINUE_TRACK();\n", indentStr.c_str(), offset);
            break;
        case LM_RESET_MOVE_MANUAL:
            dumpPrintf("%s/* %04X */ RESET_MOVE_MANUAL();\n", indentStr.c_str(), offset);
            break;
        case LM_CALL_INVENTORY:
            dumpPrintf("%s/* %04X */ CALL_INVENTORY();\n", indentStr.c_str(), offset);
            break;
        case LM_STOP_BETA:
            dumpPrintf("%s/* %04X */ STOP_BETA();\n", indentStr.c_str(), offset);
            break;
        case LM_PLUIE:
        {
            s16 val = readArg(ptr);
            dumpPrintf("%s/* %04X */ PLUIE(%d);\n", indentStr.c_str(), offset, val);
            break;
        }
        case LM_SET_GROUND:
        {
            s16 val = readArg(ptr);
            dumpPrintf("%s/* %04X */ SET_GROUND(%d);\n", indentStr.c_str(), offset, val);
            break;
        }
        case LM_SET_INVENTORY:
        {
            s16 val = readArg(ptr);
            dumpPrintf("%s/* %04X */ SET_INVENTORY(%d);\n", indentStr.c_str(), offset, val);
            break;
        }
        case LM_DEL_INVENTORY:
        {
            s16 val = readArg(ptr);
            dumpPrintf("%s/* %04X */ DEL_INVENTORY(%d);\n", indentStr.c_str(), offset, val);
            break;
        }
        case LM_PLAY_SEQUENCE:
        {
            s16 seq = readArg(ptr);
            s16 fadeIn = readArg(ptr);
            s16 fadeOut = readArg(ptr);
            dumpPrintf("%s/* %04X */ PLAY_SEQUENCE(%d, %d, %d);\n", indentStr.c_str(), offset, seq, fadeIn, fadeOut);
            break;
        }
        case LM_DEF_SEQUENCE_SAMPLE:
        {
            s16 numParams = readArg(ptr);
            dumpPrintf("%s/* %04X */ DEF_SEQUENCE_SAMPLE(", indentStr.c_str(), offset);
            for (int i = 0; i < numParams; i++)
            {
                s16 frame = readArg(ptr);
                s16 sample = readArg(ptr);
                if (i > 0) printf(", ");
                dumpPrintf("{frame=%d, sample=%d}", frame, sample);
            }
            dumpPrintf(");\n");
            break;
        }
        case LM_ANIM_HYBRIDE_ONCE:
        {
            s16 anim = readArg(ptr);
            s16 body = readArg(ptr);
            dumpPrintf("%s/* %04X */ ANIM_HYBRIDE_ONCE(%d, %d);\n", indentStr.c_str(), offset, anim, body);
            break;
        }
        case LM_ANIM_HYBRIDE_REPEAT:
        {
            s16 anim = readArg(ptr);
            s16 body = readArg(ptr);
            dumpPrintf("%s/* %04X */ ANIM_HYBRIDE_REPEAT(%d, %d);\n", indentStr.c_str(), offset, anim, body);
            break;
        }
        case LM_2D_ANIM_SAMPLE:
        {
            std::string sample = decodeEvalVar(ptr);
            s16 anim = readArg(ptr);
            s16 frame = readArg(ptr);
            dumpPrintf("%s/* %04X */ 2D_ANIM_SAMPLE(%s, %d, %d);\n", indentStr.c_str(), offset, sample.c_str(), anim, frame);
            break;
        }
        default:
            dumpPrintf("%s/* %04X */ %s(opcode=0x%02X); // UNHANDLED\n", indentStr.c_str(), offset, macroName, opcodeIdx);
            break;
        }

        instructionNum++;

        // Safety bail-out
        if (instructionNum > 10000)
        {
            dumpPrintf(LIFE_WARN "Decompiler safety limit reached for Life %d" CON_RESET "\n", lifeNum);
            break;
        }
    }

    dumpPrintf(LIFE_OK "========== END LIFE SCRIPT %d (%d instructions) ==========" CON_RESET "\n\n", lifeNum, instructionNum);
}

void dumpAllLifeScripts()
{
    // Open output file for the dump
    s_dumpFile = fopen("LISTLIFE_dump.txt", "w");
    if (!s_dumpFile)
        printf(LIFE_WARN "Could not open LISTLIFE_dump.txt for writing" CON_RESET "\n");

    dumpPrintf("// LISTLIFE Bytecode Dump - Decompiled to pseudo-C\n");
    dumpPrintf("// Auto-generated by AITD Re-Haunted native life script system\n\n");

    int scriptCount = 0;
    // Iterate through available life scripts
    for (int i = 0; i < 1000; i++)
    {
        int size = getPakSize("LISTLIFE", i);
        if (size > 0)
        {
            dumpLifeScript(i);
            scriptCount++;
        }
    }

    dumpPrintf("\n// Total: %d life scripts dumped\n", scriptCount);

    if (s_dumpFile)
    {
        fclose(s_dumpFile);
        s_dumpFile = nullptr;
        printf(LIFE_OK "Dumped %d life scripts to LISTLIFE_dump.txt" CON_RESET "\n", scriptCount);
    }
}

// Decompile a single life script to a string (used for validation).
// Console output is suppressed during capture.
static std::string captureLifeScriptDump(int lifeNum)
{
    std::string result;
    s_dumpStringBuf = &result;
    dumpLifeScript(lifeNum);
    s_dumpStringBuf = nullptr;
    return result;
}

void validateNativeLifeScriptsAgainstDump(const char* dumpFilePath)
{
    FILE* f = fopen(dumpFilePath, "r");
    if (!f)
    {
        printf(LIFE_WARN "Cannot open '%s' - native scripts not validated against bytecode" CON_RESET "\n", dumpFilePath);
        return;
    }

    // Parse the dump file into a per-script map.
    // Each entry spans from the "[LIFE] ========== LIFE SCRIPT N " header line
    // to its matching "[LIFE] ========== END LIFE SCRIPT N " footer (inclusive).
    std::map<int, std::string> refDump;
    char line[8192];
    int currentScript = -1;
    std::string currentText;

    while (fgets(line, sizeof(line), f))
    {
        int scriptNum;
        if (sscanf(line, "[LIFE] ========== LIFE SCRIPT %d", &scriptNum) == 1)
        {
            currentScript = scriptNum;
            currentText = line;
        }
        else if (currentScript >= 0)
        {
            currentText += line;
            if (sscanf(line, "[LIFE] ========== END LIFE SCRIPT %d", &scriptNum) == 1
                && scriptNum == currentScript)
            {
                refDump[currentScript] = currentText;
                currentScript = -1;
                currentText.clear();
            }
        }
    }
    fclose(f);

    // Compare each registered native script against the reference dump.
    int kept = 0, disabled = 0;
    std::vector<int> toRemove;

    for (auto& [lifeNum, func] : s_nativeLifeScripts)
    {
        auto refIt = refDump.find(lifeNum);
        if (refIt == refDump.end())
        {
            printf(LIFE_WARN "Life %d: not found in '%s', disabling native script" CON_RESET "\n", lifeNum, dumpFilePath);
            toRemove.push_back(lifeNum);
            disabled++;
            continue;
        }

        std::string liveDump = captureLifeScriptDump(lifeNum);

        // dumpLifeScript emits a leading "\n" before the header; align to "[LIFE]"
        size_t liveStart = liveDump.find("[LIFE]");
        std::string liveContent = (liveStart != std::string::npos) ? liveDump.substr(liveStart) : liveDump;

        if (liveContent == refIt->second)
        {
            kept++;
        }
        else
        {
            printf(LIFE_WARN "Life %d: bytecode differs from dump, disabling native script" CON_RESET "\n", lifeNum);
            toRemove.push_back(lifeNum);
            disabled++;
        }
    }

    for (int lifeNum : toRemove)
        s_nativeLifeScripts.erase(lifeNum);

    printf(LIFE_OK "Native script validation: %d verified, %d disabled (bytecode mismatch)" CON_RESET "\n", kept, disabled);
}

//////////////////////////////////////////////////////////////////////////////
// Native Life Script C Code Generator (Label-Based Control Flow)
//////////////////////////////////////////////////////////////////////////////

static FILE* s_genFile = nullptr;

static void genPrintf(const char* fmt, ...) {
    char buf[4096];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    if (s_genFile) fprintf(s_genFile, "%s", buf);
}

static void genFlush() {
    if (s_genFile) fflush(s_genFile);
}

// Helper to format a hex digit string
static std::string toHexStr(int val) {
    char buf[16];
    snprintf(buf, sizeof(buf), "0x%02X", val);
    return buf;
}

// Decode an evalVar expression from bytecode into compilable C code.
// Returns a C expression string using nativeLifeHelpers.h accessors.
static std::string decodeEvalVarC(const char*& ptr)
{
    s16 var1 = *(s16*)ptr;
    ptr += 2;

    if (var1 == -1)
    {
        s16 val = *(s16*)ptr;
        ptr += 2;
        return std::to_string(val);
    }
    else if (var1 == 0)
    {
        s16 idx = *(s16*)ptr;
        ptr += 2;
        return "vars[" + std::to_string(idx) + "]";
    }
    else
    {
        bool isOtherObj = (var1 & 0x8000) != 0;
        int objNum = -1;
        if (isOtherObj)
        {
            objNum = *(s16*)ptr;
            ptr += 2;
        }

        int field = ((var1 & 0x7FFF) - 1) & 0x7FFF;

        if (isOtherObj)
        {
            // Other-object fields with extra parameters
            switch (field)
            {
            case 0x0E: { // DIST
                s16 distObj = *(s16*)ptr; ptr += 2;
                return "[&]() -> int { LifeTargetScope _ts(" + std::to_string(objNum) + "); return life_GetDIST(" + std::to_string(distObj) + "); }()";
            }
            case 0x10: { // FOUND - recursive evalVar
                std::string inner = decodeEvalVarC(ptr);
                return "life_GetFOUND(" + inner + ")";
            }
            case 0x12: { // POSREL
                s16 posrelObj = *(s16*)ptr; ptr += 2;
                return "[&]() -> int { LifeTargetScope _ts(" + std::to_string(objNum) + "); return life_GetPOSREL(" + std::to_string(posrelObj) + "); }()";
            }
            case 0x1C: { // RANDOM
                s16 range = *(s16*)ptr; ptr += 2;
                return "life_GetRANDOM(" + std::to_string(range) + ")";
            }
            case 0x20: { // OBJ_FOUND
                s16 objIdx = *(s16*)ptr; ptr += 2;
                return "life_GetOBJ_FOUND(" + std::to_string(objIdx) + ")";
            }
            case 0x22: { // TEST_ZV_END_ANIM
                s16 anim = *(s16*)ptr; ptr += 2;
                s16 frame = *(s16*)ptr; ptr += 2;
                return "[&]() -> int { LifeTargetScope _ts(" + std::to_string(objNum) + "); return life_GetTEST_ZV_END_ANIM(" + std::to_string(anim) + ", " + std::to_string(frame) + "); }()";
            }
            case 0x24: { // C_VAR
                s16 cvarIdx = *(s16*)ptr; ptr += 2;
                return "life_GetCVAR(" + std::to_string(cvarIdx) + ")";
            }
            case 0x26: { // THROWN
                s16 objIdx = *(s16*)ptr; ptr += 2;
                return "life_GetTHROWN(" + std::to_string(objIdx) + ")";
            }
            // Global fields that ignore the object prefix
            case 0x11: return "life_GetACTION()";
            case 0x13: return "life_GetKEYBOARD()";
            case 0x14: return "life_GetBUTTON()";
            case 0x19: return "life_GetIN_HAND()";
            case 0x1B: return "life_GetCAMERA()";
            case 0x23: return "life_GetCURRENT_MUSIC()";
            default:
                return "life_ObjGetField(" + std::to_string(objNum) + ", " + toHexStr(field) + ")";
            }
        }
        else
        {
            // Self-actor fields
            switch (field)
            {
            case 0x00: return "life_GetCOL()";
            case 0x01: return "life_GetHARD_DEC()";
            case 0x02: return "life_GetHARD_COL()";
            case 0x03: return "life_GetHIT()";
            case 0x04: return "life_GetHIT_BY()";
            case 0x05: return "life_GetANIM()";
            case 0x06: return "life_GetEND_ANIM()";
            case 0x07: return "life_GetFRAME()";
            case 0x08: return "life_GetEND_FRAME()";
            case 0x09: return "life_GetBODY()";
            case 0x0A: return "life_GetMARK()";
            case 0x0B: return "life_GetNUM_TRACK()";
            case 0x0C: return "life_GetCHRONO()";
            case 0x0D: return "life_GetROOM_CHRONO()";
            case 0x0E: { // DIST
                s16 distObj = *(s16*)ptr; ptr += 2;
                return "life_GetDIST(" + std::to_string(distObj) + ")";
            }
            case 0x0F: return "life_GetCOL_BY()";
            case 0x10: { // FOUND - recursive evalVar
                std::string inner = decodeEvalVarC(ptr);
                return "life_GetFOUND(" + inner + ")";
            }
            case 0x11: return "life_GetACTION()";
            case 0x12: { // POSREL
                s16 posrelObj = *(s16*)ptr; ptr += 2;
                return "life_GetPOSREL(" + std::to_string(posrelObj) + ")";
            }
            case 0x13: return "life_GetKEYBOARD()";
            case 0x14: return "life_GetBUTTON()";
            case 0x15: return "life_GetCOL_OR_COL_BY()";
            case 0x16: return "life_GetALPHA()";
            case 0x17: return "life_GetBETA()";
            case 0x18: return "life_GetGAMMA()";
            case 0x19: return "life_GetIN_HAND()";
            case 0x1A: return "life_GetHIT_FORCE()";
            case 0x1B: return "life_GetCAMERA()";
            case 0x1C: { // RANDOM
                s16 range = *(s16*)ptr; ptr += 2;
                return "life_GetRANDOM(" + std::to_string(range) + ")";
            }
            case 0x1D: return "life_GetFALLING()";
            case 0x1E: return "life_GetROOM()";
            case 0x1F: return "life_GetLIFE()";
            case 0x20: { // OBJ_FOUND
                s16 objIdx = *(s16*)ptr; ptr += 2;
                return "life_GetOBJ_FOUND(" + std::to_string(objIdx) + ")";
            }
            case 0x21: return "life_GetROOM_Y()";
            case 0x22: { // TEST_ZV_END_ANIM
                s16 anim = *(s16*)ptr; ptr += 2;
                s16 frame = *(s16*)ptr; ptr += 2;
                return "life_GetTEST_ZV_END_ANIM(" + std::to_string(anim) + ", " + std::to_string(frame) + ")";
            }
            case 0x23: return "life_GetCURRENT_MUSIC()";
            case 0x24: { // C_VAR
                s16 cvarIdx = *(s16*)ptr; ptr += 2;
                return "life_GetCVAR(" + std::to_string(cvarIdx) + ")";
            }
            case 0x25: return "life_GetSTAGE()";
            case 0x26: { // THROWN
                s16 objIdx = *(s16*)ptr; ptr += 2;
                return "life_GetTHROWN(" + std::to_string(objIdx) + ")";
            }
            default:
                return "/* UNKNOWN_FIELD_" + toHexStr(field) + " */ 0";
            }
        }
    }
}

// Read a plain s16 argument from bytecode as a string (for code generation)
static std::string readArgStr(const char*& ptr)
{
    s16 val = *(s16*)ptr;
    ptr += 2;
    return std::to_string(val);
}

// Skip a plain s16 argument (2 bytes)
static void skipPlainArg(const char*& ptr) { ptr += 2; }

// Collect all jump target offsets for label generation.
// Byte counting matches the actual bytecode format exactly.
static std::set<int> collectJumpTargets(const char* basePtr, int scriptSize) {
    std::set<int> targets;
    const char* ptr = basePtr;
    const char* endPtr = basePtr + scriptSize;
    while (ptr < endPtr) {
        s16 rawOpcode = *(s16*)ptr; ptr += 2;
        if (rawOpcode == 0 && ptr >= endPtr) break;
        if (rawOpcode & 0x8000) ptr += 2; // target actor
        int opcodeIdx = rawOpcode & 0x7FFF;
        int op = (g_gameId == AITD1) ? AITD1LifeMacroTable[opcodeIdx] : AITD2LifeMacroTable[opcodeIdx];
        switch (op) {
        // Control flow - these produce jump targets
        case LM_IF_EGAL: case LM_IF_DIFFERENT: case LM_IF_SUP_EGAL:
        case LM_IF_SUP: case LM_IF_INF_EGAL: case LM_IF_INF:
            skipEvalVar(ptr); skipEvalVar(ptr);
            { s16 j = *(s16*)ptr; ptr += 2; int t = (int)(ptr - basePtr) + j * 2; if (t >= 0 && t < scriptSize) targets.insert(t); }
            break;
        case LM_GOTO:
            { s16 j = *(s16*)ptr; ptr += 2; int t = (int)(ptr - basePtr) + j * 2; if (t >= 0 && t < scriptSize) targets.insert(t); }
            break;
        case LM_SWITCH: skipEvalVar(ptr); break;
        case LM_CASE:
            ptr += 2;
            { s16 j = *(s16*)ptr; ptr += 2; int t = (int)(ptr - basePtr) + j * 2; if (t >= 0 && t < scriptSize) targets.insert(t); }
            break;
        case LM_MULTI_CASE:
            { s16 n = *(s16*)ptr; ptr += 2; ptr += n * 2; s16 j = *(s16*)ptr; ptr += 2; int t = (int)(ptr - basePtr) + j * 2; if (t >= 0 && t < scriptSize) targets.insert(t); }
            break;
        // 1 evalVar
        case LM_BODY: case LM_SAMPLE: case LM_SPEED:
            skipEvalVar(ptr); break;
        // 2 evalVar
        case LM_BODY_RESET: case LM_SAMPLE_THEN: case LM_SAMPLE_THEN_REPEAT:
            skipEvalVar(ptr); skipEvalVar(ptr); break;
        // 1 evalVar + 1 s16
        case LM_DROP:
            skipEvalVar(ptr); ptr += 2; break;
        // 1 s16 + 1 evalVar
        case LM_VAR: case LM_ADD: case LM_SUB: case LM_C_VAR: case LM_MODIF_C_VAR:
            ptr += 2; skipEvalVar(ptr); break;
        // 1 evalVar + 2 s16
        case LM_ANIM_SAMPLE: case LM_2D_ANIM_SAMPLE:
            skipEvalVar(ptr); ptr += 4; break;
        // 1 evalVar + 1 s16
        case LM_REP_SAMPLE:
            skipEvalVar(ptr); ptr += 2; break;
        // 4 s16 + 1 evalVar + 1 s16
        case LM_HIT:
            ptr += 8; skipEvalVar(ptr); ptr += 2; break;
        // Plain s16 arg opcodes
        case LM_ANIM_ONCE: case LM_ANIM_ALL_ONCE: case LM_ANIM_RESET:
        case LM_HIT_OBJECT: case LM_MESSAGE_VALUE: case LM_ANIM_HYBRIDE_ONCE:
        case LM_ANIM_HYBRIDE_REPEAT: case LM_SET_BETA: case LM_SET_ALPHA:
        case LM_MOVE: case LM_PUT_AT:
            ptr += 4; break; // 2 s16 = 4 bytes
        case LM_ANIM_REPEAT: case LM_MUSIC: case LM_NEXT_MUSIC: case LM_FADE_MUSIC:
        case LM_TYPE: case LM_LIFE: case LM_LIFE_MODE: case LM_DELETE: case LM_SPECIAL:
        case LM_CAMERA_TARGET: case LM_FOUND: case LM_TAKE: case LM_IN_HAND:
        case LM_FOUND_NAME: case LM_FOUND_BODY: case LM_FOUND_FLAG: case LM_FOUND_WEIGHT:
        case LM_FOUND_LIFE: case LM_COPY_ANGLE: case LM_STAGE_LIFE: case LM_TEST_COL:
        case LM_MESSAGE: case LM_RND_FREQ: case LM_LIGHT: case LM_SHAKING: case LM_WATER:
        case LM_INVENTORY: case LM_PLUIE: case LM_SET_GROUND: case LM_SET_INVENTORY:
        case LM_DEL_INVENTORY: case LM_INC: case LM_DEC: case LM_GET_MATRICE:
            ptr += 2; break; // 1 s16 = 2 bytes
        case LM_ANGLE:
            ptr += 6; break; // 3 s16
        case LM_PICTURE: case LM_PLAY_SEQUENCE:
            ptr += 6; break; // 3 s16
        case LM_STAGE:
            ptr += 10; break; // 5 s16
        case LM_FIRE:
            ptr += 12; break; // 6 s16
        case LM_ANIM_MOVE: case LM_THROW:
            ptr += 14; break; // 7 s16
        case LM_PUT:
            ptr += 18; break; // 9 s16
        case LM_DEF_ZV: case LM_DEF_ABS_ZV:
            ptr += 12; break; // 6 s16
        case LM_READ:
            ptr += (g_gameId == AITD1) ? 6 : 4; break;
        case LM_READ_ON_PICTURE:
            ptr += (g_gameId == AITD1) ? 18 : 16; break;
        case LM_DEF_SEQUENCE_SAMPLE:
            { s16 n = *(s16*)ptr; ptr += 2 + n * 4; } break;
        case LM_FIRE_UP_DOWN:
            ptr += 12; break; // 6 s16 (AITD3)
        // 0 args
        case LM_DO_MOVE: case LM_MANUAL_ROT: case LM_DO_REAL_ZV: case LM_DO_ROT_ZV:
        case LM_DO_MAX_ZV: case LM_DO_NORMAL_ZV: case LM_DO_CARRE_ZV: case LM_GET_HARD_CLIP:
        case LM_UP_COOR_Y: case LM_START_CHRONO: case LM_CAMERA: case LM_GAME_OVER:
        case LM_WAIT_GAME_OVER: case LM_END_SEQUENCE: case LM_PROTECT: case LM_CONTINUE_TRACK:
        case LM_RESET_MOVE_MANUAL: case LM_CALL_INVENTORY: case LM_STOP_BETA: case LM_STOP_SAMPLE:
        case LM_STOP_HIT_OBJECT: case LM_RETURN: case LM_END:
            break;
        default:
            break;
        }
    }
    return targets;
}

// Generate a single native life script function as compilable C code
void generateNativeLifeScriptC(int lifeNum)
{
    char* basePtr = HQR_Get(listLife, lifeNum);
    if (!basePtr) return;

    int scriptSize = getPakSize("LISTLIFE", lifeNum);
    const char* endPtr = basePtr + scriptSize;
    const char* ptr = basePtr;

    // First pass: collect jump targets for label placement
    std::set<int> jumpTargets = collectJumpTargets(basePtr, scriptSize);

    // Function header
    genPrintf("// Life script %d - auto-generated from bytecode\n", lifeNum);
    genPrintf("static void nativeLife_%d(int lifeNum, bool callFoundLife)\n{\n", lifeNum);
    genPrintf("    int switchVal = 0;\n");
    genPrintf("    (void)switchVal; (void)lifeNum; (void)callFoundLife;\n\n");

    int instructionNum = 0;

    while (ptr < endPtr)
    {
        int offset = (int)(ptr - basePtr);

        // Emit label if this offset is a jump target
        if (jumpTargets.count(offset))
            genPrintf("L_%04X:\n", offset);

        s16 rawOpcode = *(s16*)ptr;
        ptr += 2;

        if (rawOpcode == 0 && ptr >= endPtr) break;

        int targetActor = -1;
        bool hasTarget = false;

        if (rawOpcode & 0x8000)
        {
            hasTarget = true;
            targetActor = *(s16*)ptr;
            ptr += 2;
        }

        int opcodeIdx = rawOpcode & 0x7FFF;
        int opcodeLocated = (g_gameId == AITD1) ? AITD1LifeMacroTable[opcodeIdx] : AITD2LifeMacroTable[opcodeIdx];

        std::string tgt = hasTarget ? std::to_string(targetActor) : "";

        switch (opcodeLocated)
        {
        // ===== Control flow =====
        case LM_IF_EGAL: case LM_IF_DIFFERENT: case LM_IF_SUP_EGAL:
        case LM_IF_SUP: case LM_IF_INF_EGAL: case LM_IF_INF:
        {
            std::string lhs = decodeEvalVarC(ptr);
            std::string rhs = decodeEvalVarC(ptr);
            s16 jumpOffset = *(s16*)ptr; ptr += 2;
            int target = (int)(ptr - basePtr) + jumpOffset * 2;

            const char* op = "!="; // inverted for goto-else
            if (opcodeLocated == LM_IF_EGAL)       op = "!=";
            else if (opcodeLocated == LM_IF_DIFFERENT) op = "==";
            else if (opcodeLocated == LM_IF_SUP_EGAL) op = "<";
            else if (opcodeLocated == LM_IF_SUP)    op = "<=";
            else if (opcodeLocated == LM_IF_INF_EGAL) op = ">";
            else if (opcodeLocated == LM_IF_INF)    op = ">=";

            genPrintf("    if (%s %s %s) goto L_%04X;\n", lhs.c_str(), op, rhs.c_str(), target);
            break;
        }
        case LM_GOTO:
        {
            s16 jumpOffset = *(s16*)ptr; ptr += 2;
            int target = (int)(ptr - basePtr) + jumpOffset * 2;
            genPrintf("    goto L_%04X;\n", target);
            break;
        }
        case LM_SWITCH:
        {
            std::string val = decodeEvalVarC(ptr);
            genPrintf("    switchVal = %s;\n", val.c_str());
            break;
        }
        case LM_CASE:
        {
            s16 caseVal = *(s16*)ptr; ptr += 2;
            s16 jumpOffset = *(s16*)ptr; ptr += 2;
            int target = (int)(ptr - basePtr) + jumpOffset * 2;
            genPrintf("    if (switchVal != %d) goto L_%04X;\n", caseVal, target);
            break;
        }
        case LM_MULTI_CASE:
        {
            s16 numCases = *(s16*)ptr; ptr += 2;
            genPrintf("    if (");
            for (int i = 0; i < numCases; i++)
            {
                s16 caseVal = *(s16*)ptr; ptr += 2;
                if (i > 0) genPrintf(" && ");
                genPrintf("switchVal != %d", caseVal);
            }
            s16 jumpOffset = *(s16*)ptr; ptr += 2;
            int target = (int)(ptr - basePtr) + jumpOffset * 2;
            genPrintf(") goto L_%04X;\n", target);
            break;
        }
        case LM_RETURN:
            genPrintf("    return;\n");
            break;
        case LM_END:
            genPrintf("    return;\n");
            break;

        // ===== Body / Animation =====
        case LM_BODY:
        {
            std::string val = decodeEvalVarC(ptr);
            if (hasTarget)
            {
                genPrintf("    LIFE_TARGET_BEGIN(%s)\n", tgt.c_str());
                genPrintf("        life_Body(%s);\n", val.c_str());
                genPrintf("    LIFE_TARGET_ELSE\n");
                genPrintf("        life_WorldObj_Body(%s, %s);\n", tgt.c_str(), val.c_str());
                genPrintf("    LIFE_TARGET_END\n");
            }
            else
                genPrintf("    life_Body(%s);\n", val.c_str());
            break;
        }
        case LM_BODY_RESET:
        {
            std::string body = decodeEvalVarC(ptr);
            std::string anim = decodeEvalVarC(ptr);
            if (hasTarget)
            {
                genPrintf("    LIFE_TARGET_BEGIN(%s)\n", tgt.c_str());
                genPrintf("        life_BodyReset(%s, %s);\n", body.c_str(), anim.c_str());
                genPrintf("    LIFE_TARGET_ELSE\n");
                genPrintf("        life_WorldObj_BodyReset(%s, %s, %s);\n", tgt.c_str(), body.c_str(), anim.c_str());
                genPrintf("    LIFE_TARGET_END\n");
            }
            else
                genPrintf("    life_BodyReset(%s, %s);\n", body.c_str(), anim.c_str());
            break;
        }
        case LM_ANIM_ONCE:
        {
            std::string anim = readArgStr(ptr);
            std::string flags = readArgStr(ptr);
            if (hasTarget)
            {
                genPrintf("    LIFE_TARGET_BEGIN(%s)\n", tgt.c_str());
                genPrintf("        life_AnimOnce(%s, %s);\n", anim.c_str(), flags.c_str());
                genPrintf("    LIFE_TARGET_ELSE\n");
                genPrintf("        life_WorldObj_AnimOnce(%s, %s, %s);\n", tgt.c_str(), anim.c_str(), flags.c_str());
                genPrintf("    LIFE_TARGET_END\n");
            }
            else
                genPrintf("    life_AnimOnce(%s, %s);\n", anim.c_str(), flags.c_str());
            break;
        }
        case LM_ANIM_REPEAT:
        {
            std::string anim = readArgStr(ptr);
            if (hasTarget)
            {
                genPrintf("    LIFE_TARGET_BEGIN(%s)\n", tgt.c_str());
                genPrintf("        life_AnimRepeat(%s);\n", anim.c_str());
                genPrintf("    LIFE_TARGET_ELSE\n");
                genPrintf("        life_WorldObj_AnimRepeat(%s, %s);\n", tgt.c_str(), anim.c_str());
                genPrintf("    LIFE_TARGET_END\n");
            }
            else
                genPrintf("    life_AnimRepeat(%s);\n", anim.c_str());
            break;
        }
        case LM_ANIM_ALL_ONCE:
        {
            std::string anim = readArgStr(ptr);
            std::string flags = readArgStr(ptr);
            if (hasTarget)
            {
                genPrintf("    LIFE_TARGET_BEGIN(%s)\n", tgt.c_str());
                genPrintf("        life_AnimAllOnce(%s, %s);\n", anim.c_str(), flags.c_str());
                genPrintf("    LIFE_TARGET_ELSE\n");
                genPrintf("        life_WorldObj_AnimAllOnce(%s, %s, %s);\n", tgt.c_str(), anim.c_str(), flags.c_str());
                genPrintf("    LIFE_TARGET_END\n");
            }
            else
                genPrintf("    life_AnimAllOnce(%s, %s);\n", anim.c_str(), flags.c_str());
            break;
        }
        case LM_ANIM_RESET:
        {
            std::string anim = readArgStr(ptr);
            std::string flags = readArgStr(ptr);
            if (hasTarget)
            {
                genPrintf("    LIFE_TARGET_BEGIN(%s)\n", tgt.c_str());
                genPrintf("        life_AnimReset(%s, %s);\n", anim.c_str(), flags.c_str());
                genPrintf("    LIFE_TARGET_ELSE\n");
                genPrintf("        // AnimReset not supported when not loaded\n");
                genPrintf("    LIFE_TARGET_END\n");
            }
            else
                genPrintf("    life_AnimReset(%s, %s);\n", anim.c_str(), flags.c_str());
            break;
        }
        case LM_ANIM_MOVE:
        {
            std::string stand = readArgStr(ptr);
            std::string walk = readArgStr(ptr);
            std::string run = readArgStr(ptr);
            std::string stop = readArgStr(ptr);
            std::string back = readArgStr(ptr);
            std::string turnR = readArgStr(ptr);
            std::string turnL = readArgStr(ptr);
            genPrintf("    life_AnimMove(%s, %s, %s, %s, %s, %s, %s);\n",
                stand.c_str(), walk.c_str(), run.c_str(), stop.c_str(), back.c_str(), turnR.c_str(), turnL.c_str());
            break;
        }
        case LM_ANIM_SAMPLE:
        {
            std::string sample = decodeEvalVarC(ptr);
            std::string anim = readArgStr(ptr);
            std::string frame = readArgStr(ptr);
            genPrintf("    life_AnimSample(%s, %s, %s);\n", sample.c_str(), anim.c_str(), frame.c_str());
            break;
        }
        case LM_ANIM_HYBRIDE_ONCE:
        {
            std::string anim = readArgStr(ptr);
            std::string body = readArgStr(ptr);
            genPrintf("    life_AnimHybrideOnce(%s, %s);\n", anim.c_str(), body.c_str());
            break;
        }
        case LM_ANIM_HYBRIDE_REPEAT:
        {
            std::string anim = readArgStr(ptr);
            std::string body = readArgStr(ptr);
            genPrintf("    life_AnimHybrideRepeat(%s, %s);\n", anim.c_str(), body.c_str());
            break;
        }
        case LM_2D_ANIM_SAMPLE:
        {
            std::string sample = decodeEvalVarC(ptr);
            std::string anim = readArgStr(ptr);
            std::string frame = readArgStr(ptr);
            genPrintf("    life_2dAnimSample(%s, %s, %s);\n", sample.c_str(), anim.c_str(), frame.c_str());
            break;
        }

        // ===== Movement =====
        case LM_MOVE:
        {
            std::string trackMode = readArgStr(ptr);
            std::string trackNum = readArgStr(ptr);
            if (hasTarget)
            {
                genPrintf("    LIFE_TARGET_BEGIN(%s)\n", tgt.c_str());
                genPrintf("        life_Move(%s, %s);\n", trackMode.c_str(), trackNum.c_str());
                genPrintf("    LIFE_TARGET_ELSE\n");
                genPrintf("        life_WorldObj_Move(%s, %s, %s);\n", tgt.c_str(), trackMode.c_str(), trackNum.c_str());
                genPrintf("    LIFE_TARGET_END\n");
            }
            else
                genPrintf("    life_Move(%s, %s);\n", trackMode.c_str(), trackNum.c_str());
            break;
        }
        case LM_DO_MOVE:
            genPrintf("    life_DoMove();\n");
            break;
        case LM_MANUAL_ROT:
            genPrintf("    life_ManualRot();\n");
            break;
        case LM_CONTINUE_TRACK:
            genPrintf("    life_ContinueTrack();\n");
            break;
        case LM_RESET_MOVE_MANUAL:
            genPrintf("    life_ResetMoveManual();\n");
            break;
        case LM_SET_BETA:
        {
            std::string beta = readArgStr(ptr);
            std::string speed = readArgStr(ptr);
            genPrintf("    life_SetBeta(%s, %s);\n", beta.c_str(), speed.c_str());
            break;
        }
        case LM_SET_ALPHA:
        {
            std::string alpha = readArgStr(ptr);
            std::string speed = readArgStr(ptr);
            genPrintf("    life_SetAlpha(%s, %s);\n", alpha.c_str(), speed.c_str());
            break;
        }
        case LM_STOP_BETA:
            genPrintf("    life_StopBeta();\n");
            break;
        case LM_ANGLE:
        {
            std::string a = readArgStr(ptr);
            std::string b = readArgStr(ptr);
            std::string g = readArgStr(ptr);
            if (hasTarget)
            {
                genPrintf("    LIFE_TARGET_BEGIN(%s)\n", tgt.c_str());
                genPrintf("        life_Angle(%s, %s, %s);\n", a.c_str(), b.c_str(), g.c_str());
                genPrintf("    LIFE_TARGET_ELSE\n");
                genPrintf("        life_WorldObj_Angle(%s, %s, %s, %s);\n", tgt.c_str(), a.c_str(), b.c_str(), g.c_str());
                genPrintf("    LIFE_TARGET_END\n");
            }
            else
                genPrintf("    life_Angle(%s, %s, %s);\n", a.c_str(), b.c_str(), g.c_str());
            break;
        }
        case LM_COPY_ANGLE:
        {
            std::string obj = readArgStr(ptr);
            genPrintf("    life_CopyAngle(%s);\n", obj.c_str());
            break;
        }
        case LM_SPEED:
        {
            std::string val = decodeEvalVarC(ptr);
            genPrintf("    life_Speed(%s);\n", val.c_str());
            break;
        }

        // ===== Stage / Position =====
        case LM_STAGE:
        {
            std::string stage = readArgStr(ptr);
            std::string room = readArgStr(ptr);
            std::string x = readArgStr(ptr);
            std::string y = readArgStr(ptr);
            std::string z = readArgStr(ptr);
            if (hasTarget)
            {
                genPrintf("    LIFE_TARGET_BEGIN(%s)\n", tgt.c_str());
                genPrintf("        life_Stage(%s, %s, %s, %s, %s);\n", stage.c_str(), room.c_str(), x.c_str(), y.c_str(), z.c_str());
                genPrintf("    LIFE_TARGET_ELSE\n");
                genPrintf("        life_WorldObj_Stage(%s, %s, %s, %s, %s, %s);\n", tgt.c_str(), stage.c_str(), room.c_str(), x.c_str(), y.c_str(), z.c_str());
                genPrintf("    LIFE_TARGET_END\n");
            }
            else
                genPrintf("    life_Stage(%s, %s, %s, %s, %s);\n", stage.c_str(), room.c_str(), x.c_str(), y.c_str(), z.c_str());
            break;
        }
        case LM_STAGE_LIFE:
        {
            std::string val = readArgStr(ptr);
            genPrintf("    life_StageLife(%s);\n", val.c_str());
            break;
        }
        case LM_PUT:
        {
            std::string idx = readArgStr(ptr);
            std::string x = readArgStr(ptr);
            std::string y = readArgStr(ptr);
            std::string z = readArgStr(ptr);
            std::string room = readArgStr(ptr);
            std::string stage = readArgStr(ptr);
            std::string a = readArgStr(ptr);
            std::string b = readArgStr(ptr);
            std::string g = readArgStr(ptr);
            genPrintf("    life_Put(%s, %s, %s, %s, %s, %s, %s, %s, %s);\n",
                idx.c_str(), x.c_str(), y.c_str(), z.c_str(), room.c_str(), stage.c_str(), a.c_str(), b.c_str(), g.c_str());
            break;
        }
        case LM_PUT_AT:
        {
            std::string obj1 = readArgStr(ptr);
            std::string obj2 = readArgStr(ptr);
            genPrintf("    life_PutAt(%s, %s);\n", obj1.c_str(), obj2.c_str());
            break;
        }
        case LM_UP_COOR_Y:
            genPrintf("    life_UpCoorY();\n");
            break;

        // ===== Actor properties =====
        case LM_TEST_COL:
        {
            std::string val = readArgStr(ptr);
            if (hasTarget)
            {
                genPrintf("    LIFE_TARGET_BEGIN(%s)\n", tgt.c_str());
                genPrintf("        life_TestCol(%s);\n", val.c_str());
                genPrintf("    LIFE_TARGET_ELSE\n");
                genPrintf("        life_WorldObj_TestCol(%s, %s);\n", tgt.c_str(), val.c_str());
                genPrintf("    LIFE_TARGET_END\n");
            }
            else
                genPrintf("    life_TestCol(%s);\n", val.c_str());
            break;
        }
        case LM_TYPE:
        {
            s16 type = *(s16*)ptr; ptr += 2;
            std::string val = std::to_string(type & 0x1FF);
            if (hasTarget)
            {
                genPrintf("    LIFE_TARGET_BEGIN(%s)\n", tgt.c_str());
                genPrintf("        life_Type(%s);\n", val.c_str());
                genPrintf("    LIFE_TARGET_ELSE\n");
                genPrintf("        life_WorldObj_Type(%s, %s);\n", tgt.c_str(), val.c_str());
                genPrintf("    LIFE_TARGET_END\n");
            }
            else
                genPrintf("    life_Type(%s);\n", val.c_str());
            break;
        }
        case LM_LIFE:
        {
            std::string life = readArgStr(ptr);
            if (hasTarget)
            {
                genPrintf("    LIFE_TARGET_BEGIN(%s)\n", tgt.c_str());
                genPrintf("        life_SetLife(%s);\n", life.c_str());
                genPrintf("    LIFE_TARGET_ELSE\n");
                genPrintf("        life_WorldObj_Life(%s, %s);\n", tgt.c_str(), life.c_str());
                genPrintf("    LIFE_TARGET_END\n");
            }
            else
                genPrintf("    life_SetLife(%s);\n", life.c_str());
            break;
        }
        case LM_LIFE_MODE:
        {
            std::string mode = readArgStr(ptr);
            if (hasTarget)
            {
                genPrintf("    LIFE_TARGET_BEGIN(%s)\n", tgt.c_str());
                genPrintf("        life_LifeMode(%s);\n", mode.c_str());
                genPrintf("    LIFE_TARGET_ELSE\n");
                genPrintf("        life_WorldObj_LifeMode(%s, %s);\n", tgt.c_str(), mode.c_str());
                genPrintf("    LIFE_TARGET_END\n");
            }
            else
                genPrintf("    life_LifeMode(%s);\n", mode.c_str());
            break;
        }
        case LM_DELETE:
        {
            std::string obj = readArgStr(ptr);
            genPrintf("    life_Delete(%s);\n", obj.c_str());
            break;
        }
        case LM_SPECIAL:
        {
            std::string type = readArgStr(ptr);
            genPrintf("    life_Special(%s);\n", type.c_str());
            break;
        }
        case LM_START_CHRONO:
            genPrintf("    life_StartChrono();\n");
            break;
        case LM_CAMERA:
            genPrintf("    life_Camera();\n");
            break;
        case LM_CAMERA_TARGET:
        {
            std::string target = readArgStr(ptr);
            genPrintf("    life_CameraTarget(%s);\n", target.c_str());
            break;
        }

        // ===== Combat =====
        case LM_HIT:
        {
            std::string anim = readArgStr(ptr);
            std::string startFrame = readArgStr(ptr);
            std::string group = readArgStr(ptr);
            std::string hitBox = readArgStr(ptr);
            std::string force = decodeEvalVarC(ptr);
            std::string nextAnim = readArgStr(ptr);
            genPrintf("    life_Hit(%s, %s, %s, %s, %s, %s);\n",
                anim.c_str(), startFrame.c_str(), group.c_str(), hitBox.c_str(), force.c_str(), nextAnim.c_str());
            break;
        }
        case LM_FIRE:
        {
            std::string fireAnim = readArgStr(ptr);
            std::string shootFrame = readArgStr(ptr);
            std::string emitPoint = readArgStr(ptr);
            std::string zvSize = readArgStr(ptr);
            std::string hitForce = readArgStr(ptr);
            std::string nextAnim = readArgStr(ptr);
            genPrintf("    life_Fire(%s, %s, %s, %s, %s, %s);\n",
                fireAnim.c_str(), shootFrame.c_str(), emitPoint.c_str(), zvSize.c_str(), hitForce.c_str(), nextAnim.c_str());
            break;
        }
        case LM_FIRE_UP_DOWN:
        {
            std::string args[6];
            for (int i = 0; i < 6; i++) args[i] = readArgStr(ptr);
            genPrintf("    life_FireUpDown(%s, %s, %s, %s, %s, %s);\n",
                args[0].c_str(), args[1].c_str(), args[2].c_str(), args[3].c_str(), args[4].c_str(), args[5].c_str());
            break;
        }
        case LM_HIT_OBJECT:
        {
            std::string flags = readArgStr(ptr);
            std::string force = readArgStr(ptr);
            genPrintf("    life_HitObject(%s, %s);\n", flags.c_str(), force.c_str());
            break;
        }
        case LM_STOP_HIT_OBJECT:
            genPrintf("    life_StopHitObject();\n");
            break;
        case LM_THROW:
        {
            std::string args[7];
            for (int i = 0; i < 7; i++) args[i] = readArgStr(ptr);
            genPrintf("    life_Throw(%s, %s, %s, %s, %s, %s, %s);\n",
                args[0].c_str(), args[1].c_str(), args[2].c_str(), args[3].c_str(),
                args[4].c_str(), args[5].c_str(), args[6].c_str());
            break;
        }

        // ===== Inventory / Found =====
        case LM_FOUND:
        {
            std::string obj = readArgStr(ptr);
            genPrintf("    life_Found(%s);\n", obj.c_str());
            break;
        }
        case LM_FOUND_NAME:
        {
            std::string name = readArgStr(ptr);
            if (hasTarget)
            {
                genPrintf("    LIFE_TARGET_BEGIN(%s)\n", tgt.c_str());
                genPrintf("        life_FoundName(%s);\n", name.c_str());
                genPrintf("    LIFE_TARGET_ELSE\n");
                genPrintf("        life_WorldObj_FoundName(%s, %s);\n", tgt.c_str(), name.c_str());
                genPrintf("    LIFE_TARGET_END\n");
            }
            else
                genPrintf("    life_FoundName(%s);\n", name.c_str());
            break;
        }
        case LM_FOUND_BODY:
        {
            std::string body = readArgStr(ptr);
            if (hasTarget)
            {
                genPrintf("    LIFE_TARGET_BEGIN(%s)\n", tgt.c_str());
                genPrintf("        life_FoundBody(%s);\n", body.c_str());
                genPrintf("    LIFE_TARGET_ELSE\n");
                genPrintf("        life_WorldObj_FoundBody(%s, %s);\n", tgt.c_str(), body.c_str());
                genPrintf("    LIFE_TARGET_END\n");
            }
            else
                genPrintf("    life_FoundBody(%s);\n", body.c_str());
            break;
        }
        case LM_FOUND_FLAG:
        {
            std::string flag = readArgStr(ptr);
            if (hasTarget)
            {
                genPrintf("    LIFE_TARGET_BEGIN(%s)\n", tgt.c_str());
                genPrintf("        life_FoundFlag(%s);\n", flag.c_str());
                genPrintf("    LIFE_TARGET_ELSE\n");
                genPrintf("        life_WorldObj_FoundFlag(%s, %s);\n", tgt.c_str(), flag.c_str());
                genPrintf("    LIFE_TARGET_END\n");
            }
            else
                genPrintf("    life_FoundFlag(%s);\n", flag.c_str());
            break;
        }
        case LM_FOUND_WEIGHT:
        {
            std::string weight = readArgStr(ptr);
            if (hasTarget)
            {
                genPrintf("    LIFE_TARGET_BEGIN(%s)\n", tgt.c_str());
                genPrintf("        life_FoundWeight(%s);\n", weight.c_str());
                genPrintf("    LIFE_TARGET_ELSE\n");
                genPrintf("        life_WorldObj_FoundWeight(%s, %s);\n", tgt.c_str(), weight.c_str());
                genPrintf("    LIFE_TARGET_END\n");
            }
            else
                genPrintf("    life_FoundWeight(%s);\n", weight.c_str());
            break;
        }
        case LM_FOUND_LIFE:
        {
            std::string life = readArgStr(ptr);
            if (hasTarget)
            {
                genPrintf("    LIFE_TARGET_BEGIN(%s)\n", tgt.c_str());
                genPrintf("        life_FoundLife(%s);\n", life.c_str());
                genPrintf("    LIFE_TARGET_ELSE\n");
                genPrintf("        life_WorldObj_FoundLife(%s, %s);\n", tgt.c_str(), life.c_str());
                genPrintf("    LIFE_TARGET_END\n");
            }
            else
                genPrintf("    life_FoundLife(%s);\n", life.c_str());
            break;
        }
        case LM_TAKE:
        {
            std::string obj = readArgStr(ptr);
            genPrintf("    life_Take(%s);\n", obj.c_str());
            break;
        }
        case LM_IN_HAND:
        {
            std::string obj = readArgStr(ptr);
            genPrintf("    life_InHand(%s);\n", obj.c_str());
            break;
        }
        case LM_DROP:
        {
            std::string val = decodeEvalVarC(ptr);
            std::string arg2 = readArgStr(ptr);
            genPrintf("    life_Drop(%s, %s);\n", val.c_str(), arg2.c_str());
            break;
        }
        case LM_INVENTORY:
        {
            std::string val = readArgStr(ptr);
            genPrintf("    life_Inventory(%s);\n", val.c_str());
            break;
        }
        case LM_CALL_INVENTORY:
            genPrintf("    life_CallInventory();\n");
            break;
        case LM_SET_INVENTORY:
        {
            std::string val = readArgStr(ptr);
            genPrintf("    life_SetInventory(%s);\n", val.c_str());
            break;
        }
        case LM_DEL_INVENTORY:
        {
            std::string val = readArgStr(ptr);
            genPrintf("    life_DelInventory(%s);\n", val.c_str());
            break;
        }

        // ===== Variables =====
        case LM_VAR:
        {
            std::string idx = readArgStr(ptr);
            std::string val = decodeEvalVarC(ptr);
            genPrintf("    vars[%s] = %s;\n", idx.c_str(), val.c_str());
            break;
        }
        case LM_INC:
        {
            std::string idx = readArgStr(ptr);
            genPrintf("    vars[%s]++;\n", idx.c_str());
            break;
        }
        case LM_DEC:
        {
            std::string idx = readArgStr(ptr);
            genPrintf("    vars[%s]--;\n", idx.c_str());
            break;
        }
        case LM_ADD:
        {
            std::string idx = readArgStr(ptr);
            std::string val = decodeEvalVarC(ptr);
            genPrintf("    vars[%s] += %s;\n", idx.c_str(), val.c_str());
            break;
        }
        case LM_SUB:
        {
            std::string idx = readArgStr(ptr);
            std::string val = decodeEvalVarC(ptr);
            genPrintf("    vars[%s] -= %s;\n", idx.c_str(), val.c_str());
            break;
        }
        case LM_C_VAR:
        case LM_MODIF_C_VAR:
        {
            std::string idx = readArgStr(ptr);
            std::string val = decodeEvalVarC(ptr);
            genPrintf("    CVars[%s] = %s;\n", idx.c_str(), val.c_str());
            break;
        }

        // ===== Messages =====
        case LM_MESSAGE:
        {
            std::string msg = readArgStr(ptr);
            genPrintf("    life_Message(%s);\n", msg.c_str());
            break;
        }
        case LM_MESSAGE_VALUE:
        {
            std::string msg = readArgStr(ptr);
            std::string val = readArgStr(ptr);
            genPrintf("    life_MessageValue(%s, %s);\n", msg.c_str(), val.c_str());
            break;
        }

        // ===== Sound =====
        case LM_SAMPLE:
        {
            std::string sample = decodeEvalVarC(ptr);
            genPrintf("    life_Sample(%s);\n", sample.c_str());
            break;
        }
        case LM_REP_SAMPLE:
        {
            std::string sample = decodeEvalVarC(ptr);
            std::string freq = readArgStr(ptr);
            genPrintf("    life_RepSample(%s);\n", sample.c_str());
            break;
        }
        case LM_STOP_SAMPLE:
            genPrintf("    life_StopSample();\n");
            break;
        case LM_SAMPLE_THEN:
        {
            std::string s1 = decodeEvalVarC(ptr);
            std::string s2 = decodeEvalVarC(ptr);
            genPrintf("    life_SampleThen(%s, %s);\n", s1.c_str(), s2.c_str());
            break;
        }
        case LM_SAMPLE_THEN_REPEAT:
        {
            std::string s1 = decodeEvalVarC(ptr);
            std::string s2 = decodeEvalVarC(ptr);
            genPrintf("    life_SampleThenRepeat(%s, %s);\n", s1.c_str(), s2.c_str());
            break;
        }

        // ===== Music =====
        case LM_MUSIC:
        {
            std::string idx = readArgStr(ptr);
            genPrintf("    life_Music(%s);\n", idx.c_str());
            break;
        }
        case LM_NEXT_MUSIC:
        {
            std::string idx = readArgStr(ptr);
            genPrintf("    life_NextMusic(%s);\n", idx.c_str());
            break;
        }
        case LM_FADE_MUSIC:
        {
            std::string idx = readArgStr(ptr);
            genPrintf("    life_FadeMusic(%s);\n", idx.c_str());
            break;
        }
        case LM_RND_FREQ:
        {
            std::string val = readArgStr(ptr);
            genPrintf("    life_RndFreq(%s);\n", val.c_str());
            break;
        }

        // ===== ZV (bounding volume) =====
        case LM_DO_REAL_ZV:
            genPrintf("    life_DoRealZv();\n");
            break;
        case LM_DO_ROT_ZV:
            genPrintf("    life_DoRotZv();\n");
            break;
        case LM_DO_MAX_ZV:
            genPrintf("    life_DoMaxZv();\n");
            break;
        case LM_DO_NORMAL_ZV:
            genPrintf("    life_DoNormalZv();\n");
            break;
        case LM_DO_CARRE_ZV:
            genPrintf("    life_DoCarreZv();\n");
            break;
        case LM_DEF_ZV:
        {
            std::string args[6];
            for (int i = 0; i < 6; i++) args[i] = readArgStr(ptr);
            genPrintf("    life_DefZv(%s, %s, %s, %s, %s, %s);\n",
                args[0].c_str(), args[1].c_str(), args[2].c_str(), args[3].c_str(), args[4].c_str(), args[5].c_str());
            break;
        }
        case LM_DEF_ABS_ZV:
        {
            std::string args[6];
            for (int i = 0; i < 6; i++) args[i] = readArgStr(ptr);
            genPrintf("    life_DefAbsZv(%s, %s, %s, %s, %s, %s);\n",
                args[0].c_str(), args[1].c_str(), args[2].c_str(), args[3].c_str(), args[4].c_str(), args[5].c_str());
            break;
        }
        case LM_GET_HARD_CLIP:
            genPrintf("    life_GetHardClip();\n");
            break;

        // ===== Environment / effects =====
        case LM_LIGHT:
        {
            std::string val = readArgStr(ptr);
            genPrintf("    life_Light(%s);\n", val.c_str());
            break;
        }
        case LM_SHAKING:
        {
            std::string amp = readArgStr(ptr);
            genPrintf("    life_Shaking(%s);\n", amp.c_str());
            break;
        }
        case LM_WATER:
        {
            std::string val = readArgStr(ptr);
            genPrintf("    life_Water(%s);\n", val.c_str());
            break;
        }
        case LM_PLUIE:
        {
            std::string val = readArgStr(ptr);
            genPrintf("    life_Pluie(%s);\n", val.c_str());
            break;
        }
        case LM_SET_GROUND:
        {
            std::string val = readArgStr(ptr);
            genPrintf("    life_SetGround(%s);\n", val.c_str());
            break;
        }
        case LM_PICTURE:
        {
            std::string pic = readArgStr(ptr);
            std::string delay = readArgStr(ptr);
            std::string sample = readArgStr(ptr);
            genPrintf("    life_Picture(%s, %s, %s);\n", pic.c_str(), delay.c_str(), sample.c_str());
            break;
        }
        case LM_READ:
        {
            std::string type = readArgStr(ptr);
            std::string index = readArgStr(ptr);
            std::string vocIndex = (g_gameId == AITD1) ? readArgStr(ptr) : std::string("-1");
            genPrintf("    life_Read(%s, %s, %s);\n", type.c_str(), index.c_str(), vocIndex.c_str());
            break;
        }
        case LM_READ_ON_PICTURE:
        {
            std::string args[8];
            for (int i = 0; i < 8; i++) args[i] = readArgStr(ptr);
            std::string vocIdx = (g_gameId == AITD1) ? readArgStr(ptr) : std::string("-1");
            genPrintf("    life_ReadOnPicture(%s, %s, %s, %s, %s, %s, %s, %s, %s);\n",
                args[0].c_str(), args[1].c_str(), args[2].c_str(), args[3].c_str(),
                args[4].c_str(), args[5].c_str(), args[6].c_str(), args[7].c_str(), vocIdx.c_str());
            break;
        }

        // ===== Sequences =====
        case LM_PLAY_SEQUENCE:
        {
            std::string seq = readArgStr(ptr);
            std::string fadeIn = readArgStr(ptr);
            std::string fadeOut = readArgStr(ptr);
            genPrintf("    life_PlaySequence(%s, %s, %s);\n", seq.c_str(), fadeIn.c_str(), fadeOut.c_str());
            break;
        }
        case LM_DEF_SEQUENCE_SAMPLE:
        {
            s16 numParams = *(s16*)ptr; ptr += 2;
            genPrintf("    { int _seqPairs[] = {");
            for (int i = 0; i < numParams; i++)
            {
                s16 frame = *(s16*)ptr; ptr += 2;
                s16 sample = *(s16*)ptr; ptr += 2;
                if (i > 0) genPrintf(",");
                genPrintf(" %d, %d", frame, sample);
            }
            genPrintf(" };\n");
            genPrintf("    life_DefSequenceSample(%d, _seqPairs); }\n", numParams);
            break;
        }
        case LM_END_SEQUENCE:
            genPrintf("    life_EndSequence();\n");
            break;

        // ===== Game state =====
        case LM_GAME_OVER:
            genPrintf("    life_GameOver();\n");
            break;
        case LM_WAIT_GAME_OVER:
            genPrintf("    life_WaitGameOver();\n");
            break;
        case LM_PROTECT:
            genPrintf("    life_Protect();\n");
            break;
        case LM_GET_MATRICE:
        {
            std::string val = readArgStr(ptr);
            genPrintf("    life_GetMatrice(%s);\n", val.c_str());
            break;
        }

        default:
        {
            const char* macroName = getLifeMacroNameForDump(opcodeLocated);
            genPrintf("    /* UNHANDLED: %s (opcode=0x%02X) */\n", macroName, opcodeIdx);
            break;
        }
        }

        instructionNum++;
        if (instructionNum > 10000) break;
    }

    // Emit any trailing label
    {
        int endOffset = (int)(ptr - basePtr);
        if (jumpTargets.count(endOffset))
            genPrintf("L_%04X:\n", endOffset);
    }

    genPrintf("}\n\n");
}

void generateAllNativeLifeScripts()
{
    s_genFile = fopen("nativeLifeScripts_generated.cpp", "w");
    if (!s_genFile) {
        printf(LIFE_WARN "Could not open nativeLifeScripts_generated.cpp" CON_RESET "\n");
        return;
    }

    genPrintf("///////////////////////////////////////////////////////////////////////////////\n");
    genPrintf("// Auto-generated native life scripts (label-based control flow)\n");
    genPrintf("// Generated by AITD Re-Haunted native life script system\n");
    genPrintf("// DO NOT EDIT - this file is regenerated from LISTLIFE bytecode\n");
    genPrintf("///////////////////////////////////////////////////////////////////////////////\n\n");
    genPrintf("#include \"common.h\"\n");
    genPrintf("#include \"nativeLife.h\"\n");
    genPrintf("#include \"nativeLifeHelpers.h\"\n\n");

    // Track which scripts we generate for the registration function
    std::vector<int> generatedScripts;
    int scriptCount = 0;

    for (int i = 0; i < 1000; i++)
    {
        int size = getPakSize("LISTLIFE", i);
        if (size > 0)
        {
            char* basePtr = HQR_Get(listLife, i);
            if (basePtr)
            {
                generateNativeLifeScriptC(i);
                generatedScripts.push_back(i);
                scriptCount++;
                genFlush();
            }
        }
    }

    // Generate registration function
    genPrintf("void registerGeneratedNativeLifeScripts()\n{\n");
    for (int lifeNum : generatedScripts)
    {
        genPrintf("    registerNativeLifeScript(%d, nativeLife_%d);\n", lifeNum, lifeNum);
    }
    genPrintf("}\n");

    genFlush();
    fclose(s_genFile);
    s_genFile = nullptr;
    printf(LIFE_OK "Generated %d native life scripts to nativeLifeScripts_generated.cpp" CON_RESET "\n", scriptCount);
}
