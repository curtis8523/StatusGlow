# Runtime RGB/RGBW LED Type Switching

## Overview
Your StatusGlow device now supports switching between RGB (3-color) and RGBW (4-color) LED strips without recompiling the firmware. You can change the LED type from the web interface and reboot to apply the changes.

## LED Types Supported

### RGB (3-color) - Default
- **Channels:** Red, Green, Blue
- **Common Models:** WS2812B, WS2811, SK6812 (RGB variant)
- **Use Case:** Most standard NeoPixel strips

### RGBW (4-color)
- **Channels:** Red, Green, Blue, White
- **Common Models:** SK6812 RGBW, WS2814
- **Use Case:** Strips with dedicated white LEDs for better whites and pastels

## How to Switch LED Types

### Via Web Interface
1. Navigate to the **Config** page in your StatusGlow web interface
2. Find the **LED Type** dropdown
3. Select your LED type:
   - **RGB (3-color)** - For standard WS2812B and similar
   - **RGBW (4-color)** - For SK6812 RGBW and similar
4. Click **Save**
5. You'll see an alert: "LED type changed. Please reboot the device for changes to take effect."
6. Click the **Reboot Device** button (red button at the bottom of the Config page)
7. Confirm the reboot when prompted
8. Wait 10 seconds for the device to restart

### Via API
```bash
# Check current LED type
curl http://192.168.x.x/api/settings

# Switch to RGBW
curl -X POST http://192.168.x.x/api/settings \
  -H "Content-Type: application/json" \
  -d '{"led_type_rgbw": true}'

# Switch to RGB
curl -X POST http://192.168.x.x/api/settings \
  -H "Content-Type: application/json" \
  -d '{"led_type_rgbw": false}'

# Reboot required for changes to take effect
```

## Technical Details

### How It Works
1. **Compile-time Support:** Firmware is compiled to support both RGB and RGBW formats
2. **Runtime Selection:** The `setPixelType()` method updates the NeoPixel library configuration
3. **Persistence:** LED type setting is saved to `/config.json` on SPIFFS
4. **Boot-time Application:** Setting is loaded and applied during device startup

### Configuration Storage
The LED type is stored in the config file:
```json
{
  "system": {
    "led_type_rgbw": false,
    ...
  }
}
```

### Default Setting
- **Default:** RGB (3-color)
- Defined in `src/config.h` as `DEFAULT_LED_TYPE_RGBW false`

## Important Notes

### Reboot Required
**The LED type change ONLY takes effect after rebooting the device.** The web interface will alert you when a reboot is needed.

### Visual Differences
- **RGB strips** with RGBW mode enabled: White channel is ignored, may appear dimmer
- **RGBW strips** with RGB mode enabled: White LEDs won't be used, colors may look off

### Testing Your Configuration
After changing the LED type and rebooting:
1. Navigate to the **Effects** page
2. Test with the "Static" effect using white color
3. RGBW strips should show pure white; RGB strips show RGB-mixed white

## Troubleshooting

**Colors look wrong after switch:**
- Verify you selected the correct LED type for your hardware
- Power cycle the device completely (not just software reboot)
- Check physical LED strip model number

**Setting doesn't persist:**
- Ensure SPIFFS is mounted correctly (check serial logs)
- Try factory reset if config file is corrupted

**API returns needs_reboot: true:**
- This is normal! The LED type has been saved but won't apply until reboot
- Use the button (hold 3 seconds) or web interface to reboot

## Files Modified
- `src/config.h` - Added `DEFAULT_LED_TYPE_RGBW` constant
- `src/led_effects.h` - Added `setPixelType()` and `getPixelTypeRGBW()` methods
- `src/main.cpp` - Added `gLedTypeRGBW` global variable, persistence logic, API support
- `src/request_handler.h` - Added LED Type dropdown to Config page, updated JavaScript

## Benefits
- **Single Firmware:** One firmware image works with both RGB and RGBW strips
- **Easy Switching:** No need to recompile when changing hardware
- **Persistent:** Setting survives reboots and power cycles
- **API Access:** Can be changed programmatically

## Migration from Previous Versions
If upgrading from a firmware that used compile-time `RGBW_STRIP` flag:
1. Your existing strips will work with default RGB mode
2. If you had RGBW strips, change the setting in web UI and reboot
3. Old `effects.json` files are compatible (LED type is separate setting)
