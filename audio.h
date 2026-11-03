#pragma once
// Driver audio della scheda: lo usa solo hal_esp32.cpp. Gli effetti (enum Sfx) sono in
// hal.h, i brani (MUS_*) in musica_dati.h; il gioco li suona con halSfx() e halMusica().
#include <stdint.h>
#include "hal.h"

void audioBegin();          // ES8311 + I2S + amplificatore + motore di musica.h + task audio
void sfxPlay(uint8_t id);   // accoda un effetto (non blocca il loop)
void audioMusica(uint8_t brano);       // MUS_NESSUNA ferma; niente con MUSICA spenta
uint8_t audioVolume();                 // 0..10 (chiave "vol", migrata da "snd")
void audioSetVolume(uint8_t v);
bool audioMusicaAccesa();              // chiave "mus"
void audioSetMusicaAccesa(bool on);
void audioSetSleeping(bool sleeping);  // addormentata o schermo spento: amplificatore spento
void audioDiag(HalAudioDiag *d);       // per AUDIO; azzera carico e blocco più lento
