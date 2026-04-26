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
      logs: false,
      logsLoop: false,
      fw: false
    },
    refreshers: {
      home: null,
      config: null
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

  function formatUptime(ms) {
    const total = Math.floor((ms || 0) / 1000);
    const hours = Math.floor(total / 3600);
    const minutes = Math.floor((total % 3600) / 60);
    const seconds = total % 60;
    return hours + ":" + String(minutes).padStart(2, "0") + ":" + String(seconds).padStart(2, "0");
  }

  function routeLabel(route) {
    if (route === "config") return "Configuration";
    if (route === "effects") return "Effects";
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
    safeText($("page-subtitle"), " - " + routeLabel(route));
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
  }

  async function openRoute(route, pushHistory) {
    const nextRoute = route || "home";
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
      const currentText = (current.activity ? current.activity + " - " : "") +
        modeName(current.mode) +
        ", Speed " + (current.speed || 0) +
        ", Reverse " + (current.reverse ? "on" : "off") +
        ", Color " + toHex(current.color);
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

  function effectsRow(profile) {
    const row = document.createElement("tr");
    row.dataset.key = profile.key;

    const cells = [
      { label: "Preview" },
      { label: "Key" },
      { label: "Mode" },
      { label: "Duration (s)" },
      { label: "Color" },
      { label: "Fade (ms)" },
      { label: "Brightness (%)" },
      { label: "Actions" }
    ].map(function (meta) {
      const cell = document.createElement("td");
      cell.dataset.label = meta.label;
      row.appendChild(cell);
      return cell;
    });

    const previewBox = document.createElement("input");
    previewBox.type = "checkbox";
    previewBox.className = "prev-cb";
    previewBox.disabled = !APP.preview.enabled;
    previewBox.addEventListener("click", function () {
      selectPreview(row, previewBox).catch(console.error);
    });
    cells[0].appendChild(previewBox);

    safeText(cells[1], profile.key);
    cells[2].appendChild(buildModeSelect(profile.mode));

    const speedWrap = document.createElement("div");
    speedWrap.style.display = "flex";
    speedWrap.style.alignItems = "center";
    speedWrap.style.gap = ".35rem";
    const speedRange = document.createElement("input");
    speedRange.type = "range";
    speedRange.min = "0";
    speedRange.max = "60";
    speedRange.step = "0.1";
    speedRange.value = profile.speed || 0;
    speedRange.dataset.k = "speed";
    const speedInput = document.createElement("input");
    speedInput.type = "number";
    speedInput.min = "0";
    speedInput.max = "60";
    speedInput.step = "0.1";
    speedInput.value = profile.speed || 0;
    speedRange.addEventListener("input", function () {
      speedInput.value = speedRange.value;
    });
    speedInput.addEventListener("input", function () {
      speedRange.value = speedInput.value;
    });
    speedWrap.appendChild(speedRange);
    speedWrap.appendChild(speedInput);
    cells[3].appendChild(speedWrap);

    const colorInput = document.createElement("input");
    colorInput.type = "color";
    colorInput.value = toHex(profile.color >>> 0);
    colorInput.dataset.k = "color";
    cells[4].appendChild(colorInput);

    const fadeInput = document.createElement("input");
    fadeInput.type = "number";
    fadeInput.min = "0";
    fadeInput.step = "50";
    fadeInput.placeholder = "0=global";
    fadeInput.value = profile.fade_ms || 0;
    fadeInput.dataset.k = "fade_ms";
    cells[5].appendChild(fadeInput);

    const brightnessInput = document.createElement("input");
    brightnessInput.type = "number";
    brightnessInput.min = "0";
    brightnessInput.max = "100";
    brightnessInput.step = "1";
    brightnessInput.placeholder = "0=global (%)";
    brightnessInput.value = profile.bri ? Math.round((profile.bri * 100) / 255) : 0;
    brightnessInput.dataset.k = "bri";
    cells[6].appendChild(brightnessInput);

    const previewButton = document.createElement("button");
    previewButton.type = "button";
    previewButton.className = "btn";
    previewButton.textContent = "Preview";
    previewButton.addEventListener("click", function () {
      setMessage("fx-status", "Sending preview...");
      previewEffectRow(row).then(function () {
        setMessage("fx-status", "Preview applied for " + row.dataset.key + ".");
      }).catch(function (err) {
        setMessage("fx-status", "Preview failed: " + err.message, true);
      });
    });
    cells[7].appendChild(previewButton);

    if (APP.preview.enabled && APP.preview.key === profile.key) previewBox.checked = true;
    return row;
  }

  function effectPayloadFromRow(row) {
    const payload = { key: row.dataset.key, reverse: false };
    row.querySelectorAll("input,select").forEach(function (input) {
      const key = input.dataset.k;
      if (!key) return;
      if (key === "color") payload[key] = fromHex(input.value);
      else if (key === "bri") {
        const percent = Math.min(100, Math.max(0, parseFloat(input.value) || 0));
        payload[key] = percent <= 0 ? 0 : Math.round((percent * 255) / 100);
      } else {
        payload[key] = parseFloat(input.value) || 0;
      }
    });
    return payload;
  }

  async function previewEffectRow(row) {
    await fetchJson("/api/preview", {
      method: "POST",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify(effectPayloadFromRow(row))
    });
  }

  async function selectPreview(row, checkbox) {
    if (!APP.preview.enabled) {
      checkbox.checked = false;
      return;
    }
    document.querySelectorAll(".prev-cb").forEach(function (other) {
      if (other !== checkbox) other.checked = false;
    });
    try {
      APP.preview = await fetchJson("/api/preview_select", {
        method: "POST",
        headers: { "Content-Type": "application/json" },
        body: JSON.stringify({ key: row.dataset.key })
      });
      setMessage("fx-status", "Selected preview profile: " + row.dataset.key + ".");
    } catch (err) {
      checkbox.checked = false;
      setMessage("fx-status", "Selecting preview failed: " + err.message, true);
      throw err;
    }
  }

  function renderEffectsCurrent() {
    if (!APP.current) return;
    safeText(
      $("fx-current"),
      "Current: " + (APP.current.activity || "") + " - " + modeName(APP.current.mode) +
      ", Duration " + (APP.current.speed || 0) + "s, Color " + toHex(APP.current.color >>> 0)
    );
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
    $("fx-preview-mode").checked = !!APP.preview.enabled;

    const body = document.querySelector("#fx-table tbody");
    body.innerHTML = "";
    (APP.effects.profiles || []).forEach(function (profile) {
      body.appendChild(effectsRow(profile));
    });
    renderEffectsCurrent();
  }

  async function initEffects() {
    if (!APP.initialized.effects) {
      APP.initialized.effects = true;
      $("fx-brightness").addEventListener("input", function () {
        $("fx-brightness-range").value = $("fx-brightness").value;
      });
      $("fx-brightness-range").addEventListener("input", function () {
        $("fx-brightness").value = $("fx-brightness-range").value;
      });

      $("fx-save-btn").addEventListener("click", async function () {
        setMessage("fx-status", "Saving effects...");
        try {
          const uiBrightness = Math.min(100, Math.max(0, parseInt($("fx-brightness").value, 10) || 0));
          const payload = {
            fade_ms: parseInt($("fx-fade").value, 10) || 0,
            brightness: Math.round((uiBrightness * 255) / 100),
            gamma: parseFloat($("fx-gamma").value) || 2.2,
            profiles: []
          };
          document.querySelectorAll("#fx-table tbody tr").forEach(function (row) {
            payload.profiles.push(effectPayloadFromRow(row));
          });
          await fetchJson("/api/effects", {
            method: "POST",
            headers: { "Content-Type": "application/json" },
            body: JSON.stringify(payload)
          });
          await refreshEffects();
          setMessage("fx-status", "Effects saved.");
        } catch (err) {
          setMessage("fx-status", "Saving effects failed: " + err.message, true);
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

      $("fx-preview-mode").addEventListener("change", async function () {
        setMessage("fx-status", "Updating preview mode...");
        try {
          APP.preview = await fetchJson("/api/preview_mode", {
            method: "POST",
            headers: { "Content-Type": "application/json" },
            body: JSON.stringify({ enabled: $("fx-preview-mode").checked })
          });
          document.querySelectorAll(".prev-cb").forEach(function (checkbox) {
            checkbox.disabled = !APP.preview.enabled;
            if (!APP.preview.enabled) checkbox.checked = false;
          });
          setMessage("fx-status", APP.preview.enabled ? "Preview mode enabled." : "Preview mode disabled.");
        } catch (err) {
          $("fx-preview-mode").checked = !$("fx-preview-mode").checked;
          setMessage("fx-status", "Updating preview mode failed: " + err.message, true);
        }
      });
    }
    await refreshEffects();
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

    const fsForm = $("fw-fs-form");
    ensureHiddenKey(fsForm);
    fsForm.addEventListener("submit", function (event) {
      handleOtaSubmit(event, "fw-fs-form", "/updatefs", "fw-fs-status", "Filesystem upload complete. Rebooting...");
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
