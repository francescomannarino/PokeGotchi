#pragma once
#include <Arduino.h>

// Sprite animato PGK1 (formato ereditato, percorso di riserva). Il progetto usa
// PMD/PGK2 (PmdMon) per tutto; questo percorso resta inattivo se non c'è NNN.bin nella
// SD. I dati indicizzati vivono in PSRAM; la palette è RGB565.
struct SdMon {
  bool loaded = false;
  uint16_t w = 0, h = 0, frames = 0, frameMs = 100;
  uint8_t scale = 2;       // fattore di zoom intero in fase di disegno
  uint16_t palCount = 0;
  uint16_t pal[256];
  uint8_t *data = nullptr;  // frames * w * h indici (0xFF = trasparente)

  bool load(uint8_t dexNum, bool shiny = false);
  void unload();
};

// azioni degli sprite PMD (formato PGK2)
enum : uint8_t {
  PMD_IDLE = 0, PMD_WALKL, PMD_WALKR, PMD_SLEEP, PMD_EAT, PMD_HURT,
  PMD_ATTACK, PMD_POSE, PMD_HOP, PMD_NOD, PMD_BREATH, PMD_SIT,
  PMD_NACTS
};

struct PmdAct {
  uint8_t w = 0, h = 0, frames = 0;
  uint8_t base = 0;  // riga+1 del pixel più basso (ancorare ai piedi, non alla tela)
  uint16_t ms[24];
  const uint8_t *data = nullptr;  // frames * w * h nel blob
};

// sprite PMD multi-azione caricato dalla SD in PSRAM
struct PmdMon {
  bool loaded = false;
  uint16_t palCount = 0;
  uint16_t pal[256];
  uint8_t *blob = nullptr;
  PmdAct acts[PMD_NACTS];

  bool load(uint8_t dexNum, bool shiny = false);
  void unload();
  bool has(uint8_t a) const { return loaded && a < PMD_NACTS && acts[a].frames > 0; }
};

// miniature della galleria (thumbs.bin intero in PSRAM)
struct SdThumbs {
  bool loaded = false;
  uint8_t *data = nullptr;
  uint16_t count = 0;
  uint32_t size = 0;  // byte letti: delimita gli offset del file
  bool load();
  void unload();  // libera il blob: ricaricare dopo aver ricevuto un nuovo thumbs.bin
  const uint8_t *get(int16_t dex) const;  // blob: w,h,palCount,pal[],idx[]
};
extern SdThumbs thumbs;

bool sdBegin();                 // monta la SD (SDMMC 1-bit), true se c'è la scheda
bool sdSerialCommand(const String &line);  // PUT/LS via USB; true se lo gestisce
extern bool sdReady;
extern bool sdDirty;  // true dopo aver ricevuto file: ricaricare lo sprite
