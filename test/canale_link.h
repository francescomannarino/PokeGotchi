#pragma once
// Canale simulato per i test del collegamento: perde, duplica e riordina i pacchetti, sul
// modello di lossy_test di DylanPDao/TamaPoke (MIT). Lo usano test_link.cpp e
// test_linkbattaglia.cpp.
#include "../link.h"
#include <string.h>
#include <algorithm>
#include <string>
#include <vector>

// ---------------------------------------------------------------- canale simulato
struct Canale;
struct Nodo {
  Link lk;
  LinkConfig cfg;
  Canale *c = nullptr;
  int i = 0;
};

struct Volo {
  uint32_t t;
  uint32_t ordine;
  int da, a;
  std::vector<uint8_t> b;
};

struct Canale {
  std::vector<Nodo *> nodi;
  std::vector<Volo> voli;
  uint32_t ora = 1000;
  uint32_t rng = 1;
  uint32_t ordine = 0;
  int perdita = 0;     // % di pacchetti persi, per destinatario
  int doppi = 0;       // % consegnati due volte
  int ritardoMax = 0;  // ms: ritardi diversi = riordino
  bool sordo = false;
  int spediti = 0, persi = 0;

  uint32_t caso() {
    rng ^= rng << 13;
    rng ^= rng >> 17;
    rng ^= rng << 5;
    return rng;
  }
  void metti(int da, int a, const uint8_t *b, uint8_t n) {
    uint32_t t = ora + 1 + (ritardoMax ? caso() % (ritardoMax + 1) : 0);
    voli.push_back({t, ordine++, da, a, std::vector<uint8_t>(b, b + n)});
  }
  void spedisci(int da, const uint8_t mac[6], const uint8_t *b, uint8_t n) {
    bool tutti = !memcmp(mac, LINK_BROADCAST, 6);
    for (size_t k = 0; k < nodi.size(); k++) {
      if ((int)k == da || (!tutti && memcmp(mac, nodi[k]->cfg.mac, 6))) continue;
      spediti++;
      if (sordo || (int)(caso() % 100) < perdita) { persi++; continue; }
      metti(da, (int)k, b, n);
      if ((int)(caso() % 100) < doppi) metti(da, (int)k, b, n);
    }
  }
  // un pacchetto falso, come se arrivasse da `mac`
  void inietta(const uint8_t mac[6], int a, const uint8_t *b, uint8_t n) {
    nodi[a]->lk.ricevi(mac, b, n, ora);
  }
  void passo(uint32_t ms) {
    ora += ms;
    std::vector<Volo> ora_;
    std::vector<Volo> resto;
    for (auto &v : voli) (v.t <= ora ? ora_ : resto).push_back(v);
    voli.swap(resto);
    std::sort(ora_.begin(), ora_.end(), [](const Volo &x, const Volo &y) {
      return x.t != y.t ? x.t < y.t : x.ordine < y.ordine;
    });
    for (auto &v : ora_) nodi[v.a]->lk.ricevi(nodi[v.da]->cfg.mac, v.b.data(), v.b.size(), ora);
    for (auto *n : nodi) n->lk.tick(ora);
  }
  void corri(uint32_t ms, uint32_t passoMs = 5) {
    for (uint32_t t = 0; t < ms; t += passoMs) passo(passoMs);
  }
};

static void trasporto(void *ctx, const uint8_t mac[6], const uint8_t *b, uint8_t n) {
  Nodo *nd = (Nodo *)ctx;
  nd->c->spedisci(nd->i, mac, b, n);
}

static void prepara(Canale &c, Nodo &n, uint8_t id, uint16_t specie, const char *nome) {
  const uint8_t mac[6] = {0x24, 0x0A, 0xC4, 0x12, 0x34, id};
  memcpy(n.cfg.mac, mac, 6);
  n.cfg.trainerId = 0x1000u + id;
  n.cfg.specie = specie;
  strncpy(n.cfg.nome, nome, LINK_NOME_LEN);
  n.cfg.fw = "1.19";
  n.cfg.seme = 7u * id + c.rng;
  n.c = &c;
  n.i = (int)c.nodi.size();
  c.nodi.push_back(&n);
}

static void avvia(Canale &c, Nodo &n) {
  LinkTrasporto tr;
  tr.ctx = &n;
  tr.invia = trasporto;
  n.lk.avvia(n.cfg, tr, c.ora);
}

// corre finché tutti e due sono PRONTO, confermando appena compare il codice
static bool abbina(Canale &c, Nodo &a, Nodo &b, uint32_t maxMs, bool confermaA = true,
                   bool confermaB = true) {
  for (uint32_t t = 0; t < maxMs; t += 5) {
    c.passo(5);
    if (confermaA && a.lk.stato() == LINK_CONFERMA) a.lk.conferma();
    if (confermaB && b.lk.stato() == LINK_CONFERMA) b.lk.conferma();
    if (a.lk.stato() == LINK_PRONTO && b.lk.stato() == LINK_PRONTO) return true;
  }
  return false;
}

// due nodi (A: Pikachu "ASH", B: Eevee "GARY") già PRONTO su un canale perfetto
struct Coppia {
  Canale c;
  Nodo a, b;
  explicit Coppia(uint32_t seme = 1) {
    c.rng = seme;
    prepara(c, a, 1, 25, "ASH");
    prepara(c, b, 2, 133, "GARY");
  }
};
