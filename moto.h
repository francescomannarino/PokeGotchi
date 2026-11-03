#pragma once
// Movimento: la logica dei giochi a inclinazione, dell'atterraggio del Surf e
// dello scossone, senza hardware. La usano la scheda (hal_esp32.cpp), l'emulatore, la PWA e i test. I campioni
// arrivano già negli assi dello schermo, in g, come da halMotoGravita(): x a destra, y in
// basso, z uscente; a scheda in piano e schermo in su la gravità è (0, 0, -1).
//
// Soglie dalla prova del sensore (tools/prova_imu): a riposo |a| oscilla di ~1.5 mg, una
// camminata con la scheda in mano arriva a 1.41 g, uno scossone forte a 3.78 g.
#include <stdint.h>

// ---------------------------------------------------------------- inclinazione

// Filtro dell'inclinazione dei giochi: zero, passa-basso, zona morta e saturazione.
// L'inclinazione di un asse è l'angolo dell'asse dello schermo sull'orizzontale meno lo
// stesso angolo misurato allo zero, così si gioca anche sdraiati o con la scheda reclinata.
// Uso: zeroInizia(), zeroAggiungi() per ogni campione del conto alla rovescia, zeroFissa();
// poi aggiorna() a ogni campione e x()/y() da -1 a 1 (positivo = verso destra / verso il
// basso dello schermo, dove scivolerebbe una pallina).
struct MotoFiltro {
  float zonaMorta = 2.0f;       // gradi: sotto, l'uscita è 0
  float saturazione = 25.0f;    // gradi: da qui in su, l'uscita è ±1
  float tau = 100.0f;           // ms: costante di tempo del passa-basso

  void zeroInizia();
  void zeroAggiungi(float gx, float gy, float gz);
  // fissa lo zero sulla media dei campioni (senza campioni: la scheda in piano) e riparte
  // dal passa-basso fermo
  void zeroFissa();
  // nuovo campione, `dtMs` dopo il precedente (0 = il primo: il filtro salta lì)
  void aggiorna(float gx, float gy, float gz, uint32_t dtMs);
  float x() const { return uscita(ax); }
  float y() const { return uscita(ay); }
  float gradiX() const { return ax; }   // inclinazione filtrata, in gradi, prima di zona
  float gradiY() const { return ay; }   //   morta e saturazione

 private:
  float uscita(float gradi) const;
  float sx = 0, sy = 0, sz = 0;  // somma dei campioni dello zero
  uint16_t n = 0;
  float zx = 0, zy = 0;          // angoli degli assi allo zero, in gradi
  float ax = 0, ay = 0;          // inclinazione filtrata, in gradi
  bool primo = true;
};

// Joystick sul touch, quando il sensore manca: il dito tenuto a (`dx`, `dy`) pixel dal centro
// dà la direzione e l'intensità, come l'inclinazione (da -1 a 1 per asse, positivo = destra /
// basso). A `raggio` pixel l'intensità è piena; sotto `morta` pixel è 0. Il vettore è
// limitato al cerchio: in diagonale non si va più veloci.
void motoJoystick(float dx, float dy, float raggio, float morta, float *x, float *y);

// ---------------------------------------------------------------- Surf

// Atterraggio del Surf. Gli angoli sono in gradi, nel verso dello schermo (y in
// basso): positivo = la punta destra scende, come la pendenza dell'onda dy/dx. La tavola
// atterra se, ridotta a (-180, 180], la sua differenza dalla pendenza dell'onda sta entro
// ±tolleranza; Pikachu, Raichu e i tipi Acqua hanno la tolleranza larga.
#define MOTO_SURF_TOLLERANZA 25.0f
#define MOTO_SURF_TOLLERANZA_BONUS 40.0f
float motoAngolo(float gradi);  // ridotto a (-180, 180]
bool motoAtterra(float tavola, float onda, float tolleranza);
// giri completi di una rotazione in aria, in un verso o nell'altro: un atterraggio riuscito
// arriva vicino a un multiplo di 360, quindi si arrotonda (350 gradi sono un giro)
uint8_t motoGiri(float rotazione);

// ---------------------------------------------------------------- scossone

// Riconoscitore dello scossone sul modulo dell'accelerazione |a|. Un picco è un tratto
// sopra `soglia`, chiuso quando |a| scende sotto `rientro`. Lo scossone sono due picchi a
// non più di `finestraMs` uno dall'altro: un colpo solo (la scheda posata sul tavolo) non
// basta. Dopo uno scossone si attende `pausaMs` prima del successivo.
struct MotoScossone {
  float soglia = 2.5f;          // g: tra la camminata (1.41) e lo scossone (3.78)
  float rientro = 1.5f;         // g
  uint16_t finestraMs = 800;
  uint16_t pausaMs = 1000;

  // un campione al tempo `ms` (millis()); true quando riconosce uno scossone
  bool campione(float gx, float gy, float gz, uint32_t ms);
  void azzera();

 private:
  bool sopra = false;           // dentro un picco
  bool picco = false;           // c'è un picco precedente, chiuso a `tPicco`
  uint32_t tPicco = 0;
  bool inPausa = false;
  uint32_t tScossone = 0;
};
