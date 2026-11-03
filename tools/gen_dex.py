#!/usr/bin/env python3
"""Genera dex.h (tabella del Pokédex per il firmware) da dex_data.py, e moves.h
(mosse, learnset, tabella dei tipi) da dex_moves.py, dex_types.py e dex_move_names.py.

  python3 tools/gen_dex.py
"""
import os
import sys

sys.path.insert(0, os.path.dirname(__file__))
from dex_data import DEX, TYPE_ACCENTS, CLASSIC, RARE, LEGENDARY, EVO_SCAMBIO, SLUGS
from dex_stats import BASE_STATS, SPECIALE
from dex_names import LOCAL_NAMES
from dex_types import TIPI, CATEGORIA, TIPI_GEN1, TABELLA_TIPI
from dex_moves import MOSSE, EFFETTI, STATI, STATISTICHE, APPRENDIMENTO, RIPIEGO
from dex_move_names import MOVE_NAMES


def rgb565(hexcol):
    r, g, b = int(hexcol[1:3], 16), int(hexcol[3:5], 16), int(hexcol[5:7], 16)
    return (r >> 3) << 11 | (g >> 2) << 5 | (b >> 3)


# bioma di sfondo per tipo (la luce la dà l'ora reale dell'RTC)
# 0 PRATERIA, 1 SPIAGGIA, 2 BOSCO, 3 VULCANO, 4 MONTAGNA, 5 NEVE
TYPE_BIOME = {
    'acqua': 1, 'erba': 2, 'coleottero': 2, 'fuoco': 3,
    'roccia': 4, 'terra': 4, 'drago': 1, 'ghiaccio': 5,  # i draghi gen1 (Dratini) vivono nell'acqua
    'normale': 0, 'elettro': 0, 'lotta': 0, 'veleno': 0,
    'psico': 0, 'spettro': 0,
}

# eccezioni per dex# (il tipo non basta): fossili marini roccia/acqua -> spiaggia
BIOME_OVERRIDE = {138: 1, 139: 1, 140: 1, 141: 1}  # Omanyte, Omastar, Kabuto, Kabutops

T_NESSUNO = 0xFF  # type2 delle specie a tipo singolo


def nome_c(prefisso, nome):
    """'karate-chop' -> 'MV_KARATE_CHOP'."""
    return prefisso + nome.upper().replace('-', '_')


def scrivi(nome, out):
    path = os.path.join(os.path.dirname(__file__), '..', nome)
    open(path, 'w', encoding='utf-8').write(''.join(out))
    return os.path.normpath(path)


def main():
    out = []
    out.append("#pragma once\n#include <stdint.h>\n#include \"i18n.h\"  // gLang\n\n")
    out.append("// GENERATO da tools/gen_dex.py da tools/dex_data.py - non modificare\n\n")
    out.append("#define DEX_COUNT 151\n")
    out.append("#define DEX_EEVEE 133  // ramo casuale: 134/135/136\n\n")
    out.append("// tipi di battaglia di Rosso/Blu (tools/dex_types.py): l'ordine è l'indice di\n"
               "// TYPE_CHART in moves.h; i fisici vengono prima degli speciali\n")
    out.append("#define TYPE_COUNT %d\n" % len(TIPI))
    out.append("enum : uint8_t {\n")
    for i, t in enumerate(TIPI):
        out.append(f"  {nome_c('T_', t)} = {i},\n")
    out.append(f"  T_NESSUNO = 0x{T_NESSUNO:02X},  // type2 di chi ha un solo tipo\n}};\n\n")
    out.append("// colore RGB565 di ogni tipo T_* (TYPE_ACCENTS di dex_data.py), per la UI\n"
               "static const uint16_t TYPE_COLOR[TYPE_COUNT] = {\n ")
    out.append(''.join(f" 0x{rgb565(TYPE_ACCENTS[t]):04X}," for t in TIPI))
    out.append("\n};\n\n")
    out.append(
        "// rarità: 0 = solo per evoluzione, 1 = comune, 2 = raro, 3 = leggendario\n"
        "enum : uint8_t { R_EVO = 0, R_COMUN, R_RARO, R_LEGENDARIO };\n\n"
        "struct DexEntry {\n"
        "  const char *name;\n"
        "  uint8_t evolvesTo;    // numero di dex, 0 = forma finale\n"
        "  uint8_t evolveLevel;\n"
        "  uint8_t rarity;       // esce dall'uovo se > 0\n"
        "  uint16_t accent;      // colore RGB565 del tipo per la UI\n"
        "  uint8_t bHp, bAtk, bDef, bSpe;  // statistiche base reali della gen 1\n"
        "  uint8_t biome;        // 0 prateria 1 spiaggia 2 bosco 3 vulcano 4 montagna 5 neve\n"
        "  uint8_t type1, type2; // tipi di battaglia T_*; type2 = T_NESSUNO se è uno solo\n"
        "  uint8_t bSpc;         // Speciale base della gen 1\n"
        "  uint8_t tradeEvo;     // 1 = evolve in evolvesTo anche alla ricezione di uno scambio\n"
        "};\n\n")
    # forme base = quelle che non sono evoluzione di nessuno (i rami di Eevee lo sono)
    evolved = {evo for *_, evo, _lvl in [(d[4], d[5]) for d in DEX] for evo in [_[0] for _ in [(d[4],) for d in DEX]]}
    evolved = {d[4] for d in DEX if d[4]} | {135, 136}
    rarities = []
    out.append("static const DexEntry DEX_TBL[DEX_COUNT + 1] = {\n")
    out.append('  { "?", 0, 0, 0, 0x2946, 50, 50, 50, 50, 0, T_NORMALE, T_NESSUNO, 50, 0 },  // 0: inutilizzato\n')
    for num, slug, display, typ, evo, lvl in DEX:
        acc = rgb565(TYPE_ACCENTS[typ])
        if num in evolved:
            rar = 'R_EVO'
        elif num in LEGENDARY:
            rar = 'R_LEGENDARIO'
        elif num in RARE:
            rar = 'R_RARO'
        else:
            rar = 'R_COMUN'
        rarities.append(rar)
        hp, atk, df, spe = BASE_STATS[num]
        bio = BIOME_OVERRIDE.get(num, TYPE_BIOME[typ])
        t1, t2 = TIPI_GEN1[num]
        t1, t2 = nome_c('T_', t1), nome_c('T_', t2) if t2 else 'T_NESSUNO'
        if num in EVO_SCAMBIO:
            assert EVO_SCAMBIO[num] == evo, f'EVO_SCAMBIO[{num}] diverso da evolve_in'
        trade = int(num in EVO_SCAMBIO)
        out.append(f'  {{ "{display}", {evo}, {lvl}, {rar}, 0x{acc:04X}, {hp}, {atk}, {df}, {spe}, {bio}, '
                   f'{t1}, {t2}, {SPECIALE[num]}, {trade} }},  // {num} {typ}\n')
    out.append("};\n\n")

    # Solo FR e DE hanno nome proprio in gen 1; ES/IT/PT usano l'inglese.
    out.append(
        "// Nomi ufficiali per lingua. FR e DE sono le uniche lingue latine che differiscono\n"
        "// dall'inglese in gen 1 (ES/IT/PT usano quello di DEX_TBL); JA e KO vanno in UTF-8,\n"
        "// che si disegna con il font U8g2. nullptr = senza nome proprio.\n")
    for lg in ('fr', 'de', 'ja', 'ko'):
        out.append(f"static const char *const DEX_NAME_{lg.upper()}[DEX_COUNT + 1] = {{\n")
        riga = []
        for num in range(0, 152):
            nm = LOCAL_NAMES.get(num, {}).get(lg)
            riga.append(f'"{nm}"' if nm else 'nullptr')
            if len(riga) == 4:
                out.append("  " + ", ".join(riga) + ",\n")
                riga = []
        if riga:
            out.append("  " + ", ".join(riga) + ",\n")
        out.append("};\n\n")
    out.append(
        "// Nome della specie nella lingua attiva (ricade su quello di DEX_TBL se quella\n"
        "// lingua non ha un nome proprio per essa).\n"
        "static inline const char *dexName(int16_t dex) {\n"
        "  if (dex < 1 || dex > DEX_COUNT) return DEX_TBL[0].name;\n"
        "  const char *n = (gLang == LANG_FR)   ? DEX_NAME_FR[dex]\n"
        "                  : (gLang == LANG_DE) ? DEX_NAME_DE[dex]\n"
        "                  : (gLang == LANG_JA) ? DEX_NAME_JA[dex]\n"
        "                  : (gLang == LANG_KO) ? DEX_NAME_KO[dex]\n"
        "                                       : nullptr;\n"
        "  return n ? n : DEX_TBL[dex].name;\n"
        "}\n\n")

    out.append("// il primo uovo della partita: iniziali classici\n")
    out.append("static const int16_t CLASSIC_DEX[] = { %s };\n" % ", ".join(map(str, CLASSIC)))
    out.append(f"#define NUM_CLASSIC_DEX {len(CLASSIC)}\n")
    from collections import Counter
    c = Counter(rarities)
    print(f"bases: {c['R_COMUN']} comunes, {c['R_RARO']} raras, {c['R_LEGENDARIO']} legendarias, {c['R_EVO']} solo-evolucion")

    print(f"guardado {scrivi('dex.h', out)} ({len(DEX)} especies)")
    print(f"guardado {scrivi('moves.h', genera_mosse())} ({len(MOSSE)} mosse)")


def genera_mosse():
    out = []
    out.append("#pragma once\n#include <stdint.h>\n#include \"dex.h\"  // DEX_COUNT, tipi T_*, gLang\n\n")
    out.append("// GENERATO da tools/gen_dex.py da tools/dex_moves.py, dex_types.py e\n"
               "// dex_move_names.py - non modificare\n\n")
    out.append("// Mosse e learnset di Rosso/Blu (pret/pokered), nomi da PokeAPI. Il significato\n"
               "// di p1/p2 per ogni effetto è il contratto con il motore di battaglia.\n"
               "// Modello degli effetti derivato da DylanPDao/TamaPoke (MIT).\n\n")

    # tipi: i fisici prima degli speciali, così la categoria è un confronto
    speciali = [i for i, t in enumerate(TIPI) if CATEGORIA[t] == 'speciale']
    assert speciali == list(range(speciali[0], len(TIPI))), 'i tipi speciali vanno in fondo a TIPI'
    out.append("// categoria per tipo, come in Gen 1: fisica sotto %s, speciale da lì in poi\n"
               % nome_c('T_', TIPI[speciali[0]]))
    out.append("#define TYPE_IS_SPECIAL(t) ((t) >= %s)\n\n" % nome_c('T_', TIPI[speciali[0]]))
    out.append("// efficacia x2, così 1/2 è un intero: 0 immune, 1 = 1/2, 2 = 1x, 4 = 2x\n"
               "// TYPE_CHART[attaccante][difensore]; con due tipi si moltiplica e si divide per 4\n")
    out.append("static const uint8_t TYPE_CHART[TYPE_COUNT][TYPE_COUNT] = {\n")
    for t, riga in zip(TIPI, TABELLA_TIPI):
        valori = [int(v * 2) for v in riga]
        assert all(v / 2 == x for v, x in zip(valori, riga))
        out.append("  { " + ", ".join(map(str, valori)) + f" }},  // {t}\n")
    out.append("};\n\n")

    def enum(prefisso, nomi, commenti):
        out.append("enum : uint8_t {\n")
        for i, (n, c) in enumerate(zip(nomi, commenti)):
            out.append(f"  {nome_c(prefisso, n)} = {i},{'  // ' + c if c else ''}\n")
        out.append("};\n\n")

    out.append("// effetti (EFFETTI di tools/dex_moves.py): valore = indice, i nuovi vanno in fondo\n")
    enum('EF_', [n for n, _ in EFFETTI], [c for _, c in EFFETTI])
    out.append("// stati (p1 di EF_STATO / EF_STATO_SECONDARIO)\n")
    enum('ST_', STATI, [''] * len(STATI))
    out.append("// statistiche (p1 di EF_STADIO / EF_STADIO_SECONDARIO)\n")
    enum('STAT_', STATISTICHE, [''] * len(STATISTICHE))

    per_id = {v[0]: slug for slug, v in MOSSE.items()}
    max_id = max(per_id)
    out.append("// id Gen 1 delle mosse: è il valore da persistere; gli id mancanti sono le mosse\n"
               "// escluse (ESCLUSE di tools/dex_moves.py), con pp = 0 in MOVE_TBL\n")
    out.append("enum : uint8_t {\n")
    for i in sorted(per_id):
        out.append(f"  {nome_c('MV_', per_id[i])} = {i},\n")
    out.append("};\n")
    out.append(f"#define MOVE_MAX_ID {max_id}\n")
    out.append(f"#define MOVE_STRUGGLE {nome_c('MV_', RIPIEGO)}  // ripiego di chi non ha mosse (Abra)\n\n")

    efx = [n for n, _ in EFFETTI]
    out.append(
        "struct MoveEntry {\n"
        "  uint8_t type;      // T_*\n"
        "  uint8_t power;     // 0 = niente formula del danno\n"
        "  uint8_t accuracy;  // %\n"
        "  uint8_t pp;        // 0 = id senza mossa\n"
        "  uint8_t effect;    // EF_*\n"
        "  uint8_t p1;        // vedi EF_*\n"
        "  int8_t p2;         // vedi EF_*; gli stadi sono con segno\n"
        "};\n\n")
    out.append("static const MoveEntry MOVE_TBL[MOVE_MAX_ID + 1] = {\n")
    for i in range(max_id + 1):
        if i not in per_id:
            out.append(f"  {{ T_NORMALE, 0, 0, 0, EF_NESSUNO, 0, 0 }},  // {i}: esclusa o inutilizzato\n")
            continue
        slug = per_id[i]
        _, tipo, pot, prec, pp, eff, p1, p2 = MOSSE[slug]
        assert 0 <= p1 <= 255 and -128 <= p2 <= 127, slug
        assert eff in efx, slug
        out.append(f"  {{ {nome_c('T_', tipo)}, {pot}, {prec}, {pp}, {nome_c('EF_', eff)}, {p1}, {p2} }},"
                   f"  // {i} {slug}\n")
    out.append("};\n\n")

    # learnset in layout CSR: le mosse della specie d sono LEARN_TBL[LEARN_OFS[d]..LEARN_OFS[d+1])
    out.append(
        "// Learnset di Rosso/Blu senza MT, in layout CSR: le mosse della specie d sono\n"
        "// LEARN_TBL[LEARN_OFS[d]] .. LEARN_TBL[LEARN_OFS[d + 1] - 1], per livello crescente;\n"
        "// il livello 1 sono le mosse iniziali. Nessuna voce (Abra) = MOVE_STRUGGLE.\n"
        "struct LearnEntry {\n  uint8_t level;\n  uint8_t move;  // MV_*\n};\n\n")
    ofs, voci = [0, 0], []
    for num in range(1, 152):
        for lv, slug in APPRENDIMENTO[num]:
            voci.append((num, lv, slug))
        ofs.append(len(voci))
    out.append(f"#define LEARN_COUNT {len(voci)}\n")
    out.append("static const LearnEntry LEARN_TBL[LEARN_COUNT] = {\n")
    prec_num = None
    for num, lv, slug in voci:
        if num != prec_num:
            out.append(f"  // {num} {SLUGS[num]}\n")
            prec_num = num
        out.append(f"  {{ {lv}, {nome_c('MV_', slug)} }},\n")
    out.append("};\n\n")
    out.append("static const uint16_t LEARN_OFS[DEX_COUNT + 2] = {\n")
    for i in range(0, len(ofs), 8):
        out.append("  " + ", ".join(map(str, ofs[i:i + 8])) + ",\n")
    out.append("};\n\n")

    # nomi: l'inglese per tutti, le altre lingue solo dove differiscono (nullptr = inglese)
    out.append(
        "// Nomi delle mosse per lingua (tools/dex_move_names.py): latino in maiuscolo ASCII,\n"
        "// JA e KO in UTF-8. nullptr = come l'inglese (sempre per il PT).\n")
    lingue = ('en', 'es', 'fr', 'de', 'it', 'ja', 'ko')
    for lg in lingue:
        out.append(f"static const char *const MOVE_NAME_{lg.upper()}[MOVE_MAX_ID + 1] = {{\n")
        riga = []
        for i in range(max_id + 1):
            nomi = MOVE_NAMES.get(per_id.get(i), {})
            nm = nomi.get(lg)
            if lg != 'en' and nm == nomi.get('en'):
                nm = None
            assert nm is None or ('"' not in nm and '\\' not in nm), nm
            riga.append(f'"{nm}"' if nm else 'nullptr')
            if len(riga) == 4:
                out.append("  " + ", ".join(riga) + ",\n")
                riga = []
        if riga:
            out.append("  " + ", ".join(riga) + ",\n")
        out.append("};\n\n")
    out.append(
        "// Nome della mossa nella lingua attiva; \"?\" per un id senza mossa.\n"
        "static inline const char *moveName(uint8_t id) {\n"
        "  if (id > MOVE_MAX_ID || !MOVE_NAME_EN[id]) return \"?\";\n"
        "  const char *n = (gLang == LANG_ES)   ? MOVE_NAME_ES[id]\n"
        "                  : (gLang == LANG_FR) ? MOVE_NAME_FR[id]\n"
        "                  : (gLang == LANG_DE) ? MOVE_NAME_DE[id]\n"
        "                  : (gLang == LANG_IT) ? MOVE_NAME_IT[id]\n"
        "                  : (gLang == LANG_JA) ? MOVE_NAME_JA[id]\n"
        "                  : (gLang == LANG_KO) ? MOVE_NAME_KO[id]\n"
        "                                       : nullptr;\n"
        "  return n ? n : MOVE_NAME_EN[id];\n"
        "}\n")
    return out


if __name__ == '__main__':
    main()
