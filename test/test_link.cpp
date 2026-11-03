// Test del protocollo di collegamento (link.cpp) su un canale simulato che perde,
// duplica e riordina i pacchetti, sul modello di lossy_test di DylanPDao/TamaPoke (MIT).
#include "framework.h"
#include "canale_link.h"

// ---------------------------------------------------------------- pacchetti

TEST(Link, crc16_ccitt_false_sul_vettore_di_prova) {
  CHECK_EQ(linkCrc16((const uint8_t *)"123456789", 9), (uint16_t)0x29B1);
}

TEST(Link, intestazione_little_endian_byte_per_byte) {
  uint8_t b[LINK_FRAME_MAX];
  const uint8_t corpo[3] = {0xAA, 0xBB, 0xCC};
  uint8_t n = linkCodifica(b, sizeof(b), LINK_PROTO, LT_BATTAGLIA, 0x1234, corpo, 3);
  CHECK_EQ(n, (uint8_t)(LINK_HDR + 3));
  CHECK_EQ(b[0], (uint8_t)'T');
  CHECK_EQ(b[1], (uint8_t)'P');
  CHECK_EQ(b[2], (uint8_t)LINK_PROTO);
  CHECK_EQ(b[3], (uint8_t)LT_BATTAGLIA);
  CHECK_EQ(b[4], (uint8_t)0x34);
  CHECK_EQ(b[5], (uint8_t)0x12);
  CHECK_EQ(b[6], (uint8_t)3);
  uint16_t crc = linkCrc16(b, 7);
  crc = linkCrc16(corpo, 3, crc);
  CHECK_EQ(b[7], (uint8_t)(crc & 0xFF));
  CHECK_EQ(b[8], (uint8_t)(crc >> 8));
  CHECK_EQ(b[9], (uint8_t)0xAA);
  LinkFrame f;
  CHECK(linkDecodifica(b, n, f));
  CHECK_EQ(f.seq, (uint16_t)0x1234);
  CHECK_EQ(f.len, (uint8_t)3);
  CHECK_EQ(f.dati[2], (uint8_t)0xCC);
}

TEST(Link, pacchetti_rotti_si_scartano) {
  uint8_t b[LINK_FRAME_MAX + 4];
  const uint8_t corpo[4] = {1, 2, 3, 4};
  uint8_t n = linkCodifica(b, sizeof(b), LINK_PROTO, LT_PING, 7, corpo, 4);
  LinkFrame f;
  CHECK(linkDecodifica(b, n, f));
  CHECK(!linkDecodifica(b, n - 1, f));          // troncato
  CHECK(!linkDecodifica(b, n + 1, f));          // un byte in più
  CHECK(!linkDecodifica(b, LINK_HDR - 1, f));
  b[LINK_HDR + 2] ^= 0x10;                      // un bit del corpo
  CHECK(!linkDecodifica(b, n, f));
  b[LINK_HDR + 2] ^= 0x10;
  b[0] = 'X';                                   // magic
  CHECK(!linkDecodifica(b, n, f));
  // un corpo troppo grande non si codifica
  uint8_t grande[LINK_DATI_MAX + 1] = {0};
  CHECK_EQ(linkCodifica(b, sizeof(b), LINK_PROTO, LT_BATTAGLIA, 1, grande, LINK_DATI_MAX + 1),
           (uint8_t)0);
  CHECK_EQ(linkCodifica(b, sizeof(b), LINK_PROTO, LT_BATTAGLIA, 1, grande, LINK_DATI_MAX),
           (uint8_t)LINK_FRAME_MAX);
}

TEST(Link, codice_uguale_dalle_due_parti) {
  const uint8_t a[6] = {1, 2, 3, 4, 5, 6}, b[6] = {1, 2, 3, 4, 5, 7}, c[6] = {9, 9, 9, 9, 9, 9};
  CHECK_EQ(linkCodice(a, b), linkCodice(b, a));
  CHECK_RANGE(linkCodice(a, b), 0, 9999);
  CHECK(linkCodice(a, b) != linkCodice(a, c));
}

TEST(Link, hash_dei_dati_stabile) {
  CHECK(linkHashDati() != 0);
  CHECK_EQ(linkHashDati(), linkHashDati());
}

// ---------------------------------------------------------------- abbinamento

TEST(Link, abbinamento_su_canale_perfetto) {
  Coppia p;
  avvia(p.c, p.a);
  avvia(p.c, p.b);
  CHECK(abbina(p.c, p.a, p.b, 5000));
  CHECK_EQ(p.a.lk.codice(), p.b.lk.codice());
  // ognuno ha il nome e la specie dell'altro, non i propri rispediti indietro
  CHECK_STREQ(p.a.lk.peer().nome, "GARY");
  CHECK_STREQ(p.b.lk.peer().nome, "ASH");
  CHECK_EQ(p.a.lk.peer().specie, (uint16_t)133);
  CHECK_EQ(p.b.lk.peer().specie, (uint16_t)25);
  CHECK_EQ(p.a.lk.peer().trainerId, 0x1002u);
  CHECK_STREQ(p.a.lk.peer().fw, "1.19");
  CHECK(!memcmp(p.a.lk.peer().mac, p.b.cfg.mac, 6));
}

TEST(Link, senza_conferma_del_giocatore_non_si_e_pronti) {
  Coppia p;
  avvia(p.c, p.a);
  avvia(p.c, p.b);
  CHECK(!abbina(p.c, p.a, p.b, 5000, false));
  CHECK_EQ(p.a.lk.stato(), LINK_CONFERMA);
  CHECK_EQ(p.b.lk.stato(), LINK_CONFERMA);   // ha confermato, aspetta A
  CHECK(p.b.lk.confermatoIo());
  CHECK(p.a.lk.confermatoLoro());
  CHECK(!p.a.lk.invia(LT_BATTAGLIA, nullptr, 0));
  p.a.lk.conferma();
  CHECK_EQ(p.a.lk.stato(), LINK_PRONTO);
  p.c.corri(500);
  CHECK_EQ(p.b.lk.stato(), LINK_PRONTO);
}

TEST(Link, abbinamento_con_il_30_per_cento_di_perdita) {
  int falliti = 0;
  uint32_t peggiore = 0;
  for (uint32_t seme = 1; seme <= 40; seme++) {
    Coppia p(seme * 2654435761u);
    p.c.perdita = 30;
    p.c.doppi = 10;
    p.c.ritardoMax = 40;
    avvia(p.c, p.a);
    p.c.corri(5 * (seme % 7));   // non partono insieme
    avvia(p.c, p.b);
    uint32_t t0 = p.c.ora;
    if (!abbina(p.c, p.a, p.b, 30000) || p.a.lk.codice() != p.b.lk.codice()) falliti++;
    peggiore = std::max(peggiore, p.c.ora - t0);
  }
  CHECK_EQ(falliti, 0);
  CHECK_MSG(peggiore < 30000, std::to_string(peggiore));
}

// ---------------------------------------------------------------- affidabilità

static void scambia(Coppia &p, int quanti) {
  int mandatiA = 0, mandatiB = 0, presiA = 0, presiB = 0;
  bool ordineOk = true;
  for (int t = 0; t < 120000 && (presiA < quanti || presiB < quanti); t += 5) {
    uint8_t d[3];
    d[0] = (uint8_t)mandatiA; d[1] = 0xA0; d[2] = (uint8_t)~mandatiA;
    if (mandatiA < quanti && p.a.lk.invia(LT_BATTAGLIA + 1, d, 3)) mandatiA++;
    d[0] = (uint8_t)mandatiB; d[1] = 0xB0; d[2] = (uint8_t)~mandatiB;
    if (mandatiB < quanti && p.b.lk.invia(LT_SCAMBIO, d, 3)) mandatiB++;
    p.c.passo(5);
    LinkMessaggio m;
    while (p.b.lk.prendi(m)) {
      ordineOk &= m.tipo == LT_BATTAGLIA + 1 && m.len == 3 && m.dati[0] == presiB &&
                  m.dati[1] == 0xA0 && m.dati[2] == (uint8_t)~presiB;
      presiB++;
    }
    while (p.a.lk.prendi(m)) {
      ordineOk &= m.tipo == LT_SCAMBIO && m.len == 3 && m.dati[0] == presiA && m.dati[1] == 0xB0;
      presiA++;
    }
  }
  CHECK_EQ(presiA, quanti);
  CHECK_EQ(presiB, quanti);
  CHECK(ordineOk);
}

TEST(Link, messaggi_una_volta_sola_e_in_ordine_su_canale_cattivo) {
  Coppia p(99);
  p.c.perdita = 30;
  p.c.doppi = 20;
  p.c.ritardoMax = 60;
  avvia(p.c, p.a);
  avvia(p.c, p.b);
  CHECK(abbina(p.c, p.a, p.b, 30000));
  scambia(p, 40);
  CHECK(p.a.lk.ritrasmessi() > 0);
  CHECK_EQ(p.a.lk.stato(), LINK_PRONTO);
  CHECK_EQ(p.b.lk.stato(), LINK_PRONTO);
}

TEST(Link, chi_non_legge_rallenta_senza_perdere_niente) {
  Coppia p;
  avvia(p.c, p.a);
  avvia(p.c, p.b);
  CHECK(abbina(p.c, p.a, p.b, 5000));
  int mandati = 0;
  for (int t = 0; t < 3000; t += 5) {
    uint8_t d = (uint8_t)mandati;
    if (mandati < 10 && p.a.lk.invia(LT_BATTAGLIA, &d, 1)) mandati++;
    p.c.passo(5);   // B non chiama prendi()
  }
  CHECK_EQ(mandati, 2 * LINK_CODA);   // 4 in arrivo da B, 4 in coda da A
  CHECK(!p.a.lk.invia(LT_BATTAGLIA, nullptr, 0));
  int presi = 0;
  bool ordine = true;
  for (int t = 0; t < 5000; t += 5) {
    uint8_t d = (uint8_t)mandati;
    if (mandati < 10 && p.a.lk.invia(LT_BATTAGLIA, &d, 1)) mandati++;
    p.c.passo(5);
    LinkMessaggio m;
    while (p.b.lk.prendi(m)) ordine &= m.dati[0] == presi++;
  }
  CHECK_EQ(presi, 10);
  CHECK(ordine);
  CHECK_EQ(p.a.lk.stato(), LINK_PRONTO);
}

TEST(Link, tipi_fuori_fascia_non_si_inviano) {
  Coppia p;
  avvia(p.c, p.a);
  avvia(p.c, p.b);
  CHECK(abbina(p.c, p.a, p.b, 5000));
  CHECK(!p.a.lk.invia(LT_PING, nullptr, 0));
  CHECK(!p.a.lk.invia(LT_APP_FINE, nullptr, 0));
  CHECK(p.a.lk.invia(LT_APP_FINE - 1, nullptr, 0));
}

TEST(Link, keepalive_tiene_vivo_il_collegamento_in_ogni_stato) {
  // il giocatore legge il codice per due minuti, poi i due restano fermi per altri due
  Coppia p(5);
  p.c.perdita = 10;
  avvia(p.c, p.a);
  avvia(p.c, p.b);
  CHECK(!abbina(p.c, p.a, p.b, 120000, false));
  CHECK_EQ(p.a.lk.stato(), LINK_CONFERMA);
  CHECK_EQ(p.b.lk.stato(), LINK_CONFERMA);
  p.a.lk.conferma();
  p.c.corri(120000);
  CHECK_EQ(p.a.lk.stato(), LINK_PRONTO);
  CHECK_EQ(p.b.lk.stato(), LINK_PRONTO);
}

// ---------------------------------------------------------------- timeout

TEST(Link, timeout_quando_il_peer_tace) {
  Coppia p;
  avvia(p.c, p.a);
  avvia(p.c, p.b);
  CHECK(abbina(p.c, p.a, p.b, 5000));
  p.c.corri(3000);
  p.c.sordo = true;
  p.c.corri(LINK_TIMEOUT_MS - LINK_PING_MS - 500);
  CHECK_EQ(p.a.lk.stato(), LINK_PRONTO);
  p.c.corri(LINK_PING_MS + 1000);
  CHECK_EQ(p.a.lk.stato(), LINK_PERSO);
  CHECK_EQ(p.b.lk.stato(), LINK_PERSO);
  CHECK_EQ(p.a.lk.motivo(), LINK_SILENZIO);
  CHECK(!p.a.lk.attivo());
  CHECK(!p.a.lk.invia(LT_BATTAGLIA, nullptr, 0));
}

TEST(Link, timeout_anche_in_conferma) {
  Coppia p;
  avvia(p.c, p.a);
  avvia(p.c, p.b);
  abbina(p.c, p.a, p.b, 2000, false);
  CHECK_EQ(p.a.lk.stato(), LINK_CONFERMA);
  p.c.sordo = true;
  p.c.corri(LINK_TIMEOUT_MS + 1000);
  CHECK_EQ(p.a.lk.stato(), LINK_PERSO);
  CHECK_EQ(p.a.lk.motivo(), LINK_SILENZIO);
}

TEST(Link, timeout_della_ricerca_senza_nessuno) {
  Coppia p;
  avvia(p.c, p.a);
  p.c.corri(LINK_CERCA_MS - 100);
  CHECK_EQ(p.a.lk.stato(), LINK_CERCA);
  p.c.corri(200);
  CHECK_EQ(p.a.lk.stato(), LINK_PERSO);
  CHECK_EQ(p.a.lk.motivo(), LINK_NESSUNO);
}

TEST(Link, bye_chiude_subito_dall_altra_parte) {
  Coppia p;
  avvia(p.c, p.a);
  avvia(p.c, p.b);
  CHECK(abbina(p.c, p.a, p.b, 5000));
  p.a.lk.chiudi();
  CHECK_EQ(p.a.lk.stato(), LINK_SPENTO);
  p.c.corri(50);
  CHECK_EQ(p.b.lk.stato(), LINK_PERSO);
  CHECK_EQ(p.b.lk.motivo(), LINK_ADDIO);
}

TEST(Link, il_peer_riavviato_in_conferma_si_riaggancia) {
  Coppia p;
  avvia(p.c, p.a);
  avvia(p.c, p.b);
  abbina(p.c, p.a, p.b, 2000, false, false);
  CHECK_EQ(p.a.lk.stato(), LINK_CONFERMA);
  p.a.lk.conferma();
  avvia(p.c, p.b);   // B si riavvia: torna a cercare con una sequenza nuova
  CHECK(abbina(p.c, p.a, p.b, 5000));
  CHECK_EQ(p.a.lk.codice(), p.b.lk.codice());
}

TEST(Link, il_peer_riavviato_in_pronto_finisce_in_timeout) {
  // da PRONTO non si torna indietro: i suoi HELLO di ricerca non tengono vivo il link
  Coppia p;
  avvia(p.c, p.a);
  avvia(p.c, p.b);
  CHECK(abbina(p.c, p.a, p.b, 5000));
  avvia(p.c, p.b);
  p.c.corri(LINK_TIMEOUT_MS + 500);
  CHECK_EQ(p.a.lk.stato(), LINK_PERSO);
  CHECK_EQ(p.a.lk.motivo(), LINK_SILENZIO);
}

// ---------------------------------------------------------------- terzo dispositivo

TEST(Link, un_terzo_dispositivo_non_dirotta_l_abbinamento) {
  Coppia p;
  Nodo c3;
  prepara(p.c, c3, 3, 150, "MEW");
  avvia(p.c, p.a);
  avvia(p.c, p.b);
  abbina(p.c, p.a, p.b, 2000, false, false);
  CHECK_EQ(p.a.lk.stato(), LINK_CONFERMA);
  uint16_t codice = p.a.lk.codice();
  uint32_t scartatiPrima = p.a.lk.scartati();

  // C, compatibile, cerca e prova a infilarsi con pacchetti validi del suo MAC
  avvia(p.c, c3);
  uint8_t b[LINK_FRAME_MAX];
  uint8_t hello[LINK_HELLO_LEN] = {0};
  uint32_t h = linkHashDati();
  for (int i = 0; i < 4; i++) hello[i] = (uint8_t)(h >> (8 * i));
  memcpy(hello + 4, "1.19", 4);
  memcpy(hello + 12, p.a.cfg.mac, 6);   // «sono agganciato a te»
  memcpy(hello + 24, "MEW", 3);
  uint8_t n = linkCodifica(b, sizeof(b), LINK_PROTO, LT_HELLO, 1, hello, sizeof(hello));
  p.c.inietta(c3.cfg.mac, p.a.i, b, n);
  n = linkCodifica(b, sizeof(b), LINK_PROTO, LT_CONFERMA, 2, nullptr, 0);
  p.c.inietta(c3.cfg.mac, p.a.i, b, n);
  n = linkCodifica(b, sizeof(b), LINK_PROTO, LT_PING, 3, nullptr, 0);
  p.c.inietta(c3.cfg.mac, p.a.i, b, n);
  n = linkCodifica(b, sizeof(b), LINK_PROTO, LT_BYE, 4, nullptr, 0);
  p.c.inietta(c3.cfg.mac, p.a.i, b, n);
  p.c.inietta(c3.cfg.mac, p.b.i, b, n);
  p.c.corri(20000);

  CHECK_EQ(p.a.lk.stato(), LINK_CONFERMA);
  CHECK_EQ(p.b.lk.stato(), LINK_CONFERMA);
  CHECK(!memcmp(p.a.lk.peer().mac, p.b.cfg.mac, 6));
  CHECK(!memcmp(p.b.lk.peer().mac, p.a.cfg.mac, 6));
  CHECK_EQ(p.a.lk.codice(), codice);
  CHECK(!p.a.lk.confermatoLoro());   // la CONFERMA di C non conta
  CHECK(p.a.lk.scartati() >= scartatiPrima + 4);
  CHECK(c3.lk.stato() == LINK_CERCA || c3.lk.stato() == LINK_AGGANCIO);

  // e A e B finiscono l'abbinamento tra loro, poi C cerca ancora
  CHECK(abbina(p.c, p.a, p.b, 5000));
  p.c.corri(20000);
  CHECK_EQ(p.a.lk.stato(), LINK_PRONTO);
  CHECK(!memcmp(p.a.lk.peer().mac, p.b.cfg.mac, 6));
  CHECK(c3.lk.stato() != LINK_CONFERMA && c3.lk.stato() != LINK_PRONTO);
}

// ---------------------------------------------------------------- compatibilità

TEST(Link, hash_incompatibile_viene_rifiutato) {
  Coppia p;
  p.b.cfg.hash = linkHashDati() ^ 1;   // tabelle diverse
  p.b.cfg.fw = "1.18";
  avvia(p.c, p.a);
  avvia(p.c, p.b);
  p.c.corri(5000);
  CHECK_EQ(p.a.lk.stato(), LINK_INCOMPATIBILE);
  CHECK_EQ(p.b.lk.stato(), LINK_INCOMPATIBILE);
  // le due versioni per «aggiorna il firmware»
  CHECK_STREQ(p.a.lk.versioneMia(), "1.19");
  CHECK_STREQ(p.a.lk.peer().fw, "1.18");
  CHECK_STREQ(p.b.lk.peer().fw, "1.19");
  CHECK_EQ(p.a.lk.peer().proto, (uint8_t)LINK_PROTO);
  CHECK(!p.a.lk.invia(LT_BATTAGLIA, nullptr, 0));
}

TEST(Link, versione_del_protocollo_diversa_viene_riconosciuta) {
  Coppia p;
  avvia(p.c, p.a);
  // un PokeGotchi futuro: intestazione con versione 2, HELLO con il solo prefisso fisso
  uint8_t corpo[LINK_HELLO_FISSO] = {0x11, 0x22, 0x33, 0x44, '2', '.', '5'};
  uint8_t b[LINK_FRAME_MAX];
  uint8_t n = linkCodifica(b, sizeof(b), LINK_PROTO + 1, LT_HELLO, 1, corpo, sizeof(corpo));
  p.c.inietta(p.b.cfg.mac, p.a.i, b, n);
  CHECK_EQ(p.a.lk.stato(), LINK_INCOMPATIBILE);
  CHECK_EQ(p.a.lk.peer().proto, (uint8_t)(LINK_PROTO + 1));
  CHECK_STREQ(p.a.lk.peer().fw, "2.5");
  CHECK_EQ(p.a.lk.peer().hash, 0x44332211u);
}
