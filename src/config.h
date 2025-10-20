#pragma once

// Global firmware configuration constants

// Version
#define VERSION "0.7 Beta 3"

// Networking and timing
#define DEFAULT_POLLING_PRESENCE_INTERVAL "30"   // Polling interval (seconds) as string
#define DEFAULT_ERROR_RETRY_INTERVAL 30           // Retry delay after errors (seconds)
#define TOKEN_REFRESH_TIMEOUT 60                   // Refresh token this many seconds before expiry
#define WIFI_STA_CONNECT_TIMEOUT_MS 15000         // Wi‑Fi STA connect timeout (ms)

// LED/effects defaults
#define DEFAULT_FADE_MS 800                       // fade time between effects
#define APP_DEFAULT_BRIGHTNESS 128                // 0-255 default brightness
#define DEFAULT_GAMMA 2.2f                        // gamma correction factor
#define DEFAULT_LED_TYPE_RGBW false               // Default: RGB (false), RGBW (true)

// Filesystem paths
#define CONTEXT_FILE "/context.json"
#define EFFECTS_FILE "/effects.json"
#define CONFIG_FILE "/config.json"

// Device/AP name
#define THING_NAME "StatusGlow"
#define WIFI_INITIAL_AP_PASSWORD "presence"

// UI defaults (frontend can override using values provided by /api/settings)
#define UI_CPU_OK 50                 // percent below this is OK
#define UI_CPU_WARN 80               // percent below this is WARN, else CRIT
#define UI_HEAP_OK 60                // used percent below this is OK
#define UI_HEAP_WARN 85              // used percent below this is WARN, else CRIT
#define UI_MIN_REFRESH_MS 2000       // Minimum refresh cadence for live UI (ms)
// Wi‑Fi RSSI thresholds (dBm) for 1..4 bars
#define UI_RSSI_B0 -85
#define UI_RSSI_B1 -75
#define UI_RSSI_B2 -65
#define UI_RSSI_B3 -55
// Wi‑Fi bars heights (px) for 1..4 bars
#define UI_BAR_H0 6
#define UI_BAR_H1 10
#define UI_BAR_H2 14
#define UI_BAR_H3 18

// OTA security
// If non-empty, accessing /fw and /update requires providing this key either as
// HTTP header 'X-OTA-Key' or as query/form parameter 'key'.
#define OTA_SHARED_KEY ""

// Button/Touch configuration
#define BUTTON_PIN 7                     // GPIO7 (D7) - safe pin on both C3 and S3
#define BUTTON_PRESS_TIME_MS 3000        // Hold for 3 seconds to reboot
#define BUTTON_FACTORY_RESET_TIME_MS 8000 // Hold for 8 seconds for factory reset
#define BUTTON_DEBOUNCE_MS 50            // Debounce delay

