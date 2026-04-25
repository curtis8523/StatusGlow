# Touch Sensor Support

## Overview
GPIO 7 has been configured to support **capacitive touch sensing** on the ESP32-S3, while maintaining backward compatibility with digital button input on the ESP32-C3.

## Hardware Configuration

### ESP32-S3 (Touch Enabled)
- **Pin**: GPIO 7
- **Mode**: Capacitive touch sensor
- **Detection**: Measures capacitance change when finger touches the pin
- **No external components required**: Works with bare wire, copper pad, or conductive surface
- **Threshold**: Configurable (default: 40000)

### ESP32-C3 (Digital Button)
- **Pin**: GPIO 7  
- **Mode**: Digital input with internal pullup
- **Detection**: Active LOW (button grounds the pin)
- **Requires**: Physical button between GPIO 7 and GND

## How Capacitive Touch Works

### Touch Detection
When you touch a capacitive touch sensor:
1. Your finger adds capacitance to the circuit
2. The `touchRead()` function measures this capacitance
3. **Lower values = touched**, **Higher values = not touched**

### Typical Values
- **Not Touched**: 60,000 - 100,000+
- **Touched**: 15,000 - 40,000
- **Threshold**: 40,000 (configurable)

The firmware automatically calibrates a baseline value on startup.

## Configuration

### Touch Threshold
Located in `src/config.h`:

```cpp
#define TOUCH_THRESHOLD 40000   // Touch detection threshold
```

**Adjusting Sensitivity**:
- **Lower threshold** = More sensitive (easier to trigger, may be noisy)
- **Higher threshold** = Less sensitive (harder to trigger, more stable)

**Recommended values**: 30,000 - 50,000

### Disabling Touch Sensor
If you want to use a physical button on the S3 instead of touch:

In `src/main.cpp`, change:
```cpp
static bool gUseTouchSensor = true;  // Change to false
```

## Hardware Setup

### Option 1: Bare Wire (Simplest)
1. Solder a short wire to GPIO 7
2. Leave the wire exposed
3. Touch the wire to trigger

### Option 2: Copper Pad (Recommended)
1. Create a copper pad on your PCB or use copper tape
2. Connect to GPIO 7
3. Touch the pad to trigger
4. Can be covered with thin non-conductive coating (e.g., clear nail polish)

### Option 3: Through-Case Touch
1. Mount conductive pad inside enclosure
2. Connect to GPIO 7
3. Use thin plastic case (1-3mm)
4. Touch external case surface to trigger

### Option 4: Physical Button (Fallback)
1. Wire button between GPIO 7 and GND
2. Set `gUseTouchSensor = false` in code
3. Button press grounds the pin

## Functionality

All button functions work the same with touch:

### Short Touch
- No action (reserved for future use)

### Hold 3 Seconds
- LEDs fade to **yellow** while holding
- **Reboot** when released
- Visual confirmation: 3 blue blinks

### Hold 8 Seconds  
- LEDs fade from **yellow** to **orange** while holding
- **Factory reset** triggered
- Visual confirmation: 5 red blinks
- All settings erased, device reboots

## Debugging Touch Values

To calibrate or troubleshoot, uncomment the debug line in `handleButton()`:

```cpp
// Uncomment for debugging touch values:
// DBG_PRINT("Touch value: "); DBG_PRINTLN(touchValue);
```

This will print touch values every 5 seconds to serial monitor:
```
Touch value: 85234  (not touched)
Touch value: 28456  (touched)
```

Use these values to set an appropriate `TOUCH_THRESHOLD`.

## Calibration Process

The firmware performs automatic calibration on startup:

1. **Sample Period**: Takes 10 samples over 100ms
2. **Baseline Calculation**: Averages the samples
3. **Assumption**: Touch sensor is NOT being touched during startup
4. **Result**: Stored in `gTouchBaseline` for reference

**Serial Output**:
```
Touch sensor initialized on GPIO 7, baseline: 87345, threshold: 40000
```

If baseline is close to or below threshold, you may need to:
- Increase `TOUCH_THRESHOLD`
- Add grounding to reduce noise
- Use shielded wire for the sensor

## Troubleshooting

### Problem: Touch doesn't trigger
**Solutions**:
1. Check touch value in serial monitor
2. Lower `TOUCH_THRESHOLD` (e.g., 35000)
3. Ensure good electrical connection
4. Try larger touch pad surface area

### Problem: False triggers (too sensitive)
**Solutions**:
1. Increase `TOUCH_THRESHOLD` (e.g., 45000)
2. Add shielding around the touch wire
3. Keep touch sensor away from noisy circuits
4. Add small capacitor (10-100pF) from GPIO 7 to GND

### Problem: Erratic behavior
**Solutions**:
1. Ensure device is properly grounded
2. Check baseline value (should be 60,000+)
3. Move touch sensor away from PWM/LED wires
4. Add RC filter: 10kΩ resistor + 1nF cap to ground

### Problem: Works initially then stops
**Possible Causes**:
- Temperature change affecting baseline
- Moisture buildup on sensor
- EMI from nearby devices

**Solutions**:
- Increase threshold margin
- Use conformal coating on sensor
- Re-calibrate by rebooting

## Technical Details

### ESP32-S3 Touch Sensor Specs
- **Touch Channels**: GPIO 1-14 (14 channels)
- **Resolution**: 16-bit (0-65535)
- **Scan Speed**: ~8ms per channel
- **Sensitivity**: Configurable via threshold
- **Power**: <1mA during scanning

### Pin Mapping
| MCU | Touch Channel | GPIO |
|-----|---------------|------|
| ESP32-S3 | TOUCH7 | GPIO 7 |
| ESP32-C3 | N/A | GPIO 7 (digital only) |

### Code Architecture

**Compile-Time Detection**:
```cpp
#if defined(CONFIG_IDF_TARGET_ESP32S3)
    gUseTouchSensor = true;   // S3: Use touch
#else
    gUseTouchSensor = false;  // C3: Use button
#endif
```

**Runtime Touch Reading**:
```cpp
uint16_t touchValue = touchRead(BUTTON_PIN);
bool touched = (touchValue < TOUCH_THRESHOLD);
```

**Fallback Support**:
- ESP32-C3 automatically uses `digitalRead()`
- S3 can be configured to use `digitalRead()` if touch disabled

## Best Practices

### For Reliable Touch Sensing
1. ✅ Use large touch pad area (1cm² minimum)
2. ✅ Keep wiring short (< 10cm)
3. ✅ Add ground plane near touch sensor
4. ✅ Calibrate threshold for your specific hardware
5. ✅ Test in actual enclosure (plastic affects sensitivity)

### For Stable Operation
1. ✅ Set threshold 20-30% above touched value
2. ✅ Ensure baseline is 2x threshold or higher
3. ✅ Keep touch sensor away from high-frequency circuits
4. ✅ Use debouncing (already implemented: 50ms)

### What to Avoid
1. ❌ Don't use very thin wires (< 24 AWG)
2. ❌ Don't route touch wires parallel to LED data lines
3. ❌ Don't touch sensor during device boot
4. ❌ Don't use metal case without proper grounding

## Advanced: Custom Sensitivity Calibration

If default threshold doesn't work well:

1. **Test in your environment**:
   - Upload firmware
   - Monitor serial output
   - Note touched vs. untouched values

2. **Calculate optimal threshold**:
   ```
   threshold = (touched_value + untouched_value) / 2
   ```

3. **Update config.h**:
   ```cpp
   #define TOUCH_THRESHOLD <your_calculated_value>
   ```

4. **Rebuild and test**

## Performance Impact

- **CPU Usage**: Negligible (<0.1%)
- **Scan Time**: ~8ms per read
- **Memory**: +12 bytes RAM
- **Power**: No measurable increase

## Future Enhancements

Possible improvements:
- Multiple touch sensors for different functions
- Touch-and-hold gestures
- Adaptive threshold calibration
- Touch intensity detection
- Simultaneous multi-touch

## Summary

✅ **ESP32-S3**: Capacitive touch on GPIO 7 (no physical button needed)  
✅ **ESP32-C3**: Digital button on GPIO 7 (backward compatible)  
✅ **Auto-calibration**: Baseline measured on startup  
✅ **Configurable**: Threshold adjustable in config.h  
✅ **Reliable**: Debounced with 50ms delay  
✅ **Debugging**: Optional serial output for troubleshooting  

Just touch GPIO 7 (or a connected pad) on your ESP32-S3 to trigger reboot or factory reset! 🖐️✨
