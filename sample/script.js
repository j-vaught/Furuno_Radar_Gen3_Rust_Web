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


/* live image every 2 s */
const radarImageEl = byId('radarImage');
if (radarImageEl) {
  setInterval(() => {
    const img = new Image();
    img.onload = () => {
      if (radarImageEl) radarImageEl.src = img.src;
    };
    img.src = '/newest_png?rand=' + Date.now();
  }, 2000);
}

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
  fetch(`/get_current?radarNo=${v('radarNo')}&command=TX`)
    .then(r=>r.text()).then(t=>setCallout('txResult', t, 'info'));
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
  setCallout('patternStatus', `Added "${label}" (repeat ${repeatText}) to queue. Queue now has ${totalText}.`, 'success');
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
  fetch(`/get_current?radarNo=${v('radarNo')}&command=RANGE`)
    .then(r=>r.text()).then(t=>setCallout('rangeResult', t, 'info'));
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
  fetch(`/set_acsea?radarNo=${v('radarNo')}&status=1&manual=${v('seaMan')}&auto=${v('seaAuto')}&mode=0`)
    .then(r=>r.text()).then(t=>setCallout('gainResult', t, 'info'));
}
function setACRAIN(){
  fetch(`/set_acrain?radarNo=${v('radarNo')}&status=1&manual=${v('rainMan')}&auto=${v('rainAuto')}&ftcMode=0&rainMode=0`)
    .then(r=>r.text()).then(t=>setCallout('gainResult', t, 'info'));
}
function refreshGain(){
  fetch(`/get_current?radarNo=${v('radarNo')}&command=GAIN`)
    .then(r=>r.text()).then(t=>setCallout('gainResult', t, 'info'));
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
function refreshDisplay(){
  fetch(`/get_current?radarNo=${v('radarNo')}&command=SWEEP`)
    .then(r=>r.text()).then(t=>setCallout('displaySweepResult', t, 'info'))
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
  const data = await apiGet(`/get_current?command=STC&radarNo=${radarNo}&format=json`);
  $('#nearStc').value = data.near;
  $('#midStc').value  = data.middle;
  $('#farStc').value  = data.far;
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
  ['statusTempLabel', '--°C']
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
      if (tempLabel) tempLabel.textContent = `${data.temperature_c.toFixed(1)}°C`;
      const tempFraction = clamp01(Math.max(0, data.temperature_c) / 100.0);
      setStatusBar('statusTemp', tempFraction);
    } else {
      if (tempLabel) tempLabel.textContent = '--°C';
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
    if (tempLabel) tempLabel.textContent = '--°C';
    renderCpuCores([]);
  }
}

if (byId('statusCpu')) {
  updateSystemMetrics();
  setInterval(updateSystemMetrics, 5000);
}
