// Prova di fattibilità dell'emulatore: la vera Arduino_GFX (Arduino_GFX + Arduino_Canvas, font 5x7
// CP437 e sottoinsiemi unifont di U8g2) compilata sul PC. Disegna testo latino e CJK su
// una tela 466x466, come quella della scheda, e salva il fotogramma in PNG.
//
//   make prova && ./build/prova_gfx [file.png]
#include <Arduino_GFX_Library.h>
#include <vector>
#include "png.h"

static const int LCD_W = 466, LCD_H = 466;

// Il "pannello" finto: Arduino_Canvas::flush() gli passa l'intero framebuffer RGB565.
// Le altre primitive di Arduino_G non servono a una tela, quindi non fanno nulla.
class PannelloFinto : public Arduino_G {
public:
  std::vector<uint16_t> fb;
  PannelloFinto(int16_t w, int16_t h) : Arduino_G(w, h), fb((size_t)w * h, 0) {}
  bool begin(int32_t) override { return true; }
  void draw16bitRGBBitmap(int16_t x, int16_t y, uint16_t *bmp, int16_t w, int16_t h) override {
    for (int j = 0; j < h; j++)
      for (int i = 0; i < w; i++)
        if (x + i >= 0 && x + i < WIDTH && y + j >= 0 && y + j < HEIGHT)
          fb[(size_t)(y + j) * WIDTH + x + i] = bmp[(size_t)j * w + i];
  }
  void drawBitmap(int16_t, int16_t, uint8_t *, int16_t, int16_t, uint16_t, uint16_t) override {}
  void drawIndexedBitmap(int16_t, int16_t, uint8_t *, uint16_t *, int16_t, int16_t, int16_t) override {}
  void draw3bitRGBBitmap(int16_t, int16_t, uint8_t *, int16_t, int16_t) override {}
  void draw24bitRGBBitmap(int16_t, int16_t, uint8_t *, int16_t, int16_t) override {}
};

// riga di testo centrata in orizzontale; misura con getTextBounds() come textW()
static void centrata(Arduino_GFX *g, const char *s, int y) {
  int16_t x1, y1;
  uint16_t w, h;
  g->getTextBounds(s, 0, 0, &x1, &y1, &w, &h);
  g->setCursor(LCD_W / 2 - (int)w / 2, y);
  g->print(s);
}

int main(int argc, char **argv) {
  const char *out = argc > 1 ? argv[1] : "build/prova_gfx.png";
  PannelloFinto pannello(LCD_W, LCD_H);
  Arduino_Canvas *gfx = new Arduino_Canvas(LCD_W, LCD_H, &pannello);
  if (!gfx->begin()) {
    fprintf(stderr, "begin() fallito\n");
    return 1;
  }
  gfx->fillScreen(RGB565_BLACK);
  gfx->fillCircle(LCD_W / 2, LCD_H / 2, LCD_W / 2 - 1, RGB565(16, 40, 24));
  gfx->drawCircle(LCD_W / 2, LCD_H / 2, LCD_W / 2 - 1, RGB565_WHITE);

  // font classico 5x7 CP437: gli accenti sono byte singoli, come in i18n.cpp
  gfx->setFont();
  gfx->setUTF8Print(false);
  gfx->setTextColor(RGB565_WHITE);
  gfx->setTextSize(4);
  centrata(gfx, "POKEGOTCHI", 70);
  gfx->setTextSize(2);
  gfx->setTextColor(RGB565_YELLOW);
  centrata(gfx, "Felicit\205 e perch\202", 130);
  centrata(gfx, "Espa\244ol: ni\244o, M\201nchen", 160);
  gfx->setTextSize(1);
  gfx->setTextColor(RGB565_LIGHTGREY);
  centrata(gfx, "5x7 CP437, dimensione 1: PIKACHU LV 12", 195);

  // sottoinsiemi unifont di U8g2, gli stessi di ui_common.cpp; UTF-8
  gfx->setUTF8Print(true);
  gfx->setTextColor(RGB565_CYAN);
  gfx->setFont(u8g2_font_unifont_t_japanese3);
  gfx->setTextSize(2);
  centrata(gfx, "ピカチュウ！", 260);
  gfx->setTextSize(1);
  centrata(gfx, "たまごが かえった？ 図鑑", 290);
  gfx->setTextColor(RGB565_PINK);
  gfx->setFont(u8g2_font_unifont_t_korean2);
  gfx->setTextSize(2);
  centrata(gfx, "피카츄 안녕!", 340);
  gfx->setTextSize(1);
  centrata(gfx, "알이 부화했다", 370);

  gfx->flush();
  if (!scriviPng(out, pannello.fb.data(), LCD_W, LCD_H)) {
    fprintf(stderr, "impossibile scrivere %s\n", out);
    return 1;
  }
  printf("scritto %s\n", out);
  return 0;
}
