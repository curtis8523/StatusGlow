# Button/Touch Control Feature

## Overview
Your StatusGlow device now supports a physical button or touch sensor for device control. This provides a convenient way to reboot or factory reset the device without needing to access the web interface.

## Hardware Connection
- **LED Pin:** GPIO8 (D8 on XIAO ESP32C3/S3)
- **Button Pin:** GPIO7 (D7 on XIAO ESP32C3/S3)
- **Configuration:** Button should connect GPIO7 to GND when pressed
- **Internal:** Uses internal pull-up resistor (no external resistor needed)

## Button Functions

### 1. Quick Press (< 3 seconds)
- Visual feedback: LEDs show yellow that increases in brightness as you hold
- No action taken on release
- Used to test button connection

### 2. Reboot Device (3-8 seconds)
- Hold button for **3 seconds**
- Visual feedback: LEDs turn yellow, then blue blinks 3 times
- Device will reboot automatically
- All settings and configuration are preserved

### 3. Factory Reset (8+ seconds)
- Hold button for **8 seconds**
- Visual feedback: LEDs turn yellow → orange (getting brighter as warning)
- After 8 seconds: Red blinks 5 times, then device resets
- **WARNING:** This will:
  - Clear all WiFi credentials
  - Delete all configuration files (effects, settings, etc.)
  - Reset device to factory defaults
  - Require complete setup again

## Visual Feedback Guide

| Hold Duration | LED Color | Meaning |
|--------------|-----------|---------|
| 0-3 seconds | Yellow (increasing) | Button detected, building up to reboot |
| 3-8 seconds | Orange (increasing) | Reboot triggered, building up to factory reset |
| At 3 seconds | Blue blinks 3x | Rebooting now |
| At 8 seconds | Red blinks 5x | Factory reset in progress |

## Configuration Constants
Located in `src/config.h`:
```cpp
#define BUTTON_PIN 7                     // GPIO7 (D7)
#define BUTTON_PRESS_TIME_MS 3000        // Hold 3s to reboot
#define BUTTON_FACTORY_RESET_TIME_MS 8000 // Hold 8s for factory reset
#define BUTTON_DEBOUNCE_MS 50            // Debounce delay
```

LED data pin is configured in `platformio.ini`:
```ini
-DDATAPIN=D8  // GPIO8 (D8)
```

## Usage Tips

1. **Testing Connection:** Briefly press and release the button. You should see yellow LEDs flash.

2. **Safe Reboot:** If device becomes unresponsive, hold button for 3 seconds to safely reboot without losing settings.

3. **Factory Reset Safety:** The 8-second threshold prevents accidental resets. Make sure you really want to reset before holding that long!

4. **Boot-Time Factory Reset:** If you hold the button during device boot/power-on for 8+ seconds, it will also trigger a factory reset.

## Troubleshooting

**Button doesn't respond:**
- Check GPIO7 (D7) connection
- Ensure button connects to GND when pressed
- Check serial logs for "Button initialized on GPIO 7" message

**Accidental resets:**
- Increase `BUTTON_PRESS_TIME_MS` or `BUTTON_FACTORY_RESET_TIME_MS` in config.h
- Check for noisy button connections causing false triggers

**LEDs don't show feedback:**
- Button handling is working but LED feedback may be overridden by effects
- Check serial monitor for log messages like "Button pressed" and "Reboot triggered"
