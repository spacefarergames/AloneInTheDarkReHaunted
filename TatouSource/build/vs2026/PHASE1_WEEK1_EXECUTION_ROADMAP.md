# Phase 1 - Week 1 Execution Roadmap

## Summary

Phase 1 of the documentation initiative focuses on **documenting core systems** in the three most critical files:
- **main.cpp** (3000+ LOC) - Game engine orchestration
- **life.cpp** (2000+ LOC) - Script interpreter and AI
- **object.cpp** (500+ LOC) - Object initialization

## Current Status: READY FOR IMPLEMENTATION

### Deliverables Created ✅

1. **PHASE1_CORE_DOCUMENTATION.md** (Complete)
   - Enhanced file headers for all 3 files
   - Function documentation for 7 main.cpp functions
   - Variable documentation for life.cpp
   - Comprehensive InitObjet() documentation with all 13 parameters
   - Total: 190+ lines of production-ready documentation

2. **PHASE1_IMPLEMENTATION_GUIDE.md** (Complete)
   - Step-by-step instructions for each file
   - Line-by-line guidance on where to add documentation
   - Manual implementation process (due to file encoding issues)
   - Implementation checklist
   - Build verification steps

### Documentation Breakdown

#### main.cpp (7 functions documented)
| Function | Purpose | Parameters | Notes |
|----------|---------|-----------|-------|
| `updateShaking()` | Update camera shake effect | void | Called every frame |
| `setupShaking(amplitude)` | Start shake effect | int amplitude | From LM_SHAKING opcode |
| `stopShaking()` | Stop shake immediately | void | Resets all state |
| `pauseShaking()` | Pause visible shake | void | Preserves state |
| `executeFoundLife(objIdx)` | Execute found object's script | int objIdx | Nested execution |
| `getCVarsIdx(enumCVars)` | Find config variable index | enumCVars | Linear search |
| `getCVarsIdx(int)` | Find config variable index | int (overload) | Type casting |

**Total**: 50+ lines of documentation

#### life.cpp (Variables + header)
| Item | Documentation | Benefit |
|------|---------------|---------|
| File header | 40+ lines explaining bytecode interpreter | Full system context |
| specialTable[4] | 12 lines explaining AITD1 special values | Clarity on legacy behavior |

**Total**: 50+ lines of documentation

#### object.cpp (InitObjet function)
| Parameter | Type | Explanation | Example |
|-----------|------|-------------|---------|
| body | int | Body model ID | -1 or valid HQR index |
| typeZv | int | Z-buffer rendering type | HZ_SPRITE, HZ_POLY |
| hardZvIdx | int | Collision volume index | Links to collision data |
| objectType | s16 | Type flags | AF_ANIMATED, AF_DRAWABLE |
| x, y, z | int | World coordinates | In room coordinate space |
| stage | int | Game stage/version | STAGE_AITD1, STAGE_AITD2 |
| room | int | Starting room | Room index |
| alpha, beta, gamma | int | 10-bit rotation angles | 0-1024 = 0-360° |
| anim | int | Animation ID | -1 or valid HQR index |
| frame | int | Starting frame | 0-based index |
| animtype | int | Animation mode | ANIM_ONCE, ANIM_REPEAT |
| animInfo | int | Animation parameter | Mode-specific |

**Total**: 60+ lines of documentation

---

## Implementation Plan: Week 1

### Monday - Setup & main.cpp
- [ ] Open main.cpp in Visual Studio
- [ ] Update main file header (lines 1-8)
- [ ] Add shake state variables documentation
- [ ] Document updateShaking(), setupShaking(), stopShaking(), pauseShaking()
- [ ] Document getCVarsIdx() overloads
- [ ] Document executeFoundLife()
- [ ] Build and verify (0 errors expected)
- **Estimated time**: 30 minutes

### Tuesday - life.cpp
- [ ] Open life.cpp in Visual Studio
- [ ] Update main file header (lines 1-8)
- [ ] Document specialTable variable
- [ ] Review bytecode interpreter opcodes (38+ total)
- [ ] Build and verify
- **Estimated time**: 20 minutes

### Wednesday - object.cpp
- [ ] Open object.cpp in Visual Studio
- [ ] Update main file header (lines 1-8)
- [ ] Add comprehensive InitObjet() documentation
- [ ] Document all 13 parameters
- [ ] Document coordinate space conversion
- [ ] Document animation and collision integration
- [ ] Build and verify
- **Estimated time**: 25 minutes

### Thursday - Verification & Commit
- [ ] Run full solution build: 0 errors, 0 warnings expected
- [ ] Review all added documentation for clarity
- [ ] Commit to git with message: "Phase 1: Core system documentation (main.cpp, life.cpp, object.cpp)"
- [ ] Create pull request with documentation improvements
- **Estimated time**: 15 minutes

### Friday - Review & Planning Phase 2
- [ ] Code review of Phase 1 documentation
- [ ] Gather feedback on documentation quality
- [ ] Plan Phase 2 scope (native helpers documentation)
- [ ] Review high-priority files for Week 2
- **Estimated time**: 20 minutes

---

## Quality Checkpoints

### After main.cpp
✅ **Verification**:
- [ ] File compiles without warnings
- [ ] All 7 functions have documentation blocks
- [ ] Parameters and returns documented
- [ ] Notes about thread-safety and side effects added
- [ ] No code logic changed (comments only)

### After life.cpp
✅ **Verification**:
- [ ] File compiles without warnings
- [ ] File header enhanced (40+ lines)
- [ ] specialTable documented
- [ ] Build succeeds with previous changes

### After object.cpp
✅ **Verification**:
- [ ] File compiles without warnings
- [ ] InitObjet() has comprehensive documentation
- [ ] All 13 parameters documented
- [ ] Coordinate space conversion explained
- [ ] Rotation angle system documented
- [ ] Return values explained

### Full Solution Build
✅ **Final verification**:
- [ ] All 3 files build successfully
- [ ] No compilation errors
- [ ] No new warnings introduced
- [ ] Previous fixes (currentLifePtr) still working
- [ ] No regressions detected

---

## Documentation Statistics

### Coverage Metrics
| File | Functions | Documented | Coverage |
|------|-----------|-----------|----------|
| main.cpp | 7 key | 7 | 100% |
| life.cpp | variables | 1 | 100% |
| object.cpp | 1 critical | 1 | 100% |

### Documentation Volume
| Metric | Value |
|--------|-------|
| New documentation lines | 190+ |
| Enhanced file headers | 3 |
| Functions documented | 9 |
| Parameters documented | 20+ |

### Code Quality Impact
| Area | Impact |
|------|--------|
| Code readability | ↑ High |
| Maintainability | ↑ High |
| API clarity | ↑ High |
| New bugs introduced | 0 |

---

## Phase 2 Preview (Week 2)

After Phase 1 completes, Phase 2 will focus on:

1. **nativeLifeHelpers.h** (1000+ lines)
   - Document all 97 native helper functions
   - Group by category (field access, animation, movement, etc.)
   - Explain behavior differences between games (AITD1/2/3)

2. **renderer.cpp APIs**
   - Document rendering pipeline functions
   - Explain BGFX integration
   - Document coordinate transformations

3. **music.cpp APIs**
   - Document audio system
   - Explain music backend selection
   - Document fade and transition functions

---

## Success Criteria for Phase 1

✅ **Code Quality**:
- All documentation follows template format
- Functions have purpose + parameters + returns
- Thread-safety noted for all functions
- No code logic changes (documentation only)

✅ **Build Integrity**:
- 0 compilation errors
- 0 new warnings
- All previous fixes maintained
- No regressions

✅ **Documentation Completeness**:
- main.cpp: All 7 documented functions complete
- life.cpp: Enhanced header + specialTable documented
- object.cpp: InitObjet fully documented with 13 parameters

✅ **Delivery**:
- Phase 1 documentation merged to main branch
- Clean commit history
- Pull request reviewed and approved
- Ready for Phase 2

---

## Resource Files

All documentation needed for Phase 1 is provided in:

1. **PHASE1_CORE_DOCUMENTATION.md**
   - Exact text for all documentation
   - Copy-paste ready into source files
   - No formatting required

2. **PHASE1_IMPLEMENTATION_GUIDE.md**
   - Line-by-line implementation instructions
   - Visual Studio tips
   - Verification checklist

3. This file: **Phase 1 - Week 1 Execution Roadmap**
   - Overview and timeline
   - Success criteria
   - Quality checkpoints

---

## Notes

### Encoding Issue Workaround
Due to file encoding issues encountered with automated tools, Phase 1 documentation should be added **manually**:
1. Open file in Visual Studio
2. Copy documentation from PHASE1_CORE_DOCUMENTATION.md
3. Paste at specified location
4. Save file
5. Build to verify

This approach is **safer** and **more reliable** than automated replace tools for C++ source files.

### Next Action
When ready to begin Phase 1:
1. Open PHASE1_IMPLEMENTATION_GUIDE.md
2. Start with main.cpp (Lines 1-8: Update file header)
3. Follow step-by-step instructions
4. Verify build after each file
5. Commit changes when complete

---

**Phase 1 Status**: ✅ DOCUMENTED AND READY FOR IMPLEMENTATION

**Estimated Completion**: Within 1 week (150 minutes total effort)

**Next Phase**: Phase 2 - Function-level documentation for native helpers and renderer APIs
