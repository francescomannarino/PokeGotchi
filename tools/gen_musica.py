#!/usr/bin/env python3
"""Genera musica_dati.h (le musiche di battaglia di Rosso/Blu e dei minigiochi per l'interprete
di musica.h) dai sorgenti di pret/pokered e pret/pokeyellow copiati in tools/musica/.

  python3 tools/gen_musica.py          # -> musica_dati.h
  python3 tools/gen_musica.py durate   # durata di intro e giro di ogni canale, in fotogrammi

Il bytecode è quello del motore di pokered (audio/engine_2.asm), byte per byte: l'interprete
di musica.cpp lo decodifica come l'originale. Un comando che l'interprete non conosce, un'etichetta
mancante o un canale senza fine né ciclo fanno uscire con un errore (codice 1).
"""
import os
import re
import sys

QUI = os.path.dirname(os.path.abspath(__file__))
RADICE = os.path.join(QUI, 'musica')  # una cartella per disassemblaggio: pokered/, pokeyellow/
# note, forme d'onda e strumenti di rumore vengono da qui (in pokeyellow sono uguali)
COMUNE = 'pokered'

# frequenza dei fotogrammi del Game Boy: 4194304 / 70224 = 59,7275 Hz
FPS_NUM, FPS_DEN = 4194304, 70224

# (enum, cartella in tools/musica/, file in audio/music/, etichetta in
# audio/headers/musicheaders*.asm, gira in loop). I valori dell'enum seguono quest'ordine:
# i brani nuovi vanno in fondo.
# Le vittorie in pokered ripetono il loro .mainloop all'infinito; qui suonano una volta:
# il generatore cambia il loro sound_loop 0 in un sound_ret, cioè la fine del canale.
BRANI = [
    ('MUS_WILDBATTLE', 'pokered', 'wildbattle', 'Music_WildBattle', True),
    ('MUS_TRAINERBATTLE', 'pokered', 'trainerbattle', 'Music_TrainerBattle', True),
    ('MUS_GYMLEADERBATTLE', 'pokered', 'gymleaderbattle', 'Music_GymLeaderBattle', True),
    ('MUS_FINALBATTLE', 'pokered', 'finalbattle', 'Music_FinalBattle', True),
    ('MUS_DEFEATEDWILDMON', 'pokered', 'defeatedwildmon', 'Music_DefeatedWildMon', False),
    ('MUS_DEFEATEDTRAINER', 'pokered', 'defeatedtrainer', 'Music_DefeatedTrainer', False),
    ('MUS_DEFEATEDGYMLEADER', 'pokered', 'defeatedgymleader', 'Music_DefeatedGymLeader', False),
    # minigiochi a movimento: il Surf di Giallo e la Grotta Diglett (con la batteria)
    ('MUS_SURFINGPIKACHU', 'pokeyellow', 'surfingpikachu', 'Music_SurfingPikachu', True),
    ('MUS_DUNGEON2', 'pokered', 'dungeon2', 'Music_Dungeon2', True),
]

# strumenti di rumore della batteria (drum_note 1-19): gli effetti
# audio/sfx/noise_instrumentNN_3.asm, quelli del banco di Dungeon2 (i tre banchi sono uguali)
N_RUMORI = 19

NOTE = ['C_', 'C#', 'D_', 'D#', 'E_', 'F_', 'F#', 'G_', 'G#', 'A_', 'A#', 'B_']

# byte dei comandi (macros/scripts/audio.asm); note = $00-$BF, rest = $C0-$CF
C_DRUM_NOTE = 0xB0  # solo sul canale 4, dove $00-$BF non sono note
C_REST = 0xC0
C_NOTE_TYPE = 0xD0
C_OCTAVE = 0xE0
C_TOGGLE_PERFECT_PITCH = 0xE8
C_VIBRATO = 0xEA
C_DUTY_CYCLE = 0xEC
C_TEMPO = 0xED
C_VOLUME = 0xF0
C_EXECUTE_MUSIC = 0xF8
C_SOUND_CALL = 0xFD
C_SOUND_LOOP = 0xFE
C_SOUND_RET = 0xFF

# comando -> (numero di argomenti, byte occupati)
COMANDI = {
    'note': (2, 1), 'rest': (1, 1), 'note_type': (3, 2), 'octave': (1, 1),
    'toggle_perfect_pitch': (0, 1), 'vibrato': (3, 3), 'duty_cycle': (1, 2),
    'tempo': (1, 3), 'volume': (2, 2), 'execute_music': (0, 1),
    'sound_call': (1, 3), 'sound_loop': (2, 4), 'sound_ret': (0, 1),
    'drum_speed': (1, 1), 'drum_note': (2, 2),
}
# comandi ammessi solo sul canale 4 (rumore), e quelli che lì non hanno senso
SOLO_RUMORE = {'drum_speed', 'drum_note'}
NON_RUMORE = {'note', 'note_type', 'vibrato', 'duty_cycle', 'toggle_perfect_pitch'}


# byte occupati da un comando, dal suo primo byte (serve a percorrere il bytecode); sul
# canale 4 `rumore` è vero: drum_speed ($Dx) non ha parametri e drum_note ($Bx) ne ha uno
def lunghezza_comando(b, rumore=False):
    if rumore and b & 0xF0 == C_NOTE_TYPE:
        return 1
    if rumore and b & 0xF0 == C_DRUM_NOTE:
        return 2
    if b < C_NOTE_TYPE or C_OCTAVE <= b < C_TOGGLE_PERFECT_PITCH:
        return 1  # note, rest, octave
    return {C_TOGGLE_PERFECT_PITCH: 1, C_VIBRATO: 3, C_DUTY_CYCLE: 2, C_TEMPO: 3,
            C_VOLUME: 2, C_EXECUTE_MUSIC: 1, C_SOUND_CALL: 3, C_SOUND_LOOP: 4,
            C_SOUND_RET: 1}.get(b, 2 if b & 0xF0 == C_NOTE_TYPE else 0)


class ErroreMusica(Exception):
    pass


def numero(s, dove):
    s = s.strip()
    try:
        if s.startswith('$'):
            return int(s[1:], 16)
        if s.startswith('%'):
            return int(s[1:], 2)
        return int(s)
    except ValueError:
        raise ErroreMusica(f'{dove}: numero non valido «{s}»')


def segno_modulo(v, dove):
    """fade/pitch change [-7, 7] in rappresentazione segno e modulo su 4 bit."""
    if not -7 <= v <= 7:
        raise ErroreMusica(f'{dove}: valore {v} fuori da [-7, 7]')
    return 8 | -v if v < 0 else v


def righe_pulite(path):
    with open(path, encoding='utf-8') as fh:
        for n, riga in enumerate(fh, 1):
            riga = riga.split(';', 1)[0].rstrip()
            if riga.strip():
                yield n, riga


def leggi_intestazioni(cartella):
    """audio/headers/musicheaders*.asm -> {Music_X: [(numero, etichetta) dei canali]}"""
    teste = {}
    cartella_h = os.path.join(cartella, 'audio', 'headers')
    for nome_file in sorted(os.listdir(cartella_h)):
        if not re.fullmatch(r'musicheaders\d*\.asm', nome_file):
            continue
        attuale = None
        for n, riga in righe_pulite(os.path.join(cartella_h, nome_file)):
            dove = f'{nome_file}:{n}'
            m = re.fullmatch(r'(\w+)::?', riga.strip())
            if m:
                attuale = teste.setdefault(m.group(1), [])
                continue
            parti = riga.split(None, 1)
            args = [a.strip() for a in parti[1].split(',')] if len(parti) > 1 else []
            if parti[0] == 'channel' and attuale is not None:
                attuale.append((numero(args[0], dove), args[1]))
            elif parti[0] != 'channel_count':
                raise ErroreMusica(f'{dove}: comando sconosciuto «{parti[0]}»')
    return teste


def canali_del_brano(teste, testa):
    """Le etichette dei canali 1..n di un brano; l'interprete li vuole in quest'ordine."""
    canali = teste[testa]
    if not 1 <= len(canali) <= 4 or [c for c, _ in canali] != list(range(1, len(canali) + 1)):
        raise ErroreMusica(f'{testa}: canali fuori ordine o in numero sbagliato')
    return [et for _, et in canali]


def leggi_note(cartella):
    valori = []
    for n, riga in righe_pulite(os.path.join(cartella, 'audio', 'notes.asm')):
        parti = riga.split()
        if parti[0] == 'dw':
            valori.append(numero(parti[1], f'notes.asm:{n}'))
    if len(valori) != len(NOTE):
        raise ErroreMusica(f'notes.asm: {len(valori)} note invece di {len(NOTE)}')
    return valori


def leggi_onde(cartella):
    """wave_samples.asm -> [[32 campioni a 4 bit], ...] per .wave0, .wave1, ..."""
    onde, attuale = {}, None
    for n, riga in righe_pulite(os.path.join(cartella, 'audio', 'wave_samples.asm')):
        r = riga.strip()
        m = re.fullmatch(r'\.wave(\d+):?', r)
        if m:
            attuale = int(m.group(1))
        elif r.startswith('dn ') and attuale is not None:
            campioni = [numero(x, f'wave_samples.asm:{n}') for x in r[3:].split(',')]
            if len(campioni) != 32 or not all(0 <= c <= 15 for c in campioni):
                raise ErroreMusica(f'wave_samples.asm:{n}: servono 32 campioni da 0 a 15')
            onde[attuale] = campioni
    if not onde or sorted(onde) != list(range(len(onde))):
        raise ErroreMusica('wave_samples.asm: forme d\'onda mancanti o non consecutive')
    return [onde[i] for i in range(len(onde))]


def codifica(cmd, args, ripeti, dove, rumore=False):
    """Un comando -> (byte senza le etichette, etichetta da risolvere o None). `rumore`: il
    comando sta nel canale 4."""
    if cmd not in COMANDI:
        raise ErroreMusica(f'{dove}: comando sconosciuto «{cmd}»')
    if len(args) != COMANDI[cmd][0]:
        raise ErroreMusica(f'{dove}: {cmd} vuole {COMANDI[cmd][0]} argomenti')
    if cmd in SOLO_RUMORE and not rumore:
        raise ErroreMusica(f'{dove}: {cmd} fuori dal canale 4')
    if cmd in NON_RUMORE and rumore:
        raise ErroreMusica(f'{dove}: {cmd} nel canale 4 (rumore)')

    def intervallo(v, lo, hi, cosa):
        v = numero(v, dove)
        if not lo <= v <= hi:
            raise ErroreMusica(f'{dove}: {cosa} {v} fuori da [{lo}, {hi}]')
        return v

    if cmd == 'note':
        if args[0] not in NOTE:
            raise ErroreMusica(f'{dove}: nota sconosciuta «{args[0]}»')
        return [NOTE.index(args[0]) << 4 | intervallo(args[1], 1, 16, 'lunghezza') - 1], None
    if cmd == 'rest':
        return [C_REST | intervallo(args[0], 1, 16, 'lunghezza') - 1], None
    if cmd == 'drum_speed':
        return [C_NOTE_TYPE | intervallo(args[0], 0, 15, 'velocità')], None
    if cmd == 'drum_note':
        strumento = intervallo(args[0], 1, N_RUMORI, 'strumento')
        return [C_DRUM_NOTE | intervallo(args[1], 1, 16, 'lunghezza') - 1, strumento], None
    if cmd == 'note_type':
        vel = intervallo(args[0], 0, 15, 'velocità')
        vol = intervallo(args[1], 0, 15, 'volume')
        # sul canale 3 il terzo argomento è la forma d'onda: lo controlla controlla_onde
        return [C_NOTE_TYPE | vel, vol << 4 | segno_modulo(numero(args[2], dove), dove)], None
    if cmd == 'octave':
        return [C_OCTAVE | 8 - intervallo(args[0], 1, 8, 'ottava')], None
    if cmd == 'toggle_perfect_pitch':
        return [C_TOGGLE_PERFECT_PITCH], None
    if cmd == 'vibrato':
        return [C_VIBRATO, intervallo(args[0], 0, 255, 'ritardo'),
                intervallo(args[1], 0, 15, 'profondità') << 4
                | intervallo(args[2], 0, 15, 'velocità')], None
    if cmd == 'duty_cycle':
        return [C_DUTY_CYCLE, intervallo(args[0], 0, 3, 'duty')], None
    if cmd == 'tempo':
        t = intervallo(args[0], 0, 0xFFFF, 'tempo')
        return [C_TEMPO, t >> 8, t & 0xFF], None
    if cmd == 'volume':
        return [C_VOLUME, intervallo(args[0], 0, 7, 'volume') << 4
                | intervallo(args[1], 0, 7, 'volume')], None
    if cmd == 'execute_music':
        return [C_EXECUTE_MUSIC], None
    if cmd == 'sound_call':
        return [C_SOUND_CALL, 0, 0], args[0]
    if cmd == 'sound_loop':
        conta = intervallo(args[0], 0, 255, 'ripetizioni')
        if conta == 0 and not ripeti:
            return [C_SOUND_RET], None  # vittoria: suona una volta, poi il canale finisce
        return [C_SOUND_LOOP, conta, 0, 0], args[1]
    return [C_SOUND_RET], None


def assembla_brano(cartella, file, testa, canali, ripeti):
    """Un file .asm -> dizionario con il bytecode del brano e i punti d'ingresso.

    Tutti i canali di un brano stanno in un solo blocco di byte, come nella ROM: in
    finalbattle il canale 1 chiama una subroutine del canale 2.
    """
    path = os.path.join(cartella, 'audio', 'music', file + '.asm')
    canale4 = canali[3] if len(canali) == 4 else None  # la sezione del rumore
    istruzioni = []   # (offset, byte, etichetta da risolvere, dove)
    etichette = {}    # nome completo -> offset
    globali = {}      # offset -> etichetta globale (inizio di una sezione)
    globale, offset = None, 0
    for n, riga in righe_pulite(path):
        dove = f'{file}.asm:{n}'
        r = riga.strip()
        m = re.fullmatch(r'(\.?\w+):{0,2}', r)
        if m and (r.endswith(':') or r.startswith('.')):
            nome = m.group(1)
            if nome.startswith('.'):
                if globale is None:
                    raise ErroreMusica(f'{dove}: etichetta locale fuori da un canale')
                nome = globale + nome
            else:
                globale = nome
                globali[offset] = nome
            if nome in etichette:
                raise ErroreMusica(f'{dove}: etichetta «{nome}» ripetuta')
            etichette[nome] = offset
            continue
        if globale is None:
            raise ErroreMusica(f'{dove}: comando prima della prima etichetta')
        parti = r.split(None, 1)
        args = [a.strip() for a in parti[1].split(',')] if len(parti) > 1 else []
        byte, rif = codifica(parti[0], args, ripeti, dove, globale == canale4)
        if rif is not None and rif.startswith('.'):
            rif = globale + rif
        istruzioni.append((offset, byte, rif, dove))
        offset += len(byte)

    dati = bytearray()
    for off, byte, rif, dove in istruzioni:
        if rif is not None:
            if rif not in etichette:
                raise ErroreMusica(f'{dove}: etichetta mancante «{rif}»')
            dest = etichette[rif]
            byte = byte[:-2] + [dest & 0xFF, dest >> 8]  # dw: little endian, come la ROM
        dati += bytes(byte)
    if len(dati) > 0xFFFF:
        raise ErroreMusica(f'{file}.asm: brano troppo lungo')

    inizio = []
    for et in canali:
        if et not in etichette:
            raise ErroreMusica(f'{file}.asm: manca il canale «{et}» di {testa}')
        inizio.append(etichette[et])
    brano = {'file': file, 'testa': testa, 'ripeti': ripeti, 'dati': bytes(dati),
             'inizio': inizio, 'etichette': etichette, 'globali': globali,
             'canali': canali, 'canale4': canale4}
    controlla_flusso(brano)
    return brano


def istruzioni_da(dati, off, rumore=False, fine=None):
    """Percorre il bytecode da off: (offset, primo byte) fino a `fine` (la fine dei dati)."""
    fine = len(dati) if fine is None else fine
    while off < fine:
        b = dati[off]
        n = lunghezza_comando(b, rumore)
        if n == 0 or off + n > fine:
            raise ErroreMusica(f'byte ${b:02X} non valido a ${off:04X}')
        yield off, b
        off += n


def sezione(brano, off):
    """L'etichetta globale (la sezione) che contiene off."""
    return brano['globali'][max(o for o in brano['globali'] if o <= off)]


def istruzioni_brano(brano):
    """Tutto il bytecode, sezione per sezione: (offset, primo byte, è il canale 4)."""
    dati = brano['dati']
    tagli = sorted(set(brano['globali']) | {len(dati)})
    for a, z in zip(tagli, tagli[1:]):
        rumore = brano['globali'][a] == brano['canale4']
        for off, b in istruzioni_da(dati, a, rumore, z):
            yield off, b, rumore


def bersaglio(dati, off):
    return dati[off + 1] | dati[off + 2] << 8 if dati[off] == C_SOUND_CALL else \
        dati[off + 2] | dati[off + 3] << 8


def controlla_flusso(brano):
    """Ogni canale finisce (sound_ret) o torna indietro all'infinito (sound_loop 0); i cicli
    finiti tornano indietro e non si annidano (il contatore è uno per canale); le
    subroutine finiscono con sound_ret e non chiamano altre subroutine."""
    dati, globali, nome = brano['dati'], brano['globali'], brano['file']
    confini = set(brano['etichette'].values())

    def fino_a_ret(dest, chi, rumore):
        """Il corpo di una subroutine; errore se esce dalla sua sezione."""
        corpo = []
        for off, b in istruzioni_da(dati, dest, rumore):
            if off != dest and off in globali:
                break
            corpo.append((off, b))
            if b == C_SOUND_RET:
                return corpo
            if b == C_SOUND_CALL:
                raise ErroreMusica(f'{nome}: {chi} chiama una subroutine dentro una subroutine')
            if b == C_SOUND_LOOP and dati[off + 1] == 0:
                raise ErroreMusica(f'{nome}: {chi} ha un ciclo infinito')
        raise ErroreMusica(f'{nome}: la subroutine di {chi} non finisce con sound_ret')

    for c, et in enumerate(brano['canali']):
        chi = f'{et} (canale {c + 1})'
        start = brano['inizio'][c]
        rumore = et == brano['canale4']
        percorso = []
        for off, b in istruzioni_da(dati, start, rumore):
            if off != start and off in globali:
                raise ErroreMusica(f'{nome}: {chi} non ha né fine né ciclo '
                                   f'(continua in {globali[off]})')
            percorso.append((off, b))
            if b in (C_SOUND_CALL, C_SOUND_LOOP):
                dest = bersaglio(dati, off)
                if dest not in confini:
                    raise ErroreMusica(f'{nome}: {chi} salta a ${dest:04X}, non è un\'etichetta')
                # il canale 4 si legge con altre regole: nessun salto tra lui e gli altri
                if (sezione(brano, dest) == brano['canale4']) != rumore:
                    raise ErroreMusica(f'{nome}: {chi} salta tra il canale 4 e gli altri')
                if b == C_SOUND_CALL:
                    fino_a_ret(dest, chi, rumore)
                elif dest > off or dest < start:
                    raise ErroreMusica(f'{nome}: {chi} ha un ciclo che non torna indietro')
                if b == C_SOUND_LOOP and dati[off + 1] == 0:
                    break
            if b == C_SOUND_RET:
                break
        else:
            raise ErroreMusica(f'{nome}: {chi} non ha né fine né ciclo')
        # cicli finiti: nessun altro ciclo finito nel corpo, né nelle subroutine che chiama
        for off, b in percorso:
            if b != C_SOUND_LOOP or dati[off + 1] == 0:
                continue
            dest = bersaglio(dati, off)
            for o2, b2 in percorso:
                if not dest <= o2 < off:
                    continue
                corpo = fino_a_ret(bersaglio(dati, o2), chi, rumore) if b2 == C_SOUND_CALL \
                    else [(o2, b2)]
                if any(x == C_SOUND_LOOP for _, x in corpo):
                    raise ErroreMusica(f'{nome}: {chi} ha cicli finiti annidati')


def leggi_rumori(cartella):
    """audio/sfx/noise_instrumentNN_3.asm -> per strumento, le note (fotogrammi - 1, NR42,
    NR43). Sono effetti del canale 8 fatti solo di noise_note e di un sound_ret."""
    rumori = []
    for i in range(1, N_RUMORI + 1):
        nome = f'noise_instrument{i:02}_3.asm'
        note, finito = [], False
        for n, riga in righe_pulite(os.path.join(cartella, 'audio', 'sfx', nome)):
            dove = f'{nome}:{n}'
            r = riga.strip()
            if re.fullmatch(r'\w+::?', r):
                continue
            parti = r.split(None, 1)
            args = [a.strip() for a in parti[1].split(',')] if len(parti) > 1 else []
            if finito:
                raise ErroreMusica(f'{dove}: comandi dopo il sound_ret')
            if parti[0] == 'noise_note' and len(args) == 4:
                lung, vol, fade, freq = (numero(a, dove) for a in args)
                if not (0 <= lung <= 15 and 0 <= vol <= 15 and 0 <= freq <= 255):
                    raise ErroreMusica(f'{dove}: noise_note fuori intervallo')
                note.append((lung, vol << 4 | segno_modulo(fade, dove), freq))
            elif parti[0] == 'sound_ret' and not args:
                finito = True
            else:
                raise ErroreMusica(f'{dove}: comando sconosciuto «{parti[0]}»')
        if not note or not finito:
            raise ErroreMusica(f'{nome}: servono delle noise_note e un sound_ret')
        rumori.append(note)
    return rumori


def assembla(radice=RADICE):
    comune = os.path.join(radice, COMUNE)
    note = leggi_note(comune)
    onde = leggi_onde(comune)
    rumori = leggi_rumori(comune)
    teste = {}
    brani = []
    for enum, sorgente, file, testa, ripeti in BRANI:
        cartella = os.path.join(radice, sorgente)
        if sorgente not in teste:
            teste[sorgente] = leggi_intestazioni(cartella)
        if testa not in teste[sorgente]:
            raise ErroreMusica(f'{sorgente}/audio/headers: manca {testa}')
        b = assembla_brano(cartella, file, testa, canali_del_brano(teste[sorgente], testa), ripeti)
        b['enum'] = enum
        b['sorgente'] = sorgente
        controlla_onde(b, len(onde))
        brani.append(b)
    return {'brani': brani, 'note': note, 'onde': onde, 'rumori': rumori}


def controlla_onde(brano, n_onde):
    """Il canale 3 usa note_type velocità, volume 0-3, strumento d'onda esistente."""
    if len(brano['canali']) < 3:
        return
    dati = brano['dati']
    for off, b, _rumore in istruzioni_brano(brano):
        if b & 0xF0 != C_NOTE_TYPE:
            continue
        # sezione del canale 3: tra il suo inizio e l'etichetta globale successiva
        if sezione(brano, off) != brano['canali'][2]:
            continue
        vol, onda = dati[off + 1] >> 4, dati[off + 1] & 0xF
        if vol > 3 or onda >= n_onde:
            raise ErroreMusica(f'{brano["file"]}: note_type del canale 3 a ${off:04X} '
                               f'con volume {vol} / onda {onda} non validi')


# ---------------------------------------------------------------- simulazione del tempo

def simula(brano, giri=11, max_fotogrammi=400000):
    """Fa girare la temporizzazione del motore (Audio2_UpdateMusic) fotogramma per fotogramma.

    Regole di engine_2.asm: ogni canale ha un contatore; a 1 legge i comandi fino alla
    nota/pausa successiva, altrimenti scende di uno. Una nota dura
    alto(frazione + basso(lunghezza × velocità) × tempo) fotogrammi, e il byte basso resta
    come frazione per la nota dopo; `tempo` è comune ai canali e azzera le frazioni di
    tutti. Contatore 0 = 256 fotogrammi. Torna, per canale, i fotogrammi in cui si esegue il
    sound_loop 0 (o, per un canale che finisce, quello della fine).
    """
    dati = brano['dati']
    n = len(brano['inizio'])
    rumore = [et == brano['canale4'] for et in brano['canali']]
    pc = list(brano['inizio'])
    ret = [None] * n
    conta = [1] * n
    vel = [1] * n
    cont = [1] * n
    fraz = [0] * n
    fine = [None] * n
    cicli = [[] for _ in range(n)]
    tempo = 0x100
    for f in range(max_fotogrammi):
        for c in range(n):
            if fine[c] is not None:
                continue
            if cont[c] != 1:
                cont[c] = (cont[c] - 1) & 0xFF
                continue
            for _ in range(10000):
                b = dati[pc[c]]
                if b <= 0xCF:  # nota o pausa (sul canale 4: drum_note o pausa)
                    pc[c] += lunghezza_comando(b, rumore[c])
                    prod = ((b & 0xF) + 1) * vel[c] & 0xFF
                    v = (fraz[c] + prod * tempo) & 0xFFFF
                    fraz[c], cont[c] = v & 0xFF, v >> 8
                    break
                if b & 0xF0 == C_NOTE_TYPE:
                    vel[c] = b & 0xF
                elif b == C_TEMPO:
                    tempo = dati[pc[c] + 1] << 8 | dati[pc[c] + 2]
                    fraz = [0] * n
                elif b == C_SOUND_CALL:
                    ret[c] = pc[c] + 3
                    pc[c] = bersaglio(dati, pc[c])
                    continue
                elif b == C_SOUND_RET:
                    if ret[c] is None:
                        fine[c] = f
                        break
                    pc[c], ret[c] = ret[c], None
                    continue
                elif b == C_SOUND_LOOP:
                    k = dati[pc[c] + 1]
                    if k == 0:
                        cicli[c].append(f)
                    elif conta[c] == k:
                        conta[c] = 1
                        pc[c] += 4
                        continue
                    else:
                        conta[c] += 1
                    pc[c] = bersaglio(dati, pc[c])
                    continue
                pc[c] += lunghezza_comando(b, rumore[c])
            else:
                raise ErroreMusica(f'{brano["file"]}: canale {c + 1} senza note in un ciclo')
        if all(fine[c] is not None or len(cicli[c]) >= giri for c in range(n)):
            break
    else:
        raise ErroreMusica(f'{brano["file"]}: simulazione oltre {max_fotogrammi} fotogrammi')
    return [{'fine': fine[c], 'cicli': cicli[c]} for c in range(n)]


def durate(dati):
    """Per brano e canale: (enum, canale, 'ciclo', fotogramma del primo sound_loop 0, giri
    tra quelli consecutivi) o (enum, canale, 'fine', fotogramma della fine, None)."""
    righe = []
    for b in dati['brani']:
        for c, r in enumerate(simula(b)):
            if r['fine'] is not None:
                righe.append((b['enum'], c + 1, 'fine', r['fine'], None))
            else:
                giri = [y - x for x, y in zip(r['cicli'], r['cicli'][1:])]
                righe.append((b['enum'], c + 1, 'ciclo', r['cicli'][0], giri))
    return righe


def stampa_durate(dati):
    fps = FPS_NUM / FPS_DEN
    for enum, canale, tipo, primo, giri in durate(dati):
        if tipo == 'fine':
            print(f'{enum:22} canale {canale}: fine a {primo} fotogrammi ({primo / fps:.2f} s)')
            continue
        giro = giri[0] if len(set(giri)) == 1 else f'{min(giri)}-{max(giri)} {giri}'
        print(f'{enum:22} canale {canale}: intro {primo - giri[0]}, giro {giro} fotogrammi '
              f'({giri[0] / fps:.2f} s), primo sound_loop 0 a {primo}')


# ---------------------------------------------------------------- scrittura dell'header

INTESTAZIONE = """\
#pragma once
#include <stdint.h>

// GENERATO da tools/gen_musica.py da tools/musica/ (pret/pokered, pret/pokeyellow) - non
// modificare
//
// Musiche di Pokemon Rosso/Blu e Giallo (Junichi Masuda, (C) Nintendo / Creatures /
// GAME FREAK), per uso personale: le battaglie e i due minigiochi a movimento. Il bytecode
// e' quello del motore audio di pokered (audio/engine_2.asm), byte per byte. Un comando per
// volta, un byte piu' i parametri:
//
//   $00-$BF  note        alto = nota (0 = C ... 11 = B), basso = lunghezza - 1
//   $C0-$CF  rest        basso = lunghezza - 1
//   $Dv xy   note_type   v = velocita'; canali 1-2: x = volume 0-15, y = fade (bit 3 =
//                        sale, bit 0-2 = passo; 0 = fisso); canale 3: x = livello 0-3
//                        (0 muto, 1 100%, 2 50%, 3 25%), y = forma d'onda (MUSICA_ONDE)
//   $Eo      octave      o = 8 - ottava (ottava 1-8)
//   $E8      toggle_perfect_pitch   frequenza + 1 finche' non si ripete
//   $EA d xy vibrato     d = ritardo in fotogrammi, x = profondita', y = velocita'
//   $EC d    duty_cycle  d = 0-3 (12,5 / 25 / 50 / 75 %)
//   $ED h l  tempo       tempo a 16 bit (h alto); comune ai canali, azzera le frazioni
//   $F0 lr   volume      NR50: l = sinistra 0-7, r = destra 0-7
//   $F8      execute_music   senza effetto sui canali della musica
//   $FD a a  sound_call  offset della subroutine nel brano (little endian); una sola
//                        chiamata per volta
//   $FE n a a sound_loop n = 0: salto infinito; altrimenti ripete finche' il contatore
//                        del canale (parte da 1) arriva a n, poi lo riporta a 1 e prosegue
//   $FF      sound_ret   ritorno dalla subroutine; fuori da una chiamata: fine del canale
//
// Sul canale 4 (rumore, solo in alcuni brani) cambiano due comandi:
//   $Bl i    drum_note   l = lunghezza - 1, i = strumento 1-19: suona MUSICA_RUMORI
//   $Dv      drum_speed  v = velocita', senza parametro
// e le pause contano solo il tempo. Lo strumento e' un effetto del canale 8 del motore:
// parte nello stesso fotogramma, ogni nota dura (lunghezza + 1) fotogrammi, e un
// drum_note che trova il canale occupato (dallo strumento di prima o da un effetto) tace.
//
// Temporizzazione (un passo per fotogramma, MUSICA_FPS_NUM / MUSICA_FPS_DEN Hz): ogni
// canale ha un contatore; quando vale 1 si leggono i comandi fino alla nota o pausa
// successiva, altrimenti scende di 1. La durata e'
//   v = frazione + (uint8_t)(lunghezza * velocita') * tempo   (16 bit)
//   contatore = v >> 8, frazione = v & 0xFF                   (contatore 0 = 256)
// Il tempo iniziale e' $0100, la velocita' 1, frazioni e contatori di ripetizione a 0 e 1.
// Frequenza a 11 bit di una nota: ((int16_t)MUSICA_NOTE[nota] >> (ottava - 1)) + 0x800,
// troncata a 11 bit; il canale 3 suona un'ottava sotto (65536 / (2048 - f) Hz).
// I brani di battaglia e dei minigiochi girano in loop (i canali possono avere giri di
// lunghezza diversa, che si riallineano); le vittorie finiscono (il loro sound_loop 0 di
// pokered qui e' un sound_ret). Le durate: python3 tools/gen_musica.py durate
"""


def righe_byte(dati, a, b, rientro='  '):
    out = []
    for i in range(a, b, 16):
        out.append(rientro + ', '.join(f'0x{x:02X}' for x in dati[i:min(i + 16, b)]) + ',\n')
    return out


def header(dati):
    out = [INTESTAZIONE, '\n']
    out.append(f'#define MUSICA_FPS_NUM {FPS_NUM}  // fotogrammi al secondo = NUM / DEN\n')
    out.append(f'#define MUSICA_FPS_DEN {FPS_DEN}  // = 59,7275 Hz\n\n')
    out.append('enum : uint8_t {\n')
    for nome, val in (('DRUM_NOTE', C_DRUM_NOTE), ('REST', C_REST), ('NOTE_TYPE', C_NOTE_TYPE), ('OCTAVE', C_OCTAVE),
                      ('TOGGLE_PERFECT_PITCH', C_TOGGLE_PERFECT_PITCH),
                      ('VIBRATO', C_VIBRATO), ('DUTY_CYCLE', C_DUTY_CYCLE),
                      ('TEMPO', C_TEMPO), ('VOLUME', C_VOLUME),
                      ('EXECUTE_MUSIC', C_EXECUTE_MUSIC), ('SOUND_CALL', C_SOUND_CALL),
                      ('SOUND_LOOP', C_SOUND_LOOP), ('SOUND_RET', C_SOUND_RET)):
        out.append(f'  MCMD_{nome} = 0x{val:02X},\n')
    out.append('};\n\n')

    out.append('// brani, nell\'ordine di MUSICA_BRANI\n')
    out.append('enum : uint8_t {\n')
    for i, b in enumerate(dati['brani']):
        out.append(f'  {b["enum"]} = {i},\n')
    out.append(f'  MUS_COUNT = {len(dati["brani"])},\n')
    out.append('  MUS_NESSUNA = 0xFF,  // nessuna musica\n')
    out.append('};\n\n')

    out.append('// audio/notes.asm: frequenze dell\'ottava di riferimento, da spostare per ottava\n')
    out.append(f'static const uint16_t MUSICA_NOTE[{len(dati["note"])}] = {{\n  ')
    out.append(', '.join(f'0x{v:04X}' for v in dati['note']) + ',\n};\n\n')

    onde = dati['onde']
    out.append('// audio/wave_samples.asm: 32 campioni a 4 bit per forma d\'onda, due per byte\n'
               '// (il primo nel nibble alto, come nella wave RAM del Game Boy)\n')
    out.append(f'#define MUSICA_N_ONDE {len(onde)}\n')
    out.append('static const uint8_t MUSICA_ONDE[MUSICA_N_ONDE][16] = {\n')
    for o in onde:
        out.append('  {' + ', '.join(f'0x{o[i] << 4 | o[i + 1]:02X}' for i in range(0, 32, 2))
                   + '},\n')
    out.append('};\n\n')

    rumori = dati['rumori']
    out.append('// audio/sfx/noise_instrumentNN_3.asm: gli strumenti della batteria (drum_note 1-19).\n'
               '// Una nota per riga: fotogrammi - 1, NR42 (volume e fade), NR43 (frequenza);\n'
               '// lo strumento i sono le note da MUSICA_RUMORI[i - 1] a MUSICA_RUMORI[i] escluso\n')
    out.append(f'#define MUSICA_N_RUMORI {len(rumori)}\n')
    inizi = [0]
    for r in rumori:
        inizi.append(inizi[-1] + len(r))
    out.append('static const uint8_t MUSICA_RUMORI[MUSICA_N_RUMORI + 1] = {\n  '
               + ', '.join(str(x) for x in inizi) + ',\n};\n')
    out.append(f'static const uint8_t MUSICA_RUMORI_NOTE[{inizi[-1]}][3] = {{\n')
    for i, r in enumerate(rumori):
        out.append('  ' + ' '.join(f'{{{l}, 0x{e:02X}, 0x{f:02X}}},' for l, e, f in r)
                   + f'  // {i + 1}\n')
    out.append('};\n\n')

    for b in dati['brani']:
        d = b['dati']
        out.append(f'// {b["sorgente"]}: {b["file"]}.asm ({len(d)} byte)\n')
        out.append(f'static const uint8_t MUSICA_DATI_{b["enum"][4:]}[{len(d)}] = {{\n')
        tagli = sorted(set(b['etichette'].values()) | {len(d)})
        nomi = {}
        for nome, off in b['etichette'].items():
            nomi.setdefault(off, []).append(nome)
        for a, z in zip(tagli, tagli[1:]):
            out.append(f'  // ${a:04X} ' + ', '.join(nomi[a]) + '\n')
            out += righe_byte(d, a, z)
        out.append('};\n\n')

    out.append('struct MusicaBrano {\n'
               '  const char *nome;       // nome del file di pokered o pokeyellow\n'
               '  uint8_t canali;         // canali 1..canali, nell\'ordine di inizio\n'
               '  uint8_t ripeti;         // 1 = in loop, 0 = finisce (vittoria)\n'
               '  const uint8_t *dati;    // bytecode di tutti i canali\n'
               '  uint16_t lunghezza;     // byte di dati\n'
               '  uint16_t inizio[4];     // offset in dati del primo comando di ogni canale\n'
               '};\n\n')
    out.append('static const MusicaBrano MUSICA_BRANI[MUS_COUNT] = {\n')
    for b in dati['brani']:
        inizio = b['inizio'] + [0] * (4 - len(b['inizio']))
        out.append(f'  {{"{b["file"]}", {len(b["inizio"])}, {int(b["ripeti"])}, '
                   f'MUSICA_DATI_{b["enum"][4:]}, {len(b["dati"])}, '
                   f'{{{", ".join(str(x) for x in inizio)}}}}},\n')
    out.append('};\n')
    return ''.join(out)


def main():
    try:
        dati = assembla()
        if sys.argv[1:] == ['durate']:
            stampa_durate(dati)
            return
        if sys.argv[1:]:
            raise ErroreMusica('uso: gen_musica.py [durate]')
        path = os.path.normpath(os.path.join(QUI, '..', 'musica_dati.h'))
        with open(path, 'w', encoding='utf-8') as fh:
            fh.write(header(dati))
        print(path)
    except ErroreMusica as e:
        print(f'gen_musica: {e}', file=sys.stderr)
        sys.exit(1)


if __name__ == '__main__':
    main()
