// Emulatore su PC del firmware intero: `setup()` e `loop()` dello sketch girano
// sull'HAL di hal_host.cpp. Con la finestra (SDL2): mouse = touch, frecce = scorrimenti,
// P = pulsante PWR, S = screenshot, Esc = esci; I/J/K/L = inclinazione (tenuti), Z = scossone,
// C = doppio colpetto. La console seriale è stdin/stdout. Musica ed
// effetti escono dalla scheda audio del PC (musica.h a 32 kHz); --muto e --headless tacciono.
//
//   ./build/pokegotchi [--fast N] [--sd DIR] [--nvs FILE] [--epoch E] [--seed S] [--zoom Z] [--muto]
//   ./build/pokegotchi --headless --screenshot out.png [--ms T]
//   ./build/pokegotchi --headless --copione smoke.txt [--fast N]
//
// Il copione (una riga per comando, `#` = commento) guida il gioco senza finestra:
//   TOCCA x y | SCORRI SU|GIU|SINISTRA|DESTRA | PWR      gesti e pulsante
//   ATTENDI ms | ORE h                                    tempo di gioco
//   PAUSA ms                                              tempo reale (emulatori collegati)
//   SERIALE <riga>                                        come dalla console
//   INCLINA x y | SCUOTI | COLPETTI                       sensore di movimento (gravità in g)
//   FOTO file.png                                         il fotogramma attuale
//   SCHERMATA NOME                                        verifica gScreen (es. MAIN)
#include "../../ui_common.h"
#include "emu.h"
#include <SDL.h>
#include <signal.h>
#include <unistd.h>
#include <time.h>
#include <string>
#include <vector>

void setup();
void loop();

static volatile sig_atomic_t gEsci = 0;
static void suSegnale(int) { gEsci = 1; }

// i nomi di `enum Screen` senza SCR_, per SCHERMATA
static const struct { Screen s; const char *nome; } NOMI[] = {
  {SCR_STARTER, "STARTER"}, {SCR_MAIN, "MAIN"},         {SCR_GALLERY, "GALLERY"},
  {SCR_CARD, "CARD"},       {SCR_KEYBOARD, "KEYBOARD"}, {SCR_CLOCK, "CLOCK"},
  {SCR_GAME, "GAME"},       {SCR_SACK, "SACK"},         {SCR_CEREMONY, "CEREMONY"},
  {SCR_BATTLE, "BATTLE"},   {SCR_COLLEZIONE, "COLLEZIONE"}, {SCR_LINK, "LINK"},
  {SCR_GROTTA, "GROTTA"},   {SCR_SURF, "SURF"},         {SCR_AGGIORNA, "AGGIORNA"},
};
static const char *nomeSchermata(Screen s) {
  for (auto &n : NOMI)
    if (n.s == s) return n.nome;
  return "?";
}

// ---------------------------------------------------------------- senza finestra

static uint32_t gFast = 1;
static void passo(uint32_t ms) {
  emuTempoAvanza(ms);
  loop();
}

// tempo di gioco a passi di 20 ms (una lettura del touch) moltiplicati per --fast; nei
// minigiochi senza moltiplicare, perché la loro fisica va a tempo reale (al massimo 3 passi
// da 85 ms per frame) e un passo da 10 s finirebbe la partita in pochi frame
static void attendi(uint64_t ms) {
  while (ms > 0 && !gEsci) {
    uint64_t p = (gScreen == SCR_GAME || gScreen == SCR_GROTTA || gScreen == SCR_SURF) ? 20 : 20ULL * gFast;
    uint32_t k = (uint32_t)(ms < p ? ms : p);
    passo(k);
    ms -= k;
  }
}

// un gesto va a tempo reale: 20 ms a lettura, poi un attimo perché la UI risponda
static void finisciGesto() {
  while (emuGestoInCorso()) passo(20);
  for (int i = 0; i < 10; i++) passo(20);
}

static int eseguiCopione(const char *path) {
  FILE *f = fopen(path, "r");
  if (!f) {
    fprintf(stderr, "copione non trovato: %s\n", path);
    return 2;
  }
  char buf[4096];
  int nr = 0, errori = 0;
  while (!gEsci && fgets(buf, sizeof(buf), f)) {
    nr++;
    std::string r(buf);
    while (!r.empty() && (r.back() == '\n' || r.back() == '\r' || r.back() == ' ')) r.pop_back();
    size_t i = r.find_first_not_of(" \t");
    if (i == std::string::npos || r[i] == '#') continue;
    r = r.substr(i);
    std::string cmd = r.substr(0, r.find(' '));
    std::string arg = r.size() > cmd.size() ? r.substr(cmd.size() + 1) : "";
    printf("[copione %d] %s\n", nr, r.c_str());
    if (cmd == "TOCCA") {
      int x = 0, y = 0;
      sscanf(arg.c_str(), "%d %d", &x, &y);
      emuGestoTocco(x, y);
      finisciGesto();
    } else if (cmd == "SCORRI") {
      int dx = arg == "DESTRA" ? 1 : arg == "SINISTRA" ? -1 : 0;
      int dy = arg == "GIU" ? 1 : arg == "SU" ? -1 : 0;
      emuGestoScorri(dx, dy);
      finisciGesto();
    } else if (cmd == "PWR") {
      emuPremiPwr();
      attendi(300);  // il loop interroga il PWR ogni 250 ms
    } else if (cmd == "ATTENDI") {
      attendi(strtoull(arg.c_str(), nullptr, 10));
    } else if (cmd == "PAUSA") {
      // tempo reale, a passi di 20 ms: due emulatori collegati vanno di pari passo
      for (uint64_t t = strtoull(arg.c_str(), nullptr, 10); t > 0 && !gEsci; t -= t < 20 ? t : 20) {
        usleep(20000);
        passo(20);
      }
    } else if (cmd == "ORE") {
      attendi((uint64_t)(atof(arg.c_str()) * 3600000.0));
    } else if (cmd == "SERIALE") {
      std::string riga = arg + "\n";
      emuSerialeRiceve(riga.data(), riga.size());
      passo(20);
    } else if (cmd == "INCLINA") {
      float x = 0, y = 0;
      sscanf(arg.c_str(), "%f %f", &x, &y);
      emuMotoInclina(x, y);
      passo(20);
    } else if (cmd == "GIRA") {
      // l'inclinazione di `g` che gira di 30 gradi ogni 500 ms, per `passi` passi: nella
      // Grotta il pet fa il giro dell'arena lungo la parete
      float g = 0;
      int passi = 0;
      sscanf(arg.c_str(), "%f %d", &g, &passi);
      for (int i = 0; i < passi && !gEsci; i++) {
        float a = i * 3.14159265f / 6;
        emuMotoInclina(g * cosf(a), g * sinf(a));
        attendi(500);
      }
    } else if (cmd == "SCUOTI") {
      emuMotoScuoti();
      passo(20);
    } else if (cmd == "COLPETTI") {
      emuMotoColpetti();
      passo(20);
    } else if (cmd == "FOTO") {
      if (!emuSalvaPng(arg.c_str())) {
        fprintf(stderr, "[copione %d] FOTO non scritta: %s\n", nr, arg.c_str());
        errori++;
      }
    } else if (cmd == "SCHERMATA") {
      if (arg != nomeSchermata(gScreen)) {
        fprintf(stderr, "[copione %d] attesa la schermata %s, c'e' %s\n", nr, arg.c_str(),
                nomeSchermata(gScreen));
        errori++;
      }
    } else {
      fprintf(stderr, "[copione %d] comando sconosciuto: %s\n", nr, cmd.c_str());
      errori++;
    }
  }
  fclose(f);
  return errori ? 1 : 0;
}

// ---------------------------------------------------------------- con la finestra

// la maschera rotonda: trasparente dentro il cerchio, il vetro nero della scheda fuori
static SDL_Texture *creaMaschera(SDL_Renderer *r, int w, int h) {
  SDL_Texture *t = SDL_CreateTexture(r, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STATIC, w, h);
  std::vector<uint32_t> px((size_t)w * h);
  for (int y = 0; y < h; y++)
    for (int x = 0; x < w; x++) {
      int dx = 2 * x + 1 - w, dy = 2 * y + 1 - h;
      px[(size_t)y * w + x] = dx * dx + dy * dy > w * w ? 0xFF202020 : 0;
    }
  SDL_UpdateTexture(t, nullptr, px.data(), w * 4);
  SDL_SetTextureBlendMode(t, SDL_BLENDMODE_BLEND);
  return t;
}

static int finestra(int zoom) {
  if (SDL_Init(SDL_INIT_VIDEO) != 0) {
    fprintf(stderr, "SDL_Init: %s (per lavorare senza finestra: --headless)\n", SDL_GetError());
    return 1;
  }
  const int W = 466, H = 466;
  SDL_Window *win = SDL_CreateWindow("PokeGotchi", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                                     W * zoom, H * zoom, SDL_WINDOW_RESIZABLE);
  SDL_Renderer *ren = SDL_CreateRenderer(win, -1, SDL_RENDERER_PRESENTVSYNC);
  if (!win || !ren) {
    fprintf(stderr, "SDL: %s\n", SDL_GetError());
    return 1;
  }
  SDL_RenderSetLogicalSize(ren, W, H);  // il mouse arriva già in coordinate dello schermo
  SDL_Texture *tex = SDL_CreateTexture(ren, SDL_PIXELFORMAT_RGB565, SDL_TEXTUREACCESS_STREAMING, W, H);
  SDL_Texture *maschera = creaMaschera(ren, W, H);

  uint64_t prima = SDL_GetTicks64(), ultimoSalvataggio = prima;
  int foto = 0;
  bool dito = false;
  while (!gEsci) {
    SDL_Event e;
    while (SDL_PollEvent(&e)) {
      if (e.type == SDL_QUIT) gEsci = 1;
      else if (e.type == SDL_MOUSEBUTTONDOWN && e.button.button == SDL_BUTTON_LEFT) {
        dito = true;
        emuDito(true, e.button.x, e.button.y);
      } else if (e.type == SDL_MOUSEBUTTONUP && e.button.button == SDL_BUTTON_LEFT) {
        dito = false;
        emuDito(false, e.button.x, e.button.y);
      } else if (e.type == SDL_MOUSEMOTION && dito) {
        emuDito(true, e.motion.x, e.motion.y);
      } else if (e.type == SDL_KEYDOWN && !e.key.repeat) {
        switch (e.key.keysym.sym) {
          case SDLK_LEFT: emuGestoScorri(-1, 0); break;
          case SDLK_RIGHT: emuGestoScorri(1, 0); break;
          case SDLK_UP: emuGestoScorri(0, -1); break;
          case SDLK_DOWN: emuGestoScorri(0, 1); break;
          case SDLK_p: emuPremiPwr(); break;
          case SDLK_z: emuMotoScuoti(); break;
          case SDLK_c: emuMotoColpetti(); break;
          case SDLK_ESCAPE: gEsci = 1; break;
          case SDLK_s: {
            char nome[64];
            snprintf(nome, sizeof(nome), "pokegotchi-%03d.png", ++foto);
            printf("[emu] screenshot %s%s\n", nome, emuSalvaPng(nome) ? "" : " NON scritto");
            break;
          }
        }
      }
    }
    emuSerialeLeggiStdin(0);
    {  // I/J/K/L tenuti: la scheda inclinata verso l'alto, sinistra, basso, destra
      const Uint8 *k = SDL_GetKeyboardState(nullptr);
      auto giu = [k](SDL_Keycode c) { return k[SDL_GetScancodeFromKey(c)] ? 1 : 0; };
      emuMotoTasti(giu(SDLK_l) - giu(SDLK_j), giu(SDLK_k) - giu(SDLK_i));
    }

    // durante un gesto il tempo scorre normale: con --fast un tocco durerebbe minuti
    uint64_t ora = SDL_GetTicks64();
    uint32_t dt = (uint32_t)(ora - prima);
    prima = ora;
    emuTempoAvanza(dt * (emuGestoInCorso() ? 1 : gFast));
    loop();

    SDL_UpdateTexture(tex, nullptr, emuFramebuffer(), W * 2);
    uint8_t l = emuLuminosita();  // la luminosità dell'AMOLED, non lineare all'occhio
    uint8_t m = l == 0 ? 0 : (uint8_t)(48 + l * 207 / 255);
    SDL_SetTextureColorMod(tex, m, m, m);
    SDL_RenderClear(ren);
    SDL_RenderCopy(ren, tex, nullptr, nullptr);
    SDL_RenderCopy(ren, maschera, nullptr, nullptr);
    SDL_RenderPresent(ren);  // con il vsync fa anche da pausa del ciclo

    if (ora - ultimoSalvataggio > 5000) {
      ultimoSalvataggio = ora;
      emuNvsSalvaSeCambiata();
    }
  }
  SDL_DestroyRenderer(ren);
  SDL_DestroyWindow(win);
  emuAudioChiudi();
  SDL_Quit();
  return 0;
}

// ---------------------------------------------------------------- main

static void uso() {
  fprintf(stderr,
          "uso: pokegotchi [--fast N] [--sd DIR] [--nvs FILE] [--epoch E] [--seed S] [--zoom Z] [--muto]\n"
          "              [--ota-ultima V] [--headless (--screenshot F.png [--ms T] | --copione F)]\n");
}

int main(int argc, char **argv) {
  emuArgv = argv;
  setvbuf(stdout, nullptr, _IOLBF, 0);
  signal(SIGINT, suSegnale);
  signal(SIGTERM, suSegnale);

  bool headless = false, muto = false;
  const char *screenshot = nullptr, *copione = nullptr;
  uint64_t ms = 2000;
  int zoom = 1;
  unsigned long seme = (unsigned long)time(nullptr);
  for (int i = 1; i < argc; i++) {
    std::string a = argv[i];
    bool val = i + 1 < argc;
    if (a == "--headless") headless = true;
    else if (a == "--muto") muto = true;
    else if (a == "--fast" && val) gFast = (uint32_t)atol(argv[++i]);
    else if (a == "--screenshot" && val) screenshot = argv[++i];
    else if (a == "--copione" && val) copione = argv[++i];
    else if (a == "--ms" && val) ms = strtoull(argv[++i], nullptr, 10);
    else if (a == "--sd" && val) emuOpz.sd = argv[++i];
    else if (a == "--nvs" && val) emuOpz.nvs = argv[++i];
    else if (a == "--epoch" && val) emuOpz.epoch = (uint32_t)atol(argv[++i]);
    else if (a == "--ota-ultima" && val) emuOpz.otaUltima = argv[++i];
    else if (a == "--seed" && val) seme = strtoul(argv[++i], nullptr, 10);
    else if (a == "--zoom" && val) zoom = atoi(argv[++i]);
    else {
      uso();
      return 2;
    }
  }
  // i predefiniti partono dalla cartella dell'eseguibile (tools/emu/build)
  std::string dir = argv[0];
  dir = dir.find('/') == std::string::npos ? "." : dir.substr(0, dir.rfind('/'));
  static std::string sd = dir + "/../../sdcard", nvs = dir + "/nvs.txt";
  if (!emuOpz.sd) emuOpz.sd = sd.c_str();
  if (!emuOpz.nvs) emuOpz.nvs = nvs.c_str();
  if (gFast < 1) gFast = 1;
  if (zoom < 1) zoom = 1;
  if (headless && !screenshot && !copione) {
    uso();
    return 2;
  }
  randomSeed(seme);
  if (headless) emuSerialeStdin(false);  // senza finestra la console è il copione

  emuAudioApri(!headless && !muto);
  setup();
  int esito = 0;
  if (!headless) {
    esito = finestra(zoom);
  } else {
    if (copione) esito = eseguiCopione(copione);
    else attendi(ms);
    if (screenshot && !emuSalvaPng(screenshot)) {
      fprintf(stderr, "screenshot non scritto: %s\n", screenshot);
      esito = 1;
    }
  }
  emuSalvaStato();
  return esito;
}
