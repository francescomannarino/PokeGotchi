#include "sdmon.h"
#include "hal.h"

// magic di un file della SD: quello attuale, o quello con le iniziali del nome che il progetto
// aveva fino alla 2.1 (schede SD preparate prima del cambio di nome)
static bool magicSprite(const uint8_t *d, const char *nuovo, const char *vecchio) {
  return memcmp(d, nuovo, 4) == 0 || memcmp(d, vecchio, 4) == 0;
}

#define PUT_BLOCCO 2048  // byte per blocco del protocollo PUT (send_sd.py e installer)

bool sdReady = false;
bool sdDirty = false;
SdThumbs thumbs;

bool PmdMon::load(uint8_t dexNum, bool shiny) {
  unload();
  if (!sdReady) return false;

  char path[28];
  snprintf(path, sizeof(path), "/mons/p%s%03u.bin", shiny ? "s" : "", dexNum);
  HalFile f = halSdOpen(path);
  if (!f && shiny) {  // senza shiny PMD: usa il normale
    snprintf(path, sizeof(path), "/mons/p%03u.bin", dexNum);
    f = halSdOpen(path);
  }
  if (!f) {
    Serial.printf("manca %s\n", path);
    return false;
  }

  uint32_t size = f.size();
  if (size < 7 || size > 3UL * 1024 * 1024) { f.close(); return false; }
  blob = (uint8_t *)ps_malloc(size);
  if (!blob || f.read(blob, size) != size || !magicSprite(blob, "PGK2", "TPK2")) {
    if (blob) { free(blob); blob = nullptr; }
    f.close();
    return false;
  }
  f.close();

  uint8_t nActs = blob[4];
  memcpy(&palCount, blob + 5, 2);
  if (palCount > 256 || (uint32_t)7 + palCount * 2 > size) { unload(); return false; }
  memcpy(pal, blob + 7, palCount * 2);

  const uint8_t *p = blob + 7 + palCount * 2;
  const uint8_t *end = blob + size;
  for (uint8_t i = 0; i < nActs && p + 4 <= end; i++) {
    uint8_t id = p[0], w = p[1], h = p[2], nf = p[3];
    p += 4;
    if (id >= PMD_NACTS || nf > 24) { unload(); return false; }
    // verifica che ms[] e i dati del frame stiano nel blob (file troncato)
    uint32_t bytes = (uint32_t)nf * 2 + (uint32_t)w * h * nf;
    if (w == 0 || h == 0 || nf == 0 || p + bytes > end) { unload(); return false; }
    PmdAct &a = acts[id];
    a.w = w;
    a.h = h;
    a.frames = nf;
    for (uint8_t k = 0; k < nf; k++) {
      a.ms[k] = p[0] | (p[1] << 8);
      if (a.ms[k] == 0) a.ms[k] = 100;  // mai 0: pmdFrameAt() girerebbe senza avanzare
      p += 2;
    }
    a.data = p;
    p += (uint32_t)w * h * nf;
    // riga più bassa con contenuto in qualsiasi frame: ancorare ai piedi
    uint8_t base = 1;
    for (uint8_t f = 0; f < nf; f++) {
      const uint8_t *fr = a.data + (uint32_t)f * w * h;
      for (int r = h - 1; r >= 0; r--) {
        bool any = false;
        for (int c = 0; c < w && !any; c++)
          if (fr[r * w + c] != 0xFF) any = true;
        if (any) { if (r + 1 > base) base = r + 1; break; }
      }
    }
    a.base = base;
  }
  loaded = true;
  Serial.printf("caricato %s (%u KB)\n", path, size / 1024);
  return true;
}

void PmdMon::unload() {
  if (blob) {
    free(blob);
    blob = nullptr;
  }
  for (auto &a : acts) {
    a.w = a.h = a.frames = a.base = 0;
    a.data = nullptr;
  }
  loaded = false;
}

void SdThumbs::unload() {
  if (data) { free(data); data = nullptr; }
  loaded = false;
  count = 0;
  size = 0;
}

bool SdThumbs::load() {
  unload();  // ricaricare senza perdere il blob precedente
  if (!sdReady) return false;
  HalFile f = halSdOpen("/mons/thumbs.bin");
  if (!f) {
    Serial.println("manca thumbs.bin (Pokedex senza miniature)");
    return false;
  }
  uint32_t sz = f.size();
  // limita la dimensione: evita un ps_malloc assurdo con un file corrotto
  if (sz < 10 || sz > 2UL * 1024 * 1024) {
    Serial.println("thumbs.bin non valido (dimensione)");
    f.close();
    return false;
  }
  data = (uint8_t *)ps_malloc(sz);
  if (!data || f.read(data, sz) != sz || !magicSprite(data, "PGTH", "TPTH")) {
    Serial.println("thumbs.bin non valido");
    if (data) { free(data); data = nullptr; }
    f.close();
    return false;
  }
  f.close();
  memcpy(&count, data + 4, 2);
  // la tabella degli offset (uno per specie) deve stare interamente in quanto letto
  if ((uint32_t)6 + 4UL * count > sz) {
    Serial.println("thumbs.bin non valido (tabella degli offset)");
    unload();
    return false;
  }
  size = sz;
  loaded = true;
  Serial.printf("miniature caricate: %u (%u KB)\n", count, sz / 1024);
  return true;
}

const uint8_t *SdThumbs::get(int16_t dex) const {
  if (!loaded || dex < 1 || dex > count) return nullptr;
  uint32_t off;
  memcpy(&off, data + 6 + 4 * (dex - 1), 4);
  // l'offset arriva dal file senza controllo: senza questo, un thumbs.bin troncato o
  // corrotto faceva leggere fuori dalla riserva di PSRAM
  if (off > size - 3) return nullptr;  // l'intestazione w,h,palCount deve starci (size >= 10 sempre)
  uint32_t need = 3 + (uint32_t)data[off + 2] * 2 + (uint32_t)data[off] * data[off + 1];
  if (need > size || off > size - need) return nullptr;  // e anche l'intero blob
  return data + off;
}

bool sdBegin() {
  sdReady = halSdBegin();
  if (sdReady) {
    halSdMkdir("/mons");
  } else {
    // il montaggio non espone il codice d'errore dell'ESP-IDF: si elencano le due cause
    Serial.println("SD non montata: scheda assente o non inizializzabile, oppure file "
                   "system non FAT32 (exFAT/NTFS non supportati). La scheda NON e' stata "
                   "formattata: formattala in FAT32 dal PC. Il gioco usa gli sprite in flash.");
  }
  return sdReady;
}

bool SdMon::load(uint8_t dexNum, bool shiny) {
  unload();
  if (!sdReady) return false;

  char path[24];
  snprintf(path, sizeof(path), "/mons/%s%03u.bin", shiny ? "s" : "", dexNum);
  HalFile f = halSdOpen(path);
  if (!f && shiny) {  // senza variante shiny: usa quella normale
    snprintf(path, sizeof(path), "/mons/%03u.bin", dexNum);
    f = halSdOpen(path);
  }
  if (!f) return false;  // riserva: senza file PGK1 (il caso normale) non si scrive nulla

  char magic[4];
  uint16_t header[4];
  if (f.read((uint8_t *)magic, 4) != 4 || !magicSprite((const uint8_t *)magic, "PGK1", "TPK1") ||
      f.read((uint8_t *)header, 8) != 8) {
    f.close();
    return false;
  }
  w = header[0];
  h = header[1];
  frames = header[2];
  frameMs = header[3];
  // limita le dimensioni: evita size in overflow o assurdo con un file corrotto
  if (f.read((uint8_t *)&palCount, 2) != 2 || palCount > 256 ||
      w == 0 || w > 256 || h == 0 || h > 256 || frames == 0 || frames > 64) {
    f.close();
    return false;
  }
  if (f.read((uint8_t *)pal, palCount * 2) != palCount * 2) {
    f.close();
    return false;
  }

  uint32_t size = (uint32_t)w * h * frames;
  data = (uint8_t *)ps_malloc(size);
  if (!data) {
    Serial.println("PSRAM insufficiente per lo sprite");
    f.close();
    return false;
  }
  uint32_t got = f.read(data, size);
  f.close();
  if (got != size) {
    Serial.printf("%s troncato (%u di %u)\n", path, got, size);
    unload();
    return false;
  }

  // zoom intero perché il Pokémon misuri ~200 px di altezza a schermo
  scale = 200 / h;
  if (scale < 2) scale = 2;
  if (scale > 5) scale = 5;

  Serial.printf("caricato %s: %ux%u x%u fotogrammi @%ums, scala %u\n",
                path, w, h, frames, frameMs, scale);
  loaded = true;
  return true;
}

void SdMon::unload() {
  if (data) {
    free(data);
    data = nullptr;
  }
  loaded = false;
}

// ---------------------------------------------------------------------------
// Protocollo di caricamento via USB (per riempire la SD senza estrarla dalla scheda):
// PUT <percorso> <byte>\n  + dati grezzi   -> "OK" ... "DONE"
// LS\n                                 -> elenco di /mons
// Da usare con tools/send_sd.py
// ---------------------------------------------------------------------------

bool sdSerialCommand(const String &line) {
  if (line.startsWith("PUT ")) {
    int sp = line.lastIndexOf(' ');
    String path = line.substring(4, sp);
    uint32_t size = line.substring(sp + 1).toInt();
    if (!sdReady || size == 0 || size > 4 * 1024 * 1024) {
      Serial.println("ERR");
      return true;
    }
    if (!path.startsWith("/")) path = "/" + path;
    // limita la scrittura a /mons/: il percorso arriva tale e quale dalla riga seriale,
    // senza sanificazione, quindi un PUT manipolato potrebbe scrivere ovunque nella
    // scheda. I due client reali (tools/send_sd.py e web/index.html) mandano già nomi
    // con il prefisso mons/.
    if (!path.startsWith("/mons/") || path.indexOf("..") >= 0) {
      Serial.println("ERR");
      return true;
    }
    // il blocco sta nella PSRAM, preso al primo PUT e tenuto (2 KB di DRAM in meno)
    static uint8_t *buf = (uint8_t *)ps_malloc(PUT_BLOCCO);
    if (!buf) {
      Serial.println("ERR");
      return true;
    }
    // si riparte sempre da un file vuoto (halSdOpen): accodare a un file rimasto a metà
    // lo allungava invece di sostituirlo, e l'installer riprova proprio quelli falliti
    HalFile f = halSdOpen(path.c_str(), true);
    if (!f) {
      Serial.println("ERR");
      return true;
    }
    Serial.println("OK");
    uint32_t remaining = size;
    Serial.setTimeout(5000);
    while (remaining > 0) {
      size_t want = remaining > PUT_BLOCCO ? PUT_BLOCCO : remaining;
      size_t n = Serial.readBytes(buf, want);
      if (n == 0) break;  // timeout
      // senza controllare il valore di ritorno, una scheda piena o con errore di
      // scrittura dava comunque DONE e lasciava il file troncato nella SD: proprio la
      // voce corrotta da cui bisogna difendersi poi al caricamento dello sprite
      if (f.write(buf, n) != n) break;
      remaining -= n;
      Serial.println("#");  // ack: pronto per il blocco successivo
    }
    f.close();
    Serial.setTimeout(1000);
    sdDirty = (remaining == 0);
    Serial.println(remaining == 0 ? "DONE" : "ERR");
    return true;
  } else if (line == "SDINFO") {  // diagnostica remota di "non mi riconosce la SD"
    uint64_t tot, usato;
    halSdSpazio(&tot, &usato);
    Serial.printf("total=%llu used=%llu\n", tot, usato);
    Serial.println("DONE");
    return true;
  } else if (line == "LS") {
    halSdElenca("/mons", [](const char *nome, uint32_t size, void *) {
      Serial.printf("%s %u\n", nome, size);
    }, nullptr);
    Serial.println("DONE");
    return true;
  }
  return false;
}
