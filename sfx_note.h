#pragma once
// Le note degli effetti sonori (enum Sfx di hal.h), in onda quadra stile Game Boy:
// frequenza in Hz (0 = pausa) e durata in ms. Le suona musica.cpp sull'APU emulata, su
// scheda, emulatore e PWA.
#include <stdint.h>
#include "hal.h"

struct Note { uint16_t f, ms; };

static const Note N_TAP[]    = {{880, 35}};
static const Note N_EAT[]    = {{660, 45}, {0, 12}, {660, 45}};
static const Note N_PLAY[]   = {{784, 45}, {988, 60}};
static const Note N_HEART[]  = {{1047, 55}, {1319, 90}};
static const Note N_HATCH[]  = {{523, 80}, {659, 80}, {784, 110}, {1047, 170}};
static const Note N_EVOLVE[] = {{523, 80}, {659, 80}, {784, 80}, {1047, 90}, {1319, 230}};
static const Note N_MEDAL[]  = {{784, 70}, {0, 25}, {784, 70}, {0, 25}, {1047, 200}};
static const Note N_DENY[]   = {{300, 110}, {200, 170}};
static const Note N_BYE[]    = {{784, 150}, {659, 150}, {523, 280}};
static const Note N_LEVEL[]  = {{784, 70}, {1047, 130}};
static const Note N_ATTACK[] = {{1319, 25}, {988, 25}, {659, 35}};
static const Note N_HIT[]    = {{196, 45}, {0, 15}, {147, 70}};
static const Note N_FAINT[]  = {{523, 90}, {392, 90}, {294, 90}, {196, 240}};

// duty del canale 1 come in NR11 (0 = 12,5 %, 2 = 50 %): il rifiuto è stretto, perché a
// 300/200 Hz un'onda al 50 % finisce sotto il basso della musica di battaglia e sotto quello
// che l'altoparlantino riproduce; al 12,5 % le armoniche lo fanno sentire sopra il brano
struct SfxDef { const Note *n; uint8_t len; uint8_t duty = 2; };
static const SfxDef SFX[SFX_COUNT] = {
  {N_TAP, 1}, {N_EAT, 3}, {N_PLAY, 2}, {N_HEART, 2}, {N_HATCH, 4},
  {N_EVOLVE, 5}, {N_MEDAL, 5}, {N_DENY, 2, 0}, {N_BYE, 3}, {N_LEVEL, 2},
  {N_ATTACK, 3}, {N_HIT, 3}, {N_FAINT, 4},
};
