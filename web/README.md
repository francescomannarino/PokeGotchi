# Installer web di PokeGotchi

Una pagina che flasha il firmware e carica gli sprite dal browser (Chrome/Edge), senza
Arduino né driver. Usa [ESP Web Tools](https://esphome.github.io/esp-web-tools/) per flashare
e **Web Serial** per inviare gli sprite alla SD con il protocollo `PUT` del firmware (lo stesso
di `tools/send_sd.py`). Pubblicata in
**[francescomannarino.github.io/PokeGotchi/installa/](https://francescomannarino.github.io/PokeGotchi/installa/)**.

## Contenuto

- `index.html` — la pagina: salva i progressi, flasha, ripristina, carica gli sprite, imposta
  data e ora.
- `manifest.json` — configurazione di ESP Web Tools (versione e parti del firmware).
- `esp-web-tools/` — ESP Web Tools 10.4.0, che il sito serve da sé invece di caricarlo da una
  CDN: lo copia `tools/esp_web_tools.sh` (anche `build_web.sh` e `build_sito.sh`), dopo aver
  controllato lo SHA-512 del pacchetto npm. Non nel repository.
- `firmware/boot.bin` (a `0x0`: bootloader e tabella delle partizioni) e
  `firmware/pokegotchi.bin` (a `0xE000`: `boot_app0` e l'app). Due pezzi e non un'immagine
  unica da `0x0`, che avrebbe 0xFF sulla NVS (`0x9000`–`0xDFFF`) e cancellerebbe la partita
  a ogni flash, anche senza «Erase device». **Generati** da `tools/build_firmware.sh`, non nel
  repository: sul sito arrivano dall'ultima release.
- `pmd.js` — il convertitore degli sprite, lo stesso della PWA: scarica i PNG di
  [PMD SpriteCollab](https://github.com/PMDCollab/SpriteCollab) (CC BY-NC 4.0) a un commit
  fissato e li trasforma nel browser negli stessi file di `tools/pack_pmd.py` e
  `tools/make_thumbs.py` (`p[s]NNN.bin`, `thumbs.bin`), byte per byte. Né il repository né il
  sito contengono sprite.
- `gioca/` — la PWA, il gioco intero nel browser. **Generata** da `tools/build_pwa.sh` a
  partire da `pwa/`, non nel repository.

Sul sito l'installer sta in `/installa/` (con la sua copia di `pmd.js`) e la PWA in `/gioca/`:
la pagina la cerca in `../gioca/`, che in locale (dove `web/` è la radice) resta lì.

## Provare in locale

Web Serial ed ESP Web Tools richiedono un **contesto sicuro**: `https://` oppure
`http://localhost`.

```bash
bash tools/build_web.sh               # firmware in web/firmware/, PWA in web/gioca/
python3 tools/serve_web.py --locale   # come http.server dentro web/
# installer su http://localhost:8000, PWA su http://localhost:8000/gioca/
```

Per vedere tutto come sul sito: `bash tools/build_sito.sh` e
`python3 tools/serve_web.py --sito --locale`.

## Pubblicazione

La pubblica la CI (`.github/workflows/pages.yml`) su GitHub Pages, insieme al sito, alla PWA e
al libretto, a ogni release e quando cambiano le pagine. GitHub Pages fornisce HTTPS; gli
sprite arrivano da `raw.githubusercontent.com`, che permette le richieste da altre origini
(CORS).

## Note

- La porta seriale la può usare un solo programma alla volta: la pagina chiude la finestra
  del flash prima dei passi che usano Web Serial.
- I salvataggi (`.pgsave`) passano da `EXPORT`/`IMPORT` della console; l'importazione va
  confermata sullo schermo della scheda. Si leggono anche i file delle versioni precedenti.
- Una volta installato, il gioco si aggiorna anche da solo via WiFi (impostazioni → pillola
  della versione).
