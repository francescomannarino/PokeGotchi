# -*- coding: utf-8 -*-
"""GENERATO da tools/fetch_gen1.py da pret/pokered - non modificare a mano.

Mosse e learnset di Pokémon Rosso/Blu.

- MOSSE: slug PokeAPI -> (num, tipo, potenza, precisione, pp, effetto, p1, p2).
  num è l'id della Gen 1 (1..165): è il valore da persistere, stabile anche se
  le esclusioni cambiano. potenza 0 = non usa la formula del danno; la categoria
  fisica/speciale viene dal tipo (CATEGORIA in dex_types.py).
- EFFETTI: nome -> significato di p1/p2. È il contratto con il motore.
- APPRENDIMENTO: num specie -> [(livello, slug)], solo per livello (niente MT),
  senza le mosse di ESCLUSE né doppioni. Il livello 1 sono le mosse iniziali.
- RIPIEGO: Scontro, usata da una specie che non ha mosse; mai nei learnset.

Il modello degli effetti è derivato da DylanPDao/TamaPoke (MIT), esteso alla
Gen 1. Copyright (c) 2026 Quique Tortosa e contributori di DylanPDao/TamaPoke,
licenza MIT (testo in LICENSE).
"""

STATI = ('nessuno', 'paralisi', 'scottatura', 'veleno', 'tossico', 'sonno', 'congelamento', 'confusione', 'tentennamento')

STATISTICHE = ('attacco', 'difesa', 'velocita', 'speciale', 'precisione', 'elusione')

# (nome, significato di p1/p2); indice = valore dell'enum
EFFETTI = (
    ('NESSUNO', 'danno semplice'),  # 0
    ('STADIO', 'mossa di stato: p1 = statistica (STATISTICHE), p2 = stadi; positivi su chi la usa, negativi sul bersaglio'),  # 1
    ('RINCULO', 'p1 = denominatore: chi la usa subisce 1/p1 del danno inflitto'),  # 2
    ('ASSORBI', 'p1 = % del danno inflitto recuperata; p2 = 1: fallisce se il bersaglio non dorme (Mangiasogni)'),  # 3
    ('DANNO_LIVELLO', 'danno pari al livello di chi la usa, ignora statistiche e tipi; p1 = 100 esatto, 150 = casuale tra 1 e 1,5 volte il livello'),  # 4
    ('DANNO_FISSO', 'p1 = danno fisso, ignora statistiche'),  # 5
    ('PRIORITA', 'p1 = priorità: agisce prima delle mosse con priorità minore'),  # 6
    ('MAI_FALLISCE', 'salta il tiro di precisione'),  # 7
    ('MULTIPLO', 'p1 = colpi (0 = da 2 a 5), potenza per colpo; p2 = % di avvelenare (Doppio Ago)'),  # 8
    ('CURA', 'mossa di stato: p1 = % dei PS massimi recuperata'),  # 9
    ('RICARICA', 'chi la usa perde il turno seguente (non se manda KO, come in Gen 1)'),  # 10
    ('CARICA', 'il primo turno carica, il secondo colpisce; p1 = 1: invulnerabile mentre carica'),  # 11
    ('STADIO_SECONDARIO', 'danno + p2 % di abbassare di 1 stadio la statistica p1 del bersaglio'),  # 12
    ('STATO', 'mossa di stato: infligge lo stato p1 (STATI) se va a segno'),  # 13
    ('STATO_SECONDARIO', 'danno + p2 % di infliggere lo stato p1 (STATI); paralisi, scottatura e congelamento non colpiscono un bersaglio del tipo della mossa, il veleno non colpisce i tipi veleno'),  # 14
    ('RIPOSO', 'mossa di stato: recupera tutti i PS e chi la usa dorme 2 turni'),  # 15
    ('PARASSISEME', 'mossa di stato: ogni turno 1/16 dei PS massimi passa dal bersaglio a chi la usa; non sul tipo erba'),  # 16
    ('TRASFORMAZIONE', 'copia specie, tipi, statistiche (non i PS) e mosse del bersaglio, con 5 PP ciascuna'),  # 17
    ('META_PS', 'danno pari a metà dei PS attuali del bersaglio, minimo 1'),  # 18
    ('KO', 'manda KO in un colpo; fallisce se chi la usa è più lento'),  # 19
    ('ESPLOSIONE', 'chi la usa va KO; la difesa del bersaglio conta la metà'),  # 20
    ('CALCIO_SALTO', 'se manca, chi la usa subisce 1 PS di danno'),  # 21
    ('INTRAPPOLA', 'colpisce per 2-5 turni e intanto il bersaglio non può agire'),  # 22
    ('FURIA', 'si ripete per 2-3 turni, poi chi la usa resta confuso'),  # 23
    ('IRA', "si ripete fino alla fine; ogni colpo subito alza di 1 stadio l'attacco"),  # 24
    ('BRUTTO_COLPO', 'danno con probabilità di brutto colpo molto più alta'),  # 25
    ('NEBBIA', 'mossa di stato: protegge chi la usa dagli abbassamenti di stadio'),  # 26
    ('CONCENTRAZIONE', 'mossa di stato: aumenta i brutti colpi (in Gen 1 per un bug li riduce: decide il motore)'),  # 27
    ('BARRIERA', 'mossa di stato: p1 = 0 raddoppia la difesa (Riflesso), 1 la speciale (Schermoluce), finché chi la usa resta in campo'),  # 28
    ('AZZERA', 'mossa di stato: azzera gli stadi di entrambi e gli stati volatili'),  # 29
    ('CONVERSIONE', 'mossa di stato: chi la usa prende i tipi del bersaglio'),  # 30
    ('NULLA', 'non succede niente (Splash): la mossa si può comunque scegliere'),  # 31
)

RIPIEGO = 'struggle'

# slug -> motivo dell'esclusione (fuori da MOSSE e dai learnset)
ESCLUSE = {
    'metronome': 'effetto troppo complesso',
    'mimic': 'effetto troppo complesso',
    'mirror-move': 'effetto troppo complesso',
    'counter': 'effetto troppo complesso',
    'bide': 'effetto troppo complesso',
    'substitute': 'effetto troppo complesso',
    'disable': 'effetto troppo complesso',
    'teleport': 'nessun effetto nelle lotte tra allenatori',
    'roar': 'nessun effetto nelle lotte tra allenatori',
    'whirlwind': 'nessun effetto nelle lotte tra allenatori',
}

# slug: (num, tipo, potenza, precisione, pp, effetto, p1, p2)
MOSSE = {
    'pound': (1, 'normale', 40, 100, 35, 'NESSUNO', 0, 0),  # POUND
    'karate-chop': (2, 'normale', 50, 100, 25, 'BRUTTO_COLPO', 0, 0),  # KARATE_CHOP
    'double-slap': (3, 'normale', 15, 85, 10, 'MULTIPLO', 0, 0),  # DOUBLESLAP
    'comet-punch': (4, 'normale', 18, 85, 15, 'MULTIPLO', 0, 0),  # COMET_PUNCH
    'mega-punch': (5, 'normale', 80, 85, 20, 'NESSUNO', 0, 0),  # MEGA_PUNCH
    'pay-day': (6, 'normale', 40, 100, 20, 'NESSUNO', 0, 0),  # PAY_DAY
    'fire-punch': (7, 'fuoco', 75, 100, 15, 'STATO_SECONDARIO', 2, 10),  # FIRE_PUNCH
    'ice-punch': (8, 'ghiaccio', 75, 100, 15, 'STATO_SECONDARIO', 6, 10),  # ICE_PUNCH
    'thunder-punch': (9, 'elettro', 75, 100, 15, 'STATO_SECONDARIO', 1, 10),  # THUNDERPUNCH
    'scratch': (10, 'normale', 40, 100, 35, 'NESSUNO', 0, 0),  # SCRATCH
    'vice-grip': (11, 'normale', 55, 100, 30, 'NESSUNO', 0, 0),  # VICEGRIP
    'guillotine': (12, 'normale', 0, 30, 5, 'KO', 0, 0),  # GUILLOTINE
    'razor-wind': (13, 'normale', 80, 75, 10, 'CARICA', 0, 0),  # RAZOR_WIND
    'swords-dance': (14, 'normale', 0, 100, 30, 'STADIO', 0, 2),  # SWORDS_DANCE
    'cut': (15, 'normale', 50, 95, 30, 'NESSUNO', 0, 0),  # CUT
    'gust': (16, 'normale', 40, 100, 35, 'NESSUNO', 0, 0),  # GUST
    'wing-attack': (17, 'volante', 35, 100, 35, 'NESSUNO', 0, 0),  # WING_ATTACK
    'fly': (19, 'volante', 70, 95, 15, 'CARICA', 1, 0),  # FLY
    'bind': (20, 'normale', 15, 75, 20, 'INTRAPPOLA', 0, 0),  # BIND
    'slam': (21, 'normale', 80, 75, 20, 'NESSUNO', 0, 0),  # SLAM
    'vine-whip': (22, 'erba', 35, 100, 10, 'NESSUNO', 0, 0),  # VINE_WHIP
    'stomp': (23, 'normale', 65, 100, 20, 'STATO_SECONDARIO', 8, 30),  # STOMP
    'double-kick': (24, 'lotta', 30, 100, 30, 'MULTIPLO', 2, 0),  # DOUBLE_KICK
    'mega-kick': (25, 'normale', 120, 75, 5, 'NESSUNO', 0, 0),  # MEGA_KICK
    'jump-kick': (26, 'lotta', 70, 95, 25, 'CALCIO_SALTO', 0, 0),  # JUMP_KICK
    'rolling-kick': (27, 'lotta', 60, 85, 15, 'STATO_SECONDARIO', 8, 30),  # ROLLING_KICK
    'sand-attack': (28, 'normale', 0, 100, 15, 'STADIO', 4, -1),  # SAND_ATTACK
    'headbutt': (29, 'normale', 70, 100, 15, 'STATO_SECONDARIO', 8, 30),  # HEADBUTT
    'horn-attack': (30, 'normale', 65, 100, 25, 'NESSUNO', 0, 0),  # HORN_ATTACK
    'fury-attack': (31, 'normale', 15, 85, 20, 'MULTIPLO', 0, 0),  # FURY_ATTACK
    'horn-drill': (32, 'normale', 0, 30, 5, 'KO', 0, 0),  # HORN_DRILL
    'tackle': (33, 'normale', 35, 95, 35, 'NESSUNO', 0, 0),  # TACKLE
    'body-slam': (34, 'normale', 85, 100, 15, 'STATO_SECONDARIO', 1, 30),  # BODY_SLAM
    'wrap': (35, 'normale', 15, 85, 20, 'INTRAPPOLA', 0, 0),  # WRAP
    'take-down': (36, 'normale', 90, 85, 20, 'RINCULO', 4, 0),  # TAKE_DOWN
    'thrash': (37, 'normale', 90, 100, 20, 'FURIA', 0, 0),  # THRASH
    'double-edge': (38, 'normale', 100, 100, 15, 'RINCULO', 4, 0),  # DOUBLE_EDGE
    'tail-whip': (39, 'normale', 0, 100, 30, 'STADIO', 1, -1),  # TAIL_WHIP
    'poison-sting': (40, 'veleno', 15, 100, 35, 'STATO_SECONDARIO', 3, 20),  # POISON_STING
    'twineedle': (41, 'coleottero', 25, 100, 20, 'MULTIPLO', 2, 20),  # TWINEEDLE
    'pin-missile': (42, 'coleottero', 14, 85, 20, 'MULTIPLO', 0, 0),  # PIN_MISSILE
    'leer': (43, 'normale', 0, 100, 30, 'STADIO', 1, -1),  # LEER
    'bite': (44, 'normale', 60, 100, 25, 'STATO_SECONDARIO', 8, 10),  # BITE
    'growl': (45, 'normale', 0, 100, 40, 'STADIO', 0, -1),  # GROWL
    'sing': (47, 'normale', 0, 55, 15, 'STATO', 5, 0),  # SING
    'supersonic': (48, 'normale', 0, 55, 20, 'STATO', 7, 0),  # SUPERSONIC
    'sonic-boom': (49, 'normale', 0, 90, 20, 'DANNO_FISSO', 20, 0),  # SONICBOOM
    'acid': (51, 'veleno', 40, 100, 30, 'STADIO_SECONDARIO', 1, 33),  # ACID
    'ember': (52, 'fuoco', 40, 100, 25, 'STATO_SECONDARIO', 2, 10),  # EMBER
    'flamethrower': (53, 'fuoco', 95, 100, 15, 'STATO_SECONDARIO', 2, 10),  # FLAMETHROWER
    'mist': (54, 'ghiaccio', 0, 100, 30, 'NEBBIA', 0, 0),  # MIST
    'water-gun': (55, 'acqua', 40, 100, 25, 'NESSUNO', 0, 0),  # WATER_GUN
    'hydro-pump': (56, 'acqua', 120, 80, 5, 'NESSUNO', 0, 0),  # HYDRO_PUMP
    'surf': (57, 'acqua', 95, 100, 15, 'NESSUNO', 0, 0),  # SURF
    'ice-beam': (58, 'ghiaccio', 95, 100, 10, 'STATO_SECONDARIO', 6, 10),  # ICE_BEAM
    'blizzard': (59, 'ghiaccio', 120, 90, 5, 'STATO_SECONDARIO', 6, 10),  # BLIZZARD
    'psybeam': (60, 'psico', 65, 100, 20, 'STATO_SECONDARIO', 7, 10),  # PSYBEAM
    'bubble-beam': (61, 'acqua', 65, 100, 20, 'STADIO_SECONDARIO', 2, 33),  # BUBBLEBEAM
    'aurora-beam': (62, 'ghiaccio', 65, 100, 20, 'STADIO_SECONDARIO', 0, 33),  # AURORA_BEAM
    'hyper-beam': (63, 'normale', 150, 90, 5, 'RICARICA', 0, 0),  # HYPER_BEAM
    'peck': (64, 'volante', 35, 100, 35, 'NESSUNO', 0, 0),  # PECK
    'drill-peck': (65, 'volante', 80, 100, 20, 'NESSUNO', 0, 0),  # DRILL_PECK
    'submission': (66, 'lotta', 80, 80, 25, 'RINCULO', 4, 0),  # SUBMISSION
    'low-kick': (67, 'lotta', 50, 90, 20, 'STATO_SECONDARIO', 8, 30),  # LOW_KICK
    'seismic-toss': (69, 'lotta', 0, 100, 20, 'DANNO_LIVELLO', 100, 0),  # SEISMIC_TOSS
    'strength': (70, 'normale', 80, 100, 15, 'NESSUNO', 0, 0),  # STRENGTH
    'absorb': (71, 'erba', 20, 100, 20, 'ASSORBI', 50, 0),  # ABSORB
    'mega-drain': (72, 'erba', 40, 100, 10, 'ASSORBI', 50, 0),  # MEGA_DRAIN
    'leech-seed': (73, 'erba', 0, 90, 10, 'PARASSISEME', 0, 0),  # LEECH_SEED
    'growth': (74, 'normale', 0, 100, 40, 'STADIO', 3, 1),  # GROWTH
    'razor-leaf': (75, 'erba', 55, 95, 25, 'BRUTTO_COLPO', 0, 0),  # RAZOR_LEAF
    'solar-beam': (76, 'erba', 120, 100, 10, 'CARICA', 0, 0),  # SOLARBEAM
    'poison-powder': (77, 'veleno', 0, 75, 35, 'STATO', 3, 0),  # POISONPOWDER
    'stun-spore': (78, 'erba', 0, 75, 30, 'STATO', 1, 0),  # STUN_SPORE
    'sleep-powder': (79, 'erba', 0, 75, 15, 'STATO', 5, 0),  # SLEEP_POWDER
    'petal-dance': (80, 'erba', 70, 100, 20, 'FURIA', 0, 0),  # PETAL_DANCE
    'string-shot': (81, 'coleottero', 0, 95, 40, 'STADIO', 2, -1),  # STRING_SHOT
    'dragon-rage': (82, 'drago', 0, 100, 10, 'DANNO_FISSO', 40, 0),  # DRAGON_RAGE
    'fire-spin': (83, 'fuoco', 15, 70, 15, 'INTRAPPOLA', 0, 0),  # FIRE_SPIN
    'thunder-shock': (84, 'elettro', 40, 100, 30, 'STATO_SECONDARIO', 1, 10),  # THUNDERSHOCK
    'thunderbolt': (85, 'elettro', 95, 100, 15, 'STATO_SECONDARIO', 1, 10),  # THUNDERBOLT
    'thunder-wave': (86, 'elettro', 0, 100, 20, 'STATO', 1, 0),  # THUNDER_WAVE
    'thunder': (87, 'elettro', 120, 70, 10, 'STATO_SECONDARIO', 1, 10),  # THUNDER
    'rock-throw': (88, 'roccia', 50, 65, 15, 'NESSUNO', 0, 0),  # ROCK_THROW
    'earthquake': (89, 'terra', 100, 100, 10, 'NESSUNO', 0, 0),  # EARTHQUAKE
    'fissure': (90, 'terra', 0, 30, 5, 'KO', 0, 0),  # FISSURE
    'dig': (91, 'terra', 100, 100, 10, 'CARICA', 1, 0),  # DIG
    'toxic': (92, 'veleno', 0, 85, 10, 'STATO', 4, 0),  # TOXIC
    'confusion': (93, 'psico', 50, 100, 25, 'STATO_SECONDARIO', 7, 10),  # CONFUSION
    'psychic': (94, 'psico', 90, 100, 10, 'STADIO_SECONDARIO', 3, 33),  # PSYCHIC_M
    'hypnosis': (95, 'psico', 0, 60, 20, 'STATO', 5, 0),  # HYPNOSIS
    'meditate': (96, 'psico', 0, 100, 40, 'STADIO', 0, 1),  # MEDITATE
    'agility': (97, 'psico', 0, 100, 30, 'STADIO', 2, 2),  # AGILITY
    'quick-attack': (98, 'normale', 40, 100, 30, 'PRIORITA', 1, 0),  # QUICK_ATTACK
    'rage': (99, 'normale', 20, 100, 20, 'IRA', 0, 0),  # RAGE
    'night-shade': (101, 'spettro', 0, 100, 15, 'DANNO_LIVELLO', 100, 0),  # NIGHT_SHADE
    'screech': (103, 'normale', 0, 85, 40, 'STADIO', 1, -2),  # SCREECH
    'double-team': (104, 'normale', 0, 100, 15, 'STADIO', 5, 1),  # DOUBLE_TEAM
    'recover': (105, 'normale', 0, 100, 20, 'CURA', 50, 0),  # RECOVER
    'harden': (106, 'normale', 0, 100, 30, 'STADIO', 1, 1),  # HARDEN
    'minimize': (107, 'normale', 0, 100, 20, 'STADIO', 5, 1),  # MINIMIZE
    'smokescreen': (108, 'normale', 0, 100, 20, 'STADIO', 4, -1),  # SMOKESCREEN
    'confuse-ray': (109, 'spettro', 0, 100, 10, 'STATO', 7, 0),  # CONFUSE_RAY
    'withdraw': (110, 'acqua', 0, 100, 40, 'STADIO', 1, 1),  # WITHDRAW
    'defense-curl': (111, 'normale', 0, 100, 40, 'STADIO', 1, 1),  # DEFENSE_CURL
    'barrier': (112, 'psico', 0, 100, 30, 'STADIO', 1, 2),  # BARRIER
    'light-screen': (113, 'psico', 0, 100, 30, 'BARRIERA', 1, 0),  # LIGHT_SCREEN
    'haze': (114, 'ghiaccio', 0, 100, 30, 'AZZERA', 0, 0),  # HAZE
    'reflect': (115, 'psico', 0, 100, 20, 'BARRIERA', 0, 0),  # REFLECT
    'focus-energy': (116, 'normale', 0, 100, 30, 'CONCENTRAZIONE', 0, 0),  # FOCUS_ENERGY
    'self-destruct': (120, 'normale', 130, 100, 5, 'ESPLOSIONE', 0, 0),  # SELFDESTRUCT
    'egg-bomb': (121, 'normale', 100, 75, 10, 'NESSUNO', 0, 0),  # EGG_BOMB
    'lick': (122, 'spettro', 20, 100, 30, 'STATO_SECONDARIO', 1, 30),  # LICK
    'smog': (123, 'veleno', 20, 70, 20, 'STATO_SECONDARIO', 3, 40),  # SMOG
    'sludge': (124, 'veleno', 65, 100, 20, 'STATO_SECONDARIO', 3, 40),  # SLUDGE
    'bone-club': (125, 'terra', 65, 85, 20, 'STATO_SECONDARIO', 8, 10),  # BONE_CLUB
    'fire-blast': (126, 'fuoco', 120, 85, 5, 'STATO_SECONDARIO', 2, 30),  # FIRE_BLAST
    'waterfall': (127, 'acqua', 80, 100, 15, 'NESSUNO', 0, 0),  # WATERFALL
    'clamp': (128, 'acqua', 35, 75, 10, 'INTRAPPOLA', 0, 0),  # CLAMP
    'swift': (129, 'normale', 60, 100, 20, 'MAI_FALLISCE', 0, 0),  # SWIFT
    'skull-bash': (130, 'normale', 100, 100, 15, 'CARICA', 0, 0),  # SKULL_BASH
    'spike-cannon': (131, 'normale', 20, 100, 15, 'MULTIPLO', 0, 0),  # SPIKE_CANNON
    'constrict': (132, 'normale', 10, 100, 35, 'STADIO_SECONDARIO', 2, 33),  # CONSTRICT
    'amnesia': (133, 'psico', 0, 100, 20, 'STADIO', 3, 2),  # AMNESIA
    'kinesis': (134, 'psico', 0, 80, 15, 'STADIO', 4, -1),  # KINESIS
    'soft-boiled': (135, 'normale', 0, 100, 10, 'CURA', 50, 0),  # SOFTBOILED
    'high-jump-kick': (136, 'lotta', 85, 90, 20, 'CALCIO_SALTO', 0, 0),  # HI_JUMP_KICK
    'glare': (137, 'normale', 0, 75, 30, 'STATO', 1, 0),  # GLARE
    'dream-eater': (138, 'psico', 100, 100, 15, 'ASSORBI', 50, 1),  # DREAM_EATER
    'poison-gas': (139, 'veleno', 0, 55, 40, 'STATO', 3, 0),  # POISON_GAS
    'barrage': (140, 'normale', 15, 85, 20, 'MULTIPLO', 0, 0),  # BARRAGE
    'leech-life': (141, 'coleottero', 20, 100, 15, 'ASSORBI', 50, 0),  # LEECH_LIFE
    'lovely-kiss': (142, 'normale', 0, 75, 10, 'STATO', 5, 0),  # LOVELY_KISS
    'sky-attack': (143, 'volante', 140, 90, 5, 'CARICA', 0, 0),  # SKY_ATTACK
    'transform': (144, 'normale', 0, 100, 10, 'TRASFORMAZIONE', 0, 0),  # TRANSFORM
    'bubble': (145, 'acqua', 20, 100, 30, 'STADIO_SECONDARIO', 2, 33),  # BUBBLE
    'dizzy-punch': (146, 'normale', 70, 100, 10, 'NESSUNO', 0, 0),  # DIZZY_PUNCH
    'spore': (147, 'erba', 0, 100, 15, 'STATO', 5, 0),  # SPORE
    'flash': (148, 'normale', 0, 70, 20, 'STADIO', 4, -1),  # FLASH
    'psywave': (149, 'psico', 0, 80, 15, 'DANNO_LIVELLO', 150, 0),  # PSYWAVE
    'splash': (150, 'normale', 0, 100, 40, 'NULLA', 0, 0),  # SPLASH
    'acid-armor': (151, 'veleno', 0, 100, 40, 'STADIO', 1, 2),  # ACID_ARMOR
    'crabhammer': (152, 'acqua', 90, 85, 10, 'BRUTTO_COLPO', 0, 0),  # CRABHAMMER
    'explosion': (153, 'normale', 170, 100, 5, 'ESPLOSIONE', 0, 0),  # EXPLOSION
    'fury-swipes': (154, 'normale', 18, 80, 15, 'MULTIPLO', 0, 0),  # FURY_SWIPES
    'bonemerang': (155, 'terra', 50, 90, 10, 'MULTIPLO', 2, 0),  # BONEMERANG
    'rest': (156, 'psico', 0, 100, 10, 'RIPOSO', 0, 0),  # REST
    'rock-slide': (157, 'roccia', 75, 90, 10, 'NESSUNO', 0, 0),  # ROCK_SLIDE
    'hyper-fang': (158, 'normale', 80, 90, 15, 'STATO_SECONDARIO', 8, 10),  # HYPER_FANG
    'sharpen': (159, 'normale', 0, 100, 30, 'STADIO', 0, 1),  # SHARPEN
    'conversion': (160, 'normale', 0, 100, 30, 'CONVERSIONE', 0, 0),  # CONVERSION
    'tri-attack': (161, 'normale', 80, 100, 10, 'NESSUNO', 0, 0),  # TRI_ATTACK
    'super-fang': (162, 'normale', 0, 90, 10, 'META_PS', 0, 0),  # SUPER_FANG
    'slash': (163, 'normale', 70, 100, 20, 'BRUTTO_COLPO', 0, 0),  # SLASH
    'struggle': (165, 'normale', 50, 100, 10, 'RINCULO', 2, 0),  # STRUGGLE
}

APPRENDIMENTO = {
    1: [(1, 'tackle'), (1, 'growl'), (7, 'leech-seed'), (13, 'vine-whip'), (20, 'poison-powder'), (27, 'razor-leaf'), (34, 'growth'), (41, 'sleep-powder'), (48, 'solar-beam')],
    2: [(1, 'tackle'), (1, 'growl'), (1, 'leech-seed'), (13, 'vine-whip'), (22, 'poison-powder'), (30, 'razor-leaf'), (38, 'growth'), (46, 'sleep-powder'), (54, 'solar-beam')],
    3: [(1, 'tackle'), (1, 'growl'), (1, 'leech-seed'), (1, 'vine-whip'), (22, 'poison-powder'), (30, 'razor-leaf'), (43, 'growth'), (55, 'sleep-powder'), (65, 'solar-beam')],
    4: [(1, 'scratch'), (1, 'growl'), (9, 'ember'), (15, 'leer'), (22, 'rage'), (30, 'slash'), (38, 'flamethrower'), (46, 'fire-spin')],
    5: [(1, 'scratch'), (1, 'growl'), (1, 'ember'), (15, 'leer'), (24, 'rage'), (33, 'slash'), (42, 'flamethrower'), (56, 'fire-spin')],
    6: [(1, 'scratch'), (1, 'growl'), (1, 'ember'), (1, 'leer'), (24, 'rage'), (36, 'slash'), (46, 'flamethrower'), (55, 'fire-spin')],
    7: [(1, 'tackle'), (1, 'tail-whip'), (8, 'bubble'), (15, 'water-gun'), (22, 'bite'), (28, 'withdraw'), (35, 'skull-bash'), (42, 'hydro-pump')],
    8: [(1, 'tackle'), (1, 'tail-whip'), (1, 'bubble'), (15, 'water-gun'), (24, 'bite'), (31, 'withdraw'), (39, 'skull-bash'), (47, 'hydro-pump')],
    9: [(1, 'tackle'), (1, 'tail-whip'), (1, 'bubble'), (1, 'water-gun'), (24, 'bite'), (31, 'withdraw'), (42, 'skull-bash'), (52, 'hydro-pump')],
    10: [(1, 'tackle'), (1, 'string-shot')],
    11: [(1, 'harden')],
    12: [(1, 'confusion'), (15, 'poison-powder'), (16, 'stun-spore'), (17, 'sleep-powder'), (21, 'supersonic'), (32, 'psybeam')],
    13: [(1, 'poison-sting'), (1, 'string-shot')],
    14: [(1, 'harden')],
    15: [(1, 'fury-attack'), (16, 'focus-energy'), (20, 'twineedle'), (25, 'rage'), (30, 'pin-missile'), (35, 'agility')],
    16: [(1, 'gust'), (5, 'sand-attack'), (12, 'quick-attack'), (28, 'wing-attack'), (36, 'agility')],
    17: [(1, 'gust'), (1, 'sand-attack'), (12, 'quick-attack'), (31, 'wing-attack'), (40, 'agility')],
    18: [(1, 'gust'), (1, 'sand-attack'), (1, 'quick-attack'), (31, 'wing-attack'), (44, 'agility')],
    19: [(1, 'tackle'), (1, 'tail-whip'), (7, 'quick-attack'), (14, 'hyper-fang'), (23, 'focus-energy'), (34, 'super-fang')],
    20: [(1, 'tackle'), (1, 'tail-whip'), (1, 'quick-attack'), (14, 'hyper-fang'), (27, 'focus-energy'), (41, 'super-fang')],
    21: [(1, 'peck'), (1, 'growl'), (9, 'leer'), (15, 'fury-attack'), (29, 'drill-peck'), (36, 'agility')],
    22: [(1, 'peck'), (1, 'growl'), (1, 'leer'), (15, 'fury-attack'), (34, 'drill-peck'), (43, 'agility')],
    23: [(1, 'wrap'), (1, 'leer'), (10, 'poison-sting'), (17, 'bite'), (24, 'glare'), (31, 'screech'), (38, 'acid')],
    24: [(1, 'wrap'), (1, 'leer'), (1, 'poison-sting'), (17, 'bite'), (27, 'glare'), (36, 'screech'), (47, 'acid')],
    25: [(1, 'thunder-shock'), (1, 'growl'), (9, 'thunder-wave'), (16, 'quick-attack'), (26, 'swift'), (33, 'agility'), (43, 'thunder')],
    26: [(1, 'thunder-shock'), (1, 'growl'), (1, 'thunder-wave')],
    27: [(1, 'scratch'), (10, 'sand-attack'), (17, 'slash'), (24, 'poison-sting'), (31, 'swift'), (38, 'fury-swipes')],
    28: [(1, 'scratch'), (1, 'sand-attack'), (17, 'slash'), (27, 'poison-sting'), (36, 'swift'), (47, 'fury-swipes')],
    29: [(1, 'growl'), (1, 'tackle'), (8, 'scratch'), (14, 'poison-sting'), (21, 'tail-whip'), (29, 'bite'), (36, 'fury-swipes'), (43, 'double-kick')],
    30: [(1, 'growl'), (1, 'tackle'), (1, 'scratch'), (14, 'poison-sting'), (23, 'tail-whip'), (32, 'bite'), (41, 'fury-swipes'), (50, 'double-kick')],
    31: [(1, 'tackle'), (1, 'scratch'), (1, 'tail-whip'), (1, 'body-slam'), (14, 'poison-sting')],
    32: [(1, 'leer'), (1, 'tackle'), (8, 'horn-attack'), (14, 'poison-sting'), (21, 'focus-energy'), (29, 'fury-attack'), (36, 'horn-drill'), (43, 'double-kick')],
    33: [(1, 'leer'), (1, 'tackle'), (1, 'horn-attack'), (14, 'poison-sting'), (23, 'focus-energy'), (32, 'fury-attack'), (41, 'horn-drill'), (50, 'double-kick')],
    34: [(1, 'tackle'), (1, 'horn-attack'), (1, 'poison-sting'), (1, 'thrash')],
    35: [(1, 'pound'), (1, 'growl'), (13, 'sing'), (18, 'double-slap'), (24, 'minimize'), (39, 'defense-curl'), (48, 'light-screen')],
    36: [(1, 'sing'), (1, 'double-slap'), (1, 'minimize')],
    37: [(1, 'ember'), (1, 'tail-whip'), (16, 'quick-attack'), (28, 'confuse-ray'), (35, 'flamethrower'), (42, 'fire-spin')],
    38: [(1, 'ember'), (1, 'tail-whip'), (1, 'quick-attack')],
    39: [(1, 'sing'), (9, 'pound'), (19, 'defense-curl'), (24, 'double-slap'), (29, 'rest'), (34, 'body-slam'), (39, 'double-edge')],
    40: [(1, 'sing'), (1, 'defense-curl'), (1, 'double-slap')],
    41: [(1, 'leech-life'), (10, 'supersonic'), (15, 'bite'), (21, 'confuse-ray'), (28, 'wing-attack'), (36, 'haze')],
    42: [(1, 'leech-life'), (1, 'screech'), (1, 'bite'), (10, 'supersonic'), (21, 'confuse-ray'), (32, 'wing-attack'), (43, 'haze')],
    43: [(1, 'absorb'), (15, 'poison-powder'), (17, 'stun-spore'), (19, 'sleep-powder'), (24, 'acid'), (33, 'petal-dance'), (46, 'solar-beam')],
    44: [(1, 'absorb'), (1, 'poison-powder'), (1, 'stun-spore'), (19, 'sleep-powder'), (28, 'acid'), (38, 'petal-dance'), (52, 'solar-beam')],
    45: [(1, 'stun-spore'), (1, 'sleep-powder'), (1, 'acid'), (1, 'petal-dance'), (15, 'poison-powder')],
    46: [(1, 'scratch'), (13, 'stun-spore'), (20, 'leech-life'), (27, 'spore'), (34, 'slash'), (41, 'growth')],
    47: [(1, 'scratch'), (1, 'stun-spore'), (1, 'leech-life'), (30, 'spore'), (39, 'slash'), (48, 'growth')],
    48: [(1, 'tackle'), (24, 'poison-powder'), (27, 'leech-life'), (30, 'stun-spore'), (35, 'psybeam'), (38, 'sleep-powder'), (43, 'psychic')],
    49: [(1, 'tackle'), (1, 'poison-powder'), (1, 'leech-life'), (30, 'stun-spore'), (38, 'psybeam'), (43, 'sleep-powder'), (50, 'psychic')],
    50: [(1, 'scratch'), (15, 'growl'), (19, 'dig'), (24, 'sand-attack'), (31, 'slash'), (40, 'earthquake')],
    51: [(1, 'scratch'), (1, 'growl'), (1, 'dig'), (24, 'sand-attack'), (35, 'slash'), (47, 'earthquake')],
    52: [(1, 'scratch'), (1, 'growl'), (12, 'bite'), (17, 'pay-day'), (24, 'screech'), (33, 'fury-swipes'), (44, 'slash')],
    53: [(1, 'scratch'), (1, 'growl'), (1, 'bite'), (1, 'screech'), (17, 'pay-day'), (37, 'fury-swipes'), (51, 'slash')],
    54: [(1, 'scratch'), (28, 'tail-whip'), (36, 'confusion'), (43, 'fury-swipes'), (52, 'hydro-pump')],
    55: [(1, 'scratch'), (1, 'tail-whip'), (39, 'confusion'), (48, 'fury-swipes'), (59, 'hydro-pump')],
    56: [(1, 'scratch'), (1, 'leer'), (15, 'karate-chop'), (21, 'fury-swipes'), (27, 'focus-energy'), (33, 'seismic-toss'), (39, 'thrash')],
    57: [(1, 'scratch'), (1, 'leer'), (1, 'karate-chop'), (1, 'fury-swipes'), (27, 'focus-energy'), (37, 'seismic-toss'), (46, 'thrash')],
    58: [(1, 'bite'), (18, 'ember'), (23, 'leer'), (30, 'take-down'), (39, 'agility'), (50, 'flamethrower')],
    59: [(1, 'ember'), (1, 'leer'), (1, 'take-down')],
    60: [(1, 'bubble'), (16, 'hypnosis'), (19, 'water-gun'), (25, 'double-slap'), (31, 'body-slam'), (38, 'amnesia'), (45, 'hydro-pump')],
    61: [(1, 'bubble'), (1, 'hypnosis'), (1, 'water-gun'), (26, 'double-slap'), (33, 'body-slam'), (41, 'amnesia'), (49, 'hydro-pump')],
    62: [(1, 'hypnosis'), (1, 'water-gun'), (1, 'double-slap'), (1, 'body-slam')],
    # Abra conosce solo Teletrasporto, escluso: usa Scontro finché non evolve (16).
    63: [],
    64: [(1, 'confusion'), (27, 'psybeam'), (31, 'recover'), (38, 'psychic'), (42, 'reflect')],
    65: [(1, 'confusion'), (27, 'psybeam'), (31, 'recover'), (38, 'psychic'), (42, 'reflect')],
    66: [(1, 'karate-chop'), (20, 'low-kick'), (25, 'leer'), (32, 'focus-energy'), (39, 'seismic-toss'), (46, 'submission')],
    67: [(1, 'karate-chop'), (1, 'low-kick'), (1, 'leer'), (36, 'focus-energy'), (44, 'seismic-toss'), (52, 'submission')],
    68: [(1, 'karate-chop'), (1, 'low-kick'), (1, 'leer'), (36, 'focus-energy'), (44, 'seismic-toss'), (52, 'submission')],
    69: [(1, 'vine-whip'), (1, 'growth'), (13, 'wrap'), (15, 'poison-powder'), (18, 'sleep-powder'), (21, 'stun-spore'), (26, 'acid'), (33, 'razor-leaf'), (42, 'slam')],
    70: [(1, 'vine-whip'), (1, 'growth'), (1, 'wrap'), (15, 'poison-powder'), (18, 'sleep-powder'), (23, 'stun-spore'), (29, 'acid'), (38, 'razor-leaf'), (49, 'slam')],
    71: [(1, 'sleep-powder'), (1, 'stun-spore'), (1, 'acid'), (1, 'razor-leaf'), (13, 'wrap'), (15, 'poison-powder')],
    72: [(1, 'acid'), (7, 'supersonic'), (13, 'wrap'), (18, 'poison-sting'), (22, 'water-gun'), (27, 'constrict'), (33, 'barrier'), (40, 'screech'), (48, 'hydro-pump')],
    73: [(1, 'acid'), (1, 'supersonic'), (1, 'wrap'), (18, 'poison-sting'), (22, 'water-gun'), (27, 'constrict'), (35, 'barrier'), (43, 'screech'), (50, 'hydro-pump')],
    74: [(1, 'tackle'), (11, 'defense-curl'), (16, 'rock-throw'), (21, 'self-destruct'), (26, 'harden'), (31, 'earthquake'), (36, 'explosion')],
    75: [(1, 'tackle'), (1, 'defense-curl'), (16, 'rock-throw'), (21, 'self-destruct'), (29, 'harden'), (36, 'earthquake'), (43, 'explosion')],
    76: [(1, 'tackle'), (1, 'defense-curl'), (16, 'rock-throw'), (21, 'self-destruct'), (29, 'harden'), (36, 'earthquake'), (43, 'explosion')],
    77: [(1, 'ember'), (30, 'tail-whip'), (32, 'stomp'), (35, 'growl'), (39, 'fire-spin'), (43, 'take-down'), (48, 'agility')],
    78: [(1, 'ember'), (1, 'tail-whip'), (1, 'stomp'), (1, 'growl'), (39, 'fire-spin'), (47, 'take-down'), (55, 'agility')],
    79: [(1, 'confusion'), (22, 'headbutt'), (27, 'growl'), (33, 'water-gun'), (40, 'amnesia'), (48, 'psychic')],
    80: [(1, 'confusion'), (1, 'headbutt'), (27, 'growl'), (33, 'water-gun'), (37, 'withdraw'), (44, 'amnesia'), (55, 'psychic')],
    81: [(1, 'tackle'), (21, 'sonic-boom'), (25, 'thunder-shock'), (29, 'supersonic'), (35, 'thunder-wave'), (41, 'swift'), (47, 'screech')],
    82: [(1, 'tackle'), (1, 'sonic-boom'), (1, 'thunder-shock'), (29, 'supersonic'), (38, 'thunder-wave'), (46, 'swift'), (54, 'screech')],
    83: [(1, 'peck'), (1, 'sand-attack'), (7, 'leer'), (15, 'fury-attack'), (23, 'swords-dance'), (31, 'agility'), (39, 'slash')],
    84: [(1, 'peck'), (20, 'growl'), (24, 'fury-attack'), (30, 'drill-peck'), (36, 'rage'), (40, 'tri-attack'), (44, 'agility')],
    85: [(1, 'peck'), (1, 'growl'), (1, 'fury-attack'), (30, 'drill-peck'), (39, 'rage'), (45, 'tri-attack'), (51, 'agility')],
    86: [(1, 'headbutt'), (30, 'growl'), (35, 'aurora-beam'), (40, 'rest'), (45, 'take-down'), (50, 'ice-beam')],
    87: [(1, 'headbutt'), (1, 'growl'), (1, 'aurora-beam'), (44, 'rest'), (50, 'take-down'), (56, 'ice-beam')],
    88: [(1, 'pound'), (30, 'poison-gas'), (33, 'minimize'), (37, 'sludge'), (42, 'harden'), (48, 'screech'), (55, 'acid-armor')],
    89: [(1, 'pound'), (1, 'poison-gas'), (33, 'minimize'), (37, 'sludge'), (45, 'harden'), (53, 'screech'), (60, 'acid-armor')],
    90: [(1, 'tackle'), (1, 'withdraw'), (18, 'supersonic'), (23, 'clamp'), (30, 'aurora-beam'), (39, 'leer'), (50, 'ice-beam')],
    91: [(1, 'withdraw'), (1, 'supersonic'), (1, 'clamp'), (1, 'aurora-beam'), (50, 'spike-cannon')],
    92: [(1, 'lick'), (1, 'confuse-ray'), (1, 'night-shade'), (27, 'hypnosis'), (35, 'dream-eater')],
    93: [(1, 'lick'), (1, 'confuse-ray'), (1, 'night-shade'), (29, 'hypnosis'), (38, 'dream-eater')],
    94: [(1, 'lick'), (1, 'confuse-ray'), (1, 'night-shade'), (29, 'hypnosis'), (38, 'dream-eater')],
    95: [(1, 'tackle'), (1, 'screech'), (15, 'bind'), (19, 'rock-throw'), (25, 'rage'), (33, 'slam'), (43, 'harden')],
    96: [(1, 'pound'), (1, 'hypnosis'), (17, 'confusion'), (24, 'headbutt'), (29, 'poison-gas'), (32, 'psychic'), (37, 'meditate')],
    97: [(1, 'pound'), (1, 'hypnosis'), (1, 'confusion'), (24, 'headbutt'), (33, 'poison-gas'), (37, 'psychic'), (43, 'meditate')],
    98: [(1, 'bubble'), (1, 'leer'), (20, 'vice-grip'), (25, 'guillotine'), (30, 'stomp'), (35, 'crabhammer'), (40, 'harden')],
    99: [(1, 'bubble'), (1, 'leer'), (1, 'vice-grip'), (25, 'guillotine'), (34, 'stomp'), (42, 'crabhammer'), (49, 'harden')],
    100: [(1, 'tackle'), (1, 'screech'), (17, 'sonic-boom'), (22, 'self-destruct'), (29, 'light-screen'), (36, 'swift'), (43, 'explosion')],
    101: [(1, 'tackle'), (1, 'screech'), (1, 'sonic-boom'), (22, 'self-destruct'), (29, 'light-screen'), (40, 'swift'), (50, 'explosion')],
    102: [(1, 'barrage'), (1, 'hypnosis'), (25, 'reflect'), (28, 'leech-seed'), (32, 'stun-spore'), (37, 'poison-powder'), (42, 'solar-beam'), (48, 'sleep-powder')],
    103: [(1, 'barrage'), (1, 'hypnosis'), (28, 'stomp')],
    104: [(1, 'bone-club'), (1, 'growl'), (25, 'leer'), (31, 'focus-energy'), (38, 'thrash'), (43, 'bonemerang'), (46, 'rage')],
    105: [(1, 'bone-club'), (1, 'growl'), (1, 'leer'), (1, 'focus-energy'), (41, 'thrash'), (48, 'bonemerang'), (55, 'rage')],
    106: [(1, 'double-kick'), (1, 'meditate'), (33, 'rolling-kick'), (38, 'jump-kick'), (43, 'focus-energy'), (48, 'high-jump-kick'), (53, 'mega-kick')],
    107: [(1, 'comet-punch'), (1, 'agility'), (33, 'fire-punch'), (38, 'ice-punch'), (43, 'thunder-punch'), (48, 'mega-punch')],
    108: [(1, 'wrap'), (1, 'supersonic'), (7, 'stomp'), (23, 'defense-curl'), (31, 'slam'), (39, 'screech')],
    109: [(1, 'tackle'), (1, 'smog'), (32, 'sludge'), (37, 'smokescreen'), (40, 'self-destruct'), (45, 'haze'), (48, 'explosion')],
    110: [(1, 'tackle'), (1, 'smog'), (1, 'sludge'), (39, 'smokescreen'), (43, 'self-destruct'), (49, 'haze'), (53, 'explosion')],
    111: [(1, 'horn-attack'), (30, 'stomp'), (35, 'tail-whip'), (40, 'fury-attack'), (45, 'horn-drill'), (50, 'leer'), (55, 'take-down')],
    112: [(1, 'horn-attack'), (1, 'stomp'), (1, 'tail-whip'), (1, 'fury-attack'), (48, 'horn-drill'), (55, 'leer'), (64, 'take-down')],
    113: [(1, 'pound'), (1, 'double-slap'), (24, 'sing'), (30, 'growl'), (38, 'minimize'), (44, 'defense-curl'), (48, 'light-screen'), (54, 'double-edge')],
    114: [(1, 'constrict'), (1, 'bind'), (29, 'absorb'), (32, 'poison-powder'), (36, 'stun-spore'), (39, 'sleep-powder'), (45, 'slam'), (49, 'growth')],
    115: [(1, 'comet-punch'), (1, 'rage'), (26, 'bite'), (31, 'tail-whip'), (36, 'mega-punch'), (41, 'leer'), (46, 'dizzy-punch')],
    116: [(1, 'bubble'), (19, 'smokescreen'), (24, 'leer'), (30, 'water-gun'), (37, 'agility'), (45, 'hydro-pump')],
    117: [(1, 'bubble'), (1, 'smokescreen'), (24, 'leer'), (30, 'water-gun'), (41, 'agility'), (52, 'hydro-pump')],
    118: [(1, 'peck'), (1, 'tail-whip'), (19, 'supersonic'), (24, 'horn-attack'), (30, 'fury-attack'), (37, 'waterfall'), (45, 'horn-drill'), (54, 'agility')],
    119: [(1, 'peck'), (1, 'tail-whip'), (1, 'supersonic'), (24, 'horn-attack'), (30, 'fury-attack'), (39, 'waterfall'), (48, 'horn-drill'), (54, 'agility')],
    120: [(1, 'tackle'), (17, 'water-gun'), (22, 'harden'), (27, 'recover'), (32, 'swift'), (37, 'minimize'), (42, 'light-screen'), (47, 'hydro-pump')],
    121: [(1, 'tackle'), (1, 'water-gun'), (1, 'harden')],
    122: [(1, 'confusion'), (1, 'barrier'), (23, 'light-screen'), (31, 'double-slap'), (39, 'meditate')],
    123: [(1, 'quick-attack'), (17, 'leer'), (20, 'focus-energy'), (24, 'double-team'), (29, 'slash'), (35, 'swords-dance'), (42, 'agility')],
    124: [(1, 'pound'), (1, 'lovely-kiss'), (18, 'lick'), (23, 'double-slap'), (31, 'ice-punch'), (39, 'body-slam'), (47, 'thrash'), (58, 'blizzard')],
    125: [(1, 'quick-attack'), (1, 'leer'), (34, 'thunder-shock'), (37, 'screech'), (42, 'thunder-punch'), (49, 'light-screen'), (54, 'thunder')],
    126: [(1, 'ember'), (36, 'leer'), (39, 'confuse-ray'), (43, 'fire-punch'), (48, 'smokescreen'), (52, 'smog'), (55, 'flamethrower')],
    127: [(1, 'vice-grip'), (25, 'seismic-toss'), (30, 'guillotine'), (36, 'focus-energy'), (43, 'harden'), (49, 'slash'), (54, 'swords-dance')],
    128: [(1, 'tackle'), (21, 'stomp'), (28, 'tail-whip'), (35, 'leer'), (44, 'rage'), (51, 'take-down')],
    # Magikarp conosce solo Splash (NULLA) e impara Azione al 15.
    129: [(1, 'splash'), (15, 'tackle')],
    130: [(1, 'bite'), (1, 'dragon-rage'), (1, 'leer'), (1, 'hydro-pump'), (52, 'hyper-beam')],
    131: [(1, 'water-gun'), (1, 'growl'), (16, 'sing'), (20, 'mist'), (25, 'body-slam'), (31, 'confuse-ray'), (38, 'ice-beam'), (46, 'hydro-pump')],
    # Ditto ha solo Trasformazione, che il motore implementa.
    132: [(1, 'transform')],
    133: [(1, 'tackle'), (1, 'sand-attack'), (27, 'quick-attack'), (31, 'tail-whip'), (37, 'bite'), (45, 'take-down')],
    134: [(1, 'tackle'), (1, 'sand-attack'), (1, 'quick-attack'), (1, 'water-gun'), (37, 'tail-whip'), (40, 'bite'), (42, 'acid-armor'), (44, 'haze'), (48, 'mist'), (54, 'hydro-pump')],
    135: [(1, 'tackle'), (1, 'sand-attack'), (1, 'quick-attack'), (1, 'thunder-shock'), (37, 'tail-whip'), (40, 'thunder-wave'), (42, 'double-kick'), (44, 'agility'), (48, 'pin-missile'), (54, 'thunder')],
    136: [(1, 'tackle'), (1, 'sand-attack'), (1, 'quick-attack'), (1, 'ember'), (37, 'tail-whip'), (40, 'bite'), (42, 'leer'), (44, 'fire-spin'), (48, 'rage'), (54, 'flamethrower')],
    137: [(1, 'tackle'), (1, 'sharpen'), (1, 'conversion'), (23, 'psybeam'), (28, 'recover'), (35, 'agility'), (42, 'tri-attack')],
    138: [(1, 'water-gun'), (1, 'withdraw'), (34, 'horn-attack'), (39, 'leer'), (46, 'spike-cannon'), (53, 'hydro-pump')],
    139: [(1, 'water-gun'), (1, 'withdraw'), (1, 'horn-attack'), (39, 'leer'), (44, 'spike-cannon'), (49, 'hydro-pump')],
    140: [(1, 'scratch'), (1, 'harden'), (34, 'absorb'), (39, 'slash'), (44, 'leer'), (49, 'hydro-pump')],
    141: [(1, 'scratch'), (1, 'harden'), (1, 'absorb'), (39, 'slash'), (46, 'leer'), (53, 'hydro-pump')],
    142: [(1, 'wing-attack'), (1, 'agility'), (33, 'supersonic'), (38, 'bite'), (45, 'take-down'), (54, 'hyper-beam')],
    143: [(1, 'headbutt'), (1, 'amnesia'), (1, 'rest'), (35, 'body-slam'), (41, 'harden'), (48, 'double-edge'), (56, 'hyper-beam')],
    144: [(1, 'peck'), (1, 'ice-beam'), (51, 'blizzard'), (55, 'agility'), (60, 'mist')],
    145: [(1, 'thunder-shock'), (1, 'drill-peck'), (51, 'thunder'), (55, 'agility'), (60, 'light-screen')],
    146: [(1, 'peck'), (1, 'fire-spin'), (51, 'leer'), (55, 'agility'), (60, 'sky-attack')],
    147: [(1, 'wrap'), (1, 'leer'), (10, 'thunder-wave'), (20, 'agility'), (30, 'slam'), (40, 'dragon-rage'), (50, 'hyper-beam')],
    148: [(1, 'wrap'), (1, 'leer'), (1, 'thunder-wave'), (20, 'agility'), (35, 'slam'), (45, 'dragon-rage'), (55, 'hyper-beam')],
    149: [(1, 'wrap'), (1, 'leer'), (1, 'thunder-wave'), (1, 'agility'), (35, 'slam'), (45, 'dragon-rage'), (60, 'hyper-beam')],
    150: [(1, 'confusion'), (1, 'swift'), (1, 'psychic'), (63, 'barrier'), (70, 'recover'), (75, 'mist'), (81, 'amnesia')],
    151: [(1, 'pound'), (10, 'transform'), (20, 'mega-punch'), (40, 'psychic')],
}
