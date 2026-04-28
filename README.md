# StatusGlow

StatusGlow is an ESP32-based Microsoft Teams presence light with a built-in web UI for setup, effects, logs, and OTA updates.

It is set up for:
- Seeed Studio XIAO ESP32S3 by default
- Seeed Studio XIAO ESP32C3 and Waveshare ESP32-S3-Zero as additional PlatformIO environments
- WS2812B RGB or SK6812 RGBW LED strips

## What It Does

- Polls Microsoft Teams presence with device login
- Can instead poll a central relay service for fleet deployments
- Displays status with configurable LED effects and colors
- Plays a short startup light sequence before handing off to live status
- Lets you change RGB/RGBW mode at runtime
- Hosts a local web UI for config, effects, logs, and single-image OTA updates
- Falls back to a local AP when Wi-Fi is not configured
- Stores Wi-Fi, app settings, effects, Microsoft auth context, and OTA history in ESP32 NVS
- Supports an onboard status LED on ESP32-S3 boards that have one on GPIO21
- Embeds the web UI directly into the firmware image, so there is only one file to flash

## Default Hardware Settings

- LED data pin: `D10` on the default XIAO ESP32S3 build
- LED count: `16`
- Default board/environment: `seeed_xiao_esp32s3`
- Status LED pin: `GPIO21` on supported ESP32-S3 boards

These defaults come from [platformio.ini](platformio.ini) and can be changed there.

## Build And Flash

Install PlatformIO, then from the project root:

```bash
pio run -e seeed_xiao_esp32s3
pio run -e seeed_xiao_esp32s3 -t upload
```

Other environments:

```bash
pio run -e seeed_xiao_esp32c3 -t upload
pio run -e waveshare_esp32s3_zero -t upload
```

The web UI assets from `data/` are embedded into the firmware at build time, so `upload` flashes everything in one image.

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

## Enterprise Relay Mode

For fleet deployments, each device can now switch from `Direct Microsoft Login` to `Central Relay Service` on the Config page.

In relay mode:

- The Docker container owns Microsoft Graph access
- Each StatusGlow device only needs:
  - Relay base URL
  - Relay device ID
  - Relay device key
- Multiple devices can mirror the same user without repeating Microsoft sign-in on each controller

### Relay Container

Files added for the relay:

- [relay/server.js](relay/server.js): central presence poller and device API
- [relay/config.example.json](relay/config.example.json): sample tenant/device mapping
- [relay/Dockerfile](relay/Dockerfile): container image
- [docker-compose.relay.yml](docker-compose.relay.yml): simple local deployment

Example startup:

```bash
cp relay/config.example.json relay/config.json
docker compose -f docker-compose.relay.yml up --build -d
```

The relay serves:

- `GET /health`
- `GET /api/v1/devices/{deviceId}/presence`

Devices authenticate with the `X-StatusGlow-Device-Key` header.

### Relay Config

`relay/config.json` is a simple mapping file:

```json
{
  "tenantId": "your-tenant-id",
  "clientId": "your-app-client-id",
  "clientSecret": "your-app-client-secret",
  "pollIntervalSeconds": 30,
  "listenPort": 8787,
  "devices": [
    {
      "deviceId": "desk-alice-01",
      "apiKey": "long-random-shared-secret",
      "userId": "entra-user-object-id",
      "displayName": "Alice"
    }
  ]
}
```

Each device entry maps one physical controller to one Microsoft Entra user object ID. If you want multiple lights to follow the same person, repeat the same `userId` with different `deviceId` values.

### Relay Permissions

The relay uses Microsoft Graph app-only access, so the app registration should be granted:

- `Presence.Read.All` application permission

Direct device login mode still uses the existing delegated setup on the device itself.

## Web UI Pages

- `Home`: current status, uptime, memory, Wi-Fi, and version
- `Config`: Wi-Fi, direct-vs-relay presence settings, LED type, status LED, reboot, factory reset
- `Effects`: single-status editor with live preview, mirrored strip preview, brightness, gamma, fade, and LED count
- `Logs`: recent device logs
- `Firmware`: OTA upload and last OTA log

## Main Config Files

- [platformio.ini](platformio.ini): board environments, LED pin, LED count
- [src/config.h](src/config.h): runtime defaults, AP password, optional web UI / OTA key, status LED settings
- [src/main.cpp](src/main.cpp): firmware logic and API routes
- [src/request_handler.h](src/request_handler.h): API helpers and Microsoft device-login handlers
- [relay/server.js](relay/server.js): Docker-friendly central presence relay for fleet deployments
- `data/`: source web UI assets (`index.html`, `setup.html`, `app.css`, `app.js`) that are embedded into the firmware during build
- [scripts/embed_assets.py](scripts/embed_assets.py): build-time asset packer for the embedded web UI

## Common Tasks

Change LED count or pin:

- Edit `NUMLEDS` and `DATAPIN` in `platformio.ini`

Switch between RGB and RGBW:

- Use the Config page in the web UI

Upload firmware OTA:

- Open the Firmware page
- Upload the `.bin`
- The uploaded firmware image already contains the web UI and setup portal

Factory reset:

- Use the Config page danger area
- This clears saved Wi-Fi, app settings, effects, and auth context, then reboots

Preview a status effect:

- Open the Effects page and choose a status to edit
- Changes preview live on the strip and in the mirrored strip preview
- Leaving the Effects page automatically returns the LEDs to normal live Teams status

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

Relay mode does not update:

- Verify the device is set to `Central Relay Service`
- Verify `relay/config.json` contains the device ID and matching API key
- Confirm the mapped `userId` is the Microsoft Entra object ID for the target user
- Open the relay `/health` endpoint and check `lastPollError`

Assets like logo/favicon do not appear:

- Rebuild and upload the firmware image so the embedded assets are refreshed

## License

MIT. See [LICENSE](LICENSE).
