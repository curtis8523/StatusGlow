#include "config.h"

extern void syncTime();
extern bool gApEnabled;
extern String gApSsid;
extern bool gStatusLedEnabled;
extern bool gRelayTlsInsecure;
extern char paramPresenceSourceValue[];
extern char paramRelayUrlValue[];
extern char paramRelayDeviceIdValue[];
extern char paramRelayDeviceKeyValue[];
extern const uint8_t x509_crt_bundle[];

boolean requestJsonApi(JsonDocument& doc, String url, String payload = "", size_t capacity = 0, String type = "POST", boolean sendAuth = false, String extraHeaderName = "", String extraHeaderValue = "", bool allowInsecureTls = false) {
	time_t now = time(nullptr);
	if (now < 1609459200) {
		DBG_PRINTLN(F("[HTTPS] Time not set; syncing via NTP..."));
		syncTime();
	}
	extern void addLogf(const char*, ...);
	if (url.startsWith("http://")) {
		WiFiClient client;
		HTTPClient https;
		if (!https.begin(client, url)) {
			DBG_PRINTLN(F("[HTTP] Unable to connect"));
			return false;
		}
		https.setConnectTimeout(10000);
		https.setTimeout(10000);
		https.setFollowRedirects(HTTPC_FORCE_FOLLOW_REDIRECTS);
		https.addHeader("Accept", "application/json");
		if (type == "POST") {
			https.addHeader("Content-Type", "application/x-www-form-urlencoded");
		}
		if (sendAuth) {
			String header;
			header.reserve(access_token.length() + 8);
			header += F("Bearer ");
			header += access_token;
			https.addHeader("Authorization", header);
			DBG_PRINT("[HTTP] Auth token valid for "); DBG_PRINT(getTokenLifetime()); DBG_PRINTLN(" s.");
		}
		if (extraHeaderName.length() > 0) {
			https.addHeader(extraHeaderName, extraHeaderValue);
		}

		int httpCode = (type == "POST") ? https.POST(payload) : https.GET();
		if (httpCode <= 0) {
			DBG_PRINT("[HTTP] Request failed: "); DBG_PRINTLN(https.errorToString(httpCode).c_str());
			addLogf("HTTP request failed: %d (%s)", httpCode, https.errorToString(httpCode).c_str());
			https.end();
			return false;
		}

		DBG_PRINT("[HTTP] Method: "); DBG_PRINT(type.c_str()); DBG_PRINT(", Response code: "); DBG_PRINTLN(httpCode);
		String body = https.getString();
		https.end();
		if (body.length() == 0) {
			return false;
		}
		DeserializationError error = deserializeJson(doc, body);
		if (error) {
			DBG_PRINT(F("deserializeJson() failed: "));
			DBG_PRINTLN(error.c_str());
			DBG_PRINT(F("[HTTP] Raw body: "));
			DBG_PRINTLN(body);
			return false;
		}
		doc["_http_status"] = httpCode;
		return true;
	}

	const bool allowInsecureRetry =
		allowInsecureTls ||
		url.startsWith("https://login.microsoftonline.com/") ||
		url.startsWith("https://graph.microsoft.com/");

	auto performRequest = [&](bool insecure, bool isRetry) -> bool {
		WiFiClientSecure tls;
#ifndef DISABLECERTCHECK
		if (insecure) {
			tls.setInsecure();
		} else {
			tls.setCACertBundle(x509_crt_bundle);
		}
#else
		(void)insecure;
		tls.setInsecure();
#endif
		tls.setHandshakeTimeout(20);

		HTTPClient https;
		if (!https.begin(tls, url)) {
			DBG_PRINTLN(F("[HTTPS] Unable to connect"));
			return false;
		}

		https.setConnectTimeout(10000);
		https.setTimeout(10000);
		https.setFollowRedirects(HTTPC_FORCE_FOLLOW_REDIRECTS);
		https.addHeader("Accept", "application/json");
		if (type == "POST") {
			https.addHeader("Content-Type", "application/x-www-form-urlencoded");
		}

		if (sendAuth) {
			String header;
			header.reserve(access_token.length() + 8);
			header += F("Bearer ");
			header += access_token;
			https.addHeader("Authorization", header);
			DBG_PRINT("[HTTPS] Auth token valid for "); DBG_PRINT(getTokenLifetime()); DBG_PRINTLN(" s.");
		}
		if (extraHeaderName.length() > 0) {
			https.addHeader(extraHeaderName, extraHeaderValue);
		}

		int httpCode = (type == "POST") ? https.POST(payload) : https.GET();
		if (httpCode <= 0) {
			DBG_PRINT("[HTTPS] Request failed: "); DBG_PRINTLN(https.errorToString(httpCode).c_str());
			char tlsError[128] = {0};
			int tlsCode = tls.lastError(tlsError, sizeof(tlsError));
			if (tlsCode != 0) {
				DBG_PRINT("[HTTPS] TLS error: "); DBG_PRINT(tlsCode); DBG_PRINT(" ");
				DBG_PRINTLN(tlsError);
				addLogf("HTTPS request failed%s: %d (%s), TLS %d: %s",
					isRetry ? " [retry]" : "",
					httpCode,
					https.errorToString(httpCode).c_str(),
					tlsCode,
					tlsError);
			} else {
				addLogf("HTTPS request failed%s: %d (%s)",
					isRetry ? " [retry]" : "",
					httpCode,
					https.errorToString(httpCode).c_str());
			}
			https.end();
			return false;
		}

		DBG_PRINT("[HTTPS] Method: "); DBG_PRINT(type.c_str()); DBG_PRINT(", Response code: "); DBG_PRINTLN(httpCode);
		String body = https.getString();
		https.end();
		if (body.length() == 0) {
			DBG_PRINT("[HTTPS] Empty response body for HTTP "); DBG_PRINTLN(httpCode);
			return false;
		}

		DeserializationError error = deserializeJson(doc, body);
		if (error) {
			DBG_PRINT(F("deserializeJson() failed: "));
			DBG_PRINTLN(error.c_str());
			DBG_PRINT(F("[HTTPS] Raw body: "));
			DBG_PRINTLN(body);
			return false;
		}
		doc["_http_status"] = httpCode;
		if (insecure) {
			doc["_tls_insecure_retry"] = true;
		}
		return true;
	};

#ifndef DISABLECERTCHECK
	bool success = performRequest(false, false);
	if (!success && allowInsecureRetry) {
		addLog("Retrying HTTPS request without certificate validation");
		doc.clear();
		success = performRequest(true, true);
	}
	return success;
#else
	return performRequest(true, false);
#endif
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

static inline bool isFormSafeChar(char c) {
	return (c >= 'A' && c <= 'Z') ||
		(c >= 'a' && c <= 'z') ||
		(c >= '0' && c <= '9') ||
		c == '-' || c == '_' || c == '.' || c == '~';
}

static inline String urlEncodeFormValue(const String& in) {
	static const char hex[] = "0123456789ABCDEF";
	String out;
	out.reserve((in.length() * 3) + 1);
	for (size_t i = 0; i < in.length(); ++i) {
		unsigned char c = (unsigned char)in.charAt(i);
		if (isFormSafeChar((char)c)) {
			out += (char)c;
		} else if (c == ' ') {
			out += '+';
		} else {
			out += '%';
			out += hex[(c >> 4) & 0x0F];
			out += hex[c & 0x0F];
		}
	}
	return out;
}

void handleGetSettings() {
	DBG_PRINTLN("handleGetSettings()");

	JsonDocument responseDoc;
	responseDoc["client_id"].set(paramClientIdValue);
	responseDoc["tenant"].set(paramTenantValue);
	responseDoc["poll_interval"].set(paramPollIntervalValue);
	responseDoc["presence_source"].set(paramPresenceSourceValue);
	responseDoc["relay_url"].set(paramRelayUrlValue);
	responseDoc["relay_device_id"].set(paramRelayDeviceIdValue);
	responseDoc["relay_device_key"].set(paramRelayDeviceKeyValue);
	responseDoc["relay_tls_insecure"].set(gRelayTlsInsecure);
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
	time_t now = time(nullptr);
	if (now >= 1609459200) {
		struct tm utcTime;
		char timeBuf[32];
		gmtime_r(&now, &utcTime);
		strftime(timeBuf, sizeof(timeBuf), "%Y-%m-%d %H:%M:%S UTC", &utcTime);
		responseDoc["device_time"].set(timeBuf);
		responseDoc["device_time_unix"].set((int64_t)now);
	} else {
		responseDoc["device_time"].set("Not synced");
	}

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
	memset(paramClientIdValue, 0, sizeof(paramClientIdValue));
	memset(paramTenantValue, 0, sizeof(paramTenantValue));
	strlcpy(paramPresenceSourceValue, DEFAULT_PRESENCE_SOURCE, sizeof(paramPresenceSourceValue));
	memset(paramRelayUrlValue, 0, sizeof(paramRelayUrlValue));
	memset(paramRelayDeviceIdValue, 0, sizeof(paramRelayDeviceIdValue));
	memset(paramRelayDeviceKeyValue, 0, sizeof(paramRelayDeviceKeyValue));
	gRelayTlsInsecure = DEFAULT_RELAY_TLS_INSECURE;
	memset(paramWifiSsidValue, 0, sizeof(paramWifiSsidValue));
	memset(paramWifiPasswordValue, 0, sizeof(paramWifiPasswordValue));
	strlcpy(paramPollIntervalValue, DEFAULT_POLLING_PRESENCE_INTERVAL, sizeof(paramPollIntervalValue));
	removePrefsKey(PREF_APP_CONFIG);
	removePrefsKey(PREF_AUTH_CONTEXT);
	removePrefsKey(PREF_CLIENT_ID);
	removePrefsKey(PREF_TENANT_ID);
	removePrefsKey(PREF_NUM_LEDS);
	removePrefsKey(PREF_POLL_INTERVAL);
	removePrefsKey("ota_last_log");
	clearWifiPrefs();
	WiFi.persistent(true);
	WiFi.disconnect(false, true);
	sendApiOk(200);
	ESP.restart();
}

void handleStartDevicelogin() {
	if (String(paramPresenceSourceValue).equalsIgnoreCase("relay")) {
		sendApiError(409, "relay_mode_enabled", "Device login is disabled while the presence source is set to relay.");
		return;
	}
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
		String encodedClientId = urlEncodeFormValue(clientId);
		payload.reserve(encodedClientId.length() + 96);
		payload += F("client_id=");
		payload += encodedClientId;
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
			String detail = "Microsoft device login did not return a usable device code response.";
			if (!doc["_http_status"].isNull()) {
				detail += " HTTP ";
				detail += doc["_http_status"].as<int>();
				detail += ".";
			}
			addLogf("Device login start failed: %s", detail.c_str());
			sendApiError(502, "devicelogin_unknown_response", detail.c_str());
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
