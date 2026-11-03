#!/usr/bin/env python3
"""Scarica da pret/pokered i dati di battaglia Gen 1 e scrive i file versionati.

  python3 tools/fetch_gen1.py              # scrive dex_types.py, la SPECIALE e dex_moves.py
  python3 tools/fetch_gen1.py --controlla  # in più confronta a campione con PokeAPI

La fonte è il disassembly di Pokémon Rosso/Blu (https://github.com/pret/pokered):
- data/pokemon/base_stats.asm + base_stats/*.asm: tipi, Speciale e mosse al livello 1;
- data/types/type_matchups.asm: la tabella dei tipi (solo le celle diverse da 1×);
- data/moves/moves.asm: le 165 mosse (effetto, potenza, tipo, precisione, PP);
- data/battle/critical_hit_moves.asm: le mosse con brutti colpi frequenti;
- data/pokemon/evos_moves.asm: le mosse imparate salendo di livello (senza MT).
Gli slug delle mosse (chiavi di MOSSE, servono ai nomi localizzati) vengono da PokeAPI.

I generatori (gen_dex.py) non vanno in rete: leggono i file scritti qui. Lo script è diviso
in funzioni (scarica / leggi_* / scrivi_*).

Il modello degli effetti delle mosse (EFFETTI: nome + significato dei parametri) è derivato
da DylanPDao/TamaPoke (MIT), esteso agli effetti della Gen 1.
Copyright (c) 2026 Quique Tortosa e contributori di DylanPDao/TamaPoke, licenza MIT
(testo in LICENSE).
"""
import json
import os
import re
import subprocess
import sys
import time

POKERED = 'https://raw.githubusercontent.com/pret/pokered/master/'
POKEAPI = 'https://pokeapi.co/api/v2/'
TOOLS = os.path.dirname(os.path.abspath(__file__))

# Ordine dei tipi = indice di TYPE_CHART. È l'ordine delle costanti di pokered
# (constants/type_constants.asm) senza BIRD e i buchi: i primi 8 sono fisici, gli altri 7
# speciali, come in Gen 1. Chiavi uguali a quelle di TYPE_ACCENTS in dex_data.py.
TIPI_POKERED = (
    ('NORMAL', 'normale'),
    ('FIGHTING', 'lotta'),
    ('FLYING', 'volante'),
    ('POISON', 'veleno'),
    ('GROUND', 'terra'),
    ('ROCK', 'roccia'),
    ('BUG', 'coleottero'),
    ('GHOST', 'spettro'),
    ('FIRE', 'fuoco'),
    ('WATER', 'acqua'),
    ('GRASS', 'erba'),
    ('ELECTRIC', 'elettro'),
    ('PSYCHIC_TYPE', 'psico'),
    ('ICE', 'ghiaccio'),
    ('DRAGON', 'drago'),
)
PRIMO_SPECIALE = 8  # indice di FIRE: da qui in poi i tipi sono speciali
DA_POKERED = dict(TIPI_POKERED)
TIPI = tuple(it for _, it in TIPI_POKERED)
DA_POKEAPI = {'normal': 'normale', 'fighting': 'lotta', 'flying': 'volante',
              'poison': 'veleno', 'ground': 'terra', 'rock': 'roccia', 'bug': 'coleottero',
              'ghost': 'spettro', 'fire': 'fuoco', 'water': 'acqua', 'grass': 'erba',
              'electric': 'elettro', 'psychic': 'psico', 'ice': 'ghiaccio', 'dragon': 'drago'}
MOLTIPLICATORI = {'SUPER_EFFECTIVE': 2, 'NOT_VERY_EFFECTIVE': 0.5, 'NO_EFFECT': 0}


def scarica(url):
    for tentativo in range(4):
        r = subprocess.run(['curl', '-sf', '--max-time', '25', '-A', 'Mozilla/5.0', url],
                           capture_output=True, text=True)
        if r.returncode == 0 and r.stdout.strip():
            return r.stdout
        time.sleep(1 + tentativo)
    raise SystemExit(f'impossibile scaricare {url}')


def scarica_pokered(percorso):
    return scarica(POKERED + percorso)


def file_base_stats():
    """I 151 percorsi in ordine di Pokédex: base_stats.asm include 1..150, Mew è a parte."""
    elenco = re.findall(r'INCLUDE "(data/pokemon/base_stats/\w+\.asm)"',
                        scarica_pokered('data/pokemon/base_stats.asm'))
    elenco.append('data/pokemon/base_stats/mew.asm')
    if len(elenco) != 151:
        raise SystemExit(f'base_stats.asm: attesi 151 file, trovati {len(elenco)}')
    return elenco


def leggi_base_stats(testo, percorso):
    """Da un base_stats/*.asm: (nome DEX_, (hp, atk, def, vel, spc), (tipo1, tipo2|None),
    mosse al livello 1 come costanti di pokered)."""
    dex = re.search(r'db (DEX_\w+) ; pokedex id', testo)
    stats = re.search(r'db\s+(\d+),\s*(\d+),\s*(\d+),\s*(\d+),\s*(\d+)\s*\n\s*;\s*hp', testo)
    tipi = re.search(r'db (\w+), (\w+) ; type', testo)
    mosse = re.search(r'db (\w+), (\w+), (\w+), (\w+) ; level 1 learnset', testo)
    if not (dex and stats and tipi and mosse):
        raise SystemExit(f'{percorso}: formato inatteso')
    t1, t2 = (DA_POKERED[t] for t in tipi.groups())
    lv1 = tuple(m for m in mosse.groups() if m != 'NO_MOVE')
    return (dex.group(1), tuple(int(v) for v in stats.groups()), (t1, None if t2 == t1 else t2),
            lv1)


def leggi_specie():
    """{num: (stats, tipi, mosse_lv1, nome_file)} per le 151 specie, dai file di pokered."""
    out = {}
    for num, percorso in enumerate(file_base_stats(), 1):
        _, stats, tipi, lv1 = leggi_base_stats(scarica_pokered(percorso), percorso)
        out[num] = (stats, tipi, lv1, os.path.splitext(os.path.basename(percorso))[0])
        if num % 25 == 0:
            print(f'  {num}/151...')
    return out


def leggi_tabella_tipi():
    """Tabella [attaccante][difensore] nell'ordine di TIPI; le celle assenti valgono 1×."""
    tabella = [[1] * len(TIPI) for _ in TIPI]
    testo = scarica_pokered('data/types/type_matchups.asm')
    righe = re.findall(r'db (\w+),\s+(\w+),\s+(\w+)', testo)
    if not righe:
        raise SystemExit('type_matchups.asm: formato inatteso')
    for att, dif, eff in righe:
        tabella[TIPI.index(DA_POKERED[att])][TIPI.index(DA_POKERED[dif])] = MOLTIPLICATORI[eff]
    return tabella


def scrivi_dex_types(specie, tabella):
    path = os.path.join(TOOLS, 'dex_types.py')
    larg = max(len(t) for t in TIPI)
    with open(path, 'w', encoding='utf-8') as f:
        f.write('# -*- coding: utf-8 -*-\n')
        f.write('"""GENERATO da tools/fetch_gen1.py da pret/pokered - non modificare a mano.\n\n')
        f.write('Tipi e tabella dei tipi di Pokémon Rosso/Blu.\n\n')
        f.write("- TIPI: l'ordine è fisso e fa da indice di TYPE_CHART; le chiavi sono quelle\n")
        f.write('  di TYPE_ACCENTS in dex_data.py.\n')
        f.write('- CATEGORIA: in Gen 1 fisico/speciale dipende dal tipo della mossa.\n')
        f.write('- TIPI_GEN1: num -> (tipo1, tipo2|None). Non sostituisce il campo tipo di DEX,\n')
        f.write('  che guida colori e biomi.\n')
        f.write('- TABELLA_TIPI[attaccante][difensore], indici di TIPI, valori in {0, 0.5, 1, 2}.\n')
        f.write('  Fedele a Rosso/Blu: Spettro->Psico 0x (bug storico), Veleno<->Coleottero 2x,\n')
        f.write('  Ghiaccio->Fuoco 1x.\n"""\n\n')
        f.write('TIPI = (\n')
        for i, t in enumerate(TIPI):
            f.write(f"    {t!r},{' ' * (larg - len(t))}  # {i}\n")
        f.write(')\n\n')
        f.write('CATEGORIA = {\n')
        for i, t in enumerate(TIPI):
            cat = 'speciale' if i >= PRIMO_SPECIALE else 'fisica'
            f.write(f"    {t!r}:{' ' * (larg - len(t))} {cat!r},\n")
        f.write('}\n\n')
        f.write('TIPI_GEN1 = {\n')
        for num in sorted(specie):
            f.write(f'    {num}: {specie[num][1]!r},\n')
        f.write('}\n\n')
        f.write('# righe: attaccante; colonne: difensore (' +
                ' '.join(t[:3] for t in TIPI) + ')\n')
        f.write('TABELLA_TIPI = (\n')
        for t, riga in zip(TIPI, tabella):
            celle = ', '.join(f'{v:>3}' for v in riga)
            f.write(f"    ({celle}),  # {t}\n")
        f.write(')\n')
    print(f'scritto {os.path.relpath(path)}')


def scrivi_speciale(specie):
    """Riscrive dex_stats.py aggiungendo SPECIALE; BASE_STATS resta com'è."""
    sys.path.insert(0, TOOLS)
    from dex_stats import BASE_STATS
    path = os.path.join(TOOLS, 'dex_stats.py')
    with open(path, 'w', encoding='utf-8') as f:
        f.write('# GENERATO da PokeAPI (pokemon_stats.csv): num -> (hp, atk, def, vel)\n')
        f.write('BASE_STATS = {\n')
        for num in sorted(BASE_STATS):
            f.write(f'    {num}: {BASE_STATS[num]},\n')
        f.write('}\n\n')
        f.write('# GENERATO da tools/fetch_gen1.py (pret/pokered): num -> Speciale base Gen 1\n')
        f.write('SPECIALE = {\n')
        for num in sorted(specie):
            f.write(f'    {num}: {specie[num][0][4]},\n')
        f.write('}\n')
    print(f'scritto {os.path.relpath(path)}')


# --- Mosse e learnset ------------------------------------------------------------------

# Stati di battaglia (indice = valore). Durano solo la lotta, non toccano il pet.
STATI = ('nessuno', 'paralisi', 'scottatura', 'veleno', 'tossico', 'sonno', 'congelamento',
         'confusione', 'tentennamento')
# Statistiche con stadi in Gen 1 (indice = valore di p1 per STADIO/STADIO_SECONDARIO).
STATISTICHE = ('attacco', 'difesa', 'velocita', 'speciale', 'precisione', 'elusione')
STAT_POKERED = ('ATTACK', 'DEFENSE', 'SPEED', 'SPECIAL', 'ACCURACY', 'EVASION')

# Il contratto con il motore: nome dell'effetto -> significato di p1 e p2.
# Indice = valore dell'enum in moves.h. Modello e primi 12 nomi derivati da DylanPDao
# (EF_NONE..EF_CHARGE); gli altri coprono gli effetti di Rosso/Blu. Si aggiunge in fondo.
EFFETTI = (
    ('NESSUNO', 'danno semplice'),
    ('STADIO', 'mossa di stato: p1 = statistica (STATISTICHE), p2 = stadi; positivi su chi '
               'la usa, negativi sul bersaglio'),
    ('RINCULO', 'p1 = denominatore: chi la usa subisce 1/p1 del danno inflitto'),
    ('ASSORBI', 'p1 = % del danno inflitto recuperata; p2 = 1: fallisce se il bersaglio '
                'non dorme (Mangiasogni)'),
    ('DANNO_LIVELLO', 'danno pari al livello di chi la usa, ignora statistiche e tipi; '
                      'p1 = 100 esatto, 150 = casuale tra 1 e 1,5 volte il livello'),
    ('DANNO_FISSO', 'p1 = danno fisso, ignora statistiche'),
    ('PRIORITA', 'p1 = priorità: agisce prima delle mosse con priorità minore'),
    ('MAI_FALLISCE', 'salta il tiro di precisione'),
    ('MULTIPLO', 'p1 = colpi (0 = da 2 a 5), potenza per colpo; p2 = % di avvelenare '
                 '(Doppio Ago)'),
    ('CURA', 'mossa di stato: p1 = % dei PS massimi recuperata'),
    ('RICARICA', 'chi la usa perde il turno seguente (non se manda KO, come in Gen 1)'),
    ('CARICA', 'il primo turno carica, il secondo colpisce; p1 = 1: invulnerabile mentre '
               'carica'),
    ('STADIO_SECONDARIO', 'danno + p2 % di abbassare di 1 stadio la statistica p1 del '
                          'bersaglio'),
    ('STATO', 'mossa di stato: infligge lo stato p1 (STATI) se va a segno'),
    ('STATO_SECONDARIO', 'danno + p2 % di infliggere lo stato p1 (STATI); paralisi, '
                         'scottatura e congelamento non colpiscono un bersaglio del tipo '
                         'della mossa, il veleno non colpisce i tipi veleno'),
    ('RIPOSO', 'mossa di stato: recupera tutti i PS e chi la usa dorme 2 turni'),
    ('PARASSISEME', 'mossa di stato: ogni turno 1/16 dei PS massimi passa dal bersaglio a '
                    'chi la usa; non sul tipo erba'),
    ('TRASFORMAZIONE', 'copia specie, tipi, statistiche (non i PS) e mosse del bersaglio, '
                       'con 5 PP ciascuna'),
    ('META_PS', 'danno pari a metà dei PS attuali del bersaglio, minimo 1'),
    ('KO', 'manda KO in un colpo; fallisce se chi la usa è più lento'),
    ('ESPLOSIONE', 'chi la usa va KO; la difesa del bersaglio conta la metà'),
    ('CALCIO_SALTO', 'se manca, chi la usa subisce 1 PS di danno'),
    ('INTRAPPOLA', 'colpisce per 2-5 turni e intanto il bersaglio non può agire'),
    ('FURIA', 'si ripete per 2-3 turni, poi chi la usa resta confuso'),
    ('IRA', 'si ripete fino alla fine; ogni colpo subito alza di 1 stadio l\'attacco'),
    ('BRUTTO_COLPO', 'danno con probabilità di brutto colpo molto più alta'),
    ('NEBBIA', 'mossa di stato: protegge chi la usa dagli abbassamenti di stadio'),
    ('CONCENTRAZIONE', 'mossa di stato: aumenta i brutti colpi (in Gen 1 per un bug li '
                       'riduce: decide il motore)'),
    ('BARRIERA', 'mossa di stato: p1 = 0 raddoppia la difesa (Riflesso), 1 la speciale '
                 '(Schermoluce), finché chi la usa resta in campo'),
    ('AZZERA', 'mossa di stato: azzera gli stadi di entrambi e gli stati volatili'),
    ('CONVERSIONE', 'mossa di stato: chi la usa prende i tipi del bersaglio'),
    ('NULLA', 'non succede niente (Splash): la mossa si può comunque scegliere'),
)
NOMI_EFFETTI = tuple(n for n, _ in EFFETTI)

RIPIEGO = 'STRUGGLE'  # Scontro: usata quando una specie non ha mosse; mai nei learnset
# Escluse da MOSSE e dai learnset, come nel piano: effetti troppo complessi, oppure nessun
# effetto nelle lotte tra allenatori (le uniche del gioco).
ESCLUSE = {
    'METRONOME': 'effetto troppo complesso',
    'MIMIC': 'effetto troppo complesso',
    'MIRROR_MOVE': 'effetto troppo complesso',
    'COUNTER': 'effetto troppo complesso',
    'BIDE': 'effetto troppo complesso',
    'SUBSTITUTE': 'effetto troppo complesso',
    'DISABLE': 'effetto troppo complesso',
    'TELEPORT': 'nessun effetto nelle lotte tra allenatori',
    'ROAR': 'nessun effetto nelle lotte tra allenatori',
    'WHIRLWIND': 'nessun effetto nelle lotte tra allenatori',
}
# Commenti scritti sopra le voci di APPRENDIMENTO.
CASI_LIMITE = {
    63: 'Abra conosce solo Teletrasporto, escluso: usa Scontro finché non evolve (16).',
    129: 'Magikarp conosce solo Splash (NULLA) e impara Azione al 15.',
    132: 'Ditto ha solo Trasformazione, che il motore implementa.',
}

# Effetto di pokered -> (effetto, p1, p2). Le probabilità sono quelle di
# engine/battle/effects.asm (SIDE_EFFECT1 10%, SIDE_EFFECT2 30%, veleno 20%/40%,
# confusione 10%, abbassamento di statistica 33%).
_SECONDARI = {
    'POISON_SIDE_EFFECT1': ('veleno', 20), 'POISON_SIDE_EFFECT2': ('veleno', 40),
    'BURN_SIDE_EFFECT1': ('scottatura', 10), 'BURN_SIDE_EFFECT2': ('scottatura', 30),
    'FREEZE_SIDE_EFFECT1': ('congelamento', 10), 'FREEZE_SIDE_EFFECT2': ('congelamento', 30),
    'PARALYZE_SIDE_EFFECT1': ('paralisi', 10), 'PARALYZE_SIDE_EFFECT2': ('paralisi', 30),
    'FLINCH_SIDE_EFFECT1': ('tentennamento', 10), 'FLINCH_SIDE_EFFECT2': ('tentennamento', 30),
    'CONFUSION_SIDE_EFFECT': ('confusione', 10),
}
_STATO = {'SLEEP_EFFECT': 'sonno', 'POISON_EFFECT': 'veleno', 'PARALYZE_EFFECT': 'paralisi',
          'CONFUSION_EFFECT': 'confusione'}
_SEMPLICI = {
    'NO_ADDITIONAL_EFFECT': ('NESSUNO', 0, 0),
    'PAY_DAY_EFFECT': ('NESSUNO', 0, 0),  # niente soldi nel gioco: resta il danno
    'DRAIN_HP_EFFECT': ('ASSORBI', 50, 0),
    'DREAM_EATER_EFFECT': ('ASSORBI', 50, 1),
    'EXPLODE_EFFECT': ('ESPLOSIONE', 0, 0),
    'SWIFT_EFFECT': ('MAI_FALLISCE', 0, 0),
    'CONVERSION_EFFECT': ('CONVERSIONE', 0, 0),
    'HAZE_EFFECT': ('AZZERA', 0, 0),
    'THRASH_PETAL_DANCE_EFFECT': ('FURIA', 0, 0),
    'TWO_TO_FIVE_ATTACKS_EFFECT': ('MULTIPLO', 0, 0),
    'ATTACK_TWICE_EFFECT': ('MULTIPLO', 2, 0),
    'TWINEEDLE_EFFECT': ('MULTIPLO', 2, 20),
    'OHKO_EFFECT': ('KO', 0, 0),
    'CHARGE_EFFECT': ('CARICA', 0, 0),
    'FLY_EFFECT': ('CARICA', 1, 0),
    'SUPER_FANG_EFFECT': ('META_PS', 0, 0),
    'TRAPPING_EFFECT': ('INTRAPPOLA', 0, 0),
    'JUMP_KICK_EFFECT': ('CALCIO_SALTO', 0, 0),
    'MIST_EFFECT': ('NEBBIA', 0, 0),
    'FOCUS_ENERGY_EFFECT': ('CONCENTRAZIONE', 0, 0),
    'RECOIL_EFFECT': ('RINCULO', 4, 0),
    'HEAL_EFFECT': ('CURA', 50, 0),
    'TRANSFORM_EFFECT': ('TRASFORMAZIONE', 0, 0),
    'REFLECT_EFFECT': ('BARRIERA', 0, 0),
    'LIGHT_SCREEN_EFFECT': ('BARRIERA', 1, 0),
    'HYPER_BEAM_EFFECT': ('RICARICA', 0, 0),
    'RAGE_EFFECT': ('IRA', 0, 0),
    'LEECH_SEED_EFFECT': ('PARASSISEME', 0, 0),
    'SPLASH_EFFECT': ('NULLA', 0, 0),
}
# Casi decisi nel codice di pokered per nome della mossa, non dall'effetto.
_PER_NOME = {
    'QUICK_ATTACK': ('PRIORITA', 1, 0),
    'SEISMIC_TOSS': ('DANNO_LIVELLO', 100, 0),
    'NIGHT_SHADE': ('DANNO_LIVELLO', 100, 0),
    'PSYWAVE': ('DANNO_LIVELLO', 150, 0),
    'SONICBOOM': ('DANNO_FISSO', 20, 0),
    'DRAGON_RAGE': ('DANNO_FISSO', 40, 0),
    'TOXIC': ('STATO', STATI.index('tossico'), 0),
    'DIG': ('CARICA', 1, 0),
    'REST': ('RIPOSO', 0, 0),
    'STRUGGLE': ('RINCULO', 2, 0),
}
# Effetti che non usano la formula del danno: la potenza (1 in pokered) diventa 0.
_SENZA_POTENZA = {'DANNO_LIVELLO', 'DANNO_FISSO', 'META_PS', 'KO'}


def traduci_effetto(nome, effetto, critici):
    """(effetto, p1, p2) della nostra tabella per una mossa di pokered."""
    if nome in _PER_NOME:
        return _PER_NOME[nome]
    if nome in critici:
        return ('BRUTTO_COLPO', 0, 0)
    if effetto in _SEMPLICI:
        return _SEMPLICI[effetto]
    if effetto in _SECONDARI:
        stato, prob = _SECONDARI[effetto]
        return ('STATO_SECONDARIO', STATI.index(stato), prob)
    if effetto in _STATO:
        return ('STATO', STATI.index(_STATO[effetto]), 0)
    m = re.fullmatch(r'([A-Z]+)_(UP|DOWN)([12])_EFFECT', effetto)
    if m:
        stadi = int(m.group(3)) * (1 if m.group(2) == 'UP' else -1)
        return ('STADIO', STAT_POKERED.index(m.group(1)), stadi)
    m = re.fullmatch(r'([A-Z]+)_DOWN_SIDE_EFFECT', effetto)
    if m:
        return ('STADIO_SECONDARIO', STAT_POKERED.index(m.group(1)), 33)
    raise SystemExit(f'{nome}: effetto {effetto} non tradotto')


def leggi_slug_mosse(n):
    """Gli slug PokeAPI delle mosse 1..n, nell'ordine degli id (uguale a quello di pokered)."""
    d = json.loads(scarica(f'{POKEAPI}move/?limit={n}&offset=0'))
    slug = [r['name'] for r in d['results']]
    ids = [int(r['url'].rstrip('/').split('/')[-1]) for r in d['results']]
    if ids != list(range(1, n + 1)):
        raise SystemExit('PokeAPI: elenco delle mosse fuori ordine')
    return slug


def leggi_mosse():
    """[(num, NOME, slug, tipo, potenza, precisione, pp, effetto, p1, p2)] di tutte le
    165 mosse, escluse comprese, nell'ordine di pokered (num = id Gen 1)."""
    righe = re.findall(r'^\s*move (\w+),\s*(\w+),\s*(\d+),\s*(\w+),\s*(\d+),\s*(\d+)',
                       scarica_pokered('data/moves/moves.asm'), re.M)
    if len(righe) != 165:
        raise SystemExit(f'moves.asm: attese 165 mosse, trovate {len(righe)}')
    critici = set(re.findall(r'db (\w+)\n', scarica_pokered('data/battle/critical_hit_moves.asm')))
    slug = leggi_slug_mosse(len(righe))
    out = []
    for num, (nome, eff, pot, tipo, prec, pp) in enumerate(righe, 1):
        if nome in ESCLUSE:  # restano solo per lo slug: non finiscono in MOSSE
            effetto, p1, p2 = None, 0, 0
        else:
            effetto, p1, p2 = traduci_effetto(nome, eff, critici)
        pot = 0 if effetto in _SENZA_POTENZA else int(pot)
        out.append((num, nome, slug[num - 1], DA_POKERED[tipo], pot, int(prec), int(pp),
                    effetto, p1, p2))
    return out


def leggi_apprendimento(specie):
    """{num: [(livello, NOME)]}: mosse al livello 1 (base_stats) più quelle di evos_moves.asm."""
    testo = scarica_pokered('data/pokemon/evos_moves.asm')
    blocchi = dict(re.findall(r'^(\w+)EvosMoves:\n(.*?)(?=^\w+EvosMoves:|\Z)', testo, re.M | re.S))
    out = {}
    for num, (_, _, lv1, file) in specie.items():
        etichetta = next((k for k in blocchi if k.lower() == file), None)
        if etichetta is None:
            raise SystemExit(f'evos_moves.asm: manca il blocco di {file}')
        learnset = blocchi[etichetta].split('; Learnset', 1)[1]
        mosse = [(1, m) for m in lv1]
        mosse += [(int(lv), m) for lv, m in re.findall(r'db (\d+), (\w+)', learnset)]
        out[num] = mosse
    return out


def scrivi_dex_moves(mosse, apprendimento):
    path = os.path.join(TOOLS, 'dex_moves.py')
    slug = {nome: s for _, nome, s, *_ in mosse}
    tenute = [m for m in mosse if m[1] not in ESCLUSE]
    with open(path, 'w', encoding='utf-8') as f:
        f.write('# -*- coding: utf-8 -*-\n')
        f.write('"""GENERATO da tools/fetch_gen1.py da pret/pokered - non modificare a mano.\n\n')
        f.write('Mosse e learnset di Pokémon Rosso/Blu.\n\n')
        f.write('- MOSSE: slug PokeAPI -> (num, tipo, potenza, precisione, pp, effetto, p1, p2).\n')
        f.write('  num è l\'id della Gen 1 (1..165): è il valore da persistere, stabile anche se\n')
        f.write('  le esclusioni cambiano. potenza 0 = non usa la formula del danno; la categoria\n')
        f.write('  fisica/speciale viene dal tipo (CATEGORIA in dex_types.py).\n')
        f.write('- EFFETTI: nome -> significato di p1/p2. È il contratto con il motore.\n')
        f.write('- APPRENDIMENTO: num specie -> [(livello, slug)], solo per livello (niente MT),\n')
        f.write('  senza le mosse di ESCLUSE né doppioni. Il livello 1 sono le mosse iniziali.\n')
        f.write('- RIPIEGO: Scontro, usata da una specie che non ha mosse; mai nei learnset.\n\n')
        f.write('Il modello degli effetti è derivato da DylanPDao/TamaPoke (MIT), esteso alla\n')
        f.write('Gen 1. Copyright (c) 2026 Quique Tortosa e contributori di DylanPDao/TamaPoke,\n')
        f.write('licenza MIT (testo in LICENSE).\n"""\n\n')
        f.write(f'STATI = {STATI!r}\n\n')
        f.write(f'STATISTICHE = {STATISTICHE!r}\n\n')
        f.write('# (nome, significato di p1/p2); indice = valore dell\'enum\n')
        f.write('EFFETTI = (\n')
        for i, (nome, testo) in enumerate(EFFETTI):
            f.write(f'    ({nome!r}, {testo!r}),  # {i}\n')
        f.write(')\n\n')
        f.write(f'RIPIEGO = {slug[RIPIEGO]!r}\n\n')
        f.write('# slug -> motivo dell\'esclusione (fuori da MOSSE e dai learnset)\n')
        f.write('ESCLUSE = {\n')
        for nome, motivo in ESCLUSE.items():
            f.write(f'    {slug[nome]!r}: {motivo!r},\n')
        f.write('}\n\n')
        f.write('# slug: (num, tipo, potenza, precisione, pp, effetto, p1, p2)\n')
        f.write('MOSSE = {\n')
        for num, nome, s, tipo, pot, prec, pp, eff, p1, p2 in tenute:
            f.write(f'    {s!r}: ({num}, {tipo!r}, {pot}, {prec}, {pp}, {eff!r}, {p1}, {p2}),'
                    f'  # {nome}\n')
        f.write('}\n\n')
        f.write('APPRENDIMENTO = {\n')
        for num in sorted(apprendimento):
            if num in CASI_LIMITE:
                f.write(f'    # {CASI_LIMITE[num]}\n')
            # le evoluzioni ripetono le mosse della forma precedente: resta il livello più basso
            viste = set()
            tenute_lv = []
            for lv, m in apprendimento[num]:
                if m not in ESCLUSE and m not in viste:
                    viste.add(m)
                    tenute_lv.append((lv, m))
            voci = ', '.join(f'({lv}, {slug[m]!r})' for lv, m in tenute_lv)
            f.write(f'    {num}: [{voci}],\n')
        f.write('}\n')
    print(f'scritto {os.path.relpath(path)}')


def controlla_pokeapi(specie, campioni=(1, 6, 35, 39, 81, 82, 94, 113, 122, 150, 151)):
    """Confronto a campione con PokeAPI: tipi Gen 1 (past_types) e Speciale (past_stats)."""
    errori = 0
    for num in campioni:
        d = json.loads(scarica(f'{POKEAPI}pokemon/{num}/'))
        tipi = [t['type']['name'] for t in d['types']]
        passati = sorted(d['past_types'], key=lambda p: int(p['generation']['url'].rstrip('/').split('/')[-1]))
        if passati:  # la voce più vecchia vale fino alla sua generazione, quindi anche in Gen 1
            tipi = [t['type']['name'] for t in passati[0]['types']]
        tipi = tuple(DA_POKEAPI[t] for t in tipi) + (None,) * (2 - len(tipi))
        spc = [s['base_stat'] for p in d['past_stats'] if p['generation']['name'] == 'generation-i'
               for s in p['stats'] if s['stat']['name'] == 'special']
        stats, nostri, *_ = specie[num]
        if tipi != nostri or spc[:1] != [stats[4]]:
            errori += 1
            print(f'  #{num} {d["name"]}: pokered {nostri} spc {stats[4]}, PokeAPI {tipi} spc {spc}')
    print(f'controllo PokeAPI: {len(campioni) - errori}/{len(campioni)} specie coincidono')
    return errori == 0


def valori_gen1(d):
    """(tipo, potenza, precisione, pp) di una mossa di PokeAPI com'erano in Gen 1.

    Ogni voce di past_values dice i valori validi fino a quel version group: per ogni campo
    vale la voce più vecchia che lo riporta, altrimenti il valore attuale."""
    gruppo = {'type': d['type']['name'], 'power': d['power'], 'accuracy': d['accuracy'],
              'pp': d['pp']}
    passati = sorted(d['past_values'],
                     key=lambda p: int(p['version_group']['url'].rstrip('/').split('/')[-1]))
    for campo in gruppo:
        for p in passati:
            v = p[campo]
            if v is not None:
                gruppo[campo] = v['name'] if campo == 'type' else v
                break
    return (DA_POKEAPI[gruppo['type']], gruppo['power'], gruppo['accuracy'], gruppo['pp'])


def controlla_mosse(mosse, apprendimento,
                    campioni=('bite', 'gust', 'karate-chop', 'sand-attack', 'wrap', 'thunder',
                              'blizzard', 'razor-wind', 'dig', 'leech-life', 'high-jump-kick'),
                    specie=(1, 25, 63, 129, 150)):
    """Confronto a campione con PokeAPI: valori Gen 1 delle mosse e learnset di red-blue."""
    errori = 0
    per_slug = {m[2]: m for m in mosse}
    for s in campioni:
        _, _, _, tipo, pot, prec, pp, eff, *_ = per_slug[s]
        api = valori_gen1(json.loads(scarica(f'{POKEAPI}move/{s}/')))
        nostri = (tipo, pot or api[1], prec if api[2] is not None else None, pp)
        if nostri != api:
            errori += 1
            print(f'  {s}: pokered {nostri}, PokeAPI {api}')
    nomi = {m[1]: m[2] for m in mosse}
    for num in specie:
        d = json.loads(scarica(f'{POKEAPI}pokemon/{num}/'))
        api = sorted((v['level_learned_at'] or 1, m['move']['name']) for m in d['moves']
                     for v in m['version_group_details']
                     if v['version_group']['name'] == 'red-blue'
                     and v['move_learn_method']['name'] == 'level-up')
        nostri = sorted((lv, nomi[m]) for lv, m in apprendimento[num])
        if nostri != api:
            errori += 1
            print(f'  #{num}: pokered {nostri}\n        PokeAPI {api}')
    totale = len(campioni) + len(specie)
    print(f'controllo PokeAPI mosse: {totale - errori}/{totale} coincidono')
    return errori == 0


def main():
    specie = leggi_specie()
    tabella = leggi_tabella_tipi()
    mosse = leggi_mosse()
    apprendimento = leggi_apprendimento(specie)
    scrivi_dex_types(specie, tabella)
    scrivi_speciale(specie)
    scrivi_dex_moves(mosse, apprendimento)
    if '--controlla' in sys.argv:
        ok = controlla_pokeapi(specie)
        if not (controlla_mosse(mosse, apprendimento) and ok):
            raise SystemExit(1)


if __name__ == '__main__':
    main()
