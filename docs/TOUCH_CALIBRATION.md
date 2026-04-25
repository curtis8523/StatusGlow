# Touch Sensor Calibration & Troubleshooting

## What I Fixed

The touch sensor was being falsely triggered because the threshold was **static** (40,000) instead of **dynamic** based on your hardware's baseline.

### Changes Made

1. **Dynamic Threshold**: Now calculated as percentage of baseline
   - Default: 60% of baseline (40% drop triggers)
   - Adjustable in `config.h` via `TOUCH_THRESHOLD_PERCENT`

2. **Better Calibration**: 
   - Takes 20 samples over 400ms (was 10 over 100ms)
   - Tracks min/max values for debugging
   - Warns if baseline is suspiciously low

3. **Debug Output Enabled**: 
   - Shows touch value every 2 seconds
   - Displays baseline, threshold, and state
   - Always enabled to help you calibrate

## What You'll See on Serial Monitor

After uploading firmware, watch the serial output:

### During Boot (Calibration)
```
Calibrating touch sensor... (do not touch)
Touch sensor initialized on GPIO 7
  Baseline: 87234 (min: 85123, max: 89456)
  Threshold: 52340 (60% of baseline)
```

**If baseline is low (<50,000):**
```
WARNING: Touch baseline is low - you may be touching the sensor!
Please reboot without touching GPIO 7
```

### During Normal Operation
Every 2 seconds you'll see:
```
Touch: 86543 (baseline: 87234, threshold: 52340, state: released)
Touch: 85892 (baseline: 87234, threshold: 52340, state: released)
Touch: 28456 (baseline: 87234, threshold: 52340, state: PRESSED)  ← When touched
```

## How to Calibrate

### Step 1: Upload and Monitor
```bash
pio run -t upload && pio device monitor
```

### Step 2: Check Baseline
Look for the startup message:
- **Baseline > 70,000**: Excellent! Should work great
- **Baseline 50,000-70,000**: Good, may need minor adjustment
- **Baseline < 50,000**: WARNING - something touching the sensor

### Step 3: Test Touch
Touch GPIO 7 (or your touch pad) and watch the value:
- **Should drop by 40-60%** when touched
- Example: 87,000 → 30,000 (65% drop) ✅
- Example: 87,000 → 75,000 (14% drop) ❌ (not conductive enough)

### Step 4: Adjust if Needed

**If false triggers (shows PRESSED when not touched):**

1. Check your baseline value in serial output
2. Your current reading is probably close to threshold
3. Make threshold LOWER (more drop needed to trigger):

Edit `src/config.h`:
```cpp
#define TOUCH_THRESHOLD_PERCENT 50  // Change from 60 to 50
                                    // (requires 50% drop to trigger)
```

**If touch doesn't trigger:**

1. Check touched value vs. threshold
2. Touched value probably isn't dropping enough
3. Make threshold HIGHER (less drop needed to trigger):

Edit `src/config.h`:
```cpp
#define TOUCH_THRESHOLD_PERCENT 70  // Change from 60 to 70
                                    // (requires only 30% drop to trigger)
```

## Typical Values by Hardware Setup

| Setup | Baseline | Touched | Drop % | Recommended % |
|-------|----------|---------|--------|---------------|
| Bare wire (1cm) | 85,000 | 25,000 | 71% | 50-60% |
| Copper pad (2cm²) | 90,000 | 30,000 | 67% | 55-65% |
| Through plastic (2mm) | 80,000 | 45,000 | 44% | 65-75% |
| Through plastic (3mm) | 75,000 | 55,000 | 27% | 75-85% |

## Common Issues

### Issue: "PRESSED" constantly shown
**Cause**: Something is touching or near the touch sensor

**Fix**:
1. Check if wire/pad is touching anything
2. Move away from metal objects
3. Ensure clean, dry sensor surface
4. Lower threshold percent (50% or 45%)

### Issue: Baseline warning on boot
**Cause**: Sensor being touched during calibration

**Fix**:
1. Don't touch GPIO 7 during boot
2. Wait for "Calibrating..." message before touching
3. If using through-case, ensure case isn't pressing on pad

### Issue: Touch doesn't register
**Cause**: Not enough capacitance change

**Fix**:
1. Use larger touch pad (2cm² minimum)
2. Use thinner plastic if through-case
3. Increase threshold percent (70% or 75%)
4. Ensure good electrical connection

### Issue: Works then stops
**Cause**: Temperature/humidity change affecting baseline

**Fix**:
1. Power cycle to re-calibrate
2. Add adaptive threshold (future enhancement)
3. Use more stable hardware (shielded wire)

## Quick Reference: Threshold Percent

```
Lower percent = LESS sensitive = safer (fewer false triggers)
Higher percent = MORE sensitive = easier to trigger

Examples:
40% → Need 60% drop to trigger → Very insensitive
50% → Need 50% drop to trigger → Less sensitive
60% → Need 40% drop to trigger → Default/Balanced
70% → Need 30% drop to trigger → More sensitive
80% → Need 20% drop to trigger → Very sensitive
```

## To Disable Debug Output

Once calibrated, to reduce serial spam:

In `src/main.cpp`, change the debug interval:
```cpp
if (millis() - lastDebugMs > 10000) {  // Change from 2000 to 10000 (10 seconds)
```

Or comment out the entire debug block:
```cpp
// DBG_PRINT("Touch: "); 
// ... entire debug section ...
```

## Expected Behavior After Calibration

✅ **Normal operation**: Value stable around baseline  
✅ **When touched**: Value drops 40-60% below baseline  
✅ **State changes**: "released" → "PRESSED" → "released"  
✅ **No false triggers**: State stays "released" when not touching  

Upload the new firmware and monitor the serial output - you should see the actual touch values which will help determine if we need further adjustment! 🔧
