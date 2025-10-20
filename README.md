# StatusGlow

**Microsoft Teams Presence Indicator for ESP32 with NeoPixel LEDs**

A self-hosted, feature-rich presence indicator that displays your Microsoft Teams status using smooth LED animations. Built for Seeed Studio XIAO ESP32 boards with a modern web interface.

![StatusGlow Banner](https://img.shields.io/badge/Platform-ESP32-blue) ![PlatformIO](https://img.shields.io/badge/Built%20with-PlatformIO-orange) ![License](https://img.shields.io/badge/License-MIT-green)

## ✨ Features


<img width="1281" height="928" alt="Screenshot 2025-10-19 205620" src="https://github.com/user-attachments/assets/7475aaea-75f9-4e52-9cdd-87d1ecade3ec" />
<img width="1280" height="636" alt="Screenshot 2025-10-19 205634" src="https://github.com/user-attachments/assets/2271db48-c16d-46fa-b4f5-e5498f2abbc5" />
<img width="1256" height="993" alt="Screenshot 2025-10-19 205646" src="https://github.com/user-attachments/assets/f87d6df4-8496-4b3d-abc3-5b28d5bc72cb" />
<img width="1265" height="817" alt="Screenshot 2025-10-19 205700" src="https://github.com/user-attachments/assets/8f7c4611-e0f4-4642-95ee-25c9c2feffef" />
<img width="1259" height="544" alt="Screenshot 2025-10-19 205710" src="https://github.com/user-attachments/assets/d21f6fa0-f0da-4930-b58e-f02b6571ffa2" />

### 🎨 LED Effects & Control
- **19 Smooth Effects**: Static, Breath, Fade, Scan, Dual Scan, Comet, Rainbow, Theater Chase, Color Wipe, Running Lights, Twinkle, Sparkle, Confetti, Fire Flicker, Filler Up, and more
- **Runtime RGB/RGBW Switching**: Change between RGB (WS2812B) and RGBW (SK6812) LED types via web UI - no recompilation needed
- **Smooth Animations**: Sub-pixel interpolation for buttery-smooth motion
- **Speed Control**: Duration slider (0-60s) with consistent behavior across all effects
- **Per-Profile Customization**: Different effects for each Teams status (Available, Busy, Away, etc.)
- **Gamma Correction**: Adjustable for accurate color reproduction
- **Brightness Control**: 0-255 with per-profile overrides

### 🔧 Hardware Control
- **Physical Button** (GPIO7):
  - Hold 3s: Reboot device
  - Hold 8s: Factory reset
  - Visual LED feedback during press
- **Configurable Pins**: LED on GPIO8, Button on GPIO7

### 🌐 Web Interface
- **Modern UI**: Clean, responsive design
- **Live Monitoring**: Real-time status, CPU/memory, WiFi signal
- **Configuration**: Teams setup, WiFi, LED type, device control
- **Effects Editor**: Visual effect customization with preview
- **Live Logs**: Real-time device logs with auto-refresh
- **OTA Updates**: Firmware updates via web UI

### 🔒 Security & Reliability
- **Optional OTA Key**: Secure firmware updates
- **Factory Reset**: Hardware button or API
- **Persistent Settings**: Survives reboots
- **Auto-Reconnect**: WiFi and Teams API resilience

## 🛠️ Hardware Requirements

### Supported Boards
- **XIAO ESP32S3** (Recommended) - Dual-core, PSRAM, smoothest performance
- **XIAO ESP32C3** - Single-core RISC-V, great performance

### Components
- ESP32 board (above)
- WS2812B (RGB) or SK6812 (RGBW) LED strip
- Momentary push button (optional but recommended)
- 5V power supply (sufficient for your LED count)
- 220-470Ω resistor (for LED data line)
- 470-1000µF capacitor (recommended for LED power)

### Wiring Diagram
```
ESP32 GPIO8 (D8) --[220Ω]--→ LED Data In
ESP32 GPIO7 (D7) → Button → GND
ESP32 GND → LED GND → Power GND (common ground)
Power 5V → LED 5V+

Optional: 74AHCT125 level shifter for 5V LEDs
```

## 🚀 Getting Started

### Prerequisites
- [PlatformIO](https://platformio.org/) installed (CLI or VS Code extension)
- USB cable for programming
- Microsoft 365 account for Teams presence

### 1. Clone and Build

```bash
# Clone the repository
git clone https://github.com/YOUR_USERNAME/StatusGlow.git
cd StatusGlow

# Build for ESP32-C3 (default)
pio run -e seeed_xiao_esp32c3

# OR build for ESP32-S3
pio run -e seeed_xiao_esp32s3

# Upload
pio run --target upload
```

### 2. Initial Setup

1. **Power On**
   - Device creates WiFi AP: `StatusGlow`
   - Password: `presence`

2. **Connect**
   - Join the StatusGlow network
   - Open `http://192.168.4.1` in browser

3. **Configure** (Config page)
   - Set WiFi credentials
   - Enter Teams Client ID & Tenant ID
   - Select LED type (RGB/RGBW)
   - Set number of LEDs
   - Save and reboot

### 3. Microsoft Teams Integration

#### Azure App Registration
1. Go to [Azure Portal](https://portal.azure.com)
2. Navigate to **Azure Active Directory** → **App registrations**
3. Click **New registration**
4. Set:
   - Name: `StatusGlow`
   - Redirect URI: `https://login.microsoftonline.com/common/oauth2/nativeclient`
5. Under **API Permissions**, add:
   - `Presence.Read` (Delegated)
   - `User.Read` (Delegated)
6. Grant admin consent
7. Copy **Application (client) ID** and **Directory (tenant) ID**

#### Device Login
1. In StatusGlow Config page, click **Start device login**
2. Copy the code displayed
3. Visit [microsoft.com/devicelogin](https://microsoft.com/devicelogin)
4. Enter code and sign in
5. Device now polls Teams presence every 30s (configurable)

## 📖 Usage Guide

### Web Interface Pages

#### Home
- Current Teams status
- System stats (CPU, memory, uptime)
- WiFi signal strength
- Quick status overview

#### Config
- Teams credentials (Client ID, Tenant)
- WiFi settings (SSID, password, AP control)
- LED type selection (RGB/RGBW)
- Number of LEDs
- **Reboot Device button**

#### Effects
- Per-status effect configuration
- Effect selection (19 available)
- Speed/duration slider (0-60s)
- Color picker
- Reverse direction toggle
- Fade duration between effects
- Global brightness
- Gamma correction
- Preview mode

#### Logs
- Real-time device logs
- Auto-refresh toggle
- Configurable log count
- Timestamp display

#### Firmware
- OTA update interface
- Upload .bin files
- Progress indicator

### Physical Button

| Hold Duration | Action | LED Feedback |
|--------------|--------|--------------|
| < 3s | Test | Yellow (increasing) |
| 3s | Reboot | Blue blinks 3× |
| 8s | Factory Reset | Red blinks 5× |

### API Endpoints

```bash
# Get system settings
GET /api/settings

# Update settings
POST /api/settings
Content-Type: application/json
{"client_id": "...", "tenant": "...", "led_type_rgbw": false}

# Get effects configuration
GET /api/effects

# Update effects
POST /api/effects
Content-Type: application/json
{"profiles": [...]}

# Reboot device
POST /api/reboot

# Get logs
GET /api/logs?n=50
```

## ⚙️ Configuration

### platformio.ini
```ini
[env:seeed_xiao_esp32c3]
build_flags =
    -DNUMLEDS=16        ; Number of LEDs
    -DDATAPIN=D8        ; LED data pin
```

### src/config.h
```cpp
#define BUTTON_PIN 7                          // Button GPIO
#define BUTTON_PRESS_TIME_MS 3000             // Reboot hold time
#define BUTTON_FACTORY_RESET_TIME_MS 8000     // Factory reset time
#define DEFAULT_LED_TYPE_RGBW false           // RGB (false) or RGBW (true)
#define OTA_SHARED_KEY ""                     // OTA security key (optional)
```

## 📚 Additional Documentation

- [Button Usage Guide](BUTTON_USAGE.md) - Physical button functions and troubleshooting
- [LED Type Switching](LED_TYPE_SWITCHING.md) - Runtime RGB/RGBW configuration

## 🔧 Troubleshooting

### LEDs don't light up
- Check wiring (common ground, data pin, power)
- Verify LED type setting matches your hardware (RGB vs RGBW)
- Try lower LED count first (test with 1-5 LEDs)
- Check power supply capacity

### WiFi won't connect
- Verify SSID and password in Config
- Check 2.4GHz network (ESP32 doesn't support 5GHz)
- Try connecting to StatusGlow AP and reconfigure

### Teams status not updating
- Verify Client ID and Tenant ID correct
- Complete device login process
- Check internet connection
- View Logs page for errors

### Button not responding
- Check GPIO7 to GND connection
- Verify button wiring (momentary push button)
- Check serial logs for "Button pressed" messages

### Factory Reset
If device becomes unresponsive:
1. Hold button for 8+ seconds (LEDs show orange then red)
2. OR use hardware reset button
3. Device will clear all settings and restart as AP

## 🤝 Contributing

Contributions are welcome! Please feel free to submit a Pull Request.

1. Fork the repository
2. Create your feature branch (`git checkout -b feature/AmazingFeature`)
3. Commit your changes (`git commit -m 'Add some AmazingFeature'`)
4. Push to the branch (`git push origin feature/AmazingFeature`)
5. Open a Pull Request

## 📝 License

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.

## 🙏 Acknowledgments

- Built with [PlatformIO](https://platformio.org/)
- Uses [Adafruit NeoPixel](https://github.com/adafruit/Adafruit_NeoPixel) library
- Uses [ArduinoJson](https://arduinojson.org/) library
- Designed for [Seeed Studio XIAO ESP32](https://www.seeedstudio.com/XIAO-ESP32C3-p-5431.html) boards

## 📧 Support

For issues, questions, or suggestions:
- Open an [Issue](https://github.com/YOUR_USERNAME/StatusGlow/issues)
- Check existing documentation
- Review logs in web interface

---

**Made with ❤️ for remote workers**
