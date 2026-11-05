// Sprite PMD convertiti sul dispositivo dell'utente: l'installer (web/index.html) e la PWA
// (pwa/sprite.js) scaricano i PNG di PMD SpriteCollab (CC BY-NC 4.0) a un commit fissato e
// li trasformano qui nei file della SD, senza che il sito ne ospiti nessuno.
//
// Porting fedele di tools/pack_pmd.py (pack) e tools/make_thumbs.py (shrink, main): a parità
// di PNG i byte di p[s]NNN.bin (PGK2) e di thumbs.bin (PGTH) sono identici (TestPmdJs in
// test/test_tools.py). Modulo ES senza dipendenze: gira nel browser e in Node (>= 18).
//
//   convertiSpecie(dex, shiny, prendi)  -> Uint8Array (PGK2)
//   miniature(idle)                     -> Uint8Array (PGTH), idle[i] = PGK2 della specie i + 1
//   scaricatore()                       -> prendi(dex, shiny, file) da raw.githubusercontent.com
//   fileConvertito(nome, opz)           -> 'mons/p025.bin' | 'mons/thumbs.bin', con la cache
//   convertiTutti(opz)                  -> tutti i file, con la cache e l'avanzamento
//
// Niente canvas per leggere i PNG: con la premoltiplicazione dell'alfa i colori dei pixel
// semitrasparenti cambierebbero, e con loro la palette.

export const COMMIT = 'c163410723fbc42460f86d63f6390ad90632082d';
export const BASE = `https://raw.githubusercontent.com/PMDCollab/SpriteCollab/${COMMIT}/sprite`;
export const CREDITI = 'https://github.com/PMDCollab/SpriteCollab';
export const LICENZA = 'https://creativecommons.org/licenses/by-nc/4.0/';

const SLOW = 1.4;       // il ritmo originale di PMD sembra veloce nel tamagotchi
const MIN_MS = 70;
const ALPHA_T = 128;
const MAX_FOTOGRAMMI = 24;
const CELL = 40;        // lato delle miniature
export const N_SPECIE = 151;

// (id, nome dell'azione, riga dello sheet): direzioni 0 giù, 2 destra, 6 sinistra
const AZIONI = [
  [0, 'Idle', 0], [1, 'Walk', 6], [2, 'Walk', 2], [3, 'Sleep', 0], [4, 'Eat', 0],
  [5, 'Hurt', 0], [6, 'Attack', 0], [7, 'Pose', 0], [8, 'Hop', 0], [9, 'Nod', 0],
  [10, 'DeepBreath', 0], [11, 'Sit', 0],
];

// ---------------------------------------------------------------- PNG

const SEGNATURA = [137, 80, 78, 71, 13, 10, 26, 10];

async function inflate(dati) {
  const flusso = new Blob([dati]).stream().pipeThrough(new DecompressionStream('deflate'));
  return new Uint8Array(await new Response(flusso).arrayBuffer());
}

// Un PNG -> { w, h, rgba } come Image.open().convert('RGBA') di PIL: tipi di colore 0, 2,
// 3, 4 e 6 a 8 bit (palette anche a 1, 2 e 4 bit), tRNS compreso; niente interlacciamento.
export async function decodificaPng(b) {
  for (let i = 0; i < 8; i++) if (b[i] !== SEGNATURA[i]) throw new Error('non è un PNG');
  const dv = new DataView(b.buffer, b.byteOffset, b.byteLength);
  let p = 8, w = 0, h = 0, prof = 0, tipo = 0, intrecciato = 0, palette = null, trns = null;
  const idat = [];
  while (p + 8 <= b.length) {
    const n = dv.getUint32(p), t = String.fromCharCode(b[p + 4], b[p + 5], b[p + 6], b[p + 7]);
    const c = b.subarray(p + 8, p + 8 + n);
    if (t === 'IHDR') {
      w = dv.getUint32(p + 8); h = dv.getUint32(p + 12);
      prof = c[8]; tipo = c[9]; intrecciato = c[12];
    } else if (t === 'PLTE') palette = c;
    else if (t === 'tRNS') trns = c;
    else if (t === 'IDAT') idat.push(c);
    else if (t === 'IEND') break;
    p += 12 + n;
  }
  const canali = { 0: 1, 2: 3, 3: 1, 4: 2, 6: 4 }[tipo];
  if (!canali || intrecciato || (tipo === 3 ? ![1, 2, 4, 8].includes(prof) : prof !== 8)) {
    throw new Error(`PNG non gestito (tipo ${tipo}, ${prof} bit, interlacciato ${intrecciato})`);
  }
  let tot = 0;
  for (const c of idat) tot += c.length;
  const z = new Uint8Array(tot);
  tot = 0;
  for (const c of idat) { z.set(c, tot); tot += c.length; }
  const grezzi = await inflate(z);

  // filtri 0-4, riga per riga
  const bpp = Math.max(1, (canali * prof) >> 3), riga = (w * canali * prof + 7) >> 3;
  if (grezzi.length < h * (riga + 1)) throw new Error('PNG troncato');
  const px = new Uint8Array(h * riga);
  for (let y = 0; y < h; y++) {
    const f = grezzi[y * (riga + 1)], src = y * (riga + 1) + 1, dst = y * riga;
    for (let x = 0; x < riga; x++) {
      const a = x >= bpp ? px[dst + x - bpp] : 0;
      const su = y ? px[dst - riga + x] : 0;
      const as = x >= bpp && y ? px[dst - riga + x - bpp] : 0;
      let v = grezzi[src + x];
      if (f === 1) v += a;
      else if (f === 2) v += su;
      else if (f === 3) v += (a + su) >> 1;
      else if (f === 4) {
        const q = a + su - as, pa = Math.abs(q - a), pb = Math.abs(q - su), pc = Math.abs(q - as);
        v += pa <= pb && pa <= pc ? a : pb <= pc ? su : as;
      } else if (f !== 0) throw new Error('filtro PNG ' + f);
      px[dst + x] = v;
    }
  }

  const rgba = new Uint8Array(w * h * 4);
  for (let y = 0; y < h; y++) {
    for (let x = 0; x < w; x++) {
      const o = (y * w + x) * 4, r = y * riga;
      if (tipo === 3) {
        const bit = x * prof, i = (px[r + (bit >> 3)] >> (8 - prof - (bit & 7))) & ((1 << prof) - 1);
        rgba[o] = palette[i * 3]; rgba[o + 1] = palette[i * 3 + 1]; rgba[o + 2] = palette[i * 3 + 2];
        rgba[o + 3] = trns && i < trns.length ? trns[i] : 255;
      } else if (tipo === 6) {
        rgba.set(px.subarray(r + x * 4, r + x * 4 + 4), o);
      } else if (tipo === 2) {
        const s = r + x * 3;
        rgba[o] = px[s]; rgba[o + 1] = px[s + 1]; rgba[o + 2] = px[s + 2];
        // tRNS: il colore trasparente, su 16 bit per canale
        rgba[o + 3] = trns && trns.length >= 6 && px[s] === trns[1] && px[s + 1] === trns[3]
          && px[s + 2] === trns[5] ? 0 : 255;
      } else {
        const g = px[r + x * canali];
        rgba[o] = rgba[o + 1] = rgba[o + 2] = g;
        rgba[o + 3] = tipo === 4 ? px[r + x * 2 + 1] : trns && trns.length >= 2 && g === trns[1] ? 0 : 255;
      }
    }
  }
  return { w, h, rgba };
}

// ---------------------------------------------------------------- AnimData.xml

// Il testo di un elemento figlio <tag>…</tag>, o null
function figlio(xml, tag) {
  const m = xml.match(new RegExp(`<${tag}\\b[^>]*>([\\s\\S]*?)</${tag}>`));
  return m ? m[1] : null;
}

// AnimData.xml -> Map nome -> [larghezza, altezza, durate, nome del PNG] (o null), come
// load_animdata(): un alias CopyOf prende i dati dell'animazione a cui punta. Senza
// DOMParser, per girare anche in Node: il file è semplice e generato da SpriteCollab.
export function leggiAnimData(testo) {
  const xml = testo.replace(/<!--[\s\S]*?-->/g, '');
  const elenco = figlio(xml, 'Anims');
  if (elenco === null) throw new Error('AnimData.xml senza <Anims>');
  const anim = new Map();
  for (const m of elenco.matchAll(/<Anim\b[^>]*>([\s\S]*?)<\/Anim>/g)) {
    const a = m[1], nome = (figlio(a, 'Name') || '').trim();
    if (figlio(a, 'FrameWidth') === null) {
      const copia = figlio(a, 'CopyOf');
      if (copia !== null) anim.set(nome, ['copia', copia.trim()]);
      continue;
    }
    const durate = [...(figlio(a, 'Durations') || '').matchAll(/<Duration\b[^>]*>([\s\S]*?)<\/Duration>/g)]
      .map((d) => parseInt(d[1], 10));
    anim.set(nome, [parseInt(figlio(a, 'FrameWidth'), 10), parseInt(figlio(a, 'FrameHeight'), 10),
      durate, nome]);
  }
  // l'alias (il PNG è quello dell'animazione originale), nell'ordine del file come in Python
  for (const [k, v] of [...anim]) {
    if (v && v[0] === 'copia') anim.set(k, anim.has(v[1]) ? anim.get(v[1]) : null);
  }
  return anim;
}

// ---------------------------------------------------------------- conversione

// round() di Python: le metà vanno al pari
function arrotonda(x) {
  const f = Math.floor(x), d = x - f;
  return d > 0.5 ? f + 1 : d < 0.5 ? f : f % 2 === 0 ? f : f + 1;
}

const rgb565 = (r, g, b) => (r >> 3) << 11 | (g >> 2) << 5 | (b >> 3);

// Lo sprite PGK2 di una specie, come pack() di tools/pack_pmd.py. `prendi(file)` dà i byte
// di un file della cartella della specie (null se non c'è). `soloIdle`: solo l'azione 0,
// che basta per la miniatura (è la prima, quindi la sua palette non cambia).
export async function convertiSpecie(prendi, { soloIdle = false } = {}) {
  const xml = await prendi('AnimData.xml');
  if (!xml) throw new Error('manca AnimData.xml');
  const anim = leggiAnimData(new TextDecoder().decode(xml));
  const colmap = new Map(), pal = [], pacchi = [], png = new Map();
  for (const [aid, nome, rigaSheet] of soloIdle ? AZIONI.slice(0, 1) : AZIONI) {
    if (!anim.has(nome) || anim.get(nome) === null) continue;
    const [fw, fh, durate, sorgente] = anim.get(nome);
    if (!png.has(sorgente)) {
      const b = await prendi(`${sorgente}-Anim.png`);
      png.set(sorgente, b ? await decodificaPng(b) : null);
    }
    const im = png.get(sorgente);
    if (!im) continue;
    const righe = Math.floor(im.h / fh);
    const r = righe > rigaSheet ? rigaSheet : 0;
    const nf = Math.min(durate.length, Math.floor(im.w / fw), MAX_FOTOGRAMMI);
    const dati = new Uint8Array(fw * fh * nf);
    let k = 0;
    for (let i = 0; i < nf; i++) {
      for (let y = r * fh; y < (r + 1) * fh; y++) {
        for (let x = i * fw; x < (i + 1) * fw; x++) {
          // fuori dall'immagine il crop di PIL è trasparente
          const o = (y * im.w + x) * 4;
          if (y >= im.h || x >= im.w || im.rgba[o + 3] < ALPHA_T) { dati[k++] = 0xFF; continue; }
          const c = [im.rgba[o], im.rgba[o + 1], im.rgba[o + 2]], ch = c.join(',');
          if (!colmap.has(ch)) {
            if (pal.length >= 255) {
              // il più vicino tra quelli visti, il primo a parità (min() di Python)
              let meglio = null, dmin = Infinity;
              for (const [kc, v] of colmap) {
                const q = kc.split(',').map(Number);
                const dd = (q[0] - c[0]) ** 2 + (q[1] - c[1]) ** 2 + (q[2] - c[2]) ** 2;
                if (dd < dmin) { dmin = dd; meglio = v; }
              }
              colmap.set(ch, meglio);
            } else {
              colmap.set(ch, pal.length);
              pal.push(c);
            }
          }
          dati[k++] = colmap.get(ch);
        }
      }
    }
    const ms = durate.slice(0, nf).map((d) => Math.max(MIN_MS, arrotonda(d * 1000 / 60 * SLOW)));
    if (fw > 255 || fh > 255) throw new Error(`${nome}: fotogramma ${fw}x${fh} oltre 255`);
    pacchi.push([aid, fw, fh, nf, ms, dati]);
  }
  if (!pacchi.some((p) => p[0] === 0)) throw new Error('manca l\'azione Idle');

  let n = 7 + pal.length * 2;
  for (const [, fw, fh, nf] of pacchi) n += 4 + nf * 2 + fw * fh * nf;
  const out = new Uint8Array(n), dv = new DataView(out.buffer);
  out.set([0x50, 0x47, 0x4B, 0x32]);  // PGK2
  out[4] = pacchi.length;
  dv.setUint16(5, pal.length, true);
  let p = 7;
  for (const [r, g, b] of pal) { dv.setUint16(p, rgb565(r, g, b), true); p += 2; }
  for (const [aid, fw, fh, nf, ms, dati] of pacchi) {
    out.set([aid, fw, fh, nf], p);
    p += 4;
    for (const x of ms) { dv.setUint16(p, x, true); p += 2; }
    out.set(dati, p);
    p += dati.length;
  }
  return out;
}

// Il fotogramma 0 dell'Idle di un PGK2: [w, h, palette RGB565, indici]
export function idleFotogramma0(b) {
  const magic = String.fromCharCode(b[0], b[1], b[2], b[3]);
  if (magic !== 'PGK2' && magic !== 'TPK2') throw new Error('magic PGK2');
  const dv = new DataView(b.buffer, b.byteOffset, b.byteLength);
  const nAz = b[4], nPal = dv.getUint16(5, true), pal = [];
  for (let i = 0; i < nPal; i++) pal.push(dv.getUint16(7 + i * 2, true));
  let p = 7 + nPal * 2;
  for (let a = 0; a < nAz; a++) {
    const [aid, w, h, nf] = [b[p], b[p + 1], b[p + 2], b[p + 3]];
    p += 4 + nf * 2;
    if (aid === 0) return [w, h, pal, b.subarray(p, p + w * h)];
    p += w * h * nf;
  }
  throw new Error('manca l\'azione Idle (id 0)');
}

// shrink() di tools/make_thumbs.py: al massimo CELL x CELL, vicino più prossimo
function rimpicciolisci(w, h, pal, dati) {
  const scala = Math.min(CELL / w, CELL / h, 1.0);
  const nw = Math.max(1, arrotonda(w * scala)), nh = Math.max(1, arrotonda(h * scala));
  const out = new Uint8Array(nw * nh), usati = new Map(), nuova = [];
  let k = 0;
  for (let y = 0; y < nh; y++) {
    const sy = scala < 1 ? Math.min(h - 1, Math.trunc(y / scala)) : y;
    for (let x = 0; x < nw; x++) {
      const sx = scala < 1 ? Math.min(w - 1, Math.trunc(x / scala)) : x;
      const i = dati[sy * w + sx];
      if (i === 0xFF) { out[k++] = 0xFF; continue; }
      const c = pal[i];
      if (!usati.has(c)) { usati.set(c, nuova.length); nuova.push(c); }
      out[k++] = usati.get(c);
    }
  }
  return [nw, nh, nuova, out];
}

// thumbs.bin (PGTH) dai PGK2 delle 151 specie normali, come main() di make_thumbs.py
export function miniature(sprite) {
  if (sprite.length !== N_SPECIE) throw new Error(`servono ${N_SPECIE} sprite`);
  const blob = sprite.map((s, i) => {
    const [nw, nh, pal, dati] = rimpicciolisci(...idleFotogramma0(s));
    if (pal.length > 255) throw new Error(`${i + 1}: palette di ${pal.length} colori`);
    const b = new Uint8Array(3 + pal.length * 2 + dati.length), dv = new DataView(b.buffer);
    b.set([nw, nh, pal.length]);
    pal.forEach((c, j) => dv.setUint16(3 + j * 2, c, true));
    b.set(dati, 3 + pal.length * 2);
    return b;
  });
  const testa = 4 + 2 + 4 * N_SPECIE;
  const out = new Uint8Array(testa + blob.reduce((s, b) => s + b.length, 0));
  const dv = new DataView(out.buffer);
  out.set([0x50, 0x47, 0x54, 0x48]);  // PGTH
  dv.setUint16(4, N_SPECIE, true);
  let pos = testa;
  blob.forEach((b, i) => { dv.setUint32(6 + i * 4, pos, true); out.set(b, pos); pos += b.length; });
  return out;
}

// ---------------------------------------------------------------- rete e cache

const attesa = (ms) => new Promise((ok) => setTimeout(ok, ms));

// Un semaforo: al massimo n richieste insieme
function limitatore(n) {
  let attive = 0;
  const coda = [];
  const avanti = () => {
    while (attive < n && coda.length) {
      attive++;
      const [f, ok, ko] = coda.shift();
      f().then(ok, ko).finally(() => { attive--; avanti(); });
    }
  };
  return (f) => new Promise((ok, ko) => { coda.push([f, ok, ko]); avanti(); });
}

// prendi(dex, shiny, file) -> byte del file di SpriteCollab, null se non esiste (404).
// Al massimo `concorrenza` richieste insieme; gli errori di rete si riprovano `tentativi`
// volte con un'attesa che cresce, poi la conversione si ferma (un file che manca per un
// errore di rete darebbe uno sprite diverso da quello di pack_pmd.py).
export function scaricatore({ base = BASE, concorrenza = 6, tentativi = 4 } = {}) {
  const limita = limitatore(concorrenza);
  return (dex, shiny, file) => limita(async () => {
    const url = `${base}/${String(dex).padStart(4, '0')}${shiny ? '/0000/0001' : ''}/${file}`;
    for (let t = 0; ; t++) {
      try {
        const r = await fetch(url);
        if (r.status === 404) return null;
        if (!r.ok) throw new Error(`HTTP ${r.status}`);
        return new Uint8Array(await r.arrayBuffer());
      } catch (e) {
        if (t + 1 >= tentativi) throw new Error(`${url}: ${e.message}`);
        await attesa(500 * 2 ** t);
      }
    }
  });
}

// I nomi dei file della SD: mons/p001.bin … mons/p151.bin, gli shiny mons/ps001.bin …, e
// mons/thumbs.bin
export function nomiFile({ shiny = true, miniatura = true } = {}) {
  const nomi = [];
  for (let d = 1; d <= N_SPECIE; d++) nomi.push(`mons/p${String(d).padStart(3, '0')}.bin`);
  if (shiny) for (let d = 1; d <= N_SPECIE; d++) nomi.push(`mons/ps${String(d).padStart(3, '0')}.bin`);
  if (miniatura) nomi.push('mons/thumbs.bin');
  return nomi;
}

// Cache Storage: i file convertiti, con una chiave che contiene il commit (uno nuovo li
// rifà). La stessa cache serve all'installer e alla PWA, che hanno la stessa origine.
export const CACHE = 'pokegotchi-sprite';
const PREFISSO = `/pokegotchi-sprite/${COMMIT.slice(0, 12)}/`;
const chiave = (nome) => new URL(PREFISSO + nome, globalThis.location?.origin || 'http://localhost').href;

async function apriCache() {
  return globalThis.caches ? caches.open(CACHE) : null;
}

// Toglie dalla cache quello che non è di questo commit (sprite vecchi, l'indice di
// sprites.pak delle versioni fino alla 3.0). Torna il numero di voci tolte.
export async function pulisciCache() {
  const c = await apriCache();
  if (!c) return 0;
  let tolte = 0;
  for (const r of await c.keys()) {
    if (!new URL(r.url).pathname.startsWith(PREFISSO)) { await c.delete(r); tolte++; }
  }
  return tolte;
}

// I nomi dei file già convertiti e in cache
export async function inCache() {
  const c = await apriCache();
  if (!c) return new Set();
  const s = new Set();
  for (const r of await c.keys()) {
    const p = new URL(r.url).pathname;
    if (p.startsWith(PREFISSO)) s.add(p.slice(PREFISSO.length));
  }
  return s;
}

async function leggiCache(c, nome) {
  const r = c && await c.match(chiave(nome));
  return r ? new Uint8Array(await r.arrayBuffer()) : null;
}

async function scriviCache(c, nome, dati) {
  if (c) await c.put(chiave(nome), new Response(dati));
}

// Un file della SD ('mons/p025.bin', 'mons/ps025.bin' o 'mons/thumbs.bin'): dalla cache, o
// convertito (e messo in cache). Per thumbs.bin bastano AnimData.xml e l'Idle delle 151.
export async function fileConvertito(nome, { prendi = null, cache = true } = {}) {
  const c = cache ? await apriCache() : null;
  const pronto = await leggiCache(c, nome);
  if (pronto) return pronto;
  prendi = prendi || scaricatore();
  let dati;
  const m = nome.match(/^mons\/p(s?)(\d{3})\.bin$/);
  if (m) {
    const dex = parseInt(m[2], 10), shiny = m[1] === 's';
    if (dex < 1 || dex > N_SPECIE) throw new Error('specie ' + dex);
    dati = await convertiSpecie((f) => prendi(dex, shiny, f));
  } else if (nome === 'mons/thumbs.bin') {
    const limita = limitatore(6);  // poche specie decodificate insieme: meno memoria
    const idle = await Promise.all(Array.from({ length: N_SPECIE }, (_, i) => limita(async () => {
      const intero = await leggiCache(c, `mons/p${String(i + 1).padStart(3, '0')}.bin`);
      return intero || convertiSpecie((f) => prendi(i + 1, false, f), { soloIdle: true });
    })));
    dati = miniature(idle);
  } else {
    throw new Error('file sconosciuto: ' + nome);
  }
  await scriviCache(c, nome, dati);
  return dati;
}

// Converte (o prende dalla cache) i file di `nomi`, in parallelo: pronto(nome, dati) per
// ognuno appena c'è, avanza(fatti, totale). thumbs.bin si fa per ultimo, dagli sprite già
// convertiti. Un errore ferma tutto; quello che è già in cache resta per la volta dopo.
export async function convertiTutti({ nomi = nomiFile(), pronto = () => {}, avanza = () => {},
  concorrenza = 6 } = {}) {
  const prendi = scaricatore({ concorrenza });
  const specie = nomi.filter((n) => n !== 'mons/thumbs.bin');
  let fatti = 0;
  avanza(0, nomi.length);
  // le specie: abbastanza insieme da tenere occupate le `concorrenza` richieste
  const limita = limitatore(concorrenza);
  await Promise.all(specie.map((nome) => limita(async () => {
    const dati = await fileConvertito(nome, { prendi });
    await pronto(nome, dati);
    avanza(++fatti, nomi.length);
  })));
  if (specie.length < nomi.length) {
    const dati = await fileConvertito('mons/thumbs.bin', { prendi });
    await pronto('mons/thumbs.bin', dati);
    avanza(++fatti, nomi.length);
  }
}
