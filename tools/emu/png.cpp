#include "png.h"
#include <stdio.h>
#include <vector>

static uint32_t crc32(const uint8_t *p, size_t n, uint32_t c = 0xFFFFFFFFu) {
  for (size_t i = 0; i < n; i++) {
    c ^= p[i];
    for (int k = 0; k < 8; k++) c = (c >> 1) ^ (0xEDB88320u & (0u - (c & 1)));
  }
  return c;
}

static void be32(std::vector<uint8_t> &v, uint32_t x) {
  v.push_back(x >> 24); v.push_back(x >> 16); v.push_back(x >> 8); v.push_back(x);
}

static void chunk(FILE *f, const char *tipo, const std::vector<uint8_t> &dati) {
  std::vector<uint8_t> c;
  be32(c, (uint32_t)dati.size());
  c.insert(c.end(), tipo, tipo + 4);
  c.insert(c.end(), dati.begin(), dati.end());
  be32(c, crc32(c.data() + 4, c.size() - 4) ^ 0xFFFFFFFFu);
  fwrite(c.data(), 1, c.size(), f);
}

bool scriviPng(const char *path, const uint16_t *fb, int w, int h) {
  // righe grezze: un byte di filtro (0 = nessuno) e poi RGB888
  std::vector<uint8_t> raw;
  raw.reserve((size_t)h * (1 + w * 3));
  for (int y = 0; y < h; y++) {
    raw.push_back(0);
    for (int x = 0; x < w; x++) {
      uint16_t c = fb[(size_t)y * w + x];
      uint8_t r = (c >> 11) & 0x1F, g = (c >> 5) & 0x3F, b = c & 0x1F;
      raw.push_back((r << 3) | (r >> 2));
      raw.push_back((g << 2) | (g >> 4));
      raw.push_back((b << 3) | (b >> 2));
    }
  }
  // flusso zlib con blocchi stored da al massimo 65535 byte, poi Adler-32
  std::vector<uint8_t> z = {0x78, 0x01};
  for (size_t off = 0; off < raw.size() || off == 0;) {
    size_t n = raw.size() - off < 65535 ? raw.size() - off : 65535;
    z.push_back(off + n == raw.size() ? 1 : 0);
    z.push_back(n & 0xFF); z.push_back(n >> 8);
    z.push_back(~n & 0xFF); z.push_back((~n >> 8) & 0xFF);
    z.insert(z.end(), raw.begin() + off, raw.begin() + off + n);
    off += n;
    if (n == 0) break;
  }
  uint32_t a = 1, b = 0;
  for (uint8_t v : raw) { a = (a + v) % 65521; b = (b + a) % 65521; }
  be32(z, (b << 16) | a);

  FILE *f = fopen(path, "wb");
  if (!f) return false;
  static const uint8_t firma[8] = {0x89, 'P', 'N', 'G', '\r', '\n', 0x1A, '\n'};
  fwrite(firma, 1, 8, f);
  std::vector<uint8_t> ihdr;
  be32(ihdr, w); be32(ihdr, h);
  ihdr.insert(ihdr.end(), {8, 2, 0, 0, 0});   // 8 bit, RGB, deflate, filtro 0, no interlace
  chunk(f, "IHDR", ihdr);
  chunk(f, "IDAT", z);
  chunk(f, "IEND", {});
  return fclose(f) == 0;
}
