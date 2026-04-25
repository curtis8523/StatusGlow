# Flickering Issue - Root Causes and Fixes

## Problem Description
LEDs were flickering during transitions, especially when:
- Web UI was refreshed or interacted with
- Status changes occurred
- Preview mode was being used
- Multiple state transitions happened rapidly

## Root Causes Identified

### 1. **Redundant Animation Calls**
**Issue**: The state machine was calling `setAnimation()` repeatedly for the same animation, even when already displaying that effect.

**Location**: State machine transitions (WiFi connecting, connected, device login, etc.)

**Impact**: Each call would restart the fade, interrupting any in-progress fade and causing visible flicker.

**Example**:
```cpp
// Called every loop iteration while in SMODEWIFICONNECTING
if (state == SMODEWIFICONNECTING && laststate != SMODEWIFICONNECTING) {
    setAnimation(0, FX_MODE_THEATER_CHASE, BLUE);
}
```

The `laststate` check prevented *some* redundancy, but if `setAnimation()` was called from multiple paths, flickering occurred.

### 2. **Missing Duplicate Detection in setAnimation()**
**Issue**: `setAnimation()` didn't check if it was already showing or fading to the requested animation.

**Impact**: Every call would start a new fade, even if:
- Already displaying that exact animation
- Already in the middle of fading to that animation

**Code Path**:
```cpp
setAnimation(mode, color, speed, reverse)
  ↓
  No check for current state
  ↓
  Always starts new fade
  ↓
  Interrupts existing fade = flicker
```

### 3. **Race Condition in /api/preview Handler**
**Issue**: The `/api/preview` endpoint was modifying global fade variables without mutex protection.

**Location**: `src/main.cpp` line ~1475

**Before**:
```cpp
gFade.durationMs = perFade;  // ❌ No lock!
gNextTargetBri = perBri;     // ❌ No lock!
setAnimation(0, mode, color, speed, reverse);
```

**Impact**: 
- Web server thread (Core 0) modifies `gFade.durationMs`
- LED rendering thread (Core 1) reads `gFade.durationMs` simultaneously
- Race condition causes incorrect fade timing or brightness
- Results in brief wrong colors or flicker

### 4. **Fade Interruption During Two-Phase Transitions**
**Issue**: When a new `setAnimation()` call came in during an active fade-out phase, it would immediately start a new fade-out, causing the LEDs to:
1. Fade down (first fade-out)
2. Get interrupted mid-fade
3. Fade down again (second fade-out)
4. Visible brightness stutter

## Fixes Implemented

### Fix 1: Duplicate Animation Detection

Added comprehensive checking in `setAnimation()`:

```cpp
// Check if we're already showing this exact animation
if (gTarget.initialized) {
    bool sameTarget = (gTarget.mode == mode) && (gTarget.color == color) && 
                      (gTarget.speed == speed) && (gTarget.reverse == reverse);
    if (sameTarget && !gFade.active) {
        // Already at target and not fading, nothing to do
        EFFECTS_UNLOCK();
        return;
    }
    
    // Check if we're already fading to this exact animation
    if (gFade.active && gFade.phaseOut) {
        bool samePending = (gFade.pendingMode == mode) && (gFade.pendingColor == color) && 
                           (gFade.pendingSpeed == speed) && (gFade.pendingReverse == reverse);
        if (samePending) {
            // Already fading to this target, don't interrupt
            EFFECTS_UNLOCK();
            return;
        }
    }
}
```

**Benefits**:
- Prevents redundant animations from restarting
- Protects in-progress fades from interruption
- Eliminates flicker from duplicate calls

### Fix 2: Mutex Protection for /api/preview

Added `EFFECTS_LOCK()` around global variable modifications:

```cpp
EFFECTS_LOCK();
gFade.durationMs = perFade;
gNextTargetBri = perBri;
EFFECTS_UNLOCK();
setAnimation(0, mode, color, speed, reverse);
```

**Benefits**:
- Prevents race conditions between Core 0 (web server) and Core 1 (LED rendering)
- Ensures atomic updates to fade state
- Eliminates random brightness or timing glitches

### Fix 3: Consistent Fade Protection

Verified all paths that modify `gFade.durationMs` or `gNextTargetBri` are protected:

✅ `setPresenceAnimation()` - Already protected  
✅ `applyPreviewSelection()` - Already protected  
✅ `/api/preview` handler - **Fixed with mutex**  
✅ `updateFade()` - Called from locked context  
✅ `startFade()` - Called from locked context  

## Testing Results

### Before Fixes
- ❌ Flicker on web UI refresh
- ❌ Flicker when changing preview profiles
- ❌ Stutter during status transitions
- ❌ Occasional wrong colors shown briefly
- ❌ Interruption of smooth fades

### After Fixes
- ✅ Smooth transitions even during web UI activity
- ✅ No flicker when switching preview profiles
- ✅ Clean two-phase fades (fade-out → fade-in)
- ✅ No race conditions or timing glitches
- ✅ Protected fades complete without interruption

## Performance Impact

### CPU Usage
- **Minimal**: Added checks are simple comparisons
- **Core 0**: No measurable increase
- **Core 1**: No measurable increase

### Latency
- **Status Changes**: Slightly faster (fewer redundant fades)
- **Web UI**: No change
- **Fade Quality**: Significantly improved (no interruptions)

## Code Flow Diagram

### Before (Flickering)
```
Web UI Click
  ↓
/api/preview handler
  ↓
gFade.durationMs = value  ← No lock! (Race condition)
  ↓
setAnimation()
  ↓
Start new fade  ← No duplicate check!
  ↓
Interrupt active fade  ← Flicker!
```

### After (Fixed)
```
Web UI Click
  ↓
/api/preview handler
  ↓
EFFECTS_LOCK()
gFade.durationMs = value  ← Protected
EFFECTS_UNLOCK()
  ↓
setAnimation()
  ↓
EFFECTS_LOCK()
Check if already at target → YES → Return (no flicker)
                          → NO  → Continue
Check if fading to target → YES → Return (no flicker)
                          → NO  → Continue
Start new fade  ← Only if needed
EFFECTS_UNLOCK()
```

## Additional Safeguards

### State Tracking
The `gTarget` structure now accurately tracks:
- Current mode, color, speed, reverse settings
- Updated atomically inside mutex
- Checked before every transition

### Fade State Tracking
The `gFade` structure tracks:
- Active fade in progress
- Phase (fade-out vs fade-in)
- Pending animation settings
- All protected by mutex

### Thread Safety
All shared state accessed by both cores:
- `gFade.*` - Protected by `EFFECTS_LOCK()`
- `gTarget.*` - Protected by `EFFECTS_LOCK()`
- `gNextTargetBri` - Protected by `EFFECTS_LOCK()`
- `effects.*` - Protected by `EFFECTS_LOCK()`

## Debugging Tips

If flickering returns in the future, check:

1. **New API endpoints**: Ensure any new handlers that modify fade state use `EFFECTS_LOCK()`
2. **State machine changes**: Verify `setAnimation()` calls are only made on state transitions
3. **Preview mode**: Check preview-related code paths for proper locking
4. **Manual animation calls**: Ensure any direct `effects.setSegment()` calls are locked

## Summary

The flickering was caused by:
1. ❌ Redundant animation calls without duplicate detection
2. ❌ Race conditions from unlocked global variable access
3. ❌ Fade interruptions from overlapping transitions

Fixed by:
1. ✅ Comprehensive duplicate animation detection
2. ✅ Proper mutex protection for all fade state modifications
3. ✅ Protected in-progress fades from interruption

Result: **Smooth, flicker-free transitions** in all scenarios! 🎨✨
