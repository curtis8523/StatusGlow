#include "config.h"

extern void syncTime();
extern bool gApEnabled;
extern String gApSsid;
extern bool gStatusLedEnabled;

#ifndef DISABLECERTCHECK
static const char rootCACertificate[] PROGMEM =
"-----BEGIN CERTIFICATE-----\n"
"MIIE6DCCA9CgAwIBAgIQAnQuqhfKjiHHF7sf/P0MoDANBgkqhkiG9w0BAQsFADBh\n"
"MQswCQYDVQQGEwJVUzEVMBMGA1UEChMMRGlnaUNlcnQgSW5jMRkwFwYDVQQLExB3\n"
"d3cuZGlnaWNlcnQuY29tMSAwHgYDVQQDExdEaWdpQ2VydCBHbG9iYWwgUm9vdCBD\n"
"QTAeFw0yMDA5MjMwMDAwMDBaFw0zMDA5MjIyMzU5NTlaME0xCzAJBgNVBAYTAlVT\n"
"MRUwEwYDVQQKEwxEaWdpQ2VydCBJbmMxJzAlBgNVBAMTHkRpZ2lDZXJ0IFNIQTIg\n"
"U2VjdXJlIFNlcnZlciBDQTCCASIwDQYJKoZIhvcNAQEBBQADggEPADCCAQoCggEB\n"
"ANyuWJBNwcQwFZA1W248ghX1LFy949v/cUP6ZCWA1O4Yok3wZtAKc24RmDYXZK83\n"
"nf36QYSvx6+M/hpzTc8zl5CilodTgyu5pnVILR1WN3vaMTIa16yrBvSqXUu3R0bd\n"
"KpPDkC55gIDvEwRqFDu1m5K+wgdlTvza/P96rtxcflUxDOg5B6TXvi/TC2rSsd9f\n"
"/ld0Uzs1gN2ujkSYs58O09rg1/RrKatEp0tYhG2SS4HD2nOLEpdIkARFdRrdNzGX\n"
"kujNVA075ME/OV4uuPNcfhCOhkEAjUVmR7ChZc6gqikJTvOX6+guqw9ypzAO+sf0\n"
"/RR3w6RbKFfCs/mC/bdFWJsCAwEAAaOCAa4wggGqMB0GA1UdDgQWBBQPgGEcgjFh\n"
"1S8o541GOLQs4cbZ4jAfBgNVHSMEGDAWgBQD3lA1VtFMu2bwo+IbG8OXsj3RVTAO\n"
"BgNVHQ8BAf8EBAMCAYYwHQYDVR0lBBYwFAYIKwYBBQUHAwEGCCsGAQUFBwMCMBIG\n"
"A1UdEwEB/wQIMAYBAf8CAQAwdgYIKwYBBQUHAQEEajBoMCQGCCsGAQUFBzABhhho\n"
"dHRwOi8vb2NzcC5kaWdpY2VydC5jb20wQAYIKwYBBQUHMAKGNGh0dHA6Ly9jYWNl\n"
"cnRzLmRpZ2ljZXJ0LmNvbS9EaWdpQ2VydEdsb2JhbFJvb3RDQS5jcnQwewYDVR0f\n"
"BHQwcjA3oDWgM4YxaHR0cDovL2NybDMuZGlnaWNlcnQuY29tL0RpZ2lDZXJ0R2xv\n"
"YmFsUm9vdENBLmNybDA3oDWgM4YxaHR0cDovL2NybDQuZGlnaWNlcnQuY29tL0Rp\n"
"Z2lDZXJ0R2xvYmFsUm9vdENBLmNybDAwBgNVHSAEKTAnMAcGBWeBDAEBMAgGBmeB\n"
"DAECATAIBgZngQwBAgIwCAYGZ4EMAQIDMA0GCSqGSIb3DQEBCwUAA4IBAQB3MR8I\n"
"l9cSm2PSEWUIpvZlubj6kgPLoX7hyA2MPrQbkb4CCF6fWXF7Ef3gwOOPWdegUqHQ\n"
"S1TSSJZI73fpKQbLQxCgLzwWji3+HlU87MOY7hgNI+gH9bMtxKtXc1r2G1O6+x/6\n"
"vYzTUVEgR17vf5irF0LKhVyfIjc0RXbyQ14AniKDrN+v0ebHExfppGlkTIBn6rak\n"
"f4994VH6npdn6mkus5CkHBXIrMtPKex6XF2firjUDLuU7tC8y7WlHgjPxEEDDb0G\n"
"w6D0yDdVSvG/5XlCNatBmO/8EznDu1vr72N8gJzISUZwa6CCUD7QBLbKJcXBBVVf\n"
"8nwvV9GvlW+sbXlr\n"
"-----END CERTIFICATE-----\n";
#endif

boolean requestJsonApi(JsonDocument& doc, String url, String payload = "", size_t capacity = 0, String type = "POST", boolean sendAuth = false) {
	time_t now = time(nullptr);
	if (now < 1609459200) {
		DBG_PRINTLN(F("[HTTPS] Time not set; syncing via NTP..."));
		syncTime();
	}
	extern void addLogf(const char*, ...);
	WiFiClientSecure tls;
#ifndef DISABLECERTCHECK
	tls.setCACert(rootCACertificate);
#else
	tls.setInsecure();
#endif

	HTTPClient https;
	if (https.begin(tls, url)) {
		https.setConnectTimeout(10000);
		https.setTimeout(10000);
		https.setFollowRedirects(HTTPC_FORCE_FOLLOW_REDIRECTS);

		if (sendAuth) {
			String header;
			header.reserve(access_token.length() + 8);
			header += F("Bearer ");
			header += access_token;
			https.addHeader("Authorization", header);
			DBG_PRINT("[HTTPS] Auth token valid for "); DBG_PRINT(getTokenLifetime()); DBG_PRINTLN(" s.");
		}

		int httpCode = (type == "POST") ? https.POST(payload) : https.GET();
		if (httpCode > 0) {
			DBG_PRINT("[HTTPS] Method: "); DBG_PRINT(type.c_str()); DBG_PRINT(", Response code: "); DBG_PRINTLN(httpCode);
			if (httpCode == HTTP_CODE_OK || httpCode == HTTP_CODE_MOVED_PERMANENTLY || httpCode == HTTP_CODE_BAD_REQUEST) {
				String body = https.getString();
				https.end();
				DeserializationError error = deserializeJson(doc, body);
				if (error) {
					DBG_PRINT(F("deserializeJson() failed: "));
					DBG_PRINTLN(error.c_str());
					return false;
				}
				return true;
			}
			DBG_PRINT("[HTTPS] Other HTTP code: "); DBG_PRINTLN(httpCode); DBG_PRINT("Response: ");
			DBG_PRINTLN(https.getString());
			https.end();
			return false;
		}

		DBG_PRINT("[HTTPS] Request failed: "); DBG_PRINTLN(https.errorToString(httpCode).c_str());
		https.end();
		addLogf("HTTPS request failed: %d", httpCode);
		return false;
	}

	DBG_PRINTLN(F("[HTTPS] Unable to connect"));
	return false;
}

static inline String htmlEscape(const String& in) {
	String out;
	out.reserve(in.length());
	for (size_t i = 0; i < in.length(); i++) {
		char c = in.charAt(i);
		if (c == '&') out += F("&amp;");
		else if (c == '<') out += F("&lt;");
		else if (c == '>') out += F("&gt;");
		else if (c == '"') out += F("&quot;");
		else out += c;
	}
	return out;
}

void handleGetSettings() {
	DBG_PRINTLN("handleGetSettings()");

	JsonDocument responseDoc;
	responseDoc["client_id"].set(paramClientIdValue);
	responseDoc["tenant"].set(paramTenantValue);
	responseDoc["poll_interval"].set(paramPollIntervalValue);
	responseDoc["num_leds"].set(numberLeds);
	responseDoc["led_type_rgbw"].set(gLedTypeRGBW);
	responseDoc["status_led_enabled"].set(gStatusLedEnabled);
	responseDoc["heap"].set(ESP.getFreeHeap());
	responseDoc["heap_total"].set(ESP.getHeapSize());
	responseDoc["min_heap"].set(ESP.getMinFreeHeap());
	responseDoc["sketch_size"].set(ESP.getSketchSize());
	responseDoc["free_sketch_space"].set(ESP.getFreeSketchSpace());
	responseDoc["flash_chip_size"].set(ESP.getFlashChipSize());
	responseDoc["flash_chip_speed"].set(ESP.getFlashChipSpeed());
	responseDoc["sdk_version"].set(ESP.getSdkVersion());
	responseDoc["cpu_freq"].set(ESP.getCpuFreqMHz());
	responseDoc["uptime_ms"].set(millis());
	responseDoc["wifi_rssi"].set(WiFi.RSSI());
	if (WiFi.SSID().length() > 0) responseDoc["wifi_ssid"].set(WiFi.SSID());
	else responseDoc["wifi_ssid"].set(paramWifiSsidValue);
	responseDoc["wifi_saved_ssid"].set(paramWifiSsidValue);
	responseDoc["wifi_ip"].set(WiFi.localIP().toString());
	responseDoc["wifi_status"].set((int)WiFi.status());
	responseDoc["ap_ip"].set(WiFi.softAPIP().toString());
	responseDoc["ap_ssid"].set(gApSsid.c_str());
	responseDoc["ap_enabled"].set(gApEnabled);
	responseDoc["host_name"].set(gThingHostName.c_str());
	responseDoc["host_local"].set(String(gThingHostName) + ".local");
	extern uint8_t getCpuUsagePercent();
	responseDoc["cpu_usage"].set(getCpuUsagePercent());
	responseDoc["sketch_version"].set(VERSION);

	JsonObject ui = responseDoc["ui"].to<JsonObject>();
	ui["cpu_ok"] = UI_CPU_OK;
	ui["cpu_warn"] = UI_CPU_WARN;
	ui["heap_ok"] = UI_HEAP_OK;
	ui["heap_warn"] = UI_HEAP_WARN;
	JsonArray rssi = ui["rssi"].to<JsonArray>();
	rssi.add((int)UI_RSSI_B0); rssi.add((int)UI_RSSI_B1); rssi.add((int)UI_RSSI_B2); rssi.add((int)UI_RSSI_B3);
	JsonArray bh = ui["bar_heights"].to<JsonArray>();
	bh.add((int)UI_BAR_H0); bh.add((int)UI_BAR_H1); bh.add((int)UI_BAR_H2); bh.add((int)UI_BAR_H3);
	ui["min_refresh_ms"] = UI_MIN_REFRESH_MS;

	sendJsonDocument(200, responseDoc);
}

void handleClearSettings() {
	DBG_PRINTLN("handleClearSettings()");
	SPIFFS.remove(CONTEXT_FILE);
	SPIFFS.remove(CONFIG_FILE);
	SPIFFS.remove(EFFECTS_FILE);
	SPIFFS.remove("/rizz.json");
	memset(paramClientIdValue, 0, sizeof(paramClientIdValue));
	memset(paramTenantValue, 0, sizeof(paramTenantValue));
	memset(paramWifiSsidValue, 0, sizeof(paramWifiSsidValue));
	memset(paramWifiPasswordValue, 0, sizeof(paramWifiPasswordValue));
	strlcpy(paramPollIntervalValue, DEFAULT_POLLING_PRESENCE_INTERVAL, sizeof(paramPollIntervalValue));
	removePrefsKey(PREF_APP_CONFIG);
	removePrefsKey(PREF_AUTH_CONTEXT);
	clearWifiPrefs();
	WiFi.persistent(true);
	WiFi.disconnect(false, true);
	sendApiOk(200);
	ESP.restart();
}

void handleStartDevicelogin() {
	if (state != SMODEDEVICELOGINSTARTED) {
		DBG_PRINTLN(F("handleStartDevicelogin()"));
		String clientId = String(paramClientIdValue);
		clientId.trim();
		String tenant = String(paramTenantValue);
		tenant.trim();
		if (clientId.isEmpty()) {
			sendApiError(400, "missing_client_id", "Set and save the Microsoft Client ID before starting device login.");
			return;
		}
		if (tenant.isEmpty()) {
			sendApiError(400, "missing_tenant", "Set and save the Microsoft tenant before starting device login.");
			return;
		}
		JsonDocument doc;
		String payload;
		payload.reserve(clientId.length() + device_code.length() + 96);
		payload += F("client_id=");
		payload += clientId;
		payload += F("&scope=offline_access%20openid%20Presence.Read");
		boolean res = requestJsonApi(
			doc,
			"https://login.microsoftonline.com/" + tenant + "/oauth2/v2.0/devicecode",
			payload,
			0
		);

		if (res && !doc["device_code"].isNull() && !doc["user_code"].isNull() && !doc["interval"].isNull() &&
			!doc["verification_uri"].isNull() && !doc["message"].isNull()) {
			device_code = doc["device_code"].as<String>();
			user_code = doc["user_code"].as<String>();
			device_login_verification_uri = doc["verification_uri"].as<String>();
			device_login_verification_uri_complete = doc["verification_uri_complete"].isNull() ? "" : doc["verification_uri_complete"].as<String>();
			device_login_message = doc["message"].as<String>();
			interval = doc["interval"].as<unsigned int>();

			JsonDocument responseDoc;
			responseDoc["user_code"] = user_code;
			responseDoc["verification_uri"] = device_login_verification_uri;
			if (device_login_verification_uri_complete.length()) {
				responseDoc["verification_uri_complete"] = device_login_verification_uri_complete;
			}
			responseDoc["message"] = device_login_message;
			gDeviceLoginTransientFailures = 0;
			state = SMODEDEVICELOGINSTARTED;
			tsPolling = millis() + (interval * 1000);
			sendJsonDocument(200, responseDoc);
		} else if (res && !doc["error"].isNull()) {
			JsonDocument responseDoc;
			responseDoc["ok"] = false;
			responseDoc["error"] = doc["error"].as<String>();
			if (!doc["error_description"].isNull()) {
				responseDoc["message"] = doc["error_description"].as<String>();
			} else if (!doc["message"].isNull()) {
				responseDoc["message"] = doc["message"].as<String>();
			}
			sendJsonDocument(400, responseDoc);
		} else {
			sendApiError(502, "devicelogin_unknown_response", "Microsoft device login did not return a usable device code response.");
		}
	} else {
		JsonDocument responseDoc;
		responseDoc["ok"] = false;
		responseDoc["error"] = "devicelogin_already_running";
		responseDoc["message"] = "A device login is already waiting for approval.";
		if (user_code.length()) responseDoc["user_code"] = user_code;
		if (device_login_verification_uri.length()) responseDoc["verification_uri"] = device_login_verification_uri;
		if (device_login_verification_uri_complete.length()) responseDoc["verification_uri_complete"] = device_login_verification_uri_complete;
		if (device_login_message.length()) responseDoc["message"] = device_login_message;
		sendJsonDocument(409, responseDoc);
	}
}
