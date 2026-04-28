const http = require("node:http");
const fs = require("node:fs/promises");
const path = require("node:path");

const CONFIG_PATH = process.env.STATUSGLOW_CONFIG || path.join(__dirname, "config.json");
const TOKEN_SKEW_MS = 60 * 1000;
const GRAPH_SCOPE = "https://graph.microsoft.com/.default";

let config = null;
let tokenCache = { token: null, expiresAt: 0 };
let lastPollAt = null;
let lastPollError = null;
let activePollPromise = null;
const presenceCache = new Map();

function json(res, statusCode, payload) {
  const body = JSON.stringify(payload, null, 2);
  res.writeHead(statusCode, {
    "Content-Type": "application/json; charset=utf-8",
    "Cache-Control": "no-store"
  });
  res.end(body);
}

function normalizeDevice(device) {
  return {
    deviceId: String(device.deviceId || "").trim(),
    apiKey: String(device.apiKey || ""),
    userId: String(device.userId || "").trim(),
    displayName: String(device.displayName || "").trim(),
    fallbackAvailability: String(device.fallbackAvailability || "Offline").trim() || "Offline",
    fallbackActivity: String(device.fallbackActivity || "Offline").trim() || "Offline"
  };
}

async function loadConfig() {
  const raw = await fs.readFile(CONFIG_PATH, "utf8");
  const parsed = JSON.parse(raw);
  const next = {
    tenantId: String(parsed.tenantId || "").trim(),
    clientId: String(parsed.clientId || "").trim(),
    clientSecret: String(parsed.clientSecret || ""),
    pollIntervalSeconds: Math.max(5, parseInt(parsed.pollIntervalSeconds, 10) || 30),
    listenPort: Math.max(1, parseInt(parsed.listenPort, 10) || 8787),
    graphBaseUrl: String(parsed.graphBaseUrl || "https://graph.microsoft.com/v1.0").replace(/\/+$/, ""),
    devices: Array.isArray(parsed.devices) ? parsed.devices.map(normalizeDevice) : []
  };

  if (!next.tenantId || !next.clientId || !next.clientSecret) {
    throw new Error("Config must include tenantId, clientId, and clientSecret.");
  }
  if (!next.devices.length) {
    throw new Error("Config must include at least one device.");
  }

  for (const device of next.devices) {
    if (!device.deviceId || !device.apiKey || !device.userId) {
      throw new Error("Each device must include deviceId, apiKey, and userId.");
    }
  }

  config = next;
  return config;
}

function chunk(items, size) {
  const result = [];
  for (let index = 0; index < items.length; index += size) {
    result.push(items.slice(index, index + size));
  }
  return result;
}

async function getAccessToken() {
  if (tokenCache.token && Date.now() < tokenCache.expiresAt - TOKEN_SKEW_MS) {
    return tokenCache.token;
  }

  const body = new URLSearchParams({
    client_id: config.clientId,
    client_secret: config.clientSecret,
    scope: GRAPH_SCOPE,
    grant_type: "client_credentials"
  });

  const response = await fetch(`https://login.microsoftonline.com/${config.tenantId}/oauth2/v2.0/token`, {
    method: "POST",
    headers: { "Content-Type": "application/x-www-form-urlencoded" },
    body
  });

  const data = await response.json();
  if (!response.ok || !data.access_token) {
    throw new Error(`Token request failed: ${data.error || response.status}`);
  }

  tokenCache = {
    token: data.access_token,
    expiresAt: Date.now() + ((parseInt(data.expires_in, 10) || 3600) * 1000)
  };
  return tokenCache.token;
}

async function fetchPresenceBatch(ids) {
  const token = await getAccessToken();
  const response = await fetch(`${config.graphBaseUrl}/communications/getPresencesByUserId`, {
    method: "POST",
    headers: {
      "Authorization": `Bearer ${token}`,
      "Content-Type": "application/json"
    },
    body: JSON.stringify({ ids })
  });

  const data = await response.json();
  if (!response.ok) {
    const errorCode = data && data.error && data.error.code ? data.error.code : response.status;
    throw new Error(`Graph presence request failed: ${errorCode}`);
  }

  return Array.isArray(data.value) ? data.value : [];
}

async function pollPresence(reason = "timer") {
  if (activePollPromise) return activePollPromise;

  activePollPromise = (async () => {
    const ids = [...new Set(config.devices.map((device) => device.userId))];
    for (const batch of chunk(ids, 650)) {
      const presences = await fetchPresenceBatch(batch);
      const fetchedAt = new Date().toISOString();
      for (const presence of presences) {
        const userId = String(presence.id || "").trim();
        if (!userId) continue;
        presenceCache.set(userId, {
          id: userId,
          availability: String(presence.availability || "PresenceUnknown"),
          activity: String(presence.activity || "PresenceUnknown"),
          fetchedAt,
          source: reason
        });
      }
    }
    lastPollAt = new Date().toISOString();
    lastPollError = null;
  })();

  try {
    await activePollPromise;
  } catch (error) {
    lastPollError = error.message;
    throw error;
  } finally {
    activePollPromise = null;
  }
}

function findDevice(deviceId) {
  return config.devices.find((device) => device.deviceId === deviceId) || null;
}

function unauthorized(res, message) {
  json(res, 401, {
    ok: false,
    error: "unauthorized",
    message
  });
}

function getApiKey(req) {
  const header = req.headers["x-statusglow-device-key"];
  if (Array.isArray(header)) return header[0] || "";
  return header || "";
}

function routeRequest(req, res) {
  const url = new URL(req.url, "http://statusglow-relay.local");

  if (req.method === "GET" && url.pathname === "/health") {
    json(res, 200, {
      ok: true,
      devices: config.devices.length,
      cachedUsers: presenceCache.size,
      pollIntervalSeconds: config.pollIntervalSeconds,
      lastPollAt,
      lastPollError
    });
    return;
  }

  if (req.method === "GET" && url.pathname.startsWith("/api/v1/devices/") && url.pathname.endsWith("/presence")) {
    const deviceId = decodeURIComponent(url.pathname.slice("/api/v1/devices/".length, -"/presence".length));
    const device = findDevice(deviceId);
    if (!device) {
      json(res, 404, {
        ok: false,
        error: "unknown_device",
        message: "The requested device ID is not configured."
      });
      return;
    }
    if (getApiKey(req) !== device.apiKey) {
      unauthorized(res, "A valid device key is required.");
      return;
    }

    const cached = presenceCache.get(device.userId);
    const availability = cached && cached.availability ? cached.availability : device.fallbackAvailability;
    const activity = cached && cached.activity ? cached.activity : device.fallbackActivity;

    json(res, 200, {
      ok: true,
      source: "relay",
      deviceId: device.deviceId,
      userId: device.userId,
      displayName: device.displayName || undefined,
      availability,
      activity,
      fetched_at: cached ? cached.fetchedAt : null,
      stale: !cached
    });
    return;
  }

  json(res, 404, {
    ok: false,
    error: "not_found",
    message: "Route not found."
  });
}

async function start() {
  await loadConfig();
  try {
    await pollPresence("startup");
  } catch (error) {
    lastPollError = error.message;
    console.error(new Date().toISOString(), error);
  }
  setInterval(() => {
    pollPresence("timer").catch((error) => {
      lastPollError = error.message;
      console.error(new Date().toISOString(), error);
    });
  }, config.pollIntervalSeconds * 1000);

  const server = http.createServer((req, res) => {
    routeRequest(req, res);
  });

  server.listen(config.listenPort, () => {
    console.log(`StatusGlow relay listening on port ${config.listenPort}`);
    console.log(`Loaded ${config.devices.length} device mapping(s) from ${CONFIG_PATH}`);
  });
}

start().catch((error) => {
  console.error(error);
  process.exitCode = 1;
});
