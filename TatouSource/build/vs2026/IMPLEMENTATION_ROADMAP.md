# Multithreading Implementation Roadmap

## Quick Reference: Where Multithreading Helps Most

### 🔴 CRITICAL BOTTLENECKS (Blocks 300+ ms)

1. **PAK File Loading** - `pak.cpp`, `floor.cpp`
   - Current: Synchronous, blocks main thread
   - With threading: Can load in background during fades/renders
   - Estimated savings: **200-400 ms per load**

2. **HD Background Processing** - `hdBackground.h`, `AITD1.cpp`
   - Current: Decode + texture upload blocks UI thread
   - With threading: Decode on worker, stream texture
   - Estimated savings: **100-200 ms per screen**

3. **Floor Initialization** - `floor.cpp:initFloor()`
   - Current: Sequential load of all room/animation/collision data
   - With threading: Parallel room parsing + async animation
   - Estimated savings: **400-600 ms** (50-70% of total load time)

### 🟡 IMPORTANT OPTIMIZATIONS (Blocks 50-200 ms)

4. **VOC File Decoding** - `osystemAL.cpp`
   - Current: Decode on main thread when page turns
   - With threading: Pre-load next page in background
   - Estimated savings: **50-100 ms per page turn**

5. **Texture Atlas Building** - `rendererBGFX.cpp`
   - Current: Blocks during floor load
   - With threading: Stream in background
   - Estimated savings: **100-150 ms**

---

## Implementation Priority & Timeline

### Phase 1: Infrastructure (Weeks 1-2)
**Effort:** 800-1000 LOC | **Payoff:** 30% performance gain

Create:
- `jobSystem.h` / `jobSystem.cpp` - Thread pool, job queue
- `asyncLoader.h` / `asyncLoader.cpp` - Asset-specific wrappers

Key functions:
```cpp
class JobSystem {
    void init(int ioThreads, int workerThreads);
    JobHandle scheduleJob(JobType, Priority, std::function work);
    void waitForJob(JobHandle);
    void shutdown();
};
```

### Phase 2: Asset Loading (Weeks 2-3)
**Effort:** 600-800 LOC | **Payoff:** 60% performance gain

Async functions to add:
- `asyncLoadPakFile(filename, index)` → loads in background
- `asyncLoadHDBackground(pakName, index)` → parallel decode
- `preloadPakAtStartup(list)` → pre-load common resources

### Phase 3: Audio System (Weeks 3-4)
**Effort:** 400-600 LOC | **Payoff:** 75% performance gain

Async functions:
- `asyncLoadVocFile(bookNum, pageNum, lineNum)`
- `asyncDecodeVocToWav(vocData)` → parallel per-file
- `preloadNextPageVoC()` → intelligent pre-fetch

### Phase 4: Testing & Profiling (Week 4)
**Effort:** 200-300 LOC

Add:
- Performance profiling hooks
- Memory monitoring
- Thread safety validation
- Stress testing

---

## One-Liner Changes Needed

| File | Change | Reason |
|------|--------|--------|
| `pak.cpp:loadPak()` | Add async variant | Enable background loading |
| `floor.cpp:initFloor()` | Schedule room parsing as jobs | Parallelize room data |
| `osystemAL.cpp:osystem_playVocPageLines()` | Use job system for decode | Remove blocking decode |
| `AITD1.cpp:makeIntroScreens()` | Pre-queue PAK loads | Load during fades |
| `mainLoop.cpp` | Poll job queue each frame | Check job completions |
| `rendererBGFX.cpp` | Stream texture uploads | Avoid frame stalls |

---

## Performance Expected Results

### Before Multithreading
```
Game Start → Character Select → Floor Load → Play
     ↓              ↓              ↓           ↓
   500ms        1000ms (freeze)  1200ms     Smooth
   
Total startup time: ~2700ms with visible freezes
```

### After Multithreading
```
Game Start → Character Select → Floor Load → Play
     ↓              ↓              ↓           ↓
   200ms        300ms (smooth)   400ms      Smooth
   (hidden)     (visible)        (loading   
                                   screen)
   
Total startup time: ~900ms with NO visible freezes
```

**Overall Improvement: 65-70% faster perceived performance**

---

## Key Architecture Points

### Thread Model
```
┌─ I/O Thread Pool (2-3 threads)
│  └─ File reads (PAK, HQR, CD-ROM)
│
├─ Worker Thread Pool (CPU_COUNT-1 threads)
│  ├─ Decompression (zlib, VOC decode)
│  ├─ Data parsing
│  └─ Texture processing
│
└─ Main Thread
   ├─ Rendering (bgfx)
   ├─ Input processing
   ├─ Life scripts (game logic)
   └─ Job queue polling
```

### Synchronization Strategy
```
Loading Screen:
1. Schedule all PAK loads → return immediately
2. Poll job queue, update progress bar
3. When all jobs done → show "Ready" message
4. Main thread continues

Page Turn in Book:
1. Start reading page N
2. Schedule load of page N+1 VOC in background
3. When page N VOC finishes, play it
4. When user turns page, page N+1 already loaded
5. Seamless transition (no stall)
```

### Memory Safety
- Use reference counting for shared assets
- Job captures ownership of data
- Main thread gets notification when complete
- Double-buffering for frame data

---

## Testing Checklist

- [ ] Thread pool initializes/shutdowns cleanly
- [ ] Jobs execute in correct priority order
- [ ] Asset loading produces identical results sync vs async
- [ ] No memory leaks under heavy load
- [ ] Frame time stays < 16ms (60 FPS)
- [ ] No race conditions with thread sanitizer
- [ ] Audio plays correctly with concurrent decode
- [ ] Floor loads don't stall rendering
- [ ] Startup screens feel responsive

---

## Files to Create

1. **FitdLib/jobSystem.h** - Public API
2. **FitdLib/jobSystem.cpp** - Implementation
3. **FitdLib/asyncLoader.h** - Asset loader wrappers
4. **FitdLib/asyncLoader.cpp** - Implementations
5. **Tests/jobSystemTests.cpp** - Unit tests (optional)

---

## Risk Mitigation

| Risk | Mitigation |
|------|-----------|
| Thread safety issues | Use thread-local storage, immutable data passing |
| Memory fragmentation | Pre-allocate pools, use custom allocators |
| Job starvation | Implement fair scheduling, monitor queue depth |
| Resource lifetime bugs | Reference counting, RAII patterns |
| Platform differences | Wrap std::thread, test on Linux/Windows |

---

## Success Criteria

✅ Game startup feels instant (no perceivable freezes)
✅ Character selection menu responds immediately  
✅ Floor loads in background with smooth loading screen
✅ Page turns in books are instantaneous
✅ Frame times stable at 60 FPS
✅ No audio glitches from concurrent decoding
✅ Memory usage doesn't increase significantly
✅ Linux and Windows platforms work identically

