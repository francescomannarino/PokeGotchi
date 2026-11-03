// Sprite della PWA. La SD del firmware è la cartella /sd in memoria
// (MEMFS, pwa/hal_web.cpp) e i file di /mons arrivano su richiesta:
//   - halSdOpen() di un file che manca chiama Module.chiediFile('/mons/p025.bin');
//   - il file si cerca in Cache Storage (CACHE_SPRITE); se manca si scarica con una
//     richiesta HTTP Range da sprites.pak, di cui l'indice PGAK (tools/pack_bundle.py) dà
//     gli offset; se il server ignora Range si prova il file singolo in SPRITE_DIR;
//   - arrivato, si scrive in /sd/mons e il firmware ricarica lo sprite (webFileArrivato).
// «Scarica tutto» mette in cache tutti i file, per giocare offline: con Range uno per uno
// (riprende da dove si era fermato), senza Range sprites.pak intero in streaming.
// La cache degli sprite non dipende dalla versione del gioco (sw.js la conserva agli
// aggiornamenti) e si svuota quando cambia sprites.pak (ETag o dimensione).
'use strict';

const SPRITE_PAK = '../sprites.pak';
const SPRITE_DIR = '../sprites/';    // ripiego senza Range: tools/sdcard/mons in sprites/mons/
const CACHE_SPRITE = 'pokegotchi-sprite';
const CHIAVE_INDICE = 'sprite-indice';  // l'indice PGAK, con la versione di sprites.pak
const MEMFS_MAX = 24;                // file di /mons tenuti nella SD in memoria
const PARALLELI = 4;                 // richieste insieme in «Scarica tutto»

const chiave = (nome) => 'sprite/' + nome;  // 'mons/p025.bin' -> chiave in cache

let indice = null;       // Promise di { voci: Map nome -> [offset, size], fine, versione }
let rangeOk = true;      // il server rispetta Range (si scopre alla prima richiesta)
let singoliOk = true;    // SPRITE_DIR esiste (falso dopo il primo 404)
const inArrivo = new Set();
const mancati = new Set();  // chiesti ma non arrivati: si richiedono dopo «Scarica tutto»
const inMemoria = [];    // nomi scritti in /sd, dal più vecchio

// ---------------------------------------------------------------- indice PGAK

// { voci, fine } dai primi byte di sprites.pak, null se non bastano
function leggiIndice(b) {
  if (b.length < 6) return null;
  // TPAK: il magic fino alla 2.1
  if (!['PGAK', 'TPAK'].includes(String.fromCharCode(b[0], b[1], b[2], b[3]))) throw new Error('sprites.pak non valido');
  const dv = new DataView(b.buffer, b.byteOffset, b.byteLength);
  const n = dv.getUint16(4, true), dec = new TextDecoder();
  const elenco = [];
  let p = 6;
  for (let i = 0; i < n; i++) {
    if (p >= b.length || p + 1 + b[p] + 4 > b.length) return null;
    const l = b[p];
    elenco.push([dec.decode(b.subarray(p + 1, p + 1 + l)), dv.getUint32(p + 1 + l, true)]);
    p += 1 + l + 4;
  }
  const voci = new Map();
  let off = p;  // i dati seguono l'indice, nello stesso ordine
  for (const [nome, size] of elenco) {
    voci.set(nome, [off, size]);
    off += size;
  }
  return { voci, fine: off, inizio: p };
}

// I byte [da, da + n) di sprites.pak e la versione del file. Se il server ignora Range
// (risposta 200) si leggono dallo streaming, interrotto appena bastano (rangeOk = false).
async function leggiPak(da, n) {
  const ac = new AbortController();
  const r = await fetch(SPRITE_PAK, {
    headers: { Range: `bytes=${da}-${da + n - 1}` }, signal: ac.signal,
    cache: da ? 'default' : 'no-cache',  // l'indice si riconvalida: dice la versione
  });
  if (r.status !== 206 && r.status !== 200) throw new Error('sprites.pak: HTTP ' + r.status);
  const tot = r.status === 206 ? (r.headers.get('Content-Range') || '').split('/')[1]
    : r.headers.get('Content-Length');
  const versione = (r.headers.get('ETag') || '') + '/' + (tot || '');
  if (r.status === 206) return { dati: new Uint8Array(await r.arrayBuffer()), versione };
  rangeOk = false;
  if (da > 0) { ac.abort(); return { dati: null, versione }; }
  const lettore = r.body.getReader(), pezzi = [];
  let letti = 0;
  while (letti < n) {
    const { done, value } = await lettore.read();
    if (done) break;
    pezzi.push(value);
    letti += value.length;
  }
  ac.abort();
  const dati = new Uint8Array(letti);
  letti = 0;
  for (const x of pezzi) { dati.set(x, letti); letti += x.length; }
  return { dati: dati.subarray(0, n), versione };
}

// L'indice dalla rete (e la cache svuotata se sprites.pak è cambiato), oppure quello in
// cache se si è offline. null se non c'è né l'uno né l'altro.
async function caricaIndice() {
  const cache = await caches.open(CACHE_SPRITE);
  const salvato = await cache.match(CHIAVE_INDICE);
  try {
    let n = 65536, letto;
    for (;;) {
      letto = await leggiPak(0, n);
      const ix = leggiIndice(letto.dati);
      if (ix) {
        ix.versione = letto.versione;
        if (salvato && salvato.headers.get('x-versione') !== ix.versione) {
          await caches.delete(CACHE_SPRITE);  // sprite di un altro sprites.pak
          console.log('sprites.pak cambiato: cache degli sprite svuotata');
        }
        const c = await caches.open(CACHE_SPRITE);
        await c.put(CHIAVE_INDICE, new Response(letto.dati.slice(0, ix.inizio),
          { headers: { 'x-versione': ix.versione } }));
        return ix;
      }
      if (letto.dati.length < n) throw new Error('sprites.pak troncato');
      n *= 4;
    }
  } catch (e) {
    if (!salvato) { console.warn('sprite non disponibili', e); return null; }
    const ix = leggiIndice(new Uint8Array(await salvato.arrayBuffer()));
    if (ix) ix.versione = salvato.headers.get('x-versione');
    return ix;
  }
}

function indicePronto() {
  if (!indice) {
    indice = ('caches' in window ? caricaIndice() : Promise.resolve(null)).catch((e) => {
      console.warn('indice degli sprite', e);
      return null;
    }).then((ix) => {
      if (!ix) indice = null;  // offline al primo avvio: si riprova alla prossima richiesta
      return ix;
    });
  }
  return indice;
}

// ---------------------------------------------------------------- un file

// I byte di `nome` (es. 'mons/p025.bin'): dalla cache, altrimenti dalla rete (e poi in
// cache). null se sprites.pak non lo contiene o non si riesce a scaricarlo.
async function prendiFile(nome) {
  const ix = await indicePronto();
  if (!ix || !ix.voci.has(nome)) return null;
  const cache = await caches.open(CACHE_SPRITE);
  const c = await cache.match(chiave(nome));
  if (c) return new Uint8Array(await c.arrayBuffer());
  const [off, size] = ix.voci.get(nome);
  let dati = null;
  if (rangeOk) dati = (await leggiPak(off, size)).dati;
  if (!dati && singoliOk) {
    const r = await fetch(SPRITE_DIR + nome);
    if (r.ok) dati = new Uint8Array(await r.arrayBuffer());
    else if (r.status === 404) singoliOk = false;
  }
  if (!dati || dati.length !== size) return null;
  await cache.put(chiave(nome), new Response(dati));
  return dati;
}

// scrive `nome` nella SD in memoria; i più vecchi escono (thumbs.bin resta)
function scriviInSd(nome, dati) {
  const fs = Module.FS;
  fs.mkdirTree('/sd/' + nome.slice(0, nome.lastIndexOf('/')));
  fs.writeFile('/sd/' + nome, dati);
  const i = inMemoria.indexOf(nome);
  if (i >= 0) inMemoria.splice(i, 1);
  inMemoria.push(nome);
  while (inMemoria.length > MEMFS_MAX) {
    const via = inMemoria.find((x) => !x.endsWith('thumbs.bin'));
    inMemoria.splice(inMemoria.indexOf(via), 1);
    try { fs.unlink('/sd/' + via); } catch {}
  }
}

// dal firmware (halSdOpen): `percorso` = '/mons/...'
Module.chiediFile = (percorso) => {
  const nome = percorso.replace(/^\/+/, '');
  if (inArrivo.has(nome)) return;
  inArrivo.add(nome);
  prendiFile(nome).then((dati) => {
    if (!dati) { mancati.add(nome); return; }
    mancati.delete(nome);
    scriviInSd(nome, dati);
    Module.ccall('webFileArrivato', null, ['string'], ['/' + nome]);
  }).catch((e) => console.warn('sprite ' + nome, e)).finally(() => inArrivo.delete(nome));
};

// ---------------------------------------------------------------- scarica tutto

// { presenti, totale, byte } dei file di sprites.pak già in cache
async function statoSprite() {
  const ix = await indicePronto();
  if (!ix) return null;
  const cache = await caches.open(CACHE_SPRITE);
  const chiavi = new Set((await cache.keys()).map((r) => r.url));
  let presenti = 0;
  for (const nome of ix.voci.keys()) {
    if (chiavi.has(new URL(chiave(nome), location.href).href)) presenti++;
  }
  return { presenti, totale: ix.voci.size, byte: ix.fine };
}

// Tutti gli sprite in cache; avanza(fatti, totale) in byte
async function scaricaTutto(avanza) {
  const ix = await indicePronto();
  if (!ix) throw new Error('sprites.pak non raggiungibile');
  const cache = await caches.open(CACHE_SPRITE);
  const chiavi = new Set((await cache.keys()).map((r) => r.url));
  const manca = [...ix.voci.keys()].filter((n) => !chiavi.has(new URL(chiave(n), location.href).href));
  let fatti = ix.fine - manca.reduce((s, n) => s + ix.voci.get(n)[1], 0);
  avanza(fatti, ix.fine);
  if (rangeOk) {
    // uno per uno: un'interruzione non butta quello già scaricato
    const coda = manca.slice();
    const lavora = async () => {
      while (coda.length && rangeOk) {
        const nome = coda.shift();
        if (!(await prendiFile(nome))) {
          if (!rangeOk) return;  // il server ignora Range: si passa allo streaming
          throw new Error('impossibile scaricare ' + nome);
        }
        fatti += ix.voci.get(nome)[1];
        avanza(fatti, ix.fine);
      }
    };
    await Promise.all(Array.from({ length: PARALLELI }, lavora));
    if (rangeOk) return;
  }
  // senza Range: sprites.pak intero, diviso secondo l'indice man mano che arriva
  const r = await fetch(SPRITE_PAK, { cache: 'no-cache' });
  if (!r.ok) throw new Error('sprites.pak: HTTP ' + r.status);
  const voci = [...ix.voci].sort((a, b) => a[1][0] - b[1][0]);
  const lettore = r.body.getReader();
  let pos = 0, buf = new Uint8Array(0), v = 0;
  fatti = 0;
  for (;;) {
    const { done, value } = await lettore.read();
    if (done) break;
    const unito = new Uint8Array(buf.length + value.length);
    unito.set(buf);
    unito.set(value, buf.length);
    buf = unito;
    // i file completi nel buffer, che parte dal byte `pos` di sprites.pak
    while (v < voci.length && voci[v][1][0] + voci[v][1][1] <= pos + buf.length) {
      const [nome, [off, size]] = voci[v++];
      if (off >= pos) await cache.put(chiave(nome), new Response(buf.slice(off - pos, off - pos + size)));
    }
    const taglio = v < voci.length ? Math.max(0, voci[v][1][0] - pos) : buf.length;
    buf = buf.slice(taglio);
    pos += taglio;
    fatti += value.length;
    avanza(Math.min(fatti, ix.fine), ix.fine);
  }
  if (v < voci.length) throw new Error('sprites.pak incompleto');
}

// ---------------------------------------------------------------- pagina

const rigaOffline = document.getElementById('offline');
const bottoneTutto = document.getElementById('scarica-tutto');
const barraTutto = document.getElementById('avanzamento');
const mb = (b) => (b / 1048576).toFixed(0) + ' MB';

async function mostraOffline() {
  const s = await statoSprite().catch(() => null);
  rigaOffline.hidden = !s || s.presenti >= s.totale;
  if (s && s.presenti < s.totale) {
    bottoneTutto.textContent = s.presenti
      ? `Completa gli sprite per giocare offline (${s.presenti}/${s.totale})`
      : `Scarica gli sprite per giocare offline (${mb(s.byte)})`;
  }
}

bottoneTutto.addEventListener('click', async () => {
  bottoneTutto.disabled = true;
  barraTutto.hidden = false;
  const testo = bottoneTutto.textContent;
  try {
    await scaricaTutto((fatti, tot) => {
      barraTutto.max = tot;
      barraTutto.value = fatti;
      bottoneTutto.textContent = `Scarico gli sprite… ${mb(fatti)} di ${mb(tot)}`;
    });
    document.getElementById('stato').textContent = 'Sprite scaricati: il gioco funziona anche offline.';
    for (const nome of [...mancati]) Module.chiediFile('/' + nome);
  } catch (e) {
    bottoneTutto.textContent = testo;
    document.getElementById('stato').textContent = 'Download degli sprite interrotto: ' + e.message;
  }
  bottoneTutto.disabled = false;
  barraTutto.hidden = true;
  mostraOffline();
});

if ('caches' in window) mostraOffline();
