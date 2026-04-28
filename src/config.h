#pragma once

// Global firmware configuration constants

// Version
#define VERSION "1.1.0"

// Networking and timing
#define DEFAULT_POLLING_PRESENCE_INTERVAL "30"   // Polling interval (seconds) as string
#define DEFAULT_ERROR_RETRY_INTERVAL 30          // Retry delay after errors (seconds)
#define TOKEN_REFRESH_TIMEOUT 60                 // Refresh token this many seconds before expiry
#define WIFI_STA_CONNECT_TIMEOUT_MS 15000        // Wi-Fi STA connect timeout (ms)
#define DEFAULT_PRESENCE_SOURCE "direct"         // direct=device login on each device, relay=central service
#define DEFAULT_RELAY_TLS_INSECURE false         // allow self-signed HTTPS relay certs when explicitly enabled

// LED/effects defaults
#define DEFAULT_FADE_MS 800         // fade time between effects
#define APP_DEFAULT_BRIGHTNESS 128  // 0-255 default brightness
#define DEFAULT_GAMMA 2.2f          // gamma correction factor
#define DEFAULT_LED_TYPE_RGBW false // Default: RGB (false), RGBW (true)

// Device/AP name
#define THING_NAME "StatusGlow"
#define WIFI_INITIAL_AP_PASSWORD_PREFIX "statusglow"

// UI defaults (frontend can override using values provided by /api/settings)
#define UI_CPU_OK 50           // percent below this is OK
#define UI_CPU_WARN 80         // percent below this is WARN, else CRIT
#define UI_HEAP_OK 60          // used percent below this is OK
#define UI_HEAP_WARN 85        // used percent below this is WARN, else CRIT
#define UI_MIN_REFRESH_MS 2000 // Minimum refresh cadence for live UI (ms)
// Wi-Fi RSSI thresholds (dBm) for 1..4 bars
#define UI_RSSI_B0 -85
#define UI_RSSI_B1 -75
#define UI_RSSI_B2 -65
#define UI_RSSI_B3 -55
// Wi-Fi bars heights (px) for 1..4 bars
#define UI_BAR_H0 6
#define UI_BAR_H1 10
#define UI_BAR_H2 14
#define UI_BAR_H3 18

// UI/admin security
// Protected UI pages and management APIs use this key via either:
// - HTTP header 'X-StatusGlow-Key' or legacy 'X-OTA-Key'
// - query/form parameter 'key'
// Leave blank to disable auth for the local web UI and management APIs.
#define ADMIN_SHARED_KEY ""

// OTA security
// Leave blank to match the admin key behavior above.
#define OTA_SHARED_KEY ADMIN_SHARED_KEY

// Status LED configuration (onboard WS2812 on S3 board)
#define STATUS_LED_PIN 21                // GPIO21 on Waveshare ESP32-S3-Zero (onboard WS2812)
#define DEFAULT_STATUS_LED_ENABLED false // Default: disabled

// Render cadence for the NeoPixel task. Target ~120 FPS on ESP32-S3.
#if defined(CONFIG_IDF_TARGET_ESP32S3)
#define LED_FRAME_DELAY_MS 6  // ~125 FPS (closest integer ms to 120 FPS)
#else
#define LED_FRAME_DELAY_MS 12 // ~83 FPS on other targets by default
#endif
