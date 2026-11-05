#!/usr/bin/env python3
"""Salvataggi .pgsave: scaricarli dalla scheda, caricarli sulla scheda, ispezionarli.

  python3 tools/salvataggi.py esporta [-o file.pgsave] [--port /dev/ttyACM0]
  python3 tools/salvataggi.py importa file.pgsave [--port /dev/ttyACM0]
  python3 tools/salvataggi.py info file.pgsave

`esporta` e `importa` usano i comandi seriali EXPORT e IMPORT del firmware (115200 baud).
L'importazione va confermata sullo schermo della scheda, che poi si riavvia.
`info` decodifica intestazione, riepilogo e chiavi e verifica il CRC; non serve la scheda.
Il formato è descritto byte per byte nel commento di testa di `savefile.h`; lo stato del
Pokémon (schema 2) è il blob "st", con il layout di `PetSave` in `pet.h`; la collezione
personale è il blob "col", con i ritratti descritti in `ritratto.h`.
"""
import argparse
import base64
import binascii
import glob
import re
import struct
import sys
import time
import zlib

HDR_MIN = 52
# magic del formato; "TPSV" e il namespace "tamapoke" sono quelli delle versioni fino alla
# 2.1 (prima del cambio di nome), che si leggono ancora
MAGIC = (b'PGSV', b'TPSV')
NS_PET = ('pokegotchi', 'tamapoke')
FORMATO = 1  # SAVE_FORMATO di savefile.h: un contenitore più recente non si decodifica

PIATTAFORME = {0: 'scheda', 1: 'web'}
MOTIVI = ['manuale', 'giornaliera', 'aggiornamento', 'scambio', 'importazione']

# codici di `nvs_type_t` (SAVE_T_*) -> nome e formato struct little-endian
TIPI = {
    0x01: ('u8', '<B'), 0x11: ('i8', '<b'), 0x02: ('u16', '<H'), 0x12: ('i16', '<h'),
    0x04: ('u32', '<I'), 0x14: ('i32', '<i'), 0x08: ('u64', '<Q'), 0x18: ('i64', '<q'),
    0x21: ('str', None), 0x42: ('blob', None),
}

# esiti di `saveEsitoNome` che il firmware può rispondere a IMPORT
ESITI = {
    'MAGIC': 'non è un file .pgsave',
    'FORMATO': 'contenitore più recente del firmware: aggiorna il firmware',
    'LUNGHEZZA': 'file troncato o con byte in più',
    'CRC': 'file corrotto (CRC)',
    'SCHEMA': 'dati più recenti del firmware: aggiorna il firmware',
    'DATI': 'corpo malformato',
    'COPIA': 'la copia di sicurezza sulla SD non è riuscita',
    'SCRITTURA': 'la NVS ha rifiutato una scrittura',
    'BASE64': 'la riga ricevuta non è base64 valido',
    'OCCUPATO': "un'altra importazione aspetta già la conferma",
    'ANNULLATO': 'annullata sullo schermo della scheda',
    'SCADUTO': 'nessuna conferma sullo schermo in 150 s: la scheda ha annullato',
}


# Layout del blob PET_SAVE_KEY ("st") = `struct PetSave` di pet.h, impacchettata e
# little-endian: (campo, formato struct). Il CRC32 finale copre tutti i byte precedenti.
# I campi nuovi si aggiungono solo in coda; TestSalvataggi confronta questo elenco con pet.h.
PET_SAVE_KEY = 'st'
PET_SAVE_CAMPI = [
    ('version', 'H'), ('size', 'H'),
    ('fullness', 'B'), ('joy', 'B'), ('energy', 'B'), ('hygiene', 'B'),
    ('poops', 'B'), ('weight', 'B'),
    ('geneAtk', 'B'), ('geneDef', 'B'), ('geneSpe', 'B'),
    ('trAtk', 'B'), ('trDef', 'B'), ('trSpe', 'B'),
    ('berryKnown', 'B'), ('shiny', 'B'), ('eggShiny', 'B'), ('starterPick', 'B'),
    ('ageMinutes', 'I'),
    ('speciesId', 'h'), ('eggTarget', 'h'),
    ('eggTaps', 'B'), ('careMistakes', 'B'), ('sleeping', 'B'), ('lastEnd', 'B'),
    ('dexReg', '19s'),
    ('dexShinyReg', '19s'),
    ('streak', 'H'), ('bestStreak', 'H'),
    ('lastCareDay', 'I'),
    ('bond', 'B'),
    ('medals', 'H'), ('totalMedals', 'H'), ('lastMilestone', 'H'), ('gameHi', 'H'),
    ('strHi', 'H'),
    ('nick', '12s'),
    ('winsToday', 'B'), ('winsDay', 'I'),  # versione 2
    ('colAsk', 'B'), ('colDay', 'I'), ('farewellMinAge', 'I'),  # versione 3
    ('otId', 'I'), ('otName', '12s'),
    ('tradeFatto', 'I'),  # versione 4
    ('grottaHi', 'H'), ('surfHi', 'H'),  # versione 5
    ('crc', 'I'),
]

# Collezione (blob COL_KEY "col", pet.h): versione, numero di ritratti, poi i ritratti, ognuno
# con la sua lunghezza al byte 1. Layout del ritratto = commento di testa di ritratto.h
# (TestSalvataggi lo confronta). Il CRC32 finale copre i byte precedenti del ritratto.
COL_KEY = 'col'
RITRATTO_CAMPI = [
    ('versione', 'B'), ('lunghezza', 'B'), ('specie', 'h'), ('shiny', 'B'),
    ('ageMinutes', 'I'),
    ('geneAtk', 'B'), ('geneDef', 'B'), ('geneSpe', 'B'),
    ('trAtk', 'B'), ('trDef', 'B'), ('trSpe', 'B'),
    ('bond', 'B'), ('medals', 'H'), ('nick', '12s'),
    ('otId', 'I'), ('otNome', '12s'),
    ('crc', 'I'),
]
MINUTI_PER_LIVELLO = 60  # MINUTES_PER_LEVEL di pet.h


class ErroreSalvataggio(Exception):
    """File non valido; `esito` è il nome che userebbe il firmware (CRC, LUNGHEZZA...)."""

    def __init__(self, esito, messaggio):
        super().__init__(messaggio)
        self.esito = esito


def _cstr(b):
    return b.split(b'\0', 1)[0].decode('ascii', errors='replace')


def intestazione(dati):
    """Decodifica i primi HDR_MIN byte, senza controllare lunghezza e CRC."""
    if len(dati) < HDR_MIN or dati[:4] not in MAGIC:
        raise ErroreSalvataggio('MAGIC', 'non è un file .pgsave (magic PGSV assente)')
    formato, piattaforma, motivo, _, schema, lung_hdr, lunghezza, epoch = \
        struct.unpack_from('<BBBBHHII', dati, 4)
    specie, livello, shiny, _, pokedex = struct.unpack_from('<hHBBH', dati, 32)
    return {
        'formato': formato, 'piattaforma': piattaforma, 'motivo': motivo, 'schema': schema,
        'lunghezza_hdr': lung_hdr, 'lunghezza': lunghezza, 'epoch': epoch,
        'fw': _cstr(dati[20:32]),
        'riepilogo': {'specie': specie, 'livello': livello, 'shiny': bool(shiny),
                      'pokedex': pokedex, 'nome': _cstr(dati[40:52])},
    }


def decodifica(dati):
    """Verifica e decodifica un .pgsave intero.

    Restituisce l'intestazione con in più `crc` e `namespace`: lista di
    (nome, [(tipo, chiave, valore)]). Solleva ErroreSalvataggio se qualcosa non torna.
    """
    h = intestazione(dati)
    if h['formato'] > FORMATO:
        raise ErroreSalvataggio('FORMATO', f"formato {h['formato']} più recente di {FORMATO}")
    if h['lunghezza'] != len(dati) or h['lunghezza_hdr'] < HDR_MIN \
            or h['lunghezza_hdr'] + 4 > len(dati):
        raise ErroreSalvataggio('LUNGHEZZA', f"il file ha {len(dati)} byte, "
                                f"l'intestazione ne dichiara {h['lunghezza']}")
    crc = struct.unpack_from('<I', dati, len(dati) - 4)[0]
    if zlib.crc32(dati[:-4]) != crc:
        raise ErroreSalvataggio('CRC', f'CRC errato: atteso {zlib.crc32(dati[:-4]):08x}, '
                                f'nel file {crc:08x}')
    h['crc'] = crc

    corpo, fine = h['lunghezza_hdr'], len(dati) - 4

    def prendi(n):
        nonlocal corpo
        if corpo + n > fine:
            raise ErroreSalvataggio('DATI', f'corpo troncato a offset {corpo}')
        pezzo = dati[corpo:corpo + n]
        corpo += n
        return pezzo

    h['namespace'] = []
    while corpo < fine:
        ns = prendi(prendi(1)[0]).decode('ascii', errors='replace')
        voci = []
        for _ in range(struct.unpack('<H', prendi(2))[0]):
            tipo = prendi(1)[0]
            chiave = prendi(prendi(1)[0]).decode('ascii', errors='replace')
            valore = prendi(struct.unpack('<H', prendi(2))[0])
            if tipo not in TIPI:
                raise ErroreSalvataggio('DATI', f'{ns}/{chiave}: tipo sconosciuto 0x{tipo:02x}')
            nome_tipo, fmt = TIPI[tipo]
            if fmt:
                if len(valore) != struct.calcsize(fmt):
                    raise ErroreSalvataggio('DATI', f'{ns}/{chiave}: {nome_tipo} di '
                                            f'{len(valore)} byte')
                valore = struct.unpack(fmt, valore)[0]
            elif nome_tipo == 'str':
                valore = valore.decode('utf-8', errors='replace')
            voci.append((nome_tipo, chiave, valore))
        h['namespace'].append((ns, voci))
    # stato del Pokémon nel blob (schema 2); None nei file a chiavi singole
    h['pet'] = next((stato_pet(v) for ns, voci in h['namespace'] if ns in NS_PET
                     for t, c, v in voci if c == PET_SAVE_KEY and t == 'blob'), None)
    # collezione: lista vuota se il file non ha la chiave
    h['collezione'] = next((collezione(v) for ns, voci in h['namespace'] if ns in NS_PET
                            for t, c, v in voci if c == COL_KEY and t == 'blob'), [])
    return h


def ritratto(dati):
    """Decodifica un ritratto (ritratto.h): (campi, problema). `problema` è None se è
    valido, altrimenti il motivo per cui il firmware lo scarta."""
    if len(dati) < 2 or dati[1] > len(dati):
        return {}, 'troncato'
    n = dati[1]
    dati = dati[:n]
    campi, pos = {}, 0
    for nome, fmt in RITRATTO_CAMPI[:-1]:
        k = struct.calcsize('<' + fmt)
        if pos + k > n - 4:
            break
        valore = struct.unpack_from('<' + fmt, dati, pos)[0]
        if fmt.endswith('s'):
            valore = _cstr(valore)
        campi[nome] = valore
        pos += k
    if n < struct.calcsize('<' + ''.join(f for _, f in RITRATTO_CAMPI)) or not dati[0]:
        return campi, f'ritratto di {n} byte, versione {dati[0]}'
    campi['crc'] = struct.unpack_from('<I', dati, n - 4)[0]
    if zlib.crc32(dati[:-4]) != campi['crc']:
        return campi, 'CRC del ritratto errato'
    if not 1 <= campi['specie'] <= 151:
        return campi, f"specie {campi['specie']} fuori intervallo"
    return campi, None


def collezione(blob):
    """I ritratti del blob "col": lista di (campi, problema), come li legge `colCarica()`."""
    if len(blob) < 2 or blob[0] == 0:
        return []
    voci, pos = [], 2
    for _ in range(blob[1]):
        if pos + 1 >= len(blob):
            break
        campi, problema = ritratto(blob[pos:])
        voci.append((campi, problema))
        if 'lunghezza' not in campi or campi['lunghezza'] < 2:
            break
        pos += campi['lunghezza']
    return voci


def stato_pet(blob):
    """Decodifica il blob "st": (campi, problema). `campi` contiene il prefisso noto, come lo
    legge `Pet::load()`; `problema` è None se lunghezza e CRC tornano, altrimenti il motivo
    (il firmware in quel caso non usa il blob)."""
    if len(blob) < 8:
        return {}, f'blob di {len(blob)} byte'
    campi, pos = {}, 0
    for nome, fmt in PET_SAVE_CAMPI[:-1]:
        n = struct.calcsize('<' + fmt)
        if pos + n > len(blob) - 4:
            break
        valore = struct.unpack_from('<' + fmt, blob, pos)[0]
        if nome in ('nick', 'otName'):
            valore = _cstr(valore)
        campi[nome] = valore
        pos += n
    campi['crc'] = struct.unpack_from('<I', blob, len(blob) - 4)[0]
    if campi['size'] != len(blob):
        return campi, f"size {campi['size']} ma il blob ha {len(blob)} byte"
    if zlib.crc32(blob[:-4]) != campi['crc']:
        return campi, 'CRC del blob errato'
    return campi, None


def nome_specie(num):
    if num < 0:
        return 'uovo'
    try:
        from dex_data import DEX
        return next(row[2] for row in DEX if row[0] == num)
    except (ImportError, StopIteration):
        return '?'


def nome_file(h):
    """Nome come quello delle copie sulla SD: AAAAMMGG-HHMMSS-<motivo>.pgsave (UTC)."""
    epoch = h['epoch'] or int(time.time())
    motivo = MOTIVI[h['motivo']] if h['motivo'] < len(MOTIVI) else 'manuale'
    return time.strftime('%Y%m%d-%H%M%S', time.gmtime(epoch)) + f'-{motivo}.pgsave'


def descrivi(h):
    """Righe leggibili di un salvataggio decodificato."""
    r = h['riepilogo']
    quando = (time.strftime('%Y-%m-%d %H:%M:%S UTC', time.gmtime(h['epoch']))
              if h['epoch'] else 'sconosciuta')
    motivo = MOTIVI[h['motivo']] if h['motivo'] < len(MOTIVI) else f"? ({h['motivo']})"
    righe = [
        f"formato      {h['formato']}  schema {h['schema']}",
        f"origine      {PIATTAFORME.get(h['piattaforma'], '?')}, firmware {h['fw'] or '?'}",
        f"creato       {quando}  (epoch {h['epoch']})",
        f"motivo       {motivo}",
        f"lunghezza    {h['lunghezza']} byte, CRC {h['crc']:08x} ok",
        f"specie       {r['specie']} ({nome_specie(r['specie'])})"
        + (' shiny' if r['shiny'] else ''),
        f"livello      {r['livello']}",
        f"nome         {r['nome'] or '(nome della specie)'}",
        f"pokedex      {r['pokedex']} specie registrate",
    ]
    for ns, voci in h['namespace']:
        righe.append(f'[{ns}] {len(voci)} chiavi')
        for tipo, chiave, valore in voci:
            if isinstance(valore, bytes):
                valore = f'{len(valore)} byte: {valore.hex()}'
            elif isinstance(valore, str):
                valore = repr(valore)
            righe.append(f'  {chiave:<15} {tipo:<4} {valore}')
            if chiave == PET_SAVE_KEY and tipo == 'blob':
                righe += descrivi_pet(h['pet'] if ns in NS_PET else None, valore)
            if chiave == COL_KEY and tipo == 'blob' and ns in NS_PET:
                righe += descrivi_collezione(h['collezione'])
    return righe


def descrivi_collezione(voci):
    """Una riga per ritratto della collezione, rientrata sotto la sua chiave."""
    righe = [f'    collezione: {len(voci)} Pokémon']
    for i, (c, problema) in enumerate(voci):
        if problema and 'specie' not in c:
            righe.append(f'    {i}: {problema}')
            continue
        livello = min(999, 1 + c.get('ageMinutes', 0) // MINUTI_PER_LIVELLO)
        riga = (f"    {i}: {c['specie']} ({nome_specie(c['specie'])})"
                + (' shiny' if c.get('shiny') else '')
                + f" livello {livello}, nome {c.get('nick') or '(nome della specie)'}"
                + f", geni {c.get('geneAtk')}/{c.get('geneDef')}/{c.get('geneSpe')}"
                + f", allenamento {c.get('trAtk')}/{c.get('trDef')}/{c.get('trSpe')}"
                + f", legame {c.get('bond')}, allenatore "
                + (f"{c.get('otNome') or '?'} ({c.get('otId')})" if c.get('otId') else 'io'))
        if problema:
            riga += f'  [{problema}: il firmware lo scarta]'
        righe.append(riga)
    return righe


def descrivi_pet(pet, valore):
    """Righe dei campi del blob dello stato, rientrate sotto la sua chiave."""
    if pet is None:
        return []
    campi, problema = pet
    righe = [f"    stato del Pokémon: versione {campi.get('version', '?')}, "
             + (problema or 'CRC ok')]
    for nome, v in campi.items():
        if nome == 'crc':
            v = f'{v:08x}'
        elif isinstance(v, bytes):
            v = v.hex()
        elif isinstance(v, str):
            v = repr(v)
        righe.append(f'    {nome:<15} {v}')
    return righe


# ---------------------------------------------------------------- seriale

RIGA_B64 = re.compile(r'^[A-Za-z0-9+/]+=*$')


def estrai_salvataggio(riga):
    """I byte del salvataggio se `riga` è la risposta base64 a EXPORT, altrimenti None."""
    if len(riga) < 4 * HDR_MIN // 3 or not RIGA_B64.match(riga):
        return None
    try:
        dati = base64.b64decode(riga, validate=True)
    except binascii.Error:
        return None
    return dati if dati[:4] in MAGIC else None


def trova_porta():
    porte = sorted(glob.glob('/dev/cu.usbmodem*') + glob.glob('/dev/ttyACM*'))
    if not porte:
        sys.exit('scheda non trovata (/dev/cu.usbmodem*, /dev/ttyACM*): usa --port')
    return porte[0]


def apri(porta):
    import serial  # solo qui: `info` funziona anche senza pyserial
    porta = porta or trova_porta()
    print(f'porta {porta}')
    ser = serial.Serial(porta, 115200, timeout=1)
    time.sleep(1.5)
    ser.reset_input_buffer()
    return ser


def righe_seriali(ser, timeout):
    """Genera le righe ricevute fino a `timeout` secondi."""
    fine = time.time() + timeout
    while time.time() < fine:
        riga = ser.readline().decode(errors='replace').strip()
        if riga:
            yield riga


def esporta(args):
    ser = apri(args.port)
    ser.write(b'EXPORT\n')
    dati = None
    for riga in righe_seriali(ser, 10):
        if dati is None:
            dati = estrai_salvataggio(riga)
            if dati is None:
                print(f'  scheda: {riga}')
        elif riga == 'DONE':
            break
    if dati is None:
        sys.exit('la scheda non ha risposto a EXPORT')
    try:
        h = decodifica(dati)
    except ErroreSalvataggio as e:
        sys.exit(f'salvataggio ricevuto ma non valido: {e}')
    uscita = args.output or nome_file(h)
    with open(uscita, 'wb') as fh:
        fh.write(dati)
    r = h['riepilogo']
    print(f"salvato {uscita} ({len(dati)} byte): {nome_specie(r['specie'])} "
          f"Lv {r['livello']}")


def importa(args):
    with open(args.file, 'rb') as fh:
        dati = fh.read()
    try:
        h = decodifica(dati)
    except ErroreSalvataggio as e:
        sys.exit(f'{args.file}: {e}')
    r = h['riepilogo']
    print(f"{args.file}: {nome_specie(r['specie'])} Lv {r['livello']}, "
          f"firmware {h['fw'] or '?'}")
    ser = apri(args.port)
    ser.write(b'IMPORT ' + base64.b64encode(dati) + b'\n')
    in_attesa = False
    for riga in righe_seriali(ser, args.attesa):
        if riga.startswith('CONFERMA') and not in_attesa:
            in_attesa = True
            print('conferma sullo schermo della scheda (SÌ per importare)...')
        elif riga == 'DONE' and in_attesa:
            print('importato: la scheda si riavvia')
            return
        elif riga.startswith('ERR'):
            codice = riga[3:].strip()
            sys.exit(f'importazione rifiutata: {ESITI.get(codice, codice or "errore")}')
        else:
            print(f'  scheda: {riga}')
    sys.exit('nessuna risposta in tempo' + (': conferma non data' if in_attesa else ''))


def info(args):
    with open(args.file, 'rb') as fh:
        dati = fh.read()
    try:
        h = decodifica(dati)
    except ErroreSalvataggio as e:
        sys.exit(f'{args.file}: {e.esito}: {e}')
    print('\n'.join(descrivi(h)))


def main():
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    sub = ap.add_subparsers(dest='comando', required=True)
    p = sub.add_parser('esporta', help='scarica il salvataggio dalla scheda')
    p.add_argument('-o', '--output', help='file di uscita (predefinito: AAAAMMGG-HHMMSS-manuale.pgsave)')
    p.add_argument('--port')
    p.set_defaults(fn=esporta)
    p = sub.add_parser('importa', help='carica un salvataggio sulla scheda')
    p.add_argument('file')
    p.add_argument('--port')
    # la scheda annulla da sola dopo 150 s (SAVE_ATTESA_SERIALE_MS) e risponde ERR SCADUTO
    p.add_argument('--attesa', type=int, default=160,
                   help='secondi per confermare sullo schermo (predefinito 160; la scheda '
                        'annulla dopo 150)')
    p.set_defaults(fn=importa)
    p = sub.add_parser('info', help='decodifica un .pgsave e ne verifica il CRC')
    p.add_argument('file')
    p.set_defaults(fn=info)
    args = ap.parse_args()
    args.fn(args)


if __name__ == '__main__':
    main()
