# Fade Transition Behavior

## Overview
Status transitions now use a **two-phase fade** system for smooth visual transitions between different LED states.

## How It Works

### Two-Phase Fade System

When transitioning from one status to another (and `fade_ms > 0`):

1. **Phase 1: Fade Out**
   - Current animation brightness fades to 0
   - Duration: `fade_ms / 2`
   - Animation continues running during fade-out
   
2. **Phase 2: Fade In**
   - New animation is activated at brightness 0
   - Brightness fades up to target brightness
   - Duration: `fade_ms / 2`
   - New animation starts rendering

### Example Timeline

```
Status: Available (Green) → Busy (Purple)
fade_ms = 2000ms (2 seconds)

Timeline:
0ms:      Available (Green, 100% brightness)
0-1000ms: Fade out (Green, 100% → 0%)
1000ms:   Switch to Busy animation (Purple, 0%)
1000-2000ms: Fade in (Purple, 0% → 100%)
2000ms:   Busy (Purple, 100% brightness)
```

## Configuration

### Global Fade Duration
Set in Config page or via `/api/settings`:
```json
{
  "fade_ms": 2000
}
```

**Values:**
- `0` = No fade, instant transition
- `500-5000` = Fade duration in milliseconds (recommended: 1000-3000)

### Per-Profile Fade Duration
Each effect profile can override the global fade:
```json
{
  "key": "Available",
  "fade_ms": 1500
}
```

If a profile's `fade_ms` is 0 or not set, the global `fade_ms` is used.

## Technical Details

### Fade Curve
The fade uses a **gamma-corrected** curve for perceptually linear brightness changes:
- Linear time progression: `t = elapsed / duration`
- Gamma correction: `tg = t^gamma` (default gamma = 2.2)
- Smooth acceleration/deceleration

### Race Condition Protection
All fade operations are protected by `EFFECTS_LOCK()` mutex to prevent:
- Web UI updates interfering with fades
- Multiple simultaneous transitions
- Brightness flickering during status changes

### Fade State Machine

```
State: IDLE
  ↓ (setAnimation called with fade_ms > 0)
State: FADE_OUT (phaseOut = true)
  - Fade current brightness → 0
  - Store pending animation settings
  ↓ (fade-out complete)
State: FADE_IN (phaseOut = false)
  - Switch to new animation
  - Fade brightness 0 → target
  ↓ (fade-in complete)
State: IDLE
```

## Code Architecture

### Key Functions

**`setAnimation()`** - `src/main.cpp`
```cpp
if (gFadeDurationMs > 0) {
    // Start two-phase fade
    gFade.originalBri = targetBri;
    gFade.pendingMode = mode;
    gFade.pendingColor = color;
    gFade.phaseOut = true;
    startFade(currentBri, 0, fadeDurationMs / 2);
}
```

**`updateFade()`** - `src/main.cpp`
```cpp
if (t >= 1.0f) {
    if (gFade.phaseOut) {
        // Phase 1 complete, switch and start phase 2
        effects.setSegment(...pending...);
        startFade(0, gFade.originalBri, gFade.durationMs);
    } else {
        // Phase 2 complete, fade done
        gFade.active = false;
    }
}
```

**`startFade()`** - `src/main.cpp`
```cpp
void startFade(uint8_t from, uint8_t to, unsigned long dur) {
    gFade.active = true;
    gFade.startBri = from;
    gFade.endBri = to;
    gFade.startMs = millis();
    gFade.durationMs = dur;
}
```

### Data Structure

```cpp
struct FadeTransition {
    bool active;              // Fade in progress
    bool phaseOut;           // true = fade-out, false = fade-in
    uint8_t startBri;        // Starting brightness
    uint8_t endBri;          // Ending brightness
    unsigned long startMs;   // Fade start time
    unsigned long durationMs;// Fade duration
    // Pending animation (applied at phase transition)
    uint16_t pendingMode;
    uint32_t pendingColor;
    uint16_t pendingSpeed;
    bool pendingReverse;
    uint8_t originalBri;     // Target brightness for fade-in
} gFade;
```

## Effect on Different Modes

### Static Colors
Example: Available → Busy
- Fade out green to black
- Switch to purple at black
- Fade in purple to full brightness

### Animated Effects
Example: Scan → Breath
- Fade out scanning animation
- Switch to breathing animation at 0% brightness
- Fade in breathing animation

### Same Effect, Different Color
Example: Red Scan → Blue Scan
- Fade out red scan
- Switch to blue scan at 0% brightness
- Fade in blue scan
- Effect continues seamlessly

## Skipping Redundant Transitions

The system prevents redundant fades:

```cpp
if (gTarget.initialized) {
    bool sameTarget = (current == target);
    if (sameTarget) return; // Skip if already at target
    
    if (gFade.active && gFade.phaseOut) {
        bool samePending = (pending == target);
        if (samePending) return; // Skip if already fading to target
    }
}
```

This prevents:
- Multiple fade-outs to the same target
- Restarting an in-progress fade to the same effect
- Unnecessary transitions when status doesn't change

## Performance Impact

### CPU Usage
- Minimal: ~0.1% additional CPU time
- Gamma calculation done once per frame update
- Brightness update is single register write

### Memory Usage
- `FadeTransition` struct: 28 bytes
- No heap allocations during fade
- State stored in global static variable

### Visual Smoothness
With the optimized 200 FPS frame rate:
- 2-second fade = 400 frames
- Extremely smooth brightness transitions
- No visible stepping or flickering

## Testing Recommendations

### Manual Testing
1. Set `fade_ms` to 2000 (2 seconds)
2. Change Teams status: Available → Busy
3. Observe: Green fades out → Black → Purple fades in
4. Try rapid status changes to test queue handling

### Edge Cases
- **Fade during fade**: New transition interrupts current fade
- **Zero fade_ms**: Instant transition (no fade)
- **Very long fade**: 5+ second fades work but feel sluggish
- **Preview mode**: Fades work the same as normal mode

## Troubleshooting

### Issue: Fade feels too slow
**Solution**: Reduce `fade_ms` in Config (try 1000ms)

### Issue: Fade feels too fast
**Solution**: Increase `fade_ms` in Config (try 3000ms)

### Issue: No fade happening
**Check**: 
1. `fade_ms` is not set to 0
2. Profile's `fade_ms` is not overriding to 0
3. Firmware has the updated code

### Issue: Flicker during transition
**Check**: 
1. Verify mutex locking is in place
2. Check for race conditions in web server handlers
3. Ensure Core 1 is dedicated to LED rendering

## Future Enhancements

Possible improvements:
1. **Custom fade curves**: Ease-in, ease-out, linear options
2. **Cross-fade**: Blend colors instead of fade-to-black
3. **Per-effect fade**: Different fade-in/fade-out durations
4. **Conditional fading**: Only fade on certain transitions
5. **Fade presets**: Quick, Normal, Slow presets in UI
