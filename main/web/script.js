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

const clamp01 = value => Math.max(0, Math.min(1, value));

let calibrationActive = false;
const patternQueue = [];
let lastLoadedPattern = null;
let apiContract = null;

const fallbackReadCommandMap = Object.freeze({
  TX: 'TX',
  RANGE: 'RANGE',
  PULSE: 'PULSE',
  GAIN: 'GAIN',
  STC: 'STC',
  ACSEA: 'ACSEA',
  ACRAIN: 'ACRAIN',
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
      card.querySelectorAll('input, button, select, textarea').forEach(el => el.disabled = false);
    }
  });
}

/* ---------- POWER ---------- */
function toggleEcho(mode){
  fetch(`/toggle_echo?radarNo=${v('radarNo')}&mode=${mode}`)
    .then(r=>r.text()).then(t=>setCallout('txResult', t, 'info'));
}
function refreshTx(){
  const cmd = getReadCommand('TX');
  apiGet(`/get_current?radarNo=${v('radarNo')}&command=${cmd}`)
    .then(d=>setCallout('txResult', jsonPre(d), 'info'))
    .catch(err=>setCallout('txResult', `Failed: ${err.message}`, 'error'));
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
  fetch(`/run_pattern?steps=${encodeURIComponent(lastLoadedPattern.steps)}&repeat=${repeat}`)
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
      const helpText = `Format: range,gain,pulse,rezboost,speed,interp,repeat

range(0-21) gain(0-99) pulse(0=S1…5=L) rezboost(0=Off…3=High)
speed(0=Lo,1=Hi,2=Auto) interp(0=2048,1=4096,2=8192,3=1024,4=720,5=Off)
repeat(sweeps at this setting)

Example:
0,20,0,0,2,5,2; 0,25,1,1,2,0,3

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
    .map(item => `${item.repeat}:${btoa(item.steps)}`)
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
    .then(r=>r.text()).then(t=>{setCallout('rangeResult', t, 'info'); if(cb)cb();});
}
function setPulse(){
  fetch(`/set_pulse_width?radarNo=${v('radarNo')}&pulse=${v('pwPulse')}`
       + `&rangeIndex=${v('rangeIndex')}&rangeUnit=0`)
    .then(r=>r.text()).then(t=>setCallout('rangeResult', t, 'info'));
}
function refreshRange(){
  const cmd = getReadCommand('RANGE');
  apiGet(`/get_current?radarNo=${v('radarNo')}&command=${cmd}`)
    .then(d=>setCallout('rangeResult', jsonPre(d), 'info'))
    .catch(err=>setCallout('rangeResult', `Failed: ${err.message}`, 'error'));
}

function refreshPulse(){
  const cmd = getReadCommand('PULSE');
  apiGet(`/get_current?radarNo=${v('radarNo')}&command=${cmd}`)
    .then(d=>setCallout('rangeResult', jsonPre(d), 'info'))
    .catch(err=>setCallout('rangeResult', `Failed: ${err.message}`, 'error'));
}

/* ---------- GAIN & CLUTTER ---------- */
function setGainManual(){
  fetch(`/set_gain?radarNo=${v('radarNo')}&status=0&manualValue=${v('gainManual')}&autoValue=0`)
    .then(r=>r.text()).then(t=>setCallout('gainResult', t, 'info'));
}
function setGainAuto(){
  fetch(`/set_gain?radarNo=${v('radarNo')}&status=1&manualValue=0&autoValue=${v('gainAuto')}`)
    .then(r=>r.text()).then(t=>setCallout('gainResult', t, 'info'));
}
function setACSEA(){
  const seaMode = v('seaMode'); // manual | auto
  const status = seaMode === 'auto' ? 1 : 0;
  fetch(`/set_acsea?radarNo=${v('radarNo')}&status=${status}&manual=${v('seaMan')}&auto=${v('seaAuto')}&mode=0`)
    .then(r=>r.text()).then(t=>setCallout('clutterResult', t, 'info'));
}
function setACRAIN(){
  const rainUiMode = v('rainMode'); // manual | auto
  const status = rainUiMode === 'auto' ? 1 : 0;
  fetch(`/set_acrain?radarNo=${v('radarNo')}&status=${status}&manual=${v('rainMan')}&auto=${v('rainAuto')}&ftcMode=0&rainMode=0`)
    .then(r=>r.text()).then(t=>setCallout('clutterResult', t, 'info'));
}
function refreshGain(){
  const cmd = getReadCommand('GAIN');
  apiGet(`/get_current?radarNo=${v('radarNo')}&command=${cmd}`)
    .then(d=>setCallout('gainResult', jsonPre(d), 'info'))
    .catch(err=>setCallout('gainResult', `Failed: ${err.message}`, 'error'));
}

function refreshClutter(){
  const seaCmd = getReadCommand('ACSEA');
  const rainCmd = getReadCommand('ACRAIN');
  Promise.all([
    apiGet(`/get_current?radarNo=${v('radarNo')}&command=${seaCmd}`),
    apiGet(`/get_current?radarNo=${v('radarNo')}&command=${rainCmd}`)
  ])
    .then(([sea, rain]) => setCallout('clutterResult', jsonPre({ sea, rain }), 'info'))
    .catch(err=>setCallout('clutterResult', `Failed: ${err.message}`, 'error'));
}
function setRezBoost() {
  fetch(`/set_bearing_resolution?radarNo=${v('radarNo')}&mode=${v('rezboostMode')}`)
    .then(r => r.text())
    .then(t => setCallout('rezboostResult', t, 'info'));
}
function refreshRezBoost() {
  fetch(`/set_bearing_resolution?radarNo=${v('radarNo')}`)
    .then(r => r.text())
    .then(t => setCallout('rezboostResult', t, 'info'));
}


/* ---------- DISPLAY ---------- */
function applySweepInterp(){
  fetch(`/set_sweep_total?mode=${v('sweepMode')}`)
  .then(()=>fetch(`/set_interpolation?enable=${v('interpEnable')}`))
  .then(r=>r.text()).then(t=>setCallout('displaySweepResult', t, 'info'))
  .catch(err => setCallout('displaySweepResult', `Failed: ${err.message}`, 'error'));
}
function setAntennaRev(){
  fetch(`/set_antenna_revolution?radarNo=${v('radarNo')}&rollSpeed=${v('rollSpd')}&hiRpm=${v('hiRpm')}`)
    .then(r=>r.text()).then(t=>setCallout('antennaResult', t, 'info'))
    .catch(err => setCallout('antennaResult', `Failed: ${err.message}`, 'error'));
}
function setHighRpm(){
  fetch(`/set_antenna_revolution?radarNo=${v('radarNo')}&rollSpeed=${v('rollSpd')}&hiRpm=${v('hiRpm')}`)
    .then(r=>r.text()).then(t=>setCallout('antennaResult', t, 'info'))
    .catch(err => setCallout('antennaResult', `Failed: ${err.message}`, 'error'));
}
function refreshAntenna(){
  const cmd = getReadCommand('ANTREV');
  apiGet(`/get_current?radarNo=${v('radarNo')}&command=${cmd}`)
    .then(d=>setCallout('antennaResult', jsonPre(d), 'info'))
    .catch(err => setCallout('antennaResult', `Failed: ${err.message}`, 'error'));
}
function refreshDisplay(){
  const cmd = getReadCommand('SWEEP');
  apiGet(`/get_current?radarNo=${v('radarNo')}&command=${cmd}`)
    .then(d=>setCallout('displaySweepResult', jsonPre(d), 'info'))
    .catch(err => setCallout('displaySweepResult', `Failed: ${err.message}`, 'error'));
}


/** generic GET helper that expects JSON */
async function apiGet(url) {
  const r = await fetch(url);
  if (!r.ok) throw new Error(`GET ${url} → ${r.status}`);
  return await r.json();
}

/** Push UI fields → device */
async function setSTC() {
  const radarNo = +$('#radarNo').value;
  const nearVal = +$('#nearStc').value;
  const midVal  = +$('#midStc').value;
  const farVal  = +$('#farStc').value;
  if (Number.isNaN(radarNo)) return;

  await Promise.all([
    fetch(`/set_near_stc?radarNo=${radarNo}&value=${nearVal}`),
    fetch(`/set_middle_stc?radarNo=${radarNo}&value=${midVal}`),
    fetch(`/set_far_stc?radarNo=${radarNo}&value=${farVal}`)
  ]);

  refreshSTC();              // auto-pull after we push
}

/** Pull device → UI fields */
async function refreshSTC() {
  const radarNo = +$('#radarNo').value;
  if (Number.isNaN(radarNo)) return;
  const cmd = getReadCommand('STC');
  const payload = await apiGet(`/get_current?command=${cmd}&radarNo=${radarNo}`);
  const data = payload && payload.data ? payload.data : {};
  $('#nearStc').value = data.near ?? 0;
  $('#midStc').value  = data.middle ?? 0;
  $('#farStc').value  = data.far ?? 0;
  setCallout('gainResult', `STC updated @ ${new Date().toLocaleTimeString()}`, 'info');
}

/* tiny $ helper */
function $(sel){return document.querySelector(sel);}

renderPatternQueue();

const statusNewest = byId('statusNewestPng');
if (statusNewest) {
  fetch('/get_system_info')
    .then(r => r.json())
    .then(d => {
      statusNewest.textContent = d.newest_png || '–';
    })
    .catch(() => {
      statusNewest.textContent = 'Unavailable';
    });
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

    const cores = Array.isArray(data.cpu_cores) ? data.cpu_cores : [];
    if (cores.length) {
      renderCpuCores(cores);
    } else {
      renderCpuCores([]);
    }
    const cpu = cores.length ? clamp01(cores.reduce((sum, v) => sum + clamp01(v), 0) / cores.length) : clamp01(data.cpu_load || 0);
    setStatusBar('statusCpu', cpu);
    const cpuLabel = byId('statusCpuLabel');
    if (cpuLabel) cpuLabel.textContent = `${Math.round(cpu * 100)}%`;

    const mem = clamp01(data.memory_used || 0);
    setStatusBar('statusMemory', mem);
    const memLabel = byId('statusMemoryLabel');
    if (memLabel) memLabel.textContent = `${Math.round(mem * 100)}%`;

    const storage = clamp01(data.storage_used || 0);
    setStatusBar('statusStorage', storage);
    const storageLabel = byId('statusStorageLabel');
    if (storageLabel) {
      const path = data.storage_path ? ` (${data.storage_path})` : '';
      storageLabel.textContent = `${Math.round(storage * 100)}%${path}`;
    }

    const rx = Math.max(0, data.network_rx_mbps || 0);
    const tx = Math.max(0, data.network_tx_mbps || 0);
    const networkLabel = byId('statusNetworkLabel');
    if (networkLabel) networkLabel.textContent = `${rx.toFixed(2)} / ${tx.toFixed(2)} MB/s`;
    const networkMax = 200.0; // assume 200 MB/s peak for scaling
    const networkFraction = clamp01((rx + tx) / networkMax);
    setStatusBar('statusNetwork', networkFraction);

    const tempLabel = byId('statusTempLabel');
    if (typeof data.temperature_c === 'number') {
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
