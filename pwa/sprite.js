// Sprite della PWA. La SD del firmware è la cartella /sd in memoria
// (MEMFS, pwa/hal_web.cpp) e i file di /mons arrivano su richiesta:
//   - halSdOpen() di un file che manca chiama Module.chiediFile('/mons/p025.bin');
//   - pmd.js (lo stesso convertitore dell'installer) lo cerca nella Cache Storage
//     (pokegotchi-sprite) e, se manca, scarica i PNG di quella specie da PMD SpriteCollab e
//     lo converte qui; thumbs.bin si fa dall'Idle delle 151 specie;
//   - arrivato, si scrive in /sd/mons e il firmware ricarica lo sprite (webFileArrivato).
// «Scarica tutto» converte tutti i file, per giocare offline (un'interruzione riprende da
// quelli già in cache). Il sito non ospita sprite: arrivano da SpriteCollab (CC BY-NC 4.0).
// La cache degli sprite non dipende dalla versione del gioco (sw.js la conserva agli
// aggiornamenti); pmd.js toglie le voci di un altro commit di SpriteCollab.
'use strict';

const MEMFS_MAX = 24;    // file di /mons tenuti nella SD in memoria
const NOME_SD = /^mons\/(p(s?)\d{3}|thumbs)\.bin$/;  // i file che pmd.js sa fare

let pmd = null;          // Promise del modulo
let prendi = null;       // lo scaricatore condiviso (al massimo 6 richieste insieme)
const modulo = () => (pmd ||= import('./pmd.js').then(async (m) => {
  prendi = m.scaricatore();
  await m.pulisciCache().catch(() => 0);
  return m;
}));

const inArrivo = new Set();
const mancati = new Set();  // chiesti ma non arrivati: si richiedono dopo «Scarica tutto»
const inMemoria = [];    // nomi scritti in /sd, dal più vecchio

// I byte di `nome` (es. 'mons/p025.bin'): dalla cache o convertiti. null per i file che
// la SD della PWA non ha (es. gli sprite PGK1 di SdMon).
async function prendiFile(nome) {
  if (!NOME_SD.test(nome)) return null;
  const m = await modulo();
  return m.fileConvertito(nome, { prendi });
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
  if (inArrivo.has(nome) || !NOME_SD.test(nome)) return;
  inArrivo.add(nome);
  prendiFile(nome).then((dati) => {
    if (!dati) { mancati.add(nome); return; }
    mancati.delete(nome);
    scriviInSd(nome, dati);
    Module.ccall('webFileArrivato', null, ['string'], ['/' + nome]);
  }).catch((e) => {
    mancati.add(nome);
    console.warn('sprite ' + nome, e);
  }).finally(() => inArrivo.delete(nome));
};

// ---------------------------------------------------------------- scarica tutto

// { presenti, totale } dei file già convertiti e in cache
async function statoSprite() {
  const m = await modulo();
  const presenti = await m.inCache();
  const nomi = m.nomiFile();
  return { presenti: nomi.filter((n) => presenti.has(n)).length, totale: nomi.length };
}

// Tutti gli sprite in cache; avanza(fatti, totale) in file
async function scaricaTutto(avanza) {
  const m = await modulo();
  const presenti = await m.inCache();
  const nomi = m.nomiFile();
  const manca = nomi.filter((n) => !presenti.has(n));
  const gia = nomi.length - manca.length;
  await m.convertiTutti({ nomi: manca, avanza: (fatti) => avanza(gia + fatti, nomi.length) });
}

// ---------------------------------------------------------------- pagina

const rigaOffline = document.getElementById('offline');
const bottoneTutto = document.getElementById('scarica-tutto');
const barraTutto = document.getElementById('avanzamento');

async function mostraOffline() {
  const s = await statoSprite().catch(() => null);
  rigaOffline.hidden = !s || s.presenti >= s.totale;
  if (s && s.presenti < s.totale) {
    bottoneTutto.textContent = s.presenti
      ? `Completa gli sprite per giocare offline (${s.presenti}/${s.totale})`
      : `Scarica gli sprite per giocare offline (${s.totale} file, ~40 MB)`;
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
      bottoneTutto.textContent = `Converto gli sprite… ${fatti} di ${tot}`;
    });
    document.getElementById('stato').textContent = 'Sprite pronti: il gioco funziona anche offline.';
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
