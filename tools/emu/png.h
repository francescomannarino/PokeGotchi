// Scrittura di un PNG RGB senza dipendenze (niente zlib): i dati vanno in blocchi
// deflate "stored", non compressi. Un fotogramma 466x466 pesa ~650 KB.
#pragma once
#include <stdint.h>

// fb: RGB565, w*h pixel per righe. Restituisce false se il file non si scrive.
bool scriviPng(const char *path, const uint16_t *fb, int w, int h);
