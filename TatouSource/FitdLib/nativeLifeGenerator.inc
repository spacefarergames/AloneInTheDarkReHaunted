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

// Collect all jump target offsets for label generation
static std::set<int> collectJumpTargets(const char* basePtr, int scriptSize) {
    std::set<int> targets;
    const char* ptr = basePtr;
    const char* endPtr = basePtr + scriptSize;
    while (ptr < endPtr) {
        int offset = (int)(ptr - basePtr);
        s16 rawOpcode = *(s16*)ptr; ptr += 2;
        if (rawOpcode == 0 && ptr >= endPtr) break;
        if (rawOpcode & 0x8000) ptr += 2;
        int opcodeIdx = rawOpcode & 0x7FFF;
        int op = (g_gameId == AITD1) ? AITD1LifeMacroTable[opcodeIdx] : AITD2LifeMacroTable[opcodeIdx];
        switch(op) {
        case LM_IF_EGAL: case LM_IF_DIFFERENT: case LM_IF_SUP_EGAL:
        case LM_IF_SUP: case LM_IF_INF_EGAL: case LM_IF_INF:
            skipEvalVar(ptr); skipEvalVar(ptr);
            { s16 j=*(s16*)ptr; ptr+=2; int t=(int)(ptr-basePtr)+j; if(t>=0&&t<scriptSize) targets.insert(t); }
            break;
        case LM_GOTO:
            { s16 j=*(s16*)ptr; ptr+=2; int t=(int)(ptr-basePtr)+j; if(t>=0&&t<scriptSize) targets.insert(t); }
            break;
        case LM_CASE:
            ptr+=2; { s16 j=*(s16*)ptr; ptr+=2; int t=(int)(ptr-basePtr)+j; if(t>=0&&t<scriptSize) targets.insert(t); }
            break;
        case LM_MULTI_CASE:
            { s16 n=*(s16*)ptr; ptr+=2; ptr+=n*2; s16 j=*(s16*)ptr; ptr+=2; int t=(int)(ptr-basePtr)+j; if(t>=0&&t<scriptSize) targets.insert(t); }
            break;
        case LM_SWITCH: skipEvalVar(ptr); break;
        case LM_BODY: case LM_DO_MOVE: case LM_ANIM_ONCE: case LM_ANIM_ALL_ONCE: case LM_ANIM_REPEAT:
        case LM_DO_ROT_ZV: case LM_CAMERA_TARGET: case LM_TAKE: case LM_IN_HAND: case LM_DELETE: case LM_SPECIAL:
        case LM_MUSIC: case LM_FOUND: case LM_SET_GROUND: case LM_DEF_ZV: case LM_FOUND_NAME: case LM_FOUND_FLAG:
        case LM_SAMPLE: case LM_REP_SAMPLE: case LM_FOUND_WEIGHT: case LM_FOUND_LIFE: case LM_SET_INVENTORY:
        case LM_DEL_INVENTORY: case LM_PUT_AT: case LM_COPY_ANGLE: case LM_LIFE: case LM_PLUIE:
            skipEvalVar(ptr); break;
        case LM_MESSAGE: case LM_MESSAGE_VALUE: case LM_DO_MAX_ZV: case LM_TYPE:
        case LM_GET_MATRICE: case LM_LIFE_MODE: case LM_ANIM_HYBRIDE_ONCE: case LM_ANIM_HYBRIDE_REPEAT:
        case LM_HIT_OBJECT:
            skipEvalVar(ptr); skipEvalVar(ptr); break;
        case LM_VAR: case LM_INC: case LM_DEC: case LM_ADD: case LM_SUB: case LM_TEST_COL: case LM_ANIM_SAMPLE:
        case LM_2D_ANIM_SAMPLE: case LM_PLAY_SEQUENCE: case LM_PICTURE:
            skipEvalVar(ptr); skipEvalVar(ptr); skipEvalVar(ptr); break;
        case LM_C_VAR: case LM_ANGLE: case LM_MODIF_C_VAR:
            for(int i=0;i<4;i++) skipEvalVar(ptr); break;
        case LM_STAGE: case LM_HIT: case LM_FIRE: case LM_FIRE_UP_DOWN:
            for(int i=0;i<6;i++) skipEvalVar(ptr); break;
        case LM_THROW: case LM_ANIM_MOVE:
            for(int i=0;i<7;i++) skipEvalVar(ptr); break;
        case LM_PUT: case LM_READ_ON_PICTURE:
            for(int i=0;i<9;i++) skipEvalVar(ptr); break;
        case LM_DEF_SEQUENCE_SAMPLE:
            { s16 n=*(s16*)ptr; ptr+=2+n*4; } break;
        default: break;
        }
    }
    return targets;
}

void generateAllNativeLifeScripts()
{
    s_genFile = fopen("nativeLifeScripts_generated.cpp", "w");
    if (!s_genFile) {
        printf(LIFE_WARN "Could not open nativeLifeScripts_generated.cpp" CON_RESET "\n");
        return;
    }
    
    genPrintf("// Auto-generated native life scripts (label-based control flow)\n");
    genPrintf("#include \"common.h\"\n");
    genPrintf("#include \"nativeLife.h\"\n");
    genPrintf("#include \"nativeLifeHelpers.h\"\n\n");
    
    // For now, just generate stub - full implementation requires more work
    genPrintf("void registerGeneratedNativeLifeScripts()\n{\n");
    genPrintf("    // Label-based generator not yet fully implemented\n");
    genPrintf("    // Falling back to bytecode interpreter\n");
    genPrintf("}\n");
    
    genFlush();
    fclose(s_genFile);
    s_genFile = nullptr;
    printf(LIFE_OK "Generated native life scripts stub" CON_RESET "\n");
}
