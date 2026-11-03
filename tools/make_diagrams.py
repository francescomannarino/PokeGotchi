#!/usr/bin/env python3
"""Zone di tocco del display rotondo, ricavate dai file della UI (ui_*.cpp), per i diagrammi di COMANDI.md.

Disegna gli SVG di docs/img/comandi/ (<nome>-chiaro.svg e <nome>-scuro.svg). Ogni costante
riporta la funzione dello sketch da cui viene: se la UI cambia, si aggiorna qui e si
rigenerano i diagrammi (TestDiagrammi fallisce se gli SVG non coincidono con l'output).

Convenzioni (le stesse dello sketch):
  - coordinate in pixel dello schermo 466x466, origine in alto a sinistra;
  - RETT = (x, y, w, h), con il bordo incluso come nei test `x >= X && x <= X + W`;
  - CERCHIO = (cx, cy, r).

  python3 tools/make_diagrams.py             # rigenera gli SVG in docs/img/comandi/
  python3 tools/make_diagrams.py --out DIR   # li scrive altrove (usato dai test)
  python3 tools/make_diagrams.py --elenco    # elenca diagrammi e zone (controllo rapido)
"""

import argparse
import math
import os

from dex_data import TYPE_ACCENTS

RADICE = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

# ---------- schermo e gesti (handleTouch) ----------

SCHERMO = 466                       # #define CX 233 / CY 233: centro dello schermo rotondo
CX, CY = 233, 233
RAGGIO_SFONDO = 231                 # renderClock()/renderKeyboard(): fillCircle(CX, CY, 231)

POLL_MS = 20                        # handleTouch(): lettura del touch a 50 Hz
PRESSIONE_LUNGA_MS = 3000           # handleTouch(): millis() - tStart > 3000 -> rilascio
PRESSIONE_LUNGA_TOLLERANZA = 30     # handleTouch(): abs(tXl - tX0) < 30 && abs(...) < 30
SWIPE_MIN = 80                      # handleTouch(): abs(dx) > 80 (o abs(dy) > 80)
SWIPE_DERIVA_MAX = 70               # handleTouch(): l'altro asse resta < 70
SWIPE_MAX_MS = 800                  # handleTouch(): dt < 800
TAP_MAX_MS = 1500                   # handleTouch(): dt < 1500
TAP_DERIVA_MAX = 40                 # handleTouch(): abs(dx) < 40 && abs(dy) < 40

# ---------- schermata principale ----------

# inPetZone(): x > 110 && x < 356 && y > 95 && y < 310 (carezza, pressione lunga di 3 s)
ZONA_PET = (110, 95, 246, 215)

# buttons[4] e BTN_HIT: cerchi di tocco da 36 px attorno ai centri delle icone
# (l'icona disegnata da drawButtons() è un quadrato da 52x52, BTN_HALF 26)
BTN_HIT = 36
BTN_HALF = 26
PULSANTI = {
    'cibo':     (140, 390, BTN_HIT),   # buttons[0] SPR_ICON_FOOD  -> menu del cibo
    'gioco':    (202, 404, BTN_HIT),   # buttons[1] SPR_ICON_PLAY  -> startGame()
    'luce':     (264, 404, BTN_HIT),   # buttons[2] SPR_ICON_LIGHT -> pet.toggleLight()
    'bagnetto': (326, 390, BTN_HIT),   # buttons[3] SPR_ICON_CLEAN -> startBath()
}

# EVO_BTN_*: pulsante rosso "evolvi", centrato (onTap / drawEvolveButton)
EVO_BTN = (CX - 256 // 2, 172, 256, 64)
# FAR_BTN_*: stesso riquadro per addio (dorato) e fuga (scuro) (onTap / drawFarewellButton)
FAR_BTN = (CX - 408 // 2, 176, 408, 58)

# render(): menu del cibo, 4 voci da 66 px (onTap: item = (x - 101) / 66), aperto 6 s
CIBO_MENU = (101, 288, 264, 64)
CIBO_VOCE_W = 66
CIBO_VOCI = ('bacca', 'bacca blu', 'bacca verde', 'dolcetto')  # SPR_ICON_FOOD/BERRY_B/BERRY_G/CANDY
CIBO_MENU_MS = 6000                 # mainTap(): openOverlay(OVL_FEED, 6000)

# ---------- scelta dell'iniziale (renderStarterSelect / onTap) ----------

STARTER_ROW_Y = 110                 # #define STARTER_ROW_Y
STARTER_ROW_H = 70                  # #define STARTER_ROW_H
STARTER_ROW_GAP = 8                 # #define STARTER_ROW_GAP
STARTER_RIGHE = tuple(              # onTap(): x >= 70 && x <= 396, una riga per iniziale
    (70, STARTER_ROW_Y + i * (STARTER_ROW_H + STARTER_ROW_GAP), 326, STARTER_ROW_H)
    for i in range(3))
STARTER_NOMI = ('Bulbasaur', 'Charmander', 'Squirtle')  # STARTER_DEX = { 1, 4, 7 }

# ---------- dialoghi ----------

# render(): "rilasciare?" (dopo la pressione lunga), 10 s; solo SI' agisce
RILASCIO_DIALOGO = (94, 168, 278, 152)
RILASCIO_SI = (118, 252, 100, 52)   # onTap(): x 118..218, y 252..304
RILASCIO_NO = (248, 252, 100, 52)   # disegnato; qualunque altro tocco annulla
RILASCIO_MS = 10000                 # handleTouch(): openOverlay(OVL_CONFIRM, 10000)

# drawChoiceDialog(): evolvere/mantenere, addio/restare insieme, 12 s
SCELTA_DIALOGO = (73, 156, 320, 188)
SCELTA_AZIONE = (93, 206, 280, 52)  # onTap(): b1
SCELTA_MANTIENI = (93, 268, 280, 52)  # onTap(): b2
SCELTA_MS = 12000                   # onTap(): choiceUntil = millis() + 12000

# ---------- scheda delle statistiche (renderCard / onTap) ----------

SCHEDA_PAGINE = ('Profilo', 'Lotta', 'Medaglie', 'Progressi')  # cardPage 0..3
SCHEDA_NOME_Y_MAX = 84              # onTap(): cardPage == 0 && y < 84 -> goScreen(SCR_KEYBOARD)
SCHEDA_ALLENA = (30, 298, 156, 44)   # ui_card.cpp: CS_TRAIN_X, CS_BTN_Y, CS_TRAIN_W, CS_BTN_H
SCHEDA_LOTTA = (190, 298, 121, 44)   # ui_card.cpp: CS_FIGHT_X, CS_BTN_W (cardPage == 1)
SCHEDA_COLLEGA = (315, 298, 121, 44)  # ui_card.cpp: CS_LINK_X -> openLink()

# ---------- galleria / Pokédex (renderGallery / galleryTap / onSwipe) ----------

INTESTAZIONE_Y_MAX = 72             # galleryTap(), gameTap(), handleTouch() (sacco): y < 72
GAL_X, GAL_Y, GAL_CELL = 73, 74, 80  # #define GAL_X / GAL_Y / GAL_CELL
GAL_COLONNE = GAL_RIGHE = 4         # galleryTap(): c > 3 || r > 3 -> ignora
GAL_PAGINE = 10                     # onSwipe(): np > 9 -> 9 (16 per pagina, 151 in tutto)
GAL_GRIGLIA = (GAL_X, GAL_Y, GAL_COLONNE * GAL_CELL, GAL_RIGHE * GAL_CELL)
GAL_COLLEZIONE = (123, 396, 220, 56)  # galleryTap(): GAL_COL_TY/TH, CX ± GAL_COL_TW / 2

# ---------- orologio e impostazioni (renderClock / clockTap) ----------

OROLOGIO_BTN = {                    # drawClockBtn(): quadrati da 58x58
    'ora -':    (104, 190, 58, 58),
    'ora +':    (170, 190, 58, 58),
    'minuti -': (252, 190, 58, 58),
    'minuti +': (318, 190, 58, 58),
}
LANG_PILL_Y, LANG_PILL_H = 296, 30  # #define LANG_PILL_Y / LANG_PILL_H
SUONO = (34, LANG_PILL_Y, 96, LANG_PILL_H)    # clockTap(): x 34..130, apre il pannello Audio
LINGUA = (336, LANG_PILL_Y, 96, LANG_PILL_H)  # #define LANG_PILL_X 336 / LANG_PILL_W 96
SALVATAGGI = (141, LANG_PILL_Y, 184, LANG_PILL_H)  # #define SAVES_PILL_X 141 / SAVES_PILL_W 184
OROLOGIO_OK = (133, 340, 200, 48)  # clockTap(): applyClock()
OROLOGIO_SCHEDE = {                 # #define CLK_TAB_ORA_X / CLK_TAB_DATA_X, CLK_TAB_Y 42, 120x30
    'scheda ora':  (109, 42, 120, 30),
    'scheda data': (237, 42, 120, 30),
}

# ---------- pannello Audio (renderAudio / audioTap) ----------

AUDIO_HDR_Y = 80                    # #define AUDIO_HDR_Y: y < 80 chiude
AUDIO_MENO = (50, 118, 80, 80)      # #define AUDIO_MENO_X / AUDIO_BTN_Y / AUDIO_BTN
AUDIO_PIU = (336, 118, 80, 80)      # #define AUDIO_PIU_X
AUDIO_BARRA = (144, 132, 176, 44)   # #define AUDIO_BAR_X / AUDIO_BAR_Y / AUDIO_BAR_H: 10 tacche
AUDIO_MUSICA = (113, 250, 240, 50)  # #define AUDIO_MUS_X / Y / W / H
AUDIO_MOVIMENTO = (113, 310, 240, 50)  # #define AUDIO_MOT_Y, stesse x e misure della musica
AUDIO_INDIETRO_Y = 380              # #define AUDIO_BACK_Y: y >= 380 chiude

# ---------- minigioco della pokéball (gameTap / stepGame) ----------

PALLA_HIT = 74                      # gameTap(): dx * dx + dy * dy < 74 * 74
PALLA_PARETE = 205                  # stepGame(): rimbalzo sulla parete circolare d > 205
PALLA_TERRA_Y = 384                 # stepGame(): ballY > 384 = palla persa
GIOCO_ERRORI = 3                    # stepGame(): ++gameMisses >= 3 -> fine partita
GIOCO_FINE_MS = 4000                # stepGame(): gameOverUntil = millis() + 4000

# ---------- sacco da allenamento (startSack / renderSack) ----------

SACCO_MS = 10000                    # startSack(): sackUntil = millis() + 10000
SACCO_FINE_MS = 3500                # renderSack(): sackOverUntil = now + 3500

# ---------- tastiera del nome (renderKeyboard / keyboardTap) ----------

KB_KEYS = 'ABCDEFGHIJKLMNOPQRSTUVWXYZ.-'  # KB_KEYS[]: 28 lettere, poi <- e OK
KB_COLS, KB_RIGHE = 6, 5            # #define KB_COLS; keyboardTap(): row >= 5 -> ignora
KB_X, KB_Y, KB_W, KB_H = 40, 150, 64, 52  # #define KB_X / KB_Y / KB_W / KB_H (tasto = W-6 x H-6)
KB_NOME_MAX = 11                    # char nameBuf[12]
KB_CASELLA = (83, 84, 300, 40)      # renderKeyboard(): riquadro del nome

# ---------- pulsante fisico PWR (loop / rtcbat.cpp) ----------

PWR_POLL_MS = 250                   # loop(): now - lastPwr > 250
PWR_SPEGNIMENTO_S = 4               # pwrSetup(): setPowerKeyPressOffTime(XPOWERS_POWEROFF_4S)

# ---------- luminosità (updateBrightness) ----------

ATTENUA_MS = 90000                  # updateBrightness(): idle > 90000 -> dimStage 1
ATTENUA2_MS = 300000                # updateBrightness(): idle > 300000 -> dimStage 2

# ---------- diagrammi di COMANDI.md (segnaposto <!-- diagramma: <nome> -->) ----------

DIAGRAMMI = {
    'iniziale':   {'righe': STARTER_RIGHE},
    'principale': {'pet': ZONA_PET, 'pulsanti': PULSANTI, 'evolvi': EVO_BTN,
                   'finale': FAR_BTN},
    'cibo':       {'menu': CIBO_MENU},
    'dialoghi':   {'rilascio_si': RILASCIO_SI, 'rilascio_no': RILASCIO_NO,
                   'azione': SCELTA_AZIONE, 'mantieni': SCELTA_MANTIENI},
    'scheda':     {'nome': (0, 0, SCHERMO, SCHEDA_NOME_Y_MAX), 'allena': SCHEDA_ALLENA,
                   'lotta': SCHEDA_LOTTA, 'collega': SCHEDA_COLLEGA},
    'galleria':   {'intestazione': (0, 0, SCHERMO, INTESTAZIONE_Y_MAX), 'griglia': GAL_GRIGLIA,
                   'collezione': GAL_COLLEZIONE},
    'orologio':   {**OROLOGIO_SCHEDE, **OROLOGIO_BTN, 'volume': SUONO, 'salvataggi': SALVATAGGI, 'lingua': LINGUA,
                   'ok': OROLOGIO_OK},
    'audio':      {'intestazione': (0, 0, SCHERMO, AUDIO_HDR_Y), 'meno': AUDIO_MENO,
                   'piu': AUDIO_PIU, 'musica': AUDIO_MUSICA, 'movimento': AUDIO_MOVIMENTO,
                   'indietro': (0, AUDIO_INDIETRO_Y, SCHERMO, SCHERMO - AUDIO_INDIETRO_Y)},
    'minigioco':  {'intestazione': (0, 0, SCHERMO, INTESTAZIONE_Y_MAX)},
    'sacco':      {'intestazione': (0, 0, SCHERMO, INTESTAZIONE_Y_MAX)},
    'tastiera':   {'tasti': (KB_X, KB_Y, KB_COLS * KB_W, KB_RIGHE * KB_H)},
}


# ---------- disegno degli SVG ----------
#
# SVG statici e deterministici: niente script, CSS o font esterni (GitHub li mostra come
# <img>). Le zone si disegnano alle coordinate dello schermo dentro un gruppo traslato e
# ritagliato sul cerchio del display; frecce ed etichette stanno sopra, senza ritaglio.

ROSSO = '#E7352C'                   # rosso Pokéball dei badge del README
A = TYPE_ACCENTS

PANNELLO_W = 560                    # un pannello = un display con cornice e legenda
OX, OY = (PANNELLO_W - SCHERMO) // 2, 34   # angolo dello schermo nel pannello
LEGENDA_RIGA = 32
CORNICE_R = 241

TEMI = {
    'chiaro': {'sfondo': '#ffffff', 'schermo': '#f4f1ea', 'testo': '#1f1f24',
               'tenue': '#62626c', 'riempimento': '0.16'},
    'scuro':  {'sfondo': '#0d1117', 'schermo': '#17171d', 'testo': '#f0f0f4',
               'tenue': '#a3a3ad', 'riempimento': '0.26'},
}

DIR_SVG = 'docs/img/comandi'


def _n(v):
    s = f'{v:.1f}'
    return s[:-2] if s.endswith('.0') else s


def _esc(s):
    return s.replace('&', '&amp;').replace('<', '&lt;').replace('>', '&gt;').replace('"', '&quot;')


def testo(t, x, y, s, dim=13, colore=None, peso=None, ancora='middle', alone=False):
    attr = (f'x="{_n(x)}" y="{_n(y)}" font-size="{dim}" fill="{colore or t["testo"]}" '
            f'text-anchor="{ancora}"')
    if peso:
        attr += f' font-weight="{peso}"'
    if alone:                       # bordo del colore dello schermo, leggibile sopra le zone
        attr += (f' stroke="{t["schermo"]}" stroke-width="4" stroke-linejoin="round" '
                 'paint-order="stroke"')
    return f'<text {attr}>{_esc(s)}</text>'


def etichetta(t, x, y, s, dim=13, **kw):
    """Testo su più righe (separate da \\n), centrato in verticale su y."""
    righe = s.split('\n')
    passo = dim * 1.25
    y0 = y - passo * (len(righe) - 1) / 2 + dim * 0.35
    return [testo(t, x, y0 + i * passo, r, dim, **kw) for i, r in enumerate(righe)]


def zona(t, r, colore, s=None, dim=13, tratteggio=False, rx=8, vuota=False, **kw):
    x, y, w, h = r
    riemp = 'none' if vuota else f'{colore}" fill-opacity="{t["riempimento"]}'
    dash = ' stroke-dasharray="8 6"' if tratteggio else ''
    out = [f'<rect x="{x}" y="{y}" width="{w}" height="{h}" rx="{rx}" fill="{riemp}" '
           f'stroke="{colore}" stroke-width="2.5"{dash}/>']
    if s:
        out += etichetta(t, x + w / 2, y + h / 2, s, dim, **kw)
    return out


def zona_cerchio(t, c, colore, s=None, dim=12, tratteggio=False):
    cx, cy, r = c
    dash = ' stroke-dasharray="8 6"' if tratteggio else ''
    out = [f'<circle cx="{cx}" cy="{cy}" r="{r}" fill="{colore}" '
           f'fill-opacity="{t["riempimento"]}" stroke="{colore}" stroke-width="2.5"{dash}/>']
    if s:
        out += etichetta(t, cx, cy, s, dim)
    return out


def _punta(x1, y1, x2, y2, colore, lung=16, larg=9):
    a = math.atan2(y2 - y1, x2 - x1)
    bx, by = x2 - lung * math.cos(a), y2 - lung * math.sin(a)
    px, py = -math.sin(a) * larg, math.cos(a) * larg
    punti = ' '.join(f'{_n(x)},{_n(y)}' for x, y in
                     ((x2, y2), (bx + px, by + py), (bx - px, by - py)))
    return f'<polygon points="{punti}" fill="{colore}"/>'


def freccia(x1, y1, x2, y2, colore, doppia=False, spessore=4):
    """Freccia di un gesto di scorrimento, da (x1, y1) a (x2, y2)."""
    a = math.atan2(y2 - y1, x2 - x1)
    rit = 10                        # la linea finisce sotto la punta
    ex, ey = x2 - rit * math.cos(a), y2 - rit * math.sin(a)
    sx, sy = (x1 + rit * math.cos(a), y1 + rit * math.sin(a)) if doppia else (x1, y1)
    out = [f'<line x1="{_n(sx)}" y1="{_n(sy)}" x2="{_n(ex)}" y2="{_n(ey)}" stroke="{colore}" '
           f'stroke-width="{spessore}" stroke-linecap="round"/>',
           _punta(x1, y1, x2, y2, colore)]
    if doppia:
        out.append(_punta(x2, y2, x1, y1, colore))
    return out


def icona(t, tipo, cx, cy, colore):
    """Icone della legenda e dei gesti: dito, mouse, freccia, tasto (tipo 'tasto:PWR')."""
    if tipo == 'dito':
        return [f'<circle cx="{cx}" cy="{cy - 13}" r="9" fill="none" stroke="{colore}" '
                'stroke-width="1.5" stroke-opacity="0.7"/>',
                f'<rect x="{cx - 4}" y="{cy - 14}" width="8" height="18" rx="4" fill="{colore}"/>',
                f'<rect x="{cx - 9}" y="{cy - 2}" width="18" height="15" rx="6" fill="{colore}"/>']
    if tipo == 'mouse':
        return [f'<rect x="{cx - 9}" y="{cy - 14}" width="18" height="28" rx="9" fill="none" '
                f'stroke="{colore}" stroke-width="2.5"/>',
                f'<line x1="{cx}" y1="{cy - 14}" x2="{cx}" y2="{cy - 4}" stroke="{colore}" '
                'stroke-width="2.5"/>']
    if tipo == 'freccia':
        return freccia(cx - 12, cy, cx + 13, cy, colore, spessore=3)
    if tipo.startswith('tasto:'):
        return [f'<rect x="{cx - 17}" y="{cy - 12}" width="34" height="24" rx="5" fill="none" '
                f'stroke="{colore}" stroke-width="2.5"/>',
                testo(t, cx, cy + 4, tipo[6:], 10, colore, 'bold')]
    raise ValueError(tipo)


LEGENDA_BASE = (('dito', 'tocco sulla scheda (ESP32)'), ('mouse', 'clic nella PWA'))
LEGENDA_SCORRI = ('freccia', 'scorrimento (> 80 px)')


def legenda(t, righe):
    out = []
    y0 = OY + SCHERMO + 34
    for i, riga in enumerate(righe):
        y = y0 + i * LEGENDA_RIGA
        for j, (tipo, s) in enumerate(riga):
            x = 46 + j * 252
            colore = ROSSO if tipo.startswith('tasto') else t['testo']
            out += icona(t, tipo, x, y, colore)
            out.append(testo(t, x + (30 if tipo.startswith('tasto') else 20), y + 5, s, 13,
                             ancora='start'))
    return out


def pokeball(t, cx, cy, r):
    return [f'<circle cx="{cx}" cy="{cy}" r="{r}" fill="#ffffff" stroke="#1f1f24" '
            'stroke-width="2.5"/>',
            f'<path d="M{cx - r},{cy} A{r},{r} 0 0 1 {cx + r},{cy} Z" fill="{ROSSO}" '
            'stroke="#1f1f24" stroke-width="2.5"/>',
            f'<circle cx="{cx}" cy="{cy}" r="{r // 3}" fill="#ffffff" stroke="#1f1f24" '
            'stroke-width="2.5"/>']


# Ogni pannello restituisce (zone ritagliate sul cerchio, elementi sopra, righe di legenda).

def p_iniziale(t):
    zone, sopra = [], []
    for r, nome, tipo in zip(STARTER_RIGHE, STARTER_NOMI, ('erba', 'fuoco', 'acqua')):
        zone += zona(t, r, A[tipo], nome, 15, peso='bold')
    x, y, w, h = STARTER_RIGHE[0]
    sopra += icona(t, 'dito', x + w - 34, y + h / 2 + 2, t['testo'])
    sopra += etichetta(t, CX, 82, 'tocca una riga per scegliere', 13, colore=t['tenue'])
    sopra += etichetta(t, CX, 378, 'scorrimenti bloccati\nfinché non scegli', 12,
                       colore=t['tenue'])
    return zone, sopra, [LEGENDA_BASE]


def p_principale(t):
    zone = zona(t, ZONA_PET, A['psico'])
    zone += zona(t, FAR_BTN, A['elettro'], tratteggio=True, vuota=True)
    zone += zona(t, EVO_BTN, ROSSO, 'evolvi (se compare)', 13, tratteggio=True, alone=True)
    for (nome, c), tipo in zip(PULSANTI.items(), ('fuoco', 'elettro', 'ghiaccio', 'acqua')):
        zone += zona_cerchio(t, c, A[tipo], nome, 11)
    x, y, w, h = ZONA_PET
    sopra = etichetta(t, CX, y + 16, 'creatura', 13, peso='bold')
    sopra += etichetta(t, CX, y + h - 34, 'tocco: carezza\n3 s fermo: rilascio', 12)
    fx, fy, fw, fh = FAR_BTN
    sopra += etichetta(t, (fx + EVO_BTN[0]) / 2, fy + fh / 2, 'addio\nfuga', 11, alone=True)
    sopra += freccia(150, 58, 316, 58, A['drago'], doppia=True)
    sopra += etichetta(t, CX, 80, 'Pokédex', 12, colore=A['drago'], peso='bold')
    sopra += freccia(62, 378, 62, 270, A['drago'])     # sotto la fascia di addio/fuga
    sopra += etichetta(t, 62, 254, 'scheda', 12, colore=A['drago'], peso='bold')
    sopra += freccia(404, 270, 404, 378, A['drago'])
    sopra += etichetta(t, 404, 254, 'orologio', 12, colore=A['drago'], peso='bold')
    return zone, sopra, [LEGENDA_BASE, (LEGENDA_SCORRI, ('tasto:PWR', 'breve: schermo · 4 s: spegne'))]


def p_cibo(t):
    zone = []
    for c in PULSANTI.values():     # la schermata principale sotto il menu, solo come contesto
        zone.append(f'<circle cx="{c[0]}" cy="{c[1]}" r="{c[2]}" fill="none" '
                    f'stroke="{t["tenue"]}" stroke-width="1.5" stroke-dasharray="4 5"/>')
    x, y, w, h = CIBO_MENU
    etichette = ('bacca', 'bacca\nblu', 'bacca\nverde', 'dolcetto')
    for i, (s, tipo) in enumerate(zip(etichette, ('fuoco', 'acqua', 'erba', 'psico'))):
        zone += zona(t, (x + i * CIBO_VOCE_W, y, CIBO_VOCE_W, h), A[tipo], s, 11, rx=4)
    sopra = icona(t, 'dito', x + CIBO_VOCE_W / 2, y - 22, t['testo'])
    sopra += etichetta(t, CX, 200, 'tocca una voce per dare da mangiare', 13)
    sopra += etichetta(t, CX, 230, f'tocco fuori: chiude · aperto {CIBO_MENU_MS // 1000} s',
                       12, colore=t['tenue'])
    return zone, sopra, [LEGENDA_BASE]


def p_rilascio(t):
    zone = zona(t, RILASCIO_DIALOGO, t['tenue'], vuota=True)
    zone += zona(t, RILASCIO_SI, ROSSO, 'SÌ: rilascia', 13, peso='bold')
    zone += zona(t, RILASCIO_NO, A['normale'], 'NO', 13, peso='bold')
    x, y, w, h = RILASCIO_DIALOGO
    sopra = etichetta(t, CX, 118, 'dopo la pressione lunga', 12, colore=t['tenue'])
    sopra += etichetta(t, CX, y + 40, 'Rilasciare?', 16, peso='bold')
    sopra += etichetta(t, CX, y + h + 30,
                       f'altro tocco: annulla · {RILASCIO_MS // 1000} s', 12, colore=t['tenue'])
    return zone, sopra, [LEGENDA_BASE]


def p_scelta(t):
    zone = zona(t, SCELTA_DIALOGO, t['tenue'], vuota=True)
    zone += zona(t, SCELTA_AZIONE, ROSSO, 'evolvi / addio', 13, peso='bold')
    zone += zona(t, SCELTA_MANTIENI, A['erba'], 'mantieni / restiamo insieme', 13, peso='bold')
    x, y, w, h = SCELTA_DIALOGO
    sopra = etichetta(t, CX, 118, 'evoluzione o fine del ciclo', 12, colore=t['tenue'])
    sopra += etichetta(t, CX, y + 26, 'Scelta', 16, peso='bold')
    sopra += etichetta(t, CX, y + h + 26,
                       f'altro tocco: chiude · {SCELTA_MS // 1000} s', 12, colore=t['tenue'])
    return zone, sopra, [LEGENDA_BASE]


def p_scheda(t):
    zone = zona(t, (0, 0, SCHERMO, SCHEDA_NOME_Y_MAX), A['drago'], rx=0)
    zone += zona(t, SCHEDA_ALLENA, A['lotta'], 'Allena forza', 12, rx=6)
    zone += zona(t, SCHEDA_LOTTA, A['fuoco'], 'Lotta', 12, rx=6)
    zone += zona(t, SCHEDA_COLLEGA, A['acqua'], 'Collega', 12, rx=6)
    sopra = etichetta(t, CX, 62, 'nome (Profilo): tastiera', 13, peso='bold')
    sopra += freccia(130, 196, 336, 196, A['drago'], doppia=True)
    sopra += etichetta(t, CX, 176, 'pagina precedente / successiva', 12, colore=A['drago'],
                       peso='bold')
    sopra += freccia(62, 262, 62, 150, A['drago'])
    sopra += etichetta(t, 62, 284, 'chiude', 12, colore=A['drago'], peso='bold')
    sopra += etichetta(t, CX, 250, 'altro tocco: chiude', 13)
    sopra += etichetta(t, CX, 362, 'pulsanti: pagina Lotta', 11, colore=t['tenue'])
    sopra += etichetta(t, CX, 390, ' · '.join(SCHEDA_PAGINE), 11, colore=t['tenue'])
    return zone, sopra, [LEGENDA_BASE, (LEGENDA_SCORRI,)]


def p_galleria(t):
    zone = zona(t, (0, 0, SCHERMO, INTESTAZIONE_Y_MAX), A['normale'], rx=0)
    for r in range(GAL_RIGHE):
        for c in range(GAL_COLONNE):
            zone += zona(t, (GAL_X + c * GAL_CELL + 3, GAL_Y + r * GAL_CELL + 3,
                             GAL_CELL - 6, GAL_CELL - 6), A['acqua'], rx=6)
    sopra = etichetta(t, CX, 52, 'intestazione: esce', 13, peso='bold')
    sopra += freccia(330, 200, 136, 200, A['drago'])
    sopra += etichetta(t, CX, 182, 'pagina successiva', 12, colore=A['drago'], peso='bold',
                       alone=True)
    sopra += freccia(136, 290, 330, 290, A['drago'])
    sopra += etichetta(t, CX, 272, 'precedente (dalla 1ª esce)', 12, colore=A['drago'],
                       peso='bold', alone=True)
    zone += zona(t, GAL_COLLEZIONE, A['elettro'], 'collezione', 12, alone=True)
    sopra += etichetta(t, CX, 386, 'tocco su una cella: dettaglio', 12, peso='bold', alone=True)
    return zone, sopra, [LEGENDA_BASE, (LEGENDA_SCORRI,)]


def p_orologio(t):
    zone = []
    nomi = {'ora -': 'ora −', 'ora +': 'ora +', 'minuti -': 'min −', 'minuti +': 'min +'}
    for k, r in OROLOGIO_BTN.items():
        zone += zona(t, r, A['acqua' if k.startswith('ora') else 'erba'], nomi[k], 13,
                     peso='bold')
    zone += zona(t, SUONO, A['elettro'], 'volume', 12, rx=15)
    zone += zona(t, SALVATAGGI, A['terra'], 'salvataggi', 12, rx=15)
    zone += zona(t, LINGUA, A['psico'], 'lingua', 12, rx=15)
    zone += zona(t, OROLOGIO_SCHEDE['scheda ora'], A['normale'], 'ORA', 12, rx=8)
    zone += zona(t, OROLOGIO_SCHEDE['scheda data'], A['normale'], 'DATA', 12, rx=8)
    zone += zona(t, OROLOGIO_OK, ROSSO, "OK: salva data e ora", 14, peso='bold')
    sopra = freccia(56, 140, 56, 260, A['drago'], doppia=True)
    sopra += freccia(410, 140, 410, 260, A['drago'], doppia=True)
    sopra += etichetta(t, CX, 112, 'scorrimento su o giù:\nchiude senza salvare', 12,
                       colore=A['drago'], peso='bold')
    sopra += etichetta(t, CX, 168, 'scheda DATA: anno, mese, giorno −/+', 11,
                       colore=t['tenue'])
    sopra += etichetta(t, CX, 410, 'in fondo: versione del firmware', 11, colore=t['tenue'])
    return zone, sopra, [LEGENDA_BASE, (LEGENDA_SCORRI,)]


def p_audio(t):
    zone = zona(t, (0, 0, SCHERMO, AUDIO_HDR_Y), A['normale'], rx=0)
    zone += zona(t, AUDIO_MENO, A['acqua'], 'volume −', 13, peso='bold', rx=16)
    zone += zona(t, AUDIO_PIU, A['erba'], 'volume +', 13, peso='bold', rx=16)
    zone += zona(t, AUDIO_BARRA, A['normale'], '10 tacche', 11, tratteggio=True, rx=4)
    zone += zona(t, AUDIO_MUSICA, A['elettro'], 'musica sì / no', 13, rx=12)
    zone += zona(t, AUDIO_MOVIMENTO, A['lotta'], 'movimento sì / no', 13, rx=12)
    zone += zona(t, (0, AUDIO_INDIETRO_Y, SCHERMO, SCHERMO - AUDIO_INDIETRO_Y), A['normale'],
                 rx=0)
    sopra = etichetta(t, CX, 46, 'intestazione: torna indietro', 13, peso='bold')
    sopra += etichetta(t, CX, 222, 'a ogni tocco un bip\nal volume nuovo', 11,
                       colore=t['tenue'])
    sopra += freccia(56, 340, 56, 230, A['drago'], doppia=True)
    sopra += freccia(410, 340, 410, 230, A['drago'], doppia=True)
    sopra += etichetta(t, CX, 404, 'in fondo: torna indietro', 12, peso='bold')
    sopra += etichetta(t, CX, 426, 'come uno scorrimento su o giù', 11, colore=A['drago'],
                       peso='bold')
    return zone, sopra, [LEGENDA_BASE, (LEGENDA_SCORRI,)]


def p_minigioco(t):
    zone = zona(t, (0, 0, SCHERMO, INTESTAZIONE_Y_MAX), A['normale'], rx=0)
    zone.append(f'<circle cx="{CX}" cy="{CY}" r="{PALLA_PARETE}" fill="none" '
                f'stroke="{t["tenue"]}" stroke-width="2" stroke-dasharray="6 6"/>')
    zone.append(f'<rect x="0" y="{PALLA_TERRA_Y}" width="{SCHERMO}" '
                f'height="{SCHERMO - PALLA_TERRA_Y}" fill="{A["fuoco"]}" '
                f'fill-opacity="{t["riempimento"]}"/>')
    zone.append(f'<line x1="0" y1="{PALLA_TERRA_Y}" x2="{SCHERMO}" y2="{PALLA_TERRA_Y}" '
                f'stroke="{A["fuoco"]}" stroke-width="2.5" stroke-dasharray="8 6"/>')
    bx, by = CX, 216
    zone += zona_cerchio(t, (bx, by, PALLA_HIT), A['elettro'], tratteggio=True)
    sopra = etichetta(t, CX, 52, 'intestazione: esce senza premio', 13, peso='bold',
                        alone=True)
    sopra += pokeball(t, bx, by, 20)
    sopra += etichetta(t, CX, by + PALLA_HIT + 18, f'tocco entro {PALLA_HIT} px: colpo', 12,
                       peso='bold')
    sopra += etichetta(t, CX, PALLA_TERRA_Y - 16, f'parete: raggio {PALLA_PARETE}', 11,
                       colore=t['tenue'])
    sopra += etichetta(t, CX, PALLA_TERRA_Y + 26,
                       f'a terra: persa ({GIOCO_ERRORI} = fine)', 12, colore=A['fuoco'],
                       peso='bold')
    return zone, sopra, [LEGENDA_BASE]


def p_sacco(t):
    zone = zona(t, (0, 0, SCHERMO, INTESTAZIONE_Y_MAX), A['normale'], rx=0)
    zone += zona(t, (0, INTESTAZIONE_Y_MAX, SCHERMO, SCHERMO - INTESTAZIONE_Y_MAX), A['lotta'],
                 rx=0)
    sopra = etichetta(t, CX, 52, 'intestazione: esce', 13, peso='bold')
    sopra += icona(t, 'dito', CX, 214, t['testo'])
    sopra += etichetta(t, CX, 262, 'ovunque: un colpo\nappena il dito si appoggia', 14,
                       peso='bold')
    sopra += etichetta(t, CX, 320, f'{SACCO_MS // 1000} s per colpire · risultato per '
                       f'{SACCO_FINE_MS / 1000:.1f} s'.replace('.', ','), 12, colore=t['tenue'])
    return zone, sopra, [LEGENDA_BASE]


def p_tastiera(t):
    zone = zona(t, KB_CASELLA, A['drago'], f'nome (al massimo {KB_NOME_MAX})', 13)
    tasti = list(KB_KEYS) + ['<-', 'OK']
    for i, s in enumerate(tasti):
        c, r = i % KB_COLS, i // KB_COLS
        colore = ROSSO if s == 'OK' else A['fuoco'] if s == '<-' else A['acqua']
        zone += zona(t, (KB_X + c * KB_W, KB_Y + r * KB_H, KB_W - 6, KB_H - 6), colore, s, 15,
                     rx=6, peso='bold')
    sopra = etichetta(t, CX, 434, 'OK salva sempre: niente annulla', 12, colore=t['tenue'])
    return zone, sopra, [LEGENDA_BASE]


# nome -> (titolo, pannelli, testo alternativo). L'ordine segue COMANDI.md.
SCHEDE = {
    'iniziale': ('Scelta dell\'iniziale', (p_iniziale,),
                 'Display rotondo con tre righe una sotto l\'altra, Bulbasaur, Charmander e '
                 'Squirtle: un tocco su una riga sceglie l\'iniziale; gli scorrimenti sono '
                 'bloccati finché non scegli.'),
    'principale': ('Schermata principale', (p_principale,),
                   'Display rotondo della schermata principale: al centro la zona della '
                   'creatura (tocco per la carezza, dito fermo 3 secondi per il rilascio); '
                   'sopra, tratteggiati, i pulsanti evolvi e addio che compaiono solo quando '
                   'servono; in basso quattro pulsanti rotondi, cibo, gioco, luce e '
                   'bagnetto. Frecce: scorrimento orizzontale per il Pokédex, verso l\'alto '
                   'per la scheda, verso il basso per l\'orologio. Il tasto PWR spegne lo '
                   'schermo con una pressione breve e la scheda con 4 secondi.'),
    'cibo': ('Menu del cibo', (p_cibo,),
             'Menu del cibo sopra i pulsanti della schermata principale: quattro voci '
             'affiancate, bacca, bacca blu, bacca verde e dolcetto; un tocco fuori dal menu lo '
             'chiude, che altrimenti resta aperto 6 secondi.'),
    'dialoghi': ('Dialoghi', (p_rilascio, p_scelta),
                 'Due display affiancati. A sinistra il dialogo «Rilasciare?» con i pulsanti '
                 'SÌ, che rilascia, e NO; qualunque altro tocco annulla, dura 10 secondi. A '
                 'destra il dialogo di scelta con «evolvi / addio» sopra e «mantieni / '
                 'restiamo insieme» sotto; un tocco fuori lo chiude, dura 12 secondi.'),
    'scheda': ('Scheda delle statistiche', (p_scheda,),
               'Scheda delle statistiche: la fascia in alto con il nome apre la tastiera '
               '(pagina Profilo); nella pagina Lotta «Allena forza» apre il sacco, '
               '«Lotta» una battaglia contro un Pokémon selvatico e «Collega» il collegamento '
               'con un altro PokeGotchi. Scorrimento orizzontale per cambiare pagina tra Profilo, '
               'Lotta, Medaglie e Progressi, verso l\'alto per chiudere; ogni altro '
               'tocco chiude.'),
    'galleria': ('Galleria / Pokédex', (p_galleria,),
                 'Galleria del Pokédex: griglia di 4 per 4 celle sotto un\'intestazione che, '
                 'toccata, esce. Un tocco su una cella apre il dettaglio; scorrimento a '
                 'sinistra per la pagina successiva, a destra per la precedente o, dalla '
                 'prima, per uscire. Sotto la griglia il pulsante Collezione.'),
    'orologio': ('Orologio e impostazioni', (p_orologio,),
                 'Orologio e impostazioni: in alto le schede ORA e DATA; quattro pulsanti '
                 'quadrati, ora meno, ora più, minuti meno e minuti più (nella scheda DATA '
                 'sei: anno, mese e giorno, meno e più); sotto, a sinistra il volume, che '
                 'apre il pannello Audio, al centro i salvataggi e a destra la lingua; '
                 'in basso il pulsante OK che salva data e ora. Uno scorrimento verso l\'alto o '
                 'verso il basso chiude senza salvare.'),
    'audio': ('Pannello Audio', (p_audio,),
              'Pannello Audio: in alto i pulsanti grandi volume meno e volume più, tra loro '
              'la barra di 10 tacche con il numero; sotto le pillole musica sì o no e '
              'movimento sì o no (scossone e colpetti sulla schermata principale). '
              'L\'intestazione, il fondo o uno scorrimento verticale tornano alle '
              'impostazioni.'),
    'minigioco': ('Minigioco della pokéball', (p_minigioco,),
                  'Minigioco: una pokéball con un cerchio tratteggiato di 74 pixel attorno, '
                  'dove un tocco la colpisce; la parete circolare su cui rimbalza; in basso la '
                  'fascia di terra dove la palla è persa, e alla terza la partita finisce. '
                  'L\'intestazione esce senza premio.'),
    'sacco': ('Sacco da allenamento', (p_sacco,),
              'Sacco da allenamento: tutto lo schermo sotto l\'intestazione è un\'unica zona '
              'in cui ogni tocco è un colpo; 10 secondi per colpire. L\'intestazione esce.'),
    'tastiera': ('Tastiera del nome', (p_tastiera,),
                 'Tastiera del nome: in alto la casella del nome, al massimo 11 caratteri; '
                 'sotto una griglia di 6 colonne per 5 righe con le lettere dalla A alla Z, '
                 'il punto e il trattino, poi il tasto cancella e il tasto OK, che salva.'),
}
assert tuple(SCHEDE) == tuple(DIAGRAMMI)


def svg(nome, tema):
    titolo, pannelli, alt = SCHEDE[nome]
    t = TEMI[tema]
    contenuti = [p(t) for p in pannelli]
    righe = max(len(leg) for _, _, leg in contenuti)
    w = PANNELLO_W * len(pannelli)
    h = OY + SCHERMO + 34 + LEGENDA_RIGA * (righe - 1) + 26
    out = ['<?xml version="1.0" encoding="UTF-8"?>',
           '<!-- GENERATO da tools/make_diagrams.py: non modificare a mano -->',
           f'<svg xmlns="http://www.w3.org/2000/svg" width="{w}" height="{h}" '
           f'viewBox="0 0 {w} {h}" font-family="sans-serif" role="img" '
           'aria-labelledby="titolo descrizione">',
           f'<title id="titolo">{_esc(titolo)}</title>',
           f'<desc id="descrizione">{_esc(alt)}</desc>',
           f'<rect width="{w}" height="{h}" rx="24" fill="{t["sfondo"]}"/>']
    for i, (zone, sopra, leg) in enumerate(contenuti):
        out.append(f'<g transform="translate({i * PANNELLO_W},0)">')
        out.append(f'<circle cx="{OX + CX}" cy="{OY + CY}" r="{CORNICE_R}" fill="none" '
                   f'stroke="{ROSSO}" stroke-width="8"/>')
        out.append(f'<g transform="translate({OX},{OY})">')
        out.append(f'<clipPath id="schermo{i}"><circle cx="{CX}" cy="{CY}" r="{CX}"/></clipPath>')
        out.append(f'<circle cx="{CX}" cy="{CY}" r="{CX}" fill="{t["schermo"]}"/>')
        out.append(f'<g clip-path="url(#schermo{i})">')
        out += zone
        out.append('</g>')
        out += sopra
        out.append('</g>')
        out += legenda(t, leg)
        out.append('</g>')
    out.append('</svg>')
    return '\n'.join(out) + '\n'


def file_svg(nome, tema):
    return f'{nome}-{tema}.svg'


def picture(nome):
    """Il blocco <picture> che sostituisce <!-- diagramma: nome --> in COMANDI.md."""
    titolo, pannelli, alt = SCHEDE[nome]
    larg = 420 * len(pannelli)
    return (f'<picture>\n'
            f'  <source media="(prefers-color-scheme: dark)" '
            f'srcset="{DIR_SVG}/{file_svg(nome, "scuro")}">\n'
            f'  <img src="{DIR_SVG}/{file_svg(nome, "chiaro")}" width="{larg}" '
            f'alt="{_esc(alt)}">\n'
            f'</picture>')


def genera(cartella):
    os.makedirs(cartella, exist_ok=True)
    scritti = []
    for nome in SCHEDE:
        for tema in TEMI:
            percorso = os.path.join(cartella, file_svg(nome, tema))
            with open(percorso, 'w', encoding='utf-8', newline='\n') as fh:
                fh.write(svg(nome, tema))
            scritti.append(percorso)
    return scritti


def main():
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument('--out', default=os.path.join(RADICE, DIR_SVG),
                    help=f'cartella di uscita (predefinita: {DIR_SVG})')
    ap.add_argument('--elenco', action='store_true', help='elenca solo diagrammi e zone')
    args = ap.parse_args()
    if args.elenco:
        for nome, zone in DIAGRAMMI.items():
            print(f'{nome}: {", ".join(zone)}')
        return
    for percorso in genera(args.out):
        print(percorso)


if __name__ == '__main__':
    main()
