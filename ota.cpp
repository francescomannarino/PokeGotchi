#include "ota.h"
#include <Preferences.h>
#include <ctype.h>
#include <string.h>
#include <stdlib.h>

// ---------------------------------------------------------------- versioni

bool otaLeggiVersione(const char *s, OtaVersione *v) {
  if (!s) return false;
  if (*s == 'v' || *s == 'V') s++;
  uint32_t parti[3] = { 0, 0, 0 };
  int n = 0;
  while (n < 3) {
    if (!isdigit((unsigned char)*s)) return false;
    uint32_t x = 0;
    while (isdigit((unsigned char)*s)) {
      x = x * 10 + (uint32_t)(*s++ - '0');
      if (x > 0xFFFF) return false;
    }
    parti[n++] = x;
    if (*s != '.') break;
    s++;
  }
  // dopo le cifre: fine, oppure un suffisso ("-rc1", "+abc") che si ignora
  if (*s && *s != '-' && *s != '+') return false;
  v->maggiore = (uint16_t)parti[0];
  v->minore = (uint16_t)parti[1];
  v->patch = (uint16_t)parti[2];
  return true;
}

int otaConfronta(const OtaVersione &a, const OtaVersione &b) {
  if (a.maggiore != b.maggiore) return a.maggiore < b.maggiore ? -1 : 1;
  if (a.minore != b.minore) return a.minore < b.minore ? -1 : 1;
  if (a.patch != b.patch) return a.patch < b.patch ? -1 : 1;
  return 0;
}

bool otaPiuRecente(const char *nuova, const char *attuale) {
  OtaVersione n, a;
  return otaLeggiVersione(nuova, &n) && otaLeggiVersione(attuale, &a) && otaConfronta(n, a) > 0;
}

// ---------------------------------------------------------------- manifest

static const char *saltaSpazi(const char *p) {
  while (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n') p++;
  return p;
}

// Valore di `"chiave": ...` in un oggetto JSON piatto: una stringa (senza virgolette, con gli
// escape \" \\ \/ risolti) o un numero intero. false se manca o non ci sta in `out`.
static bool campo(const char *json, const char *chiave, char *out, size_t max, bool *stringa) {
  size_t lk = strlen(chiave);
  for (const char *p = strchr(json, '"'); p; p = strchr(p + 1, '"')) {
    if (strncmp(p + 1, chiave, lk) != 0 || p[1 + lk] != '"') continue;
    const char *q = saltaSpazi(p + 2 + lk);
    if (*q != ':') continue;  // era un valore uguale alla chiave, non la chiave
    q = saltaSpazi(q + 1);
    size_t n = 0;
    if (*q == '"') {
      *stringa = true;
      for (q++; *q && *q != '"'; q++) {
        char c = *q;
        if (c == '\\') {
          c = *++q;
          if (c != '"' && c != '\\' && c != '/') return false;  // \n, \uXXXX: non nostri
        }
        if (n + 1 >= max) return false;
        out[n++] = c;
      }
      if (*q != '"') return false;
    } else {
      *stringa = false;
      while (isdigit((unsigned char)*q)) {
        if (n + 1 >= max) return false;
        out[n++] = *q++;
      }
      if (n == 0) return false;
    }
    out[n] = 0;
    return true;
  }
  return false;
}

bool otaLeggiManifest(const char *json, OtaManifest *m, bool consentiHttp) {
  if (!json || !m) return false;
  OtaManifest r;
  char num[12];
  bool s;
  if (!campo(json, "version", r.versione, sizeof(r.versione), &s) || !s) return false;
  OtaVersione v;
  if (!otaLeggiVersione(r.versione, &v)) return false;
  if (!campo(json, "url", r.url, sizeof(r.url), &s) || !s) return false;
  bool https = strncmp(r.url, "https://", 8) == 0;
  if (!https && !(consentiHttp && strncmp(r.url, "http://", 7) == 0)) return false;
  if (!campo(json, "size", num, sizeof(num), &s) || s) return false;
  unsigned long size = strtoul(num, nullptr, 10);
  if (size == 0 || size > OTA_SIZE_MAX) return false;
  r.size = (uint32_t)size;
  if (!campo(json, "sha256", r.sha256, sizeof(r.sha256), &s) || !s) return false;
  if (strlen(r.sha256) != 64) return false;
  for (char *c = r.sha256; *c; c++) {
    if (!isxdigit((unsigned char)*c)) return false;
    *c = (char)tolower((unsigned char)*c);
  }
  *m = r;
  return true;
}

static char gUrlProva[OTA_URL_MAX] = "";

const char *otaUrlManifest() { return gUrlProva[0] ? gUrlProva : OTA_URL_MANIFEST; }

void otaImpostaUrl(const char *url) {
  if (!url) url = "";
  strncpy(gUrlProva, url, sizeof(gUrlProva) - 1);
  gUrlProva[sizeof(gUrlProva) - 1] = 0;
}

bool otaUrlProva() { return gUrlProva[0] != 0; }

// ---------------------------------------------------------------- credenziali

bool otaCredenziali(char *ssid, size_t maxSsid, char *pass, size_t maxPass) {
  Preferences p;
  if (!p.begin(OTA_NS, true)) return false;  // mai scritto
  bool ok = p.isKey("ssid") && p.getString("ssid", ssid, maxSsid) > 0 && ssid[0];
  if (ok) {
    pass[0] = 0;
    if (p.isKey("pass")) p.getString("pass", pass, maxPass);
  }
  p.end();
  return ok;
}

bool otaSalvaCredenziali(const char *ssid, const char *pass) {
  size_t ls = strlen(ssid), lp = strlen(pass);
  if (ls == 0 || ls > OTA_SSID_MAX || lp > OTA_PASS_MAX) return false;
  Preferences p;
  if (!p.begin(OTA_NS, false)) return false;
  bool ok = p.putString("ssid", ssid) == ls;
  // una rete aperta non ha password: la chiave si toglie (putString("") restituirebbe 0)
  if (ok) ok = lp ? p.putString("pass", pass) == lp : (p.remove("pass"), true);
  p.end();
  return ok;
}

void otaDimentica() {
  Preferences p;
  if (!p.begin(OTA_NS, false)) return;
  p.clear();
  p.end();
}

// ---------------------------------------------------------------- regole

bool otaBatteriaOk(int percento, bool usb) {
  return usb || percento >= OTA_BATTERIA_MIN;
}
