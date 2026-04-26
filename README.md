# StatusGlow

StatusGlow is an ESP32-based Microsoft Teams presence light with a built-in web UI for setup, effects, logs, and OTA updates.

It is set up for:
- Seeed Studio XIAO ESP32S3 by default
- Seeed Studio XIAO ESP32C3 and Waveshare ESP32-S3-Zero as additional PlatformIO environments
- WS2812B RGB or SK6812 RGBW LED strips

## What It Does

- Polls Microsoft Teams presence with device login
- Displays status with configurable LED effects and colors
- Lets you change RGB/RGBW mode at runtime
- Hosts a local web UI for config, effects, logs, and firmware upload
- Falls back to a local AP when Wi-Fi is not configured
- Stores Wi-Fi, app settings, effects, and Microsoft auth context in ESP32 NVS so they survive normal flashes and `uploadfs`
- Supports an onboard status LED on ESP32-S3 boards that have one on GPIO21

## Default Hardware Settings

- LED data pin: `D8`
- LED count: `16`
- Default board/environment: `seeed_xiao_esp32s3`
- Status LED pin: `GPIO21` on supported ESP32-S3 boards

These defaults come from [platformio.ini](platformio.ini) and can be changed there.

## Build And Flash

Install PlatformIO, then from the project root:

```bash
pio run -e seeed_xiao_esp32s3
pio run -e seeed_xiao_esp32s3 -t upload
pio run -e seeed_xiao_esp32s3 -t uploadfs
```

Other environments:

```bash
pio run -e seeed_xiao_esp32c3 -t upload
pio run -e waveshare_esp32s3_zero -t upload
```

`uploadfs` pushes the static web UI and assets from `data/`.

Use both `upload` and `uploadfs` after frontend changes. For firmware-only changes, `upload` is enough.

## First-Time Setup

1. Flash firmware.
2. Connect to the device AP.
3. The captive portal should open the Wi-Fi setup page automatically. If it does not, open `http://192.168.4.1/setup`.
4. Enter Wi-Fi credentials on the setup page and connect the device to your normal network.
5. Open the full web UI on the device LAN IP or `.local` hostname.
6. Enter Microsoft app details, start device login, and complete the sign-in flow.
7. Save settings and reboot if prompted.

### Access Point

If the device does not have working Wi-Fi, it starts its own AP:

- SSID: `StatusGlow-XXXXXX`
- Password: `statusglow-xxxxxx`
- IP: `http://192.168.4.1`

When you join that AP, the device also runs a captive portal and redirects common phone/laptop Wi-Fi setup checks to a dedicated Wi-Fi setup page.

`XXXXXX` is the last three octets of the Wi-Fi MAC address in hex. The same suffix is also added to the device hostname, so the device will show up with a matching name like `statusglow-fc4a58.local`.

## Web UI Security

By default, the local web UI and management APIs do not require a key. Open the device directly at:

```text
http://192.168.4.1
```

If you want to add protection later, set `ADMIN_SHARED_KEY` and optionally `OTA_SHARED_KEY` in [src/config.h](src/config.h).

In fallback AP mode, the device serves a dedicated Wi-Fi setup portal first. Once it is connected to normal Wi-Fi, the full admin UI is available from the device IP or `.local` hostname.

## Microsoft Teams Setup

Create an app registration in Azure / Microsoft Entra and use device login.

Recommended app permissions:

- `Presence.Read`
- `User.Read`

In the web UI Config page, provide:

- Client ID
- Tenant ID

Then click `Start device login`, open the Microsoft device login page, and complete sign-in.

## Web UI Pages

- `Home`: current status, uptime, memory, Wi-Fi, and version
- `Config`: Wi-Fi, Teams login settings, LED type, status LED, reboot, factory reset
- `Effects`: per-status effect settings, brightness, gamma, fade, LED count, preview mode
- `Logs`: recent device logs
- `Firmware`: OTA upload and last OTA log

## Main Config Files

- [platformio.ini](platformio.ini): board environments, LED pin, LED count
- [src/config.h](src/config.h): runtime defaults, AP password, optional web UI / OTA key, status LED settings
- [src/main.cpp](src/main.cpp): firmware logic and API routes
- [src/request_handler.h](src/request_handler.h): API helpers and Microsoft device-login handlers
- [src/spiffs_webserver.h](src/spiffs_webserver.h): SPIFFS/static-file helpers and upload endpoints
- `data/`: static web UI (`index.html`, `setup.html`, `app.css`, `app.js`) and assets

## Common Tasks

Change LED count or pin:

- Edit `NUMLEDS` and `DATAPIN` in `platformio.ini`

Switch between RGB and RGBW:

- Use the Config page in the web UI

Upload firmware OTA:

- Open the Firmware page
- Upload the `.bin`

Recover the web UI after changing frontend files:

- Run `pio run -e <env> -t uploadfs`

Factory reset:

- Use the Config page danger area
- This clears saved Wi-Fi, app settings, effects, and auth context, then reboots

## Troubleshooting

LEDs do not respond:

- Verify power, ground, and data wiring
- Confirm the correct `DATAPIN`
- Confirm RGB vs RGBW matches your strip
- Try a small LED count first

Cannot reach the UI:

- Join the device AP and use `http://192.168.4.1`
- If on normal Wi-Fi, find the device IP from serial output, the setup page, or your router
- Try the `.local` hostname shown by the device, such as `http://statusglow-fc4a58.local/`

Teams status does not update:

- Recheck Client ID and Tenant ID
- Complete device login again
- Check the Logs page

Assets like logo/favicon do not appear:

- Run `pio run -e <env> -t uploadfs`

## License

MIT. See [LICENSE](LICENSE).
