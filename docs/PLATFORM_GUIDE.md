# Platform-Specific Configuration Guide

## Overview
StatusGlow supports three ESP32 variants with optimized configurations for each platform. This document explains the differences and how platform-specific code is organized.

## Supported Platforms

### 1. ESP32-C3 (Seeed XIAO ESP32C3)
**Architecture**: RISC-V single-core @ 160MHz  
**Memory**: 400KB SRAM, 4MB Flash  
**PlatformIO env**: `seeed_xiao_esp32c3`

**Characteristics**:
- ✅ Simple, reliable operation
- ✅ Low power consumption
- ✅ NeoPixel works with software timing (no special config)
- ⚠️ Single-core limits LED performance to ~83 FPS
- ⚠️ Cannot run WiFi and LED rendering in parallel

**Pin Configuration**:
- `GPIO8 (D8)`: LED strip data
- `GPIO7 (D7)`: Button (available)

**Build Flags**:
```ini
-DNUMLEDS=16
-DDATAPIN=D8
```

**No special NeoPixel configuration required** - software timing works perfectly.

---

### 2. ESP32-S3 (Seeed XIAO ESP32S3)
**Architecture**: Dual-core Xtensa LX7 @ 240MHz  
**Memory**: 512KB SRAM, 8MB PSRAM, 8MB Flash  
**PlatformIO env**: `seeed_xiao_esp32s3`

**Characteristics**:
- ✅ Dual-core enables parallel WiFi + LED rendering (~200 FPS)
- ✅ Large PSRAM for future expansion
- ⚠️ **Requires special NeoPixel configuration** (see below)
- ⚠️ WiFi requires explicit state management

**Pin Configuration**:
- `GPIO8 (D8)`: LED strip data
- `GPIO7 (D7)`: Button (available)

**Critical Build Flags**:
```ini
-DNUMLEDS=16
-DDATAPIN=D8
-DCONFIG_NEOPIXEL_RMT_DEFAULT=1  # Hardware RMT timing
-DCONFIG_FREERTOS_HZ=1000         # 1ms tick resolution
```

**Why Special Configuration?**
ESP32-S3's WiFi/Bluetooth interrupts can disrupt NeoPixel timing (WS2812 requires 1.25µs precision). The solution uses:
1. **RMT peripheral** (hardware timing) instead of software bit-banging
2. **Critical sections** (interrupt blocking during updates)
3. **Higher FreeRTOS tick rate** (better timing precision)

---

### 3. ESP32-S3 (Waveshare ESP32-S3-Zero)
**Architecture**: Dual-core Xtensa LX7 @ 240MHz  
**Memory**: 512KB SRAM, 2MB OPI PSRAM, 4MB Flash  
**PlatformIO env**: `waveshare_esp32s3_zero`

**Characteristics**:
- ✅ Dual-core enables parallel WiFi + LED rendering (~200 FPS)
- ✅ **Onboard WS2812 RGB LED** on GPIO21 (status indicator)
- ⚠️ **Requires special NeoPixel configuration** (same as XIAO S3)
- ⚠️ WiFi requires explicit state management

**Pin Configuration**:
- `GPIO8`: External LED strip data
- `GPIO7`: Button (available)
- `GPIO21`: **Onboard WS2812 status LED** (built-in)

**Critical Build Flags**:
```ini
-DNUMLEDS=16
-DDATAPIN=8           # Note: no 'D' prefix on Waveshare
-DCONFIG_NEOPIXEL_RMT_DEFAULT=1
-DCONFIG_FREERTOS_HZ=1000
```

---

## Platform-Specific Code Organization

### 📁 platformio.ini
**Location**: Project root  
**Purpose**: Build configuration for each platform

**Organization**:
```ini
[platformio]           # Project defaults

[env]                  # Common settings (shared)

[env:seeed_xiao_esp32c3]      # ESP32-C3 config
[env:seeed_xiao_esp32s3]      # ESP32-S3 XIAO config  
[env:waveshare_esp32s3_zero]  # ESP32-S3 Waveshare config
```

Each section clearly documents:
- Platform features
- Performance characteristics
- Why certain flags are needed
- Pin assignments

### 📄 src/main.cpp
**Lines 1-53**: Platform-specific configuration section

```cpp
// ============================================================================
// PLATFORM-SPECIFIC CONFIGURATION
// ============================================================================

// LED Task Core Assignment
#if defined(CONFIG_IDF_TARGET_ESP32S3)
  #define LED_TASK_CORE 1        // Dual-core: Use Core 1 for LEDs
  #define PLATFORM_NAME "ESP32-S3"
  #define IS_DUAL_CORE true
#else
  #define LED_TASK_CORE 0        // Single-core: Use Core 0
  #define PLATFORM_NAME "ESP32-C3"
  #define IS_DUAL_CORE false
#endif
```

**Key Points**:
- All platform detection in ONE place (top of file)
- Clear comments explaining why each setting exists
- Defines used throughout the codebase

### 📄 src/led_effects.h
**Lines 1-20**: Platform-specific LED timing documentation  
**Lines 70-92**: `safeShow()` method with platform detection

```cpp
// ============================================================================
// PLATFORM-SPECIFIC LED TIMING NOTES
// ============================================================================
// ESP32-S3: Requires RMT + critical sections
// ESP32-C3: Software timing works fine
// ============================================================================

void safeShow() {
  #if defined(CONFIG_IDF_TARGET_ESP32S3) || defined(ESP32S3)
    // ESP32-S3: Block interrupts
    portMUX_TYPE mux = portMUX_INITIALIZER_UNLOCKED;
    portENTER_CRITICAL(&mux);
    strip.show();
    portEXIT_CRITICAL(&mux);
  #else
    // ESP32-C3: Direct call
    strip.show();
  #endif
}
```

---

## ESP32-S3 NeoPixel Flicker Fix

### The Problem
ESP32-S3 WiFi/Bluetooth interrupts disrupt WS2812 LED timing:
- WS2812 protocol requires 1.25µs precision
- WiFi interrupts can delay timing by 10-100µs
- Results in: flickering, random colors, specific LEDs misbehaving

### The Solution (3-Part Fix)

#### 1. RMT Peripheral (Hardware Timing)
**Set in**: `platformio.ini`
```ini
-DCONFIG_NEOPIXEL_RMT_DEFAULT=1
```
**What it does**: Uses ESP32's RMT (Remote Control) peripheral for hardware-based timing instead of software bit-banging. More resistant to interrupts.

#### 2. FreeRTOS Tick Rate
**Set in**: `platformio.ini`
```ini
-DCONFIG_FREERTOS_HZ=1000
```
**What it does**: Increases tick resolution from default 100Hz to 1000Hz (1ms). Improves timing precision for RMT peripheral.

#### 3. Critical Sections
**Implemented in**: `led_effects.h` → `safeShow()` method
```cpp
portENTER_CRITICAL(&mux);  // Block interrupts
strip.show();               // Update LEDs
portEXIT_CRITICAL(&mux);    // Re-enable interrupts
```
**What it does**: Temporarily blocks ALL interrupts (WiFi, Bluetooth, timers) during the ~300µs LED update. Guarantees uninterrupted timing.

**Performance Impact**: Negligible - LEDs update in <1ms, WiFi handles the brief delay.

---

## ESP32-S3 WiFi Stability

### The Problem
ESP32-S3 WiFi stack is more sensitive than ESP32-C3:
- Requires explicit state cleanup
- Needs delays between mode changes
- Unstable without proper AP configuration

### The Solution (WiFi Helper Functions)
**Implemented in**: `src/main.cpp` lines 405-425

```cpp
// Safe WiFi mode change with delay
setWiFiModeWithDelay(mode, delayMs = 100);

// Disconnect with credential erase option
disconnectWiFiWithDelay(wifiOff, eraseAp, delayMs = 100);

// Configure default AP IP
configureDefaultAPIP();
```

**Key Pattern**:
```cpp
// Before changing WiFi state:
WiFi.disconnect(true);  // Clean state
delay(100);             // Let hardware settle
WiFi.mode(WIFI_STA);    // Change mode
delay(100);             // Let hardware settle
```

These patterns are **critical** for ESP32-S3 but optional for ESP32-C3.

---

## Performance Comparison

| Feature | ESP32-C3 | ESP32-S3 (XIAO) | ESP32-S3 (Waveshare) |
|---------|----------|-----------------|----------------------|
| **Cores** | 1 | 2 | 2 |
| **Clock** | 160MHz | 240MHz | 240MHz |
| **LED FPS** | ~83 | ~200 | ~200 |
| **PSRAM** | None | 8MB | 2MB OPI |
| **Flash** | 4MB | 8MB | 4MB |
| **NeoPixel Config** | Simple | Complex | Complex |
| **WiFi Stability** | Good | Needs helpers | Needs helpers |
| **Onboard LED** | No | No | Yes (GPIO21) |

---

## Quick Reference

### When to Use ESP32-C3
- ✅ Simple projects with <100 LEDs
- ✅ Power-sensitive applications
- ✅ Don't need maximum LED performance
- ✅ Want simpler configuration

### When to Use ESP32-S3
- ✅ Need high LED frame rates (>100 FPS)
- ✅ Large LED installations (>100 LEDs)
- ✅ Future expansion (lots of PSRAM)
- ✅ Want parallel WiFi + LED rendering
- ⚠️ Requires careful NeoPixel + WiFi configuration

---

## How to Switch Platforms

1. **Edit platformio.ini**:
   ```ini
   [platformio]
   default_envs = seeed_xiao_esp32c3  # Change this line
   ```

2. **Compile**:
   ```bash
   pio run -e seeed_xiao_esp32c3
   ```

3. **Upload**:
   ```bash
   pio run -e seeed_xiao_esp32c3 --target upload
   ```

**No code changes needed** - platform detection is automatic!

---

## Troubleshooting

### ESP32-S3: LEDs flickering or wrong colors
✅ **Verify build flags in platformio.ini**:
- `CONFIG_NEOPIXEL_RMT_DEFAULT=1` present?
- `CONFIG_FREERTOS_HZ=1000` present?

✅ **Check safeShow() is used**: All `strip.show()` calls should use `safeShow()` wrapper

### ESP32-S3: WiFi unstable or won't connect
✅ **Verify WiFi helpers are used**: Check that `disconnectWiFiWithDelay()` and `setWiFiModeWithDelay()` are called before WiFi operations

### ESP32-C3: Slow LED performance
✅ **Expected behavior**: Single-core limits to ~83 FPS. This is normal.
✅ **Upgrade to ESP32-S3** for 200 FPS

---

## Code Changes Required When Adding New Platforms

### ✅ Required Updates

1. **platformio.ini**: Add new `[env:platform_name]` section
2. **Document platform**: Add to this file
3. **Test**: Verify NeoPixel and WiFi work

### ❌ No Changes Needed

- `src/main.cpp`: Platform detection is automatic
- `src/led_effects.h`: `safeShow()` handles all platforms
- Helper functions: Platform-agnostic

---

## Summary

All platform-specific code is now organized in **three clear locations**:

1. **platformio.ini** → Build configuration
2. **src/main.cpp (lines 1-53)** → Core detection and task assignment
3. **src/led_effects.h (lines 1-92)** → NeoPixel timing fixes

Everything else is **platform-agnostic** and works on all ESP32 variants! 🎉

---

**Last Updated**: October 21, 2025  
**Platforms Tested**: ESP32-C3, ESP32-S3 (XIAO), ESP32-S3 (Waveshare)
