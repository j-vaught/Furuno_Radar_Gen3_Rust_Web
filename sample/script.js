/* tiny helpers */
const v=id=>document.getElementById(id).value;
const setHtml=(id,html)=>{document.getElementById(id).innerHTML=html;};
let calibrationActive = false;


/* live image every 2 s */
setInterval(()=>{
  const img=new Image();
  img.onload=()=>{document.getElementById('radarImage').src=img.src;};
  img.src='/newest_png?rand='+Date.now();
},2000);

/* info overlay */
document.getElementById('infoIcon').onclick=()=>{
  document.getElementById('infoCard').style.display='block';
  fetch('/get_system_info').then(r=>r.json()).then(d=>{
    setHtml('newestPng',d.newest_png||'–');
  });
};

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

document.getElementById('closeInfo').onclick=()=>{document.getElementById('infoCard').style.display='none';};
document.getElementById('beginCalBtn').onclick = beginCalibration;
document.getElementById('stopCalBtn').onclick = stopCalibration;


/* ---------- POWER ---------- */
function toggleEcho(mode){
  fetch(`/toggle_echo?radarNo=${v('radarNo')}&mode=${mode}`)
    .then(r=>r.text()).then(t=>setHtml('txResult',t));
}
function refreshTx(){
  fetch(`/get_current?radarNo=${v('radarNo')}&command=TX`)
    .then(r=>r.text()).then(t=>setHtml('txResult',t));
}

function beginCalibration() {
  fetch('/begin_calibration')
    .then(r => r.text())
    .then(t => setHtml('txResult', t));
  setCardCalibrationUI(true);  // disables cards
  calibrationActive = true;
}

function stopCalibration() {
  fetch('/stop_pattern')
    .then(r => r.text())
    .then(t => setHtml('txResult', t));
  setCardCalibrationUI(false); // re-enables cards
  calibrationActive = false;
}


/* ---------- Pattern ramping ---------- */
function startGainSweep() {
  const steps = v('gainSweepSteps').trim();
  const repeat = v('gainSweepRepeat');
  if (!steps) {
    setHtml('gainSweepResult', 'Please enter steps!');
    return;
  }
  setHtml('gainSweepResult', 'Starting...');
  fetch(`/run_pattern?steps=${encodeURIComponent(steps)}&repeat=${repeat}`)
    .then(r => r.text()).then(t => setHtml('gainSweepResult', t));
}

function stopGainSweep() {
  setHtml('gainSweepResult', 'Stopping...');
  fetch('/stop_pattern')
    .then(r => r.text()).then(t => setHtml('gainSweepResult', t));
}

/* ---------- RANGE / PULSE ---------- */
function applyRangePulse(){ setRange(()=>setPulse()); }
function setRange(cb){
  fetch(`/set_range?radarNo=${v('radarNo')}&rangeIndex=${v('rangeIndex')}&rangeUnit=0`)
    .then(r=>r.text()).then(t=>{setHtml('rangeResult',t); if(cb)cb();});
}
function setPulse(){
  fetch(`/set_pulse_width?radarNo=${v('radarNo')}&pulse=${v('pwPulse')}`
       + `&rangeIndex=${v('rangeIndex')}&rangeUnit=0`)
    .then(r=>r.text()).then(t=>setHtml('rangeResult',t));
}
function refreshRange(){
  fetch(`/get_current?radarNo=${v('radarNo')}&command=RANGE`)
    .then(r=>r.text()).then(t=>setHtml('rangeResult',t));
}

/* ---------- GAIN & CLUTTER ---------- */
function setGainManual(){
  fetch(`/set_gain?radarNo=${v('radarNo')}&status=0&manualValue=${v('gainManual')}&autoValue=0`)
    .then(r=>r.text()).then(t=>setHtml('gainResult',t));
}
function setGainAuto(){
  fetch(`/set_gain?radarNo=${v('radarNo')}&status=1&manualValue=0&autoValue=${v('gainAuto')}`)
    .then(r=>r.text()).then(t=>setHtml('gainResult',t));
}
function setSTC(){
  const n=v('nearStc'), m=v('midStc'), f=v('farStc'), r=v('radarNo');
  fetch(`/set_near_stc?radarNo=${r}&value=${n}`)
  .then(()=>fetch(`/set_middle_stc?radarNo=${r}&value=${m}`))
  .then(()=>fetch(`/set_far_stc?radarNo=${r}&value=${f}`))
  .then(r=>r.text()).then(t=>setHtml('gainResult',t));
}
function setACSEA(){
  fetch(`/set_acsea?radarNo=${v('radarNo')}&status=1&manual=${v('seaMan')}&auto=${v('seaAuto')}&mode=0`)
    .then(r=>r.text()).then(t=>setHtml('gainResult',t));
}
function setACRAIN(){
  fetch(`/set_acrain?radarNo=${v('radarNo')}&status=1&manual=${v('rainMan')}&auto=${v('rainAuto')}&ftcMode=0&rainMode=0`)
    .then(r=>r.text()).then(t=>setHtml('gainResult',t));
}
function refreshGain(){
  fetch(`/get_current?radarNo=${v('radarNo')}&command=GAIN`)
    .then(r=>r.text()).then(t=>setHtml('gainResult',t));
}


/* ---------- DISPLAY ---------- */
function applySweepInterp(){
  fetch(`/set_sweep_total?mode=${v('sweepMode')}`)
  .then(()=>fetch(`/set_interpolation?enable=${v('interpEnable')}`))
  .then(r=>r.text()).then(t=>setHtml('displayResult',t));
}
function setAntennaRev(){
  fetch(`/set_antenna_revolution?radarNo=${v('radarNo')}&rollSpeed=${v('rollSpd')}&hiRpm=${v('hiRpm')}`)
    .then(r=>r.text()).then(t=>setHtml('displayResult',t));
}
function refreshDisplay(){
  fetch(`/get_current?radarNo=${v('radarNo')}&command=SWEEP`)
    .then(r=>r.text()).then(t=>setHtml('displayResult',t));
}