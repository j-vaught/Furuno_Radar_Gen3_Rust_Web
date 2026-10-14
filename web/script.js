/* tiny helpers */
const byId = id => document.getElementById(id);
const v = id => {
  const el = byId(id);
  return el ? el.value : '';
};
const setHtml = (id, html) => {
  const el = byId(id);
  if (el) el.innerHTML = html;
};

const setCallout = (id, html, variant = 'info') => {
  const el = byId(id);
  if (!el) return;
  const hasMessage = Boolean(html);
  el.innerHTML = hasMessage ? html : '';
  if (hasMessage) {
    el.style.display = '';
    if (variant) {
      el.setAttribute('data-variant', variant);
    } else {
      el.removeAttribute('data-variant');
    }
  } else {
    el.style.display = 'none';
    el.removeAttribute('data-variant');
  }
};

function serverResponseVariant(text) {
  const message = String(text || '').toLowerCase();
  if (/\b(error|failed|failure|no ack|unacknowledged|mismatch|unknown|unavailable)\b/.test(message)) return 'error';
  if (/\b(readback verified|confirmed|acknowledged|set to|updated)\b/.test(message)) return 'success';
  return 'info';
}

function showServerResponse(id, text) {
  setCallout(id, text, serverResponseVariant(text));
}

const clamp01 = value => Math.max(0, Math.min(1, value));

let calibrationActive = false;
const patternQueue = [];
let lastLoadedPattern = null;
let apiContract = null;

// Readback is kept separate from the command cache. A value is allowed into
// this object only after a read endpoint returns a parseable radar response.
// This prevents startup defaults and unsent form values from being presented
// as measured device state.
const readbackState = {
  values: {},
  lastAt: null,
  validUntil: null,
  lastError: null,
  invalid: new Set()
};

const fallbackReadCommandMap = Object.freeze({
  TX: 'TX',
  RANGE: 'RANGE',
  PULSE: 'PULSE',
  GAIN: 'GAIN',
  STC: 'STC',
  CLUTTER: 'CLUTTER',
  ANTREV: 'ANTREV',
  SWEEP: 'SWEEP',
  ALL: 'ALL'
});

function getReadCommand(name) {
  if (apiContract && apiContract.commandMap && apiContract.commandMap[name]) {
    return apiContract.commandMap[name];
  }
  return fallbackReadCommandMap[name] || name;
}

function jsonPre(value) {
  return `<pre>${JSON.stringify(value, null, 2)}</pre>`;
}

async function loadApiContract() {
  try {
    const resp = await fetch('/api_contract');
    if (!resp.ok) throw new Error(`HTTP ${resp.status}`);
    const data = await resp.json();
    const commandMap = {};
    const names = new Set((data.commands || []).map(c => c.name));
    Object.keys(fallbackReadCommandMap).forEach(key => {
      const canonical = fallbackReadCommandMap[key];
      if (names.has(canonical)) commandMap[key] = canonical;
    });
    apiContract = { ...data, commandMap };
  } catch (err) {
    apiContract = { commandMap: { ...fallbackReadCommandMap } };
  }
}

function triggerFileInput() {
  const input = byId('patternFile');
  if (input) input.click();
}

// Setup file input auto-load and gain mode selector
document.addEventListener('DOMContentLoaded', () => {
  loadApiContract().catch(() => {});

  const fileInput = byId('patternFile');
  if (fileInput) {
    fileInput.addEventListener('change', () => {
      if (fileInput.files.length > 0) {
        loadPatternFromFile();
      }
    });
  }

  // Setup gain mode selector
  const gainModeSelect = byId('gainMode');
  if (gainModeSelect) {
    gainModeSelect.addEventListener('change', (e) => {
      const mode = e.target.value;
      const manualSection = document.querySelector('[data-gain-mode="manual"]');
      const autoSection = document.querySelector('[data-gain-mode="auto"]');

      if (mode === 'manual') {
        if (manualSection) manualSection.style.opacity = '1';
        if (autoSection) autoSection.style.opacity = '0.5';
      } else {
        if (manualSection) manualSection.style.opacity = '0.5';
        if (autoSection) autoSection.style.opacity = '1';
      }
    });
    // Trigger initial state
    gainModeSelect.dispatchEvent(new Event('change'));
  }

  // Setup clutter (SEA/RAIN) mode selectors
  const seaModeSelect = byId('seaMode');
  const rainModeSelect = byId('rainMode');

  if (seaModeSelect) {
    seaModeSelect.addEventListener('change', (e) => {
      const mode = e.target.value;
      const manualSection = document.querySelector('[data-clutter-mode="sea-manual"]');
      const autoSection = document.querySelector('[data-clutter-mode="sea-auto"]');

      if (mode === 'manual') {
        if (manualSection) manualSection.style.display = '';
        if (autoSection) autoSection.style.display = 'none';
      } else {
        if (manualSection) manualSection.style.display = 'none';
        if (autoSection) autoSection.style.display = '';
      }
    });
    // Trigger initial state
    seaModeSelect.dispatchEvent(new Event('change'));
  }

  if (rainModeSelect) {
    rainModeSelect.addEventListener('change', (e) => {
      const mode = e.target.value;
      const manualSection = document.querySelector('[data-clutter-mode="rain-manual"]');
      const autoSection = document.querySelector('[data-clutter-mode="rain-auto"]');

      if (mode === 'manual') {
        if (manualSection) manualSection.style.display = '';
        if (autoSection) autoSection.style.display = 'none';
      } else {
        if (manualSection) manualSection.style.display = 'none';
        if (autoSection) autoSection.style.display = '';
      }
    });
    // Trigger initial state
    rainModeSelect.dispatchEvent(new Event('change'));
  }
});

function normaliseTextareaSteps() {
  const textarea = byId('patternTextarea');
  if (!textarea) return '';
  const normalised = normalizePatternInput(textarea.value);
  textarea.value = normalised;
  return normalised;
}

function normalizePatternInput(raw) {
  if (!raw) return '';
  return raw
    .split(/[\n;]+/)
    .map(s => s.trim())
    .filter(Boolean)
    .join('; ');
}

function countPatternSteps(raw) {
  if (!raw) return 0;
  return raw
    .split(/;/)
    .map(s => s.trim())
    .filter(Boolean)
    .length;
}

function renderPatternQueue() {
  const list = byId('patternQueueList');
  if (list) {
    list.innerHTML = '';
    if (!patternQueue.length) {
      const li = document.createElement('li');
      li.textContent = 'Queue is empty.';
      li.className = 'queue-empty';
      list.appendChild(li);
    } else {
      patternQueue.forEach((item, idx) => {
        const li = document.createElement('li');
        li.className = 'queue-item';

        const info = document.createElement('div');
        info.className = 'queue-item__info';

        const label = document.createElement('span');
        label.className = 'queue-item__label';
        label.textContent = `${idx + 1}. ${item.label}`;

        const meta = document.createElement('span');
        meta.className = 'queue-item__meta';
        const repeatText = item.repeat === -1 ? 'infinite (loops forever)' : item.repeat;
        meta.textContent = `${item.stepsCount} steps · repeat ${repeatText}`;

        info.append(label, meta);

        const remove = document.createElement('button');
        remove.type = 'button';
        remove.className = 'queue-remove';
        remove.textContent = 'Remove';
        remove.setAttribute('aria-label', `Remove ${item.label} from queue`);
        remove.onclick = () => removeQueueItem(idx);

        li.append(info, remove);
        list.appendChild(li);
      });
    }
  }

  const queueLen = byId('statusQueueLength');
  if (queueLen) {
    queueLen.textContent = patternQueue.length;
  }
}

function removeQueueItem(index) {
  const removed = patternQueue[index];
  patternQueue.splice(index, 1);
  renderPatternQueue();
  const remaining = patternQueue.length;
  let tail;
  if (remaining) {
    const desc = remaining === 1 ? '1 item' : `${remaining} items`;
    tail = `Queue now has ${desc} remaining.`;
  } else if (lastLoadedPattern) {
    tail = `Queue is now empty. "${lastLoadedPattern.label}" remains loaded (${lastLoadedPattern.stepsCount} steps).`;
  } else {
    tail = 'Queue is now empty. Load a pattern file to begin.';
  }
  const removedLabel = removed ? `Removed "${removed.label}" from the queue. ` : 'Removed queued pattern. ';
  setCallout('patternStatus', `${removedLabel}${tail}`, 'info');
}


/* live image refresh + radar history compositing */
const radarImageEl = byId('radarImage');
const radarHistoryCanvas = byId('radarHistoryCanvas');
const radarHistoryCtx = radarHistoryCanvas ? radarHistoryCanvas.getContext('2d') : null;
const isRadarView = window.location.pathname.includes('radar.html');

const RADAR_HISTORY_STORAGE_KEY = 'radarHistoryConfig.v1';
const radarHistoryDefaults = {
  frameCount: 8,
  currentColor: '#ffe600',
  historyColor: '#ff00ff',
  fadePivot: 0.78,
  fadeSteepness: 12
};

let radarHistoryConfig = { ...radarHistoryDefaults };
const radarHistoryState = {
  frames: [],
  fetching: false,
  offscreen: radarHistoryCanvas ? document.createElement('canvas') : null,
  offscreenCtx: null,
  fallbackUrl: null
};
if (radarHistoryState.offscreen) {
  radarHistoryState.offscreenCtx = radarHistoryState.offscreen.getContext('2d');
}

hydrateRadarHistoryConfig();

if (isRadarView && radarHistoryCanvas && radarHistoryCtx) {
  setupRadarHistoryControls();
  startRadarHistoryLoop();
} else if (radarImageEl) {
  startLegacyRadarLoop();
}

function startRadarHistoryLoop() {
  const refreshInterval = 500;
  setFallbackVisibility(false);
  fetchAndRenderHistoryFrame();
  setInterval(() => {
    fetchAndRenderHistoryFrame();
  }, refreshInterval);
}

function startLegacyRadarLoop() {
  const refreshInterval = isRadarView ? 500 : 2000;
  setInterval(() => {
    const img = new Image();
    img.onload = () => {
      if (radarImageEl) {
        radarImageEl.src = img.src;
        setFallbackVisibility(true);
      }
    };
    img.src = '/newest_png?rand=' + Date.now();
  }, refreshInterval);
}

async function fetchAndRenderHistoryFrame() {
  if (!radarHistoryCtx || radarHistoryState.fetching) {
    return;
  }
  radarHistoryState.fetching = true;
  const cacheBuster = Date.now();
  try {
    const response = await fetch(`/newest_png?rand=${cacheBuster}`, { cache: 'no-store' });
    if (!response.ok) {
      throw new Error(`Radar image fetch failed: ${response.status}`);
    }
    const blob = await response.blob();
    const bitmap = await blobToBitmap(blob);
    const frameLayer = createFrameLayer(bitmap);
    if (typeof bitmap.close === 'function') {
      bitmap.close();
    }
    if (!frameLayer) {
      throw new Error('Unable to build radar frame layer.');
    }
    pushRadarFrame(frameLayer);
    setFallbackImageFromBlob(blob);
    drawRadarHistory();
  } catch (err) {
    console.warn(err);
    triggerFallbackImage();
  } finally {
    radarHistoryState.fetching = false;
  }
}

function pushRadarFrame(frameLayer) {
  radarHistoryState.frames.unshift(frameLayer);
  trimRadarFrameBuffer();
}

function trimRadarFrameBuffer() {
  const maxFrames = clampHistoryDepth(radarHistoryConfig.frameCount);
  while (radarHistoryState.frames.length > maxFrames) {
    radarHistoryState.frames.pop();
  }
}

function drawRadarHistory() {
  if (!radarHistoryCtx || !radarHistoryState.frames.length) {
    return;
  }
  const framesToRender = radarHistoryState.frames.slice(0, clampHistoryDepth(radarHistoryConfig.frameCount));
  if (!framesToRender.length) return;

  const { width, height } = getLayerDimensions(framesToRender[0]);
  if (!width || !height) {
    return;
  }

  if (radarHistoryCanvas.width !== width || radarHistoryCanvas.height !== height) {
    radarHistoryCanvas.width = width;
    radarHistoryCanvas.height = height;
  }
  if (radarHistoryState.offscreen) {
    if (radarHistoryState.offscreen.width !== width || radarHistoryState.offscreen.height !== height) {
      radarHistoryState.offscreen.width = width;
      radarHistoryState.offscreen.height = height;
    }
  }

  radarHistoryCtx.clearRect(0, 0, width, height);

  // Draw history frames first (oldest to newest) so their contributions remain under the latest frame
  for (let i = framesToRender.length - 1; i >= 1; i--) {
    const norm = framesToRender.length > 1 ? i / (framesToRender.length - 1) : 1;
    const alpha = computeHistoryAlpha(norm);
    if (alpha <= 0.02) continue;
    drawTintedFrame(framesToRender[i], radarHistoryConfig.historyColor, alpha);
  }

  // Draw current frame last so it stays dominant
  drawTintedFrame(framesToRender[0], radarHistoryConfig.currentColor, 1);
}

function drawTintedFrame(frameLayer, color, alpha) {
  if (!radarHistoryCtx || !frameLayer?.layer) return;
  const width = frameLayer.width;
  const height = frameLayer.height;
  if (!width || !height) return;

  if (radarHistoryState.offscreen && radarHistoryState.offscreenCtx) {
    const offscreenCtx = radarHistoryState.offscreenCtx;
    if (radarHistoryState.offscreen.width !== width || radarHistoryState.offscreen.height !== height) {
      radarHistoryState.offscreen.width = width;
      radarHistoryState.offscreen.height = height;
    }
    offscreenCtx.clearRect(0, 0, width, height);
    offscreenCtx.globalCompositeOperation = 'copy';
    offscreenCtx.globalAlpha = 1;
    offscreenCtx.drawImage(frameLayer.layer, 0, 0, width, height);
    offscreenCtx.globalCompositeOperation = 'source-in';
    offscreenCtx.fillStyle = color;
    offscreenCtx.fillRect(0, 0, width, height);
    offscreenCtx.globalCompositeOperation = 'source-over';
    radarHistoryCtx.globalAlpha = alpha;
    radarHistoryCtx.drawImage(radarHistoryState.offscreen, 0, 0, width, height);
  } else {
    radarHistoryCtx.globalAlpha = alpha;
    radarHistoryCtx.drawImage(frameLayer.layer, 0, 0, width, height);
  }
  radarHistoryCtx.globalAlpha = 1;
}

function computeHistoryAlpha(norm) {
  const pivot = radarHistoryConfig.fadePivot;
  const steep = radarHistoryConfig.fadeSteepness;
  const alpha = 1 / (1 + Math.exp(steep * (norm - pivot)));
  return clamp01(alpha);
}

function getLayerDimensions(frameLayer) {
  if (!frameLayer) return { width: 0, height: 0 };
  return {
    width: frameLayer.width || 0,
    height: frameLayer.height || 0
  };
}

async function blobToBitmap(blob) {
  if ('createImageBitmap' in window) {
    return await createImageBitmap(blob);
  }
  return await blobToHtmlImage(blob);
}

function blobToHtmlImage(blob) {
  return new Promise((resolve, reject) => {
    const img = new Image();
    const url = URL.createObjectURL(blob);
    img.onload = () => {
      URL.revokeObjectURL(url);
      resolve(img);
    };
    img.onerror = (err) => {
      URL.revokeObjectURL(url);
      reject(err);
    };
    img.src = url;
  });
}

function createFrameLayer(bitmap) {
  const { width, height } = getBitmapDimensions(bitmap);
  if (!width || !height) {
    return null;
  }
  const canvas = document.createElement('canvas');
  canvas.width = width;
  canvas.height = height;
  const ctx = canvas.getContext('2d', { willReadFrequently: true });
  if (!ctx) {
    return null;
  }
  ctx.drawImage(bitmap, 0, 0, width, height);
  const imageData = ctx.getImageData(0, 0, width, height);
  const data = imageData.data;
  for (let i = 0; i < data.length; i += 4) {
    const r = data[i];
    const g = data[i + 1];
    const b = data[i + 2];
    const luminance = Math.max(r, g, b);
    data[i] = 255;
    data[i + 1] = 255;
    data[i + 2] = 255;
    data[i + 3] = luminance;
  }
  ctx.putImageData(imageData, 0, 0);
  return { layer: canvas, width, height, timestamp: Date.now() };
}

function getBitmapDimensions(bitmap) {
  if (!bitmap) {
    return { width: 0, height: 0 };
  }
  return {
    width: bitmap.width || bitmap.naturalWidth || bitmap.videoWidth || 0,
    height: bitmap.height || bitmap.naturalHeight || bitmap.videoHeight || 0
  };
}

function setFallbackImageFromBlob(blob) {
  if (!radarImageEl) return;
  if (radarHistoryState.fallbackUrl) {
    URL.revokeObjectURL(radarHistoryState.fallbackUrl);
    radarHistoryState.fallbackUrl = null;
  }
  const url = URL.createObjectURL(blob);
  radarHistoryState.fallbackUrl = url;
  radarImageEl.src = url;
  setFallbackVisibility(false);
}

function triggerFallbackImage() {
  if (!radarImageEl) return;
  const img = new Image();
  img.onload = () => {
    if (radarImageEl) {
      radarImageEl.src = img.src;
      setFallbackVisibility(true);
    }
  };
  img.src = '/newest_png?rand=' + Date.now();
}

function setFallbackVisibility(visible) {
  if (!radarImageEl) return;
  if (visible) {
    radarImageEl.setAttribute('data-visible', 'true');
  } else {
    radarImageEl.removeAttribute('data-visible');
  }
}

function hydrateRadarHistoryConfig() {
  if (!isRadarView) return;
  try {
    const stored = localStorage.getItem(RADAR_HISTORY_STORAGE_KEY);
    if (stored) {
      const parsed = JSON.parse(stored);
      radarHistoryConfig = {
        ...radarHistoryConfig,
        ...parsed
      };
    }
  } catch (err) {
    console.warn('Unable to load radar history config', err);
  }
  radarHistoryConfig.frameCount = clampHistoryDepth(radarHistoryConfig.frameCount);
  radarHistoryConfig.currentColor = sanitizeColor(radarHistoryConfig.currentColor, radarHistoryDefaults.currentColor);
  radarHistoryConfig.historyColor = sanitizeColor(radarHistoryConfig.historyColor, radarHistoryDefaults.historyColor);
}

function persistRadarHistoryConfig() {
  if (!isRadarView) return;
  try {
    localStorage.setItem(RADAR_HISTORY_STORAGE_KEY, JSON.stringify({
      frameCount: clampHistoryDepth(radarHistoryConfig.frameCount),
      currentColor: radarHistoryConfig.currentColor,
      historyColor: radarHistoryConfig.historyColor
    }));
  } catch (err) {
    console.warn('Unable to save radar history config', err);
  }
}

function setupRadarHistoryControls() {
  const depthRange = byId('historyDepthRange');
  const currentColorInput = byId('currentFrameColor');
  const historyColorInput = byId('historyFrameColor');
  const resetBtn = byId('resetHistoryControls');

  const updateDepthUI = (value) => {
    const clamped = clampHistoryDepth(value);
    if (depthRange) depthRange.value = clamped;
    radarHistoryConfig.frameCount = clamped;
    trimRadarFrameBuffer();
    drawRadarHistory();
    persistRadarHistoryConfig();
  };

  if (depthRange) {
    depthRange.value = radarHistoryConfig.frameCount;
    depthRange.addEventListener('input', (e) => updateDepthUI(e.target.value));
  }

  if (currentColorInput) {
    currentColorInput.value = radarHistoryConfig.currentColor;
    currentColorInput.addEventListener('input', (e) => {
      radarHistoryConfig.currentColor = sanitizeColor(e.target.value, radarHistoryDefaults.currentColor);
      persistRadarHistoryConfig();
      drawRadarHistory();
    });
  }

  if (historyColorInput) {
    historyColorInput.value = radarHistoryConfig.historyColor;
    historyColorInput.addEventListener('input', (e) => {
      radarHistoryConfig.historyColor = sanitizeColor(e.target.value, radarHistoryDefaults.historyColor);
      persistRadarHistoryConfig();
      drawRadarHistory();
    });
  }

  if (resetBtn) {
    resetBtn.addEventListener('click', () => {
      radarHistoryConfig.currentColor = radarHistoryDefaults.currentColor;
      radarHistoryConfig.historyColor = radarHistoryDefaults.historyColor;
      if (currentColorInput) currentColorInput.value = radarHistoryConfig.currentColor;
      if (historyColorInput) historyColorInput.value = radarHistoryConfig.historyColor;
      persistRadarHistoryConfig();
      drawRadarHistory();
    });
  }
}

function clampHistoryDepth(value) {
  const num = Number(value);
  if (!Number.isFinite(num)) return radarHistoryDefaults.frameCount;
  return Math.min(50, Math.max(1, Math.round(num)));
}

function sanitizeColor(value, fallback) {
  if (typeof value !== 'string') return fallback;
  const hex = value.trim();
  if (/^#[0-9a-fA-F]{6}$/.test(hex)) {
    return hex.toLowerCase();
  }
  return fallback;
}

window.addEventListener('beforeunload', () => {
  radarHistoryState.frames.forEach(frame => {
    if (frame && typeof frame.bitmap?.close === 'function') {
      frame.bitmap.close();
    }
  });
  if (radarHistoryState.fallbackUrl) {
    URL.revokeObjectURL(radarHistoryState.fallbackUrl);
  }
});

/* info overlay */
const infoIcon = byId('infoIcon');
const infoCard = byId('infoCard');
if (infoIcon && infoCard) {
  infoIcon.onclick = () => {
    infoCard.classList.add('show');
    fetch('/get_system_info')
      .then(r => r.json())
      .then(d => setHtml('newestPng', d.newest_png || '–'));
  };
  const closeInfo = byId('closeInfo');
  if (closeInfo) {
    closeInfo.onclick = () => infoCard.classList.remove('show');
  }
}

function setCardCalibrationUI(disabled) {
  document.querySelectorAll('.calib-lock-card').forEach(card => {
    if (disabled) {
      card.classList.add('disabled');
      // Optional: Set disabled attr for accessibility
      card.querySelectorAll('input, button, select, textarea').forEach(el => el.disabled = true);
    } else {
      card.classList.remove('disabled');
      card.querySelectorAll('input, button, select, textarea').forEach(el => {
        el.disabled = el.dataset.unavailable === 'true';
      });
    }
  });
}

/* ---------- POWER ---------- */
function toggleEcho(mode){
  fetch(`/toggle_echo?radarNo=${v('radarNo')}&mode=${mode}`)
    .then(r=>r.text()).then(t=>setCallout('txResult', t, 'info'));
}
function refreshTx(){
  return readback('TX', 'txResult');
}
function beginCalibration() {
  fetch('/begin_calibration')
    .then(r => r.text())
    .then(t => setCallout('txResult', t, 'info'));
  setCardCalibrationUI(true);  // disables cards
  calibrationActive = true;
}
function stopCalibration() {
  fetch('/stop_pattern')
    .then(r => r.text())
    .then(t => setCallout('txResult', t, 'info'));
  setCardCalibrationUI(false); // re-enables cards
  calibrationActive = false;
}
function beginRezboostCalibration() {
  fetch('/run_ramp_pattern')
    .then(resp => resp.text())
    .then(html => setCallout('txResult', html, 'info'))
    .catch(err => {
      setCallout('txResult', `Error starting RezBoost calibration: ${err.message}`, 'error');
    });
}



/* ---------- Pattern ramping ---------- */
function ensurePatternLoaded() {
  if (!lastLoadedPattern) {
    setCallout('patternStatus', 'Load a pattern file first.', 'error');
    return false;
  }
  return true;
}

function getPatternRepeat() {
  const input = byId('patternRepeat');
  if (!input) return 1;
  const value = parseInt(input.value, 10);
  if (Number.isNaN(value)) return 1;
  if (value === 0) return 1;
  return value < -1 ? -1 : value;
}

function getPatternLabel() {
  const labelInput = byId('patternLabel');
  const custom = labelInput && labelInput.value.trim();
  if (custom) return custom;
  if (lastLoadedPattern && lastLoadedPattern.label) return lastLoadedPattern.label;
  return `Pattern ${patternQueue.length + 1}`;
}

function startGainSweep() {
  if (!ensurePatternLoaded()) return;
  const repeat = getPatternRepeat();
  const label = getPatternLabel();
  const repeatText = repeat === -1 ? 'infinite (loops forever)' : repeat;
  if (lastLoadedPattern) {
    lastLoadedPattern.label = label;
  }
  setCallout('patternStatus', `Running "${label}" with repeat ${repeatText}.`, 'info');
  setCallout('patternRunResult', 'Starting pattern…', 'info');
  fetch(`/run_pattern?steps=${encodeURIComponent(lastLoadedPattern.steps)}&repeat=${repeat}&label=${encodeURIComponent(label)}`)
    .then(r => r.text())
    .then(t => {
      setCallout('patternRunResult', t, 'info');
      setCallout('patternStatus', `Pattern command sent to radar for "${label}" (repeat ${repeatText}).`, 'success');
    })
    .catch(err => {
      setCallout('patternRunResult', err.message, 'error');
      setCallout('patternStatus', `Failed to start pattern: ${err.message}`, 'error');
    });
}

function stopGainSweep() {
  setCallout('patternRunResult', 'Stopping pattern…', 'info');
  fetch('/stop_pattern')
    .then(r => r.text())
    .then(t => {
      setCallout('patternRunResult', t, 'info');
      setCallout('patternStatus', 'Stop command acknowledged.', 'success');
    })
    .catch(err => {
      setCallout('patternRunResult', err.message, 'error');
      setCallout('patternStatus', `Failed to stop pattern: ${err.message}`, 'error');
    });
  setCallout('patternStatus', 'Stop requested for the active pattern.', 'info');
}

function loadPatternFromFile() {
  const input = byId('patternFile');
  if (!input || !input.files.length) {
    setCallout('patternStatus', 'Select a pattern file first.', 'error');
    return;
  }

  const file = input.files[0];
  const reader = new FileReader();
  reader.onload = () => {
    const raw = reader.result || '';
    const normalized = normalizePatternInput(raw);
    if (!normalized) {
      setCallout('patternStatus', `${file.name} did not contain any steps.`, 'error');
      return;
    }
    const stepsCount = countPatternSteps(normalized);
    const defaultLabel = file.name.replace(/\.[^.]+$/, '') || file.name;
    const labelInput = byId('patternLabel');
    if (labelInput && !labelInput.value) {
      labelInput.value = defaultLabel;
    }
    lastLoadedPattern = {
      label: defaultLabel,
      steps: normalized,
      stepsCount
    };
    const repeatValue = getPatternRepeat();
    const repeatDisplay = repeatValue === -1 ? 'infinite (loops forever)' : repeatValue;
    setCallout('patternStatus', `Loaded ${file.name} · ${stepsCount} steps ready. Repeat ${repeatDisplay}.`, 'success');
    setCallout('patternRunResult', '', 'info');
  };
  reader.onerror = () => {
    setCallout('patternStatus', `Failed to read ${file.name}.`, 'error');
  };
  reader.readAsText(file);
}

function loadPatternFromTextarea() {
  const textarea = byId('patternTextarea');
  if (!textarea) return;

  // Toggle textarea visibility
  if (textarea.style.display === 'none') {
    textarea.style.display = '';
    // Show help text if textarea is empty
    if (!textarea.value) {
      const helpText = `Legacy format: range,gain,pulse,rezboost,speed,interp,repeat
Advanced format: range,gain,pulse,rezboost,speed,interp,sea,rain,mainbang,ir,bird,doppler,repeat

range(0-21) gain(0-255) pulse(0=S1…5=L) rezboost(0=Off…3=High)
speed(0=Lo,1=Hi,2=Auto) interp(0=2048,1=4096,2=8192,3=1024,4=720,5=Off)
sea/rain/mainbang use radar numeric values
ir(0=off,1=on) bird(0=off..3=high) doppler(0=off,1=internal,2=external)
In advanced patterns, any field except repeat can be set to NaN to preserve the current radar setting
repeat(sweeps at this setting)

Examples:
0,20,0,0,2,5,2
0,128,0,0,2,5,10,0,0,1,0,0,2
0,128,0,0,2,5,NaN,NaN,2,keep,3,NaN,2

Separate steps with semicolons`;
      textarea.value = helpText;
      textarea.dataset.helpText = helpText;

      // Clear on first interaction
      const clearHelp = () => {
        if (textarea.value === helpText) {
          textarea.value = '';
          textarea.removeEventListener('click', clearHelp);
          textarea.removeEventListener('keydown', clearHelp);
        }
      };
      textarea.addEventListener('click', clearHelp);
      textarea.addEventListener('keydown', clearHelp);
    }
    textarea.focus();
    return;
  }

  const steps = normaliseTextareaSteps();
  if (!steps) {
    setCallout('patternStatus', 'Paste pattern steps before using this option.', 'error');
    return;
  }
  const stepsCount = countPatternSteps(steps);
  const labelInput = byId('patternLabel');
  if (labelInput && !labelInput.value) {
    labelInput.value = `Pattern ${patternQueue.length + 1}`;
  }
  lastLoadedPattern = {
    label: getPatternLabel(),
    steps,
    stepsCount
  };
  const repeatValue = getPatternRepeat();
  const repeatDisplay = repeatValue === -1 ? 'infinite (loops forever)' : repeatValue;
  setCallout('patternStatus', `Using pasted steps · ${stepsCount} steps ready. Repeat ${repeatDisplay}.`, 'success');
  setCallout('patternRunResult', '', 'info');
}

function addPatternToQueue() {
  if (!ensurePatternLoaded()) return;
  const repeat = getPatternRepeat();
  const label = getPatternLabel();
  const repeatText = repeat === -1 ? 'infinite (loops forever)' : repeat;
  if (lastLoadedPattern) {
    lastLoadedPattern.label = label;
  }
  patternQueue.push({
    label,
    repeat,
    steps: lastLoadedPattern.steps,
    stepsCount: lastLoadedPattern.stepsCount
  });
  renderPatternQueue();
  const total = patternQueue.length;
  const totalText = total === 1 ? '1 item' : `${total} items`;
  setCallout('patternRunResult', `Added "${label}" (repeat ${repeatText}) to queue. Queue now has ${totalText}.`, 'success');

  // Clear inputs for next pattern
  const fileInput = byId('patternFile');
  if (fileInput) fileInput.value = '';
  const labelInput = byId('patternLabel');
  if (labelInput) labelInput.value = '';
  const textarea = byId('patternTextarea');
  if (textarea) {
    textarea.value = '';
    textarea.style.display = 'none';
  }
  lastLoadedPattern = null;
}

function startPatternQueue() {
  if (!patternQueue.length) {
    setCallout('patternRunResult', 'Queue is empty.', 'error');
    return;
  }

  const payload = patternQueue
    .map(item => `${item.repeat}:${btoa(unescape(encodeURIComponent(item.label || 'pattern')))}:${btoa(item.steps)}`)
    .join(',');

  setCallout('patternRunResult', 'Starting queued patterns…', 'info');
  setCallout('patternStatus', `Running queued patterns (${patternQueue.length} items).`, 'info');
  fetch('/run_pattern_queue', {
    method: 'POST',
    headers: { 'Content-Type': 'application/x-www-form-urlencoded' },
    body: `payload=${encodeURIComponent(payload)}`
  })
    .then(r => r.text())
    .then(t => {
      setCallout('patternRunResult', t, 'info');
      setCallout('patternStatus', `Queued patterns are running (${patternQueue.length} items).`, 'success');
    })
    .catch(err => {
      setCallout('patternRunResult', err.message, 'error');
      setCallout('patternStatus', `Failed to start queued patterns: ${err.message}`, 'error');
    });
}

function clearPatternQueue() {
  patternQueue.length = 0;
  renderPatternQueue();
  if (lastLoadedPattern) {
    setCallout('patternStatus', `Queue cleared. Queue is now empty. "${lastLoadedPattern.label}" is still loaded (${lastLoadedPattern.stepsCount} steps).`, 'info');
  } else {
    setCallout('patternStatus', 'Queue cleared. Queue is now empty. Load a pattern file to begin.', 'info');
  }
  setCallout('patternRunResult', '', 'info');
}

/* ---------- RANGE / PULSE ---------- */
function applyRangePulse(){ setRange(()=>setPulse()); }
function setRange(cb){
  fetch(`/set_range?radarNo=${v('radarNo')}&rangeIndex=${v('rangeIndex')}&rangeUnit=0`)
    .then(r=>r.text()).then(t=>{showServerResponse('rangeResult', t); if(cb && serverResponseVariant(t) !== 'error')cb();});
}
function setPulse(){
  fetch(`/set_pulse_width?radarNo=${v('radarNo')}&pulse=${v('pwPulse')}`
       + `&rangeIndex=${v('rangeIndex')}&rangeUnit=0`)
    .then(r=>r.text()).then(t=>showServerResponse('rangeResult', t));
}
function refreshRange(){
  return readback('RANGE', 'rangeResult');
}

function refreshPulse(){
  return readback('PULSE', 'rangeResult');
}

/* ---------- GAIN & CLUTTER ---------- */
function setGainManual(){
  fetch(`/set_gain?radarNo=${v('radarNo')}&status=0&manualValue=${v('gainManual')}&autoValue=0`)
    .then(r=>r.text()).then(t=>showServerResponse('gainResult', t));
}
function setGainAuto(){
  fetch(`/set_gain?radarNo=${v('radarNo')}&status=1&manualValue=0&autoValue=${v('gainAuto')}`)
    .then(r=>r.text()).then(t=>showServerResponse('gainResult', t));
}
function setACSEA(){
  const seaMode = v('seaMode'); // manual | auto
  const status = seaMode === 'auto' ? 1 : 0;
  fetch(`/set_acsea?radarNo=${v('radarNo')}&status=${status}&manual=${v('seaMan')}&auto=${v('seaAuto')}&mode=0`)
    .then(r=>r.text()).then(t=>showServerResponse('clutterResult', t));
}
function setACRAIN(){
  const rainUiMode = v('rainMode'); // manual | auto
  const status = rainUiMode === 'auto' ? 1 : 0;
  fetch(`/set_acrain?radarNo=${v('radarNo')}&status=${status}&manual=${v('rainMan')}&auto=${v('rainAuto')}&ftcMode=0&rainMode=0`)
    .then(r=>r.text()).then(t=>showServerResponse('clutterResult', t));
}
function refreshGain(){
  return readback('GAIN', 'gainResult');
}

function refreshClutter(){
  return readback('CLUTTER', 'clutterResult');
}
function setRezBoost() {
  fetch(`/set_bearing_resolution?radarNo=${v('radarNo')}&mode=${v('rezboostMode')}`)
    .then(r => r.text())
    .then(t => showServerResponse('rezboostResult', t));
}
function refreshRezBoost() {
  setCallout('rezboostResult', 'RezBoost refresh is unavailable until a read endpoint is provided.', 'info');
}


/* ---------- SIGNAL PROCESSING ---------- */
function setMainBang(){
  fetch(`/set_main_bang?value=${v('mainBangVal')}`)
    .then(r=>r.text()).then(t=>showServerResponse('signalResult', t));
}
function refreshMainBang(){
  return readback('MAINBANG', 'signalResult');
}
function setIR(){
  fetch(`/set_interference_rejection?enabled=${v('irMode')}`)
    .then(r=>r.text()).then(t=>showServerResponse('signalResult', t));
}
function refreshIR(){
  return readback('IR', 'signalResult');
}
function setBirdMode(){
  fetch(`/set_bird_mode?level=${v('birdMode')}`)
    .then(r=>r.text()).then(t=>showServerResponse('signalResult', t));
}
function refreshBirdMode(){
  return readback('BIRDMODE', 'signalResult');
}
function setDoppler(){
  fetch(`/set_doppler?mode=${v('dopplerMode')}`)
    .then(r=>r.text()).then(t=>showServerResponse('signalResult', t));
}
function refreshDoppler(){
  return readback('DOPPLER', 'signalResult');
}

/* ---------- RECORDING ---------- */
function toggleRecording(mode){
  fetch(`/toggle_recording?mode=${mode}`)
    .then(r=>r.text()).then(t=>{
      const variant = t.toLowerCase().includes('started') ? 'success' : 'info';
      setCallout('recordResult', t, variant);
    });
}

/* ---------- CONNECTION ---------- */
function reconnectRadar(){
  setCallout('connStatus', 'Reconnecting...', 'info');
  fetch('/reconnect')
    .then(r=>r.text()).then(t=>{
      const ok = t.toLowerCase().includes('reconnected');
      setCallout('connStatus', t, ok ? 'success' : 'error');
    })
    .catch(err=>setCallout('connStatus', `Error: ${err.message}`, 'error'));
}
function refreshConnectionStatus(){
  fetch('/get_system_info')
    .then(r=>r.json())
    .then(d=>{
      const parts = [];
      parts.push(d.connected ? 'Connected' : 'Disconnected');
      if(d.tx_active) parts.push('TX ON');
      if(d.firmware) parts.push(`FW: ${d.firmware}`);
      const variant = d.connected ? 'success' : 'error';
      setCallout('connStatus', parts.join(' | '), variant);
    })
    .catch(()=>setCallout('connStatus', 'Unable to reach server', 'error'));
}

/* ---------- DISPLAY ---------- */
function applySweepInterp(){
  setCallout('displaySweepResult', 'Sweep count and interpolation controls are unavailable.', 'info');
}
function setAntennaRev(){
  fetch(`/set_antenna_revolution?radarNo=${v('radarNo')}&rollSpeed=${v('rollSpd')}&hiRpm=${v('hiRpm')}`)
    .then(r=>r.text()).then(t=>showServerResponse('antennaResult', t))
    .catch(err => setCallout('antennaResult', `Failed: ${err.message}`, 'error'));
}
function setHighRpm(){
  fetch(`/set_antenna_revolution?radarNo=${v('radarNo')}&rollSpeed=${v('rollSpd')}&hiRpm=${v('hiRpm')}`)
    .then(r=>r.text()).then(t=>showServerResponse('antennaResult', t))
    .catch(err => setCallout('antennaResult', `Failed: ${err.message}`, 'error'));
}
function refreshAntenna(){
  const cmd = getReadCommand('ANTREV');
  apiText(`/get_current?radarNo=${v('radarNo')}&command=${cmd}`)
    .then(t=>setCallout('antennaResult', t, 'info'))
    .catch(err => setCallout('antennaResult', `Failed: ${err.message}`, 'error'));
}
function refreshDisplay(){
  const cmd = getReadCommand('SWEEP');
  apiText(`/get_current?radarNo=${v('radarNo')}&command=${cmd}`)
    .then(t=>setCallout('displaySweepResult', t, 'info'))
    .catch(err => setCallout('displaySweepResult', `Failed: ${err.message}`, 'error'));
}


/** Generic GET helper for JSON endpoints. */
async function apiGet(url) {
  const r = await fetch(url);
  if (!r.ok) throw new Error(`GET ${url} → ${r.status}`);
  return await r.json();
}

/** Generic GET helper for the server's text status endpoints. */
async function apiText(url) {
  const r = await fetch(url);
  if (!r.ok) throw new Error(`GET ${url} → ${r.status}`);
  return await r.text();
}

function parseResponseBody(text) {
  try {
    return JSON.parse(text);
  } catch (_) {
    return { raw: text };
  }
}

function unwrapReadback(payload) {
  if (!payload || typeof payload !== 'object') return payload;
  if (payload.data && typeof payload.data === 'object') return payload.data;
  if (payload.readback && typeof payload.readback === 'object') return payload.readback;
  if (payload.settings && typeof payload.settings === 'object') return payload.settings;
  return payload;
}

function registerFields(payload) {
  const raw = payload && typeof payload.raw === 'string' ? payload.raw : '';
  const match = raw.match(/\$N[0-9A-F]{2},([^\r\n]*)/i);
  return match ? match[1].split(',').map(value => value.trim()) : [];
}

function namedRegisterFields(payload, register) {
  const raw = payload && typeof payload.raw === 'string' ? payload.raw : '';
  const match = raw.match(new RegExp(`\\$N${register},([^\\r\\n|]*)`, 'i'));
  return match ? match[1].split(',').map(value => value.trim()) : [];
}

function firstNumber(object, keys, fields = []) {
  for (const key of keys) {
    const value = object && object[key];
    if (typeof value === 'number' && Number.isFinite(value)) return value;
    if (typeof value === 'boolean') return value ? 1 : 0;
    if (typeof value === 'string' && value.trim() !== '' && Number.isFinite(Number(value))) return Number(value);
  }
  for (const value of fields) {
    if (value !== '' && Number.isFinite(Number(value))) return Number(value);
  }
  return null;
}

function normaliseReadback(name, payload) {
  const object = unwrapReadback(payload) || {};
  const fields = registerFields(payload);
  const values = {};
  if (object.error || object.ok === false) return null;

  if (name === 'RANGE') {
    const value = firstNumber(object, ['range_index', 'rangeIndex', 'range', 'value'], fields);
    if (value !== null) values.range_index = value;
  } else if (name === 'PULSE') {
    const value = firstNumber(object, ['pulse_width', 'pulseWidth', 'pulse', 'value'], fields);
    if (value !== null) values.pulse_width = value;
    const pulseRange = firstNumber(object, ['pulse_range_index', 'pulseRangeIndex'], fields.slice(2, 3));
    if (pulseRange !== null) values.pulse_range_index = pulseRange;
  } else if (name === 'GAIN') {
    const status = firstNumber(object, ['gain_auto', 'gainAuto', 'auto', 'status']) ?? firstNumber({}, [], fields.slice(0, 1));
    const manualValue = firstNumber(object, ['gain_manual_value', 'gainManualValue', 'manual_value', 'manualValue'])
      ?? firstNumber({}, [], [fields[1]]);
    const autoValue = firstNumber(object, ['gain_auto_value', 'gainAutoValue', 'auto_value', 'autoValue'])
      ?? firstNumber({}, [], [fields[3]]);
    const value = firstNumber(object, ['gain_value', 'gainValue', 'value'])
      ?? (status === null ? null : (status ? autoValue : manualValue));
    if (status !== null) values.gain_auto = Boolean(status);
    if (value !== null) values.gain_value = value;
    if (manualValue !== null) values.gain_manual_value = manualValue;
    if (autoValue !== null) values.gain_auto_value = autoValue;
  } else if (name === 'STC') {
    const near = firstNumber(object, ['near', 'near_stc', 'nearStc']);
    const middle = firstNumber(object, ['middle', 'mid', 'middle_stc', 'midStc']);
    const far = firstNumber(object, ['far', 'far_stc', 'farStc']);
    if (near !== null) values.near_stc = near;
    if (middle !== null) values.middle_stc = middle;
    if (far !== null) values.far_stc = far;
  } else if (name === 'CLUTTER') {
    const seaFields = namedRegisterFields(payload, '64');
    const rainFields = namedRegisterFields(payload, '65');
    const sea = firstNumber(object, ['sea', 'sea_clutter', 'seaClutter'])
      ?? firstNumber({}, [], [seaFields[1], seaFields[0]]);
    const rain = firstNumber(object, ['rain', 'rain_clutter', 'rainClutter'])
      ?? firstNumber({}, [], [rainFields[1], rainFields[0]]);
    const seaAuto = firstNumber(object, ['sea_clutter_auto', 'seaClutterAuto', 'sea_auto'], seaFields.slice(0, 1));
    const seaAutoValue = firstNumber(object, ['sea_clutter_auto_value', 'seaClutterAutoValue', 'sea_auto_value'], seaFields.slice(2, 3));
    const seaMode = firstNumber(object, ['sea_clutter_mode', 'seaClutterMode', 'sea_mode'], seaFields.slice(3, 4));
    const rainAuto = firstNumber(object, ['rain_clutter_auto', 'rainClutterAuto', 'rain_auto'], rainFields.slice(0, 1));
    const rainAutoValue = firstNumber(object, ['rain_clutter_auto_value', 'rainClutterAutoValue', 'rain_auto_value'], rainFields.slice(2, 3));
    const rainFtcMode = firstNumber(object, ['rain_clutter_ftc_mode', 'rainClutterFtcMode', 'rain_ftc_mode'], rainFields.slice(3, 4));
    const rainMode = firstNumber(object, ['rain_clutter_mode', 'rainClutterMode', 'rain_mode'], rainFields.slice(4, 5));
    if (sea !== null) values.sea_clutter = sea;
    if (rain !== null) values.rain_clutter = rain;
    if (seaAuto !== null) values.sea_clutter_auto = Boolean(seaAuto);
    if (seaAutoValue !== null) values.sea_clutter_auto_value = seaAutoValue;
    if (seaMode !== null) values.sea_clutter_mode = seaMode;
    if (rainAuto !== null) values.rain_clutter_auto = Boolean(rainAuto);
    if (rainAutoValue !== null) values.rain_clutter_auto_value = rainAutoValue;
    if (rainFtcMode !== null) values.rain_clutter_ftc_mode = rainFtcMode;
    if (rainMode !== null) values.rain_clutter_mode = rainMode;
  } else if (name === 'ANTREV') {
    const speed = firstNumber(object, ['antenna_speed', 'antennaSpeed', 'rotation_speed', 'rotationSpeed', 'speed'], fields);
    if (speed !== null) values.antenna_speed = speed;
    const hiRpm = firstNumber(object, ['antenna_hi_rpm', 'antennaHiRpm', 'hi_rpm', 'hiRpm'], fields.slice(1, 2));
    if (hiRpm !== null) values.antenna_hi_rpm = Boolean(hiRpm);
  } else if (name === 'MAINBANG') {
    const value = firstNumber(object, ['main_bang', 'mainBang', 'value'], fields);
    if (value !== null) values.main_bang = value;
  } else if (name === 'IR') {
    const value = firstNumber(object, ['interference_rejection', 'interferenceRejection', 'enabled', 'value'], fields);
    if (value !== null) values.interference_rejection = Boolean(value);
  } else if (name === 'BIRDMODE') {
    const value = firstNumber(object, ['bird_mode', 'birdMode', 'level', 'value'], fields);
    if (value !== null) values.bird_mode = value;
  } else if (name === 'DOPPLER') {
    const value = firstNumber(object, ['doppler', 'mode', 'value'], fields);
    if (value !== null) values.doppler = value;
  } else if (name === 'TX') {
    if (typeof object.tx_active === 'boolean') values.tx_active = object.tx_active;
    else if (/transmitting|\btx\s*on|,2[,\s]/i.test(payload.raw || '')) values.tx_active = true;
    else if (/standby|\btx\s*off|,1[,\s]/i.test(payload.raw || '')) values.tx_active = false;
  }
  return Object.keys(values).length ? values : null;
}

function applyReadbackToControls(values) {
  const set = (id, value) => { const el = byId(id); if (el && value !== undefined && value !== null) el.value = value; };
  if (values.range_index !== undefined) set('rangeIndex', values.range_index);
  if (values.pulse_width !== undefined) set('pwPulse', values.pulse_width);
  if (values.gain_auto !== undefined) set('gainMode', values.gain_auto ? 'auto' : 'manual');
  if (values.gain_manual_value !== undefined) set('gainManual', values.gain_manual_value);
  if (values.gain_auto_value !== undefined) set('gainAuto', values.gain_auto_value);
  if (values.gain_value !== undefined && values.gain_manual_value === undefined && values.gain_auto_value === undefined) {
    set(values.gain_auto ? 'gainAuto' : 'gainManual', values.gain_value);
  }
  if (values.near_stc !== undefined) set('nearStc', values.near_stc);
  if (values.middle_stc !== undefined) set('midStc', values.middle_stc);
  if (values.far_stc !== undefined) set('farStc', values.far_stc);
  if (values.main_bang !== undefined) set('mainBangVal', values.main_bang);
  if (values.interference_rejection !== undefined) set('irMode', values.interference_rejection ? 1 : 0);
  if (values.bird_mode !== undefined) set('birdMode', values.bird_mode);
  if (values.doppler !== undefined) set('dopplerMode', values.doppler);
  if (values.antenna_speed !== undefined) set('rollSpd', values.antenna_speed);
  if (values.antenna_hi_rpm !== undefined) set('hiRpm', values.antenna_hi_rpm ? 1 : 0);
  if (values.sea_clutter_auto !== undefined) set('seaMode', values.sea_clutter_auto ? 'auto' : 'manual');
  if (values.sea_clutter !== undefined) set('seaMan', values.sea_clutter);
  if (values.sea_clutter_auto_value !== undefined) set('seaAuto', values.sea_clutter_auto_value);
  if (values.rain_clutter_auto !== undefined) set('rainMode', values.rain_clutter_auto ? 'auto' : 'manual');
  if (values.rain_clutter !== undefined) set('rainMan', values.rain_clutter);
  if (values.rain_clutter_auto_value !== undefined) set('rainAuto', values.rain_clutter_auto_value);
}

const READBACK_FIELDS = Object.freeze({
  TX: ['tx_state'], RANGE: ['range_index'], PULSE: ['pulse_width', 'pulse_range_index'],
  GAIN: ['gain_auto', 'gain_value', 'gain_manual_value', 'gain_auto_value'],
  CLUTTER: ['sea_clutter', 'sea_clutter_auto', 'sea_clutter_auto_value', 'sea_clutter_mode',
    'rain_clutter', 'rain_clutter_auto', 'rain_clutter_auto_value', 'rain_clutter_ftc_mode', 'rain_clutter_mode'],
  ANTREV: ['antenna_speed', 'antenna_hi_rpm'], STC: ['near_stc', 'middle_stc', 'far_stc'],
  MAINBANG: ['main_bang'], IR: ['interference_rejection'], BIRDMODE: ['bird_mode'], DOPPLER: ['doppler']
});

function clearReadback(name) {
  (READBACK_FIELDS[name] || []).forEach(field => {
    delete readbackState.values[field];
    readbackState.invalid.add(field);
  });
}

function parseReadbackTime(value) {
  if (typeof value !== 'string') return null;
  const unix = value.match(/^unix_ms:(\d+)$/);
  if (unix) return Number(unix[1]);
  const parsed = Date.parse(value);
  return Number.isFinite(parsed) ? parsed : null;
}

function flattenSettingsReadback(snapshot) {
  if (!snapshot || typeof snapshot !== 'object') return {};
  const values = {};
  const fields = snapshot.fields && typeof snapshot.fields === 'object' ? snapshot.fields : {};
  const captured = parseReadbackTime(snapshot.captured_at);
  const validUntil = parseReadbackTime(snapshot.valid_until);
  Object.entries(fields).forEach(([name, field]) => {
    const readAt = parseReadbackTime(field?.read_at);
    const stale = validUntil !== null
      ? validUntil <= Date.now()
      : (captured !== null && Date.now() - captured > 5000);
    if (field && field.verified === true && !stale && field.value !== null && field.value !== undefined) {
      values[name] = field.value;
    }
    if (field && (field.verified !== true || stale)) readbackState.invalid.add(name);
    if (readAt !== null && (!readbackState.lastAt || readAt > readbackState.lastAt.getTime())) readbackState.lastAt = new Date(readAt);
  });
  if (captured !== null && (!readbackState.lastAt || captured > readbackState.lastAt.getTime())) readbackState.lastAt = new Date(captured);
  readbackState.validUntil = parseReadbackTime(snapshot.valid_until);
  readbackState.invalid.forEach(field => delete readbackState.values[field]);
  return values;
}

function renderReadbackStatus(message, variant = 'info') {
  setCallout('settingsReadbackStatus', message, variant);
  const status = byId('siSettingsVerified');
  if (status) {
    status.textContent = message;
    status.className = `status-val ${variant === 'success' ? 'status-val--ok' : 'status-val--warn'}`;
  }
  const note = byId('siSettingsNote');
  if (note) note.textContent = message;
  const timestamp = byId('siSettingsReadAt');
  if (timestamp && readbackState.lastAt) timestamp.textContent = readbackState.lastAt.toLocaleTimeString();
}

async function readback(name, resultId) {
  const command = getReadCommand(name);
  try {
    const text = await apiText(`/get_current?radarNo=${encodeURIComponent(v('radarNo'))}&command=${encodeURIComponent(command)}&format=json`);
    const values = normaliseReadback(name, parseResponseBody(text));
    if (!values) throw new Error('The radar response did not contain a confirmed value.');
    Object.assign(readbackState.values, values);
    Object.keys(values).forEach(field => readbackState.invalid.delete(field));
    readbackState.lastAt = new Date();
    readbackState.lastError = null;
    applyReadbackToControls(values);
    renderReadbackStatus(`Confirmed radar readback updated ${readbackState.lastAt.toLocaleTimeString()}.`, 'success');
    if (resultId) setCallout(resultId, `Confirmed radar readback: ${text}`, 'success');
    return values;
  } catch (error) {
    clearReadback(name);
    readbackState.lastError = error;
    renderReadbackStatus(`Readback failed for ${name}: ${error.message}`, 'error');
    if (resultId) setCallout(resultId, `Readback failed: ${error.message}`, 'error');
    throw error;
  }
}

async function readFreshSnapshot() {
  const text = await apiText(`/get_current?radarNo=${encodeURIComponent(v('radarNo'))}&command=ALL&format=json&fresh=1`);
  const payload = parseResponseBody(text);
  const snapshot = payload.settings_readback || payload.readback || payload.data || payload;
  const values = flattenSettingsReadback(snapshot);
  if (!Object.keys(values).length) throw new Error('The fresh response contained no verified settings.');
  Object.assign(readbackState.values, values);
  Object.keys(values).forEach(field => readbackState.invalid.delete(field));
  applyReadbackToControls(values);
  readbackState.lastError = null;
  renderReadbackStatus(snapshot.verified === true
    ? `Confirmed radar readback captured ${readbackState.lastAt?.toLocaleTimeString() || 'now'}.`
    : 'Partial radar readback captured. Unverified fields remain unavailable.', snapshot.verified === true ? 'success' : 'error');
  return { snapshot, values };
}

async function refreshAllReadbacks() {
  const names = [
    ['RANGE', 'rangeResult'], ['PULSE', 'rangeResult'], ['GAIN', 'gainResult'],
    ['CLUTTER', 'clutterResult'], ['ANTREV', 'antennaResult'], ['STC', 'gainResult'],
    ['MAINBANG', 'signalResult'], ['IR', 'signalResult'], ['BIRDMODE', 'signalResult'],
    ['DOPPLER', 'signalResult']
  ];
  try {
    const fresh = await readFreshSnapshot();
    const expected = Object.keys(READBACK_FIELDS).reduce((count, name) => count + READBACK_FIELDS[name].length, 0);
    const valid = Object.keys(fresh.values).length;
    if (fresh.snapshot.verified === true) {
      renderReadbackStatus(`Confirmed fresh readback for ${valid} fields.`, 'success');
    } else {
      renderReadbackStatus(`Fresh readback returned ${valid} of ${expected} fields.`, 'error');
    }
    return fresh;
  } catch (allError) {
    // Older servers do not expose the aggregate read endpoint. Fall back to
    // individual reads, while preserving the same stale-value guarantees.
    Object.keys(readbackState.values).forEach(field => {
      readbackState.invalid.add(field);
      delete readbackState.values[field];
    });
    renderReadbackStatus(`Fresh aggregate read unavailable: ${allError.message}. Trying individual reads.`, 'error');
  }
  let successCount = 0;
  for (const [name, resultId] of names) {
    try { await readback(name, resultId); successCount += 1; } catch (_) { /* status callout already explains the failure */ }
  }
  if (successCount === names.length) {
    renderReadbackStatus(`Confirmed readback for ${successCount} radar settings.`, 'success');
  } else {
    renderReadbackStatus(`Confirmed ${successCount} of ${names.length} radar settings. Unconfirmed values remain unavailable.`, 'error');
  }
}

/** Push UI fields → device */
async function setSTC() {
  const radarNo = +$('#radarNo').value;
  const nearVal = +$('#nearStc').value;
  const midVal  = +$('#midStc').value;
  const farVal  = +$('#farStc').value;
  if (Number.isNaN(radarNo)) return;

  try {
    const responses = await Promise.all([
    fetch(`/set_near_stc?radarNo=${radarNo}&value=${nearVal}`),
    fetch(`/set_middle_stc?radarNo=${radarNo}&value=${midVal}`),
    fetch(`/set_far_stc?radarNo=${radarNo}&value=${farVal}`)
    ]);
    const texts = await Promise.all(responses.map(async response => {
      if (!response.ok) throw new Error(`STC setter returned HTTP ${response.status}`);
      return response.text();
    }));
    const failure = texts.find(text => serverResponseVariant(text) === 'error');
    if (failure) throw new Error(failure);
    showServerResponse('gainResult', texts.join(' | '));
    await refreshSTC();
  } catch (error) {
    clearReadback('STC');
    setCallout('gainResult', `STC update failed: ${error.message}`, 'error');
  }
}

/** Pull device → UI fields */
async function refreshSTC() {
  return readback('STC', 'gainResult');
}

/* tiny $ helper */
function $(sel){return document.querySelector(sel);}

renderPatternQueue();

// Auto-refresh connection status on load
if (byId('connStatus')) {
  refreshConnectionStatus();
}

function formatUptime(secs) {
  const d = Math.floor(secs / 86400);
  const h = Math.floor((secs % 86400) / 3600);
  const m = Math.floor((secs % 3600) / 60);
  const s = secs % 60;
  if (d > 0) return `${d}d ${h}h ${m}m`;
  if (h > 0) return `${h}h ${m}m ${s}s`;
  return `${m}m ${s}s`;
}

const PULSE_LABELS = ['S1','S2','M1','M2','M3','L'];
const REZBOOST_LABELS = ['Off','Low','Medium','High'];
const BIRD_LABELS = ['Off','Low','Medium','High'];
const DOPPLER_LABELS = ['Off','Mode 1','Mode 2'];
const ANTENNA_LABELS = ['Low','High','Auto'];

function updateSystemInfo() {
  fetch('/get_system_info')
    .then(r => r.json())
    .then(d => {
      const set = (id, val) => { const el = byId(id); if (el) el.textContent = val; };
      const setClass = (id, val, cls) => { const el = byId(id); if (el) { el.textContent = val; el.className = 'status-val ' + cls; } };

      if (d.connected) {
        setClass('siConnected', 'Connected', 'status-val--ok');
      } else {
        setClass('siConnected', 'Disconnected', 'status-val--warn');
      }
      set('siRadarIp', d.radar_ip || '--');
      set('siHostIp', d.host_ip || '--');
      if (d.tx_active) {
        setClass('siTxState', 'Transmitting', 'status-val--ok');
      } else {
        setClass('siTxState', 'Standby', '');
      }
      set('siFirmware', d.firmware || '--');

      set('siUptime', formatUptime(d.uptime_seconds || 0));
      set('siRotations', (d.rotation_count || 0).toLocaleString());
      if (d.recording) {
        setClass('siRecording', 'Active', 'status-val--ok');
      } else {
        setClass('siRecording', 'Inactive', '');
      }
      const recordingRoot = d.recording_root || '--';
      const recordPath = d.record_dir || recordingRoot;
      const recordPathLabel = !d.record_dir && d.recording_root_configured === false
        ? `${recordPath} (default)`
        : recordPath;
      set('siRecordDir', recordPathLabel);
      set('siPatternRunning', d.pattern_running ? 'Running' : 'Idle');
      // The server may expose a confirmed snapshot as `settings_readback` or
      // `actual_settings`. The legacy `settings` field is a command cache and
      // is intentionally ignored while `settings_verified` is false.
      const serverReadback = flattenSettingsReadback(d.settings_readback);
      const confirmed = Object.keys(serverReadback).length
        ? serverReadback
        : (d.settings_verified === true ? (d.actual_settings || d.settings) : null);
      const s = { ...serverReadback, ...readbackState.values, ...(confirmed || {}) };
      const hasConfirmed = Object.keys(s).length > 0;
      const validUntil = parseReadbackTime(d.settings_readback?.valid_until);
      const capturedAt = parseReadbackTime(d.settings_readback?.captured_at);
      const readbackFresh = validUntil !== null
        ? validUntil > Date.now()
        : (capturedAt === null || Date.now() - capturedAt <= 5000);
      const fullyVerified = readbackFresh && (d.settings_verified === true || d.settings_readback?.verified === true);
      const statusText = !readbackFresh && d.settings_readback?.captured_at
        ? 'Stale readback — refresh required'
        : fullyVerified
        ? 'Verified'
        : (hasConfirmed ? 'Partial readback — some fields unavailable' : 'Unverified — use Read all confirmed settings');
      setClass('siSettingsVerified', statusText, fullyVerified ? 'status-val--ok' : 'status-val--warn');
      const note = byId('siSettingsNote');
      if (note && !hasConfirmed) note.textContent = 'No confirmed radar readback has been collected. Cached command values are hidden.';
      if (hasConfirmed) {
        set('siRange', s.range_index ?? '--');
        set('siPulse', s.pulse_width == null ? '--' : (PULSE_LABELS[s.pulse_width] || s.pulse_width));
        set('siPulseRange', s.pulse_range_index ?? '--');
        set('siGain', s.gain_auto == null ? '--' : (s.gain_auto ? 'Auto' : 'Manual'));
        set('siGainValues', s.gain_manual_value == null && s.gain_auto_value == null ? '--' : `${s.gain_manual_value ?? '--'} / ${s.gain_auto_value ?? '--'}`);
        set('siRezboost', s.rezboost == null ? '--' : (REZBOOST_LABELS[s.rezboost] || s.rezboost));
        set('siAntenna', s.antenna_speed == null ? '--' : (ANTENNA_LABELS[s.antenna_speed] || s.antenna_speed));
        set('siAntennaHiRpm', s.antenna_hi_rpm == null ? '--' : (s.antenna_hi_rpm ? 'On' : 'Off'));
        set('siDoppler', s.doppler == null ? '--' : (DOPPLER_LABELS[s.doppler] || s.doppler));
        set('siIR', s.interference_rejection == null ? '--' : (s.interference_rejection ? 'On' : 'Off'));
        set('siBird', s.bird_mode == null ? '--' : (BIRD_LABELS[s.bird_mode] || s.bird_mode));
        set('siMainBang', s.main_bang ?? '--');
        set('siSTC', s.near_stc == null || s.middle_stc == null || s.far_stc == null ? '--' : `${s.near_stc} / ${s.middle_stc} / ${s.far_stc}`);
        set('siSea', s.sea_clutter ?? '--');
        set('siSeaMode', s.sea_clutter_auto == null ? '--' : `${s.sea_clutter_auto ? 'Auto' : 'Manual'} / ${s.sea_clutter_auto_value ?? '--'} (mode ${s.sea_clutter_mode ?? '--'})`);
        set('siRain', s.rain_clutter ?? '--');
        set('siRainMode', s.rain_clutter_auto == null ? '--' : `${s.rain_clutter_auto ? 'Auto' : 'Manual'} / ${s.rain_clutter_auto_value ?? '--'} (FTC ${s.rain_clutter_ftc_mode ?? '--'}, mode ${s.rain_clutter_mode ?? '--'})`);
      } else {
        ['siRange', 'siPulse', 'siPulseRange', 'siGain', 'siGainValues', 'siRezboost', 'siAntenna', 'siAntennaHiRpm', 'siDoppler', 'siIR', 'siBird', 'siMainBang', 'siSTC', 'siSea', 'siSeaMode', 'siRain', 'siRainMode'].forEach(id => set(id, '--'));
      }
      if (d.settings_read_at || d.settings_readback_at || d.settings_readback?.captured_at) {
        set('siSettingsReadAt', d.settings_read_at || d.settings_readback_at || d.settings_readback.captured_at);
      }

      const formatAbsolute = (value, unit, digits = 0) => {
        if (typeof value !== 'number' || !Number.isFinite(value)) return '--';
        return `${value.toFixed(digits)} ${unit}`;
      };
      set('siMemoryAbs', `${formatAbsolute(d.memory_used_mb, 'MB')} / ${formatAbsolute(d.memory_total_mb, 'MB')}`);
      set('siStorageAbs', `${formatAbsolute(d.storage_used_gb, 'GB', 1)} / ${formatAbsolute(d.storage_total_gb, 'GB', 1)}`);
    })
    .catch(() => {});
}

if (byId('siConnected')) {
  updateSystemInfo();
  setInterval(updateSystemInfo, 1000);
}

const statusLabels = [
  ['statusCpuLabel', '--%'],
  ['statusMemoryLabel', '--%'],
  ['statusStorageLabel', '--%'],
  ['statusNetworkLabel', '-- / -- MB/s'],
  ['statusTempLabel', '--°C / --°F']
];
statusLabels.forEach(([id, fallback]) => {
  const label = byId(id);
  if (label) label.textContent = fallback;
});

const statusBars = ['statusCpu','statusMemory','statusStorage','statusNetwork','statusTemp'];
statusBars.forEach(id=>{
  const bar = byId(id);
  if (bar) bar.style.width = '0%';
});

function setStatusBar(id, fraction) {
  const bar = byId(id);
  if (!bar) return;
  const pct = clamp01(fraction) * 100.0;
  bar.style.width = `${pct.toFixed(1)}%`;
}

function renderCpuCores(values) {
  const container = byId('cpuCores');
  if (!container) return;
  container.innerHTML = '';
  if (!values || !values.length) {
    const msg = document.createElement('span');
    msg.className = 'small-note';
    msg.textContent = 'Per-core data unavailable.';
    container.appendChild(msg);
    return;
  }
  values.forEach((fraction, idx) => {
    const row = document.createElement('div');
    row.className = 'status-core-item';

    const label = document.createElement('span');
    label.className = 'status-core-label';
    label.textContent = `Core ${idx}`;

    const progress = document.createElement('div');
    progress.className = 'progress progress--mini';
    const bar = document.createElement('span');
    progress.appendChild(bar);
    const clamped = clamp01(fraction);
    bar.style.width = `${(clamped * 100).toFixed(1)}%`;

    const value = document.createElement('span');
    value.className = 'status-core-value';
    value.textContent = `${Math.round(clamped * 100)}%`;

    row.append(label, progress, value);
    container.appendChild(row);
  });
}

async function updateSystemMetrics() {
  const cpuBar = byId('statusCpu');
  if (!cpuBar) return;
  try {
    const resp = await fetch('/system_metrics');
    if (!resp.ok) throw new Error("HTTP " + resp.status);
    const data = await resp.json();

    const cores = Array.isArray(data.cpu_cores)
      ? data.cpu_cores.filter(value => typeof value === 'number' && Number.isFinite(value))
      : [];
    if (cores.length) {
      renderCpuCores(cores);
    } else {
      renderCpuCores([]);
    }
    const cpu = cores.length
      ? cores.reduce((sum, value) => sum + clamp01(value), 0) / cores.length
      : data.cpu_load;
    const cpuLabel = byId('statusCpuLabel');
    if (typeof cpu === 'number' && Number.isFinite(cpu)) {
      setStatusBar('statusCpu', cpu);
      if (cpuLabel) cpuLabel.textContent = `${Math.round(clamp01(cpu) * 100)}%`;
    } else {
      setStatusBar('statusCpu', 0);
      if (cpuLabel) cpuLabel.textContent = '--%';
    }

    const mem = data.memory_used;
    const memLabel = byId('statusMemoryLabel');
    if (typeof mem === 'number' && Number.isFinite(mem)) {
      setStatusBar('statusMemory', mem);
      if (memLabel) memLabel.textContent = `${Math.round(clamp01(mem) * 100)}%`;
    } else {
      setStatusBar('statusMemory', 0);
      if (memLabel) memLabel.textContent = '--%';
    }

    const storage = data.storage_used;
    const storageLabel = byId('statusStorageLabel');
    if (typeof storage === 'number' && Number.isFinite(storage)) {
      setStatusBar('statusStorage', storage);
      if (storageLabel) {
        const path = data.storage_path ? ` (${data.storage_path})` : '';
        storageLabel.textContent = `${Math.round(clamp01(storage) * 100)}%${path}`;
      }
    } else {
      setStatusBar('statusStorage', 0);
      if (storageLabel) storageLabel.textContent = '--%';
    }

    const rx = typeof data.network_rx_mbps === 'number' && Number.isFinite(data.network_rx_mbps)
      ? Math.max(0, data.network_rx_mbps)
      : null;
    const tx = typeof data.network_tx_mbps === 'number' && Number.isFinite(data.network_tx_mbps)
      ? Math.max(0, data.network_tx_mbps)
      : null;
    const networkLabel = byId('statusNetworkLabel');
    if (networkLabel) {
      const rxText = rx === null ? '--' : rx.toFixed(2);
      const txText = tx === null ? '--' : tx.toFixed(2);
      networkLabel.textContent = `${rxText} / ${txText} MB/s`;
    }
    if (rx !== null && tx !== null) {
      const networkMax = 200.0; // assume 200 MB/s peak for scaling
      setStatusBar('statusNetwork', (rx + tx) / networkMax);
    } else {
      setStatusBar('statusNetwork', 0);
    }

    const tempLabel = byId('statusTempLabel');
    if (typeof data.temperature_c === 'number' && Number.isFinite(data.temperature_c)) {
      const celsius = data.temperature_c.toFixed(1);
      const fahrenheit = (data.temperature_c * 9/5 + 32).toFixed(1);
      if (tempLabel) tempLabel.textContent = `${celsius}°C / ${fahrenheit}°F`;
      const tempFraction = clamp01(Math.max(0, data.temperature_c) / 100.0);
      setStatusBar('statusTemp', tempFraction);
    } else {
      if (tempLabel) tempLabel.textContent = '--°C / --°F';
      setStatusBar('statusTemp', 0);
    }
  } catch (err) {
    const cpuLabel = byId('statusCpuLabel');
    if (cpuLabel) cpuLabel.textContent = '--%';
    const memLabel = byId('statusMemoryLabel');
    if (memLabel) memLabel.textContent = '--%';
    const storageLabel = byId('statusStorageLabel');
    if (storageLabel) storageLabel.textContent = '--%';
    setStatusBar('statusCpu', 0);
    setStatusBar('statusMemory', 0);
    setStatusBar('statusStorage', 0);
    setStatusBar('statusNetwork', 0);
    setStatusBar('statusTemp', 0);
    const networkLabel = byId('statusNetworkLabel');
    if (networkLabel) networkLabel.textContent = '-- / -- MB/s';
    const tempLabel = byId('statusTempLabel');
    if (tempLabel) tempLabel.textContent = '--°C / --°F';
    renderCpuCores([]);
  }
}

if (byId('statusCpu')) {
  updateSystemMetrics();
  // Use 500ms refresh on status page, 5000ms elsewhere
  const isStatusPage = window.location.pathname.includes('status.html');
  const refreshInterval = isStatusPage ? 500 : 5000;
  setInterval(updateSystemMetrics, refreshInterval);
}
