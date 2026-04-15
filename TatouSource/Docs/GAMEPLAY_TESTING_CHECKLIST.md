# Gameplay Testing Checklist - Native Life Scripts Validation

## Pre-Test Verification
- [x] Build compiles successfully
- [x] No compilation errors or warnings
- [x] All helper functions verified 1:1 with bytecode
- [x] Critical `currentLifePtr` bug fixed
- [x] Edge cases checked and validated
- [x] Code review completed

---

## Test Area 1: Door Mechanics

### Test Case 1.1: Door Opening Timing
- **Script**: Script 22 (door idle/opening state machine)
- **Expected Behavior**: Doors open and close smoothly at consistent speed
- **Previous Bug**: Doors opening too quickly or in wrong direction
- **Root Cause Fixed**: currentLifePtr stale data → rotation interpolation state corruption
- **Test Steps**:
  1. Enter a room with a door
  2. Trigger the door to open
  3. Observe animation timing and direction
  4. Confirm smooth rotation without jerking
  5. Verify closing animation matches opening speed
- **Pass Criteria**: 
  - [ ] Door opens smoothly in correct direction
  - [ ] Door closes smoothly in opposite direction
  - [ ] Opening/closing speed consistent
  - [ ] No animation jerking or skipping frames

### Test Case 1.2: Door Direction Consistency
- **Expected Behavior**: Doors always rotate in consistent direction
- **Test Steps**:
  1. Open and close same door 3+ times
  2. Verify rotation direction is always the same
  3. Test multiple doors in different rooms
- **Pass Criteria**:
  - [ ] All doors rotate in expected direction
  - [ ] No directional flip or inconsistency
  - [ ] Multiple doors behave consistently

### Test Case 1.3: Door State Transitions
- **Expected Behavior**: Door state changes (closed→opening→open→closing→closed)
- **Test Steps**:
  1. Trigger door open
  2. While opening, check if actor can interact with it
  3. Let door fully open
  4. Verify interaction state in open position
  5. Close door and verify state changes
- **Pass Criteria**:
  - [ ] State transitions are correct
  - [ ] Actor cannot pass through partially-open door
  - [ ] Door fully opens before responding to close command

---

## Test Area 2: Character Movement

### Test Case 2.1: Movement Through Rooms
- **Expected Behavior**: Character moves smoothly without getting stuck
- **Previous Bug**: Character getting stuck on obstacles
- **Root Cause Fixed**: currentLifePtr corruption → movement state initialization issues
- **Test Steps**:
  1. Enter a room with furniture/obstacles
  2. Move character through the room
  3. Walk near doors, furniture, and walls
  4. Try to navigate around obstacles
  5. Check for smooth movement flow
- **Pass Criteria**:
  - [ ] Character moves smoothly in all directions
  - [ ] No getting stuck on doors or furniture
  - [ ] No unexpected stopping or jerking
  - [ ] Collision detection works properly

### Test Case 2.2: Track-Based Movement
- **Expected Behavior**: Characters following tracks move smoothly
- **Modes Tested**: 
  - Mode 2 (Follow mode) - following the player
  - Mode 3 (Patrol mode) - patrolling fixed track
- **Test Steps**:
  1. Trigger a follower character (mode 2)
  2. Verify smooth following without getting stuck
  3. Find a character on patrol (mode 3)
  4. Observe smooth patrol pattern
  5. Interrupt and resume patrol
- **Pass Criteria**:
  - [ ] Followers smoothly follow without losing path
  - [ ] Patrol characters don't get stuck at waypoints
  - [ ] Speed changes are smooth, not jerky
  - [ ] State resets properly when switching modes

### Test Case 2.3: Movement Speed Consistency
- **Expected Behavior**: Movement speed remains consistent
- **Test Steps**:
  1. Start character movement
  2. Monitor speed for inconsistencies
  3. Test fast movement (run)
  4. Test slow movement (walk)
  5. Test track-based movement speeds
- **Pass Criteria**:
  - [ ] Speed changes smoothly
  - [ ] No sudden acceleration/deceleration
  - [ ] Speed interpolation works correctly

---

## Test Area 3: Combat Operations

### Test Case 3.1: Melee Attack (HIT)
- **Expected Behavior**: Melee attacks execute with proper animation and hit detection
- **Previous Bug**: Fighting not working properly
- **Root Cause Fixed**: currentLifePtr corruption → actor context during HIT operations
- **Test Steps**:
  1. Engage enemy in melee combat
  2. Execute melee attack
  3. Verify hit detection (does damage register?)
  4. Check animation plays correctly
  5. Verify next animation transitions properly
- **Pass Criteria**:
  - [ ] Attack animation plays smoothly
  - [ ] Hit detection works (damage is applied)
  - [ ] Animation transitions to next state correctly
  - [ ] No missing or skipped frames

### Test Case 3.2: Ranged Attack (FIRE)
- **Expected Behavior**: Ranged attacks execute with projectile emission
- **Test Steps**:
  1. Engage enemy with ranged weapon
  2. Execute ranged attack
  3. Verify projectile emission from correct point
  4. Check hit detection on target
  5. Verify animation completes properly
- **Pass Criteria**:
  - [ ] Fire animation plays correctly
  - [ ] Projectile emits from correct location
  - [ ] Hit detection works on distant targets
  - [ ] Animation transitions properly

### Test Case 3.3: Hit-On-Contact Detection
- **Expected Behavior**: Continuous hit detection during attack animation
- **Test Steps**:
  1. Enable hit-on-contact mode (LM_HIT_OBJECT)
  2. Move attacking actor through enemy
  3. Verify hits are registered during movement
  4. Exit hit-on-contact mode
  5. Verify detection stops
- **Pass Criteria**:
  - [ ] Hits register during animation playback
  - [ ] Detection state clears properly
  - [ ] No lingering hit state after disabling

### Test Case 3.4: Combat State Consistency
- **Expected Behavior**: Combat state remains consistent across transitions
- **Test Steps**:
  1. Start combat with one enemy
  2. Engage second enemy while attacking first
  3. Switch targets quickly
  4. Verify state changes are clean
- **Pass Criteria**:
  - [ ] Target switches smoothly
  - [ ] No combat state corruption
  - [ ] Actor states remain consistent

---

## Test Area 4: Object Interactions

### Test Case 4.1: Found Objects (life_Found)
- **Expected Behavior**: Objects correctly transition to "found" state
- **Previous Bug**: Behavioral inconsistency with bytecode
- **Root Cause Fixed**: currentLifePtr management in found-object callbacks
- **Test Steps**:
  1. Find a collectible object in the game
  2. Trigger the "found" lifecycle (life_Found)
  3. Verify object transitions to found state
  4. Check if object appears in inventory
  5. Verify object animation/behavior changes
- **Pass Criteria**:
  - [ ] Object correctly identified as found
  - [ ] Item appears in inventory
  - [ ] Found animation plays correctly
  - [ ] Object state changes properly (disappears/transforms)

### Test Case 4.2: Object Type Attributes
- **Expected Behavior**: Object attributes set correctly based on type
- **Test Steps**:
  1. Find different object types (actor vs world object)
  2. Verify type flags are set correctly
  3. Check object interactions respond to correct attributes
  4. Verify loaded vs not-loaded objects behave differently
- **Pass Criteria**:
  - [ ] Actor attributes (AF_MASK) applied correctly
  - [ ] World object attributes (TYPE_MASK) applied correctly
  - [ ] Interaction behavior matches object type

### Test Case 4.3: Object Animation
- **Expected Behavior**: Object animations play correctly
- **Test Steps**:
  1. Trigger various object animations
  2. Check ANIM_ONCE (play once) works
  3. Check ANIM_REPEAT (loop) works
  4. Check ANIM_UNINTERRUPTABLE (can't be interrupted) works
  5. Verify animation state transitions
- **Pass Criteria**:
  - [ ] ANIM_ONCE animations play once and stop
  - [ ] ANIM_REPEAT animations loop continuously
  - [ ] ANIM_UNINTERRUPTABLE prevents interruption
  - [ ] State transitions are clean

---

## Test Area 5: Audio/Visual Effects

### Test Case 5.1: Sound Effects
- **Expected Behavior**: Sound effects play at correct times
- **Test Steps**:
  1. Trigger events that play sounds (door open, hit, etc.)
  2. Verify sounds play at appropriate moments
  3. Check sound timing with animation
  4. Test looping sounds
  5. Test stopping sounds
- **Pass Criteria**:
  - [ ] Sounds play at correct animation frames
  - [ ] Sound timing matches visual effects
  - [ ] Looping sounds continue properly
  - [ ] Sound stop commands work

### Test Case 5.2: Special Effects
- **Expected Behavior**: Special effects (evaporate, blood, smoke) display correctly
- **Test Steps**:
  1. Trigger death/destruction (evaporate effect)
  2. Verify effect appears at correct position
  3. Check effect completes properly
  4. Trigger other special effects (blood, smoke, etc.)
- **Pass Criteria**:
  - [ ] Effects appear at correct actor position
  - [ ] Effect animation completes properly
  - [ ] Multiple effects don't interfere with each other

### Test Case 5.3: Visual State Transitions
- **Expected Behavior**: Visual state changes smoothly
- **Test Steps**:
  1. Trigger angle/rotation changes (SET_BETA/SET_ALPHA)
  2. Verify smooth interpolation
  3. Check 360° rotation edge cases
  4. Verify angle wrapping (10-bit angles)
- **Pass Criteria**:
  - [ ] Rotations are smooth, not jumpy
  - [ ] 360° wraparound handled correctly
  - [ ] Angle changes complete properly

---

## Test Area 6: Advanced Scenarios

### Test Case 6.1: Nested Script Execution
- **Expected Behavior**: Found-life scripts execute correctly within native scripts
- **Previous Bug**: Nested execution corrupted by stale currentLifePtr
- **Root Cause Fixed**: Proper state management in executeFoundLife
- **Test Steps**:
  1. Find complex interaction triggering found-life
  2. Verify found-life script executes correctly
  3. Check state transitions in nested execution
  4. Trigger multiple nested scripts in sequence
- **Pass Criteria**:
  - [ ] Found-life scripts execute without corruption
  - [ ] State is properly restored after nested execution
  - [ ] Multiple nested calls work correctly
  - [ ] No state leakage between scripts

### Test Case 6.2: Actor Context Switching
- **Expected Behavior**: Context switches to other actors work correctly
- **Test Steps**:
  1. Trigger action affecting another actor (LIFE_TARGET)
  2. Verify correct actor is modified
  3. Check context is restored properly
  4. Test multiple context switches in sequence
- **Pass Criteria**:
  - [ ] Context switches to correct actor
  - [ ] Operations affect correct actor
  - [ ] Context properly restored
  - [ ] No state corruption from context switching

### Test Case 6.3: Edge Case - Null/Invalid Actors
- **Expected Behavior**: Operations on invalid actors handled gracefully
- **Test Steps**:
  1. Test operations on non-loaded actors
  2. Test operations on invalid actor indices
  3. Verify error handling
  4. Check field access for limited cases (ROOM, STAGE only)
- **Pass Criteria**:
  - [ ] No crashes on invalid actors
  - [ ] Limited field access works for not-loaded
  - [ ] Error handling is graceful

### Test Case 6.4: Performance Under Load
- **Expected Behavior**: No performance degradation with all fixes
- **Test Steps**:
  1. Run scenes with many actors
  2. Run scenes with complex scripts
  3. Monitor frame rate
  4. Check for lag spikes
  5. Test long play sessions
- **Pass Criteria**:
  - [ ] Frame rate remains consistent
  - [ ] No unexpected lag spikes
  - [ ] Long play sessions remain stable
  - [ ] No memory leaks

---

## Test Area 7: Regression Testing

### Test Case 7.1: Previously Working Features
- **Expected Behavior**: Features that worked before still work
- **Test Steps**:
  1. Go through common gameplay actions
  2. Verify basic movement still works
  3. Verify camera controls still work
  4. Verify saving/loading still works
  5. Verify menu navigation still works
- **Pass Criteria**:
  - [ ] All basic features still functional
  - [ ] No new bugs introduced
  - [ ] Performance not degraded

### Test Case 7.2: Save Game Compatibility
- **Expected Behavior**: Old save games load and work correctly
- **Test Steps**:
  1. Load an old save game from before fix
  2. Continue playing from save point
  3. Verify game state is valid
  4. Verify all features work
- **Pass Criteria**:
  - [ ] Save game loads without errors
  - [ ] Game continues normally
  - [ ] No state corruption visible
  - [ ] No features broken

---

## Test Area 8: Final Validation

### Comprehensive Gameplay Test
- **Duration**: 30+ minutes of normal gameplay
- **Areas to Test**: 
  - Multiple rooms with doors
  - Complex combat scenarios
  - Various object interactions
  - NPC interactions
- **Metrics to Monitor**:
  - Stability (no crashes)
  - Performance (consistent frame rate)
  - Correctness (expected behavior observed)
  - Quality (smooth animations, proper audio)

### Success Criteria
- [ ] No crashes during gameplay
- [ ] All major features work correctly
- [ ] Door mechanics work properly
- [ ] Character movement is smooth
- [ ] Combat operations function correctly
- [ ] Object interactions behave correctly
- [ ] Audio/visual effects play correctly
- [ ] No regression in previously working features
- [ ] Save game compatibility maintained

---

## Notes and Observations

### During Testing
Keep note of:
- Any unusual behavior or edge cases
- Performance characteristics
- Player experience quality
- Any remaining issues

### Post-Testing
- Document any bugs found
- Record performance metrics
- Note any behavior differences from bytecode
- Provide feedback on fix effectiveness

---

## Sign-Off

**Build**: [Version]  
**Test Date**: [Date]  
**Tester**: [Name]  
**Status**: [ ] PASS [ ] FAIL [ ] PENDING

**Comments**:
[Space for testing notes]

---

**CRITICAL FIX VALIDATION CHECKLIST READY**

Use this checklist to validate that the `currentLifePtr` bug fix resolves all gameplay issues related to doors, movement, and combat. ✅

