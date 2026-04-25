# Reddit Post Templates for StatusGlow

## For r/esp32

**Title:** [Project] StatusGlow - Microsoft Teams Presence Indicator with 19 LED Effects for ESP32

**Body:**

Hey r/esp32! I just open-sourced my Microsoft Teams presence indicator project that I've been working on.

**StatusGlow** displays your Teams status using NeoPixel LEDs with smooth, customizable animations. Built for XIAO ESP32C3/S3 boards.

### Key Features:
- 🎨 **19 LED effects** - Static, Breath, Fade, Rainbow, Comet, Fire Flicker, and more
- 🔄 **Runtime RGB/RGBW switching** - Change LED type via web UI without recompiling
- 🔘 **Physical button control** - Hold 3s to reboot, 8s for factory reset
- 🌐 **Modern web interface** - Live status, effect editor, OTA updates
- ⚡ **Smooth animations** - Sub-pixel interpolation for buttery motion
- 📱 **Teams integration** - OAuth device flow for secure authentication

### Hardware:
- XIAO ESP32C3 or S3
- WS2812B (RGB) or SK6812 (RGBW) LED strip
- Optional: Push button for hardware control
- GPIO8 for LEDs, GPIO7 for button

### What Makes It Different:
Most presence indicators I've seen are pretty basic. I wanted something that:
- Has really smooth LED effects (no stuttering)
- Can switch between RGB and RGBW strips without reflashing
- Has a proper web UI for configuration
- Supports physical button control for troubleshooting
- Per-status effect customization (different animation for Available vs Busy)

### Tech Stack:
- PlatformIO
- Adafruit NeoPixel library
- ArduinoJson
- Native ESP32 WebServer
- Microsoft Graph API

The firmware is about 85% of flash on ESP32C3. Runs great on both C3 (single-core) and S3 (dual-core with PSRAM).

**GitHub:** https://github.com/curtis8523/StatusGlow

Would love feedback from the community! Happy to answer any questions about the implementation.

---

## For r/homeautomation

**Title:** [Project] Open-sourced my Microsoft Teams Presence Indicator (ESP32 + NeoPixels)

**Body:**

I just released **StatusGlow** - an open-source Teams presence indicator that's been sitting on my desk for the past few months. Finally cleaned it up and put it on GitHub!

### What It Does:
Displays your Microsoft Teams status using LED animations:
- 🟢 Available - Static green (or custom effect)
- 🔴 Busy - Red pulse
- 🟡 Away - Yellow fade
- 🟣 Do Not Disturb - Purple
- And all other Teams statuses

### Why I Built This:
Working from home, my family kept interrupting me during meetings. Needed a visual indicator that's easy to see from across the room. Plus, I wanted an excuse to play with smooth LED animations!

### Cool Features:
- **19 LED effects** with smooth sub-pixel animations
- **Web-based configuration** - no code changes needed
- **Physical button** - hold 3s to reboot, 8s for factory reset
- **OTA updates** - upload new firmware from the web UI
- **Runtime LED type switching** - change RGB ↔ RGBW without reflashing
- **Per-status customization** - different effects for each Teams status

### Hardware Needed:
- ESP32 board (XIAO C3 or S3) - ~$5-10
- WS2812B LED strip - ~$5-15
- Optional button - ~$0.50
- Total cost: Under $20

### Setup Time:
- Hardware assembly: 15 minutes
- Azure app registration: 10 minutes
- Configuration: 5 minutes

**GitHub:** https://github.com/curtis8523/StatusGlow

The documentation includes full wiring diagrams, Azure setup guide, and troubleshooting. MIT licensed so use it however you want!

---

## For r/MicrosoftTeams

**Title:** [DIY] Built an Open-Source Physical Presence Indicator for Teams (With LED Effects!)

**Body:**

Hey Teams users! I made a physical presence indicator that sits on my desk and shows my Teams status using LED animations.

**StatusGlow** is an ESP32-based device that polls the Microsoft Graph API and displays your presence with customizable LED effects.

### Why This Exists:
My partner and kids kept walking in during calls. Needed something visible from the hallway that shows "I'm busy, don't interrupt!"

### What It Shows:
- 🟢 Available
- 🔴 Busy/In a call  
- 🟡 Away/Be Right Back
- 🟣 Do Not Disturb
- And all other Teams statuses

### Features That Might Interest You:
- **Different effects per status** - Static green when available, pulsing red when busy, etc.
- **Web interface** - Configure everything from your browser
- **Secure OAuth** - Uses Microsoft's device code flow (same as Azure CLI)
- **No cloud dependency** - Everything runs locally on your network
- **Open source** - MIT licensed, customize however you want

### How It Works:
1. Register an Azure app (one-time, 10 minutes)
2. Flash the firmware to ESP32
3. Connect to LED strip
4. Device login with your Microsoft account
5. It polls your presence every 30 seconds

### Cost:
- ESP32 board: ~$7
- LED strip: ~$10  
- Optional button: ~$0.50
- **Total: Under $20**

No subscription, no cloud service, just a one-time setup.

### For the Technically Inclined:
- Built with PlatformIO
- Supports both RGB and RGBW LED strips
- 19 different LED effects
- Physical button for reboot/factory reset
- OTA firmware updates
- Live system monitoring

**GitHub:** https://github.com/curtis8523/StatusGlow

Full documentation included. Happy to answer questions about setup or customization!

---

## For r/Arduino (or r/embedded)

**Title:** [Project Showcase] StatusGlow - Teams Presence Indicator with Smooth NeoPixel Effects (ESP32)

**Body:**

Sharing a project I've been polishing: **StatusGlow** - a Microsoft Teams presence indicator with focus on smooth LED animations.

### Technical Highlights:

**Effects Engine:**
- 19 effects with sub-pixel interpolation
- Mutex-protected double-buffering (no mid-frame glitches)
- FreeRTOS task on core 1 (ESP32S3) for smooth 80+ FPS
- Gamma-corrected color blending
- Runtime RGB/RGBW switching via `updateType()`

**Architecture:**
- Native ESP32 WebServer (no external dependencies)
- Unified JSON config in SPIFFS
- Microsoft Graph OAuth device code flow
- OTA updates with optional shared key auth
- Ring buffer logging system

**Performance:**
- ESP32C3 (single-core): ~81% CPU idle during heavy effects
- ESP32S3 (dual-core): ~95% CPU idle, LED task isolated to core 1
- 85% flash usage, 19% RAM

**Hardware Interface:**
- GPIO7: Physical button with debouncing
- GPIO8: NeoPixel data with 220Ω series resistor
- Hold detection: 3s reboot, 8s factory reset
- Visual LED feedback during button press

**Code Quality:**
- PlatformIO build system
- Compile-time feature flags
- Persistent configuration with migration
- Comprehensive error handling and logging

### Interesting Implementation Details:

**Smooth Fades:**
```cpp
// Sine-based easing instead of linear
float f = (sinf((t * 2.0f - 1.0f) * 1.5708f) + 1.0f) * 0.5f;
```

**Sub-pixel Scanning:**
```cpp
// Fractional position for buttery-smooth comet tail
float pos = fmodf(offset, (float)n);
int i0 = (int)floorf(pos);
float frac = pos - (float)i0;
```

**Runtime LED Type:**
```cpp
// Switch RGB ↔ RGBW without recompilation
effects.setPixelType(isRGBW);
strip.updateType(isRGBW ? NEO_GRBW : NEO_GRB);
```

### Repo:
https://github.com/curtis8523/StatusGlow

Includes full API documentation, wiring diagrams, and effect customization guide. MIT licensed.

Open to code review feedback and optimization suggestions!

---

## For r/homelab

**Title:** [Lab Accessory] StatusGlow - Self-Hosted Teams Presence Indicator (ESP32)

**Body:**

Added a new piece to the homelab setup: **StatusGlow** - a self-hosted Microsoft Teams presence indicator.

### Why This Fits in a Homelab:
- **Self-hosted** - No cloud service, runs on your network
- **API-driven** - Polls Microsoft Graph every 30s
- **Monitoring-friendly** - JSON API for status, logs, metrics
- **OTA updates** - Flash new firmware from web UI
- **Low power** - ESP32 at ~0.5W, perfect for 24/7 operation

### What It Does:
Sits on my desk and shows Teams status via LED strip:
- Green = Available
- Red = In a meeting
- Yellow = Away
- Purple = DND

Helps family know when not to interrupt. Also doubles as ambient lighting with 19 different LED effects.

### Homelab Integration Potential:
- Could feed status to Home Assistant via REST API
- Trigger automations based on presence
- Log availability metrics
- Control via API (change effects, brightness, etc.)

### Tech Stack:
- ESP32C3/S3 microcontroller
- WS2812B RGB LEDs
- PlatformIO firmware
- Native web server + REST API
- SPIFFS for config storage
- Microsoft Graph for presence data

### Features:
- Web UI for configuration
- Physical button (reboot/factory reset)
- Live logs and metrics
- Runtime RGB/RGBW LED switching
- Per-status effect customization

### Cost:
~$15 in parts (ESP32 + LED strip)

**GitHub:** https://github.com/curtis8523/StatusGlow

Perfect for anyone wanting a physical presence indicator without relying on cloud services. Everything stays on your network.

---

## Posting Tips

### Best Subreddits (in order of relevance):
1. **r/esp32** - Most relevant, technical audience
2. **r/homeautomation** - Practical use case focused
3. **r/MicrosoftTeams** - End-user focused
4. **r/Arduino** - Electronics community
5. **r/homelab** - Self-hosting angle

### Posting Strategy:
- **Start with r/esp32** - Wait 1-2 days for feedback
- **Then r/homeautomation** - Different audience, different angle
- **Space out posts** - Don't spam all subreddits at once
- **Engage with comments** - Reply to questions quickly

### What to Include:
- ✅ Clear, descriptive title
- ✅ What problem it solves
- ✅ Key features (bullet points)
- ✅ Photos/GIF if you have them
- ✅ GitHub link
- ✅ Cost estimate
- ✅ Offer to answer questions

### Optional Additions:
- Photo of your setup
- Short video/GIF of the LED effects
- Wiring diagram screenshot
- Web UI screenshot

### Avoid:
- ❌ All-caps titles
- ❌ Posting to all subreddits at once
- ❌ Over-selling ("AMAZING", "BEST EVER")
- ❌ Walls of text (use formatting!)

Good luck with your launch! 🚀
