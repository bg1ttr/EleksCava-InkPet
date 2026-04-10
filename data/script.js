/* =====================================================
   InksPet Web Portal - Shared Utilities
   Loaded on all pages. Keep it lean for ESP32 RAM.
   ===================================================== */

/* --- Language System --- */
const LANG = {
  en: {
    connected:    'LIVE',
    connecting:   'CONNECTING',
    disconnected: 'OFFLINE',
    state_idle:       'idle',
    state_sleeping:   'sleeping',
    state_thinking:   'thinking',
    state_typing:     'typing',
    state_building:   'building',
    state_juggling:   'juggling',
    state_conducting: 'conducting',
    state_error:      'error',
    state_attention:  'attention',
    state_permission: 'permission',
    state_sweeping:   'sweeping',
    state_carrying:   'carrying',
    no_agent:    '-- no agent active --',
    sessions:    'sessions',
  },
  zh: {
    connected:    '在线',
    connecting:   '连接中',
    disconnected: '离线',
    state_idle:       '待机',
    state_sleeping:   '休眠',
    state_thinking:   '思考中',
    state_typing:     '执行中',
    state_building:   '构建中',
    state_juggling:   '多任务',
    state_conducting: '并行中',
    state_error:      '错误',
    state_attention:  '已完成',
    state_permission: '需要授权',
    state_sweeping:   '压缩中',
    state_carrying:   '迁移中',
    no_agent:    '-- 无活跃 Agent --',
    sessions:    '会话',
  }
};

let currentLang = (localStorage.getItem('inkspet_lang') || 'en');

function t(key) {
  return (LANG[currentLang] && LANG[currentLang][key]) || (LANG.en[key]) || key;
}

function setLang(lang) {
  currentLang = lang;
  localStorage.setItem('inkspet_lang', lang);
  document.querySelectorAll('[data-lang-key]').forEach(el => {
    const key = el.dataset.langKey;
    el.textContent = t(key);
  });
  document.querySelectorAll('.lang-btn').forEach(btn => {
    btn.classList.toggle('active', btn.dataset.lang === lang);
  });
  // notify page if it wants to re-render
  document.dispatchEvent(new CustomEvent('langchange', { detail: lang }));
}

// Init lang buttons
document.addEventListener('DOMContentLoaded', () => {
  document.querySelectorAll('.lang-btn').forEach(btn => {
    btn.addEventListener('click', () => setLang(btn.dataset.lang));
    btn.classList.toggle('active', btn.dataset.lang === currentLang);
  });
  // Apply initial translations
  setLang(currentLang);
});

/* --- WebSocket Manager --- */
const WS = (function() {
  let ws = null;
  let reconnectTimer = null;
  let reconnectDelay = 1000;
  const MAX_DELAY = 30000;
  let handlers = {};
  let statusEl = null;
  let dotEl = null;

  function getWsUrl() {
    const proto = location.protocol === 'https:' ? 'wss:' : 'ws:';
    return `${proto}//${location.host}/ws`;
  }

  function setStatus(state) {
    if (statusEl) statusEl.textContent = t(state);
    if (dotEl) {
      dotEl.className = 'conn-dot';
      if (state === 'connected')    dotEl.classList.add('connected');
      if (state === 'disconnected') dotEl.classList.add('disconnected');
      if (state === 'connecting')   dotEl.classList.add('connecting');
    }
  }

  function connect() {
    if (ws && ws.readyState <= WebSocket.OPEN) return;

    setStatus('connecting');
    try {
      ws = new WebSocket(getWsUrl());
    } catch(e) {
      scheduleReconnect();
      return;
    }

    ws.onopen = function() {
      reconnectDelay = 1000;
      setStatus('connected');
      // Start ping to keep alive
      startPing();
    };

    ws.onmessage = function(evt) {
      let msg;
      try { msg = JSON.parse(evt.data); } catch(e) { return; }
      const type = msg.type;
      if (handlers[type]) handlers[type](msg);
      // Also call wildcard handler
      if (handlers['*']) handlers['*'](msg);
    };

    ws.onerror = function() {
      // Will be followed by onclose
    };

    ws.onclose = function() {
      setStatus('disconnected');
      stopPing();
      scheduleReconnect();
    };
  }

  let pingTimer = null;
  function startPing() {
    stopPing();
    pingTimer = setInterval(() => {
      if (ws && ws.readyState === WebSocket.OPEN) {
        ws.send(JSON.stringify({ type: 'ping' }));
      }
    }, 20000);
  }
  function stopPing() {
    if (pingTimer) { clearInterval(pingTimer); pingTimer = null; }
  }

  function scheduleReconnect() {
    if (reconnectTimer) return;
    reconnectTimer = setTimeout(() => {
      reconnectTimer = null;
      connect();
    }, reconnectDelay);
    // Exponential backoff with jitter
    reconnectDelay = Math.min(reconnectDelay * 1.5 + Math.random() * 500, MAX_DELAY);
  }

  function send(data) {
    if (ws && ws.readyState === WebSocket.OPEN) {
      ws.send(JSON.stringify(data));
    }
  }

  return {
    // Set status UI elements
    init(dotElem, statusElem) {
      dotEl = dotElem;
      statusEl = statusElem;
      connect();
    },
    // Register message handler: WS.on('stateUpdate', fn)
    on(type, fn) { handlers[type] = fn; },
    send,
    connect,
  };
})();

/* --- HTTP Helpers --- */

/**
 * Fetch JSON from API endpoint with error handling.
 * @param {string} url
 * @param {object} options - fetch options
 * @returns {Promise<{ok:boolean, data:any, error:string}>}
 */
async function fetchJSON(url, options) {
  try {
    const res = await fetch(url, options);
    const text = await res.text();
    let data;
    try { data = JSON.parse(text); } catch(e) { data = { raw: text }; }
    return { ok: res.ok, status: res.status, data };
  } catch(e) {
    return { ok: false, status: 0, error: e.message, data: null };
  }
}

/**
 * POST JSON helper.
 */
function postJSON(url, body) {
  return fetchJSON(url, {
    method: 'POST',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify(body),
  });
}

/* --- Toast Notifications --- */

let toastContainer = null;

function ensureToastContainer() {
  if (!toastContainer) {
    toastContainer = document.createElement('div');
    toastContainer.id = 'toast-container';
    document.body.appendChild(toastContainer);
  }
}

/**
 * Show a toast notification.
 * @param {string} msg
 * @param {'success'|'error'|'warn'|'info'} type
 * @param {number} duration ms
 */
function showMessage(msg, type = 'info', duration = 3000) {
  ensureToastContainer();
  const el = document.createElement('div');
  el.className = `toast ${type}`;
  el.textContent = msg;
  toastContainer.appendChild(el);
  setTimeout(() => {
    el.style.opacity = '0';
    el.style.transition = 'opacity 0.3s ease';
    setTimeout(() => el.remove(), 300);
  }, duration);
}

/* --- RSSI to Signal Level --- */

/**
 * Convert RSSI dBm to 1-4 signal bars.
 * @param {number} rssi
 * @returns {number} 1-4
 */
function rssiToBars(rssi) {
  if (rssi >= -55) return 4;
  if (rssi >= -65) return 3;
  if (rssi >= -75) return 2;
  return 1;
}

/**
 * Build signal bars HTML element.
 * @param {number} rssi
 * @returns {string} HTML string
 */
function signalBarsHTML(rssi) {
  const bars = rssiToBars(rssi);
  return `<span class="signal-bars s${bars}" title="${rssi} dBm">
    <span class="signal-bar"></span>
    <span class="signal-bar"></span>
    <span class="signal-bar"></span>
    <span class="signal-bar"></span>
  </span>`;
}

/* --- Battery Display --- */

/**
 * Build battery icon HTML.
 * @param {number} pct 0-100
 * @returns {string} HTML string
 */
function batteryHTML(pct, charging) {
  const clampedPct = Math.max(0, Math.min(100, pct));
  let cls = '';
  if (clampedPct <= 20) cls = 'low';
  else if (clampedPct <= 50) cls = 'medium';

  const chargingSymbol = charging ? ' ⚡' : '';
  return `<span class="battery">
    <span class="battery-icon ${cls}">
      <span class="battery-fill" style="width:${clampedPct}%"></span>
    </span>
    <span class="mono">${clampedPct}%${chargingSymbol}</span>
  </span>`;
}

/* --- Uptime Formatter --- */

/**
 * Format seconds into human-readable uptime string.
 * @param {number} seconds
 * @returns {string}
 */
function formatUptime(seconds) {
  if (!seconds && seconds !== 0) return '--';
  const s = Math.floor(seconds);
  const d = Math.floor(s / 86400);
  const h = Math.floor((s % 86400) / 3600);
  const m = Math.floor((s % 3600) / 60);
  const sec = s % 60;
  if (d > 0) return `${d}d ${h}h ${m}m`;
  if (h > 0) return `${h}h ${m}m ${sec}s`;
  if (m > 0) return `${m}m ${sec}s`;
  return `${sec}s`;
}

/* --- Heap Formatter --- */

/**
 * Format bytes to KB string.
 */
function formatHeap(bytes) {
  if (!bytes) return '--';
  return `${Math.round(bytes / 1024)} KB`;
}

/* --- State Display Helpers --- */

function stateColor(state) {
  const map = {
    idle: 'var(--text-dim)',
    sleeping: 'var(--text-dim)',
    thinking: 'var(--info)',
    typing: 'var(--accent)',
    building: 'var(--accent)',
    juggling: 'var(--purple)',
    conducting: 'var(--purple)',
    error: 'var(--danger)',
    attention: 'var(--warn)',
    permission: 'var(--warn)',
    sweeping: 'var(--info)',
    carrying: '#f8954a',
  };
  return map[state] || 'var(--text-sec)';
}

/* --- Export via window (no module system on ESP32 plain HTML) --- */
window.LANG = LANG;
window.t = t;
window.setLang = setLang;
window.WS = WS;
window.fetchJSON = fetchJSON;
window.postJSON = postJSON;
window.showMessage = showMessage;
window.rssiToBars = rssiToBars;
window.signalBarsHTML = signalBarsHTML;
window.batteryHTML = batteryHTML;
window.formatUptime = formatUptime;
window.formatHeap = formatHeap;
window.stateColor = stateColor;
