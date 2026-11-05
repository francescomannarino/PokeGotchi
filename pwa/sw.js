// Service worker della PWA.
//   - Precache della shell (pagina, script e worklet audio, wasm, manifest, icone) in una
//     cache il cui nome contiene FW_VERSION e l'impronta della build: tools/build_pwa.sh
//     sostituisce @VERSIONE@. Una build nuova installa una cache nuova e, attivata,
//     cancella le vecchie.
//   - Gli sprite li converte pmd.js (cache 'pokegotchi-sprite', a runtime) dai PNG di PMD
//     SpriteCollab: qui si conservano agli aggiornamenti, e le richieste a
//     raw.githubusercontent.com vanno in rete.
//   - Nessun skipWaiting() da solo: la versione nuova aspetta il pulsante «Aggiorna» della
//     pagina (messaggio 'attiva'), così la prima installazione non ricarica mai la pagina.
'use strict';

const VERSIONE = '@VERSIONE@';
const SHELL = 'pokegotchi-shell-' + VERSIONE;
const FILE = [
  './', 'index.html', 'stile.css', 'app.js', 'audio.js', 'salvataggi.js', 'sprite.js', 'pmd.js',
  'pokegotchi.js', 'pokegotchi.wasm', 'app.webmanifest',
  'icone/icona-192.png', 'icone/icona-512.png', 'icone/icona-maskable-512.png',
  'icone/apple-touch-icon.png', 'CREDITI_SPRITE.txt', 'THIRD_PARTY_NOTICES.txt',
];

self.addEventListener('install', (e) => {
  // cache: 'reload' scavalca la cache HTTP: i file sono quelli di questa build
  e.waitUntil(caches.open(SHELL)
    .then((c) => c.addAll(FILE.map((f) => new Request(f, { cache: 'reload' })))));
});

self.addEventListener('activate', (e) => {
  e.waitUntil((async () => {
    for (const k of await caches.keys()) {
      if (k.startsWith('pokegotchi-shell-') && k !== SHELL) await caches.delete(k);
    }
    await self.clients.claim();
  })());
});

self.addEventListener('message', (e) => {
  if (e.data === 'attiva') self.skipWaiting();
});

// La shell dalla cache (?debug compreso), poi la rete; fuori dalla cartella dell'app
// (i PNG di SpriteCollab) solo la rete.
const BASE = new URL('./', self.location).href;
self.addEventListener('fetch', (e) => {
  const r = e.request;
  if (r.method !== 'GET' || !r.url.startsWith(BASE)) return;
  e.respondWith((async () => {
    const c = await caches.open(SHELL);
    const x = await c.match(r, { ignoreSearch: true });
    return x || fetch(r);
  })());
});
