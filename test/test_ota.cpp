// Test della parte senza hardware dell'aggiornamento via WiFi (ota.cpp).
#include "framework.h"
#include "shim/Arduino.h"
#include "../ota.h"
#include "../savefile.h"
#include <string.h>

static const char *SHA = "0123456789abcdef0123456789ABCDEF0123456789abcdef0123456789abcdef";

TEST(ota, le_versioni_si_leggono_e_si_confrontano) {
  OtaVersione v;
  CHECK(otaLeggiVersione("3.1.2", &v));
  CHECK_EQ(v.maggiore, (uint16_t)3);
  CHECK_EQ(v.minore, (uint16_t)1);
  CHECK_EQ(v.patch, (uint16_t)2);
  CHECK(otaLeggiVersione("v3.0.0", &v));  // il tag
  CHECK(otaLeggiVersione("2.1", &v));     // le versioni fino alla 2.1
  CHECK_EQ(v.patch, (uint16_t)0);
  CHECK(otaLeggiVersione("3.0.0-rc1", &v));
  CHECK(!otaLeggiVersione("", &v));
  CHECK(!otaLeggiVersione("tre", &v));
  CHECK(!otaLeggiVersione("3.x", &v));
  CHECK(!otaLeggiVersione("3.0.0 ", &v));
  CHECK(!otaLeggiVersione("70000.0.0", &v));

  CHECK(otaPiuRecente("3.0.1", "3.0.0"));
  CHECK(otaPiuRecente("3.1.0", "3.0.9"));
  CHECK(otaPiuRecente("3.0.0", "2.1"));
  CHECK(otaPiuRecente("3.10.0", "3.9.0"));  // numeri, non testo
  CHECK(!otaPiuRecente("3.0.0", "3.0.0"));
  CHECK(!otaPiuRecente("2.9.9", "3.0.0"));
  CHECK(!otaPiuRecente("boh", "3.0.0"));
}

TEST(ota, il_manifest_della_release_si_legge) {
  char json[512];
  snprintf(json, sizeof(json),
           "{\n  \"version\": \"3.1.0\",\n  \"url\": \"https:\\/\\/github.com/o/r/releases/"
           "download/v3.1.0/pokegotchi-3.1.0-ota.bin\",\n  \"size\": 1504752,\n"
           "  \"sha256\": \"%s\"\n}\n", SHA);
  OtaManifest m;
  CHECK(otaLeggiManifest(json, &m));
  CHECK_STREQ(m.versione, "3.1.0");
  CHECK_STREQ(m.url, "https://github.com/o/r/releases/download/v3.1.0/pokegotchi-3.1.0-ota.bin");
  CHECK_EQ(m.size, (uint32_t)1504752);
  CHECK_EQ(strlen(m.sha256), (size_t)64);
  CHECK(strchr(m.sha256, 'A') == nullptr);  // tutto minuscolo

  // chiavi in un altro ordine, senza spazi
  snprintf(json, sizeof(json),
           "{\"sha256\":\"%s\",\"size\":10,\"url\":\"https://x/y.bin\",\"version\":\"v3.0.1\"}", SHA);
  CHECK(otaLeggiManifest(json, &m));
  CHECK_STREQ(m.versione, "v3.0.1");
}

TEST(ota, un_manifest_sbagliato_si_rifiuta) {
  char json[512];
  OtaManifest m;
  strcpy(m.versione, "intatto");
  const char *fmt[] = {
    "{\"version\":\"3.1.0\",\"url\":\"https://x/y.bin\",\"size\":10}",                       // niente sha
    "{\"version\":\"3.1.0\",\"url\":\"https://x/y.bin\",\"size\":10,\"sha256\":\"abc\"}",    // sha corto
    "{\"version\":\"3.1.0\",\"url\":\"http://x/y.bin\",\"size\":10,\"sha256\":\"%s\"}",      // http
    "{\"version\":\"3.1.0\",\"url\":\"ftp://x/y.bin\",\"size\":10,\"sha256\":\"%s\"}",
    "{\"version\":\"tre\",\"url\":\"https://x/y.bin\",\"size\":10,\"sha256\":\"%s\"}",
    "{\"version\":\"3.1.0\",\"url\":\"https://x/y.bin\",\"size\":0,\"sha256\":\"%s\"}",
    "{\"version\":\"3.1.0\",\"url\":\"https://x/y.bin\",\"size\":4000000,\"sha256\":\"%s\"}",  // > 3 MB
    "{\"version\":\"3.1.0\",\"url\":\"https://x/y.bin\",\"size\":\"10\",\"sha256\":\"%s\"}",  // stringa
    "{\"version\":3,\"url\":\"https://x/y.bin\",\"size\":10,\"sha256\":\"%s\"}",              // numero
    "<html>Not Found</html>",
    "",
  };
  for (const char *f : fmt) {
    snprintf(json, sizeof(json), f, SHA);
    CHECK(!otaLeggiManifest(json, &m));
  }
  CHECK_STREQ(m.versione, "intatto");  // un rifiuto non tocca il risultato
  // un URL più lungo del buffer
  char lungo[600];
  snprintf(lungo, sizeof(lungo), "{\"version\":\"3.1.0\",\"url\":\"https://%0400d\",\"size\":10,"
           "\"sha256\":\"%s\"}", 0, SHA);
  CHECK(!otaLeggiManifest(lungo, &m));
  CHECK(!otaLeggiManifest(nullptr, &m));
}

TEST(ota, http_solo_per_le_prove) {
  char json[512];
  snprintf(json, sizeof(json), "{\"version\":\"3.1.0\",\"url\":\"http://192.168.1.2:8080/a.bin\","
           "\"size\":10,\"sha256\":\"%s\"}", SHA);
  OtaManifest m;
  CHECK(!otaLeggiManifest(json, &m));
  CHECK(otaLeggiManifest(json, &m, true));
}

TEST(ota, l_url_del_manifest_e_quello_delle_release_salvo_le_prove) {
  CHECK(strncmp(otaUrlManifest(), "https://github.com/", 19) == 0);
  CHECK(strstr(otaUrlManifest(), "/releases/latest/download/pokegotchi.json") != nullptr);
  CHECK(!otaUrlProva());
  otaImpostaUrl("http://192.168.1.2:8080/pokegotchi.json");
  CHECK(otaUrlProva());
  CHECK_STREQ(otaUrlManifest(), "http://192.168.1.2:8080/pokegotchi.json");
  otaImpostaUrl(nullptr);
  CHECK(!otaUrlProva());
  CHECK(strncmp(otaUrlManifest(), "https://", 8) == 0);
}

TEST(ota, le_credenziali_restano_fuori_dai_salvataggi) {
  mockNvsReset();
  char s[OTA_SSID_MAX + 1], p[OTA_PASS_MAX + 1];
  CHECK(!otaCredenziali(s, sizeof(s), p, sizeof(p)));
  CHECK(otaSalvaCredenziali("CasaMia", "segreta!1"));
  CHECK(otaCredenziali(s, sizeof(s), p, sizeof(p)));
  CHECK_STREQ(s, "CasaMia");
  CHECK_STREQ(p, "segreta!1");
  // una rete aperta: niente password, nemmeno quella di prima
  CHECK(otaSalvaCredenziali("Bar", ""));
  CHECK(otaCredenziali(s, sizeof(s), p, sizeof(p)));
  CHECK_STREQ(s, "Bar");
  CHECK_STREQ(p, "");
  // limiti dell'802.11 e del WPA2
  char lunga[80];
  memset(lunga, 'x', sizeof(lunga) - 1);
  lunga[sizeof(lunga) - 1] = 0;
  CHECK(!otaSalvaCredenziali("", "x"));
  CHECK(!otaSalvaCredenziali(lunga, "x"));
  CHECK(!otaSalvaCredenziali("Bar", lunga));
  lunga[OTA_PASS_MAX] = 0;
  CHECK(otaSalvaCredenziali("Bar", lunga));
  // il namespace non è tra quelli esportati
  for (uint8_t i = 0; i < SAVE_N_NAMESPACE; i++) CHECK(strcmp(SAVE_NAMESPACE[i], OTA_NS) != 0);
  otaDimentica();
  CHECK(!otaCredenziali(s, sizeof(s), p, sizeof(p)));
}

TEST(ota, si_installa_solo_con_batteria_sufficiente_o_usb) {
  CHECK(otaBatteriaOk(OTA_BATTERIA_MIN, false));
  CHECK(otaBatteriaOk(100, false));
  CHECK(!otaBatteriaOk(OTA_BATTERIA_MIN - 1, false));
  CHECK(otaBatteriaOk(5, true));
  CHECK(!otaBatteriaOk(-1, false));  // nessuna lettura e niente USB
  CHECK(otaBatteriaOk(-1, true));
}
