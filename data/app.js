(function () {
  const APP = {
    route: "home",
    settings: null,
    modes: [],
    current: null,
    effects: null,
    preview: { enabled: false, key: "" },
    initialized: {
      navigation: false,
      topbar: false,
      home: false,
      homeLoop: false,
      config: false,
      effects: false,
      effectsLoop: false,
      logs: false,
      logsLoop: false,
      fw: false
    },
    refreshers: {
      home: null,
      config: null
    },
    effectsEditor: {
      selectedKey: "",
      previewTimer: null,
      previewNonce: 0
    },
    ui: {
      CPU_OK: 50,
      CPU_WARN: 80,
      HEAP_OK: 60,
      HEAP_WARN: 85,
      RSSI: [-85, -75, -65, -55],
      BAR_HEIGHTS: [6, 10, 14, 18],
      MIN_REFRESH_MS: 2000
    }
  };
  const EFFECT_COLOR_PRESETS = [
    { name: "Available", hex: "#00ff00" },
    { name: "Focused", hex: "#32d6ff" },
    { name: "Sky", hex: "#3f8cff" },
    { name: "Call", hex: "#7a5cff" },
    { name: "Magenta", hex: "#d91f8b" },
    { name: "Busy", hex: "#ff5a5f" },
    { name: "Warm", hex: "#ff9f1c" },
    { name: "Away", hex: "#fff200" },
    { name: "Soft White", hex: "#f4f7ff" },
    { name: "Off", hex: "#000000" }
  ];

  function $(id) {
    return document.getElementById(id);
  }

  function safeText(el, value) {
    if (el) el.textContent = value == null ? "" : String(value);
  }

  function setMessage(id, message, isError) {
    const el = $(id);
    if (!el) return;
    safeText(el, message || "");
    el.style.color = isError ? "#b00020" : "";
  }

  function cfgPayloadFromForm() {
    return {
      client_id: $("cfg-client-id").value || "",
      tenant: $("cfg-tenant").value || "",
      poll_interval: parseInt($("cfg-poll").value, 10) || 30,
      led_type_rgbw: $("cfg-led-type").value === "true",
      status_led_enabled: $("cfg-status-led").checked
    };
  }

  function configHasUnsavedChanges(payload) {
    const current = APP.settings || {};
    return (current.client_id || "") !== (payload.client_id || "") ||
      (current.tenant || "") !== (payload.tenant || "") ||
      (parseInt(current.poll_interval, 10) || 30) !== (payload.poll_interval || 30) ||
      (!!current.led_type_rgbw) !== (!!payload.led_type_rgbw) ||
      (!!current.status_led_enabled) !== (!!payload.status_led_enabled);
  }

  async function saveConfigPayload(payload) {
    return fetchJson("/api/settings", {
      method: "POST",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify(payload)
    });
  }

  function normalizePath(pathname) {
    let path = pathname || "/";
    if (path.charAt(0) !== "/") path = "/" + path;
    path = path.replace(/\/+$/, "");
    if (!path) path = "/";
    if (path === "/index.html") return "/";
    return path;
  }

  function routeFromPath(pathname) {
    const path = normalizePath(pathname);
    if (path === "/config") return "config";
    if (path === "/effects") return "effects";
    if (path === "/logs") return "logs";
    if (path === "/fw") return "fw";
    return "home";
  }

  function pathFromRoute(route) {
    return route === "home" ? "/" : "/" + route;
  }

  function getKey() {
    try {
      return new URL(window.location.href).searchParams.get("key") || "";
    } catch (err) {
      return "";
    }
  }

  function addKeyToUrl(value) {
    const key = getKey();
    if (!key) return value;
    const url = new URL(value, window.location.href);
    if (url.origin === window.location.origin && !url.searchParams.has("key")) {
      url.searchParams.set("key", key);
    }
    return url.toString();
  }

  function authFetch(input, init) {
    const key = getKey();
    const nextInit = init ? Object.assign({}, init) : {};
    const target = typeof input === "string" ? addKeyToUrl(input) : input;
    if (key) {
      const headers = new Headers(nextInit.headers || {});
      if (!headers.has("X-StatusGlow-Key")) headers.set("X-StatusGlow-Key", key);
      if (!headers.has("X-OTA-Key")) headers.set("X-OTA-Key", key);
      nextInit.headers = headers;
    }
    return fetch(target, nextInit);
  }

  async function fetchJson(url, init) {
    const response = await authFetch(url, init);
    const text = await response.text();
    let data = null;
    try {
      data = text ? JSON.parse(text) : null;
    } catch (err) {
      if (!response.ok) throw new Error("HTTP " + response.status);
      throw err;
    }
    if (!response.ok) {
      const message = data && data.message ? data.message : data && data.error ? data.error : "HTTP " + response.status;
      const error = new Error(message);
      error.status = response.status;
      error.data = data;
      throw error;
    }
    return data;
  }

  function setTheme(theme) {
    try {
      localStorage.setItem("theme", theme);
    } catch (err) {}
    document.documentElement.setAttribute("data-theme", theme);
  }

  function initTheme() {
    let theme = "light";
    try {
      theme = localStorage.getItem("theme") || "";
    } catch (err) {}
    if (!theme && window.matchMedia && window.matchMedia("(prefers-color-scheme: dark)").matches) {
      theme = "dark";
    }
    setTheme(theme || "light");
    $("theme_toggle").addEventListener("click", function () {
      const current = document.documentElement.getAttribute("data-theme") || "light";
      setTheme(current === "dark" ? "light" : "dark");
    });
  }

  function syncProtectedLinks() {
    const key = getKey();
    if (!key) return;
    document.querySelectorAll('a[href^="/"]').forEach(function (anchor) {
      anchor.href = addKeyToUrl(anchor.getAttribute("href"));
    });
    document.querySelectorAll('form[action^="/"]').forEach(function (form) {
      form.action = addKeyToUrl(form.getAttribute("action"));
    });
  }

  function ensureHiddenKey(form) {
    const key = getKey();
    if (!key || !form) return;
    let input = form.querySelector('input[name="key"]');
    if (!input) {
      input = document.createElement("input");
      input.type = "hidden";
      input.name = "key";
      form.appendChild(input);
    }
    input.value = key;
  }

  function applyUiSettings(settings) {
    if (!settings || !settings.ui) return;
    const ui = settings.ui;
    APP.ui.CPU_OK = ui.cpu_ok != null ? ui.cpu_ok : APP.ui.CPU_OK;
    APP.ui.CPU_WARN = ui.cpu_warn != null ? ui.cpu_warn : APP.ui.CPU_WARN;
    APP.ui.HEAP_OK = ui.heap_ok != null ? ui.heap_ok : APP.ui.HEAP_OK;
    APP.ui.HEAP_WARN = ui.heap_warn != null ? ui.heap_warn : APP.ui.HEAP_WARN;
    APP.ui.MIN_REFRESH_MS = ui.min_refresh_ms != null ? ui.min_refresh_ms : APP.ui.MIN_REFRESH_MS;
    if (Array.isArray(ui.rssi) && ui.rssi.length === 4) APP.ui.RSSI = ui.rssi.slice(0, 4);
    if (Array.isArray(ui.bar_heights) && ui.bar_heights.length === 4) APP.ui.BAR_HEIGHTS = ui.bar_heights.slice(0, 4);
  }

  function pillClassFromPercent(percent, ok, warn) {
    return percent < ok ? "pill-ok" : (percent < warn ? "pill-warn" : "pill-crit");
  }

  function wifiBarsFromRssi(rssi) {
    const value = parseInt(rssi, 10);
    if (Number.isNaN(value)) return { bars: 0, cls: "bars-crit" };
    if (value >= APP.ui.RSSI[3]) return { bars: 4, cls: "bars-ok" };
    if (value >= APP.ui.RSSI[2]) return { bars: 3, cls: "bars-ok" };
    if (value >= APP.ui.RSSI[1]) return { bars: 2, cls: "bars-warn" };
    if (value >= APP.ui.RSSI[0]) return { bars: 1, cls: "bars-crit" };
    return { bars: 0, cls: "bars-crit" };
  }

  function renderWifiBars(el, rssi) {
    if (!el) return;
    const result = wifiBarsFromRssi(rssi);
    el.className = "bars " + result.cls;
    el.innerHTML = "";
    for (let i = 0; i < 4; i++) {
      const bar = document.createElement("span");
      bar.className = "bar" + (i < result.bars ? " f" : "");
      bar.style.height = APP.ui.BAR_HEIGHTS[i] + "px";
      el.appendChild(bar);
    }
  }

  function updateTopbar(settings) {
    if (!settings) return;
    const cpu = parseInt(settings.cpu_usage, 10) || 0;
    const heapTotal = parseInt(settings.heap_total, 10) || 327680;
    const heapFree = parseInt(settings.heap, 10) || 0;
    const heapUsed = Math.min(100, Math.max(0, Math.round(((Math.max(0, heapTotal - heapFree)) / heapTotal) * 100)));
    const cpuEl = $("live_cpu");
    const heapEl = $("live_heap");
    safeText(cpuEl, cpu + "%");
    safeText(heapEl, heapUsed + "%");
    if (cpuEl) cpuEl.className = "pill " + pillClassFromPercent(cpu, APP.ui.CPU_OK, APP.ui.CPU_WARN);
    if (heapEl) heapEl.className = "pill " + pillClassFromPercent(heapUsed, APP.ui.HEAP_OK, APP.ui.HEAP_WARN);
    renderWifiBars($("live_wifi"), settings.wifi_rssi);
    safeText($("brand-version"), "v" + (settings.sketch_version || "-"));
  }

  function toHex(color) {
    const value = Number(color) >>> 0;
    const r = (value >> 16) & 255;
    const g = (value >> 8) & 255;
    const b = value & 255;
    return "#" + [r, g, b].map(function (part) { return part.toString(16).padStart(2, "0"); }).join("");
  }

  function fromHex(hex) {
    if (!hex || hex.length < 7) return 0;
    return (parseInt(hex.slice(1, 3), 16) << 16) |
      (parseInt(hex.slice(3, 5), 16) << 8) |
      parseInt(hex.slice(5, 7), 16);
  }

  function normalizeHexInput(value) {
    let hex = String(value || "").trim().replace(/[^#0-9a-fA-F]/g, "");
    if (!hex) return "";
    if (hex.charAt(0) !== "#") hex = "#" + hex;
    if (hex.length === 4) {
      hex = "#" + hex.slice(1).split("").map(function (part) { return part + part; }).join("");
    }
    if (hex.length > 7) hex = hex.slice(0, 7);
    return hex.toLowerCase();
  }

  function isValidHexColor(value) {
    return /^#[0-9a-f]{6}$/i.test(String(value || ""));
  }

  function colorPresetName(hex) {
    const normalized = normalizeHexInput(hex).toLowerCase();
    const match = EFFECT_COLOR_PRESETS.find(function (preset) {
      return preset.hex.toLowerCase() === normalized;
    });
    if (match) return match.name;
    if (!isValidHexColor(normalized)) return "Custom";
    return normalized === "#000000" ? "Off" : "Custom";
  }

  function formatUptime(ms) {
    const total = Math.floor((ms || 0) / 1000);
    const hours = Math.floor(total / 3600);
    const minutes = Math.floor((total % 3600) / 60);
    const seconds = total % 60;
    return hours + ":" + String(minutes).padStart(2, "0") + ":" + String(seconds).padStart(2, "0");
  }

  function routeLabel(route) {
    if (route === "config") return "Configuration";
    if (route === "effects") return "Effects Studio";
    if (route === "logs") return "Logs";
    if (route === "fw") return "Firmware";
    return "Home";
  }

  function setActiveRoute(route) {
    APP.route = route;
    document.querySelectorAll(".page").forEach(function (page) {
      page.classList.add("hidden");
    });
    const activePage = $("page-" + route);
    if (activePage) activePage.classList.remove("hidden");
    document.querySelectorAll(".tab").forEach(function (tab) {
      tab.classList.toggle("active", tab.dataset.route === route);
    });
    safeText($("page-subtitle"), routeLabel(route));
    document.title = "StatusGlow - " + routeLabel(route);
  }

  function initNavigation() {
    if (APP.initialized.navigation) return;
    APP.initialized.navigation = true;
    document.querySelectorAll(".tab[data-route]").forEach(function (tab) {
      tab.addEventListener("click", function (event) {
        event.preventDefault();
        openRoute(tab.dataset.route || "home", true).catch(function (err) {
          console.error(err);
          alert("Navigation failed: " + err.message);
        });
      });
    });
    window.addEventListener("popstate", function () {
      openRoute(routeFromPath(window.location.pathname), false).catch(console.error);
    });
    window.addEventListener("pagehide", requestEffectsPreviewRestore);
  }

  async function openRoute(route, pushHistory) {
    const nextRoute = route || "home";
    if (APP.route === "effects" && nextRoute !== "effects") {
      try {
        await restoreLiveStatusFromEffects();
      } catch (err) {
        console.error("Failed to restore live status before route change:", err);
      }
    }
    if (pushHistory) {
      const nextUrl = addKeyToUrl(pathFromRoute(nextRoute));
      if (nextUrl !== window.location.href) {
        window.history.pushState({ route: nextRoute }, "", nextUrl);
      }
    }
    setActiveRoute(nextRoute);
    if (nextRoute !== "home") initTopbarLoop();
    if (nextRoute === "config") await initConfig();
    else if (nextRoute === "effects") await initEffects();
    else if (nextRoute === "logs") await initLogs();
    else if (nextRoute === "fw") await initFirmware();
    else await initHome();
  }

  async function loadSettings() {
    APP.settings = await fetchJson("/api/settings", { cache: "no-store" });
    applyUiSettings(APP.settings);
    updateTopbar(APP.settings);
    return APP.settings;
  }

  async function loadModes() {
    APP.modes = await fetchJson("/api/modes", { cache: "no-store" });
    return APP.modes;
  }

  async function loadCurrent() {
    APP.current = await fetchJson("/api/current", { cache: "no-store" });
    return APP.current;
  }

  async function loadLedFrame() {
    return fetchJson("/api/led_frame", { cache: "no-store" });
  }

  function modeName(id) {
    const found = (APP.modes || []).find(function (mode) {
      return parseInt(mode.id, 10) === parseInt(id, 10);
    });
    return found ? found.name + " (" + found.id + ")" : "#" + id;
  }

  function fillHome(settings, current) {
    safeText($("home-poll-interval"), settings.poll_interval);
    safeText($("home-num-leds"), settings.num_leds);
    safeText($("home-rssi"), settings.wifi_rssi);
    safeText($("home-ssid"), settings.wifi_ssid);
    safeText($("home-ip"), settings.wifi_ip);
    safeText($("home-heap"), settings.heap);
    safeText($("home-min-heap"), settings.min_heap);
    safeText($("home-cpu-freq"), settings.cpu_freq != null ? settings.cpu_freq + " MHz" : "");
    safeText($("home-version"), settings.sketch_version);
    safeText($("home-uptime"), formatUptime(settings.uptime_ms));
    renderWifiBars($("home-wifi-bars"), settings.wifi_rssi);

    const wifiStatus = parseInt(settings.wifi_status, 10);
    const staText = wifiStatus === 3
      ? "STA " + ((settings.wifi_ssid || "") ? settings.wifi_ssid + " @ " : "") + (settings.wifi_ip || "")
      : "STA " + ((settings.wifi_ssid || "") ? settings.wifi_ssid + " (disconnected)" : "disconnected");
    const apText = "AP " + (settings.ap_ssid || "") + (settings.ap_enabled ? ((settings.ap_ip || "") ? " @ " + settings.ap_ip : "") : " (off)");
    safeText($("home-network"), staText + " | " + apText);

    const sketchUsed = Number(settings.sketch_size) || 0;
    const sketchFree = Number(settings.free_sketch_space) || 0;
    const heapTotal = Number(settings.heap_total) || 0;
    const heapFree = Number(settings.heap) || 0;
    safeText($("home-sketch-text"), "Sketch: " + sketchUsed + " bytes used, " + sketchFree + " bytes free for OTA");
    safeText($("home-ram-text"), "RAM: " + heapFree + " of " + heapTotal + " bytes free");
    $("home-sketch-progress").max = Math.max(1, sketchUsed + sketchFree);
    $("home-sketch-progress").value = sketchUsed;
    $("home-ram-progress").max = Math.max(1, heapTotal);
    $("home-ram-progress").value = Math.max(0, heapTotal - heapFree);

    if (current) {
      const currentText = (current.activity || "Presence Unknown") +
        " \u2022 " + modeName(current.mode) +
        " \u2022 " + (current.speed || 0) + "s" +
        (current.reverse ? " \u2022 Reverse" : "") +
        " \u2022 " + toHex(current.color);
      safeText($("home-current-line"), currentText);
      $("home-swatch").style.backgroundColor = toHex(current.color);
    }
  }

  async function initHome() {
    if (!APP.refreshers.home) {
      APP.refreshers.home = async function () {
        if (!APP.modes.length) await loadModes();
        const result = await Promise.all([loadSettings(), loadCurrent()]);
        fillHome(result[0], result[1]);
      };
    }
    await APP.refreshers.home();
    if (!APP.initialized.homeLoop) {
      APP.initialized.homeLoop = true;
      const pollSeconds = parseInt(APP.settings.poll_interval, 10) || 0;
      const interval = Math.max(APP.ui.MIN_REFRESH_MS, pollSeconds > 0 ? pollSeconds * 1000 : APP.ui.MIN_REFRESH_MS);
      setInterval(function () {
        APP.refreshers.home().catch(console.error);
      }, interval);
    }
    APP.initialized.home = true;
  }

  function fillConfig(settings) {
    $("cfg-client-id").value = settings.client_id || "";
    $("cfg-tenant").value = settings.tenant || "";
    $("cfg-poll").value = parseInt(settings.poll_interval, 10) || 30;
    $("cfg-led-type").value = settings.led_type_rgbw ? "true" : "false";
    $("cfg-status-led").checked = !!settings.status_led_enabled;
  }

  async function loadWifiState() {
    const wifi = await fetchJson("/api/wifi", { cache: "no-store" });
    const ap = wifi.ap_enabled ? "AP " + wifi.ap_ssid + " @ " + wifi.ap_ip : "AP off";
    const sta = parseInt(wifi.status, 10) === 3 ? "STA " + wifi.ssid + " @ " + wifi.sta_ip : "STA disconnected";
    safeText($("cfg-wifi-state"), sta + " | " + ap);
    const preferredSsid = wifi.ssid || wifi.saved_ssid || "";
    if (preferredSsid && !$("cfg-ssid").value) $("cfg-ssid").value = preferredSsid;

    const apCopy = wifi.ap_enabled ?
      ('The local AP is currently ON ("' + wifi.ap_ssid + '" @ ' + wifi.ap_ip + ').') :
      ('The local AP is currently OFF; if STA does not connect after reboot, it will start as "' + wifi.ap_ssid + '".');
    safeText($("cfg-reset-copy"), "This will erase Wi-Fi credentials, app configuration, and effects, then reboot. " + apCopy);
    return wifi;
  }

  function populateWifiOptions(networks) {
    const datalist = $("cfg-ssid-options");
    datalist.innerHTML = "";
    const sorted = (Array.isArray(networks) ? networks.slice() : []).sort(function (a, b) {
      return ((b && b.rssi) || -100) - ((a && a.rssi) || -100);
    });
    sorted.forEach(function (network) {
      if (!network || !network.ssid) return;
      const option = document.createElement("option");
      let label = network.ssid;
      if (network.rssi != null) {
        label += " (" + network.rssi + " dBm";
        label += network.secure ? " secured" : " open";
        label += ")";
      }
      option.value = network.ssid;
      option.textContent = label;
      datalist.appendChild(option);
    });
    if (sorted.length && !$("cfg-ssid").value) $("cfg-ssid").value = sorted[0].ssid;
  }

  function sleep(ms) {
    return new Promise(function (resolve) {
      window.setTimeout(resolve, ms);
    });
  }

  async function pollWifiConnectionStatus() {
    const startedAt = Date.now();
    while (Date.now() - startedAt < 30000) {
      const wifi = await fetchJson("/api/wifi", { cache: "no-store" });
      const connect = wifi && wifi.connect ? wifi.connect : {};
      if (connect.state !== "running") {
        return wifi;
      }
      setMessage("cfg-status", "Connecting to Wi-Fi...");
      await sleep(750);
    }
    throw new Error("connect_timeout");
  }

  async function pollWifiScanStatus() {
    const startedAt = Date.now();
    while (Date.now() - startedAt < 30000) {
      const scan = await fetchJson("/api/wifi_scan", { cache: "no-store" });
      if (scan.state !== "running") {
        return scan;
      }
      safeText($("cfg-wifi-scan-status"), "Scanning for networks...");
      await sleep(500);
    }
    throw new Error("scan_timeout");
  }

  function isConflictError(err, code) {
    return !!(err && err.status === 409 && (err.message === code || (err.data && err.data.message === code)));
  }

  async function initConfig() {
    if (!APP.refreshers.config) {
      APP.refreshers.config = async function () {
        const settings = await loadSettings();
        fillConfig(settings);
        await loadWifiState();
      };
    }
    if (!APP.initialized.config) {
      APP.initialized.config = true;
      let wifiScanBusy = false;

      function showDeviceLoginDialog(result) {
        $("cfg-device-login-open-link").href = result.verification_uri_complete || result.verification_uri || "https://microsoft.com/devicelogin";
        safeText($("cfg-device-login-message"), result.message || "Use the code below to finish Microsoft device login.");
        $("cfg-device-login-code").value = result.user_code || "";
        $("cfg-device-login-dialog").showModal();
      }

      $("cfg-form").addEventListener("submit", async function (event) {
        event.preventDefault();
        setMessage("cfg-status", "Saving settings...");
        try {
          const payload = cfgPayloadFromForm();
          const result = await saveConfigPayload(payload);
          if (result && result.needs_reboot) {
            setMessage("cfg-status", "Settings saved. LED type changed, so a reboot is required.");
          } else {
            setMessage("cfg-status", "Settings saved.");
          }
          await APP.refreshers.config();
        } catch (err) {
          setMessage("cfg-status", "Saving settings failed: " + err.message, true);
        }
      });

      $("cfg-connect-btn").addEventListener("click", async function () {
        setMessage("cfg-status", "Connecting to Wi-Fi...");
        try {
          try {
            await fetchJson("/api/wifi", {
              method: "POST",
              headers: { "Content-Type": "application/json" },
              body: JSON.stringify({
                ssid: ($("cfg-ssid").value || "").trim(),
                password: $("cfg-wifi-password").value || ""
              })
            });
          } catch (err) {
            if (!isConflictError(err, "connect_in_progress")) throw err;
          }
          const wifi = await pollWifiConnectionStatus();
          await loadWifiState();
          const connect = wifi && wifi.connect ? wifi.connect : {};
          if (connect.state === "success") {
            const hostPart = wifi.host_local ? " Hostname: http://" + wifi.host_local + "/" : "";
            setMessage("cfg-status", "Connected to " + (wifi.ssid || "Wi-Fi") + ". IP: http://" + wifi.sta_ip + "/" + hostPart);
          } else {
            setMessage("cfg-status", "Wi-Fi connection failed. Please verify the SSID and password.", true);
          }
        } catch (err) {
          setMessage("cfg-status", "Wi-Fi connection failed: " + err.message, true);
        }
      });

      $("cfg-reload-btn").addEventListener("click", function () {
        APP.refreshers.config().then(function () {
          setMessage("cfg-status", "Configuration reloaded.");
        }).catch(function (err) {
          setMessage("cfg-status", "Reload failed: " + err.message, true);
        });
      });

      $("cfg-wifi-scan-btn").addEventListener("click", async function () {
        if (wifiScanBusy) return;
        wifiScanBusy = true;
        $("cfg-wifi-scan-btn").disabled = true;
        $("cfg-wifi-scan-btn").textContent = "Scanning...";
        safeText($("cfg-wifi-scan-status"), "Scanning for networks...");
        $("cfg-wifi-scan-status").style.color = "";
        try {
          try {
            await fetchJson("/api/wifi_scan", { method: "POST" });
          } catch (err) {
            if (!isConflictError(err, "scan_in_progress")) throw err;
          }
          const result = await pollWifiScanStatus();
          const networks = Array.isArray(result.networks) ? result.networks : [];
          populateWifiOptions(networks);
          safeText($("cfg-wifi-scan-status"), networks.length ? "Found " + networks.length + " network" + (networks.length === 1 ? "." : "s.") : "No networks found.");
        } catch (err) {
          safeText($("cfg-wifi-scan-status"), "Scan failed: " + err.message);
          $("cfg-wifi-scan-status").style.color = "#b00020";
        } finally {
          wifiScanBusy = false;
          $("cfg-wifi-scan-btn").disabled = false;
          $("cfg-wifi-scan-btn").textContent = "Scan";
        }
      });

      $("cfg-ap-start-btn").addEventListener("click", async function () {
        setMessage("cfg-status", "Starting AP...");
        try {
          const result = await fetchJson("/api/ap_start", { method: "POST" });
          await loadWifiState();
          setMessage("cfg-status", "AP started: " + (result.ap_ssid || "") + (result.ap_ip ? " @ " + result.ap_ip : ""));
        } catch (err) {
          setMessage("cfg-status", "Starting AP failed: " + err.message, true);
        }
      });

      $("cfg-ap-stop-btn").addEventListener("click", async function () {
        setMessage("cfg-status", "Stopping AP...");
        try {
          await fetchJson("/api/ap_stop", { method: "POST" });
          await loadWifiState();
          setMessage("cfg-status", "AP stopped.");
        } catch (err) {
          setMessage("cfg-status", "Stopping AP failed: " + err.message, true);
        }
      });

      $("cfg-reboot-btn").addEventListener("click", async function () {
        if (!window.confirm("Are you sure you want to reboot the device?")) return;
        setMessage("cfg-status", "Rebooting device...");
        try {
          await fetchJson("/api/reboot", { method: "POST" });
        } catch (err) {}
        setMessage("cfg-status", "Device is rebooting. Please wait about 10 seconds and refresh.");
      });

      $("cfg-factory-reset-btn").addEventListener("click", function () {
        $("cfg-reset-dialog").showModal();
      });

      $("cfg-reset-cancel-btn").addEventListener("click", function (event) {
        event.preventDefault();
        $("cfg-reset-dialog").close();
      });

      $("cfg-reset-confirm-btn").addEventListener("click", async function (event) {
        event.preventDefault();
        try {
          await fetchJson("/api/clearSettings", { method: "POST", cache: "no-store" });
          $("cfg-reset-dialog").close();
          $("cfg-reset-result-dialog").showModal();
        } catch (err) {
          $("cfg-reset-dialog").close();
          setMessage("cfg-status", "Factory reset failed: " + err.message, true);
        }
      });

      $("cfg-device-login-btn").addEventListener("click", async function () {
        const payload = cfgPayloadFromForm();
        if (!(payload.client_id || "").trim()) {
          setMessage("cfg-status", "Enter and save the Microsoft Client ID before starting device login.", true);
          return;
        }
        if (!(payload.tenant || "").trim()) {
          setMessage("cfg-status", "Enter and save the Microsoft tenant before starting device login.", true);
          return;
        }
        try {
          if (configHasUnsavedChanges(payload)) {
            setMessage("cfg-status", "Saving settings before starting device login...");
            const saveResult = await saveConfigPayload(payload);
            if (saveResult && saveResult.needs_reboot) {
              setMessage("cfg-status", "Save completed, but a reboot is required before device login because LED type changed.", true);
              return;
            }
            await APP.refreshers.config();
          }
          setMessage("cfg-status", "Starting device login...");
          const result = await fetchJson("/api/startDevicelogin", { cache: "no-store" });
          if (result && result.user_code) showDeviceLoginDialog(result);
          setMessage("cfg-status", "Device login started. Complete the Microsoft sign-in in the dialog.");
        } catch (err) {
          if (err && err.status === 409 && err.data && err.data.user_code) {
            showDeviceLoginDialog(err.data);
            setMessage("cfg-status", err.message || "A device login is already waiting for approval.");
            return;
          }
          setMessage("cfg-status", "Device login failed: " + err.message, true);
        }
      });

      $("cfg-device-login-close-btn").addEventListener("click", function (event) {
        event.preventDefault();
        $("cfg-device-login-dialog").close();
      });
    }
    await APP.refreshers.config();
  }

  function buildModeSelect(value) {
    const select = document.createElement("select");
    select.dataset.k = "mode";
    APP.modes.forEach(function (mode) {
      const option = document.createElement("option");
      option.value = mode.id;
      option.textContent = mode.name + " (" + mode.id + ")";
      if (parseInt(value, 10) === parseInt(mode.id, 10)) option.selected = true;
      select.appendChild(option);
    });
    return select;
  }

  function effectProfiles() {
    return APP.effects && Array.isArray(APP.effects.profiles) ? APP.effects.profiles : [];
  }

  function normalizeEffectProfile(profile) {
    return {
      key: profile.key,
      mode: parseInt(profile.mode, 10) || 0,
      speed: parseFloat(profile.speed) || 0,
      color: Number(profile.color) >>> 0,
      fade_ms: parseInt(profile.fade_ms, 10) || 0,
      bri: parseInt(profile.bri, 10) || 0,
      reverse: !!profile.reverse
    };
  }

  function getEffectProfileByKey(key) {
    const found = effectProfiles().find(function (profile) {
      return profile.key === key;
    });
    return found ? normalizeEffectProfile(found) : null;
  }

  function buildEffectsPayload(profiles) {
    const uiBrightness = Math.min(100, Math.max(0, parseInt($("fx-brightness").value, 10) || 0));
    return {
      fade_ms: parseInt($("fx-fade").value, 10) || 0,
      brightness: Math.round((uiBrightness * 255) / 100),
      gamma: parseFloat($("fx-gamma").value) || 2.2,
      profiles: profiles.map(function (profile) {
        return normalizeEffectProfile(profile);
      })
    };
  }

  function fillEffectModeSelect(value) {
    const select = $("fx-profile-mode");
    select.innerHTML = "";
    APP.modes.forEach(function (mode) {
      const option = document.createElement("option");
      option.value = mode.id;
      option.textContent = mode.name + " (" + mode.id + ")";
      if (parseInt(value, 10) === parseInt(mode.id, 10)) option.selected = true;
      select.appendChild(option);
    });
  }

  function ensureEffectSelection() {
    const profiles = effectProfiles();
    if (!profiles.length) {
      APP.effectsEditor.selectedKey = "";
      return "";
    }
    const currentKey = APP.effectsEditor.selectedKey;
    if (currentKey && getEffectProfileByKey(currentKey)) return currentKey;
    if (APP.preview && APP.preview.key && getEffectProfileByKey(APP.preview.key)) {
      APP.effectsEditor.selectedKey = APP.preview.key;
      return APP.effectsEditor.selectedKey;
    }
    APP.effectsEditor.selectedKey = profiles[0].key;
    return APP.effectsEditor.selectedKey;
  }

  function effectProfilePayloadFromForm() {
    const percent = Math.min(100, Math.max(0, parseFloat($("fx-profile-brightness").value) || 0));
    return normalizeEffectProfile({
      key: APP.effectsEditor.selectedKey,
      mode: parseInt($("fx-profile-mode").value, 10) || 0,
      speed: parseFloat($("fx-profile-speed").value) || 0,
      color: fromHex($("fx-profile-color").value),
      fade_ms: parseInt($("fx-profile-fade").value, 10) || 0,
      bri: percent <= 0 ? 0 : Math.round((percent * 255) / 100),
      reverse: false
    });
  }

  function renderEffectEditorSummaryLegacy(profile) {
    if (!profile) {
      safeText($("fx-editor-summary"), "No status profiles available.");
      return;
    }
    const brightnessText = profile.bri ? Math.round((profile.bri * 100) / 255) + "% brightness" : "using global brightness";
    const fadeText = profile.fade_ms ? profile.fade_ms + " ms fade" : "using global fade";
    safeText(
      $("fx-editor-summary"),
      "Editing " + profile.key + " • " + modeName(profile.mode) + " • " + (profile.speed || 0) + "s • " + fadeText + " • " + brightnessText
    );
  }

  function setEffectColorChooser(hex) {
    const normalized = normalizeHexInput(hex);
    const value = isValidHexColor(normalized) ? normalized : "#000000";
    if ($("fx-profile-color").value !== value) $("fx-profile-color").value = value;
    if ($("fx-profile-color-hex").value !== value) $("fx-profile-color-hex").value = value;
    if ($("fx-profile-color-preview")) {
      $("fx-profile-color-preview").style.background = value;
      $("fx-profile-color-preview").style.boxShadow = "inset 0 1px 0 rgba(255,255,255,.34), 0 0 24px " + value + "55";
    }
    safeText($("fx-profile-color-name"), colorPresetName(value));
    Array.prototype.forEach.call(document.querySelectorAll(".color-preset"), function (button) {
      const isActive = String(button.dataset.hex || "").toLowerCase() === value;
      button.classList.toggle("is-active", isActive);
      button.setAttribute("aria-pressed", isActive ? "true" : "false");
    });
  }

  function renderEffectColorPresets() {
    const host = $("fx-profile-color-presets");
    if (!host || host.childElementCount) return;
    EFFECT_COLOR_PRESETS.forEach(function (preset) {
      const button = document.createElement("button");
      button.type = "button";
      button.className = "color-preset";
      button.dataset.hex = preset.hex.toLowerCase();
      button.setAttribute("aria-label", preset.name + " " + preset.hex);
      button.innerHTML =
        '<span class="color-preset-swatch" style="background:' + preset.hex + '"></span>' +
        '<span class="color-preset-name">' + preset.name + '</span>' +
        '<span class="color-preset-value">' + preset.hex + "</span>";
      button.addEventListener("click", function () {
        setEffectColorChooser(preset.hex);
        renderEffectEditorSummary(effectProfilePayloadFromForm());
        scheduleEditorPreview();
      });
      host.appendChild(button);
    });
  }

  function fillEffectEditor(profile) {
    const select = $("fx-profile-select");
    if (select.value !== profile.key) select.value = profile.key;
    fillEffectModeSelect(profile.mode);
    $("fx-profile-speed").value = profile.speed || 0;
    $("fx-profile-speed-range").value = profile.speed || 0;
    setEffectColorChooser(toHex(profile.color >>> 0));
    $("fx-profile-fade").value = profile.fade_ms || 0;
    $("fx-profile-brightness").value = profile.bri ? Math.round((profile.bri * 100) / 255) : 0;
    renderEffectEditorSummary(profile);
  }

  function renderEffectEditorSummary(profile) {
    if (!profile) {
      safeText($("fx-editor-summary"), "No status profiles available.");
      return;
    }
    const brightnessText = profile.bri ? Math.round((profile.bri * 100) / 255) + "% brightness" : "using global brightness";
    const fadeText = profile.fade_ms ? profile.fade_ms + " ms fade" : "using global fade";
    safeText(
      $("fx-editor-summary"),
      "Editing " + profile.key + " | " + modeName(profile.mode) + " | " + (profile.speed || 0) + "s | " + fadeText + " | " + brightnessText
    );
  }

  function populateEffectProfileSelect() {
    const select = $("fx-profile-select");
    const previous = select.value;
    select.innerHTML = "";
    effectProfiles().forEach(function (profile) {
      const option = document.createElement("option");
      option.value = profile.key;
      option.textContent = profile.key;
      if (profile.key === previous) option.selected = true;
      select.appendChild(option);
    });
  }

  async function ensurePreviewSelection(key) {
    if (!APP.preview.enabled) {
      APP.preview = await fetchJson("/api/preview_mode", {
        method: "POST",
        headers: { "Content-Type": "application/json" },
        body: JSON.stringify({ enabled: true })
      });
    }
    if (APP.preview.key !== key) {
      APP.preview = await fetchJson("/api/preview_select", {
        method: "POST",
        headers: { "Content-Type": "application/json" },
        body: JSON.stringify({ key: key })
      });
    }
  }

  async function restoreLiveStatusFromEffects() {
    if (!(APP.preview && APP.preview.enabled)) return;
    APP.preview = await fetchJson("/api/preview_mode", {
      method: "POST",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify({ enabled: false })
    });
    APP.effectsEditor.previewNonce += 1;
    if (APP.effectsEditor.previewTimer) {
      window.clearTimeout(APP.effectsEditor.previewTimer);
      APP.effectsEditor.previewTimer = null;
    }
    await loadCurrent();
  }

  function requestEffectsPreviewRestore() {
    if (APP.route !== "effects" || !(APP.preview && APP.preview.enabled)) return;
    const url = addKeyToUrl("/api/preview_mode");
    const body = JSON.stringify({ enabled: false });
    if (navigator.sendBeacon) {
      try {
        const blob = new Blob([body], { type: "application/json" });
        if (navigator.sendBeacon(url, blob)) return;
      } catch (err) {}
    }
    authFetch(url, {
      method: "POST",
      headers: { "Content-Type": "application/json" },
      body: body,
      keepalive: true
    }).catch(function () {});
  }

  async function previewEditorProfileNow(message) {
    const payload = effectProfilePayloadFromForm();
    await ensurePreviewSelection(payload.key);
    await fetchJson("/api/preview", {
      method: "POST",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify(payload)
    });
    await loadCurrent();
    renderEffectsCurrent();
    renderEffectEditorSummary(payload);
    if (message) setMessage("fx-status", message);
  }

  function scheduleEditorPreview() {
    if (!APP.effectsEditor.selectedKey) return;
    APP.effectsEditor.previewNonce += 1;
    const nonce = APP.effectsEditor.previewNonce;
    if (APP.effectsEditor.previewTimer) window.clearTimeout(APP.effectsEditor.previewTimer);
    APP.effectsEditor.previewTimer = window.setTimeout(function () {
      previewEditorProfileNow("Previewing " + APP.effectsEditor.selectedKey + " live...").catch(function (err) {
        if (nonce !== APP.effectsEditor.previewNonce) return;
        setMessage("fx-status", "Preview failed: " + err.message, true);
      });
    }, 140);
  }

  function renderEffectsCurrent() {
    if (!APP.current) return;
    const activityName = APP.current.activity || "Manual Preview";
    const color = toHex(APP.current.color >>> 0);
    safeText($("fx-current"), activityName);
    safeText(
      $("fx-current-detail"),
      modeName(APP.current.mode) + " \u2022 " + (APP.current.speed || 0) + "s" +
      (APP.current.reverse ? " \u2022 Reverse" : "") +
      " \u2022 " + color
    );
    safeText($("fx-chip-mode"), modeName(APP.current.mode));
    safeText($("fx-chip-speed"), (APP.current.speed || 0) + "s");
    safeText($("fx-chip-color"), color);
    safeText($("fx-chip-preview"), APP.preview && APP.preview.enabled ? (APP.preview.key || "Live") : "Off");
    if ($("fx-chip-swatch")) $("fx-chip-swatch").style.backgroundColor = color;
    renderLiveStrip(APP.current.color >>> 0);
  }

  function estimateLedLuma(color) {
    const value = Number(color) >>> 0;
    const r = (value >> 16) & 255;
    const g = (value >> 8) & 255;
    const b = value & 255;
    return ((0.2126 * r) + (0.7152 * g) + (0.0722 * b)) / 255;
  }

  function renderLiveStrip(color, frame) {
    const host = $("fx-live-strip");
    if (!host) return;
    const source = Array.isArray(frame) && frame.length ? frame.slice() : null;
    const ledCount = source ? source.length : Math.max(1, Math.min(16, parseInt(APP.settings && APP.settings.num_leds, 10) || 8));
    const hex = toHex(color >>> 0);
    if (host.children.length !== ledCount) {
      host.innerHTML = "";
      for (let i = 0; i < ledCount; i++) {
        const led = document.createElement("span");
        led.className = "live-led";
        host.appendChild(led);
      }
    }
    for (let i = 0; i < ledCount; i++) {
      const led = host.children[i];
      const ledColor = source ? (source[i] >>> 0) : (color >>> 0);
      const ledHex = source ? toHex(ledColor) : hex;
      const luma = source ? estimateLedLuma(ledColor) : (0.45 + ((i + 1) / ledCount) * 0.45);
      const intensity = Math.max(0.12, Math.min(1, luma));
      const scaleY = 0.72 + (intensity * 0.42);
      const lift = (1 - intensity) * 7;
      const glow = 8 + Math.round(intensity * 18);
      led.style.background = "linear-gradient(180deg, rgba(255,255,255,.34), rgba(255,255,255,0) 38%), " + ledHex;
      led.style.opacity = String(0.28 + (intensity * 0.9));
      led.style.transform = "translateY(" + lift.toFixed(1) + "px) scaleY(" + scaleY.toFixed(3) + ")";
      led.style.boxShadow = "0 0 " + glow + "px " + ledHex + "66, inset 0 1px 0 rgba(255,255,255,.30)";
      led.style.filter = "saturate(" + (1 + intensity * 0.55).toFixed(2) + ")";
    }
  }

  async function refreshLedStripPreview() {
    if (APP.route !== "effects") return;
    try {
      const frame = await loadLedFrame();
      const colors = frame && Array.isArray(frame.frame) ? frame.frame : [];
      if (colors.length) {
        const firstNonZero = colors.find(function (value) { return Number(value) !== 0; });
        renderLiveStrip((firstNonZero != null ? firstNonZero : (APP.current ? APP.current.color : 0)) >>> 0, colors);
        safeText($("fx-live-caption"), colors.length + " LEDs mirrored from device");
        return;
      }
    } catch (err) {
      console.error(err);
    }
    renderLiveStrip(APP.current ? (APP.current.color >>> 0) : 0);
    safeText($("fx-live-caption"), (APP.settings && APP.settings.num_leds ? APP.settings.num_leds : 0) + " LEDs active");
  }

  async function refreshEffects() {
    if (!APP.modes.length) await loadModes();
    const result = await Promise.all([
      fetchJson("/api/effects", { cache: "no-store" }),
      loadCurrent(),
      fetchJson("/api/preview_state", { cache: "no-store" })
    ]);
    APP.effects = result[0];
    APP.preview = result[2] || { enabled: false, key: "" };

    $("fx-fade").value = APP.effects.fade_ms || 0;
    const brightness = APP.effects.brightness != null ? APP.effects.brightness : 128;
    const brightnessPercent = Math.round((brightness * 100) / 255);
    $("fx-brightness").value = brightnessPercent;
    $("fx-brightness-range").value = brightnessPercent;
    $("fx-gamma").value = Number(APP.effects.gamma != null ? APP.effects.gamma : 2.2).toFixed(1);
    $("fx-num-leds").value = APP.effects.num_leds || 0;
    populateEffectProfileSelect();
    APP.effectsEditor.selectedKey = ensureEffectSelection();
    if (APP.effectsEditor.selectedKey) {
      fillEffectEditor(getEffectProfileByKey(APP.effectsEditor.selectedKey));
    }
    renderEffectsCurrent();
    await refreshLedStripPreview();
  }

  async function initEffects() {
    if (!APP.initialized.effects) {
      APP.initialized.effects = true;
      renderEffectColorPresets();

      $("fx-brightness").addEventListener("input", function () {
        $("fx-brightness-range").value = $("fx-brightness").value;
      });
      $("fx-brightness-range").addEventListener("input", function () {
        $("fx-brightness").value = $("fx-brightness-range").value;
      });

      $("fx-profile-select").addEventListener("change", function () {
        APP.effectsEditor.selectedKey = $("fx-profile-select").value;
        const profile = getEffectProfileByKey(APP.effectsEditor.selectedKey);
        if (!profile) return;
        fillEffectEditor(profile);
        setMessage("fx-status", "Entering edit mode for " + profile.key + "...");
        previewEditorProfileNow("Editing " + profile.key + " with live preview.").catch(function (err) {
          setMessage("fx-status", "Preview setup failed: " + err.message, true);
        });
      });

      ["fx-profile-mode", "fx-profile-speed", "fx-profile-speed-range", "fx-profile-fade", "fx-profile-brightness"].forEach(function (id) {
        $(id).addEventListener("input", function () {
          if (id === "fx-profile-speed") $("fx-profile-speed-range").value = $("fx-profile-speed").value;
          if (id === "fx-profile-speed-range") $("fx-profile-speed").value = $("fx-profile-speed-range").value;
          renderEffectEditorSummary(effectProfilePayloadFromForm());
          scheduleEditorPreview();
        });
        if (id === "fx-profile-mode") {
          $(id).addEventListener("change", function () {
            renderEffectEditorSummary(effectProfilePayloadFromForm());
            scheduleEditorPreview();
          });
        }
      });

      $("fx-profile-color-trigger").addEventListener("click", function () {
        $("fx-profile-color").click();
      });
      $("fx-profile-color").addEventListener("input", function () {
        setEffectColorChooser($("fx-profile-color").value);
        renderEffectEditorSummary(effectProfilePayloadFromForm());
        scheduleEditorPreview();
      });
      $("fx-profile-color").addEventListener("change", function () {
        setEffectColorChooser($("fx-profile-color").value);
        renderEffectEditorSummary(effectProfilePayloadFromForm());
        scheduleEditorPreview();
      });
      $("fx-profile-color-hex").addEventListener("input", function () {
        const normalized = normalizeHexInput($("fx-profile-color-hex").value);
        $("fx-profile-color-hex").value = normalized;
        if (!isValidHexColor(normalized)) return;
        setEffectColorChooser(normalized);
        renderEffectEditorSummary(effectProfilePayloadFromForm());
        scheduleEditorPreview();
      });
      $("fx-profile-color-hex").addEventListener("blur", function () {
        const normalized = normalizeHexInput($("fx-profile-color-hex").value);
        setEffectColorChooser(isValidHexColor(normalized) ? normalized : $("fx-profile-color").value);
      });

      $("fx-save-profile-btn").addEventListener("click", async function () {
        if (!APP.effectsEditor.selectedKey) return;
        setMessage("fx-status", "Saving " + APP.effectsEditor.selectedKey + "...");
        try {
          const edited = effectProfilePayloadFromForm();
          const nextProfiles = effectProfiles().map(function (profile) {
            return profile.key === edited.key ? edited : normalizeEffectProfile(profile);
          });
          const payload = buildEffectsPayload(nextProfiles);
          await fetchJson("/api/effects", {
            method: "POST",
            headers: { "Content-Type": "application/json" },
            body: JSON.stringify(payload)
          });
          await refreshEffects();
          setMessage("fx-status", "Saved " + edited.key + ".");
        } catch (err) {
          setMessage("fx-status", "Saving status failed: " + err.message, true);
        }
      });

      $("fx-revert-profile-btn").addEventListener("click", function () {
        const profile = getEffectProfileByKey(APP.effectsEditor.selectedKey);
        if (!profile) return;
        fillEffectEditor(profile);
        previewEditorProfileNow("Reverted " + profile.key + " to the saved version.").catch(function (err) {
          setMessage("fx-status", "Revert preview failed: " + err.message, true);
        });
      });

      $("fx-stop-preview-btn").addEventListener("click", async function () {
        setMessage("fx-status", "Stopping preview mode...");
        try {
          APP.preview = await fetchJson("/api/preview_mode", {
            method: "POST",
            headers: { "Content-Type": "application/json" },
          body: JSON.stringify({ enabled: false })
          });
          await loadCurrent();
          renderEffectsCurrent();
          await refreshLedStripPreview();
          setMessage("fx-status", "Preview mode stopped.");
        } catch (err) {
          setMessage("fx-status", "Stopping preview failed: " + err.message, true);
        }
      });

      $("fx-save-defaults-btn").addEventListener("click", async function () {
        setMessage("fx-status", "Saving global defaults...");
        try {
          await fetchJson("/api/effects", {
            method: "POST",
            headers: { "Content-Type": "application/json" },
            body: JSON.stringify(buildEffectsPayload(effectProfiles()))
          });
          await refreshEffects();
          setMessage("fx-status", "Global defaults saved.");
        } catch (err) {
          setMessage("fx-status", "Saving defaults failed: " + err.message, true);
        }
      });

      $("fx-apply-leds-btn").addEventListener("click", async function () {
        setMessage("fx-status", "Applying LED count...");
        try {
          await fetchJson("/api/leds", {
            method: "POST",
            headers: { "Content-Type": "application/json" },
            body: JSON.stringify({ num_leds: parseInt($("fx-num-leds").value, 10) || 0 })
          });
          await refreshEffects();
          setMessage("fx-status", "LED count applied.");
        } catch (err) {
          setMessage("fx-status", "Applying LED count failed: " + err.message, true);
        }
      });

      $("fx-reload-btn").addEventListener("click", function () {
        refreshEffects().then(function () {
          setMessage("fx-status", "Effects reloaded.");
        }).catch(function (err) {
          setMessage("fx-status", "Reload failed: " + err.message, true);
        });
      });
    }
    await refreshEffects();
    if (!APP.initialized.effectsLoop) {
      APP.initialized.effectsLoop = true;
      setInterval(function () {
        if (APP.route !== "effects") return;
        Promise.all([loadCurrent(), loadLedFrame()]).then(function (result) {
          APP.current = result[0];
          renderEffectsCurrent();
          const frame = result[1];
          const colors = frame && Array.isArray(frame.frame) ? frame.frame : [];
          if (colors.length) {
            const firstNonZero = colors.find(function (value) { return Number(value) !== 0; });
            renderLiveStrip((firstNonZero != null ? firstNonZero : APP.current.color) >>> 0, colors);
            safeText($("fx-live-caption"), colors.length + " LEDs mirrored from device");
          }
        }).catch(console.error);
      }, 180);
    }
  }

  async function renderLogs() {
    const count = parseInt($("logs-count").value, 10) || 50;
    try {
      const data = await fetchJson("/api/logs?n=" + count, { cache: "no-store" });
      const lines = Array.isArray(data.lines) ? data.lines : [];
      safeText($("logs-box"), lines.length ? lines.join("\n") : "(no logs)");
      $("logs-box").scrollTop = $("logs-box").scrollHeight;
      safeText($("logs-meta"), "[" + lines.length + " shown, " + data.count + " stored, cap " + data.capacity + "] " + new Date().toLocaleTimeString());
    } catch (err) {
      safeText($("logs-box"), "Error loading logs: " + err.message);
    }
  }

  async function initLogs() {
    if (!APP.initialized.logs) {
      APP.initialized.logs = true;
      $("logs-reload-btn").addEventListener("click", function () {
        renderLogs().catch(console.error);
      });
    }
    await renderLogs();
    if (!APP.initialized.logsLoop) {
      APP.initialized.logsLoop = true;
      setInterval(function () {
        if ($("logs-auto").checked) renderLogs().catch(console.error);
      }, APP.ui.MIN_REFRESH_MS);
    }
  }

  function fillFirmwareInfo(settings) {
    safeText($("fw-version"), settings.sketch_version || "");
    safeText($("fw-sketch"), (settings.sketch_size || 0) + " used, " + (settings.free_sketch_space || 0) + " free for OTA");
    safeText($("fw-flash"), (settings.flash_chip_size || "") + " @ " + (settings.flash_chip_speed || "") + " Hz");
    safeText($("fw-sdk"), settings.sdk_version || "");
  }

  async function initFirmware() {
    fillFirmwareInfo(await loadSettings());
    if (APP.initialized.fw) return;
    APP.initialized.fw = true;

    async function handleOtaSubmit(event, formId, route, statusId, successMessage) {
      event.preventDefault();
      const form = $(formId);
      const status = $(statusId);
      safeText(status, "Uploading...");
      const body = new FormData(form);
      try {
        const response = await authFetch(route, { method: "POST", body: body });
        const text = await response.text();
        if ((text || "").indexOf("OK") >= 0) {
          safeText(status, successMessage);
          setTimeout(function () { window.location.reload(); }, 5000);
        } else {
          safeText(status, "Update failed: " + text);
        }
      } catch (err) {
        safeText(status, "Upload error: " + err.message);
      }
    }

    const form = $("fw-form");
    ensureHiddenKey(form);
    form.addEventListener("submit", function (event) {
      handleOtaSubmit(event, "fw-form", "/update", "fw-status", "Firmware upload complete. Rebooting...");
    });

    $("fw-view-ota-btn").addEventListener("click", async function () {
      $("fw-ota-log").textContent = "(loading...)";
      $("fw-ota-dialog").showModal();
      try {
        const response = await authFetch("/api/ota_last", { cache: "no-store" });
        const text = await response.text();
        let data = null;
        try {
          data = text ? JSON.parse(text) : null;
        } catch (err) {}
        if (response.status === 404) {
          safeText($("fw-ota-log"), data && data.message ? data.message : "No OTA log saved yet.");
          return;
        }
        const trimmed = (text || "").trim();
        safeText($("fw-ota-log"), trimmed ? text : "No OTA log saved yet.");
      } catch (err) {
        safeText($("fw-ota-log"), "Error loading log: " + err.message);
      }
    });

    $("fw-ota-close-btn").addEventListener("click", function () {
      $("fw-ota-dialog").close();
    });
  }

  function initTopbarLoop() {
    if (APP.initialized.topbar) return;
    APP.initialized.topbar = true;
    setInterval(function () {
      loadSettings().catch(console.error);
    }, APP.ui.MIN_REFRESH_MS);
  }

  async function boot() {
    APP.route = routeFromPath(window.location.pathname);
    initTheme();
    syncProtectedLinks();
    initNavigation();
    await loadSettings();
    await openRoute(APP.route, false);
  }

  window.addEventListener("load", function () {
    boot().catch(function (err) {
      console.error(err);
      alert("Failed to load the StatusGlow UI: " + err.message);
    });
  });
})();
