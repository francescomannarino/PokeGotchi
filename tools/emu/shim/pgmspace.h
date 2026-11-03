// Sul PC la flash e la RAM sono la stessa memoria: PROGMEM non fa nulla e le letture
// pgm_read_* sono semplici dereferenziazioni.
#pragma once
#include <stdint.h>
#include <string.h>

#define PROGMEM
#define PGM_P const char *
#define PSTR(s) (s)
#define pgm_read_byte(addr) (*(const uint8_t *)(addr))
#define pgm_read_word(addr) (*(const uint16_t *)(addr))
#define pgm_read_dword(addr) (*(const uint32_t *)(addr))
#define pgm_read_ptr(addr) (*(void *const *)(addr))
