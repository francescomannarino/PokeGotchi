# -*- coding: utf-8 -*-
"""GENERATO da tools/fetch_gen1.py da pret/pokered - non modificare a mano.

Tipi e tabella dei tipi di Pokémon Rosso/Blu.

- TIPI: l'ordine è fisso e fa da indice di TYPE_CHART; le chiavi sono quelle
  di TYPE_ACCENTS in dex_data.py.
- CATEGORIA: in Gen 1 fisico/speciale dipende dal tipo della mossa.
- TIPI_GEN1: num -> (tipo1, tipo2|None). Non sostituisce il campo tipo di DEX,
  che guida colori e biomi.
- TABELLA_TIPI[attaccante][difensore], indici di TIPI, valori in {0, 0.5, 1, 2}.
  Fedele a Rosso/Blu: Spettro->Psico 0x (bug storico), Veleno<->Coleottero 2x,
  Ghiaccio->Fuoco 1x.
"""

TIPI = (
    'normale',     # 0
    'lotta',       # 1
    'volante',     # 2
    'veleno',      # 3
    'terra',       # 4
    'roccia',      # 5
    'coleottero',  # 6
    'spettro',     # 7
    'fuoco',       # 8
    'acqua',       # 9
    'erba',        # 10
    'elettro',     # 11
    'psico',       # 12
    'ghiaccio',    # 13
    'drago',       # 14
)

CATEGORIA = {
    'normale':    'fisica',
    'lotta':      'fisica',
    'volante':    'fisica',
    'veleno':     'fisica',
    'terra':      'fisica',
    'roccia':     'fisica',
    'coleottero': 'fisica',
    'spettro':    'fisica',
    'fuoco':      'speciale',
    'acqua':      'speciale',
    'erba':       'speciale',
    'elettro':    'speciale',
    'psico':      'speciale',
    'ghiaccio':   'speciale',
    'drago':      'speciale',
}

TIPI_GEN1 = {
    1: ('erba', 'veleno'),
    2: ('erba', 'veleno'),
    3: ('erba', 'veleno'),
    4: ('fuoco', None),
    5: ('fuoco', None),
    6: ('fuoco', 'volante'),
    7: ('acqua', None),
    8: ('acqua', None),
    9: ('acqua', None),
    10: ('coleottero', None),
    11: ('coleottero', None),
    12: ('coleottero', 'volante'),
    13: ('coleottero', 'veleno'),
    14: ('coleottero', 'veleno'),
    15: ('coleottero', 'veleno'),
    16: ('normale', 'volante'),
    17: ('normale', 'volante'),
    18: ('normale', 'volante'),
    19: ('normale', None),
    20: ('normale', None),
    21: ('normale', 'volante'),
    22: ('normale', 'volante'),
    23: ('veleno', None),
    24: ('veleno', None),
    25: ('elettro', None),
    26: ('elettro', None),
    27: ('terra', None),
    28: ('terra', None),
    29: ('veleno', None),
    30: ('veleno', None),
    31: ('veleno', 'terra'),
    32: ('veleno', None),
    33: ('veleno', None),
    34: ('veleno', 'terra'),
    35: ('normale', None),
    36: ('normale', None),
    37: ('fuoco', None),
    38: ('fuoco', None),
    39: ('normale', None),
    40: ('normale', None),
    41: ('veleno', 'volante'),
    42: ('veleno', 'volante'),
    43: ('erba', 'veleno'),
    44: ('erba', 'veleno'),
    45: ('erba', 'veleno'),
    46: ('coleottero', 'erba'),
    47: ('coleottero', 'erba'),
    48: ('coleottero', 'veleno'),
    49: ('coleottero', 'veleno'),
    50: ('terra', None),
    51: ('terra', None),
    52: ('normale', None),
    53: ('normale', None),
    54: ('acqua', None),
    55: ('acqua', None),
    56: ('lotta', None),
    57: ('lotta', None),
    58: ('fuoco', None),
    59: ('fuoco', None),
    60: ('acqua', None),
    61: ('acqua', None),
    62: ('acqua', 'lotta'),
    63: ('psico', None),
    64: ('psico', None),
    65: ('psico', None),
    66: ('lotta', None),
    67: ('lotta', None),
    68: ('lotta', None),
    69: ('erba', 'veleno'),
    70: ('erba', 'veleno'),
    71: ('erba', 'veleno'),
    72: ('acqua', 'veleno'),
    73: ('acqua', 'veleno'),
    74: ('roccia', 'terra'),
    75: ('roccia', 'terra'),
    76: ('roccia', 'terra'),
    77: ('fuoco', None),
    78: ('fuoco', None),
    79: ('acqua', 'psico'),
    80: ('acqua', 'psico'),
    81: ('elettro', None),
    82: ('elettro', None),
    83: ('normale', 'volante'),
    84: ('normale', 'volante'),
    85: ('normale', 'volante'),
    86: ('acqua', None),
    87: ('acqua', 'ghiaccio'),
    88: ('veleno', None),
    89: ('veleno', None),
    90: ('acqua', None),
    91: ('acqua', 'ghiaccio'),
    92: ('spettro', 'veleno'),
    93: ('spettro', 'veleno'),
    94: ('spettro', 'veleno'),
    95: ('roccia', 'terra'),
    96: ('psico', None),
    97: ('psico', None),
    98: ('acqua', None),
    99: ('acqua', None),
    100: ('elettro', None),
    101: ('elettro', None),
    102: ('erba', 'psico'),
    103: ('erba', 'psico'),
    104: ('terra', None),
    105: ('terra', None),
    106: ('lotta', None),
    107: ('lotta', None),
    108: ('normale', None),
    109: ('veleno', None),
    110: ('veleno', None),
    111: ('terra', 'roccia'),
    112: ('terra', 'roccia'),
    113: ('normale', None),
    114: ('erba', None),
    115: ('normale', None),
    116: ('acqua', None),
    117: ('acqua', None),
    118: ('acqua', None),
    119: ('acqua', None),
    120: ('acqua', None),
    121: ('acqua', 'psico'),
    122: ('psico', None),
    123: ('coleottero', 'volante'),
    124: ('ghiaccio', 'psico'),
    125: ('elettro', None),
    126: ('fuoco', None),
    127: ('coleottero', None),
    128: ('normale', None),
    129: ('acqua', None),
    130: ('acqua', 'volante'),
    131: ('acqua', 'ghiaccio'),
    132: ('normale', None),
    133: ('normale', None),
    134: ('acqua', None),
    135: ('elettro', None),
    136: ('fuoco', None),
    137: ('normale', None),
    138: ('roccia', 'acqua'),
    139: ('roccia', 'acqua'),
    140: ('roccia', 'acqua'),
    141: ('roccia', 'acqua'),
    142: ('roccia', 'volante'),
    143: ('normale', None),
    144: ('ghiaccio', 'volante'),
    145: ('elettro', 'volante'),
    146: ('fuoco', 'volante'),
    147: ('drago', None),
    148: ('drago', None),
    149: ('drago', 'volante'),
    150: ('psico', None),
    151: ('psico', None),
}

# righe: attaccante; colonne: difensore (nor lot vol vel ter roc col spe fuo acq erb ele psi ghi dra)
TABELLA_TIPI = (
    (  1,   1,   1,   1,   1, 0.5,   1,   0,   1,   1,   1,   1,   1,   1,   1),  # normale
    (  2,   1, 0.5, 0.5,   1,   2, 0.5,   0,   1,   1,   1,   1, 0.5,   2,   1),  # lotta
    (  1,   2,   1,   1,   1, 0.5,   2,   1,   1,   1,   2, 0.5,   1,   1,   1),  # volante
    (  1,   1,   1, 0.5, 0.5, 0.5,   2, 0.5,   1,   1,   2,   1,   1,   1,   1),  # veleno
    (  1,   1,   0,   2,   1,   2, 0.5,   1,   2,   1, 0.5,   2,   1,   1,   1),  # terra
    (  1, 0.5,   2,   1, 0.5,   1,   2,   1,   2,   1,   1,   1,   1,   2,   1),  # roccia
    (  1, 0.5, 0.5,   2,   1,   1,   1, 0.5, 0.5,   1,   2,   1,   2,   1,   1),  # coleottero
    (  0,   1,   1,   1,   1,   1,   1,   2,   1,   1,   1,   1,   0,   1,   1),  # spettro
    (  1,   1,   1,   1,   1, 0.5,   2,   1, 0.5, 0.5,   2,   1,   1,   2, 0.5),  # fuoco
    (  1,   1,   1,   1,   2,   2,   1,   1,   2, 0.5, 0.5,   1,   1,   1, 0.5),  # acqua
    (  1,   1, 0.5, 0.5,   2,   2, 0.5,   1, 0.5,   2, 0.5,   1,   1,   1, 0.5),  # erba
    (  1,   1,   2,   1,   0,   1,   1,   1,   1,   2, 0.5, 0.5,   1,   1, 0.5),  # elettro
    (  1,   2,   1,   2,   1,   1,   1,   1,   1,   1,   1,   1, 0.5,   1,   1),  # psico
    (  1,   1,   2,   1,   2,   1,   1,   1,   1, 0.5,   2,   1,   1, 0.5,   2),  # ghiaccio
    (  1,   1,   1,   1,   1,   1,   1,   1,   1,   1,   1,   1,   1,   1,   2),  # drago
)
