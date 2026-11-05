#!/usr/bin/env python3
"""Scarica da PokeAPI i nomi ufficiali per lingua e scrive tools/dex_names.py.

  python3 tools/gen_names.py          # specie -> tools/dex_names.py
  python3 tools/gen_names.py mosse    # mosse  -> tools/dex_move_names.py

Solo FR e DE hanno nomi propri in gen 1; ES/IT/PT usano quelli inglesi, quindi
qui si conserva unicamente ciò che differisce dall'inglese. Il font GFX è
ASCII, quindi i nomi escono in maiuscolo e senza accenti.

Le mosse invece hanno un nome proprio anche in ES e IT (PokeAPI non ha il
portoghese: il PT usa l'inglese, come per le specie). Stessi vincoli dei nomi
delle specie, con i limiti di MAX_MOSSA e MAX_MOSSA_CJK; ciò che sfora va
accorciato a mano in ABBREVIAZIONI, altrimenti lo script si ferma.
"""
import json
import os
import subprocess
import sys
import time
import unicodedata

LANGS = ('fr', 'de')
LANGS_UTF8 = ('ja-hrkt', 'ko')  # CJK: vanno così come sono in UTF-8, non in ottale ASCII

# Convenzione di genere per lingua: né la tabella CP437 né alcun sottoinsieme unifont di
# U8g2 contengono ♀/♂, quindi ogni lingua sceglie con cosa sostituirli. Il percorso
# latino usa F/M da sempre. Il coreano usa 암/수 ("femmina"/"maschio"), che si leggono
# come parola e non come lettera isolata, e vanno SEPARATI DA UNO SPAZIO: 니드런 암. Lo
# spazio sta dentro la sostituzione perché ciò che si sostituisce è solo il simbolo. Le
# due sillabe sono in KS X 1001, e compaiono già nella tabella (암나이트, 암스타, 홍수몬). Il nome
# sono quattro sillabe più uno spazio, contro voci di cinque come 깨비드릴조, quindi non è il
# più largo della tabella.
GENERE = {'ko': (' 암', ' 수')}
GENERE_PREDEFINITO = ('F', 'M')


def senza_genere(s, lang=None):
    """Nidoran femmina/maschio: ♀ e ♂ non esistono in nessun font che usiamo.

    Né la tabella CP437 di glcdfont.h li ha in una posizione stampabile, né
    li contiene alcun sottoinsieme unifont di U8g2 (japanese*, korean*, chinese*).
    E Arduino_GFX non disegna nulla quando manca il glifo, non avanza nemmeno
    il cursore, quindi senza questo i due Nidoran escono con lo stesso nome.
    """
    femmina, maschio = GENERE.get(lang, GENERE_PREDEFINITO)
    return s.replace('♀', femmina).replace('♂', maschio)


def ascii_up(s):
    """'Salamèche' -> 'SALAMECHE'; conserva i simboli di genere come f/m."""
    s = senza_genere(s)
    s = unicodedata.normalize('NFD', s)
    s = ''.join(c for c in s if unicodedata.category(c) != 'Mn')
    return s.upper()


def fetch(num, risorsa='pokemon-species'):
    url = f'https://pokeapi.co/api/v2/{risorsa}/{num}/'
    for tentativo in range(4):
        r = subprocess.run(['curl', '-s', '--max-time', '25', '-A', 'Mozilla/5.0', url],
                           capture_output=True, text=True)
        if r.returncode == 0 and r.stdout.strip().startswith('{'):
            return json.loads(r.stdout)
        time.sleep(1 + tentativo)
    raise SystemExit(f'no se pudo bajar {risorsa} {num}')


# --- nomi delle mosse ---

# Limiti per la griglia delle mosse. Il latino è il 5x7 a scala 2 (12 px per
# carattere, 12 caratteri = 144 px); il CJK è la unifont a 16 px per carattere.
# Il CJK sale a 7 (112 px, più stretto del latino): un terzo delle mosse
# giapponesi originali ha 7 kana (サイコキネシス, はかいこうせん...) e a 6 andrebbero
# tutte storpiate.
MAX_MOSSA = 12
MAX_MOSSA_CJK = 7
LINGUE_MOSSE = ('en', 'es', 'fr', 'de', 'it')  # latine; il PT ricade sull'inglese

# Accorciamenti dei nomi che non stanno in MAX_MOSSA, già nella forma finale
# (maiuscolo ASCII). Regole:
# - EN: i nomi di Rosso/Blu, senza spazi (THUNDERSHOCK, SELFDESTRUCT, HI JUMP KICK);
# - FR: i nomi della Gen 1 se esistono (LANCE-FLAMME, DANSEFLAMME);
# - altrimenti si tronca una parola con il punto, la prima se la seconda è quella
#   che distingue la mossa (ATT. RAPIDO), l'ultima se no (AUTODISTRUZ.).
ABBREVIAZIONI = {
    'en': {
        'thunder-punch': 'THUNDERPUNCH',
        'poison-powder': 'POISONPOWDER',
        'thunder-shock': 'THUNDERSHOCK',
        'self-destruct': 'SELFDESTRUCT',
        'high-jump-kick': 'HI JUMP KICK',
    },
    'es': {
        'double-slap': 'DOBLEBOFETON',
        'razor-wind': 'V. CORTANTE',
        'poison-sting': 'PICOT.VENENO',
        'string-shot': 'DISP. DEMORA',
        'quick-attack': 'ATAQ. RAPIDO',
        'smokescreen': 'PANT. HUMO',
        'light-screen': 'PANT. LUZ',
        'self-destruct': 'AUTODESTRUC.',
        'high-jump-kick': 'P.SALTO ALTA',  # PATADA SALTO è già Calcio Salto
        'transform': 'TRANSFORMAC.',
        'acid-armor': 'ARMAD. ACIDA',
        'hyper-fang': 'HIPERCOLMIL.',
    },
    'fr': {
        'flamethrower': 'LANCE-FLAMME',
        'fire-spin': 'DANSEFLAMME',
        'night-shade': 'OMBRE NOCT.',
        'glare': 'REGARD MEDU.',
    },
    'de': {
        'glare': 'SCHLANGENBL.',
    },
    'it': {
        'wing-attack': "ATT. D'ALA",
        'submission': 'SOTTOMISS.',
        'seismic-toss': 'MOV. SISMICO',
        'mega-drain': 'MEGASSORBIM.',
        'quick-attack': 'ATT. RAPIDO',
        'night-shade': 'OMBRA NOTT.',
        'self-destruct': 'AUTODISTRUZ.',
        'glare': 'SGUARDO FER.',
        'barrage': 'ATT. PIOGGIA',
        'transform': 'TRASFORMAZ.',
    },
}

# Caratteri latini che NFD non scompone: si scrivono come nella grafia senza accenti.
LEGATURE = {'’': "'", 'Œ': 'OE', 'œ': 'OE', 'Æ': 'AE', 'æ': 'AE'}


def nome_mossa_latino(s, lg, slug):
    for k, v in LEGATURE.items():
        s = s.replace(k, v)
    v = ascii_up(s)
    v = ABBREVIAZIONI.get(lg, {}).get(slug, v)
    if not v.isascii() or len(v) > MAX_MOSSA:
        raise SystemExit(f'{slug} {lg}: {v!r} ({len(v)}) va accorciato in ABBREVIAZIONI')
    return v


def nome_mossa_cjk(s, slug):
    # PokeAPI scrive a tutta larghezza le cifre di １０まんボルト: in ASCII le ha
    # qualsiasi font, e occupano mezza casella
    v = ''.join(unicodedata.normalize('NFKC', c) if '\uff01' <= c <= '\uff5e' else c for c in s)
    if len(v) > MAX_MOSSA_CJK:
        raise SystemExit(f'{slug}: {v!r} ({len(v)}) supera {MAX_MOSSA_CJK} caratteri')
    return v


def main_mosse():
    from dex_moves import MOSSE
    out = {}
    for i, slug in enumerate(sorted(MOSSE, key=lambda k: MOSSE[k][0])):
        names = {n['language']['name']: n['name'] for n in fetch(slug, 'move')['names']}
        voce = {lg: nome_mossa_latino(names[lg], lg, slug) for lg in LINGUE_MOSSE}
        voce['ja'] = nome_mossa_cjk(names['ja-hrkt'], slug)
        voce['ko'] = nome_mossa_cjk(names['ko'], slug)
        out[slug] = voce
        if (i + 1) % 25 == 0:
            print(f'  {i + 1}/{len(MOSSE)}...')
        time.sleep(0.05)

    path = os.path.join(os.path.dirname(__file__), 'dex_move_names.py')
    with open(path, 'w', encoding='utf-8') as f:
        f.write('# -*- coding: utf-8 -*-\n')
        f.write('"""GENERATO da tools/gen_names.py mosse da PokeAPI - non modificare a mano.\n\n')
        f.write('Nomi delle mosse per lingua, chiave = slug di MOSSE (dex_moves.py), in\n')
        f.write('ordine di id Gen 1. Le lingue latine in maiuscolo ASCII (al massimo\n')
        f.write(f'{MAX_MOSSA} caratteri, accorciamenti in ABBREVIAZIONI di gen_names.py); JA e KO\n')
        f.write(f'in UTF-8, al massimo {MAX_MOSSA_CJK} caratteri. Il PT usa l\'inglese.\n"""\n\n')
        f.write(f'MAX_MOSSA = {MAX_MOSSA}\nMAX_MOSSA_CJK = {MAX_MOSSA_CJK}\n\n')
        f.write('MOVE_NAMES = {\n')
        for slug, voce in out.items():
            f.write(f'    {slug!r}: {voce!r},\n')
        f.write('}\n')
    print(f'guardado {os.path.normpath(path)}: {len(out)} mosse')


def main():
    out = {}
    for num in range(1, 152):
        d = fetch(num)
        names = {n['language']['name']: n['name'] for n in d['names']}
        en = ascii_up(names['en'])
        dif = {}
        for lg in LANGS:
            v = ascii_up(names.get(lg, names['en']))
            if v != en:
                dif[lg] = v
        for lg in LANGS_UTF8:
            v = names.get(lg)
            if v:
                # 'ja-hrkt' -> 'ja'; l'UTF-8 si rispetta tranne ♀/♂, che il font non ha
                # (vedi senza_genere)
                breve = lg.split('-')[0]
                dif[breve] = senza_genere(v, breve)
        if dif:
            out[num] = dif
        if num % 25 == 0:
            print(f'  {num}/151...')
        time.sleep(0.05)

    path = os.path.join(os.path.dirname(__file__), 'dex_names.py')
    with open(path, 'w', encoding='utf-8') as f:
        f.write('# -*- coding: utf-8 -*-\n')
        f.write('"""GENERATO da tools/gen_names.py da PokeAPI - non modificare a mano.\n\n')
        f.write('Nomi ufficiali per lingua. FR e DE vanno in maiuscolo e senza\n')
        f.write('accenti (font CP437, un byte per carattere); JA e KO vanno in\n')
        f.write('UTF-8 così come sono, perché si disegnano con un font U8g2.\n"""\n\n')
        f.write('LOCAL_NAMES = {\n')
        for num in sorted(out):
            f.write(f'    {num}: {out[num]!r},\n')
        f.write('}\n')
    print(f'guardado {os.path.normpath(path)}: {len(out)} especies con nombre propio')


if __name__ == '__main__':
    if sys.argv[1:] == ['mosse']:
        main_mosse()
    else:
        main()
