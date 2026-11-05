// Il touch su PC, condiviso da emulatore (hal_host.cpp) e PWA (pwa/hal_web.cpp): il dito
// del mouse o del pointer, oppure un gesto in coda (frecce, copione) letto un fotogramma
// per volta da halTouchRead().
#include "../../hal.h"
#include "emu.h"
#include <deque>

static const int LCD_W = 466, LCD_H = 466;

// ---------------------------------------------------------------- touch

struct Tocco { bool giu; int16_t x, y; };
static std::deque<Tocco> gGesto;  // un fotogramma del gesto per ogni lettura
static Tocco gMouse = {false, 0, 0};
static bool gGiuLetto = true;  // halTouchRead() ha visto il dito giù del mouse

// Un clic più breve di un fotogramma (giù e su prima di una lettura) si perderebbe:
// diventa un tocco in coda.
void emuDito(bool giu, int16_t x, int16_t y) {
  if (giu && !gMouse.giu) gGiuLetto = false;
  if (!giu && gMouse.giu && !gGiuLetto) emuGestoTocco(gMouse.x, gMouse.y);
  gMouse = {giu, x, y};
}
bool emuGestoInCorso() { return !gGesto.empty() || gMouse.giu; }

void emuGestoTocco(int16_t x, int16_t y) {
  gGesto.push_back({true, x, y});
  gGesto.push_back({true, x, y});
  gGesto.push_back({false, x, y});
}

// 200 px in 5 letture (~100 ms): ben oltre la soglia di 80 px di handleTouch()
void emuGestoScorri(int dx, int dy) {
  for (int i = 0; i <= 4; i++) {
    int d = -100 + 50 * i;
    gGesto.push_back({true, (int16_t)(LCD_W / 2 + dx * d), (int16_t)(LCD_H / 2 + dy * d)});
  }
  gGesto.push_back({false, (int16_t)(LCD_W / 2 + dx * 100), (int16_t)(LCD_H / 2 + dy * 100)});
}

int8_t halTouchRead(int16_t *x, int16_t *y, bool giu) {
  Tocco t = gMouse;
  if (!gGesto.empty()) {
    t = gGesto.front();
    gGesto.pop_front();
  } else if (!t.giu && !giu) {
    return HAL_TOUCH_NIENTE;
  } else if (t.giu) {
    gGiuLetto = true;
  }
  *x = t.x;
  *y = t.y;
  return t.giu ? HAL_TOUCH_GIU : HAL_TOUCH_SU;
}
