#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ESPmDNS.h>
#include <Update.h>
#include <ArduinoJson.h>
#include <Adafruit_NeoPixel.h>
#if defined(CONFIG_IDF_TARGET_ESP32S3)
#include "driver/gpio.h"
#endif
#include <EEPROM.h>
#include "FS.h"
#include "SPIFFS.h"
#include <time.h>
#include <math.h>
#include <stdarg.h>
#include "esp_freertos_hooks.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "config.h"
#include "led_effects.h"
#ifndef VERBOSE_LOG
#define VERBOSE_LOG 0
#endif
#if VERBOSE_LOG
#define DBG_PRINT(x) Serial.print(x)
#define DBG_PRINTLN(x) Serial.println(x)
#else
#define DBG_PRINT(x) do{}while(0)
#define DBG_PRINTLN(x) do{}while(0)
#endif

// Device identity and first-time access credentials
String gThingName;
String gThingHostName;
String gDeviceSuffixUpper;
String gDeviceSuffixLower;
String gWifiInitialApPassword;
String gAdminSharedKey;
String gOtaSharedKey;

// Available if we later add a DNS-based captive portal
DNSServer dnsServer;
WebServer server(80);

// App parameters (persisted in unified /config.json)
#define STRING_LEN 64
#define INTEGER_LEN 16
char paramClientIdValue[STRING_LEN];
char paramTenantValue[STRING_LEN];
char paramPollIntervalValue[INTEGER_LEN];

// NeoPixel strip / effects engine
// Note: Compiled to support both RGB and RGBW, switchable at runtime
#define LEDTYPE (NEO_GRB + NEO_KHZ800)  // Default to RGB; can switch to RGBW via config

LedEffects effects = LedEffects(NUMLEDS, DATAPIN, LEDTYPE);
int numberLeds;
bool gLedTypeRGBW = DEFAULT_LED_TYPE_RGBW;  // Runtime LED type setting
uint16_t gFadeDurationMs = DEFAULT_FADE_MS;
uint8_t gDefaultBrightness = APP_DEFAULT_BRIGHTNESS;
float gGamma = DEFAULT_GAMMA;

// Preview mode: when enabled, presence-driven changes are paused and a selected profile is shown
static bool gPreviewMode = false;
static String gPreviewKey = "";

// Status LED (onboard WS2812 on S3 board, GPIO21)
static Adafruit_NeoPixel* gStatusLed = nullptr;
bool gStatusLedEnabled = DEFAULT_STATUS_LED_ENABLED;
static uint32_t gStatusLedLastColor = 0xFFFFFFFFu;

// Guard concurrent access to effects and fade state
static SemaphoreHandle_t gEffectsMutex = nullptr;
#define EFFECTS_LOCK()   do { if (gEffectsMutex) xSemaphoreTake(gEffectsMutex, portMAX_DELAY); } while(0)
#define EFFECTS_UNLOCK() do { if (gEffectsMutex) xSemaphoreGive(gEffectsMutex); } while(0)

// Lightweight logs ring buffer (kept in RAM)
#define LOG_CAPACITY 120
#define LOG_LINE_MAX 160
static char gLogs[LOG_CAPACITY][LOG_LINE_MAX];
static uint16_t gLogHead = 0;
static uint16_t gLogCount = 0;

void addLog(const char* msg) {
	if (!msg) msg = "";
	char line[LOG_LINE_MAX];
	snprintf(line, sizeof(line), "[%lu] %s", (unsigned long)millis(), msg);
	strncpy(gLogs[gLogHead], line, LOG_LINE_MAX - 1);
	gLogs[gLogHead][LOG_LINE_MAX - 1] = '\0';
	gLogHead = (gLogHead + 1) % LOG_CAPACITY;
	if (gLogCount < LOG_CAPACITY) gLogCount++;
}

void addLogf(const char* fmt, ...) {
	char tmp[LOG_LINE_MAX - 8];
	va_list ap; va_start(ap, fmt);
	vsnprintf(tmp, sizeof(tmp), fmt, ap);
	va_end(ap);
	addLog(tmp);
}

// Persistable OTA session log (written to SPIFFS after OTA completes)
static String gOtaLog;
static void otaLog(const char* msg) {
	addLog(msg);
	if (gOtaLog.length() < 4096) { // cap ~4KB
		gOtaLog += msg; gOtaLog += '\n';
	}
}
static void otaLogf(const char* fmt, ...) {
	char tmp[160];
	va_list args; va_start(args, fmt);
	vsnprintf(tmp, sizeof(tmp), fmt, args);
	va_end(args);
	otaLog(tmp);
}
static void otaLogSaveToFile() {
	File f = SPIFFS.open("/ota_last.txt", "w");
	if (f) { f.print(gOtaLog); f.close(); }
}

// Build a recent logs string for server-side initial page render
String getLogsText(int n) {
	if (n <= 0) n = 50;
	if (n > LOG_CAPACITY) n = LOG_CAPACITY;
	int count = (gLogCount < n) ? gLogCount : n;
	String out;
	out.reserve(count * 48);
	for (int i = count - 1; i >= 0; --i) {
		int idx = (int)gLogHead - 1 - i;
		while (idx < 0) idx += LOG_CAPACITY;
		out += gLogs[idx];
		if (i > 0) out += '\n';
	}
	return out;
}

// Effect profile mapping per presence activity
struct EffectProfile {
	const char* key;
	uint32_t color;
	uint16_t mode;
	uint16_t speed;
	bool reverse;
	uint16_t fadeMs;
	uint8_t bri;
};

// Default effect profiles for common Teams activities
EffectProfile gProfiles[] = {
	{"Available",         GREEN,           FX_MODE_STATIC,      3000, false, 0, 0},
	{"Away",              YELLOW,          FX_MODE_STATIC,      3000, false, 0, 0},
	{"BeRightBack",       ORANGE,          FX_MODE_STATIC,      3000, false, 0, 0},
	{"Busy",              PURPLE,          FX_MODE_STATIC,      3000, false, 0, 0},
	{"DoNotDisturb",      PINK,            FX_MODE_STATIC,      3000, false, 0, 0},
	{"UrgentInterruptionsOnly", PINK,      FX_MODE_STATIC,      3000, false, 0, 0},
	{"InACall",           RED,             FX_MODE_BREATH,      3000, false, 0, 0},
	{"InAConferenceCall", RED,             FX_MODE_BREATH,      9000, false, 0, 0},
	{"Inactive",          0,               FX_MODE_BREATH,      3000, false, 0, 0},
	{"InAMeeting",        RED,             FX_MODE_SCAN,        3000, false, 0, 0},
	{"Offline",           BLACK,           FX_MODE_STATIC,      3000, false, 0, 0},
	{"OffWork",           BLACK,           FX_MODE_STATIC,      3000, false, 0, 0},
	{"OutOfOffice",       BLACK,           FX_MODE_STATIC,      3000, false, 0, 0},
	{"PresenceUnknown",   BLACK,           FX_MODE_STATIC,      3000, false, 0, 0},
	{"Presenting",        RED,             FX_MODE_COLOR_WIPE,  3000, false, 0, 0},
};

EffectProfile* findProfile(const String& k) {
	for (size_t i = 0; i < (sizeof(gProfiles)/sizeof(gProfiles[0])); i++) {
		if (k.equals(gProfiles[i].key)) return &gProfiles[i];
	}
	return nullptr;
}

// Save both system and effects settings into CONFIG_FILE (unified config)
void saveAppConfig() {
	JsonDocument doc;
	JsonObject sys = doc["system"].to<JsonObject>();
	sys["client_id"] = paramClientIdValue;
	sys["tenant"] = paramTenantValue;
	sys["poll_interval"] = atoi(paramPollIntervalValue);
	sys["num_leds"] = numberLeds;
	sys["fade_ms"] = gFadeDurationMs;
	sys["brightness"] = gDefaultBrightness;
	sys["gamma"] = gGamma;
	sys["led_type_rgbw"] = gLedTypeRGBW;  // Save LED type setting
	sys["status_led_enabled"] = gStatusLedEnabled;  // Save status LED setting
	JsonObject eff = doc["effects"].to<JsonObject>();
	JsonArray arr = eff["profiles"].to<JsonArray>();
	for (size_t i = 0; i < (sizeof(gProfiles)/sizeof(gProfiles[0])); i++) {
		JsonObject o = arr.add<JsonObject>();
		o["key"] = gProfiles[i].key;
		o["mode"] = gProfiles[i].mode;
		o["speed"] = gProfiles[i].speed;
		o["reverse"] = gProfiles[i].reverse;
		o["color"] = gProfiles[i].color;
		o["fade_ms"] = gProfiles[i].fadeMs;
		o["bri"] = gProfiles[i].bri;
	}

	File f = SPIFFS.open(CONFIG_FILE, FILE_WRITE);
	if (!f) { DBG_PRINTLN(F("saveAppConfig() - open failed")); return; }
	size_t n = serializeJsonPretty(doc, f);
	f.close();
	DBG_PRINT(F("saveAppConfig() - wrote bytes: ")); DBG_PRINTLN(n);
}

// Backwards-compat wrapper
void saveEffectsConfig() {
	saveAppConfig();
}


void loadAppConfig() {
	// If CONFIG_FILE exists, prefer it; else migrate from EFFECTS_FILE + current params
	if (!SPIFFS.exists(CONFIG_FILE)) {
		if (SPIFFS.exists(EFFECTS_FILE)) {
			File fe = SPIFFS.open(EFFECTS_FILE, FILE_READ);
			if (fe) {
				JsonDocument edoc;
				if (!deserializeJson(edoc, fe)) {
					if (!edoc["fade_ms"].isNull()) gFadeDurationMs = (uint16_t)edoc["fade_ms"].as<unsigned int>();
					if (!edoc["brightness"].isNull()) { gDefaultBrightness = (uint8_t)edoc["brightness"].as<unsigned int>(); effects.setBrightness(gDefaultBrightness); }
					if (!edoc["gamma"].isNull()) { gGamma = edoc["gamma"].as<float>(); if (isnan(gGamma) || gGamma < 0.1f) gGamma = 2.2f; if (gGamma > 5.0f) gGamma = 5.0f; }
					if (edoc["profiles"].is<JsonArray>()) {
						JsonArray arr = edoc["profiles"].as<JsonArray>();
						for (JsonObject o : arr) {
							const char* key = o["key"] | "";
							EffectProfile* p = findProfile(String(key));
							if (p) {
								if (!o["mode"].isNull()) p->mode = (uint16_t)o["mode"].as<unsigned int>();
								if (!o["speed"].isNull()) p->speed = (uint16_t)o["speed"].as<unsigned int>();
								if (!o["reverse"].isNull()) p->reverse = o["reverse"].as<bool>();
								if (!o["color"].isNull()) p->color = o["color"].as<uint32_t>();
								if (!o["fade_ms"].isNull()) p->fadeMs = (uint16_t)o["fade_ms"].as<unsigned int>();
								if (!o["bri"].isNull()) p->bri = (uint8_t)o["bri"].as<unsigned int>();
							}
						}
					}
				}
				fe.close();
			}
		}
		numberLeds = NUMLEDS;
		effects.setLength(numberLeds);
		saveAppConfig();
		DBG_PRINTLN(F("loadAppConfig() - created initial CONFIG_FILE"));
		return;
	}
	File f = SPIFFS.open(CONFIG_FILE, FILE_READ);
	if (!f) { DBG_PRINTLN(F("loadAppConfig() - open failed")); return; }
	JsonDocument doc;
	DeserializationError err = deserializeJson(doc, f);
	f.close();
	if (err) { DBG_PRINT(F("loadAppConfig() - parse error: ")); DBG_PRINTLN(err.c_str()); return; }
	JsonObject sys = doc["system"];
	if (!sys.isNull()) {
		if (!sys["client_id"].isNull()) strlcpy(paramClientIdValue, sys["client_id"], sizeof(paramClientIdValue));
		if (!sys["tenant"].isNull()) strlcpy(paramTenantValue, sys["tenant"], sizeof(paramTenantValue));
		if (!sys["poll_interval"].isNull()) { snprintf(paramPollIntervalValue, sizeof(paramPollIntervalValue), "%u", (unsigned int)sys["poll_interval"].as<unsigned int>()); }
		if (!sys["num_leds"].isNull()) { numberLeds = (int)sys["num_leds"].as<int>(); effects.setLength(numberLeds); }
		if (!sys["fade_ms"].isNull()) gFadeDurationMs = (uint16_t)sys["fade_ms"].as<unsigned int>();
		if (!sys["brightness"].isNull()) { gDefaultBrightness = (uint8_t)sys["brightness"].as<unsigned int>(); effects.setBrightness(gDefaultBrightness); }
		if (!sys["gamma"].isNull()) { gGamma = sys["gamma"].as<float>(); if (isnan(gGamma) || gGamma < 0.1f) gGamma = 2.2f; if (gGamma > 5.0f) gGamma = 5.0f; }
		if (!sys["led_type_rgbw"].isNull()) { gLedTypeRGBW = sys["led_type_rgbw"].as<bool>(); effects.setPixelType(gLedTypeRGBW); }
		if (!sys["status_led_enabled"].isNull()) { gStatusLedEnabled = sys["status_led_enabled"].as<bool>(); }
	}
	JsonObject eff = doc["effects"];
	if (!eff.isNull() && eff["profiles"].is<JsonArray>()) {
		JsonArray arr = eff["profiles"].as<JsonArray>();
		for (JsonObject o : arr) {
			const char* key = o["key"] | "";
			EffectProfile* p = findProfile(String(key));
			if (p) {
				if (!o["mode"].isNull()) p->mode = (uint16_t)o["mode"].as<unsigned int>();
				if (!o["speed"].isNull()) p->speed = (uint16_t)o["speed"].as<unsigned int>();
				if (!o["reverse"].isNull()) p->reverse = o["reverse"].as<bool>();
				if (!o["color"].isNull()) p->color = o["color"].as<uint32_t>();
				if (!o["fade_ms"].isNull()) p->fadeMs = (uint16_t)o["fade_ms"].as<unsigned int>();
				if (!o["bri"].isNull()) p->bri = (uint8_t)o["bri"].as<unsigned int>();
			}
		}
	}
	DBG_PRINTLN(F("loadAppConfig() - applied"));
}

// Backwards-compat wrapper
void loadEffectsConfig() {
	loadAppConfig();
}

// Fade transition state (single-phase fade-in by default)
struct FadeTransition {
	bool active = false;
	bool phaseOut = true;
	uint8_t startBri = 0;
	uint8_t endBri = 0;
	unsigned long startMs = 0;
	unsigned long durationMs = 0;
	uint16_t pendingMode = FX_MODE_STATIC;
	uint32_t pendingColor = BLACK;
	uint16_t pendingSpeed = 3000;
	bool pendingReverse = false;
	uint8_t originalBri = 128;
} gFade;

// Track last requested target effect to avoid redundant restarts
struct TargetEffect {
	bool initialized = false;
	uint16_t mode = FX_MODE_STATIC;
	uint32_t color = BLACK;
	uint16_t speed = 3000;
	bool reverse = false;
	uint8_t targetBri = 0;
} gTarget;

// Next transition's desired brightness (0=use global)
static uint8_t gNextTargetBri = 0;

void startFade(uint8_t from, uint8_t to, unsigned long dur) {
	gFade.active = true;
	gFade.startBri = from;
	gFade.endBri = to;
	gFade.startMs = millis();
	gFade.durationMs = dur;
}

void updateFade() {
	if (!gFade.active) return;
	unsigned long now = millis();
	float t = (gFade.durationMs == 0) ? 1.0f : (float)(now - gFade.startMs) / (float)gFade.durationMs;
	if (t >= 1.0f) t = 1.0f;
	float tg = powf(t, gGamma);
	if (tg < 0.0f) tg = 0.0f; if (tg > 1.0f) tg = 1.0f;
	int delta = (int)gFade.endBri - (int)gFade.startBri;
	uint8_t bri = (uint8_t)((int)gFade.startBri + (int)(delta * tg));
	effects.setBrightness(bri);

	if (t >= 1.0f) {
		if (gFade.phaseOut) {
			// Fade-out phase complete, switch to new effect and start fade-in
			effects.setSegment(0, 0, numberLeds, gFade.pendingMode, gFade.pendingColor, gFade.pendingSpeed, gFade.pendingReverse);
			effects.trigger();
			gFade.phaseOut = false;
			// Start fade-in phase (0 -> target brightness)
			startFade(0, gFade.originalBri, gFade.durationMs);
		} else {
			// Fade-in phase complete, end transition
			gFade.active = false;
			gFade.phaseOut = true;
			gFade.durationMs = 0;
		}
	}
}

// Global variables
String user_code = "";
String device_code = "";
uint8_t interval = 5;
String access_token = "";
String refresh_token = "";
String id_token = "";
unsigned long expires = 0;
String availability = "";
String activity = "";

// Statemachine
#define SMODEINITIAL 0
#define SMODEWIFICONNECTING 1
#define SMODEWIFICONNECTED 2
#define SMODEDEVICELOGINSTARTED 10
#define SMODEDEVICELOGINFAILED 11
#define SMODEAUTHREADY 20
#define SMODEPOLLPRESENCE 21
#define SMODEREFRESHTOKEN 22
#define SMODEPRESENCEREQUESTERROR 23
uint8_t state = SMODEINITIAL;
uint8_t laststate = SMODEINITIAL;
static unsigned long tsPolling = 0;
uint8_t retries = 0;

// AP state flag
bool gApEnabled = false;
String gApSsid; // SoftAP SSID (matches the generated device name)

String getPresentedSharedKey() {
	String k = server.header("X-StatusGlow-Key");
	if (k.length() == 0) k = server.header("X-OTA-Key");
	if (k.length() == 0 && server.hasArg("key")) k = server.arg("key");
	return k;
}

bool isRequestAuthorized(const char* expectedKey) {
	return expectedKey && expectedKey[0] != '\0' && getPresentedSharedKey() == expectedKey;
}

static void sendUnauthorizedResponse(const char* scope) {
	const bool structured = server.uri().startsWith("/api/") || server.uri().startsWith("/fs/");
	if (structured) {
		JsonDocument d;
		d["ok"] = false;
		d["error"] = "unauthorized";
		d["scope"] = scope;
		server.send(401, "application/json", d.as<String>());
	} else {
		server.send(401, "text/plain", "Unauthorized");
	}
}

bool requireSharedKey(const char* expectedKey, const char* scope) {
	if (isRequestAuthorized(expectedKey)) return true;
	addLogf("Unauthorized %s request blocked: %s", scope, server.uri().c_str());
	sendUnauthorizedResponse(scope);
	return false;
}

bool isAdminRequestAuthorized() {
	return isRequestAuthorized(gAdminSharedKey.c_str());
}

bool requireAdminAuth() {
	return requireSharedKey(gAdminSharedKey.c_str(), "admin");
}

bool requireOtaAuth() {
	return requireSharedKey(gOtaSharedKey.c_str(), "ota");
}

static String urlEncode(const String& in) {
	String out;
	out.reserve(in.length() * 3);
	for (size_t i = 0; i < in.length(); ++i) {
		const unsigned char c = (unsigned char)in.charAt(i);
		const bool safe =
			(c >= 'A' && c <= 'Z') ||
			(c >= 'a' && c <= 'z') ||
			(c >= '0' && c <= '9') ||
			c == '-' || c == '_' || c == '.' || c == '~';
		if (safe) {
			out += (char)c;
		} else {
			char buf[4];
			snprintf(buf, sizeof(buf), "%%%02X", c);
			out += buf;
		}
	}
	return out;
}

static String normalizedHostHeader(String host) {
	host.trim();
	const int colon = host.indexOf(':');
	if (colon >= 0) host.remove(colon);
	host.toLowerCase();
	return host;
}

static bool isCaptivePortalLocalHost(const String& host) {
	if (host.length() == 0) return false;
	if (host == F("localhost")) return true;
	if (host == WiFi.softAPIP().toString()) return true;
	if (host == gThingHostName) return true;
	if (host == (gThingHostName + F(".local"))) return true;
	return false;
}

static String getCaptivePortalUrl() {
	String url = String(F("http://")) + WiFi.softAPIP().toString() + F("/config");
	if (gAdminSharedKey.length() > 0) {
		url += F("?key=");
		url += urlEncode(gAdminSharedKey);
	}
	return url;
}

static bool shouldRedirectToCaptivePortal() {
	if (!gApEnabled) return false;
	return !isCaptivePortalLocalHost(normalizedHostHeader(server.hostHeader()));
}

static void redirectToCaptivePortal() {
	const String portalUrl = getCaptivePortalUrl();
	server.sendHeader("Cache-Control", "no-store, no-cache, must-revalidate, max-age=0");
	server.sendHeader("Pragma", "no-cache");
	server.sendHeader("Expires", "0");
	server.sendHeader("Location", portalUrl, true);
	server.send(302, "text/plain; charset=utf-8", String(F("Redirecting to ")) + portalUrl);
}

static void handleCaptivePortalRequest() {
	addLogf("Captive portal redirect: %s host=%s", server.uri().c_str(), server.hostHeader().c_str());
	redirectToCaptivePortal();
}

static String withSuffixWithinLimit(const String& base, const String& suffix, size_t maxLen) {
	const size_t suffixOverhead = 1 + suffix.length();
	if (maxLen <= suffixOverhead) return suffix;
	String trimmedBase = base;
	if (trimmedBase.length() + suffixOverhead > maxLen) {
		trimmedBase.remove(maxLen - suffixOverhead);
	}
	return trimmedBase + "-" + suffix;
}

static String getDeviceSuffixUpper() {
	char buf[5];
	snprintf(buf, sizeof(buf), "%04X", (uint16_t)(ESP.getEfuseMac() & 0xFFFFu));
	return String(buf);
}

static void initDeviceIdentity() {
	gDeviceSuffixUpper = getDeviceSuffixUpper();
	gDeviceSuffixLower = gDeviceSuffixUpper;
	gDeviceSuffixLower.toLowerCase();
	gThingName = withSuffixWithinLimit(String(THING_NAME), gDeviceSuffixUpper, 31);
	gThingHostName = gThingName;
	gThingHostName.toLowerCase();
	gWifiInitialApPassword = String(WIFI_INITIAL_AP_PASSWORD_PREFIX) + "-" + gDeviceSuffixLower;
	if (strlen(ADMIN_SHARED_KEY) > 0) gAdminSharedKey = String(ADMIN_SHARED_KEY);
	else gAdminSharedKey = gWifiInitialApPassword;
	if (strlen(OTA_SHARED_KEY) > 0) gOtaSharedKey = String(OTA_SHARED_KEY);
	else gOtaSharedKey = gAdminSharedKey;
}

static String makeApSsid() {
	return gThingName;
}

static void startSoftAPIfNeeded() {
	if (!gApEnabled) {
		// Clear any existing WiFi state
		WiFi.disconnect(true);
		delay(100);
		
		WiFi.mode(WIFI_AP_STA);
		delay(100);
		
		gApSsid = makeApSsid();
		
		// Configure AP with explicit IP settings
		IPAddress local_IP(192,168,4,1);
		IPAddress gateway(192,168,4,1);
		IPAddress subnet(255,255,255,0);
		WiFi.softAPConfig(local_IP, gateway, subnet);
		
		WiFi.softAP(gApSsid.c_str(), gWifiInitialApPassword.c_str(), 1, 0, 4);
		dnsServer.start(53, "*", local_IP);
		delay(500);
		
		gApEnabled = true;
		IPAddress apip = WiFi.softAPIP();
		DBG_PRINT(F("SoftAP active. Connect to SSID '")); DBG_PRINT(gApSsid.c_str()); DBG_PRINT(F("' (pass: '")); DBG_PRINT(gWifiInitialApPassword.c_str()); DBG_PRINT(F("') and open http://")); DBG_PRINTLN(apip.toString().c_str());
	}
}

static void stopSoftAPIfActive() {
	if (gApEnabled) {
		dnsServer.stop();
		WiFi.softAPdisconnect(true);
		delay(100);
		
		gApEnabled = false;
		if (WiFi.status() == WL_CONNECTED) {
			WiFi.mode(WIFI_STA);
			delay(100);
		}
		DBG_PRINTLN(F("SoftAP stopped."));
	}
}

// Multicore
TaskHandle_t TaskNeopixel;
#if defined(CONFIG_IDF_TARGET_ESP32S3)
#define LED_TASK_CORE 1
#else
#define LED_TASK_CORE 0
#endif

// Calculate token lifetime
int getTokenLifetime() {
	// Compute signed difference to avoid unsigned wrap after expiry
	int32_t deltaMs = (int32_t)((uint32_t)expires - (uint32_t)millis());
	return (int)(deltaMs / 1000);
}

// CPU usage measurement using heap allocation rate as activity proxy
// Correlates well with actual CPU load (more allocations = more work)
uint8_t getCpuUsagePercent() {
	static uint32_t lastCheckMs = 0;
	static uint32_t lastHeapFree = 0;
	static uint32_t lastMinHeap = 0;
	static uint32_t heapChangeAccum = 0;
	static uint8_t cachedCpu = 0;
	static uint8_t sampleCount = 0;
	
	uint32_t nowMs = millis();
	uint32_t heapFree = ESP.getFreeHeap();
	uint32_t minHeap = ESP.getMinFreeHeap();
	
	// Track heap activity between samples
	if (lastCheckMs > 0) {
		// Calculate heap activity (how much it changed)
		uint32_t heapDelta = (lastHeapFree > heapFree) ? 
			(lastHeapFree - heapFree) : (heapFree - lastHeapFree);
		heapChangeAccum += heapDelta;
		sampleCount++;
	}
	
	// Update display every 500ms
	if (lastCheckMs != 0 && (nowMs - lastCheckMs) < 500) {
		return cachedCpu;
	}
	
	uint8_t cpuPercent = 15; // Baseline idle CPU
	
	if (lastCheckMs > 0 && sampleCount > 0) {
		uint32_t deltaTime = nowMs - lastCheckMs;
		
		// Calculate heap churn rate (bytes/second)
		uint32_t heapChurnRate = (heapChangeAccum * 1000) / deltaTime;
		
		// Map heap activity to CPU percentage
		// Empirically determined thresholds:
		// 0-500 bytes/s = idle (10-15%)
		// 500-2000 bytes/s = light (15-30%)
		// 2000-8000 bytes/s = moderate (30-60%)
		// 8000+ bytes/s = heavy (60-90%)
		
		if (heapChurnRate < 500) {
			cpuPercent = 10 + (heapChurnRate / 50); // 10-20%
		} else if (heapChurnRate < 2000) {
			cpuPercent = 20 + ((heapChurnRate - 500) / 50); // 20-50%
		} else if (heapChurnRate < 8000) {
			cpuPercent = 50 + ((heapChurnRate - 2000) / 150); // 50-90%
		} else {
			cpuPercent = 90; // Cap at 90%
		}
		
		// Add boost if minimum heap is dropping (sign of sustained activity)
		if (lastMinHeap > 0 && minHeap < lastMinHeap) {
			uint32_t heapDrop = lastMinHeap - minHeap;
			if (heapDrop > 1000) {
				cpuPercent += 10; // Heavy sustained load
			} else if (heapDrop > 100) {
				cpuPercent += 5; // Moderate sustained load
			}
		}
		
		// Clamp to 10-95% range
		if (cpuPercent < 10) cpuPercent = 10;
		if (cpuPercent > 95) cpuPercent = 95;
	}
	
	// Reset accumulators
	heapChangeAccum = 0;
	sampleCount = 0;
	lastCheckMs = nowMs;
	lastHeapFree = heapFree;
	lastMinHeap = minHeap;
	cachedCpu = cpuPercent;
	
	return cpuPercent;
}

static void ensureStatusLedReady() {
#if defined(CONFIG_IDF_TARGET_ESP32S3)
	if (gStatusLedEnabled && !gStatusLed) {
		gStatusLed = new Adafruit_NeoPixel(1, STATUS_LED_PIN, NEO_GRB + NEO_KHZ800);
		if (gStatusLed) {
			gStatusLed->begin();
			gStatusLed->setBrightness(50);
			gStatusLed->clear();
			gStatusLed->show();
			gStatusLedLastColor = 0;
		}
	}
#endif
}

static void setStatusLedColor(uint32_t color) {
	if (!gStatusLed || gStatusLedLastColor == color) return;
	gStatusLed->setPixelColor(0, color);
	gStatusLed->show();
	gStatusLedLastColor = color;
}

// Save context information to file in SPIFFS
void saveContext() {
	JsonDocument contextDoc;
	contextDoc["access_token"] = access_token.c_str();
	contextDoc["refresh_token"] = refresh_token.c_str();
	contextDoc["id_token"] = id_token.c_str();
	File contextFile = SPIFFS.open(CONTEXT_FILE, FILE_WRITE);
	size_t bytesWritten = serializeJsonPretty(contextDoc, contextFile);
	contextFile.close();
	DBG_PRINT(F("saveContext() - Success: "));
	DBG_PRINTLN(bytesWritten);
}

boolean loadContext() {
	File file = SPIFFS.open(CONTEXT_FILE);
	boolean success = false;

	if (!file) {
		DBG_PRINTLN(F("loadContext() - No file found"));
	} else {
		size_t size = file.size();
		if (size == 0) {
			DBG_PRINTLN(F("loadContext() - File empty"));
		} else {
			JsonDocument contextDoc;
			DeserializationError err = deserializeJson(contextDoc, file);

			if (err) {
				DBG_PRINT(F("loadContext() - deserializeJson() failed with code: "));
				DBG_PRINTLN(err.c_str());
			} else {
				int numSettings = 0;
				if (!contextDoc["access_token"].isNull()) {
					access_token = contextDoc["access_token"].as<String>();
					numSettings++;
				}
				if (!contextDoc["refresh_token"].isNull()) {
					refresh_token = contextDoc["refresh_token"].as<String>();
					numSettings++;
				}
				if (!contextDoc["id_token"].isNull()){
					id_token = contextDoc["id_token"].as<String>();
					numSettings++;
				}
				if (numSettings == 3) {
					success = true;
					DBG_PRINTLN(F("loadContext() - Success"));
					if (strlen(paramClientIdValue) > 0 && strlen(paramTenantValue) > 0) {
						DBG_PRINTLN(F("loadContext() - Next: Refresh token."));
						state = SMODEREFRESHTOKEN;
					} else {
						DBG_PRINTLN(F("loadContext() - No client id or tenant setting found."));
					}
				} else {
					DBG_PRINT("loadContext() - ERROR Number of valid settings in file: "); DBG_PRINT(numSettings); DBG_PRINTLN(", should be 3.");
				}
			}
		}
		file.close();
	}

	return success;
}

// Remove context information file in SPIFFS
void removeContext() {
	SPIFFS.remove(CONTEXT_FILE);
	DBG_PRINTLN(F("removeContext() - Success"));
}

void startMDNS() {
	DBG_PRINTLN("startMDNS()");
    if (!MDNS.begin(gThingHostName.c_str())) {
        DBG_PRINTLN("Error setting up MDNS responder!");
		addLog("mDNS setup failed");
        while(1) {
            delay(1000);
        }
    }
	MDNS.addService("http", "tcp", 80);

    DBG_PRINT("mDNS responder started: ");
    DBG_PRINT(gThingHostName.c_str());
    DBG_PRINTLN(".local");
	addLogf("mDNS started: %s.local", gThingHostName.c_str());
}

// Synchronize time via NTP so TLS validation succeeds
void syncTime() {
	DBG_PRINTLN(F("syncTime() starting NTP"));
    addLog("NTP sync start");
	configTime(0, 0, "pool.ntp.org", "time.nist.gov");

	// Wait up to ~10 seconds for time to be set
	time_t now = 0;
	int retries = 0;
	while (retries < 100) {
		now = time(nullptr);
		if (now > 1609459200) {
			break;
		}
		vTaskDelay(100 / portTICK_PERIOD_MS);
		retries++;
	}
	if (now > 1609459200) {
		struct tm timeinfo;
		gmtime_r(&now, &timeinfo);
		DBG_PRINT(F("NTP time set: "));
		DBG_PRINTLN(asctime(&timeinfo));
        addLog("NTP time set");
	} else {
		DBG_PRINTLN(F("NTP time sync timed out; TLS may fail until time is set."));
        addLog("NTP sync timeout");
	}
}

#include "request_handler.h"
#include "spiffs_webserver.h"

void handleFirmwareUi();

// Neopixel control
void setAnimation(uint8_t segment, uint8_t mode = FX_MODE_STATIC, uint32_t color = RED, uint16_t speed = 3000, bool reverse = false) {
	uint16_t startLed = 0, endLed = 0;
	if (segment == 0) {
		startLed = 0;
		endLed = numberLeds;
	}
	DBG_PRINT("setAnimation ");
	DBG_PRINT(segment); DBG_PRINT(": "); DBG_PRINT(startLed); DBG_PRINT("-"); DBG_PRINT(endLed); DBG_PRINT(" M:"); DBG_PRINT(mode); DBG_PRINT(" C:"); DBG_PRINT((unsigned int)color); DBG_PRINT(" S:"); DBG_PRINTLN((unsigned int)speed);
			uint8_t targetBri = (gNextTargetBri != 0) ? gNextTargetBri : gDefaultBrightness;
			gNextTargetBri = 0;
			
			EFFECTS_LOCK();
			
			// Check if we're already showing this exact animation
			if (gTarget.initialized) {
				bool sameTarget = (gTarget.mode == mode) && (gTarget.color == color) && 
				                  (gTarget.speed == speed) && (gTarget.reverse == reverse);
				if (sameTarget && !gFade.active) {
					// Already at target and not fading, nothing to do
					EFFECTS_UNLOCK();
					return;
				}
				
				// Check if we're already fading to this exact animation
				if (gFade.active && gFade.phaseOut) {
					bool samePending = (gFade.pendingMode == mode) && (gFade.pendingColor == color) && 
					                   (gFade.pendingSpeed == speed) && (gFade.pendingReverse == reverse);
					if (samePending) {
						// Already fading to this target, don't interrupt
						EFFECTS_UNLOCK();
						return;
					}
				}
			}
			
			// Update gTarget inside the lock to prevent race conditions
			gTarget.initialized = true;
			gTarget.mode = mode;
			gTarget.color = color;
			gTarget.speed = speed;
			gTarget.reverse = reverse;
			gTarget.targetBri = targetBri;
			
			if (gFadeDurationMs > 0) {
				// Two-phase transition: fade out current effect, then fade in new effect
				gFade.originalBri = targetBri;
				gFade.pendingMode = mode;
				gFade.pendingColor = color;
				gFade.pendingSpeed = speed;
				gFade.pendingReverse = reverse;
				gFade.phaseOut = true;
				// Start fade-out phase (current brightness -> 0)
				uint8_t currentBri = effects.getBrightness();
				startFade(currentBri, 0, (gFade.durationMs == 0 ? gFadeDurationMs : gFade.durationMs) / 2);
			} else {
				// No fade, immediate transition
				if (mode == FX_MODE_STATIC) {
					effects.setBrightness(targetBri);
					effects.setSegment(segment, startLed, endLed, mode, color, speed, reverse);
					effects.trigger();
				} else {
					effects.setSegment(segment, startLed, endLed, mode, color, speed, reverse);
				}
			}
			EFFECTS_UNLOCK();
}

void setPresenceAnimation() {
	if (gPreviewMode) return;
	uint16_t tMode = FX_MODE_STATIC;
	uint32_t tColor = BLACK;
	uint16_t tSpeed = 3000;
	bool tReverse = false;
	uint16_t perFade = 0;
	uint8_t perBri = gDefaultBrightness;
	EffectProfile* p = findProfile(activity);
	if (p != nullptr) {
		tMode = p->mode;
		tColor = p->color;
		tSpeed = p->speed;
		tReverse = p->reverse;
		perFade = p->fadeMs;
		perBri = (p->bri != 0) ? p->bri : gDefaultBrightness;
	} else {
	if (activity.equals("Inactive")) {
	    tMode = FX_MODE_STATIC; tColor = BLACK;
	} else if (activity.equals("Presenting")) {
			tMode = FX_MODE_COLOR_WIPE; tColor = RED;
		} else if (activity.equals("InAMeeting")) {
			tMode = FX_MODE_SCAN; tColor = RED;
		} else if (activity.equals("InACall")) {
			tMode = FX_MODE_BREATH; tColor = RED;
		} else if (activity.equals("Offline") || activity.equals("OffWork") || activity.equals("OutOfOffice") || activity.equals("PresenceUnknown")) {
			tMode = FX_MODE_STATIC; tColor = BLACK;
		} else if (activity.equals("DoNotDisturb") || activity.equals("UrgentInterruptionsOnly")) {
			tMode = FX_MODE_STATIC; tColor = PINK;
		} else if (activity.equals("Busy")) {
			tMode = FX_MODE_STATIC; tColor = PURPLE;
		} else if (activity.equals("BeRightBack")) {
			tMode = FX_MODE_STATIC; tColor = ORANGE;
		} else if (activity.equals("Away")) {
			tMode = FX_MODE_STATIC; tColor = YELLOW;
		} else if (activity.equals("Available")) {
			tMode = FX_MODE_STATIC; tColor = GREEN;
		}
	}
	if (gTarget.initialized) {
		bool sameTarget = (gTarget.mode == tMode) && (gTarget.color == tColor) && (gTarget.speed == tSpeed) && (gTarget.reverse == tReverse);
		if (sameTarget) return;
		if (gFade.active && gFade.phaseOut) {
			bool samePending = (gFade.pendingMode == tMode) && (gFade.pendingColor == tColor) && (gFade.pendingSpeed == tSpeed) && (gFade.pendingReverse == tReverse);
			if (samePending) return;
		}
	}
	EFFECTS_LOCK();
	gFade.durationMs = perFade;
	gNextTargetBri = perBri;
	EFFECTS_UNLOCK();
	setAnimation(0, tMode, tColor, tSpeed, tReverse);
}

// Apply the current preview selection if preview mode is active
void applyPreviewSelection() {
	if (!gPreviewMode || gPreviewKey.length() == 0) return;
	EffectProfile* p = findProfile(gPreviewKey);
	if (!p) return;
	uint16_t tMode = p->mode;
	uint32_t tColor = p->color;
	uint16_t tSpeed = p->speed;
	bool tReverse = p->reverse;
	uint16_t perFade = p->fadeMs;
	uint8_t perBri = (p->bri != 0) ? p->bri : gDefaultBrightness;
	EFFECTS_LOCK();
	gFade.durationMs = perFade;
	gNextTargetBri = perBri;
	EFFECTS_UNLOCK();
	setAnimation(0, tMode, tColor, tSpeed, tReverse);
}

// Handler: Wifi connected
void onWifiConnected() { state = SMODEWIFICONNECTED; }

// Poll for access token during device login flow
void pollForToken() {
	if (time(nullptr) < 1609459200) {
		syncTime();
	}
	String payload = "client_id=" + String(paramClientIdValue) + "&grant_type=urn:ietf:params:oauth:grant-type:device_code&device_code=" + device_code;
	DBG_PRINTLN("pollForToken()");
	JsonDocument responseDoc;
	boolean res = requestJsonApi(responseDoc, "https://login.microsoftonline.com/" + String(paramTenantValue) + "/oauth2/v2.0/token", payload, 0);
	if (!res) {
		state = SMODEDEVICELOGINFAILED;
	} else if (!responseDoc["error"].isNull()) {
		const char* _error = responseDoc["error"];
		const char* _error_description = responseDoc["error_description"];
		if (strcmp(_error, "authorization_pending") == 0) {
			DBG_PRINT("pollForToken() - Wating for authorization by user: "); DBG_PRINTLN(_error_description);
		} else {
			DBG_PRINT("pollForToken() - Unexpected error: "); DBG_PRINT(_error); DBG_PRINT(", "); DBG_PRINTLN(_error_description);
			state = SMODEDEVICELOGINFAILED;
		}
	} else {
	if (!responseDoc["access_token"].isNull() && !responseDoc["refresh_token"].isNull() && !responseDoc["id_token"].isNull()) {
			// Save tokens and expiration
			access_token = responseDoc["access_token"].as<String>();
			refresh_token = responseDoc["refresh_token"].as<String>();
			id_token = responseDoc["id_token"].as<String>();
			unsigned int _expires_in = responseDoc["expires_in"].as<unsigned int>();
			// Calculate timestamp when token expires
			expires = millis() + (_expires_in * 1000);

			// Set state
			state = SMODEAUTHREADY;
		} else {
			DBG_PRINT("pollForToken() - Unknown response: "); DBG_PRINTLN(responseDoc.as<const char*>());
		}
	}
}

// Get presence information from Microsoft Graph
void pollPresence() {
	JsonDocument responseDoc;
	boolean res = requestJsonApi(responseDoc, "https://graph.microsoft.com/v1.0/me/presence", "", 0, "GET", true);

	if (!res) {
		state = SMODEPRESENCEREQUESTERROR;
		retries++;
	} else if (!responseDoc["error"].isNull()) {
		const char* _error_code = responseDoc["error"]["code"];
		if (_error_code && strcmp(_error_code, "InvalidAuthenticationToken") == 0) {
			DBG_PRINTLN(F("pollPresence() - Refresh needed"));
			tsPolling = millis();
			state = SMODEREFRESHTOKEN;
		} else {
			DBG_PRINT("pollPresence() - Error: "); DBG_PRINTLN(_error_code ? _error_code : "(null)");
			state = SMODEPRESENCEREQUESTERROR;
			retries++;
		}
	} else {
		availability = responseDoc["availability"].as<String>();
		activity = responseDoc["activity"].as<String>();
		retries = 0;

		setPresenceAnimation();
	}
}

// Refresh the access token
boolean refreshToken() {
	if (time(nullptr) < 1609459200) {
		syncTime();
	}
	boolean success = false;
	String payload = "client_id=" + String(paramClientIdValue) + "&grant_type=refresh_token&refresh_token=" + refresh_token;
	DBG_PRINTLN(F("refreshToken()"));
	JsonDocument responseDoc;
	boolean res = requestJsonApi(responseDoc, "https://login.microsoftonline.com/" + String(paramTenantValue) + "/oauth2/v2.0/token", payload, 0);
	if (res && !responseDoc["access_token"].isNull() && !responseDoc["refresh_token"].isNull()) {
		if (!responseDoc["access_token"].isNull()) {
			access_token = responseDoc["access_token"].as<String>();
			success = true;
		}
		if (!responseDoc["refresh_token"].isNull()) {
			refresh_token = responseDoc["refresh_token"].as<String>();
			success = true;
		}
		if (!responseDoc["id_token"].isNull()) {
			id_token = responseDoc["id_token"].as<String>();
		}
		if (!responseDoc["expires_in"].isNull()) {
			int _expires_in = responseDoc["expires_in"].as<unsigned int>();
			expires = millis() + (_expires_in * 1000);
		}
		DBG_PRINTLN(F("refreshToken() - Success"));
		state = SMODEPOLLPRESENCE;
	} else {
		DBG_PRINTLN(F("refreshToken() - Error:"));
	DBG_PRINTLN(responseDoc.as<String>());
		tsPolling = millis() + (DEFAULT_ERROR_RETRY_INTERVAL * 1000);
	}
	return success;
}

// Main application state machine
void statemachine() {
	if (state == SMODEWIFICONNECTING && laststate != SMODEWIFICONNECTING) {
		setAnimation(0, FX_MODE_THEATER_CHASE, BLUE);
	}
	if (state == SMODEWIFICONNECTED && laststate != SMODEWIFICONNECTED)
	{
		setAnimation(0, FX_MODE_THEATER_CHASE, GREEN);
		startMDNS();
		syncTime();
		loadContext();
		DBG_PRINTLN(F("Wifi connected, waiting for requests ..."));
	}
	if (state == SMODEDEVICELOGINSTARTED) {
		if (laststate != SMODEDEVICELOGINSTARTED) {
			setAnimation(0, FX_MODE_THEATER_CHASE, PURPLE);
		}
		if (millis() >= tsPolling) {
			pollForToken();
			tsPolling = millis() + (interval * 1000);
		}
	}
	if (state == SMODEDEVICELOGINFAILED) {
		DBG_PRINTLN(F("Device login failed"));
		state = SMODEWIFICONNECTED;
	}
	if (state == SMODEAUTHREADY) {
		saveContext();
		state = SMODEPOLLPRESENCE;
		tsPolling = millis();
	}
	if (state == SMODEPOLLPRESENCE) {
		if (millis() >= tsPolling) {
			DBG_PRINTLN(F("Polling presence info ..."));
			pollPresence();
			tsPolling = millis() + (atoi(paramPollIntervalValue) * 1000);
			DBG_PRINT("--> Availability: "); DBG_PRINT(availability.c_str()); DBG_PRINT(", Activity: "); DBG_PRINTLN(activity.c_str());
		}

		if (getTokenLifetime() < TOKEN_REFRESH_TIMEOUT) {
			DBG_PRINT("Token needs refresh, valid for "); DBG_PRINT(getTokenLifetime()); DBG_PRINTLN(" s.");
			state = SMODEREFRESHTOKEN;
		}
	}
	if (state == SMODEREFRESHTOKEN) {
		if (laststate != SMODEREFRESHTOKEN) {
			setAnimation(0, FX_MODE_THEATER_CHASE, RED);
		}
		if (millis() >= tsPolling) {
			boolean success = refreshToken();
			if (success) {
				saveContext();
			}
		}
	}
	if (state == SMODEPRESENCEREQUESTERROR) {
		if (laststate != SMODEPRESENCEREQUESTERROR) {
			retries = 0;
		}
		
	DBG_PRINT("Polling presence failed, retry #"); DBG_PRINTLN(retries);
		if (retries >= 5) {
			state = SMODEREFRESHTOKEN;
		} else {
			state = SMODEPOLLPRESENCE;
		}
	}
	if (laststate != state) {
		laststate = state;
		DBG_PRINTLN(F("======================================================================"));
		// Only log errors, not normal state transitions
		// addLogf("state => %u", (unsigned)state);
	}
}

void neopixelTask(void * parameter) {
	for (;;) {
		EFFECTS_LOCK();
		updateFade();
		effects.service();
		if (!gFade.active && gTarget.initialized && gTarget.mode == FX_MODE_STATIC) {
			uint8_t cur = effects.getBrightness();
			if (cur != gTarget.targetBri) {
				effects.setBrightness(gTarget.targetBri);
			}
		}
		EFFECTS_UNLOCK();
		// Frame pacing: use LED_FRAME_DELAY_MS (configured per target in config.h)
		vTaskDelay(LED_FRAME_DELAY_MS / portTICK_PERIOD_MS);
	}
}

void setup()
{
	Serial.begin(115200);
	DBG_PRINTLN();
	DBG_PRINTLN(F("setup() Starting up..."));
	// Only log errors, not startup info
	// addLog("setup() starting");
	#ifdef DISABLECERTCHECK
		DBG_PRINTLN(F("WARNING: Checking of HTTPS certificates disabled."));
	#endif

	gEffectsMutex = xSemaphoreCreateMutex();
	// Improve signal integrity for WS2812 data pin (especially on S3 at 5V LED power)
	pinMode(DATAPIN, OUTPUT);
	digitalWrite(DATAPIN, LOW);
#if defined(CONFIG_IDF_TARGET_ESP32S3)
	// Max drive strength, no internal pull (external 330-470Ω series resistor still recommended)
	gpio_set_drive_capability((gpio_num_t)DATAPIN, GPIO_DRIVE_CAP_3);
	gpio_set_pull_mode((gpio_num_t)DATAPIN, GPIO_FLOATING);
#endif
	effects.init();
	// CPU usage now measured via FreeRTOS task statistics (no idle hooks needed)
	effects.setBrightness(gDefaultBrightness);
	effects.start();
	// Initialize LED count early so setAnimation uses a valid range; loadAppConfig() may override later
	numberLeds = NUMLEDS;
	EFFECTS_LOCK();
	effects.setLength(numberLeds);
	EFFECTS_UNLOCK();
	setAnimation(0, FX_MODE_STATIC, BLACK);
	
	initDeviceIdentity();

	// Prepare dynamic AP SSID early so UI reflects it even if AP is off
	gApSsid = makeApSsid();
	
	// Initialize optional status LED only if enabled (pin from config)
	ensureStatusLedReady();
	
	// Mount SPIFFS early so settings can be loaded before bringing up network/server
	DBG_PRINTLN(F("SPIFFS.begin() "));
	if(!SPIFFS.begin(true)) {
		DBG_PRINTLN("SPIFFS Mount Failed");
	} else {
		loadEffectsConfig();
		ensureStatusLedReady();
	}
	
	// Clear any existing WiFi configuration before starting
	WiFi.disconnect(true);
	delay(100);
	
	WiFi.mode(WIFI_STA);
	WiFi.setHostname(gThingHostName.c_str());
	WiFi.setAutoConnect(true);
	WiFi.persistent(true);
	WiFi.begin();
	unsigned long t0 = millis();
	while (WiFi.status() != WL_CONNECTED && millis() - t0 < WIFI_STA_CONNECT_TIMEOUT_MS) {
		delay(100);
	}
	if (WiFi.status() == WL_CONNECTED) {
		onWifiConnected();
		DBG_PRINT(F("WiFi connected. IP: ")); DBG_PRINTLN(WiFi.localIP().toString().c_str());
		addLogf("WiFi connected: %s", WiFi.localIP().toString().c_str());
	} else {
		// Failed to connect - erase stored credentials and start AP
		DBG_PRINTLN(F("WiFi connection failed. Erasing stored credentials and starting AP..."));
		WiFi.disconnect(true, true); // disconnect and erase stored credentials
		delay(100);
		startSoftAPIfNeeded();
		addLogf("SoftAP started: %s @ %s", gApSsid.c_str(), WiFi.softAPIP().toString().c_str());
		state = SMODEWIFICONNECTING;
	}
	server.on("/update", HTTP_GET, []() {
		if (!requireOtaAuth()) { otaLog("OTA GET /update unauthorized"); return; }
		otaLog("OTA GET /update page served");
		String page = F("<!DOCTYPE html><html><body><form method='POST' action='/update' enctype='multipart/form-data'><input type='file' name='firmware'>");
		if (server.hasArg("key")) {
			page += String("<input type='hidden' name='key' value='") + server.arg("key") + "'>";
		}
		page += F("<input type='submit' value='Update'></form></body></html>");
		server.send(200, "text/html", page);
	});
	server.on("/update", HTTP_POST,
		[]() {
			if (!requireOtaAuth()) { otaLog("OTA POST unauthorized"); return; }
			bool ok = !Update.hasError();
			otaLogf("OTA POST finalize: %s", ok ? "OK" : "FAIL");
			server.send(200, "text/plain", ok ? "OK" : "FAIL");
			delay(200);
			if (ok) ESP.restart();
		},
		[]() {
			if (!isRequestAuthorized(gOtaSharedKey.c_str())) { otaLog("OTA upload unauthorized (chunk)"); return; }
			HTTPUpload &upload = server.upload();
			static size_t s_ota_total = 0; static size_t s_ota_written = 0; static int s_ota_milestone = 0;
			if (upload.status == UPLOAD_FILE_START) {
				gOtaLog = String();
				otaLogf("OTA start: %s size=%u", upload.filename.c_str(), (unsigned)upload.totalSize);
				EFFECTS_LOCK();
				effects.setBrightness(200);
				effects.setSegment(0, 0, numberLeds, FX_MODE_STATIC, BLACK, 0, false);
				effects.strip.clear();
				effects.strip.setPixelColor(0, effects.Color(255, 255, 255));
				effects.strip.show();
				EFFECTS_UNLOCK();
				// Some clients send multipart with unknown total size (chunked), handle that gracefully
				size_t total = upload.totalSize;
				bool beginOk = (total > 0) ? Update.begin(total) : Update.begin(UPDATE_SIZE_UNKNOWN);
				if (!beginOk) {
					otaLog("OTA begin failed");
					Update.printError(Serial);
				}
				s_ota_total = total; s_ota_written = 0; s_ota_milestone = 0;
			} else if (upload.status == UPLOAD_FILE_WRITE) {
				size_t wrote = Update.write(upload.buf, upload.currentSize);
				if (wrote != upload.currentSize) {
					otaLogf("OTA write mismatch: wrote %u of %u", (unsigned)wrote, (unsigned)upload.currentSize);
				}
				s_ota_written += upload.currentSize;
				if (s_ota_total > 0) {
					int pct = (int)((s_ota_written * 100) / s_ota_total);
					int target = (s_ota_milestone + 1) * 25; // 25,50,75
					if (target <= 75 && pct >= target) { otaLogf("OTA progress: %d%%", target); s_ota_milestone++; }
				}
				static bool t = false; t = !t;
				EFFECTS_LOCK();
				effects.strip.setPixelColor(0, t ? effects.Color(255,255,255) : effects.Color(0,0,0));
				effects.strip.show();
				EFFECTS_UNLOCK();
			} else if (upload.status == UPLOAD_FILE_END) {
				bool ok = Update.end(true);
				if (ok) {
					otaLogf("OTA end OK, size=%u", (unsigned)upload.totalSize);
				} else {
					otaLog("OTA end failed");
					Update.printError(Serial);
				}
				otaLogSaveToFile();
			}
		}
	);
	if (strlen(paramClientIdValue) == 0) {
		strlcpy(paramClientIdValue, "3837bbf0-30fb-47ad-bce8-f460ba9880c3", sizeof(paramClientIdValue));
	}
	if (strlen(paramPollIntervalValue) == 0) {
		strlcpy(paramPollIntervalValue, DEFAULT_POLLING_PRESENCE_INTERVAL, sizeof(paramPollIntervalValue));
	}
	server.on("/generate_204", HTTP_GET, handleCaptivePortalRequest);
	server.on("/gen_204", HTTP_GET, handleCaptivePortalRequest);
	server.on("/hotspot-detect.html", HTTP_GET, handleCaptivePortalRequest);
	server.on("/library/test/success.html", HTTP_GET, handleCaptivePortalRequest);
	server.on("/success.txt", HTTP_GET, handleCaptivePortalRequest);
	server.on("/ncsi.txt", HTTP_GET, handleCaptivePortalRequest);
	server.on("/connecttest.txt", HTTP_GET, handleCaptivePortalRequest);
	server.on("/redirect", HTTP_GET, handleCaptivePortalRequest);
	server.on("/fwlink", HTTP_GET, handleCaptivePortalRequest);
	server.on("/canonical.html", HTTP_GET, handleCaptivePortalRequest);
	server.on("/", HTTP_GET, [] {
		if (gApEnabled && !isAdminRequestAuthorized()) {
			redirectToCaptivePortal();
			return;
		}
		if (!requireAdminAuth()) return;
		handleRoot();
	});
	server.on("/api/health", HTTP_GET, [] {
		JsonDocument d;
		d["ok"] = true;
		d["wifi"] = (int)WiFi.status();
		d["uptime_ms"] = millis();
		d["cpu"] = (int)ESP.getCpuFreqMHz();
		d["heap_free"] = (int)ESP.getFreeHeap();
		server.send(200, "application/json", d.as<String>());
	});
	server.on("/api/ota_last", HTTP_GET, [] {
		if (!requireOtaAuth()) return;
		File f = SPIFFS.open("/ota_last.txt", "r");
		if (!f) { server.send(404, "text/plain", "no_ota_log"); return; }
		String s; while (f.available()) { s += (char)f.read(); }
		f.close();
		server.send(200, "text/plain; charset=utf-8", s);
	});
	server.on("/config", HTTP_GET, [] {
		if (gApEnabled && !isAdminRequestAuthorized()) {
			redirectToCaptivePortal();
			return;
		}
		if (!requireAdminAuth()) return;
		handleConfigUi();
	});
	server.on("/upload", HTTP_GET, [] {
		if (!requireAdminAuth()) return;
		handleMinimalUpload();
	});
	server.on("/fw", HTTP_GET, [](){
		if (!requireOtaAuth()) return;
		handleFirmwareUi();
	});
	server.on("/api/startDevicelogin", HTTP_GET, [] {
		if (!requireAdminAuth()) return;
		handleStartDevicelogin();
	});
	server.on("/api/settings", HTTP_GET, [] {
		if (!requireAdminAuth()) return;
		handleGetSettings();
	});
	server.on("/logs", HTTP_GET, [] {
		if (!requireAdminAuth()) return;
		handleLogsUi();
	});
	server.on("/api/wifi", HTTP_GET, [] {
		if (!requireAdminAuth()) return;
		JsonDocument d;
		d["status"] = (int)WiFi.status();
		d["sta_ip"] = WiFi.localIP().toString();
		d["ssid"] = WiFi.SSID();
		d["ap_ip"] = WiFi.softAPIP().toString();
		d["ap_ssid"] = gApSsid.c_str();
		d["ap_enabled"] = gApEnabled;
		server.send(200, "application/json", d.as<String>());
	});
	server.on("/api/wifi", HTTP_POST, [] {
		if (!requireAdminAuth()) return;
		String body = server.arg("plain");
		JsonDocument doc; DeserializationError err = deserializeJson(doc, body);
		if (err) { server.send(400, "application/json", F("{\"ok\":false,\"error\":\"invalid_json\"}")); return; }
		const char* ssid = doc["ssid"] | "";
		const char* pass = doc["password"] | "";
		if (strlen(ssid) == 0) { server.send(400, "application/json", F("{\"ok\":false,\"error\":\"missing_ssid\"}")); return; }
		WiFi.mode(WIFI_STA);
		WiFi.persistent(true);
		WiFi.begin(ssid, pass);
		unsigned long start = millis();
	while (WiFi.status() != WL_CONNECTED && millis() - start < WIFI_STA_CONNECT_TIMEOUT_MS) {
			delay(250);
		}
		JsonDocument resp;
		if (WiFi.status() == WL_CONNECTED) {
			onWifiConnected();
			resp["ok"] = true;
			resp["ip"] = WiFi.localIP().toString();
			stopSoftAPIfActive();
            addLogf("WiFi connected via /api/wifi: %s", WiFi.localIP().toString().c_str());
		} else {
			resp["ok"] = false;
			resp["status"] = (int)WiFi.status();
            addLog("WiFi connect via /api/wifi failed");
		}
		server.send(200, "application/json", resp.as<String>());
	});
	server.on("/api/wifi_scan", HTTP_GET, [] {
		if (!requireAdminAuth()) return;
		uint32_t startMs = millis();
		int16_t found = WiFi.scanNetworks();
		JsonDocument doc;
		if (found < 0) {
			doc["ok"] = false;
			doc["error"] = "scan_failed";
			doc["count"] = (int)found;
			doc["duration_ms"] = (uint32_t)(millis() - startMs);
			server.send(200, "application/json", doc.as<String>());
			return;
		}
		doc["ok"] = true;
		doc["count"] = (int)found;
		doc["duration_ms"] = (uint32_t)(millis() - startMs);
		JsonArray arr = doc["networks"].to<JsonArray>();
		const int maxNetworks = 20;
		int limit = (found > maxNetworks) ? maxNetworks : found;
		for (int i = 0; i < limit; ++i) {
			JsonObject o = arr.add<JsonObject>();
			o["ssid"] = WiFi.SSID(i);
			o["rssi"] = WiFi.RSSI(i);
			o["secure"] = (WiFi.encryptionType(i) != WIFI_AUTH_OPEN);
		}
		WiFi.scanDelete();
		server.send(200, "application/json", doc.as<String>());
	});
	server.on("/api/ap_start", HTTP_POST, [] {
		if (!requireAdminAuth()) return;
		startSoftAPIfNeeded();
		JsonDocument d; d["ok"] = true; d["ap_ip"] = WiFi.softAPIP().toString(); d["ap_ssid"] = gApSsid.c_str(); d["ap_enabled"] = gApEnabled; server.send(200, "application/json", d.as<String>());
        addLogf("AP started: %s @ %s", gApSsid.c_str(), WiFi.softAPIP().toString().c_str());
	});
	server.on("/api/ap_stop", HTTP_POST, [] {
		if (!requireAdminAuth()) return;
		stopSoftAPIfActive();
		JsonDocument d; d["ok"] = true; d["ap_enabled"] = gApEnabled; server.send(200, "application/json", d.as<String>());
        addLog("AP stopped");
	});
	server.on("/api/reboot", HTTP_POST, [] {
		if (!requireAdminAuth()) return;
		addLog("Reboot requested via API");
		server.send(200, "application/json", F("{\"ok\":true,\"message\":\"Rebooting...\"}"));
		delay(500);  // Give time for response to be sent
		ESP.restart();
	});
	server.on("/api/ap_state", HTTP_GET, [] {
		if (!requireAdminAuth()) return;
		JsonDocument d; d["ap_enabled"] = gApEnabled; d["ap_ip"] = WiFi.softAPIP().toString(); d["ap_ssid"] = gApSsid.c_str(); server.send(200, "application/json", d.as<String>());
	});
	server.on("/api/logs", HTTP_GET, [] {
		if (!requireAdminAuth()) return;
		int n = LOG_CAPACITY;
		if (server.hasArg("n")) {
			int req = atoi(server.arg("n").c_str());
			if (req > 0 && req < LOG_CAPACITY) n = req;
		}
		JsonDocument d;
		JsonArray lines = d["lines"].to<JsonArray>();
		int count = (gLogCount < n) ? gLogCount : n;
		for (int i = count - 1; i >= 0; --i) {
			int idx = (int)gLogHead - 1 - i;
			while (idx < 0) idx += LOG_CAPACITY;
			lines.add((const char*)gLogs[idx]);
		}
		d["count"] = (int)gLogCount;
		d["capacity"] = (int)LOG_CAPACITY;
		d["uptime_ms"] = (unsigned long)millis();
		server.sendHeader("Cache-Control", "no-store, no-cache, must-revalidate, max-age=0");
		server.sendHeader("Pragma", "no-cache");
		server.sendHeader("Expires", "0");
		server.send(200, "application/json", d.as<String>());
	});
	server.on("/api/settings", HTTP_POST, [] {
		if (!requireAdminAuth()) return;
		String body = server.arg("plain");
		JsonDocument doc; DeserializationError err = deserializeJson(doc, body);
		if (err) { server.send(400, "application/json", F("{\"ok\":false,\"error\":\"invalid_json\"}")); return; }
		bool needsReboot = false;
		if (!doc["client_id"].isNull()) { strlcpy(paramClientIdValue, doc["client_id"], sizeof(paramClientIdValue)); }
		if (!doc["tenant"].isNull()) { strlcpy(paramTenantValue, doc["tenant"], sizeof(paramTenantValue)); }
		if (!doc["poll_interval"].isNull()) { snprintf(paramPollIntervalValue, sizeof(paramPollIntervalValue), "%u", (unsigned int)doc["poll_interval"].as<unsigned int>()); }
		if (!doc["led_type_rgbw"].isNull()) {
			bool newType = doc["led_type_rgbw"].as<bool>();
			if (newType != gLedTypeRGBW) {
				gLedTypeRGBW = newType;
				needsReboot = true;  // LED type change requires reboot
			}
		}
		if (!doc["status_led_enabled"].isNull()) {
			gStatusLedEnabled = doc["status_led_enabled"].as<bool>();
			ensureStatusLedReady();
		}
		saveAppConfig();
		JsonDocument resp; 
		resp["ok"] = true; 
		resp["needs_reboot"] = needsReboot;
		server.send(200, "application/json", resp.as<String>());
	});
	server.on("/api/clearSettings", HTTP_GET, [] {
		if (!requireAdminAuth()) return;
		handleClearSettings();
	});
		server.on("/api/effects", HTTP_GET, [] {
			if (!requireAdminAuth()) return;
			JsonDocument doc;
			doc["fade_ms"] = gFadeDurationMs;
			doc["brightness"] = gDefaultBrightness;
			doc["gamma"] = gGamma;
			doc["num_leds"] = numberLeds;
			JsonArray arr = doc["profiles"].to<JsonArray>();
			for (size_t i = 0; i < (sizeof(gProfiles)/sizeof(gProfiles[0])); i++) {
				JsonObject o = arr.add<JsonObject>();
				o["key"] = gProfiles[i].key;
				o["mode"] = gProfiles[i].mode;
				// Expose speed in seconds for UI convenience
				o["speed"] = ((float)gProfiles[i].speed) / 1000.0f;
				o["reverse"] = gProfiles[i].reverse;
				o["color"] = gProfiles[i].color;
				o["fade_ms"] = gProfiles[i].fadeMs;
				o["bri"] = gProfiles[i].bri;
			}
			server.send(200, "application/json", doc.as<String>());
		});
		server.on("/api/effects", HTTP_POST, [] {
			if (!requireAdminAuth()) return;
			String body = server.arg("plain");
			JsonDocument doc;
			DeserializationError err = deserializeJson(doc, body);
			if (err) { server.send(400, "application/json", F("{\"ok\":false,\"error\":\"invalid_json\"}")); return; }
			if (!doc["fade_ms"].isNull()) {
				gFadeDurationMs = (uint16_t)doc["fade_ms"].as<unsigned int>();
			}
			if (!doc["brightness"].isNull()) {
				gDefaultBrightness = (uint8_t)doc["brightness"].as<unsigned int>();
				EFFECTS_LOCK();
				effects.setBrightness(gDefaultBrightness);
				EFFECTS_UNLOCK();
			}
			if (!doc["gamma"].isNull()) {
				gGamma = doc["gamma"].as<float>();
				if (isnan(gGamma) || gGamma < 0.1f) gGamma = 2.2f;
				if (gGamma > 5.0f) gGamma = 5.0f;
			}
			if (!doc["profiles"].isNull() && doc["profiles"].is<JsonArray>()) {
				JsonArray arr = doc["profiles"].as<JsonArray>();
				for (JsonObject o : arr) {
					const char* key = o["key"] | "";
					EffectProfile* p = findProfile(String(key));
					if (p) {
						if (!o["mode"].isNull()) p->mode = (uint16_t)o["mode"].as<unsigned int>();
						// Accept speed in seconds from UI and convert to ms internally
						if (!o["speed"].isNull()) {
							float s = o["speed"].as<float>();
							if (s < 0) s = 0;
							uint32_t ms = (uint32_t)(s * 1000.0f + 0.5f);
							if (ms > 600000) ms = 600000; // clamp to 10 minutes
							p->speed = (uint16_t)ms;
						}
						if (!o["reverse"].isNull()) p->reverse = o["reverse"].as<bool>();
						if (!o["color"].isNull()) p->color = o["color"].as<uint32_t>();
						if (!o["fade_ms"].isNull()) p->fadeMs = (uint16_t)o["fade_ms"].as<unsigned int>();
						if (!o["bri"].isNull()) p->bri = (uint8_t)o["bri"].as<unsigned int>();
					}
				}
			}
			saveEffectsConfig();
			server.send(200, "application/json", F("{\"ok\":true}"));
		});
		server.on("/api/preview", HTTP_POST, [] {
			if (!requireAdminAuth()) return;
			String body = server.arg("plain");
			JsonDocument doc;
			DeserializationError err = deserializeJson(doc, body);
			if (err) { server.send(400, "application/json", F("{\"ok\":false,\"error\":\"invalid_json\"}")); return; }
			uint16_t mode = FX_MODE_STATIC; uint32_t color = RED; uint16_t speed = 3000; bool reverse = false; uint16_t perFade = 0; uint8_t perBri = 0;
			if (!doc["key"].isNull()) {
				EffectProfile* p = findProfile(doc["key"].as<String>());
				if (p) { mode = p->mode; color = p->color; speed = p->speed; reverse = p->reverse; perFade = p->fadeMs; perBri = p->bri; }
			}
			if (!doc["mode"].isNull()) mode = (uint16_t)doc["mode"].as<unsigned int>();
			if (!doc["speed"].isNull()) {
				float s = doc["speed"].as<float>();
				if (s < 0) s = 0;
				uint32_t ms = (uint32_t)(s * 1000.0f + 0.5f);
				if (ms > 600000) ms = 600000;
				speed = (uint16_t)ms;
			}
			if (!doc["reverse"].isNull()) reverse = doc["reverse"].as<bool>();
			if (!doc["color"].isNull()) color = doc["color"].as<uint32_t>();
			if (!doc["fade_ms"].isNull()) perFade = (uint16_t)doc["fade_ms"].as<unsigned int>();
			if (!doc["bri"].isNull()) perBri = (uint8_t)doc["bri"].as<unsigned int>();
			EFFECTS_LOCK();
			gFade.durationMs = perFade;
			gNextTargetBri = perBri;
			EFFECTS_UNLOCK();
			setAnimation(0, mode, color, speed, reverse);
			server.send(200, "application/json", F("{\"ok\":true}"));
		});
		server.on("/api/leds", HTTP_POST, [] {
			if (!requireAdminAuth()) return;
			String body = server.arg("plain");
			JsonDocument doc;
			DeserializationError err = deserializeJson(doc, body);
			if (err) { server.send(400, "application/json", F("{\"ok\":false,\"error\":\"invalid_json\"}")); return; }
			if (doc["num_leds"].isNull()) { server.send(400, "application/json", F("{\"ok\":false,\"error\":\"missing_num_leds\"}")); return; }
			int n = (int)doc["num_leds"].as<int>();
			if (n < 1) n = 1; if (n > 1024) n = 1024;
			numberLeds = n;
			EFFECTS_LOCK();
			effects.setLength(numberLeds);
			EFFECTS_UNLOCK();
		saveAppConfig();
			JsonDocument resp; resp["ok"] = true; resp["num_leds"] = numberLeds; server.send(200, "application/json", resp.as<String>());
		});
		server.on("/api/modes", HTTP_GET, [] {
			if (!requireAdminAuth()) return;
			JsonDocument doc;
			JsonArray arr = doc.to<JsonArray>();
			uint16_t mc = effects.getModeCount();
			for (uint16_t i = 0; i < mc; i++) {
				JsonObject o = arr.add<JsonObject>();
				o["id"] = i;
				o["name"] = effects.getModeName(i);
			}
			server.send(200, "application/json", doc.as<String>());
		});
		server.on("/api/preview_state", HTTP_GET, [] {
			if (!requireAdminAuth()) return;
			JsonDocument d; d["enabled"] = gPreviewMode; d["key"] = gPreviewKey.c_str(); server.send(200, "application/json", d.as<String>());
		});
		server.on("/api/preview_mode", HTTP_POST, [] {
			if (!requireAdminAuth()) return;
			String body = server.arg("plain");
			JsonDocument doc; DeserializationError err = deserializeJson(doc, body);
			if (err) { server.send(400, "application/json", F("{\"ok\":false,\"error\":\"invalid_json\"}")); return; }
			bool en = doc["enabled"].as<bool>();
			gPreviewMode = en;
			if (gPreviewMode) {
				applyPreviewSelection();
			} else {
				setPresenceAnimation();
			}
			JsonDocument resp; resp["ok"] = true; resp["enabled"] = gPreviewMode; resp["key"] = gPreviewKey.c_str(); server.send(200, "application/json", resp.as<String>());
		});
		server.on("/api/preview_select", HTTP_POST, [] {
			if (!requireAdminAuth()) return;
			String body = server.arg("plain");
			JsonDocument doc; DeserializationError err = deserializeJson(doc, body);
			if (err) { server.send(400, "application/json", F("{\"ok\":false,\"error\":\"invalid_json\"}")); return; }
			if (doc["key"].isNull()) { server.send(400, "application/json", F("{\"ok\":false,\"error\":\"missing_key\"}")); return; }
			String k = doc["key"].as<String>();
			if (!findProfile(k)) { server.send(404, "application/json", F("{\"ok\":false,\"error\":\"unknown_key\"}")); return; }
			gPreviewKey = k;
			if (gPreviewMode) applyPreviewSelection();
			JsonDocument resp; resp["ok"] = true; resp["enabled"] = gPreviewMode; resp["key"] = gPreviewKey.c_str(); server.send(200, "application/json", resp.as<String>());
		});
		server.on("/api/current", HTTP_GET, [] {
			if (!requireAdminAuth()) return;
			JsonDocument d;
			d["activity"] = activity.c_str();
			d["mode"] = gTarget.mode;
			d["color"] = gTarget.color;
			// Report current target speed in seconds
			d["speed"] = ((float)gTarget.speed) / 1000.0f;
			d["reverse"] = gTarget.reverse;
			d["brightness"] = effects.getBrightness();
			server.send(200, "application/json", d.as<String>());
		});
	server.on("/fs/delete", HTTP_DELETE, []() {
		if (!requireAdminAuth()) return;
		handleFileDelete();
	});
	server.on("/fs/list", HTTP_GET, []() {
		if (!requireAdminAuth()) return;
		handleFileList();
	});
	server.on("/fs/upload", HTTP_POST, []() {
		if (!requireAdminAuth()) return;
		server.send(200, "text/plain", "");
	}, handleFileUpload);
		server.on("/effects", HTTP_GET, []() {
			if (!requireAdminAuth()) return;
			handleEffectsUi();
		});
	server.onNotFound([]() {
		if (shouldRedirectToCaptivePortal()) {
			handleCaptivePortalRequest();
			return;
		}
		if (!handleFileRead(server.uri())) {
			server.send(404, "text/plain", "FileNotFound");
		}
	});
	server.begin();
	DBG_PRINTLN(F("setup() ready..."));
	xTaskCreatePinnedToCore(
		neopixelTask,
		"Neopixels",
		2048,
		NULL,
		3,
		&TaskNeopixel,
		LED_TASK_CORE);
}

// Update status LED based on current state
void updateStatusLed() {
	if (!gStatusLedEnabled) {
		setStatusLedColor(0);
		return;
	}

	ensureStatusLedReady();
	if (!gStatusLed) return;
	
	uint32_t color = 0;
	static unsigned long lastBlink = 0;
	static bool blinkState = false;
	unsigned long now = millis();
	
	// Choose color based on state
	switch (state) {
		case SMODEINITIAL:
		case SMODEWIFICONNECTING:
			// Blinking yellow - connecting
			if (now - lastBlink > 500) {
				blinkState = !blinkState;
				lastBlink = now;
			}
			color = blinkState ? 0x202000 : 0x000000; // Dim yellow
			break;
			
		case SMODEWIFICONNECTED:
		case SMODEDEVICELOGINSTARTED:
			// Cyan - WiFi connected, waiting for auth
			color = 0x002020;
			break;
			
		case SMODEDEVICELOGINFAILED:
			// Blinking red - login failed
			if (now - lastBlink > 300) {
				blinkState = !blinkState;
				lastBlink = now;
			}
			color = blinkState ? 0x200000 : 0x000000; // Dim red
			break;
			
		case SMODEAUTHREADY:
		case SMODEPOLLPRESENCE:
			// Green - authenticated and polling
			color = 0x002000;
			break;
			
		case SMODEREFRESHTOKEN:
			// Blinking cyan - refreshing token
			if (now - lastBlink > 700) {
				blinkState = !blinkState;
				lastBlink = now;
			}
			color = blinkState ? 0x002020 : 0x000000;
			break;
			
		case SMODEPRESENCEREQUESTERROR:
			// Orange - presence request error
			color = 0x201000;
			break;
			
		default:
			color = 0x100010; // Dim magenta for unknown state
			break;
	}
	
	setStatusLedColor(color);
}

void loop()
{
	if (gApEnabled) dnsServer.processNextRequest();
	server.handleClient();
	statemachine();
	updateStatusLed();
}
