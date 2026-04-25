# Status LED Feature

## Overview

The StatusGlow firmware supports an onboard WS2812 RGB status LED on the **XIAO ESP32S3 board only** (GPIO21). This LED provides visual feedback about the device's current operational state.

## Hardware

- **Board**: Seeed XIAO ESP32S3
- **Pin**: GPIO21
- **LED Type**: WS2812 RGB (onboard)
- **Note**: The ESP32C3 board does NOT have this onboard LED

## LED Status Indicators

The status LED displays different colors and patterns based on the device state:

| State | Color/Pattern | Meaning |
|-------|--------------|---------|
| **Initial/WiFi Connecting** | Blinking Yellow (500ms) | Device starting up or connecting to WiFi |
| **WiFi Connected** | Cyan (solid) | WiFi connected, waiting for authentication |
| **Device Login Started** | Cyan (solid) | Waiting for user to complete device login |
| **Login Failed** | Blinking Red (300ms) | Authentication failed |
| **Authenticated** | Green (solid) | Successfully authenticated and polling presence |
| **Token Refresh** | Blinking Cyan (700ms) | Refreshing authentication token |
| **Presence Error** | Orange (solid) | Error while fetching presence data |
| **Unknown State** | Magenta (dim) | Unknown/unexpected state |

## Configuration

### Enable/Disable Status LED

The status LED can be toggled on or off through the web interface:

1. Navigate to the **Config** page
2. Find the checkbox: **"Enable Status LED (S3 only, GPIO21)"**
3. Check to enable, uncheck to disable
4. Click **Save** to apply changes

The setting is saved to configuration and persists across reboots.

### Default Setting

- **Default**: Disabled (`false`)
- You can change the default in `src/config.h`:
  ```cpp
  #define DEFAULT_STATUS_LED_ENABLED false
  ```

## API

### GET /api/settings

Returns the current status LED setting:

```json
{
  "status_led_enabled": true
}
```

### POST /api/settings

Update the status LED setting:

```json
{
  "status_led_enabled": true
}
```

## Technical Details

- The status LED uses a separate `Adafruit_NeoPixel` instance from the main LED strip
- Update rate: Called every loop iteration (typically ~1000 times per second)
- Brightness: Colors are intentionally dimmed (e.g., `0x202000` instead of `0xFFFF00`) to avoid being too bright
- No reboot required when toggling the status LED setting

## Troubleshooting

### Status LED not working

1. **Check board type**: Verify you're using an ESP32S3 board (C3 doesn't have onboard LED)
2. **Check setting**: Ensure "Enable Status LED" is checked in Config page
3. **GPIO conflict**: If using GPIO21 for other purposes, disable status LED
4. **Build for S3**: Ensure you're building for `seeed_xiao_esp32s3` environment

### Status LED shows wrong color

- If the LED shows unexpected colors, verify the device state in the web UI
- Check the logs page for state machine information

## Implementation Notes

- The status LED function (`updateStatusLed()`) is called in the main loop
- When disabled, the LED is turned off completely
- The implementation uses static variables for blink timing to avoid blocking
- Colors are deliberately dim to avoid distraction
