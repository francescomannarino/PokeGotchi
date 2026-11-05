// Movimento: filtro dell'inclinazione, atterraggio del Surf e riconoscitore dello
// scossone. Vedi moto.h.
#include "moto.h"
#include <math.h>

static const float GRADI = 57.29578f;

// angoli degli assi x e y dello schermo sull'orizzontale, in gradi (positivo = l'asse
// scende); falso se il campione non ha modulo (sensore che non risponde)
static bool angoli(float gx, float gy, float gz, float *x, float *y) {
  float m = sqrtf(gx * gx + gy * gy + gz * gz);
  if (m < 0.2f) return false;
  float cx = gx / m, cy = gy / m;
  *x = asinf(cx > 1 ? 1 : cx < -1 ? -1 : cx) * GRADI;
  *y = asinf(cy > 1 ? 1 : cy < -1 ? -1 : cy) * GRADI;
  return true;
}

// ---------------------------------------------------------------- inclinazione

void MotoFiltro::zeroInizia() {
  sx = sy = sz = 0;
  n = 0;
}

void MotoFiltro::zeroAggiungi(float gx, float gy, float gz) {
  if (n == 0xFFFF) return;
  sx += gx;
  sy += gy;
  sz += gz;
  n++;
}

void MotoFiltro::zeroFissa() {
  zx = zy = 0;
  if (n) angoli(sx / n, sy / n, sz / n, &zx, &zy);
  ax = ay = 0;
  primo = true;
}

void MotoFiltro::aggiorna(float gx, float gy, float gz, uint32_t dtMs) {
  float x, y;
  if (!angoli(gx, gy, gz, &x, &y)) return;
  x -= zx;
  y -= zy;
  if (primo || dtMs == 0 || tau <= 0) {
    ax = x;
    ay = y;
    primo = false;
    return;
  }
  float a = dtMs / (tau + dtMs);
  ax += a * (x - ax);
  ay += a * (y - ay);
}

float MotoFiltro::uscita(float gradi) const {
  float m = fabsf(gradi) - zonaMorta;
  if (m <= 0) return 0;
  float campo = saturazione - zonaMorta;
  float v = campo > 0 ? m / campo : 1;
  if (v > 1) v = 1;
  return gradi < 0 ? -v : v;
}

void motoJoystick(float dx, float dy, float raggio, float morta, float *x, float *y) {
  *x = *y = 0;
  float d = sqrtf(dx * dx + dy * dy);
  if (d <= morta || raggio <= morta) return;
  float v = (d - morta) / (raggio - morta);
  if (v > 1) v = 1;
  *x = dx / d * v;
  *y = dy / d * v;
}

// ---------------------------------------------------------------- Surf

float motoAngolo(float gradi) {
  float a = fmodf(gradi, 360.0f);  // (-360, 360)
  if (a > 180.0f) a -= 360.0f;
  else if (a <= -180.0f) a += 360.0f;
  return a;
}

bool motoAtterra(float tavola, float onda, float tolleranza) {
  return fabsf(motoAngolo(tavola - onda)) <= tolleranza;
}

uint8_t motoGiri(float rotazione) {
  float g = floorf(fabsf(rotazione) / 360.0f + 0.5f);
  return g > 255 ? 255 : (uint8_t)g;
}

// ---------------------------------------------------------------- scossone

void MotoScossone::azzera() {
  sopra = picco = inPausa = false;
  tPicco = tScossone = 0;
}

bool MotoScossone::campione(float gx, float gy, float gz, uint32_t ms) {
  float q = gx * gx + gy * gy + gz * gz;
  if (inPausa && ms - tScossone >= pausaMs) inPausa = false;
  if (sopra) {
    if (q < rientro * rientro) sopra = false;
    return false;
  }
  if (q <= soglia * soglia) return false;
  // inizio di un picco
  sopra = true;
  if (inPausa) return false;
  if (picco && ms - tPicco <= finestraMs) {
    picco = false;
    inPausa = true;
    tScossone = ms;
    return true;
  }
  picco = true;
  tPicco = ms;
  return false;
}
