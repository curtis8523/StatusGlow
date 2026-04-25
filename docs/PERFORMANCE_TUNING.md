# Quick Reference: LED Performance Tuning

## Current Settings (Optimized for ESP32-S3)

### Frame Rate Control
**Location**: `src/main.cpp` - Line ~923

```cpp
vTaskDelay(5 / portTICK_PERIOD_MS);  // Current: 5ms = ~200 FPS
```

**Adjustment Guide**:
| Delay (ms) | FPS | Use Case |
|------------|-----|----------|
| 3 | ~333 | Maximum smoothness (may cause slight CPU overhead) |
| 5 | ~200 | **Current - Optimal for most effects** |
| 8 | ~125 | Balanced performance |
| 12 | ~83 | Original setting (more conservative) |
| 16 | ~62 | Power saving mode |

### Effect Timing
**Location**: `src/led_effects.h` - Line ~264

```cpp
uint16_t frameMs = constrain(_speed / 50, 4, 100);
```

**Parameters**:
- **4ms**: Minimum delay between effect frames (250 FPS max)
- **100ms**: Maximum delay (10 FPS min for slow effects)
- **_speed / 50**: Dynamic scaling based on effect speed

## Troubleshooting Flicker

### 1. Web UI Refresh Flicker
**Symptom**: LEDs briefly show wrong color when refreshing browser  
**Cause**: Race condition during animation updates  
**Fixed**: `gTarget` updates now inside `EFFECTS_LOCK()`

### 2. Animation Jitter
**Symptom**: Animations appear choppy or stuttering  
**Possible Causes**:
- Frame delay too high → Reduce `vTaskDelay` value
- Effect complexity too high → Simplify effect calculations
- WiFi interference → Check for packet loss

### 3. Color Artifacts
**Symptom**: Brief wrong colors during transitions  
**Possible Causes**:
- Multiple `strip.show()` calls → Verify all calls are mutex-protected
- Timing issues → Ensure `renderFrame()` timing is consistent

## Performance Monitoring

### Check CPU Usage
Monitor the system with:
```cpp
// Available in your code at /api/status endpoint
uint8_t cpu0 = getCpuUsagePercent(); // Core 0
// Check via web UI or serial debug output
```

### Expected CPU Usage (ESP32-S3 @ 200 FPS)
- **Core 0**: 15-30% (WiFi, web server, state machine)
- **Core 1**: 5-10% (LED rendering only)

## Advanced Tuning

### For Maximum Smoothness
1. Reduce to 3ms delay (333 FPS)
2. Ensure WiFi power save is disabled (already done in code)
3. Use simpler color calculations in effects

### For Power Saving
1. Increase to 16ms delay (62 FPS)
2. Reduce LED brightness
3. Use static modes instead of animations

### For Complex Effects
1. Keep 5ms delay (200 FPS)
2. Optimize effect calculations
3. Consider pre-calculating color tables

## Testing Your Changes

### Build Command
```bash
pio run -e waveshare_esp32s3_zero
```

### Upload Command
```bash
pio run -e waveshare_esp32s3_zero -t upload
```

### Monitor Serial Output
```bash
pio device monitor
```

## Benchmarking

To test different frame rates:
1. Modify `vTaskDelay` value in `neopixelTask()`
2. Build and upload
3. Test with these effects for best visibility:
   - **Scan**: Shows smoothness of movement
   - **Comet**: Shows tail blending quality
   - **Running Lights**: Shows timing precision
   - **Rainbow Cycle**: Shows color transition smoothness

## Notes

- Changes require recompilation and upload
- ESP32-C3 single-core will cap at lower FPS
- LED protocol limits maximum practical FPS to ~500
- Brightness changes take effect immediately (no rebuild needed)
