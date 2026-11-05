// Movimento: filtro dell'inclinazione e riconoscitore dello scossone di moto.h.
// Le soglie vengono dalla prova del sensore: camminata 1.41 g, scossone 3.78 g.
#include "framework.h"
#include "../moto.h"
#include <math.h>

static const float RAD = 0.01745329f;

// gravità negli assi dello schermo con la scheda inclinata di `dx` gradi verso destra e
// `dy` verso il basso, partendo dal piano con lo schermo in su
static void inclinata(float dx, float dy, float *gx, float *gy, float *gz) {
  *gx = sinf(dx * RAD);
  *gy = sinf(dy * RAD);
  float r = 1 - *gx * *gx - *gy * *gy;
  *gz = -sqrtf(r > 0 ? r : 0);
}

static MotoFiltro filtroInPiano() {
  MotoFiltro f;
  f.zeroInizia();
  for (int i = 0; i < 10; i++) f.zeroAggiungi(0, 0, -1);
  f.zeroFissa();
  return f;
}

static float leggiX(MotoFiltro &f, float dx, float dy) {
  float x, y, z;
  inclinata(dx, dy, &x, &y, &z);
  f.aggiorna(x, y, z, 0);
  return f.x();
}

// ---------------------------------------------------------------- filtro

TEST(moto, in_piano_l_uscita_e_zero) {
  MotoFiltro f = filtroInPiano();
  f.aggiorna(0, 0, -1, 0);
  CHECK(f.x() == 0 && f.y() == 0);
}

TEST(moto, zona_morta_di_2_gradi) {
  MotoFiltro f = filtroInPiano();
  CHECK(leggiX(f, 1.5f, 0) == 0);
  CHECK(leggiX(f, -1.9f, 0) == 0);
  CHECK(leggiX(f, 3.0f, 0) > 0);
}

TEST(moto, lineare_tra_zona_morta_e_saturazione) {
  MotoFiltro f = filtroInPiano();
  CHECK_RANGE(leggiX(f, 13.5f, 0), 0.49f, 0.51f);  // (13.5 - 2) / (25 - 2)
  CHECK_RANGE(f.gradiX(), 13.49f, 13.51f);
}

TEST(moto, saturazione_a_25_gradi) {
  MotoFiltro f = filtroInPiano();
  CHECK_RANGE(leggiX(f, 25.0f, 0), 0.999f, 1.0f);
  CHECK(leggiX(f, 60.0f, 0) == 1.0f);
  CHECK(leggiX(f, -80.0f, 0) == -1.0f);
}

TEST(moto, assi_simmetrici_e_separati) {
  MotoFiltro f = filtroInPiano();
  float gx, gy, gz;
  inclinata(10, 0, &gx, &gy, &gz);
  f.aggiorna(gx, gy, gz, 0);
  float destra = f.x();
  CHECK(destra > 0);
  CHECK(f.y() == 0);
  inclinata(-10, 0, &gx, &gy, &gz);
  f.aggiorna(gx, gy, gz, 0);
  CHECK_RANGE(f.x(), -destra - 1e-5f, -destra + 1e-5f);
  inclinata(0, 10, &gx, &gy, &gz);
  f.aggiorna(gx, gy, gz, 0);
  CHECK_RANGE(f.y(), destra - 1e-5f, destra + 1e-5f);
  CHECK(f.x() == 0);
  inclinata(0, -10, &gx, &gy, &gz);
  f.aggiorna(gx, gy, gz, 0);
  CHECK_RANGE(f.y(), -destra - 1e-5f, -destra + 1e-5f);
}

TEST(moto, lo_zero_toglie_la_posizione_di_partenza) {
  // scheda reclinata di 40 gradi verso l'utente (il lato basso scende): è lo zero
  MotoFiltro f;
  float gx, gy, gz;
  inclinata(0, 40, &gx, &gy, &gz);
  f.zeroInizia();
  for (int i = 0; i < 20; i++) f.zeroAggiungi(gx, gy + (i % 2 ? 0.001f : -0.001f), gz);
  f.zeroFissa();
  f.aggiorna(gx, gy, gz, 0);
  CHECK(f.x() == 0 && f.y() == 0);
  inclinata(0, 52, &gx, &gy, &gz);  // 12 gradi in più verso il basso
  f.aggiorna(gx, gy, gz, 0);
  CHECK_RANGE(f.gradiY(), 11.9f, 12.1f);
  CHECK(f.y() > 0.4f);
}

TEST(moto, sdraiati_con_lo_schermo_in_giu) {
  // sopra la faccia, schermo verso il basso: la gravità esce dallo schermo
  MotoFiltro f;
  f.zeroInizia();
  f.zeroAggiungi(0, 0, 1);
  f.zeroFissa();
  float s = sinf(10 * RAD);
  f.aggiorna(s, 0, sqrtf(1 - s * s), 0);  // il lato destro scende
  CHECK_RANGE(f.gradiX(), 9.9f, 10.1f);
  CHECK(f.x() > 0);
}

TEST(moto, senza_campioni_lo_zero_e_il_piano) {
  MotoFiltro f;
  f.zeroInizia();
  f.zeroFissa();
  CHECK_RANGE(leggiX(f, 13.5f, 0), 0.49f, 0.51f);
}

TEST(moto, campione_nullo_ignorato) {
  MotoFiltro f = filtroInPiano();
  leggiX(f, 13.5f, 0);
  f.aggiorna(0, 0, 0, 10);  // lettura fallita
  CHECK_RANGE(f.x(), 0.49f, 0.51f);
}

TEST(moto, passa_basso_che_converge) {
  MotoFiltro f = filtroInPiano();
  f.aggiorna(0, 0, -1, 0);
  float gx, gy, gz;
  inclinata(20, 0, &gx, &gy, &gz);
  f.aggiorna(gx, gy, gz, 8);
  CHECK(f.gradiX() > 0 && f.gradiX() < 5);  // un campione a 125 Hz non basta
  float prima = f.gradiX();
  for (int i = 0; i < 125; i++) {           // un secondo
    f.aggiorna(gx, gy, gz, 8);
    CHECK(f.gradiX() >= prima);              // sale senza superare il valore vero
    CHECK(f.gradiX() <= 20.001f);
    prima = f.gradiX();
  }
  CHECK_RANGE(f.gradiX(), 19.9f, 20.001f);
}

TEST(moto, passa_basso_a_frequenze_diverse) {
  // 85 ms a fotogramma (il render dei giochi) o 8 ms (il task): dopo 400 ms ci si trova
  // più o meno allo stesso punto
  float gx, gy, gz;
  inclinata(20, 0, &gx, &gy, &gz);
  MotoFiltro a = filtroInPiano(), b = filtroInPiano();
  a.aggiorna(0, 0, -1, 0);
  b.aggiorna(0, 0, -1, 0);
  for (int t = 0; t < 400; t += 8) a.aggiorna(gx, gy, gz, 8);
  for (int t = 0; t < 400; t += 80) b.aggiorna(gx, gy, gz, 80);
  CHECK(fabsf(a.gradiX() - b.gradiX()) < 1.5f);
}

// ---------------------------------------------------------------- joystick

TEST(moto, joystick_zona_morta_e_centro) {
  float x = 9, y = 9;
  motoJoystick(0, 0, 150, 8, &x, &y);
  CHECK(x == 0 && y == 0);
  motoJoystick(5, -5, 150, 8, &x, &y);  // 7 px: dentro la zona morta
  CHECK(x == 0 && y == 0);
}

TEST(moto, joystick_lineare_e_pieno_al_raggio) {
  float x, y;
  motoJoystick(79, 0, 150, 8, &x, &y);  // a metà tra zona morta e raggio
  CHECK_RANGE(x, 0.49f, 0.51f);
  CHECK(y == 0);
  motoJoystick(0, -150, 150, 8, &x, &y);
  CHECK_RANGE(y, -1.001f, -0.999f);
  motoJoystick(0, 400, 150, 8, &x, &y);  // oltre il raggio: resta pieno
  CHECK_RANGE(y, 0.999f, 1.001f);
}

TEST(moto, joystick_in_diagonale_non_va_piu_veloce) {
  float x, y;
  motoJoystick(300, 300, 150, 8, &x, &y);
  CHECK_RANGE(sqrtf(x * x + y * y), 0.999f, 1.001f);
  CHECK_RANGE(x, 0.70f, 0.72f);
  CHECK_RANGE(y, 0.70f, 0.72f);
}

// ---------------------------------------------------------------- Surf

TEST(moto, angolo_ridotto_a_piu_o_meno_180) {
  CHECK_RANGE(motoAngolo(0), 0 - 0.01f, 0 + 0.01f);
  CHECK_RANGE(motoAngolo(190), -170 - 0.01f, -170 + 0.01f);
  CHECK_RANGE(motoAngolo(-190), 170 - 0.01f, 170 + 0.01f);
  CHECK_RANGE(motoAngolo(180), 180 - 0.01f, 180 + 0.01f);
  CHECK_RANGE(motoAngolo(-180), 180 - 0.01f, 180 + 0.01f);
  CHECK_RANGE(motoAngolo(725), 5 - 0.01f, 5 + 0.01f);
  CHECK_RANGE(motoAngolo(-1085), -5 - 0.01f, -5 + 0.01f);
}

TEST(moto, atterraggio_dentro_e_fuori_dalla_tolleranza) {
  const float T = MOTO_SURF_TOLLERANZA;
  CHECK(motoAtterra(10, 10, T));
  CHECK(motoAtterra(10 + 25, 10, T));    // sul bordo
  CHECK(motoAtterra(10 - 25, 10, T));
  CHECK(!motoAtterra(10 + 26, 10, T));
  CHECK(!motoAtterra(-20, 10, T));
  CHECK(!motoAtterra(190, 10, T));        // a testa in giù
  // dopo uno o due giri conta solo l'angolo finale, in un verso o nell'altro
  CHECK(motoAtterra(360 + 15, 0, T));
  CHECK(motoAtterra(-720 - 20, -10, T));
  CHECK(!motoAtterra(360 + 40, 0, T));
  // a cavallo di ±180: 170 e -175 distano 15 gradi
  CHECK(motoAtterra(170, -175, T));
}

TEST(moto, atterraggio_con_il_bonus) {
  const float T = MOTO_SURF_TOLLERANZA, B = MOTO_SURF_TOLLERANZA_BONUS;
  CHECK_EQ(T, 25.0f);
  CHECK_EQ(B, 40.0f);
  for (float d : {30.0f, -30.0f, 40.0f, -40.0f}) {
    CHECK(!motoAtterra(20 + d, 20, T));
    CHECK(motoAtterra(20 + d, 20, B));
  }
  CHECK(!motoAtterra(20 + 41, 20, B));
  CHECK(motoAtterra(360 - 35, 0, B));
}

TEST(moto, giri_completi_arrotondati) {
  CHECK_EQ(motoGiri(0), 0);
  CHECK_EQ(motoGiri(30), 0);
  CHECK_EQ(motoGiri(170), 0);
  CHECK_EQ(motoGiri(340), 1);
  CHECK_EQ(motoGiri(375), 1);
  CHECK_EQ(motoGiri(-360), 1);
  CHECK_EQ(motoGiri(-700), 2);
  CHECK_EQ(motoGiri(1080 + 20), 3);
}

// ---------------------------------------------------------------- scossone

// `secondi` di campioni a 125 Hz, con |a| (lungo z) dato da `modulo(t)`; conta gli scossoni
template <typename F>
static int scossoni(MotoScossone &s, float secondi, F modulo, uint32_t t0 = 1000) {
  int n = 0;
  for (uint32_t i = 0; i < (uint32_t)(secondi * 125); i++) {
    uint32_t ms = t0 + i * 8;
    float t = (ms - t0) / 1000.0f;
    if (s.campione(0, 0, -modulo(t), ms)) n++;
  }
  return n;
}

TEST(moto, scossone_riconosciuto) {
  // 4 scosse al secondo con picchi da 3.5 g
  MotoScossone s;
  int n = scossoni(s, 1.0f, [](float t) { return 1 + 2.5f * fabsf(sinf(2 * 3.14159f * 4 * t)); });
  CHECK(n == 1);
}

TEST(moto, scossone_con_i_valori_misurati) {
  // misurati: 3.78 g di picco, un superamento di 2.5 g ogni ~300 ms
  MotoScossone s;
  int n = scossoni(s, 3.5f, [](float t) {
    float f = fmodf(t, 0.3f);
    return f < 0.04f ? 3.78f : 1.0f;
  });
  CHECK_RANGE(n, 2, 4);  // con la pausa di 1 s, non uno a ogni coppia di picchi
}

TEST(moto, camminata_ignorata) {
  // passi a 2 Hz fino a 1.41 g, come la camminata misurata, per 30 s
  MotoScossone s;
  int n = scossoni(s, 30, [](float t) { return 1 + 0.41f * fabsf(sinf(2 * 3.14159f * 2 * t)); });
  CHECK(n == 0);
}

TEST(moto, colpo_singolo_ignorato) {
  // la scheda posata sul tavolo: un picco solo da 4 g
  MotoScossone s;
  int n = scossoni(s, 3, [](float t) { return t > 1.0f && t < 1.02f ? 4.0f : 1.0f; });
  CHECK(n == 0);
}

TEST(moto, due_colpi_lontani_ignorati) {
  MotoScossone s;
  int n = scossoni(s, 4, [](float t) {
    return (t > 1.0f && t < 1.02f) || (t > 2.5f && t < 2.52f) ? 4.0f : 1.0f;
  });
  CHECK(n == 0);
}

TEST(moto, un_picco_lungo_conta_una_volta) {
  // sopra soglia per 300 ms di fila: è un picco solo, non uno per campione
  MotoScossone s;
  int n = scossoni(s, 2, [](float t) { return t > 0.5f && t < 0.8f ? 3.0f : 1.0f; });
  CHECK(n == 0);
}

TEST(moto, pausa_dopo_lo_scossone) {
  MotoScossone s;
  auto scuoti = [](float t) { return 1 + 2.5f * fabsf(sinf(2 * 3.14159f * 4 * t)); };
  CHECK(scossoni(s, 0.5f, scuoti, 1000) == 1);
  CHECK(scossoni(s, 0.4f, scuoti, 1500) == 0);  // dentro il secondo di pausa
  CHECK(scossoni(s, 0.5f, scuoti, 3000) == 1);
}

TEST(moto, giro_del_contatore_dei_millisecondi) {
  MotoScossone s;
  auto scuoti = [](float t) { return 1 + 2.5f * fabsf(sinf(2 * 3.14159f * 4 * t)); };
  CHECK(scossoni(s, 1.0f, scuoti, 0xFFFFFF00u) == 1);
}
