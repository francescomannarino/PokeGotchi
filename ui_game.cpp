// Minigiochi: pokéball da tenere in aria e sacco da allenamento.

#include "ui_common.h"

uint32_t gameOverUntil = 0;
float ballX, ballY, ballVX, ballVY, gamePetX;
uint32_t lastGameStep = 0;  // ultima chiamata a stepGame(): fisica a tempo reale, non per frame
uint8_t gameScore, gameMisses;
float hitX, hitY;             // ultimo colpo (anello d'impatto)
uint32_t hitTime = 0;
bool gameNewHi = false;
uint32_t sackUntil = 0, sackOverUntil = 0;
uint16_t sackHits = 0;
float sackShake = 0;
uint8_t sackGain = 0;
bool sackNewHi = false;

// ---------- minigioco: tocchi con la pokéball ----------

void startGame() {
  if (pet.isEgg() || pet.sleeping || pet.ceremony) return;
  goScreen(SCR_GAME);
}

void gameEnter() {
  gameOverUntil = 0;
  gameScore = 0;
  gameMisses = 0;
  gameNewHi = false;
  hitTime = 0;
  gamePetX = 233;
  lastGameStep = millis();
  respawnBall();
}

void respawnBall() {
  ballX = 150 + random(166);
  ballY = 96;
  float sp = 1.6f + gameScore * 0.05f;  // più vivace man mano che avanzi
  if (sp > 4.0f) sp = 4.0f;
  ballVX = random(2) ? sp : -sp;
  ballVY = 0;
}

void gameTap(int16_t x, int16_t y) {
  if (gameOverUntil) return;
  if (y < 72) {  // toccare l'intestazione = abbandonare senza premio
    goScreen(SCR_MAIN);
    return;
  }
  float dx = ballX - x, dy = ballY - y;
  if (dx * dx + dy * dy < 74 * 74) {  // tocco alla palla!
    gameScore++;
    halSfx(SFX_PLAY);
    // colpo più morbido: impulso moderato che cresce poco a poco con il punteggio
    float lift = 6.6f + (gameScore > 16 ? 3.5f : gameScore * 0.22f);
    ballVY = -lift;
    ballVX += dx * 0.12f;
    if (ballVX > 6.5f) ballVX = 6.5f;
    if (ballVX < -6.5f) ballVX = -6.5f;
    hitX = ballX;
    hitY = ballY;
    hitTime = millis();
  }
}

void stepGame() {
  uint32_t now = millis();
  // passo relativo a 85 ms, il periodo di frame con cui furono messe a punto queste
  // costanti (vedi il pianificatore di render in loop()): senza questo la fisica
  // avanzava "un passo per frame" e il gioco andava più veloce o più lento a seconda di
  // quanto ci metteva a disegnarsi il frame precedente, che dipende dalla dimensione
  // dello sprite caricato. Con un Charizard la palla cadeva più lentamente che con un
  // Diglett, quindi il record non era confrontabile tra specie.
  float k = lastGameStep ? (now - lastGameStep) / 85.0f : 1.0f;
  if (k > 3.0f) k = 3.0f;  // frame anormalmente in ritardo: non fare un salto enorme
  lastGameStep = now;

  float grav = 0.40f + gameScore * 0.013f;  // cade un po' più veloce ogni volta
  if (grav > 0.80f) grav = 0.80f;
  ballVY += grav * k;
  ballX += ballVX * k;
  ballY += ballVY * k;
  // rimbalzo sulla parete circolare
  float dx = ballX - CX, dy = ballY - CY;
  float d = sqrtf(dx * dx + dy * dy);
  if (d > 205) {
    float nx = dx / d, ny = dy / d;
    float dot = ballVX * nx + ballVY * ny;
    if (dot > 0) {
      ballVX = (ballVX - 2 * dot * nx) * 0.85f;
      ballVY = (ballVY - 2 * dot * ny) * 0.85f;
    }
    ballX = CX + nx * 205;
    ballY = CY + ny * 205;
  }
  if (ballY > 384) {  // a terra
    if (++gameMisses >= 3) {
      gameNewHi = (gameScore > pet.gameHi);
      pet.playResult(gameScore);  // aggiorna il record e dà felicità
      halSfx(gameNewHi && gameScore > 0 ? SFX_MEDAL : SFX_LEVEL);
      gameOverUntil = millis() + 4000;
    } else {
      respawnBall();
    }
  }
  // il Pokémon la segue sotto
  float chase = (ballX - gamePetX) * 0.12f;
  if (chase > 7) chase = 7;
  if (chase < -7) chase = -7;
  gamePetX += chase * k;
}

// ---------- sacco da allenamento (allena la forza) ----------

void startSack() {
  if (pet.isEgg() || pet.sleeping || pet.ceremony) return;
  goScreen(SCR_SACK);
}

void sackEnter() {
  sackUntil = millis() + 10000;
  sackOverUntil = 0;
  sackHits = 0;
  sackShake = 0;
  sackNewHi = false;
}

void sackTap() {
  if (!timeLeft(sackUntil)) return;  // il tempo è finito
  sackHits++;
  sackShake = 16;  // scuote il sacco
}

// onTap di SCR_SACK: handleTouch lo chiama alla pressione, non al sollevamento
void sackScreenTap(int16_t, int16_t y) {
  if (y < 72) goScreen(SCR_MAIN);  // toccare in alto = abbandonare
  else sackTap();
}

void drawGameScene();  // prototipo (definita più in basso)

void renderSack() {
  uint32_t now = millis();
  drawGameScene();  // sfondo dell'habitat
  bool night = sceneHour() < 6 || sceneHour() >= 20;
  uint16_t ink = night ? UI_INK_NIGHT : UI_INK;

  // schermata dei risultati
  if (sackOverUntil) {
    if (!timeLeft(sackOverUntil)) { goScreen(SCR_MAIN); return; }
    char b[20];
    snprintf(b, sizeof(b), T(S_HITS_FMT), sackHits);
    gfx->setTextColor(ink);
    setSize(4);
    setCur(centerX(b, 4), 150);
    printT(b);
    char g[18];
    snprintf(g, sizeof(g), T(S_STR_GAIN_FMT), sackGain);
    gfx->setTextColor(UI_BAR_BAD);
    setSize(3);
    setCur(centerX(g, 3), 210);
    printT(g);
    setSize(2);
    if (sackNewHi && sackHits > 0) {
      gfx->setTextColor(UI_BAR_WARN);
      setCur(centerX(T(S_NEW_RECORD), 2), 256);
      printT(T(S_NEW_RECORD));
    } else {
      char r[18];
      snprintf(r, sizeof(r), T(S_RECORD_FMT), pet.strHi);
      gfx->setTextColor(ink);
      setCur(centerX(r, 2), 256);
      printT(r);
    }
    halFlush();
    return;
  }

  // i 10 s sono finiti: applicare l'allenamento
  if (!timeLeft(sackUntil)) {
    sackNewHi = (sackHits > pet.strHi);
    sackGain = pet.trainStrength(sackHits);
    halSfx(sackNewHi ? SFX_MEDAL : SFX_PLAY);
    sackOverUntil = now + 3500;
    halFlush();
    return;
  }

  // martellamento attivo
  sackShake *= 0.84f;
  int off = (int)(sackShake * sinf(now * 0.05f));
  int sx = CX + off, top = 86, sy = 150;
  gfx->fillRect(CX - 3, 56, 6, top - 56, ink);          // gancio/corda
  gfx->fillRect(sx - 4, top - 30, 8, 34, ink);          // catena
  gfx->fillRoundRect(sx - 42, top, 84, 150, 26, C565(0xb5, 0x3a, 0x3a));  // sacco
  gfx->fillRoundRect(sx - 42, top, 84, 22, 18, C565(0x7e, 0x28, 0x28));   // coperchio
  gfx->drawRoundRect(sx - 42, top, 84, 150, 26, ink);
  gfx->fillRect(sx - 42, top + 70, 84, 4, C565(0x7e, 0x28, 0x28));        // cucitura

  // contatore dei colpi
  char buf[8];
  snprintf(buf, sizeof(buf), "%u", sackHits);
  gfx->setTextColor(ink);
  setSize(6);
  setCur(centerX(buf, 6), 268);
  printT(buf);

  setSize(2);
  setCur(centerX(T(S_HIT_FAST), 2), 322);
  printT(T(S_HIT_FAST));

  // barra del tempo
  uint32_t left = sackUntil - now;
  int bw = 280, fw = (int)((uint32_t)bw * left / 10000);
  gfx->fillRoundRect(CX - bw / 2, 350, bw, 16, 5, UI_TRACK);
  if (fw > 2) gfx->fillRoundRect(CX - bw / 2, 350, fw, 16, 5, UI_BAR_OK);

  halFlush();
}

// sfondo del minigioco: habitat del Pokémon (cielo per ora + suolo del bioma)
void drawGameScene() {
  int hh = sceneHour();
  bool night = hh < 6 || hh >= 20;
  uint16_t top, bot;
  if (night)       { top = C565(0x0c, 0x12, 0x24); bot = C565(0x1e, 0x26, 0x46); }
  else if (hh < 8) { top = C565(0xd1, 0x6a, 0x86); bot = C565(0xf3, 0xb8, 0x7c); }
  else if (hh < 18){ top = C565(0x8f, 0xc8, 0xea); bot = C565(0xdc, 0xee, 0xe6); }
  else             { top = C565(0xc7, 0x5a, 0x4a); bot = C565(0xf0, 0xae, 0x64); }
  int hor = 376;
  for (int y = 0; y < hor; y += 8)
    gfx->fillRect(0, y, 466, 8, lerp565(top, bot, y, hor));
  if (night)
    for (auto &st : STARS) gfx->fillRect(st[0], st[1], 4, 4, UI_WHITE);
  uint8_t bio = pet.isEgg() ? 0 : DEX_TBL[pet.speciesId].biome;
  uint16_t soil = BIOME_SOIL[bio < 6 ? bio : 0];
  if (night) soil = lerp565(soil, C565(0x16, 0x1c, 0x30), 9, 16);
  gfx->fillRect(0, hor, 466, 466 - hor, soil);
}

void renderGame() {
  // niente fillScreen(NERO): drawGameScene copre interamente i 466x466. Se il DMA del
  // flush precedente sta ancora leggendo il buffer, vedrà contenuto valido (non nero a
  // metà), che era lo sfarfallio a 25 fps.
  bool night = sceneHour() < 6 || sceneHour() >= 20;
  uint16_t ink = night ? UI_INK_NIGHT : UI_INK;

  if (gameOverUntil) {
    drawGameScene();
    if (!timeLeft(gameOverUntil)) {
      goScreen(SCR_MAIN);
      return;
    }
    char buf[22];
    snprintf(buf, sizeof(buf), T(S_SCORE_FMT), gameScore);
    gfx->setTextColor(ink);
    setSize(4);
    setCur(centerX(buf, 4), 160);
    printT(buf);
    setSize(2);
    if (gameNewHi && gameScore > 0) {
      gfx->setTextColor(UI_BAR_WARN);
      setCur(centerX(T(S_NEW_RECORD), 2), 214);
      printT(T(S_NEW_RECORD));
    } else {
      char rec[20];
      snprintf(rec, sizeof(rec), T(S_RECORD_FMT), pet.gameHi);
      gfx->setTextColor(ink);
      setCur(centerX(rec, 2), 214);
      printT(rec);
    }
    const char *msg = gameScore >= 10 ? T(S_GREAT_JOY) : T(S_PLUS_JOY);
    gfx->setTextColor(ink);
    setCur(centerX(msg, 2), 250);
    printT(msg);
    halFlush();
    return;
  }

  drawGameScene();
  stepGame();

  // punteggio, record e vite
  char buf[8];
  snprintf(buf, sizeof(buf), "%u", gameScore);
  gfx->setTextColor(ink);
  setSize(4);
  setCur(centerX(buf, 4), 30);
  printT(buf);
  // 24 e non 12: in giapponese "きろく %u" sono 10 byte prima della cifra e il record
  // restava alla sua prima cifra ("きろく 2" con 219). Le dimensioni dei buffer di testo
  // le sorveglia TestBuffersDeTexto in test/test_tools.py.
  char rec[24];
  snprintf(rec, sizeof(rec), T(S_REC_FMT), pet.gameHi);
  setSize(2);
  setCur(centerX(rec, 2), 76);
  printT(rec);
  for (int i = 0; i < 3; i++) {
    if (i < 3 - gameMisses) gfx->fillCircle(180 + i * 28, 104, 6, UI_BAR_BAD);
    else gfx->drawCircle(180 + i * 28, 104, 6, UI_TRACK);
  }

  if (pmd.loaded) {
    uint8_t act = (ballX > gamePetX + 4) ? PMD_WALKR : (ballX < gamePetX - 4) ? PMD_WALKL : PMD_IDLE;
    if (!pmd.has(act)) act = PMD_IDLE;
    drawPmdAct(act, (int)gamePetX, 394, millis(), true, false, 3);
  } else if (mon.loaded) {
    int s = (mon.h * 2 > 130) ? 1 : 2;
    int w = mon.w * s, h = mon.h * s;
    uint16_t fm = mon.frameMs ? mon.frameMs : 100;
    uint16_t fi = (millis() / fm) % mon.frames;
    const uint8_t *fr = mon.data + (uint32_t)fi * mon.w * mon.h;
    int px = (int)gamePetX - w / 2, py = 394 - h;
    for (int r = 0; r < mon.h; r++)
      for (int c = 0; c < mon.w; c++) {
        uint8_t idx = fr[r * mon.w + c];
        if (idx == 0xFF) continue;
        gfx->fillRect(px + c * s, py + r * s, s, s, mon.pal[idx]);
      }
  }

  // anello d'impatto che si espande e svanisce (feedback morbido del colpo)
  uint32_t ht = millis() - hitTime;
  if (hitTime && ht < 260) {
    int rad = 22 + (int)(ht / 6);
    gfx->drawCircle((int)hitX, (int)hitY, rad, C565(0xff, 0xe7, 0x9f));
    gfx->drawCircle((int)hitX, (int)hitY, rad - 2, C565(0xff, 0xd9, 0x8a));
  }

  // la pokéball
  drawMap(SPR_ICON_PLAY, 16, (int)ballX - 24, (int)ballY - 24, 3, false);

  halFlush();
}
