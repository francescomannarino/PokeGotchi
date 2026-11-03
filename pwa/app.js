// Glue della PWA: collega il firmware in WebAssembly (pokegotchi.js, Emscripten)
// alla pagina. Il C++ (pwa/hal_web.cpp) chiama Module.disegna(); qui si passano a lui
// pointer, tastiera e visibilità con le funzioni esportate _web*, e si suona il suo audio.
//   mouse/dito = touch | frecce = scorrimenti | P = PWR | S = screenshot PNG
//   DeviceMotion = sensore di movimento (su iOS col permesso chiesto al primo tocco)
// Dalla console del browser: pokegotchi.seriale('STATS') (la risposta esce nel log); con
// ?debug nell'indirizzo c'è anche un pannello con la console seriale (handleSerial).
// Persistenza, salvataggi e lingua sono in salvataggi.js, che carica pokegotchi.js; gli
// sprite in sprite.js, il worklet audio in audio.js. Qui anche la registrazione del
// service worker (sw.js).
'use strict';

const LATO = 466;
const tela = document.getElementById('schermo');
const ctx2d = tela.getContext('2d');
const immagine = ctx2d.createImageData(LATO, LATO);
const stato = document.getElementById('stato');
let pronto = false;
const DEBUG = new URLSearchParams(location.search).has('debug');

// ---------------------------------------------------------------- dimensioni

// Il canvas resta 466x466: lo scala il CSS. Quando c'è spazio il lato è un multiplo
// intero dei pixel del dispositivo (devicePixelRatio), così i pixel restano netti.
const QUOTA = 0.76;  // lato dello schermo / lato della cornice
function dimensiona() {
  const dpr = window.devicePixelRatio || 1;
  // le righe sotto la palla (importa, sprite offline, aggiornamento) tolgono altezza
  let righe = 0;
  for (const r of document.querySelectorAll('#scena > p:not(#stato)')) {
    if (!r.hidden) righe += r.offsetHeight + 12;
  }
  // la console di ?debug sta in basso, fissa: la scena le lascia il posto, se no copriva i
  // pulsanti sotto la palla
  const debug = document.getElementById('debug');
  const sotto = debug.hidden ? 0 : debug.offsetHeight + 8;
  document.getElementById('scena').style.paddingBottom =
    sotto ? `calc(${sotto}px + env(safe-area-inset-bottom))` : '';
  const spazio = Math.min(window.innerWidth - 32, window.innerHeight - 80 - righe - sotto);
  const palla = Math.max(200, spazio);
  let lato = palla * QUOTA;
  // il multiplo intero solo se non rimpicciolisce troppo; se no almeno pixel interi
  const k = Math.floor(lato * dpr / LATO);
  if (k >= 1 && k * LATO >= 0.85 * lato * dpr) lato = k * LATO / dpr;
  else lato = Math.floor(lato * dpr) / dpr;
  const radice = document.documentElement.style;
  radice.setProperty('--tela', lato + 'px');
  radice.setProperty('--palla', lato / QUOTA + 'px');
}
window.addEventListener('resize', dimensiona);
new MutationObserver(dimensiona).observe(document.getElementById('scena'),
  { subtree: true, attributeFilter: ['hidden'] });
dimensiona();

// ---------------------------------------------------------------- audio

// Musica ed effetti li produce il motore di musica.h nel WASM (pwa/hal_web.cpp); qui ci
// sono il contesto audio e il worklet di audio.js che li suona. Il contesto si apre al primo
// gesto dell'utente (lo vogliono i browser), alla sua frequenza. Il worklet ogni ~20 ms dice
// quanti campioni ha suonato, e qui si riporta la coda a CODA secondi chiamando
// _webAudioRiempi(): la produzione segue il clock dell'audio, non i fotogrammi. Con la pagina
// nascosta il contesto si sospende (visibilita()).
const CODA = 0.1;    // secondi in coda: coprono un fotogramma lento del thread principale
let audio = null;
let nodo = null;     // l'AudioWorkletNode, quando worklet e motore sono pronti
let riserva = null;  // lo ScriptProcessorNode di apriRiserva(): tenuto, se no il GC lo ferma
let prodotti = 0;    // campioni mandati al worklet (o alla riserva)
const diagAudio = { consumati: 0, vuoti: 0, picco: 0 };  // l'ultimo rapporto del worklet

function sbloccaAudio() {
  const AC = window.AudioContext || window.webkitAudioContext;
  if (!AC || !pronto) return;
  if (!audio) {
    audio = new AC();
    // musicaInizia() vuole 16..48 kHz: un contesto a 88,2 o 96 kHz si riapre a 48 kHz
    if (audio.sampleRate < 16000 || audio.sampleRate > 48000) {
      audio.close();
      audio = new AC({ sampleRate: 48000 });
    }
    audio.onstatechange = () => Module._webAudioAcceso(audio.state === 'running');
    if (!audio.audioWorklet) {
      apriRiserva();
      return;
    }
    audio.audioWorklet.addModule('audio.js').then(() => {
      if (!Module._webAudioApri(audio.sampleRate)) throw new Error('frequenza ' + audio.sampleRate);
      const n = new AudioWorkletNode(audio, 'pokegotchi-audio',
        { numberOfInputs: 0, outputChannelCount: [1] });
      n.port.onmessage = (e) => { Object.assign(diagAudio, e.data); riempiAudio(); };
      n.connect(audio.destination);
      nodo = n;
      Module._webAudioAcceso(audio.state === 'running');
    }).catch((e) => console.warn('audio non disponibile', e));
  }
  // anche 'interrupted' (iOS, dopo una telefonata o un'altra app): riparte al tocco
  if (audio.state !== 'running' && document.visibilityState === 'visible') audio.resume();
}

// Senza AudioWorklet (pagina non sicura, come http://<IP del PC> dal telefono, o un browser
// vecchio) la riserva è il vecchio ScriptProcessorNode: la sua callback gira sul thread
// principale e chiede i campioni direttamente, con più latenza e qualche scatto in più.
function apriRiserva() {
  if (!audio.createScriptProcessor || !Module._webAudioApri(audio.sampleRate)) {
    console.warn('audio non disponibile a ' + audio.sampleRate + ' Hz');
    return;
  }
  const n = audio.createScriptProcessor(2048, 0, 1);
  n.onaudioprocess = (e) => {
    const out = e.outputBuffer.getChannelData(0);
    out.set(new Float32Array(Module.HEAPU8.buffer, Module._webAudioRiempi(out.length), out.length));
    prodotti += out.length;
    diagAudio.consumati = prodotti;
    for (const v of out) diagAudio.picco = Math.max(diagAudio.picco, Math.abs(v));
  };
  n.connect(audio.destination);
  riserva = n;
  Module._webAudioAcceso(audio.state === 'running');
}

function riempiAudio() {
  if (!nodo) return;
  let manca = Math.round(CODA * audio.sampleRate) - (prodotti - diagAudio.consumati);
  while (manca > 0) {
    const n = Math.min(manca, 4096);
    const p = Module._webAudioRiempi(n);
    const blocco = new Float32Array(Module.HEAPU8.buffer, p, n).slice();
    nodo.port.postMessage(blocco, [blocco.buffer]);
    prodotti += n;
    manca -= n;
  }
}

// ---------------------------------------------------------------- modulo Emscripten

var Module = {
  disegna(px) {
    immagine.data.set(px);
    ctx2d.putImageData(immagine, 0, 0);
  },
  print: (t) => { console.log(t); if (DEBUG) rigaDebug(t); },
  printErr: (t) => console.warn(t),
  onRuntimeInitialized() {
    avvioSalvataggi();  // lo stato di IndexedDB, prima di main()
    pronto = true;
    stato.textContent = '';
  },
  onAbort(e) {
    stato.textContent = 'Errore: ' + e;
  },
};

window.pokegotchi = {
  seriale(riga) { if (pronto) Module.ccall('webSeriale', null, ['string'], [String(riga)]); },
  // stato dell'audio (prove): picco = il massimo |campione| suonato, vuoti = campioni di silenzio
  audio: () => ({ stato: audio ? audio.state : 'chiuso', fs: audio ? audio.sampleRate : 0,
                  prodotti, ...diagAudio }),
};

// ---------------------------------------------------------------- touch

function coordinate(e) {
  const r = tela.getBoundingClientRect();
  const x = Math.round((e.clientX - r.left) * LATO / r.width);
  const y = Math.round((e.clientY - r.top) * LATO / r.height);
  return [Math.min(LATO - 1, Math.max(0, x)), Math.min(LATO - 1, Math.max(0, y))];
}

let dito = null;  // pointerId del dito giù
tela.addEventListener('pointerdown', (e) => {
  sbloccaAudio();
  if (!pronto || !e.isPrimary || dito !== null) return;
  dito = e.pointerId;
  tela.setPointerCapture(e.pointerId);
  Module._webDito(1, ...coordinate(e));
  e.preventDefault();
});
tela.addEventListener('pointermove', (e) => {
  if (e.pointerId === dito) Module._webDito(1, ...coordinate(e));
});
function su(e) {
  chiediMoto();  // pointerup è un gesto anche per iOS, pointerdown non sempre
  if (e.pointerId !== dito) return;
  dito = null;
  Module._webDito(0, ...coordinate(e));
}
tela.addEventListener('pointerup', su);
tela.addEventListener('pointercancel', su);
tela.addEventListener('contextmenu', (e) => e.preventDefault());

// ---------------------------------------------------------------- movimento

// accelerationIncludingGravity va al firmware con l'angolo dello schermo (pwa/hal_web.cpp,
// webMoto). Il listener c'è da subito; dove esiste DeviceMotionEvent.requestPermission()
// (iOS 13+, e ora anche Chromium) gli eventi arrivano solo dopo il permesso, chiesto dentro
// un gesto: al primo tocco, e se il gesto non vale (errore) al successivo. Negato, sul
// desktop o in http non arrivano campioni: halMotoPresente() resta falso e si gioca col
// joystick.
let motoChiesto = false;
window.addEventListener('devicemotion', (e) => {
  const a = e.accelerationIncludingGravity;
  if (!pronto || !a || a.x == null || a.y == null || a.z == null) return;
  const o = screen.orientation;
  const angolo = (o && typeof o.angle === 'number' ? o.angle : window.orientation) || 0;
  Module._webMoto(a.x, a.y, a.z, angolo);
});
function chiediMoto() {
  const DME = window.DeviceMotionEvent;
  if (motoChiesto || !DME || typeof DME.requestPermission !== 'function') return;
  motoChiesto = true;
  DME.requestPermission().then((esito) => {
    if (esito !== 'granted') console.info('movimento: permesso ' + esito);
  }).catch((e) => {
    motoChiesto = false;  // fuori da un gesto: si riprova al prossimo tocco
    console.info('movimento: ' + e);
  });
}

// ---------------------------------------------------------------- PWR e tastiera

document.getElementById('pwr').addEventListener('click', () => {
  sbloccaAudio();
  chiediMoto();
  if (pronto) Module._webPwr();
});

let foto = 0;
function screenshot() {
  tela.toBlob((b) => {
    const a = document.createElement('a');
    a.href = URL.createObjectURL(b);
    a.download = 'pokegotchi-' + String(++foto).padStart(3, '0') + '.png';
    a.click();
    setTimeout(() => URL.revokeObjectURL(a.href), 1000);
  });
}

const SCORRI = { ArrowLeft: [-1, 0], ArrowRight: [1, 0], ArrowUp: [0, -1], ArrowDown: [0, 1] };
window.addEventListener('keydown', (e) => {
  if (document.querySelector('dialog[open]')) return;  // il pannello dei salvataggi
  if (e.target instanceof HTMLInputElement) return;    // la riga della console di debug
  sbloccaAudio();
  if (!pronto || e.repeat || e.ctrlKey || e.metaKey || e.altKey) return;
  if (SCORRI[e.key]) Module._webScorri(...SCORRI[e.key]);
  else if (e.key === 'p' || e.key === 'P') Module._webPwr();
  else if (e.key === 's' || e.key === 'S') screenshot();
  else return;
  e.preventDefault();
});

// ---------------------------------------------------------------- primo piano

// Nascosta, il browser ferma il ciclo: il firmware annota l'ora e al ritorno applica il
// tempo passato. pagehide copre la chiusura (non beforeunload).
function visibilita() {
  if (!pronto) return;
  if (document.visibilityState === 'hidden') {
    Module._webNascosta();
    if (audio) audio.suspend();
  } else {
    Module._webVisibile();
    if (audio && audio.state === 'suspended') audio.resume();
  }
}
document.addEventListener('visibilitychange', visibilita);
window.addEventListener('pagehide', () => { if (pronto) Module._webNascosta(); });
window.addEventListener('pageshow', visibilita);

// ---------------------------------------------------------------- console di debug

// ?debug: le righe del firmware (Serial) e una riga di comando verso handleSerial()
const logDebug = document.getElementById('debug-log');
function rigaDebug(t) {
  const giu = logDebug.scrollTop + logDebug.clientHeight >= logDebug.scrollHeight - 4;
  logDebug.append(t + '\n');
  while (logDebug.childNodes.length > 500) logDebug.firstChild.remove();
  if (giu) logDebug.scrollTop = logDebug.scrollHeight;
}
if (DEBUG) {
  document.getElementById('debug').hidden = false;
  dimensiona();
  document.getElementById('debug-riga').addEventListener('submit', (e) => {
    e.preventDefault();
    const campo = document.getElementById('debug-testo');
    const riga = campo.value.trim();
    if (!riga) return;
    rigaDebug('> ' + riga);
    pokegotchi.seriale(riga);
    campo.value = '';
  });
}

// ---------------------------------------------------------------- service worker

// sw.js tiene la pagina e il gioco per l'offline (gli sprite li mette in cache sprite.js).
// Alla prima installazione la pagina NON si ricarica (si perderebbe la scelta
// dell'iniziale): un aggiornamento aspetta il pulsante «Aggiorna», poi la nuova versione
// prende il controllo e solo allora si ricarica. Al ritorno in primo piano si cerca una
// versione nuova (reg.update()).
if ('serviceWorker' in navigator) {
  let aggiornando = false;
  const offri = (sw) => {
    if (!sw || !navigator.serviceWorker.controller) return;  // prima installazione
    const riga = document.getElementById('aggiorna');
    riga.hidden = false;
    document.getElementById('aggiorna-ora').onclick = () => {
      aggiornando = true;
      riga.hidden = true;
      sw.postMessage('attiva');
    };
  };
  navigator.serviceWorker.addEventListener('controllerchange', () => {
    if (aggiornando) location.reload();  // pagehide salva lo stato prima
  });
  navigator.serviceWorker.register('sw.js').then((reg) => {
    offri(reg.waiting);
    reg.addEventListener('updatefound', () => {
      const sw = reg.installing;
      sw.addEventListener('statechange', () => { if (sw.state === 'installed') offri(sw); });
    });
    document.addEventListener('visibilitychange', () => {
      if (document.visibilityState === 'visible') reg.update().catch(() => {});
    });
  }).catch((e) => console.warn('service worker non registrato', e));
}
