#include "config.h"

extern void syncTime();
// Access SoftAP state exposed in main.cpp
extern bool gApEnabled;
extern String gApSsid;
extern bool gStatusLedEnabled;  // Status LED setting

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
	// Only log errors, not successful requests
	// addLogf("HTTPS %s %s", type.c_str(), url.c_str());
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
			String header = "Bearer " + access_token;
			https.addHeader("Authorization", header);
			DBG_PRINT("[HTTPS] Auth token valid for "); DBG_PRINT(getTokenLifetime()); DBG_PRINTLN(" s.");
		}

		int httpCode = (type == "POST") ? https.POST(payload) : https.GET();
		if (httpCode > 0) {
			DBG_PRINT("[HTTPS] Method: "); DBG_PRINT(type.c_str()); DBG_PRINT(", Response code: "); DBG_PRINTLN(httpCode);
			// Only log errors, not successful requests
			// addLogf("HTTPS code: %d", httpCode);
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
			} else {
				DBG_PRINT("[HTTPS] Other HTTP code: "); DBG_PRINTLN(httpCode); DBG_PRINT("Response: ");
				DBG_PRINTLN(https.getString());
				https.end();
				return false;
			}
		} else {
			DBG_PRINT("[HTTPS] Request failed: "); DBG_PRINTLN(https.errorToString(httpCode).c_str());
			https.end();
			addLogf("HTTPS request failed: %d", httpCode);
			return false;
		}
	} else {
		DBG_PRINTLN(F("[HTTPS] Unable to connect"));
		return false;
	}
}


// Shared, minified CSS and helper for page heads (kept in PROGMEM to save RAM)
static const char COMMON_STYLE[] PROGMEM =
":root{--bg:#f8fafc;--fg:#0e1a2b;--muted:#5b6b7f;--card-bg:#fff;--card-border:#d0d8e6;--border:#d0d8e6;--btn-bg:#eef2f8;--btn-fg:#0e1a2b;--accent:#2fcf7a;--accent-soft:#2b8f73;--warn:#ffd166;--crit:#ff6b6b;--mono-bg:#0f172a;--mono-fg:#a6ffcf}html[data-theme='dark']{--bg:#0b1220;--fg:#e8eef6;--muted:#a0b3c6;--card-bg:#0f1628;--card-border:#24324a;--border:#24324a;--btn-bg:#1b2436;--btn-fg:#e8eef6;--accent:#39e37d;--accent-soft:#2b8f73;--warn:#ffd166;--crit:#ff6b6b;--mono-bg:#0b0f1a;--mono-fg:#9cffbd}*,*::before,*::after{box-sizing:border-box}body{font-family:system-ui,-apple-system,Segoe UI,Roboto,Arial,sans-serif;margin:0;background:var(--bg);color:var(--fg);text-align:center;line-height:1.5}h1,h2,h3{line-height:1.2;margin:.25rem 0 .5rem}h1{font-size:clamp(1.2rem,2.2vw,1.6rem)}h2{font-size:clamp(1.1rem,2vw,1.4rem)}h3{font-size:clamp(1rem,1.8vw,1.2rem);color:var(--muted)}.container{max-width:1280px;width:min(95vw,1280px);margin:0 auto;text-align:left;padding:16px 20px}.brand{margin:.5rem 0 1rem}.brand-row{display:flex;align-items:center;justify-content:space-between;gap:.75rem}.brand-left{display:flex;align-items:center;gap:.5rem}.brand-img{height:22px;width:auto;display:inline-block}.brand-logo{width:22px;height:22px;border-radius:50%;background:radial-gradient(circle at 30% 30%, var(--accent), var(--accent-soft));display:none;border:1px solid rgba(0,0,0,.08)}.brand-title{font-weight:700;letter-spacing:.2px}.brand-sub{color:var(--muted)}.brand-ver{color:var(--muted);font-variant-numeric:tabular-nums}.tabs{display:flex;flex-wrap:wrap;justify-content:center;gap:.5rem;margin:.25rem auto 1rem}.tab{display:inline-block;padding:.5rem .75rem;border:1px solid var(--card-border);border-bottom:none;border-radius:.5rem .5rem 0 0;background:var(--btn-bg);text-decoration:none;color:var(--btn-fg)}.tab.active{background:var(--card-bg);font-weight:600}.card{border:1px solid var(--card-border);border-radius:.5rem;background:var(--card-bg);padding:1rem}.row{margin:.6rem 0}.row strong{display:inline-block;min-width:160px}.ml-s{margin-left:1rem}.mt-s{margin-top:1rem}.mt{margin-top:2rem}.btn{display:inline-block;padding:.45rem .8rem;border:1px solid var(--card-border);border-radius:.4rem;background:var(--btn-bg);color:var(--btn-fg);text-decoration:none;cursor:pointer}.btn:active{transform:translateY(1px)}.topbar{display:flex;flex-wrap:wrap;gap:.5rem;align-items:center;justify-content:center;padding:.35rem .6rem;margin-bottom:1rem;border:1px solid var(--border);border-radius:.5rem;background:var(--card-bg)}.pill{display:inline-block;padding:.1rem .5rem;border-radius:999px;min-width:2.5rem;text-align:center;font-variant-numeric:tabular-nums;border:1px solid transparent}.pill-ok{background:#e8f5e9;border-color:#c8e6c9;color:#1b5e20}.pill-warn{background:#fff8e1;border-color:#ffe0b2;color:#e65100}.pill-crit{background:#ffebee;border-color:#ffcdd2;color:#b71c1c}.bars{display:inline-flex;gap:2px;vertical-align:middle;align-items:flex-end;--bar-color:var(--accent)}.bars-ok{--bar-color:var(--accent)}.bars-warn{--bar-color:var(--warn)}.bars-crit{--bar-color:var(--crit)}.bar{width:6px;height:10px;border-radius:2px;background:#e0e0e0;border:1px solid #bdbdbd}.bar.f{background:var(--bar-color);border-color:var(--bar-color)}@media (max-width:700px){.brand-row{flex-direction:column;align-items:flex-start}.row strong{min-width:120px}}@media (max-width:600px){.tab{font-size:.95rem;padding:.45rem .6rem}.card{padding:.75rem}.ml-s{margin-left:.5rem}}";

static const char ROOT_EXTRA_STYLE[] PROGMEM =
"dialog{width:min(92vw,640px)}.lead{font-size:1.05rem;font-weight:600}.status-grid{display:grid;grid-template-columns:1.25fr 1fr;gap:1rem;align-items:start}.stat{display:inline-flex;align-items:center;gap:.5rem;padding:.35rem .6rem;border:1px solid var(--card-border);border-radius:.5rem;background:var(--card-bg)}.swatch{display:inline-block;width:22px;height:22px;border:1px solid var(--card-border);border-radius:4px;background:#000}.kv{display:grid;grid-template-columns:160px 1fr;gap:.5rem;margin:.35rem 0}.kv strong{color:var(--muted)}progress{width:100%;height:10px}.meta{color:var(--muted);font-size:.9rem}.pill{vertical-align:middle} @media(max-width:840px){.status-grid{grid-template-columns:1fr}}";

static const char EFFECTS_EXTRA_STYLE[] PROGMEM =
".controls{display:flex;flex-wrap:wrap;gap:.5rem;align-items:center;justify-content:flex-start}.controls label{white-space:nowrap}.controls input,.controls select,.controls button{margin:.25rem 0}.table-wrap{overflow-x:auto;-webkit-overflow-scrolling:touch;margin-top:1rem}table{border-collapse:collapse;width:100%;min-width:860px;table-layout:auto}td,th{border:1px solid #ccc;padding:4px 6px;text-align:left}input[type=number]{width:5rem}select{min-width:8.5rem;max-width:100%}@media (max-width:700px){.table-wrap{overflow:visible}table{min-width:0}thead{display:none}tbody, tr, td{display:block}tr{border:1px solid #ddd;border-radius:.35rem;padding:.5rem;margin-bottom:.75rem;background:var(--card-bg)}td{border:none;border-bottom:1px solid #eee;padding:6px 4px}td:last-child{border-bottom:none}td::before{content:attr(data-label);display:block;font-weight:600;color:#555;margin-bottom:2px}td input,td select,td button{width:100%}td input[type=checkbox]{width:auto}}@media (max-width:600px){input[type=number]{width:4.5rem}.controls{gap:.4rem}}";

static const char CONFIG_EXTRA_STYLE[] PROGMEM =
"label{display:grid;grid-template-columns:180px 1fr;align-items:center;gap:.5rem;margin:.5rem 0}input,select{max-width:100%}fieldset{border:1px solid #ddd;border-radius:.35rem;padding: .5rem .75rem}legend{padding:0 .25rem;color:#555}dialog{width:min(92vw,640px)}.dialog-menu{display:flex;justify-content:flex-end;gap:.5rem}.wifi-ssid-input{display:flex;align-items:center;gap:.5rem;flex-wrap:wrap}.wifi-scan-status{display:block;font-size:.85rem;color:#555;margin-top:.35rem}";

static const char LOGS_EXTRA_STYLE[] PROGMEM =
".controls{display:flex;flex-wrap:wrap;gap:.5rem;align-items:center;margin:.5rem 0}.controls input{margin:.25rem 0}.logbox{font-family:ui-monospace,SFMono-Regular,Menlo,Consolas,monospace;white-space:pre-wrap;background:var(--mono-bg);color:var(--mono-fg);border:1px solid var(--card-border);padding:.5rem;height:50vh;overflow:auto;border-radius:.35rem}";

static const char FW_EXTRA_STYLE[] PROGMEM =
"form#fwform{display:flex;flex-wrap:wrap;align-items:center;gap:.75rem}.hint{color:#555;font-size:.9rem}.kv{margin:.25rem 0}.kv strong{min-width:160px;display:inline-block}.logarea{font-family:ui-monospace,SFMono-Regular,Menlo,Consolas,monospace;white-space:pre-wrap;background:var(--mono-bg);color:var(--mono-fg);border:1px solid var(--card-border);padding:.5rem;max-height:55vh;overflow:auto;border-radius:.35rem}";

// Minimal HTML escape for server-side text injection
static inline String htmlEscape(const String &in) {
	String out; out.reserve(in.length());
	for (size_t i = 0; i < in.length(); i++) {
		char c = in.charAt(i);
		if (c == '&') out += F("&amp;");
		else if (c == '<') out += F("&lt;");
		else if (c == '>') out += F("&gt;");
		else out += c;
	}
	return out;
}

static inline void appendHeadWithStyles(String &s, const char* title, const char* extraCssPROGMEM) {
	s += F("<!DOCTYPE html><html lang='en'><head><meta charset='utf-8'><meta name='viewport' content='width=device-width, initial-scale=1'><script>(function(){try{var t=localStorage.getItem('theme');if(!t){t=(window.matchMedia&&window.matchMedia('(prefers-color-scheme: dark)').matches)?'dark':'light'}document.documentElement.setAttribute('data-theme',t)}catch(e){}function getKey(){try{return new URL(window.location.href).searchParams.get('key')||''}catch(err){return''}}function addKeyToUrl(value){var key=getKey();if(!key)return value;try{var url=value instanceof Request?new URL(value.url,window.location.href):new URL(String(value),window.location.href);if(url.origin===window.location.origin&&!url.searchParams.has('key'))url.searchParams.set('key',key);return value instanceof Request?new Request(url.toString(),value):url.toString()}catch(err){return value}}var rawFetch=window.fetch?window.fetch.bind(window):null;if(rawFetch){window.fetch=function(input,init){var key=getKey();var nextInput=addKeyToUrl(input);var nextInit=init?Object.assign({},init):{};if(key){var baseHeaders=nextInit.headers;if(!baseHeaders&&nextInput instanceof Request)baseHeaders=nextInput.headers;var headers=new Headers(baseHeaders||{});if(!headers.has('X-StatusGlow-Key'))headers.set('X-StatusGlow-Key',key);if(!headers.has('X-OTA-Key'))headers.set('X-OTA-Key',key);nextInit.headers=headers}return rawFetch(nextInput,nextInit)}}function syncProtectedLinks(){var key=getKey();if(!key)return;document.querySelectorAll(\"a[href^='/']\").forEach(function(a){try{var url=new URL(a.getAttribute('href'),window.location.href);if(url.origin===window.location.origin&&!url.searchParams.has('key')){url.searchParams.set('key',key);a.setAttribute('href',url.pathname+url.search+url.hash)}}catch(err){}});document.querySelectorAll(\"form[action^='/']\").forEach(function(form){try{var url=new URL(form.getAttribute('action'),window.location.href);if(url.origin===window.location.origin&&!url.searchParams.has('key')){url.searchParams.set('key',key);form.setAttribute('action',url.pathname+url.search+url.hash)}}catch(err){}})}window.StatusGlowAuth={getKey:getKey,addKeyToUrl:addKeyToUrl};window.addEventListener('DOMContentLoaded',syncProtectedLinks)})();</script><title>");
	s += title;
	s += F("</title><link rel='icon' href='/favicon.ico' sizes='any'><style>");
	s += FPSTR(COMMON_STYLE);
	if (extraCssPROGMEM != nullptr) { s += FPSTR(extraCssPROGMEM); }
	s += F("</style></head><body><div class='container'>");
}

// Branded header and navigation (appears on every page)
static inline void appendBrandHeaderAndTabs(String &s, const char* pageLabel, const char* activeTab) {
	s += F("<header class='brand'>");
	s += F("<div class='brand-row'>");
	s += F("<div class='brand-left'>");
	s += F("<img class='brand-img' src='/logo.svg' alt='StatusGlow' onerror=\"this.style.display='none';var b=this.parentNode.querySelector('.brand-logo');if(b) b.style.display='inline-block';\">");
	s += F("<span class='brand-logo' aria-hidden='true'></span><span class='brand-title'>StatusGlow</span>");
	if (pageLabel && pageLabel[0] != '\0') { s += String("<span class='brand-sub'> - ") + pageLabel + "</span>"; }
	s += F("</div>");
	s += String("<div class='brand-right'><span class='brand-ver'>v") + String(VERSION) + "</span></div>";
	s += F("</div>");
	// Tabs
	s += F("<nav class='tabs'>");
	s += String("<a class='tab ") + ((activeTab&&String(activeTab)=="home")?"active":"") + "' href='/'>Home</a>";
	s += String("<a class='tab ") + ((activeTab&&String(activeTab)=="config")?"active":"") + "' href='/config'>Config</a>";
	s += String("<a class='tab ") + ((activeTab&&String(activeTab)=="effects")?"active":"") + "' href='/effects'>Effects</a>";
	s += String("<a class='tab ") + ((activeTab&&String(activeTab)=="logs")?"active":"") + "' href='/logs'>Logs</a>";
	s += String("<a class='tab ") + ((activeTab&&String(activeTab)=="fw")?"active":"") + "' href='/fw'>Firmware</a>";
	s += F("</nav></header>");
}

// Shared live top bar (CPU, Heap, Wi-Fi) and a script that refreshes it on an interval
static inline void appendLiveTopBarAndScript(String &s) {
	s += F("<div class='topbar'><strong>CPU</strong> <span id='live_cpu' class='pill'></span> <strong class='ml-s'>Heap</strong> <span id='live_heap' class='pill'></span> <strong class='ml-s'>Wi-Fi</strong> <span id='live_wifi' class='bars' aria-label='Wi-Fi signal'></span> <button id='theme_toggle' class='btn ml-s' type='button' title='Toggle theme'>Theme</button></div>");
	s += F("<script>(function(){\n"
			"// Centralized UI thresholds/config (defaults here; device can override via /api/settings)\n"
			"let CFG={CPU_OK:50,CPU_WARN:80,HEAP_OK:60,HEAP_WARN:85,RSSI:[-85,-75,-65,-55],BAR_HEIGHTS:[6,10,14,18],MIN_REFRESH_MS:2000};\n"
			"function applyUiCfgFromDevice(s){if(!s||!s.ui)return;const u=s.ui;CFG.CPU_OK=(u.cpu_ok??CFG.CPU_OK);CFG.CPU_WARN=(u.cpu_warn??CFG.CPU_WARN);CFG.HEAP_OK=(u.heap_ok??CFG.HEAP_OK);CFG.HEAP_WARN=(u.heap_warn??CFG.HEAP_WARN);if(u.rssi&&u.rssi.length===4){CFG.RSSI=u.rssi.slice(0,4)}if(u.bar_heights&&u.bar_heights.length===4){CFG.BAR_HEIGHTS=u.bar_heights.slice(0,4)}if(typeof u.min_refresh_ms==='number'&&u.min_refresh_ms>0){CFG.MIN_REFRESH_MS=u.min_refresh_ms}}\n"
			"function pillClassFromPercent(p,ok,warn){return (p<ok)?'pill-ok':(p<warn)?'pill-warn':'pill-crit'}\n"
			"function wifiBarsFromRssi(rssi){rssi=parseInt(rssi);var bars=0,cls='bars-crit';if(isNaN(rssi)){bars=0;cls='bars-crit'}else if(rssi>=CFG.RSSI[3]){bars=4;cls='bars-ok'}else if(rssi>=CFG.RSSI[2]){bars=3;cls='bars-ok'}else if(rssi>=CFG.RSSI[1]){bars=2;cls='bars-warn'}else if(rssi>=CFG.RSSI[0]){bars=1;cls='bars-crit'}else{bars=0;cls='bars-crit'}return {bars:bars,cls:cls}}\n"
			"function renderWifiBars(el,rssi){var res=wifiBarsFromRssi(rssi);el.className='bars '+res.cls;el.innerHTML='';for(var i=1;i<=4;i++){var b=document.createElement('span');b.className='bar'+(i<=res.bars?' f':'');b.style.height=CFG.BAR_HEIGHTS[i-1]+'px';el.appendChild(b)}}\n"
			"function setTheme(t){try{localStorage.setItem('theme',t)}catch(e){}document.documentElement.setAttribute('data-theme',t)}\n"
			"function toggleTheme(){var cur=document.documentElement.getAttribute('data-theme')||'light';setTheme(cur==='dark'?'light':'dark')}\n"
			"window.CFG=CFG;window.pillClassFromPercent=pillClassFromPercent;window.wifiBarsFromRssi=wifiBarsFromRssi;window.renderWifiBars=renderWifiBars;\n"
			"let didApply=false;function upd(){fetch('/api/settings').then(r=>r.json()).then(s=>{if(!didApply){applyUiCfgFromDevice(s);didApply=true;}\n"
				"var c=document.getElementById('live_cpu');if(c){var v=parseInt(s.cpu_usage)||0;c.textContent=v+'%';c.classList.remove('pill-ok','pill-warn','pill-crit');c.classList.add(pillClassFromPercent(v,CFG.CPU_OK,CFG.CPU_WARN))}\n"
				"var h=document.getElementById('live_heap');if(h){var total=parseInt(s.heap_total)||327680;var free=parseInt(s.heap)||0;var up=Math.min(100,Math.max(0,Math.round(((Math.max(0,total-free))/total)*100)));h.textContent=up+'%';h.classList.remove('pill-ok','pill-warn','pill-crit');h.classList.add(pillClassFromPercent(up,CFG.HEAP_OK,CFG.HEAP_WARN))}\n"
				"var wb=document.getElementById('live_wifi');if(wb){renderWifiBars(wb,s.wifi_rssi)}\n"
				"var tbtn=document.getElementById('theme_toggle');if(tbtn&&!tbtn._bound){tbtn._bound=true;tbtn.addEventListener('click',toggleTheme)}\n"
			"})}upd();setInterval(upd,CFG.MIN_REFRESH_MS)})();</script>");
}

// Requests to /
void handleRoot() {
	DBG_PRINTLN("handleRoot()");
	// The main router handles captive-portal redirects before this page renders.

	String s;
	appendHeadWithStyles(s, "StatusGlow", ROOT_EXTRA_STYLE);
	appendLiveTopBarAndScript(s);
	s += F("<script>function performClearSettings(){fetch('/api/clearSettings').then(r=>r.json()).then(d=>{console.log('clearSettings',d);document.getElementById('dialog-clearsettings').close();document.getElementById('dialog-clearsettings-result').showModal()})}function toHex(c){let r=(c>>16)&255,g=(c>>8)&255,b=c&255;return '#'+[r,g,b].map(x=>x.toString(16).padStart(2,'0')).join('')}let modes=[];function loadModes(){return fetch('/api/modes').then(r=>r.json()).then(m=>modes=m||[])}function modeName(id){let m=modes.find(x=>parseInt(x.id)===parseInt(id));return m?m.name+' ('+m.id+')':('#'+id)}function fillSettings(s){let gi=(v)=>v!=null?v:'';document.getElementById('poll_interval').textContent=gi(s.poll_interval);document.getElementById('num_leds').textContent=gi(s.num_leds);document.getElementById('rssi').textContent=gi(s.wifi_rssi);var ss=document.getElementById('ssid');if(ss){ss.textContent=gi(s.wifi_ssid)}document.getElementById('ip').textContent=gi(s.wifi_ip);document.getElementById('heap').textContent=gi(s.heap);document.getElementById('min_heap').textContent=gi(s.min_heap);document.getElementById('cpu').textContent=s.cpu_freq!=null?(s.cpu_freq+' MHz'):'';let cuEl=document.getElementById('cpu_usage');let cu=parseInt(s.cpu_usage)||0;cuEl.textContent=cu+'%';cuEl.classList.remove('pill-ok','pill-warn','pill-crit');cuEl.classList.add(pillClassFromPercent(cu,CFG.CPU_OK,CFG.CPU_WARN));let heapEl=document.getElementById('heap_usage');if(heapEl){let total=parseInt(s.heap_total)||327680;let free=parseInt(s.heap)||0;let used=Math.max(0,total-free);let hup=Math.min(100,Math.max(0,Math.round((used/total)*100)));heapEl.textContent=hup+'%';heapEl.classList.remove('pill-ok','pill-warn','pill-crit');heapEl.classList.add(pillClassFromPercent(hup,CFG.HEAP_OK,CFG.HEAP_WARN))}let barsEl=document.getElementById('wifi_bars');renderWifiBars(barsEl,s.wifi_rssi);document.getElementById('ver').textContent=gi(s.sketch_version);let up=s.uptime_ms||0;let sec=Math.floor(up/1000),h=Math.floor(sec/3600);sec%=3600;let m=Math.floor(sec/60);sec%=60;document.getElementById('uptime').textContent=(h+':'+String(m).padStart(2,'0')+':'+String(sec).padStart(2,'0'));let net=document.getElementById('network');if(net){let parts=[];let st=parseInt(s.wifi_status);let ssidTxt=gi(s.wifi_ssid);if(st===3){let sta=(ssidTxt?(ssidTxt+' @ '):'')+(s.wifi_ip||'');parts.push('STA '+sta)}else{parts.push('STA '+(ssidTxt?ssidTxt+' (disconnected)':'disconnected'))}let apn=(s.ap_ssid||'');let aip=(s.ap_ip||'');let apTxt='AP '+apn+(s.ap_enabled?(aip?(' @ '+aip):''):' (off)');parts.push(apTxt);net.textContent=parts.join(' | ')} }function loadSettings(){return fetch('/api/settings').then(r=>r.json()).then(fillSettings)}function fillCurrent(c){if(!c){document.getElementById('current_line').textContent='';return}let name=modeName(c.mode);let hex=toHex((c.color>>>0));let txt=(c.activity?c.activity+' - ':'')+name+', Speed '+(c.speed||0)+', Reverse '+(c.reverse?'on':'off')+', Color '+hex;document.getElementById('current_line').textContent=txt;let sw=document.getElementById('swatch');sw.style.backgroundColor=hex}function loadCurrent(){return fetch('/api/current').then(r=>r.json()).then(fillCurrent)}function init(){Promise.all([loadModes(),loadSettings(),loadCurrent()]).then(()=>{let iv=CFG.MIN_REFRESH_MS;let pi=document.getElementById('poll_interval').textContent;let n=parseInt(pi);if(!isNaN(n)&&n>0)iv=Math.max(CFG.MIN_REFRESH_MS,n*1000);setInterval(()=>{loadSettings();loadCurrent()},iv)})}window.onload=init;</script>");
	appendBrandHeaderAndTabs(s, "Home", "home");
	s += "<div class='card'>";
	s += "<div class='status-grid'>";
	// Left: LED status
	s += "<section><h3>LED status</h3>";
	s += "<div id='current_line' class='lead'></div>";
	s += "<div class='mt-s stat'><span>Color</span><span id='swatch' class='swatch'></span></div>";
	s += "</section>";
	// Right: System info quick view
	s += "<section><h3>System info</h3>";
	s += "<div class='kv'><strong>Polling interval</strong><span><span id='poll_interval'></span> s</span></div>";
	s += "<div class='kv'><strong>LEDs</strong><span id='num_leds'></span></div>";
	s += "<div class='kv'><strong>Uptime</strong><span id='uptime'></span></div>";
	s += "<div class='kv'><strong>Wi-Fi</strong><span><span id='wifi_bars' class='bars' aria-label='Wi-Fi signal'></span><span class='ml-s meta'>RSSI <span id='rssi'></span> dBm</span><span class='ml-s meta'>SSID <span id='ssid'></span></span><span class='ml-s meta'>IP <span id='ip'></span></span></span></div>";
	s += "<div class='kv'><strong>Network</strong><span id='network'></span></div>";
	s += "<div class='kv'><strong>Heap</strong><span><span id='heap'></span> bytes free, min <span id='min_heap'></span></span></div>";
	s += "<div class='kv'><strong>CPU / Version</strong><span><span id='cpu'></span> / <span id='ver'></span></span></div>";
	s += "</section>";
	s += "</div>"; // end status-grid

	// Memory usage block
	s += "<section class='mt'><h3>Memory usage</h3>";
	s += "<div class='row'>Sketch: " + String(ESP.getSketchSize()) + " bytes used, " + String(ESP.getFreeSketchSpace()) + " bytes free for OTA</div>";
	s += "<progress value=\"" + String(ESP.getSketchSize()) + "\" max=\"" + String(ESP.getSketchSize() + ESP.getFreeSketchSpace()) + "\"></progress>";
	s += "<div class='row mt-s'>RAM: " + String(ESP.getFreeHeap()) + " of " + String(ESP.getHeapSize()) + " bytes free</div>";
	s += "<progress value=\"" + String(ESP.getHeapSize() - ESP.getFreeHeap()) + "\" max=\"" + String(ESP.getHeapSize()) + "\"></progress>";
	s += "</section>";

	// Danger area moved to Config page for better IA
	s += "</div></div></body>\n</html>\n";

	server.sendHeader("Cache-Control", "no-store, no-cache, must-revalidate, max-age=0");
	server.sendHeader("Pragma", "no-cache");
	server.sendHeader("Expires", "0");
	server.send(200, "text/html; charset=utf-8", s);
}

void handleGetSettings() {
	DBG_PRINTLN("handleGetSettings()");
	
	JsonDocument responseDoc;
	responseDoc["client_id"].set(paramClientIdValue);
	responseDoc["tenant"].set(paramTenantValue);
	responseDoc["poll_interval"].set(paramPollIntervalValue);
	responseDoc["num_leds"].set(numberLeds);
	responseDoc["led_type_rgbw"].set(gLedTypeRGBW);  // Add LED type setting
	responseDoc["status_led_enabled"].set(gStatusLedEnabled);  // Add status LED setting

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
	responseDoc["wifi_ssid"].set(WiFi.SSID());
	responseDoc["wifi_ip"].set(WiFi.localIP().toString());
	responseDoc["wifi_status"].set((int)WiFi.status());
	responseDoc["ap_ip"].set(WiFi.softAPIP().toString());
	responseDoc["ap_ssid"].set(gApSsid.c_str());
	responseDoc["ap_enabled"].set(gApEnabled);
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
	server.send(200, "application/json", responseDoc.as<String>());
}

// Clear all settings: wipe unified config/context files and reboot
void handleClearSettings() {
	DBG_PRINTLN("handleClearSettings()");
	SPIFFS.remove(CONTEXT_FILE);
	SPIFFS.remove(CONFIG_FILE);
	SPIFFS.remove(EFFECTS_FILE);
	SPIFFS.remove("/rizz.json");
	memset(paramClientIdValue, 0, sizeof(paramClientIdValue));
	memset(paramTenantValue, 0, sizeof(paramTenantValue));
	strlcpy(paramPollIntervalValue, DEFAULT_POLLING_PRESENCE_INTERVAL, sizeof(paramPollIntervalValue));
	server.send(200, "application/json", F("{\"ok\":true}"));
	ESP.restart();
}

// Requests to /startDevicelogin
void handleStartDevicelogin() {
	if (state != SMODEDEVICELOGINSTARTED) {
		DBG_PRINTLN(F("handleStartDevicelogin()"));
	JsonDocument doc;
	boolean res = requestJsonApi(doc, "https://login.microsoftonline.com/" + String(paramTenantValue) + "/oauth2/v2.0/devicecode", "client_id=" + String(paramClientIdValue) + "&scope=offline_access%20openid%20Presence.Read", 0);

	if (res && !doc["device_code"].isNull() && !doc["user_code"].isNull() && !doc["interval"].isNull() && !doc["verification_uri"].isNull() && !doc["message"].isNull()) {
			device_code = doc["device_code"].as<String>();
			user_code = doc["user_code"].as<String>();
			interval = doc["interval"].as<unsigned int>();
			JsonDocument responseDoc;
			responseDoc["user_code"] = doc["user_code"].as<const char*>();
			responseDoc["verification_uri"] = doc["verification_uri"].as<const char*>();
			responseDoc["message"] = doc["message"].as<const char*>();
			state = SMODEDEVICELOGINSTARTED;
			tsPolling = millis() + (interval * 1000);
			server.send(200, "application/json", responseDoc.as<String>());
		} else {
			server.send(500, "application/json", F("{\"error\": \"devicelogin_unknown_response\"}"));
		}
	} else {
		server.send(409, "application/json", F("{\"error\": \"devicelogin_already_running\"}"));
	}
}

// Simple Effects Editor page with tabs and centered layout
void handleEffectsUi() {
	String s;
	appendHeadWithStyles(s, "StatusGlow - Effects", EFFECTS_EXTRA_STYLE);
	appendLiveTopBarAndScript(s);
	appendBrandHeaderAndTabs(s, "Effects", "effects");
	s += F("<div class='card'>");
	s += F("<div class='row controls'><label>Global Fade (ms): <input id='fade' type='number' min='0' step='50'></label><label>Brightness (%) : <input id='bri' type='number' min='0' max='100' step='1'> <input id='bri_range' type='range' min='0' max='100' step='1' style='vertical-align:middle;width:120px'></label><label>Gamma: <input id='gamma' type='number' min='0.1' max='5' step='0.1'></label><label>LEDs: <input id='numleds' type='number' min='1' max='1024' step='1'></label><label><input id='previewMode' type='checkbox' onchange='togglePreviewMode()'> Preview Mode</label><button class='btn' onclick='save()'>Save</button><button class='btn' onclick='applyLedCount()'>Apply LED Count</button><button class='btn' onclick='loadAll()'>Reload</button></div>");
	s += F("<div id='current' class='row' style='font-weight:bold;'></div>");
	s += F("<div class='table-wrap'><table id='t'><thead><tr><th style='width:72px'>Preview</th><th style='min-width:140px'>Key</th><th style='min-width:150px'>Mode</th><th style='width:170px'>Duration (s)</th><th style='width:110px'>Color</th><th style='width:120px'>Fade (ms)</th><th style='width:120px'>Brightness (%)</th><th style='width:110px'>Actions</th></tr></thead><tbody></tbody></table></div>");
	s += F("<script>let data={},modes=[],current={},preview={enabled:false,key:''};function toHex(c){let r=(c>>16)&255,g=(c>>8)&255,b=c&255;return '#'+[r,g,b].map(x=>x.toString(16).padStart(2,'0')).join('')}function fromHex(h){if(!h||h.length<7)return 0;let r=parseInt(h.slice(1,3),16),g=parseInt(h.slice(3,5),16),b=parseInt(h.slice(5,7),16);return (r<<16)|(g<<8)|b}function buildModeSelect(v){let s=document.createElement('select');s.dataset.k='mode';(modes||[]).forEach(m=>{let o=document.createElement('option');o.value=m.id;o.textContent=m.name+' ('+m.id+')';if(parseInt(v)===parseInt(m.id))o.selected=true;s.appendChild(o)});return s}function rowForProfile(p){let tr=document.createElement('tr');tr.dataset.key=p.key;let tdPrev=document.createElement('td');tdPrev.dataset.label='Preview';let cb=document.createElement('input');cb.type='checkbox';cb.className='prev-cb';cb.disabled=!preview.enabled;cb.onclick=()=>selectPreview(tr,cb);tdPrev.appendChild(cb);tr.appendChild(tdPrev);let tdKey=document.createElement('td');tdKey.dataset.label='Key';tdKey.textContent=p.key;tr.appendChild(tdKey);let tdMode=document.createElement('td');tdMode.dataset.label='Mode';tdMode.appendChild(buildModeSelect(p.mode));tr.appendChild(tdMode);let tdSpeed=document.createElement('td');tdSpeed.dataset.label='Duration (s)';let wrap=document.createElement('div');wrap.style.display='flex';wrap.style.alignItems='center';wrap.style.gap='.35rem';let inSpeed=document.createElement('input');inSpeed.type='range';inSpeed.min='0';inSpeed.max='60';inSpeed.step='0.1';inSpeed.value=(p.speed||0);inSpeed.dataset.k='speed';inSpeed.title='Higher = Slower effect';let inSpeedN=document.createElement('input');inSpeedN.type='number';inSpeedN.min='0';inSpeedN.max='60';inSpeedN.step='0.1';inSpeedN.style.width='4rem';inSpeedN.value=(p.speed||0);inSpeed.addEventListener('input',()=>{inSpeedN.value=inSpeed.value});inSpeedN.addEventListener('input',()=>{inSpeed.value=inSpeedN.value});wrap.appendChild(inSpeed);wrap.appendChild(inSpeedN);tdSpeed.appendChild(wrap);tr.appendChild(tdSpeed);let tdColor=document.createElement('td');tdColor.dataset.label='Color';let inColor=document.createElement('input');inColor.type='color';inColor.value=toHex((p.color>>>0));inColor.dataset.k='color';tdColor.appendChild(inColor);tr.appendChild(tdColor);let tdFade=document.createElement('td');tdFade.dataset.label='Fade (ms)';let inFade=document.createElement('input');inFade.type='number';inFade.min='0';inFade.step='50';inFade.placeholder='0=global';inFade.value=p.fade_ms||0;inFade.dataset.k='fade_ms';tdFade.appendChild(inFade);tr.appendChild(tdFade);let tdBri=document.createElement('td');tdBri.dataset.label='Brightness (%)';let inBri=document.createElement('input');inBri.type='number';inBri.min='0';inBri.max='100';inBri.step='1';inBri.placeholder='0=global (%)';inBri.value=(p.bri?Math.round((p.bri*100)/255):0);inBri.dataset.k='bri';inBri.style.width='4rem';tdBri.appendChild(inBri);tr.appendChild(tdBri);let tdAct=document.createElement('td');tdAct.dataset.label='Actions';let btn=document.createElement('button');btn.className='btn';btn.textContent='Preview';btn.onclick=()=>previewRow(tr);tdAct.appendChild(btn);tr.appendChild(tdAct);return tr}function renderCurrent(){if(!current||!modes.length){document.getElementById('current').textContent='';return}let m=modes.find(x=>parseInt(x.id)===parseInt(current.mode));let name=m?m.name+' ('+m.id+')':('#'+current.mode);let hex=toHex((current.color>>>0));document.getElementById('current').textContent=`Current: ${current.activity||''} \u2014 ${name}, Duration ${current.speed||0}s, Color ${hex}`}function loadAll(){Promise.all([fetch('/api/modes').then(r=>r.json()),fetch('/api/effects').then(r=>r.json()),fetch('/api/current').then(r=>r.json()),fetch('/api/preview_state').then(r=>r.json())]).then(([mods,d,c,ps])=>{modes=mods||[];data=d||{};current=c||{};preview=ps||{enabled:false,key:''};document.getElementById('fade').value=d.fade_ms||0;let b=(d.brightness!=null?d.brightness:128);let bp=Math.round((b*100)/255);document.getElementById('bri').value=bp;let br=document.getElementById('bri_range');if(br)br.value=bp;let g=(d.gamma!=null?d.gamma:2.2);document.getElementById('gamma').value=g.toFixed(1);document.getElementById('numleds').value=d.num_leds||0;document.getElementById('previewMode').checked=!!preview.enabled;let tb=document.querySelector('#t tbody');tb.innerHTML='';(d.profiles||[]).forEach(p=>{let tr=rowForProfile(p);tb.appendChild(tr);if(preview.enabled&&preview.key===p.key){let cb=tr.querySelector('.prev-cb');if(cb)cb.checked=true}});renderCurrent();bindBriSync()})}function bindBriSync(){let n=document.getElementById('bri'),r=document.getElementById('bri_range');if(!n||!r)return;n.addEventListener('input',()=>{r.value=n.value});r.addEventListener('input',()=>{n.value=r.value})}function applyLedCount(){let n=parseInt(document.getElementById('numleds').value)||0;fetch('/api/leds',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({num_leds:n})}).then(r=>r.json()).then(()=>loadAll())}function save(){let uiB=parseInt(document.getElementById('bri').value)||0;uiB=Math.min(100,Math.max(0,uiB));let d={fade_ms:parseInt(document.getElementById('fade').value)||0,brightness:Math.round((uiB*255)/100),gamma:parseFloat(document.getElementById('gamma').value)||2.2,profiles:[]};document.querySelectorAll('#t tbody tr').forEach(tr=>{let p={key:tr.dataset.key};tr.querySelectorAll('input,select').forEach(inp=>{let k=inp.dataset.k;if(!k)return;if(k==='color'){p[k]=fromHex(inp.value)}else if(k==='bri'){let v=parseFloat(inp.value)||0;v=Math.min(100,Math.max(0,v));p[k]=(v<=0)?0:Math.round((v*255)/100)}else{p[k]=parseFloat(inp.value)||0}});p.reverse=0;d.profiles.push(p)});fetch('/api/effects',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify(d)}).then(r=>r.json()).then(()=>loadAll())}function objFromRow(tr){let p={};tr.querySelectorAll('input,select').forEach(inp=>{let k=inp.dataset.k;if(!k)return;if(k==='color'){p[k]=fromHex(inp.value)}else if(k==='bri'){let v=parseFloat(inp.value)||0;v=Math.min(100,Math.max(0,v));p[k]=(v<=0)?0:Math.round((v*255)/100)}else{p[k]=parseFloat(inp.value)||0}});p.key=tr.dataset.key;p.reverse=0;return p}function previewRow(tr){let p=objFromRow(tr);fetch('/api/preview',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify(p)})}function togglePreviewMode(){let en=document.getElementById('previewMode').checked;fetch('/api/preview_mode',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({enabled:en})}).then(r=>r.json()).then(ps=>{preview=ps||{enabled:false,key:''};document.querySelectorAll('.prev-cb').forEach(cb=>{cb.disabled=!preview.enabled;if(!preview.enabled)cb.checked=false})})}function selectPreview(tr,cb){if(!preview.enabled){cb.checked=false;return}document.querySelectorAll('.prev-cb').forEach(x=>{if(x!==cb)x.checked=false});let key=tr.dataset.key;fetch('/api/preview_select',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({key})}).then(r=>r.json()).then(ps=>{preview=ps||preview})}window.onload=loadAll;</script>");
	s += F("</div></div></body></html>");
	server.sendHeader("Cache-Control", "no-store, no-cache, must-revalidate, max-age=0");
	server.sendHeader("Pragma", "no-cache");
	server.sendHeader("Expires", "0");
	server.send(200, "text/html; charset=utf-8", s);
}


// Simple Config wrapper page to align with tabs and center content
void handleConfigUi() {
	String s;
	appendHeadWithStyles(s, "StatusGlow - Configuration", CONFIG_EXTRA_STYLE);
	appendLiveTopBarAndScript(s);
	appendBrandHeaderAndTabs(s, "Configuration", "config");
	s += F("<div class='card'>");
	s += F("<form id='cfg' onsubmit=\"ev=>ev&&ev.preventDefault()\"><label>Client ID<input id='clientId'></label><label>Tenant<input id='tenant'></label><label>Presence polling (sec)<input id='poll' type='number' min='1' step='1'></label><label>LED Type<select id='ledType'><option value='false'>RGB (3-color)</option><option value='true'>RGBW (4-color)</option></select></label><label style='display:flex;align-items:center;gap:0.5rem'><input type='checkbox' id='statusLed' style='width:auto;margin:0'>Enable Status LED (S3 only, GPIO21)</label><fieldset class='mt'><legend>Wi-Fi</legend><div id='wifi_state' style='font-size:.9rem;color:#555;margin:.25rem 0 .5rem'></div><label>SSID<span><span class='wifi-ssid-input'><input id='ssid' list='ssidOptions' placeholder='Select or type SSID'><button type='button' class='btn' id='wifi_scan_btn' onclick='scanWifiNetworks()'>Scan</button></span><datalist id='ssidOptions'></datalist><span id='wifi_scan_status' class='wifi-scan-status'></span></span></label><label>Password<input id='wpass' type='password'></label><div class='mt'><button type='button' class='btn' onclick='saveWifi()'>Connect Wi-Fi</button><button type='button' class='btn ml-s' onclick='startAp()'>Start AP</button><button type='button' class='btn ml-s' onclick='stopAp()'>Stop AP</button></div></fieldset><div class='mt'><button type='button' class='btn' onclick='saveCfg()'>Save</button><button type='button' class='btn ml-s' onclick='loadCfg()'>Reload</button><button type='button' class='btn ml-s' onclick='openDeviceLoginModal()'>Start device login</button><button type='button' class='btn ml-s' style='background:#e74c3c' onclick='rebootDevice()'>Reboot Device</button></div></form>");
	// Danger area moved here from Home
	s += F("<section class='mt'><h3>Danger area</h3>");
	s += F("<dialog id=\"dialog-clearsettings\">\n");
	s += F("<p class=\"title\">Factory reset: clear all settings and reboot?</p>\n");
	String apMsg;
	if (gApEnabled) { apMsg = String("The local AP is currently ON (\"") + gApSsid + "\" @ " + WiFi.softAPIP().toString() + ")."; }
	else { apMsg = String("The local AP is currently OFF; if STA doesn't connect after reboot, it will start as \"") + gApSsid + "\"."; }
	s += String("<p>This will erase Wi-Fi credentials, app configuration, and effects, then reboot. ") + apMsg + "</p>\n";
	s += F("<button class=\"btn\" onclick=\"document.getElementById('dialog-clearsettings').close()\">Cancel</button>\n");
	s += F("<button class=\"btn\" onclick=\"performClearSettings()\">Clear settings and reboot</button>\n");
	s += F("</dialog>\n");
	s += F("<dialog id=\"dialog-clearsettings-result\">\n");
	s += F("<p class=\"title\">Settings cleared. Rebooting...</p>\n");
	s += F("</dialog>\n");
	s += F("<div><button type=\"button\" class=\"btn\" onclick=\"document.getElementById('dialog-clearsettings').showModal();\">Factory reset (clear and reboot)</button></div>");
	s += F("</section>");
	s += F("<dialog id='dialog-devicelogin'><p class='title'>Start device login</p><p id='lbl_message'></p><input type='text' id='code_field' disabled><menu class='dialog-menu'><button id='btn_close' onclick='closeDeviceLoginModal()'>Close</button><a class='ml-s' id='btn_open' href='https://microsoft.com/devicelogin' target='_blank'>Open device login</a></menu></dialog>");
	s += F("<script>function performClearSettings(){fetch('/api/clearSettings').then(r=>r.json()).then(()=>{document.getElementById('dialog-clearsettings').close();document.getElementById('dialog-clearsettings-result').showModal()})}let wifiScanBusy=false;function fill(d){if(!d)return;document.getElementById('clientId').value=d.client_id||'';document.getElementById('tenant').value=d.tenant||'';document.getElementById('poll').value=parseInt(d.poll_interval)||30;document.getElementById('ledType').value=d.led_type_rgbw?'true':'false';document.getElementById('statusLed').checked=d.status_led_enabled||false}function loadCfg(){fetch('/api/settings').then(r=>r.json()).then(fill);loadWifiState()}function saveCfg(){let d={client_id:document.getElementById('clientId').value||'',tenant:document.getElementById('tenant').value||'',poll_interval:parseInt(document.getElementById('poll').value)||30,led_type_rgbw:document.getElementById('ledType').value==='true',status_led_enabled:document.getElementById('statusLed').checked};fetch('/api/settings',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify(d)}).then(r=>r.json()).then(x=>{if(x.needs_reboot){alert('LED type changed. Please reboot the device for changes to take effect.');} loadCfg()})}function saveWifi(){let d={ssid:(document.getElementById('ssid').value||'').trim(),password:document.getElementById('wpass').value||''};fetch('/api/wifi',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify(d)}).then(r=>r.json()).then(()=>{loadWifiState()})}function setWifiScanStatus(msg,isError){let el=document.getElementById('wifi_scan_status');if(!el)return;el.textContent=msg||'';el.style.color=isError?'#b00020':'#555'}function setWifiScanBusy(busy){wifiScanBusy=!!busy;let btn=document.getElementById('wifi_scan_btn');if(btn){btn.disabled=wifiScanBusy;btn.textContent=wifiScanBusy?'Scanning...':'Scan'}}function populateWifiOptions(list){let dl=document.getElementById('ssidOptions');if(!dl)return;dl.innerHTML='';if(!Array.isArray(list))list=[];let sorted=list.slice().sort((a,b)=>((b&&b.rssi)||-100)-((a&&a.rssi)||-100));sorted.forEach(net=>{if(!net||!net.ssid)return;let opt=document.createElement('option');opt.value=net.ssid;let label=net.ssid;if(net.rssi!=null){label+=' ('+net.rssi+' dBm';label+=net.secure?' secured':' open';label+=')'}opt.textContent=label;dl.appendChild(opt)});if(sorted.length){let inp=document.getElementById('ssid');if(inp&&!inp.value){inp.value=sorted[0].ssid}}}function scanWifiNetworks(){if(wifiScanBusy)return;setWifiScanBusy(true);setWifiScanStatus('Scanning for networks...');fetch('/api/wifi_scan',{cache:'no-store'}).then(r=>r.json()).then(d=>{if(!d||d.ok===false){throw new Error((d&&d.error)||'scan failed')}let nets=Array.isArray(d.networks)?d.networks:[];populateWifiOptions(nets);let count=nets.length||0;setWifiScanStatus(count?('Found '+count+' network'+(count===1?'':'s')+'.'):'No networks found.');}).catch(err=>{setWifiScanStatus('Scan failed: '+(err&&err.message?err.message:err),true);}).then(()=>{setWifiScanBusy(false);})}function loadWifiState(){fetch('/api/wifi').then(r=>r.json()).then(s=>{let el=document.getElementById('wifi_state');let ap=s.ap_enabled?('AP '+s.ap_ssid+' @ '+s.ap_ip):'AP off';let sta=(s.status==3)?('STA '+s.ssid+' @ '+s.sta_ip):'STA disconnected';el.textContent=sta+' | '+ap;let inp=document.getElementById('ssid');if(inp&&s.ssid&&!inp.value){inp.value=s.ssid}})}function startAp(){fetch('/api/ap_start',{method:'POST'}).then(()=>loadWifiState())}function stopAp(){fetch('/api/ap_stop',{method:'POST'}).then(()=>loadWifiState())}function rebootDevice(){if(confirm('Are you sure you want to reboot the device?')){fetch('/api/reboot',{method:'POST'}).then(()=>{alert('Device is rebooting... Please wait 10 seconds and refresh the page.')}).catch(()=>{alert('Device is rebooting... Please wait 10 seconds and refresh the page.')})}}function closeDeviceLoginModal(){document.getElementById('dialog-devicelogin').close()}function openDeviceLoginModal(){fetch('/api/startDevicelogin').then(r=>r.json()).then(d=>{if(d&&d.user_code){document.getElementById('btn_open').href=d.verification_uri;document.getElementById('lbl_message').innerText=d.message;document.getElementById('code_field').value=d.user_code}document.getElementById('dialog-devicelogin').showModal()})}window.onload=loadCfg;</script>");
	s += F("</div></div></body></html>");
	server.sendHeader("Cache-Control", "no-store, no-cache, must-revalidate, max-age=0");
	server.sendHeader("Pragma", "no-cache");
	server.sendHeader("Expires", "0");
	server.send(200, "text/html; charset=utf-8", s);
}

// Logs page UI
void handleLogsUi() {
	String s;
	appendHeadWithStyles(s, "StatusGlow - Logs", LOGS_EXTRA_STYLE);
	appendLiveTopBarAndScript(s);
	appendBrandHeaderAndTabs(s, "Logs", "logs");
	s += F("<div class='card'>");
	s += F("<div class='controls'><label>Show last <input id='cnt' type='number' min='1' max='120' value='50' style='width:5rem'></label><label><input id='auto' type='checkbox' checked> Auto-refresh</label><button class='btn' onclick='reloadLogs()'>Reload</button><span id='meta' style='margin-left:.5rem;color:#555;font-size:.9rem'></span></div>");
	extern String getLogsText(int n);
	String initial = htmlEscape(getLogsText(50));
	s += "<pre id='logbox' class='logbox'>" + initial + "</pre>";
	s += F("<script>function renderLogs(d){let box=document.getElementById('logbox');let meta=document.getElementById('meta');let lines=(d&&d.lines)?d.lines:[];box.textContent=(lines.length?lines.join('\\n'):'(no logs)');box.scrollTop=box.scrollHeight;if(meta){let cnt=(d&&d.count!=null)?d.count:'?';let cap=(d&&d.capacity!=null)?d.capacity:'?';let ts=new Date();meta.textContent='['+lines.length+' shown, '+cnt+' stored, cap '+cap+'] '+ts.toLocaleTimeString()}}function loadLogs(){let n=parseInt(document.getElementById('cnt').value)||50;fetch('/api/logs?n='+n,{cache:'no-store'}).then(r=>{if(!r.ok)throw new Error('HTTP '+r.status);return r.json()}).then(d=>{renderLogs(d)}).catch(e=>{let box=document.getElementById('logbox');box.textContent='Error loading logs: '+e})}function reloadLogs(){loadLogs()}function tick(){let a=document.getElementById('auto');if(!a||a.checked){loadLogs()}}window.onload=function(){let box=document.getElementById('logbox');if(box&&!box.textContent)box.textContent='Loading...';loadLogs();let iv=(window.CFG&&CFG.MIN_REFRESH_MS)||2000;setInterval(tick,iv)}</script>");
	s += F("</div></div></body></html>");
	server.sendHeader("Cache-Control", "no-store, no-cache, must-revalidate, max-age=0");
	server.sendHeader("Pragma", "no-cache");
	server.sendHeader("Expires", "0");
	server.send(200, "text/html; charset=utf-8", s);
}

// Firmware update page UI
void handleFirmwareUi() {
	String s;
	appendHeadWithStyles(s, "StatusGlow - Firmware", FW_EXTRA_STYLE);
	appendLiveTopBarAndScript(s);
	appendBrandHeaderAndTabs(s, "Firmware", "fw");
	s += F("<div class='card'>");
	s += F("<section><h3>Current version</h3><div class='kv'><strong>Version:</strong> <span id='ver'></span></div><div class='kv'><strong>Sketch:</strong> <span id='sketch'></span></div><div class='kv'><strong>Flash:</strong> <span id='flash'></span></div><div class='kv'><strong>SDK:</strong> <span id='sdk'></span></div></section>");
	s += F("<section class='mt'><h3>Upload new firmware</h3><form id='fwform' method='POST' action='/update' enctype='multipart/form-data'><input type='file' name='firmware' accept='.bin,.bin.gz' required><button class='btn' type='submit'>Upload and Install</button><button class='btn' type='button' id='btn_view_ota' title='Open the last OTA log'>View last OTA log</button></form><div class='hint mt-s'>Device will reboot automatically after a successful update.</div><div id='fw_status' class='mt-s' style='color:#555'></div></section><dialog id='dlg_ota'><h3>Last OTA log</h3><pre id='ota_log' class='logarea'>(loading...)</pre><div class='dialog-menu'><button class='btn' id='ota_close'>Close</button></div></dialog>");
	s += F("<script>function fillInfo(s){if(!s)return;document.getElementById('ver').textContent=s.sketch_version||'';var used=(s.sketch_size!=null?s.sketch_size:0),free=(s.free_sketch_space!=null?s.free_sketch_space:0);document.getElementById('sketch').textContent=used+' used, '+free+' free for OTA';document.getElementById('flash').textContent=(s.flash_chip_size||'')+' @ '+(s.flash_chip_speed||'')+' Hz';document.getElementById('sdk').textContent=s.sdk_version||''}function loadInfo(){fetch('/api/settings').then(r=>r.json()).then(fillInfo)}function onFwSubmit(ev){ev.preventDefault();var st=document.getElementById('fw_status');st.textContent='Uploading...';var fd=new FormData(document.getElementById('fwform'));fetch('/update',{method:'POST',body:fd}).then(r=>r.text()).then(t=>{if((t||'').indexOf('OK')>=0){st.textContent='Upload complete. Rebooting...';setTimeout(()=>{location.reload()},5000)}else{st.textContent='Update failed: '+t}}).catch(e=>{st.textContent='Upload error: '+e})}function propagateKey(){try{var u=new URL(location.href);var k=u.searchParams.get('key');if(!k)return;var f=document.getElementById('fwform');if(!f)return;var h=document.createElement('input');h.type='hidden';h.name='key';h.value=k;f.appendChild(h)}catch(e){}}function openOtaLog(){var dlg=document.getElementById('dlg_ota');var pre=document.getElementById('ota_log');pre.textContent='(loading...)';dlg.showModal();fetch('/api/ota_last',{cache:'no-store'}).then(r=>{if(r.status===404){return 'No OTA log saved yet.'}return r.text()}).then(t=>{pre.textContent=t||'(empty)'}).catch(e=>{pre.textContent='Error loading log: '+e})}window.onload=function(){loadInfo();propagateKey();document.getElementById('fwform').addEventListener('submit',onFwSubmit);var b=document.getElementById('btn_view_ota');if(b){b.addEventListener('click',openOtaLog)}var c=document.getElementById('ota_close');if(c){c.addEventListener('click',function(){document.getElementById('dlg_ota').close()})}}</script>");
	s += F("</div></div></body></html>");
	server.sendHeader("Cache-Control", "no-store, no-cache, must-revalidate, max-age=0");
	server.sendHeader("Pragma", "no-cache");
	server.sendHeader("Expires", "0");
	server.send(200, "text/html; charset=utf-8", s);
}

