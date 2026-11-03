// Persistenza e salvataggi della PWA.
//   - Stato: il testo della NVS dello shim (mockNvsTesto) e lo scarto dell'RTC, in
//     IndexedDB, con una copia sincrona in localStorage per la chiusura. Si legge PRIMA di caricare pokegotchi.js e si passa al firmware prima di
//     main() (webCaricaStato); il firmware lo risalva con Module.salvaStato() quando la NVS
//     cambia e quando la pagina va in secondo piano (pwa/hal_web.cpp).
//   - Salvataggi: lo stesso .pgsave della scheda (savefile.h), scritto e verificato dal
//     firmware stesso (webEsporta, webVerifica, webImporta). Esporta con Web Share sullo
//     smartphone o con un download; importa da file, da una copia automatica o dalla scheda
//     via Web Serial (comandi EXPORT/IMPORT di savesd.h, come nell'installer).
//   - Copie automatiche in IndexedDB, le ultime 3: giornaliera, aggiornamento del firmware,
//     prima di ogni importazione.
//   - Lingua: quella del browser, se il gioco non ne ha una salvata.
//   - Copie della SD in memoria (/sd/save, quelle di savesd.cpp che il gioco mostra nelle
//     impostazioni): in IndexedDB come 'sd', riscritte in MEMFS prima di main().
'use strict';

// i motivi di savefile.h (SAVE_MANUALE ... SAVE_PRIMA_IMPORT) e i loro nomi nei file
const SAVE_MANUALE = 0, SAVE_GIORNALIERA = 1, SAVE_PRIMA_IMPORT = 4;
const MOTIVI = ['manuale', 'giornaliera', 'aggiornamento', 'scambio', 'importazione'];
const MOTIVI_TESTO = ['manuale', 'giornaliera', 'aggiornamento del gioco', 'prima di uno scambio',
  "prima di un'importazione"];
const SAVE_MAX_BYTES = 8192;  // il buffer di webBuffer()
const COPIE_MAX = 3;
const PROMEMORIA_MS = 7 * 864e5;
// l'ordine dell'enum Lang di i18n.h
const LINGUE = ['es', 'en', 'fr', 'de', 'it', 'pt', 'ja', 'ko'];

const ESITI = {
  MAGIC: 'non è un file .pgsave',
  FORMATO: 'file di una versione più recente del gioco: aggiorna la pagina',
  LUNGHEZZA: 'file troncato o incompleto',
  CRC: 'file corrotto',
  SCHEMA: 'file di una versione più recente del gioco: aggiorna la pagina',
  DATI: 'contenuto del file non valido',
  COPIA: 'la copia di sicurezza non è riuscita',
  SCRITTURA: 'scrittura non riuscita',
  BASE64: 'il file non è arrivato intero, riprova',
  OCCUPATO: "c'è già un'importazione che aspetta la conferma sullo schermo",
  ANNULLATO: 'annullata sullo schermo della scheda',
};

const el = (id) => document.getElementById(id);
const pannello = el('pannello'), conferma = el('conferma');
let meta = {};          // { primoUso, esportato, giornaliera } (ms, ms, data locale)
let copie = [];         // [{ dati: Uint8Array, quando: ms }], dalla più vecchia
let inIniziale = false; // il gioco aspetta la scelta dell'iniziale
let cartellaSd = {};    // { nome: Uint8Array } di /sd/save, fino a avvioSalvataggi()

// ---------------------------------------------------------------- IndexedDB

// un solo archivio chiave-valore: 'nvs' (stato), 'copie', 'meta', 'sd' (/save della SD)
let db = null;
// Fino alla 2.1 il progetto aveva un altro nome, e con lui il database e la chiave di
// localStorage: se ci sono (stessa origine), al primo avvio si copiano nei nuovi.
const NOME_VECCHIO = 'tamapoke';
function apriDb(nome = 'pokegotchi') {
  return new Promise((ok, ko) => {
    const r = indexedDB.open(nome, 1);
    r.onupgradeneeded = () => r.result.createObjectStore('kv');
    r.onsuccess = () => ok(r.result);
    r.onerror = () => ko(r.error);
  });
}
function dbLeggi(chiave) {
  if (!db) return Promise.resolve(undefined);
  return new Promise((ok, ko) => {
    const r = db.transaction('kv').objectStore('kv').get(chiave);
    r.onsuccess = () => ok(r.result);
    r.onerror = () => ko(r.error);
  });
}
function dbScrivi(chiave, valore) {
  if (!db) return Promise.resolve();
  return new Promise((ok, ko) => {
    const t = db.transaction('kv', 'readwrite');
    t.objectStore('kv').put(valore, chiave);
    t.oncomplete = () => ok();
    t.onerror = t.onabort = () => ko(t.error);
  });
}
// Una scrittura di IndexedDB cominciata mentre la pagina si chiude (pagehide, ricarica)
// può essere interrotta; localStorage è sincrono e invece resta. All'avvio vince la copia
// più recente delle due.
const LS_STATO = 'pokegotchi-stato';
function statoLocale() {
  try { return JSON.parse(localStorage.getItem(LS_STATO)); } catch { return null; }
}
const salvaMeta = () => dbScrivi('meta', meta).catch((e) => console.warn('meta', e));

// ---------------------------------------------------------------- avvio

// il database del nome vecchio, se esiste: tutte le sue chiavi passano nel nuovo, poi lo si
// cancella. Il namespace NVS dentro lo stato lo rinomina il firmware (saveMigraNamespace).
async function migraDbVecchio() {
  const vecchioLs = localStorage.getItem(NOME_VECCHIO + '-stato');
  if (vecchioLs !== null && localStorage.getItem(LS_STATO) === null) {
    localStorage.setItem(LS_STATO, vecchioLs);
    localStorage.removeItem(NOME_VECCHIO + '-stato');
  }
  if (!indexedDB.databases || (await dbLeggi('nvs')) !== undefined) return;
  const elenco = await indexedDB.databases();
  if (!elenco.some((d) => d.name === NOME_VECCHIO)) return;
  const nuovo = db;
  db = await apriDb(NOME_VECCHIO);
  const valori = {};
  for (const k of ['nvs', 'meta', 'copie', 'sd']) valori[k] = await dbLeggi(k);
  db.close();
  db = nuovo;
  for (const [k, v] of Object.entries(valori)) if (v !== undefined) await dbScrivi(k, v);
  indexedDB.deleteDatabase(NOME_VECCHIO);
  console.log('salvataggi: partita copiata dal database del nome vecchio');
}

// prima di pokegotchi.js: lo stato deve esserci quando parte setup()
(async function avvia() {
  let stato = null;
  try {
    db = await apriDb();
    await migraDbVecchio().catch((e) => console.warn('migrazione del database', e));
    stato = await dbLeggi('nvs');
    meta = (await dbLeggi('meta')) || {};
    copie = (await dbLeggi('copie')) || [];
    cartellaSd = (await dbLeggi('sd')) || {};
  } catch (e) {
    db = null;
    console.warn('IndexedDB non disponibile', e);
  }
  const locale = statoLocale();
  if (locale && locale.nvs && (!stato || (locale.quando || 0) > (stato.quando || 0))) stato = locale;
  if (!meta.primoUso) {
    meta.primoUso = Date.now();
    salvaMeta();
  }
  Module.stato = stato || null;
  // chiede al browser di non cancellare i dati del sito quando manca spazio
  if (navigator.storage && navigator.storage.persist) {
    navigator.storage.persisted().then((si) => si || navigator.storage.persist()).catch(() => {});
  }
  const s = document.createElement('script');
  s.src = 'pokegotchi.js';
  document.body.appendChild(s);
})();

function linguaBrowser() {
  for (const l of navigator.languages || [navigator.language]) {
    const i = LINGUE.indexOf(String(l).slice(0, 2).toLowerCase());
    if (i >= 0) return i;
  }
  return -1;
}

// da onRuntimeInitialized (app.js), prima di main()
function avvioSalvataggi() {
  const dir = '/sd/save';
  Module.FS.mkdirTree(dir);
  for (const [nome, dati] of Object.entries(cartellaSd)) Module.FS.writeFile(dir + '/' + nome, dati);
  cartellaSd = null;
  const s = Module.stato || {};
  Module.ccall('webCaricaStato', null, ['string', 'number', 'string', 'number'],
    [s.nvs || '', s.rtc || 0, s.fw || '', linguaBrowser()]);
  delete Module.stato;
  // la copia di oggi, se manca: poco dopo l'avvio, a gioco già fermo sulla schermata
  setTimeout(copiaGiornaliera, 5000);
}

// chiamate dal firmware (hal_web.cpp)
Module.salvaStato = (nvs, rtc, fw, ricarica) => {
  const stato = { nvs, rtc, fw, quando: Date.now() };
  try { localStorage.setItem(LS_STATO, JSON.stringify(stato)); } catch {}
  const p = dbScrivi('nvs', stato);
  p.catch((e) => console.warn('salvataggio dello stato non riuscito', e));
  if (ricarica) {
    const via = () => location.reload();
    p.then(via, via);
    setTimeout(via, 2000);
  }
};
// /sd/save intera (pochi KB): dopo ogni copia o rotazione di savesd.cpp
Module.salvaCartella = (dir) => {
  const file = {};
  try {
    for (const nome of Module.FS.readdir(dir)) {
      if (nome !== '.' && nome !== '..') file[nome] = Module.FS.readFile(dir + '/' + nome);
    }
  } catch {}
  dbScrivi('sd', file).catch((e) => console.warn('copie della SD non salvate', e));
};
Module.copiaAutomatica = (dati) => aggiungiCopia(dati);
Module.iniziale = (si) => {
  inIniziale = !!si;
  el('primo').hidden = !inIniziale;
  aggiornaPromemoria();
};

// ---------------------------------------------------------------- .pgsave

const CRC_TAB = Array.from({ length: 256 }, (_, n) => {
  let c = n;
  for (let k = 0; k < 8; k++) c = c & 1 ? 0xEDB88320 ^ (c >>> 1) : c >>> 1;
  return c >>> 0;
});
function crc32(b) {
  let c = 0xFFFFFFFF;
  for (const x of b) c = CRC_TAB[(c ^ x) & 0xFF] ^ (c >>> 8);
  return (c ^ 0xFFFFFFFF) >>> 0;
}

// intestazione e riepilogo di un .pgsave intero; lancia un errore con il motivo. Il resto
// (schema, corpo) lo controlla il firmware con webVerifica().
function leggiSalvataggio(b) {
  const dv = new DataView(b.buffer, b.byteOffset, b.byteLength);
  const cstr = (o, n) => new TextDecoder().decode(b.subarray(o, o + n)).split('\0')[0];
  if (b.length < 52 || !['PGSV', 'TPSV'].includes(cstr(0, 4))) throw new Error(ESITI.MAGIC);
  if (dv.getUint32(12, true) !== b.length) throw new Error(ESITI.LUNGHEZZA);
  if (crc32(b.subarray(0, b.length - 4)) !== dv.getUint32(b.length - 4, true)) throw new Error(ESITI.CRC);
  return {
    piattaforma: dv.getUint8(5), motivo: dv.getUint8(6), epoch: dv.getUint32(16, true),
    fw: cstr(20, 12), specie: dv.getInt16(32, true), livello: dv.getUint16(34, true),
    shiny: dv.getUint8(36) === 1, nome: cstr(40, 12),
  };
}

function nomeSpecie(dex) {
  try { return Module.UTF8ToString(Module._webNomeSpecie(dex)); } catch { return '#' + dex; }
}
const descrivi = (h) => (h.specie < 0 ? 'uovo' : nomeSpecie(h.specie) + (h.nome ? ' «' + h.nome + '»' : ''))
  + (h.shiny ? ' shiny' : '') + (h.specie < 0 ? '' : ' Lv ' + h.livello);
const dataOra = (ms) => new Date(ms).toLocaleString(undefined, { dateStyle: 'medium', timeStyle: 'short' });
// l'epoch di un salvataggio è in ora locale (l'RTC della scheda e del gioco): si mostra com'è
const dataOraEpoch = (e) => new Date(e * 1000).toLocaleString(undefined, { dateStyle: 'medium', timeStyle: 'short', timeZone: 'UTC' });

// nome come quello delle copie sulla SD: AAAAMMGG-HHMMSS-<motivo>.pgsave, nell'ora locale
// dell'epoch del salvataggio (letta con i metodi UTC)
function nomeFile(h) {
  const d = new Date(h.epoch ? h.epoch * 1000 : Date.now() - new Date().getTimezoneOffset() * 60000);
  const p = (n) => String(n).padStart(2, '0');
  return `${d.getUTCFullYear()}${p(d.getUTCMonth() + 1)}${p(d.getUTCDate())}-`
    + `${p(d.getUTCHours())}${p(d.getUTCMinutes())}${p(d.getUTCSeconds())}-`
    + `${MOTIVI[h.motivo] || 'manuale'}.pgsave`;
}

// il salvataggio attuale, scritto dal firmware; null se non riesce
function esportaDati(motivo) {
  if (!pronto) return null;
  const n = Module._webEsporta(motivo);
  return n > 0 ? Module.HEAPU8.slice(Module._webBuffer(), Module._webBuffer() + n) : null;
}

const nomeEsito = (r) => Module.UTF8ToString(Module._webEsitoNome(r));

// ---------------------------------------------------------------- copie automatiche

async function aggiungiCopia(dati) {
  copie.push({ dati, quando: Date.now() });
  while (copie.length > COPIE_MAX) copie.shift();
  try { await dbScrivi('copie', copie); } catch (e) { console.warn('copia non salvata', e); }
  if (pannello.open) mostraCopie();
}

// una al giorno: all'avvio o quando la pagina va in secondo piano
function copiaGiornaliera() {
  const oggi = new Date().toDateString();
  if (!pronto || inIniziale || meta.giornaliera === oggi) return;
  const dati = esportaDati(SAVE_GIORNALIERA);
  if (!dati) return;
  meta.giornaliera = oggi;
  salvaMeta();
  aggiungiCopia(dati);
}
document.addEventListener('visibilitychange', () => {
  if (document.visibilityState === 'hidden') copiaGiornaliera();
});

function mostraCopie() {
  const ul = el('copie');
  ul.replaceChildren();
  if (!copie.length) {
    const li = document.createElement('li');
    li.className = 'vuoto';
    li.textContent = 'Ancora nessuna: la prima arriva entro oggi.';
    ul.append(li);
    return;
  }
  for (const c of copie.slice().reverse()) {
    let h;
    try { h = leggiSalvataggio(c.dati); } catch { continue; }
    const li = document.createElement('li');
    const testo = document.createElement('span');
    testo.innerHTML = '<b></b><br><small></small>';
    testo.querySelector('b').textContent = descrivi(h);
    testo.querySelector('small').textContent = `${dataOra(c.quando)} · ${MOTIVI_TESTO[h.motivo] || ''}`;
    const b = document.createElement('button');
    b.type = 'button';
    b.textContent = 'Ripristina';
    b.onclick = () => importa(c.dati, 'Copia');
    li.append(testo, b);
    ul.append(li);
  }
}

// ---------------------------------------------------------------- promemoria

const suIos = /iPad|iPhone|iPod/.test(navigator.userAgent)
  || (navigator.platform === 'MacIntel' && navigator.maxTouchPoints > 1);
const installata = () => navigator.standalone === true
  || matchMedia('(display-mode: standalone)').matches;

function aggiornaPromemoria() {
  const da = meta.esportato || meta.primoUso || Date.now();
  const giorni = Math.floor((Date.now() - da) / 864e5);
  const scaduto = !inIniziale && Date.now() - da > PROMEMORIA_MS;
  el('promemoria').hidden = !scaduto;
  el('salva').title = scaduto ? `Salvataggi: nessuna esportazione da ${giorni} giorni` : 'Salvataggi';
  el('ultima').textContent = meta.esportato
    ? 'Ultima esportazione: ' + dataOra(meta.esportato) + '.'
    : 'Non hai ancora esportato un salvataggio.';
  const avviso = el('avviso');
  avviso.hidden = !scaduto;
  if (scaduto) {
    avviso.textContent = `Sono passati ${giorni} giorni: esporta un salvataggio e tienilo da parte.`
      + (suIos && !installata()
        ? ' Su iPhone e iPad Safari può cancellare i dati di un sito non installato: '
          + 'aggiungi PokeGotchi alla schermata Home.'
        : '');
  }
}

// ---------------------------------------------------------------- esporta e importa

// nel pannello, oppure sotto lo schermo (l'importazione accanto alla scelta dell'iniziale)
const esito = (t) => { el(pannello.open ? 'esito' : 'stato').textContent = t; };

function scarica(dati, nome) {
  const a = document.createElement('a');
  a.href = URL.createObjectURL(new Blob([dati], { type: 'application/octet-stream' }));
  a.download = nome;
  a.click();
  setTimeout(() => URL.revokeObjectURL(a.href), 1000);
}

function segnaEsportato() {
  meta.esportato = Date.now();
  salvaMeta();
  aggiornaPromemoria();
}

async function esporta() {
  const dati = esportaDati(SAVE_MANUALE);
  if (!dati) { esito('Esportazione non riuscita.'); return; }
  const h = leggiSalvataggio(dati);
  const nome = nomeFile(h);
  // sullo smartphone il foglio di condivisione (File, Drive, inviarlo a sé stessi...)
  const file = new File([dati], nome, { type: 'application/octet-stream' });
  if (matchMedia('(pointer: coarse)').matches && navigator.canShare && navigator.canShare({ files: [file] })) {
    try {
      await navigator.share({ files: [file], title: 'PokeGotchi: ' + descrivi(h) });
      segnaEsportato();
      esito(`Condiviso ${nome}.`);
      return;
    } catch (e) {
      if (e.name === 'AbortError') return;  // chiuso senza scegliere
      // altrimenti si scarica
    }
  }
  scarica(dati, nome);
  segnaEsportato();
  esito(`Scaricato ${nome}: ${descrivi(h)}.`);
}

// finestra di conferma; true = importa
function chiediConferma(testo) {
  return new Promise((ok) => {
    el('conferma-testo').textContent = testo;
    conferma.returnValue = '';
    conferma.onclose = () => ok(conferma.returnValue === 'si');
    conferma.showModal();
  });
}

// Importa `dati` (Uint8Array): verifica, conferma, copia dello stato attuale, scrittura e
// ricaricamento. `da` è l'origine, per i messaggi. false se non è stato importato.
async function importa(dati, da) {
  if (!pronto) return false;
  let h;
  try {
    if (dati.length > SAVE_MAX_BYTES) throw new Error(ESITI.LUNGHEZZA);
    h = leggiSalvataggio(dati);
  } catch (e) { esito(`${da}: ${e.message}.`); return false; }
  const buf = Module._webBuffer();
  Module.HEAPU8.set(dati, buf);
  let r = Module._webVerifica(dati.length);
  if (r) { esito(`${da}: ${ESITI[nomeEsito(r)] || nomeEsito(r)}.`); return false; }
  const quando = h.epoch ? dataOraEpoch(h.epoch) : 'data sconosciuta';
  const primo = Module._webPrimoAvvio();
  if (!await chiediConferma(`Importare ${descrivi(h)} (${quando}, ${h.piattaforma ? 'browser' : 'scheda'} `
    + `${h.fw || '?'})?` + (primo ? '' : ' La partita attuale resta tra le copie automatiche.'))) return false;
  // la copia di sicurezza prima di sovrascrivere (non al primo avvio: non c'è nulla)
  if (!primo) {
    const ora = esportaDati(SAVE_PRIMA_IMPORT);
    if (!ora) { esito(`${da}: ${ESITI.COPIA}.`); return false; }
    await aggiungiCopia(ora);
  }
  Module.HEAPU8.set(dati, buf);  // webEsporta ha usato lo stesso buffer
  r = Module._webImporta(dati.length);
  if (r) { esito(`${da}: ${ESITI[nomeEsito(r)] || nomeEsito(r)}.`); return false; }
  esito('Importato: il gioco si riavvia…');
  return true;  // hal_web.cpp salva lo stato e ricarica la pagina
}

const sceltaFile = el('file-salvataggio');
sceltaFile.onchange = async () => {
  const f = sceltaFile.files[0];
  sceltaFile.value = '';
  if (!f) return;
  if (f.size > SAVE_MAX_BYTES) { esito(`${f.name}: ${ESITI.LUNGHEZZA}.`); return; }
  await importa(new Uint8Array(await f.arrayBuffer()), f.name);
};

// ---------------------------------------------------------------- scheda via Web Serial
// Gli stessi comandi dell'installer (web/index.html): EXPORT e IMPORT <base64>.

let porta, lettore, scrittore, righe = '', letturaSospesa = null;
const dec = new TextDecoder(), enc = new TextEncoder();

async function collega() {
  porta = await navigator.serial.requestPort();
  await porta.open({ baudRate: 115200 });
  lettore = porta.readable.getReader();
  scrittore = porta.writable.getWriter();
  righe = '';
  letturaSospesa = null;
}
async function scollega() {
  try { await lettore.cancel(); } catch {}
  try { lettore.releaseLock(); scrittore.releaseLock(); await porta.close(); } catch {}
  porta = lettore = scrittore = null;
  letturaSospesa = null;
}

// una riga dalla scheda, o null dopo `ms`. La lettura rimasta in sospeso si riusa.
async function leggiRiga(ms) {
  const fine = Date.now() + ms;
  while (true) {
    const nl = righe.indexOf('\n');
    if (nl >= 0) { const l = righe.slice(0, nl).trim(); righe = righe.slice(nl + 1); return l; }
    const resta = fine - Date.now();
    if (resta <= 0) return null;
    if (!letturaSospesa) letturaSospesa = lettore.read();
    const r = await Promise.race([letturaSospesa, new Promise((ok) => setTimeout(() => ok(null), resta))]);
    if (r === null) return null;
    letturaSospesa = null;
    if (r.done) return null;
    righe += dec.decode(r.value, { stream: true });
  }
}

async function dallaScheda() {
  try { await collega(); } catch (e) { esito('Scheda non collegata: ' + e.message); return; }
  esito('Chiedo il salvataggio alla scheda…');
  await scrittore.write(enc.encode('EXPORT\n'));
  let dati = null;
  const fine = Date.now() + 10000;
  while (Date.now() < fine) {
    const l = await leggiRiga(fine - Date.now());
    if (l === null || l === 'ERR' || (dati && l === 'DONE')) break;
    // la risposta è una riga base64 che comincia con "PGSV" (UEdTV; "TPSV" = VFBTV fino
    // alla 2.1)
    if (!dati && (l.startsWith('UEdTV') || l.startsWith('VFBTV')) && /^[A-Za-z0-9+/]+=*$/.test(l)) {
      try { dati = Uint8Array.from(atob(l), (c) => c.charCodeAt(0)); } catch {}
    }
  }
  await scollega();
  if (!dati) { esito('La scheda non ha risposto: serve il firmware 1.18 o successivo.'); return; }
  esito('');
  await importa(dati, 'Dalla scheda');
}

async function sullaScheda() {
  const dati = esportaDati(SAVE_MANUALE);
  if (!dati) { esito('Esportazione non riuscita.'); return; }
  try { await collega(); } catch (e) { esito('Scheda non collegata: ' + e.message); return; }
  let b64 = '';
  for (const x of dati) b64 += String.fromCharCode(x);
  await scrittore.write(enc.encode('IMPORT ' + btoa(b64) + '\n'));
  let inAttesa = false, fatto = 'La scheda non ha risposto: serve il firmware 1.18 o successivo.';
  const fine = Date.now() + 180000;
  while (Date.now() < fine) {
    const l = await leggiRiga(inAttesa ? fine - Date.now() : 10000);
    if (l === null) { if (inAttesa) fatto = 'Nessuna conferma sullo schermo della scheda: riprova.'; break; }
    if (l.startsWith('CONFERMA') && !inAttesa) {
      inAttesa = true;
      esito('Conferma sullo schermo della scheda (SÌ per ripristinare).');
    } else if (l === 'DONE' && inAttesa) {
      fatto = 'Fatto: la scheda si riavvia con questa partita.';
      break;
    } else if (l.startsWith('ERR')) {
      const codice = l.slice(3).trim();
      fatto = 'Non portato sulla scheda: ' + (ESITI[codice] || codice || 'errore') + '.';
      break;
    }
  }
  await scollega();
  esito(fatto);
}

// ---------------------------------------------------------------- pannello

function apriPannello() {
  sbloccaAudio();
  esito('');
  mostraCopie();
  aggiornaPromemoria();
  pannello.showModal();
}

// un'operazione alla volta: i pulsanti restano spenti finché non finisce
function pulsante(id, fn) {
  el(id).onclick = async () => {
    const tutti = pannello.querySelectorAll('.azioni button');
    tutti.forEach((b) => b.disabled = true);
    try { await fn(); } catch (e) { esito('Errore: ' + e.message); }
    tutti.forEach((b) => b.disabled = false);
  };
}

el('salva').addEventListener('click', apriPannello);
el('importa-primo').addEventListener('click', () => { sbloccaAudio(); sceltaFile.click(); });
pulsante('esporta', esporta);
el('importa').onclick = () => sceltaFile.click();
if ('serial' in navigator) {
  el('scheda').hidden = false;
  pulsante('dalla-scheda', dallaScheda);
  pulsante('sulla-scheda', sullaScheda);
}
aggiornaPromemoria();
