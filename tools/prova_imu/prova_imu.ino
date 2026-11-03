// Prova del sensore di movimento QMI8658.
//
// Sketch autonomo, in una cartella propria perché non entri nella build di PokeGotchi. Serve a
// decidere, PRIMA di scrivere i minigiochi a movimento, se l'accelerometro della
// Waveshare ESP32-S3-Touch-AMOLED-1.75 si legge bene insieme al touch sullo stesso bus I2C,
// come sono orientati i suoi assi rispetto allo schermo, e se il suo motore di «tocco»
// hardware riconosce il doppio colpetto sulla scocca. Lo schermo e il touch si creano come
// nel gioco (hal_esp32.cpp); il sensore si legge con Wire direttamente, a registri, come farà
// il gioco: il driver di SensorLib non rispetta il timeout di Wire (vedi hal_esp32.cpp).
// Registri dal datasheet «QMI8658A Datasheet Rev A» (CTRL1-9, STATUS0/1, TAP_STATUS, CTRL9).
//
// COMPILARE E CARICARE
//   FQBN="esp32:esp32:esp32s3:CDCOnBoot=cdc,FlashSize=16M,PSRAM=opi,PartitionScheme=app3M_fat9M_16MB"
//   arduino-cli compile --fqbn "$FQBN" tools/prova_imu
//   arduino-cli upload -p <porta> --fqbn "$FQBN" tools/prova_imu
//   arduino-cli monitor -p <porta> -c baudrate=115200
// Dopo la prova si torna al gioco ricaricando PokeGotchi (o dall'installer web, SENZA «Erase
// device»): questo sketch non scrive nella NVS, la partita resta.
//
// COMANDI (seriale, 115200, una riga ciascuno; il nome della prova in corso è in alto sullo
// schermo, i comandi senza argomenti si possono scrivere anche minuscoli)
//   LIVELLA  RUMORE  PICCO  COLPI  TOUCH     passano alla prova (vedi sotto)
//   SCAN                bus I2C, WHO_AM_I, livello dei pin INT1/INT2
//   AZZERA              azzera picchi e contatori della prova in corso
//   STATO               configurazione attuale       AIUTO   questo elenco, in breve
//   ASSI <d> <b>        mappa degli assi per la livella: <d> è il valore che CRESCE quando
//                       abbassi il lato destro dello schermo, <b> quello che cresce quando
//                       abbassi il lato in basso; ognuno tra +X -X +Y -Y +Z -Z (iniziale +X +Y)
//   SCOSSA <g>          soglia dello scossone su |a| (iniziale 2.5)
//   ODR <Hz>            frequenza del sensore: 31 62 125 250 500 1000 (ogni prova ha la sua:
//                       125, COLPI 500; vale fino al cambio di prova)
//   FS <g>              fondo scala: 2 4 8 16 (iniziale 8)
//   LETTURA <ms>        ogni quanto si legge il sensore (iniziale 8 = 125 letture/s)
//   TASK ON|OFF         il sensore si legge da un task sul core 0 (iniziale) o dal loop,
//                       che disegna e ne legge solo ~14 al secondo: «persi=» lo mostra
//   I2C <kHz>           velocità del bus (iniziale quella del gioco, 100)
//   MUTEX ON|OFF        ogni transazione I2C (sensore e touch) sotto un mutex (iniziale ON;
//                       OFF mostra i campioni corrotti quando task e loop si incrociano)
//   FORZA ON|OFF        touch letto ogni 20 ms anche senza dito, come col dito sempre giù
//                       (il caso peggiore per il bus; per la prova 5 senza nessuno davanti)
//   FINESTRE <p> <t> <d>  motore di tocco, in ms: durata massima del picco, quiete dopo il
//                         primo colpo, tempo massimo per il secondo (iniziali 40 100 500)
//   SOGLIE <picco> <quiete>  motore di tocco, in g^2 (iniziali 0.8 0.4, i valori d'esempio
//                            del datasheet); dopo FINESTRE/SOGLIE si riconfigura da solo
//
// RIGA DI STATISTICHE (ogni 10 s, in tutte le prove; azzerata da AZZERA e all'inizio di TOUCH)
//   campioni= persi=     campioni letti e campioni che il loop non ha fatto in tempo a
//                        leggere (dal contatore del sensore; solo indicativo)
//   ko_i2c=              letture/scritture del sensore fallite (NACK o timeout)
//   ko_touch=            letture del touch in cui il CST9217 non risponde
//   tocchi=              pressioni del dito sullo schermo
//   blocco=              l'operazione I2C più lunga (sensore o touch), in ms
//   loop=                il giro del loop più lungo, in ms (comprende il disegno)
//   giri=                giri del loop (ognuno ridisegna lo schermo intero)
//
// LE PROVE
//   0. SCAN (parte da solo all'avvio). Annota l'indirizzo del QMI8658 e il WHO_AM_I
//      (atteso 0x05) -> riga «Indirizzo e WHO_AM_I». Se il sensore non risponde, fermati qui.
//   1. LIVELLA (la prova iniziale). Scheda in piano, poi abbassa il lato DESTRO dello schermo
//      di circa 30 gradi: guarda quale dei tre valori x, y, z (in g, sullo schermo e sulla
//      seriale) cambia di più e con che segno -> riga «Inclinata verso destra», ad esempio
//      «Y, negativo». Poi abbassa il lato in BASSO -> riga «Inclinata verso il basso».
//      Conferma con ASSI (es. «ASSI -Y +X»): la pallina deve scendere verso il lato più
//      basso. Scrivi in Note il comando ASSI giusto.
//   2. RUMORE: scheda ferma sul tavolo, non toccarla per 10 s. Stampa la deviazione standard
//      di x, y, z e |a| in mg -> riga «Rumore a riposo» (la più grande delle tre, e |a|).
//   3. PICCO: misura il picco di |a| (g) da quando parte o da AZZERA.
//      a) scuoti forte la scheda in mano per 3 s, annota «picco=» -> riga «Scossone forte»;
//      b) AZZERA, poi cammina 30 s con la scheda in mano (come si tiene giocando) -> riga
//         «Camminata». In Note quanti «scossone» ha contato con la soglia di 2.5 g.
//   4. COLPI: il motore di tocco del sensore (a 500 Hz). Ogni colpetto riconosciuto stampa
//      una riga «colpo SINGOLO|DOPPIO asse= verso=»; lo schermo conta singoli, doppi, tocchi
//      del dito e scossoni.
//      a) 10 doppi colpetti con un dito sulla scocca (sul retro o sul bordo), uno ogni 2-3 s
//         -> riga «Doppio colpetto: riconosciuti su 10» (quanti DOPPIO); in Note su quale
//         lato e i singoli in più;
//      b) AZZERA, 20 tocchi normali del dito sullo schermo, uno al secondo -> riga «Tocchi
//         del dito: falsi colpetti su 20» (singoli + doppi; «tocchi=» deve dire 20);
//      c) AZZERA, martella lo schermo con le dita per 10 s come nel sacco -> riga «Sacco
//         martellato» (colpetti singoli/doppi e scossoni).
//      Se i doppi sono pochi, prova FINESTRE e SOGLIE (es. «SOGLIE 0.5 0.3», «FINESTRE 40 80
//      600») e annota in Note i valori migliori. «int2=» conta i fronti del pin INT2 (GPIO21):
//      se cresce con i colpetti, il pin INT2 funziona -> Note della riga INT1/INT2.
//   5. TOUCH: 2 minuti di prova del bus, con il touch letto ogni 20 ms come nel gioco e il
//      sensore a 125 Hz. Per tutta la durata trascina e tocca lo schermo (anche tocchi rapidi
//      e ravvicinati, come sulla palla). Alla fine stampa «FINE TOUCH» con ko_i2c, ko_touch,
//      blocco e loop -> riga «Touch + sensore per 2 min» (ko_i2c + ko_touch, e blocco).
//   6. INT1 / INT2: dallo schema la riga è già compilata (INT1 -> EXIO6 del TCA9554, INT2 ->
//      GPIO21); aggiungi in Note se int2= cresceva nella prova 4.
//
// ESITO: i giochi a movimento hanno senso solo se WHO_AM_I è giusto, gli assi sono
// ricavati, nessun errore I2C né blocchi oltre 50 ms con il touch (blocco=, non
// loop=), e il doppio colpetto è riconosciuto almeno 8 volte su 10. Se il tocco hardware non funziona, sulla principale
// resta solo lo scossone.

#define XPOWERS_CHIP_AXP2101
#include <Wire.h>
#include <XPowersLib.h>
#include "Arduino_GFX_Library.h"
#include "TouchDrvCSTXXX.hpp"

// pin e indirizzi da pin_config.h e hal_esp32.cpp
#define LCD_SDIO0 4
#define LCD_SDIO1 5
#define LCD_SDIO2 6
#define LCD_SDIO3 7
#define LCD_SCLK 38
#define LCD_CS 12
#define LCD_RESET 39
#define LCD_WIDTH 466
#define LCD_HEIGHT 466
#define IIC_SDA 15
#define IIC_SCL 14
#define TP_INT 11
#define TP_RESET 40
#define TOUCH_ADDR 0x5A   // CST9217
// dallo schema: INT1 del QMI8658 va a EXIO6 dell'espansore TCA9554 (0x20), INT2 a GPIO21
#define QMI_INT2 21
#define TCA_ADDR 0x20

// registri del QMI8658 (datasheet, tabella 22 e seguenti)
#define Q_WHO_AM_I 0x00   // 0x05
#define Q_CTRL1 0x02      // interfaccia: auto-incremento, little-endian, uscite INT
#define Q_CTRL2 0x03      // accelerometro: fondo scala <6:4>, ODR <3:0>
#define Q_CTRL7 0x08      // abilitazioni: aEN bit 0, DRDY_DIS bit 5
#define Q_CTRL8 0x09      // motori: Tap_EN bit 0; bit 6 = 0 eventi su INT2; bit 7 handshake CTRL9 su STATUSINT
#define Q_CTRL9 0x0A      // comandi
#define Q_CAL1_L 0x0B     // CAL1_L .. CAL4_H = 0x0B .. 0x12
#define Q_STATUSINT 0x2D  // bit 7 CmdDone
#define Q_STATUS0 0x2E    // bit 0 accelerometro pronto
#define Q_TAP_STATUS 0x59 // <1:0> numero (1 singolo, 2 doppio), <5:4> asse, bit 7 verso negativo
#define Q_RESET 0x60      // scrivere 0xB0
#define Q_RST_RESULT 0x4D // 0x80 a reset finito
#define CMD_ACK 0x00
#define CMD_TAP 0x0C

#define STAT_MS 10000     // riga di statistiche
#define DISEGNO_MS 50     // 20 fotogrammi al secondo, come il gioco al massimo
#define TOUCH_MS 20       // touch letto ogni 20 ms
#define RUMORE_MS 10000
#define TOUCH_PROVA_MS 120000

Arduino_DataBus *bus = new Arduino_ESP32QSPI(
  LCD_CS, LCD_SCLK, LCD_SDIO0, LCD_SDIO1, LCD_SDIO2, LCD_SDIO3);
Arduino_CO5300 *panel = new Arduino_CO5300(
  bus, LCD_RESET, 0 /*rotation*/, LCD_WIDTH, LCD_HEIGHT, 6, 0, 0, 0);
Arduino_Canvas *gfx = new Arduino_Canvas(LCD_WIDTH, LCD_HEIGHT, panel);
static XPowersPMU pmu;
static TouchDrvCST92xx touch;
static volatile bool touchIrq = false;
static volatile uint32_t int2Fronti = 0;  // scritto solo dalla ISR
static void IRAM_ATTR touchIsr() { touchIrq = true; }
static void IRAM_ATTR int2Isr() { int2Fronti = int2Fronti + 1; }

enum Modo : uint8_t { M_LIVELLA, M_RUMORE, M_PICCO, M_COLPI, M_TOUCH };
static const char *NOMI[] = { "LIVELLA", "RUMORE", "PICCO", "COLPI", "TOUCH" };
static Modo modo = M_LIVELLA;

// configurazione
static uint8_t qAddr = 0;            // 0x6B (SA0 a massa sullo schema) o 0x6A; 0 = assente
static uint16_t odrHz = 125;
static uint8_t fsG = 8;
static uint8_t letturaMs = 8;
static bool usaMutex = true;         // MUTEX ON/OFF: transazioni I2C intere sotto un mutex
static SemaphoreHandle_t busMutex;
static bool forzaTouch = false;      // FORZA ON: touch letto ogni 20 ms anche senza dito
static bool usaTask = true;          // TASK ON/OFF: sensore letto dal task o dal loop
static volatile bool pausa = false;  // il task non legge mentre si riconfigura
static volatile uint32_t scartaFino;  // millis(): i primi campioni dopo l'accensione sono spuri
static int8_t assiD = 1, assiB = 2;  // +1..+3 = +X..+Z, -1..-3 = -X..-Z
static float sogliaScossa = 2.5f;
static uint16_t finPicco = 40, finQuiete = 100, finDoppio = 500;  // ms
static float sogliaPicco = 0.8f, sogliaQuiete = 0.4f;            // g^2

// misure
static float ax, ay, az, amag;       // ultimo campione, in g
static uint32_t ultimoTs = 0;
static bool tsValido = false;
static uint32_t campioni, persi, koI2c, koTouch, letTouch, tocchi, scossoni;
static uint32_t bloccoUs, loopUs, giri;
static float picco;
static uint32_t ultimoScossone;
static uint32_t singoli, doppi;
static uint32_t int2Base;
static double sx, sy, sz, sm, qx, qy, qz, qm;
static uint32_t nRumore;
static uint32_t inizioModo;
static bool modoFinito;
static bool dito = false;
static int16_t ditoX, ditoY;
static char ultimoColpo[40] = "";

// ---------------------------------------------------------------- I2C del sensore

// Misurato sulla scheda (31/10): con il sensore letto dal task e il touch dal loop, Wire da
// solo non basta, ~2 % dei campioni arriva corrotto (scossoni falsi da ~7 g) ed escono errori
// I2C. Ogni transazione intera (scrittura del registro + lettura) va sotto un mutex.
struct Bus {
  Bus() { if (usaMutex && busMutex) xSemaphoreTake(busMutex, portMAX_DELAY); }
  ~Bus() { if (usaMutex && busMutex) xSemaphoreGive(busMutex); }
};

static void notaBlocco(uint32_t t0) {
  uint32_t d = micros() - t0;
  if (d > bloccoUs) bloccoUs = d;
}

static bool qLeggi(uint8_t reg, uint8_t *buf, uint8_t n) {
  uint32_t t0 = micros();
  Bus b;
  Wire.beginTransmission(qAddr);
  Wire.write(reg);
  bool ok = Wire.endTransmission(false) == 0 && Wire.requestFrom(qAddr, (size_t)n) == n;
  if (ok) for (uint8_t i = 0; i < n; i++) buf[i] = Wire.read();
  else { koI2c++; while (Wire.available()) Wire.read(); }
  notaBlocco(t0);
  return ok;
}

static bool qScrivi(uint8_t reg, uint8_t v) {
  uint32_t t0 = micros();
  Bus b;
  Wire.beginTransmission(qAddr);
  Wire.write(reg);
  Wire.write(v);
  bool ok = Wire.endTransmission() == 0;
  if (!ok) koI2c++;
  notaBlocco(t0);
  return ok;
}

static uint8_t leggiByte(uint8_t addr, uint8_t reg, bool *ok) {
  Bus b;
  Wire.beginTransmission(addr);
  Wire.write(reg);
  *ok = Wire.endTransmission(false) == 0 && Wire.requestFrom(addr, (size_t)1) == 1;
  return *ok ? Wire.read() : 0;
}

// Protocollo CTRL9 (datasheet 5.10.4): comando, attesa di CmdDone in STATUSINT (CTRL8 bit 7
// = 1), ACK. Al massimo 100 ms, così non si blocca come il driver.
static bool qComando(uint8_t cmd) {
  if (!qScrivi(Q_CTRL9, cmd)) return false;
  uint32_t t0 = millis();
  uint8_t s = 0;
  bool fatto = false;
  while (millis() - t0 < 100) {
    if (qLeggi(Q_STATUSINT, &s, 1) && (s & 0x80)) { fatto = true; break; }
    delay(1);
  }
  qScrivi(Q_CTRL9, CMD_ACK);
  return fatto;
}

static uint8_t codiceOdr(uint16_t hz) {  // aODR in modalità solo accelerometro
  switch (hz) {
    case 1000: return 3; case 500: return 4; case 250: return 5;
    case 62: return 7; case 31: return 8; default: return 6;  // 125
  }
}

static uint8_t codiceFs(uint8_t g) { return g == 2 ? 0 : g == 4 ? 1 : g == 16 ? 3 : 2; }

static float lsbPerG() { return 32768.0f / fsG; }

static uint16_t campioniDa(uint16_t ms) {
  uint32_t n = (uint32_t)ms * odrHz / 1000;
  return n ? n : 1;
}

// Motore di tocco (datasheet cap. 10): due serie di parametri nei CAL, con CMD_TAP ciascuna,
// a sensori spenti. Finestre in campioni, soglie in g^2 con 1/1000 (l'esempio 0.8 g^2 =
// 0x0320 del datasheet).
static bool configuraTocco() {
  uint16_t p = campioniDa(finPicco), t = campioniDa(finQuiete), d = campioniDa(finDoppio);
  if (p > 255) p = 255;
  uint16_t sp = (uint16_t)(sogliaPicco * 1000), sq = (uint16_t)(sogliaQuiete * 1000);
  bool ok = qScrivi(Q_CAL1_L, p) && qScrivi(Q_CAL1_L + 1, 0)  // priorità X > Y > Z
    && qScrivi(Q_CAL1_L + 2, t & 0xFF) && qScrivi(Q_CAL1_L + 3, t >> 8)
    && qScrivi(Q_CAL1_L + 4, d & 0xFF) && qScrivi(Q_CAL1_L + 5, d >> 8)
    && qScrivi(Q_CAL1_L + 7, 0x01) && qComando(CMD_TAP);
  ok = ok && qScrivi(Q_CAL1_L, 8) && qScrivi(Q_CAL1_L + 1, 32)  // alfa 0.0625, gamma 0.25 (x128)
    && qScrivi(Q_CAL1_L + 2, sp & 0xFF) && qScrivi(Q_CAL1_L + 3, sp >> 8)
    && qScrivi(Q_CAL1_L + 4, sq & 0xFF) && qScrivi(Q_CAL1_L + 5, sq >> 8)
    && qScrivi(Q_CAL1_L + 7, 0x02) && qComando(CMD_TAP);
  return ok;
}

// Configura il sensore per la prova in corso: solo accelerometro, niente giroscopio.
static bool configuraSensore() {
  if (!qAddr) return false;
  pausa = true;
  delay(letturaMs + 2);                          // lascia finire la lettura in corso del task
  bool colpi = modo == M_COLPI;
  bool ok = qScrivi(Q_CTRL7, 0x00)               // tutto spento mentre si configura
    && qScrivi(Q_CTRL1, 0x58)                    // auto-incremento, little-endian, INT1 e INT2 attivi
    && qScrivi(Q_CTRL8, 0x80)                    // handshake CTRL9 su STATUSINT, motori spenti
    && qScrivi(Q_CTRL2, (codiceFs(fsG) << 4) | codiceOdr(odrHz));
  if (ok && colpi) ok = configuraTocco();
  ok = ok && qScrivi(Q_CTRL7, 0x21)              // accelerometro acceso, DRDY tolto da INT2
    && (!colpi || qScrivi(Q_CTRL8, 0x81));       // motore di tocco, eventi su INT2 (GPIO21)
  tsValido = false;
  scartaFino = millis() + 100;                   // visto sulla scheda: un campione da ~14 g
  pausa = false;
  Serial.printf("sensore: %s, %u Hz, +-%u g%s\n", ok ? "ok" : "ERRORE", odrHz, fsG,
                colpi ? ", motore di tocco acceso" : "");
  return ok;
}

static bool iniziaSensore() {
  bool ok;
  for (uint8_t a : { 0x6B, 0x6A }) {
    if (leggiByte(a, Q_WHO_AM_I, &ok) == 0x05 && ok) { qAddr = a; break; }
  }
  if (!qAddr) return false;
  qScrivi(Q_RESET, 0xB0);
  delay(15);                                     // durante il reset il chip non risponde
  uint32_t t0 = millis();
  uint8_t r = 0;
  while (millis() - t0 < 500 && !(qLeggi(Q_RST_RESULT, &r, 1) && r == 0x80)) delay(10);
  koI2c = 0;                                     // i NACK del reset non contano
  return configuraSensore();
}

// ---------------------------------------------------------------- misure

static float asse(int8_t a) {
  float v = (a == 1 || a == -1) ? ax : (a == 2 || a == -2) ? ay : az;
  return a < 0 ? -v : v;
}

static const char *nomeAsse(int8_t a) {
  static const char *N[] = { "-Z", "-Y", "-X", "?", "+X", "+Y", "+Z" };
  return N[a + 3];
}

static void azzera() {
  campioni = persi = koI2c = koTouch = letTouch = tocchi = scossoni = 0;
  bloccoUs = loopUs = giri = 0;
  picco = 0;
  singoli = doppi = 0;
  int2Base = int2Fronti;
  sx = sy = sz = sm = qx = qy = qz = qm = 0;
  nRumore = 0;
  ultimoColpo[0] = 0;
}

static void nuovoCampione() {
  campioni++;
  if (amag > picco) picco = amag;
  uint32_t ora = millis();
  if (amag > sogliaScossa && ora - ultimoScossone > 300) {
    ultimoScossone = ora;
    scossoni++;
    Serial.printf("scossone |a|=%.2f g\n", amag);
  }
  if (modo == M_RUMORE && !modoFinito) {
    sx += ax; sy += ay; sz += az; sm += amag;
    qx += ax * ax; qy += ay * ay; qz += az * az; qm += amag * amag;
    nRumore++;
  }
}

// Una lettura sola da STATUS0 a AZ_H (0x2E..0x3A): stato, stato delle attività, contatore
// dei campioni, temperatura, accelerazioni.
static void leggiSensore() {
  uint8_t b[13];
  if (!qLeggi(Q_STATUS0, b, sizeof(b))) return;
  uint32_t ts = b[2] | (b[3] << 8) | ((uint32_t)b[4] << 16);
  bool nuovo = b[0] & 0x01;
  if (tsValido && ts != ultimoTs) {
    nuovo = true;
    uint32_t d = (ts - ultimoTs) & 0xFFFFFF;
    // i tre byte non si leggono insieme: un riporto a metà lettura sembra un salto di 256
    if (d > 1 && d < 200) persi += d - 1;
  }
  if (ts != ultimoTs || !tsValido) { ultimoTs = ts; tsValido = true; }
  if (b[1] & 0x02) {                             // STATUS1: colpetto (si azzera leggendolo)
    uint8_t t;
    if (!qLeggi(Q_TAP_STATUS, &t, 1)) {
    } else if ((int32_t)(millis() - scartaFino) < 900) {
      // appena acceso il motore parte con la media a zero e vede un colpo (asse Z): non conta
      Serial.println("colpo ignorato (motore appena acceso)");
      int2Base = int2Fronti;
    } else {
      uint8_t n = t & 0x03;
      if (n == 1) singoli++;
      if (n == 2) doppi++;
      const char *assi[] = { "?", "X", "Y", "Z" };
      snprintf(ultimoColpo, sizeof(ultimoColpo), "%s asse=%s verso=%c",
               n == 2 ? "DOPPIO" : n == 1 ? "SINGOLO" : "?", assi[(t >> 4) & 3],
               (t & 0x80) ? '-' : '+');
      Serial.printf("colpo %s\n", ultimoColpo);
    }
  }
  if (!nuovo || (int32_t)(millis() - scartaFino) < 0) return;
  float k = lsbPerG();
  ax = (int16_t)(b[7] | (b[8] << 8)) / k;
  ay = (int16_t)(b[9] | (b[10] << 8)) / k;
  az = (int16_t)(b[11] | (b[12] << 8)) / k;
  amag = sqrtf(ax * ax + ay * ay + az * az);
  nuovoCampione();
}

// Come halTouchRead del gioco: si tocca il bus solo dopo l'INT o col dito giù, e prima un
// ciclo di solo indirizzo che sveglia il CST9217.
static void leggiTouch() {
  if (!touchIrq && !dito && !forzaTouch) return;
  touchIrq = false;
  uint32_t t0 = micros();
  letTouch++;
  Bus b;
  Wire.beginTransmission(TOUCH_ADDR);
  if (Wire.endTransmission() != 0) { koTouch++; notaBlocco(t0); return; }
  int16_t x, y;
  bool giu = touch.getPoint(&x, &y, 1) > 0;
  notaBlocco(t0);
  if (giu && !dito) tocchi++;
  dito = giu;
  if (giu) { ditoX = x; ditoY = y; }
}

// ---------------------------------------------------------------- prove

static void stampaStat(const char *pre) {
  Serial.printf("%scampioni=%lu persi=%lu ko_i2c=%lu ko_touch=%lu letture_touch=%lu tocchi=%lu "
                "blocco=%.1f ms loop=%.1f ms giri=%lu scossoni=%lu int2=%lu\n", pre,
                (unsigned long)campioni, (unsigned long)persi, (unsigned long)koI2c,
                (unsigned long)koTouch, (unsigned long)letTouch, (unsigned long)tocchi,
                bloccoUs / 1000.0f, loopUs / 1000.0f, (unsigned long)giri, (unsigned long)scossoni,
                (unsigned long)(int2Fronti - int2Base));
}

static void stampaRumore() {
  if (nRumore < 2) { Serial.println("RUMORE: nessun campione"); return; }
  double n = nRumore;
  auto ds = [n](double s, double q) { double v = q / n - (s / n) * (s / n); return v > 0 ? sqrt(v) * 1000 : 0; };
  Serial.printf("FINE RUMORE (%lu campioni) dev. std in mg: x=%.1f y=%.1f z=%.1f |a|=%.1f; "
                "media in g: x=%+.3f y=%+.3f z=%+.3f\n", (unsigned long)nRumore,
                ds(sx, qx), ds(sy, qy), ds(sz, qz), ds(sm, qm), sx / n, sy / n, sz / n);
}

static void vaiModo(uint8_t m) {  // uint8_t: i prototipi generati da Arduino non vedono Modo
  modo = (Modo)m;
  odrHz = m == M_COLPI ? 500 : 125;
  modoFinito = false;
  configuraSensore();
  azzera();
  inizioModo = millis();
  Serial.printf("prova %s\n", NOMI[m]);
  if (m == M_RUMORE) Serial.println("non toccare la scheda per 10 s");
  if (m == M_TOUCH) Serial.println("2 minuti: tocca e trascina lo schermo");
}

static void scan() {
  Serial.printf("bus I2C a %lu Hz, indirizzi:", (unsigned long)Wire.getClock());
  for (uint8_t a = 1; a < 127; a++) {
    Wire.beginTransmission(a);
    if (Wire.endTransmission() == 0) {
      const char *n = a == 0x5A ? "touch" : a == 0x34 ? "PMU" : a == 0x51 ? "RTC"
        : a == 0x18 ? "ES8311" : a == 0x20 ? "TCA9554" : (a == 0x6B || a == 0x6A) ? "QMI8658" : "";
      Serial.printf(" 0x%02X%s%s", a, *n ? " " : "", n);
    }
  }
  Serial.println();
  bool ok;
  for (uint8_t a : { 0x6B, 0x6A }) {
    uint8_t w = leggiByte(a, Q_WHO_AM_I, &ok);
    if (ok) {
      uint8_t rev = leggiByte(a, 0x01, &ok);
      Serial.printf("0x%02X: WHO_AM_I=0x%02X (atteso 0x05) revisione=0x%02X\n", a, w, rev);
    }
  }
  uint8_t tca = leggiByte(TCA_ADDR, 0x00, &ok);
  Serial.printf("INT2 (GPIO21)=%d  INT1 (EXIO6 del TCA9554)=%s\n", digitalRead(QMI_INT2),
                ok ? ((tca & 0x40) ? "1" : "0") : "TCA9554 assente");
  Serial.printf("QMI8658 in uso: %s\n", qAddr ? (qAddr == 0x6B ? "0x6B" : "0x6A") : "NESSUNO");
}

static void stato() {
  Serial.printf("prova %s, %u Hz, +-%u g, lettura ogni %u ms dal %s, I2C %lu Hz%s\n",
                NOMI[modo], odrHz, fsG, letturaMs, usaTask ? "task" : "loop",
                (unsigned long)Wire.getClock(), forzaTouch ? ", touch forzato" : "");
  Serial.printf("mutex I2C %s\n", usaMutex ? "ON" : "OFF");
  Serial.printf("ASSI %s %s, SCOSSA %.2f, FINESTRE %u %u %u, SOGLIE %.2f %.2f\n",
                nomeAsse(assiD), nomeAsse(assiB), sogliaScossa, finPicco, finQuiete, finDoppio,
                sogliaPicco, sogliaQuiete);
}

static int8_t leggiAsse(const char *s) {
  if (!s || strlen(s) != 2 || (s[0] != '+' && s[0] != '-')) return 0;
  char c = toupper(s[1]);
  int8_t a = c == 'X' ? 1 : c == 'Y' ? 2 : c == 'Z' ? 3 : 0;
  return s[0] == '-' ? -a : a;
}

static void comando(char *riga) {
  for (char *p = riga; *p && *p != ' '; p++) *p = toupper(*p);
  char *cmd = strtok(riga, " ");
  char *a1 = strtok(nullptr, " "), *a2 = strtok(nullptr, " "), *a3 = strtok(nullptr, " ");
  if (!cmd) return;
  for (uint8_t m = 0; m < 5; m++) if (!strcmp(cmd, NOMI[m])) { vaiModo(m); return; }
  if (!strcmp(cmd, "SCAN")) scan();
  else if (!strcmp(cmd, "AZZERA")) { azzera(); inizioModo = millis(); modoFinito = false; Serial.println("azzerato"); }
  else if (!strcmp(cmd, "STATO")) stato();
  else if (!strcmp(cmd, "ASSI") && leggiAsse(a1) && leggiAsse(a2)) {
    assiD = leggiAsse(a1); assiB = leggiAsse(a2); stato();
  } else if (!strcmp(cmd, "SCOSSA") && a1) { sogliaScossa = atof(a1); stato(); }
  else if (!strcmp(cmd, "ODR") && a1) { odrHz = atoi(a1); if (codiceOdr(odrHz) == 6) odrHz = 125; configuraSensore(); }
  else if (!strcmp(cmd, "FS") && a1) { fsG = atoi(a1); if (codiceFs(fsG) == 2) fsG = 8; configuraSensore(); }
  else if (!strcmp(cmd, "MUTEX") && a1) { usaMutex = !strcmp(a1, "ON") || !strcmp(a1, "on"); stato(); }
  else if (!strcmp(cmd, "FORZA") && a1) { forzaTouch = !strcmp(a1, "ON") || !strcmp(a1, "on"); stato(); }
  else if (!strcmp(cmd, "TASK") && a1) { usaTask = !strcmp(a1, "ON") || !strcmp(a1, "on"); stato(); }
  else if (!strcmp(cmd, "LETTURA") && a1) { letturaMs = constrain(atoi(a1), 1, 100); stato(); }
  else if (!strcmp(cmd, "I2C") && a1) { Wire.setClock(constrain(atoi(a1), 50, 1000) * 1000UL); stato(); }
  else if (!strcmp(cmd, "FINESTRE") && a3) {
    finPicco = atoi(a1); finQuiete = atoi(a2); finDoppio = atoi(a3); configuraSensore(); stato();
  } else if (!strcmp(cmd, "SOGLIE") && a2) {
    sogliaPicco = atof(a1); sogliaQuiete = atof(a2); configuraSensore(); stato();
  } else if (!strcmp(cmd, "AIUTO")) {
    Serial.println("LIVELLA RUMORE PICCO COLPI TOUCH | SCAN AZZERA STATO | ASSI <d> <b> "
                   "SCOSSA <g> ODR <Hz> FS <g> LETTURA <ms> TASK ON|OFF MUTEX ON|OFF FORZA ON|OFF I2C <kHz> FINESTRE <p> <t> <d> "
                   "SOGLIE <picco> <quiete>");
  } else Serial.println("ERR (AIUTO per i comandi)");
}

static void leggiSeriale() {
  static char riga[64];
  static uint8_t n = 0;
  while (Serial.available()) {
    char c = Serial.read();
    if (c == '\r' || c == '\n') {
      riga[n] = 0;
      if (n) comando(riga);
      n = 0;
    } else if (n < sizeof(riga) - 1) riga[n++] = c;
  }
}

static void aggiornaModo() {
  uint32_t t = millis() - inizioModo;
  static uint32_t ultimaStampa = 0;
  if (modoFinito) return;
  if (modo == M_RUMORE && t >= RUMORE_MS) { modoFinito = true; stampaRumore(); }
  if (modo == M_TOUCH && t >= TOUCH_PROVA_MS) { modoFinito = true; stampaStat("FINE TOUCH "); }
  if (millis() - ultimaStampa < (modo == M_LIVELLA ? 500u : modo == M_PICCO ? 1000u : 2000u)) return;
  ultimaStampa = millis();
  if (modo == M_LIVELLA)
    Serial.printf("x=%+.3f y=%+.3f z=%+.3f |a|=%.3f g\n", ax, ay, az, amag);
  else if (modo == M_PICCO)
    Serial.printf("picco=%.2f g  ora=%.2f g  scossoni=%lu (soglia %.1f g)\n", picco, amag,
                  (unsigned long)scossoni, sogliaScossa);
  else if (modo == M_COLPI)
    Serial.printf("singoli=%lu doppi=%lu tocchi=%lu scossoni=%lu int2=%lu\n",
                  (unsigned long)singoli, (unsigned long)doppi, (unsigned long)tocchi,
                  (unsigned long)scossoni, (unsigned long)(int2Fronti - int2Base));
}

// ---------------------------------------------------------------- schermo

#define NERO 0x0000
#define BIANCO 0xFFFF
#define GRIGIO 0x7BEF
#define VERDE 0x07E0
#define GIALLO 0xFFE0
#define ROSSO 0xF800
#define CX (LCD_WIDTH / 2)
#define CY (LCD_HEIGHT / 2)

static void testo(int y, uint8_t dim, uint16_t col, const char *fmt, ...) {
  char buf[48];
  va_list ap;
  va_start(ap, fmt);
  vsnprintf(buf, sizeof(buf), fmt, ap);
  va_end(ap);
  gfx->setTextSize(dim);
  gfx->setTextColor(col);
  gfx->setCursor(CX - (int)strlen(buf) * 3 * dim, y);
  gfx->print(buf);
}

static void disegna() {
  gfx->fillScreen(NERO);
  // livella: un cerchio di 1 g, la pallina va verso il lato più basso
  const int R = 150;
  gfx->drawCircle(CX, CY, R, GRIGIO);
  gfx->drawCircle(CX, CY, R / 2, GRIGIO);
  gfx->drawFastHLine(CX - R, CY, 2 * R, GRIGIO);
  gfx->drawFastVLine(CX, CY - R, 2 * R, GRIGIO);
  float bx = constrain(asse(assiD), -1.2f, 1.2f), by = constrain(asse(assiB), -1.2f, 1.2f);
  gfx->fillCircle(CX + (int)(bx * R), CY + (int)(by * R), 18, amag > sogliaScossa ? ROSSO : VERDE);
  if (dito) gfx->drawCircle(ditoX, ditoY, 26, GIALLO);

  testo(40, 3, BIANCO, "%s", NOMI[modo]);
  testo(74, 2, GRIGIO, "%u Hz  +-%u g  %s %s", odrHz, fsG, nomeAsse(assiD), nomeAsse(assiB));
  testo(100, 2, BIANCO, "x%+.2f y%+.2f z%+.2f", ax, ay, az);
  uint32_t t = (millis() - inizioModo) / 1000;
  switch (modo) {
    case M_LIVELLA:
      testo(330, 2, BIANCO, "|a| %.3f g", amag);
      testo(356, 2, GRIGIO, "abbassa destra, poi basso");
      break;
    case M_RUMORE:
      testo(330, 2, modoFinito ? VERDE : GIALLO, modoFinito ? "fatto: vedi seriale" : "ferma! %lu s",
            (unsigned long)(t < 10 ? 10 - t : 0));
      break;
    case M_PICCO:
      testo(320, 3, BIANCO, "picco %.2f g", picco);
      testo(356, 2, GRIGIO, "scossoni %lu (>%.1f g)", (unsigned long)scossoni, sogliaScossa);
      break;
    case M_COLPI:
      testo(310, 3, BIANCO, "doppi %lu", (unsigned long)doppi);
      testo(342, 2, BIANCO, "singoli %lu  dito %lu", (unsigned long)singoli, (unsigned long)tocchi);
      testo(366, 2, GRIGIO, "%s", ultimoColpo);
      testo(390, 2, GRIGIO, "scossoni %lu int2 %lu", (unsigned long)scossoni,
            (unsigned long)(int2Fronti - int2Base));
      break;
    case M_TOUCH:
      testo(310, 3, modoFinito ? VERDE : BIANCO, modoFinito ? "FINE" : "%lu s", (unsigned long)t);
      testo(342, 2, (koI2c || koTouch) ? ROSSO : BIANCO, "ko i2c %lu  touch %lu",
            (unsigned long)koI2c, (unsigned long)koTouch);
      testo(366, 2, bloccoUs > 50000 ? ROSSO : BIANCO, "blocco %.1f ms", bloccoUs / 1000.0f);
      testo(390, 2, GRIGIO, "loop %.1f ms tocchi %lu", loopUs / 1000.0f, (unsigned long)tocchi);
      break;
  }
  if (!qAddr) testo(220, 3, ROSSO, "QMI8658 assente");
  gfx->flush();
}

// ---------------------------------------------------------------- setup e loop

// Il sensore letto da un task sul core 0, ogni letturaMs, mentre il loop disegna: il fill e
// il flush del framebuffer tengono il loop ~70 ms a giro, e dal loop si leggerebbero ~14
// campioni al secondo (TASK OFF per vederlo). Wire serializza da sé i due core.
static void taskSensore(void *) {
  TickType_t t = xTaskGetTickCount();
  for (;;) {
    if (usaTask && qAddr && !pausa) leggiSensore();
    vTaskDelayUntil(&t, pdMS_TO_TICKS(letturaMs));
  }
}

void setup() {
  Serial.begin(115200);
  Serial.setTxTimeoutMs(0);
  busMutex = xSemaphoreCreateMutex();
  Wire.begin(IIC_SDA, IIC_SCL);
  Wire.setTimeOut(50);
  // BLDO1 = alimentazione del pannello, prima di gfx->begin() (vedi pmuEnablePanel)
  if (pmu.begin(Wire, AXP2101_SLAVE_ADDRESS, IIC_SDA, IIC_SCL)) {
    pmu.setBLDO1Voltage(3300);
    pmu.enableBLDO1();
  } else Serial.println("AXP2101 non rilevato");
  if (!gfx->begin(80000000)) Serial.println("gfx->begin() fallito");
  panel->setBrightness(180);

  touch.setPins(TP_RESET, TP_INT);
  bool touchOk = false;
  for (int i = 0; i < 3 && !touchOk; i++) {
    touchOk = touch.begin(Wire, TOUCH_ADDR, IIC_SDA, IIC_SCL);
    if (!touchOk) delay(150);
  }
  if (!touchOk) Serial.println("CST9217 non rilevato");
  touch.reset();
  touch.setMaxCoordinates(LCD_WIDTH, LCD_HEIGHT);
  touch.setMirrorXY(true, true);
  pinMode(TP_INT, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(TP_INT), touchIsr, FALLING);
  pinMode(QMI_INT2, INPUT);
  attachInterrupt(digitalPinToInterrupt(QMI_INT2), int2Isr, CHANGE);

  delay(500);  // il tempo di aprire il monitor seriale
  Serial.println("\nprova_imu: QMI8658 + touch (AIUTO per i comandi)");
  if (!iniziaSensore()) Serial.println("QMI8658 NON trovato a 0x6B ne' a 0x6A");
  scan();
  vaiModo(M_LIVELLA);
  xTaskCreatePinnedToCore(taskSensore, "imu", 4096, nullptr, 2, nullptr, 0);
}

void loop() {
  static uint32_t tSensore = 0, tTouch = 0, tDisegno = 0, tStat = 0;
  uint32_t t0 = micros();
  uint32_t ora = millis();
  leggiSeriale();
  if (ora - tTouch >= TOUCH_MS) { tTouch = ora; leggiTouch(); }
  if (qAddr && !usaTask && ora - tSensore >= letturaMs) { tSensore = ora; leggiSensore(); }
  aggiornaModo();
  if (ora - tStat >= STAT_MS) { tStat = ora; stampaStat(""); }
  if (ora - tDisegno >= DISEGNO_MS) { tDisegno = ora; disegna(); }
  uint32_t d = micros() - t0;
  if (d > loopUs) loopUs = d;
  giri++;
}
